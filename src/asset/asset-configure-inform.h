#pragma once

#include "asset-db.h"
#include <fty_common_asset_types.h>
#include <vector>

namespace fty::asset {

Expected<void> sendConfigure(
    const std::vector<std::pair<db::AssetElement, persist::asset_operation>>& rows, const std::string& agentName);

Expected<void> sendConfigure(const db::AssetElement& row, persist::asset_operation actionType, const std::string& agentName);

} // namespace fty::asset
