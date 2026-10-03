// cos_sdk: host memory for host-side bookkeeping (header only).
//
// Provenance: written for cos_sdk.
//
// On PC the game's global operator new/delete (game/src/JSystem/JKernel/JKRHeap.cpp) put
// every allocation made after the root heap exists into the current JKRHeap, and freeing such a
// block takes that heap's OSMutex. Host bookkeeping must not go through them:
// - the SDK's own containers are used from the alarm timer thread, whose handlers run "with
//   interrupts disabled" and must not block (OSLockMutex there is fatal), and from host threads
//   that end (thread-local destructors);
// - its records would sit in game heaps that the game frees wholesale (freeAll, destroy);
// - test harnesses that measure the game heaps must not change what they measure.
// HostAllocator<T> and the helpers below use malloc/free directly, so they never enter JKRHeap.
#ifndef COS_SDK_HOST_ALLOC_H
#define COS_SDK_HOST_ALLOC_H

#ifdef __cplusplus

#include <cstddef>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cos_sdk {

// Raw host memory, aligned for `align` (at least alignof(max_align_t)). Never null: aborts when
// the host is out of memory, as there is nothing sensible for bookkeeping to do then.
inline void* HostAllocate(std::size_t size, std::size_t align) {
    if (size == 0) {
        size = 1;
    }
    void* p;
    if (align <= alignof(std::max_align_t)) {
        p = std::malloc(size);
    } else {
        // aligned_alloc needs a size that is a multiple of the (power-of-two) alignment.
        p = std::aligned_alloc(align, (size + align - 1) & ~(align - 1));
    }
    if (p == nullptr) {
        std::abort();
    }
    return p;
}

inline void HostFree(void* p) {
    std::free(p);
}

// A standard allocator over HostAllocate/HostFree.
template <class T>
struct HostAllocator {
    using value_type = T;

    HostAllocator() noexcept = default;
    template <class U>
    HostAllocator(const HostAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        if (n > static_cast<std::size_t>(-1) / sizeof(T)) {
            std::abort();
        }
        return static_cast<T*>(HostAllocate(n * sizeof(T), alignof(T)));
    }
    void deallocate(T* p, std::size_t) noexcept { HostFree(p); }

    template <class U>
    bool operator==(const HostAllocator<U>&) const noexcept {
        return true;
    }
    template <class U>
    bool operator!=(const HostAllocator<U>&) const noexcept {
        return false;
    }
};

// new/delete of one object in host memory.
template <class T, class... Args>
T* HostNew(Args&&... args) {
    void* p = HostAllocate(sizeof(T), alignof(T));
    return ::new (p) T(std::forward<Args>(args)...);
}

template <class T>
void HostDelete(T* p) noexcept {
    if (p != nullptr) {
        p->~T();
        HostFree(p);
    }
}

template <class T>
struct HostDeleter {
    void operator()(T* p) const noexcept { HostDelete(p); }
};

template <class T>
using HostUniquePtr = std::unique_ptr<T, HostDeleter<T>>;

template <class T, class... Args>
HostUniquePtr<T> HostMakeUnique(Args&&... args) {
    return HostUniquePtr<T>(HostNew<T>(std::forward<Args>(args)...));
}

// The object and its control block in host memory.
template <class T, class... Args>
std::shared_ptr<T> HostMakeShared(Args&&... args) {
    return std::allocate_shared<T>(HostAllocator<T>(), std::forward<Args>(args)...);
}

using HostString = std::basic_string<char, std::char_traits<char>, HostAllocator<char>>;

template <class T>
using HostVector = std::vector<T, HostAllocator<T>>;

template <class K, class V, class Compare = std::less<K>>
using HostMap = std::map<K, V, Compare, HostAllocator<std::pair<const K, V>>>;

template <class K, class V, class Hash = std::hash<K>, class Eq = std::equal_to<K>>
using HostUnorderedMap = std::unordered_map<K, V, Hash, Eq, HostAllocator<std::pair<const K, V>>>;

} // namespace cos_sdk

#endif // __cplusplus

#endif // COS_SDK_HOST_ALLOC_H
