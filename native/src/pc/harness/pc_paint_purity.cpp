// COS_PAINT_PURITY ([dev] or the environment; docs/FPS60_PLAN.md, step A): checks that paint B of a
// COS_FPS60_TEST split frame (cAPIGph_Painter run again on the same draw lists) leaves the game's
// state as it found it. pc_paint_extra_begin/end (pc_frame.cpp) call paintPurityBegin/End around
// paint B: the first copies the checked memory, the second compares it in 64-byte blocks and names
// what changed.
//
//   COS_PAINT_PURITY=1   the game's writable globals: the __DATA,__data/__bss/__common symbols whose
//                        object file was compiled from game/src/ (the executable's debug map, the
//                        N_OSO/N_STSYM/N_GSYM stabs of its symbol table); Aurora, Dawn, ImGui, the
//                        SDK and the harness are left out.
//   COS_PAINT_PURITY=2   also the game's heaps (JKRHeap's root heap), without the two mDoGph heaps
//                        (the sea's per-paint vertices) and the audio heap (the audio thread).
//   COS_PAINT_PURITY_REPEAT=1   paint B runs a second time (no wait, its picture drawn over the
//                        first: a check run only) and what the second run changes is reported. A
//                        paint that writes the same value again (a scratch matrix, the current view,
//                        a texture object set up per paint) drops out; what accumulates (a counter,
//                        a state machine, an allocation) stays.
//   COS_PAINT_PURITY_TRAP=1   the next checked paint B after a change in the heaps (2: or in the
//                        globals) is first seen sets a hardware watchpoint (arm64) on the game thread
//                        for the 8 bytes of that change and logs the backtrace of the first write
//                        ("[cos] paint-purity trap: ..."), then lets the write through: the code that
//                        writes, when the symbol is not enough. On other CPUs the page of the change
//                        is write-protected instead (the first write to the page, from any code).
//   COS_PAINT_PURITY_IGNORE=a,b   leaves out the symbols whose name or object path contains one of
//                        the substrings (for state judged harmless).
//
// A change is logged the first time its symbol is seen ("[cos] paint-purity frame N: ..."), as the
// symbol (demangled) plus the offset and its source file; in the heaps as the heap, the allocation,
// the process (its g_profile_*) or the class (the nearest vtable pointer before it) it lies in, and
// who points to the allocation or to its solid heap; with the 8 bytes before and after. Every 300 checked
// paints a summary line counts the paints that changed something, per symbol. Other threads
// (audio, disc) write some of this memory at any time: their symbols show up too and are listed
// as such in docs/FPS60_PLAN.md. macOS only (the symbol table read here is Mach-O's); other hosts
// log that the check is off.
#include "pc_internal.h"

#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JKernel/JKRSolidHeap.h"
#include "f_pc/f_pc_base.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_graphic.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <unistd.h>
#include <vector>

#if defined(__APPLE__)
#include <atomic>
#include <cxxabi.h>
#include <execinfo.h>
#include <signal.h>
#include <mach/mach.h>
#include <sys/mman.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/stab.h>
#endif

