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
        Station station;
        station.uuid = Attribute(owner.Get(), L"stationuuid");
        station.name = Attribute(owner.Get(), L"name");
        station.url = Attribute(owner.Get(), L"url_resolved");
        if (station.url.empty()) {
            station.url = Attribute(owner.Get(), L"url");
        }
        station.codec = Attribute(owner.Get(), L"codec");
        station.bitrate = Attribute(owner.Get(), L"bitrate");
        station.country = Attribute(owner.Get(), L"country");
        station.tags = Attribute(owner.Get(), L"tags");
        station.experimentalVideo = LooksLikeVideoCodec(station.codec);

        if (station.uuid.empty() || station.name.empty() || station.url.empty() ||
            station.experimentalVideo != wantVideo || !IsAllowedPublicStreamUrl(station.url)) {
            continue;
        }
        stations.push_back(std::move(station));
    }
    return true;
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

std::wstring SearchPath(
    const SearchRequest& request,
    const std::wstring& codec,
    size_t limit) {
    std::wstring path = L"/xml/stations/search?hidebroken=true&order=clickcount&reverse=true&limit=" +
                        std::to_wstring(limit);
    if (!request.text.empty()) {
        path += L"&name=" + PercentEncode(request.text) + L"&nameExact=false";
    }
    if (!codec.empty()) {
        path += L"&codec=" + PercentEncode(codec) + L"&codecExact=true";
    }
    return path;
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

SearchResult Search(const SearchRequest& request, const std::atomic_bool& cancelled) {
    SearchResult result;
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(com);
    const std::vector<Mirror> mirrors = DiscoverMirrors();
    const std::vector<std::wstring> videoCodecs = {
        L"AAC,H.264", L"AAC+,H.264", L"MP4", L"FLV", L"UNKNOWN,H.264"};
    const std::vector<std::wstring> codecs = request.video ? videoCodecs : std::vector<std::wstring>{L""};
    std::unordered_set<std::wstring> seen;
    std::wstring lastError;

    for (const std::wstring& codec : codecs) {
        if (cancelled.load()) {
            break;
        }
        const std::wstring path = SearchPath(request, codec, request.video ? 100U : 200U);
        bool fetched = false;
        for (const Mirror& mirror : mirrors) {
            std::vector<unsigned char> body;
            std::wstring fetchError;
            if (!Fetch(mirror, path, cancelled, body, fetchError)) {
                lastError = std::move(fetchError);
                continue;
            }
            std::vector<Station> parsed;
            std::wstring parseError;
            if (!ParseStations(body, request.video, parsed, parseError)) {
                lastError = std::move(parseError);
                continue;
            }
            for (Station& station : parsed) {
                if (seen.insert(station.uuid).second) {
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
        result.error = request.video
                           ? L"No matching experimental video streams were returned."
                           : L"No matching radio stations were returned.";
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
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(com);
    const std::vector<Mirror> mirrors = DiscoverMirrors();
    const std::wstring path = L"/xml/url/" + PercentEncode(station.uuid);
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

    if (result.url.empty() && IsAllowedPublicStreamUrl(station.url)) {
        result.url = station.url;
        result.error = L"The click endpoint was unavailable; using the last checked stream URL.";
    } else if (result.url.empty()) {
        result.error = lastError.empty() ? L"No safe stream URL was available." : lastError;
    }
    if (uninitialize) {
        CoUninitialize();
    }
    return result;
}

}  // namespace live_stations
