# Update check and in-game update (issues #27 and #28)

SwitchWaker's release zips are the whole program (the NRO and its two shader caches; the game data
comes from the player's disc), so the game can update itself: check GitHub Releases at start, offer
the new version, download the zip of the running region, check it, put its files in place with a way
back, and restart into it. #27 (the check and the popup) and #28 (the install) share the HTTPS
client, so they are done together.

## What a release is

- `SwitchWaker-vX.Y.Z-switch.zip` (USA, `GZLE01`) and `SwitchWaker-vX.Y.Z-switch-eur.zip` (Europe,
  `GZLP01`), each with `<zip>.sha256` (`shasum -a 256` output: `<hex>  <name>`).
- Each zip holds `switch/switchwaker/switchwaker.nro`, `initial_pipeline_cache.db` and
  `initial_dksh_cache.bin` (deflate, `zip -qr`), about 9.5 MB packed, 37 MB unpacked.
- The update replaces exactly what "copy the zip's `switch` folder over the old one" (the release
  notes' manual update) replaces: the disc, saves, settings, logs, user caches and the forwarder stay.

## Version

The NRO's version is `COS_SWITCH_VERSION_STR` (`git describe` without the `v`, at most 15 bytes):
`0.5.0` on a tag, `0.5.0-3` or `0.5.0-3-gabcdef` past it, `0.5.0-dirty`, a bare hash or `dev` without
tags. A version that does not start with `X.Y.Z` never checks. A suffix `-N` means "after X.Y.Z", so
`0.5.0-3` is not older than `v0.5.0` and is older than `v0.5.1`. Only `releases/latest` is read, so
drafts and pre-releases are never offered.

`COS_UPDATE_VERSION` (`[dev]`) pretends another version, for testing on the console.

## Check (#27)

- At start, when **Sistema > Buscar actualizaciones al arrancar** (`COS_UPDATE_CHECK`, default on; the
  Sistema tab sits before Depuración, which stays last)
  is on and the version is a release one: a background thread (host memory) GETs
  `https://api.github.com/repos/centollOS/SwitchWaker/releases/latest` (`COS_UPDATE_URL` overrides,
  for tests), with a User-Agent, and reads `tag_name`, `name`, `html_url` and the assets' `name`,
  `browser_download_url` and `size`. It never delays boot. Offline, rate-limited (60/hour/IP) or any
  error: one log line, nothing on screen.
- Newer: a popup once the logo is over and outside the PLAY scene (title, file select): current -> latest, release title,
  **Actualizar ahora** / **Más tarde** / **No avisar de esta versión** (`COS_UPDATE_SKIP=<tag>`, a
  setting without a menu row). During PLAY it waits for the next non-PLAY scene; the Sistema tab
  shows it any time.
- The Sistema tab's **Actualización** row checks again on A (it ignores `COS_UPDATE_SKIP`).
- Release notes: the release's `body` (GitHub Markdown) as plain Latin-1 lines (`notesFromMarkdown`:
  headings, items, links reduced to their text, tables on one line; the "Install..." section left out),
  on a scrolling page of the Sistema tab (**Notas de la versión**, or the popup's **Ver notas de la
  versión**); A there installs. Only the latest release's notes, not those of releases skipped between.

## Install (#28)

Only on the Switch, from the forwarder's NRO path `sdmc:/switch/switchwaker/switchwaker.nro`
(`argv[0]`; another path: the row says to update by hand). Desktop builds use the same code against
`COS_UPDATE_DIR` for the tests.

1. Pick the asset of the running region (`-switch.zip` for GZLE01, `-switch-eur.zip` for GZLP01)
   and its `.sha256`. Missing: "no update for this region".
2. Free space: the zip plus its unpacked size plus 4 MiB, else a message.
3. Download the zip to `<dir>/native/update/<zip>` (curl follows GitHub's redirect to its asset
   host; progress in the row), and the `.sha256`. SHA-256 must match.
