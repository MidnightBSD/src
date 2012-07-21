/*
 * Copyright (c) 2007 Cisco Systems, Inc. All rights reserved.
 * Copyright (c) 2007, 2008 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/log2.h>
#include <linux/netdevice.h>

#include <rdma/ib_cache.h>
#include <rdma/ib_pack.h>
#include <rdma/ib_addr.h>

#include <linux/mlx4/qp.h>
#include <linux/io.h>

#include "mlx4_ib.h"
#include "user.h"

enum {
	MLX4_IB_ACK_REQ_FREQ	= 8,
};

enum {
	MLX4_IB_DEFAULT_SCHED_QUEUE	= 0x83,
	MLX4_IB_DEFAULT_QP0_SCHED_QUEUE	= 0x3f,
	MLX4_IB_LINK_TYPE_IB		= 0,
	MLX4_IB_LINK_TYPE_ETH		= 1,
};

enum {
	/*
	 * Largest possible UD header: send with GRH and immediate data.
	 * 4 bytes added to accommodate for eth header instead of lrh
	 */
	MLX4_IB_UD_HEADER_SIZE		= 76,
	MLX4_IB_MAX_RAW_ETY_HDR_SIZE	= 12
};

enum {
	MLX4_IBOE_ETHERTYPE = 0x8915
};

struct mlx4_ib_xrc_reg_entry {
	struct list_head list;
	void *context;
};

struct mlx4_ib_sqp {
	struct mlx4_ib_qp	qp;
	int			pkey_index;
	u32			qkey;
	u32			send_psn;
	struct ib_ud_header	ud_header;
	u8			header_buf[MLX4_IB_UD_HEADER_SIZE];
};

enum {
	MLX4_IB_MIN_SQ_STRIDE = 6
};

static const __be32 mlx4_ib_opcode[] = {
	[IB_WR_SEND]				= cpu_to_be32(MLX4_OPCODE_SEND),
	[IB_WR_LSO]				= cpu_to_be32(MLX4_OPCODE_LSO),
	[IB_WR_SEND_WITH_IMM]			= cpu_to_be32(MLX4_OPCODE_SEND_IMM),
	[IB_WR_RDMA_WRITE]			= cpu_to_be32(MLX4_OPCODE_RDMA_WRITE),
	[IB_WR_RDMA_WRITE_WITH_IMM]		= cpu_to_be32(MLX4_OPCODE_RDMA_WRITE_IMM),
	[IB_WR_RDMA_READ]			= cpu_to_be32(MLX4_OPCODE_RDMA_READ),
	[IB_WR_ATOMIC_CMP_AND_SWP]		= cpu_to_be32(MLX4_OPCODE_ATOMIC_CS),
	[IB_WR_ATOMIC_FETCH_AND_ADD]		= cpu_to_be32(MLX4_OPCODE_ATOMIC_FA),
	[IB_WR_SEND_WITH_INV]			= cpu_to_be32(MLX4_OPCODE_SEND_INVAL),
	[IB_WR_LOCAL_INV]			= cpu_to_be32(MLX4_OPCODE_LOCAL_INVAL),
	[IB_WR_FAST_REG_MR]			= cpu_to_be32(MLX4_OPCODE_FMR),
	[IB_WR_MASKED_ATOMIC_CMP_AND_SWP]	= cpu_to_be32(MLX4_OPCODE_MASKED_ATOMIC_CS),
	[IB_WR_MASKED_ATOMIC_FETCH_AND_ADD]	= cpu_to_be32(MLX4_OPCODE_MASKED_ATOMIC_FA),
};

#ifndef wc_wmb
	#if defined(__i386__)
		#define wc_wmb() __asm volatile("lock; addl $0,0(%%esp) " ::: "memory")
	#elif defined(__x86_64__)
		#define wc_wmb() __asm volatile("sfence" ::: "memory")
	#elif defined(__ia64__)
		#define wc_wmb() __asm volatile("fwb" ::: "memory")
	#else
		#define wc_wmb() wmb()
	#endif
#endif


static struct mlx4_ib_sqp *to_msqp(struct mlx4_ib_qp *mqp)
{
	return container_of(mqp, struct mlx4_ib_sqp, qp);
}

static int is_sqp(struct mlx4_ib_dev *dev, struct mlx4_ib_qp *qp)
{
	return qp->mqp.qpn >= dev->dev->caps.sqp_start &&
		qp->mqp.qpn <= dev->dev->caps.sqp_start + 3;
}

static int is_qp0(struct mlx4_ib_dev *dev, struct mlx4_ib_qp *qp)
{
	return qp->mqp.qpn >= dev->dev->caps.sqp_start &&
		qp->mqp.qpn <= dev->dev->caps.sqp_start + 1;
}

static void *get_wqe(struct mlx4_ib_qp *qp, int offset)
{
	return mlx4_buf_offset(&qp->buf, offset);
}

static void *get_recv_wqe(struct mlx4_ib_qp *qp, int n)
{
	return get_wqe(qp, qp->rq.offset + (n << qp->rq.wqe_shift));
}

static void *get_send_wqe(struct mlx4_ib_qp *qp, int n)
{
	return get_wqe(qp, qp->sq.offset + (n << qp->sq.wqe_shift));
}

/*
 * Stamp a SQ WQE so that it is invalid if prefetched by marking the
 * first four bytes of every 64 byte chunk with
 *     0x7FFFFFF | (invalid_ownership_value << 31).
 *
 * When the max work request size is less than or equal to the WQE
 * basic block size, as an optimization, we can stamp all WQEs with
 * 0xffffffff, and skip the very first chunk of each WQE.
 */
static void stamp_send_wqe(struct mlx4_ib_qp *qp, int n, int size)
{
	__be32 *wqe;
	int i;
	int s;
	int ind;
	void *buf;
	__be32 stamp;
	struct mlx4_wqe_ctrl_seg *ctrl;

	if (qp->sq_max_wqes_per_wr > 1) {
		s = roundup(size, 1U << qp->sq.wqe_shift);
		for (i = 0; i < s; i += 64) {
			ind = (i >> qp->sq.wqe_shift) + n;
			stamp = ind & qp->sq.wqe_cnt ? cpu_to_be32(0x7fffffff) :
						       cpu_to_be32(0xffffffff);
			buf = get_send_wqe(qp, ind & (qp->sq.wqe_cnt - 1));
			wqe = buf + (i & ((1 << qp->sq.wqe_shift) - 1));
			*wqe = stamp;
		}
	} else {
		ctrl = buf = get_send_wqe(qp, n & (qp->sq.wqe_cnt - 1));
		s = (ctrl->fence_size & 0x3f) << 4;
		for (i = 64; i < s; i += 64) {
			wqe = buf + i;
			*wqe = cpu_to_be32(0xffffffff);
		}
	}
}

static void post_nop_wqe(struct mlx4_ib_qp *qp, int n, int size)
{
	struct mlx4_wqe_ctrl_seg *ctrl;
	struct mlx4_wqe_inline_seg *inl;
	void *wqe;
	int s;

	ctrl = wqe = get_send_wqe(qp, n & (qp->sq.wqe_cnt - 1));
	s = sizeof(struct mlx4_wqe_ctrl_seg);

	if (qp->ibqp.qp_type == IB_QPT_UD) {
		struct mlx4_wqe_datagram_seg *dgram = wqe + sizeof *ctrl;
		struct mlx4_av *av = (struct mlx4_av *)dgram->av;
		memset(dgram, 0, sizeof *dgram);
		av->port_pd = cpu_to_be32((qp->port << 24) | to_mpd(qp->ibqp.pd)->pdn);
		s += sizeof(struct mlx4_wqe_datagram_seg);
	}

	/* Pad the remainder of the WQE with an inline data segment. */
	if (size > s) {
		inl = wqe + s;
		inl->byte_count = cpu_to_be32(1 << 31 | (size - s - sizeof *inl));
	}
	ctrl->srcrb_flags = 0;
	ctrl->fence_size = size / 16;
	/*
	 * Make sure descriptor is fully written before setting ownership bit
	 * (because HW can start executing as soon as we do).
	 */
	wmb();

	ctrl->owner_opcode = cpu_to_be32(MLX4_OPCODE_NOP | MLX4_WQE_CTRL_NEC) |
		(n & qp->sq.wqe_cnt ? cpu_to_be32(1 << 31) : 0);

	stamp_send_wqe(qp, n + qp->sq_spare_wqes, size);
}

/* Post NOP WQE to prevent wrap-around in the middle of WR */
static inline unsigned pad_wraparound(struct mlx4_ib_qp *qp, int ind)
{
	unsigned s = qp->sq.wqe_cnt - (ind & (qp->sq.wqe_cnt - 1));
	if (unlikely(s < qp->sq_max_wqes_per_wr)) {
		post_nop_wqe(qp, ind, s << qp->sq.wqe_shift);
		ind += s;
	}
	return ind;
}

static void mlx4_ib_qp_event(struct mlx4_qp *qp, enum mlx4_event type)
{
	struct ib_event event;
	struct mlx4_ib_qp *mqp = to_mibqp(qp);
	struct ib_qp *ibqp = &mqp->ibqp;
	struct mlx4_ib_xrc_reg_entry *ctx_entry;
	unsigned long flags;

	if (type == MLX4_EVENT_TYPE_PATH_MIG)
		to_mibqp(qp)->port = to_mibqp(qp)->alt_port;

	if (ibqp->event_handler) {
		event.device     = ibqp->device;
		switch (type) {
		case MLX4_EVENT_TYPE_PATH_MIG:
			event.event = IB_EVENT_PATH_MIG;
			break;
		case MLX4_EVENT_TYPE_COMM_EST:
			event.event = IB_EVENT_COMM_EST;
			break;
		case MLX4_EVENT_TYPE_SQ_DRAINED:
			event.event = IB_EVENT_SQ_DRAINED;
			break;
		case MLX4_EVENT_TYPE_SRQ_QP_LAST_WQE:
			event.event = IB_EVENT_QP_LAST_WQE_REACHED;
			break;
		case MLX4_EVENT_TYPE_WQ_CATAS_ERROR:
			event.event = IB_EVENT_QP_FATAL;
			break;
		case MLX4_EVENT_TYPE_PATH_MIG_FAILED:
			event.event = IB_EVENT_PATH_MIG_ERR;
			break;
		case MLX4_EVENT_TYPE_WQ_INVAL_REQ_ERROR:
			event.event = IB_EVENT_QP_REQ_ERR;
			break;
		case MLX4_EVENT_TYPE_WQ_ACCESS_ERROR:
			event.event = IB_EVENT_QP_ACCESS_ERR;
			break;
		default:
			printk(KERN_WARNING "mlx4_ib: Unexpected event type %d "
			       "on QP %06x\n", type, qp->qpn);
			return;
		}

		if (unlikely(ibqp->qp_type == IB_QPT_XRC &&
			     mqp->flags & MLX4_IB_XRC_RCV)) {
			event.event |= IB_XRC_QP_EVENT_FLAG;
			event.element.xrc_qp_num = ibqp->qp_num;
			spin_lock_irqsave(&mqp->xrc_reg_list_lock, flags);
			list_for_each_entry(ctx_entry, &mqp->xrc_reg_list, list)
				ibqp->event_handler(&event, ctx_entry->context);
			spin_unlock_irqrestore(&mqp->xrc_reg_list_lock, flags);
			return;
		}
		event.element.qp = ibqp;
		ibqp->event_handler(&event, ibqp->qp_context);
	}
}

static int send_wqe_overhead(enum ib_qp_type type, u32 flags)
{
	/*
	 * UD WQEs must have a datagram segment.
	 * RC and UC WQEs might have a remote address segment.
	 * MLX WQEs need two extra inline data segments (for the UD
	 * header and space for the ICRC).
	 */
	switch (type) {
	case IB_QPT_UD:
		return sizeof (struct mlx4_wqe_ctrl_seg) +
			sizeof (struct mlx4_wqe_datagram_seg) +
			((flags & MLX4_IB_QP_LSO) ? 128 : 0);
	case IB_QPT_UC:
		return sizeof (struct mlx4_wqe_ctrl_seg) +
			sizeof (struct mlx4_wqe_raddr_seg);
	case IB_QPT_XRC:
	case IB_QPT_RC:
		return sizeof (struct mlx4_wqe_ctrl_seg) +
			sizeof (struct mlx4_wqe_atomic_seg) +
			sizeof (struct mlx4_wqe_raddr_seg);
	case IB_QPT_SMI:
	case IB_QPT_GSI:
		return sizeof (struct mlx4_wqe_ctrl_seg) +
			ALIGN(MLX4_IB_UD_HEADER_SIZE +
			      DIV_ROUND_UP(MLX4_IB_UD_HEADER_SIZE,
					   MLX4_INLINE_ALIGN) *
			      sizeof (struct mlx4_wqe_inline_seg),
			      sizeof (struct mlx4_wqe_data_seg)) +
			ALIGN(4 +
			      sizeof (struct mlx4_wqe_inline_seg),
			      sizeof (struct mlx4_wqe_data_seg));
	case IB_QPT_RAW_ETY:
		return sizeof(struct mlx4_wqe_ctrl_seg) +
			ALIGN(MLX4_IB_MAX_RAW_ETY_HDR_SIZE +
			      sizeof(struct mlx4_wqe_inline_seg),
			      sizeof(struct mlx4_wqe_data_seg));

	default:
		return sizeof (struct mlx4_wqe_ctrl_seg);
	}
}

