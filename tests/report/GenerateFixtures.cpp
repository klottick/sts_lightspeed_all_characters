#include "EngineFixtures.h"
#include "sim/report/HtmlExporter.h"
#include "sim/report/CombatObserver.h"
#include "sim/report/ScreenAdapter.h"
#include "sim/report/SnapshotAdapter.h"
#include "sim/search/Action.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace sts;
using namespace sts::report;

namespace {
void writeReport(const fs::path& output, const std::string& status,
                 const std::function<void(TraceWriter&, SnapshotAdapter&, IdentityRegistry&, GameContext&, BattleContext&)>& build) {
    if (fs::exists(output)) throw std::runtime_error("refusing to overwrite " + output.string());
    const auto tracePath = output.string() + ".trace.jsonl";
    if (fs::exists(tracePath)) throw std::runtime_error("refusing to overwrite " + tracePath);
    std::ofstream trace(tracePath, std::ios::binary);
    if (!trace) throw std::runtime_error("cannot create trace " + tracePath);
    GameContext gc(CharacterClass::IRONCLAD, 1, 0);
    gc.info = ScreenStateInfo{};
    gc.screenState = ScreenState::MAP_SCREEN;
    BattleContext bc;
    IdentityRegistry identities;
    identities.beginCombat();
    bc.init(gc, MonsterEncounter::TWO_LOUSE);
    SnapshotAdapter adapter(identities);
    TraceWriter writer(trace, {{"seed", "1"}, {"seedDisplay", "1"}, {"character", "IRONCLAD"},
                               {"ascension", 0}, {"agent", "controlled fixture"}, {"schemaVersion", 1}});
    build(writer, adapter, identities, gc, bc);
    writer.finish(status);
    trace.close();
    if (!trace) throw std::runtime_error("failed closing trace " + tracePath);
    std::ifstream input(tracePath, std::ios::binary);
    std::ofstream html(output, std::ios::binary);
    if (!input || !html) throw std::runtime_error("cannot open fixture export paths");
    exportReport(input, {}, html);
    html.close();
    if (!html) throw std::runtime_error("failed closing fixture report " + output.string());
}

void append(TraceWriter& writer, std::uint64_t id, const std::string& kind, Json state,
            Json action = nullptr, Json effects = Json::array()) {
    writer.appendFrame({{"id", id}, {"kind", kind}, {"previousFrameId", id ? Json(id - 1) : Json(nullptr)},
                        {"act", state.value("act", 1)}, {"floor", state.value("floor", 1)},
                        {"combatId", state.contains("combat") && state["combat"].is_object() ? state["combat"].value("id", Json(nullptr)) : Json(nullptr)},
                        {"turn", state.contains("combat") && state["combat"].is_object() ? state["combat"].value("turn", Json(nullptr)) : Json(nullptr)},
                        {"action", std::move(action)}, {"beforeScreen", nullptr}, {"state", std::move(state)}, {"effects", std::move(effects)}});
}

void controlledCombat(TraceWriter& writer, SnapshotAdapter& adapter, IdentityRegistry& identities, GameContext& gc, BattleContext& bc) {
    gc.curRoom = Room::MONSTER;
    gc.curHp = 55;
    gc.screenState = ScreenState::BATTLE;
    bc.player.curHp = 55;
    bc.player.block = 5;
    bc.cards.cardsInHand = 3;
    bc.cards.hand[0] = CardInstance(CardId::CLEAVE); bc.cards.hand[0].uniqueId = 701;
    bc.cards.hand[1] = CardInstance(CardId::BASH); bc.cards.hand[1].uniqueId = 702;
    bc.cards.hand[2] = CardInstance(CardId::SEARING_BLOW); bc.cards.hand[2].uniqueId = 703;
    bc.cards.hand[2].specialData = 3;
    bc.player.energy = 4;
    for (int slot = 0; slot < 2; ++slot) bc.monsters.arr[slot].curHp = 1;
    auto state = adapter.game(gc);
    append(writer, 0, "run_start", state);
    state = adapter.battle(state, bc);
    append(writer, 1, "combat_start", state);

    // These explicit state mutations model a known 7-point hit absorbed by 5 Block,
    // followed by a 5 HP heal. Observer hooks record the exact mutation values.
    CombatObserver directObserver(bc, identities);
    Json directEffects;
    {
        ScopedCombatObserver scope(directObserver);
        const int before = bc.player.curHp;
        DamageScope damage(&directObserver, 7);
        damage.absorbed(5);
        bc.player.block -= 5;
        bc.player.curHp -= 2;
        directObserver.hpLost("player", before, bc.player.curHp, 2);
        const int beforeHeal = bc.player.curHp;
        bc.player.curHp += 5;
        directObserver.healed("player", beforeHeal, bc.player.curHp);
        directEffects = directObserver.takeEffects();
    }
    state = adapter.battle(state, bc);
    append(writer, 2, "action", state,
           {{"type", "controlled_state"}, {"description", "Controlled Block, HP loss, and healing fixture"}}, directEffects);

    // A real monster replacement exercises the observer's spawn identity hook.
    {
        CombatObserver spawnObserver(bc, identities);
        ScopedCombatObserver scope(spawnObserver);
        bc.monsters.arr[2].initSpawnedMonster(bc, MonsterId::GREEN_LOUSE, 2, 1);
        bc.monsters.monsterCount = 3;
        ++bc.monsters.monstersAlive;
        state = adapter.battle(state, bc);
        append(writer, 3, "action", state, {{"type", "controlled_spawn"}, {"description", "Initialize summoned Green Louse"}}, spawnObserver.takeEffects());
    }

    // Exercise the production action description, execution, and observer path.
    const search::Action cleave(search::ActionType::CARD, 0, 0);
    const auto action = describeBattleAction(bc, cleave, identities);
    CombatObserver observer(bc, identities);
    Json effects;
    {
        ScopedCombatObserver scope(observer);
        cleave.execute(bc);
        effects = observer.takeEffects();
    }
    state = adapter.battle(state, bc);
    state["combat"]["lastPlayedCard"] = action["card"];
    append(writer, 4, "action", state, action, effects);

    gc.screenState = ScreenState::REWARDS;
    gc.outcome = GameOutcome::PLAYER_VICTORY;
    gc.info.rewardsContainer.clear();
    auto terminal = adapter.game(gc);
    terminal["combat"] = state["combat"];
    append(writer, 5, "transition", terminal);
}

void screenShowcase(TraceWriter& writer, SnapshotAdapter& adapter, IdentityRegistry& screenIdentities, GameContext& gc, BattleContext&) {
    Json state = adapter.game(gc);
    append(writer, 0, "run_start", state);
    gc.screenState = ScreenState::MAP_SCREEN;
    state = adapter.game(gc); state["screen"] = screenSnapshot(gc, screenIdentities); state["floor"] = 4;
    append(writer, 1, "run_state", state, {{"type", "map_choice"}, {"description", "Choose route"}},
           Json::array({{{"kind", "state_change"}, {"field", "screen"}, {"description", "Map route selected"}}}));
    gc.screenState = ScreenState::CARD_SELECT;
    gc.info.selectScreenType = CardSelectScreenType::UPGRADE;
    gc.info.toSelectCount = 2;
    gc.info.toSelectCards.clear();
    gc.info.toSelectCards.push_back(SelectScreenCard(Card(CardId::BASH), 0));
    gc.info.toSelectCards.push_back(SelectScreenCard(Card(CardId::DEFEND_RED), 1));
    state = adapter.game(gc); state["screen"] = screenSnapshot(gc, screenIdentities); state["floor"] = 5;
    append(writer, 2, "run_state", state, {{"type", "card_select"}, {"description", "Choose a card"}});
    gc.screenState = ScreenState::SHOP_ROOM;
    gc.info.shop = Shop{};
    std::fill(std::begin(gc.info.shop.prices), std::end(gc.info.shop.prices), -1);
    std::fill(std::begin(gc.info.shop.cards), std::end(gc.info.shop.cards), Card(CardId::INVALID));
    std::fill(std::begin(gc.info.shop.relics), std::end(gc.info.shop.relics), RelicId::INVALID);
    std::fill(std::begin(gc.info.shop.potions), std::end(gc.info.shop.potions), Potion::INVALID);
    gc.info.shop.removeCost = -1;
    gc.info.shop.cards[0] = Card(CardId::BASH);
    gc.info.shop.cardPrice(0) = 50;
    gc.gold = 99;
    state = adapter.game(gc); state["screen"] = screenSnapshot(gc, screenIdentities); state["floor"] = 6;
    append(writer, 3, "run_state", state, {{"type", "shop"}, {"description", "Shop offer"}});
    gc.screenState = ScreenState::REST_ROOM;
    state = adapter.game(gc); state["screen"] = screenSnapshot(gc, screenIdentities); state["floor"] = 7;
    append(writer, 4, "run_state", state, {{"type", "rest"}, {"description", "Rest site"}});
    gc.screenState = ScreenState::EVENT_SCREEN;
    gc.curEvent = Event::NEOW;
    state = adapter.game(gc); state["screen"] = screenSnapshot(gc, screenIdentities); state["floor"] = 8;
    append(writer, 5, "run_state", state, {{"type", "event"}, {"description", "Neow"}});

    gc.screenState = ScreenState::REWARDS;
    gc.info.rewardsContainer.clear();
    gc.info.rewardsContainer.goldRewardCount = 1;
    gc.info.rewardsContainer.gold[0] = 17;
    gc.info.rewardsContainer.cardRewardCount = 1;
    gc.info.rewardsContainer.cardRewards[0].push_back(Card(CardId::STRIKE_RED));
    state = adapter.game(gc); state["screen"] = screenSnapshot(gc, screenIdentities); state["floor"] = 9;
    append(writer, 6, "run_state", state, {{"type", "rewards"}, {"description", "Recorded rewards"}});

    gc.screenState = ScreenState::BOSS_RELIC_REWARDS;
    std::fill(std::begin(gc.info.bossRelics), std::end(gc.info.bossRelics), RelicId::INVALID);
    gc.info.bossRelics[0] = RelicId::BURNING_BLOOD;
    state = adapter.game(gc); state["screen"] = screenSnapshot(gc, screenIdentities); state["floor"] = 10;
    append(writer, 7, "run_state", state, {{"type", "boss_relics"}, {"description", "Boss relic choice"}});

    gc.screenState = ScreenState::TREASURE_ROOM;
    gc.info.chestSize = ChestSize::SMALL;
    gc.info.haveGold = true;
    gc.info.tier = RelicTier::COMMON;
    state = adapter.game(gc); state["screen"] = screenSnapshot(gc, screenIdentities); state["floor"] = 11;
    append(writer, 8, "run_end", state, {{"type", "treasure"}, {"description", "Treasure room"}});
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 3 || std::string(argv[1]) != "--output-dir")
            throw std::invalid_argument("usage: report-fixtures --output-dir DIR");
        const fs::path dir(argv[2]);
        fs::create_directories(dir);
        writeReport(dir / "controlled-combat.html", "victory", controlledCombat);
        writeReport(dir / "screen-showcase.html", "paused", screenShowcase);
        std::cout << "Generated controlled report fixtures in " << fs::absolute(dir) << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << '\n';
        return 1;
    }
}
