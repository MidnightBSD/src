/*
 * Copyright (c) 1999-2001, 2003-2004 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
# ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <pwd.h>
#ifdef HAVE_ERR_H
# include <err.h>
#else
# include "emul/err.h"
#endif /* HAVE_ERR_H */
#include <ctype.h>

#include "sudo.h"

#ifndef lint
static const char rcsid[] = "$Sudo: defaults.c,v 1.48 2004/06/06 23:58:10 millert Exp $";
#endif /* lint */

/*
 * For converting between syslog numbers and strings.
 */
struct strmap {
    char *name;
    int num;
};

#ifdef LOG_NFACILITIES
static struct strmap facilities[] = {
#ifdef LOG_AUTHPRIV
	{ "authpriv",	LOG_AUTHPRIV },
#endif
	{ "auth",	LOG_AUTH },
	{ "daemon",	LOG_DAEMON },
	{ "user",	LOG_USER },
	{ "local0",	LOG_LOCAL0 },
	{ "local1",	LOG_LOCAL1 },
	{ "local2",	LOG_LOCAL2 },
	{ "local3",	LOG_LOCAL3 },
	{ "local4",	LOG_LOCAL4 },
	{ "local5",	LOG_LOCAL5 },
	{ "local6",	LOG_LOCAL6 },
	{ "local7",	LOG_LOCAL7 },
	{ NULL,		-1 }
};
#endif /* LOG_NFACILITIES */

static struct strmap priorities[] = {
	{ "alert",	LOG_ALERT },
	{ "crit",	LOG_CRIT },
	{ "debug",	LOG_DEBUG },
	{ "emerg",	LOG_EMERG },
	{ "err",	LOG_ERR },
	{ "info",	LOG_INFO },
	{ "notice",	LOG_NOTICE },
	{ "warning",	LOG_WARNING },
	{ NULL,		-1 }
};

extern int sudolineno;

/*
 * Local prototypes.
 */
static int store_int __P((char *, struct sudo_defs_types *, int));
static int store_list __P((char *, struct sudo_defs_types *, int));
static int store_mode __P((char *, struct sudo_defs_types *, int));
static int store_str __P((char *, struct sudo_defs_types *, int));
static int store_syslogfac __P((char *, struct sudo_defs_types *, int));
static int store_syslogpri __P((char *, struct sudo_defs_types *, int));
static int store_tuple __P((char *, struct sudo_defs_types *, int));
static int store_uint __P((char *, struct sudo_defs_types *, int));
static void list_op __P((char *, size_t, struct sudo_defs_types *, enum list_ops));
static const char *logfac2str __P((int));
static const char *logpri2str __P((int));

/*
 * Table describing compile-time and run-time options.
 */
#include <def_data.c>

/*
 * Print version and configure info.
 */
void
dump_defaults()
{
    struct sudo_defs_types *cur;
    struct list_member *item;
    struct def_values *def;

    for (cur = sudo_defs_table; cur->name; cur++) {
	if (cur->desc) {
	    switch (cur->type & T_MASK) {
		case T_FLAG:
		    if (cur->sd_un.flag)
			puts(cur->desc);
		    break;
		case T_STR:
		    if (cur->sd_un.str) {
			(void) printf(cur->desc, cur->sd_un.str);
			putchar('\n');
		    }
		    break;
		case T_LOGFAC:
		    if (cur->sd_un.ival) {
			(void) printf(cur->desc, logfac2str(cur->sd_un.ival));
			putchar('\n');
		    }
		    break;
		case T_LOGPRI:
		    if (cur->sd_un.ival) {
			(void) printf(cur->desc, logpri2str(cur->sd_un.ival));
			putchar('\n');
		    }
		    break;
		case T_UINT:
		case T_INT:
		    (void) printf(cur->desc, cur->sd_un.ival);
		    putchar('\n');
		    break;
		case T_MODE:
		    (void) printf(cur->desc, cur->sd_un.mode);
		    putchar('\n');
		    break;
		case T_LIST:
		    if (cur->sd_un.list) {
			puts(cur->desc);
			for (item = cur->sd_un.list; item; item = item->next)
			    printf("\t%s\n", item->value);
		    }
		    break;
		case T_TUPLE:
		    for (def = cur->values; def->sval; def++) {
			if (cur->sd_un.ival == def->ival) {
			    (void) printf(cur->desc, def->sval);
			    break;
			}
		    }
		    putchar('\n');
		    break;
	    }
	}
    }
}