static int set_rq_size(struct mlx4_ib_dev *dev, struct ib_qp_cap *cap,
		       int is_user, int has_srq_or_is_xrc, struct mlx4_ib_qp *qp)
{
	/* Sanity check RQ size before proceeding */
	if (cap->max_recv_wr > dev->dev->caps.max_wqes - MLX4_IB_SQ_MAX_SPARE ||
	    cap->max_recv_sge >
		min(dev->dev->caps.max_sq_sg, dev->dev->caps.max_rq_sg)) {
		mlx4_ib_dbg("Requested RQ size (sge or wr) too large");
		return -EINVAL;
	}

	if (has_srq_or_is_xrc) {
		/* QPs attached to an SRQ should have no RQ */
		if (cap->max_recv_wr) {
			mlx4_ib_dbg("non-zero RQ size for QP using SRQ");
			return -EINVAL;
		}

		qp->rq.wqe_cnt = qp->rq.max_gs = 0;
	} else {
		/* HW requires >= 1 RQ entry with >= 1 gather entry */
		if (is_user && (!cap->max_recv_wr || !cap->max_recv_sge)) {
			mlx4_ib_dbg("user QP RQ has 0 wr's or 0 sge's "
				    "(wr: 0x%x, sge: 0x%x)", cap->max_recv_wr,
				    cap->max_recv_sge);
			return -EINVAL;
		}

		qp->rq.wqe_cnt	 = roundup_pow_of_two(max(1U, cap->max_recv_wr));
		qp->rq.max_gs	 = roundup_pow_of_two(max(1U, cap->max_recv_sge));
		qp->rq.wqe_shift = ilog2(qp->rq.max_gs * sizeof (struct mlx4_wqe_data_seg));
	}

	/* leave userspace return values as they were, so as not to break ABI */
	if (is_user) {
		cap->max_recv_wr  = qp->rq.max_post = qp->rq.wqe_cnt;
		cap->max_recv_sge = qp->rq.max_gs;
	} else {
		cap->max_recv_wr  = qp->rq.max_post =
			min(dev->dev->caps.max_wqes - MLX4_IB_SQ_MAX_SPARE, qp->rq.wqe_cnt);
		cap->max_recv_sge = min(qp->rq.max_gs,
					min(dev->dev->caps.max_sq_sg,
				    	dev->dev->caps.max_rq_sg));
	}
	/* We don't support inline sends for kernel QPs (yet) */


	return 0;
}

static int set_kernel_sq_size(struct mlx4_ib_dev *dev, struct ib_qp_cap *cap,
			      enum ib_qp_type type, struct mlx4_ib_qp *qp)
{
	int s;

	/* Sanity check SQ size before proceeding */
	if (cap->max_send_wr	 > (dev->dev->caps.max_wqes - MLX4_IB_SQ_MAX_SPARE) ||
	    cap->max_send_sge	 >
		min(dev->dev->caps.max_sq_sg, dev->dev->caps.max_rq_sg) ||
	    cap->max_inline_data + send_wqe_overhead(type, qp->flags) +
	    sizeof (struct mlx4_wqe_inline_seg) > dev->dev->caps.max_sq_desc_sz) {
		mlx4_ib_dbg("Requested SQ resources exceed device maxima");
		return -EINVAL;
	}

	/*
	 * For MLX transport we need 2 extra S/G entries:
	 * one for the header and one for the checksum at the end
	 */
	if ((type == IB_QPT_SMI || type == IB_QPT_GSI) &&
	    cap->max_send_sge + 2 > dev->dev->caps.max_sq_sg) {
		mlx4_ib_dbg("No space for SQP hdr/csum sge's");
		return -EINVAL;
	}

	if (type == IB_QPT_RAW_ETY &&
	    cap->max_send_sge + 1 > dev->dev->caps.max_sq_sg) {
		mlx4_ib_dbg("No space for RAW ETY hdr");
		return -EINVAL;
	}

	s = max(cap->max_send_sge * sizeof (struct mlx4_wqe_data_seg),
		cap->max_inline_data + sizeof (struct mlx4_wqe_inline_seg)) +
		send_wqe_overhead(type, qp->flags);

	if (s > dev->dev->caps.max_sq_desc_sz)
		return -EINVAL;

	/*
	 * Hermon supports shrinking WQEs, such that a single work
	 * request can include multiple units of 1 << wqe_shift.  This
	 * way, work requests can differ in size, and do not have to
	 * be a power of 2 in size, saving memory and speeding up send
	 * WR posting.  Unfortunately, if we do this then the
	 * wqe_index field in CQEs can't be used to look up the WR ID
	 * anymore, so we do this only if selective signaling is off.
	 *
	 * Further, on 32-bit platforms, we can't use vmap() to make
	 * the QP buffer virtually contigious.  Thus we have to use
	 * constant-sized WRs to make sure a WR is always fully within
	 * a single page-sized chunk.
	 *
	 * Finally, we use NOP work requests to pad the end of the
	 * work queue, to avoid wrap-around in the middle of WR.  We
	 * set NEC bit to avoid getting completions with error for
	 * these NOP WRs, but since NEC is only supported starting
	 * with firmware 2.2.232, we use constant-sized WRs for older
	 * firmware.
	 *
	 * And, since MLX QPs only support SEND, we use constant-sized
	 * WRs in this case.
	 *
	 * We look for the smallest value of wqe_shift such that the
	 * resulting number of wqes does not exceed device
	 * capabilities.
	 *
	 * We set WQE size to at least 64 bytes, this way stamping
	 * invalidates each WQE.
	 */
	if (dev->dev->caps.fw_ver >= MLX4_FW_VER_WQE_CTRL_NEC &&
	    qp->sq_signal_bits && BITS_PER_LONG == 64 &&
	    type != IB_QPT_SMI && type != IB_QPT_GSI && type != IB_QPT_RAW_ETY)
		qp->sq.wqe_shift = ilog2(64);
	else
		qp->sq.wqe_shift = ilog2(roundup_pow_of_two(s));

	for (;;) {
		qp->sq_max_wqes_per_wr = DIV_ROUND_UP(s, 1U << qp->sq.wqe_shift);

		/*
		 * We need to leave 2 KB + 1 WR of headroom in the SQ to
		 * allow HW to prefetch.
		 */
		qp->sq_spare_wqes = (2048 >> qp->sq.wqe_shift) + qp->sq_max_wqes_per_wr;
		qp->sq.wqe_cnt = roundup_pow_of_two(cap->max_send_wr *
						    qp->sq_max_wqes_per_wr +
						    qp->sq_spare_wqes);

		if (qp->sq.wqe_cnt <= dev->dev->caps.max_wqes)
			break;

		if (qp->sq_max_wqes_per_wr <= 1)
			return -EINVAL;

		++qp->sq.wqe_shift;
	}

	qp->sq.max_gs = (min(dev->dev->caps.max_sq_desc_sz,
			     (qp->sq_max_wqes_per_wr << qp->sq.wqe_shift)) -
			 send_wqe_overhead(type, qp->flags)) /
		sizeof (struct mlx4_wqe_data_seg);

	qp->buf_size = (qp->rq.wqe_cnt << qp->rq.wqe_shift) +
		(qp->sq.wqe_cnt << qp->sq.wqe_shift);
	if (qp->rq.wqe_shift > qp->sq.wqe_shift) {
		qp->rq.offset = 0;
		qp->sq.offset = qp->rq.wqe_cnt << qp->rq.wqe_shift;
	} else {
		qp->rq.offset = qp->sq.wqe_cnt << qp->sq.wqe_shift;
		qp->sq.offset = 0;
	}

	cap->max_send_wr  = qp->sq.max_post =
		(qp->sq.wqe_cnt - qp->sq_spare_wqes) / qp->sq_max_wqes_per_wr;
	cap->max_send_sge = min(qp->sq.max_gs,
				min(dev->dev->caps.max_sq_sg,
				    dev->dev->caps.max_rq_sg));
	qp->max_inline_data = cap->max_inline_data;

	return 0;
}

static int set_user_sq_size(struct mlx4_ib_dev *dev,
			    struct mlx4_ib_qp *qp,
			    struct mlx4_ib_create_qp *ucmd)
{
	/* Sanity check SQ size before proceeding */
	if ((1 << ucmd->log_sq_bb_count) > dev->dev->caps.max_wqes	 ||
	    ucmd->log_sq_stride >
		ilog2(roundup_pow_of_two(dev->dev->caps.max_sq_desc_sz)) ||
	    ucmd->log_sq_stride < MLX4_IB_MIN_SQ_STRIDE) {
		mlx4_ib_dbg("Requested max wqes or wqe stride exceeds max");
		return -EINVAL;
	}

	qp->sq.wqe_cnt   = 1 << ucmd->log_sq_bb_count;
	qp->sq.wqe_shift = ucmd->log_sq_stride;

	qp->buf_size = (qp->rq.wqe_cnt << qp->rq.wqe_shift) +
		(qp->sq.wqe_cnt << qp->sq.wqe_shift);

	return 0;
}

static int create_qp_common(struct mlx4_ib_dev *dev, struct ib_pd *pd,
			    struct ib_qp_init_attr *init_attr,
			    struct ib_udata *udata, int sqpn, struct mlx4_ib_qp *qp)
{
	int qpn;
	int err;

	mutex_init(&qp->mutex);
	spin_lock_init(&qp->sq.lock);
	spin_lock_init(&qp->rq.lock);
	spin_lock_init(&qp->xrc_reg_list_lock);
	INIT_LIST_HEAD(&qp->gid_list);

	qp->state	 = IB_QPS_RESET;
	if (init_attr->sq_sig_type == IB_SIGNAL_ALL_WR)
		qp->sq_signal_bits = cpu_to_be32(MLX4_WQE_CTRL_CQ_UPDATE);

	err = set_rq_size(dev, &init_attr->cap, !!pd->uobject,
			  !!init_attr->srq || !!init_attr->xrc_domain , qp);
	if (err)
		goto err;

	if (pd->uobject) {
		struct mlx4_ib_create_qp ucmd;

		if (ib_copy_from_udata(&ucmd, udata, sizeof ucmd)) {
			err = -EFAULT;
			goto err;
		}

		qp->sq_no_prefetch = ucmd.sq_no_prefetch;

		err = set_user_sq_size(dev, qp, &ucmd);
		if (err)
			goto err;

		qp->umem = ib_umem_get(pd->uobject->context, ucmd.buf_addr,
				       qp->buf_size, 0, 0);
		if (IS_ERR(qp->umem)) {
			err = PTR_ERR(qp->umem);
			mlx4_ib_dbg("ib_umem_get error (%d)", err);
			goto err;
		}

		err = mlx4_mtt_init(dev->dev, ib_umem_page_count(qp->umem),
				    ilog2(qp->umem->page_size), &qp->mtt);
		if (err) {
			mlx4_ib_dbg("mlx4_mtt_init error (%d)", err);
			goto err_buf;
		}

		err = mlx4_ib_umem_write_mtt(dev, &qp->mtt, qp->umem);
		if (err) {
			mlx4_ib_dbg("mlx4_ib_umem_write_mtt error (%d)", err);
			goto err_mtt;
		}

		if (!init_attr->srq && init_attr->qp_type != IB_QPT_XRC) {
			err = mlx4_ib_db_map_user(to_mucontext(pd->uobject->context),
						  ucmd.db_addr, &qp->db);
			if (err) {
				mlx4_ib_dbg("mlx4_ib_db_map_user error (%d)", err);
				goto err_mtt;
			}
		}
	} else {
		qp->sq_no_prefetch = 0;

		if (init_attr->create_flags & IB_QP_CREATE_BLOCK_MULTICAST_LOOPBACK)
			qp->flags |= MLX4_IB_QP_BLOCK_MULTICAST_LOOPBACK;

		if (init_attr->create_flags & IB_QP_CREATE_IPOIB_UD_LSO)
			qp->flags |= MLX4_IB_QP_LSO;

		err = set_kernel_sq_size(dev, &init_attr->cap, init_attr->qp_type, qp);
		if (err)
			goto err;

		if (!init_attr->srq && init_attr->qp_type != IB_QPT_XRC) {
			err = mlx4_db_alloc(dev->dev, &qp->db, 0);
			if (err)
				goto err;

			*qp->db.db = 0;
		}

		if (qp->max_inline_data) {
			err = mlx4_bf_alloc(dev->dev, &qp->bf);
			if (err) {
				mlx4_ib_dbg("failed to allocate blue flame register (%d)", err);
				qp->bf.uar = &dev->priv_uar;
			}
		} else
			qp->bf.uar = &dev->priv_uar;

		if (mlx4_buf_alloc(dev->dev, qp->buf_size, PAGE_SIZE * 2, &qp->buf)) {
			err = -ENOMEM;
			goto err_db;
		}

		err = mlx4_mtt_init(dev->dev, qp->buf.npages, qp->buf.page_shift,
				    &qp->mtt);
		if (err) {
			mlx4_ib_dbg("kernel qp mlx4_mtt_init error (%d)", err);
			goto err_buf;
		}

		err = mlx4_buf_write_mtt(dev->dev, &qp->mtt, &qp->buf);
		if (err) {
			mlx4_ib_dbg("mlx4_buf_write_mtt error (%d)", err);
			goto err_mtt;
		}

		qp->sq.wrid  = kmalloc(qp->sq.wqe_cnt * sizeof (u64), GFP_KERNEL);
		qp->rq.wrid  = kmalloc(qp->rq.wqe_cnt * sizeof (u64), GFP_KERNEL);

		if (!qp->sq.wrid || !qp->rq.wrid) {
			err = -ENOMEM;
			goto err_wrid;
		}
	}

	if (sqpn) {
		qpn = sqpn;
	} else {
		err = mlx4_qp_reserve_range(dev->dev, 1, 1, &qpn);
		if (err)
			goto err_wrid;
	}

	err = mlx4_qp_alloc(dev->dev, qpn, &qp->mqp);
	if (err)
		goto err_qpn;

	if (init_attr->qp_type == IB_QPT_XRC)
		qp->mqp.qpn |= (1 << 23);

	/*
	 * Hardware wants QPN written in big-endian order (after
	 * shifting) for send doorbell.  Precompute this value to save
	 * a little bit when posting sends.
	 */
	qp->doorbell_qpn = swab32(qp->mqp.qpn << 8);

	qp->mqp.event = mlx4_ib_qp_event;

	return 0;

err_qpn:
	if (!sqpn)
		mlx4_qp_release_range(dev->dev, qpn, 1);

err_wrid:
	if (pd->uobject) {
		if (!init_attr->srq && init_attr->qp_type != IB_QPT_XRC)
			mlx4_ib_db_unmap_user(to_mucontext(pd->uobject->context),
					      &qp->db);
	} else {
		kfree(qp->sq.wrid);
		kfree(qp->rq.wrid);
	}

err_mtt:
	mlx4_mtt_cleanup(dev->dev, &qp->mtt);

err_buf:
	if (pd->uobject)
		ib_umem_release(qp->umem);
	else
		mlx4_buf_free(dev->dev, qp->buf_size, &qp->buf);

err_db:
	if (!pd->uobject && !init_attr->srq && init_attr->qp_type != IB_QPT_XRC)
		mlx4_db_free(dev->dev, &qp->db);

	if (qp->max_inline_data)
		mlx4_bf_free(dev->dev, &qp->bf);

err:
	return err;
}

