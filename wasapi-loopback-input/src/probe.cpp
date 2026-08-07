#include "wasapi_loopback.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <thread>

namespace {

struct ProbeAccumulator {
    std::uint64_t frames = 0;
    double sumSquares = 0.0;
    float peak = 0.0F;
};

void OnPcmBlock(const std::int16_t* samples,
                std::uint32_t frameCount,
                std::uint32_t,
                void* context) {
    auto* accumulator = static_cast<ProbeAccumulator*>(context);
    for (std::uint32_t index = 0; index < frameCount * 2; ++index) {
        const float sample = static_cast<float>(samples[index]) / 32768.0F;
        accumulator->peak = std::max(accumulator->peak, std::abs(sample));
        accumulator->sumSquares += static_cast<double>(sample) * sample;
    }
    accumulator->frames += frameCount;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    int seconds = 5;
    if (argc > 1) {
        seconds = std::clamp(_wtoi(argv[1]), 1, 60);
    }

    ProbeAccumulator accumulator;
    synced_visualizer::WasapiLoopbackCapture capture;
    const HRESULT result = capture.Start(&OnPcmBlock, &accumulator);
    if (FAILED(result)) {
        std::wcerr << L"Unable to start WASAPI loopback. HRESULT=0x"
                   << std::hex << static_cast<unsigned long>(result) << L"\n";
        return 1;
    }

    std::wcout << L"Device: " << capture.DeviceName() << L"\n"
               << L"Source: " << capture.SourceSampleRate() << L" Hz, "
               << capture.SourceChannels() << L" channel(s)\n"
               << L"Visualization: " << capture.SampleRate() << L" Hz, 2 channel(s)\n"
               << L"Capturing for " << seconds << L" second(s)...\n";

    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    capture.Stop();

    const auto stats = capture.Stats();
    const double sampleCount = static_cast<double>(accumulator.frames) * 2.0;
    const double rms = sampleCount > 0.0 ? std::sqrt(accumulator.sumSquares / sampleCount) : 0.0;

    std::wcout << std::fixed << std::setprecision(6)
               << L"Captured source frames: " << stats.sourceFrames << L"\n"
               << L"Visualization frames: " << stats.visualizationFrames << L"\n"
               << L"Discontinuities: " << stats.discontinuities << L"\n"
               << L"Peak: " << accumulator.peak << L"\n"
               << L"RMS: " << rms << L"\n"
               << L"Signal: " << (accumulator.peak >= 0.001F ? L"YES" : L"NO / SILENT") << L"\n";

    if (FAILED(stats.runtimeResult)) {
        std::wcerr << L"Capture ended with HRESULT=0x" << std::hex
                   << static_cast<unsigned long>(stats.runtimeResult) << L"\n";
        return 2;
    }
    return 0;
}
