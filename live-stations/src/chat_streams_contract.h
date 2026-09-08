#pragma once

#include "station_contracts.h"

#include <cstddef>
#include <cstdint>

namespace live_stations::chat {

inline constexpr std::uint32_t kChatStreamsContractVersion = 1;

#if defined(_MSC_VER)
#define LIVE_STATIONS_CHAT_CALL __cdecl
#else
#define LIVE_STATIONS_CHAT_CALL
#endif

// All strings are borrowed, read-only UTF-16 pointers valid only for the
// duration of the provider call. No allocation crosses the plug-in boundary.
struct ChatStreamRoomV1 {
    std::uint32_t size = sizeof(ChatStreamRoomV1);
    const wchar_t* providerId = nullptr;
    const wchar_t* stationId = nullptr;
    const wchar_t* displayName = nullptr;
    const wchar_t* fallbackUrlAlias = nullptr;
    std::uint64_t presenceExpiresAtUnixMilliseconds = 0;
    std::uint32_t advertisedPeerCount = 0;
};

enum class ChatOpenResult : std::int32_t {
    Opened = 0,
    Unavailable = 1,
    InvalidIdentity = 2,
    RoomInactive = 3,
    Failed = 4,
};

using ChatRoomVisitorV1 = void(LIVE_STATIONS_CHAT_CALL*)(
    void* visitorContext,
    const ChatStreamRoomV1* room);

struct ChatStreamsProviderV1 {
    std::uint32_t size = sizeof(ChatStreamsProviderV1);
    std::uint32_t version = kChatStreamsContractVersion;
    void* context = nullptr;
    std::int32_t(LIVE_STATIONS_CHAT_CALL* isAvailable)(void* context) = nullptr;
    // Providers must visit only stream-backed rooms with unexpired presence.
    // Consumers should still call IsChatRoomDiscoverable before displaying one.
    std::uint32_t(LIVE_STATIONS_CHAT_CALL* visitActiveRooms)(
        void* context,
        std::uint64_t nowUnixMilliseconds,
        ChatRoomVisitorV1 visitor,
        void* visitorContext) = nullptr;
    ChatOpenResult(LIVE_STATIONS_CHAT_CALL* openRoomForStream)(
        void* context,
        const wchar_t* providerId,
        const wchar_t* stationId) = nullptr;
};

using GetChatStreamsProviderV1 = const ChatStreamsProviderV1*(LIVE_STATIONS_CHAT_CALL*)();

[[nodiscard]] inline bool IsChatRoomDiscoverable(
    const ChatStreamRoomV1* room,
    std::uint64_t nowUnixMilliseconds) noexcept {
    return room != nullptr && room->size >= sizeof(ChatStreamRoomV1) &&
           room->providerId != nullptr && room->providerId[0] != L'\0' &&
           room->stationId != nullptr && room->stationId[0] != L'\0' &&
           room->displayName != nullptr && room->displayName[0] != L'\0' &&
           room->fallbackUrlAlias != nullptr && room->fallbackUrlAlias[0] != L'\0' &&
           room->advertisedPeerCount > 0 &&
           room->presenceExpiresAtUnixMilliseconds > nowUnixMilliseconds;
}

[[nodiscard]] inline bool IsChatStreamsProviderAvailable(
    const ChatStreamsProviderV1* provider) noexcept {
    return provider != nullptr && provider->size >= sizeof(ChatStreamsProviderV1) &&
           provider->version == kChatStreamsContractVersion &&
           provider->isAvailable != nullptr && provider->isAvailable(provider->context) != 0;
}

[[nodiscard]] inline std::uint32_t VisitActiveChatStreamRooms(
    const ChatStreamsProviderV1* provider,
    std::uint64_t nowUnixMilliseconds,
    ChatRoomVisitorV1 visitor,
    void* visitorContext) noexcept {
    if (!IsChatStreamsProviderAvailable(provider) || provider->visitActiveRooms == nullptr ||
        visitor == nullptr) {
        return 0;
    }
    return provider->visitActiveRooms(
        provider->context, nowUnixMilliseconds, visitor, visitorContext);
}

[[nodiscard]] inline ChatOpenResult TryOpenChatRoom(
    const ChatStreamsProviderV1* provider,
    const StationIdentity& identity) noexcept {
    if (!identity.IsValid()) {
        return ChatOpenResult::InvalidIdentity;
    }
    if (!IsChatStreamsProviderAvailable(provider) || provider->openRoomForStream == nullptr) {
        return ChatOpenResult::Unavailable;
    }
    return provider->openRoomForStream(
        provider->context, identity.providerId.c_str(), identity.stationId.c_str());
}

#undef LIVE_STATIONS_CHAT_CALL

}  // namespace live_stations::chat
