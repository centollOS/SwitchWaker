// cos_rando_test_R2_4 (docs/RANDOMIZER_PLAN.md step R2.4): the event location patcher
// (features/rando/pc_rando_place_event.h) without the game, on synthetic event_list.dat files built
// from the key table (event names, actor slots, action ordinals, vanilla names and prm0 of R0.5's
// fingerprints; no disc bytes):
//   - the key set: 11 Event keys over 11 locations (10 EVENT, Orca EVENT+CODE), over the stages
//     Ojhous, Orichh, Opub, sea (5), Atorizk (2) and Siren;
//   - pc_rando() 0: nothing is written;
//   - pc_rando() 1 with a placement that puts songs into 011get_item / 046pget actions and items
//     into 059get_dance ones: every key's action is renamed (059get_dance for a song, else
//     011get_item) with prm0 = item - 0x6D / item, and nothing else in the file changes;
//   - the vanilla item is rewritten too (wwrando does): 046pget becomes 011get_item;
//   - D9: a changed prm0 or action name leaves that action vanilla (FAILED, mismatch) while the other
//     keys of the file are placed; a missing event fails with no-event;
//   - files that are not a stage's Stage event_list.dat (another archive, another file name, a
//     non-.dat type) are not touched;
//   - registration through pc_rando_place_event_init and pc_rando_res_dispatch.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_locs.h"
#include "pc_rando_event.h"
#include "pc_rando_lockeys.h"
#include "pc_rando_place_event.h"
#include "pc_rando_resources.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

void wr32(Bytes& b, size_t off, uint32_t v) {
    b[off] = (uint8_t)(v >> 24);
    b[off + 1] = (uint8_t)(v >> 16);
    b[off + 2] = (uint8_t)(v >> 8);
    b[off + 3] = (uint8_t)v;
}

uint32_t rd32(const Bytes& b, size_t off) {
    return ((uint32_t)b[off] << 24) | ((uint32_t)b[off + 1] << 16) | ((uint32_t)b[off + 2] << 8) | b[off + 3];
}

void wrName(Bytes& b, size_t off, const char* name) {
    memset(&b[off], 0, 32);
    memcpy(&b[off], name, strlen(name));
}

// A synthetic event list: per key of the stage one event with the key's name; its actor slots up to
// the key's slot hold actors; the key's actor has a chain of (ordinal + 2) actions, the key's one
// named and valued as on the disc (first property "prm0", an int), the others "WAIT" without
// properties; the other actors have one "WAIT" action. Plus one unrelated event first, so the
// indexes do not start at 0. keyAction[keyIndex] gets the global action index of each key's action.
struct Built {
    Bytes bytes;
    std::map<int, int> keyAction;
    std::map<int, int> keyInt;
};

