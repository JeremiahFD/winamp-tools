#include "radio_browser.h"

#include <winsock2.h>
#include <windows.h>
#include <windns.h>
#include <winhttp.h>
#include <ws2tcpip.h>
#include <xmllite.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cwctype>
#include <memory>
#include <random>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace live_stations {
namespace {

constexpr wchar_t kUserAgent[] =
    L"WinampTools-LiveStations/0.1.0-alpha (+https://github.com/JeremiahFD/winamp-tools)";
constexpr size_t kMaximumResponseBytes = 4U * 1024U * 1024U;

struct Mirror {
    std::wstring host;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
};

struct InternetCloser {
    void operator()(void* handle) const noexcept {
        if (handle != nullptr) {
            WinHttpCloseHandle(handle);
        }
    }
};

using InternetHandle = std::unique_ptr<void, InternetCloser>;

std::wstring TrimTrailingDot(std::wstring value) {
    while (!value.empty() && value.back() == L'.') {
        value.pop_back();
    }
    return value;
}

std::vector<Mirror> DiscoverMirrors() {
    std::vector<Mirror> mirrors;
    std::unordered_set<std::wstring> seenHosts;
    PDNS_RECORDW records = nullptr;
    const DNS_STATUS status = DnsQuery_W(
        L"_api._tcp.radio-browser.info",
        DNS_TYPE_SRV,
        DNS_QUERY_STANDARD,
        nullptr,
        &records,
        nullptr);

    if (status == ERROR_SUCCESS) {
        for (PDNS_RECORDW record = records; record != nullptr; record = record->pNext) {
            if (record->wType != DNS_TYPE_SRV || record->Data.SRV.pNameTarget == nullptr) {
                continue;
            }
            Mirror mirror;
            mirror.host = TrimTrailingDot(record->Data.SRV.pNameTarget);
            mirror.port = record->Data.SRV.wPort;
            if (!mirror.host.empty() && seenHosts.insert(mirror.host).second) {
                mirrors.push_back(std::move(mirror));
            }
        }
    }
    if (records != nullptr) {
        DnsRecordListFree(records, DnsFreeRecordList);
    }

    std::mt19937 generator(GetTickCount());
    std::shuffle(mirrors.begin(), mirrors.end(), generator);
    if (seenHosts.insert(L"all.api.radio-browser.info").second) {
        mirrors.push_back({L"all.api.radio-browser.info", INTERNET_DEFAULT_HTTPS_PORT});
    }
    return mirrors;
}

std::wstring Win32Message(DWORD error) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        0,
        reinterpret_cast<wchar_t*>(&buffer),
        0,
        nullptr);
    std::wstring message;
    if (length != 0 && buffer != nullptr) {
        message.assign(buffer, length);
        while (!message.empty() && std::iswspace(message.back()) != 0) {
            message.pop_back();
        }
    }
    if (buffer != nullptr) {
        LocalFree(buffer);
    }
    return message.empty() ? L"Windows error " + std::to_wstring(error) : message;
}

bool Fetch(
    const Mirror& mirror,
    const std::wstring& path,
    const std::atomic_bool& cancelled,
    std::vector<unsigned char>& body,
    std::wstring& error) {
    if (cancelled.load()) {
        error = L"Cancelled.";
        return false;
    }

    InternetHandle session(WinHttpOpen(
        kUserAgent,
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0));
    if (!session) {
        error = L"Could not start Windows HTTP: " + Win32Message(GetLastError());
        return false;
    }
    WinHttpSetTimeouts(session.get(), 3000, 3000, 5000, 5000);

    InternetHandle connection(WinHttpConnect(
        session.get(), mirror.host.c_str(), mirror.port, 0));
    if (!connection) {
        error = L"Could not connect to " + mirror.host + L": " + Win32Message(GetLastError());
        return false;
    }

    InternetHandle request(WinHttpOpenRequest(
        connection.get(),
        L"GET",
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE));
    if (!request) {
        error = L"Could not create a request: " + Win32Message(GetLastError());
        return false;
    }

    if (WinHttpSendRequest(
            request.get(),
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0) == FALSE ||
        WinHttpReceiveResponse(request.get(), nullptr) == FALSE) {
        error = L"Request to " + mirror.host + L" failed: " + Win32Message(GetLastError());
        return false;
    }

    DWORD httpStatus = 0;
    DWORD statusSize = sizeof(httpStatus);
    if (WinHttpQueryHeaders(
            request.get(),
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &httpStatus,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX) == FALSE ||
        httpStatus != HTTP_STATUS_OK) {
        error = L"Radio Browser returned HTTP " + std::to_wstring(httpStatus) + L" from " +
                mirror.host + L".";
        return false;
    }

    body.clear();
    while (!cancelled.load()) {
        DWORD available = 0;
        if (WinHttpQueryDataAvailable(request.get(), &available) == FALSE) {
            error = L"Could not read the response: " + Win32Message(GetLastError());
            return false;
        }
        if (available == 0) {
            return true;
        }
        if (body.size() + available > kMaximumResponseBytes) {
            error = L"Radio Browser returned more data than this plug-in accepts.";
            return false;
        }
        const size_t start = body.size();
        body.resize(start + available);
        DWORD read = 0;
        if (WinHttpReadData(request.get(), body.data() + start, available, &read) == FALSE) {
            error = L"Could not read the response: " + Win32Message(GetLastError());
            return false;
        }
        body.resize(start + read);
    }

    error = L"Cancelled.";
    return false;
}

