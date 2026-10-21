// cos_rando_test_R0_11 (docs/RANDOMIZER_PLAN.md step R0.11): the event_list.dat in-place patch
// library (features/rando/pc_rando_event.h) on a synthetic file (no disc bytes):
//   - the fixture R0_11_wwrando_events.txt (fixtures/R0_11_make_fixture.py): a file built by wwrando's
//     own wwlib/events.py; this library's walk of it gives wwlib's dump ("before"), and the same edits
//     tweaks.py / items.py make (change_event_item for an item and a song, truncate, remove, reorder,
//     keep the last action, event and action flags, a rename, an int list shrink, float and vec3
//     values), done here in place, give wwrando's dump after its own edits and save ("after");
//   - lookups (events, actor slots, actions by ordinal and name, properties, values);
//   - refusals that write nothing: bad and swapped files, fingerprint mismatches, bad types and
//     ranges, list slots shared by two properties, broken chains.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc_rando_event.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

void wr32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

bool unhex(const std::string& s, Bytes& out) {
    if (s.size() % 2) {
        return false;
    }
    out.clear();
    for (size_t i = 0; i < s.size(); i += 2) {
        out.push_back((uint8_t)strtoul(s.substr(i, 2).c_str(), nullptr, 16));
    }
    return true;
}

std::string fhex(float f) {
    uint32_t b;
    memcpy(&b, &f, 4);
    char buf[16];
    snprintf(buf, sizeof buf, "%08x", b);
    return buf;
}

// The fixture dump format (R0_11_make_fixture.py dump()), walked through the library.
std::vector<std::string> dump(const PcRandoEvl& evl) {
    std::vector<std::string> out;
    char line[256];
    char name[PCR_EVL_NAME_MAX + 1];
    for (uint32_t e = 0; e < evl.eventNum; e++) {
        const uint8_t* ev = pc_rando_evl_event_p(&evl, (int)e);
        pc_rando_evl_name(ev, name);
        snprintf(line, sizeof line, "event %u %s prio %d start %d %d end %d %d %d jingle %d", e, name,
                 (int)rd32(ev + 0x28), (int)rd32(ev + 0x80), (int)rd32(ev + 0x84), (int)rd32(ev + 0x88),
                 (int)rd32(ev + 0x8C), (int)rd32(ev + 0x90), ev[0x94]);
        out.push_back(line);
        for (int slot = 0;; slot++) {
            const int actor = pc_rando_evl_event_actor(&evl, (int)e, slot);
            if (actor < 0) {
                break;
            }
            const uint8_t* ac = pc_rando_evl_actor_p(&evl, actor);
            pc_rando_evl_name(ac, name);
            snprintf(line, sizeof line, " actor %s id %d type %d flag %d", name, (int)rd32(ac + 0x20),
                     (int)rd32(ac + 0x2C), (int)pc_rando_evl_actor_flag(&evl, actor));
            out.push_back(line);
            const int n = pc_rando_evl_action_count(&evl, actor);
            for (int i = 0; i < n; i++) {
                const int action = pc_rando_evl_actor_action(&evl, actor, i);
                const uint8_t* act = pc_rando_evl_action_p(&evl, action);
                pc_rando_evl_name(act, name);
                snprintf(line, sizeof line, "  action %s dup %d start %d %d %d flag %d", name, (int)rd32(act + 0x20),
                         (int)rd32(act + 0x28), (int)rd32(act + 0x2C), (int)rd32(act + 0x30),
                         (int)pc_rando_evl_action_flag(&evl, action));
                out.push_back(line);
                for (int k = 0;; k++) {
                    const int prop = pc_rando_evl_action_property_at(&evl, action, k);
                    if (prop < 0) {
                        break;
                    }
                    pc_rando_evl_name(pc_rando_evl_property_p(&evl, prop), name);
                    std::string s = std::string("   prop ") + name;
                    const int type = pc_rando_evl_property_type(&evl, prop);
                    const int size = pc_rando_evl_property_size(&evl, prop);
                    if (type == PCR_EVL_TYPE_STRING) {
                        const char* str = pc_rando_evl_get_string(&evl, prop);
                        s += std::string(" str ") + (str ? str : "(bad)");
                    } else if (type == PCR_EVL_TYPE_INT) {
                        s += " int";
                        for (int j = 0; j < size; j++) {
                            int32_t v = 0;
                            pc_rando_evl_get_int(&evl, prop, j, &v);
                            s += " " + std::to_string(v);
                        }
                    } else {
                        s += type == PCR_EVL_TYPE_VEC ? " vec" : " float";
                        const int nf = type == PCR_EVL_TYPE_VEC ? size * 3 : size;
                        for (int j = 0; j < nf; j++) {
                            float v = 0;
                            pc_rando_evl_get_float(&evl, prop, j, &v);
                            s += " " + fhex(v);
                        }
                    }
                    out.push_back(s);
                }
            }
        }
    }
    return out;
}

