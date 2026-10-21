// Tower of the Gods servants (pc_rando_tweak_totg.h, docs/RANDOMIZER_PLAN.md step R3.18): tweaks.py
// allow_nonlinear_servants_of_the_towers (wwrando 1.10.0) on the raw Siren stage.dzs, Room7 room.dzr
// and event_list.dat, through the dzx library (R0.9/R0.10) and the event_list.dat serializer (R3.14).
// Pure: no game headers, no game calls.
//
// The event edits follow tweaks.py statement by statement and in the same order, because the flags
// of new actors and actions come from get_unused_flag_id in call order and the serialized file must
// equal wwrando's. Python list operations map to the pool-index vectors of pcr::EvsList:
// list.remove(x) erases the first occurrence of x, list[-n] counts from the end, and a property's
// value assignment replaces its type and words. Values that wwrando writes as Python floats are
// written here as (float)<double literal>, which rounds as struct.pack(">f") does.
#include "pc_rando_tweak_totg.h"

#include "pc_rando_dzx.h"
#include "pc_rando_event.h"
#include "pc_rando_event_ser.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

bool sInit = false;
int sEvntIndex = -1;
PcRandoTotgStatus sStatus[PCR_TOTG_FILE_COUNT];

void resetStatus() {
    for (PcRandoTotgStatus& s : sStatus) {
        s = PcRandoTotgStatus{0, 0, -1, 0, 0};
    }
}

struct StatusInit {
    StatusInit() { resetStatus(); }
} sStatusInit;

constexpr uint32_t kActrSize = 0x20;
constexpr uint32_t kEvntSize = 0x18;
constexpr const char* const kDzsTags[2] = {"TGDR", "EVNT"};
constexpr const char* const kDzrTags[2] = {"ACTR", "SCOB"};

void putBe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}
void putBe32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
void putBeF32(uint8_t* p, float f) {
    uint32_t v;
    memcpy(&v, &f, 4);
    putBe32(p, v);
}

// The index of the nth (0-based) entry of `tag` named `name`, or -1.
int findNamed(const PcRandoDzx* dzx, const char* tag, const char* name, int nth = 0) {
    const uint32_t n = pc_rando_dzx_count(dzx, tag);
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t* e = pc_rando_dzx_entry(dzx, tag, i);
        char nm[9];
        memcpy(nm, e, 8);
        nm[8] = 0;
        if (strcmp(nm, name) == 0 && nth-- == 0) {
            return (int)i;
        }
    }
    return -1;
}

// One byte edit with its fingerprint (D9): entry `index` of `tag` is named `name` and holds `old`
// at `off`.
struct ByteEdit {
    const char* what;
    const char* tag;
    int index;
    const char* name;
    uint32_t off;
    uint8_t old;
    uint8_t value;
};

// Checks every edit, then writes them all. A PcRandoDzxResult.
int applyBytes(PcRandoDzx* dzx, const char* file, const ByteEdit* edits, int n) {
    for (int i = 0; i < n; i++) {
        const ByteEdit& e = edits[i];
        if (e.index < 0) {
            logf("[cos] rando totg: %s: %s not found\n", file, e.what);
            return PCR_DZX_MISMATCH;
        }
        const PcRandoDzxFingerprint fp = {e.name, (int32_t)e.off, &e.old, 1};
        const int r = pc_rando_dzx_check(dzx, e.tag, (uint32_t)e.index, &fp);
        if (r != PCR_DZX_OK) {
            logf("[cos] rando totg: %s: %s (%s[%d]): %s\n", file, e.what, e.tag, e.index, pc_rando_dzx_result_name(r));
            return r;
        }
    }
    for (int i = 0; i < n; i++) {
        const ByteEdit& e = edits[i];
        const int r = pc_rando_dzx_set_u8(dzx, e.tag, (uint32_t)e.index, e.off, e.value, nullptr);
        if (r != PCR_DZX_OK) {
            return r;
        }
    }
    return PCR_DZX_OK;
}

// An ACTR entry as wwlib's ACTR class writes a new one: rotation 0 and enemy_number 0xFFFF unless
// given.
void makeActr(uint8_t* e, const char* name, uint32_t params, float x, float y, float z, uint16_t xRot) {
    memset(e, 0, kActrSize);
    strncpy((char*)e, name, 8);
    putBe32(e + 0x08, params);
    putBeF32(e + 0x0C, x);
    putBeF32(e + 0x10, y);
    putBeF32(e + 0x14, z);
    putBe16(e + 0x18, xRot);
    putBe16(e + 0x1A, 0);
    putBe16(e + 0x1C, 0);
    putBe16(e + 0x1E, 0xFFFF);
}

// SwOp params (actor_parameters.txt d_a_switch_op).
constexpr uint32_t swopParams(uint32_t op, uint32_t continuous, uint32_t toSet, uint32_t first, uint32_t count) {
    return (op & 0xF) | (continuous ? 0x10u : 0u) | (toSet << 8) | (first << 16) | (count << 24);
}