namespace pc {

namespace {

constexpr size_t kBlock = 64;
constexpr unsigned int kSummaryEvery = 300;

struct Sym {
    uintptr_t addr;
    uintptr_t end;
    const char* name;   // mangled, from the string table
    const char* object; // the object file's path (debug map), or nullptr
};

struct Range {
    uintptr_t start;
    uintptr_t end;
    size_t snapOffset;
    bool heap;
};

int sMode = -1; // 0 off, 1 globals, 2 globals and heaps
bool sReady = false;
std::vector<Sym> sDataSyms; // sorted by address
std::vector<Sym> sVtables;  // sorted by address
std::vector<Range> sRanges;
std::vector<uint8_t> sSnap;
std::vector<std::string> sIgnore;
unsigned int sChecked = 0;
unsigned int sDirty = 0;
std::map<std::string, unsigned int> sCounts; // per label: paints that changed it

bool paintPurityRepeatEnv() {
    static const bool on = [] {
        const char* v = getenv("COS_PAINT_PURITY_REPEAT");
        return v != nullptr && v[0] == '1';
    }();
    return on;
}

std::string demangle(const char* name) {
#if defined(__APPLE__)
    const char* m = name[0] == '_' ? name + 1 : name;
    if (m[0] == '_' && m[1] == 'Z') {
        int status = 0;
        char* d = abi::__cxa_demangle(m, nullptr, nullptr, &status);
        if (d != nullptr) {
            std::string out = d;
            free(d);
            return out;
        }
    }
    return m;
#else
    return name;
#endif
}

const char* baseName(const char* path) {
    if (path == nullptr) {
        return "?";
    }
    const char* slash = strrchr(path, '/');
    return slash != nullptr ? slash + 1 : path;
}

bool ignored(const std::string& name, const char* object) {
    for (const std::string& s : sIgnore) {
        if (name.find(s) != std::string::npos || (object != nullptr && strstr(object, s.c_str()) != nullptr)) {
            return true;
        }
    }
    return false;
}

const Sym* findSym(const std::vector<Sym>& syms, uintptr_t a) {
    auto it = std::upper_bound(syms.begin(), syms.end(), a,
                               [](uintptr_t v, const Sym& s) { return v < s.addr; });
    if (it == syms.begin()) {
        return nullptr;
    }
    --it;
    return a < it->end ? &*it : nullptr;
}

#if defined(__APPLE__)
// The main image's data symbols and vtables, each with the object file the debug map names.
bool loadSymbols(uintptr_t& dataBytes, unsigned int& gameSyms, unsigned int& hostSyms) {
    const auto* mh = reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(0));
    const intptr_t slide = _dyld_get_image_vmaddr_slide(0);
    if (mh == nullptr || mh->magic != MH_MAGIC_64) {
        return false;
    }
    struct Sect {
        uintptr_t start, end;
        bool data;
    };
    std::vector<Sect> sects;
    uintptr_t linkeditBase = 0;
    const symtab_command* symtab = nullptr;
    const auto* lc = reinterpret_cast<const load_command*>(mh + 1);
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        if (lc->cmd == LC_SEGMENT_64) {
            const auto* seg = reinterpret_cast<const segment_command_64*>(lc);
            if (strcmp(seg->segname, "__LINKEDIT") == 0) {
                linkeditBase = (uintptr_t)(seg->vmaddr + slide - seg->fileoff);
            }
            const auto* sect = reinterpret_cast<const section_64*>(seg + 1);
            for (uint32_t j = 0; j < seg->nsects; j++, sect++) {
                const bool data = strcmp(sect->segname, "__DATA") == 0 &&
                                  (strcmp(sect->sectname, "__data") == 0 || strcmp(sect->sectname, "__bss") == 0 ||
                                   strcmp(sect->sectname, "__common") == 0);
                const bool consts = strcmp(sect->segname, "__DATA_CONST") == 0 && strcmp(sect->sectname, "__const") == 0;
                if (data || consts) {
                    const uintptr_t start = (uintptr_t)(sect->addr + slide);
                    sects.push_back({start, start + (uintptr_t)sect->size, data});
                }
            }
        } else if (lc->cmd == LC_SYMTAB) {
            symtab = reinterpret_cast<const symtab_command*>(lc);
        }
        lc = reinterpret_cast<const load_command*>(reinterpret_cast<const uint8_t*>(lc) + lc->cmdsize);
    }
    if (symtab == nullptr || linkeditBase == 0) {
        return false;
    }
    const auto* nl = reinterpret_cast<const nlist_64*>(linkeditBase + symtab->symoff);
    const char* strs = reinterpret_cast<const char*>(linkeditBase + symtab->stroff);
    // The debug map: N_OSO names the object file of the stabs after it; N_STSYM gives a static's
    // address, N_GSYM a global's name.
    std::unordered_map<uint64_t, const char*> objByAddr;
    std::unordered_map<std::string, const char*> objByName;
    const char* object = nullptr;
    for (uint32_t i = 0; i < symtab->nsyms; i++) {
        const nlist_64& n = nl[i];
        if ((n.n_type & N_STAB) == 0) {
            continue;
        }
        const char* name = strs + n.n_un.n_strx;
        if (n.n_type == N_OSO) {
            object = name;
        } else if (n.n_type == N_STSYM) {
            objByAddr[n.n_value] = object;
        } else if (n.n_type == N_GSYM) {
            objByName[name] = object;
        }
    }
    for (uint32_t i = 0; i < symtab->nsyms; i++) {
        const nlist_64& n = nl[i];
        if ((n.n_type & N_STAB) != 0 || (n.n_type & N_TYPE) != N_SECT) {
            continue;
        }
        const uintptr_t a = (uintptr_t)(n.n_value + slide);
        const char* name = strs + n.n_un.n_strx;
        for (const Sect& s : sects) {
            if (a < s.start || a >= s.end) {
                continue;
            }
            if (s.data) {
                const char* obj = nullptr;
                auto byAddr = objByAddr.find(n.n_value);
                if (byAddr != objByAddr.end()) {
                    obj = byAddr->second;
                } else {
                    auto byName = objByName.find(name);
                    if (byName != objByName.end()) {
                        obj = byName->second;
                    }
                }
                sDataSyms.push_back({a, s.end, name, obj});
            } else if (strncmp(name, "__ZTV", 5) == 0) {
                sVtables.push_back({a, s.end, name, nullptr});
            }
            break;
        }
    }
    auto bySize = [](std::vector<Sym>& syms) {
        std::sort(syms.begin(), syms.end(), [](const Sym& x, const Sym& y) { return x.addr < y.addr; });
        for (size_t i = 0; i + 1 < syms.size(); i++) {
            syms[i].end = std::min(syms[i].end, syms[i + 1].addr);
        }
    };
    bySize(sDataSyms);
    bySize(sVtables);
    // The checked globals: the game's symbols, merged into ranges.
    dataBytes = 0;
    gameSyms = hostSyms = 0;
    for (const Sym& s : sDataSyms) {
        const bool game = s.object != nullptr && strstr(s.object, "/game/src/") != nullptr;
        if (!game || ignored(demangle(s.name), s.object) || s.end <= s.addr) {
            hostSyms++;
            continue;
        }
        gameSyms++;
        dataBytes += s.end - s.addr;
        if (!sRanges.empty() && !sRanges.back().heap && sRanges.back().end == s.addr) {
            sRanges.back().end = s.end;
        } else {
            sRanges.push_back({s.addr, s.end, 0, false});
        }
    }
    return true;
}
#endif

