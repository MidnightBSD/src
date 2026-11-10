/*
 * Copyright (c) 2025-2026 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <net/page_pool/helpers.h>

#ifndef PAGE_IS_LKPI_PAGE
/*
 * MNBSD-KPI: page pool without a LinuxKPI 'struct page'.
 *
 * In LinuxKPI 'struct page' is the native vm_page, which has no room for
 * the page pool pointer or the DMA address.  Upstream only implements the
 * page pool if PAGE_IS_LKPI_PAGE is defined, which nothing does yet, so
 * page_pool_alloc_frag() always failed and mt76 had no rx buffers (the MCU
 * never answered: "failed to get patch semaphore").
 *
 * Keep the per-page state in a side table keyed by the page instead.
 * Each entry records the device and DMA direction so it can be unmapped
 * without the pool.  page_pool_destroy() unmaps the pages of that pool
 * which are still out (e.g. queued in an skb); they are freed normally
 * when their last user lets go.  Callers' "pp" arguments to put are
 * ignored (mt76 derives them through pp_page_to_nmdesc() which is not
 * meaningful here).
 */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>

struct lkpi_pp_page {
	struct lkpi_pp_page		*next;	/* hash chain */
	struct page			*page;
	const struct page_pool		*pool;	/* owner; never dereferenced */
	struct device			*dev;
	dma_addr_t			dma;
	enum dma_data_direction		dir;
	bool				mapped;
};

#define	LKPI_PP_HASH_SIZE	1024	/* power of 2 */
#define	LKPI_PP_HASH(_p)	\
    ((((uintptr_t)(_p)) >> 7) & (LKPI_PP_HASH_SIZE - 1))

static MALLOC_DEFINE(M_LKPI_PP, "lkpipp", "LinuxKPI page pool pages");
/* Plain chains; <sys/queue.h> LIST_HEAD clashes with LinuxKPI's. */
static struct lkpi_pp_page *lkpi_pp_hash[LKPI_PP_HASH_SIZE];
static struct mtx lkpi_pp_mtx;
MTX_SYSINIT(lkpi_pp_mtx, &lkpi_pp_mtx, "lkpi page pool", MTX_DEF);

static struct lkpi_pp_page *
lkpi_pp_lookup_locked(const struct page *page)
{
	struct lkpi_pp_page *ppp;

	mtx_assert(&lkpi_pp_mtx, MA_OWNED);
	for (ppp = lkpi_pp_hash[LKPI_PP_HASH(page)]; ppp != NULL;
	    ppp = ppp->next) {
		if (ppp->page == page)
			return (ppp);
	}
	return (NULL);
}

/* Unlink and return the entry for page, or NULL. */
static struct lkpi_pp_page *
lkpi_pp_remove_locked(const struct page *page)
{
	struct lkpi_pp_page **pp, *ppp;

	mtx_assert(&lkpi_pp_mtx, MA_OWNED);
	for (pp = &lkpi_pp_hash[LKPI_PP_HASH(page)]; (ppp = *pp) != NULL;
	    pp = &ppp->next) {
		if (ppp->page == page) {
			*pp = ppp->next;
			ppp->next = NULL;
			return (ppp);
		}
	}
	return (NULL);
}

/*
 * Unmap and free a page handed out by a page pool.  Returns false if the
 * page did not come from a page pool, in which case nothing was done.
 */
bool
linuxkpi_page_pool_release_page(struct page *page)
{
	struct lkpi_pp_page *ppp;

	if (page == NULL)
		return (false);
	mtx_lock(&lkpi_pp_mtx);
	ppp = lkpi_pp_remove_locked(page);
	mtx_unlock(&lkpi_pp_mtx);
	if (ppp == NULL)
		return (false);

	if (ppp->mapped)
		dma_unmap_page(ppp->dev, ppp->dma, PAGE_SIZE, ppp->dir);
	free(ppp, M_LKPI_PP);
	put_page(page);
	return (true);
}
#endif /* !PAGE_IS_LKPI_PAGE */