// --- event_list.dat ---

using pcr::EvsList;
using pcr::EvsVec;

// The names of an actor's actions, comma-separated.
void actionNames(const EvsList& L, int actor, char* out, size_t cap) {
    size_t n = 0;
    out[0] = 0;
    for (int c : L.actors[actor].actions) {
        const pcr::EvsString s = L.actions[c].name.str();
        const int w = snprintf(out + n, cap - n, "%s%s", n ? "," : "", s.c_str());
        if (w < 0 || (size_t)w >= cap - n) {
            return;
        }
        n += (size_t)w;
    }
}

// The vanilla shape of every actor the edits read or change (USA disc). Link in hsehi1_tact is only
// counted: step R2.4 may already have renamed its 059get_dance (the Stone Tablet location).
struct Shape {
    const char* event;
    const char* actor;
    const char* names; // nullptr: only `count` is checked
    int count;
};
constexpr const char* kTk8 = "COUNTDOWN,WAIT,COUNTDOWN,WAIT,COUNTDOWN,WAIT,COUNTDOWN,WAIT";
constexpr const char* kHdai = "WAIT,EFFSET,MOVE,EFFEND,WAIT";
constexpr Shape kShapes[] = {
    {"Os_Finish", "TIMEKEEPER", "COUNTDOWN,WAIT,COUNTDOWN,WAIT,COUNTDOWN,WAIT,COUNTDOWN,WAIT,COUNTDOWN,WAIT", 10},
    {"Os_Finish", "Os", "WAIT,MOVE,MOVEEND,TURN,WAIT,FINISH,WAIT,WAIT,END", 9},
    {"Os_Finish", "Hdai1", kHdai, 5},
    {"Os_Finish", "CAMERA", "PAUSE,FIXEDFRM,FIXEDFRM,UNITRANS,UNITRANS,PAUSE,FIXEDFRM,PAUSE", 8},
    {"Os_Finish", "Hsh", "WAIT,Disp,Appear,WAIT,Disp,WAIT", 6},
    {"Os1_Finish", "TIMEKEEPER", kTk8, 8},
    {"Os1_Finish", "Os1", "WAIT,MOVE,MOVEEND,TURN,WAIT,FINISH,WAIT,FINISH,SW_ON,END,WAIT", 11},
    {"Os1_Finish", "Hdai2", kHdai, 5},
    {"Os1_Finish", "CAMERA", "PAUSE,FIXEDFRM,FIXEDFRM,UNITRANS,UNITRANS,UNITRANS,PAUSE", 7},
    {"Os1_Message", "Os1", "WAIT,FINISH,WAIT,WAIT", 4},
    {"Os1_Message", "CAMERA", "FIXEDFRM,PAUSE", 2},
    {"Os2_Finish", "TIMEKEEPER", kTk8, 8},
    {"Os2_Finish", "Os2", "WAIT,MOVE,MOVEEND,TURN,WAIT,FINISH,WAIT,FINISH,SW_ON,END", 10},
    {"Os2_Finish", "Hdai3", kHdai, 5},
    {"Os2_Finish", "CAMERA", "PAUSE,FIXEDFRM,FIXEDFRM,UNITRANS,UNITRANS,UNITRANS,FIXEDFRM,FIXEDFRM,PAUSE", 9},
    {"Os2_Finish", "Os1", "WAIT,FINISH,END", 3},
    {"Os2_Finish", "Os", "WAIT,FINISH,END", 3},
    {"hsehi1_tact", "CAMERA", "TACT,PAUSE,SAVE,FIXEDFRM,LOAD,TACT,GETITEM,UNITRANS", 8},
    {"hsehi1_tact", "Hsh", "Disp,WAIT,Disp,Disp,Disp,Disp,WAIT,MsgSet,Talk,WAIT,Tact,WAIT,Judge,WAIT,Disp,WAIT,WAIT", 17},
    {"hsehi1_tact", "TIMEKEEPER", "COUNTDOWN,WAIT", 2},
    {"hsehi1_tact", "Link", nullptr, 6},
};

int checkShapes(const EvsList& L) {
    for (const Shape& s : kShapes) {
        const int ev = L.findEvent(s.event);
        if (ev < 0) {
            logf("[cos] rando totg: event %s not found\n", s.event);
            return PCR_EVL_NO_EVENT;
        }
        const int a = L.findActor(ev, s.actor);
        if (a < 0) {
            logf("[cos] rando totg: %s has no actor %s\n", s.event, s.actor);
            return PCR_EVL_NO_ACTOR;
        }
        if ((int)L.actors[a].actions.size() != s.count) {
            logf("[cos] rando totg: %s/%s has %d actions, expected %d\n", s.event, s.actor,
                 (int)L.actors[a].actions.size(), s.count);
            return PCR_EVL_MISMATCH;
        }
        if (s.names != nullptr) {
            char names[512];
            actionNames(L, a, names, sizeof names);
            if (strcmp(names, s.names) != 0) {
                logf("[cos] rando totg: %s/%s actions are '%s'\n", s.event, s.actor, names);
                return PCR_EVL_MISMATCH;
            }
        }
    }
    if (L.findEvent(PCR_TOTG_APPEAR_EVENT) >= 0) {
        logf("[cos] rando totg: the event list already has %s\n", PCR_TOTG_APPEAR_EVENT);
        return PCR_EVL_MISMATCH;
    }
    return PCR_EVL_OK;
}

