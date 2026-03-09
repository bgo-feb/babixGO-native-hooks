#include "ipc_feed.h"

#include <android/log.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <mutex>
#include <string>

#define LOG_TAG "IPCFeed"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace {

// TCP-Port, auf dem der Empfaenger (Python/APK) lauscht.
// Ueberschreibbar per Umgebungsvariable BABIX_IPC_PORT.
// Ueber `adb reverse tcp:27182 tcp:27182` wird der Port auf den Host-PC weitergeleitet.
constexpr uint16_t kDefaultPort = 27182;
constexpr const char* kPortEnv  = "BABIX_IPC_PORT";
constexpr size_t kMaxPayload    = 512;

std::atomic<bool> g_initialized{false};
std::mutex g_socket_mutex;
int g_socket_fd = -1;

void EnsureConnectedLocked() {
    if (g_socket_fd >= 0) {
        return;
    }

    uint16_t port = kDefaultPort;
    const char* port_env = getenv(kPortEnv);
    if (port_env != nullptr && port_env[0] != '\0') {
        const long v = strtol(port_env, nullptr, 10);
        if (v > 0 && v <= 65535) {
            port = static_cast<uint16_t>(v);
        }
    }

    g_socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (g_socket_fd < 0) {
        LOGW("socket(AF_INET) failed");
        return;
    }

    sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // 127.0.0.1

    if (connect(g_socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        LOGW("connect(127.0.0.1:%u) failed; IPC feed inactive", port);
        close(g_socket_fd);
        g_socket_fd = -1;
    } else {
        LOGD("IPC feed connected to 127.0.0.1:%u", port);
    }
}

}  // namespace

void IPCFeed::Initialize() {
    if (g_initialized.exchange(true)) {
        return;
    }
    std::scoped_lock lock(g_socket_mutex);
    EnsureConnectedLocked();
}

void IPCFeed::Publish(const std::string& message) {
    if (!g_initialized.load()) {
        Initialize();
    }

    if (message.empty()) {
        return;
    }

    // Newline-Terminierung fuer einfaches readline() auf der Empfaengerseite
    const std::string line = (message.size() < kMaxPayload)
        ? message + '\n'
        : message.substr(0, kMaxPayload) + '\n';

    std::scoped_lock lock(g_socket_mutex);
    EnsureConnectedLocked();
    if (g_socket_fd < 0) {
        return;
    }

    const ssize_t rc = send(g_socket_fd, line.data(), line.size(), MSG_DONTWAIT | MSG_NOSIGNAL);
    if (rc < 0) {
        LOGW("send failed; socket closed for reconnect on next Publish");
        close(g_socket_fd);
        g_socket_fd = -1;
    }
}
