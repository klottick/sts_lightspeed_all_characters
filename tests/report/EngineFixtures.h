#ifndef STS_REPORT_ENGINE_FIXTURES_H
#define STS_REPORT_ENGINE_FIXTURES_H
#include "game/GameContext.h"
#include "combat/BattleContext.h"
#include "sim/report/Trace.h"

namespace report_test {
struct BattleFixture { sts::GameContext gc; sts::BattleContext bc; };
BattleFixture twoLouses(std::uint64_t seed = 1);
sts::report::Json rngState(const sts::GameContext& gc);
sts::report::Json rngState(const sts::BattleContext& bc);
}
#endif
