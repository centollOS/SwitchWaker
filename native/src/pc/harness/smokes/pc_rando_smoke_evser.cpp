// COS_SMOKE=rando COS_RANDO_SMOKE=evser (docs/RANDOMIZER_PLAN.md step R3.14): the event_list.dat
// serializer (features/rando/pc_rando_event_ser.h) on every stage's file, run as
//   run.sh rando --stage Orichh:0 --env COS_RANDO_SMOKE=evser [--env COS_RANDO_FORCE=1]
//
// 1. The sweep (always). Once the play scene has run 10 frames, every /res/Stage/<stage>/Stage.arc
//    of the disc (the FST walked with DVDOpenDir/DVDReadDir, as pc_stage.cpp's stage-sweep does) is
//    read into a private test heap and mounted there (JKRMemArchive::mountFixed, out of the volume
//    list, so the game's mounted Stage.arc is never touched), a few archives a frame. Its
//    event_list.dat, raw:
//      - reads (EvsList::read) and serializes back byte-identical (EvsList::serialize);
//      - has the size and FNV-1a 64 of fixtures/R3_14_wwrando_disc.txt (R3_14_make_fixture.py
//        --disc: wwrando's own read + save_changes of that file, hashes only);
//      - after editDisc() (one new event PcrEvserTest with actor Link and two actions with int, int
//        list, vec3 and string properties; one action PCR_ADD inserted before the last action of
//        the first event's first actor) serializes to the size and hash wwrando's EventList gives
//        after the same edits;
//      - that output reads back and serializes to itself.
//    Every stage of the fixture must be seen (154 files) and no Stage.arc may hold an event list the
//    fixture lacks.
// 2. With COS_RANDO_FORCE=1 (pc_rando() on), a .dat patcher registered from the first frame makes
//    the same editDisc() on Orichh's Stage.arc event_list.dat as dRes_info_c::loadResource hands it
//    over and writes it back through pc_rando_evser_write_res (it grows: a copy on the archive
//    heap). In play, dEvent_manager_c reads the new event the way the game does (getEventIdx,
//    staff, start cut, next cut, data and getSubstance): staff Link, cuts 001n (Timer 30) and 002n
//    (prm0 1 2, Pos, msg "evser"); the first event's first staff's cut chain has PCR_ADD before its
//    last cut; and the vanilla Gp1_Get_Itm actor 2 action 1 still reads 011get_item / prm0 0xEE.
//    Without COS_RANDO_FORCE the patcher must never run.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "features/rando/pc_rando_event_ser.h"
#include "features/rando/pc_rando_resources.h"

#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRDvdRipper.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRMemArchive.h"
#include "d/d_com_inf_game.h"
#include "d/d_event_data.h"
#include "d/d_event_manager.h"

#include <dolphin/dvd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

using pcr::EvsBytes;
using pcr::EvsList;
using pcr::EvsString;
using pcr::EvsVec;

constexpr const char* kName = "evser";
constexpr const char* kFixture = "R3_14_wwrando_disc.txt";
constexpr int kArchivesPerFrame = 6;
constexpr const char* kNewEvent = "PcrEvserTest";

struct DiscLine {
    char stage[16];
    uint32_t size, editSize;
    uint64_t fnv, editFnv;
    bool seen;
};

bool sStarted = false, sSweepDone = false, sForceChecked = false;
unsigned int sEndFrame = 0;
EvsVec<DiscLine> sLines;
EvsVec<EvsString> sStages; // stage directories with a Stage.arc
size_t sNext = 0;
JKRExpHeap* sHeap = nullptr;
int sFiles = 0, sRoundTrip = 0, sHashOk = 0, sEditOk = 0, sEditRound = 0, sUnknown = 0, sNoMount = 0;

// The patcher (COS_RANDO_FORCE): what it did to Orichh's file.
bool sRegistered = false;
int sPatchCalls = 0, sPatchSeen = 0, sPatchRead = -1, sPatchWrite = -1;
bool sPatchGrew = false;
EvsVec<EvsString> sFirstChain; // the first event's first actor's action names after the edit

