#include "sim/report/RunRecorder.h"
#include "sim/report/CombatObserver.h"
#include "sim/report/ScreenAdapter.h"
#include "game/GameContext.h"
#include "combat/BattleContext.h"
#include <algorithm>
#include <stdexcept>
namespace sts::report {
void RunRecorder::append(const std::string& kind, Json state, Json action, Json effects, Json beforeScreen) {
    if (finished_) throw std::runtime_error("recording is already finished");
    const auto catalog = adapter_.catalogDelta();
    if (!catalog.empty()) writer_.appendCatalog(catalog);
    const auto combat = state.value("combat", Json(nullptr));
    writer_.appendFrame({{"id", nextId_}, {"kind", kind}, {"previousFrameId", nextId_ ? Json(nextId_ - 1) : Json(nullptr)},
                         {"act", state.value("act", 0)}, {"floor", state.value("floor", 0)},
                         {"combatId", combat.is_object() ? combat["id"] : Json(nullptr)}, {"turn", combat.is_object() ? combat["turn"] : Json(nullptr)},
                         {"action", std::move(action)}, {"beforeScreen", std::move(beforeScreen)}, {"state", state}, {"effects", std::move(effects)}});
    ++nextId_;
    state_ = std::move(state);
}

Json RunRecorder::gameChanges(const Json& before, const Json& after) const {
    auto effects = Json::array();
    for (const auto* field : {"hp", "maxHp", "gold"}) {
        if (!before.contains("player") || before["player"][field] == after["player"][field]) continue;
        effects.push_back({{"sequence", effects.size()}, {"kind", "state_change"}, {"source", nullptr}, {"target", "player"}, {"field", field},
                           {"before", before["player"][field]}, {"after", after["player"][field]},
                           {"description", std::string(field) + ": " + before["player"][field].dump() + " → " + after["player"][field].dump()}});
    }
    for (const auto* field : {"deck", "relics", "potions"}) {
        if (!before.contains("inventory") || before["inventory"][field] == after["inventory"][field]) continue;
        effects.push_back({{"sequence", effects.size()}, {"kind", "inventory_change"}, {"source", nullptr}, {"target", "player"}, {"field", field},
                           {"before", before["inventory"][field]}, {"after", after["inventory"][field]}, {"description", std::string(field) + " changed"}});
    }
    return effects;
}

void RunRecorder::startRun(const GameContext& gc) {
    if (started_) throw std::runtime_error("recorder can only record one run");
    started_ = true;
    runState_ = adapter_.game(gc);
    append("run_start", runState_);
}

Json RunRecorder::battleState(const BattleContext& bc) {
    auto state = adapter_.battle(runState_, bc);
    state["combat"]["lastPlayedCard"] = lastCard_;
    return state;
}

void RunRecorder::enterBattle(const GameContext& gc, BattleContext& bc) {
    if (!started_) throw std::runtime_error("run must start before combat");
    runState_ = adapter_.game(gc);
    identities_.beginCombat();
    lastCard_ = nullptr; pendingCard_ = nullptr; pendingActionId_ = nullptr;
    inBattle_ = true;
    append("combat_start", battleState(bc));
}

void RunRecorder::recordGameAction(GameContext& gc, search::GameAction choice) {
    const auto before = adapter_.game(gc);
    auto action = describeGameAction(gc, choice, identities_);
    choice.execute(gc);
    runState_ = adapter_.game(gc);
    append("action", runState_, std::move(action), gameChanges(before, runState_), before["screen"]);
}

void RunRecorder::recordBattleAction(BattleContext& bc, search::Action choice) {
    auto action = describeBattleAction(bc, choice, identities_);
    const auto beforeScreen = state_["screen"];
    if (choice.getActionType() == search::ActionType::CARD && !action["card"].is_null()) {
        lastCard_ = action["card"]; pendingCard_ = lastCard_; pendingActionId_ = nextId_;
    } else if (!pendingCard_.is_null() && bc.inputState == InputState::CARD_SELECT) {
        action["card"] = pendingCard_; action["parentActionId"] = pendingActionId_;
    }
    CombatObserver observer(bc, identities_);
    Json effects;
    {
        ScopedCombatObserver scope(observer);
        choice.execute(bc);
        effects = observer.takeEffects();
    }
    for (const auto& effect : effects) {
        if (!effect.contains("target") || !effect["target"].is_string()) continue;
        const auto target = effect["target"];
        if (std::find(action["targets"].begin(), action["targets"].end(), target) == action["targets"].end()) action["targets"].push_back(target);
    }
    append("action", battleState(bc), std::move(action), std::move(effects), beforeScreen);
    if (bc.inputState != InputState::CARD_SELECT) { pendingCard_ = nullptr; pendingActionId_ = nullptr; }
}

void RunRecorder::finishBattle(const BattleContext& bc) {
    auto final = battleState(bc);
    if (final != state_) append("combat_end", std::move(final));
}

void RunRecorder::leaveBattle(const GameContext& gc) {
    inBattle_ = false;
    runState_ = adapter_.game(gc);
    append("run_state", runState_, nullptr, gameChanges(state_, runState_));
}

void RunRecorder::finishRun(const std::string& status, const GameContext& gc) {
    if (!started_ || finished_) throw std::runtime_error("run is not recording");
    append("run_end", inBattle_ ? state_ : adapter_.game(gc));
    writer_.finish(status);
    finished_ = true;
}
}
