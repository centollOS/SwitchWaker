// COS_SMOKE=rando COS_RANDO_SMOKE=stagesB (docs/RANDOMIZER_PLAN.md step R3.16): stage tweaks B
// (features/rando/pc_rando_tweak_stages_b.h), run as
//   run.sh rando --stage <stage>:<room>:<point> --env COS_RANDO_SMOKE=stagesB [--env COS_RANDO_FORCE=1]
//                [--env COS_RANDO_STAGESB_DUMP=<file>]
// From the first frame the smoke registers the step's patcher (pc_rando_tweak_stages_b_init, as the
// game side would before the first dispatch) and after it a check patcher, which sees the final bytes
// of every stage.dzs / room.dzr: it runs pc_rando_tweak_stages_b_verify on them (the record asserts)
// and keeps their PLYR spawns. With COS_RANDO_STAGESB_DUMP it also appends every entry of the
// actor-like chunks (PLYR, ACTR/ACTx, SCOB/SCOx, TGDR, TRES/TREx, TGOB, TGSC, DOOR) to that file as
// "<stage>/<arc>/<file> <tag> <index> <hex>", the format of the oracle that runs wwrando's own tweak
// functions on the disc (the step's deviations note), so the two can be diffed.
// Once the play scene has run 10 frames:
//   - with pc_rando() 1: every site of the booted stage whose file was dispatched was applied (result
//     ok), the files of the booted room and the stage were dispatched, every verified file held its
//     records, the booted spawn point exists in the final bytes (in the PLYR list the game reads:
//     stage.dzs's when it has one, else the room's) and the player stands within 150 units (x/z) of
//     it (not checked for a ship spawn, ship_id != 0xFF, where the player starts on the boat); in M_NewD2 room 2 a Kanban actor with message 847 exists and message 847 is in the
//     overlay;
//   - with pc_rando() 0: no file was dispatched and M_NewD2 room 2 has no Kanban actor.
// The game then runs 30 more frames. Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "features/rando/pc_rando_dzx.h"
#include "features/rando/pc_rando_msg.h"
#include "features/rando/pc_rando_resources.h"
#include "features/rando/pc_rando_tweak_stages_b.h"

#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "stagesB";

bool sRegistered = false;
unsigned int sEndFrame = 0;
const char* sDump = nullptr;

struct Spawn {
    char stage[16];
    int fileRoom; // -1: stage.dzs
    int room;     // the room the spawn is for (the file's, or room_num for a stage.dzs spawn; logs)
    int id;
    int shipId; // z_rot >> 8: 0xFF none; else the player starts on the ship (the SHIP chunk places it)
    float x, y, z;
};
Spawn sSpawns[256];
int sNSpawns = 0;

struct Seen {
    char stage[16];
    int room;
    int ok;
    char why[160];
};
Seen sSeen[64];
int sNSeen = 0;

const char* kDumpBases[] = {"PLYR", "ACT", "SCO", "TGDR", "TRE", "TGOB", "TGSC", "DOOR"};

uint32_t dumpSize(const uint8_t* tag) {
    for (const char* b : kDumpBases) {
        if (memcmp(tag, b, strlen(b)) == 0) {
            return (tag[0] == 'S' || tag[0] == 'D' || memcmp(tag, "TGDR", 4) == 0 || memcmp(tag, "TGSC", 4) == 0)
                       ? 0x24
                       : 0x20;
        }
    }
    return 0;
}

void dumpFile(const PcRandoResFile* f) {
    FILE* out = fopen(sDump, "a");
    if (out == nullptr) {
        return;
    }
    const uint8_t* d = f->data;
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, f->data, f->size) != PCR_DZX_OK) {
        fclose(out);
        return;
    }
    const uint32_t n = dzx.chunkCount;
    for (uint32_t c = 0; c < n; c++) {
        const uint8_t* row = d + 4 + c * 0xC;
        const uint32_t es = dumpSize(row);
        if (es == 0) {
            continue;
        }
        const uint32_t cnt = pc_rando_dzx_be32(row + 4), off = pc_rando_dzx_be32(row + 8);
        if (off > f->size || (uint64_t)cnt * es > f->size - off) {
            fprintf(out, "%s/%s/%s %.4s: out of the file (%u at 0x%x)\n", f->stage, f->arc, f->name,
                    (const char*)row, cnt, off);
            continue;
        }
        for (uint32_t i = 0; i < cnt; i++) {
            fprintf(out, "%s/%s/%s %.4s %u ", f->stage, f->arc, f->name, (const char*)row, i);
            for (uint32_t k = 0; k < es; k++) {
                fprintf(out, "%02x", d[off + i * es + k]);
            }
            fputc('\n', out);
        }
    }
    fclose(out);
}

