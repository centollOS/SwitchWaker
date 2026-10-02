#include "fdlibm.h"

/* 8032FBC8-8032FBE8       .text pow */
double pow(double x, double y) {
    return __ieee754_pow(x, y);
}
