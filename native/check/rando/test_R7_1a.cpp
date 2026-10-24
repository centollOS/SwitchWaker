// cos_rando_test_R7_1a (docs/RANDOMIZER_PLAN.md step R7.1a): the particle-file library
// (features/rando/pc_rando_jpc.h) against gclib, on the synthetic files of fixtures/R7_1a_jpc.txt
// (native/tools/rando/make_jpc_fixture.py; no disc bytes):
//   - scan: particle ids, true sizes (not the header's size field), blocks, TDB1 texture names and
//     the texture list equal what gclib's JPC100 read;
//   - load + save of each file is byte-identical to gclib's JPC100.save(), and so is saving again;
//   - wwrando's warp-pot copy loop (0x8161 and 0x8162 share a texture, 0x0001 already exists) is
//     byte-identical to gclib's result;
//   - colour edits through the accessors, on a loaded file and in place on the raw bytes, give
//     gclib's edited file; the edits read back with alpha kept;
//   - bad input: wrong version, truncation, duplicate ids, a TDB1 id past the texture list, missing
//     blocks, key index past the table, a too small output buffer.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc_rando_jpc.h"

#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

Bytes unhex(const std::string& s) {
    Bytes b;
    for (size_t i = 0; i + 1 < s.size(); i += 2) {
        b.push_back((uint8_t)std::strtoul(s.substr(i, 2).c_str(), nullptr, 16));
    }
    return b;
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

struct Fixture {
    std::map<std::string, Bytes> files, saved, resaved;
    std::vector<std::vector<std::string>> particles, textures, edits, keyEdits;
    Bytes copied, edited;
};

bool loadFixture(Fixture& fx) {
    std::string text;
    if (!rt::readFixture("R7_1a_jpc.txt", text)) {
        return false;
    }
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::vector<std::string> w = split(line, ' ');
        if (w[0] == "file") {
            fx.files[w[1]] = unhex(w[2]);
        } else if (w[0] == "saved") {
            fx.saved[w[1]] = unhex(w[2]);
        } else if (w[0] == "resaved") {
            fx.resaved[w[1]] = unhex(w[2]);
        } else if (w[0] == "particle") {
            fx.particles.push_back(w);
        } else if (w[0] == "texture") {
            fx.textures.push_back(w);
        } else if (w[0] == "copied") {
            fx.copied = unhex(w[1]);
        } else if (w[0] == "edited") {
            fx.edited = unhex(w[1]);
        } else if (w[0] == "edit") {
            fx.edits.push_back(w);
        } else if (w[0] == "editkey") {
            fx.keyEdits.push_back(w);
        }
    }
    return fx.files.size() == 2 && fx.saved.size() == 2 && fx.resaved.size() == 2 && !fx.copied.empty() && !fx.edited.empty();
}

// The first differing byte, for a failure message.
bool sameBytes(const Bytes& got, const Bytes& want, const char* what) {
    if (got == want) {
        return true;
    }
    size_t i = 0;
    while (i < got.size() && i < want.size() && got[i] == want[i]) {
        i++;
    }
    std::fprintf(stderr, "FAIL %s: %zu vs %zu bytes, first difference at 0x%zx\n", what, got.size(),
                 want.size(), i);
    rt::failures()++;
    return false;
}