/*
 * List each option along with its description.
 */
void
list_options()
{
    struct sudo_defs_types *cur;
    char *p;

    (void) puts("Available options in a sudoers ``Defaults'' line:\n");
    for (cur = sudo_defs_table; cur->name; cur++) {
	if (cur->name && cur->desc) {
	    switch (cur->type & T_MASK) {
		case T_FLAG:
		    (void) printf("%s: %s\n", cur->name, cur->desc);
		    break;
		default:
		    p = strrchr(cur->desc, ':');
		    if (p)
			(void) printf("%s: %.*s\n", cur->name,
			    (int) (p - cur->desc), cur->desc);
		    else
			(void) printf("%s: %s\n", cur->name, cur->desc);
		    break;
	    }
	}
    }
}

/*
 * Sets/clears an entry in the defaults structure
 * If a variable that takes a value is used in a boolean
 * context with op == 0, disable that variable.
 * Eg. you may want to turn off logging to a file for some hosts.
 * This is only meaningful for variables that are *optional*.
 */
int
set_default(var, val, op)
    char *var;
    char *val;
    int op;     /* TRUE or FALSE */
{
    struct sudo_defs_types *cur;
    int num;

    for (cur = sudo_defs_table, num = 0; cur->name; cur++, num++) {
	if (strcmp(var, cur->name) == 0)
	    break;
    }
    if (!cur->name) {
	warnx("unknown defaults entry `%s' referenced near line %d",
	    var, sudolineno);
	return(FALSE);
    }

    switch (cur->type & T_MASK) {
	case T_LOGFAC:
	    if (!store_syslogfac(val, cur, op)) {
		if (val)
		    warnx("value `%s' is invalid for option `%s'", val, var);
		else
		    warnx("no value specified for `%s' on line %d",
			var, sudolineno);
		return(FALSE);
	    }
	    break;
	case T_LOGPRI:
	    if (!store_syslogpri(val, cur, op)) {
		if (val)
		    warnx("value `%s' is invalid for option `%s'", val, var);
		else
		    warnx("no value specified for `%s' on line %d",
			var, sudolineno);
		return(FALSE);
	    }
	    break;
	case T_STR:
	    if (!val) {
		/* Check for bogus boolean usage or lack of a value. */
		if (!ISSET(cur->type, T_BOOL) || op != FALSE) {
		    warnx("no value specified for `%s' on line %d",
			var, sudolineno);
		    return(FALSE);
		}
	    }
	    if (ISSET(cur->type, T_PATH) && val && *val != '/') {
		warnx("values for `%s' must start with a '/'", var);
		return(FALSE);
	    }
	    if (!store_str(val, cur, op)) {
		warnx("value `%s' is invalid for option `%s'", val, var);
		return(FALSE);
	    }
	    break;
	case T_INT:
	    if (!val) {
		/* Check for bogus boolean usage or lack of a value. */
		if (!ISSET(cur->type, T_BOOL) || op != FALSE) {
		    warnx("no value specified for `%s' on line %d",
			var, sudolineno);
		    return(FALSE);
		}
	    }
	    if (!store_int(val, cur, op)) {
		warnx("value `%s' is invalid for option `%s'", val, var);
		return(FALSE);
	    }
	    break;
	case T_UINT:
	    if (!val) {
		/* Check for bogus boolean usage or lack of a value. */
		if (!ISSET(cur->type, T_BOOL) || op != FALSE) {
		    warnx("no value specified for `%s' on line %d",
			var, sudolineno);
		    return(FALSE);
		}
	    }
	    if (!store_uint(val, cur, op)) {
		warnx("value `%s' is invalid for option `%s'", val, var);
		return(FALSE);
	    }
	    break;
	case T_MODE:
	    if (!val) {
		/* Check for bogus boolean usage or lack of a value. */
		if (!ISSET(cur->type, T_BOOL) || op != FALSE) {
		    warnx("no value specified for `%s' on line %d",
			var, sudolineno);
		    return(FALSE);
		}
	    }
	    if (!store_mode(val, cur, op)) {
		warnx("value `%s' is invalid for option `%s'", val, var);
		return(FALSE);
	    }
	    break;
	case T_FLAG:
	    if (val) {
		warnx("option `%s' does not take a value on line %d",
		    var, sudolineno);
		return(FALSE);
	    }
	    cur->sd_un.flag = op;

	    /* Special action for I_FQDN.  Move to own switch if we get more */
	    if (num == I_FQDN && op)
		set_fqdn();
	    break;
	case T_LIST:
	    if (!val) {
		/* Check for bogus boolean usage or lack of a value. */
		if (!ISSET(cur->type, T_BOOL) || op != FALSE) {
		    warnx("no value specified for `%s' on line %d",
			var, sudolineno);
		    return(FALSE);
		}
	    }
	    if (!store_list(val, cur, op)) {
		warnx("value `%s' is invalid for option `%s'", val, var);
		return(FALSE);
	    }
	    break;
	case T_TUPLE:
	    if (!val && !ISSET(cur->type, T_BOOL)) {
		warnx("no value specified for `%s' on line %d",
		    var, sudolineno);
		return(FALSE);
	    }
	    if (!store_tuple(val, cur, op)) {
		warnx("value `%s' is invalid for option `%s'", val, var);
		return(FALSE);
	    }
	    break;
    }

    return(TRUE);
}

