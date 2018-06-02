# $MidnightBSD$
# $FreeBSD: stable/10/sys/boot/Makefile.inc 319025 2017-05-28 01:14:59Z ngie $

SSP_CFLAGS=

# The boot loader build uses dd status=none, where possible, for reproducible
# build output (since performance varies from run to run). Trouble is that
# option was recently (10.3) added to FreeBSD and is non-standard. Only use it
# when this test succeeds rather than require dd to be a bootstrap tool.
DD_NOSTATUS!=(dd status=none count=0 2> /dev/null && echo status=none) || true
DD=dd ${DD_NOSTATUS}
