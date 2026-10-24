// The particle bank hook, game side (docs/RANDOMIZER_PLAN.md step R7.1b; the registry is
// features/rando/pc_rando_jpc_scene.h, the JPC library features/rando/pc_rando_jpc.h, hunks listed in
// docs/RANDO_HOOKS.md "R7.1b").
//
// wwrando edits particle files on the disc (randomizer.py get_jpc, saved by JPC.save()); the port
// edits them as the game loads them. The JPA loaders read a file's bytes in place, so the merge and
// the colour edits work on the raw big-endian bytes before the JPAResourceManager is built.
//
// - dPa_control_c::createRoomScene (d_particle.cpp), under pc_rando(): pc_rando_particles_scene gets
//   the loaded Pscene%03d.jpc. When the registry has imports for that scene, the file is loaded into
//   a PcRandoJpcFile, each particle is copied from its source file (read synchronously from the disc
//   into host memory, kept for the activation: the raw files up to kSourceCacheBytes, and for "any"
//   sources the particle ids of every Pscene000-254 file scanned, in wwrando's order, first file
//   holding the id wins) and the result is saved in gclib's layout into memory taken from the
//   scene's own solid heap (mSceneHeap) before the JPAResourceManager is made there. So the merged
//   copy lives exactly as long as the scene resources that point into it: removeRoomScene destroys
//   mSceneHeap, and still frees the loaded file through mpData, which keeps pointing at it (the
//   loaded file stays allocated for the scene, as in a vanilla game). mDoExt_adjustSolidHeap trims
//   the heap after the manager as before, so the copy costs only its own size. Without imports (or
//   when one fails to be built) the loaded file is used, and editors edit it in place.
//   The file size is the disc file's (DVDOpen); Pscene files are not compressed.
// - dPa_control_c::createCommon: common.jpc is loaded once at boot, before any randomized file, so
//   pc_rando_particles_common keeps its address in every game (that is all it does while pc_rando()
//   is 0). Under pc_rando() its editors run on those bytes: at createCommon (COS_RANDO_FORCE) and
//   whenever an editor for it is registered later (the registry's notifier). Before the first edit
//   of an activation the file is copied to host memory, and the copy is put back when pc_rando()
//   turns 0. Limit: the JPA bakes some values at load (BSP1 colour key tables, makeColorTable), so
//   an edit made after createCommon only reaches values it reads live (BSP1/SSP1 prm and env colours,
//   which is what customizer.py's arrow trail colour changes).
// - At deactivation the registry, the source caches and the last-scene stats are cleared.
//
// Std containers made here (the JPC library's, the editors') use host memory (JKRPcHostAllocScope),
// never the game's current JKRHeap.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_jpc.h"
#include "features/rando/pc_rando_jpc_scene.h"

#include "JSystem/JKernel/JKRHeap.h"

#include "cos_sdk/host_alloc.h"

#include <dolphin/dvd.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <unistd.h>

