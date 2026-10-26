// The update check and the in-game update (native/include/pc/pc_update.h; issues #27, #28;
// docs/AUTO_UPDATE_PLAN.md). The pure parts (versions, SHA-256, JSON, zip, journal) are in
// pc_update_core.cpp; this file adds the files, the HTTP transport and the worker thread.
//
// One worker thread, started on the first check, runs one job at a time (a check or an install)
// and publishes its state under a mutex; the menu polls pc_update_get_info. Every entry point opens
// a host allocation scope: the strings here are shared between the game thread and the worker, and
// a block of the game's current JKRHeap freed on the worker would corrupt that heap (the debug
// server's log tap did, 2026-10-17).
//
// Transport: file:// (tests) always; http(s) through libcurl when the build has it (COS_HAVE_CURL:
// the Switch's devkitPro switch-curl, whose TLS is libnx's ssl service with the console's own CA
// store, or the desktop's libcurl). GitHub's asset URLs redirect to its download host, so redirects
// are followed (https only).
//
// cos_update_tests builds this file without the game (COS_UPDATE_HOST_TEST): no JKRHeap scopes.
#include "pc/pc_update.h"
#include "features/pc_update_core.h"

#if !defined(COS_UPDATE_HOST_TEST)
#include "JSystem/JKernel/JKRHeap.h" // JKRPcHostAllocScope (pc/game_hooks/jkr_heap.h)
#endif
#if defined(COS_HAVE_CURL)
#include <curl/curl.h>
#endif
#if defined(__SWITCH__)
#include "cos_switch.h"
#endif

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <mutex>
#include <string>
#include <strings.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <thread>
#include <unistd.h>
#include <vector>

#ifndef COS_GAME_ID_STR
#define COS_GAME_ID_STR "GZLE01"
#endif

namespace {

using namespace pc::update;

#if defined(COS_UPDATE_HOST_TEST)
struct HostScope {};
#else
using HostScope = JKRPcHostAllocScope;
#endif

constexpr const char* kLatestUrl = "https://api.github.com/repos/centollOS/SwitchWaker/releases/latest";
constexpr uint64_t kSpareBytes = 4ull << 20; // free space kept on top of the files
#if defined(__SWITCH__)
constexpr const char* kSwitchDir = "sdmc:/switch/switchwaker";
#endif

void say(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void say(const char* fmt, ...) {
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    fprintf(stderr, "[cos] update: %s\n", line);
}

// ---- versions and places ---------------------------------------------------------------------------

// The build's own version: the NRO's; the desktop has none (COS_UPDATE_VERSION stands in, tests).
std::string buildVersion() {
#if defined(__SWITCH__)
    return cos_switch_version();
#else
    const char* v = getenv("COS_UPDATE_VERSION");
    return v != nullptr && *v != '\0' ? v : "dev";
#endif
}

// The version the check compares with: COS_UPDATE_VERSION pretends an older build on the console.
std::string checkVersion() {
    const char* v = getenv("COS_UPDATE_VERSION");
    return v != nullptr && *v != '\0' ? v : buildVersion();
}

// Where the release's files live, "" when this copy cannot update itself (pc_update.h).
std::string installDir() {
#if defined(__SWITCH__)
    const char* nro = cos_switch_nro_path();
    const std::string want = std::string(kSwitchDir) + "/switchwaker.nro";
    if (nro == nullptr || strcasecmp(nro, want.c_str()) != 0) {
        return "";
    }
    return kSwitchDir;
#else
    const char* d = getenv("COS_UPDATE_DIR");
    return d != nullptr && *d != '\0' ? d : "";
#endif
}

struct Paths {
    std::string dir, work, journal, failed;
    explicit Paths(const std::string& d)
        : dir(d), work(d + "/native/update"), journal(work + "/state.txt"), failed(work + "/failed.txt") {}
};

// ---- files -------------------------------------------------------------------------------------------

bool exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

void makeDirs(const std::string& path) {
    for (size_t i = 1; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '/') {
            const std::string part = path.substr(0, i);
            if (!part.empty() && part.back() != ':') {
                mkdir(part.c_str(), 0777);
            }
        }
    }
}

bool readText(const std::string& path, std::string* out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    out->clear();
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0 && out->size() < (1u << 20)) {
        out->append(buf, n);
    }
    fclose(f);
    return true;
}

