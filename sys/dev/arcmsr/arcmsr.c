/*
*****************************************************************************************
**        O.S   : FreeBSD
**   FILE NAME  : arcmsr.c
**        BY    : Erich Chen, Ching Huang
**   Description: SCSI RAID Device Driver for 
**                ARECA (ARC11XX/ARC12XX/ARC13XX/ARC16XX/ARC188x) SATA/SAS RAID HOST Adapter
**                ARCMSR RAID Host adapter
**                [RAID controller:INTEL 331(PCI-X) 341(PCI-EXPRESS) chip set]
******************************************************************************************
************************************************************************
**
** Copyright (c) 2004-2010 ARECA Co. Ltd.
**        Erich Chen, Taipei Taiwan All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, 
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION)HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
**(INCLUDING NEGLIGENCE OR OTHERWISE)ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**************************************************************************
** History
**
**        REV#         DATE             NAME             DESCRIPTION
**     1.00.00.00   03/31/2004      Erich Chen           First release
**     1.20.00.02   11/29/2004      Erich Chen           bug fix with arcmsr_bus_reset when PHY error
**     1.20.00.03   04/19/2005      Erich Chen           add SATA 24 Ports adapter type support
**                                                       clean unused function
**     1.20.00.12   09/12/2005      Erich Chen           bug fix with abort command handling, 
**                                                       firmware version check 
**                                                       and firmware update notify for hardware bug fix
**                                                       handling if none zero high part physical address 
**                                                       of srb resource 
**     1.20.00.13   08/18/2006      Erich Chen           remove pending srb and report busy
**                                                       add iop message xfer 
**                                                       with scsi pass-through command
**                                                       add new device id of sas raid adapters 
**                                                       code fit for SPARC64 & PPC 
**     1.20.00.14   02/05/2007      Erich Chen           bug fix for incorrect ccb_h.status report
**                                                       and cause g_vfs_done() read write error
**     1.20.00.15   10/10/2007      Erich Chen           support new RAID adapter type ARC120x
**     1.20.00.16   10/10/2009      Erich Chen           Bug fix for RAID adapter type ARC120x
**                                                       bus_dmamem_alloc() with BUS_DMA_ZERO
**     1.20.00.17   07/15/2010      Ching Huang          Added support ARC1880
**                                                       report CAM_DEV_NOT_THERE instead of CAM_SEL_TIMEOUT when device failed,
**                                                       prevent cam_periph_error removing all LUN devices of one Target id
**                                                       for any one LUN device failed
**     1.20.00.18   10/14/2010      Ching Huang          Fixed "inquiry data fails comparion at DV1 step"
**                  10/25/2010      Ching Huang          Fixed bad range input in bus_alloc_resource for ADAPTER_TYPE_B
**     1.20.00.19   11/11/2010      Ching Huang          Fixed arcmsr driver prevent arcsas support for Areca SAS HBA ARC13x0
**     1.20.00.20   12/08/2010      Ching Huang          Avoid calling atomic_set_int function
**     1.20.00.21   02/08/2011      Ching Huang          Implement I/O request timeout
**                  02/14/2011      Ching Huang          Modified pktRequestCount
**     1.20.00.21   03/03/2011      Ching Huang          if a command timeout, then wait its ccb back before free it
**     1.20.00.22   07/04/2011      Ching Huang          Fixed multiple MTX panic
**     1.20.00.23   10/28/2011      Ching Huang          Added TIMEOUT_DELAY in case of too many HDDs need to start 
**     1.20.00.23   11/08/2011      Ching Huang          Added report device transfer speed 
**     1.20.00.23   01/30/2012      Ching Huang          Fixed Request requeued and Retrying command
**     1.20.00.24   06/11/2012      Ching Huang          Fixed return sense data condition
**     1.20.00.25   08/17/2012      Ching Huang          Fixed hotplug device no function on type A adapter
******************************************************************************************
*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#if 0
#define ARCMSR_DEBUG1			1
#endif
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/devicestat.h>
#include <sys/kthread.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/sysctl.h>
#include <sys/poll.h>
#include <sys/ioccom.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <isa/rtc.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/atomic.h>
#include <sys/conf.h>
#include <sys/rman.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
/*
**************************************************************************
**************************************************************************
*/
#if __FreeBSD_version >= 500005
    #include <sys/selinfo.h>
    #include <sys/mutex.h>
    #include <sys/endian.h>
    #include <dev/pci/pcivar.h>
    #include <dev/pci/pcireg.h>
    #define ARCMSR_LOCK_INIT(l, s)	mtx_init(l, s, NULL, MTX_DEF)
    #define ARCMSR_LOCK_DESTROY(l)	mtx_destroy(l)
    #define ARCMSR_LOCK_ACQUIRE(l)	mtx_lock(l)
    #define ARCMSR_LOCK_RELEASE(l)	mtx_unlock(l)
    #define ARCMSR_LOCK_TRY(l)		mtx_trylock(l)
    #define arcmsr_htole32(x)		htole32(x)
    typedef struct mtx				arcmsr_lock_t;
#else
    #include <sys/select.h>
    #include <pci/pcivar.h>
    #include <pci/pcireg.h>
    #define ARCMSR_LOCK_INIT(l, s)	simple_lock_init(l)
    #define ARCMSR_LOCK_DESTROY(l)
    #define ARCMSR_LOCK_ACQUIRE(l)	simple_lock(l)
    #define ARCMSR_LOCK_RELEASE(l)	simple_unlock(l)
    #define ARCMSR_LOCK_TRY(l)		simple_lock_try(l)
    #define arcmsr_htole32(x)		(x)
    typedef struct simplelock		arcmsr_lock_t;
#endif

#if !defined(CAM_NEW_TRAN_CODE) && __FreeBSD_version >= 700025
#define	CAM_NEW_TRAN_CODE	1
#endif

#if __FreeBSD_version > 500000
#define arcmsr_callout_init(a)	callout_init(a, /*mpsafe*/1);
#else
#define arcmsr_callout_init(a)	callout_init(a);
#endif

#define ARCMSR_DRIVER_VERSION			"Driver Version 1.20.00.25 2012-08-17"
#include <dev/arcmsr/arcmsr.h>
#define	SRB_SIZE						((sizeof(struct CommandControlBlock)+0x1f) & 0xffe0)
#define ARCMSR_SRBS_POOL_SIZE           (SRB_SIZE * ARCMSR_MAX_FREESRB_NUM)
/*
**************************************************************************
**************************************************************************
*/
#define CHIP_REG_READ32(s, b, r)		bus_space_read_4(acb->btag[b], acb->bhandle[b], offsetof(struct s, r))
#define CHIP_REG_WRITE32(s, b, r, d)	bus_space_write_4(acb->btag[b], acb->bhandle[b], offsetof(struct s, r), d)
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_free_srb(struct CommandControlBlock *srb);
static struct CommandControlBlock * arcmsr_get_freesrb(struct AdapterControlBlock *acb);
static u_int8_t arcmsr_seek_cmd2abort(union ccb * abortccb);
static int arcmsr_probe(device_t dev);
static int arcmsr_attach(device_t dev);
static int arcmsr_detach(device_t dev);
static u_int32_t arcmsr_iop_ioctlcmd(struct AdapterControlBlock *acb, u_int32_t ioctl_cmd, caddr_t arg);
static void arcmsr_iop_parking(struct AdapterControlBlock *acb);
static int arcmsr_shutdown(device_t dev);
static void arcmsr_interrupt(struct AdapterControlBlock *acb);
static void arcmsr_polling_srbdone(struct AdapterControlBlock *acb, struct CommandControlBlock *poll_srb);
static void arcmsr_free_resource(struct AdapterControlBlock *acb);
static void arcmsr_bus_reset(struct AdapterControlBlock *acb);
static void arcmsr_stop_adapter_bgrb(struct AdapterControlBlock *acb);
static void arcmsr_start_adapter_bgrb(struct AdapterControlBlock *acb);
static void arcmsr_iop_init(struct AdapterControlBlock *acb);
static void arcmsr_flush_adapter_cache(struct AdapterControlBlock *acb);
static void arcmsr_post_ioctldata2iop(struct AdapterControlBlock *acb);
static void arcmsr_abort_allcmd(struct AdapterControlBlock *acb);
static void arcmsr_srb_complete(struct CommandControlBlock *srb, int stand_flag);
static void arcmsr_iop_reset(struct AdapterControlBlock *acb);
static void arcmsr_report_sense_info(struct CommandControlBlock *srb);
static void arcmsr_build_srb(struct CommandControlBlock *srb, bus_dma_segment_t * dm_segs, u_int32_t nseg);
static int arcmsr_iop_message_xfer(struct AdapterControlBlock *acb, union ccb * pccb);
static int arcmsr_resume(device_t dev);
static int arcmsr_suspend(device_t dev);
static void arcmsr_rescanLun_cb(struct cam_periph *periph, union ccb *ccb);
static void	arcmsr_polling_devmap(void* arg);
static void	arcmsr_srb_timeout(void* arg);
#ifdef ARCMSR_DEBUG1
static void arcmsr_dump_data(struct AdapterControlBlock *acb);
#endif
/*
**************************************************************************
**************************************************************************
*/
static void UDELAY(u_int32_t us) { DELAY(us); }
/*
**************************************************************************
**************************************************************************
*/
static bus_dmamap_callback_t arcmsr_map_free_srb;
static bus_dmamap_callback_t arcmsr_execute_srb;
/*
**************************************************************************
**************************************************************************
*/
static d_open_t	arcmsr_open;
static d_close_t arcmsr_close;
static d_ioctl_t arcmsr_ioctl;

static device_method_t arcmsr_methods[]={
	DEVMETHOD(device_probe,		arcmsr_probe),
	DEVMETHOD(device_attach,	arcmsr_attach),
	DEVMETHOD(device_detach,	arcmsr_detach),
	DEVMETHOD(device_shutdown,	arcmsr_shutdown),
	DEVMETHOD(device_suspend,	arcmsr_suspend),
	DEVMETHOD(device_resume,	arcmsr_resume),

	DEVMETHOD_END
};
	
static driver_t arcmsr_driver={
	"arcmsr", arcmsr_methods, sizeof(struct AdapterControlBlock)
};
	
static devclass_t arcmsr_devclass;
DRIVER_MODULE(arcmsr, pci, arcmsr_driver, arcmsr_devclass, 0, 0);
MODULE_DEPEND(arcmsr, pci, 1, 1, 1);
MODULE_DEPEND(arcmsr, cam, 1, 1, 1);
#ifndef BUS_DMA_COHERENT		
	#define	BUS_DMA_COHERENT	0x04	/* hint: map memory in a coherent way */
#endif
#if __FreeBSD_version >= 501000
static struct cdevsw arcmsr_cdevsw={
	#if __FreeBSD_version >= 503000
		.d_version = D_VERSION, 
	#endif
	#if (__FreeBSD_version>=503000 && __FreeBSD_version<600034)
		.d_flags   = D_NEEDGIANT, 
	#endif
		.d_open    = arcmsr_open, 	/* open     */
		.d_close   = arcmsr_close, 	/* close    */
		.d_ioctl   = arcmsr_ioctl, 	/* ioctl    */
		.d_name    = "arcmsr", 		/* name     */
	};
#else
	#define ARCMSR_CDEV_MAJOR	180
	
static struct cdevsw arcmsr_cdevsw = {
		arcmsr_open,				/* open     */
		arcmsr_close,				/* close    */
		noread,						/* read     */
		nowrite,					/* write    */
		arcmsr_ioctl,				/* ioctl    */
		nopoll,						/* poll     */
		nommap,						/* mmap     */
		nostrategy,					/* strategy */
		"arcmsr",					/* name     */
		ARCMSR_CDEV_MAJOR,			/* major    */
		nodump,						/* dump     */
		nopsize,					/* psize    */
		0							/* flags    */
	};
#endif
/*
**************************************************************************
**************************************************************************
*/
#if __FreeBSD_version < 500005
	static int arcmsr_open(dev_t dev, int flags, int fmt, struct proc *proc)
#else
	#if __FreeBSD_version < 503000
	static int arcmsr_open(dev_t dev, int flags, int fmt, struct thread *proc)
	#else
	static int arcmsr_open(struct cdev *dev, int flags, int fmt, struct thread *proc)
	#endif 
#endif
{
	#if __FreeBSD_version < 503000
		struct AdapterControlBlock *acb=dev->si_drv1;
	#else
		int	unit = dev2unit(dev);
		struct AdapterControlBlock *acb = devclass_get_softc(arcmsr_devclass, unit);
	#endif
	if(acb==NULL) {
		return ENXIO;
	}
	return (0);
}
/*
**************************************************************************
**************************************************************************
*/
#if __FreeBSD_version < 500005
	static int arcmsr_close(dev_t dev, int flags, int fmt, struct proc *proc)
#else
	#if __FreeBSD_version < 503000
	static int arcmsr_close(dev_t dev, int flags, int fmt, struct thread *proc)
	#else
	static int arcmsr_close(struct cdev *dev, int flags, int fmt, struct thread *proc)
	#endif 
#endif
{
	#if __FreeBSD_version < 503000
		struct AdapterControlBlock *acb=dev->si_drv1;
	#else
		int	unit = dev2unit(dev);
		struct AdapterControlBlock *acb = devclass_get_softc(arcmsr_devclass, unit);
	#endif
	if(acb==NULL) {
		return ENXIO;
	}
	return 0;
}
/*
**************************************************************************
**************************************************************************
*/
#if __FreeBSD_version < 500005
	static int arcmsr_ioctl(dev_t dev, u_long ioctl_cmd, caddr_t arg, int flags, struct proc *proc)
#else
	#if __FreeBSD_version < 503000
	static int arcmsr_ioctl(dev_t dev, u_long ioctl_cmd, caddr_t arg, int flags, struct thread *proc)
	#else
	static int arcmsr_ioctl(struct cdev *dev, u_long ioctl_cmd, caddr_t arg, int flags, struct thread *proc)
	#endif 
