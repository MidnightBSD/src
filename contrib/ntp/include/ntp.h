/*
 * ntp.h - NTP definitions for the masses
 */
#ifndef NTP_H
#define NTP_H

#include "ntp_types.h"
#include <math.h>
#ifdef OPENSSL
#include "ntp_crypto.h"
#endif /* OPENSSL */

/*
 * Calendar arithmetic - contributed by G. Healton
 */
#define YEAR_BREAK 500		/* years < this are tm_year values:
				 * Break < AnyFourDigitYear && Break >
				 * Anytm_yearYear */

#define YEAR_PIVOT 98		/* 97/98: years < this are year 2000+
				 * FYI: official UNIX pivot year is
				 * 68/69 */

/*
 * Number of Days since 1 BC Gregorian to 1 January of given year
 */
#define julian0(year)	(((year) * 365 ) + ((year) > 0 ? (((year) + 3) \
			    / 4 - ((year - 1) / 100) + ((year - 1) / \
			    400)) : 0))

/*
 * Number of days since start of NTP time to 1 January of given year
 */
#define ntp0(year)	(julian0(year) - julian0(1900))

/*
 * Number of days since start of UNIX time to 1 January of given year
 */
#define unix0(year)	(julian0(year) - julian0(1970))

/*
 * LEAP YEAR test for full 4-digit years (e.g, 1999, 2010)
 */
#define isleap_4(y)	((y) % 4 == 0 && !((y) % 100 == 0 && !(y % \
			    400 == 0)))

/*
 * LEAP YEAR test for tm_year (struct tm) years (e.g, 99, 110)
 */
#define isleap_tm(y)	((y) % 4 == 0 && !((y) % 100 == 0 && !(((y) \
			    + 1900) % 400 == 0)))

/*
 * to convert simple two-digit years to tm_year style years:
 *
 *	if (year < YEAR_PIVOT)
 *		year += 100;
 *
 * to convert either two-digit OR tm_year years to four-digit years:
 *
 *	if (year < YEAR_PIVOT)
 *		year += 100;
 *
 *	if (year < YEAR_BREAK)
 *		year += 1900;
 */

/*
 * How to get signed characters.  On machines where signed char works,
 * use it. On machines where signed char doesn't work, char had better
 * be signed.
 */
#ifdef NEED_S_CHAR_TYPEDEF
# if SIZEOF_SIGNED_CHAR
typedef signed char s_char;
# else
typedef char s_char;
# endif
  /* XXX: Why is this sequent bit INSIDE this test? */
# ifdef sequent
#  undef SO_RCVBUF
#  undef SO_SNDBUF
# endif
#endif
#ifndef TRUE
# define TRUE 1
#endif /* TRUE */
#ifndef FALSE
# define FALSE 0
#endif /* FALSE */

/*
 * NTP protocol parameters.  See section 3.2.6 of the specification.
 */
#define	NTP_VERSION	((u_char)4) /* current version number */
#define	NTP_OLDVERSION	((u_char)1) /* oldest credible version */
#define	NTP_PORT	123	/* included for non-unix machines */

/*
 * Poll interval parameters
 */
#define NTP_UNREACH	16	/* poll interval backoff count */
#define	NTP_MINPOLL	4	/* log2 min poll interval (16 s) */
#define NTP_MINDPOLL	6	/* log2 default min poll (64 s) */
#define NTP_MAXDPOLL	10	/* log2 default max poll (~17 m) */
#define	NTP_MAXPOLL	17	/* log2 max poll interval (~36 h) */
#define NTP_BURST	8	/* packets in burst */
#define BURST_DELAY	2	/* interburst delay (s) */
#define	RESP_DELAY	1	/* crypto response delay (s) */

/*
 * Clock filter algorithm tuning parameters
 */
#define MINDISPERSE	.01	/* min dispersion */
#define MAXDISPERSE	16.	/* max dispersion */
#define	NTP_SHIFT	8	/* clock filter stages */
#define NTP_FWEIGHT	.5	/* clock filter weight */

/*
 * Selection algorithm tuning parameters
 */
#define	NTP_MINCLOCK	4	/* minimum survivors */
#define	NTP_MAXCLOCK	50	/* maximum candidates */
#define MAXDISTANCE	1.	/* max root distance */
#define CLOCK_SGATE	3.	/* popcorn spike gate */
#define HUFFPUFF	900	/* huff-n'-puff sample interval (s) */
#define HYST		.5	/* anti-clockhop hysteresis */
#define HYST_TC		.875	/* anti-clockhop hysteresis decay */
#define MAX_TTL		8	/* max ttl mapping vector size */
#define NTP_MAXEXTEN	1024	/* maximum extension field size */

/*
 * Miscellaneous stuff
 */
#define NTP_MAXKEY	65535	/* maximum authentication key number */

/*
 * Limits of things
 */
