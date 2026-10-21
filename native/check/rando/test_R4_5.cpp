// cos_rando_test_R4_5: the spoiler viewer's model (docs/RANDOMIZER_PLAN.md step R4.5,
// features/rando/pc_rando_spoilerview.h).
//
// 1. wwrando's spoiler logs (fixtures/R1_5_logs.txt, the "S" lines of each case): the parser finds
//    the header (seed, hash, permalink as the case records them), every sphere (numbered 1..n, one
//    of them holding "Ganon's Tower" / "Rooftop" -> "Defeat Ganondorf", not always the last: the
//    key-lunacy seed leaves Jalhalla for after it), and the locations by zone,
//    one per "loc" line of the case; every location of a sphere (not the "Boss"/"Rooftop" pseudo
//    entries) holds the same item in "All item locations:". The do_not_generate_spoiler_log case
//    (no spoiler log) is refused.
// 2. spoilerZonesFromPlacement on the log's own locations (full names rebuilt as "<zone> - <name>")
//    gives that section back line for line; on the TestSeed1 placement (R0_6_TestSeed1.json) one line
//    per location under its zone.
// 3. SpoilerPager: the confirmation (B cancels, A shows), the cursor (up/down, left/right by
//    heading), L/R switching section with its own cursor, B closing.
#include <cstdio>
#include <string>
#include <vector>

#include "pc_rando_seedfile.h"
#include "pc_rando_spoilerview.h"
#include "rando_test.h"

