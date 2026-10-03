// Smoke tests of the run harness (COS_SMOKE, docs/NATIVE_PORT_PHASE4_6.md, step 6.0).
//
// Every test that runs before the SDK starts lives here and ends the process:
// - static-init (milestone M0, moved here from m_Do_main.cpp, phase 3 step 3.9): every static
//   constructor (main.dol, REL and audio units, Aurora) ran before main, and the profile list the
//   REL units used to provide is complete and in order;
// - crash-test, panic-test, stall-test, timeout-test: self-tests of the harness that must end
//   with exit 13, 12, 11 and 10.
// After the disc check (runDiscSmoke), before the game starts:
// - disc-ls (step 4.0d): opens COS_DISC through Aurora's DVD layer and lists the whole FST with
//   DVDOpenDir/DVDReadDir, one line per entry, into <COS_RUN_DIR>/disc_ls.txt
//       D <entrynum> <path>               a directory (the root is not listed)
//       F <entrynum> <size> <path>        a file, size from DVDFastOpen's DVDFileInfo
//   native/tools/run.sh then compares it with the manifest native/tools/disc_manifest.py reads
//   from the same image independently (disc_manifest.py --check-ls).
// After the Aurora bring-up (runAuroraSmoke, from pc_aurora_init), before the game's main code:
// - heap (step 4.2, pc_heap.cpp): the JKR heaps on the host.
// After mDoMch_Create (runHeapsSmoke, from pc_heaps_created, once milestone M2's checks held):
// - font (step 4.3, pc_font.cpp): the system font, a disc font and a console line drawn;
// - arc-sweep (step 4.4, pc_arc.cpp): every .arc of the disc mounted in the four JKRArchive modes
//   and compared with an independent reading;
// - msg-sweep (step 4.6, pc_msg.cpp): every message of every BMG decoded through the game's
//   message code, the BMC colour table and the message fonts;
// - jpa-sweep (step 4.7, pc_jpa.cpp): every JPC's emitter resources and textures read through
//   JParticle and compared with an independent reading, every emitter calculated for 30 frames.
// - stage-sweep (step 4.9a, pc_stage.cpp): every dzs/dzr chunk table relocated and decoded through
//   d_stage.cpp, its RTBL and paths relocated by the game's loaders, and /res/Menu/Menu1.dat.
// - blo-sweep (step 4.13, pc_blo.cpp): every BLO screen of the disc built by J2DScreen::set and its
//   panes compared with an independent reading.
// - save (step 4.15, pc_save.cpp): a new save written to a memory card in the run directory,
//   checked as big-endian GCI bytes, reloaded and compared.
// - dzb-sweep (step 4.10, pc_dzb.cpp): every dzb converted by cBgS::ConvDzb and compared with an
//   independent reading, set into a dBgW and probed with downward GroundCross rays.
// - audio-parse (step 5.1, pc_audio.cpp): JaiInit.aaf, every bank, wave system, sequence and
//   stream read through JAudio's init-data code and compared with an independent reading.
// - j3d-sweep (step 4.11, pc_j3d.cpp): every BMD/BDL/BMT of the disc loaded through the game's J3D
//   loaders with the game's flags and compared with an independent reading.
// - anm-sweep (step 4.12, pc_anm.cpp): every J3D animation of the disc loaded as the game loads it
//   and evaluated at its first, middle and last frame through the game's getters.
// - amp-sweep (fix F2-agb-map, pc_amp.cpp): every AGB floor map (m<N>.amp) of the room archives
//   read through dMap_2DAGBScrDsp_c and mDoLib_cnvind16/32 and compared with an independent
//   little-endian reading, and the GBA buffer byte order.
// - stb-sweep (step 4.17, pc_stb.cpp): every STB of the disc parsed by JStudio and played with null
//   adaptors through all its frames, as dDemo_manager_c plays a demo.
// In the booted game:
// - pad-echo (step 6.3, pc_input.cpp): the COS_INPUT script read back from g_mDoCPd_cpadInfo after
//   each mDoCPd_Read.
// - title-audio (step 5.5, pc_title_audio.cpp): after milestone M9 title, the AI output level
//   (above -40 dBFS RMS) and the sequence ticks (advancing) over 300 game frames.
// The format sweeps and the other smoke tests of phases 4-6 add their names to kSmokes; one that
// runs after some of the boot is started by the boot code at that point, not by runEarlySmoke.
#include "pc_internal.h"

