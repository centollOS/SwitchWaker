// The subset of <sys/mman.h> the donor DSP's Common shim uses
// (apple/ios/src/dsp_common_shim.cpp): anonymous read/write allocations and
// write protection of its instruction ROM. Horizon homebrew has no mmap, so
// allocate page-aligned heap memory and treat protection changes as no-ops.
#pragma once

#include <malloc.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_PRIVATE 0x02
#define MAP_ANON 0x20
#define MAP_ANONYMOUS MAP_ANON
#define MAP_FAILED ((void*)-1)

static inline void* mmap(void* address, size_t size, int protection, int flags, int fd,
                         off_t offset) {
    (void)address;
    (void)protection;
    (void)flags;
    (void)fd;
    (void)offset;
    void* memory = memalign(0x1000, size);
    if (memory == NULL)
        return MAP_FAILED;
    memset(memory, 0, size);
    return memory;
}

static inline int munmap(void* address, size_t size) {
    (void)size;
    free(address);
    return 0;
}

static inline int mprotect(void* address, size_t size, int protection) {
    (void)address;
    (void)size;
    (void)protection;
    return 0;
}
