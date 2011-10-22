/*-
 * Copyright (c) 2007-2008, Marcus von Appen
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

static const char* c99_keywords[] = 
{
    "acosh", "acoshf", "acoshl", "asinh", "asinhf", "asinhl", "atanh",
    "atanhl", "atanhf", "atoll",

    "cabs", "cabsf", "cabsl", "cacos", "cacosf", "cacosh", "cacosl", "cacoshl",
    "cacoshf", "carg", "cargf", "cargl", "casin", "casinf", "casinh", "casinhf",
    "casinhl", "casinl", "catan", "catanf", "catanh", "catanhf", "catanhl",
    "catanl", "cbrt", "cbrtf", "cbrtl", "ccos", "ccosf", "ccosh", "ccoshf",
    "ccoshl", "ccosl", "cexp", "cexpf", "cexpl", "cimag", "cimag", "cimagf",
    "cimagl", "clog", "clogf", "clogl", "conj", "conj", "conjf", "conjl",
    "copysign", "copysign", "copysignf", "copysignl", "cpow", "cpowf", "cpowl",
    "cproj", "cprojf", "cprojl", "creal", "crealf", "creall", "csin", "csinf",
    "csinh", "csinhf", "csinhl", "csinl", "csqrt", "csqrtf", "csqrtl", "ctan",
    "ctanf", "ctanh", "ctanhf", "ctanhl", "ctanl",

    "erf", "erfc", "erfcf", "erfcl", "erff", "erfl", "exp2", "exp2f", "exp2l",
    "expm1", "expm1f", "expm1l", 

    "fdim", "fdimf", "fdiml", "feclearexept", "fegetenv", "fegetexceptflag",
    "fegetround", "feholdexcept", "feraiseexcept", "fesetenv",
    "fesetexceptflag", "fesetround", "fetestexcept", "feupdateenv", "fma",
    "fmaf", "fmal", "fmax", "fmaxf", "fmaxl", "fmin", "fminf", "fminl",
    "fpclassify", 

    "hypot", "hypotf", "hypotl",

    "ilogb", "ilogbf", "ilogbl", "imaginary", "imaxabs", "imaxdiv", "isblank",
    "isfinite", "isgreater", "isgreaterequal", "isinf", "isless", "islessequal",
    "islessgreater", "isnan", "isnormal", "isunordered", "iswblank",

    "lgamma", "lgammaf", "lgammal", "llabs", "lldiv", "llrint", "llrintf",
    "llrintl", "llround", "llroundf", "llroundl", "log1p", "log1pf", "log1pl",
    "log2", "log2f", "log2l", "logb", "logbf", "logbl", "lrint", "lrintf",
    "lrintl", "lround", "lroundf", "lroundl",

    "nan", "nanf", "nanl", "nearbyint", "nearbyintf", "nearbyintl", "nextafter",
    "nextafterf", "nextafterl", "nexttoward", "nexttowardf", "nexttowardl",

    "powf", "powl",

    "remainder", "remainderf", "remainderl", "remquo", "remquof", "remquol",
    "rint", "rintf", "rintl", "round", "roundf", "roundl", 

    "scalbln", "scalblnf", "scalblnl", "scalbn", "scalbnf", "scalbnl", "sinf",
    "sinl", "sinhf", "sinhl", "sqrtf", "sqrtl", "snprintf", "strtof",
    "strtoimax", "strtold", "strtoll", "strtoull", "strtoumax",

    "tanf", "tanl", "tanhf", "tanhl", "tgamma", "tgammaf", "tgammal", "trunc",
    "truncf", "truncl",

    "va_copy", "vfscanf", "vfwscanf", "vscanf", "vsnprintf", "vsscanf",
    "vswscanf", "vwscanf",

    "wcstof", "wcstoimax", "wcstold", "wcstoll", "wcstoull", "wcstoumax",

    NULL
};
