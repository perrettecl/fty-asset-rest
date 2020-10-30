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

    Expected<db::WebAssetElementExt> getItem(uint32_t id);
    Expected<AssetList>              getItems(const std::string& typeName, const std::string& subtypeName);
    Expected<db::AssetElement>       deleteItem(uint32_t id);
};

} // namespace fty::asset