// R3_14_make_fixture.py edit_disc(), in the same order.
bool editDisc(EvsList& el) {
    const int ev = el.addEvent(kNewEvent);
    const int link = el.addActor(ev, "Link");
    int c = el.addAction(link, "001n");
    if (c < 0 || el.addPropInt(c, "Timer", 30) < 0) {
        return false;
    }
    c = el.addAction(link, "002n");
    const int32_t prm0[2] = {1, 2};
    if (c < 0 || el.addPropInts(c, "prm0", prm0, 2) < 0 || el.addPropVec(c, "Pos", 1.0f, -2.5f, 3.0f) < 0 ||
        el.addPropString(c, "msg", "evser") < 0) {
        return false;
    }
    if (!el.events.empty() && !el.events[0].actors.empty()) {
        const int actor = el.events[0].actors[0];
        const int add = el.addAction(actor, "PCR_ADD");
        if (add < 0 || el.addPropInt(add, "Timer", 7) < 0) {
            return false;
        }
        auto& acts = el.actors[actor].actions;
        acts.pop_back(); // actions.remove(act) (appended last)
        acts.insert(acts.end() - 1, add);
    }
    return true;
}

// Candidate fixture folders, as smokes/pc_rando_smoke_wrap.cpp: COS_RANDO_FIXTURES, else
// native/check/rando/fixtures from this source file's path (also from the build folder when
// __FILE__ is relative).
bool readFixture(EvsString& text) {
    char dirs[2][1024];
    int nDirs = 0;
    const char* env = std::getenv("COS_RANDO_FIXTURES");
    if (env != nullptr && env[0] != '\0') {
        snprintf(dirs[nDirs++], sizeof dirs[0], "%s", env);
    } else {
        char file[1024];
        snprintf(file, sizeof file, "%s", __FILE__);
        for (int up = 0; up < 5; up++) {
            char* slash = strrchr(file, '/');
            if (slash == nullptr) {
                return false;
            }
            *slash = '\0';
        }
        snprintf(dirs[nDirs++], sizeof dirs[0], "%s/check/rando/fixtures", file);
        const char* run = std::getenv("COS_RUN_DIR");
        if (file[0] != '/' && run != nullptr && run[0] != '\0') {
            snprintf(dirs[nDirs++], sizeof dirs[0], "%s/../../%s/check/rando/fixtures", run, file);
        }
    }
    for (int i = 0; i < nDirs; i++) {
        char path[1200];
        snprintf(path, sizeof path, "%s/%s", dirs[i], kFixture);
        FILE* f = std::fopen(path, "rb");
        if (f == nullptr) {
            writef(STDERR_FILENO, "[cos] rando-smoke evser: cannot read %s\n", path);
            continue;
        }
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
            text.append(buf, n);
        }
        std::fclose(f);
        return true;
    }
    return false;
}

bool parseFixture() {
    EvsString text;
    if (!readFixture(text)) {
        return false;
    }
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == EvsString::npos) {
            eol = text.size();
        }
        const EvsString line = text.substr(pos, eol - pos);
        pos = eol + 1;
        DiscLine d = {};
        unsigned long long fnv = 0, editFnv = 0;
        unsigned size = 0, editSize = 0;
        if (sscanf(line.c_str(), "disc %15s %u %llx %u %llx", d.stage, &size, &fnv, &editSize, &editFnv) == 5) {
            d.size = size;
            d.editSize = editSize;
            d.fnv = fnv;
            d.editFnv = editFnv;
            sLines.push_back(d);
        }
    }
    return !sLines.empty();
}

DiscLine* lineFor(const char* stage) {
    for (DiscLine& d : sLines) {
        if (strcmp(d.stage, stage) == 0) {
            return &d;
        }
    }
    return nullptr;
}

void findStages() {
    DVDDir dir;
    if (!DVDOpenDir("/res/Stage", &dir)) {
        return;
    }
    DVDDirEntry entry;
    while (DVDReadDir(&dir, &entry)) {
        if (entry.name == nullptr || !entry.isDir) {
            continue;
        }
        char path[128];
        snprintf(path, sizeof path, "/res/Stage/%s/Stage.arc", entry.name);
        if (DVDConvertPathToEntrynum(path) >= 0) {
            sStages.push_back(EvsString(entry.name));
        }
    }
    DVDCloseDir(&dir);
}