// The root heap minus the heaps paint B may use or other threads write.
void addHeapRanges(uintptr_t& heapBytes) {
    heapBytes = 0;
    JKRHeap* root = JKRHeap::getRootHeap();
    if (root == nullptr) {
        return;
    }
    std::vector<std::pair<uintptr_t, uintptr_t>> holes;
    auto hole = [&](JKRHeap* h) {
        if (h != nullptr) {
            holes.push_back({(uintptr_t)h->getStartAddr(), (uintptr_t)h->getEndAddr()});
        }
    };
    hole(mDoGph_gInf_c::mHeap[0]);
    hole(mDoGph_gInf_c::mHeap[1]);
    hole(g_mDoAud_audioHeap);
    std::sort(holes.begin(), holes.end());
    uintptr_t at = (uintptr_t)root->getStartAddr();
    const uintptr_t end = (uintptr_t)root->getEndAddr();
    for (const auto& h : holes) {
        if (h.first > at && h.first <= end) {
            sRanges.push_back({at, h.first, 0, true});
            heapBytes += h.first - at;
        }
        at = std::max(at, h.second);
    }
    if (at < end) {
        sRanges.push_back({at, end, 0, true});
        heapBytes += end - at;
    }
}

#if defined(__APPLE__)
// COS_PAINT_PURITY_TRAP: one write-protected page at a time.
int sTrapOn = 0; // 1: changes in the heaps, 2: in the globals too
std::atomic<uintptr_t> sTrapPage{0};
std::atomic<uintptr_t> sLastTrapPage{0}; // a write by another thread that faulted meanwhile retries
uintptr_t sTrapTarget = 0;
size_t sPageSize = 16384;
std::vector<uintptr_t> sTrapQueue;
std::vector<uintptr_t> sTrappedPages;
struct sigaction sPrevSegv, sPrevBus;

