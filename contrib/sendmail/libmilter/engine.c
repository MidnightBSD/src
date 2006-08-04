/*
 *  Copyright (c) 1999-2004, 2006 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sm/gen.h>
SM_RCSID("@(#)$Id: engine.c,v 1.1.1.3 2006-08-04 02:03:05 laffer1 Exp $")

#include "libmilter.h"

#if NETINET || NETINET6
# include <arpa/inet.h>
#endif /* NETINET || NETINET6 */

/* generic argument for functions in the command table */
struct arg_struct
{
	size_t		a_len;		/* length of buffer */
	char		*a_buf;		/* argument string */
	int		a_idx;		/* index for macro array */
	SMFICTX_PTR	a_ctx;		/* context */
};

typedef struct arg_struct genarg;

/* structure for commands received from MTA */
struct cmdfct_t
{
	char	cm_cmd;				/* command */
	int	cm_argt;			/* type of arguments expected */
	int	cm_next;			/* next state */
	int	cm_todo;			/* what to do next */
	int	cm_macros;			/* index for macros */
	int	(*cm_fct) __P((genarg *));	/* function to execute */
};

typedef struct cmdfct_t cmdfct;

/* possible values for cm_argt */
#define	CM_ARG0	0	/* no args */
#define	CM_ARG1	1	/* one arg (string) */
#define	CM_ARG2	2	/* two args (strings) */
#define	CM_ARGA	4	/* one string and _SOCK_ADDR */
#define	CM_ARGO	5	/* two integers */
#define	CM_ARGV	8	/* \0 separated list of args, NULL-terminated */
#define	CM_ARGN	9	/* \0 separated list of args (strings) */

/* possible values for cm_todo */
#define	CT_CONT		0x0000	/* continue reading commands */
#define	CT_IGNO		0x0001	/* continue even when error  */

/* not needed right now, done via return code instead */
#define	CT_KEEP		0x0004	/* keep buffer (contains symbols) */
#define	CT_END		0x0008	/* start replying */

/* index in macro array: macros only for these commands */
#define	CI_NONE		(-1)
#define	CI_CONN		0
#define	CI_HELO		1
#define	CI_MAIL		2
#define CI_RCPT		3
#define CI_EOM		4
#if CI_EOM >= MAX_MACROS_ENTRIES
ERROR: do not compile with CI_EOM >= MAX_MACROS_ENTRIES
#endif

/* function prototypes */
static int	st_abortfct __P((genarg *));
static int	st_macros __P((genarg *));
static int	st_optionneg __P((genarg *));
static int	st_bodychunk __P((genarg *));
static int	st_connectinfo __P((genarg *));
static int	st_bodyend __P((genarg *));
static int	st_helo __P((genarg *));
static int	st_header __P((genarg *));
static int	st_sender __P((genarg *));
static int	st_rcpt __P((genarg *));
#if SMFI_VERSION > 2
static int	st_unknown __P((genarg *));
#endif /* SMFI_VERSION > 2 */
#if SMFI_VERSION > 3
static int	st_data __P((genarg *));
#endif /* SMFI_VERSION > 3 */
static int	st_eoh __P((genarg *));
static int	st_quit __P((genarg *));
static int	sendreply __P((sfsistat, socket_t, struct timeval *, SMFICTX_PTR));
static void	fix_stm __P((SMFICTX_PTR));
static bool	trans_ok __P((int, int));
static char	**dec_argv __P((char *, size_t));
static int	dec_arg2 __P((char *, size_t, char **, char **));

/* states */
#define ST_NONE	(-1)
#define ST_INIT	0	/* initial state */
#define ST_OPTS	1	/* option negotiation */
#define ST_CONN	2	/* connection info */
#define ST_HELO	3	/* helo */
#define ST_MAIL	4	/* mail from */
#define ST_RCPT	5	/* rcpt to */
#define ST_DATA	6	/* data */
#define ST_HDRS	7	/* headers */
#define ST_EOHS	8	/* end of headers */
#define ST_BODY	9	/* body */
#define ST_ENDM	10	/* end of message */
#define ST_QUIT	11	/* quit */
#define ST_ABRT	12	/* abort */
#define ST_UNKN 13	/* unknown SMTP command */
#define ST_LAST	ST_UNKN	/* last valid state */
#define ST_SKIP	15	/* not a state but required for the state table */

/* in a mail transaction? must be before eom according to spec. */
#define ST_IN_MAIL(st)	((st) >= ST_MAIL && (st) < ST_ENDM)

/*
**  set of next states
**  each state (ST_*) corresponds to bit in an int value (1 << state)
**  each state has a set of allowed transitions ('or' of bits of states)
**  so a state transition is valid if the mask of the next state
**  is set in the NX_* value
**  this function is coded in trans_ok(), see below.
*/

