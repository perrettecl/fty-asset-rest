#include "edit.h"
#include "asset/asset-configure-inform.h"
#include "asset/asset-import.h"
#include "asset/asset-manager.h"
#include "asset/csv.h"
#include "asset/asset/asset-cam.h"
#include "asset/logger.h"
#include <cxxtools/jsondeserializer.h>
#include <fty/rest/audit-log.h>
#include <fty/rest/component.h>
#include <fty_asset_activator.h>
#include <fty_common_asset.h>
#include <fty_common_asset_types.h>
#include <fty_common_mlm.h>
#include <fty_security_wallet.h>

namespace fty::asset {

#define AGENT_ASSET_ACTIVATOR "etn-licensing-credits"

unsigned Edit::run()
{
    rest::User user(m_request);
    if (auto ret = checkPermissions(user.profile(), m_permissions); !ret) {
        throw rest::Error(ret.error());
    }

    Expected<std::string> id = m_request.queryArg<std::string>("id");
    if (!id) {
        auditError("Request CREATE OR UPDATE asset FAILED: {}"_tr, "Asset id is not set"_tr);
        throw rest::errors::RequestParamRequired("id");
    }

    if (!persist::is_ok_name(id->c_str())) {
        auditError("Request CREATE OR UPDATE asset FAILED: {}"_tr, "Asset id is not valid"_tr);
        throw rest::errors::RequestParamBad("id", *id, "Valid id"_tr);
    }

    std::string                 asset_json(m_request.body());
    cxxtools::SerializationInfo si;
    try {
        std::stringstream          input(asset_json, std::ios_base::in);
        cxxtools::JsonDeserializer deserializer(input);
        deserializer.deserialize(si);
        auto        si_id = si.findMember("id");
        std::string status;
        si.getMember("status") >>= status;
        std::string type;
        si.getMember("type") >>= type;

        if (!si_id) {
            si.addMember("id") <<= *id;
        } else if (status == "nonactive") {
            if (*id == "rackcontroller-0" || persist::is_container(type)) {
                logDebug("Element {} cannot be inactivated.", *id);
                auditError("Request CREATE OR UPDATE asset id {} FAILED"_tr, *id);
                throw rest::errors::ActionForbidden("inactivate", "Inactivation of this asset"_tr);
            }
        } else {
            si_id->setValue(*id);
        }
    } catch (const std::exception& e) {
        auditError("Request CREATE OR UPDATE asset id {} FAILED"_tr, *id);
        throw rest::errors::Internal(e.what());
    }

    try {
        fty::FullAsset asset;
        si >>= asset;

        if (asset.isPowerAsset() && asset.getStatusString() == "active") {
            mlm::MlmSyncClient  client(AGENT_FTY_ASSET, AGENT_ASSET_ACTIVATOR);
            fty::AssetActivator activationAccessor(client);
            if (!activationAccessor.isActivable(asset)) {
                throw std::runtime_error("Asset cannot be activated"_tr);
            }
        }
    } catch (const std::exception& e) {
        auditError("Request CREATE OR UPDATE asset id {} FAILED"_tr, id);
        throw rest::errors::LicensingErr(e.what());
    }

    CsvMap cm;
    try {
        cm = CsvMap_from_serialization_info(si);
        cm.setUpdateUser(user.login());
        std::time_t timestamp = std::time(nullptr);
        std::string mbstr(100, '\0');
        if (std::strftime(&mbstr[0], sizeof(mbstr), "%FT%T%z", std::localtime(&timestamp))) {
            cm.setUpdateTs(mbstr);
        }
    } catch (const std::invalid_argument& e) {
        logError(e.what());
        auditError("Request CREATE OR UPDATE asset id {} FAILED"_tr, *id);
        throw rest::errors::BadRequestDocument(e.what());
    } catch (const std::exception& e) {
        auditError("Request CREATE OR UPDATE asset id {} FAILED"_tr, *id);
        throw rest::errors::Internal(e.what());
    }

    // PUT /asset is currently used to update an existing device (only asset_element and ext_attributes)
    //      for EMC4J.
    // empty document
    if (cm.cols() == 0 || cm.rows() == 0) {
        auditError("Request CREATE OR UPDATE asset id {} FAILED", *id);
        throw rest::errors::BadRequestDocument("Cannot import empty document."_tr);
    }

    if (!cm.hasTitle("type")) {
        auditError("Request CREATE OR UPDATE asset id {} FAILED", *id);
        throw rest::errors::RequestParamRequired("type"_tr);
    }

    logDebug("starting load");
    Import import(cm);
    if (auto res = import.process(true)) {
        const auto& imported = import.items();
        if (imported.find(1) == imported.end()) {
            throw rest::errors::Internal("Request CREATE OR UPDATE asset id {} FAILED"_tr.format(*id));
        }

        if (imported.at(1)) {
            // this code can be executed in multiple threads -> agent's name should
            // be unique at the every moment
            std::string agent_name = generateMlmClientId("web.asset_put");
            if (auto sent = sendConfigure(*(imported.at(1)), import.operation(), agent_name); !sent) {
                logError(sent.error());
                throw rest::errors::Internal(sent.error());
            }

            // no unexpected errors was detected
            // process results
            auto ret = db::idToNameExtName(imported.at(1)->id);
            if (!ret) {
                logError(ret.error());
                throw rest::errors::Internal(ret.error());
            }
            m_reply << "{\"id\": \"" << ret->first << "\"}";
            auditInfo("Request CREATE OR UPDATE asset id {} SUCCESS"_tr, *id);

            try {
                const auto& assetIname = ret.value().first;

                logDebug("Updating mappings for asset {}", assetIname);

                ExtMap map;
                getExtMapFromSi(si, map);

                // get list of secw credentials from ext map of asset
                auto credentialList = getCredentialMappings(map);

                cam::Accessor camAccessor(CAM_CLIENT_ID, CAM_TIMEOUT_MS, MALAMUTE_ENDPOINT);

                // get asset existing mappings
                auto existingMappings = camAccessor.getAssetMappings(assetIname);

                for(const auto& c :credentialList) {
                    auto mappingExists = std::find_if(existingMappings.begin(), existingMappings.end(), [&] (const cam::CredentialAssetMapping &mapping) {
                        // only one mapping can exist with same service ID and same protocol
                        return mapping.m_serviceId == c.serviceId && mapping.m_protocol == c.protocol;
                    });
                    if(mappingExists != existingMappings.end()) {
                        logDebug("Asset {} already mapped to {} : {}", assetIname, c.serviceId, c.protocol);
                        // if either credential ID or port changed, update the credential
                        if(mappingExists->m_credentialId != c.credentialId || mappingExists->m_port != c.port) {
                            cam::CredentialAssetMapping newMapping;
                            newMapping.m_assetId = assetIname;
                            newMapping.m_serviceId = c.serviceId;
                            newMapping.m_protocol = c.protocol;
                            newMapping.m_port = c.port;
                            newMapping.m_credentialId = c.credentialId;
                            newMapping.m_status = cam::Status::UNKNOWN;

                            logDebug("Mapping {} : {} updated", assetIname, c.serviceId, c.protocol);

                            camAccessor.updateMapping(newMapping);
                        }
                        // delete from existing mappings (remaining mappings will be deleted)
                        existingMappings.erase(mappingExists);
                    } else {
                        logDebug("Create new mapping to credential with ID {}", c.credentialId);
                        camAccessor.createMapping(assetIname, c.serviceId, c.protocol, c.port, c.credentialId, cam::Status::UNKNOWN /*, empty map */);
                    }
                }

                // if existingMappings is not empty -> some credentials were removed from asset and need to be deleted
                for(const auto& m : existingMappings) {
                    logDebug("Deleting unused mapping {} : {}", m.m_serviceId, m.m_protocol);
                    camAccessor.removeMapping(assetIname, m.m_serviceId, m.m_protocol);
                }
            } catch (const std::exception& e) {
                log_error("Failed to update CAM: %s", e.what());
            }
            return HTTP_OK;
        } else {
            auditError("Request CREATE OR UPDATE asset id {} FAILED: {}"_tr, *id, imported.at(1).error());
            throw rest::errors::Internal("Import failed: {}"_tr.format(imported.at(1).error()));
        }
    } else {
        throw rest::errors::Internal(res.error());
    }
}

} // namespace fty::asset

registerHandler(fty::asset::Edit)
