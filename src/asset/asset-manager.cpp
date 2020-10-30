#include "asset-manager.h"
#include "db.h"
#include "error.h"
#include "logger.h"
#include <fty_common_db.h>
#include <fty_log.h>

namespace fty::asset {

static std::vector<std::tuple<uint32_t, std::string, std::string, std::string>> getParents(uint32_t id)
{

    std::vector<std::tuple<uint32_t, std::string, std::string, std::string>> ret{};

    auto cb = [&ret](const tnt::Row& row) {
        // Dim: I keep this comment from original code for history :)

        // C++ is c r a z y!! Having static initializer in lambda function made
        // my life easier here, but I did not expected this will work!!
        static const std::vector<std::tuple<std::string, std::string, std::string, std::string>> NAMES = {
            std::make_tuple("id_parent1", "parent_name1", "id_type_parent1", "id_subtype_parent1"),
            std::make_tuple("id_parent2", "parent_name2", "id_type_parent2", "id_subtype_parent2"),
            std::make_tuple("id_parent3", "parent_name3", "id_type_parent3", "id_subtype_parent3"),
            std::make_tuple("id_parent4", "parent_name4", "id_type_parent4", "id_subtype_parent4"),
            std::make_tuple("id_parent5", "parent_name5", "id_type_parent5", "id_subtype_parent5"),
            std::make_tuple("id_parent6", "parent_name6", "id_type_parent6", "id_subtype_parent6"),
            std::make_tuple("id_parent7", "parent_name7", "id_type_parent7", "id_subtype_parent7"),
            std::make_tuple("id_parent8", "parent_name8", "id_type_parent8", "id_subtype_parent8"),
            std::make_tuple("id_parent9", "parent_name9", "id_type_parent9", "id_subtype_parent9"),
            std::make_tuple("id_parent10", "parent_name10", "id_type_parent10", "id_subtype_parent10"),
        };

        for (const auto& it : NAMES) {
            uint32_t    pid        = row.get<uint32_t>(std::get<0>(it));
            std::string name       = row.get(std::get<1>(it));
            uint16_t    id_type    = row.get<uint16_t>(std::get<2>(it));
            uint16_t    id_subtype = row.get<uint16_t>(std::get<3>(it));

            if (!name.empty()) {
                ret.push_back(std::make_tuple(
                    pid, name, persist::typeid_to_type(id_type), persist::subtypeid_to_subtype(id_subtype)));
            }
        }
    };

    int r = db::selectAssetElementSuperParent(id, cb);
    if (r == -1) {
        logError("select_asset_element_super_parent failed");
        throw std::runtime_error("DBAssets::select_asset_element_super_parent () failed.");
    }

    return ret;
}

Expected<db::WebAssetElementExt> AssetManager::getItem(uint32_t id)
{
    db::WebAssetElementExt el;
    try {
        if (auto ret = db::selectAssetElementWebById(id, el); !ret) {
            return unexpected(ret.error());
        }

        if (auto ret = db::selectExtAttributes(id)) {
            el.extAttributes = *ret;
        } else {
            return unexpected(ret.error());
        }

        ;
        if (auto ret = db::selectAssetElementGroups(id)) {
            el.groups = *ret;
        } else {
            return unexpected(ret.error());
        }

        if (el.typeId == persist::asset_type::DEVICE) {
            if (auto powers = db::selectAssetDeviceLinksTo(id, INPUT_POWER_CHAIN)) {
                el.powers = *powers;
            } else {
                return unexpected(powers.error());
            }
        }

        el.parents = getParents(id);

        return std::move(el);
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

Expected<AssetManager::AssetList> AssetManager::getItems(const std::string& typeName, const std::string& subtypeName)
{
    uint16_t subtypeId = 0;

    uint16_t typeId = persist::type_to_typeid(typeName);
    if (typeId == persist::asset_type::TUNKNOWN) {
        return unexpected("Expected datacenters,rooms,ros,racks,devices"_tr);
    }

    if (typeName == "device" && !subtypeName.empty()) {
        subtypeId = persist::subtype_to_subtypeid(subtypeName);
        if (subtypeId == persist::asset_subtype::SUNKNOWN) {
            return unexpected("Expected ups, epdu, pdu, genset, sts, server, feed"_tr);
        }
    }

    try {
        auto els = db::selectShortElements(typeId, subtypeId);
        if (!els) {
            return unexpected(els.error());
        }
        return *els;
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

Expected<db::AssetElement> AssetManager::deleteItem(uint32_t id)
{
    auto asset = db::selectAssetElementWebById(id);
    if (!asset) {
        return unexpected(asset.error());
    }

    // disable deleting RC0
    if (asset->name == "rackcontroller-0") {
        logDebug("Prevented deleting RC-0");
        return unexpected("Prevented deleting RC-0");
    }

    // check if a logical_asset refer to the item we are trying to delete
    if (db::countKeytag("logical_asset", asset->name) > 0) {
        logWarn("a logical_asset (sensor) refers to it");
        return unexpected("a logical_asset (sensor) refers to it"_tr);
    }

    switch (asset->typeId) {
        case persist::asset_type::DATACENTER:
        case persist::asset_type::ROW:
        case persist::asset_type::ROOM:
        case persist::asset_type::RACK:
            return persist::delete_dc_room_row_rack(conn, id);
        case persist::asset_type::GROUP:
            return persist::delete_group(conn, id);
        case persist::asset_type::DEVICE:
        {
            if (basic_info.item.status == "active")
            {
                //we need device JSON in order to delete active device
                std::string asset_json = getJsonAsset (NULL, id);
                return persist::delete_device(conn, id, asset_json);
            }
            return persist::delete_device(conn, id);
        }
    }
    logError("unknown type");
    return unexpected("unknown type"_tr);
}

} // namespace fty::asset
