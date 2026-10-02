// Forwarder (phase 2, step 2.4): the game's dolphin/db/db.h over Aurora's <dolphin/db.h>, which
// declares DBInterface, __DBInterface, DBInit, DBPrintf and the exception hooks. Added here: the
// debugger-link functions the decomp declares and Aurora lacks (only OdemuExi2, outside the native
// build, uses them). Aurora declares DBInitComm, DBQueryData and DBRead with other signatures;
// Aurora's wins.
#ifndef COS_SDK_DOLPHIN_DB_DB_H
#define COS_SDK_DOLPHIN_DB_DB_H

#include <dolphin/db.h>
#include <dolphin/amcstubs/AmcExi2Stubs.h>

#ifdef __cplusplus
extern "C" {
#endif

void DBInitInterrupts(void);
BOOL DBWrite(const void*, u32);
void DBOpen(void);
void DBClose(void);

#ifdef __cplusplus
}
#endif

#endif
