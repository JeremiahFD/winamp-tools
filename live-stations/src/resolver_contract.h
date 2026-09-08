#pragma once

#include "station_contracts.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace live_stations::resolver {

inline constexpr std::uint16_t kDefaultPort = 48937;
inline constexpr wchar_t kCompanionModuleName[] = L"gen_livestations_resolver.dll";

[[nodiscard]] bool IsSafeStationId(std::wstring_view stationId) noexcept;
[[nodiscard]] std::wstring BuildLoopbackUrl(
    const StationIdentity& identity,
    std::uint16_t port = kDefaultPort);
[[nodiscard]] bool ParseRequestTarget(
    std::string_view target,
    StationIdentity* identity) noexcept;

}  // namespace live_stations::resolver
