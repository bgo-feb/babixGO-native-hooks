#include "jail_hook.h"

#include <android/log.h>

#include <atomic>
#include <cstdint>
#include <inttypes.h>

#include "hook_utils.h"
#include "../ipc_feed.h"

#define LOG_TAG "JailHooks"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

using BeginRollFn = void* (*)(void* thiz);
using OnSingleRollCompleteFn = void (*)(void* thiz);
using OnRollButtonPressedFn = void (*)(void* thiz);
using SetStateFn = void (*)(void* thiz, int state);
using TryInterruptAutoRollFn = bool (*)(void* thiz);
using UpdateRollResultOnRollFn = void (*)(void* thiz, int index);

namespace Originals {
BeginRollFn BeginRoll = nullptr;
OnSingleRollCompleteFn OnSingleRollComplete = nullptr;
OnRollButtonPressedFn OnRollButtonPressed = nullptr;
SetStateFn SetState = nullptr;
TryInterruptAutoRollFn TryInterruptAutoRoll = nullptr;
UpdateRollResultOnRollFn UpdateRollResultOnRoll = nullptr;
}  // namespace Originals

std::atomic<uint64_t> g_begin_roll_calls{0};
std::atomic<uint64_t> g_on_single_roll_complete_calls{0};
std::atomic<uint64_t> g_on_roll_button_pressed_calls{0};
std::atomic<uint64_t> g_set_state_calls{0};
std::atomic<uint64_t> g_try_interrupt_calls{0};
std::atomic<uint64_t> g_update_roll_result_calls{0};

const char* ToStateName(int state) {
    switch (state) {
        case 0:
            return "Intro";
        case 1:
            return "Minigame";
        case 2:
            return "Succeeded";
        case 3:
            return "Failed";
        case 4:
            return "GetOutOfJailFree";
        case 5:
            return "Outro";
        case 6:
            return "Teardown";
        default:
            return "Unknown";
    }
}

void* HookedBeginRoll(void* thiz) {
    const uint64_t call_count = g_begin_roll_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    void* result = nullptr;
    if (Originals::BeginRoll != nullptr) {
        result = Originals::BeginRoll(thiz);
    }

    IPCFeed::Publish("jail:begin_roll");
    LOGI(
        "EscapeJailService.BeginRoll self=%p result=%p call=%" PRIu64,
        thiz,
        result,
        call_count);
    return result;
}

void HookedOnSingleRollComplete(void* thiz) {
    const uint64_t call_count = g_on_single_roll_complete_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call_count <= 10 || (call_count % 25) == 0) {
        LOGD("EscapeJailService.OnSingleRollComplete self=%p call=%" PRIu64, thiz, call_count);
    }
    if (Originals::OnSingleRollComplete != nullptr) {
        Originals::OnSingleRollComplete(thiz);
    }
}

void HookedOnRollButtonPressed(void* thiz) {
    const uint64_t call_count = g_on_roll_button_pressed_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    IPCFeed::Publish("jail:roll_btn");
    LOGI("EscapeJailService.OnRollButtonPressed self=%p call=%" PRIu64, thiz, call_count);
    if (Originals::OnRollButtonPressed != nullptr) {
        Originals::OnRollButtonPressed(thiz);
    }
}

void HookedSetState(void* thiz, int state) {
    const uint64_t call_count = g_set_state_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    {
        char ipc_msg[64];
        snprintf(ipc_msg, sizeof(ipc_msg), "jail:state=%d(%s)", state, ToStateName(state));
        IPCFeed::Publish(ipc_msg);
    }
    LOGI(
        "EscapeJailService.SetState state=%d(%s) self=%p call=%" PRIu64,
        state,
        ToStateName(state),
        thiz,
        call_count);
    if (Originals::SetState != nullptr) {
        Originals::SetState(thiz, state);
    }
}

bool HookedTryInterruptAutoRoll(void* thiz) {
    const uint64_t call_count = g_try_interrupt_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    bool interrupted = false;
    if (Originals::TryInterruptAutoRoll != nullptr) {
        interrupted = Originals::TryInterruptAutoRoll(thiz);
    }
    LOGD(
        "EscapeJailService.TryInterruptAutoRoll self=%p -> %d call=%" PRIu64,
        thiz,
        interrupted ? 1 : 0,
        call_count);
    return interrupted;
}

void HookedUpdateRollResultOnRoll(void* thiz, int index) {
    const uint64_t call_count = g_update_roll_result_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call_count <= 20 || (call_count % 40) == 0) {
        LOGD(
            "EscapeJailService.UpdateRollResultOnRoll index=%d self=%p call=%" PRIu64,
            index,
            thiz,
            call_count);
    }
    if (Originals::UpdateRollResultOnRoll != nullptr) {
        Originals::UpdateRollResultOnRoll(thiz, index);
    }
}

}  // namespace

bool Hooks::Jail::Install() {
    bool required_ok = true;
    bool optional_ok = true;

    required_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.Jail",
                      "EscapeJailService",
                      "BeginRoll",
                      0,
                      reinterpret_cast<void*>(&HookedBeginRoll),
                      reinterpret_cast<void**>(&Originals::BeginRoll),
                      "EscapeJailService.BeginRoll") &&
                  required_ok;

    required_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.Jail",
                      "EscapeJailService",
                      "SetState",
                      1,
                      reinterpret_cast<void*>(&HookedSetState),
                      reinterpret_cast<void**>(&Originals::SetState),
                      "EscapeJailService.SetState") &&
                  required_ok;

    required_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.Jail",
                      "EscapeJailService",
                      "OnRollButtonPressed",
                      0,
                      reinterpret_cast<void*>(&HookedOnRollButtonPressed),
                      reinterpret_cast<void**>(&Originals::OnRollButtonPressed),
                      "EscapeJailService.OnRollButtonPressed") &&
                  required_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.Jail",
                      "EscapeJailService",
                      "OnSingleRollComplete",
                      0,
                      reinterpret_cast<void*>(&HookedOnSingleRollComplete),
                      reinterpret_cast<void**>(&Originals::OnSingleRollComplete),
                      "EscapeJailService.OnSingleRollComplete") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.Jail",
                      "EscapeJailService",
                      "TryInterruptAutoRoll",
                      0,
                      reinterpret_cast<void*>(&HookedTryInterruptAutoRoll),
                      reinterpret_cast<void**>(&Originals::TryInterruptAutoRoll),
                      "EscapeJailService.TryInterruptAutoRoll") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.Jail",
                      "EscapeJailService",
                      "UpdateRollResultOnRoll",
                      1,
                      reinterpret_cast<void*>(&HookedUpdateRollResultOnRoll),
                      reinterpret_cast<void**>(&Originals::UpdateRollResultOnRoll),
                      "EscapeJailService.UpdateRollResultOnRoll") &&
                  optional_ok;

    if (!required_ok) {
        LOGE("Jail hook installation failed (required hooks)");
        return false;
    }

    if (!optional_ok) {
        LOGW("Jail hook installation partial (optional hooks missing)");
    }

    LOGI("Jail hooks installed");
    return true;
}
