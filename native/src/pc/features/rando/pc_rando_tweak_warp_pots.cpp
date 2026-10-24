// The inter-dungeon warp pots (pc_rando_tweak_warp_pots.h, docs/RANDOMIZER_PLAN.md step R7.1c):
// wwrando 1.10.0 tweaks.py add_inter_dungeon_warp_pots on the raw stage files, plus the particle
// imports it makes into Pscene050 and Pscene043. Pure: no game headers, no game calls.
#include "pc_rando_tweak_warp_pots.h"

#include "pc/pc_rando.h"
#include "pc_rando_dzx.h"
#include "pc_rando_jpc_scene.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
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

void put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}
void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
void putf(uint8_t* p, float f) {
    uint32_t v;
    memcpy(&v, &f, 4);
    put32(p, v);
}

// INTER_DUNGEON_WARP_DATA (tweaks.py). The coordinates are Python floats written as float32
// (struct.pack('>f'), round to nearest), as the casts below do.
const PcRandoWarpPot kPots[PCR_WARP_POT_COUNT] = {
    {"M_NewD2", 2, (float)2185.0, (float)0.0, (float)590.0, 0xA000, 2, 0, 0, 0},      // DRC
    {"kindan", 1, (float)986.0, (float)3956.43, (float)9588.0, 0xB929, 2, 0, 1, 0},   // FW
    {"Siren", 6, (float)277.0, (float)229.42, (float)-6669.0, 0xC000, 2, 0, 2, 0},    // TotG
    {"ma2room", 2, (float)1556.0, (float)728.46, (float)-7091.0, 0xEAA6, 5, 1, 0, 0}, // FF
    {"M_Dai", 1, (float)-8010.0, (float)1010.0, (float)-1610.0, 0x0000, 5, 1, 1, 1},  // ET
    {"kaze", 3, (float)-4333.0, (float)1100.0, (float)48.0, 0x4000, 5, 1, 2, 1},      // WT
};

const uint16_t kParticles[PCR_WARP_POT_PARTICLE_COUNT] = {0x8161, 0x8162, 0x8165, 0x8166, 0x8112};
// tweaks.py: for dest_jpc in [totg_jpc, ff_jpc]
const int kParticleDests[2] = {50, 43};

bool sInit = false;
int sImports = 0;
PcRandoWarpPotStatus sStatus[PCR_WARP_POT_COUNT];
bool sStatusInit = false;
uint32_t sSkipped = 0;

void resetStatus() {
    for (PcRandoWarpPotStatus& s : sStatus) {
        s = PcRandoWarpPotStatus{0, 0, 0, 0, -3, -3, 0};
    }
    sSkipped = 0;
    sStatusInit = true;
}

// Does the file hold a PLYR with spawn_id (low byte of z_rot, +0x1D) 69? PLYR has no layer chunks.
bool hasSpawn69(const PcRandoDzx* dzx) {
    const uint32_t n = pc_rando_dzx_count(dzx, "PLYR");
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t* e = pc_rando_dzx_entry(dzx, "PLYR", i);
        if (e != nullptr && e[0x1D] == PCR_WARP_POT_SPAWN_ID) {
            return true;
        }
    }
    return false;
}

// The adds of pot i for a file (what: 0 room.dzr, 1 stage.dzs) once dzx is open. Returns the number
// of adds (0: nothing for this file), or -1 when the file is refused.
struct Plan {
    uint8_t plyr[0x20];
    uint8_t scls[3][0xC];
    uint8_t actr[0x20];
    PcRandoDzxAdd adds[3];
    int n;
    uint32_t firstScls;
};

