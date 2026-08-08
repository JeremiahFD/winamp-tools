#include "wasapi_loopback.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#include <audioclient.h>
#include <propsys.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <propvarutil.h>
#include <wrl/client.h>

namespace synced_visualizer {
namespace {

using Microsoft::WRL::ComPtr;

constexpr std::uint32_t kVisualizationFrameCount = 576;
constexpr std::uint32_t kVisualizationChannels = 2;

class ComApartment final {
public:
    explicit ComApartment(bool initialized) noexcept : initialized_(initialized) {}
    ~ComApartment() {
        if (initialized_) {
            CoUninitialize();
        }
    }

    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;

private:
    bool initialized_ = false;
};

bool IsFloatFormat(const WAVEFORMATEX* format) {
    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        return true;
    }
    if (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE ||
        format->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
        return false;
    }
    const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
    return extensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
}

bool IsPcmFormat(const WAVEFORMATEX* format) {
    if (format->wFormatTag == WAVE_FORMAT_PCM) {
        return true;
    }
    if (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE ||
        format->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
        return false;
    }
    const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
    return extensible->SubFormat == KSDATAFORMAT_SUBTYPE_PCM;
}

float ClampSample(float sample) {
    if (!std::isfinite(sample)) {
        return 0.0F;
    }
    return std::clamp(sample, -1.0F, 1.0F);
}

float ReadSample(const BYTE* frame, std::uint32_t channel, const WAVEFORMATEX* format) {
    const std::uint32_t bytesPerSample = format->wBitsPerSample / 8;
    const BYTE* sample = frame + channel * bytesPerSample;

    if (IsFloatFormat(format)) {
        if (format->wBitsPerSample == 32) {
            float value = 0.0F;
            std::memcpy(&value, sample, sizeof(value));
            return ClampSample(value);
        }
        if (format->wBitsPerSample == 64) {
            double value = 0.0;
            std::memcpy(&value, sample, sizeof(value));
            return ClampSample(static_cast<float>(value));
        }
    }

    if (!IsPcmFormat(format)) {
        return 0.0F;
    }

    switch (format->wBitsPerSample) {
        case 8:
            return (static_cast<int>(*sample) - 128) / 128.0F;
        case 16: {
            std::int16_t value = 0;
            std::memcpy(&value, sample, sizeof(value));
            return static_cast<float>(value) / 32768.0F;
        }
        case 24: {
            std::int32_t value = static_cast<std::int32_t>(sample[0]) |
                                 (static_cast<std::int32_t>(sample[1]) << 8) |
                                 (static_cast<std::int32_t>(sample[2]) << 16);
            if ((value & 0x00800000) != 0) {
                value |= static_cast<std::int32_t>(0xFF000000);
            }
            return static_cast<float>(value) / 8388608.0F;
        }
        case 32: {
            std::int32_t value = 0;
            std::memcpy(&value, sample, sizeof(value));
            return static_cast<float>(static_cast<double>(value) / 2147483648.0);
        }
        default:
            return 0.0F;
    }
}

std::int16_t ToInt16(float sample) {
    const float clamped = ClampSample(sample);
    if (clamped <= -1.0F) {
        return std::numeric_limits<std::int16_t>::min();
    }
    return static_cast<std::int16_t>(std::lround(clamped * 32767.0F));
}

HRESULT GetDeviceName(IMMDevice* device, std::wstring* name) {
    ComPtr<IPropertyStore> properties;
    HRESULT result = device->OpenPropertyStore(STGM_READ, &properties);
    if (FAILED(result)) {
        return result;
    }

    PROPVARIANT value;
    PropVariantInit(&value);
    result = properties->GetValue(PKEY_Device_FriendlyName, &value);
    if (SUCCEEDED(result) && value.vt == VT_LPWSTR && value.pwszVal != nullptr) {
        *name = value.pwszVal;
    }
    PropVariantClear(&value);
    return result;
}

bool IsSupportedMixFormat(const WAVEFORMATEX* format) {
    if (format == nullptr || format->nChannels == 0 || format->nSamplesPerSec == 0 ||
        format->nBlockAlign == 0 || format->wBitsPerSample % 8 != 0) {
        return false;
    }
    if (IsFloatFormat(format)) {
        return format->wBitsPerSample == 32 || format->wBitsPerSample == 64;
    }
    if (IsPcmFormat(format)) {
        return format->wBitsPerSample == 8 || format->wBitsPerSample == 16 ||
               format->wBitsPerSample == 24 || format->wBitsPerSample == 32;
    }
    return false;
}

}  // namespace

