/*
 * $FreeBSD: stable/11/sys/contrib/rdma/krping/krping.h 353181 2019-10-07 08:28:55Z hselasky $
 */

struct krping_stats {
	unsigned long long send_bytes;
	unsigned long long send_msgs;
	unsigned long long recv_bytes;
	unsigned long long recv_msgs;
	unsigned long long write_bytes;
	unsigned long long write_msgs;
	unsigned long long read_bytes;
	unsigned long long read_msgs;
	char name[16];
};

int krping_doit(char *);
void krping_walk_cb_list(void (*)(struct krping_stats *, void *), void *);
int krping_sigpending(void);
void krping_cancel_all(void);
