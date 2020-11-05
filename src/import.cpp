#include "import.h"
#include <fty/rest/audit-log.h>

namespace fty::asset {

unsigned Import::run()
{
    rest::User user(m_request);
    if (auto ret = checkPermissions(user.profile(), m_permissions); !ret) {
        throw rest::Error(ret.error());
    }

    // sanity check
    Expected<std::string> id  = m_request.queryArg<std::string>("id");
    Expected<std::string> ids = m_request.queryArg<std::string>("ids");
    if (!id && !ids) {
        auditError("Request DELETE asset FAILED"_tr);
        throw rest::Error("request-param-required", "id");
    }
    return HTTP_OK;
}

}

#if 0
#include <string>
#include <stdexcept>
#include <fstream>
#include <cstring>
#include <sys/types.h>
#include <unistd.h>
#include <fty_common_rest_helpers.h>

#include "src/db/inout.h"
#include "bios_magic.h"
#include "shared/configure_inform.h"
#include <fty_common_rest_audit_log.h>
</%pre>
<%request scope="global">
UserInfo user;
bool database_ready;
</%request>
<%cpp>
    // verify server is ready
    if (!database_ready) {
        log_debug ("Database is not ready yet.");
        std::string err =  TRANSLATE_ME ("Database is not ready yet, please try again after a while.");
        if (request.getMethod () == "POST") {
            log_error_audit ("Request CREATE asset_import FAILED");
        }
        http_die ("internal-error", err.c_str ());
    }

    // check user permissions
    static const std::map <BiosProfile, std::string> PERMISSIONS = {
            {BiosProfile::Admin,     "C"}
            };
    std::string audit_msg;
    if (request.getMethod () == "POST")
        audit_msg = std::string ("Request CREATE asset_import FAILED");
    CHECK_USER_PERMISSIONS_OR_DIE_AUDIT (PERMISSIONS, audit_msg.empty () ? nullptr : audit_msg.c_str ());

if (request.getMethod () != "POST") {
    http_die ("method-not-allowed", request.getMethod ().c_str () );
}

//HARDCODED limit: can't import things larger than 128K
// this prevents DoS attacks against the box - can be raised if needed
// don't forget internal processing is in UCS-32, therefore the
// real memory requirements are ~640kB
// Content size = body + something. So max size of body is about 125k
if (request.getContentSize () > 128*1024) {
    log_error_audit ("Request CREATE asset_import FAILED");
    http_die ("content-too-big", "125k");
}

// http://www.tntnet.org/howto/upload-howto.html
const tnt::Multipart& mp = request.getMultipart ();
tnt::Multipart::const_iterator it = mp.find ("assets");
if (it == mp.end ()) {
    log_error_audit ("Request CREATE asset_import FAILED");
    http_die ("request-param-required", "file=assets");
}

std::string path_p;
try {
    shared::convert_file (it->getBodyBegin (), it->getBodyEnd (), path_p);
}
catch (const std::logic_error &e) {
    LOG_END_ABNORMAL (e);
    if (!path_p.empty ())
        ::unlink (path_p.c_str ());
    log_error_audit ("Request CREATE asset_import FAILED");
    http_die ("bad-request-document", e.what ());
}
catch (const std::runtime_error &e) {
    LOG_END_ABNORMAL (e);
    if (!path_p.empty ())
        ::unlink (path_p.c_str ());
    log_error_audit ("Request CREATE asset_import FAILED");
    http_die ("internal-error", e.what ());
}
catch (const std::exception &e) {
    LOG_END_ABNORMAL (e);
    if (!path_p.empty ())
        ::unlink (path_p.c_str ());
    log_error_audit ("Request CREATE asset_import FAILED");
    http_die ("internal-error", e.what ());
}

try{
    std::ifstream inp{path_p};
    std::vector <std::pair<db_a_elmnt_t,persist::asset_operation>> okRows{};
    std::map<int,std::string> failRows{};

    auto touch_fn = [&request]() {
        request.touch ();};
    persist::load_asset_csv (inp, okRows, failRows, touch_fn, user.login ());

    inp.close ();
    ::unlink (path_p.c_str ());

    log_debug ("ok size is %zu", okRows.size ());
    log_debug ("fail size is %zu", failRows.size ());

    // ACE: TODO
    // ATTENTION:  1. sending messages is "hidden functionality" from user
    //             2. if any error would occur during the sending message,
    //                user will never know what was actually imported or not
    // in theory
    // this code can be executed in multiple threads -> agent's name should
    // be unique at the every moment
    std::string agent_name = utils::generate_mlm_client_id ("web.asset_import");
    send_configure (okRows, agent_name);
</%cpp>
{
    "imported_lines" : <$$ okRows.size () $>,
    "errors" : [
<%cpp>
    size_t cnt = 0;
    for ( auto &oneRow : failRows )
    {
        cnt++;
</%cpp>
            [ <$$ oneRow.first$>, <$$ oneRow.second.c_str ()$>]<$$ cnt != failRows.size () ? ',' : ' ' $>
%   }
    ]
}
<%cpp>
}
catch (const BiosError &e) {
    if (!path_p.empty ())
        ::unlink (path_p.c_str ());
    log_error_audit ("Request CREATE asset_import FAILED");
    http_die_idx (e.idx, e.what ());
}
catch (const std::exception &e) {
    LOG_END_ABNORMAL (e);
    if (!path_p.empty ())
        ::unlink (path_p.c_str ());
    log_error_audit ("Request CREATE asset_import FAILED");
    http_die ("internal-error", e.what ());
}
log_info_audit ("Request CREATE asset_import SUCCESS");
</%cpp>
#endif
