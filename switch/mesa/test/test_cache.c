/* Host test of the Switch Mesa patches: the single-file disk cache and nvc0's codegen cache. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "util/disk_cache.h"
#include "util/u_switch_stats.h"
#include "tgsi/tgsi_text.h"
#include "tgsi/tgsi_parse.h"
#include "nvc0/nvc0_context.h"
#include "nvc0/nvc0_program.h"
int compare_relocs_fixups(const void *ra, const void *rb, const void *fa, const void *fb, unsigned *nfix);

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static const char *dir = "/tmp/decomp-cache-test";
static char path[256];
static char idx_path[256];

static long fsize(void) { struct stat st; return stat(path, &st) == 0 ? st.st_size : -1; }
static long isize(void) { struct stat st; return stat(idx_path, &st) == 0 ? st.st_size : -1; }

static void copy_file(const char *from, const char *to)
{
   FILE *in = fopen(from, "rb"), *out = fopen(to, "wb");
   assert(in && out);
   char buf[65536];
   size_t n;
   while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
   fclose(in); fclose(out);
}

static void poke(const char *file, long offset, const void *data, size_t size)
{
   FILE *f = fopen(file, "r+b");
   assert(f);
   fseek(f, offset, SEEK_SET); fwrite(data, 1, size, f); fclose(f);
}

static const char *reopen(struct disk_cache **c, const char *id)
{
   if (*c) disk_cache_destroy(*c);
   *c = disk_cache_create("GM20B", id, 0);
   const char *s = mesa_switch_shader_cache_status();
   printf("status: %s\n", s);
   return s;
}

/* The .idx beside the .bin: opening reads it at once, and every way it can disagree with the .bin
 * (missing, torn, stale, damaged, foreign, older file) falls back to reading the records. */
