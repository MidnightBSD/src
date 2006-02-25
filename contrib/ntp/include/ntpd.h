/*
 * ntpd.h - Prototypes for ntpd.
 */

#include "ntp_syslog.h"
#include "ntp_fp.h"
#include "ntp.h"
#include "ntp_select.h"
#include "ntp_malloc.h"
#include "ntp_refclock.h"
#include "recvbuff.h"

#ifdef SYS_WINNT
#define exit service_exit
extern	void	service_exit	(int);
/*	declare the service threads */
void	service_main	(DWORD, LPTSTR *);
void	service_ctrl	(DWORD);
void	worker_thread	(void *);
#define sleep(x) Sleep((DWORD) x * 1000 /* milliseconds */ );
#endif /* SYS_WINNT */

/* ntp_config.c */
extern	void	getconfig	P((int, char **));

/* ntp_config.c */
extern	void	ctl_clr_stats	P((void));
extern	int	ctlclrtrap	P((struct sockaddr_storage *, struct interface *, int));
extern	u_short ctlpeerstatus	P((struct peer *));
extern	int	ctlsettrap	P((struct sockaddr_storage *, struct interface *, int, int));
extern	u_short ctlsysstatus	P((void));
extern	void	init_control	P((void));
extern	void	process_control P((struct recvbuf *, int));
extern	void	report_event	P((int, struct peer *));

extern	double	fabs		P((double));
extern	double	sqrt		P((double));

/* ntp_control.c */
/*
 * Structure for translation tables between internal system
 * variable indices and text format.
 */
struct ctl_var {
	u_short code;
	u_short flags;
	char *text;
};
/*
 * Flag values
 */
#define	CAN_READ	0x01
#define	CAN_WRITE	0x02

#define DEF		0x20
#define	PADDING		0x40
#define	EOV		0x80

#define	RO	(CAN_READ)
#define	WO	(CAN_WRITE)
#define	RW	(CAN_READ|CAN_WRITE)

extern  char *  add_var P((struct ctl_var **, u_long, u_short));
extern  void    free_varlist P((struct ctl_var *));
extern  void    set_var P((struct ctl_var **, const char *, u_long, u_short));
extern  void    set_sys_var P((char *, u_long, u_short));

/* ntp_intres.c */
extern	void	ntp_res_name	P((struct sockaddr_storage, u_short));
extern	void	ntp_res_recv	P((void));
extern	void	ntp_intres	P((void));

/* ntp_io.c */
extern	struct interface *findinterface P((struct sockaddr_storage *));
extern  struct interface *findbcastinter P((struct sockaddr_storage *));

extern	void	init_io 	P((void));
extern	void	input_handler	P((l_fp *));
extern	void	io_clr_stats	P((void));
extern	void	io_setbclient	P((void));
extern	void	io_unsetbclient P((void));
extern	void	io_multicast_add P((struct sockaddr_storage));
extern	void	io_multicast_del P((struct sockaddr_storage));
extern	void	kill_asyncio	 P((int));

extern	void	sendpkt 	P((struct sockaddr_storage *, struct interface *, int, struct pkt *, int));
#ifdef HAVE_SIGNALED_IO
extern	void	wait_for_signal P((void));
extern	void	unblock_io_and_alarm P((void));
extern	void	block_io_and_alarm P((void));
#endif

/* ntp_leap.c */
extern	void	init_leap	P((void));
extern	void	leap_process	P((void));
extern	int 	leap_setleap	P((int, int));
/*
 * there seems to be a bug in the IRIX 4 compiler which prevents
 * u_char from beeing used in prototyped functions.
 * This is also true AIX compiler.
 * So give up and define it to be int. WLJ
 */
extern	int	leap_actual P((int));

/* ntp_loopfilter.c */
extern	void	init_loopfilter P((void));
extern	int 	local_clock P((struct peer *, double, double));
extern	void	adj_host_clock	P((void));
extern	void	loop_config P((int, double));
extern	void	huffpuff	P((void));

/* ntp_monitor.c */
extern	void	init_mon	P((void));
extern	void	mon_start	P((int));
extern	void	mon_stop	P((int));
extern	void	ntp_monitor P((struct recvbuf *));

