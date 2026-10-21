// The randomizer's sidecar housekeeping and the file-select info line (docs/RANDOMIZER_PLAN.md step
// R4.4, decisions D3 and D15).
//
// Clean unused seed files (D3): a sidecar <card dir>/switchwaker-rando-<hash32>.json is used when a
// save slot's mReserve record names it, or when it is the pending seed or the active seed (a new file
// not saved yet). Every other file of that exact name pattern is an orphan: a seed generated and never
// started, or a slot erased or overwritten. sidecarClean removes the orphans of a folder. It is
// conservative:
//   - the records are found by scanning every byte of every .gci file in the folder for the record
//     magic "WWR1" (any version), so it does not depend on the card's layout (the save file's two
//     copies, the three slots, a game it does not know); a stray match only keeps a file;
//   - when a .gci file cannot be read, nothing is removed;
//   - only files named exactly "switchwaker-rando-" + 8 lowercase hex digits + ".json" are touched.
//
// The info line (D15): the text the file select shows for a slot, from its record and its sidecar.
//
// Pure C++ (cos_rando_core, no game code; unit test native/check/rando/test_R4_4.cpp). The game side
// (the card folder, the pending and active seeds, the file-select hook and its ImGui overlay, the menu
// row) is native/src/pc/game_hooks/pc_rando_hooks_fileselect.cpp.
#pragma once

#include "pc_rando_seedfile.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pcr {

using SeedHash = std::array<uint8_t, 16>;

// Appends the hash of every slot record ("WWR1" magic at any offset, the hash at +8) found in
// data[0..len). Duplicates are not appended twice.
void sidecarScanRecords(const uint8_t* data, size_t len, std::vector<SeedHash>* out);

// 1 for "switchwaker-rando-<8 lowercase hex>.json"; *hash32 (may be NULL) gets the number.
bool sidecarNameParse(const std::string& name, uint32_t* hash32);

struct SidecarCleanResult {
    bool ok = false;           // false: nothing was removed (error says why)
    std::string error;         // the first problem ("" none)
    int gciFiles = 0;          // .gci files scanned
    int records = 0;           // distinct records found in them
    std::vector<std::string> removed; // file names, sorted
    std::vector<std::string> kept;    // sidecars kept, sorted
    std::vector<std::string> failed;  // orphans that could not be removed
};

// Removes the orphan sidecars of `dir` (see above). `keep` are extra hashes in use (the pending and
// the active seed). With dryRun nothing is removed, but `removed` lists what would be.
SidecarCleanResult sidecarClean(const std::string& dir, const std::vector<SeedHash>& keep, bool dryRun);

// The seed's "Name1 Name2" (wwrando's get_seed_hash from its permalink and its
// do_not_generate_spoiler_log option); "" when it has no permalink.
std::string seedHashWords(const PcRandoSeed& seed);

// What the file select shows for the selected slot.
enum class FileInfoKind {
    None,    // nothing: a vanilla slot, or a blank one with no pending seed
    Rando,   // a randomized slot whose sidecar loads
    Missing, // a randomized slot whose sidecar is missing or differs: it plays vanilla
    Pending, // a blank slot: the next new file uses the pending seed
};
// The line for a kind: "Randomizer: <seed>  ·  <Name1 Name2>", "New file: randomizer seed <seed>
// (<Name1 Name2>)", "Randomizer: seed file <name> missing: plays vanilla"; "" for None. `seed` and
// `words` are the sidecar's seed name and seedHashWords (Rando) or the pending seed's (Pending); without
// words the line has none. `sidecar` is the missing file's name (Missing).
std::string fileInfoText(FileInfoKind kind, const std::string& seed, const std::string& words,
                         const std::string& sidecar, bool spanish);

} // namespace pcr
