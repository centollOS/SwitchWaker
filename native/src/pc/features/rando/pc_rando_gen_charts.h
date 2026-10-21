// pc_rando_gen_charts.h - the chart randomizer of the seed generator (docs/RANDOMIZER_PLAN.md step
// R6.6): a port of wwrando 1.10.0_9775811's randomizers/charts.py ChartRandomizer._randomize.
//
// Pure C++ (no game headers): cos_rando_core, the unit tests and cos_rando_cli link it. The runtime
// side (the sea chart table and the salvage SCOBs) is features/rando/pc_rando_charts.h.
#pragma once

#include <string>
#include <vector>

#include "pc_rando_rng.h"

namespace pcr {

// ChartRandomizer._randomize on charts.island_number_to_chart_name, held as islandCharts[1..49]
// (index 0 unused; LogicState::islandCharts): the 49 chart names in island order are taken as the
// "original item names"; the island numbers 1..49 are shuffled with `rng` (random.shuffle), then each
// original name, in order, goes to the island popped from the end of the shuffled list. The caller
// then runs Logic::updateChartMacros, as wwrando does. False when islandCharts does not hold 50
// entries.
bool randomizeCharts(pc::rando::PyRandom& rng, std::vector<std::string>& islandCharts);

} // namespace pcr
