#include "ipc_feed.h"

#include <android/log.h>
#include <fcntl.h>
#include <cstddef>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <mutex>

#define LOG_TAG "IPCFeed"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace {

constexpr const char* kSocketEnv = "BABIX_IPC_SOCKET";
constexpr const char* kDefaultSocketName = "babix_native_hooks";
constexpr size_t kMaxPayload = 512;

std::atomic<bool> g_initialized{false};
std::mutex g_socket_mutex;
int g_socket_fd = -1;

void EnsureSocketConnectedLocked() {
    if (g_socket_fd >= 0) {
        return;
    }

    g_socket_fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (g_socket_fd < 0) {
        LOGW("socket(AF_UNIX) failed");
        return;
    }

    sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;

    const char* socket_name = getenv(kSocketEnv);
    if (socket_name == nullptr || socket_name[0] == '\0') {
        socket_name = kDefaultSocketName;
    }

    const bool is_abstract = socket_name[0] != '/';
    const size_t name_len = strlen(socket_name);
    if (name_len + (is_abstract ? 1 : 0) >= sizeof(addr.sun_path)) {
        LOGW("socket path too long; disabling IPC feed");
        close(g_socket_fd);
        g_socket_fd = -1;
        return;
    }

    socklen_t addr_len = 0;
    if (is_abstract) {
        addr.sun_path[0] = '\0';
        memcpy(addr.sun_path + 1, socket_name, name_len);
        addr_len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + 1 + name_len);
    } else {
        memcpy(addr.sun_path, socket_name, name_len + 1);
        addr_len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + name_len + 1);
    }

    if (connect(g_socket_fd, reinterpret_cast<sockaddr*>(&addr), addr_len) != 0) {
        LOGW("connect(%s) failed; feed will stay best-effort", is_abstract ? "@babix_native_hooks" : socket_name);
        close(g_socket_fd);
        g_socket_fd = -1;
    }
}

}  // namespace

void IPCFeed::Initialize() {
    if (g_initialized.exchange(true)) {
        return;
    }
    std::scoped_lock lock(g_socket_mutex);
    EnsureSocketConnectedLocked();
    LOGD("IPC feed initialized");
}

void IPCFeed::Publish(const std::string& message) {
    if (!g_initialized.load()) {
        Initialize();
    }

    if (message.empty()) {
        return;
    }

    std::scoped_lock lock(g_socket_mutex);
    EnsureSocketConnectedLocked();
    if (g_socket_fd < 0) {
        return;
    }

    const size_t send_len = message.size() > kMaxPayload ? kMaxPayload : message.size();
    const ssize_t rc = send(g_socket_fd, message.data(), send_len, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (rc < 0) {
        close(g_socket_fd);
        g_socket_fd = -1;
    }
}
