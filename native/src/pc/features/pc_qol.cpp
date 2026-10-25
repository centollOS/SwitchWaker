// Quality-of-life options (native/include/pc/pc_qol.h, docs/QOL_PLAN.md): their values. The game's
// hunks that act on them are listed in the plan; the menu rows are in pc_menu.cpp.
#include "pc_internal.h"

#include "pc/pc_qol.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {

struct Option {
    const char* key;
    int value; // -1: not read yet
};

Option sOptions[PC_QOLS] = {
    {"COS_QOL_INSTANT_TEXT", -1},  {"COS_QOL_FAST_ROLL", -1},     {"COS_QOL_FAST_BLOCKS", -1},
    {"COS_QOL_FAST_GRAPPLE", -1},  {"COS_QOL_FAST_CLIMB", -1},    {"COS_QOL_FAST_BALLAD", -1},
    {"COS_QOL_REVEAL_CHART", -1},  {"COS_QOL_TINGLE_CHESTS", -1},
    {"COS_QOL_SAIL", -1},          {"COS_QOL_SWING_TURN", -1},    {"COS_QOL_NO_SONG_REPLAY", -1},
    {"COS_QOL_FREE_BOAT", -1},     {"COS_QOL_SKIP_INTRO", -1},    {"COS_QOL_FIX_BUGS", -1},
    {"COS_QOL_GANON", -1},
};

int parse(int which, const char* v) {
    if (v == nullptr) {
        return 0;
    }
    if (which == PC_QOL_SAIL) {
        if (strcmp(v, "swift") == 0) {
            return PC_QOL_SAIL_SWIFT;
        }
        if (strcmp(v, "brisk") == 0) {
            return PC_QOL_SAIL_BRISK;
        }
        if (strcmp(v, "wind") == 0) {
            return PC_QOL_SAIL_WIND;
        }
        return 0;
    }
    return strcmp(v, "1") == 0 ? 1 : 0;
}

} // namespace

extern "C" {

int pc_qol(PcQol which) {
    Option& o = sOptions[which];
    if (o.value < 0) {
        const char* v = getenv(o.key);
        o.value = parse(which, v);
        if (o.value != 0) {
            pc::writef(STDERR_FILENO, "[cos] qol: %s=%s\n", o.key, v);
        }
    }
    return o.value;
}

int pc_qol_fast_sail(void) {
    const int sail = pc_qol(PC_QOL_SAIL);
    return sail == PC_QOL_SAIL_SWIFT || sail == PC_QOL_SAIL_BRISK ? sail : 0;
}

float pc_qol_ganon_walk(void) {
    return pc_qol(PC_QOL_GANON) ? 0.8f : 1.0f;
}

float pc_qol_ganon_lunge(void) {
    return pc_qol(PC_QOL_GANON) ? 2.0f : 1.0f;
}

float pc_qol_ganon_gravity(void) {
    return pc_qol(PC_QOL_GANON) ? 1.5f : 1.0f;
}

const char* pc_qol_key(PcQol which) {
    return sOptions[which].key;
}

int pc_qol_set(const char* key, const char* value) {
    for (int i = 0; i < PC_QOLS; i++) {
        if (strcmp(sOptions[i].key, key) == 0) {
            sOptions[i].value = parse(i, value); // pc_settings logs the change
            return 1;
        }
    }
    return 0;
}

} // extern "C"
