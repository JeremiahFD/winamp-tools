#pragma once

#include <cstdint>

namespace synced_visualizer {

inline constexpr std::uint32_t kVisualizationSampleRate = 48000;

// Converts source-frame cadence to a stable visualization-frame cadence. The
// caller emits the current stereo frame CopiesForNextSourceFrame() times. This
// zero-order hold adapter is intentionally simple: Winamp receives analysis
// samples only, never replayed audio, and continuity matters more than playback
// fidelity here.
class VisualizationRateAdapter final {
public:
    explicit VisualizationRateAdapter(
        std::uint32_t sourceSampleRate,
        std::uint32_t targetSampleRate = kVisualizationSampleRate) noexcept
        : sourceSampleRate_(sourceSampleRate), targetSampleRate_(targetSampleRate) {}

    [[nodiscard]] std::uint32_t CopiesForNextSourceFrame() noexcept {
        if (sourceSampleRate_ == 0 || targetSampleRate_ == 0) {
            return 0;
        }
        accumulator_ += targetSampleRate_;
        const auto copies = static_cast<std::uint32_t>(accumulator_ / sourceSampleRate_);
        accumulator_ %= sourceSampleRate_;
        return copies;
    }

private:
    std::uint32_t sourceSampleRate_ = 0;
    std::uint32_t targetSampleRate_ = 0;
    std::uint64_t accumulator_ = 0;
};

}  // namespace synced_visualizer