struct Fixture {
    Bytes src;
    std::vector<std::string> before, after;
};

bool loadFixture(Fixture& f) {
    std::string text;
    if (!rt::readFixture("R0_11_wwrando_events.txt", text)) {
        return false;
    }
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("src ", 0) == 0) {
            unhex(line.substr(4), f.src);
        } else if (line.rfind("before ", 0) == 0) {
            f.before.push_back(line.substr(7));
        } else if (line.rfind("after ", 0) == 0) {
            f.after.push_back(line.substr(6));
        }
    }
    return !f.src.empty() && !f.before.empty() && !f.after.empty();
}

void checkDump(const std::vector<std::string>& got, const std::vector<std::string>& want, const char* what) {
    RT_CHECK_EQ(got.size(), want.size());
    for (size_t i = 0; i < got.size() && i < want.size(); i++) {
        if (got[i] != want[i]) {
            fprintf(stderr, "%s line %zu:\n  got  %s\n  want %s\n", what, i, got[i].c_str(), want[i].c_str());
            RT_CHECK(false);
            return;
        }
    }
}

int actorOf(const PcRandoEvl& evl, const char* event, const char* actor) {
    const int e = pc_rando_evl_find_event(&evl, event);
    return pc_rando_evl_event_actor(&evl, e, pc_rando_evl_find_actor(&evl, e, actor));
}

