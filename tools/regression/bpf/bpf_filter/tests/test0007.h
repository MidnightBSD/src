/*-
 * Test 0007:	BPF_LD+BPF_W+BPF_LEN
 *
 * $FreeBSD: release/10.0.0/tools/regression/bpf/bpf_filter/tests/test0007.h 182393 2008-08-28 18:38:55Z jkim $
 */

/* BPF program */
struct bpf_insn pc[] = {
	BPF_STMT(BPF_LD+BPF_W+BPF_LEN, 0),
	BPF_STMT(BPF_RET+BPF_A, 0),
};

/* Packet */
u_char	pkt[] = {
	0x00,
};

/* Packet length seen on wire */
u_int	wirelen =	0xdeadc0de;

/* Packet length passed on buffer */
u_int	buflen =	0xdeadc0de;

/* Invalid instruction */
int	invalid =	0;

/* Expected return value */
u_int	expect =	0xdeadc0de;

/* Expected signal */
int	expect_signal =	0;
