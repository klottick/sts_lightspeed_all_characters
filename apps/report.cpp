#include "sim/report/AssetCatalog.h"
#include "sim/report/HtmlExporter.h"
#include "sim/report/ReportOptions.h"
#include "sim/report/RunRecorder.h"
#include "sim/search/SimpleAgent.h"
#include "sim/search/ScumSearchAgent2.h"
#include "game/Game.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace sts;
using namespace sts::report;

namespace {
void requireAbsent(const fs::path& path) {
    std::error_code ec;
    const auto status = fs::symlink_status(path, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) throw std::runtime_error("cannot inspect destination: " + path.string());
    if (fs::exists(status)) throw std::runtime_error("destination already exists (not overwritten): " + path.string());
}

// Same-directory hard links publish without replacing an existing destination.
// Only the two files created inside this exclusively-owned directory are cleaned.
class OutputFiles {
public:
    fs::path output, trace, working, temporary;
    explicit OutputFiles(const fs::path& requested) : output(fs::absolute(requested).lexically_normal()),
        trace(output.string() + ".trace.jsonl"), working(output.string() + ".working"), temporary(working / "report.html") {
        if (!fs::is_directory(output.parent_path())) throw std::runtime_error("output directory does not exist");
        requireAbsent(output); requireAbsent(trace); requireAbsent(working);
        if (!fs::create_directory(working)) throw std::runtime_error("cannot reserve report working directory");
        try {
            { std::ofstream seed(working / "trace", std::ios::binary); if (!seed) throw std::runtime_error("output directory is not writable"); }
            fs::create_hard_link(working / "trace", trace);
        } catch (...) { cleanup(); throw; }
    }
    ~OutputFiles() { cleanup(); }
    void publish() { fs::create_hard_link(temporary, output); }
private:
    void cleanup() noexcept {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        fs::remove(working / "trace", ignored);
        fs::remove(working, ignored);
    }
};

void preflightAssets(const fs::path& path) {
    if (path.empty()) return;
    const auto catalog = loadAssetCatalog(path);
    std::set<std::string> keys;
    for (auto it = catalog["assets"].begin(); it != catalog["assets"].end(); ++it) keys.insert(it.key());
    // Detect unsafe/active image content before spending time on a simulation.
    embedReferencedAssets(catalog, keys);
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "usage: sts-report --seed GAME_SEED --output run.html [--ascension 0..20]\n"
                     "                  [--agent simple|search] [--simulations N] [--assets manifest.json]\n"
                     "Creates a standalone read-only report and a recoverable .trace.jsonl sidecar.\n"
                     "Existing reports/traces are never overwritten. No network access is required.\n";
        return 0;
    }
    try {
        const auto options = parseReportOptions(std::vector<std::string>(argv + 1, argv + argc));
        if (options.agent == "search" && options.simulationCount > std::numeric_limits<int>::max() / 3)
            throw std::invalid_argument("--simulations exceeds the engine's integer range including the boss multiplier");
        preflightAssets(options.assets);
        OutputFiles files(options.output);
        std::ofstream trace(files.trace, std::ios::binary);
        if (!trace) throw std::runtime_error("cannot open reserved trace output");
        Json settings = Json::object();
        if (options.agent == "search") settings = {{"simulationCount", options.simulationCount}, {"bossSimulationMultiplier", 3},
                                                   {"stepsWithSolution", 15}, {"stepsNoSolution", 5}};
        TraceWriter writer(trace, {{"seed", std::to_string(options.seed)}, {"seedDisplay", options.seedDisplay}, {"character", "IRONCLAD"},
                                   {"ascension", options.ascension}, {"agent", options.agent}, {"settings", settings}, {"schemaVersion", 1}});
        RunRecorder recorder(writer);
        GameContext gc(CharacterClass::IRONCLAD, options.seed, options.ascension);
        std::string interrupted;
        std::cout << "Recording seed " << options.seedDisplay << " with " << options.agent << " agent\n" << std::flush;
        try {
            if (options.agent == "simple") {
                search::SimpleAgent agent{}; agent.recorder = &recorder; agent.playout(gc);
            } else {
                search::ScumSearchAgent2 agent{};
                agent.simulationCountBase = static_cast<int>(options.simulationCount);
                agent.rng = std::default_random_engine(options.seed);
                agent.recorder = &recorder; agent.playout(gc);
            }
        } catch (const std::exception& ex) {
            interrupted = ex.what();
            try { recorder.finishRun("interrupted", gc); } catch (const std::exception&) { /* Keep complete trace lines. */ }
        }
        trace.close();
        if (!trace) throw std::runtime_error("failed to close trace; complete records retained at " + files.trace.string());
        std::ifstream input(files.trace, std::ios::binary);
        std::ofstream html(files.temporary, std::ios::binary);
        if (!input || !html) throw std::runtime_error("cannot open trace or temporary report for export");
        exportReport(input, options.assets, html);
        html.close();
        if (!html) throw std::runtime_error("failed to close standalone report");
        files.publish();
        if (!interrupted.empty()) {
            std::cerr << "error: recording interrupted: " << interrupted << "; partial report: " << files.output << '\n';
            return 1;
        }
        std::cout << "Report written: " << files.output << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << '\n';
        return 1;
    }
}
