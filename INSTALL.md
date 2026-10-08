# Installing SwitchWaker on your Switch

SwitchWaker runs The Wind Waker (GameCube) natively on a Switch with custom firmware. **Nothing of the
game comes with it**: you build it on your computer from your own disc, with one script, and copy the
result to the SD card. What you build contains code generated from your disc, so it is for your own
console only: do not share it.

## 1. What you need

| | |
|---|---|
| **The game** | Your own disc of **The Wind Waker (USA)**, `GZLE01` **revision 0**, as an **uncompressed `.iso`** (1.4 GB). RVZ, GCZ, CISO or NKit images must be converted first: in Dolphin, right-click the game → *Convert File...* → ISO |
| **A Switch** | Running Atmosphère with the Homebrew Menu, and about 3 GB free on the SD card |
| **A computer** | Windows 10/11 (with WSL), macOS or Linux; 8 GB of RAM or more, 20 GB of free disk space |
| **On the computer** | [Docker Desktop](https://www.docker.com/products/docker-desktop/) (or Docker / Podman on Linux), git, Python 3, and this release unzipped |

How to dump your disc is outside this guide: [Dolphin's guide](https://dolphin-emu.org/docs/guides/ripping-games/)
covers it.

**Windows**: run everything below inside WSL (Ubuntu): install WSL with `wsl --install` in an
administrator PowerShell, turn on Docker Desktop's *Use the WSL 2 based engine* and its integration with
your Ubuntu, then open Ubuntu and work there. Keep the unzipped release inside the WSL file system (for
example `~/switchwaker`), not under `/mnt/c`: builds there are much slower.

## 2. Build it

Open a terminal in the unzipped release folder:

```sh
scripts/switch/make_sd.sh --disc /path/to/GZLE01.iso
```

It checks the disc first, then builds everything in Docker. **The first build takes an hour or more**
(it downloads and compiles the graphics libraries once); later builds take minutes. If the computer runs
out of memory, add `--jobs 2`.

At the end the folder `build/sd/` holds:

```
build/sd/switch/switchwaker/
├── switchwaker.nro                the game
├── initial_pipeline_cache.db      the list of shaders to prepare on the first start
└── GZLE01.iso                     a copy of your disc (the game reads it on the console)
```

## 3. Copy it to the SD card

Copy the **contents** of `build/sd/` to the root of the SD card, so the game ends up at
`sdmc:/switch/switchwaker/switchwaker.nro`. You can take the card out, or connect the Switch by USB with
hekate (*Tools → USB Tools → SD Card*).

## 4. Play

- Start the Homebrew Menu **in title mode**: hold **R** while starting any installed game, then pick
  **SwitchWaker**. From the album (applet mode) the game does not get enough memory.
- Optional: a HOME-screen icon that always starts it the right way (you build it with your own console
  keys): [switch/forwarder/INSTALL.md](switch/forwarder/INSTALL.md).
- **The first start prepares the shaders once** ("Preparing shaders (first start only)", with a progress
  bar): about 8 to 10 minutes on a typical microSD. **Do not close the game meanwhile.** Later starts take
  a few seconds.
- **Minus (−)** opens the options menu (in Spanish or English, following the console's language):
  graphics, performance, controls, debug. Settings are saved on the card.

## Updating

Build the new release the same way, with `--no-disc` (your disc is already on the card), and copy
`switchwaker.nro` and `initial_pipeline_cache.db` over the old ones. Saves
(`native/user/USA/Card A`), settings and the prepared shaders stay; a new version may prepare a few new
shaders on its first start.

## Problems

| | |
|---|---|
| `this is not the disc the port is built for` | The image is not GZLE01 revision 0 or is compressed: use the USA disc, converted to an uncompressed ISO |
| Docker errors | Docker Desktop must be running (on Windows, with its WSL integration on) |
| The game closes at once or says it is out of memory | Start it in title mode (hold R), not from the album |
| It stutters on every start or fails after the shader preparation was interrupted | Delete `sdmc:/switch/switchwaker/native/user/cache/` and let it prepare once more |
| Something else | The session logs in `sdmc:/switch/switchwaker/native/logs/` (the newest file is the last run) say what happened; include them in a report (they contain no game data) |