void checkScan(const Fixture& fx) {
    for (const auto& kv : fx.files) {
        std::vector<PcRandoJpcParticle> ps;
        std::vector<PcRandoJpcTexture> ts;
        RT_CHECK_EQ(pc_rando_jpc_scan(kv.second.data(), kv.second.size(), &ps, &ts), (int)PCR_JPC_OK);
        size_t np = 0, nt = 0;
        for (const auto& w : fx.particles) {
            if (w[1] != kv.first) {
                continue;
            }
            np++;
            size_t i = std::strtoul(w[2].c_str(), nullptr, 10);
            if (!RT_CHECK(i < ps.size())) {
                continue;
            }
            const PcRandoJpcParticle& p = ps[i];
            RT_CHECK_EQ((unsigned)p.id, (unsigned)std::strtoul(w[3].c_str(), nullptr, 16));
            RT_CHECK_EQ(p.size, (uint32_t)std::strtoul(w[4].c_str(), nullptr, 10));
            RT_CHECK_EQ(p.sizeField, (uint32_t)std::strtoul(w[5].c_str(), nullptr, 10));
            std::string blocks;
            for (const PcRandoJpcBlock& b : p.blocks) {
                blocks += (blocks.empty() ? "" : ",") + std::string(b.magic) + ":" + std::to_string(b.size);
            }
            RT_CHECK_EQ(blocks, w[6]);
            std::string names;
            for (uint16_t id : p.textureIds) {
                names += (names.empty() ? "" : ",") + (id < ts.size() ? ts[id].filename : std::string("?"));
            }
            RT_CHECK_EQ(names.empty() ? std::string("-") : names, w[7]);
            RT_CHECK(p.bsp1 >= 0 && p.tdb1 >= 0);
            RT_CHECK(pc_rando_jpc_has_particle(kv.second.data(), kv.second.size(), p.id));
        }
        for (const auto& w : fx.textures) {
            if (w[1] != kv.first) {
                continue;
            }
            nt++;
            size_t i = std::strtoul(w[2].c_str(), nullptr, 10);
            if (RT_CHECK(i < ts.size())) {
                RT_CHECK_EQ(ts[i].filename, w[3]);
                RT_CHECK_EQ(ts[i].size, (uint32_t)std::strtoul(w[4].c_str(), nullptr, 10));
            }
        }
        RT_CHECK_EQ(ps.size(), np);
        RT_CHECK_EQ(ts.size(), nt);
        RT_CHECK(!pc_rando_jpc_has_particle(kv.second.data(), kv.second.size(), 0x7777));
    }
}

void checkSave(const Fixture& fx) {
    for (const auto& kv : fx.files) {
        PcRandoJpcFile f;
        RT_CHECK_EQ(pc_rando_jpc_load(&f, kv.second.data(), kv.second.size()), (int)PCR_JPC_OK);
        Bytes out;
        RT_CHECK_EQ(pc_rando_jpc_save(f, &out), (int)PCR_JPC_OK);
        RT_CHECK_EQ(out.size(), pc_rando_jpc_saved_size(f));
        sameBytes(out, fx.saved.at(kv.first), ("save " + kv.first).c_str());
        // Loading and saving the saved file again gives what gclib gives (the same bytes, except a
        // TEX1 whose image did not start at 0x40: gclib leaves its header offset stale).
        PcRandoJpcFile g;
        RT_CHECK_EQ(pc_rando_jpc_load(&g, out.data(), out.size()), (int)PCR_JPC_OK);
        Bytes again;
        RT_CHECK_EQ(pc_rando_jpc_save(g, &again), (int)PCR_JPC_OK);
        sameBytes(again, fx.resaved.at(kv.first), ("save again " + kv.first).c_str());
        // save_into: exact capacity works, one byte less is refused.
        Bytes buf(out.size());
        size_t n = 0;
        RT_CHECK_EQ(pc_rando_jpc_save_into(f, buf.data(), buf.size(), &n), (int)PCR_JPC_OK);
        RT_CHECK_EQ(n, out.size());
        RT_CHECK(buf == out);
        RT_CHECK_EQ(pc_rando_jpc_save_into(f, buf.data(), buf.size() - 1, &n), (int)PCR_JPC_NO_SPACE);
    }
}

void checkCopy(const Fixture& fx) {
    PcRandoJpcFile dst, src;
    RT_CHECK_EQ(pc_rando_jpc_load(&dst, fx.files.at("A").data(), fx.files.at("A").size()), (int)PCR_JPC_OK);
    RT_CHECK_EQ(pc_rando_jpc_load(&src, fx.files.at("B").data(), fx.files.at("B").size()), (int)PCR_JPC_OK);
    // tweaks.py add_inter_dungeon_warp_pots order; 0x0001 is already in A.
    RT_CHECK_EQ(pc_rando_jpc_copy_particle(&dst, src, 0x8161), (int)PCR_JPC_OK);
    RT_CHECK_EQ(pc_rando_jpc_copy_particle(&dst, src, 0x8162), (int)PCR_JPC_OK);
    RT_CHECK_EQ(pc_rando_jpc_copy_particle(&dst, src, 0x0001), (int)PCR_JPC_EXISTS);
    RT_CHECK_EQ(pc_rando_jpc_copy_particle(&dst, src, 0x8161), (int)PCR_JPC_EXISTS);
    RT_CHECK_EQ(pc_rando_jpc_copy_particle(&dst, src, 0x4242), (int)PCR_JPC_NO_PARTICLE);
    RT_CHECK_EQ(dst.particles.size(), (size_t)4);
    // texC is shared by both copies and added once; A's own "shared" is kept, not B's.
    RT_CHECK_EQ(dst.textures.size(), (size_t)4);
    RT_CHECK_EQ(pc_rando_jpc_find_texture(dst, "texC"), 3);
    Bytes out;
    RT_CHECK_EQ(pc_rando_jpc_save(dst, &out), (int)PCR_JPC_OK);
    sameBytes(out, fx.copied, "copy");
    // The copy's TDB1 ids now index A's list: shared -> 1, texC -> 3.
    std::vector<PcRandoJpcParticle> ps;
    std::vector<PcRandoJpcTexture> ts;
    RT_CHECK_EQ(pc_rando_jpc_scan(out.data(), out.size(), &ps, &ts), (int)PCR_JPC_OK);
    if (RT_CHECK_EQ(ps.size(), (size_t)4) && RT_CHECK_EQ(ps[2].textureIds.size(), (size_t)2)) {
        RT_CHECK_EQ((int)ps[2].textureIds[0], 1);
        RT_CHECK_EQ((int)ps[2].textureIds[1], 3);
    }
}

