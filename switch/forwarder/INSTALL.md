# HOME-screen icon (forwarder NSP)

`build/forwarder/centollos_forwarder.nsp` installs the "centollOS" icon on the HOME screen.
Opening it starts `sdmc:/switch/centollos/centollos.nro` as an application, with all the memory:
there is no need to open hbmenu by holding R over a game any more. `centollos.log` should then
show `application (title mode)`.

It is built with `scripts/switch/build_forwarder.sh` (keys from `~/.switch/prod.keys` by default;
they are never copied into the repository). Title ID: `01FF43454E540000`.

## Requirements

- Atmosphère with sigpatches up to date for your firmware (the NSP does not carry the console's
  official signature). If installing fails over the signature, or the icon shows an error when
  opened, update the sigpatches (including the loader/ACID ones).
- The NRO **must** stay at `sdmc:/switch/centollos/centollos.nro`. The forwarder only contains a
  loader: if you move or rename the NRO, the icon will show an error when opened. To update the
  game, replace the NRO at that path; the NSP does not need reinstalling (only when the icon, the
  name or the displayed version change).

## Install with DBI (USB/MTP)

1. Copy `centollos_forwarder.nsp` to the computer.
2. On the Switch, open DBI and choose **Run MTP responder**.
3. Connect the console over USB. An MTP device shows up on the computer: open
   **"SD Card install"** (or "NAND install") and drag the `.nsp` into it.
4. Wait for DBI to finish, leave DBI and go back to HOME: the icon is there.

macOS needs an MTP client (for example OpenMTP or Android File Transfer).

## Install with Goldleaf

1. Copy the `.nsp` to the SD card (for example to `sdmc:/nsp/`).
2. Open Goldleaf → **Explore SD card** → select the `.nsp` → **Install** → destination
   **SD card**. Ignore the warning that it is not an official title.
3. Go back to HOME. You can delete the `.nsp` from the SD card afterwards.

(Tinfoil works too: "File browser" → the `.nsp` → Install.)

## If you had the earlier forwarder installed

The earlier forwarder used another title ID (`01FF57574E000000`) and opened the NRO under its old
name. Delete it from HOME (Manage Software) or from DBI/Goldleaf by that title ID; your data on the
SD card is not touched. Then install this one.

## Uninstall

- From HOME: System Settings → Data Management → Manage Software → "centollOS" → **Delete
  Software**. Or select the icon, press + (Options) → Data Management → Delete Software.
- Or from DBI/Goldleaf: list of installed titles → `01FF43454E540000` → delete.

Uninstalling the icon does not touch the NRO, the ISO or the data in `sdmc:/switch/centollos/`.

## Warning: ban risk

Installing homebrew NSPs leaves the title in the console's records, and the console's maker can
detect it when the console connects to its servers. **Do not go online with the forwarder
installed** (use an offline emuMMC, or block the official servers with DNS/90DNS or Atmosphère's
`hosts` file). Use it at your own risk.