std::string Utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0,
        nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        length,
        nullptr,
        nullptr);
    return result;
}

std::wstring PercentEncode(const std::wstring& value) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    const std::string utf8 = Utf8(value);
    std::wstring result;
    result.reserve(utf8.size() * 3U);
    for (const unsigned char ch : utf8) {
        if (std::isalnum(ch) != 0 || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            result.push_back(static_cast<wchar_t>(ch));
        } else {
            result.push_back(L'%');
            result.push_back(static_cast<wchar_t>(kHex[(ch >> 4U) & 0x0FU]));
            result.push_back(static_cast<wchar_t>(kHex[ch & 0x0FU]));
        }
    }
    return result;
}

class XmlReaderOwner {
public:
    ~XmlReaderOwner() {
        if (reader_ != nullptr) {
            reader_->Release();
        }
        if (stream_ != nullptr) {
            stream_->Release();
        }
    }

    bool Open(const std::vector<unsigned char>& bytes, std::wstring& error) {
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
        if (memory == nullptr) {
            error = L"Not enough memory to parse the directory response.";
            return false;
        }
        void* destination = GlobalLock(memory);
        if (destination == nullptr) {
            GlobalFree(memory);
            error = L"Could not lock memory for the directory response.";
            return false;
        }
        if (!bytes.empty()) {
            CopyMemory(destination, bytes.data(), bytes.size());
        }
        GlobalUnlock(memory);

        HRESULT hr = CreateStreamOnHGlobal(memory, TRUE, &stream_);
        if (FAILED(hr)) {
            GlobalFree(memory);
            error = L"Could not create an XML input stream.";
            return false;
        }
        hr = CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(&reader_), nullptr);
        if (FAILED(hr)) {
            error = L"Windows XML Lite is unavailable.";
            return false;
        }
        hr = reader_->SetInput(stream_);
        if (FAILED(hr)) {
            error = L"Could not read the directory XML.";
            return false;
        }
        return true;
    }

    IXmlReader* Get() const noexcept { return reader_; }

private:
    IStream* stream_ = nullptr;
    IXmlReader* reader_ = nullptr;
};

std::wstring Attribute(IXmlReader* reader, const wchar_t* wanted) {
    if (reader->MoveToFirstAttribute() != S_OK) {
        return {};
    }
    do {
        const wchar_t* name = nullptr;
        const wchar_t* value = nullptr;
        if (SUCCEEDED(reader->GetLocalName(&name, nullptr)) && name != nullptr &&
            _wcsicmp(name, wanted) == 0 && SUCCEEDED(reader->GetValue(&value, nullptr)) &&
            value != nullptr) {
            reader->MoveToElement();
            return value;
        }
    } while (reader->MoveToNextAttribute() == S_OK);
    reader->MoveToElement();
    return {};
}

bool LooksLikeVideoCodec(const std::wstring& codec) {
    std::wstring upper = codec;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towupper(ch));
    });
    return upper.find(L"H.264") != std::wstring::npos || upper == L"MP4" || upper == L"FLV";
}

