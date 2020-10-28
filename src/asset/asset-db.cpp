#include "asset-db.h"
#include "db.h"
#include "error.h"
#include "logger.h"
#include <fty/split.h>
#include <fty/translate.h>
#include <fty_common_asset_types.h>


namespace fty::asset::db {

Expected<int64_t> nameToAssetId(const std::string& assetName)
{
    static const std::string sql = R"(
        SELECT
            id_asset_element
        FROM
            t_bios_asset_element
        WHERE name = :asset_name
    )";

    try {
        tnt::Connection db;

        auto res = db.selectRow(sql, "asset_name"_a = assetName);

        return res.get<int64_t>("id_asset_element");
    } catch (const tntdb::NotFound&) {
        return unexpected(error(Errors::ElementNotFound).format(assetName));
    } catch (const std::exception& e) {
        return unexpected(error(Errors::ExceptionForElement).format(e.what(), assetName));
    }
}

Expected<std::pair<std::string, std::string>> idToNameExtName(uint32_t assetId)
{
    static std::string sql = R"(
        SELECT asset.name, ext.value
        FROM
            t_bios_asset_element AS asset
        LEFT JOIN
            t_bios_asset_ext_attributes AS ext
        ON
            ext.id_asset_element = asset.id_asset_element
        WHERE
            ext.keytag = "name" AND asset.id_asset_element = :asset_id
    )";

    try {
        tnt::Connection db;
        auto            res = db.selectRow(sql, "asset_id"_a = assetId);
        return std::make_pair(res.get<std::string>("name"), res.get<std::string>("value"));
    } catch (const tntdb::NotFound&) {
        return unexpected(error(Errors::ElementNotFound).format(assetId));
    } catch (const std::exception& e) {
        return unexpected(error(Errors::ExceptionForElement).format(e.what(), assetId));
    }
}

Expected<std::string> extNameToAssetName(const std::string& assetExtName)
{
    try {
        tntdb::Connection conn = tntdb::connectCached(DBConn::url);

        tntdb::Statement st = conn.prepareCached(R"(
            SELECT a.name
            FROM t_bios_asset_element AS a
            INNER JOIN
                t_bios_asset_ext_attributes AS e
            ON
                a.id_asset_element = e.id_asset_element
            WHERE
                keytag = "name" and value = :extname
        )");

        tntdb::Row row = st.set("extname", assetExtName).selectRow();
        logDebug("[extNameToAssetName]: were selected {} rows", 1);

        return row.getString("name");
    } catch (const tntdb::NotFound&) {
        return unexpected(error(Errors::ElementNotFound).format(assetExtName));
    } catch (const std::exception& e) {
        return unexpected(error(Errors::ExceptionForElement).format(e.what(), assetExtName));
    }
}

Expected<int64_t> extNameToAssetId(const std::string& assetExtName)
{
    try {
        tntdb::Connection conn = tntdb::connectCached(DBConn::url);

        tntdb::Statement st = conn.prepareCached(R"(
            SELECT
                a.id_asset_element
            FROM
                t_bios_asset_element AS a
            INNER JOIN
                t_bios_asset_ext_attributes AS e
            ON
                a.id_asset_element = e.id_asset_element
            WHERE
                keytag = 'name' and value = :extname
        )");

        tntdb::Row row = st.set("extname", assetExtName).selectRow();
        logDebug("[extNameToAssetId]: were selected {} rows", 1);

        return row.getInt64("id_asset_element");
    } catch (const tntdb::NotFound&) {
        return unexpected(error(Errors::ElementNotFound).format(assetExtName));
    } catch (const std::exception& e) {
        return unexpected(error(Errors::ExceptionForElement).format(e.what(), assetExtName));
    }
}