// One stage's event_list.dat through the serializer.
void checkFile(const char* stage, const uint8_t* raw, size_t size) {
    sFiles++;
    DiscLine* d = lineFor(stage);
    if (d == nullptr) {
        sUnknown++;
        writef(STDERR_FILENO, "[cos] rando-smoke evser: %s: event_list.dat not in the fixture\n", stage);
        return;
    }
    d->seen = true;
    EvsList el;
    const int r = el.read(raw, size);
    EvsBytes out;
    const int s = r == PCR_EVL_OK ? el.serialize(out) : r;
    // The archive's file may be padded past the list's end; the list itself is the first out.size().
    const bool round = s == PCR_EVL_OK && out.size() <= size && memcmp(out.data(), raw, out.size()) == 0 &&
                       out.size() == d->size;
    sRoundTrip += round ? 1 : 0;
    const bool hash = out.size() == d->size && pcr::pc_rando_evser_fnv64(out.data(), out.size()) == d->fnv;
    sHashOk += hash ? 1 : 0;

    EvsBytes edited;
    const bool editOk = s == PCR_EVL_OK && editDisc(el) && el.serialize(edited) == PCR_EVL_OK &&
                        edited.size() == d->editSize &&
                        pcr::pc_rando_evser_fnv64(edited.data(), edited.size()) == d->editFnv;
    sEditOk += editOk ? 1 : 0;
    EvsList back;
    EvsBytes again;
    const bool editRound = editOk && back.read(edited.data(), edited.size()) == PCR_EVL_OK &&
                           back.serialize(again) == PCR_EVL_OK && again == edited &&
                           back.findEvent(kNewEvent) == (int)back.events.size() - 1;
    sEditRound += editRound ? 1 : 0;
    if (!round || !hash || !editOk || !editRound) {
        writef(STDERR_FILENO,
               "[cos] rando-smoke evser: %s: read %s, serialize %s, %zu bytes (file %zu, fixture %u): round-trip %d "
               "hash %d edit %d (%zu bytes, fixture %u) edit round-trip %d\n",
               stage, pc_rando_evl_result_name(r), pc_rando_evl_result_name(s), out.size(), size, d->size, round, hash,
               editOk, edited.size(), d->editSize, editRound);
    }
}

void sweepOne(const char* stage) {
    char path[128];
    snprintf(path, sizeof path, "/res/Stage/%s/Stage.arc", stage);
    void* buf = JKRDvdRipper::loadToMainRAM(path, nullptr, EXPAND_SWITCH_UNKNOWN1, 0, sHeap,
                                            JKRDvdRipper::ALLOC_DIRECTION_FORWARD, 0, nullptr);
    JKRMemArchive* arc = buf != nullptr ? new (sHeap, 0) JKRMemArchive() : nullptr;
    if (arc != nullptr && !arc->mountFixed(buf, JKRMEMBREAK_FLAG_UNKNOWN0)) {
        arc = nullptr;
    }
    if (arc == nullptr) {
        sNoMount++;
        writef(STDERR_FILENO, "[cos] rando-smoke evser: %s: cannot mount a private copy\n", path);
        sHeap->freeAll();
        return;
    }
    // Out of the volume list: name lookups keep going to the game's mounts.
    for (JSULink<JKRFileLoader>* l = JKRFileLoader::getVolumeList().getFirst(); l != nullptr; l = l->getNext()) {
        if (l->getObject() == arc) {
            JKRFileLoader::getVolumeList().remove(l);
            break;
        }
    }
    void* res = arc->getResource(0, "event_list.dat");
    if (res != nullptr) {
        checkFile(stage, (const uint8_t*)res, arc->getResSize(res));
    }
    arc->unmountFixed();
    sHeap->freeAll();
}

// --- the patcher (COS_RANDO_FORCE) ---

void patcher(PcRandoResFile* f) {
    sPatchCalls++;
    if (strcmp(f->stage, "Orichh") != 0 || strcmp(f->arc, "Stage") != 0 || strcmp(f->name, "event_list.dat") != 0) {
        return;
    }
    sPatchSeen++;
    EvsList el;
    sPatchRead = el.read(f->data, f->size);
    if (sPatchRead != PCR_EVL_OK) {
        return;
    }
    if (!editDisc(el)) {
        sPatchWrite = PCR_EVL_BAD_RANGE;
        return;
    }
    sFirstChain.clear();
    if (!el.events.empty() && !el.events[0].actors.empty()) {
        for (int c : el.actors[el.events[0].actors[0]].actions) {
            sFirstChain.push_back(el.actions[c].name.str());
        }
    }
    const size_t before = f->size;
    sPatchWrite = pcr::pc_rando_evser_write_res(f, el);
    sPatchGrew = f->replaced != 0 && f->size > before;
    writef(STDERR_FILENO, "[cos] rando-smoke evser: patcher: %s/%s/%s %zu -> %zu bytes (%s), %zu events\n", f->stage,
           f->arc, f->name, before, f->size, pc_rando_evl_result_name(sPatchWrite), el.events.size());
}