static enum mlx4_qp_state to_mlx4_state(enum ib_qp_state state)
{
	switch (state) {
	case IB_QPS_RESET:	return MLX4_QP_STATE_RST;
	case IB_QPS_INIT:	return MLX4_QP_STATE_INIT;
	case IB_QPS_RTR:	return MLX4_QP_STATE_RTR;
	case IB_QPS_RTS:	return MLX4_QP_STATE_RTS;
	case IB_QPS_SQD:	return MLX4_QP_STATE_SQD;
	case IB_QPS_SQE:	return MLX4_QP_STATE_SQER;
	case IB_QPS_ERR:	return MLX4_QP_STATE_ERR;
	default:		return -1;
	}
}

static void mlx4_ib_lock_cqs(struct mlx4_ib_cq *send_cq, struct mlx4_ib_cq *recv_cq)
{
	if (send_cq == recv_cq)
		spin_lock_irq(&send_cq->lock);
	else if (send_cq->mcq.cqn < recv_cq->mcq.cqn) {
		spin_lock_irq(&send_cq->lock);
		spin_lock_nested(&recv_cq->lock, SINGLE_DEPTH_NESTING);
	} else {
		spin_lock_irq(&recv_cq->lock);
		spin_lock_nested(&send_cq->lock, SINGLE_DEPTH_NESTING);
	}
}

static void mlx4_ib_unlock_cqs(struct mlx4_ib_cq *send_cq, struct mlx4_ib_cq *recv_cq)
{
	if (send_cq == recv_cq)
		spin_unlock_irq(&send_cq->lock);
	else if (send_cq->mcq.cqn < recv_cq->mcq.cqn) {
		spin_unlock(&recv_cq->lock);
		spin_unlock_irq(&send_cq->lock);
	} else {
		spin_unlock(&send_cq->lock);
		spin_unlock_irq(&recv_cq->lock);
	}
}

static void del_gid_entries(struct mlx4_ib_qp *qp)
{
	struct gid_entry *ge, *tmp;

	list_for_each_entry_safe(ge, tmp, &qp->gid_list, list) {
		list_del(&ge->list);
		kfree(ge);
	}
}

static void destroy_qp_common(struct mlx4_ib_dev *dev, struct mlx4_ib_qp *qp,
			      int is_user)
{
	struct mlx4_ib_cq *send_cq, *recv_cq;

	if (qp->state != IB_QPS_RESET)
		if (mlx4_qp_modify(dev->dev, NULL, to_mlx4_state(qp->state),
				   MLX4_QP_STATE_RST, NULL, 0, 0, &qp->mqp))
			printk(KERN_WARNING "mlx4_ib: modify QP %06x to RESET failed.\n",
			       qp->mqp.qpn);

	send_cq = to_mcq(qp->ibqp.send_cq);
	recv_cq = to_mcq(qp->ibqp.recv_cq);

	mlx4_ib_lock_cqs(send_cq, recv_cq);

	if (!is_user) {
		__mlx4_ib_cq_clean(recv_cq, qp->mqp.qpn,
				 qp->ibqp.srq ? to_msrq(qp->ibqp.srq): NULL);
		if (send_cq != recv_cq)
			__mlx4_ib_cq_clean(send_cq, qp->mqp.qpn, NULL);
	}

	mlx4_qp_remove(dev->dev, &qp->mqp);

	mlx4_ib_unlock_cqs(send_cq, recv_cq);

	mlx4_qp_free(dev->dev, &qp->mqp);

	if (!is_sqp(dev, qp))
		mlx4_qp_release_range(dev->dev, qp->mqp.qpn, 1);

	mlx4_mtt_cleanup(dev->dev, &qp->mtt);

	if (is_user) {
		if (!qp->ibqp.srq && qp->ibqp.qp_type != IB_QPT_XRC)
			mlx4_ib_db_unmap_user(to_mucontext(qp->ibqp.uobject->context),
					      &qp->db);
		ib_umem_release(qp->umem);
	} else {
		kfree(qp->sq.wrid);
		kfree(qp->rq.wrid);
		mlx4_buf_free(dev->dev, qp->buf_size, &qp->buf);
		if (qp->max_inline_data)
			mlx4_bf_free(dev->dev, &qp->bf);
		if (!qp->ibqp.srq && qp->ibqp.qp_type != IB_QPT_XRC)
			mlx4_db_free(dev->dev, &qp->db);
	}

	del_gid_entries(qp);
}

struct ib_qp *mlx4_ib_create_qp(struct ib_pd *pd,
				struct ib_qp_init_attr *init_attr,
				struct ib_udata *udata)
{
	struct mlx4_ib_dev *dev = to_mdev(pd->device);
	struct mlx4_ib_sqp *sqp;
	struct mlx4_ib_qp *qp;
	int err;

	/*
	 * We only support LSO and multicast loopback blocking, and
	 * only for kernel UD QPs.
	 */
	if (init_attr->create_flags & ~(IB_QP_CREATE_IPOIB_UD_LSO |
					IB_QP_CREATE_BLOCK_MULTICAST_LOOPBACK))
		return ERR_PTR(-EINVAL);

	if (init_attr->create_flags &&
	    (pd->uobject || init_attr->qp_type != IB_QPT_UD))
		return ERR_PTR(-EINVAL);

	switch (init_attr->qp_type) {
	case IB_QPT_XRC:
		if (!(dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_XRC))
			return ERR_PTR(-ENOSYS);
	case IB_QPT_RC:
	case IB_QPT_UC:
	case IB_QPT_UD:
	case IB_QPT_RAW_ETH:
	{
		qp = kzalloc(sizeof *qp, GFP_KERNEL);
		if (!qp)
			return ERR_PTR(-ENOMEM);

		err = create_qp_common(dev, pd, init_attr, udata, 0, qp);
		if (err) {
			kfree(qp);
			return ERR_PTR(err);
		}

		if (init_attr->qp_type == IB_QPT_XRC)
			qp->xrcdn = to_mxrcd(init_attr->xrc_domain)->xrcdn;
		else
			qp->xrcdn = 0;

		qp->ibqp.qp_num = qp->mqp.qpn;

		break;
	}
	case IB_QPT_RAW_ETY:
		if (!(dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_RAW_ETY))
			return ERR_PTR(-ENOSYS);
	case IB_QPT_SMI:
	case IB_QPT_GSI:
	{
		/* Userspace is not allowed to create special QPs: */
		if (pd->uobject) {
			mlx4_ib_dbg("Userspace is not allowed to create special QPs");
			return ERR_PTR(-EINVAL);
		}

		sqp = kzalloc(sizeof *sqp, GFP_KERNEL);
		if (!sqp)
			return ERR_PTR(-ENOMEM);

		qp = &sqp->qp;

		err = create_qp_common(dev, pd, init_attr, udata,
				       dev->dev->caps.sqp_start +
				       (init_attr->qp_type == IB_QPT_RAW_ETY ? 4 :
				       (init_attr->qp_type == IB_QPT_SMI ? 0 : 2)) +
				       init_attr->port_num - 1,
				       qp);
		if (err) {
			kfree(sqp);
			return ERR_PTR(err);
		}

		qp->port	= init_attr->port_num;
		qp->ibqp.qp_num = init_attr->qp_type == IB_QPT_SMI ? 0 : 1;

		break;
	}
	default:
		mlx4_ib_dbg("Invalid QP type requested for create_qp (%d)",
			    init_attr->qp_type);
		return ERR_PTR(-EINVAL);
	}

	return &qp->ibqp;
}

int mlx4_ib_destroy_qp(struct ib_qp *qp)
{
	struct mlx4_ib_dev *dev = to_mdev(qp->device);
	struct mlx4_ib_qp *mqp = to_mqp(qp);

	if (is_qp0(dev, mqp))
		mlx4_CLOSE_PORT(dev->dev, mqp->port);

	destroy_qp_common(dev, mqp, !!qp->pd->uobject);

	if (is_sqp(dev, mqp))
		kfree(to_msqp(mqp));
	else
		kfree(mqp);

	return 0;
}

static int to_mlx4_st(enum ib_qp_type type)
{
	switch (type) {
	case IB_QPT_RC:		return MLX4_QP_ST_RC;
	case IB_QPT_UC:		return MLX4_QP_ST_UC;
	case IB_QPT_UD:		return MLX4_QP_ST_UD;
	case IB_QPT_XRC:	return MLX4_QP_ST_XRC;
	case IB_QPT_RAW_ETY:
	case IB_QPT_SMI:
	case IB_QPT_GSI:
	case IB_QPT_RAW_ETH:	return MLX4_QP_ST_MLX;
	default:		return -1;
	}
}

static __be32 to_mlx4_access_flags(struct mlx4_ib_qp *qp, const struct ib_qp_attr *attr,
				   int attr_mask)
{
	u8 dest_rd_atomic;
	u32 access_flags;
	u32 hw_access_flags = 0;

	if (attr_mask & IB_QP_MAX_DEST_RD_ATOMIC)
		dest_rd_atomic = attr->max_dest_rd_atomic;
	else
		dest_rd_atomic = qp->resp_depth;

	if (attr_mask & IB_QP_ACCESS_FLAGS)
		access_flags = attr->qp_access_flags;
	else
		access_flags = qp->atomic_rd_en;

	if (!dest_rd_atomic)
		access_flags &= IB_ACCESS_REMOTE_WRITE;

	if (access_flags & IB_ACCESS_REMOTE_READ)
		hw_access_flags |= MLX4_QP_BIT_RRE;
	if (access_flags & IB_ACCESS_REMOTE_ATOMIC)
		hw_access_flags |= MLX4_QP_BIT_RAE;
	if (access_flags & IB_ACCESS_REMOTE_WRITE)
		hw_access_flags |= MLX4_QP_BIT_RWE;

	return cpu_to_be32(hw_access_flags);
}

static void store_sqp_attrs(struct mlx4_ib_sqp *sqp, const struct ib_qp_attr *attr,
			    int attr_mask)
{
	if (attr_mask & IB_QP_PKEY_INDEX)
		sqp->pkey_index = attr->pkey_index;
	if (attr_mask & IB_QP_QKEY)
		sqp->qkey = attr->qkey;
	if (attr_mask & IB_QP_SQ_PSN)
		sqp->send_psn = attr->sq_psn;
}

static void mlx4_set_sched(struct mlx4_qp_path *path, u8 port)
{
	path->sched_queue = (path->sched_queue & 0xbf) | ((port - 1) << 6);
}

static int mlx4_set_path(struct mlx4_ib_dev *dev, const struct ib_ah_attr *ah,
			 struct mlx4_qp_path *path, u8 port)
{
	int err;
	int is_eth = rdma_port_get_link_layer(&dev->ib_dev, port) ==
		IB_LINK_LAYER_ETHERNET;
	u8 mac[6];
	int is_mcast;
	u16 vlan_tag;
	int vidx;

	path->grh_mylmc     = ah->src_path_bits & 0x7f;
	path->rlid	    = cpu_to_be16(ah->dlid);
	if (ah->static_rate) {
		path->static_rate = ah->static_rate + MLX4_STAT_RATE_OFFSET;
		while (path->static_rate > IB_RATE_2_5_GBPS + MLX4_STAT_RATE_OFFSET &&
		       !(1 << path->static_rate & dev->dev->caps.stat_rate_support))
			--path->static_rate;
	} else
		path->static_rate = 0;

	if (ah->ah_flags & IB_AH_GRH) {
		if (ah->grh.sgid_index >= dev->dev->caps.gid_table_len[port]) {
			printk(KERN_ERR "sgid_index (%u) too large. max is %d\n",
			       ah->grh.sgid_index, dev->dev->caps.gid_table_len[port] - 1);
			return -1;
		}

		path->grh_mylmc |= 1 << 7;
		path->mgid_index = ah->grh.sgid_index;
		path->hop_limit  = ah->grh.hop_limit;
		path->tclass_flowlabel =
			cpu_to_be32((ah->grh.traffic_class << 20) |
				    (ah->grh.flow_label));
		memcpy(path->rgid, ah->grh.dgid.raw, 16);
	}

	if (is_eth) {
		path->sched_queue = MLX4_IB_DEFAULT_SCHED_QUEUE |
			((port - 1) << 6) | ((ah->sl & 0x7) << 3) | ((ah->sl & 8) >> 1);

		if (!(ah->ah_flags & IB_AH_GRH))
			return -1;

		err = mlx4_ib_resolve_grh(dev, ah, mac, &is_mcast, port);
		if (err)
			return err;

		memcpy(path->dmac, mac, 6);
		path->ackto = MLX4_IB_LINK_TYPE_ETH;
		/* use index 0 into MAC table for IBoE */
		path->grh_mylmc &= 0x80;

		vlan_tag = rdma_get_vlan_id(&dev->iboe.gid_table[port - 1][ah->grh.sgid_index]);
		if (vlan_tag < 0x1000) {
			if (mlx4_find_cached_vlan(dev->dev, port, vlan_tag, &vidx))
				return -ENOENT;

			path->vlan_index = vidx;
			path->fl = 1 << 6;
		}
	} else
		path->sched_queue = MLX4_IB_DEFAULT_SCHED_QUEUE |
			((port - 1) << 6) | ((ah->sl & 0xf) << 2);

	return 0;
}

static void update_mcg_macs(struct mlx4_ib_dev *dev, struct mlx4_ib_qp *qp)
{
	struct gid_entry *ge, *tmp;

	list_for_each_entry_safe(ge, tmp, &qp->gid_list, list) {
		if (!ge->added && mlx4_ib_add_mc(dev, qp, &ge->gid)) {
			ge->added = 1;
			ge->port = qp->port;
		}
	}
}

static int __mlx4_ib_modify_qp(struct ib_qp *ibqp,
			       const struct ib_qp_attr *attr, int attr_mask,
			       enum ib_qp_state cur_state, enum ib_qp_state new_state)
{
	struct mlx4_ib_dev *dev = to_mdev(ibqp->device);
	struct mlx4_ib_qp *qp = to_mqp(ibqp);
	struct mlx4_qp_context *context;
	enum mlx4_qp_optpar optpar = 0;
	int sqd_event;
	int err = -EINVAL;

	context = kzalloc(sizeof *context, GFP_KERNEL);
	if (!context)
		return -ENOMEM;

	context->flags = cpu_to_be32((to_mlx4_state(new_state) << 28) |
				     (to_mlx4_st(ibqp->qp_type) << 16));