void checkPatcher(PcRandoResFile* f) {
    // The dzx patcher filter (DZS/DZR types) also passes other files of a room archive (model.brk in
    // M_NewD2/Room11), so the file name decides.
    if (f->stage[0] == 0 || (strcmp(f->name, "stage.dzs") != 0 && strcmp(f->name, "room.dzr") != 0)) {
        return;
    }
    if (sDump != nullptr) {
        dumpFile(f);
    }
    if (sNSeen < 64) {
        Seen& s = sSeen[sNSeen++];
        snprintf(s.stage, sizeof s.stage, "%s", f->stage);
        s.room = f->room;
        s.why[0] = 0;
        s.ok = pc_rando_tweak_stages_b_verify(f->stage, f->room, f->data, (uint32_t)f->size, s.why, sizeof s.why);
    }
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, f->data, f->size) != PCR_DZX_OK) {
        return;
    }
    const uint32_t n = pc_rando_dzx_count(&dzx, "PLYR");
    for (uint32_t i = 0; i < n && sNSpawns < 256; i++) {
        const uint8_t* e = pc_rando_dzx_entry(&dzx, "PLYR", i);
        Spawn& s = sSpawns[sNSpawns++];
        snprintf(s.stage, sizeof s.stage, "%s", f->stage);
        s.fileRoom = f->room;
        s.room = f->room >= 0 ? f->room : (int)(pc_rando_dzx_be32(e + 8) & 0x3F);
        s.id = e[0x1D]; // spawn_id: z_rot & 0xFF
        s.shipId = e[0x1C];
        s.x = pc_rando_dzx_bef32(e + 0x0C);
        s.y = pc_rando_dzx_bef32(e + 0x10);
        s.z = pc_rando_dzx_bef32(e + 0x14);
    }
}

// The spawn the game starts the player at (dStage_playerInit): the first PLYR list loaded wins, which
// is stage.dzs's when it has one (matched by spawn id only, as in I_TestM, M_Dai, kaze), else the
// booted room's.
const Spawn* findSpawn(const char* stage, int room, int id) {
    bool stageList = false;
    for (int i = 0; i < sNSpawns; i++) {
        const Spawn& s = sSpawns[i];
        if (strcmp(s.stage, stage) != 0 || s.fileRoom >= 0) {
            continue;
        }
        stageList = true;
        if (s.id == id) {
            return &s;
        }
    }
    if (stageList) {
        return nullptr;
    }
    for (int i = 0; i < sNSpawns; i++) {
        const Spawn& s = sSpawns[i];
        if (strcmp(s.stage, stage) == 0 && s.fileRoom == room && s.id == id) {
            return &s;
        }
    }
    return nullptr;
}

bool seenFile(const char* stage, int room) {
    for (int i = 0; i < sNSeen; i++) {
        if (strcmp(sSeen[i].stage, stage) == 0 && sSeen[i].room == room) {
            return true;
        }
    }
    return false;
}

struct Signs {
    int n;
    int with847;
};

void* collectKanban(void* proc, void* data) {
    Signs* s = static_cast<Signs*>(data);
    if (fpcM_GetName(proc) == fpcNm_KANBAN_e) {
        s->n++;
        if ((fopAcM_GetParam(static_cast<fopAc_ac_c*>(proc)) & 0xFFFF) == 847) {
            s->with847++;
        }
    }
    return nullptr;
}

} // namespace

