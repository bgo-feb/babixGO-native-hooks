#pragma once

class HookManager {
public:
    static bool InitializeBNM();
    static bool InstallHooks();
    static bool SafeHook(void* target, void* hook, void** original, const char* name);

private:
    static void OnBNMLoaded();
    static void StartInstallThread();
    static void* InstallThreadMain(void*);
    static bool InstallUnityDebugLogHook();
};

