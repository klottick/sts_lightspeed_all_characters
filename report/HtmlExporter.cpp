#include "sim/report/HtmlExporter.h"
#include "sim/report/AssetCatalog.h"
#include "ReportResources.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace sts::report {
namespace {
void collectAssetKeys(const Json& value, std::set<std::string>& keys) {
    if (value.is_object()) {
        for (auto it = value.begin(); it != value.end(); ++it) {
            if ((it.key() == "assetKey" || it.key() == "characterAssetKey") && it.value().is_string()) keys.insert(it.value().get<std::string>());
            collectAssetKeys(it.value(), keys);
        }
    } else if (value.is_array()) for (const auto& item : value) collectAssetKeys(item, keys);
}

std::size_t count(const std::string& text, const std::string& marker) {
    std::size_t found = 0, pos = 0;
    while ((pos = text.find(marker, pos)) != std::string::npos) { ++found; pos += marker.size(); }
    return found;
}

void replaceAt(std::string& text, std::size_t position, const std::string& marker, const std::string& replacement) {
    text.replace(position, marker.size(), replacement);
}
}

std::string jsonForHtml(const Json& value) {
    auto output = value.dump(-1, ' ', false, Json::error_handler_t::replace);
    std::string safe; safe.reserve(output.size());
    for (std::size_t i = 0; i < output.size();) {
        if (output.compare(i, 3, "\xE2\x80\xA8", 3) == 0) { safe += "\\u2028"; i += 3; }
        else if (output.compare(i, 3, "\xE2\x80\xA9", 3) == 0) { safe += "\\u2029"; i += 3; }
        else {
            switch (output[i]) {
                case '<': safe += "\\u003c"; break;
                case '>': safe += "\\u003e"; break;
                case '&': safe += "\\u0026"; break;
                default: safe.push_back(output[i]); break;
            }
            ++i;
        }
    }
    return safe;
}

void exportReport(std::istream& trace, const std::filesystem::path& assetManifest, std::ostream& html) {
    Json report = readTrace(trace);
    Json catalog;
    std::set<std::string> keys;
    collectAssetKeys(report["catalog"], keys);
    collectAssetKeys(report["frames"], keys);
    for (const auto& frame : report["frames"]) {
        if (frame.contains("state") && frame["state"].contains("combat") && frame["state"]["combat"].is_object()) {
            keys.insert("ui/energy/red");
            keys.insert("status/BLOCK");
            break;
        }
    }
    if (assetManifest.empty()) {
        catalog = {{"schemaVersion", 1}, {"assets", Json::object()},
                   {"attribution", "Game artwork © Mega Crit; sourced from Slay the Spire Wiki"}};
    } else catalog = loadAssetCatalog(assetManifest);
    auto assets = embedReferencedAssets(catalog, keys);
    for (const auto& warning : assets["warnings"]) report["warnings"].push_back(warning);
    assets.erase("warnings");
    report["assets"] = std::move(assets);

    std::string document = resources::html;
    const std::array<std::string, 3> markers {"@@REPORT_JSON@@", "@@VIEWER_CSS@@", "@@VIEWER_JS@@"};
    for (const auto& marker : markers) if (count(document, marker) != 1) throw std::runtime_error("viewer template marker must occur exactly once: " + marker);
    std::vector<std::pair<std::size_t, std::pair<std::string, std::string>>> replacements;
    replacements.push_back({document.find(markers[0]), {markers[0], jsonForHtml(report)}});
    replacements.push_back({document.find(markers[1]), {markers[1], resources::css}});
    replacements.push_back({document.find(markers[2]), {markers[2], resources::js}});
    std::sort(replacements.begin(), replacements.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (const auto& replacement : replacements) replaceAt(document, replacement.first, replacement.second.first, replacement.second.second);
    html.write(document.data(), static_cast<std::streamsize>(document.size()));
    html.flush();
    if (!html) throw std::runtime_error("failed to write standalone HTML report");
}
}
