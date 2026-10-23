#include "sim/report/CombatObserver.h"
#include "combat/BattleContext.h"
#include <algorithm>

namespace sts::report {
thread_local CombatObserver* activeCombatObserver = nullptr;
CombatObserver::CombatObserver(BattleContext& battle, IdentityRegistry& ids) : battle_(battle), identities_(ids), previous_(mutations()) {}
bool CombatObserver::matches(const BattleContext* battle) const noexcept { return battle == &battle_; }
bool CombatObserver::matches(const Player* player) const noexcept { return player == &battle_.player; }
bool CombatObserver::matches(const Monster* monster) const noexcept {
    for (const auto& value : battle_.monsters.arr) if (&value == monster) return true;
    return false;
}
Json CombatObserver::takeEffects() {
    if (activeCombatObserver == this) endEnemyMove();
    auto value = std::move(effects_); effects_ = Json::array(); return value;
}
Json CombatObserver::mutations() const {
    Json value{{"player", playerSnapshot(battle_)}};
    for (int i = 0; i < battle_.monsters.monsterCount; ++i) {
        if (battle_.monsters.arr[i].id == MonsterId::INVALID) continue;
        auto enemy = enemySnapshot(battle_, i, identities_);
        const auto ref = enemy["ref"].get<std::string>();
        value[ref] = std::move(enemy);
    }
    return value;
}
void CombatObserver::checkpoint() {
    const auto current = mutations();
    for (auto actor = current.begin(); actor != current.end(); ++actor) {
        if (!previous_.contains(actor.key())) continue;
        const auto& old = previous_[actor.key()];
        for (const auto* field : {"block", "energy", "gold", "maxHp", "statuses", "stance", "bombTurns"}) {
            if (!actor->contains(field) || !old.contains(field) || (*actor)[field] == old[field]) continue;
            recordMutation({{"kind", "state_change"}, {"target", actor.key()}, {"field", field},
                            {"description", std::string(field) + " changed"}, {"before", old[field]}, {"after", (*actor)[field]}});
        }
        if (actor->contains("hp") && old.contains("hp") && (*actor)["hp"] != old["hp"] && !observedHp_.count(actor.key())) {
            recordMutation({{"kind", "hp_change"}, {"target", actor.key()}, {"description", "HP changed"},
                            {"before", old["hp"]}, {"after", (*actor)["hp"]}});
        }
    }
    previous_ = current;
    observedHp_.clear();
}
void CombatObserver::beginEnemyMove(int slot, const Json& move) {
    endEnemyMove();
    moveSource_ = identities_.enemyRef(slot);
    const auto resolved = move.empty() ? enemySnapshot(battle_, slot, identities_)["intent"] : move;
    recordMutation({{"kind", "enemy_move"}, {"source", moveSource_}, {"target", "player"}, {"resolvedMove", resolved},
                    {"description", resolved.value("name", "Enemy move") + " resolved"}});
}
void CombatObserver::endEnemyMove() { checkpoint(); moveSource_.clear(); }
void CombatObserver::recordDamage(const Json& effect) {
    auto value = effect;
    value["sequence"] = effects_.size();
    effects_.push_back(std::move(value));
}
void CombatObserver::recordMutation(const Json& effect) {
    auto value = effect;
    if (!value.contains("source")) value["source"] = moveSource_.empty() ? Json(nullptr) : Json(moveSource_);
    value["sequence"] = effects_.size();
    effects_.push_back(std::move(value));
}
void CombatObserver::enemyReplaced(int slot) {
    const auto old = identities_.enemyRef(slot); identities_.replaceEnemy(slot);
    recordMutation({{"kind", "spawn"}, {"target", identities_.enemyRef(slot)}, {"description", "Enemy lifetime replaced"},
                    {"before", old}, {"after", identities_.enemyRef(slot)}});
}
std::string CombatObserver::enemyRef(int slot) { return identities_.enemyRef(slot); }
void CombatObserver::hpLost(const std::string& target, int before, int after, int amount) {
    const int lost = std::max(0, before - after);
    observedHp_.insert(target);
    recordDamage({{"kind", "damage"}, {"target", target}, {"source", damage_ && !damage_->source_.empty() ? Json(damage_->source_) : Json(nullptr)},
                  {"description", std::to_string(lost) + " HP lost"}, {"before", before}, {"after", after},
                  {"incoming", damage_ ? damage_->incoming_ : amount}, {"absorbedBlock", damage_ ? damage_->absorbed_ : 0},
                  {"hpLost", lost}, {"overkill", std::max(0, amount - before)}});
}
void CombatObserver::healed(const std::string& target, int before, int after) {
    if (before == after) return;
    observedHp_.insert(target);
    recordMutation({{"kind", "heal"}, {"source", nullptr}, {"target", target}, {"description", std::to_string(after - before) + " HP healed"},
                    {"before", before}, {"after", after}, {"amount", after - before}});
}
void CombatObserver::blockedHit(const std::string& target) {
    recordDamage({{"kind", "damage"}, {"target", target}, {"source", damage_ && !damage_->source_.empty() ? Json(damage_->source_) : Json(nullptr)},
                  {"description", "Hit prevented HP loss"}, {"incoming", damage_ ? damage_->incoming_ : 0},
                  {"absorbedBlock", damage_ ? damage_->absorbed_ : 0}, {"hpLost", 0}, {"overkill", 0}});
}
ScopedCombatObserver::ScopedCombatObserver(CombatObserver& observer) : previous_(activeCombatObserver) { activeCombatObserver = &observer; }
ScopedCombatObserver::~ScopedCombatObserver() { activeCombatObserver = previous_; }
DamageScope::DamageScope(CombatObserver* observer, int incoming) : observer_(observer), incoming_(incoming) {
    if (observer_) { previous_ = observer_->damage_; observer_->damage_ = this; }
}
DamageScope::~DamageScope() { if (observer_) observer_->damage_ = previous_; }
}
