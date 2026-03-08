#include "hook_manager.h"

#include <BNM/Loading.hpp>
#include <BNM/Utils.hpp>
#include <android/log.h>
#include <dlfcn.h>
#include <dobby.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <atomic>

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

constexpr int kIl2CppWaitAttempts = 600;
constexpr useconds_t kIl2CppWaitSleepUs = 100000;
constexpr unsigned int kGameWarmupSeconds = 3;

std::atomic<bool> g_bnm_bootstrap_started{false};
std::atomic<bool> g_bnm_loaded_callback_seen{false};
std::atomic<bool> g_install_thread_started{false};
std::atomic<bool> g_hooks_installed{false};
std::atomic<bool> g_late_fallback_started{false};

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

void* WaitForIl2CppHandle() {
    for (int attempt = 1; attempt <= kIl2CppWaitAttempts; ++attempt) {
        void* handle = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
        if (handle != nullptr) {
            LOGI("libil2cpp.so found on attempt %d: %p", attempt, handle);
            return handle;
        }
        usleep(kIl2CppWaitSleepUs);
    }

    return nullptr;
}
}  // namespace

bool HookManager::InitializeBNM() {
    if (g_bnm_bootstrap_started.exchange(true)) {
        LOGD("InitializeBNM already called");
        return true;
    }

    LOGI("Waiting for libil2cpp.so...");
    void* il2cpp_handle = WaitForIl2CppHandle();
    if (il2cpp_handle == nullptr) {
        LOGE("FATAL: libil2cpp.so not found after %d attempts", kIl2CppWaitAttempts);
        return false;
    }

    BNM::Loading::AllowLateInitHook();
    BNM::Loading::AddOnLoadedEvent(&HookManager::OnBNMLoaded);

    if (!BNM::Loading::TryLoadByDlfcnHandle(il2cpp_handle)) {
        LOGE("FATAL: BNM::Loading::TryLoadByDlfcnHandle failed");
        return false;
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

    LOGI("Installing hooks...");
    IPCFeed::Publish("hook_install_start");

    const bool roll_ok = Hooks::Roll::Install();
    const bool jail_ok = Hooks::Jail::Install();
    const bool coinflip_ok = Hooks::CoinFlip::Install();
    const bool pickups_ok = Hooks::Pickups::Install();
    const bool chance_ok = Hooks::Chance::Install();
    const bool speed_ok = Hooks::Speed::Install();

    if (!(roll_ok && jail_ok && coinflip_ok && pickups_ok && chance_ok && speed_ok)) {
        LOGE(
            "Hook install summary: roll=%d jail=%d coinflip=%d pickups=%d chance=%d speed=%d",
            roll_ok ? 1 : 0,
            jail_ok ? 1 : 0,
            coinflip_ok ? 1 : 0,
            pickups_ok ? 1 : 0,
            chance_ok ? 1 : 0,
            speed_ok ? 1 : 0);
        return false;
    }

    g_hooks_installed.store(true);
    LOGI("Hook install summary: roll=1 jail=1 coinflip=1 pickups=1 chance=1 speed=1");
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
