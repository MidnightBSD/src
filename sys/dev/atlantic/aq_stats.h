/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2014-2017 aQuantia Corporation. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   (1) Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer.
 *
 *   (2) Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   (3)The name of the author may not be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _AQ_STATS_H_
#define _AQ_STATS_H_

#include <sys/types.h>

struct aq_stats_s {
	uint64_t good_pkts_rcvd;
	uint64_t ucast_pkts_rcvd;
	uint64_t mcast_pkts_rcvd;
	uint64_t bcast_pkts_rcvd;
	uint64_t pause_frames_rcvd;
	uint64_t rsc_pkts_rcvd;
	uint64_t err_pkts_rcvd;
	uint64_t drop_pkts_dma;
	uint64_t good_octets_rcvd;
	uint64_t ucast_octets_rcvd;
	uint64_t mcast_octets_rcvd;
	uint64_t bcast_octets_rcvd;

	uint64_t good_pkts_txd;
	uint64_t ucast_pkts_txd;
	uint64_t mcast_pkts_txd;
	uint64_t bcast_pkts_txd;
	uint64_t pause_frames_txd;
	uint64_t err_pkts_txd;
	uint64_t good_octets_txd;
	uint64_t ucast_octets_txd;
	uint64_t mcast_octets_txd;
	uint64_t bcast_octets_txd;
};

#endif /* _AQ_STATS_H_ */
