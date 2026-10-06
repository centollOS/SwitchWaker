// cos_pc_tests: host tests of the port helpers (phase 4, step 4.0b of docs/NATIVE_PORT_PHASE4_6.md).
//
//   ninja -C build/native-mac cos_pc_tests && build/native-mac/cos_pc_tests
//
// Compiled like a game unit (cos_game_headers), so it sees the same headers and flags as the game:
// native/include/helpers/{endian.h, endian_gx.hpp, endian_ssystem.h, offset_ptr.h}, and
// native/src/helpers/offset_ptr.cpp through cos_pc. It covers:
//   - BE(T) round trips and byte order for u16/s16/u32/s32/u64/f32, Vec, S16Vec, cXyz, csXyz,
//     GX enums and vertex-format lists; RES_* and be_swap; constant evaluation;
//   - the compound assignments and post-increment/decrement on BE fields;
//   - OffsetPtr::setBase: relocation, idempotence (a second call changes nothing), negative
//     offsets, the extremes of the range, and the panics on a null or out-of-range offset;
//     setBaseAllowZero, where an offset of 0 is the base (step 4.9a);
//   - JUtility::TColor's u32 form, 0xRRGGBBAA as GX and the disc have it (step 4.5);
//   - BMG data read in place: JMessage's header, block and INF1 accessors, the JMSMesgEntry_c
//     fields, and JGadget's TParseValue_endian_big_ for tag parameters (step 4.6).
// Prints "ok" and exits 0 when every check passes; otherwise prints each failed check and exits 1.
#include "helpers/endian.h"
#include "helpers/endian_gx.hpp"
#include "helpers/endian_ssystem.h"
#include "helpers/offset_ptr.h"
#include "JSystem/JGadget/binary.h"
#include "JSystem/JMessage/data.h"
#include "JSystem/JUtility/TColor.h"
#include "f_op/f_op_msg_mng.h"
#include "../src/pc/features/pc_precompile_gate.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <sys/wait.h>
#include <unistd.h>

namespace {

int g_failures = 0;

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expr);         \
            ++g_failures;                                                                          \
        }                                                                                          \
    } while (0)

// The object's bytes in memory order equal the big-endian bytes given.
template <typename T>
bool bytesAre(const T& obj, std::initializer_list<int> expect) {
    if (sizeof(T) != expect.size()) {
        return false;
    }
    const u8* p = reinterpret_cast<const u8*>(&obj);
    size_t i = 0;
    for (int b : expect) {
        if (p[i++] != (u8)b) {
            return false;
        }
    }
    return true;
}

// The object as read in place from big-endian file bytes.
template <typename T>
T fromBytes(std::initializer_list<int> bytes) {
    T obj;
    u8 raw[sizeof(T)] = {};
    size_t i = 0;
    for (int b : bytes) {
        raw[i++] = (u8)b;
    }
    std::memcpy(&obj, raw, sizeof(T));
    return obj;
}

// ---- BE(T) ------------------------------------------------------------------------------------

// Constant evaluation takes the be*_manual path; it must agree with the builtins.
static_assert(be16(0x1234) == 0x3412);
static_assert(be32(0x11223344u) == 0x44332211u);
static_assert(be64(0x0102030405060708ull) == 0x0807060504030201ull);
static_assert(be16s(-2) == (s16)0xFEFF);
static_assert(BE<u32>(0x11223344u).inner == 0x44332211u);
static_assert(BE<u32>(0x11223344u).host() == 0x11223344u);
static_assert(RES_F32(RES_F32(1.5f)) == 1.5f);