/* ntp_peer.c */
extern	void	init_peer	P((void));
extern	struct peer *findexistingpeer P((struct sockaddr_storage *, struct peer *, int));
extern	struct peer *findpeer	P((struct sockaddr_storage *, struct interface *, int, int, int *));
extern	struct peer *findpeerbyassoc P((u_int));
extern	struct peer *newpeer	P((struct sockaddr_storage *, struct interface *, int, int, int, int, u_int, u_char, int, keyid_t));
extern	void	peer_all_reset	P((void));
extern	void	peer_clr_stats	P((void));
extern	struct peer *peer_config P((struct sockaddr_storage *, struct interface *, int, int, int, int, u_int, int, keyid_t, u_char *));
extern	void	peer_reset	P((struct peer *));
extern	int 	peer_unconfig	P((struct sockaddr_storage *, struct interface *, int));
extern	void	unpeer		P((struct peer *));
extern	void	clear_all	P((void));
#ifdef OPENSSL
extern	void	expire_all	P((void));
#endif /* OPENSSL */
extern	struct	peer *findmanycastpeer	P((struct recvbuf *));
extern	void	resetmanycast	P((void));

/* ntp_crypto.c */
#ifdef OPENSSL
extern	int	crypto_recv	P((struct peer *, struct recvbuf *));
extern	int	crypto_xmit	P((struct pkt *, struct sockaddr_storage *, int, struct exten *, keyid_t));
extern	keyid_t	session_key	P((struct sockaddr_storage *, struct sockaddr_storage *, keyid_t, keyid_t, u_long));
extern	void	make_keylist	P((struct peer *, struct interface *));
extern	void	key_expire	P((struct peer *));
extern	void	crypto_update	P((void));
extern	void	crypto_config	P((int, char *));
extern	void	crypto_setup	P((void));
extern	u_int	crypto_ident	P((struct peer *));
extern	struct exten *crypto_args P((struct peer *, u_int, char *));
extern	int	crypto_public	P((struct peer *, u_char *, u_int));
extern	void	value_free	P((struct value *));
#endif /* OPENSSL */

/* ntp_proto.c */
extern	void	transmit	P((struct peer *));
extern	void	receive 	P((struct recvbuf *));
extern	void	peer_clear	P((struct peer *, char *));
extern	void 	process_packet	P((struct peer *, struct pkt *, l_fp *));
extern	void	clock_select	P((void));
extern	void	kod_proto	P((void));

/*
 * there seems to be a bug in the IRIX 4 compiler which prevents
 * u_char from beeing used in prototyped functions.
 * This is also true AIX compiler.
 * So give up and define it to be int. WLJ
 */
extern	void	poll_update P((struct peer *, int));

extern	void	clear		P((struct peer *));
extern	void	clock_filter	P((struct peer *, double, double, double));
extern	void	init_proto	P((void));
extern	void	proto_config	P((int, u_long, double, struct sockaddr_storage*));
extern	void	proto_clr_stats P((void));

#ifdef	REFCLOCK
/* ntp_refclock.c */
extern	int	refclock_newpeer P((struct peer *));
extern	void	refclock_unpeer P((struct peer *));
extern	void	refclock_receive P((struct peer *));
extern	void	refclock_transmit P((struct peer *));
extern	void	init_refclock	P((void));
#endif	/* REFCLOCK */

/* ntp_request.c */
extern	void	init_request	P((void));
extern	void	process_private P((struct recvbuf *, int));

/* ntp_restrict.c */
extern	void	init_restrict	P((void));
extern	int 	restrictions	P((struct sockaddr_storage *));
extern	void	hack_restrict	P((int, struct sockaddr_storage *, struct sockaddr_storage *, int, int));

/* ntp_timer.c */
extern	void	init_timer	P((void));
extern	void	reinit_timer	P((void));
extern	void	timer		P((void));
extern	void	timer_clr_stats P((void));
#ifdef OPENSSL
extern	char	*sys_hostname;
extern	l_fp	sys_revoketime;
#endif /* OPENSSL */

/* ntp_util.c */
extern	void	init_util	P((void));
extern	void	hourly_stats	P((void));
extern	void	stats_config	P((int, char *));
extern	void	record_peer_stats P((struct sockaddr_storage *, int, double, double, double, double));
extern	void	record_loop_stats P((double, double, double, double, int));
extern	void	record_clock_stats P((struct sockaddr_storage *, const char *));
extern	void	record_raw_stats P((struct sockaddr_storage *, struct sockaddr_storage *, l_fp *, l_fp *, l_fp *, l_fp *));
extern	void	record_sys_stats P((void));
extern	void	record_crypto_stats P((struct sockaddr_storage *, const char *));
extern  int	sock_hash P((struct sockaddr_storage *));

/*
 * Variable declarations for ntpd.
 */