#if defined(__aarch64__)
// The game thread's hardware watchpoint 0 on the 8 bytes at addr (0: cleared).
bool setWatchpoint(uintptr_t addr) {
    arm_debug_state64_t ds = {};
    if (addr != 0) {
        ds.__wvr[0] = addr & ~(uintptr_t)7;
        // E (enabled), PAC EL0, LSC store, BAS all eight bytes.
        ds.__wcr[0] = 1u | (2u << 1) | (2u << 3) | (0xFFu << 5);
    }
    mach_port_t self = mach_thread_self();
    const kern_return_t kr =
        thread_set_state(self, ARM_DEBUG_STATE64, (thread_state_t)&ds, ARM_DEBUG_STATE64_COUNT);
    mach_port_deallocate(mach_task_self(), self);
    return kr == KERN_SUCCESS;
}
std::atomic<bool> sWatchArmed{false};
struct sigaction sPrevTrap;
#endif

void trapHandler(int sig, siginfo_t* si, void* ctx) {
#if defined(__aarch64__)
    if (sig == SIGTRAP) {
        if (sWatchArmed.exchange(false)) {
            setWatchpoint(0);
            char line[200];
            const int n = snprintf(line, sizeof(line), "[cos] paint-purity trap: write to %p (the change was at %p) from:\n",
                                   si->si_addr, (void*)sTrapTarget);
            write(STDERR_FILENO, line, (size_t)n);
            void* bt[24];
            const int frames = backtrace(bt, 24);
            backtrace_symbols_fd(bt, frames, STDERR_FILENO);
            return; // the write runs again, unwatched
        }
        if ((sPrevTrap.sa_flags & SA_SIGINFO) != 0 && sPrevTrap.sa_sigaction != nullptr) {
            sPrevTrap.sa_sigaction(sig, si, ctx);
        } else {
            signal(sig, SIG_DFL);
            raise(sig);
        }
        return;
    }
#endif
    const uintptr_t page = sTrapPage.load();
    const uintptr_t addr = (uintptr_t)si->si_addr;
    if (page != 0 && addr >= page && addr < page + sPageSize) {
        mprotect((void*)page, sPageSize, PROT_READ | PROT_WRITE);
        sTrapPage.store(0);
        char line[200];
        const int n = snprintf(line, sizeof(line),
                               "[cos] paint-purity trap: write to %p (the change was at %p, %s) from:\n", (void*)addr,
                               (void*)sTrapTarget,
                               addr / 64 == sTrapTarget / 64 ? "the same 64 bytes" : "elsewhere in its page");
        write(STDERR_FILENO, line, (size_t)n);
        void* bt[24];
        const int frames = backtrace(bt, 24);
        backtrace_symbols_fd(bt, frames, STDERR_FILENO);
        return; // the write runs again, unprotected
    }
    const uintptr_t last = sLastTrapPage.load();
    if (last != 0 && addr >= last && addr < last + sPageSize) {
        return; // the page is writable again
    }
    const struct sigaction& prev = sig == SIGBUS ? sPrevBus : sPrevSegv;
    if ((prev.sa_flags & SA_SIGINFO) != 0 && prev.sa_sigaction != nullptr) {
        prev.sa_sigaction(sig, si, ctx);
    } else if (prev.sa_handler != SIG_DFL && prev.sa_handler != SIG_IGN && prev.sa_handler != nullptr) {
        prev.sa_handler(sig);
    } else {
        signal(sig, SIG_DFL);
        raise(sig);
    }
}

