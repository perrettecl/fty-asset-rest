#pragma once
#include <fty/expected.h>
#include <string>
#include "error.h"


namespace fty::asset {

AssetExpected<uint32_t> checkElementIdentifier(const std::string& paramName, const std::string& paramValue);
AssetExpected<std::string> sanitizeDate(const std::string& inp);
AssetExpected<double> sanitizeValueDouble(const std::string& key, const std::string& value);

} // namespace fty::asset
