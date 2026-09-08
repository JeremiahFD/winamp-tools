#include "chat_streams_contract.h"
#include "favorites.h"
#include "radio_browser.h"

#include <iostream>
#include <string>

namespace {

int gFailures = 0;

void Check(bool condition, const wchar_t* description) {
    if (!condition) {
        ++gFailures;
        std::wcerr << L"FAILED: " << description << L'\n';
    }
}

bool Contains(const std::wstring& value, const wchar_t* expected) {
    return value.find(expected) != std::wstring::npos;
}

}  // namespace

int wmain() {
    live_stations::SearchRequest request;
    request.filters.name = {L"Public Radio", live_stations::TextMatch::Contains};
    request.filters.tag = {L"jazz fusion", live_stations::TextMatch::Exact};
    request.filters.country = {L"United States", live_stations::TextMatch::Exact};
    request.filters.countryCode = L"us";
    request.filters.state = {L"New York", live_stations::TextMatch::Exact};
    request.filters.language = {L"English", live_stations::TextMatch::Contains};
    request.filters.codec = L"MP3";
    request.filters.bitrate.minimumKbps = 64U;
    request.filters.bitrate.maximumKbps = 320U;
    request.offset = 100U;
    request.limit = 50U;

    const live_stations::PreparedSearchPaths prepared =
        live_stations::PrepareSearchPaths(request);
    Check(prepared.error.empty(), L"typed Radio Browser filters are accepted");
    Check(prepared.paths.size() == 1U, L"radio browsing prepares one request path");
    if (prepared.paths.size() == 1U) {
        const std::wstring& path = prepared.paths.front();
        Check(Contains(path, L"name=Public%20Radio&nameExact=false"), L"name contains filter");
        Check(Contains(path, L"tag=jazz%20fusion&tagExact=true"), L"genre/tag exact filter");
        Check(
            Contains(path, L"country=United%20States&countryExact=true"),
            L"country exact filter");
        Check(Contains(path, L"countrycode=US"), L"normalized country-code filter");
        Check(Contains(path, L"state=New%20York&stateExact=true"), L"state exact filter");
        Check(Contains(path, L"language=English&languageExact=false"), L"language filter");
        Check(Contains(path, L"codec=MP3"), L"codec filter");
        Check(Contains(path, L"bitrateMin=64"), L"minimum bitrate filter");
        Check(Contains(path, L"bitrateMax=320"), L"maximum bitrate filter");
        Check(Contains(path, L"offset=100"), L"station page offset is typed");
        Check(Contains(path, L"limit=50"), L"station page size remains bounded");
    }

    live_stations::BrowseFacetPageRequest genrePage;
    genrePage.kind = live_stations::BrowseFacetKind::Genre;
    genrePage.offset = 160U;
    genrePage.limit = 80U;
    const auto preparedGenrePage = live_stations::PrepareFacetPagePath(genrePage);
    Check(preparedGenrePage.error.empty() &&
              Contains(preparedGenrePage.path, L"/xml/tags?") &&
              Contains(preparedGenrePage.path, L"offset=160") &&
              Contains(preparedGenrePage.path, L"limit=80"),
          L"genre categories use bounded replacement pages");

    live_stations::BrowseFacetPageRequest statePage;
    statePage.kind = live_stations::BrowseFacetKind::State;
    statePage.country = L"United States";
    statePage.limit = 80U;
    const auto preparedStatePage = live_stations::PrepareFacetPagePath(statePage);
    Check(preparedStatePage.error.empty() &&
              Contains(preparedStatePage.path, L"/xml/states?") &&
              Contains(preparedStatePage.path, L"country=United%20States"),
          L"state pages remain scoped to the selected country");

    live_stations::BrowseFacetPageRequest invalidFacetPage;
    invalidFacetPage.limit = 201U;
    Check(!live_stations::PrepareFacetPagePath(invalidFacetPage).error.empty(),
          L"category pages reject unbounded row counts");

    live_stations::SearchRequest invalidStationPage;
    invalidStationPage.limit = 201U;
    Check(!live_stations::PrepareSearchPaths(invalidStationPage).error.empty(),
          L"station pages reject more than 200 visible rows");

    const auto under100 = live_stations::BitrateRangeForRadioPreset(
        live_stations::RadioBitratePreset::Under100);
    const auto from100 = live_stations::BitrateRangeForRadioPreset(
        live_stations::RadioBitratePreset::From100To159);
    const auto from160 = live_stations::BitrateRangeForRadioPreset(
        live_stations::RadioBitratePreset::From160To255);
    const auto atLeast256 = live_stations::BitrateRangeForRadioPreset(
        live_stations::RadioBitratePreset::AtLeast256);
    const auto unknown = live_stations::BitrateRangeForRadioPreset(
        live_stations::RadioBitratePreset::Unknown);
    const auto all = live_stations::BitrateRangeForRadioPreset(
        live_stations::RadioBitratePreset::All);
    Check(!under100.minimumKbps.has_value() && under100.maximumKbps == 99U,
          L"under-100 preset has an inclusive 99 kbps maximum");
    Check(from100.minimumKbps == 100U && from100.maximumKbps == 159U,
          L"100-159 preset has deterministic inclusive boundaries");
    Check(from160.minimumKbps == 160U && from160.maximumKbps == 255U,
          L"160-255 preset has deterministic inclusive boundaries");
    Check(atLeast256.minimumKbps == 256U && !atLeast256.maximumKbps.has_value(),
          L"256-plus preset has an inclusive minimum and no maximum");
    Check(unknown.minimumKbps == 0U && unknown.maximumKbps == 0U,
          L"unknown preset explicitly requests provider-reported zero bitrate");
    Check(!all.minimumKbps.has_value() && !all.maximumKbps.has_value(),
          L"all-bitrates preset emits no bitrate bounds");

    Check(live_stations::ClassifyRadioBitrate(std::nullopt) ==
              live_stations::RadioBitratePreset::Unknown,
          L"missing bitrate classifies as unknown");
    Check(live_stations::ClassifyRadioBitrate(0U) ==
              live_stations::RadioBitratePreset::Unknown,
          L"zero bitrate classifies as unknown");
    Check(live_stations::ClassifyRadioBitrate(99U) ==
              live_stations::RadioBitratePreset::Under100,
          L"99 kbps remains under 100");
    Check(live_stations::ClassifyRadioBitrate(100U) ==
              live_stations::RadioBitratePreset::From100To159,
          L"100 kbps enters the 100-159 bucket");
    Check(live_stations::ClassifyRadioBitrate(159U) ==
              live_stations::RadioBitratePreset::From100To159,
          L"159 kbps remains in the 100-159 bucket");
    Check(live_stations::ClassifyRadioBitrate(160U) ==
              live_stations::RadioBitratePreset::From160To255,
          L"160 kbps enters the 160-255 bucket");
    Check(live_stations::ClassifyRadioBitrate(255U) ==
              live_stations::RadioBitratePreset::From160To255,
          L"255 kbps remains in the 160-255 bucket");
    Check(live_stations::ClassifyRadioBitrate(256U) ==
              live_stations::RadioBitratePreset::AtLeast256,
          L"256 kbps enters the 256-plus bucket");

    live_stations::SearchRequest unknownBitrateRequest;
    unknownBitrateRequest.filters.bitrate = unknown;
    const live_stations::PreparedSearchPaths unknownBitratePrepared =
        live_stations::PrepareSearchPaths(unknownBitrateRequest);
    Check(unknownBitratePrepared.paths.size() == 1U &&
              Contains(unknownBitratePrepared.paths.front(), L"bitrateMin=0") &&
              Contains(unknownBitratePrepared.paths.front(), L"bitrateMax=0"),
          L"unknown bitrate uses bounded server-side zero filtering");

    live_stations::SearchRequest bestCompatibility;
    bestCompatibility.filters.compatibility =
        live_stations::RadioCompatibilityPreset::Best;
    const auto bestCompatibilityPrepared =
        live_stations::PrepareSearchPaths(bestCompatibility);
    Check(bestCompatibilityPrepared.error.empty() &&
              bestCompatibilityPrepared.paths.size() == 1U &&
              Contains(bestCompatibilityPrepared.paths.front(), L"codec=MP3"),
          L"best compatibility maps to the provider MP3 filter");

    live_stations::SearchRequest usualCompatibility;
    usualCompatibility.filters.compatibility =
        live_stations::RadioCompatibilityPreset::Usually;
    const auto usualCompatibilityPrepared =
        live_stations::PrepareSearchPaths(usualCompatibility);
    Check(usualCompatibilityPrepared.error.empty() &&
              usualCompatibilityPrepared.paths.size() == 2U &&
              Contains(usualCompatibilityPrepared.paths[0], L"codec=AAC") &&
              Contains(usualCompatibilityPrepared.paths[1], L"codec=AAC%2B"),
          L"usual compatibility uses bounded AAC and AAC+ provider paths");

    live_stations::SearchRequest decoderCompatibility;
    decoderCompatibility.filters.compatibility =
        live_stations::RadioCompatibilityPreset::DecoderRequired;
    const auto decoderCompatibilityPrepared =
        live_stations::PrepareSearchPaths(decoderCompatibility);
    Check(decoderCompatibilityPrepared.error.empty() &&
              decoderCompatibilityPrepared.paths.size() == 3U,
          L"decoder-required compatibility uses a bounded three-codec group");

    live_stations::SearchRequest unknownCompatibility;
    unknownCompatibility.filters.compatibility =
        live_stations::RadioCompatibilityPreset::Unknown;
    const auto unknownCompatibilityPrepared =
        live_stations::PrepareSearchPaths(unknownCompatibility);
    Check(unknownCompatibilityPrepared.error.empty() &&
              unknownCompatibilityPrepared.paths.size() == 1U &&
              Contains(unknownCompatibilityPrepared.paths.front(), L"codec=UNKNOWN"),
          L"unknown compatibility remains an explicit provider query");

    live_stations::SearchRequest conflictingCompatibility;
    conflictingCompatibility.filters.codec = L"MP3";
    conflictingCompatibility.filters.compatibility =
        live_stations::RadioCompatibilityPreset::Usually;
    Check(!live_stations::PrepareSearchPaths(conflictingCompatibility).error.empty(),
          L"exact codec and grouped compatibility cannot conflict");

    live_stations::SearchRequest videoRequest;
    videoRequest.mode = live_stations::BrowseMode::ExperimentalVideo;
    const live_stations::PreparedSearchPaths videoPrepared =
        live_stations::PrepareSearchPaths(videoRequest);
    Check(videoPrepared.error.empty(), L"experimental video browsing contract is accepted");
    Check(videoPrepared.paths.size() == 5U, L"video browsing prepares the bounded codec set");

    live_stations::SearchRequest invalidCountry;
    invalidCountry.filters.countryCode = L"USA";
    Check(
        !live_stations::PrepareSearchPaths(invalidCountry).error.empty(),
        L"invalid country codes are rejected before network work");

    live_stations::SearchRequest invalidBitrate;
    invalidBitrate.filters.bitrate.minimumKbps = 320U;
    invalidBitrate.filters.bitrate.maximumKbps = 64U;
    Check(
        !live_stations::PrepareSearchPaths(invalidBitrate).error.empty(),
        L"inverted bitrate ranges are rejected before network work");

    live_stations::SearchRequest invalidText;
    invalidText.filters.tag.value = L"jazz\r\nunsafe";
    Check(
        !live_stations::PrepareSearchPaths(invalidText).error.empty(),
        L"control characters are rejected before network work");

    live_stations::FavoritesModel favorites;
    Check(favorites.FindFolder(live_stations::kFavoritesRootFolderId) != nullptr,
          L"favorites has a user-owned root folder");
    Check(favorites.AddFolder(10U, live_stations::kFavoritesRootFolderId, L"Jazz"),
          L"favorites adds a top-level folder");
    Check(favorites.AddFolder(11U, 10U, L"Late Night"),
          L"favorites supports nested folders");
    Check(!favorites.MoveFolder(10U, 11U), L"favorites prevents folder cycles");

    const live_stations::StationIdentity identity{
        live_stations::kRadioBrowserProviderId,
        L"station-uuid"};
    Check(
        favorites.AddOrUpdateStation(
            {identity, 11U, L"Example Jazz", L"https://old.example/live"}),
        L"favorite is keyed by provider and stable station ID");
    Check(
        favorites.AddOrUpdateStation(
            {identity, 11U, L"Example Jazz", L"https://new.example/live"}),
        L"favorite updates its replaceable URL alias");
    Check(favorites.stations().size() == 1U, L"alias updates do not duplicate favorites");
    const live_stations::FavoriteStation* favorite = favorites.FindStation(identity);
    Check(
        favorite != nullptr && favorite->fallbackUrlAlias == L"https://new.example/live",
        L"favorite retains the current fallback alias under the stable identity");
    const std::wstring serializedFavorites = live_stations::SerializeFavorites(favorites);
    live_stations::FavoritesModel restoredFavorites;
    live_stations::FavoriteFolderId nextFavoriteFolderId = 0;
    std::wstring favoritesStorageError;
    Check(
        live_stations::DeserializeFavorites(
            serializedFavorites,
            &restoredFavorites,
            &nextFavoriteFolderId,
            &favoritesStorageError),
        L"favorites storage round-trips the owned hierarchy");
    const live_stations::FavoriteStation* restoredFavorite = restoredFavorites.FindStation(identity);
    Check(
        restoredFavorite != nullptr && restoredFavorite->folderId == 11U &&
            restoredFavorite->fallbackUrlAlias == L"https://new.example/live" &&
            nextFavoriteFolderId == 12U,
        L"favorites storage preserves stable identity, alias, folder, and next ID");
    Check(
        !live_stations::DeserializeFavorites(
            L"LIVE_STATIONS_FAVORITES_V1\nF\t10\t10\tSmF6eg==\n",
            &restoredFavorites,
            &nextFavoriteFolderId,
            &favoritesStorageError),
        L"favorites storage rejects a cyclic folder hierarchy");
    Check(
        !live_stations::DeserializeFavorites(
            L"UNKNOWN\n", &restoredFavorites, &nextFavoriteFolderId, &favoritesStorageError),
        L"favorites storage rejects unknown versions");

    Check(!live_stations::chat::IsChatStreamsProviderAvailable(nullptr),
          L"missing Chat provider is safely unavailable");
    Check(
        live_stations::chat::TryOpenChatRoom(nullptr, identity) ==
            live_stations::chat::ChatOpenResult::Unavailable,
        L"missing Chat provider produces a bounded unavailable result");

    live_stations::chat::ChatStreamRoomV1 room;
    room.providerId = live_stations::kRadioBrowserProviderId;
    room.stationId = L"station-uuid";
    room.displayName = L"Example Jazz";
    room.fallbackUrlAlias = L"https://new.example/live";
    room.presenceExpiresAtUnixMilliseconds = 2'000U;
    room.advertisedPeerCount = 1U;
    Check(live_stations::chat::IsChatRoomDiscoverable(&room, 1'999U),
          L"room is discoverable while peer presence is unexpired");
    Check(!live_stations::chat::IsChatRoomDiscoverable(&room, 2'000U),
          L"room disappears when peer presence expires");
    room.fallbackUrlAlias = nullptr;
    Check(!live_stations::chat::IsChatRoomDiscoverable(&room, 1'000U),
          L"room must advertise a playable stream alias");
    room.fallbackUrlAlias = L"https://new.example/live";
    room.advertisedPeerCount = 0U;
    Check(!live_stations::chat::IsChatRoomDiscoverable(&room, 1'000U),
          L"room requires at least one advertising peer");

    if (gFailures != 0) {
        std::wcerr << gFailures << L" contract check(s) failed.\n";
        return 1;
    }
    std::wcout << L"Live Stations contracts passed.\n";
    return 0;
}
