#include "radio_browser.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cwctype>
#include <thread>

namespace live_stations {
namespace {

constexpr wchar_t kFixtureProviderId[] = L"radio-browser";
constexpr wchar_t kFixtureStreamUrl[] = L"https://stream.example.com/live-stations-fixture.mp3";

std::wstring Lowercase(const std::wstring& value) {
    std::wstring lowered = value;
    std::transform(
        lowered.begin(),
        lowered.end(),
        lowered.begin(),
        [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
    return lowered;
}

bool MatchesText(const std::wstring& value, const TextFilter& filter) {
    if (filter.value.empty()) {
        return true;
    }
    const std::wstring lowerValue = Lowercase(value);
    const std::wstring lowerFilter = Lowercase(filter.value);
    return filter.match == TextMatch::Exact ? lowerValue == lowerFilter
                                            : lowerValue.find(lowerFilter) != std::wstring::npos;
}

bool MatchesTagList(const std::wstring& tags, const TextFilter& filter) {
    if (filter.value.empty()) {
        return true;
    }
    if (filter.match != TextMatch::Exact) {
        return MatchesText(tags, filter);
    }
    const std::wstring needle = L"," + Lowercase(filter.value) + L",";
    const std::wstring haystack = L"," + Lowercase(tags) + L",";
    return haystack.find(needle) != std::wstring::npos;
}

std::optional<std::uint32_t> ParseBitrate(const std::wstring& value) {
    if (value.empty() || value == L"Unknown") {
        return std::nullopt;
    }
    wchar_t* end = nullptr;
    const unsigned long parsed = wcstoul(value.c_str(), &end, 10);
    return end != value.c_str() && *end == L'\0'
               ? std::optional<std::uint32_t>(static_cast<std::uint32_t>(parsed))
               : std::nullopt;
}

bool MatchesBitrate(const Station& station, const BitrateRange& range) {
    const std::optional<std::uint32_t> bitrate = ParseBitrate(station.bitrate);
    if (!range.minimumKbps.has_value() && !range.maximumKbps.has_value()) {
        return true;
    }
    if (!bitrate.has_value()) {
        return !range.minimumKbps.has_value() && !range.maximumKbps.has_value();
    }
    return (!range.minimumKbps.has_value() || *bitrate >= *range.minimumKbps) &&
           (!range.maximumKbps.has_value() || *bitrate <= *range.maximumKbps);
}

bool Matches(const Station& station, const SearchRequest& request) {
    const BrowseFilters& filters = request.filters;
    return MatchesText(station.name, filters.name) && MatchesTagList(station.tags, filters.tag) &&
           MatchesText(station.country, filters.country) &&
           (filters.countryCode.empty() ||
            Lowercase(station.countryCode) == Lowercase(filters.countryCode)) &&
           MatchesText(station.state, filters.state) &&
           MatchesText(station.language, filters.language) &&
           (filters.codec.empty() || Lowercase(station.codec) == Lowercase(filters.codec)) &&
           MatchesBitrate(station, filters.bitrate);
}

void WaitBrieflyForCancellation(const std::atomic_bool& cancelled) {
    for (int iteration = 0; iteration < 8 && !cancelled.load(); ++iteration) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

Station MakeStation(std::uint32_t number) {
    Station station;
    station.identity = {kFixtureProviderId, L"fixture-station-" + std::to_wstring(number)};
    station.name = number >= 56U ? L"Fixture Rock Radio " + std::to_wstring(number)
                                 : L"Fixture Jazz Radio " + std::to_wstring(number);
    station.fallbackUrlAlias = kFixtureStreamUrl;
    station.codec = L"MP3";
    station.bitrate = number == 1U ? L"64" : L"128";
    station.country = L"United States";
    station.countryCode = L"US";
    station.state = L"Fixture State";
    station.language = L"English";
    station.tags = number >= 56U ? L"rock,fixture" : L"jazz,fixture";
    station.homepageUrl = L"https://example.com/live-stations-fixture";
    station.lastSuccessfulCheck = L"fixture";
    return station;
}

std::vector<Station> FixtureStations() {
    std::vector<Station> stations;
    stations.reserve(60U);
    for (std::uint32_t number = 1U; number <= 60U; ++number) {
        stations.push_back(MakeStation(number));
    }
    return stations;
}

BrowseFacet MakeFacet(const wchar_t* name, const wchar_t* count, const wchar_t* providerId = L"") {
    BrowseFacet facet;
    facet.name = name;
    facet.stationCount = count;
    facet.providerId = providerId;
    return facet;
}

std::vector<BrowseFacet> FixtureGenres() {
    std::vector<BrowseFacet> facets;
    facets.reserve(80U);
    facets.push_back(MakeFacet(L"jazz", L"55"));
    facets.push_back(MakeFacet(L"rock", L"5"));
    for (int number = 3; number <= 80; ++number) {
        facets.push_back(MakeFacet((L"fixture-genre-" + std::to_wstring(number)).c_str(), L"1"));
    }
    return facets;
}

}  // namespace

BrowseFacets FixtureLoadBrowseFacets(const std::atomic_bool& cancelled);

BitrateRange BitrateRangeForRadioPreset(RadioBitratePreset preset) {
    switch (preset) {
        case RadioBitratePreset::Under100:
            return {std::nullopt, 99U};
        case RadioBitratePreset::From100To159:
            return {100U, 159U};
        case RadioBitratePreset::From160To255:
            return {160U, 255U};
        case RadioBitratePreset::AtLeast256:
            return {256U, std::nullopt};
        case RadioBitratePreset::Unknown:
        case RadioBitratePreset::All:
            return {};
    }
    return {};
}

RadioBitratePreset ClassifyRadioBitrate(std::optional<std::uint32_t> bitrateKbps) {
    if (!bitrateKbps.has_value() || *bitrateKbps == 0U) {
        return RadioBitratePreset::Unknown;
    }
    if (*bitrateKbps < 100U) {
        return RadioBitratePreset::Under100;
    }
    if (*bitrateKbps < 160U) {
        return RadioBitratePreset::From100To159;
    }
    return *bitrateKbps < 256U ? RadioBitratePreset::From160To255
                               : RadioBitratePreset::AtLeast256;
}

bool IsAllowedPublicStreamUrl(const std::wstring& url) {
    return url == kFixtureStreamUrl;
}

SearchResult FixtureSearch(const SearchRequest& request, const std::atomic_bool& cancelled) {
    // The full-host probe uses this one test-only query to make a completion
    // arrive after Reset has invalidated it. It intentionally ignores
    // cancellation so the view generation check, rather than provider timing,
    // is what protects the visible list.
    if (Lowercase(request.filters.name.value) == L"fixture-stale") {
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        SearchResult stale;
        stale.offset = request.offset;
        stale.mirror = L"offline fixture";
        Station sentinel = MakeStation(60U);
        sentinel.name = L"Stale fixture completion sentinel";
        stale.stations.push_back(std::move(sentinel));
        return stale;
    }
    WaitBrieflyForCancellation(cancelled);
    SearchResult result;
    result.offset = request.offset;
    result.mirror = L"offline fixture";
    if (cancelled.load()) {
        result.error = L"Cancelled.";
        return result;
    }
    for (const Station& station : FixtureStations()) {
        if (Matches(station, request)) {
            result.stations.push_back(station);
        }
    }
    const size_t offset = (std::min)(static_cast<size_t>(request.offset), result.stations.size());
    const size_t limit = (std::max)(static_cast<size_t>(1U), static_cast<size_t>(request.limit));
    result.hasMore = offset + limit < result.stations.size();
    const size_t end = (std::min)(result.stations.size(), offset + limit);
    result.stations = std::vector<Station>(
        result.stations.begin() + static_cast<std::ptrdiff_t>(offset),
        result.stations.begin() + static_cast<std::ptrdiff_t>(end));
    if (request.includeFacets) {
        result.facets = FixtureLoadBrowseFacets(cancelled);
    }
    if (result.stations.empty()) {
        result.error = L"No matching fixture stations.";
    }
    return result;
}

BrowseFacets FixtureLoadBrowseFacets(const std::atomic_bool& cancelled) {
    WaitBrieflyForCancellation(cancelled);
    BrowseFacets facets;
    facets.mirror = L"offline fixture";
    if (cancelled.load()) {
        facets.error = L"Cancelled.";
        return facets;
    }
    facets.genres = FixtureGenres();
    facets.countries.push_back(MakeFacet(L"United States", L"60", L"US"));
    facets.languages.push_back(MakeFacet(L"English", L"60"));
    facets.genresHaveMore = true;
    facets.stats.stationTotal = L"60";
    facets.stats.genreTotal = L"81";
    facets.stats.countryTotal = L"1";
    facets.stats.languageTotal = L"1";
    return facets;
}

BrowseFacetPage FixtureLoadBrowseFacetPage(
    const BrowseFacetPageRequest& request,
    const std::atomic_bool& cancelled) {
    WaitBrieflyForCancellation(cancelled);
    BrowseFacetPage page;
    page.offset = request.offset;
    page.mirror = L"offline fixture";
    if (cancelled.load()) {
        page.error = L"Cancelled.";
        return page;
    }
    if (request.kind == BrowseFacetKind::Genre) {
        if (request.offset == 0U) {
            page.items = FixtureGenres();
            page.hasMore = true;
        } else if (request.offset == 80U) {
            page.items.push_back(MakeFacet(L"fixture-genre-extra", L"1"));
        }
    } else if (request.kind == BrowseFacetKind::Country) {
        page.items.push_back(MakeFacet(L"United States", L"60", L"US"));
    } else if (request.kind == BrowseFacetKind::Language) {
        page.items.push_back(MakeFacet(L"English", L"60"));
    } else {
        page.items.push_back(MakeFacet(L"Fixture State", L"60"));
    }
    if (page.items.empty()) {
        page.error = L"No more fixture categories.";
    }
    return page;
}

ResolveResult FixtureResolveAndCountClick(
    const Station& station,
    const std::atomic_bool& cancelled) {
    WaitBrieflyForCancellation(cancelled);
    ResolveResult result;
    if (cancelled.load()) {
        result.error = L"Cancelled.";
    } else if (station.identity.providerId != kFixtureProviderId ||
               station.identity.stationId.rfind(L"fixture-station-", 0) != 0) {
        result.error = L"This is not a fixture station.";
    } else {
        result.url = kFixtureStreamUrl;
    }
    return result;
}

const ProviderOperations& OfflineFixtureProviderOperations() {
    static const ProviderOperations operations{
        FixtureSearch,
        FixtureLoadBrowseFacets,
        FixtureLoadBrowseFacetPage,
        FixtureResolveAndCountClick,
    };
    return operations;
}

}  // namespace live_stations
