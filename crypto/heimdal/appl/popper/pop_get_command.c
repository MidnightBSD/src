/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <popper.h>
RCSID("$Id$");

/*
 *  get_command:    Extract the command from an input line form a POP client
 */

int pop_capa (POP *p);
static state_table states[] = {
        {auth1,  "user", 1,  1,  pop_user,   {auth1, auth2}},
        {auth2,  "pass", 1,  99, pop_pass,   {auth1, trans}},
#ifdef RPOP
        {auth2,  "rpop", 1,  1,  pop_rpop,   {auth1, trans}},
#endif /* RPOP */
#ifdef SASL
	{auth1,  "auth", 1,  2,  pop_auth,   {auth1, trans}},
#endif
        {auth1,  "quit", 0,  0,  pop_quit,   {halt,  halt}},
        {auth2,  "quit", 0,  0,  pop_quit,   {halt,  halt}},
#ifdef CAPA
	{auth1,  "capa", 0,  0,  pop_capa,   {auth1, auth1}},
	{auth2,  "capa", 0,  0,  pop_capa,   {auth2, auth2}},
	{trans,  "capa", 0,  0,  pop_capa,   {trans, trans}},
#endif
        {trans,  "stat", 0,  0,  pop_stat,   {trans, trans}},
        {trans,  "list", 0,  1,  pop_list,   {trans, trans}},
        {trans,  "retr", 1,  1,  pop_send,   {trans, trans}},
        {trans,  "dele", 1,  1,  pop_dele,   {trans, trans}},
        {trans,  "noop", 0,  0,  NULL,       {trans, trans}},
        {trans,  "rset", 0,  0,  pop_rset,   {trans, trans}},
        {trans,  "top",  2,  2,  pop_send,   {trans, trans}},
        {trans,  "last", 0,  0,  pop_last,   {trans, trans}},
        {trans,  "quit", 0,  0,  pop_updt,   {halt,  halt}},
	{trans,  "help", 0,  0,  pop_help,   {trans, trans}},
#ifdef UIDL
        {trans,  "uidl", 0,  1,  pop_uidl,   {trans, trans}},
#endif
#ifdef XOVER
	{trans,	"xover", 0,  0,	 pop_xover,  {trans, trans}},
#endif
#ifdef XDELE
        {trans,  "xdele", 1,  2,  pop_xdele,   {trans, trans}},
#endif
        {(state) 0,  NULL,   0,  0,  NULL,       {halt,  halt}},
};

int
pop_capa (POP *p)
{
    /*  Search for the POP command in the command/state table */
    pop_msg (p,POP_SUCCESS, "Capability list follows");
    if(p->auth_level == AUTH_NONE || p->auth_level == AUTH_OTP)
	fprintf(p->output, "USER\r\n");
    fprintf(p->output, "TOP\r\n");
    fprintf(p->output, "PIPELINING\r\n");
    fprintf(p->output, "EXPIRE NEVER\r\n");
    fprintf(p->output, "RESP-CODES\r\n");
#ifdef SASL
    pop_capa_sasl(p);
#endif
#ifdef UIDL
    fprintf(p->output, "UIDL\r\n");
#endif
#ifdef XOVER
    fprintf(p->output, "XOVER\r\n");
#endif
#ifdef XDELE
    fprintf(p->output, "XDELE\r\n");
#endif
    if(p->CurrentState == trans)
	fprintf(p->output, "IMPLEMENTATION %s-%s\r\n", PACKAGE, VERSION);
    fprintf(p->output,".\r\n");
    fflush(p->output);

    p->flags |= POP_FLAG_CAPA;

    return(POP_SUCCESS);
}

state_table *
pop_get_command(POP *p, char *mp)
{
    state_table     *   s;
    char                buf[MAXMSGLINELEN];

    /*  Save a copy of the original client line */
#ifdef DEBUG
    if(p->debug) strlcpy (buf, mp, sizeof(buf));
#endif /* DEBUG */

    /*  Parse the message into the parameter array */
    if ((p->parm_count = pop_parse(p,mp)) < 0) return(NULL);

    /*  Do not log cleartext passwords */
#ifdef DEBUG
    if(p->debug){
        if(strcmp(p->pop_command,"pass") == 0)
            pop_log(p,POP_DEBUG,"Received: \"%s xxxxxxxxx\"",p->pop_command);
        else {
            /*  Remove trailing <LF> */
            buf[strlen(buf)-2] = '\0';
            pop_log(p,POP_DEBUG,"Received: \"%s\"",buf);
        }
    }
#endif /* DEBUG */

    /*  Search for the POP command in the command/state table */
    for (s = states; s->command; s++) {

        /*  Is this a valid command for the current operating state? */
        if (strcmp(s->command,p->pop_command) == 0
             && s->ValidCurrentState == p->CurrentState) {

            /*  Were too few parameters passed to the command? */
            if (p->parm_count < s->min_parms) {
                pop_msg(p,POP_FAILURE,
			"Too few arguments for the %s command.",
			p->pop_command);
		return NULL;
	    }

            /*  Were too many parameters passed to the command? */
            if (p->parm_count > s->max_parms) {
                pop_msg(p,POP_FAILURE,
			"Too many arguments for the %s command.",
			p->pop_command);
		return NULL;
	   }

            /*  Return a pointer to the entry for this command in
                the command/state table */
            return (s);
        }
    }
    /*  The client command was not located in the command/state table */
    pop_msg(p,POP_FAILURE,
	    "Unknown command: \"%s\".",p->pop_command);
    return NULL;
}

int
pop_help (POP *p)
{
    state_table		*s;

    pop_msg(p, POP_SUCCESS, "help");

    for (s = states; s->command; s++) {
	fprintf (p->output, "%s\r\n", s->command);
    }
    fprintf (p->output, ".\r\n");
    fflush (p->output);
    return POP_SUCCESS;
}
