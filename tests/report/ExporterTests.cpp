#include "TestSupport.h"
#include "sim/report/Trace.h"
#include "sim/report/AssetCatalog.h"
#include "sim/report/HtmlExporter.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <streambuf>
#include <iterator>

using namespace sts::report;
namespace {
namespace fs = std::filesystem;
struct TempDir {
    fs::path path = fs::temp_directory_path() / ("sts-export-test-" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
    TempDir() { fs::create_directories(path); }
    ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
};
const std::string png = std::string("\x89PNG\r\n\x1a\n", 8) + "fixture";
Json sampleFrame(Json state = Json::object()) {
    return {{"id", 0}, {"kind", "run_start"}, {"previousFrameId", nullptr}, {"act", 1}, {"floor", 0},
            {"combatId", nullptr}, {"turn", nullptr}, {"action", nullptr}, {"beforeScreen", nullptr},
            {"state", state}, {"effects", Json::array()}};
}
std::string makeTrace(Json state = Json::object(), Json catalog = Json::object(), Json run = Json{{"seed", "1"}}) {
    std::ostringstream out;
    TraceWriter writer(out, run);
    if (!catalog.empty()) writer.appendCatalog(catalog);
    writer.appendFrame(sampleFrame(state));
    writer.finish("victory");
    return out.str();
}
std::string makePartialTrace() {
    std::ostringstream out; TraceWriter writer(out, {{"seed", "1"}}); writer.appendFrame(sampleFrame()); return out.str();
}
Json embeddedReport(const std::string& html) {
    const auto startTag = html.find("id=\"report-data\">");
    CHECK(startTag != std::string::npos);
    const auto begin = html.find('>', startTag) + 1;
    const auto end = html.find("</script>", begin);
    CHECK(end != std::string::npos);
    return Json::parse(html.substr(begin, end - begin));
}
void writeManifest(const fs::path& path, Json assets) {
    std::ofstream out(path);
    out << Json{{"schemaVersion", 1}, {"assets", std::move(assets)},
                {"attribution", "Game artwork © Mega Crit; sourced from Slay the Spire Wiki"}}.dump();
}
int occurrences(const std::string& text, const std::string& needle) {
    int count = 0; std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) { ++count; pos += needle.size(); }
    return count;
}
struct FailingBuffer : std::streambuf {
    std::streamsize xsputn(const char*, std::streamsize) override { return 0; }
    int overflow(int) override { return traits_type::eof(); }
};
}

REPORT_TEST(export_self_contained) {
    TempDir tmp; fs::create_directories(tmp.path / "assets");
    { std::ofstream image(tmp.path / "assets/a.png", std::ios::binary); image.write(png.data(), png.size()); }
    const auto manifest = tmp.path / "manifest.json";
    writeManifest(manifest, Json{{"card/A", {{"path", "assets/a.png"}, {"source", "https://example.test/a.png"}}},
                                 {"card/A-alias", {{"path", "assets/a.png"}, {"source", "https://example.test/a.png"}}},
                                 {"card/Unused", {{"path", "assets/a.png"}, {"source", "https://example.test/a.png"}}}});
    const auto htmlTrace = makeTrace({{"screen", {{"assetKey", "card/A"}}}, {"combat", {{"played", {{"assetKey", "card/A-alias"}}}}}},
                                     {{"card/A", {{"name", "Card A"}}}, {"card/A-alias", {{"name", "Alias"}}}});
    std::istringstream trace(htmlTrace); std::ostringstream html;
    exportReport(trace, manifest, html);
    const auto value = html.str();
    CHECK(value.find("data:image/png;base64,") != std::string::npos);
    CHECK(occurrences(value, "data:image/png;base64,") == 1);
    CHECK(value.find("@@REPORT_JSON@@") == std::string::npos);
    CHECK(value.find("@@VIEWER_CSS@@") == std::string::npos);
    CHECK(value.find("@@VIEWER_JS@@") == std::string::npos);
    CHECK(value.find("application/json") != std::string::npos);
    CHECK(value.find("<img src=\"http") == std::string::npos);
    CHECK(value.find("<style>") != std::string::npos);
    CHECK(value.find("<script>") != std::string::npos);
    const auto embedded = embeddedReport(value);
    CHECK(embedded["assets"]["byKey"].size() == 2);
    CHECK(embedded["assets"]["byKey"]["card/A"] == embedded["assets"]["byKey"]["card/A-alias"]);
    CHECK(embedded["assets"]["blobs"].size() == 1);
}

REPORT_TEST(export_escape) {
    const std::string hostile = "</script><script>globalThis.pwned=1</script><img onerror=...>&\xE2\x80\xA8text\xE2\x80\xA9";
    const auto encoded = jsonForHtml(Json{{"text", hostile}});
    CHECK(encoded.find("\\u003c/script\\u003e") != std::string::npos);
    CHECK(encoded.find("\\u0026") != std::string::npos);
    CHECK(encoded.find("\\u2028") != std::string::npos);
    CHECK(encoded.find("\\u2029") != std::string::npos);
    CHECK(Json::parse(encoded)["text"] == hostile);
    CHECK(encoded.find("</script>") == std::string::npos);
}