#define	MAXFILENAME	128	/* max length of file name */
#define MAXHOSTNAME	512	/* max length of host/node name */
#define NTP_MAXSTRLEN	256	/* maximum string length */
#define MAXINTERFACES	512	/* max number of interfaces */

/*
 * Operations for jitter calculations (these use doubles).
 *
 * Note that we carefully separate the jitter component from the
 * dispersion component (frequency error plus precision). The frequency
 * error component is computed as CLOCK_PHI times the difference between
 * the epoch of the time measurement and the reference time. The
 * precision componen is computed as the square root of the mean of the
 * squares of a zero-mean, uniform distribution of unit maximum
 * amplitude. Whether this makes statistical sense may be arguable.
 */
#define SQUARE(x) ((x) * (x))
#define SQRT(x) (sqrt(x))
#define DIFF(x, y) (SQUARE((x) - (y)))
#define LOGTOD(a)	((a) < 0 ? 1. / (1L << -(a)) : \
			    1L << (int)(a)) /* log2 to double */
#define UNIVAR(x)	(SQUARE(.28867513 * LOGTOD(x))) /* std uniform distr */
#define ULOGTOD(a)	(1L << (int)(a)) /* ulog2 to double */

#define	EVENT_TIMEOUT	0	/* one second, that is */

/*
 * The interface structure is used to hold the addresses and socket
 * numbers of each of the interfaces we are using.
 */
struct interface {
	SOCKET fd;			/* socket this is opened on */
	SOCKET bfd;		/* socket for receiving broadcasts */
	struct sockaddr_storage sin;	/* interface address */
	struct sockaddr_storage bcast; /* broadcast address */
	struct sockaddr_storage mask; /* interface mask */
	char name[32];		/* name of interface */
	int flags;		/* interface flags */
	int last_ttl;		/* last TTL specified */
	u_int addr_refid;	/* IPv4 addr or IPv6 hash */
	int num_mcast;		/* No. of IP addresses in multicast socket */
	volatile long received;	/* number of incoming packets */
	long sent;		/* number of outgoing packets */
	long notsent;		/* number of send failures */
};

/*
 * Flags for interfaces
 */
#define INT_UP		 1	/* Interface is up */
#define	INT_PPP		 2	/* Point-to-point interface */
#define	INT_LOOPBACK	 4	/* the loopback interface */
#define	INT_BROADCAST	 8	/* can broadcast out this interface */
#define INT_MULTICAST	16	/* multicasting enabled */
#define	INT_BCASTOPEN	32	/* broadcast socket is open */

/*
 * Define flasher bits (tests 1 through 11 in packet procedure)
 * These reveal the state at the last grumble from the peer and are
 * most handy for diagnosing problems, even if not strictly a state
 * variable in the spec. These are recorded in the peer structure.
 */
#define TEST1		0x0001	/* duplicate packet received */
#define TEST2		0x0002	/* bogus packet received */
#define TEST3		0x0004	/* protocol unsynchronized */
#define TEST4		0x0008	/* access denied */
#define TEST5		0x0010	/* authentication failed */
#define TEST6		0x0020	/* peer clock unsynchronized */
#define TEST7		0x0040	/* peer stratum out of bounds */
#define TEST8		0x0080  /* root delay/dispersion bounds check */
#define TEST9		0x0100	/* peer delay/dispersion bounds check */
#define TEST10		0x0200	/* autokey failed */
#define	TEST11		0x0400	/* proventic not confirmed */

/*
 * The peer structure. Holds state information relating to the guys
 * we are peering with. Most of this stuff is from section 3.2 of the
 * spec.
 */
struct peer {
	struct peer *next;	/* pointer to next association */
	struct peer *ass_next;	/* link pointer in associd hash */
	struct sockaddr_storage srcadr; /* address of remote host */
	struct interface *dstadr; /* pointer to address on local host */
	associd_t associd;	/* association ID */
	u_char	version;	/* version number */
	u_char	hmode;		/* local association mode */
	u_char	hpoll;		/* local poll interval */
	u_char	kpoll;		/* last poll interval */
	u_char	minpoll;	/* min poll interval */
	u_char	maxpoll;	/* max poll interval */
	u_char	burst;		/* packets remaining in burst */
	u_int	flags;		/* association flags */
	u_char	cast_flags;	/* additional flags */
	u_int	flash;		/* protocol error test tally bits */
	u_char	last_event;	/* last peer error code */
	u_char	num_events;	/* number of error events */
	u_char	ttl;		/* ttl/refclock mode */

	/*
	 * Variables used by reference clock support
	 */
	struct refclockproc *procptr; /* refclock structure pointer */
	u_char	refclktype;	/* reference clock type */
	u_char	refclkunit;	/* reference clock unit number */
	u_char	sstclktype;	/* clock type for system status word */

