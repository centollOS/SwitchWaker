#!/usr/bin/env python3
"""The 22 dungeon-item messages of step R3.4 (docs/RANDOMIZER_PLAN.md), word-wrapped as wwrando does.

    python3 -I native/tools/rando/gen_dungeon_item_msgs.py [disc.iso]   (default $COS_DISC)

Builds each description string exactly as tweaks.py allow_dungeon_items_to_appear_anywhere does,
wraps it with gclib's BFN.word_wrap_string at the ITEM_GET width (bmg.py
TEXT_BOX_TYPE_TO_MAX_LINE_LENGTH, 419) using the USA font rock_24_20_4i_usa.bfn from the player's
own disc (files/res/Msg/fontres.arc), and prints the kItems[] rows of
native/src/pc/features/rando/pc_rando_dungeon_items.cpp. Only the resulting text (wwrando's output)
goes into the source; the font itself is never checked in.
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

from gclib.bfn import BFN  # noqa: E402
from gclib.gcm import GCM  # noqa: E402
from gclib.rarc import RARC  # noqa: E402
from gclib.yaz0_yay0 import Yaz0  # noqa: E402
from logic.logic import Logic  # noqa: E402

ITEM_GET_MAX_LINE_LENGTH = 419

# tweaks.py dungeon_items, with the stage number each custom_funcs.asm wrapper passes.
STAGES = {"FF": 2, "DRC": 3, "FW": 4, "TotG": 5, "ET": 6, "WT": 7}
DUNGEON_ITEMS = [
  ("DRC", "Small Key", 0x13), ("DRC", "Big Key", 0x14), ("DRC", "Dungeon Map", 0x1B), ("DRC", "Compass", 0x1C),
  ("FW", "Small Key", 0x1D), ("FW", "Big Key", 0x40), ("FW", "Dungeon Map", 0x41), ("FW", "Compass", 0x5A),
  ("TotG", "Small Key", 0x5B), ("TotG", "Big Key", 0x5C), ("TotG", "Dungeon Map", 0x5D), ("TotG", "Compass", 0x5E),
  ("FF", "Dungeon Map", 0x5F), ("FF", "Compass", 0x60),
  ("ET", "Small Key", 0x73), ("ET", "Big Key", 0x74), ("ET", "Dungeon Map", 0x75), ("ET", "Compass", 0x76),
  ("WT", "Small Key", 0x77), ("WT", "Big Key", 0x81), ("WT", "Dungeon Map", 0x84), ("WT", "Compass", 0x85),
]
KIND = {"Small Key": "SMALL_KEY", "Big Key": "BIG_KEY", "Dungeon Map": "MAP", "Compass": "COMPASS"}
TAGS = [
  ("\\{1A 05 00 00 01}", "PCR_MSG_INSTANT"),
  ("\\{1A 06 FF 00 00 01}", "PCR_MSG_COLOR_RED"),
  ("\\{1A 06 FF 00 00 00}", "PCR_MSG_COLOR_DEFAULT"),
]


def indefinite_article(s):  # tweaks.py get_indefinite_article
  return "an" if s.strip()[0].lower() in "aeiou" else "a"


def description(dungeon_name, base_item_name):  # tweaks.py allow_dungeon_items_to_appear_anywhere
  if base_item_name == "Small Key":
    return ("\\{1A 05 00 00 01}You got %s \\{1A 06 FF 00 00 01}%s small key\\{1A 06 FF 00 00 00}!"
            % (indefinite_article(dungeon_name), dungeon_name))
  return "\\{1A 05 00 00 01}You got the \\{1A 06 FF 00 00 01}%s %s\\{1A 06 FF 00 00 00}!" % (dungeon_name,
                                                                                         base_item_name)


def c_string(s):
  parts = [s]
  for tag, macro in TAGS:
    nxt = []
    for p in parts:
      if p.startswith("\x00"):
        nxt.append(p)
        continue
      pieces = p.split(tag)
      for i, piece in enumerate(pieces):
        if i:
          nxt.append("\x00" + macro)
        nxt.append(piece)
    parts = nxt
  out = []
  for p in parts:
    if p.startswith("\x00"):
      out.append(p[1:])
    elif p:
      out.append('"' + p.replace("\\", "\\\\").replace("\n", "\\n").replace('"', '\\"') + '"')
  return " ".join(out)


def main():
  iso = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("COS_DISC")
  if not iso or not os.path.isfile(iso):
    oc.die("pass the USA disc image (or set COS_DISC)")
  gcm = GCM(iso)
  gcm.read_entire_disc()
  data = gcm.get_changed_file_data("files/res/Msg/fontres.arc")
  if Yaz0.check_is_compressed(data):
    data = Yaz0.decompress(data)
  arc = RARC(data)
  arc.read()
  bfn = arc.get_file("rock_24_20_4i_usa.bfn", BFN)
  for short, base, item_id in DUNGEON_ITEMS:
    text = bfn.word_wrap_string(description(Logic.DUNGEON_NAMES[short], base), ITEM_GET_MAX_LINE_LENGTH)
    kind = KIND[base]
    print('    {0x%02X, %d, PCR_DITEM_%s, PCR_DITEM_BASE_%s, "%s", "%s %s",\n     %s},'
          % (item_id, STAGES[short], kind, kind, short, short, base, c_string(text)))


if __name__ == "__main__":
  main()
