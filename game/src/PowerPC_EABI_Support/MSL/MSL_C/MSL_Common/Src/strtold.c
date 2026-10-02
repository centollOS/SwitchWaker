#include "ansi_fp.h"
#include "ctype.h"
#include "float.h"
#include "limits.h"
#include "math.h"
#include "scanf.h"
#include "string.h"

struct lconv {
    char* decimal_point;
    char* thousands_sep;
    char* grouping;
    char* mon_decimal_point;
    char* mon_thousands_sep;
    char* mon_grouping;
    char* positive_sign;
    char* negative_sign;
    char* currency_symbol;
    char frac_digits;
    char p_cs_precedes;
    char n_cs_precedes;
    char p_sep_by_space;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
    char* int_curr_symbol;
    char int_frac_digits;
    char int_p_cs_precedes;
    char int_n_cs_precedes;
    char int_p_sep_by_space;
    char int_n_sep_by_space;
    char int_p_sign_posn;
    char int_n_sign_posn;
};

extern struct lconv __lconv;

extern unsigned long __double_huge[];
extern unsigned long __extended_min[];
extern unsigned long __extended_max[];

#define HUGE_VAL (*(double*)__double_huge)
#define LDBL_MIN (*(long double*)__extended_min)
#define LDBL_MAX (*(long double*)__extended_max)

enum scan_states {
    start = 0x0001,
    sig_start = 0x0002,
    leading_sig_zeroes = 0x0004,
    int_digit_loop = 0x0008,
    frac_start = 0x0010,
    frac_digit_loop = 0x0020,
    sig_end = 0x0040,
    exp_start = 0x0080,
    leading_exp_digit = 0x0100,
    leading_exp_zeroes = 0x0200,
    exp_digit_loop = 0x0400,
    finished = 0x0800,
    failure = 0x1000,
    nan_state = 0x2000,
    infin_state = 0x4000,
    hex_state = 0x8000
};

enum hex_scan_states {
    not_hex = 0x0000,
    hex_start = 0x0001,
    hex_leading_sig_zeroes = 0x0002,
    hex_int_digit_loop = 0x0004,
    hex_frac_digit_loop = 0x0008,
    hex_sig_end = 0x0010,
    hex_exp_start = 0x0020,
    hex_leading_exp_digit = 0x0040,
    hex_leading_exp_zeroes = 0x0080,
    hex_exp_digit_loop = 0x0100
};

#define final_state(scan_state) (scan_state & (finished | failure))
#define success(scan_state) (scan_state & (leading_sig_zeroes | int_digit_loop | frac_digit_loop | leading_exp_zeroes | exp_digit_loop | finished))
#define fetch() (count++, (*ReadProc)(ReadProcArg, 0, __GetAChar))
#define unfetch(c) (*ReadProc)(ReadProcArg, c, __UngetAChar)