	/*
	 * Variables set by received packet
	 */
	u_char	leap;		/* local leap indicator */
	u_char	pmode;		/* remote association mode */
	u_char	stratum;	/* remote stratum */
	s_char	precision;	/* remote clock precision */
	u_char	ppoll;		/* remote poll interval */
	u_int32	refid;		/* remote reference ID */
	l_fp	reftime;	/* update epoch */

	/*
	 * Variables used by authenticated client
	 */
	keyid_t keyid;		/* current key ID */
#ifdef OPENSSL
#define clear_to_zero assoc
	associd_t assoc;	/* peer association ID */
	u_int32	crypto;		/* peer status word */
	EVP_PKEY *pkey;		/* public key */
	const EVP_MD *digest;	/* message digest algorithm */
	char	*subject;	/* certificate subject name */
	char	*issuer;	/* certificate issuer name */
	keyid_t	pkeyid;		/* previous key ID */
	keyid_t	pcookie;	/* peer cookie */
	EVP_PKEY *ident_pkey;	/* identity key */
	tstamp_t fstamp;	/* identity filestamp */
	BIGNUM	*iffval;	/* IFF/GQ challenge */
	BIGNUM	*grpkey;	/* GQ group key */
	struct value cookval;	/* cookie values */
	struct value recval;	/* receive autokey values */
	struct value tai_leap;	/* leapseconds values */
	struct exten *cmmd;	/* extension pointer */

	/*
	 * Variables used by authenticated server
	 */
	keyid_t	*keylist;	/* session key ID list */
	int	keynumber;	/* current key number */
	struct value encrypt;	/* send encrypt values */
	struct value sndval;	/* send autokey values */
#else /* OPENSSL */
#define clear_to_zero status
#endif /* OPENSSL */

	/*
	 * Ephemeral state variables
	 */
	u_char	status;		/* peer status */
	u_char	reach;		/* reachability register */
	u_long	epoch;		/* reference epoch */
	u_short	filter_nextpt;	/* index into filter shift register */
	double	filter_delay[NTP_SHIFT]; /* delay shift register */
	double	filter_offset[NTP_SHIFT]; /* offset shift register */
	double	filter_disp[NTP_SHIFT]; /* dispersion shift register */
	u_long	filter_epoch[NTP_SHIFT]; /* epoch shift register */
	u_char	filter_order[NTP_SHIFT]; /* filter sort index */
	l_fp	org;		/* originate time stamp */
	l_fp	rec;		/* receive time stamp */
	l_fp	xmt;		/* transmit time stamp */
	double	offset;		/* peer clock offset */
	double	delay;		/* peer roundtrip delay */
	double	jitter;		/* peer jitter (squares) */
	double	disp;		/* peer dispersion */
	double	estbdelay;	/* clock offset to broadcast server */
	double	hyst;		/* anti-clockhop hysteresis */

	/*
	 * Variables set by received packet
	 */
	double	rootdelay;	/* roundtrip delay to primary clock */
	double	rootdispersion;	/* dispersion to primary clock */

	/*
	 * End of clear-to-zero area
	 */
	u_long	update;		/* receive epoch */
#define end_clear_to_zero update
	u_int	unreach;	/* unreachable count */
	u_long	outdate;	/* send time last packet */
	u_long	nextdate;	/* send time next packet */
	u_long	nextaction;	/* peer local activity timeout (refclocks mainly) */
	void (*action) P((struct peer *)); /* action timeout function */
	/*
	 * Statistic counters
	 */
	u_long	timereset;	/* time stat counters were reset */
	u_long	timereceived;	/* last packet received time */
	u_long	timereachable;	/* last reachable/unreachable time */

	u_long	sent;		/* packets sent */
	u_long	received;	/* packets received */
	u_long	processed;	/* packets processed by the protocol */
	u_long	badauth;	/* packets cryptosum failed */
	u_long	bogusorg;	/* packets bogus origin */
	u_long	oldpkt;		/* packets duplicate packet */
	u_long	seldisptoolarge; /* packets dispersion to large*/
	u_long	selbroken;	/* not used */
	u_long	rank;	/* number of times selected or in cluster */
};

/*
 * Values for peer.leap, sys_leap
 */
#define	LEAP_NOWARNING	0x0	/* normal, no leap second warning */
#define	LEAP_ADDSECOND	0x1	/* last minute of day has 61 seconds */
#define	LEAP_DELSECOND	0x2	/* last minute of day has 59 seconds */
#define	LEAP_NOTINSYNC	0x3	/* overload, clock is free running */

/*
 * Values for peer.mode
 */
#define	MODE_UNSPEC	0	/* unspecified (old version) */
#define	MODE_ACTIVE	1	/* symmetric active */
#define	MODE_PASSIVE	2	/* symmetric passive */
#define	MODE_CLIENT	3	/* client mode */
#define	MODE_SERVER	4	/* server mode */
#define	MODE_BROADCAST	5	/* broadcast mode */
#define	MODE_CONTROL	6	/* control mode packet */
#define	MODE_PRIVATE	7	/* implementation defined function */
#define	MODE_BCLIENT	8	/* broadcast client mode */