Built makeEventList(const char* stage, const pcr::Span<pcr::LocKey>& keys) {
    struct Ev { std::string name; std::vector<int> actors; };
    struct Ac { std::string name; int first; };
    struct Act { std::string name; int prop; int next; };
    struct Prop { std::string name; int slot; };
    std::vector<Ev> evs;
    std::vector<Ac> acs;
    std::vector<Act> acts;
    std::vector<Prop> props;
    std::vector<int32_t> ints;
    Built out;
    auto addActor = [&](const char* name, int nActions, int keyOrdinal, const char* keyName, int32_t prm0) {
        const int first = (int)acts.size();
        for (int i = 0; i < nActions; i++) {
            Act a{"WAIT", -1, i + 1 < nActions ? first + i + 1 : -1};
            if (i == keyOrdinal) {
                a.name = keyName;
                a.prop = (int)props.size();
                props.push_back(Prop{"prm0", (int)ints.size()});
                ints.push_back(prm0);
            }
            acts.push_back(a);
        }
        acs.push_back(Ac{name, first});
        return (int)acs.size() - 1;
    };
    evs.push_back(Ev{"OTHER_EVENT", {addActor("Link", 1, -1, nullptr, 0)}});
    for (size_t i = 0; i < keys.size; i++) {
        const pcr::LocKey& k = keys[i];
        if (k.kind != pcr::KeyKind::Event || strcmp(k.stage, stage) != 0) {
            continue;
        }
        Ev e{k.event, {}};
        for (int slot = 0; slot <= (int)k.actor; slot++) {
            if (slot == (int)k.actor) {
                const int firstAction = (int)acts.size();
                const int firstInt = (int)ints.size();
                e.actors.push_back(addActor("Link", k.action + 2, k.action, k.action_name, k.prm0));
                out.keyAction[(int)i] = firstAction + k.action;
                out.keyInt[(int)i] = firstInt;
            } else {
                e.actors.push_back(addActor("CAMERA", 1, -1, nullptr, 0));
            }
        }
        evs.push_back(e);
    }
    const uint32_t evTop = 0x40;
    const uint32_t acTop = evTop + (uint32_t)evs.size() * 0xB0;
    const uint32_t actTop = acTop + (uint32_t)acs.size() * 0x50;
    const uint32_t propTop = actTop + (uint32_t)acts.size() * 0x50;
    const uint32_t intTop = propTop + (uint32_t)props.size() * 0x40;
    const uint32_t strTop = intTop + (uint32_t)ints.size() * 4;
    Bytes& b = out.bytes;
    b.assign(strTop + 0x10, 0);
    wr32(b, 0x00, evTop);
    wr32(b, 0x04, (uint32_t)evs.size());
    wr32(b, 0x08, acTop);
    wr32(b, 0x0C, (uint32_t)acs.size());
    wr32(b, 0x10, actTop);
    wr32(b, 0x14, (uint32_t)acts.size());
    wr32(b, 0x18, propTop);
    wr32(b, 0x1C, (uint32_t)props.size());
    wr32(b, 0x20, intTop); // no floats: an empty list at the int list's offset
    wr32(b, 0x24, 0);
    wr32(b, 0x28, intTop);
    wr32(b, 0x2C, (uint32_t)ints.size());
    wr32(b, 0x30, strTop);
    wr32(b, 0x34, 0x10);
    for (size_t i = 0; i < evs.size(); i++) {
        const size_t o = evTop + i * 0xB0;
        wrName(b, o, evs[i].name.c_str());
        wr32(b, o + 0x20, (uint32_t)i);
        for (int s = 0; s < PCR_EVL_EVENT_ACTORS; s++) {
            wr32(b, o + 0x2C + 4 * s, s < (int)evs[i].actors.size() ? (uint32_t)evs[i].actors[s] : 0xFFFFFFFFu);
        }
        wr32(b, o + 0x7C, (uint32_t)evs[i].actors.size());
        for (int f = 0; f < 5; f++) {
            wr32(b, o + 0x80 + 4 * f, 0xFFFFFFFFu);
        }
    }
    for (size_t i = 0; i < acs.size(); i++) {
        const size_t o = acTop + i * 0x50;
        wrName(b, o, acs[i].name.c_str());
        wr32(b, o + 0x24, (uint32_t)i);
        wr32(b, o + 0x28, (uint32_t)(1000 + i));
        wr32(b, o + 0x30, (uint32_t)acs[i].first);
    }
    for (size_t i = 0; i < acts.size(); i++) {
        const size_t o = actTop + i * 0x50;
        wrName(b, o, acts[i].name.c_str());
        wr32(b, o + 0x24, (uint32_t)i);
        for (int f = 0; f < 3; f++) {
            wr32(b, o + 0x28 + 4 * f, 0xFFFFFFFFu);
        }
        wr32(b, o + 0x34, (uint32_t)(2000 + i));
        wr32(b, o + 0x38, (uint32_t)acts[i].prop);
        wr32(b, o + 0x3C, (uint32_t)acts[i].next);
    }
    for (size_t i = 0; i < props.size(); i++) {
        const size_t o = propTop + i * 0x40;
        wrName(b, o, props[i].name.c_str());
        wr32(b, o + 0x20, (uint32_t)i);
        wr32(b, o + 0x24, PCR_EVL_TYPE_INT);
        wr32(b, o + 0x28, (uint32_t)props[i].slot);
        wr32(b, o + 0x2C, 1);
        wr32(b, o + 0x30, 0xFFFFFFFFu);
    }
    for (size_t i = 0; i < ints.size(); i++) {
        wr32(b, intTop + i * 4, (uint32_t)ints[i]);
    }
    return out;
}

