// cos_rando_test_R2_1 (docs/RANDOMIZER_PLAN.md step R2.1): the chest part of the placement patcher
// (features/rando/pc_rando_place_dzx.h) on synthetic stage.dzs / room.dzr files built from R0.5's
// key table (each chest key's entry holds the key's 8-byte name and its disc value in angle z's high
// byte; the low byte and the rest of the entry are filler the patcher must not touch):
//   - with pc_rando() 0 nothing is patched;
//   - every one of the 182 chest keys (166 locations, multi-path ones included: Ghost Ship, Pirate
//     Ship, Windfall's transparent chest in TRE1/3/5, majroom/ma2room/ma3room) gets its placed item
//     at +0x1C, only there, and its status says "written";
//   - the identity placement writes nothing ("unchanged");
//   - a file of another stage directory (ma2room vs majroom), arc, name or type is left alone;
//   - D9: a renamed entry or a changed item byte is a mismatch and stays vanilla, the others are
//     still written; a missing chunk or index is "missing";
//   - the stats counters.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc_rando_dzx.h"
#include "pc_rando_lockeys.h"
#include "pc_rando_place_dzx.h"
#include "pc_rando_resources.h"

#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;
using pcr::DzxField;
using pcr::KeyKind;
using pcr::LocKey;

void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

bool isChest(const LocKey& k) {
    return k.kind == KeyKind::Dzx && k.field == DzxField::TboxItem;
}

// The filler of entry `index` of chunk `tag` (any byte the patcher must leave as it is).
uint8_t filler(const std::string& tag, int index, int off) {
    return (uint8_t)(0x40 + tag[3] * 3 + index * 7 + off);
}

struct File {
    std::string stage, arc;
    Bytes bytes;
    std::map<std::string, int> chunkOff; // tag -> offset of its first entry
};

// A dzx holding, for every chest key of (stage, arc), its chunk with enough entries; the key's entry
// gets its name and disc value, every other byte filler.
File makeFile(const std::string& stage, const std::string& arc, const char* dropTag = nullptr) {
    std::map<std::string, int> counts;
    for (const LocKey& k : pcr::location_keys()) {
        if (isChest(k) && stage == k.stage && arc == k.arc && (dropTag == nullptr || strcmp(dropTag, k.tag) != 0)) {
            int& n = counts[k.tag];
            n = n > k.index + 1 ? n : k.index + 1;
        }
    }
    File f{stage, arc, {}, {}};
    size_t size = 4 + 12 * counts.size();
    for (const auto& c : counts) {
        size += 0x20 * (size_t)c.second;
    }
    f.bytes.assign(size, 0);
    put32(&f.bytes[0], (uint32_t)counts.size());
    size_t hdr = 4, data = 4 + 12 * counts.size();
    for (const auto& c : counts) {
        memcpy(&f.bytes[hdr], c.first.data(), 4);
        put32(&f.bytes[hdr + 4], (uint32_t)c.second);
        put32(&f.bytes[hdr + 8], (uint32_t)data);
        f.chunkOff[c.first] = (int)data;
        for (int i = 0; i < c.second; i++) {
            for (int b = 0; b < 0x20; b++) {
                f.bytes[data + i * 0x20 + b] = filler(c.first, i, b);
            }
        }
        hdr += 12;
        data += 0x20 * (size_t)c.second;
    }
    for (const LocKey& k : pcr::location_keys()) {
        if (isChest(k) && stage == k.stage && arc == k.arc && f.chunkOff.count(k.tag)) {
            uint8_t* e = &f.bytes[f.chunkOff[k.tag] + k.index * 0x20];
            memcpy(e, k.fp_name, 8);
            pcr::dzx_key_write(k, e, k.disc_byte);
        }
    }
    return f;
}

void patch(File& f, const char* stage = nullptr, const char* arc = nullptr, const char* name = nullptr,
           uint32_t type = 0) {
    PcRandoResFile rf = {};
    const bool isStage = f.arc == "Stage";
    rf.dir = "";
    rf.stage = stage != nullptr ? stage : f.stage.c_str();
    rf.arc = arc != nullptr ? arc : f.arc.c_str();
    rf.room = -1;
    rf.name = name != nullptr ? name : isStage ? "stage.dzs" : "room.dzr";
    rf.type = type != 0 ? type : isStage ? PCR_RES_DZS : PCR_RES_DZR;
    rf.data = f.bytes.data();
    rf.size = f.bytes.size();
    pc_rando_place_dzx_patch(&rf);
}

