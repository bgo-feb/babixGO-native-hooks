#include "speed_hook.h"

#include <android/log.h>

#include <atomic>
#include <bit>
#include <cstdint>
#include <inttypes.h>

#include "hook_utils.h"

#define LOG_TAG "SpeedHooks"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

using SetTimeScaleFn = void (*)(float value);
using GetTimeScaleFn = float (*)();

namespace Originals {
SetTimeScaleFn SetTimeScale = nullptr;
GetTimeScaleFn GetTimeScale = nullptr;
}  // namespace Originals

std::atomic<uint64_t> g_set_time_scale_calls{0};
std::atomic<uint64_t> g_get_time_scale_calls{0};
std::atomic<uint32_t> g_last_set_bits{std::bit_cast<uint32_t>(1.0f)};

void HookedSetTimeScale(float value) {
    const uint64_t call_count = g_set_time_scale_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const float last_value = std::bit_cast<float>(g_last_set_bits.exchange(std::bit_cast<uint32_t>(value)));

    if (call_count <= 20 || (call_count % 120) == 0 || (value < 0.99f || value > 1.01f) || (last_value != value)) {
        LOGD("Time.set_timeScale(%0.3f) call=%" PRIu64, value, call_count);
    }

    if (Originals::SetTimeScale != nullptr) {
        Originals::SetTimeScale(value);
    }
}

float HookedGetTimeScale() {
    const uint64_t call_count = g_get_time_scale_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    float value = 0.0f;
    if (Originals::GetTimeScale != nullptr) {
        value = Originals::GetTimeScale();
    }

    if ((call_count % 240) == 1) {
        LOGD("Time.get_timeScale() -> %0.3f call=%" PRIu64, value, call_count);
    }
    return value;
}

}  // namespace

bool Hooks::Speed::Install() {
    bool ok = true;

    ok = Hooks::Internal::ResolveAndHook(
             "UnityEngine.CoreModule.dll",
             "UnityEngine",
             "Time",
             "set_timeScale",
             1,
             reinterpret_cast<void*>(&HookedSetTimeScale),
             reinterpret_cast<void**>(&Originals::SetTimeScale),
             "UnityEngine.Time.set_timeScale") &&
         ok;

    ok = Hooks::Internal::ResolveAndHook(
             "UnityEngine.CoreModule.dll",
             "UnityEngine",
             "Time",
             "get_timeScale",
             0,
             reinterpret_cast<void*>(&HookedGetTimeScale),
             reinterpret_cast<void**>(&Originals::GetTimeScale),
             "UnityEngine.Time.get_timeScale") &&
         ok;

    if (!ok) {
        LOGE("Speed hook installation failed");
        return false;
    }

    LOGI("Speed hooks installed");
    return true;
}

