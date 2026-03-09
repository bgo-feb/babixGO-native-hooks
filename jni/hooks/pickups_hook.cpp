#include "pickups_hook.h"

#include <android/log.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <inttypes.h>

#include "hook_utils.h"
#include "../ipc_feed.h"

#define LOG_TAG "PickupHooks"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

using CheckForPickupsAwaitingSpawnFn = void (*)(void* thiz, int tile_index);
using EvaluateBoardPickupsFn = void (*)(void* thiz, bool generate_new_pickups, bool use_quick_spawn);
using SetNextMoveTokenAnimatorParamsFn = void (*)(void* thiz, int start_index, void* move_params);
using TriggerOnLandHandlerFn = void* (*)(void* thiz, int tile_index);
using RemoveBoardPickupFn = void (*)(void* thiz, int tile_index);
using AnimateCollectNearestPickupsFn = void* (*)(void* thiz, void* tiles, int reward_type, int animation_trigger, int effect_type);

using OnLandHandlerSourceFn = void* (*)(void* thiz, void* pickup);
using OnCollectNearestHandlerSourceFn = void* (*)(void* thiz, void* pickups, int effect_type);
using GetPickupsToSpawnFn = void* (*)(void* thiz, void* all_existing_view_pickups);

using ProcessPickupsFn = void* (*)(void* movement_context, void* execution_context, int ledger_entry_type);
using ProcessClosestPickupsFn = void* (*)(void* movement_context, void* execution_context, void* arguments, int ledger_entry_type);
using HandlePickupFn = void* (*)(void* pickup, void* pickup_state, void* configs, void* tile, int multiplier, void* context, void* objective_handler, int ledger_entry_type);
using AwardPickupContentsFn = void* (*)(void* pickup, int multiplier, void* context, int ledger_entry_type);

namespace Originals {
CheckForPickupsAwaitingSpawnFn CheckForPickupsAwaitingSpawn = nullptr;
EvaluateBoardPickupsFn EvaluateBoardPickups = nullptr;
SetNextMoveTokenAnimatorParamsFn SetNextMoveTokenAnimatorParams = nullptr;
TriggerOnLandHandlerFn TriggerOnLandHandler = nullptr;
RemoveBoardPickupFn RemoveBoardPickup = nullptr;
AnimateCollectNearestPickupsFn AnimateCollectNearestPickups = nullptr;

OnLandHandlerSourceFn SpecialCurrencyOnLandHandler = nullptr;
OnCollectNearestHandlerSourceFn SpecialCurrencyOnCollectNearestHandler = nullptr;
GetPickupsToSpawnFn CoreMechanicGetPickupsToSpawn = nullptr;
GetPickupsToSpawnFn ShieldGetPickupsToSpawn = nullptr;
GetPickupsToSpawnFn RentDueGetPickupsToSpawn = nullptr;
GetPickupsToSpawnFn BonusBoardGetPickupsToSpawn = nullptr;

ProcessPickupsFn ProcessPickups = nullptr;
ProcessClosestPickupsFn ProcessClosestPickups = nullptr;
HandlePickupFn HandlePickup = nullptr;
AwardPickupContentsFn AwardPickupContents = nullptr;
}  // namespace Originals

std::atomic<uint64_t> g_check_for_pickups_calls{0};
std::atomic<uint64_t> g_evaluate_pickups_calls{0};
std::atomic<uint64_t> g_set_next_move_calls{0};
std::atomic<uint64_t> g_trigger_on_land_calls{0};
std::atomic<uint64_t> g_remove_pickup_calls{0};
std::atomic<uint64_t> g_collect_nearest_calls{0};

std::atomic<uint64_t> g_special_currency_on_land_calls{0};
std::atomic<uint64_t> g_special_currency_collect_nearest_calls{0};
std::atomic<uint64_t> g_core_mechanic_spawn_calls{0};
std::atomic<uint64_t> g_shield_spawn_calls{0};
std::atomic<uint64_t> g_rent_due_spawn_calls{0};
std::atomic<uint64_t> g_bonus_board_spawn_calls{0};

std::atomic<uint64_t> g_process_pickups_calls{0};
std::atomic<uint64_t> g_process_closest_pickups_calls{0};
std::atomic<uint64_t> g_handle_pickup_calls{0};
std::atomic<uint64_t> g_award_pickup_contents_calls{0};

constexpr ptrdiff_t kPickupTypeOffset = 0x18;
constexpr ptrdiff_t kPickupTileIndexOffset = 0x28;