#endif
{
	#if __FreeBSD_version < 503000
		struct AdapterControlBlock *acb=dev->si_drv1;
	#else
		int	unit = dev2unit(dev);
		struct AdapterControlBlock *acb = devclass_get_softc(arcmsr_devclass, unit);
	#endif
	
	if(acb==NULL) {
		return ENXIO;
	}
	return (arcmsr_iop_ioctlcmd(acb, ioctl_cmd, arg));
}
/*
**********************************************************************
**********************************************************************
*/
static u_int32_t arcmsr_disable_allintr( struct AdapterControlBlock *acb)
{
	u_int32_t intmask_org=0;
	
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			/* disable all outbound interrupt */
			intmask_org=CHIP_REG_READ32(HBA_MessageUnit, 0, outbound_intmask); /* disable outbound message0 int */
			CHIP_REG_WRITE32(HBA_MessageUnit, 0, outbound_intmask, intmask_org|ARCMSR_MU_OUTBOUND_ALL_INTMASKENABLE);
		}
		break;
	case ACB_ADAPTER_TYPE_B: {
			/* disable all outbound interrupt */
			intmask_org=CHIP_REG_READ32(HBB_DOORBELL, 
						0, iop2drv_doorbell_mask) & (~ARCMSR_IOP2DRV_MESSAGE_CMD_DONE); /* disable outbound message0 int */
			CHIP_REG_WRITE32(HBB_DOORBELL, 0, iop2drv_doorbell_mask, 0); /* disable all interrupt */
		}
		break;
	case ACB_ADAPTER_TYPE_C: {
			/* disable all outbound interrupt */
			intmask_org=CHIP_REG_READ32(HBC_MessageUnit, 0, host_int_mask)	; /* disable outbound message0 int */
			CHIP_REG_WRITE32(HBC_MessageUnit, 0, host_int_mask, intmask_org|ARCMSR_HBCMU_ALL_INTMASKENABLE);
		}
		break;
	}
	return (intmask_org);
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_enable_allintr( struct AdapterControlBlock *acb, u_int32_t intmask_org)
{
	u_int32_t mask;
	
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			/* enable outbound Post Queue, outbound doorbell Interrupt */
			mask=~(ARCMSR_MU_OUTBOUND_POSTQUEUE_INTMASKENABLE|ARCMSR_MU_OUTBOUND_DOORBELL_INTMASKENABLE|ARCMSR_MU_OUTBOUND_MESSAGE0_INTMASKENABLE);
			CHIP_REG_WRITE32(HBA_MessageUnit, 0, outbound_intmask, intmask_org & mask);
			acb->outbound_int_enable = ~(intmask_org & mask) & 0x000000ff;
		}
		break;
	case ACB_ADAPTER_TYPE_B: {
			/* enable ARCMSR_IOP2DRV_MESSAGE_CMD_DONE */
			mask=(ARCMSR_IOP2DRV_DATA_WRITE_OK|ARCMSR_IOP2DRV_DATA_READ_OK|ARCMSR_IOP2DRV_CDB_DONE|ARCMSR_IOP2DRV_MESSAGE_CMD_DONE);
			CHIP_REG_WRITE32(HBB_DOORBELL, 0, iop2drv_doorbell_mask, intmask_org | mask); /*1=interrupt enable, 0=interrupt disable*/
			acb->outbound_int_enable = (intmask_org | mask) & 0x0000000f;
		}
		break;
	case ACB_ADAPTER_TYPE_C: {
			/* enable outbound Post Queue, outbound doorbell Interrupt */
			mask=~(ARCMSR_HBCMU_UTILITY_A_ISR_MASK | ARCMSR_HBCMU_OUTBOUND_DOORBELL_ISR_MASK | ARCMSR_HBCMU_OUTBOUND_POSTQUEUE_ISR_MASK);
			CHIP_REG_WRITE32(HBC_MessageUnit, 0, host_int_mask, intmask_org & mask);
			acb->outbound_int_enable= ~(intmask_org & mask) & 0x0000000f;
		}
		break;
	}
}
/*
**********************************************************************
**********************************************************************
*/
static u_int8_t arcmsr_hba_wait_msgint_ready(struct AdapterControlBlock *acb)
{
	u_int32_t Index;
	u_int8_t Retries=0x00;
	
	do {
		for(Index=0; Index < 100; Index++) {
			if(CHIP_REG_READ32(HBA_MessageUnit, 0, outbound_intstatus) & ARCMSR_MU_OUTBOUND_MESSAGE0_INT) {
				CHIP_REG_WRITE32(HBA_MessageUnit, 0, outbound_intstatus, ARCMSR_MU_OUTBOUND_MESSAGE0_INT);/*clear interrupt*/
				return TRUE;
			}
			UDELAY(10000);
		}/*max 1 seconds*/
	}while(Retries++ < 20);/*max 20 sec*/
	return (FALSE);
}
/*
**********************************************************************
**********************************************************************
*/
static u_int8_t arcmsr_hbb_wait_msgint_ready(struct AdapterControlBlock *acb)
{
	u_int32_t Index;
	u_int8_t Retries=0x00;
	
	do {
		for(Index=0; Index < 100; Index++) {
			if(CHIP_REG_READ32(HBB_DOORBELL, 0, iop2drv_doorbell) & ARCMSR_IOP2DRV_MESSAGE_CMD_DONE) {
				CHIP_REG_WRITE32(HBB_DOORBELL, 0, iop2drv_doorbell, ARCMSR_MESSAGE_INT_CLEAR_PATTERN);/*clear interrupt*/
				CHIP_REG_WRITE32(HBB_DOORBELL, 0, drv2iop_doorbell, ARCMSR_DRV2IOP_END_OF_INTERRUPT);
				return TRUE;
			}
			UDELAY(10000);
		}/*max 1 seconds*/
	}while(Retries++ < 20);/*max 20 sec*/
	return (FALSE);
}
/*
**********************************************************************
**********************************************************************
*/
static u_int8_t arcmsr_hbc_wait_msgint_ready(struct AdapterControlBlock *acb)
{
	u_int32_t Index;
	u_int8_t Retries=0x00;
	
	do {
		for(Index=0; Index < 100; Index++) {
			if(CHIP_REG_READ32(HBC_MessageUnit, 0, outbound_doorbell) & ARCMSR_HBCMU_IOP2DRV_MESSAGE_CMD_DONE) {
				CHIP_REG_WRITE32(HBC_MessageUnit, 0, outbound_doorbell_clear, ARCMSR_HBCMU_IOP2DRV_MESSAGE_CMD_DONE_DOORBELL_CLEAR);/*clear interrupt*/
				return TRUE;
			}
			UDELAY(10000);
		}/*max 1 seconds*/
	}while(Retries++ < 20);/*max 20 sec*/
	return (FALSE);
}
/*
************************************************************************
************************************************************************
*/
static void arcmsr_flush_hba_cache(struct AdapterControlBlock *acb)
{
	int retry_count=30;/* enlarge wait flush adapter cache time: 10 minute */
	
	CHIP_REG_WRITE32(HBA_MessageUnit, 0, inbound_msgaddr0, ARCMSR_INBOUND_MESG0_FLUSH_CACHE);
	do {
		if(arcmsr_hba_wait_msgint_ready(acb)) {
			break;
		} else {
			retry_count--;
		}
	}while(retry_count!=0);
}
/*
************************************************************************
************************************************************************
*/
static void arcmsr_flush_hbb_cache(struct AdapterControlBlock *acb)
{
	int retry_count=30;/* enlarge wait flush adapter cache time: 10 minute */
	
	CHIP_REG_WRITE32(HBB_DOORBELL, 
	0, drv2iop_doorbell, ARCMSR_MESSAGE_FLUSH_CACHE);
	do {
		if(arcmsr_hbb_wait_msgint_ready(acb)) {
			break;
		} else {
			retry_count--;
		}
	}while(retry_count!=0);
}
/*
************************************************************************
************************************************************************
*/
static void arcmsr_flush_hbc_cache(struct AdapterControlBlock *acb)
{
	int retry_count=30;/* enlarge wait flush adapter cache time: 10 minute */
	
	CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_msgaddr0, ARCMSR_INBOUND_MESG0_FLUSH_CACHE);
	CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_doorbell, ARCMSR_HBCMU_DRV2IOP_MESSAGE_CMD_DONE);
	do {
		if(arcmsr_hbc_wait_msgint_ready(acb)) {
			break;
		} else {
			retry_count--;
		}
	}while(retry_count!=0);
}
/*
************************************************************************
************************************************************************
*/
static void arcmsr_flush_adapter_cache(struct AdapterControlBlock *acb)
{
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			arcmsr_flush_hba_cache(acb);
		}
		break;
	case ACB_ADAPTER_TYPE_B: {
			arcmsr_flush_hbb_cache(acb);
		}
		break;
	case ACB_ADAPTER_TYPE_C: {
			arcmsr_flush_hbc_cache(acb);
		}
		break;
	}
}
/*
*******************************************************************************
*******************************************************************************
*/
static int arcmsr_suspend(device_t dev)
{
	struct AdapterControlBlock	*acb = device_get_softc(dev);
	
	/* flush controller */
	arcmsr_iop_parking(acb);
	/* disable all outbound interrupt */
	arcmsr_disable_allintr(acb);
	return(0);
}
/*
*******************************************************************************
*******************************************************************************
*/
static int arcmsr_resume(device_t dev)
{
	struct AdapterControlBlock	*acb = device_get_softc(dev);
	
	arcmsr_iop_init(acb);
	return(0);
}
/*
*********************************************************************************
*********************************************************************************
*/
static void arcmsr_async(void *cb_arg, u_int32_t code, struct cam_path *path, void *arg)
{
	struct AdapterControlBlock *acb;
	u_int8_t target_id, target_lun;
	struct cam_sim * sim;
	
	sim=(struct cam_sim *) cb_arg;
	acb =(struct AdapterControlBlock *) cam_sim_softc(sim);
	switch (code) {
	case AC_LOST_DEVICE:
		target_id=xpt_path_target_id(path);
		target_lun=xpt_path_lun_id(path);
		if((target_id > ARCMSR_MAX_TARGETID) || (target_lun > ARCMSR_MAX_TARGETLUN)) {
			break;
		}
	//	printf("%s:scsi id=%d lun=%d device lost \n", device_get_name(acb->pci_dev), target_id, target_lun);
		break;
	default:
		break;
	}
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_report_sense_info(struct CommandControlBlock *srb)
{
	union ccb * pccb=srb->pccb;
	
	pccb->ccb_h.status |= CAM_SCSI_STATUS_ERROR;
	pccb->csio.scsi_status = SCSI_STATUS_CHECK_COND;
	if(pccb->csio.sense_len) {
		memset(&pccb->csio.sense_data, 0, sizeof(pccb->csio.sense_data));
		memcpy(&pccb->csio.sense_data, srb->arcmsr_cdb.SenseData, 
		get_min(sizeof(struct SENSE_DATA), sizeof(pccb->csio.sense_data)));
		((u_int8_t *)&pccb->csio.sense_data)[0] = (0x1 << 7 | 0x70); /* Valid,ErrorCode */
		pccb->ccb_h.status |= CAM_AUTOSNS_VALID;
	}
}
/*
*********************************************************************
*********************************************************************
*/
static void arcmsr_abort_hba_allcmd(struct AdapterControlBlock *acb)
{
	CHIP_REG_WRITE32(HBA_MessageUnit, 0, inbound_msgaddr0, ARCMSR_INBOUND_MESG0_ABORT_CMD);
	if(!arcmsr_hba_wait_msgint_ready(acb)) {
		printf("arcmsr%d: wait 'abort all outstanding command' timeout \n", acb->pci_unit);
	}
}
/*
*********************************************************************
*********************************************************************
*/
static void arcmsr_abort_hbb_allcmd(struct AdapterControlBlock *acb)
{
	CHIP_REG_WRITE32(HBB_DOORBELL, 0, drv2iop_doorbell, ARCMSR_MESSAGE_ABORT_CMD);
	if(!arcmsr_hbb_wait_msgint_ready(acb)) {
		printf("arcmsr%d: wait 'abort all outstanding command' timeout \n", acb->pci_unit);
	}
}
/*
*********************************************************************
*********************************************************************
*/
static void arcmsr_abort_hbc_allcmd(struct AdapterControlBlock *acb)
{
	CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_msgaddr0, ARCMSR_INBOUND_MESG0_ABORT_CMD);
	CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_doorbell, ARCMSR_HBCMU_DRV2IOP_MESSAGE_CMD_DONE);
	if(!arcmsr_hbc_wait_msgint_ready(acb)) {
		printf("arcmsr%d: wait 'abort all outstanding command' timeout \n", acb->pci_unit);
	}
}
/*
*********************************************************************
*********************************************************************
*/
static void arcmsr_abort_allcmd(struct AdapterControlBlock *acb)
{
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			arcmsr_abort_hba_allcmd(acb);
		}
		break;
	case ACB_ADAPTER_TYPE_B: {
			arcmsr_abort_hbb_allcmd(acb);
		}
		break;
	case ACB_ADAPTER_TYPE_C: {
			arcmsr_abort_hbc_allcmd(acb);
		}
		break;
	}
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_srb_complete(struct CommandControlBlock *srb, int stand_flag)
{
	struct AdapterControlBlock *acb=srb->acb;
	union ccb * pccb=srb->pccb;
	
	if(srb->srb_flags & SRB_FLAG_TIMER_START)
		callout_stop(&srb->ccb_callout);
	if((pccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		bus_dmasync_op_t op;
	
		if((pccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
			op = BUS_DMASYNC_POSTREAD;
		} else {
			op = BUS_DMASYNC_POSTWRITE;
		}
		bus_dmamap_sync(acb->dm_segs_dmat, srb->dm_segs_dmamap, op);
		bus_dmamap_unload(acb->dm_segs_dmat, srb->dm_segs_dmamap);
	}
	if(stand_flag==1) {
		atomic_subtract_int(&acb->srboutstandingcount, 1);
		if((acb->acb_flags & ACB_F_CAM_DEV_QFRZN) && (
		acb->srboutstandingcount < ARCMSR_RELEASE_SIMQ_LEVEL)) {
			acb->acb_flags &= ~ACB_F_CAM_DEV_QFRZN;
			pccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		}
	}
	if(srb->srb_state != ARCMSR_SRB_TIMEOUT)
		arcmsr_free_srb(srb);
#ifdef ARCMSR_DEBUG1
	acb->pktReturnCount++;
#endif
	xpt_done(pccb);
}
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_report_srb_state(struct AdapterControlBlock *acb, struct CommandControlBlock *srb, u_int16_t error)
{
	int target, lun;
	
	target=srb->pccb->ccb_h.target_id;
	lun=srb->pccb->ccb_h.target_lun;
	if(error == FALSE) {
		if(acb->devstate[target][lun]==ARECA_RAID_GONE) {
			acb->devstate[target][lun]=ARECA_RAID_GOOD;
		}
		srb->pccb->ccb_h.status |= CAM_REQ_CMP;
		arcmsr_srb_complete(srb, 1);
	} else {
		switch(srb->arcmsr_cdb.DeviceStatus) {
		case ARCMSR_DEV_SELECT_TIMEOUT: {
				if(acb->devstate[target][lun]==ARECA_RAID_GOOD) {
					printf( "arcmsr%d: Target=%x, Lun=%x, selection timeout, raid volume was lost\n", acb->pci_unit, target, lun);
				}
				acb->devstate[target][lun]=ARECA_RAID_GONE;
				srb->pccb->ccb_h.status |= CAM_DEV_NOT_THERE;
				arcmsr_srb_complete(srb, 1);
			}
			break;
		case ARCMSR_DEV_ABORTED:
		case ARCMSR_DEV_INIT_FAIL: {
				acb->devstate[target][lun]=ARECA_RAID_GONE;
				srb->pccb->ccb_h.status |= CAM_DEV_NOT_THERE;
				arcmsr_srb_complete(srb, 1);
			}
			break;
		case SCSISTAT_CHECK_CONDITION: {
				acb->devstate[target][lun]=ARECA_RAID_GOOD;
				arcmsr_report_sense_info(srb);
				arcmsr_srb_complete(srb, 1);
			}
			break;
		default:
			printf("arcmsr%d: scsi id=%d lun=%d isr got command error done,but got unknow DeviceStatus=0x%x \n"
					, acb->pci_unit, target, lun ,srb->arcmsr_cdb.DeviceStatus);
			acb->devstate[target][lun]=ARECA_RAID_GONE;
			srb->pccb->ccb_h.status |= CAM_UNCOR_PARITY;
			/*unknow error or crc error just for retry*/
			arcmsr_srb_complete(srb, 1);
			break;
		}
	}
}
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_drain_donequeue(struct AdapterControlBlock *acb, u_int32_t flag_srb, u_int16_t error)
{
	struct CommandControlBlock *srb;
	
	/* check if command done with no error*/
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_C:
		srb = (struct CommandControlBlock *)(acb->vir2phy_offset+(flag_srb & 0xFFFFFFE0));/*frame must be 32 bytes aligned*/
		break;
	case ACB_ADAPTER_TYPE_A:
	case ACB_ADAPTER_TYPE_B:
	default:
		srb = (struct CommandControlBlock *)(acb->vir2phy_offset+(flag_srb << 5));/*frame must be 32 bytes aligned*/
		break;
	}
	if((srb->acb!=acb) || (srb->srb_state!=ARCMSR_SRB_START)) {
		if(srb->srb_state == ARCMSR_SRB_TIMEOUT) {
			arcmsr_free_srb(srb);
			printf("arcmsr%d: srb='%p' return srb has been timeouted\n", acb->pci_unit, srb);
			return;
		}
		printf("arcmsr%d: return srb has been completed\n"
			"srb='%p' srb_state=0x%x outstanding srb count=%d \n",
			acb->pci_unit, srb, srb->srb_state, acb->srboutstandingcount);
		return;
	}
	arcmsr_report_srb_state(acb, srb, error);
}
/*
**************************************************************************
**************************************************************************
*/
static void	arcmsr_srb_timeout(void* arg)
{
	struct CommandControlBlock *srb = (struct CommandControlBlock *)arg;
	struct AdapterControlBlock *acb;
	int target, lun;
	u_int8_t cmd;
	
	target=srb->pccb->ccb_h.target_id;
	lun=srb->pccb->ccb_h.target_lun;
	acb = srb->acb;
	ARCMSR_LOCK_ACQUIRE(&acb->qbuffer_lock);
	if(srb->srb_state == ARCMSR_SRB_START)
	{
		cmd = srb->pccb->csio.cdb_io.cdb_bytes[0];
		srb->srb_state = ARCMSR_SRB_TIMEOUT;
		srb->pccb->ccb_h.status |= CAM_CMD_TIMEOUT;
		arcmsr_srb_complete(srb, 1);
		printf("arcmsr%d: scsi id %d lun %d cmd=0x%x srb='%p' ccb command time out!\n",
				 acb->pci_unit, target, lun, cmd, srb);
	}
	ARCMSR_LOCK_RELEASE(&acb->qbuffer_lock);
#ifdef ARCMSR_DEBUG1
    	arcmsr_dump_data(acb);
#endif
}

/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_done4abort_postqueue(struct AdapterControlBlock *acb)
{
	int i=0;
	u_int32_t flag_srb;
	u_int16_t error;
	
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			u_int32_t outbound_intstatus;
	
			/*clear and abort all outbound posted Q*/
			outbound_intstatus=CHIP_REG_READ32(HBA_MessageUnit, 0, outbound_intstatus) & acb->outbound_int_enable;
			CHIP_REG_WRITE32(HBA_MessageUnit, 0, outbound_intstatus, outbound_intstatus);/*clear interrupt*/
			while(((flag_srb=CHIP_REG_READ32(HBA_MessageUnit, 0, outbound_queueport)) != 0xFFFFFFFF) && (i++ < ARCMSR_MAX_OUTSTANDING_CMD)) {
                error=(flag_srb & ARCMSR_SRBREPLY_FLAG_ERROR_MODE0)?TRUE:FALSE;
				arcmsr_drain_donequeue(acb, flag_srb, error);
			}
		}
		break;
	case ACB_ADAPTER_TYPE_B: {
			struct HBB_MessageUnit *phbbmu=(struct HBB_MessageUnit *)acb->pmu;
	
			/*clear all outbound posted Q*/
			CHIP_REG_WRITE32(HBB_DOORBELL, 0, iop2drv_doorbell, ARCMSR_DOORBELL_INT_CLEAR_PATTERN); /* clear doorbell interrupt */
			for(i=0; i < ARCMSR_MAX_HBB_POSTQUEUE; i++) {
				if((flag_srb=phbbmu->done_qbuffer[i])!=0) {
					phbbmu->done_qbuffer[i]=0;
                	error=(flag_srb & ARCMSR_SRBREPLY_FLAG_ERROR_MODE0)?TRUE:FALSE;
					arcmsr_drain_donequeue(acb, flag_srb, error);
				}
				phbbmu->post_qbuffer[i]=0;
			}/*drain reply FIFO*/
			phbbmu->doneq_index=0;
			phbbmu->postq_index=0;
		}
		break;
	case ACB_ADAPTER_TYPE_C: {
	
			while((CHIP_REG_READ32(HBC_MessageUnit, 0, host_int_status) & ARCMSR_HBCMU_OUTBOUND_POSTQUEUE_ISR) && (i++ < ARCMSR_MAX_OUTSTANDING_CMD)) {
				flag_srb=CHIP_REG_READ32(HBC_MessageUnit, 0, outbound_queueport_low);
                error=(flag_srb & ARCMSR_SRBREPLY_FLAG_ERROR_MODE1)?TRUE:FALSE;
				arcmsr_drain_donequeue(acb, flag_srb, error);
			}
		}
		break;
	}
}
/*
****************************************************************************
****************************************************************************
*/
static void arcmsr_iop_reset(struct AdapterControlBlock *acb)
{
	struct CommandControlBlock *srb;
	u_int32_t intmask_org;
	u_int32_t i=0;
	
	if(acb->srboutstandingcount>0) {
		/* disable all outbound interrupt */
		intmask_org=arcmsr_disable_allintr(acb);
		/*clear and abort all outbound posted Q*/
		arcmsr_done4abort_postqueue(acb);
		/* talk to iop 331 outstanding command aborted*/
		arcmsr_abort_allcmd(acb);
		for(i=0;i<ARCMSR_MAX_FREESRB_NUM;i++) {
			srb=acb->psrb_pool[i];
			if(srb->srb_state==ARCMSR_SRB_START) {
				srb->srb_state=ARCMSR_SRB_ABORTED;
				srb->pccb->ccb_h.status |= CAM_REQ_ABORTED;
				arcmsr_srb_complete(srb, 1);
				printf("arcmsr%d: scsi id=%d lun=%d srb='%p' aborted\n"
						, acb->pci_unit, srb->pccb->ccb_h.target_id
						, srb->pccb->ccb_h.target_lun, srb);
			}
		}
		/* enable all outbound interrupt */
		arcmsr_enable_allintr(acb, intmask_org);
	}
	acb->srboutstandingcount=0;
	acb->workingsrb_doneindex=0;
	acb->workingsrb_startindex=0;
#ifdef ARCMSR_DEBUG1
	acb->pktRequestCount = 0;
	acb->pktReturnCount = 0;
#endif
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_build_srb(struct CommandControlBlock *srb, 
		bus_dma_segment_t *dm_segs, u_int32_t nseg)
{
	struct ARCMSR_CDB * arcmsr_cdb= &srb->arcmsr_cdb;
	u_int8_t * psge=(u_int8_t *)&arcmsr_cdb->u;
	u_int32_t address_lo, address_hi;
	union ccb * pccb=srb->pccb;
	struct ccb_scsiio * pcsio= &pccb->csio;
	u_int32_t arccdbsize=0x30;
	
	memset(arcmsr_cdb, 0, sizeof(struct ARCMSR_CDB));
	arcmsr_cdb->Bus=0;
	arcmsr_cdb->TargetID=pccb->ccb_h.target_id;
	arcmsr_cdb->LUN=pccb->ccb_h.target_lun;
	arcmsr_cdb->Function=1;
	arcmsr_cdb->CdbLength=(u_int8_t)pcsio->cdb_len;
	arcmsr_cdb->Context=0;
	bcopy(pcsio->cdb_io.cdb_bytes, arcmsr_cdb->Cdb, pcsio->cdb_len);
	if(nseg != 0) {
		struct AdapterControlBlock *acb=srb->acb;
		bus_dmasync_op_t op;	
		u_int32_t length, i, cdb_sgcount=0;
	
		if((pccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
			op=BUS_DMASYNC_PREREAD;
		} else {
			op=BUS_DMASYNC_PREWRITE;
			arcmsr_cdb->Flags|=ARCMSR_CDB_FLAG_WRITE;
			srb->srb_flags|=SRB_FLAG_WRITE;
		}
		bus_dmamap_sync(acb->dm_segs_dmat, srb->dm_segs_dmamap, op);
		for(i=0;i<nseg;i++) {
			/* Get the physical address of the current data pointer */
			length=arcmsr_htole32(dm_segs[i].ds_len);
			address_lo=arcmsr_htole32(dma_addr_lo32(dm_segs[i].ds_addr));
			address_hi=arcmsr_htole32(dma_addr_hi32(dm_segs[i].ds_addr));
			if(address_hi==0) {
				struct SG32ENTRY * pdma_sg=(struct SG32ENTRY *)psge;
				pdma_sg->address=address_lo;
				pdma_sg->length=length;
				psge += sizeof(struct SG32ENTRY);
				arccdbsize += sizeof(struct SG32ENTRY);
			} else {
				u_int32_t sg64s_size=0, tmplength=length;
	
				while(1) {
					u_int64_t span4G, length0;
					struct SG64ENTRY * pdma_sg=(struct SG64ENTRY *)psge;
	
					span4G=(u_int64_t)address_lo + tmplength;
					pdma_sg->addresshigh=address_hi;
					pdma_sg->address=address_lo;
					if(span4G > 0x100000000) {
						/*see if cross 4G boundary*/
						length0=0x100000000-address_lo;
						pdma_sg->length=(u_int32_t)length0|IS_SG64_ADDR;
						address_hi=address_hi+1;
						address_lo=0;
						tmplength=tmplength-(u_int32_t)length0;
						sg64s_size += sizeof(struct SG64ENTRY);
						psge += sizeof(struct SG64ENTRY);
						cdb_sgcount++;
					} else {
						pdma_sg->length=tmplength|IS_SG64_ADDR;
						sg64s_size += sizeof(struct SG64ENTRY);
						psge += sizeof(struct SG64ENTRY);
						break;
					}
				}
				arccdbsize += sg64s_size;
			}
			cdb_sgcount++;
		}
		arcmsr_cdb->sgcount=(u_int8_t)cdb_sgcount;
		arcmsr_cdb->DataLength=pcsio->dxfer_len;
		if( arccdbsize > 256) {
			arcmsr_cdb->Flags|=ARCMSR_CDB_FLAG_SGL_BSIZE;
		}
	} else {
		arcmsr_cdb->DataLength = 0;
	}
    srb->arc_cdb_size=arccdbsize;
}
/*
**************************************************************************
**************************************************************************
*/ 
static void arcmsr_post_srb(struct AdapterControlBlock *acb, struct CommandControlBlock *srb)
{
	u_int32_t cdb_shifted_phyaddr=(u_int32_t) srb->cdb_shifted_phyaddr;
	struct ARCMSR_CDB * arcmsr_cdb=(struct ARCMSR_CDB *)&srb->arcmsr_cdb;
	
	bus_dmamap_sync(acb->srb_dmat, acb->srb_dmamap, (srb->srb_flags & SRB_FLAG_WRITE) ? BUS_DMASYNC_POSTWRITE:BUS_DMASYNC_POSTREAD);
	atomic_add_int(&acb->srboutstandingcount, 1);
	srb->srb_state=ARCMSR_SRB_START;

	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			if(arcmsr_cdb->Flags & ARCMSR_CDB_FLAG_SGL_BSIZE) {
				CHIP_REG_WRITE32(HBA_MessageUnit, 0, inbound_queueport, cdb_shifted_phyaddr|ARCMSR_SRBPOST_FLAG_SGL_BSIZE);
			} else {
				CHIP_REG_WRITE32(HBA_MessageUnit, 0, inbound_queueport, cdb_shifted_phyaddr);
			}
		}
		break;
	case ACB_ADAPTER_TYPE_B: {
			struct HBB_MessageUnit *phbbmu=(struct HBB_MessageUnit *)acb->pmu;
			int ending_index, index;
	
			index=phbbmu->postq_index;
			ending_index=((index+1)%ARCMSR_MAX_HBB_POSTQUEUE);
			phbbmu->post_qbuffer[ending_index]=0;
			if(arcmsr_cdb->Flags & ARCMSR_CDB_FLAG_SGL_BSIZE) {
				phbbmu->post_qbuffer[index]= cdb_shifted_phyaddr|ARCMSR_SRBPOST_FLAG_SGL_BSIZE;
			} else {
				phbbmu->post_qbuffer[index]= cdb_shifted_phyaddr;
			}
			index++;
			index %= ARCMSR_MAX_HBB_POSTQUEUE;     /*if last index number set it to 0 */
			phbbmu->postq_index=index;
			CHIP_REG_WRITE32(HBB_DOORBELL, 0, drv2iop_doorbell, ARCMSR_DRV2IOP_CDB_POSTED);
		}
		break;
    case ACB_ADAPTER_TYPE_C:
        {
            u_int32_t ccb_post_stamp, arc_cdb_size, cdb_phyaddr_hi32;

            arc_cdb_size=(srb->arc_cdb_size>0x300)?0x300:srb->arc_cdb_size;
            ccb_post_stamp=(cdb_shifted_phyaddr | ((arc_cdb_size-1) >> 6) | 1);
			cdb_phyaddr_hi32 = acb->srb_phyaddr.B.phyadd_high;
            if(cdb_phyaddr_hi32)
            {
			    CHIP_REG_WRITE32(HBC_MessageUnit,0,inbound_queueport_high, cdb_phyaddr_hi32);
			    CHIP_REG_WRITE32(HBC_MessageUnit,0,inbound_queueport_low, ccb_post_stamp);
            }
            else
            {
			    CHIP_REG_WRITE32(HBC_MessageUnit,0,inbound_queueport_low, ccb_post_stamp);
            }
        }
        break;
	}
}
/*
************************************************************************
************************************************************************
*/
static struct QBUFFER * arcmsr_get_iop_rqbuffer( struct AdapterControlBlock *acb)
{
	struct QBUFFER *qbuffer=NULL;
	
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			struct HBA_MessageUnit *phbamu=(struct HBA_MessageUnit *)acb->pmu;
	
