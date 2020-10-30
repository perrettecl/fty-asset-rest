#include "delete.h"
#include <fty/rest/audit-log.h>
#include <fty_common_asset_types.h>
#include "asset/asset-db.h"
#include "asset/asset-manager.h"

namespace fty::asset {

unsigned Delete::run()
{
    rest::User user(m_request);
    if (auto ret = checkPermissions(user.profile(), m_permissions); !ret) {
        throw rest::Error(ret.error());
    }

    // sanity check
    Expected<std::string> id = m_request.queryArg<std::string>("id");
    {
        if (!id) {
            auditError("Request DELETE asset FAILED"_tr);
            throw rest::Error("request-param-required", "id");
        }

        if (!persist::is_ok_name(id->c_str())) {
            auditError("Request DELETE asset id {} FAILED"_tr, id);
            throw rest::Error("request-param-bad", "id", id, "valid asset name"_tr);
        }
    }

    Expected<int64_t> dbid = db::nameToAssetId(*id);
    if (!dbid) {
        auditError("Request DELETE asset id {} FAILED: {}"_tr, id, dbid.error());
        throw rest::Error(dbid.error());
    }

    AssetManager assetMgr;

    auto res = assetMgr.deleteItem(uint32_t(*dbid));
    return HTTP_OK;
}

} // namespace fty::asset


#if 0
#include "shared/configure_inform.h"
#include "shared/data.h"
#include <fty_common_db_asset.h>
#include <fty_common_macros.h>
#include <fty_common_rest_audit_log.h>
#include <fty_common_rest_helpers.h>
#include <sys/types.h>
#include <unistd.h>
</%pre>
<%thread scope="global">
asset_manager     asset_mgr;
</%thread>
<%request scope="global">
UserInfo user;
bool database_ready;
</%request>
<%cpp>
{

    // delete it
    db_a_elmnt_t row;
    auto ret = asset_mgr.delete_item (dbid, row);
    if ( ret.status == 0 ) {
        if ( ret.errsubtype == DB_ERROR_NOTFOUND ) {
            log_error_audit ("Request DELETE asset id %s FAILED", id.c_str ());
            http_die ("element-not-found", checked_id.c_str ());
        }
        else {
            std::string reason = TRANSLATE_ME ("Asset is in use, remove children/power source links first.");
            log_error_audit ("Request DELETE asset id %s FAILED", id.c_str ());
            http_die ("data-conflict",  checked_id.c_str (), reason.c_str ());
        }
    }
    // here we are -> delete was successful
    // ATTENTION:  1. sending messages is "hidden functionality" from user
    //             2. if any error would occur during the sending message,
    //                user will never know if element was actually deleted
    //                or not

    // this code can be executed in multiple threads -> agent's name should
    // be unique at the every moment
    std::string agent_name = utils::generate_mlm_client_id ("web.asset_delete");
    try {
        send_configure (row, persist::asset_operation::DELETE, agent_name);
</%cpp>
{}
<%cpp>
        log_info_audit ("Request DELETE asset id %s SUCCESS", id.c_str ());
        return HTTP_OK;
    }
    catch (const std::runtime_error &e) {
        log_error ("%s", e.what ());
        std::string msg = TRANSLATE_ME ("Error during configuration sending of asset change notification. Consult system log.");
        log_error_audit ("Request DELETE asset id %s FAILED", id.c_str ());
        http_die ("internal-error", msg.c_str ());
    }
}
</%cpp>
#endif
