// Audio data formats on the host (docs/NATIVE_PORT_PHASE4_6.md, step 5.1): the COS_SMOKE=
// audio-parse test.
//
// Runs from pc_heaps_created (main01, right after mDoMch_Create), then exits. It brings up the part
// of JAudio that reads the audio data, as mDoAud_Create and JAIZelBasic::init do, without the audio
// thread, the DVD thread or the DSP (step 5.A):
// - a solid heap stands for g_mDoAud_audioHeap; JASystem::Kernel::sysDramSetup and sysAramSetup
//   take it and 0xa00000 bytes of ARAM, as TAudioThread::start does;
// - the JAIGlobalParameter values JAIZelBasic::init sets for the paths and the interface heap;
// - /Audiores/JaiInit.aaf loaded into main RAM and handed over with setParamInitDataPointer, as
//   mDoAud_Create does with l_affCommand;
// - then JAIBasic's own initHeap, initResourcePath, initArchive (the JaiSeqs.arc mount) and
//   initReadFile, which runs JAInter::InitData::checkInitDataOnMemory: the sound table
//   (SoundTable::init), the bank and wave-system lists, the stream table, the scene table, the fx
//   scene table, and BankWave::init, which parses every wave system (WSParser, wave banks and
//   groups with their .aw files) and every bank (BNKParser: instruments, oscillators, key and
//   velocity regions, drum sets).
// What the game built is written to <COS_RUN_DIR>/audio_parse.txt, one line per record, read back
// through the game's own structures (SoundTable, BankMgr, WaveBankMgr, the stream list,
// JAIBasic::field_0x1c, Fx::initOnCodeFxScene), and the sequences are looked up in the archive as
// JAISequenceMgr does (getSoundOffsetNumberFromID, ResArcLoader::getResSize). Every stream's .afc
// header is also read from the disc through StreamLib's StreamHeader. native/tools/run.sh then
// compares the file with the disc manifest (disc_manifest.py --check-audio), which decodes the
// same records from the disc independently: the bank, wave and sequence counts and every field.
// The test itself checks that every count is non-zero, that each wave lies inside its .aw file
// and that each stream's table header equals its file's.
#include "pc_internal.h"

#include "JAZelAudio/JAIZelParam.h"
#include "JSystem/JAudio/JAIBankWave.h"
#include "JSystem/JAudio/JAIBasic.h"
#include "JSystem/JAudio/JAIFx.h"
#include "JSystem/JAudio/JAIGlobalParameter.h"
#include "JSystem/JAudio/JAIInitData.h"
#include "JSystem/JAudio/JAISequenceMgr.h"
#include "JSystem/JAudio/JAISound.h"
#include "JSystem/JAudio/JAISoundTable.h"
#include "JSystem/JAudio/JAIStreamMgr.h"
#include "JSystem/JAudio/JASBankMgr.h"
#include "JSystem/JAudio/JASBasicBank.h"
#include "JSystem/JAudio/JASBasicInst.h"
#include "JSystem/JAudio/JASBasicWaveBank.h"
#include "JSystem/JAudio/JASDSPInterface.h"
#include "JSystem/JAudio/JASDrumSet.h"
#include "JSystem/JAudio/JASResArcLoader.h"
#include "JSystem/JAudio/JASSimpleWaveBank.h"
#include "JSystem/JAudio/JASSystemHeap.h"
#include "JSystem/JAudio/JASWSParser.h"
#include "JSystem/JAudio/JASWaveBankMgr.h"
#include "JSystem/JKernel/JKRDvdRipper.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRSolidHeap.h"

#include <dolphin/dvd.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace pc {