4. Read the zip (own reader over zlib's inflate): only entries `switch/switchwaker/<name>` with a
   plain name (no `/`, no `..`); it must hold `switchwaker.nro`. Each entry goes to `<dir>/<name>.new`,
   checked against the entry's CRC-32 and size.
5. Journal `<dir>/native/update/state.txt`: `state=installing`, the target tag, the names, and which
   names existed before. Then per name: `<name>` -> `<name>.old` (when it existed), `<name>.new` ->
   `<name>` (Horizon's rename never replaces a file, so the target name is always free first). Then
   `state=installed`. The zip is deleted.
6. Restart: from the forwarder (program ID 0x01FF43454E540000), `appletRestartProgram` (the forwarder
   loads the new NRO). From hbmenu: "close the game and open it again". While the memory card is being
   written the restart waits; in the PLAY scene it asks for a second A (unsaved progress is lost).

## Start and rollback

`pc_update_boot()` runs at the very start (the Switch: in `cos_switch_start`, after the settings),
before anything that could crash:

- No journal: stray `*.new` files in `<dir>` are deleted (an interrupted extraction).
- `installing`: the swap was interrupted: every `<name>.old` goes back, names that did not exist
  before are deleted, `*.new` deleted, journal deleted.
- `installed` and this build is the target version: `state=booting`. Once a scene after the logo runs
  (the shader preload is over, the title is up): `*.old` deleted, journal deleted, "Actualizado a vX"
  shown once.
- `booting` and this build is the target: the last start of the new version never got past the
  logo. Roll back as for `installing`, write `<dir>/native/update/failed.txt` (the tag: the restored
  version does not offer it again at start, only from the menu), then restart (forwarder) or show
  an error asking to open the game again (hbmenu) and exit.
- A journal for another version than this build (the player copied files by hand): `*.old` and the
  journal are deleted, nothing restored.

## Not done (follow-ups)

- A signature (ed25519 key in the NRO) on top of the SHA-256: the `.sha256` comes from the same
  release as the zip, so today it only guards against a broken download; HTTPS (the console's own CA
  store through libnx's ssl service) guards the transport.
- A manifest in the release: the zip's own layout is the manifest for now.
- The forwarder itself is never updated (it does not change between releases).

## Pieces

- `native/src/pc/features/pc_update_core.{h,cpp}`: pure code, checked by `cos_pc_tests`: version
  parse/compare, SHA-256, the release JSON, the zip reader, journal parse/format.
- `native/src/pc/features/pc_update.cpp` (+ `native/include/pc/pc_update.h`): the worker thread,
  the HTTP transport (libcurl: the Switch's devkitPro `switch-curl`, libnx ssl backend; desktop's
  when CMake finds it; `file://` always), the install, `pc_update_boot`, the state the menu reads.
- `pc_menu.cpp`: the Sistema tab (the two rows) and the popup.
- `switch/native/source/cos_switch.cpp`: socket init shared with the debug server, forwarder
  detection, restart with a reason, free space.

## Tests

- `cos_update_tests` (in `native/tools/regress.sh`): the pure part and the whole flow on `file://`
  fixtures (install, confirm, failed first start and rollback, interrupted swap, bad checksum, bad zip,
  no zip for the region, failed checks). `cos_update_tests --live`: GitHub for real, the latest release
  installed into a temporary directory.
- Smokes `options-menu --input native/check/input/menu-update.txt` and `menu-update-skip.txt`: the popup
  on the title screen with the fixture release `native/check/update` (v9.9.9).

## Steps (one commit each)

1. This plan.
2. `pc_update_core` + host tests.
3. Install and boot/rollback on files (desktop smoke `COS_SMOKE=update` with a local zip).
4. Transport and check thread (desktop: file:// fixtures, then a live GitHub check).
5. Settings, Sistema tab, popup.
6. Switch: curl + sockets, forwarder restart, free space; NRO; console test (update from a pretended
   older version to the latest release, then the rollback).
7. Docs (README, INSTALL, release checklist, DEBUG_SERVER if touched).
