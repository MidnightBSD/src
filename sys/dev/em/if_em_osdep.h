/**************************************************************************

Copyright (c) 2001-2006, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

/*$FreeBSD: /repoman/r/ncvs/src/sys/dev/em/if_em_osdep.h,v 1.14.2.2 2006/08/08 09:20:26 glebius Exp $*/

#ifndef _FREEBSD_OS_H_
#define _FREEBSD_OS_H_

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/socket.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#define usec_delay(x) DELAY(x)
#define msec_delay(x) DELAY(1000*(x))
/* TODO: Should we be paranoid about delaying in interrupt context? */
#define msec_delay_irq(x) DELAY(1000*(x))

#define MSGOUT(S, A, B)     printf(S "\n", A, B)
#define DEBUGFUNC(F)        DEBUGOUT(F);
#ifdef DBG
	#define DEBUGOUT(S)         printf(S "\n")
	#define DEBUGOUT1(S,A)      printf(S "\n",A)
	#define DEBUGOUT2(S,A,B)    printf(S "\n",A,B)
	#define DEBUGOUT3(S,A,B,C)  printf(S "\n",A,B,C)
	#define DEBUGOUT7(S,A,B,C,D,E,F,G)  printf(S "\n",A,B,C,D,E,F,G)
#else
	#define DEBUGOUT(S)
	#define DEBUGOUT1(S,A)
	#define DEBUGOUT2(S,A,B)
	#define DEBUGOUT3(S,A,B,C)
	#define DEBUGOUT7(S,A,B,C,D,E,F,G)
#endif

#define FALSE               0
#define TRUE                1
#define CMD_MEM_WRT_INVALIDATE          0x0010  /* BIT_4 */
#define PCI_COMMAND_REGISTER            PCIR_COMMAND

struct em_osdep
{
	bus_space_tag_t    mem_bus_space_tag;
	bus_space_handle_t mem_bus_space_handle;
	bus_space_tag_t    io_bus_space_tag;
	bus_space_handle_t io_bus_space_handle;
	bus_space_tag_t    flash_bus_space_tag;
	bus_space_handle_t flash_bus_space_handle;
	device_t 	   dev;
};

#define E1000_WRITE_FLUSH(hw) E1000_READ_REG(hw, STATUS)

/* Read from an absolute offset in the adapter's memory space */
#define E1000_READ_OFFSET(hw, offset) \
    bus_space_read_4( ((struct em_osdep *)(hw)->back)->mem_bus_space_tag, \
    ((struct em_osdep *)(hw)->back)->mem_bus_space_handle, offset)

/* Write to an absolute offset in the adapter's memory space */
#define E1000_WRITE_OFFSET(hw, offset, value) \
    bus_space_write_4( ((struct em_osdep *)(hw)->back)->mem_bus_space_tag, \
    ((struct em_osdep *)(hw)->back)->mem_bus_space_handle, offset, value)

/* Convert a register name to its offset in the adapter's memory space */
#define E1000_REG_OFFSET(hw, reg) \
    ((hw)->mac_type >= em_82543 ? E1000_##reg : E1000_82542_##reg)

/*
 * Register READ/WRITE macros.
 *
 * XXXGL: Due to define's namespace mangling in recent version of
 * if_em_hw.*, we prepend "_" to the register name in all macros,
 * to prevent reg from being substituted, and then, in E1000_REG_OFFSET()
 * we prepend either "E1000" or "E1000_82542".
 *
 * P.S. The problematic defines are E1000_PHY_CTRL and PHY_CTRL.
 *
 * P.P.S. Intel has removed E1000_REG_OFFSET() and copy-pasted it to all
 * macros.
 */
#define _E1000_REG_OFFSET(hw, reg) \
    ((hw)->mac_type >= em_82543 ? E1000##reg : E1000_82542##reg)

#define E1000_READ_REG(hw, reg) \
    E1000_READ_OFFSET(hw, _E1000_REG_OFFSET(hw, _##reg))

#define E1000_WRITE_REG(hw, reg, value) \
    E1000_WRITE_OFFSET(hw, _E1000_REG_OFFSET(hw, _##reg), value)

#define E1000_READ_REG_ARRAY(hw, reg, index) \
    E1000_READ_OFFSET(hw, _E1000_REG_OFFSET(hw, _##reg) + ((index) << 2))

#define E1000_READ_REG_ARRAY_DWORD E1000_READ_REG_ARRAY

#define E1000_WRITE_REG_ARRAY(hw, reg, index, value) \
    E1000_WRITE_OFFSET(hw, _E1000_REG_OFFSET(hw, _##reg) + ((index) << 2), value)

#define E1000_WRITE_REG_ARRAY_BYTE(hw, reg, index, value) \
    bus_space_write_1( ((struct em_osdep *)(hw)->back)->mem_bus_space_tag, \
                       ((struct em_osdep *)(hw)->back)->mem_bus_space_handle, \
                       _E1000_REG_OFFSET(hw, _##reg) + (index), \
                       value)

#define E1000_WRITE_REG_ARRAY_WORD(hw, reg, index, value) \
    bus_space_write_2( ((struct em_osdep *)(hw)->back)->mem_bus_space_tag, \
                       ((struct em_osdep *)(hw)->back)->mem_bus_space_handle, \
                       _E1000_REG_OFFSET(hw, _##reg) + (index), \
                       value)

#define E1000_WRITE_REG_ARRAY_DWORD(hw, reg, index, value) \
    E1000_WRITE_OFFSET(hw, _E1000_REG_OFFSET(hw, _##reg) + ((index) << 2), value)

#define E1000_READ_ICH8_REG(hw, reg) \
    bus_space_read_4(((struct em_osdep *)(hw)->back)->flash_bus_space_tag, \
        ((struct em_osdep *)(hw)->back)->flash_bus_space_handle, reg)

#define E1000_READ_ICH8_REG16(hw, reg) \
    bus_space_read_2(((struct em_osdep *)(hw)->back)->flash_bus_space_tag, \
        ((struct em_osdep *)(hw)->back)->flash_bus_space_handle, reg)

#define E1000_WRITE_ICH8_REG(hw, reg, value) \
    bus_space_write_4(((struct em_osdep *)(hw)->back)->flash_bus_space_tag, \
        ((struct em_osdep *)(hw)->back)->flash_bus_space_handle, reg, value)

#define E1000_WRITE_ICH8_REG16(hw, reg, value) \
    bus_space_write_2(((struct em_osdep *)(hw)->back)->flash_bus_space_tag, \
        ((struct em_osdep *)(hw)->back)->flash_bus_space_handle, reg, value)
 
#define em_io_read(hw, port)						\
    bus_space_read_4(((struct em_osdep *)(hw)->back)->io_bus_space_tag, \
	((struct em_osdep *)(hw)->back)->io_bus_space_handle, (port))

#define em_io_write(hw, port, value)					 \
    bus_space_write_4(((struct em_osdep *)(hw)->back)->io_bus_space_tag, \
	((struct em_osdep *)(hw)->back)->io_bus_space_handle, (port),	 \
	(value))

#endif  /* _FREEBSD_OS_H_ */

