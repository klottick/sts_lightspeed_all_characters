#include "TestSupport.h"
#include "sim/report/ReportOptions.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using namespace sts::report;

REPORT_TEST(command_options_defaults) {
    const auto options = parseReportOptions({"--seed", "4J8K2", "--output", "run.html"});
    CHECK(options.seed == 6'827'627);
    CHECK(options.seedDisplay == "4J8K2");
    CHECK(options.ascension == 0);
    CHECK(options.agent == "search");
    CHECK(options.simulationCount == 50'000);
    CHECK(options.output == "run.html");
    CHECK(options.assets.empty());

    const auto simple = parseReportOptions({"--agent", "simple", "--seed", "00a", "--output", "out.html"});
    CHECK(simple.agent == "simple");
    CHECK(simple.seed == 10);
    CHECK(simple.seedDisplay == "A");
}

REPORT_TEST(command_options_invalid) {
    for (const auto& args : std::vector<std::vector<std::string>>{
             {}, {"--seed"}, {"--seed", "1"}, {"--output", "x.html"},
             {"--seed", "1", "--output"}, {"--seed", "1", "--output", ""},
             {"--seed", "1", "--output", "x.html", "--seed", "2"},
             {"--seed", "1", "--output", "x.html", "--output", "y.html"},
             {"--seed", "1", "--output", "x.html", "--agent", "search", "--agent", "simple"},
             {"--seed", "1", "--output", "x.html", "--mystery", "x"},
             {"--seed", "1", "--output", "x.html", "--agent"},
             {"--seed", "1", "--output", "x.html", "--agent", "other"},
             {"--seed", "1", "--output", "x.html", "--ascension", "21"},
             {"--seed", "1", "--output", "x.html", "--ascension", "-1"},
             {"--seed", "1", "--output", "x.html", "--ascension", "1.5"},
             {"--seed", "1", "--output", "x.html", "--ascension", "1", "--ascension", "2"},
             {"--seed", "1", "--output", "x.html", "--simulations", "0"},
             {"--seed", "1", "--output", "x.html", "--simulations", "-1"},
             {"--seed", "1", "--output", "x.html", "--simulations", "1x"},
             {"--seed", "1", "--output", "x.html", "--simulations", "9223372036854775808"},
             {"--seed", "1", "--output", "x.html", "--simulations", "2", "--simulations", "3"},
             {"--seed", "1", "--output", "x.html", "--simulations", "2", "--agent", "simple"},
             {"--seed", "1", "--output", "x.html", "--agent", "simple", "--simulations", "2"},
             {"--seed", "1", "--output", "x.html", "--assets"},
             {"--seed", "1", "--output", "x.html", "--assets", ""},
             {"--seed", "1", "--output", "x.html", "--assets", "a.json", "--assets", "b.json"},
         }) CHECK_THROWS(parseReportOptions(args));

    const auto valid = parseReportOptions({"--seed", "1", "--output", "x.html", "--ascension", "020",
                                           "--simulations", "9223372036854775807", "--assets", "assets/manifest.json"});
    CHECK(valid.ascension == 20);
    CHECK(valid.simulationCount == std::numeric_limits<std::int64_t>::max());
    CHECK(valid.assets == "assets/manifest.json");
    CHECK(parseReportOptions({"--seed", "1", "--output", "x.html", "--ascension", "000"}).ascension == 0);
}

REPORT_TEST(command_seed_bounds) {
    const auto maximum = parseReportOptions({"--seed", "5g24a25uxkxFF", "--output", "max.html"});
    CHECK(maximum.seed == std::numeric_limits<std::uint64_t>::max());
    CHECK(maximum.seedDisplay == "5G24A25UXKXFF");
    CHECK_THROWS(parseReportOptions({"--seed", "5G24A25UXKXFG", "--output", "overflow.html"}));
    for (const auto* invalid : {"", "O", "-1", "+1", "1.0", "0_10", "?"})
        CHECK_THROWS(parseReportOptions({"--seed", invalid, "--output", "bad.html"}));
    const auto zero = parseReportOptions({"--seed", "000", "--output", "zero.html"});
    CHECK(zero.seed == 0);
    CHECK(zero.seedDisplay == "0");
}