bool ParseStations(
    const std::vector<unsigned char>& xml,
    bool wantVideo,
    std::vector<Station>& stations,
    size_t& providerRows,
    std::wstring& error) {
    XmlReaderOwner owner;
    if (!owner.Open(xml, error)) {
        return false;
    }

    XmlNodeType nodeType = XmlNodeType_None;
    while (owner.Get()->Read(&nodeType) == S_OK) {
        if (nodeType != XmlNodeType_Element) {
            continue;
        }
        const wchar_t* localName = nullptr;
        if (FAILED(owner.Get()->GetLocalName(&localName, nullptr)) || localName == nullptr ||
            _wcsicmp(localName, L"station") != 0) {
            continue;
        }
        ++providerRows;
        Station station;
        station.identity.providerId = kRadioBrowserProviderId;
        station.identity.stationId = Attribute(owner.Get(), L"stationuuid");
        station.name = Attribute(owner.Get(), L"name");
        station.fallbackUrlAlias = Attribute(owner.Get(), L"url_resolved");
        if (station.fallbackUrlAlias.empty()) {
            station.fallbackUrlAlias = Attribute(owner.Get(), L"url");
        }
        station.codec = Attribute(owner.Get(), L"codec");
        station.bitrate = Attribute(owner.Get(), L"bitrate");
        station.country = Attribute(owner.Get(), L"country");
        station.countryCode = Attribute(owner.Get(), L"countrycode");
        station.state = Attribute(owner.Get(), L"state");
        station.language = Attribute(owner.Get(), L"language");
        station.tags = Attribute(owner.Get(), L"tags");
        station.homepageUrl = Attribute(owner.Get(), L"homepage");
        if (!station.homepageUrl.empty() &&
            !IsAllowedPublicStreamUrl(station.homepageUrl)) {
            station.homepageUrl.clear();
        }
        station.lastSuccessfulCheck =
            Attribute(owner.Get(), L"lastcheckoktime_iso8601");
        if (station.lastSuccessfulCheck.empty()) {
            station.lastSuccessfulCheck = Attribute(owner.Get(), L"lastcheckoktime");
        }
        station.hls = Attribute(owner.Get(), L"hls") == L"1";
        station.experimentalVideo = LooksLikeVideoCodec(station.codec);

        if (!station.identity.IsValid() || station.name.empty() ||
            station.fallbackUrlAlias.empty() || station.experimentalVideo != wantVideo ||
            !IsAllowedPublicStreamUrl(station.fallbackUrlAlias)) {
            continue;
        }
        stations.push_back(std::move(station));
    }
    return true;
}

bool ParseFacetList(
    const std::vector<unsigned char>& xml,
    const wchar_t* elementName,
    std::vector<BrowseFacet>& facets,
    std::wstring& error) {
    XmlReaderOwner owner;
    if (!owner.Open(xml, error)) {
        return false;
    }

    XmlNodeType nodeType = XmlNodeType_None;
    while (owner.Get()->Read(&nodeType) == S_OK) {
        if (nodeType != XmlNodeType_Element) {
            continue;
        }
        const wchar_t* localName = nullptr;
        if (FAILED(owner.Get()->GetLocalName(&localName, nullptr)) || localName == nullptr) {
            continue;
        }
        const bool expectedElement = _wcsicmp(localName, elementName) == 0;
        // Current Radio Browser XML mirrors serialize country facet rows as
        // <language ... iso_3166_1="XX"> even though the documented example
        // uses <country>. Accept both without weakening the other parsers.
        const bool currentCountryAlias = _wcsicmp(elementName, L"country") == 0 &&
                                         _wcsicmp(localName, L"language") == 0;
        if (!expectedElement && !currentCountryAlias) {
            continue;
        }
        BrowseFacet facet;
        facet.providerId = Attribute(owner.Get(), L"iso_3166_1");
        facet.name = Attribute(owner.Get(), L"name");
        facet.country = Attribute(owner.Get(), L"country");
        facet.stationCount = Attribute(owner.Get(), L"stationcount");
        if (facet.providerId.empty()) {
            facet.providerId = facet.name;
        }
        if (!facet.name.empty()) {
            facets.push_back(std::move(facet));
        }
    }
    if (facets.empty()) {
        error = L"The directory returned an empty category list.";
        return false;
    }
    return true;
}

