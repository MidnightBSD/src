/*-
 * Copyright (c) 2009 Yahoo! Inc.
 * Copyright (c) 2012 LSI Corp.
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
 * LSI MPT-Fusion Host Adapter FreeBSD
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* Communications core for LSI MPT2 */

/* TODO Move headers to mpsvar */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/selinfo.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/kthread.h>
#include <sys/endian.h>
#include <sys/eventhandler.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/proc.h>

#include <dev/pci/pcivar.h>

#include <cam/scsi/scsi_all.h>

#include <dev/mps/mpi/mpi2_type.h>
#include <dev/mps/mpi/mpi2.h>
#include <dev/mps/mpi/mpi2_ioc.h>
#include <dev/mps/mpi/mpi2_sas.h>
#include <dev/mps/mpi/mpi2_cnfg.h>
#include <dev/mps/mpi/mpi2_init.h>
#include <dev/mps/mpi/mpi2_tool.h>
#include <dev/mps/mps_ioctl.h>
#include <dev/mps/mpsvar.h>
#include <dev/mps/mps_table.h>

static int mps_diag_reset(struct mps_softc *sc, int sleep_flag);
static int mps_init_queues(struct mps_softc *sc);
static int mps_message_unit_reset(struct mps_softc *sc, int sleep_flag);
static int mps_transition_operational(struct mps_softc *sc);
static void mps_startup(void *arg);
static int mps_send_iocinit(struct mps_softc *sc);
static int mps_attach_log(struct mps_softc *sc);
static __inline void mps_complete_command(struct mps_command *cm);
static void mps_dispatch_event(struct mps_softc *sc, uintptr_t data,
    MPI2_EVENT_NOTIFICATION_REPLY *reply);
static void mps_config_complete(struct mps_softc *sc, struct mps_command *cm);
static void mps_periodic(void *);
static int mps_reregister_events(struct mps_softc *sc);
static void mps_enqueue_request(struct mps_softc *sc, struct mps_command *cm);
static int mps_wait_db_ack(struct mps_softc *sc, int timeout, int sleep_flag);
SYSCTL_NODE(_hw, OID_AUTO, mps, CTLFLAG_RD, 0, "MPS Driver Parameters");

MALLOC_DEFINE(M_MPT2, "mps", "mpt2 driver memory");

/*
 * Do a "Diagnostic Reset" aka a hard reset.  This should get the chip out of
 * any state and back to its initialization state machine.
 */
static char mpt2_reset_magic[] = { 0x00, 0x0f, 0x04, 0x0b, 0x02, 0x07, 0x0d };

/* Added this union to smoothly convert le64toh cm->cm_desc.Words.
 * Compiler only support unint64_t to be passed as argument.
 * Otherwise it will through below error
 * "aggregate value used where an integer was expected"
 */

typedef union _reply_descriptor {
        u64 word;
        struct {
                u32 low;
                u32 high;
        } u;
}reply_descriptor,address_descriptor;

/* 
 * sleep_flag can be either CAN_SLEEP or NO_SLEEP.
 * If this function is called from process context, it can sleep
 * and there is no harm to sleep, in case if this fuction is called
 * from Interrupt handler, we can not sleep and need NO_SLEEP flag set.
 * based on sleep flags driver will call either msleep, pause or DELAY.
 * msleep and pause are of same variant, but pause is used when mps_mtx
 * is not hold by driver.
 *
 */
static int
mps_diag_reset(struct mps_softc *sc,int sleep_flag)
{
	uint32_t reg;
	int i, error, tries = 0;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	/* Clear any pending interrupts */
	mps_regwrite(sc, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0x0);

	/*Force NO_SLEEP for threads prohibited to sleep
 	* e.a Thread from interrupt handler are prohibited to sleep.
 	*/	
	if(curthread->td_pflags & TDP_NOSLEEPING)
		sleep_flag = NO_SLEEP;
 
	/* Push the magic sequence */
	error = ETIMEDOUT;
	while (tries++ < 20) {
		for (i = 0; i < sizeof(mpt2_reset_magic); i++)
			mps_regwrite(sc, MPI2_WRITE_SEQUENCE_OFFSET,
			    mpt2_reset_magic[i]);
		/* wait 100 msec */
		if (mtx_owned(&sc->mps_mtx) && sleep_flag == CAN_SLEEP)
			msleep(&sc->msleep_fake_chan, &sc->mps_mtx, 0, "mpsdiag", hz/10);
		else if (sleep_flag == CAN_SLEEP)
			pause("mpsdiag", hz/10);
		else
			DELAY(100 * 1000);

		reg = mps_regread(sc, MPI2_HOST_DIAGNOSTIC_OFFSET);
		if (reg & MPI2_DIAG_DIAG_WRITE_ENABLE) {
			error = 0;
			break;
		}
	}
	if (error)
		return (error);

	/* Send the actual reset.  XXX need to refresh the reg? */
	mps_regwrite(sc, MPI2_HOST_DIAGNOSTIC_OFFSET,
	    reg | MPI2_DIAG_RESET_ADAPTER);

	/* Wait up to 300 seconds in 50ms intervals */
	error = ETIMEDOUT;
	for (i = 0; i < 60000; i++) {
		/* wait 50 msec */
		if (mtx_owned(&sc->mps_mtx) && sleep_flag == CAN_SLEEP)
			msleep(&sc->msleep_fake_chan, &sc->mps_mtx, 0, "mpsdiag", hz/20);
		else if (sleep_flag == CAN_SLEEP)
			pause("mpsdiag", hz/20);
		else
			DELAY(50 * 1000);
		reg = mps_regread(sc, MPI2_DOORBELL_OFFSET);
		if ((reg & MPI2_IOC_STATE_MASK) != MPI2_IOC_STATE_RESET) {
			error = 0;
			break;
		}
	}
	if (error)
		return (error);

	mps_regwrite(sc, MPI2_WRITE_SEQUENCE_OFFSET, 0x0);

	return (0);
}

static int
mps_message_unit_reset(struct mps_softc *sc, int sleep_flag)
{

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	mps_regwrite(sc, MPI2_DOORBELL_OFFSET,
	    MPI2_FUNCTION_IOC_MESSAGE_UNIT_RESET <<
	    MPI2_DOORBELL_FUNCTION_SHIFT);

	if (mps_wait_db_ack(sc, 5, sleep_flag) != 0) {
		mps_dprint(sc, MPS_FAULT, "Doorbell handshake failed : <%s>\n",
				__func__);
		return (ETIMEDOUT);
	}

	return (0);
}

static int
mps_transition_ready(struct mps_softc *sc)
{
	uint32_t reg, state;
	int error, tries = 0;
	int sleep_flags;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);
	/* If we are in attach call, do not sleep */
	sleep_flags = (sc->mps_flags & MPS_FLAGS_ATTACH_DONE)
					? CAN_SLEEP:NO_SLEEP;
	error = 0;
	while (tries++ < 5) {
		reg = mps_regread(sc, MPI2_DOORBELL_OFFSET);
		mps_dprint(sc, MPS_INFO, "Doorbell= 0x%x\n", reg);

		/*
		 * Ensure the IOC is ready to talk.  If it's not, try
		 * resetting it.
		 */
		if (reg & MPI2_DOORBELL_USED) {
			mps_diag_reset(sc, sleep_flags);
			DELAY(50000);
			continue;
		}

		/* Is the adapter owned by another peer? */
		if ((reg & MPI2_DOORBELL_WHO_INIT_MASK) ==
		    (MPI2_WHOINIT_PCI_PEER << MPI2_DOORBELL_WHO_INIT_SHIFT)) {
			device_printf(sc->mps_dev, "IOC is under the control "
			    "of another peer host, aborting initialization.\n");
			return (ENXIO);
		}
		
		state = reg & MPI2_IOC_STATE_MASK;
		if (state == MPI2_IOC_STATE_READY) {
			/* Ready to go! */
			error = 0;
			break;
		} else if (state == MPI2_IOC_STATE_FAULT) {
			mps_dprint(sc, MPS_INFO, "IOC in fault state 0x%x\n",
			    state & MPI2_DOORBELL_FAULT_CODE_MASK);
			mps_diag_reset(sc, sleep_flags);
		} else if (state == MPI2_IOC_STATE_OPERATIONAL) {
			/* Need to take ownership */
			mps_message_unit_reset(sc, sleep_flags);
		} else if (state == MPI2_IOC_STATE_RESET) {
			/* Wait a bit, IOC might be in transition */
			mps_dprint(sc, MPS_FAULT,
			    "IOC in unexpected reset state\n");
		} else {
			mps_dprint(sc, MPS_FAULT,
			    "IOC in unknown state 0x%x\n", state);
			error = EINVAL;
			break;
		}
	
		/* Wait 50ms for things to settle down. */
		DELAY(50000);
	}

	if (error)
		device_printf(sc->mps_dev, "Cannot transition IOC to ready\n");

	return (error);
}

static int
mps_transition_operational(struct mps_softc *sc)
{
	uint32_t reg, state;
	int error;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	error = 0;
	reg = mps_regread(sc, MPI2_DOORBELL_OFFSET);
	mps_dprint(sc, MPS_INFO, "Doorbell= 0x%x\n", reg);

	state = reg & MPI2_IOC_STATE_MASK;
	if (state != MPI2_IOC_STATE_READY) {
		if ((error = mps_transition_ready(sc)) != 0) {
			mps_dprint(sc, MPS_FAULT, 
			    "%s failed to transition ready\n", __func__);
			return (error);
		}
	}

	error = mps_send_iocinit(sc);
	return (error);
}

/* 
 * XXX Some of this should probably move to mps.c
 *
 * The terms diag reset and hard reset are used interchangeably in the MPI
 * docs to mean resetting the controller chip.  In this code diag reset
 * cleans everything up, and the hard reset function just sends the reset
 * sequence to the chip.  This should probably be refactored so that every
 * subsystem gets a reset notification of some sort, and can clean up
 * appropriately.
 */
int
mps_reinit(struct mps_softc *sc)
{
	int error;
	uint32_t db;

	mps_printf(sc, "%s sc %p\n", __func__, sc);

	mtx_assert(&sc->mps_mtx, MA_OWNED);

	if (sc->mps_flags & MPS_FLAGS_DIAGRESET) {
		mps_printf(sc, "%s reset already in progress\n", __func__);
		return 0;
	}

	/* make sure the completion callbacks can recognize they're getting
	 * a NULL cm_reply due to a reset.
	 */
	sc->mps_flags |= MPS_FLAGS_DIAGRESET;

	mps_printf(sc, "%s mask interrupts\n", __func__);
	mps_mask_intr(sc);

	error = mps_diag_reset(sc, CAN_SLEEP);
	if (error != 0) {
		panic("%s hard reset failed with error %d\n",
		    __func__, error);
	}

	/* Restore the PCI state, including the MSI-X registers */
	mps_pci_restore(sc);

	/* Give the I/O subsystem special priority to get itself prepared */
	mpssas_handle_reinit(sc);

	/* reinitialize queues after the reset */
	bzero(sc->free_queue, sc->fqdepth * 4);
	mps_init_queues(sc);

	/* get the chip out of the reset state */
	error = mps_transition_operational(sc);
	if (error != 0)
		panic("%s transition operational failed with error %d\n",
		    __func__, error);

	/* Reinitialize the reply queue. This is delicate because this
	 * function is typically invoked by task mgmt completion callbacks,
	 * which are called by the interrupt thread.  We need to make sure
	 * the interrupt handler loop will exit when we return to it, and
	 * that it will recognize the indexes we've changed.
	 */
	sc->replypostindex = 0;
	mps_regwrite(sc, MPI2_REPLY_FREE_HOST_INDEX_OFFSET, sc->replyfreeindex);
	mps_regwrite(sc, MPI2_REPLY_POST_HOST_INDEX_OFFSET, sc->replypostindex);

	db = mps_regread(sc, MPI2_DOORBELL_OFFSET);
	mps_printf(sc, "%s doorbell 0x%08x\n", __func__, db);

	mps_printf(sc, "%s unmask interrupts post %u free %u\n", __func__,
	    sc->replypostindex, sc->replyfreeindex);

	mps_unmask_intr(sc);

	mps_printf(sc, "%s restarting post %u free %u\n", __func__,
	    sc->replypostindex, sc->replyfreeindex);

	/* restart will reload the event masks clobbered by the reset, and
	 * then enable the port.
	 */
	mps_reregister_events(sc);

	/* the end of discovery will release the simq, so we're done. */
	mps_printf(sc, "%s finished sc %p post %u free %u\n", 
	    __func__, sc, 
	    sc->replypostindex, sc->replyfreeindex);

	sc->mps_flags &= ~MPS_FLAGS_DIAGRESET;

	return 0;
}