int ReadPickupType(void* pickup) {
    if (pickup == nullptr) {
        return -1;
    }
    return *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pickup) + kPickupTypeOffset);
}

int ReadPickupTileIndex(void* pickup) {
    if (pickup == nullptr) {
        return -1;
    }
    return *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pickup) + kPickupTileIndexOffset);
}

const char* PickupTypeName(int type) {
    switch (type) {
        case 0:
            return "Unknown";
        case 1:
            return "BoardPickup";
        case 2:
            return "CashBag";
        case 5:
            return "Quest";
        case 6:
            return "Shields";
        case 7:
            return "RentDue";
        case 8:
            return "Block";
        case 9:
            return "SpecialCurrency";
        case 10:
            return "BonusBoardReward";
        default:
            return "Other";
    }
}

void HookedCheckForPickupsAwaitingSpawn(void* thiz, int tile_index) {
    const uint64_t call_count = g_check_for_pickups_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call_count <= 30 || (call_count % 120) == 0) {
        LOGD(
            "BoardPickupsService.CheckForPickupsAwaitingSpawn tile=%d self=%p call=%" PRIu64,
            tile_index,
            thiz,
            call_count);
    }
    if (Originals::CheckForPickupsAwaitingSpawn != nullptr) {
        Originals::CheckForPickupsAwaitingSpawn(thiz, tile_index);
    }
}

void HookedEvaluateBoardPickups(void* thiz, bool generate_new_pickups, bool use_quick_spawn) {
    const uint64_t call_count = g_evaluate_pickups_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    LOGD(
        "BoardPickupsService.EvaluateBoardPickups gen=%d quick=%d self=%p call=%" PRIu64,
        generate_new_pickups ? 1 : 0,
        use_quick_spawn ? 1 : 0,
        thiz,
        call_count);
    if (Originals::EvaluateBoardPickups != nullptr) {
        Originals::EvaluateBoardPickups(thiz, generate_new_pickups, use_quick_spawn);
    }
}

void HookedSetNextMoveTokenAnimatorParams(void* thiz, int start_index, void* move_params) {
    const uint64_t call_count = g_set_next_move_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call_count <= 40 || (call_count % 120) == 0) {
        LOGD(
            "BoardPickupsService.SetNextMoveTokenAnimatorParams start=%d params=%p self=%p call=%" PRIu64,
            start_index,
            move_params,
            thiz,
            call_count);
    }
    if (Originals::SetNextMoveTokenAnimatorParams != nullptr) {
        Originals::SetNextMoveTokenAnimatorParams(thiz, start_index, move_params);
    }
}

void* HookedTriggerOnLandHandler(void* thiz, int tile_index) {
    const uint64_t call_count = g_trigger_on_land_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    LOGI(
        "BoardPickupsService.TriggerOnLandHandler tile=%d self=%p call=%" PRIu64,
        tile_index,
        thiz,
        call_count);
    {
        char ipc_msg[64];
        snprintf(ipc_msg, sizeof(ipc_msg), "pickup:land:tile=%d", tile_index);
        IPCFeed::Publish(ipc_msg);
    }
    if (Originals::TriggerOnLandHandler == nullptr) {
        return nullptr;
    }
    return Originals::TriggerOnLandHandler(thiz, tile_index);
}

void HookedRemoveBoardPickup(void* thiz, int tile_index) {
    const uint64_t call_count = g_remove_pickup_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    LOGD("BoardPickupsService.RemoveBoardPickup tile=%d self=%p call=%" PRIu64, tile_index, thiz, call_count);
    if (Originals::RemoveBoardPickup != nullptr) {
        Originals::RemoveBoardPickup(thiz, tile_index);
    }
}

void* HookedAnimateCollectNearestPickups(void* thiz, void* tiles, int reward_type, int animation_trigger, int effect_type) {
    const uint64_t call_count = g_collect_nearest_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    LOGD(
        "BoardPickupsService.AnimateCollectNearestPickups tiles=%p rewardType=%d animation=%d effect=%d self=%p call=%" PRIu64,
        tiles,
        reward_type,
        animation_trigger,
        effect_type,
        thiz,
        call_count);
    if (Originals::AnimateCollectNearestPickups == nullptr) {
        return nullptr;
    }
    return Originals::AnimateCollectNearestPickups(thiz, tiles, reward_type, animation_trigger, effect_type);
}

