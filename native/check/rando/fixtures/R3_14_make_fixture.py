#!/usr/bin/env python3
"""Writes the fixtures of the event_list.dat serializer (docs/RANDOMIZER_PLAN.md step R3.14).

    python3 -I native/check/rando/fixtures/R3_14_make_fixture.py [--disc <GZLE01.iso>]

R3_14_wwrando_evser.txt (cos_rando_test_R3_14): a synthetic event_list.dat built by wwrando's own
wwlib/events.py (no disc bytes), then the serializer edits of edit_synth() (add_event, add_actor,
add_action with int / int list / float / float list / vec3 / vec3 list / string properties, an
action inserted into an existing chain, an actor removed from an event, a longer string, an empty
int list, flags) saved by EventList.save_changes:

    src <hex>      the synthetic file (itself a save_changes output)
    out <hex>      the file after edit_synth() and save_changes
    flags <ids>    the flags get_unused_flag_id handed out, in order

R3_14_wwrando_disc.txt (smoke evser), only with --disc: for every files/res/Stage/<stage>/Stage.arc
that holds an event_list.dat, hashes only (no disc bytes):

    disc <stage> <size> <fnv64> <edited size> <edited fnv64>

where the first pair is the vanilla file (wwrando's save_changes gives it back byte-identical; the
tool checks so) and the second the file after edit_disc() (one new event with one actor and two
actions, and one action inserted before the last action of the first event's first actor) and
save_changes. test_R3_14.cpp and smokes/pc_rando_smoke_evser.cpp make the same edits in the same
order. Uses the oracle venv (native/tools/rando/setup_oracle.sh) through oracle_common.bootstrap().
"""
import os
import re
import struct
import sys
sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools", "rando"))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

from io import BytesIO  # noqa: E402
from gclib.gclib_file import GCLibFileEntry  # noqa: E402
from wwlib.events import EventList  # noqa: E402


def load(data):
  entry = GCLibFileEntry()
  entry.data = BytesIO(data)
  return EventList(entry)


def save(el):
  el.save_changes()
  return el.data.getvalue()