// The cut names of a staff's chain, and each cut's first data, through dEvDtBase_c.
bool staffChain(dEvDtBase_c& list, int staffIdx, EvsVec<dEvDtCut_c*>& cuts) {
    cuts.clear();
    s32 cut = (s32)list.getStaffP(staffIdx)->getStartCut();
    while (cut >= 0) {
        if (cut >= list.getCutNum() || cuts.size() > 256) {
            return false;
        }
        dEvDtCut_c* c = list.getCutP(cut);
        cuts.push_back(c);
        cut = c->getNext();
    }
    return true;
}

dEvDtData_c* cutData(dEvDtBase_c& list, dEvDtCut_c* c, const char* name) {
    s32 d = (s32)c->getDataTop();
    for (int guard = 0; d >= 0 && d < list.getDataNum() && guard < 64; guard++) {
        dEvDtData_c* p = list.getDataP(d);
        if (strcmp(p->getName(), name) == 0) {
            return p;
        }
        d = p->getNext();
    }
    return nullptr;
}

void checkForced() {
    randoSmokeCheck(kName, "patcher saw Orichh's Stage event_list.dat once", sPatchSeen == 1);
    randoSmokeCheck(kName, "patcher read it (raw, not swapped)", sPatchRead == PCR_EVL_OK);
    randoSmokeCheck(kName, "patcher wrote it into a grown copy", sPatchWrite == PCR_EVL_OK && sPatchGrew);

    dEvent_manager_c* mgr = dComIfGp_getPEvtManager();
    dEvDtBase_c& list = mgr->mList;
    const s16 idx = mgr->getEventIdx(kNewEvent, 0xFF);
    dEvDtEvent_c* ev = idx >= 0 ? mgr->getEventData(idx) : nullptr;
    randoSmokeCheck(kName, "dEvent_manager_c finds PcrEvserTest (last event)",
                    ev != nullptr && idx == list.getEventNum() - 1);
    if (ev == nullptr) {
        return;
    }
    bool staffOk = ev->getNStaff() == 1;
    EvsVec<dEvDtCut_c*> cuts;
    staffOk = staffOk && strcmp(list.getStaffP(ev->getStaff(0))->getName(), "Link") == 0 &&
              staffChain(list, ev->getStaff(0), cuts) && cuts.size() == 2;
    randoSmokeCheck(kName, "PcrEvserTest: staff Link with 2 cuts", staffOk);
    if (!staffOk) {
        return;
    }
    dEvDtData_c* timer = cutData(list, cuts[0], "Timer");
    int* tv = timer != nullptr ? (int*)mgr->getSubstance(timer, dEvDtData_c::TYPE_INT) : nullptr;
    randoSmokeCheck(kName, "cut 001n: Timer 30",
                    strcmp(cuts[0]->getName(), "001n") == 0 && tv != nullptr && *tv == 30);
    dEvDtData_c* prm0 = cutData(list, cuts[1], "prm0");
    int* pv = prm0 != nullptr ? (int*)mgr->getSubstance(prm0, dEvDtData_c::TYPE_INT) : nullptr;
    dEvDtData_c* pos = cutData(list, cuts[1], "Pos");
    float* fv = pos != nullptr ? (float*)mgr->getSubstance(pos, dEvDtData_c::TYPE_VEC) : nullptr;
    dEvDtData_c* msg = cutData(list, cuts[1], "msg");
    char* sv = msg != nullptr ? (char*)mgr->getSubstance(msg, dEvDtData_c::TYPE_STRING) : nullptr;
    randoSmokeCheck(kName, "cut 002n: prm0 1 2, Pos (1, -2.5, 3), msg evser",
                    strcmp(cuts[1]->getName(), "002n") == 0 && pv != nullptr && pv[0] == 1 && pv[1] == 2 &&
                        fv != nullptr && fv[0] == 1.0f && fv[1] == -2.5f && fv[2] == 3.0f && sv != nullptr &&
                        strcmp(sv, "evser") == 0);

    // The first event's first staff: the chain the patcher built, PCR_ADD before the last cut.
    dEvDtEvent_c* first = list.getEventP(0);
    bool chainOk = first->getNStaff() > 0 && staffChain(list, first->getStaff(0), cuts) &&
                   cuts.size() == sFirstChain.size() && cuts.size() >= 2 &&
                   strcmp(cuts[cuts.size() - 2]->getName(), "PCR_ADD") == 0;
    for (size_t i = 0; chainOk && i < cuts.size(); i++) {
        chainOk = sFirstChain[i] == cuts[i]->getName();
    }
    randoSmokeCheck(kName, "first event's first staff: the edited chain with PCR_ADD", chainOk);

    // The vanilla Gp1_Get_Itm actor 2 action 1 (R0.11's location) after the file moved.
    const s16 gi = mgr->getEventIdx("Gp1_Get_Itm", 0xFF);
    dEvDtEvent_c* gp = gi >= 0 ? mgr->getEventData(gi) : nullptr;
    bool gpOk = gp != nullptr && gp->getNStaff() > 2 && staffChain(list, gp->getStaff(2), cuts) && cuts.size() > 1;
    int* gv = nullptr;
    if (gpOk) {
        dEvDtData_c* d = cutData(list, cuts[1], "prm0");
        gv = d != nullptr ? (int*)mgr->getSubstance(d, dEvDtData_c::TYPE_INT) : nullptr;
    }
    randoSmokeCheck(kName, "Gp1_Get_Itm actor 2 action 1 still 011get_item / prm0 0xEE",
                    gpOk && strcmp(cuts[1]->getName(), "011get_item") == 0 && gv != nullptr && *gv == 0xEE);
}

} // namespace

