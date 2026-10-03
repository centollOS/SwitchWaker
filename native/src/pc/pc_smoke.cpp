// Smoke tests of the run harness (COS_SMOKE, docs/NATIVE_PORT_PHASE4_6.md, step 6.0).
//
// Every test that runs before the SDK starts lives here and ends the process:
// - static-init (milestone M0, moved here from m_Do_main.cpp, phase 3 step 3.9): every static
//   constructor (main.dol, REL and audio units, Aurora) ran before main, and the profile list the
//   REL units used to provide is complete and in order;
// - crash-test, panic-test, stall-test, timeout-test: self-tests of the harness that must end
//   with exit 13, 12, 11 and 10.
// The format sweeps and the other smoke tests of phases 4-6 add their names to kSmokes; one that
// runs after some of the boot is started by the boot code at that point, not by runEarlySmoke.
#include "pc_internal.h"

#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_profile.h"

#include <dolphin/os.h>

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

// Checks of step 3.9: g_fpcPf_ProfileList_p points at g_fpcPfLst_ProfileList, the list has
// fpcNm_MAX_NUM_e entries and a NULL terminator, each non-NULL entry i has mProcName == i, and
// fpcPf_Get(i) returns entry i. Returns the number of errors.
int smokeStaticInit() {
    int errors = 0;
    int profiles = 0;

    if (g_fpcPf_ProfileList_p != g_fpcPfLst_ProfileList) {
        fprintf(stderr, "static-init: g_fpcPf_ProfileList_p %p is not g_fpcPfLst_ProfileList %p\n",
                (void*)g_fpcPf_ProfileList_p, (void*)g_fpcPfLst_ProfileList);
        errors++;
    }
    if (g_fpcPfLst_ProfileList[fpcNm_MAX_NUM_e] != NULL) {
        fprintf(stderr, "static-init: the profile list has no NULL at index %d\n", (int)fpcNm_MAX_NUM_e);
        errors++;
    }
    for (int i = 0; i < fpcNm_MAX_NUM_e; i++) {
        process_profile_definition* profile = g_fpcPfLst_ProfileList[i];
        if (fpcPf_Get(i) != profile) {
            fprintf(stderr, "static-init: fpcPf_Get(%d) = %p, list entry %p\n", i, (void*)fpcPf_Get(i),
                    (void*)profile);
            errors++;
        }
        if (profile == NULL) {
            continue;
        }
        profiles++;
        if (profile->mProcName != i) {
            fprintf(stderr, "static-init: profile list entry %d has mProcName %d\n", i, profile->mProcName);
            errors++;
        }
    }

    fprintf(stderr, "static-init: %d of %d profile slots filled, %d error(s)\n", profiles,
            (int)fpcNm_MAX_NUM_e, errors);
    return errors;
}

// A null pointer the compiler cannot see through, so the write really executes and faults
// (a visible null dereference may be compiled to a trap instruction instead).
int* volatile sNullTarget = nullptr;

void requireWatchdog(const char* test, double seconds, const char* var) {
    if (seconds <= 0) {
        writef(STDERR_FILENO, "[cos] smoke %s needs %s > 0\n", test, var);
        pc_exit(PC_EXIT_USAGE);
    }
}

[[noreturn]] void runSmoke(const char* name) {
    if (strcmp(name, "static-init") == 0) {
        int errors = smokeStaticInit();
        if (errors == 0) {
            pc_milestone("static-init");
        }
        pc_exit(errors == 0 ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
    }
    if (strcmp(name, "crash-test") == 0) {
        writef(STDERR_FILENO, "[cos] crash-test: writing through a null pointer\n");
        *sNullTarget = 0x7777;
        writef(STDERR_FILENO, "[cos] crash-test: the write did not fault\n");
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    if (strcmp(name, "panic-test") == 0) {
        OSPanic(__FILE__, __LINE__, "panic-test: deliberate OSPanic");
        writef(STDERR_FILENO, "[cos] panic-test: OSPanic returned\n");
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    if (strcmp(name, "stall-test") == 0) {
        requireWatchdog(name, gConfig.stallS, "COS_STALL_S");
        writef(STDERR_FILENO, "[cos] stall-test: no frame from now on\n");
        for (;;) {
            pause();
        }
    }
    if (strcmp(name, "timeout-test") == 0) {
        requireWatchdog(name, gConfig.timeoutS, "COS_TIMEOUT_S");
        writef(STDERR_FILENO, "[cos] timeout-test: ticking frames until the timeout\n");
        for (;;) {
            usleep(16 * 1000);
            pc_frame_tick();
        }
    }
    writef(STDERR_FILENO, "[cos] smoke %s has no runner\n", name);
    pc_exit(PC_EXIT_USAGE);
}

struct Smoke {
    const char* name;
    bool early; // runs from pc_harness_init, before the SDK and the disc check
};

const Smoke kSmokes[] = {
    {"static-init", true},
    {"crash-test", true},
    {"panic-test", true},
    {"stall-test", true},
    {"timeout-test", true},
};

const Smoke* findSmoke(const char* name) {
    for (const Smoke& s : kSmokes) {
        if (strcmp(s.name, name) == 0) {
            return &s;
        }
    }
    return nullptr;
}

} // namespace

bool isKnownSmoke(const char* name) {
    return findSmoke(name) != nullptr;
}

void printSmokes(int fd) {
    for (const Smoke& s : kSmokes) {
        writef(fd, " %s", s.name);
    }
    writef(fd, "\n");
}

void runEarlySmoke() {
    if (gConfig.smoke == nullptr) {
        return;
    }
    const Smoke* smoke = findSmoke(gConfig.smoke);
    if (smoke != nullptr && smoke->early) {
        runSmoke(smoke->name);
    }
}

} // namespace pc