int colorSlot(const std::string& s) {
    if (s == "bsp1_prm") return PCR_JPC_BSP1_PRM;
    if (s == "bsp1_env") return PCR_JPC_BSP1_ENV;
    if (s == "ssp1_prm") return PCR_JPC_SSP1_PRM;
    return PCR_JPC_SSP1_ENV;
}

// Applies the fixture's edits to the particles found in base (rgb set, alpha kept) and reads them back.
void applyEdits(const Fixture& fx, uint8_t* base, const std::vector<PcRandoJpcParticle>& ps,
                const std::vector<uint8_t*>& bases) {
    auto find = [&](const std::string& hex) -> int {
        uint16_t id = (uint16_t)std::strtoul(hex.c_str(), nullptr, 16);
        for (size_t i = 0; i < ps.size(); i++) {
            if (ps[i].id == id) {
                return (int)i;
            }
        }
        return -1;
    };
    for (const auto& w : fx.edits) {
        int i = find(w[1]);
        if (!RT_CHECK(i >= 0)) {
            continue;
        }
        uint8_t* b = bases.empty() ? base : bases[i];
        int slot = colorSlot(w[2]);
        PcRandoJpcRgba c;
        RT_CHECK_EQ(pc_rando_jpc_get_color(b, ps[i], slot, &c), (int)PCR_JPC_OK);
        uint8_t alpha = c.a;
        c.r = (uint8_t)std::atoi(w[3].c_str());
        c.g = (uint8_t)std::atoi(w[4].c_str());
        c.b = (uint8_t)std::atoi(w[5].c_str());
        RT_CHECK_EQ(pc_rando_jpc_set_color(b, ps[i], slot, c), (int)PCR_JPC_OK);
        PcRandoJpcRgba back;
        RT_CHECK_EQ(pc_rando_jpc_get_color(b, ps[i], slot, &back), (int)PCR_JPC_OK);
        RT_CHECK(back.r == c.r && back.g == c.g && back.b == c.b && back.a == alpha);
    }
    for (const auto& w : fx.keyEdits) {
        int i = find(w[1]);
        if (!RT_CHECK(i >= 0)) {
            continue;
        }
        uint8_t* b = bases.empty() ? base : bases[i];
        int slot = w[2] == "prm" ? PCR_JPC_BSP1_PRM : PCR_JPC_BSP1_ENV;
        uint32_t k = (uint32_t)std::atoi(w[3].c_str());
        uint16_t time = 0, time2 = 0;
        PcRandoJpcRgba c;
        RT_CHECK_EQ(pc_rando_jpc_get_key(b, ps[i], slot, k, &time, &c), (int)PCR_JPC_OK);
        uint8_t alpha = c.a;
        c.r = (uint8_t)std::atoi(w[4].c_str());
        c.g = (uint8_t)std::atoi(w[5].c_str());
        c.b = (uint8_t)std::atoi(w[6].c_str());
        RT_CHECK_EQ(pc_rando_jpc_set_key_color(b, ps[i], slot, k, c), (int)PCR_JPC_OK);
        PcRandoJpcRgba back;
        RT_CHECK_EQ(pc_rando_jpc_get_key(b, ps[i], slot, k, &time2, &back), (int)PCR_JPC_OK);
        RT_CHECK(back.r == c.r && back.g == c.g && back.b == c.b && back.a == alpha && time2 == time);
    }
}