// The edits' lookups. A failed one records the first failure and every later call does nothing
// (the list is a copy, so a failure leaves the file vanilla); indexes < 0 are failures. No
// exceptions: the port builds with -fno-exceptions.
class Editor {
public:
    explicit Editor(EvsList& l) : L(l) {}
    EvsList& L;
    int fail = PCR_EVL_OK;
    const char* failWhat = "";

    int bad(int code, const char* what) {
        if (fail == PCR_EVL_OK) {
            fail = code;
            failWhat = what;
        }
        return -1;
    }
    bool okEvent(int e) { return fail == PCR_EVL_OK && e >= 0 && (size_t)e < L.events.size(); }
    bool okActor(int a) { return fail == PCR_EVL_OK && a >= 0 && (size_t)a < L.actors.size(); }
    bool okAction(int c) { return fail == PCR_EVL_OK && c >= 0 && (size_t)c < L.actions.size(); }

    int event(const char* name) {
        const int e = fail == PCR_EVL_OK ? L.findEvent(name) : -1;
        return e >= 0 ? e : bad(PCR_EVL_NO_EVENT, name);
    }
    int actor(int ev, const char* name) {
        const int a = okEvent(ev) ? L.findActor(ev, name) : -1;
        return a >= 0 ? a : bad(PCR_EVL_NO_ACTOR, name);
    }
    // event.actors / actor.actions; a scratch list after a failure.
    EvsVec<int>& evActors(int ev) {
        if (!okEvent(ev)) {
            bad(PCR_EVL_NO_EVENT, "event");
            mScratch.clear();
            return mScratch;
        }
        return L.events[ev].actors;
    }
    EvsVec<int>& acts(int actor) {
        if (!okActor(actor)) {
            bad(PCR_EVL_NO_ACTOR, "actor");
            mScratch.clear();
            return mScratch;
        }
        return L.actors[actor].actions;
    }
    // actor.actions[i] (negative from the end).
    int act(int actor, int i) {
        EvsVec<int>& v = acts(actor);
        const int n = (int)v.size();
        const int k = i < 0 ? n + i : i;
        if (fail != PCR_EVL_OK || k < 0 || k >= n) {
            return bad(PCR_EVL_NO_ACTION, "action index");
        }
        return v[(size_t)k];
    }
    // next(act for act in actor.actions if act.name == name), the nth such.
    int actNamed(int actor, const char* name, int nth = 0) {
        const int c = okActor(actor) ? L.findAction(actor, name, nth) : -1;
        return c >= 0 ? c : bad(PCR_EVL_NO_ACTION, name);
    }
    int prop(int action, const char* name) {
        const int q = okAction(action) ? L.getProp(action, name) : -1;
        return q >= 0 ? q : bad(PCR_EVL_NO_PROPERTY, name);
    }
    int addActor(int ev, const char* name) {
        const int a = okEvent(ev) ? L.addActor(ev, name) : -1;
        return a >= 0 ? a : bad(PCR_EVL_BAD_RANGE, "add_actor (no flag left)");
    }
    void staffType(int actor, uint32_t t) {
        if (okActor(actor)) {
            L.actors[actor].staffType = t;
        }
    }
    int addAction(int actor, const char* name) {
        const int c = okActor(actor) ? L.addAction(actor, name) : -1;
        return c >= 0 ? c : bad(PCR_EVL_BAD_RANGE, "add_action (no flag left)");
    }
    void addProp(int q, const char* what) {
        if (q < 0) {
            bad(PCR_EVL_BAD_RANGE, what);
        }
    }
    void addString(int action, const char* name, const char* s) {
        if (okAction(action)) {
            addProp(L.addPropString(action, name, s), name);
        }
    }
    int32_t flag(int action) { return okAction(action) ? L.actions[action].flag : bad(PCR_EVL_NO_ACTION, "flag"); }
    void startFlag(int action, int32_t f) {
        if (okAction(action)) {
            L.actions[action].startingFlags[0] = f;
        }
    }
    void duplicateId(int action, uint32_t d) {
        if (okAction(action)) {
            L.actions[action].duplicateId = d;
        }
    }
    void endFlag(int ev, int32_t f) {
        if (okEvent(ev)) {
            L.events[ev].endingFlags[0] = f;
        }
    }

