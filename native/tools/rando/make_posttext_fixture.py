#!/usr/bin/env python3
"""Fixtures for the randomizer's seed-dependent message texts (docs/RANDOMIZER_PLAN.md step R3.21).

    python3 -I native/tools/rando/make_posttext_fixture.py [--out-dir DIR]
                [--disc GZLE01.iso --placement native/check/rando/fixtures/R0_6_rotation.json]

Runs wwrando's own tweaks.py functions (ref/wwrando, MIT) on a stand-in randomizer object:

  update_shop_item_descriptions, update_auction_item_names, update_battlesquid_item_names,
  update_item_names_in_letter_advertising_rock_spire_shop, update_korl_dialogue,
  add_chart_number_to_item_get_messages, update_beedle_spoil_selling_text, and the message part of
  make_sail_behave_like_swift_sail (463 = "Swift Sail"; the rest of that function patches code and
  textures).

R3_21_posttext.txt (test_R3_21.cpp; no disc data): a made-up BMG (the vanilla texts below, written for
this fixture) and the made-up width table of make_wrap_fixture.py, with a fixed set of item names.
Lines (strings as UTF-8 hex, "-" for an empty one):

    F <256 widths> <replacement width>        the width table (as R3_20_wrap.txt)
    V <msg> <text_box_type> <text>            a message of the made-up BMG
    L <location name> <item name>             done_item_locations
    S <msg> <text>                            the text wwrando leaves in the message

R3_21_posttext_game.txt (smoke `posttext`; needs --disc and --placement): the same functions on the
disc's zel_00.bmg and message font (as randomizer.py loads them) with the placement's items. A text
built only from wwrando's strings and item names is written out; one that keeps vanilla BMG text
(the letter 3325, Beedle's 3957, the chart item-get messages) is written as the MD5 of its UTF-8 bytes,
so the fixture holds no disc text:

    P <placement file name>
    T <msg> <text>
    H <msg> <md5 hex>
"""
import hashlib
import os
import sys
import types
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

import contextlib  # noqa: E402
import json  # noqa: E402
import re  # noqa: E402

from gclib.bmg import Message, TextBoxType  # noqa: E402
import make_wrap_fixture as mwf  # noqa: E402  (the made-up font)

with contextlib.redirect_stdout(sys.stderr):
  import tweaks  # noqa: E402

RED = "\\{1A 06 FF 00 00 01}"
WHITE = "\\{1A 06 FF 00 00 00}"

SHOP_LOCS = [
  "The Great Sea - Beedle's Shop Ship - 20 Rupee Item",
  "Rock Spire Isle - Beedle's Special Shop Ship - 500 Rupee Item",
  "Rock Spire Isle - Beedle's Special Shop Ship - 950 Rupee Item",
  "Rock Spire Isle - Beedle's Special Shop Ship - 900 Rupee Item",
  "Windfall Island - 5 Rupee Auction",
  "Windfall Island - 40 Rupee Auction",
  "Windfall Island - 60 Rupee Auction",
  "Windfall Island - 80 Rupee Auction",
  "Windfall Island - Battlesquid - First Prize",
  "Windfall Island - Battlesquid - Second Prize",
]

# Item names for the made-up run: a vowel, a long name, names with articles, numbers and so on.
SYN_ITEMS = [
  "Empty Bottle", "Piece of Heart", "Treasure Chart 17", "Tower of the Gods Small Key",
  "Hurricane Spin", "Progressive Sword", "Orange Rupee", "Joy Pendant", "Ice Trap Chest",
  "Triforce Shard 3",
]

# Made-up vanilla texts with the shape the code relies on (the letter has more than 12 lines; Beedle's
# line 3 is replaced; the chart messages hold "a <red>Treasure Chart" once, twice or not at all).
SYN_LETTER = "\n".join(
  ["  Line %d of the made-up letter." % i for i in range(8)] +
  ["  old hint %d" % i for i in range(4)] +
  ["", "  After the hint, with a " + RED + "tag" + WHITE + ".", "     Signed"])
SYN_BEEDLE = "First line\nsecond " + RED + "line" + WHITE + "\n\n\nfifth line"


def item_names():
  with open(os.path.join(oc.WWRANDO_DIR, "data", "item_names.txt"), "r") as f:
    matches = re.findall(r"^([0-9a-f]{2}) - (.+)$", f.read(), re.IGNORECASE | re.MULTILINE)
  return {int(i, 16): name for i, name in matches if name}


def message(box_type, string):
  msg = Message.__new__(Message)
  msg.text_box_type = box_type
  msg.string = string
  return msg


def chart_msg_ids(names):
  out = []
  for item_id, name in names.items():
    if name.startswith("Treasure Chart ") or (name.startswith("Triforce Chart ") and "deciphered" not in name):
      out.append(101 + item_id)
  return out


def run_tweaks(fake, swift_sail=True):
  """randomizer.py's order: apply_necessary_tweaks, then apply_necessary_post_randomization_tweaks."""
  if swift_sail:
    fake.bmg.messages_by_id[463].string = "Swift Sail"
  tweaks.update_korl_dialogue(fake)
  tweaks.add_chart_number_to_item_get_messages(fake)
  tweaks.update_beedle_spoil_selling_text(fake)
  tweaks.update_shop_item_descriptions(fake)
  tweaks.update_auction_item_names(fake)
  tweaks.update_battlesquid_item_names(fake)
  tweaks.update_item_names_in_letter_advertising_rock_spire_shop(fake)