/* ntp_config.c */
extern char const *	progname;
extern char	sys_phone[][MAXDIAL];	/* ACTS phone numbers */
extern char	pps_device[];		/* PPS device name */
#if defined(HAVE_SCHED_SETSCHEDULER)
extern int	config_priority_override;
extern int	config_priority;
#endif

/* ntp_control.c */
struct ctl_trap;
extern struct ctl_trap ctl_trap[];
extern int	num_ctl_traps;
extern keyid_t	ctl_auth_keyid;		/* keyid used for authenticating write requests */

/*
 * Statistic counters to keep track of requests and responses.
 */
extern u_long	ctltimereset;		/* time stats reset */
extern u_long	numctlreq;		/* number of requests we've received */
extern u_long	numctlbadpkts;		/* number of bad control packets */
extern u_long	numctlresponses; 	/* number of resp packets sent with data */
extern u_long	numctlfrags; 		/* number of fragments sent */
extern u_long	numctlerrors;		/* number of error responses sent */
extern u_long	numctltooshort;		/* number of too short input packets */
extern u_long	numctlinputresp; 	/* number of responses on input */
extern u_long	numctlinputfrag; 	/* number of fragments on input */
extern u_long	numctlinputerr;		/* number of input pkts with err bit set */
extern u_long	numctlbadoffset; 	/* number of input pkts with nonzero offset */
extern u_long	numctlbadversion;	/* number of input pkts with unknown version */
extern u_long	numctldatatooshort;	/* data too short for count */
extern u_long	numctlbadop; 		/* bad op code found in packet */
extern u_long	numasyncmsgs;		/* number of async messages we've sent */

/* ntp_intres.c */
extern keyid_t	req_keyid;		/* request keyid */
extern char *	req_file;		/* name of the file with configuration info */

/*
 * Other statistics of possible interest
 */
extern volatile u_long packets_dropped;	/* total number of packets dropped on reception */
extern volatile u_long packets_ignored;	/* packets received on wild card interface */
extern volatile u_long packets_received;/* total number of packets received */
extern u_long	packets_sent;		/* total number of packets sent */
extern u_long	packets_notsent; 	/* total number of packets which couldn't be sent */

extern volatile u_long handler_calls;	/* number of calls to interrupt handler */
extern volatile u_long handler_pkts;	/* number of pkts received by handler */
extern u_long	io_timereset;		/* time counters were reset */

/*
 * Interface stuff
 */
extern struct interface *any_interface;	/* default ipv4 interface */
extern struct interface *any6_interface;/* default ipv6 interface */
extern struct interface *loopback_interface; /* loopback interface */

/*
 * File descriptor masks etc. for call to select
 */
extern fd_set	activefds;
extern int	maxactivefd;

/* ntp_loopfilter.c */
extern double	drift_comp;		/* clock frequency (s/s) */
extern double	clock_stability;	/* clock stability (s/s) */
extern double	clock_max;		/* max offset before step (s) */
extern double	clock_panic;		/* max offset before panic (s) */
extern double	clock_phi;		/* dispersion rate (s/s) */
extern double	clock_minstep;		/* step timeout (s) */
extern u_long	pps_control;		/* last pps sample time */
#ifdef KERNEL_PLL
extern int	pll_status;		/* status bits for kernel pll */
#endif /* KERNEL_PLL */

/*
 * Clock state machine control flags
 */
extern int	ntp_enable;		/* clock discipline enabled */
extern int	pll_control;		/* kernel support available */
extern int	kern_enable;		/* kernel support enabled */
extern int	pps_enable;		/* kernel PPS discipline enabled */
extern int	ext_enable;		/* external clock enabled */
extern int	cal_enable;		/* refclock calibrate enable */
extern int	allow_panic;		/* allow panic correction */
extern int	mode_ntpdate;		/* exit on first clock set */
extern int	peer_ntpdate;		/* count of ntpdate peers */
extern int	forground_process;	/* run the process in the forground */

/*
 * Clock state machine variables
 */
extern u_char	sys_poll;		/* system poll interval (log2 s) */
extern int	state;			/* clock discipline state */
extern int	tc_counter;		/* poll-adjust counter */
extern u_long	last_time;		/* time of last clock update (s) */
extern double	last_offset;		/* last clock offset (s) */
extern double	allan_xpt;		/* Allan intercept (s) */
extern double	sys_error;		/* system RMS error (s) */
extern double	sys_jitter;		/* system RMS jitter (s) */

/* ntp_monitor.c */
extern struct mon_data mon_mru_list;
extern struct mon_data mon_fifo_list;
extern int	mon_enabled;