    // list.remove(x)
    void removeVal(EvsVec<int>& v, int x) {
        if (fail != PCR_EVL_OK) {
            return;
        }
        for (size_t i = 0; i < v.size(); i++) {
            if (v[i] == x) {
                v.erase(v.begin() + (ptrdiff_t)i);
                return;
            }
        }
        bad(PCR_EVL_BAD_RANGE, "list.remove");
    }
    // actor.actions.remove(actor.actions[i])
    void removeAt(int actor, int i) {
        const int c = act(actor, i);
        removeVal(acts(actor), c);
    }

    void setVec(int action, const char* name, double x, double y, double z) {
        const int q = prop(action, name);
        if (q >= 0) {
            const float v[3] = {(float)x, (float)y, (float)z};
            L.props[q].setVecs(v, 1);
        }
    }
    void setInt(int action, const char* name, int32_t v) {
        const int q = prop(action, name);
        if (q >= 0) {
            L.props[q].setInts(&v, 1);
        }
    }
    // prop.value *= m for a one-float property.
    void scaleFloat(int action, const char* name, int m) {
        const int q = prop(action, name);
        if (q < 0) {
            return;
        }
        pcr::EvsProperty& p = L.props[q];
        if (p.type != PCR_EVL_TYPE_FLOAT || p.words.size() != 1) {
            bad(PCR_EVL_BAD_TYPE, name);
            return;
        }
        float f;
        memcpy(&f, &p.words[0], 4);
        const float r = (float)((double)f * m);
        p.setFloats(&r, 1);
    }
    // prop.value //= d for a one-int property (Python floor division).
    void floorDiv(int action, const char* name, int32_t d) {
        const int q = prop(action, name);
        if (q < 0) {
            return;
        }
        pcr::EvsProperty& p = L.props[q];
        if (p.type != PCR_EVL_TYPE_INT || p.words.size() != 1) {
            bad(PCR_EVL_BAD_TYPE, name);
            return;
        }
        const int32_t v = (int32_t)p.words[0];
        int32_t r = v / d;
        if (v % d != 0 && ((v < 0) != (d < 0))) {
            r--;
        }
        p.words[0] = (uint32_t)r;
    }

private:
    EvsVec<int> mScratch;
};

// A property as (name, value), copied the way tweaks.py keeps camera_tablet_fixedfrm_props.
struct SavedProp {
    char name[pcr::kEvsNameSize];
    uint32_t type;
    EvsVec<uint32_t> words;
    pcr::EvsString str;
};

int addSavedProp(EvsList& L, int action, const SavedProp& s) {
    int q;
    switch (s.type) {
    case PCR_EVL_TYPE_INT:
        q = L.addPropInts(action, s.name, reinterpret_cast<const int32_t*>(s.words.data()), (int)s.words.size());
        break;
    case PCR_EVL_TYPE_STRING:
        q = L.addPropString(action, s.name, s.str.c_str());
        break;
    default: {
        q = L.addPropInts(action, s.name, nullptr, 0);
        if (q >= 0) {
            L.props[q].type = s.type;
            L.props[q].words = s.words;
        }
        break;
    }
    }
    return q;
}

