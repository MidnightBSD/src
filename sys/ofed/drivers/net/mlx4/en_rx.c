/*
 * Copyright (c) 2007 Mellanox Technologies. All rights reserved.
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
 *
 */

#include "mlx4_en.h"

#include <linux/mlx4/cq.h>
#include <linux/mlx4/qp.h>

#include <net/ethernet.h>
#include <net/if_vlan_var.h>
#include <sys/mbuf.h>

enum {
	MIN_RX_ARM = 1024,
};

static int mlx4_en_alloc_buf(struct mlx4_en_priv *priv,
			     struct mlx4_en_rx_desc *rx_desc,
			     struct mbuf **mb_list,
			     int i)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_frag_info *frag_info = &priv->frag_info[i];
	struct mbuf *mb;
	dma_addr_t dma;

	if (i == 0)
		mb = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, frag_info->frag_size);
	else
		mb = m_getjcl(M_NOWAIT, MT_DATA, 0, frag_info->frag_size);
	if (mb == NULL) {
		priv->port_stats.rx_alloc_failed++;
		return -ENOMEM;
	}
	dma = pci_map_single(mdev->pdev, mb->m_data, frag_info->frag_size,
			     PCI_DMA_FROMDEVICE);
	rx_desc->data[i].addr = cpu_to_be64(dma);
	mb_list[i] = mb;
	return 0;
}

static void mlx4_en_init_rx_desc(struct mlx4_en_priv *priv,
				 struct mlx4_en_rx_ring *ring, int index)
{
	struct mlx4_en_rx_desc *rx_desc = ring->buf + ring->stride * index;
	int possible_frags;
	int i;

	/* Set size and memtype fields */
	for (i = 0; i < priv->num_frags; i++) {
		rx_desc->data[i].byte_count =
			cpu_to_be32(priv->frag_info[i].frag_size);
		rx_desc->data[i].lkey = cpu_to_be32(priv->mdev->mr.key);
	}

	/* If the number of used fragments does not fill up the ring stride,
	 * remaining (unused) fragments must be padded with null address/size
	 * and a special memory key */
	possible_frags = (ring->stride - sizeof(struct mlx4_en_rx_desc)) / DS_SIZE;
	for (i = priv->num_frags; i < possible_frags; i++) {
		rx_desc->data[i].byte_count = 0;
		rx_desc->data[i].lkey = cpu_to_be32(MLX4_EN_MEMTYPE_PAD);
		rx_desc->data[i].addr = 0;
	}
}

static int mlx4_en_prepare_rx_desc(struct mlx4_en_priv *priv,
				   struct mlx4_en_rx_ring *ring, int index)
{
	struct mlx4_en_rx_desc *rx_desc = ring->buf + (index * ring->stride);
	struct mbuf **mb_list = ring->rx_info + (index << priv->log_rx_info);
	int i;

	for (i = 0; i < priv->num_frags; i++)
		if (mlx4_en_alloc_buf(priv, rx_desc, mb_list, i))
			goto err;

	return 0;

err:
	while (i--)
		m_free(mb_list[i]);
	return -ENOMEM;
}

static inline void mlx4_en_update_rx_prod_db(struct mlx4_en_rx_ring *ring)
{
	*ring->wqres.db.db = cpu_to_be32(ring->prod & 0xffff);
}

static void mlx4_en_free_rx_desc(struct mlx4_en_priv *priv,
				 struct mlx4_en_rx_ring *ring,
				 int index)
{
	struct mlx4_en_frag_info *frag_info;
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mbuf **mb_list;
	struct mlx4_en_rx_desc *rx_desc = ring->buf + (index << ring->log_stride);
	dma_addr_t dma;
	int nr;

	mb_list = ring->rx_info + (index << priv->log_rx_info);
	for (nr = 0; nr < priv->num_frags; nr++) {
		en_dbg(DRV, priv, "Freeing fragment:%d\n", nr);
 		frag_info = &priv->frag_info[nr];
		dma = be64_to_cpu(rx_desc->data[nr].addr);

		en_dbg(DRV, priv, "Unmaping buffer at dma:0x%llx\n", (u64) dma);
		pci_unmap_single(mdev->pdev, dma, frag_info->frag_size,
				 PCI_DMA_FROMDEVICE);
		m_free(mb_list[nr]);
	}
}

