#include "hook_utils.h"

#include <BNM/Class.hpp>
#include <BNM/Image.hpp>
#include <BNM/MethodBase.hpp>
#include <android/log.h>
#include <unistd.h>

#include "../hook_manager.h"

#define LOG_TAG "HookResolver"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

constexpr int kImageWaitAttempts = 100;
constexpr useconds_t kImageWaitSleepUs = 200000;

BNM::Image WaitForImage(const char* image_name) {
    for (int attempt = 1; attempt <= kImageWaitAttempts; ++attempt) {
        BNM::Image image(image_name);
        if (image.IsValid()) {
            LOGD("Image %s found on attempt %d", image_name, attempt);
            return image;
        }
        usleep(kImageWaitSleepUs);
    }
    return {};
}

}  // namespace

bool Hooks::Internal::ResolveAndHook(
    const char* image_name,
    const char* namespace_name,
    const char* class_name,
    const char* method_name,
    int parameter_count,
    void* detour,
    void** original,
    const char* label) {
    BNM::Image image = WaitForImage(image_name);
    if (!image.IsValid()) {
        LOGE("Image not found: %s (%s)", image_name, label);
        return false;
    }

    BNM::Class klass(namespace_name, class_name, image);
    if (!klass.IsValid()) {
        LOGE("Class not found: %s.%s (%s)", namespace_name, class_name, label);
        return false;
    }

    BNM::MethodBase method = klass.GetMethod(method_name, parameter_count);
    if (!method.IsValid()) {
        LOGE(
            "Method not found: %s.%s::%s(%d) (%s)",
            namespace_name,
            class_name,
            method_name,
            parameter_count,
            label);
        return false;
    }

    void* target = reinterpret_cast<void*>(method.GetOffset());
    if (target == nullptr) {
        LOGE("Method offset is null: %s", label);
        return false;
    }

    return HookManager::SafeHook(target, detour, original, label);
}

