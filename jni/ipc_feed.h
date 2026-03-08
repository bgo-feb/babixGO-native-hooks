#pragma once

#include <string>

namespace IPCFeed {

void Initialize();
void Publish(const std::string& message);

}  // namespace IPCFeed