/* Wait for the chip to ACK a word that we've put into its FIFO 
 * Wait for <timeout> seconds. In single loop wait for busy loop
 * for 500 microseconds.
 * Total is [ 0.5 * (2000 * <timeout>) ] in miliseconds.
 * */
static int
mps_wait_db_ack(struct mps_softc *sc, int timeout, int sleep_flag)
{

	u32 cntdn, count;
	u32 int_status;
	u32 doorbell;

	count = 0;
	cntdn = (sleep_flag == CAN_SLEEP) ? 1000*timeout : 2000*timeout;
	do {
		int_status = mps_regread(sc, MPI2_HOST_INTERRUPT_STATUS_OFFSET);
		if (!(int_status & MPI2_HIS_SYS2IOC_DB_STATUS)) {
			mps_dprint(sc, MPS_INFO, 
			"%s: successfull count(%d), timeout(%d)\n",
			__func__, count, timeout);
		return 0;
		} else if (int_status & MPI2_HIS_IOC2SYS_DB_STATUS) {
			doorbell = mps_regread(sc, MPI2_DOORBELL_OFFSET);
			if ((doorbell & MPI2_IOC_STATE_MASK) ==
				MPI2_IOC_STATE_FAULT) {
				mps_dprint(sc, MPS_FAULT, 
					"fault_state(0x%04x)!\n", doorbell);
				return (EFAULT);
			}
		} else if (int_status == 0xFFFFFFFF)
			goto out;

		/* If it can sleep, sleep for 1 milisecond, else busy loop for 
		* 0.5 milisecond */
		if (mtx_owned(&sc->mps_mtx) && sleep_flag == CAN_SLEEP)
			msleep(&sc->msleep_fake_chan, &sc->mps_mtx, 0, 
			"mpsdba", hz/1000);
		else if (sleep_flag == CAN_SLEEP)
			pause("mpsdba", hz/1000);
		else
			DELAY(500);
		count++;
	} while (--cntdn);

	out:
	mps_dprint(sc, MPS_FAULT, "%s: failed due to timeout count(%d), "
		"int_status(%x)!\n", __func__, count, int_status);
	return (ETIMEDOUT);

}

/* Wait for the chip to signal that the next word in its FIFO can be fetched */
static int
mps_wait_db_int(struct mps_softc *sc)
{
	int retry;

	for (retry = 0; retry < MPS_DB_MAX_WAIT; retry++) {
		if ((mps_regread(sc, MPI2_HOST_INTERRUPT_STATUS_OFFSET) &
		    MPI2_HIS_IOC2SYS_DB_STATUS) != 0)
			return (0);
		DELAY(2000);
	}
	return (ETIMEDOUT);
}

/* Step through the synchronous command state machine, i.e. "Doorbell mode" */
static int
mps_request_sync(struct mps_softc *sc, void *req, MPI2_DEFAULT_REPLY *reply,
    int req_sz, int reply_sz, int timeout)
{
	uint32_t *data32;
	uint16_t *data16;
	int i, count, ioc_sz, residual;
	int sleep_flags = CAN_SLEEP;
	
	if(curthread->td_pflags & TDP_NOSLEEPING)
		sleep_flags = NO_SLEEP;

	/* Step 1 */
	mps_regwrite(sc, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0x0);

	/* Step 2 */
	if (mps_regread(sc, MPI2_DOORBELL_OFFSET) & MPI2_DOORBELL_USED)
		return (EBUSY);

	/* Step 3
	 * Announce that a message is coming through the doorbell.  Messages
	 * are pushed at 32bit words, so round up if needed.
	 */
	count = (req_sz + 3) / 4;
	mps_regwrite(sc, MPI2_DOORBELL_OFFSET,
	    (MPI2_FUNCTION_HANDSHAKE << MPI2_DOORBELL_FUNCTION_SHIFT) |
	    (count << MPI2_DOORBELL_ADD_DWORDS_SHIFT));

	/* Step 4 */
	if (mps_wait_db_int(sc) ||
	    (mps_regread(sc, MPI2_DOORBELL_OFFSET) & MPI2_DOORBELL_USED) == 0) {
		mps_dprint(sc, MPS_FAULT, "Doorbell failed to activate\n");
		return (ENXIO);
	}
	mps_regwrite(sc, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0x0);
	if (mps_wait_db_ack(sc, 5, sleep_flags) != 0) {
		mps_dprint(sc, MPS_FAULT, "Doorbell handshake failed\n");
		return (ENXIO);
	}

	/* Step 5 */
	/* Clock out the message data synchronously in 32-bit dwords*/
	data32 = (uint32_t *)req;
	for (i = 0; i < count; i++) {
		mps_regwrite(sc, MPI2_DOORBELL_OFFSET, htole32(data32[i]));
		if (mps_wait_db_ack(sc, 5, sleep_flags) != 0) {
			mps_dprint(sc, MPS_FAULT,
			    "Timeout while writing doorbell\n");
			return (ENXIO);
		}
	}

	/* Step 6 */
	/* Clock in the reply in 16-bit words.  The total length of the
	 * message is always in the 4th byte, so clock out the first 2 words
	 * manually, then loop the rest.
	 */
	data16 = (uint16_t *)reply;
	if (mps_wait_db_int(sc) != 0) {
		mps_dprint(sc, MPS_FAULT, "Timeout reading doorbell 0\n");
		return (ENXIO);
	}
	data16[0] =
	    mps_regread(sc, MPI2_DOORBELL_OFFSET) & MPI2_DOORBELL_DATA_MASK;
	mps_regwrite(sc, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0x0);
	if (mps_wait_db_int(sc) != 0) {
		mps_dprint(sc, MPS_FAULT, "Timeout reading doorbell 1\n");
		return (ENXIO);
	}
	data16[1] =
	    mps_regread(sc, MPI2_DOORBELL_OFFSET) & MPI2_DOORBELL_DATA_MASK;
	mps_regwrite(sc, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0x0);

	/* Number of 32bit words in the message */
	ioc_sz = reply->MsgLength;

	/*
	 * Figure out how many 16bit words to clock in without overrunning.
	 * The precision loss with dividing reply_sz can safely be
	 * ignored because the messages can only be multiples of 32bits.
	 */
	residual = 0;
	count = MIN((reply_sz / 4), ioc_sz) * 2;
	if (count < ioc_sz * 2) {
		residual = ioc_sz * 2 - count;
		mps_dprint(sc, MPS_FAULT, "Driver error, throwing away %d "
		    "residual message words\n", residual);
	}

	for (i = 2; i < count; i++) {
		if (mps_wait_db_int(sc) != 0) {
			mps_dprint(sc, MPS_FAULT,
			    "Timeout reading doorbell %d\n", i);
			return (ENXIO);
		}
		data16[i] = mps_regread(sc, MPI2_DOORBELL_OFFSET) &
		    MPI2_DOORBELL_DATA_MASK;
		mps_regwrite(sc, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0x0);
	}

	/*
	 * Pull out residual words that won't fit into the provided buffer.
	 * This keeps the chip from hanging due to a driver programming
	 * error.
	 */
	while (residual--) {
		if (mps_wait_db_int(sc) != 0) {
			mps_dprint(sc, MPS_FAULT,
			    "Timeout reading doorbell\n");
			return (ENXIO);
		}
		(void)mps_regread(sc, MPI2_DOORBELL_OFFSET);
		mps_regwrite(sc, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0x0);
	}

	/* Step 7 */
	if (mps_wait_db_int(sc) != 0) {
		mps_dprint(sc, MPS_FAULT, "Timeout waiting to exit doorbell\n");
		return (ENXIO);
	}
	if (mps_regread(sc, MPI2_DOORBELL_OFFSET) & MPI2_DOORBELL_USED)
		mps_dprint(sc, MPS_FAULT, "Warning, doorbell still active\n");
	mps_regwrite(sc, MPI2_HOST_INTERRUPT_STATUS_OFFSET, 0x0);

	return (0);
}

static void
mps_enqueue_request(struct mps_softc *sc, struct mps_command *cm)
{
	reply_descriptor rd;
	mps_dprint(sc, MPS_TRACE, "%s SMID %u cm %p ccb %p\n", __func__,
	    cm->cm_desc.Default.SMID, cm, cm->cm_ccb);

	if (sc->mps_flags & MPS_FLAGS_ATTACH_DONE && !(sc->mps_flags & MPS_FLAGS_SHUTDOWN))
		mtx_assert(&sc->mps_mtx, MA_OWNED);

	if (++sc->io_cmds_active > sc->io_cmds_highwater)
		sc->io_cmds_highwater++;
	rd.u.low = cm->cm_desc.Words.Low;
	rd.u.high = cm->cm_desc.Words.High;
	rd.word = htole64(rd.word);
	/* TODO-We may need to make below regwrite atomic */
	mps_regwrite(sc, MPI2_REQUEST_DESCRIPTOR_POST_LOW_OFFSET,
	    rd.u.low);
	mps_regwrite(sc, MPI2_REQUEST_DESCRIPTOR_POST_HIGH_OFFSET,
	    rd.u.high);
}

/*
 * Just the FACTS, ma'am.
 */
static int
mps_get_iocfacts(struct mps_softc *sc, MPI2_IOC_FACTS_REPLY *facts)
{
	MPI2_DEFAULT_REPLY *reply;
	MPI2_IOC_FACTS_REQUEST request;
	int error, req_sz, reply_sz;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	req_sz = sizeof(MPI2_IOC_FACTS_REQUEST);
	reply_sz = sizeof(MPI2_IOC_FACTS_REPLY);
	reply = (MPI2_DEFAULT_REPLY *)facts;

	bzero(&request, req_sz);
	request.Function = MPI2_FUNCTION_IOC_FACTS;
	error = mps_request_sync(sc, &request, reply, req_sz, reply_sz, 5);

	return (error);
}

static int
mps_get_portfacts(struct mps_softc *sc, MPI2_PORT_FACTS_REPLY *facts, int port)
{
	MPI2_PORT_FACTS_REQUEST *request;
	MPI2_PORT_FACTS_REPLY *reply;
	struct mps_command *cm;
	int error;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	if ((cm = mps_alloc_command(sc)) == NULL)
		return (EBUSY);
	request = (MPI2_PORT_FACTS_REQUEST *)cm->cm_req;
	request->Function = MPI2_FUNCTION_PORT_FACTS;
	request->PortNumber = port;
	cm->cm_desc.Default.RequestFlags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	cm->cm_data = NULL;
	error = mps_request_polled(sc, cm);
	reply = (MPI2_PORT_FACTS_REPLY *)cm->cm_reply;
	if (reply == NULL) {
		mps_printf(sc, "%s NULL reply\n", __func__);
		goto done;
	}
	if ((reply->IOCStatus & MPI2_IOCSTATUS_MASK) != MPI2_IOCSTATUS_SUCCESS) {
		mps_printf(sc, 
		    "%s error %d iocstatus 0x%x iocloginfo 0x%x type 0x%x\n",
		    __func__, error, reply->IOCStatus, reply->IOCLogInfo, 
		    reply->PortType);
		error = ENXIO;
	}
	bcopy(reply, facts, sizeof(MPI2_PORT_FACTS_REPLY));
done:
	mps_free_command(sc, cm);

	return (error);
}

