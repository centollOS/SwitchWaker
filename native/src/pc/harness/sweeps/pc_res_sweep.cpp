// COS_SMOKE=res-sweep: draw every J3D model and every 2D screen of the disc once in a running PLAY
// scene, so Aurora records the pipelines their materials make (native/tools/gen_pipeline_cache.sh,
// tiers model-sweep and screen-sweep; docs/SWITCH_BUILD.md, "Pipeline precompile").
// With the debug stage boot (COS_BOOT_STAGE, e.g. sea:44:0), once the player is in the start room (the
// M12 probe, pc_outset.cpp) and kSettleFrames later, every .arc under /res (the FST walked as the
// j3d-sweep and blo-sweep smokes walk it) is taken in turn, one archive at a time:
// - mounted into this sweep's heap as the blo-sweep mounts it (in main RAM; with ARAM when files are
//   stored compressed), an archive inside it mounted in place (JKRMemArchive::mountFixed); one the
//   game has mounted (JKRArchive::mount would hand out the game's, whose BDL display lists its own
//   load patched already) is read into the heap and mounted in place as a private copy, its files
//   stored compressed left out;
// - models (COS_RES_SWEEP=models or all): every BMD and BDL loaded with the call and flags
//   dRes_info_c::loadResource uses for its directory (the j3d-sweep's plan; BMT material tables
//   are left out), then mDoExt_J3DModel__create(data, 0, 0x11020203) as most actors make theirs;
//   each model is drawn in front of the player for one frame lit as an actor (dKy_tevstr_c,
//   settingTevStruct(TEV_TYPE_ACTOR), setLightTevColorType, the actor draw lists) and one frame
//   lit as a room (TEV_TYPE_BG0, the BG draw lists): the opaque and translucent buckets are the
//   materials' own;
// - screens (COS_RES_SWEEP=screens or all): every BLO set with J2DScreen::set(<file>, archive), as
//   the game makes its screens, and drawn on the 2D list (dComIfGd_set2DOpa) for one frame, up to
//   kScreensPerFrame a frame;
// - then, once the frame that paints the last draw lists is over, everything freed (the archive
//   unmounted, the sweep heap emptied) before the next archive.
// The draws are made from the PLAY scene's draw (pc_play_draw, called by dScnPly_Draw after the
// actors' draws), so they go through the game's own draw lists and passes.
// COS_RES_SWEEP_SHARD=<k>/<n> takes the archives whose index modulo n is k (to run several
// processes); COS_RES_SWEEP_FROM=<index> starts at that archive index (to go on after a fault).
// <COS_RUN_DIR>/res_sweep.txt gets "begin <index> <archive>" before each archive (written straight
// to the file, so it names the archive when a fault ends the process) and "<index> <archive>
// models=N screens=M skipped=S" after; native/tools/gen_pipeline_cache.sh restarts a run after
// the archive that faulted. An archive or model the heap cannot hold is skipped and counted.
// Exit 0 when every archive was taken, 1 if the PLAY scene or the player went away.
#include "pc_internal.h"

#include "JSystem/JSystem.h" // IWYU pragma: keep

#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "JSystem/J3DGraphLoader/J3DModelLoader.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRDecomp.h"
#include "JSystem/JKernel/JKRDvdRipper.h"
#include "JSystem/JSupport/JSUMemoryStream.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRMemArchive.h"
#include "SSystem/SComponent/c_math.h"
#include "d/d_com_inf_game.h"
#include "d/d_drawlist.h"
#include "d/d_kankyo.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"

#include <dolphin/dvd.h>

#include "cos_sdk/host_alloc.h"

#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <unistd.h>

