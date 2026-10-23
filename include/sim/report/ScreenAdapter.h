#ifndef STS_REPORT_SCREEN_ADAPTER_H
#define STS_REPORT_SCREEN_ADAPTER_H

#include "sim/report/Trace.h"
#include "sim/report/IdentityRegistry.h"

namespace sts {
struct GameContext;
struct BattleContext;
namespace search { struct GameAction; struct Action; }
namespace report {
Json screenSnapshot(const GameContext& gc, IdentityRegistry& identities);
Json describeGameAction(const GameContext& gc, const search::GameAction& action, IdentityRegistry& identities);
Json describeBattleAction(const BattleContext& bc, const search::Action& action, IdentityRegistry& identities);
}
}

#endif