PcRandoResFile fileFor(const char* stage, const char* arc, const char* name, uint32_t type, Bytes& b) {
    PcRandoResFile f = {};
    f.dir = "/res/Stage/x/";
    f.stage = stage;
    f.arc = arc;
    f.room = -1;
    f.name = name;
    f.type = type;
    f.data = b.data();
    f.size = b.size();
    return f;
}

uint8_t sItems[PCR_LOC_COUNT];

// The test placement of the 11 event locations: songs into 011get_item / 046pget actions, items into
// 059get_dance ones, and two locations keep their vanilla item.
struct Place {
    int loc;
    uint8_t item;
};
const Place kPlace[] = {
    {PCR_LOC_OUTSET_ISLAND_ORCA_GIVE_10_KNIGHTS_CRESTS, 0x6D},             // 046pget <- Wind's Requiem
    {PCR_LOC_WINDFALL_ISLAND_MAGGIES_FATHER_GIVE_20_SKULL_NECKLACES, 0x72}, // 011 <- Song of Passing
    {PCR_LOC_WINDFALL_ISLAND_CAFE_BAR_POSTMAN, 0x9B},                      // vanilla Moblin's Letter
    {PCR_LOC_WINDFALL_ISLAND_TOTT_TEACH_RHYTHM, 0xAA},                     // 059 <- Hurricane Spin
    {PCR_LOC_WINDFALL_ISLAND_MILA_FOLLOW_THE_THIEF, 0x6E},                 // 011 <- Ballad of Gales
    {PCR_LOC_DRAGON_ROOST_ISLAND_WIND_SHRINE, 0xEE},                       // 059 <- Treasure Chart 2
    {PCR_LOC_DRAGON_ROOST_ISLAND_RITO_AERIE_GIVE_HOSKIT_20_GOLDEN_FEATHERS, 0x07}, // Piece of Heart
    {PCR_LOC_DRAGON_ROOST_ISLAND_RITO_AERIE_MAIL_SORTING, 0x99},           // vanilla (011_get_item renamed)
    {PCR_LOC_TOWER_OF_THE_GODS_STONE_TABLET, 0xB2},                        // 059 <- Magic Meter Upgrade
    {PCR_LOC_THE_GREAT_SEA_CYCLOS, 0x50},                                  // 059 <- Empty Bottle
    {PCR_LOC_TWO_EYE_REEF_BIG_OCTO_GREAT_FAIRY, 0xB2},                     // vanilla 046pget renamed
};

bool isSong(uint8_t item) {
    return item >= 0x6D && item <= 0x72;
}

// The action's name and prm0 in a built file.
void actionOf(const Bytes& b, int action, char name[33], int32_t* prm0) {
    PcRandoEvl evl;
    Bytes copy = b;
    *prm0 = -1;
    name[0] = '\0';
    if (pc_rando_evl_open(&evl, copy.data(), copy.size()) != PCR_EVL_OK) {
        return;
    }
    pc_rando_evl_name(pc_rando_evl_action_p(&evl, action), name);
    const int prop = pc_rando_evl_action_property_at(&evl, action, 0);
    pc_rando_evl_get_int(&evl, prop, 0, prm0);
}