namespace {

constexpr size_t kSourceCacheBytes = 2u << 20;
constexpr int kScenes = 255;

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[320];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

void scenePath(int scene, char* out, size_t cap) {
    if (scene == PCR_JPC_SCENE_COMMON) {
        snprintf(out, cap, "/res/Particle/common.jpc");
    } else {
        snprintf(out, cap, "/res/Particle/Pscene%03d.jpc", scene);
    }
}

// The disc file's size, 0 when it does not exist.
uint32_t discSize(int scene) {
    char path[48];
    scenePath(scene, path, sizeof path);
    DVDFileInfo info;
    if (!DVDOpen(path, &info)) {
        return 0;
    }
    const uint32_t n = info.length;
    DVDClose(&info);
    return n;
}

bool readDisc(int scene, cos_sdk::HostVector<uint8_t>* out) {
    char path[48];
    scenePath(scene, path, sizeof path);
    DVDFileInfo info;
    if (!DVDOpen(path, &info)) {
        return false;
    }
    out->resize(info.length);
    const s32 got = DVDReadPrio(&info, out->data(), (s32)info.length, 0, 2);
    DVDClose(&info);
    return got == (s32)info.length;
}

// ---- per-activation caches (host memory) ----
// Raw source files (the import sources), up to kSourceCacheBytes in all.
struct SourceFile {
    int scene;
    cos_sdk::HostVector<uint8_t> bytes;
};
cos_sdk::HostVector<std::unique_ptr<SourceFile>>* sSources = nullptr;
size_t sSourceBytes = 0;
// "any" scans: per Pscene number, 0 not scanned, 1 its ids are in sIds, -1 missing or unreadable.
int8_t sScanState[kScenes];
cos_sdk::HostVector<uint16_t> sIds[kScenes];

PcRandoParticleSceneStats sLast;
uint32_t sScenesBuilt = 0;

// common.jpc
uint8_t* sCommon = nullptr;
uint32_t sCommonSize = 0;
size_t sCommonFrom = 0; // registry index of the next common editor to run
cos_sdk::HostVector<uint8_t> sCommonSnapshot;
int sCommonEdits = 0;

cos_sdk::HostVector<std::unique_ptr<SourceFile>>& sources() {
    if (sSources == nullptr) {
        // Never freed: outlives the program's static destructors.
        sSources = new (cos_sdk::HostAllocate(sizeof *sSources, alignof(cos_sdk::HostVector<int>)))
            cos_sdk::HostVector<std::unique_ptr<SourceFile>>();
    }
    return *sSources;
}

// The raw bytes of Pscene<scene> (kept in the cache when it fits), or NULL. `hold` keeps an uncached
// read alive for the caller.
const cos_sdk::HostVector<uint8_t>* sourceBytes(int scene, cos_sdk::HostVector<uint8_t>* hold) {
    for (const std::unique_ptr<SourceFile>& f : sources()) {
        if (f->scene == scene) {
            return &f->bytes;
        }
    }
    cos_sdk::HostVector<uint8_t> bytes;
    if (!readDisc(scene, &bytes)) {
        return nullptr;
    }
    if (sSourceBytes + bytes.size() <= kSourceCacheBytes) {
        sSourceBytes += bytes.size();
        std::unique_ptr<SourceFile> f(new SourceFile{scene, {}});
        f->bytes.swap(bytes);
        sources().push_back(std::move(f));
        return &sources().back()->bytes;
    }
    hold->swap(bytes);
    return hold;
}

void clearCaches() {
    JKRPcHostAllocScope scope;
    sources().clear();
    sSourceBytes = 0;
    for (int i = 0; i < kScenes; i++) {
        sScanState[i] = 0;
        cos_sdk::HostVector<uint16_t>().swap(sIds[i]);
    }
}

// The source scene for an "any" import (enemies.py update_loaded_particles: range(255), skip files
// the disc lacks, the first that holds the id), -1 when none.
int findAny(uint16_t id) {
    for (int i = 0; i < kScenes; i++) {
        if (sScanState[i] == 0) {
            sScanState[i] = -1;
            cos_sdk::HostVector<uint8_t> hold;
            const cos_sdk::HostVector<uint8_t>* raw = discSize(i) != 0 ? sourceBytes(i, &hold) : nullptr;
            std::vector<PcRandoJpcParticle> ps;
            if (raw != nullptr && pc_rando_jpc_scan(raw->data(), raw->size(), &ps, nullptr) == PCR_JPC_OK) {
                sIds[i].reserve(ps.size());
                for (const PcRandoJpcParticle& p : ps) {
                    sIds[i].push_back(p.id);
                }
                sScanState[i] = 1;
            } else if (raw != nullptr) {
                logf("[cos] rando: particles: Pscene%03d.jpc cannot be read as a JPAC1-00 file (skipped)\n", i);
            }
        }
        if (sScanState[i] == 1) {
            for (uint16_t have : sIds[i]) {
                if (have == id) {
                    return i;
                }
            }
        }
    }
    return -1;
}

// A loaded source file for one createRoomScene call.
struct LoadedSource {
    int scene;
    PcRandoJpcFile file;
};

void applyCommon() {
    if (sCommon == nullptr || !pc_rando()) {
        return;
    }
    if (!pc_rando_jpc_scene_has_editors(PCR_JPC_SCENE_COMMON)) {
        sCommonFrom = pc_rando_jpc_scene_editor_count();
        return;
    }
    if (sCommonSize == 0) {
        sCommonSize = discSize(PCR_JPC_SCENE_COMMON);
        if (sCommonSize == 0) {
            logf("[cos] rando: particles: common.jpc size unknown, its editors do not run\n");
            return;
        }
    }
    JKRPcHostAllocScope scope;
    if (sCommonSnapshot.empty()) {
        sCommonSnapshot.assign(sCommon, sCommon + sCommonSize);
    }
    const size_t from = sCommonFrom;
    sCommonFrom = pc_rando_jpc_scene_run_editors(PCR_JPC_SCENE_COMMON, sCommon, sCommonSize, from);
    sCommonEdits++;
}

void onRando(int active) {
    if (active) {
        return;
    }
    if (sCommon != nullptr && !sCommonSnapshot.empty()) {
        memcpy(sCommon, sCommonSnapshot.data(), sCommonSnapshot.size());
        logf("[cos] rando: particles: common.jpc restored (%u bytes)\n", (unsigned)sCommonSnapshot.size());
    }
    {
        JKRPcHostAllocScope scope;
        cos_sdk::HostVector<uint8_t>().swap(sCommonSnapshot);
    }
    sCommonFrom = 0;
    sCommonEdits = 0;
    pc_rando_jpc_scene_clear();
    clearCaches();
    sLast = PcRandoParticleSceneStats{};
    sLast.scene = -1;
}

struct RegisterParticles {
    RegisterParticles() {
        sLast.scene = -1;
        pc_rando_jpc_scene_set_common_notifier(applyCommon);
        if (!pc_rando_add_listener(onRando)) {
            static const char kMsg[] = "[cos] rando: particles: listener list full, imports are never cleared\n";
            (void)!write(STDERR_FILENO, kMsg, sizeof kMsg - 1);
        }
    }
} sRegisterParticles;

} // namespace

