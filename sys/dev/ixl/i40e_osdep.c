/* $MidnightBSD$ */
/******************************************************************************

  Copyright (c) 2013-2014, Intel Corporation 
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

******************************************************************************/
/*$FreeBSD: stable/10/sys/dev/ixl/i40e_osdep.c 292099 2015-12-11 13:05:18Z smh $*/

#include <machine/stdarg.h>

#include "ixl.h"

/********************************************************************
 * Manage DMA'able memory.
 *******************************************************************/
static void
i40e_dmamap_cb(void *arg, bus_dma_segment_t * segs, int nseg, int error)
{
        if (error)
                return;
        *(bus_addr_t *) arg = segs->ds_addr;
        return;
}

i40e_status
i40e_allocate_virt_mem(struct i40e_hw *hw, struct i40e_virt_mem *mem, u32 size)
{
	mem->va = malloc(size, M_DEVBUF, M_NOWAIT | M_ZERO);
	return(mem->va == NULL);
}

i40e_status
i40e_free_virt_mem(struct i40e_hw *hw, struct i40e_virt_mem *mem)
{
	free(mem->va, M_DEVBUF);
	return(0);
}

i40e_status
i40e_allocate_dma_mem(struct i40e_hw *hw, struct i40e_dma_mem *mem,
	enum i40e_memory_type type __unused, u64 size, u32 alignment)
{
	device_t	dev = ((struct i40e_osdep *)hw->back)->dev;
	int		err;


	err = bus_dma_tag_create(bus_get_dma_tag(dev),	/* parent */
			       alignment, 0,	/* alignment, bounds */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,	/* filter, filterarg */
			       size,	/* maxsize */
			       1,	/* nsegments */
			       size,	/* maxsegsize */
			       BUS_DMA_ALLOCNOW, /* flags */
			       NULL,	/* lockfunc */
			       NULL,	/* lockfuncarg */
			       &mem->tag);
	if (err != 0) {
		device_printf(dev,
		    "i40e_allocate_dma: bus_dma_tag_create failed, "
		    "error %u\n", err);
		goto fail_0;
	}
	err = bus_dmamem_alloc(mem->tag, (void **)&mem->va,
			     BUS_DMA_NOWAIT | BUS_DMA_ZERO, &mem->map);
	if (err != 0) {
		device_printf(dev,
		    "i40e_allocate_dma: bus_dmamem_alloc failed, "
		    "error %u\n", err);
		goto fail_1;
	}
	err = bus_dmamap_load(mem->tag, mem->map, mem->va,
			    size,
			    i40e_dmamap_cb,
			    &mem->pa,
			    BUS_DMA_NOWAIT);
	if (err != 0) {
		device_printf(dev,
		    "i40e_allocate_dma: bus_dmamap_load failed, "
		    "error %u\n", err);
		goto fail_2;
	}
	mem->nseg = 1;
	mem->size = size;
	bus_dmamap_sync(mem->tag, mem->map,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	return (0);
fail_2:
	bus_dmamem_free(mem->tag, mem->va, mem->map);
fail_1:
	bus_dma_tag_destroy(mem->tag);
fail_0:
	mem->map = NULL;
	mem->tag = NULL;
	return (err);
}

i40e_status
i40e_free_dma_mem(struct i40e_hw *hw, struct i40e_dma_mem *mem)
{
	bus_dmamap_sync(mem->tag, mem->map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(mem->tag, mem->map);
	bus_dmamem_free(mem->tag, mem->va, mem->map);
	bus_dma_tag_destroy(mem->tag);
	return (0);
}

void
i40e_init_spinlock(struct i40e_spinlock *lock)
{
	mtx_init(&lock->mutex, "mutex",
	    MTX_NETWORK_LOCK, MTX_DEF | MTX_DUPOK);
}

void
i40e_acquire_spinlock(struct i40e_spinlock *lock)
{
	mtx_lock(&lock->mutex);
}

void
i40e_release_spinlock(struct i40e_spinlock *lock)
{
	mtx_unlock(&lock->mutex);
}

void
i40e_destroy_spinlock(struct i40e_spinlock *lock)
{
	mtx_destroy(&lock->mutex);
}

/*
** i40e_debug_d - OS dependent version of shared code debug printing
*/
void i40e_debug_d(void *hw, u32 mask, char *fmt, ...)
{
        char buf[512];
        va_list args;

        if (!(mask & ((struct i40e_hw *)hw)->debug_mask))
                return;

	va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

        /* the debug string is already formatted with a newline */
        printf("%s", buf);
}

u16
i40e_read_pci_cfg(struct i40e_hw *hw, u32 reg)
{
        u16 value;

        value = pci_read_config(((struct i40e_osdep *)hw->back)->dev,
            reg, 2);

        return (value);
}

void
i40e_write_pci_cfg(struct i40e_hw *hw, u32 reg, u16 value)
{
        pci_write_config(((struct i40e_osdep *)hw->back)->dev,
            reg, value, 2);

        return;
}

