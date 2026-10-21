#!/usr/bin/env python3
"""Writes R5_2_hint_text.txt for cos_rando_test_R5_2 (docs/RANDOMIZER_PLAN.md step R5.2).

    python3 -I native/check/rando/fixtures/R5_2_make_fixture.py

Runs wwrando 1.10.0_9775811 (dry, no disc) on the seeds and option sets of R5_1_make_fixture.py,
then wwrando's own hint text functions (randomizers/hints.py HintsRandomizer:
update_savage_labyrinth_hint_tablet, update_big_octo_great_fairy_item_name_hint,
update_fishmen_hints, update_hoho_hints, update_korl_hints, in _save's order) on a made-up BMG
(every message a fixed text box type) and the made-up width table of make_wrap_fixture.py. Each run
is the real HintsRandomizer after randomize_all with only cryptic_hints, hint_importance and
instant_text_boxes changed (they only change the texts) and, in the last runs, the Savage Labyrinth
validity forced; rotate_hoho_to_face_hint (sea data, R5.3) is not run. No disc data.

Lines (strings as UTF-8 hex, "-" for an empty one):

    F <256 widths> <replacement width>      the width table (as R3_20_wrap.txt)
    B <msg> <text_box_type>                 the made-up BMG's box type of each hint message
    T <index> <text>                        a text (each distinct text once)
    K <name> <cryptic> <importance> <instant> <keylunacy> <v30> <v50> <forced> <hints json>
                                            a run: its flags (0/1), check_item_can_be_hinted_at of
                                            the two floors' rewards (forced = 1 when the fixture set
                                            them instead of wwrando) and the hints, as our
                                            generator's "hints" value (ww_verify_hints.hints_to_json)
    S <msg> <index>                         a text the run leaves in message msg (after its K line)
"""
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
import make_wrap_fixture as mwf  # noqa: E402  (the made-up font)

from gclib.bmg import Message, TextBoxType  # noqa: E402
from randomizers.hints import HintsRandomizer  # noqa: E402

HEAVY = {"num_path_hints": 6, "num_barren_hints": 6, "num_item_hints": 5, "num_location_hints": 6,
         "korl_hints": True, "prioritize_remote_hints": True, "hint_importance": True,
         "cryptic_hints": True}

# R5_1_make_fixture.py's cases.
CASES = [
  ("TestSeed1", {}),
  ("HintSeed7", {}),
  ("Heavy5", HEAVY),
  ("Heavy12", HEAVY),
  ("KeyHints3", dict(HEAVY, keylunacy=True)),
  ("AllCats2", dict({"num_path_hints": 4, "num_barren_hints": 4, "hint_importance": True},
                    **{p: True for p in __import__("check_seeds").progression_options()})),
  ("OneHint1", {"num_item_hints": 1, "num_location_hints": 0, "num_path_hints": 0, "num_barren_hints": 0,
                "fishmen_hints": True, "hoho_hints": True, "korl_hints": True}),
  ("NoPlacements4", {"fishmen_hints": False, "hoho_hints": False, "korl_hints": False}),
]

# Cases run with every (cryptic, importance, instant) combination; the others use their own options.
ALL_FLAGS = ("TestSeed1", "Heavy5", "KeyHints3")
# Case whose Savage Labyrinth validity is also forced to each of the four combinations (with plain
# names and importance).
FORCE_VALIDITY = "Heavy5"

MSGS = ([837, 12015, 12016, 12017] + [13026 + i for i in range(1, 50)] + [14001 + i for i in range(10)] +
        [1502, 3443, 3444, 3445, 3446, 3447, 3448])
BOX_TYPES = [0, 9, 0, 2, 0xE, 6, 0]


def box_type(msg):
  return BOX_TYPES[msg % len(BOX_TYPES)]