// R0_11_make_fixture.py edit(), in the same order.
void applyEdits(PcRandoEvl& evl) {
    const int gp1 = pc_rando_evl_find_event(&evl, "Gp1_Get_Itm");
    const int tact = pc_rando_evl_find_event(&evl, "TACT_HT");
    const int link = actorOf(evl, "Gp1_Get_Itm", "Link");
    const int cam = actorOf(evl, "Gp1_Get_Itm", "CAMERA");
    const int hr = actorOf(evl, "Gp1_Get_Itm", "Hr");
    const int os0 = actorOf(evl, "TACT_HT", "Os0");
    const int os1 = actorOf(evl, "TACT_HT", "Os1");
    const int tk = actorOf(evl, "TACT_HT", "TIMEKEEPER");
    RT_CHECK(gp1 == 0 && tact == 1 && link >= 0 && cam >= 0 && hr >= 0 && os0 >= 0 && os1 >= 0 && tk >= 0);

    int action = -1, slot = -1;
    RT_CHECK_EQ(pc_rando_evl_change_item(&evl, "Gp1_Get_Itm", 0, 1, 0x07, "011get_item", 238, &action, &slot),
                (int)PCR_EVL_OK);
    RT_CHECK_EQ(action, pc_rando_evl_actor_action(&evl, link, 1));
    RT_CHECK_EQ(slot, pc_rando_evl_property_slot(&evl, pc_rando_evl_action_property(&evl, action, "prm0")));
    RT_CHECK_EQ(pc_rando_evl_change_item(&evl, "TACT_HT", 0, 1, 0x6F, "059get_dance", 0, nullptr, nullptr),
                (int)PCR_EVL_OK);
    RT_CHECK_EQ(pc_rando_evl_truncate_actions(&evl, hr, 7), (int)PCR_EVL_OK);
    RT_CHECK_EQ(pc_rando_evl_remove_action(&evl, cam, 2), (int)PCR_EVL_OK);
    // a = os0.actions[3]; os0.actions.remove(a); os0.actions.insert(7, a)
    const int order[10] = {0, 1, 2, 4, 5, 6, 7, 3, 8, 9};
    RT_CHECK_EQ(pc_rando_evl_set_chain(&evl, os0, order, 10), (int)PCR_EVL_OK);
    RT_CHECK_EQ(pc_rando_evl_keep_last_actions(&evl, os1, 1), (int)PCR_EVL_OK);
    const int lastOf[3] = {link, cam, hr};
    for (int i = 0; i < 3; i++) {
        const int last = pc_rando_evl_actor_action(&evl, lastOf[i], pc_rando_evl_action_count(&evl, lastOf[i]) - 1);
        RT_CHECK_EQ(pc_rando_evl_set_ending_flag(&evl, gp1, i, pc_rando_evl_action_flag(&evl, last)), (int)PCR_EVL_OK);
    }
    RT_CHECK_EQ(pc_rando_evl_set_action_starting_flag(&evl, pc_rando_evl_actor_action(&evl, cam, 1), 0,
                                                      pc_rando_evl_action_flag(&evl, pc_rando_evl_actor_action(&evl, hr, 2))),
                (int)PCR_EVL_OK);
    RT_CHECK_EQ(pc_rando_evl_set_starting_flag(&evl, tact, 1, 77), (int)PCR_EVL_OK);
    const int timer = pc_rando_evl_action_property(&evl, pc_rando_evl_actor_action(&evl, tk, 0), "Timer");
    const int32_t nine = 9;
    const int32_t oldTimer[1] = {1};
    RT_CHECK_EQ(pc_rando_evl_set_ints(&evl, timer, &nine, 1, oldTimer), (int)PCR_EVL_OK);
    const int unitrans = pc_rando_evl_actor_action(&evl, cam, 0);
    const float sixty = 60.0f, two = 2.0f;
    RT_CHECK_EQ(pc_rando_evl_set_float(&evl, pc_rando_evl_action_property_at(&evl, unitrans, 1), 0, 45.5f, &sixty),
                (int)PCR_EVL_OK);
    RT_CHECK_EQ(pc_rando_evl_set_float(&evl, pc_rando_evl_action_property(&evl, unitrans, "Eye"), 1, -7.25f, &two),
                (int)PCR_EVL_OK);
    RT_CHECK_EQ(pc_rando_evl_rename_action(&evl, pc_rando_evl_actor_action(&evl, link, 0), "002n_wait2", "001n_wait"),
                (int)PCR_EVL_OK);
}

void testFixture(const Fixture& f) {
    Bytes b = f.src;
    PcRandoEvl evl;
    RT_CHECK_EQ(pc_rando_evl_open(&evl, b.data(), b.size()), (int)PCR_EVL_OK);
    RT_CHECK_EQ(evl.eventNum, 3u);
    checkDump(dump(evl), f.before, "before");
    applyEdits(evl);
    checkDump(dump(evl), f.after, "after");
    // In place: same size, same header, the chunk lists did not move.
    RT_CHECK_EQ(b.size(), f.src.size());
    RT_CHECK(memcmp(b.data(), f.src.data(), 0x40) == 0);
    // Nothing in the event/actor/action/property records changed outside the fields the edits own:
    // name (0x00-0x1F), event flags, actor initial action, action starting flags / next, property size.
    // Spot-check: the string list is untouched.
    RT_CHECK(memcmp(b.data() + evl.strTop, f.src.data() + evl.strTop, evl.strSize) == 0);
}

