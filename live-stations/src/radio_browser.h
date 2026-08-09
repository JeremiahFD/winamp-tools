#pragma once

#include <atomic>
#include <string>
#include <vector>

namespace live_stations {

struct Station {
    std::wstring uuid;
    std::wstring name;
    std::wstring url;
    std::wstring codec;
    std::wstring bitrate;
    std::wstring country;
    std::wstring tags;
    bool experimentalVideo = false;
};

struct SearchRequest {
    std::wstring text;
    bool video = false;
};

struct SearchResult {
    std::vector<Station> stations;
    std::wstring mirror;
    std::wstring error;
};

struct ResolveResult {
    std::wstring url;
    std::wstring error;
};

SearchResult Search(const SearchRequest& request, const std::atomic_bool& cancelled);
ResolveResult ResolveAndCountClick(
    const Station& station,
    const std::atomic_bool& cancelled);
bool IsAllowedPublicStreamUrl(const std::wstring& url);

}  // namespace live_stations
