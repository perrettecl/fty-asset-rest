#include "list.h"
#include "asset/asset-db.h"
#include "asset/asset-manager.h"
#include <fty/split.h>
#include <fty_common_asset_types.h>
#include <pack/pack.h>
#include <fty/rest/component.h>

namespace fty::asset {

struct Info : public pack::Node
{
    pack::UInt32 id   = FIELD("id");
    pack::String name = FIELD("name");

    using pack::Node::Node;
    META(Info, id, name);
};

unsigned List::run()
{
    rest::User user(m_request);
    if (auto ret = checkPermissions(user.profile(), m_permissions); !ret) {
        throw rest::Error(ret.error());
    }

    if (m_request.type() != rest::Request::Type::Get) {
        throw rest::Error("method-not-allowed", m_request.type());
    }

    Expected<std::string> assetType    = m_request.queryArg<std::string>("type");
    Expected<std::string> subtype = m_request.queryArg<std::string>("subtype");

    if (!assetType || !persist::type_to_typeid(*assetType)) {
        throw rest::Error("request-param-bad", "type", "datacenter/room/row/rack/group/device");
    }

    std::vector<std::string> subtypes;
    if (subtype) {
        subtypes = split(*subtype, ",");
        for (const auto& it : subtypes) {
            if (!persist::subtype_to_subtypeid(it)) {
                throw rest::Error("request-param-bad", "subtype", subtype, "See RFC-11 for possible values"_tr);
            }
        }
    }

    AssetManager assetMgr;

    pack::Map<pack::ObjectList<Info>> ret;
    auto&                             val = ret.append(*assetType + "s");

    for (const auto& assetSubtype : subtypes) {
        // Get data
        auto allAssetsShort = assetMgr.getItems(*assetType, assetSubtype);
        if (!allAssetsShort) {
            throw rest::Error(allAssetsShort.error());
        }

        for (const auto& [id, name] : *allAssetsShort) {
            auto assetNames = db::idToNameExtName(id);
            if (!assetNames) {
                throw rest::Error("internal-error", "Database failure"_tr);
            }

            auto& ins = val.append();
            ins.id    = id;
            ins.name  = assetNames->second;
        }
    }

    m_reply << *pack::json::serialize(ret);
    return HTTP_OK;
}

} // namespace fty::asset

registerHandler(fty::asset::List)