// Horizon's rename never replaces a file (EEXIST): the target goes first.
bool moveOver(const std::string& from, const std::string& to) {
    remove(to.c_str());
    if (rename(from.c_str(), to.c_str()) != 0) {
        say("cannot rename %s to %s: %s", from.c_str(), to.c_str(), strerror(errno));
        return false;
    }
    return true;
}

// Written to <path>.tmp first, so a kill leaves the old file or the .tmp (readJournal takes either).
bool writeText(const std::string& path, const std::string& text) {
    const std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (f == nullptr) {
        say("cannot write %s: %s", tmp.c_str(), strerror(errno));
        return false;
    }
    bool ok = fwrite(text.data(), 1, text.size(), f) == text.size() && fflush(f) == 0;
    ok = fsync(fileno(f)) == 0 && ok;
    fclose(f);
    return ok && moveOver(tmp, path);
}

uint64_t freeBytes(const std::string& dir) {
    struct statvfs st;
    if (statvfs(dir.c_str(), &st) != 0) {
        return UINT64_MAX; // unknown: let the writes fail instead
    }
    return (uint64_t)st.f_bavail * (uint64_t)st.f_frsize;
}

bool readJournal(const Paths& p, Journal* j) {
    std::string text;
    if (readText(p.journal, &text) && parseJournal(text, j)) {
        return true;
    }
    return readText(p.journal + ".tmp", &text) && parseJournal(text, j);
}

void dropJournal(const Paths& p) {
    remove(p.journal.c_str());
    remove((p.journal + ".tmp").c_str());
}

bool contains(const std::vector<std::string>& v, const std::string& s) {
    for (const std::string& x : v) {
        if (x == s) {
            return true;
        }
    }
    return false;
}

// Puts back what the journal's install replaced: every <name>.old, and the names that were not there
// before are removed. Works from any point of the swap.
bool rollBack(const Paths& p, const Journal& j) {
    bool ok = true;
    for (const std::string& name : j.names) {
        const std::string file = p.dir + "/" + name;
        if (exists(file + ".old")) {
            ok = moveOver(file + ".old", file) && ok;
        } else if (!contains(j.existed, name)) {
            remove(file.c_str());
        }
        remove((file + ".new").c_str());
    }
    return ok;
}

void dropOld(const Paths& p, const Journal& j) {
    for (const std::string& name : j.names) {
        const std::string file = p.dir + "/" + name;
        remove((file + ".old").c_str());
        remove((file + ".new").c_str());
    }
}

// Leftovers of an install that never wrote its journal (a kill while extracting) or of a confirmed one
// whose clean-up did not finish: <file>.new / <file>.old of the release's kinds of files.
void dropStrays(const Paths& p) {
    DIR* d = opendir(p.dir.c_str());
    if (d == nullptr) {
        return;
    }
    std::vector<std::string> doomed;
    while (dirent* e = readdir(d)) {
        const std::string n = e->d_name;
        for (const char* kind : {".nro", ".db", ".bin"}) {
            for (const char* suffix : {".new", ".old"}) {
                const std::string tail = std::string(kind) + suffix;
                if (n.size() > tail.size() && n.compare(n.size() - tail.size(), tail.size(), tail) == 0) {
                    doomed.push_back(p.dir + "/" + n);
                }
            }
        }
    }
    closedir(d);
    for (const std::string& f : doomed) {
        say("removing the leftover %s", f.c_str());
        remove(f.c_str());
    }
}

// The work directory's downloads (not the journal or failed.txt).
void dropDownloads(const Paths& p) {
    DIR* d = opendir(p.work.c_str());
    if (d == nullptr) {
        return;
    }
    std::vector<std::string> doomed;
    while (dirent* e = readdir(d)) {
        const std::string n = e->d_name;
        if (n != "." && n != ".." && n.rfind("state.txt", 0) != 0 && n.rfind("failed.txt", 0) != 0) {
            doomed.push_back(p.work + "/" + n);
        }
    }
    closedir(d);
    for (const std::string& f : doomed) {
        remove(f.c_str());
    }
}