int makePlan(int i, bool stageFile, const PcRandoDzx* dzx, Plan* p) {
    const PcRandoWarpPot& pot = kPots[i];
    p->n = 0;
    p->firstScls = 0;
    if (stageFile) {
        if (!pot.spawnInStage) {
            return 0;
        }
        if (hasSpawn69(dzx)) {
            return -1;
        }
        pc_rando_warp_pot_entries(i, 0, p->plyr, nullptr, nullptr);
        p->adds[p->n++] = PcRandoDzxAdd{"PLYR", p->plyr, 1};
        return p->n;
    }
    if (!pot.spawnInStage && hasSpawn69(dzx)) {
        return -1;
    }
    p->firstScls = pc_rando_dzx_count(dzx, "SCLS");
    if (p->firstScls + 2 > 0xFF) {
        return -1; // the exits are u8 fields (0xFF: none)
    }
    pc_rando_warp_pot_entries(i, p->firstScls, p->plyr, p->scls, p->actr);
    if (!pot.spawnInStage) {
        p->adds[p->n++] = PcRandoDzxAdd{"PLYR", p->plyr, 1};
    }
    p->adds[p->n++] = PcRandoDzxAdd{"SCLS", p->scls, 3};
    p->adds[p->n++] = PcRandoDzxAdd{"ACTR", p->actr, 1};
    return p->n;
}

// The pot whose edit a file takes: room >= 0 the pot of that room, -1 (stage.dzs) the pot whose spawn
// goes in the stage file. -1 when none.
int potFor(const char* stage, int room) {
    if (stage == nullptr) {
        return -1;
    }
    for (int k = 0; k < PCR_WARP_POT_COUNT; k++) {
        if (strcmp(kPots[k].stage, stage) == 0 && (room < 0 ? kPots[k].spawnInStage != 0 : kPots[k].room == room)) {
            return k;
        }
    }
    return -1;
}

void onRando(int active) {
    if (active != 1) {
        sImports = 0; // R7.1b clears its registry itself when pc_rando() turns 0
        return;
    }
    const PcRandoConfig* cfg = pc_rando_cfg();
    if (cfg == nullptr || !cfg->inter_dungeon_warps) {
        sImports = 0;
        return;
    }
    int n = 0;
    for (uint16_t id : kParticles) {
        for (int dest : kParticleDests) {
            n += pc_rando_jpc_scene_add_import(dest, id, PCR_WARP_POT_PARTICLE_SOURCE);
        }
    }
    sImports = n;
    logf("[cos] rando warp pots: %d particle import(s) registered (Pscene035 -> 050, 043)\n", n);
}

} // namespace

