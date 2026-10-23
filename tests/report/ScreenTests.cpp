#include "TestSupport.h"
#include "EngineFixtures.h"
#include "sim/report/ScreenAdapter.h"
#include "sim/search/GameAction.h"
#include "sim/search/Action.h"

#include <algorithm>

using namespace sts;
using namespace sts::report;

REPORT_TEST(screen_all_kinds) {
    auto f = report_test::twoLouses();
    IdentityRegistry ids; ids.beginCombat();

    f.gc.screenState = ScreenState::MAP_SCREEN;
    auto map = screenSnapshot(f.gc, ids);
    CHECK(map["kind"] == "map"); CHECK(map.contains("map")); CHECK(!map["map"]["nodes"].empty());

    f.gc.screenState = ScreenState::EVENT_SCREEN; f.gc.curEvent = Event::NEOW;
    auto event = screenSnapshot(f.gc, ids);
    CHECK(event["kind"] == "event"); CHECK(event["name"] == "NEOW"); CHECK(event["options"].size() == 4);
    CHECK(event["options"][0]["label"].get<std::string>().size() > 0);

    f.gc.screenState = ScreenState::SHOP_ROOM;
    f.gc.info.shop = Shop{};
    std::fill(std::begin(f.gc.info.shop.prices), std::end(f.gc.info.shop.prices), -1);
    f.gc.info.shop.removeCost = -1;
    std::fill(std::begin(f.gc.info.shop.relics), std::end(f.gc.info.shop.relics), RelicId::INVALID);
    std::fill(std::begin(f.gc.info.shop.potions), std::end(f.gc.info.shop.potions), Potion::INVALID);
    f.gc.info.shop.cards[0] = Card(CardId::BASH); f.gc.info.shop.cardPrice(0) = 50; f.gc.gold = 99;
    f.gc.info.shop.removeCost = 75;
    auto shop = screenSnapshot(f.gc, ids);
    CHECK(shop["kind"] == "shop"); CHECK(shop["options"].size() >= 2);

    f.gc.screenState = ScreenState::REWARDS;
    auto& rewards = f.gc.info.rewardsContainer; rewards.clear();
    rewards.goldRewardCount = 1; rewards.gold[0] = 17;
    rewards.cardRewardCount = 1; rewards.cardRewards[0].push_back(Card(CardId::STRIKE_RED));
    rewards.relicCount = 1; rewards.relics[0] = RelicId::ODDLY_SMOOTH_STONE;
    rewards.emeraldKey = true; rewards.potionCount = 1; rewards.potions[0] = Potion::FIRE_POTION;
    auto reward = screenSnapshot(f.gc, ids);
    CHECK(reward["kind"] == "rewards"); CHECK(reward["options"].size() >= 5);

    f.gc.screenState = ScreenState::BOSS_RELIC_REWARDS;
    std::fill(std::begin(f.gc.info.bossRelics), std::end(f.gc.info.bossRelics), RelicId::INVALID);
    f.gc.info.bossRelics[0] = RelicId::BURNING_BLOOD;
    auto boss = screenSnapshot(f.gc, ids);
    CHECK(boss["kind"] == "boss_relics"); CHECK(boss["options"].size() == 4);

    f.gc.screenState = ScreenState::REST_ROOM;
    auto rest = screenSnapshot(f.gc, ids);
    CHECK(rest["kind"] == "rest"); CHECK(!rest["options"].empty());

    f.gc.screenState = ScreenState::TREASURE_ROOM;
    f.gc.info.chestSize = ChestSize::SMALL; f.gc.info.haveGold = true; f.gc.info.tier = RelicTier::COMMON;
    auto treasure = screenSnapshot(f.gc, ids);
    CHECK(treasure["kind"] == "treasure"); CHECK(treasure["options"].size() == 2);

    f.gc.screenState = ScreenState::CARD_SELECT;
    f.gc.info.selectScreenType = CardSelectScreenType::UPGRADE; f.gc.info.toSelectCount = 1;
    f.gc.info.toSelectCards.clear(); f.gc.info.toSelectCards.push_back(SelectScreenCard(Card(CardId::BASH), 0));
    auto select = screenSnapshot(f.gc, ids);
    CHECK(select["kind"] == "card_select"); CHECK(select["options"].size() == 1);
}