void trapInit() {
    const char* v = getenv("COS_PAINT_PURITY_TRAP");
    sTrapOn = v != nullptr && (v[0] == '1' || v[0] == '2') ? v[0] - '0' : 0;
    if (!sTrapOn) {
        return;
    }
    sPageSize = (size_t)getpagesize();
    struct sigaction sa = {};
    sa.sa_sigaction = trapHandler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
#if defined(__aarch64__)
    sigaction(SIGTRAP, &sa, &sPrevTrap);
#else
    sigaction(SIGSEGV, &sa, &sPrevSegv);
    sigaction(SIGBUS, &sa, &sPrevBus);
#endif
}

void trapQueue(uintptr_t at, bool heap) {
#if defined(__aarch64__)
    const uintptr_t page = at & ~(uintptr_t)7;
#else
    const uintptr_t page = at & ~(uintptr_t)(sPageSize - 1);
#endif
    if (sTrapOn == 0 || (!heap && sTrapOn < 2) || std::find(sTrappedPages.begin(), sTrappedPages.end(), page) != sTrappedPages.end()) {
        return;
    }
    sTrappedPages.push_back(page);
    sTrapQueue.push_back(at);
}

void trapArm() {
    if (!sTrapOn || sTrapQueue.empty() || sTrapPage.load() != 0) {
        return;
    }
    sTrapTarget = sTrapQueue.front();
    sTrapQueue.erase(sTrapQueue.begin());
#if defined(__aarch64__)
    if (setWatchpoint(sTrapTarget)) {
        sWatchArmed.store(true);
    } else {
        writef(STDERR_FILENO, "[cos] paint-purity trap: the watchpoint on %p was refused\n", (void*)sTrapTarget);
    }
    return;
#endif
    const uintptr_t page = sTrapTarget & ~(uintptr_t)(sPageSize - 1);
    sTrapPage.store(page);
    sLastTrapPage.store(page);
    if (mprotect((void*)page, sPageSize, PROT_READ) != 0) {
        sTrapPage.store(0);
    }
}

void trapDisarm() {
#if defined(__aarch64__)
    if (sWatchArmed.exchange(false)) {
        setWatchpoint(0);
        writef(STDERR_FILENO, "[cos] paint-purity trap: no write to %p in this paint B\n", (void*)sTrapTarget);
    }
    return;
#endif
    const uintptr_t page = sTrapPage.exchange(0);
    if (page != 0) {
        mprotect((void*)page, sPageSize, PROT_READ | PROT_WRITE);
        writef(STDERR_FILENO, "[cos] paint-purity trap: no write to the page of %p in this paint B\n",
               (void*)sTrapTarget);
    }
}
#else
void trapInit() {}
void trapQueue(uintptr_t, bool) {}
void trapArm() {}
void trapDisarm() {}
#endif

