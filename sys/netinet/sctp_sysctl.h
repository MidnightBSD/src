/*-
 * Copyright (c) 2007, by Cisco Systems, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/netinet/sctp_sysctl.h 174854 2007-12-22 06:32:46Z cvs2svn $");

#ifndef __sctp_sysctl_h__
#define __sctp_sysctl_h__

#include <netinet/sctp_os.h>
#include <netinet/sctp_constants.h>

/*
 * limits for the sysctl variables
 */
/* maxdgram: Maximum outgoing SCTP buffer size */
#define SCTPCTL_MAXDGRAM_DESC		"Maximum outgoing SCTP buffer size"
#define SCTPCTL_MAXDGRAM_MIN		0
#define SCTPCTL_MAXDGRAM_MAX		0xFFFFFFFF
#define SCTPCTL_MAXDGRAM_DEFAULT	262144	/* 256k */

/* recvspace: Maximum incoming SCTP buffer size */
#define SCTPCTL_RECVSPACE_DESC		"Maximum incoming SCTP buffer size"
#define SCTPCTL_RECVSPACE_MIN		0
#define SCTPCTL_RECVSPACE_MAX		0xFFFFFFFF
#define SCTPCTL_RECVSPACE_DEFAULT	262144	/* 256k */

/* autoasconf: Enable SCTP Auto-ASCONF */
#define SCTPCTL_AUTOASCONF_DESC		"Enable SCTP Auto-ASCONF"
#define SCTPCTL_AUTOASCONF_MIN		0
#define SCTPCTL_AUTOASCONF_MAX		1
#define SCTPCTL_AUTOASCONF_DEFAULT	SCTP_DEFAULT_AUTO_ASCONF

/* ecn_enable: Enable SCTP ECN */
#define SCTPCTL_ECN_ENABLE_DESC		"Enable SCTP ECN"
#define SCTPCTL_ECN_ENABLE_MIN		0
#define SCTPCTL_ECN_ENABLE_MAX		1
#define SCTPCTL_ECN_ENABLE_DEFAULT	1

/* ecn_nonce: Enable SCTP ECN Nonce */
#define SCTPCTL_ECN_NONCE_DESC		"Enable SCTP ECN Nonce"
#define SCTPCTL_ECN_NONCE_MIN		0
#define SCTPCTL_ECN_NONCE_MAX		1
#define SCTPCTL_ECN_NONCE_DEFAULT	0

/* strict_sacks: Enable SCTP Strict SACK checking */
#define SCTPCTL_STRICT_SACKS_DESC	"Enable SCTP Strict SACK checking"
#define SCTPCTL_STRICT_SACKS_MIN	0
#define SCTPCTL_STRICT_SACKS_MAX	1
#define SCTPCTL_STRICT_SACKS_DEFAULT	0

/* loopback_nocsum: Enable NO Csum on packets sent on loopback */
#define SCTPCTL_LOOPBACK_NOCSUM_DESC	"Enable NO Csum on packets sent on loopback"
#define SCTPCTL_LOOPBACK_NOCSUM_MIN	0
#define SCTPCTL_LOOPBACK_NOCSUM_MAX	1
#define SCTPCTL_LOOPBACK_NOCSUM_DEFAULT	1

/* strict_init: Enable strict INIT/INIT-ACK singleton enforcement */
#define SCTPCTL_STRICT_INIT_DESC	"Enable strict INIT/INIT-ACK singleton enforcement"
#define SCTPCTL_STRICT_INIT_MIN		0
#define SCTPCTL_STRICT_INIT_MAX		1
#define SCTPCTL_STRICT_INIT_DEFAULT	1

/* peer_chkoh: Amount to debit peers rwnd per chunk sent */
#define SCTPCTL_PEER_CHKOH_DESC		"Amount to debit peers rwnd per chunk sent"
#define SCTPCTL_PEER_CHKOH_MIN		0
#define SCTPCTL_PEER_CHKOH_MAX		0xFFFFFFFF
#define SCTPCTL_PEER_CHKOH_DEFAULT	256

