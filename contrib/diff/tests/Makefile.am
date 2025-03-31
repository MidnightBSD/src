# tests for GNU diff

TESTS = \
  basic \
  binary \
  colliding-file-names \
  excess-slash \
  help-version	\
  function-line-vs-leading-space \
  label-vs-func	\
  new-file \
  no-dereference \
  no-newline-at-eof \
  stdin \
  filename-quoting

EXTRA_DIST = \
  $(TESTS) init.sh t-local.sh

# Note that the first lines are statements.  They ensure that environment
# variables that can perturb tests are unset or set to expected values.
# The rest are envvar settings that propagate build-related Makefile
# variables to test scripts.
TESTS_ENVIRONMENT =				\
  tmp__=$$TMPDIR; test -d "$$tmp__" || tmp__=.;	\
  TMPDIR=$$tmp__; export TMPDIR;		\
  export					\
  VERSION='$(VERSION)'				\
  abs_top_builddir='$(abs_top_builddir)'	\
  abs_top_srcdir='$(abs_top_srcdir)'		\
  abs_srcdir='$(abs_srcdir)'			\
  built_programs="`$(built_programs)`"		\
  srcdir='$(srcdir)'				\
  top_srcdir='$(top_srcdir)'			\
  CC='$(CC)'					\
  MAKE=$(MAKE)					\
  PACKAGE_BUGREPORT='$(PACKAGE_BUGREPORT)'	\
  PACKAGE_VERSION=$(PACKAGE_VERSION)		\
  CONFIG_HEADER='$(abs_top_builddir)/lib/config.h' \
  ENABLE_DEVICE_MAPPER=$(ENABLE_DEVICE_MAPPER)	\
  PERL='$(PERL)'				\
  PREFERABLY_POSIX_SHELL='$(PREFERABLY_POSIX_SHELL)' \
  REPLACE_GETCWD=$(REPLACE_GETCWD)		\
  PATH='$(abs_top_builddir)/src$(PATH_SEPARATOR)'"$$PATH" \
  ; 9>&2

LOG_COMPILER= $(SHELL)

built_programs =							\
  echo 'spy:;@echo $$(PROGRAMS)'					\
    | (cd ../src && MAKEFLAGS= $(MAKE) -s -f Makefile -f - spy)		\
    | tr ' ' '\n' | sed '/^$$/d; s,$(EXEEXT)$$,,' | sort -u

VERBOSE = yes