void* HookedSpecialCurrencyOnLandHandler(void* thiz, void* pickup) {
    const uint64_t call_count = g_special_currency_on_land_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const int type = ReadPickupType(pickup);
    const int tile = ReadPickupTileIndex(pickup);
    LOGI(
        "SpecialCurrencyEventPickupSource.OnLandHandler pickup=%p type=%d(%s) tile=%d self=%p call=%" PRIu64,
        pickup,
        type,
        PickupTypeName(type),
        tile,
        thiz,
        call_count);
    if (Originals::SpecialCurrencyOnLandHandler == nullptr) {
        return nullptr;
    }
    return Originals::SpecialCurrencyOnLandHandler(thiz, pickup);
}

void* HookedSpecialCurrencyOnCollectNearestHandler(void* thiz, void* pickups, int effect_type) {
    const uint64_t call_count =
        g_special_currency_collect_nearest_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    LOGD(
        "SpecialCurrencyEventPickupSource.OnCollectNearestHandler pickups=%p effect=%d self=%p call=%" PRIu64,
        pickups,
        effect_type,
        thiz,
        call_count);
    if (Originals::SpecialCurrencyOnCollectNearestHandler == nullptr) {
        return nullptr;
    }
    return Originals::SpecialCurrencyOnCollectNearestHandler(thiz, pickups, effect_type);
}

void* HookedCoreMechanicGetPickupsToSpawn(void* thiz, void* current_pickups) {
    const uint64_t call_count = g_core_mechanic_spawn_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call_count <= 20 || (call_count % 60) == 0) {
        LOGD(
            "CoreMechanicPickupSource.GetPickupsToSpawn current=%p self=%p call=%" PRIu64,
            current_pickups,
            thiz,
            call_count);
    }
    if (Originals::CoreMechanicGetPickupsToSpawn == nullptr) {
        return nullptr;
    }
    return Originals::CoreMechanicGetPickupsToSpawn(thiz, current_pickups);
}

void* HookedShieldGetPickupsToSpawn(void* thiz, void* current_pickups) {
    const uint64_t call_count = g_shield_spawn_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call_count <= 20 || (call_count % 60) == 0) {
        LOGD(
            "ShieldPickupSource.GetPickupsToSpawn current=%p self=%p call=%" PRIu64,
            current_pickups,
            thiz,
            call_count);
    }
    if (Originals::ShieldGetPickupsToSpawn == nullptr) {
        return nullptr;
    }
    return Originals::ShieldGetPickupsToSpawn(thiz, current_pickups);
}

void* HookedRentDueGetPickupsToSpawn(void* thiz, void* current_pickups) {
    const uint64_t call_count = g_rent_due_spawn_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call_count <= 20 || (call_count % 60) == 0) {
        LOGD(
            "RentDuePickupSource.GetPickupsToSpawn current=%p self=%p call=%" PRIu64,
            current_pickups,
            thiz,
            call_count);
    }
    if (Originals::RentDueGetPickupsToSpawn == nullptr) {
        return nullptr;
    }
    return Originals::RentDueGetPickupsToSpawn(thiz, current_pickups);
}

void* HookedBonusBoardGetPickupsToSpawn(void* thiz, void* current_pickups) {
    const uint64_t call_count = g_bonus_board_spawn_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (call_count <= 20 || (call_count % 60) == 0) {
        LOGD(
            "BonusBoardPickupSource.GetPickupsToSpawn current=%p self=%p call=%" PRIu64,
            current_pickups,
            thiz,
            call_count);
    }
    if (Originals::BonusBoardGetPickupsToSpawn == nullptr) {
        return nullptr;
    }
    return Originals::BonusBoardGetPickupsToSpawn(thiz, current_pickups);
}

void* HookedPickupHandlerProcessPickups(void* movement_context, void* execution_context, int ledger_entry_type) {
    const uint64_t call_count = g_process_pickups_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    LOGD(
        "PickupHandler.ProcessPickups movement=%p context=%p ledger=%d call=%" PRIu64,
        movement_context,
        execution_context,
        ledger_entry_type,
        call_count);
    if (Originals::ProcessPickups == nullptr) {
        return nullptr;
    }
    return Originals::ProcessPickups(movement_context, execution_context, ledger_entry_type);
}

void* HookedPickupHandlerProcessClosestPickups(
    void* movement_context,
    void* execution_context,
    void* arguments,
    int ledger_entry_type) {
    const uint64_t call_count = g_process_closest_pickups_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    LOGD(
        "PickupHandler.ProcessClosestPickups movement=%p context=%p args=%p ledger=%d call=%" PRIu64,
        movement_context,
        execution_context,
        arguments,
        ledger_entry_type,
        call_count);
    if (Originals::ProcessClosestPickups == nullptr) {
        return nullptr;
    }
    return Originals::ProcessClosestPickups(movement_context, execution_context, arguments, ledger_entry_type);
}