/*
 * Values for peer.stratum, sys_stratum
 */
#define	STRATUM_REFCLOCK ((u_char)0) /* default stratum */
/* A stratum of 0 in the packet is mapped to 16 internally */
#define	STRATUM_PKT_UNSPEC ((u_char)0) /* unspecified in packet */
#define	STRATUM_UNSPEC	((u_char)16) /* unspecified */

/*
 * Values for peer.flags
 */
#define	FLAG_CONFIG	0x0001	/* association was configured */
#define	FLAG_AUTHENABLE	0x0002	/* authentication required */
#define	FLAG_AUTHENTIC	0x0004	/* last message was authentic */
#define FLAG_SKEY	0x0008  /* autokey authentication */
#define FLAG_MCAST	0x0010  /* multicast client mode */
#define	FLAG_REFCLOCK	0x0020	/* this is actually a reference clock */
#define	FLAG_SYSPEER	0x0040	/* this is one of the selected peers */
#define FLAG_PREFER	0x0080	/* this is the preferred peer */
#define FLAG_BURST	0x0100	/* burst mode */
#define FLAG_IBURST	0x0200	/* initial burst mode */
#define FLAG_NOSELECT	0x0400	/* this is a "noselect" peer */
#define FLAG_ASSOC	0x0800	/* autokey request */

/*
 * Definitions for the clear() routine.  We use memset() to clear
 * the parts of the peer structure which go to zero.  These are
 * used to calculate the start address and length of the area.
 */
#define	CLEAR_TO_ZERO(p)	((char *)&((p)->clear_to_zero))
#define	END_CLEAR_TO_ZERO(p)	((char *)&((p)->end_clear_to_zero))
#define	LEN_CLEAR_TO_ZERO	(END_CLEAR_TO_ZERO((struct peer *)0) \
				    - CLEAR_TO_ZERO((struct peer *)0))
#define CRYPTO_TO_ZERO(p)	((char *)&((p)->clear_to_zero))
#define END_CRYPTO_TO_ZERO(p)	((char *)&((p)->end_clear_to_zero))
#define LEN_CRYPTO_TO_ZERO	(END_CRYPTO_TO_ZERO((struct peer *)0) \
				    - CRYPTO_TO_ZERO((struct peer *)0))

/*
 * Reference clock identifiers (for pps signal)
 */
#define PPSREFID (u_int32)"PPS "	/* used when pps controls stratum>1 */

/*
 * Reference clock types.  Added as necessary.
 */
#define	REFCLK_NONE		0	/* unknown or missing */
#define	REFCLK_LOCALCLOCK	1	/* external (e.g., lockclock) */
#define	REFCLK_GPS_TRAK		2	/* TRAK 8810 GPS Receiver */
#define	REFCLK_WWV_PST		3	/* PST/Traconex 1020 WWV/H */
#define	REFCLK_SPECTRACOM	4	/* Spectracom (generic) Receivers */
#define	REFCLK_TRUETIME		5	/* TrueTime (generic) Receivers */
#define REFCLK_IRIG_AUDIO	6	/* IRIG-B/W audio decoder */
#define	REFCLK_CHU_AUDIO	7	/* CHU audio demodulator/decoder */
#define REFCLK_PARSE		8	/* generic driver (usually DCF77,GPS,MSF) */
#define	REFCLK_GPS_MX4200	9	/* Magnavox MX4200 GPS */
#define REFCLK_GPS_AS2201	10	/* Austron 2201A GPS */
#define	REFCLK_GPS_ARBITER	11	/* Arbiter 1088A/B/ GPS */
#define REFCLK_IRIG_TPRO	12	/* KSI/Odetics TPRO-S IRIG */
#define REFCLK_ATOM_LEITCH	13	/* Leitch CSD 5300 Master Clock */
#define REFCLK_MSF_EES		14	/* EES M201 MSF Receiver */
#define	REFCLK_GPSTM_TRUE	15	/* OLD TrueTime GPS/TM-TMD Receiver */
#define REFCLK_IRIG_BANCOMM	16	/* Bancomm GPS/IRIG Interface */
#define REFCLK_GPS_DATUM	17	/* Datum Programmable Time System */
#define REFCLK_NIST_ACTS	18	/* NIST Auto Computer Time Service */
#define REFCLK_WWV_HEATH	19	/* Heath GC1000 WWV/WWVH Receiver */
#define REFCLK_GPS_NMEA		20	/* NMEA based GPS clock */
#define REFCLK_GPS_VME		21	/* TrueTime GPS-VME Interface */
#define REFCLK_ATOM_PPS		22	/* 1-PPS Clock Discipline */
#define REFCLK_PTB_ACTS		23	/* PTB Auto Computer Time Service */
#define REFCLK_USNO		24	/* Naval Observatory dialup */
#define REFCLK_GPS_HP		26	/* HP 58503A Time/Frequency Receiver */
#define REFCLK_ARCRON_MSF	27	/* ARCRON MSF radio clock. */
#define REFCLK_SHM		28	/* clock attached thru shared memory */
#define REFCLK_PALISADE		29	/* Trimble Navigation Palisade GPS */
#define REFCLK_ONCORE		30	/* Motorola UT Oncore GPS */
#define REFCLK_GPS_JUPITER	31	/* Rockwell Jupiter GPS receiver */
#define REFCLK_CHRONOLOG	32	/* Chrono-log K WWVB receiver */
#define REFCLK_DUMBCLOCK	33	/* Dumb localtime clock */
#define REFCLK_ULINK		34	/* Ultralink M320 WWVB receiver */
#define REFCLK_PCF		35	/* Conrad parallel port radio clock */
#define REFCLK_WWV_AUDIO	36	/* WWV/H audio demodulator/decoder */
#define REFCLK_FG		37	/* Forum Graphic GPS */
#define REFCLK_HOPF_SERIAL	38	/* hopf DCF77/GPS serial receiver  */
#define REFCLK_HOPF_PCI		39	/* hopf DCF77/GPS PCI receiver  */
#define REFCLK_JJY		40	/* JJY receiver  */
#define	REFCLK_TT560		41	/* TrueTime 560 IRIG-B decoder */
#define REFCLK_ZYFER		42	/* Zyfer GPStarplus receiver  */
#define REFCLK_RIPENCC		43	/* RIPE NCC Trimble driver */
#define REFCLK_NEOCLOCK4X	44	/* NeoClock4X DCF77 or TDF receiver */
#define REFCLK_MAX		44	/* NeoClock4X DCF77 or TDF receiver */

 /*
 * Macro for sockaddr_storage structures operations
 */
