// Entry point of the native port's NRO (COS_EXE_ENTRY in native/cmake/executable.cmake; the Mac
// uses aurora::main). It sets up the logs and run options first, then runs the game's main on a
// thread of its own with a 16 MiB stack: the NRO's main thread has the fixed stack hbloader gives
// it, and Aurora's start-up compiles shaders on the calling thread (Tint needs several MiB, see
// the translated port's checklist). The game thread prefers core 0 (source/thread_wrap.c).
#include <pthread.h>
#include <stdio.h>

#include "cos_switch_internal.h"

namespace {

constexpr size_t kGameStackSize = 16u * 1024u * 1024u;

struct GameArgs {
    int argc;
    char** argv;
    int result;
};

void* gameThread(void* raw) {
    auto* args = static_cast<GameArgs*>(raw);
    cos_switch_thread_role(COS_SWITCH_THREAD_GAME);
    args->result = aurora_main(args->argc, args->argv);
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    cos_switch_start(argc, argv);

    GameArgs args{argc, argv, 0};
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, kGameStackSize);
    cos_switch_next_thread_core(0);
    pthread_t thread;
    const int err = pthread_create(&thread, &attr, gameThread, &args);
    pthread_attr_destroy(&attr);
    if (err != 0) {
        fprintf(stderr, "[switch] cannot start the game thread (%d); running it on the main thread\n", err);
        args.result = aurora_main(argc, argv);
    } else {
        pthread_join(thread, nullptr);
    }
    fprintf(stderr, "[switch] the game's main returned %d\n", args.result);
    cos_switch_exit(args.result);
}