// Field sizes are the disc's: a BE(T) member never changes a struct's layout.
static_assert(sizeof(BE(u16)) == 2 && sizeof(BE(s16)) == 2);
static_assert(sizeof(BE(u32)) == 4 && sizeof(BE(s32)) == 4 && sizeof(BE(f32)) == 4);
static_assert(sizeof(BE(u64)) == 8 && sizeof(BE(s64)) == 8);
static_assert(sizeof(BE(Vec)) == 12 && sizeof(BE(cXyz)) == 12 && sizeof(BE(csXyz)) == 6);
static_assert(sizeof(BE(Mtx)) == 48 && sizeof(BE(Mtx44)) == 64);
static_assert(sizeof(BE(GXAttr)) == 4 && sizeof(BE(GXVtxDescList)) == 8);
static_assert(sizeof(OFFSET_PTR(u8)) == 4 && sizeof(OFFSET_PTR_RAW) == 4);
static_assert(alignof(BE(u32)) == 4 && alignof(BE(u16)) == 2);

void testScalars() {
    BE(u16) u16v = (u16)0x1234;
    CHECK(bytesAre(u16v, {0x12, 0x34}));
    CHECK((u16)u16v == 0x1234);
    CHECK(fromBytes<BE(u16)>({0xBE, 0xEF}) == 0xBEEF);

    BE(s16) s16v = (s16)-2;
    CHECK(bytesAre(s16v, {0xFF, 0xFE}));
    CHECK((s16)s16v == -2);
    CHECK(fromBytes<BE(s16)>({0x80, 0x00}) == -32768);

    BE(u32) u32v = 0xDEADBEEFu;
    CHECK(bytesAre(u32v, {0xDE, 0xAD, 0xBE, 0xEF}));
    CHECK((u32)u32v == 0xDEADBEEFu);
    CHECK(BE_HOST(u32v) == 0xDEADBEEFu);
    CHECK(fromBytes<BE(u32)>({0x52, 0x41, 0x52, 0x43}) == 'RARC');

    BE(s32) s32v = -100000;
    CHECK(bytesAre(s32v, {0xFF, 0xFE, 0x79, 0x60}));
    CHECK((s32)s32v == -100000);

    BE(u64) u64v = 0x0102030405060708ull;
    CHECK(bytesAre(u64v, {1, 2, 3, 4, 5, 6, 7, 8}));
    CHECK((u64)u64v == 0x0102030405060708ull);

    BE(f32) f32v = 1.0f;
    CHECK(bytesAre(f32v, {0x3F, 0x80, 0x00, 0x00}));
    f32v = -2.5f;
    CHECK(bytesAre(f32v, {0xC0, 0x20, 0x00, 0x00}));
    CHECK((f32)f32v == -2.5f);
    CHECK(fromBytes<BE(f32)>({0x42, 0xF6, 0xE9, 0x79}) == 123.456f);
    // NaN payloads and signed zero survive the round trip bit for bit.
    f32 nan = std::bit_cast<f32>(0x7FC12345u);
    BE(f32) nanv = nan;
    CHECK(std::bit_cast<u32>((f32)nanv) == 0x7FC12345u);
    BE(f32) negzero = -0.0f;
    CHECK(std::signbit((f32)negzero) && (f32)negzero == 0.0f);

    // Single values (RES_*), and in-place swaps of loaded words (be_swap).
    CHECK(RES_U16(0x3412) == 0x1234);
    CHECK(RES_U32(0x44332211u) == 0x11223344u);
    CHECK(RES_S32(RES_S32(-7)) == -7);
    u32 words[3] = {0x11223344u, 0xAABBCCDDu, 0};
    be_swap(words);
    CHECK(words[0] == 0x44332211u && words[1] == 0xDDCCBBAAu && words[2] == 0);
    s16 halves[2] = {1, -1};
    be_swap(halves, 2);
    CHECK(halves[0] == 0x0100 && halves[1] == -1);
    Mtx m;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            m[i][j] = (f32)(i * 4 + j);
        }
    }
    be_swap(m);
    CHECK(std::bit_cast<u32>(m[0][1]) == 0x0000803Fu);  // 1.0f stored big-endian
    be_swap(m);
    CHECK(m[2][3] == 11.0f);
}

