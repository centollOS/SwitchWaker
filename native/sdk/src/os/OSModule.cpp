// cos_sdk: the OS module list globals (docs/NATIVE_PORT_PHASE2_3.md, step 2.9).
//
// On the GameCube __OSModuleList (the queue of linked RELs) and __OSStringTable live in the OS
// globals at fixed addresses (0x800030C8 and 0x800030D0); the decomp's OSLink.h defines them with
// AT_ADDRESS. The forwarder native/include/sdk/dolphin/os/OSLink.h only declares them, so they are
// defined once here. JUTException and m_Do_printf walk the list to name the module a crash
// address belongs to; it is empty, since no REL is ever linked dynamically on PC (phase 3 links
// the REL units statically, step 3.5). OSLink, OSLinkFixed, OSUnlink and OSSetStringTable are
// deliberately not provided: their callers go away in step 3.5.
//
// Provenance: written for this project (Dusklight has no counterpart; Dusklight's DynamicLink is
// compiled out under TARGET_PC).
#include <dolphin/os/OSModule.h>

extern "C" {

OSModuleQueue __OSModuleList = {nullptr, nullptr};
void* __OSStringTable = nullptr;

}  // extern "C"