/* maxburst: Default max burst for sctp endpoints */
#define SCTPCTL_MAXBURST_DESC		"Default max burst for sctp endpoints"
#define SCTPCTL_MAXBURST_MIN		1
#define SCTPCTL_MAXBURST_MAX		0xFFFFFFFF
#define SCTPCTL_MAXBURST_DEFAULT	SCTP_DEF_MAX_BURST

/* maxchunks: Default max chunks on queue per asoc */
#define SCTPCTL_MAXCHUNKS_DESC		"Default max chunks on queue per asoc"
#define SCTPCTL_MAXCHUNKS_MIN		0
#define SCTPCTL_MAXCHUNKS_MAX		0xFFFFFFFF
#define SCTPCTL_MAXCHUNKS_DEFAULT	SCTP_ASOC_MAX_CHUNKS_ON_QUEUE

/* tcbhashsize: Tuneable for Hash table sizes */
#define SCTPCTL_TCBHASHSIZE_DESC	"Tunable for TCB hash table sizes"
#define SCTPCTL_TCBHASHSIZE_MIN		1
#define SCTPCTL_TCBHASHSIZE_MAX		0xFFFFFFFF
#define SCTPCTL_TCBHASHSIZE_DEFAULT	SCTP_TCBHASHSIZE

/* pcbhashsize: Tuneable for PCB Hash table sizes */
#define SCTPCTL_PCBHASHSIZE_DESC	"Tunable for PCB hash table sizes"
#define SCTPCTL_PCBHASHSIZE_MIN		1
#define SCTPCTL_PCBHASHSIZE_MAX		0xFFFFFFFF
#define SCTPCTL_PCBHASHSIZE_DEFAULT	SCTP_PCBHASHSIZE

/* min_split_point: Minimum size when splitting a chunk */
#define SCTPCTL_MIN_SPLIT_POINT_DESC	"Minimum size when splitting a chunk"
#define SCTPCTL_MIN_SPLIT_POINT_MIN	0
#define SCTPCTL_MIN_SPLIT_POINT_MAX	0xFFFFFFFF
#define SCTPCTL_MIN_SPLIT_POINT_DEFAULT	SCTP_DEFAULT_SPLIT_POINT_MIN

/* chunkscale: Tuneable for Scaling of number of chunks and messages */
#define SCTPCTL_CHUNKSCALE_DESC		"Tuneable for Scaling of number of chunks and messages"
#define SCTPCTL_CHUNKSCALE_MIN		1
#define SCTPCTL_CHUNKSCALE_MAX		0xFFFFFFFF
#define SCTPCTL_CHUNKSCALE_DEFAULT	SCTP_CHUNKQUEUE_SCALE

/* delayed_sack_time: Default delayed SACK timer in msec */
#define SCTPCTL_DELAYED_SACK_TIME_DESC	"Default delayed SACK timer in msec"
#define SCTPCTL_DELAYED_SACK_TIME_MIN	0
#define SCTPCTL_DELAYED_SACK_TIME_MAX	0xFFFFFFFF
#define SCTPCTL_DELAYED_SACK_TIME_DEFAULT	SCTP_RECV_MSEC

/* sack_freq: Default SACK frequency */
#define SCTPCTL_SACK_FREQ_DESC		"Default SACK frequency"
#define SCTPCTL_SACK_FREQ_MIN		0
#define SCTPCTL_SACK_FREQ_MAX		0xFFFFFFFF
#define SCTPCTL_SACK_FREQ_DEFAULT	SCTP_DEFAULT_SACK_FREQ

/* sys_resource: Max number of cached resources in the system */
#define SCTPCTL_SYS_RESOURCE_DESC	"Max number of cached resources in the system"
#define SCTPCTL_SYS_RESOURCE_MIN	0
#define SCTPCTL_SYS_RESOURCE_MAX	0xFFFFFFFF
#define SCTPCTL_SYS_RESOURCE_DEFAULT	SCTP_DEF_SYSTEM_RESC_LIMIT