def fnv64(data):
  h = 0xcbf29ce484222325
  for b in data:
    h ^= b
    h = (h * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
  return h


def empty_file():
  header = bytearray(0x40)
  for i in range(7):
    struct.pack_into(">I", header, i * 8, 0x40)
  return bytes(header)


def build_src():
  el = load(empty_file())
  ev = el.add_event("Gp1_Get_Itm")
  ev.priority = 1
  ev.play_jingle = True
  link = ev.add_actor("Link")
  link.add_action("001n_wait", [("Timer", [30])])
  link.add_action("011get_item", [("prm0", [238])])
  link.add_action("FINISH")
  cam = ev.add_actor("CAMERA")
  cam.staff_type = 2
  cam.staff_identifier = 7
  cam.add_action("UNITRANS", [("Eye", (1.0, 2.0, 3.0)), ("Fovy", 60.0), ("Center", [(0.5, 0.25, -1.0), (9.0, 8.0, 7.0)])])
  cam.add_action("FIXEDFRM", [("Timer", [10]), ("Ratio", [0.5, 0.75])])
  cam.add_action("END", [("Timer", [5])])

  ev = el.add_event("TACT_HT")
  ev.priority = 3
  ev.starting_flags = [4, -1]
  ev.ending_flags = [1, 2, -1]
  tlink = ev.add_actor("Link")
  tlink.add_action("WAIT")
  tlink.add_action("059get_dance", [("prm0", [0])])
  hr = ev.add_actor("Hr")
  hr.staff_identifier = 3
  hr.add_action("MSG", [("msgNo", [1234]), ("text", "seven77"), ("name", "an eight")])
  hr.add_action("MSG", [("msgNo", [1235]), ("list", [1, 2, 3, 4])])
  tk = ev.add_actor("TIMEKEEPER")
  tk.staff_type = 4
  tk.add_action("WAIT", [("Timer", [1, 2, 3])])

  ev = el.add_event("EmptyEvt")
  ev.add_actor("DIRECTOR").add_action("WAIT")
  return save(el)


def edit_synth(el, flags):
  # The same edits, in the same order, as test_R3_14.cpp's editSynth.
  orig = el.get_unused_flag_id
  def unused():
    f = orig()
    flags.append(f)
    return f
  # Count the flags add_actor / add_action take.
  el.get_unused_flag_id = unused

  ev = el.add_event("PcrNewEvent")
  ev.priority = 2
  ev.starting_flags = [5, -1]
  a = ev.add_actor("Link")
  a.add_action("001n_wait", [("Timer", [30])])
  a.add_action("011get_item", [("prm0", [7]), ("Pos", (1.0, 2.0, 3.0)), ("Spd", 1.5),
                               ("msg", "hello there evser")])
  cam = ev.add_actor("CAMERA")
  cam.staff_type = 2
  cam.add_action("FIXEDFRM", [("Eye", [(1.0, 2.0, 3.0), (4.0, 5.0, 6.0)]), ("Fovy", [1.0, 2.0]),
                              ("Empty", [])])

  gp1 = el.events_by_name["Gp1_Get_Itm"]
  link = gp1.actors[0]
  ins = link.add_action("PCR_INS", [("Timer", [3])])
  link.actions.remove(ins)
  link.actions.insert(1, ins)

  tact = el.events_by_name["TACT_HT"]
  hr = tact.actors[1]
  tact.actors.remove(hr)
  # The removed actor's flags are not given back (wwrando never does).
  tk = tact.actors[1]
  tk.add_action("PCR_TK", [("msg", "a much longer string than before")])

  cam0 = gp1.actors[1]
  cam0.actions[1].properties[1].value = []
  cam0.actions[0].properties[1].value = 12.25
  ev.ending_flags = [a.actions[-1].flag_id_to_set, -1, -1]
  el.get_unused_flag_id = orig


def edit_disc(el):
  # The same edits, in the same order, as smokes/pc_rando_smoke_evser.cpp's editDisc.
  ev = el.add_event("PcrEvserTest")
  link = ev.add_actor("Link")
  link.add_action("001n", [("Timer", [30])])
  link.add_action("002n", [("prm0", [1, 2]), ("Pos", (1.0, -2.5, 3.0)), ("msg", "evser")])
  first = el.events[0]
  if first.actors:
    actor = first.actors[0]
    act = actor.add_action("PCR_ADD", [("Timer", [7])])
    actor.actions.remove(act)
    actor.actions.insert(len(actor.actions) - 1, act)


def write_synth():
  src = build_src()
  if save(load(src)) != src:
    oc.die("the synthetic file does not round-trip")
  el = load(src)
  flags = []
  edit_synth(el, flags)
  out = save(el)
  if save(load(out)) != out:
    oc.die("the edited synthetic file does not round-trip")
  path = os.path.join(HERE, "R3_14_wwrando_evser.txt")
  with open(path, "w") as f:
    f.write("src %s\nout %s\nflags %s\n" % (src.hex(), out.hex(), " ".join(str(x) for x in flags)))
  print("wrote %s (%d -> %d bytes, %d flags)" % (path, len(src), len(out), len(flags)))


def write_disc(iso):
  from gclib.gcm import GCM
  from gclib.rarc import RARC
  gcm = GCM(iso)
  gcm.read_entire_disc()
  lines = []
  for path in sorted(gcm.files_by_path):
    m = re.fullmatch(r"files/res/Stage/([^/]+)/Stage\.arc", path)
    if not m:
      continue
    arc = RARC(gcm.read_file_data(path))
    entry = arc.get_file_entry("event_list.dat")
    if entry is None:
      continue
    raw = entry.data.getvalue()
    if save(load(raw)) != raw:
      oc.die("%s: event_list.dat does not round-trip through wwrando" % path)
    el = load(raw)
    edit_disc(el)
    out = save(el)
    if save(load(out)) != out:
      oc.die("%s: the edited event_list.dat does not round-trip" % path)
    lines.append("disc %s %d %016x %d %016x" % (m.group(1), len(raw), fnv64(raw), len(out), fnv64(out)))
  path = os.path.join(HERE, "R3_14_wwrando_disc.txt")
  with open(path, "w") as f:
    f.write("\n".join(lines) + "\n")
  print("wrote %s (%d stages)" % (path, len(lines)))


def main():
  write_synth()
  if len(sys.argv) == 3 and sys.argv[1] == "--disc":
    write_disc(sys.argv[2])
  elif len(sys.argv) != 1:
    oc.die("usage: R3_14_make_fixture.py [--disc <GZLE01.iso>]")


if __name__ == "__main__":
  main()
