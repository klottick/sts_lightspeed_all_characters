#include "TestSupport.h"
#include "sim/report/Trace.h"

#include <sstream>

using namespace sts::report;
namespace {
Json frame(int id = 0) {
    return {{"id", id}, {"kind", "run_start"}, {"previousFrameId", id ? Json(id - 1) : Json(nullptr)},
            {"act", 1}, {"floor", 0}, {"combatId", nullptr}, {"turn", nullptr}, {"action", nullptr},
            {"beforeScreen", nullptr}, {"state", {{"screen", {{"kind", "event"}}}}}, {"effects", Json::array()}};
}
std::string header(int version = 1) {
    return Json{{"type", "header"}, {"schemaVersion", version}, {"run", {{"seed", "1"}}}}.dump() + "\n";
}
std::string record(const Json& value) { return Json{{"type", "frame"}, {"frame", value}}.dump() + "\n"; }
Json read(const std::string& text) { std::istringstream input(text); return readTrace(input); }
}

REPORT_TEST(trace_round_trip) {
    std::ostringstream out;
    TraceWriter writer(out, {{"seed", "18446744073709551615"}, {"agent", "simple"}});
    writer.appendCatalog({{"card/BASH", {{"name", "Bash"}}}});
    writer.appendFrame(frame());
    writer.finish("paused");
    auto report = read(out.str());
    CHECK(report["schemaVersion"] == 1);
    CHECK(report["run"]["seed"] == "18446744073709551615");
    CHECK(report["frames"].size() == 1);
    CHECK(report["frames"][0]["id"] == 0);
    CHECK(report["catalog"]["card/BASH"]["name"] == "Bash");
    CHECK(report["status"] == "paused");
    CHECK(report["warnings"].empty());
}

REPORT_TEST(trace_partial_and_invalid) {
    auto report = read(header() + record(frame()));
    CHECK(report["status"] == "interrupted");
    CHECK(report["frames"].size() == 1);
    report = read(header() + record(frame()) + "{\"type\":\"frame\"");
    CHECK(report["frames"].size() == 1);
    CHECK(!report["warnings"].empty());
    CHECK_THROWS(read(header() + "broken\n" + record(frame())));
    CHECK_THROWS(read(header() + record(frame()) + record(frame())));
    CHECK_THROWS(read(header(2)));
    CHECK_THROWS(read(""));
    CHECK_THROWS(read(Json{{"type", "header"}, {"schemaVersion", 1}, {"run", {{"seed", 42}}}}.dump() + "\n"));
    auto badPrevious = frame(1); badPrevious["previousFrameId"] = 9;
    CHECK_THROWS(read(header() + record(frame()) + record(badPrevious)));
    CHECK_THROWS(read(header() + "{\"type\":\"footer\",\"status\":\"victory\"}\n" + record(frame())));
}

REPORT_TEST(trace_stream_failure) {
    std::ostringstream out;
    TraceWriter writer(out, {{"seed", "1"}});
    out.setstate(std::ios::badbit);
    CHECK_THROWS(writer.appendFrame(frame()));
    CHECK(out.str().find("victory") == std::string::npos);
    auto partial = read(out.str());
    CHECK(partial["status"] == "interrupted");
}

REPORT_TEST(trace_writer_validation) {
    std::ostringstream out;
    TraceWriter writer(out, {{"seed", "1"}});
    CHECK_THROWS(writer.appendFrame(frame(1)));
    writer.appendFrame(frame());
    CHECK_THROWS(writer.appendFrame(frame()));
    CHECK_THROWS(writer.finish("unknown"));
    writer.finish("loss");
    CHECK_THROWS(writer.appendCatalog(Json::object()));
    CHECK_THROWS(writer.finish("victory"));
}