static int mlx4_en_fill_rx_buffers(struct mlx4_en_priv *priv)
{
	struct mlx4_en_rx_ring *ring;
	int ring_ind;
	int buf_ind;
	int new_size;
	int err;

	for (buf_ind = 0; buf_ind < priv->prof->rx_ring_size; buf_ind++) {
		for (ring_ind = 0; ring_ind < priv->rx_ring_num; ring_ind++) {
			ring = &priv->rx_ring[ring_ind];

			err = mlx4_en_prepare_rx_desc(priv, ring,
						      ring->actual_size);
			if (err) {
				if (ring->actual_size == 0) {
					en_err(priv, "Failed to allocate "
						     "enough rx buffers\n");
					return -ENOMEM;
				} else {
					new_size = rounddown_pow_of_two(ring->actual_size);
					en_warn(priv, "Only %d buffers allocated "
						      "reducing ring size to %d\n",
						ring->actual_size, new_size);
					goto reduce_rings;
				}
			}
			ring->actual_size++;
			ring->prod++;
		}
	}
	return 0;

reduce_rings:
	for (ring_ind = 0; ring_ind < priv->rx_ring_num; ring_ind++) {
		ring = &priv->rx_ring[ring_ind];
		while (ring->actual_size > new_size) {
			ring->actual_size--;
			ring->prod--;
			mlx4_en_free_rx_desc(priv, ring, ring->actual_size);
		}
	}

	return 0;
}

static void mlx4_en_free_rx_buf(struct mlx4_en_priv *priv,
				struct mlx4_en_rx_ring *ring)
{
	int index;

	en_dbg(DRV, priv, "Freeing Rx buf - cons:%d prod:%d\n",
	       ring->cons, ring->prod);

	/* Unmap and free Rx buffers */
	BUG_ON((u32) (ring->prod - ring->cons) > ring->actual_size);
	while (ring->cons != ring->prod) {
		index = ring->cons & ring->size_mask;
		en_dbg(DRV, priv, "Processing descriptor:%d\n", index);
		mlx4_en_free_rx_desc(priv, ring, index);
		++ring->cons;
	}
}


int mlx4_en_create_rx_ring(struct mlx4_en_priv *priv,
			   struct mlx4_en_rx_ring *ring, u32 size)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	int err;
	int tmp;


	ring->prod = 0;
	ring->cons = 0;
	ring->size = size;
	ring->size_mask = size - 1;
	ring->stride = roundup_pow_of_two(sizeof(struct mlx4_en_rx_desc) +
					  DS_SIZE * MLX4_EN_MAX_RX_FRAGS);
	ring->log_stride = ffs(ring->stride) - 1;
	ring->buf_size = ring->size * ring->stride + TXBB_SIZE;

	tmp = size * roundup_pow_of_two(MLX4_EN_MAX_RX_FRAGS *
					sizeof(struct mbuf *));

	ring->rx_info = kmalloc(tmp, GFP_KERNEL);
	if (!ring->rx_info) {
		en_err(priv, "Failed allocating rx_info ring\n");
		return -ENOMEM;
	}
	en_dbg(DRV, priv, "Allocated rx_info ring at addr:%p size:%d stride:%d (%d)\n",
		 ring->rx_info, tmp, ring->stride, ring->log_stride);

	err = mlx4_alloc_hwq_res(mdev->dev, &ring->wqres,
				 ring->buf_size, 2 * PAGE_SIZE);
	if (err)
		goto err_ring;

	err = mlx4_en_map_buffer(&ring->wqres.buf);
	if (err) {
		en_err(priv, "Failed to map RX buffer\n");
		goto err_hwq;
	}
	ring->buf = ring->wqres.buf.direct.buf;

	return 0;

	mlx4_en_unmap_buffer(&ring->wqres.buf);
