#ifndef STS_REPORT_COMBAT_OBSERVER_H
#define STS_REPORT_COMBAT_OBSERVER_H
#include "sim/report/SnapshotAdapter.h"
#include <set>

namespace sts { struct Player; struct Monster; namespace report {
class DamageScope;
class CombatObserver {
public:
    CombatObserver(BattleContext& battle, IdentityRegistry& identities);
    bool matches(const BattleContext*) const noexcept;
    bool matches(const Player*) const noexcept;
    bool matches(const Monster*) const noexcept;
    Json takeEffects();
    void checkpoint();
    void beginEnemyMove(int slot, const Json& move);
    void endEnemyMove();
    void recordDamage(const Json& effect);
    void recordMutation(const Json& effect);
    void enemyReplaced(int slot);
    std::string enemyRef(int slot);
    void hpLost(const std::string& target, int before, int after, int amount);
    void healed(const std::string& target, int before, int after);
    void blockedHit(const std::string& target);
private:
    friend class DamageScope;
    Json mutations() const;
    BattleContext& battle_;
    IdentityRegistry& identities_;
    Json effects_ = Json::array();
    Json previous_;
    std::string moveSource_;
    DamageScope* damage_ = nullptr;
    std::set<std::string> observedHp_;
};

extern thread_local CombatObserver* activeCombatObserver;
inline CombatObserver* observerFor(const BattleContext* battle) noexcept { auto* o = activeCombatObserver; return o && o->matches(battle) ? o : nullptr; }
inline CombatObserver* observerFor(const Player* player) noexcept { auto* o = activeCombatObserver; return o && o->matches(player) ? o : nullptr; }
inline CombatObserver* observerFor(const Monster* monster) noexcept { auto* o = activeCombatObserver; return o && o->matches(monster) ? o : nullptr; }

class ScopedCombatObserver {
public:
    explicit ScopedCombatObserver(CombatObserver& observer);
    ~ScopedCombatObserver();
    ScopedCombatObserver(const ScopedCombatObserver&) = delete;
    ScopedCombatObserver& operator=(const ScopedCombatObserver&) = delete;
private:
    CombatObserver* previous_;
};

class DamageScope {
public:
    DamageScope(CombatObserver* observer, int incoming);
    ~DamageScope();
    void absorbed(int block) { absorbed_ = block; }
    void source(std::string value) { source_ = std::move(value); }
    DamageScope(const DamageScope&) = delete;
private:
    friend class CombatObserver;
    CombatObserver* observer_;
    DamageScope* previous_ = nullptr;
    int incoming_;
    int absorbed_ = 0;
    std::string source_;
};
}}
#endif