extern "C" const void* pc_rando_particles_scene(int scene, const void* data, void* sceneHeap) {
    PcRandoParticleSceneStats st = {};
    st.scene = scene;
    st.data = data;
    st.loaded = data;
    if (data == nullptr || scene < 0 || scene >= kScenes) {
        sLast = st;
        return data;
    }
    const size_t numImports = pc_rando_jpc_scene_imports(scene, nullptr, 0);
    const bool editors = pc_rando_jpc_scene_has_editors(scene);
    if (numImports == 0 && !editors) {
        sLast = st;
        return data;
    }
    const uint32_t size = discSize(scene);
    st.size = size;
    if (size == 0) {
        logf("[cos] rando: particles: Pscene%03d.jpc size unknown, left vanilla\n", scene);
        sLast = st;
        return data;
    }
    JKRPcHostAllocScope scope;
    uint8_t* out = (uint8_t*)const_cast<void*>(data);
    size_t outSize = size;
    if (numImports != 0) {
        cos_sdk::HostVector<PcRandoJpcImport> imports(numImports);
        pc_rando_jpc_scene_imports(scene, imports.data(), imports.size());
        PcRandoJpcFile dst;
        const int r = pc_rando_jpc_load(&dst, (const uint8_t*)data, size);
        if (r != PCR_JPC_OK) {
            logf("[cos] rando: particles: Pscene%03d.jpc: %s, its %u imports are skipped\n", scene,
                 pc_rando_jpc_result_name(r), (unsigned)numImports);
            st.failed = (int)numImports;
        } else {
            std::vector<std::unique_ptr<LoadedSource>> loaded;
            for (const PcRandoJpcImport& im : imports) {
                if (pc_rando_jpc_find_particle(dst, im.id) >= 0) {
                    st.skipped++;
                    continue;
                }
                const int src = im.source == PCR_JPC_SCENE_ANY ? findAny(im.id) : im.source;
                if (src < 0) {
                    logf("[cos] rando: particles: no Pscene file holds particle 0x%04X (import into %03d skipped)\n",
                         im.id, scene);
                    st.failed++;
                    continue;
                }
                LoadedSource* ls = nullptr;
                for (const std::unique_ptr<LoadedSource>& l : loaded) {
                    if (l->scene == src) {
                        ls = l.get();
                    }
                }
                if (ls == nullptr) {
                    cos_sdk::HostVector<uint8_t> hold;
                    const cos_sdk::HostVector<uint8_t>* raw = sourceBytes(src, &hold);
                    std::unique_ptr<LoadedSource> l(new LoadedSource{src, {}});
                    const int lr = raw != nullptr ? pc_rando_jpc_load(&l->file, raw->data(), raw->size())
                                                  : PCR_JPC_BAD_FILE;
                    if (lr != PCR_JPC_OK) {
                        logf("[cos] rando: particles: source Pscene%03d.jpc: %s\n", src,
                             raw != nullptr ? pc_rando_jpc_result_name(lr) : "cannot be read");
                        l->scene = -1 - src; // remembered as unusable for this call
                    }
                    loaded.push_back(std::move(l));
                    ls = loaded.back().get();
                }
                if (ls->scene != src) {
                    st.failed++;
                    continue;
                }
                const int cr = pc_rando_jpc_copy_particle(&dst, ls->file, im.id);
                if (cr == PCR_JPC_OK) {
                    st.imported++;
                } else {
                    logf("[cos] rando: particles: particle 0x%04X from Pscene%03d into %03d: %s\n", im.id, src, scene,
                         pc_rando_jpc_result_name(cr));
                    st.failed++;
                }
            }
            if (st.imported != 0) {
                const size_t n = pc_rando_jpc_saved_size(dst);
                void* mem = n <= 0xFFFFFFFFu ? JKRHeap::alloc((u32)n, 0x20, (JKRHeap*)sceneHeap) : nullptr;
                size_t written = 0;
                const int sr = mem != nullptr ? pc_rando_jpc_save_into(dst, (uint8_t*)mem, n, &written)
                                              : PCR_JPC_NO_SPACE;
                if (sr == PCR_JPC_OK) {
                    out = (uint8_t*)mem;
                    outSize = written;
                    st.merged = 1;
                } else {
                    logf("[cos] rando: particles: Pscene%03d.jpc: the merged copy (%u bytes) cannot be made (%s), "
                         "the %d imports are lost\n",
                         scene, (unsigned)n, mem == nullptr ? "scene heap full" : pc_rando_jpc_result_name(sr),
                         st.imported);
                    if (mem != nullptr) {
                        JKRHeap::free(mem, (JKRHeap*)sceneHeap);
                    }
                    st.failed += st.imported;
                    st.imported = 0;
                }
            }
        }
    }
    if (editors) {
        pc_rando_jpc_scene_run_editors(scene, out, outSize, 0);
        st.edited = 1;
    }
    st.data = out;
    st.size = (uint32_t)outSize;
    sScenesBuilt++;
    st.serial = sScenesBuilt;
    logf("[cos] rando: particles: Pscene%03d.jpc: %d imported, %d skipped, %d failed%s, %u -> %u bytes%s\n", scene,
         st.imported, st.skipped, st.failed, st.edited ? ", edited" : "", (unsigned)size, (unsigned)outSize,
         st.merged ? " (on the scene heap)" : "");
    sLast = st;
    return out;
}

extern "C" void pc_rando_particles_common(const void* data) {
    sCommon = (uint8_t*)const_cast<void*>(data);
    sCommonSize = 0;
    sCommonFrom = 0;
    if (pc_rando()) {
        applyCommon();
    }
}

extern "C" const PcRandoParticleSceneStats* pc_rando_particles_last_scene(void) {
    return &sLast;
}

extern "C" const uint8_t* pc_rando_particles_common_data(uint32_t* size, int* edits) {
    if (size != nullptr) {
        *size = sCommon != nullptr && sCommonSize == 0 ? discSize(PCR_JPC_SCENE_COMMON) : sCommonSize;
    }
    if (edits != nullptr) {
        *edits = sCommonEdits;
    }
    return sCommon;
}

extern "C" int pc_rando_particles_find_any(uint16_t id) {
    JKRPcHostAllocScope scope;
    return findAny(id);
}