	if (!(attr_mask & IB_QP_PATH_MIG_STATE))
		context->flags |= cpu_to_be32(MLX4_QP_PM_MIGRATED << 11);
	else {
		optpar |= MLX4_QP_OPTPAR_PM_STATE;
		switch (attr->path_mig_state) {
		case IB_MIG_MIGRATED:
			context->flags |= cpu_to_be32(MLX4_QP_PM_MIGRATED << 11);
			break;
		case IB_MIG_REARM:
			context->flags |= cpu_to_be32(MLX4_QP_PM_REARM << 11);
			break;
		case IB_MIG_ARMED:
			context->flags |= cpu_to_be32(MLX4_QP_PM_ARMED << 11);
			break;
		}
	}
	if (ibqp->qp_type == IB_QPT_RAW_ETH)
		context->mtu_msgmax = 0xff;
	else if (ibqp->qp_type == IB_QPT_GSI || ibqp->qp_type == IB_QPT_SMI ||
	    ibqp->qp_type == IB_QPT_RAW_ETY)
		context->mtu_msgmax = (IB_MTU_4096 << 5) | 11;
	else if (ibqp->qp_type == IB_QPT_UD) {
		if (qp->flags & MLX4_IB_QP_LSO)
			context->mtu_msgmax = (IB_MTU_4096 << 5) |
					      ilog2(dev->dev->caps.max_gso_sz);
		else
			context->mtu_msgmax = (IB_MTU_4096 << 5) | 12;
	} else if (attr_mask & IB_QP_PATH_MTU) {
		if (attr->path_mtu < IB_MTU_256 || attr->path_mtu > IB_MTU_4096) {
			printk(KERN_ERR "path MTU (%u) is invalid\n",
			       attr->path_mtu);
			goto out;
		}
		context->mtu_msgmax = (attr->path_mtu << 5) |
			ilog2(dev->dev->caps.max_msg_sz);
	}

	if (qp->rq.wqe_cnt)
		context->rq_size_stride = ilog2(qp->rq.wqe_cnt) << 3;
	context->rq_size_stride |= qp->rq.wqe_shift - 4;

	if (qp->sq.wqe_cnt)
		context->sq_size_stride = ilog2(qp->sq.wqe_cnt) << 3;
	context->sq_size_stride |= qp->sq.wqe_shift - 4;

	if (cur_state == IB_QPS_RESET && new_state == IB_QPS_INIT) {
		context->sq_size_stride |= !!qp->sq_no_prefetch << 7;
		if (ibqp->qp_type == IB_QPT_XRC)
			context->xrcd = cpu_to_be32((u32) qp->xrcdn);
	}

	if (qp->ibqp.uobject)
		context->usr_page = cpu_to_be32(to_mucontext(ibqp->uobject->context)->uar.index);
	else
		context->usr_page = cpu_to_be32(qp->bf.uar->index);

	if (attr_mask & IB_QP_DEST_QPN)
		context->remote_qpn = cpu_to_be32(attr->dest_qp_num);

	if (attr_mask & IB_QP_PORT) {
		if (cur_state == IB_QPS_SQD && new_state == IB_QPS_SQD &&
		    !(attr_mask & IB_QP_AV)) {
			mlx4_set_sched(&context->pri_path, attr->port_num);
			optpar |= MLX4_QP_OPTPAR_SCHED_QUEUE;
		}
	}

	if (cur_state == IB_QPS_INIT && new_state == IB_QPS_RTR &&
	    dev->counters[qp->port - 1] != -1) {
		context->pri_path.counter_index = dev->counters[qp->port - 1];
		optpar |= MLX4_QP_OPTPAR_COUNTER_INDEX;
	}

	if (attr_mask & IB_QP_PKEY_INDEX) {
		context->pri_path.pkey_index = attr->pkey_index;
		optpar |= MLX4_QP_OPTPAR_PKEY_INDEX;
	}

	if (attr_mask & IB_QP_AV) {
		if (mlx4_set_path(dev, &attr->ah_attr, &context->pri_path,
				  attr_mask & IB_QP_PORT ? attr->port_num : qp->port)) {
			mlx4_ib_dbg("qpn 0x%x: could not set pri path params",
				    ibqp->qp_num);
			goto out;
		}

		optpar |= (MLX4_QP_OPTPAR_PRIMARY_ADDR_PATH |
			   MLX4_QP_OPTPAR_SCHED_QUEUE);
	}

	if (attr_mask & IB_QP_TIMEOUT) {
		context->pri_path.ackto |= (attr->timeout << 3);
		optpar |= MLX4_QP_OPTPAR_ACK_TIMEOUT;
	}

	if (attr_mask & IB_QP_ALT_PATH) {
		if (attr->alt_port_num == 0 ||
		    attr->alt_port_num > dev->num_ports) {
			mlx4_ib_dbg("qpn 0x%x: invalid alternate port num (%d)",
				    ibqp->qp_num, attr->alt_port_num);
			goto out;
		}

		if (attr->alt_pkey_index >=
		    dev->dev->caps.pkey_table_len[attr->alt_port_num]) {
			mlx4_ib_dbg("qpn 0x%x: invalid alt pkey index (0x%x)",
				    ibqp->qp_num, attr->alt_pkey_index);
			goto out;
		}

		if (mlx4_set_path(dev, &attr->alt_ah_attr, &context->alt_path,
				  attr->alt_port_num)) {
			mlx4_ib_dbg("qpn 0x%x: could not set alt path params",
				    ibqp->qp_num);
			goto out;
		}

		context->alt_path.pkey_index = attr->alt_pkey_index;
		context->alt_path.ackto = attr->alt_timeout << 3;
		optpar |= MLX4_QP_OPTPAR_ALT_ADDR_PATH;
	}

	context->pd	    = cpu_to_be32(to_mpd(ibqp->pd)->pdn);
	context->params1    = cpu_to_be32(MLX4_IB_ACK_REQ_FREQ << 28);

	/* Set "fast registration enabled" for all kernel QPs */
	if (!qp->ibqp.uobject)
		context->params1 |= cpu_to_be32(1 << 11);

	if (attr_mask & IB_QP_RNR_RETRY) {
		context->params1 |= cpu_to_be32(attr->rnr_retry << 13);
		optpar |= MLX4_QP_OPTPAR_RNR_RETRY;
	}

	if (attr_mask & IB_QP_RETRY_CNT) {
		context->params1 |= cpu_to_be32(attr->retry_cnt << 16);
		optpar |= MLX4_QP_OPTPAR_RETRY_COUNT;
	}

	if (attr_mask & IB_QP_MAX_QP_RD_ATOMIC) {
		if (attr->max_rd_atomic)
			context->params1 |=
				cpu_to_be32(fls(attr->max_rd_atomic - 1) << 21);
		optpar |= MLX4_QP_OPTPAR_SRA_MAX;
	}

	if (attr_mask & IB_QP_SQ_PSN)
		context->next_send_psn = cpu_to_be32(attr->sq_psn);

	context->cqn_send = cpu_to_be32(to_mcq(ibqp->send_cq)->mcq.cqn);

	if (attr_mask & IB_QP_MAX_DEST_RD_ATOMIC) {
		if (attr->max_dest_rd_atomic)
			context->params2 |=
				cpu_to_be32(fls(attr->max_dest_rd_atomic - 1) << 21);
		optpar |= MLX4_QP_OPTPAR_RRA_MAX;
	}

	if (attr_mask & (IB_QP_ACCESS_FLAGS | IB_QP_MAX_DEST_RD_ATOMIC)) {
		context->params2 |= to_mlx4_access_flags(qp, attr, attr_mask);
		optpar |= MLX4_QP_OPTPAR_RWE | MLX4_QP_OPTPAR_RRE | MLX4_QP_OPTPAR_RAE;
	}

	if (ibqp->srq)
		context->params2 |= cpu_to_be32(MLX4_QP_BIT_RIC);

	if (attr_mask & IB_QP_MIN_RNR_TIMER) {
		context->rnr_nextrecvpsn |= cpu_to_be32(attr->min_rnr_timer << 24);
		optpar |= MLX4_QP_OPTPAR_RNR_TIMEOUT;
	}
	if (attr_mask & IB_QP_RQ_PSN)
		context->rnr_nextrecvpsn |= cpu_to_be32(attr->rq_psn);

	context->cqn_recv = cpu_to_be32(to_mcq(ibqp->recv_cq)->mcq.cqn);

	if (attr_mask & IB_QP_QKEY) {
		context->qkey = cpu_to_be32(attr->qkey);
		optpar |= MLX4_QP_OPTPAR_Q_KEY;
	}

	if (ibqp->srq)
		context->srqn = cpu_to_be32(1 << 24 | to_msrq(ibqp->srq)->msrq.srqn);

	if (!ibqp->srq && ibqp->qp_type != IB_QPT_XRC &&
	    cur_state == IB_QPS_RESET && new_state == IB_QPS_INIT)
		context->db_rec_addr = cpu_to_be64(qp->db.dma);

	if (cur_state == IB_QPS_INIT &&
	    new_state == IB_QPS_RTR  &&
	    (ibqp->qp_type == IB_QPT_GSI || ibqp->qp_type == IB_QPT_SMI ||
	     ibqp->qp_type == IB_QPT_UD || ibqp->qp_type == IB_QPT_RAW_ETY ||
		ibqp->qp_type == IB_QPT_RAW_ETH)) {
		context->pri_path.sched_queue = (qp->port - 1) << 6;
		if (is_qp0(dev, qp))
			context->pri_path.sched_queue |= MLX4_IB_DEFAULT_QP0_SCHED_QUEUE;
		else
			context->pri_path.sched_queue |= MLX4_IB_DEFAULT_SCHED_QUEUE;
	}

	if (cur_state == IB_QPS_RTS && new_state == IB_QPS_SQD	&&
	    attr_mask & IB_QP_EN_SQD_ASYNC_NOTIFY && attr->en_sqd_async_notify)
		sqd_event = 1;
	else
		sqd_event = 0;

	if (!ibqp->uobject && cur_state == IB_QPS_RESET && new_state == IB_QPS_INIT)
		context->rlkey |= (1 << 4);

	/*
	 * Before passing a kernel QP to the HW, make sure that the
	 * ownership bits of the send queue are set and the SQ
	 * headroom is stamped so that the hardware doesn't start
	 * processing stale work requests.
	 */
	if (!ibqp->uobject && cur_state == IB_QPS_RESET && new_state == IB_QPS_INIT) {
		struct mlx4_wqe_ctrl_seg *ctrl;
		int i;

		for (i = 0; i < qp->sq.wqe_cnt; ++i) {
			ctrl = get_send_wqe(qp, i);
			ctrl->owner_opcode = cpu_to_be32(1 << 31);
			if (qp->sq_max_wqes_per_wr == 1)
				ctrl->fence_size = 1 << (qp->sq.wqe_shift - 4);

			stamp_send_wqe(qp, i, 1 << qp->sq.wqe_shift);
		}
	}

	err = mlx4_qp_modify(dev->dev, &qp->mtt, to_mlx4_state(cur_state),
			     to_mlx4_state(new_state), context, optpar,
			     sqd_event, &qp->mqp);
	if (err)
		goto out;

	qp->state = new_state;

	if (attr_mask & IB_QP_ACCESS_FLAGS)
		qp->atomic_rd_en = attr->qp_access_flags;
	if (attr_mask & IB_QP_MAX_DEST_RD_ATOMIC)
		qp->resp_depth = attr->max_dest_rd_atomic;
	if (attr_mask & IB_QP_PORT) {
		qp->port = attr->port_num;
		update_mcg_macs(dev, qp);
	}
	if (attr_mask & IB_QP_ALT_PATH)
		qp->alt_port = attr->alt_port_num;

	if (is_sqp(dev, qp))
		store_sqp_attrs(to_msqp(qp), attr, attr_mask);

	/*
	 * If we moved QP0 to RTR, bring the IB link up; if we moved
	 * QP0 to RESET or ERROR, bring the link back down.
	 */
	if (is_qp0(dev, qp)) {
		if (cur_state != IB_QPS_RTR && new_state == IB_QPS_RTR)
			if (mlx4_INIT_PORT(dev->dev, qp->port))
				printk(KERN_WARNING "INIT_PORT failed for port %d\n",
				       qp->port);

		if (cur_state != IB_QPS_RESET && cur_state != IB_QPS_ERR &&
		    (new_state == IB_QPS_RESET || new_state == IB_QPS_ERR))
			mlx4_CLOSE_PORT(dev->dev, qp->port);
	}

	/*
	 * If we moved a kernel QP to RESET, clean up all old CQ
	 * entries and reinitialize the QP.
	 */
	if (new_state == IB_QPS_RESET && !ibqp->uobject) {
		mlx4_ib_cq_clean(to_mcq(ibqp->recv_cq), qp->mqp.qpn,
				 ibqp->srq ? to_msrq(ibqp->srq): NULL);
		if (ibqp->send_cq != ibqp->recv_cq)
			mlx4_ib_cq_clean(to_mcq(ibqp->send_cq), qp->mqp.qpn, NULL);

		qp->rq.head = 0;
		qp->rq.tail = 0;
		qp->sq.head = 0;
		qp->sq.tail = 0;
		qp->sq_next_wqe = 0;
		if (!ibqp->srq && ibqp->qp_type != IB_QPT_XRC)
			*qp->db.db  = 0;
	}

out:
	kfree(context);
	return err;
}