void testLookups(const Fixture& f) {
    Bytes b = f.src;
    PcRandoEvl evl;
    RT_CHECK_EQ(pc_rando_evl_open(&evl, b.data(), b.size()), (int)PCR_EVL_OK);
    RT_CHECK_EQ(pc_rando_evl_find_event(&evl, "TACT_HT"), 1);
    RT_CHECK_EQ(pc_rando_evl_find_event(&evl, "TACT"), -1);
    RT_CHECK_EQ(pc_rando_evl_find_event(&evl, "nope"), -1);
    RT_CHECK_EQ(pc_rando_evl_find_actor(&evl, 1, "TIMEKEEPER"), 3);
    RT_CHECK_EQ(pc_rando_evl_find_actor(&evl, 1, "Hr"), -1);
    RT_CHECK_EQ(pc_rando_evl_event_actor(&evl, 2, 1), -1); // blank slot
    RT_CHECK_EQ(pc_rando_evl_event_actor(&evl, 2, 20), -1);
    RT_CHECK_EQ(pc_rando_evl_event_actor(&evl, 3, 0), -1);
    const int hr = actorOf(evl, "Gp1_Get_Itm", "Hr");
    RT_CHECK_EQ(pc_rando_evl_action_count(&evl, hr), 9);
    RT_CHECK_EQ(pc_rando_evl_actor_action(&evl, hr, 9), -1);
    RT_CHECK_EQ(pc_rando_evl_find_action(&evl, hr, "a5", 0), 5);
    RT_CHECK_EQ(pc_rando_evl_find_action(&evl, hr, "a5", 1), -1);
    const int link = actorOf(evl, "Gp1_Get_Itm", "Link");
    const int get = pc_rando_evl_actor_action(&evl, link, 1);
    const int prm0 = pc_rando_evl_action_property(&evl, get, "prm0");
    RT_CHECK(prm0 >= 0);
    RT_CHECK_EQ(pc_rando_evl_action_property(&evl, get, "prm1"), -1);
    RT_CHECK_EQ(pc_rando_evl_action_property_at(&evl, get, 1), -1);
    RT_CHECK_EQ(pc_rando_evl_action_property_at(&evl, pc_rando_evl_actor_action(&evl, link, 2), 0), -1); // FINISH
    int32_t v = 0;
    RT_CHECK_EQ(pc_rando_evl_get_int(&evl, prm0, 0, &v), (int)PCR_EVL_OK);
    RT_CHECK_EQ(v, 238);
    RT_CHECK_EQ(pc_rando_evl_get_int(&evl, prm0, 1, &v), (int)PCR_EVL_BAD_RANGE);
    float fl = 0;
    RT_CHECK_EQ(pc_rando_evl_get_float(&evl, prm0, 0, &fl), (int)PCR_EVL_BAD_TYPE);
    const int os0 = actorOf(evl, "TACT_HT", "Os0");
    const int msg = pc_rando_evl_action_property(&evl, pc_rando_evl_actor_action(&evl, os0, 4), "msg");
    RT_CHECK(pc_rando_evl_get_string(&evl, msg) != nullptr && strcmp(pc_rando_evl_get_string(&evl, msg), "hello world") == 0);
    RT_CHECK(pc_rando_evl_get_string(&evl, prm0) == nullptr);
    RT_CHECK(pc_rando_evl_event_p(&evl, -1) == nullptr && pc_rando_evl_actor_p(&evl, (int)evl.actorNum) == nullptr);
    RT_CHECK_EQ(pc_rando_evl_actor_flag(&evl, -1), -1);
    for (int r = PCR_EVL_OK; r <= PCR_EVL_LOOP; r++) {
        RT_CHECK(strcmp(pc_rando_evl_result_name(r), "?") != 0);
    }
    RT_CHECK(memcmp(b.data(), f.src.data(), b.size()) == 0); // lookups write nothing
}

// Runs op on a fresh copy and checks it returns `want` and writes nothing.
template <class Op> void refused(const Bytes& src, int want, const char* what, Op op) {
    Bytes b = src;
    PcRandoEvl evl;
    if (!RT_CHECK_EQ(pc_rando_evl_open(&evl, b.data(), b.size()), (int)PCR_EVL_OK)) {
        return;
    }
    const int got = op(evl);
    if (got != want) {
        fprintf(stderr, "%s: got %s, want %s\n", what, pc_rando_evl_result_name(got), pc_rando_evl_result_name(want));
    }
    RT_CHECK_EQ(got, want);
    if (memcmp(b.data(), src.data(), b.size()) != 0) {
        fprintf(stderr, "%s: wrote bytes\n", what);
        RT_CHECK(false);
    }
}