bool FetchFacetList(
    const std::vector<Mirror>& mirrors,
    const std::wstring& path,
    const wchar_t* elementName,
    const std::atomic_bool& cancelled,
    std::vector<BrowseFacet>& facets,
    std::wstring& respondingMirror,
    std::wstring& error) {
    for (const Mirror& mirror : mirrors) {
        std::vector<unsigned char> body;
        std::wstring fetchError;
        if (!Fetch(mirror, path, cancelled, body, fetchError)) {
            error = std::move(fetchError);
            continue;
        }
        std::vector<BrowseFacet> parsed;
        std::wstring parseError;
        if (!ParseFacetList(body, elementName, parsed, parseError)) {
            error = std::move(parseError);
            continue;
        }
        facets = std::move(parsed);
        respondingMirror = mirror.host;
        return true;
    }
    return false;
}

bool ParseProviderStats(
    const std::vector<unsigned char>& xml,
    ProviderStats& stats,
    std::wstring& error) {
    XmlReaderOwner owner;
    if (!owner.Open(xml, error)) {
        return false;
    }

    XmlNodeType nodeType = XmlNodeType_None;
    while (owner.Get()->Read(&nodeType) == S_OK) {
        if (nodeType != XmlNodeType_Element) {
            continue;
        }
        const wchar_t* localName = nullptr;
        if (FAILED(owner.Get()->GetLocalName(&localName, nullptr)) || localName == nullptr ||
            _wcsicmp(localName, L"stats") != 0) {
            continue;
        }
        stats.stationTotal = Attribute(owner.Get(), L"stations");
        stats.brokenStationTotal = Attribute(owner.Get(), L"stations_broken");
        stats.genreTotal = Attribute(owner.Get(), L"tags");
        stats.countryTotal = Attribute(owner.Get(), L"countries");
        stats.languageTotal = Attribute(owner.Get(), L"languages");
        if (!stats.stationTotal.empty()) {
            return true;
        }
    }
    error = L"The directory statistics response was incomplete.";
    return false;
}

bool FetchProviderStats(
    const std::vector<Mirror>& mirrors,
    const std::atomic_bool& cancelled,
    ProviderStats& stats,
    std::wstring& respondingMirror,
    std::wstring& error) {
    for (const Mirror& mirror : mirrors) {
        std::vector<unsigned char> body;
        std::wstring fetchError;
        if (!Fetch(mirror, L"/xml/stats", cancelled, body, fetchError)) {
            error = std::move(fetchError);
            continue;
        }
        ProviderStats parsed;
        std::wstring parseError;
        if (!ParseProviderStats(body, parsed, parseError)) {
            error = std::move(parseError);
            continue;
        }
        stats = std::move(parsed);
        respondingMirror = mirror.host;
        return true;
    }
    return false;
}

bool ParseResolvedUrl(
    const std::vector<unsigned char>& xml,
    std::wstring& url,
    std::wstring& error) {
    XmlReaderOwner owner;
    if (!owner.Open(xml, error)) {
        return false;
    }
    XmlNodeType nodeType = XmlNodeType_None;
    while (owner.Get()->Read(&nodeType) == S_OK) {
        if (nodeType != XmlNodeType_Element) {
            continue;
        }
        const wchar_t* localName = nullptr;
        if (SUCCEEDED(owner.Get()->GetLocalName(&localName, nullptr)) && localName != nullptr &&
            _wcsicmp(localName, L"status") == 0) {
            url = Attribute(owner.Get(), L"url");
            return !url.empty();
        }
    }
    error = L"The station URL response was incomplete.";
    return false;
}

void AppendTextFilter(
    std::wstring& path,
    const wchar_t* parameter,
    const wchar_t* exactParameter,
    const TextFilter& filter) {
    if (filter.value.empty()) {
        return;
    }
    path += L"&";
    path += parameter;
    path += L"=" + PercentEncode(filter.value);
    path += L"&";
    path += exactParameter;
    path += filter.match == TextMatch::Exact ? L"=true" : L"=false";
}

