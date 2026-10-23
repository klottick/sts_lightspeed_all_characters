#ifndef STS_REPORT_ASSET_CATALOG_H
#define STS_REPORT_ASSET_CATALOG_H
#include "sim/report/Trace.h"
#include <filesystem>
#include <set>
namespace sts::report {
Json loadAssetCatalog(const std::filesystem::path& path);
Json embedReferencedAssets(const Json& catalog, const std::set<std::string>& keys);
}
#endif