extern "C" {

const PcRandoWarpPot* pc_rando_warp_pot(int i) {
    return i >= 0 && i < PCR_WARP_POT_COUNT ? &kPots[i] : nullptr;
}

int pc_rando_warp_pot_find(const char* stage, int room) {
    if (stage == nullptr) {
        return -1;
    }
    for (int i = 0; i < PCR_WARP_POT_COUNT; i++) {
        if (strcmp(kPots[i].stage, stage) == 0 && kPots[i].room == room) {
            return i;
        }
    }
    return -1;
}

int pc_rando_warp_pot_dest(int i, int d) {
    if (i < 0 || i >= PCR_WARP_POT_COUNT || d < 0 || d >= PCR_WARP_POT_CYCLE) {
        return -1;
    }
    return kPots[i].cycle * PCR_WARP_POT_CYCLE + d;
}

int pc_rando_warp_pot_next_dest(int i) {
    if (i < 0 || i >= PCR_WARP_POT_COUNT) {
        return -1;
    }
    return (kPots[i].index + 1) % PCR_WARP_POT_CYCLE;
}

uint32_t pc_rando_warp_pot_params(int i, uint32_t firstScls) {
    if (i < 0 || i >= PCR_WARP_POT_COUNT) {
        return 0;
    }
    const PcRandoWarpPot& pot = kPots[i];
    // actor_parameters.txt d_a_obj_warpt: type 0xF, cyclic_event_reg_index 0xF0, cyclic_dest_1..3_exit
    // 0xFF00, 0xFF0000, 0xFF000000.
    uint32_t v = (uint32_t)(pot.index + 2) & 0xF;
    v |= ((uint32_t)pot.eventReg & 0xF) << 4;
    v |= ((firstScls + 0) & 0xFF) << 8;
    v |= ((firstScls + 1) & 0xFF) << 16;
    v |= ((firstScls + 2) & 0xFF) << 24;
    return v;
}

void pc_rando_warp_pot_name(int i, char out[8]) {
    memset(out, 0, 8);
    if (i >= 0 && i < PCR_WARP_POT_COUNT) {
        snprintf(out, 8, "Warpts%d", kPots[i].index + 1);
    }
}

uint16_t pc_rando_warp_pot_particle(int k) {
    return k >= 0 && k < PCR_WARP_POT_PARTICLE_COUNT ? kParticles[k] : 0;
}

void pc_rando_warp_pot_entries(int i, uint32_t firstScls, uint8_t plyr[0x20], uint8_t scls[3][0xC],
                               uint8_t actr[0x20]) {
    if (i < 0 || i >= PCR_WARP_POT_COUNT) {
        return;
    }
    const PcRandoWarpPot& pot = kPots[i];
    if (plyr != nullptr) {
        // PLYR(): name "Link", unknown_param_4 / evnt_index 0xFF (params 0xFFFF0000), ship_id 0xFF
        // (z_rot high byte); spawn_type 0xF000, room_num 0x3F; spawn_id the z_rot low byte.
        memset(plyr, 0, 0x20);
        memcpy(plyr, "Link", 4);
        put32(plyr + 0x08, 0xFFFF0000u | ((uint32_t)PCR_WARP_POT_SPAWN_TYPE << 12) | ((uint32_t)pot.room & 0x3F));
        putf(plyr + 0x0C, pot.x);
        putf(plyr + 0x10, pot.y);
        putf(plyr + 0x14, pot.z);
        put16(plyr + 0x18, 0);
        put16(plyr + 0x1A, pot.yRot);
        put16(plyr + 0x1C, (uint16_t)(0xFF00 | PCR_WARP_POT_SPAWN_ID));
        put16(plyr + 0x1E, 0xFFFF);
    }
    if (scls != nullptr) {
        for (int d = 0; d < PCR_WARP_POT_CYCLE; d++) {
            const PcRandoWarpPot& to = kPots[pc_rando_warp_pot_dest(i, d)];
            uint8_t* e = scls[d];
            memset(e, 0, 0xC);
            strncpy((char*)e, to.stage, 8);
            e[0x8] = PCR_WARP_POT_SPAWN_ID;
            e[0x9] = (uint8_t)to.room;
            e[0xA] = PCR_WARP_POT_FADE;
            e[0xB] = 0xFF;
        }
    }
    if (actr != nullptr) {
        memset(actr, 0, 0x20);
        char name[8];
        pc_rando_warp_pot_name(i, name);
        memcpy(actr, name, 8);
        put32(actr + 0x08, pc_rando_warp_pot_params(i, firstScls));
        putf(actr + 0x0C, pot.x);
        putf(actr + 0x10, pot.y);
        putf(actr + 0x14, pot.z);
        put16(actr + 0x18, 0xFFFF);
        put16(actr + 0x1A, pot.yRot);
        put16(actr + 0x1C, 0xFFFF);
        put16(actr + 0x1E, 0xFFFF);
    }
}

size_t pc_rando_warp_pots_grown_size(const char* stage, int room, const uint8_t* src, size_t size) {
    const int which = potFor(stage, room);
    if (which < 0 || src == nullptr) {
        return 0;
    }
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, const_cast<uint8_t*>(src), size) != PCR_DZX_OK) {
        return 0;
    }
    Plan p = {};
    if (makePlan(which, room < 0, &dzx, &p) <= 0) {
        return 0;
    }
    return pc_rando_dzx_grown_size(src, size, p.adds, p.n, nullptr);
}

int pc_rando_warp_pots_apply(const char* stage, int room, const uint8_t* src, size_t size, uint8_t* dst,
                             size_t cap, size_t* outSize) {
    const int which = potFor(stage, room);
    if (which < 0 || src == nullptr) {
        return PCR_WARP_POTS_NONE;
    }
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, const_cast<uint8_t*>(src), size) != PCR_DZX_OK) {
        return PCR_WARP_POTS_REFUSED;
    }
    Plan p = {};
    const int n = makePlan(which, room < 0, &dzx, &p);
    if (n < 0) {
        return PCR_WARP_POTS_REFUSED;
    }
    if (n == 0) {
        return PCR_WARP_POTS_NONE;
    }
    uint32_t first[3] = {};
    const int r = pc_rando_dzx_grow(src, size, p.adds, p.n, dst, cap, outSize, first);
    if (r != PCR_DZX_OK) {
        return r == PCR_DZX_NO_SPACE ? PCR_WARP_POTS_FAILED : PCR_WARP_POTS_REFUSED;
    }
    return PCR_WARP_POTS_PATCHED;
}

