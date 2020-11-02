#include "asset-manager.h"
#include "db.h"
#include "error.h"
#include "json.h"
#include "logger.h"
#include <fty_asset_activator.h>
#include <fty_common_db.h>
#include <fty_common_mlm.h>
#include <fty_log.h>

static constexpr const char* ENV_OVERRIDE_LAST_DC_DELETION_CHECK = "FTY_OVERRIDE_LAST_DC_DELETION_CHECK";
static constexpr const char* AGENT_ASSET_ACTIVATOR               = "etn-licensing-credits";

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
    return deleteAsset(*asset);
}

Expected<db::AssetElement> AssetManager::deleteAsset(const db::AssetElement& asset)
{
    // disable deleting RC0
    if (asset.name == "rackcontroller-0") {
        logDebug("Prevented deleting RC-0");
        return unexpected("Prevented deleting RC-0");
    }

    // check if a logical_asset refer to the item we are trying to delete
    if (db::countKeytag("logical_asset", asset.name) > 0) {
        logWarn("a logical_asset (sensor) refers to it");
        return unexpected("a logical_asset (sensor) refers to it"_tr);
    }

    switch (asset.typeId) {
        case persist::asset_type::DATACENTER:
        case persist::asset_type::ROW:
        case persist::asset_type::ROOM:
        case persist::asset_type::RACK:
            return deleteDcRoomRowRack(asset);
        case persist::asset_type::GROUP:
            return deleteGroup(asset);
        case persist::asset_type::DEVICE: {
            return deleteDevice(asset);
        }
    }
    logError("unknown type");
    return unexpected("unknown type"_tr);
}

Expected<db::AssetElement> AssetManager::deleteDcRoomRowRack(const db::AssetElement& element)
{
    tnt::Connection  conn;
    tnt::Transaction trans(conn);

    static const std::string countSql = R"(
        SELECT
            COUNT(id_asset_element) as count
        FROM
            t_bios_asset_element
        WHERE
            id_type = (select id_asset_element_type from  t_bios_asset_element_type where name = "datacenter") AND
            id_asset_element != :element
    )";

    // Don't allow the deletion of the last datacenter (unless overriden)
    if (getenv(ENV_OVERRIDE_LAST_DC_DELETION_CHECK) == nullptr) {
        unsigned numDatacentersAfterDelete;
        try {
            numDatacentersAfterDelete = conn.selectRow(countSql, "element"_p = element.id).get<unsigned>("count");
        } catch (const std::exception& e) {
            return unexpected(e.what());
        }
        if (numDatacentersAfterDelete == 0) {
            return unexpected("will not allow last datacenter to be deleted"_tr);
        }
    }

    if (auto ret = db::deleteAssetElementFromAssetGroups(conn, element.id); !ret) {
        trans.rollback();
        logInfo("error occured during removing from groups");
        return unexpected("error occured during removing from groups"_tr);
    }

    if (auto ret = db::convertAssetToMonitor(element.id); !ret) {
        logError(ret.error());
        return unexpected("error during converting asset to monitor"_tr);
    }

    if (auto ret = db::deleteMonitorAssetRelationByA(conn, element.id); !ret) {
        trans.rollback();
        logError(ret.error());
        return unexpected("error occured during removing ma relation"_tr);
    }

    if (auto ret = db::deleteAssetElement(conn, element.id); !ret) {
        trans.rollback();
        logError(ret.error());
        return unexpected("error occured during removing element"_tr);
    }

    trans.commit();
    return element;
}

Expected<db::AssetElement> AssetManager::deleteGroup(const db::AssetElement& element)
{
    tnt::Connection  conn;
    tnt::Transaction trans(conn);

    if (auto ret = db::deleteAssetGroupLinks(conn, element.id); !ret) {
        trans.rollback();
        logError(ret.error());
        return unexpected("error occured during removing from groups"_tr);
    }

    if (auto ret = db::deleteAssetElement(conn, element.id); !ret) {
        trans.rollback();
        logError(ret.error());
        return unexpected("error occured during removing element"_tr);
    }

    trans.commit();
    return element;
}