WasapiLoopbackCapture::~WasapiLoopbackCapture() {
    Stop();
}

HRESULT WasapiLoopbackCapture::Start(PcmBlockCallback callback, void* context) {
    Stop();

    callback_ = callback;
    callbackContext_ = context;
    stopRequested_.store(false);
    paused_.store(false);
    running_.store(false);
    sampleRate_.store(0);
    sourceSampleRate_.store(0);
    sourceChannels_.store(0);
    sourceFrames_.store(0);
    visualizationFrames_.store(0);
    discontinuities_.store(0);
    peak_.store(0.0F);
    runtimeResult_.store(S_OK);

    {
        std::lock_guard lock(stateMutex_);
        initializationFinished_ = false;
        initializationResult_ = E_UNEXPECTED;
        deviceName_.clear();
    }

    try {
        worker_ = std::thread(&WasapiLoopbackCapture::CaptureThread, this);
    } catch (...) {
        return E_OUTOFMEMORY;
    }

    std::unique_lock lock(stateMutex_);
    const bool initialized = initializationChanged_.wait_for(
        lock, std::chrono::seconds(5), [this] { return initializationFinished_; });
    if (!initialized) {
        lock.unlock();
        Stop();
        return HRESULT_FROM_WIN32(WAIT_TIMEOUT);
    }
    const HRESULT result = initializationResult_;
    lock.unlock();

    if (FAILED(result)) {
        Stop();
    }
    return result;
}

void WasapiLoopbackCapture::Stop() {
    stopRequested_.store(true);
    if (worker_.joinable()) {
        worker_.join();
    }
    running_.store(false);
}

void WasapiLoopbackCapture::SetPaused(bool paused) noexcept {
    paused_.store(paused);
}

bool WasapiLoopbackCapture::IsRunning() const noexcept {
    return running_.load();
}

bool WasapiLoopbackCapture::IsPaused() const noexcept {
    return paused_.load();
}

std::uint32_t WasapiLoopbackCapture::SampleRate() const noexcept {
    return sampleRate_.load();
}

std::uint32_t WasapiLoopbackCapture::SourceSampleRate() const noexcept {
    return sourceSampleRate_.load();
}

std::uint32_t WasapiLoopbackCapture::SourceChannels() const noexcept {
    return sourceChannels_.load();
}

std::wstring WasapiLoopbackCapture::DeviceName() const {
    std::lock_guard lock(stateMutex_);
    return deviceName_;
}

LoopbackStats WasapiLoopbackCapture::Stats() const noexcept {
    return LoopbackStats{
        sourceFrames_.load(),
        visualizationFrames_.load(),
        discontinuities_.load(),
        peak_.load(),
        runtimeResult_.load(),
    };
}

void WasapiLoopbackCapture::SignalInitialization(HRESULT result) {
    {
        std::lock_guard lock(stateMutex_);
        initializationResult_ = result;
        initializationFinished_ = true;
    }
    initializationChanged_.notify_all();
}