bool init() {
    const char* v = getenv("COS_PAINT_PURITY");
    sMode = v != nullptr && (v[0] == '1' || v[0] == '2') ? v[0] - '0' : 0;
    if (sMode == 0) {
        return false;
    }
    if (const char* ig = getenv("COS_PAINT_PURITY_IGNORE")) {
        std::string list = ig;
        size_t pos = 0;
        while (pos <= list.size()) {
            const size_t comma = list.find(',', pos);
            std::string item = list.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
            if (!item.empty()) {
                sIgnore.push_back(item);
            }
            if (comma == std::string::npos) {
                break;
            }
            pos = comma + 1;
        }
    }
#if defined(__APPLE__)
    uintptr_t dataBytes = 0, heapBytes = 0;
    unsigned int gameSyms = 0, hostSyms = 0;
    if (!loadSymbols(dataBytes, gameSyms, hostSyms)) {
        writef(STDERR_FILENO, "[cos] paint-purity: the executable's symbol table was not found; off\n");
        sMode = 0;
        return false;
    }
    if (sMode == 2) {
        addHeapRanges(heapBytes);
    }
    size_t total = 0;
    for (Range& r : sRanges) {
        r.snapOffset = total;
        total += r.end - r.start;
    }
    sSnap.resize(total);
    trapInit();
    writef(STDERR_FILENO,
           "[cos] paint-purity: COS_PAINT_PURITY=%d%s: paint B checked over %u game symbols (%.1f KiB of "
           ".data/.bss, %u host symbols left out)%s%.1f MiB of heaps; %zu ignore patterns\n",
           sMode, paintPurityRepeatEnv() ? " (REPEAT: paint B's second run)" : "", gameSyms, dataBytes / 1024.0, hostSyms, sMode == 2 ? " and " : ", ",
           heapBytes / 1048576.0, sIgnore.size());
    sReady = true;
    return true;
#else
    writef(STDERR_FILENO, "[cos] paint-purity: COS_PAINT_PURITY needs the macOS host (Mach-O symbols); off\n");
    sMode = 0;
    return false;
#endif
}

const char* heapName(JKRHeap* h) {
    for (JKRHeap* p = h; p != nullptr; p = p == JKRHeap::getRootHeap() ? nullptr : p->getParent()) {
        if (p == (JKRHeap*)mDoExt_getGameHeap()) {
            return "game";
        }
        if (p == (JKRHeap*)mDoExt_getZeldaHeap()) {
            return "zelda";
        }
        if (p == (JKRHeap*)mDoExt_getArchiveHeap()) {
            return "archive";
        }
        if (p == (JKRHeap*)mDoExt_getCommandHeap()) {
            return "command";
        }
    }
    return "root";
}

// The used block of an expanded heap that holds a, or 0.
uintptr_t expBlock(JKRHeap* h, uintptr_t a, uint32_t* size) {
    if (h == nullptr || h->getHeapType() != 'EXPH') {
        return 0;
    }
    for (JKRExpHeap::CMemBlock* b = static_cast<JKRExpHeap*>(h)->getUsedFirst(); b != nullptr; b = b->getNextBlock()) {
        const uintptr_t content = (uintptr_t)b->getContent();
        if (a >= content && a < content + b->getSize()) {
            *size = b->getSize();
            return content;
        }
    }
    return 0;
}

// What object is at a, cheaply: a process (its g_profile_* global, base_process_class::mpProf),
// else the class of the nearest vtable pointer before a (a primary vptr: the vtable symbol + 16)
// within the block, else "".
std::string objectAt(uintptr_t a, uintptr_t blockStart) {
    char buf[64];
    if (blockStart != 0) {
        uintptr_t prof;
        memcpy(&prof, (const void*)(blockStart + offsetof(base_process_class, mpProf)), sizeof(prof));
        const Sym* ps = findSym(sDataSyms, prof);
        if (ps != nullptr && ps->addr == prof && strstr(ps->name, "g_profile_") != nullptr) {
            snprintf(buf, sizeof(buf), " +0x%lx", (unsigned long)(a - blockStart));
            return "process " + demangle(ps->name) + buf;
        }
    }
    const uintptr_t lowest = std::max(blockStart, a > 4096 ? a - 4096 : (uintptr_t)0);
    for (uintptr_t p = a & ~(uintptr_t)7; p >= lowest && p != 0; p -= 8) {
        uintptr_t word;
        memcpy(&word, (const void*)p, sizeof(word));
        const Sym* vt = findSym(sVtables, word);
        if (vt != nullptr && word == vt->addr + 16) {
            std::string cls = demangle(vt->name);
            if (cls.rfind("vtable for ", 0) == 0) {
                cls = cls.substr(11);
            }
            snprintf(buf, sizeof(buf), " +0x%lx", (unsigned long)(a - p));
            return cls + buf;
        }
    }
    return "";
}

