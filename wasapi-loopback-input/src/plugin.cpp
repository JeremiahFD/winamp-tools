#include "wasapi_loopback.h"
#include "winamp_input_abi.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

using synced_visualizer::WasapiLoopbackCapture;

constexpr char kPrimaryScheme[] = "loopback://";
constexpr char kAlternateScheme[] = "svloopback://";

char g_description[] = "Winamp Tools WASAPI Loopback Input v0.1.1 (x86)";
char g_extensions[] = "SVLOOPBACK\0Winamp Tools Loopback URL (*.SVLOOPBACK)\0";
char g_title[] = "System Output (WASAPI Loopback)";
std::atomic<bool> g_visualizationReady{false};
std::atomic<bool> g_saInitialized{false};
WasapiLoopbackCapture g_capture;

In_Module g_module;

void ShowResult(HWND parent, const char* heading, HRESULT result) {
    char message[512]{};
    std::snprintf(message, sizeof(message),
                  "%s\n\nHRESULT: 0x%08lX\n\n"
                  "Try stopping and replaying loopback:// after selecting a working Windows output device.",
                  heading, static_cast<unsigned long>(result));
    MessageBoxA(parent, message, "Winamp Tools Loopback", MB_OK | MB_ICONERROR);
}

void OnPcmBlock(const std::int16_t* samples,
                std::uint32_t frameCount,
                std::uint32_t sampleRate,
                void*) {
    if (!g_visualizationReady.load() || frameCount != 576 || sampleRate == 0) {
        return;
    }

    const auto stats = g_capture.Stats();
    const int timestampMs = static_cast<int>(
        (stats.visualizationFrames * 1000ULL) / sampleRate);

    if (g_module.SAAddPCMData != nullptr) {
        g_module.SAAddPCMData(const_cast<std::int16_t*>(samples), 2, 16, timestampMs);
    }
    if (g_module.VSAAddPCMData != nullptr) {
        g_module.VSAAddPCMData(const_cast<std::int16_t*>(samples), 2, 16, timestampMs);
    }
}

void Config(HWND parent) {
    MessageBoxA(parent,
                "Open loopback:// (Ctrl+L) to visualize the current default Windows output device.\n\n"
                "Stop and replay the URL after changing output devices. This plug-in does not replay or record audio.",
                "Winamp Tools Loopback", MB_OK | MB_ICONINFORMATION);
}

void About(HWND parent) {
    MessageBoxA(parent,
                "Experimental WASAPI loopback bridge for Winamp visualization plug-ins.\n"
                "A Winamp Tools hobby project made with AI assistance.",
                "About Winamp Tools Loopback", MB_OK | MB_ICONINFORMATION);
}

void Init() {}

void Stop();

void Quit() {
    Stop();
}

void GetFileInfo(char*, char* title, int* lengthInMs) {
    if (title != nullptr) {
        strncpy_s(title, 256, g_title, _TRUNCATE);
    }
    if (lengthInMs != nullptr) {
        *lengthInMs = -1000;
    }
}

int InfoBox(char*, HWND parent) {
    Config(parent);
    return 0;
}

int IsOurFile(char* filename) {
    if (filename == nullptr) {
        return 0;
    }
    return _strnicmp(filename, kPrimaryScheme, sizeof(kPrimaryScheme) - 1) == 0 ||
           _strnicmp(filename, kAlternateScheme, sizeof(kAlternateScheme) - 1) == 0;
}

int Play(char* filename) {
    if (!IsOurFile(filename)) {
        return 1;
    }

    Stop();
    const HRESULT result = g_capture.Start(&OnPcmBlock, nullptr);
    if (FAILED(result)) {
        ShowResult(g_module.hMainWindow, "Unable to start WASAPI loopback capture.", result);
        return 1;
    }

    const auto sampleRate = static_cast<int>(g_capture.SampleRate());
    if (g_module.SAVSAInit != nullptr) {
        g_module.SAVSAInit(0, sampleRate);
        g_saInitialized.store(true);
    }
    if (g_module.VSASetInfo != nullptr) {
        g_module.VSASetInfo(sampleRate, 2);
    }
    if (g_module.SetInfo != nullptr) {
        g_module.SetInfo((sampleRate * 16 * 2) / 1000, sampleRate / 1000, 2, 1);
    }

    g_visualizationReady.store(true);
    return 0;
}

void Pause() {
    g_capture.SetPaused(true);
}

void UnPause() {
    g_capture.SetPaused(false);
}

int IsPaused() {
    return g_capture.IsPaused() ? 1 : 0;
}

void Stop() {
    g_visualizationReady.store(false);
    g_capture.Stop();
    if (g_saInitialized.exchange(false) && g_module.SAVSADeInit != nullptr) {
        g_module.SAVSADeInit();
    }
}

int GetLength() {
    // Winamp's live-input convention is -1000, not a generic -1 error.
    return -1000;
}

int GetOutputTime() {
    const std::uint32_t sampleRate = g_capture.SampleRate();
    if (sampleRate == 0) {
        return 0;
    }
    return static_cast<int>((g_capture.Stats().visualizationFrames * 1000ULL) / sampleRate);
}

void SetOutputTime(int) {}
void SetVolume(int) {}
void SetPan(int) {}
void EqSet(int, char[10], int) {}

In_Module CreateModule() {
    In_Module module{};
    module.version = kWinampInputModuleVersion;
    module.description = g_description;
    module.FileExtensions = g_extensions;
    module.is_seekable = 0;
    module.UsesOutputPlug = 0;
    module.Config = &Config;
    module.About = &About;
    module.Init = &Init;
    module.Quit = &Quit;
    module.GetFileInfo = &GetFileInfo;
    module.InfoBox = &InfoBox;
    module.IsOurFile = &IsOurFile;
    module.Play = &Play;
    module.Pause = &Pause;
    module.UnPause = &UnPause;
    module.IsPaused = &IsPaused;
    module.Stop = &Stop;
    module.GetLength = &GetLength;
    module.GetOutputTime = &GetOutputTime;
    module.SetOutputTime = &SetOutputTime;
    module.SetVolume = &SetVolume;
    module.SetPan = &SetPan;
    module.EQSet = &EqSet;
    return module;
}

}  // namespace

extern "C" __declspec(dllexport) In_Module* winampGetInModule2() {
    static bool initialized = false;
    if (!initialized) {
        g_module = CreateModule();
        initialized = true;
    }
    return &g_module;
}
