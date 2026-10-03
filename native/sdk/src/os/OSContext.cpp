// cos_sdk: OSContext (docs/NATIVE_PORT_PHASE2_3.md, step 2.6a).
//
// Provenance: adapted from Dusklight src/dusk/OSContext.cpp (CC0, ref/dusklight). There is no
// PowerPC register state on the host: Aurora's TARGET_PC OSContext is opaque storage, and the host
// OS saves and restores thread registers itself. Kept from it: OSClearContext, OSFillFPUContext,
// OSLoadFPUContext, OSSaveFPUContext and __OSContextInit do nothing; OSDumpContext says there is
// nothing to dump; OSGetStackPointer returns 0 (a 64-bit stack pointer does not fit its u32 result;
// the game's back-chain walkers stop at 0, see TODO(native phase 4) in m_Do_printf.cpp and
// DynamicLink.cpp); OSSwitchStack and OSSwitchFiber abort. Changed:
// - the current context is per host thread (the running thread's context, as on the GameCube),
//   not one process-wide pointer;
// - OSInitContext, OSSaveContext and OSLoadContext abort loudly instead of doing nothing, since a
//   caller that relies on them would silently run wrong.
#include "os_internal.h"

using namespace cos_sdk::os;

namespace {

thread_local OSContext* tCurrentContext = nullptr;

} // namespace

extern "C" {

OSContext* OSGetCurrentContext(void) {
    if (tCurrentContext == nullptr) {
        return &OSGetCurrentThread()->context;
    }
    return tCurrentContext;
}

void OSSetCurrentContext(OSContext* context) {
    tCurrentContext = context;
}

void OSClearContext(OSContext* context) {
    (void)context;
}

void OSInitContext(OSContext* context, u32 pc, u32 newsp) {
    Fatal("OSInitContext(%p, 0x%08x, 0x%08x): PowerPC contexts cannot be built on the host",
          static_cast<void*>(context), pc, newsp);
}

u32 OSSaveContext(OSContext* context) {
    Fatal("OSSaveContext(%p): PowerPC register state cannot be saved on the host",
          static_cast<void*>(context));
}

void OSLoadContext(OSContext* context) {
    Fatal("OSLoadContext(%p): PowerPC register state cannot be loaded on the host",
          static_cast<void*>(context));
}

void OSDumpContext(OSContext* context) {
    Log("OSDumpContext(%p): no PowerPC register state on the host", static_cast<void*>(context));
}

void OSFillFPUContext(OSContext* context) {
    (void)context;
}

void OSLoadFPUContext(OSContext* fpucontext) {
    (void)fpucontext;
}

void OSSaveFPUContext(OSContext* fpucontext) {
    (void)fpucontext;
}

u32 OSGetStackPointer(void) {
    return 0;
}

u32 OSSwitchStack(u32 newsp) {
    Fatal("OSSwitchStack(0x%08x): stacks cannot be switched on the host", newsp);
}

int OSSwitchFiber(u32 pc, u32 newsp) {
    Fatal("OSSwitchFiber(0x%08x, 0x%08x): stacks cannot be switched on the host", pc, newsp);
}

void __OSContextInit(void) {}

} // extern "C"