namespace pc {

namespace {

using String = cos_sdk::HostString;
template <class T>
using Vector = cos_sdk::HostVector<T>;

constexpr unsigned int kSettleFrames = 60;
constexpr int kScreensPerFrame = 12;
constexpr int kMaxModels = 512;   // per archive
constexpr int kMaxScreens = 256;  // per archive
constexpr float kSpawnDistance = 400.0f;

enum State { kOff, kWaitLink, kNext, kDraw, kDrain, kDone };
// Draw passes of an archive: 0 models as actors, 1 models as rooms, 2.. screen batches.

struct ScreenDraw : dDlst_base_c {
    J2DScreen* screen = nullptr;
    void draw() override {
        if (screen != nullptr) {
            screen->draw(0.0f, 0.0f, dComIfGp_getCurrentGrafPort());
        }
    }
};

State sState = kOff;
bool sChecked = false;
bool sDoModels = true, sDoScreens = true;
int sShard = 0, sShards = 1, sFrom = 0;
Vector<String> sArchives;
int sIndex = -1;
JKRExpHeap* sHeap = nullptr;
JKRArchive* sArc = nullptr;
JKRMemArchive* sCopy = nullptr; // a private copy of an archive the game has mounted
JKRMemArchive* sNested[16];
int sNumNested = 0;
J3DModel* sModels[kMaxModels];
int sNumModels = 0;
ScreenDraw sScreens[kMaxScreens];
int sNumScreens = 0;
int sPass = 0;
bool sDrawn = false; // pc_play_draw ran this pass
unsigned int sSince = 0;
unsigned int sArcSkipped = 0;
int sFd = -1;
unsigned int sTotalArchives = 0, sTotalModels = 0, sTotalScreens = 0, sTotalSkipped = 0,
             sMountFailed = 0;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

const char* sceneLost() {
    if (fpcM_Search(isPlayScene, nullptr) == nullptr) {
        return "PLAY scene gone";
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr || fopAcM_GetName(player) != fpcNm_PLAYER_e) {
        return "player gone";
    }
    return nullptr;
}

void findArchives(const String& dirPath, Vector<String>& out, int depth) {
    DVDDir dir;
    if (!DVDOpenDir(dirPath.c_str(), &dir)) {
        return;
    }
    DVDDirEntry entry;
    Vector<String> subdirs;
    while (DVDReadDir(&dir, &entry)) {
        if (entry.name == nullptr) {
            continue;
        }
        String child = dirPath + "/" + entry.name;
        if (entry.isDir) {
            subdirs.push_back(child);
            continue;
        }
        size_t len = child.size();
        if (len > 4 && strcasecmp(child.c_str() + len - 4, ".arc") == 0) {
            out.push_back(child);
        }
    }
    DVDCloseDir(&dir);
    if (depth < 6) {
        for (const String& d : subdirs) {
            findArchives(d, out, depth + 1);
        }
    }
}

bool hasSuffix(const String& s, const char* suffix) {
    size_t n = strlen(suffix);
    return s.size() > n && strcasecmp(s.c_str() + s.size() - n, suffix) == 0;
}

struct Entry {
    String path;
    uint32_t dirType;
};

void archiveEntries(JKRArchive* arc, uint32_t node, const String& prefix, int depth,
                    Vector<Entry>& out) {
    if (depth > 32 || node >= arc->mArcInfoBlock->num_nodes) {
        return;
    }
    JKRArchive::SDIDirEntry* n = arc->mNodes + node;
    for (uint32_t k = n->first_file_index; k < n->first_file_index + n->num_entries; k++) {
        if (k >= arc->mArcInfoBlock->num_file_entries) {
            return;
        }
        JKRArchive::SDIFileEntry* e = arc->mFiles + k;
        const char* name = arc->mStringTable + e->getNameOffset();
        if (e->isDirectory()) {
            if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                archiveEntries(arc, e->data_offset, prefix + name + "/", depth + 1, out);
            }
            continue;
        }
        out.push_back({prefix + name, n->type});
    }
}

bool hasCompressedFiles(JKRArchive* arc) {
    for (uint32_t k = 0; k < arc->mArcInfoBlock->num_file_entries; k++) {
        JKRArchive::SDIFileEntry* e = arc->mFiles + k;
        if (!e->isDirectory() && e->isCompressed()) {
            return true;
        }
    }
    return false;
}

// dRes_info_c::loadResource's loader and flags per directory type (as pc_j3d.cpp's planFor);
// 0: BMD, 1: BDL, -1: not a model this sweep loads.
int modelPlan(const uint8_t* b, uint32_t size, uint32_t dirType, uint32_t& flags) {
    if (size < 8 || memcmp(b, "J3D2", 4) != 0) {
        return -1;
    }
    int kind;
    if (memcmp(b + 4, "bmd2", 4) == 0 || memcmp(b + 4, "bmd3", 4) == 0) {
        kind = 0;
    } else if (memcmp(b + 4, "bdl4", 4) == 0) {
        kind = 1;
    } else {
        return -1;
    }
    static const struct {
        uint32_t type;
        int kind;
        uint32_t flags;
    } kRes[] = {
        {'BMD ', 0, 0x51240020}, {'BMDM', 0, 0x51240020}, {'BMDC', 0, 0x51240020},
        {'BMDS', 0, 0x00220020}, {'BSMD', 0, 0x01020020}, {'BDL ', 1, 0x00002020},
        {'BDLL', 1, 0x00001020}, {'BDLM', 1, 0x00002020}, {'BDLI', 1, 0x01002020},
        {'BDLC', 1, 0x00002020},
    };
    for (const auto& r : kRes) {
        if (r.type == dirType) {
            if (r.kind != kind) {
                return -1; // dRes_info_c would fail the archive
            }
            flags = r.flags;
            return kind;
        }
    }
    flags = kind == 0 ? 0x51240020 : 0x00002020;
    return kind;
}

void loadFrom(JKRArchive* arc, int depth) {
    Vector<Entry> entries;
    archiveEntries(arc, 0, "", 0, entries);
    for (const Entry& e : entries) {
        const String res = "/" + e.path;
        if (hasSuffix(e.path, ".arc") && depth == 0 && sNumNested < 16) {
            void* data = arc->getResource(res.c_str());
            JKRMemArchive* sub = data != nullptr ? new (sHeap, 0) JKRMemArchive() : nullptr;
            if (sub != nullptr && sub->mountFixed(data, JKRMEMBREAK_FLAG_UNKNOWN0)) {
                sNested[sNumNested++] = sub;
                loadFrom(sub, depth + 1);
            }
            continue;
        }
        const bool blo = hasSuffix(e.path, ".blo");
        // Models are told by their magic (modelPlan), whatever the file's name.
        const bool model = !blo && !hasSuffix(e.path, ".arc");
        if ((blo && !sDoScreens) || (model && !sDoModels) || (!blo && !model)) {
            continue;
        }
        // Room for the file and what the game builds from it, or skip it.
        if (sHeap->getTotalFreeSize() < 4 * 1024 * 1024) {
            sArcSkipped++;
            continue;
        }
        void* bytes = arc->getResource(res.c_str());
        uint32_t size = bytes != nullptr ? arc->getExpandedResSize(bytes) : 0;
        if (bytes != nullptr && memcmp(bytes, "Yaz0", 4) == 0) {
            // Stored compressed in a private copy (a MEM mount hands it out as stored): expanded
            // here into the sweep heap.
            void* out = JKRHeap::alloc(JKRDecompExpandSize((u8*)bytes), 32, sHeap);
            if (out != nullptr) {
                // decode's third argument is the number of bytes to write, the fourth to skip.
                size = JKRDecompExpandSize((u8*)bytes);
                JKRDecomp::decode((u8*)bytes, (u8*)out, size, 0);
            }
            bytes = out;
        }
        if (bytes == nullptr) {
            sArcSkipped++;
            continue;
        }
        if (model && (size < 8 || memcmp(bytes, "J3D2", 4) != 0)) {
            continue; // not a J3D file
        }
        if (sHeap->getTotalFreeSize() < (s32)(size * 3 + 2 * 1024 * 1024)) {
            sArcSkipped++;
            continue;
        }
        JKRHeap* old = sHeap->becomeCurrentHeap();
        if (model && sNumModels < kMaxModels) {
            uint32_t flags = 0;
            int kind = modelPlan((const uint8_t*)bytes, size, e.dirType, flags);
            J3DModelData* md = nullptr;
            if (kind == 0) {
                md = J3DModelLoaderDataBase::load(bytes, flags);
            } else if (kind == 1) {
                md = J3DModelLoaderDataBase::loadBinaryDisplayList(bytes, flags);
            }
            J3DModel* m = md != nullptr ? mDoExt_J3DModel__create(md, 0, 0x11020203) : nullptr;
            if (m != nullptr) {
                sModels[sNumModels++] = m;
            } else if (kind >= 0) {
                sArcSkipped++;
            }
            // kind -1: a BMT or another J3D file, not a model

        } else if (blo && sNumScreens < kMaxScreens) {
            // J2DScreen::set(<name>, archive) reads the file this way.
            JSUMemoryInputStream stream(bytes, size);
            J2DScreen* screen = new J2DScreen();
            if (screen != nullptr && screen->set(&stream)) {
                sScreens[sNumScreens++].screen = screen;
            } else {
                sArcSkipped++;
            }
        }
        old->becomeCurrentHeap();
    }
}

void freeArchive() {
    for (int i = sNumNested - 1; i >= 0; i--) {
        sNested[i]->unmountFixed();
    }
    sNumNested = 0;
    if (sCopy != nullptr) {
        sCopy->unmountFixed();
        sCopy = nullptr;
    } else if (sArc != nullptr) {
        sArc->unmount();
    }
    sArc = nullptr;
    sNumModels = 0;
    for (int i = 0; i < sNumScreens; i++) {
        sScreens[i].screen = nullptr;
    }
    sNumScreens = 0;
    // Models and screens are not destroyed one by one: nothing outside this heap points into it
    // once the frame that drew them is over.
    sHeap->freeAll();
}

bool selected(int index) {
    return index >= sFrom && index % sShards == sShard;
}

// The next archive to take, mounted and loaded; false when there is none left.
bool nextArchive() {
    for (;;) {
        sIndex++;
        if (sIndex >= (int)sArchives.size()) {
            return false;
        }
        if (!selected(sIndex)) {
            continue;
        }
        const String& path = sArchives[sIndex];
        if (sFd >= 0) {
            writef(sFd, "begin %d %s\n", sIndex, path.c_str());
        }
        sArcSkipped = 0;
        // An archive the game has mounted is shared by JKRArchive::mount (its models' display
        // lists already patched by the game's load): this sweep reads a private copy instead.
        const s32 entry = DVDConvertPathToEntrynum(path.c_str());
        if (entry >= 0 && JKRArchive::check_mount_already(entry) != nullptr) {
            void* buf = JKRDvdRipper::loadToMainRAM(path.c_str(), nullptr, EXPAND_SWITCH_UNKNOWN1,
                                                    0, sHeap, JKRDvdRipper::ALLOC_DIRECTION_FORWARD,
                                                    0, nullptr);
            sCopy = buf != nullptr ? new (sHeap, 0) JKRMemArchive() : nullptr;
            if (sCopy != nullptr && !sCopy->mountFixed(buf, JKRMEMBREAK_FLAG_UNKNOWN0)) {
                sCopy = nullptr;
            }
            // Out of the volume list, so that resources looked up by name (a screen's textures
            // and fonts) keep coming from the game's mount, which hands them out expanded.
            for (JSULink<JKRFileLoader>* l = JKRFileLoader::getVolumeList().getFirst();
                 sCopy != nullptr && l != nullptr; l = l->getNext()) {
                if (l->getObject() == sCopy) {
                    JKRFileLoader::getVolumeList().remove(l);
                    break;
                }
            }
            sArc = sCopy;
        } else {
        sArc = JKRArchive::mount(path.c_str(), JKRArchive::MOUNT_MEM, sHeap,
                                 JKRArchive::MOUNT_DIRECTION_HEAD);
        }
        if (sCopy == nullptr && sArc != nullptr && hasCompressedFiles(sArc)) {
            sArc->unmount();
            sArc = JKRArchive::mount(path.c_str(), JKRArchive::MOUNT_ARAM, sHeap,
                                     JKRArchive::MOUNT_DIRECTION_HEAD);
        }
        if (sArc == nullptr) {
            sMountFailed++;
            if (sFd >= 0) {
                writef(sFd, "%d %s mount-failed\n", sIndex, path.c_str());
            }
            sHeap->freeAll();
            continue;
        }
        loadFrom(sArc, 0);
        sTotalArchives++;
        if (sNumModels == 0 && sNumScreens == 0) {
            if (sFd >= 0) {
                writef(sFd, "%d %s models=0 screens=0 skipped=%u\n", sIndex, path.c_str(),
                       sArcSkipped);
            }
            sTotalSkipped += sArcSkipped;
            freeArchive();
            continue;
        }
        sPass = sNumModels > 0 ? 0 : 2;
        sDrawn = false;
        return true;
    }
}

int lastPass() {
    return sNumScreens > 0 ? 2 + (sNumScreens - 1) / kScreensPerFrame : 1;
}

void parseConfig() {
    const char* which = getenv("COS_RES_SWEEP");
    if (which != nullptr && *which != '\0') {
        sDoModels = strcmp(which, "all") == 0 || strcmp(which, "models") == 0;
        sDoScreens = strcmp(which, "all") == 0 || strcmp(which, "screens") == 0;
        if (!sDoModels && !sDoScreens) {
            writef(STDERR_FILENO, "[cos] res-sweep: COS_RES_SWEEP=\"%s\" is not models, screens "
                                  "or all\n", which);
            pc_exit(PC_EXIT_USAGE);
        }
    }
    const char* shard = getenv("COS_RES_SWEEP_SHARD");
    if (shard != nullptr && *shard != '\0') {
        int k = -1, n = 0;
        if (sscanf(shard, "%d/%d", &k, &n) != 2 || n < 1 || k < 0 || k >= n) {
            writef(STDERR_FILENO, "[cos] res-sweep: COS_RES_SWEEP_SHARD=\"%s\" is not <k>/<n>\n",
                   shard);
            pc_exit(PC_EXIT_USAGE);
        }
        sShard = k;
        sShards = n;
    }
    const char* from = getenv("COS_RES_SWEEP_FROM");
    if (from != nullptr && *from != '\0') {
        sFrom = atoi(from);
    }
}

} // namespace