err_hwq:
	mlx4_free_hwq_res(mdev->dev, &ring->wqres, ring->buf_size);
err_ring:
	kfree(ring->rx_info);
	ring->rx_info = NULL;
	return err;
}

int mlx4_en_activate_rx_rings(struct mlx4_en_priv *priv)
{
	struct mlx4_en_rx_ring *ring;
	int i;
	int ring_ind;
	int err;
	int stride = roundup_pow_of_two(sizeof(struct mlx4_en_rx_desc) +
					DS_SIZE * priv->num_frags);

	for (ring_ind = 0; ring_ind < priv->rx_ring_num; ring_ind++) {
		ring = &priv->rx_ring[ring_ind];

		ring->prod = 0;
		ring->cons = 0;
		ring->actual_size = 0;
		ring->cqn = priv->rx_cq[ring_ind].mcq.cqn;
		ring->stride = stride;
		if (ring->stride <= TXBB_SIZE)
			ring->buf += TXBB_SIZE;

		ring->log_stride = ffs(ring->stride) - 1;
		ring->buf_size = ring->size * ring->stride;

		memset(ring->buf, 0, ring->buf_size);
		mlx4_en_update_rx_prod_db(ring);

		/* Initailize all descriptors */
		for (i = 0; i < ring->size; i++)
			mlx4_en_init_rx_desc(priv, ring, i);
		/* Configure lro mngr */
		if (priv->dev->if_capenable & IFCAP_LRO) {
			if (tcp_lro_init(&ring->lro))
				priv->dev->if_capenable &= ~IFCAP_LRO;
			else
				ring->lro.ifp = priv->dev;
		}
	}
	err = mlx4_en_fill_rx_buffers(priv);
	if (err)
		goto err_buffers;

	for (ring_ind = 0; ring_ind < priv->rx_ring_num; ring_ind++) {
		ring = &priv->rx_ring[ring_ind];

		ring->size_mask = ring->actual_size - 1;
		mlx4_en_update_rx_prod_db(ring);
	}


	return 0;

err_buffers:
	for (ring_ind = 0; ring_ind < priv->rx_ring_num; ring_ind++)
		mlx4_en_free_rx_buf(priv, &priv->rx_ring[ring_ind]);

	return err;
}

void mlx4_en_destroy_rx_ring(struct mlx4_en_priv *priv,
			     struct mlx4_en_rx_ring *ring)
{
	struct mlx4_en_dev *mdev = priv->mdev;

	mlx4_en_unmap_buffer(&ring->wqres.buf);
	mlx4_free_hwq_res(mdev->dev, &ring->wqres, ring->buf_size + TXBB_SIZE);
	kfree(ring->rx_info);
	ring->rx_info = NULL;
}

void mlx4_en_deactivate_rx_ring(struct mlx4_en_priv *priv,
				struct mlx4_en_rx_ring *ring)
{
	tcp_lro_free(&ring->lro);
	mlx4_en_free_rx_buf(priv, ring);
	if (ring->stride <= TXBB_SIZE)
		ring->buf -= TXBB_SIZE;
}


/* Unmap a completed descriptor and free unused pages */
static int mlx4_en_complete_rx_desc(struct mlx4_en_priv *priv,
				    struct mlx4_en_rx_desc *rx_desc,
				    struct mbuf **mb_list,
				    int length)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_frag_info *frag_info;
	dma_addr_t dma;
	struct mbuf *mb;
	int nr;

	mb = mb_list[0];
	mb->m_pkthdr.len = length;
	/* Collect used fragments while replacing them in the HW descirptors */
	for (nr = 0; nr < priv->num_frags; nr++) {
		frag_info = &priv->frag_info[nr];
		if (length <= frag_info->frag_prefix_size)
			break;
		if (nr) 
			mb->m_next = mb_list[nr];
		mb = mb_list[nr];
		mb->m_len = frag_info[nr].frag_size;
		dma = be64_to_cpu(rx_desc->data[nr].addr);

		/* Allocate a replacement page */
		if (mlx4_en_alloc_buf(priv, rx_desc, mb_list, nr))
			goto fail;

		/* Unmap buffer */
		pci_unmap_single(mdev->pdev, dma, frag_info[nr].frag_size,
				 PCI_DMA_FROMDEVICE);
	}
	/* Adjust size of last fragment to match actual length */
	mb->m_len = length - priv->frag_info[nr - 1].frag_prefix_size;
	mb->m_next = NULL;
	return 0;