void checkColors(const Fixture& fx) {
    const Bytes& a = fx.files.at("A");
    // On a loaded file.
    PcRandoJpcFile f;
    RT_CHECK_EQ(pc_rando_jpc_load(&f, a.data(), a.size()), (int)PCR_JPC_OK);
    std::vector<PcRandoJpcParticle> infos;
    std::vector<uint8_t*> bases;
    for (PcRandoJpcFile::Particle& p : f.particles) {
        infos.push_back(p.info);
        bases.push_back(p.bytes.data());
    }
    applyEdits(fx, nullptr, infos, bases);
    Bytes out;
    RT_CHECK_EQ(pc_rando_jpc_save(f, &out), (int)PCR_JPC_OK);
    sameBytes(out, fx.edited, "edited (loaded file)");

    // In place on the raw bytes: the same edits, the size unchanged, then gclib's layout on save.
    Bytes raw = a;
    std::vector<PcRandoJpcParticle> ps;
    RT_CHECK_EQ(pc_rando_jpc_scan(raw.data(), raw.size(), &ps, nullptr), (int)PCR_JPC_OK);
    applyEdits(fx, raw.data(), ps, {});
    RT_CHECK_EQ(raw.size(), a.size());
    size_t changed = 0;
    for (size_t i = 0; i < raw.size(); i++) {
        changed += raw[i] != a[i];
    }
    RT_CHECK(changed > 0 && changed <= (fx.edits.size() + fx.keyEdits.size()) * 3);
    PcRandoJpcFile g;
    RT_CHECK_EQ(pc_rando_jpc_load(&g, raw.data(), raw.size()), (int)PCR_JPC_OK);
    Bytes out2;
    RT_CHECK_EQ(pc_rando_jpc_save(g, &out2), (int)PCR_JPC_OK);
    sameBytes(out2, fx.edited, "edited (in place)");

    // Missing blocks and bad indexes. Particle 0x0002 of A has no SSP1 and no keys.
    if (RT_CHECK_EQ(ps.size(), (size_t)2)) {
        PcRandoJpcRgba c;
        RT_CHECK_EQ(pc_rando_jpc_get_color(raw.data(), ps[1], PCR_JPC_SSP1_PRM, &c), (int)PCR_JPC_NO_BLOCK);
        RT_CHECK_EQ(pc_rando_jpc_set_color(raw.data(), ps[1], PCR_JPC_SSP1_ENV, c), (int)PCR_JPC_NO_BLOCK);
        RT_CHECK_EQ(pc_rando_jpc_get_color(raw.data(), ps[1], 99, &c), (int)PCR_JPC_NO_BLOCK);
        RT_CHECK_EQ(pc_rando_jpc_key_count(raw.data(), ps[1], PCR_JPC_BSP1_PRM), 0u);
        RT_CHECK_EQ(pc_rando_jpc_get_key(raw.data(), ps[1], PCR_JPC_BSP1_PRM, 0, nullptr, &c),
                    (int)PCR_JPC_BAD_INDEX);
        RT_CHECK_EQ(pc_rando_jpc_key_count(raw.data(), ps[0], PCR_JPC_BSP1_PRM), 2u);
        RT_CHECK_EQ(pc_rando_jpc_key_count(raw.data(), ps[0], PCR_JPC_BSP1_ENV), 1u);
        RT_CHECK_EQ(pc_rando_jpc_key_count(raw.data(), ps[0], PCR_JPC_SSP1_PRM), 0u);
        RT_CHECK_EQ(pc_rando_jpc_set_key_color(raw.data(), ps[0], PCR_JPC_BSP1_ENV, 1, c), (int)PCR_JPC_BAD_INDEX);
        uint16_t t = 0;
        RT_CHECK_EQ(pc_rando_jpc_get_key(raw.data(), ps[0], PCR_JPC_BSP1_PRM, 1, &t, &c), (int)PCR_JPC_OK);
        RT_CHECK_EQ((int)t, 15); // the fixture's second prm key
    }
}

