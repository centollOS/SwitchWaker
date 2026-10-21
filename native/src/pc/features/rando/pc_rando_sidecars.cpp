// Sidecar housekeeping and the file-select info line, pure part (docs/RANDOMIZER_PLAN.md step R4.4;
// API in pc_rando_sidecars.h). Part of cos_rando_core: no game headers or calls here.
#include "pc_rando_sidecars.h"

#include "pc_rando_options.h"
#include "pc_rando_slot.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace pcr {

namespace {

constexpr char kPrefix[] = "switchwaker-rando-";
constexpr char kSuffix[] = ".json";
constexpr size_t kPrefixLen = sizeof kPrefix - 1;
constexpr size_t kSuffixLen = sizeof kSuffix - 1;
constexpr int kHashOffset = 8; // pc_rando_slot.h: the record's hash at +0x08

bool endsWithGci(const char* name) {
    const size_t n = strlen(name);
    if (n < 4) {
        return false;
    }
    const char* e = name + n - 4;
    return e[0] == '.' && (e[1] == 'g' || e[1] == 'G') && (e[2] == 'c' || e[2] == 'C') && (e[3] == 'i' || e[3] == 'I');
}

bool readFile(const std::string& path, std::vector<uint8_t>* out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    out->clear();
    uint8_t buf[16384];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        out->insert(out->end(), buf, buf + n);
    }
    const bool ok = ferror(f) == 0;
    fclose(f);
    return ok;
}

bool isRegularFile(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

} // namespace

void sidecarScanRecords(const uint8_t* data, size_t len, std::vector<SeedHash>* out) {
    if (data == nullptr || len < (size_t)kHashOffset + 16) {
        return;
    }
    for (size_t i = 0; i + kHashOffset + 16 <= len; i++) {
        if (data[i] != 'W' || !pc_rando_slot_record_is_rando(data + i)) {
            continue;
        }
        SeedHash h;
        memcpy(h.data(), data + i + kHashOffset, 16);
        if (std::find(out->begin(), out->end(), h) == out->end()) {
            out->push_back(h);
        }
    }
}

bool sidecarNameParse(const std::string& name, uint32_t* hash32) {
    if (name.size() != kPrefixLen + 8 + kSuffixLen || name.compare(0, kPrefixLen, kPrefix) != 0 ||
        name.compare(kPrefixLen + 8, kSuffixLen, kSuffix) != 0) {
        return false;
    }
    uint32_t v = 0;
    for (size_t i = kPrefixLen; i < kPrefixLen + 8; i++) {
        const char c = name[i];
        uint32_t d;
        if (c >= '0' && c <= '9') {
            d = (uint32_t)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = (uint32_t)(c - 'a' + 10);
        } else {
            return false;
        }
        v = (v << 4) | d;
    }
    if (hash32 != nullptr) {
        *hash32 = v;
    }
    return true;
}

SidecarCleanResult sidecarClean(const std::string& dir, const std::vector<SeedHash>& keep, bool dryRun) {
    SidecarCleanResult r;
    if (dir.empty()) {
        r.error = "no card folder";
        return r;
    }
    DIR* d = opendir(dir.c_str());
    if (d == nullptr) {
        r.error = "cannot open " + dir + ": " + strerror(errno);
        return r;
    }
    std::vector<std::string> gcis, sidecars;
    while (const dirent* e = readdir(d)) {
        const char* name = e->d_name;
        if (name[0] == '.') {
            continue;
        }
        if (endsWithGci(name)) {
            gcis.push_back(name);
        } else if (sidecarNameParse(name, nullptr)) {
            sidecars.push_back(name);
        }
    }
    closedir(d);
    std::sort(gcis.begin(), gcis.end());
    std::sort(sidecars.begin(), sidecars.end());

    std::vector<SeedHash> used = keep;
    std::vector<SeedHash> found;
    std::vector<uint8_t> bytes;
    for (const std::string& g : gcis) {
        const std::string path = dir + "/" + g;
        if (!isRegularFile(path)) {
            continue;
        }
        if (!readFile(path, &bytes)) {
            r.error = "cannot read " + g + ": nothing removed";
            return r;
        }
        r.gciFiles++;
        sidecarScanRecords(bytes.data(), bytes.size(), &found);
    }
    r.records = (int)found.size();
    used.insert(used.end(), found.begin(), found.end());

    for (const std::string& s : sidecars) {
        const std::string path = dir + "/" + s;
        if (!isRegularFile(path)) {
            continue;
        }
        bool inUse = false;
        for (const SeedHash& h : used) {
            if (pc_rando_slot_sidecar_name(h.data()) == s) {
                inUse = true;
                break;
            }
        }
        if (inUse) {
            r.kept.push_back(s);
            continue;
        }
        if (dryRun || unlink(path.c_str()) == 0) {
            r.removed.push_back(s);
        } else {
            r.failed.push_back(s);
            if (r.error.empty()) {
                r.error = "cannot remove " + s + ": " + strerror(errno);
            }
        }
    }
    r.ok = r.failed.empty();
    return r;
}

std::string seedHashWords(const PcRandoSeed& seed) {
    Options o;
    const Json* j = seed.options.find("do_not_generate_spoiler_log");
    o.do_not_generate_spoiler_log = j != nullptr && j->asBool(false);
    return seedHash(seed.permalink, o);
}

std::string fileInfoText(FileInfoKind kind, const std::string& name, const std::string& words,
                         const std::string& sidecar, bool es) {
    switch (kind) {
    case FileInfoKind::None:
        return std::string();
    case FileInfoKind::Rando: {
        std::string s = std::string("Randomizer: ") + name;
        if (!words.empty()) {
            s += "  \xC2\xB7  " + words;
        }
        return s;
    }
    case FileInfoKind::Missing:
        return std::string(es ? "Randomizer: falta el archivo de semilla " : "Randomizer: seed file ") + sidecar +
               (es ? ": se juega sin randomizer" : " missing: plays vanilla");
    case FileInfoKind::Pending: {
        std::string s = std::string(es ? "Partida nueva: semilla del randomizer " : "New file: randomizer seed ") + name;
        if (!words.empty()) {
            s += " (" + words + ")";
        }
        return s;
    }
    }
    return std::string();
}

} // namespace pcr
