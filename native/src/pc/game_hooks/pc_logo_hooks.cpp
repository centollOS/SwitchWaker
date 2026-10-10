// The logo scene's run-harness probes and the debug stage boot (d_s_logo.cpp), moved out of game/
// (step G3 of docs/GAME_CODE_ORGANIZATION.md): milestones M5 logo-scene and M6 logo-res (steps
// 4.5 and 4.8) and COS_BOOT_STAGE (step 6.4, decision H4). Declared in
// native/include/pc/game_hooks.h; d_s_logo.cpp calls them under TARGET_PC.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/game_hooks.h"
#include "pc/pc_harness.h"
#include "res/Object/Logo.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_s_logo.h"
#include "f_op/f_op_overlap_mng.h"
#include "f_op/f_op_scene_mng.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_dvd_thread.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/JKernel/JKRArchive.h"

// The logo scene's load commands (d_s_logo.cpp).
extern mDoDvdThd_mountXArchive_c * l_anmCommand;
extern mDoDvdThd_mountXArchive_c * l_fmapCommand;
extern mDoDvdThd_mountXArchive_c * l_itemResCommand;
extern mDoDvdThd_mountXArchive_c * l_fmapResCommand;
extern mDoDvdThd_mountXArchive_c * l_dmapResCommand;
extern mDoDvdThd_mountXArchive_c * l_clctResCommand;
extern mDoDvdThd_mountXArchive_c * l_optResCommand;
extern mDoDvdThd_mountXArchive_c * l_saveResCommand;
extern mDoDvdThd_mountXArchive_c * l_clothResCommand;
extern mDoDvdThd_mountXArchive_c * l_itemiconCommand;
extern mDoDvdThd_mountXArchive_c * l_actioniconCommand;
extern mDoDvdThd_mountXArchive_c * l_scopeResCommand;
extern mDoDvdThd_mountXArchive_c * l_camResCommand;
extern mDoDvdThd_mountXArchive_c * l_swimResCommand;
extern mDoDvdThd_mountXArchive_c * l_windResCommand;
extern mDoDvdThd_mountXArchive_c * l_nameResCommand;
extern mDoDvdThd_mountXArchive_c * l_tmsgCommand;
#if VERSION > VERSION_DEMO
extern mDoDvdThd_mountXArchive_c * l_dmsgCommand;
#endif
extern mDoDvdThd_mountXArchive_c * l_errorResCommand;
extern mDoDvdThd_mountXArchive_c * l_msgDtCommand;
#if VERSION > VERSION_JPN
extern mDoDvdThd_mountXArchive_c * l_msgDtCommand2;
#endif
extern mDoDvdThd_mountXArchive_c * l_msgCommand;
extern mDoDvdThd_mountXArchive_c * l_menuCommand;
extern mDoDvdThd_mountXArchive_c * l_fontCommand;
extern mDoDvdThd_mountXArchive_c * l_rubyCommand;
extern mDoDvdThd_toMainRam_c * l_particleCommand;
extern mDoDvdThd_toMainRam_c * l_itemTableCommand;
extern mDoDvdThd_toMainRam_c * l_ActorDataCommand;
extern mDoDvdThd_toMainRam_c * l_FmapDataCommand;
#if VERSION == VERSION_DEMO
extern mDoDvdThd_callback_c * l_DmcMountCommand;
#else
extern mDoDvdThd_mountXArchive_c * l_lodCommand;
#endif