/* asoc_resource: Max number of cached resources in an asoc */
#define SCTPCTL_ASOC_RESOURCE_DESC	"Max number of cached resources in an asoc"
#define SCTPCTL_ASOC_RESOURCE_MIN	0
#define SCTPCTL_ASOC_RESOURCE_MAX	0xFFFFFFFF
#define SCTPCTL_ASOC_RESOURCE_DEFAULT	SCTP_DEF_ASOC_RESC_LIMIT

/* heartbeat_interval: Default heartbeat interval in msec */
#define SCTPCTL_HEARTBEAT_INTERVAL_DESC	"Default heartbeat interval in msec"
#define SCTPCTL_HEARTBEAT_INTERVAL_MIN	0
#define SCTPCTL_HEARTBEAT_INTERVAL_MAX	0xFFFFFFFF
#define SCTPCTL_HEARTBEAT_INTERVAL_DEFAULT	SCTP_HB_DEFAULT_MSEC

/* pmtu_raise_time: Default PMTU raise timer in sec */
#define SCTPCTL_PMTU_RAISE_TIME_DESC	"Default PMTU raise timer in sec"
#define SCTPCTL_PMTU_RAISE_TIME_MIN	0
#define SCTPCTL_PMTU_RAISE_TIME_MAX	0xFFFFFFFF
#define SCTPCTL_PMTU_RAISE_TIME_DEFAULT	SCTP_DEF_PMTU_RAISE_SEC

/* shutdown_guard_time: Default shutdown guard timer in sec */
#define SCTPCTL_SHUTDOWN_GUARD_TIME_DESC	"Default shutdown guard timer in sec"
#define SCTPCTL_SHUTDOWN_GUARD_TIME_MIN		0
#define SCTPCTL_SHUTDOWN_GUARD_TIME_MAX		0xFFFFFFFF
#define SCTPCTL_SHUTDOWN_GUARD_TIME_DEFAULT	SCTP_DEF_MAX_SHUTDOWN_SEC

/* secret_lifetime: Default secret lifetime in sec */
#define SCTPCTL_SECRET_LIFETIME_DESC	"Default secret lifetime in sec"
#define SCTPCTL_SECRET_LIFETIME_MIN	0
#define SCTPCTL_SECRET_LIFETIME_MAX	0xFFFFFFFF
#define SCTPCTL_SECRET_LIFETIME_DEFAULT	SCTP_DEFAULT_SECRET_LIFE_SEC

/* rto_max: Default maximum retransmission timeout in msec */
#define SCTPCTL_RTO_MAX_DESC		"Default maximum retransmission timeout in msec"
#define SCTPCTL_RTO_MAX_MIN		0
#define SCTPCTL_RTO_MAX_MAX		0xFFFFFFFF
#define SCTPCTL_RTO_MAX_DEFAULT		SCTP_RTO_UPPER_BOUND

/* rto_min: Default minimum retransmission timeout in msec */
#define SCTPCTL_RTO_MIN_DESC		"Default minimum retransmission timeout in msec"
#define SCTPCTL_RTO_MIN_MIN		0
#define SCTPCTL_RTO_MIN_MAX		0xFFFFFFFF
#define SCTPCTL_RTO_MIN_DEFAULT		SCTP_RTO_LOWER_BOUND

/* rto_initial: Default initial retransmission timeout in msec */
#define SCTPCTL_RTO_INITIAL_DESC	"Default initial retransmission timeout in msec"
#define SCTPCTL_RTO_INITIAL_MIN		0
#define SCTPCTL_RTO_INITIAL_MAX		0xFFFFFFFF
#define SCTPCTL_RTO_INITIAL_DEFAULT	SCTP_RTO_INITIAL

/* init_rto_max: Default maximum retransmission timeout during association setup in msec */
#define SCTPCTL_INIT_RTO_MAX_DESC	"Default maximum retransmission timeout during association setup in msec"
#define SCTPCTL_INIT_RTO_MAX_MIN	0
#define SCTPCTL_INIT_RTO_MAX_MAX	0xFFFFFFFF
#define SCTPCTL_INIT_RTO_MAX_DEFAULT	SCTP_RTO_UPPER_BOUND

