# $MidnightBSD$

# Setup variables for the compiler
#
# COMPILER_TYPE is the major type of compiler. Currently gcc and clang support
# automatic detection. Other compiler types can be shoe-horned in, but require
# explicit setting of the compiler type. The compiler type can also be set
# explicitly if, say, you install gcc as clang...
#
# COMPILER_VERSION is a numeric constant equal to:
#     major * 10000 + minor * 100 + tiny
# It too can be overriden on the command line. When testing it, be sure to
# make sure that you are limiting the test to a specific compiler. Testing
# against 30300 for gcc likely isn't  what you wanted (since versions of gcc
# prior to 4.2 likely have no prayer of working).
#
# COMPILER_FEATURES will contain one or more of the following, based on
# compiler support for that feature:
#
# - c++11 : supports full (or nearly full) C++11 programming environment.
#
# This file may be included multiple times, but only has effect the first time.
#

.if !defined(COMPILER_TYPE) || !defined(COMPILER_VERSION)
_v!=	${CC} --version 2>/dev/null || echo 0.0.0
.if !defined(COMPILER_TYPE)
. if ${CC:T:M*gcc*}
COMPILER_TYPE:=	gcc  
. elif ${CC:T:M*clang*}
COMPILER_TYPE:=	clang
. elif ${_v:Mgcc}
COMPILER_TYPE:=	gcc
. elif ${_v:M\(GCC\)}
COMPILER_TYPE:=	gcc
. elif ${_v:Mclang}
COMPILER_TYPE:=	clang
. else
.error Unable to determine compiler type for ${CC}.  Consider setting COMPILER_TYPE.
. endif
.endif
.if !defined(COMPILER_VERSION)
COMPILER_VERSION!=echo "${_v:M[1-9].[0-9]*}" | awk -F. '{print $$1 * 10000 + $$2 * 100 + $$3;}'
.endif
.undef _v
.endif

.if ${COMPILER_TYPE} == "clang" || \
	(${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} >= 40800)
COMPILER_FEATURES=	c++11
.else
COMPILER_FEATURES=
.endif