void randoSmokeStagesB(unsigned int frames) {
    if (!sRegistered) {
        sRegistered = true;
        sDump = getenv("COS_RANDO_STAGESB_DUMP");
        pc_rando_tweak_stages_b_init();
        pc_rando_register_dzx_patcher(checkPatcher);
        writef(STDERR_FILENO, "[cos] rando-smoke stagesB: pc_rando() %d\n", pc_rando());
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
    sEndFrame = frames + 30;

    const char* stage = dComIfGp_getStartStageName();
    const int room = dComIfGp_getStartStageRoomNo();
    const int point = dComIfGp_getStartStagePoint();
    writef(STDERR_FILENO, "[cos] rando-smoke stagesB: booted %s room %d point %d\n", stage, room, point);
    Signs signs = {};
    fopAcM_Search(collectKanban, &signs);
    const bool drcHub = strcmp(stage, "M_NewD2") == 0 && room == 2;
    PcRandoResStats st;
    pc_rando_res_stats(&st);

    if (!pc_rando()) {
        randoSmokeCheck(kName, "vanilla: no file dispatched", st.dispatched == 0 && sNSeen == 0);
        if (drcHub) {
            randoSmokeCheck(kName, "vanilla: no hint sign in M_NewD2 room 2", signs.with847 == 0);
        }
        return;
    }

    int applied = 0;
    bool sitesOk = true;
    for (int i = 0; i < pc_rando_tweak_stages_b_site_count(); i++) {
        const PcRandoStagesBSite* s = pc_rando_tweak_stages_b_site(i);
        if (strcmp(s->stage, stage) != 0 || s->runs == 0) {
            continue;
        }
        writef(STDERR_FILENO, "[cos] rando-smoke stagesB: site #%d %s/%d %s: runs %d, %s\n", s->tweak, s->stage,
               s->room, s->what, s->runs, pc_rando_dzx_result_name(s->result));
        sitesOk = sitesOk && s->result == PCR_DZX_OK;
        applied++;
    }
    randoSmokeCheck(kName, "every dispatched site of the stage was applied", sitesOk);
    randoSmokeCheck(kName, "the stage's stage.dzs and the room's room.dzr were dispatched",
                    seenFile(stage, -1) && seenFile(stage, room));
    bool recordsOk = true;
    for (int i = 0; i < sNSeen; i++) {
        if (!sSeen[i].ok) {
            writef(STDERR_FILENO, "[cos] rando-smoke stagesB: record check failed: %s\n", sSeen[i].why);
            recordsOk = false;
        }
    }
    char what[96];
    snprintf(what, sizeof what, "records hold in the %d dispatched file(s) (%d site(s) applied)", sNSeen, applied);
    randoSmokeCheck(kName, what, recordsOk);

    const Spawn* sp = findSpawn(stage, room, point);
    snprintf(what, sizeof what, "spawn %d exists for room %d", point, room);
    randoSmokeCheck(kName, what, sp != nullptr);
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (sp != nullptr && player != nullptr) {
        const float dx = player->current.pos.x - sp->x, dz = player->current.pos.z - sp->z;
        const float d = std::sqrt(dx * dx + dz * dz);
        writef(STDERR_FILENO,
               "[cos] rando-smoke stagesB: spawn %d at (%.1f, %.1f, %.1f), player at (%.1f, %.1f, %.1f), %.1f away\n",
               point, sp->x, sp->y, sp->z, player->current.pos.x, player->current.pos.y, player->current.pos.z, d);
        if (sp->shipId != 0xFF) {
            writef(STDERR_FILENO, "[cos] rando-smoke stagesB: ship spawn (ship_id %d): position not checked\n",
                   sp->shipId);
        } else {
            randoSmokeCheck(kName, "the player stands at the spawn (x/z within 150)", d < 150.0f);
        }
    } else {
        randoSmokeCheck(kName, "the player exists", player != nullptr);
    }
    if (drcHub) {
        randoSmokeCheck(kName, "M_NewD2 room 2 has the hint sign (Kanban, message 847)", signs.with847 == 1);
        const PcRandoMsg* m = pc_rando_msg_find(847);
        randoSmokeCheck(kName, "message 847 is in the overlay (wood box, right arrow)",
                        m != nullptr && m->has_entry && m->entry.text_box_type == 2 && m->len == 6 &&
                            m->bytes[0] == 0x1A && m->bytes[4] == 0x15);
    }
}

} // namespace pc