const uint8_t* entryOf(const File& f, const LocKey& k) {
    auto it = f.chunkOff.find(k.tag);
    return it == f.chunkOff.end() ? nullptr : &f.bytes[it->second + k.index * 0x20];
}

// The key's index in pcr::location_keys() (the status table's index).
int keyIndex(const LocKey& k) {
    return (int)(&k - pcr::location_keys().data);
}

std::vector<std::pair<std::string, std::string>> chestFiles() {
    std::vector<std::pair<std::string, std::string>> out;
    for (const LocKey& k : pcr::location_keys()) {
        if (!isChest(k)) {
            continue;
        }
        std::pair<std::string, std::string> p(k.stage, k.arc);
        bool seen = false;
        for (const auto& q : out) {
            seen = seen || q == p;
        }
        if (!seen) {
            out.push_back(p);
        }
    }
    return out;
}

uint8_t sItems[PCR_LOC_COUNT];
uint8_t sIdentity[PCR_LOC_COUNT];

uint8_t placed(int loc) {
    return (uint8_t)(0x20 + (loc * 37) % 0xC0);
}

const LocKey* firstKey(int loc, const char* stage, const char* tag) {
    for (const LocKey& k : pcr::location_keys(loc)) {
        if (isChest(k) && strcmp(k.stage, stage) == 0 && strcmp(k.tag, tag) == 0) {
            return &k;
        }
    }
    return nullptr;
}

} // namespace

