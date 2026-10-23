#include "TestSupport.h"
#include "EngineFixtures.h"
#include "sim/report/SnapshotAdapter.h"

using namespace sts;
using namespace sts::report;

REPORT_TEST(snapshot_authoritative_state) {
    auto f = report_test::twoLouses();
    IdentityRegistry ids; ids.beginCombat(); SnapshotAdapter adapter(ids);
    f.gc.curHp = 80; f.bc.player.curHp = 17; f.bc.player.gold = 23;
    f.bc.potions[0] = Potion::EMPTY_POTION_SLOT;
    auto state = adapter.battle(adapter.game(f.gc), f.bc);
    CHECK(state["player"]["hp"] == 17);
    CHECK(state["player"]["gold"] == 23);
    CHECK(state["inventory"]["potions"].size() == static_cast<std::size_t>(f.bc.potionCapacity));
    CHECK(state["inventory"]["potions"][0]["empty"] == true);
    CHECK(state["combat"]["enemies"].size() == 2);
    CHECK(state["combat"]["orbs"].is_null());
}

REPORT_TEST(snapshot_card_identity) {
    IdentityRegistry ids; ids.beginCombat(); SnapshotAdapter adapter(ids);
    CardInstance a(CardId::STRIKE_RED), b(CardId::STRIKE_RED); a.uniqueId = 10; b.uniqueId = 11;
    CHECK(adapter.card(a)["ref"] != adapter.card(b)["ref"]);
    auto f = report_test::twoLouses(); f.bc.cards.hand[0] = a; f.bc.cards.cardsInHand = 1;
    auto first = adapter.battle(adapter.game(f.gc), f.bc);
    f.bc.cards.cardsInHand = 0; f.bc.cards.discardPile.clear(); f.bc.cards.discardPile.push_back(a);
    auto second = adapter.battle(adapter.game(f.gc), f.bc);
    CHECK(first["combat"]["hand"][0]["ref"] == second["combat"]["piles"]["discard"][0]["ref"]);
}

REPORT_TEST(snapshot_replacement_identity) {
    IdentityRegistry ids; ids.beginCombat();
    const auto first = ids.enemyRef(0); ids.replaceEnemy(0);
    const auto second = ids.enemyRef(0); ids.replaceEnemy(0);
    CHECK(first != second); CHECK(second != ids.enemyRef(0));
    CHECK(ids.enemyRef(1) != ids.enemyRef(0));
}

REPORT_TEST(snapshot_special_cards) {
    IdentityRegistry ids; ids.beginCombat(); SnapshotAdapter adapter(ids);
    CardInstance x(CardId::WHIRLWIND); x.uniqueId = 1; x.cost = -1; x.costForTurn = -1;
    auto value = adapter.card(x); CHECK(value["baseCost"] == -1); CHECK(value["currentCost"] == -1);
    x.freeToPlayOnce = true; value = adapter.card(x); CHECK(value["freeToPlay"] == true); CHECK(value["baseCost"] == -1);
    CardInstance zero(CardId::STRIKE_RED); zero.costForTurn = 0; CHECK(adapter.card(zero)["currentCost"] == 0);
    CardInstance searing(CardId::SEARING_BLOW); searing.upgrade(); searing.upgrade(); searing.upgrade();
    CHECK(adapter.card(searing)["upgrades"] == 3);
}

REPORT_TEST(snapshot_no_rng_mutation) {
    auto f = report_test::twoLouses(); IdentityRegistry ids; ids.beginCombat(); SnapshotAdapter adapter(ids);
    const auto gcBefore = report_test::rngState(f.gc), bcBefore = report_test::rngState(f.bc);
    for (int i = 0; i < 3; ++i) adapter.battle(adapter.game(f.gc), f.bc);
    CHECK(gcBefore == report_test::rngState(f.gc)); CHECK(bcBefore == report_test::rngState(f.bc));
    CHECK(!adapter.catalogDelta().empty()); CHECK(adapter.catalogDelta().empty());
}
