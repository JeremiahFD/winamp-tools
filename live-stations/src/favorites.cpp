#include "favorites.h"

#include <windows.h>
#include <shlobj_core.h>

#include <algorithm>
#include <array>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

namespace live_stations {

namespace {

constexpr wchar_t kStorageHeader[] = L"LIVE_STATIONS_FAVORITES_V1";
constexpr size_t kMaximumFavoritesFileBytes = 4U * 1024U * 1024U;
constexpr size_t kMaximumEncodedFieldLength = 256U * 1024U;

struct ParsedFolder {
    FavoriteFolderId id = kFavoritesRootFolderId;
    FavoriteFolderId parentId = kFavoritesRootFolderId;
    std::wstring name;
};

struct ParsedStation {
    FavoriteStation station;
};

void SetError(std::wstring* error, const wchar_t* text) {
    if (error != nullptr) {
        *error = text;
    }
}

bool ParseFolderId(std::wstring_view text, FavoriteFolderId* value) {
    if (value == nullptr || text.empty()) {
        return false;
    }
    FavoriteFolderId parsed = 0;
    for (const wchar_t character : text) {
        if (character < L'0' || character > L'9') {
            return false;
        }
        const FavoriteFolderId digit = static_cast<FavoriteFolderId>(character - L'0');
        if (parsed > ((std::numeric_limits<FavoriteFolderId>::max)() - digit) / 10U) {
            return false;
        }
        parsed = (parsed * 10U) + digit;
    }
    *value = parsed;
    return true;
}

std::vector<std::wstring_view> SplitTabs(std::wstring_view line) {
    std::vector<std::wstring_view> fields;
    size_t begin = 0;
    while (begin <= line.size()) {
        const size_t end = line.find(L'\t', begin);
        fields.push_back(line.substr(begin, end == std::wstring_view::npos ? line.size() - begin
                                                                            : end - begin));
        if (end == std::wstring_view::npos) {
            break;
        }
        begin = end + 1U;
    }
    return fields;
}

std::wstring EncodeBase64(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int byteCount = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (byteCount <= 0) {
        return {};
    }
    std::string bytes(static_cast<size_t>(byteCount), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            bytes.data(),
            byteCount,
            nullptr,
            nullptr) != byteCount) {
        return {};
    }
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::wstring encoded;
    encoded.reserve(((bytes.size() + 2U) / 3U) * 4U);
    for (size_t index = 0; index < bytes.size(); index += 3U) {
        const unsigned int first = static_cast<unsigned char>(bytes[index]);
        const unsigned int second = index + 1U < bytes.size()
                                        ? static_cast<unsigned char>(bytes[index + 1U])
                                        : 0U;
        const unsigned int third = index + 2U < bytes.size()
                                       ? static_cast<unsigned char>(bytes[index + 2U])
                                       : 0U;
        encoded.push_back(alphabet[(first >> 2U) & 0x3fU]);
        encoded.push_back(alphabet[((first & 0x03U) << 4U) | ((second >> 4U) & 0x0fU)]);
        encoded.push_back(index + 1U < bytes.size()
                              ? alphabet[((second & 0x0fU) << 2U) | ((third >> 6U) & 0x03U)]
                              : L'=');
        encoded.push_back(index + 2U < bytes.size() ? alphabet[third & 0x3fU] : L'=');
    }
    return encoded;
}

int DecodeBase64Character(wchar_t character) {
    if (character >= L'A' && character <= L'Z') {
        return character - L'A';
    }
    if (character >= L'a' && character <= L'z') {
        return character - L'a' + 26;
    }
    if (character >= L'0' && character <= L'9') {
        return character - L'0' + 52;
    }
    if (character == L'+') {
        return 62;
    }
    return character == L'/' ? 63 : -1;
}

bool DecodeBase64(std::wstring_view encoded, std::wstring* value) {
    if (value == nullptr || encoded.size() > kMaximumEncodedFieldLength ||
        (encoded.size() % 4U) != 0U) {
        return false;
    }
    if (encoded.empty()) {
        value->clear();
        return true;
    }
    std::string bytes;
    bytes.reserve((encoded.size() / 4U) * 3U);
    for (size_t index = 0; index < encoded.size(); index += 4U) {
        const int first = DecodeBase64Character(encoded[index]);
        const int second = DecodeBase64Character(encoded[index + 1U]);
        const bool thirdPadding = encoded[index + 2U] == L'=';
        const bool fourthPadding = encoded[index + 3U] == L'=';
        const int third = thirdPadding ? 0 : DecodeBase64Character(encoded[index + 2U]);
        const int fourth = fourthPadding ? 0 : DecodeBase64Character(encoded[index + 3U]);
        if (first < 0 || second < 0 || third < 0 || fourth < 0 ||
            (thirdPadding && !fourthPadding) ||
            ((thirdPadding || fourthPadding) && index + 4U != encoded.size())) {
            return false;
        }
        bytes.push_back(static_cast<char>((first << 2U) | (second >> 4U)));
        if (!thirdPadding) {
            bytes.push_back(static_cast<char>(((second & 0x0f) << 4U) | (third >> 2U)));
        }
        if (!fourthPadding) {
            bytes.push_back(static_cast<char>(((third & 0x03) << 6U) | fourth));
        }
    }
    if (bytes.empty()) {
        value->clear();
        return true;
    }
    const int wideCount = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (wideCount <= 0) {
        return false;
    }
    std::wstring decoded(static_cast<size_t>(wideCount), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            bytes.data(),
            static_cast<int>(bytes.size()),
            decoded.data(),
            wideCount) != wideCount) {
        return false;
    }
    *value = std::move(decoded);
    return true;
}