// A global or heap word equal to v (the first found): "<symbol>+0x<off>" or the object holding it.
std::string whoPointsTo(uintptr_t lo, uintptr_t hi) {
    char buf[64];
    for (const Range& r : sRanges) {
        for (uintptr_t q = (r.start + 7) & ~(uintptr_t)7; q + 8 <= r.end; q += 8) {
            uintptr_t word;
            memcpy(&word, (const void*)q, sizeof(word));
            if (word < lo || word >= hi || (q >= lo && q < hi)) {
                continue;
            }
            if (!r.heap) {
                const Sym* g = findSym(sDataSyms, q);
                snprintf(buf, sizeof(buf), "+0x%lx", g != nullptr ? (unsigned long)(q - g->addr) : 0ul);
                return "global " + (g != nullptr ? demangle(g->name) : std::string("?")) + buf;
            }
            JKRHeap* h = JKRHeap::findFromRoot((void*)q);
            uint32_t size = 0;
            const uintptr_t block = expBlock(h, q, &size);
            std::string obj = objectAt(q, block);
            snprintf(buf, sizeof(buf), "heap %s %p", heapName(h), (void*)q);
            return obj.empty() ? std::string(buf) : obj + " (" + buf + ")";
        }
    }
    return "nothing";
}

// What changed at address a in the heaps. The key (cheap, per paint): the heap and the object; the
// detail (first sighting only): the block and who points to it or to its heap.
std::string heapLabel(uintptr_t a, bool wantDetail, std::string& detail) {
    char buf[160];
    JKRHeap* h = JKRHeap::findFromRoot((void*)a);
    const char* named = heapName(h);
    uint32_t size = 0;
    const uintptr_t block = expBlock(h, a, &size);
    std::string obj = objectAt(a, block);
    std::string key;
    if (!obj.empty()) {
        key = std::string("heap ") + named + ": " + obj.substr(0, obj.find(" +0x"));
    } else if (block != 0) {
        snprintf(buf, sizeof(buf), "heap %s: block of %u bytes at %p", named, (unsigned int)size, (void*)block);
        key = buf;
    } else {
        snprintf(buf, sizeof(buf), "heap %s: in heap %p", named, (void*)h);
        key = buf;
    }
    if (!wantDetail) {
        return key;
    }
    char type[5] = "????";
    if (h != nullptr) {
        const uint32_t t = h->getHeapType();
        type[0] = (char)(t >> 24);
        type[1] = (char)(t >> 16);
        type[2] = (char)(t >> 8);
        type[3] = (char)t;
    }
    snprintf(buf, sizeof(buf), "heap %s %p (%s), at %p: ", named, (void*)h, type, (void*)a);
    detail = buf;
    if (block != 0) {
        snprintf(buf, sizeof(buf), "block of %u bytes +0x%lx, ", (unsigned int)size, (unsigned long)(a - block));
        detail += buf;
    }
    detail += obj.empty() ? std::string("no object found") : obj;
    if (block != 0) {
        detail += "; the block is pointed to by " + whoPointsTo(block, block + size);
    } else if (h != nullptr) {
        // A solid heap (a model's, a room's): whoever holds the heap owns it.
        detail += "; its heap is held by " + whoPointsTo((uintptr_t)h, (uintptr_t)h + 1);
        snprintf(buf, sizeof(buf), " (+0x%lx in the heap)", (unsigned long)(a - (uintptr_t)h->getStartAddr()));
        detail += buf;
    }
    return key;
}

} // namespace

void paintPurityArm() {
    if (sReady) {
        trapArm();
    }
}

bool paintPurityRepeat() {
    return sReady && paintPurityRepeatEnv();
}

void paintPurityBegin() {
    // The check's own containers use host memory, not the game's current heap (JKRHeap.cpp).
    JKRPcHostAllocScope hostAlloc;
    if (sMode < 0) {
        init();
    }
    if (!sReady) {
        return;
    }
    for (const Range& r : sRanges) {
        memcpy(sSnap.data() + r.snapOffset, (const void*)r.start, r.end - r.start);
    }
}

