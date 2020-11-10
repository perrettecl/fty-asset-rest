#include "asset/json.h"
#include "test-utils.h"


TEST_CASE("Read asset")
{
    fty::asset::db::AssetElement dc = createAsset("datacenter", "Data center", "datacenter");
    fty::asset::db::AssetElement el = createAsset("device", "Device name", "device", dc.id);

    // =================================================================================================================

    SECTION("Check id")
    {
        std::string strId = "device";
        uint32_t    id    = 0;
        if (auto res = fty::asset::checkElementIdentifier("dev", strId)) {
            id = *res;
        } else if (auto res2 = fty::asset::checkElementIdentifier("dev", "device" + strId)) {
            id = *res2;
        } else {
            FAIL("Not found " + strId);
        }
        CHECK(id > 0);
    }

    SECTION("Check id/wrong")
    {
        std::string strId = "device1";
        uint32_t    id    = 0;
        if (auto res = fty::asset::checkElementIdentifier("dev", strId)) {
            id = *res;
        } else if (auto res2 = fty::asset::checkElementIdentifier("dev", "device" + strId)) {
            id = *res2;
        }
        CHECK(id == 0);
    }

    SECTION("Read")
    {
        static std::string check =  R"({
            "id" :                  "device",
            "power_devices_in_uri": "/api/v1/assets?in=device&sub_type=epdu,pdu,feed,genset,ups,sts,rackcontroller",
            "name" :                "Device name",
            "status" :              "active",
            "priority" :            "P1",
            "type" :                "device",
            "location_uri" :        "/api/v1/asset/datacenter",
            "location_id" :         "datacenter",
            "location" :            "Data center",
            "groups":               [],
            "powers":               [],
            "sub_type" :            "N_A",
            "ext" :                 [],
            "computed" :            {},
            "parents" : [{
                "id" :          "datacenter",
                "name" :        "Data center",
                "type" :        "datacenter",
                "sub_type" :    "N_A"
            }]
        })";

        uint32_t id = 0;
        if (auto res = fty::asset::checkElementIdentifier("dev", "device")) {
            id = *res;
        }

        REQUIRE(id > 0);
        std::string jsonAsset = fty::asset::getJsonAsset(id);

        CHECK(!jsonAsset.empty());
        CHECK(normalizeJson(jsonAsset) == normalizeJson(check));
    }

    // =================================================================================================================

    deleteAsset(el);
    deleteAsset(dc);
}