bool ReadFileAsAscii(const std::wstring& path, std::wstring* text, std::wstring* error) {
    if (text == nullptr) {
        SetError(error, L"favorites storage output was unavailable");
        return false;
    }
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD lastError = GetLastError();
        if (lastError == ERROR_FILE_NOT_FOUND || lastError == ERROR_PATH_NOT_FOUND) {
            return true;
        }
        SetError(error, L"could not read Live Stations favorites");
        return false;
    }
    LARGE_INTEGER size{};
    if (GetFileSizeEx(file, &size) == FALSE || size.QuadPart < 0 ||
        static_cast<unsigned long long>(size.QuadPart) > kMaximumFavoritesFileBytes) {
        CloseHandle(file);
        SetError(error, L"Live Stations favorites file is too large");
        return false;
    }
    std::string bytes(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    const DWORD expectedBytes = static_cast<DWORD>(bytes.size());
    const BOOL readOk = bytes.empty() || ReadFile(file, bytes.data(), expectedBytes, &read, nullptr);
    CloseHandle(file);
    if (readOk == FALSE || read != expectedBytes) {
        SetError(error, L"could not completely read Live Stations favorites");
        return false;
    }
    text->clear();
    text->reserve(bytes.size());
    for (const unsigned char byte : bytes) {
        if (byte > 0x7fU) {
            SetError(error, L"Live Stations favorites file is not valid storage text");
            return false;
        }
        text->push_back(static_cast<wchar_t>(byte));
    }
    return true;
}

bool EnsureParentDirectory(const std::wstring& path, std::wstring* error) {
    const size_t separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos || separator == 0U) {
        SetError(error, L"Live Stations favorites path has no parent directory");
        return false;
    }
    const std::wstring parent = path.substr(0, separator);
    const int createResult = SHCreateDirectoryExW(nullptr, parent.c_str(), nullptr);
    if (createResult != ERROR_SUCCESS && createResult != ERROR_FILE_EXISTS &&
        createResult != ERROR_ALREADY_EXISTS) {
        SetError(error, L"could not create Live Stations favorites directory");
        return false;
    }
    return true;
}

}  // namespace

FavoritesModel::FavoritesModel() {
    folders_.emplace(
        kFavoritesRootFolderId,
        FavoriteFolder{kFavoritesRootFolderId, kFavoritesRootFolderId, L"Favorites"});
}

bool FavoritesModel::AddFolder(
    FavoriteFolderId id,
    FavoriteFolderId parentId,
    std::wstring name) {
    if (id == kFavoritesRootFolderId || name.empty() || folders_.contains(id) ||
        !folders_.contains(parentId)) {
        return false;
    }
    folders_.emplace(id, FavoriteFolder{id, parentId, std::move(name)});
    return true;
}

bool FavoritesModel::MoveFolder(FavoriteFolderId id, FavoriteFolderId newParentId) {
    const auto folder = folders_.find(id);
    if (id == kFavoritesRootFolderId || folder == folders_.end() ||
        !folders_.contains(newParentId) || WouldCreateFolderCycle(id, newParentId)) {
        return false;
    }
    folder->second.parentId = newParentId;
    return true;
}

bool FavoritesModel::AddOrUpdateStation(FavoriteStation station) {
    if (!station.identity.IsValid() || station.displayName.empty() ||
        !folders_.contains(station.folderId)) {
        return false;
    }
    const StationIdentity identity = station.identity;
    stations_.insert_or_assign(identity, std::move(station));
    return true;
}