void randoSmokeEvser(unsigned int frames) {
    if (!sRegistered) {
        sRegistered = true;
        pc_rando_register_res_patcher(patcher, PCR_RES_DAT);
        writef(STDERR_FILENO, "[cos] rando-smoke evser: pc_rando() %d\n", pc_rando());
    }
    if (!randoSmokeInPlay(10)) {
        return;
    }
    if (sEndFrame != 0) {
        if (frames >= sEndFrame) {
            randoSmokeEnd(kName);
        }
        return;
    }
    if (!sForceChecked) {
        sForceChecked = true;
        if (pc_rando()) {
            checkForced();
        } else {
            randoSmokeCheck(kName, "vanilla: the patcher never ran", sPatchCalls == 0 && sPatchSeen == 0);
        }
    }
    if (!sStarted) {
        sStarted = true;
        const bool fixture = parseFixture();
        randoSmokeCheck(kName, "fixture R3_14_wwrando_disc.txt read", fixture);
        findStages();
        for (u32 size = 32u << 20; sHeap == nullptr && size >= (4u << 20); size >>= 1) {
            sHeap = JKRExpHeap::create(size, JKRHeap::getRootHeap(), false);
        }
        if (!randoSmokeCheck(kName, "test heap", sHeap != nullptr) || !fixture) {
            sEndFrame = frames + 1;
            return;
        }
        writef(STDERR_FILENO, "[cos] rando-smoke evser: %zu stages with a Stage.arc, %zu fixture files, heap %u\n",
               sStages.size(), sLines.size(), (unsigned)sHeap->getTotalFreeSize());
        return;
    }
    if (!sSweepDone) {
        for (int i = 0; i < kArchivesPerFrame && sNext < sStages.size(); i++) {
            sweepOne(sStages[sNext++].c_str());
        }
        if (sNext < sStages.size()) {
            return;
        }
        sSweepDone = true;
        int missing = 0;
        for (const DiscLine& d : sLines) {
            if (!d.seen) {
                missing++;
                writef(STDERR_FILENO, "[cos] rando-smoke evser: %s: fixture file not found on the disc\n", d.stage);
            }
        }
        writef(STDERR_FILENO,
               "[cos] rando-smoke evser: %d event_list.dat files: %d round-trip, %d wwrando hash, %d edited = wwrando, "
               "%d edited round-trip; %d not in the fixture, %d fixture files missing, %d mount failures\n",
               sFiles, sRoundTrip, sHashOk, sEditOk, sEditRound, sUnknown, missing, sNoMount);
        randoSmokeCheck(kName, "every fixture stage's event_list.dat found (no extra, no mount failure)",
                        sFiles == (int)sLines.size() && sUnknown == 0 && missing == 0 && sNoMount == 0);
        randoSmokeCheck(kName, "every file reads and serializes byte-identical", sRoundTrip == sFiles && sFiles > 0);
        randoSmokeCheck(kName, "every file matches wwrando's save (size, FNV-1a 64)", sHashOk == sFiles);
        randoSmokeCheck(kName, "add event + add action: every file equals wwrando's save", sEditOk == sFiles);
        randoSmokeCheck(kName, "every edited file reads and serializes to itself", sEditRound == sFiles);
        if (sHeap != nullptr) {
            sHeap->destroy();
            sHeap = nullptr;
        }
        sEndFrame = frames + 30; // the game runs on
    }
}

} // namespace pc