// tweaks.py allow_nonlinear_servants_of_the_towers, the event_list.dat part. A PcRandoEvlResult
// (*what names the failed lookup).
int editEvents(EvsList& L, const char** what) {
    Editor E(L);
    // --- East servant returned (Os_Finish) ---
    const int os0Finish = E.event("Os_Finish");
    const int os0 = E.actor(os0Finish, "Os");
    const int tablet = E.actor(os0Finish, "Hsh");
    const int tk = E.actor(os0Finish, "TIMEKEEPER");
    const int cam = E.actor(os0Finish, "CAMERA");
    // set_switch_action = os0.add_action("SW_ON"); remove; insert(7, ...)
    const int sw = E.addAction(os0, "SW_ON");
    E.removeVal(E.acts(os0), sw);
    if (E.acts(os0).size() < 7) {
        E.bad(PCR_EVL_BAD_RANGE, "Os insert");
    }
    if (E.fail == PCR_EVL_OK) {
        E.acts(os0).insert(E.acts(os0).begin() + 7, sw);
    }
    E.removeVal(E.evActors(os0Finish), tablet);
    E.startFlag(E.act(os0, -1), -1);
    E.startFlag(E.act(tk, -2), -1);
    E.startFlag(E.act(cam, 6), -1);
    E.removeAt(tk, -1);
    E.removeAt(tk, -1);
    const int os0Unitrans = E.act(cam, 3);
    E.setVec(os0Unitrans, "Eye", 546.0, 719.0, -8789.0);
    E.setVec(os0Unitrans, "Center", 783.0, 582.0, -9085.0);
    const int pauseAct = E.act(cam, 7);
    const int fixedAct = E.act(cam, 6);
    // camera_tablet_fixedfrm_props: (name, value) of each property, in order.
    EvsVec<SavedProp> saved;
    if (E.okAction(fixedAct)) {
        for (int q : L.actions[fixedAct].props) {
            SavedProp sp;
            memcpy(sp.name, L.props[q].name.raw, sizeof sp.name);
            sp.name[sizeof sp.name - 1] = 0;
            sp.type = L.props[q].type;
            sp.words = L.props[q].words;
            sp.str = L.props[q].str;
            saved.push_back(sp);
        }
    }
    E.removeVal(E.acts(cam), pauseAct);
    E.removeVal(E.acts(cam), fixedAct);
    E.setInt(E.actNamed(os0, "FINISH"), "Type", 2);

    // --- West servant returned (Os1_Finish) ---
    const int os1Finish = E.event("Os1_Finish");
    {
        const int os1 = E.actor(os1Finish, "Os1");
        const int cam1 = E.actor(os1Finish, "CAMERA");
        const int finish0 = E.actNamed(os1, "FINISH", 0);
        const int finish1 = E.actNamed(os1, "FINISH", 1);
        E.setInt(finish1, "Type", 2);
        const int ut = E.act(cam1, 3);
        E.setVec(ut, "Eye", -512.0, 626.0, -8775.0);
        E.setVec(ut, "Center", -790.0, 667.0, -9065.0);
        E.removeAt(cam1, -2);
        E.removeAt(os1, -1);
        E.endFlag(os1Finish, E.flag(E.act(os1, -1)));
        E.removeVal(E.acts(os1), finish0);
        E.startFlag(finish1, E.flag(E.act(cam1, -2)));
    }

    // --- After the west servant returned (Os1_Message): keep only the last actions ---
    {
        const int os1Message = E.event("Os1_Message");
        const int os1 = E.actor(os1Message, "Os1");
        const int cam1 = E.actor(os1Message, "CAMERA");
        const int lastOs1 = E.act(os1, -1);
        const int lastCam = E.act(cam1, -1);
        if (E.fail == PCR_EVL_OK) {
            E.acts(os1).assign(1, lastOs1);
            E.acts(cam1).assign(1, lastCam);
        }
    }

    // --- North servant returned (Os2_Finish) ---
    const int os2Finish = E.event("Os2_Finish");
    {
        const int os0b = E.actor(os2Finish, "Os");
        const int os1b = E.actor(os2Finish, "Os1");
        const int os2 = E.actor(os2Finish, "Os2");
        const int cam2 = E.actor(os2Finish, "CAMERA");
        E.removeVal(E.evActors(os2Finish), os0b);
        E.removeVal(E.evActors(os2Finish), os1b);
        E.startFlag(E.actNamed(os2, "SW_ON"), -1);
        E.startFlag(E.act(cam2, -1), -1);
        E.removeAt(cam2, -3);
        E.removeAt(cam2, -2);
        const int ut = E.act(cam2, 3);
        E.setVec(ut, "Eye", 124.0, 589.0, -9482.0);
        E.setVec(ut, "Center", -7.0, 644.0, -9625.0);
        E.removeAt(cam2, -1);
        E.removeAt(cam2, -1);
        const int finish0 = E.actNamed(os2, "FINISH", 0);
        const int finish1 = E.actNamed(os2, "FINISH", 1);
        E.removeVal(E.acts(os2), finish0);
        E.startFlag(finish1, E.flag(E.act(cam2, -1)));
    }

    // --- The tablet's Command Melody event (hsehi1_tact) ---
    const int tact = E.event("hsehi1_tact");
    {
        const int camT = E.actor(tact, "CAMERA");
        const int hsh = E.actor(tact, "Hsh");
        (void)E.actor(tact, "TIMEKEEPER");
        const int link = E.actor(tact, "Link");
        E.removeAt(camT, -1);
        E.removeAt(hsh, -1);
        E.removeAt(hsh, -1);
        const int hide = E.addAction(hsh, "Disp");
        E.addString(hide, "target", "@PLAYER");
        E.addString(hide, "disp", "off");
        const int del = E.addAction(hsh, "Delete");
        const int fixedfrm = E.addAction(camT, "FIXEDFRM");
        if (E.okAction(fixedfrm)) {
            E.addProp(L.addPropVec(fixedfrm, "Eye", (float)3.314825, (float)690.2266, (float)-8600.536), "Eye");
            E.addProp(L.addPropVec(fixedfrm, "Center", (float)0.82259, (float)677.7084, (float)-8721.426), "Center");
            E.addProp(L.addPropFloat(fixedfrm, "Fovy", (float)60.0), "Fovy");
            E.addProp(L.addPropInt(fixedfrm, "Timer", 30), "Timer");
        }
        E.startFlag(fixedfrm, E.flag(E.act(link, 4))); // link.actions[4], 059get_dance
        E.startFlag(del, E.flag(fixedfrm));
        const int show = E.addAction(hsh, "Disp");
        E.addString(show, "target", "@PLAYER");
        E.addString(show, "disp", "on");
        E.addAction(hsh, "WAIT");
        E.endFlag(tact, E.flag(E.act(hsh, -1)));
    }

    // --- The new event that makes the tablet appear (hsehi1_appear) ---
    {
        const int appear = E.fail == PCR_EVL_OK ? L.addEvent(PCR_TOTG_APPEAR_EVENT) : -1;
        if (appear < 0) {
            E.bad(PCR_EVL_BAD_RANGE, "add_event");
        }
        const int camA = E.addActor(appear, "CAMERA");
        E.staffType(camA, 2);
        const int tabletA = E.addActor(appear, "Hsh");
        E.staffType(tabletA, 0);
        E.addAction(tabletA, "WAIT");
        const int linkA = E.addActor(appear, "Link");
        E.staffType(linkA, 0);
        E.addAction(linkA, "001n_wait");
        const int tkA = E.addActor(appear, "TIMEKEEPER");
        E.staffType(tkA, 4);
        E.addAction(tkA, "WAIT");
        const int fixedA = E.addAction(camA, "FIXEDFRM");
        if (E.okAction(fixedA)) {
            for (const SavedProp& sp : saved) {
                E.addProp(addSavedProp(L, fixedA, sp), "FIXEDFRM property");
            }
        }
        E.addAction(camA, "PAUSE");
        const int appearAct = E.addAction(tabletA, "Appear");
        E.startFlag(appearAct, E.flag(fixedA));
        const int countdown = E.addAction(tkA, "COUNTDOWN");
        if (E.okAction(countdown)) {
            E.addProp(L.addPropInt(countdown, "Timer", 90), "Timer");
        }
        E.duplicateId(countdown, 1);
        E.startFlag(countdown, E.flag(appearAct));
        const int wait1 = E.addAction(tabletA, "WAIT");
        E.duplicateId(wait1, 1);
        E.startFlag(wait1, E.flag(countdown));
        E.endFlag(appear, E.flag(wait1));
    }

    // --- A SwOp staff with a DUMMY cut in the four events ---
    for (int ev : {os0Finish, os1Finish, os2Finish, tact}) {
        const int swop = E.addActor(ev, "SwOp");
        E.addAction(swop, "DUMMY");
    }

    // --- Speed up the servants, their platforms and the beams ---
    constexpr int kServant = 4, kPlatform = 2, kBeam = 2;
    for (int ev : {os0Finish, os1Finish, os2Finish}) {
        int servant = -1, platform = -1;
        for (int a : E.evActors(ev)) {
            const pcr::EvsName& n = L.actors[a].name;
            if (servant < 0 && (n.is("Os") || n.is("Os1") || n.is("Os2"))) {
                servant = a;
            }
            if (platform < 0 && (n.is("Hdai1") || n.is("Hdai2") || n.is("Hdai3"))) {
                platform = a;
            }
        }
        if (servant < 0 || platform < 0) {
            E.bad(PCR_EVL_NO_ACTOR, "servant or platform");
        }
        const int tkS = E.actor(ev, "TIMEKEEPER");
        const int camS = E.actor(ev, "CAMERA");
        E.scaleFloat(E.actNamed(servant, "MOVE"), "Stick", kServant);  // 0.5 -> 2.0
        E.scaleFloat(E.actNamed(platform, "MOVE"), "Speed", kPlatform); // 2.5 -> 5.0
        E.floorDiv(E.actNamed(tkS, "COUNTDOWN", 0), "Timer", kServant); // 60 -> 15
        E.floorDiv(E.actNamed(tkS, "COUNTDOWN", 1), "Timer", kServant); // 210 -> 52
        E.floorDiv(E.actNamed(tkS, "COUNTDOWN", 3), "Timer", kBeam);    // 30 or 60 -> 15 or 30
        E.floorDiv(E.actNamed(camS, "UNITRANS", 0), "Timer", kPlatform); // 90 -> 45
    }
    *what = E.failWhat;
    return E.fail;
}

