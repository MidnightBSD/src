/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013-2015 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __X86_IOMMU_INTEL_DMAR_H
#define	__X86_IOMMU_INTEL_DMAR_H

#include <dev/iommu/iommu.h>

struct dmar_unit;

/*
 * Locking annotations:
 * (u) - Protected by iommu unit lock
 * (d) - Protected by domain lock
 * (c) - Immutable after initialization
 */

/*
 * The domain abstraction.  Most non-constant members of the domain
 * are protected by owning dmar unit lock, not by the domain lock.
 * Most important, the dmar lock protects the contexts list.
 *
 * The domain lock protects the address map for the domain, and list
 * of unload entries delayed.
 *
 * Page tables pages and pages content is protected by the vm object
 * lock pgtbl_obj, which contains the page tables pages.
 */
struct dmar_domain {
	struct iommu_domain iodom;
	int domain;			/* (c) DID, written in context entry */
	int mgaw;			/* (c) Real max address width */
	int agaw;			/* (c) Adjusted guest address width */
	int pglvl;			/* (c) The pagelevel */
	int awlvl;			/* (c) The pagelevel as the bitmask,
					   to set in context entry */
	u_int ctx_cnt;			/* (u) Number of contexts owned */
	u_int refs;			/* (u) Refs, including ctx */
	struct dmar_unit *dmar;		/* (c) */
	LIST_ENTRY(dmar_domain) link;	/* (u) Member in the dmar list */
	LIST_HEAD(, dmar_ctx) contexts;	/* (u) */
	vm_object_t pgtbl_obj;		/* (c) Page table pages */
	u_int batch_no;
};

struct dmar_ctx {
	struct iommu_ctx context;
	uint64_t last_fault_rec[2];	/* Last fault reported */
	LIST_ENTRY(dmar_ctx) link;	/* (u) Member in the domain list */
	u_int refs;			/* (u) References from tags */
};

#define	DMAR_DOMAIN_PGLOCK(dom)		VM_OBJECT_WLOCK((dom)->pgtbl_obj)
#define	DMAR_DOMAIN_PGTRYLOCK(dom)	VM_OBJECT_TRYWLOCK((dom)->pgtbl_obj)
#define	DMAR_DOMAIN_PGUNLOCK(dom)	VM_OBJECT_WUNLOCK((dom)->pgtbl_obj)
#define	DMAR_DOMAIN_ASSERT_PGLOCKED(dom) \
	VM_OBJECT_ASSERT_WLOCKED((dom)->pgtbl_obj)

#define	DMAR_DOMAIN_LOCK(dom)	mtx_lock(&(dom)->iodom.lock)
#define	DMAR_DOMAIN_UNLOCK(dom)	mtx_unlock(&(dom)->iodom.lock)
#define	DMAR_DOMAIN_ASSERT_LOCKED(dom) mtx_assert(&(dom)->iodom.lock, MA_OWNED)

#define	DMAR2IOMMU(dmar)	&((dmar)->iommu)
#define	IOMMU2DMAR(dmar)	\
	__containerof((dmar), struct dmar_unit, iommu)

#define	DOM2IODOM(domain)	&((domain)->iodom)
#define	IODOM2DOM(domain)	\
	__containerof((domain), struct dmar_domain, iodom)

#define	CTX2IOCTX(ctx)		&((ctx)->context)
#define	IOCTX2CTX(ctx)		\
	__containerof((ctx), struct dmar_ctx, context)

#define	CTX2DOM(ctx)		IODOM2DOM((ctx)->context.domain)
#define	CTX2DMAR(ctx)		(CTX2DOM(ctx)->dmar)
#define	DOM2DMAR(domain)	((domain)->dmar)

struct dmar_msi_data {
	int irq;
	int irq_rid;
	struct resource *irq_res;
	void *intr_handle;
	int (*handler)(void *);
	int msi_data_reg;
	int msi_addr_reg;
	int msi_uaddr_reg;
	void (*enable_intr)(struct dmar_unit *);
	void (*disable_intr)(struct dmar_unit *);
	const char *name;
};

