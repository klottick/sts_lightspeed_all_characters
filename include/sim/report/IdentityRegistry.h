#ifndef STS_REPORT_IDENTITY_REGISTRY_H
#define STS_REPORT_IDENTITY_REGISTRY_H
#include <map>
#include <string>

namespace sts::report {
class IdentityRegistry {
public:
    void beginCombat();
    int combatId() const { return combat_; }
    std::string cardRef(int uniqueId) const;
    std::string enemyRef(int slot);
    void replaceEnemy(int slot);
private:
    int combat_ = 0;
    std::map<int, int> enemyGenerations_;
};
}
#endif