fail:
	/* Drop all accumulated fragments (which have already been replaced in
	 * the descriptor) of this packet; remaining fragments are reused... */
	while (nr > 0) {
		nr--;
		m_free(mb_list[nr]);
	}
	return -ENOMEM;
}


static inline int invalid_cqe(struct mlx4_en_priv *priv,
			      struct mlx4_cqe *cqe)
{
	/* Drop packet on bad receive or bad checksum */
	if (unlikely((cqe->owner_sr_opcode & MLX4_CQE_OPCODE_MASK) ==
		     MLX4_CQE_OPCODE_ERROR)) {
		en_err(priv, "CQE completed in error - vendor "
			 "syndrom:%d syndrom:%d\n",
			 ((struct mlx4_err_cqe *) cqe)->vendor_err_syndrome,
			 ((struct mlx4_err_cqe *) cqe)->syndrome);
		return 1;
	}
	if (unlikely(cqe->badfcs_enc & MLX4_CQE_BAD_FCS)) {
		en_dbg(RX_ERR, priv, "Accepted frame with bad FCS\n");
		return 1;;
	}

	return 0;
}

static void validate_loopback(struct mlx4_en_priv *priv, struct mbuf *mb)
{
	int i;
	int offset = ETHER_HDR_LEN;

	for (i = 0; i < MLX4_LOOPBACK_TEST_PAYLOAD; i++, offset++) {
		if (*(mb->m_data + offset) != (unsigned char) (i & 0xff))
			goto out_loopback;
	}
	/* Loopback found */
	priv->loopback_ok = 1;

out_loopback:
	m_freem(mb);
}

static struct mbuf *mlx4_en_rx_mb(struct mlx4_en_priv *priv,
				  struct mlx4_en_rx_desc *rx_desc,
				  struct mbuf **mb_list,
				  unsigned int length)
{
	struct mbuf *mb;

	mb = mb_list[0];
	/* Move relevant fragments to mb */
	if (unlikely(mlx4_en_complete_rx_desc(priv, rx_desc, mb_list, length)))
		return NULL;

	return mb;
}


