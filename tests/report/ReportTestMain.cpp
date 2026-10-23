#include "TestSupport.h"

#include <iostream>

int main(int argc, char** argv) {
    std::string filter;
    if (argc == 3 && std::string(argv[1]) == "--case") filter = argv[2];
    else if (argc != 1) { std::cerr << "usage: report-tests [--case name]\n"; return 2; }
    int ran = 0, failed = 0;
    for (const auto& test : report_test::cases()) {
        if (!filter.empty() && filter != test.name) continue;
        ++ran;
        try { test.run(); std::cout << "PASS " << test.name << '\n'; }
        catch (const std::exception& ex) { ++failed; std::cerr << "FAIL " << test.name << ": " << ex.what() << '\n'; }
    }
    if (!ran) { std::cerr << "No matching tests\n"; return 2; }
    std::cout << ran - failed << '/' << ran << " passed\n";
    return failed ? 1 : 0;
}