std::wstring BuildSearchPath(
    const SearchRequest& request,
    const std::wstring& codecOverride,
    size_t limit) {
    std::wstring path = L"/xml/stations/search?hidebroken=true&order=clickcount&reverse=true&limit=" +
                        std::to_wstring(limit) + L"&offset=" +
                        std::to_wstring(request.offset);
    AppendTextFilter(path, L"name", L"nameExact", request.filters.name);
    AppendTextFilter(path, L"tag", L"tagExact", request.filters.tag);
    AppendTextFilter(path, L"country", L"countryExact", request.filters.country);
    if (!request.filters.countryCode.empty()) {
        std::wstring countryCode = request.filters.countryCode;
        std::transform(countryCode.begin(), countryCode.end(), countryCode.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(std::towupper(ch));
        });
        path += L"&countrycode=" + PercentEncode(countryCode);
    }
    AppendTextFilter(path, L"state", L"stateExact", request.filters.state);
    AppendTextFilter(path, L"language", L"languageExact", request.filters.language);
    const std::wstring& codec =
        codecOverride.empty() ? request.filters.codec : codecOverride;
    if (!codec.empty()) {
        path += L"&codec=" + PercentEncode(codec);
    }
    if (request.filters.bitrate.minimumKbps.has_value()) {
        path += L"&bitrateMin=" +
                std::to_wstring(request.filters.bitrate.minimumKbps.value());
    }
    if (request.filters.bitrate.maximumKbps.has_value()) {
        path += L"&bitrateMax=" +
                std::to_wstring(request.filters.bitrate.maximumKbps.value());
    }
    return path;
}

bool IsAsciiCountryCode(const std::wstring& value) {
    return value.empty() ||
           (value.size() == 2U &&
            ((value[0] >= L'A' && value[0] <= L'Z') ||
             (value[0] >= L'a' && value[0] <= L'z')) &&
            ((value[1] >= L'A' && value[1] <= L'Z') ||
             (value[1] >= L'a' && value[1] <= L'z')));
}

bool IsSafeFilterText(const std::wstring& value, size_t maximumLength) {
    if (value.size() > maximumLength) {
        return false;
    }
    return std::none_of(value.begin(), value.end(), [](wchar_t ch) {
        return ch < 0x20 || ch == 0x7F;
    });
}

bool IsPrivateIpv4(const std::wstring& host) {
    IN_ADDR address{};
    if (InetPtonW(AF_INET, host.c_str(), &address) != 1) {
        return false;
    }
    const uint32_t value = ntohl(address.S_un.S_addr);
    const uint8_t first = static_cast<uint8_t>((value >> 24U) & 0xFFU);
    const uint8_t second = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    return first == 10U || first == 127U || (first == 169U && second == 254U) ||
           (first == 172U && second >= 16U && second <= 31U) ||
           (first == 192U && second == 168U) || first == 0U;
}

}  // namespace

bool IsAllowedPublicStreamUrl(const std::wstring& url) {
    if (url.empty() || url.size() > 4096U) {
        return false;
    }
    for (const wchar_t ch : url) {
        if (ch < 0x20 || ch == 0x7F) {
            return false;
        }
    }

    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    std::array<wchar_t, 512> host{};
    components.lpszHostName = host.data();
    components.dwHostNameLength = static_cast<DWORD>(host.size());
    if (WinHttpCrackUrl(url.c_str(), 0, 0, &components) == FALSE ||
        (components.nScheme != INTERNET_SCHEME_HTTP &&
         components.nScheme != INTERNET_SCHEME_HTTPS) ||
        components.dwHostNameLength == 0 || components.dwHostNameLength >= host.size()) {
        return false;
    }

    std::wstring hostname(host.data(), components.dwHostNameLength);
    std::transform(hostname.begin(), hostname.end(), hostname.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    const bool ipv6Literal = hostname.find(L':') != std::wstring::npos;
    if (hostname == L"localhost" || hostname.ends_with(L".localhost") ||
        hostname.ends_with(L".local") || hostname == L"::1" ||
        (ipv6Literal && (hostname.starts_with(L"fe80:") || hostname.starts_with(L"fc") ||
                         hostname.starts_with(L"fd"))) ||
        IsPrivateIpv4(hostname)) {
        return false;
    }
    return true;
}

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
            // Radio Browser represents a missing/unknown station bitrate as 0.
            return {0U, 0U};
        case RadioBitratePreset::All:
        default:
            return {};
    }
}

