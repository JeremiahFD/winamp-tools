#pragma once

#include <windows.h>

// Minimal Winamp 2/5 input plug-in ABI declaration. The field order is part of
// the binary interface and must remain unchanged. This project intentionally
// does not bundle the historical SDK or any Winamp implementation code.

struct Out_Module;

constexpr int kWinampInputModuleVersion = 0x100;

struct In_Module {
    int version;
    char* description;
    HWND hMainWindow;
    HINSTANCE hDllInstance;
    char* FileExtensions;
    int is_seekable;
    int UsesOutputPlug;

    void (*Config)(HWND hwndParent);
    void (*About)(HWND hwndParent);
    void (*Init)();
    void (*Quit)();
    void (*GetFileInfo)(char* file, char* title, int* lengthInMs);
    int (*InfoBox)(char* file, HWND hwndParent);
    int (*IsOurFile)(char* filename);

    int (*Play)(char* filename);
    void (*Pause)();
    void (*UnPause)();
    int (*IsPaused)();
    void (*Stop)();

    int (*GetLength)();
    int (*GetOutputTime)();
    void (*SetOutputTime)(int timeInMs);

    void (*SetVolume)(int volume);
    void (*SetPan)(int pan);

    void (*SAVSAInit)(int maxLatencyInMs, int sampleRate);
    void (*SAVSADeInit)();
    void (*SAAddPCMData)(void* pcmData, int channels, int bitsPerSample, int timestampMs);
    int (*SAGetMode)();
    void (*SAAdd)(void* data, int timestampMs, int mode);

    void (*VSAAddPCMData)(void* pcmData, int channels, int bitsPerSample, int timestampMs);
    int (*VSAGetMode)(int* spectrumChannels, int* waveformChannels);
    void (*VSAAdd)(void* data, int timestampMs);
    void (*VSASetInfo)(int sampleRate, int channels);

    int (*dsp_isactive)();
    int (*dsp_dosamples)(short* samples, int sampleCount, int bitsPerSample, int channels, int sampleRate);

    void (*EQSet)(int enabled, char bands[10], int preamp);
    void (*SetInfo)(int bitrateKbps, int sampleRateKhz, int stereo, int synchronized);
    Out_Module* outMod;
};

