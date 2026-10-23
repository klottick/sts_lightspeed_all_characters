#include "TestSupport.h"

#include <iostream>
#include <cstdlib>
#include <new>

namespace report_test {
thread_local bool countAllocations = false;
thread_local std::size_t allocationBlocks = 0;
}
void* operator new(std::size_t size) {
    if (report_test::countAllocations) ++report_test::allocationBlocks;
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

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