RadioBitratePreset ClassifyRadioBitrate(std::optional<std::uint32_t> bitrateKbps) {
    if (!bitrateKbps.has_value() || bitrateKbps.value() == 0U) {
        return RadioBitratePreset::Unknown;
    }
    if (bitrateKbps.value() < 100U) {
        return RadioBitratePreset::Under100;
    }
    if (bitrateKbps.value() < 160U) {
        return RadioBitratePreset::From100To159;
    }
    if (bitrateKbps.value() < 256U) {
        return RadioBitratePreset::From160To255;
    }
    return RadioBitratePreset::AtLeast256;
}

PreparedFacetPagePath PrepareFacetPagePath(const BrowseFacetPageRequest& request) {
    PreparedFacetPagePath prepared;
    if (request.limit == 0U || request.limit > 200U) {
        prepared.error = L"Browse category pages must contain between 1 and 200 rows.";
        return prepared;
    }
    if (request.offset > 100000U || !IsSafeFilterText(request.filter, 255U) ||
        !IsSafeFilterText(request.country, 255U)) {
        prepared.error = L"The browse category page request is not valid.";
        return prepared;
    }

    switch (request.kind) {
        case BrowseFacetKind::Genre:
            prepared.path = L"/xml/tags";
            prepared.elementName = L"tag";
            break;
        case BrowseFacetKind::Country:
            prepared.path = L"/xml/countries";
            prepared.elementName = L"country";
            break;
        case BrowseFacetKind::Language:
            prepared.path = L"/xml/languages";
            prepared.elementName = L"language";
            break;
        case BrowseFacetKind::State:
            if (request.country.empty()) {
                prepared.error = L"A country is required before browsing states or regions.";
                return prepared;
            }
            prepared.path = L"/xml/states";
            prepared.elementName = L"state";
            break;
    }
    if (!request.filter.empty()) {
        prepared.path += L"/" + PercentEncode(request.filter);
    }
    prepared.path += L"?order=stationcount&reverse=true&hidebroken=true&offset=" +
                     std::to_wstring(request.offset) + L"&limit=" +
                     std::to_wstring(request.limit);
    if (request.kind == BrowseFacetKind::State) {
        prepared.path += L"&country=" + PercentEncode(request.country);
    }
    return prepared;
}

PreparedSearchPaths PrepareSearchPaths(const SearchRequest& request) {
    PreparedSearchPaths prepared;
    if (request.limit == 0U || request.limit > 200U || request.offset > 1'000'000U) {
        prepared.error = L"Station pages must contain between 1 and 200 rows.";
        return prepared;
    }
    if (!IsSafeFilterText(request.filters.name.value, 255U) ||
        !IsSafeFilterText(request.filters.tag.value, 255U) ||
        !IsSafeFilterText(request.filters.country.value, 255U) ||
        !IsSafeFilterText(request.filters.state.value, 255U) ||
        !IsSafeFilterText(request.filters.language.value, 255U) ||
        !IsSafeFilterText(request.filters.codec, 64U)) {
        prepared.error = L"A browse filter is too long or contains a control character.";
        return prepared;
    }
    if (!IsAsciiCountryCode(request.filters.countryCode)) {
        prepared.error = L"Country code must be a two-letter ISO code.";
        return prepared;
    }
    if (!request.filters.codec.empty() &&
        request.filters.compatibility != RadioCompatibilityPreset::Any) {
        prepared.error = L"Choose either an exact codec or a compatibility group.";
        return prepared;
    }
    constexpr std::uint32_t kMaximumBitrateKbps = 1'000'000U;
    if ((request.filters.bitrate.minimumKbps.has_value() &&
         request.filters.bitrate.minimumKbps.value() > kMaximumBitrateKbps) ||
        (request.filters.bitrate.maximumKbps.has_value() &&
         request.filters.bitrate.maximumKbps.value() > kMaximumBitrateKbps)) {
        prepared.error = L"Bitrate filters exceed the provider limit.";
        return prepared;
    }
    if (request.filters.bitrate.minimumKbps.has_value() &&
        request.filters.bitrate.maximumKbps.has_value() &&
        request.filters.bitrate.minimumKbps.value() >
            request.filters.bitrate.maximumKbps.value()) {
        prepared.error = L"Minimum bitrate cannot exceed maximum bitrate.";
        return prepared;
    }

    const bool video = request.mode == BrowseMode::ExperimentalVideo;
    const size_t fetchLimit = video ? (std::min)(request.limit, 100U) : request.limit;
    if (!video && request.filters.compatibility != RadioCompatibilityPreset::Any) {
        std::vector<std::wstring> compatibleCodecs;
        switch (request.filters.compatibility) {
            case RadioCompatibilityPreset::Best:
                compatibleCodecs = {L"MP3"};
                break;
            case RadioCompatibilityPreset::Usually:
                compatibleCodecs = {L"AAC", L"AAC+"};
                break;
            case RadioCompatibilityPreset::DecoderRequired:
                compatibleCodecs = {L"OGG", L"OPUS", L"FLAC"};
                break;
            case RadioCompatibilityPreset::Unknown:
                compatibleCodecs = {L"UNKNOWN"};
                break;
            case RadioCompatibilityPreset::Any:
                break;
        }
        for (const std::wstring& codec : compatibleCodecs) {
            prepared.paths.push_back(BuildSearchPath(request, codec, fetchLimit));
        }
        return prepared;
    }
    if (!video || !request.filters.codec.empty()) {
        prepared.paths.push_back(BuildSearchPath(request, L"", fetchLimit));
        return prepared;
    }

    const std::vector<std::wstring> videoCodecs = {
        L"AAC,H.264", L"AAC+,H.264", L"MP4", L"FLV", L"UNKNOWN,H.264"};
    for (const std::wstring& codec : videoCodecs) {
        prepared.paths.push_back(BuildSearchPath(request, codec, fetchLimit));
    }
    return prepared;
}

