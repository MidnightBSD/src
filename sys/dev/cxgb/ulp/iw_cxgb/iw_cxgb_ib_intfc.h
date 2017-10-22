#ifndef  __IB_INTFC_H__
#define  __IB_INTFC_H__

/* $FreeBSD: release/10.0.0/sys/dev/cxgb/ulp/iw_cxgb/iw_cxgb_ib_intfc.h 237263 2012-06-19 07:34:13Z np $ */

#undef prefetch
#undef WARN_ON
#undef max_t
#undef udelay
#undef le32_to_cpu
#undef le16_to_cpu
#undef cpu_to_le32
#undef swab32
#undef container_of

#undef LIST_HEAD
#define LIST_HEAD(name, type)                                           \
struct name {                                                           \
        struct type *lh_first;  /* first element */                     \
}

#endif /* __IB_INTFC_H__ */
