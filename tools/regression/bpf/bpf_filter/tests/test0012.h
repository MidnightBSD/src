/*-
 * Test 0012:	BPF_LDX+BPF_MSH+BPF_B
 *
 * $FreeBSD: release/10.0.0/tools/regression/bpf/bpf_filter/tests/test0012.h 182393 2008-08-28 18:38:55Z jkim $
 */

/* BPF program */
struct bpf_insn pc[] = {
	BPF_STMT(BPF_LDX+BPF_MSH+BPF_B, 1),
	BPF_STMT(BPF_MISC+BPF_TXA, 0),
	BPF_STMT(BPF_RET+BPF_A, 0),
};

/* Packet */
u_char	pkt[] = {
	0x01, 0x23,
};

/* Packet length seen on wire */
u_int	wirelen =	sizeof(pkt);

/* Packet length passed on buffer */
u_int	buflen =	sizeof(pkt);

/* Invalid instruction */
int	invalid =	0;

/* Expected return value */
u_int	expect =	(0x23 & 0xf) << 2;

/* Expected signal */
int	expect_signal =	0;
