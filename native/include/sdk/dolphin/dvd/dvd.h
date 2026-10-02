// Forwarder (phase 2, step 2.4): the game's dolphin/dvd/dvd.h over Aurora's <dolphin/dvd.h>, which
// declares the DVD structs, state/result/command constants and the DVD, DVDFS and DVDLow APIs
// (aurora_dvd implements them over nod).
//
// Added here, the game-only names:
// - DVDDirectory and DVDDirectoryEntry, the decomp's names for Aurora's DVDDir and DVDDirEntry
//   (same layout; the field names differ, e.g. entry_number/entryNum, and the game adapts under
//   TARGET_PC in step 2.7);
// - DVDGetLength, DVDOptionalCommandChecker, DVD_WATYPE_MAX and DVDBB1;
// - the SDK-internal __DVDPrepareResetAsync and __DVDAudioBufferConfig.
//   __DVDPopWaitingQueue comes from <dolphin/dvd/dvdqueue.h>, which this header includes, so it is
//   declared once.
//
// Left out on purpose:
// - the DVDState and DVDResult enums: their enumerators are macros in Aurora, so the enums cannot
//   be declared again (no game unit names the enum types). Aurora's macro values win; note that
//   Aurora's DVD_RESULT_CANCELED is -6 where the decomp's is -3;
// - the decomp's `static DVDCancelAsync` (Aurora declares it with external linkage);
// - struct field names that differ from Aurora's (DVDFileInfo.block/cb, DVDDiskID...): Aurora's
//   win, step 2.7.
#ifndef COS_SDK_DOLPHIN_DVD_DVD_H
#define COS_SDK_DOLPHIN_DVD_DVD_H

#include <dolphin/dvd.h>
#include <dolphin/os.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef DVDDir DVDDirectory;
typedef DVDDirEntry DVDDirectoryEntry;

typedef struct DVDBB1 {
    u32 appLoaderLength;
    void* appLoaderFunc1;
    void* appLoaderFunc2;
    void* appLoaderFunc3;
} DVDBB1;

#define DVD_WATYPE_MAX 2

typedef void (*DVDOptionalCommandChecker)(DVDCommandBlock* block, void (*cb)(u32 intType));

#define DVDGetLength(fi) (fi)->length

void __DVDPrepareResetAsync(DVDCBCallback callback);
void __DVDAudioBufferConfig(DVDCommandBlock* block, u32 enable, u32 size, DVDCBCallback callback);

#ifdef __cplusplus
}
#endif

#include <dolphin/dvd/dvdqueue.h>

#endif
