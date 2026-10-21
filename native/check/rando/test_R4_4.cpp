// Unit test of plan step R4.4: the sidecar housekeeping and the file-select line
// (features/rando/pc_rando_sidecars.h).
//   - sidecarScanRecords finds slot records at any offset of a card image (the save file's two
//     copies of three slots), skips other bytes and does not list a hash twice;
//   - sidecarNameParse accepts exactly "switchwaker-rando-<8 lowercase hex>.json";
//   - sidecarClean over a scratch card folder: an orphan sidecar is removed; the sidecars named by a
//     record in a .gci file (slot 1, a copy of it in slot 2, and one only in the backup copy) and by
//     `keep` (pending/active) stay; other files (another name, uppercase hex, a .gci, notes) are not
//     touched; a dry run lists without removing; an unreadable .gci removes nothing;
//   - seedHashWords of TestSeed1 is wwrando's "Ivan Woods" (fixtures/R1_2_permalinks.txt, the same
//     permalink), and the info
//     lines read as documented.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "pc_rando_seedfile.h"
#include "pc_rando_sidecars.h"
#include "pc_rando_slot.h"
#include "rando_test.h"

namespace {

// The GameCube layout (m_Do_MemCardRWmng.h), only to build a realistic image: a GCI header, the
// banner block, then two card_savedata copies of 3 card_gamedata (0x770) at +8, mReserve at +0x718.
constexpr size_t kGciHeader = 0x40, kBlock = 0x2000, kGamedata = 0x770, kReserve = 0x718;

size_t reserveAt(int copy, int slot) {
    return kGciHeader + kBlock * (1 + copy) + 8 + kGamedata * slot + kReserve;
}

void hashOf(uint8_t h[16], uint8_t seed) {
    for (int i = 0; i < 16; i++) {
        h[i] = (uint8_t)(seed * 17 + i);
    }
}

bool writeBytes(const std::string& path, const void* data, size_t n) {
    FILE* f = fopen(path.c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    const bool ok = fwrite(data, 1, n, f) == n;
    fclose(f);
    return ok;
}

bool exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool contains(const std::vector<std::string>& v, const std::string& s) {
    for (const std::string& x : v) {
        if (x == s) {
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    // ---- names
    uint32_t v = 0;
    RT_CHECK(pcr::sidecarNameParse("switchwaker-rando-0badf00d.json", &v));
    RT_CHECK_EQ(v, 0x0badf00du);
    RT_CHECK(!pcr::sidecarNameParse("switchwaker-rando-0BADF00D.json", nullptr));
    RT_CHECK(!pcr::sidecarNameParse("switchwaker-rando-0badf00.json", nullptr));
    RT_CHECK(!pcr::sidecarNameParse("switchwaker-rando-0badf00d.json.tmp", nullptr));
    RT_CHECK(!pcr::sidecarNameParse("xswitchwaker-rando-0badf00d.json", nullptr));
    RT_CHECK(!pcr::sidecarNameParse("switchwaker-rando-zzzzzzzz.json", nullptr));

    // ---- a card image with records
    std::vector<uint8_t> gci(kGciHeader + 3 * kBlock, 0);
    for (size_t i = kGciHeader; i < gci.size(); i++) {
        gci[i] = (uint8_t)(i * 7); // noise
    }
    uint8_t h1[16], h2[16], h3[16], h4[16];
    hashOf(h1, 1);
    hashOf(h2, 2);
    hashOf(h3, 3);
    hashOf(h4, 4);
    pc_rando_slot_record_write(&gci[reserveAt(0, 0)], h1, 0); // slot 1
    pc_rando_slot_record_write(&gci[reserveAt(0, 1)], h1, 0); // slot 2: a copy of slot 1
    pc_rando_slot_record_write(&gci[reserveAt(1, 0)], h1, 0); // the backup copy
    pc_rando_slot_record_write(&gci[reserveAt(1, 2)], h2, 0); // only in the backup copy
    std::vector<pcr::SeedHash> found;
    pcr::sidecarScanRecords(gci.data(), gci.size(), &found);
    RT_CHECK_EQ(found.size(), (size_t)2);
    RT_CHECK(found.size() == 2 && memcmp(found[0].data(), h1, 16) == 0 && memcmp(found[1].data(), h2, 16) == 0);
    found.clear();
    pcr::sidecarScanRecords(gci.data(), reserveAt(0, 0) + 8 + 15, &found); // the hash cut off
    RT_CHECK_EQ(found.size(), (size_t)0);
    pcr::sidecarScanRecords(nullptr, 0, &found);
    RT_CHECK_EQ(found.size(), (size_t)0);

    // ---- clean a scratch card folder
    char tmpl[] = "/tmp/cos_rando_R4_4_XXXXXX";
    const char* dirc = mkdtemp(tmpl);
    RT_CHECK(dirc != nullptr);
    if (dirc == nullptr) {
        return rt::finish();
    }
    const std::string dir = dirc;
    const std::string n1 = pc_rando_slot_sidecar_name(h1), n2 = pc_rando_slot_sidecar_name(h2),
                      n3 = pc_rando_slot_sidecar_name(h3), n4 = pc_rando_slot_sidecar_name(h4);
    const char* other[] = {"switchwaker-rando-0BADF00D.json", "notes.txt", "switchwaker-rando-12.json",
                           "switchwaker-rando-0badf00d.json.tmp"};
    RT_CHECK(writeBytes(dir + "/01-GZLE-gczelda.gci", gci.data(), gci.size()));
    for (const std::string& n : {n1, n2, n3, n4}) {
        RT_CHECK(writeBytes(dir + "/" + n, "{}\n", 3));
    }
    for (const char* n : other) {
        RT_CHECK(writeBytes(dir + "/" + n, "x", 1));
    }
    pcr::SeedHash k4;
    memcpy(k4.data(), h4, 16);

    pcr::SidecarCleanResult dry = pcr::sidecarClean(dir, {k4}, true);
    RT_CHECK(dry.ok);
    RT_CHECK_EQ(dry.gciFiles, 1);
    RT_CHECK_EQ(dry.records, 2);
    RT_CHECK(dry.removed.size() == 1 && dry.removed[0] == n3);
    RT_CHECK(exists(dir + "/" + n3));

    pcr::SidecarCleanResult r = pcr::sidecarClean(dir, {k4}, false);
    RT_CHECK(r.ok);
    RT_CHECK(r.error.empty());
    RT_CHECK(r.removed.size() == 1 && r.removed[0] == n3);
    RT_CHECK_EQ(r.kept.size(), (size_t)3);
    RT_CHECK(contains(r.kept, n1) && contains(r.kept, n2) && contains(r.kept, n4));
    RT_CHECK(!exists(dir + "/" + n3));
    RT_CHECK(exists(dir + "/" + n1) && exists(dir + "/" + n2) && exists(dir + "/" + n4));
    for (const char* n : other) {
        RT_CHECK(exists(dir + "/" + n));
    }
    RT_CHECK(exists(dir + "/01-GZLE-gczelda.gci"));

    // without the keep, the pending/active sidecar is an orphan
    r = pcr::sidecarClean(dir, {}, false);
    RT_CHECK(r.ok && r.removed.size() == 1 && r.removed[0] == n4);
    // a second run has nothing to do
    r = pcr::sidecarClean(dir, {}, false);
    RT_CHECK(r.ok && r.removed.empty() && r.kept.size() == 2);

    // an unreadable .gci: nothing removed
    RT_CHECK(writeBytes(dir + "/" + n3, "{}\n", 3));
    const std::string locked = dir + "/02-GZLE-other.gci";
    RT_CHECK(writeBytes(locked, "x", 1));
    chmod(locked.c_str(), 0);
    FILE* probe = fopen(locked.c_str(), "rb"); // root can read anyway
    if (probe == nullptr) {
        r = pcr::sidecarClean(dir, {}, false);
        RT_CHECK(!r.ok && r.removed.empty() && !r.error.empty());
        RT_CHECK(exists(dir + "/" + n3));
    } else {
        fclose(probe);
    }
    chmod(locked.c_str(), 0644);
    unlink(locked.c_str());
    r = pcr::sidecarClean(dir, {}, false);
    RT_CHECK(r.ok && r.removed.size() == 1 && r.removed[0] == n3);

    // a missing folder
    r = pcr::sidecarClean(dir + "/nope", {}, false);
    RT_CHECK(!r.ok && !r.error.empty());
    r = pcr::sidecarClean("", {}, false);
    RT_CHECK(!r.ok);

    // clean up the scratch folder
    for (const std::string& n : {n1, n2, std::string("01-GZLE-gczelda.gci")}) {
        unlink((dir + "/" + n).c_str());
    }
    for (const char* n : other) {
        unlink((dir + "/" + n).c_str());
    }
    rmdir(dir.c_str());

    // ---- hash words and lines
    std::string json;
    PcRandoSeed seed;
    std::string err;
    RT_CHECK(rt::readFixture("R0_6_TestSeed1.json", json));
    RT_CHECK(pc_rando_seed_parse(json, &seed, &err));
    const std::string words = pcr::seedHashWords(seed);
    RT_CHECK_EQ(words, std::string("Ivan Woods"));
    using K = pcr::FileInfoKind;
    RT_CHECK_EQ(pcr::fileInfoText(K::Rando, seed.seed, words, "", false),
                std::string("Randomizer: TestSeed1  \xC2\xB7  Ivan Woods"));
    RT_CHECK_EQ(pcr::fileInfoText(K::Pending, seed.seed, words, "", false),
                std::string("New file: randomizer seed TestSeed1 (Ivan Woods)"));
    RT_CHECK_EQ(pcr::fileInfoText(K::Pending, seed.seed, words, "", true),
                std::string("Partida nueva: semilla del randomizer TestSeed1 (Ivan Woods)"));
    RT_CHECK_EQ(pcr::fileInfoText(K::Missing, "", "", n1, false),
                "Randomizer: seed file " + n1 + " missing: plays vanilla");
    RT_CHECK_EQ(pcr::fileInfoText(K::None, seed.seed, words, "", false), std::string());
    RT_CHECK_EQ(pcr::fileInfoText(K::Rando, "S", "", "", false), std::string("Randomizer: S"));
    PcRandoSeed noLink = seed;
    noLink.permalink.clear();
    RT_CHECK_EQ(pcr::seedHashWords(noLink), std::string());
    return rt::finish();
}
