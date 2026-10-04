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

static long fsize(void) { struct stat st; return stat(path, &st) == 0 ? st.st_size : -1; }

static void test_store(void)
{
   unlink(path);
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
   test_store();

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