void note(int file, int result, uint64_t hash, uint32_t size) {
    PcRandoTotgStatus& s = sStatus[file];
    s.seen++;
    s.last = result;
    if (result == 0) {
        s.applied++;
        s.hash = hash;
        s.size = size;
    }
}

} // namespace

extern "C" uint64_t pc_rando_tweak_totg_content_hash(const void* data, size_t size, const char* const* tags,
                                                     int nTags) {
    uint64_t h = 0xcbf29ce484222325ull;
    auto mix = [&h](const uint8_t* p, size_t n) {
        for (size_t i = 0; i < n; i++) {
            h ^= p[i];
            h *= 0x100000001b3ull;
        }
    };
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, const_cast<void*>(data), size) != PCR_DZX_OK) {
        return 0;
    }
    for (int t = 0; t < nTags; t++) {
        mix((const uint8_t*)tags[t], 4);
        const bool actorChunk = strcmp(tags[t], "EVNT") != 0;
        const uint32_t esize = pc_rando_dzx_entry_size(tags[t]);
        const uint32_t n = pc_rando_dzx_count(&dzx, tags[t]);
        for (uint32_t i = 0; i < n; i++) {
            const uint8_t* e = pc_rando_dzx_entry(&dzx, tags[t], i);
            if (e == nullptr || (actorChunk && e[0] == 0)) {
                continue;
            }
            mix(e, esize);
        }
    }
    return h;
}

