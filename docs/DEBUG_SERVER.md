# Debug server (Switch)

A TCP port on the console for the development machine: the log as it is written, files on the SD card,
controller presses, warps, screenshots, and a new build deployed and restarted without touching the console.
It is off unless the options menu turns it on. There is no password, so use it only on your local network. It is
the same server as SwitchWakerHD's (`docs/debug-server.md` there), with this port's commands.

## Turning it on

In the game's options menu (**Minus**): Depuración > **Servidor de depuración (red)** ("Debug server
(network)"), *Activado*. It takes effect at the next start (quit and start the game again, or `reload` once
the server runs). The menu saves it in `sdmc:/switch/switchwaker/native/user/settings.ini` as
`COS_DEBUG_SERVER=1` (port 6543; a port number there instead uses that port). The `native/env.txt` of
earlier builds is moved into that file at the first start of a newer build (`COS_DEBUG_SERVER=1` in it
becomes the menu setting, so the server starts in that same session) and kept as `env.txt.old`.

Other developer variables (those without a menu row: `COS_TRACE`, `COS_BOOT_STAGE`, `COS_DK_*`, ...) go in
the `[dev]` section at the end of the same file
([switch/native/settings-dev.example.ini](../switch/native/settings-dev.example.ini)); they are read at
start. Over the server:

```sh
scripts/switch/switchwaker_debug.py get native/user/settings.ini build/settings.ini
# edit build/settings.ini: add or change lines after "[dev]"
scripts/switch/switchwaker_debug.py put build/settings.ini native/user/settings.ini
scripts/switch/switchwaker_debug.py reload
```

At start the log says
`[switch] debug server listening on <ip>:6543`. A fixed address for the console (a DHCP reservation in the
router) saves looking it up each time. On the Mac, tell the client where the console is, either with
`export SWITCHWAKER_HOST=<ip>` or with the address on the first line of `build/switch_host.txt` (`build/` is
not in git).

## The client: `scripts/switch/switchwaker_debug.py`

| Command | What it does |
|---|---|
| `info` | version, frame, stage and room, memory, applet type, address |
| `log [--all] [--save F] [--grep RE] [--seconds N]` | the log as it is written; `--all` starts with the last 2 MiB |
| `deploy [NRO] [--no-reload]` | uploads `build/switch-native/switchwaker.nro`, checks its CRC32, restarts the game, waits until it answers again |
| `shot [OUT.png] [--game]` | PNG of the next frame, with the FPS panel and the options menu drawn over it; `--game` gives the game's picture alone |
| `press A [B ...] [ms]` | buttons pressed together (`A+B` is the same); 120 ms by default; replies once released |
| `hold X`, `release [X]` | held until released (`release` alone lets go of everything) |
| `stick L\|R x y [ms]` | stick override, -1..1, y up; `stick L 0 0` lets go |
| `warps`, `warp N`, `warp STAGE [ROOM] [POINT]` | the options menu's travel list, by number or by name; applied once a file is being played |
| `get`, `put`, `ls`, `rm`, `mkdir` | SD card files; paths are relative to `sdmc:/switch/switchwaker` unless they start with `/` |
| `logs`, `lastlog [LOCAL]` | the session logs in `native/logs/`; download the newest one. The running session's file cannot be opened while the game writes it, so `lastlog` then takes the text the console keeps in memory (`logtext`, the last 2 MiB, from the first line) |
| `crashes [--fetch DIR]` | Atmosphère's crash reports (`/atmosphere/crash_reports`) |
| `wait`, `quit`, `reload`, `ping`, `help`, `raw CMD ...` | `raw logtext` prints the kept log text |
| `crash` | a deliberate data abort (on the server's thread): checks the crash report in the log and that the process ends a few seconds later |

Button names: `A B X Y L R ZL ZR PLUS MINUS UP DOWN LEFT RIGHT LS RS`. The presses go into the gamepad shim
(`switch/aurora/sdl3_shim/sdl3_shim_gamepad.c`), as if they came from the controller, so they reach the
options menu too: `press MINUS` opens it.

A typical loop:

```
scripts/switch/build_native.sh && scripts/switch/switchwaker_debug.py deploy
scripts/switch/switchwaker_debug.py log --save build/session.log &
scripts/switch/switchwaker_debug.py warp 2 && sleep 10 && scripts/switch/switchwaker_debug.py shot
```

`reload` restarts the application (`appletRestartProgram`: the GPU profile is restored and the log written
first, as at an exit), so it only works when the game was started from the HOME-menu forwarder. Started from
hbmenu, use `quit`, then start it again. A game that crashed takes the server down with it: start it again
from the console, then fetch `lastlog` (the crashed session is then the second newest, see `logs`) and
`crashes`.

## Session logs

Since the debug server, the log follows SwitchWakerHD's pattern: one file per session,
`native/logs/switchwaker_<date>_<time>.log` (the console clock at start), and only the 10 most recent
sessions are kept, so the logs cannot fill the SD card. The `switchwaker.log` / `switchwaker.prev.log` of
earlier builds move into `logs/` at the first start.

## Protocol

Plain TCP, one command per line: `name arg arg ...`. An argument with spaces goes in double quotes. Every
reply is `ok <n>\n` or `err <n>\n` followed by n bytes (text, or a file's contents).
`put <path> <size>\n` is followed by the file's bytes and replies `crc32 <hex> size <n>`. The file is
written to `<path>.part` and then renamed, so a broken upload never replaces a good file. `log` replies `ok 0`,
then streams the log's text until the client closes the connection. At most 4 connections are open at once,
each with its own thread.

## Code

- `switch/native/source/debug_server.{h,cpp}`: the server. It knows nothing about the game, and it is the
  same file as SwitchWakerHD's (`runtime/src/platform/`): keep the two copies the same. Its stand-in for
  testing on the Mac is `scripts/switch/debug_server_host_test.cpp`.
- `switch/native/source/cos_debug.cpp`: this port's commands (`info`, `warps`, `warp`, `shot`, `reload`,
  `quit`), the start from the settings file (`cos_switch_debug_start`, called by `cos_switch_start` after
  `pc_settings_load_early`), and the input
  bridge.
- The hooks:
  - `cos_switch.cpp` `queueBytes`: the log's text, kept from the first line (`keep_log`, let go by `drop_log`
    when the server is off).
  - `sdl3_shim_gamepad.c`: the presses and sticks.
  - `native/src/pc/harness/pc_shot.cpp` (`pc_debug_shot_*`): the screenshot at the next frame end.
  - `native/src/pc/features/pc_menu.cpp` (`pc_debug_warp*`, `pc_debug_status`): the warp and the game's
    state, both on the game thread.
- `scripts/switch/switchwaker_debug.py`: the client, the same as SwitchWakerHD's `tools/switch/wwhd_debug.py`
  except for its configuration block.