namespace {

struct Case {
    std::string name, seed, hash, permalink;
    std::string spoiler; // the S lines joined
    int locs = 0;
};

std::vector<Case> readCases(const std::string& text) {
    std::vector<Case> out;
    Case* c = nullptr;
    size_t b = 0;
    while (b < text.size()) {
        size_t e = text.find('\n', b);
        if (e == std::string::npos) {
            e = text.size();
        }
        const std::string line = text.substr(b, e - b);
        b = e + 1;
        const size_t t = line.find('\t');
        const std::string tag = line.substr(0, t);
        const std::string rest = t == std::string::npos ? std::string() : line.substr(t + 1);
        if (tag == "case") {
            out.push_back(Case());
            c = &out.back();
            const size_t t2 = rest.find('\t');
            c->name = rest.substr(0, t2);
            c->seed = rest.substr(t2 + 1);
        } else if (c == nullptr) {
            continue;
        } else if (tag == "hash") {
            c->hash = rest;
        } else if (tag == "permalink") {
            c->permalink = rest;
        } else if (tag == "loc") {
            c->locs++;
        } else if (tag == "S") {
            c->spoiler += rest + "\n";
        }
    }
    return out;
}

// The item of `zone`/`name` in a locations section ("" + false when missing).
bool findLocation(const std::vector<pcr::SpoilerLine>& locs, const std::string& zone, const std::string& name,
                  std::string* item) {
    std::string z;
    for (const pcr::SpoilerLine& l : locs) {
        if (l.depth == 0) {
            z = l.text;
        } else if (z == zone && l.text == name) {
            *item = l.item;
            return true;
        }
    }
    return false;
}

void checkLogs() {
    std::string text;
    RT_CHECK(rt::readFixture("R1_5_logs.txt", text));
    const std::vector<Case> cases = readCases(text);
    RT_CHECK_EQ((int)cases.size(), 9);
    int parsed = 0;
    for (const Case& c : cases) {
        pcr::SpoilerLog log;
        std::string err;
        const bool ok = pcr::spoilerLogParse(c.spoiler, &log, &err);
        if (c.name == "no_spoiler") {
            RT_CHECK(c.spoiler.empty());
            RT_CHECK(!ok);
            continue;
        }
        if (!RT_CHECK(ok)) {
            std::fprintf(stderr, "  case %s: %s\n", c.name.c_str(), err.c_str());
            continue;
        }
        parsed++;
        RT_CHECK_EQ(log.seed, c.seed);
        RT_CHECK_EQ(log.seedHash, c.hash);
        RT_CHECK_EQ(log.permalink, c.permalink);
        RT_CHECK(log.haveLocations);

        // spheres: "1".."n", each with zones and locations
        int sphere = 0, zone = 0;
        std::string zoneName;
        int rooftop = 0;
        for (const pcr::SpoilerLine& l : log.spheres) {
            if (l.depth == 0) {
                sphere++;
                RT_CHECK_EQ(l.text, std::to_string(sphere));
            } else if (l.depth == 1) {
                zone++;
                zoneName = l.text;
            } else {
                RT_CHECK_EQ(l.depth, 2);
                RT_CHECK(!l.item.empty());
                if (zoneName == "Ganon's Tower" && l.text == "Rooftop") {
                    RT_CHECK_EQ(l.item, std::string("Defeat Ganondorf"));
                    rooftop++;
                } else if (l.text != "Boss") {
                    std::string item;
                    const bool found = findLocation(log.locations, zoneName, l.text, &item);
                    if (!RT_CHECK(found && item == l.item)) {
                        std::fprintf(stderr, "  case %s: %s - %s: %s / %s\n", c.name.c_str(), zoneName.c_str(),
                                     l.text.c_str(), l.item.c_str(), item.c_str());
                    }
                }
            }
        }
        RT_CHECK(sphere >= 3);
        RT_CHECK(zone >= sphere);
        if (!RT_CHECK_EQ(rooftop, 1)) {
            std::fprintf(stderr, "  case %s: Rooftop %d times\n", c.name.c_str(), rooftop);
        }

        // locations: one per loc line; the placement builder gives the section back
        std::vector<std::string> names, items;
        std::string z;
        for (const pcr::SpoilerLine& l : log.locations) {
            if (l.depth == 0) {
                z = l.text;
                continue;
            }
            names.push_back(l.text == z ? z : z + " - " + l.text);
            items.push_back(l.item);
        }
        RT_CHECK_EQ((int)names.size(), c.locs);
        const std::vector<pcr::SpoilerLine> back = pcr::spoilerZonesFromPlacement(names, items);
        bool same = back.size() == log.locations.size();
        for (size_t i = 0; same && i < back.size(); i++) {
            same = back[i].depth == log.locations[i].depth && back[i].text == log.locations[i].text &&
                   back[i].item == log.locations[i].item;
        }
        RT_CHECK(same);
    }
    RT_CHECK_EQ(parsed, 8);

    pcr::SpoilerLog log;
    std::string err;
    RT_CHECK(!pcr::spoilerLogParse("Seed: X\n\nAll item locations:\nA:\n    B: C\n", &log, &err));
    RT_CHECK(!err.empty());
    RT_CHECK(!pcr::spoilerLogParse("", &log, &err));
    // a Windows line end and a name holding ':' (the last ": " ends it)
    RT_CHECK(pcr::spoilerLogParse("Seed: S\r\nPlaythrough progression spheres:\r\n1:\r\n  Zone:\r\n      A: b:  Item X\r\n", &log,
                                  &err));
    RT_CHECK_EQ((int)log.spheres.size(), 3);
    if (log.spheres.size() == 3) {
        RT_CHECK_EQ(log.spheres[2].text, std::string("A: b"));
        RT_CHECK_EQ(log.spheres[2].item, std::string("Item X"));
        RT_CHECK_EQ(log.spheres[1].text, std::string("Zone"));
    }
}

void checkPlacement() {
    std::string json;
    RT_CHECK(rt::readFixture("R0_6_TestSeed1.json", json));
    PcRandoSeed seed;
    std::string err;
    if (!RT_CHECK(pc_rando_seed_parse(json, &seed, &err))) {
        std::fprintf(stderr, "  %s\n", err.c_str());
        return;
    }
    const std::vector<pcr::SpoilerLine> lines =
        pcr::spoilerZonesFromPlacement(seed.location_names, seed.location_item_names);
    int zones = 0, entries = 0;
    for (const pcr::SpoilerLine& l : lines) {
        zones += l.depth == 0;
        entries += l.depth == 2;
    }
    RT_CHECK_EQ(entries, (int)seed.location_names.size());
    RT_CHECK(zones > 40 && zones < entries);
    RT_CHECK(!lines.empty() && lines[0].depth == 0 && lines[0].text == "Outset Island");
    RT_CHECK(lines.size() > 1 && lines[1].text == "Underneath Link's House");
}

void checkPager() {
    using pcr::SpoilerLine;
    const std::vector<SpoilerLine> spheres = {
        {0, "1", ""}, {1, "Outset Island", ""}, {2, "A", "x"}, {2, "B", "y"}, {0, "2", ""}, {1, "Z", ""}, {2, "C", "z"},
    };
    const std::vector<SpoilerLine> zones = {{0, "Outset Island", ""}, {2, "A", "x"}, {0, "Z", ""}, {2, "C", "z"}};
    pcr::SpoilerPager p;
    RT_CHECK(p.state() == pcr::SpoilerState::Closed);
    RT_CHECK(!p.press(pcr::kSpoilerConfirm));

    p.open(spheres, zones);
    RT_CHECK(p.state() == pcr::SpoilerState::Confirm);
    RT_CHECK(!p.press(pcr::kSpoilerDown)); // nothing moves before A
    RT_CHECK(p.state() == pcr::SpoilerState::Confirm);
    p.press(pcr::kSpoilerBack);
    RT_CHECK(p.state() == pcr::SpoilerState::Closed);
    RT_CHECK(p.lines(pcr::kSpoilerSpheres).empty());

    p.open(spheres, zones);
    RT_CHECK(p.press(pcr::kSpoilerConfirm));
    RT_CHECK(p.state() == pcr::SpoilerState::View);
    RT_CHECK_EQ(p.section(), (int)pcr::kSpoilerSpheres);
    RT_CHECK_EQ(p.headings(pcr::kSpoilerSpheres), 2);
    RT_CHECK_EQ(p.entries(pcr::kSpoilerSpheres), 3);
    RT_CHECK_EQ(p.entries(pcr::kSpoilerZones), 2);
    RT_CHECK_EQ(p.cursor(), 0);
    RT_CHECK(!p.press(pcr::kSpoilerUp)); // at the top
    RT_CHECK(p.press(pcr::kSpoilerDown));
    RT_CHECK(p.press(pcr::kSpoilerDown));
    RT_CHECK_EQ(p.cursor(), 2);
    RT_CHECK_EQ(p.heading(), 0);
    RT_CHECK(p.press(pcr::kSpoilerRight));
    RT_CHECK_EQ(p.cursor(), 4);
    RT_CHECK(!p.press(pcr::kSpoilerRight)); // no heading after the last
    for (int i = 0; i < 5; i++) {
        p.press(pcr::kSpoilerDown);
    }
    RT_CHECK_EQ(p.cursor(), 6); // the last line
    RT_CHECK(p.press(pcr::kSpoilerLeft)); // to its own heading
    RT_CHECK_EQ(p.cursor(), 4);
    RT_CHECK(p.press(pcr::kSpoilerLeft)); // to the one before
    RT_CHECK_EQ(p.cursor(), 0);

    RT_CHECK(p.press(pcr::kSpoilerTabR));
    RT_CHECK_EQ(p.section(), (int)pcr::kSpoilerZones);
    RT_CHECK_EQ(p.cursor(), 0);
    RT_CHECK(p.press(pcr::kSpoilerRight));
    RT_CHECK_EQ(p.cursor(), 2);
    RT_CHECK_EQ(p.lines()[p.cursor()].text, std::string("Z"));
    RT_CHECK(p.press(pcr::kSpoilerTabL));
    RT_CHECK_EQ(p.section(), (int)pcr::kSpoilerSpheres);
    RT_CHECK_EQ(p.cursor(), 0); // each section keeps its cursor
    p.press(pcr::kSpoilerTabL);
    RT_CHECK_EQ(p.cursor(), 2);

    p.press(pcr::kSpoilerBack);
    RT_CHECK(p.state() == pcr::SpoilerState::Closed);

    // empty sections
    p.open({}, {});
    p.press(pcr::kSpoilerConfirm);
    RT_CHECK(!p.press(pcr::kSpoilerDown));
    RT_CHECK_EQ(p.heading(), -1);
}

} // namespace

int main() {
    checkLogs();
    checkPlacement();
    checkPager();
    return rt::finish();
}