static int
mps_send_iocinit(struct mps_softc *sc)
{
	MPI2_IOC_INIT_REQUEST	init;
	MPI2_DEFAULT_REPLY	reply;
	int req_sz, reply_sz, error;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	req_sz = sizeof(MPI2_IOC_INIT_REQUEST);
	reply_sz = sizeof(MPI2_IOC_INIT_REPLY);
	bzero(&init, req_sz);
	bzero(&reply, reply_sz);

	/*
	 * Fill in the init block.  Note that most addresses are
	 * deliberately in the lower 32bits of memory.  This is a micro-
	 * optimzation for PCI/PCIX, though it's not clear if it helps PCIe.
	 */
	init.Function = MPI2_FUNCTION_IOC_INIT;
	init.WhoInit = MPI2_WHOINIT_HOST_DRIVER;
	init.MsgVersion = htole16(MPI2_VERSION);
	init.HeaderVersion = htole16(MPI2_HEADER_VERSION);
	init.SystemRequestFrameSize = htole16(sc->facts->IOCRequestFrameSize);
	init.ReplyDescriptorPostQueueDepth = htole16(sc->pqdepth);
	init.ReplyFreeQueueDepth = htole16(sc->fqdepth);
	init.SenseBufferAddressHigh = 0;
	init.SystemReplyAddressHigh = 0;
	init.SystemRequestFrameBaseAddress.High = 0;
	init.SystemRequestFrameBaseAddress.Low = htole32((uint32_t)sc->req_busaddr);
	init.ReplyDescriptorPostQueueAddress.High = 0;
	init.ReplyDescriptorPostQueueAddress.Low = htole32((uint32_t)sc->post_busaddr);
	init.ReplyFreeQueueAddress.High = 0;
	init.ReplyFreeQueueAddress.Low = htole32((uint32_t)sc->free_busaddr);
	init.TimeStamp.High = 0;
	init.TimeStamp.Low = htole32((uint32_t)time_uptime);

	error = mps_request_sync(sc, &init, &reply, req_sz, reply_sz, 5);
	if ((reply.IOCStatus & MPI2_IOCSTATUS_MASK) != MPI2_IOCSTATUS_SUCCESS)
		error = ENXIO;

	mps_dprint(sc, MPS_INFO, "IOCInit status= 0x%x\n", reply.IOCStatus);
	return (error);
}

void
mps_memaddr_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	bus_addr_t *addr;

	addr = arg;
	*addr = segs[0].ds_addr;
}

static int
mps_alloc_queues(struct mps_softc *sc)
{
	bus_addr_t queues_busaddr;
	uint8_t *queues;
	int qsize, fqsize, pqsize;

	/*
	 * The reply free queue contains 4 byte entries in multiples of 16 and
	 * aligned on a 16 byte boundary. There must always be an unused entry.
	 * This queue supplies fresh reply frames for the firmware to use.
	 *
	 * The reply descriptor post queue contains 8 byte entries in
	 * multiples of 16 and aligned on a 16 byte boundary.  This queue
	 * contains filled-in reply frames sent from the firmware to the host.
	 *
	 * These two queues are allocated together for simplicity.
	 */
	sc->fqdepth = roundup2((sc->num_replies + 1), 16);
	sc->pqdepth = roundup2((sc->num_replies + 1), 16);
	fqsize= sc->fqdepth * 4;
	pqsize = sc->pqdepth * 8;
	qsize = fqsize + pqsize;

        if (bus_dma_tag_create( sc->mps_parent_dmat,    /* parent */
				16, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
                                qsize,			/* maxsize */
                                1,			/* nsegments */
                                qsize,			/* maxsegsize */
                                0,			/* flags */
                                NULL, NULL,		/* lockfunc, lockarg */
                                &sc->queues_dmat)) {
		device_printf(sc->mps_dev, "Cannot allocate queues DMA tag\n");
		return (ENOMEM);
        }
        if (bus_dmamem_alloc(sc->queues_dmat, (void **)&queues, BUS_DMA_NOWAIT,
	    &sc->queues_map)) {
		device_printf(sc->mps_dev, "Cannot allocate queues memory\n");
		return (ENOMEM);
        }
        bzero(queues, qsize);
        bus_dmamap_load(sc->queues_dmat, sc->queues_map, queues, qsize,
	    mps_memaddr_cb, &queues_busaddr, 0);

	sc->free_queue = (uint32_t *)queues;
	sc->free_busaddr = queues_busaddr;
	sc->post_queue = (MPI2_REPLY_DESCRIPTORS_UNION *)(queues + fqsize);
	sc->post_busaddr = queues_busaddr + fqsize;

	return (0);
}

static int
mps_alloc_replies(struct mps_softc *sc)
{
	int rsize, num_replies;

	/*
	 * sc->num_replies should be one less than sc->fqdepth.  We need to
	 * allocate space for sc->fqdepth replies, but only sc->num_replies
	 * replies can be used at once.
	 */
	num_replies = max(sc->fqdepth, sc->num_replies);

	rsize = sc->facts->ReplyFrameSize * num_replies * 4; 
        if (bus_dma_tag_create( sc->mps_parent_dmat,    /* parent */
				4, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
                                rsize,			/* maxsize */
                                1,			/* nsegments */
                                rsize,			/* maxsegsize */
                                0,			/* flags */
                                NULL, NULL,		/* lockfunc, lockarg */
                                &sc->reply_dmat)) {
		device_printf(sc->mps_dev, "Cannot allocate replies DMA tag\n");
		return (ENOMEM);
        }
        if (bus_dmamem_alloc(sc->reply_dmat, (void **)&sc->reply_frames,
	    BUS_DMA_NOWAIT, &sc->reply_map)) {
		device_printf(sc->mps_dev, "Cannot allocate replies memory\n");
		return (ENOMEM);
        }
        bzero(sc->reply_frames, rsize);
        bus_dmamap_load(sc->reply_dmat, sc->reply_map, sc->reply_frames, rsize,
	    mps_memaddr_cb, &sc->reply_busaddr, 0);

	return (0);
}

static int
mps_alloc_requests(struct mps_softc *sc)
{
	struct mps_command *cm;
	struct mps_chain *chain;
	int i, rsize, nsegs;

	rsize = sc->facts->IOCRequestFrameSize * sc->num_reqs * 4;
        if (bus_dma_tag_create( sc->mps_parent_dmat,    /* parent */
				16, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
                                rsize,			/* maxsize */
                                1,			/* nsegments */
                                rsize,			/* maxsegsize */
                                0,			/* flags */
                                NULL, NULL,		/* lockfunc, lockarg */
                                &sc->req_dmat)) {
		device_printf(sc->mps_dev, "Cannot allocate request DMA tag\n");
		return (ENOMEM);
        }
        if (bus_dmamem_alloc(sc->req_dmat, (void **)&sc->req_frames,
	    BUS_DMA_NOWAIT, &sc->req_map)) {
		device_printf(sc->mps_dev, "Cannot allocate request memory\n");
		return (ENOMEM);
        }
        bzero(sc->req_frames, rsize);
        bus_dmamap_load(sc->req_dmat, sc->req_map, sc->req_frames, rsize,
	    mps_memaddr_cb, &sc->req_busaddr, 0);

	rsize = sc->facts->IOCRequestFrameSize * sc->max_chains * 4;
        if (bus_dma_tag_create( sc->mps_parent_dmat,    /* parent */
				16, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
                                rsize,			/* maxsize */
                                1,			/* nsegments */
                                rsize,			/* maxsegsize */
                                0,			/* flags */
                                NULL, NULL,		/* lockfunc, lockarg */
                                &sc->chain_dmat)) {
		device_printf(sc->mps_dev, "Cannot allocate chain DMA tag\n");
		return (ENOMEM);
        }
        if (bus_dmamem_alloc(sc->chain_dmat, (void **)&sc->chain_frames,
	    BUS_DMA_NOWAIT, &sc->chain_map)) {
		device_printf(sc->mps_dev, "Cannot allocate chain memory\n");
		return (ENOMEM);
        }
        bzero(sc->chain_frames, rsize);
        bus_dmamap_load(sc->chain_dmat, sc->chain_map, sc->chain_frames, rsize,
	    mps_memaddr_cb, &sc->chain_busaddr, 0);

	rsize = MPS_SENSE_LEN * sc->num_reqs;
        if (bus_dma_tag_create( sc->mps_parent_dmat,    /* parent */
				1, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
                                rsize,			/* maxsize */
                                1,			/* nsegments */
                                rsize,			/* maxsegsize */
                                0,			/* flags */
                                NULL, NULL,		/* lockfunc, lockarg */
                                &sc->sense_dmat)) {
		device_printf(sc->mps_dev, "Cannot allocate sense DMA tag\n");
		return (ENOMEM);
        }
        if (bus_dmamem_alloc(sc->sense_dmat, (void **)&sc->sense_frames,
	    BUS_DMA_NOWAIT, &sc->sense_map)) {
		device_printf(sc->mps_dev, "Cannot allocate sense memory\n");
		return (ENOMEM);
        }
        bzero(sc->sense_frames, rsize);
        bus_dmamap_load(sc->sense_dmat, sc->sense_map, sc->sense_frames, rsize,
	    mps_memaddr_cb, &sc->sense_busaddr, 0);

	sc->chains = malloc(sizeof(struct mps_chain) * sc->max_chains, M_MPT2,
	    M_WAITOK | M_ZERO);
	if(!sc->chains) {
		device_printf(sc->mps_dev, 
		"Cannot allocate chains memory %s %d\n",
		 __func__, __LINE__);
		return (ENOMEM);
	}
	for (i = 0; i < sc->max_chains; i++) {
		chain = &sc->chains[i];
		chain->chain = (MPI2_SGE_IO_UNION *)(sc->chain_frames +
		    i * sc->facts->IOCRequestFrameSize * 4);
		chain->chain_busaddr = sc->chain_busaddr +
		    i * sc->facts->IOCRequestFrameSize * 4;
		mps_free_chain(sc, chain);
		sc->chain_free_lowwater++;
	}

	/* XXX Need to pick a more precise value */
	nsegs = (MAXPHYS / PAGE_SIZE) + 1;
        if (bus_dma_tag_create( sc->mps_parent_dmat,    /* parent */
				1, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
                                BUS_SPACE_MAXSIZE_32BIT,/* maxsize */
                                nsegs,			/* nsegments */
                                BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
                                BUS_DMA_ALLOCNOW,	/* flags */
                                busdma_lock_mutex,	/* lockfunc */
				&sc->mps_mtx,		/* lockarg */
                                &sc->buffer_dmat)) {
		device_printf(sc->mps_dev, "Cannot allocate buffer DMA tag\n");
		return (ENOMEM);
        }

	/*
	 * SMID 0 cannot be used as a free command per the firmware spec.
	 * Just drop that command instead of risking accounting bugs.
	 */
	sc->commands = malloc(sizeof(struct mps_command) * sc->num_reqs,
	    M_MPT2, M_WAITOK | M_ZERO);
	if(!sc->commands) {
		device_printf(sc->mps_dev, "Cannot allocate memory %s %d\n",
		 __func__, __LINE__);
		return (ENOMEM);
	}
	for (i = 1; i < sc->num_reqs; i++) {
		cm = &sc->commands[i];
		cm->cm_req = sc->req_frames +
		    i * sc->facts->IOCRequestFrameSize * 4;
		cm->cm_req_busaddr = sc->req_busaddr +
		    i * sc->facts->IOCRequestFrameSize * 4;
		cm->cm_sense = &sc->sense_frames[i];
		cm->cm_sense_busaddr = sc->sense_busaddr + i * MPS_SENSE_LEN;
		cm->cm_desc.Default.SMID = i;
		cm->cm_sc = sc;
		TAILQ_INIT(&cm->cm_chain_list);
		callout_init_mtx(&cm->cm_callout, &sc->mps_mtx, 0);

		/* XXX Is a failure here a critical problem? */
		if (bus_dmamap_create(sc->buffer_dmat, 0, &cm->cm_dmamap) == 0)
			if (i <= sc->facts->HighPriorityCredit)
				mps_free_high_priority_command(sc, cm);
			else
				mps_free_command(sc, cm);
		else {
			panic("failed to allocate command %d\n", i);
			sc->num_reqs = i;
			break;
		}
	}

	return (0);
}

