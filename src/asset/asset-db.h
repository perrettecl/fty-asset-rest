#pragma once
#include <fty/expected.h>
#include <map>
#include <set>
#include <vector>
#include <functional>

namespace tntdb {
class Connection;
class Row;
}

namespace fty::asset::db {

struct AssetElement
{
public:
    uint32_t                           id = 0;
    std::string                        name;
    std::string                        status;
    uint32_t                           parentId  = 0;
    uint16_t                           priority  = 0;
    uint16_t                           typeId    = 0;
    uint16_t                           subtypeId = 0;
    std::string                        assetTag;
    std::map<std::string, std::string> ext;
};

struct WebAssetElement : public AssetElement
{
    using AssetElement::AssetElement;

    std::string typeName;
    uint16_t    parentTypeId;
    std::string subtypeName;
    std::string parentName;
};

struct AssetLink
{
    uint32_t    src;    //!< id of src element
    uint32_t    dest;   //!< id of dest element
    std::string srcOut; //!< outlet in src
    std::string destIn; //!< inlet in dest
    uint16_t    type;   //!< link type id
};


Expected<int64_t> nameToAssetId(const std::string& assetName);
Expected<std::pair<std::string, std::string>> idToNameExtName(uint32_t assetId);

Expected<std::map<std::string, int>> readElementTypes();
Expected<std::map<std::string, int>> readDeviceTypes();

Expected<std::map<std::string, int>> getDictionary(const std::string& stStr);


Expected<std::string>                         extNameToAssetName(const std::string& assetExtName);
Expected<int64_t>                             extNameToAssetId(const std::string& assetExtName);

Expected<AssetElement>    selectAssetElementByName(const std::string& elementName);
Expected<WebAssetElement> selectAssetElementWebById(uint32_t elementId);

Expected<uint>    updateAssetElement(tntdb::Connection& conn, uint32_t elementId, const std::string& elementName,
       uint32_t parentId, const std::string& status, uint16_t priority, const std::string& assetTag);
Expected<int64_t> insertIntoAssetElement(tntdb::Connection& conn, const std::string& elementName,
    uint16_t elementTypeId, uint32_t parentId, const std::string& status, uint16_t priority, uint16_t subtypeId,
    const std::string& assetTag, bool update);

Expected<uint> deleteAssetExtAttributesWithRo(tntdb::Connection& conn, uint32_t assetElementId, bool readOnly);
Expected<uint> insertIntoAssetExtAttributes(
    tntdb::Connection& conn, uint32_t elementId, const std::map<std::string, std::string>& attributes, bool readOnly);
Expected<uint> deleteAssetElementFromAssetGroups(tntdb::Connection& conn, uint32_t assetElementId);
Expected<uint> insertElementIntoGroups(
    tntdb::Connection& conn, std::set<uint32_t> const& groups, uint32_t assetElementId);

Expected<uint>    deleteAssetLinksTo(tntdb::Connection& conn, uint32_t assetDeviceId);
Expected<int64_t> insertIntoAssetLink(tntdb::Connection& conn, uint32_t assetElementSrcId, uint32_t assetElementDestId,
    uint16_t linkTypeId, const std::string& srcOut, const std::string& destIn);
Expected<uint>    insertIntoAssetLinks(tntdb::Connection& conn, const std::vector<AssetLink>& links);

Expected<uint16_t> insertIntoMonitorDevice(
    tntdb::Connection& conn, uint16_t deviceTypeId, const std::string& deviceName);
Expected<int64_t>  insertIntoMonitorAssetRelation(tntdb::Connection& conn, uint16_t monitorId, uint32_t elementId);
Expected<uint16_t> selectMonitorDeviceTypeId(tntdb::Connection& conn, const std::string& deviceTypeName);

Expected<void> selectAssetElementSuperParent(
    tntdb::Connection& conn, uint32_t id, std::function<void(const tntdb::Row&)>& cb);

Expected<void> selectAssetsByContainer(tntdb::Connection& conn, uint32_t elementId, std::vector<uint16_t> types,
    std::vector<uint16_t> subtypes, const std::string& without, const std::string& status,
    std::function<void(const tntdb::Row&)>&& cb);
} // namespace fty::asset::db
