#include "coinflip_hook.h"

#include <android/log.h>

#include <atomic>
#include <cstdint>
#include <inttypes.h>

#include "hook_utils.h"

#define LOG_TAG "CoinFlipHooks"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

using TryFlipFn = void (*)(void* thiz);
using ChangeAnimationStateFn = void (*)(void* thiz, int state);
using GetMaxFlipsFn = int (*)(void* thiz);
using OnInitializeFn = bool (*)(void* thiz);
using RefreshEventModelFn = void (*)(void* thiz);

using GetCurrentFlipsFn = int (*)(void* player_state);
using GetNumberOfHeadsFn = int (*)(void* player_state);
using GetHeadChanceFn = int (*)(void* player_state, float current_chance);

namespace Originals {
TryFlipFn TryFlip = nullptr;
ChangeAnimationStateFn ChangeAnimationState = nullptr;
GetMaxFlipsFn GetMaxFlips = nullptr;
OnInitializeFn OnInitialize = nullptr;
RefreshEventModelFn RefreshEventModel = nullptr;

GetCurrentFlipsFn GetCurrentFlips = nullptr;
GetNumberOfHeadsFn GetNumberOfHeads = nullptr;
GetHeadChanceFn GetHeadChance = nullptr;
}  // namespace Originals

std::atomic<uint64_t> g_try_flip_calls{0};
std::atomic<uint64_t> g_change_animation_state_calls{0};
std::atomic<uint64_t> g_get_max_flips_calls{0};
std::atomic<uint64_t> g_on_initialize_calls{0};
std::atomic<uint64_t> g_refresh_event_model_calls{0};

std::atomic<uint64_t> g_get_current_flips_calls{0};
std::atomic<uint64_t> g_get_number_of_heads_calls{0};
std::atomic<uint64_t> g_get_head_chance_calls{0};

const char* ToAnimationStateName(int state) {
    switch (state) {
        case 0:
            return "None";
        case 1:
            return "Open";
        case 2:
            return "Idle";
        case 3:
            return "Flipping";
        case 4:
            return "ResolveFlip";
        case 5:
            return "Rewarding";
        case 6:
            return "Close";
        case 7:
            return "TryToFlip";
        case 8:
            return "ReadyToFlip";
        case 9:
            return "Unlucky";
        case 10:
            return "Success";
        case 11:
            return "StartFlipping";
        default:
            return "Unknown";
    }
}

void HookedTryFlip(void* thiz) {
    const uint64_t call_count = g_try_flip_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    LOGI("CoinFlipService.TryFlip self=%p call=%" PRIu64, thiz, call_count);
    if (Originals::TryFlip != nullptr) {
        Originals::TryFlip(thiz);
    }
}

void HookedChangeAnimationState(void* thiz, int state) {
    const uint64_t call_count = g_change_animation_state_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    LOGD(
        "CoinFlipService.ChangeAnimationState state=%d(%s) self=%p call=%" PRIu64,
        state,
        ToAnimationStateName(state),
        thiz,
        call_count);
    if (Originals::ChangeAnimationState != nullptr) {
        Originals::ChangeAnimationState(thiz, state);
    }
}

int HookedGetMaxFlips(void* thiz) {
    const uint64_t call_count = g_get_max_flips_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    int max_flips = 0;
    if (Originals::GetMaxFlips != nullptr) {
        max_flips = Originals::GetMaxFlips(thiz);
    }
    if (call_count <= 20 || (call_count % 60) == 0) {
        LOGD("CoinFlipService.GetMaxFlips self=%p -> %d call=%" PRIu64, thiz, max_flips, call_count);
    }
    return max_flips;
}

bool HookedOnInitialize(void* thiz) {
    const uint64_t call_count = g_on_initialize_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    bool ok = false;
    if (Originals::OnInitialize != nullptr) {
        ok = Originals::OnInitialize(thiz);
    }
    LOGI("CoinFlipService.OnInitialize self=%p -> %d call=%" PRIu64, thiz, ok ? 1 : 0, call_count);
    return ok;
}

void HookedRefreshEventModel(void* thiz) {
    const uint64_t call_count = g_refresh_event_model_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    LOGD("CoinFlipService.RefreshEventModel self=%p call=%" PRIu64, thiz, call_count);
    if (Originals::RefreshEventModel != nullptr) {
        Originals::RefreshEventModel(thiz);
    }
}

int HookedModifierExecutorGetCurrentFlips(void* player_state) {
    const uint64_t call_count = g_get_current_flips_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    int value = 0;
    if (Originals::GetCurrentFlips != nullptr) {
        value = Originals::GetCurrentFlips(player_state);
    }
    if (call_count <= 20 || (call_count % 90) == 0) {
        LOGD(
            "CoinFlipModifierExecutor.GetCurrentFlips state=%p -> %d call=%" PRIu64,
            player_state,
            value,
            call_count);
    }
    return value;
}

