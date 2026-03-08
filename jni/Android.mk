LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := dobby
LOCAL_SRC_FILES := external/Dobby/prebuilt/$(TARGET_ARCH_ABI)/libdobby.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/external/Dobby/include
include $(PREBUILT_STATIC_LIBRARY)

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
    hook_manager.cpp
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