Expected<AssetElement> selectAssetElementByName(const std::string& elementName)
{
    if (!persist::is_ok_name(elementName.c_str())) {
        return unexpected("name is not valid"_tr);
    }

    try {
        tntdb::Connection conn = tntdb::connectCached(DBConn::url);

        tntdb::Row row;
        try {
            tntdb::Statement st = conn.prepare(R"(
                SELECT
                    v.name, v.id_parent, v.status, v.priority, v.id, v.id_type
                FROM
                    v_bios_asset_element v
                WHERE v.name = :name
            )");

            row = st.set("name", elementName).selectRow();
        } catch (const tntdb::NotFound&) {
            // maybe we got extname instead of name
            tntdb::Statement st = conn.prepare(R"(
                SELECT v.name, v.id_parent, v.status, v.priority, v.id, v.id_type
                FROM
                    v_bios_asset_element AS v
                LEFT JOIN
                    v_bios_asset_ext_attributes AS ext
                ON
                    ext.id_asset_element = v.id
                WHERE
                    ext.keytag = 'name' AND ext.value = :name
            )");

            row = st.set("name", elementName).selectRow();
        }


        AssetElement el;
        el.name     = row.getString("name");
        el.parentId = row.getUnsigned32("id_parent");
        el.status   = row.getString("status");
        el.priority = uint16_t(row.getUnsigned32("priority"));
        el.id       = row.getUnsigned32("id");
        el.typeId   = uint16_t(row.getUnsigned32("id_type"));

        return std::move(el);
    } catch (const tntdb::NotFound&) {
        return unexpected("element with specified name was not found"_tr);
    } catch (const std::exception& e) {
        return unexpected(error(Errors::ExceptionForElement).format(e.what(), elementName));
    }
}

Expected<WebAssetElement> selectAssetElementWebById(uint32_t elementId)
{
    WebAssetElement el;

    if (auto ret = selectAssetElementWebById(elementId, el)) {
        return el;
    } else {
        return unexpected(ret.error());
    }
}

Expected<void> selectAssetElementWebById(uint32_t elementId, WebAssetElement& asset)
{
    log_debug("element_id = %" PRIi32, elementId);

    try {
        tntdb::Connection conn = tntdb::connectCached(DBConn::url);

        // Can return more than one row.
        tntdb::Statement st = conn.prepareCached(
            " SELECT"
            "   v.id, v.name, v.id_type, v.type_name,"
            "   v.subtype_id, v.subtype_name, v.id_parent,"
            "   v.id_parent_type, v.status,"
            "   v.priority, v.asset_tag, v.parent_name "
            " FROM"
            "   v_web_element v"
            " WHERE :id = v.id");

        tntdb::Row row = st.set("id", elementId).selectRow();
        log_debug("[v_web_element]: were selected %" PRIu32 " rows", 1);

        row[0].get(asset.id);
        row[1].get(asset.name);
        row[2].get(asset.typeId);
        row[3].get(asset.typeName);
        row[4].get(asset.subtypeId);
        row[5].get(asset.subtypeName);
        row[6].get(asset.parentId);
        row[7].get(asset.parentTypeId);
        row[8].get(asset.status);
        row[9].get(asset.priority);
        row[10].get(asset.assetTag);
        row[11].get(asset.parentName);

        return {};
    } catch (const tntdb::NotFound&) {
        return unexpected("element with specified id was not found"_tr);
    } catch (const std::exception& e) {
        return unexpected(error(Errors::ExceptionForElement).format(e.what(), elementId));
    }
}