REPORT_TEST(export_character_and_only_referenced_icons) {
    TempDir tmp;
    { std::ofstream image(tmp.path / "a.png", std::ios::binary); image.write(png.data(), png.size()); }
    const auto manifest = tmp.path / "manifest.json";
    writeManifest(manifest, {{"character/IRONCLAD", {{"path", "a.png"}, {"source", "https://example.test/character.png"}}},
                             {"status/WEAK", {{"path", "a.png"}, {"source", "https://example.test/weak.png"}}}});
    std::istringstream trace(makeTrace({{"characterAssetKey", "character/IRONCLAD"}}));
    std::ostringstream html; exportReport(trace, manifest, html);
    const auto embedded = embeddedReport(html.str());
    CHECK(embedded["assets"]["byKey"].contains("character/IRONCLAD"));
    CHECK(!embedded["assets"]["byKey"].contains("status/WEAK"));
}

REPORT_TEST(export_missing_art) {
    TempDir tmp; const auto manifest = tmp.path / "manifest.json";
    writeManifest(manifest, {{"status/WEAK", {{"path", "missing.png"}, {"source", "https://example.test/weak.png"}}}});
    std::istringstream trace(makeTrace({{"statuses", Json::array({{{"assetKey", "status/WEAK"}}})}}));
    std::ostringstream html; exportReport(trace, manifest, html);
    const auto value = html.str();
    CHECK(value.find("status/WEAK") != std::string::npos);
    CHECK(value.find("missing.png") == std::string::npos);
    CHECK(value.find("Missing artwork") != std::string::npos);
}

REPORT_TEST(export_supported_image_signatures) {
    TempDir tmp;
    const std::string jpeg("\xff\xd8\xff\x00", 4);
    const std::string webp("RIFF\x04\x00\x00\x00WEBPpayload", 19);
    { std::ofstream out(tmp.path / "a.jpg", std::ios::binary); out.write(jpeg.data(), jpeg.size()); }
    { std::ofstream out(tmp.path / "b.webp", std::ios::binary); out.write(webp.data(), webp.size()); }
    const auto manifest = tmp.path / "manifest.json";
    writeManifest(manifest, {{"card/J", {{"path", "a.jpg"}, {"source", "https://example.test/a.jpg"}}},
                              {"card/W", {{"path", "b.webp"}, {"source", "https://example.test/b.webp"}}}});
    const auto assets = embedReferencedAssets(loadAssetCatalog(manifest), {"card/J", "card/W"});
    CHECK(assets["blobs"].size() == 2);
    const auto jpegBlob = assets["byKey"]["card/J"].get<std::string>();
    const auto webpBlob = assets["byKey"]["card/W"].get<std::string>();
    CHECK(assets["blobs"][jpegBlob].get<std::string>().find("data:image/jpeg;base64,") == 0);
    CHECK(assets["blobs"][webpBlob].get<std::string>().find("data:image/webp;base64,") == 0);
}

REPORT_TEST(export_asset_validation) {
    TempDir tmp; const auto manifest = tmp.path / "manifest.json";
    writeManifest(manifest, Json::array()); CHECK_THROWS(loadAssetCatalog(manifest));
    writeManifest(manifest, {{"bad", {{"path", "../escape.png"}, {"source", "https://example.test/escape.png"}}}});
    CHECK_THROWS(loadAssetCatalog(manifest));
    writeManifest(manifest, {{"empty", {{"path", "missing.png"}, {"source", "https://example.test/missing.png"}}}});
    CHECK(loadAssetCatalog(manifest)["assets"].contains("empty"));

    const auto outside = tmp.path / "outside.svg";
    { std::ofstream file(outside); file << "<svg><script>alert(1)</script></svg>"; }
    const auto svgManifest = tmp.path / "svg.json";
    writeManifest(svgManifest, {{"active", {{"path", "outside.svg"}, {"source", "https://example.test/outside.svg"}}}});
    CHECK_THROWS(embedReferencedAssets(loadAssetCatalog(svgManifest), {"active"}));

    const auto manifestDir = tmp.path / "manifest-dir"; fs::create_directories(manifestDir);
    const auto link = manifestDir / "link.png";
    std::error_code ec; fs::create_symlink(outside, link, ec);
    if (!ec) {
        const auto linkManifest = manifestDir / "link.json";
        writeManifest(linkManifest, {{"link", {{"path", "link.png"}, {"source", "https://example.test/link.png"}}}});
        CHECK_THROWS(loadAssetCatalog(linkManifest));
    }
}

REPORT_TEST(export_unknown_schema_and_truncated_trace) {
    std::istringstream unsupported("{\"type\":\"header\",\"schemaVersion\":2,\"run\":{\"seed\":\"1\"}}\n");
    std::ostringstream output; CHECK_THROWS(exportReport(unsupported, {}, output));
    const auto partial = makePartialTrace();
    std::istringstream truncated(partial + "{\"type\":\"frame\"");
    exportReport(truncated, {}, output);
    const auto report = embeddedReport(output.str());
    CHECK(report["status"] == "interrupted");
    CHECK(report["frames"].size() == 1);
    CHECK(!report["warnings"].empty());
    CHECK(output.str().find("incomplete final trace record") != std::string::npos);
}

REPORT_TEST(export_stream_failure) {
    const std::string original = makeTrace();
    std::istringstream input(original); FailingBuffer buffer; std::ostream html(&buffer);
    CHECK_THROWS(exportReport(input, {}, html));
    CHECK(original.find("victory") != std::string::npos);
}
