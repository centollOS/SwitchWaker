// The randomizer's message overlay and text encoding (pc_rando_msg.h, docs/RANDOMIZER_PLAN.md D10,
// step R0.12). Keep this file free of game headers and game calls: it links into cos_rando_core and
// the unit tests (native/check/rando/README.md). Storage is malloc'd, never the JKR heaps (cos_pc's
// operator new), so the overlay survives stage changes.
#include "pc_rando_msg.h"

#include "pc/pc_rando.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// ---------------------------------------------------------------------------------------------
// UTF-8

// Decodes one code point at s[*i]; returns -1 on a malformed sequence.
long utf8Next(const char* s, size_t* i) {
    const unsigned char c = (unsigned char)s[*i];
    if (c < 0x80) {
        (*i)++;
        return c;
    }
    int n;
    long cp;
    if ((c & 0xE0) == 0xC0) {
        n = 1;
        cp = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        n = 2;
        cp = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        n = 3;
        cp = c & 0x07;
    } else {
        return -1;
    }
    for (int k = 1; k <= n; k++) {
        const unsigned char d = (unsigned char)s[*i + k];
        if ((d & 0xC0) != 0x80) {
            return -1;
        }
        cp = (cp << 6) | (d & 0x3F);
    }
    static const long kMin[4] = {0, 0x80, 0x800, 0x10000};
    if (cp < kMin[n] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        return -1;
    }
    *i += (size_t)n + 1;
    return cp;
}