int mlx4_en_process_rx_cq(struct net_device *dev, struct mlx4_en_cq *cq, int budget)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_cqe *cqe;
	struct mlx4_en_rx_ring *ring = &priv->rx_ring[cq->ring];
	struct mbuf **mb_list;
	struct mlx4_en_rx_desc *rx_desc;
	struct mbuf *mb;
	struct lro_entry *queued;
	int index;
	unsigned int length;
	int polled = 0;

	if (!priv->port_up)
		return 0;

	/* We assume a 1:1 mapping between CQEs and Rx descriptors, so Rx
	 * descriptor offset can be deduced from the CQE index instead of
	 * reading 'cqe->index' */
	index = cq->mcq.cons_index & ring->size_mask;
	cqe = &cq->buf[index];

	/* Process all completed CQEs */
	while (XNOR(cqe->owner_sr_opcode & MLX4_CQE_OWNER_MASK,
		    cq->mcq.cons_index & cq->size)) {

		mb_list = ring->rx_info + (index << priv->log_rx_info);
		rx_desc = ring->buf + (index << ring->log_stride);

		/*
		 * make sure we read the CQE after we read the ownership bit
		 */
		rmb();

		if (invalid_cqe(priv, cqe))
			goto next;

		/*
		 * Packet is OK - process it.
		 */
		length = be32_to_cpu(cqe->byte_cnt);
		mb = mlx4_en_rx_mb(priv, rx_desc, mb_list, length);
		if (!mb) {
			ring->errors++;
			goto next;
		}

		ring->bytes += length;
		ring->packets++;

                if (unlikely(priv->validate_loopback)) {
			validate_loopback(priv, mb);
			goto next;
		}

		mb->m_pkthdr.flowid = cq->ring;
		mb->m_flags |= M_FLOWID;
		mb->m_pkthdr.rcvif = dev;
		if (be32_to_cpu(cqe->vlan_my_qpn) &
		    MLX4_CQE_VLAN_PRESENT_MASK) {
			mb->m_pkthdr.ether_vtag = be16_to_cpu(cqe->sl_vid);
			mb->m_flags |= M_VLANTAG;
		}
		if (likely(priv->rx_csum) &&
		    (cqe->status & cpu_to_be16(MLX4_CQE_STATUS_IPOK)) &&
		    (cqe->checksum == cpu_to_be16(0xffff))) {
			priv->port_stats.rx_chksum_good++;
			mb->m_pkthdr.csum_flags = 
			    CSUM_IP_CHECKED | CSUM_IP_VALID |
			    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
			mb->m_pkthdr.csum_data = htons(0xffff);
			/* This packet is eligible for LRO if it is:
			 * - DIX Ethernet (type interpretation)
			 * - TCP/IP (v4)
			 * - without IP options
			 * - not an IP fragment
			 */
			if (mlx4_en_can_lro(cqe->status) &&
			    (dev->if_capenable & IFCAP_LRO)) {
				if (ring->lro.lro_cnt != 0 &&
				    tcp_lro_rx(&ring->lro, mb, 0) == 0)
					goto next;
			}

			/* LRO not possible, complete processing here */
			INC_PERF_COUNTER(priv->pstats.lro_misses);
		} else {
			mb->m_pkthdr.csum_flags = 0;
			priv->port_stats.rx_chksum_none++;
			if (priv->ip_reasm &&
			    cqe->status & cpu_to_be16(MLX4_CQE_STATUS_IPV4) &&
			    !mlx4_en_rx_frags(priv, ring, mb, cqe))
				goto next;
		}

		/* Push it up the stack */
		dev->if_input(dev, mb);

next:
		++cq->mcq.cons_index;
		index = (cq->mcq.cons_index) & ring->size_mask;
		cqe = &cq->buf[index];
		if (++polled == budget)
			goto out;
	}
	/* Flush all pending IP reassembly sessions */
out:
	mlx4_en_flush_frags(priv, ring);
	while ((queued = SLIST_FIRST(&ring->lro.lro_active)) != NULL) {
		SLIST_REMOVE_HEAD(&ring->lro.lro_active, next);
		tcp_lro_flush(&ring->lro, queued);
	}
	AVG_PERF_COUNTER(priv->pstats.rx_coal_avg, polled);
	mlx4_cq_set_ci(&cq->mcq);
	wmb(); /* ensure HW sees CQ consumer before we post new buffers */
	ring->cons = cq->mcq.cons_index;
	ring->prod += polled; /* Polled descriptors were realocated in place */
	mlx4_en_update_rx_prod_db(ring);
	return polled;
}


/* Rx CQ polling - called by NAPI */
static int mlx4_en_poll_rx_cq(struct mlx4_en_cq *cq, int budget)
{
	struct net_device *dev = cq->dev;
	int done;

	done = mlx4_en_process_rx_cq(dev, cq, budget);
	cq->tot_rx += done;

	return done;
}

void mlx4_en_rx_que(void *context, int pending)
{
	struct mlx4_en_cq *cq;

        cq = context;
	while (mlx4_en_poll_rx_cq(cq, MLX4_EN_MAX_RX_POLL)
	    == MLX4_EN_MAX_RX_POLL);
	mlx4_en_arm_cq(cq->dev->if_softc, cq);
}

void mlx4_en_rx_irq(struct mlx4_cq *mcq)
{
	struct mlx4_en_cq *cq = container_of(mcq, struct mlx4_en_cq, mcq);
	struct mlx4_en_priv *priv = netdev_priv(cq->dev);
	int done;

	done = mlx4_en_poll_rx_cq(cq, MLX4_EN_MAX_RX_POLL);
	if (done == MLX4_EN_MAX_RX_POLL)
		taskqueue_enqueue(cq->tq, &cq->cq_task);
	else
		mlx4_en_arm_cq(priv, cq);
}


