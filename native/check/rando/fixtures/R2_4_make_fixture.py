#!/usr/bin/env python3
"""Writes R2_4_events.json, the placement of step R2.4's event smoke (docs/RANDOMIZER_PLAN.md).

    python3 -I native/check/rando/fixtures/R2_4_make_fixture.py

It is R0_6_identity.json (R0.4's make_test_placement.py --kind identity) with the items of 8 of the
11 event locations swapped among themselves, so the pool multiset is unchanged and every kind of
change_event_item edit is exercised: a song into an 011get_item action (Maggie's Father, Mila), a song
into a 046pget action (Orca, Big Octo fairy), and items into 059get_dance actions (Tott: Hurricane
Spin, Wind Shrine, Tower of the Gods tablet, Cyclos). The other three keep their vanilla item (the
rename of the vanilla 011_get_item / 011get_item actions). Not meant to be beatable.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))

SWAPS = {
    "Windfall Island - Maggie's Father - Give 20 Skull Necklaces": "Song of Passing",
    "Windfall Island - Tott - Teach Rhythm": "Hurricane Spin",
    "Outset Island - Orca - Give 10 Knight's Crests": "Wind's Requiem",
    "Dragon Roost Island - Wind Shrine": "Treasure Chart 2",
    "Two-Eye Reef - Big Octo Great Fairy": "Command Melody",
    "Tower of the Gods - Stone Tablet": "Magic Meter Upgrade",
    "The Great Sea - Cyclos": "Empty Bottle",
    "Windfall Island - Mila - Follow the Thief": "Ballad of Gales",
}


def main():
    with open(os.path.join(HERE, "R0_6_identity.json")) as f:
        placement = json.load(f)
    locs = placement["locations"]
    before = sorted(locs[name] for name in SWAPS)
    for name, item in SWAPS.items():
        assert name in locs, name
        locs[name] = item
    assert sorted(SWAPS.values()) == before, "the swaps must keep the pool multiset"
    placement["seed"] = "R2_4_events"
    with open(os.path.join(HERE, "R2_4_events.json"), "w") as f:
        json.dump(placement, f, indent=1)
        f.write("\n")


if __name__ == "__main__":
    main()