static int
mps_init_queues(struct mps_softc *sc)
{
	int i;

	memset((uint8_t *)sc->post_queue, 0xff, sc->pqdepth * 8);

	/*
	 * According to the spec, we need to use one less reply than we
	 * have space for on the queue.  So sc->num_replies (the number we
	 * use) should be less than sc->fqdepth (allocated size).
	 */
	if (sc->num_replies >= sc->fqdepth)
		return (EINVAL);

	/*
	 * Initialize all of the free queue entries.
	 */
	for (i = 0; i < sc->fqdepth; i++)
		sc->free_queue[i] = sc->reply_busaddr + (i * sc->facts->ReplyFrameSize * 4);
	sc->replyfreeindex = sc->num_replies;

	return (0);
}

/* Get the driver parameter tunables.  Lowest priority are the driver defaults.
 * Next are the global settings, if they exist.  Highest are the per-unit
 * settings, if they exist.
 */
static void
mps_get_tunables(struct mps_softc *sc)
{
	char tmpstr[80];

	/* XXX default to some debugging for now */
	sc->mps_debug = MPS_FAULT;
	sc->disable_msix = 0;
	sc->disable_msi = 0;
	sc->max_chains = MPS_CHAIN_FRAMES;

	/*
	 * Grab the global variables.
	 */
	TUNABLE_INT_FETCH("hw.mps.debug_level", &sc->mps_debug);
	TUNABLE_INT_FETCH("hw.mps.disable_msix", &sc->disable_msix);
	TUNABLE_INT_FETCH("hw.mps.disable_msi", &sc->disable_msi);
	TUNABLE_INT_FETCH("hw.mps.max_chains", &sc->max_chains);

	/* Grab the unit-instance variables */
	snprintf(tmpstr, sizeof(tmpstr), "dev.mps.%d.debug_level",
	    device_get_unit(sc->mps_dev));
	TUNABLE_INT_FETCH(tmpstr, &sc->mps_debug);

	snprintf(tmpstr, sizeof(tmpstr), "dev.mps.%d.disable_msix",
	    device_get_unit(sc->mps_dev));
	TUNABLE_INT_FETCH(tmpstr, &sc->disable_msix);

	snprintf(tmpstr, sizeof(tmpstr), "dev.mps.%d.disable_msi",
	    device_get_unit(sc->mps_dev));
	TUNABLE_INT_FETCH(tmpstr, &sc->disable_msi);

	snprintf(tmpstr, sizeof(tmpstr), "dev.mps.%d.max_chains",
	    device_get_unit(sc->mps_dev));
	TUNABLE_INT_FETCH(tmpstr, &sc->max_chains);
}

static void
mps_setup_sysctl(struct mps_softc *sc)
{
	struct sysctl_ctx_list	*sysctl_ctx = NULL;
	struct sysctl_oid	*sysctl_tree = NULL;
	char tmpstr[80], tmpstr2[80];

	/*
	 * Setup the sysctl variable so the user can change the debug level
	 * on the fly.
	 */
	snprintf(tmpstr, sizeof(tmpstr), "MPS controller %d",
	    device_get_unit(sc->mps_dev));
	snprintf(tmpstr2, sizeof(tmpstr2), "%d", device_get_unit(sc->mps_dev));

	sysctl_ctx = device_get_sysctl_ctx(sc->mps_dev);
	if (sysctl_ctx != NULL)
		sysctl_tree = device_get_sysctl_tree(sc->mps_dev);

	if (sysctl_tree == NULL) {
		sysctl_ctx_init(&sc->sysctl_ctx);
		sc->sysctl_tree = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
		    SYSCTL_STATIC_CHILDREN(_hw_mps), OID_AUTO, tmpstr2,
		    CTLFLAG_RD, 0, tmpstr);
		if (sc->sysctl_tree == NULL)
			return;
		sysctl_ctx = &sc->sysctl_ctx;
		sysctl_tree = sc->sysctl_tree;
	}

	SYSCTL_ADD_INT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "debug_level", CTLFLAG_RW, &sc->mps_debug, 0,
	    "mps debug level");

	SYSCTL_ADD_INT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "disable_msix", CTLFLAG_RD, &sc->disable_msix, 0,
	    "Disable the use of MSI-X interrupts");

	SYSCTL_ADD_INT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "disable_msi", CTLFLAG_RD, &sc->disable_msi, 0,
	    "Disable the use of MSI interrupts");

	SYSCTL_ADD_STRING(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "firmware_version", CTLFLAG_RW, &sc->fw_version,
	    strlen(sc->fw_version), "firmware version");

	SYSCTL_ADD_STRING(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "driver_version", CTLFLAG_RW, MPS_DRIVER_VERSION,
	    strlen(MPS_DRIVER_VERSION), "driver version");

	SYSCTL_ADD_INT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "io_cmds_active", CTLFLAG_RD,
	    &sc->io_cmds_active, 0, "number of currently active commands");

	SYSCTL_ADD_INT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "io_cmds_highwater", CTLFLAG_RD,
	    &sc->io_cmds_highwater, 0, "maximum active commands seen");

	SYSCTL_ADD_INT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "chain_free", CTLFLAG_RD,
	    &sc->chain_free, 0, "number of free chain elements");

	SYSCTL_ADD_INT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "chain_free_lowwater", CTLFLAG_RD,
	    &sc->chain_free_lowwater, 0,"lowest number of free chain elements");

	SYSCTL_ADD_INT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "max_chains", CTLFLAG_RD,
	    &sc->max_chains, 0,"maximum chain frames that will be allocated");

#if __FreeBSD_version >= 900030
	SYSCTL_ADD_UQUAD(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree),
	    OID_AUTO, "chain_alloc_fail", CTLFLAG_RD,
	    &sc->chain_alloc_fail, "chain allocation failures");
#endif //FreeBSD_version >= 900030
}

int
mps_attach(struct mps_softc *sc)
{
	int i, error;

	mps_get_tunables(sc);

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	mtx_init(&sc->mps_mtx, "MPT2SAS lock", NULL, MTX_DEF);
	callout_init_mtx(&sc->periodic, &sc->mps_mtx, 0);
	TAILQ_INIT(&sc->event_list);

	if ((error = mps_transition_ready(sc)) != 0) {
		mps_printf(sc, "%s failed to transition ready\n", __func__);
		return (error);
	}

	sc->facts = malloc(sizeof(MPI2_IOC_FACTS_REPLY), M_MPT2,
	    M_ZERO|M_NOWAIT);
	if(!sc->facts) {
		device_printf(sc->mps_dev, "Cannot allocate memory %s %d\n",
		 __func__, __LINE__);
		return (ENOMEM);
	}
	if ((error = mps_get_iocfacts(sc, sc->facts)) != 0)
		return (error);

	mps_print_iocfacts(sc, sc->facts);

	snprintf(sc->fw_version, sizeof(sc->fw_version), 
	    "%02d.%02d.%02d.%02d", 
	    sc->facts->FWVersion.Struct.Major,
	    sc->facts->FWVersion.Struct.Minor,
	    sc->facts->FWVersion.Struct.Unit,
	    sc->facts->FWVersion.Struct.Dev);

	mps_printf(sc, "Firmware: %s, Driver: %s\n", sc->fw_version,
	    MPS_DRIVER_VERSION);
	mps_printf(sc, "IOCCapabilities: %b\n", sc->facts->IOCCapabilities,
	    "\20" "\3ScsiTaskFull" "\4DiagTrace" "\5SnapBuf" "\6ExtBuf"
	    "\7EEDP" "\10BiDirTarg" "\11Multicast" "\14TransRetry" "\15IR"
	    "\16EventReplay" "\17RaidAccel" "\20MSIXIndex" "\21HostDisc");

	/*
	 * If the chip doesn't support event replay then a hard reset will be
	 * required to trigger a full discovery.  Do the reset here then
	 * retransition to Ready.  A hard reset might have already been done,
	 * but it doesn't hurt to do it again.
	 */
	if ((sc->facts->IOCCapabilities &
	    MPI2_IOCFACTS_CAPABILITY_EVENT_REPLAY) == 0) {
		mps_diag_reset(sc, NO_SLEEP);
		if ((error = mps_transition_ready(sc)) != 0)
			return (error);
	}

	/*
	 * Set flag if IR Firmware is loaded.
	 */
	if (sc->facts->IOCCapabilities &
	    MPI2_IOCFACTS_CAPABILITY_INTEGRATED_RAID)
		sc->ir_firmware = 1;

	/*
	 * Check if controller supports FW diag buffers and set flag to enable
	 * each type.
	 */
	if (sc->facts->IOCCapabilities &
	    MPI2_IOCFACTS_CAPABILITY_DIAG_TRACE_BUFFER)
		sc->fw_diag_buffer_list[MPI2_DIAG_BUF_TYPE_TRACE].enabled =
		    TRUE;
	if (sc->facts->IOCCapabilities &
	    MPI2_IOCFACTS_CAPABILITY_SNAPSHOT_BUFFER)
		sc->fw_diag_buffer_list[MPI2_DIAG_BUF_TYPE_SNAPSHOT].enabled =
		    TRUE;
	if (sc->facts->IOCCapabilities &
	    MPI2_IOCFACTS_CAPABILITY_EXTENDED_BUFFER)
		sc->fw_diag_buffer_list[MPI2_DIAG_BUF_TYPE_EXTENDED].enabled =
		    TRUE;

	/*
	 * Set flag if EEDP is supported and if TLR is supported.
	 */
	if (sc->facts->IOCCapabilities & MPI2_IOCFACTS_CAPABILITY_EEDP)
		sc->eedp_enabled = TRUE;
	if (sc->facts->IOCCapabilities & MPI2_IOCFACTS_CAPABILITY_TLR)
		sc->control_TLR = TRUE;

	/*
	 * Size the queues. Since the reply queues always need one free entry,
	 * we'll just deduct one reply message here.
	 */
	sc->num_reqs = MIN(MPS_REQ_FRAMES, sc->facts->RequestCredit);
	sc->num_replies = MIN(MPS_REPLY_FRAMES + MPS_EVT_REPLY_FRAMES,
	    sc->facts->MaxReplyDescriptorPostQueueDepth) - 1;
	TAILQ_INIT(&sc->req_list);
	TAILQ_INIT(&sc->high_priority_req_list);
	TAILQ_INIT(&sc->chain_list);
	TAILQ_INIT(&sc->tm_list);

	if (((error = mps_alloc_queues(sc)) != 0) ||
	    ((error = mps_alloc_replies(sc)) != 0) ||
	    ((error = mps_alloc_requests(sc)) != 0)) {
		mps_printf(sc, "%s failed to alloc\n", __func__);
		mps_free(sc);
		return (error);
	}

	if (((error = mps_init_queues(sc)) != 0) ||
	    ((error = mps_transition_operational(sc)) != 0)) {
		mps_printf(sc, "%s failed to transition operational\n", __func__);
		mps_free(sc);
		return (error);
	}

	/*
	 * Finish the queue initialization.
	 * These are set here instead of in mps_init_queues() because the
	 * IOC resets these values during the state transition in
	 * mps_transition_operational().  The free index is set to 1
	 * because the corresponding index in the IOC is set to 0, and the
	 * IOC treats the queues as full if both are set to the same value.
	 * Hence the reason that the queue can't hold all of the possible
	 * replies.
	 */
	sc->replypostindex = 0;
	mps_regwrite(sc, MPI2_REPLY_FREE_HOST_INDEX_OFFSET, sc->replyfreeindex);
	mps_regwrite(sc, MPI2_REPLY_POST_HOST_INDEX_OFFSET, 0);

	sc->pfacts = malloc(sizeof(MPI2_PORT_FACTS_REPLY) *
	    sc->facts->NumberOfPorts, M_MPT2, M_ZERO|M_WAITOK);
	if(!sc->pfacts) {
		device_printf(sc->mps_dev, "Cannot allocate memory %s %d\n",
		 __func__, __LINE__);
		return (ENOMEM);
	}
	for (i = 0; i < sc->facts->NumberOfPorts; i++) {
		if ((error = mps_get_portfacts(sc, &sc->pfacts[i], i)) != 0) {
			mps_printf(sc, "%s failed to get portfacts for port %d\n",
			    __func__, i);
			mps_free(sc);
			return (error);
		}
		mps_print_portfacts(sc, &sc->pfacts[i]);
	}

	/* Attach the subsystems so they can prepare their event masks. */
	/* XXX Should be dynamic so that IM/IR and user modules can attach */
	if (((error = mps_attach_log(sc)) != 0) ||
	    ((error = mps_attach_sas(sc)) != 0) ||
	    ((error = mps_attach_user(sc)) != 0)) {
		mps_printf(sc, "%s failed to attach all subsystems: error %d\n",
		    __func__, error);
		mps_free(sc);
		return (error);
	}

	if ((error = mps_pci_setup_interrupts(sc)) != 0) {
		mps_printf(sc, "%s failed to setup interrupts\n", __func__);
		mps_free(sc);
		return (error);
	}

	/*
	 * The static page function currently read is ioc page8.  Others can be
	 * added in future.
	 */
	mps_base_static_config_pages(sc);

	/* Start the periodic watchdog check on the IOC Doorbell */
	mps_periodic(sc);

	/*
	 * The portenable will kick off discovery events that will drive the
	 * rest of the initialization process.  The CAM/SAS module will
	 * hold up the boot sequence until discovery is complete.
	 */
	sc->mps_ich.ich_func = mps_startup;
	sc->mps_ich.ich_arg = sc;
	if (config_intrhook_establish(&sc->mps_ich) != 0) {
		mps_dprint(sc, MPS_FAULT, "Cannot establish MPS config hook\n");
		error = EINVAL;
	}

	/*
	 * Allow IR to shutdown gracefully when shutdown occurs.
	 */
	sc->shutdown_eh = EVENTHANDLER_REGISTER(shutdown_final,
	    mpssas_ir_shutdown, sc, SHUTDOWN_PRI_DEFAULT);

	if (sc->shutdown_eh == NULL)
		mps_dprint(sc, MPS_FAULT, "shutdown event registration "
		    "failed\n");

	mps_setup_sysctl(sc);

	sc->mps_flags |= MPS_FLAGS_ATTACH_DONE;

	return (error);
}

