// The pure part of the update check and the in-game update (issues #27, #28; docs/AUTO_UPDATE_PLAN.md):
// versions, SHA-256, the GitHub release JSON, the release zip reader and the install journal. No
// network, no threads, no game state: cos_pc_tests checks it on the host. pc_update.cpp builds the
// check, the download, the install and the rollback on it.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace pc::update {

// A version as the NRO and the release tags write it: "0.5.0", "v0.5.0", "0.5.0-3", "0.5.0-3-gabcdef",
// "0.5.0-dirty". `after` is the number of commits past the tag (git describe's -N; 0 on the tag).
struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
    int after = 0;
};

// False when the text does not start with [v]X.Y.Z ("dev", a bare hash): such a build never checks.
bool parseVersion(const char* text, Version* out);
// The release is newer than the running build: a higher X.Y.Z (a build past a tag is not older
// than that tag's release).
bool isNewer(const Version& release, const Version& running);
// "v0.5.0" and "0.5.0" name the same version.
bool sameTag(const std::string& a, const std::string& b);

struct Sha256 {
    Sha256();
    void update(const void* data, size_t size);
    void final(uint8_t out[32]);

private:
    void block(const uint8_t* p);
    uint32_t h_[8];
    uint8_t buf_[64];
    size_t used_ = 0;
    uint64_t total_ = 0;
};
std::string toHex(const uint8_t* data, size_t size);
// The SHA-256 of a file as 64 lowercase hex digits; "" when it cannot be read.
std::string sha256File(const char* path);
// The digest of a `shasum -a 256` line ("<64 hex>  <name>"), lowercase; "" when there is none.
std::string parseSha256Line(const std::string& text);

struct Asset {
    std::string name;
    std::string url; // browser_download_url
    uint64_t size = 0;
};

struct Release {
    std::string tag;     // tag_name, e.g. "v0.5.1"
    std::string name;    // the release title
    std::string htmlUrl; // the release page
    std::string body;    // the release notes (GitHub Markdown)
    std::vector<Asset> assets;
};

// Reads the fields above from GitHub's GET /repos/{owner}/{repo}/releases/latest answer.
bool parseRelease(const std::string& json, Release* out, std::string* error);
const Asset* findAsset(const Release& release, const std::string& name);
// The release notes as plain lines for the menu, whose font has only Latin-1: "# " starts a heading,
// "- " (after two spaces per level) an item; **bold**, `code` and links lose their marks ([text](url) ->
// text), a table row becomes its cells joined by " · ", typographic quotes, dashes, ellipses and arrows
// become Latin-1 or ASCII and other characters past U+00FF go. The "Install..." section is left out
// (the game installs the update itself); runs of blank lines become one, and none touches a heading.
std::string notesFromMarkdown(const std::string& markdown);
// The release zip of a region: "SwitchWaker-<tag>-switch.zip" (GZLE01) or "-switch-eur.zip" (GZLP01);
// "" for another game ID.
std::string zipNameFor(const std::string& tag, const char* gameId);

struct ZipEntry {
    std::string name;
    uint16_t method = 0; // 0 stored, 8 deflate
    uint32_t crc = 0;
    uint64_t packedSize = 0;
    uint64_t size = 0;
    uint64_t localOffset = 0;
};

// The central directory of a zip file (no zip64, no encryption, which `zip -qr` never writes for
// these sizes).
bool zipList(FILE* file, std::vector<ZipEntry>* out, std::string* error);
// Writes one entry to outPath (replacing it) and checks its size and CRC-32.
bool zipExtract(FILE* file, const ZipEntry& entry, const char* outPath, std::string* error);
// The file a release zip entry installs: "switch/switchwaker/<name>" with a plain name (no further
// '/', not "." or "..", no '\\' or ':'); false for directories and anything else.
bool installName(const std::string& entry, std::string* name);

// The install journal (<dir>/native/update/state.txt): what the swap was doing when the game ended.
struct Journal {
    std::string state; // "installing", "installed" or "booting"
    std::string tag;   // the version being installed, e.g. "v0.5.1"
    std::string from;  // the version it replaces (for the log)
    std::vector<std::string> names;   // the files the update installs
    std::vector<std::string> existed; // the ones that were there before (they have a .old)
};
std::string formatJournal(const Journal& journal);
bool parseJournal(const std::string& text, Journal* out);

} // namespace pc::update