static void test_index(void)
{
   enum { N = 3000, SIZE = 4000 };
   unlink(path); unlink(idx_path);
   struct disk_cache *c = NULL;
   reopen(&c, "id-idx");
   CHECK(isize() == 32);
   static char payload[SIZE];
   cache_key keys[N];
   for (unsigned i = 0; i < N; i++) {
      disk_cache_compute_key(c, &i, sizeof(i), keys[i]);
      memset(payload, (int)i, sizeof(payload));
      memcpy(payload, &i, sizeof(i));
      disk_cache_put(c, keys[i], payload, SIZE - (i % 7), NULL);
   }
   CHECK(isize() == 32 + 40L * N);
   const char *s = reopen(&c, "id-idx");
   CHECK(strstr(s, ": 3000 entries") && strstr(s, "index: 3000 records in one read") &&
         !strstr(s, "more read") && !strstr(s, "cut"));
   size_t sz; void *d;
   for (unsigned i = 0; i < N; i += 397) {
      d = disk_cache_get(c, keys[i], &sz);
      CHECK(d && sz == SIZE - (i % 7) && memcmp(d, &i, sizeof(i)) == 0 &&
            ((unsigned char *)d)[sz - 1] == (unsigned char)i);
      free(d);
   }

   /* No .idx: rebuilt from the .bin once, then read at once again. */
   unlink(idx_path);
   s = reopen(&c, "id-idx");
   CHECK(strstr(s, ": 3000 entries") && strstr(s, "index rebuilt (no index file): 3000 records read one by one"));
   CHECK(isize() == 32 + 40L * N);
   s = reopen(&c, "id-idx");
   CHECK(strstr(s, "index: 3000 records in one read"));

   /* A removal and a put after it: tombstones are indexed too. */
   disk_cache_remove(c, keys[5]);
   disk_cache_put(c, keys[5], "back", 5, NULL);
   disk_cache_remove(c, keys[6]);
   s = reopen(&c, "id-idx");
   CHECK(strstr(s, ": 2999 entries") && strstr(s, "index: 3003 records in one read"));
   d = disk_cache_get(c, keys[5], &sz); CHECK(d && sz == 5 && strcmp(d, "back") == 0); free(d);
   CHECK(disk_cache_get(c, keys[6], &sz) == NULL);
   const long n_rec = 3003;

   /* A torn last entry (stopped mid-write): cut, and its record read from the .bin. */
   CHECK(truncate(idx_path, isize() - 7) == 0);
   s = reopen(&c, "id-idx");
   CHECK(strstr(s, ": 2999 entries") && strstr(s, "3002 records in one read, 1 more read one by one, 1 stale entries cut"));
   CHECK(isize() == 32 + 40L * n_rec);

   /* A record written without its entry (stopped between the two writes). */
   char saved[300];
   snprintf(saved, sizeof(saved), "%s.saved", idx_path);
   copy_file(idx_path, saved);
   cache_key extra;
   disk_cache_compute_key(c, "extra", 5, extra);
   disk_cache_put(c, extra, "extra", 6, NULL);
   disk_cache_destroy(c); c = NULL;
   copy_file(saved, idx_path);
   s = reopen(&c, "id-idx");
   CHECK(strstr(s, ": 3000 entries") && strstr(s, "3003 records in one read, 1 more read one by one") && !strstr(s, "cut"));
   d = disk_cache_get(c, extra, &sz); CHECK(d && strcmp(d, "extra") == 0); free(d);
   s = reopen(&c, "id-idx");
   CHECK(strstr(s, "index: 3004 records in one read") && !strstr(s, "more read"));

   /* A torn .bin (its last record cut) under a whole .idx: the stale entry and the torn tail go. */
   disk_cache_destroy(c); c = NULL;
   CHECK(truncate(path, fsize() - 3) == 0);
   s = reopen(&c, "id-idx");
   CHECK(strstr(s, ": 2999 entries") && strstr(s, "3003 records in one read, 1 stale entries cut") &&
         strstr(s, "cut 35 bytes of a damaged tail"));
   CHECK(disk_cache_get(c, extra, &sz) == NULL);
   CHECK(isize() == 32 + 40L * n_rec);

   /* A damaged entry in the middle: the entries before it are used, the records after it read. */
   disk_cache_destroy(c); c = NULL;
   poke(idx_path, 32 + 40 * 100 + 10, "\x42", 1);
   s = reopen(&c, "id-idx");
   CHECK(strstr(s, ": 2999 entries") && strstr(s, "100 records in one read, 2903 more read one by one, 2903 stale entries cut"));
   s = reopen(&c, "id-idx");
   CHECK(strstr(s, "index: 3003 records in one read") && !strstr(s, "more read"));

   /* An .idx of another .bin (another generation): rebuilt. */
   disk_cache_destroy(c); c = NULL;
   poke(path, 44, "\x01\x02\x03\x04", 4);
   s = reopen(&c, "id-idx");
   CHECK(strstr(s, ": 2999 entries") && strstr(s, "index rebuilt (the index file belongs to another cache file)"));
   s = reopen(&c, "id-idx");
   CHECK(strstr(s, "index: 3003 records in one read"));

   /* A .bin written before the index existed (generation 0): it gets one, and an index. */
   disk_cache_destroy(c); c = NULL;
   poke(path, 44, "\0\0\0\0", 4);
   s = reopen(&c, "id-idx");
   CHECK(strstr(s, ": 2999 entries") && strstr(s, "index rebuilt (the cache file predates the index): 3003 records"));
   s = reopen(&c, "id-idx");
   CHECK(strstr(s, "index: 3003 records in one read"));
   d = disk_cache_get(c, keys[2999], &sz); CHECK(d && memcmp(d, &(unsigned){2999}, 4) == 0); free(d);

   /* A damaged .idx header: rebuilt. */
   disk_cache_destroy(c); c = NULL;
   poke(idx_path, 3, "?", 1);
   s = reopen(&c, "id-idx");
   CHECK(strstr(s, "index rebuilt (the index file is damaged or of another version)"));

   /* Another driver build: both files emptied. */
   s = reopen(&c, "id-idx-2");
   CHECK(strstr(s, "another driver build") && isize() == 32 && fsize() == 48);
   disk_cache_destroy(c);
   unlink(saved);
}

