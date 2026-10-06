// COS_SMOKE=stage-hop: scene changes as the doors make them (dComIfGp_setNextStage), through the
// stages listed in COS_STAGE_HOP ("stage:room:point,stage:room:point,...", visited in order after
// the boot stage), COS_STAGE_HOP_LOOPS times (default 3), kStayFrames in each. Meant to run with
// --heap-check 1 (and the ASan build) to find heap damage from a room change: the console crashed
// freeing a process in JKRExpHeap::recycleFreeBlock (a block header's pointer with its upper
// 32 bits overwritten) after going from Adanmae into Atorizk and straight back (bug B23: Dragon
// Roost's settled ash wrote one SNOW_EFF past dKankyo_snow_Packet::mEff). Without COS_STAGE_HOP
// it does that hop. Exit 0 after the
// last stay with the right stage each time, 1 if a stage did not come.
#include "pc_internal.h"

#include "d/d_com_inf_game.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace pc {

namespace {

constexpr unsigned int kStayFrames = 150;
constexpr unsigned int kArriveFrames = 900;

struct Hop {
    std::string stage;
    int room, point;
};

bool sChecked = false;
bool sOn = false;
std::vector<Hop> sHops;
int sLoops = 3;
int sIndex = -1; // into sHops * sLoops
unsigned int sSince = 0;
bool sArrived = false;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}

} // namespace

void stageHopFrame(unsigned int frames) {
    if (!sChecked) {
        sChecked = true;
        if (gConfig.smoke == nullptr || strcmp(gConfig.smoke, "stage-hop") != 0) {
            return;
        }
        JKRPcHostAllocScope hostAlloc;
        // Default (the regression target): Dragon Roost's ash (bug B23), from Adanmae into Atorizk
        // and back.
        const char* list = getenv("COS_STAGE_HOP");
        if (list == nullptr || list[0] == '\0') {
            list = "Atorizk:0:0,Adanmae:0:1";
        }
        std::string s = list;
        size_t pos = 0;
        while (pos <= s.size()) {
            size_t end = s.find(',', pos);
            std::string item = s.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
            char stage[16] = {};
            int room = 0, point = 0;
            if (sscanf(item.c_str(), "%15[^:]:%d:%d", stage, &room, &point) == 3) {
                sHops.push_back({stage, room, point});
            }
            if (end == std::string::npos) {
                break;
            }
            pos = end + 1;
        }
        if (const char* l = getenv("COS_STAGE_HOP_LOOPS")) {
            sLoops = atoi(l);
        }
        sOn = !sHops.empty();
    }
    if (!sOn || fpcM_Search(isPlayScene, nullptr) == nullptr || dComIfGp_getPlayer(0) == nullptr) {
        return;
    }
    ++sSince;
    if (sIndex < 0) {
        if (sSince >= kStayFrames) {
            sIndex = 0;
            sSince = 0;
            sArrived = false;
            const Hop& h = sHops[0];
            writef(STDERR_FILENO, "[cos] stage-hop: frame %u: to %s room %d point %d\n", frames, h.stage.c_str(),
                   h.room, h.point);
            dComIfGp_setNextStage(h.stage.c_str(), (s16)h.point, (s8)h.room);
        }
        return;
    }
    const Hop& h = sHops[sIndex % sHops.size()];
    if (!sArrived) {
        if (strcmp(dComIfGp_getStartStageName(), h.stage.c_str()) == 0 && sSince > 10) {
            sArrived = true;
            sSince = 0;
            writef(STDERR_FILENO, "[cos] stage-hop: frame %u: in %s\n", frames, h.stage.c_str());
        } else if (sSince >= kArriveFrames) {
            writef(STDERR_FILENO, "[cos] stage-hop: FAIL %s did not come\n", h.stage.c_str());
            pc_exit(PC_EXIT_CHECK_FAILED);
        }
        return;
    }
    if (sSince >= kStayFrames) {
        if (++sIndex >= (int)sHops.size() * sLoops) {
            writef(STDERR_FILENO, "[cos] stage-hop: PASS (%d hops)\n", sIndex);
            sOn = false;
            pc_exit(PC_EXIT_REACHED);
            return;
        }
        sSince = 0;
        sArrived = false;
        const Hop& n = sHops[sIndex % sHops.size()];
        writef(STDERR_FILENO, "[cos] stage-hop: frame %u: to %s room %d point %d\n", frames, n.stage.c_str(), n.room,
               n.point);
        dComIfGp_setNextStage(n.stage.c_str(), (s16)n.point, (s8)n.room);
    }
}

} // namespace pc
