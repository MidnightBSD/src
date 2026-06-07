# Opt-in AddressSanitizer build, shared by libmport and the mport CLI.
#
# Usage:
#	make WITH_ASAN=1            (build component with ASan)
#
# AddressSanitizer instruments memory accesses and aborts on heap buffer
# overflows, use-after-free, double free, and free() of a non-heap/interior
# pointer.  It is the regression net for the kinds of vector-handling bugs in
# the CLI (e.g. freeing an advanced pointer).
#
# NOTE: LeakSanitizer (ASAN_OPTIONS=detect_leaks=1) is NOT supported on
# MidnightBSD, so this lane does NOT report plain "allocated but never freed"
# leaks -- only active memory errors.  Verify leaks by code tracing.
#
# Because libmport.so is instrumented under this knob, only ASan-linked
# executables (the mport CLI built with the same WITH_ASAN) can load it; build
# and run the libexec tools in a normal, non-ASan tree.

.if defined(WITH_ASAN)
CFLAGS+=	-fsanitize=address -fno-omit-frame-pointer -g
LDFLAGS+=	-fsanitize=address
.endif
