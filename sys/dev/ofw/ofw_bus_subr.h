/*-
 * Copyright (c) 2005 Marius Strobl <marius@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: release/7.0.0/sys/dev/ofw/ofw_bus_subr.h 174854 2007-12-22 06:32:46Z cvs2svn $
 */

#ifndef	_DEV_OFW_OFW_BUS_SUBR_H_
#define	_DEV_OFW_OFW_BUS_SUBR_H_

#include <sys/bus.h>

#include <dev/ofw/openfirm.h>

#include "ofw_bus_if.h"

int	ofw_bus_gen_setup_devinfo(struct ofw_bus_devinfo *, phandle_t);
void	ofw_bus_gen_destroy_devinfo(struct ofw_bus_devinfo *);

ofw_bus_get_compat_t	ofw_bus_gen_get_compat;
ofw_bus_get_model_t	ofw_bus_gen_get_model;
ofw_bus_get_name_t	ofw_bus_gen_get_name;
ofw_bus_get_node_t	ofw_bus_gen_get_node;
ofw_bus_get_type_t	ofw_bus_gen_get_type;

#endif /* !_DEV_OFW_OFW_BUS_SUBR_H_ */
