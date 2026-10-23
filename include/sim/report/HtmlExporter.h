#ifndef STS_REPORT_HTML_EXPORTER_H
#define STS_REPORT_HTML_EXPORTER_H
#include "sim/report/Trace.h"
#include <filesystem>
#include <istream>
#include <ostream>
namespace sts::report {
std::string jsonForHtml(const Json& value);
void exportReport(std::istream& trace, const std::filesystem::path& assetManifest, std::ostream& html);
}
#endif