void testCompound() {
    BE(u16) a = (u16)0x00F0;
    a |= 0x0F00;
    CHECK(a == 0x0FF0);
    a &= 0x0FF;
    CHECK(a == 0x00F0);
    a ^= 0xFFFF;
    CHECK(a == 0xFF0F);
    a += 0x00F1;
    CHECK(a == 0x0000);  // u16 wraps as on the console
    a -= 1;
    CHECK(a == 0xFFFF && bytesAre(a, {0xFF, 0xFF}));
    a /= 0x101;
    CHECK(a == 0x00FF);

    BE(s32) b = 10;
    CHECK(b++ == 10 && b == 11);
    CHECK(b-- == 11 && b == 10);
    b -= 25;
    CHECK(b == -15 && bytesAre(b, {0xFF, 0xFF, 0xFF, 0xF1}));
    b /= 4;
    CHECK(b == -3);

    BE(u32) c = 0xFFFFFFFFu;
    c += 2;
    CHECK(c == 1u && bytesAre(c, {0, 0, 0, 1}));

    BE(f32) f = 1.5f;
    f += 0.25f;
    CHECK(f == 1.75f);
    f -= 3.0f;
    CHECK(f == -1.25f);
    f /= 0.5f;
    CHECK(f == -2.5f && bytesAre(f, {0xC0, 0x20, 0x00, 0x00}));
}

void testVectors() {
    Vec hv = {1.0f, -2.0f, 0.5f};
    BE(Vec) v = hv;
    CHECK(bytesAre(v, {0x3F, 0x80, 0, 0, 0xC0, 0x00, 0, 0, 0x3F, 0x00, 0, 0}));
    Vec back = v;
    CHECK(back.x == 1.0f && back.y == -2.0f && back.z == 0.5f);
    BE(Vec) v2(3.0f, 4.0f, 5.0f);
    CHECK(v2.y == 4.0f);
    Vec sw = BE<Vec>::swap(BE<Vec>::swap(hv));
    CHECK(sw.x == hv.x && sw.y == hv.y && sw.z == hv.z);

    S16Vec sv = {1, -1, 0x1234};
    S16Vec svs = BE<S16Vec>::swap(sv);
    CHECK(svs.x == 0x0100 && svs.y == -1 && svs.z == 0x3412);

    BE(cXyz) cv;
    cv = cXyz(10.0f, -20.0f, 30.0f);
    CHECK(bytesAre(cv, {0x41, 0x20, 0, 0, 0xC1, 0xA0, 0, 0, 0x41, 0xF0, 0, 0}));
    cXyz cback = cv;
    CHECK(cback.x == 10.0f && cback.y == -20.0f && cback.z == 30.0f);
    cv.y += 5.0f;
    CHECK(cv.y == -15.0f);

    BE(csXyz) sxyz;
    sxyz.set(0x4000, -0x4000, 7);
    CHECK(bytesAre(sxyz, {0x40, 0x00, 0xC0, 0x00, 0x00, 0x07}));
    CHECK(sxyz.x == 0x4000 && sxyz.y == -0x4000 && sxyz.z == 7);
    csXyz sback = sxyz;
    CHECK(sback.x == 0x4000 && sback.y == -0x4000 && sback.z == 7);
    BE(csXyz) sxyz2;
    sxyz2 = sback;
    CHECK(bytesAre(sxyz2, {0x40, 0x00, 0xC0, 0x00, 0x00, 0x07}));
    csXyz plain;
    plain.set(1, 2, 3);
    csXyz swapped = BE<csXyz>::swap(plain);
    CHECK(swapped.x == 0x0100 && swapped.y == 0x0200 && swapped.z == 0x0300);

    cXy xy = {1.0f, 2.0f};
    cXy xys = BE<cXy>::swap(BE<cXy>::swap(xy));
    CHECK(xys.x == 1.0f && xys.y == 2.0f);

    // A J3D-style matrix read in place.
    BE(Mtx) bm;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            bm.contents[i][j] = (f32)(i * 4 + j);
        }
    }
    Mtx hm;
    bm.to_host(hm);
    CHECK(hm[1][2] == 6.0f && hm[2][3] == 11.0f);
    CHECK(bytesAre(bm.contents[0][1], {0x3F, 0x80, 0, 0}));
}