#if MLX4_EN_MAX_RX_FRAGS == 3
static int frag_sizes[] = {
	FRAG_SZ0,
	FRAG_SZ1,
	FRAG_SZ2,
};
#elif MLX4_EN_MAX_RX_FRAGS == 2
static int frag_sizes[] = {
	FRAG_SZ0,
	FRAG_SZ1,
};
#else
#error "Unknown MAX_RX_FRAGS"
#endif

void mlx4_en_calc_rx_buf(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int eff_mtu = dev->if_mtu + ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN + ETH_LLC_SNAP_SIZE;
	int buf_size = 0;
	int i, frag;

	for (i = 0, frag = 0; buf_size < eff_mtu; frag++, i++) {
		/*
		 * Allocate small to large but only as much as is needed for
		 * the tail.
		 */
		while (i > 0 && eff_mtu - buf_size <= frag_sizes[i - 1])
			i--;
		priv->frag_info[frag].frag_size = frag_sizes[i];
		priv->frag_info[frag].frag_prefix_size = buf_size;
		buf_size += priv->frag_info[frag].frag_size;
	}

	priv->num_frags = frag;
	priv->rx_mb_size = eff_mtu;
	priv->log_rx_info =
	    ROUNDUP_LOG2(priv->num_frags * sizeof(struct mbuf *));

	en_dbg(DRV, priv, "Rx buffer scatter-list (effective-mtu:%d "
		  "num_frags:%d):\n", eff_mtu, priv->num_frags);
	for (i = 0; i < priv->num_frags; i++) {
		en_dbg(DRV, priv, "  frag:%d - size:%d prefix:%d\n", i,
				priv->frag_info[i].frag_size,
				priv->frag_info[i].frag_prefix_size)
	}
}

/* RSS related functions */

static int mlx4_en_config_rss_qp(struct mlx4_en_priv *priv, int qpn,
				 struct mlx4_en_rx_ring *ring,
				 enum mlx4_qp_state *state,
				 struct mlx4_qp *qp)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_qp_context *context;
	int err = 0;

	context = kmalloc(sizeof *context , GFP_KERNEL);
	if (!context) {
		en_err(priv, "Failed to allocate qp context\n");
		return -ENOMEM;
	}

	err = mlx4_qp_alloc(mdev->dev, qpn, qp);
	if (err) {
		en_err(priv, "Failed to allocate qp #%x\n", qpn);
		goto out;
	}
	qp->event = mlx4_en_sqp_event;

	memset(context, 0, sizeof *context);
	mlx4_en_fill_qp_context(priv, ring->actual_size, ring->stride, 0, 0,
				qpn, ring->cqn, context);
	context->db_rec_addr = cpu_to_be64(ring->wqres.db.dma);

	err = mlx4_qp_to_ready(mdev->dev, &ring->wqres.mtt, context, qp, state);
	if (err) {
		mlx4_qp_remove(mdev->dev, qp);
		mlx4_qp_free(mdev->dev, qp);
	}
	mlx4_en_update_rx_prod_db(ring);
out:
	kfree(context);
	return err;
}

