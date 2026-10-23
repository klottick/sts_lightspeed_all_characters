#ifndef STS_REPORT_RUN_RECORDER_H
#define STS_REPORT_RUN_RECORDER_H
#include "sim/report/SnapshotAdapter.h"
#include "sim/search/Action.h"
#include "sim/search/GameAction.h"

namespace sts::report {
class RunRecorder {
public:
    explicit RunRecorder(TraceWriter& writer) : writer_(writer), adapter_(identities_) {}
    void startRun(const GameContext& gc);
    void enterBattle(const GameContext& gc, BattleContext& bc);
    void recordGameAction(GameContext& gc, search::GameAction action);
    void recordBattleAction(BattleContext& bc, search::Action action);
    void finishBattle(const BattleContext& bc);
    void leaveBattle(const GameContext& gc);
    void finishRun(const std::string& status, const GameContext& gc);
private:
    void append(const std::string& kind, Json state, Json action = nullptr, Json effects = Json::array(), Json beforeScreen = nullptr);
    Json gameChanges(const Json& before, const Json& after) const;
    Json battleState(const BattleContext& bc);
    TraceWriter& writer_;
    IdentityRegistry identities_;
    SnapshotAdapter adapter_;
    std::uint64_t nextId_ = 0;
    Json state_;
    Json runState_;
    Json lastCard_ = nullptr;
    Json pendingCard_ = nullptr;
    Json pendingActionId_ = nullptr;
    bool started_ = false;
    bool inBattle_ = false;
    bool finished_ = false;
};
}
#endif
