#include "hook_manager.h"

#include <BNM/Loading.hpp>
#include <BNM/Utils.hpp>
#include <android/log.h>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <dobby.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <atomic>
#include <string>

#include "hooks/chance_hook.h"
#include "hooks/coinflip_hook.h"
#include "hooks/jail_hook.h"
#include "hooks/pickups_hook.h"
#include "hooks/roll_hook.h"
#include "hooks/speed_hook.h"
#include "ipc_feed.h"
#include "pattern_scanner.h"

#define LOG_TAG "HookManager"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

constexpr useconds_t kIl2CppWaitSleepUs = 100000;
constexpr unsigned int kGameWarmupSeconds = 3;

std::atomic<bool> g_bnm_bootstrap_started{false};
std::atomic<bool> g_bnm_loaded_callback_seen{false};
std::atomic<bool> g_install_thread_started{false};
std::atomic<bool> g_hooks_installed{false};
std::atomic<bool> g_late_fallback_started{false};
std::atomic<bool> g_users_finder_fail_logged{false};

constexpr int kLateFallbackAttempts = 50;
constexpr useconds_t kLateFallbackSleepUs = 100000;

void* LateInjectionFallbackMain(void*) {
    prctl(PR_SET_NAME, "babix-latefix", 0, 0, 0);

    for (int i = 0; i < kLateFallbackAttempts; ++i) {
        if (g_bnm_loaded_callback_seen.load()) {
            return nullptr;
        }
        usleep(kLateFallbackSleepUs);
    }

    LOGI("BNM callback timeout; enabling late-injection fallback install path");
    IPCFeed::Publish("late_injection_fallback");
    HookManager::StartInstallThread();
    return nullptr;
}

void StartLateInjectionFallbackThread() {
    if (g_late_fallback_started.exchange(true)) {
        return;
    }

    pthread_t thread = {};
    const int rc = pthread_create(&thread, nullptr, &LateInjectionFallbackMain, nullptr);
    if (rc != 0) {
        LOGE("Failed to create late fallback thread: %d", rc);
        g_late_fallback_started.store(false);
        return;
    }
    pthread_detach(thread);
}

bool FindIl2CppPathFromMaps(std::string* out_path) {
    if (out_path == nullptr) {
        return false;
    }

    FILE* fp = fopen("/proc/self/maps", "r");
    if (fp == nullptr) {
        return false;
    }

    char line[1024] = {};
    while (fgets(line, sizeof(line), fp) != nullptr) {
        if (strstr(line, "libil2cpp.so") == nullptr) {
            continue;
        }

        const char* path_start = strchr(line, '/');
        if (path_start == nullptr) {
            continue;
        }

        std::string path(path_start);
        while (!path.empty() &&
               (path.back() == '\n' || path.back() == '\r' || path.back() == ' ' || path.back() == '\t')) {
            path.pop_back();
        }

        if (!path.empty()) {
            *out_path = path;
            fclose(fp);
            return true;
        }
    }

    fclose(fp);
    return false;
}

std::string ExtractMissingLibraryName(const char* dlopen_error) {
    if (dlopen_error == nullptr) {
        return {};
    }

    const char* marker = strstr(dlopen_error, "library \"");
    if (marker == nullptr) {
        return {};
    }
    marker += strlen("library \"");

    const char* end_quote = strchr(marker, '"');
    if (end_quote == nullptr || end_quote <= marker) {
        return {};
    }

    return std::string(marker, static_cast<size_t>(end_quote - marker));
}