void testRefusals(const Fixture& f) {
    PcRandoEvl evl;
    Bytes b = f.src;
    RT_CHECK_EQ(pc_rando_evl_open(&evl, b.data(), 0x3F), (int)PCR_EVL_BAD_FILE);
    RT_CHECK_EQ(pc_rando_evl_open(&evl, nullptr, 0x100), (int)PCR_EVL_BAD_FILE);
    RT_CHECK_EQ(pc_rando_evl_open(&evl, b.data(), b.size() - 4), (int)PCR_EVL_BAD_FILE);
    Bytes big = b;
    wr32(big.data() + 0x2C, 0x40000000u); // int count way past the buffer
    RT_CHECK_EQ(pc_rando_evl_open(&evl, big.data(), big.size()), (int)PCR_EVL_BAD_FILE);
    Bytes swapped = b;
    swapped[0x38] = 1; // dEvent_manager_c::setData already swapped the lists
    RT_CHECK_EQ(pc_rando_evl_open(&evl, swapped.data(), swapped.size()), (int)PCR_EVL_SWAPPED);

    const Bytes& s = f.src;
    refused(s, PCR_EVL_MISMATCH, "change_item wrong prm0", [](PcRandoEvl& e) {
        return pc_rando_evl_change_item(&e, "Gp1_Get_Itm", 0, 1, 0x07, "011get_item", 237, nullptr, nullptr);
    });
    refused(s, PCR_EVL_MISMATCH, "change_item wrong name", [](PcRandoEvl& e) {
        return pc_rando_evl_change_item(&e, "Gp1_Get_Itm", 0, 1, 0x07, "059get_dance", 238, nullptr, nullptr);
    });
    refused(s, PCR_EVL_NO_EVENT, "change_item no event", [](PcRandoEvl& e) {
        return pc_rando_evl_change_item(&e, "Gp2_Get_Itm", 0, 1, 0x07, nullptr, 0, nullptr, nullptr);
    });
    refused(s, PCR_EVL_NO_ACTOR, "change_item no actor", [](PcRandoEvl& e) {
        return pc_rando_evl_change_item(&e, "Gp1_Get_Itm", 3, 1, 0x07, nullptr, 0, nullptr, nullptr);
    });
    refused(s, PCR_EVL_NO_ACTION, "change_item no action", [](PcRandoEvl& e) {
        return pc_rando_evl_change_item(&e, "Gp1_Get_Itm", 0, 3, 0x07, nullptr, 0, nullptr, nullptr);
    });
    refused(s, PCR_EVL_NO_PROPERTY, "change_item FINISH has no property", [](PcRandoEvl& e) {
        return pc_rando_evl_change_item(&e, "Gp1_Get_Itm", 0, 2, 0x07, nullptr, 0, nullptr, nullptr);
    });
    refused(s, PCR_EVL_BAD_TYPE, "change_item on a float property", [](PcRandoEvl& e) {
        return pc_rando_evl_change_item(&e, "Gp1_Get_Itm", 1, 0, 0x07, nullptr, 0, nullptr, nullptr);
    });
    refused(s, PCR_EVL_MISMATCH, "rename fingerprint", [](PcRandoEvl& e) {
        return pc_rando_evl_rename_action(&e, pc_rando_evl_actor_action(&e, actorOf(e, "Gp1_Get_Itm", "Link"), 0), "x",
                                          "001n_wai");
    });
    refused(s, PCR_EVL_BAD_RANGE, "rename to 32 chars", [](PcRandoEvl& e) {
        return pc_rando_evl_rename_action(&e, 0, "0123456789abcdef0123456789abcdef", nullptr);
    });
    refused(s, PCR_EVL_NO_ACTION, "rename bad index", [](PcRandoEvl& e) {
        return pc_rando_evl_rename_action(&e, (int)e.actionNum, "x", nullptr);
    });
    refused(s, PCR_EVL_BAD_RANGE, "set_ints grows", [](PcRandoEvl& e) {
        const int32_t v[2] = {1, 2};
        const int p = pc_rando_evl_action_property(&e, pc_rando_evl_actor_action(&e, actorOf(e, "Gp1_Get_Itm", "Hr"), 0),
                                                   "Timer");
        return pc_rando_evl_set_ints(&e, p, v, 2, nullptr);
    });
    refused(s, PCR_EVL_BAD_RANGE, "set_ints empty", [](PcRandoEvl& e) {
        const int32_t v[1] = {1};
        const int p = pc_rando_evl_action_property(&e, pc_rando_evl_actor_action(&e, actorOf(e, "TACT_HT", "TIMEKEEPER"), 0),
                                                   "Timer");
        return pc_rando_evl_set_ints(&e, p, v, 0, nullptr);
    });
    refused(s, PCR_EVL_MISMATCH, "set_ints fingerprint", [](PcRandoEvl& e) {
        const int32_t v[2] = {7, 8}, old[2] = {1, 3};
        const int p = pc_rando_evl_action_property(&e, pc_rando_evl_actor_action(&e, actorOf(e, "TACT_HT", "TIMEKEEPER"), 0),
                                                   "Timer");
        return pc_rando_evl_set_ints(&e, p, v, 2, old);
    });
    refused(s, PCR_EVL_BAD_TYPE, "set_int on a string", [](PcRandoEvl& e) {
        const int p = pc_rando_evl_action_property(&e, pc_rando_evl_actor_action(&e, actorOf(e, "TACT_HT", "Os0"), 4), "msg");
        return pc_rando_evl_set_int(&e, p, 0, 1, nullptr);
    });
    refused(s, PCR_EVL_BAD_RANGE, "set_float past a vec3", [](PcRandoEvl& e) {
        const int p = pc_rando_evl_action_property(&e, pc_rando_evl_actor_action(&e, actorOf(e, "Gp1_Get_Itm", "CAMERA"), 0),
                                                   "Eye");
        return pc_rando_evl_set_float(&e, p, 3, 1.0f, nullptr);
    });
    refused(s, PCR_EVL_BAD_RANGE, "set_chain repeats an action", [](PcRandoEvl& e) {
        const int order[2] = {1, 1};
        return pc_rando_evl_set_chain(&e, actorOf(e, "Gp1_Get_Itm", "Link"), order, 2);
    });
    refused(s, PCR_EVL_NO_ACTION, "set_chain ordinal out of the chain", [](PcRandoEvl& e) {
        const int order[2] = {0, 3};
        return pc_rando_evl_set_chain(&e, actorOf(e, "Gp1_Get_Itm", "Link"), order, 2);
    });
    refused(s, PCR_EVL_BAD_RANGE, "truncate to 0", [](PcRandoEvl& e) {
        return pc_rando_evl_truncate_actions(&e, actorOf(e, "Gp1_Get_Itm", "Link"), 0);
    });
    refused(s, PCR_EVL_BAD_RANGE, "truncate past the chain", [](PcRandoEvl& e) {
        return pc_rando_evl_truncate_actions(&e, actorOf(e, "Gp1_Get_Itm", "Link"), 4);
    });
    refused(s, PCR_EVL_BAD_RANGE, "remove the only action", [](PcRandoEvl& e) {
        return pc_rando_evl_remove_action(&e, actorOf(e, "EmptyEvt", "DIRECTOR"), 0);
    });
    refused(s, PCR_EVL_NO_ACTOR, "remove from a bad actor", [](PcRandoEvl& e) {
        return pc_rando_evl_remove_action(&e, (int)e.actorNum, 0);
    });
    refused(s, PCR_EVL_BAD_RANGE, "ending flag 3", [](PcRandoEvl& e) { return pc_rando_evl_set_ending_flag(&e, 0, 3, 1); });
    refused(s, PCR_EVL_BAD_RANGE, "starting flag 2", [](PcRandoEvl& e) { return pc_rando_evl_set_starting_flag(&e, 0, 2, 1); });
    refused(s, PCR_EVL_NO_EVENT, "flags of a bad event", [](PcRandoEvl& e) { return pc_rando_evl_set_ending_flag(&e, 3, 0, 1); });
    refused(s, PCR_EVL_BAD_RANGE, "action starting flag 3",
            [](PcRandoEvl& e) { return pc_rando_evl_set_action_starting_flag(&e, 0, 3, 1); });

    // Two properties that share an int slot: Os0 x0's Timer points at x1's.
    {
        Bytes sh = s;
        PcRandoEvl e;
        RT_CHECK_EQ(pc_rando_evl_open(&e, sh.data(), sh.size()), (int)PCR_EVL_OK);
        const int os0 = actorOf(e, "TACT_HT", "Os0");
        const int p0 = pc_rando_evl_action_property(&e, pc_rando_evl_actor_action(&e, os0, 0), "Timer");
        const int p1 = pc_rando_evl_action_property(&e, pc_rando_evl_actor_action(&e, os0, 1), "Timer");
        wr32(pc_rando_evl_property_p(&e, p0) + 0x28, (uint32_t)pc_rando_evl_property_slot(&e, p1));
        refused(sh, PCR_EVL_SHARED, "set_int on a shared slot", [&](PcRandoEvl& x) {
            return pc_rando_evl_set_int(&x, p0, 0, 5, nullptr);
        });
        refused(sh, PCR_EVL_SHARED, "set_ints on a shared slot", [&](PcRandoEvl& x) {
            const int32_t v = 5;
            return pc_rando_evl_set_ints(&x, p1, &v, 1, nullptr);
        });
        // The neighbour (x2) is not shared and still writes.
        Bytes ok = sh;
        PcRandoEvl x;
        RT_CHECK_EQ(pc_rando_evl_open(&x, ok.data(), ok.size()), (int)PCR_EVL_OK);
        const int p2 = pc_rando_evl_action_property(&x, pc_rando_evl_actor_action(&x, os0, 2), "Timer");
        RT_CHECK_EQ(pc_rando_evl_set_int(&x, p2, 0, 5, nullptr), (int)PCR_EVL_OK);
    }

    // A looping chain: Hr's a3 points back at a1.
    {
        Bytes lp = s;
        PcRandoEvl e;
        RT_CHECK_EQ(pc_rando_evl_open(&e, lp.data(), lp.size()), (int)PCR_EVL_OK);
        const int hr = actorOf(e, "Gp1_Get_Itm", "Hr");
        const int a1 = pc_rando_evl_actor_action(&e, hr, 1);
        wr32(pc_rando_evl_action_p(&e, pc_rando_evl_actor_action(&e, hr, 3)) + 0x3C, (uint32_t)a1);
        RT_CHECK_EQ(pc_rando_evl_action_count(&e, hr), -1);
        RT_CHECK_EQ(pc_rando_evl_find_action(&e, hr, "a8", 0), -1);
        refused(lp, PCR_EVL_LOOP, "truncate a looping chain",
                [&](PcRandoEvl& x) { return pc_rando_evl_truncate_actions(&x, hr, 2); });
        refused(lp, PCR_EVL_LOOP, "remove from a looping chain",
                [&](PcRandoEvl& x) { return pc_rando_evl_remove_action(&x, hr, 0); });
        // A chain that leaves the list.
        wr32(pc_rando_evl_action_p(&e, pc_rando_evl_actor_action(&e, hr, 0)) + 0x3C, e.actionNum);
        RT_CHECK_EQ(pc_rando_evl_action_count(&e, hr), -1);
        RT_CHECK_EQ(pc_rando_evl_actor_action(&e, hr, 1), -1);
    }
}

