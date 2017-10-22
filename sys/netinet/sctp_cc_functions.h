/*-
 * Copyright (c) 2001-2007, by Cisco Systems, Inc. All rights reserved.
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
__FBSDID("$FreeBSD: release/7.0.0/sys/netinet/sctp_cc_functions.h 174854 2007-12-22 06:32:46Z cvs2svn $");

#ifndef __sctp_cc_functions_h__
#define __sctp_cc_functions_h__

#if defined(_KERNEL)

void
sctp_set_initial_cc_param(struct sctp_tcb *stcb,
    struct sctp_nets *net);

void
sctp_cwnd_update_after_fr(struct sctp_tcb *stcb,
    struct sctp_association *asoc);

void
sctp_cwnd_update_after_sack(struct sctp_tcb *stcb,
    struct sctp_association *asoc,
    int accum_moved, int reneged_all, int will_exit);

void
sctp_cwnd_update_after_timeout(struct sctp_tcb *stcb,
    struct sctp_nets *net);

void
sctp_hs_cwnd_update_after_fr(struct sctp_tcb *stcb,
    struct sctp_association *asoc);

void
sctp_hs_cwnd_update_after_sack(struct sctp_tcb *stcb,
    struct sctp_association *asoc,
    int accum_moved, int reneged_all, int will_exit);

void
sctp_cwnd_update_after_ecn_echo(struct sctp_tcb *stcb,
    struct sctp_nets *net);

void
sctp_cwnd_update_after_packet_dropped(struct sctp_tcb *stcb,
    struct sctp_nets *net, struct sctp_pktdrop_chunk *cp,
    uint32_t * bottle_bw, uint32_t * on_queue);

void
sctp_cwnd_update_after_output(struct sctp_tcb *stcb,
    struct sctp_nets *net, int burst_limit);

void
sctp_cwnd_update_after_fr_timer(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb, struct sctp_nets *net);

/*
 * HTCP algorithms are directly taken from
 * R.N.Shorten, D.J.Leith and are work/outcome from
 * a Cisco-URP grant to enhance HTCP for satellite
 * communications. We use the BSD Liscense
 * granted from his source and have modified his
 * algorithms to fit within the SCTP BSD framework.
 */

void
sctp_htcp_set_initial_cc_param(struct sctp_tcb *stcb,
    struct sctp_nets *net);

void
sctp_htcp_cwnd_update_after_fr(struct sctp_tcb *stcb,
    struct sctp_association *asoc);

void
sctp_htcp_cwnd_update_after_sack(struct sctp_tcb *stcb,
    struct sctp_association *asoc,
    int accum_moved, int reneged_all, int will_exit);

void
sctp_htcp_cwnd_update_after_timeout(struct sctp_tcb *stcb,
    struct sctp_nets *net);

void
sctp_htcp_cwnd_update_after_ecn_echo(struct sctp_tcb *stcb,
    struct sctp_nets *net);

void
sctp_htcp_cwnd_update_after_fr_timer(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb, struct sctp_nets *net);

#endif
#endif