// ---- transport ---------------------------------------------------------------------------------------

struct Fetch {
    std::string url;
    bool api = false;              // GitHub's API (its Accept header)
    std::string* body = nullptr;   // the answer into memory (small: at most 4 MiB)...
    FILE* file = nullptr;          // ...or into a file
    std::atomic<uint64_t>* done = nullptr;
    PcUpdateError error = PC_UPDATE_OK;
    std::string detail;
};

bool fetchFile(Fetch& f) {
    const std::string path = f.url.substr(strlen("file://"));
    FILE* in = fopen(path.c_str(), "rb");
    if (in == nullptr) {
        f.error = PC_UPDATE_ERR_SERVER;
        f.detail = "cannot open " + path;
        return false;
    }
    std::vector<char> buf(64 * 1024);
    size_t n;
    bool ok = true;
    while ((n = fread(buf.data(), 1, buf.size(), in)) > 0) {
        if (f.body != nullptr) {
            f.body->append(buf.data(), n);
        } else if (fwrite(buf.data(), 1, n, f.file) != n) {
            ok = false;
            f.error = PC_UPDATE_ERR_WRITE;
            f.detail = std::string("write failed: ") + strerror(errno);
            break;
        }
        if (f.done != nullptr) {
            f.done->fetch_add(n);
        }
    }
    fclose(in);
    return ok;
}

#if defined(COS_HAVE_CURL)
size_t curlWrite(char* data, size_t size, size_t count, void* user) {
    Fetch& f = *static_cast<Fetch*>(user);
    const size_t n = size * count;
    if (f.body != nullptr) {
        if (f.body->size() + n > (4u << 20)) {
            return 0;
        }
        f.body->append(data, n);
    } else if (fwrite(data, 1, n, f.file) != n) {
        f.error = PC_UPDATE_ERR_WRITE;
        f.detail = std::string("write failed: ") + strerror(errno);
        return 0;
    }
    if (f.done != nullptr) {
        f.done->fetch_add(n);
    }
    return n;
}

bool fetchCurl(Fetch& f) {
    static std::once_flag sInit;
    std::call_once(sInit, [] {
        const CURLcode rc = curl_global_init(CURL_GLOBAL_DEFAULT);
        say("libcurl %s (init %d)", curl_version(), (int)rc);
    });
#if defined(__SWITCH__)
    if (!cos_switch_network_up()) {
        f.error = PC_UPDATE_ERR_NETWORK;
        f.detail = "no network service";
        return false;
    }
#endif
    CURL* c = curl_easy_init();
    if (c == nullptr) {
        f.error = PC_UPDATE_ERR_NETWORK;
        f.detail = "curl_easy_init failed";
        return false;
    }
    char errbuf[CURL_ERROR_SIZE] = {};
    const std::string agent = "SwitchWaker/" + buildVersion();
    curl_slist* headers = nullptr;
    if (f.api) {
        headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
        headers = curl_slist_append(headers, "X-GitHub-Api-Version: 2022-11-28");
    }
    curl_easy_setopt(c, CURLOPT_URL, f.url.c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(c, CURLOPT_USERAGENT, agent.c_str());
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, 8L);
    curl_easy_setopt(c, CURLOPT_PROTOCOLS, (long)CURLPROTO_HTTPS);
    curl_easy_setopt(c, CURLOPT_REDIR_PROTOCOLS, (long)CURLPROTO_HTTPS);
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 20L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 1L); // a stalled transfer ends after 30 s
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &f);
    curl_easy_setopt(c, CURLOPT_ERRORBUFFER, errbuf);
    const CURLcode rc = curl_easy_perform(c);
    long http = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(c);
    curl_slist_free_all(headers);
    if (rc == CURLE_OK) {
        return true;
    }
    if (f.error == PC_UPDATE_OK) {
        f.error = rc == CURLE_HTTP_RETURNED_ERROR || rc == CURLE_WRITE_ERROR ? PC_UPDATE_ERR_SERVER : PC_UPDATE_ERR_NETWORK;
        f.detail = std::string(errbuf[0] != '\0' ? errbuf : curl_easy_strerror(rc)) +
                   (http != 0 ? " (HTTP " + std::to_string(http) + ")" : "");
    }
    return false;
}
#endif

