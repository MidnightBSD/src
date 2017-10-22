# $FreeBSD: release/10.0.0/tools/regression/poll/Makefile 196554 2009-08-25 20:33:37Z jilles $
# This makefile has been uglified for portability.
# Nothing yet works with gmake for the path to the sources.
.PATH: ..

PROG=	pipepoll pipeselect sockpoll
CFLAGS+= -Werror -Wall

all: ${PROG}
pipepoll: pipepoll.c
pipeselect: pipeselect.c
sockpoll: sockpoll.c

pipepoll pipeselect sockpoll:
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $@.c

test: all
	-for prog in ${PROG}; do \
		./$${prog} > $${prog}.out.new; \
		diff -u1 $${prog}.out $${prog}.out.new; \
	done

clean:
	for prog in ${PROG}; do \
		rm -f $${prog} $${prog}.out.new; \
	done

rename:
	for prog in ${PROG}; do \
		mv $${prog}.out.new $${prog}.out; \
	done

veryclean: clean
	for prog in ${PROG}; do \
		rm -f $${prog}.out; \
	done
