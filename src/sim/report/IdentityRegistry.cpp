#include "sim/report/IdentityRegistry.h"
namespace sts::report {
void IdentityRegistry::beginCombat() { ++combat_; enemyGenerations_.clear(); }
std::string IdentityRegistry::cardRef(int id) const { return "combat-" + std::to_string(combat_) + "/card-" + std::to_string(id); }
std::string IdentityRegistry::enemyRef(int slot) { return "combat-" + std::to_string(combat_) + "/enemy-" + std::to_string(slot) + "-" + std::to_string(enemyGenerations_[slot]); }
void IdentityRegistry::replaceEnemy(int slot) { ++enemyGenerations_[slot]; }
}
