// pc_rando_gen_palettes.cpp - the enemy palette randomizer's generator side (docs/RANDOMIZER_PLAN.md
// step R7.2a); see pc_rando_gen_palettes.h.
#include "pc_rando_gen_palettes.h"

#include <cstring>

namespace pcr {

namespace {
#include "generated/rando_palettes.inc"

constexpr int kGroupCount = (int)(sizeof kPaletteGroupTable / sizeof kPaletteGroupTable[0]);
static_assert(kGroupCount == 37, "palette_randomizable_files.txt has 37 groups at 1.10.0_9775811");
} // namespace

int paletteGroupCount() { return kGroupCount; }

const PaletteGroup& paletteGroup(int i) { return kPaletteGroupTable[i]; }

int paletteGroupIndex(const char* name) {
    for (int i = 0; i < kGroupCount; i++) {
        if (std::strcmp(kPaletteGroupTable[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void randomizePalettes(pc::rando::PyRandom& rng, std::vector<PaletteShift>& out) {
    out.clear();
    out.reserve(kGroupCount);
    for (int i = 0; i < kGroupCount; i++) {
        PaletteShift s;
        s.h = (int)rng.randint(20, 340);
        s.v = (int)rng.randint(-40, 40);
        out.push_back(s);
    }
}

Json palettesToJson(const std::vector<PaletteShift>& shifts) {
    Json out = Json::object();
    for (int i = 0; i < kGroupCount && i < (int)shifts.size(); i++) {
        Json pair = Json::array();
        pair.push(Json::integer(shifts[i].h));
        pair.push(Json::integer(shifts[i].v));
        out.set(kPaletteGroupTable[i].name, pair);
    }
    return out;
}

bool palettesFromJson(const Json& j, std::vector<PaletteShift>& out, std::string* err) {
    auto failWith = [&](const std::string& msg) {
        if (err != nullptr) {
            *err = msg;
        }
        out.clear();
        return false;
    };
    if (!j.isObject()) {
        return failWith("palettes: not an object");
    }
    out.assign(kGroupCount, PaletteShift());
    std::vector<bool> seen(kGroupCount, false);
    for (size_t k = 0; k < j.size(); k++) {
        const std::string& name = j.keyAt(k);
        const int g = paletteGroupIndex(name.c_str());
        if (g < 0) {
            return failWith("palettes: unknown group " + name);
        }
        const Json& v = j.at(k);
        if (!v.isArray() || v.size() != 2 || !v.at(0).isInteger() || !v.at(1).isInteger()) {
            return failWith("palettes: " + name + " is not [h_shift, v_shift]");
        }
        const int64_t h = v.at(0).asInt(), vs = v.at(1).asInt();
        if (h < 20 || h > 340 || vs < -40 || vs > 40) {
            return failWith("palettes: " + name + " out of range");
        }
        out[g].h = (int)h;
        out[g].v = (int)vs;
        seen[g] = true;
    }
    for (int g = 0; g < kGroupCount; g++) {
        if (!seen[g]) {
            return failWith(std::string("palettes: missing group ") + kPaletteGroupTable[g].name);
        }
    }
    return true;
}

} // namespace pcr