#include "f_pc/f_pc_name.h"
#include "f_pc/f_pc_profile.h"

#include <aurora/dvd.h>
#include <dolphin/dvd.h>
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

struct DiscListing {
    int fd;              // disc_ls.txt, or -1 (counts only)
    unsigned int files;
    unsigned int dirs;
    unsigned int errors;
};

// Lists the directory at path (no trailing slash; "" is the root) and its subdirectories.
void listDiscDir(DiscListing& out, const char* path, int depth) {
    DVDDir dir;
    if (!DVDOpenDir(path[0] != '\0' ? path : "/", &dir)) {
        writef(STDERR_FILENO, "[cos] disc-ls: DVDOpenDir(\"%s\") failed\n", path);
        out.errors++;
        return;
    }
    DVDDirEntry entry;
    while (DVDReadDir(&dir, &entry)) {
        char child[512];
        int n = snprintf(child, sizeof(child), "%s/%s", path, entry.name != nullptr ? entry.name : "");
        if (entry.name == nullptr || n <= 0 || n >= (int)sizeof(child)) {
            writef(STDERR_FILENO, "[cos] disc-ls: entry %u under \"%s\" has no usable name\n",
                   (unsigned int)entry.entryNum, path);
            out.errors++;
            continue;
        }
        if (entry.isDir) {
            out.dirs++;
            if (out.fd >= 0) {
                writef(out.fd, "D %u %s\n", (unsigned int)entry.entryNum, child);
            }
            if (depth >= 32) {
                writef(STDERR_FILENO, "[cos] disc-ls: %s is nested too deep\n", child);
                out.errors++;
                continue;
            }
            listDiscDir(out, child, depth + 1);
            continue;
        }
        DVDFileInfo info;
        if (!DVDFastOpen((s32)entry.entryNum, &info)) {
            writef(STDERR_FILENO, "[cos] disc-ls: DVDFastOpen(%u) %s failed\n",
                   (unsigned int)entry.entryNum, child);
            out.errors++;
            continue;
        }
        out.files++;
        if (out.fd >= 0) {
            writef(out.fd, "F %u %u %s\n", (unsigned int)entry.entryNum, (unsigned int)info.length,
                   child);
        }
        DVDClose(&info);
    }
    DVDCloseDir(&dir);
}