bool TryLoadSiblingLibrary(const std::string& module_path, const std::string& library_name, int attempt) {
    if (module_path.empty() || library_name.empty()) {
        return false;
    }

    const size_t last_slash = module_path.find_last_of('/');
    if (last_slash == std::string::npos) {
        return false;
    }

    const std::string sibling_path = module_path.substr(0, last_slash + 1) + library_name;
    void* handle = dlopen(sibling_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (handle == nullptr) {
        const char* err = dlerror();
        LOGD(
            "dependency preload failed (attempt=%d, lib=%s, path=%s, err=%s)",
            attempt,
            library_name.c_str(),
            sibling_path.c_str(),
            err != nullptr ? err : "<none>");
        return false;
    }

    LOGI(
        "dependency preload succeeded (attempt=%d, lib=%s, path=%s, handle=%p)",
        attempt,
        library_name.c_str(),
        sibling_path.c_str(),
        handle);
    return true;
}

void* ResolveIl2CppSymbol(const char* symbol_name) {
    void* symbol = dlsym(RTLD_DEFAULT, symbol_name);
    if (symbol != nullptr) {
        return symbol;
    }

    // Fallback for isolated linker namespaces where RTLD_DEFAULT cannot see app-local exports.
    return DobbySymbolResolver("libil2cpp.so", symbol_name);
}

void* Il2CppSymbolFinder(const char* symbol_name, void* user_data) {
    (void)user_data;
    return ResolveIl2CppSymbol(symbol_name);
}

bool TryBootstrapViaUsersFinder(int attempt) {
    if ((attempt % 25) != 1) {
        return false;
    }

    std::string full_path;
    if (!FindIl2CppPathFromMaps(&full_path)) {
        return false;
    }

    LOGD("Trying users-finder bootstrap (attempt=%d, il2cpp=%s)", attempt, full_path.c_str());

    void* il2cpp_init = ResolveIl2CppSymbol("il2cpp_init");
    void* il2cpp_class_from_type = ResolveIl2CppSymbol("il2cpp_class_from_il2cpp_type");
    if (il2cpp_init == nullptr || il2cpp_class_from_type == nullptr) {
        LOGD(
            "users-finder symbols unavailable (attempt=%d, il2cpp_init=%p, il2cpp_class_from_il2cpp_type=%p)",
            attempt,
            il2cpp_init,
            il2cpp_class_from_type);
        return false;
    }

    LOGI(
        "libil2cpp symbols visible on attempt %d: il2cpp_init=%p, il2cpp_class_from_il2cpp_type=%p",
        attempt,
        il2cpp_init,
        il2cpp_class_from_type);

    BNM::Loading::SetMethodFinder(&Il2CppSymbolFinder, nullptr);
    if (!BNM::Loading::TryLoadByUsersFinder()) {
        if (!g_users_finder_fail_logged.exchange(true)) {
            LOGE("BNM::Loading::TryLoadByUsersFinder failed despite visible il2cpp symbols");
        }
        return false;
    }

    LOGI("BNM bootstrap armed via users finder");
    return true;
}

void* TryResolveIl2CppHandle(int attempt) {
    void* handle = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
    if (handle != nullptr) {
        LOGI("libil2cpp.so found on attempt %d: %p", attempt, handle);
        return handle;
    }
    const char* name_noload_err = dlerror();

    // Namespace fallback: a normal dlopen can still succeed when NOLOAD lookups fail.
    handle = dlopen("libil2cpp.so", RTLD_NOW);
    if (handle != nullptr) {
        LOGI("libil2cpp.so opened via dlopen on attempt %d: %p", attempt, handle);
        return handle;
    }
    const char* name_open_err = dlerror();

    std::string full_path;
    if (FindIl2CppPathFromMaps(&full_path)) {
        // Prefer NOLOAD to avoid creating duplicate mappings if possible.
        handle = dlopen(full_path.c_str(), RTLD_NOW | RTLD_NOLOAD);
        const char* path_noload_err = dlerror();
        if (handle == nullptr) {
            // Namespace fallback: open by absolute path.
            handle = dlopen(full_path.c_str(), RTLD_NOW);
        }
        const char* path_open_err = dlerror();

        if (handle == nullptr) {
            std::string missing_dep = ExtractMissingLibraryName(path_open_err);
            if (missing_dep.empty()) {
                missing_dep = ExtractMissingLibraryName(name_open_err);
            }

            if (!missing_dep.empty() && TryLoadSiblingLibrary(full_path, missing_dep, attempt)) {
                // Retry after dependency preload.
                handle = dlopen(full_path.c_str(), RTLD_NOW);
                path_open_err = dlerror();
            }
        }

        if (handle != nullptr) {
            LOGI("libil2cpp.so found via maps on attempt %d: %p (%s)", attempt, handle, full_path.c_str());
            return handle;
        }

        if ((attempt % 100) == 0) {
            LOGD(
                "il2cpp mapped but dlopen still failing (attempt=%d, path=%s, noLoadErr=%s, openErr=%s, pathNoLoadErr=%s, pathOpenErr=%s)",
                attempt,
                full_path.c_str(),
                name_noload_err != nullptr ? name_noload_err : "<none>",
                name_open_err != nullptr ? name_open_err : "<none>",
                path_noload_err != nullptr ? path_noload_err : "<none>",
                path_open_err != nullptr ? path_open_err : "<none>");
        }
    } else if ((attempt % 100) == 0) {
        LOGD(
            "il2cpp not yet in /proc/self/maps (attempt=%d, noLoadErr=%s, openErr=%s)",
            attempt,
            name_noload_err != nullptr ? name_noload_err : "<none>",
            name_open_err != nullptr ? name_open_err : "<none>");
    }

    return nullptr;
}
}  // namespace