int mlx4_ib_modify_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr,
		      int attr_mask, struct ib_udata *udata)
{
	struct mlx4_ib_dev *dev = to_mdev(ibqp->device);
	struct mlx4_ib_qp *qp = to_mqp(ibqp);
	enum ib_qp_state cur_state, new_state;
	int err = -EINVAL;

	mutex_lock(&qp->mutex);

	cur_state = attr_mask & IB_QP_CUR_STATE ? attr->cur_qp_state : qp->state;
	new_state = attr_mask & IB_QP_STATE ? attr->qp_state : cur_state;

	if (!ib_modify_qp_is_ok(cur_state, new_state, ibqp->qp_type, attr_mask)) {
		mlx4_ib_dbg("qpn 0x%x: invalid attribute mask specified "
			    "for transition %d to %d. qp_type %d, attr_mask 0x%x",
			    ibqp->qp_num, cur_state, new_state,
			    ibqp->qp_type, attr_mask);
		goto out;
	}

	if ((attr_mask & IB_QP_PORT) && (ibqp->qp_type != IB_QPT_RAW_ETH) &&
	    (attr->port_num == 0 || attr->port_num > dev->num_ports)) {
		mlx4_ib_dbg("qpn 0x%x: invalid port number (%d) specified "
			    "for transition %d to %d. qp_type %d",
			    ibqp->qp_num, attr->port_num, cur_state,
			    new_state, ibqp->qp_type);
		goto out;
	}

	if ((attr_mask & IB_QP_PORT) && (ibqp->qp_type == IB_QPT_RAW_ETH) &&
		(rdma_port_get_link_layer(&dev->ib_dev, attr->port_num)
				!= IB_LINK_LAYER_ETHERNET)) {
		mlx4_ib_dbg("qpn 0x%x: invalid port (%d) specified (not RDMAoE)"
			    "for transition %d to %d. qp_type %d",
			    ibqp->qp_num, attr->port_num, cur_state,
			    new_state, ibqp->qp_type);
		goto out;
	}

	if (attr_mask & IB_QP_PKEY_INDEX) {
		int p = attr_mask & IB_QP_PORT ? attr->port_num : qp->port;
		if (attr->pkey_index >= dev->dev->caps.pkey_table_len[p]) {
			mlx4_ib_dbg("qpn 0x%x: invalid pkey index (%d) specified "
				    "for transition %d to %d. qp_type %d",
				    ibqp->qp_num, attr->pkey_index, cur_state,
				    new_state, ibqp->qp_type);
			goto out;
		}
	}

	if (attr_mask & IB_QP_MAX_QP_RD_ATOMIC &&
	    attr->max_rd_atomic > dev->dev->caps.max_qp_init_rdma) {
		mlx4_ib_dbg("qpn 0x%x: max_rd_atomic (%d) too large. "
			    "Transition %d to %d. qp_type %d",
			    ibqp->qp_num, attr->max_rd_atomic, cur_state,
			    new_state, ibqp->qp_type);
		goto out;
	}

	if (attr_mask & IB_QP_MAX_DEST_RD_ATOMIC &&
	    attr->max_dest_rd_atomic > dev->dev->caps.max_qp_dest_rdma) {
		mlx4_ib_dbg("qpn 0x%x: max_dest_rd_atomic (%d) too large. "
			    "Transition %d to %d. qp_type %d",
			    ibqp->qp_num, attr->max_dest_rd_atomic, cur_state,
			    new_state, ibqp->qp_type);
		goto out;
	}

	if (cur_state == new_state && cur_state == IB_QPS_RESET) {
		err = 0;
		goto out;
	}

	err = __mlx4_ib_modify_qp(ibqp, attr, attr_mask, cur_state, new_state);

out:
	mutex_unlock(&qp->mutex);
	return err;
}

static int build_raw_ety_header(struct mlx4_ib_sqp *sqp, struct ib_send_wr *wr,
			    void *wqe, unsigned *mlx_seg_len)
{
	int payload = 0;
	int header_size, packet_length;
	struct mlx4_wqe_mlx_seg *mlx = wqe;
	struct mlx4_wqe_inline_seg *inl = wqe + sizeof *mlx;
	u32 *lrh = wqe + sizeof *mlx + sizeof *inl;
	int i;

	/* Only IB_WR_SEND is supported */
	if (wr->opcode != IB_WR_SEND)
		return -EINVAL;

	for (i = 0; i < wr->num_sge; ++i)
		payload += wr->sg_list[i].length;

	header_size = IB_LRH_BYTES + 4; /* LRH + RAW_HEADER (32 bits) */

	/* headers + payload and round up */
	packet_length = (header_size + payload + 3) / 4;

	mlx->flags &= cpu_to_be32(MLX4_WQE_CTRL_CQ_UPDATE);

	mlx->flags |= cpu_to_be32(MLX4_WQE_MLX_ICRC |
				  (wr->wr.raw_ety.lrh->service_level << 8));

	mlx->rlid = wr->wr.raw_ety.lrh->destination_lid;

	wr->wr.raw_ety.lrh->packet_length = cpu_to_be16(packet_length);

	ib_lrh_header_pack(wr->wr.raw_ety.lrh, lrh);
	lrh += IB_LRH_BYTES / 4;	/* LRH size is a dword multiple */
	*lrh = cpu_to_be32(wr->wr.raw_ety.eth_type);

	inl->byte_count = cpu_to_be32(1 << 31 | header_size);

	*mlx_seg_len =
		ALIGN(sizeof(struct mlx4_wqe_inline_seg) + header_size, 16);

	return 0;
}

static int build_mlx_header(struct mlx4_ib_sqp *sqp, struct ib_send_wr *wr,
			    void *wqe, unsigned *mlx_seg_len)
{
	struct ib_device *ib_dev = &to_mdev(sqp->qp.ibqp.device)->ib_dev;
	struct mlx4_wqe_mlx_seg *mlx = wqe;
	struct mlx4_wqe_inline_seg *inl = wqe + sizeof *mlx;
	struct mlx4_ib_ah *ah = to_mah(wr->wr.ud.ah);
	u16 pkey;
	int send_size;
	int header_size;
	int spc;
	int i;
	union ib_gid sgid;
	int is_eth;
	int is_grh;
	int is_vlan = 0;
	int err;
	u16 vlan;

	vlan = 0;
	send_size = 0;
	for (i = 0; i < wr->num_sge; ++i)
		send_size += wr->sg_list[i].length;

	is_eth = rdma_port_get_link_layer(sqp->qp.ibqp.device, sqp->qp.port) == IB_LINK_LAYER_ETHERNET;
	is_grh = mlx4_ib_ah_grh_present(ah);
	err = ib_get_cached_gid(ib_dev, be32_to_cpu(ah->av.ib.port_pd) >> 24,
				ah->av.ib.gid_index, &sgid);
	if (err)
		return err;
	if (is_eth) {
		is_vlan = rdma_get_vlan_id(&sgid) < 0x1000;
		vlan = rdma_get_vlan_id(&sgid);
	}

	ib_ud_header_init(send_size, !is_eth, is_eth, is_vlan, is_grh, 0, &sqp->ud_header);
	if (!is_eth) {
		sqp->ud_header.lrh.service_level =
			be32_to_cpu(ah->av.ib.sl_tclass_flowlabel) >> 28;
		sqp->ud_header.lrh.destination_lid = ah->av.ib.dlid;
		sqp->ud_header.lrh.source_lid = cpu_to_be16(ah->av.ib.g_slid & 0x7f);
	}

	if (is_grh) {
		sqp->ud_header.grh.traffic_class =
			(be32_to_cpu(ah->av.ib.sl_tclass_flowlabel) >> 20) & 0xff;
		sqp->ud_header.grh.flow_label    =
			ah->av.ib.sl_tclass_flowlabel & cpu_to_be32(0xfffff);
		sqp->ud_header.grh.hop_limit     = ah->av.ib.hop_limit;
		ib_get_cached_gid(ib_dev, be32_to_cpu(ah->av.ib.port_pd) >> 24,
				  ah->av.ib.gid_index, &sqp->ud_header.grh.source_gid);
		memcpy(sqp->ud_header.grh.destination_gid.raw,
		       ah->av.ib.dgid, 16);
	}

	mlx->flags &= cpu_to_be32(MLX4_WQE_CTRL_CQ_UPDATE);

	if (!is_eth) {
		mlx->flags |= cpu_to_be32((!sqp->qp.ibqp.qp_num ? MLX4_WQE_MLX_VL15 : 0) |
					  (sqp->ud_header.lrh.destination_lid ==
					   IB_LID_PERMISSIVE ? MLX4_WQE_MLX_SLR : 0) |
					  (sqp->ud_header.lrh.service_level << 8));
		mlx->rlid = sqp->ud_header.lrh.destination_lid;
	}

	switch (wr->opcode) {
	case IB_WR_SEND:
		sqp->ud_header.bth.opcode        = IB_OPCODE_UD_SEND_ONLY;
		sqp->ud_header.immediate_present = 0;
		break;
	case IB_WR_SEND_WITH_IMM:
		sqp->ud_header.bth.opcode        = IB_OPCODE_UD_SEND_ONLY_WITH_IMMEDIATE;
		sqp->ud_header.immediate_present = 1;
		sqp->ud_header.immediate_data    = wr->ex.imm_data;
		break;
	default:
		return -EINVAL;
	}

	if (is_eth) {
		u8 *smac;

		memcpy(sqp->ud_header.eth.dmac_h, ah->av.eth.mac, 6);
#ifdef __linux__
		smac = to_mdev(sqp->qp.ibqp.device)->iboe.netdevs[sqp->qp.port - 1]->dev_addr; /* fixme: cache this value */
#else
		smac = IF_LLADDR(to_mdev(sqp->qp.ibqp.device)->iboe.netdevs[sqp->qp.port - 1]); /* fixme: cache this value */
#endif
		memcpy(sqp->ud_header.eth.smac_h, smac, 6);
		if (!memcmp(sqp->ud_header.eth.smac_h, sqp->ud_header.eth.dmac_h, 6))
			mlx->flags |= cpu_to_be32(MLX4_WQE_CTRL_FORCE_LOOPBACK);
		if (!is_vlan)
			sqp->ud_header.eth.type = cpu_to_be16(MLX4_IBOE_ETHERTYPE);
		else {
			u16 pcp;

			sqp->ud_header.vlan.type = cpu_to_be16(MLX4_IBOE_ETHERTYPE);
			pcp = (be32_to_cpu(ah->av.ib.sl_tclass_flowlabel) >> 27 & 3) << 13;
			sqp->ud_header.vlan.tag = cpu_to_be16(vlan | pcp);
		}
	} else {
		sqp->ud_header.lrh.virtual_lane    = !sqp->qp.ibqp.qp_num ? 15 : 0;
		if (sqp->ud_header.lrh.destination_lid == IB_LID_PERMISSIVE)
			sqp->ud_header.lrh.source_lid = IB_LID_PERMISSIVE;
	}
	sqp->ud_header.bth.solicited_event = !!(wr->send_flags & IB_SEND_SOLICITED);
	if (!sqp->qp.ibqp.qp_num)
		ib_get_cached_pkey(ib_dev, sqp->qp.port, sqp->pkey_index, &pkey);
	else
		ib_get_cached_pkey(ib_dev, sqp->qp.port, wr->wr.ud.pkey_index, &pkey);
	sqp->ud_header.bth.pkey = cpu_to_be16(pkey);
	sqp->ud_header.bth.destination_qpn = cpu_to_be32(wr->wr.ud.remote_qpn);
	sqp->ud_header.bth.psn = cpu_to_be32((sqp->send_psn++) & ((1 << 24) - 1));
	sqp->ud_header.deth.qkey = cpu_to_be32(wr->wr.ud.remote_qkey & 0x80000000 ?
					       sqp->qkey : wr->wr.ud.remote_qkey);
	sqp->ud_header.deth.source_qpn = cpu_to_be32(sqp->qp.ibqp.qp_num);

	header_size = ib_ud_header_pack(&sqp->ud_header, sqp->header_buf);

	if (0) {
		printk(KERN_ERR "built UD header of size %d:\n", header_size);
		for (i = 0; i < header_size / 4; ++i) {
			if (i % 8 == 0)
				printk("  [%02x] ", i * 4);
			printk(" %08x",
			       be32_to_cpu(((__be32 *) sqp->header_buf)[i]));
			if ((i + 1) % 8 == 0)
				printk("\n");
		}
		printk("\n");
	}

	/*
	 * Inline data segments may not cross a 64 byte boundary.  If
	 * our UD header is bigger than the space available up to the
	 * next 64 byte boundary in the WQE, use two inline data
	 * segments to hold the UD header.
	 */
	spc = MLX4_INLINE_ALIGN -
	      ((unsigned long) (inl + 1) & (MLX4_INLINE_ALIGN - 1));
	if (header_size <= spc) {
		inl->byte_count = cpu_to_be32(1 << 31 | header_size);
		memcpy(inl + 1, sqp->header_buf, header_size);
		i = 1;
	} else {
		inl->byte_count = cpu_to_be32(1 << 31 | spc);
		memcpy(inl + 1, sqp->header_buf, spc);

		inl = (void *) (inl + 1) + spc;
		memcpy(inl + 1, sqp->header_buf + spc, header_size - spc);
		/*
		 * Need a barrier here to make sure all the data is
		 * visible before the byte_count field is set.
		 * Otherwise the HCA prefetcher could grab the 64-byte
		 * chunk with this inline segment and get a valid (!=
		 * 0xffffffff) byte count but stale data, and end up
		 * generating a packet with bad headers.
		 *
		 * The first inline segment's byte_count field doesn't
		 * need a barrier, because it comes after a
		 * control/MLX segment and therefore is at an offset
		 * of 16 mod 64.
		 */
		wmb();
		inl->byte_count = cpu_to_be32(1 << 31 | (header_size - spc));
		i = 2;
	}

	*mlx_seg_len =
	ALIGN(i * sizeof (struct mlx4_wqe_inline_seg) + header_size, 16);
	return 0;
}

static int mlx4_wq_overflow(struct mlx4_ib_wq *wq, int nreq, struct ib_cq *ib_cq)
{
	unsigned cur;
	struct mlx4_ib_cq *cq;

	cur = wq->head - wq->tail;
	if (likely(cur + nreq < wq->max_post))
		return 0;

	cq = to_mcq(ib_cq);
	spin_lock(&cq->lock);
	cur = wq->head - wq->tail;
	spin_unlock(&cq->lock);

	return cur + nreq >= wq->max_post;
}

static __be32 convert_access(int acc)
{
	return (acc & IB_ACCESS_REMOTE_ATOMIC ? cpu_to_be32(MLX4_WQE_FMR_PERM_ATOMIC)       : 0) |
	       (acc & IB_ACCESS_REMOTE_WRITE  ? cpu_to_be32(MLX4_WQE_FMR_PERM_REMOTE_WRITE) : 0) |
	       (acc & IB_ACCESS_REMOTE_READ   ? cpu_to_be32(MLX4_WQE_FMR_PERM_REMOTE_READ)  : 0) |
	       (acc & IB_ACCESS_LOCAL_WRITE   ? cpu_to_be32(MLX4_WQE_FMR_PERM_LOCAL_WRITE)  : 0) |
		cpu_to_be32(MLX4_WQE_FMR_PERM_LOCAL_READ);
}

