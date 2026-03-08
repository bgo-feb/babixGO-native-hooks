#include <jni.h>

#include <android/log.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "hook_manager.h"
#include "ipc_feed.h"

#define LOG_TAG "BabixPayload"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

void* BootstrapThreadMain(void*) {
    prctl(PR_SET_NAME, "babix-bootstrap", 0, 0, 0);

    if (!HookManager::InitializeBNM()) {
        LOGE("BNM bootstrap failed");
    }

    return nullptr;
}

}  // namespace

__attribute__((constructor))
static void OnLibraryLoad() {
    LOGI("=== Babix Payload Loaded ===");
    LOGI("PID: %d", getpid());
    IPCFeed::Initialize();
    IPCFeed::Publish("payload_loaded");

    pthread_t thread = {};
    const int rc = pthread_create(&thread, nullptr, &BootstrapThreadMain, nullptr);
    if (rc != 0) {
        LOGE("Failed to create bootstrap thread: %d", rc);
        return;
    }

    pthread_detach(thread);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)vm;
    (void)reserved;
    LOGD("JNI_OnLoad called");
    return JNI_VERSION_1_6;
}