/* Allocate rx qp's and configure them according to rss map */
int mlx4_en_config_rss_steer(struct mlx4_en_priv *priv)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_rss_map *rss_map = &priv->rss_map;
	struct mlx4_qp_context context;
	struct mlx4_en_rss_context *rss_context;
	void *ptr;
	u8 rss_mask;
	int i, qpn;
	int err = 0;
	int good_qps = 0;

	if (mdev->profile.udp_rss)
		rss_mask = 0x3f;
	else
		rss_mask = 0x14;
	en_dbg(DRV, priv, "Configuring rss steering\n");
	err = mlx4_qp_reserve_range(mdev->dev, priv->rx_ring_num,
				    roundup_pow_of_two(priv->rx_ring_num),
				    &rss_map->base_qpn);
	if (err) {
		en_err(priv, "Failed reserving %d qps\n", priv->rx_ring_num);
		return err;
	}

	for (i = 0; i < priv->rx_ring_num; i++) {
		qpn = rss_map->base_qpn + i;
		err = mlx4_en_config_rss_qp(priv, qpn,
					    &priv->rx_ring[i],
					    &rss_map->state[i],
					    &rss_map->qps[i]);
		if (err)
			goto rss_err;

		++good_qps;
	}

	/* Configure RSS indirection qp */
	err = mlx4_qp_reserve_range(mdev->dev, 1, 1, &priv->base_qpn);
	if (err) {
		en_err(priv, "Failed to reserve range for RSS "
			     "indirection qp\n");
		goto rss_err;
	}
	err = mlx4_qp_alloc(mdev->dev, priv->base_qpn, &rss_map->indir_qp);
	if (err) {
		en_err(priv, "Failed to allocate RSS indirection QP\n");
		goto reserve_err;
	}
	rss_map->indir_qp.event = mlx4_en_sqp_event;
	mlx4_en_fill_qp_context(priv, 0, 0, 0, 1, priv->base_qpn,
				priv->rx_ring[0].cqn, &context);

	ptr = ((void *) &context) + 0x3c;
	rss_context = (struct mlx4_en_rss_context *) ptr;
	rss_context->base_qpn = cpu_to_be32(ilog2(priv->rx_ring_num) << 24 |
					    (rss_map->base_qpn));
	rss_context->default_qpn = cpu_to_be32(rss_map->base_qpn);
	rss_context->flags = rss_mask;
	rss_context->base_qpn_udp = rss_context->default_qpn;

	err = mlx4_qp_to_ready(mdev->dev, &priv->res.mtt, &context,
			       &rss_map->indir_qp, &rss_map->indir_state);
	if (err)
		goto indir_err;

	return 0;

indir_err:
	mlx4_qp_modify(mdev->dev, NULL, rss_map->indir_state,
		       MLX4_QP_STATE_RST, NULL, 0, 0, &rss_map->indir_qp);
	mlx4_qp_remove(mdev->dev, &rss_map->indir_qp);
	mlx4_qp_free(mdev->dev, &rss_map->indir_qp);
reserve_err:
	mlx4_qp_release_range(mdev->dev, priv->base_qpn, 1);
rss_err:
	for (i = 0; i < good_qps; i++) {
		mlx4_qp_modify(mdev->dev, NULL, rss_map->state[i],
			       MLX4_QP_STATE_RST, NULL, 0, 0, &rss_map->qps[i]);
		mlx4_qp_remove(mdev->dev, &rss_map->qps[i]);
		mlx4_qp_free(mdev->dev, &rss_map->qps[i]);
	}
	mlx4_qp_release_range(mdev->dev, rss_map->base_qpn, priv->rx_ring_num);
	return err;
}

void mlx4_en_release_rss_steer(struct mlx4_en_priv *priv)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_rss_map *rss_map = &priv->rss_map;
	int i;

	mlx4_qp_modify(mdev->dev, NULL, rss_map->indir_state,
		       MLX4_QP_STATE_RST, NULL, 0, 0, &rss_map->indir_qp);
	mlx4_qp_remove(mdev->dev, &rss_map->indir_qp);
	mlx4_qp_free(mdev->dev, &rss_map->indir_qp);
	mlx4_qp_release_range(mdev->dev, priv->base_qpn, 1);

	for (i = 0; i < priv->rx_ring_num; i++) {
		mlx4_qp_modify(mdev->dev, NULL, rss_map->state[i],
			       MLX4_QP_STATE_RST, NULL, 0, 0, &rss_map->qps[i]);
		mlx4_qp_remove(mdev->dev, &rss_map->qps[i]);
		mlx4_qp_free(mdev->dev, &rss_map->qps[i]);
	}
	mlx4_qp_release_range(mdev->dev, rss_map->base_qpn, priv->rx_ring_num);
}