/* ntp_peer.c */
extern struct peer *peer_hash[];	/* peer hash table */
extern int	peer_hash_count[];	/* count of peers in each bucket */
extern struct peer *assoc_hash[];	/* association ID hash table */
extern int	assoc_hash_count[];
extern int	peer_free_count;

/*
 * Miscellaneous statistic counters which may be queried.
 */
extern u_long	peer_timereset;		/* time stat counters were zeroed */
extern u_long	findpeer_calls;		/* number of calls to findpeer */
extern u_long	assocpeer_calls;	/* number of calls to findpeerbyassoc */
extern u_long	peer_allocations;	/* number of allocations from the free list */
extern u_long	peer_demobilizations;	/* number of structs freed to free list */
extern int	total_peer_structs;	/* number of peer structs in circulation */
extern int	peer_associations;	/* number of active associations */

/* ntp_proto.c */
/*
 * System variables are declared here.	See Section 3.2 of the
 * specification.
 */
extern u_char	sys_leap;		/* system leap indicator */
extern u_char	sys_stratum;		/* stratum of system */
extern s_char	sys_precision;		/* local clock precision */
extern double	sys_rootdelay;		/* distance to current sync source */
extern double	sys_rootdispersion;	/* dispersion of system clock */
extern u_int32	sys_refid;		/* reference source for local clock */
extern l_fp	sys_reftime;		/* time we were last updated */
extern struct peer *sys_peer;		/* our current peer */
extern struct peer *sys_prefer;		/* our cherished peer */
extern u_long	sys_automax;		/* maximum session key lifetime */

/*
 * Nonspecified system state variables.
 */
extern int	sys_bclient;		/* we set our time to broadcasts */
extern double	sys_bdelay; 		/* broadcast client default delay */
extern int	sys_authenticate;	/* requre authentication for config */
extern l_fp	sys_authdelay;		/* authentication delay */
extern keyid_t	sys_private;		/* private value for session seed */
extern int	sys_manycastserver;	/* respond to manycast client pkts */
extern int	sys_minclock;		/* minimum survivors */
extern int	sys_minsane;		/* minimum candidates */
extern int	sys_floor;		/* cluster stratum floor */
extern int	sys_ceiling;		/* cluster stratum ceiling */
extern u_char	sys_ttl[];		/* ttl mapping vector */
extern int	sys_ttlmax;		/* max ttl mapping vector index */

/*
 * Statistics counters
 */
extern u_long	sys_stattime;		/* time when we started recording */
extern u_long	sys_restricted;	 	/* restricted packets */
extern u_long	sys_oldversionpkt;	/* old version packets */
extern u_long	sys_newversionpkt;	/* new version packets  */
extern u_long	sys_unknownversion;	/* don't know version packets */
extern u_long	sys_badlength;		/* bad length or format */
extern u_long	sys_processed;		/* packets processed */
extern u_long	sys_badauth;		/* bad authentication */
extern u_long	sys_limitrejected;	/* rate limit exceeded */
extern u_long	sys_received;		/* packets received */

/* ntp_refclock.c */
#ifdef REFCLOCK
#if defined(PPS) || defined(HAVE_PPSAPI)
extern int	fdpps;			/* pps file descriptor */
#endif /* PPS */
#endif

/* ntp_request.c */
extern keyid_t	info_auth_keyid;	/* keyid used to authenticate requests */

/* ntp_restrict.c */
extern struct restrictlist *restrictlist; /* the ipv4 restriction list */
extern struct restrictlist6 *restrictlist6; /* the ipv6 restriction list */
extern u_long	res_min_interval;
extern u_long	res_avg_interval;
extern u_long	mon_age;		/* monitor preempt age */

/* ntp_timer.c */
extern volatile int alarm_flag;		/* alarm flag */
extern u_char	sys_revoke;		/* keys revoke timeout (log2 s) */
extern volatile u_long alarm_overflow;
extern u_long	current_time;		/* current time (s) */
extern u_long	timer_timereset;
extern u_long	timer_overflows;
extern u_long	timer_xmtcalls;

/* ntp_util.c */
extern int	stats_control;		/* write stats to fileset? */

/* ntpd.c */
extern volatile int debug;		/* debugging flag */
extern int	nofork;			/* no-fork flag */
extern int 	initializing;		/* initializing flag */
#ifdef HAVE_CLOCKCTL
extern char *user;			/* user to switch to */
extern char *group;			/* group to switch to */
extern char *chrootdir;			/* directory to chroot to */
#endif

/* refclock_conf.c */
#ifdef REFCLOCK
extern struct refclock *refclock_conf[]; /* refclock configuration table */
extern u_char	num_refclock_conf;
#endif
