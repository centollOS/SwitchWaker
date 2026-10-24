// COS_SMOKE=rando COS_RANDO_SMOKE=idhash (docs/RANDOMIZER_PLAN.md step R9.2, Milestone R2 "identity is
// vanilla"): hashes every file of every archive the game loads through dRes_info_c::loadResource, as
// the archive-load hook (R0.10, features/rando/pc_rando_resources.h) sees it, before any patcher and
// after each step's patchers, e.g.
//   run.sh rando --stage Asoko:0:0:2 --env COS_RANDO_SMOKE=idhash \
//          --env COS_RANDO_PLACEMENT=native/check/rando/fixtures/R0_6_identity.json
// native/tools/rando/identity_check.py runs it and judges the result against the disc.
//
// How the hashes are taken. On its first frame (before the game's first dispatch: the game registers
// the patchers lazily, at the first archive loaded under pc_rando()), the smoke registers observer 0
// as the first patcher of both lists (pc_rando_register_res_patcher, PCR_RES_ANY, and the raw J3D
// list), then calls each init function of features/rando/pc_rando_patchers.def in the game's order,
// registering observer N right after init N. The dispatcher runs patchers in registration order, so
// observer N sees the bytes after every patcher of init N. A step that changed a file's bytes (MD5 or
// size) is named on the file's line. When the sweep starts, a tail observer is moved to the end of
// both lists, so a patcher registered later (on activation) is caught too ("late").
//
// The sweep. Once the play scene has run 10 frames, every .arc under /res/Stage/<stage>/ (Stage.arc,
// RoomN.arc, ...) and /res/Object/ is loaded through dRes_control_c::setRes into the smoke's own
// dRes_info_c slots (directory /res/Stage/<stage>/ or /res/Object/, as setStageRes / setObjectRes
// build it, so the hook records the same directory as the game), four at a time, synced and deleted
// again. The boot stage's own loads are recorded as well.
//   COS_RANDO_IDHASH_ONLY=<a,b,...>  only archives whose path below /res/ starts with one of these
//                                    (e.g. Stage/Asoko/,Stage/sea/Room44.arc,Object/Nzg.arc)
//   COS_RANDO_IDHASH_OUT             the report (default $COS_RUN_DIR/idhash.txt)
//
// The report:
//   step <n> <init function>                   (n = 1.. in pc_rando_patchers.def order; late = after them)
//   arc <path> loaded|failed
//   file res|raw <dir> <arc> <name> <type> <size> <md5> <final size> <final md5> <steps that changed it|->
// one "file" line per dispatch (res: the post-load list, raw: the J3D list before the loaders).
// Checks: observer 0 was registered first (no dispatch before the smoke), the report was written,
// every archive loaded, and under pc_rando() files were dispatched (without pc_rando() none were).
// Exit 0 when every check held, else 1. The verdict on the hashes is identity_check.py's.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "features/rando/pc_rando_options.h"
#include "features/rando/pc_rando_resources.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"

#include "JSystem/JKernel/JKRHeap.h"
#include "d/d_com_inf_game.h"
#include "d/d_resorce.h"
#include <dolphin/dvd.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <strings.h>
#include <unistd.h>
#include <utility>
#include <vector>

#define PC_RANDO_PATCHER(fn) extern "C" void fn(void);
#include "features/rando/pc_rando_patchers.def"
#undef PC_RANDO_PATCHER

