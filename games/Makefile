#	@(#)Makefile	8.2 (Berkeley) 3/31/94
# $MidnightBSD$

.include <bsd.own.mk>

SUBDIR= \
	arithmetic \
	atc \
	backgammon \
	battlestar \
	bcd \
	bs \
	caesar \
	dm \
	factor \
	fish \
	fortune \
	grdc \
	hangman \
	larn \
	morse \
	number \
	piano \
	pig \
	pom \
	ppt \
	primes \
	rain \
	random \
	robots \
	rogue \
	snake \
	trek \
	wargames \
	worm \
	worms \
	wump \
	${_tests}

.if ${MK_TESTS} != "no"
_tests=	tests
.endif

.include <bsd.subdir.mk>
