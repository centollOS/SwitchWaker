# Notes for agents

## Reaching the Switch: the debug server

The console runs a TCP debug server (port 6543) when the options menu (Minus) has Depuración >
**Servidor de depuración (red)** set to *Activado*; it starts at the next start of the game. Full reference:
[docs/DEBUG_SERVER.md](docs/DEBUG_SERVER.md).

- The console's address: `build/switch_host.txt` (first line), else `export SWITCHWAKER_HOST=<ip>`, else
  `--host <ip>`. `build/` is not in git, so a fresh checkout needs the file written again.
- Check that it answers: `scripts/switch/switchwaker_debug.py ping` (or `info`: version, frame, stage and
  room, memory). If it does not answer, ask the user to start the game (and turn the server on if it is off);
  do not guess.

## Update the console

```sh
scripts/switch/build_native.sh && scripts/switch/switchwaker_debug.py deploy
```

`deploy` uploads `build/switch-native/switchwaker.nro`, checks its CRC32, restarts the game and waits until it
answers. The restart (`reload`) needs the game started from the HOME-menu forwarder; from hbmenu,
`deploy --no-reload`, then ask the user to start it again. Building: [docs/SWITCH_BUILD.md](docs/SWITCH_BUILD.md).

## Debug

```sh
scripts/switch/switchwaker_debug.py log --save build/session.log &   # the log as it is written
scripts/switch/switchwaker_debug.py warps                            # the options menu's travel list
scripts/switch/switchwaker_debug.py warp 2 && sleep 10 && scripts/switch/switchwaker_debug.py shot build/shot.png
scripts/switch/switchwaker_debug.py press MINUS                      # buttons as if from the controller
scripts/switch/switchwaker_debug.py get native/user/settings.ini build/settings.ini   # relative to sdmc:/switch/switchwaker
```

After a crash the server is gone with the game: ask the user to start it again, then
`scripts/switch/switchwaker_debug.py lastlog` (the crashed session is then the second newest, see `logs`) and
`scripts/switch/switchwaker_debug.py crashes --fetch build/crashes`.

Developer variables without a menu row (`COS_TRACE`, `COS_BOOT_STAGE`, `MESA_*`, ...) go in the `[dev]`
section of `native/user/settings.ini` ([switch/native/settings-dev.example.ini](switch/native/settings-dev.example.ini)),
read at start: `get` it, edit, `put` it back, `reload`.

The debug server (`switch/native/source/debug_server.{h,cpp}`) and the client are shared with SwitchWakerHD:
keep the two copies the same.
