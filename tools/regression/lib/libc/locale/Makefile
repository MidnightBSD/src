# $FreeBSD: stable/9/tools/regression/lib/libc/locale/Makefile 137960 2004-11-21 03:21:22Z tjr $

TESTS=	test-mbrtowc	\
	test-wcrtomb	\
	test-mbsnrtowcs	\
	test-mbsrtowcs	\
	test-wcsnrtombs	\
	test-wcsrtombs	\
	test-btowc	\
	test-mbrlen	\
	test-mbtowc	\
	test-wctomb	\
	test-mbstowcs	\
	test-wcstombs	\
	test-mblen	\
	test-iswctype	\
	test-towctrans

.PHONY: tests
tests: ${TESTS}
	for p in ${TESTS}; do ${.OBJDIR}/$$p; done

.PHONY: clean
clean:
	-rm -f ${TESTS}
