#!/usr/bin/env python3
"""Writes R5_3_hints_game.txt for the smoke COS_RANDO_SMOKE=hoho (docs/RANDOMIZER_PLAN.md step R5.3).

    python3 -I native/check/rando/fixtures/R5_3_make_fixture.py --disc GZLE01.iso \
        native/check/rando/fixtures/R5_3_TestSeed1.json native/check/rando/fixtures/R5_3_Heavy5.json

Each placement was made by our generator (cos_rando_cli, the port's seed key). For each, wwrando
1.10.0_9775811 is run (dry) on the same seed and options with the same seed key, and its hints must
equal the placement's "hints" value (ww_verify_hints.hints_to_json); then wwrando's own
HintsRandomizer._save runs on the disc's zel_00.bmg and message font (rock_24_20_4i_usa.bfn) and
rotate_hoho_to_face_hint on the disc's sea stage and rooms (flexible_hint_locations is code, not
data: the smoke checks that hunk on its own). The fixture holds what the game must show:

    P <placement file name>                 a section (the lines below belong to it)
    B <msg> <text_box_type>                 the disc BMG's INF1 text box type of a hint message
    T <msg> <text>                          the text _save leaves in msg (UTF-8 hex), every message
                                            it writes, in _save's order
    R <room> <chunk> <index> <y_rot>        every Ah actor of the Ho Ho rooms' room.dzr (chunk ACTR,
                                            ACT0..ACTb, entry index in that chunk) and its y_rot after
                                            _save (vanilla for a Ho Ho whose hints name no island)

The texts are built from wwrando's strings and item names; only the line breaks come from the disc
font's widths. No disc bytes are written.
"""
import argparse
import json
import os
import sys
import types
sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.join(HERE, "..", "..", "..", "tools", "rando")
sys.path.insert(0, TOOLS)
import oracle_common as oc  # noqa: E402

oc.bootstrap()
import ww_verify_hints  # noqa: E402

from randomizers.hints import HintsRandomizer  # noqa: E402

MSGS = ([837, 12015, 12016, 12017] + [13026 + i for i in range(1, 50)] + [14001 + i for i in range(10)] +
        [1502, 3443, 3444, 3445, 3446, 3447, 3448])


def h(s):
  return s.encode("utf-8").hex() if s else "-"


class Disc:
  def __init__(self, path):
    from gclib.bfn import BFN
    from gclib.bmg import BMG
    from gclib.gcm import GCM
    from gclib.rarc import RARC
    self.RARC = RARC
    self.gcm = GCM(path)
    self.gcm.read_entire_disc()
    self.arcs = {}
    self.bmg_arc = self.get_arc("files/res/Msg/bmgres.arc")
    font_arc = self.get_arc("files/res/Msg/fontres.arc")
    self.font = font_arc.get_file("rock_24_20_4i_usa.bfn", BFN)
    self.BMG = BMG

  def get_arc(self, path):
    if path not in self.arcs:
      arc = self.RARC(self.gcm.read_file_data(path))
      arc.read()
      self.arcs[path] = arc
    return self.arcs[path]

  def fresh(self):
    """A fresh copy of the disc's BMG and sea stage files (each placement starts from vanilla)."""
    self.arcs = {k: v for k, v in self.arcs.items() if "Stage/sea" not in k}
    arc = self.RARC(self.gcm.read_file_data("files/res/Msg/bmgres.arc"))
    arc.read()
    return arc.get_file("zel_00.bmg", self.BMG)


