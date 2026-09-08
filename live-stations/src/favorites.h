#pragma once

#include "station_contracts.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <string_view>

namespace live_stations {

using FavoriteFolderId = std::uint64_t;
inline constexpr FavoriteFolderId kFavoritesRootFolderId = 0;

struct FavoriteFolder {
    FavoriteFolderId id = kFavoritesRootFolderId;
    FavoriteFolderId parentId = kFavoritesRootFolderId;
    std::wstring name;
};

struct FavoriteStation {
    StationIdentity identity;
    FavoriteFolderId folderId = kFavoritesRootFolderId;
    std::wstring displayName;
    // Directory URLs change. This is only the last known playable alias; callers
    // should resolve identity through its provider before falling back to it.
    std::wstring fallbackUrlAlias;
};

class FavoritesModel {
public:
    FavoritesModel();

    [[nodiscard]] bool AddFolder(
        FavoriteFolderId id,
        FavoriteFolderId parentId,
        std::wstring name);
    [[nodiscard]] bool MoveFolder(FavoriteFolderId id, FavoriteFolderId newParentId);
    [[nodiscard]] bool AddOrUpdateStation(FavoriteStation station);
    [[nodiscard]] bool MoveStation(
        const StationIdentity& identity,
        FavoriteFolderId newFolderId);
    [[nodiscard]] bool RemoveStation(const StationIdentity& identity);

    [[nodiscard]] const FavoriteFolder* FindFolder(FavoriteFolderId id) const noexcept;
    [[nodiscard]] const FavoriteStation* FindStation(
        const StationIdentity& identity) const noexcept;
    [[nodiscard]] const std::unordered_map<FavoriteFolderId, FavoriteFolder>& folders()
        const noexcept;
    [[nodiscard]] const std::unordered_map<
        StationIdentity,
        FavoriteStation,
        StationIdentityHash>& stations() const noexcept;

private:
    [[nodiscard]] bool WouldCreateFolderCycle(
        FavoriteFolderId id,
        FavoriteFolderId newParentId) const noexcept;

    std::unordered_map<FavoriteFolderId, FavoriteFolder> folders_;
    std::unordered_map<StationIdentity, FavoriteStation, StationIdentityHash> stations_;
};

struct FavoritesLoadResult {
    FavoritesModel model;
    FavoriteFolderId nextFolderId = 1;
    bool found = false;
    std::wstring error;
};

// Live Stations owns this small versioned file. It deliberately does not read
// or write Winamp Bookmarks' private data file.
[[nodiscard]] std::wstring DefaultFavoritesStoragePath();
[[nodiscard]] std::wstring SerializeFavorites(const FavoritesModel& model);
[[nodiscard]] bool DeserializeFavorites(
    std::wstring_view serialized,
    FavoritesModel* model,
    FavoriteFolderId* nextFolderId,
    std::wstring* error);
[[nodiscard]] FavoritesLoadResult LoadFavoritesFromFile(const std::wstring& path);
[[nodiscard]] FavoritesLoadResult LoadFavoritesFromDefaultPath();
[[nodiscard]] bool SaveFavoritesToFile(
    const FavoritesModel& model,
    const std::wstring& path,
    std::wstring* error);
[[nodiscard]] bool SaveFavoritesToDefaultPath(
    const FavoritesModel& model,
    std::wstring* error);

}  // namespace live_stations
