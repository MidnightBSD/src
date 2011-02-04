# $Id: Makefile,v 1.1 2011-02-04 14:36:14 laffer1 Exp $

sysconfdir=/etc/ssh
piddir=/var/run
srcdir=.
top_srcdir=..


CC=gcc
LD=gcc
CFLAGS=-g -O2 -Wall -Wpointer-arith -Wuninitialized -Wsign-compare -Wformat-security -Wno-pointer-sign -fno-strict-aliasing -fno-builtin-memset -fstack-protector-all 
CPPFLAGS=-I. -I.. -I$(srcdir) -I$(srcdir)/..  -DHAVE_CONFIG_H
LIBS=-lcrypto -lutil -lz  -lcrypt
AR=/usr/bin/ar
RANLIB=ranlib
INSTALL=/usr/bin/install -c
LDFLAGS=-L.  -fstack-protector-all

OPENBSD=base64.o basename.o bindresvport.o daemon.o dirname.o fmt_scaled.o getcwd.o getgrouplist.o getopt.o getrrsetbyname.o glob.o inet_aton.o inet_ntoa.o inet_ntop.o mktemp.o pwcache.o readpassphrase.o realpath.o rresvport.o setenv.o setproctitle.o sha2.o sigact.o strlcat.o strlcpy.o strmode.o strptime.o strsep.o strtonum.o strtoll.o strtoul.o timingsafe_bcmp.o vis.o

COMPAT=bsd-arc4random.o bsd-asprintf.o bsd-closefrom.o bsd-cray.o bsd-cygwin_util.o bsd-getpeereid.o bsd-misc.o bsd-nextstep.o bsd-openpty.o bsd-poll.o bsd-snprintf.o bsd-statvfs.o bsd-waitpid.o fake-rfc2553.o openssl-compat.o xmmap.o xcrypt.o

PORTS=port-aix.o port-irix.o port-linux.o port-solaris.o port-tun.o port-uw.o

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

all: libopenbsd-compat.a

$(COMPAT): ../config.h
$(OPENBSD): ../config.h
$(PORTS): ../config.h

libopenbsd-compat.a:  $(COMPAT) $(OPENBSD) $(PORTS)
	$(AR) rv $@ $(COMPAT) $(OPENBSD) $(PORTS)
	$(RANLIB) $@

clean:
	rm -f *.o *.a core 

distclean: clean
	rm -f Makefile *~
