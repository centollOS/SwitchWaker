// Hint placement at runtime, pure part (docs/RANDOMIZER_PLAN.md step R5.3; pc_rando_hint_place.h).
#include "pc_rando_hint_place.h"

#include <cmath>
#include <cstring>

namespace pcr {

namespace {

// The sea stage.dzs MULT entries of islands 1..49 (x_pos, z_pos), as the USA disc holds them: the
// 7x7 grid of sea rooms, 100000 units apart, island 1 at the north-west corner. Its first MULT
// (room 0, at 0,0) is not an island and never a look target.
const float kMult[49][2] = {
    {-300000.0f, -300000.0f}, {-200000.0f, -300000.0f}, {-100000.0f, -300000.0f}, {0.0f, -300000.0f},
    {100000.0f, -300000.0f},  {200000.0f, -300000.0f},  {300000.0f, -300000.0f},  // 1-7
    {-300000.0f, -200000.0f}, {-200000.0f, -200000.0f}, {-100000.0f, -200000.0f}, {0.0f, -200000.0f},
    {100000.0f, -200000.0f},  {200000.0f, -200000.0f},  {300000.0f, -200000.0f},  // 8-14
    {-300000.0f, -100000.0f}, {-200000.0f, -100000.0f}, {-100000.0f, -100000.0f}, {0.0f, -100000.0f},
    {100000.0f, -100000.0f},  {200000.0f, -100000.0f},  {300000.0f, -100000.0f},  // 15-21
    {-300000.0f, 0.0f},       {-200000.0f, 0.0f},       {-100000.0f, 0.0f},       {0.0f, 0.0f},
    {100000.0f, 0.0f},        {200000.0f, 0.0f},        {300000.0f, 0.0f},        // 22-28
    {-300000.0f, 100000.0f},  {-200000.0f, 100000.0f},  {-100000.0f, 100000.0f},  {0.0f, 100000.0f},
    {100000.0f, 100000.0f},   {200000.0f, 100000.0f},   {300000.0f, 100000.0f},   // 29-35
    {-300000.0f, 200000.0f},  {-200000.0f, 200000.0f},  {-100000.0f, 200000.0f},  {0.0f, 200000.0f},
    {100000.0f, 200000.0f},   {200000.0f, 200000.0f},   {300000.0f, 200000.0f},   // 36-42
    {-300000.0f, 300000.0f},  {-200000.0f, 300000.0f},  {-100000.0f, 300000.0f},  {0.0f, 300000.0f},
    {100000.0f, 300000.0f},   {200000.0f, 300000.0f},   {300000.0f, 300000.0f},   // 43-49
};

// wwlib dzx.py ACTR (0x20 bytes): name[8], params u32, x/y/z f32, x/y/z rot u16, enemy number u16.
constexpr uint32_t kActrParams = 0x08, kActrX = 0x0C, kActrZ = 0x14, kActrYRot = 0x1A;

} // namespace

bool seaIslandMultPos(int island, float* x, float* z) {
    if (island < 1 || island > 49) {
        return false;
    }
    *x = kMult[island - 1][0];
    *z = kMult[island - 1][1];
    return true;
}

bool hohoYRot(float x, float z, int island, uint16_t* yRot) {
    float mx = 0, mz = 0;
    if (!seaIslandMultPos(island, &mx, &mz)) {
        return false;
    }
    // math.atan2(dest.x_pos - actor.x_pos, dest.z_pos - actor.z_pos), on Python floats (doubles) made
    // from the files' f32 values.
    const double rad = std::atan2((double)mx - (double)x, (double)mz - (double)z);
    const double scaled = rad * (32768.0 / 3.14159265358979323846); // math.pi
    const long long truncated = (long long)scaled;                  // int(): towards zero
    *yRot = (uint16_t)(((truncated % 0x10000) + 0x10000) % 0x10000);
    return true;
}

int hohoIndexOfRoom(int room) {
    for (int i = 0; i < kHohoCount; i++) {
        if (kHohoIndexToIsland[i] == room) {
            return i;
        }
    }
    return -1;
}

bool patchHohoRoom(PcRandoDzx* dzx, int room, const HintsResult& h, HohoPatchResult* out) {
    HohoPatchResult r;
    const int index = hohoIndexOfRoom(room);
    const int look = index >= 0 && (size_t)index < h.hohoLookIsland.size() ? h.hohoLookIsland[index] : -1;
    if (index < 0 || h.hoho.empty() || look < 1) {
        if (out != nullptr) {
            *out = r;
        }
        return false;
    }
    for (int layer = -1; layer < 12; layer++) {
        char tag[5];
        if (!pc_rando_dzx_tag("ACTR", layer, tag)) {
            continue;
        }
        const uint32_t n = pc_rando_dzx_count(dzx, tag);
        for (uint32_t i = 0; i < n; i++) {
            uint8_t* e = pc_rando_dzx_entry(dzx, tag, i);
            if (e == nullptr || std::memcmp(e, "Ah\0\0\0\0\0\0", 8) != 0) {
                continue;
            }
            r.found++;
            if ((int)(pc_rando_dzx_be32(e + kActrParams) & 0xFF) != index) {
                r.mismatch++;
                continue;
            }
            uint16_t y = 0;
            hohoYRot(pc_rando_dzx_bef32(e + kActrX), pc_rando_dzx_bef32(e + kActrZ), look, &y);
            const PcRandoDzxFingerprint fp = {"Ah", -1, nullptr, 0};
            if (pc_rando_dzx_set_u16(dzx, tag, i, kActrYRot, y, &fp) == PCR_DZX_OK) {
                r.turned++;
                r.yRot = y;
            }
        }
    }
    if (out != nullptr) {
        *out = r;
    }
    return r.turned > 0;
}

HintSeedOptions hintSeedOptions(const Json& options) {
    HintSeedOptions o;
    auto flag = [&](const char* key, bool def) {
        const Json* v = options.find(key);
        return v != nullptr && v->isBool() ? v->asBool() : def;
    };
    o.cryptic = flag("cryptic_hints", o.cryptic);
    o.importance = flag("hint_importance", o.importance);
    o.keylunacy = flag("keylunacy", o.keylunacy);
    return o;
}

} // namespace pcr
