#include "TestSupport.h"
#include "EngineFixtures.h"
#include "sim/report/CombatObserver.h"
#include "sim/search/Action.h"

using namespace sts;
using namespace sts::report;
namespace {
Json only(const Json& effects, const std::string& kind) {
    auto values = Json::array();
    for (const auto& e : effects) if (e.value("kind", "") == kind) values.push_back(e);
    return values;
}
}

REPORT_TEST(effects_damage_math) {
    auto f = report_test::twoLouses(); IdentityRegistry ids; ids.beginCombat();
    CombatObserver observer(f.bc, ids); ScopedCombatObserver scope(observer);
    f.bc.player.curHp = 20; f.bc.player.block = 5;
    f.bc.player.attacked(f.bc, 0, 7);
    auto damage = only(observer.takeEffects(), "damage");
    CHECK(damage.size() == 1); CHECK(damage[0]["absorbedBlock"] == 5); CHECK(damage[0]["hpLost"] == 2);
    f.bc.player.block = 5; f.bc.player.attacked(f.bc, 0, 3);
    damage = only(observer.takeEffects(), "damage"); CHECK(damage.size() == 1); CHECK(damage[0]["hpLost"] == 0);
    f.bc.monsters.arr[0].curHp = 2; f.bc.monsters.arr[0].block = 0;
    f.bc.monsters.arr[0].attacked(f.bc, 9);
    damage = only(observer.takeEffects(), "damage");
    CHECK(damage.size() == 1); CHECK(damage[0]["hpLost"] == 2); CHECK(damage[0]["overkill"] == 7);
}

REPORT_TEST(effects_multihit_reactive) {
    auto f = report_test::twoLouses(); IdentityRegistry ids; ids.beginCombat();
    f.bc.player.curHp = 50; f.bc.player.block = 0; f.bc.player.buff<PS::THORNS>(3);
    CombatObserver observer(f.bc, ids); ScopedCombatObserver scope(observer);
    f.bc.player.attacked(f.bc, 0, 2); f.bc.player.attacked(f.bc, 0, 2);
    f.bc.inputState = InputState::EXECUTING_ACTIONS;
    f.bc.executeActions();
    auto damage = only(observer.takeEffects(), "damage");
    CHECK(damage.size() == 4);
    CHECK(damage[0]["target"] == "player"); CHECK(damage[1]["target"] == "player");
    CHECK(damage[2]["target"] == ids.enemyRef(0)); CHECK(damage[3]["target"] == ids.enemyRef(0));
}

REPORT_TEST(effects_damage_then_heal) {
    auto f = report_test::twoLouses(); IdentityRegistry ids; ids.beginCombat();
    f.bc.player.curHp = 10; f.bc.player.block = 0;
    CombatObserver observer(f.bc, ids); ScopedCombatObserver scope(observer);
    f.bc.player.attacked(f.bc, 0, 7); f.bc.player.heal(4);
    auto effects = observer.takeEffects(); auto damage = only(effects, "damage"), heal = only(effects, "heal");
    CHECK(damage.size() == 1); CHECK(damage[0]["hpLost"] == 7);
    CHECK(heal.size() == 1); CHECK(heal[0]["amount"] == 4); CHECK(f.bc.player.curHp == 7);
}

REPORT_TEST(effects_revival) {
    auto f = report_test::twoLouses(); IdentityRegistry ids; ids.beginCombat();
    f.bc.player.curHp = 2; f.bc.player.block = 0; f.bc.player.setHasRelic<RelicId::LIZARD_TAIL>(true);
    CombatObserver observer(f.bc, ids); ScopedCombatObserver scope(observer);
    f.bc.player.attacked(f.bc, 0, 9);
    auto effects = observer.takeEffects(); auto damage = only(effects, "damage"), heal = only(effects, "heal");
    CHECK(damage.size() == 1); CHECK(damage[0]["hpLost"] == 2);
    CHECK(heal.size() == 1); CHECK(heal[0]["amount"] == 40); CHECK(f.bc.player.curHp == 40);
}

REPORT_TEST(effects_enemy_heal) {
    auto f = report_test::twoLouses(); IdentityRegistry ids; ids.beginCombat();
    auto& enemy = f.bc.monsters.arr[0]; enemy.curHp = 10; enemy.maxHp = 15; enemy.block = 0;
    CombatObserver observer(f.bc, ids); ScopedCombatObserver scope(observer);
    enemy.damage(f.bc, 7); enemy.heal(4);
    auto effects = observer.takeEffects();
    auto damage = only(effects, "damage"), heals = only(effects, "heal");
    CHECK(damage.size() == 1); CHECK(damage[0]["hpLost"] == 7);
    CHECK(heals.size() == 1); CHECK(heals[0]["amount"] == 4); CHECK(enemy.curHp == 7);
}

REPORT_TEST(effects_queued_enemy_moves) {
    auto f = report_test::twoLouses(); IdentityRegistry ids; ids.beginCombat();
    f.bc.monsters.arr[0].construct(f.bc, MonsterId::RED_LOUSE, 0);
    f.bc.monsters.arr[1].construct(f.bc, MonsterId::GREEN_LOUSE, 1);
    f.bc.monsters.arr[0].setMove(MMID::RED_LOUSE_BITE);
    f.bc.monsters.arr[1].setMove(MMID::GREEN_LOUSE_SPIT_WEB);
    f.bc.player.curHp = 50; f.bc.player.block = 0;
    CombatObserver observer(f.bc, ids); ScopedCombatObserver scope(observer);
    search::Action(search::ActionType::END_TURN).execute(f.bc);
    auto effects = observer.takeEffects(); auto moves = only(effects, "enemy_move"), damage = only(effects, "damage");
    CHECK(moves.size() == 2); CHECK(moves[0]["source"] == ids.enemyRef(0)); CHECK(moves[1]["source"] == ids.enemyRef(1));
    CHECK(!damage.empty()); CHECK(damage[0]["source"] == ids.enemyRef(0));
    CHECK(f.bc.player.hasStatus<PS::WEAK>());
}

REPORT_TEST(effects_same_slot_spawn) {
    auto f = report_test::twoLouses(); IdentityRegistry ids; ids.beginCombat(); const auto old = ids.enemyRef(0);
    CombatObserver observer(f.bc, ids); ScopedCombatObserver scope(observer);
    f.bc.monsters.arr[0].construct(f.bc, MonsterId::RED_LOUSE, 0);
    const auto next = ids.enemyRef(0); CHECK(next != old);
    f.bc.monsters.arr[0].initSpawnedMonster(f.bc, MonsterId::RED_LOUSE, 0, 12);
    CHECK(ids.enemyRef(0) != next); CHECK(only(observer.takeEffects(), "spawn").size() == 2);
}

REPORT_TEST(effects_foreign_copy_and_selection) {
    auto f = report_test::twoLouses(); IdentityRegistry ids; ids.beginCombat(); CombatObserver observer(f.bc, ids);
    {
        ScopedCombatObserver scope(observer); auto copy = f.bc;
        copy.player.damage(copy, 2); copy.player.heal(1);
        CHECK(observer.takeEffects().empty()); CHECK(observerFor(&copy) == nullptr);
        CHECK(observerFor(&f.bc) == &observer);
        try { CombatObserver other(copy, ids); ScopedCombatObserver nested(other); throw std::runtime_error("test"); } catch (const std::exception&) {}
        CHECK(observerFor(&f.bc) == &observer);
        f.bc.inputState = InputState::CARD_SELECT;
    }
    CHECK(observerFor(&f.bc) == nullptr);
    f.bc.player.heal(1); CHECK(observer.takeEffects().empty());
}
