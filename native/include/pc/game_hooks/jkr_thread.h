#ifndef PC_GAME_HOOKS_JKR_THREAD_H
#define PC_GAME_HOOKS_JKR_THREAD_H

// JKRThread's list (JKRThread::sThreadList) under a lock: on the host the threads that build and
// drop JKRThreads run at the same time (native/src/pc/game_hooks/pc_jkr_hooks.cpp). Included by
// game/src/JSystem/JKernel/JKRThread.cpp under TARGET_PC (step G3 of docs/GAME_CODE_ORGANIZATION.md).
class JKRThread;
template <typename T> class JSULink;

void JKRPcThreadListAppend(JSULink<JKRThread>* link);
void JKRPcThreadListRemove(JSULink<JKRThread>* link);

#endif /* PC_GAME_HOOKS_JKR_THREAD_H */
