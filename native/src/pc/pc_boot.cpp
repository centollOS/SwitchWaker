// Debug stage boot (docs/NATIVE_PORT_PHASE4_6.md, step 6.4, decision H4).
//
// COS_BOOT_STAGE=<stage>:<room>[:<point>[:<layer>]] (point 0 and layer -1 by default). The logo
// scene's dvdWaitDraw (d_s_logo.cpp, TARGET_PC), once every l_*Command synced, starts a new file
// the way the name scene does (dComIfGs_init, dComIfGp_itemDataInit) and requests the PLAY scene
// at that stage the way dScnName_c::changeGameScene does, instead of dComIfG_changeOpeningScene.
// This file parses the request, logs it, and checks that the game honoured it: the next stage the
// request left (pc_boot_stage_requested) and the start stage of the first PLAY scene
// (pc_play_stage_started) must both be the requested one. Reaching a working stage is M12, not
// this harness.
#include "pc_internal.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

PcBootStage sBootStage;
bool sHaveBootStage = false;
bool sRequested = false;
bool sStarted = false;

[[noreturn]] void malformed(const char* why) {
    writef(STDERR_FILENO,
           "[cos] COS_BOOT_STAGE=\"%s\": %s; expected <stage>:<room>[:<point>[:<layer>]], e.g. "
           "sea:44:206 (stage name 1-7 characters [A-Za-z0-9_], room 0..63, point -32768..32767, "
           "layer -1..15)\n",
           gConfig.bootStage, why);
    pc_exit(PC_EXIT_USAGE);
}

// One decimal field of the spec, [lo, hi]; *end must stop at ':' or the end of the string.
int parseField(const char* p, const char** end, long lo, long hi, const char* what) {
    char* e = nullptr;
    errno = 0;
    long v = strtol(p, &e, 10);
    if (e == p || errno != 0 || (*e != ':' && *e != '\0')) {
        writef(STDERR_FILENO, "[cos] COS_BOOT_STAGE: %s is not a number\n", what);
        malformed("bad field");
    }
    if (v < lo || v > hi) {
        writef(STDERR_FILENO, "[cos] COS_BOOT_STAGE: %s %ld out of range\n", what, v);
        malformed("bad field");
    }
    *end = e;
    return (int)v;
}

bool same(const char* stage, int room, int point, int layer) {
    return stage != nullptr && strcmp(stage, sBootStage.stage) == 0 && room == sBootStage.room &&
           point == sBootStage.point && layer == sBootStage.layer;
}

} // namespace

void loadBootStage() {
    const char* spec = gConfig.bootStage;
    if (spec == nullptr) {
        return;
    }
    const char* colon = strchr(spec, ':');
    if (colon == nullptr) {
        malformed("no room");
    }
    size_t len = (size_t)(colon - spec);
    if (len == 0 || len >= sizeof(sBootStage.stage)) {
        malformed("stage name length");
    }
    for (size_t i = 0; i < len; i++) {
        char c = spec[i];
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == '_';
        if (!ok) {
            malformed("stage name character");
        }
    }
    memcpy(sBootStage.stage, spec, len);
    sBootStage.stage[len] = '\0';
    sBootStage.point = 0;
    sBootStage.layer = -1;

    const char* p = colon + 1;
    sBootStage.room = parseField(p, &p, 0, 63, "room");
    if (*p == ':') {
        sBootStage.point = parseField(p + 1, &p, -32768, 32767, "point");
        if (*p == ':') {
            sBootStage.layer = parseField(p + 1, &p, -1, 15, "layer");
            if (*p != '\0') {
                malformed("too many fields");
            }
        }
    }
    sHaveBootStage = true;
    writef(STDERR_FILENO, "[cos] boot-stage: COS_BOOT_STAGE asks for stage %s room %d point %d "
                          "layer %d\n",
           sBootStage.stage, sBootStage.room, sBootStage.point, sBootStage.layer);
}

} // namespace pc

using namespace pc;

extern "C" {

const PcBootStage* pc_boot_stage(void) {
    return sHaveBootStage ? &sBootStage : nullptr;
}

void pc_boot_stage_requested(const char* stage, int room, int point, int layer) {
    if (!sHaveBootStage || sRequested) {
        return;
    }
    sRequested = true;
    writef(STDERR_FILENO, "[cos] boot-stage: new file started, PLAY scene requested at frame %u; "
                          "next stage %s room %d point %d layer %d\n",
           pc_frame_count(), stage != nullptr ? stage : "(null)", room, point, layer);
    if (!same(stage, room, point, layer)) {
        writef(STDERR_FILENO, "[cos] boot-stage: the next stage is not the requested one\n");
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    logoResDone("COS_BOOT_STAGE request made instead of dComIfG_changeOpeningScene");
}

void pc_play_stage_started(const char* stage, int room, int point, int layer) {
    if (!sHaveBootStage || sStarted) {
        return;
    }
    sStarted = true;
    writef(STDERR_FILENO, "[cos] boot-stage: PLAY scene starts stage %s room %d point %d layer %d "
                          "at frame %u\n",
           stage != nullptr ? stage : "(null)", room, point, layer, pc_frame_count());
    if (!sRequested || !same(stage, room, point, layer)) {
        writef(STDERR_FILENO, "[cos] boot-stage: the first PLAY scene is not the requested stage\n");
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    writef(STDERR_FILENO, "[cos] boot-stage: request honoured\n");
}

} // extern "C"
