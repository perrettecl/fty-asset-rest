/*
 *
 * Copyright (C) 2015 - 2018 Eaton
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "asset/asset-cam.h"
#include "asset/logger.h"

#include <algorithm>
#include <fty/split.h>

static constexpr const char* SECW_CRED_ID_KEY = "secw_credential_id";

void getExtMapFromSi(const cxxtools::SerializationInfo& si, ExtMap& map) {
    const cxxtools::SerializationInfo extSi = si.getMember("ext");

    if (extSi.category() == cxxtools::SerializationInfo::Array) {
        log_debug("it is GET format");
        // this information in GET format
        for (const auto& oneAttrEl : extSi) { // iterate through the array
            if (oneAttrEl.memberCount() != 2) {
                throw std::invalid_argument("Expected two properties per each ext attribute");
            }
            // ASSUMPTION: oneAttr has 2 fields:
            // "read_only" - this is an information only
            // "some_unknown_name"
            std::string key;
            ExtMapElement el;

            for (unsigned int i = 0; i < 2; ++i) {
                auto& oneAttr = oneAttrEl.getMember(i);
                auto& name = oneAttr.name();

                if (name == "read_only") {
                    oneAttr.getValue(el.readOnly);
                } else {
                    key = name;
                    oneAttr.getValue(el.value);
                }
            }

            el.wasUpdated = true;

            map[key] = el;
        }
    } else {
        throw std::invalid_argument("Key 'ext' should be an Array or Object");
    }
}

std::list<CredentialMapping> getCredentialMappings(const ExtMap& extMap) {
    std::list<CredentialMapping> credentialList;

    logDebug("Looking for mapping entries");

    auto findCredKey = [&] (const auto& el) {
        return el.first.find(SECW_CRED_ID_KEY) != std::string::npos;
    };

    // lookup for ext attributes which contains secw_credential_id in the key
    auto found = std::find_if(extMap.begin(), extMap.end(), findCredKey);

    // create mapping
    while(found != extMap.end()) {
        CredentialMapping c;
        c.credentialId = found->second.value;

        logDebug("Found new credential : {}", c.credentialId);

        // extract protocol from element key (endpoint.XX.protocol.secw_credential_id)
        // auto keyTokens = fty::split(found->first, ".");
        c.serviceId = CAM_SERVICE_ID;
        c.protocol = /* keyTokens.size() >= 3 ? keyTokens[2] : */ CAM_DEFAULT_PROTOCOL;
        c.port = CAM_DEFAULT_PORT;
        credentialList.push_back(c);

        logDebug("Protocol : {}", c.protocol);

        found = std::find_if(++found, extMap.end(), findCredKey);
    }
    return credentialList;
}