/* Run through any late-start handlers. */
static void
mps_startup(void *arg)
{
	struct mps_softc *sc;

	sc = (struct mps_softc *)arg;

	mps_lock(sc);
	mps_unmask_intr(sc);
	/* initialize device mapping tables */
	mps_mapping_initialize(sc);
	mpssas_startup(sc);
	mps_unlock(sc);
}

/* Periodic watchdog.  Is called with the driver lock already held. */
static void
mps_periodic(void *arg)
{
	struct mps_softc *sc;
	uint32_t db;

	sc = (struct mps_softc *)arg;
	if (sc->mps_flags & MPS_FLAGS_SHUTDOWN)
		return;

	db = mps_regread(sc, MPI2_DOORBELL_OFFSET);
	if ((db & MPI2_IOC_STATE_MASK) == MPI2_IOC_STATE_FAULT) {
		device_printf(sc->mps_dev, "IOC Fault 0x%08x, Resetting\n", db);

		mps_reinit(sc);
	}

	callout_reset(&sc->periodic, MPS_PERIODIC_DELAY * hz, mps_periodic, sc);
}

static void
mps_log_evt_handler(struct mps_softc *sc, uintptr_t data,
    MPI2_EVENT_NOTIFICATION_REPLY *event)
{
	MPI2_EVENT_DATA_LOG_ENTRY_ADDED *entry;

	mps_print_event(sc, event);

	switch (event->Event) {
	case MPI2_EVENT_LOG_DATA:
		device_printf(sc->mps_dev, "MPI2_EVENT_LOG_DATA:\n");
		hexdump(event->EventData, event->EventDataLength, NULL, 0);
		break;
	case MPI2_EVENT_LOG_ENTRY_ADDED:
		entry = (MPI2_EVENT_DATA_LOG_ENTRY_ADDED *)event->EventData;
		mps_dprint(sc, MPS_INFO, "MPI2_EVENT_LOG_ENTRY_ADDED event "
		    "0x%x Sequence %d:\n", entry->LogEntryQualifier,
		     entry->LogSequence);
		break;
	default:
		break;
	}
	return;
}

static int
mps_attach_log(struct mps_softc *sc)
{
	u32 events[MPI2_EVENT_NOTIFY_EVENTMASK_WORDS];

	bzero(events, 16);
	setbit(events, MPI2_EVENT_LOG_DATA);
	setbit(events, MPI2_EVENT_LOG_ENTRY_ADDED);

	mps_register_events(sc, events, mps_log_evt_handler, NULL,
	    &sc->mps_log_eh);

	return (0);
}

static int
mps_detach_log(struct mps_softc *sc)
{

	if (sc->mps_log_eh != NULL)
		mps_deregister_events(sc, sc->mps_log_eh);
	return (0);
}

/*
 * Free all of the driver resources and detach submodules.  Should be called
 * without the lock held.
 */
int
mps_free(struct mps_softc *sc)
{
	struct mps_command *cm;
	int i, error;

	/* Turn off the watchdog */
	mps_lock(sc);
	sc->mps_flags |= MPS_FLAGS_SHUTDOWN;
	mps_unlock(sc);
	/* Lock must not be held for this */
	callout_drain(&sc->periodic);

	if (((error = mps_detach_log(sc)) != 0) ||
	    ((error = mps_detach_sas(sc)) != 0))
		return (error);

	mps_detach_user(sc);

	/* Put the IOC back in the READY state. */
	mps_lock(sc);
	if ((error = mps_transition_ready(sc)) != 0) {
		mps_unlock(sc);
		return (error);
	}
	mps_unlock(sc);

	if (sc->facts != NULL)
		free(sc->facts, M_MPT2);

	if (sc->pfacts != NULL)
		free(sc->pfacts, M_MPT2);

	if (sc->post_busaddr != 0)
		bus_dmamap_unload(sc->queues_dmat, sc->queues_map);
	if (sc->post_queue != NULL)
		bus_dmamem_free(sc->queues_dmat, sc->post_queue,
		    sc->queues_map);
	if (sc->queues_dmat != NULL)
		bus_dma_tag_destroy(sc->queues_dmat);

	if (sc->chain_busaddr != 0)
		bus_dmamap_unload(sc->chain_dmat, sc->chain_map);
	if (sc->chain_frames != NULL)
		bus_dmamem_free(sc->chain_dmat, sc->chain_frames,sc->chain_map);
	if (sc->chain_dmat != NULL)
		bus_dma_tag_destroy(sc->chain_dmat);

	if (sc->sense_busaddr != 0)
		bus_dmamap_unload(sc->sense_dmat, sc->sense_map);
	if (sc->sense_frames != NULL)
		bus_dmamem_free(sc->sense_dmat, sc->sense_frames,sc->sense_map);
	if (sc->sense_dmat != NULL)
		bus_dma_tag_destroy(sc->sense_dmat);

	if (sc->reply_busaddr != 0)
		bus_dmamap_unload(sc->reply_dmat, sc->reply_map);
	if (sc->reply_frames != NULL)
		bus_dmamem_free(sc->reply_dmat, sc->reply_frames,sc->reply_map);
	if (sc->reply_dmat != NULL)
		bus_dma_tag_destroy(sc->reply_dmat);

	if (sc->req_busaddr != 0)
		bus_dmamap_unload(sc->req_dmat, sc->req_map);
	if (sc->req_frames != NULL)
		bus_dmamem_free(sc->req_dmat, sc->req_frames, sc->req_map);
	if (sc->req_dmat != NULL)
		bus_dma_tag_destroy(sc->req_dmat);

	if (sc->chains != NULL)
		free(sc->chains, M_MPT2);
	if (sc->commands != NULL) {
		for (i = 1; i < sc->num_reqs; i++) {
			cm = &sc->commands[i];
			bus_dmamap_destroy(sc->buffer_dmat, cm->cm_dmamap);
		}
		free(sc->commands, M_MPT2);
	}
	if (sc->buffer_dmat != NULL)
		bus_dma_tag_destroy(sc->buffer_dmat);

	if (sc->sysctl_tree != NULL)
		sysctl_ctx_free(&sc->sysctl_ctx);

	mps_mapping_free_memory(sc);

	/* Deregister the shutdown function */
	if (sc->shutdown_eh != NULL)
		EVENTHANDLER_DEREGISTER(shutdown_final, sc->shutdown_eh);

	mtx_destroy(&sc->mps_mtx);

	return (0);
}

static __inline void
mps_complete_command(struct mps_command *cm)
{
	if (cm->cm_flags & MPS_CM_FLAGS_POLLED)
		cm->cm_flags |= MPS_CM_FLAGS_COMPLETE;

	if (cm->cm_complete != NULL) {
		mps_dprint(cm->cm_sc, MPS_TRACE,
			   "%s cm %p calling cm_complete %p data %p reply %p\n",
			   __func__, cm, cm->cm_complete, cm->cm_complete_data,
			   cm->cm_reply);
		cm->cm_complete(cm->cm_sc, cm);
	}

	if (cm->cm_flags & MPS_CM_FLAGS_WAKEUP) {
		mps_dprint(cm->cm_sc, MPS_TRACE, "%s: waking up %p\n",
			   __func__, cm);
		wakeup(cm);
	}

	if (cm->cm_sc->io_cmds_active != 0) {
		cm->cm_sc->io_cmds_active--;
	} else {
		mps_dprint(cm->cm_sc, MPS_INFO, "Warning: io_cmds_active is "
		    "out of sync - resynching to 0\n");
	}
}


static void
mps_sas_log_info(struct mps_softc *sc , u32 log_info)
{
	union loginfo_type {
		u32     loginfo;
		struct {
			u32     subcode:16;
			u32     code:8;
			u32     originator:4;
			u32     bus_type:4;
		} dw;
	};
	union loginfo_type sas_loginfo;
	char *originator_str = NULL;

	sas_loginfo.loginfo = log_info;
	if (sas_loginfo.dw.bus_type != 3 /*SAS*/)
		return;

	/* each nexus loss loginfo */
	if (log_info == 0x31170000)
		return;

	/* eat the loginfos associated with task aborts */
	if ((log_info == 30050000 || log_info ==
	    0x31140000 || log_info == 0x31130000))
		return;

	switch (sas_loginfo.dw.originator) {
	case 0:
		originator_str = "IOP";
		break;
	case 1:
		originator_str = "PL";
		break;
	case 2:
		originator_str = "IR";
		break;
}

	mps_dprint(sc, MPS_INFO, "log_info(0x%08x): originator(%s), "
	"code(0x%02x), sub_code(0x%04x)\n", log_info,
	originator_str, sas_loginfo.dw.code,
	sas_loginfo.dw.subcode);
}

