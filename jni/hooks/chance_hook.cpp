#include "chance_hook.h"

#include <android/log.h>

#include <atomic>
#include <cstdint>
#include <inttypes.h>

#include "hook_utils.h"

#define LOG_TAG "ChanceHooks"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

using DrawCardFn = void* (*)(void* movement_context, void* execution_context);

namespace Originals {
DrawCardFn DrawCard = nullptr;
}  // namespace Originals

std::atomic<uint64_t> g_draw_card_calls{0};

void* HookedCardHandlerDrawCard(void* movement_context, void* execution_context) {
    const uint64_t call_count = g_draw_card_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    LOGD("CardHandler.DrawCard hit #%" PRIu64, call_count);

    if (Originals::DrawCard == nullptr) {
        return nullptr;
    }
    return Originals::DrawCard(movement_context, execution_context);
}

}  // namespace

bool Hooks::Chance::Install() {
    const bool ok = Hooks::Internal::ResolveAndHook(
        "Tophat.Common.dll",
        "Tophat.Common.Rolling",
        "CardHandler",
        "DrawCard",
        2,
        reinterpret_cast<void*>(&HookedCardHandlerDrawCard),
        reinterpret_cast<void**>(&Originals::DrawCard),
        "CardHandler.DrawCard");

    if (!ok) {
        LOGE("Chance hook installation failed");
        return false;
    }

    LOGI("Chance hooks installed");
    return true;
}