static void set_fmr_seg(struct mlx4_wqe_fmr_seg *fseg, struct ib_send_wr *wr)
{
	struct mlx4_ib_fast_reg_page_list *mfrpl = to_mfrpl(wr->wr.fast_reg.page_list);
	int i;

	for (i = 0; i < wr->wr.fast_reg.page_list_len; ++i)
		mfrpl->mapped_page_list[i] =
			cpu_to_be64(wr->wr.fast_reg.page_list->page_list[i] |
				    MLX4_MTT_FLAG_PRESENT);

	fseg->flags		= convert_access(wr->wr.fast_reg.access_flags);
	fseg->mem_key		= cpu_to_be32(wr->wr.fast_reg.rkey);
	fseg->buf_list		= cpu_to_be64(mfrpl->map);
	fseg->start_addr	= cpu_to_be64(wr->wr.fast_reg.iova_start);
	fseg->reg_len		= cpu_to_be64(wr->wr.fast_reg.length);
	fseg->offset		= 0; /* XXX -- is this just for ZBVA? */
	fseg->page_size		= cpu_to_be32(wr->wr.fast_reg.page_shift);
	fseg->reserved[0]	= 0;
	fseg->reserved[1]	= 0;
}

static void set_local_inv_seg(struct mlx4_wqe_local_inval_seg *iseg, u32 rkey)
{
	iseg->flags	= 0;
	iseg->mem_key	= cpu_to_be32(rkey);
	iseg->guest_id	= 0;
	iseg->pa	= 0;
}

static __always_inline void set_raddr_seg(struct mlx4_wqe_raddr_seg *rseg,
					  u64 remote_addr, u32 rkey)
{
	rseg->raddr    = cpu_to_be64(remote_addr);
	rseg->rkey     = cpu_to_be32(rkey);
	rseg->reserved = 0;
}

static void set_atomic_seg(struct mlx4_wqe_atomic_seg *aseg, struct ib_send_wr *wr)
{
	if (wr->opcode == IB_WR_ATOMIC_CMP_AND_SWP) {
		aseg->swap_add = cpu_to_be64(wr->wr.atomic.swap);
		aseg->compare  = cpu_to_be64(wr->wr.atomic.compare_add);
	} else if (wr->opcode == IB_WR_MASKED_ATOMIC_FETCH_AND_ADD) {
		aseg->swap_add = cpu_to_be64(wr->wr.atomic.compare_add);
		aseg->compare  = cpu_to_be64(wr->wr.atomic.compare_add_mask);
	} else {
		aseg->swap_add = cpu_to_be64(wr->wr.atomic.compare_add);
		aseg->compare  = 0;
	}

}

static void set_masked_atomic_seg(struct mlx4_wqe_masked_atomic_seg *aseg,
				  struct ib_send_wr *wr)
{
	aseg->swap_add		= cpu_to_be64(wr->wr.atomic.swap);
	aseg->swap_add_mask	= cpu_to_be64(wr->wr.atomic.swap_mask);
	aseg->compare		= cpu_to_be64(wr->wr.atomic.compare_add);
	aseg->compare_mask	= cpu_to_be64(wr->wr.atomic.compare_add_mask);
}

static void set_datagram_seg(struct mlx4_wqe_datagram_seg *dseg,
			     struct ib_send_wr *wr, __be16 *vlan)
{
	memcpy(dseg->av, &to_mah(wr->wr.ud.ah)->av, sizeof (struct mlx4_av));
	dseg->dqpn = cpu_to_be32(wr->wr.ud.remote_qpn);
	dseg->qkey = cpu_to_be32(wr->wr.ud.remote_qkey);
	dseg->vlan = to_mah(wr->wr.ud.ah)->av.eth.vlan;
	memcpy(dseg->mac, to_mah(wr->wr.ud.ah)->av.eth.mac, 6);
	*vlan = dseg->vlan;
}

static void set_mlx_icrc_seg(void *dseg)
{
	u32 *t = dseg;
	struct mlx4_wqe_inline_seg *iseg = dseg;

	t[1] = 0;

	/*
	 * Need a barrier here before writing the byte_count field to
	 * make sure that all the data is visible before the
	 * byte_count field is set.  Otherwise, if the segment begins
	 * a new cacheline, the HCA prefetcher could grab the 64-byte
	 * chunk and get a valid (!= * 0xffffffff) byte count but
	 * stale data, and end up sending the wrong data.
	 */
	wmb();

	iseg->byte_count = cpu_to_be32((1 << 31) | 4);
}

static void set_data_seg(struct mlx4_wqe_data_seg *dseg, struct ib_sge *sg)
{
	dseg->lkey       = cpu_to_be32(sg->lkey);
	dseg->addr       = cpu_to_be64(sg->addr);

	/*
	 * Need a barrier here before writing the byte_count field to
	 * make sure that all the data is visible before the
	 * byte_count field is set.  Otherwise, if the segment begins
	 * a new cacheline, the HCA prefetcher could grab the 64-byte
	 * chunk and get a valid (!= * 0xffffffff) byte count but
	 * stale data, and end up sending the wrong data.
	 */
	wmb();

	dseg->byte_count = cpu_to_be32(sg->length);
}

static void __set_data_seg(struct mlx4_wqe_data_seg *dseg, struct ib_sge *sg)
{
	dseg->byte_count = cpu_to_be32(sg->length);
	dseg->lkey       = cpu_to_be32(sg->lkey);
	dseg->addr       = cpu_to_be64(sg->addr);
}

static int build_lso_seg(struct mlx4_wqe_lso_seg *wqe, struct ib_send_wr *wr,
			 struct mlx4_ib_qp *qp, unsigned *lso_seg_len,
			 __be32 *lso_hdr_sz, int *blh)
{
	unsigned halign = ALIGN(sizeof *wqe + wr->wr.ud.hlen, 16);

	*blh = unlikely(halign > 64) ? 1 : 0;

	if (unlikely(!(qp->flags & MLX4_IB_QP_LSO) &&
		     wr->num_sge > qp->sq.max_gs - (halign >> 4)))
		return -EINVAL;

	memcpy(wqe->header, wr->wr.ud.header, wr->wr.ud.hlen);

	*lso_hdr_sz  = cpu_to_be32((wr->wr.ud.mss - wr->wr.ud.hlen) << 16 |
				   wr->wr.ud.hlen);
	*lso_seg_len = halign;
	return 0;
}

static __be32 send_ieth(struct ib_send_wr *wr)
{
	switch (wr->opcode) {
	case IB_WR_SEND_WITH_IMM:
	case IB_WR_RDMA_WRITE_WITH_IMM:
		return wr->ex.imm_data;

	case IB_WR_SEND_WITH_INV:
		return cpu_to_be32(wr->ex.invalidate_rkey);

	default:
		return 0;
	}
}

static int lay_inline_data(struct mlx4_ib_qp *qp, struct ib_send_wr *wr,
			   void *wqe, int *sz)
{
	struct mlx4_wqe_inline_seg *seg;
	void *addr;
	int len, seg_len;
	int num_seg;
	int off, to_copy;
	int i;
	int inl = 0;

	seg = wqe;
	wqe += sizeof *seg;
	off = ((unsigned long)wqe) & (unsigned long)(MLX4_INLINE_ALIGN - 1);
	num_seg = 0;
	seg_len = 0;

	for (i = 0; i < wr->num_sge; ++i) {
		addr = (void *) (unsigned long)(wr->sg_list[i].addr);
		len  = wr->sg_list[i].length;
		inl += len;

		if (inl > qp->max_inline_data) {
			inl = 0;
			return -1;
		}

		while (len >= MLX4_INLINE_ALIGN - off) {
			to_copy = MLX4_INLINE_ALIGN - off;
			memcpy(wqe, addr, to_copy);
			len -= to_copy;
			wqe += to_copy;
			addr += to_copy;
			seg_len += to_copy;
			wmb(); /* see comment below */
			seg->byte_count = htonl(MLX4_INLINE_SEG | seg_len);
			seg_len = 0;
			seg = wqe;
			wqe += sizeof *seg;
			off = sizeof *seg;
			++num_seg;
		}

		memcpy(wqe, addr, len);
		wqe += len;
		seg_len += len;
		off += len;
	}

	if (seg_len) {
		++num_seg;
		/*
		 * Need a barrier here to make sure
		 * all the data is visible before the
		 * byte_count field is set.  Otherwise
		 * the HCA prefetcher could grab the
		 * 64-byte chunk with this inline
		 * segment and get a valid (!=
		 * 0xffffffff) byte count but stale
		 * data, and end up sending the wrong
		 * data.
		 */
		wmb();
		seg->byte_count = htonl(MLX4_INLINE_SEG | seg_len);
	}

	*sz = (inl + num_seg * sizeof *seg + 15) / 16;

	return 0;
}

/*
 * Avoid using memcpy() to copy to BlueFlame page, since memcpy()
 * implementations may use move-string-buffer assembler instructions,
 * which do not guarantee order of copying.
 */
static void mlx4_bf_copy(unsigned long *dst, unsigned long *src, unsigned bytecnt)
{
	__iowrite64_copy(dst, src, bytecnt / 8);
}

int mlx4_ib_post_send(struct ib_qp *ibqp, struct ib_send_wr *wr,
		      struct ib_send_wr **bad_wr)
{
	struct mlx4_ib_qp *qp = to_mqp(ibqp);
	void *wqe;
	struct mlx4_wqe_ctrl_seg *ctrl;
	struct mlx4_wqe_data_seg *dseg;
	unsigned long flags;
	int nreq;
	int err = 0;
	unsigned ind;
	int uninitialized_var(stamp);
	int uninitialized_var(size);
	unsigned uninitialized_var(seglen);
	__be32 dummy;
	__be32 *lso_wqe;
	__be32 uninitialized_var(lso_hdr_sz);
	int i;
	int blh = 0;
	__be16 vlan = 0;
	int inl = 0;

	ctrl = NULL;
	spin_lock_irqsave(&qp->sq.lock, flags);

	ind = qp->sq_next_wqe;

