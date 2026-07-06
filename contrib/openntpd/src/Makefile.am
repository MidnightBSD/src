#
# Copyright (c) 2014 Brent Cook
# Parts based on Makefile.in from Portable OpenNTPD:
# Copyright (c) 2004-2008 Darren Tucker.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CPPFLAGS += -DYYSTYPE_IS_DECLARED

ACLOCAL_AMFLAGS = -Im4
CLEANFILES = parse.c ntpctl.8 ntpd.8 ntpd.conf.5

do_subst = sed \
	-e 's,[@]sysconfdir[@],$(sysconfdir),g' \
	-e 's,[@]localstatedir[@],$(localstatedir),g' \
	-e 's,[@]runstatedir[@],$(runstatedir),g' \
	-e 's,[@]sbindir[@],$(sbindir),g'

ntpd.8: $(srcdir)/ntpd.8.in Makefile
	$(do_subst) < $(srcdir)/ntpd.8.in > ntpd.8

ntpd.conf.5: $(srcdir)/ntpd.conf.5.in Makefile
	$(do_subst) < $(srcdir)/ntpd.conf.5.in > ntpd.conf.5

ntpctl.8: $(srcdir)/ntpctl.8.in Makefile
	$(do_subst) < $(srcdir)/ntpctl.8.in > ntpctl.8

sbin_PROGRAMS = ntpd
nodist_man_MANS = ntpctl.8 ntpd.8 ntpd.conf.5
EXTRA_DIST = ntpctl.8.in ntpd.8.in ntpd.conf.5.in

ntpd_CFLAGS = $(CFLAGS)
ntpd_CFLAGS += -DSYSCONFDIR=\"$(sysconfdir)\"
ntpd_CFLAGS += -DLOCALSTATEDIR=\"$(localstatedir)\"
ntpd_CFLAGS += -DRUNSTATEDIR=\"$(runstatedir)\"

ntpd_LDADD = $(PLATFORM_LDADD) $(PROG_LDADD) -lm
ntpd_LDADD += $(top_builddir)/compat/libcompat.la
if !HAVE_EXPLICIT_BZERO
ntpd_LDADD += $(top_builddir)/compat/libcompatnoopt.la
endif

ntpd_SOURCES = client.c
ntpd_SOURCES += config.c
if HAVE_LIBTLS
ntpd_SOURCES += constraint.c
else
ntpd_SOURCES += constraint-disabled.c
endif
ntpd_SOURCES += control.c
ntpd_SOURCES += log.c
ntpd_SOURCES += log.h
ntpd_SOURCES += ntp.c
ntpd_SOURCES += ntp.h
ntpd_SOURCES += ntp_dns.c
ntpd_SOURCES += ntp_msg.c
ntpd_SOURCES += ntpd.c
ntpd_SOURCES += ntpd.h
ntpd_SOURCES += parse.y
if HAVE_SENSORS
ntpd_SOURCES += sensors.c
else
ntpd_SOURCES += fake-sensors.c
endif
ntpd_SOURCES += server.c
ntpd_SOURCES += util.c

install-exec-hook:
	@ln -f "$(DESTDIR)$(sbindir)/ntpd" "$(DESTDIR)$(sbindir)/ntpctl"
	@if [ ! -d "$(DESTDIR)$(sysconfdir)" ]; then \
		$(INSTALL) -m 755 -d "$(DESTDIR)$(sysconfdir)"; \
	fi
	@if [ ! -d "$(DESTDIR)$(localstatedir)" ]; then \
		$(INSTALL) -m 755 -d "$(DESTDIR)$(localstatedir)"; \
	fi
	@if [ ! -d "$(DESTDIR)$(runstatedir)" ]; then \
		$(INSTALL) -m 755 -d "$(DESTDIR)$(runstatedir)"; \
	fi
	@if egrep "^$(PRIVSEP_USER):" /etc/group >/dev/null; then \
		: ; \
	else \
		echo; \
		echo " Please create a dedicated group for ntpd." ; \
		echo ; \
		echo "   groupadd $(PRIVSEP_USER)" ; \
	fi
	@if egrep "^$(PRIVSEP_USER):" /etc/passwd >/dev/null; then \
		: ; \
	else \
		echo; \
		echo " Please create a dedicated user for ntpd and ensure it cannot be used to log in." ; \
		echo ; \
		echo "   useradd -g $(PRIVSEP_USER) -s /sbin/nologin -d /var/run/openntpd -c 'OpenNTP daemon' $(PRIVSEP_USER)" ; \
		echo "   mkdir -p /var/run/openntpd" ; \
		echo "   chown 0 /var/run/openntpd" ; \
		echo "   chgrp 0 /var/run/openntpd" ; \
		echo "   chmod 0755 /var/run/openntpd" ; \
	fi
	@echo
