/*-
 * Copyright (c) 2020-2025 The FreeBSD Foundation
 * Copyright (c) 2021-2022 Bjoern A. Zeeb
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
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
 */

/*
 * NOTE: this socket buffer compatibility code is highly EXPERIMENTAL.
 *       Do not rely on the internals of this implementation.  They are highly
 *       likely to change as we will improve the integration to FreeBSD mbufs.
 */

#include <sys/cdefs.h>
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>

#include <vm/uma.h>
#ifdef __MidnightBSD__
#include <sys/systm.h>
#include <machine/atomic.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_phys.h>
#endif

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#ifdef __LP64__
#include <linux/log2.h>
#endif

#include <net/page_pool/helpers.h>

#if defined(__MidnightBSD__) && !defined(PAGE_IS_LKPI_PAGE)
/*
 * MNBSD-KPI: skbs may carry page pool pages (head and/or frags; mt76 marks
 * them for recycling but do not rely on the flag).  Those have to go back through the page pool so the DMA mapping
 * is torn down.  Fall back to the normal free if a page is not known to
 * any page pool.
 */
static void
lkpi_skb_free_frag_page(struct sk_buff *skb, struct page *p)
{

	if (linuxkpi_page_pool_release_page(p))
		return;
	__free_page(p);
}

static void
lkpi_skb_free_head_frag(struct sk_buff *skb, void *head)
{

	if (linuxkpi_page_pool_release_page(virt_to_head_page(head)))
		return;
	skb_free_frag(head);
}
#endif

SYSCTL_DECL(_compat_linuxkpi);
SYSCTL_NODE(_compat_linuxkpi, OID_AUTO, skb, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "LinuxKPI skbuff");

#ifdef SKB_DEBUG
int linuxkpi_debug_skb;
SYSCTL_INT(_compat_linuxkpi_skb, OID_AUTO, debug, CTLFLAG_RWTUN,
    &linuxkpi_debug_skb, 0, "SKB debug level");
#endif

static uma_zone_t skbzone;

#define	SKB_DMA32_MALLOC
#ifdef	SKB_DMA32_MALLOC
/*
 * Realtek wireless drivers (e.g., rtw88) require 32bit DMA in a single segment.
 * busdma(9) has a hard time providing this currently for 3-ish pages at large
 * quantities (see lkpi_pci_nseg1_fail in linux_pci.c).
 * Work around this for now by allowing a tunable to enforce physical addresses
 * allocation limits using "old-school" contigmalloc(9) to avoid bouncing.
 * Note: with the malloc/contigmalloc + kmalloc changes also providing physical
 * contiguous memory, and the nseg=1 limit for bouncing we should in theory be
 * fine now and not need any of this anymore, however busdma still has troubles
 * boncing three contiguous pages so for now this stays.
 */
static int linuxkpi_skb_memlimit;
SYSCTL_INT(_compat_linuxkpi_skb, OID_AUTO, mem_limit, CTLFLAG_RDTUN,
    &linuxkpi_skb_memlimit, 0, "SKB memory limit: 0=no limit, "
    "1=32bit, 2=36bit, other=undef (currently 32bit)");

static MALLOC_DEFINE(M_LKPISKB, "lkpiskb", "Linux KPI skbuff compat");

#ifdef __MidnightBSD__
/*
 * MNBSD-KPI: pick the limit automatically.  A device with a DMA mask
 * below the top of physical memory otherwise gets skb heads > 4K that
 * bounce into several segments, which the nseg=1 LinuxKPI tag rejects
 * (mt7925 on a >4G machine: "Failed to send patch").  An explicitly
 * set compat.linuxkpi.skb.mem_limit tunable always wins.
 *
 * The limit may change at runtime, so each skb records how its head
 * was allocated (_SKB_FLAGS_HEADCONTIG) and is freed accordingly.
 */
static bool linuxkpi_skb_memlimit_user;

static void
linuxkpi_skb_memlimit_init(void *arg __unused)
{

	linuxkpi_skb_memlimit_user =
	    testenv("compat.linuxkpi.skb.mem_limit") != 0;
}
SYSINIT(lkpi_skb_memlimit, SI_SUB_TUNABLES, SI_ORDER_ANY,
    linuxkpi_skb_memlimit_init, NULL);

