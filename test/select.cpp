#include "src/asset/asset-db.h"
#include "src/asset/db.h"
#include <catch2/catch.hpp>
#include <fty_common_asset_types.h>

TEST_CASE("Select asset")
{
    tnt::Connection conn;

    fty::asset::db::AssetElement el;
    el.name      = "device";
    el.status    = "active";
    el.priority  = 1;
    el.subtypeId = persist::subtype_to_subtypeid("ups");
    el.typeId    = persist::type_to_typeid("device");

    {
        auto ret = fty::asset::db::insertIntoAssetElement(conn, el, true);
        if (!ret) {
            FAIL(ret.error());
        }
        REQUIRE(*ret > 0);
        el.id = *ret;
    }

    {
        auto ret = fty::asset::db::insertIntoAssetExtAttributes(conn, el.id, {{"name", "Device name"}}, true);
        if (!ret) {
            FAIL(ret.error());
        }
        REQUIRE(*ret > 0);
    }

    SECTION("selectAssetElementByName")
    {
        auto res = fty::asset::db::selectAssetElementByName("device");
        if (!res) {
            FAIL(res.error());
        }
        REQUIRE(res);
        CHECK(res->id == el.id);
        CHECK(res->name == "device");
        CHECK(res->status == "active");
        CHECK(res->priority == 1);
        CHECK(res->subtypeId == persist::subtype_to_subtypeid("ups"));
        CHECK(res->typeId == persist::type_to_typeid("device"));
    }

    SECTION("selectAssetElementByName/wrong")
    {
        auto res = fty::asset::db::selectAssetElementByName("mydevice");
        CHECK(!res);
        CHECK(res.error() == "Element 'mydevice' not found.");
    }

    SECTION("selectAssetElementWebById")
    {
        auto res = fty::asset::db::selectAssetElementWebById(el.id);
        if (!res) {
            FAIL(res.error());
        }
        REQUIRE(res);
        CHECK(res->id == el.id);
        CHECK(res->name == "device");
        CHECK(res->status == "active");
        CHECK(res->priority == 1);
        CHECK(res->subtypeId == persist::subtype_to_subtypeid("ups"));
        CHECK(res->typeId == persist::type_to_typeid("device"));
    }


    // Clean up
    {
        auto res = fty::asset::db::deleteAssetExtAttributesWithRo(conn, el.id, true);
        if (!res) {
            FAIL(res.error());
        }
        CHECK(res);
        CHECK(res > 0);
    }

    {
        auto res = fty::asset::db::deleteAssetElement(conn, el.id);
        if (!res) {
            FAIL(res.error());
        }
        REQUIRE(*res > 0);
    }
}
