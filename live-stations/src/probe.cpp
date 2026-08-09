#include "radio_browser.h"

#include <windows.h>

#include <atomic>
#include <iostream>

int wmain(int argumentCount, wchar_t** arguments) {
    const bool urlPolicyValid =
        live_stations::IsAllowedPublicStreamUrl(L"https://stream.example.com/live.mp3") &&
        live_stations::IsAllowedPublicStreamUrl(L"http://203.0.113.10:8000/radio") &&
        !live_stations::IsAllowedPublicStreamUrl(L"file:///C:/Windows/win.ini") &&
        !live_stations::IsAllowedPublicStreamUrl(L"http://localhost:8080/") &&
        !live_stations::IsAllowedPublicStreamUrl(L"http://192.168.1.10/radio") &&
        !live_stations::IsAllowedPublicStreamUrl(L"http://station.local/radio") &&
        !live_stations::IsAllowedPublicStreamUrl(L"https://example.com/bad\r\nheader");
    if (!urlPolicyValid) {
        std::wcerr << L"Safe stream URL policy checks failed.\n";
        return 4;
    }

    std::atomic_bool cancelled = false;
    live_stations::SearchRequest request;
    request.video = argumentCount > 1 && _wcsicmp(arguments[1], L"video") == 0;
    request.text = request.video ? L"" : L"rock";
    const live_stations::SearchResult result = live_stations::Search(request, cancelled);
    if (!result.error.empty()) {
        std::wcerr << L"Search failed: " << result.error << L'\n';
        return 1;
    }
    if (result.stations.empty()) {
        std::wcerr << L"Search returned no stations.\n";
        return 2;
    }
    for (const auto& station : result.stations) {
        if (station.uuid.empty() || station.name.empty() || station.url.empty() ||
            !live_stations::IsAllowedPublicStreamUrl(station.url)) {
            std::wcerr << L"Search returned an invalid station record.\n";
            return 3;
        }
    }

    std::wcout << L"Mirror: " << result.mirror << L'\n';
    std::wcout << L"Mode: " << (request.video ? L"experimental video" : L"radio") << L'\n';
    std::wcout << L"Stations: " << result.stations.size() << L'\n';
    std::wcout << L"First: " << result.stations.front().name << L" ["
               << result.stations.front().codec << L"]\n";
    std::wcout << L"Safe stream URL policy checks passed.\n";
    return 0;
}