bool fetch(Fetch& f) {
    if (f.url.rfind("file://", 0) == 0) {
        return fetchFile(f);
    }
#if defined(COS_HAVE_CURL)
    return fetchCurl(f);
#else
    f.error = PC_UPDATE_ERR_NETWORK;
    f.detail = "this build has no HTTPS client";
    return false;
#endif
}

// ---- state and the worker ------------------------------------------------------------------------------

enum class Job { None, Check, Install };

struct State {
    std::mutex mu;
    std::condition_variable cv;
    bool threadStarted = false;
    Job job = Job::None;
    bool atStart = false;
    std::string skipTag;
    // published (under mu)
    PcUpdateStatus status = PC_UPDATE_IDLE;
    PcUpdateError error = PC_UPDATE_OK;
    std::string current, latest, title, detail, notes;
    uint64_t total = 0;
    bool fromStart = false, skipped = false;
    Release release; // the newer release the last check found
    PcUpdateBootResult boot = PC_UPDATE_BOOT_NONE;
    std::string bootTag;
    std::atomic<uint64_t> done{0};
    // pc_update_boot -> pc_update_frame
    bool booted = false;
    bool confirmPending = false;
    Journal pendingJournal;
};

State& st() {
    static State* s = new State();
    return *s;
}

void publish(PcUpdateStatus status, PcUpdateError error = PC_UPDATE_OK, const std::string& detail = "") {
    State& s = st();
    std::lock_guard<std::mutex> lk(s.mu);
    s.status = status;
    s.error = error;
    s.detail = detail;
}

void runCheck(bool atStart, const std::string& skipTag) {
    State& s = st();
    const std::string current = checkVersion();
    {
        std::lock_guard<std::mutex> lk(s.mu);
        s.current = current;
        s.fromStart = atStart;
        s.skipped = false;
        s.latest.clear();
        s.title.clear();
        s.notes.clear();
        s.release = Release();
    }
    Version cur;
    if (!parseVersion(current.c_str(), &cur)) {
        say("version %s is a development build: no check", current.c_str());
        publish(PC_UPDATE_DISABLED);
        return;
    }
    publish(PC_UPDATE_CHECKING);
    const char* env = getenv("COS_UPDATE_URL");
    std::string body;
    Fetch f;
    f.url = env != nullptr && *env != '\0' ? env : kLatestUrl;
    f.api = true;
    f.body = &body;
    if (!fetch(f)) {
        say("check failed: %s (%s)", f.detail.c_str(), f.url.c_str());
        publish(PC_UPDATE_CHECK_FAILED, f.error, f.detail);
        return;
    }
    Release rel;
    std::string why;
    Version latest;
    if (!parseRelease(body, &rel, &why) || !parseVersion(rel.tag.c_str(), &latest)) {
        why = why.empty() ? "tag " + rel.tag + " is not a version" : why;
        say("check failed: the answer is not a release: %s", why.c_str());
        publish(PC_UPDATE_CHECK_FAILED, PC_UPDATE_ERR_SERVER, "not a release: " + why);
        return;
    }
    if (f.url.rfind("file://", 0) == 0) {
        // test fixtures (native/check/update): asset URLs without a scheme are files next to the answer
        const std::string base = f.url.substr(0, f.url.rfind('/') + 1);
        for (Asset& a : rel.assets) {
            if (a.url.find("://") == std::string::npos) {
                a.url = base + a.url;
            }
        }
    }
    const bool newer = isNewer(latest, cur);
    std::string failedTag;
    if (std::string dir = installDir(); !dir.empty()) {
        std::string text;
        if (readText(Paths(dir).failed, &text)) {
            failedTag = text.substr(0, text.find('\n'));
        }
    }
    const bool skipped = newer && atStart && (sameTag(rel.tag, skipTag) || sameTag(rel.tag, failedTag));
    say("latest release %s (\"%s\"), this build %s: %s%s", rel.tag.c_str(), rel.name.c_str(), current.c_str(),
        newer ? "newer" : "up to date",
        skipped ? (sameTag(rel.tag, failedTag) ? " (it failed to start here before: not offered at start)"
                                               : " (the player chose not to be told about it)")
                : "");
    std::lock_guard<std::mutex> lk(s.mu);
    s.latest = rel.tag;
    s.title = rel.name;
    s.notes = notesFromMarkdown(rel.body);
    s.skipped = skipped;
    s.status = newer ? PC_UPDATE_AVAILABLE : PC_UPDATE_UP_TO_DATE;
    s.error = PC_UPDATE_OK;
    s.detail.clear();
    if (newer) {
        s.release = std::move(rel);
    }
}

