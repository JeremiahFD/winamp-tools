#include "winamp_media_library_abi.h"

#include <windows.h>

#include <iostream>

using GetPluginFunction = winampMediaLibraryPlugin*(__cdecl*)();

int wmain(int argumentCount, wchar_t** arguments) {
    const wchar_t* dllPath = argumentCount > 1 ? arguments[1] : L"ml_livestations.dll";
    HMODULE module = LoadLibraryW(dllPath);
    if (module == nullptr) {
        std::wcerr << L"Could not load " << dllPath << L" (error " << GetLastError() << L").\n";
        return 1;
    }
    const auto getPlugin = reinterpret_cast<GetPluginFunction>(
        GetProcAddress(module, "winampGetMediaLibraryPlugin"));
    if (getPlugin == nullptr) {
        std::wcerr << L"Missing winampGetMediaLibraryPlugin export.\n";
        FreeLibrary(module);
        return 2;
    }
    winampMediaLibraryPlugin* plugin = getPlugin();
    const bool valid = plugin != nullptr && plugin->version == MLHDR_VER && plugin->init != nullptr &&
                       plugin->quit != nullptr && plugin->MessageProc != nullptr;
    std::wcout << (valid ? L"Media Library ABI looks valid.\n" : L"Media Library ABI is invalid.\n");
    FreeLibrary(module);
    return valid ? 0 : 3;
}