Expected<Attributes> selectExtAttributes(uint32_t element_id)
{
    Attributes attrs;

    try {
        tntdb::Connection conn = tntdb::connectCached(DBConn::url);

        // Can return more than one row
        tntdb::Statement st_extattr = conn.prepareCached(
            " SELECT"
            "   v.keytag, v.value, v.read_only"
            " FROM"
            "   v_bios_asset_ext_attributes v"
            " WHERE v.id_asset_element = :idelement");

        tntdb::Result result = st_extattr.set("idelement", element_id).select();

        log_debug("[v_bios_asset_ext_attributes]: were selected %" PRIu32 " rows", result.size());

        // Go through the selected extra attributes
        for (auto& row : result) {
            std::string keytag;
            row[0].get(keytag);

            ExtAttrValue val;
            row[1].get(val.value);
            row[2].get(val.readOnly);

            attrs.emplace(keytag, val);
        }
        return attrs;
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

/*db_reply<std::map<uint32_t, std::string>> select_asset_element_groups(tntdb::Connection& conn, uint32_t element_id)
{
    LOG_START;
    log_debug("element_id = %" PRIi32, element_id);

    std::map<uint32_t, std::string>           item{};
    db_reply<std::map<uint32_t, std::string>> ret = db_reply_new(item);

    try {
        // Get information about the groups element belongs to
        // Can return more than one row
        tntdb::Statement st_gr = conn.prepareCached(
            " SELECT "
            "   v1.id_asset_group, v.name "
            " FROM "
            "   v_bios_asset_group_relation v1, "
            "   v_bios_asset_element v "
            " WHERE "
            "   v1.id_asset_element = :idelement AND "
            "   v.id = v1.id_asset_group ");

        tntdb::Result result = st_gr.set("idelement", element_id).select();

        log_debug("[v_bios_asset_group_relation]: were selected %" PRIu32 " rows", result.size());
        // Go through the selected groups
        for (auto& row : result) {
            // group_id, required
            uint32_t group_id = 0;
            row["id_asset_group"].get(group_id);

            std::string group_name;
            row["name"].get(group_name);
            ret.item.emplace(group_id, group_name);
        }
        ret.status = 1;
        LOG_END;
        return ret;
    } catch (const std::exception& e) {
        ret.status     = 0;
        ret.errtype    = DB_ERR;
        ret.errsubtype = DB_ERROR_INTERNAL;
        ret.msg        = e.what();
        ret.item.clear();
        LOG_END_ABNORMAL(e);
        return ret;
    }
}*/

Expected<uint> updateAssetElement(tntdb::Connection& conn, uint32_t elementId, const std::string& elementName,
    uint32_t parentId, const std::string& status, uint16_t priority, const std::string& assetTag)
{
    LOG_START;
    log_debug("  element_id = %" PRIi32, elementId);
    log_debug("  element_name = '%s'", elementName.c_str());
    log_debug("  parent_id = %" PRIu32, parentId);
    log_debug("  status = '%s'", status.c_str());
    log_debug("  priority = %" PRIu16, priority);
    log_debug("  asset_tag = '%s'", assetTag.c_str());

    // if parent id == 0 ->  it means that there is no parent and value
    // should be updated to NULL
    try {
        tntdb::Statement st = conn.prepareCached(
            " UPDATE"
            "   t_bios_asset_element"
            " SET"
            "   asset_tag = :asset_tag,"
            "   id_parent = :id_parent,"
            "   status = :status,"
            "   priority = :priority"
            " WHERE id_asset_element = :id");

        st = st.set("id", elementId).set("status", status).set("priority", priority).set("asset_tag", assetTag);

        uint affectedRows;
        if (parentId != 0) {
            affectedRows = st.set("id_parent", parentId).execute();
        } else {
            affectedRows = st.setNull("id_parent").execute();
        }
        log_debug("[t_asset_element]: updated %" PRIu32 " rows", affectedRows);
        return affectedRows;
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

Expected<uint> deleteAssetExtAttributesWithRo(tntdb::Connection& conn, uint32_t assetElementId, bool readOnly)
{
    LOG_START;
    log_debug("read_only = %i, asset_element_id = %" PRIu32, readOnly, assetElementId);

    try {
        tntdb::Statement st = conn.prepareCached(
            " DELETE FROM"
            "   t_bios_asset_ext_attributes"
            " WHERE"
            "   id_asset_element = :element AND "
            "   read_only = :ro ");

        uint affectedRows = st.set("element", assetElementId).set("ro", readOnly).execute();
        log_debug("[t_bios_asset_ext_attributes]: was deleted %" PRIu64 " rows", affectedRows);
        return affectedRows;
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

Expected<uint> insertIntoAssetExtAttributes(
    tntdb::Connection& conn, uint32_t elementId, const std::map<std::string, std::string>& attributes, bool readOnly)
{
    if (attributes.empty()) {
        return unexpected("no attributes to insert"_tr);
    }

    try {
        std::stringstream ss;
        ss << "INSERT INTO ";
        ss << "t_bios_asset_ext_attributes (keytag, value, id_asset_element, read_only) ";
        ss << "VALUES ";
        for (size_t i = 0; i < attributes.size(); ++i) {
            ss << (i > 0 ? ", " : "") << fmt::format("(:keytag_{0}, :value_{0}, :elId_{0}, :ro_{0})", i);
        }
        ss << " ON DUPLICATE KEY ";
        ss << "   UPDATE ";
        ss << "       id_asset_ext_attribute = LAST_INSERT_ID(id_asset_ext_attribute) ";

        std::string sql = ss.str();
        log_debug("sql: '%s'", sql.c_str());
        auto st = conn.prepare(sql);

        int count = 0;
        for (const auto& [key, value] : attributes) {
            st.set(fmt::format("keytag_{}", count), key);
            st.set(fmt::format("value_{}", count), value);
            st.set(fmt::format("elId_{}", count), elementId);
            st.set(fmt::format("ro_{}", count), readOnly);
            ++count;
        }

        uint i = st.execute();
        log_debug("%zu attributes written", i);
        return i;
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

Expected<uint> deleteAssetElementFromAssetGroups(tntdb::Connection& conn, uint32_t assetElementId)
{
    log_debug("asset_element_id = %" PRIu32, assetElementId);

    try {
        tntdb::Statement st = conn.prepareCached(
            " DELETE FROM"
            "   t_bios_asset_group_relation"
            " WHERE"
            "   id_asset_element = :element");

        uint affectedRows = st.set("element", assetElementId).execute();
        log_debug("[t_bios_asset_group_relation]: was deleted %" PRIu64 " rows", affectedRows);
        return affectedRows;
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

Expected<uint> insertElementIntoGroups(
    tntdb::Connection& conn, std::set<uint32_t> const& groups, uint32_t assetElementId)
{
    LOG_START;
    log_debug("  asset_element_id = %" PRIu32, assetElementId);

    // input parameters control
    if (assetElementId == 0) {
        auto msg = fmt::format("{}, {}", "ignore insert"_tr, "0 value of asset_element_id is not allowed"_tr);
        log_error(msg.c_str());
        return unexpected(msg);
    }

    if (groups.empty()) {
        log_debug("nothing to insert");
        return 0;
    }

    log_debug("input parameters are correct");

    try {
        tntdb::Statement st = conn.prepare(
            " INSERT INTO"
            "   t_bios_asset_group_relation"
            "   (id_asset_group, id_asset_element)"
            " VALUES " +
            implode(groups, ", ", [&](uint32_t grp) -> std::string {
                return "(" + std::to_string(grp) + "," + std::to_string(assetElementId) + ")";
            }));

        uint affectedRows = st.execute();
        log_debug("[t_bios_asset_group_relation]: was inserted %" PRIu64 " rows", affectedRows);

        if (affectedRows == groups.size()) {
            return affectedRows;
        } else {
            auto msg = "not all links were inserted"_tr;
            log_error("%s", msg.toString().c_str());
            return unexpected(msg);
        }
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

Expected<uint> deleteAssetLinksTo(tntdb::Connection& conn, uint32_t assetDeviceId)
{
    log_debug("  asset_device_id = %" PRIu32, assetDeviceId);

    try {
        tntdb::Statement st = conn.prepareCached(
            " DELETE FROM"
            "   t_bios_asset_link"
            " WHERE"
            "   id_asset_device_dest = :dest");

        uint affectedRows = st.set("dest", assetDeviceId).execute();
        log_debug("[t_bios_asset_link]: was deleted %" PRIu64 " rows", affectedRows);
        return affectedRows;
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

Expected<uint> insertIntoAssetLinks(tntdb::Connection& conn, const std::vector<AssetLink>& links)
{
    // input parameters control
    if (links.empty()) {
        log_debug("nothing to insert");
        return 0;
    }
    log_debug("input parameters are correct");

    uint affected_rows = 0;
    for (auto& one_link : links) {
        auto ret =
            insertIntoAssetLink(conn, one_link.src, one_link.dest, one_link.type, one_link.srcOut, one_link.destIn);

        if (ret) {
            affected_rows++;
        }
    }

    if (affected_rows == links.size()) {
        log_debug("all links were inserted successfully");
        return affected_rows;
    } else {
        log_error("%s", "not all links were inserted");
        return unexpected("not all links were inserted");
    }
}

Expected<int64_t> insertIntoAssetLink(tntdb::Connection& conn, uint32_t assetElementSrcId, uint32_t assetElementDestId,
    uint16_t linkTypeId, const std::string& srcOut, const std::string& destIn)
{
    // input parameters control
    if (assetElementDestId == 0) {
        log_error("ignore insert: %s", "destination device is not specified");
        return unexpected("destination device is not specified");
    }

    if (assetElementSrcId == 0) {
        log_error("ignore insert: %s", "source device is not specified");
        return unexpected("source device is not specified");
    }

    if (!persist::is_ok_link_type(uint8_t(linkTypeId))) {
        log_error("ignore insert: %s", "wrong link type");
        return unexpected("wrong link type");
    }

    // src_out and dest_in can take any value from available range
    log_debug("input parameters are correct");

    try {
        tntdb::Statement st = conn.prepareCached(
            " INSERT INTO"
            "   t_bios_asset_link"
            "   (id_asset_device_src, id_asset_device_dest,"
            "        id_asset_link_type, src_out, dest_in)"
            " SELECT"
            "   v1.id_asset_element, v2.id_asset_element, :linktype,"
            "   :out, :in"
            " FROM"
            "   v_bios_asset_device v1," // src
            "   v_bios_asset_device v2"  // dvc
            " WHERE"
            "   v1.id_asset_element = :src AND"
            "   v2.id_asset_element = :dest AND"
            "   NOT EXISTS"
            "     ("
            "           SELECT"
            "             id_link"
            "           FROM"
            "             t_bios_asset_link v3"
            "           WHERE"
            "               v3.id_asset_device_src = v1.id_asset_element AND"
            "               v3.id_asset_device_dest = v2.id_asset_element AND"
            "               ( ((v3.src_out = :out) AND (v3.dest_in = :in)) ) AND"
            "               v3.id_asset_device_dest = v2.id_asset_element"
            "    )");

        if (srcOut.empty()) {
            st = st.setNull("out");
        } else {
            st = st.set("out", srcOut);
        }

        if (destIn.empty()) {
            st = st.setNull("in");
        } else {
            st = st.set("in", destIn);
        }

        uint affected_rows =
            st.set("src", assetElementSrcId).set("dest", assetElementDestId).set("linktype", linkTypeId).execute();

        int64_t rowid = conn.lastInsertId();
        log_debug("[t_bios_asset_link]: was inserted %" PRIu64 " rows", affected_rows);
        return rowid;
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

Expected<int64_t> insertIntoAssetElement(tntdb::Connection& conn, const std::string& elementName,
    uint16_t elementTypeId, uint32_t parentId, const std::string& status, uint16_t priority, uint16_t subtypeId,
    const std::string& assetTag, bool update)
{
    log_debug("element_name = '%s'", elementName.c_str());
    if (subtypeId == 0) {
        subtypeId = persist::asset_subtype::N_A;
    }

    // input parameters control
    if (!persist::is_ok_name(elementName.c_str())) {
        auto msg = "wrong element name"_tr;
        log_error("end: %s, %s", "ignore insert", msg.toString().c_str());
        return unexpected(msg);
    }

    if (!persist::is_ok_element_type(elementTypeId)) {
        auto msg = "0 value of element_type_id is not allowed"_tr;
        log_error("end: %s, %s", "ignore insert", msg.toString().c_str());
        return unexpected(msg);
    }

    // ASSUMPTION: all datacenters are unlocated elements
    if (elementTypeId == persist::asset_type::DATACENTER && parentId != 0) {
        return unexpected("cannot inset datacenter"_tr);
    }

    log_debug("input parameters are correct");

    try {
        // this concat with last_insert_id may have raise condition issue but hopefully is not important
        tntdb::Statement statement;
        if (update) {
            statement = conn.prepareCached(
                " INSERT INTO t_bios_asset_element "
                " (name, id_type, id_subtype, id_parent, status, priority, asset_tag) "
                " VALUES "
                " (:name, :id_type, :id_subtype, :id_parent, :status, :priority, :asset_tag) "
                " ON DUPLICATE KEY UPDATE name = :name ");
        } else {
            // @ is prohibited in name => name-@@-342 is unique
            statement = conn.prepareCached(
                " INSERT INTO t_bios_asset_element "
                " (name, id_type, id_subtype, id_parent, status, priority, asset_tag) "
                " VALUES "
                " (concat (:name, '-@@-', " +
                std::to_string(rand()) + "), :id_type, :id_subtype, :id_parent, :status, :priority, :asset_tag) ");
        }
        uint affected_rows;
        if (parentId == 0) {
            affected_rows = statement.set("name", elementName)
                                .set("id_type", elementTypeId)
                                .set("id_subtype", subtypeId)
                                .setNull("id_parent")
                                .set("status", status)
                                .set("priority", priority)
                                .set("asset_tag", assetTag)
                                .execute();
        } else {
            affected_rows = statement.set("name", elementName)
                                .set("id_type", elementTypeId)
                                .set("id_subtype", subtypeId)
                                .set("id_parent", parentId)
                                .set("status", status)
                                .set("priority", priority)
                                .set("asset_tag", assetTag)
                                .execute();
        }

        int64_t rowid = conn.lastInsertId();
        log_debug("[t_bios_asset_element]: was inserted %" PRIu64 " rows", affected_rows);
        if (!update) {
            // it is insert, fix the name
            statement = conn.prepareCached(
                " UPDATE t_bios_asset_element "
                "  set name = concat(:name, '-', :id) "
                " WHERE id_asset_element = :id ");
            statement.set("name", elementName).set("id", rowid).execute();
        }

        if (affected_rows == 0) {
            return unexpected("Something going wrong");
        }

        return rowid;
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

Expected<uint16_t> insertIntoMonitorDevice(
    tntdb::Connection& conn, uint16_t deviceTypeId, const std::string& deviceName)
{
    try {
        tntdb::Statement st = conn.prepareCached(
            " INSERT INTO"
            "   t_bios_discovered_device (name, id_device_type)"
            " VALUES (:name, :iddevicetype)"
            " ON DUPLICATE KEY"
            "   UPDATE"
            "       id_discovered_device = LAST_INSERT_ID(id_discovered_device)");

        // Insert one row or nothing
        uint affected_rows = st.set("name", deviceName).set("iddevicetype", deviceTypeId).execute();
        log_debug("[t_bios_discovered_device]: was inserted %" PRIu64 " rows", affected_rows);
        return uint16_t(conn.lastInsertId());
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

// TODO: inserted data are probably unused, check and remove
Expected<int64_t> insertIntoMonitorAssetRelation(tntdb::Connection& conn, uint16_t monitorId, uint32_t elementId)
{
    if (elementId == 0) {
        auto msg = "0 value of element_id is not allowed"_tr;
        log_error("end: %s, %s", "ignore insert", msg.toString().c_str());
        return unexpected(msg);
    }

    if (monitorId == 0) {
        auto msg = "0 value of monitor_id is not allowed"_tr;
        log_error("end: %s, %s", "ignore insert", msg.toString().c_str());
        return unexpected(msg);
    }

    log_debug("input parameters are correct");

    try {
        tntdb::Statement st = conn.prepareCached(
            " INSERT INTO"
            "   t_bios_monitor_asset_relation"
            "   (id_discovered_device, id_asset_element)"
            " VALUES"
            "   (:monitor, :asset)");

        uint affected_rows = st.set("monitor", monitorId).set("asset", elementId).execute();
        log_debug("[t_bios_monitor_asset_relation]: was inserted %" PRIu64 " rows", affected_rows);

        return conn.lastInsertId();
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

Expected<uint16_t> selectMonitorDeviceTypeId(tntdb::Connection& conn, const std::string& deviceTypeName)
{
    try {
        tntdb::Statement st = conn.prepareCached(
            " SELECT"
            "   v.id"
            " FROM"
            "   v_bios_device_type v"
            " WHERE v.name = :name");

        tntdb::Value val = st.set("name", deviceTypeName).selectValue();
        log_debug("[t_bios_monitor_device]: was selected 1 rows");

        return uint16_t(val.getUnsigned32());
    } catch (const tntdb::NotFound& e) {
        log_info("end: discovered device type was not found with '%s'", e.what());
        return unexpected("not found");
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

Expected<void> selectAssetElementSuperParent(
    tntdb::Connection& conn, uint32_t id, std::function<void(const tntdb::Row&)>& cb)
{
    try {
        tntdb::Statement st = conn.prepareCached(
            " SELECT "
            "   v.id_asset_element as id, "
            "   v.id_parent1 as id_parent1, "
            "   v.id_parent2 as id_parent2, "
            "   v.id_parent3 as id_parent3, "
            "   v.id_parent4 as id_parent4, "
            "   v.id_parent5 as id_parent5, "
            "   v.id_parent6 as id_parent6, "
            "   v.id_parent7 as id_parent7, "
            "   v.id_parent8 as id_parent8, "
            "   v.id_parent9 as id_parent9, "
            "   v.id_parent10 as id_parent10, "
            "   v.name_parent1 as parent_name1, "
            "   v.name_parent2 as parent_name2, "
            "   v.name_parent3 as parent_name3, "
            "   v.name_parent4 as parent_name4, "
            "   v.name_parent5 as parent_name5, "
            "   v.name_parent6 as parent_name6, "
            "   v.name_parent7 as parent_name7, "
            "   v.name_parent8 as parent_name8, "
            "   v.name_parent9 as parent_name9, "
            "   v.name_parent10 as parent_name10, "
            "   v.id_type_parent1 as id_type_parent1, "
            "   v.id_type_parent2 as id_type_parent2, "
            "   v.id_type_parent3 as id_type_parent3, "
            "   v.id_type_parent4 as id_type_parent4, "
            "   v.id_type_parent5 as id_type_parent5, "
            "   v.id_type_parent6 as id_type_parent6, "
            "   v.id_type_parent7 as id_type_parent7, "
            "   v.id_type_parent8 as id_type_parent8, "
            "   v.id_type_parent9 as id_type_parent9, "
            "   v.id_type_parent10 as id_type_parent10, "
            "   v.id_subtype_parent1 as id_subtype_parent1, "
            "   v.id_subtype_parent2 as id_subtype_parent2, "
            "   v.id_subtype_parent3 as id_subtype_parent3, "
            "   v.id_subtype_parent4 as id_subtype_parent4, "
            "   v.id_subtype_parent5 as id_subtype_parent5, "
            "   v.id_subtype_parent6 as id_subtype_parent6, "
            "   v.id_subtype_parent7 as id_subtype_parent7, "
            "   v.id_subtype_parent8 as id_subtype_parent8, "
            "   v.id_subtype_parent9 as id_subtype_parent9, "
            "   v.id_subtype_parent10 as id_subtype_parent10, "
            "   v.name as name, "
            "   v.type_name as type_name, "
            "   v.id_asset_device_type as device_type, "
            "   v.status as status, "
            "   v.asset_tag as asset_tag, "
            "   v.priority as priority, "
            "   v.id_type as id_type "
            " FROM v_bios_asset_element_super_parent AS v "
            " WHERE "
            "   v.id_asset_element = :id ");

        tntdb::Result res = st.set("id", id).select();

        for (const auto& r : res) {
            cb(r);
        }
        return {};
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

Expected<void> selectAssetsByContainer(tntdb::Connection& conn, uint32_t elementId, std::vector<uint16_t> types,
    std::vector<uint16_t> subtypes, const std::string& without, const std::string& status,
    std::function<void(const tntdb::Row&)>&& cb)
{
    try {
        std::string select =
            " SELECT "
            "   v.name, "
            "   v.id_asset_element as asset_id, "
            "   v.id_asset_device_type as subtype_id, "
            "   v.type_name as subtype_name, "
            "   v.id_type as type_id "
            " FROM "
            "   v_bios_asset_element_super_parent AS v"
            " WHERE "
            "   :containerid in (v.id_parent1, v.id_parent2, v.id_parent3, "
            "                    v.id_parent4, v.id_parent5, v.id_parent6, "
            "                    v.id_parent7, v.id_parent8, v.id_parent9, "
            "                    v.id_parent10)";

        if (!subtypes.empty()) {
            std::string list = implode(subtypes, ", ", [](const auto& it) {
                return std::to_string(it);
            });
            select += " AND v.id_asset_device_type in (" + list + ")";
        }

        if (!types.empty()) {
            std::string list = implode(types, ", ", [](const auto& it) {
                return std::to_string(it);
            });
            select += " AND v.id_type in (" + list + ")";
        }

        if (!status.empty()) {
            select += " AND v.status = \"" + status + "\"";
        }

        std::string endSelect = "";
        if (!without.empty()) {
            if (without == "location") {
                select += " AND v.id_parent1 is NULL ";
            } else if (without == "powerchain") {
                endSelect +=
                    " AND NOT EXISTS "
                    " (SELECT id_asset_device_dest "
                    "  FROM t_bios_asset_link_type as l JOIN t_bios_asset_link as a"
                    "  ON a.id_asset_link_type=l.id_asset_link_type "
                    "  WHERE "
                    "     name=\"power chain\" "
                    "     AND v.id_asset_element=a.id_asset_device_dest)";
            } else {
                endSelect +=
                    " AND NOT EXISTS "
                    " (SELECT a.id_asset_element "
                    "  FROM "
                    "     t_bios_asset_ext_attributes as a "
                    "  WHERE "
                    "     a.keytag=\"" +
                    without +
                    "\""
                    "     AND v.id_asset_element = a.id_asset_element)";
            }
        }

        select += endSelect;

        // Can return more than one row.
        tntdb::Statement st = conn.prepareCached(select);

        tntdb::Result result = st.set("containerid", elementId).select();
        log_debug("[v_bios_asset_element_super_parent]: were selected %" PRIu32 " rows", result.size());
        for (auto& row : result) {
            cb(row);
        }
        return {};
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
}

Expected<std::map<std::string, int>> getDictionary(const std::string& stStr)
{
    std::map<std::string, int> mymap;

    try {
        tntdb::Connection conn = tntdb::connectCached(DBConn::url);
        tntdb::Statement  st   = conn.prepare(stStr);
        tntdb::Result     res  = st.select();

        std::string name = "";
        int         id   = 0;
        for (auto& row : res) {
            row[0].get(name);
            row[1].get(id);
            mymap.insert(std::pair<std::string, int>(name, id));
        }
        return std::move(mymap);
    } catch (const std::exception& e) {
        return unexpected(error(Errors::ExceptionForElement).format(e.what(), stStr));
    }
}

Expected<std::map<std::string, int>> readElementTypes()
{
    static std::string st_dictionary_element_type =
        " SELECT"
        "   v.name, v.id"
        " FROM"
        "   v_bios_asset_element_type v";

    return getDictionary(st_dictionary_element_type);
}

Expected<std::map<std::string, int>> readDeviceTypes()
{
    static std::string st_dictionary_device_type =
        " SELECT"
        "   v.name, v.id"
        " FROM"
        "   v_bios_asset_device_type v";

    return getDictionary(st_dictionary_device_type);
}

} // namespace fty::asset::db