void checkBadInput(const Fixture& fx) {
    const Bytes& a = fx.files.at("A");
    std::vector<PcRandoJpcParticle> ps;
    RT_CHECK_EQ(pc_rando_jpc_scan(nullptr, 0, &ps, nullptr), (int)PCR_JPC_BAD_FILE);
    Bytes v = a;
    std::memcpy(v.data(), "JPAC2-10", 8);
    RT_CHECK_EQ(pc_rando_jpc_scan(v.data(), v.size(), &ps, nullptr), (int)PCR_JPC_BAD_VERSION);
    v = a;
    v[0] = 'X';
    RT_CHECK_EQ(pc_rando_jpc_scan(v.data(), v.size(), &ps, nullptr), (int)PCR_JPC_BAD_FILE);
    // Every truncation fails cleanly.
    int truncOk = 0;
    for (size_t n = 0; n < a.size(); n += 7) {
        truncOk += pc_rando_jpc_scan(a.data(), n, &ps, nullptr) == PCR_JPC_OK;
    }
    RT_CHECK_EQ(truncOk, 0);
    // Duplicate particle id: particle 2 gets particle 1's id.
    std::vector<PcRandoJpcParticle> orig;
    RT_CHECK_EQ(pc_rando_jpc_scan(a.data(), a.size(), &orig, nullptr), (int)PCR_JPC_OK);
    if (RT_CHECK_EQ(orig.size(), (size_t)2)) {
        v = a;
        v[orig[1].offset + 0x18] = v[orig[0].offset + 0x18];
        v[orig[1].offset + 0x19] = v[orig[0].offset + 0x19];
        RT_CHECK_EQ(pc_rando_jpc_scan(v.data(), v.size(), &ps, nullptr), (int)PCR_JPC_DUPLICATE);
        // A TDB1 id past the 3 textures.
        v = a;
        const PcRandoJpcBlock& tdb = orig[1].blocks[orig[1].tdb1];
        v[tdb.offset + 0xC] = 0;
        v[tdb.offset + 0xD] = 3;
        RT_CHECK_EQ(pc_rando_jpc_scan(v.data(), v.size(), &ps, nullptr), (int)PCR_JPC_BAD_FILE);
        // A BSP1 key table past its block.
        v = a;
        const PcRandoJpcBlock& bsp = orig[0].blocks[orig[0].bsp1];
        v[bsp.offset + 0x28] = 0x40;
        RT_CHECK_EQ(pc_rando_jpc_scan(v.data(), v.size(), &ps, nullptr), (int)PCR_JPC_BAD_FILE);
        // A block size of 0 would loop forever in gclib; refused.
        v = a;
        std::memset(&v[bsp.offset + 4], 0, 4);
        RT_CHECK_EQ(pc_rando_jpc_scan(v.data(), v.size(), &ps, nullptr), (int)PCR_JPC_BAD_FILE);
    }
    // Two textures with one filename (B's "texC" renamed "shared").
    std::vector<PcRandoJpcTexture> ts;
    const Bytes& b = fx.files.at("B");
    RT_CHECK_EQ(pc_rando_jpc_scan(b.data(), b.size(), nullptr, &ts), (int)PCR_JPC_OK);
    if (RT_CHECK_EQ(ts.size(), (size_t)2)) {
        v = b;
        std::memset(&v[ts[0].offset + 0xC], 0, 0x14);
        std::memcpy(&v[ts[0].offset + 0xC], "shared", 6);
        RT_CHECK_EQ(pc_rando_jpc_scan(v.data(), v.size(), nullptr, nullptr), (int)PCR_JPC_DUPLICATE);
        // An unknown image format.
        v = b;
        v[ts[0].offset + 0x20] = 0x7;
        RT_CHECK_EQ(pc_rando_jpc_scan(v.data(), v.size(), nullptr, nullptr), (int)PCR_JPC_BAD_FILE);
    }
    PcRandoJpcFile f;
    RT_CHECK_EQ(pc_rando_jpc_load(&f, a.data(), 0x10), (int)PCR_JPC_BAD_FILE);
    RT_CHECK_EQ(std::string(pc_rando_jpc_result_name(PCR_JPC_EXISTS)), std::string("exists"));
}

} // namespace

int main() {
    Fixture fx;
    if (!RT_CHECK(loadFixture(fx))) {
        return rt::finish();
    }
    checkScan(fx);
    checkSave(fx);
    checkCopy(fx);
    checkColors(fx);
    checkBadInput(fx);
    return rt::finish();
}
