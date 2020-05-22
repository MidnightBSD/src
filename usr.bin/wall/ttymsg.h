/* $FreeBSD: stable/11/usr.bin/wall/ttymsg.h 335059 2018-06-13 13:41:23Z ed $ */

#define	TTYMSG_IOV_MAX	32

const char	*ttymsg(struct iovec *, int, const char *, int);