def touched_ids(names):
  return ([463, 3443] + chart_msg_ids(names) + [3957, 3906, 3909, 12106, 12109, 12107, 12110, 12108, 12111,
          7441, 7440, 7442, 7443, 7520, 7521, 3325])


def fake_randomizer(bmg_messages, font, done, names):
  bmg = types.SimpleNamespace(messages_by_id=bmg_messages)
  return types.SimpleNamespace(bmg=bmg, bfn=font, item_names=names,
                               logic=types.SimpleNamespace(done_item_locations=done))


def write_synthetic(path):
  h = mwf.h
  names = item_names()
  msgs = {}
  vanilla_lines = []

  def add(n, box, s):
    msgs[n] = message(box, s)
    vanilla_lines.append("V %d %d %s" % (n, box.value, h(s)))

  for n in [463, 3443, 3906, 3909, 12106, 12107, 12108, 12109, 12110, 12111, 7440, 7441, 7442, 7443]:
    add(n, TextBoxType.DIALOG, "vanilla %d" % n)
  add(7520, TextBoxType.DIALOG, "vanilla 7520")
  add(7521, TextBoxType.ITEM_GET, "vanilla 7521")  # item-get width, to check the box type is used
  add(3325, TextBoxType.PARCHMENT, SYN_LETTER)
  add(3957, TextBoxType.DIALOG, SYN_BEEDLE)
  for i, n in enumerate(chart_msg_ids(names)):
    kind = "Treasure Chart" if names[n - 101].startswith("Treasure") else "Triforce Chart"
    s = ["You got a %s%s%s!" % (RED, kind, WHITE),
         "a %s%s and again a %s%s%s." % (RED, kind, RED, kind, WHITE),
         "Nothing to replace here, %s." % kind][i % 3]
    add(n, TextBoxType.ITEM_GET, s)

  done = {loc: SYN_ITEMS[i % len(SYN_ITEMS)] for i, loc in enumerate(SHOP_LOCS)}
  fake = fake_randomizer(msgs, mwf.syn_font(), done, names)
  run_tweaks(fake)

  lines = ["# R3.21: wwrando's post-randomization texts on a made-up BMG and font "
           "(make_posttext_fixture.py)",
           "F " + " ".join(str(w) for w in mwf.SYN_WIDTHS) + " %d" % mwf.SYN_REPLACEMENT]
  lines += vanilla_lines
  for loc in SHOP_LOCS:
    lines.append("L %s %s" % (h(loc), h(done[loc])))
  for n in touched_ids(names):
    lines.append("S %d %s" % (n, h(msgs[n].string)))
  with open(path, "w", encoding="ascii") as f:
    f.write("\n".join(lines) + "\n")
  print("%s: %d messages" % (path, len(touched_ids(names))), file=sys.stderr)


# Messages whose result keeps vanilla BMG text: written as an MD5.
def keeps_disc_text(n, names):
  return n in (3325, 3957) or n in chart_msg_ids(names)


def write_game(path, disc, placement_path):
  from gclib.bfn import BFN
  from gclib.bmg import BMG
  from gclib.gcm import GCM
  from gclib.rarc import RARC
  gcm = GCM(disc)
  gcm.read_entire_disc()
  arc = RARC(gcm.read_file_data("files/res/Msg/bmgres.arc"))
  arc.read()
  bmg = arc.get_file("zel_00.bmg", BMG)
  farc = RARC(gcm.read_file_data("files/res/Msg/fontres.arc"))
  farc.read()
  font = farc.get_file("rock_24_20_4i_usa.bfn", BFN)
  with open(placement_path, encoding="utf-8") as f:
    placement = json.load(f)
  done = placement["locations"]
  swift = placement.get("options", {}).get("swift_sail", True)
  names = item_names()
  fake = fake_randomizer(bmg.messages_by_id, font, done, names)
  before = {n: bmg.messages_by_id[n].string for n in touched_ids(names)}
  run_tweaks(fake, swift_sail=swift)

  lines = ["# R3.21: wwrando's post-randomization texts on the disc's BMG and font for one placement "
           "(make_posttext_fixture.py --disc; MD5 where the text keeps vanilla BMG text)",
           "P %s" % os.path.basename(placement_path)]
  changed = 0
  for n in touched_ids(names):
    s = bmg.messages_by_id[n].string
    changed += s != before[n]
    if keeps_disc_text(n, names):
      lines.append("H %d %s" % (n, hashlib.md5(s.encode("utf-8")).hexdigest()))
    else:
      lines.append("T %d %s" % (n, mwf.h(s)))
  with open(path, "w", encoding="ascii") as f:
    f.write("\n".join(lines) + "\n")
  print("%s: %d messages, %d changed" % (path, len(touched_ids(names)), changed), file=sys.stderr)


def main():
  args = sys.argv[1:]
  out_dir = os.path.join(oc.REPO_ROOT, "native", "check", "rando", "fixtures")
  disc = None
  placement = None
  while args:
    a = args.pop(0)
    if a == "--out-dir" and args:
      out_dir = args.pop(0)
    elif a == "--disc" and args:
      disc = args.pop(0)
    elif a == "--placement" and args:
      placement = args.pop(0)
    else:
      oc.die("usage: make_posttext_fixture.py [--out-dir DIR] [--disc GZLE01.iso --placement P.json]")
  write_synthetic(os.path.join(out_dir, "R3_21_posttext.txt"))
  if disc or placement:
    if not (disc and placement):
      oc.die("--disc and --placement go together")
    write_game(os.path.join(out_dir, "R3_21_posttext_game.txt"), disc, placement)


if __name__ == "__main__":
  main()