static void
mps_display_reply_info(struct mps_softc *sc, uint8_t *reply)
{
	MPI2DefaultReply_t *mpi_reply;
	u16 sc_status;

	mpi_reply = (MPI2DefaultReply_t*)reply;
	sc_status = le16toh(mpi_reply->IOCStatus);
	if (sc_status & MPI2_IOCSTATUS_FLAG_LOG_INFO_AVAILABLE)
		mps_sas_log_info(sc, le32toh(mpi_reply->IOCLogInfo));
}
void
mps_intr(void *data)
{
	struct mps_softc *sc;
	uint32_t status;

	sc = (struct mps_softc *)data;
	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	/*
	 * Check interrupt status register to flush the bus.  This is
	 * needed for both INTx interrupts and driver-driven polling
	 */
	status = mps_regread(sc, MPI2_HOST_INTERRUPT_STATUS_OFFSET);
	if ((status & MPI2_HIS_REPLY_DESCRIPTOR_INTERRUPT) == 0)
		return;

	mps_lock(sc);
	mps_intr_locked(data);
	mps_unlock(sc);
	return;
}

/*
 * In theory, MSI/MSIX interrupts shouldn't need to read any registers on the
 * chip.  Hopefully this theory is correct.
 */
void
mps_intr_msi(void *data)
{
	struct mps_softc *sc;

	sc = (struct mps_softc *)data;
	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);
	mps_lock(sc);
	mps_intr_locked(data);
	mps_unlock(sc);
	return;
}

/*
 * The locking is overly broad and simplistic, but easy to deal with for now.
 */
void
mps_intr_locked(void *data)
{
	MPI2_REPLY_DESCRIPTORS_UNION *desc;
	struct mps_softc *sc;
	struct mps_command *cm = NULL;
	uint8_t flags;
	u_int pq;
	MPI2_DIAG_RELEASE_REPLY *rel_rep;
	mps_fw_diagnostic_buffer_t *pBuffer;

	sc = (struct mps_softc *)data;

	pq = sc->replypostindex;
	mps_dprint(sc, MPS_TRACE,
	    "%s sc %p starting with replypostindex %u\n", 
	    __func__, sc, sc->replypostindex);

	for ( ;; ) {
		cm = NULL;
		desc = &sc->post_queue[sc->replypostindex];
		flags = desc->Default.ReplyFlags &
		    MPI2_RPY_DESCRIPT_FLAGS_TYPE_MASK;
		if ((flags == MPI2_RPY_DESCRIPT_FLAGS_UNUSED)
		 || (le32toh(desc->Words.High) == 0xffffffff))
			break;

		/* increment the replypostindex now, so that event handlers
		 * and cm completion handlers which decide to do a diag
		 * reset can zero it without it getting incremented again
		 * afterwards, and we break out of this loop on the next
		 * iteration since the reply post queue has been cleared to
		 * 0xFF and all descriptors look unused (which they are).
		 */
		if (++sc->replypostindex >= sc->pqdepth)
			sc->replypostindex = 0;

		switch (flags) {
		case MPI2_RPY_DESCRIPT_FLAGS_SCSI_IO_SUCCESS:
			cm = &sc->commands[le16toh(desc->SCSIIOSuccess.SMID)];
			cm->cm_reply = NULL;
			break;
		case MPI2_RPY_DESCRIPT_FLAGS_ADDRESS_REPLY:
		{
			uint32_t baddr;
			uint8_t *reply;

			/*
			 * Re-compose the reply address from the address
			 * sent back from the chip.  The ReplyFrameAddress
			 * is the lower 32 bits of the physical address of
			 * particular reply frame.  Convert that address to
			 * host format, and then use that to provide the
			 * offset against the virtual address base
			 * (sc->reply_frames).
			 */
			baddr = le32toh(desc->AddressReply.ReplyFrameAddress);
			reply = sc->reply_frames +
				(baddr - ((uint32_t)sc->reply_busaddr));
			/*
			 * Make sure the reply we got back is in a valid
			 * range.  If not, go ahead and panic here, since
			 * we'll probably panic as soon as we deference the
			 * reply pointer anyway.
			 */
			if ((reply < sc->reply_frames)
			 || (reply > (sc->reply_frames +
			     (sc->fqdepth * sc->facts->ReplyFrameSize * 4)))) {
				printf("%s: WARNING: reply %p out of range!\n",
				       __func__, reply);
				printf("%s: reply_frames %p, fqdepth %d, "
				       "frame size %d\n", __func__,
				       sc->reply_frames, sc->fqdepth,
				       sc->facts->ReplyFrameSize * 4);
				printf("%s: baddr %#x,\n", __func__, baddr);
				/* LSI-TODO. See Linux Code. Need Gracefull exit*/
				panic("Reply address out of range");
			}
			if (le16toh(desc->AddressReply.SMID) == 0) {
				if (((MPI2_DEFAULT_REPLY *)reply)->Function ==
				    MPI2_FUNCTION_DIAG_BUFFER_POST) {
					/*
					 * If SMID is 0 for Diag Buffer Post,
					 * this implies that the reply is due to
					 * a release function with a status that
					 * the buffer has been released.  Set
					 * the buffer flags accordingly.
					 */
					rel_rep =
					    (MPI2_DIAG_RELEASE_REPLY *)reply;
					if (le16toh(rel_rep->IOCStatus) ==
					    MPI2_IOCSTATUS_DIAGNOSTIC_RELEASED)
					    {
						pBuffer =
						    &sc->fw_diag_buffer_list[
						    rel_rep->BufferType];
						pBuffer->valid_data = TRUE;
						pBuffer->owned_by_firmware =
						    FALSE;
						pBuffer->immediate = FALSE;
					}
				} else
					mps_dispatch_event(sc, baddr,
					    (MPI2_EVENT_NOTIFICATION_REPLY *)
					    reply);
			} else {
				cm = &sc->commands[le16toh(desc->AddressReply.SMID)];
				cm->cm_reply = reply;
				cm->cm_reply_data =
				    le32toh(desc->AddressReply.ReplyFrameAddress);
			}
			break;
		}
		case MPI2_RPY_DESCRIPT_FLAGS_TARGETASSIST_SUCCESS:
		case MPI2_RPY_DESCRIPT_FLAGS_TARGET_COMMAND_BUFFER:
		case MPI2_RPY_DESCRIPT_FLAGS_RAID_ACCELERATOR_SUCCESS:
		default:
			/* Unhandled */
			device_printf(sc->mps_dev, "Unhandled reply 0x%x\n",
			    desc->Default.ReplyFlags);
			cm = NULL;
			break;
		}
		

		if (cm != NULL) {
			// Print Error reply frame
			if (cm->cm_reply)
				mps_display_reply_info(sc,cm->cm_reply);
			mps_complete_command(cm);
		}

		desc->Words.Low = 0xffffffff;
		desc->Words.High = 0xffffffff;
	}

	if (pq != sc->replypostindex) {
		mps_dprint(sc, MPS_TRACE,
		    "%s sc %p writing postindex %d\n",
		    __func__, sc, sc->replypostindex);
		mps_regwrite(sc, MPI2_REPLY_POST_HOST_INDEX_OFFSET, sc->replypostindex);
	}

	return;
}

static void
mps_dispatch_event(struct mps_softc *sc, uintptr_t data,
    MPI2_EVENT_NOTIFICATION_REPLY *reply)
{
	struct mps_event_handle *eh;
	int event, handled = 0;

	event = le16toh(reply->Event);
	TAILQ_FOREACH(eh, &sc->event_list, eh_list) {
		if (isset(eh->mask, event)) {
			eh->callback(sc, data, reply);
			handled++;
		}
	}

	if (handled == 0)
		device_printf(sc->mps_dev, "Unhandled event 0x%x\n", le16toh(event));

	/*
	 * This is the only place that the event/reply should be freed.
	 * Anything wanting to hold onto the event data should have
	 * already copied it into their own storage.
	 */
	mps_free_reply(sc, data);
}

static void
mps_reregister_events_complete(struct mps_softc *sc, struct mps_command *cm)
{
	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	if (cm->cm_reply)
		mps_print_event(sc,
			(MPI2_EVENT_NOTIFICATION_REPLY *)cm->cm_reply);

	mps_free_command(sc, cm);

	/* next, send a port enable */
	mpssas_startup(sc);
}

/*
 * For both register_events and update_events, the caller supplies a bitmap
 * of events that it _wants_.  These functions then turn that into a bitmask
 * suitable for the controller.
 */
int
mps_register_events(struct mps_softc *sc, u32 *mask,
    mps_evt_callback_t *cb, void *data, struct mps_event_handle **handle)
{
	struct mps_event_handle *eh;
	int error = 0;

	eh = malloc(sizeof(struct mps_event_handle), M_MPT2, M_WAITOK|M_ZERO);
	if(!eh) {
		device_printf(sc->mps_dev, "Cannot allocate memory %s %d\n",
		 __func__, __LINE__);
		return (ENOMEM);
	}
	eh->callback = cb;
	eh->data = data;
	TAILQ_INSERT_TAIL(&sc->event_list, eh, eh_list);
	if (mask != NULL)
		error = mps_update_events(sc, eh, mask);
	*handle = eh;

	return (error);
}

int
mps_update_events(struct mps_softc *sc, struct mps_event_handle *handle,
    u32 *mask)
{
	MPI2_EVENT_NOTIFICATION_REQUEST *evtreq;
	MPI2_EVENT_NOTIFICATION_REPLY *reply;
	struct mps_command *cm;
	int error, i;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	if ((mask != NULL) && (handle != NULL))
		bcopy(mask, &handle->mask[0], sizeof(u32) * 
				MPI2_EVENT_NOTIFY_EVENTMASK_WORDS);
    
	for (i = 0; i < MPI2_EVENT_NOTIFY_EVENTMASK_WORDS; i++)
		sc->event_mask[i] = -1;

	for (i = 0; i < MPI2_EVENT_NOTIFY_EVENTMASK_WORDS; i++)
		sc->event_mask[i] &= ~handle->mask[i];


	if ((cm = mps_alloc_command(sc)) == NULL)
		return (EBUSY);
	evtreq = (MPI2_EVENT_NOTIFICATION_REQUEST *)cm->cm_req;
	evtreq->Function = MPI2_FUNCTION_EVENT_NOTIFICATION;
	evtreq->MsgFlags = 0;
	evtreq->SASBroadcastPrimitiveMasks = 0;
#ifdef MPS_DEBUG_ALL_EVENTS
	{
		u_char fullmask[16];
		memset(fullmask, 0x00, 16);
		bcopy(fullmask, &evtreq->EventMasks[0], sizeof(u32) * 
				MPI2_EVENT_NOTIFY_EVENTMASK_WORDS);
	}
#else
        for (i = 0; i < MPI2_EVENT_NOTIFY_EVENTMASK_WORDS; i++)
                evtreq->EventMasks[i] =
                    htole32(sc->event_mask[i]);
#endif
	cm->cm_desc.Default.RequestFlags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	cm->cm_data = NULL;

	error = mps_request_polled(sc, cm);
	reply = (MPI2_EVENT_NOTIFICATION_REPLY *)cm->cm_reply;
	if ((reply == NULL) ||
	    (reply->IOCStatus & MPI2_IOCSTATUS_MASK) != MPI2_IOCSTATUS_SUCCESS)
		error = ENXIO;
	mps_print_event(sc, reply);
	mps_dprint(sc, MPS_TRACE, "%s finished error %d\n", __func__, error);

	mps_free_command(sc, cm);
	return (error);
}

