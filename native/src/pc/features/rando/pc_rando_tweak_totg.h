/*
 * pc_rando_tweak_totg.h - Tower of the Gods servants (docs/RANDOMIZER_PLAN.md step R3.18, tweaks
 * report #28): tweaks.py allow_nonlinear_servants_of_the_towers (wwrando 1.10.0), which wwrando
 * always applies, made in memory through the archive-load hook (pc_rando_resources.h, step R0.10)
 * while pc_rando() is 1. The three Servants of the Tower can be brought back in any order: the
 * Command Melody tablet appears once any one is back, the light-beam warp once all three are back
 * and the tablet's item is taken.
 *
 *   Siren stage.dzs   TGDR[6] (north door) and TGDR[8] (west door) switch_1 (params & 0xFF) = 0xFF
 *                     (unlocked); a new EVNT "hsehi1_appear" (its index goes to the first SwOp).
 *   Siren Room7       ACTR Hsh (the tablet) switch_to_set 0x29 -> 0x2B; ACTR Ywarp00 activation
 *   room.dzr          switch 0x28 -> 0x7F; SCOB kytag00 switch_to_check (x_rot & 0xFF) and SCOB
 *                     AttTag (type 1, switch 0x28) switch 0x28 -> 0x7F; two new ACTR SwOp (step R3.7):
 *                     OR over 0x28-0x2A sets 0x7E after playing hsehi1_appear, AND over 0x28-0x2B
 *                     sets 0x7F.
 *   Siren             Os_Finish (east servant: sets 0x29 itself with a new SW_ON, no tablet, light
 *   event_list.dat    beam), Os1_Finish, Os1_Message (cut to its last actions), Os2_Finish (no east
 *                     and west servants), hsehi1_tact (the tablet disappears at the end), the new
 *                     event hsehi1_appear (CAMERA, Hsh, Link, TIMEKEEPER), a SwOp staff with a DUMMY
 *                     cut in four events, and the servant/platform/timer speed-ups. The whole file is
 *                     re-serialized (pc_rando_event_ser.h, step R3.14) and equals wwrando's
 *                     save_changes byte for byte.
 *
 * Each file is checked against its vanilla shape first (D9): the stage.dzs and room.dzr records by
 * name and vanilla byte, the six events by the names of every action of every actor the edits read
 * or change. On a mismatch (replaced content files) the file is left vanilla and the reason logged.
 * room.dzr is only patched when this stage.dzs patch added the EVNT (the first SwOp needs its index).
 *
 * Pure code (no game headers, no game calls): it links into cos_rando_core. The game side calls
 * pc_rando_tweak_totg_init through pc_rando_patchers.def. Game thread only.
 */
#ifndef PC_RANDO_TWEAK_TOTG_H
#define PC_RANDO_TWEAK_TOTG_H

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The files the tweak edits (status index). */
enum PcRandoTotgFile {
    PCR_TOTG_DZS = 0, /* Siren Stage.arc stage.dzs */
    PCR_TOTG_DZR,     /* Siren Room7.arc room.dzr */
    PCR_TOTG_EVL,     /* Siren Stage.arc event_list.dat */
    PCR_TOTG_FILE_COUNT
};

/* Switches (stage memory of Tower of the Gods). */
#define PCR_TOTG_SW_NORTH 0x28       /* north servant returned */
#define PCR_TOTG_SW_EAST 0x29        /* east servant returned (vanilla: the tablet's switch) */
#define PCR_TOTG_SW_WEST 0x2A        /* west servant returned */
#define PCR_TOTG_SW_TABLET 0x2B      /* the tablet's item taken (new) */
#define PCR_TOTG_SW_ANY 0x7E         /* any servant returned (set by the OR SwOp) */
#define PCR_TOTG_SW_ALL 0x7F         /* all returned and the tablet's item taken (AND SwOp) */
#define PCR_TOTG_APPEAR_EVENT "hsehi1_appear"

typedef struct PcRandoTotgStatus {
    int seen;      /* times the file went through the patcher */
    int applied;   /* times it was patched */
    int last;      /* the last result: PCR_DZX_* for dzs/dzr, PCR_EVL_* for the event list; -1 none */
    uint64_t hash; /* after the last patch: dzs/dzr the content hash (pc_rando_tweak_totg_content_hash)
                      of TGDR+EVNT / ACTR+SCOB, the event list the FNV-1a 64 of the whole file */
    uint32_t size; /* the event list's serialized size (0 for the dzx files) */
} PcRandoTotgStatus;

/* Registers the patcher (dzs, dzr and dat files). Idempotent. */
void pc_rando_tweak_totg_init(void);

/* The patcher (also callable directly by tests): picks the edit by stage, arc and file name. */
void pc_rando_tweak_totg_patch(PcRandoResFile* file);

/* The edits on one file, for tests. dzs/dzr return a PcRandoDzxResult, the event list a
   PcRandoEvlResult; on an error the file is unchanged. pc_rando_tweak_totg_dzr uses the EVNT index
   the last successful pc_rando_tweak_totg_dzs produced (PCR_DZX_MISMATCH when there was none). */
int pc_rando_tweak_totg_dzs(PcRandoResFile* file);
int pc_rando_tweak_totg_dzr(PcRandoResFile* file);
int pc_rando_tweak_totg_evl(PcRandoResFile* file);

/* The EVNT index of hsehi1_appear from the last successful stage.dzs patch, or -1. */
int pc_rando_tweak_totg_evnt_index(void);

/* 64-bit FNV-1a over, for each tag in order, the 4 tag bytes and every entry of that chunk (actor
   entries with an empty name left out). The fixture tool R3_18_make_fixture.py computes the same. */
uint64_t pc_rando_tweak_totg_content_hash(const void* data, size_t size, const char* const* tags, int nTags);

void pc_rando_tweak_totg_status(int file, PcRandoTotgStatus* out);
void pc_rando_tweak_totg_reset_status(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_TWEAK_TOTG_H */