void paintPurityEnd(unsigned int frame) {
    if (!sReady) {
        return;
    }
    JKRPcHostAllocScope hostAlloc;
    trapDisarm();
    sChecked++;
    std::map<std::string, unsigned int> changed; // label -> blocks this paint
    struct First {
        uintptr_t at;
        bool heap;
        uint64_t before, after; // the 8 bytes from the first changed one
    };
    std::map<std::string, First> firstAt; // label -> first change
    for (const Range& r : sRanges) {
        const uint8_t* snap = sSnap.data() + r.snapOffset;
        const uint8_t* cur = (const uint8_t*)r.start;
        const size_t size = r.end - r.start;
        for (size_t page = 0; page < size; page += 4096) {
            const size_t pageLen = std::min<size_t>(4096, size - page);
            if (memcmp(snap + page, cur + page, pageLen) == 0) {
                continue;
            }
            for (size_t b = page; b < page + pageLen; b += kBlock) {
                const size_t len = std::min(kBlock, page + pageLen - b);
                if (memcmp(snap + b, cur + b, len) == 0) {
                    continue;
                }
                for (size_t i = 0; i < len; i++) {
                    if (snap[b + i] == cur[b + i]) {
                        continue;
                    }
                    const size_t off = b + i;
                    const uintptr_t at = r.start + off;
                    std::string key;
                    if (r.heap) {
                        // One label per block in the heaps (each costs a heap lookup).
                        std::string none;
                        key = heapLabel(at, false, none);
                        i = len;
                    } else {
                        // Every symbol of the block that changed.
                        const Sym* sym = findSym(sDataSyms, at);
                        key = sym != nullptr ? demangle(sym->name) : "?";
                        if (sym != nullptr && sym->end > at) {
                            i += sym->end - at - 1;
                        }
                    }
                    if (changed[key]++ == 0) {
                        First f{at, r.heap, 0, 0};
                        const size_t n = std::min<size_t>(8, size - off);
                        memcpy(&f.before, snap + off, n);
                        memcpy(&f.after, cur + off, n);
                        firstAt[key] = f;
                    }
                }
            }
        }
    }
    if (!changed.empty()) {
        sDirty++;
    }
    for (const auto& c : changed) {
        if (sCounts[c.first]++ == 0) {
            const First& at = firstAt[c.first];
            trapQueue(at.at, at.heap);
            std::string detail;
            if (at.heap) {
                heapLabel(at.at, true, detail);
            } else {
                const Sym* sym = findSym(sDataSyms, at.at);
                char off[160];
                snprintf(off, sizeof(off), "+0x%lx (%s)", sym != nullptr ? (unsigned long)(at.at - sym->addr) : 0ul,
                         sym != nullptr ? baseName(sym->object) : "?");
                detail = c.first + off;
            }
            writef(STDERR_FILENO,
                   "[cos] paint-purity frame %u: paint B changed %s (%u block%s of 64 bytes; 8 bytes from the "
                   "first change %016llx -> %016llx, little-endian)\n",
                   frame, detail.c_str(), c.second, c.second == 1 ? "" : "s", (unsigned long long)at.before,
                   (unsigned long long)at.after);
        }
    }
    if (sChecked % kSummaryEvery == 0) {
        std::vector<std::pair<unsigned int, std::string>> top;
        for (const auto& c : sCounts) {
            top.push_back({c.second, c.first});
        }
        std::sort(top.begin(), top.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
        std::string list;
        for (size_t i = 0; i < top.size() && i < 16; i++) {
            list += (i == 0 ? " " : ", ") + top[i].second + " x" + std::to_string(top[i].first);
        }
        writef(STDERR_FILENO, "[cos] paint-purity: %u paints B checked, %u changed something;%s\n", sChecked,
               sDirty, list.empty() ? " nothing changed" : list.c_str());
    }
}

} // namespace pc