void
linuxkpi_skb_note_dma_mask(uint64_t mask)
{
	vm_paddr_t top;
	int lim, cur;

	if (linuxkpi_skb_memlimit_user || mask == 0 || vm_phys_nsegs == 0)
		return;
	top = vm_phys_segs[vm_phys_nsegs - 1].end;
	if (mask >= top - 1)
		return;		/* The device reaches all of memory. */

	if (mask <= 0xffffffffULL)
		lim = 1;
#ifdef __LP64__
	else if (mask <= 0xfffffffffULL)
		lim = 2;
#endif
	else
		return;		/* No matching limit; leave it alone. */

	cur = atomic_load_int(&linuxkpi_skb_memlimit);
	if (cur != 0 && cur <= lim)
		return;		/* Already as strict or stricter. */
	atomic_store_int(&linuxkpi_skb_memlimit, lim);
	printf("LinuxKPI: DMA mask %#jx below top of memory %#jx, "
	    "allocating skbs below %s\n", (uintmax_t)mask, (uintmax_t)top,
	    (lim == 1) ? "4G" : "64G");
}
#endif /* __MidnightBSD__ */
#endif

struct sk_buff *
linuxkpi_alloc_skb(size_t size, gfp_t gfp)
{
	struct sk_buff *skb;
	void *p;
	size_t len;

	skb = uma_zalloc(skbzone, linux_check_m_flags(gfp) | M_ZERO);
	if (skb == NULL)
		return (NULL);

	skb->prev = skb->next = skb;
	skb->truesize = size;
	skb->shinfo = (struct skb_shared_info *)(skb + 1);

	if (size == 0)
		return (skb);

	len = size;
#ifdef	SKB_DMA32_MALLOC
	/*
	 * Using our own type here not backing my kmalloc.
	 * We assume no one calls kfree directly on the skb.
	 */
	if (__predict_false(linuxkpi_skb_memlimit != 0)) {
		vm_paddr_t high;

		switch (atomic_load_int(&linuxkpi_skb_memlimit)) {
#ifdef __LP64__
		case 2:
			high = (0xfffffffff);	/* 1<<36 really. */
			break;
#endif
		case 1:
		default:
			high = (0xffffffff);	/* 1<<32 really. */
			break;
		}
		len = roundup_pow_of_two(len);
		p = contigmalloc(len, M_LKPISKB,
		    linux_check_m_flags(gfp) | M_ZERO, 0, high, PAGE_SIZE, 0);
#ifdef __MidnightBSD__
		if (p != NULL)
			skb->_flags |= _SKB_FLAGS_HEADCONTIG;	/* MNBSD-KPI */
#endif
	} else
#endif
	p = __kmalloc(len, linux_check_m_flags(gfp) | M_ZERO);
	if (p == NULL) {
		uma_zfree(skbzone, skb);
		return (NULL);
	}

	skb->head = skb->data = (uint8_t *)p;
	skb_reset_tail_pointer(skb);
	skb->end = skb->head + size;
	refcount_set(&skb->refcnt, 1);

	SKB_TRACE_FMT(skb, "data %p size %zu", (skb) ? skb->data : NULL, size);
	return (skb);
}

struct sk_buff *
linuxkpi_dev_alloc_skb(size_t size, gfp_t gfp)
{
	struct sk_buff *skb;
	size_t len;

	len = size + NET_SKB_PAD;
	skb = linuxkpi_alloc_skb(len, gfp);

	if (skb != NULL)
		skb_reserve(skb, NET_SKB_PAD);

	SKB_TRACE_FMT(skb, "data %p size %zu len %zu",
	    (skb) ? skb->data : NULL, size, len);
	return (skb);
}

struct sk_buff *
linuxkpi_build_skb(void *data, size_t fragsz)
{
	struct sk_buff *skb;

	if (data == NULL || fragsz == 0)
		return (NULL);

	/* Just allocate a skb without data area. */
	skb = linuxkpi_alloc_skb(0, GFP_KERNEL);
	if (skb == NULL)
		return (NULL);

	skb->_flags |= _SKB_FLAGS_SKBEXTFRAG;
	skb->truesize = fragsz;
	skb->head = skb->data = data;
	skb_reset_tail_pointer(skb);
	skb->end = skb->head + fragsz;
	refcount_set(&skb->refcnt, 1);

	return (skb);
}

struct sk_buff *
linuxkpi_skb_copy(const struct sk_buff *skb, gfp_t gfp)
{
	struct sk_buff *new;
	struct skb_shared_info *shinfo;
	size_t len;
	unsigned int headroom;
	uint16_t fragno, count;

	/* Full buffer size + any fragments. */
	len = skb->end - skb->head + skb->data_len;

	new = linuxkpi_alloc_skb(len, gfp);
	if (new == NULL)
		return (NULL);

	headroom = skb_headroom(skb);
	/* Fixup head and end. */
	skb_reserve(new, headroom);	/* data and tail move headroom forward. */
	skb_put(new, skb->len);		/* tail and len get adjusted */

	/* Copy data. */
	memcpy(new->head, skb->data - headroom, headroom + skb->len);

	/* Deal with fragments. */
	shinfo = skb->shinfo;
	for (count = fragno = 0;
	    count < shinfo->nr_frags && fragno < nitems(shinfo->frags);
	    fragno++) {
		if (shinfo->frags[fragno].page != NULL) {
			skb_put_data(new,
			    skb_frag_address(&shinfo->frags[fragno]),
			    shinfo->frags[fragno].size);
			count++;
		}
	}

	/* Deal with header fields. */
	memcpy(new->cb, skb->cb, sizeof(skb->cb));
	SKB_IMPROVE("more header fields to copy?");

	return (new);
}