long double __strtold(int max_width, int (*ReadProc)(void*, int, int), void* ReadProcArg, int* chars_scanned, int* overflow) {
    int scan_state = start;
    int hex_scan_state = not_hex;
    int count = 0;
    int spaces = 0;
    int c;
    decimal d = {0, 0, 0, {0, ""}};
    int sig_negative = 0;
    int exp_negative = 0;
    long exp_value = 0;
    int exp_adjust = 0;
    long double result;
    int sign_detected = 0;

    unsigned char* chptr = (unsigned char*)&result;
    unsigned char uch, uch1;
    int ui;
    int chindex;
    int NibbleIndex;
    int expsign = 0;
    int exp_digits = 0;
    unsigned intdigits = 0;
    short exp = 0;
    int radix_marker = *(unsigned char*)__lconv.decimal_point;

    *overflow = 0;
    c = fetch();

    while (count <= max_width && c != EOF && !final_state(scan_state)) {
        switch (scan_state) {
        case start:
            if (isspace(c)) {
                c = fetch();
                count--;
                spaces++;
                break;
            }

            switch (toupper(c)) {
            case '-':
                sig_negative = 1;
            case '+':
                c = fetch();
                sign_detected = 1;
                break;
            case 'I':
                c = fetch();
                scan_state = infin_state;
                break;
            case 'N':
                c = fetch();
                scan_state = nan_state;
                break;
            default:
                scan_state = sig_start;
                break;
            }
            break;

        case infin_state: {
            int i = 1;
            char model[] = "INFINITY";

            while ((i < 8) && (toupper(c) == model[i])) {
                i++;
                c = fetch();
            }

            if ((i == 3) || (i == 8)) {
                if (sig_negative) {
                    result = -HUGE_VALF;
                } else {
                    result = HUGE_VALF;
                }

                *chars_scanned = spaces + i + sign_detected;
                return result;
            } else {
                scan_state = failure;
            }
            break;
        }

        case nan_state: {
            int i = 1;
            int j = 0;
            char model[] = "NAN(";

            while ((i < 4) && (toupper(c) == model[i])) {
                i++;
                c = fetch();
            }

            if ((i == 3) || (i == 4)) {
                if (i == 4) {
                    while ((j < 32) && (isdigit(c) || isalpha(c))) {
                        j++;
                        c = fetch();
                    }

                    if (c != ')') {
                        scan_state = failure;
                        break;
                    } else {
                        j++;
                    }
                }

                if (sig_negative) {
                    result = -NAN;
                } else {
                    result = NAN;
                }

                *chars_scanned = spaces + i + j + sign_detected;
                return result;
            } else {
                scan_state = failure;
            }
            break;
        }

        case sig_start:
            if (c == radix_marker) {
                scan_state = frac_start;
                c = fetch();
                break;
            }

            if (!isdigit(c)) {
                scan_state = failure;
                break;
            }

            if (c == '0') {
                c = fetch();
                if (toupper(c) == 'X') {
                    scan_state = hex_state;
                    hex_scan_state = hex_start;
                } else {
                    scan_state = leading_sig_zeroes;
                }
                break;
            }

            scan_state = int_digit_loop;
            break;

        case leading_sig_zeroes:
            if (c == '0') {
                c = fetch();
                break;
            }

            scan_state = int_digit_loop;
            break;

        case int_digit_loop:
            if (!isdigit(c)) {
                if (c == radix_marker) {
                    scan_state = frac_digit_loop;
                    c = fetch();
                } else {
                    scan_state = sig_end;
                }
                break;
            }

            if (d.sig.length < 20) {
                d.sig.text[d.sig.length++] = c;
            } else {
                exp_adjust++;
            }

            c = fetch();
            break;

        case frac_start:
            if (!isdigit(c)) {
                scan_state = failure;
                break;
            }

            scan_state = frac_digit_loop;
            break;

        case frac_digit_loop:
            if (!isdigit(c)) {
                scan_state = sig_end;
                break;
            }

            if (d.sig.length < 20) {
                if (c != '0' || d.sig.length) {
                    d.sig.text[d.sig.length++] = c;
                }
                exp_adjust--;
            }

            c = fetch();
            break;

        case sig_end:
            if (toupper(c) == 'E') {
                scan_state = exp_start;
                c = fetch();
                break;
            }

            scan_state = finished;
            break;

        case exp_start:
            if (c == '+') {
                c = fetch();
            } else if (c == '-') {
                c = fetch();
                exp_negative = 1;
            }

            scan_state = leading_exp_digit;
            break;

        case leading_exp_digit:
            if (!isdigit(c)) {
                scan_state = failure;
                break;
            }

            if (c == '0') {
                scan_state = leading_exp_zeroes;
                c = fetch();
                break;
            }

            scan_state = exp_digit_loop;
            break;

        case leading_exp_zeroes:
            if (c == '0') {
                c = fetch();
                break;
            }

            scan_state = exp_digit_loop;
            break;

        case exp_digit_loop:
            if (!isdigit(c)) {
                scan_state = finished;
                break;
            }

            exp_value = (exp_value * 10) + (c - '0');
            if (exp_value > SHRT_MAX) {
                *overflow = 1;
            }

            c = fetch();
            break;

        case hex_state:
            switch (hex_scan_state) {
            case hex_start:
                for (ui = 0; ui < 8; ui++) {
                    chptr[ui] = 0;
                }
                NibbleIndex = 2;
                hex_scan_state = hex_leading_sig_zeroes;
                c = fetch();
                break;

            case hex_leading_sig_zeroes:
                if (c == '0') {
                    c = fetch();
                    break;
                }

                hex_scan_state = hex_int_digit_loop;
                break;

            case hex_int_digit_loop:
                if (!isxdigit(c)) {
                    if (c == radix_marker) {
                        hex_scan_state = hex_frac_digit_loop;
                        c = fetch();
                    } else {
                        hex_scan_state = hex_sig_end;
                    }
                    break;
                }

                if (NibbleIndex < 17) {
                    intdigits++;
                    uch = *(chptr + NibbleIndex / 2);
                    ui = toupper(c);

                    if (ui >= 'A') {
                        ui = ui - 'A' + 10;
                    } else {
                        ui -= '0';
                    }

                    uch1 = ui;

                    if ((NibbleIndex % 2) != 0) {
                        uch |= uch1;
                    } else {
                        uch |= (uch1 << 4);
                    }

                    *(chptr + NibbleIndex++ / 2) = uch;
                    c = fetch();
                } else {
                    c = fetch();
                }
                break;

            case hex_frac_digit_loop:
                if (!isxdigit(c)) {
                    hex_scan_state = hex_sig_end;
                    break;
                }

                if (NibbleIndex < 17) {
                    uch = *(chptr + NibbleIndex / 2);
                    chindex = toupper(c);

                    if (chindex >= 'A') {
                        chindex = chindex - 'A' + 10;
                    } else {
                        chindex -= '0';
                    }

                    uch1 = chindex;

                    if ((NibbleIndex % 2) != 0) {
                        uch |= uch1;
                    } else {
                        uch |= (uch1 << 4);
                    }

                    *(chptr + NibbleIndex++ / 2) = uch;
                    c = fetch();
                } else {
                    c = fetch();
                }
                break;

            case hex_sig_end:
                if (toupper(c) == 'P') {
                    hex_scan_state = hex_exp_start;
                    exp_digits++;
                    c = fetch();
                } else {
                    scan_state = finished;
                }
                break;

            case hex_exp_start:
                exp_digits++;
                if (c == '-') {
                    expsign = 1;
                } else if (c != '+') {
                    unfetch(c);
                    exp_digits--;
                }

                hex_scan_state = hex_leading_exp_digit;
                c = fetch();
                break;

            case hex_leading_exp_digit:
                if (!isdigit(c)) {
                    scan_state = failure;
                    break;
                }

                if (c == '0') {
                    hex_scan_state = hex_leading_exp_zeroes;
                    exp_digits++;
                    c = fetch();
                    break;
                }

                hex_scan_state = hex_exp_digit_loop;
                break;

            case hex_exp_digit_loop:
                if (!isdigit(c)) {
                    scan_state = finished;
                    break;
                }

                exp = (exp * 10) + (c - '0');
                if (exp_value > SHRT_MAX) {
                    *overflow = 1;
                }

                exp_digits++;
                c = fetch();
                break;
            }
            break;
        }
    }

    if (!success(scan_state)) {
        *chars_scanned = 0;
    } else {
        *chars_scanned = count - 1 + spaces;
    }

    unfetch(c);

    if (hex_scan_state == not_hex) {
        if (exp_negative) {
            exp_value = -exp_value;
        }

        {
            int n = d.sig.length;
            unsigned char* p = &d.sig.text[n];

            while (n-- && *--p == '0') {
                exp_adjust++;
            }

            d.sig.length = n + 1;

            if (d.sig.length == 0) {
                d.sig.text[d.sig.length++] = '0';
            }
        }

        exp_value += exp_adjust;

        if (exp_value < SHRT_MIN || exp_value > SHRT_MAX) {
            *overflow = 1;
        }

        if (*overflow) {
            if (exp_negative) {
                return 0.0;
            } else if (sig_negative) {
                return -HUGE_VAL;
            } else {
                return HUGE_VAL;
            }
        }

        d.exp = exp_value;

        result = __dec2num(&d);

        if (result != 0.0 && result < LDBL_MIN) {
            *overflow = 1;
        } else if (result > LDBL_MAX) {
            *overflow = 1;
            result = HUGE_VAL;
        }

        if (sig_negative && success(scan_state)) {
            result = -result;
        }

        return result;
    } else {
        unsigned long long* uptr = (unsigned long long*)&result;

        if (result) {
            if (expsign) {
                exp = -exp;
            }

            while ((*(short*)(&result) & 0x00F0) != 0x0010) {
                *uptr = (*uptr) >> 1;
                exp++;
            }

            exp += 4 * (intdigits - 1);
            *(short*)&result &= 0x000F;
            *(short*)&result |= ((exp + 1023) << 4);
            *chars_scanned = spaces + sign_detected + NibbleIndex + 1 + exp_digits;

            if (result != 0.0 && result < LDBL_MIN) {
                *overflow = 1;
                result = 0.0;
            } else if (result > LDBL_MAX) {
                *overflow = 1;
                result = HUGE_VAL;
            }

            if (sig_negative) {
                *(short*)&result |= 0x8000;
            }
        } else {
            result = 0.0;
        }

        return result;
    }
}
