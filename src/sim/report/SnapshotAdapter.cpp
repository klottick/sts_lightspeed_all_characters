#include "sim/report/SnapshotAdapter.h"
#include "sim/report/ScreenAdapter.h"
#include "game/GameContext.h"
#include "combat/BattleContext.h"
#include <algorithm>
#include <cctype>

namespace sts::report {
namespace {
template<std::size_t N> std::string named(const char* const (&names)[N], int id) {
    return id >= 0 && static_cast<std::size_t>(id) < N ? names[id] : "UNKNOWN_" + std::to_string(id);
}
std::string friendly(std::string name) {
    bool first = true;
    for (auto& c : name) {
        if (c == '_') { c = ' '; first = true; }
        else { c = first ? std::toupper(static_cast<unsigned char>(c)) : std::tolower(static_cast<unsigned char>(c)); first = false; }
    }
    return name;
}
std::string statusKey(std::string name) {
    for (auto& c : name) c = c == ' ' ? '_' : std::toupper(static_cast<unsigned char>(c));
    return name;
}
Json potions(const std::array<Potion, 5>& items, int capacity) {
    Json result = Json::array();
    for (int i = 0; i < std::clamp(capacity, 0, 5); ++i) {
        const auto token = named(potionEnumNames, static_cast<int>(items[i]));
        result.push_back({{"slot", i}, {"id", token}, {"name", named(potionNames, static_cast<int>(items[i]))},
                          {"empty", items[i] == Potion::EMPTY_POTION_SLOT || items[i] == Potion::INVALID}, {"assetKey", "potion/" + token}});
    }
    return result;
}
Json statuses(const Player& player) {
    Json values = Json::array();
    for (int i = 1; i <= static_cast<int>(PS::THE_BOMB); ++i) {
        const auto id = static_cast<PS>(i);
        if (!player.hasStatusRuntime(id)) continue;
        Json amount = nullptr;
        if (id == PS::ARTIFACT) amount = player.artifact;
        else if (id == PS::DEXTERITY) amount = player.dexterity;
        else if (id == PS::STRENGTH) amount = player.strength;
        else if (id == PS::FOCUS) amount = player.focus;
        else if (const auto it = player.statusMap.find(id); it != player.statusMap.end()) amount = it->second;
        const auto key = named(playerStatusEnumStrings, i);
        values.push_back({{"id", key}, {"name", named(playerStatusStrings, i)}, {"amount", amount}, {"assetKey", "status/" + key}});
    }
    return values;
}
}

void SnapshotAdapter::remember(const std::string& key, const Json& value) {
    if (!catalog_.contains(key)) { catalog_[key] = value; pending_[key] = value; }
}
Json SnapshotAdapter::catalogDelta() {
    Json value = std::move(pending_); pending_ = Json::object(); return value;
}
Json SnapshotAdapter::card(const CardInstance& instance) {
    const auto token = named(cardEnumStrings, static_cast<int>(instance.id));
    const auto name = named(cardNames, static_cast<int>(instance.id));
    const auto asset = "card/" + token;
    remember(asset, {{"name", name}, {"assetKey", asset}});
    return {{"ref", identities_.cardRef(instance.uniqueId)}, {"cardId", token}, {"name", name},
            {"upgrades", instance.getUpgradeCount()}, {"baseCost", instance.cost}, {"currentCost", instance.costForTurn},
            {"freeToPlay", instance.freeToPlayOnce}, {"specialData", instance.specialData}, {"assetKey", asset}};
}

Json SnapshotAdapter::game(const GameContext& gc) {
    Json deck = Json::array(), relics = Json::array();
    int index = 0;
    for (const auto& value : gc.deck.cards) {
        CardInstance instance(value);
        auto item = card(instance); item["ref"] = "deck-" + std::to_string(index++); item["currentCost"] = nullptr;
        deck.push_back(std::move(item));
    }
    for (const auto& value : gc.relics.relics) {
        const auto token = named(relicEnumNames, static_cast<int>(value.id));
        const auto name = named(relicNames, static_cast<int>(value.id));
        const auto asset = "relic/" + token;
        remember(asset, {{"name", name}, {"assetKey", asset}});
        relics.push_back({{"id", token}, {"name", name}, {"counter", value.data}, {"assetKey", asset}});
    }
    const auto character = named(characterClassEnumNames, static_cast<int>(gc.cc));
    const auto screen = screenSnapshot(gc, identities_);
    return {{"screen", screen},
            {"act", gc.act}, {"floor", gc.floorNum}, {"ascension", gc.ascension}, {"character", character},
            {"characterAssetKey", "character/" + character},
            {"player", {{"hp", gc.curHp}, {"maxHp", gc.maxHp}, {"gold", gc.gold}, {"block", nullptr}, {"energy", nullptr}, {"statuses", Json::array()}}},
            {"inventory", {{"deck", deck}, {"relics", relics}, {"potions", potions(gc.potions, gc.potionCapacity)}}},
            {"map", screen.value("map", Json(nullptr))}, {"combat", nullptr}};
}

Json playerSnapshot(const BattleContext& bc) {
    const auto& p = bc.player;
    // The historical stanceStrings array swaps Calm and Wrath; serialize the actual enum.
    const char* stance = p.stance == Stance::CALM ? "CALM" : p.stance == Stance::WRATH ? "WRATH" : p.stance == Stance::DIVINITY ? "DIVINITY" : "NEUTRAL";
    return {{"ref", "player"}, {"hp", p.curHp}, {"maxHp", p.maxHp}, {"gold", p.gold}, {"block", p.block},
            {"energy", p.energy}, {"energyPerTurn", p.energyPerTurn}, {"statuses", statuses(p)}, {"stance", stance},
            {"orbSlots", p.orbSlots}, {"orbs", nullptr}, {"bombTurns", Json::array({p.bomb1, p.bomb2, p.bomb3})}};
}

Json enemySnapshot(const BattleContext& bc, int slot, IdentityRegistry& identities) {
    const auto& m = bc.monsters.arr[slot];
    const auto token = named(monsterIdStrings, static_cast<int>(m.id));
    Json values = Json::array();
    for (int i = 0; i < static_cast<int>(MS::INVALID); ++i) {
        const auto id = static_cast<MS>(i);
        if (!(id == MS::STRENGTH ? m.strength != 0 : m.hasStatusInternal(id))) continue;
        const auto name = named(enemyStatusStrings, i);
        values.push_back({{"id", statusKey(name)}, {"name", name}, {"amount", m.getStatusInternal(id)}, {"assetKey", "status/" + statusKey(name)}});
    }
    const auto move = m.moveHistory[0];
    const auto moveId = named(monsterMoveStrings, static_cast<int>(move));
    const bool attacking = isMoveAttack(move);
    Json damage = nullptr, hits = nullptr;
    if (attacking) { const auto base = m.getMoveBaseDamage(bc); damage = m.calculateDamageToPlayer(bc, base.damage); hits = base.attackCount; }
    auto life = m.isEscaping() || m.isDeadOrEscaped() && m.curHp > 0 ? "escaped" : m.isHalfDead() ? "half_dead" : m.curHp <= 0 ? "defeated" : "alive";
    return {{"ref", identities.enemyRef(slot)}, {"slot", slot}, {"monsterId", token}, {"name", friendly(token)},
            {"hp", m.curHp}, {"maxHp", m.maxHp}, {"block", m.block}, {"statuses", values}, {"lifeState", life}, {"assetKey", "monster/" + token},
            {"intent", {{"moveId", moveId}, {"name", friendly(moveId)}, {"kind", attacking ? "attack" : "non_attack"},
                        {"damagePerHit", damage}, {"hitCount", hits}, {"assetKey", attacking ? "intent/attack" : "intent/unknown"}}}};
}

Json SnapshotAdapter::battle(const Json& runSnapshot, const BattleContext& bc) {
    auto state = runSnapshot;
    state["player"] = playerSnapshot(bc);
    state["inventory"]["potions"] = potions(bc.potions, bc.potionCapacity);
    for (auto& relic : state["inventory"]["relics"]) {
        const auto id = relic.value("id", "");
        if (id == "HAPPY_FLOWER") relic["counter"] = bc.player.happyFlowerCounter;
        else if (id == "INCENSE_BURNER") relic["counter"] = bc.player.incenseBurnerCounter;
        else if (id == "INK_BOTTLE") relic["counter"] = bc.player.inkBottleCounter;
        else if (id == "INSERTER") relic["counter"] = bc.player.inserterCounter;
        else if (id == "NUNCHAKU") relic["counter"] = bc.player.nunchakuCounter;
        else if (id == "PEN_NIB") relic["counter"] = bc.player.penNibCounter == -1 ? 9 : bc.player.penNibCounter;
        else if (id == "SUNDIAL") relic["counter"] = bc.player.sundialCounter;
        else if (id == "LIZARD_TAIL") relic["counter"] = bc.player.hasRelic<RelicId::LIZARD_TAIL>() ? 1 : 0;
    }
    state["screen"] = {{"kind", "battle"}, {"name", "Combat"}, {"options", Json::array()}};
    Json enemies = Json::array(), hand = Json::array(), draw = Json::array(), discard = Json::array(), exhaust = Json::array();
    for (int i = 0; i < bc.monsters.monsterCount; ++i) {
        if (bc.monsters.arr[i].id == MonsterId::INVALID) continue;
        enemies.push_back(enemySnapshot(bc, i, identities_));
    }
    for (int i = 0; i < bc.cards.cardsInHand; ++i) hand.push_back(card(bc.cards.hand[i]));
    for (const auto& value : bc.cards.drawPile) draw.push_back(card(value));
    for (const auto& value : bc.cards.discardPile) discard.push_back(card(value));
    for (const auto& value : bc.cards.exhaustPile) exhaust.push_back(card(value));
    Json selection = nullptr;
    if (bc.inputState == InputState::CARD_SELECT) {
        const auto& info = bc.cardSelectInfo;
        selection = {{"task", named(cardSelectTaskStrings, static_cast<int>(info.cardSelectTask))},
                     {"pickCount", info.pickCount}, {"canPickZero", info.canPickZero}, {"canPickAnyNumber", info.canPickAnyNumber}};
        if (info.cardSelectTask == CardSelectTask::CODEX || info.cardSelectTask == CardSelectTask::DISCOVERY) {
            selection["offered"] = Json::array();
            for (auto value : info.cards) { CardInstance temporary(value); auto item = card(temporary); item["ref"] = nullptr; selection["offered"].push_back(item); }
        }
    }
    state["combat"] = {{"id", identities_.combatId()}, {"turn", bc.turn}, {"inputState", bc.inputState == InputState::CARD_SELECT ? "CARD_SELECT" : bc.inputState == InputState::PLAYER_NORMAL ? "PLAYER_NORMAL" : "EXECUTING_ACTIONS"},
                       {"outcome", bc.outcome == Outcome::PLAYER_VICTORY ? "victory" : bc.outcome == Outcome::PLAYER_LOSS ? "loss" : "undecided"},
                       {"enemies", enemies}, {"hand", hand}, {"piles", {{"draw", draw}, {"discard", discard}, {"exhaust", exhaust}}},
                       {"selection", selection}, {"stance", state["player"]["stance"]}, {"orbSlots", bc.player.orbSlots}, {"orbs", nullptr}};
    return state;
}
}