def fake_hints(hr, cryptic, importance, instant, validity=None):
  """A stand-in self for HintsRandomizer's update_* methods: the real one's state, a made-up BMG."""
  msgs = {}
  for n in MSGS:
    m = Message.__new__(Message)
    m.text_box_type = TextBoxType(box_type(n))
    m.string = "vanilla %d" % n
    msgs[n] = m
  fake = types.SimpleNamespace(**vars(hr))
  fake.rando = types.SimpleNamespace(bmg=types.SimpleNamespace(messages_by_id=msgs), bfn=mwf.syn_font())
  fake.options = types.SimpleNamespace(instant_text_boxes=instant)
  fake.cryptic_hints = cryptic
  fake.hint_importance = importance
  if validity is None:
    fake.check_item_can_be_hinted_at = hr.check_item_can_be_hinted_at
  else:
    v30, v50 = validity
    fake.check_item_can_be_hinted_at = (
      lambda item: v30 if item == hr.floor_30_hint.reward else v50 if item == hr.floor_50_hint.reward
      else hr.check_item_can_be_hinted_at(item))
  fake.rotate_hoho_to_face_hint = lambda *a: None
  return fake, msgs


def save_texts(hr, fake):
  """HintsRandomizer._save without the patch and the Ho Ho rotation."""
  HintsRandomizer.update_savage_labyrinth_hint_tablet(fake, hr.floor_30_hint, hr.floor_50_hint, fake.hint_importance)
  if not any(hr.check_item_can_be_hinted_at(n) for n in hr.rando.all_randomized_progress_items):
    return
  HintsRandomizer.update_big_octo_great_fairy_item_name_hint(fake, hr.octo_fairy_hint, fake.hint_importance)
  for placement in hr.hints_per_placement:
    if placement == "fishmen_hints":
      HintsRandomizer.update_fishmen_hints(fake)
    elif placement == "hoho_hints":
      HintsRandomizer.update_hoho_hints(fake)
    elif placement == "korl_hints":
      HintsRandomizer.update_korl_hints(fake, hr.hints_per_placement["korl_hints"])


def main():
  oc.set_seed_key(ww_verify_hints.SWITCHWAKER_SEED_KEY)
  h = mwf.h
  texts = {}
  runs = []

  def text_index(s):
    if s not in texts:
      texts[s] = len(texts)
    return texts[s]

  def run(name, r, cryptic, importance, instant, validity=None):
    hr = r.hints
    fake, msgs = fake_hints(hr, cryptic, importance, instant, validity)
    save_texts(hr, fake)
    if validity is None:
      validity = (hr.check_item_can_be_hinted_at(hr.floor_30_hint.reward),
                  hr.check_item_can_be_hinted_at(hr.floor_50_hint.reward))
      forced = 0
    else:
      forced = 1
    hints = json.dumps(ww_verify_hints.hints_to_json(r), separators=(",", ":"))
    flags = [cryptic, importance, instant, bool(r.options.keylunacy), validity[0], validity[1]]
    k = "K %s %s %d %s" % (name, " ".join(str(int(bool(x))) for x in flags), forced, h(hints))
    written = [(n, text_index(msgs[n].string)) for n in MSGS if msgs[n].string != "vanilla %d" % n]
    runs.append((k, written))

  for seed, values in CASES:
    opts = oc.make_options(values)
    r = oc.make_randomizer(seed, opts)
    oc.randomize(r)
    if seed in ALL_FLAGS:
      for c in (False, True):
        for i in (False, True):
          for t in (False, True):
            run(seed, r, c, i, t)
    else:
      run(seed, r, opts.cryptic_hints, opts.hint_importance, opts.instant_text_boxes)
    if seed == FORCE_VALIDITY:
      for v in ((False, False), (True, False), (False, True), (True, True)):
        # Plain names: a forced-valid junk reward has no cryptic text (wwrando never gets there).
        run(seed, r, False, True, opts.instant_text_boxes, v)

  lines = ["# R5.2: wwrando's hint texts on a made-up BMG and font (R5_2_make_fixture.py)",
           "F " + " ".join(str(w) for w in mwf.SYN_WIDTHS) + " %d" % mwf.SYN_REPLACEMENT]
  lines += ["B %d %d" % (n, box_type(n)) for n in MSGS]
  lines += ["T %d %s" % (i, h(s)) for s, i in texts.items()]
  for k, written in runs:
    lines.append(k)
    lines += ["S %d %d" % w for w in written]
  path = os.path.join(HERE, "R5_2_hint_text.txt")
  with open(path, "w", encoding="ascii", newline="\n") as f:
    f.write("\n".join(lines) + "\n")
  print("wrote %s (%d runs, %d texts)" % (path, len(runs), len(texts)), file=sys.stderr)


if __name__ == "__main__":
  main()
