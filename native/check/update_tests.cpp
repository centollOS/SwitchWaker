// cos_update_tests: host tests of the update's pure part (native/src/pc/features/pc_update_core.h,
// issues #27 and #28, docs/AUTO_UPDATE_PLAN.md):
//
//   ninja -C build/native-mac cos_update_tests && build/native-mac/cos_update_tests
//
// Versions and their order, SHA-256 (FIPS 180-4's examples), the GitHub release JSON, the install
// names, the zip reader (stored and deflated entries, CRC, truncated and non-zip files) and the
// install journal; then the whole flow of pc_update.cpp (compiled with COS_UPDATE_HOST_TEST, file://
// fixtures in a temporary directory): check, install, first start confirmed, a first start that does
// not get past the logo (rollback, not offered again at start), an interrupted swap, a bad checksum,
// a zip with a file outside switch/switchwaker/, a release without this region's zip, a failed
// check and a development version. pc_update_core.cpp and pc_update.cpp need no game code, so this is a plain host program (cos_pc_tests
// links the game's operator new, which needs JKRHeap). Prints "ok" and exits 0 when every check
// passes; otherwise prints each failed check and exits 1.
#include "../src/pc/features/pc_update_core.h"
#include "pc/pc_update.h"

#include <zlib.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

int g_failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expr);         \
            ++g_failures;                                                                          \
        }                                                                                          \
    } while (0)


// A zip as `zip -qr` writes one: local headers, data, central directory, end record. deflated=true
// stores each entry deflated (raw, as zip does).
std::string makeZip(const std::vector<std::pair<std::string, std::string>>& files, bool deflated) {
    auto put16 = [](std::string& s, uint32_t v) { s += (char)(v & 0xFF); s += (char)((v >> 8) & 0xFF); };
    auto put32 = [&](std::string& s, uint32_t v) { put16(s, v & 0xFFFF); put16(s, v >> 16); };
    std::string out, cd;
    for (const auto& [name, data] : files) {
        std::string packed = data;
        if (deflated) {
            z_stream zs{};
            deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
            packed.resize(deflateBound(&zs, (uLong)data.size()));
            zs.next_in = (Bytef*)data.data();
            zs.avail_in = (uInt)data.size();
            zs.next_out = (Bytef*)packed.data();
            zs.avail_out = (uInt)packed.size();
            deflate(&zs, Z_FINISH);
            packed.resize(zs.total_out);
            deflateEnd(&zs);
        }
        const uint32_t crc = (uint32_t)crc32(0, (const Bytef*)data.data(), (uInt)data.size());
        const uint32_t offset = (uint32_t)out.size();
        put32(out, 0x04034b50); put16(out, 20); put16(out, 0); put16(out, deflated ? 8 : 0);
        put32(out, 0); put32(out, crc); put32(out, (uint32_t)packed.size()); put32(out, (uint32_t)data.size());
        put16(out, (uint32_t)name.size()); put16(out, 0);
        out += name;
        out += packed;
        put32(cd, 0x02014b50); put16(cd, 20); put16(cd, 20); put16(cd, 0); put16(cd, deflated ? 8 : 0);
        put32(cd, 0); put32(cd, crc); put32(cd, (uint32_t)packed.size()); put32(cd, (uint32_t)data.size());
        put16(cd, (uint32_t)name.size()); put16(cd, 0); put16(cd, 0); put16(cd, 0); put16(cd, 0);
        put32(cd, 0); put32(cd, offset);
        cd += name;
    }
    const uint32_t cdOffset = (uint32_t)out.size();
    out += cd;
    put32(out, 0x06054b50); put16(out, 0); put16(out, 0);
    put16(out, (uint32_t)files.size()); put16(out, (uint32_t)files.size());
    put32(out, (uint32_t)cd.size()); put32(out, cdOffset); put16(out, 0);
    return out;
}

std::string readAll(const char* path) {
    std::string s;
    if (FILE* f = std::fopen(path, "rb")) {
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
            s.append(buf, n);
        }
        std::fclose(f);
    }
    return s;
}

