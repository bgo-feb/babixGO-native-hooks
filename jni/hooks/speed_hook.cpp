#include "speed_hook.h"

#include <android/log.h>

#include <atomic>
#include <bit>
#include <cstdint>
#include <inttypes.h>

#include "hook_utils.h"
#include "../ipc_feed.h"

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

// Speed modification settings
std::atomic<uint32_t> g_speed_multiplier_bits{std::bit_cast<uint32_t>(2.0f)};
std::atomic<bool> g_speed_modification_enabled{true};

void HookedSetTimeScale(float value) {
    const uint64_t call_count = g_set_time_scale_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const float last_value = std::bit_cast<float>(g_last_set_bits.exchange(std::bit_cast<uint32_t>(value)));

    float modified_value = value;
    if (g_speed_modification_enabled.load(std::memory_order_relaxed)) {
        const float multiplier = std::bit_cast<float>(g_speed_multiplier_bits.load(std::memory_order_relaxed));
        modified_value = value * multiplier;
        
        // Clamp to reasonable range
        if (modified_value < 0.0f) modified_value = 0.0f;
        if (modified_value > 10.0f) modified_value = 10.0f;
    }

    if (call_count <= 20 || (call_count % 120) == 0 || (value < 0.99f || value > 1.01f) || (last_value != value)) {
        if (g_speed_modification_enabled.load(std::memory_order_relaxed)) {
            LOGD("Time.set_timeScale(%0.3f -> %0.3f) call=%" PRIu64, value, modified_value, call_count);
        } else {
            LOGD("Time.set_timeScale(%0.3f) call=%" PRIu64, value, call_count);
        }
    }

    if (last_value != value) {
        char ipc_msg[64];
        snprintf(ipc_msg, sizeof(ipc_msg), "speed:scale=%.3f->%.3f", value, modified_value);
        IPCFeed::Publish(ipc_msg);
    }

    if (Originals::SetTimeScale != nullptr) {
        Originals::SetTimeScale(modified_value);
    }
}

float HookedGetTimeScale() {
    const uint64_t call_count = g_get_time_scale_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    float value = 0.0f;
    if (Originals::GetTimeScale != nullptr) {
        value = Originals::GetTimeScale();
    }

    // The stored timeScale is already (original_input * multiplier) because set_timeScale applied
    // the multiplier when it was set. To return a value consistent with what the caller originally
    // passed to set_timeScale, we divide by the multiplier here.
    float reported_value = value;
    if (g_speed_modification_enabled.load(std::memory_order_relaxed)) {
        const float multiplier = std::bit_cast<float>(g_speed_multiplier_bits.load(std::memory_order_relaxed));
        if (multiplier > 0.0001f) {
            reported_value = value / multiplier;
        }
    }

    if ((call_count % 240) == 1) {
        if (g_speed_modification_enabled.load(std::memory_order_relaxed)) {
            LOGD("Time.get_timeScale() stored=%0.3f reported=%0.3f call=%" PRIu64, value, reported_value, call_count);
        } else {
            LOGD("Time.get_timeScale() -> %0.3f call=%" PRIu64, value, call_count);
        }
    }
    return reported_value;
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

    const float multiplier = std::bit_cast<float>(g_speed_multiplier_bits.load(std::memory_order_relaxed));
    LOGI("Speed hooks installed (multiplier: %.1fx, enabled: %s)", multiplier, 
         g_speed_modification_enabled.load(std::memory_order_relaxed) ? "true" : "false");
    return true;
}

void Hooks::Speed::SetSpeedMultiplier(float multiplier) {
    g_speed_multiplier_bits.store(std::bit_cast<uint32_t>(multiplier), std::memory_order_relaxed);
    LOGI("Speed multiplier set to %.2fx", multiplier);
}

float Hooks::Speed::GetSpeedMultiplier() {
    return std::bit_cast<float>(g_speed_multiplier_bits.load(std::memory_order_relaxed));
}

void Hooks::Speed::EnableSpeedModification(bool enabled) {
    g_speed_modification_enabled.store(enabled, std::memory_order_relaxed);
    LOGI("Speed modification %s", enabled ? "enabled" : "disabled");
}

bool Hooks::Speed::IsSpeedModificationEnabled() {
    return g_speed_modification_enabled.load(std::memory_order_relaxed);
}
