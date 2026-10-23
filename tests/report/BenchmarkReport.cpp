// Identical standalone workload for the baseline and recording-disabled engines.
#include "combat/BattleContext.h"
#include "game/GameContext.h"
#include "sim/search/BattleScumSearcher2.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
bool counting = false;
std::uint64_t allocations = 0;
}
void* operator new(std::size_t size) {
    if (counting) ++allocations;
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main(int argc, char** argv) {
    const int repetitions = argc == 2 ? std::stoi(argv[1]) : 5000;
    if (repetitions <= 0) return 2;
    sts::GameContext gc(sts::CharacterClass::IRONCLAD, 1, 0);
    sts::BattleContext battle{}; battle.init(gc, sts::MonsterEncounter::TWO_LOUSE);
    { sts::search::BattleScumSearcher2 warmup(battle); warmup.search(100); }
    std::uint64_t checksum = 1469598103934665603ULL;
    const auto mix = [&](std::uint64_t value) { checksum ^= value; checksum *= 1099511628211ULL; };
    const auto start = std::chrono::steady_clock::now();
    counting = true;
    for (int i = 0; i < repetitions; ++i) {
        sts::search::BattleScumSearcher2 searcher(battle);
        searcher.search(100);
        mix(searcher.root.simulationCount);
        mix(searcher.outcomePlayerHp);
        for (auto action : searcher.bestActionSequence) mix(action.bits);
        for (const auto* rng : {&searcher.rootState->aiRng, &searcher.rootState->cardRandomRng, &searcher.rootState->miscRng,
                               &searcher.rootState->monsterHpRng, &searcher.rootState->potionRng, &searcher.rootState->shuffleRng}) {
            mix(rng->seed0); mix(rng->seed1); mix(rng->counter);
        }
    }
    counting = false;
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::cout << "{\"seconds\":" << seconds << ",\"operations\":" << repetitions * 100
              << ",\"throughput\":" << repetitions * 100 / seconds << ",\"allocations\":" << allocations
              << ",\"checksum\":\"" << checksum << "\"}\n";
}