/* valid_cookie_life: Default cookie lifetime in sec */
#define SCTPCTL_VALID_COOKIE_LIFE_DESC	"Default cookie lifetime in sec"
#define SCTPCTL_VALID_COOKIE_LIFE_MIN	0
#define SCTPCTL_VALID_COOKIE_LIFE_MAX	0xFFFFFFFF
#define SCTPCTL_VALID_COOKIE_LIFE_DEFAULT	SCTP_DEFAULT_COOKIE_LIFE

/* init_rtx_max: Default maximum number of retransmission for INIT chunks */
#define SCTPCTL_INIT_RTX_MAX_DESC	"Default maximum number of retransmission for INIT chunks"
#define SCTPCTL_INIT_RTX_MAX_MIN	0
#define SCTPCTL_INIT_RTX_MAX_MAX	0xFFFFFFFF
#define SCTPCTL_INIT_RTX_MAX_DEFAULT	SCTP_DEF_MAX_INIT

/* assoc_rtx_max: Default maximum number of retransmissions per association */
#define SCTPCTL_ASSOC_RTX_MAX_DESC	"Default maximum number of retransmissions per association"
#define SCTPCTL_ASSOC_RTX_MAX_MIN	0
#define SCTPCTL_ASSOC_RTX_MAX_MAX	0xFFFFFFFF
#define SCTPCTL_ASSOC_RTX_MAX_DEFAULT	SCTP_DEF_MAX_SEND

/* path_rtx_max: Default maximum of retransmissions per path */
#define SCTPCTL_PATH_RTX_MAX_DESC	"Default maximum of retransmissions per path"
#define SCTPCTL_PATH_RTX_MAX_MIN	0
#define SCTPCTL_PATH_RTX_MAX_MAX	0xFFFFFFFF
#define SCTPCTL_PATH_RTX_MAX_DEFAULT	SCTP_DEF_MAX_PATH_RTX

/* add_more_on_output: When space wise is it worthwhile to try to add more to a socket send buffer */
#define SCTPCTL_ADD_MORE_ON_OUTPUT_DESC	"When space wise is it worthwhile to try to add more to a socket send buffer"
#define SCTPCTL_ADD_MORE_ON_OUTPUT_MIN	0
#define SCTPCTL_ADD_MORE_ON_OUTPUT_MAX	0xFFFFFFFF
#define SCTPCTL_ADD_MORE_ON_OUTPUT_DEFAULT SCTP_DEFAULT_ADD_MORE

/* outgoing_streams: Default number of outgoing streams */
#define SCTPCTL_OUTGOING_STREAMS_DESC	"Default number of outgoing streams"
#define SCTPCTL_OUTGOING_STREAMS_MIN	1
#define SCTPCTL_OUTGOING_STREAMS_MAX	65535
#define SCTPCTL_OUTGOING_STREAMS_DEFAULT SCTP_OSTREAM_INITIAL

/* cmt_on_off: CMT on/off flag */
#define SCTPCTL_CMT_ON_OFF_DESC		"CMT on/off flag"
#define SCTPCTL_CMT_ON_OFF_MIN		0
#define SCTPCTL_CMT_ON_OFF_MAX		1
#define SCTPCTL_CMT_ON_OFF_DEFAULT	0

/* cmt_use_dac: CMT DAC on/off flag */
#define SCTPCTL_CMT_USE_DAC_DESC	"CMT DAC on/off flag"
#define SCTPCTL_CMT_USE_DAC_MIN		0
#define SCTPCTL_CMT_USE_DAC_MAX		1
#define SCTPCTL_CMT_USE_DAC_DEFAULT    	0

/* JRS 5/2107 - CMT PF type flag */
#define SCTPCTL_CMT_PF_DESC		"CMT PF type flag"
#define SCTPCTL_CMT_PF_MIN		0
#define SCTPCTL_CMT_PF_MAX		2
#define SCTPCTL_CMT_PF_DEFAULT		0

