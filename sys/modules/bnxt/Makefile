SUBDIR=	bnxt_en

# MNBSD-KPI: bnxt_re needs the Linux 5.7-based ibcore (FreeBSD b633e08c705f:
# ib_device ops, rdma/uverbs_ioctl.h, new verb signatures) and the
# if_getlinkstate() IfAPI accessor, which this tree does not have yet.
# FreeBSD stable/15 has:
#SUBDIR=	bnxt_en	\
#	bnxt_re

.include <bsd.subdir.mk>
