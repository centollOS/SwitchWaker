/*
 * pc_rando_msg.h - the randomizer's message overlay (docs/RANDOMIZER_PLAN.md D10, step R0.12).
 *
 * wwrando rewrites zel_00.bmg: it replaces the text of vanilla messages (msg.string = ...), adds new
 * message numbers (bmg.add_new_message) and sets INF1 entry fields (text box type, draw type, item
 * image, ...). The port does not rebuild the BMG. It keeps an overlay table instead,
 *
 *     msgNo -> { optional INF1 entry fields, optional encoded text }
 *
 * which four hunks in f_op_msg_mng.cpp consult while pc_rando() is 1:
 * fopMsgM_msgGet_c::getMessage / getMesgEntry and fopMsgM_itemMsgGet_c::getMessage / getMesgEntry
 * (through pc_rando_msg_hook_text / pc_rando_msg_hook_entry, declared in pc/pc_rando_hooks.h). Every
 * reader of BMG text goes through those (talk boxes, item-get boxes, fopMsgM_messageGet for menus and
 * names), except the JMessage/STB path, which never shows randomized text.
 *
 *  - A record with text and no entry ("set_text") replaces the text of a vanilla message and keeps
 *    its BMG entry. Given for a number the BMG lacks, the entry is wwrando's new-message defaults.
 *  - A record with an entry and no text ("set_entry") keeps the BMG text and replaces the entry.
 *  - "add" sets both: a new message, as bmg.add_new_message.
 *
 * Text is given in wwrando's string syntax (gclib bmg.py Message.write_string): characters are bytes
 * (code points up to 0xFF; the input is UTF-8), and "\{1A 06 FF 00 00 01}" writes the hex bytes in
 * the braces (control tags). pc_rando_msg_encode reproduces write_string byte for byte, quirks
 * included, and refuses what the USA game cannot show as written: a code point above 0xFF, a plain
 * character 0x80-0x9F (the text code reads it as a Shift-JIS lead byte and eats the next byte; see
 * fopMsgM_messageGet's hi_nibble 8/9 case), a 0x1A tag whose size byte is not its length, and an
 * unclosed brace.
 *
 * The overlay lives in malloc'd memory (not the JKR heaps), so it survives stage changes. It is
 * cleared when pc_rando() turns 0 (pc_rando_deactivate on a non-forced boot), so the owner of each
 * entry rebuilds it at the next activation; pc_rando_msg_clear clears it at once.
 *
 * Pure code: no game headers, no game calls (it links into cos_rando_core and the unit tests).
 * Game thread only.
 */
#ifndef PC_RANDO_MSG_H
#define PC_RANDO_MSG_H

#include "pc/pc_rando_hooks.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- control tags in wwrando's string syntax (string literals, to paste into a message) --- */
/* Text colour (group 0xFF, code 0, one byte: a BMC colour index). 00 is the default colour; wwrando
   uses 01 (red) for item and place names and 05 for hint text, 02/04/07 for hint importance. */
#define PCR_MSG_COLOR(nn) "\\{1A 06 FF 00 00 " #nn "}"
#define PCR_MSG_COLOR_DEFAULT PCR_MSG_COLOR(00)
#define PCR_MSG_COLOR_RED PCR_MSG_COLOR(01)
/* The player's name (group 0, code 0). */
#define PCR_MSG_PLAYER_NAME "\\{1A 05 00 00 00}"
/* Draw the rest of the box at once (group 0, code 1; wwrando starts item-get texts with it). */
#define PCR_MSG_INSTANT "\\{1A 05 00 00 01}"
/* A two-option choice; the two options follow on the next two lines (group 0, code 8). */
#define PCR_MSG_SELECT_TWO "\\{1A 05 00 00 08}"
/* A three-option choice (group 0, code 9). */
#define PCR_MSG_SELECT_THREE "\\{1A 05 00 00 09}"

/* Tag groups and codes for pc_rando_msg_tag. */
#define PCR_MSG_GROUP_SYSTEM 0x00 /* MsgCtrlCode_* (f_op_msg_mng.h) */
#define PCR_MSG_GROUP_STYLE 0xFF  /* code 0 colour (1 byte), code 1 text scale (u16 percent) */
#define PCR_MSG_CODE_WAIT 0x07    /* group 0: wait, u16 frames */

/* Writes one control tag as bytes: 1A, size (5 + nargs), group, code (big-endian u16), args.
   Returns the bytes written, or 0 when it does not fit in `cap` or nargs > 250. */
size_t pc_rando_msg_tag(uint8_t* out, size_t cap, uint8_t group, uint16_t code, const uint8_t* args,
                        size_t nargs);
/* The same tag in wwrando's string syntax ("\{1A 07 00 00 07 00 1E}"), NUL-terminated. Returns the
   length without the NUL, or 0 when it does not fit. */
size_t pc_rando_msg_tag_str(char* out, size_t cap, uint8_t group, uint16_t code, const uint8_t* args,
                            size_t nargs);
/* "\{1A 07 00 00 07 HH LL}": wait `frames` frames. Same return as pc_rando_msg_tag_str. */
size_t pc_rando_msg_wait_str(char* out, size_t cap, uint16_t frames);
/* "\{1A 06 FF 00 00 NN}": switch to colour index `color`. Same return. */
size_t pc_rando_msg_color_str(char* out, size_t cap, uint8_t color);