int main() {
    // The table: 182 chest keys over 166 locations (plan: 186 TRES paths, 4 of them R2.10's added
    // chests).
    int nKeys = 0;
    std::vector<int> locs;
    for (const LocKey& k : pcr::location_keys()) {
        if (isChest(k)) {
            nKeys++;
            if (locs.empty() || locs.back() != k.loc) {
                locs.push_back(k.loc);
            }
        }
    }
    RT_CHECK_EQ(nKeys, 182);
    RT_CHECK_EQ((int)locs.size(), 166);
    RT_CHECK_EQ(pc_rando_place_dzx_handles((int)DzxField::TboxItem), 1);
    RT_CHECK_EQ(pc_rando_place_dzx_handles((int)DzxField::ItemItem), 0);
    RT_CHECK_EQ(pc_rando_place_dzx_handles((int)DzxField::SalvageItem), 0);
    for (int l = 0; l < PCR_LOC_COUNT; l++) {
        sItems[l] = placed(l);
        sIdentity[l] = 0xFF;
    }
    for (const LocKey& k : pcr::location_keys()) {
        if (isChest(k)) {
            sIdentity[k.loc] = k.vanilla;
            RT_CHECK(k.flags & pcr::KEY_FP_ITEM);
            RT_CHECK_EQ(k.disc_byte, k.vanilla);
        }
    }
    const auto files = chestFiles();

    // 1. Off: nothing is patched.
    RT_CHECK_EQ(pc_rando(), 0);
    pc_rando_place_dzx_reset();
    for (const auto& p : files) {
        File f = makeFile(p.first, p.second);
        const Bytes before = f.bytes;
        patch(f);
        RT_CHECK(f.bytes == before);
    }
    PcRandoPlaceStats st;
    pc_rando_place_dzx_stats(&st);
    RT_CHECK_EQ(st.files, 0u);

    // 2. A placement: every chest key gets its item at +0x1C (high byte of angle z), nothing else changes.
    pc_rando_set_loc_items(sItems, PCR_LOC_COUNT);
    pc_rando_set_active(nullptr, nullptr);
    RT_CHECK_EQ(pc_rando(), 1);
    pc_rando_place_dzx_reset();
    int written = 0;
    for (const auto& p : files) {
        File f = makeFile(p.first, p.second);
        Bytes want = f.bytes;
        for (const LocKey& k : pcr::location_keys()) {
            if (isChest(k) && p.first == k.stage && p.second == k.arc) {
                want[entryOf(f, k) - f.bytes.data() + 0x1C] = sItems[k.loc];
            }
        }
        patch(f);
        RT_CHECK(f.bytes == want);
        for (const LocKey& k : pcr::location_keys()) {
            if (isChest(k) && p.first == k.stage && p.second == k.arc) {
                const PcRandoPlaceStatus* s = pc_rando_place_dzx_status(keyIndex(k));
                RT_CHECK(s != nullptr);
                if (s == nullptr) {
                    continue;
                }
                const int want = sItems[k.loc] == k.vanilla ? PCR_PLACE_SAME : PCR_PLACE_WRITTEN;
                RT_CHECK_EQ((int)s->result, want);
                RT_CHECK_EQ(s->item, sItems[k.loc]);
                RT_CHECK_EQ(s->found, k.vanilla);
                RT_CHECK_EQ(s->patches, 1u);
                RT_CHECK_EQ(s->entry_off, (uint32_t)(entryOf(f, k) - f.bytes.data()));
                const uint8_t* e = entryOf(f, k);
                RT_CHECK_EQ(pcr::dzx_key_read(k, e), (uint32_t)sItems[k.loc]);
                RT_CHECK_EQ(e[0x1D], filler(k.tag, k.index, 0x1D)); // angle z's low byte (chest flags)
                written += s->result == PCR_PLACE_WRITTEN;
            }
        }
    }
    pc_rando_place_dzx_stats(&st);
    RT_CHECK_EQ(st.files, (uint32_t)files.size());
    RT_CHECK_EQ((int)(st.written + st.same), 182);
    RT_CHECK_EQ((int)st.written, written);
    RT_CHECK_EQ(st.mismatch + st.missing, 0u);

    // Multi-path locations: every path gets the same item.
    for (int loc : {PCR_LOC_THE_GREAT_SEA_GHOST_SHIP, PCR_LOC_WINDFALL_ISLAND_PIRATE_SHIP,
                    PCR_LOC_WINDFALL_ISLAND_TRANSPARENT_CHEST, PCR_LOC_FORSAKEN_FORTRESS_CHEST_ON_BED}) {
        int paths = 0;
        for (const LocKey& k : pcr::location_keys(loc)) {
            const PcRandoPlaceStatus* s = pc_rando_place_dzx_status(keyIndex(k));
            RT_CHECK(s != nullptr && s->patches == 1 && s->item == sItems[loc]);
            paths++;
        }
        RT_CHECK(paths >= 2);
    }

    // 3. Other files are left alone: ma2room's keys are not majroom's; Stage vs Room arcs; names, types.
    {
        pc_rando_place_dzx_reset();
        File f = makeFile("majroom", "Stage");
        const Bytes before = f.bytes;
        patch(f, "ma2room"); // the same four chests, but ma2room's keys
        File g = makeFile("majroom", "Stage");
        const LocKey* bed = firstKey(PCR_LOC_FORSAKEN_FORTRESS_CHEST_ON_BED, "majroom", "TRES");
        RT_CHECK(bed != nullptr);
        if (bed != nullptr) {
            RT_CHECK_EQ(pc_rando_place_dzx_status(keyIndex(*bed))->patches, 0u);
        }
        patch(g, "Asoko");
        RT_CHECK(g.bytes == before);
        patch(g, nullptr, "Room0");
        RT_CHECK(g.bytes == before);
        patch(g, nullptr, nullptr, "room.dzr");
        RT_CHECK(g.bytes == before);
        patch(g, nullptr, nullptr, nullptr, PCR_RES_DAT);
        RT_CHECK(g.bytes == before);
        patch(g, "");
        RT_CHECK(g.bytes == before);
        File r = makeFile("sea", "Room11");
        const Bytes rBefore = r.bytes;
        patch(r, nullptr, "Room12");
        RT_CHECK(r.bytes == rBefore);
        patch(r, nullptr, nullptr, "stage.dzs", PCR_RES_DZS);
        RT_CHECK(r.bytes == rBefore);
    }

    // 4. The identity placement writes nothing.
    pc_rando_set_loc_items(sIdentity, PCR_LOC_COUNT);
    pc_rando_place_dzx_reset();
    for (const auto& p : files) {
        File f = makeFile(p.first, p.second);
        const Bytes before = f.bytes;
        patch(f);
        RT_CHECK(f.bytes == before);
    }
    pc_rando_place_dzx_stats(&st);
    RT_CHECK_EQ(st.written, 0u);
    RT_CHECK_EQ(st.same, 182u);

    // 5. D9 and missing entries (Tetra's ship, layer chunks TRE2 and TRE3: the Pirate Ship chest).
    pc_rando_set_loc_items(sItems, PCR_LOC_COUNT);
    {
        const LocKey* k2 = firstKey(PCR_LOC_WINDFALL_ISLAND_PIRATE_SHIP, "Asoko", "TRE2");
        const LocKey* k3 = firstKey(PCR_LOC_WINDFALL_ISLAND_PIRATE_SHIP, "Asoko", "TRE3");
        RT_CHECK(k2 != nullptr && k3 != nullptr);
        if (k2 != nullptr && k3 != nullptr) {
            // a renamed entry (replaced content): mismatch, left vanilla; the other layer still written
            pc_rando_place_dzx_reset();
            File f = makeFile("Asoko", "Stage");
            uint8_t* e2 = &f.bytes[f.chunkOff["TRE2"] + k2->index * 0x20];
            e2[0] ^= 0x20;
            const Bytes e2Before(e2, e2 + 0x20);
            patch(f);
            RT_CHECK(Bytes(e2, e2 + 0x20) == e2Before);
            RT_CHECK_EQ((int)pc_rando_place_dzx_status(keyIndex(*k2))->result, (int)PCR_PLACE_MISMATCH);
            RT_CHECK_EQ((int)pc_rando_place_dzx_status(keyIndex(*k3))->result, (int)PCR_PLACE_WRITTEN);
            RT_CHECK_EQ(pcr::dzx_key_read(*k3, entryOf(f, *k3)), (uint32_t)sItems[k3->loc]);
            // another item byte than the disc's (a modded chest): mismatch, its found value reported
            pc_rando_place_dzx_reset();
            File g = makeFile("Asoko", "Stage");
            uint8_t* g2 = &g.bytes[g.chunkOff["TRE2"] + k2->index * 0x20];
            pcr::dzx_key_write(*k2, g2, 0x01);
            patch(g);
            RT_CHECK_EQ(pcr::dzx_key_read(*k2, g2), 1u);
            RT_CHECK_EQ((int)pc_rando_place_dzx_status(keyIndex(*k2))->result, (int)PCR_PLACE_MISMATCH);
            RT_CHECK_EQ(pc_rando_place_dzx_status(keyIndex(*k2))->found, 0x01);
            // no TRE3 chunk: missing, the rest still written
            pc_rando_place_dzx_reset();
            File h = makeFile("Asoko", "Stage", "TRE3");
            patch(h);
            RT_CHECK_EQ((int)pc_rando_place_dzx_status(keyIndex(*k3))->result, (int)PCR_PLACE_MISSING);
            RT_CHECK_EQ((int)pc_rando_place_dzx_status(keyIndex(*k2))->result, (int)PCR_PLACE_WRITTEN);
            pc_rando_place_dzx_stats(&st);
            RT_CHECK_EQ(st.missing, 1u);
            RT_CHECK_EQ(st.written, 1u);
            // not a dzx at all: left as it is
            pc_rando_place_dzx_reset();
            File bad = makeFile("Asoko", "Stage");
            put32(&bad.bytes[0], 0x7FFFFFFF);
            const Bytes badBefore = bad.bytes;
            patch(bad);
            RT_CHECK(bad.bytes == badBefore);
            pc_rando_place_dzx_stats(&st);
            RT_CHECK_EQ(st.written, 0u);
        }
    }
    // A second load of the same file (a new archive mount) is patched again; status counts it.
    {
        pc_rando_place_dzx_reset();
        File f = makeFile("LinkUG", "Stage");
        patch(f);
        File g = makeFile("LinkUG", "Stage");
        patch(g);
        const LocKey* k = firstKey(PCR_LOC_OUTSET_ISLAND_UNDERNEATH_LINKS_HOUSE, "LinkUG", "TRES");
        RT_CHECK(k != nullptr && pc_rando_place_dzx_status(keyIndex(*k))->patches == 2);
        RT_CHECK(f.bytes == g.bytes);
    }
    RT_CHECK(pc_rando_place_dzx_status(-1) == nullptr);
    RT_CHECK(pc_rando_place_dzx_status((int)pcr::location_keys().size) == nullptr);

    pc_rando_deactivate();
    RT_CHECK_EQ(pc_rando(), 0);
    return rt::finish();
}