			qbuffer=(struct QBUFFER *)&phbamu->message_rbuffer;
		}
		break;
	case ACB_ADAPTER_TYPE_B: {
			struct HBB_MessageUnit *phbbmu=(struct HBB_MessageUnit *)acb->pmu;
	
			qbuffer=(struct QBUFFER *)&phbbmu->hbb_rwbuffer->message_rbuffer;
		}
		break;
	case ACB_ADAPTER_TYPE_C: {
			struct HBC_MessageUnit *phbcmu=(struct HBC_MessageUnit *)acb->pmu;
	
			qbuffer=(struct QBUFFER *)&phbcmu->message_rbuffer;
		}
		break;
	}
	return(qbuffer);
}
/*
************************************************************************
************************************************************************
*/
static struct QBUFFER * arcmsr_get_iop_wqbuffer( struct AdapterControlBlock *acb)
{
	struct QBUFFER *qbuffer=NULL;
	
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			struct HBA_MessageUnit *phbamu=(struct HBA_MessageUnit *)acb->pmu;
	
			qbuffer=(struct QBUFFER *)&phbamu->message_wbuffer;
		}
		break;
	case ACB_ADAPTER_TYPE_B: {
			struct HBB_MessageUnit *phbbmu=(struct HBB_MessageUnit *)acb->pmu;
	
			qbuffer=(struct QBUFFER *)&phbbmu->hbb_rwbuffer->message_wbuffer;
		}
		break;
	case ACB_ADAPTER_TYPE_C: {
			struct HBC_MessageUnit *phbcmu=(struct HBC_MessageUnit *)acb->pmu;
	
			qbuffer=(struct QBUFFER *)&phbcmu->message_wbuffer;
		}
		break;
	}
	return(qbuffer);
}
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_iop_message_read(struct AdapterControlBlock *acb)
{
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			/* let IOP know data has been read */
			CHIP_REG_WRITE32(HBA_MessageUnit, 0, inbound_doorbell, ARCMSR_INBOUND_DRIVER_DATA_READ_OK);
		}
		break;
	case ACB_ADAPTER_TYPE_B: {
			/* let IOP know data has been read */
			CHIP_REG_WRITE32(HBB_DOORBELL, 0, drv2iop_doorbell, ARCMSR_DRV2IOP_DATA_READ_OK);
		}
		break;
	case ACB_ADAPTER_TYPE_C: {
			/* let IOP know data has been read */
			CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_doorbell, ARCMSR_HBCMU_DRV2IOP_DATA_READ_OK);
		}
	}
}
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_iop_message_wrote(struct AdapterControlBlock *acb)
{
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			/*
			** push inbound doorbell tell iop, driver data write ok 
			** and wait reply on next hwinterrupt for next Qbuffer post
			*/
			CHIP_REG_WRITE32(HBA_MessageUnit, 0, inbound_doorbell, ARCMSR_INBOUND_DRIVER_DATA_WRITE_OK);
		}
		break;
	case ACB_ADAPTER_TYPE_B: {
			/*
			** push inbound doorbell tell iop, driver data write ok 
			** and wait reply on next hwinterrupt for next Qbuffer post
			*/
			CHIP_REG_WRITE32(HBB_DOORBELL, 0, drv2iop_doorbell, ARCMSR_DRV2IOP_DATA_WRITE_OK);
		}
		break;
	case ACB_ADAPTER_TYPE_C: {
			/*
			** push inbound doorbell tell iop, driver data write ok 
			** and wait reply on next hwinterrupt for next Qbuffer post
			*/
			CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_doorbell, ARCMSR_HBCMU_DRV2IOP_DATA_WRITE_OK);
		}
		break;
	}
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_post_ioctldata2iop(struct AdapterControlBlock *acb)
{
	u_int8_t *pQbuffer;
	struct QBUFFER *pwbuffer;
	u_int8_t * iop_data;
	int32_t allxfer_len=0;
	
	pwbuffer=arcmsr_get_iop_wqbuffer(acb);
	iop_data=(u_int8_t *)pwbuffer->data;
	if(acb->acb_flags & ACB_F_MESSAGE_WQBUFFER_READ) {
		acb->acb_flags &= (~ACB_F_MESSAGE_WQBUFFER_READ);
		while((acb->wqbuf_firstindex!=acb->wqbuf_lastindex) 
			&& (allxfer_len<124)) {
			pQbuffer=&acb->wqbuffer[acb->wqbuf_firstindex];
			memcpy(iop_data, pQbuffer, 1);
			acb->wqbuf_firstindex++;
			acb->wqbuf_firstindex %=ARCMSR_MAX_QBUFFER; /*if last index number set it to 0 */
			iop_data++;
			allxfer_len++;
		}
		pwbuffer->data_len=allxfer_len;
		/*
		** push inbound doorbell and wait reply at hwinterrupt routine for next Qbuffer post
		*/
		arcmsr_iop_message_wrote(acb);
	}
}
/*
************************************************************************
************************************************************************
*/
static void arcmsr_stop_hba_bgrb(struct AdapterControlBlock *acb)
{
	acb->acb_flags &=~ACB_F_MSG_START_BGRB;
	CHIP_REG_WRITE32(HBA_MessageUnit, 
	0, inbound_msgaddr0, ARCMSR_INBOUND_MESG0_STOP_BGRB);
	if(!arcmsr_hba_wait_msgint_ready(acb)) {
		printf("arcmsr%d: wait 'stop adapter background rebulid' timeout \n"
			, acb->pci_unit);
	}
}
/*
************************************************************************
************************************************************************
*/
static void arcmsr_stop_hbb_bgrb(struct AdapterControlBlock *acb)
{
	acb->acb_flags &= ~ACB_F_MSG_START_BGRB;
	CHIP_REG_WRITE32(HBB_DOORBELL, 
	0, drv2iop_doorbell, ARCMSR_MESSAGE_STOP_BGRB);
	if(!arcmsr_hbb_wait_msgint_ready(acb)) {
		printf( "arcmsr%d: wait 'stop adapter background rebulid' timeout \n"
			, acb->pci_unit);
	}
}
/*
************************************************************************
************************************************************************
*/
static void arcmsr_stop_hbc_bgrb(struct AdapterControlBlock *acb)
{
	acb->acb_flags &=~ACB_F_MSG_START_BGRB;
	CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_msgaddr0, ARCMSR_INBOUND_MESG0_STOP_BGRB);
	CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_doorbell,ARCMSR_HBCMU_DRV2IOP_MESSAGE_CMD_DONE);
	if(!arcmsr_hbc_wait_msgint_ready(acb)) {
		printf("arcmsr%d: wait 'stop adapter background rebulid' timeout \n", acb->pci_unit);
	}
}
/*
************************************************************************
************************************************************************
*/
static void arcmsr_stop_adapter_bgrb(struct AdapterControlBlock *acb)
{
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			arcmsr_stop_hba_bgrb(acb);
		}
		break;
	case ACB_ADAPTER_TYPE_B: {
			arcmsr_stop_hbb_bgrb(acb);
		}
		break;
	case ACB_ADAPTER_TYPE_C: {
			arcmsr_stop_hbc_bgrb(acb);
		}
		break;
	}
}
/*
************************************************************************
************************************************************************
*/
static void arcmsr_poll(struct cam_sim * psim)
{
	struct AdapterControlBlock *acb;
	int	mutex;

	acb = (struct AdapterControlBlock *)cam_sim_softc(psim);
	mutex = mtx_owned(&acb->qbuffer_lock);
	if( mutex == 0 )
		ARCMSR_LOCK_ACQUIRE(&acb->qbuffer_lock);
	arcmsr_interrupt(acb);
	if( mutex == 0 )
		ARCMSR_LOCK_RELEASE(&acb->qbuffer_lock);
}
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_iop2drv_data_wrote_handle(struct AdapterControlBlock *acb)
{
	struct QBUFFER *prbuffer;
	u_int8_t *pQbuffer;
	u_int8_t *iop_data;
	int my_empty_len, iop_len, rqbuf_firstindex, rqbuf_lastindex;
	
	/*check this iop data if overflow my rqbuffer*/
	rqbuf_lastindex=acb->rqbuf_lastindex;
	rqbuf_firstindex=acb->rqbuf_firstindex;
	prbuffer=arcmsr_get_iop_rqbuffer(acb);
	iop_data=(u_int8_t *)prbuffer->data;
	iop_len=prbuffer->data_len;
	my_empty_len=(rqbuf_firstindex-rqbuf_lastindex-1)&(ARCMSR_MAX_QBUFFER-1);
	if(my_empty_len>=iop_len) {
		while(iop_len > 0) {
			pQbuffer=&acb->rqbuffer[rqbuf_lastindex];
			memcpy(pQbuffer, iop_data, 1);
			rqbuf_lastindex++;
			rqbuf_lastindex %= ARCMSR_MAX_QBUFFER;/*if last index number set it to 0 */
			iop_data++;
			iop_len--;
		}
		acb->rqbuf_lastindex=rqbuf_lastindex;
		arcmsr_iop_message_read(acb);
		/*signature, let IOP know data has been read */
	} else {
		acb->acb_flags|=ACB_F_IOPDATA_OVERFLOW;
	}
}
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_iop2drv_data_read_handle(struct AdapterControlBlock *acb)
{
	acb->acb_flags |= ACB_F_MESSAGE_WQBUFFER_READ;
	/*
	*****************************************************************
	**   check if there are any mail packages from user space program
	**   in my post bag, now is the time to send them into Areca's firmware
	*****************************************************************
	*/
	if(acb->wqbuf_firstindex!=acb->wqbuf_lastindex) {
		u_int8_t *pQbuffer;
		struct QBUFFER *pwbuffer;
		u_int8_t *iop_data;
		int allxfer_len=0;
	
		acb->acb_flags &= (~ACB_F_MESSAGE_WQBUFFER_READ);
		pwbuffer=arcmsr_get_iop_wqbuffer(acb);
		iop_data=(u_int8_t *)pwbuffer->data;
		while((acb->wqbuf_firstindex!=acb->wqbuf_lastindex) 
			&& (allxfer_len<124)) {
			pQbuffer=&acb->wqbuffer[acb->wqbuf_firstindex];
			memcpy(iop_data, pQbuffer, 1);
			acb->wqbuf_firstindex++;
			acb->wqbuf_firstindex %=ARCMSR_MAX_QBUFFER; /*if last index number set it to 0 */
			iop_data++;
			allxfer_len++;
		}
		pwbuffer->data_len=allxfer_len;
		/*
		** push inbound doorbell tell iop driver data write ok 
		** and wait reply on next hwinterrupt for next Qbuffer post
		*/
		arcmsr_iop_message_wrote(acb);
	}
	if(acb->wqbuf_firstindex==acb->wqbuf_lastindex) {
		acb->acb_flags |= ACB_F_MESSAGE_WQBUFFER_CLEARED;
	}
}

static void arcmsr_rescanLun_cb(struct cam_periph *periph, union ccb *ccb)
{
/*
	if (ccb->ccb_h.status != CAM_REQ_CMP)
		printf("arcmsr_rescanLun_cb: Rescan Target=%x, lun=%x, failure status=%x\n",ccb->ccb_h.target_id,ccb->ccb_h.target_lun,ccb->ccb_h.status);
	else
		printf("arcmsr_rescanLun_cb: Rescan lun successfully!\n");
*/
	xpt_free_path(ccb->ccb_h.path);
	xpt_free_ccb(ccb);
}

static void	arcmsr_rescan_lun(struct AdapterControlBlock *acb, int target, int lun)
{
	struct cam_path     *path;
	union ccb           *ccb;

	if ((ccb = (union ccb *)xpt_alloc_ccb_nowait()) == NULL)
 		return;
	if (xpt_create_path(&path, xpt_periph, cam_sim_path(acb->psim), target, lun) != CAM_REQ_CMP)
	{
		xpt_free_ccb(ccb);
		return;
	}
/*	printf("arcmsr_rescan_lun: Rescan Target=%x, Lun=%x\n", target, lun); */
	bzero(ccb, sizeof(union ccb));
	xpt_setup_ccb(&ccb->ccb_h, path, 5);
	ccb->ccb_h.func_code = XPT_SCAN_LUN;
	ccb->ccb_h.cbfcnp = arcmsr_rescanLun_cb;
	ccb->crcn.flags = CAM_FLAG_NONE;
	xpt_action(ccb);
}


static void arcmsr_abort_dr_ccbs(struct AdapterControlBlock *acb, int target, int lun)
{
   	struct CommandControlBlock *srb;
	u_int32_t intmask_org;
   	int i;

	ARCMSR_LOCK_ACQUIRE(&acb->qbuffer_lock);
	/* disable all outbound interrupts */
	intmask_org = arcmsr_disable_allintr(acb);
	for (i = 0; i < ARCMSR_MAX_FREESRB_NUM; i++)
	{
		srb = acb->psrb_pool[i];
		if (srb->srb_state == ARCMSR_SRB_START)
		{
           	if((target == srb->pccb->ccb_h.target_id) && (lun == srb->pccb->ccb_h.target_lun))
            {
		    	srb->srb_state = ARCMSR_SRB_ABORTED;
				srb->pccb->ccb_h.status |= CAM_REQ_ABORTED;
		    	arcmsr_srb_complete(srb, 1);
				printf("arcmsr%d: abort scsi id %d lun %d srb=%p \n", acb->pci_unit, target, lun, srb);
       		}
		}
	}
	/* enable outbound Post Queue, outbound doorbell Interrupt */
	arcmsr_enable_allintr(acb, intmask_org);
	ARCMSR_LOCK_RELEASE(&acb->qbuffer_lock);
}