int
lkpi___skb_linearize(struct sk_buff *skb)
{
	struct sk_buff *new;
	struct skb_shared_info *shinfo;
	uint16_t fragno, count;

	SKB_TRACE(skb);
	SKB_IMPROVE("Hack completely re-allocating and freeing; FIXME");
	new = skb_copy(skb, GFP_NOWAIT);
	if (new == NULL)
		return (-ENOMEM);

	/* Now need to swap head, data, tail, ... and free from old (and then new). */
	shinfo = skb->shinfo;
	for (count = fragno = 0;
	    count < shinfo->nr_frags && fragno < nitems(shinfo->frags);
	    fragno++) {

		if (shinfo->frags[fragno].page != NULL) {
			struct page *p;

			p = shinfo->frags[fragno].page;
			shinfo->frags[fragno].size = 0;
			shinfo->frags[fragno].offset = 0;
			shinfo->frags[fragno].page = NULL;
#if defined(__MidnightBSD__) && !defined(PAGE_IS_LKPI_PAGE)
			lkpi_skb_free_frag_page(skb, p);	/* MNBSD-KPI */
#else
			__free_page(p);
#endif
			count++;
		}
	}

	if ((skb->_flags & _SKB_FLAGS_SKBEXTFRAG) != 0) {
		void *p;

		p = skb->head;
#if defined(__MidnightBSD__) && !defined(PAGE_IS_LKPI_PAGE)
		lkpi_skb_free_head_frag(skb, p);	/* MNBSD-KPI */
#else
		skb_free_frag(p);
#endif
		skb->head = NULL;
		skb->_flags &= ~_SKB_FLAGS_SKBEXTFRAG;
	}

#ifdef	SKB_DMA32_MALLOC
#ifdef __MidnightBSD__
	if (__predict_false((skb->_flags & _SKB_FLAGS_HEADCONTIG) != 0))
#else
	if (__predict_false(linuxkpi_skb_memlimit != 0))
#endif
		free(skb->head, M_LKPISKB);
	else
#endif
	kfree(skb->head);

	skb->head = new->head;
	skb->data = new->data;
	skb->tail = new->tail;
	skb->end = new->end;
	skb->len = new->len;
	skb->data_len = new->data_len;
	skb->truesize = new->truesize;
#ifdef __MidnightBSD__
	/* MNBSD-KPI: the head now comes from new; so does how to free it. */
	skb->_flags = (skb->_flags & ~_SKB_FLAGS_HEADCONTIG) |
	    (new->_flags & _SKB_FLAGS_HEADCONTIG);
#endif

	uma_zfree(skbzone, new);

	return (0);
}

static bool
lkpi_skb_refcount_release(struct sk_buff *skb)
{
	if (skb == NULL)
		return (false);

	/* Do we need further tests to avoid freeing this one? */

	if (!refcount_dec_and_test(&skb->refcnt))
		return (false);

	return (true);
}

