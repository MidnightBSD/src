/* $MidnightBSD$ */
/* $FreeBSD: stable/10/sys/cddl/dev/dtrace/dtrace_hacks.c 283676 2015-05-29 04:01:39Z markj $ */
/* XXX Hacks.... */

dtrace_cacheid_t dtrace_predcache_id;

boolean_t
priv_policy_only(const cred_t *a, int b, boolean_t c)
{
	return 0;
}