#define SOCKCMP(sock1, sock2) \
	(((struct sockaddr_storage *)sock1)->ss_family \
	    == ((struct sockaddr_storage *)sock2)->ss_family ? \
 	((struct sockaddr_storage *)sock1)->ss_family == AF_INET ? \
 	memcmp(&((struct sockaddr_in *)sock1)->sin_addr, \
	    &((struct sockaddr_in *)sock2)->sin_addr, \
	    sizeof(struct in_addr)) == 0 : \
	memcmp(&((struct sockaddr_in6 *)sock1)->sin6_addr, \
	    &((struct sockaddr_in6 *)sock2)->sin6_addr, \
	    sizeof(struct in6_addr)) == 0 : \
	0)

#define SOCKNUL(sock1) \
	(((struct sockaddr_storage *)sock1)->ss_family == AF_INET ? \
 	(((struct sockaddr_in *)sock1)->sin_addr.s_addr == 0) : \
 	(IN6_IS_ADDR_UNSPECIFIED(&((struct sockaddr_in6 *)sock1)->sin6_addr)))

#define SOCKLEN(sock) \
	(((struct sockaddr_storage *)sock)->ss_family == AF_INET ? \
 	(sizeof(struct sockaddr_in)) : (sizeof(struct sockaddr_in6)))

#define ANYSOCK(sock) \
	memset(((struct sockaddr_in *)sock), 0, \
	    sizeof(struct sockaddr_storage))

#define ANY_INTERFACE_CHOOSE(sock) \
	(((struct sockaddr_storage *)sock)->ss_family == AF_INET ? \
 	any_interface : any6_interface)

/*
 * We tell reference clocks from real peers by giving the reference
 * clocks an address of the form 127.127.t.u, where t is the type and
 * u is the unit number.  We define some of this here since we will need
 * some sanity checks to make sure this address isn't interpretted as
 * that of a normal peer.
 */
#define	REFCLOCK_ADDR	0x7f7f0000	/* 127.127.0.0 */
#define	REFCLOCK_MASK	0xffff0000	/* 255.255.0.0 */

#define	ISREFCLOCKADR(srcadr)	((SRCADR(srcadr) & REFCLOCK_MASK) \
					== REFCLOCK_ADDR)

/*
 * Macro for checking for invalid addresses.  This is really, really
 * gross, but is needed so no one configures a host on net 127 now that
 * we're encouraging it the the configuration file.
 */
#define	LOOPBACKADR	0x7f000001
#define	LOOPNETMASK	0xff000000

#define	ISBADADR(srcadr)	(((SRCADR(srcadr) & LOOPNETMASK) \
				    == (LOOPBACKADR & LOOPNETMASK)) \
				    && (SRCADR(srcadr) != LOOPBACKADR))

/*
 * Utilities for manipulating addresses and port numbers
 */
#define	NSRCADR(src)	(((struct sockaddr_in *)src)->sin_addr.s_addr) /* address in net byte order */
#define	NSRCPORT(src)	(((struct sockaddr_in *)src)->sin_port)	/* port in net byte order */
#define	SRCADR(src)	(ntohl(NSRCADR((src))))	/* address in host byte order */
#define	SRCPORT(src)	(ntohs(NSRCPORT((src))))	/* host port */