namespace {

// Stands for g_mDoAud_audioHeap (0x166800 on the GameCube; step 5.2 scales the real one).
constexpr uint32_t kAudioHeapSize = 16 * 1024 * 1024;
constexpr uint32_t kAramSize = 0x00a00000; // mDoAud_Create's g_mDoAud_zelAudio.init argument
constexpr const char* kInitData = "/Audiores/JaiInit.aaf";

int sErrors = 0;

void fail(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void fail(const char* fmt, ...) {
    if (sErrors < 60) {
        char text[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(text, sizeof(text), fmt, ap);
        va_end(ap);
        writef(STDERR_FILENO, "[cos] audio-parse: %s\n", text);
    }
    sErrors++;
}

int sFd = -1;

void line(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void line(const char* fmt, ...) {
    if (sFd < 0) {
        return;
    }
    char text[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    writef(sFd, "%s\n", text);
}

// FNV-1a 64 (disc_manifest.py's fnv1a64) of host s16 values taken as their big-endian bytes.
uint64_t fnvS16(const s16* p, int n) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (int i = 0; i < n; i++) {
        uint8_t b[2] = {(uint8_t)((uint16_t)p[i] >> 8), (uint8_t)p[i]};
        for (uint8_t c : b) {
            h ^= c;
            h *= 0x100000001B3ull;
        }
    }
    return h;
}

// The length (in s16) of an oscillator table the bank parser copied: triplets up to and including
// the first whose mode is above 0xa (BNKParser::getOscTableEndPtr, over the host copy).
int oscTableLength(const s16* table) {
    int n = 0;
    s16 mode;
    do {
        mode = table[n];
        n += 3;
    } while (mode <= 0xa && n < 3 * 4096);
    return n;
}

struct Totals {
    int sounds = 0, banks = 0, insts = 0, percs = 0, oscs = 0, velos = 0, waveSystems = 0,
        waveGroups = 0, waves = 0, sequences = 0, streams = 0, scenes = 0, fxLines = 0;
} sTotals;

void reportSoundTable() {
    using namespace JAInter;
    if (SoundTable::mAddress == NULL) {
        fail("no sound table");
        return;
    }
    line("STBL formats %u %u %u version %u categories %u", SoundTable::mAddress[0],
         SoundTable::mAddress[1], SoundTable::mAddress[2], SoundTable::mVersion,
         SoundTable::mCategotyMax);
    SoundInfo* base = (SoundInfo*)&SoundTable::mAddress[0x50];
    for (int c = 0; c < 18; c++) {
        line("CAT %d %u %ld", c, SoundTable::mSoundMax[c], (long)(SoundTable::mPointerCategory[c] - base));
        for (int i = 0; i < SoundTable::mSoundMax[c]; i++) {
            const SoundInfo& s = SoundTable::mPointerCategory[c][i];
            line("SND %d %d %08x %u %u %u %.9g %08x", c, i, (u32)s.mFlag, s.mPriority, s._05,
                 (u16)s.mOffsetNo, (double)(f32)s.mPitch, (u32)s.mVolume.typeView);
            sTotals.sounds++;
        }
    }
}

int waveBankIndex(const JASystem::TWaveBank* bank) {
    for (int i = 0; i < JASystem::WaveBankMgr::sTableSize; i++) {
        if (bank != NULL && JASystem::WaveBankMgr::sWaveBank[i] == bank) {
            return i;
        }
    }
    return -1;
}

void reportOsc(int bank, int slot, int j, const JASystem::TOscillator::Osc_* osc) {
    int len = osc->table != NULL ? oscTableLength(osc->table) : 0;
    int rel = osc->rel_table != NULL ? oscTableLength(osc->rel_table) : 0;
    line("OSC %d %d %d %u %.9g %d %016llx %d %016llx %.9g %.9g", bank, slot, j, osc->field_0x0,
         (double)osc->field_0x4, len,
         (unsigned long long)(osc->table != NULL ? fnvS16(osc->table, len) : 0), rel,
         (unsigned long long)(osc->rel_table != NULL ? fnvS16(osc->rel_table, rel) : 0),
         (double)osc->field_0x10, (double)osc->field_0x14);
    sTotals.oscs++;
}

void reportInst(int b, int slot, const JASystem::TBasicInst* inst) {
    int effects = 0;
    for (u32 e = 0; e < inst->mEffectCount; e++) {
        effects += inst->mEffect[e] != NULL;
    }
    int oscs = 0;
    for (u32 o = 0; o < inst->mOscCount; o++) {
        oscs += inst->mOsc[o] != NULL;
    }
    line("INST %d %d %.9g %.9g osc=%d effects=%d keys=%u", b, slot, (double)inst->mVolume,
         (double)inst->mPitch, oscs, effects, inst->mKeyRegionCount);
    for (u32 o = 0; o < inst->mOscCount; o++) {
        if (inst->mOsc[o] != NULL) {
            reportOsc(b, slot, o, inst->mOsc[o]);
        }
    }
    for (u32 k = 0; k < inst->mKeyRegionCount; k++) {
        const JASystem::TBasicInst::TKeymap& key = inst->mKeymap[k];
        line("KEY %d %d %u %d %u", b, slot, k, key.mBaseKey, key.getVeloRegionCount());
        for (u32 v = 0; v < key.getVeloRegionCount(); v++) {
            const JASystem::TBasicInst::TVeloRegion* r = key.getVeloRegion(v);
            line("VEL %d %d %u %u %d %d %.9g %.9g", b, slot, k, v, r->mBaseVel, r->field_0x04,
                 (double)r->field_0x08, (double)r->field_0x0c);
            sTotals.velos++;
        }
    }
    sTotals.insts++;
}

// A drum set through TDrumSet::getParam at velocity 127, the only public view of its percussion
// entries: volume and pitch (the entry's times its velocity region's), pan, release and wave.
void reportDrumSet(int b, int set, const JASystem::TDrumSet* drums) {
    line("DRUMS %d %d", b, set);
    for (int key = 0; key < (int)JASystem::TDrumSet::sPercCount; key++) {
        JASystem::TInstParam p;
        if (!drums->getParam(key, 127, &p)) {
            continue;
        }
        line("PERC %d %d %d %.9g %.9g %.9g %u %d", b, set, key, (double)p.field_0x10,
             (double)p.field_0x14, (double)p.field_0x20, p.field_0x3a, p.field_0x4);
        sTotals.percs++;
    }
}

void reportBanks() {
    using namespace JAInter;
    if (BankWave::initOnCodeBnk == NULL) {
        fail("no bank list");
        return;
    }
    for (int vir = 0; vir < JASystem::BankMgr::sTableSize; vir++) {
        if (JASystem::BankMgr::sVir2PhyTable[vir] != 0xFFFF) {
            line("VIR %d %u", vir, JASystem::BankMgr::getPhysicalNumber(vir));
        }
    }
    for (int b = 0; BankWave::initOnCodeBnk[b].field_0x0 != NULL; b++) {
        const BankWave::initOnCode_s& entry = BankWave::initOnCodeBnk[b];
        JASystem::TBank* bank = JASystem::BankMgr::getBank(b);
        if (bank == NULL || bank->getType() != 'BSIC') {
            fail("bank %d: BankMgr has no basic bank", b);
            continue;
        }
        JASystem::TBasicBank* basic = (JASystem::TBasicBank*)bank;
        int insts = 0, sets = 0;
        for (u32 i = 0; i < basic->mInstCount; i++) {
            JASystem::TInst* inst = basic->getInst(i);
            if (inst == NULL) {
                continue;
            }
            if (inst->getType() == 'PERC') {
                sets++;
            } else {
                insts++;
            }
        }
        line("BNK %d size %d wavebank %u assigned %d insts %d drumsets %d", b, entry.field_0x4,
             entry.field_0x8, waveBankIndex(bank->getWaveBank()), insts, sets);
        for (u32 i = 0; i < basic->mInstCount; i++) {
            JASystem::TInst* inst = basic->getInst(i);
            if (inst == NULL) {
                continue;
            }
            if (inst->getType() == 'PERC') {
                reportDrumSet(b, i, (JASystem::TDrumSet*)inst);
            } else if (inst->getType() == 'BSIC') {
                reportInst(b, i, (JASystem::TBasicInst*)inst);
            } else {
                fail("bank %d inst %u: type %08x", b, i, inst->getType());
            }
        }
        sTotals.banks++;
    }
}

void reportWaveInfo(int ws, int g, int idx, u32 id, const JASystem::TWaveInfo& w, int arcSize) {
    line("WAVE %d %d %d %u %u %u %u %.9g %d %d %d %d %d %d %d %d %d", ws, g, idx, id, w.mBlockType,
         w.field_0x1, w.field_0x2, (double)w.field_0x4, w.mWavePtrOffs, w.field_0xc, w.field_0x10,
         w.mBlockCount, w.field_0x18, w.field_0x1c, w.field_0x20, w.field_0x22, w.field_0x28);
    if (w.mWavePtrOffs < 0 || w.field_0xc < 0 || (int64_t)w.mWavePtrOffs + w.field_0xc > arcSize) {
        fail("wave system %d group %d wave %d: %d+%d bytes is outside its .aw (%d bytes)", ws, g,
             idx, w.mWavePtrOffs, w.field_0xc, arcSize);
    }
    sTotals.waves++;
}

void reportWaveSystems() {
    using namespace JAInter;
    if (BankWave::initOnCodeWs == NULL) {
        fail("no wave-system list");
        return;
    }
    for (int i = 0; BankWave::initOnCodeWs[i].field_0x0 != NULL; i++) {
        const BankWave::initOnCode_s& entry = BankWave::initOnCodeWs[i];
        JASystem::TWaveBank* bank = JASystem::WaveBankMgr::getWaveBank(i);
        if (bank == NULL) {
            fail("wave system %d: WaveBankMgr has no bank", i);
            continue;
        }
        u32 groups = JASystem::WSParser::getGroupCount(entry.field_0x0);
        if (groups == 1) {
            JASystem::TSimpleWaveBank* simple = (JASystem::TSimpleWaveBank*)bank;
            line("WS %d size %d mode %u simple groups 1 table %u", i, entry.field_0x4, entry.field_0x8,
                 simple->mWaveCount);
            line("WGRP %d 0 %d %d", i, simple->mEntryNum, simple->mSize);
            if (simple->mEntryNum < 0) {
                fail("wave system %d: its .aw is not on the disc", i);
            }
            for (u32 id = 0; id < simple->mWaveCount; id++) {
                const JASystem::TSimpleWaveBank::TWaveHandle& h = simple->mWaveTable[id];
                if (h.mHeap != NULL) {
                    reportWaveInfo(i, 0, id, id, h.mWaveInfo, simple->mSize);
                }
            }
            sTotals.waveGroups++;
        } else {
            JASystem::TBasicWaveBank* basic = (JASystem::TBasicWaveBank*)bank;
            line("WS %d size %d mode %u basic groups %u table %d", i, entry.field_0x4, entry.field_0x8,
                 basic->mWaveGroupCount, basic->mWaveCount);
            for (u32 g = 0; g < basic->mWaveGroupCount; g++) {
                JASystem::TBasicWaveBank::TWaveGroup* group = basic->getWaveGroup(g);
                line("WGRP %d %u %d %d", i, g, group->mEntryNum, group->mSize);
                if (group->mEntryNum < 0) {
                    fail("wave system %d group %u: its .aw is not on the disc", i, g);
                }
                for (u32 w = 0; w < group->getWaveCount(); w++) {
                    reportWaveInfo(i, g, w, group->getWaveID(w),
                                   group->mCtrlWaveArray[w].mWaveHandle.mWaveInfo, group->mSize);
                }
                sTotals.waveGroups++;
            }
        }
        sTotals.waveSystems++;
    }
    if (sTotals.waveSystems != JAInter::BankWave::wsMax) {
        fail("%d wave systems listed, wsMax %d", sTotals.waveSystems, (int)JAInter::BankWave::wsMax);
    }
}

void reportSequences() {
    using namespace JAInter;
    if (SequenceMgr::arcPointer == NULL) {
        fail("JaiSeqs.arc is not mounted");
        return;
    }
    for (int n = 0; n < JAInter::SoundTable::mSoundMax[16]; n++) {
        u32 id = JAISoundID_Type_Sequence | n;
        u16 res = JAIBasic::msBasic->getSoundOffsetNumberFromID(id);
        u32 size = JASystem::ResArcLoader::getResSize(SequenceMgr::arcPointer, res);
        line("SEQ %d %u %u", n, res, size);
        if (size == 0) {
            fail("sequence %d: resource %u is not in JaiSeqs.arc", n, res);
        }
        sTotals.sequences++;
    }
}

void headerLine(char* out, size_t n, const JAInter::StreamLib::StreamHeader& h) {
    snprintf(out, n, "%d %d %u %u %u %u", (int)h.field_0x0, (int)h.field_0x4, (u16)h.field_0x8,
             (u16)h.field_0xa, (u16)h.field_0xc, (u16)h.field_0xe);
}

void reportStreams() {
    using namespace JAInter;
    if (StreamMgr::streamList == NULL) {
        fail("no stream table");
        return;
    }
    // StreamLib::start takes the table entry's header (Head) or, without one, the file's first 32
    // bytes; both go through StreamHeader.
    static u8 head[32] __attribute__((aligned(32)));
    for (int n = 0; n < JAInter::SoundTable::mSoundMax[17]; n++) {
        const streamList_t& s = StreamMgr::streamList[n];
        char name[sizeof(s.field_0x10) + 1];
        memcpy(name, s.field_0x10, sizeof(s.field_0x10));
        name[sizeof(s.field_0x10)] = '\0';
        char table[96];
        headerLine(table, sizeof(table), *(const StreamLib::StreamHeader*)s.field_0x20);
        char path[128];
        snprintf(path, sizeof(path), "%s%s", JAIGlobalParameter::getParamStreamPath(), name);
        DVDFileInfo info;
        int entry = DVDConvertPathToEntrynum(path);
        char file[96] = "-";
        int size = -1;
        if (entry >= 0 && DVDFastOpen(entry, &info)) {
            size = (int)info.length;
            if (DVDReadPrio(&info, head, sizeof(head), 0, 2) == (s32)sizeof(head)) {
                headerLine(file, sizeof(file), *(const StreamLib::StreamHeader*)head);
            }
            DVDClose(&info);
        }
        line("STRM %d %s %d %d %s afc %s", n, name, entry, size, table, file);
        if (strcmp(table, file) != 0) {
            fail("stream %d %s: table header %s, file header %s", n, name, table, file);
        }
        sTotals.streams++;
    }
}

void reportScenes() {
    u8** scenes = JAIBasic::getInterface()->field_0x1c;
    if (scenes == NULL) {
        fail("no scene table");
        return;
    }
    for (u32 i = 0; i < JAIGlobalParameter::getParamSoundSceneMax(); i++) {
        char hex[2 * 16 + 1];
        for (int j = 0; j < 16; j++) {
            snprintf(hex + 2 * j, 3, "%02x", scenes[i][j]);
        }
        line("SCENE %u %s", i, hex);
        sTotals.scenes++;
    }
}

void reportFx() {
    using namespace JAInter;
    Fx::initOnCodeFxScene_s* fx = Fx::initOnCodeFxScene;
    if (fx == NULL) {
        fail("no fx scene table");
        return;
    }
    line("FX %u %u %u %u %u", (u32)fx->field_0x0, (u32)fx->field_0x4, (u32)fx->field_0x8,
         (u32)fx->field_0xc, (u32)fx->field_0x10);
    for (u32 s = 0; s < fx->field_0x0; s++) {
        // Fx::init's scene pointer; JASystem::DSPInterface::setFXLine reads the four lines.
        const JASystem::DSPInterface::FxlineConfig_* c =
            (const JASystem::DSPInterface::FxlineConfig_*)((u8*)fx + fx->field_0x14[s]);
        for (int l = 0; l < 4; l++) {
            char taps[8 * 8];
            int n = 0;
            for (int t = 0; t < 8; t++) {
                n += snprintf(taps + n, sizeof(taps) - n, " %d", (s16)c[l].field_0x10[t]);
            }
            line("FXL %u %d %u %u %d %u %d %d%s", s, l, c[l].field_0x0, (u16)c[l].field_0x2,
                 (s16)c[l].field_0x4, (u16)c[l].field_0x6, (s16)c[l].field_0x8,
                 (int)c[l].field_0xc, taps);
            sTotals.fxLines++;
        }
    }
}

} // namespace

[[noreturn]] void smokeAudioParse() {
    JKRHeap* root = JKRHeap::getRootHeap();
    JKRSolidHeap* heap = JKRSolidHeap::create(kAudioHeapSize, root, false);
    if (heap == NULL) {
        writef(STDERR_FILENO, "[cos] audio-parse: no audio heap (root free %d)\n",
               (int)root->getTotalFreeSize());
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    // TAudioThread::start's heap and ARAM set-up.
    JASystem::Kernel::sysDramSetup(heap);
    JASystem::Kernel::sysAramSetup(kAramSize);
    // JAIZelBasic::init's parameters for the interface heap and the paths.
    JAIGlobalParameter::setParamInterfaceHeapSize(JAIZelParam::DRAM_HEAP_SIZE);
    JAIGlobalParameter::setParamAudioResPath(NULL);
    JAIGlobalParameter::setParamInitDataFileName((char*)JAIZelParam::INIT_DATA_FILE_NAME);
    JAIGlobalParameter::setParamWavePath((char*)JAIZelParam::WAVE_PATH);
    JAIGlobalParameter::setParamSequenceArchivesPath((char*)JAIZelParam::SEQ_PATH);
    JAIGlobalParameter::setParamStreamPath((char*)JAIZelParam::STREAM_PATH);
    JAIGlobalParameter::setParamSequenceArchivesFileName((char*)JAIZelParam::SEQ_ARCH_FILE_NAME);
    // mDoAud_Create: the init data in main RAM (the game frees it after init; kept here).
    void* aaf = JKRDvdRipper::loadToMainRAM(kInitData, NULL, EXPAND_SWITCH_UNKNOWN1, 0, root,
                                            JKRDvdRipper::ALLOC_DIRECTION_BACKWARD, 0, NULL);
    if (aaf == NULL) {
        writef(STDERR_FILENO, "[cos] audio-parse: cannot load %s\n", kInitData);
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    JAIGlobalParameter::setParamInitDataPointer(aaf);

    sFd = openRunFile("audio_parse.txt");
    if (sFd >= 0) {
        writef(sFd, "# audio-parse (COS_SMOKE=audio-parse): JaiInit.aaf and its banks, wave "
                    "systems, sequences and streams as JAudio read them\n");
    }
    uint64_t start = elapsedMs();
    u32 freeBefore = heap->getFreeSize();
    JAIBasic* basic = JAIBasic::getInterface();
    basic->initHeap();
    basic->initResourcePath();
    basic->initArchive();
    BOOL read = basic->initReadFile();
    pc_frame_tick();
    if (!read) {
        fail("JAIBasic::initReadFile failed");
    } else {
        int bnk = 0;
        while (JAInter::BankWave::initOnCodeBnk != NULL &&
               JAInter::BankWave::initOnCodeBnk[bnk].field_0x0 != NULL) {
            bnk++;
        }
        line("AAF banks %d wavesystems %d scenes %u", bnk, (int)JAInter::BankWave::wsMax,
             JAIGlobalParameter::getParamSoundSceneMax());
        reportSoundTable();
        reportBanks();
        pc_frame_tick();
        reportWaveSystems();
        pc_frame_tick();
        reportSequences();
        reportStreams();
        reportScenes();
        reportFx();
    }
    u32 used = freeBefore - heap->getFreeSize();
    if (sFd >= 0) {
        close(sFd);
    }
    writef(STDERR_FILENO,
           "[cos] audio-parse: %d sounds, %d banks (%d instruments, %d oscillators, %d velocity "
           "regions, %d percussion keys), %d wave systems (%d groups, %d waves), %d sequences, %d "
           "streams, %d scenes, %d fx lines; audio heap used %u bytes; %llu ms; %d error(s)%s\n",
           sTotals.sounds, sTotals.banks, sTotals.insts, sTotals.oscs, sTotals.velos, sTotals.percs,
           sTotals.waveSystems, sTotals.waveGroups, sTotals.waves, sTotals.sequences,
           sTotals.streams, sTotals.scenes, sTotals.fxLines, used,
           (unsigned long long)(elapsedMs() - start), sErrors,
           gConfig.runDir != nullptr ? " (listing in audio_parse.txt)" : "");
    bool ok = sErrors == 0 && sTotals.sounds > 0 && sTotals.banks > 0 && sTotals.insts > 0 &&
              sTotals.waveSystems > 0 && sTotals.waves > 0 && sTotals.sequences > 0 &&
              sTotals.streams > 0;
    pc_exit(ok ? PC_EXIT_REACHED : PC_EXIT_CHECK_FAILED);
}

} // namespace pc
