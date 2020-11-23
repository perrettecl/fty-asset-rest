#pragma once

#include "asset-db.h"
#include <fty/expected.h>
#include <fty/translate.h>
#include <fty_common_db_asset.h>
#include <fty_common_db_exception.h>
#include <map>
#include <string>

namespace fty::asset {

class AssetManager
{
public:
    using AssetList = std::map<uint32_t, std::string>;
    using ImportList = std::map<size_t, Expected<uint32_t>>;

    static Expected<db::WebAssetElementExt> getItem(uint32_t id);
    static Expected<AssetList>              getItems(const std::string& typeName, const std::string& subtypeName);

    static Expected<db::AssetElement>                        deleteAsset(uint32_t id);
    static std::map<std::string, Expected<db::AssetElement>> deleteAsset(const std::map<uint32_t, std::string>& ids);
    static Expected<db::AssetElement>                        deleteAsset(const db::AssetElement& element);

    static Expected<uint32_t> createAsset(const std::string& json, const std::string& user, bool sendNotify = true);
    static Expected<ImportList> importCsv(const std::string& csv, const std::string& user, bool sendNotify = true);
    static Expected<std::string> exportCsv(const std::optional<db::AssetElement>& dc = std::nullopt);
private:
    static Expected<db::AssetElement> deleteDcRoomRowRack(const db::AssetElement& element);
    static Expected<db::AssetElement> deleteGroup(const db::AssetElement& element);
    static Expected<db::AssetElement> deleteDevice(const db::AssetElement& element);
};

} // namespace fty::asset