REPORT_TEST(screen_transition_offer) {
    auto f = report_test::twoLouses(); IdentityRegistry ids; ids.beginCombat();
    f.gc.screenState = ScreenState::SHOP_ROOM; f.gc.gold = 99;
    f.gc.info.shop = Shop{};
    std::fill(std::begin(f.gc.info.shop.prices), std::end(f.gc.info.shop.prices), -1);
    std::fill(std::begin(f.gc.info.shop.relics), std::end(f.gc.info.shop.relics), RelicId::INVALID);
    std::fill(std::begin(f.gc.info.shop.potions), std::end(f.gc.info.shop.potions), Potion::INVALID);
    f.gc.info.shop.cards[0] = Card(CardId::BASH); f.gc.info.shop.cardPrice(0) = 50;
    auto action = describeGameAction(f.gc, search::GameAction(search::GameAction::RewardsActionType::CARD, 0), ids);
    CHECK(action["type"] == "shop_card"); CHECK(action["preActionOffer"]["price"] == 50);
    CHECK(action["preActionOffer"]["card"]["assetKey"] == "card/BASH");
}

REPORT_TEST(screen_unknown) {
    auto f = report_test::twoLouses(); IdentityRegistry ids;
    f.gc.screenState = static_cast<ScreenState>(255);
    auto screen = screenSnapshot(f.gc, ids);
    CHECK(screen["kind"] == "unknown"); CHECK(screen["id"] == 255); CHECK(screen["options"].empty());
}

REPORT_TEST(screen_unknown_event_fallback) {
    auto f = report_test::twoLouses(); IdentityRegistry ids;
    f.gc.screenState = ScreenState::EVENT_SCREEN;
    f.gc.curEvent = static_cast<Event>(255);
    const auto screen = screenSnapshot(f.gc, ids);
    CHECK(screen["kind"] == "event");
    CHECK(screen["name"] == "Unknown event");
    CHECK(screen["eventId"] == 255);
    CHECK(screen["unknown"] == true);
    CHECK(screen["options"].empty());
}

REPORT_TEST(screen_known_passive_event_title_bounds) {
    auto f = report_test::twoLouses(); IdentityRegistry ids;
    f.gc.screenState = ScreenState::EVENT_SCREEN;
    f.gc.curEvent = Event::WORLD_OF_GOOP;
    const auto last = screenSnapshot(f.gc, ids);
    CHECK(last["name"] == "World of Goop");
    f.gc.curEvent = Event::TREASURE;
    const auto passive = screenSnapshot(f.gc, ids);
    CHECK(passive["name"] == "TREASURE");
    CHECK(passive["options"].empty());
}

REPORT_TEST(screen_shop_transition_keeps_offer) {
    auto f = report_test::twoLouses(); IdentityRegistry ids;
    f.gc.screenState = ScreenState::SHOP_ROOM; f.gc.gold = 99;
    f.gc.info.shop = Shop{};
    std::fill(std::begin(f.gc.info.shop.prices), std::end(f.gc.info.shop.prices), -1);
    std::fill(std::begin(f.gc.info.shop.relics), std::end(f.gc.info.shop.relics), RelicId::INVALID);
    std::fill(std::begin(f.gc.info.shop.potions), std::end(f.gc.info.shop.potions), Potion::INVALID);
    f.gc.info.shop.cards[0] = Card(CardId::BASH); f.gc.info.shop.cardPrice(0) = 50;
    const auto action = describeGameAction(f.gc, search::GameAction(search::GameAction::RewardsActionType::CARD, 0), ids);
    CHECK(action["preActionOffer"]["price"] == 50);
    search::GameAction(search::GameAction::RewardsActionType::CARD, 0).execute(f.gc);
    CHECK(f.gc.gold == 49);
    CHECK(action["preActionOffer"]["price"] == 50);
}