	for (nreq = 0; wr; ++nreq, wr = wr->next) {
		lso_wqe = &dummy;

		if (mlx4_wq_overflow(&qp->sq, nreq, qp->ibqp.send_cq)) {
			mlx4_ib_dbg("QP 0x%x: WQE overflow", ibqp->qp_num);
			err = -ENOMEM;
			*bad_wr = wr;
			goto out;
		}

		if (unlikely(wr->num_sge > qp->sq.max_gs)) {
			mlx4_ib_dbg("QP 0x%x: too many sg entries (%d)",
				    ibqp->qp_num, wr->num_sge);
			err = -EINVAL;
			*bad_wr = wr;
			goto out;
		}

		ctrl = wqe = get_send_wqe(qp, ind & (qp->sq.wqe_cnt - 1));
		*((u32 *) (&ctrl->vlan_tag)) = 0;
		qp->sq.wrid[(qp->sq.head + nreq) & (qp->sq.wqe_cnt - 1)] = wr->wr_id;

		ctrl->srcrb_flags =
			(wr->send_flags & IB_SEND_SIGNALED ?
			 cpu_to_be32(MLX4_WQE_CTRL_CQ_UPDATE) : 0) |
			(wr->send_flags & IB_SEND_SOLICITED ?
			 cpu_to_be32(MLX4_WQE_CTRL_SOLICITED) : 0) |
			((wr->send_flags & IB_SEND_IP_CSUM) ?
			 cpu_to_be32(MLX4_WQE_CTRL_IP_CSUM |
				     MLX4_WQE_CTRL_TCP_UDP_CSUM) : 0) |
			qp->sq_signal_bits;

		ctrl->imm = send_ieth(wr);

		wqe += sizeof *ctrl;
		size = sizeof *ctrl / 16;

		switch (ibqp->qp_type) {
		case IB_QPT_XRC:
			ctrl->srcrb_flags |=
				cpu_to_be32(wr->xrc_remote_srq_num << 8);
			/* fall thru */
		case IB_QPT_RC:
		case IB_QPT_UC:
			switch (wr->opcode) {
			case IB_WR_ATOMIC_CMP_AND_SWP:
			case IB_WR_ATOMIC_FETCH_AND_ADD:
			case IB_WR_MASKED_ATOMIC_FETCH_AND_ADD:
				set_raddr_seg(wqe, wr->wr.atomic.remote_addr,
					      wr->wr.atomic.rkey);
				wqe  += sizeof (struct mlx4_wqe_raddr_seg);

				set_atomic_seg(wqe, wr);
				wqe  += sizeof (struct mlx4_wqe_atomic_seg);

				size += (sizeof (struct mlx4_wqe_raddr_seg) +
					 sizeof (struct mlx4_wqe_atomic_seg)) / 16;

				break;

			case IB_WR_MASKED_ATOMIC_CMP_AND_SWP:
				set_raddr_seg(wqe, wr->wr.atomic.remote_addr,
					      wr->wr.atomic.rkey);
				wqe  += sizeof (struct mlx4_wqe_raddr_seg);

				set_masked_atomic_seg(wqe, wr);
				wqe  += sizeof (struct mlx4_wqe_masked_atomic_seg);

				size += (sizeof (struct mlx4_wqe_raddr_seg) +
					 sizeof (struct mlx4_wqe_masked_atomic_seg)) / 16;

				break;

			case IB_WR_RDMA_READ:
			case IB_WR_RDMA_WRITE:
			case IB_WR_RDMA_WRITE_WITH_IMM:
				set_raddr_seg(wqe, wr->wr.rdma.remote_addr,
					      wr->wr.rdma.rkey);
				wqe  += sizeof (struct mlx4_wqe_raddr_seg);
				size += sizeof (struct mlx4_wqe_raddr_seg) / 16;
				break;

			case IB_WR_LOCAL_INV:
				ctrl->srcrb_flags |=
					cpu_to_be32(MLX4_WQE_CTRL_STRONG_ORDER);
				set_local_inv_seg(wqe, wr->ex.invalidate_rkey);
				wqe  += sizeof (struct mlx4_wqe_local_inval_seg);
				size += sizeof (struct mlx4_wqe_local_inval_seg) / 16;
				break;

			case IB_WR_FAST_REG_MR:
				ctrl->srcrb_flags |=
					cpu_to_be32(MLX4_WQE_CTRL_STRONG_ORDER);
				set_fmr_seg(wqe, wr);
				wqe  += sizeof (struct mlx4_wqe_fmr_seg);
				size += sizeof (struct mlx4_wqe_fmr_seg) / 16;
				break;

			default:
				/* No extra segments required for sends */
				break;
			}
			break;

		case IB_QPT_UD:
			set_datagram_seg(wqe, wr, &vlan);
			wqe  += sizeof (struct mlx4_wqe_datagram_seg);
			size += sizeof (struct mlx4_wqe_datagram_seg) / 16;

			if (wr->opcode == IB_WR_LSO) {
				err = build_lso_seg(wqe, wr, qp, &seglen, &lso_hdr_sz, &blh);
				if (unlikely(err)) {
					*bad_wr = wr;
					goto out;
				}
				lso_wqe = (__be32 *) wqe;
				wqe  += seglen;
				size += seglen / 16;
			}
			break;

		case IB_QPT_SMI:
		case IB_QPT_GSI:
			err = build_mlx_header(to_msqp(qp), wr, ctrl, &seglen);
			if (unlikely(err)) {
				*bad_wr = wr;
				goto out;
			}
			wqe  += seglen;
			size += seglen / 16;
			break;

		case IB_QPT_RAW_ETY:
			err = build_raw_ety_header(to_msqp(qp), wr, ctrl,
						   &seglen);
			if (unlikely(err)) {
				*bad_wr = wr;
				goto out;
			}
			wqe  += seglen;
			size += seglen / 16;
			break;

		default:
			break;
		}

		/*
		 * Write data segments in reverse order, so as to
		 * overwrite cacheline stamp last within each
		 * cacheline.  This avoids issues with WQE
		 * prefetching.
		 */

		dseg = wqe;
		dseg += wr->num_sge - 1;

		/* Add one more inline data segment for ICRC for MLX sends */
		if (unlikely(qp->ibqp.qp_type == IB_QPT_SMI ||
			     qp->ibqp.qp_type == IB_QPT_GSI)) {
			set_mlx_icrc_seg(dseg + 1);
			size += sizeof (struct mlx4_wqe_data_seg) / 16;
		}

		if (wr->send_flags & IB_SEND_INLINE && wr->num_sge) {
			int sz;
			err = lay_inline_data(qp, wr, wqe, &sz);
			if (!err) {
				inl = 1;
				size += sz;
			}
		} else {
			size += wr->num_sge * (sizeof (struct mlx4_wqe_data_seg) / 16);
			for (i = wr->num_sge - 1; i >= 0; --i, --dseg)
				set_data_seg(dseg, wr->sg_list + i);
		}

		/*
		 * Possibly overwrite stamping in cacheline with LSO
		 * segment only after making sure all data segments
		 * are written.
		 */
		wmb();
		*lso_wqe = lso_hdr_sz;

		ctrl->fence_size = (wr->send_flags & IB_SEND_FENCE ?
				    MLX4_WQE_CTRL_FENCE : 0) | size;

		if (vlan) {
			ctrl->ins_vlan = 1 << 6;
			ctrl->vlan_tag = vlan;
		}

		/*
		 * Make sure descriptor is fully written before
		 * setting ownership bit (because HW can start
		 * executing as soon as we do).
		 */
		wmb();

		if (wr->opcode < 0 || wr->opcode >= ARRAY_SIZE(mlx4_ib_opcode)) {
			err = -EINVAL;
			goto out;
		}

		ctrl->owner_opcode = mlx4_ib_opcode[wr->opcode] |
			(ind & qp->sq.wqe_cnt ? cpu_to_be32(1 << 31) : 0) |
			(blh ? cpu_to_be32(1 << 6) : 0);

		stamp = ind + qp->sq_spare_wqes;
		ind += DIV_ROUND_UP(size * 16, 1U << qp->sq.wqe_shift);

		/*
		 * We can improve latency by not stamping the last
		 * send queue WQE until after ringing the doorbell, so
		 * only stamp here if there are still more WQEs to post.
		 *
		 * Same optimization applies to padding with NOP wqe
		 * in case of WQE shrinking (used to prevent wrap-around
		 * in the middle of WR).
		 */
		if (wr->next) {
			stamp_send_wqe(qp, stamp, size * 16);
			ind = pad_wraparound(qp, ind);
		}
	}

out:
	if (nreq == 1 && inl && size > 1 && size < qp->bf.buf_size / 16) {
		ctrl->owner_opcode |= htonl((qp->sq_next_wqe & 0xffff) << 8);
		*(u32 *) (&ctrl->vlan_tag) |= qp->doorbell_qpn;
		/*
		 * Make sure that descriptor is written to memory
		 * before writing to BlueFlame page.
		 */
		wmb();

		++qp->sq.head;

		mlx4_bf_copy(qp->bf.reg + qp->bf.offset, (unsigned long *) ctrl,
			     ALIGN(size * 16, 64));
		wc_wmb();

		qp->bf.offset ^= qp->bf.buf_size;

	} else if (nreq) {
		qp->sq.head += nreq;

		/*
		 * Make sure that descriptors are written before
		 * doorbell record.
		 */
		wmb();

		writel(qp->doorbell_qpn, qp->bf.uar->map + MLX4_SEND_DOORBELL);

		/*
		 * Make sure doorbells don't leak out of SQ spinlock
		 * and reach the HCA out of order.
		 */
		mmiowb();

	}

	if (likely(nreq)) {
		stamp_send_wqe(qp, stamp, size * 16);
		ind = pad_wraparound(qp, ind);
		qp->sq_next_wqe = ind;
	}

	spin_unlock_irqrestore(&qp->sq.lock, flags);

	return err;
}

int mlx4_ib_post_recv(struct ib_qp *ibqp, struct ib_recv_wr *wr,
		      struct ib_recv_wr **bad_wr)
{
	struct mlx4_ib_qp *qp = to_mqp(ibqp);
	struct mlx4_wqe_data_seg *scat;
	unsigned long flags;
	int err = 0;
	int nreq;
	int ind;
	int i;

	spin_lock_irqsave(&qp->rq.lock, flags);

	ind = qp->rq.head & (qp->rq.wqe_cnt - 1);

	for (nreq = 0; wr; ++nreq, wr = wr->next) {
		if (mlx4_wq_overflow(&qp->rq, nreq, qp->ibqp.recv_cq)) {
			mlx4_ib_dbg("QP 0x%x: WQE overflow", ibqp->qp_num);
			err = -ENOMEM;
			*bad_wr = wr;
			goto out;
		}

		if (unlikely(wr->num_sge > qp->rq.max_gs)) {
			mlx4_ib_dbg("QP 0x%x: too many sg entries (%d)",
				    ibqp->qp_num, wr->num_sge);
			err = -EINVAL;
			*bad_wr = wr;
			goto out;
		}

		scat = get_recv_wqe(qp, ind);

		for (i = 0; i < wr->num_sge; ++i)
			__set_data_seg(scat + i, wr->sg_list + i);

		if (i < qp->rq.max_gs) {
			scat[i].byte_count = 0;
			scat[i].lkey       = cpu_to_be32(MLX4_INVALID_LKEY);
			scat[i].addr       = 0;
		}

		qp->rq.wrid[ind] = wr->wr_id;

		ind = (ind + 1) & (qp->rq.wqe_cnt - 1);
	}

out:
	if (likely(nreq)) {
		qp->rq.head += nreq;

		/*
		 * Make sure that descriptors are written before
		 * doorbell record.
		 */
		wmb();

		*qp->db.db = cpu_to_be32(qp->rq.head & 0xffff);
	}

	spin_unlock_irqrestore(&qp->rq.lock, flags);

	return err;
}

static inline enum ib_qp_state to_ib_qp_state(enum mlx4_qp_state mlx4_state)
{
	switch (mlx4_state) {
	case MLX4_QP_STATE_RST:      return IB_QPS_RESET;
	case MLX4_QP_STATE_INIT:     return IB_QPS_INIT;
	case MLX4_QP_STATE_RTR:      return IB_QPS_RTR;
	case MLX4_QP_STATE_RTS:      return IB_QPS_RTS;
	case MLX4_QP_STATE_SQ_DRAINING:
	case MLX4_QP_STATE_SQD:      return IB_QPS_SQD;
	case MLX4_QP_STATE_SQER:     return IB_QPS_SQE;
	case MLX4_QP_STATE_ERR:      return IB_QPS_ERR;
	default:		     return -1;
	}
}

static inline enum ib_mig_state to_ib_mig_state(int mlx4_mig_state)
{
	switch (mlx4_mig_state) {
	case MLX4_QP_PM_ARMED:		return IB_MIG_ARMED;
	case MLX4_QP_PM_REARM:		return IB_MIG_REARM;
	case MLX4_QP_PM_MIGRATED:	return IB_MIG_MIGRATED;
	default: return -1;
	}
}

static int to_ib_qp_access_flags(int mlx4_flags)
{
	int ib_flags = 0;

	if (mlx4_flags & MLX4_QP_BIT_RRE)
		ib_flags |= IB_ACCESS_REMOTE_READ;
	if (mlx4_flags & MLX4_QP_BIT_RWE)
		ib_flags |= IB_ACCESS_REMOTE_WRITE;
	if (mlx4_flags & MLX4_QP_BIT_RAE)
		ib_flags |= IB_ACCESS_REMOTE_ATOMIC;

	return ib_flags;
}

static void to_ib_ah_attr(struct mlx4_ib_dev *ib_dev, struct ib_ah_attr *ib_ah_attr,
			  struct mlx4_qp_path *path)
{
	struct mlx4_dev *dev = ib_dev->dev;
	int is_eth;

	memset(ib_ah_attr, 0, sizeof *ib_ah_attr);
	ib_ah_attr->port_num	  = path->sched_queue & 0x40 ? 2 : 1;

	if (ib_ah_attr->port_num == 0 || ib_ah_attr->port_num > dev->caps.num_ports)
		return;

	is_eth = rdma_port_get_link_layer(&ib_dev->ib_dev, ib_ah_attr->port_num) ==
		IB_LINK_LAYER_ETHERNET;
	if (is_eth)
		ib_ah_attr->sl = ((path->sched_queue >> 3) & 0x7) |
		((path->sched_queue & 4) << 1);
	else
		ib_ah_attr->sl = (path->sched_queue >> 2) & 0xf;

	ib_ah_attr->dlid	  = be16_to_cpu(path->rlid);

	ib_ah_attr->src_path_bits = path->grh_mylmc & 0x7f;
	ib_ah_attr->static_rate   = path->static_rate ? path->static_rate - 5 : 0;
	ib_ah_attr->ah_flags      = (path->grh_mylmc & (1 << 7)) ? IB_AH_GRH : 0;
	if (ib_ah_attr->ah_flags) {
		ib_ah_attr->grh.sgid_index = path->mgid_index;
		ib_ah_attr->grh.hop_limit  = path->hop_limit;
		ib_ah_attr->grh.traffic_class =
			(be32_to_cpu(path->tclass_flowlabel) >> 20) & 0xff;
		ib_ah_attr->grh.flow_label =
			be32_to_cpu(path->tclass_flowlabel) & 0xfffff;
		memcpy(ib_ah_attr->grh.dgid.raw,
			path->rgid, sizeof ib_ah_attr->grh.dgid.raw);
	}
}

int mlx4_ib_query_qp(struct ib_qp *ibqp, struct ib_qp_attr *qp_attr, int qp_attr_mask,
		     struct ib_qp_init_attr *qp_init_attr)
{
	struct mlx4_ib_dev *dev = to_mdev(ibqp->device);
	struct mlx4_ib_qp *qp = to_mqp(ibqp);
	struct mlx4_qp_context context;
	int mlx4_state;
	int err = 0;

	mutex_lock(&qp->mutex);

	if (qp->state == IB_QPS_RESET) {
		qp_attr->qp_state = IB_QPS_RESET;
		goto done;
	}

	err = mlx4_qp_query(dev->dev, &qp->mqp, &context);
	if (err) {
		err = -EINVAL;
		goto out;
	}

	mlx4_state = be32_to_cpu(context.flags) >> 28;

	qp->state		     = to_ib_qp_state(mlx4_state);
	qp_attr->qp_state	     = qp->state;
	qp_attr->path_mtu	     = context.mtu_msgmax >> 5;
	qp_attr->path_mig_state	     =
		to_ib_mig_state((be32_to_cpu(context.flags) >> 11) & 0x3);
	qp_attr->qkey		     = be32_to_cpu(context.qkey);
	qp_attr->rq_psn		     = be32_to_cpu(context.rnr_nextrecvpsn) & 0xffffff;
	qp_attr->sq_psn		     = be32_to_cpu(context.next_send_psn) & 0xffffff;
	qp_attr->dest_qp_num	     = be32_to_cpu(context.remote_qpn) & 0xffffff;
	qp_attr->qp_access_flags     =
		to_ib_qp_access_flags(be32_to_cpu(context.params2));

	if (qp->ibqp.qp_type == IB_QPT_RC || qp->ibqp.qp_type == IB_QPT_UC ||
	    qp->ibqp.qp_type == IB_QPT_XRC) {
		to_ib_ah_attr(dev, &qp_attr->ah_attr, &context.pri_path);
		to_ib_ah_attr(dev, &qp_attr->alt_ah_attr, &context.alt_path);
		qp_attr->alt_pkey_index = context.alt_path.pkey_index & 0x7f;
		qp_attr->alt_port_num	= qp_attr->alt_ah_attr.port_num;
	}

	qp_attr->pkey_index = context.pri_path.pkey_index & 0x7f;
	if (qp_attr->qp_state == IB_QPS_INIT)
		qp_attr->port_num = qp->port;
	else
		qp_attr->port_num = context.pri_path.sched_queue & 0x40 ? 2 : 1;

	/* qp_attr->en_sqd_async_notify is only applicable in modify qp */
	qp_attr->sq_draining = mlx4_state == MLX4_QP_STATE_SQ_DRAINING;

	qp_attr->max_rd_atomic = 1 << ((be32_to_cpu(context.params1) >> 21) & 0x7);

	qp_attr->max_dest_rd_atomic =
		1 << ((be32_to_cpu(context.params2) >> 21) & 0x7);
	qp_attr->min_rnr_timer	    =
		(be32_to_cpu(context.rnr_nextrecvpsn) >> 24) & 0x1f;
	qp_attr->timeout	    = context.pri_path.ackto >> 3;
	qp_attr->retry_cnt	    = (be32_to_cpu(context.params1) >> 16) & 0x7;
	qp_attr->rnr_retry	    = (be32_to_cpu(context.params1) >> 13) & 0x7;
	qp_attr->alt_timeout	    = context.alt_path.ackto >> 3;

done:
	qp_attr->cur_qp_state	     = qp_attr->qp_state;
	qp_attr->cap.max_recv_wr     = qp->rq.wqe_cnt;
	qp_attr->cap.max_recv_sge    = qp->rq.max_gs;

