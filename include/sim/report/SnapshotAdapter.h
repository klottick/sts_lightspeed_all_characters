#ifndef STS_REPORT_SNAPSHOT_ADAPTER_H
#define STS_REPORT_SNAPSHOT_ADAPTER_H
#include "sim/report/Trace.h"
#include "sim/report/IdentityRegistry.h"

namespace sts {
struct GameContext;
struct BattleContext;
struct CardInstance;
namespace report {
class SnapshotAdapter {
public:
    explicit SnapshotAdapter(IdentityRegistry& identities) : identities_(identities) {}
    Json game(const GameContext& gc);
    Json battle(const Json& runSnapshot, const BattleContext& bc);
    Json card(const CardInstance& card);
    Json catalogDelta();
private:
    void remember(const std::string& key, const Json& value);
    IdentityRegistry& identities_;
    Json catalog_ = Json::object();
    Json pending_ = Json::object();
};
Json playerSnapshot(const BattleContext& bc);
Json enemySnapshot(const BattleContext& bc, int slot, IdentityRegistry& identities);
}
}
#endif
