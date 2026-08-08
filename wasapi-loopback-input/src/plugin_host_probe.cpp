#include "winamp_input_abi.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

namespace {

using GetInputModule = In_Module* (*)();

std::atomic<int> g_savsaInitCalls{0};
std::atomic<int> g_savsaDeInitCalls{0};
std::atomic<int> g_saPcmCalls{0};
std::atomic<int> g_vsaPcmCalls{0};
std::atomic<int> g_maxLatencyMs{-1};
std::atomic<int> g_saSampleRate{0};
std::atomic<int> g_vsaSampleRate{0};
std::atomic<int> g_vsaChannels{0};
std::atomic<int> g_infoSampleRateKhz{0};
std::atomic<int> g_infoChannels{0};

void SAVSAInit(int maxLatencyMs, int sampleRate) {
    g_maxLatencyMs.store(maxLatencyMs);
    g_saSampleRate.store(sampleRate);
    g_savsaInitCalls.fetch_add(1);
}

void SAVSADeInit() {
    g_savsaDeInitCalls.fetch_add(1);
}

void SAAddPCMData(void*, int channels, int bitsPerSample, int) {
    if (channels == 2 && bitsPerSample == 16) {
        g_saPcmCalls.fetch_add(1);
    }
}

void VSAAddPCMData(void*, int channels, int bitsPerSample, int) {
    if (channels == 2 && bitsPerSample == 16) {
        g_vsaPcmCalls.fetch_add(1);
    }
}

void VSASetInfo(int sampleRate, int channels) {
    g_vsaSampleRate.store(sampleRate);
    g_vsaChannels.store(channels);
}

void SetInfo(int, int sampleRateKhz, int channels, int) {
    g_infoSampleRateKhz.store(sampleRateKhz);
    g_infoChannels.store(channels);
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    constexpr int kCycles = 10;
    std::filesystem::path dllPath;
    if (argc > 1) {
        dllPath = argv[1];
    } else {
        wchar_t executablePath[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, executablePath, MAX_PATH) == 0) {
            std::wcerr << L"Unable to locate the host probe executable.\n";
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
    if (module == nullptr || module->Play == nullptr || module->Stop == nullptr) {
        std::wcerr << L"Input module is incomplete.\n";
        FreeLibrary(library);
        return 4;
    }

    module->hMainWindow = GetConsoleWindow();
    module->hDllInstance = library;
    module->SAVSAInit = &SAVSAInit;
    module->SAVSADeInit = &SAVSADeInit;
    module->SAAddPCMData = &SAAddPCMData;
    module->VSAAddPCMData = &VSAAddPCMData;
    module->VSASetInfo = &VSASetInfo;
    module->SetInfo = &SetInfo;

    if (module->Init != nullptr) {
        module->Init();
    }

    int playResult = 0;
    for (int cycle = 0; cycle < kCycles; ++cycle) {
        char address[] = "loopback://";
        playResult = module->Play(address);
        if (playResult != 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        module->Stop();
    }
    if (module->Quit != nullptr) {
        module->Quit();
    }

    const bool passed = playResult == 0 &&
                        g_savsaInitCalls.load() == kCycles &&
                        g_savsaDeInitCalls.load() == kCycles &&
                        g_saSampleRate.load() == 48000 &&
                        g_vsaSampleRate.load() == 48000 &&
                        g_vsaChannels.load() == 2 &&
                        g_infoSampleRateKhz.load() == 48 &&
                        g_infoChannels.load() == 2 &&
                        g_saPcmCalls.load() > 0 &&
                        g_vsaPcmCalls.load() > 0;

    std::wcout << L"DLL: " << dllPath << L"\n"
               << L"Start/stop cycles: " << kCycles << L"\n"
               << L"Play result: " << playResult << L"\n"
               << L"SAVSAInit: latency=" << g_maxLatencyMs.load()
               << L" ms, rate=" << g_saSampleRate.load() << L" Hz\n"
               << L"VSASetInfo: rate=" << g_vsaSampleRate.load()
               << L" Hz, channels=" << g_vsaChannels.load() << L"\n"
               << L"PCM callbacks: SA=" << g_saPcmCalls.load()
               << L", VSA=" << g_vsaPcmCalls.load() << L"\n"
               << L"Clean stop/deinit: " << g_savsaDeInitCalls.load() << L"\n"
               << L"Host integration validation: " << (passed ? L"PASS" : L"FAIL") << L"\n";

    FreeLibrary(library);
    return passed ? 0 : 5;
}