int HookedModifierExecutorGetNumberOfHeads(void* player_state) {
    const uint64_t call_count = g_get_number_of_heads_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    int value = 0;
    if (Originals::GetNumberOfHeads != nullptr) {
        value = Originals::GetNumberOfHeads(player_state);
    }
    if (call_count <= 20 || (call_count % 90) == 0) {
        LOGD(
            "CoinFlipModifierExecutor.GetNumberOfHeads state=%p -> %d call=%" PRIu64,
            player_state,
            value,
            call_count);
    }
    return value;
}

int HookedModifierExecutorGetHeadChance(void* player_state, float current_chance) {
    const uint64_t call_count = g_get_head_chance_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    int value = 0;
    if (Originals::GetHeadChance != nullptr) {
        value = Originals::GetHeadChance(player_state, current_chance);
    }
    if (call_count <= 40 || (call_count % 120) == 0) {
        LOGD(
            "CoinFlipModifierExecutor.GetHeadChance state=%p current=%0.3f -> %d call=%" PRIu64,
            player_state,
            current_chance,
            value,
            call_count);
    }
    return value;
}

}  // namespace

bool Hooks::CoinFlip::Install() {
    bool required_ok = true;
    bool optional_ok = true;

    required_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.ForeverGames.CoinFlip",
                      "CoinFlipService",
                      "TryFlip",
                      0,
                      reinterpret_cast<void*>(&HookedTryFlip),
                      reinterpret_cast<void**>(&Originals::TryFlip),
                      "CoinFlipService.TryFlip") &&
                  required_ok;

    required_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.ForeverGames.CoinFlip",
                      "CoinFlipService",
                      "ChangeAnimationState",
                      1,
                      reinterpret_cast<void*>(&HookedChangeAnimationState),
                      reinterpret_cast<void**>(&Originals::ChangeAnimationState),
                      "CoinFlipService.ChangeAnimationState") &&
                  required_ok;

    required_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.ForeverGames.CoinFlip",
                      "CoinFlipService",
                      "GetMaxFlips",
                      0,
                      reinterpret_cast<void*>(&HookedGetMaxFlips),
                      reinterpret_cast<void**>(&Originals::GetMaxFlips),
                      "CoinFlipService.GetMaxFlips") &&
                  required_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.ForeverGames.CoinFlip",
                      "CoinFlipService",
                      "OnInitialize",
                      0,
                      reinterpret_cast<void*>(&HookedOnInitialize),
                      reinterpret_cast<void**>(&Originals::OnInitialize),
                      "CoinFlipService.OnInitialize") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.ForeverGames.CoinFlip",
                      "CoinFlipService",
                      "RefreshEventModel",
                      0,
                      reinterpret_cast<void*>(&HookedRefreshEventModel),
                      reinterpret_cast<void**>(&Originals::RefreshEventModel),
                      "CoinFlipService.RefreshEventModel") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Common.dll",
                      "Tophat.Common.ForeverGames.CoinFlip.Modifiers",
                      "CoinFlipModifierExecutor",
                      "GetCurrentFlips",
                      1,
                      reinterpret_cast<void*>(&HookedModifierExecutorGetCurrentFlips),
                      reinterpret_cast<void**>(&Originals::GetCurrentFlips),
                      "CoinFlipModifierExecutor.GetCurrentFlips") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Common.dll",
                      "Tophat.Common.ForeverGames.CoinFlip.Modifiers",
                      "CoinFlipModifierExecutor",
                      "GetNumberOfHeads",
                      1,
                      reinterpret_cast<void*>(&HookedModifierExecutorGetNumberOfHeads),
                      reinterpret_cast<void**>(&Originals::GetNumberOfHeads),
                      "CoinFlipModifierExecutor.GetNumberOfHeads") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Common.dll",
                      "Tophat.Common.ForeverGames.CoinFlip.Modifiers",
                      "CoinFlipModifierExecutor",
                      "GetHeadChance",
                      2,
                      reinterpret_cast<void*>(&HookedModifierExecutorGetHeadChance),
                      reinterpret_cast<void**>(&Originals::GetHeadChance),
                      "CoinFlipModifierExecutor.GetHeadChance") &&
                  optional_ok;

    if (!required_ok) {
        LOGE("CoinFlip hook installation failed (required hooks)");
        return false;
    }

    if (!optional_ok) {
        LOGW("CoinFlip hook installation partial (optional hooks missing)");
    }

    LOGI("CoinFlip hooks installed");
    return true;
}
