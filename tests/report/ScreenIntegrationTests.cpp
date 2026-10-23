#include "TestSupport.h"
#include "EngineFixtures.h"
#include "sim/report/SnapshotAdapter.h"

using namespace sts;
using namespace sts::report;

REPORT_TEST(screen_adapter_in_game_snapshot) {
    auto f = report_test::twoLouses(); IdentityRegistry ids; SnapshotAdapter adapter(ids);
    auto state = adapter.game(f.gc);
    CHECK(state["screen"]["kind"] == "event");
    CHECK(state["screen"]["options"].size() == 4);
    f.gc.screenState = ScreenState::MAP_SCREEN;
    state = adapter.game(f.gc);
    CHECK(state["screen"]["kind"] == "map");
    CHECK(!state["map"]["nodes"].empty());
}
