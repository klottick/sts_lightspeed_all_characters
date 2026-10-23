#include "sim/report/ScreenAdapter.h"

#include "game/GameContext.h"
#include "combat/BattleContext.h"
#include "combat/CardInstance.h"
#include "constants/Rooms.h"
#include "constants/Misc.h"
#include "constants/Events.h"
#include "constants/Relics.h"
#include "constants/Potions.h"
#include "constants/Cards.h"
#include "sim/search/GameAction.h"
#include "sim/search/Action.h"

#include <algorithm>
#include <iterator>

namespace sts::report {
namespace {
const char* cardName(CardId id) {
    const int value = static_cast<int>(id);
    return value >= 0 && value < static_cast<int>(std::size(cardNames)) ? cardNames[value] : "Unknown card";
}
const char* cardEnumName(CardId id) {
    const int value = static_cast<int>(id);
    return value >= 0 && value < static_cast<int>(std::size(cardEnumStrings)) ? cardEnumStrings[value] : "UNKNOWN";
}
const char* relicName(RelicId id) {
    const int value = static_cast<int>(id);
    return value >= 0 && value < static_cast<int>(std::size(relicNames)) ? relicNames[value] : "Unknown relic";
}
const char* potionName(Potion id) {
    const int value = static_cast<int>(id);
    return value >= 0 && value < static_cast<int>(std::size(potionNames)) ? potionNames[value] : "Unknown potion";
}
const char* roomName(Room room) {
    const int value = static_cast<int>(room);
    return value >= 0 && value < static_cast<int>(std::size(roomStrings)) ? roomStrings[value] : "UNKNOWN";
}

Json cardValue(const Card& card) {
    if (card.id == CardId::INVALID) return nullptr;
    const auto id = cardEnumName(card.id);
    return Json{{"cardId", id}, {"name", cardName(card.id)},
                {"upgrades", card.getUpgraded()}, {"assetKey", std::string("card/") + id}};
}

Json cardInstanceValue(const CardInstance& card, IdentityRegistry& ids) {
    if (card.id == CardId::INVALID) return nullptr;
    const auto id = cardEnumName(card.id);
    return Json{{"ref", ids.cardRef(card.uniqueId)}, {"cardId", id},
                {"name", cardName(card.id)}, {"upgrades", card.getUpgradeCount()},
                {"baseCost", card.cost}, {"currentCost", card.costForTurn},
                {"freeToPlay", card.freeToPlayOnce}, {"specialData", card.specialData},
                {"assetKey", std::string("card/") + id}};
}

Json option(std::string key, std::string label, bool available=true) {
    return Json{{"key", std::move(key)}, {"label", std::move(label)}, {"available", available}, {"data", Json::object()}};
}

std::string eventOptionLabel(Event event, int idx, const GameContext& gc) {
    if (idx < 0 || idx >= 32) return "Unknown event choice";
    switch (event) {
        case Event::NEOW: {
            if (idx >= 4) return "Unknown Neow choice";
            const auto& reward = gc.info.neowRewards[idx];
            const int bonus = static_cast<int>(reward.r);
            std::string text = bonus >= 0 && bonus < static_cast<int>(std::size(Neow::bonusStrings)) ? Neow::bonusStrings[bonus] : "Unknown Neow bonus";
            const int drawback = static_cast<int>(reward.d);
            if (drawback > 0 && drawback < 7 && Neow::drawbackStrings[drawback][0] != '\0')
                text += " — " + std::string(Neow::drawbackStrings[drawback]);
            return text;
        }
        case Event::OMINOUS_FORGE: return idx == 0 ? "Forge: upgrade a card" : idx == 1 ? "Rummage: obtain Warped Tongs and a curse" : "Leave";
        case Event::PLEADING_VAGRANT: return idx == 0 ? "Give 85 gold for a relic" : idx == 1 ? "Rob for a relic and take a curse" : "Leave";
        case Event::ANCIENT_WRITING: return idx == 0 ? "Elegance: remove a card" : "Simplicity: upgrade Strikes and Defends";
        case Event::OLD_BEGGAR: return idx == 0 ? "Offer gold to remove a card" : "Leave";
        case Event::BIG_FISH: return idx == 0 ? "Banana: heal" : idx == 1 ? "Donut: gain max HP" : "Box: obtain a relic and curse";
        case Event::COLOSSEUM: return gc.info.eventData == 0 ? "Fight" : idx == 0 ? "Escape" : "Fight for rewards";
        case Event::CURSED_TOME: return idx == 1 ? "Read" : idx == 5 ? "Take the book" : idx == 6 ? "Stop" : "Continue reading";
        case Event::DEAD_ADVENTURER: return idx == 0 ? "Search" : "Escape";
        case Event::DESIGNER_IN_SPIRE: return idx == 5 ? "Punch" : idx == 0 || idx == 1 ? "Adjustments" : idx == 2 || idx == 3 ? "Clean Up" : "Full Service";
        case Event::AUGMENTER: return idx == 0 ? "Test J.A.X" : idx == 1 ? "Become Test Subject" : "Ingest Mutagens";
        case Event::DUPLICATOR: return idx == 0 ? "Pray: duplicate a card" : "Leave";
        case Event::FACE_TRADER: return idx == 0 ? "Touch" : idx == 1 ? "Trade" : "Leave";
        case Event::FALLING: return idx == 0 ? "Land" : idx == 1 ? "Channel" : idx == 2 ? "Strike" : "Splat";
        case Event::FORGOTTEN_ALTAR: return idx == 0 ? "Offer Golden Idol" : idx == 1 ? "Sacrifice" : "Desecrate";
        case Event::THE_DIVINE_FOUNTAIN: return idx == 0 ? "Drink: remove curses" : "Leave";
        case Event::GHOSTS: return idx == 0 ? "Accept Apparitions" : "Refuse";
        case Event::GOLDEN_IDOL: return idx == 0 ? "Take Golden Idol" : idx == 1 ? "Leave" : idx == 2 ? "Outrun" : idx == 3 ? "Smash" : "Hide";
        case Event::GOLDEN_SHRINE: return idx == 0 ? "Pray" : idx == 1 ? "Desecrate" : "Leave";
        case Event::WING_STATUE: return idx == 0 ? "Pray: remove a card and lose HP" : idx == 1 ? "Destroy for gold" : "Leave";
        case Event::KNOWING_SKULL: return idx == 0 ? "Riches?" : idx == 1 ? "Success?" : idx == 2 ? "A Pick Me Up?" : "Leave";
        case Event::THE_SSSSSERPENT: return idx == 0 ? "Agree" : "Disagree";
        case Event::LIVING_WALL: return idx == 0 ? "Forget" : idx == 1 ? "Change" : "Grow";
        case Event::MASKED_BANDITS: return idx == 0 ? "Pay all gold" : "Fight";
        case Event::MATCH_AND_KEEP: return "Select card";
        case Event::MINDBLOOM: return idx == 0 ? "I am War" : idx == 1 ? "I am Awake" : gc.floorNum <= 40 ? "I am Rich" : "I am Healthy";
        case Event::HYPNOTIZING_COLORED_MUSHROOMS: return idx == 0 ? "Stomp" : "Eat";
        case Event::MYSTERIOUS_SPHERE: return idx == 0 ? "Open Sphere" : "Leave";
        case Event::THE_NEST: return idx == 0 ? "Smash and Grab" : "Stay in Line";
        case Event::NLOTH: return idx < 2 ? "Offer a relic" : "Leave";
        case Event::NOTE_FOR_YOURSELF: return idx == 0 ? "Take and Give" : "Ignore";
        case Event::PURIFIER: case Event::TRANSMORGRIFIER: case Event::UPGRADE_SHRINE: return idx == 0 ? "Pray" : "Leave";
        case Event::SCRAP_OOZE: return idx == 0 ? "Reach Inside" : "Leave";
        case Event::SECRET_PORTAL: return idx == 0 ? "Enter the Portal" : "Leave";
        case Event::SENSORY_STONE: return "Recall";
        case Event::SHINING_LIGHT: return idx == 0 ? "Enter" : "Leave";
        case Event::THE_CLERIC: return idx == 0 ? "Heal" : idx == 1 ? "Purify" : "Leave";
        case Event::THE_JOUST: return idx == 0 ? "Bet on the murderer" : "Bet on the owner";
        case Event::THE_LIBRARY: return idx == 0 ? "Read" : "Sleep";
        case Event::THE_MAUSOLEUM: return idx == 0 ? "Open Coffin" : "Leave";
        case Event::THE_MOAI_HEAD: return idx == 0 ? "Jump Inside" : idx == 1 ? "Offer Golden Idol" : "Leave";
        case Event::THE_WOMAN_IN_BLUE: return idx == 0 ? "Buy one potion" : idx == 1 ? "Buy two potions" : idx == 2 ? "Buy three potions" : "Leave";
        case Event::TOMB_OF_LORD_RED_MASK: return idx == 0 ? "Don the Red Mask" : idx == 1 ? "Offer all gold" : "Leave";
        case Event::VAMPIRES: return idx == 0 ? "Offer Blood Vial" : idx == 1 ? "Accept" : "Refuse";
        case Event::WE_MEET_AGAIN: return idx == 0 ? "Give potion" : idx == 1 ? "Give gold" : idx == 2 ? "Give card" : "Attack";
        case Event::WHEEL_OF_CHANGE: return "Play";
        case Event::WINDING_HALLS: return idx == 0 ? "Embrace Madness" : idx == 1 ? "Press On" : "Retrace Your Steps";
        case Event::WORLD_OF_GOOP: return idx == 0 ? "Gather Gold" : "Leave It";
        default: return "Choice " + std::to_string(idx);
    }
}

Json eventOptionData(Event event, int idx, const GameContext& gc) {
    Json data = Json::object();
    if (event == Event::NEOW) {
        if (idx < 0 || idx >= 4) return data;
        const auto& r = gc.info.neowRewards[idx];
        data["bonus"] = static_cast<int>(r.r);
        data["drawback"] = static_cast<int>(r.d);
    } else if (event == Event::FALLING) {
        const int deckIdx = idx == 0 ? gc.info.skillCardDeckIdx : idx == 1 ? gc.info.powerCardDeckIdx : idx == 2 ? gc.info.attackCardDeckIdx : -1;
        if (deckIdx >= 0 && deckIdx < gc.deck.size()) data["card"] = cardValue(gc.deck.cards[deckIdx]);
    } else if (event == Event::NLOTH && idx < 2) {
        const int relicIdx = idx == 0 ? gc.info.relicIdx0 : gc.info.relicIdx1;
        if (relicIdx >= 0 && relicIdx < gc.relics.size()) {
            const auto relic = gc.relics.relics[relicIdx].id;
            data["relicId"] = static_cast<int>(relic);
            data["relicName"] = relicName(relic);
        }
    } else if (event == Event::MATCH_AND_KEEP && idx >= 0 && idx < gc.info.toSelectCards.size()) {
        data["card"] = cardValue(gc.info.toSelectCards[idx].card);
        data["revealed"] = gc.info.toSelectCards[idx].deckIdx == 0 || gc.info.toSelectCards[idx].deckIdx == 1;
    }
    return data;
}

Json eventOptions(const GameContext& gc) {
    Json options = Json::array();
    const int eventId = static_cast<int>(gc.curEvent);
    if (eventId < 0 || eventId >= static_cast<int>(std::size(eventGameNames))) return options;
    if (gc.curEvent == Event::MATCH_AND_KEEP) {
        for (int i = 0; i < gc.info.toSelectCards.size(); ++i) {
            auto o = option("event/card/" + std::to_string(i), eventOptionLabel(gc.curEvent, i, gc));
            o["data"] = eventOptionData(gc.curEvent, i, gc);
            options.push_back(std::move(o));
        }
        return options;
    }
    const auto bits = search::GameAction::getValidEventSelectBits(gc);
    for (int idx = 0; idx < 32; ++idx) {
        if ((bits & (1 << idx)) == 0) continue;
        auto o = option("event/" + std::to_string(idx), eventOptionLabel(gc.curEvent, idx, gc));
        o["data"] = eventOptionData(gc.curEvent, idx, gc);
        options.push_back(std::move(o));
    }
    return options;
}

Json rewardsOptions(const GameContext& gc) {
    Json options = Json::array();
    const auto& r = gc.info.rewardsContainer;
    for (int i = 0; i < std::clamp(r.goldRewardCount, 0, 2); ++i) {
        auto o = option("reward/gold/" + std::to_string(i), "Take " + std::to_string(r.gold[i]) + " gold");
        o["data"] = {{"rewardType", "gold"}, {"index", i}, {"amount", r.gold[i]}}; options.push_back(o);
    }
    for (int i = 0; i < std::clamp(r.cardRewardCount, 0, 5); ++i) {
        for (int j = 0; j < r.cardRewards[i].size(); ++j) {
            auto c = cardValue(r.cardRewards[i][j]);
            auto o = option("reward/card/" + std::to_string(i) + "/" + std::to_string(j), "Take " + c["name"].get<std::string>());
            o["data"] = {{"rewardType", "card"}, {"rewardIndex", i}, {"cardIndex", j}, {"card", c}}; options.push_back(o);
        }
        if (gc.hasRelic(RelicId::SINGING_BOWL)) {
            auto o = option("reward/singing_bowl/" + std::to_string(i), "Gain 2 max HP instead of a card");
            o["data"] = {{"rewardType", "singing_bowl"}, {"index", i}, {"maxHpGain", 2}}; options.push_back(o);
        } else {
            auto o = option("reward/skip_card/" + std::to_string(i), "Skip card reward");
            o["data"] = {{"rewardType", "skip_card"}, {"index", i}}; options.push_back(o);
        }
    }
    for (int i = 0; i < std::clamp(r.relicCount, 0, 3); ++i) {
        const auto relic = r.relics[i]; auto o = option("reward/relic/" + std::to_string(i), relicName(relic));
        o["data"] = {{"rewardType", "relic"}, {"index", i}, {"relicId", static_cast<int>(relic)}, {"relicName", relicName(relic)}}; options.push_back(o);
    }
    for (int i = 0; i < std::clamp(r.potionCount, 0, 5); ++i) {
        const auto potion = r.potions[i]; auto o = option("reward/potion/" + std::to_string(i), potionName(potion));
        o["data"] = {{"rewardType", "potion"}, {"index", i}, {"potionId", static_cast<int>(potion)}, {"potionName", potionName(potion)}}; options.push_back(o);
    }
    if (r.emeraldKey) { auto o = option("reward/key/emerald", "Obtain Emerald Key"); o["data"] = {{"rewardType", "key"}, {"key", "emerald"}}; options.push_back(o); }
    if (r.sapphireKey) { auto o = option("reward/key/sapphire", "Obtain Sapphire Key"); o["data"] = {{"rewardType", "key"}, {"key", "sapphire"}}; options.push_back(o); }
    auto skip = option(r.getTotalCount() ? "reward/skip" : "reward/proceed", r.getTotalCount() ? "Skip rewards" : "Proceed");
    skip["data"] = {{"rewardType", "skip"}}; options.push_back(skip);
    return options;
}

std::string gameActionType(const GameContext& gc, const search::GameAction& a) {
    if (a.isPotionAction()) return a.isPotionDiscard() ? "potion_discard" : "potion_use";
    switch (gc.screenState) {
        case ScreenState::MAP_SCREEN: return "map_route";
        case ScreenState::EVENT_SCREEN: return gc.curEvent == Event::MATCH_AND_KEEP ? "match_and_keep" : "event_choice";
        case ScreenState::SHOP_ROOM:
            switch (a.getRewardsActionType()) {
                case search::GameAction::RewardsActionType::CARD: return "shop_card";
                case search::GameAction::RewardsActionType::RELIC: return "shop_relic";
                case search::GameAction::RewardsActionType::POTION: return "shop_potion";
                case search::GameAction::RewardsActionType::CARD_REMOVE: return "shop_remove";
                default: return "shop_leave";
            }
        case ScreenState::REWARDS:
            switch (a.getRewardsActionType()) {
                case search::GameAction::RewardsActionType::CARD:
                    return a.getIdx2() == 5 ? (gc.hasRelic(RelicId::SINGING_BOWL) ? "reward_singing_bowl" : "reward_skip_card") : "reward_card";
                case search::GameAction::RewardsActionType::GOLD: return "reward_gold";
                case search::GameAction::RewardsActionType::KEY: return "reward_key";
                case search::GameAction::RewardsActionType::POTION: return "reward_potion";
                case search::GameAction::RewardsActionType::RELIC: return "reward_relic";
                default: return "reward_skip";
            }
        case ScreenState::BOSS_RELIC_REWARDS: return a.getIdx1() == 3 ? "boss_relic_skip" : "boss_relic_take";
        case ScreenState::REST_ROOM: return "rest_option";
        case ScreenState::TREASURE_ROOM: return a.getIdx1() == 0 ? "treasure_open" : "treasure_skip";
        case ScreenState::CARD_SELECT: return "card_select";
        default: return "unknown";
    }
}

std::string gameActionLabel(const GameContext& gc, const search::GameAction& a) {
    const int idx = a.getIdx1();
    if (a.isPotionAction()) {
        const int slot = idx;
        const char* name = slot >= 0 && slot < gc.potionCapacity ? potionName(gc.potions[slot]) : "Unknown potion";
        return std::string(a.isPotionDiscard() ? "Discard " : "Use ") + name;
    }
    switch (gc.screenState) {
        case ScreenState::MAP_SCREEN: return gc.curMapNodeY == 14 ? "Advance to boss" : "Choose map node " + std::to_string(idx);
        case ScreenState::EVENT_SCREEN: return eventOptionLabel(gc.curEvent, idx, gc);
        case ScreenState::BOSS_RELIC_REWARDS: return idx == 3 ? "Skip boss relic" : idx >= 0 && idx < 3 ? std::string("Take ") + relicName(gc.info.bossRelics[idx]) : "Unknown boss relic choice";
        case ScreenState::REST_ROOM: {
            static const char* labels[] = {"Rest", "Smith", "Recall", "Lift", "Toke", "Dig", "Skip"};
            return idx >= 0 && idx < 7 ? labels[idx] : "Unknown rest option";
        }
        case ScreenState::TREASURE_ROOM: return idx == 0 ? "Open chest" : "Skip chest";
        case ScreenState::CARD_SELECT: return idx >= 0 && idx < gc.info.toSelectCards.size() ? "Select " + std::string(cardName(gc.info.toSelectCards[idx].card.id)) : "Select card";
        case ScreenState::SHOP_ROOM: {
            const int itemIdx = a.getIdx1();
            switch (a.getRewardsActionType()) {
                case search::GameAction::RewardsActionType::CARD: return itemIdx < 7 ? "Buy " + std::string(cardName(gc.info.shop.cards[itemIdx].id)) : "Buy card";
                case search::GameAction::RewardsActionType::RELIC: return itemIdx < 3 ? "Buy " + std::string(relicName(gc.info.shop.relics[itemIdx])) : "Buy relic";
                case search::GameAction::RewardsActionType::POTION: return itemIdx < 3 ? "Buy " + std::string(potionName(gc.info.shop.potions[itemIdx])) : "Buy potion";
                case search::GameAction::RewardsActionType::CARD_REMOVE: return "Remove a card";
                default: return "Leave shop";
            }
        }
        case ScreenState::REWARDS: {
            const auto& r = gc.info.rewardsContainer;
            switch (a.getRewardsActionType()) {
                case search::GameAction::RewardsActionType::GOLD: return idx < r.goldRewardCount ? "Take " + std::to_string(r.gold[idx]) + " gold" : "Take gold reward";
                case search::GameAction::RewardsActionType::CARD: {
                    const int ci = a.getIdx2();
                    if (idx >= 0 && idx < std::clamp(r.cardRewardCount, 0, 5) && ci >= 0 && ci < r.cardRewards[idx].size()) return "Take " + std::string(cardName(r.cardRewards[idx][ci].id));
                    if (ci == 5 && gc.hasRelic(RelicId::SINGING_BOWL)) return "Gain 2 max HP";
                    return "Skip card choice";
                }
                case search::GameAction::RewardsActionType::RELIC: return idx >= 0 && idx < std::clamp(r.relicCount, 0, 3) ? "Take " + std::string(relicName(r.relics[idx])) : "Take relic reward";
                case search::GameAction::RewardsActionType::POTION: return idx >= 0 && idx < std::clamp(r.potionCount, 0, 5) ? "Take " + std::string(potionName(r.potions[idx])) : "Take potion reward";
                case search::GameAction::RewardsActionType::KEY: return r.sapphireKey ? "Take Sapphire Key" : "Take Emerald Key";
                default: return r.getTotalCount() ? "Skip rewards" : "Proceed";
            }
        }
        default: return "Game action";
    }
}

Json gameActionOffer(const GameContext& gc, const search::GameAction& a, IdentityRegistry&) {
    Json offer = Json::object();
    const int idx = a.getIdx1();
    if (a.isPotionAction()) {
        const auto potion = idx >= 0 && idx < gc.potionCapacity ? gc.potions[idx] : Potion::INVALID;
        offer = {{"kind", "potion"}, {"slot", idx}, {"potionId", static_cast<int>(potion)}, {"name", potionName(potion)}, {"discard", a.isPotionDiscard()}};
        return offer;
    }
    if (gc.screenState == ScreenState::SHOP_ROOM) {
        const auto type = a.getRewardsActionType();
        if (type == search::GameAction::RewardsActionType::CARD && idx < 7) {
            offer = {{"kind", "card"}, {"card", cardValue(gc.info.shop.cards[idx])}, {"price", gc.info.shop.cardPrice(idx)}};
        } else if (type == search::GameAction::RewardsActionType::RELIC && idx < 3) {
            auto id = gc.info.shop.relics[idx]; offer = {{"kind", "relic"}, {"relicId", static_cast<int>(id)}, {"name", relicName(id)}, {"price", gc.info.shop.relicPrice(idx)}};
        } else if (type == search::GameAction::RewardsActionType::POTION && idx < 3) {
            auto id = gc.info.shop.potions[idx]; offer = {{"kind", "potion"}, {"potionId", static_cast<int>(id)}, {"name", potionName(id)}, {"price", gc.info.shop.potionPrice(idx)}};
        } else if (type == search::GameAction::RewardsActionType::CARD_REMOVE) {
            offer = {{"kind", "remove"}, {"price", gc.info.shop.removeCost}};
        } else offer = {{"kind", "leave"}};
    } else if (gc.screenState == ScreenState::REWARDS) {
        const auto& r = gc.info.rewardsContainer;
        switch (a.getRewardsActionType()) {
            case search::GameAction::RewardsActionType::GOLD:
                if (idx < r.goldRewardCount) offer = {{"kind", "gold"}, {"index", idx}, {"amount", r.gold[idx]}};
                break;
            case search::GameAction::RewardsActionType::CARD: {
                int ci = a.getIdx2();
                if (idx < r.cardRewardCount && ci < r.cardRewards[idx].size()) offer = {{"kind", "card"}, {"rewardIndex", idx}, {"cardIndex", ci}, {"card", cardValue(r.cardRewards[idx][ci])}};
                else if (idx >= 0 && idx < std::clamp(r.cardRewardCount, 0, 5) && ci == 5 && gc.hasRelic(RelicId::SINGING_BOWL)) offer = {{"kind", "singing_bowl"}, {"maxHpGain", 2}};
                else if (idx >= 0 && idx < std::clamp(r.cardRewardCount, 0, 5) && ci == 5) offer = {{"kind", "skip_card"}, {"index", idx}};
                break;
            }
            case search::GameAction::RewardsActionType::RELIC:
                if (idx >= 0 && idx < std::clamp(r.relicCount, 0, 3)) offer = {{"kind", "relic"}, {"index", idx}, {"relicId", static_cast<int>(r.relics[idx])}, {"name", relicName(r.relics[idx])}};
                break;
            case search::GameAction::RewardsActionType::POTION:
                if (idx >= 0 && idx < std::clamp(r.potionCount, 0, 5)) offer = {{"kind", "potion"}, {"index", idx}, {"potionId", static_cast<int>(r.potions[idx])}, {"name", potionName(r.potions[idx])}};
                break;
            case search::GameAction::RewardsActionType::KEY: offer = {{"kind", "key"}, {"key", r.sapphireKey ? "sapphire" : "emerald"}}; break;
            default: offer = {{"kind", "skip"}}; break;
        }
    } else if (gc.screenState == ScreenState::EVENT_SCREEN) {
        const int eventId = static_cast<int>(gc.curEvent);
        const std::string eventName = eventId >= 0 && eventId < static_cast<int>(std::size(eventGameNames)) ? eventGameNames[eventId] : "Unknown event";
        offer = {{"kind", "event_choice"}, {"event", eventName}, {"index", idx}, {"label", eventOptionLabel(gc.curEvent, idx, gc)}, {"data", eventOptionData(gc.curEvent, idx, gc)}};
    } else if (gc.screenState == ScreenState::MAP_SCREEN) {
        offer = {{"kind", "map_node"}, {"x", idx}};
        if (gc.map && gc.curMapNodeY < 14) {
            const int y = gc.curMapNodeY == -1 ? 0 : gc.curMapNodeY + 1;
            if (idx >= 0 && idx < 7 && y >= 0 && y < 15) {
                const auto& n = gc.map->getNode(idx, y); offer["node"] = {{"x", n.x}, {"y", n.y}, {"room", roomName(n.room)}};
            }
        }
    } else if (gc.screenState == ScreenState::CARD_SELECT && idx >= 0 && idx < gc.info.toSelectCards.size()) {
        offer = {{"kind", "card"}, {"card", cardValue(gc.info.toSelectCards[idx].card)}, {"deckIndex", gc.info.toSelectCards[idx].deckIdx}};
    }
    return offer;
}

Json actionJson(std::string type, std::uint32_t bits, std::string description) {
    return Json{{"type", std::move(type)}, {"bits", bits}, {"description", std::move(description)},
                {"source", nullptr}, {"targets", Json::array()}, {"card", nullptr}};
}

Json selectedOptionKey(const GameContext& gc, const search::GameAction& a) {
    using R = search::GameAction::RewardsActionType;
    if (a.isPotionAction()) return nullptr;
    const auto index = std::to_string(a.getIdx1());
    switch (gc.screenState) {
        case ScreenState::MAP_SCREEN: return "map/" + index;
        case ScreenState::EVENT_SCREEN: return std::string(gc.curEvent == Event::MATCH_AND_KEEP ? "event/card/" : "event/") + index;
        case ScreenState::REST_ROOM: return "rest/" + index;
        case ScreenState::CARD_SELECT: return "card_select/" + index;
        case ScreenState::TREASURE_ROOM: return a.getIdx1() == 0 ? "treasure/open" : "treasure/skip";
        case ScreenState::BOSS_RELIC_REWARDS: return a.getIdx1() == 3 ? "boss_relic/skip" : "boss_relic/" + index;
        case ScreenState::SHOP_ROOM:
            switch (a.getRewardsActionType()) {
                case R::CARD: return "shop/card/" + index;
                case R::RELIC: return "shop/relic/" + index;
                case R::POTION: return "shop/potion/" + index;
                case R::CARD_REMOVE: return "shop/remove";
                default: return "shop/leave";
            }
        case ScreenState::REWARDS:
            switch (a.getRewardsActionType()) {
                case R::CARD:
                    if (a.getIdx2() == 5) return std::string(gc.hasRelic(RelicId::SINGING_BOWL) ? "reward/singing_bowl/" : "reward/skip_card/") + index;
                    return "reward/card/" + index + "/" + std::to_string(a.getIdx2());
                case R::GOLD: return "reward/gold/" + index;
                case R::RELIC: return "reward/relic/" + index;
                case R::POTION: return "reward/potion/" + index;
                case R::KEY: return gc.info.rewardsContainer.sapphireKey ? "reward/key/sapphire" : "reward/key/emerald";
                default: return gc.info.rewardsContainer.getTotalCount() ? "reward/skip" : "reward/proceed";
            }
        default: return nullptr;
    }
}
}

Json screenSnapshot(const GameContext& gc, IdentityRegistry&) {
    Json result{{"kind", "unknown"}, {"name", "Unknown screen"}, {"options", Json::array()}};
    switch (gc.screenState) {
        case ScreenState::MAP_SCREEN: {
            result["kind"] = "map"; result["name"] = "Map";
            Json map{{"current", {{"x", gc.curMapNodeX}, {"y", gc.curMapNodeY}}}, {"nodes", Json::array()}};
            if (gc.map) {
                for (int y = 0; y < 15; ++y) for (int x = 0; x < 7; ++x) {
                    const auto& n = gc.map->getNode(x, y); Json edges = Json::array();
                    for (int i = 0; i < n.edgeCount && i < static_cast<int>(n.edges.size()); ++i) edges.push_back(n.edges[i]);
                    map["nodes"].push_back({{"x", n.x}, {"y", n.y}, {"room", roomName(n.room)}, {"edges", edges}});
                }
                Json next = Json::array();
                if (gc.curMapNodeY == 14) next.push_back(0);
                else if (gc.curMapNodeY == -1) {
                    for (const auto& n : gc.map->nodes[0]) if (n.edgeCount > 0) next.push_back(n.x);
                } else if (gc.curMapNodeY >= 0 && gc.curMapNodeY < 14 && gc.curMapNodeX >= 0 && gc.curMapNodeX < 7) {
                    const auto& n = gc.map->getNode(gc.curMapNodeX, gc.curMapNodeY);
                    for (int i = 0; i < n.edgeCount && i < static_cast<int>(n.edges.size()); ++i) next.push_back(n.edges[i]);
                }
                result["map"] = std::move(map);
                for (const auto& x : next) {
                    const int nx = x.get<int>(); const int ny = gc.curMapNodeY == 14 ? 14 : gc.curMapNodeY + 1;
                    std::string label = gc.curMapNodeY == 14 ? "Advance to boss" : "Choose " + std::string(roomName(gc.map->getNode(nx, ny).room));
                    auto o = option("map/" + std::to_string(nx), label); o["data"] = {{"x", nx}, {"y", ny}, {"room", gc.curMapNodeY == 14 ? "BOSS" : roomName(gc.map->getNode(nx, ny).room)}}; result["options"].push_back(o);
                }
            } else result["map"] = nullptr;
            break;
        }
        case ScreenState::EVENT_SCREEN: {
            result["kind"] = "event"; const int e = static_cast<int>(gc.curEvent);
            const bool knownEvent = e >= 0 && e < static_cast<int>(std::size(eventGameNames));
            result["name"] = knownEvent ? eventGameNames[e] : "Unknown event";
            result["eventId"] = e;
            if (knownEvent) result["options"] = eventOptions(gc);
            else { result["unknown"] = true; result["options"] = Json::array(); }
            if (gc.curEvent == Event::MATCH_AND_KEEP) result["attemptsRemaining"] = gc.info.eventData;
            else if (gc.curEvent == Event::CURSED_TOME || gc.curEvent == Event::COLOSSEUM || gc.curEvent == Event::SCRAP_OOZE || gc.curEvent == Event::MINDBLOOM)
                result["phase"] = gc.info.eventData;
            break;
        }
        case ScreenState::SHOP_ROOM: {
            result["kind"] = "shop"; result["name"] = "Shop"; result["gold"] = gc.gold;
            for (int i = 0; i < 7; ++i) {
                const auto& c = gc.info.shop.cards[i]; const int price = gc.info.shop.cardPrice(i);
                if (c.id != CardId::INVALID && price >= 0) { auto o = option("shop/card/" + std::to_string(i), "Buy " + std::string(cardName(c.id)), gc.gold >= price); o["data"] = {{"kind", "card"}, {"index", i}, {"card", cardValue(c)}, {"price", price}}; result["options"].push_back(o); }
            }
            for (int i = 0; i < 3; ++i) {
                const int price = gc.info.shop.relicPrice(i); const auto id = gc.info.shop.relics[i];
                if (id != RelicId::INVALID && price >= 0) { auto o = option("shop/relic/" + std::to_string(i), "Buy " + std::string(relicName(id)), gc.gold >= price); o["data"] = {{"kind", "relic"}, {"index", i}, {"relicId", static_cast<int>(id)}, {"name", relicName(id)}, {"price", price}}; result["options"].push_back(o); }
            }
            for (int i = 0; i < 3; ++i) {
                const int price = gc.info.shop.potionPrice(i); const auto id = gc.info.shop.potions[i];
                if (id != Potion::INVALID && id != Potion::EMPTY_POTION_SLOT && price >= 0) { auto o = option("shop/potion/" + std::to_string(i), "Buy " + std::string(potionName(id)), gc.gold >= price); o["data"] = {{"kind", "potion"}, {"index", i}, {"potionId", static_cast<int>(id)}, {"name", potionName(id)}, {"price", price}}; result["options"].push_back(o); }
            }
            if (gc.info.shop.removeCost >= 0) { auto o = option("shop/remove", "Remove a card", gc.gold >= gc.info.shop.removeCost); o["data"] = {{"kind", "remove"}, {"price", gc.info.shop.removeCost}}; result["options"].push_back(o); }
            result["options"].push_back(option("shop/leave", "Leave shop"));
            break;
        }
        case ScreenState::REWARDS: result["kind"] = "rewards"; result["name"] = "Rewards"; result["options"] = rewardsOptions(gc); break;
        case ScreenState::BOSS_RELIC_REWARDS:
            result["kind"] = "boss_relics"; result["name"] = "Boss relic rewards";
            for (int i = 0; i < 3; ++i) { const auto relic = gc.info.bossRelics[i]; auto o = option("boss_relic/" + std::to_string(i), relicName(relic)); o["data"] = {{"index", i}, {"relicId", static_cast<int>(relic)}, {"name", relicName(relic)}}; result["options"].push_back(o); }
            result["options"].push_back(option("boss_relic/skip", "Skip reward")); break;
        case ScreenState::REST_ROOM: {
            result["kind"] = "rest"; result["name"] = "Rest site";
            auto add = [&](int i, const char* label) { auto o = option("rest/" + std::to_string(i), label); o["data"] = {{"index", i}}; result["options"].push_back(o); };
            if (!gc.relics.has(RelicId::COFFEE_DRIPPER)) add(0, "Rest");
            if (!gc.relics.has(RelicId::FUSION_HAMMER) && gc.deck.getUpgradeableCount() > 0) add(1, "Smith: upgrade a card");
            if (!gc.hasKey(Key::RUBY_KEY)) add(2, "Recall: take Ruby Key");
            if (gc.relics.has(RelicId::GIRYA) && gc.relics.getRelicValue(RelicId::GIRYA) != 3) add(3, "Lift");
            if (gc.relics.has(RelicId::PEACE_PIPE)) add(4, "Toke: remove a card");
            if (gc.relics.has(RelicId::SHOVEL)) add(5, "Dig");
            if (result["options"].empty()) add(6, "Skip");
            break;
        }
        case ScreenState::TREASURE_ROOM: {
            result["kind"] = "treasure"; result["name"] = "Treasure room";
            const int chest = static_cast<int>(gc.info.chestSize);
            result["chestSize"] = chest >= 0 && chest < 3 ? Json(chestSizeNames[chest]) : Json(nullptr);
            result["haveGold"] = gc.info.haveGold; result["relicTier"] = static_cast<int>(gc.info.tier);
            result["options"].push_back(option("treasure/open", "Open chest")); result["options"].push_back(option("treasure/skip", "Skip reward")); break;
        }
        case ScreenState::CARD_SELECT: {
            result["kind"] = "card_select"; result["name"] = "Card selection"; result["selectType"] = static_cast<int>(gc.info.selectScreenType); result["toSelectCount"] = gc.info.toSelectCount;
            result["selected"] = Json::array();
            for (const auto& selected : gc.info.haveSelectedCards) result["selected"].push_back(cardValue(selected.card));
            for (int i = 0; i < gc.info.toSelectCards.size(); ++i) {
                const auto& c = gc.info.toSelectCards[i]; auto o = option("card_select/" + std::to_string(i), cardName(c.card.id)); o["data"] = {{"index", i}, {"card", cardValue(c.card)}, {"deckIndex", c.deckIdx}}; result["options"].push_back(o);
            }
            break;
        }
        case ScreenState::BATTLE: result["kind"] = "battle"; result["name"] = "Battle"; break;
        case ScreenState::INVALID: result["id"] = 0; break;
        default: result["id"] = static_cast<int>(gc.screenState); break;
    }
    return result;
}

Json describeGameAction(const GameContext& gc, const search::GameAction& action, IdentityRegistry& ids) {
    auto result = actionJson(gameActionType(gc, action), action.bits, gameActionLabel(gc, action));
    result["optionKey"] = selectedOptionKey(gc, action);
    const Json offer = gameActionOffer(gc, action, ids);
    result["preActionOffer"] = offer;
    if (offer.is_object() && offer.value("kind", std::string()) == "card" && offer.contains("card")) result["card"] = offer["card"];
    if (action.isPotionAction()) {
        result["source"] = "potion/" + std::to_string(action.getIdx1());
        result["potionAction"] = action.isPotionDiscard() ? "discard" : "use";
        result["discard"] = action.isPotionDiscard();
    }
    if (gc.screenState == ScreenState::MAP_SCREEN && gc.curMapNodeY < 14 && gc.map) {
        const int x = action.getIdx1(), y = gc.curMapNodeY + 1;
        if (x >= 0 && x < 7 && y >= 0 && y < 15) result["description"] = "Choose " + std::string(roomName(gc.map->getNode(x, y).room)) + " node (" + std::to_string(x) + ", " + std::to_string(y) + ")";
    }
    return result;
}

Json describeBattleAction(const BattleContext& bc, const search::Action& action, IdentityRegistry& ids) {
    using search::ActionType;
    const auto type = action.getActionType();
    Json result = actionJson("unknown", action.bits, "Battle action");
    if (type == ActionType::CARD) {
        const int source = action.getSourceIdx();
        result["type"] = "card_play";
        if (source >= 0 && source < bc.cards.cardsInHand) {
            const auto& card = bc.cards.hand[source]; result["source"] = ids.cardRef(card.uniqueId); result["card"] = cardInstanceValue(card, ids);
            result["description"] = "Play " + std::string(cardName(card.id));
            if (card.requiresTarget()) {
                const int target = action.getTargetIdx();
                if (target >= 0 && target < static_cast<int>(bc.monsters.arr.size())) { result["targets"].push_back(ids.enemyRef(target)); result["description"] = result["description"].get<std::string>() + " on " + bc.monsters.arr[target].getName(); }
            }
        } else result["description"] = "Play card";
    } else if (type == ActionType::POTION) {
        const int source = action.getSourceIdx(), target = action.getTargetIdx();
        const bool discard = target > 5;
        result["type"] = discard ? "potion_discard" : "potion_use";
        result["source"] = "potion/" + std::to_string(source);
        const auto potion = source >= 0 && source < static_cast<int>(bc.potions.size()) ? bc.potions[source] : Potion::INVALID;
        result["preActionOffer"] = {{"kind", "potion"}, {"slot", source}, {"potionId", static_cast<int>(potion)}, {"name", potionName(potion)}, {"discard", discard}};
        result["description"] = std::string(discard ? "Discard " : "Use ") + potionName(potion);
        if (!discard && potionRequiresTarget(potion) && target >= 0 && target < static_cast<int>(bc.monsters.arr.size())) result["targets"].push_back(ids.enemyRef(target));
    } else if (type == ActionType::END_TURN) {
        result["type"] = "end_turn"; result["description"] = "End turn";
    } else if (type == ActionType::SINGLE_CARD_SELECT || type == ActionType::MULTI_CARD_SELECT) {
        result["type"] = type == ActionType::SINGLE_CARD_SELECT ? "card_select" : "multi_card_select";
        Json selected = Json::array();
        if (type == ActionType::SINGLE_CARD_SELECT) selected.push_back(action.getSelectIdx());
        else for (int index : action.getSelectedIdxs()) selected.push_back(index);
        const auto task = bc.cardSelectInfo.cardSelectTask;
        Json selectedCards = Json::array();
        for (const auto& item : selected) {
            const int index = item.get<int>();
            Json value = nullptr;
            switch (task) {
                case CardSelectTask::CODEX:
                case CardSelectTask::DISCOVERY:
                    if (index >= 0 && index < static_cast<int>(bc.cardSelectInfo.cards.size())) value = cardValue(Card(bc.cardSelectInfo.cards[index]));
                    break;
                case CardSelectTask::EXHUME:
                    if (index >= 0 && index < bc.cards.exhaustPile.size()) value = cardInstanceValue(bc.cards.exhaustPile[index], ids);
                    break;
                case CardSelectTask::HEADBUTT:
                case CardSelectTask::HOLOGRAM:
                case CardSelectTask::LIQUID_MEMORIES_POTION:
                    if (index >= 0 && index < bc.cards.discardPile.size()) value = cardInstanceValue(bc.cards.discardPile[index], ids);
                    break;
                case CardSelectTask::SECRET_TECHNIQUE:
                case CardSelectTask::SECRET_WEAPON:
                case CardSelectTask::SEEK:
                    if (index >= 0 && index < bc.cards.drawPile.size()) value = cardInstanceValue(bc.cards.drawPile[index], ids);
                    break;
                case CardSelectTask::ARMAMENTS:
                case CardSelectTask::DUAL_WIELD:
                case CardSelectTask::EXHAUST_ONE:
                case CardSelectTask::FORETHOUGHT:
                case CardSelectTask::MEDITATE:
                case CardSelectTask::NIGHTMARE:
                case CardSelectTask::RECYCLE:
                case CardSelectTask::SETUP:
                case CardSelectTask::WARCRY:
                    if (index >= 0 && index < bc.cards.cardsInHand) value = cardInstanceValue(bc.cards.hand[index], ids);
                    break;
                default: break;
            }
            selectedCards.push_back(value);
        }
        result["selection"] = {{"indices", selected}, {"task", static_cast<int>(task)}, {"cards", selectedCards}};
        result["preActionOffer"] = result["selection"];
        result["description"] = "Select card";
        if (selected.size() > 1) result["description"] = "Select " + std::to_string(selected.size()) + " cards";
    }
    return result;
}
}