#define CAST_V4(src)	((struct sockaddr_in *)&(src))
#define CAST_V6(src)	((struct sockaddr_in6 *)&(src))
#define GET_INADDR(src)  (CAST_V4(src)->sin_addr.s_addr)
#define GET_INADDR6(src) (CAST_V6(src)->sin6_addr)

#define SET_HOSTMASK(addr, family)	\
	do { \
		memset((char *)(addr), 0, sizeof(struct sockaddr_storage)); \
		(addr)->ss_family = (family); \
		if ((family) == AF_INET) \
			GET_INADDR(*(addr)) = 0xffffffff; \
		else \
			memset(&GET_INADDR6(*(addr)), 0xff, \
			    sizeof(struct in6_addr)); \
	} while(0)

/*
 * NTP packet format.  The mac field is optional.  It isn't really
 * an l_fp either, but for now declaring it that way is convenient.
 * See Appendix A in the specification.
 *
 * Note that all u_fp and l_fp values arrive in network byte order
 * and must be converted (except the mac, which isn't, really).
 */
struct pkt {
	u_char	li_vn_mode;	/* leap indicator, version and mode */
	u_char	stratum;	/* peer stratum */
	u_char	ppoll;		/* peer poll interval */
	s_char	precision;	/* peer clock precision */
	u_fp	rootdelay;	/* distance to primary clock */
	u_fp	rootdispersion;	/* clock dispersion */
	u_int32	refid;		/* reference clock ID */
	l_fp	reftime;	/* time peer clock was last updated */
	l_fp	org;		/* originate time stamp */
	l_fp	rec;		/* receive time stamp */
	l_fp	xmt;		/* transmit time stamp */

#define	LEN_PKT_NOMAC	12 * sizeof(u_int32) /* min header length */
#define	LEN_PKT_MAC	LEN_PKT_NOMAC +  sizeof(u_int32)
#define MIN_MAC_LEN	3 * sizeof(u_int32)	/* DES */
#define MAX_MAC_LEN	5 * sizeof(u_int32)	/* MD5 */

	/*
	 * The length of the packet less MAC must be a multiple of 64
	 * with an RSA modulus and Diffie-Hellman prime of 64 octets
	 * and maximum host name of 128 octets, the maximum autokey
	 * command is 152 octets and maximum autokey response is 460
	 * octets. A packet can contain no more than one command and one
	 * response, so the maximum total extension field length is 672
	 * octets. But, to handle humungus certificates, the bank must
	 * be broke.
	 */
#ifdef OPENSSL
	u_int32	exten[NTP_MAXEXTEN / 4]; /* max extension field */
#else /* OPENSSL */
	u_int32	exten[1];	/* misused */
#endif /* OPENSSL */
	u_char	mac[MAX_MAC_LEN]; /* mac */
};

/*
 * Stuff for extracting things from li_vn_mode
 */
#define	PKT_MODE(li_vn_mode)	((u_char)((li_vn_mode) & 0x7))
#define	PKT_VERSION(li_vn_mode)	((u_char)(((li_vn_mode) >> 3) & 0x7))
#define	PKT_LEAP(li_vn_mode)	((u_char)(((li_vn_mode) >> 6) & 0x3))

/*
 * Stuff for putting things back into li_vn_mode
 */
#define	PKT_LI_VN_MODE(li, vn, md) \
	((u_char)((((li) << 6) & 0xc0) | (((vn) << 3) & 0x38) | ((md) & 0x7)))


/*
 * Dealing with stratum.  0 gets mapped to 16 incoming, and back to 0
 * on output.
 */
#define	PKT_TO_STRATUM(s)	((u_char)(((s) == (STRATUM_PKT_UNSPEC)) ?\
				(STRATUM_UNSPEC) : (s)))

#define	STRATUM_TO_PKT(s)	((u_char)(((s) == (STRATUM_UNSPEC)) ?\
				(STRATUM_PKT_UNSPEC) : (s)))

/*
 * Event codes. Used for reporting errors/events to the control module
 */
#define	PEER_EVENT	0x080	/* this is a peer event */
#define CRPT_EVENT	0x100	/* this is a crypto event */

/*
 * System event codes
 */
#define	EVNT_UNSPEC	0	/* unspecified */
#define	EVNT_SYSRESTART	1	/* system restart */
#define	EVNT_SYSFAULT	2	/* wsystem or hardware fault */
#define	EVNT_SYNCCHG	3	/* new leap or synch change */
#define	EVNT_PEERSTCHG	4	/* new source or stratum */
#define	EVNT_CLOCKRESET	5	/* clock reset */
#define	EVNT_BADDATETIM	6	/* invalid time or date */
#define	EVNT_CLOCKEXCPT	7	/* reference clock exception */

/*
 * Peer event codes
 */
