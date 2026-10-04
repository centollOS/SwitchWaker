// COS_ASPECT: the widescreen option (native/include/pc/pc_aspect.h, docs/WIDESCREEN.md).
#include "pc/pc_aspect.h"

#include "pc_internal.h"

#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {

struct Aspect {
    const char* name;
    float ratio;
};

constexpr Aspect kAspects[] = {
    {"4:3", 4.0f / 3.0f},
    {"16:9", 16.0f / 9.0f},
    {"16:10", 16.0f / 10.0f},
};

// -1 until the first call reads COS_ASPECT (pc_harness_init, before any game thread starts).
int sAspect = -1;
// Set when COS_ASPECT names no aspect; pc_aspect_init reports it and exits.
const char* sBadValue = nullptr;

int parse() {
    const char* v = getenv("COS_ASPECT");
    if (v == nullptr || v[0] == '\0') {
        return PC_ASPECT_4_3;
    }
    for (int i = 0; i < (int)(sizeof(kAspects) / sizeof(kAspects[0])); i++) {
        if (strcmp(v, kAspects[i].name) == 0) {
            return i;
        }
    }
    sBadValue = v;
    return PC_ASPECT_4_3;
}

} // namespace

extern "C" {

int pc_aspect(void) {
    if (sAspect < 0) {
        sAspect = parse();
    }
    return sAspect;
}

const char* pc_aspect_name(void) {
    return kAspects[pc_aspect()].name;
}

float pc_aspect_ratio(void) {
    return kAspects[pc_aspect()].ratio;
}

float pc_aspect_t(void) {
    const float a43 = kAspects[PC_ASPECT_4_3].ratio;
    return (pc_aspect_ratio() - a43) / (kAspects[PC_ASPECT_16_9].ratio - a43);
}

void pc_aspect_init(void) {
    pc_aspect();
    if (sBadValue != nullptr) {
        pc::writef(STDERR_FILENO, "[cos] COS_ASPECT=\"%s\" is not 4:3, 16:9 or 16:10\n", sBadValue);
        pc_exit(PC_EXIT_USAGE);
    }
    if (pc_aspect_wide()) {
        pc::writef(STDERR_FILENO, "[cos] aspect: %s (widescreen, t=%.4f)\n", pc_aspect_name(),
                   (double)pc_aspect_t());
    }
}

} // extern "C"