#define	DMAR_INTR_FAULT		0
#define	DMAR_INTR_QI		1
#define	DMAR_INTR_TOTAL		2

struct dmar_unit {
	struct iommu_unit iommu;
	device_t dev;
	uint16_t segment;
	uint64_t base;

	/* Resources */
	int reg_rid;
	struct resource *regs;

	struct dmar_msi_data intrs[DMAR_INTR_TOTAL];

	/* Hardware registers cache */
	uint32_t hw_ver;
	uint64_t hw_cap;
	uint64_t hw_ecap;
	uint32_t hw_gcmd;

	/* Data for being a dmar */
	LIST_HEAD(, dmar_domain) domains;
	struct unrhdr *domids;
	vm_object_t ctx_obj;
	u_int barrier_flags;

	/* Fault handler data */
	struct mtx fault_lock;
	uint64_t *fault_log;
	int fault_log_head;
	int fault_log_tail;
	int fault_log_size;
	struct task fault_task;
	struct taskqueue *fault_taskqueue;

	/* QI */
	int qi_enabled;
	vm_offset_t inv_queue;
	vm_size_t inv_queue_size;
	uint32_t inv_queue_avail;
	uint32_t inv_queue_tail;
	volatile uint32_t inv_waitd_seq_hw; /* hw writes there on wait
					       descr completion */
	uint64_t inv_waitd_seq_hw_phys;
	uint32_t inv_waitd_seq; /* next sequence number to use for wait descr */
	u_int inv_waitd_gen;	/* seq number generation AKA seq overflows */
	u_int inv_seq_waiters;	/* count of waiters for seq */
	u_int inv_queue_full;	/* informational counter */

	/* IR */
	int ir_enabled;
	vm_paddr_t irt_phys;
	dmar_irte_t *irt;
	u_int irte_cnt;
	vmem_t *irtids;

	/*
	 * Delayed freeing of map entries queue processing:
	 *
	 * tlb_flush_head and tlb_flush_tail are used to implement a FIFO
	 * queue that supports concurrent dequeues and enqueues.  However,
	 * there can only be a single dequeuer (accessing tlb_flush_head) and
	 * a single enqueuer (accessing tlb_flush_tail) at a time.  Since the
	 * unit's qi_task is the only dequeuer, it can access tlb_flush_head
	 * without any locking.  In contrast, there may be multiple enqueuers,
	 * so the enqueuers acquire the iommu unit lock to serialize their
	 * accesses to tlb_flush_tail.
	 *
	 * In this FIFO queue implementation, the key to enabling concurrent
	 * dequeues and enqueues is that the dequeuer never needs to access
	 * tlb_flush_tail and the enqueuer never needs to access
	 * tlb_flush_head.  In particular, tlb_flush_head and tlb_flush_tail
	 * are never NULL, so neither a dequeuer nor an enqueuer ever needs to
	 * update both.  Instead, tlb_flush_head always points to a "zombie"
	 * struct, which previously held the last dequeued item.  Thus, the
	 * zombie's next field actually points to the struct holding the first
	 * item in the queue.  When an item is dequeued, the current zombie is
	 * finally freed, and the struct that held the just dequeued item
	 * becomes the new zombie.  When the queue is empty, tlb_flush_tail
	 * also points to the zombie.
	 */
	struct iommu_map_entry *tlb_flush_head;
	struct iommu_map_entry *tlb_flush_tail;
	struct task qi_task;
	struct taskqueue *qi_taskqueue;
};

#define	DMAR_LOCK(dmar)		mtx_lock(&(dmar)->iommu.lock)
#define	DMAR_UNLOCK(dmar)	mtx_unlock(&(dmar)->iommu.lock)
#define	DMAR_ASSERT_LOCKED(dmar) mtx_assert(&(dmar)->iommu.lock, MA_OWNED)

