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
void hand(BattleContext& bc, CardId id) {
    bc.cards.cardsInHand = 2;
    bc.cards.hand[0] = CardInstance(id); bc.cards.hand[0].uniqueId = 98;
    bc.cards.hand[1] = CardInstance(CardId::STRIKE_RED); bc.cards.hand[1].uniqueId = 99;
    bc.player.energy = 3;
}
Json parsed(const std::ostringstream& output) { std::istringstream input(output.str()); return readTrace(input); }
Json actions(const Json& report) {
    auto result = Json::array();
    for (const auto& frame : report["frames"]) if (frame["kind"] == "action") result.push_back(frame);
    return result;
}
}

REPORT_TEST(recorder_one_execution) {
    auto f = report_test::twoLouses(); hand(f.bc, CardId::BASH);
    std::ostringstream output; TraceWriter writer(output, {{"seed", "1"}}); RunRecorder recorder(writer);
    recorder.startRun(f.gc); recorder.enterBattle(f.gc, f.bc);
    const int before = f.bc.monsters.arr[0].curHp;
    const auto action = search::Action(search::ActionType::CARD, 0, 0);
    recorder.recordBattleAction(f.bc, action); recorder.finishRun("paused", f.gc);
    CHECK(f.bc.monsters.arr[0].curHp == before - 8); CHECK(f.bc.player.energy == 1);
    const auto chosen = actions(parsed(output)); CHECK(chosen.size() == 1);
    CHECK(chosen[0]["action"]["bits"] == action.bits); CHECK(chosen[0]["action"]["card"]["cardId"] == "BASH");
}

REPORT_TEST(recorder_both_agents) {
    for (bool useSearch : {false, true}) {
        GameContext gc(CharacterClass::IRONCLAD, 1, 0);
        std::ostringstream output; TraceWriter writer(output, {{"seed", "1"}}); RunRecorder recorder(writer);
        if (useSearch) { search::ScumSearchAgent2 agent{}; agent.simulationCountBase = 5; agent.bossSimulationMultiplier = 1; agent.recorder = &recorder; agent.playout(gc); }
        else { search::SimpleAgent agent{}; agent.recorder = &recorder; agent.playout(gc); }
        const auto report = parsed(output);
        CHECK(report["frames"].size() > 3); CHECK(!actions(report).empty());
        CHECK(report["status"] == (gc.outcome == GameOutcome::PLAYER_VICTORY ? "victory" : "loss"));
        CHECK(report["frames"].back()["kind"] == "run_end");
    }
}

REPORT_TEST(recorder_combat_lifecycle) {
    auto f = report_test::twoLouses(); hand(f.bc, CardId::CLEAVE);
    f.gc.curRoom = Room::MONSTER;
    f.gc.enterBattle(MonsterEncounter::TWO_LOUSE);
    f.gc.regainControlAction = [](GameContext& gc) { gc.afterBattle(); };
    f.bc.player.curHp = 40; f.gc.curHp = 40;
    for (int i = 0; i < 2; ++i) f.bc.monsters.arr[i].curHp = 1;
    std::ostringstream output; TraceWriter writer(output, {{"seed", "1"}}); RunRecorder recorder(writer);
    recorder.startRun(f.gc); recorder.enterBattle(f.gc, f.bc);
    recorder.recordBattleAction(f.bc, search::Action(search::ActionType::CARD, 0));
    CHECK(f.bc.outcome == Outcome::PLAYER_VICTORY);
    recorder.finishBattle(f.bc); f.bc.exitBattle(f.gc); recorder.leaveBattle(f.gc); recorder.finishRun("paused", f.gc);
    const auto report = parsed(output); CHECK(report["frames"][1]["kind"] == "combat_start");
    CHECK(report["frames"][1]["state"]["combat"]["hand"].size() == 2);
    CHECK(actions(report).back()["state"]["combat"]["outcome"] == "victory");
    CHECK(report["frames"].back()["state"]["player"]["hp"] == 46);
}

REPORT_TEST(recorder_selection_continuation) {
    auto f = report_test::twoLouses(); hand(f.bc, CardId::ARMAMENTS);
    f.bc.cards.cardsInHand = 3;
    f.bc.cards.hand[2] = CardInstance(CardId::DEFEND_RED); f.bc.cards.hand[2].uniqueId = 100;
    std::ostringstream output; TraceWriter writer(output, {{"seed", "1"}}); RunRecorder recorder(writer);
    recorder.startRun(f.gc); recorder.enterBattle(f.gc, f.bc);
    recorder.recordBattleAction(f.bc, search::Action(search::ActionType::CARD, 0));
    CHECK(f.bc.inputState == InputState::CARD_SELECT);
    recorder.recordBattleAction(f.bc, search::Action(search::ActionType::SINGLE_CARD_SELECT, 0));
    recorder.finishRun("paused", f.gc);
    const auto chosen = actions(parsed(output)); CHECK(chosen.size() == 2);
    CHECK(chosen[1]["action"]["parentActionId"] == chosen[0]["id"]);
    CHECK(chosen[1]["action"]["card"]["cardId"] == "ARMAMENTS");
}

REPORT_TEST(recorder_paused) {
    GameContext gc(CharacterClass::IRONCLAD, 1, 0); gc.screenState = ScreenState::REWARDS;
    gc.info.rewardsContainer.clear(); gc.info.rewardsContainer.cardRewardCount = 1;
    gc.info.rewardsContainer.cardRewards[0].push_back(Card(CardId::BASH));
    std::ostringstream output; TraceWriter writer(output, {{"seed", "1"}}); RunRecorder recorder(writer);
    search::ScumSearchAgent2 agent{}; agent.pauseOnCardReward = true; agent.recorder = &recorder; agent.playout(gc);
    CHECK(parsed(output)["status"] == "paused");
}

REPORT_TEST(recorder_search_isolation) {
    auto f = report_test::twoLouses(); hand(f.bc, CardId::BASH);
    std::ostringstream output; TraceWriter writer(output, {{"seed", "1"}}); RunRecorder recorder(writer);
    recorder.startRun(f.gc); recorder.enterBattle(f.gc, f.bc); const auto start = output.str();
    for (int budget : {1, 100}) { search::BattleScumSearcher2 searcher(f.bc); searcher.search(budget); CHECK(output.str() == start); }
    recorder.recordBattleAction(f.bc, search::Action(search::ActionType::CARD, 0, 0)); recorder.finishRun("paused", f.gc);
    CHECK(actions(parsed(output)).size() == 1);
}

REPORT_TEST(recorder_null_path) {
    auto f = report_test::twoLouses(), reference = f; hand(f.bc, CardId::BASH); hand(reference.bc, CardId::BASH);
    search::ScumSearchAgent2 agent{}; CHECK(agent.recorder == nullptr);
    const auto a = search::Action(search::ActionType::CARD, 0, 0);
    const auto directAllocations = report_test::allocations([&] { a.execute(reference.bc); });
    const auto guardedAllocations = report_test::allocations([&] { agent.takeAction(f.bc, a); });
    CHECK(guardedAllocations == directAllocations);
    CHECK(f.bc.player.energy == reference.bc.player.energy);
    CHECK(report_test::rngState(f.bc) == report_test::rngState(reference.bc));
    CHECK(f.bc.monsters.arr[0].curHp == reference.bc.monsters.arr[0].curHp);
}
