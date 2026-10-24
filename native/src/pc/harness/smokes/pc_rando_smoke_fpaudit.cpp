// COS_SMOKE=rando COS_RANDO_SMOKE=fpaudit (docs/RANDOMIZER_PLAN.md step R7.6b): the fingerprint audit of
// every dzx and event patcher on the disc the game runs (USA or EUR), run as
//   run.sh rando --stage sea:44:0 --env COS_RANDO_SMOKE=fpaudit --env COS_RANDO_PLACEMENT=<placement>
// (native/tools/rando/fp_audit.py builds the placement, runs it on both regions and reads the log).
//
// Once the play scene has run 30 frames, every archive of every /res/Stage/<stage>/ directory of the
// disc (Stage.arc and Room<N>.arc, the FST walked with DVDOpenDir/DVDReadDir) is read into a private
// test heap and mounted there, out of the volume list (as smokes/pc_rando_smoke_evser.cpp does), a
// few archives a frame. Its stage.dzs, room.dzr and event_list.dat, copied out raw, go through the
// archive hook's whole patcher chain (pc_rando_res_dispatch, every patcher of pc_rando_patchers.def
// with the active seed), with a malloc allocator for grown copies, exactly as
// dRes_info_c::loadResource hands them over. Every patcher checks its own fingerprints (D9) and logs
// a mismatch, a missing entry or a refusal ("... left vanilla"); the smoke prints
//     [cos] rando-smoke fpaudit: file <stage>/<arc>/<name> <size> -> <size>
// before each file, so fp_audit.py can tell which file a patcher line belongs to, and
//     [cos] rando-smoke fpaudit: begin / end
// around the sweep. Room.dzb is not part of it: its patcher (R3.15's FF door) runs on the file after
// cBgS::ConvDzb, not on disc bytes. J3D and particle files are not stage data.
// Checks: pc_rando() is on (a placement was given), the test heap exists, at least one Stage.arc,
// every archive mounts, every stage directory with a Stage.arc gives a stage.dzs, and the patcher
// chain ran on every file (the dispatcher's counter). Exit 0 when they hold; the audit's result is
// fp_audit.py's (no patcher line with a mismatch or a refusal, or only those the USA disc has too).
#include "pc_rando_smokes.h"

#include "features/rando/pc_rando_resources.h"
#include "pc/pc_rando.h"

#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRDvdRipper.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRMemArchive.h"

#include <dolphin/dvd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "fpaudit";
constexpr int kArchivesPerFrame = 4;
const char* const kFiles[] = {"stage.dzs", "room.dzr", "event_list.dat"};

struct Arc {
    std::string stage;
    std::string arc; // "Stage", "Room12"
};

bool sStarted = false, sDone = false;
unsigned int sEndFrame = 0;
std::vector<Arc> sArcs;
size_t sNext = 0;
JKRExpHeap* sHeap = nullptr;
int sStages = 0, sStageDzs = 0, sFilesSeen = 0, sNoMount = 0;
std::string sNoDzs; // stages whose Stage.arc holds no stage.dzs
int sCount[3] = {};
uint32_t sDispatched0 = 0;

bool isRoomArc(const char* name) {
    if (strncmp(name, "Room", 4) != 0) {
        return false;
    }
    const char* p = name + 4;
    if (*p < '0' || *p > '9') {
        return false;
    }
    while (*p >= '0' && *p <= '9') {
        p++;
    }
    return strcmp(p, ".arc") == 0;
}

void findArcs() {
    DVDDir root;
    if (!DVDOpenDir("/res/Stage", &root)) {
        return;
    }
    DVDDirEntry entry;
    std::vector<std::string> stages;
    while (DVDReadDir(&root, &entry)) {
        if (entry.name != nullptr && entry.isDir) {
            stages.push_back(entry.name);
        }
    }
    DVDCloseDir(&root);
    for (const std::string& stage : stages) {
        char path[128];
        snprintf(path, sizeof path, "/res/Stage/%s", stage.c_str());
        DVDDir dir;
        if (!DVDOpenDir(path, &dir)) {
            continue;
        }
        bool hasStage = false;
        std::vector<Arc> rooms;
        while (DVDReadDir(&dir, &entry)) {
            if (entry.name == nullptr || entry.isDir) {
                continue;
            }
            if (strcmp(entry.name, "Stage.arc") == 0) {
                hasStage = true;
            } else if (isRoomArc(entry.name)) {
                rooms.push_back({stage, std::string(entry.name, strlen(entry.name) - 4)});
            }
        }
        DVDCloseDir(&dir);
        if (hasStage) {
            sStages++;
            sArcs.push_back({stage, "Stage"});
        }
        sArcs.insert(sArcs.end(), rooms.begin(), rooms.end());
    }
}

void* mallocAlloc(size_t size, void*) {
    return calloc(1, size);
}