bool installFail(PcUpdateError error, const std::string& detail) {
    say("install failed: %s", detail.c_str());
    publish(PC_UPDATE_INSTALL_FAILED, error, detail);
    return false;
}

bool runInstall() {
    State& s = st();
    Release rel;
    {
        std::lock_guard<std::mutex> lk(s.mu);
        rel = s.release;
    }
    const std::string dir = installDir();
    if (dir.empty()) {
        return installFail(PC_UPDATE_ERR_NOT_HERE, "this copy does not run from the place the update installs to");
    }
    const Paths p(dir);
    const std::string zipName = zipNameFor(rel.tag, COS_GAME_ID_STR);
    const Asset* zip = findAsset(rel, zipName);
    const Asset* sha = findAsset(rel, zipName + ".sha256");
    if (zipName.empty() || zip == nullptr || sha == nullptr) {
        return installFail(PC_UPDATE_ERR_NO_ASSET, "release " + rel.tag + " has no " +
                                                       (zipName.empty() ? std::string("zip for ") + COS_GAME_ID_STR : zipName) +
                                                       " with its .sha256");
    }
    // The zip, its files unpacked (about 4x) and some room: checked again exactly once it is here.
    if (freeBytes(dir) < zip->size * 5 + kSpareBytes) {
        return installFail(PC_UPDATE_ERR_NO_SPACE, "not enough free space for " + zipName);
    }
    say("installing %s: %s (%llu bytes) from %s", rel.tag.c_str(), zipName.c_str(), (unsigned long long)zip->size,
        zip->url.c_str());
    makeDirs(p.work);
    dropDownloads(p);
    dropStrays(p);
    s.done = 0;
    {
        std::lock_guard<std::mutex> lk(s.mu);
        s.total = zip->size;
        s.status = PC_UPDATE_DOWNLOADING;
    }
    std::string shaText;
    Fetch fs;
    fs.url = sha->url;
    fs.body = &shaText;
    if (!fetch(fs)) {
        return installFail(fs.error, "downloading " + sha->name + ": " + fs.detail);
    }
    const std::string want = parseSha256Line(shaText);
    if (want.empty()) {
        return installFail(PC_UPDATE_ERR_SERVER, sha->name + " holds no SHA-256");
    }
    const std::string zipPath = p.work + "/" + zipName;
    FILE* out = fopen(zipPath.c_str(), "wb");
    if (out == nullptr) {
        return installFail(PC_UPDATE_ERR_WRITE, "cannot write " + zipPath + ": " + strerror(errno));
    }
    Fetch fz;
    fz.url = zip->url;
    fz.file = out;
    fz.done = &s.done;
    bool ok = fetch(fz);
    ok = fflush(out) == 0 && ok;
    fclose(out);
    if (!ok) {
        remove(zipPath.c_str());
        return installFail(fz.error == PC_UPDATE_OK ? PC_UPDATE_ERR_WRITE : fz.error, "downloading " + zipName + ": " + fz.detail);
    }
    publish(PC_UPDATE_INSTALLING);
    const std::string got = sha256File(zipPath.c_str());
    if (got != want) {
        remove(zipPath.c_str());
        return installFail(PC_UPDATE_ERR_CHECKSUM, zipName + ": SHA-256 " + got + ", the release says " + want);
    }
    FILE* zf = fopen(zipPath.c_str(), "rb");
    if (zf == nullptr) {
        return installFail(PC_UPDATE_ERR_WRITE, "cannot read " + zipPath);
    }
    std::vector<ZipEntry> entries;
    std::string why;
    if (!zipList(zf, &entries, &why)) {
        fclose(zf);
        remove(zipPath.c_str());
        return installFail(PC_UPDATE_ERR_BAD_ZIP, zipName + ": " + why);
    }
    Journal j;
    j.state = "installing";
    j.tag = rel.tag;
    j.from = buildVersion();
    std::vector<const ZipEntry*> files;
    uint64_t unpacked = 0;
    for (const ZipEntry& e : entries) {
        std::string name;
        if (!e.name.empty() && e.name.back() == '/') {
            continue; // switch/, switch/switchwaker/
        }
        if (!installName(e.name, &name)) {
            fclose(zf);
            remove(zipPath.c_str());
            return installFail(PC_UPDATE_ERR_BAD_ZIP, zipName + " holds " + e.name + ", not a file of switch/switchwaker/");
        }
        j.names.push_back(name);
        files.push_back(&e);
        unpacked += e.size;
    }
    if (!contains(j.names, "switchwaker.nro")) {
        fclose(zf);
        remove(zipPath.c_str());
        return installFail(PC_UPDATE_ERR_BAD_ZIP, zipName + " holds no switch/switchwaker/switchwaker.nro");
    }
    if (freeBytes(dir) < unpacked + kSpareBytes) {
        fclose(zf);
        remove(zipPath.c_str());
        return installFail(PC_UPDATE_ERR_NO_SPACE, "not enough free space to unpack " + zipName);
    }
    for (size_t k = 0; k < files.size(); k++) {
        const std::string target = p.dir + "/" + j.names[k] + ".new";
        if (!zipExtract(zf, *files[k], target.c_str(), &why)) {
            fclose(zf);
            for (const std::string& n : j.names) {
                remove((p.dir + "/" + n + ".new").c_str());
            }
            remove(zipPath.c_str());
            return installFail(why.rfind("cannot write", 0) == 0 ? PC_UPDATE_ERR_WRITE : PC_UPDATE_ERR_BAD_ZIP, why);
        }
    }
    fclose(zf);
    for (const std::string& n : j.names) {
        if (exists(p.dir + "/" + n)) {
            j.existed.push_back(n);
        }
        remove((p.dir + "/" + n + ".old").c_str());
    }
    // The swap, one file at a time behind the journal: a kill at any point is undone at the next start.
    if (!writeText(p.journal, formatJournal(j))) {
        for (const std::string& n : j.names) {
            remove((p.dir + "/" + n + ".new").c_str());
        }
        remove(zipPath.c_str());
        return installFail(PC_UPDATE_ERR_WRITE, "cannot write the journal " + p.journal);
    }
    bool swapped = true;
    for (const std::string& n : j.names) {
        const std::string file = p.dir + "/" + n;
        if ((contains(j.existed, n) && !moveOver(file, file + ".old")) || !moveOver(file + ".new", file)) {
            swapped = false;
            break;
        }
    }
    if (swapped) {
        j.state = "installed";
        swapped = writeText(p.journal, formatJournal(j));
    }
    remove(zipPath.c_str());
    if (!swapped) {
        rollBack(p, j);
        dropJournal(p);
        return installFail(PC_UPDATE_ERR_WRITE, "the SD card refused a rename; the old files are back");
    }
    remove(p.failed.c_str());
    say("installed %s over %s (%zu files); it runs at the next start", rel.tag.c_str(), j.from.c_str(), j.names.size());
    publish(PC_UPDATE_INSTALLED);
    return true;
}