void* HookedPickupHandlerHandlePickup(
    void* pickup,
    void* pickup_state,
    void* configs,
    void* tile,
    int multiplier,
    void* context,
    void* objective_handler,
    int ledger_entry_type) {
    const uint64_t call_count = g_handle_pickup_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const int type = ReadPickupType(pickup);
    const int tile_index = ReadPickupTileIndex(pickup);
    if (call_count <= 60 || (call_count % 200) == 0) {
        LOGD(
            "PickupHandler.HandlePickup pickup=%p type=%d(%s) tile=%d mult=%d ledger=%d call=%" PRIu64,
            pickup,
            type,
            PickupTypeName(type),
            tile_index,
            multiplier,
            ledger_entry_type,
            call_count);
    }
    if (Originals::HandlePickup == nullptr) {
        return nullptr;
    }
    return Originals::HandlePickup(
        pickup,
        pickup_state,
        configs,
        tile,
        multiplier,
        context,
        objective_handler,
        ledger_entry_type);
}

void* HookedPickupHandlerAwardPickupContents(void* pickup, int multiplier, void* context, int ledger_entry_type) {
    const uint64_t call_count = g_award_pickup_contents_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const int type = ReadPickupType(pickup);
    const int tile_index = ReadPickupTileIndex(pickup);
    if (call_count <= 60 || (call_count % 200) == 0) {
        LOGD(
            "PickupHandler.AwardPickupContents pickup=%p type=%d(%s) tile=%d mult=%d ledger=%d call=%" PRIu64,
            pickup,
            type,
            PickupTypeName(type),
            tile_index,
            multiplier,
            ledger_entry_type,
            call_count);
    }
    {
        char ipc_msg[64];
        snprintf(ipc_msg, sizeof(ipc_msg), "pickup:award:type=%s,tile=%d,mult=%d",
            PickupTypeName(type), tile_index, multiplier);
        IPCFeed::Publish(ipc_msg);
    }
    if (Originals::AwardPickupContents == nullptr) {
        return nullptr;
    }
    return Originals::AwardPickupContents(pickup, multiplier, context, ledger_entry_type);
}

}  // namespace