Expected<db::AssetElement> AssetManager::deleteDevice(const db::AssetElement& element)
{
    tnt::Connection  conn;
    tnt::Transaction trans(conn);

    // make the device inactive first
    if (element.status == "active") {
        std::string asset_json = getJsonAsset(element.id);

        try {
            mlm::MlmSyncClient  client(AGENT_FTY_ASSET, AGENT_ASSET_ACTIVATOR);
            fty::AssetActivator activationAccessor(client);
            activationAccessor.deactivate(asset_json);
        } catch (const std::exception& e) {
            logError("Error during asset deactivation - {}", e.what());
            return unexpected(e.what());
        }
    }

    if (auto ret = db::deleteAssetElementFromAssetGroups(conn, element.id); !ret) {
        trans.rollback();
        logError(ret.error());
        return unexpected("error occured during removing from groups"_tr);
    }

    if (auto ret = db::deleteAssetLinksTo(conn, element.id); !ret) {
        trans.rollback();
        logError(ret.error());
        return unexpected("error occured during removing links"_tr);
    }

    if (auto ret = db::deleteMonitorAssetRelationByA(conn, element.id); !ret) {
        trans.rollback();
        logError(ret.error());
        return unexpected("error occured during removing ma relation"_tr);
    }

    if (auto ret = db::deleteAssetElement(conn, element.id); !ret) {
        trans.rollback();
        logError(ret.error());
        return unexpected("error occured during removing element"_tr);
    }

    trans.commit();
    return element;
}

struct Element;
using ElementPtr = std::shared_ptr<Element>;
struct Element : public db::WebAssetElement
{
    std::vector<ElementPtr> chidren;
    std::vector<ElementPtr> links;
    bool                    isDeleted = false;
};

// Collect children, filter by ids which should be deleted. If some ids will be in children, then we cannot delete asset
static void collectChildren(
    uint32_t parentId, std::vector<uint32_t>& children, const std::map<uint32_t, std::string>& ids)
{
    if (auto ret = db::selectAssetsByParent(parentId)) {
        for (uint32_t id : *ret) {
            if (ids.find(id) == ids.end()) {
                children.push_back(id);
            }
            collectChildren(id, children, ids);
        }
    }
}


// Collect links, filter by ids which should be deleted. If some ids will be in links, then we cannot delete asset
static void collectLinks(uint32_t elementId, std::vector<uint32_t>& links, const std::map<uint32_t, std::string>& ids)
{
    if (auto ret = db::selectAssetDeviceLinksSrc(elementId)) {
        for (uint32_t id : *ret) {
            if (ids.find(id) == ids.end()) {
                links.push_back(id);
            }
        }
    }
}

// Delete asset recursively
static Expected<void> deleteAssetRec(ElementPtr& el)
{
    if (el->isDeleted) {
        return {};
    }

    for (auto& it : el->links) {
        if (auto ret = deleteAssetRec(it); !ret) {
            return unexpected(ret.error());
        }
    }
    for (auto& it : el->chidren) {
        if (auto ret = deleteAssetRec(it); !ret) {
            return unexpected(ret.error());
        }
    }
    if (auto ret = AssetManager::deleteAsset(*el); !ret) {
        return unexpected(ret.error());
    }
    el->isDeleted = true;
    return {};
}

std::map<std::string, Expected<db::AssetElement>> AssetManager::deleteItems(const std::map<uint32_t, std::string>& ids)
{
    std::map<std::string, Expected<db::AssetElement>> result;
    std::vector<std::shared_ptr<Element>>             toDel;

    for (const auto& [id, name] : ids) {
        ElementPtr el = std::make_shared<Element>();

        if (auto ret = db::selectAssetElementWebById(id, *el)) {
            std::vector<uint32_t> allChildren;
            collectChildren(el->id, allChildren, ids);

            std::vector<uint32_t> links;
            collectLinks(el->id, links, ids);

            if (!allChildren.empty() || !links.empty()) {
                result.emplace(
                    name, unexpected("can't delete the asset because it has at least one child or asset is linked"_tr));
            } else {
                toDel.push_back(el);
            }
        } else {
            result.emplace(name, unexpected(ret.error()));
        }
    }

    auto find = [&](uint32_t id) {
        auto ret = std::find_if(toDel.begin(), toDel.end(), [&](const auto& ptr) {
            return ptr->id = id;
        });
        return ret != toDel.end() ? *ret : nullptr;
    };

    for (auto& it : toDel) {
        // Collect element children
        if (auto ret = db::selectAssetsByParent(it->id)) {
            for (uint32_t id : *ret) {
                if (auto el = find(id)) {
                    it->chidren.push_back(el);
                }
            }
        }
        // Collect element links
        if (auto ret = db::selectAssetDeviceLinksSrc(it->id)) {
            for (uint32_t id : *ret) {
                if (auto el = find(id)) {
                    it->links.push_back(el);
                }
            }
        }
    }

    // Delete all elements recursively
    for (auto& it : toDel) {
        if (auto ret = deleteAssetRec(it)) {
            result.emplace(it->name, *it);
        } else {
            result.emplace(it->name, unexpected(ret.error()));
        }
    }

    return result;
}

} // namespace fty::asset
