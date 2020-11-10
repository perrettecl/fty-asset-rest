#include "import.h"
#include <fty/rest/audit-log.h>
#include "asset/csv.h"

namespace fty::asset {

#define CREATE_MODE_CSV         2

unsigned Import::run()
{
    rest::User user(m_request);
    if (auto ret = checkPermissions(user.profile(), m_permissions); !ret) {
        throw rest::Error(ret.error());
    }

    //HARDCODED limit: can't import things larger than 128K
    // this prevents DoS attacks against the box - can be raised if needed
    // don't forget internal processing is in UCS-32, therefore the
    // real memory requirements are ~640kB
    // Content size = body + something. So max size of body is about 125k
    if (m_request.contentSize () > 128*1024) {
        auditError("Request CREATE asset_import FAILED {}"_tr, "can't import things larger than 128K"_tr);
        throw rest::Error("content-too-big", "128k");
    }

    if (auto part = m_request.multipart("assets")) {
        std::stringstream ss(*part);
        CsvMap csv = CsvMap_from_istream(ss);

        csv.setCreateMode(CREATE_MODE_CSV);
        csv.setCreateUser(user.login());
        csv.setUpdateUser(user.login());
    } else {
        auditError("Request CREATE asset_import FAILED {}"_tr, part.error());
        throw rest::Error("request-param-required", "file=assets");
    }

    return HTTP_OK;
}

}

#if 0

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