#define	EVNT_PEERIPERR	(1 | PEER_EVENT) /* IP error */
#define	EVNT_PEERAUTH	(2 | PEER_EVENT) /* authentication failure */
#define	EVNT_UNREACH	(3 | PEER_EVENT) /* change to unreachable */
#define	EVNT_REACH	(4 | PEER_EVENT) /* change to reachable */
#define	EVNT_PEERCLOCK	(5 | PEER_EVENT) /* clock exception */

/*
 * Clock event codes
 */
#define	CEVNT_NOMINAL	0	/* unspecified */
#define	CEVNT_TIMEOUT	1	/* poll timeout */
#define	CEVNT_BADREPLY	2	/* bad reply format */
#define	CEVNT_FAULT	3	/* hardware or software fault */
#define	CEVNT_PROP	4	/* propagation failure */
#define	CEVNT_BADDATE	5	/* bad date format or value */
#define	CEVNT_BADTIME	6	/* bad time format or value */
#define CEVNT_MAX	CEVNT_BADTIME

/*
 * Very misplaced value.  Default port through which we send traps.
 */
#define	TRAPPORT	18447


/*
 * To speed lookups, peers are hashed by the low order bits of the
 * remote IP address. These definitions relate to that.
 */
#define	HASH_SIZE	128
#define	HASH_MASK	(HASH_SIZE-1)
#define	HASH_ADDR(src)	sock_hash(src)

/*
 * How we randomize polls.  The poll interval is a power of two.
 * We chose a random value which is between 1/4 and 3/4 of the
 * poll interval we would normally use and which is an even multiple
 * of the EVENT_TIMEOUT.  The random number routine, given an argument
 * spread value of n, returns an integer between 0 and (1<<n)-1.  This
 * is shifted by EVENT_TIMEOUT and added to the base value.
 */
#if defined(HAVE_MRAND48)
# define RANDOM		(mrand48())
# define SRANDOM(x)	(srand48(x))
#else
# define RANDOM		(random())
# define SRANDOM(x)	(srandom(x))
#endif

#define RANDPOLL(x)	((1 << (x)) - 1 + (RANDOM & 0x3))
#define	RANDOM_SPREAD(poll)	((poll) - (EVENT_TIMEOUT+1))
#define	RANDOM_POLL(poll, rval)	((((rval)+1)<<EVENT_TIMEOUT) + (1<<((poll)-2)))

/*
 * min, min3 and max.  Makes it easier to transliterate the spec without
 * thinking about it.
 */
#define	min(a,b)	(((a) < (b)) ? (a) : (b))
#define	max(a,b)	(((a) > (b)) ? (a) : (b))
#define	min3(a,b,c)	min(min((a),(b)), (c))


/*
 * Configuration items.  These are for the protocol module (proto_config())
 */
#define	PROTO_BROADCLIENT	1
#define	PROTO_PRECISION		2	/* (not used) */
#define	PROTO_AUTHENTICATE	3
#define	PROTO_BROADDELAY	4
#define	PROTO_AUTHDELAY		5	/* (not used) */
#define PROTO_MULTICAST_ADD	6
#define PROTO_MULTICAST_DEL	7
#define PROTO_NTP		8
#define PROTO_KERNEL		9
#define PROTO_MONITOR		10
#define PROTO_FILEGEN		11
#define	PROTO_PPS		12
#define PROTO_CAL		13
#define PROTO_MINCLOCK		14
#define PROTO_MINSANE		15
#define PROTO_FLOOR		16
#define PROTO_CEILING		17
#define PROTO_COHORT		18
#define PROTO_CALLDELAY		19
#define PROTO_ADJ		20

/*
 * Configuration items for the loop filter
 */
#define	LOOP_DRIFTINIT		1	/* set initial frequency offset */
#define LOOP_DRIFTCOMP		2	/* set frequency offset */
#define LOOP_MAX		3	/* set step offset */
#define LOOP_PANIC		4	/* set panic offseet */
#define LOOP_PHI		5	/* set dispersion rate */
#define LOOP_MINSTEP		6	/* set step timeout */
#define LOOP_MINPOLL		7	/* set min poll interval (log2 s) */
#define LOOP_ALLAN		8	/* set minimum Allan intercept */
#define LOOP_HUFFPUFF		9	/* set huff-n'-puff filter length */
#define LOOP_FREQ		10	/* set initial frequency */

/*
 * Configuration items for the stats printer
 */
#define	STATS_FREQ_FILE		1	/* configure drift file */
#define STATS_STATSDIR		2	/* directory prefix for stats files */
#define	STATS_PID_FILE		3	/* configure ntpd PID file */

#define MJD_1900		15020	/* MJD for 1 Jan 1900 */

/*
 * Default parameters.  We use these in the absence of something better.
 */
#define	DEFBROADDELAY	4e-3		/* default broadcast offset */
#define INADDR_NTP	0xe0000101	/* NTP multicast address 224.0.1.1 */

/*
 * Structure used optionally for monitoring when this is turned on.
 */
