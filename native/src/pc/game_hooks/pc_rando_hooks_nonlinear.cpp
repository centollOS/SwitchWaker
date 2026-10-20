// Randomizer hook bodies for the nonlinear patches (docs/RANDOMIZER_PLAN.md step R3.9; wwrando
// asm/patches/make_game_nonlinear.asm). Called only under pc_rando() from the marked hunks in
// game/src/d/actor/d_a_npc_cb1.cpp and d_a_npc_md.cpp (docs/RANDO_HOOKS.md, "R3.9").
#include "pc/pc_rando_hooks.h"

#include "d/d_com_inf_game.h"

namespace {

// make_game_nonlinear.asm makar_possible_wt_spawn_positions / medli_possible_et_spawn_positions:
// spawn ID, room index, rotation, position.
struct PartnerSpawn {
    s16 point;
    s8 room;
    u16 angle;
    float x, y, z;
};

constexpr PartnerSpawn kMakarWindTemple[] = {
    {15, 0xF, 0x94A0, -3651.02f, 1557.67f, 13235.2f},   // Wind Temple entrance
    {22, 0, 0x4000, -4196.33f, 754.518f, 7448.5f},      // first warp pot
    {23, 2, 0xB000, 668.107f, 1550.0f, 2298.75f},       // second warp pot
    {24, 12, 0xC000, 14203.1f, -5062.49f, 8948.05f},    // third warp pot
    {69, 3, 0x4000, -4146.65f, 1100.0f, 47.88f},        // inter-dungeon warp pot
    {20, 2, 0x0000, -111.29f, -3600.0f, -1747.64f},     // miniboss door
    {27, 12, 0x8000, 14084.78f, -5062.49f, 9650.55f},   // boss door
};

constexpr PartnerSpawn kMedliEarthTemple[] = {
    {0, 0, 0x8000, -7215.21f, -200.0f, 5258.79f},       // Earth Temple entrance
    {22, 2, 0xE000, -2013.11f, 200.0f, -1262.97f},      // first warp pot
    {23, 6, 0x8000, 4750.0f, 350.0f, -2251.06f},        // second warp pot
    {24, 15, 0xE000, -2371.38f, -2000.0f, 8471.54f},    // third warp pot
    {69, 1, 0x0000, -8010.0f, 1000.0f, -1508.94f},      // inter-dungeon warp pot
    {9, 7, 0x8000, 6301.06f, 1400.0f, 2355.29f},        // miniboss door
    {27, 15, 0x4000, -5689.21f, -2300.0f, 9667.67f},    // boss door
};

} // namespace

void pc_rando_reset_partner_position(int option) {
    const PartnerSpawn* table = option == 1 ? kMakarWindTemple : kMedliEarthTemple;
    const int count = option == 1 ? (int)(sizeof kMakarWindTemple / sizeof kMakarWindTemple[0])
                                  : (int)(sizeof kMedliEarthTemple / sizeof kMedliEarthTemple[0]);
    const s16 point = dComIfGp_getStartStagePoint();
    const u8 flag = dComIfGs_getPlayerPriestFlag();
    for (int i = 0; i < count; i++) {
        if (table[i].point == point) {
            cXyz pos(table[i].x, table[i].y, table[i].z);
            dComIfGs_setPlayerPriest(flag, pos, (s16)table[i].angle, table[i].room);
            return;
        }
    }
    // No entry for this spawn (a spawn the table misses, a debug warp): wwrando copies the player's
    // position, Y rotation and current room (0x803E440C, 0x803F6F1A, mStayNo) so the partner is not
    // left at another partner's saved position, possibly out of bounds.
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == nullptr) {
        return;
    }
    cXyz pos = player->current.pos;
    dComIfGs_setPlayerPriest(flag, pos, player->shape_angle.y, (s8)dComIfGp_roomControl_getStayNo());
}
