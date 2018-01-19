# $MidnightBSD$
# $FreeBSD: head/usr.bin/mandoc/Makefile 321810 2017-07-31 19:34:38Z bapt $

.include <bsd.own.mk>

MDOCMLDIR=	${.CURDIR}/../../contrib/mandoc
.PATH: ${MDOCMLDIR}

PROG=	mandoc
MAN=	mandoc.1 eqn.7 mandoc_char.7 tbl.7 man.7 mdoc.7 # roff.7
MLINKS=	mandoc.1 mdocml.1
.if ${MK_MAN_UTILS} != no
MAN+=	apropos.1 makewhatis.8
MLINKS+=	apropos.1 whatis.1
LINKS=	${BINDIR}/mandoc ${BINDIR}/whatis \
	${BINDIR}/mandoc ${BINDIR}/makewhatis \
	${BINDIR}/mandoc ${BINDIR}/apropos
.endif

LIBMAN_SRCS=	man.c \
		man_macro.c \
		man_validate.c

LIBMDOC_SRCS=	att.c \
		lib.c \
		mdoc.c \
		mdoc_argv.c \
		mdoc_macro.c \
		mdoc_markdown.c \
		mdoc_state.c \
		mdoc_validate.c \
		st.c \

LIBROFF_SRCS=	eqn.c \
		roff.c \
		roff_html.c \
		roff_term.c \
		roff_validate.c \
		tbl.c \
		tbl_data.c \
		tbl_layout.c \
		tbl_opts.c \

LIB_SRCS=	${LIBMAN_SRCS} \
		${LIBMDOC_SRCS} \
		${LIBROFF_SRCS} \
		chars.c \
		mandoc.c \
		mandoc_aux.c \
		mandoc_ohash.c \
		mandoc_xr.c \
		msec.c \
		preconv.c \
		read.c \
                   compat_err.c \
                   compat_fts.c \
                   compat_getline.c \
                   compat_getsubopt.c \
                   compat_isblank.c \
                   compat_mkdtemp.c \
                   compat_ohash.c \
                   compat_progname.c \
                   compat_reallocarray.c \
                   compat_recallocarray.c \
                   compat_strcasestr.c \
                   compat_stringlist.c \
                   compat_strlcat.c \
                   compat_strlcpy.c \
                   compat_strsep.c \
                   compat_strtonum.c \
                   compat_vasprintf.c \

HTML_SRCS=	eqn_html.c \
		html.c \
		man_html.c \
		mdoc_html.c \
		tbl_html.c

MAN_SRCS=	mdoc_man.c

TERM_SRCS=	eqn_term.c \
		man_term.c \
		mdoc_term.c \
		term.c \
		term_ascii.c \
		term_ps.c \
		term_tab.c \
		tbl_term.c

DBM_SRCS=	dbm.c \
		dbm_map.c \
		mansearch.c

DBA_SRCS=	dba.c \
		dba_array.c \
		dba_read.c \
		dba_write.c \
		mandocdb.c

SRCS=		${LIB_SRCS} \
		${HTML_SRCS} \
		${MAN_SRCS} \
		${TERM_SRCS} \
		${DBM_SRCS} \
		${DBA_SRCS} \
		main.c \
		manpath.c \
		out.c \
		tag.c \
		tree.c

WARNS?=	3
CFLAGS+= -DHAVE_CONFIG_H
DPADD=	${LIBZ}
LDADD=	${LIBZ}

.include <bsd.prog.mk>
