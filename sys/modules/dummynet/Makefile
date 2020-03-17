# $FreeBSD: stable/11/sys/modules/dummynet/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:  ${SRCTOP}/sys/netpfil/ipfw
KMOD=   dummynet
SRCS=   ip_dummynet.c
SRCS+= ip_dn_glue.c ip_dn_io.c
SRCS+= dn_aqm_codel.c dn_aqm_pie.c
SRCS+= dn_heap.c dn_sched_fifo.c dn_sched_qfq.c dn_sched_rr.c dn_sched_wf2q.c 
SRCS+= dn_sched_prio.c dn_sched_fq_codel.c dn_sched_fq_pie.c
SRCS+=	opt_inet6.h

.include <bsd.kmod.mk>
