/*
 * pc_update.h - the update check and the in-game update (issues #27, #28; docs/AUTO_UPDATE_PLAN.md;
 * native/src/pc/features/pc_update.cpp).
 *
 * A worker thread asks GitHub for the latest release, and on request downloads the running region's
 * zip, checks it and swaps its files in with a journal, so an interrupted install or a new version
 * that does not get past the logo is undone at the next start. No game state and no settings here:
 * the options menu (pc_menu.cpp) reads the state, passes the settings in and draws everything.
 *
 * Where it installs: the Switch's sdmc:/switch/switchwaker, only when the NRO runs from there; on the
 * desktop, COS_UPDATE_DIR (tests). Environment (the Switch: the settings file's [dev] section):
 *   COS_UPDATE_URL      the releases/latest URL (https://, or file:// for tests)
 *   COS_UPDATE_VERSION  the version the check compares with (pretend an older build); on the
 *                       desktop also the build's own version, which has none
 *   COS_UPDATE_DIR      the desktop's install directory
 */
#ifndef PC_UPDATE_H
#define PC_UPDATE_H

#include <stddef.h>
#include <stdint.h>

typedef enum PcUpdateStatus {
    PC_UPDATE_IDLE = 0,     /* nothing asked yet */
    PC_UPDATE_DISABLED,     /* this build never checks (a development version) */
    PC_UPDATE_CHECKING,
    PC_UPDATE_UP_TO_DATE,
    PC_UPDATE_AVAILABLE,    /* latest/title name a newer release */
    PC_UPDATE_CHECK_FAILED, /* error says why */
    PC_UPDATE_DOWNLOADING,  /* done/total bytes */
    PC_UPDATE_INSTALLING,   /* checking the zip and putting its files in place */
    PC_UPDATE_INSTALLED,    /* restart to run it */
    PC_UPDATE_INSTALL_FAILED
} PcUpdateStatus;

/* Why a check or an install failed. */
typedef enum PcUpdateError {
    PC_UPDATE_OK = 0,
    PC_UPDATE_ERR_NETWORK,     /* no connection, DNS, TLS, timeout */
    PC_UPDATE_ERR_SERVER,      /* an HTTP error or an answer that is not a release */
    PC_UPDATE_ERR_NO_ASSET,    /* the release has no zip for this region */
    PC_UPDATE_ERR_NOT_HERE,    /* this copy cannot update itself (another NRO path, no install dir) */
    PC_UPDATE_ERR_NO_SPACE,
    PC_UPDATE_ERR_CHECKSUM,    /* the zip's SHA-256 is not the release's */
    PC_UPDATE_ERR_BAD_ZIP,
    PC_UPDATE_ERR_WRITE        /* the SD card refused a write or a rename */
} PcUpdateError;

/* What happened at this start (pc_update_boot). */
typedef enum PcUpdateBootResult {
    PC_UPDATE_BOOT_NONE = 0,
    PC_UPDATE_BOOT_FIRST_START, /* first start of a version the game installed: confirmed once past the logo */
    PC_UPDATE_BOOT_ROLLED_BACK, /* an interrupted install was undone */
    PC_UPDATE_BOOT_FAILED_BACK  /* the installed version did not get past the logo: the old one is back */
} PcUpdateBootResult;

typedef struct PcUpdateInfo {
    PcUpdateStatus status;
    PcUpdateError error;
    char current[32];  /* the version the check compares with */
    char latest[32];   /* the release's tag */
    char title[96];    /* the release's title */
    char detail[160];  /* the error in English, for the log and the menu's help line */
    uint64_t done;     /* downloaded bytes */
    uint64_t total;    /* the zip's size */
    int canInstall;    /* this copy can update itself (pc_update_install would start) */
    int fromStart;     /* the newest check is the one at start */
    int skipped;       /* that check found a release the player chose not to be told about */
    PcUpdateBootResult boot;
    char bootTag[32];  /* the version of the boot result */
} PcUpdateInfo;

#ifdef __cplusplus
extern "C" {
#endif

/* At the very start, before anything that could crash: finishes or undoes what the journal says
 * (docs/AUTO_UPDATE_PLAN.md, "Start and rollback"). Once per process; later calls do nothing.
 * After a failed first start of a new version it restarts into the old one (the Switch's forwarder)
 * or tells the player to start the game again and exits. */
void pc_update_boot(void);
/* Once per frame on the game thread. afterLogo: a scene after the logo runs (the title, the file
 * select, the game), which confirms a version the game installed. */
void pc_update_frame(int afterLogo);
/* Asks for the latest release in the background. atStart: the check at start, which says nothing
 * about skipTag (the release the player chose not to be told about) or a version that failed to
 * start here. Ignored while a check or an install runs. */
void pc_update_check(int atStart, const char* skipTag);
/* Downloads and installs the release the last check found, in the background. 0 if it cannot start
 * (no newer release found, this copy cannot update itself, already running). */
int pc_update_install(void);
void pc_update_get_info(PcUpdateInfo* out);
/* The release notes of the newest release found (notesFromMarkdown in pc_update_core.h: lines, "# " a
 * heading, "- " an item after two spaces per level) into out, cut to size; the full length. 0: none. */
size_t pc_update_get_notes(char* out, size_t size);
/* Restarts into the installed version: 1 never returns (the Switch's forwarder restarts the
 * program); 0 when the player has to start the game again (hbmenu, the desktop). */
int pc_update_restart(void);
/* 1 if a restart can happen without the player opening the game again (the HOME menu forwarder). */
int pc_update_can_restart(void);

#ifdef __cplusplus
}
#endif

#endif