/* cwnd_maxburst: Use a CWND adjusting maxburst */
#define SCTPCTL_CWND_MAXBURST_DESC	"Use a CWND adjusting maxburst"
#define SCTPCTL_CWND_MAXBURST_MIN	0
#define SCTPCTL_CWND_MAXBURST_MAX	1
#define SCTPCTL_CWND_MAXBURST_DEFAULT	1

/* early_fast_retran: Early Fast Retransmit with timer */
#define SCTPCTL_EARLY_FAST_RETRAN_DESC	"Early Fast Retransmit with timer"
#define SCTPCTL_EARLY_FAST_RETRAN_MIN	0
#define SCTPCTL_EARLY_FAST_RETRAN_MAX	0xFFFFFFFF
#define SCTPCTL_EARLY_FAST_RETRAN_DEFAULT	0

/* early_fast_retran_msec: Early Fast Retransmit minimum timer value */
#define SCTPCTL_EARLY_FAST_RETRAN_MSEC_DESC	"Early Fast Retransmit minimum timer value"
#define SCTPCTL_EARLY_FAST_RETRAN_MSEC_MIN	0
#define SCTPCTL_EARLY_FAST_RETRAN_MSEC_MAX	0xFFFFFFFF
#define SCTPCTL_EARLY_FAST_RETRAN_MSEC_DEFAULT	SCTP_MINFR_MSEC_TIMER

/* asconf_auth_nochk: Disable SCTP ASCONF AUTH requirement */
#define SCTPCTL_ASCONF_AUTH_NOCHK_DESC	"Disable SCTP ASCONF AUTH requirement"
#define SCTPCTL_ASCONF_AUTH_NOCHK_MIN	0
#define SCTPCTL_ASCONF_AUTH_NOCHK_MAX	1
#define SCTPCTL_ASCONF_AUTH_NOCHK_DEFAULT	0

/* auth_disable: Disable SCTP AUTH function */
#define SCTPCTL_AUTH_DISABLE_DESC	"Disable SCTP AUTH function"
#define SCTPCTL_AUTH_DISABLE_MIN	0
#define SCTPCTL_AUTH_DISABLE_MAX	1
#define SCTPCTL_AUTH_DISABLE_DEFAULT	0

/* nat_friendly: SCTP NAT friendly operation */
#define SCTPCTL_NAT_FRIENDLY_DESC	"SCTP NAT friendly operation"
#define SCTPCTL_NAT_FRIENDLY_MIN	0
#define SCTPCTL_NAT_FRIENDLY_MAX	1
#define SCTPCTL_NAT_FRIENDLY_DEFAULT	1

/* abc_l_var: SCTP ABC max increase per SACK (L) */
#define SCTPCTL_ABC_L_VAR_DESC		"SCTP ABC max increase per SACK (L)"
#define SCTPCTL_ABC_L_VAR_MIN		0
#define SCTPCTL_ABC_L_VAR_MAX		0xFFFFFFFF
#define SCTPCTL_ABC_L_VAR_DEFAULT	1

/* max_chained_mbufs: Default max number of small mbufs on a chain */
#define SCTPCTL_MAX_CHAINED_MBUFS_DESC	"Default max number of small mbufs on a chain"
#define SCTPCTL_MAX_CHAINED_MBUFS_MIN	0
#define SCTPCTL_MAX_CHAINED_MBUFS_MAX	0xFFFFFFFF
#define SCTPCTL_MAX_CHAINED_MBUFS_DEFAULT	SCTP_DEFAULT_MBUFS_IN_CHAIN

/* do_sctp_drain: Should SCTP respond to the drain calls */
#define SCTPCTL_DO_SCTP_DRAIN_DESC	"Should SCTP respond to the drain calls"
#define SCTPCTL_DO_SCTP_DRAIN_MIN	0
#define SCTPCTL_DO_SCTP_DRAIN_MAX	1
#define SCTPCTL_DO_SCTP_DRAIN_DEFAULT	1

