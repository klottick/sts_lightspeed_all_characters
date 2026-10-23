#ifndef STS_REPORT_REPORT_OPTIONS_H
#define STS_REPORT_REPORT_OPTIONS_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sts::report {
struct ReportOptions {
    std::uint64_t seed = 0;
    std::string seedDisplay;
    int ascension = 0;
    std::string agent = "search";
    std::int64_t simulationCount = 50000;
    std::filesystem::path output;
    std::filesystem::path assets;
};

ReportOptions parseReportOptions(const std::vector<std::string>& args);
}

#endif