BrowseFacets LoadBrowseFacets(const std::atomic_bool& cancelled) {
    BrowseFacets facets;
    constexpr size_t kInitialFacetPageSize = 80U;
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(com);
    const std::vector<Mirror> mirrors = DiscoverMirrors();
    struct FacetRequest {
        const wchar_t* path;
        const wchar_t* elementName;
        std::vector<BrowseFacet>* destination;
    };
    const FacetRequest facetRequests[] = {
        {L"/xml/tags?order=stationcount&reverse=true&hidebroken=true&limit=80",
         L"tag",
         &facets.genres},
        {L"/xml/countries?order=stationcount&reverse=true&hidebroken=true&limit=80",
         L"country",
         &facets.countries},
        {L"/xml/languages?order=stationcount&reverse=true&hidebroken=true&limit=80",
         L"language",
         &facets.languages},
    };
    std::wstring lastError;
    for (const FacetRequest& facetRequest : facetRequests) {
        if (cancelled.load()) {
            break;
        }
        FetchFacetList(
            mirrors,
            facetRequest.path,
            facetRequest.elementName,
            cancelled,
            *facetRequest.destination,
            facets.mirror,
            lastError);
    }
    facets.genresHaveMore = facets.genres.size() == kInitialFacetPageSize;
    facets.countriesHaveMore = facets.countries.size() == kInitialFacetPageSize;
    facets.languagesHaveMore = facets.languages.size() == kInitialFacetPageSize;
    if (!cancelled.load()) {
        FetchProviderStats(
            mirrors,
            cancelled,
            facets.stats,
            facets.mirror,
            lastError);
    }
    if (facets.genres.empty() || facets.countries.empty() || facets.languages.empty()) {
        facets.error = cancelled.load()
                           ? L"Cancelled."
                           : (lastError.empty()
                                  ? L"Some browse categories are temporarily unavailable."
                                  : lastError);
    }
    if (uninitialize) {
        CoUninitialize();
    }
    return facets;
}

BrowseFacetPage LoadBrowseFacetPage(
    const BrowseFacetPageRequest& request,
    const std::atomic_bool& cancelled) {
    BrowseFacetPage page;
    page.offset = request.offset;
    const PreparedFacetPagePath prepared = PrepareFacetPagePath(request);
    if (!prepared.error.empty()) {
        page.error = prepared.error;
        return page;
    }

    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(com);
    const std::vector<Mirror> mirrors = DiscoverMirrors();
    std::wstring error;
    FetchFacetList(
        mirrors,
        prepared.path,
        prepared.elementName.c_str(),
        cancelled,
        page.items,
        page.mirror,
        error);
    if (cancelled.load()) {
        page.error = L"Cancelled.";
    } else if (page.items.empty()) {
        page.error = error.empty() ? L"No more browse categories were returned." : error;
    } else {
        page.hasMore = page.items.size() == request.limit;
    }
    if (uninitialize) {
        CoUninitialize();
    }
    return page;
}