void workerMain() {
    HostScope hostAlloc;
    State& s = st();
    for (;;) {
        Job job;
        bool atStart;
        std::string skipTag;
        {
            std::unique_lock<std::mutex> lk(s.mu);
            s.cv.wait(lk, [&] { return s.job != Job::None; });
            job = s.job;
            atStart = s.atStart;
            skipTag = s.skipTag;
        }
        if (job == Job::Check) {
            runCheck(atStart, skipTag);
        } else {
            runInstall();
        }
        std::lock_guard<std::mutex> lk(s.mu);
        s.job = Job::None;
    }
}

// Starts the worker if needed and hands it a job; false while another one runs. Under s.mu.
bool post(State& s, Job job) {
    if (s.job != Job::None) {
        return false;
    }
    if (!s.threadStarted) {
        s.threadStarted = true;
        std::thread(workerMain).detach();
    }
    s.job = job;
    // busy from now on, so a caller polling right after sees the job, not the state before it
    s.status = job == Job::Check ? PC_UPDATE_CHECKING : PC_UPDATE_DOWNLOADING;
    s.error = PC_UPDATE_OK;
    s.detail.clear();
    if (job == Job::Install) {
        s.done = 0;
        s.total = 0;
    }
    s.cv.notify_one();
    return true;
}

