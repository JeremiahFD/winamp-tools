#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include <windows.h>

namespace synced_visualizer {

using PcmBlockCallback = void (*)(const std::int16_t* interleavedStereo,
                                  std::uint32_t frameCount,
                                  std::uint32_t sampleRate,
                                  void* context);

struct LoopbackStats {
    std::uint64_t sourceFrames = 0;
    std::uint64_t visualizationFrames = 0;
    std::uint32_t discontinuities = 0;
    float peak = 0.0F;
    HRESULT runtimeResult = S_OK;
};

class WasapiLoopbackCapture final {
public:
    WasapiLoopbackCapture() = default;
    ~WasapiLoopbackCapture();

    WasapiLoopbackCapture(const WasapiLoopbackCapture&) = delete;
    WasapiLoopbackCapture& operator=(const WasapiLoopbackCapture&) = delete;

    HRESULT Start(PcmBlockCallback callback, void* context);
    void Stop();
    void SetPaused(bool paused) noexcept;

    [[nodiscard]] bool IsRunning() const noexcept;
    [[nodiscard]] bool IsPaused() const noexcept;
    [[nodiscard]] std::uint32_t SampleRate() const noexcept;
    [[nodiscard]] std::uint32_t SourceSampleRate() const noexcept;
    [[nodiscard]] std::uint32_t SourceChannels() const noexcept;
    [[nodiscard]] std::wstring DeviceName() const;
    [[nodiscard]] LoopbackStats Stats() const noexcept;

private:
    void CaptureThread();
    void SignalInitialization(HRESULT result);

    PcmBlockCallback callback_ = nullptr;
    void* callbackContext_ = nullptr;

    std::thread worker_;
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> running_{false};
    std::atomic<std::uint32_t> sampleRate_{0};
    std::atomic<std::uint32_t> sourceSampleRate_{0};
    std::atomic<std::uint32_t> sourceChannels_{0};
    std::atomic<std::uint64_t> sourceFrames_{0};
    std::atomic<std::uint64_t> visualizationFrames_{0};
    std::atomic<std::uint32_t> discontinuities_{0};
    std::atomic<float> peak_{0.0F};
    std::atomic<HRESULT> runtimeResult_{S_OK};

    mutable std::mutex stateMutex_;
    std::condition_variable initializationChanged_;
    bool initializationFinished_ = false;
    HRESULT initializationResult_ = E_UNEXPECTED;
    std::wstring deviceName_;
};

}  // namespace synced_visualizer