#define	DMAR_FAULT_LOCK(dmar)	mtx_lock_spin(&(dmar)->fault_lock)
#define	DMAR_FAULT_UNLOCK(dmar)	mtx_unlock_spin(&(dmar)->fault_lock)
#define	DMAR_FAULT_ASSERT_LOCKED(dmar) mtx_assert(&(dmar)->fault_lock, MA_OWNED)

#define	DMAR_IS_COHERENT(dmar)	(((dmar)->hw_ecap & DMAR_ECAP_C) != 0)
#define	DMAR_HAS_QI(dmar)	(((dmar)->hw_ecap & DMAR_ECAP_QI) != 0)
#define	DMAR_X2APIC(dmar) \
	(x2apic_mode && ((dmar)->hw_ecap & DMAR_ECAP_EIM) != 0)

/* Barrier ids */
#define	DMAR_BARRIER_RMRR	0
#define	DMAR_BARRIER_USEQ	1

struct dmar_unit *dmar_find(device_t dev, bool verbose);
struct dmar_unit *dmar_find_hpet(device_t dev, uint16_t *rid);
struct dmar_unit *dmar_find_ioapic(u_int apic_id, uint16_t *rid);

u_int dmar_nd2mask(u_int nd);
bool dmar_pglvl_supported(struct dmar_unit *unit, int pglvl);
int domain_set_agaw(struct dmar_domain *domain, int mgaw);
int dmar_maxaddr2mgaw(struct dmar_unit *unit, iommu_gaddr_t maxaddr,
    bool allow_less);
vm_pindex_t pglvl_max_pages(int pglvl);
int domain_is_sp_lvl(struct dmar_domain *domain, int lvl);
iommu_gaddr_t pglvl_page_size(int total_pglvl, int lvl);
iommu_gaddr_t domain_page_size(struct dmar_domain *domain, int lvl);
int calc_am(struct dmar_unit *unit, iommu_gaddr_t base, iommu_gaddr_t size,
    iommu_gaddr_t *isizep);
struct vm_page *dmar_pgalloc(vm_object_t obj, vm_pindex_t idx, int flags);
void dmar_pgfree(vm_object_t obj, vm_pindex_t idx, int flags);
void *dmar_map_pgtbl(vm_object_t obj, vm_pindex_t idx, int flags,
    struct sf_buf **sf);
void dmar_unmap_pgtbl(struct sf_buf *sf);
int dmar_load_root_entry_ptr(struct dmar_unit *unit);
int dmar_inv_ctx_glob(struct dmar_unit *unit);
int dmar_inv_iotlb_glob(struct dmar_unit *unit);
int dmar_flush_write_bufs(struct dmar_unit *unit);
void dmar_flush_pte_to_ram(struct dmar_unit *unit, dmar_pte_t *dst);
void dmar_flush_ctx_to_ram(struct dmar_unit *unit, dmar_ctx_entry_t *dst);
void dmar_flush_root_to_ram(struct dmar_unit *unit, dmar_root_entry_t *dst);
int dmar_enable_translation(struct dmar_unit *unit);
int dmar_disable_translation(struct dmar_unit *unit);
int dmar_load_irt_ptr(struct dmar_unit *unit);
int dmar_enable_ir(struct dmar_unit *unit);
int dmar_disable_ir(struct dmar_unit *unit);
bool dmar_barrier_enter(struct dmar_unit *dmar, u_int barrier_id);
void dmar_barrier_exit(struct dmar_unit *dmar, u_int barrier_id);
uint64_t dmar_get_timeout(void);
void dmar_update_timeout(uint64_t newval);

int dmar_fault_intr(void *arg);
void dmar_enable_fault_intr(struct dmar_unit *unit);
void dmar_disable_fault_intr(struct dmar_unit *unit);
int dmar_init_fault_log(struct dmar_unit *unit);
void dmar_fini_fault_log(struct dmar_unit *unit);

