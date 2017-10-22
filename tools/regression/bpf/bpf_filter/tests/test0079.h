/*-
 * Test 0079:	An empty filter program.
 *
 * $FreeBSD: release/10.0.0/tools/regression/bpf/bpf_filter/tests/test0079.h 182393 2008-08-28 18:38:55Z jkim $
 */

/* BPF program */
struct bpf_insn pc[] = {
};

/* Packet */
u_char	pkt[] = {
	0x00,
};

/* Packet length seen on wire */
u_int	wirelen =	sizeof(pkt);

/* Packet length passed on buffer */
u_int	buflen =	sizeof(pkt);

/* Invalid instruction */
int	invalid =	0;

/* Expected return value */
u_int	expect =	(u_int)-1;

/* Expected signal */
int	expect_signal =	0;
