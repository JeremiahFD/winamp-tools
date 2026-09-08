#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace live_stations {

inline constexpr wchar_t kRadioBrowserProviderId[] = L"radio-browser";

struct StationIdentity {
    std::wstring providerId;
    std::wstring stationId;

    [[nodiscard]] bool IsValid() const noexcept {
        return !providerId.empty() && !stationId.empty();
    }

    bool operator==(const StationIdentity&) const = default;
};

struct StationIdentityHash {
    [[nodiscard]] std::size_t operator()(const StationIdentity& identity) const noexcept {
        const std::size_t providerHash = std::hash<std::wstring>{}(identity.providerId);
        const std::size_t stationHash = std::hash<std::wstring>{}(identity.stationId);
        return providerHash ^ (stationHash + 0x9e3779b9U + (providerHash << 6U) +
                               (providerHash >> 2U));
    }
};

}  // namespace live_stations