static int
mps_reregister_events(struct mps_softc *sc)
{
	MPI2_EVENT_NOTIFICATION_REQUEST *evtreq;
	struct mps_command *cm;
	struct mps_event_handle *eh;
	int error, i;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	/* first, reregister events */

    for (i = 0; i < MPI2_EVENT_NOTIFY_EVENTMASK_WORDS; i++)
		sc->event_mask[i] = -1;

	TAILQ_FOREACH(eh, &sc->event_list, eh_list) {
		for (i = 0; i < MPI2_EVENT_NOTIFY_EVENTMASK_WORDS; i++)
			sc->event_mask[i] &= ~eh->mask[i];
	}

	if ((cm = mps_alloc_command(sc)) == NULL)
		return (EBUSY);
	evtreq = (MPI2_EVENT_NOTIFICATION_REQUEST *)cm->cm_req;
	evtreq->Function = MPI2_FUNCTION_EVENT_NOTIFICATION;
	evtreq->MsgFlags = 0;
	evtreq->SASBroadcastPrimitiveMasks = 0;
#ifdef MPS_DEBUG_ALL_EVENTS
	{
		u_char fullmask[16];
		memset(fullmask, 0x00, 16);
		bcopy(fullmask, &evtreq->EventMasks[0], sizeof(u32) *
			MPI2_EVENT_NOTIFY_EVENTMASK_WORDS);
	}
#else
        for (i = 0; i < MPI2_EVENT_NOTIFY_EVENTMASK_WORDS; i++)
                evtreq->EventMasks[i] =
                    htole32(sc->event_mask[i]);
#endif
	cm->cm_desc.Default.RequestFlags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	cm->cm_data = NULL;
	cm->cm_complete = mps_reregister_events_complete;

	error = mps_map_command(sc, cm);

	mps_dprint(sc, MPS_TRACE, "%s finished with error %d\n", __func__, error);
	return (error);
}

void
mps_deregister_events(struct mps_softc *sc, struct mps_event_handle *handle)
{

	TAILQ_REMOVE(&sc->event_list, handle, eh_list);
	free(handle, M_MPT2);
}

/*
 * Add a chain element as the next SGE for the specified command.
 * Reset cm_sge and cm_sgesize to indicate all the available space.
 */
static int
mps_add_chain(struct mps_command *cm)
{
	MPI2_SGE_CHAIN32 *sgc;
	struct mps_chain *chain;
	int space;

	if (cm->cm_sglsize < MPS_SGC_SIZE)
		panic("MPS: Need SGE Error Code\n");

	chain = mps_alloc_chain(cm->cm_sc);
	if (chain == NULL)
		return (ENOBUFS);

	space = (int)cm->cm_sc->facts->IOCRequestFrameSize * 4;

	/*
	 * Note: a double-linked list is used to make it easier to
	 * walk for debugging.
	 */
	TAILQ_INSERT_TAIL(&cm->cm_chain_list, chain, chain_link);

	sgc = (MPI2_SGE_CHAIN32 *)&cm->cm_sge->MpiChain;
	sgc->Length = htole16(space);
	sgc->NextChainOffset = 0;
	/* TODO Looks like bug in Setting sgc->Flags. 
	 *	sgc->Flags = ( MPI2_SGE_FLAGS_CHAIN_ELEMENT | MPI2_SGE_FLAGS_64_BIT_ADDRESSING |
	 *	            MPI2_SGE_FLAGS_SYSTEM_ADDRESS) << MPI2_SGE_FLAGS_SHIFT
	 *	This is fine.. because we are not using simple element. In case of 
	 *	MPI2_SGE_CHAIN32, we have seperate Length and Flags feild.
 	 */
	sgc->Flags = MPI2_SGE_FLAGS_CHAIN_ELEMENT;
	sgc->Address = htole32(chain->chain_busaddr);

	cm->cm_sge = (MPI2_SGE_IO_UNION *)&chain->chain->MpiSimple;
	cm->cm_sglsize = space;
	return (0);
}

/*
 * Add one scatter-gather element (chain, simple, transaction context)
 * to the scatter-gather list for a command.  Maintain cm_sglsize and
 * cm_sge as the remaining size and pointer to the next SGE to fill
 * in, respectively.
 */
int
mps_push_sge(struct mps_command *cm, void *sgep, size_t len, int segsleft)
{
	MPI2_SGE_TRANSACTION_UNION *tc = sgep;
	MPI2_SGE_SIMPLE64 *sge = sgep;
	int error, type;
	uint32_t saved_buf_len, saved_address_low, saved_address_high;
	u32 sge_flags;

	type = (tc->Flags & MPI2_SGE_FLAGS_ELEMENT_MASK);

#ifdef INVARIANTS
	switch (type) {
	case MPI2_SGE_FLAGS_TRANSACTION_ELEMENT: {
		if (len != tc->DetailsLength + 4)
			panic("TC %p length %u or %zu?", tc,
			    tc->DetailsLength + 4, len);
		}
		break;
	case MPI2_SGE_FLAGS_CHAIN_ELEMENT:
		/* Driver only uses 32-bit chain elements */
		if (len != MPS_SGC_SIZE)
			panic("CHAIN %p length %u or %zu?", sgep,
			    MPS_SGC_SIZE, len);
		break;
	case MPI2_SGE_FLAGS_SIMPLE_ELEMENT:
		/* Driver only uses 64-bit SGE simple elements */
		sge = sgep;
		if (len != MPS_SGE64_SIZE)
			panic("SGE simple %p length %u or %zu?", sge,
			    MPS_SGE64_SIZE, len);
		if (((sge->FlagsLength >> MPI2_SGE_FLAGS_SHIFT) &
		    MPI2_SGE_FLAGS_ADDRESS_SIZE) == 0)
			panic("SGE simple %p flags %02x not marked 64-bit?",
			    sge, sge->FlagsLength >> MPI2_SGE_FLAGS_SHIFT);

		break;
	default:
		panic("Unexpected SGE %p, flags %02x", tc, tc->Flags);
	}
#endif

	/*
	 * case 1: 1 more segment, enough room for it
	 * case 2: 2 more segments, enough room for both
	 * case 3: >=2 more segments, only enough room for 1 and a chain
	 * case 4: >=1 more segment, enough room for only a chain
	 * case 5: >=1 more segment, no room for anything (error)
         */

	/*
	 * There should be room for at least a chain element, or this
	 * code is buggy.  Case (5).
	 */
	if (cm->cm_sglsize < MPS_SGC_SIZE)
		panic("MPS: Need SGE Error Code\n");

	if (segsleft >= 2 &&
	    cm->cm_sglsize < len + MPS_SGC_SIZE + MPS_SGE64_SIZE) {
		/*
		 * There are 2 or more segments left to add, and only
		 * enough room for 1 and a chain.  Case (3).
		 *
		 * Mark as last element in this chain if necessary.
		 */
		if (type == MPI2_SGE_FLAGS_SIMPLE_ELEMENT) {
			sge->FlagsLength |=
				(MPI2_SGE_FLAGS_LAST_ELEMENT << MPI2_SGE_FLAGS_SHIFT);
		}

		/*
		 * Add the item then a chain.  Do the chain now,
		 * rather than on the next iteration, to simplify
		 * understanding the code.
		 */
		cm->cm_sglsize -= len;
		/* Endian Safe code */
		sge_flags = sge->FlagsLength;
		sge->FlagsLength = htole32(sge_flags);
		sge->Address.High = htole32(sge->Address.High);	
		sge->Address.Low = 	htole32(sge->Address.Low);
		bcopy(sgep, cm->cm_sge, len);
		cm->cm_sge = (MPI2_SGE_IO_UNION *)((uintptr_t)cm->cm_sge + len);
		return (mps_add_chain(cm));
	}

	if (segsleft >= 1 && cm->cm_sglsize < len + MPS_SGC_SIZE) {
		/*
		 * 1 or more segment, enough room for only a chain.
		 * Hope the previous element wasn't a Simple entry
		 * that needed to be marked with
		 * MPI2_SGE_FLAGS_LAST_ELEMENT.  Case (4).
		 */
		if ((error = mps_add_chain(cm)) != 0)
			return (error);
	}

#ifdef INVARIANTS
	/* Case 1: 1 more segment, enough room for it. */
	if (segsleft == 1 && cm->cm_sglsize < len)
		panic("1 seg left and no room? %u versus %zu",
		    cm->cm_sglsize, len);

	/* Case 2: 2 more segments, enough room for both */
	if (segsleft == 2 && cm->cm_sglsize < len + MPS_SGE64_SIZE)
		panic("2 segs left and no room? %u versus %zu",
		    cm->cm_sglsize, len);
#endif

	if (segsleft == 1 && type == MPI2_SGE_FLAGS_SIMPLE_ELEMENT) {
		/*
		 * If this is a bi-directional request, need to account for that
		 * here.  Save the pre-filled sge values.  These will be used
		 * either for the 2nd SGL or for a single direction SGL.  If
		 * cm_out_len is non-zero, this is a bi-directional request, so
		 * fill in the OUT SGL first, then the IN SGL, otherwise just
		 * fill in the IN SGL.  Note that at this time, when filling in
		 * 2 SGL's for a bi-directional request, they both use the same
		 * DMA buffer (same cm command).
		 */
		saved_buf_len = sge->FlagsLength & 0x00FFFFFF;
		saved_address_low = sge->Address.Low;
		saved_address_high = sge->Address.High;
		if (cm->cm_out_len) {
			sge->FlagsLength = cm->cm_out_len |
			    ((uint32_t)(MPI2_SGE_FLAGS_SIMPLE_ELEMENT |
			    MPI2_SGE_FLAGS_END_OF_BUFFER |
			    MPI2_SGE_FLAGS_HOST_TO_IOC |
			    MPI2_SGE_FLAGS_64_BIT_ADDRESSING) <<
			    MPI2_SGE_FLAGS_SHIFT);
			cm->cm_sglsize -= len;
			/* Endian Safe code */
			sge_flags = sge->FlagsLength;
			sge->FlagsLength = htole32(sge_flags);
			sge->Address.High = htole32(sge->Address.High);	
			sge->Address.Low = 	htole32(sge->Address.Low);
			bcopy(sgep, cm->cm_sge, len);
			cm->cm_sge = (MPI2_SGE_IO_UNION *)((uintptr_t)cm->cm_sge
			    + len);
		}
		sge->FlagsLength = saved_buf_len |
		    ((uint32_t)(MPI2_SGE_FLAGS_SIMPLE_ELEMENT |
		    MPI2_SGE_FLAGS_END_OF_BUFFER |
		    MPI2_SGE_FLAGS_LAST_ELEMENT |
		    MPI2_SGE_FLAGS_END_OF_LIST |
		    MPI2_SGE_FLAGS_64_BIT_ADDRESSING) <<
		    MPI2_SGE_FLAGS_SHIFT);
		if (cm->cm_flags & MPS_CM_FLAGS_DATAIN) {
			sge->FlagsLength |=
			    ((uint32_t)(MPI2_SGE_FLAGS_IOC_TO_HOST) <<
			    MPI2_SGE_FLAGS_SHIFT);
		} else {
			sge->FlagsLength |=
			    ((uint32_t)(MPI2_SGE_FLAGS_HOST_TO_IOC) <<
			    MPI2_SGE_FLAGS_SHIFT);
		}
		sge->Address.Low = saved_address_low;
		sge->Address.High = saved_address_high;
	}

	cm->cm_sglsize -= len;
	/* Endian Safe code */
	sge_flags = sge->FlagsLength;
	sge->FlagsLength = htole32(sge_flags);
	sge->Address.High = htole32(sge->Address.High);	
	sge->Address.Low = 	htole32(sge->Address.Low);
	bcopy(sgep, cm->cm_sge, len);
	cm->cm_sge = (MPI2_SGE_IO_UNION *)((uintptr_t)cm->cm_sge + len);
	return (0);
}