// One file through the chain, on a copy (the mounted archive's bytes stay the disc's).
void auditFile(const Arc& a, int which, const uint8_t* raw, size_t size) {
    std::vector<uint8_t> copy(raw, raw + size);
    PcRandoResFile file = {};
    PcRandoResNames names = {};
    char dir[PCR_RES_DIR_MAX];
    snprintf(dir, sizeof dir, "/res/Stage/%s/", a.stage.c_str());
    pc_rando_res_describe(&file, &names, a.arc.c_str(), dir);
    file.name = kFiles[which];
    file.type = which == 0 ? PCR_RES_DZS : which == 1 ? PCR_RES_DZR : PCR_RES_DAT;
    file.data = copy.data();
    file.size = copy.size();
    file.alloc = mallocAlloc;
    writef(STDERR_FILENO, "[cos] rando-smoke %s: file %s/%s/%s %zu\n", kName, a.stage.c_str(), a.arc.c_str(), kFiles[which], size);
    pc_rando_res_dispatch(&file);
    if (file.size != size || file.replaced) {
        writef(STDERR_FILENO, "[cos] rando-smoke %s:   %zu -> %zu bytes%s\n", kName, size, file.size, file.replaced ? " (copy)" : "");
    }
    // Grown copies of earlier patchers in the chain are not tracked; the last one is freed here.
    if (file.data != copy.data()) {
        free(file.data);
    }
    sFilesSeen++;
    sCount[which]++;
    if (which == 0) {
        sStageDzs++;
    }
}

void auditArc(const Arc& a) {
    char path[128];
    snprintf(path, sizeof path, "/res/Stage/%s/%s.arc", a.stage.c_str(), a.arc.c_str());
    void* buf = JKRDvdRipper::loadToMainRAM(path, nullptr, EXPAND_SWITCH_UNKNOWN1, 0, sHeap,
                                            JKRDvdRipper::ALLOC_DIRECTION_FORWARD, 0, nullptr);
    JKRMemArchive* arc = buf != nullptr ? new (sHeap, 0) JKRMemArchive() : nullptr;
    if (arc != nullptr && !arc->mountFixed(buf, JKRMEMBREAK_FLAG_UNKNOWN0)) {
        arc = nullptr;
    }
    if (arc == nullptr) {
        sNoMount++;
        writef(STDERR_FILENO, "[cos] rando-smoke %s: %s: cannot mount a private copy\n", kName, path);
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
    for (int which = 0; which < 3; which++) {
        void* res = arc->getResource(0, kFiles[which]);
        if (res != nullptr) {
            auditFile(a, which, (const uint8_t*)res, arc->getResSize(res));
        } else if (which == 0 && a.arc == "Stage") {
            sNoDzs += (sNoDzs.empty() ? "" : " ") + a.stage;
        }
    }
    arc->unmountFixed();
    sHeap->freeAll();
}

} // namespace

void randoSmokeFpaudit(unsigned int frames) {
    if (!randoSmokeInPlay(30)) {
        return;
    }
    if (sEndFrame != 0) {
        if (frames >= sEndFrame) {
            randoSmokeEnd(kName);
        }
        return;
    }
    if (!sStarted) {
        sStarted = true;
        const bool on = randoSmokeCheck(kName, "pc_rando() on (a placement was given)", pc_rando() != 0);
        findArcs();
        for (u32 size = 32u << 20; sHeap == nullptr && size >= (8u << 20); size >>= 1) {
            sHeap = JKRExpHeap::create(size, JKRHeap::getRootHeap(), false);
        }
        const bool heap = randoSmokeCheck(kName, "test heap", sHeap != nullptr);
        const bool found = randoSmokeCheck(kName, "stage archives found on the disc", sStages > 0);
        if (!on || !heap || !found) {
            sEndFrame = frames + 1;
            return;
        }
        PcRandoResStats st;
        pc_rando_res_stats(&st);
        sDispatched0 = st.dispatched;
        writef(STDERR_FILENO, "[cos] rando-smoke %s: begin: %d stages with a Stage.arc, %zu archives, heap %u\n", kName, sStages,
             sArcs.size(), (unsigned)sHeap->getTotalFreeSize());
        return;
    }
    if (!sDone) {
        for (int i = 0; i < kArchivesPerFrame && sNext < sArcs.size(); i++) {
            auditArc(sArcs[sNext++]);
        }
        if (sNext < sArcs.size()) {
            return;
        }
        sDone = true;
        PcRandoResStats after;
        pc_rando_res_stats(&after);
        writef(STDERR_FILENO, "[cos] rando-smoke %s: end: %d files (%d stage.dzs, %d room.dzr, %d event_list.dat), %d mount failures\n",
             kName, sFilesSeen, sCount[0], sCount[1], sCount[2], sNoMount);
        randoSmokeCheck(kName, "every archive mounts", sNoMount == 0);
        writef(STDERR_FILENO, "[cos] rando-smoke %s: Stage.arc without a stage.dzs: %s\n", kName,
               sNoDzs.empty() ? "none" : sNoDzs.c_str());
        // Every Stage.arc the game boots holds a stage.dzs; the disc has one that holds none.
        randoSmokeCheck(kName, "stage.dzs files found (at most one Stage.arc without)",
                        sStageDzs > 0 && sStages - sStageDzs <= 1);
        // The game's own loads in between are counted too, so at least one dispatch per file.
        randoSmokeCheck(kName, "the patcher chain ran on every file",
                        after.dispatched - sDispatched0 >= (uint32_t)sFilesSeen && sFilesSeen > 0);
        if (sHeap != nullptr) {
            sHeap->destroy();
            sHeap = nullptr;
        }
        sEndFrame = frames + 10;
    }
}

} // namespace pc
