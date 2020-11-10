#include "test-utils.h"
#include "asset/asset-manager.h"

TEST_CASE("Create asset")
{
    fty::asset::db::AssetElement dc = createAsset("datacenter", "Data center", "datacenter");

    SECTION("Create 1")
    {
        static std::string json = R"({
            "location" :            "Data center",
            "name" :                "dev1",
            "powers":               [],
            "priority" :            "P2",
            "status" :              "active",
            "sub_type" :            "N_A",
            "type" :                "room",
            "ext": [
                {"asset_tag": "", "read_only": false},
                {"contact_name": "", "read_only": false},
                {"contact_email": "", "read_only": false},
                {"contact_phone": "", "read_only": false},
                {"description": "", "read_only": false},
                {"create_mode": "", "read_only": false},
                {"update_ts": "", "read_only": false}
            ]
        })";

        auto ret = fty::asset::AssetManager::createAsset(json, "dummy", false);
        if (!ret) {
            FAIL(ret.error());
        }
        REQUIRE(ret);
        CHECK(*ret > 0);
        auto el = fty::asset::db::selectAssetElementWebById(*ret);
        deleteAsset(*el);
    }

    deleteAsset(dc);
}