/*
 * Add one dma segment to the scatter-gather list for a command.
 */
int
mps_add_dmaseg(struct mps_command *cm, vm_paddr_t pa, size_t len, u_int flags,
    int segsleft)
{
	MPI2_SGE_SIMPLE64 sge;

	/*
	 * This driver always uses 64-bit address elements for simplicity.
	 */
	flags |= MPI2_SGE_FLAGS_SIMPLE_ELEMENT |
	    MPI2_SGE_FLAGS_64_BIT_ADDRESSING;
	/* Set Endian safe macro in mps_push_sge */
	sge.FlagsLength = len | (flags << MPI2_SGE_FLAGS_SHIFT);
	mps_from_u64(pa, &sge.Address);

	return (mps_push_sge(cm, &sge, sizeof sge, segsleft));
}

static void
mps_data_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct mps_softc *sc;
	struct mps_command *cm;
	u_int i, dir, sflags;

	cm = (struct mps_command *)arg;
	sc = cm->cm_sc;

	/*
	 * In this case, just print out a warning and let the chip tell the
	 * user they did the wrong thing.
	 */
	if ((cm->cm_max_segs != 0) && (nsegs > cm->cm_max_segs)) {
		mps_printf(sc, "%s: warning: busdma returned %d segments, "
			   "more than the %d allowed\n", __func__, nsegs,
			   cm->cm_max_segs);
	}

	/*
	 * Set up DMA direction flags.  Bi-directional requests are also handled
	 * here.  In that case, both direction flags will be set.
	 */
	sflags = 0;
	if (cm->cm_flags & MPS_CM_FLAGS_SMP_PASS) {
		/*
		 * We have to add a special case for SMP passthrough, there
		 * is no easy way to generically handle it.  The first
		 * S/G element is used for the command (therefore the
		 * direction bit needs to be set).  The second one is used
		 * for the reply.  We'll leave it to the caller to make
		 * sure we only have two buffers.
		 */
		/*
		 * Even though the busdma man page says it doesn't make
		 * sense to have both direction flags, it does in this case.
		 * We have one s/g element being accessed in each direction.
		 */
		dir = BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD;

		/*
		 * Set the direction flag on the first buffer in the SMP
		 * passthrough request.  We'll clear it for the second one.
		 */
		sflags |= MPI2_SGE_FLAGS_DIRECTION |
			  MPI2_SGE_FLAGS_END_OF_BUFFER;
	} else if (cm->cm_flags & MPS_CM_FLAGS_DATAOUT) {
		sflags |= MPI2_SGE_FLAGS_HOST_TO_IOC;
		dir = BUS_DMASYNC_PREWRITE;
	} else
		dir = BUS_DMASYNC_PREREAD;

	for (i = 0; i < nsegs; i++) {
		if ((cm->cm_flags & MPS_CM_FLAGS_SMP_PASS) && (i != 0)) {
			sflags &= ~MPI2_SGE_FLAGS_DIRECTION;
		}
		error = mps_add_dmaseg(cm, segs[i].ds_addr, segs[i].ds_len,
		    sflags, nsegs - i);
		if (error != 0) {
			/* Resource shortage, roll back! */
			mps_dprint(sc, MPS_INFO, "out of chain frames\n");
			cm->cm_flags |= MPS_CM_FLAGS_CHAIN_FAILED;
			mps_complete_command(cm);
			return;
		}
	}

	bus_dmamap_sync(sc->buffer_dmat, cm->cm_dmamap, dir);
	mps_enqueue_request(sc, cm);

	return;
}

static void
mps_data_cb2(void *arg, bus_dma_segment_t *segs, int nsegs, bus_size_t mapsize,
	     int error)
{
	mps_data_cb(arg, segs, nsegs, error);
}

/*
 * This is the routine to enqueue commands ansynchronously.
 * Note that the only error path here is from bus_dmamap_load(), which can
 * return EINPROGRESS if it is waiting for resources.  Other than this, it's
 * assumed that if you have a command in-hand, then you have enough credits
 * to use it.
 */
int
mps_map_command(struct mps_softc *sc, struct mps_command *cm)
{
	MPI2_SGE_SIMPLE32 *sge;
	int error = 0;

	if (cm->cm_flags & MPS_CM_FLAGS_USE_UIO) {
		error = bus_dmamap_load_uio(sc->buffer_dmat, cm->cm_dmamap,
		    &cm->cm_uio, mps_data_cb2, cm, 0);
	} else if ((cm->cm_data != NULL) && (cm->cm_length != 0)) {
		error = bus_dmamap_load(sc->buffer_dmat, cm->cm_dmamap,
		    cm->cm_data, cm->cm_length, mps_data_cb, cm, 0);
	} else {
		/* Add a zero-length element as needed */
		if (cm->cm_sge != NULL) {
			sge = (MPI2_SGE_SIMPLE32 *)cm->cm_sge;
			sge->FlagsLength = htole32((MPI2_SGE_FLAGS_LAST_ELEMENT |
			    MPI2_SGE_FLAGS_END_OF_BUFFER |
			    MPI2_SGE_FLAGS_END_OF_LIST |
			    MPI2_SGE_FLAGS_SIMPLE_ELEMENT) <<
			    MPI2_SGE_FLAGS_SHIFT);
			sge->Address = 0;
		}
		mps_enqueue_request(sc, cm);	
	}

	return (error);
}

/*
 * This is the routine to enqueue commands synchronously.  An error of
 * EINPROGRESS from mps_map_command() is ignored since the command will
 * be executed and enqueued automatically.  Other errors come from msleep().
 */
int
mps_wait_command(struct mps_softc *sc, struct mps_command *cm, int timeout)
{
	int error, rc;

	mtx_assert(&sc->mps_mtx, MA_OWNED);
	
	if(sc->mps_flags & MPS_FLAGS_DIAGRESET) 
		return  EBUSY;

	cm->cm_complete = NULL;
	cm->cm_flags |= MPS_CM_FLAGS_WAKEUP;
	error = mps_map_command(sc, cm);
	if ((error != 0) && (error != EINPROGRESS))
		return (error);
	error = msleep(cm, &sc->mps_mtx, 0, "mpswait", timeout*hz);
	if (error == EWOULDBLOCK) {
		mps_dprint(sc, MPS_FAULT, "Calling Reinit from %s\n", __func__);
		rc = mps_reinit(sc);
		mps_dprint(sc, MPS_FAULT, "Reinit %s\n", 
				(rc == 0) ? "success" : "failed");
		error = ETIMEDOUT;
	}
	return (error);
}

/*
 * This is the routine to enqueue a command synchonously and poll for
 * completion.  Its use should be rare.
 */
int
mps_request_polled(struct mps_softc *sc, struct mps_command *cm)
{
	int error, timeout = 0, rc;

	error = 0;

	cm->cm_flags |= MPS_CM_FLAGS_POLLED;
	cm->cm_complete = NULL;
	mps_map_command(sc, cm);

	while ((cm->cm_flags & MPS_CM_FLAGS_COMPLETE) == 0) {
		mps_intr_locked(sc);

		DELAY(50 * 1000);
		if (timeout++ > 1000) {
			mps_dprint(sc, MPS_FAULT, "polling failed\n");
			error = ETIMEDOUT;
			break;
		}
	}
	
	if (error) {
		mps_dprint(sc, MPS_FAULT, "Calling Reinit from %s\n", __func__);
		rc = mps_reinit(sc);
		mps_dprint(sc, MPS_FAULT, "Reinit %s\n", 
				(rc == 0) ? "success" : "failed");
	}

	return (error);
}

/*
 * The MPT driver had a verbose interface for config pages.  In this driver,
 * reduce it to much simplier terms, similar to the Linux driver.
 */
int
mps_read_config_page(struct mps_softc *sc, struct mps_config_params *params)
{
	MPI2_CONFIG_REQUEST *req;
	struct mps_command *cm;
	int error;

	if (sc->mps_flags & MPS_FLAGS_BUSY) {
		return (EBUSY);
	}

	cm = mps_alloc_command(sc);
	if (cm == NULL) {
		return (EBUSY);
	}

	req = (MPI2_CONFIG_REQUEST *)cm->cm_req;
	req->Function = MPI2_FUNCTION_CONFIG;
	req->Action = params->action;
	req->SGLFlags = 0;
	req->ChainOffset = 0;
	req->PageAddress = params->page_address;
	if (params->hdr.Ext.ExtPageType != 0) {
		MPI2_CONFIG_EXTENDED_PAGE_HEADER *hdr;

		hdr = &params->hdr.Ext;
		req->ExtPageType = hdr->ExtPageType;
		req->ExtPageLength = hdr->ExtPageLength;
		req->Header.PageType = MPI2_CONFIG_PAGETYPE_EXTENDED;
		req->Header.PageLength = 0; /* Must be set to zero */
		req->Header.PageNumber = hdr->PageNumber;
		req->Header.PageVersion = hdr->PageVersion;
	} else {
		MPI2_CONFIG_PAGE_HEADER *hdr;

		hdr = &params->hdr.Struct;
		req->Header.PageType = hdr->PageType;
		req->Header.PageNumber = hdr->PageNumber;
		req->Header.PageLength = hdr->PageLength;
		req->Header.PageVersion = hdr->PageVersion;
	}

	cm->cm_data = params->buffer;
	cm->cm_length = params->length;
	cm->cm_sge = &req->PageBufferSGE;
	cm->cm_sglsize = sizeof(MPI2_SGE_IO_UNION);
	cm->cm_flags = MPS_CM_FLAGS_SGE_SIMPLE | MPS_CM_FLAGS_DATAIN;
	cm->cm_desc.Default.RequestFlags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;

	cm->cm_complete_data = params;
	if (params->callback != NULL) {
		cm->cm_complete = mps_config_complete;
		return (mps_map_command(sc, cm));
	} else {
		error = mps_wait_command(sc, cm, 0);
		if (error) {
			mps_dprint(sc, MPS_FAULT,
			    "Error %d reading config page\n", error);
			mps_free_command(sc, cm);
			return (error);
		}
		mps_config_complete(sc, cm);
	}

	return (0);
}

int
mps_write_config_page(struct mps_softc *sc, struct mps_config_params *params)
{
	return (EINVAL);
}

static void
mps_config_complete(struct mps_softc *sc, struct mps_command *cm)
{
	MPI2_CONFIG_REPLY *reply;
	struct mps_config_params *params;

	params = cm->cm_complete_data;

	if (cm->cm_data != NULL) {
		bus_dmamap_sync(sc->buffer_dmat, cm->cm_dmamap,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->buffer_dmat, cm->cm_dmamap);
	}

	/*
	 * XXX KDM need to do more error recovery?  This results in the
	 * device in question not getting probed.
	 */
	if ((cm->cm_flags & MPS_CM_FLAGS_ERROR_MASK) != 0) {
		params->status = MPI2_IOCSTATUS_BUSY;
		goto done;
	}

	reply = (MPI2_CONFIG_REPLY *)cm->cm_reply;
	if (reply == NULL) {
		params->status = MPI2_IOCSTATUS_BUSY;
		goto done;
	}
	params->status = reply->IOCStatus;
	if (params->hdr.Ext.ExtPageType != 0) {
		params->hdr.Ext.ExtPageType = reply->ExtPageType;
		params->hdr.Ext.ExtPageLength = reply->ExtPageLength;
	} else {
		params->hdr.Struct.PageType = reply->Header.PageType;
		params->hdr.Struct.PageNumber = reply->Header.PageNumber;
		params->hdr.Struct.PageLength = reply->Header.PageLength;
		params->hdr.Struct.PageVersion = reply->Header.PageVersion;
	}

done:
	mps_free_command(sc, cm);
	if (params->callback != NULL)
		params->callback(sc, params);

	return;
}
