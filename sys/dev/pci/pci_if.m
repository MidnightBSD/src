#-
# Copyright (c) 1998 Doug Rabson
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
# $FreeBSD: src/sys/dev/pci/pci_if.m,v 1.7 2005/01/06 01:43:05 imp Exp $
#

#include <sys/bus.h>

INTERFACE pci;

METHOD u_int32_t read_config {
	device_t	dev;
	device_t	child;
	int		reg;
	int		width;
};

METHOD void write_config {
	device_t	dev;
	device_t	child;
	int		reg;
	u_int32_t	val;
	int		width;
};

METHOD int get_powerstate {
	device_t	dev;
	device_t	child;
};

METHOD int set_powerstate {
	device_t	dev;
	device_t	child;
	int		state;
};

METHOD int enable_busmaster {
	device_t	dev;
	device_t	child;
};

METHOD int disable_busmaster {
	device_t	dev;
	device_t	child;
};

METHOD int enable_io {
	device_t	dev;
	device_t	child;
	int		space;
};

METHOD int disable_io {
	device_t	dev;
	device_t	child;
	int		space;
};

METHOD int assign_interrupt {
	device_t	dev;
	device_t	child;
};