/*
 * Set default options to compiled-in values.
 * Any of these may be overridden at runtime by a "Defaults" file.
 */
void
init_defaults()
{
    static int firsttime = 1;
    struct sudo_defs_types *def;

    /* Free any strings that were set. */
    if (!firsttime) {
	for (def = sudo_defs_table; def->name; def++)
	    switch (def->type & T_MASK) {
		case T_STR:
		    if (def->sd_un.str) {
			free(def->sd_un.str);
			def->sd_un.str = NULL;
		    }
		    break;
		case T_LIST:
		    list_op(NULL, 0, def, freeall);
		    break;
	    }
    }

    /* First initialize the flags. */
#ifdef LONG_OTP_PROMPT
    def_long_otp_prompt = TRUE;
#endif
#ifdef IGNORE_DOT_PATH
    def_ignore_dot = TRUE;
#endif
#ifdef ALWAYS_SEND_MAIL
    def_mail_always = TRUE;
#endif
#ifdef SEND_MAIL_WHEN_NO_USER
    def_mail_no_user = TRUE;
#endif
#ifdef SEND_MAIL_WHEN_NO_HOST
    def_mail_no_host = TRUE;
#endif
#ifdef SEND_MAIL_WHEN_NOT_OK
    def_mail_no_perms = TRUE;
#endif
#ifdef USE_TTY_TICKETS
    def_tty_tickets = TRUE;
#endif
#ifndef NO_LECTURE
    def_lecture = once;
#endif
#ifndef NO_AUTHENTICATION
    def_authenticate = TRUE;
#endif
#ifndef NO_ROOT_SUDO
    def_root_sudo = TRUE;
#endif
#ifdef HOST_IN_LOG
    def_log_host = TRUE;
#endif
#ifdef SHELL_IF_NO_ARGS
    def_shell_noargs = TRUE;
#endif
#ifdef SHELL_SETS_HOME
    def_set_home = TRUE;
#endif
#ifndef DONT_LEAK_PATH_INFO
    def_path_info = TRUE;
#endif
#ifdef FQDN
    def_fqdn = TRUE;
#endif
#ifdef USE_INSULTS
    def_insults = TRUE;
#endif
#ifdef ENV_EDITOR
    def_env_editor = TRUE;
#endif
    def_set_logname = TRUE;

    /* Syslog options need special care since they both strings and ints */
#if (LOGGING & SLOG_SYSLOG)
    (void) store_syslogfac(LOGFAC, &sudo_defs_table[I_SYSLOG], TRUE);
    (void) store_syslogpri(PRI_SUCCESS, &sudo_defs_table[I_SYSLOG_GOODPRI],
	TRUE);
    (void) store_syslogpri(PRI_FAILURE, &sudo_defs_table[I_SYSLOG_BADPRI],
	TRUE);
#endif

    /* Password flags also have a string and integer component. */
    (void) store_tuple("any", &sudo_defs_table[I_LISTPW], TRUE);
    (void) store_tuple("all", &sudo_defs_table[I_VERIFYPW], TRUE);

    /* Then initialize the int-like things. */
#ifdef SUDO_UMASK
    def_umask = SUDO_UMASK;
#else
    def_umask = 0777;
#endif
    def_loglinelen = MAXLOGFILELEN;
    def_timestamp_timeout = TIMEOUT;
    def_passwd_timeout = PASSWORD_TIMEOUT;
    def_passwd_tries = TRIES_FOR_PASSWORD;

    /* Now do the strings */
    def_mailto = estrdup(MAILTO);
    def_mailsub = estrdup(MAILSUBJECT);
    def_badpass_message = estrdup(INCORRECT_PASSWORD);
    def_timestampdir = estrdup(_PATH_SUDO_TIMEDIR);
    def_passprompt = estrdup(PASSPROMPT);
    def_runas_default = estrdup(RUNAS_DEFAULT);
#ifdef _PATH_SUDO_SENDMAIL
    def_mailerpath = estrdup(_PATH_SUDO_SENDMAIL);
    def_mailerflags = estrdup("-t");
#endif
#if (LOGGING & SLOG_FILE)
    def_logfile = estrdup(_PATH_SUDO_LOGFILE);
#endif
#ifdef EXEMPTGROUP
    def_exempt_group = estrdup(EXEMPTGROUP);
#endif
    def_editor = estrdup(EDITOR);
#ifdef _PATH_SUDO_NOEXEC
    def_noexec_file = estrdup(_PATH_SUDO_NOEXEC);
#endif

    /* Finally do the lists (currently just environment tables). */
    init_envtables();

    /*
     * The following depend on the above values.
     * We use a pointer to the string so that if its
     * value changes we get the change.
     */
    if (user_runas == NULL)
	user_runas = &def_runas_default;

    firsttime = 0;
}