static void test_store(void)
{
   unlink(path); unlink(idx_path);
   struct disk_cache *c = disk_cache_create("GM20B", "id-1", 0);
   CHECK(c);
   printf("status: %s\n", mesa_switch_shader_cache_status());
   cache_key k1, k2, k3;
   disk_cache_compute_key(c, "a", 1, k1);
   disk_cache_compute_key(c, "b", 1, k2);
   disk_cache_compute_key(c, "c", 1, k3);
   char big[100000];
   for (unsigned i = 0; i < sizeof(big); i++) big[i] = (char)(i * 7);
   disk_cache_put(c, k1, big, sizeof(big), NULL);
   disk_cache_put(c, k2, "hello", 6, NULL);
   disk_cache_put_key(c, k3);
   CHECK(disk_cache_has_key(c, k3));
   CHECK(!disk_cache_has_key(c, (uint8_t[20]){0}));
   size_t sz; void *d = disk_cache_get(c, k1, &sz);
   CHECK(d && sz == sizeof(big) && memcmp(d, big, sz) == 0); free(d);
   disk_cache_destroy(c);

   /* Reopen: persisted. */
   c = disk_cache_create("GM20B", "id-1", 0);
   printf("status: %s\n", mesa_switch_shader_cache_status());
   CHECK(strstr(mesa_switch_shader_cache_status(), ": 3 entries"));
   d = disk_cache_get(c, k2, &sz);
   CHECK(d && sz == 6 && strcmp(d, "hello") == 0); free(d);
   CHECK(disk_cache_has_key(c, k3));
   disk_cache_remove(c, k2);
   CHECK(disk_cache_get(c, k2, &sz) == NULL);
   disk_cache_destroy(c);
   c = disk_cache_create("GM20B", "id-1", 0);
   CHECK(disk_cache_get(c, k2, &sz) == NULL);   /* tombstone persisted */
   disk_cache_put(c, k2, "again", 6, NULL);      /* and can be stored again */
   disk_cache_destroy(c);
   c = disk_cache_create("GM20B", "id-1", 0);
   d = disk_cache_get(c, k2, &sz);
   CHECK(d && strcmp(d, "again") == 0); free(d);
   disk_cache_destroy(c);

   /* Torn tail: cut at the next start. */
   long before = fsize();
   FILE *f = fopen(path, "ab"); fwrite("\x52\x53\x43\x31garbage", 1, 11, f); fclose(f);
   c = disk_cache_create("GM20B", "id-1", 0);
   printf("status: %s\n", mesa_switch_shader_cache_status());
   CHECK(strstr(mesa_switch_shader_cache_status(), "cut 11 bytes"));
   CHECK(fsize() == before);
   d = disk_cache_get(c, k1, &sz); CHECK(d != NULL); free(d);
   disk_cache_destroy(c);

   /* A damaged payload: a miss, never bad data. */
   f = fopen(path, "r+b"); fseek(f, 48 + 32 + 500, SEEK_SET); fputc(0x5a ^ big[500], f); fclose(f);
   c = disk_cache_create("GM20B", "id-1", 0);
   uint64_t corrupt0 = mesa_switch_stat_read(MESA_SWITCH_CACHE_CORRUPT);
   CHECK(disk_cache_get(c, k1, &sz) == NULL);
   CHECK(mesa_switch_stat_read(MESA_SWITCH_CACHE_CORRUPT) == corrupt0 + 1);
   d = disk_cache_get(c, k2, &sz); CHECK(d != NULL); free(d);
   disk_cache_destroy(c);

   /* Another driver build: emptied. */
   c = disk_cache_create("GM20B", "id-2", 0);
   printf("status: %s\n", mesa_switch_shader_cache_status());
   CHECK(strstr(mesa_switch_shader_cache_status(), "another driver build"));
   CHECK(disk_cache_get(c, k2, &sz) == NULL);
   disk_cache_destroy(c);

   /* Garbage file: emptied. */
   f = fopen(path, "wb"); fwrite("nonsense", 1, 8, f); fclose(f);
   c = disk_cache_create("GM20B", "id-2", 0);
   CHECK(c && strstr(mesa_switch_shader_cache_status(), "not a cache file"));
   disk_cache_destroy(c);

   /* Size limit: dropped, not written. */
   setenv("MESA_GLSL_CACHE_MAX_SIZE", "1K", 1);
   c = disk_cache_create("GM20B", "id-2", 0);
   disk_cache_put(c, k1, big, 5000, NULL);
   CHECK(disk_cache_get(c, k1, &sz) == NULL);
   CHECK(mesa_switch_stat_read(MESA_SWITCH_CACHE_PUTS_DROPPED) >= 1);
   disk_cache_destroy(c);
   unsetenv("MESA_GLSL_CACHE_MAX_SIZE");

   setenv("MESA_SHADER_CACHE_DISABLE", "1", 1);
   CHECK(disk_cache_create("GM20B", "id-2", 0) == NULL);
   printf("status: %s\n", mesa_switch_shader_cache_status());
   unsetenv("MESA_SHADER_CACHE_DISABLE");
}

static const char *vs_text =
   "VERT\n"
   "DCL IN[0]\nDCL IN[1]\nDCL IN[2]\n"
   "DCL OUT[0], POSITION\nDCL OUT[1], COLOR\nDCL OUT[2], GENERIC[0]\n"
   "DCL CONST[0..3]\nDCL TEMP[0]\n"
   "  0: MUL TEMP[0], IN[0].xxxx, CONST[0]\n"
   "  1: MAD TEMP[0], IN[0].yyyy, CONST[1], TEMP[0]\n"
   "  2: MAD TEMP[0], IN[0].zzzz, CONST[2], TEMP[0]\n"
   "  3: MAD OUT[0], IN[0].wwww, CONST[3], TEMP[0]\n"
   "  4: MOV OUT[1], IN[1]\n"
   "  5: MOV OUT[2], IN[2]\n"
   "  6: END\n";
static const char *fs_text =
   "FRAG\n"
   "DCL IN[0], COLOR, COLOR\n"
   "DCL IN[1], GENERIC[0], PERSPECTIVE\n"
   "DCL OUT[0], COLOR\n"
   "DCL SAMP[0]\nDCL SVIEW[0], 2D, FLOAT\n"
   "DCL TEMP[0]\n"
   "  0: TEX TEMP[0], IN[1], SAMP[0], 2D\n"
   "  1: MUL OUT[0], TEMP[0], IN[0]\n"
   "  2: END\n";

