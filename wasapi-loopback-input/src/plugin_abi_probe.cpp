#include "winamp_input_abi.h"

#include <filesystem>
#include <iostream>

namespace {

using GetInputModule = In_Module* (*)();

}  // namespace

int wmain(int argc, wchar_t** argv) {
    std::filesystem::path dllPath;
    if (argc > 1) {
        dllPath = argv[1];
    } else {
        wchar_t executablePath[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, executablePath, MAX_PATH) == 0) {
            std::wcerr << L"Unable to locate the ABI probe executable.\n";
            return 1;
        }
        dllPath = std::filesystem::path(executablePath).parent_path() / L"in_svloopback.dll";
    }

    HMODULE library = LoadLibraryW(dllPath.c_str());
    if (library == nullptr) {
        std::wcerr << L"LoadLibrary failed for " << dllPath << L" (error "
                   << GetLastError() << L")\n";
        return 2;
    }

    const auto getModule = reinterpret_cast<GetInputModule>(
        GetProcAddress(library, "winampGetInModule2"));
    if (getModule == nullptr) {
        std::wcerr << L"winampGetInModule2 export was not found.\n";
        FreeLibrary(library);
        return 3;
    }

    In_Module* module = getModule();
    const bool valid = module != nullptr &&
                       module->version == kWinampInputModuleVersion &&
                       module->description != nullptr &&
                       module->UsesOutputPlug == 0 &&
                       module->IsOurFile != nullptr &&
                       module->IsOurFile(const_cast<char*>("loopback://")) == 1 &&
                       module->IsOurFile(const_cast<char*>("svloopback://")) == 1 &&
                       module->IsOurFile(const_cast<char*>("linein://")) == 0 &&
                       module->Play != nullptr && module->Stop != nullptr;

    if (module != nullptr) {
        std::cout << "Description: "
                  << (module->description != nullptr ? module->description : "<missing>") << "\n"
                  << "ABI version: 0x" << std::hex << module->version << "\n"
                  << "Uses Winamp output plug-in: " << std::dec << module->UsesOutputPlug << "\n";
    }
    std::cout << "ABI validation: " << (valid ? "PASS" : "FAIL") << "\n";

    FreeLibrary(library);
    return valid ? 0 : 4;
}