void copyOut(char* dst, size_t size, const std::string& src) {
    snprintf(dst, size, "%s", src.c_str());
}

// The installed version did not get past the logo: the old files are back; start them.
void restartAfterFailedStart(const std::string& tag) {
#if defined(__SWITCH__)
    if (cos_switch_from_forwarder()) {
        cos_switch_restart_program("update: the new version did not start, back to the previous one");
    }
    const std::string text = "La actualización a " + tag +
                             " no llegó a arrancar y se ha restaurado la versión anterior. Vuelve a abrir el juego.\n\n"
                             "The update to " + tag +
                             " did not start, so the previous version is back. Open the game again.";
    cos_switch_show_error(text.c_str());
    cos_switch_exit(0);
#else
    say("desktop: not restarting (the previous files of %s are back)", tag.c_str());
#endif
}

} // namespace

extern "C" {

void pc_update_boot(void) {
    HostScope hostAlloc;
    State& s = st();
    if (s.booted) {
        return;
    }
    s.booted = true;
    const std::string dir = installDir();
    if (dir.empty()) {
        return;
    }
    const Paths p(dir);
    std::string failedText;
    if (readText(p.failed, &failedText) && failedText.find("\nnotice=1") != std::string::npos) {
        // the restored version's first start after a failed one: say it once
        s.boot = PC_UPDATE_BOOT_FAILED_BACK;
        s.bootTag = failedText.substr(0, failedText.find('\n'));
        writeText(p.failed, s.bootTag + "\n");
    }
    Journal j;
    if (!readJournal(p, &j)) {
        dropStrays(p);
        dropDownloads(p);
        return;
    }
    const std::string own = buildVersion();
    say("journal: %s %s (from %s); this build %s", j.state.c_str(), j.tag.c_str(), j.from.c_str(), own.c_str());
    if (j.state == "installing") {
        rollBack(p, j);
        dropJournal(p);
        s.boot = PC_UPDATE_BOOT_ROLLED_BACK;
        s.bootTag = j.tag;
        say("the install of %s was interrupted: the previous files are back", j.tag.c_str());
        if (sameTag(j.tag, own)) {
            // this process is the half-installed version itself: start the restored one
            restartAfterFailedStart(j.tag);
        }
        return;
    }
    if (!sameTag(j.tag, own)) {
        // files copied by hand since: keep them, forget the install
        say("this build is not %s: dropping the journal and the .old files", j.tag.c_str());
        dropOld(p, j);
        dropJournal(p);
        return;
    }
    if (j.state == "installed") {
        j.state = "booting";
        writeText(p.journal, formatJournal(j));
        s.confirmPending = true;
        s.pendingJournal = j;
        say("first start of %s: confirmed once a scene after the logo runs", j.tag.c_str());
        if (const char* fail = getenv("COS_UPDATE_FAIL_FIRST_START"); fail != nullptr && strcmp(fail, "1") == 0) {
            // test of the rollback (docs/AUTO_UPDATE_PLAN.md): a new version that dies before the logo
            say("COS_UPDATE_FAIL_FIRST_START=1: ending this first start before the logo");
            fflush(stderr);
            abort();
        }
        return;
    }
    // "booting": the last start of this version never got past the logo
    say("%s did not get past the logo at its first start: restoring %s", j.tag.c_str(), j.from.c_str());
    rollBack(p, j);
    dropJournal(p);
    writeText(p.failed, j.tag + "\nnotice=1\n");
    s.boot = PC_UPDATE_BOOT_FAILED_BACK;
    s.bootTag = j.tag;
    restartAfterFailedStart(j.tag);
}

void pc_update_frame(int afterLogo) {
    State& s = st();
    if (!s.confirmPending || !afterLogo) {
        return;
    }
    HostScope hostAlloc;
    s.confirmPending = false;
    const Paths p(installDir());
    dropOld(p, s.pendingJournal);
    dropJournal(p);
    std::lock_guard<std::mutex> lk(s.mu);
    s.boot = PC_UPDATE_BOOT_FIRST_START;
    s.bootTag = s.pendingJournal.tag;
    say("%s confirmed: the previous version's files are gone", s.bootTag.c_str());
}

void pc_update_check(int atStart, const char* skipTag) {
    HostScope hostAlloc;
    State& s = st();
    std::lock_guard<std::mutex> lk(s.mu);
    if (s.status == PC_UPDATE_INSTALLED) {
        return; // the new version is in place: nothing to check until the restart
    }
    s.atStart = atStart != 0;
    s.skipTag = skipTag != nullptr ? skipTag : "";
    post(s, Job::Check);
}

int pc_update_install(void) {
    HostScope hostAlloc;
    State& s = st();
    std::lock_guard<std::mutex> lk(s.mu);
    if ((s.status != PC_UPDATE_AVAILABLE && s.status != PC_UPDATE_INSTALL_FAILED) || s.release.tag.empty() ||
        installDir().empty()) {
        return 0;
    }
    return post(s, Job::Install) ? 1 : 0;
}

void pc_update_get_info(PcUpdateInfo* out) {
    HostScope hostAlloc;
    State& s = st();
    std::lock_guard<std::mutex> lk(s.mu);
    memset(out, 0, sizeof(*out));
    out->status = s.status;
    out->error = s.error;
    copyOut(out->current, sizeof(out->current), s.current.empty() ? checkVersion() : s.current);
    copyOut(out->latest, sizeof(out->latest), s.latest);
    copyOut(out->title, sizeof(out->title), s.title);
    copyOut(out->detail, sizeof(out->detail), s.detail);
    out->done = s.done.load();
    out->total = s.total;
    out->canInstall = !installDir().empty();
    out->fromStart = s.fromStart;
    out->skipped = s.skipped;
    out->boot = s.boot;
    copyOut(out->bootTag, sizeof(out->bootTag), s.bootTag);
}

size_t pc_update_get_notes(char* out, size_t size) {
    HostScope hostAlloc;
    State& s = st();
    std::lock_guard<std::mutex> lk(s.mu);
    if (out != nullptr && size > 0) {
        snprintf(out, size, "%s", s.notes.c_str());
    }
    return s.notes.size();
}

int pc_update_can_restart(void) {
#if defined(__SWITCH__)
    return cos_switch_from_forwarder();
#else
    return 0;
#endif
}

int pc_update_restart(void) {
#if defined(__SWITCH__)
    if (cos_switch_from_forwarder()) {
        cos_switch_restart_program("update: starting the installed version");
    }
#endif
    return 0;
}

#if defined(COS_UPDATE_HOST_TEST)
// cos_update_tests: a new process as far as this file knows (the worker thread stays).
void pc_update_test_reset(void) {
    State& s = st();
    for (;;) {
        {
            std::lock_guard<std::mutex> lk(s.mu);
            if (s.job == Job::None) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::lock_guard<std::mutex> lk(s.mu);
    s.status = PC_UPDATE_IDLE;
    s.error = PC_UPDATE_OK;
    s.current.clear();
    s.latest.clear();
    s.title.clear();
    s.detail.clear();
    s.notes.clear();
    s.release = Release();
    s.fromStart = s.skipped = false;
    s.boot = PC_UPDATE_BOOT_NONE;
    s.bootTag.clear();
    s.booted = false;
    s.confirmPending = false;
    s.pendingJournal = Journal();
}
#endif

} // extern "C"