bool Hooks::Pickups::Install() {
    bool required_ok = true;
    bool optional_ok = true;

    required_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.BoardPickups",
                      "BoardPickupsService",
                      "EvaluateBoardPickups",
                      2,
                      reinterpret_cast<void*>(&HookedEvaluateBoardPickups),
                      reinterpret_cast<void**>(&Originals::EvaluateBoardPickups),
                      "BoardPickupsService.EvaluateBoardPickups") &&
                  required_ok;

    required_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.BoardPickups",
                      "BoardPickupsService",
                      "CheckForPickupsAwaitingSpawn",
                      1,
                      reinterpret_cast<void*>(&HookedCheckForPickupsAwaitingSpawn),
                      reinterpret_cast<void**>(&Originals::CheckForPickupsAwaitingSpawn),
                      "BoardPickupsService.CheckForPickupsAwaitingSpawn") &&
                  required_ok;

    required_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.BoardPickups",
                      "BoardPickupsService",
                      "SetNextMoveTokenAnimatorParams",
                      2,
                      reinterpret_cast<void*>(&HookedSetNextMoveTokenAnimatorParams),
                      reinterpret_cast<void**>(&Originals::SetNextMoveTokenAnimatorParams),
                      "BoardPickupsService.SetNextMoveTokenAnimatorParams") &&
                  required_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.BoardPickups",
                      "BoardPickupsService",
                      "TriggerOnLandHandler",
                      1,
                      reinterpret_cast<void*>(&HookedTriggerOnLandHandler),
                      reinterpret_cast<void**>(&Originals::TriggerOnLandHandler),
                      "BoardPickupsService.TriggerOnLandHandler") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.BoardPickups",
                      "BoardPickupsService",
                      "RemoveBoardPickup",
                      1,
                      reinterpret_cast<void*>(&HookedRemoveBoardPickup),
                      reinterpret_cast<void**>(&Originals::RemoveBoardPickup),
                      "BoardPickupsService.RemoveBoardPickup") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.BoardPickups",
                      "BoardPickupsService",
                      "AnimateCollectNearestPickups",
                      4,
                      reinterpret_cast<void*>(&HookedAnimateCollectNearestPickups),
                      reinterpret_cast<void**>(&Originals::AnimateCollectNearestPickups),
                      "BoardPickupsService.AnimateCollectNearestPickups") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.BoardPickups.PickupSources",
                      "SpecialCurrencyEventPickupSource",
                      "OnLandHandler",
                      1,
                      reinterpret_cast<void*>(&HookedSpecialCurrencyOnLandHandler),
                      reinterpret_cast<void**>(&Originals::SpecialCurrencyOnLandHandler),
                      "SpecialCurrencyEventPickupSource.OnLandHandler") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.BoardPickups.PickupSources",
                      "SpecialCurrencyEventPickupSource",
                      "OnCollectNearestHandler",
                      2,
                      reinterpret_cast<void*>(&HookedSpecialCurrencyOnCollectNearestHandler),
                      reinterpret_cast<void**>(&Originals::SpecialCurrencyOnCollectNearestHandler),
                      "SpecialCurrencyEventPickupSource.OnCollectNearestHandler") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.BoardPickups.PickupSources",
                      "CoreMechanicPickupSource",
                      "GetPickupsToSpawn",
                      1,
                      reinterpret_cast<void*>(&HookedCoreMechanicGetPickupsToSpawn),
                      reinterpret_cast<void**>(&Originals::CoreMechanicGetPickupsToSpawn),
                      "CoreMechanicPickupSource.GetPickupsToSpawn") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.BoardPickups.PickupSources",
                      "ShieldPickupSource",
                      "GetPickupsToSpawn",
                      1,
                      reinterpret_cast<void*>(&HookedShieldGetPickupsToSpawn),
                      reinterpret_cast<void**>(&Originals::ShieldGetPickupsToSpawn),
                      "ShieldPickupSource.GetPickupsToSpawn") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.BoardPickups.PickupSources",
                      "RentDuePickupSource",
                      "GetPickupsToSpawn",
                      1,
                      reinterpret_cast<void*>(&HookedRentDueGetPickupsToSpawn),
                      reinterpret_cast<void**>(&Originals::RentDueGetPickupsToSpawn),
                      "RentDuePickupSource.GetPickupsToSpawn") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Client.dll",
                      "Tophat.Client.BoardPickups.PickupSources",
                      "BonusBoardPickupSource",
                      "GetPickupsToSpawn",
                      1,
                      reinterpret_cast<void*>(&HookedBonusBoardGetPickupsToSpawn),
                      reinterpret_cast<void**>(&Originals::BonusBoardGetPickupsToSpawn),
                      "BonusBoardPickupSource.GetPickupsToSpawn") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Common.dll",
                      "Tophat.Common.Rolling",
                      "PickupHandler",
                      "ProcessPickups",
                      3,
                      reinterpret_cast<void*>(&HookedPickupHandlerProcessPickups),
                      reinterpret_cast<void**>(&Originals::ProcessPickups),
                      "PickupHandler.ProcessPickups") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Common.dll",
                      "Tophat.Common.Rolling",
                      "PickupHandler",
                      "ProcessClosestPickups",
                      4,
                      reinterpret_cast<void*>(&HookedPickupHandlerProcessClosestPickups),
                      reinterpret_cast<void**>(&Originals::ProcessClosestPickups),
                      "PickupHandler.ProcessClosestPickups") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Common.dll",
                      "Tophat.Common.Rolling",
                      "PickupHandler",
                      "HandlePickup",
                      8,
                      reinterpret_cast<void*>(&HookedPickupHandlerHandlePickup),
                      reinterpret_cast<void**>(&Originals::HandlePickup),
                      "PickupHandler.HandlePickup") &&
                  optional_ok;

    optional_ok = Hooks::Internal::ResolveAndHook(
                      "Tophat.Common.dll",
                      "Tophat.Common.Rolling",
                      "PickupHandler",
                      "AwardPickupContents",
                      4,
                      reinterpret_cast<void*>(&HookedPickupHandlerAwardPickupContents),
                      reinterpret_cast<void**>(&Originals::AwardPickupContents),
                      "PickupHandler.AwardPickupContents") &&
                  optional_ok;

    if (!required_ok) {
        LOGE("Pickup hook installation failed (required hooks)");
        return false;
    }

    if (!optional_ok) {
        LOGW("Pickup hook installation partial (optional hooks missing)");
    }

    LOGI("Pickup hooks installed");
    return true;
}
