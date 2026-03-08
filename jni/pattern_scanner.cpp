#include "pattern_scanner.h"

#include <android/log.h>
#include <link.h>
#include <cstring>

#define LOG_TAG "PatternScanner"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace {

struct ScanRequest {
    const char* module_name;
    const uint8_t* pattern;
    const char* mask;
    size_t pattern_size;
    void* result;
};

bool MatchPattern(const uint8_t* memory, const uint8_t* pattern, const char* mask, size_t pattern_size) {
    for (size_t i = 0; i < pattern_size; ++i) {
        if (mask[i] == 'x' && memory[i] != pattern[i]) {
            return false;
        }
    }
    return true;
}

int IteratePhdr(dl_phdr_info* info, size_t, void* data) {
    auto* req = reinterpret_cast<ScanRequest*>(data);
    if (info->dlpi_name == nullptr || strstr(info->dlpi_name, req->module_name) == nullptr) {
        return 0;
    }

    for (ElfW(Half) i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr)& phdr = info->dlpi_phdr[i];
        if (phdr.p_type != PT_LOAD || (phdr.p_flags & PF_X) == 0 || phdr.p_memsz < req->pattern_size) {
            continue;
        }

        const auto* start = reinterpret_cast<const uint8_t*>(info->dlpi_addr + phdr.p_vaddr);
        const size_t limit = phdr.p_memsz - req->pattern_size;
        for (size_t offset = 0; offset <= limit; ++offset) {
            if (MatchPattern(start + offset, req->pattern, req->mask, req->pattern_size)) {
                req->result = const_cast<uint8_t*>(start + offset);
                LOGD("Pattern match in %s at %p", info->dlpi_name, req->result);
                return 1;
            }
        }
    }

    return 0;
}

}  // namespace

void* PatternScanner::FindFirstInModule(const char* module_name, const uint8_t* pattern, const char* mask, size_t pattern_size) {
    if (module_name == nullptr || pattern == nullptr || mask == nullptr || pattern_size == 0) {
        return nullptr;
    }

    ScanRequest req = {
        .module_name = module_name,
        .pattern = pattern,
        .mask = mask,
        .pattern_size = pattern_size,
        .result = nullptr,
    };

    dl_iterate_phdr(&IteratePhdr, &req);
    return req.result;
}
