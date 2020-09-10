#!/bin/sh
#
# $FreeBSD: stable/11/tools/regression/redzone9/test.sh 155087 2006-01-31 11:20:13Z pjd $

sysctl debug.redzone.malloc_underflow=1
sysctl debug.redzone.malloc_overflow=1
sysctl debug.redzone.realloc_smaller_underflow=1
sysctl debug.redzone.realloc_smaller_overflow=1
sysctl debug.redzone.realloc_bigger_underflow=1
sysctl debug.redzone.realloc_bigger_overflow=1
