LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := dobby
LOCAL_CPPFLAGS += -std=c++17 -DDOBBY_LOGGING_DISABLE -DBUILD_WITH_TRAMPOLINE_ASSEMBLER -D__DOBBY_BUILD_VERSION__=\"BabixDobby\"
LOCAL_CFLAGS += -DDOBBY_LOGGING_DISABLE -DBUILD_WITH_TRAMPOLINE_ASSEMBLER -D__DOBBY_BUILD_VERSION__=\"BabixDobby\"
LOCAL_SRC_FILES := \
    generated/Dobby/source/core/assembler/assembler-arm.cc \
    generated/Dobby/source/core/assembler/assembler-ia32.cc \
    generated/Dobby/source/core/assembler/assembler-x64.cc \
    generated/Dobby/source/core/codegen/codegen-arm.cc \
    generated/Dobby/source/core/codegen/codegen-ia32.cc \
    generated/Dobby/source/InstructionRelocation/arm/InstructionRelocationARM.cc \
    generated/Dobby/source/InstructionRelocation/arm64/InstructionRelocationARM64.cc \
    generated/Dobby/source/InstructionRelocation/x86/InstructionRelocationX86.cc \
    generated/Dobby/source/InstructionRelocation/x86/InstructionRelocationX86Shared.cc \
    generated/Dobby/source/InstructionRelocation/x64/InstructionRelocationX64.cc \
    generated/Dobby/source/InstructionRelocation/x86/x86_insn_decode/x86_insn_decode.c \
    generated/Dobby/source/InterceptRouting/InstrumentRouting/instrument_routing_handler.cpp \
    generated/Dobby/source/InterceptRouting/NearBranchTrampoline/near_trampoline_arm64.cc \
    generated/Dobby/source/TrampolineBridge/Trampoline/trampoline_arm.cc \
    generated/Dobby/source/TrampolineBridge/Trampoline/trampoline_arm64.cc \
    generated/Dobby/source/TrampolineBridge/Trampoline/trampoline_x86.cc \
    generated/Dobby/source/TrampolineBridge/Trampoline/trampoline_x64.cc \
    generated/Dobby/source/TrampolineBridge/ClosureTrampolineBridge/arm/helper_arm.cc \
    generated/Dobby/source/TrampolineBridge/ClosureTrampolineBridge/arm/closure_bridge_arm.cc \
    generated/Dobby/source/TrampolineBridge/ClosureTrampolineBridge/arm/ClosureTrampolineARM.cc \
    generated/Dobby/source/TrampolineBridge/ClosureTrampolineBridge/arm64/helper_arm64.cc \
    generated/Dobby/source/TrampolineBridge/ClosureTrampolineBridge/arm64/closure_bridge_arm64.cc \
    generated/Dobby/source/TrampolineBridge/ClosureTrampolineBridge/arm64/ClosureTrampolineARM64.cc \
    generated/Dobby/source/TrampolineBridge/ClosureTrampolineBridge/x86/helper_x86.cc \
    generated/Dobby/source/TrampolineBridge/ClosureTrampolineBridge/x86/closure_bridge_x86.cc \
    generated/Dobby/source/TrampolineBridge/ClosureTrampolineBridge/x86/ClosureTrampolineX86.cc \
    generated/Dobby/source/TrampolineBridge/ClosureTrampolineBridge/x64/helper_x64.cc \
    generated/Dobby/source/TrampolineBridge/ClosureTrampolineBridge/x64/closure_bridge_x64.cc \
    generated/Dobby/source/TrampolineBridge/ClosureTrampolineBridge/x64/ClosureTrampolineX64.cc \
    generated/Dobby/source/Backend/UserMode/PlatformUtil/Linux/ProcessRuntime.cc \
    generated/Dobby/source/Backend/UserMode/UnifiedInterface/platform-posix.cc \
    generated/Dobby/source/Backend/UserMode/ExecMemory/code-patch-tool-posix.cc \
    generated/Dobby/source/Backend/UserMode/ExecMemory/clear-cache-tool-all.c \
    generated/Dobby/builtin-plugin/SymbolResolver/elf/dobby_symbol_resolver.cc \
    generated/Dobby/external/logging/logging.cc \
    generated/Dobby/source/dobby.cpp
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/generated/Dobby \
    $(LOCAL_PATH)/generated/Dobby/include \
    $(LOCAL_PATH)/generated/Dobby/source \
    $(LOCAL_PATH)/generated/Dobby/source/dobby \
    $(LOCAL_PATH)/generated/Dobby/source/Backend/UserMode \
    $(LOCAL_PATH)/generated/Dobby/external \
    $(LOCAL_PATH)/generated/Dobby/external/logging \
    $(LOCAL_PATH)/generated/Dobby/builtin-plugin
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/generated/Dobby/include
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := bnm
LOCAL_CPPFLAGS += -std=c++20
LOCAL_CPP_FEATURES := exceptions rtti
LOCAL_SRC_FILES := \
    external/BNM/src/Class.cpp \
    external/BNM/src/ClassesManagement.cpp \
    external/BNM/src/Coroutine.cpp \
    external/BNM/src/Delegates.cpp \
    external/BNM/src/Defaults.cpp \
    external/BNM/src/EventBase.cpp \
    external/BNM/src/Exceptions.cpp \
    external/BNM/src/FieldBase.cpp \
    external/BNM/src/Hooks.cpp \
    external/BNM/src/Image.cpp \
    external/BNM/src/Internals.cpp \
    external/BNM/src/Loading.cpp \
    external/BNM/src/MethodBase.cpp \
    external/BNM/src/MonoStructures.cpp \
    external/BNM/src/PropertyBase.cpp \
    external/BNM/src/UnityStructures.cpp \
    external/BNM/src/Utils.cpp
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/generated/BNM/include \
    $(LOCAL_PATH)/external/BNM/external/include \
    $(LOCAL_PATH)/external/BNM/external \
    $(LOCAL_PATH)/external/BNM/external/utf8 \
    $(LOCAL_PATH)/external/BNM/src/private \
    $(LOCAL_PATH)/external/Dobby/include
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := babix_payload
LOCAL_CPPFLAGS += -std=c++20
LOCAL_CPP_FEATURES := exceptions rtti
LOCAL_SRC_FILES := \
    main.cpp \
    hook_manager.cpp \
    ipc_feed.cpp \
    pattern_scanner.cpp \
    hooks/hook_utils.cpp \
    hooks/roll_hook.cpp \
    hooks/jail_hook.cpp \
    hooks/coinflip_hook.cpp \
    hooks/pickups_hook.cpp \
    hooks/chance_hook.cpp \
    hooks/speed_hook.cpp
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/generated/BNM/include \
    $(LOCAL_PATH)/external/BNM/external/include \
    $(LOCAL_PATH)/external/BNM/external \
    $(LOCAL_PATH)/external/BNM/external/utf8 \
    $(LOCAL_PATH)/external/BNM/src/private \
    $(LOCAL_PATH)/external/Dobby/include
LOCAL_STATIC_LIBRARIES := bnm dobby
LOCAL_LDLIBS := -llog -ldl
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := babix_zygisk
LOCAL_CPPFLAGS += -std=c++20
LOCAL_SRC_FILES := zygisk_loader.cpp
LOCAL_LDLIBS := -llog -ldl
include $(BUILD_SHARED_LIBRARY)