extern "C" int pc_rando_tweak_totg_dzs(PcRandoResFile* file) {
    sEvntIndex = -1;
    PcRandoDzx dzx;
    int r = pc_rando_dzx_open(&dzx, file->data, file->size);
    if (r != PCR_DZX_OK) {
        return r;
    }
    // tweaks.py: doors = totg.entries_by_type(TGDR); north_door = doors[6]; west_door = doors[8].
    const ByteEdit edits[] = {
        {"north door switch_1", "TGDR", 6, "door10", 0x0B, 0x2A, 0xFF},
        {"west door switch_1", "TGDR", 8, "door10", 0x0B, 0x29, 0xFF},
    };
    if (pc_rando_dzx_find(&dzx, "EVNT") < 0) {
        logf("[cos] rando totg: stage.dzs has no EVNT chunk\n");
        return PCR_DZX_NO_CHUNK;
    }
    if (findNamed(&dzx, "TGDR", "door10", 0) < 0 || pc_rando_dzx_count(&dzx, "TGDR") < 9) {
        logf("[cos] rando totg: stage.dzs TGDR shape differs\n");
        return PCR_DZX_MISMATCH;
    }
    r = applyBytes(&dzx, "stage.dzs", edits, 2);
    if (r != PCR_DZX_OK) {
        return r;
    }
    // totg.add_entity(EVNT) with EVNT's defaults and the name.
    uint8_t evnt[kEvntSize];
    memset(evnt, 0, sizeof evnt);
    evnt[0x00] = 0xFF; // unknown_1
    strncpy((char*)evnt + 1, PCR_TOTG_APPEAR_EVENT, 0xF);
    evnt[0x10] = 0xFF; // unknown_2
    evnt[0x11] = 0xFF; // unknown_3
    evnt[0x12] = 0x00; // unknown_4
    evnt[0x13] = 0xFF; // event_played_by_spawn_switch
    evnt[0x14] = 0xFF; // room_index
    evnt[0x15] = evnt[0x16] = evnt[0x17] = 0xFF;
    const PcRandoDzxAdd add = {"EVNT", evnt, 1};
    uint32_t first = 0;
    r = pc_rando_res_grow_dzx(file, &add, 1, &first);
    if (r != PCR_DZX_OK) {
        // The door bytes were written to the archive's own buffer; put them back (vanilla).
        PcRandoDzx back;
        if (pc_rando_dzx_open(&back, file->data, file->size) == PCR_DZX_OK) {
            pc_rando_dzx_set_u8(&back, "TGDR", 6, 0x0B, 0x2A, nullptr);
            pc_rando_dzx_set_u8(&back, "TGDR", 8, 0x0B, 0x29, nullptr);
        }
        return r;
    }
    sEvntIndex = (int)first;
    return PCR_DZX_OK;
}

extern "C" int pc_rando_tweak_totg_dzr(PcRandoResFile* file) {
    if (sEvntIndex < 0 || sEvntIndex > 0xFE) {
        logf("[cos] rando totg: Room7 room.dzr left vanilla (stage.dzs has no %s EVNT)\n", PCR_TOTG_APPEAR_EVENT);
        return PCR_DZX_MISMATCH;
    }
    PcRandoDzx dzx;
    int r = pc_rando_dzx_open(&dzx, file->data, file->size);
    if (r != PCR_DZX_OK) {
        return r;
    }
    // next(x for x in hub_room_dzr.entries_by_type(SCOB) if x.name == "AttTag" and
    // x.switch_to_check == 0x28 and x.type == 1)
    int attTag = -1;
    for (int nth = 0;; nth++) {
        const int i = findNamed(&dzx, "SCOB", "AttTag", nth);
        if (i < 0) {
            break;
        }
        const uint32_t prm = pc_rando_dzx_be32(pc_rando_dzx_entry(&dzx, "SCOB", (uint32_t)i) + 8);
        if ((prm & 0xFF) == PCR_TOTG_SW_NORTH && ((prm >> 8) & 3) == 1) {
            attTag = i;
            break;
        }
    }
    const ByteEdit edits[] = {
        {"Hsh switch_to_set", "ACTR", findNamed(&dzx, "ACTR", "Hsh"), "Hsh", 0x0B, PCR_TOTG_SW_EAST, PCR_TOTG_SW_TABLET},
        {"Ywarp00 activation_switch", "ACTR", findNamed(&dzx, "ACTR", "Ywarp00"), "Ywarp00", 0x0B, PCR_TOTG_SW_NORTH,
         PCR_TOTG_SW_ALL},
        {"kytag00 switch_to_check", "SCOB", findNamed(&dzx, "SCOB", "kytag00"), "kytag00", 0x19, PCR_TOTG_SW_NORTH,
         PCR_TOTG_SW_ALL},
        {"AttTag switch_to_check", "SCOB", attTag, "AttTag", 0x0B, PCR_TOTG_SW_NORTH, PCR_TOTG_SW_ALL},
    };
    r = applyBytes(&dzx, "Room7 room.dzr", edits, 4);
    if (r != PCR_DZX_OK) {
        return r;
    }
    uint8_t swops[2 * kActrSize];
    // OR over the three servant switches -> 0x7E, after the tablet's event.
    makeActr(swops, "SwOp", swopParams(2, 0, PCR_TOTG_SW_ANY, PCR_TOTG_SW_NORTH, 3), -800.0f, 1000.0f, -9000.0f,
             (uint16_t)sEvntIndex);
    // AND over the three and the tablet's switch -> 0x7F, no event.
    makeActr(swops + kActrSize, "SwOp", swopParams(0, 0, PCR_TOTG_SW_ALL, PCR_TOTG_SW_NORTH, 4), 800.0f, 1000.0f,
             -9000.0f, 0xFF);
    const PcRandoDzxAdd add = {"ACTR", swops, 2};
    r = pc_rando_res_grow_dzx(file, &add, 1, nullptr);
    if (r != PCR_DZX_OK) {
        PcRandoDzx back;
        if (pc_rando_dzx_open(&back, file->data, file->size) == PCR_DZX_OK) {
            for (const ByteEdit& e : edits) {
                pc_rando_dzx_set_u8(&back, e.tag, (uint32_t)e.index, e.off, e.old, nullptr);
            }
        }
    }
    return r;
}