int dmar_qi_intr(void *arg);
void dmar_enable_qi_intr(struct dmar_unit *unit);
void dmar_disable_qi_intr(struct dmar_unit *unit);
int dmar_init_qi(struct dmar_unit *unit);
void dmar_fini_qi(struct dmar_unit *unit);
void dmar_qi_invalidate_locked(struct dmar_domain *domain,
    struct iommu_map_entry *entry, bool emit_wait);
void dmar_qi_invalidate_sync(struct dmar_domain *domain, iommu_gaddr_t start,
    iommu_gaddr_t size, bool cansleep);
void dmar_qi_invalidate_ctx_glob_locked(struct dmar_unit *unit);
void dmar_qi_invalidate_iotlb_glob_locked(struct dmar_unit *unit);
void dmar_qi_invalidate_iec_glob(struct dmar_unit *unit);
void dmar_qi_invalidate_iec(struct dmar_unit *unit, u_int start, u_int cnt);

vm_object_t domain_get_idmap_pgtbl(struct dmar_domain *domain,
    iommu_gaddr_t maxaddr);
void put_idmap_pgtbl(vm_object_t obj);
void domain_flush_iotlb_sync(struct dmar_domain *domain, iommu_gaddr_t base,
    iommu_gaddr_t size);
int domain_alloc_pgtbl(struct dmar_domain *domain);
void domain_free_pgtbl(struct dmar_domain *domain);
extern const struct iommu_domain_map_ops dmar_domain_map_ops;

int dmar_dev_depth(device_t child);
void dmar_dev_path(device_t child, int *busno, void *path1, int depth);

struct dmar_ctx *dmar_get_ctx_for_dev(struct dmar_unit *dmar, device_t dev,
    uint16_t rid, bool id_mapped, bool rmrr_init);
struct dmar_ctx *dmar_get_ctx_for_devpath(struct dmar_unit *dmar, uint16_t rid,
    int dev_domain, int dev_busno, const void *dev_path, int dev_path_len,
    bool id_mapped, bool rmrr_init);
int dmar_move_ctx_to_domain(struct dmar_domain *domain, struct dmar_ctx *ctx);
void dmar_free_ctx_locked(struct dmar_unit *dmar, struct dmar_ctx *ctx);
void dmar_free_ctx(struct dmar_ctx *ctx);
struct dmar_ctx *dmar_find_ctx_locked(struct dmar_unit *dmar, uint16_t rid);
void dmar_domain_free_entry(struct iommu_map_entry *entry, bool free);

void dmar_dev_parse_rmrr(struct dmar_domain *domain, int dev_domain,
    int dev_busno, const void *dev_path, int dev_path_len,
    struct iommu_map_entries_tailq *rmrr_entries);
int dmar_instantiate_rmrr_ctxs(struct iommu_unit *dmar);

void dmar_quirks_post_ident(struct dmar_unit *dmar);
void dmar_quirks_pre_use(struct iommu_unit *dmar);

int dmar_init_irt(struct dmar_unit *unit);
void dmar_fini_irt(struct dmar_unit *unit);

extern iommu_haddr_t dmar_high;
extern int haw;
extern int dmar_tbl_pagecnt;
extern int dmar_batch_coalesce;
extern int dmar_rmrr_enable;

static inline uint32_t
dmar_read4(const struct dmar_unit *unit, int reg)
{

	return (bus_read_4(unit->regs, reg));
}

static inline uint64_t
dmar_read8(const struct dmar_unit *unit, int reg)
{
#ifdef __i386__
	uint32_t high, low;

	low = bus_read_4(unit->regs, reg);
	high = bus_read_4(unit->regs, reg + 4);
	return (low | ((uint64_t)high << 32));
#else
	return (bus_read_8(unit->regs, reg));
#endif
}