/* hb_max_burst: Confirmation Heartbeat max burst? */
#define SCTPCTL_HB_MAX_BURST_DESC	"Confirmation Heartbeat max burst"
#define SCTPCTL_HB_MAX_BURST_MIN	1
#define SCTPCTL_HB_MAX_BURST_MAX	0xFFFFFFFF
#define SCTPCTL_HB_MAX_BURST_DEFAULT	SCTP_DEF_MAX_BURST

/* abort_at_limit: When one-2-one hits qlimit abort */
#define SCTPCTL_ABORT_AT_LIMIT_DESC	"When one-2-one hits qlimit abort"
#define SCTPCTL_ABORT_AT_LIMIT_MIN	0
#define SCTPCTL_ABORT_AT_LIMIT_MAX	1
#define SCTPCTL_ABORT_AT_LIMIT_DEFAULT	0

/* strict_data_order: Enforce strict data ordering, abort if control inside data */
#define SCTPCTL_STRICT_DATA_ORDER_DESC	"Enforce strict data ordering, abort if control inside data"
#define SCTPCTL_STRICT_DATA_ORDER_MIN	0
#define SCTPCTL_STRICT_DATA_ORDER_MAX	1
#define SCTPCTL_STRICT_DATA_ORDER_DEFAULT	0

/* min_residual: min residual in a data fragment leftover */
#define SCTPCTL_MIN_RESIDUAL_DESC	"Minimum residual data chunk in second part of split"
#define SCTPCTL_MIN_RESIDUAL_MIN	20
#define SCTPCTL_MIN_RESIDUAL_MAX	65535
#define SCTPCTL_MIN_RESIDUAL_DEFAULT	1452

/* max_retran_chunk: max chunk retransmissions */
#define SCTPCTL_MAX_RETRAN_CHUNK_DESC	"Maximum times an unlucky chunk can be retran'd before assoc abort"
#define SCTPCTL_MAX_RETRAN_CHUNK_MIN	0
#define SCTPCTL_MAX_RETRAN_CHUNK_MAX	65535
#define SCTPCTL_MAX_RETRAN_CHUNK_DEFAULT	30

/* sctp_logging: This gives us logging when the options are enabled */
#define SCTPCTL_LOGGING_LEVEL_DESC	"Ltrace/KTR trace logging level"
#define SCTPCTL_LOGGING_LEVEL_MIN	0
#define SCTPCTL_LOGGING_LEVEL_MAX	0xffffffff
#define SCTPCTL_LOGGING_LEVEL_DEFAULT	0

/* JRS - default congestion control module sysctl */
#define SCTPCTL_DEFAULT_CC_MODULE_DESC		"Default congestion control module"
#define SCTPCTL_DEFAULT_CC_MODULE_MIN		0
#define SCTPCTL_DEFAULT_CC_MODULE_MAX		2
#define SCTPCTL_DEFAULT_CC_MODULE_DEFAULT	0

/* RRS - default fragment interleave */
#define SCTPCTL_DEFAULT_FRAG_INTERLEAVE_DESC	"Default fragment interleave level"
#define SCTPCTL_DEFAULT_FRAG_INTERLEAVE_MIN	0
#define SCTPCTL_DEFAULT_FRAG_INTERLEAVE_MAX	2
#define SCTPCTL_DEFAULT_FRAG_INTERLEAVE_DEFAULT	1

/* mobility_base: Enable SCTP mobility support */
#define SCTPCTL_MOBILITY_BASE_DESC	"Enable SCTP base mobility"
#define SCTPCTL_MOBILITY_BASE_MIN	0
#define SCTPCTL_MOBILITY_BASE_MAX	1
#define SCTPCTL_MOBILITY_BASE_DEFAULT	SCTP_DEFAULT_MOBILITY_BASE

/* mobility_fasthandoff: Enable SCTP fast handoff support */
#define SCTPCTL_MOBILITY_FASTHANDOFF_DESC	"Enable SCTP fast handoff"
#define SCTPCTL_MOBILITY_FASTHANDOFF_MIN	0
#define SCTPCTL_MOBILITY_FASTHANDOFF_MAX	1
#define SCTPCTL_MOBILITY_FASTHANDOFF_DEFAULT	SCTP_DEFAULT_MOBILITY_FASTHANDOFF