void testUpdateCore() {
    using namespace pc::update;
    // Versions: a build past a tag is not older than that release; only X.Y.Z builds check.
    {
        Version v, r;
        CHECK(parseVersion("0.5.0", &v) && v.major == 0 && v.minor == 5 && v.patch == 0 && v.after == 0);
        CHECK(parseVersion("v1.12.3", &v) && v.major == 1 && v.minor == 12 && v.patch == 3);
        CHECK(parseVersion("0.4.2-12", &v) && v.patch == 2 && v.after == 12);
        CHECK(parseVersion("0.5.0-3-gabcdef1", &v) && v.after == 3);
        CHECK(parseVersion("0.5.0-dirty", &v) && v.after == 0);
        CHECK(!parseVersion("dev", &v));
        CHECK(!parseVersion("ceadab8", &v));
        CHECK(!parseVersion("0.5", &v));
        CHECK(!parseVersion("0.5.0x", &v));
        CHECK(!parseVersion(nullptr, &v));
        parseVersion("0.5.0", &v);
        CHECK(parseVersion("v0.5.1", &r) && isNewer(r, v));
        CHECK(parseVersion("v0.6.0", &r) && isNewer(r, v));
        CHECK(parseVersion("v1.0.0", &r) && isNewer(r, v));
        CHECK(parseVersion("v0.5.0", &r) && !isNewer(r, v));
        CHECK(parseVersion("v0.4.9", &r) && !isNewer(r, v));
        parseVersion("0.5.0-7", &v);
        CHECK(parseVersion("v0.5.0", &r) && !isNewer(r, v));
        CHECK(parseVersion("v0.5.1", &r) && isNewer(r, v));
        CHECK(sameTag("v0.5.1", "0.5.1") && sameTag("0.5.1", "v0.5.1") && !sameTag("v0.5.1", "0.5.10"));
        CHECK(!sameTag("", ""));
    }
    // SHA-256: FIPS 180-4's examples, and a message across several blocks.
    {
        auto hex = [](const std::string& s) {
            Sha256 sha;
            sha.update(s.data(), s.size());
            uint8_t d[32];
            sha.final(d);
            return toHex(d, 32);
        };
        CHECK(hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        CHECK(hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        CHECK(hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
        CHECK(hex(std::string(1000000, 'a')) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
        CHECK(parseSha256Line("51b43ba95c6a64ad0ff859e539f0aca1ffb4f3464cab18d83854ebc48991329b  SwitchWaker-v0.5.0-switch.zip\n") ==
              "51b43ba95c6a64ad0ff859e539f0aca1ffb4f3464cab18d83854ebc48991329b");
        CHECK(parseSha256Line("51B43BA95C6A64AD0FF859E539F0ACA1FFB4F3464CAB18D83854EBC48991329B") ==
              "51b43ba95c6a64ad0ff859e539f0aca1ffb4f3464cab18d83854ebc48991329b");
        CHECK(parseSha256Line("51b43ba9  x").empty());
        CHECK(parseSha256Line("<html>Not Found</html>").empty());
    }
    // The release JSON: the fields read, the rest skipped, escapes decoded; malformed answers fail.
    {
        const std::string json =
            "{\"url\":\"https://api.github.com/x\",\"html_url\":\"https://github.com/centollOS/SwitchWaker/releases/tag/v0.5.1\","
            "\"id\":123,\"author\":{\"login\":\"Pulpparty\",\"site_admin\":false,\"n\":null},\"tag_name\":\"v0.5.1\","
            "\"draft\":false,\"prerelease\":false,\"name\":\"SwitchWaker v0.5.1 \\u00e9\\ud83d\\ude00\\\"q\\\"\","
            "\"assets\":[{\"name\":\"SwitchWaker-v0.5.1-switch.zip\",\"size\":9518038,\"uploader\":{\"id\":1},"
            "\"browser_download_url\":\"https://github.com/centollOS/SwitchWaker/releases/download/v0.5.1/SwitchWaker-v0.5.1-switch.zip\"},"
            "{\"name\":\"SwitchWaker-v0.5.1-switch.zip.sha256\",\"size\":96,\"browser_download_url\":\"https://x/y.sha256\"}],"
            "\"body\":\"## Changes\\r\\n- a\\tb [x](y) \\/ \\\\\",\"reactions\":{\"+1\":2.5e1,\"list\":[1,-2,[true]]}}";
        Release r;
        std::string err;
        CHECK(parseRelease(json, &r, &err));
        CHECK(r.tag == "v0.5.1");
        CHECK(r.name == "SwitchWaker v0.5.1 \xC3\xA9\xF0\x9F\x98\x80\"q\"");
        CHECK(r.htmlUrl == "https://github.com/centollOS/SwitchWaker/releases/tag/v0.5.1");
        CHECK(r.assets.size() == 2);
        const Asset* a = findAsset(r, zipNameFor(r.tag, "GZLE01"));
        CHECK(a != nullptr && a->size == 9518038 &&
              a->url == "https://github.com/centollOS/SwitchWaker/releases/download/v0.5.1/SwitchWaker-v0.5.1-switch.zip");
        CHECK(findAsset(r, zipNameFor(r.tag, "GZLP01")) == nullptr);
        CHECK(zipNameFor("v0.5.1", "GZLP01") == "SwitchWaker-v0.5.1-switch-eur.zip");
        CHECK(zipNameFor("v0.5.1", "GZLJ01").empty());
        CHECK(parseRelease("{\"tag_name\":\"v1.0.0\",\"name\":null}", &r, &err) && r.name.empty() && r.assets.empty());
        CHECK(!parseRelease("{\"message\":\"API rate limit exceeded\"}", &r, &err) && err == "no tag_name");
        CHECK(!parseRelease("{\"tag_name\":\"v1\"", &r, &err));
        CHECK(!parseRelease("{\"tag_name\":\"v1\"} x", &r, &err));
        CHECK(!parseRelease("[1,2]", &r, &err));
        CHECK(!parseRelease("", &r, &err));
        CHECK(!parseRelease("<html>", &r, &err));
    }
    // Release notes: Markdown to the menu's plain Latin-1 lines.
    {
        Release r;
        std::string err;
        CHECK(parseRelease("{\"tag_name\":\"v1.0.0\",\"body\":\"## New\\r\\n- **Bold** [#15](https://x/15) and `code`\"}", &r, &err));
        CHECK(r.body == "## New\r\n- **Bold** [#15](https://x/15) and `code`");
        CHECK(parseRelease("{\"tag_name\":\"v1.0.0\",\"body\":null}", &r, &err) && r.body.empty());
        const std::string md =
            "SwitchWaker runs \xE2\x80\x9CThe Wind Waker\xE2\x80\x9D \xE2\x80\x94 natively\xE2\x80\xA6 \xF0\x9F\x8E\x89 caf\xC3\xA9\r\n"
            "\r\n\r\n"
            "## New\r\n"
            "\r\n"
            "- **Button remapping** ([#15](https://github.com/x/issues/15)): a new **Controls** tab\r\n"
            "  - Pick a row, then press `ZL` \xE2\x86\x92 done\r\n"
            "* star item with ![img](https://x/a.png) and <https://example.com>\r\n"
            "<!-- a comment -->\r\n"
            "## Install / update\r\n"
            "| Your disc | Download |\r\n"
            "|---|---|\r\n"
            "| USA | `x.zip` |\r\n"
            "### Sub of install\r\n"
            "skipped too\r\n"
            "## Known issues\r\n"
            "| a | b |\r\n"
            "|:-|-:|\r\n"
            "| one | **two** |\r\n"
            "Escaped \\*star\\* and 2 * 3\r\n"
            "\r\n";
        const std::string want =
            "SwitchWaker runs \"The Wind Waker\" - natively...  caf\xC3\xA9\n"
            "# New\n"
            "- Button remapping (#15): a new Controls tab\n"
            "  - Pick a row, then press ZL -> done\n"
            "- star item with img and https://example.com\n"
            "# Known issues\n"
            "a \xC2\xB7 b\n"
            "one \xC2\xB7 two\n"
            "Escaped *star* and 2 * 3\n";
        const std::string got = notesFromMarkdown(md);
        CHECK(got == want);
        if (got != want) {
            std::fprintf(stderr, "notes:\n%s---\n", got.c_str());
        }
        CHECK(notesFromMarkdown("").empty());
        CHECK(notesFromMarkdown("\n\n").empty());
    }
    // Install names: only plain files under switch/switchwaker/.
    {
        std::string n;
        CHECK(installName("switch/switchwaker/switchwaker.nro", &n) && n == "switchwaker.nro");
        CHECK(installName("switch/switchwaker/initial_dksh_cache.bin", &n) && n == "initial_dksh_cache.bin");
        CHECK(!installName("switch/switchwaker/", &n));
        CHECK(!installName("switch/", &n));
        CHECK(!installName("switch/switchwaker/native/user/settings.ini", &n));
        CHECK(!installName("switch/switchwaker/..", &n));
        CHECK(!installName("switch/switchwaker/a\\b", &n));
        CHECK(!installName("switch/other/x.nro", &n));
        CHECK(!installName("../switch/switchwaker/x", &n));
    }
    // The zip reader: both methods, entries checked against their CRC.
    {
        std::string big;
        for (int i = 0; i < 300000; i++) {
            big += (char)('a' + (i * 7 + i / 13) % 26);
        }
        const std::vector<std::pair<std::string, std::string>> files = {
            {"switch/", ""}, {"switch/switchwaker/switchwaker.nro", big}, {"switch/switchwaker/empty.bin", ""},
            {"switch/switchwaker/initial_pipeline_cache.db", "pipeline"}};
        char dir[] = "/tmp/cos_update_test_XXXXXX";
        CHECK(mkdtemp(dir) != nullptr);
        const std::string zipPath = std::string(dir) + "/r.zip", outPath = std::string(dir) + "/out";
        for (bool deflated : {false, true}) {
            const std::string zip = makeZip(files, deflated);
            FILE* f = std::fopen(zipPath.c_str(), "wb");
            std::fwrite(zip.data(), 1, zip.size(), f);
            std::fclose(f);
            f = std::fopen(zipPath.c_str(), "rb");
            std::vector<ZipEntry> entries;
            std::string err;
            CHECK(zipList(f, &entries, &err) && entries.size() == 4);
            for (size_t k = 0; k < entries.size() && k < files.size(); k++) {
                CHECK(entries[k].name == files[k].first && entries[k].size == files[k].second.size());
                CHECK(zipExtract(f, entries[k], outPath.c_str(), &err));
                CHECK(readAll(outPath.c_str()) == files[k].second);
            }
            // A wrong CRC: the entry fails and leaves no file behind.
            ZipEntry bad = entries[1];
            bad.crc ^= 1;
            CHECK(!zipExtract(f, bad, outPath.c_str(), &err) && err.find("CRC") != std::string::npos);
            CHECK(access(outPath.c_str(), F_OK) != 0);
            std::fclose(f);
        }
        // Not a zip; a truncated zip.
        FILE* f = std::fopen(zipPath.c_str(), "wb");
        std::fputs("<html>Not Found</html>", f);
        std::fclose(f);
        f = std::fopen(zipPath.c_str(), "rb");
        std::vector<ZipEntry> entries;
        std::string err;
        CHECK(!zipList(f, &entries, &err));
        std::fclose(f);
        const std::string zip = makeZip(files, true);
        f = std::fopen(zipPath.c_str(), "wb");
        std::fwrite(zip.data(), 1, zip.size() / 2, f);
        std::fclose(f);
        f = std::fopen(zipPath.c_str(), "rb");
        CHECK(!zipList(f, &entries, &err));
        std::fclose(f);
        // sha256File agrees with the in-memory digest.
        f = std::fopen(zipPath.c_str(), "wb");
        std::fputs("abc", f);
        std::fclose(f);
        CHECK(sha256File(zipPath.c_str()) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        CHECK(sha256File((std::string(dir) + "/missing").c_str()).empty());
        std::remove(zipPath.c_str());
        std::remove(outPath.c_str());
        rmdir(dir);
    }
    // The journal round trip; bad journals are refused.
    {
        Journal j;
        j.state = "installing";
        j.tag = "v0.5.1";
        j.from = "0.5.0";
        j.names = {"switchwaker.nro", "initial_pipeline_cache.db", "initial_dksh_cache.bin"};
        j.existed = {"switchwaker.nro", "initial_pipeline_cache.db"};
        Journal back;
        CHECK(parseJournal(formatJournal(j), &back));
        CHECK(back.state == j.state && back.tag == j.tag && back.from == j.from && back.names == j.names &&
              back.existed == j.existed);
        CHECK(parseJournal("state=booting\r\ntag=v1.0.0\r\nnames=a.nro\r\n", &back) && back.existed.empty());
        CHECK(!parseJournal("state=weird\ntag=v1\nnames=a\n", &back));
        CHECK(!parseJournal("state=installed\nnames=a\n", &back));
        CHECK(!parseJournal("state=installed\ntag=v1\nnames=\n", &back));
        CHECK(!parseJournal("state=installed\ntag=v1\nnames=../x\n", &back));
        CHECK(!parseJournal("", &back));
    }
}

// ---- the whole flow (pc_update.cpp) ------------------------------------------------------------------

extern "C" void pc_update_test_reset(void);

void writeAll(const std::string& path, const std::string& data) {
    FILE* f = std::fopen(path.c_str(), "wb");
    std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
}

bool fileExists(const std::string& path) { return access(path.c_str(), F_OK) == 0; }

PcUpdateInfo waitIdle(int seconds = 10) {
    PcUpdateInfo info;
    for (int k = 0; k < seconds * 200; k++) {
        pc_update_get_info(&info);
        if (info.status != PC_UPDATE_CHECKING && info.status != PC_UPDATE_DOWNLOADING &&
            info.status != PC_UPDATE_INSTALLING) {
            // the worker clears its job just after publishing
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            pc_update_get_info(&info);
            return info;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return info;
}

struct Fixture {
    std::string root, dir, rel;
    std::string newNro = std::string(200000, 'N') + "new nro", newDb = "new pipeline db", newDksh = "new dksh";

    // A release v0.5.1 whose USA zip holds `files`; the .sha256 is the zip's unless `badSha`.
    void release(const std::vector<std::pair<std::string, std::string>>& files, bool badSha = false,
                 bool usaZip = true) {
        const std::string zipName = "SwitchWaker-v0.5.1-switch.zip";
        const std::string zip = makeZip(files, true);
        writeAll(rel + "/" + zipName, zip);
        pc::update::Sha256 sha;
        sha.update(zip.data(), zip.size());
        uint8_t d[32];
        sha.final(d);
        std::string hex = pc::update::toHex(d, 32);
        if (badSha) {
            hex[0] = hex[0] == '0' ? '1' : '0';
        }
        writeAll(rel + "/" + zipName + ".sha256", hex + "  " + zipName + "\n");
        const std::string name = usaZip ? zipName : "SwitchWaker-v0.5.1-switch-eur.zip";
        writeAll(rel + "/latest.json",
                 "{\"tag_name\":\"v0.5.1\",\"name\":\"SwitchWaker v0.5.1\",\"html_url\":\"https://example/r\","
                 "\"body\":\"## New\\r\\n- Faster **ZL**\\r\\n## Install / update\\r\\n- copy\",\"assets\":["
                 "{\"name\":\"" + name + "\",\"size\":" + std::to_string(zip.size()) +
                 ",\"browser_download_url\":\"file://" + rel + "/" + zipName + "\"},"
                 "{\"name\":\"" + name + ".sha256\",\"size\":96,\"browser_download_url\":\"file://" + rel + "/" + zipName +
                 ".sha256\"}]}");
    }
    void goodRelease() {
        release({{"switch/", ""}, {"switch/switchwaker/", ""}, {"switch/switchwaker/initial_dksh_cache.bin", newDksh},
                 {"switch/switchwaker/initial_pipeline_cache.db", newDb}, {"switch/switchwaker/switchwaker.nro", newNro}});
    }
    // The installed version 0.5.0: an NRO and a pipeline cache, no DKSH cache (a file the update adds).
    void oldInstall() {
        writeAll(dir + "/switchwaker.nro", "old nro");
        writeAll(dir + "/initial_pipeline_cache.db", "old db");
        std::remove((dir + "/initial_dksh_cache.bin").c_str());
    }
    bool isOld() {
        return readAll((dir + "/switchwaker.nro").c_str()) == "old nro" &&
               readAll((dir + "/initial_pipeline_cache.db").c_str()) == "old db" && !fileExists(dir + "/initial_dksh_cache.bin");
    }
    bool isNew() {
        return readAll((dir + "/switchwaker.nro").c_str()) == newNro &&
               readAll((dir + "/initial_pipeline_cache.db").c_str()) == newDb &&
               readAll((dir + "/initial_dksh_cache.bin").c_str()) == newDksh;
    }
    bool noLeftovers() {
        for (const char* n : {"switchwaker.nro", "initial_pipeline_cache.db", "initial_dksh_cache.bin"}) {
            if (fileExists(dir + "/" + n + ".old") || fileExists(dir + "/" + n + ".new")) {
                return false;
            }
        }
        return true;
    }
    std::string journal() { return readAll((dir + "/native/update/state.txt").c_str()); }
    // A new process of version `v`.
    void start(const char* v) {
        setenv("COS_UPDATE_VERSION", v, 1);
        pc_update_test_reset();
        pc_update_boot();
    }
    bool installFrom050() {
        oldInstall();
        start("0.5.0");
        pc_update_check(1, "");
        PcUpdateInfo info = waitIdle();
        if (info.status != PC_UPDATE_AVAILABLE || pc_update_install() != 1) {
            return false;
        }
        info = waitIdle();
        return info.status == PC_UPDATE_INSTALLED;
    }
};

void testUpdateFlow() {
    Fixture fx;
    char root[] = "/tmp/cos_update_flow_XXXXXX";
    CHECK(mkdtemp(root) != nullptr);
    fx.root = root;
    fx.dir = fx.root + "/switchwaker";
    fx.rel = fx.root + "/release";
    mkdir(fx.dir.c_str(), 0777);
    mkdir(fx.rel.c_str(), 0777);
    setenv("COS_UPDATE_DIR", fx.dir.c_str(), 1);
    setenv("COS_UPDATE_URL", ("file://" + fx.rel + "/latest.json").c_str(), 1);
    fx.goodRelease();

    // A development version never checks.
    fx.oldInstall();
    fx.start("dev");
    pc_update_check(1, "");
    PcUpdateInfo info = waitIdle();
    CHECK(info.status == PC_UPDATE_DISABLED);

    // Up to date.
    fx.start("0.5.1-2");
    pc_update_check(1, "");
    info = waitIdle();
    CHECK(info.status == PC_UPDATE_UP_TO_DATE && std::strcmp(info.latest, "v0.5.1") == 0);
    CHECK(pc_update_install() == 0);

    // Check and install: the new files in place, the old ones kept as .old, the journal "installed".
    fx.start("0.5.0");
    pc_update_check(1, "v0.5.0");
    info = waitIdle();
    CHECK(info.status == PC_UPDATE_AVAILABLE && std::strcmp(info.latest, "v0.5.1") == 0 &&
          std::strcmp(info.title, "SwitchWaker v0.5.1") == 0 && info.fromStart && !info.skipped && info.canInstall);
    {
        char notes[256];
        CHECK(pc_update_get_notes(notes, sizeof(notes)) > 0 && std::strcmp(notes, "# New\n- Faster ZL\n") == 0);
    }
    pc_update_check(1, "v0.5.1"); // the player chose not to be told about v0.5.1
    info = waitIdle();
    CHECK(info.status == PC_UPDATE_AVAILABLE && info.skipped);
    pc_update_check(0, "v0.5.1"); // the menu's check says it anyway
    info = waitIdle();
    CHECK(info.status == PC_UPDATE_AVAILABLE && !info.skipped);
    CHECK(pc_update_install() == 1);
    info = waitIdle();
    CHECK(info.status == PC_UPDATE_INSTALLED);
    CHECK(info.done == info.total && info.total > 0);
    CHECK(fx.isNew());
    CHECK(readAll((fx.dir + "/switchwaker.nro.old").c_str()) == "old nro");
    CHECK(fileExists(fx.dir + "/initial_pipeline_cache.db.old") && !fileExists(fx.dir + "/initial_dksh_cache.bin.old"));
    CHECK(fx.journal().find("state=installed\n") == 0 && fx.journal().find("existed=switchwaker.nro") == std::string::npos &&
          fx.journal().find("existed=") != std::string::npos);
    CHECK(!fileExists(fx.dir + "/native/update/SwitchWaker-v0.5.1-switch.zip"));

    // The first start of v0.5.1: "booting" until a scene after the logo runs, then clean.
    fx.start("0.5.1");
    CHECK(fx.journal().find("state=booting\n") == 0);
    pc_update_frame(0);
    pc_update_get_info(&info);
    CHECK(info.boot == PC_UPDATE_BOOT_NONE && fx.journal().find("state=booting\n") == 0);
    pc_update_frame(1);
    pc_update_get_info(&info);
    CHECK(info.boot == PC_UPDATE_BOOT_FIRST_START && std::strcmp(info.bootTag, "v0.5.1") == 0);
    CHECK(fx.journal().empty() && fx.noLeftovers() && fx.isNew());
    fx.start("0.5.1"); // and nothing happens at the next one
    pc_update_get_info(&info);
    CHECK(info.boot == PC_UPDATE_BOOT_NONE && fx.isNew());

    // A first start that never gets past the logo: the next start puts 0.5.0 back and remembers.
    CHECK(fx.installFrom050());
    fx.start("0.5.1"); // ... and it crashes before the title
    fx.start("0.5.1");
    pc_update_get_info(&info);
    CHECK(info.boot == PC_UPDATE_BOOT_FAILED_BACK && std::strcmp(info.bootTag, "v0.5.1") == 0);
    CHECK(fx.isOld() && fx.noLeftovers() && fx.journal().empty());
    fx.start("0.5.0"); // the restored version says so once, and does not offer v0.5.1 at start
    pc_update_get_info(&info);
    CHECK(info.boot == PC_UPDATE_BOOT_FAILED_BACK);
    pc_update_check(1, "");
    info = waitIdle();
    CHECK(info.status == PC_UPDATE_AVAILABLE && info.skipped);
    fx.start("0.5.0");
    pc_update_get_info(&info);
    CHECK(info.boot == PC_UPDATE_BOOT_NONE);
    pc_update_check(0, "");
    info = waitIdle();
    CHECK(info.status == PC_UPDATE_AVAILABLE && !info.skipped);
    // a successful install forgets the failure
    CHECK(pc_update_install() == 1);
    info = waitIdle();
    CHECK(info.status == PC_UPDATE_INSTALLED && !fileExists(fx.dir + "/native/update/failed.txt"));
    fx.start("0.5.1");
    pc_update_frame(1);

    // A swap interrupted after the NRO moved: the next start (the old NRO's) puts it back.
    fx.oldInstall();
    writeAll(fx.dir + "/switchwaker.nro.old", "old nro");
    writeAll(fx.dir + "/switchwaker.nro", "new nro, half installed");
    writeAll(fx.dir + "/initial_pipeline_cache.db.new", "new db");
    writeAll(fx.dir + "/native/update/state.txt",
             "state=installing\ntag=v0.5.1\nfrom=0.5.0\nnames=switchwaker.nro,initial_pipeline_cache.db,initial_dksh_cache.bin\n"
             "existed=switchwaker.nro,initial_pipeline_cache.db\n");
    fx.start("0.5.0");
    pc_update_get_info(&info);
    CHECK(info.boot == PC_UPDATE_BOOT_ROLLED_BACK && fx.isOld() && fx.noLeftovers() && fx.journal().empty());

    // Leftovers without a journal (a kill while unpacking) go at the next start.
    writeAll(fx.dir + "/switchwaker.nro.new", "x");
    writeAll(fx.dir + "/native/update/SwitchWaker-v0.5.1-switch.zip", "partial");
    writeAll(fx.dir + "/notes.txt.old", "the player's");
    fx.start("0.5.0");
    CHECK(fx.noLeftovers() && !fileExists(fx.dir + "/native/update/SwitchWaker-v0.5.1-switch.zip") &&
          fileExists(fx.dir + "/notes.txt.old"));

    // A journal of a version this build is not (files copied by hand): kept files, journal gone.
    writeAll(fx.dir + "/switchwaker.nro.old", "older");
    writeAll(fx.dir + "/native/update/state.txt", "state=installed\ntag=v0.5.1\nfrom=0.5.0\nnames=switchwaker.nro\nexisted=switchwaker.nro\n");
    fx.start("0.6.0");
    CHECK(fx.journal().empty() && fx.noLeftovers() && readAll((fx.dir + "/switchwaker.nro").c_str()) == "old nro");

    // A bad checksum, a file outside switch/switchwaker/, no NRO, no zip for this region: nothing changes.
    struct Bad {
        std::vector<std::pair<std::string, std::string>> files;
        bool badSha;
        bool usaZip;
        PcUpdateError error;
    };
    const Bad bads[] = {
        {{{"switch/switchwaker/switchwaker.nro", "n"}}, true, true, PC_UPDATE_ERR_CHECKSUM},
        {{{"switch/switchwaker/switchwaker.nro", "n"}, {"switch/switchwaker/native/user/settings.ini", "x"}}, false, true,
         PC_UPDATE_ERR_BAD_ZIP},
        {{{"switch/switchwaker/initial_pipeline_cache.db", "d"}}, false, true, PC_UPDATE_ERR_BAD_ZIP},
        {{{"switch/switchwaker/switchwaker.nro", "n"}}, false, false, PC_UPDATE_ERR_NO_ASSET},
    };
    for (const Bad& b : bads) {
        fx.release(b.files, b.badSha, b.usaZip);
        fx.oldInstall();
        fx.start("0.5.0");
        pc_update_check(0, "");
        info = waitIdle();
        CHECK(info.status == PC_UPDATE_AVAILABLE && pc_update_install() == 1);
        info = waitIdle();
        CHECK(info.status == PC_UPDATE_INSTALL_FAILED && info.error == b.error);
        CHECK(fx.isOld() && fx.noLeftovers() && fx.journal().empty());
    }

    // A failed check: nothing to fetch; an answer that is not a release.
    setenv("COS_UPDATE_URL", ("file://" + fx.rel + "/missing.json").c_str(), 1);
    fx.start("0.5.0");
    pc_update_check(1, "");
    info = waitIdle();
    CHECK(info.status == PC_UPDATE_CHECK_FAILED && info.error == PC_UPDATE_ERR_SERVER);
    writeAll(fx.rel + "/limit.json", "{\"message\":\"API rate limit exceeded for 1.2.3.4.\"}");
    setenv("COS_UPDATE_URL", ("file://" + fx.rel + "/limit.json").c_str(), 1);
    pc_update_check(1, "");
    info = waitIdle();
    CHECK(info.status == PC_UPDATE_CHECK_FAILED && info.error == PC_UPDATE_ERR_SERVER);

    // Without an install directory the copy cannot update itself.
    unsetenv("COS_UPDATE_DIR");
    fx.goodRelease();
    setenv("COS_UPDATE_URL", ("file://" + fx.rel + "/latest.json").c_str(), 1);
    fx.start("0.5.0");
    pc_update_check(0, "");
    info = waitIdle();
    CHECK(info.status == PC_UPDATE_AVAILABLE && !info.canInstall && pc_update_install() == 0);

    std::string cmd = "rm -rf '" + fx.root + "'";
    CHECK(std::system(cmd.c_str()) == 0);
}

// --live: the real thing against GitHub (network; not part of the default run): check as version
// 0.4.2, download the latest USA zip, install it into a temporary directory, print what landed.
int liveTest() {
    char root[] = "/tmp/cos_update_live_XXXXXX";
    if (mkdtemp(root) == nullptr) {
        return 1;
    }
    const std::string dir = std::string(root) + "/switchwaker";
    mkdir(dir.c_str(), 0777);
    writeAll(dir + "/switchwaker.nro", "old nro");
    setenv("COS_UPDATE_DIR", dir.c_str(), 1);
    unsetenv("COS_UPDATE_URL");
    setenv("COS_UPDATE_VERSION", "0.4.2", 1);
    pc_update_boot();
    pc_update_check(1, "");
    PcUpdateInfo info = waitIdle(90);
    std::printf("check: status %d error %d latest %s \"%s\" %s\n", (int)info.status, (int)info.error, info.latest,
                info.title, info.detail);
    if (info.status != PC_UPDATE_AVAILABLE || pc_update_install() != 1) {
        return 1;
    }
    for (;;) {
        pc_update_get_info(&info);
        if (info.status != PC_UPDATE_DOWNLOADING && info.status != PC_UPDATE_INSTALLING) {
            break;
        }
        std::printf("  %d: %llu / %llu\n", (int)info.status, (unsigned long long)info.done, (unsigned long long)info.total);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    info = waitIdle(90);
    std::printf("install: status %d error %d %s\n", (int)info.status, (int)info.error, info.detail);
    std::string cmd = "ls -la '" + dir + "' '" + dir + "/native/update'; shasum -a 256 '" + dir + "'/*.nro";
    std::system(cmd.c_str());
    std::printf("left in %s\n", root);
    return info.status == PC_UPDATE_INSTALLED ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--live") == 0) {
        return liveTest();
    }
    testUpdateCore();
    testUpdateFlow();
    if (g_failures != 0) {
        std::printf("FAIL: %d checks\n", g_failures);
        return 1;
    }
    std::printf("ok\n");
    return 0;
}
