#include "src/asset/asset-db.h"
#include "src/asset/db.h"
#include <catch2/catch.hpp>
#include <fty_common_asset_types.h>

TEST_CASE("Asset/insert")
{
    tnt::Connection conn;

    fty::asset::db::AssetElement el;
    el.name      = "device";
    el.status    = "active";
    el.priority  = 1;
    el.subtypeId = persist::subtype_to_subtypeid("ups");
    el.typeId    = persist::type_to_typeid("device");

    auto ret = fty::asset::db::insertIntoAssetElement(conn, el, true);
    if (!ret) {
        FAIL(ret.error());
    }
    REQUIRE(*ret > 0);
    uint32_t id = *ret;

    auto ret2 = fty::asset::db::deleteAssetElement(conn, id);
    if (!ret) {
        FAIL(ret.error());
    }
    REQUIRE(*ret > 0);
}

TEST_CASE("Asset/Wrong parent")
{
    tnt::Connection conn;

    fty::asset::db::AssetElement el;
    el.name      = "device";
    el.status    = "active";
    el.priority  = 1;
    el.subtypeId = persist::subtype_to_subtypeid("ups");
    el.typeId    = persist::type_to_typeid("device");
    el.parentId  = uint32_t(-1);

    auto ret = fty::asset::db::insertIntoAssetElement(conn, el, true);
    REQUIRE(!ret);
    CHECK(ret.error().find("a foreign key constraint fails") != std::string::npos);
    //std::cerr << ret.error() << std::endl;
}

TEST_CASE("Asset/Wrong type")
{
    tnt::Connection conn;

    fty::asset::db::AssetElement el;
    el.name      = "device";
    el.status    = "active";
    el.priority  = 1;
    el.subtypeId = persist::subtype_to_subtypeid("ups");

    auto ret = fty::asset::db::insertIntoAssetElement(conn, el, true);
    REQUIRE(!ret);
    CHECK("0 value of element_type_id is not allowed" == ret.error());
}

TEST_CASE("Asset/Wrong name1")
{
    tnt::Connection conn;

    fty::asset::db::AssetElement el;
    el.name      = "device_1";
    el.status    = "active";
    el.priority  = 1;
    el.typeId    = persist::type_to_typeid("device");
    el.subtypeId = persist::subtype_to_subtypeid("ups");

    auto ret = fty::asset::db::insertIntoAssetElement(conn, el, true);
    REQUIRE(!ret);
    CHECK("wrong element name" == ret.error());
}

TEST_CASE("Asset/Wrong name2")
{
    tnt::Connection conn;

    fty::asset::db::AssetElement el;
    el.name      = "device@";
    el.status    = "active";
    el.priority  = 1;
    el.typeId    = persist::type_to_typeid("device");
    el.subtypeId = persist::subtype_to_subtypeid("ups");

    auto ret = fty::asset::db::insertIntoAssetElement(conn, el, true);
    REQUIRE(!ret);
    CHECK("wrong element name" == ret.error());
}

TEST_CASE("Asset/Wrong name3")
{
    tnt::Connection conn;

    fty::asset::db::AssetElement el;
    el.name      = "device%";
    el.status    = "active";
    el.priority  = 1;
    el.typeId    = persist::type_to_typeid("device");
    el.subtypeId = persist::subtype_to_subtypeid("ups");

    auto ret = fty::asset::db::insertIntoAssetElement(conn, el, true);
    REQUIRE(!ret);
    CHECK("wrong element name" == ret.error());
}