// disc-ls: the FST as Aurora's DVD layer presents it to the game. Exit 0 when the disc opened and
// the listing is complete (the comparison with the manifest is native/tools/run.sh's).
[[noreturn]] void smokeDiscLs() {
    if (!aurora_dvd_open(gConfig.disc)) {
        writef(STDERR_FILENO, "[cos] disc-ls: aurora_dvd_open(%s) failed\n", gConfig.disc);
        pc_exit(PC_EXIT_DISC);
    }
    DVDInit();
    const DVDDiskID* id = DVDGetCurrentDiskID();
    char game[7];
    memcpy(game, id->gameName, 4);
    memcpy(game + 4, id->company, 2);
    game[6] = '\0';
    DiscListing out = {openRunFile("disc_ls.txt"), 0, 0, 0};
    if (out.fd >= 0) {
        writef(out.fd, "# disc-ls %s revision %u (COS_SMOKE=disc-ls, DVDReadDir order)\n", game,
               (unsigned int)id->gameVersion);
    }
    listDiscDir(out, "", 0);
    if (out.fd >= 0) {
        close(out.fd);
    }
    aurora_dvd_close();
    writef(STDERR_FILENO, "[cos] disc-ls: %s revision %u: %u files, %u directories, %u error(s)%s\n",
           game, (unsigned int)id->gameVersion, out.files, out.dirs, out.errors,
           gConfig.runDir != nullptr ? " (listing in disc_ls.txt)" : "");
    bool ok = out.errors == 0 && out.files > 0 && strcmp(game, "GZLE01") == 0;
    pc_exit(ok ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

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
        waitForever();
    }
    if (strcmp(name, "timeout-test") == 0) {
        requireWatchdog(name, gConfig.timeoutS, "COS_TIMEOUT_S");
        writef(STDERR_FILENO, "[cos] timeout-test: ticking frames until the timeout\n");
        for (;;) {
            usleep(16 * 1000);
            pc_frame_tick();
        }
    }
    if (strcmp(name, "disc-ls") == 0) {
        smokeDiscLs();
    }
    if (strcmp(name, "heap") == 0) {
        smokeHeap();
    }
    if (strcmp(name, "font") == 0) {
        smokeFont();
    }
    if (strcmp(name, "arc-sweep") == 0) {
        smokeArcSweep();
    }
    if (strcmp(name, "msg-sweep") == 0) {
        smokeMsgSweep();
    }
    if (strcmp(name, "jpa-sweep") == 0) {
        smokeJpaSweep();
    }
    if (strcmp(name, "stage-sweep") == 0) {
        smokeStageSweep();
    }
    if (strcmp(name, "blo-sweep") == 0) {
        smokeBloSweep();
    }
    if (strcmp(name, "save") == 0) {
        smokeSave();
    }
    if (strcmp(name, "dzb-sweep") == 0) {
        smokeDzbSweep();
    }
    if (strcmp(name, "audio-parse") == 0) {
        smokeAudioParse();
    }
    if (strcmp(name, "j3d-sweep") == 0) {
        smokeJ3dSweep();
    }
    if (strcmp(name, "anm-sweep") == 0) {
        smokeAnmSweep();
    }
    if (strcmp(name, "amp-sweep") == 0) {
        smokeAmpSweep();
    }
    if (strcmp(name, "stb-sweep") == 0) {
        smokeStbSweep();
    }
    writef(STDERR_FILENO, "[cos] smoke %s has no runner\n", name);
    pc_exit(PC_EXIT_USAGE);
}

enum SmokeStage {
    kEarly,     // runs from pc_harness_init, before the SDK and the disc check
    kAfterDisc, // runs from pc_harness_init once the disc check passed
    kAfterAurora, // runs from pc_aurora_init once Aurora, the disc and OSInit are up
    kAfterHeaps,  // runs from pc_heaps_created once mDoMch_Create made every heap
    kInGame,      // runs inside the booted game; the code it checks ends the process
};

struct Smoke {
    const char* name;
    SmokeStage stage;
};

const Smoke kSmokes[] = {
    {"static-init", kEarly},
    {"crash-test", kEarly},
    {"panic-test", kEarly},
    {"stall-test", kEarly},
    {"timeout-test", kEarly},
    {"disc-ls", kAfterDisc},
    {"heap", kAfterAurora},
    {"font", kAfterHeaps},
    {"arc-sweep", kAfterHeaps},
    {"msg-sweep", kAfterHeaps},
    {"jpa-sweep", kAfterHeaps},
    {"stage-sweep", kAfterHeaps},
    {"blo-sweep", kAfterHeaps},
    {"save", kAfterHeaps},
    {"dzb-sweep", kAfterHeaps},
    {"audio-parse", kAfterHeaps},
    {"j3d-sweep", kAfterHeaps},
    {"anm-sweep", kAfterHeaps},
    {"amp-sweep", kAfterHeaps},
    {"stb-sweep", kAfterHeaps},
    {"pad-echo", kInGame},
    {"title-audio", kInGame},
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

namespace {

void runSmokeAt(SmokeStage stage) {
    if (gConfig.smoke == nullptr) {
        return;
    }
    const Smoke* smoke = findSmoke(gConfig.smoke);
    if (smoke != nullptr && smoke->stage == stage) {
        runSmoke(smoke->name);
    }
}

} // namespace

void runEarlySmoke() {
    runSmokeAt(kEarly);
}

void runDiscSmoke() {
    runSmokeAt(kAfterDisc);
}

void runAuroraSmoke() {
    runSmokeAt(kAfterAurora);
}

void runHeapsSmoke() {
    runSmokeAt(kAfterHeaps);
}

} // namespace pc