struct page_pool *
linuxkpi_page_pool_create(const struct page_pool_params *ppparams)
{
	struct page_pool *pp;
	int error;


	pp = kzalloc_node(sizeof(*pp), GFP_KERNEL, ppparams->nid);
	if (pp == NULL)
		return (ERR_PTR(-ENOMEM));

	pp->params = *ppparams;

	if ((pp->params.flags & PP_FLAG_DMA_MAP) != 0) {
		if (pp->params.dma_dir != DMA_FROM_DEVICE &&
		    pp->params.dma_dir != DMA_BIDIRECTIONAL) {
			pr_warn_once("%s: invalid dma_dir %#04x\n",
			    __func__, pp->params.dma_dir);
			error = -EINVAL;
			goto free_pp;
		}
	}

	if ((pp->params.flags & PP_FLAG_DMA_SYNC_DEV) != 0) {
		if ((pp->params.flags & PP_FLAG_DMA_MAP) == 0) {
			pr_warn_once("%s: DMA_SYNC_DEV but no DMA_MAP\n",
			    __func__);
			error = -EINVAL;
			goto free_pp;
		}

		if (pp->params.max_len == 0) {
			pr_warn_once("%s: max_len unset\n", __func__);
			error = -EINVAL;
			goto free_pp;
		}
	}

	return (pp);

free_pp:
	kfree(pp);
	return (ERR_PTR(error));
}

void
linuxkpi_page_pool_destroy(struct page_pool *pp)
{
#ifndef PAGE_IS_LKPI_PAGE
	/*
	 * MNBSD-KPI: unmap pages of this pool still in flight; the device
	 * may go away before they are freed.  Their entries stay so that
	 * the eventual free still takes the (now unmapped) side table path.
	 */
	struct lkpi_pp_page *ppp;
	int i;

	if (pp == NULL || IS_ERR(pp))
		return;
	mtx_lock(&lkpi_pp_mtx);
	for (i = 0; i < LKPI_PP_HASH_SIZE; i++) {
		for (ppp = lkpi_pp_hash[i]; ppp != NULL; ppp = ppp->next) {
			if (ppp->pool != pp)
				continue;
			if (ppp->mapped) {
				dma_unmap_page(ppp->dev, ppp->dma, PAGE_SIZE,
				    ppp->dir);
				ppp->mapped = false;
				ppp->dma = 0;
			}
			ppp->pool = NULL;
		}
	}
	mtx_unlock(&lkpi_pp_mtx);
#endif
	kfree(pp);
}

/* ---------------------------------------------------------------------------- */

