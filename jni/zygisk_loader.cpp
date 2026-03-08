#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>

#include <cstring>

#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

#define LOG_TAG "BabixZygisk"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

constexpr const char* kTargetPackage = "com.scopely.monopolygo";
constexpr const char* kPayloadPath = "/system/lib64/libbabix_payload.so";

bool IsTargetProcess(const char* process_name) {
    if (process_name == nullptr) {
        return false;
    }

    const size_t package_len = strlen(kTargetPackage);
    if (strncmp(process_name, kTargetPackage, package_len) != 0) {
        return false;
    }

    const char suffix = process_name[package_len];
    return suffix == '\0' || suffix == ':';
}

}  // namespace

class BabixZygiskModule : public zygisk::ModuleBase {
public:
    void onLoad(Api* api, JNIEnv* env) override {
        api_ = api;
        env_ = env;
    }

    void preAppSpecialize(AppSpecializeArgs* args) override {
        const char* process_name = env_->GetStringUTFChars(args->nice_name, nullptr);
        should_inject_ = IsTargetProcess(process_name);

        LOGD("preAppSpecialize process=%s target=%d", process_name != nullptr ? process_name : "<null>", should_inject_ ? 1 : 0);

        if (!should_inject_) {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }

        if (process_name != nullptr) {
            env_->ReleaseStringUTFChars(args->nice_name, process_name);
        }
    }

    void postAppSpecialize(const AppSpecializeArgs* args) override {
        (void)args;

        if (!should_inject_) {
            return;
        }

        if (access(kPayloadPath, R_OK) != 0) {
            LOGE("payload not accessible: %s", kPayloadPath);
            return;
        }

        dlerror();
        void* handle = dlopen(kPayloadPath, RTLD_NOW);
        if (handle == nullptr) {
            const char* err = dlerror();
            LOGE("dlopen failed for %s: %s", kPayloadPath, err != nullptr ? err : "<unknown>");
            return;
        }

        LOGI("payload loaded via zygisk: %s (handle=%p)", kPayloadPath, handle);

        // Loader can be unloaded after injection to reduce footprint.
        api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api* api_ = nullptr;
    JNIEnv* env_ = nullptr;
    bool should_inject_ = false;
};

REGISTER_ZYGISK_MODULE(BabixZygiskModule)