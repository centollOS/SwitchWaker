#include <cstring>
#include "codegen/nv50_ir_target.h"
using namespace nv50_ir;
extern "C" int compare_relocs_fixups(const void *ra, const void *rb, const void *fa, const void *fb,
                                     unsigned *nfix)
{
   const RelocInfo *a = (const RelocInfo *)ra, *b = (const RelocInfo *)rb;
   if (!a != !b) return 0;
   if (a && (a->count != b->count || a->codePos != b->codePos || a->libPos != b->libPos ||
             a->dataPos != b->dataPos || memcmp(a->entry, b->entry, sizeof(RelocEntry) * a->count)))
      return 0;
   const FixupInfo *x = (const FixupInfo *)fa, *y = (const FixupInfo *)fb;
   if (!x != !y) return 0;
   *nfix = x ? x->count : 0;
   if (x) {
      if (x->count != y->count) return 0;
      for (unsigned i = 0; i < x->count; i++)
         if (x->entry[i].val != y->entry[i].val || x->entry[i].apply != y->entry[i].apply) return 0;
   }
   return 1;
}
extern "C" void nouveau_drm_screen_unref(void *) {}