static inline void
dmar_write4(const struct dmar_unit *unit, int reg, uint32_t val)
{

	KASSERT(reg != DMAR_GCMD_REG || (val & DMAR_GCMD_TE) ==
	    (unit->hw_gcmd & DMAR_GCMD_TE),
	    ("dmar%d clearing TE 0x%08x 0x%08x", unit->iommu.unit,
	    unit->hw_gcmd, val));
	bus_write_4(unit->regs, reg, val);
}

static inline void
dmar_write8(const struct dmar_unit *unit, int reg, uint64_t val)
{

	KASSERT(reg != DMAR_GCMD_REG, ("8byte GCMD write"));
#ifdef __i386__
	uint32_t high, low;

	low = val;
	high = val >> 32;
	bus_write_4(unit->regs, reg, low);
	bus_write_4(unit->regs, reg + 4, high);
#else
	bus_write_8(unit->regs, reg, val);
#endif
}

/*
 * dmar_pte_store and dmar_pte_clear ensure that on i386, 32bit writes
 * are issued in the correct order.  For store, the lower word,
 * containing the P or R and W bits, is set only after the high word
 * is written.  For clear, the P bit is cleared first, then the high
 * word is cleared.
 *
 * dmar_pte_update updates the pte.  For amd64, the update is atomic.
 * For i386, it first disables the entry by clearing the word
 * containing the P bit, and then defer to dmar_pte_store.  The locked
 * cmpxchg8b is probably available on any machine having DMAR support,
 * but interrupt translation table may be mapped uncached.
 */
static inline void
dmar_pte_store1(volatile uint64_t *dst, uint64_t val)
{
#ifdef __i386__
	volatile uint32_t *p;
	uint32_t hi, lo;

	hi = val >> 32;
	lo = val;
	p = (volatile uint32_t *)dst;
	*(p + 1) = hi;
	*p = lo;
#else
	*dst = val;
#endif
}

static inline void
dmar_pte_store(volatile uint64_t *dst, uint64_t val)
{

	KASSERT(*dst == 0, ("used pte %p oldval %jx newval %jx",
	    dst, (uintmax_t)*dst, (uintmax_t)val));
	dmar_pte_store1(dst, val);
}

static inline void
dmar_pte_update(volatile uint64_t *dst, uint64_t val)
{

#ifdef __i386__
	volatile uint32_t *p;

	p = (volatile uint32_t *)dst;
	*p = 0;
#endif
	dmar_pte_store1(dst, val);
}

static inline void
dmar_pte_clear(volatile uint64_t *dst)
{
#ifdef __i386__
	volatile uint32_t *p;

	p = (volatile uint32_t *)dst;
	*p = 0;
	*(p + 1) = 0;
#else
	*dst = 0;
#endif
}

extern struct timespec dmar_hw_timeout;

#define	DMAR_WAIT_UNTIL(cond)					\
{								\
	struct timespec last, curr;				\
	bool forever;						\
								\
	if (dmar_hw_timeout.tv_sec == 0 &&			\
	    dmar_hw_timeout.tv_nsec == 0) {			\
		forever = true;					\
	} else {						\
		forever = false;				\
		nanouptime(&curr);				\
		timespecadd(&curr, &dmar_hw_timeout, &last);	\
	}							\
	for (;;) {						\
		if (cond) {					\
			error = 0;				\
			break;					\
		}						\
		nanouptime(&curr);				\
		if (!forever && timespeccmp(&last, &curr, <)) {	\
			error = ETIMEDOUT;			\
			break;					\
		}						\
		cpu_spinwait();					\
	}							\
}

#ifdef INVARIANTS
#define	TD_PREP_PINNED_ASSERT						\
	int old_td_pinned;						\
	old_td_pinned = curthread->td_pinned
#define	TD_PINNED_ASSERT						\
	KASSERT(curthread->td_pinned == old_td_pinned,			\
	    ("pin count leak: %d %d %s:%d", curthread->td_pinned,	\
	    old_td_pinned, __FILE__, __LINE__))
#else
#define	TD_PREP_PINNED_ASSERT
#define	TD_PINNED_ASSERT
#endif

#endif
