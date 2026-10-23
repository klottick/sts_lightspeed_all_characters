#include "sim/report/ReportOptions.h"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>

namespace sts::report {
namespace {
constexpr std::string_view seedAlphabet = "0123456789ABCDEFGHIJKLMNPQRSTUVWXYZ";

unsigned seedDigit(char value) {
    if (value >= 'a' && value <= 'z') value = static_cast<char>(value - 'a' + 'A');
    const auto position = seedAlphabet.find(value);
    if (position == std::string_view::npos) throw std::invalid_argument("invalid game seed character");
    return static_cast<unsigned>(position);
}

std::string displaySeed(std::uint64_t value) {
    if (value == 0) return "0";
    std::string result;
    while (value) {
        result.push_back(seedAlphabet[value % seedAlphabet.size()]);
        value /= seedAlphabet.size();
    }
    std::reverse(result.begin(), result.end());
    return result;
}

std::uint64_t parseSeed(const std::string& text) {
    if (text.empty()) throw std::invalid_argument("game seed must not be empty");
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t value = 0;
    for (const char character : text) {
        const auto digit = seedDigit(character);
        if (value > (maximum - digit) / seedAlphabet.size())
            throw std::out_of_range("game seed exceeds uint64 range");
        value = value * seedAlphabet.size() + digit;
    }
    return value;
}

std::int64_t parsePositiveInteger(const std::string& text, const char* option) {
    if (text.empty()) throw std::invalid_argument(std::string(option) + " requires a positive integer");
    std::int64_t value = 0;
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    for (const char character : text) {
        if (character < '0' || character > '9')
            throw std::invalid_argument(std::string(option) + " requires a positive integer");
        const int digit = character - '0';
        if (value > (maximum - digit) / 10)
            throw std::out_of_range(std::string(option) + " value is too large");
        value = value * 10 + digit;
    }
    if (value <= 0) throw std::invalid_argument(std::string(option) + " must be positive");
    return value;
}

int parseAscension(const std::string& text) {
    if (text.empty()) throw std::invalid_argument("--ascension requires an integer from 0 to 20");
    int value = 0;
    for (const char character : text) {
        if (character < '0' || character > '9')
            throw std::invalid_argument("--ascension requires an integer from 0 to 20");
        value = value * 10 + (character - '0');
        if (value > 20) throw std::out_of_range("--ascension must be between 0 and 20");
    }
    return value;
}
}

ReportOptions parseReportOptions(const std::vector<std::string>& args) {
    ReportOptions options;
    std::set<std::string> seen;
    bool hasSeed = false, hasOutput = false, simulationsSpecified = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& name = args[i];
        if (name != "--seed" && name != "--output" && name != "--ascension" &&
            name != "--agent" && name != "--simulations" && name != "--assets")
            throw std::invalid_argument("unknown option: " + name);
        if (!seen.insert(name).second) throw std::invalid_argument("duplicate option: " + name);
        if (i + 1 >= args.size() || args[i + 1].empty() || args[i + 1].rfind("--", 0) == 0)
            throw std::invalid_argument("missing value for " + name);
        const auto& value = args[++i];
        if (name == "--seed") {
            options.seed = parseSeed(value);
            options.seedDisplay = displaySeed(options.seed);
            hasSeed = true;
        } else if (name == "--output") {
            options.output = value;
            hasOutput = true;
        } else if (name == "--ascension") {
            options.ascension = parseAscension(value);
        } else if (name == "--agent") {
            if (value != "simple" && value != "search") throw std::invalid_argument("--agent must be simple or search");
            options.agent = value;
        } else if (name == "--simulations") {
            options.simulationCount = parsePositiveInteger(value, "--simulations");
            simulationsSpecified = true;
        } else {
            options.assets = value;
        }
    }
    if (!hasSeed) throw std::invalid_argument("--seed is required");
    if (!hasOutput) throw std::invalid_argument("--output is required");
    if (simulationsSpecified && options.agent == "simple")
        throw std::invalid_argument("--simulations is only valid with --agent search");
    return options;
}
}
