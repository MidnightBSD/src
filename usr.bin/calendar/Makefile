#	@(#)Makefile	8.1 (Berkeley) 6/6/93

.include <src.opts.mk>

PROG=	calendar
SRCS=	calendar.c locale.c events.c dates.c parsedata.c io.c day.c \
	ostern.c paskha.c pom.c sunpos.c
LIBADD=	m util

.if ${MK_ICONV} == "yes"
CFLAGS+=	-DWITH_ICONV
.endif

FILESGROUPS+=	CALS
CALS=	calendars/calendar.all \
	calendars/calendar.australia \
	calendars/calendar.birthday \
	calendars/calendar.brazilian \
	calendars/calendar.christian \
	calendars/calendar.computer \
	calendars/calendar.croatian \
	calendars/calendar.dutch \
	calendars/calendar.french \
	calendars/calendar.german \
	calendars/calendar.history \
	calendars/calendar.holiday \
	calendars/calendar.hungarian \
 	calendars/calendar.judaic \
	calendars/calendar.lotr \
	calendars/calendar.music \
	calendars/calendar.newzealand \
	calendars/calendar.russian \
	calendars/calendar.southafrica \
	calendars/calendar.ukrainian \
	calendars/calendar.usholiday \
	calendars/calendar.world
CALSDIR=	${SHAREDIR}/calendar

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
