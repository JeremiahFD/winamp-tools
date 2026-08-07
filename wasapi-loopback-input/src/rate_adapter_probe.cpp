#include "visualization_rate_adapter.h"

#include <array>
#include <cstdint>
#include <iostream>

int main() {
    constexpr std::array<std::uint32_t, 12> sourceRates{
        8000, 11025, 16000, 22050, 32000, 44100,
        48000, 88200, 96000, 176400, 192000, 384000,
    };
    constexpr std::uint32_t seconds = 10;
    constexpr std::uint64_t expectedFrames =
        static_cast<std::uint64_t>(synced_visualizer::kVisualizationSampleRate) * seconds;

    bool passed = true;
    for (const auto sourceRate : sourceRates) {
        synced_visualizer::VisualizationRateAdapter adapter(sourceRate);
        std::uint64_t outputFrames = 0;
        std::uint32_t maximumCopies = 0;
        const auto sourceFrames = static_cast<std::uint64_t>(sourceRate) * seconds;
        for (std::uint64_t frame = 0; frame < sourceFrames; ++frame) {
            const auto copies = adapter.CopiesForNextSourceFrame();
            outputFrames += copies;
            if (copies > maximumCopies) {
                maximumCopies = copies;
            }
        }

        const bool ratePassed = outputFrames == expectedFrames;
        passed = passed && ratePassed;
        std::cout << sourceRate << " Hz -> " << outputFrames << " frames, max copies "
                  << maximumCopies << ": " << (ratePassed ? "PASS" : "FAIL") << "\n";
    }

    std::cout << "Rate adapter validation: " << (passed ? "PASS" : "FAIL") << "\n";
    return passed ? 0 : 1;
}
