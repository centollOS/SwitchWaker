// cos_rando_test_R2_11 (docs/RANDOMIZER_PLAN.md step R2.11): "any item anywhere" without a game, on
// synthetic item tables of the real struct types (d/d_item_data.h), bound to R0.8's registry:
//   - the id list equals wwrando's data/items_without_field_models.txt (fixture
//     R2_11_items_without_field_models.txt, parsed with randomizer.py's regex);
//   - the source of each id (Master Swords 0x38, songs and Hurricane Spin 0x22, magic 0x52, else itself)
//     and which ids also get their item_resource fixed;
//   - the mode_wait arrow cases (0x20-0x44 and 0x4C+);
//   - after activation: field_item_res of every listed id holds the source's arc, model block, mTevFrm
//     and mUnknown and keeps its own heap size; the fixed item_resource entries hold the source's arc,
//     block and mUnknown and keep their texture, message and heap size; unlisted entries are untouched;
//     item_info heights/radii of 0 are 0x28 and nothing else changed;
//   - deactivation gives back the boot bytes; registering again replaces (same edit count).
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc_rando_field_items.h"
#include "pc_rando_items.h"

#include "d/d_item_data.h"

#include <cstddef>
#include <cstring>
#include <cctype>
#include <set>
#include <string>

namespace {

dItem_data_item_resource sRes[0x100];
dItem_data_field_item_res sField[0x100];
dItem_data_item_info sInfo[0x100];
dItem_data_item_resource sBootRes[0x100];
dItem_data_field_item_res sBootField[0x100];
dItem_data_item_info sBootInfo[0x100];
char sArcNames[0x100][8];
char sFieldArcNames[0x100][8];

void fill() {
    for (int i = 0; i < 0x100; i++) {
        snprintf(sArcNames[i], sizeof sArcNames[i], "r%02X", i);
        snprintf(sFieldArcNames[i], sizeof sFieldArcNames[i], "f%02X", i);
        dItem_data_item_resource& r = sRes[i];
        memset(&r, 0, sizeof r);
        r.mArcname = sArcNames[i];
        r.mTexture = sFieldArcNames[i];
        r.mBmdIdx = (s16)(0x100 + i);
        r.mSrtIdx = (s16)(0x200 + i);
        r.mSrtIdx2 = (s16)(0x300 + i);
        r.mTevIdx = (s16)(0x400 + i);
        r.mTevIdx2 = (s16)(0x500 + i);
        r.mBckIdx = (s16)(0x600 + i);
        r.mTevFrm = (s8)(i & 0x7F);
        r.mItemMesgNum = (s16)(0x700 + i);
        r.mUnused = 0x800u + i;
        r.mUnknown = 0x900u + i;
        r.mHeapSize = (u16)(0xA00 + i);
        dItem_data_field_item_res& f = sField[i];
        memset(&f, 0, sizeof f);
        f.mArc = (i % 7 == 0) ? nullptr : sFieldArcNames[i];
        f.mBmdIdx = (s16)(-0x100 - i);
        f.mSrtIdx = -1;
        f.mSrtIdx2 = -1;
        f.mTevIdx = -1;
        f.mTevIdx2 = -1;
        f.mBckIdx = -1;
        f.mTevFrm = -1;
        f.mUnknown = 0xB00u + i;
        f.mHeapSize = (u16)(0xC00 + i);
        dItem_data_item_info& n = sInfo[i];
        n.mShadowSize = (u8)i;
        n.mCollisionH = (i % 3 == 0) ? 0 : (u8)(i | 1);
        n.mCollisionR = (i % 5 == 0) ? 0 : (u8)(i | 2);
        n.mFlag = (u8)(i ^ 0x5A);
    }
    memcpy(sBootRes, sRes, sizeof sRes);
    memcpy(sBootField, sField, sizeof sField);
    memcpy(sBootInfo, sInfo, sizeof sInfo);
}

bool atBoot() {
    return memcmp(sRes, sBootRes, sizeof sRes) == 0 && memcmp(sField, sBootField, sizeof sField) == 0 &&
           memcmp(sInfo, sBootInfo, sizeof sInfo) == 0;
}

void* sResPtr[0x100];
void* sFieldPtr[0x100];
void* sInfoPtr[0x100];

void bind() {
    for (int i = 0; i < 0x100; i++) {
        sResPtr[i] = &sRes[i];
        sFieldPtr[i] = &sField[i];
        sInfoPtr[i] = &sInfo[i];
    }
    PcRandoTableDesc t[PCR_TBL_COUNT] = {};
    t[PCR_TBL_ITEM_RESOURCE] = {sResPtr, 0x100, sizeof(dItem_data_item_resource)};
    t[PCR_TBL_FIELD_ITEM_RES] = {sFieldPtr, 0x100, sizeof(dItem_data_field_item_res)};
    t[PCR_TBL_ITEM_INFO] = {sInfoPtr, 0x100, sizeof(dItem_data_item_info)};
    RT_CHECK(pc_rando_items_bind_tables(t, PCR_TBL_COUNT));
}

bool sameModel(const dItem_data_field_item_res& f, const dItem_data_item_resource& r) {
    return f.mArc == r.mArcname && f.mBmdIdx == r.mBmdIdx && f.mSrtIdx == r.mSrtIdx && f.mSrtIdx2 == r.mSrtIdx2 &&
           f.mTevIdx == r.mTevIdx && f.mTevIdx2 == r.mTevIdx2 && f.mBckIdx == r.mBckIdx && f.mTevFrm == r.mTevFrm &&
           f.mUnknown == r.mUnknown;
}

} // namespace