void
linuxkpi_kfree_skb(struct sk_buff *skb)
{
	struct skb_shared_info *shinfo;
	uint16_t fragno, count;

	SKB_TRACE(skb);
	if (skb == NULL)
		return;

	if (!lkpi_skb_refcount_release(skb)) {
		SKB_TRACE_FMT(skb, "not freed due to refcnt");
		return;
	}

	/*
	 * XXX TODO this will go away once we have skb backed by mbuf.
	 * currently we allow the mbuf to stay around and use a private
	 * free function to allow secondary resources to be freed along.
	 */
	if (skb->m != NULL) {
		void *m;

		m = skb->m;
		skb->m = NULL;

		KASSERT(skb->m_free_func != NULL, ("%s: skb %p has m %p but no "
		    "m_free_func %p\n", __func__, skb, m, skb->m_free_func));
		skb->m_free_func(m);
	}
	KASSERT(skb->m == NULL,
	    ("%s: skb %p m %p != NULL\n", __func__, skb, skb->m));

	shinfo = skb->shinfo;
	for (count = fragno = 0;
	    count < shinfo->nr_frags && fragno < nitems(shinfo->frags);
	    fragno++) {

		if (shinfo->frags[fragno].page != NULL) {
			struct page *p;

			p = shinfo->frags[fragno].page;
			shinfo->frags[fragno].size = 0;
			shinfo->frags[fragno].offset = 0;
			shinfo->frags[fragno].page = NULL;
#ifdef PAGE_IS_LKPI_PAGE
			if ((skb->_flags & _SKB_PP_RECYCLE) != 0)
				page_pool_put_full_page(p->pp, p, false);
			else
				__free_page(p);
#elif defined(__MidnightBSD__)
			lkpi_skb_free_frag_page(skb, p);	/* MNBSD-KPI */
#else
				__free_page(p);
#endif
			count++;
		}
	}

	if ((skb->_flags & _SKB_FLAGS_SKBEXTFRAG) != 0) {
		void *p;

		p = skb->head;
#if defined(__MidnightBSD__) && !defined(PAGE_IS_LKPI_PAGE)
		lkpi_skb_free_head_frag(skb, p);	/* MNBSD-KPI */
#else
		skb_free_frag(p);
#endif
		skb->head = NULL;
	}

#ifdef	SKB_DMA32_MALLOC
#ifdef __MidnightBSD__
	if (__predict_false((skb->_flags & _SKB_FLAGS_HEADCONTIG) != 0))
#else
	if (__predict_false(linuxkpi_skb_memlimit != 0))
#endif
		free(skb->head, M_LKPISKB);
	else
#endif
	kfree(skb->head);
	uma_zfree(skbzone, skb);
}

static void
lkpi_skbuff_init(void *arg __unused)
{
	skbzone = uma_zcreate("skbuff",
	    sizeof(struct sk_buff) + sizeof(struct skb_shared_info),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	/* Do we need to apply limits? */
}
SYSINIT(linuxkpi_skbuff, SI_SUB_DRIVERS, SI_ORDER_FIRST, lkpi_skbuff_init, NULL);

static void
lkpi_skbuff_destroy(void *arg __unused)
{
	uma_zdestroy(skbzone);
}
SYSUNINIT(linuxkpi_skbuff, SI_SUB_DRIVERS, SI_ORDER_SECOND, lkpi_skbuff_destroy, NULL);

#ifdef DDB
DB_SHOW_COMMAND(skb, db_show_skb)
{
	struct sk_buff *skb;
	int i;

	if (!have_addr) {
		db_printf("usage: show skb <addr>\n");
			return;
	}

	skb = (struct sk_buff *)addr;

	db_printf("skb %p\n", skb);
	db_printf("\tnext %p prev %p\n", skb->next, skb->prev);
	db_printf("\tlist %p\n", &skb->list);
	db_printf("\tlen %u data_len %u truesize %u mac_len %u\n",
	    skb->len, skb->data_len, skb->truesize, skb->mac_len);
	db_printf("\tcsum %#06x l3hdroff %u l4hdroff %u priority %u qmap %u\n",
	    skb->csum, skb->l3hdroff, skb->l4hdroff, skb->priority, skb->qmap);
	db_printf("\tpkt_type %d dev %p sk %p\n",
	    skb->pkt_type, skb->dev, skb->sk);
	db_printf("\tcsum_offset %d csum_start %d ip_summed %d protocol %d\n",
	    skb->csum_offset, skb->csum_start, skb->ip_summed, skb->protocol);
	db_printf("\t_flags %#06x\n", skb->_flags);		/* XXX-BZ print names? */
	db_printf("\thead %p data %p tail %p end %p\n",
	    skb->head, skb->data, skb->tail, skb->end);
	db_printf("\tshinfo %p m %p m_free_func %p\n",
	    skb->shinfo, skb->m, skb->m_free_func);

	if (skb->shinfo != NULL) {
		struct skb_shared_info *shinfo;

		shinfo = skb->shinfo;
		db_printf("\t\tgso_type %d gso_size %u nr_frags %u\n",
		    shinfo->gso_type, shinfo->gso_size, shinfo->nr_frags);
		for (i = 0; i < nitems(shinfo->frags); i++) {
			struct skb_frag *frag;

			frag = &shinfo->frags[i];
			if (frag == NULL || frag->page == NULL)
				continue;
			db_printf("\t\t\tfrag %p fragno %d page %p %p "
			    "offset %ju size %zu\n",
			    frag, i, frag->page, linux_page_address(frag->page),
			    (uintmax_t)frag->offset, frag->size);
		}
	}
	db_printf("\tcb[] %p {", skb->cb);
	for (i = 0; i < nitems(skb->cb); i++) {
		db_printf("%#04x%s",
		    skb->cb[i], (i < (nitems(skb->cb)-1)) ? ", " : "");
	}
	db_printf("}\n");

	db_printf("\t__scratch[0] %p\n", skb->__scratch);
};
#endif
