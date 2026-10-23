#include "sim/report/AssetCatalog.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace sts::report {
namespace fs = std::filesystem;
namespace {
const char* attributionText = "Game artwork © Mega Crit; sourced from Slay the Spire Wiki";

bool within(const fs::path& child, const fs::path& root) {
    auto c = child.begin(), r = root.begin();
    for (; r != root.end(); ++r, ++c) if (c == child.end() || *c != *r) return false;
    return true;
}

std::string extensionFor(const std::string& data) {
    if (data.size() >= 8 && data.compare(0, 8, "\x89PNG\r\n\x1a\n", 8) == 0) return ".png";
    if (data.size() >= 3 && static_cast<unsigned char>(data[0]) == 0xff &&
        static_cast<unsigned char>(data[1]) == 0xd8 && static_cast<unsigned char>(data[2]) == 0xff) return ".jpg";
    if (data.size() >= 12 && data.compare(0, 4, "RIFF") == 0 && data.compare(8, 4, "WEBP") == 0) return ".webp";
    throw std::runtime_error("unsupported image signature (PNG, JPEG, or WebP required; SVG is not allowed)");
}

std::string mimeType(const std::string& extension) {
    if (extension == ".png") return "image/png";
    if (extension == ".jpg") return "image/jpeg";
    return "image/webp";
}

std::string base64(const std::string& data) {
    static constexpr char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out; out.reserve((data.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < data.size(); i += 3) {
        const auto a = static_cast<unsigned char>(data[i]);
        const auto b = i + 1 < data.size() ? static_cast<unsigned char>(data[i + 1]) : 0;
        const auto c = i + 2 < data.size() ? static_cast<unsigned char>(data[i + 2]) : 0;
        out.push_back(table[a >> 2]); out.push_back(table[((a & 3) << 4) | (b >> 4)]);
        out.push_back(i + 1 < data.size() ? table[((b & 15) << 2) | (c >> 6)] : '=');
        out.push_back(i + 2 < data.size() ? table[c & 63] : '=');
    }
    return out;
}

std::string safeRead(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read asset: " + path.string());
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}
}

Json loadAssetCatalog(const fs::path& manifestPath) {
    std::ifstream input(manifestPath);
    if (!input) throw std::runtime_error("cannot open asset manifest: " + manifestPath.string());
    Json value;
    try { input >> value; } catch (const std::exception& e) { throw std::runtime_error(std::string("malformed asset manifest: ") + e.what()); }
    if (!value.is_object() || !value.contains("schemaVersion") || !value["schemaVersion"].is_number_integer() || value["schemaVersion"] != 1 ||
        !value.contains("assets") || !value["assets"].is_object() || !value.contains("attribution") || !value["attribution"].is_string())
        throw std::runtime_error("invalid asset manifest schema");
    for (auto it = value["assets"].begin(); it != value["assets"].end(); ++it) {
        const auto& item = it.value();
        if (!item.is_object() || !item.contains("path") || !item["path"].is_string() || !item.contains("source") || !item["source"].is_string())
            throw std::runtime_error("invalid asset entry: " + it.key());
        const fs::path relative(item["path"].get<std::string>());
        if (relative.empty() || relative.is_absolute()) throw std::runtime_error("asset path must be relative: " + it.key());
        for (const auto& component : relative) if (component == "..") throw std::runtime_error("asset path escapes manifest directory: " + it.key());
        const std::string source = item["source"].get<std::string>();
        if (source.rfind("https://", 0) != 0 || source.find('\n') != std::string::npos) throw std::runtime_error("asset source must be an HTTPS URL: " + it.key());
    }
    std::error_code ec;
    const auto root = fs::canonical(manifestPath.parent_path().empty() ? fs::current_path() : manifestPath.parent_path(), ec);
    if (ec) throw std::runtime_error("cannot resolve asset manifest directory: " + ec.message());
    for (auto it = value["assets"].begin(); it != value["assets"].end(); ++it) {
        const auto candidate = fs::weakly_canonical(root / it.value()["path"].get<std::string>(), ec);
        if (ec || !within(candidate, root)) throw std::runtime_error("asset path escapes manifest directory: " + it.key());
    }
    value["_root"] = root.string();
    return value;
}

Json embedReferencedAssets(const Json& catalog, const std::set<std::string>& requestedKeys) {
    if (!catalog.is_object() || !catalog.contains("assets") || !catalog["assets"].is_object()) throw std::runtime_error("invalid loaded asset catalog");
    const fs::path root = catalog.value("_root", std::string());
    Json result{{"byKey", Json::object()}, {"blobs", Json::object()}, {"sources", Json::array()},
                {"attribution", catalog.value("attribution", std::string(attributionText))}, {"warnings", Json::array()}};
    std::set<std::string> keys = requestedKeys;
    std::map<std::string, std::string> blobForBytes;
    int nextBlob = 0;
    for (const auto& key : keys) {
        if (!catalog["assets"].contains(key)) { result["warnings"].push_back("No local asset is cataloged for " + key + "."); continue; }
        const auto& entry = catalog["assets"][key];
        const fs::path path = root / entry["path"].get<std::string>();
        std::error_code ec;
        if (!fs::exists(path, ec)) { result["warnings"].push_back("Missing artwork for " + key + "."); continue; }
        const auto resolved = fs::canonical(path, ec);
        if (ec || !within(resolved, root) || !fs::is_regular_file(resolved)) throw std::runtime_error("unsafe or non-file asset path: " + key);
        const auto bytes = safeRead(resolved);
        const auto extension = extensionFor(bytes);
        auto found = blobForBytes.find(bytes);
        std::string blobId;
        if (found == blobForBytes.end()) {
            blobId = "blob-" + std::to_string(nextBlob++);
            blobForBytes.emplace(bytes, blobId);
            result["blobs"][blobId] = "data:" + mimeType(extension) + ";base64," + base64(bytes);
        } else blobId = found->second;
        result["byKey"][key] = blobId;
        result["sources"].push_back({{"key", key}, {"source", entry["source"]}});
    }
    return result;
}
}