void testGX() {
    BE(GXAttr) attr = GX_VA_TEX0;
    CHECK(bytesAre(attr, {0, 0, 0, (int)GX_VA_TEX0}));
    CHECK((GXAttr)attr == GX_VA_TEX0);

    // A J3D VTX1 descriptor list entry as stored in a BMD.
    auto desc = fromBytes<BE(GXVtxDescList)>({0, 0, 0, (int)GX_VA_POS, 0, 0, 0, (int)GX_INDEX16});
    CHECK(desc.attr == GX_VA_POS && desc.type == GX_INDEX16);
    GXVtxDescList hd = {GX_VA_NRM, GX_DIRECT};
    GXVtxDescList hds = BE<GXVtxDescList>::swap(BE<GXVtxDescList>::swap(hd));
    CHECK(hds.attr == GX_VA_NRM && hds.type == GX_DIRECT);

    GXVtxAttrFmtList fmt = {GX_VA_POS, GX_POS_XYZ, GX_S16, 7};
    GXVtxAttrFmtList fmts = BE<GXVtxAttrFmtList>::swap(fmt);
    CHECK(fmts.frac == 7 && std::bit_cast<u32>(fmts.attr) == RES_U32((u32)GX_VA_POS));
    GXVtxAttrFmtList fmtb = BE<GXVtxAttrFmtList>::swap(fmts);
    CHECK(fmtb.attr == GX_VA_POS && fmtb.cnt == GX_POS_XYZ && fmtb.type == GX_S16);

    GXColorS10 col = {-1, 0x100, 2, 0x3FF};
    GXColorS10 cols = BE<GXColorS10>::swap(col);
    CHECK(cols.r == -1 && cols.g == 1 && cols.b == 0x0200 && cols.a == (s16)0xFF03);
}

// ---- OffsetPtr --------------------------------------------------------------------------------

// A chunk table as the stage and collision files lay it out: big-endian offsets from the start
// of the file, relocated in place.
struct TestChunk {
    BE(u32) tag;
    BE(u32) count;
    OFFSET_PTR(u8) data;
    OFFSET_PTR_RAW raw;
};
static_assert(sizeof(TestChunk) == 16, "OFFSET_PTR keeps the 4-byte disc field");

void setRaw(OffsetPtr& p, s32 v) {
    p.value = v;
}

