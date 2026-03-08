#pragma once

class HookManager {
public:
    static bool InitializeBNM();
    static bool InstallHooks();
    static bool SafeHook(void* target, void* hook, void** original, const char* name);

private:
    static void OnBNMLoaded();

public:
    static void StartInstallThread();

private:
    static void* InstallThreadMain(void*);
};