struct page *
linuxkpi_page_pool_alloc_frag(struct page_pool *pp, uint32_t *offset,
    size_t size, gfp_t gfp)
{
#ifdef PAGE_IS_LKPI_PAGE
	struct page *p;
	dma_addr_t dma;

	if (size > PAGE_SIZE) {
		pr_warn_once("%s: size %zu > PAGE_SIZE\n", __func__, size);
		return (NULL);
	}

	/*
	 * We do not try to be smart on the first cut until 'struct page' work
	 * is sorted and settled.  Everyone gets a page for now.
	 * XXX in the future deal with page fragments, pool limits, etc.
	 */
	p = alloc_page(gfp);
	if (p == NULL)
		return (NULL);

	if (offset != NULL)
		*offset = 0;

	if ((pp->params.flags & PP_FLAG_DMA_MAP) != 0) {
		dma = dma_map_page_attrs(pp->params.dev, p, 0, PAGE_SIZE,
		    pp->params.dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
		if (dma_mapping_error(pp->params.dev, dma)) {
			put_page(p);
			return (NULL);
		}

		/* We store the dma address in struct page as well. */
		p->dma_addr = dma;
	}

	if ((pp->params.flags & PP_FLAG_DMA_SYNC_DEV) != 0)
		dma_sync_single_for_device(pp->params.dev, dma, PAGE_SIZE,
		    pp->params.dma_dir);

	/*
	 * We may not yet need to track the page pool on the page in order
	 * to know where to return it but prepare at least for that.
	 */
	p->pp = pp;

	return (p);
#else
	/* MNBSD-KPI: see the side table comment at the top. */
	struct lkpi_pp_page *ppp;
	struct page *p;

	if (size > PAGE_SIZE) {
		pr_warn_once("%s: size %zu > PAGE_SIZE\n", __func__, size);
		return (NULL);
	}

	ppp = malloc(sizeof(*ppp), M_LKPI_PP, M_NOWAIT | M_ZERO);
	if (ppp == NULL)
		return (NULL);

	/*
	 * Everyone gets a full page for now (as upstream).  If the device
	 * cannot reach all of memory (mt76 uses a 32-bit mask) allocate
	 * below 4G; a bounced rx page would never see the device's writes
	 * without an explicit sync and costs a bounce page per buffer.
	 */
	if ((pp->params.flags & PP_FLAG_DMA_MAP) != 0) {
		u64 mask;

		mask = linuxkpi_dma_get_mask(pp->params.dev);
		if (mask != 0 && mask <= BUS_SPACE_MAXADDR_32BIT)
			gfp |= GFP_DMA32;
	}
	p = alloc_page(gfp);
	if (p == NULL) {
		free(ppp, M_LKPI_PP);
		return (NULL);
	}
	if (offset != NULL)
		*offset = 0;

	ppp->page = p;
	ppp->pool = pp;
	ppp->dev = pp->params.dev;
	ppp->dir = pp->params.dma_dir;
	if ((pp->params.flags & PP_FLAG_DMA_MAP) != 0) {
		ppp->dma = dma_map_page_attrs(pp->params.dev, p, 0, PAGE_SIZE,
		    pp->params.dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
		if (dma_mapping_error(pp->params.dev, ppp->dma)) {
			free(ppp, M_LKPI_PP);
			put_page(p);
			return (NULL);
		}
		ppp->mapped = true;
		if ((pp->params.flags & PP_FLAG_DMA_SYNC_DEV) != 0)
			dma_sync_single_for_device(pp->params.dev, ppp->dma,
			    PAGE_SIZE, pp->params.dma_dir);
	}

	mtx_lock(&lkpi_pp_mtx);
	ppp->next = lkpi_pp_hash[LKPI_PP_HASH(p)];
	lkpi_pp_hash[LKPI_PP_HASH(p)] = ppp;
	mtx_unlock(&lkpi_pp_mtx);

	return (p);
#endif /* PAGE_IS_LKPI_PAGE */
}

struct page *
linuxkpi_page_pool_dev_alloc_frag(struct page_pool *pp, uint32_t *offset,
    size_t size)
{
	gfp_t gfp;

	gfp = (GFP_ATOMIC | __GFP_NOWARN);
	return (page_pool_alloc_frag(pp, offset, size, gfp));
}

void
linuxkpi_page_pool_put_full_page(struct page_pool *pp, struct page *page,
    bool allow_direct)
{
	/* We just free the full page we handed out for now. */
#ifdef PAGE_IS_LKPI_PAGE
	page->pp = NULL;
	page->dma_addr = 0;
#else
	/* MNBSD-KPI: unmap and drop the side table entry. */
	if (linuxkpi_page_pool_release_page(page))
		return;
#endif
	put_page(page);
}

dma_addr_t
linuxkpi_page_pool_get_dma_addr(const struct page *page)
{

	KASSERT(page != NULL, ("%s: page %p\n", __func__, page));

#ifdef PAGE_IS_LKPI_PAGE
	return (page->dma_addr);
#else
	/* MNBSD-KPI: look the DMA address up in the side table. */
	struct lkpi_pp_page *ppp;
	dma_addr_t dma;

	mtx_lock(&lkpi_pp_mtx);
	ppp = lkpi_pp_lookup_locked(page);
	dma = (ppp != NULL) ? ppp->dma : 0;
	mtx_unlock(&lkpi_pp_mtx);
	if (ppp == NULL)
		pr_warn_once("%s: page %p not from a page pool\n", __func__,
		    page);
	return (dma);
#endif
}