void testOffsetPtr() {
    alignas(32) static u8 file[0x200];
    std::memset(file, 0, sizeof(file));
    // Chunk header at 0x100, its data at 0x40 (before it: negative relative offset) and its raw
    // offset at 0x180 (after it: positive).
    auto* chunk = reinterpret_cast<TestChunk*>(file + 0x100);
    std::memcpy(file + 0x100, "\x52\x54\x42\x4C\x00\x00\x00\x02\x00\x00\x00\x40\x00\x00\x01\x80",
                16);
    file[0x40] = 0xAB;
    file[0x180] = 0xCD;
    CHECK(chunk->tag == 'RTBL' && chunk->count == 2);

    CHECK(!chunk->data.isRelocated());
    CHECK(chunk->data.setBase(file));
    CHECK(chunk->data.isRelocated());
    u8* data = chunk->data;
    CHECK(data == file + 0x40);
    CHECK(*data == 0xAB);
    CHECK(chunk->data.operator->() == file + 0x40);

    // Idempotent: a second relocation (another actor sharing the resource) changes nothing.
    u32 before = std::bit_cast<u32>(chunk->data.value.value.inner);
    CHECK(!chunk->data.setBase(file));
    CHECK(!chunk->data.setBase(file + 0x10));
    CHECK(std::bit_cast<u32>(chunk->data.value.value.inner) == before);
    CHECK((u8*)chunk->data == file + 0x40);

    CHECK(chunk->raw.setBase(file));
    CHECK(!chunk->raw.setBase(file));
    CHECK((u8*)chunk->raw == file + 0x180 && *(u8*)chunk->raw == 0xCD);
    u16* asU16 = (u16*)chunk->raw;
    CHECK((u8*)asU16 == file + 0x180);

    // A file offset that points at the field itself: relative offset 0.
    OffsetPtr self;
    setRaw(self, 0x20);
    CHECK(self.setBase((u8*)&self - 0x20));
    CHECK((u8*)(u8*)self == (u8*)&self);

    // Range limits (the base is never dereferenced, so it may lie anywhere). The largest positive
    // relative offset is 0x3FFF'FFFF (bit 30 is the sign once bit 31 is the flag), the most
    // negative -0x4000'0000.
    OffsetPtr far;
    setRaw(far, 0x3FFF'FFFF + 0x100);
    CHECK(far.setBase((u8*)&far - 0x100));
    CHECK((uintptr_t)(u8*)far == (uintptr_t)&far + 0x3FFF'FFFFu);
    OffsetPtr back;
    setRaw(back, 0x10);
    CHECK(back.setBase((u8*)&back - (0x4000'0000 + 0x10)));
    CHECK((uintptr_t)(u8*)back == (uintptr_t)&back - 0x4000'0000u);
    OffsetPtr minusOne;
    setRaw(minusOne, 0x10);
    CHECK(minusOne.setBase((u8*)&minusOne - 0x11));
    CHECK((uintptr_t)(u8*)minusOne == (uintptr_t)&minusOne - 1);

    // setBaseAllowZero (step 4.9a): an offset of 0 is the base itself (a path's first point at
    // the start of its PPNT entries), relocated once like any other.
    // The fields and the base in one object (the range is +-1 GiB, the stack is farther away).
    static struct {
        OFFSET_PTR(u8) first;
        OFFSET_PTR(u8) second;
        u8 points[0x40];
    } path;
    u8* points = path.points;
    auto& first = path.first;
    auto& second = path.second;
    setRaw(first.value, 0);
    CHECK(first.setBaseAllowZero(points));
    CHECK(first.isRelocated());
    CHECK((u8*)first == points);
    CHECK(!first.setBaseAllowZero(points + 0x10));
    CHECK((u8*)first == points);
    setRaw(second.value, 0x10);
    CHECK(second.setBaseAllowZero(points));
    CHECK((u8*)second == points + 0x10);
}

// Runs fn in a child process; true if the child ended abnormally (OSPanic aborts).
bool panics(void (*fn)()) {
    std::fflush(nullptr);
    pid_t pid = fork();
    if (pid == 0) {
        // The expected panic message would only clutter the output.
        std::freopen("/dev/null", "w", stderr);
        fn();
        _exit(0);
    }
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) != pid) {
        return false;
    }
    return !(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

void testOffsetPtrPanics() {
    // A null offset (the original relocation code treated it as "no data").
    CHECK(panics([] {
        OffsetPtr p;
        setRaw(p, 0);
        p.setBase(&p);
    }));
    // Relative offset 0x4000'0000 would read back as negative (the bit-30 sign).
    CHECK(panics([] {
        OffsetPtr p;
        setRaw(p, 0x4000'0000 + 0x100);
        p.setBase((u8*)&p - 0x100);
    }));
    // One below the most negative.
    CHECK(panics([] {
        OffsetPtr p;
        setRaw(p, 0x10);
        p.setBase((u8*)&p - (0x4000'0000 + 0x11));
    }));
    // The fork path itself works (a call in range does not panic).
    CHECK(!panics([] {
        OffsetPtr p;
        setRaw(p, 0x10);
        p.setBase(&p);
    }));
}

// TColor's u32 form is 0xRRGGBBAA (GXColor1u32, GXSetTevColor's packed form, BLO colours read
// with JSUInputStream::readU32): r is its high byte whatever the host's byte order (step 4.5).
void testTColor() {
    JUtility::TColor c(0x11223344u);
    CHECK(c.r == 0x11 && c.g == 0x22 && c.b == 0x33 && c.a == 0x44);
    CHECK(c.toUInt32() == 0x11223344u);
    CHECK((u32)c == 0x11223344u);
    JUtility::TColor d(0xDC, 0x00, 0x00, 0x80);
    CHECK(d.toUInt32() == 0xDC000080u);
    d.set(0x000000FFu);
    CHECK(d.r == 0 && d.a == 0xFF);
    BE(u32) be = 0xAABBCCDDu;
    JUtility::TColor e(be);
    CHECK(e.r == 0xAA && e.a == 0xDD);
    CHECK(bytesAre(c, {0x11, 0x22, 0x33, 0x44}));
}

// A BMG as the disc stores it, big-endian, read in place (step 4.6).
void testBmg() {
    alignas(4) static const u8 file[] = {
        'M', 'E', 'S', 'G', 'b', 'm', 'g', '1', 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02,
        0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // INF1: size 0x28, 1 entry of 0x18 bytes, group 1
        'I', 'N', 'F', '1', 0x00, 0x00, 0x00, 0x28, 0x00, 0x01, 0x00, 0x18, 0x00, 0x01, 0x00, 0x00,
        // entry: offset 0x01020304, number 0x0D49, price -2, next 0x0102, 0x0304, bytes 1..12
        0x01, 0x02, 0x03, 0x04, 0x0D, 0x49, 0xFF, 0xFE, 0x01, 0x02, 0x03, 0x04,
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
        // DAT1: size 0x10
        'D', 'A', 'T', '1', 0x00, 0x00, 0x00, 0x10, 0x1A, 0x06, 0xFF, 0x00, 0x05, 0x00, 0x00, 0x00,
    };
    JMessage::data::TParse_THeader header(file);
    CHECK(header.get_type() == 'bmg1');
    CHECK(header.get_blockNumber() == 2);
    CHECK(header.get_encoding() == 1);
    JMessage::data::TParse_TBlock_info info(header.getContent());
    CHECK(info.get_type() == 'INF1');
    CHECK(info.get_size() == 0x28);
    CHECK(info.get_messageEntryNumber() == 1);
    CHECK(info.get_messageEntrySize() == 0x18);
    CHECK(info.get_groupID() == 1);
    JMessage::data::TParse_TBlock dat(info.getNext());
    CHECK(dat.get_type() == 'DAT1');
    CHECK(dat.get_size() == 0x10);
    const JMSMesgEntry_c* e = (const JMSMesgEntry_c*)info.getContent();
    CHECK(sizeof(JMSMesgEntry_c) == 0x18);
    CHECK(e->mDataOffs == 0x01020304u);
    CHECK(e->mMsgNo == 0x0D49);
    CHECK(e->mItemPrice == -2);
    CHECK(e->mNextMsgNo == 0x0102 && e->field_0x0a == 0x0304);
    CHECK(e->mTextboxType == 1 && e->field_0x17 == 12);
    JMSMesgEntry_c copy = *e; // what getMesgEntry returns: still the file's bytes
    CHECK(copy.mMsgNo == 0x0D49 && bytesAre(copy.mDataOffs, {0x01, 0x02, 0x03, 0x04}));
    // Tag values are big-endian too: the code u16 after 0x1A, length, group (JMessage reads tag
    // parameters, such as system tag 5's u32 message code, with TParseValue_endian_big_).
    const u8* code = file + 0x20 + 0x28 + 8 + 3;
    using namespace JGadget::binary;
    CHECK(TParseValue<TParseValue_endian_big_<u16> >::parse(code) == 5);
    CHECK(TParseValue<TParseValue_endian_big_<u32> >::parse(file + 8) == 3);
}


// ---- the warm-up's late loading screen (pc_precompile_gate.h) ----------------------------------

void testLateGate() {
    pc::LateGateConfig cfg; // 16 builds, 8 slow, 1.5 s, COS_PRECOMPILE_SCREEN_MIN_S 3
    // The warm boot seen on the console every time: the first frame's 3 builds took 98 ms each
    // (all shader cache hits that waited for the GL context), then everything is fast. The old rule
    // (3 slow, 1121 left at 98 ms: 110 s) brought the screen up; now the sample is too small.
    {
        pc::LateGate gate(cfg);
        pc::LateGateVerdict v = gate.add(3, 3, 0.295, 1121);
        CHECK(!v.screen && !v.sampleEnough);
        for (int frame = 0; frame < 120; frame++) {
            v = gate.add(4, 0, 0.0, 1100 - frame * 4);
            CHECK(!v.screen);
        }
        CHECK(v.windowSlow == 0 && v.leftMs == 0.0);
    }
    // A cold (or wiped) shader cache behind the logos, throttled: one 150 ms build a frame. Below
    // 8 slow builds and 1.5 s nothing; the 10th build (1.5 s measured) brings the screen up, with
    // every pipeline left taken as slow (all of the recent builds were).
    {
        pc::LateGate gate(cfg);
        pc::LateGateVerdict v;
        int shownAt = -1;
        for (int i = 1; i <= 20 && shownAt < 0; i++) {
            v = gate.add(1, 1, 0.150, 1000 - i);
            if (v.screen) {
                shownAt = i;
            }
        }
        CHECK(shownAt == 10);
        CHECK(v.slowShare == 1.0 && v.slowEachMs > 149.0 && v.slowEachMs < 151.0);
        CHECK(v.leftMs > 990 * 149.0 && v.leftMs < 990 * 151.0);
    }
    // Half the recent builds slow at 100 ms, 1000 left: 16 builds with 8 slow is enough sample, and
    // the estimate is the hit ratio's: 1000 * 0.5 * 100 ms = 50 s, not 1000 * 100 ms.
    {
        pc::LateGate gate(cfg);
        pc::LateGateVerdict v;
        for (int i = 0; i < 8; i++) {
            v = gate.add(2, 1, 0.100, 1000);
        }
        CHECK(v.windowBuilds == 16 && v.windowSlow == 8 && v.sampleEnough && v.screen);
        CHECK(v.leftMs > 49000.0 && v.leftMs < 51000.0);
    }
    // Sparse misses (2 of 16 slow, 0.2 s measured): built behind the logos, no screen.
    {
        pc::LateGate gate(cfg);
        pc::LateGateVerdict v;
        for (int i = 0; i < 16; i++) {
            v = gate.add(1, (i % 8) == 0 ? 1 : 0, 0.100, 900);
        }
        CHECK(!v.sampleEnough && !v.screen);
    }
    // Enough sample but little left: 8 of 16 slow at 100 ms with 40 left is 2 s of slow work, under
    // COS_PRECOMPILE_SCREEN_MIN_S.
    {
        pc::LateGate gate(cfg);
        pc::LateGateVerdict v;
        for (int i = 0; i < 8; i++) {
            v = gate.add(2, 1, 0.100, 40);
        }
        CHECK(v.sampleEnough && !v.screen && v.leftMs < 3000.0);
    }
    // The window follows the recent builds: an early slow burst followed by 16 fast builds no
    // longer counts (the measured time keeps growing only with slow builds).
    {
        pc::LateGate gate(cfg);
        pc::LateGateVerdict v = gate.add(6, 6, 0.6, 1000);
        CHECK(!v.screen);
        for (int i = 0; i < 16; i++) {
            v = gate.add(1, 0, 0.0, 1000);
        }
        CHECK(v.windowSlow == 0 && !v.screen);
        CHECK(v.measuredSlowS > 0.59 && v.measuredSlowS < 0.61);
    }
}

} // namespace

int main() {
    testScalars();
    testCompound();
    testVectors();
    testGX();
    testOffsetPtr();
    testOffsetPtrPanics();
    testTColor();
    testBmg();
    testLateGate();
    if (g_failures != 0) {
        std::printf("FAIL: %d checks\n", g_failures);
        return 1;
    }
    std::printf("ok\n");
    return 0;
}
