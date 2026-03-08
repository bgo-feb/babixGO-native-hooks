#include "roll_hook.h"

#include <android/log.h>

#include <atomic>
#include <cstdint>
#include <inttypes.h>

#include "hook_utils.h"

#define LOG_TAG "RollHooks"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

using FlusherUpdateFn = void (*)(void* thiz);
using AttemptRollFn = void (*)(void* thiz);
using SetMultiplierFn = void (*)(void* thiz, int multiplier);
using GetCurrentMultiplierFn = int (*)(void* thiz, bool check_multiplier);

namespace Originals {
FlusherUpdateFn FlusherUpdate = nullptr;
AttemptRollFn AttemptRoll = nullptr;
SetMultiplierFn SetMultiplier = nullptr;
GetCurrentMultiplierFn GetCurrentMultiplier = nullptr;
}  // namespace Originals

std::atomic<uint64_t> g_flusher_update_calls{0};
std::atomic<uint64_t> g_attempt_roll_calls{0};
std::atomic<uint64_t> g_set_multiplier_calls{0};
std::atomic<uint64_t> g_get_multiplier_calls{0};

void HookedTophatClientActionsFlusherUpdate(void* thiz) {
    const uint64_t call_count = g_flusher_update_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if ((call_count % 300) == 1) {
        LOGD("TophatClientActionsFlusher.Update hits=%" PRIu64, call_count);
    }
    if (Originals::FlusherUpdate != nullptr) {
        Originals::FlusherUpdate(thiz);
    }
}

void HookedRollServiceAttemptRoll(void* thiz) {
    const uint64_t call_count = g_attempt_roll_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    LOGI("RollService.AttemptRoll hit #%" PRIu64, call_count);
    if (Originals::AttemptRoll != nullptr) {
        Originals::AttemptRoll(thiz);
    }
}

void HookedRollServiceSetMultiplier(void* thiz, int multiplier) {
    const uint64_t call_count = g_set_multiplier_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    LOGD("RollService.SetMultiplier(%d) hit #%" PRIu64, multiplier, call_count);
    if (Originals::SetMultiplier != nullptr) {
        Originals::SetMultiplier(thiz, multiplier);
    }
}

int HookedRollServiceGetCurrentMultiplier(void* thiz, bool check_multiplier) {
    const uint64_t call_count = g_get_multiplier_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    int value = 0;
    if (Originals::GetCurrentMultiplier != nullptr) {
        value = Originals::GetCurrentMultiplier(thiz, check_multiplier);
    }
    if ((call_count % 60) == 1) {
        LOGD(
            "RollService.GetCurrentMultiplier(check=%d) -> %d (#%" PRIu64 ")",
            check_multiplier ? 1 : 0,
            value,
            call_count);
    }
    return value;
}

}  // namespace

bool Hooks::Roll::Install() {
    bool ok = true;

    ok = Hooks::Internal::ResolveAndHook(
             "Tophat.Client.dll",
             "Tophat.Client.ClientActions",
             "TophatClientActionsFlusher",
             "Update",
             0,
             reinterpret_cast<void*>(&HookedTophatClientActionsFlusherUpdate),
             reinterpret_cast<void**>(&Originals::FlusherUpdate),
             "TophatClientActionsFlusher.Update") &&
         ok;

    ok = Hooks::Internal::ResolveAndHook(
             "Tophat.Client.dll",
             "Tophat.Client.DiceRoll",
             "RollService",
             "AttemptRoll",
             0,
             reinterpret_cast<void*>(&HookedRollServiceAttemptRoll),
             reinterpret_cast<void**>(&Originals::AttemptRoll),
             "RollService.AttemptRoll") &&
         ok;

    ok = Hooks::Internal::ResolveAndHook(
             "Tophat.Client.dll",
             "Tophat.Client.DiceRoll",
             "RollService",
             "SetMultiplier",
             1,
             reinterpret_cast<void*>(&HookedRollServiceSetMultiplier),
             reinterpret_cast<void**>(&Originals::SetMultiplier),
             "RollService.SetMultiplier") &&
         ok;

    ok = Hooks::Internal::ResolveAndHook(
             "Tophat.Client.dll",
             "Tophat.Client.DiceRoll",
             "RollService",
             "GetCurrentMultiplier",
             1,
             reinterpret_cast<void*>(&HookedRollServiceGetCurrentMultiplier),
             reinterpret_cast<void**>(&Originals::GetCurrentMultiplier),
             "RollService.GetCurrentMultiplier") &&
         ok;

    if (!ok) {
        LOGE("Roll hook installation incomplete");
        return false;
    }

    LOGI("Roll hooks installed");
    return true;
}
