#include "scanf.h"
#if VERSION == VERSION_DEMO
#include "ctype.h"
#include "limits.h"
#include "mbstring.h"
#include "stdarg.h"
#include "strtoul.h"

enum argument_options {
    normal_argument,
    char_argument,
    short_argument,
    long_argument,
    long_long_argument,
    double_argument,
    long_double_argument,
    wchar_argument
};

typedef unsigned char char_map[32];

#define set_char_map(map, ch) map[(unsigned char)ch >> 3] |= (1 << (ch & 7))
#define tst_char_map(map, ch) (map[(unsigned char)ch >> 3] & (1 << (ch & 7)))

typedef struct {
    unsigned char suppress_assignment;
    unsigned char field_width_specified;
    unsigned char argument_options;
    unsigned char conversion_char;
    int field_width;
    char_map char_set;
} scan_format;

#define bad_conversion 0xFF

unsigned long long __strtoull(int base, int max_width, int (*ReadProc)(void*, int, int), void* ReadProcArg,
                              int* chars_scanned, int* negative, int* overflow);
long double __strtold(int max_width, int (*ReadProc)(void*, int, int), void* ReadProcArg, int* chars_scanned,
                      int* overflow);

int __sformatter(int (*ReadProc)(void*, int, int), void* ReadProcArg, const char* format_str, va_list arg);

static const char* parse_format(const char* format_string, scan_format* format) {
    const char* s = format_string;
    int c;
    int flag_found;
    int invert;
    scan_format f = {0, 0, normal_argument, 0, INT_MAX, {0}};

    if ((c = *++s) == '%') {
        f.conversion_char = c;
        *format = f;
        return s + 1;
    }

    if (c == '*') {
        f.suppress_assignment = 1;
        c = *++s;
    }

    if (isdigit(c)) {
        f.field_width = 0;

        do {
            f.field_width = (f.field_width * 10) + (c - '0');
            c = *++s;
        } while (isdigit(c));

        if (f.field_width == 0) {
            f.conversion_char = bad_conversion;
            *format = f;
            return s + 1;
        }

        f.field_width_specified = 1;
    }

    flag_found = 1;

    switch (c) {
    case 'h':
        f.argument_options = short_argument;

        if (s[1] == 'h') {
            f.argument_options = char_argument;
            c = *++s;
        }
        break;

    case 'l':
        f.argument_options = long_argument;

        if (s[1] == 'l') {
            f.argument_options = long_long_argument;
            c = *++s;
        }
        break;

    case 'L':
        f.argument_options = long_double_argument;
        break;

    default:
        flag_found = 0;
    }

    if (flag_found) {
        c = *++s;
    }

    f.conversion_char = c;

    switch (c) {
    case 'd':
    case 'i':
    case 'u':
    case 'o':
    case 'x':
    case 'X':
        if (f.argument_options == long_double_argument) {
            f.conversion_char = bad_conversion;
            break;
        }
        break;

    case 'a':
    case 'f':
    case 'e':
    case 'E':
    case 'g':
    case 'G':
        if (f.argument_options == char_argument || f.argument_options == short_argument ||
            f.argument_options == long_long_argument)
        {
            f.conversion_char = bad_conversion;
            break;
        }

        if (f.argument_options == long_argument) {
            f.argument_options = double_argument;
        }
        break;

    case 'p':
        f.argument_options = long_argument;
        f.conversion_char = 'x';
        break;

    case 'c':
        if (f.argument_options == long_argument) {
            f.argument_options = wchar_argument;
        } else if (f.argument_options != normal_argument) {
            f.conversion_char = bad_conversion;
        }
        break;

    case 's':
        if (f.argument_options == long_argument) {
            f.argument_options = wchar_argument;
        } else if (f.argument_options != normal_argument) {
            f.conversion_char = bad_conversion;
        }

        {
            int i;
            unsigned char* p;

            for (i = sizeof(f.char_set), p = f.char_set; i; --i) {
                *p++ = 0xFF;
            }

            f.char_set[1] = 0xC1;
            f.char_set[4] = 0xFE;
        }
        break;

    case '[':
        if (f.argument_options == long_argument) {
            f.argument_options = wchar_argument;
        } else if (f.argument_options != normal_argument) {
            f.conversion_char = bad_conversion;
        }

        c = *++s;

        invert = 0;

        if (c == '^') {
            invert = 1;
            c = *++s;
        }

        if (c == ']') {
            set_char_map(f.char_set, ']');
            c = *++s;
        }

        while (c && c != ']') {
            int d;

            set_char_map(f.char_set, c);

            if (*(s + 1) == '-' && (d = *(s + 2)) != 0 && d != ']') {
                while (++c <= d) {
                    set_char_map(f.char_set, c);
                }

                c = *(s += 3);
            } else {
                c = *++s;
            }
        }

        if (!c) {
            f.conversion_char = bad_conversion;
            break;
        }

        if (invert) {
            int i;
            unsigned char* p;

            for (i = sizeof(f.char_set), p = f.char_set; i; --i, ++p) {
                *p = ~*p;
            }
            break;
        }
        break;

    case 'n':
        break;

    default:
        f.conversion_char = bad_conversion;
        break;
    }

    *format = f;
    return s + 1;
}