// M6 logo-res (step 4.8): reports the object archives the logo scene keeps resident (every file
// of System, Logo, Always, Link and Agb must have been converted by dRes_info_c::loadResource)
// and the archives and files the l_*Commands read. pc_logo_res_synced exits 1 on a gap.
static void pcLogoResSynced() {
    // dvdWaitDraw keeps running until the scene change happens: report the first time only.
    static bool reported = false;
    if (reported) {
        return;
    }
    reported = true;
    static const char* const objectArcs[] = {"System", "Logo", "Always", "Link", "Agb"};
    int missing = 0;
    for (int i = 0; i < ARRAY_SIZE(objectArcs); i++) {
        dRes_info_c* info = dComIfG_getObjectResInfo(objectArcs[i]);
        JKRArchive* arc = info != NULL ? info->getArchive() : NULL;
        int entries = arc != NULL ? info->getResNum() : 0;
        int files = 0;
        int loaded = 0;
        for (int j = 0; j < entries; j++) {
            // The entries include the directories' "." and ".." links; only files are resources.
            if (arc->isFileEntry(j)) {
                files++;
                if (info->getRes(j) != NULL) {
                    loaded++;
                }
            }
        }
        pc_logo_res_object(objectArcs[i], files, loaded);
    }
    const mDoDvdThd_mountXArchive_c* const arcCommands[] = {
        l_anmCommand, l_fmapCommand, l_itemResCommand, l_fmapResCommand, l_dmapResCommand,
        l_clctResCommand, l_optResCommand, l_saveResCommand, l_clothResCommand, l_itemiconCommand,
        l_actioniconCommand, l_scopeResCommand, l_camResCommand, l_swimResCommand,
        l_windResCommand, l_nameResCommand, l_tmsgCommand,
#if VERSION > VERSION_DEMO
        l_dmsgCommand,
#endif
        l_errorResCommand, l_msgDtCommand,
#if VERSION > VERSION_JPN
        l_msgDtCommand2,
#endif
        l_msgCommand, l_menuCommand, l_fontCommand, l_rubyCommand,
#if VERSION != VERSION_DEMO
        l_lodCommand,
#endif
    };
    int arcs = 0;
    for (int i = 0; i < ARRAY_SIZE(arcCommands); i++) {
        if (arcCommands[i]->getArchive() != NULL) {
            arcs++;
        } else {
            missing++;
        }
    }
    const mDoDvdThd_toMainRam_c* const fileCommands[] = {
        l_particleCommand, l_itemTableCommand, l_ActorDataCommand, l_FmapDataCommand,
    };
    int files = 0;
    for (int i = 0; i < ARRAY_SIZE(fileCommands); i++) {
        if (fileCommands[i]->getMemAddress() != NULL) {
            files++;
        } else {
            missing++;
        }
    }
    pc_logo_res_synced(arcs, files, missing);
}

// Set once the PLAY scene request was accepted; dScnLogo_Create clears it (a reset comes back
// through a new logo scene).
static bool l_pcBootStageRequested = false;

