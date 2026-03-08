#if __cplusplus < 202002L
static_assert(false, "ByNameModding requires C++20 and above!");
#endif

#pragma once

#define UNITY_VER 222
#define UNITY_PATCH_VER 32

// Keep the MWE lean. We only need lookup + hooking, not class synthesis.
// #define BNM_CLASSES_MANAGEMENT
// #define BNM_COROUTINE

#define BNM_USE_IL2CPP_ALLOCATOR

#ifndef NDEBUG
#define BNM_ALLOW_STR_METHODS
#define BNM_ALLOW_SAFE_IS_ALLOCATED
#define BNM_ALLOW_SELF_CHECKS
#define BNM_CHECK_INSTANCE_TYPE
#define BNM_DEBUG
#define BNM_INFO
#define BNM_ERROR
#define BNM_WARNING
#endif

#define BNM_OBFUSCATE(str) str
#define BNM_OBFUSCATE_TMP(str) str

#include <dobby.h>

template<typename PTR_T, typename NEW_T, typename T_OLD>
inline void* BasicHook(PTR_T ptr, NEW_T newMethod, T_OLD& oldBytes) {
    if ((void*)ptr != nullptr) {
        DobbyHook((void*)ptr, (void*)newMethod, (void**)&oldBytes);
    }
    return (void*)ptr;
}

template<typename PTR_T, typename NEW_T, typename T_OLD>
inline void* BasicHook(PTR_T ptr, NEW_T newMethod, T_OLD&& oldBytes) {
    if ((void*)ptr != nullptr) {
        DobbyHook((void*)ptr, (void*)newMethod, (void**)&oldBytes);
    }
    return (void*)ptr;
}

template<typename PTR_T>
inline void Unhook(PTR_T ptr) {
    if ((void*)ptr != nullptr) {
        DobbyDestroy((void*)ptr);
    }
}

#include <dlfcn.h>

#define BNM_dlopen dlopen
#define BNM_dlsym dlsym
#define BNM_dlclose dlclose
#define BNM_dladdr dladdr

#include <cstdlib>

#define BNM_malloc malloc
#define BNM_free free

#include <android/log.h>

#define BNM_TAG "BabixBNM"

#ifdef BNM_ALLOW_SELF_CHECKS
#define BNM_CHECK_SELF(returnValue) if (!SelfCheck()) return returnValue
#else
#define BNM_CHECK_SELF(returnValue) ((void)0)
#endif

#ifdef BNM_INFO
#define BNM_LOG_INFO(...) ((void)__android_log_print(ANDROID_LOG_INFO, BNM_TAG, __VA_ARGS__))
#else
#define BNM_LOG_INFO(...) ((void)0)
#endif

#ifdef BNM_DEBUG
#define BNM_LOG_DEBUG(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, BNM_TAG, __VA_ARGS__))
#define BNM_LOG_DEBUG_IF(condition, ...) if (condition) ((void)__android_log_print(ANDROID_LOG_DEBUG, BNM_TAG, __VA_ARGS__))
#else
#define BNM_LOG_DEBUG(...) ((void)0)
#define BNM_LOG_DEBUG_IF(...) ((void)0)
#endif

#ifdef BNM_ERROR
#define BNM_LOG_ERR(...) ((void)__android_log_print(ANDROID_LOG_ERROR, BNM_TAG, __VA_ARGS__))
#define BNM_LOG_ERR_IF(condition, ...) if (condition) ((void)__android_log_print(ANDROID_LOG_ERROR, BNM_TAG, __VA_ARGS__))
#else
#define BNM_LOG_ERR(...) ((void)0)
#define BNM_LOG_ERR_IF(condition, ...) ((void)0)
#endif

#ifdef BNM_WARNING
#define BNM_LOG_WARN(...) ((void)__android_log_print(ANDROID_LOG_WARN, BNM_TAG, __VA_ARGS__))
#define BNM_LOG_WARN_IF(condition, ...) if (condition) ((void)__android_log_print(ANDROID_LOG_WARN, BNM_TAG, __VA_ARGS__))
#else
#define BNM_LOG_WARN(...) ((void)0)
#define BNM_LOG_WARN_IF(condition, ...) ((void)0)
#endif

namespace BNM {
#if defined(__LP64__)
typedef long BNM_INT_PTR;
typedef unsigned long BNM_PTR;
#else
typedef int BNM_INT_PTR;
typedef unsigned int BNM_PTR;
#endif
}

#define BNM_VER "2.5.2-babix"

