# $FreeBSD: stable/11/stand/lua.mk 344220 2019-02-17 02:39:17Z kevans $

# Common flags to build lua related files

CFLAGS+=	-I${LUASRC} -I${LDRSRC} -I${LIBLUASRC}
# CFLAGS+=	-Ddouble=jagged-little-pill -Dfloat=poison-shake -D__OMIT_FLOAT
CFLAGS+=	-DLUA_FLOAT_TYPE=LUA_FLOAT_INT64
