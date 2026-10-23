#ifndef STS_REPORT_TEST_SUPPORT_H
#define STS_REPORT_TEST_SUPPORT_H

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace report_test {
struct Case { std::string name; std::function<void()> run; };
inline std::vector<Case>& cases() { static std::vector<Case> all; return all; }
struct Register {
    Register(const char* name, std::function<void()> run) { cases().push_back({name, std::move(run)}); }
};
template<class F> void throws(F fn) {
    bool caught = false;
    try { fn(); } catch (const std::exception&) { caught = true; }
    if (!caught) throw std::runtime_error("expected an exception");
}
}

#define REPORT_TEST(name) static void name(); static report_test::Register reg_##name(#name, name); static void name()
#define CHECK(...) do { if (!(__VA_ARGS__)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " + #__VA_ARGS__); } while (false)
#define CHECK_THROWS(...) report_test::throws([&] { __VA_ARGS__; })

#endif
