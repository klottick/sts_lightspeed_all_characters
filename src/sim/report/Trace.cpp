#include "sim/report/Trace.h"

#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <istream>
#include <ostream>

namespace sts::report {
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void validateRun(const Json& run) {
    require(run.is_object() && run.contains("seed") && run["seed"].is_string(), "trace seed must be a decimal string");
    const auto seed = run["seed"].get<std::string>();
    require(!seed.empty() && std::all_of(seed.begin(), seed.end(), [](unsigned char c) { return std::isdigit(c); }), "invalid decimal seed");
}
void validateStatus(const std::string& status) {
    require(status == "victory" || status == "loss" || status == "paused" || status == "interrupted", "invalid trace completion status");
}
void validateFrame(const Json& frame, std::uint64_t next) {
    require(frame.is_object() && frame.contains("id") && frame["id"].is_number_integer(), "frame requires an integer id");
    require(frame["id"] == next, "frame ids must be consecutive and start at zero");
    require(frame.contains("previousFrameId"), "frame requires a prior frame reference");
    require(next ? frame["previousFrameId"].is_number_integer() && frame["previousFrameId"] == next - 1 : frame["previousFrameId"].is_null(), "invalid prior frame reference");
    require(frame.contains("kind") && frame["kind"].is_string(), "frame requires a kind");
    require(frame.contains("state") && frame["state"].is_object(), "frame requires a state object");
    require(frame.contains("effects") && frame["effects"].is_array(), "frame effects must be an array");
}
}

TraceWriter::TraceWriter(std::ostream& out, const Json& run) : out_(out) {
    validateRun(run);
    write({{"type", "header"}, {"schemaVersion", 1}, {"run", run}});
}

void TraceWriter::write(const Json& record) {
    require(!finished_, "trace is already finished");
    out_ << record.dump() << '\n';
    out_.flush();
    require(static_cast<bool>(out_), "failed to write simulation trace");
}

void TraceWriter::appendCatalog(const Json& entries) {
    require(entries.is_object(), "catalog entries must be an object");
    write({{"type", "catalog"}, {"entries", entries}});
}

void TraceWriter::appendFrame(const Json& frame) {
    validateFrame(frame, nextId_);
    write({{"type", "frame"}, {"frame", frame}});
    ++nextId_;
}

void TraceWriter::finish(const std::string& status) {
    validateStatus(status);
    write({{"type", "footer"}, {"status", status}});
    finished_ = true;
}

Json readTrace(std::istream& input) {
    Json report{{"schemaVersion", 1}, {"catalog", Json::object()}, {"frames", Json::array()},
                {"status", "interrupted"}, {"warnings", Json::array()}};
    bool header = false, footer = false;
    std::string line;
    while (std::getline(input, line)) {
        require(!footer, "records found after trace footer");
        Json record;
        try { record = Json::parse(line); }
        catch (const Json::parse_error&) {
            if (!input.eof()) throw std::runtime_error("malformed complete trace record");
            report["warnings"].push_back("Ignored an incomplete final trace record.");
            break;
        }
        require(record.is_object() && record.contains("type") && record["type"].is_string(), "trace record requires a type");
        const auto type = record["type"].get<std::string>();
        if (!header) {
            require(type == "header", "trace must start with a header");
            require(record.contains("schemaVersion") && record["schemaVersion"].is_number_integer() && record["schemaVersion"] == 1, "unsupported trace schema version");
            validateRun(record.at("run"));
            report["run"] = record["run"];
            header = true;
        } else if (type == "catalog") {
            const auto& entries = record.at("entries");
            require(entries.is_object(), "catalog entries must be an object");
            for (auto it = entries.begin(); it != entries.end(); ++it) {
                require(!report["catalog"].contains(it.key()) || report["catalog"][it.key()] == it.value(), "conflicting catalog definition");
                report["catalog"][it.key()] = it.value();
            }
        } else if (type == "frame") {
            const auto& frame = record.at("frame");
            validateFrame(frame, report["frames"].size());
            report["frames"].push_back(frame);
        } else if (type == "footer") {
            const auto status = record.at("status").get<std::string>();
            validateStatus(status);
            report["status"] = status;
            footer = true;
        } else {
            throw std::runtime_error("unexpected trace record type: " + type);
        }
    }
    require(!input.bad(), "failed to read simulation trace");
    require(header, "trace has no complete header");
    if (!footer) report["warnings"].push_back("Recording is incomplete: no completion footer.");
    return report;
}
}
