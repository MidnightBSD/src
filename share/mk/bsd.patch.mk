#
# Copyright (c) 2006  The MidnightBSD Project
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# bsd.patch.mk - D. Adam Karim
#

# determine system that is being built for
ARCH=${MACHINE_ARCH}

# determine OS name and release if not set
.if !defined(OSNAME)
OSNAME!= uname -s
.endif

.if !defined(OSREL)
OSREL!= uname -r | sed -e 's/[-(].*//'
.endif

# program to use to grab patch
FETCH=ftp

# set default type to common
TYPE?=common

# where to find the patches
MASTER_SITE_MBSD?=ftp://ftp.midnightbsd.org/pub
MASTER_SITE_SUBDIR?=MidnightBSD/patches/${OSREL}/${TYPE}

# set up basic patch info
PATCH_BASE=/usr/src
PATCH_FILE=${PATCH_NUM}-${PATCH_NAME}.patch
PATCH_NAME?=
PATCH_NUM?=
PATCH_SRC?=

PKG_NAME=${OSNAME}-${OSREL}-${ARCH}-${PATCH_NUM}-${PATCH_NAME}.tbz

# set working directories
WRKBASE=${.CURDIR}/work
WRKDIR?=${.CURDIR}/work/${PATCH_NAME}
KCONF=${WRKDIR}/${ARCH}/conf
KWRKDIR=${WRKDIR}/${ARCH}/compile/GENERIC
WRKINST?=${.CURDIR}/work/fake

# set up bindir to pass to make
BNDIR?=

# default comment
COMMENT?="Binary Update"

# pass some env info to make to build properly
MAKE_ENV=env DESTDIR=${WRKINST} BINDIR=${BNDIR}

all: package

init:
	@echo "===> Creating working directory"
	@rm -rf ${WRKDIR}
	@mkdir -p ${WRKDIR}
.if !defined(KERNEL)
	@lndir -silent ${PATCH_BASE}/${PATCH_SRC} ${WRKDIR}
.else
	@lndir -silent ${PATCH_BASE}/sys ${WRKDIR}
.endif

fetch: init
	@echo "===> Fetching patch"
	@${FETCH} ${MASTER_SITE_MBSD}/${MASTER_SITE_SUBDIR}/${PATCH_FILE}

patch: fetch
	@echo "===> Patching src"
	@cd ${WRKDIR} && \
	patch -p0 < ${.CURDIR}/${PATCH_FILE}

build: patch
.if !defined(KERNEL)
	cd ${WRKDIR} && \
	make
.else
	cd ${KCONF} && \
	config GENERIC && \
	cd  ${KWRKDIR} && \
	make depend && \
	make
.endif

mtree:
	@mkdir -p ${WRKINST}
	@mtree -Uq -f /etc/mtree/BSD.root.dist -d -e -p ${WRKINST}
	@mtree -Uq -f /etc/mtree/BSD.usr.dist -d -e -p ${WRKINST}/usr
	@mtree -Uq -f /etc/mtree/BSD.var.dist -d -e -p ${WRKINST}/var
	@chflags noschg ${WRKINST}/var/empty

fake: mtree build
	@echo "===> Installing in fake directory"
.if !defined(KERNEL)
	@cd ${WRKDIR} && ${MAKE_ENV} make install
.else
	@cd ${KWRKDIR} && ${MAKE_ENV} make install
.endif
	
package: fake
	@cd ${WRKINST} && \
	pkg_create -hv -c -${COMMENT} -d ${.CURDIR}/descr -f ${.CURDIR}/plist \
		-p / ${.CURDIR}/${PKG_NAME}
	@md5 ${.CURDIR}/${PKG_NAME} > ${.CURDIR}/${PKG_NAME}.md5

purge: fake
	@find -d ${WRKINST} -empty -execdir rm -rf {} \;

plist: fake purge
	@cd ${WRKINST} && \
	find . -type f | sed 's/^\.\///' > ${.CURDIR}/plist

install:
	@pkg_add -n ${.CURDIR}/${PKG_NAME}

clean:
	@echo "===> Making clean"
	@rm -rf ${PATCH_FILE} ${WRKDIR} ${WRKINST}

.include <bsd.own.mk>