#if defined(SCTP_DEBUG)
/* debug: Configure debug output */
#define SCTPCTL_DEBUG_DESC	"Configure debug output"
#define SCTPCTL_DEBUG_MIN	0
#define SCTPCTL_DEBUG_MAX	0xFFFFFFFF
#define SCTPCTL_DEBUG_DEFAULT	0
#endif



#if defined(_KERNEL)

/*
 * variable definitions
 */
extern uint32_t sctp_sendspace;
extern uint32_t sctp_recvspace;
extern uint32_t sctp_auto_asconf;
extern uint32_t sctp_ecn_enable;
extern uint32_t sctp_ecn_nonce;
extern uint32_t sctp_strict_sacks;
extern uint32_t sctp_no_csum_on_loopback;
extern uint32_t sctp_strict_init;
extern uint32_t sctp_peer_chunk_oh;
extern uint32_t sctp_max_burst_default;
extern uint32_t sctp_max_chunks_on_queue;
extern uint32_t sctp_hashtblsize;
extern uint32_t sctp_pcbtblsize;
extern uint32_t sctp_min_split_point;
extern uint32_t sctp_chunkscale;
extern uint32_t sctp_delayed_sack_time_default;
extern uint32_t sctp_sack_freq_default;
extern uint32_t sctp_system_free_resc_limit;
extern uint32_t sctp_asoc_free_resc_limit;
extern uint32_t sctp_heartbeat_interval_default;
extern uint32_t sctp_pmtu_raise_time_default;
extern uint32_t sctp_shutdown_guard_time_default;
extern uint32_t sctp_secret_lifetime_default;
extern uint32_t sctp_rto_max_default;
extern uint32_t sctp_rto_min_default;
extern uint32_t sctp_rto_initial_default;
extern uint32_t sctp_init_rto_max_default;
extern uint32_t sctp_valid_cookie_life_default;
extern uint32_t sctp_init_rtx_max_default;
extern uint32_t sctp_assoc_rtx_max_default;
extern uint32_t sctp_path_rtx_max_default;
extern uint32_t sctp_add_more_threshold;
extern uint32_t sctp_nr_outgoing_streams_default;
extern uint32_t sctp_cmt_on_off;
extern uint32_t sctp_cmt_use_dac;

/* JRS 5/21/07 - CMT PF type flag variables  */
extern uint32_t sctp_cmt_pf;
extern uint32_t sctp_use_cwnd_based_maxburst;
extern uint32_t sctp_early_fr;
extern uint32_t sctp_early_fr_msec;
extern uint32_t sctp_asconf_auth_nochk;
extern uint32_t sctp_auth_disable;
extern uint32_t sctp_nat_friendly;
extern uint32_t sctp_L2_abc_variable;
extern uint32_t sctp_mbuf_threshold_count;
extern uint32_t sctp_do_drain;
extern uint32_t sctp_hb_maxburst;
extern uint32_t sctp_abort_if_one_2_one_hits_limit;
extern uint32_t sctp_strict_data_order;
extern uint32_t sctp_min_residual;
extern uint32_t sctp_max_retran_chunk;
extern uint32_t sctp_logging_level;

/* JRS - Variable for the default congestion control module */
extern uint32_t sctp_default_cc_module;
extern uint32_t sctp_default_frag_interleave;
extern uint32_t sctp_mobility_base;
extern uint32_t sctp_mobility_fasthandoff;

#if defined(SCTP_LOCAL_TRACE_BUF)
extern struct sctp_log sctp_log;

#endif
#if defined(SCTP_DEBUG)
extern uint32_t sctp_debug_on;

#endif

extern struct sctpstat sctpstat;

#if defined(SYSCTL_DECL)
SYSCTL_DECL(_net_inet_sctp);
#endif

#endif				/* _KERNEL */
#endif				/* __sctp_sysctl_h__ */
