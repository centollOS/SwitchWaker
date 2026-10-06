// The host part of cBgS::ConvDzb (c_bg_s.cpp), moved out of game/ (step G3 of
// docs/GAME_CODE_ORGANIZATION.md): the DZB tables' offsets made pointers, the vertex table swapped
// to host order. Declared in native/include/pc/game_hooks.h.
#include "pc/game_hooks.h"
#include "SSystem/SComponent/c_bg_s.h"
#include "SSystem/SComponent/c_bg_w.h"
#include "JSystem/JUtility/JUTAssert.h"

void cBgS_PcConvDzbTables(cBgD_t* pbgd) {
    // Step 4.10: the table offsets are OFFSET_PTR (4 bytes, self-relative once relocated), as in
    // Dusklight's cBgS::ConvDzb (src/d/d_bg_s.cpp, CC0, ref/dusklight at 40457c6). The alignment
    // checks apply to the file offsets, as on GameCube. The vertex table is swapped to host order
    // once here (the 0x80000000 flag ConvDzb sets guards against a second pass): cBgW and its users read
    // it as host Vec. The other tables stay big-endian and are read through BE(T).
    JUT_ASSERT(0x214, ((s32)pbgd->m_v_tbl.value.value % 4) == 0);
    JUT_ASSERT(0x215, ((s32)pbgd->m_t_tbl.value.value % 2) == 0);
    JUT_ASSERT(0x216, ((s32)pbgd->m_b_tbl.value.value % 2) == 0);
    JUT_ASSERT(0x217, ((s32)pbgd->m_tree_tbl.value.value % 2) == 0);
    JUT_ASSERT(0x218, ((s32)pbgd->m_g_tbl.value.value % 4) == 0);
    JUT_ASSERT(0x219, ((s32)pbgd->m_ti_tbl.value.value % 4) == 0);

    if (pbgd->m_v_tbl.value.value != 0)
        pbgd->m_v_tbl.setBase(pbgd);

    pbgd->m_t_tbl.setBase(pbgd);
    pbgd->m_b_tbl.setBase(pbgd);
    pbgd->m_tree_tbl.setBase(pbgd);
    pbgd->m_g_tbl.setBase(pbgd);
    pbgd->m_ti_tbl.setBase(pbgd);

    for (s32 i = 0; i < pbgd->m_g_num; i++) {
        pbgd->m_g_tbl[i].m_name.setBase(pbgd);
    }

#if TARGET_LITTLE_ENDIAN
    cBgD_Vtx_t* vtx = pbgd->m_v_tbl;
    if (vtx != NULL) {
        for (s32 i = 0; i < pbgd->m_v_num; i++) {
            be_swap(vtx[i].x);
            be_swap(vtx[i].y);
            be_swap(vtx[i].z);
        }
    }
#endif
}