def save_on_disc(hr, disc):
  """HintsRandomizer._save on the disc's BMG, font and sea rooms (no code patch)."""
  bmg = disc.fresh()
  fake = types.SimpleNamespace(**vars(hr))
  fake.HOHO_INDEX_TO_ISLAND_NUM = HintsRandomizer.HOHO_INDEX_TO_ISLAND_NUM
  fake.rando = types.SimpleNamespace(bmg=bmg, bfn=disc.font, get_arc=disc.get_arc,
                                     island_name_to_number=hr.rando.island_name_to_number,
                                     entrances=hr.rando.entrances)
  for name in ("check_item_can_be_hinted_at", "update_savage_labyrinth_hint_tablet",
               "update_big_octo_great_fairy_item_name_hint", "update_fishmen_hints", "update_hoho_hints",
               "update_korl_hints", "rotate_hoho_to_face_hint"):
    setattr(fake, name, getattr(HintsRandomizer, name).__get__(fake))
  fake.check_item_can_be_hinted_at = hr.check_item_can_be_hinted_at
  before = {n: bmg.messages_by_id[n].string for n in MSGS}
  types_ = {n: bmg.messages_by_id[n].text_box_type.value for n in MSGS}
  # _save without patcher.apply_patch (code; the d_a_bigelf hunk).
  fake.update_savage_labyrinth_hint_tablet(hr.floor_30_hint, hr.floor_50_hint, hr.hint_importance)
  written = [837]
  if any(hr.check_item_can_be_hinted_at(n) for n in hr.rando.all_randomized_progress_items):
    fake.update_big_octo_great_fairy_item_name_hint(hr.octo_fairy_hint, hr.hint_importance)
    written += [12015, 12016, 12017]
    for placement in hr.hints_per_placement:
      if placement == "fishmen_hints":
        fake.update_fishmen_hints()
        written += [13026 + i for i in sorted(hr.island_to_fishman_hint)]
      elif placement == "hoho_hints":
        fake.update_hoho_hints()
        written += [14001 + i for i in sorted(hr.hoho_index_to_hints)]
      elif placement == "korl_hints":
        fake.update_korl_hints(hr.hints_per_placement["korl_hints"])
        written += [1502, 3443, 3444, 3445, 3446, 3447, 3448]
  texts = [(n, bmg.messages_by_id[n].string) for n in written]
  for n, s in texts:
    assert s != before[n] or n in (837,), "message %d unchanged" % n
  return types_, texts


def hoho_rotations(disc):
  from wwlib.dzx import DZx, ACTR
  rows = []
  for room in sorted(set(HintsRandomizer.HOHO_INDEX_TO_ISLAND_NUM.values())):
    dzr = disc.get_arc("files/res/Stage/sea/Room%d.arc" % room).get_file("room.dzr", DZx)
    for ch in dzr.chunks:
      if ch.chunk_type != ACTR:
        continue
      for i, e in enumerate(ch.entries):
        if e.name == "Ah":
          rows.append((room, ch.fourcc, i, e.y_rot))
  return rows


def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--disc", required=True)
  ap.add_argument("--out", default=os.path.join(HERE, "R5_3_hints_game.txt"))
  ap.add_argument("placements", nargs="+")
  a = ap.parse_args()
  oc.set_seed_key(ww_verify_hints.SWITCHWAKER_SEED_KEY)
  disc = Disc(a.disc)
  # The vanilla rotations (for the log).
  disc.fresh()
  vanilla = {(r, c, i): y for r, c, i, y in hoho_rotations(disc)}
  lines = ["# R5.3: wwrando's hint texts and Ho Ho rotations on the disc's BMG, font and sea rooms for our "
           "generator's placements (R5_3_make_fixture.py --disc; no disc bytes)"]
  for path in a.placements:
    placement = oc.read_json(path)
    opts = oc.make_options(placement["options"])
    r = oc.make_randomizer(placement["seed"], opts)
    oc.randomize(r)
    ours = placement.get("hints")
    theirs = ww_verify_hints.hints_to_json(r)
    if json.dumps(ours, sort_keys=True) != json.dumps(theirs, sort_keys=True):
      oc.die("%s: the placement's hints differ from wwrando's" % path, 1)
    types_, texts = save_on_disc(r.hints, disc)
    lines.append("P %s" % os.path.basename(path))
    lines += ["B %d %d" % (n, types_[n]) for n in MSGS]
    lines += ["T %d %s" % (n, h(s)) for n, s in texts]
    turned = 0
    for room, chunk, i, y in hoho_rotations(disc):
      lines.append("R %d %s %d %d" % (room, chunk, i, y))
      turned += y != vanilla[(room, chunk, i)]
    print("%s: %d messages, %d Ah actors turned" % (path, len(texts), turned), file=sys.stderr)
  with open(a.out, "w", encoding="ascii", newline="\n") as f:
    f.write("\n".join(lines) + "\n")
  print("wrote %s" % a.out, file=sys.stderr)


if __name__ == "__main__":
  main()