#define MI_MASK(x)	(0x0001 << (x))	/* generate a bit "mask" for a state */
#define NX_INIT	(MI_MASK(ST_OPTS))
#define NX_OPTS	(MI_MASK(ST_CONN) | MI_MASK(ST_UNKN))
#define NX_CONN	(MI_MASK(ST_HELO) | MI_MASK(ST_MAIL) | MI_MASK(ST_UNKN))
#define NX_HELO	(MI_MASK(ST_HELO) | MI_MASK(ST_MAIL) | MI_MASK(ST_UNKN))
#define NX_MAIL	(MI_MASK(ST_RCPT) | MI_MASK(ST_ABRT) | MI_MASK(ST_UNKN))
#define NX_RCPT	(MI_MASK(ST_HDRS) | MI_MASK(ST_EOHS) | MI_MASK(ST_DATA) | \
		 MI_MASK(ST_BODY) | MI_MASK(ST_ENDM) | \
		 MI_MASK(ST_RCPT) | MI_MASK(ST_ABRT) | MI_MASK(ST_UNKN))
#define NX_DATA	(MI_MASK(ST_EOHS) | MI_MASK(ST_HDRS) | MI_MASK(ST_ABRT))
#define NX_HDRS	(MI_MASK(ST_EOHS) | MI_MASK(ST_HDRS) | MI_MASK(ST_ABRT))
#define NX_EOHS	(MI_MASK(ST_BODY) | MI_MASK(ST_ENDM) | MI_MASK(ST_ABRT))
#define NX_BODY	(MI_MASK(ST_ENDM) | MI_MASK(ST_BODY) | MI_MASK(ST_ABRT))
#define NX_ENDM	(MI_MASK(ST_QUIT) | MI_MASK(ST_MAIL) | MI_MASK(ST_UNKN))
#define NX_QUIT	0
#define NX_ABRT	0
#define NX_UNKN (MI_MASK(ST_HELO) | MI_MASK(ST_MAIL) | \
		 MI_MASK(ST_RCPT) | MI_MASK(ST_ABRT) | \
		 MI_MASK(ST_DATA) | \
		 MI_MASK(ST_BODY) | MI_MASK(ST_UNKN) | \
		 MI_MASK(ST_ABRT) | MI_MASK(ST_QUIT))
#define NX_SKIP MI_MASK(ST_SKIP)

static int next_states[] =
{
	NX_INIT,
	NX_OPTS,
	NX_CONN,
	NX_HELO,
	NX_MAIL,
	NX_RCPT,
	NX_DATA,
	NX_HDRS,
	NX_EOHS,
	NX_BODY,
	NX_ENDM,
	NX_QUIT,
	NX_ABRT,
	NX_UNKN
};

#define SIZE_NEXT_STATES	(sizeof(next_states) / sizeof(next_states[0]))

/* commands received by milter */
static cmdfct cmds[] =
{
{SMFIC_ABORT,	CM_ARG0, ST_ABRT,  CT_CONT,	CI_NONE, st_abortfct	},
{SMFIC_MACRO,	CM_ARGV, ST_NONE,  CT_KEEP,	CI_NONE, st_macros	},
{SMFIC_BODY,	CM_ARG1, ST_BODY,  CT_CONT,	CI_NONE, st_bodychunk	},
{SMFIC_CONNECT,	CM_ARG2, ST_CONN,  CT_CONT,	CI_CONN, st_connectinfo	},
{SMFIC_BODYEOB,	CM_ARG1, ST_ENDM,  CT_CONT,	CI_EOM,  st_bodyend	},
{SMFIC_HELO,	CM_ARG1, ST_HELO,  CT_CONT,	CI_HELO, st_helo	},
{SMFIC_HEADER,	CM_ARG2, ST_HDRS,  CT_CONT,	CI_NONE, st_header	},
{SMFIC_MAIL,	CM_ARGV, ST_MAIL,  CT_CONT,	CI_MAIL, st_sender	},
{SMFIC_OPTNEG,	CM_ARGO, ST_OPTS,  CT_CONT,	CI_NONE, st_optionneg	},
{SMFIC_EOH,	CM_ARG0, ST_EOHS,  CT_CONT,	CI_NONE, st_eoh		},
{SMFIC_QUIT,	CM_ARG0, ST_QUIT,  CT_END,	CI_NONE, st_quit	},
#if SMFI_VERSION > 3
{SMFIC_DATA,	CM_ARG0, ST_DATA,  CT_CONT,	CI_NONE, st_data	},
#endif /* SMFI_VERSION > 3 */
{SMFIC_RCPT,	CM_ARGV, ST_RCPT,  CT_IGNO,	CI_RCPT, st_rcpt	}
#if SMFI_VERSION > 2
,{SMFIC_UNKNOWN,CM_ARG1, ST_UNKN,  CT_IGNO,	CI_NONE, st_unknown	}
#endif /* SMFI_VERSION > 2 */
};

/* additional (internal) reply codes */
#define _SMFIS_KEEP	20
#define _SMFIS_ABORT	21
#define _SMFIS_OPTIONS	22
#define _SMFIS_NOREPLY	23
#define _SMFIS_FAIL	(-1)
#define _SMFIS_NONE	(-2)

