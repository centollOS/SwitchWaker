/*
 * pc_rando_resources.h - the randomizer's archive-load hook: a dispatcher that hands the raw files of
 * a freshly loaded archive to registered patchers (docs/RANDOMIZER_PLAN.md step R0.10, decision D8).
 *
 * wwrando edits stage.dzs, room.dzr and event_list.dat on the disc. The port edits them in memory
 * instead, right after dRes_info_c::loadResource (game/src/d/d_resorce.cpp) took each file out of
 * the archive, while they are still the disc's big-endian bytes (dStage_dt_c_offsetToPtr and
 * dEvent_manager_c::setData run later). Two hunks, both only under pc_rando():
 *   - dRes_info_c::set records the archive's directory (pc_rando_res_hook_set), so "/res/Stage/
 *     ma2room/" and "/res/Stage/ma3room/" (both mounted from "Stage") are told apart;
 *   - dRes_info_c::loadResource passes every file it puts into mRes[] through
 *     pc_rando_res_hook_loaded (game_hooks/pc_rando_hooks_resources.cpp), which builds a
 *     PcRandoResFile and calls pc_rando_res_dispatch.
 *
 * A patcher edits file->data in place (the bytes live in the mounted archive, which is read from the
 * disc again on every mount, so an edit never outlives the archive), or replaces the file with a
 * bigger copy (pc_rando_res_grow_dzx, pc_rando_res_replace). Copies are allocated on the archive's
 * own data heap (the current heap while loadResource runs), so they are freed with the archive.
 * Patchers run in registration order; each sees the result of the ones before it.
 *
 * Patchers of later steps register from their module's init function, listed in
 * pc_rando_patchers.def (append-only), which the game side calls once before the first dispatch.
 * They may also register at run time (smokes, tests).
 *
 * This file and pc_rando_resources.cpp use no game header or game symbol (cos_rando_core links them
 * into the unit tests). Game thread only.
 */
#ifndef PC_RANDO_RESOURCES_H
#define PC_RANDO_RESOURCES_H

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_dzx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* loadResource's resource type tags (big-endian four-character codes as the game writes them). */
#define PCR_RES_TYPE(a, b, c, d) \
    (((uint32_t)(uint8_t)(a) << 24) | ((uint32_t)(uint8_t)(b) << 16) | ((uint32_t)(uint8_t)(c) << 8) | (uint32_t)(uint8_t)(d))
#define PCR_RES_DZS PCR_RES_TYPE('D', 'Z', 'S', ' ') /* stage.dzs */
#define PCR_RES_DZR PCR_RES_TYPE('D', 'Z', 'R', ' ') /* room.dzr */
#define PCR_RES_DAT PCR_RES_TYPE('D', 'A', 'T', ' ') /* event_list.dat and other .dat files */
#define PCR_RES_ANY 0u                               /* registration filter: every type */

/* The longest directory dRes_info_c::set builds (its path buffer is 40 bytes). */
#define PCR_RES_DIR_MAX 40

/* Allocates `size` bytes (0x20-aligned) that live as long as the archive; NULL on failure. */
typedef void* (*PcRandoResAlloc)(size_t size, void* ctx);

typedef struct PcRandoResFile {
    /* where the file comes from */
    const char* dir;   /* the archive's directory as dRes_info_c::set got it, e.g. "/res/Stage/Asoko/";
                          "" when it was not recorded */
    const char* stage; /* the stage directory when dir is "/res/Stage/<stage>/" ("Asoko", "ma3room"),
                          else "" */
    const char* arc;   /* the archive name without ".arc": "Stage", "Room0", an object archive name */
    int room;          /* N for an archive named "Room<N>", else -1 */
    const char* name;  /* the file name inside the archive: "stage.dzs", "room.dzr", "event_list.dat" */
    uint32_t type;     /* loadResource's type: PCR_RES_DZS, PCR_RES_DZR, PCR_RES_DAT, ... */
    /* the bytes: the archive's own buffer, or a copy once a patcher replaced it */
    uint8_t* data;
    size_t size;
    int replaced;      /* 1 once data is a copy (pc_rando_res_replace / pc_rando_res_grow_dzx) */
    /* for the dispatcher (pc_rando_res_dispatch's caller fills them; NULL alloc: nothing can grow) */
    PcRandoResAlloc alloc;
    void* allocCtx;
} PcRandoResFile;

typedef void (*PcRandoResPatcher)(PcRandoResFile* file);

/* Registers a patcher for files of `type` (PCR_RES_ANY: all). A patcher is added once however often
   it is passed (the first type wins). Returns 0 when the list (64 entries) is full or fn is NULL. */
int pc_rando_register_res_patcher(PcRandoResPatcher fn, uint32_t type);
/* The plan's pc_rando_register_dzx_patcher: a patcher for stage.dzs and room.dzr files only. */
int pc_rando_register_dzx_patcher(PcRandoResPatcher fn);
/* Removes a patcher (smokes and tests). Returns 1 when it was registered. */
int pc_rando_unregister_res_patcher(PcRandoResPatcher fn);
int pc_rando_res_patcher_count(void);

/* Runs every patcher that takes file->type, in registration order, and returns the final bytes
   (file->data, which may now be a copy). dir/stage/arc/room/name/type/data/size/alloc must be set;
   pc_rando_res_describe fills dir/stage/arc/room from an archive name and directory. */
void* pc_rando_res_dispatch(PcRandoResFile* file);

/* Fills file->dir, stage, arc and room from the archive name and directory (dir NULL: ""). The
   strings are copied into `store`, which must outlive the file. */
typedef struct PcRandoResNames {
    char dir[PCR_RES_DIR_MAX];
    char stage[PCR_RES_DIR_MAX];
    char arc[16];
} PcRandoResNames;
void pc_rando_res_describe(PcRandoResFile* file, PcRandoResNames* store, const char* arcName,
                           const char* dir);

/* Replaces the file with a new buffer of newSize bytes from file->alloc. The old bytes are copied in
   (min of the two sizes; the rest is zero). Returns the new buffer, or NULL (no allocator, out of
   memory), in which case the file is unchanged. */
uint8_t* pc_rando_res_replace(PcRandoResFile* file, size_t newSize);
/* Appends entries to the file's dzx chunks (pc_rando_dzx_grow) into a copy that replaces it.
   firstIndex as pc_rando_dzx_grow. Returns PCR_DZX_OK, a dzx error, or PCR_DZX_NO_SPACE when the
   copy cannot be allocated; on an error the file is unchanged. */
int pc_rando_res_grow_dzx(PcRandoResFile* file, const PcRandoDzxAdd* adds, int nAdds,
                          uint32_t* firstIndex);

/* --- the archive directory table (the dRes_info_c::set hook) --- */
/* Records `dir` for the dRes_info_c `info` (a later call for the same info overwrites it). */
void pc_rando_res_note_dir(const void* info, const char* dir);
/* The recorded directory of `info`, or NULL. */
const char* pc_rando_res_dir_of(const void* info);
/* Forgets every recorded directory (tests). */
void pc_rando_res_clear_dirs(void);

/* Counters since boot, for smokes and logs. */
typedef struct PcRandoResStats {
    uint32_t dispatched; /* files passed to pc_rando_res_dispatch */
    uint32_t patched;    /* files at least one patcher was called for */
    uint32_t replaced;   /* files that ended as a copy */
    uint32_t failed;     /* failed replacements (no allocator, out of memory) */
} PcRandoResStats;
void pc_rando_res_stats(PcRandoResStats* out);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_RESOURCES_H */
