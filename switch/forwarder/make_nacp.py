#!/usr/bin/env python3
"""Write the forwarder's control.nacp (the HOME-menu title data) with the standard library only.

    make_nacp.py OUT.nacp --title-id 0x... --name NAME --publisher PUB --version DISPLAY

Layout: switchbrew "NACP Format" (0x4000 bytes). Every language slot gets the same name and
publisher (scripts/switch/build_forwarder.sh writes an icon for each of them too), so the HOME menu
shows the title and icon whatever the console's language. Choices for the native port:
  - StartupUserAccount = None: no user picker (the game keeps its saves on the SD card).
  - Screenshot = Allow, VideoCapture = Disable: automatic gameplay recording would make hbloader
    hold back 96 MiB of the application's memory (nx-hbloader calculateMaxHeapSize).
  - No save data, no rating ages (-1), no parental-control flags, LogoHandling = Auto.
  - PresenceGroupId, SaveDataOwnerId, LocalCommunicationId[0] and SeedForPseudoDeviceId = the
    title ID, as the console maker's tools and hacBrewPack's --titleid fill them.
"""
import argparse
import struct

LANGUAGES = [
    "AmericanEnglish", "BritishEnglish", "Japanese", "French", "German", "LatinAmericanSpanish",
    "Spanish", "Italian", "Dutch", "CanadianFrench", "Portuguese", "Russian", "Korean",
    "TraditionalChinese", "SimplifiedChinese", "BrazilianPortuguese",
]


def put(buf, off, data, size):
    data = data[: size - 1] if isinstance(data, bytes) else data
    buf[off : off + len(data)] = data


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--title-id", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--publisher", required=True)
    ap.add_argument("--version", required=True)
    a = ap.parse_args()
    tid = int(a.title_id, 16)
    name, pub, ver = (s.encode("utf-8") for s in (a.name, a.publisher, a.version))
    if len(name) >= 0x200 or len(pub) >= 0x100 or len(ver) >= 0x10:
        raise SystemExit("make_nacp: name/publisher/version too long")

    n = bytearray(0x4000)
    for i in range(len(LANGUAGES)):
        put(n, i * 0x300, name, 0x200)
        put(n, i * 0x300 + 0x200, pub, 0x100)
    n[0x3025] = 0  # StartupUserAccount: None
    n[0x3026] = 0  # UserAccountSwitchLock: Disable
    struct.pack_into("<I", n, 0x3028, 0)  # AttributeFlag
    struct.pack_into("<I", n, 0x302C, (1 << len(LANGUAGES)) - 1)  # SupportedLanguageFlag
    struct.pack_into("<I", n, 0x3030, 0)  # ParentalControlFlag
    n[0x3034] = 0  # Screenshot: Allow
    n[0x3035] = 0  # VideoCapture: Disable
    n[0x3036] = 0  # DataLossConfirmation: None
    n[0x3037] = 0  # PlayLogPolicy: All
    struct.pack_into("<Q", n, 0x3038, tid)  # PresenceGroupId
    n[0x3040:0x3060] = b"\xff" * 0x20  # RatingAge: none
    put(n, 0x3060, ver, 0x10)  # DisplayVersion
    struct.pack_into("<Q", n, 0x3070, tid + 0x1000)  # AddOnContentBaseId
    struct.pack_into("<Q", n, 0x3078, tid)  # SaveDataOwnerId
    struct.pack_into("<Q", n, 0x30B0, tid)  # LocalCommunicationId[0]
    n[0x30F0] = 0  # LogoType (unused: --nologo packs no logo section, nothing is shown)
    n[0x30F1] = 0  # LogoHandling: Auto
    struct.pack_into("<Q", n, 0x30F8, tid)  # SeedForPseudoDeviceId
    with open(a.out, "wb") as f:
        f.write(n)
    print(f"make_nacp: {a.out}: {a.name!r} / {a.publisher!r} {a.version!r} 0x{tid:016x}")


if __name__ == "__main__":
    main()