int main() {
    // The id list against wwrando's data file.
    std::string text;
    RT_CHECK(rt::readFixture("R2_11_items_without_field_models.txt", text));
    std::set<int> want, unnamed;
    // randomizer.py: re.findall(r"^([0-9a-f]{2}) ", ..., re.IGNORECASE | re.MULTILINE)
    for (size_t pos = 0; pos < text.size();) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) {
            end = text.size();
        }
        if (end - pos >= 3 && isxdigit((unsigned char)text[pos]) && isxdigit((unsigned char)text[pos + 1]) &&
            text[pos + 2] == ' ') {
            const int id = (int)strtol(text.substr(pos, 2).c_str(), nullptr, 16);
            want.insert(id);
            if (text.find_first_not_of(" \t\r", pos + 3) >= end) {
                unnamed.insert(id);
            }
        }
        pos = end + 1;
    }
    int count = 0;
    const uint8_t* ids = pc_rando_field_item_ids(&count);
    RT_CHECK_EQ(count, 171);
    RT_CHECK_EQ((int)want.size(), count);
    std::set<int> have;
    for (int i = 0; i < count; i++) {
        have.insert(ids[i]);
        if (i > 0) {
            RT_CHECK(ids[i] > ids[i - 1]);
        }
    }
    RT_CHECK(have == want);
    RT_CHECK_EQ((int)unnamed.size(), 16);
    for (int id = 0; id < 0x100; id++) {
        RT_CHECK_EQ(pc_rando_field_item_unnamed((uint8_t)id), (int)unnamed.count(id));
    }

    // Sources.
    int fix = -1;
    RT_CHECK_EQ(pc_rando_field_item_source(0x20, &fix), 0x20);
    RT_CHECK_EQ(fix, 0);
    RT_CHECK_EQ(pc_rando_field_item_source(0x38, &fix), -1); // the Hero's Sword has a field model
    RT_CHECK_EQ(pc_rando_field_item_source(0x01, nullptr), -1);
    int fixedCount = 0;
    for (int id = 0; id < 0x100; id++) {
        const int src = pc_rando_field_item_source((uint8_t)id, &fix);
        if (src < 0) {
            RT_CHECK(have.count(id) == 0);
            continue;
        }
        int expect = id, expectFix = 1;
        if (id == 0x39 || id == 0x3A || id == 0x3E) {
            expect = 0x38;
        } else if ((id >= 0x6D && id <= 0x72) || id == 0xAA) {
            expect = 0x22;
        } else if (id == 0xB1 || id == 0xB2) {
            expect = 0x52;
        } else {
            expectFix = 0;
        }
        RT_CHECK_EQ(src, expect);
        RT_CHECK_EQ(fix, expectFix);
        fixedCount += fix;
    }
    RT_CHECK_EQ(fixedCount, 12);

    // mode_wait cases.
    for (int id = 0; id < 0x100; id++) {
        const int arrow = (id >= 0x20 && id <= 0x44) || id >= 0x4C;
        RT_CHECK_EQ(pc_rando_field_item_uses_arrow((uint8_t)id), arrow);
    }

    // The table edits.
    fill();
    bind();
    RT_CHECK_EQ(pc_rando_tables_applied(), 0);
    const int before = pc_rando_table_edit_count();
    const int n = pc_rando_field_items_register(sRes, sField, sInfo);
    int zeroInfo = 0;
    for (int i = 0; i < 0x100; i++) {
        zeroInfo += sBootInfo[i].mCollisionH == 0 || sBootInfo[i].mCollisionR == 0;
    }
    RT_CHECK_EQ(n, count + 3 * fixedCount + zeroInfo);
    RT_CHECK_EQ(pc_rando_table_edit_count(), before + n);
    RT_CHECK(atBoot()); // registered while inactive: nothing written yet

    pc_rando_set_active(nullptr, nullptr);
    RT_CHECK_EQ(pc_rando(), 1);
    RT_CHECK_EQ(pc_rando_tables_applied(), 1);
    for (int id = 0; id < 0x100; id++) {
        const int src = pc_rando_field_item_source((uint8_t)id, &fix);
        const dItem_data_field_item_res& f = sField[id];
        const dItem_data_item_resource& r = sRes[id];
        if (src < 0) {
            RT_CHECK(memcmp(&f, &sBootField[id], sizeof f) == 0);
            RT_CHECK(memcmp(&r, &sBootRes[id], sizeof r) == 0);
        } else {
            RT_CHECK(sameModel(f, sBootRes[src]));
            RT_CHECK_EQ((int)f.mHeapSize, (int)sBootField[id].mHeapSize);
            if (fix) {
                RT_CHECK(r.mArcname == sBootRes[src].mArcname);
                RT_CHECK(memcmp(&r.mBmdIdx, &sBootRes[src].mBmdIdx,
                                offsetof(dItem_data_item_resource, mTevFrm) + 1 -
                                    offsetof(dItem_data_item_resource, mBmdIdx)) == 0);
                RT_CHECK_EQ(r.mUnknown, sBootRes[src].mUnknown);
                RT_CHECK(r.mTexture == sBootRes[id].mTexture);
                RT_CHECK_EQ(r.mItemMesgNum, sBootRes[id].mItemMesgNum);
                RT_CHECK_EQ(r.mUnused, sBootRes[id].mUnused);
                RT_CHECK_EQ(r.mHeapSize, sBootRes[id].mHeapSize);
            } else {
                RT_CHECK(memcmp(&r, &sBootRes[id], sizeof r) == 0);
            }
        }
        const dItem_data_item_info& in = sInfo[id];
        const dItem_data_item_info& bi = sBootInfo[id];
        RT_CHECK_EQ((int)in.mCollisionH, bi.mCollisionH == 0 ? 0x28 : (int)bi.mCollisionH);
        RT_CHECK_EQ((int)in.mCollisionR, bi.mCollisionR == 0 ? 0x28 : (int)bi.mCollisionR);
        RT_CHECK_EQ(in.mShadowSize, bi.mShadowSize);
        RT_CHECK_EQ(in.mFlag, bi.mFlag);
    }
    // A source that is itself fixed is not one (0x38, 0x22, 0x52 have field models or are unlisted
    // sources): the model copied is the boot one either way.
    RT_CHECK(pc_rando_field_item_source(0x22, nullptr) == 0x22);
    RT_CHECK(sField[0x6D].mArc == sBootRes[0x22].mArcname && sRes[0xAA].mArcname == sBootRes[0x22].mArcname);

    pc_rando_deactivate();
    RT_CHECK_EQ(pc_rando(), 0);
    RT_CHECK(atBoot());

    // Registering again replaces the same keys.
    RT_CHECK_EQ(pc_rando_field_items_register(sRes, sField, sInfo), n);
    RT_CHECK_EQ(pc_rando_table_edit_count(), before + n);
    RT_CHECK(atBoot());
    return rt::finish();
}