/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_dr_handle(struct AdapterControlBlock *acb) {
	u_int32_t	devicemap;
	u_int32_t	target, lun;
    u_int32_t	deviceMapCurrent[4]={0};
    u_int8_t	*pDevMap;

	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A:
			devicemap = offsetof(struct HBA_MessageUnit, msgcode_rwbuffer[ARCMSR_FW_DEVMAP_OFFSET]);
			for (target= 0; target < 4; target++) 
			{
            	deviceMapCurrent[target]=bus_space_read_4(acb->btag[0], acb->bhandle[0],  devicemap);
            	devicemap += 4;
			}
			break;

	case ACB_ADAPTER_TYPE_B:
			devicemap = offsetof(struct HBB_RWBUFFER, msgcode_rwbuffer[ARCMSR_FW_DEVMAP_OFFSET]);
			for (target= 0; target < 4; target++) 
			{
            	deviceMapCurrent[target]=bus_space_read_4(acb->btag[1], acb->bhandle[1],  devicemap);
            	devicemap += 4;
			}
			break;

	case ACB_ADAPTER_TYPE_C:
			devicemap = offsetof(struct HBC_MessageUnit, msgcode_rwbuffer[ARCMSR_FW_DEVMAP_OFFSET]);
			for (target= 0; target < 4; target++) 
			{
            	deviceMapCurrent[target]=bus_space_read_4(acb->btag[0], acb->bhandle[0],  devicemap);
            	devicemap += 4;
			}
			break;
	}

		if(acb->acb_flags & ACB_F_BUS_HANG_ON)
		{
			acb->acb_flags &= ~ACB_F_BUS_HANG_ON;
		}
		/* 
		** adapter posted CONFIG message 
		** copy the new map, note if there are differences with the current map
		*/
		pDevMap = (u_int8_t	*)&deviceMapCurrent[0];
		for (target= 0; target < ARCMSR_MAX_TARGETID - 1; target++) 
		{
			if (*pDevMap != acb->device_map[target])
			{
                u_int8_t difference, bit_check;

                difference= *pDevMap ^ acb->device_map[target];
                for(lun=0; lun < ARCMSR_MAX_TARGETLUN; lun++)
                {
                    bit_check=(1 << lun);						/*check bit from 0....31*/
                    if(difference & bit_check)
                    {
                        if(acb->device_map[target] & bit_check)
                        {/* unit departed */
							printf("arcmsr_dr_handle: Target=%x, lun=%x, GONE!!!\n",target,lun);
 							arcmsr_abort_dr_ccbs(acb, target, lun);
                        	arcmsr_rescan_lun(acb, target, lun);
        					acb->devstate[target][lun] = ARECA_RAID_GONE;
                        }
                        else
                        {/* unit arrived */
							printf("arcmsr_dr_handle: Target=%x, lun=%x, Plug-IN!!!\n",target,lun);
                        	arcmsr_rescan_lun(acb, target, lun);
        					acb->devstate[target][lun] = ARECA_RAID_GOOD;
                        }
                    }
                }
/*				printf("arcmsr_dr_handle: acb->device_map[%x]=0x%x, deviceMapCurrent[%x]=%x\n",target,acb->device_map[target],target,*pDevMap); */
				acb->device_map[target]= *pDevMap;
			}
			pDevMap++;
		}
}
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_hba_message_isr(struct AdapterControlBlock *acb) {
	u_int32_t outbound_message;

	CHIP_REG_WRITE32(HBA_MessageUnit, 0, outbound_intstatus, ARCMSR_MU_OUTBOUND_MESSAGE0_INT);
	outbound_message = CHIP_REG_READ32(HBA_MessageUnit, 0, msgcode_rwbuffer[0]);
	if (outbound_message == ARCMSR_SIGNATURE_GET_CONFIG)
		arcmsr_dr_handle( acb );
}
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_hbb_message_isr(struct AdapterControlBlock *acb) {
	u_int32_t outbound_message;

	/* clear interrupts */
	CHIP_REG_WRITE32(HBB_DOORBELL, 0, iop2drv_doorbell, ARCMSR_MESSAGE_INT_CLEAR_PATTERN);
	outbound_message = CHIP_REG_READ32(HBB_RWBUFFER, 1, msgcode_rwbuffer[0]);
	if (outbound_message == ARCMSR_SIGNATURE_GET_CONFIG)
		arcmsr_dr_handle( acb );
}
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_hbc_message_isr(struct AdapterControlBlock *acb) {
	u_int32_t outbound_message;

	CHIP_REG_WRITE32(HBC_MessageUnit, 0, outbound_doorbell_clear, ARCMSR_HBCMU_IOP2DRV_MESSAGE_CMD_DONE_DOORBELL_CLEAR);
	outbound_message = CHIP_REG_READ32(HBC_MessageUnit, 0, msgcode_rwbuffer[0]);
	if (outbound_message == ARCMSR_SIGNATURE_GET_CONFIG)
		arcmsr_dr_handle( acb );
}
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_hba_doorbell_isr(struct AdapterControlBlock *acb)
{
	u_int32_t outbound_doorbell;
	
	/*
	*******************************************************************
	**  Maybe here we need to check wrqbuffer_lock is lock or not
	**  DOORBELL: din! don! 
	**  check if there are any mail need to pack from firmware
	*******************************************************************
	*/
	outbound_doorbell=CHIP_REG_READ32(HBA_MessageUnit, 
	0, outbound_doorbell);
	CHIP_REG_WRITE32(HBA_MessageUnit, 
	0, outbound_doorbell, outbound_doorbell); /* clear doorbell interrupt */
	if(outbound_doorbell & ARCMSR_OUTBOUND_IOP331_DATA_WRITE_OK) {
		arcmsr_iop2drv_data_wrote_handle(acb);
	}
	if(outbound_doorbell & ARCMSR_OUTBOUND_IOP331_DATA_READ_OK) {
		arcmsr_iop2drv_data_read_handle(acb);
	}
}
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_hbc_doorbell_isr(struct AdapterControlBlock *acb)
{
	u_int32_t outbound_doorbell;
	
	/*
	*******************************************************************
	**  Maybe here we need to check wrqbuffer_lock is lock or not
	**  DOORBELL: din! don! 
	**  check if there are any mail need to pack from firmware
	*******************************************************************
	*/
	outbound_doorbell=CHIP_REG_READ32(HBC_MessageUnit, 0, outbound_doorbell);
	CHIP_REG_WRITE32(HBC_MessageUnit, 0, outbound_doorbell_clear, outbound_doorbell); /* clear doorbell interrupt */
	if(outbound_doorbell & ARCMSR_HBCMU_IOP2DRV_DATA_WRITE_OK) {
		arcmsr_iop2drv_data_wrote_handle(acb);
	}
	if(outbound_doorbell & ARCMSR_HBCMU_IOP2DRV_DATA_READ_OK) {
		arcmsr_iop2drv_data_read_handle(acb);
	}
	if(outbound_doorbell & ARCMSR_HBCMU_IOP2DRV_MESSAGE_CMD_DONE) {
		arcmsr_hbc_message_isr(acb);    /* messenger of "driver to iop commands" */
	}
}
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_hba_postqueue_isr(struct AdapterControlBlock *acb)
{
	u_int32_t flag_srb;
	u_int16_t error;
	
	/*
	*****************************************************************************
	**               areca cdb command done
	*****************************************************************************
	*/
	bus_dmamap_sync(acb->srb_dmat, acb->srb_dmamap, 
		BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	while((flag_srb=CHIP_REG_READ32(HBA_MessageUnit, 
		0, outbound_queueport)) != 0xFFFFFFFF) {
		/* check if command done with no error*/
        error=(flag_srb & ARCMSR_SRBREPLY_FLAG_ERROR_MODE0)?TRUE:FALSE;
		arcmsr_drain_donequeue(acb, flag_srb, error);
	}	/*drain reply FIFO*/
}
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_hbb_postqueue_isr(struct AdapterControlBlock *acb)
{
	struct HBB_MessageUnit *phbbmu=(struct HBB_MessageUnit *)acb->pmu;
	u_int32_t flag_srb;
	int index;
	u_int16_t error;

	/*
	*****************************************************************************
	**               areca cdb command done
	*****************************************************************************
	*/
	bus_dmamap_sync(acb->srb_dmat, acb->srb_dmamap, 
		BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	index=phbbmu->doneq_index;
	while((flag_srb=phbbmu->done_qbuffer[index]) != 0) {
		phbbmu->done_qbuffer[index]=0;
		index++;
		index %= ARCMSR_MAX_HBB_POSTQUEUE;     /*if last index number set it to 0 */
		phbbmu->doneq_index=index;
		/* check if command done with no error*/
        error=(flag_srb & ARCMSR_SRBREPLY_FLAG_ERROR_MODE0)?TRUE:FALSE;
		arcmsr_drain_donequeue(acb, flag_srb, error);
	}	/*drain reply FIFO*/
}
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_hbc_postqueue_isr(struct AdapterControlBlock *acb)
{
	u_int32_t flag_srb,throttling=0;
	u_int16_t error;
	
	/*
	*****************************************************************************
	**               areca cdb command done
	*****************************************************************************
	*/
	bus_dmamap_sync(acb->srb_dmat, acb->srb_dmamap, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		
	while(CHIP_REG_READ32(HBC_MessageUnit, 0, host_int_status) & ARCMSR_HBCMU_OUTBOUND_POSTQUEUE_ISR) {
		
		flag_srb=CHIP_REG_READ32(HBC_MessageUnit, 0, outbound_queueport_low);
		/* check if command done with no error*/
        error=(flag_srb & ARCMSR_SRBREPLY_FLAG_ERROR_MODE1)?TRUE:FALSE;
		arcmsr_drain_donequeue(acb, flag_srb, error);
        if(throttling==ARCMSR_HBC_ISR_THROTTLING_LEVEL) {
            CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_doorbell,ARCMSR_HBCMU_DRV2IOP_POSTQUEUE_THROTTLING);
            break;
        }
        throttling++;
	}	/*drain reply FIFO*/
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_handle_hba_isr( struct AdapterControlBlock *acb)
{
	u_int32_t outbound_intStatus;
	/*
	*********************************************
	**   check outbound intstatus 
	*********************************************
	*/
	outbound_intStatus=CHIP_REG_READ32(HBA_MessageUnit, 0, outbound_intstatus) & acb->outbound_int_enable;
	if(!outbound_intStatus) {
		/*it must be share irq*/
		return;
	}
	CHIP_REG_WRITE32(HBA_MessageUnit, 0, outbound_intstatus, outbound_intStatus);/*clear interrupt*/
	/* MU doorbell interrupts*/
	if(outbound_intStatus & ARCMSR_MU_OUTBOUND_DOORBELL_INT) {
		arcmsr_hba_doorbell_isr(acb);
	}
	/* MU post queue interrupts*/
	if(outbound_intStatus & ARCMSR_MU_OUTBOUND_POSTQUEUE_INT) {
		arcmsr_hba_postqueue_isr(acb);
	}
	if(outbound_intStatus & ARCMSR_MU_OUTBOUND_MESSAGE0_INT) {
		arcmsr_hba_message_isr(acb);
	}
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_handle_hbb_isr( struct AdapterControlBlock *acb)
{
	u_int32_t outbound_doorbell;
	/*
	*********************************************
	**   check outbound intstatus 
	*********************************************
	*/
	outbound_doorbell=CHIP_REG_READ32(HBB_DOORBELL, 0, iop2drv_doorbell) & acb->outbound_int_enable;
	if(!outbound_doorbell) {
		/*it must be share irq*/
		return;
	}
	CHIP_REG_WRITE32(HBB_DOORBELL, 0, iop2drv_doorbell, ~outbound_doorbell); /* clear doorbell interrupt */
	CHIP_REG_READ32(HBB_DOORBELL, 0, iop2drv_doorbell);
	CHIP_REG_WRITE32(HBB_DOORBELL, 0, drv2iop_doorbell, ARCMSR_DRV2IOP_END_OF_INTERRUPT);
	/* MU ioctl transfer doorbell interrupts*/
	if(outbound_doorbell & ARCMSR_IOP2DRV_DATA_WRITE_OK) {
		arcmsr_iop2drv_data_wrote_handle(acb);
	}
	if(outbound_doorbell & ARCMSR_IOP2DRV_DATA_READ_OK) {
		arcmsr_iop2drv_data_read_handle(acb);
	}
	/* MU post queue interrupts*/
	if(outbound_doorbell & ARCMSR_IOP2DRV_CDB_DONE) {
		arcmsr_hbb_postqueue_isr(acb);
	}
	if(outbound_doorbell & ARCMSR_IOP2DRV_MESSAGE_CMD_DONE) {
		arcmsr_hbb_message_isr(acb);
	}
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_handle_hbc_isr( struct AdapterControlBlock *acb)
{
	u_int32_t host_interrupt_status;
	/*
	*********************************************
	**   check outbound intstatus 
	*********************************************
	*/
	host_interrupt_status=CHIP_REG_READ32(HBC_MessageUnit, 0, host_int_status);
	if(!host_interrupt_status) {
		/*it must be share irq*/
		return;
	}
	/* MU doorbell interrupts*/
	if(host_interrupt_status & ARCMSR_HBCMU_OUTBOUND_DOORBELL_ISR) {
		arcmsr_hbc_doorbell_isr(acb);
	}
	/* MU post queue interrupts*/
	if(host_interrupt_status & ARCMSR_HBCMU_OUTBOUND_POSTQUEUE_ISR) {
		arcmsr_hbc_postqueue_isr(acb);
	}
}
/*
******************************************************************************
******************************************************************************
*/
static void arcmsr_interrupt(struct AdapterControlBlock *acb)
{
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A:
		arcmsr_handle_hba_isr(acb);
		break;
	case ACB_ADAPTER_TYPE_B:
		arcmsr_handle_hbb_isr(acb);
		break;
	case ACB_ADAPTER_TYPE_C:
		arcmsr_handle_hbc_isr(acb);
		break;
	default:
		printf("arcmsr%d: interrupt service,"
		" unknow adapter type =%d\n", acb->pci_unit, acb->adapter_type);
		break;
	}
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_intr_handler(void *arg)
{
	struct AdapterControlBlock *acb=(struct AdapterControlBlock *)arg;
	
	ARCMSR_LOCK_ACQUIRE(&acb->qbuffer_lock);
	arcmsr_interrupt(acb);
	ARCMSR_LOCK_RELEASE(&acb->qbuffer_lock);
}
/*
******************************************************************************
******************************************************************************
*/
static void	arcmsr_polling_devmap(void* arg)
{
	struct AdapterControlBlock *acb = (struct AdapterControlBlock *)arg;
	switch (acb->adapter_type) {
    	case ACB_ADAPTER_TYPE_A:
			CHIP_REG_WRITE32(HBA_MessageUnit, 0, inbound_msgaddr0, ARCMSR_INBOUND_MESG0_GET_CONFIG);
	    	break;

    	case ACB_ADAPTER_TYPE_B:
			CHIP_REG_WRITE32(HBB_DOORBELL, 0, drv2iop_doorbell, ARCMSR_MESSAGE_GET_CONFIG);
	    	break;

    	case ACB_ADAPTER_TYPE_C:
			CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_msgaddr0, ARCMSR_INBOUND_MESG0_GET_CONFIG);
			CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_doorbell, ARCMSR_HBCMU_DRV2IOP_MESSAGE_CMD_DONE);
	    	break;
	}

	if((acb->acb_flags & ACB_F_SCSISTOPADAPTER) == 0)
	{
		callout_reset(&acb->devmap_callout, 5 * hz, arcmsr_polling_devmap, acb);	/* polling per 5 seconds */
	}
}

/*
*******************************************************************************
**
*******************************************************************************
*/
static void arcmsr_iop_parking(struct AdapterControlBlock *acb)
{
	u_int32_t intmask_org;

	if(acb!=NULL) {
		/* stop adapter background rebuild */
		if(acb->acb_flags & ACB_F_MSG_START_BGRB) {
			intmask_org = arcmsr_disable_allintr(acb);
			arcmsr_stop_adapter_bgrb(acb);
			arcmsr_flush_adapter_cache(acb);
			arcmsr_enable_allintr(acb, intmask_org);
		}
	}
}
/*
***********************************************************************
**
************************************************************************
*/
u_int32_t arcmsr_iop_ioctlcmd(struct AdapterControlBlock *acb, u_int32_t ioctl_cmd, caddr_t arg)
{
	struct CMD_MESSAGE_FIELD * pcmdmessagefld;
	u_int32_t retvalue=EINVAL;
	
	pcmdmessagefld=(struct CMD_MESSAGE_FIELD *) arg;
	if(memcmp(pcmdmessagefld->cmdmessage.Signature, "ARCMSR", 6)!=0) {
		return retvalue;
	}
	ARCMSR_LOCK_ACQUIRE(&acb->qbuffer_lock);
	switch(ioctl_cmd) {
	case ARCMSR_MESSAGE_READ_RQBUFFER: {
			u_int8_t * pQbuffer;
			u_int8_t * ptmpQbuffer=pcmdmessagefld->messagedatabuffer;			
			u_int32_t allxfer_len=0;
	
			while((acb->rqbuf_firstindex!=acb->rqbuf_lastindex) 
				&& (allxfer_len<1031)) {
				/*copy READ QBUFFER to srb*/
				pQbuffer= &acb->rqbuffer[acb->rqbuf_firstindex];
				memcpy(ptmpQbuffer, pQbuffer, 1);
				acb->rqbuf_firstindex++;
				acb->rqbuf_firstindex %= ARCMSR_MAX_QBUFFER; 
				/*if last index number set it to 0 */
				ptmpQbuffer++;
				allxfer_len++;
			}
			if(acb->acb_flags & ACB_F_IOPDATA_OVERFLOW) {
				struct QBUFFER * prbuffer;
				u_int8_t * iop_data;
				u_int32_t iop_len;
	
				acb->acb_flags &= ~ACB_F_IOPDATA_OVERFLOW;
				prbuffer=arcmsr_get_iop_rqbuffer(acb);
				iop_data=(u_int8_t *)prbuffer->data;
				iop_len=(u_int32_t)prbuffer->data_len;
				/*this iop data does no chance to make me overflow again here, so just do it*/
				while(iop_len>0) {
					pQbuffer= &acb->rqbuffer[acb->rqbuf_lastindex];
					memcpy(pQbuffer, iop_data, 1);
					acb->rqbuf_lastindex++;
					acb->rqbuf_lastindex %= ARCMSR_MAX_QBUFFER;
					/*if last index number set it to 0 */
					iop_data++;
					iop_len--;
				}
				arcmsr_iop_message_read(acb);
				/*signature, let IOP know data has been readed */
			}
			pcmdmessagefld->cmdmessage.Length=allxfer_len;
			pcmdmessagefld->cmdmessage.ReturnCode=ARCMSR_MESSAGE_RETURNCODE_OK;
			retvalue=ARCMSR_MESSAGE_SUCCESS;
		}
		break;
	case ARCMSR_MESSAGE_WRITE_WQBUFFER: {
			u_int32_t my_empty_len, user_len, wqbuf_firstindex, wqbuf_lastindex;
			u_int8_t * pQbuffer;
			u_int8_t * ptmpuserbuffer=pcmdmessagefld->messagedatabuffer;
	
			user_len=pcmdmessagefld->cmdmessage.Length;
			/*check if data xfer length of this request will overflow my array qbuffer */
			wqbuf_lastindex=acb->wqbuf_lastindex;
			wqbuf_firstindex=acb->wqbuf_firstindex;
			if(wqbuf_lastindex!=wqbuf_firstindex) {
				arcmsr_post_ioctldata2iop(acb);
				pcmdmessagefld->cmdmessage.ReturnCode=ARCMSR_MESSAGE_RETURNCODE_ERROR;
			} else {
				my_empty_len=(wqbuf_firstindex-wqbuf_lastindex-1)&(ARCMSR_MAX_QBUFFER-1);
				if(my_empty_len>=user_len) {
					while(user_len>0) {
						/*copy srb data to wqbuffer*/
						pQbuffer= &acb->wqbuffer[acb->wqbuf_lastindex];
						memcpy(pQbuffer, ptmpuserbuffer, 1);
						acb->wqbuf_lastindex++;
						acb->wqbuf_lastindex %= ARCMSR_MAX_QBUFFER;
						/*if last index number set it to 0 */
						ptmpuserbuffer++;
						user_len--;
					}
					/*post fist Qbuffer*/
					if(acb->acb_flags & ACB_F_MESSAGE_WQBUFFER_CLEARED) {
						acb->acb_flags &=~ACB_F_MESSAGE_WQBUFFER_CLEARED;
						arcmsr_post_ioctldata2iop(acb);
					}
					pcmdmessagefld->cmdmessage.ReturnCode=ARCMSR_MESSAGE_RETURNCODE_OK;
				} else {
					pcmdmessagefld->cmdmessage.ReturnCode=ARCMSR_MESSAGE_RETURNCODE_ERROR;
				}
			}
			retvalue=ARCMSR_MESSAGE_SUCCESS;
		}
		break;
	case ARCMSR_MESSAGE_CLEAR_RQBUFFER: {
			u_int8_t * pQbuffer=acb->rqbuffer;
	
			if(acb->acb_flags & ACB_F_IOPDATA_OVERFLOW) {
				acb->acb_flags &= ~ACB_F_IOPDATA_OVERFLOW;
				arcmsr_iop_message_read(acb);
				/*signature, let IOP know data has been readed */
			}
			acb->acb_flags |= ACB_F_MESSAGE_RQBUFFER_CLEARED;
			acb->rqbuf_firstindex=0;
			acb->rqbuf_lastindex=0;
			memset(pQbuffer, 0, ARCMSR_MAX_QBUFFER);
			pcmdmessagefld->cmdmessage.ReturnCode=ARCMSR_MESSAGE_RETURNCODE_OK;
			retvalue=ARCMSR_MESSAGE_SUCCESS;
		}
		break;
	case ARCMSR_MESSAGE_CLEAR_WQBUFFER:
		{
			u_int8_t * pQbuffer=acb->wqbuffer;
 
			if(acb->acb_flags & ACB_F_IOPDATA_OVERFLOW) {
				acb->acb_flags &= ~ACB_F_IOPDATA_OVERFLOW;
                arcmsr_iop_message_read(acb);
				/*signature, let IOP know data has been readed */
			}
			acb->acb_flags |= (ACB_F_MESSAGE_WQBUFFER_CLEARED|ACB_F_MESSAGE_WQBUFFER_READ);
			acb->wqbuf_firstindex=0;
			acb->wqbuf_lastindex=0;
			memset(pQbuffer, 0, ARCMSR_MAX_QBUFFER);
			pcmdmessagefld->cmdmessage.ReturnCode=ARCMSR_MESSAGE_RETURNCODE_OK;
			retvalue=ARCMSR_MESSAGE_SUCCESS;
		}
		break;
	case ARCMSR_MESSAGE_CLEAR_ALLQBUFFER: {
			u_int8_t * pQbuffer;
 
			if(acb->acb_flags & ACB_F_IOPDATA_OVERFLOW) {
				acb->acb_flags &= ~ACB_F_IOPDATA_OVERFLOW;
                arcmsr_iop_message_read(acb);
				/*signature, let IOP know data has been readed */
			}
			acb->acb_flags  |= (ACB_F_MESSAGE_WQBUFFER_CLEARED
					|ACB_F_MESSAGE_RQBUFFER_CLEARED
					|ACB_F_MESSAGE_WQBUFFER_READ);
			acb->rqbuf_firstindex=0;
			acb->rqbuf_lastindex=0;
			acb->wqbuf_firstindex=0;
			acb->wqbuf_lastindex=0;
			pQbuffer=acb->rqbuffer;
			memset(pQbuffer, 0, sizeof(struct QBUFFER));
			pQbuffer=acb->wqbuffer;
			memset(pQbuffer, 0, sizeof(struct QBUFFER));
			pcmdmessagefld->cmdmessage.ReturnCode=ARCMSR_MESSAGE_RETURNCODE_OK;
			retvalue=ARCMSR_MESSAGE_SUCCESS;
		}
		break;
	case ARCMSR_MESSAGE_REQUEST_RETURNCODE_3F: {
			pcmdmessagefld->cmdmessage.ReturnCode=ARCMSR_MESSAGE_RETURNCODE_3F;
			retvalue=ARCMSR_MESSAGE_SUCCESS;
		}
		break;
	case ARCMSR_MESSAGE_SAY_HELLO: {
			u_int8_t * hello_string="Hello! I am ARCMSR";
			u_int8_t * puserbuffer=(u_int8_t *)pcmdmessagefld->messagedatabuffer;
 
			if(memcpy(puserbuffer, hello_string, (int16_t)strlen(hello_string))) {
				pcmdmessagefld->cmdmessage.ReturnCode=ARCMSR_MESSAGE_RETURNCODE_ERROR;
				ARCMSR_LOCK_RELEASE(&acb->qbuffer_lock);
				return ENOIOCTL;
			}
			pcmdmessagefld->cmdmessage.ReturnCode=ARCMSR_MESSAGE_RETURNCODE_OK;
			retvalue=ARCMSR_MESSAGE_SUCCESS;
		}
		break;
	case ARCMSR_MESSAGE_SAY_GOODBYE: {
			arcmsr_iop_parking(acb);
			retvalue=ARCMSR_MESSAGE_SUCCESS;
		}
		break;
	case ARCMSR_MESSAGE_FLUSH_ADAPTER_CACHE: {
			arcmsr_flush_adapter_cache(acb);
			retvalue=ARCMSR_MESSAGE_SUCCESS;
		}
		break;
	}
	ARCMSR_LOCK_RELEASE(&acb->qbuffer_lock);
	return (retvalue);
}
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_free_srb(struct CommandControlBlock *srb)
{
	struct AdapterControlBlock	*acb;
	int	mutex;
	
	acb = srb->acb;
	mutex = mtx_owned(&acb->qbuffer_lock);
	if( mutex == 0 )
		ARCMSR_LOCK_ACQUIRE(&acb->qbuffer_lock);
	srb->srb_state=ARCMSR_SRB_DONE;
	srb->srb_flags=0;
	acb->srbworkingQ[acb->workingsrb_doneindex]=srb;
	acb->workingsrb_doneindex++;
	acb->workingsrb_doneindex %= ARCMSR_MAX_FREESRB_NUM;
	if( mutex == 0 )
		ARCMSR_LOCK_RELEASE(&acb->qbuffer_lock);
}
/*
**************************************************************************
**************************************************************************
*/
struct CommandControlBlock * arcmsr_get_freesrb(struct AdapterControlBlock *acb)
{
	struct CommandControlBlock *srb=NULL;
	u_int32_t workingsrb_startindex, workingsrb_doneindex;
	int	mutex;

	mutex = mtx_owned(&acb->qbuffer_lock);
	if( mutex == 0 )
		ARCMSR_LOCK_ACQUIRE(&acb->qbuffer_lock);
	workingsrb_doneindex=acb->workingsrb_doneindex;
	workingsrb_startindex=acb->workingsrb_startindex;
	srb=acb->srbworkingQ[workingsrb_startindex];
	workingsrb_startindex++;
	workingsrb_startindex %= ARCMSR_MAX_FREESRB_NUM;
	if(workingsrb_doneindex!=workingsrb_startindex) {
		acb->workingsrb_startindex=workingsrb_startindex;
	} else {
		srb=NULL;
	}
	if( mutex == 0 )
		ARCMSR_LOCK_RELEASE(&acb->qbuffer_lock);
	return(srb);
}
/*
**************************************************************************
**************************************************************************
*/
static int arcmsr_iop_message_xfer(struct AdapterControlBlock *acb, union ccb * pccb)
{
	struct CMD_MESSAGE_FIELD * pcmdmessagefld;
	int retvalue = 0, transfer_len = 0;
	char *buffer;
	u_int32_t controlcode = (u_int32_t ) pccb->csio.cdb_io.cdb_bytes[5] << 24 |
				(u_int32_t ) pccb->csio.cdb_io.cdb_bytes[6] << 16 |
				(u_int32_t ) pccb->csio.cdb_io.cdb_bytes[7] << 8  |
				(u_int32_t ) pccb->csio.cdb_io.cdb_bytes[8];
					/* 4 bytes: Areca io control code */
	if((pccb->ccb_h.flags & CAM_SCATTER_VALID) == 0) {
		buffer = pccb->csio.data_ptr;
		transfer_len = pccb->csio.dxfer_len;
	} else {
		retvalue = ARCMSR_MESSAGE_FAIL;
		goto message_out;
	}
	if (transfer_len > sizeof(struct CMD_MESSAGE_FIELD)) {
		retvalue = ARCMSR_MESSAGE_FAIL;
		goto message_out;
	}
	pcmdmessagefld = (struct CMD_MESSAGE_FIELD *) buffer;
	switch(controlcode) {
	case ARCMSR_MESSAGE_READ_RQBUFFER: {
			u_int8_t *pQbuffer;
			u_int8_t *ptmpQbuffer=pcmdmessagefld->messagedatabuffer;
			int32_t allxfer_len = 0;
	
			while ((acb->rqbuf_firstindex != acb->rqbuf_lastindex)
				&& (allxfer_len < 1031)) {
				pQbuffer = &acb->rqbuffer[acb->rqbuf_firstindex];
				memcpy(ptmpQbuffer, pQbuffer, 1);
				acb->rqbuf_firstindex++;
				acb->rqbuf_firstindex %= ARCMSR_MAX_QBUFFER;
				ptmpQbuffer++;
				allxfer_len++;
			}
			if (acb->acb_flags & ACB_F_IOPDATA_OVERFLOW) {
				struct QBUFFER  *prbuffer;
				u_int8_t  *iop_data;
				int32_t iop_len;
	
				acb->acb_flags &= ~ACB_F_IOPDATA_OVERFLOW;
				prbuffer=arcmsr_get_iop_rqbuffer(acb);
				iop_data = (u_int8_t *)prbuffer->data;
				iop_len =(u_int32_t)prbuffer->data_len;
				while (iop_len > 0) {
			        pQbuffer= &acb->rqbuffer[acb->rqbuf_lastindex];
					memcpy(pQbuffer, iop_data, 1);
					acb->rqbuf_lastindex++;
					acb->rqbuf_lastindex %= ARCMSR_MAX_QBUFFER;
					iop_data++;
					iop_len--;
				}
				arcmsr_iop_message_read(acb);
			}
			pcmdmessagefld->cmdmessage.Length = allxfer_len;
			pcmdmessagefld->cmdmessage.ReturnCode = ARCMSR_MESSAGE_RETURNCODE_OK;
			retvalue=ARCMSR_MESSAGE_SUCCESS;
		}
		break;
	case ARCMSR_MESSAGE_WRITE_WQBUFFER: {
			int32_t my_empty_len, user_len, wqbuf_firstindex, wqbuf_lastindex;
			u_int8_t *pQbuffer;
			u_int8_t *ptmpuserbuffer=pcmdmessagefld->messagedatabuffer;
	
			user_len = pcmdmessagefld->cmdmessage.Length;
			wqbuf_lastindex = acb->wqbuf_lastindex;
			wqbuf_firstindex = acb->wqbuf_firstindex;
			if (wqbuf_lastindex != wqbuf_firstindex) {
				arcmsr_post_ioctldata2iop(acb);
				/* has error report sensedata */
			    if(pccb->csio.sense_len) {
				((u_int8_t *)&pccb->csio.sense_data)[0] = (0x1 << 7 | 0x70); 
				/* Valid,ErrorCode */
				((u_int8_t *)&pccb->csio.sense_data)[2] = 0x05; 
				/* FileMark,EndOfMedia,IncorrectLength,Reserved,SenseKey */
				((u_int8_t *)&pccb->csio.sense_data)[7] = 0x0A; 
				/* AdditionalSenseLength */
				((u_int8_t *)&pccb->csio.sense_data)[12] = 0x20; 
				/* AdditionalSenseCode */
				}
				retvalue = ARCMSR_MESSAGE_FAIL;
			} else {
				my_empty_len = (wqbuf_firstindex-wqbuf_lastindex - 1)
						&(ARCMSR_MAX_QBUFFER - 1);
				if (my_empty_len >= user_len) {
					while (user_len > 0) {
						pQbuffer = &acb->wqbuffer[acb->wqbuf_lastindex];
						memcpy(pQbuffer, ptmpuserbuffer, 1);
						acb->wqbuf_lastindex++;
						acb->wqbuf_lastindex %= ARCMSR_MAX_QBUFFER;
						ptmpuserbuffer++;
						user_len--;
					}
					if (acb->acb_flags & ACB_F_MESSAGE_WQBUFFER_CLEARED) {
						acb->acb_flags &=
						~ACB_F_MESSAGE_WQBUFFER_CLEARED;
						arcmsr_post_ioctldata2iop(acb);
					}
				} else {
					/* has error report sensedata */
					if(pccb->csio.sense_len) {
					((u_int8_t *)&pccb->csio.sense_data)[0] = (0x1 << 7 | 0x70);
					/* Valid,ErrorCode */
					((u_int8_t *)&pccb->csio.sense_data)[2] = 0x05; 
					/* FileMark,EndOfMedia,IncorrectLength,Reserved,SenseKey */
					((u_int8_t *)&pccb->csio.sense_data)[7] = 0x0A; 
					/* AdditionalSenseLength */
					((u_int8_t *)&pccb->csio.sense_data)[12] = 0x20; 
					/* AdditionalSenseCode */
					}
					retvalue = ARCMSR_MESSAGE_FAIL;
				}
			}
		}
		break;
	case ARCMSR_MESSAGE_CLEAR_RQBUFFER: {
			u_int8_t *pQbuffer = acb->rqbuffer;
	
			if (acb->acb_flags & ACB_F_IOPDATA_OVERFLOW) {
				acb->acb_flags &= ~ACB_F_IOPDATA_OVERFLOW;
				arcmsr_iop_message_read(acb);
			}
			acb->acb_flags |= ACB_F_MESSAGE_RQBUFFER_CLEARED;
			acb->rqbuf_firstindex = 0;
			acb->rqbuf_lastindex = 0;
			memset(pQbuffer, 0, ARCMSR_MAX_QBUFFER);
			pcmdmessagefld->cmdmessage.ReturnCode =
			ARCMSR_MESSAGE_RETURNCODE_OK;
		}
		break;
	case ARCMSR_MESSAGE_CLEAR_WQBUFFER: {
			u_int8_t *pQbuffer = acb->wqbuffer;
	
			if (acb->acb_flags & ACB_F_IOPDATA_OVERFLOW) {
				acb->acb_flags &= ~ACB_F_IOPDATA_OVERFLOW;
				arcmsr_iop_message_read(acb);
			}
			acb->acb_flags |=
				(ACB_F_MESSAGE_WQBUFFER_CLEARED |
					ACB_F_MESSAGE_WQBUFFER_READ);
			acb->wqbuf_firstindex = 0;
			acb->wqbuf_lastindex = 0;
			memset(pQbuffer, 0, ARCMSR_MAX_QBUFFER);
			pcmdmessagefld->cmdmessage.ReturnCode =
				ARCMSR_MESSAGE_RETURNCODE_OK;
		}
		break;
	case ARCMSR_MESSAGE_CLEAR_ALLQBUFFER: {
			u_int8_t *pQbuffer;
	
			if (acb->acb_flags & ACB_F_IOPDATA_OVERFLOW) {
				acb->acb_flags &= ~ACB_F_IOPDATA_OVERFLOW;
				arcmsr_iop_message_read(acb);
			}
			acb->acb_flags |=
				(ACB_F_MESSAGE_WQBUFFER_CLEARED
				| ACB_F_MESSAGE_RQBUFFER_CLEARED
				| ACB_F_MESSAGE_WQBUFFER_READ);
			acb->rqbuf_firstindex = 0;
			acb->rqbuf_lastindex = 0;
			acb->wqbuf_firstindex = 0;
			acb->wqbuf_lastindex = 0;
			pQbuffer = acb->rqbuffer;
			memset(pQbuffer, 0, sizeof (struct QBUFFER));
			pQbuffer = acb->wqbuffer;
			memset(pQbuffer, 0, sizeof (struct QBUFFER));
			pcmdmessagefld->cmdmessage.ReturnCode = ARCMSR_MESSAGE_RETURNCODE_OK;
		}
		break;
	case ARCMSR_MESSAGE_REQUEST_RETURNCODE_3F: {
			pcmdmessagefld->cmdmessage.ReturnCode = ARCMSR_MESSAGE_RETURNCODE_3F;
		}
		break;
	case ARCMSR_MESSAGE_SAY_HELLO: {
			int8_t * hello_string = "Hello! I am ARCMSR";
	
			memcpy(pcmdmessagefld->messagedatabuffer, hello_string
				, (int16_t)strlen(hello_string));
			pcmdmessagefld->cmdmessage.ReturnCode = ARCMSR_MESSAGE_RETURNCODE_OK;
		}
		break;
	case ARCMSR_MESSAGE_SAY_GOODBYE:
		arcmsr_iop_parking(acb);
		break;
	case ARCMSR_MESSAGE_FLUSH_ADAPTER_CACHE:
		arcmsr_flush_adapter_cache(acb);
		break;
	default:
		retvalue = ARCMSR_MESSAGE_FAIL;
	}
message_out:
	return (retvalue);
}
/*
*********************************************************************
*********************************************************************
*/
static void arcmsr_execute_srb(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	struct CommandControlBlock *srb=(struct CommandControlBlock *)arg;
	struct AdapterControlBlock *acb=(struct AdapterControlBlock *)srb->acb;
	union ccb * pccb;
	int target, lun; 
	
	pccb=srb->pccb;
	target=pccb->ccb_h.target_id;
	lun=pccb->ccb_h.target_lun;
#ifdef ARCMSR_DEBUG1
	acb->pktRequestCount++;
#endif
	if(error != 0) {
		if(error != EFBIG) {
			printf("arcmsr%d: unexpected error %x"
				" returned from 'bus_dmamap_load' \n"
				, acb->pci_unit, error);
		}
		if((pccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_INPROG) {
			pccb->ccb_h.status |= CAM_REQ_TOO_BIG;
		}
		arcmsr_srb_complete(srb, 0);
		return;
	}
	if(nseg > ARCMSR_MAX_SG_ENTRIES) {
		pccb->ccb_h.status |= CAM_REQ_TOO_BIG;
		arcmsr_srb_complete(srb, 0);
		return;
	}
	if(acb->acb_flags & ACB_F_BUS_RESET) {
		printf("arcmsr%d: bus reset and return busy \n", acb->pci_unit);
		pccb->ccb_h.status |= CAM_SCSI_BUS_RESET;
		arcmsr_srb_complete(srb, 0);
		return;
	}
	if(acb->devstate[target][lun]==ARECA_RAID_GONE) {
		u_int8_t block_cmd, cmd;

		cmd = pccb->csio.cdb_io.cdb_bytes[0];
		block_cmd= cmd & 0x0f;
		if(block_cmd==0x08 || block_cmd==0x0a) {
			printf("arcmsr%d:block 'read/write' command "
				"with gone raid volume Cmd=0x%2x, TargetId=%d, Lun=%d \n"
				, acb->pci_unit, cmd, target, lun);
			pccb->ccb_h.status |= CAM_DEV_NOT_THERE;
			arcmsr_srb_complete(srb, 0);
			return;
		}
	}
	if((pccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_INPROG) {
		if(nseg != 0) {
			bus_dmamap_unload(acb->dm_segs_dmat, srb->dm_segs_dmamap);
		}
		arcmsr_srb_complete(srb, 0);
		return;
	}
	if(acb->srboutstandingcount > ARCMSR_MAX_OUTSTANDING_CMD) {
		xpt_freeze_simq(acb->psim, 1);
		pccb->ccb_h.status = CAM_REQUEUE_REQ;
		acb->acb_flags |= ACB_F_CAM_DEV_QFRZN;
		arcmsr_srb_complete(srb, 0);
		return;
	}
	pccb->ccb_h.status |= CAM_SIM_QUEUED;
	arcmsr_build_srb(srb, dm_segs, nseg);
	arcmsr_post_srb(acb, srb);
	if (pccb->ccb_h.timeout != CAM_TIME_INFINITY)
	{
		arcmsr_callout_init(&srb->ccb_callout);
		callout_reset(&srb->ccb_callout, ((pccb->ccb_h.timeout + (ARCMSR_TIMEOUT_DELAY * 1000)) * hz) / 1000, arcmsr_srb_timeout, srb);
		srb->srb_flags |= SRB_FLAG_TIMER_START;
	}
}
/*
*****************************************************************************************
*****************************************************************************************
*/
static u_int8_t arcmsr_seek_cmd2abort(union ccb * abortccb)
{
	struct CommandControlBlock *srb;
	struct AdapterControlBlock *acb=(struct AdapterControlBlock *) abortccb->ccb_h.arcmsr_ccbacb_ptr;
	u_int32_t intmask_org;
	int i=0;
	
	acb->num_aborts++;
	/*
	***************************************************************************
	** It is the upper layer do abort command this lock just prior to calling us.
	** First determine if we currently own this command.
	** Start by searching the device queue. If not found
	** at all, and the system wanted us to just abort the
	** command return success.
	***************************************************************************
	*/
	if(acb->srboutstandingcount!=0) {
		/* disable all outbound interrupt */
		intmask_org=arcmsr_disable_allintr(acb);
		for(i=0;i<ARCMSR_MAX_FREESRB_NUM;i++) {
			srb=acb->psrb_pool[i];
			if(srb->srb_state==ARCMSR_SRB_START) {
				if(srb->pccb==abortccb) {
					srb->srb_state=ARCMSR_SRB_ABORTED;
					printf("arcmsr%d:scsi id=%d lun=%d abort srb '%p'"
						"outstanding command \n"
						, acb->pci_unit, abortccb->ccb_h.target_id
						, abortccb->ccb_h.target_lun, srb);
					arcmsr_polling_srbdone(acb, srb);
					/* enable outbound Post Queue, outbound doorbell Interrupt */
					arcmsr_enable_allintr(acb, intmask_org);
					return (TRUE);
				}
			}
		}
		/* enable outbound Post Queue, outbound doorbell Interrupt */
		arcmsr_enable_allintr(acb, intmask_org);
	}
	return(FALSE);
}
/*
****************************************************************************
****************************************************************************
*/
static void arcmsr_bus_reset(struct AdapterControlBlock *acb)
{
	int retry=0;
	
	acb->num_resets++;
	acb->acb_flags |=ACB_F_BUS_RESET;
	while(acb->srboutstandingcount!=0 && retry < 400) {
		arcmsr_interrupt(acb);
		UDELAY(25000);
		retry++;
	}
	arcmsr_iop_reset(acb);
	acb->acb_flags &= ~ACB_F_BUS_RESET;
} 
/*
**************************************************************************
**************************************************************************
*/
static void arcmsr_handle_virtual_command(struct AdapterControlBlock *acb,
		union ccb * pccb)
{
	pccb->ccb_h.status |= CAM_REQ_CMP;
	switch (pccb->csio.cdb_io.cdb_bytes[0]) {
	case INQUIRY: {
		unsigned char inqdata[36];
		char *buffer=pccb->csio.data_ptr;
	
		if (pccb->ccb_h.target_lun) {
			pccb->ccb_h.status |= CAM_SEL_TIMEOUT;
			xpt_done(pccb);
			return;
		}
		inqdata[0] = T_PROCESSOR;	/* Periph Qualifier & Periph Dev Type */
		inqdata[1] = 0;				/* rem media bit & Dev Type Modifier */
		inqdata[2] = 0;				/* ISO, ECMA, & ANSI versions */
		inqdata[3] = 0;
		inqdata[4] = 31;			/* length of additional data */
		inqdata[5] = 0;
		inqdata[6] = 0;
		inqdata[7] = 0;
		strncpy(&inqdata[8], "Areca   ", 8);	/* Vendor Identification */
		strncpy(&inqdata[16], "RAID controller ", 16);	/* Product Identification */
		strncpy(&inqdata[32], "R001", 4); /* Product Revision */
		memcpy(buffer, inqdata, sizeof(inqdata));
		xpt_done(pccb);
	}
	break;
	case WRITE_BUFFER:
	case READ_BUFFER: {
		if (arcmsr_iop_message_xfer(acb, pccb)) {
			pccb->ccb_h.status |= CAM_SCSI_STATUS_ERROR;
			pccb->csio.scsi_status = SCSI_STATUS_CHECK_COND;
		}
		xpt_done(pccb);
	}
	break;
	default:
		xpt_done(pccb);
	}
}
/*
*********************************************************************
*********************************************************************
*/
static void arcmsr_action(struct cam_sim * psim, union ccb * pccb)
{
	struct AdapterControlBlock *  acb;
	
	acb=(struct AdapterControlBlock *) cam_sim_softc(psim);
	if(acb==NULL) {
		pccb->ccb_h.status |= CAM_REQ_INVALID;
		xpt_done(pccb);
		return;
	}
	switch (pccb->ccb_h.func_code) {
	case XPT_SCSI_IO: {
			struct CommandControlBlock *srb;
			int target=pccb->ccb_h.target_id;
	
			if(target == 16) {
				/* virtual device for iop message transfer */
				arcmsr_handle_virtual_command(acb, pccb);
				return;
			}
			if((srb=arcmsr_get_freesrb(acb)) == NULL) {
				pccb->ccb_h.status |= CAM_RESRC_UNAVAIL;
				xpt_done(pccb);
				return;
			}
			pccb->ccb_h.arcmsr_ccbsrb_ptr=srb;
			pccb->ccb_h.arcmsr_ccbacb_ptr=acb;
			srb->pccb=pccb;
			if((pccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
				if(!(pccb->ccb_h.flags & CAM_SCATTER_VALID)) {
					/* Single buffer */
					if(!(pccb->ccb_h.flags & CAM_DATA_PHYS)) {
						/* Buffer is virtual */
						u_int32_t error, s;
	
						s=splsoftvm();
						error =	bus_dmamap_load(acb->dm_segs_dmat
							, srb->dm_segs_dmamap
							, pccb->csio.data_ptr
							, pccb->csio.dxfer_len
							, arcmsr_execute_srb, srb, /*flags*/0);
	         				if(error == EINPROGRESS) {
							xpt_freeze_simq(acb->psim, 1);
							pccb->ccb_h.status |= CAM_RELEASE_SIMQ;
						}
						splx(s);
					}
					else {		/* Buffer is physical */
#ifdef	PAE
						panic("arcmsr: CAM_DATA_PHYS not supported");
#else
						struct bus_dma_segment seg;
						
						seg.ds_addr = (bus_addr_t)pccb->csio.data_ptr;
						seg.ds_len = pccb->csio.dxfer_len;
						arcmsr_execute_srb(srb, &seg, 1, 0);
#endif
					}
				} else { 
					/* Scatter/gather list */
					struct bus_dma_segment *segs;
	
					if((pccb->ccb_h.flags & CAM_SG_LIST_PHYS) == 0
					|| (pccb->ccb_h.flags & CAM_DATA_PHYS) != 0) {
						pccb->ccb_h.status |= CAM_PROVIDE_FAIL;
						xpt_done(pccb);
						free(srb, M_DEVBUF);
						return;
					}
					segs=(struct bus_dma_segment *)pccb->csio.data_ptr;
					arcmsr_execute_srb(srb, segs, pccb->csio.sglist_cnt, 0);
				}
			} else {
				arcmsr_execute_srb(srb, NULL, 0, 0);
			}
			break;
		}
	case XPT_TARGET_IO: {
			/* target mode not yet support vendor specific commands. */
			pccb->ccb_h.status |= CAM_REQ_CMP;
			xpt_done(pccb);
			break;
		}
	case XPT_PATH_INQ: {
			struct ccb_pathinq *cpi= &pccb->cpi;

			cpi->version_num=1;
			cpi->hba_inquiry=PI_SDTR_ABLE | PI_TAG_ABLE;
			cpi->target_sprt=0;
			cpi->hba_misc=0;
			cpi->hba_eng_cnt=0;
			cpi->max_target=ARCMSR_MAX_TARGETID;        /* 0-16 */
			cpi->max_lun=ARCMSR_MAX_TARGETLUN;	    /* 0-7 */
			cpi->initiator_id=ARCMSR_SCSI_INITIATOR_ID; /* 255 */
			cpi->bus_id=cam_sim_bus(psim);
			strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
			strncpy(cpi->hba_vid, "ARCMSR", HBA_IDLEN);
			strncpy(cpi->dev_name, cam_sim_name(psim), DEV_IDLEN);
			cpi->unit_number=cam_sim_unit(psim);
		#ifdef	CAM_NEW_TRAN_CODE
			if(acb->adapter_bus_speed == ACB_BUS_SPEED_6G)
				cpi->base_transfer_speed = 600000;
			else
				cpi->base_transfer_speed = 300000;
			if((acb->vendor_device_id == PCIDevVenIDARC1880) ||
			   (acb->vendor_device_id == PCIDevVenIDARC1680))
			{
				cpi->transport = XPORT_SAS;
				cpi->transport_version = 0;
				cpi->protocol_version = SCSI_REV_SPC2;
			}
			else
			{
				cpi->transport = XPORT_SPI;
				cpi->transport_version = 2;
				cpi->protocol_version = SCSI_REV_2;
			}
			cpi->protocol = PROTO_SCSI;
		#endif
			cpi->ccb_h.status |= CAM_REQ_CMP;
			xpt_done(pccb);
			break;
		}
	case XPT_ABORT: {
			union ccb *pabort_ccb;
	
			pabort_ccb=pccb->cab.abort_ccb;
			switch (pabort_ccb->ccb_h.func_code) {
			case XPT_ACCEPT_TARGET_IO:
			case XPT_IMMED_NOTIFY:
			case XPT_CONT_TARGET_IO:
				if(arcmsr_seek_cmd2abort(pabort_ccb)==TRUE) {
					pabort_ccb->ccb_h.status |= CAM_REQ_ABORTED;
					xpt_done(pabort_ccb);
					pccb->ccb_h.status |= CAM_REQ_CMP;
				} else {
					xpt_print_path(pabort_ccb->ccb_h.path);
					printf("Not found\n");
					pccb->ccb_h.status |= CAM_PATH_INVALID;
				}
				break;
			case XPT_SCSI_IO:
				pccb->ccb_h.status |= CAM_UA_ABORT;
				break;
			default:
				pccb->ccb_h.status |= CAM_REQ_INVALID;
				break;
			}
			xpt_done(pccb);
			break;
		}
	case XPT_RESET_BUS:
	case XPT_RESET_DEV: {
			u_int32_t     i;
	
			arcmsr_bus_reset(acb);
			for (i=0; i < 500; i++) {
				DELAY(1000);	
			}
			pccb->ccb_h.status |= CAM_REQ_CMP;
			xpt_done(pccb);
			break;
		}
	case XPT_TERM_IO: {
			pccb->ccb_h.status |= CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		}
	case XPT_GET_TRAN_SETTINGS: {
			struct ccb_trans_settings *cts;
	
			if(pccb->ccb_h.target_id == 16) {
				pccb->ccb_h.status |= CAM_FUNC_NOTAVAIL;
				xpt_done(pccb);
				break;
			}
			cts= &pccb->cts;
		#ifdef	CAM_NEW_TRAN_CODE
			{
				struct ccb_trans_settings_scsi *scsi;
				struct ccb_trans_settings_spi *spi;
				struct ccb_trans_settings_sas *sas;	
	
				scsi = &cts->proto_specific.scsi;
				scsi->flags = CTS_SCSI_FLAGS_TAG_ENB;
				scsi->valid = CTS_SCSI_VALID_TQ;
				cts->protocol = PROTO_SCSI;

				if((acb->vendor_device_id == PCIDevVenIDARC1880) ||
				   (acb->vendor_device_id == PCIDevVenIDARC1680))
				{
					cts->protocol_version = SCSI_REV_SPC2;
					cts->transport_version = 0;
					cts->transport = XPORT_SAS;
					sas = &cts->xport_specific.sas;
					sas->valid = CTS_SAS_VALID_SPEED;
					if(acb->vendor_device_id == PCIDevVenIDARC1880)
						sas->bitrate = 600000;
					else if(acb->vendor_device_id == PCIDevVenIDARC1680)
						sas->bitrate = 300000;
				}
				else
				{
					cts->protocol_version = SCSI_REV_2;
					cts->transport_version = 2;
					cts->transport = XPORT_SPI;
					spi = &cts->xport_specific.spi;
					spi->flags = CTS_SPI_FLAGS_DISC_ENB;
					spi->sync_period=2;
					spi->sync_offset=32;
					spi->bus_width=MSG_EXT_WDTR_BUS_16_BIT;
					spi->valid = CTS_SPI_VALID_DISC
						| CTS_SPI_VALID_SYNC_RATE
						| CTS_SPI_VALID_SYNC_OFFSET
						| CTS_SPI_VALID_BUS_WIDTH;
				}
			}
		#else
			{
				cts->flags=(CCB_TRANS_DISC_ENB | CCB_TRANS_TAG_ENB);
				cts->sync_period=2;
				cts->sync_offset=32;
				cts->bus_width=MSG_EXT_WDTR_BUS_16_BIT;
				cts->valid=CCB_TRANS_SYNC_RATE_VALID | 
				CCB_TRANS_SYNC_OFFSET_VALID | 
				CCB_TRANS_BUS_WIDTH_VALID | 
				CCB_TRANS_DISC_VALID | 
				CCB_TRANS_TQ_VALID;
			}
		#endif
			pccb->ccb_h.status |= CAM_REQ_CMP;
			xpt_done(pccb);
			break;
		}
	case XPT_SET_TRAN_SETTINGS: {
			pccb->ccb_h.status |= CAM_FUNC_NOTAVAIL;
			xpt_done(pccb);
			break;
		}
	case XPT_CALC_GEOMETRY:
			if(pccb->ccb_h.target_id == 16) {
				pccb->ccb_h.status |= CAM_FUNC_NOTAVAIL;
				xpt_done(pccb);
				break;
			}
#if __FreeBSD_version >= 500000
			cam_calc_geometry(&pccb->ccg, 1);
#else
			{
			struct ccb_calc_geometry *ccg;
			u_int32_t size_mb;
			u_int32_t secs_per_cylinder;

			ccg= &pccb->ccg;
			if (ccg->block_size == 0) {
				pccb->ccb_h.status = CAM_REQ_INVALID;
				xpt_done(pccb);
				break;
			}
			if(((1024L * 1024L)/ccg->block_size) < 0) {
				pccb->ccb_h.status = CAM_REQ_INVALID;
				xpt_done(pccb);
				break;
			}
			size_mb=ccg->volume_size/((1024L * 1024L)/ccg->block_size);
			if(size_mb > 1024 ) {
				ccg->heads=255;
				ccg->secs_per_track=63;
			} else {
				ccg->heads=64;
				ccg->secs_per_track=32;
			}
			secs_per_cylinder=ccg->heads * ccg->secs_per_track;
			ccg->cylinders=ccg->volume_size / secs_per_cylinder;
			pccb->ccb_h.status |= CAM_REQ_CMP;
			}
#endif
			xpt_done(pccb);
			break;
	default:
		pccb->ccb_h.status |= CAM_REQ_INVALID;
		xpt_done(pccb);
		break;
	}
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_start_hba_bgrb(struct AdapterControlBlock *acb)
{
	acb->acb_flags |= ACB_F_MSG_START_BGRB;
	CHIP_REG_WRITE32(HBA_MessageUnit, 0, inbound_msgaddr0, ARCMSR_INBOUND_MESG0_START_BGRB);
	if(!arcmsr_hba_wait_msgint_ready(acb)) {
		printf("arcmsr%d: wait 'start adapter background rebulid' timeout \n", acb->pci_unit);
	}
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_start_hbb_bgrb(struct AdapterControlBlock *acb)
{
	acb->acb_flags |= ACB_F_MSG_START_BGRB;
	CHIP_REG_WRITE32(HBB_DOORBELL, 0, drv2iop_doorbell,  ARCMSR_MESSAGE_START_BGRB);
	if(!arcmsr_hbb_wait_msgint_ready(acb)) {
		printf( "arcmsr%d: wait 'start adapter background rebulid' timeout \n", acb->pci_unit);
	}
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_start_hbc_bgrb(struct AdapterControlBlock *acb)
{
	acb->acb_flags |= ACB_F_MSG_START_BGRB;
	CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_msgaddr0, ARCMSR_INBOUND_MESG0_START_BGRB);
	CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_doorbell, ARCMSR_HBCMU_DRV2IOP_MESSAGE_CMD_DONE);
	if(!arcmsr_hbc_wait_msgint_ready(acb)) {
		printf("arcmsr%d: wait 'start adapter background rebulid' timeout \n", acb->pci_unit);
	}
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_start_adapter_bgrb(struct AdapterControlBlock *acb)
{
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A:
		arcmsr_start_hba_bgrb(acb);
		break;
	case ACB_ADAPTER_TYPE_B:
		arcmsr_start_hbb_bgrb(acb);
		break;
	case ACB_ADAPTER_TYPE_C:
		arcmsr_start_hbc_bgrb(acb);
		break;
	}
}
/*
**********************************************************************
** 
**********************************************************************
*/
static void arcmsr_polling_hba_srbdone(struct AdapterControlBlock *acb, struct CommandControlBlock *poll_srb)
{
	struct CommandControlBlock *srb;
	u_int32_t flag_srb, outbound_intstatus, poll_srb_done=0, poll_count=0;
	u_int16_t	error;
	
polling_ccb_retry:
	poll_count++;
	outbound_intstatus=CHIP_REG_READ32(HBA_MessageUnit, 0, outbound_intstatus) & acb->outbound_int_enable;
	CHIP_REG_WRITE32(HBA_MessageUnit, 0, outbound_intstatus, outbound_intstatus);	/*clear interrupt*/
	bus_dmamap_sync(acb->srb_dmat, acb->srb_dmamap, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	while(1) {
		if((flag_srb=CHIP_REG_READ32(HBA_MessageUnit, 
			0, outbound_queueport))==0xFFFFFFFF) {
			if(poll_srb_done) {
				break;/*chip FIFO no ccb for completion already*/
			} else {
				UDELAY(25000);
				if ((poll_count > 100) && (poll_srb != NULL)) {
					break;
				}
				goto polling_ccb_retry;
			}
		}
		/* check if command done with no error*/
		srb=(struct CommandControlBlock *)
			(acb->vir2phy_offset+(flag_srb << 5));/*frame must be 32 bytes aligned*/
        error=(flag_srb & ARCMSR_SRBREPLY_FLAG_ERROR_MODE0)?TRUE:FALSE;
		poll_srb_done = (srb==poll_srb) ? 1:0;
		if((srb->acb!=acb) || (srb->srb_state!=ARCMSR_SRB_START)) {
			if(srb->srb_state==ARCMSR_SRB_ABORTED) {
				printf("arcmsr%d: scsi id=%d lun=%d srb='%p'"
					"poll command abort successfully \n"
					, acb->pci_unit
					, srb->pccb->ccb_h.target_id
					, srb->pccb->ccb_h.target_lun, srb);
				srb->pccb->ccb_h.status |= CAM_REQ_ABORTED;
				arcmsr_srb_complete(srb, 1);
				continue;
			}
			printf("arcmsr%d: polling get an illegal srb command done srb='%p'"
				"srboutstandingcount=%d \n"
				, acb->pci_unit
				, srb, acb->srboutstandingcount);
			continue;
		}
		arcmsr_report_srb_state(acb, srb, error);
	}	/*drain reply FIFO*/
}
/*
**********************************************************************
**
**********************************************************************
*/
static void arcmsr_polling_hbb_srbdone(struct AdapterControlBlock *acb, struct CommandControlBlock *poll_srb)
{
	struct HBB_MessageUnit *phbbmu=(struct HBB_MessageUnit *)acb->pmu;
	struct CommandControlBlock *srb;
	u_int32_t flag_srb, poll_srb_done=0, poll_count=0;
	int index;
	u_int16_t	error;
	
polling_ccb_retry:
	poll_count++;
	CHIP_REG_WRITE32(HBB_DOORBELL, 
	0, iop2drv_doorbell, ARCMSR_DOORBELL_INT_CLEAR_PATTERN); /* clear doorbell interrupt */
	bus_dmamap_sync(acb->srb_dmat, acb->srb_dmamap, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	while(1) {
		index=phbbmu->doneq_index;
		if((flag_srb=phbbmu->done_qbuffer[index]) == 0) {
			if(poll_srb_done) {
				break;/*chip FIFO no ccb for completion already*/
			} else {
				UDELAY(25000);
			    if ((poll_count > 100) && (poll_srb != NULL)) {
					break;
				}
				goto polling_ccb_retry;
			}
		}
		phbbmu->done_qbuffer[index]=0;
		index++;
		index %= ARCMSR_MAX_HBB_POSTQUEUE;     /*if last index number set it to 0 */
		phbbmu->doneq_index=index;
		/* check if command done with no error*/
		srb=(struct CommandControlBlock *)
			(acb->vir2phy_offset+(flag_srb << 5));/*frame must be 32 bytes aligned*/
        error=(flag_srb & ARCMSR_SRBREPLY_FLAG_ERROR_MODE0)?TRUE:FALSE;
		poll_srb_done = (srb==poll_srb) ? 1:0;
		if((srb->acb!=acb) || (srb->srb_state!=ARCMSR_SRB_START)) {
			if(srb->srb_state==ARCMSR_SRB_ABORTED) {
				printf("arcmsr%d: scsi id=%d lun=%d srb='%p'"
					"poll command abort successfully \n"
					, acb->pci_unit
					, srb->pccb->ccb_h.target_id
					, srb->pccb->ccb_h.target_lun, srb);
				srb->pccb->ccb_h.status |= CAM_REQ_ABORTED;
				arcmsr_srb_complete(srb, 1);		
				continue;
			}
			printf("arcmsr%d: polling get an illegal srb command done srb='%p'"
				"srboutstandingcount=%d \n"
				, acb->pci_unit
				, srb, acb->srboutstandingcount);
			continue;
		}
		arcmsr_report_srb_state(acb, srb, error);
	}	/*drain reply FIFO*/
}
/*
**********************************************************************
** 
**********************************************************************
*/
static void arcmsr_polling_hbc_srbdone(struct AdapterControlBlock *acb, struct CommandControlBlock *poll_srb)
{
	struct CommandControlBlock *srb;
	u_int32_t flag_srb, poll_srb_done=0, poll_count=0;
	u_int16_t	error;
	
polling_ccb_retry:
	poll_count++;
	bus_dmamap_sync(acb->srb_dmat, acb->srb_dmamap, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	while(1) {
		if(!(CHIP_REG_READ32(HBC_MessageUnit, 0, host_int_status) & ARCMSR_HBCMU_OUTBOUND_POSTQUEUE_ISR)) {
			if(poll_srb_done) {
				break;/*chip FIFO no ccb for completion already*/
			} else {
				UDELAY(25000);
			    if ((poll_count > 100) && (poll_srb != NULL)) {
					break;
				}
			    if (acb->srboutstandingcount == 0) {
				    break;
			    }
				goto polling_ccb_retry;
			}
		}
		flag_srb = CHIP_REG_READ32(HBC_MessageUnit, 0, outbound_queueport_low);
		/* check if command done with no error*/
		srb=(struct CommandControlBlock *)(acb->vir2phy_offset+(flag_srb & 0xFFFFFFE0));/*frame must be 32 bytes aligned*/
        error=(flag_srb & ARCMSR_SRBREPLY_FLAG_ERROR_MODE1)?TRUE:FALSE;
		if (poll_srb != NULL)
			poll_srb_done = (srb==poll_srb) ? 1:0;
		if((srb->acb!=acb) || (srb->srb_state!=ARCMSR_SRB_START)) {
			if(srb->srb_state==ARCMSR_SRB_ABORTED) {
				printf("arcmsr%d: scsi id=%d lun=%d srb='%p'poll command abort successfully \n"
						, acb->pci_unit, srb->pccb->ccb_h.target_id, srb->pccb->ccb_h.target_lun, srb);
				srb->pccb->ccb_h.status |= CAM_REQ_ABORTED;
				arcmsr_srb_complete(srb, 1);
				continue;
			}
			printf("arcmsr%d: polling get an illegal srb command done srb='%p'srboutstandingcount=%d \n"
					, acb->pci_unit, srb, acb->srboutstandingcount);
			continue;
		}
		arcmsr_report_srb_state(acb, srb, error);
	}	/*drain reply FIFO*/
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_polling_srbdone(struct AdapterControlBlock *acb, struct CommandControlBlock *poll_srb)
{
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			arcmsr_polling_hba_srbdone(acb, poll_srb);
		}
		break;
	case ACB_ADAPTER_TYPE_B: {
			arcmsr_polling_hbb_srbdone(acb, poll_srb);
		}
		break;
	case ACB_ADAPTER_TYPE_C: {
			arcmsr_polling_hbc_srbdone(acb, poll_srb);
		}
		break;
	}
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_get_hba_config(struct AdapterControlBlock *acb)
{
	char *acb_firm_model=acb->firm_model;
	char *acb_firm_version=acb->firm_version;
	char *acb_device_map = acb->device_map;
	size_t iop_firm_model=offsetof(struct HBA_MessageUnit,msgcode_rwbuffer[ARCMSR_FW_MODEL_OFFSET]);	/*firm_model,15,60-67*/
	size_t iop_firm_version=offsetof(struct HBA_MessageUnit,msgcode_rwbuffer[ARCMSR_FW_VERS_OFFSET]);	/*firm_version,17,68-83*/
	size_t iop_device_map = offsetof(struct HBA_MessageUnit,msgcode_rwbuffer[ARCMSR_FW_DEVMAP_OFFSET]);
	int i;
	
	CHIP_REG_WRITE32(HBA_MessageUnit, 0, inbound_msgaddr0, ARCMSR_INBOUND_MESG0_GET_CONFIG);
	if(!arcmsr_hba_wait_msgint_ready(acb)) {
		printf("arcmsr%d: wait 'get adapter firmware miscellaneous data' timeout \n", acb->pci_unit);
	}
	i=0;
	while(i<8) {
		*acb_firm_model=bus_space_read_1(acb->btag[0], acb->bhandle[0], iop_firm_model+i); 
		/* 8 bytes firm_model, 15, 60-67*/
		acb_firm_model++;
		i++;
	}
	i=0;
	while(i<16) {
		*acb_firm_version=bus_space_read_1(acb->btag[0], acb->bhandle[0], iop_firm_version+i);  
		/* 16 bytes firm_version, 17, 68-83*/
		acb_firm_version++;
		i++;
	}
	i=0;
	while(i<16) {
		*acb_device_map=bus_space_read_1(acb->btag[0], acb->bhandle[0], iop_device_map+i);  
		acb_device_map++;
		i++;
	}
	printf("ARECA RAID ADAPTER%d: %s \n", acb->pci_unit, ARCMSR_DRIVER_VERSION);
	printf("ARECA RAID ADAPTER%d: FIRMWARE VERSION %s \n", acb->pci_unit, acb->firm_version);
	acb->firm_request_len=CHIP_REG_READ32(HBA_MessageUnit, 0, msgcode_rwbuffer[1]);   /*firm_request_len, 1, 04-07*/
	acb->firm_numbers_queue=CHIP_REG_READ32(HBA_MessageUnit, 0, msgcode_rwbuffer[2]); /*firm_numbers_queue, 2, 08-11*/
	acb->firm_sdram_size=CHIP_REG_READ32(HBA_MessageUnit, 0, msgcode_rwbuffer[3]);    /*firm_sdram_size, 3, 12-15*/
	acb->firm_ide_channels=CHIP_REG_READ32(HBA_MessageUnit, 0, msgcode_rwbuffer[4]);  /*firm_ide_channels, 4, 16-19*/
	acb->firm_cfg_version=CHIP_REG_READ32(HBA_MessageUnit, 0, msgcode_rwbuffer[ARCMSR_FW_CFGVER_OFFSET]);	/*firm_cfg_version,  25, 	  */
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_get_hbb_config(struct AdapterControlBlock *acb)
{
	char *acb_firm_model=acb->firm_model;
	char *acb_firm_version=acb->firm_version;
	char *acb_device_map = acb->device_map;
	size_t iop_firm_model=offsetof(struct HBB_RWBUFFER, msgcode_rwbuffer[ARCMSR_FW_MODEL_OFFSET]);	/*firm_model,15,60-67*/
	size_t iop_firm_version=offsetof(struct HBB_RWBUFFER, msgcode_rwbuffer[ARCMSR_FW_VERS_OFFSET]);	/*firm_version,17,68-83*/
	size_t iop_device_map = offsetof(struct HBB_RWBUFFER, msgcode_rwbuffer[ARCMSR_FW_DEVMAP_OFFSET]);
	int i;
	
	CHIP_REG_WRITE32(HBB_DOORBELL, 0, drv2iop_doorbell, ARCMSR_MESSAGE_GET_CONFIG);
	if(!arcmsr_hbb_wait_msgint_ready(acb)) {
		printf( "arcmsr%d: wait" "'get adapter firmware miscellaneous data' timeout \n", acb->pci_unit);
	}
	i=0;
	while(i<8) {
		*acb_firm_model=bus_space_read_1(acb->btag[1], acb->bhandle[1], iop_firm_model+i);
		/* 8 bytes firm_model, 15, 60-67*/
		acb_firm_model++;
		i++;
	}
	i=0;
	while(i<16) {
		*acb_firm_version=bus_space_read_1(acb->btag[1], acb->bhandle[1], iop_firm_version+i);
		/* 16 bytes firm_version, 17, 68-83*/
		acb_firm_version++;
		i++;
	}
	i=0;
	while(i<16) {
		*acb_device_map=bus_space_read_1(acb->btag[1], acb->bhandle[1], iop_device_map+i);  
		acb_device_map++;
		i++;
	}
	printf("ARECA RAID ADAPTER%d: %s \n", acb->pci_unit, ARCMSR_DRIVER_VERSION);
	printf("ARECA RAID ADAPTER%d: FIRMWARE VERSION %s \n", acb->pci_unit, acb->firm_version);
	acb->firm_request_len=CHIP_REG_READ32(HBB_RWBUFFER, 1, msgcode_rwbuffer[1]);   /*firm_request_len, 1, 04-07*/
	acb->firm_numbers_queue=CHIP_REG_READ32(HBB_RWBUFFER, 1, msgcode_rwbuffer[2]); /*firm_numbers_queue, 2, 08-11*/
	acb->firm_sdram_size=CHIP_REG_READ32(HBB_RWBUFFER, 1, msgcode_rwbuffer[3]);    /*firm_sdram_size, 3, 12-15*/
	acb->firm_ide_channels=CHIP_REG_READ32(HBB_RWBUFFER, 1, msgcode_rwbuffer[4]);  /*firm_ide_channels, 4, 16-19*/
	acb->firm_cfg_version=CHIP_REG_READ32(HBB_RWBUFFER, 1, msgcode_rwbuffer[ARCMSR_FW_CFGVER_OFFSET]);	/*firm_cfg_version,  25, 	  */
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_get_hbc_config(struct AdapterControlBlock *acb)
{
	char *acb_firm_model=acb->firm_model;
	char *acb_firm_version=acb->firm_version;
	char *acb_device_map = acb->device_map;
	size_t iop_firm_model=offsetof(struct HBC_MessageUnit,msgcode_rwbuffer[ARCMSR_FW_MODEL_OFFSET]);   /*firm_model,15,60-67*/
	size_t iop_firm_version=offsetof(struct HBC_MessageUnit,msgcode_rwbuffer[ARCMSR_FW_VERS_OFFSET]); /*firm_version,17,68-83*/
	size_t iop_device_map = offsetof(struct HBC_MessageUnit,msgcode_rwbuffer[ARCMSR_FW_DEVMAP_OFFSET]);
	int i;
	
	CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_msgaddr0, ARCMSR_INBOUND_MESG0_GET_CONFIG);
	CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_doorbell, ARCMSR_HBCMU_DRV2IOP_MESSAGE_CMD_DONE);
	if(!arcmsr_hbc_wait_msgint_ready(acb)) {
		printf("arcmsr%d: wait 'get adapter firmware miscellaneous data' timeout \n", acb->pci_unit);
	}
	i=0;
	while(i<8) {
		*acb_firm_model=bus_space_read_1(acb->btag[0], acb->bhandle[0], iop_firm_model+i); 
		/* 8 bytes firm_model, 15, 60-67*/
		acb_firm_model++;
		i++;
	}
	i=0;
	while(i<16) {
		*acb_firm_version=bus_space_read_1(acb->btag[0], acb->bhandle[0], iop_firm_version+i);  
		/* 16 bytes firm_version, 17, 68-83*/
		acb_firm_version++;
		i++;
	}
	i=0;
	while(i<16) {
		*acb_device_map=bus_space_read_1(acb->btag[0], acb->bhandle[0], iop_device_map+i);  
		acb_device_map++;
		i++;
	}
	printf("ARECA RAID ADAPTER%d: %s \n", acb->pci_unit, ARCMSR_DRIVER_VERSION);
	printf("ARECA RAID ADAPTER%d: FIRMWARE VERSION %s \n", acb->pci_unit, acb->firm_version);
	acb->firm_request_len	=CHIP_REG_READ32(HBC_MessageUnit, 0, msgcode_rwbuffer[1]);	/*firm_request_len,   1, 04-07*/
	acb->firm_numbers_queue	=CHIP_REG_READ32(HBC_MessageUnit, 0, msgcode_rwbuffer[2]);	/*firm_numbers_queue, 2, 08-11*/
	acb->firm_sdram_size	=CHIP_REG_READ32(HBC_MessageUnit, 0, msgcode_rwbuffer[3]);	/*firm_sdram_size,    3, 12-15*/
	acb->firm_ide_channels	=CHIP_REG_READ32(HBC_MessageUnit, 0, msgcode_rwbuffer[4]);	/*firm_ide_channels,  4, 16-19*/
	acb->firm_cfg_version	=CHIP_REG_READ32(HBC_MessageUnit, 0, msgcode_rwbuffer[ARCMSR_FW_CFGVER_OFFSET]);	/*firm_cfg_version,  25, 	  */
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_get_firmware_spec(struct AdapterControlBlock *acb)
{
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			arcmsr_get_hba_config(acb);
		}
		break;
	case ACB_ADAPTER_TYPE_B: {
			arcmsr_get_hbb_config(acb);
		}
		break;
	case ACB_ADAPTER_TYPE_C: {
			arcmsr_get_hbc_config(acb);
		}
		break;
	}
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_wait_firmware_ready( struct AdapterControlBlock *acb)
{
	int	timeout=0;
	
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			while ((CHIP_REG_READ32(HBA_MessageUnit, 0, outbound_msgaddr1) & ARCMSR_OUTBOUND_MESG1_FIRMWARE_OK) == 0)
			{
				if (timeout++ > 2000) /* (2000*15)/1000 = 30 sec */
				{
					printf( "arcmsr%d:timed out waiting for firmware \n", acb->pci_unit);
					return;
				}
				UDELAY(15000); /* wait 15 milli-seconds */
			}
		}
		break;
	case ACB_ADAPTER_TYPE_B: {
			while ((CHIP_REG_READ32(HBB_DOORBELL, 0, iop2drv_doorbell) & ARCMSR_MESSAGE_FIRMWARE_OK) == 0)
			{
				if (timeout++ > 2000) /* (2000*15)/1000 = 30 sec */
				{
					printf( "arcmsr%d: timed out waiting for firmware \n", acb->pci_unit);
					return;
				}
				UDELAY(15000); /* wait 15 milli-seconds */
			}
			CHIP_REG_WRITE32(HBB_DOORBELL, 0, drv2iop_doorbell, ARCMSR_DRV2IOP_END_OF_INTERRUPT);
		}
		break;
	case ACB_ADAPTER_TYPE_C: {
			while ((CHIP_REG_READ32(HBC_MessageUnit, 0, outbound_msgaddr1) & ARCMSR_HBCMU_MESSAGE_FIRMWARE_OK) == 0)
			{
				if (timeout++ > 2000) /* (2000*15)/1000 = 30 sec */
				{
					printf( "arcmsr%d:timed out waiting for firmware ready\n", acb->pci_unit);
					return;
				}
				UDELAY(15000); /* wait 15 milli-seconds */
			}
		}
		break;
	}
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_clear_doorbell_queue_buffer( struct AdapterControlBlock *acb)
{
	u_int32_t outbound_doorbell;

	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			/* empty doorbell Qbuffer if door bell ringed */
			outbound_doorbell = CHIP_REG_READ32(HBA_MessageUnit, 0, outbound_doorbell);
			CHIP_REG_WRITE32(HBA_MessageUnit, 0, outbound_doorbell, outbound_doorbell);	/*clear doorbell interrupt */
			CHIP_REG_WRITE32(HBA_MessageUnit, 0, inbound_doorbell, ARCMSR_INBOUND_DRIVER_DATA_READ_OK);
			
		}
		break;
	case ACB_ADAPTER_TYPE_B: {
			CHIP_REG_WRITE32(HBB_DOORBELL, 0, iop2drv_doorbell, ARCMSR_MESSAGE_INT_CLEAR_PATTERN);/*clear interrupt and message state*/
			CHIP_REG_WRITE32(HBB_DOORBELL, 0, drv2iop_doorbell, ARCMSR_DRV2IOP_DATA_READ_OK);
			/* let IOP know data has been read */
		}
		break;
	case ACB_ADAPTER_TYPE_C: {
			/* empty doorbell Qbuffer if door bell ringed */
			outbound_doorbell = CHIP_REG_READ32(HBC_MessageUnit, 0, outbound_doorbell);
			CHIP_REG_WRITE32(HBC_MessageUnit, 0, outbound_doorbell_clear, outbound_doorbell);	/*clear doorbell interrupt */
			CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_doorbell, ARCMSR_HBCMU_DRV2IOP_DATA_READ_OK);
			
		}
		break;
	}
}
/*
************************************************************************
************************************************************************
*/
static u_int32_t arcmsr_iop_confirm(struct AdapterControlBlock *acb)
{
	unsigned long srb_phyaddr;
	u_int32_t srb_phyaddr_hi32;
	
	/*
	********************************************************************
	** here we need to tell iop 331 our freesrb.HighPart 
	** if freesrb.HighPart is not zero
	********************************************************************
	*/
	srb_phyaddr= (unsigned long) acb->srb_phyaddr.phyaddr;
//	srb_phyaddr_hi32=(u_int32_t) ((srb_phyaddr>>16)>>16);
	srb_phyaddr_hi32=acb->srb_phyaddr.B.phyadd_high;
	switch (acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			if(srb_phyaddr_hi32!=0) {
				CHIP_REG_WRITE32(HBA_MessageUnit, 0, msgcode_rwbuffer[0], ARCMSR_SIGNATURE_SET_CONFIG);
				CHIP_REG_WRITE32(HBA_MessageUnit, 0, msgcode_rwbuffer[1], srb_phyaddr_hi32);
				CHIP_REG_WRITE32(HBA_MessageUnit, 0, inbound_msgaddr0, ARCMSR_INBOUND_MESG0_SET_CONFIG);
				if(!arcmsr_hba_wait_msgint_ready(acb)) {
					printf( "arcmsr%d: 'set srb high part physical address' timeout \n", acb->pci_unit);
					return FALSE;
				}
			}
		}
		break;
		/*
		***********************************************************************
		**    if adapter type B, set window of "post command Q" 
		***********************************************************************
		*/
	case ACB_ADAPTER_TYPE_B: {
			u_int32_t post_queue_phyaddr;
			struct HBB_MessageUnit *phbbmu;
	
			phbbmu=(struct HBB_MessageUnit *)acb->pmu;
			phbbmu->postq_index=0;
			phbbmu->doneq_index=0;
			CHIP_REG_WRITE32(HBB_DOORBELL, 0, drv2iop_doorbell, ARCMSR_MESSAGE_SET_POST_WINDOW);
			if(!arcmsr_hbb_wait_msgint_ready(acb)) {
				printf( "arcmsr%d: 'set window of post command Q' timeout\n", acb->pci_unit);
				return FALSE;
			}
			post_queue_phyaddr = srb_phyaddr + ARCMSR_SRBS_POOL_SIZE 
								+ offsetof(struct HBB_MessageUnit, post_qbuffer);
			CHIP_REG_WRITE32(HBB_RWBUFFER, 1, msgcode_rwbuffer[0], ARCMSR_SIGNATURE_SET_CONFIG); /* driver "set config" signature */
			CHIP_REG_WRITE32(HBB_RWBUFFER, 1, msgcode_rwbuffer[1], srb_phyaddr_hi32); /* normal should be zero */
			CHIP_REG_WRITE32(HBB_RWBUFFER, 1, msgcode_rwbuffer[2], post_queue_phyaddr); /* postQ size (256+8)*4 */
			CHIP_REG_WRITE32(HBB_RWBUFFER, 1, msgcode_rwbuffer[3], post_queue_phyaddr+1056); /* doneQ size (256+8)*4 */
			CHIP_REG_WRITE32(HBB_RWBUFFER, 1, msgcode_rwbuffer[4], 1056); /* srb maxQ size must be --> [(256+8)*4] */
			CHIP_REG_WRITE32(HBB_DOORBELL, 0, drv2iop_doorbell, ARCMSR_MESSAGE_SET_CONFIG);
			if(!arcmsr_hbb_wait_msgint_ready(acb)) {
				printf( "arcmsr%d: 'set command Q window' timeout \n", acb->pci_unit);
				return FALSE;
			}
			CHIP_REG_WRITE32(HBB_DOORBELL, 0, drv2iop_doorbell, ARCMSR_MESSAGE_START_DRIVER_MODE);
			if(!arcmsr_hbb_wait_msgint_ready(acb)) {
				printf( "arcmsr%d: 'start diver mode' timeout \n", acb->pci_unit);
				return FALSE;
			}
		}
		break;
	case ACB_ADAPTER_TYPE_C: {
			if(srb_phyaddr_hi32!=0) {
				CHIP_REG_WRITE32(HBC_MessageUnit, 0, msgcode_rwbuffer[0], ARCMSR_SIGNATURE_SET_CONFIG);
				CHIP_REG_WRITE32(HBC_MessageUnit, 0, msgcode_rwbuffer[1], srb_phyaddr_hi32);
				CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_msgaddr0, ARCMSR_INBOUND_MESG0_SET_CONFIG);
				CHIP_REG_WRITE32(HBC_MessageUnit, 0, inbound_doorbell,ARCMSR_HBCMU_DRV2IOP_MESSAGE_CMD_DONE);
				if(!arcmsr_hbc_wait_msgint_ready(acb)) {
					printf( "arcmsr%d: 'set srb high part physical address' timeout \n", acb->pci_unit);
					return FALSE;
				}
			}
		}
		break;
	}
	return (TRUE);
}
/*
************************************************************************
************************************************************************
*/
static void arcmsr_enable_eoi_mode(struct AdapterControlBlock *acb)
{
	switch (acb->adapter_type)
	{
	case ACB_ADAPTER_TYPE_A:
	case ACB_ADAPTER_TYPE_C:
		break;
	case ACB_ADAPTER_TYPE_B: {
			CHIP_REG_WRITE32(HBB_DOORBELL, 0, drv2iop_doorbell,ARCMSR_MESSAGE_ACTIVE_EOI_MODE);
			if(!arcmsr_hbb_wait_msgint_ready(acb)) {
				printf( "arcmsr%d: 'iop enable eoi mode' timeout \n", acb->pci_unit);
				
				return;
			}
		}
		break;
	}
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_iop_init(struct AdapterControlBlock *acb)
{
	u_int32_t intmask_org;
	
	/* disable all outbound interrupt */
	intmask_org=arcmsr_disable_allintr(acb);
	arcmsr_wait_firmware_ready(acb);
	arcmsr_iop_confirm(acb);
	arcmsr_get_firmware_spec(acb);
	/*start background rebuild*/
	arcmsr_start_adapter_bgrb(acb);
	/* empty doorbell Qbuffer if door bell ringed */
	arcmsr_clear_doorbell_queue_buffer(acb);
	arcmsr_enable_eoi_mode(acb);
	/* enable outbound Post Queue, outbound doorbell Interrupt */
	arcmsr_enable_allintr(acb, intmask_org);
	acb->acb_flags |=ACB_F_IOP_INITED;
}
/*
**********************************************************************
**********************************************************************
*/
static void arcmsr_map_free_srb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct AdapterControlBlock *acb=arg;
	struct CommandControlBlock *srb_tmp;
	u_int8_t * dma_memptr;
	u_int32_t i;
	unsigned long srb_phyaddr=(unsigned long)segs->ds_addr;
	
	dma_memptr=acb->uncacheptr;
	acb->srb_phyaddr.phyaddr=srb_phyaddr; 
	srb_tmp=(struct CommandControlBlock *)dma_memptr;
	for(i=0;i<ARCMSR_MAX_FREESRB_NUM;i++) {
		if(bus_dmamap_create(acb->dm_segs_dmat,
			 /*flags*/0, &srb_tmp->dm_segs_dmamap)!=0) {
			acb->acb_flags |= ACB_F_MAPFREESRB_FAILD;
			printf("arcmsr%d:"
			" srb dmamap bus_dmamap_create error\n", acb->pci_unit);
			return;
		}
		srb_tmp->cdb_shifted_phyaddr=(acb->adapter_type==ACB_ADAPTER_TYPE_C)?srb_phyaddr:(srb_phyaddr >> 5);
		srb_tmp->acb=acb;
		acb->srbworkingQ[i]=acb->psrb_pool[i]=srb_tmp;
		srb_phyaddr=srb_phyaddr+SRB_SIZE;
		srb_tmp = (struct CommandControlBlock *)((unsigned long)srb_tmp+SRB_SIZE);
	}
	acb->vir2phy_offset=(unsigned long)srb_tmp-(unsigned long)srb_phyaddr;
}
/*
************************************************************************
**
**
************************************************************************
*/
static void arcmsr_free_resource(struct AdapterControlBlock *acb)
{
	/* remove the control device */
	if(acb->ioctl_dev != NULL) {
		destroy_dev(acb->ioctl_dev);
	}
	bus_dmamap_unload(acb->srb_dmat, acb->srb_dmamap);
	bus_dmamap_destroy(acb->srb_dmat, acb->srb_dmamap);
	bus_dma_tag_destroy(acb->srb_dmat);
	bus_dma_tag_destroy(acb->dm_segs_dmat);
	bus_dma_tag_destroy(acb->parent_dmat);
}
/*
************************************************************************
************************************************************************
*/
static u_int32_t arcmsr_initialize(device_t dev)
{
	struct AdapterControlBlock *acb=device_get_softc(dev);
	u_int16_t pci_command;
	int i, j,max_coherent_size;
	u_int32_t vendor_dev_id;

	vendor_dev_id = pci_get_devid(dev);
	acb->vendor_device_id = vendor_dev_id;
	switch (vendor_dev_id) {
	case PCIDevVenIDARC1880:
	case PCIDevVenIDARC1882:
	case PCIDevVenIDARC1213:
	case PCIDevVenIDARC1223: {
			acb->adapter_type=ACB_ADAPTER_TYPE_C;
			acb->adapter_bus_speed = ACB_BUS_SPEED_6G;
			max_coherent_size=ARCMSR_SRBS_POOL_SIZE;
		}
		break;
	case PCIDevVenIDARC1200:
	case PCIDevVenIDARC1201: {
			acb->adapter_type=ACB_ADAPTER_TYPE_B;
			acb->adapter_bus_speed = ACB_BUS_SPEED_3G;
			max_coherent_size=ARCMSR_SRBS_POOL_SIZE+(sizeof(struct HBB_MessageUnit));
		}
		break;
	case PCIDevVenIDARC1110:
	case PCIDevVenIDARC1120:
	case PCIDevVenIDARC1130:
	case PCIDevVenIDARC1160:
	case PCIDevVenIDARC1170:
	case PCIDevVenIDARC1210:
	case PCIDevVenIDARC1220:
	case PCIDevVenIDARC1230:
	case PCIDevVenIDARC1231:
	case PCIDevVenIDARC1260:
	case PCIDevVenIDARC1261:
	case PCIDevVenIDARC1270:
	case PCIDevVenIDARC1280:
	case PCIDevVenIDARC1212:
	case PCIDevVenIDARC1222:
	case PCIDevVenIDARC1380:
	case PCIDevVenIDARC1381:
	case PCIDevVenIDARC1680:
	case PCIDevVenIDARC1681: {
			acb->adapter_type=ACB_ADAPTER_TYPE_A;
			acb->adapter_bus_speed = ACB_BUS_SPEED_3G;
			max_coherent_size=ARCMSR_SRBS_POOL_SIZE;
		}
		break;
	default: {
			printf("arcmsr%d:"
			" unknown RAID adapter type \n", device_get_unit(dev));
			return ENOMEM;
		}
	}
	if(bus_dma_tag_create(  /*PCI parent*/		bus_get_dma_tag(dev),
							/*alignemnt*/	1,
							/*boundary*/	0,
							/*lowaddr*/		BUS_SPACE_MAXADDR,
							/*highaddr*/	BUS_SPACE_MAXADDR,
							/*filter*/		NULL,
							/*filterarg*/	NULL,
							/*maxsize*/		BUS_SPACE_MAXSIZE_32BIT,
							/*nsegments*/	BUS_SPACE_UNRESTRICTED,
							/*maxsegsz*/	BUS_SPACE_MAXSIZE_32BIT,
							/*flags*/		0,
#if __FreeBSD_version >= 501102
							/*lockfunc*/	NULL,
							/*lockarg*/		NULL,
#endif
						&acb->parent_dmat) != 0)
	{
		printf("arcmsr%d: parent_dmat bus_dma_tag_create failure!\n", device_get_unit(dev));
		return ENOMEM;
	}

	/* Create a single tag describing a region large enough to hold all of the s/g lists we will need. */
	if(bus_dma_tag_create(  /*parent_dmat*/	acb->parent_dmat,
							/*alignment*/	1,
							/*boundary*/	0,
#ifdef PAE
							/*lowaddr*/		BUS_SPACE_MAXADDR_32BIT,
#else
							/*lowaddr*/		BUS_SPACE_MAXADDR,
#endif
							/*highaddr*/	BUS_SPACE_MAXADDR,
							/*filter*/		NULL,
							/*filterarg*/	NULL,
							/*maxsize*/		ARCMSR_MAX_SG_ENTRIES * PAGE_SIZE * ARCMSR_MAX_FREESRB_NUM,
							/*nsegments*/	ARCMSR_MAX_SG_ENTRIES,
							/*maxsegsz*/	BUS_SPACE_MAXSIZE_32BIT,
							/*flags*/		0,
#if __FreeBSD_version >= 501102
							/*lockfunc*/	busdma_lock_mutex,
							/*lockarg*/		&acb->qbuffer_lock,
#endif
						&acb->dm_segs_dmat) != 0)
	{
		bus_dma_tag_destroy(acb->parent_dmat);
		printf("arcmsr%d: dm_segs_dmat bus_dma_tag_create failure!\n", device_get_unit(dev));
		return ENOMEM;
	}

	/* DMA tag for our srb structures.... Allocate the freesrb memory */
	if(bus_dma_tag_create(  /*parent_dmat*/	acb->parent_dmat,
							/*alignment*/	0x20,
							/*boundary*/	0,
							/*lowaddr*/		BUS_SPACE_MAXADDR_32BIT,
							/*highaddr*/	BUS_SPACE_MAXADDR,
							/*filter*/		NULL,
							/*filterarg*/	NULL,
							/*maxsize*/		max_coherent_size,
							/*nsegments*/	1,
							/*maxsegsz*/	BUS_SPACE_MAXSIZE_32BIT,
							/*flags*/		0,
#if __FreeBSD_version >= 501102
							/*lockfunc*/	NULL,
							/*lockarg*/		NULL,
#endif
						&acb->srb_dmat) != 0)
	{
		bus_dma_tag_destroy(acb->dm_segs_dmat);
		bus_dma_tag_destroy(acb->parent_dmat);
		printf("arcmsr%d: srb_dmat bus_dma_tag_create failure!\n", device_get_unit(dev));
		return ENXIO;
	}
	/* Allocation for our srbs */
	if(bus_dmamem_alloc(acb->srb_dmat, (void **)&acb->uncacheptr, BUS_DMA_WAITOK | BUS_DMA_COHERENT | BUS_DMA_ZERO, &acb->srb_dmamap) != 0) {
		bus_dma_tag_destroy(acb->srb_dmat);
		bus_dma_tag_destroy(acb->dm_segs_dmat);
		bus_dma_tag_destroy(acb->parent_dmat);
		printf("arcmsr%d: srb_dmat bus_dmamem_alloc failure!\n", device_get_unit(dev));
		return ENXIO;
	}
	/* And permanently map them */
	if(bus_dmamap_load(acb->srb_dmat, acb->srb_dmamap, acb->uncacheptr, max_coherent_size, arcmsr_map_free_srb, acb, /*flags*/0)) {
		bus_dma_tag_destroy(acb->srb_dmat);
		bus_dma_tag_destroy(acb->dm_segs_dmat);
		bus_dma_tag_destroy(acb->parent_dmat);
		printf("arcmsr%d: srb_dmat bus_dmamap_load failure!\n", device_get_unit(dev));
		return ENXIO;
	}
	pci_command=pci_read_config(dev, PCIR_COMMAND, 2);
	pci_command |= PCIM_CMD_BUSMASTEREN;
	pci_command |= PCIM_CMD_PERRESPEN;
	pci_command |= PCIM_CMD_MWRICEN;
	/* Enable Busmaster/Mem */
	pci_command |= PCIM_CMD_MEMEN;
	pci_write_config(dev, PCIR_COMMAND, pci_command, 2);
	switch(acb->adapter_type) {
	case ACB_ADAPTER_TYPE_A: {
			u_int32_t rid0=PCIR_BAR(0);
			vm_offset_t	mem_base0;

			acb->sys_res_arcmsr[0]=bus_alloc_resource(dev,SYS_RES_MEMORY, &rid0, 0ul, ~0ul, 0x1000, RF_ACTIVE);
			if(acb->sys_res_arcmsr[0] == NULL) {
				arcmsr_free_resource(acb);
				printf("arcmsr%d: bus_alloc_resource failure!\n", device_get_unit(dev));
				return ENOMEM;
			}
			if(rman_get_start(acb->sys_res_arcmsr[0]) <= 0) {
				arcmsr_free_resource(acb);
				printf("arcmsr%d: rman_get_start failure!\n", device_get_unit(dev));
				return ENXIO;
			}
			mem_base0=(vm_offset_t) rman_get_virtual(acb->sys_res_arcmsr[0]);
			if(mem_base0==0) {
				arcmsr_free_resource(acb);
				printf("arcmsr%d: rman_get_virtual failure!\n", device_get_unit(dev));
				return ENXIO;
			}
			acb->btag[0]=rman_get_bustag(acb->sys_res_arcmsr[0]);
			acb->bhandle[0]=rman_get_bushandle(acb->sys_res_arcmsr[0]);
			acb->pmu=(struct MessageUnit_UNION *)mem_base0;
		}
		break;
	case ACB_ADAPTER_TYPE_B: {
			struct HBB_MessageUnit *phbbmu;
			struct CommandControlBlock *freesrb;
			u_int32_t rid[]={ PCIR_BAR(0), PCIR_BAR(2) };
			vm_offset_t	mem_base[]={0,0};
			for(i=0; i<2; i++) {
				if(i==0) {
					acb->sys_res_arcmsr[i]=bus_alloc_resource(dev,SYS_RES_MEMORY, &rid[i],
											0ul, ~0ul, sizeof(struct HBB_DOORBELL), RF_ACTIVE);
				} else {
					acb->sys_res_arcmsr[i]=bus_alloc_resource(dev, SYS_RES_MEMORY, &rid[i],
											0ul, ~0ul, sizeof(struct HBB_RWBUFFER), RF_ACTIVE);
				}
				if(acb->sys_res_arcmsr[i] == NULL) {
					arcmsr_free_resource(acb);
					printf("arcmsr%d: bus_alloc_resource %d failure!\n", device_get_unit(dev), i);
					return ENOMEM;
				}
				if(rman_get_start(acb->sys_res_arcmsr[i]) <= 0) {
					arcmsr_free_resource(acb);
					printf("arcmsr%d: rman_get_start %d failure!\n", device_get_unit(dev), i);
					return ENXIO;
				}
				mem_base[i]=(vm_offset_t) rman_get_virtual(acb->sys_res_arcmsr[i]);
				if(mem_base[i]==0) {
					arcmsr_free_resource(acb);
					printf("arcmsr%d: rman_get_virtual %d failure!\n", device_get_unit(dev), i);
					return ENXIO;
				}
				acb->btag[i]=rman_get_bustag(acb->sys_res_arcmsr[i]);
				acb->bhandle[i]=rman_get_bushandle(acb->sys_res_arcmsr[i]);
			}
			freesrb=(struct CommandControlBlock *)acb->uncacheptr;
//			acb->pmu=(struct MessageUnit_UNION *)&freesrb[ARCMSR_MAX_FREESRB_NUM];
			acb->pmu=(struct MessageUnit_UNION *)((unsigned long)freesrb+ARCMSR_SRBS_POOL_SIZE);
			phbbmu=(struct HBB_MessageUnit *)acb->pmu;
			phbbmu->hbb_doorbell=(struct HBB_DOORBELL *)mem_base[0];
			phbbmu->hbb_rwbuffer=(struct HBB_RWBUFFER *)mem_base[1];
		}
		break;
	case ACB_ADAPTER_TYPE_C: {
			u_int32_t rid0=PCIR_BAR(1);
			vm_offset_t	mem_base0;

			acb->sys_res_arcmsr[0]=bus_alloc_resource(dev,SYS_RES_MEMORY, &rid0, 0ul, ~0ul, sizeof(struct HBC_MessageUnit), RF_ACTIVE);
			if(acb->sys_res_arcmsr[0] == NULL) {
				arcmsr_free_resource(acb);
				printf("arcmsr%d: bus_alloc_resource failure!\n", device_get_unit(dev));
				return ENOMEM;
			}
			if(rman_get_start(acb->sys_res_arcmsr[0]) <= 0) {
				arcmsr_free_resource(acb);
				printf("arcmsr%d: rman_get_start failure!\n", device_get_unit(dev));
				return ENXIO;
			}
			mem_base0=(vm_offset_t) rman_get_virtual(acb->sys_res_arcmsr[0]);
			if(mem_base0==0) {
				arcmsr_free_resource(acb);
				printf("arcmsr%d: rman_get_virtual failure!\n", device_get_unit(dev));
				return ENXIO;
			}
			acb->btag[0]=rman_get_bustag(acb->sys_res_arcmsr[0]);
			acb->bhandle[0]=rman_get_bushandle(acb->sys_res_arcmsr[0]);
			acb->pmu=(struct MessageUnit_UNION *)mem_base0;
		}
		break;
	}
	if(acb->acb_flags & ACB_F_MAPFREESRB_FAILD) {
		arcmsr_free_resource(acb);
		printf("arcmsr%d: map free srb failure!\n", device_get_unit(dev));
		return ENXIO;
	}
	acb->acb_flags  |= (ACB_F_MESSAGE_WQBUFFER_CLEARED|ACB_F_MESSAGE_RQBUFFER_CLEARED|ACB_F_MESSAGE_WQBUFFER_READ);
	acb->acb_flags &= ~ACB_F_SCSISTOPADAPTER;
	/*
	********************************************************************
	** init raid volume state
	********************************************************************
	*/
	for(i=0;i<ARCMSR_MAX_TARGETID;i++) {
		for(j=0;j<ARCMSR_MAX_TARGETLUN;j++) {
			acb->devstate[i][j]=ARECA_RAID_GONE;
		}
	}
	arcmsr_iop_init(acb);
	return(0);
}
/*
************************************************************************
************************************************************************
*/
static int arcmsr_attach(device_t dev)
{
	struct AdapterControlBlock *acb=(struct AdapterControlBlock *)device_get_softc(dev);
	u_int32_t unit=device_get_unit(dev);
	struct ccb_setasync csa;
	struct cam_devq	*devq;	/* Device Queue to use for this SIM */
	struct resource	*irqres;
	int	rid;
	
	if(acb == NULL) {
		printf("arcmsr%d: cannot allocate softc\n", unit);
		return (ENOMEM);
	}
	ARCMSR_LOCK_INIT(&acb->qbuffer_lock, "arcmsr Q buffer lock");
	if(arcmsr_initialize(dev)) {
		printf("arcmsr%d: initialize failure!\n", unit);
		ARCMSR_LOCK_DESTROY(&acb->qbuffer_lock);
		return ENXIO;
	}
	/* After setting up the adapter, map our interrupt */
	rid=0;
	irqres=bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0ul, ~0ul, 1, RF_SHAREABLE | RF_ACTIVE);
	if(irqres == NULL || 
#if __FreeBSD_version >= 700025
		bus_setup_intr(dev, irqres, INTR_TYPE_CAM|INTR_ENTROPY|INTR_MPSAFE, NULL, arcmsr_intr_handler, acb, &acb->ih)) {
#else
		bus_setup_intr(dev, irqres, INTR_TYPE_CAM|INTR_ENTROPY|INTR_MPSAFE, arcmsr_intr_handler, acb, &acb->ih)) {
#endif
		arcmsr_free_resource(acb);
		ARCMSR_LOCK_DESTROY(&acb->qbuffer_lock);
		printf("arcmsr%d: unable to register interrupt handler!\n", unit);
		return ENXIO;
	}
	acb->irqres=irqres;
	acb->pci_dev=dev;
	acb->pci_unit=unit;
	/*
	 * Now let the CAM generic SCSI layer find the SCSI devices on
	 * the bus *  start queue to reset to the idle loop. *
	 * Create device queue of SIM(s) *  (MAX_START_JOB - 1) :
	 * max_sim_transactions
	*/
	devq=cam_simq_alloc(ARCMSR_MAX_START_JOB);
	if(devq == NULL) {
	    arcmsr_free_resource(acb);
		bus_release_resource(dev, SYS_RES_IRQ, 0, acb->irqres);
		ARCMSR_LOCK_DESTROY(&acb->qbuffer_lock);
		printf("arcmsr%d: cam_simq_alloc failure!\n", unit);
		return ENXIO;
	}
#if __FreeBSD_version >= 700025
	acb->psim=cam_sim_alloc(arcmsr_action, arcmsr_poll, "arcmsr", acb, unit, &acb->qbuffer_lock, 1, ARCMSR_MAX_OUTSTANDING_CMD, devq);
#else
	acb->psim=cam_sim_alloc(arcmsr_action, arcmsr_poll, "arcmsr", acb, unit, 1, ARCMSR_MAX_OUTSTANDING_CMD, devq);
#endif
	if(acb->psim == NULL) {
		arcmsr_free_resource(acb);
		bus_release_resource(dev, SYS_RES_IRQ, 0, acb->irqres);
		cam_simq_free(devq);
		ARCMSR_LOCK_DESTROY(&acb->qbuffer_lock);
		printf("arcmsr%d: cam_sim_alloc failure!\n", unit);
		return ENXIO;
	}
	ARCMSR_LOCK_ACQUIRE(&acb->qbuffer_lock);
#if __FreeBSD_version >= 700044
	if(xpt_bus_register(acb->psim, dev, 0) != CAM_SUCCESS) {
#else
	if(xpt_bus_register(acb->psim, 0) != CAM_SUCCESS) {
#endif
		arcmsr_free_resource(acb);
		bus_release_resource(dev, SYS_RES_IRQ, 0, acb->irqres);
		cam_sim_free(acb->psim, /*free_devq*/TRUE);
		ARCMSR_LOCK_DESTROY(&acb->qbuffer_lock);
		printf("arcmsr%d: xpt_bus_register failure!\n", unit);
		return ENXIO;
	}
	if(xpt_create_path(&acb->ppath, /* periph */ NULL, cam_sim_path(acb->psim), CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		arcmsr_free_resource(acb);
		bus_release_resource(dev, SYS_RES_IRQ, 0, acb->irqres);
		xpt_bus_deregister(cam_sim_path(acb->psim));
		cam_sim_free(acb->psim, /* free_simq */ TRUE);
		ARCMSR_LOCK_DESTROY(&acb->qbuffer_lock);
		printf("arcmsr%d: xpt_create_path failure!\n", unit);
		return ENXIO;
	}
	/*
	****************************************************
	*/
	xpt_setup_ccb(&csa.ccb_h, acb->ppath, /*priority*/5);
	csa.ccb_h.func_code=XPT_SASYNC_CB;
	csa.event_enable=AC_FOUND_DEVICE|AC_LOST_DEVICE;
	csa.callback=arcmsr_async;
	csa.callback_arg=acb->psim;
	xpt_action((union ccb *)&csa);
	ARCMSR_LOCK_RELEASE(&acb->qbuffer_lock);
	/* Create the control device.  */
	acb->ioctl_dev=make_dev(&arcmsr_cdevsw, unit, UID_ROOT, GID_WHEEL /* GID_OPERATOR */, S_IRUSR | S_IWUSR, "arcmsr%d", unit);
		
#if __FreeBSD_version < 503000
	acb->ioctl_dev->si_drv1=acb;
#endif
#if __FreeBSD_version > 500005
	(void)make_dev_alias(acb->ioctl_dev, "arc%d", unit);
#endif
	arcmsr_callout_init(&acb->devmap_callout);
	callout_reset(&acb->devmap_callout, 60 * hz, arcmsr_polling_devmap, acb);
	return (0);
}

/*
************************************************************************
************************************************************************
*/
static int arcmsr_probe(device_t dev)
{
	u_int32_t id;
	static char buf[256];
	char x_type[]={"X-TYPE"};
	char *type;
	int raid6 = 1;
	
	if (pci_get_vendor(dev) != PCI_VENDOR_ID_ARECA) {
		return (ENXIO);
	}
	switch(id=pci_get_devid(dev)) {
	case PCIDevVenIDARC1110:
	case PCIDevVenIDARC1200:
	case PCIDevVenIDARC1201:
	case PCIDevVenIDARC1210:
		raid6 = 0;
		/*FALLTHRU*/
	case PCIDevVenIDARC1120:
	case PCIDevVenIDARC1130:
	case PCIDevVenIDARC1160:
	case PCIDevVenIDARC1170:
	case PCIDevVenIDARC1220:
	case PCIDevVenIDARC1230:
	case PCIDevVenIDARC1231:
	case PCIDevVenIDARC1260:
	case PCIDevVenIDARC1261:
	case PCIDevVenIDARC1270:
	case PCIDevVenIDARC1280:
		type = "SATA";
		break;
	case PCIDevVenIDARC1212:
	case PCIDevVenIDARC1222:
	case PCIDevVenIDARC1380:
	case PCIDevVenIDARC1381:
	case PCIDevVenIDARC1680:
	case PCIDevVenIDARC1681:
		type = "SAS 3G";
		break;
	case PCIDevVenIDARC1880:
	case PCIDevVenIDARC1882:
	case PCIDevVenIDARC1213:
	case PCIDevVenIDARC1223:
		type = "SAS 6G";
		break;
	default:
		type = x_type;
		break;
	}
	if(type == x_type)
		return(ENXIO);
	sprintf(buf, "Areca %s Host Adapter RAID Controller %s\n", type, raid6 ? "(RAID6 capable)" : "");
	device_set_desc_copy(dev, buf);
	return 0;
}
/*
************************************************************************
************************************************************************
*/
static int arcmsr_shutdown(device_t dev)
{
	u_int32_t  i;
	u_int32_t intmask_org;
	struct CommandControlBlock *srb;
	struct AdapterControlBlock *acb=(struct AdapterControlBlock *)device_get_softc(dev);
	
	/* stop adapter background rebuild */
	ARCMSR_LOCK_ACQUIRE(&acb->qbuffer_lock);
	/* disable all outbound interrupt */
	intmask_org=arcmsr_disable_allintr(acb);
	arcmsr_stop_adapter_bgrb(acb);
	arcmsr_flush_adapter_cache(acb);
	/* abort all outstanding command */
	acb->acb_flags |= ACB_F_SCSISTOPADAPTER;
	acb->acb_flags &= ~ACB_F_IOP_INITED;
	if(acb->srboutstandingcount!=0) {
		/*clear and abort all outbound posted Q*/
		arcmsr_done4abort_postqueue(acb);
		/* talk to iop 331 outstanding command aborted*/
		arcmsr_abort_allcmd(acb);
		for(i=0;i<ARCMSR_MAX_FREESRB_NUM;i++) {
			srb=acb->psrb_pool[i];
			if(srb->srb_state==ARCMSR_SRB_START) {
				srb->srb_state=ARCMSR_SRB_ABORTED;
				srb->pccb->ccb_h.status |= CAM_REQ_ABORTED;
				arcmsr_srb_complete(srb, 1);
			}
		}
	}
	acb->srboutstandingcount=0;
	acb->workingsrb_doneindex=0;
	acb->workingsrb_startindex=0;
#ifdef ARCMSR_DEBUG1
	acb->pktRequestCount = 0;
	acb->pktReturnCount = 0;
#endif
	ARCMSR_LOCK_RELEASE(&acb->qbuffer_lock);
	return (0);
}
/*
************************************************************************
************************************************************************
*/
static int arcmsr_detach(device_t dev)
{
	struct AdapterControlBlock *acb=(struct AdapterControlBlock *)device_get_softc(dev);
	int i;
	
	callout_stop(&acb->devmap_callout);
	bus_teardown_intr(dev, acb->irqres, acb->ih);
	arcmsr_shutdown(dev);
	arcmsr_free_resource(acb);
	for(i=0; (acb->sys_res_arcmsr[i]!=NULL) && (i<2); i++) {
		bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BAR(i), acb->sys_res_arcmsr[i]);
	}
	bus_release_resource(dev, SYS_RES_IRQ, 0, acb->irqres);
	ARCMSR_LOCK_ACQUIRE(&acb->qbuffer_lock);
	xpt_async(AC_LOST_DEVICE, acb->ppath, NULL);
	xpt_free_path(acb->ppath);
	xpt_bus_deregister(cam_sim_path(acb->psim));
	cam_sim_free(acb->psim, TRUE);
	ARCMSR_LOCK_RELEASE(&acb->qbuffer_lock);
	ARCMSR_LOCK_DESTROY(&acb->qbuffer_lock);
	return (0);
}

#ifdef ARCMSR_DEBUG1
static void arcmsr_dump_data(struct AdapterControlBlock *acb)
{
	if((acb->pktRequestCount - acb->pktReturnCount) == 0)
		return;
	printf("Command Request Count   =0x%x\n",acb->pktRequestCount);
	printf("Command Return Count    =0x%x\n",acb->pktReturnCount);
	printf("Command (Req-Rtn) Count =0x%x\n",(acb->pktRequestCount - acb->pktReturnCount));
	printf("Queued Command Count    =0x%x\n",acb->srboutstandingcount);
}
#endif

