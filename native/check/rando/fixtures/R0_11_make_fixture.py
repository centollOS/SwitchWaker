#!/usr/bin/env python3
"""Writes R0_11_wwrando_events.txt for cos_rando_test_R0_11 (docs/RANDOMIZER_PLAN.md step R0.11).

    python3 -I native/check/rando/fixtures/R0_11_make_fixture.py

Builds a small synthetic event_list.dat (no disc bytes) with wwrando's own wwlib/events.py
EventList (add_event / add_actor / add_action, save_changes): three events, actors with int, float,
vec3 and string properties and an int list. It then reads that file back with wwlib, applies the
edits tweaks.py and items.py make (change_event_item for an item and a song, actions[0:7],
actions.remove(a), remove + insert, actions[-1:], ending/starting flags, an action rename, int list
shrink, float and vec3 values), saves it and reads it again. It writes:

    src <hex>       the synthetic file
    before <line>   the logical dump of src (events, actors, actions, properties, as wwlib reads them)
    after <line>    the logical dump after wwrando's edits

The test makes the same edits in place with features/rando/pc_rando_event.h and checks its own dump
of the patched bytes equals "after" (the layouts differ: wwrando rewrites the file, the port only
relinks and overwrites in place). Uses the oracle venv (native/tools/rando/setup_oracle.sh) through
oracle_common.bootstrap().
"""
import os
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
  link.staff_type = 0
  link.add_action("001n_wait", [("Timer", [30])])
  link.add_action("011get_item", [("prm0", [238])])
  link.add_action("FINISH")
  cam = ev.add_actor("CAMERA")
  cam.staff_type = 2
  cam.staff_identifier = 7
  cam.add_action("UNITRANS", [("Eye", (1.0, 2.0, 3.0)), ("Fovy", 60.0)])
  cam.add_action("FIXEDFRM", [("Timer", [10])])
  cam.add_action("PAUSE")
  cam.add_action("END", [("Timer", [5])])
  hr = ev.add_actor("Hr")
  for i in range(9):
    hr.add_action("a%d" % i, [("Timer", [i])])

  ev = el.add_event("TACT_HT")
  ev.priority = 3
  ev.starting_flags = [4, -1]
  link = ev.add_actor("Link")
  link.add_action("WAIT")
  link.add_action("059get_dance", [("prm0", [0])])
  os0 = ev.add_actor("Os0")
  for i in range(10):
    props = [("msg", "hello world")] if i == 4 else [("Timer", [100 + i])]
    os0.add_action("x%d" % i, props)
  os1 = ev.add_actor("Os1")
  for i in range(5):
    os1.add_action("y%d" % i)
  tk = ev.add_actor("TIMEKEEPER")
  tk.staff_type = 4
  tk.add_action("WAIT", [("Timer", [1, 2, 3])])

  ev = el.add_event("EmptyEvt")
  ev.add_actor("DIRECTOR").add_action("WAIT")

  el.save_changes()
  return el.data.getvalue()


def change_event_item(el, event_index, actor_index, action_index, item_id):
  # randomizers/items.py change_event_item, with the item id instead of the name.
  action = el.events[event_index].actors[actor_index].actions[action_index]
  if 0x6D <= item_id <= 0x72:
    action.name = "059get_dance"
    action.properties[0].value = [item_id - 0x6D]
  else:
    action.name = "011get_item"
    action.properties[0].value = [item_id]


def edit(el):
  # The same edits, in the same order, as test_R0_11.cpp's applyEdits.
  gp1 = el.events_by_name["Gp1_Get_Itm"]
  tact = el.events_by_name["TACT_HT"]
  link, cam, hr = gp1.actors
  tlink, os0, os1, tk = tact.actors
  change_event_item(el, 0, 0, 1, 0x07)
  change_event_item(el, 1, 0, 1, 0x6F)
  hr.actions = hr.actions[0:7]
  cam.actions.remove(cam.actions[2])
  a = os0.actions[3]
  os0.actions.remove(a)
  os0.actions.insert(7, a)
  os1.actions = os1.actions[-1:]
  gp1.ending_flags = [link.actions[-1].flag_id_to_set, cam.actions[-1].flag_id_to_set,
                      hr.actions[-1].flag_id_to_set]
  cam.actions[1].starting_flags[0] = hr.actions[2].flag_id_to_set
  tact.starting_flags[1] = 77
  tk.actions[0].properties[0].value = [9]
  cam.actions[0].properties[1].value = 45.5
  cam.actions[0].properties[0].value = (1.0, -7.25, 3.0)
  link.actions[0].name = "002n_wait2"


def fhex(v):
  return struct.pack(">f", v).hex()


def dump(el):
  out = []
  for i, ev in enumerate(el.events):
    out.append("event %d %s prio %d start %d %d end %d %d %d jingle %d" % (
      i, ev.name, ev.priority, *ev.starting_flags, *ev.ending_flags, int(ev.play_jingle)))
    for actor in ev.actors:
      out.append(" actor %s id %d type %d flag %d" % (
        actor.name, actor.staff_identifier, actor.staff_type, actor.flag_id_to_set))
      for action in actor.actions:
        out.append("  action %s dup %d start %d %d %d flag %d" % (
          action.name, action.duplicate_id, *action.starting_flags, action.flag_id_to_set))
        for prop in action.properties:
          v = prop.value
          if prop.data_type == 4:
            out.append("   prop %s str %s" % (prop.name, v))
            continue
          vals = v if isinstance(v, list) else [v]
          if prop.data_type == 3:
            out.append("   prop %s int %s" % (prop.name, " ".join(str(x) for x in vals)))
          elif prop.data_type == 0:
            out.append("   prop %s float %s" % (prop.name, " ".join(fhex(x) for x in vals)))
          elif prop.data_type == 1:
            out.append("   prop %s vec %s" % (prop.name, " ".join(fhex(c) for t in vals for c in t)))
  return out


def main():
  src = build_src()
  before = dump(load(src))
  el = load(src)
  edit(el)
  el.save_changes()
  after = dump(load(el.data.getvalue()))
  lines = ["src " + src.hex()]
  lines += ["before " + x for x in before]
  lines += ["after " + x for x in after]
  path = os.path.join(HERE, "R0_11_wwrando_events.txt")
  with open(path, "w") as f:
    f.write("\n".join(lines) + "\n")
  print("wrote %s (%d bytes src, %d dump lines)" % (path, len(src), len(after)))


if __name__ == "__main__":
  main()