void WasapiLoopbackCapture::CaptureThread() {
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool comInitialized = SUCCEEDED(result);
    if (result == RPC_E_CHANGED_MODE) {
        result = S_OK;
    }
    if (FAILED(result)) {
        SignalInitialization(result);
        return;
    }

    // This guard must be declared before the COM smart pointers. Destruction is
    // reversed, so every interface is released before CoUninitialize runs.
    ComApartment apartment(comInitialized);
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> device;
    ComPtr<IAudioClient> audioClient;
    ComPtr<IAudioCaptureClient> captureClient;
    WAVEFORMATEX* mixFormat = nullptr;

    result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              IID_PPV_ARGS(&enumerator));
    if (SUCCEEDED(result)) {
        result = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
    }
    if (SUCCEEDED(result)) {
        std::wstring name;
        if (SUCCEEDED(GetDeviceName(device.Get(), &name))) {
            std::lock_guard lock(stateMutex_);
            deviceName_ = std::move(name);
        }
        result = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void**>(audioClient.GetAddressOf()));
    }
    if (SUCCEEDED(result)) {
        result = audioClient->GetMixFormat(&mixFormat);
    }
    if (SUCCEEDED(result) && !IsSupportedMixFormat(mixFormat)) {
        result = AUDCLNT_E_UNSUPPORTED_FORMAT;
    }
    if (SUCCEEDED(result)) {
        sourceSampleRate_.store(mixFormat->nSamplesPerSec);
        sampleRate_.store(mixFormat->nSamplesPerSec);
        sourceChannels_.store(mixFormat->nChannels);
        result = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                         AUDCLNT_STREAMFLAGS_LOOPBACK,
                                         0, 0, mixFormat, nullptr);
    }
    if (SUCCEEDED(result)) {
        result = audioClient->GetService(IID_PPV_ARGS(&captureClient));
    }
    if (SUCCEEDED(result)) {
        result = audioClient->Start();
    }

    if (FAILED(result)) {
        runtimeResult_.store(result);
        SignalInitialization(result);
        if (mixFormat != nullptr) {
            CoTaskMemFree(mixFormat);
        }
        return;
    }

    running_.store(true);
    SignalInitialization(S_OK);

    std::array<std::int16_t, kVisualizationFrameCount * kVisualizationChannels> visualizationBuffer{};
    std::uint32_t bufferedFrames = 0;
    while (!stopRequested_.load()) {
        UINT32 packetFrames = 0;
        result = captureClient->GetNextPacketSize(&packetFrames);
        if (FAILED(result)) {
            break;
        }
        if (packetFrames == 0) {
            Sleep(5);
            continue;
        }

        while (packetFrames > 0 && !stopRequested_.load()) {
            BYTE* data = nullptr;
            UINT32 frameCount = 0;
            DWORD flags = 0;
            result = captureClient->GetBuffer(&data, &frameCount, &flags, nullptr, nullptr);
            if (FAILED(result)) {
                break;
            }

            sourceFrames_.fetch_add(frameCount);
            if ((flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) != 0) {
                discontinuities_.fetch_add(1);
            }

            if (!paused_.load()) {
                const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || data == nullptr;
                for (UINT32 frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
                    float left = 0.0F;
                    float right = 0.0F;
                    if (!silent) {
                        const BYTE* frame = data + static_cast<std::size_t>(frameIndex) * mixFormat->nBlockAlign;
                        left = ReadSample(frame, 0, mixFormat);
                        right = mixFormat->nChannels > 1 ? ReadSample(frame, 1, mixFormat) : left;
                    }

                    float observedPeak = std::max(std::abs(left), std::abs(right));
                    float priorPeak = peak_.load();
                    while (observedPeak > priorPeak &&
                           !peak_.compare_exchange_weak(priorPeak, observedPeak)) {
                    }

                    const std::size_t destination = static_cast<std::size_t>(bufferedFrames) * 2;
                    visualizationBuffer[destination] = ToInt16(left);
                    visualizationBuffer[destination + 1] = ToInt16(right);
                    ++bufferedFrames;

                    if (bufferedFrames == kVisualizationFrameCount) {
                        if (callback_ != nullptr) {
                            callback_(visualizationBuffer.data(), kVisualizationFrameCount,
                                      mixFormat->nSamplesPerSec, callbackContext_);
                        }
                        visualizationFrames_.fetch_add(kVisualizationFrameCount);
                        bufferedFrames = 0;
                    }
                }
            }

            const HRESULT releaseResult = captureClient->ReleaseBuffer(frameCount);
            if (FAILED(releaseResult)) {
                result = releaseResult;
                break;
            }
            result = captureClient->GetNextPacketSize(&packetFrames);
            if (FAILED(result)) {
                break;
            }
        }

        if (FAILED(result)) {
            break;
        }
    }

    audioClient->Stop();
    running_.store(false);
    runtimeResult_.store(result);
    CoTaskMemFree(mixFormat);
}

}  // namespace synced_visualizer
