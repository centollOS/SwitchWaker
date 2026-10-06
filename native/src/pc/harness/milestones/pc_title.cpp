// Milestone M9 title (docs/NATIVE_PORT_PHASE4_6.md, milestone table; step 4.14, boot loop): the
// d_a_title actor is created and drawing its BLO screen and its JPA particles.
//
// daTitle_proc_c::proc_draw calls pc_title_drawn after it drew the title_logo BLO screen. Once per
// game frame, titleFrame (pc_frame_end) reads game state only and counts a frame as good when:
// - the d_a_title actor (fpcNm_TITLE_e) exists and finished creating, with its daTitle_proc_c,
//   its J2DScreen and the logo pane (pane[0]);
// - its screen was drawn since the previous frame (pc_title_drawn);
// - the logo pane is fully faded in: its J2DPane alpha equals the pane's initial (BLO) alpha,
//   which is nonzero;
// - its title smoke emitter (dComIfGp_particle_set2Dback, ID_AK_S1_TITLESMOKE00) is set and has
//   live particles, and its sparkle emitter (set2Dfore, ID_AK_S2_TITLEKIRAKIRA00) was set once.
// After 60 good frames in a row it logs the state and reports the milestone. The probe changes no
// game state.
#include "pc_internal.h"

#include "d/actor/d_a_title.h"
#include "JSystem/J2DGraph/J2DPane.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "f_op/f_op_actor_iter.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kTitleFrames = 60;

unsigned int sDraws = 0;     // pc_title_drawn calls
unsigned int sSeenDraws = 0; // sDraws at the previous titleFrame
bool sCreatedLogged = false;
bool sSparkleSet = false;
bool sDone = false;
unsigned int sGoodFrames = 0;

int findTitle(void* proc, void* data) {
    fopAc_ac_c* actor = (fopAc_ac_c*)proc;
    if (fopAcM_GetName(actor) == fpcNm_TITLE_e && !fpcM_IsCreating(fopAcM_GetID(actor))) {
        *(daTitle_c**)data = (daTitle_c*)actor;
        return FALSE;
    }
    return TRUE;
}

} // namespace

void titleFrame(unsigned int frames) {
    if (sDone) {
        return;
    }
    const bool drawn = sDraws != sSeenDraws;
    sSeenDraws = sDraws;

    daTitle_c* title = nullptr;
    fopAcIt_Executor(findTitle, &title);
    daTitle_proc_c* proc = title != nullptr ? title->mpTitleProc : nullptr;
    if (proc == nullptr || proc->m_Screen == nullptr || proc->pane[0].pane == nullptr) {
        sGoodFrames = 0;
        return;
    }
    if (!sCreatedLogged) {
        sCreatedLogged = true;
        writef(STDERR_FILENO, "[cos] title: d_a_title created at frame %u\n", frames);
    }
    if (proc->mpEmitter2 != nullptr) {
        sSparkleSet = true;
    }

    const u8 initAlpha = proc->pane[0].mInitAlpha;
    const u8 logoAlpha = proc->pane[0].pane->getAlpha();
    const int smokeParticles = proc->mpEmitter != nullptr ? proc->mpEmitter->getParticleNumber() : 0;
    if (!drawn || initAlpha == 0 || logoAlpha != initAlpha || smokeParticles <= 0 || !sSparkleSet) {
        sGoodFrames = 0;
        return;
    }
    if (++sGoodFrames < kTitleFrames) {
        return;
    }
    sDone = true;
    writef(STDERR_FILENO, "[cos] title: %u frames drawn with the logo at alpha %u, %d title smoke "
                          "particle(s), sparkle emitter set; enter mode %d at frame %u\n",
           sGoodFrames, (unsigned int)logoAlpha, smokeParticles, (int)proc->mEnterMode, frames);
    pc_milestone("title");
}

bool titleReached() {
    return sDone;
}

} // namespace pc

extern "C" void pc_title_drawn(void) {
    pc::sDraws++;
}
