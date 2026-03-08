#pragma once

namespace Hooks::Internal {

bool ResolveAndHook(
    const char* image_name,
    const char* namespace_name,
    const char* class_name,
    const char* method_name,
    int parameter_count,
    void* detour,
    void** original,
    const char* label);

}