REPORT_TEST(screen_reward_gold_indices_are_distinct) {
    auto f = report_test::twoLouses(); IdentityRegistry ids;
    f.gc.screenState = ScreenState::REWARDS;
    auto& rewards = f.gc.info.rewardsContainer; rewards.clear();
    rewards.goldRewardCount = 2; rewards.gold[0] = 11; rewards.gold[1] = 29;
    const auto first = describeGameAction(f.gc, search::GameAction(search::GameAction::RewardsActionType::GOLD, 0), ids);
    const auto second = describeGameAction(f.gc, search::GameAction(search::GameAction::RewardsActionType::GOLD, 1), ids);
    CHECK(first["preActionOffer"]["amount"] == 11);
    CHECK(second["preActionOffer"]["amount"] == 29);
    CHECK(first["preActionOffer"]["index"] != second["preActionOffer"]["index"]);
}

REPORT_TEST(screen_card_skip_without_singing_bowl_not_hp) {
    auto f = report_test::twoLouses(); IdentityRegistry ids;
    f.gc.screenState = ScreenState::REWARDS;
    auto& rewards = f.gc.info.rewardsContainer; rewards.clear();
    rewards.cardRewardCount = 1; rewards.cardRewards[0].push_back(Card(CardId::BASH));
    const auto action = describeGameAction(f.gc, search::GameAction(search::GameAction::RewardsActionType::CARD, 0, 5), ids);
    CHECK(action["type"] != "reward_singing_bowl");
    CHECK(action["description"] == "Skip card choice");
    CHECK(!action["preActionOffer"].contains("maxHpGain"));
}

REPORT_TEST(screen_descriptions_nonempty) {
    auto f = report_test::twoLouses(); IdentityRegistry ids; ids.beginCombat();
    f.gc.screenState = ScreenState::TREASURE_ROOM;
    f.gc.info.chestSize = ChestSize::SMALL;
    for (const auto& action : search::GameAction::getAllActionsInState(f.gc))
        CHECK(!describeGameAction(f.gc, action, ids)["description"].get<std::string>().empty());

    f.bc.cards.cardsInHand = 1; f.bc.cards.hand[0] = CardInstance(CardId::BASH); f.bc.cards.hand[0].uniqueId = 44;
    auto battleAction = describeBattleAction(f.bc, search::Action(search::ActionType::CARD, 0, 0), ids);
    CHECK(!battleAction["description"].get<std::string>().empty());
    CHECK(battleAction["card"]["assetKey"] == "card/BASH");

    f.gc.screenState = ScreenState::MAP_SCREEN;
    f.gc.potions[0] = Potion::FIRE_POTION;
    auto gamePotion = describeGameAction(f.gc, search::GameAction(0x80000000U), ids);
    CHECK(gamePotion["type"] == "potion_use"); CHECK(gamePotion["discard"] == false);
    auto gameDiscard = describeGameAction(f.gc, search::GameAction(0xC0000000U), ids);
    CHECK(gameDiscard["type"] == "potion_discard"); CHECK(gameDiscard["discard"] == true);

    f.bc.cardSelectInfo.cardSelectTask = CardSelectTask::HEADBUTT;
    f.bc.cards.discardPile.clear(); f.bc.cards.discardPile.push_back(CardInstance(CardId::BASH));
    f.bc.cards.discardPile[0].uniqueId = 77;
    auto selection = describeBattleAction(f.bc, search::Action(search::ActionType::SINGLE_CARD_SELECT, 0), ids);
    CHECK(selection["type"] == "card_select");
    CHECK(selection["selection"]["cards"][0]["assetKey"] == "card/BASH");
}

REPORT_TEST(screen_no_rng_mutation) {
    auto f = report_test::twoLouses(); IdentityRegistry ids;
    const auto before = report_test::rngState(f.gc);
    f.gc.screenState = ScreenState::MAP_SCREEN;
    screenSnapshot(f.gc, ids);
    f.gc.screenState = ScreenState::EVENT_SCREEN; f.gc.curEvent = Event::NEOW;
    screenSnapshot(f.gc, ids);
    f.gc.screenState = ScreenState::REST_ROOM; screenSnapshot(f.gc, ids);
    CHECK(before == report_test::rngState(f.gc));
}