void pc_rando_tweak_warp_pots_patch(PcRandoResFile* file) {
    if (file == nullptr || file->stage == nullptr || file->arc == nullptr || file->name == nullptr) {
        return;
    }
    const bool stageFile = strcmp(file->arc, "Stage") == 0 && strcmp(file->name, "stage.dzs") == 0;
    const bool roomFile = file->room >= 0 && strcmp(file->name, "room.dzr") == 0;
    if (!stageFile && !roomFile) {
        return;
    }
    const int which = potFor(file->stage, stageFile ? -1 : file->room);
    if (which < 0) {
        return;
    }
    if (!sStatusInit) {
        resetStatus();
    }
    if (!pc_rando() || !pc_rando_cfg()->inter_dungeon_warps) {
        sSkipped++;
        return;
    }
    PcRandoWarpPotStatus& st = sStatus[which];
    (stageFile ? st.stageSeen : st.roomSeen)++;
    int result = PCR_WARP_POTS_REFUSED;
    PcRandoDzx dzx;
    Plan p = {};
    int n = -1;
    if (pc_rando_dzx_open(&dzx, file->data, file->size) == PCR_DZX_OK) {
        n = makePlan(which, stageFile, &dzx, &p);
    }
    if (n > 0) {
        uint32_t first[3] = {};
        const int r = pc_rando_res_grow_dzx(file, p.adds, p.n, first);
        if (r == PCR_DZX_OK) {
            result = PCR_WARP_POTS_PATCHED;
            // the SCLS add is adds[n - 2] for a room file; its first index must be the count used for
            // the params
            if (!stageFile && first[p.n - 2] != p.firstScls) {
                logf("[cos] rando warp pots: %s/%s: SCLS appended at %u, expected %u\n", file->stage, file->arc,
                     (unsigned)first[p.n - 2], (unsigned)p.firstScls);
            }
        } else {
            result = r == PCR_DZX_NO_SPACE ? PCR_WARP_POTS_FAILED : PCR_WARP_POTS_REFUSED;
        }
    } else if (n == 0) {
        result = PCR_WARP_POTS_NONE;
    }
    if (stageFile) {
        st.lastStage = result;
        st.stagePatched += result == PCR_WARP_POTS_PATCHED ? 1 : 0;
    } else {
        st.lastRoom = result;
        st.roomPatched += result == PCR_WARP_POTS_PATCHED ? 1 : 0;
        st.firstScls = p.firstScls;
    }
    if (result == PCR_WARP_POTS_PATCHED) {
        logf("[cos] rando warp pots: %s/%s %s: pot %d added%s (SCLS from %u)\n", file->stage, file->arc, file->name,
             which, stageFile ? " (spawn 69)" : "", stageFile ? 0u : (unsigned)p.firstScls);
    } else {
        logf("[cos] rando warp pots: %s/%s %s: left vanilla (%d)\n", file->stage, file->arc, file->name, result);
    }
}

void pc_rando_tweak_warp_pots_init(void) {
    if (sInit) {
        return;
    }
    sInit = true;
    if (!sStatusInit) {
        resetStatus();
    }
    pc_rando_register_dzx_patcher(pc_rando_tweak_warp_pots_patch);
    pc_rando_add_listener(onRando); // called with 1 at once when already active
}

int pc_rando_warp_pots_imports_registered(void) {
    return sImports;
}

void pc_rando_warp_pots_status(int i, PcRandoWarpPotStatus* out) {
    if (out == nullptr) {
        return;
    }
    if (!sStatusInit) {
        resetStatus();
    }
    if (i < 0 || i >= PCR_WARP_POT_COUNT) {
        *out = PcRandoWarpPotStatus{0, 0, 0, 0, -3, -3, 0};
        return;
    }
    *out = sStatus[i];
}

void pc_rando_warp_pots_reset_status(void) {
    resetStatus();
}

uint32_t pc_rando_warp_pots_skipped(void) {
    if (!sStatusInit) {
        resetStatus();
    }
    return sSkipped;
}

} // extern "C"
