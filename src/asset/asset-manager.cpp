#include "asset-manager.h"
#include <fty_common_db.h>
#include <fty_log.h>
#include "error.h"

namespace fty::asset {

static std::vector<std::tuple<uint32_t, std::string, std::string, std::string>> s_get_parents(
    tntdb::Connection& conn, uint32_t id)
{

    std::vector<std::tuple<uint32_t, std::string, std::string, std::string>> ret{};

    std::function<void(const tntdb::Row&)> cb = [&ret](const tntdb::Row& row) {
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
            uint32_t    pid        = row[std::get<0>(it)].getUnsigned32();
            std::string name       = row[std::get<1>(it)].getString();
            uint16_t    id_type    = uint16_t(row[std::get<2>(it)].getUnsigned());
            uint16_t    id_subtype = uint16_t(row[std::get<3>(it)].getUnsigned());

            if (!name.empty()) {
                ret.push_back(std::make_tuple(
                    pid, name, persist::typeid_to_type(id_type), persist::subtypeid_to_subtype(id_subtype)));
            }
        }
    };

    int r = DBAssets::select_asset_element_super_parent(conn, id, cb);
    if (r == -1) {
        log_error("select_asset_element_super_parent failed");
        throw std::runtime_error("DBAssets::select_asset_element_super_parent () failed.");
    }

    return ret;
}

template <typename T>
ErrCode createErr(const db_reply<T>& other)
{
    ErrCode ret(ErrorType(other.errtype), ErrorSubtype(other.errsubtype), other.msg);
    return ret;
}

Expected<db::WebAssetElementExt> AssetManager::getItem(uint32_t id)
{
    db::WebAssetElementExt el;
    try {
        tntdb::Connection conn = tntdb::connect(DBConn::url);
        log_debug("connection was successful");

        if (auto ret = db::selectAssetElementWebById(id, el); !ret) {
            return unexpected(ret.error());
        }
        log_debug("1/5 basic select is done");

        if (auto ret = db::selectExtAttributes(id)) {
            el.extAttributes = *ret;
        } else {
            return unexpected(ret.error());
        }
        log_debug("2/5 ext select is done");

        auto group_ret = DBAssets::select_asset_element_groups(conn, id);
        log_debug("3/5 groups select is done, but next one is only for devices");

        if (group_ret.status == 0) {
            return unexpected(createErr(group_ret));
        }
        log_debug("    3/5 no errors");
        /*ret.groups = group_ret.item;

        if (ret.basic.type_id == persist::asset_type::DEVICE) {
            auto powers = DBAssets::select_asset_device_links_to(conn, id, INPUT_POWER_CHAIN);
            log_debug("4/5 powers select is done");

            if (powers.status == 0) {
                return unexpected(createErr(powers));
            }
            log_debug("    4/5 no errors");
            ret.powers = powers.item;
        }

        // parents select
        log_debug("5/5 parents select");
        ret.parents = s_get_parents(conn, id);
        log_debug("     5/5 no errors");

        return std::move(ret);*/
        return el;
    } catch (const std::exception& e) {
        return unexpected(ErrCode{DB_ERR, DB_ERROR_INTERNAL, error(Errors::InternalError).format(e.what())});
    }
}

Expected<AssetManager::AssetList, ErrCode> AssetManager::getItems(
    const std::string& typeName, const std::string& subtypeName)
{
    LOG_START;
    log_debug("subtypename = '%s', typename = '%s'", subtypeName.c_str(), typeName.c_str());

    uint16_t  subtype_id = 0;

    uint16_t type_id = persist::type_to_typeid(typeName);
    if (type_id == persist::asset_type::TUNKNOWN) {
        return unexpected(ErrCode{DB_ERR, DB_ERROR_INTERNAL,
            error(Errors::BadParams).format("type", typeName, "datacenters,rooms,ros,racks,devices")});
    }

    if (typeName == "device" && !subtypeName.empty()) {
        subtype_id = persist::subtype_to_subtypeid(subtypeName);
        if (subtype_id == persist::asset_subtype::SUNKNOWN) {
            return unexpected(ErrCode{DB_ERR, DB_ERROR_INTERNAL,
                error(Errors::BadParams).format("subtype", subtypeName, "ups, epdu, pdu, genset, sts, server, feed")});
        }
    }

    log_debug("subtypeid = %" PRIi16 " typeid = %" PRIi16, subtype_id, type_id);

    try {
        tntdb::Connection conn = tntdb::connect(DBConn::url);
        auto els                    = DBAssets::select_short_elements(conn, type_id, subtype_id);
        if (els.status == 0) {
            return unexpected(createErr(els));
        }
        LOG_END;
        return els.item;
    } catch (const std::exception& e) {
        LOG_END_ABNORMAL(e);
        return unexpected(ErrCode{DB_ERR, DB_ERROR_INTERNAL, error(Errors::InternalError).format(e.what())});
    }
}

Expected<db::AssetElement, ErrCode> AssetManager::deleteItem(uint32_t id)
{
    return unexpected(ErrCode{DB_ERR, DB_ERROR_INTERNAL, "not-implemented"_tr});
}

} // namespace fty::asset
