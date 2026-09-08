#pragma once

#include "station_contracts.h"

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace live_stations {

struct Station {
    StationIdentity identity;
    std::wstring name;
    // Provider resolution should be attempted first. This is only a last-known
    // playable alias for fallback and user-owned favorites.
    std::wstring fallbackUrlAlias;
    std::wstring codec;
    std::wstring bitrate;
    std::wstring country;
    std::wstring countryCode;
    std::wstring state;
    std::wstring language;
    std::wstring tags;
    std::wstring homepageUrl;
    std::wstring lastSuccessfulCheck;
    bool hls = false;
    bool experimentalVideo = false;
};

enum class BrowseMode {
    Radio,
    ExperimentalVideo,
};

enum class TextMatch {
    Contains,
    Exact,
};

struct TextFilter {
    std::wstring value;
    TextMatch match = TextMatch::Contains;
};

struct BitrateRange {
    std::optional<std::uint32_t> minimumKbps;
    std::optional<std::uint32_t> maximumKbps;
};

enum class RadioBitratePreset {
    All,
    Under100,
    From100To159,
    From160To255,
    AtLeast256,
    Unknown,
};

enum class RadioCompatibilityPreset {
    Any,
    Best,
    Usually,
    DecoderRequired,
    Unknown,
};

struct BrowseFilters {
    TextFilter name;
    // Radio Browser models genres as station tags, so one typed filter covers
    // both genre and free-form tag browsing.
    TextFilter tag;
    TextFilter country;
    std::wstring countryCode;
    TextFilter state;
    TextFilter language;
    std::wstring codec;
    RadioCompatibilityPreset compatibility = RadioCompatibilityPreset::Any;
    BitrateRange bitrate;
};

struct SearchRequest {
    BrowseMode mode = BrowseMode::Radio;
    BrowseFilters filters;
    bool includeFacets = false;
    std::uint32_t offset = 0;
    std::uint32_t limit = 200;
};

struct BrowseFacet {
    // Stable provider identity where the endpoint supplies one (for example,
    // an ISO country code). Name remains the exact provider filter value.
    std::wstring providerId;
    std::wstring name;
    std::wstring country;
    std::wstring stationCount;
};

enum class BrowseFacetKind {
    Genre,
    Country,
    Language,
    State,
};

struct BrowseFacetPageRequest {
    BrowseFacetKind kind = BrowseFacetKind::Genre;
    std::wstring country;
    std::wstring filter;
    std::uint32_t offset = 0;
    std::uint32_t limit = 80;
};

struct PreparedFacetPagePath {
    std::wstring path;
    std::wstring elementName;
    std::wstring error;
};

struct BrowseFacetPage {
    std::vector<BrowseFacet> items;
    std::uint32_t offset = 0;
    bool hasMore = false;
    std::wstring mirror;
    std::wstring error;
};

struct ProviderStats {
    std::wstring stationTotal;
    std::wstring brokenStationTotal;
    std::wstring genreTotal;
    std::wstring countryTotal;
    std::wstring languageTotal;
};

struct BrowseFacets {
    std::vector<BrowseFacet> genres;
    std::vector<BrowseFacet> countries;
    std::vector<BrowseFacet> languages;
    ProviderStats stats;
    bool genresHaveMore = false;
    bool countriesHaveMore = false;
    bool languagesHaveMore = false;
    std::wstring mirror;
    std::wstring error;
};

struct PreparedSearchPaths {
    std::vector<std::wstring> paths;
    std::wstring error;
};

struct SearchResult {
    std::vector<Station> stations;
    BrowseFacets facets;
    std::uint32_t offset = 0;
    bool hasMore = false;
    std::wstring mirror;
    std::wstring error;
};

struct ResolveResult {
    std::wstring url;
    std::wstring error;
};

// The production DLL always uses RadioBrowserProviderOperations. The separately
// named offline-fixture DLL selects OfflineFixtureProviderOperations at compile
// time; no runtime setting can redirect the installed plug-in.
struct ProviderOperations {
    SearchResult (*search)(const SearchRequest& request, const std::atomic_bool& cancelled);
    BrowseFacets (*loadBrowseFacets)(const std::atomic_bool& cancelled);
    BrowseFacetPage (*loadBrowseFacetPage)(
        const BrowseFacetPageRequest& request,
        const std::atomic_bool& cancelled);
    ResolveResult (*resolveAndCountClick)(
        const Station& station,
        const std::atomic_bool& cancelled);
};

SearchResult Search(const SearchRequest& request, const std::atomic_bool& cancelled);
BrowseFacets LoadBrowseFacets(const std::atomic_bool& cancelled);
BrowseFacetPage LoadBrowseFacetPage(
    const BrowseFacetPageRequest& request,
    const std::atomic_bool& cancelled);
PreparedSearchPaths PrepareSearchPaths(const SearchRequest& request);
PreparedFacetPagePath PrepareFacetPagePath(const BrowseFacetPageRequest& request);
BitrateRange BitrateRangeForRadioPreset(RadioBitratePreset preset);
RadioBitratePreset ClassifyRadioBitrate(std::optional<std::uint32_t> bitrateKbps);
ResolveResult ResolveAndCountClick(
    const Station& station,
    const std::atomic_bool& cancelled);
const ProviderOperations& RadioBrowserProviderOperations();
#if defined(LIVE_STATIONS_OFFLINE_FIXTURE)
const ProviderOperations& OfflineFixtureProviderOperations();
#endif
bool IsAllowedPublicStreamUrl(const std::wstring& url);

}  // namespace live_stations
