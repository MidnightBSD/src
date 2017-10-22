/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2003 Paul Saab
 * Copyright (c) 2003 Vinod Kashyap
 * Copyright (c) 2000 BSDi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
/*
 * Portability and compatibility interfaces.
 */

#ifdef __FreeBSD__
/******************************************************************************
 * FreeBSD
 */
#define TWE_SUPPORTED_PLATFORM

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/stat.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#define TWE_DRIVER_NAME		twe
#define TWED_DRIVER_NAME	twed
#define TWE_MALLOC_CLASS	M_TWE

/* 
 * Wrappers for bus-space actions
 */
#define TWE_CONTROL(sc, val)		bus_space_write_4((sc)->twe_btag, (sc)->twe_bhandle, 0x0, (u_int32_t)val)
#define TWE_STATUS(sc)			(u_int32_t)bus_space_read_4((sc)->twe_btag, (sc)->twe_bhandle, 0x4)
#define TWE_COMMAND_QUEUE(sc, val)	bus_space_write_4((sc)->twe_btag, (sc)->twe_bhandle, 0x8, (u_int32_t)val)
#define TWE_RESPONSE_QUEUE(sc)		(TWE_Response_Queue)bus_space_read_4((sc)->twe_btag, (sc)->twe_bhandle, 0xc)

/*
 * FreeBSD-specific softc elements
 */
#define TWE_PLATFORM_SOFTC								\
    bus_dmamap_t		twe_cmdmap;	/* DMA map for command */				\
    u_int32_t			twe_cmdphys;	/* address of command in controller space */		\
    device_t			twe_dev;		/* bus device */		\
    struct cdev *twe_dev_t;		/* control device */		\
    struct resource		*twe_io;		/* register interface window */	\
    bus_space_handle_t		twe_bhandle;		/* bus space handle */		\
    bus_space_tag_t		twe_btag;		/* bus space tag */		\
    bus_dma_tag_t		twe_parent_dmat;	/* parent DMA tag */		\
    bus_dma_tag_t		twe_buffer_dmat;	/* data buffer DMA tag */	\
    bus_dma_tag_t		twe_cmd_dmat;		/* command buffer DMA tag */	\
    bus_dma_tag_t		twe_immediate_dmat;	/* command buffer DMA tag */	\
    struct resource		*twe_irq;		/* interrupt */			\
    void			*twe_intr;		/* interrupt handle */		\
    struct intr_config_hook	twe_ich;		/* delayed-startup hook */	\
    void			*twe_cmd;		/* command structures */	\
    void			*twe_immediate;		/* immediate commands */	\
    bus_dmamap_t		twe_immediate_map;					\
    struct sysctl_ctx_list	sysctl_ctx;						\
    struct sysctl_oid		*sysctl_tree;

/*
 * FreeBSD-specific request elements
 */
#define TWE_PLATFORM_REQUEST										\
    bus_dmamap_t		tr_dmamap;	/* DMA map for data */					\
    u_int32_t			tr_dataphys;	/* data buffer base address in controller space */

/*
 * Output identifying the controller/disk
 */
#define twe_printf(sc, fmt, args...)	device_printf(sc->twe_dev, fmt , ##args)
#define twed_printf(twed, fmt, args...)	device_printf(twed->twed_dev, fmt , ##args)

#if __FreeBSD_version < 500003
# include <machine/clock.h>
# define INTR_ENTROPY			0
# define FREEBSD_4

# include <sys/buf.h>			/* old buf style */
typedef struct buf			twe_bio;
typedef struct buf_queue_head		twe_bioq;
# define TWE_BIO_QINIT(bq)		bufq_init(&bq);
# define TWE_BIO_QINSERT(bq, bp)	bufq_insert_tail(&bq, bp)
# define TWE_BIO_QFIRST(bq)		bufq_first(&bq)
# define TWE_BIO_QREMOVE(bq, bp)	bufq_remove(&bq, bp)
# define TWE_BIO_IS_READ(bp)		((bp)->b_flags & B_READ)
# define TWE_BIO_DATA(bp)		(bp)->b_data
# define TWE_BIO_LENGTH(bp)		(bp)->b_bcount
# define TWE_BIO_LBA(bp)		(bp)->b_pblkno
# define TWE_BIO_SOFTC(bp)		(bp)->b_dev->si_drv1
# define TWE_BIO_UNIT(bp)		*(int *)((bp)->b_dev->si_drv2)
# define TWE_BIO_SET_ERROR(bp, err)	do { (bp)->b_error = err; (bp)->b_flags |= B_ERROR;} while(0)
# define TWE_BIO_HAS_ERROR(bp)		((bp)->b_flags & B_ERROR)
# define TWE_BIO_RESID(bp)		(bp)->b_resid
# define TWE_BIO_DONE(bp)		biodone(bp)
# define TWE_BIO_STATS_START(bp)	devstat_start_transaction(&((struct twed_softc *)TWE_BIO_SOFTC(bp))->twed_stats)
# define TWE_BIO_STATS_END(bp)		devstat_end_transaction_buf(&((struct twed_softc *)TWE_BIO_SOFTC(bp))->twed_stats, bp)
#else
# include <sys/bio.h>
# include <geom/geom_disk.h>
typedef struct bio			twe_bio;
typedef struct bio_queue_head		twe_bioq;
# define TWE_BIO_QINIT(bq)		bioq_init(&bq);
# define TWE_BIO_QINSERT(bq, bp)	bioq_insert_tail(&bq, bp)
# define TWE_BIO_QFIRST(bq)		bioq_first(&bq)
# define TWE_BIO_QREMOVE(bq, bp)	bioq_remove(&bq, bp)
# define TWE_BIO_IS_READ(bp)		((bp)->bio_cmd == BIO_READ)
# define TWE_BIO_DATA(bp)		(bp)->bio_data
# define TWE_BIO_LENGTH(bp)		(bp)->bio_bcount
# define TWE_BIO_LBA(bp)		(bp)->bio_pblkno
# define TWE_BIO_SOFTC(bp)		(bp)->bio_disk->d_drv1
# define TWE_BIO_UNIT(bp)		*(int *)(bp->bio_driver1)
# define TWE_BIO_SET_ERROR(bp, err)	do { (bp)->bio_error = err; (bp)->bio_flags |= BIO_ERROR;} while(0)
# define TWE_BIO_HAS_ERROR(bp)		((bp)->bio_flags & BIO_ERROR)
# define TWE_BIO_RESID(bp)		(bp)->bio_resid
# define TWE_BIO_DONE(bp)		biodone(bp)
# define TWE_BIO_STATS_START(bp)
# define TWE_BIO_STATS_END(bp)
#endif

#endif /* FreeBSD */

#ifndef TWE_SUPPORTED_PLATFORM
#error platform not supported
#endif
