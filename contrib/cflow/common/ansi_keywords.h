/*-
 * Copyright (c) 2007, Marcus von Appen
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

/*
 * ANSI keywords as defined by ISO C89.
 */
static const char* ansi_keywords[] = 
{
    "assert", "acos", "asin", "atan", "atan2", "atof", "atoi", "atol",
    "abort", "atexit", "abs", "asctime",
    
    "bsearch", 
    
    "cos", "cosh", "ceil", "clearerr", "calloc", "clock", "ctime", 
    
    "div", "difftime",
    
    "exp", "exit", "errno",

    "fabs", "frexp", "floor", "fmod", "fclose", "fflush", "fopen", "freopen",
    "fprintf", "fscanf", "fgetc", "fgets", "fputc", "fputs", "fread", "fwrite",
    "fgetpos", "fsetpos", "fseek", "ftell", "feof",  "ferror", "free",

    "getc", "getchar", "gets", "getenv", "gmtime",
    
    "isalpha", "isalnum", "iscntrl", "isdigit", "isgraph", "islower",
    "isprint", "ispunct", "isspace", "isupper", "isxdigit", "isascii",
    
    "localeconv", "ldexp", "log", "log10", "longjmp", "labs", "ldiv",
    "localtime",
    
    "modf", "malloc", "mblen", "mbtowc", "mbtowcs", "memcpy", "memmove",
    "memset", "memcmp", "memchr", "mktime", 
    
    "offsetof",
    
    "pow", "printf", "putc", "putchar", "puts", "perror",
    
    "qsort",
    
    "raise", "remove", "rename", "rewind", "rand", "realloc",
    
    "setlocale", "sin", "sinh", "sqrt", "setjmp", "signal", "setbuf",
    "setvbuf", "sprintf",  "scanf", "sscanf", "strtod", "strtol", "strtoul",
    "srand", "system", "strcpy", "strncpy", "strcat", "strncat", "strcmp",
    "strncmp", "strcoll", "strxfrm", "strchr", "strcspn", "strpbrk",
    "strrchr", "strstr", "strspn", "strtok", "strerror", "strlen", "strftime",
    
    "tan", "tanh", "tolower", "toupper", "tmpfile", "tmpnam", "time",
    
    "ungetc",
    
    "va_arg", "va_start", "va_end", "vfprintf", "vprintf", "vsprintf",
    
    "wctomb", "wcstombs", 

    NULL
};