// From dScnPly_Draw, after the actors' draws: this pass's models or screens.
void resSweepPlayDraw() {
    if (sState != kDraw || sDrawn) {
        return;
    }
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr) {
        return;
    }
    sDrawn = true;
    if (sPass < 2) {
        const s16 yaw = player->shape_angle.y;
        cXyz pos = player->current.pos;
        pos.x += kSpawnDistance * cM_ssin(yaw);
        pos.z += kSpawnDistance * cM_scos(yaw);
        static dKy_tevstr_c tevstr;
        dKy_tevstr_init(&tevstr, fopAcM_GetRoomNo(player), 0xFF);
        g_env_light.settingTevStruct(sPass == 0 ? TEV_TYPE_ACTOR : TEV_TYPE_BG0, &pos, &tevstr);
        if (sPass == 0) {
            dComIfGd_setList();
        } else {
            dComIfGd_setListBG();
        }
        for (int i = 0; i < sNumModels; i++) {
            J3DModel* m = sModels[i];
            mDoMtx_stack_c::transS(pos.x, pos.y, pos.z);
            mDoMtx_stack_c::YrotM(yaw);
            m->setBaseTRMtx(mDoMtx_stack_c::get());
            g_env_light.setLightTevColorType(m, &tevstr);
            mDoExt_modelUpdateDL(m);
        }
        dComIfGd_setList();
        return;
    }
    const int first = (sPass - 2) * kScreensPerFrame;
    for (int i = first; i < sNumScreens && i < first + kScreensPerFrame; i++) {
        dComIfGd_set2DOpa(&sScreens[i]);
    }
}

void resSweepFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke == nullptr || strcmp(gConfig.smoke, "res-sweep") != 0) {
            return;
        }
        if (pc_boot_stage() == nullptr) {
            writef(STDERR_FILENO, "[cos] res-sweep: needs COS_BOOT_STAGE (e.g. --stage sea:44:0)\n");
            pc_exit(PC_EXIT_USAGE);
        }
        parseConfig();
        sFd = openRunFile("res_sweep.txt");
        if (sFd >= 0) {
            writef(sFd, "# res-sweep (COS_SMOKE=res-sweep): \"begin <index> <archive>\" before "
                        "each archive, then its models and screens drawn and those skipped\n");
        }
        sState = kWaitLink;
    }
    if (sState == kOff || sState == kDone) {
        return;
    }
    sSince++;

    if (sState == kWaitLink) {
        if (!outsetLinkReady()) {
            sSince = 0;
            return;
        }
        if (sSince < kSettleFrames) {
            return;
        }
        findArchives("/res", sArchives, 0);
        uint32_t size = 64u << 20;
        if (const char* mb = getenv("COS_RES_SWEEP_HEAP_MB")) {
            size = (uint32_t)atoi(mb) << 20;
        }
        while (sHeap == nullptr && size >= (8u << 20)) {
            sHeap = JKRExpHeap::create(size, JKRHeap::getRootHeap(), false);
            if (sHeap == nullptr) {
                size -= 8u << 20;
            }
        }
        if (sHeap == nullptr) {
            writef(STDERR_FILENO, "[cos] res-sweep: no sweep heap (root free %d)\n",
                   (int)JKRHeap::getRootHeap()->getTotalFreeSize());
            pc_exit(PC_EXIT_CHECK_FAILED);
        }
        writef(STDERR_FILENO, "[cos] res-sweep: the player in the room; %zu archives (shard %d/%d, "
                              "from %d), models %s, screens %s, heap %u MiB, from frame %u\n",
               sArchives.size(), sShard, sShards, sFrom, sDoModels ? "on" : "off",
               sDoScreens ? "on" : "off", (unsigned)(size >> 20), frames);
        sState = kNext;
    }

    if (sState == kDraw) {
        if (!sDrawn) {
            if (sSince > 300) {
                writef(STDERR_FILENO, "[cos] res-sweep: the PLAY scene has not drawn for 300 "
                                      "frames; stopping\n");
                pc_exit(PC_EXIT_CHECK_FAILED);
            }
            return;
        }
        sSince = 0;
        sDrawn = false;
        sPass = sPass == 0 ? 1 : sPass == 1 ? (sNumScreens > 0 ? 2 : 99) : sPass + 1;
        if (sPass <= lastPass()) {
            return;
        }
        // The draw lists are painted at the start of the next game frame (fpcM_Management runs
        // cAPIGph_Painter before the processes): free after that one.
        sState = kDrain;
        return;
    }

    if (sState == kDrain) {
        if (sSince < 1) {
            return;
        }
        if (sFd >= 0) {
            writef(sFd, "%d %s models=%d screens=%d skipped=%u\n", sIndex,
                   sArchives[sIndex].c_str(), sNumModels, sNumScreens, sArcSkipped);
        }
        sTotalModels += sNumModels;
        sTotalScreens += sNumScreens;
        sTotalSkipped += sArcSkipped;
        freeArchive();
        if (const char* lost = sceneLost()) {
            writef(STDERR_FILENO, "[cos] res-sweep: after archive %d: %s; stopping\n", sIndex, lost);
            pc_exit(PC_EXIT_CHECK_FAILED);
        }
        sState = kNext;
    }

    if (sState == kNext) {
        if (!nextArchive()) {
            sState = kDone;
            writef(STDERR_FILENO, "[cos] res-sweep: %u archives: %u models, %u screens drawn, %u "
                                  "skipped, %u archives not mounted\n",
                   sTotalArchives, sTotalModels, sTotalScreens, sTotalSkipped, sMountFailed);
            if (sFd >= 0) {
                writef(sFd, "done: %u archives, %u models, %u screens, %u skipped, %u not "
                            "mounted\n", sTotalArchives, sTotalModels, sTotalScreens,
                       sTotalSkipped, sMountFailed);
                close(sFd);
                sFd = -1;
            }
            pc_exit(PC_EXIT_REACHED);
        }
        sState = kDraw;
        sSince = 0;
    }
}

} // namespace pc

extern "C" void pc_play_draw(void) {
    pc::resSweepPlayDraw();
}
