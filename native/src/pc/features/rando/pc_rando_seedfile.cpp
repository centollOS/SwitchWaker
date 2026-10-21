// The randomizer's placement file (pc_rando_seedfile.h, docs/RANDOMIZER_PLAN.md step R0.6). Pure C++:
// no game headers or calls (cos_rando_core links it into the unit tests without game code).
#include "pc_rando_seedfile.h"

#include "pc_rando_data.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// ---- wwrando's item table --------------------------------------------------------------------------

struct ItemName {
    uint8_t id;
    const char* name;
    uint8_t renamed; // registered by tweaks.py (register_renamed_item); the id's display name
};

const ItemName kItemNames[] = {
#include "pc_rando_seedfile_items.inc"
};

// ---- MD5 (RFC 1321), file-local: the content hash ----------------------------------------------------

struct Md5 {
    uint32_t a = 0x67452301, b = 0xefcdab89, c = 0x98badcfe, d = 0x10325476;
    uint64_t len = 0;
    uint8_t buf[64];
    size_t fill = 0;

    static uint32_t rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

    void block(const uint8_t* p) {
        static const uint32_t K[64] = {
            0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
            0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
            0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
            0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
            0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
            0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
            0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
            0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
        static const int R[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                                  5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                                  4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                                  6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
        uint32_t m[16];
        for (int i = 0; i < 16; i++) {
            m[i] = (uint32_t)p[i * 4] | ((uint32_t)p[i * 4 + 1] << 8) | ((uint32_t)p[i * 4 + 2] << 16) |
                   ((uint32_t)p[i * 4 + 3] << 24);
        }
        uint32_t A = a, B = b, C = c, D = d;
        for (int i = 0; i < 64; i++) {
            uint32_t f;
            int g;
            if (i < 16) {
                f = (B & C) | (~B & D);
                g = i;
            } else if (i < 32) {
                f = (D & B) | (~D & C);
                g = (5 * i + 1) & 15;
            } else if (i < 48) {
                f = B ^ C ^ D;
                g = (3 * i + 5) & 15;
            } else {
                f = C ^ (B | ~D);
                g = (7 * i) & 15;
            }
            const uint32_t t = D;
            D = C;
            C = B;
            B = B + rotl(A + f + K[i] + m[g], R[i]);
            A = t;
        }
        a += A;
        b += B;
        c += C;
        d += D;
    }

    void update(const uint8_t* p, size_t n) {
        len += n;
        while (n > 0) {
            const size_t take = (64 - fill) < n ? (64 - fill) : n;
            memcpy(buf + fill, p, take);
            fill += take;
            p += take;
            n -= take;
            if (fill == 64) {
                block(buf);
                fill = 0;
            }
        }
    }

    void finish(uint8_t out[16]) {
        const uint64_t bits = len * 8;
        static const uint8_t kPad[64] = {0x80};
        update(kPad, fill < 56 ? 56 - fill : 120 - fill);
        uint8_t l[8];
        for (int i = 0; i < 8; i++) {
            l[i] = (uint8_t)(bits >> (8 * i));
        }
        update(l, 8);
        const uint32_t w[4] = {a, b, c, d};
        for (int i = 0; i < 16; i++) {
            out[i] = (uint8_t)(w[i / 4] >> (8 * (i % 4)));
        }
    }
};

void md5(const void* data, size_t len, uint8_t out[16]) {
    Md5 m;
    m.update(static_cast<const uint8_t*>(data), len);
    m.finish(out);
}

// ---- parse helpers ---------------------------------------------------------------------------------

bool failWith(std::string* err, std::string msg) {
    if (err != nullptr) {
        *err = std::move(msg);
    }
    return false;
}

const char* kKnownKeys[] = {"format",   "seed",       "permalink",          "options",
                            "extra_starting_items",   "charts",             "required_boss_locations",
                            "entrances", "starting_island_room",            "locations",
                            "hints",    "version"};

bool isKnownKey(const std::string& k) {
    for (const char* known : kKnownKeys) {
        if (k == known) {
            return true;
        }
    }
    return false;
}

// "1".."49" exactly as Python's str(int) writes them (so the file round-trips).
bool parseIslandKey(const std::string& k, int* out) {
    if (k.empty() || k.size() > 2 || k[0] == '0') {
        return false;
    }
    int v = 0;
    for (char c : k) {
        if (c < '0' || c > '9') {
            return false;
        }
        v = v * 10 + (c - '0');
    }
    if (v < 1 || v > 49) {
        return false;
    }
    *out = v;
    return true;
}

bool needString(const pcr::Json& root, const char* key, std::string* out, std::string* err) {
    const pcr::Json* v = root.find(key);
    if (v == nullptr || !v->isString()) {
        return failWith(err, std::string("\"") + key + "\" must be a string");
    }
    *out = v->text();
    return true;
}

bool itemId(const std::string& name, const char* where, uint8_t* out, std::string* err) {
    const int id = pc_rando_item_id(name);
    if (id < 0) {
        return failWith(err, std::string(where) + ": unknown item \"" + name + "\"");
    }
    *out = (uint8_t)id;
    return true;
}

// R6.5: a trap chest ("Ice Trap Chest", items.py change_chest_item) keeps the chest's own item id
// (wwrando sets only the trap flag, never item_id), so the location's item id is its original item;
// the trap itself is read from the item name (pc_rando_trap.h).
bool locationItemId(const std::string& name, const std::string& location, uint8_t* out, std::string* err) {
    static const char kTrap[] = " Trap Chest";
    const size_t n = sizeof kTrap - 1;
    if (name.size() > n && name.compare(name.size() - n, n, kTrap) == 0) {
        const int loc = pcr::location_index(location.c_str());
        if (loc < 0) {
            return failWith(err, location + ": a trap chest at an unknown location");
        }
        return itemId(pcr::locations()[(size_t)loc].original_item, location.c_str(), out, err);
    }
    return itemId(name, location.c_str(), out, err);
}

bool parseInto(std::string_view text, PcRandoSeed* s, std::string* err) {
    pcr::Json root;
    std::string jerr;
    if (!pcr::jsonParse(text, &root, &jerr)) {
        return failWith(err, "bad JSON: " + jerr);
    }
    if (!root.isObject()) {
        return failWith(err, "the placement is not a JSON object");
    }

    const pcr::Json* format = root.find("format");
    if (format == nullptr || !format->isInteger()) {
        return failWith(err, "\"format\" must be an integer");
    }
    if (format->asInt() != PC_RANDO_SEED_FORMAT) {
        return failWith(err, "unsupported placement format " + format->text());
    }
    s->format = (int)format->asInt();

    if (!needString(root, "seed", &s->seed, err) || !needString(root, "permalink", &s->permalink, err)) {
        return false;
    }

    const pcr::Json* options = root.find("options");
    if (options == nullptr || !options->isObject()) {
        return failWith(err, "\"options\" must be an object");
    }
    s->options = *options;

    const pcr::Json* extra = root.find("extra_starting_items");
    if (extra == nullptr || !extra->isArray()) {
        return failWith(err, "\"extra_starting_items\" must be an array");
    }
    for (size_t i = 0; i < extra->size(); i++) {
        const pcr::Json& e = extra->at(i);
        uint8_t id;
        if (!e.isString()) {
            return failWith(err, "\"extra_starting_items\" holds a non-string");
        }
        if (!itemId(e.text(), "extra_starting_items", &id, err)) {
            return false;
        }
        s->extra_starting_items.push_back(e.text());
        s->extra_starting_item_ids.push_back(id);
    }

    const pcr::Json* charts = root.find("charts");
    if (charts == nullptr || !charts->isObject()) {
        return failWith(err, "\"charts\" must be an object");
    }
    for (size_t i = 0; i < charts->size(); i++) {
        int island;
        if (!parseIslandKey(charts->keyAt(i), &island)) {
            return failWith(err, "\"charts\": bad island number \"" + charts->keyAt(i) + "\"");
        }
        const pcr::Json& v = charts->at(i);
        uint8_t id;
        if (!v.isString()) {
            return failWith(err, "\"charts\" holds a non-string");
        }
        if (!itemId(v.text(), "charts", &id, err)) {
            return false;
        }
        s->charts.emplace_back(island, v.text());
    }

    const pcr::Json* bosses = root.find("required_boss_locations");
    if (bosses == nullptr || !bosses->isArray()) {
        return failWith(err, "\"required_boss_locations\" must be an array");
    }
    for (size_t i = 0; i < bosses->size(); i++) {
        if (!bosses->at(i).isString()) {
            return failWith(err, "\"required_boss_locations\" holds a non-string");
        }
        s->required_boss_locations.push_back(bosses->at(i).text());
    }

    const pcr::Json* entrances = root.find("entrances");
    if (entrances == nullptr || !entrances->isObject()) {
        return failWith(err, "\"entrances\" must be an object");
    }
    for (size_t i = 0; i < entrances->size(); i++) {
        if (!entrances->at(i).isString()) {
            return failWith(err, "\"entrances\" holds a non-string");
        }
        s->entrances.emplace_back(entrances->keyAt(i), entrances->at(i).text());
    }

    const pcr::Json* room = root.find("starting_island_room");
    if (room == nullptr || !room->isInteger() || room->asInt() < 1 || room->asInt() > 49) {
        return failWith(err, "\"starting_island_room\" must be an integer 1-49");
    }
    s->starting_island_room = (int)room->asInt();

    const pcr::Json* locations = root.find("locations");
    if (locations == nullptr || !locations->isObject()) {
        return failWith(err, "\"locations\" must be an object");
    }
    if (locations->size() != PC_RANDO_SEED_LOCATIONS) {
        char b[96];
        snprintf(b, sizeof b, "\"locations\" holds %zu locations, not %d", locations->size(),
                 PC_RANDO_SEED_LOCATIONS);
        return failWith(err, b);
    }
    for (size_t i = 0; i < locations->size(); i++) {
        const pcr::Json& v = locations->at(i);
        if (!v.isString()) {
            return failWith(err, "\"locations\": \"" + locations->keyAt(i) + "\" is not an item name");
        }
        if (!locationItemId(v.text(), locations->keyAt(i), &s->locations[i], err)) {
            return false;
        }
        s->location_names.push_back(locations->keyAt(i));
        s->location_item_names.push_back(v.text());
    }

    if (const pcr::Json* hints = root.find("hints")) {
        s->has_hints = true;
        s->hints = *hints;
    }
    if (const pcr::Json* version = root.find("version")) {
        if (!version->isString()) {
            return failWith(err, "\"version\" must be a string");
        }
        s->has_version = true;
        s->version = version->text();
    }

    for (size_t i = 0; i < root.size(); i++) {
        if (!isKnownKey(root.keyAt(i))) {
            s->extra.set(root.keyAt(i), root.at(i));
        }
    }
    return true;
}

} // namespace

PcRandoSeed::PcRandoSeed() {
    memset(locations, PC_RANDO_NO_ITEM, sizeof locations);
    memset(hash, 0, sizeof hash);
}

bool pc_rando_seed_parse(std::string_view text, PcRandoSeed* out, std::string* err) {
    PcRandoSeed s;
    if (!parseInto(text, &s, err)) {
        *out = PcRandoSeed();
        return false;
    }
    pc_rando_seed_update_hash(&s);
    *out = std::move(s);
    return true;
}

std::string pc_rando_seed_serialize(const PcRandoSeed& s) {
    using pcr::Json;
    Json root = Json::object();
    root.set("format", Json::integer(s.format));
    root.set("seed", Json::string(s.seed));
    root.set("permalink", Json::string(s.permalink));
    root.set("options", s.options.isObject() ? s.options : Json::object());

    Json extra = Json::array();
    for (const std::string& n : s.extra_starting_items) {
        extra.push(Json::string(n));
    }
    root.set("extra_starting_items", std::move(extra));

    Json charts = Json::object();
    for (const auto& c : s.charts) {
        char k[16];
        snprintf(k, sizeof k, "%d", c.first);
        charts.set(k, Json::string(c.second));
    }
    root.set("charts", std::move(charts));

    Json bosses = Json::array();
    for (const std::string& n : s.required_boss_locations) {
        bosses.push(Json::string(n));
    }
    root.set("required_boss_locations", std::move(bosses));

    Json entrances = Json::object();
    for (const auto& e : s.entrances) {
        entrances.set(e.first, Json::string(e.second));
    }
    root.set("entrances", std::move(entrances));
    root.set("starting_island_room", Json::integer(s.starting_island_room));

    Json locations = Json::object();
    const size_t n = s.location_names.size();
    for (size_t i = 0; i < n; i++) {
        // The item name as read; for a seed built in code without names, the id's display name.
        std::string item = i < s.location_item_names.size() ? s.location_item_names[i] : std::string();
        if (item.empty() && i < PC_RANDO_SEED_LOCATIONS) {
            const char* dn = pc_rando_item_name(s.locations[i]);
            item = dn != nullptr ? dn : "";
        }
        locations.set(s.location_names[i], Json::string(item));
    }
    root.set("locations", std::move(locations));

    if (s.has_hints) {
        root.set("hints", s.hints);
    }
    if (s.has_version) {
        root.set("version", Json::string(s.version));
    }
    for (size_t i = 0; i < s.extra.size(); i++) {
        if (!isKnownKey(s.extra.keyAt(i))) {
            root.set(s.extra.keyAt(i), s.extra.at(i));
        }
    }

    std::string out;
    pcr::jsonWrite(root, &out, 1);
    out += '\n';
    return out;
}

void pc_rando_seed_update_hash(PcRandoSeed* s) {
    const std::string text = pc_rando_seed_serialize(*s);
    md5(text.data(), text.size(), s->hash);
}

uint32_t pc_rando_seed_hash32(const PcRandoSeed& s) {
    return ((uint32_t)s.hash[0] << 24) | ((uint32_t)s.hash[1] << 16) | ((uint32_t)s.hash[2] << 8) | s.hash[3];
}

std::string pc_rando_seed_hash_hex(const PcRandoSeed& s) {
    char b[33];
    for (int i = 0; i < 16; i++) {
        snprintf(b + i * 2, 3, "%02x", s.hash[i]);
    }
    return std::string(b, 32);
}

std::string pc_rando_seed_sidecar_name(const PcRandoSeed& s) {
    char b[48];
    snprintf(b, sizeof b, "switchwaker-rando-%08x.json", (unsigned)pc_rando_seed_hash32(s));
    return b;
}

bool pc_rando_seed_load_file(const char* path, PcRandoSeed* out, std::string* err) {
    FILE* f = fopen(path, "rb");
    if (f == nullptr) {
        *out = PcRandoSeed();
        return failWith(err, std::string("cannot open ") + path);
    }
    std::string text;
    char buf[16384];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        text.append(buf, n);
        if (text.size() > (8u << 20)) { // a placement is ~30 KB
            fclose(f);
            *out = PcRandoSeed();
            return failWith(err, std::string(path) + " is too large for a placement");
        }
    }
    const bool readErr = ferror(f) != 0;
    fclose(f);
    if (readErr) {
        *out = PcRandoSeed();
        return failWith(err, std::string("cannot read ") + path);
    }
    std::string perr;
    if (!pc_rando_seed_parse(text, out, &perr)) {
        return failWith(err, std::string(path) + ": " + perr);
    }
    return true;
}

