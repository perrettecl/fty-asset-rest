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

    static Expected<db::WebAssetElementExt> getItem(uint32_t id);
    static Expected<AssetList>              getItems(const std::string& typeName, const std::string& subtypeName);
    static Expected<db::AssetElement>       deleteItem(uint32_t id);
    static std::map<std::string, Expected<db::AssetElement>> deleteItems(const std::map<uint32_t, std::string>& ids);

    static Expected<db::AssetElement> deleteAsset(const db::AssetElement& element);

private:
    static Expected<db::AssetElement> deleteDcRoomRowRack(const db::AssetElement& element);
    static Expected<db::AssetElement> deleteGroup(const db::AssetElement& element);
    static Expected<db::AssetElement> deleteDevice(const db::AssetElement& element);
};

} // namespace fty::asset
