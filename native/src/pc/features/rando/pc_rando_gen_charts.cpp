// pc_rando_gen_charts.cpp - ChartRandomizer._randomize (docs/RANDOMIZER_PLAN.md step R6.6); see
// pc_rando_gen_charts.h.
#include "pc_rando_gen_charts.h"

namespace pcr {

bool randomizeCharts(pc::rando::PyRandom& rng, std::vector<std::string>& islandCharts) {
    if (islandCharts.size() != 50) {
        return false;
    }
    // original_item_names = list(self.island_number_to_chart_name.values()): the dict is built in
    // island order 1..49 and _randomize runs once, so its values are islandCharts[1..49].
    const std::vector<std::string> originalItemNames(islandCharts.begin() + 1, islandCharts.end());
    // shuffled_island_numbers = list(keys()); self.rng.shuffle(shuffled_island_numbers)
    std::vector<int> shuffledIslandNumbers;
    shuffledIslandNumbers.reserve(49);
    for (int island = 1; island <= 49; island++) {
        shuffledIslandNumbers.push_back(island);
    }
    rng.shuffle(shuffledIslandNumbers);
    for (const std::string& name : originalItemNames) {
        const int island = shuffledIslandNumbers.back(); // shuffled_island_numbers.pop()
        shuffledIslandNumbers.pop_back();
        islandCharts[island] = name;
    }
    return true;
}

} // namespace pcr