/*
**  MI_ENGINE -- receive commands and process them
**
**	Parameters:
**		ctx -- context structure
**
**	Returns:
**		MI_FAILURE/MI_SUCCESS
*/
int
mi_engine(ctx)
	SMFICTX_PTR ctx;
{
	size_t len;
	int i;
	socket_t sd;
	int ret = MI_SUCCESS;
	int ncmds = sizeof(cmds) / sizeof(cmdfct);
	int curstate = ST_INIT;
	int newstate;
	bool call_abort;
	sfsistat r;
	char cmd;
	char *buf = NULL;
	genarg arg;
	struct timeval timeout;
	int (*f) __P((genarg *));
	sfsistat (*fi_abort) __P((SMFICTX *));
	sfsistat (*fi_close) __P((SMFICTX *));

	arg.a_ctx = ctx;
	sd = ctx->ctx_sd;
	fi_abort = ctx->ctx_smfi->xxfi_abort;
	mi_clr_macros(ctx, 0);
	fix_stm(ctx);
	r = _SMFIS_NONE;
	do
	{
		/* call abort only if in a mail transaction */
		call_abort = ST_IN_MAIL(curstate);
		timeout.tv_sec = ctx->ctx_timeout;
		timeout.tv_usec = 0;
		if (mi_stop() == MILTER_ABRT)
		{
			if (ctx->ctx_dbg > 3)
				sm_dprintf("[%d] milter_abort\n",
					(int) ctx->ctx_id);
			ret = MI_FAILURE;
			break;
		}

		/*
		**  Notice: buf is allocated by mi_rd_cmd() and it will
		**  usually be free()d after it has been used in f().
		**  However, if the function returns _SMFIS_KEEP then buf
		**  contains macros and will not be free()d.
		**  Hence r must be set to _SMFIS_NONE if a new buf is
		**  allocated to avoid problem with housekeeping, esp.
		**  if the code "break"s out of the loop.
		*/

		r = _SMFIS_NONE;
		if ((buf = mi_rd_cmd(sd, &timeout, &cmd, &len,
				     ctx->ctx_smfi->xxfi_name)) == NULL &&
		    cmd < SMFIC_VALIDCMD)
		{
			if (ctx->ctx_dbg > 5)
				sm_dprintf("[%d] mi_engine: mi_rd_cmd error (%x)\n",
					(int) ctx->ctx_id, (int) cmd);

			/*
			**  eof is currently treated as failure ->
			**  abort() instead of close(), otherwise use:
			**  if (cmd != SMFIC_EOF)
			*/

			ret = MI_FAILURE;
			break;
		}
		if (ctx->ctx_dbg > 4)
			sm_dprintf("[%d] got cmd '%c' len %d\n",
				(int) ctx->ctx_id, cmd, (int) len);
		for (i = 0; i < ncmds; i++)
		{
			if (cmd == cmds[i].cm_cmd)
				break;
		}
		if (i >= ncmds)
		{
			/* unknown command */
			if (ctx->ctx_dbg > 1)
				sm_dprintf("[%d] cmd '%c' unknown\n",
					(int) ctx->ctx_id, cmd);
			ret = MI_FAILURE;
			break;
		}
		if ((f = cmds[i].cm_fct) == NULL)
		{
			/* stop for now */
			if (ctx->ctx_dbg > 1)
				sm_dprintf("[%d] cmd '%c' not impl\n",
					(int) ctx->ctx_id, cmd);
			ret = MI_FAILURE;
			break;
		}

		/* is new state ok? */
		newstate = cmds[i].cm_next;
		if (ctx->ctx_dbg > 5)
			sm_dprintf("[%d] cur %x new %x nextmask %x\n",
				(int) ctx->ctx_id,
				curstate, newstate, next_states[curstate]);

		if (newstate != ST_NONE && !trans_ok(curstate, newstate))
		{
			if (ctx->ctx_dbg > 1)
				sm_dprintf("[%d] abort: cur %d (%x) new %d (%x) next %x\n",
					(int) ctx->ctx_id,
					curstate, MI_MASK(curstate),
					newstate, MI_MASK(newstate),
					next_states[curstate]);

			/* call abort only if in a mail transaction */
			if (fi_abort != NULL && call_abort)
				(void) (*fi_abort)(ctx);

			/*
			**  try to reach the new state from HELO
			**  if it can't be reached, ignore the command.
			*/

			curstate = ST_HELO;
			if (!trans_ok(curstate, newstate))
			{
				if (buf != NULL)
				{
					free(buf);
					buf = NULL;
				}
				continue;
			}
		}
		arg.a_len = len;
		arg.a_buf = buf;
		if (newstate != ST_NONE)
		{
			curstate = newstate;
			ctx->ctx_state = curstate;
		}
		arg.a_idx = cmds[i].cm_macros;
		call_abort = ST_IN_MAIL(curstate);

		/* call function to deal with command */
		r = (*f)(&arg);
		if (r != _SMFIS_KEEP && buf != NULL)
		{
			free(buf);
			buf = NULL;
		}
		if (sendreply(r, sd, &timeout, ctx) != MI_SUCCESS)
		{
			ret = MI_FAILURE;
			break;
		}

		if (r == SMFIS_ACCEPT)
		{
			/* accept mail, no further actions taken */
			curstate = ST_HELO;
		}
		else if (r == SMFIS_REJECT || r == SMFIS_DISCARD ||
			 r ==  SMFIS_TEMPFAIL)
		{
			/*
			**  further actions depend on current state
			**  if the IGNO bit is set: "ignore" the error,
			**  i.e., stay in the current state
			*/
			if (!bitset(CT_IGNO, cmds[i].cm_todo))
				curstate = ST_HELO;
		}
		else if (r == _SMFIS_ABORT)
		{
			if (ctx->ctx_dbg > 5)
				sm_dprintf("[%d] function returned abort\n",
					(int) ctx->ctx_id);
			ret = MI_FAILURE;
			break;
		}
	} while (!bitset(CT_END, cmds[i].cm_todo));

	if (ret != MI_SUCCESS)
	{
		/* call abort only if in a mail transaction */
		if (fi_abort != NULL && call_abort)
			(void) (*fi_abort)(ctx);
	}

	/* close must always be called */
	if ((fi_close = ctx->ctx_smfi->xxfi_close) != NULL)
		(void) (*fi_close)(ctx);
	if (r != _SMFIS_KEEP && buf != NULL)
		free(buf);
	mi_clr_macros(ctx, 0);
	return ret;
}
/*
**  SENDREPLY -- send a reply to the MTA
**
**	Parameters:
**		r -- reply code
**		sd -- socket descriptor
**		timeout_ptr -- (ptr to) timeout to use for sending
**		ctx -- context structure
**
**	Returns:
**		MI_SUCCESS/MI_FAILURE
*/

