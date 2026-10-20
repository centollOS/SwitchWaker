// "Any item anywhere" (pc_rando_field_items.h, docs/RANDOMIZER_PLAN.md step R2.11): wwrando tweaks.py
// allow_all_items_to_be_field_items as edits of R0.8's table registry, plus the mode_wait case test.
// Pure: the struct layouts of d/d_item_data.h only, no game table or call (cos_rando_core).
#include "pc_rando_field_items.h"

#include "pc_rando_items.h"

#include "d/d_item_data.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

// wwrando data/items_without_field_models.txt, every line that starts with "<hex byte> " (randomizer.py
// reads it with ^([0-9a-f]{2}) ), in file order (ascending).
constexpr uint8_t kIds[] = {
    0x1A, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
    0x31, 0x32, 0x33, 0x35, 0x36, 0x39, 0x3A, 0x3C, 0x3E, 0x3F, 0x42, 0x43, 0x4C, 0x4D, 0x4E, 0x50, 0x51, 0x52,
    0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B,
    0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x98, 0x99,
    0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xAA, 0xAB, 0xAC, 0xAD,
    0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1,
    0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3,
    0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5,
    0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE,
};
constexpr int kIdCount = (int)(sizeof kIds / sizeof kIds[0]);
static_assert(kIdCount == 171, "items_without_field_models.txt has 171 ids");

// The field entry's edit: everything before mHeapSize (arc, indices, mTevFrm, mUnknown), one edit.
constexpr size_t kFieldEditLen = offsetof(dItem_data_field_item_res, mHeapSize);
static_assert(kFieldEditLen <= PC_RANDO_TABLE_EDIT_MAX, "field_item_res edit fits one registry edit");
// item_resource's model block: mBmdIdx .. mTevFrm (wwrando's +8, 0xD bytes).
constexpr size_t kResBlockOff = offsetof(dItem_data_item_resource, mBmdIdx);
constexpr size_t kResBlockLen = offsetof(dItem_data_item_resource, mTevFrm) + 1 - kResBlockOff;
static_assert(kResBlockLen == 0xD, "item_resource model block is wwrando's 0xD bytes");
constexpr size_t kInfoOff = offsetof(dItem_data_item_info, mCollisionH);
static_assert(offsetof(dItem_data_item_info, mCollisionR) == kInfoOff + 1, "height and radius are adjacent");

bool sLoggedFail = false;

bool edit(int table, int index, size_t offset, const void* bytes, size_t len) {
    if (pc_rando_table_edit(table, index, offset, bytes, len)) {
        return true;
    }
    if (!sLoggedFail) {
        sLoggedFail = true;
        char buf[160];
        const int n = snprintf(buf, sizeof buf,
                               "[cos] rando: field items: table edit %d/0x%02X+%zu refused (registry full at %d?)\n",
                               table, index, offset, pc_rando_table_edit_count());
        if (n > 0) {
            (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
        }
    }
    return false;
}

} // namespace

const uint8_t* pc_rando_field_item_ids(int* count) {
    if (count != nullptr) {
        *count = kIdCount;
    }
    return kIds;
}

int pc_rando_field_item_source(uint8_t id, int* fixes_get_model) {
    if (fixes_get_model != nullptr) {
        *fixes_get_model = 0;
    }
    bool listed = false;
    for (uint8_t v : kIds) {
        listed = listed || v == id;
    }
    if (!listed) {
        return -1;
    }
    int src = id;
    switch (id) {
    case 0x39: // Master Sword (Powerless), (Half Power), (Full Power): no proper get model; Hero's Sword
    case 0x3A:
    case 0x3E:
        src = 0x38;
        break;
    case 0x6D: // the six songs use the Pirate's Charm model; the Wind Waker instead
    case 0x6E:
    case 0x6F:
    case 0x70:
    case 0x71:
    case 0x72:
        src = 0x22;
        break;
    case 0xAA: // Hurricane Spin: wwrando's Vscroll.arc; plan D12: the Wind Waker's resource
        src = 0x22;
        break;
    case 0xB1: // Magic Meter, Magic Meter Upgrade: no model; the Green Potion
    case 0xB2:
        src = 0x52;
        break;
    default:
        return src;
    }
    if (fixes_get_model != nullptr) {
        *fixes_get_model = 1;
    }
    return src;
}

int pc_rando_field_item_unnamed(uint8_t id) {
    return id == 0x2B || id == 0x2E || id == 0x54 || id == 0x6C || (id >= 0xA0 && id <= 0xA2) ||
           (id >= 0xB9 && id <= 0xC1);
}

int pc_rando_field_item_uses_arrow(uint8_t id) {
    // tweaks.py: mode_wait jump table entries 0x20-0x44 -> 0x800F8160 (itemActionForArrow), and the
    // switch's bound check (ids above 0x4B went to the default, itemActionForRupee) -> the same case.
    return (id >= 0x20 && id <= 0x44) || id > 0x4B;
}

int pc_rando_field_items_register(const dItem_data_item_resource* res, const dItem_data_field_item_res* field,
                                  const dItem_data_item_info* info) {
    if (res == nullptr || field == nullptr || info == nullptr) {
        return -1;
    }
    int n = 0;
    bool ok = true;
    for (uint8_t id : kIds) {
        int fixGet = 0;
        const int src = pc_rando_field_item_source(id, &fixGet);
        const dItem_data_item_resource& from = res[src];

        dItem_data_field_item_res f = field[id];
        f.mArc = from.mArcname;
        f.mBmdIdx = from.mBmdIdx;
        f.mSrtIdx = from.mSrtIdx;
        f.mSrtIdx2 = from.mSrtIdx2;
        f.mTevIdx = from.mTevIdx;
        f.mTevIdx2 = from.mTevIdx2;
        f.mBckIdx = from.mBckIdx;
        f.mTevFrm = from.mTevFrm;
        f.mUnknown = from.mUnknown;
        ok = edit(PCR_TBL_FIELD_ITEM_RES, id, 0, &f, kFieldEditLen) && ok;
        n++;

        if (fixGet) {
            const char* arc = from.mArcname;
            ok = edit(PCR_TBL_ITEM_RESOURCE, id, offsetof(dItem_data_item_resource, mArcname), &arc, sizeof arc) && ok;
            ok = edit(PCR_TBL_ITEM_RESOURCE, id, kResBlockOff, (const uint8_t*)&from + kResBlockOff, kResBlockLen) &&
                 ok;
            ok = edit(PCR_TBL_ITEM_RESOURCE, id, offsetof(dItem_data_item_resource, mUnknown), &from.mUnknown,
                      sizeof from.mUnknown) &&
                 ok;
            n += 3;
        }
    }
    for (int id = 0; id < 0x100; id++) {
        const u8 h = info[id].mCollisionH;
        const u8 r = info[id].mCollisionR;
        if (h != 0 && r != 0) {
            continue;
        }
        const u8 hr[2] = {h != 0 ? h : (u8)PC_RANDO_FIELD_ITEM_MIN_HR, r != 0 ? r : (u8)PC_RANDO_FIELD_ITEM_MIN_HR};
        ok = edit(PCR_TBL_ITEM_INFO, id, kInfoOff, hr, sizeof hr) && ok;
        n++;
    }
    return ok ? n : -1;
}