struct mon_data {
	struct mon_data *hash_next;	/* next structure in hash list */
	struct mon_data *mru_next;	/* next structure in MRU list */
	struct mon_data *mru_prev;	/* previous structure in MRU list */
	u_long drop_count;		/* dropped due RESLIMIT*/
	double avg_interval;		/* average interpacket interval */
	u_long lasttime;		/* interval since last packet */
	u_long count;			/* total packet count */
	struct sockaddr_storage rmtadr;	/* address of remote host */
	struct interface *interface;	/* interface on which this arrived */
	u_short rmtport;		/* remote port last came from */
	u_char mode;			/* mode of incoming packet */
	u_char version;			/* version of incoming packet */
	u_char cast_flags;		/* flags MDF_?CAST */
};

/*
 * Values for cast_flags
 */
#define	MDF_UCAST	0x01		/* unicast */
#define	MDF_MCAST	0x02		/* multicast */
#define	MDF_BCAST	0x04		/* broadcast */
#define	MDF_LCAST	0x08		/* localcast */
#define MDF_ACAST	0x10		/* manycast */
#define	MDF_BCLNT	0x20		/* broadcast client */
#define MDF_ACLNT	0x40		/* manycast client */

/*
 * Values used with mon_enabled to indicate reason for enabling monitoring
 */
#define MON_OFF    0x00			/* no monitoring */
#define MON_ON     0x01			/* monitoring explicitly enabled */
#define MON_RES    0x02			/* implicit monitoring for RES_LIMITED */
/*
 * Structure used for restrictlist entries
 */
struct restrictlist {
	struct restrictlist *next;	/* link to next entry */
	u_int32 addr;			/* Ipv4 host address (host byte order) */
	u_int32 mask;			/* Ipv4 mask for address (host byte order) */
	u_long count;			/* number of packets matched */
	u_short flags;			/* accesslist flags */
	u_short mflags;			/* match flags */
};

struct restrictlist6 {
	struct restrictlist6 *next;	/* link to next entry */
	struct in6_addr addr6;		/* Ipv6 host address */
	struct in6_addr mask6;		/* Ipv6 mask address */
	u_long count;			/* number of packets matched */
	u_short flags;			/* accesslist flags */
	u_short mflags;			/* match flags */
};


/*
 * Access flags
 */
#define	RES_IGNORE		0x001	/* ignore packet */
#define	RES_DONTSERVE		0x002	/* access denied */
#define	RES_DONTTRUST		0x004	/* authentication required */
#define	RES_VERSION		0x008	/* version mismatch */
#define	RES_NOPEER		0x010	/* new association denied */
#define RES_LIMITED		0x020	/* packet rate exceeded */

#define RES_FLAGS		(RES_IGNORE | RES_DONTSERVE |\
				    RES_DONTTRUST | RES_VERSION |\
				    RES_NOPEER | RES_LIMITED)

#define	RES_NOQUERY		0x040	/* mode 6/7 packet denied */
#define	RES_NOMODIFY		0x080	/* mode 6/7 modify denied */
#define	RES_NOTRAP		0x100	/* mode 6/7 set trap denied */
#define	RES_LPTRAP		0x200	/* mode 6/7 low priority trap */

#define RES_DEMOBILIZE		0x400	/* send kiss of death packet */
#define RES_TIMEOUT		0x800	/* timeout this entry */

#define	RES_ALLFLAGS		(RES_FLAGS | RES_NOQUERY |\
				    RES_NOMODIFY | RES_NOTRAP |\
				    RES_LPTRAP | RES_DEMOBILIZE |\
				    RES_TIMEOUT)

/*
 * Match flags
 */
#define	RESM_INTERFACE		0x1	/* this is an interface */
#define	RESM_NTPONLY		0x2	/* match ntp port only */

/*
 * Restriction configuration ops
 */
#define	RESTRICT_FLAGS		1	/* add flags to restrict entry */
#define	RESTRICT_UNFLAG		2	/* remove flags from restrict entry */
#define	RESTRICT_REMOVE		3	/* remove a restrict entry */

/*
 * Endpoint structure for the select algorithm
 */
struct endpoint {
	double	val;			/* offset of endpoint */
	int	type;			/* interval entry/exit */
};

/*
 * Defines for association matching 
 */
#define AM_MODES	10	/* total number of modes */
#define NO_PEER		0	/* action when no peer is found */

/*
 * Association matching AM[] return codes
 */
#define AM_ERR		-1
#define AM_NOMATCH	 0
#define AM_PROCPKT	 1
#define AM_FXMIT	 2
#define AM_MANYCAST	 3
#define AM_NEWPASS	 4
#define AM_NEWBCL	 5
#define AM_POSSBCL	 6

/* NetInfo configuration locations */
#ifdef HAVE_NETINFO
#define NETINFO_CONFIG_DIR "/config/ntp"
#endif

#endif /* NTP_H */