int hexVal(long c) {
    if (c >= '0' && c <= '9') {
        return (int)(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return (int)(c - 'a' + 10);
    }
    if (c >= 'A' && c <= 'F') {
        return (int)(c - 'A' + 10);
    }
    return -1;
}

// ---------------------------------------------------------------------------------------------
// The overlay: a malloc'd array sorted by msg_no; each record's bytes are their own malloc.

PcRandoMsg* sRecs = nullptr;
int sCount = 0;
int sCap = 0;
bool sListening = false;

void onRando(int active) {
    if (!active) {
        pc_rando_msg_clear();
    }
}

int lowerBound(uint16_t msg_no) {
    int lo = 0;
    int hi = sCount;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (sRecs[mid].msg_no < msg_no) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

// The record for msg_no, created empty when missing; NULL when out of memory.
PcRandoMsg* findOrInsert(uint16_t msg_no) {
    if (!sListening) {
        sListening = pc_rando_add_listener(onRando) != 0;
    }
    const int at = lowerBound(msg_no);
    if (at < sCount && sRecs[at].msg_no == msg_no) {
        return &sRecs[at];
    }
    if (sCount == sCap) {
        const int cap = sCap == 0 ? 64 : sCap * 2;
        PcRandoMsg* grown = (PcRandoMsg*)realloc(sRecs, (size_t)cap * sizeof(PcRandoMsg));
        if (grown == nullptr) {
            return nullptr;
        }
        sRecs = grown;
        sCap = cap;
    }
    memmove(&sRecs[at + 1], &sRecs[at], (size_t)(sCount - at) * sizeof(PcRandoMsg));
    sCount++;
    PcRandoMsg* r = &sRecs[at];
    memset(r, 0, sizeof *r);
    r->msg_no = msg_no;
    return r;
}

// Encodes text into a fresh malloc'd buffer.
int encodeAlloc(const char* text, uint8_t** out, size_t* len) {
    size_t n = 0;
    int rc = pc_rando_msg_encode(text, nullptr, 0, &n);
    if (rc != PCR_MSG_OK && rc != PCR_MSG_ERR_SPACE) {
        return rc;
    }
    uint8_t* buf = (uint8_t*)malloc(n);
    if (buf == nullptr) {
        return PCR_MSG_ERR_SPACE;
    }
    rc = pc_rando_msg_encode(text, buf, n, &n);
    if (rc != PCR_MSG_OK) {
        free(buf);
        return rc;
    }
    *out = buf;
    *len = n;
    return PCR_MSG_OK;
}

void put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

uint16_t get16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

} // namespace

extern "C" {

// ---------------------------------------------------------------------------------------------
// Tags

size_t pc_rando_msg_tag(uint8_t* out, size_t cap, uint8_t group, uint16_t code, const uint8_t* args,
                        size_t nargs) {
    const size_t n = 5 + nargs;
    if (nargs > 250 || out == nullptr || cap < n) {
        return 0;
    }
    out[0] = 0x1A;
    out[1] = (uint8_t)n;
    out[2] = group;
    put16(out + 3, code);
    if (nargs > 0) {
        memcpy(out + 5, args, nargs);
    }
    return n;
}

size_t pc_rando_msg_tag_str(char* out, size_t cap, uint8_t group, uint16_t code, const uint8_t* args,
                            size_t nargs) {
    uint8_t bytes[256];
    const size_t n = pc_rando_msg_tag(bytes, sizeof bytes, group, code, args, nargs);
    // "\{" + "XX" + n-1 times " XX" + "}"
    const size_t need = 2 + 2 + (n - 1) * 3 + 1;
    if (n == 0 || out == nullptr || cap < need + 1) {
        return 0;
    }
    size_t o = 0;
    out[o++] = '\\';
    out[o++] = '{';
    for (size_t i = 0; i < n; i++) {
        o += (size_t)snprintf(out + o, cap - o, i == 0 ? "%02X" : " %02X", bytes[i]);
    }
    out[o++] = '}';
    out[o] = '\0';
    return o;
}

size_t pc_rando_msg_wait_str(char* out, size_t cap, uint16_t frames) {
    const uint8_t args[2] = {(uint8_t)(frames >> 8), (uint8_t)frames};
    return pc_rando_msg_tag_str(out, cap, PCR_MSG_GROUP_SYSTEM, PCR_MSG_CODE_WAIT, args, 2);
}

size_t pc_rando_msg_color_str(char* out, size_t cap, uint8_t color) {
    return pc_rando_msg_tag_str(out, cap, PCR_MSG_GROUP_STYLE, 0, &color, 1);
}

// ---------------------------------------------------------------------------------------------
// Encoding: gclib bmg.py Message.write_string, including its quirks: a backslash is dropped and from
// then on (the flag is never reset) every "{" opens a brace group up to the next "}", whose hex
// numbers ([0-9a-fA-F]+, any other character separates them) are written as bytes.

int pc_rando_msg_encode(const char* text, uint8_t* out, size_t cap, size_t* out_len) {
    size_t n = 0;
    bool escaped = false;
    size_t i = 0;
    auto emit = [&](uint8_t b) {
        if (out != nullptr && n < cap) {
            out[n] = b;
        }
        n++;
    };
    while (text[i] != '\0') {
        const long c = utf8Next(text, &i);
        if (c < 0) {
            return PCR_MSG_ERR_UTF8;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (escaped && c == '{') {
            const char* close = strchr(text + i, '}');
            if (close == nullptr) {
                return PCR_MSG_ERR_BRACE;
            }
            const size_t end = (size_t)(close - text);
            uint8_t sizeByte = 0;
            bool isTag = false;
            size_t count = 0;
            long val = -1;
            size_t j = i;
            while (true) {
                const long d = j < end ? utf8Next(text, &j) : -2; // -2: the closing brace
                if (d == -1) {
                    return PCR_MSG_ERR_UTF8;
                }
                const int h = d >= 0 ? hexVal(d) : -1;
                if (h >= 0) {
                    val = (val < 0 ? 0 : val) * 16 + h;
                    if (val > 0xFF) {
                        return PCR_MSG_ERR_BRACE;
                    }
                    continue;
                }
                if (val >= 0) {
                    if (count == 0) {
                        isTag = val == 0x1A;
                    } else if (count == 1) {
                        sizeByte = (uint8_t)val;
                    }
                    emit((uint8_t)val);
                    count++;
                    val = -1;
                }
                if (d == -2) {
                    break;
                }
            }
            if (isTag && (count < 2 || sizeByte != count)) {
                return PCR_MSG_ERR_TAG_SIZE;
            }
            i = end + 1;
            continue;
        }
        if (c > 0xFF || (c >= 0x80 && c <= 0x9F)) {
            return PCR_MSG_ERR_CHAR;
        }
        emit((uint8_t)c);
    }
    emit(0);
    if (out_len != nullptr) {
        *out_len = n;
    }
    return (out != nullptr && n <= cap) ? PCR_MSG_OK : PCR_MSG_ERR_SPACE;
}

size_t pc_rando_msg_decode(const uint8_t* bytes, size_t len, char* out, size_t cap) {
    size_t o = 0;
    auto put = [&](const char* s, size_t k) {
        for (size_t q = 0; q < k; q++) {
            if (out != nullptr && o + 1 < cap) {
                out[o] = s[q];
            }
            o++;
        }
    };
    size_t i = 0;
    while (i < len && bytes[i] != 0) {
        const uint8_t b = bytes[i++];
        if (b == 0x1A && i < len) {
            const uint8_t size = bytes[i++];
            char tmp[8];
            snprintf(tmp, sizeof tmp, "\\{%02X %02X", b, size);
            put(tmp, strlen(tmp));
            for (int k = 0; k < (int)size - 2 && i < len; k++) {
                snprintf(tmp, sizeof tmp, " %02X", bytes[i++]);
                put(tmp, 3);
            }
            put("}", 1);
        } else if (b < 0x80) {
            const char ch = (char)b;
            put(&ch, 1);
        } else {
            const char u[2] = {(char)(0xC0 | (b >> 6)), (char)(0x80 | (b & 0x3F))};
            put(u, 2);
        }
    }
    if (out != nullptr && cap > 0) {
        out[o < cap ? o : cap - 1] = '\0';
    }
    return o;
}

// ---------------------------------------------------------------------------------------------
// Entries

void pc_rando_msg_entry_defaults(PcRandoMsgEntry* e) {
    memset(e, 0, sizeof *e);
    e->unknown_1 = 0x60;
    e->text_box_type = 0;
    e->initial_draw_type = 0;
    e->text_box_position = 3;
    e->display_item_id = 0xFF;
    e->num_lines_per_box = 4;
}

void pc_rando_msg_entry_pack(const PcRandoMsgEntry* e, uint16_t msg_no, uint32_t data_offs,
                             uint8_t out[PCR_MSG_ENTRY_SIZE]) {
    out[0] = (uint8_t)(data_offs >> 24);
    out[1] = (uint8_t)(data_offs >> 16);
    out[2] = (uint8_t)(data_offs >> 8);
    out[3] = (uint8_t)data_offs;
    put16(out + 0x04, msg_no);
    put16(out + 0x06, e->item_price);
    put16(out + 0x08, e->next_message_id);
    put16(out + 0x0A, e->unknown_1);
    out[0x0C] = e->text_box_type;
    out[0x0D] = e->initial_draw_type;
    out[0x0E] = e->text_box_position;
    out[0x0F] = e->display_item_id;
    out[0x10] = e->text_alignment;
    out[0x11] = e->initial_sound;
    out[0x12] = e->initial_camera_behavior;
    out[0x13] = e->initial_speaker_anim;
    out[0x14] = e->unknown_3;
    put16(out + 0x15, e->num_lines_per_box);
    out[0x17] = e->unknown_4;
}

void pc_rando_msg_entry_unpack(const uint8_t in[PCR_MSG_ENTRY_SIZE], PcRandoMsgEntry* e) {
    e->item_price = get16(in + 0x06);
    e->next_message_id = get16(in + 0x08);
    e->unknown_1 = get16(in + 0x0A);
    e->text_box_type = in[0x0C];
    e->initial_draw_type = in[0x0D];
    e->text_box_position = in[0x0E];
    e->display_item_id = in[0x0F];
    e->text_alignment = in[0x10];
    e->initial_sound = in[0x11];
    e->initial_camera_behavior = in[0x12];
    e->initial_speaker_anim = in[0x13];
    e->unknown_3 = in[0x14];
    e->num_lines_per_box = get16(in + 0x15);
    e->unknown_4 = in[0x17];
}

// ---------------------------------------------------------------------------------------------
// The overlay

int pc_rando_msg_set_bytes(uint16_t msg_no, const uint8_t* bytes, size_t len) {
    const bool terminated = len > 0 && bytes[len - 1] == 0;
    const size_t n = terminated ? len : len + 1;
    uint8_t* buf = (uint8_t*)malloc(n);
    if (buf == nullptr) {
        return PCR_MSG_ERR_SPACE;
    }
    if (len > 0) {
        memcpy(buf, bytes, len);
    }
    buf[n - 1] = 0;
    PcRandoMsg* r = findOrInsert(msg_no);
    if (r == nullptr) {
        free(buf);
        return PCR_MSG_ERR_SPACE;
    }
    free((void*)r->bytes);
    r->bytes = buf;
    r->len = n;
    return PCR_MSG_OK;
}

int pc_rando_msg_set_text(uint16_t msg_no, const char* text) {
    uint8_t* buf = nullptr;
    size_t len = 0;
    const int rc = encodeAlloc(text, &buf, &len);
    if (rc != PCR_MSG_OK) {
        return rc;
    }
    PcRandoMsg* r = findOrInsert(msg_no);
    if (r == nullptr) {
        free(buf);
        return PCR_MSG_ERR_SPACE;
    }
    free((void*)r->bytes);
    r->bytes = buf;
    r->len = len;
    return PCR_MSG_OK;
}

int pc_rando_msg_set_entry(uint16_t msg_no, const PcRandoMsgEntry* entry) {
    PcRandoMsg* r = findOrInsert(msg_no);
    if (r == nullptr) {
        return PCR_MSG_ERR_SPACE;
    }
    r->entry = *entry;
    r->has_entry = 1;
    return PCR_MSG_OK;
}

int pc_rando_msg_add(uint16_t msg_no, const PcRandoMsgEntry* entry, const char* text) {
    uint8_t* buf = nullptr;
    size_t len = 0;
    const int rc = encodeAlloc(text, &buf, &len);
    if (rc != PCR_MSG_OK) {
        return rc;
    }
    PcRandoMsg* r = findOrInsert(msg_no);
    if (r == nullptr) {
        free(buf);
        return PCR_MSG_ERR_SPACE;
    }
    free((void*)r->bytes);
    r->bytes = buf;
    r->len = len;
    if (entry != nullptr) {
        r->entry = *entry;
    } else {
        pc_rando_msg_entry_defaults(&r->entry);
    }
    r->has_entry = 1;
    return PCR_MSG_OK;
}

int pc_rando_msg_remove(uint16_t msg_no) {
    const int at = lowerBound(msg_no);
    if (at >= sCount || sRecs[at].msg_no != msg_no) {
        return 0;
    }
    free((void*)sRecs[at].bytes);
    memmove(&sRecs[at], &sRecs[at + 1], (size_t)(sCount - at - 1) * sizeof(PcRandoMsg));
    sCount--;
    return 1;
}

void pc_rando_msg_clear(void) {
    for (int i = 0; i < sCount; i++) {
        free((void*)sRecs[i].bytes);
    }
    free(sRecs);
    sRecs = nullptr;
    sCount = 0;
    sCap = 0;
}

const PcRandoMsg* pc_rando_msg_find(uint16_t msg_no) {
    const int at = lowerBound(msg_no);
    return (at < sCount && sRecs[at].msg_no == msg_no) ? &sRecs[at] : nullptr;
}

int pc_rando_msg_count(void) {
    return sCount;
}

// ---------------------------------------------------------------------------------------------
// The hooks (pc/pc_rando_hooks.h)

const char* pc_rando_msg_hook_text(uint16_t msg_no) {
    const PcRandoMsg* r = pc_rando_msg_find(msg_no);
    return r != nullptr ? (const char*)r->bytes : nullptr;
}

void pc_rando_msg_hook_entry(uint16_t msg_no, void* entry) {
    const PcRandoMsg* r = pc_rando_msg_find(msg_no);
    if (r == nullptr) {
        return;
    }
    uint8_t* e = (uint8_t*)entry;
    const uint32_t offs = ((uint32_t)e[0] << 24) | ((uint32_t)e[1] << 16) | ((uint32_t)e[2] << 8) | e[3];
    if (r->has_entry) {
        pc_rando_msg_entry_pack(&r->entry, msg_no, offs, e);
    } else if (r->bytes != nullptr && get16(e + 4) != msg_no) {
        // Text only, for a number the BMG lacks: a new message with gclib's defaults.
        PcRandoMsgEntry d;
        pc_rando_msg_entry_defaults(&d);
        pc_rando_msg_entry_pack(&d, msg_no, 0, e);
    }
}

} // extern "C"