static int
sendreply(r, sd, timeout_ptr, ctx)
	sfsistat r;
	socket_t sd;
	struct timeval *timeout_ptr;
	SMFICTX_PTR ctx;
{
	int ret = MI_SUCCESS;

	switch (r)
	{
	  case SMFIS_CONTINUE:
		ret = mi_wr_cmd(sd, timeout_ptr, SMFIR_CONTINUE, NULL, 0);
		break;
	  case SMFIS_TEMPFAIL:
	  case SMFIS_REJECT:
		if (ctx->ctx_reply != NULL &&
		    ((r == SMFIS_TEMPFAIL && *ctx->ctx_reply == '4') ||
		     (r == SMFIS_REJECT && *ctx->ctx_reply == '5')))
		{
			ret = mi_wr_cmd(sd, timeout_ptr, SMFIR_REPLYCODE,
					ctx->ctx_reply,
					strlen(ctx->ctx_reply) + 1);
			free(ctx->ctx_reply);
			ctx->ctx_reply = NULL;
		}
		else
		{
			ret = mi_wr_cmd(sd, timeout_ptr, r == SMFIS_REJECT ?
					SMFIR_REJECT : SMFIR_TEMPFAIL, NULL, 0);
		}
		break;
	  case SMFIS_DISCARD:
		ret = mi_wr_cmd(sd, timeout_ptr, SMFIR_DISCARD, NULL, 0);
		break;
	  case SMFIS_ACCEPT:
		ret = mi_wr_cmd(sd, timeout_ptr, SMFIR_ACCEPT, NULL, 0);
		break;
	  case _SMFIS_OPTIONS:
		{
			char buf[MILTER_OPTLEN];
			mi_int32 v;

			v = htonl(ctx->ctx_smfi->xxfi_version);
			(void) memcpy(&(buf[0]), (void *) &v, MILTER_LEN_BYTES);
			v = htonl(ctx->ctx_smfi->xxfi_flags);
			(void) memcpy(&(buf[MILTER_LEN_BYTES]), (void *) &v,
				      MILTER_LEN_BYTES);
			v = htonl(ctx->ctx_pflags);
			(void) memcpy(&(buf[MILTER_LEN_BYTES * 2]), (void *) &v,
				      MILTER_LEN_BYTES);
			ret = mi_wr_cmd(sd, timeout_ptr, SMFIC_OPTNEG, buf,
				       MILTER_OPTLEN);
		}
		break;
	  default:	/* don't send a reply */
		break;
	}
	return ret;
}

/*
**  CLR_MACROS -- clear set of macros starting from a given index
**
**	Parameters:
**		ctx -- context structure
**		m -- index from which to clear all macros
**
**	Returns:
**		None.
*/
void
mi_clr_macros(ctx, m)
	SMFICTX_PTR ctx;
	int m;
{
	int i;

	for (i = m; i < MAX_MACROS_ENTRIES; i++)
	{
		if (ctx->ctx_mac_ptr[i] != NULL)
		{
			free(ctx->ctx_mac_ptr[i]);
			ctx->ctx_mac_ptr[i] = NULL;
		}
		if (ctx->ctx_mac_buf[i] != NULL)
		{
			free(ctx->ctx_mac_buf[i]);
			ctx->ctx_mac_buf[i] = NULL;
		}
	}
}
/*
**  ST_OPTIONNEG -- negotiate options
**
**	Parameters:
**		g -- generic argument structure
**
**	Returns:
**		abort/send options/continue
*/