namespace pc {

namespace {

constexpr const char* kName = "idhash";
constexpr size_t kBatch = 4;
constexpr unsigned kMaxWait = 900;
constexpr int kMaxObservers = 64;

void (*const kInits[])(void) = {
#define PC_RANDO_PATCHER(fn) fn,
#include "features/rando/pc_rando_patchers.def"
#undef PC_RANDO_PATCHER
};
const char* const kInitNames[] = {
#define PC_RANDO_PATCHER(fn) #fn,
#include "features/rando/pc_rando_patchers.def"
#undef PC_RANDO_PATCHER
};
constexpr int kSteps = (int)(sizeof kInits / sizeof kInits[0]);
constexpr int kLate = kSteps + 1; // the tail observer's index
static_assert(kLate < kMaxObservers, "more patcher steps than observers");

struct Rec {
    bool raw;
    std::string dir, arc, name;
    uint32_t type;
    size_t size0, sizeF;
    std::string md50, md5F;
    std::vector<int> steps;
};

struct Arc {
    std::string path; // "/res/Stage/Asoko/Room0.arc"
    std::string dir;  // "/res/Stage/Asoko/"
    std::string name; // "Room0"
    bool stage;
};

enum State { kInit, kWaitPlay, kLoad, kWait, kDone };
State sState = kInit;
std::vector<Rec> sRecs;
int sCur[2] = {-1, -1};      // the record of the file each list is dispatching
std::string sPrev[2];        // its MD5 at the previous observer
size_t sPrevSize[2] = {0, 0};
std::vector<Arc> sArcs;
size_t sNext = 0;
std::vector<size_t> sBatch;
unsigned sWait = 0;
int sLoaded = 0, sFailed = 0;
bool sFirst = false;
std::vector<std::pair<std::string, bool>> sArcResults;
dRes_info_c sInfos[kBatch];

void cosLogf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void cosLogf(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

std::string md5Hex(const uint8_t* p, size_t n) {
    uint8_t d[16];
    pcr::md5(p, n, d);
    char s[33];
    for (int i = 0; i < 16; i++) {
        snprintf(s + i * 2, 3, "%02x", d[i]);
    }
    return std::string(s, 32);
}

std::string typeStr(uint32_t t) {
    char s[5];
    for (int i = 0; i < 4; i++) {
        const char c = (char)((t >> (24 - 8 * i)) & 0xFF);
        s[i] = (c > ' ' && c < 0x7F) ? c : '_';
    }
    s[4] = '\0';
    return s;
}

std::string field(const char* s) {
    if (s == nullptr || s[0] == '\0') {
        return "-";
    }
    std::string out(s);
    for (char& c : out) {
        if (c == ' ' || c == '\t' || c == '\n') {
            c = '_';
        }
    }
    return out;
}

void observe(int list, int n, PcRandoResFile* f) {
    const std::string h = md5Hex(f->data, f->size);
    if (n == 0) {
        Rec r;
        r.raw = list == 1;
        r.dir = field(f->dir);
        r.arc = field(f->arc);
        r.name = field(f->name);
        r.type = f->type;
        r.size0 = r.sizeF = f->size;
        r.md50 = r.md5F = h;
        sRecs.push_back(std::move(r));
        sCur[list] = (int)sRecs.size() - 1;
        sPrev[list] = h;
        sPrevSize[list] = f->size;
        return;
    }
    if (sCur[list] < 0) {
        return;
    }
    Rec& r = sRecs[(size_t)sCur[list]];
    if (h != sPrev[list] || f->size != sPrevSize[list]) {
        r.steps.push_back(n);
    }
    sPrev[list] = h;
    sPrevSize[list] = f->size;
    r.md5F = h;
    r.sizeF = f->size;
}

template <int N> void obsRes(PcRandoResFile* f) {
    observe(0, N, f);
}
template <int N> void obsRaw(PcRandoResFile* f) {
    observe(1, N, f);
}

template <int... I> constexpr auto makeTable(std::integer_sequence<int, I...>, int list) {
    struct T {
        PcRandoResPatcher fn[sizeof...(I)];
    };
    return list == 0 ? T{{obsRes<I>...}} : T{{obsRaw<I>...}};
}
const auto kObsRes = makeTable(std::make_integer_sequence<int, kMaxObservers>{}, 0);
const auto kObsRaw = makeTable(std::make_integer_sequence<int, kMaxObservers>{}, 1);

void addObservers(int n) {
    pc_rando_register_res_patcher(kObsRes.fn[n], PCR_RES_ANY);
    pc_rando_register_raw_patcher(kObsRaw.fn[n], PCR_RES_J3D);
}

void findArchives(const std::string& dirPath, int depth, bool stage) {
    DVDDir dir;
    if (!DVDOpenDir(dirPath.c_str(), &dir)) {
        cosLogf("[cos] rando-smoke idhash: DVDOpenDir %s failed\n", dirPath.c_str());
        sFailed++;
        return;
    }
    DVDDirEntry entry;
    std::vector<std::string> subdirs;
    while (DVDReadDir(&dir, &entry)) {
        if (entry.name == nullptr) {
            continue;
        }
        const std::string child = dirPath + "/" + entry.name;
        if (entry.isDir) {
            subdirs.push_back(child);
            continue;
        }
        const size_t len = child.size();
        if (len > 4 && strcasecmp(child.c_str() + len - 4, ".arc") == 0) {
            Arc a;
            a.path = child;
            a.dir = dirPath + "/";
            a.name = std::string(entry.name, strlen(entry.name) - 4);
            a.stage = stage;
            sArcs.push_back(a);
        }
    }
    DVDCloseDir(&dir);
    if (depth < 1) {
        for (const std::string& d : subdirs) {
            findArchives(d, depth + 1, stage);
        }
    }
}

bool wanted(const Arc& a) {
    const char* only = getenv("COS_RANDO_IDHASH_ONLY");
    if (only == nullptr || only[0] == '\0') {
        return true;
    }
    const std::string rel = a.path.substr(strlen("/res/"));
    std::string list(only);
    size_t pos = 0;
    while (pos <= list.size()) {
        size_t comma = list.find(',', pos);
        if (comma == std::string::npos) {
            comma = list.size();
        }
        const std::string p = list.substr(pos, comma - pos);
        if (!p.empty() && rel.compare(0, p.size(), p) == 0) {
            return true;
        }
        pos = comma + 1;
    }
    return false;
}

void buildList() {
    findArchives("/res/Stage", 0, true);
    findArchives("/res/Object", 1, false); // the archives directly in /res/Object only
    std::vector<Arc> keep;
    for (const Arc& a : sArcs) {
        if (wanted(a)) {
            keep.push_back(a);
        }
    }
    sArcs.swap(keep);
}

// dRes_control_c::setRes finds a slot by archive name: a batch never holds two archives of one name.
bool nameInBatch(const std::string& name) {
    for (size_t i : sBatch) {
        if (strcasecmp(sArcs[i].name.c_str(), name.c_str()) == 0) {
            return true;
        }
    }
    return false;
}

void finish() {
    JKRPcHostAllocScope host;
    // Move the tail observer to the end once more: nothing may be registered after the sweep anyway.
    const char* out = getenv("COS_RANDO_IDHASH_OUT");
    char buf[1024];
    if (out == nullptr || out[0] == '\0') {
        const char* run = getenv("COS_RUN_DIR");
        if (run != nullptr && run[0] != '\0') {
            snprintf(buf, sizeof buf, "%s/idhash.txt", run);
            out = buf;
        }
    }
    FILE* f = out != nullptr ? fopen(out, "w") : nullptr;
    int changed = 0, rawN = 0;
    std::vector<int> perStep(kLate + 1, 0);
    for (const Rec& r : sRecs) {
        rawN += r.raw ? 1 : 0;
        if (!r.steps.empty()) {
            changed++;
        }
        for (int s : r.steps) {
            perStep[(size_t)s]++;
        }
    }
    if (f != nullptr) {
        fprintf(f, "# rando-smoke idhash (docs/RANDOMIZER_PLAN.md R9.2): pc_rando() %d, seed %s, boot stage %s\n",
                pc_rando(), pc_rando_seed() != nullptr ? "active" : "none", dComIfGp_getStartStageName());
        for (int i = 0; i < kSteps; i++) {
            fprintf(f, "step %d %s\n", i + 1, kInitNames[i]);
        }
        fprintf(f, "step %d late\n", kLate);
        for (const auto& a : sArcResults) {
            fprintf(f, "arc %s %s\n", a.first.c_str(), a.second ? "loaded" : "failed");
        }
        for (const Rec& r : sRecs) {
            std::string steps;
            for (int s : r.steps) {
                if (!steps.empty()) {
                    steps += ",";
                }
                steps += std::to_string(s);
            }
            fprintf(f, "file %s %s %s %s %s %zu %s %zu %s %s\n", r.raw ? "raw" : "res", r.dir.c_str(), r.arc.c_str(),
                    r.name.c_str(), typeStr(r.type).c_str(), r.size0, r.md50.c_str(), r.sizeF, r.md5F.c_str(),
                    steps.empty() ? "-" : steps.c_str());
        }
        fclose(f);
        cosLogf("[cos] rando-smoke idhash: report %s\n", out);
    }
    cosLogf("[cos] rando-smoke idhash: pc_rando() %d; %d archives loaded, %d failed; %zu files dispatched (%d raw "
            "J3D), %d changed by a patcher\n",
            pc_rando(), sLoaded, sFailed, sRecs.size(), rawN, changed);
    for (int s = 1; s <= kLate; s++) {
        if (perStep[(size_t)s] > 0) {
            cosLogf("[cos] rando-smoke idhash:   step %d %s changed %d file(s)\n", s,
                    s == kLate ? "late" : kInitNames[s - 1], perStep[(size_t)s]);
        }
    }
    randoSmokeCheck(kName, "observer 0 registered before any patcher (no dispatch before the smoke)", sFirst);
    randoSmokeCheck(kName, "report written", f != nullptr);
    char what[160];
    snprintf(what, sizeof what, "every archive loaded (%d of %zu)", sLoaded, sArcs.size());
    randoSmokeCheck(kName, what, sFailed == 0 && sLoaded == (int)sArcs.size() && !sArcs.empty());
    if (pc_rando()) {
        randoSmokeCheck(kName, "pc_rando(): files were dispatched", !sRecs.empty());
    } else {
        randoSmokeCheck(kName, "vanilla: no file was dispatched", sRecs.empty());
    }
    sState = kDone;
    randoSmokeEnd(kName);
}

} // namespace

void randoSmokeIdhash(unsigned int frames) {
    (void)frames;
    JKRPcHostAllocScope host;
    switch (sState) {
    case kInit: {
        // Before the game's first dispatch: observer 0 first, then each step's patchers and its observer.
        sFirst = pc_rando_res_patcher_count() == 0 && pc_rando_raw_patcher_count() == 0;
        sRecs.reserve(8192);
        addObservers(0);
        for (int i = 0; i < kSteps; i++) {
            kInits[i]();
            addObservers(i + 1);
        }
        addObservers(kLate);
        cosLogf("[cos] rando-smoke idhash: %d steps observed (%d res, %d raw patchers in all)\n", kSteps,
                pc_rando_res_patcher_count(), pc_rando_raw_patcher_count());
        sState = kWaitPlay;
        return;
    }
    case kWaitPlay:
        if (!randoSmokeInPlay(10)) {
            return;
        }
        // The tail observer goes last again (patchers registered on activation come before it).
        pc_rando_unregister_res_patcher(kObsRes.fn[kLate]);
        pc_rando_unregister_raw_patcher(kObsRaw.fn[kLate]);
        addObservers(kLate);
        buildList();
        cosLogf("[cos] rando-smoke idhash: pc_rando() %d, stage %s, %zu archives to load\n", pc_rando(),
                dComIfGp_getStartStageName(), sArcs.size());
        sState = kLoad;
        return;
    case kLoad:
        sBatch.clear();
        while (sNext < sArcs.size() && sBatch.size() < kBatch && !nameInBatch(sArcs[sNext].name)) {
            const Arc& a = sArcs[sNext];
            if (dRes_control_c::setRes(a.name.c_str(), sInfos, (int)kBatch, a.dir.c_str(), a.stage ? 1 : 0,
                                       nullptr) != 1) {
                cosLogf("[cos] rando-smoke idhash: setRes %s failed\n", a.path.c_str());
                sFailed++;
                sArcResults.emplace_back(a.path, false);
                sNext++;
                continue;
            }
            sBatch.push_back(sNext++);
        }
        sWait = 0;
        sState = sBatch.empty() && sNext >= sArcs.size() ? kDone : kWait;
        if (sState == kDone) {
            finish();
        }
        return;
    case kWait: {
        bool pending = false;
        for (size_t i : sBatch) {
            if (dRes_control_c::syncRes(sArcs[i].name.c_str(), sInfos, (int)kBatch) > 0) {
                pending = true;
            }
        }
        if (pending && ++sWait < kMaxWait) {
            return;
        }
        for (size_t i : sBatch) {
            const Arc& a = sArcs[i];
            const bool ok = dRes_control_c::syncRes(a.name.c_str(), sInfos, (int)kBatch) == 0;
            if (ok) {
                sLoaded++;
            } else {
                cosLogf("[cos] rando-smoke idhash: %s did not load\n", a.path.c_str());
                sFailed++;
            }
            sArcResults.emplace_back(a.path, ok);
            dRes_control_c::deleteRes(a.name.c_str(), sInfos, (int)kBatch);
        }
        if (sNext < sArcs.size()) {
            sState = kLoad;
        } else {
            finish();
        }
        return;
    }
    case kDone:
        return;
    }
}

} // namespace pc
