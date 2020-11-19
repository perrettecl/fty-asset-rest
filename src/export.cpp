#include "export.h"
#include "asset/asset-db.h"
#include <fty/rest/component.h>
#include <fty_common_asset_types.h>
#include <chrono>
#include <regex>
#include "asset/asset-manager.h"

namespace fty::asset {

unsigned Export::run()
{
    rest::User user(m_request);
    if (auto ret = checkPermissions(user.profile(), m_permissions); !ret) {
        throw rest::Error(ret.error());
    }

    auto dc = m_request.queryArg<std::string>("dc");
    std::optional<db::AssetElement> dcAsset;
    if (dc) {
        auto asset = db::selectAssetElementByName(*dc);
        if (!asset || asset->typeId != persist::type_to_typeid("datacenter")) {
            throw rest::Error(
                "request-param-bad", "dc", "not a datacenter"_tr, "existing asset which is a datacenter"_tr);
        }
        dcAsset = *asset;
    }

    auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::string strTime(30, '\0');
    std::strftime(&strTime[0], strTime.size(), "%Y-%m-%d%H-%M-%S%TZ", std::localtime(&time));

    if (dcAsset != std::nullopt) {
        auto dcENameRet = db::idToNameExtName(dcAsset->id);
        if (!dcENameRet) {
            throw rest::Error("element-not-found", dcAsset->id);
        }
        //escape special characters
        std::string dcEName = std::regex_replace(dcENameRet->second, std::regex("( |\t)"), "_");
        m_reply.setHeader(tnt::httpheader::contentDisposition, "attachment; filename=\"asset_export_" + dcEName + "_" + strTime + ".csv\"");
    }
    else {
        m_reply.setHeader(tnt::httpheader::contentDisposition, "attachment; filename=\"asset_export" + strTime + ".csv\"");
    }

    auto ret = AssetManager::exportCsv(dcAsset);
    if (ret) {
        m_reply.setContentType ("text/csv;charset=UTF-8");
        m_reply << "\xef\xbb\xbf";
        m_reply << *ret;
    } else {
        throw rest::Error("internal-error", ret.error());
    }

    return HTTP_OK;
}

} // namespace fty::asset

registerHandler(fty::asset::Export)