namespace {

// The event list edit; on success *hash and *size describe the serialized file.
int patchEvl(PcRandoResFile* file, uint64_t* hash, uint32_t* size) {
    EvsList L;
    int r = L.read(file->data, file->size);
    if (r != PCR_EVL_OK) {
        return r;
    }
    r = checkShapes(L);
    if (r != PCR_EVL_OK) {
        return r;
    }
    const char* what = "";
    r = editEvents(L, &what);
    if (r != PCR_EVL_OK) {
        logf("[cos] rando totg: event edit failed at '%s': %s\n", what, pc_rando_evl_result_name(r));
        return r;
    }
    pcr::EvsBytes bytes;
    r = L.serialize(bytes);
    if (r != PCR_EVL_OK) {
        return r;
    }
    if (hash != nullptr) {
        *hash = pcr::pc_rando_evser_fnv64(bytes.data(), bytes.size());
    }
    if (size != nullptr) {
        *size = (uint32_t)bytes.size();
    }
    return pcr::pc_rando_evser_write_res(file, L);
}

} // namespace

extern "C" int pc_rando_tweak_totg_evl(PcRandoResFile* file) {
    return patchEvl(file, nullptr, nullptr);
}

extern "C" void pc_rando_tweak_totg_patch(PcRandoResFile* file) {
    if (file == nullptr || strcmp(file->stage, "Siren") != 0) {
        return;
    }
    int which;
    if (strcmp(file->arc, "Stage") == 0 && strcmp(file->name, "stage.dzs") == 0) {
        which = PCR_TOTG_DZS;
    } else if (strcmp(file->arc, "Room7") == 0 && strcmp(file->name, "room.dzr") == 0) {
        which = PCR_TOTG_DZR;
    } else if (strcmp(file->arc, "Stage") == 0 && strcmp(file->name, "event_list.dat") == 0) {
        which = PCR_TOTG_EVL;
    } else {
        return;
    }
    int r;
    uint64_t hash = 0;
    uint32_t size = 0;
    const char* rname;
    if (which == PCR_TOTG_EVL) {
        r = patchEvl(file, &hash, &size);
        rname = pc_rando_evl_result_name(r);
    } else {
        r = which == PCR_TOTG_DZS ? pc_rando_tweak_totg_dzs(file) : pc_rando_tweak_totg_dzr(file);
        rname = pc_rando_dzx_result_name(r);
        if (r == PCR_DZX_OK) {
            hash = pc_rando_tweak_totg_content_hash(file->data, file->size, which == PCR_TOTG_DZS ? kDzsTags : kDzrTags, 2);
        }
    }
    note(which, r, hash, size);
    logf("[cos] rando totg: Siren/%s %s (allow_nonlinear_servants_of_the_towers): %s\n", file->arc, file->name, rname);
}

extern "C" void pc_rando_tweak_totg_init(void) {
    if (sInit) {
        return;
    }
    sInit = true;
    pc_rando_register_res_patcher(pc_rando_tweak_totg_patch, PCR_RES_ANY);
}

extern "C" int pc_rando_tweak_totg_evnt_index(void) {
    return sEvntIndex;
}

extern "C" void pc_rando_tweak_totg_status(int file, PcRandoTotgStatus* out) {
    if (out == nullptr) {
        return;
    }
    if (file < 0 || file >= PCR_TOTG_FILE_COUNT) {
        *out = PcRandoTotgStatus{0, 0, -1, 0, 0};
        return;
    }
    *out = sStatus[file];
}

extern "C" void pc_rando_tweak_totg_reset_status(void) {
    resetStatus();
    sEvntIndex = -1;
}