int __sformatter(int (*ReadProc)(void*, int, int), void* ReadProcArg, const char* format_str, va_list arg) {
    int num_chars, chars_read, items_assigned, conversions;
    int base, negative, overflow;
    const char* format_ptr;
    char format_char;
    char c;
    scan_format format;
    long long_num;
    unsigned long u_long_num;
    long long long_long_num;
    unsigned long long u_long_long_num;
    long double long_double_num;
    char* arg_ptr;
    int rval;

    format_ptr = format_str;
    chars_read = 0;
    items_assigned = 0;
    conversions = 0;

    while ((format_char = *format_ptr) != 0) {
        if (isspace(format_char)) {
            do {
                format_char = *++format_ptr;
            } while (isspace(format_char));

            while (isspace(c = (*ReadProc)(ReadProcArg, 0, __GetAChar))) {
                ++chars_read;
            }

            (*ReadProc)(ReadProcArg, c, __UngetAChar);
            continue;
        }

        if (format_char != '%') {
            if ((unsigned char)format_char != (c = (*ReadProc)(ReadProcArg, 0, __GetAChar))) {
                (*ReadProc)(ReadProcArg, c, __UngetAChar);
                goto exit;
            }

            chars_read++;
            format_ptr++;
            continue;
        }

        format_ptr = parse_format(format_ptr, &format);

        if (!format.suppress_assignment && format.conversion_char != '%') {
            arg_ptr = va_arg(arg, char*);
        } else {
            arg_ptr = 0;
        }

        if ((format.conversion_char != 'n') && (*ReadProc)(ReadProcArg, 0, __TestForError)) {
            goto exit;
        }

        switch (format.conversion_char) {
        case 'd':
            base = 10;
            goto signed_int;

        case 'i':
            base = 0;

        signed_int:
            if (format.argument_options == long_long_argument) {
                u_long_long_num = __strtoull(base, format.field_width, ReadProc, ReadProcArg, &num_chars, &negative, &overflow);
            }
            if (format.argument_options != long_long_argument) {
                u_long_num = __strtoul(base, format.field_width, ReadProc, ReadProcArg, &num_chars, &negative, &overflow);
            }

            if (!num_chars) {
                goto exit;
            }

            chars_read += num_chars;

            if (format.argument_options == long_long_argument) {
                long_long_num = (negative ? -u_long_long_num : u_long_long_num);
            } else {
                long_num = (negative ? -u_long_num : u_long_num);
            }

            if (arg_ptr) {
                switch (format.argument_options) {
                case normal_argument:
                    *(int*)arg_ptr = long_num;
                    break;
                case char_argument:
                    *(signed char*)arg_ptr = long_num;
                    break;
                case short_argument:
                    *(short*)arg_ptr = long_num;
                    break;
                case long_argument:
                    *(long*)arg_ptr = long_num;
                    break;
                case long_long_argument:
                    *(long long*)arg_ptr = long_long_num;
                    break;
                }
                items_assigned++;
            }

            conversions++;
            break;

        case 'o':
            base = 8;
            goto unsigned_int;

        case 'u':
            base = 10;
            goto unsigned_int;

        case 'x':
        case 'X':
            base = 16;

        unsigned_int:
            if (format.argument_options == long_long_argument) {
                u_long_long_num = __strtoull(base, format.field_width, ReadProc, ReadProcArg, &num_chars, &negative, &overflow);
            }
            if (format.argument_options != long_long_argument) {
                u_long_num = __strtoul(base, format.field_width, ReadProc, ReadProcArg, &num_chars, &negative, &overflow);
            }

            if (!num_chars) {
                goto exit;
            }

            chars_read += num_chars;

            if (negative) {
                if (format.argument_options == long_long_argument) {
                    u_long_long_num = -u_long_long_num;
                } else {
                    u_long_num = -u_long_num;
                }
            }

            if (arg_ptr) {
                switch (format.argument_options) {
                case normal_argument:
                    *(unsigned int*)arg_ptr = u_long_num;
                    break;
                case char_argument:
                    *(unsigned char*)arg_ptr = u_long_num;
                    break;
                case short_argument:
                    *(unsigned short*)arg_ptr = u_long_num;
                    break;
                case long_argument:
                    *(unsigned long*)arg_ptr = u_long_num;
                    break;
                case long_long_argument:
                    *(unsigned long long*)arg_ptr = u_long_long_num;
                    break;
                }
                items_assigned++;
            }

            conversions++;
            break;

        case 'a':
        case 'f':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
            long_double_num = __strtold(format.field_width, ReadProc, ReadProcArg, &num_chars, &overflow);

            if (!num_chars) {
                goto exit;
            }

            chars_read += num_chars;

            if (arg_ptr) {
                switch (format.argument_options) {
                case normal_argument:
                    *(float*)arg_ptr = long_double_num;
                    break;
                case double_argument:
                    *(double*)arg_ptr = long_double_num;
                    break;
                case long_double_argument:
                    *(long double*)arg_ptr = long_double_num;
                    break;
                }
                items_assigned++;
            }

            conversions++;
            break;

        case 'c':
            if (!format.field_width_specified) {
                format.field_width = 1;
            }

            if (arg_ptr) {
                num_chars = 0;

                while (format.field_width-- && (rval = (*ReadProc)(ReadProcArg, 0, __GetAChar)) != -1) {
                    c = rval;
                    if (format.argument_options == wchar_argument) {
                        mbtowc((wchar_t*)arg_ptr, &c, 1);
                        arg_ptr++;
                    } else {
                        *arg_ptr++ = c;
                    }
                    num_chars++;
                }

                if (!num_chars) {
                    goto exit;
                }

                chars_read += num_chars;
                items_assigned++;
            } else {
                num_chars = 0;

                while (format.field_width-- && (c = (*ReadProc)(ReadProcArg, 0, __GetAChar)) != -1) {
                    num_chars++;
                }

                if (!num_chars) {
                    goto exit;
                }
            }

            conversions++;
            break;

        case '%':
            while (isspace(c = (*ReadProc)(ReadProcArg, 0, __GetAChar))) {
                chars_read++;
            }

            if (c != '%') {
                (*ReadProc)(ReadProcArg, c, __UngetAChar);
                goto exit;
            }

            chars_read++;
            break;

        case 's':
            c = (*ReadProc)(ReadProcArg, 0, __GetAChar);
            while (isspace(c)) {
                chars_read++;
                c = (*ReadProc)(ReadProcArg, 0, __GetAChar);
            }

            (*ReadProc)(ReadProcArg, c, __UngetAChar);

        case '[':
            if (arg_ptr) {
                num_chars = 0;

                while (format.field_width-- && (c = (*ReadProc)(ReadProcArg, 0, __GetAChar)) != -1 &&
                       tst_char_map(format.char_set, c))
                {
                    if (format.argument_options == wchar_argument) {
                        mbtowc((wchar_t*)arg_ptr, &c, 1);
                        arg_ptr = (char*)((wchar_t*)arg_ptr + 1);
                    } else {
                        *arg_ptr++ = c;
                    }
                    num_chars++;
                }

                if (!num_chars) {
                    (*ReadProc)(ReadProcArg, c, __UngetAChar);
                    goto exit;
                }

                chars_read += num_chars;

                if (format.argument_options == wchar_argument) {
                    *(wchar_t*)arg_ptr = L'\0';
                } else {
                    *arg_ptr = 0;
                }

                items_assigned++;
            } else {
                num_chars = 0;

                while (format.field_width-- && (c = (*ReadProc)(ReadProcArg, 0, __GetAChar)) != -1 &&
                       tst_char_map(format.char_set, c))
                {
                    num_chars++;
                }

                if (!num_chars) {
                    (*ReadProc)(ReadProcArg, c, __UngetAChar);
                    goto exit;
                }

                chars_read += num_chars;
            }

            if (format.field_width >= 0) {
                (*ReadProc)(ReadProcArg, c, __UngetAChar);
            }

            conversions++;
            break;

        case 'n':
            if (arg_ptr) {
                switch (format.argument_options) {
                case normal_argument:
                    *(int*)arg_ptr = chars_read;
                    break;
                case short_argument:
                    *(short*)arg_ptr = chars_read;
                    break;
                case long_argument:
                    *(long*)arg_ptr = chars_read;
                    break;
                case char_argument:
                    *(char*)arg_ptr = chars_read;
                    break;
                case long_long_argument:
                    *(long long*)arg_ptr = chars_read;
                    break;
                }
            }
            continue;

        case bad_conversion:
        default:
            goto exit;
        }
    }

exit:
    if ((*ReadProc)(ReadProcArg, 0, __TestForError) && conversions == 0) {
        return -1;
    }

    return items_assigned;
}
#endif

int __StringRead(void* pPtr, int ch, int act) {
    char ret;
    __InStrCtrl* Iscp = (__InStrCtrl*)pPtr;

    switch (act) {
    case __GetAChar:
        ret = *(Iscp->NextChar);

        if (ret == '\0') {
            Iscp->NullCharDetected = 1;
            return -1;
        } else {
            Iscp->NextChar++;
            return (unsigned char)ret;
        }

    case __UngetAChar:
        if (Iscp->NullCharDetected == 0) {
            Iscp->NextChar--;
        } else {
            Iscp->NullCharDetected = 0;
        }

        return ch;

    case __TestForError:
        return Iscp->NullCharDetected;
    }

    return 0;
}

#if VERSION == VERSION_DEMO
inline int vsscanf(const char* s, const char* format, va_list arg) {
    __InStrCtrl isc;
    isc.NextChar = (char*)s;
    if ((s == 0) || (*isc.NextChar == '\0')) {
        return -1;
    }
    isc.NullCharDetected = 0;
    return __sformatter(&__StringRead, (void*)&isc, format, arg);
}

int sscanf(const char* s, const char* format, ...) {
    va_list args;
    va_start(args, format);
    return vsscanf(s, format, args);
}
#endif