bool HookManager::InitializeBNM() {
    if (g_bnm_bootstrap_started.exchange(true)) {
        LOGD("InitializeBNM already called");
        return true;
    }

    LOGI("Waiting for libil2cpp bootstrap preconditions...");
    BNM::Loading::AllowLateInitHook();
    BNM::Loading::AddOnLoadedEvent(&HookManager::OnBNMLoaded);

    int attempt = 0;
    while (true) {
        ++attempt;

        if (TryBootstrapViaUsersFinder(attempt)) {
            break;
        }

        void* il2cpp_handle = TryResolveIl2CppHandle(attempt);
        if (il2cpp_handle != nullptr) {
            if (BNM::Loading::TryLoadByDlfcnHandle(il2cpp_handle)) {
                LOGI("BNM bootstrap armed via dlfcn handle");
                break;
            }
            LOGE("BNM::Loading::TryLoadByDlfcnHandle failed with handle %p", il2cpp_handle);
        }

        if ((attempt % 100) == 0) {
            LOGD("Still waiting for libil2cpp bootstrap preconditions (attempt=%d)", attempt);
        }
        usleep(kIl2CppWaitSleepUs);
    }

    LOGI("BNM bootstrap armed");
    IPCFeed::Publish("bnm_bootstrap_armed");

    static const uint8_t kIl2CppElfMagic[] = {0x7F, 0x45, 0x4C, 0x46};
    void* elf_signature = PatternScanner::FindFirstInModule("libil2cpp.so", kIl2CppElfMagic, "xxxx", sizeof(kIl2CppElfMagic));
    LOGD("Pattern scanner probe (ELF magic) => %p", elf_signature);

    StartLateInjectionFallbackThread();
    return true;
}

void HookManager::OnBNMLoaded() {
    if (g_bnm_loaded_callback_seen.exchange(true)) {
        LOGD("BNM loaded callback already handled");
        return;
    }

    LOGI("BNM reported IL2CPP ready");
    IPCFeed::Publish("bnm_ready");
    StartInstallThread();
}

void HookManager::StartInstallThread() {
    if (g_install_thread_started.exchange(true)) {
        LOGD("Install thread already started");
        return;
    }

    pthread_t thread = {};
    const int rc = pthread_create(&thread, nullptr, &HookManager::InstallThreadMain, nullptr);
    if (rc != 0) {
        LOGE("Failed to create install thread: %d", rc);
        g_install_thread_started.store(false);
        return;
    }

    pthread_detach(thread);
}

void* HookManager::InstallThreadMain(void*) {
    prctl(PR_SET_NAME, "babix-hooks", 0, 0, 0);

    const bool attached_here = BNM::AttachIl2Cpp();
    LOGD(attached_here ? "Worker thread attached to IL2CPP" : "Worker thread already attached to IL2CPP");

    LOGI("Waiting %u seconds for game initialization...", kGameWarmupSeconds);
    sleep(kGameWarmupSeconds);

    const bool ok = InstallHooks();
    LOGI(ok ? "All hooks installed" : "Hook installation failed");

    if (attached_here) {
        BNM::DetachIl2Cpp();
    }

    return nullptr;
}

bool HookManager::InstallHooks() {
    if (g_hooks_installed.load()) {
        LOGD("Hooks already installed");
        return true;
    }

    // Mark as attempted immediately so no second thread can enter even if we return false below.
    g_hooks_installed.store(true);

    LOGI("Installing hooks...");
    IPCFeed::Publish("hook_install_start");

    const bool roll_ok = Hooks::Roll::Install();
    const bool jail_ok = Hooks::Jail::Install();
    const bool coinflip_ok = Hooks::CoinFlip::Install();
    const bool pickups_ok = Hooks::Pickups::Install();
    const bool chance_ok = Hooks::Chance::Install();
    const bool speed_ok = Hooks::Speed::Install();

    const bool all_ok = roll_ok && jail_ok && coinflip_ok && pickups_ok && chance_ok && speed_ok;
    LOGI(
        "Hook install summary: roll=%d jail=%d coinflip=%d pickups=%d chance=%d speed=%d",
        roll_ok ? 1 : 0,
        jail_ok ? 1 : 0,
        coinflip_ok ? 1 : 0,
        pickups_ok ? 1 : 0,
        chance_ok ? 1 : 0,
        speed_ok ? 1 : 0);

    if (!all_ok) {
        LOGE("Hook installation incomplete – see per-module logs above");
        IPCFeed::Publish("hook_install_partial");
        return false;
    }

    LOGI("All hooks installed successfully");
    IPCFeed::Publish("hook_install_success");
    return true;
}

bool HookManager::SafeHook(void* target, void* hook, void** original, const char* name) {
    if (target == nullptr) {
        LOGE("SafeHook failed for %s: target is null", name);
        return false;
    }

    if (hook == nullptr) {
        LOGE("SafeHook failed for %s: hook is null", name);
        return false;
    }

    LOGD("Installing hook %s (target=%p, hook=%p)", name, target, hook);

    const int rc = DobbyHook(target, hook, original);
    if (rc != 0) {
        LOGE("DobbyHook failed for %s: %d", name, rc);
        return false;
    }

    if (original == nullptr || *original == nullptr) {
        LOGE("Hook installed for %s but original is null", name);
        return false;
    }

    LOGI("Hook installed: %s (original=%p)", name, *original);
    return true;
}