	if (!ibqp->uobject) {
		qp_attr->cap.max_send_wr  = qp->sq.wqe_cnt;
		qp_attr->cap.max_send_sge = qp->sq.max_gs;
	} else {
		qp_attr->cap.max_send_wr  = 0;
		qp_attr->cap.max_send_sge = 0;
	}

	/*
	 * We don't support inline sends for kernel QPs (yet), and we
	 * don't know what userspace's value should be.
	 */
	qp_attr->cap.max_inline_data = 0;

	qp_init_attr->cap	     = qp_attr->cap;

	qp_init_attr->create_flags = 0;
	if (qp->flags & MLX4_IB_QP_BLOCK_MULTICAST_LOOPBACK)
		qp_init_attr->create_flags |= IB_QP_CREATE_BLOCK_MULTICAST_LOOPBACK;

	if (qp->flags & MLX4_IB_QP_LSO)
		qp_init_attr->create_flags |= IB_QP_CREATE_IPOIB_UD_LSO;

out:
	mutex_unlock(&qp->mutex);
	return err;
}

int mlx4_ib_create_xrc_rcv_qp(struct ib_qp_init_attr *init_attr,
			      u32 *qp_num)
{
	struct mlx4_ib_dev *dev = to_mdev(init_attr->xrc_domain->device);
	struct mlx4_ib_xrcd *xrcd = to_mxrcd(init_attr->xrc_domain);
	struct mlx4_ib_qp *qp;
	struct ib_qp *ibqp;
	struct mlx4_ib_xrc_reg_entry *ctx_entry;
	unsigned long flags;
	int err;

	if (!(dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_XRC))
		return -ENOSYS;

	if (init_attr->qp_type != IB_QPT_XRC)
		return -EINVAL;

	ctx_entry = kmalloc(sizeof *ctx_entry, GFP_KERNEL);
	if (!ctx_entry)
		return -ENOMEM;

	qp = kzalloc(sizeof *qp, GFP_KERNEL);
	if (!qp) {
		kfree(ctx_entry);
		return -ENOMEM;
	}
	mutex_lock(&dev->xrc_reg_mutex);
	qp->flags = MLX4_IB_XRC_RCV;
	qp->xrcdn = to_mxrcd(init_attr->xrc_domain)->xrcdn;
	INIT_LIST_HEAD(&qp->xrc_reg_list);
	err = create_qp_common(dev, xrcd->pd, init_attr, NULL, 0, qp);
	if (err) {
		mutex_unlock(&dev->xrc_reg_mutex);
		kfree(ctx_entry);
		kfree(qp);
		return err;
	}

	ibqp = &qp->ibqp;
	/* set the ibpq attributes which will be used by the mlx4 module */
	ibqp->qp_num = qp->mqp.qpn;
	ibqp->device = init_attr->xrc_domain->device;
	ibqp->pd = xrcd->pd;
	ibqp->send_cq = ibqp->recv_cq = xrcd->cq;
	ibqp->event_handler = init_attr->event_handler;
	ibqp->qp_context = init_attr->qp_context;
	ibqp->qp_type = init_attr->qp_type;
	ibqp->xrcd = init_attr->xrc_domain;

	mutex_lock(&qp->mutex);
	ctx_entry->context = init_attr->qp_context;
	spin_lock_irqsave(&qp->xrc_reg_list_lock, flags);
	list_add_tail(&ctx_entry->list, &qp->xrc_reg_list);
	spin_unlock_irqrestore(&qp->xrc_reg_list_lock, flags);
	mutex_unlock(&qp->mutex);
	mutex_unlock(&dev->xrc_reg_mutex);
	*qp_num = qp->mqp.qpn;
	return 0;
}

int mlx4_ib_modify_xrc_rcv_qp(struct ib_xrcd *ibxrcd, u32 qp_num,
			      struct ib_qp_attr *attr, int attr_mask)
{
	struct mlx4_ib_dev *dev = to_mdev(ibxrcd->device);
	struct mlx4_ib_xrcd *xrcd = to_mxrcd(ibxrcd);
	struct mlx4_qp *mqp;
	struct mlx4_ib_qp *mibqp;
	int err = -EINVAL;

	if (!(dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_XRC))
		return -ENOSYS;

	mutex_lock(&dev->xrc_reg_mutex);
	mqp = mlx4_qp_lookup_lock(dev->dev, qp_num);
	if (unlikely(!mqp)) {
		printk(KERN_WARNING "mlx4_ib_reg_xrc_rcv_qp: "
		       "unknown QPN %06x\n", qp_num);
		goto err_out;
	}

	mibqp = to_mibqp(mqp);

	if (!(mibqp->flags & MLX4_IB_XRC_RCV) || !mibqp->ibqp.xrcd ||
	    xrcd->xrcdn != to_mxrcd(mibqp->ibqp.xrcd)->xrcdn)
		goto err_out;

	err = mlx4_ib_modify_qp(&mibqp->ibqp, attr, attr_mask, NULL);
	mutex_unlock(&dev->xrc_reg_mutex);
	return err;

err_out:
	mutex_unlock(&dev->xrc_reg_mutex);
	return err;
}

int mlx4_ib_query_xrc_rcv_qp(struct ib_xrcd *ibxrcd, u32 qp_num,
			     struct ib_qp_attr *qp_attr, int qp_attr_mask,
			     struct ib_qp_init_attr *qp_init_attr)
{
	struct mlx4_ib_dev *dev = to_mdev(ibxrcd->device);
	struct mlx4_ib_xrcd *xrcd = to_mxrcd(ibxrcd);
	struct mlx4_ib_qp *qp;
	struct mlx4_qp *mqp;
	struct mlx4_qp_context context;
	int mlx4_state;
	int err = -EINVAL;

	if (!(dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_XRC))
		return -ENOSYS;

	mutex_lock(&dev->xrc_reg_mutex);
	mqp = mlx4_qp_lookup_lock(dev->dev, qp_num);
	if (unlikely(!mqp)) {
		printk(KERN_WARNING "mlx4_ib_reg_xrc_rcv_qp: "
		       "unknown QPN %06x\n", qp_num);
		goto err_out;
	}

	qp = to_mibqp(mqp);
	if (!(qp->flags & MLX4_IB_XRC_RCV) || !(qp->ibqp.xrcd) ||
	    xrcd->xrcdn != to_mxrcd(qp->ibqp.xrcd)->xrcdn)
		goto err_out;

	if (qp->state == IB_QPS_RESET) {
		qp_attr->qp_state = IB_QPS_RESET;
		goto done;
	}

	err = mlx4_qp_query(dev->dev, mqp, &context);
	if (err)
		goto err_out;

	mlx4_state = be32_to_cpu(context.flags) >> 28;

	qp_attr->qp_state = to_ib_qp_state(mlx4_state);
	qp_attr->path_mtu = context.mtu_msgmax >> 5;
	qp_attr->path_mig_state =
		to_ib_mig_state((be32_to_cpu(context.flags) >> 11) & 0x3);
	qp_attr->qkey = be32_to_cpu(context.qkey);
	qp_attr->rq_psn = be32_to_cpu(context.rnr_nextrecvpsn) & 0xffffff;
	qp_attr->sq_psn = be32_to_cpu(context.next_send_psn) & 0xffffff;
	qp_attr->dest_qp_num = be32_to_cpu(context.remote_qpn) & 0xffffff;
	qp_attr->qp_access_flags =
		to_ib_qp_access_flags(be32_to_cpu(context.params2));

	if (qp->ibqp.qp_type == IB_QPT_RC || qp->ibqp.qp_type == IB_QPT_UC ||
	    qp->ibqp.qp_type == IB_QPT_XRC) {
		to_ib_ah_attr(dev, &qp_attr->ah_attr, &context.pri_path);
		to_ib_ah_attr(dev, &qp_attr->alt_ah_attr,
			      &context.alt_path);
		qp_attr->alt_pkey_index = context.alt_path.pkey_index & 0x7f;
		qp_attr->alt_port_num	= qp_attr->alt_ah_attr.port_num;
	}

	qp_attr->pkey_index = context.pri_path.pkey_index & 0x7f;
	if (qp_attr->qp_state == IB_QPS_INIT)
		qp_attr->port_num = qp->port;
	else
		qp_attr->port_num = context.pri_path.sched_queue & 0x40 ? 2 : 1;

	/* qp_attr->en_sqd_async_notify is only applicable in modify qp */
	qp_attr->sq_draining = mlx4_state == MLX4_QP_STATE_SQ_DRAINING;

	qp_attr->max_rd_atomic =
		1 << ((be32_to_cpu(context.params1) >> 21) & 0x7);

	qp_attr->max_dest_rd_atomic =
		1 << ((be32_to_cpu(context.params2) >> 21) & 0x7);
	qp_attr->min_rnr_timer =
		(be32_to_cpu(context.rnr_nextrecvpsn) >> 24) & 0x1f;
	qp_attr->timeout = context.pri_path.ackto >> 3;
	qp_attr->retry_cnt = (be32_to_cpu(context.params1) >> 16) & 0x7;
	qp_attr->rnr_retry = (be32_to_cpu(context.params1) >> 13) & 0x7;
	qp_attr->alt_timeout = context.alt_path.ackto >> 3;

done:
	qp_attr->cur_qp_state	     = qp_attr->qp_state;
	qp_attr->cap.max_recv_wr     = 0;
	qp_attr->cap.max_recv_sge    = 0;
	qp_attr->cap.max_send_wr     = 0;
	qp_attr->cap.max_send_sge    = 0;
	qp_attr->cap.max_inline_data = 0;
	qp_init_attr->cap	     = qp_attr->cap;

	mutex_unlock(&dev->xrc_reg_mutex);
	return 0;

err_out:
	mutex_unlock(&dev->xrc_reg_mutex);
	return err;
}

int mlx4_ib_reg_xrc_rcv_qp(struct ib_xrcd *xrcd, void *context, u32 qp_num)
{

	struct mlx4_ib_xrcd *mxrcd = to_mxrcd(xrcd);

	struct mlx4_qp *mqp;
	struct mlx4_ib_qp *mibqp;
	struct mlx4_ib_xrc_reg_entry *ctx_entry, *tmp;
	unsigned long flags;
	int err = -EINVAL;

	mutex_lock(&to_mdev(xrcd->device)->xrc_reg_mutex);
	mqp = mlx4_qp_lookup_lock(to_mdev(xrcd->device)->dev, qp_num);
	if (unlikely(!mqp)) {
		printk(KERN_WARNING "mlx4_ib_reg_xrc_rcv_qp: "
		       "unknown QPN %06x\n", qp_num);
		goto err_out;
	}

	mibqp = to_mibqp(mqp);

	if (!(mibqp->flags & MLX4_IB_XRC_RCV) || !(mibqp->ibqp.xrcd) ||
	    mxrcd->xrcdn != to_mxrcd(mibqp->ibqp.xrcd)->xrcdn)
		goto err_out;

	ctx_entry = kmalloc(sizeof *ctx_entry, GFP_KERNEL);
	if (!ctx_entry) {
		err = -ENOMEM;
		goto err_out;
	}

	mutex_lock(&mibqp->mutex);
	list_for_each_entry(tmp, &mibqp->xrc_reg_list, list)
		if (tmp->context == context) {
			mutex_unlock(&mibqp->mutex);
			kfree(ctx_entry);
			mutex_unlock(&to_mdev(xrcd->device)->xrc_reg_mutex);
			return 0;
		}

	ctx_entry->context = context;
	spin_lock_irqsave(&mibqp->xrc_reg_list_lock, flags);
	list_add_tail(&ctx_entry->list, &mibqp->xrc_reg_list);
	spin_unlock_irqrestore(&mibqp->xrc_reg_list_lock, flags);
	mutex_unlock(&mibqp->mutex);
	mutex_unlock(&to_mdev(xrcd->device)->xrc_reg_mutex);
	return 0;

err_out:
	mutex_unlock(&to_mdev(xrcd->device)->xrc_reg_mutex);
	return err;
}

int mlx4_ib_unreg_xrc_rcv_qp(struct ib_xrcd *xrcd, void *context, u32 qp_num)
{

	struct mlx4_ib_xrcd *mxrcd = to_mxrcd(xrcd);

	struct mlx4_qp *mqp;
	struct mlx4_ib_qp *mibqp;
	struct mlx4_ib_xrc_reg_entry *ctx_entry, *tmp;
	unsigned long flags;
	int found = 0;
	int err = -EINVAL;

	mutex_lock(&to_mdev(xrcd->device)->xrc_reg_mutex);
	mqp = mlx4_qp_lookup_lock(to_mdev(xrcd->device)->dev, qp_num);
	if (unlikely(!mqp)) {
		printk(KERN_WARNING "mlx4_ib_unreg_xrc_rcv_qp: "
		       "unknown QPN %06x\n", qp_num);
		goto err_out;
	}

	mibqp = to_mibqp(mqp);

	if (!(mibqp->flags & MLX4_IB_XRC_RCV) ||
	    mxrcd->xrcdn != (mibqp->xrcdn & 0xffff))
		goto err_out;

	mutex_lock(&mibqp->mutex);
	spin_lock_irqsave(&mibqp->xrc_reg_list_lock, flags);
	list_for_each_entry_safe(ctx_entry, tmp, &mibqp->xrc_reg_list, list)
		if (ctx_entry->context == context) {
			found = 1;
			list_del(&ctx_entry->list);
			spin_unlock_irqrestore(&mibqp->xrc_reg_list_lock, flags);
			kfree(ctx_entry);
			break;
		}

	if (!found)
		spin_unlock_irqrestore(&mibqp->xrc_reg_list_lock, flags);
	mutex_unlock(&mibqp->mutex);
	if (!found)
		goto err_out;

	/* destroy the QP if the registration list is empty */
	if (list_empty(&mibqp->xrc_reg_list))
		mlx4_ib_destroy_qp(&mibqp->ibqp);

	mutex_unlock(&to_mdev(xrcd->device)->xrc_reg_mutex);
	return 0;

err_out:
	mutex_unlock(&to_mdev(xrcd->device)->xrc_reg_mutex);
	return err;
}