bool FavoritesModel::MoveStation(
    const StationIdentity& identity,
    FavoriteFolderId newFolderId) {
    const auto station = stations_.find(identity);
    if (station == stations_.end() || !folders_.contains(newFolderId)) {
        return false;
    }
    station->second.folderId = newFolderId;
    return true;
}

bool FavoritesModel::RemoveStation(const StationIdentity& identity) {
    return stations_.erase(identity) == 1U;
}

const FavoriteFolder* FavoritesModel::FindFolder(FavoriteFolderId id) const noexcept {
    const auto folder = folders_.find(id);
    return folder == folders_.end() ? nullptr : &folder->second;
}

const FavoriteStation* FavoritesModel::FindStation(
    const StationIdentity& identity) const noexcept {
    const auto station = stations_.find(identity);
    return station == stations_.end() ? nullptr : &station->second;
}

const std::unordered_map<FavoriteFolderId, FavoriteFolder>& FavoritesModel::folders()
    const noexcept {
    return folders_;
}

const std::unordered_map<StationIdentity, FavoriteStation, StationIdentityHash>&
FavoritesModel::stations() const noexcept {
    return stations_;
}

bool FavoritesModel::WouldCreateFolderCycle(
    FavoriteFolderId id,
    FavoriteFolderId newParentId) const noexcept {
    FavoriteFolderId current = newParentId;
    while (current != kFavoritesRootFolderId) {
        if (current == id) {
            return true;
        }
        const auto folder = folders_.find(current);
        if (folder == folders_.end() || folder->second.parentId == current) {
            return true;
        }
        current = folder->second.parentId;
    }
    return false;
}

std::wstring DefaultFavoritesStoragePath() {
    PWSTR roamingAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roamingAppData)) ||
        roamingAppData == nullptr) {
        return {};
    }
    std::wstring path(roamingAppData);
    CoTaskMemFree(roamingAppData);
    return path + L"\\Winamp\\Plugins\\ml\\live-stations-favorites.v1";
}

std::wstring SerializeFavorites(const FavoritesModel& model) {
    std::vector<FavoriteFolder> folders;
    folders.reserve(model.folders().size());
    for (const auto& [id, folder] : model.folders()) {
        if (id != kFavoritesRootFolderId) {
            folders.push_back(folder);
        }
    }
    std::sort(folders.begin(), folders.end(), [](const FavoriteFolder& left, const FavoriteFolder& right) {
        return left.id < right.id;
    });

    std::vector<FavoriteStation> stations;
    stations.reserve(model.stations().size());
    for (const auto& [identity, station] : model.stations()) {
        (void)identity;
        stations.push_back(station);
    }
    std::sort(stations.begin(), stations.end(), [](const FavoriteStation& left, const FavoriteStation& right) {
        if (left.identity.providerId != right.identity.providerId) {
            return left.identity.providerId < right.identity.providerId;
        }
        return left.identity.stationId < right.identity.stationId;
    });

    std::wostringstream output;
    output << kStorageHeader << L'\n';
    for (const FavoriteFolder& folder : folders) {
        output << L"F\t" << folder.id << L'\t' << folder.parentId << L'\t'
               << EncodeBase64(folder.name) << L'\n';
    }
    for (const FavoriteStation& station : stations) {
        output << L"S\t" << EncodeBase64(station.identity.providerId) << L'\t'
               << EncodeBase64(station.identity.stationId) << L'\t' << station.folderId << L'\t'
               << EncodeBase64(station.displayName) << L'\t'
               << EncodeBase64(station.fallbackUrlAlias) << L'\n';
    }
    return output.str();
}

