
.include <src.opts.mk>

PACKAGE=	periodic

CONFGROUPS=	CONFS

CONFS=	100.clean-disks \
	110.clean-tmps \
	120.clean-preserve \
	140.clean-rwho \
	200.backup-passwd \
	210.backup-aliases \
	222.backup-gmirror \
	220.backup-pkgdb \
	221.backup-gpart \
	330.news \
	400.status-disks \
	401.status-graid \
	406.status-gmirror \
	407.status-graid3 \
	408.status-gstripe \
	409.status-gconcat \
	410.status-mfi \
	420.status-network \
	430.status-uptime \
	450.status-security \
	490.status-pkg-changes \
	510.status-world-kernel \
	999.local

# NB: keep these sorted by MK_* knobs

.if ${MK_ACCT} != "no"
CONFGROUPS+=	ACCT
ACCT+=		310.accounting
ACCTMODE=	${BINMODE}
ACCTPACKAGE=	acct
.endif

.if ${MK_CALENDAR} != "no"
CONFS+=	300.calendar
.endif

.if ${MK_MAIL} != "no"
CONFS+=	130.clean-msgs
.endif

.if ${MK_SENDMAIL} != "no"
CONFGROUPS+=		SENDMAIL
SENDMAIL+=		150.clean-hoststat \
			440.status-mailq \
			460.status-mail-rejects \
			500.queuerun
SENDMAILPACKAGE=	sendmail
.endif

.if ${MK_ZFS} != "no"
CONFS+=	223.backup-zfs \
	404.status-zfs \
	800.scrub-zfs
.endif

.include <bsd.prog.mk>