void testItemKinds(const Fixture& f) {
    // Every song id gives 059get_dance with id - 0x6D; the ids around them stay 011get_item.
    for (int id : {0x00, 0x07, 0x6C, 0x6D, 0x6E, 0x72, 0x73, 0xAA, 0xFF}) {
        Bytes b = f.src;
        PcRandoEvl e;
        RT_CHECK_EQ(pc_rando_evl_open(&e, b.data(), b.size()), (int)PCR_EVL_OK);
        int action = -1, slot = -1;
        RT_CHECK_EQ(pc_rando_evl_change_item(&e, "Gp1_Get_Itm", 0, 1, (uint8_t)id, "011get_item", 238, &action, &slot),
                    (int)PCR_EVL_OK);
        char name[PCR_EVL_NAME_MAX + 1];
        pc_rando_evl_name(pc_rando_evl_action_p(&e, action), name);
        const bool song = id >= 0x6D && id <= 0x72;
        RT_CHECK_EQ(std::string(name), std::string(song ? "059get_dance" : "011get_item"));
        const uint8_t* zeros = pc_rando_evl_action_p(&e, action) + strlen(name);
        bool padded = true;
        for (size_t i = strlen(name); i < PCR_EVL_NAME_MAX; i++) {
            padded = padded && zeros[i - strlen(name)] == 0;
        }
        RT_CHECK(padded);
        RT_CHECK_EQ((int32_t)rd32(b.data() + e.intTop + (uint32_t)slot * 4), song ? id - 0x6D : id);
    }
}

} // namespace

int main() {
    Fixture f;
    if (RT_CHECK(loadFixture(f))) {
        testFixture(f);
        testLookups(f);
        testRefusals(f);
        testItemKinds(f);
    }
    return rt::finish();
}