bool DeserializeFavorites(
    std::wstring_view serialized,
    FavoritesModel* model,
    FavoriteFolderId* nextFolderId,
    std::wstring* error) {
    if (model == nullptr || nextFolderId == nullptr) {
        SetError(error, L"favorites storage destination was unavailable");
        return false;
    }
    std::wistringstream input{std::wstring(serialized)};
    std::wstring line;
    if (!std::getline(input, line) || line != kStorageHeader) {
        SetError(error, L"Live Stations favorites file has an unknown format");
        return false;
    }

    std::vector<ParsedFolder> parsedFolders;
    std::vector<ParsedStation> parsedStations;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        const std::vector<std::wstring_view> fields = SplitTabs(line);
        if (fields.empty()) {
            continue;
        }
        if (fields[0] == L"F" && fields.size() == 4U) {
            ParsedFolder folder;
            if (!ParseFolderId(fields[1], &folder.id) || !ParseFolderId(fields[2], &folder.parentId) ||
                !DecodeBase64(fields[3], &folder.name) || folder.id == kFavoritesRootFolderId ||
                folder.name.empty()) {
                SetError(error, L"Live Stations favorites contains an invalid folder");
                return false;
            }
            parsedFolders.push_back(std::move(folder));
            continue;
        }
        if (fields[0] == L"S" && fields.size() == 6U) {
            ParsedStation parsed;
            if (!DecodeBase64(fields[1], &parsed.station.identity.providerId) ||
                !DecodeBase64(fields[2], &parsed.station.identity.stationId) ||
                !ParseFolderId(fields[3], &parsed.station.folderId) ||
                !DecodeBase64(fields[4], &parsed.station.displayName) ||
                !DecodeBase64(fields[5], &parsed.station.fallbackUrlAlias) ||
                !parsed.station.identity.IsValid() || parsed.station.displayName.empty()) {
                SetError(error, L"Live Stations favorites contains an invalid station");
                return false;
            }
            parsedStations.push_back(std::move(parsed));
            continue;
        }
        SetError(error, L"Live Stations favorites contains an unknown record");
        return false;
    }

    FavoritesModel decoded;
    FavoriteFolderId maximumId = kFavoritesRootFolderId;
    while (!parsedFolders.empty()) {
        bool addedFolder = false;
        for (auto folder = parsedFolders.begin(); folder != parsedFolders.end();) {
            if (decoded.AddFolder(folder->id, folder->parentId, folder->name)) {
                maximumId = (std::max)(maximumId, folder->id);
                folder = parsedFolders.erase(folder);
                addedFolder = true;
            } else {
                ++folder;
            }
        }
        if (!addedFolder) {
            SetError(error, L"Live Stations favorites has an invalid folder hierarchy");
            return false;
        }
    }
    for (ParsedStation& parsed : parsedStations) {
        if (!decoded.AddOrUpdateStation(std::move(parsed.station))) {
            SetError(error, L"Live Stations favorites references an unknown folder");
            return false;
        }
    }
    *model = std::move(decoded);
    *nextFolderId = maximumId == (std::numeric_limits<FavoriteFolderId>::max)()
                        ? maximumId
                        : maximumId + 1U;
    return true;
}

FavoritesLoadResult LoadFavoritesFromFile(const std::wstring& path) {
    FavoritesLoadResult result;
    if (path.empty()) {
        result.error = L"Live Stations favorites storage path is unavailable";
        return result;
    }
    std::wstring text;
    if (!ReadFileAsAscii(path, &text, &result.error)) {
        return result;
    }
    if (text.empty()) {
        return result;
    }
    if (!DeserializeFavorites(text, &result.model, &result.nextFolderId, &result.error)) {
        return result;
    }
    result.found = true;
    return result;
}

FavoritesLoadResult LoadFavoritesFromDefaultPath() {
    return LoadFavoritesFromFile(DefaultFavoritesStoragePath());
}

bool SaveFavoritesToFile(
    const FavoritesModel& model,
    const std::wstring& path,
    std::wstring* error) {
    if (path.empty() || !EnsureParentDirectory(path, error)) {
        return false;
    }
    const std::wstring serialized = SerializeFavorites(model);
    std::string bytes;
    bytes.reserve(serialized.size());
    for (const wchar_t character : serialized) {
        if (character > 0x7f) {
            SetError(error, L"Live Stations favorites serialization was not storage-safe");
            return false;
        }
        bytes.push_back(static_cast<char>(character));
    }
    const std::wstring temporaryPath = path + L".tmp";
    HANDLE file = CreateFileW(
        temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        SetError(error, L"could not create Live Stations favorites update");
        return false;
    }
    DWORD written = 0;
    const DWORD expectedBytes = static_cast<DWORD>(bytes.size());
    const BOOL writeOk = bytes.empty() || WriteFile(file, bytes.data(), expectedBytes, &written, nullptr);
    const BOOL flushOk = writeOk != FALSE && FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    if (writeOk == FALSE || flushOk == FALSE || written != expectedBytes) {
        DeleteFileW(temporaryPath.c_str());
        SetError(error, L"could not save Live Stations favorites");
        return false;
    }
    if (MoveFileExW(
            temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE) {
        DeleteFileW(temporaryPath.c_str());
        SetError(error, L"could not replace Live Stations favorites");
        return false;
    }
    return true;
}

bool SaveFavoritesToDefaultPath(const FavoritesModel& model, std::wstring* error) {
    return SaveFavoritesToFile(model, DefaultFavoritesStoragePath(), error);
}

}  // namespace live_stations
