// Milestone M10 file-select (docs/NATIVE_PORT_PHASE4_6.md, milestone table; boot loop): the
// scripted START on the title (native/check/input/file-select.txt) reaches the name scene
// (fpcNm_NAME_SCENE_e, d_s_name.cpp), which runs the memory card check and then the file select.
//
// dScnName_c::draw calls pc_name_scene_drawn at its end with its main, memory card check and draw
// procedure indices. Once per game frame, fileSelectFrame (pc_frame_end) counts a frame as good
// when the name scene was drawn since the previous frame with a screen up (draw procedure other
// than NoneDraw: the memory card / file error dialog, the file select, name entry or save). After
// 60 good frames in a row it logs the procedures and reports the milestone. The probe changes no
// game state.
#include "pc_internal.h"

#include <unistd.h>

namespace pc {

namespace {

constexpr unsigned int kFileSelectFrames = 60;
constexpr int kNoneDraw = 4; // dScnName_c::DrawProc[4], dScnName_c::NoneDraw

// dScnName_c::DrawProc, in table order.
const char* const kDrawProcs[] = {"FileErrorDraw", "FileSelectDraw", "NameInDraw", "SaveDraw",
                                  "NoneDraw"};

unsigned int sDraws = 0;     // pc_name_scene_drawn calls
unsigned int sSeenDraws = 0; // sDraws at the previous fileSelectFrame
int sMainProc = -1;
int sMemCardProc = -1;
int sDrawProc = -1;
bool sFirstLogged = false;
bool sDone = false;
unsigned int sGoodFrames = 0;

} // namespace

void fileSelectFrame(unsigned int frames) {
    if (sDone) {
        return;
    }
    const bool drawn = sDraws != sSeenDraws;
    sSeenDraws = sDraws;
    if (!drawn) {
        sGoodFrames = 0;
        return;
    }
    if (!sFirstLogged) {
        sFirstLogged = true;
        writef(STDERR_FILENO, "[cos] file-select: name scene first drawn at frame %u\n", frames);
    }
    if (sDrawProc < 0 || sDrawProc == kNoneDraw) {
        sGoodFrames = 0;
        return;
    }
    if (++sGoodFrames < kFileSelectFrames) {
        return;
    }
    sDone = true;
    const char* drawName = sDrawProc < (int)(sizeof(kDrawProcs) / sizeof(kDrawProcs[0]))
                               ? kDrawProcs[sDrawProc]
                               : "?";
    writef(STDERR_FILENO, "[cos] file-select: name scene drew %u frames in a row with %s; main proc "
                          "%d, memory card check proc %d, at frame %u\n",
           sGoodFrames, drawName, sMainProc, sMemCardProc, frames);
    pc_milestone("file-select");
}

} // namespace pc

extern "C" void pc_name_scene_drawn(int mainProc, int memCardCheckProc, int drawProc) {
    pc::sMainProc = mainProc;
    pc::sMemCardProc = memCardCheckProc;
    pc::sDrawProc = drawProc;
    pc::sDraws++;
}
