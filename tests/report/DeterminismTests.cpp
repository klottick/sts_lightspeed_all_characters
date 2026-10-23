#include "TestSupport.h"
#include "EngineFixtures.h"
#include "sim/report/RunRecorder.h"
#include "sim/search/SimpleAgent.h"
#include "sim/search/ScumSearchAgent2.h"
#include "sim/search/BattleScumSearcher2.h"
#include <sstream>

using namespace sts;
using namespace sts::report;
namespace {
struct CapturedOutput {
    std::ostringstream output;
    std::streambuf* previous = std::cout.rdbuf(output.rdbuf());
    ~CapturedOutput() { std::cout.rdbuf(previous); }
};
Json display(const GameContext& gc) {
    IdentityRegistry ids; SnapshotAdapter adapter(ids);
    return adapter.game(gc);
}
Json recorded(std::ostringstream& output) { std::istringstream input(output.str()); return readTrace(input); }
}

REPORT_TEST(determinism_both_agents) {
    for (std::uint64_t seed : {1, 2, 3}) {
        GameContext plain(CharacterClass::IRONCLAD, seed, 0), observed(CharacterClass::IRONCLAD, seed, 0);
        std::ostringstream trace; TraceWriter writer(trace, {{"seed", std::to_string(seed)}}); RunRecorder recorder(writer);
        search::SimpleAgent a{}, b{}; b.recorder = &recorder;
        a.playout(plain); b.playout(observed);
        if (a.actionHistory != b.actionHistory) {
            std::size_t first = 0;
            while (first < a.actionHistory.size() && first < b.actionHistory.size() && a.actionHistory[first] == b.actionHistory[first]) ++first;
            std::cerr << "seed " << seed << " first divergence " << first << " lengths " << a.actionHistory.size() << '/' << b.actionHistory.size() << '\n';
            if (first < a.actionHistory.size() && first < b.actionHistory.size()) std::cerr << a.actionHistory[first] << " != " << b.actionHistory[first] << '\n';
        }
        CHECK(a.actionHistory == b.actionHistory);
        CHECK(plain.outcome == observed.outcome); CHECK(display(plain) == display(observed));
        CHECK(report_test::rngState(plain) == report_test::rngState(observed));
        std::vector<int> bits;
        const auto report = recorded(trace);
        for (const auto& frame : report["frames"]) if (frame["kind"] == "action") bits.push_back(frame["action"]["bits"].get<int>());
        CHECK(bits == a.actionHistory);
        for (bool logging : {false, true}) {
            plain = GameContext(CharacterClass::IRONCLAD, seed, 0);
            observed = GameContext(CharacterClass::IRONCLAD, seed, 0);
            std::ostringstream searchTrace; TraceWriter searchWriter(searchTrace, {{"seed", std::to_string(seed)}}); RunRecorder searchRecorder(searchWriter);
            search::ScumSearchAgent2 x{}, y{};
            x.simulationCountBase = y.simulationCountBase = 10;
            x.rng = y.rng = std::default_random_engine(seed);
            x.printActions = y.printActions = logging; y.recorder = &searchRecorder;
            std::string xLog, yLog;
            { CapturedOutput capture; x.playout(plain); xLog = capture.output.str(); }
            { CapturedOutput capture; y.playout(observed); yLog = capture.output.str(); }
            CHECK(xLog == yLog); CHECK(x.gameActionHistory == y.gameActionHistory);
            CHECK(x.rng == y.rng); CHECK(plain.outcome == observed.outcome);
            CHECK(display(plain) == display(observed)); CHECK(report_test::rngState(plain) == report_test::rngState(observed));
            CHECK(x.simulationCountTotal == y.simulationCountTotal);
        }
    }
}

REPORT_TEST(determinism_battle_boundaries) {
    auto plain = report_test::twoLouses(), observed = plain;
    std::ostringstream trace; TraceWriter writer(trace, {{"seed", "1"}}); RunRecorder recorder(writer);
    recorder.startRun(observed.gc); recorder.enterBattle(observed.gc, observed.bc);
    for (int i = 0; i < 4 && plain.bc.outcome == Outcome::UNDECIDED; ++i) {
        search::BattleScumSearcher2 enumerator(plain.bc);
        search::BattleScumSearcher2::Node choices;
        enumerator.enumerateActionsForNode(choices, plain.bc);
        CHECK(!choices.edges.empty());
        const auto action = choices.edges.front().action;
        action.execute(plain.bc); recorder.recordBattleAction(observed.bc, action);
        CHECK(report_test::rngState(plain.bc) == report_test::rngState(observed.bc));
        IdentityRegistry ids; ids.beginCombat(); SnapshotAdapter adapter(ids);
        CHECK(adapter.battle(adapter.game(plain.gc), plain.bc) == adapter.battle(adapter.game(observed.gc), observed.bc));
    }
}
