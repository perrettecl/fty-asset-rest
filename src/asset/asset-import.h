#pragma once
#include "error.h"
#include "asset-db.h"
#include <map>
#include <set>
#include <fty_common_asset_types.h>

namespace tntdb {
class Connection;
}

namespace fty::asset {
class CsvMap;
struct LimitationsStruct;

namespace db {
    struct AssetLink;
}

class Import
{
public:
    Import(const CsvMap& cm);
    AssetExpected<void> process();
    const db::AssetElement& item() const;
    persist::asset_operation operation() const;

private:
    std::string                        mandatoryMissing() const;
    std::map<std::string, std::string> sanitizeRowExtNames(size_t row, bool sanitize) const;
    AssetExpected<void>                processRow(
                       size_t row, std::set<uint32_t>& ids, bool sanitize, const LimitationsStruct& limitations);
    uint16_t    getPriority(const std::string& s) const;
    bool        isDate(const std::string& key) const;
    std::string matchExtAttr(const std::string& value, const std::string& key) const;
    bool        checkUSize(const std::string& s) const;

    AssetExpected<void> updateDcRoomRowRackGroup(tntdb::Connection& conn, uint32_t elementId,
        const std::string& elementName, uint32_t parentId, const std::map<std::string, std::string>& extattributes,
        const std::string& status, uint16_t priority, const std::set<uint32_t>& groups, const std::string& assetTag,
        const std::map<std::string, std::string>& extattributesRO) const;

    AssetExpected<void> updateDevice(tntdb::Connection& conn, uint32_t elementId, const std::string& elementName,
        uint32_t parentId, const std::map<std::string, std::string>& extattributes, const std::string& status,
        uint16_t priority, const std::set<uint32_t>& groups, const std::vector<db::AssetLink>& links,
        const std::string& assetTag, const std::map<std::string, std::string>& extattributesRO) const;

    Expected<uint32_t> insertDcRoomRowRackGroup(tntdb::Connection& conn, const std::string& elementName,
        uint16_t elementTypeId, uint32_t parentId, const std::map<std::string, std::string>& extattributes,
        const std::string& status, uint16_t priority, const std::set<uint32_t>& groups, const std::string& assetTag,
        const std::map<std::string, std::string>& extattributesRO) const;

    Expected<uint32_t> insertDevice(tntdb::Connection& conn, const std::vector<db::AssetLink>& links,
        const std::set<uint32_t>& groups, const std::string& elementName, uint32_t parentId,
        const std::map<std::string, std::string>& extattributes, uint16_t assetDeviceTypeId, const std::string& status,
        uint16_t priority, const std::string& assetTag,
        const std::map<std::string, std::string>& extattributesRO) const;

private:
    const CsvMap& m_cm;
    db::AssetElement m_el;
    persist::asset_operation m_operation;
};

} // namespace fty::asset
