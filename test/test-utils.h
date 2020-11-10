#pragma once
#include "asset/asset-db.h"
#include "asset/asset-helpers.h"
#include "asset/db.h"
#include <fty_common_asset_types.h>
#include <yaml-cpp/yaml.h>
#include <catch2/catch.hpp>

inline fty::asset::db::AssetElement createAsset(
    const std::string& name, const std::string& extName, const std::string& type, uint32_t parentId = 0)
{
    tnt::Connection conn;

    fty::asset::db::AssetElement el;
    el.name     = name;
    el.status   = "active";
    el.priority = 1;
    el.typeId   = persist::type_to_typeid(type);
    el.parentId = parentId;

    {
        auto ret = fty::asset::db::insertIntoAssetElement(conn, el, true);
        if (!ret) {
            FAIL(ret.error());
        }
        REQUIRE(*ret > 0);
        el.id = *ret;
    }

    {
        auto ret = fty::asset::db::insertIntoAssetExtAttributes(conn, el.id, {{"name", extName}}, true);
        if (!ret) {
            FAIL(ret.error());
        }
        REQUIRE(*ret > 0);
    }

    return el;
}

inline void deleteAsset(const fty::asset::db::AssetElement& el)
{
    tnt::Connection conn;

    {
        auto ret = fty::asset::db::deleteAssetExtAttributesWithRo(conn, el.id, true);
        if (!ret) {
            FAIL(ret.error());
        }
        CHECK(ret);
        CHECK(*ret > 0);
    }

    {
        auto ret = fty::asset::db::deleteAssetExtAttributesWithRo(conn, el.id, false);
        if (!ret) {
            FAIL(ret.error());
        }
        CHECK(ret);
    }

    {
        auto ret = fty::asset::db::deleteAssetElement(conn, el.id);
        if (!ret) {
            FAIL(ret.error());
        }
        CHECK(ret);
        CHECK(*ret > 0);
    }
}

inline YAML::Node reorder(const YAML::Node& node)
{
    YAML::Node ret;
    if (node.IsMap()) {
        ret = YAML::Node(YAML::NodeType::Map);
        std::map<std::string, YAML::Node> map;
        for(const auto& it: node) {
            map[it.first.as<std::string>()] = it.second.IsMap() ? reorder(it.second) : it.second;
        }
        for(const auto&[key, val]: map) {
            ret[key] = val;
        }
    } else if (node.IsSequence()) {
        ret = YAML::Node(YAML::NodeType::Sequence);
        for(const auto& it: node) {
            ret.push_back(it.second.IsMap() ? reorder(it.second) : it.second);
        }
    } else {
        ret = node;
    }
    return ret;
}

inline std::string normalizeJson(const std::string& json)
{
    try {
        YAML::Node yaml = YAML::Load(json);

        if (yaml.IsMap()) {
            yaml = reorder(yaml);
        }

        YAML::Emitter out;
        out << YAML::DoubleQuoted << YAML::Flow << yaml;
        return std::string(out.c_str());
    } catch (const std::exception&) {
        return "";
    }
}
