/*  ====================================================================================================================
    create.cpp - Implementation of POST (create) operation on any asset

    Copyright (C) 2014 - 2020 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    ====================================================================================================================
*/

#include "create.h"
#include "asset/csv.h"
#include <fty/rest/audit-log.h>

#include <fty_common_asset.h>
#include <fty_asset_activator.h>
#include "asset/asset-import.h"
#include "asset/asset-configure-inform.h"

#define AGENT_ASSET_ACTIVATOR "etn-licensing-credits"
#define CREATE_MODE_ONE_ASSET 1
#define CREATE_MODE_CSV       2

namespace fty::asset {

Expected<std::string> Create::readName(const std::string& cnt) const
{
    // cache the manipulated asset name
    std::string item;
    try {
        auto item_pos1 = cnt.find("\"name\"");
        auto item_pos2 = cnt.find_first_of('"', item_pos1 + strlen("\"name\"") + 1);
        auto item_pos3 = cnt.find_first_of('"', item_pos2 + 2);
        item           = cnt.substr(item_pos2 + 1, item_pos3 - item_pos2 - 1);
    } catch (const std::exception& e) {
        return unexpected(e.what());
    }
    return std::move(item);
}

static std::string generateMlmClientId(const std::string& client_name) {
    std::string name = client_name;
    std::stringstream ss;
    ss << std::this_thread::get_id();
    std::string pid = ss.str();
    if (!pid.empty()) {
        name += "." + pid;
    } else {
        name += "." + std::to_string(random());
    }
    return name;
}

unsigned Create::run()
{
    rest::User user(m_request);
    if (auto ret = checkPermissions(user.profile(), m_permissions); !ret) {
        throw rest::Error(ret.error());
    }

    std::string asset_json = m_request.body();
    std::string itemName;

    if (auto name = readName(asset_json); !name) {
        auditError("Request CREATE asset <undefined> FAILED: {}", name.error());
        throw rest::Error("bad-request-document", name.error());
    } else {
        itemName = name;
    }

    // Read json, transform to csv, use existing functionality
    cxxtools::SerializationInfo si;
    try {
        std::stringstream          input(asset_json, std::ios_base::in);
        cxxtools::JsonDeserializer deserializer(input);
        deserializer.deserialize(si);
    } catch (const std::exception& e) {
        auditError("Request CREATE asset {} FAILED"_tr, itemName);
        throw rest::Error("bad-request-document", e.what());
    }

    CsvMap cm;
    try {
        cm = CsvMap_from_serialization_info(si);
        cm.setCreateUser(user.login());
        cm.setCreateMode(CREATE_MODE_ONE_ASSET);
    } catch (const std::invalid_argument& e) {
        log_error("%s", e.what());
        auditError("Request CREATE asset {} FAILED: {}"_tr, itemName, e.what());
        throw rest::Error("bad-request-document", e.what());
    } catch (const std::exception& e) {
        auditError("Request CREATE asset {} FAILED: {}"_tr, itemName, e.what());
        throw rest::Error("internal-error", "See logs for more details");
    }

    if (cm.cols() == 0 || cm.rows() == 0) {
        auditError("Request CREATE asset {} FAILED"_tr, itemName);
        throw rest::Error("bad-request-document", "Cannot import empty document.");
    }
    // in underlying functions like update_device
    if (!cm.hasTitle("type")) {
        auditError("Request CREATE asset {} FAILED"_tr, itemName);
        throw rest::Error("request-param-required", rest::error("type"_tr).message);
    }
    if (cm.hasTitle("id")) {
        auditError("Request CREATE asset {} FAILED"_tr, itemName);
        throw rest::Error("bad-request-document", "key 'id' is forbidden to be used"_tr);
    }

    fty::FullAsset asset;
    try {
        si >>= asset;
    } catch (const std::invalid_argument& e) {
        log_error("%s", e.what());
        auditError("Request CREATE asset {} FAILED"_tr, itemName);
        throw rest::Error("bad-input", e.what());
    } catch (const std::exception& e) {
        log_error("%s", e.what());
        auditError("Request CREATE asset {} FAILED"_tr, itemName);
        throw rest::Error("request-param-required", e.what());
    }

    try {
        if (asset.isPowerAsset() && asset.getStatusString() == "active") {
            mlm::MlmSyncClient  client(AGENT_FTY_ASSET, AGENT_ASSET_ACTIVATOR);
            fty::AssetActivator activationAccessor(client);
            if (!activationAccessor.isActivable(asset)) {
                throw rest::Error("internal-error", "Asset cannot be activated"_tr);
            }
        }
    } catch (const std::exception& e) {
        auditError("Request CREATE asset {} FAILED"_tr, itemName);
        throw rest::Error("licensing-err", e.what());
    }

    // actual insert - throws exceptions
    Import import(cm);
    if (auto res = import.process()) {
        // here we are -> insert was successful
        // ATTENTION:  1. sending messages is "hidden functionality" from user
        //             2. if any error would occur during the sending message,
        //                user will never know if element was actually inserted
        //                or not

        // this code can be executed in multiple threads -> agent's name should
        // be unique at the every moment
        std::string agent_name = generateMlmClientId("web.asset_post");
        if (auto sent = sendConfigure(import.item(), import.operation(), agent_name); !sent) {
            log_error ("%s", sent.error().c_str());
            std::string msg = "Error during configuration sending of asset change notification. Consult system log."_tr;
            auditError("Request CREATE asset {} FAILED"_tr, itemName);
            throw rest::Error("internal-error", msg);
        }

        // no unexpected errors was detected
        // process results
        auto ret =  db::idToNameExtName(import.item().id);
        if (!ret) {
            std::string err = "Database failure"_tr;
            auditError("Request CREATE asset {} FAILED", itemName);
            throw rest::Error("internal-error", err);
        }
        m_reply << ret->first;
        auditInfo("Request CREATE asset {} SUCCESS"_tr, itemName);
    } else {
        throw rest::Error(res.error());
    }

    return HTTP_OK;
}

} // namespace fty::asset
