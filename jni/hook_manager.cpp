#include "hook_manager.h"

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Loading.hpp>
#include <BNM/MethodBase.hpp>
#include <BNM/Utils.hpp>
#include <android/log.h>
#include <dlfcn.h>
#include <dobby.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <atomic>
#include <string_view>

#define LOG_TAG "HookManager"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

constexpr int kIl2CppWaitAttempts = 600;
constexpr useconds_t kIl2CppWaitSleepUs = 100000;
constexpr int kImageWaitAttempts = 100;
constexpr useconds_t kImageWaitSleepUs = 200000;
constexpr unsigned int kGameWarmupSeconds = 3;

std::atomic<bool> g_bnm_bootstrap_started{false};
std::atomic<bool> g_bnm_loaded_callback_seen{false};
std::atomic<bool> g_install_thread_started{false};
std::atomic<bool> g_hooks_installed{false};

using DebugLogFn = void (*)(BNM::IL2CPP::Il2CppObject* message, BNM::IL2CPP::MethodInfo* method);

namespace Originals {
DebugLogFn DebugLog = nullptr;
}

BNM::IL2CPP::Il2CppClass* g_system_string_class = nullptr;

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

BNM::Image WaitForImage(std::string_view image_name) {
    for (int attempt = 1; attempt <= kImageWaitAttempts; ++attempt) {
        BNM::Image image(image_name);
        if (image.IsValid()) {
            LOGD("Image %.*s found on attempt %d", static_cast<int>(image_name.size()), image_name.data(), attempt);
            return image;
        }
        usleep(kImageWaitSleepUs);
    }

    return {};
}

void HookedDebugLog(BNM::IL2CPP::Il2CppObject* message, BNM::IL2CPP::MethodInfo* method) {
    if (Originals::DebugLog != nullptr) {
        Originals::DebugLog(message, method);
    }

    if (message == nullptr || g_system_string_class == nullptr || message->klass != g_system_string_class) {
        return;
    }

    auto* text = reinterpret_cast<BNM::Structures::Mono::String*>(message);
    if (text->length <= 0) {
        return;
    }

    const std::string utf8 = text->str();
    if (!utf8.empty()) {
        LOGD("[Unity] %s", utf8.c_str());
    }
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
    return true;
}

void HookManager::OnBNMLoaded() {
    if (g_bnm_loaded_callback_seen.exchange(true)) {
        LOGD("BNM loaded callback already handled");
        return;
    }

    LOGI("BNM reported IL2CPP ready");
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

    if (!InstallUnityDebugLogHook()) {
        return false;
    }

    g_hooks_installed.store(true);
    return true;
}

bool HookManager::InstallUnityDebugLogHook() {
    BNM::Image unity_core = WaitForImage("UnityEngine.CoreModule.dll");
    if (!unity_core.IsValid()) {
        LOGE("UnityEngine.CoreModule.dll not found");
        return false;
    }

    BNM::Class debug_class("UnityEngine", "Debug", unity_core);
    if (!debug_class.IsValid()) {
        LOGE("UnityEngine.Debug class not found");
        return false;
    }

    BNM::Class string_class("System", "String");
    if (string_class.IsValid()) {
        g_system_string_class = string_class.GetClass();
    } else {
        LOGD("System.String class not found; string payload logging disabled");
    }

    BNM::MethodBase log_method = debug_class.GetMethod("Log", 1);
    if (!log_method.IsValid()) {
        LOGE("UnityEngine.Debug.Log(1) not found");
        return false;
    }

    void* target = reinterpret_cast<void*>(log_method.GetOffset());
    if (target == nullptr) {
        LOGE("UnityEngine.Debug.Log target address is null");
        return false;
    }

    return SafeHook(
        target,
        reinterpret_cast<void*>(&HookedDebugLog),
        reinterpret_cast<void**>(&Originals::DebugLog),
        "UnityEngine.Debug.Log(object)");
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

