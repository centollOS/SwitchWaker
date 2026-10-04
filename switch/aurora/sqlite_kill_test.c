// Kill -9 test of Aurora's dawn_cache.db writes with the Switch's sqlite build options
// (switch/native/CMakeLists.txt: SQLITE_OMIT_WAL, exclusive locking, temp store in memory; the
// unix-none VFS of sqlite_horizon.c; synchronous=OFF). Runs on the Mac or Linux, not the console.
//
//   cc -O2 -o build/sqlite_kill_test switch/aurora/sqlite_kill_test.c <sqlite-src>/sqlite3.c \
//      -I<sqlite-src> -DSQLITE_OMIT_WAL=1 -DSQLITE_MAX_MMAP_SIZE=0 -DSQLITE_OMIT_LOAD_EXTENSION=1 \
//      -DSQLITE_THREADSAFE=1 -DSQLITE_DEFAULT_LOCKING_MODE=1 -DSQLITE_TEMP_STORE=3
//   (cd <empty dir> && sqlite_kill_test MODE [N] [VACUUM])
//
// <sqlite-src>: the amalgamation the NRO builds (build/switch-native/_deps/sqlite3-src). MODE is
// the journal_mode (memory, persist, truncate, delete); N iterations (100); VACUUM 1 (default) or
// 0. Each iteration forks a writer that does what gpu_cache.cpp does (one REPLACE of a 2-62 KiB
// blob per transaction, and every 97 rows a prune DELETE and a VACUUM), kills it with SIGKILL
// after 20-420 ms, reopens the file (a hot journal is rolled back) and runs
// PRAGMA integrity_check; a damaged file is counted and deleted. 2026-10-04: memory 113/150
// damaged (117/150 without VACUUM, 198/300 with journal_size_limit), persist, truncate and
// delete 0/150 (persist 0/300 with journal_size_limit).
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "sqlite3.h"

static const char* g_mode;
static const char* g_path = "kt.db";
static int g_vacuum = 1;

static sqlite3* open_db(void) {
    sqlite3* db = NULL;
    if (sqlite3_open_v2(g_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, "unix-none") != SQLITE_OK) {
        fprintf(stderr, "open: %s\n", sqlite3_errmsg(db));
        exit(3);
    }
    char sql[256];
    snprintf(sql, sizeof sql, "PRAGMA journal_mode=%s; PRAGMA journal_size_limit=4194304; PRAGMA synchronous=OFF;", g_mode);
    sqlite3_exec(db, sql, NULL, NULL, NULL);
    return db;
}

static void writer(unsigned seed) {
    srand(seed);
    sqlite3* db = open_db();
    sqlite3_exec(db,
                 "CREATE TABLE IF NOT EXISTS cache (key BLOB PRIMARY KEY NOT NULL, value BLOB NOT NULL, "
                 "size INTEGER NOT NULL, compressed INTEGER NOT NULL);",
                 NULL, NULL, NULL);
    sqlite3_stmt* st;
    sqlite3_prepare_v2(db, "REPLACE INTO cache (key, value, size, compressed) VALUES (?, ?, ?, 0)", -1, &st, NULL);
    static unsigned char buf[65536];
    for (unsigned i = 0;; i++) {
        unsigned char key[16];
        for (int k = 0; k < 16; k++) key[k] = (unsigned char)rand();
        int n = 2048 + rand() % (60 * 1024);
        memset(buf, (int)i, (size_t)n);
        sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
        sqlite3_bind_blob(st, 1, key, 16, SQLITE_TRANSIENT);
        sqlite3_bind_blob(st, 2, buf, n, SQLITE_STATIC);
        sqlite3_bind_int(st, 3, n);
        if (sqlite3_step(st) != SQLITE_DONE) {
            fprintf(stderr, "insert: %s\n", sqlite3_errmsg(db));
            _exit(4);
        }
        sqlite3_reset(st);
        sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
        if (g_vacuum && i % 97 == 96) {
            sqlite3_exec(db, "BEGIN IMMEDIATE; DELETE FROM cache WHERE rowid % 3 = 0; COMMIT; VACUUM;", NULL, NULL,
                         NULL);
        }
    }
}

static int check(void) {
    sqlite3* db = open_db();
    int ok = 0;
    sqlite3_stmt* st;
    if (sqlite3_prepare_v2(db, "PRAGMA integrity_check", -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW && strcmp((const char*)sqlite3_column_text(st, 0), "ok") == 0) ok = 1;
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return ok;
}

int main(int argc, char** argv) {
    g_mode = argc > 1 ? argv[1] : "memory";
    int iters = argc > 2 ? atoi(argv[2]) : 100;
    g_vacuum = argc > 3 ? atoi(argv[3]) : 1;
    sqlite3_vfs_register(sqlite3_vfs_find("unix-none"), 1);
    unlink(g_path);
    char j[64];
    snprintf(j, sizeof j, "%s-journal", g_path);
    unlink(j);
    srand((unsigned)time(NULL));
    int corrupt = 0, firstCorrupt = -1;
    for (int it = 0; it < iters; it++) {
        pid_t pid = fork();
        if (pid == 0) writer((unsigned)rand());
        usleep(20000 + rand() % 400000);
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        if (!check()) {
            corrupt++;
            if (firstCorrupt < 0) firstCorrupt = it;
            unlink(g_path);
            unlink(j);
        }
    }
    printf("mode=%s vacuum=%d iterations=%d corrupt=%d (first at %d)\n", g_mode, g_vacuum, iters, corrupt,
           firstCorrupt);
    return 0;
}