static int
st_optionneg(g)
	genarg *g;
{
	mi_int32 i, v;

	if (g == NULL || g->a_ctx->ctx_smfi == NULL)
		return SMFIS_CONTINUE;
	mi_clr_macros(g->a_ctx, g->a_idx + 1);

	/* check for minimum length */
	if (g->a_len < MILTER_OPTLEN)
	{
		smi_log(SMI_LOG_ERR,
			"%s: st_optionneg[%d]: len too short %d < %d",
			g->a_ctx->ctx_smfi->xxfi_name,
			(int) g->a_ctx->ctx_id, (int) g->a_len,
			MILTER_OPTLEN);
		return _SMFIS_ABORT;
	}

	(void) memcpy((void *) &i, (void *) &(g->a_buf[0]),
		      MILTER_LEN_BYTES);
	v = ntohl(i);
	if (v < g->a_ctx->ctx_smfi->xxfi_version)
	{
		/* hard failure for now! */
		smi_log(SMI_LOG_ERR,
			"%s: st_optionneg[%d]: version mismatch MTA: %d < milter: %d",
			g->a_ctx->ctx_smfi->xxfi_name,
			(int) g->a_ctx->ctx_id, (int) v,
			g->a_ctx->ctx_smfi->xxfi_version);
		return _SMFIS_ABORT;
	}

	(void) memcpy((void *) &i, (void *) &(g->a_buf[MILTER_LEN_BYTES]),
		      MILTER_LEN_BYTES);
	v = ntohl(i);

	/* no flags? set to default value for V1 actions */
	if (v == 0)
		v = SMFI_V1_ACTS;
	i = g->a_ctx->ctx_smfi->xxfi_flags;
	if ((v & i) != i)
	{
		smi_log(SMI_LOG_ERR,
			"%s: st_optionneg[%d]: 0x%x does not fulfill action requirements 0x%x",
			g->a_ctx->ctx_smfi->xxfi_name,
			(int) g->a_ctx->ctx_id, v, i);
		return _SMFIS_ABORT;
	}

	(void) memcpy((void *) &i, (void *) &(g->a_buf[MILTER_LEN_BYTES * 2]),
		      MILTER_LEN_BYTES);
	v = ntohl(i);

	/* no flags? set to default value for V1 protocol */
	if (v == 0)
		v = SMFI_V1_PROT;
	i = g->a_ctx->ctx_pflags;
	if ((v & i) != i)
	{
		smi_log(SMI_LOG_ERR,
			"%s: st_optionneg[%d]: 0x%x does not fulfill protocol requirements 0x%x",
			g->a_ctx->ctx_smfi->xxfi_name,
			(int) g->a_ctx->ctx_id, v, i);
		return _SMFIS_ABORT;
	}

	return _SMFIS_OPTIONS;
}
/*
**  ST_CONNECTINFO -- receive connection information
**
**	Parameters:
**		g -- generic argument structure
**
**	Returns:
**		continue or filter-specified value
*/

static int
st_connectinfo(g)
	genarg *g;
{
	size_t l;
	size_t i;
	char *s, family;
	unsigned short port = 0;
	_SOCK_ADDR sockaddr;
	sfsistat (*fi_connect) __P((SMFICTX *, char *, _SOCK_ADDR *));

	if (g == NULL)
		return _SMFIS_ABORT;
	mi_clr_macros(g->a_ctx, g->a_idx + 1);
	if (g->a_ctx->ctx_smfi == NULL ||
	    (fi_connect = g->a_ctx->ctx_smfi->xxfi_connect) == NULL)
		return SMFIS_CONTINUE;

	s = g->a_buf;
	i = 0;
	l = g->a_len;
	while (s[i] != '\0' && i <= l)
		++i;
	if (i + 1 >= l)
		return _SMFIS_ABORT;

	/* Move past trailing \0 in host string */
	i++;
	family = s[i++];
	(void) memset(&sockaddr, '\0', sizeof sockaddr);
	if (family != SMFIA_UNKNOWN)
	{
		if (i + sizeof port >= l)
		{
			smi_log(SMI_LOG_ERR,
				"%s: connect[%d]: wrong len %d >= %d",
				g->a_ctx->ctx_smfi->xxfi_name,
				(int) g->a_ctx->ctx_id, (int) i, (int) l);
			return _SMFIS_ABORT;
		}
		(void) memcpy((void *) &port, (void *) (s + i),
			      sizeof port);
		i += sizeof port;

		/* make sure string is terminated */
		if (s[l - 1] != '\0')
			return _SMFIS_ABORT;
# if NETINET
		if (family == SMFIA_INET)
		{
			if (inet_aton(s + i, (struct in_addr *) &sockaddr.sin.sin_addr)
			    != 1)
			{
				smi_log(SMI_LOG_ERR,
					"%s: connect[%d]: inet_aton failed",
					g->a_ctx->ctx_smfi->xxfi_name,
					(int) g->a_ctx->ctx_id);
				return _SMFIS_ABORT;
			}
			sockaddr.sa.sa_family = AF_INET;
			if (port > 0)
				sockaddr.sin.sin_port = port;
		}
		else
# endif /* NETINET */
# if NETINET6
		if (family == SMFIA_INET6)
		{
			if (mi_inet_pton(AF_INET6, s + i,
					 &sockaddr.sin6.sin6_addr) != 1)
			{
				smi_log(SMI_LOG_ERR,
					"%s: connect[%d]: mi_inet_pton failed",
					g->a_ctx->ctx_smfi->xxfi_name,
					(int) g->a_ctx->ctx_id);
				return _SMFIS_ABORT;
			}
			sockaddr.sa.sa_family = AF_INET6;
			if (port > 0)
				sockaddr.sin6.sin6_port = port;
		}
		else
# endif /* NETINET6 */
# if NETUNIX
		if (family == SMFIA_UNIX)
		{
			if (sm_strlcpy(sockaddr.sunix.sun_path, s + i,
			    sizeof sockaddr.sunix.sun_path) >=
			    sizeof sockaddr.sunix.sun_path)
			{
				smi_log(SMI_LOG_ERR,
					"%s: connect[%d]: path too long",
					g->a_ctx->ctx_smfi->xxfi_name,
					(int) g->a_ctx->ctx_id);
				return _SMFIS_ABORT;
			}
			sockaddr.sunix.sun_family = AF_UNIX;
		}
		else
# endif /* NETUNIX */
		{
			smi_log(SMI_LOG_ERR,
				"%s: connect[%d]: unknown family %d",
				g->a_ctx->ctx_smfi->xxfi_name,
				(int) g->a_ctx->ctx_id, family);
			return _SMFIS_ABORT;
		}
	}
	return (*fi_connect)(g->a_ctx, g->a_buf,
			     family != SMFIA_UNKNOWN ? &sockaddr : NULL);
}