SearchResult Search(const SearchRequest& request, const std::atomic_bool& cancelled) {
    SearchResult result;
    result.offset = request.offset;
    const PreparedSearchPaths prepared = PrepareSearchPaths(request);
    if (!prepared.error.empty()) {
        result.error = prepared.error;
        return result;
    }
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(com);
    const std::vector<Mirror> mirrors = DiscoverMirrors();
    const bool video = request.mode == BrowseMode::ExperimentalVideo;
    std::unordered_set<std::wstring> seen;
    std::wstring lastError;
    bool providerPageMayContinue = false;

    for (const std::wstring& path : prepared.paths) {
        if (cancelled.load()) {
            break;
        }
        bool fetched = false;
        for (const Mirror& mirror : mirrors) {
            std::vector<unsigned char> body;
            std::wstring fetchError;
            if (!Fetch(mirror, path, cancelled, body, fetchError)) {
                lastError = std::move(fetchError);
                continue;
            }
            std::vector<Station> parsed;
            size_t providerRows = 0;
            std::wstring parseError;
            if (!ParseStations(body, video, parsed, providerRows, parseError)) {
                lastError = std::move(parseError);
                continue;
            }
            providerPageMayContinue =
                providerPageMayContinue || providerRows >= request.limit;
            for (Station& station : parsed) {
                if (seen.insert(station.identity.stationId).second) {
                    result.stations.push_back(std::move(station));
                }
            }
            result.mirror = mirror.host;
            fetched = true;
            break;
        }
        if (!fetched && result.stations.empty()) {
            result.error = lastError;
            break;
        }
    }

    if (cancelled.load()) {
        result.error = L"Cancelled.";
    } else if (result.stations.empty() && result.error.empty()) {
        result.error = video
                           ? L"No matching experimental video streams were returned."
                           : L"No matching radio stations were returned.";
    }
    const size_t visibleLimit = request.mode == BrowseMode::ExperimentalVideo
                                    ? (std::min)(static_cast<size_t>(request.limit), 100U)
                                    : static_cast<size_t>(request.limit);
    result.hasMore = providerPageMayContinue || result.stations.size() > visibleLimit;
    if (result.stations.size() > visibleLimit) {
        result.stations.resize(visibleLimit);
    }
    if (request.includeFacets && !cancelled.load()) {
        result.facets = LoadBrowseFacets(cancelled);
    }
    if (uninitialize) {
        CoUninitialize();
    }
    return result;
}

ResolveResult ResolveAndCountClick(
    const Station& station,
    const std::atomic_bool& cancelled) {
    ResolveResult result;
    if (!station.identity.IsValid() ||
        station.identity.providerId != kRadioBrowserProviderId) {
        result.error = L"This is not a valid Radio Browser station identity.";
        return result;
    }
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(com);
    const std::vector<Mirror> mirrors = DiscoverMirrors();
    const std::wstring path =
        L"/xml/url/" + PercentEncode(station.identity.stationId);
    std::wstring lastError;

    for (const Mirror& mirror : mirrors) {
        std::vector<unsigned char> body;
        if (!Fetch(mirror, path, cancelled, body, lastError)) {
            continue;
        }
        if (ParseResolvedUrl(body, result.url, lastError) &&
            IsAllowedPublicStreamUrl(result.url)) {
            break;
        }
        result.url.clear();
    }

    if (result.url.empty() && IsAllowedPublicStreamUrl(station.fallbackUrlAlias)) {
        result.url = station.fallbackUrlAlias;
        result.error = L"The click endpoint was unavailable; using the last checked stream URL.";
    } else if (result.url.empty()) {
        result.error = lastError.empty() ? L"No safe stream URL was available." : lastError;
    }
    if (uninitialize) {
        CoUninitialize();
    }
    return result;
}

const ProviderOperations& RadioBrowserProviderOperations() {
    static const ProviderOperations operations{
        Search,
        LoadBrowseFacets,
        LoadBrowseFacetPage,
        ResolveAndCountClick,
    };
    return operations;
}

}  // namespace live_stations