static int
store_int(val, def, op)
    char *val;
    struct sudo_defs_types *def;
    int op;
{
    char *endp;
    long l;

    if (op == FALSE) {
	def->sd_un.ival = 0;
    } else {
	l = strtol(val, &endp, 10);
	if (*endp != '\0')
	    return(FALSE);
	/* XXX - should check against INT_MAX */
	def->sd_un.ival = (unsigned int)l;
    }
    if (def->callback)
	return(def->callback(val));
    return(TRUE);
}

static int
store_uint(val, def, op)
    char *val;
    struct sudo_defs_types *def;
    int op;
{
    char *endp;
    long l;

    if (op == FALSE) {
	def->sd_un.ival = 0;
    } else {
	l = strtol(val, &endp, 10);
	if (*endp != '\0' || l < 0)
	    return(FALSE);
	/* XXX - should check against INT_MAX */
	def->sd_un.ival = (unsigned int)l;
    }
    if (def->callback)
	return(def->callback(val));
    return(TRUE);
}

static int
store_tuple(val, def, op)
    char *val;
    struct sudo_defs_types *def;
    int op;
{
    struct def_values *v;

    /*
     * Since enums are really just ints we store the value as an ival.
     * In the future, there may be multiple enums for different tuple
     * types we want to avoid and special knowledge of the tuple type.
     * This does assume that the first entry in the tuple enum will
     * be the equivalent to a boolean "false".
     */
    if (!val) {
	def->sd_un.ival = (op == FALSE) ? 0 : 1;
    } else {
	for (v = def->values; v->sval != NULL; v++) {
	    if (strcmp(v->sval, val) == 0) {
		def->sd_un.ival = v->ival;
		break;
	    }
	}
	if (v->sval == NULL)
	    return(FALSE);
    }
    if (def->callback)
	return(def->callback(val));
    return(TRUE);
}

static int
store_str(val, def, op)
    char *val;
    struct sudo_defs_types *def;
    int op;
{

    if (def->sd_un.str)
	free(def->sd_un.str);
    if (op == FALSE)
	def->sd_un.str = NULL;
    else
	def->sd_un.str = estrdup(val);
    if (def->callback)
	return(def->callback(val));
    return(TRUE);
}

