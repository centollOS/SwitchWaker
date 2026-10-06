/*
 * pc_hd_textures.h - optional HD texture replacement from a Dolphin-format pack
 * (native/src/pc/features/pc_hd_textures.cpp; docs/HD_TEXTURES.md). Off by default.
 *
 * The replacement itself is Aurora's (lib/gfx/texture_replacement.cpp, tuned by
 * native/patches/aurora/0010): static GX textures are keyed by Dolphin's name
 * (tex1_<w>x<h>[_m]_<xxh64 of the base level>[_<xxh64 of the used TLUT range>|_$]_<fmt>), looked
 * up when a texture object is first resolved, loaded by background workers and swapped in when
 * ready; until then the original texture is drawn. EFB copies are never replaced.
 *
 * COS_HD_TEXTURES=0|1     off (default) / on.
 * COS_HD_PACK=<dir>       the pack: a directory with index.bin + dataNN.bin written by
 *                         native/tools/hd_pack (cos_hd_pack), or a Dolphin texture directory of
 *                         loose tex1_*.dds/.png files (e.g. a pack's GZL folder). Default:
 *                         <user path>/hd_textures (Switch: sdmc:/switch/centollos/native/
 *                         user/hd_textures).
 * COS_HD_BUDGET_MB=<n>    GPU memory the replacements may hold (default 512 on the Switch, 1024
 *                         elsewhere). Textures bound in the last 60 frames are never evicted; a
 *                         load that does not fit is dropped and retried 300 frames later.
 * COS_HD_PUBLISH_MB=<n>   upload budget per frame (default 4 on the Switch, 12 elsewhere).
 * COS_HD_WORKERS=<n>      loader threads (default 1 on the Switch, auto elsewhere).
 * COS_HD_MAX_SIZE=auto|256|512|1024|full  larger side of the textures loaded, per operation mode
 *                         (options menu "Tamaño máx. texturas HD"); auto: 512 handheld, 1024 docked.
 *                         The loader skips the levels above it in the pack; a change (or a
 *                         dock/undock) reloads the cached textures gradually.
 * COS_HD_PUBLISH_US=<n>   publish budget per frame in estimated GPU-side time (default 2500 on
 *                         the Switch, 3000 elsewhere; 0: bytes only), with the cost model
 *                         COS_HD_COST_TEXTURE_US / _LEVEL_US / _MIB_US (Switch: scaled at run time
 *                         by Dawn's measured upload and creation time).
 * COS_HD_REGISTER_US=<n>  game-thread time per frame for registering / unregistering the pack's
 *                         names after a toggle (default 2000).
 * COS_HD_RESCAN_KB=<n>    source bytes of static textures looked up again per frame after a toggle
 *                         (default 2048 on the Switch, 8192 elsewhere; 0: no limit).
 * COS_HD_MAX_SIZE_FRAMES=f:size,...  set the cap at those frames (checks of the reload).
 * COS_HD_STATS_EVERY=<n>  an "[cos] hd-textures" stats line every n frames while on (default 300;
 *                         0: none).
 * COS_HD_CENSUS=<file>    (on or off) writes the Dolphin name of every distinct static texture the
 *                         game resolves to <file> (one per line), for native/tools/hd_census.py.
 * COS_HD_TOGGLE_FRAMES=a,b,... flips the setting at those game frames (checks of the toggle).
 *
 * The game thread calls all of them.
 */
#ifndef PC_HD_TEXTURES_H
#define PC_HD_TEXTURES_H

#ifdef __cplusplus
extern "C" {
#endif

/* After aurora_initialize, with the user path (COS_HD_PACK default): reads the settings and, if on,
   registers the pack. */
void pc_hd_textures_init(const char* userPath);
/* After aurora_end_frame: applies a requested toggle, the test toggles and the stats line. */
void pc_hd_textures_frame_end(unsigned int frame);

/* The runtime setting (the options menu's "Texturas HD" row, COS_HD_TEXTURES in pc_settings.h, reaches
   it through a subscription made by pc_hd_textures_init). Turning it on reads the pack's index
   (COS_HD_PACK) on a helper thread and registers it over the next frames; returns false, and
   stays off, if there is no pack. Turning it off unregisters every replacement over the next
   frames: the original textures come back. */
bool cos_hd_textures_set_enabled(bool enabled);
bool cos_hd_textures_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_HD_TEXTURES_H */
