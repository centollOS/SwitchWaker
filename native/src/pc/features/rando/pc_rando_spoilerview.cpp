// The spoiler viewer's model (docs/RANDOMIZER_PLAN.md step R4.5; API in pc_rando_spoilerview.h).
#include "pc_rando_spoilerview.h"

#include "pc_rando_spoiler.h"

#include <algorithm>

namespace pcr {

namespace {

// The lines of `text` without their '\n' (and a '\r' before it).
std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> out;
    size_t b = 0;
    while (b <= text.size()) {
        size_t e = text.find('\n', b);
        if (e == std::string::npos) {
            e = text.size();
        }
        std::string line = text.substr(b, e - b);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        out.push_back(std::move(line));
        if (e == text.size()) {
            break;
        }
        b = e + 1;
    }
    return out;
}

bool startsWith(const std::string& s, const char* head) {
    const size_t n = std::char_traits<char>::length(head);
    return s.size() >= n && s.compare(0, n, head) == 0;
}

size_t indentOf(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && s[i] == ' ') {
        i++;
    }
    return i;
}

// "<name>:" -> name.
std::string heading(const std::string& s, size_t indent) {
    std::string h = s.substr(indent);
    if (!h.empty() && h.back() == ':') {
        h.pop_back();
    }
    return h;
}

// "<name>:<padding> <item>" (padRight(name + ":", w) + " " + item): the last ": " ends the name
// (no item name holds one).
SpoilerLine entry(const std::string& s, size_t indent) {
    SpoilerLine l;
    l.depth = 2;
    const std::string body = s.substr(indent);
    const size_t c = body.rfind(": ");
    if (c == std::string::npos) {
        l.text = heading(body, 0);
        return l;
    }
    l.text = body.substr(0, c);
    l.item = body.substr(c + 1 + indentOf(body.substr(c + 1)));
    return l;
}

} // namespace

bool spoilerLogParse(const std::string& text, SpoilerLog* out, std::string* err) {
    *out = SpoilerLog();
    const std::vector<std::string> lines = splitLines(text);
    enum { None, Spheres, Locations } section = None;
    bool haveSeed = false;
    for (const std::string& line : lines) {
        if (section == None) {
            if (startsWith(line, "Seed: ")) {
                out->seed = line.substr(6);
                haveSeed = true;
            } else if (startsWith(line, "Seed Hash: ")) {
                out->seedHash = line.substr(11);
            } else if (startsWith(line, "Permalink: ")) {
                out->permalink = line.substr(11);
            } else if (line == "Playthrough progression spheres:") {
                section = Spheres;
                out->haveSpheres = true;
            } else if (line == "All item locations:") {
                section = Locations;
                out->haveLocations = true;
            }
            continue;
        }
        if (line.empty()) {
            section = None; // each section ends with "\n\n\n"
            continue;
        }
        const size_t ind = indentOf(line);
        if (section == Spheres) {
            if (ind == 0) {
                out->spheres.push_back({0, heading(line, 0), ""});
            } else if (ind == 2) {
                out->spheres.push_back({1, heading(line, 2), ""});
            } else {
                out->spheres.push_back(entry(line, ind));
            }
        } else {
            if (ind == 0) {
                out->locations.push_back({0, heading(line, 0), ""});
            } else {
                out->locations.push_back(entry(line, ind));
            }
        }
    }
    if (!haveSeed || !out->haveSpheres) {
        if (err != nullptr) {
            *err = !haveSeed ? "no \"Seed: \" line" : "no \"Playthrough progression spheres:\" section";
        }
        return false;
    }
    return true;
}

std::vector<SpoilerLine> spoilerZonesFromPlacement(const std::vector<std::string>& names,
                                                   const std::vector<std::string>& items) {
    struct Zone {
        std::string name;
        std::vector<SpoilerLine> lines;
    };
    std::vector<Zone> zones;
    for (size_t i = 0; i < names.size(); i++) {
        std::string zone, specific;
        splitLocationName(names[i], zone, specific);
        Zone* z = nullptr;
        for (Zone& it : zones) {
            if (it.name == zone) {
                z = &it;
                break;
            }
        }
        if (z == nullptr) {
            zones.push_back(Zone{zone, {}});
            z = &zones.back();
        }
        z->lines.push_back({2, specific, i < items.size() ? items[i] : std::string()});
    }
    std::vector<SpoilerLine> out;
    out.reserve(names.size() + zones.size());
    for (Zone& z : zones) {
        out.push_back({0, z.name, ""});
        for (SpoilerLine& l : z.lines) {
            out.push_back(std::move(l));
        }
    }
    return out;
}

// ---- SpoilerPager ----------------------------------------------------------------------------------

void SpoilerPager::open(std::vector<SpoilerLine> spheres, std::vector<SpoilerLine> zones) {
    mLines[kSpoilerSpheres] = std::move(spheres);
    mLines[kSpoilerZones] = std::move(zones);
    mCursor[0] = mCursor[1] = 0;
    mSection = kSpoilerSpheres;
    mState = SpoilerState::Confirm;
}

void SpoilerPager::close() {
    mState = SpoilerState::Closed;
    mLines[0].clear();
    mLines[1].clear();
    mCursor[0] = mCursor[1] = 0;
}

bool SpoilerPager::press(uint32_t keys) {
    if (mState == SpoilerState::Closed || keys == 0) {
        return false;
    }
    if (mState == SpoilerState::Confirm) {
        if (keys & kSpoilerBack) {
            close();
        } else if (keys & kSpoilerConfirm) {
            mState = SpoilerState::View;
            return true;
        }
        return false;
    }
    if (keys & kSpoilerBack) {
        close();
        return false;
    }
    if (keys & (kSpoilerTabL | kSpoilerTabR)) {
        mSection = (mSection + 1) % kSpoilerSectionCount; // two sections: L and R both switch
        return true;
    }
    const std::vector<SpoilerLine>& ls = mLines[mSection];
    const int n = (int)ls.size();
    if (n == 0) {
        return false;
    }
    int& c = mCursor[mSection];
    const int before = c;
    if (keys & kSpoilerUp) {
        c = std::max(0, c - 1);
    } else if (keys & kSpoilerDown) {
        c = std::min(n - 1, c + 1);
    } else if (keys & kSpoilerLeft) {
        // the heading above the cursor's own one (or its own, when the cursor is below it)
        const int h = heading();
        int to = h;
        if (h == c) {
            to = 0;
            for (int i = c - 1; i >= 0; i--) {
                if (ls[i].depth == 0) {
                    to = i;
                    break;
                }
            }
        }
        c = std::max(0, to);
    } else if (keys & kSpoilerRight) {
        for (int i = c + 1; i < n; i++) {
            if (ls[i].depth == 0) {
                c = i;
                break;
            }
        }
    }
    return c != before;
}

int SpoilerPager::heading() const {
    const std::vector<SpoilerLine>& ls = mLines[mSection];
    for (int i = std::min(mCursor[mSection], (int)ls.size() - 1); i >= 0; i--) {
        if (ls[i].depth == 0) {
            return i;
        }
    }
    return -1;
}

int SpoilerPager::headings(int section) const {
    int n = 0;
    for (const SpoilerLine& l : mLines[section]) {
        n += l.depth == 0;
    }
    return n;
}

int SpoilerPager::entries(int section) const {
    int n = 0;
    for (const SpoilerLine& l : mLines[section]) {
        n += l.depth == 2;
    }
    return n;
}

} // namespace pcr