// The bytes that differ between two files, as offsets.
std::vector<size_t> diff(const Bytes& a, const Bytes& b) {
    std::vector<size_t> d;
    for (size_t i = 0; i < a.size() && i < b.size(); i++) {
        if (a[i] != b[i]) {
            d.push_back(i);
        }
    }
    return d;
}

} // namespace

int main() {
    const pcr::Span<pcr::LocKey> keys = pcr::location_keys();

    // --- the key set
    std::set<int> locs;
    std::map<std::string, int> perStage;
    int nKeys = 0;
    for (const pcr::LocKey& k : keys) {
        if (k.kind != pcr::KeyKind::Event) {
            continue;
        }
        nKeys++;
        locs.insert(k.loc);
        perStage[k.stage]++;
        RT_CHECK(strcmp(k.arc, "Stage") == 0);
        RT_CHECK((k.flags & pcr::KEY_FP_ITEM) != 0);
        const bool song = strcmp(k.action_name, "059get_dance") == 0;
        RT_CHECK_EQ(k.prm0, song ? (int32_t)k.vanilla - 0x6D : (int32_t)k.vanilla);
        const pcr::LocInfo* info = pcr::location_info(k.loc);
        RT_CHECK(info != nullptr);
        if (info != nullptr) {
            RT_CHECK_EQ((int)info->classes, k.loc == PCR_LOC_OUTSET_ISLAND_ORCA_GIVE_10_KNIGHTS_CRESTS
                                                ? (int)(pcr::PCR_CLASS_EVENT | pcr::PCR_CLASS_CODE)
                                                : (int)pcr::PCR_CLASS_EVENT);
        }
    }
    RT_CHECK_EQ(nKeys, 11);
    RT_CHECK_EQ(pc_rando_place_event_key_count(), 11);
    RT_CHECK_EQ((int)locs.size(), 11);
    RT_CHECK_EQ(perStage["Ojhous"], 1);
    RT_CHECK_EQ(perStage["Orichh"], 1);
    RT_CHECK_EQ(perStage["Opub"], 1);
    RT_CHECK_EQ(perStage["sea"], 5);
    RT_CHECK_EQ(perStage["Atorizk"], 2);
    RT_CHECK_EQ(perStage["Siren"], 1);
    for (const Place& p : kPlace) {
        RT_CHECK(locs.count(p.loc) == 1);
    }

    for (int l = 0; l < PCR_LOC_COUNT; l++) {
        const pcr::Span<pcr::LocKey> ks = pcr::location_keys(l);
        sItems[l] = ks.size > 0 ? ks[0].vanilla : 0;
    }
    for (const Place& p : kPlace) {
        sItems[p.loc] = p.item;
    }

    // --- pc_rando() 0: nothing is written
    pc_rando_place_event_reset();
    {
        Built built = makeEventList("sea", keys);
        const Bytes before = built.bytes;
        PcRandoResFile f = fileFor("sea", "Stage", "event_list.dat", PCR_RES_DAT, built.bytes);
        pc_rando_place_event_patch(&f);
        RT_CHECK(built.bytes == before);
        for (const auto& ka : built.keyAction) {
            RT_CHECK_EQ(pc_rando_place_event_result(ka.first), PCR_EVLOC_NOT_SEEN);
        }
    }

    pc_rando_set_loc_items(sItems, PCR_LOC_COUNT);
    pc_rando_set_active(nullptr, nullptr);
    RT_CHECK_EQ(pc_rando(), 1);

    // --- every stage: every key placed; only its action name and its prm0 slot change
    int nPlaced = 0;
    for (const auto& st : perStage) {
        pc_rando_place_event_reset();
        Built built = makeEventList(st.first.c_str(), keys);
        const Bytes before = built.bytes;
        PcRandoResFile f = fileFor(st.first.c_str(), "Stage", "event_list.dat", PCR_RES_DAT, built.bytes);
        pc_rando_place_event_patch(&f);
        std::set<size_t> allowed;
        PcRandoEvl evl;
        Bytes scratch = before;
        RT_CHECK_EQ(pc_rando_evl_open(&evl, scratch.data(), scratch.size()), (int)PCR_EVL_OK);
        for (const auto& ka : built.keyAction) {
            const pcr::LocKey& k = keys[(size_t)ka.first];
            const uint8_t item = sItems[k.loc];
            RT_CHECK_EQ(pc_rando_place_event_result(ka.first), PCR_EVLOC_PLACED);
            RT_CHECK_EQ((int)pc_rando_place_event_item(ka.first), (int)item);
            RT_CHECK_EQ(pc_rando_place_event_action(ka.first), ka.second);
            char name[33];
            int32_t prm0;
            actionOf(built.bytes, ka.second, name, &prm0);
            RT_CHECK_EQ(std::string(name), std::string(isSong(item) ? "059get_dance" : "011get_item"));
            RT_CHECK_EQ(prm0, isSong(item) ? (int32_t)item - 0x6D : (int32_t)item);
            nPlaced++;
            const size_t actionOff = evl.actionTop + (size_t)ka.second * 0x50;
            for (size_t i = 0; i < 32; i++) {
                allowed.insert(actionOff + i);
            }
            const size_t intOff = evl.intTop + (size_t)built.keyInt[ka.first] * 4;
            for (size_t i = 0; i < 4; i++) {
                allowed.insert(intOff + i);
            }
        }
        for (size_t off : diff(before, built.bytes)) {
            if (!RT_CHECK(allowed.count(off) == 1)) {
                fprintf(stderr, "  %s: byte 0x%zx changed outside a key's action name / prm0\n", st.first.c_str(), off);
                break;
            }
        }
        RT_CHECK(before.size() == built.bytes.size());
    }
    RT_CHECK_EQ(nPlaced, 11);

    // --- the vanilla item is rewritten too: the Big Octo fairy's 046pget becomes 011get_item 0xB2
    {
        pc_rando_place_event_reset();
        Built built = makeEventList("sea", keys);
        PcRandoResFile f = fileFor("sea", "Stage", "event_list.dat", PCR_RES_DAT, built.bytes);
        pc_rando_place_event_patch(&f);
        for (const auto& ka : built.keyAction) {
            const pcr::LocKey& k = keys[(size_t)ka.first];
            if (k.loc == PCR_LOC_TWO_EYE_REEF_BIG_OCTO_GREAT_FAIRY) {
                RT_CHECK_EQ(std::string(k.action_name), std::string("046pget"));
                char name[33];
                int32_t prm0;
                actionOf(built.bytes, ka.second, name, &prm0);
                RT_CHECK_EQ(std::string(name), std::string("011get_item"));
                RT_CHECK_EQ(prm0, 0xB2);
            }
        }
    }

    // --- D9: a changed prm0 or name leaves that action vanilla, the others are still placed
    for (int variant = 0; variant < 2; variant++) {
        pc_rando_place_event_reset();
        Built built = makeEventList("sea", keys);
        PcRandoEvl evl;
        Bytes scratch = built.bytes;
        RT_CHECK_EQ(pc_rando_evl_open(&evl, scratch.data(), scratch.size()), (int)PCR_EVL_OK);
        int victim = -1;
        for (const auto& ka : built.keyAction) {
            if (keys[(size_t)ka.first].loc == PCR_LOC_WINDFALL_ISLAND_TOTT_TEACH_RHYTHM) {
                victim = ka.first;
            }
        }
        RT_CHECK(victim >= 0);
        if (victim < 0) {
            break;
        }
        if (variant == 0) {
            wr32(built.bytes, evl.intTop + (size_t)built.keyInt[victim] * 4, 3); // a replaced song
        } else {
            wrName(built.bytes, evl.actionTop + (size_t)built.keyAction[victim] * 0x50, "011get_item");
        }
        const Bytes before = built.bytes;
        PcRandoResFile f = fileFor("sea", "Stage", "event_list.dat", PCR_RES_DAT, built.bytes);
        pc_rando_place_event_patch(&f);
        RT_CHECK_EQ(pc_rando_place_event_result(victim), PCR_EVLOC_FAILED);
        RT_CHECK_EQ(pc_rando_place_event_error(victim), (int)PCR_EVL_MISMATCH);
        RT_CHECK_EQ((int)pc_rando_place_event_item(victim), 0xFF);
        char name[33], nameBefore[33];
        int32_t prm0, prm0Before;
        actionOf(built.bytes, built.keyAction[victim], name, &prm0);
        actionOf(before, built.keyAction[victim], nameBefore, &prm0Before);
        RT_CHECK_EQ(std::string(name), std::string(nameBefore));
        RT_CHECK_EQ(prm0, prm0Before);
        for (const auto& ka : built.keyAction) {
            if (ka.first != victim) {
                RT_CHECK_EQ(pc_rando_place_event_result(ka.first), PCR_EVLOC_PLACED);
            }
        }
    }

    // --- a missing event fails with no-event (another stage's list under this stage's name)
    {
        pc_rando_place_event_reset();
        Built built = makeEventList("Orichh", keys);
        const Bytes before = built.bytes;
        PcRandoResFile f = fileFor("Opub", "Stage", "event_list.dat", PCR_RES_DAT, built.bytes);
        pc_rando_place_event_patch(&f);
        RT_CHECK(built.bytes == before);
        for (size_t i = 0; i < keys.size; i++) {
            if (keys[i].kind == pcr::KeyKind::Event && strcmp(keys[i].stage, "Opub") == 0) {
                RT_CHECK_EQ(pc_rando_place_event_result((int)i), PCR_EVLOC_FAILED);
                RT_CHECK_EQ(pc_rando_place_event_error((int)i), (int)PCR_EVL_NO_EVENT);
            }
        }
    }

    // --- other files are not touched
    {
        struct Other {
            const char* arc;
            const char* name;
            uint32_t type;
        };
        const Other others[] = {
            {"Room0", "event_list.dat", PCR_RES_DAT},
            {"Stage", "other.dat", PCR_RES_DAT},
            {"Stage", "event_list.dat", PCR_RES_DZS},
        };
        for (const Other& o : others) {
            pc_rando_place_event_reset();
            Built built = makeEventList("Orichh", keys);
            const Bytes before = built.bytes;
            PcRandoResFile f = fileFor("Orichh", o.arc, o.name, o.type, built.bytes);
            pc_rando_place_event_patch(&f);
            RT_CHECK(built.bytes == before);
            for (const auto& ka : built.keyAction) {
                RT_CHECK_EQ(pc_rando_place_event_result(ka.first), PCR_EVLOC_NOT_SEEN);
            }
        }
    }

    // --- registration and dispatch
    {
        const int before = pc_rando_res_patcher_count();
        pc_rando_place_event_init();
        pc_rando_place_event_init();
        RT_CHECK_EQ(pc_rando_res_patcher_count(), before + 1);
        pc_rando_place_event_reset();
        Built built = makeEventList("Atorizk", keys);
        PcRandoResFile f = fileFor("Atorizk", "Stage", "event_list.dat", PCR_RES_DAT, built.bytes);
        RT_CHECK(pc_rando_res_dispatch(&f) == built.bytes.data());
        for (const auto& ka : built.keyAction) {
            RT_CHECK_EQ(pc_rando_place_event_result(ka.first), PCR_EVLOC_PLACED);
        }
        RT_CHECK_EQ((int)built.keyAction.size(), 2);
        RT_CHECK_EQ(pc_rando_unregister_res_patcher(pc_rando_place_event_patch), 1);
    }

    pc_rando_deactivate();
    return rt::finish();
}
