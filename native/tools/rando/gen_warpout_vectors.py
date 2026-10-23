#!/usr/bin/env python3
"""Boss warp-out vectors for cos_rando_test_R6_13 (plan step R6.13).

    python3 -I native/tools/rando/gen_warpout_vectors.py [out.txt]

Default output: native/check/rando/fixtures/R6_13_warpout.txt. The output is deterministic, so
regenerating must give a byte-identical file:

    python3 -I native/tools/rando/gen_warpout_vectors.py /tmp/x.txt && \\
        diff /tmp/x.txt native/check/rando/fixtures/R6_13_warpout.txt

It runs wwrando 1.10.0_9775811 (ref/wwrando, see README.md) in dry mode over the cases of
gen_entrance_vectors.py (the R6.11 fixture's cases), plus one with every entrance option off. For each
case it randomizes the entrances as gen_entrance_vectors.py does, then runs wwrando's own
EntranceRandomizer.update_all_boss_warp_out_destinations with its two writers replaced by recorders:
update_boss_warp_out_destination (the WARP_WIND_AFTER event's DIRECTOR NEXT action: Stage, RoomNo,
StartCode) and update_helmaroc_king_arena_ganon_door (M2tower/Room0 SCLS[1]). wwrando only saves the
entrances when the randomizer ran (BaseRandomizer.save: made_any_changes, i.e. is_enabled), so a case
with every entrance option off records "disabled". Tab-separated output:

    version  <VERSION>
    case     <seed>
    opt      <name> <value>        options that differ from Options() (R1.2 value format)
    conn     <exit|exit|...>       entrance_connections' values, in dict order (44)
    warp     <boss stage> <stage> <room> <spawn>    update_boss_warp_out_destination, call order
    scls     <stage> <room> <spawn>                 update_helmaroc_king_arena_ganon_door
    disabled                        the entrance randomizer is off: nothing is saved
    error    <text>                 randomize() raised (newlines as \\n)
    endcase
    end

Exit 0 written, 2 setup error.
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

import contextlib  # noqa: E402

import gen_entrance_vectors as gev  # noqa: E402


def main():
  out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    oc.REPO_ROOT, "native", "check", "rando", "fixtures", "R6_13_warpout.txt")
  oc.bootstrap()
  from version import VERSION
  from options.wwrando_options import Options
  defaults = Options()

  lines = [f"version\t{VERSION}"]
  for seed, values in [("EntOff", {})] + gev.cases():
    opts = oc.make_options(values)
    lines.append(f"case\t{seed}")
    for k in sorted(values):
      if getattr(opts, k) != getattr(defaults, k):
        lines.append(f"opt\t{k}\t{gev.opt_value(getattr(opts, k))}")
    try:
      r = oc.make_randomizer(seed, opts)
    except Exception as e:  # noqa: BLE001 - e.g. TooFewProgressionLocationsError in __init__
      lines.append("initerror\t" + str(e).replace("\n", "\\n"))
      lines.append("endcase")
      continue
    er = r.entrances
    if not er.is_enabled():
      lines.append("conn\t" + "|".join(er.entrance_connections.values()))
      lines.append("disabled")
      lines.append("endcase")
      continue
    try:
      with contextlib.redirect_stdout(sys.stderr):
        if r.boss_reqs.is_enabled():
          r.boss_reqs.randomize()
        er.randomize()
    except Exception as e:  # noqa: BLE001 - recorded for the test
      lines.append("error\t" + str(e).replace("\n", "\\n"))
      lines.append("endcase")
      continue
    lines.append("conn\t" + "|".join(er.entrance_connections.values()))

    def record_warp(boss_stage_name, outermost):
      lines.append(f"warp\t{boss_stage_name}\t{outermost.warp_out_stage_name}\t"
                   f"{outermost.warp_out_room_num}\t{outermost.warp_out_spawn_id}")

    def record_scls(hk_exit, outermost):
      assert (hk_exit.stage_name, hk_exit.room_num) == ("M2tower", 0)
      lines.append(f"scls\t{outermost.warp_out_stage_name}\t{outermost.warp_out_room_num}\t"
                   f"{outermost.warp_out_spawn_id}")

    er.update_boss_warp_out_destination = record_warp
    er.update_helmaroc_king_arena_ganon_door = record_scls
    er.update_all_boss_warp_out_destinations()
    lines.append("endcase")
  lines.append("end")
  with open(out_path, "w") as f:
    f.write("\n".join(lines) + "\n")
  return 0


if __name__ == "__main__":
  sys.exit(main())