static struct nvc0_program *make_prog(const char *text, unsigned type)
{
   static struct tgsi_token tokens[2][1024];
   struct tgsi_token *t = tokens[type == PIPE_SHADER_FRAGMENT];
   bool ok = tgsi_text_translate(text, t, 1024);
   assert(ok);
   struct nvc0_program *p = calloc(1, sizeof(*p));
   p->type = type;
   p->pipe.type = PIPE_SHADER_IR_TGSI;
   p->pipe.tokens = t;
   return p;
}

static void compare(const struct nvc0_program *a, const struct nvc0_program *b, const char *what)
{
   CHECK(a->code_size == b->code_size && a->code_size > 0);
   CHECK(memcmp(a->code, b->code, a->code_size) == 0);
   CHECK(memcmp(a->hdr, b->hdr, sizeof(a->hdr)) == 0);
   CHECK(memcmp(a->flags, b->flags, sizeof(a->flags)) == 0);
   CHECK(memcmp(&a->vp, &b->vp, sizeof(a->vp)) == 0);
   CHECK(memcmp(&a->fp, &b->fp, sizeof(a->fp)) == 0);
   CHECK(memcmp(&a->tp, &b->tp, sizeof(a->tp)) == 0);
   CHECK(a->num_gprs == b->num_gprs && a->need_tls == b->need_tls && a->num_barriers == b->num_barriers);
   unsigned nfix = 0;
   CHECK(compare_relocs_fixups(a->relocs, b->relocs, a->fixups, b->fixups, &nfix));
   printf("%s: code %u bytes, gprs %u, fixups %u, relocs %s\n", what, a->code_size, a->num_gprs,
          nfix, a->relocs ? "yes" : "none");
}

int main(void)
{
   setvbuf(stdout, NULL, _IONBF, 0);
   setenv("MESA_SHADER_CACHE_DIR", dir, 1);
   snprintf(path, sizeof(path), "%s/mesa_shader_cache.bin", dir);
   snprintf(idx_path, sizeof(idx_path), "%s/mesa_shader_cache.idx", dir);
   test_store();
   test_index();

   unlink(path);
   struct disk_cache *c = disk_cache_create("GM20B", "id-3", 0);
   const unsigned types[2] = {PIPE_SHADER_VERTEX, PIPE_SHADER_FRAGMENT};
   const char *texts[2] = {vs_text, fs_text};
   for (int i = 0; i < 2; i++) {
      printf("shader %d\n", i);
      struct nvc0_program *ref = make_prog(texts[i], types[i]);
      CHECK(nvc0_program_translate(ref, 0x12b, NULL, NULL));          /* no cache */
      struct nvc0_program *miss = make_prog(texts[i], types[i]);
      uint64_t hits0 = mesa_switch_stat_read(MESA_SWITCH_NVC0_CACHE_HITS);
      CHECK(nvc0_program_translate(miss, 0x12b, c, NULL));            /* stores */
      CHECK(mesa_switch_stat_read(MESA_SWITCH_NVC0_CACHE_HITS) == hits0);
      compare(ref, miss, "miss");
      disk_cache_destroy(c);
      c = disk_cache_create("GM20B", "id-3", 0);                       /* a new run */
      struct nvc0_program *hit = make_prog(texts[i], types[i]);
      CHECK(nvc0_program_translate(hit, 0x12b, c, NULL));
      CHECK(mesa_switch_stat_read(MESA_SWITCH_NVC0_CACHE_HITS) == hits0 + 1);
      compare(ref, hit, "hit");
      /* Different user clip planes: a different key. */
      if (types[i] == PIPE_SHADER_VERTEX) {
         struct nvc0_program *ucp = make_prog(texts[i], types[i]);
         ucp->vp.num_ucps = 2;
         CHECK(nvc0_program_translate(ucp, 0x12b, c, NULL));
         CHECK(mesa_switch_stat_read(MESA_SWITCH_NVC0_CACHE_HITS) == hits0 + 1);
         CHECK(ucp->code_size != ref->code_size);
      }
   }
   disk_cache_destroy(c);
   uint64_t s[MESA_SWITCH_STAT_COUNT];
   mesa_switch_get_stats(s, MESA_SWITCH_STAT_COUNT);
   printf("translates %llu (%.1f ms), cache hits %llu\n", (unsigned long long)s[MESA_SWITCH_NVC0_TRANSLATES],
          s[MESA_SWITCH_NVC0_TRANSLATE_NS] / 1e6, (unsigned long long)s[MESA_SWITCH_NVC0_CACHE_HITS]);
   printf(failures ? "FAILED (%d)\n" : "ALL OK\n", failures);
   return failures != 0;
}