static int
store_list(str, def, op)
    char *str;
    struct sudo_defs_types *def;
    int op;
{
    char *start, *end;

    /* Remove all old members. */
    if (op == FALSE || op == TRUE)
	list_op(NULL, 0, def, freeall);

    /* Split str into multiple space-separated words and act on each one. */
    if (op != FALSE) {
	end = str;
	do {
	    /* Remove leading blanks, if nothing but blanks we are done. */
	    for (start = end; isblank(*start); start++)
		;
	    if (*start == '\0')
		break;

	    /* Find end position and perform operation. */
	    for (end = start; *end && !isblank(*end); end++)
		;
	    list_op(start, end - start, def, op == '-' ? delete : add);
	} while (*end++ != '\0');
    }
    return(TRUE);
}

static int
store_syslogfac(val, def, op)
    char *val;
    struct sudo_defs_types *def;
    int op;
{
    struct strmap *fac;

    if (op == FALSE) {
	def->sd_un.ival = FALSE;
	return(TRUE);
    }
#ifdef LOG_NFACILITIES
    if (!val)
	return(FALSE);
    for (fac = facilities; fac->name && strcmp(val, fac->name); fac++)
	;
    if (fac->name == NULL)
	return(FALSE);				/* not found */

    def->sd_un.ival = fac->num;
#else
    def->sd_un.ival = -1;
#endif /* LOG_NFACILITIES */
    return(TRUE);
}

static const char *
logfac2str(n)
    int n;
{
#ifdef LOG_NFACILITIES
    struct strmap *fac;

    for (fac = facilities; fac->name && fac->num != n; fac++)
	;
    return (fac->name);
#else
    return ("default");
#endif /* LOG_NFACILITIES */
}

static int
store_syslogpri(val, def, op)
    char *val;
    struct sudo_defs_types *def;
    int op;
{
    struct strmap *pri;

    if (op == FALSE || !val)
	return(FALSE);

    for (pri = priorities; pri->name && strcmp(val, pri->name); pri++)
	;
    if (pri->name == NULL)
	return(FALSE);				/* not found */

    def->sd_un.ival = pri->num;
    return(TRUE);
}

static const char *
logpri2str(n)
    int n;
{
    struct strmap *pri;

    for (pri = priorities; pri->name && pri->num != n; pri++)
	;
    return (pri->name);
}

static int
store_mode(val, def, op)
    char *val;
    struct sudo_defs_types *def;
    int op;
{
    char *endp;
    long l;

    if (op == FALSE) {
	def->sd_un.mode = (mode_t)0777;
    } else {
	l = strtol(val, &endp, 8);
	if (*endp != '\0' || l < 0 || l > 0777)
	    return(FALSE);
	def->sd_un.mode = (mode_t)l;
    }
    if (def->callback)
	return(def->callback(val));
    return(TRUE);
}

static void
list_op(val, len, def, op)
    char *val;
    size_t len;
    struct sudo_defs_types *def;
    enum list_ops op;
{
    struct list_member *cur, *prev, *tmp;

    if (op == freeall) {
	for (cur = def->sd_un.list; cur; ) {
	    tmp = cur;
	    cur = tmp->next;
	    free(tmp->value);
	    free(tmp);
	}
	def->sd_un.list = NULL;
	return;
    }

    for (cur = def->sd_un.list, prev = NULL; cur; prev = cur, cur = cur->next) {
	if ((strncmp(cur->value, val, len) == 0 && cur->value[len] == '\0')) {

	    if (op == add)
		return;			/* already exists */

	    /* Delete node */
	    if (prev != NULL)
		prev->next = cur->next;
	    else
		def->sd_un.list = cur->next;
	    free(cur->value);
	    free(cur);
	    break;
	}
    }

    /* Add new node to the head of the list. */
    if (op == add) {
	cur = emalloc(sizeof(struct list_member));
	cur->value = emalloc(len + 1);
	(void) memcpy(cur->value, val, len);
	cur->value[len] = '\0';
	cur->next = def->sd_un.list;
	def->sd_un.list = cur;
    }
}
