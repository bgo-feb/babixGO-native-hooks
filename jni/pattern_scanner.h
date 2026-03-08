#pragma once

#include <cstddef>
#include <cstdint>

namespace PatternScanner {

void* FindFirstInModule(const char* module_name, const uint8_t* pattern, const char* mask, size_t pattern_size);

}  // namespace PatternScanner
