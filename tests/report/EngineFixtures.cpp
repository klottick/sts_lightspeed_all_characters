#include "EngineFixtures.h"
namespace report_test {
using namespace sts;
using sts::report::Json;
BattleFixture twoLouses(std::uint64_t seed) {
    BattleFixture f{GameContext(CharacterClass::IRONCLAD, seed, 0), BattleContext{}};
    f.bc.init(f.gc, MonsterEncounter::TWO_LOUSE);
    return f;
}
namespace {
Json rng(const Random& r) { return Json::array({std::to_string(r.seed0), std::to_string(r.seed1), r.counter}); }
}
Json rngState(const GameContext& gc) {
    Json all = Json::array();
    for (auto r : {&gc.aiRng,&gc.cardRandomRng,&gc.cardRng,&gc.eventRng,&gc.mathUtilRng,&gc.merchantRng,&gc.miscRng,&gc.monsterHpRng,&gc.monsterRng,&gc.neowRng,&gc.potionRng,&gc.relicRng,&gc.shuffleRng,&gc.treasureRng}) all.push_back(rng(*r));
    return all;
}
Json rngState(const BattleContext& bc) {
    Json all = Json::array();
    for (auto r : {&bc.aiRng,&bc.cardRandomRng,&bc.miscRng,&bc.monsterHpRng,&bc.potionRng,&bc.shuffleRng}) all.push_back(rng(*r));
    return all;
}
}
