/*
 * pc_settings.h - the persistent settings of the native port and the registry behind the in-game
 * options menu (native/src/pc/features/pc_settings.cpp, pc_menu.cpp; native/README.md, "Options menu").
 *
 * Every setting is named by the environment variable that already selects it (COS_FB_SCALE,
 * COS_DOF, ...), so the menu, the settings file and the environment on the Mac all speak the same
 * names. Where a value comes from, highest first:
 *   1. on the Mac and Linux, the process environment at start (test kits and run.sh keep working
 *      unchanged). Such a setting is locked: the menu shows it with a note and does not change
 *      it. The Switch has no such environment: nothing is locked there.
 *   2. the settings file: <user dir>/settings.ini (the Switch: /switch/switchwaker/native/
 *      user/settings.ini on the SD card; the Mac: user/settings.ini next to the executable;
 *      COS_SETTINGS=<path> names another file, run.sh gives each run its own). Lines
 *      NAME=value, or NAME@handheld=value / NAME@docked=value for a setting with one value per
 *      operation mode; '#' starts a comment.
 *   3. the platform default (the Switch's setDefault list in cos_switch.cpp, or the code's own).
 * The file may end with a [dev] section: everything after a "[dev]" line, NAME=value lines of
 * developer variables (COS_TRACE, COS_BOOT_STAGE, MESA_..., any variable the code reads) that
 * have no row in the menu. They are set in the environment at start, after the menu's values; a
 * menu setting there is ignored (with a log line). The menu writes the section back as it was.
 * pc_settings_load_early copies the file's values into the environment (setenv) before anything
 * reads it, so code that reads its variable once at start sees the file's value without knowing
 * about this file. Later changes do not touch the environment: they reach the code through the
 * setting's apply function or a subscriber.
 *
 * Operation mode: the Switch reports docked or handheld (appletGetOperationMode) and the menu
 * applies the matching values of the per-mode settings when it changes at run time. The Mac is
 * handheld unless COS_OPERATION_MODE=docked (a debug override).
 *
 * For other modules (e.g. the HD texture loader, lane hd-textures): call pc_settings_register for
 * an option of your own (it appears in the menu), or pc_settings_get / pc_settings_subscribe for an
 * option the menu already lists (COS_HD_TEXTURES: "0" or "1"). Registering a key again replaces its
 * description and apply function; values and subscribers stay. All functions run on the game
 * thread, except pc_settings_load_early (before any game thread starts) and pc_settings_get, which
 * any thread may call once registration is over.
 */
#ifndef PC_SETTINGS_H
#define PC_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PcSettingTab {
    PC_SETTING_TAB_GRAPHICS = 0,    /* "Gráficos" */
    PC_SETTING_TAB_PERFORMANCE = 1, /* "Rendimiento" */
    PC_SETTING_TAB_DEBUG = 2,       /* "Depuración" */
    PC_SETTING_TABS = 3
} PcSettingTab;

typedef enum PcOperationMode {
    PC_MODE_HANDHELD = 0, /* "Portátil" */
    PC_MODE_DOCKED = 1,   /* "Sobremesa" */
    PC_MODES = 2
} PcOperationMode;

/* Flags of a setting. */
#define PC_SETTING_RESTART 0x1u   /* takes effect at the next start ("requiere reiniciar") */
#define PC_SETTING_PER_MODE 0x2u  /* one value per operation mode */
#define PC_SETTING_NO_SAVE 0x4u   /* a value for this run only, never written to the file */
#define PC_SETTING_SWITCH_ONLY 0x8u /* shown on the Mac, but only the Switch acts on it */

typedef struct PcSettingChoice {
    const char* value; /* as in the environment variable, e.g. "1.5" */
    const char* label; /* what the menu shows in Spanish, e.g. "1280x720 (1.5)" */
    const char* labelEn; /* the same in English (NULL: label) */
} PcSettingChoice;

/* Called with the setting's new effective value (a value of the current operation mode). */
typedef void (*PcSettingApplyFn)(const char* key, const char* value, void* user);