bool pc_rando_seed_save_file(const char* path, const PcRandoSeed& s, std::string* err) {
    const std::string text = pc_rando_seed_serialize(s);
    const std::string tmp = std::string(path) + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (f == nullptr) {
        return failWith(err, "cannot create " + tmp);
    }
    const bool wrote = fwrite(text.data(), 1, text.size(), f) == text.size();
    const bool closed = fclose(f) == 0;
    if (!wrote || !closed) {
        remove(tmp.c_str());
        return failWith(err, "cannot write " + tmp);
    }
    if (rename(tmp.c_str(), path) != 0) {
        // Some file systems (the Switch's sdmc) do not rename over an existing file.
        remove(path);
        if (rename(tmp.c_str(), path) != 0) {
            remove(tmp.c_str());
            return failWith(err, std::string("cannot rename ") + tmp + " to " + path);
        }
    }
    return true;
}

int pc_rando_item_id(std::string_view name) {
    for (const ItemName& e : kItemNames) {
        if (name == e.name) {
            return e.id;
        }
    }
    return -1;
}

const char* pc_rando_item_name(uint8_t id) {
    // The last name registered for an id wins, as in wwrando's item_names dict.
    const char* found = nullptr;
    for (const ItemName& e : kItemNames) {
        if (e.id == id) {
            found = e.name;
        }
    }
    return found;
}