/* --- encoding --- */
#define PCR_MSG_OK 0
#define PCR_MSG_ERR_UTF8 1      /* the input is not valid UTF-8 */
#define PCR_MSG_ERR_CHAR 2      /* a code point above 0xFF, or a plain 0x80-0x9F (Shift-JIS lead) */
#define PCR_MSG_ERR_BRACE 3     /* "\{" without "}", or a hex number above 0xFF in the braces */
#define PCR_MSG_ERR_TAG_SIZE 4  /* a "\{1A ...}" group whose size byte is not its byte count */
#define PCR_MSG_ERR_SPACE 5     /* the output does not fit */

/* Encodes `text` (wwrando syntax, UTF-8, NUL-terminated) into BMG bytes ending with the NUL that
   write_string appends. *out_len (if not NULL) gets the byte count including that NUL, also when
   `out` is NULL or too small (then PCR_MSG_ERR_SPACE): call with out NULL to size a buffer. Returns
   PCR_MSG_OK or a PCR_MSG_ERR_* code. */
int pc_rando_msg_encode(const char* text, uint8_t* out, size_t cap, size_t* out_len);
/* Decodes BMG bytes (up to the terminating NUL outside a tag, or `len` bytes) into wwrando syntax as
   gclib's Message.read_string does ("\{1A 05 00 00 01}", tag bytes in upper-case hex; characters
   0x80-0xFF as UTF-8). Always NUL-terminates when cap > 0. Returns the length of the full result
   (without the NUL), which is >= cap when it was cut. */
size_t pc_rando_msg_decode(const uint8_t* bytes, size_t len, char* out, size_t cap);

/* --- INF1 entry fields (gclib bmg.py Message; offsets in the 0x18-byte BMG entry) --- */
typedef struct PcRandoMsgEntry {
    uint16_t item_price;        /* 0x06 */
    uint16_t next_message_id;   /* 0x08 */
    uint16_t unknown_1;         /* 0x0A (0x60 for a new message) */
    uint8_t text_box_type;      /* 0x0C: 0 dialog, 1 special, 2 wood, 5 none, 6 stone, 7 parchment,
                                   9 item get, 0xA hint, 0xD centered, 0xE song */
    uint8_t initial_draw_type;  /* 0x0D: 0 normal, 1 instant, 2 slow */
    uint8_t text_box_position;  /* 0x0E: 3 for a new message, 2 centered */
    uint8_t display_item_id;    /* 0x0F: 0xFF none */
    uint8_t text_alignment;     /* 0x10: 4 bottom text box */
    uint8_t initial_sound;      /* 0x11 */
    uint8_t initial_camera_behavior; /* 0x12 */
    uint8_t initial_speaker_anim;    /* 0x13 */
    uint8_t unknown_3;          /* 0x14 */
    uint16_t num_lines_per_box; /* 0x15-0x16 (big-endian, unaligned); 4 for a new message */
    uint8_t unknown_4;          /* 0x17 */
} PcRandoMsgEntry;

#define PCR_MSG_ENTRY_SIZE 0x18

/* gclib's Message() defaults: what bmg.add_new_message gives before the caller sets fields. */
void pc_rando_msg_entry_defaults(PcRandoMsgEntry* e);
/* Writes `e` as a big-endian BMG INF1 entry with the given message number and string offset. */
void pc_rando_msg_entry_pack(const PcRandoMsgEntry* e, uint16_t msg_no, uint32_t data_offs,
                             uint8_t out[PCR_MSG_ENTRY_SIZE]);
/* Reads a big-endian BMG INF1 entry. */
void pc_rando_msg_entry_unpack(const uint8_t in[PCR_MSG_ENTRY_SIZE], PcRandoMsgEntry* e);

/* --- the overlay --- */
typedef struct PcRandoMsg {
    uint16_t msg_no;
    uint8_t has_entry;
    PcRandoMsgEntry entry;  /* valid when has_entry */
    const uint8_t* bytes;   /* encoded text ending with its NUL, or NULL (keep the BMG's) */
    size_t len;             /* bytes, including the NUL; 0 when bytes is NULL */
} PcRandoMsg;

/* A new message (bmg.add_new_message, then fields and msg.string): `entry` NULL means the defaults.
   Replaces any record for msg_no. Returns PCR_MSG_OK or a PCR_MSG_ERR_* code (then nothing changed). */
int pc_rando_msg_add(uint16_t msg_no, const PcRandoMsgEntry* entry, const char* text);
/* New text for msg_no (msg.string = ...); keeps the record's entry, or the BMG's when there is none.
   Same return. */
int pc_rando_msg_set_text(uint16_t msg_no, const char* text);
/* As set_text with already-encoded bytes (`len` counts up to and including a NUL, which is appended
   when missing). Same return. */
int pc_rando_msg_set_bytes(uint16_t msg_no, const uint8_t* bytes, size_t len);
/* New entry fields for msg_no; keeps the record's text, or the BMG's when there is none. Returns
   PCR_MSG_OK or PCR_MSG_ERR_SPACE (out of memory). */
int pc_rando_msg_set_entry(uint16_t msg_no, const PcRandoMsgEntry* entry);
/* Drops msg_no's record. Returns 1 when there was one. */
int pc_rando_msg_remove(uint16_t msg_no);
/* Drops every record. */
void pc_rando_msg_clear(void);
/* The record for msg_no, or NULL. Valid until the next change to the overlay. Does not test
   pc_rando(). */
const PcRandoMsg* pc_rando_msg_find(uint16_t msg_no);
/* The number of records. */
int pc_rando_msg_count(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_MSG_H */