/*
**  ST_EOH -- end of headers
**
**	Parameters:
**		g -- generic argument structure
**
**	Returns:
**		continue or filter-specified value
*/

static int
st_eoh(g)
	genarg *g;
{
	sfsistat (*fi_eoh) __P((SMFICTX *));

	if (g == NULL)
		return _SMFIS_ABORT;
	if (g->a_ctx->ctx_smfi != NULL &&
	    (fi_eoh = g->a_ctx->ctx_smfi->xxfi_eoh) != NULL)
		return (*fi_eoh)(g->a_ctx);
	return SMFIS_CONTINUE;
}

#if SMFI_VERSION > 3
/*
**  ST_DATA -- DATA command
**
**	Parameters:
**		g -- generic argument structure
**
**	Returns:
**		continue or filter-specified value
*/

static int
st_data(g)
	genarg *g;
{
	sfsistat (*fi_data) __P((SMFICTX *));

	if (g == NULL)
		return _SMFIS_ABORT;
	if (g->a_ctx->ctx_smfi != NULL &&
	    (fi_data = g->a_ctx->ctx_smfi->xxfi_data) != NULL)
		return (*fi_data)(g->a_ctx);
	return SMFIS_CONTINUE;
}
#endif /* SMFI_VERSION > 3 */

/*
**  ST_HELO -- helo/ehlo command
**
**	Parameters:
**		g -- generic argument structure
**
**	Returns:
**		continue or filter-specified value
*/
static int
st_helo(g)
	genarg *g;
{
	sfsistat (*fi_helo) __P((SMFICTX *, char *));

	if (g == NULL)
		return _SMFIS_ABORT;
	mi_clr_macros(g->a_ctx, g->a_idx + 1);
	if (g->a_ctx->ctx_smfi != NULL &&
	    (fi_helo = g->a_ctx->ctx_smfi->xxfi_helo) != NULL)
	{
		/* paranoia: check for terminating '\0' */
		if (g->a_len == 0 || g->a_buf[g->a_len - 1] != '\0')
			return MI_FAILURE;
		return (*fi_helo)(g->a_ctx, g->a_buf);
	}
	return SMFIS_CONTINUE;
}
/*
**  ST_HEADER -- header line
**
**	Parameters:
**		g -- generic argument structure
**
**	Returns:
**		continue or filter-specified value
*/

static int
st_header(g)
	genarg *g;
{
	char *hf, *hv;
	sfsistat (*fi_header) __P((SMFICTX *, char *, char *));

	if (g == NULL)
		return _SMFIS_ABORT;
	if (g->a_ctx->ctx_smfi == NULL ||
	    (fi_header = g->a_ctx->ctx_smfi->xxfi_header) == NULL)
		return SMFIS_CONTINUE;
	if (dec_arg2(g->a_buf, g->a_len, &hf, &hv) == MI_SUCCESS)
		return (*fi_header)(g->a_ctx, hf, hv);
	else
		return _SMFIS_ABORT;
}

#define ARGV_FCT(lf, rf, idx)					\
	char **argv;						\
	sfsistat (*lf) __P((SMFICTX *, char **));		\
	int r;							\
								\
	if (g == NULL)						\
		return _SMFIS_ABORT;				\
	mi_clr_macros(g->a_ctx, g->a_idx + 1);			\
	if (g->a_ctx->ctx_smfi == NULL ||			\
	    (lf = g->a_ctx->ctx_smfi->rf) == NULL)		\
		return SMFIS_CONTINUE;				\
	if ((argv = dec_argv(g->a_buf, g->a_len)) == NULL)	\
		return _SMFIS_ABORT;				\
	r = (*lf)(g->a_ctx, argv);				\
	free(argv);						\
	return r;

/*
**  ST_SENDER -- MAIL FROM command
**
**	Parameters:
**		g -- generic argument structure
**
**	Returns:
**		continue or filter-specified value
*/

static int
st_sender(g)
	genarg *g;
{
	ARGV_FCT(fi_envfrom, xxfi_envfrom, CI_MAIL)
}
/*
**  ST_RCPT -- RCPT TO command
**
**	Parameters:
**		g -- generic argument structure
**
**	Returns:
**		continue or filter-specified value
*/