typedef struct PcSettingDesc {
    const char* key;          /* the environment variable, e.g. "COS_HD_TEXTURES" */
    const char* label;        /* Spanish label */
    const char* help;         /* Spanish one-line description, shown for the selected row */
    PcSettingTab tab;
    unsigned int flags;       /* PC_SETTING_* */
    const PcSettingChoice* choices; /* the values the menu cycles through */
    int choiceCount;
    const char* defaultValue; /* when neither the environment nor the file has one (NULL: "") */
    PcSettingApplyFn apply;   /* live effect of a change (NULL: none, e.g. PC_SETTING_RESTART) */
    void* user;
    int order;                /* rows sort by order within a tab (built-in rows use 0-999) */
    const char* labelEn;      /* English label and description (NULL: the Spanish ones); the menu */
    const char* helpEn;       /* shows the console's language, pc_ui_spanish() */
} PcSettingDesc;

/* Adds (or replaces) a setting; the strings and choices must outlive the program. 0 on success. */
int pc_settings_register(const PcSettingDesc* desc);
/* The value in effect now (the current operation mode's for a per-mode setting): the locked
   environment value, else the file's, else the environment's platform default, else the
   description's default; "" when there is none. Never NULL. */
const char* pc_settings_get(const char* key);
/* The value a per-mode setting has (or will have) in that mode; pc_settings_get otherwise. */
const char* pc_settings_get_mode(const char* key, PcOperationMode mode);
/* Sets the value (for `mode` if the setting is per mode; mode is ignored otherwise), saves the
   file, and when the value in effect changes calls the apply function and the subscribers.
   -1 if the key is unknown or locked. */
int pc_settings_set_mode(const char* key, PcOperationMode mode, const char* value);
int pc_settings_set(const char* key, const char* value); /* for the current mode */
/* 1 if the environment fixed the value at start (the Mac's and Linux's; never on the Switch). */
int pc_settings_locked(const char* key);
/* 1 if a PC_SETTING_RESTART setting now differs from the value this run started with. */
int pc_settings_restart_pending(const char* key);
/* Calls fn(key, value, user) after every change of the value in effect (also on a mode change). */
int pc_settings_subscribe(const char* key, PcSettingApplyFn fn, void* user);
/* 1 if anyone subscribed to the key. */
int pc_settings_has_subscriber(const char* key);

/* The current operation mode, and its Spanish name ("Portátil" / "Sobremesa"). */
PcOperationMode pc_settings_mode(void);
const char* pc_settings_mode_name(PcOperationMode mode);
/* The game thread, once per frame: notices a docked/handheld change and applies the per-mode
   values of the new mode. */
void pc_settings_poll_mode(void);

/* Reads the settings file and copies its values, then its [dev] variables, into the environment
   for every variable the environment does not set yet; remembers which ones it did set (locked).
   Once per process: the Switch calls it before its defaults, pc_harness_init calls it again (a
   no-op). */
void pc_settings_load_early(void);
/* Moves an old run-options file (the Switch's native/env.txt: NAME=value lines, '#' comments) into
   the settings file: a menu setting's line becomes that setting (one value for both modes), any
   other variable a [dev] line; comments and blank lines are dropped. The file is then renamed to
   oldPath (an older oldPath is replaced). Before pc_settings_load_early. 0: no such file, 1:
   migrated, -1: the settings file could not be written (envPath is left in place). */
int pc_settings_migrate_env_file(const char* envPath, const char* oldPath);
/* A platform default with one value per operation mode (the Switch: COS_FB_SCALE 1.5 handheld,
   2.25 docked): used for a per-mode setting when the file has no value for that mode, before the
   environment's single platform default. */
void pc_settings_set_mode_default(const char* key, PcOperationMode mode, const char* value);
/* Re-reads the file (menu: "Recargar ajustes") and applies what changed. */
void pc_settings_reload(void);
/* The settings file's path. */
const char* pc_settings_path(void);

/* Iteration for the menu: the number of registered settings and the i-th in menu order. */
int pc_settings_count(void);
const PcSettingDesc* pc_settings_at(int index);

#ifdef __cplusplus
}
#endif

#endif /* PC_SETTINGS_H */
