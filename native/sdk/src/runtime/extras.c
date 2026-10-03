/* cos_sdk: MSL-only C library extras the host C library lacks (docs/NATIVE_PORT_PHASE2_3.md,
 * step 2.6f).
 *
 * MSL's <extras.h> declares stricmp and strnicmp (case-insensitive compares); macOS and Linux
 * only have strcasecmp/strncasecmp. No other MSL-only name is needed: a scan of all 840 compiled
 * game units (REL units included) against cos_sdk, the Aurora libraries and the host C/C++
 * libraries leaves no MSL/runtime symbol unresolved, and JSystem/JAudio and JAZelAudio (compiled
 * in phase 3.7) call none either. __dcbz and __cntlzw, which Dusklight defines next to these,
 * are inline in native/include/pc/cos_pc_config.h.
 *
 * Both return -1, 0 or 1 and compare the lower-cased characters, as MSL's do. The characters go
 * through unsigned char before tolower: char is unsigned on the GameCube's PowerPC, so bytes
 * >= 0x80 compare by their unsigned value as they did there (and tolower of a negative char, as
 * the host's signed char would give, is undefined).
 *
 * Provenance: adapted from Dusklight src/dusk/extras.c (CC0, ref/dusklight), keeping only the
 * string functions. Changed: the unsigned char conversion, and strnicmp's locals are ints.
 */
#include <ctype.h>

int stricmp(const char* str1, const char* str2);
int strnicmp(const char* str1, const char* str2, int n);

int stricmp(const char* str1, const char* str2) {
    int a;
    int b;

    do {
        b = tolower((unsigned char)*str1++);
        a = tolower((unsigned char)*str2++);

        if (b < a) {
            return -1;
        }
        if (b > a) {
            return 1;
        }
    } while (b != 0);

    return 0;
}

int strnicmp(const char* str1, const char* str2, int n) {
    int i;
    int c1;
    int c2;

    for (i = 0; i < n; i++) {
        c1 = tolower((unsigned char)*str1++);
        c2 = tolower((unsigned char)*str2++);

        if (c1 < c2) {
            return -1;
        }
        if (c1 > c2) {
            return 1;
        }
        if (c1 == '\0') {
            return 0;
        }
    }

    return 0;
}