static int
st_rcpt(g)
	genarg *g;
{
	ARGV_FCT(fi_envrcpt, xxfi_envrcpt, CI_RCPT)
}

#if SMFI_VERSION > 2
/*
**  ST_UNKNOWN -- unrecognized or unimplemented command
**
**	Parameters:
**		g -- generic argument structure
**
**	Returns:
**		continue or filter-specified value
*/

static int
st_unknown(g)
	genarg *g;
{
	sfsistat (*fi_unknown) __P((SMFICTX *, char *));

	if (g == NULL)
		return _SMFIS_ABORT;
	mi_clr_macros(g->a_ctx, g->a_idx + 1);
	if (g->a_ctx->ctx_smfi != NULL &&
	    (fi_unknown = g->a_ctx->ctx_smfi->xxfi_unknown) != NULL)
		return (*fi_unknown)(g->a_ctx, g->a_buf);
	return SMFIS_CONTINUE;
}
#endif /* SMFI_VERSION > 2 */

/*
**  ST_MACROS -- deal with macros received from the MTA
**
**	Parameters:
**		g -- generic argument structure
**
**	Returns:
**		continue/keep
**
**	Side effects:
**		set pointer in macro array to current values.
*/

static int
st_macros(g)
	genarg *g;
{
	int i;
	char **argv;

	if (g == NULL || g->a_len < 1)
		return _SMFIS_FAIL;
	if ((argv = dec_argv(g->a_buf + 1, g->a_len - 1)) == NULL)
		return _SMFIS_FAIL;
	switch (g->a_buf[0])
	{
	  case SMFIC_CONNECT:
		i = CI_CONN;
		break;
	  case SMFIC_HELO:
		i = CI_HELO;
		break;
	  case SMFIC_MAIL:
		i = CI_MAIL;
		break;
	  case SMFIC_RCPT:
		i = CI_RCPT;
		break;
	  case SMFIC_BODYEOB:
		i = CI_EOM;
		break;
	  default:
		free(argv);
		return _SMFIS_FAIL;
	}
	if (g->a_ctx->ctx_mac_ptr[i] != NULL)
		free(g->a_ctx->ctx_mac_ptr[i]);
	if (g->a_ctx->ctx_mac_buf[i] != NULL)
		free(g->a_ctx->ctx_mac_buf[i]);
	g->a_ctx->ctx_mac_ptr[i] = argv;
	g->a_ctx->ctx_mac_buf[i] = g->a_buf;
	return _SMFIS_KEEP;
}
/*
**  ST_QUIT -- quit command
**
**	Parameters:
**		g -- generic argument structure
**
**	Returns:
**		noreply
*/

/* ARGSUSED */
static int
st_quit(g)
	genarg *g;
{
	return _SMFIS_NOREPLY;
}
/*
**  ST_BODYCHUNK -- deal with a piece of the mail body
**
**	Parameters:
**		g -- generic argument structure
**
**	Returns:
**		continue or filter-specified value
*/

static int
st_bodychunk(g)
	genarg *g;
{
	sfsistat (*fi_body) __P((SMFICTX *, unsigned char *, size_t));

	if (g == NULL)
		return _SMFIS_ABORT;
	if (g->a_ctx->ctx_smfi != NULL &&
	    (fi_body = g->a_ctx->ctx_smfi->xxfi_body) != NULL)
		return (*fi_body)(g->a_ctx, (unsigned char *)g->a_buf,
				  g->a_len);
	return SMFIS_CONTINUE;
}
/*
**  ST_BODYEND -- deal with the last piece of the mail body
**
**	Parameters:
**		g -- generic argument structure
**
**	Returns:
**		continue or filter-specified value
**
**	Side effects:
**		sends a reply for the body part (if non-empty).
*/

static int
st_bodyend(g)
	genarg *g;
{
	sfsistat r;
	sfsistat (*fi_body) __P((SMFICTX *, unsigned char *, size_t));
	sfsistat (*fi_eom) __P((SMFICTX *));

	if (g == NULL)
		return _SMFIS_ABORT;
	r = SMFIS_CONTINUE;
	if (g->a_ctx->ctx_smfi != NULL)
	{
		if ((fi_body = g->a_ctx->ctx_smfi->xxfi_body) != NULL &&
		    g->a_len > 0)
		{
			socket_t sd;
			struct timeval timeout;

			timeout.tv_sec = g->a_ctx->ctx_timeout;
			timeout.tv_usec = 0;
			sd = g->a_ctx->ctx_sd;
			r = (*fi_body)(g->a_ctx, (unsigned char *)g->a_buf,
				       g->a_len);
			if (r != SMFIS_CONTINUE &&
			    sendreply(r, sd, &timeout, g->a_ctx) != MI_SUCCESS)
				return _SMFIS_ABORT;
		}
	}
	if (r == SMFIS_CONTINUE &&
	    (fi_eom = g->a_ctx->ctx_smfi->xxfi_eom) != NULL)
		return (*fi_eom)(g->a_ctx);
	return r;
}
/*
**  ST_ABORTFCT -- deal with aborts
**
**	Parameters:
**		g -- generic argument structure
**
**	Returns:
**		abort or filter-specified value
*/