// Debug stage boot (step 6.4, decision H4: COS_BOOT_STAGE). Called by dvdWaitDraw instead of
// dComIfG_changeOpeningScene, every frame until the scene change happens. It does what the new
// game flow does between here and the PLAY scene, minus the title, file select, name entry and
// intro scenes: a new file as dScnName_c::NameInMain makes it (dComIfGs_init sets the default
// name and return place; dComIfGp_itemDataInit), then the requested next stage in place of
// dComIfGs_gameStart's return place, and the scene request of dScnName_c::changeGameScene.
static void pcBootStage(dScnLogo_c* i_this) {
    if (l_pcBootStageRequested || fopOvlpM_IsPeek()) {
        return;
    }
    dComIfGs_init();
    dComIfGp_itemDataInit();
    // COS_BOOT_EVENTS: story event bits for a boot later in the story (bug B6: 2A80 makes Aryll
    // give the telescope on the lookout, which leads to the zelda_fly event).
    unsigned short bits[16];
    int nbits = pc_boot_event_bits(bits, 16);
    for (int i = 0; i < nbits; i++) {
        dComIfGs_onEventBit(bits[i]);
    }
    // COS_BOOT_ITEMS: items given as a chest or an NPC gives them (execItemGet), the first on X
    // (bug B6: 20, the telescope, which Link then raises with X).
    unsigned short items[16];
    int nitems = pc_boot_items(items, 16);
    // The first item goes on X: looked for right after it is given (an empty bottle, 50, before a
    // later item fills it), else once every item is given (all-purpose bait, 82, lands in the bait
    // bag only once the bag, 2C, is given too); the bags count too, as the item menu's bag pages
    // put a bait or a spoil on a button.
    bool onX = false;
    for (int i = 0; i <= nitems; i++) {
        if (i < nitems) {
            execItemGet((u8)items[i]);
        }
        if (onX || (i != 0 && i != nitems)) {
            continue;
        }
        for (int slot = 0; nitems > 0 && slot < dInvSlot_ReserveLast_e; slot++) {
            if (dComIfGs_getItem(slot) == items[0]) {
                dComIfGs_setSelectItem(dItemBtn_X_e, (u8)slot);
                onX = true;
                break;
            }
        }
    }
    // COS_BOOT_PRESET (pc_preset.cpp): a story preset, e.g. sailing (the boat, its sail, the wind
    // baton and song).
    pc_boot_preset_apply();
    const PcBootStage* boot = pc_boot_stage();
    dComIfGp_offEnableNextStage();
    dComIfGp_setNextStage(boot->stage, boot->point, boot->room, boot->layer);
    if (fopScnM_ChangeReq(i_this, fpcNm_PLAY_SCENE_e, fpcNm_OVERLAP0_e, 5)) {
        l_pcBootStageRequested = true;
        dComIfGs_resetDan();
        dComIfGs_setRestartRoomParam(0);
        mDoAud_setSceneName(dComIfGp_getNextStageName(), dComIfGp_getNextStageRoomNo(),
                            dComIfGp_getNextStageLayer());
        // The skipped opening is a PLAY scene (sea_T) whose camera registers the audio camera
        // (d_camera.cpp init_phase1 and camera_draw: mDoAud_getCameraInfo); JAIZelBasic::initSe
        // left it NULL, and the game never clears it again, so every stage of the real game
        // starts with one already set. Without it, JAIZelBasic::zeldaGFrameWork (stage BGM
        // 0x35, Hyrule) and JAInter::SeMgr::checkNextFrameSe (a positional SE before the new
        // camera's first draw) read through NULL. Register it the way init_phase1 does: the
        // "no camera yet" eye (1e7, 1e7, 1e7) and j3dSys's view matrix, for camera 0. The eye
        // is static: init_phase1's is a stack local the game reads after it returned.
        static Vec l_pcBootAudioCameraEye = {10000000.0f, 10000000.0f, 10000000.0f};
        mDoAud_getCameraInfo(&l_pcBootAudioCameraEye, j3dSys.getViewMtx(), 0);
        pc_boot_stage_requested(dComIfGp_getNextStageName(), dComIfGp_getNextStageRoomNo(),
                                dComIfGp_getNextStagePoint(), dComIfGp_getNextStageLayer());
    }
}

bool pc_logo_dvd_synced(dScnLogo_c* i_this) {
    // Run harness (step 4.8): milestone M6 logo-res. Every l_*Command has synced; this checks
    // what they and the object archives left, then dComIfG_changeOpeningScene logs the
    // milestone when it is called.
    pcLogoResSynced();
    if (pc_boot_stage() != NULL) {
        // Debug stage boot (step 6.4): straight to the PLAY scene instead of the opening.
        pcBootStage(i_this);
        return true;
    }
    return false;
}

void pc_logo_scene_created_hook() {
    // Debug stage boot (step 6.4): this logo scene has not made its request yet.
    l_pcBootStageRequested = false;

    // Run harness (step 4.5): milestone M5 logo-scene, the scene is created. It checks the Logo
    // archive and the Nintendo logo's header, then waits for Aurora's first texture upload.
    dRes_info_c* logoInfo = dComIfG_getObjectResInfo("Logo");
    JKRArchive* logoArc = logoInfo != NULL ? logoInfo->getArchive() : NULL;
    ResTIMG* timg = (ResTIMG *)dComIfG_getObjectRes("Logo", dRes_INDEX_LOGO_BTI_NINTENDO_376X104_e);
    pc_logo_scene_created(logoArc != NULL ? logoInfo->getResNum() : 0, timg,
                          logoArc != NULL && timg != NULL ? logoArc->getResSize(timg) : 0);
}