static int
st_abortfct(g)
	genarg *g;
{
	sfsistat (*fi_abort) __P((SMFICTX *));

	if (g == NULL)
		return _SMFIS_ABORT;
	if (g != NULL && g->a_ctx->ctx_smfi != NULL &&
	    (fi_abort = g->a_ctx->ctx_smfi->xxfi_abort) != NULL)
		(void) (*fi_abort)(g->a_ctx);
	return _SMFIS_NOREPLY;
}
/*
**  TRANS_OK -- is the state transition ok?
**
**	Parameters:
**		old -- old state
**		new -- new state
**
**	Returns:
**		state transition ok
*/

static bool
trans_ok(old, new)
	int old, new;
{
	int s, n;

	s = old;
	if (s >= SIZE_NEXT_STATES)
		return false;
	do
	{
		/* is this state transition allowed? */
		if ((MI_MASK(new) & next_states[s]) != 0)
			return true;

		/*
		**  no: try next state;
		**  this works since the relevant states are ordered
		**  strict sequentially
		*/

		n = s + 1;
		if (n >= SIZE_NEXT_STATES)
			return false;

		/*
		**  can we actually "skip" this state?
		**  see fix_stm() which sets this bit for those
		**  states which the filter program is not interested in
		*/

		if (bitset(NX_SKIP, next_states[n]))
			s = n;
		else
			return false;
	} while (s < SIZE_NEXT_STATES);
	return false;
}
/*
**  FIX_STM -- add "skip" bits to the state transition table
**
**	Parameters:
**		ctx -- context structure
**
**	Returns:
**		None.
**
**	Side effects:
**		may change state transition table.
*/

static void
fix_stm(ctx)
	SMFICTX_PTR ctx;
{
	unsigned long fl;

	if (ctx == NULL || ctx->ctx_smfi == NULL)
		return;
	fl = ctx->ctx_pflags;
	if (bitset(SMFIP_NOCONNECT, fl))
		next_states[ST_CONN] |= NX_SKIP;
	if (bitset(SMFIP_NOHELO, fl))
		next_states[ST_HELO] |= NX_SKIP;
	if (bitset(SMFIP_NOMAIL, fl))
		next_states[ST_MAIL] |= NX_SKIP;
	if (bitset(SMFIP_NORCPT, fl))
		next_states[ST_RCPT] |= NX_SKIP;
	if (bitset(SMFIP_NOHDRS, fl))
		next_states[ST_HDRS] |= NX_SKIP;
	if (bitset(SMFIP_NOEOH, fl))
		next_states[ST_EOHS] |= NX_SKIP;
	if (bitset(SMFIP_NOBODY, fl))
		next_states[ST_BODY] |= NX_SKIP;
}
/*
**  DEC_ARGV -- split a buffer into a list of strings, NULL terminated
**
**	Parameters:
**		buf -- buffer with several strings
**		len -- length of buffer
**
**	Returns:
**		array of pointers to the individual strings
*/

static char **
dec_argv(buf, len)
	char *buf;
	size_t len;
{
	char **s;
	size_t i;
	int elem, nelem;

	nelem = 0;
	for (i = 0; i < len; i++)
	{
		if (buf[i] == '\0')
			++nelem;
	}
	if (nelem == 0)
		return NULL;

	/* last entry is only for the name */
	s = (char **)malloc((nelem + 1) * (sizeof *s));
	if (s == NULL)
		return NULL;
	s[0] = buf;
	for (i = 0, elem = 0; i < len && elem < nelem; i++)
	{
		if (buf[i] == '\0')
		{
			++elem;
			if (i + 1 >= len)
				s[elem] = NULL;
			else
				s[elem] = &(buf[i + 1]);
		}
	}

	/* overwrite last entry (already done above, just paranoia) */
	s[elem] = NULL;
	return s;
}
/*
**  DEC_ARG2 -- split a buffer into two strings
**
**	Parameters:
**		buf -- buffer with two strings
**		len -- length of buffer
**		s1,s2 -- pointer to result strings
**
**	Returns:
**		MI_FAILURE/MI_SUCCESS
*/

static int
dec_arg2(buf, len, s1, s2)
	char *buf;
	size_t len;
	char **s1;
	char **s2;
{
	size_t i;

	/* paranoia: check for terminating '\0' */
	if (len == 0 || buf[len - 1] != '\0')
		return MI_FAILURE;
	*s1 = buf;
	for (i = 1; i < len && buf[i] != '\0'; i++)
		continue;
	if (i >= len - 1)
		return MI_FAILURE;
	*s2 = buf + i + 1;
	return MI_SUCCESS;
}
/*
**  SENDOK -- is it ok for the filter to send stuff to the MTA?
**
**	Parameters:
**		ctx -- context structure
**		flag -- flag to check
**
**	Returns:
**		sending allowed (in current state)
*/

bool
mi_sendok(ctx, flag)
	SMFICTX_PTR ctx;
	int flag;
{
	if (ctx == NULL || ctx->ctx_smfi == NULL)
		return false;

	/* did the milter request this operation? */
	if (flag != 0 && !bitset(flag, ctx->ctx_smfi->xxfi_flags))
		return false;

	/* are we in the correct state? It must be "End of Message". */
	return ctx->ctx_state == ST_ENDM;
}
