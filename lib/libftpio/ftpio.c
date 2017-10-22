/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * Major Changelog:
 *
 * Jordan K. Hubbard
 * 17 Jan 1996
 *
 * Turned inside out. Now returns xfers as new file ids, not as a special
 * `state' of FTP_t
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <ftpio.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SUCCESS		 0
#define FAILURE		-1

#ifndef TRUE
#define TRUE	(1)
#define FALSE	(0)
#endif

/* How to see by a given code whether or not the connection has timed out */
#define FTP_TIMEOUT(code)	(FtpTimedOut || code == FTP_TIMED_OUT)

/* Internal routines - deal only with internal FTP_t type */
static FTP_t	ftp_new(void);
static void	check_passive(FILE *fp);
static int	ftp_read_method(void *n, char *buf, int nbytes);
static int	ftp_write_method(void *n, const char *buf, int nbytes);
static int	ftp_close_method(void *n);
static int	writes(int fd, char *s);
static __inline char *get_a_line(FTP_t ftp);
static int	get_a_number(FTP_t ftp, char **q);
static int	botch(char *func, char *botch_state);
static int	cmd(FTP_t ftp, const char *fmt, ...);
static int	ftp_login_session(FTP_t ftp, char *host, int af, char *user, char *passwd, int port, int verbose);
static int	ftp_file_op(FTP_t ftp, char *operation, char *file, FILE **fp, char *mode, off_t *seekto);
static int	ftp_close(FTP_t ftp);
static int	get_url_info(char *url_in, char *host_ret, int *port_ret, char *name_ret);
static void	ftp_timeout(int sig);
static void	ftp_set_timeout(void);
static void	ftp_clear_timeout(void);
static void	ai_unmapped(struct addrinfo *);


/* Global status variable - ick */
int FtpTimedOut;

/* FTP happy status codes */
#define FTP_GENERALLY_HAPPY	200
#define FTP_ASCII_HAPPY		FTP_GENERALLY_HAPPY
#define FTP_BINARY_HAPPY	FTP_GENERALLY_HAPPY
#define FTP_PORT_HAPPY		FTP_GENERALLY_HAPPY
#define FTP_HAPPY_COMMENT	220
#define FTP_QUIT_HAPPY		221
#define FTP_TRANSFER_HAPPY	226
#define FTP_PASSIVE_HAPPY	227
#define FTP_LPASSIVE_HAPPY	228
#define FTP_EPASSIVE_HAPPY	229
#define FTP_CHDIR_HAPPY		250

/* FTP unhappy status codes */
#define FTP_TIMED_OUT		421

/*
 * XXX
 * gross!  evil!  bad!  We really need an access primitive for cookie in stdio itself.
 * it's too convenient a hook to bury and it's already exported through funopen as it is, so...
 * XXX
 */
#define fcookie(fp)	((fp)->_cookie)

/* Placeholder in case we want to do any pre-init stuff at some point */ 
int
networkInit()
{
    return SUCCESS;	/* XXX dummy function for now XXX */
}

/* Check a return code with some lenience for back-dated garbage that might be in the buffer */
static int
check_code(FTP_t ftp, int var, int preferred)
{
    ftp->error = 0;
    while (1) {
	if (var == preferred)
	    return 0;
	else if (var == FTP_TRANSFER_HAPPY)	/* last operation succeeded */
	    var = get_a_number(ftp, NULL);
	else if (var == FTP_HAPPY_COMMENT)	/* chit-chat */
	    var = get_a_number(ftp, NULL);
	else if (var == FTP_GENERALLY_HAPPY)	/* general success code */
	    var = get_a_number(ftp, NULL);
	else {
	    ftp->error = var;
	    return 1;
	}
    }
}
    
int
ftpAscii(FILE *fp)
{
    FTP_t ftp = fcookie(fp);
    int i;

    if (!ftp->is_binary)
	return SUCCESS;
    i = cmd(ftp, "TYPE A");
    if (i < 0 || check_code(ftp, i, FTP_ASCII_HAPPY))
	return i;
    ftp->is_binary = FALSE;
    return SUCCESS;
}

int
ftpBinary(FILE *fp)
{
    FTP_t ftp = fcookie(fp);
    int i;

    if (ftp->is_binary)
	return SUCCESS;
    i = cmd(ftp, "TYPE I");
    if (i < 0 || check_code(ftp, i, FTP_BINARY_HAPPY))
	return i;
    ftp->is_binary = TRUE;
    return SUCCESS;
}
void
ftpVerbose(FILE *fp, int status)
{
    FTP_t ftp = fcookie(fp);
    ftp->is_verbose = status;
}

int
ftpChdir(FILE *fp, char *dir)
{
    int i;
    FTP_t ftp = fcookie(fp);

    i = cmd(ftp, "CWD %s", dir);
    if (i < 0 || check_code(ftp, i, FTP_CHDIR_HAPPY))
	return i;
    return SUCCESS;
}

int
ftpErrno(FILE *fp)
{
    FTP_t ftp = fcookie(fp);
    return ftp->error;
}

const char *
ftpErrString(int error)
{
    int	k;

    if (error == -1)
	return("connection in wrong state");
    if (error < 100)
	/* XXX soon UNIX errnos will catch up with FTP protocol errnos */
	return strerror(error);
    for (k = 0; k < ftpErrListLength; k++)
      if (ftpErrList[k].num == error)
	return(ftpErrList[k].string);
    return("Unknown error");
}

off_t
ftpGetSize(FILE *fp, char *name)
{
    int i;
    char p[BUFSIZ], *cp, *ep;
    FTP_t ftp = fcookie(fp);
    off_t size;

    check_passive(fp);
    sprintf(p, "SIZE %s\r\n", name);
    if (ftp->is_verbose)
	fprintf(stderr, "Sending %s", p);
    if (writes(ftp->fd_ctrl, p))
	return (off_t)-1;
    i = get_a_number(ftp, &cp);
    if (check_code(ftp, i, 213))
	return (off_t)-1;

    errno = 0;				/* to check for ERANGE */
    size = (off_t)strtoq(cp, &ep, 10);
    if (*ep != '\0' || errno == ERANGE)
	return (off_t)-1;
    return size;
}

time_t
ftpGetModtime(FILE *fp, char *name)
{
    char p[BUFSIZ], *cp;
    struct tm t;
    time_t t0 = time (0);
    FTP_t ftp = fcookie(fp);
    int i;

    check_passive(fp);
    sprintf(p, "MDTM %s\r\n", name);
    if (ftp->is_verbose)
	fprintf(stderr, "Sending %s", p);
    if (writes(ftp->fd_ctrl, p))
	return (time_t)0;
    i = get_a_number(ftp, &cp);
    if (check_code(ftp, i, 213))
	return (time_t)0;
    while (*cp && !isdigit(*cp))
	cp++;
    if (!*cp)
	return (time_t)0;
    t0 = localtime (&t0)->tm_gmtoff;
    sscanf(cp, "%04d%02d%02d%02d%02d%02d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec);
    t.tm_mon--;
    t.tm_year -= 1900;
    t.tm_isdst=-1;
    t.tm_gmtoff = 0;
    t0 += mktime (&t);
    return t0;
}

FILE *
ftpGet(FILE *fp, char *file, off_t *seekto)
{
    FILE *fp2;
    FTP_t ftp = fcookie(fp);

    check_passive(fp);
    if (ftpBinary(fp) != SUCCESS)
	return NULL;

    if (ftp_file_op(ftp, "RETR", file, &fp2, "r", seekto) == SUCCESS)
	return fp2;
    return NULL;
}

/* Returns a standard FILE pointer type representing an open control connection */
FILE *
ftpLogin(char *host, char *user, char *passwd, int port, int verbose, int *retcode)
{
#ifdef INET6
    return ftpLoginAf(host, AF_UNSPEC, user, passwd, port, verbose, retcode);
#else
    return ftpLoginAf(host, AF_INET, user, passwd, port, verbose, retcode);
#endif
}

FILE *
ftpLoginAf(char *host, int af, char *user, char *passwd, int port, int verbose, int *retcode)
{
    FTP_t n;
    FILE *fp;

    if (retcode)
	*retcode = 0;
    if (networkInit() != SUCCESS)
	return NULL;

    n = ftp_new();
    fp = NULL;
    if (n && ftp_login_session(n, host, af, user, passwd, port, verbose) == SUCCESS) {
	fp = funopen(n, ftp_read_method, ftp_write_method, NULL, ftp_close_method);	/* BSD 4.4 function! */
    }
    if (retcode) {
	if (!n)
	    *retcode = (FtpTimedOut ? FTP_TIMED_OUT : -1);
	/* Poor attempt at mapping real errnos to FTP error codes */
	else switch(n->error) {
	    case EADDRNOTAVAIL:
		*retcode = FTP_TIMED_OUT;	/* Actually no such host, but we have no way of saying that. :-( */
		break;

            case ETIMEDOUT:
		*retcode = FTP_TIMED_OUT;
		break;

	    default:
		*retcode = n->error;
		break;
	}
    }
    return fp;
}

FILE *
ftpPut(FILE *fp, char *file)
{
    FILE *fp2;
    FTP_t ftp = fcookie(fp);

    check_passive(fp);
    if (ftp_file_op(ftp, "STOR", file, &fp2, "w", NULL) == SUCCESS)
	return fp2;
    return NULL;
}

int
ftpPassive(FILE *fp, int st)
{
    FTP_t ftp = fcookie(fp);

    ftp->is_passive = !!st;	/* normalize "st" to zero or one */
    return SUCCESS;
}

FILE *
ftpGetURL(char *url, char *user, char *passwd, int *retcode)
{
#ifdef INET6
    return ftpGetURLAf(url, AF_UNSPEC, user, passwd, retcode);
#else
    return ftpGetURLAf(url, AF_INET, user, passwd, retcode);
#endif
}

FILE *
ftpGetURLAf(char *url, int af, char *user, char *passwd, int *retcode)
{
    char host[255], name[255];
    int port;
    FILE *fp2;
    static FILE *fp = NULL;
    static char *prev_host;

    if (retcode)
	*retcode = 0;
    if (get_url_info(url, host, &port, name) == SUCCESS) {
	if (fp && prev_host) {
	    if (!strcmp(prev_host, host)) {
		/* Try to use cached connection */
		fp2 = ftpGet(fp, name, NULL);
		if (!fp2) {
		    /* Connection timed out or was no longer valid */
		    fclose(fp);
		    free(prev_host);
		    prev_host = NULL;
		}
		else
		    return fp2;
	    }
	    else {
		/* It's a different host now, flush old */
		fclose(fp);
		free(prev_host);
		prev_host = NULL;
	    }
	}
	fp = ftpLoginAf(host, af, user, passwd, port, 0, retcode);
	if (fp) {
	    fp2 = ftpGet(fp, name, NULL);
	    if (!fp2) {
		/* Connection timed out or was no longer valid */
		if (retcode)
		    *retcode = ftpErrno(fp);
		fclose(fp);
		fp = NULL;
	    }
	    else
		prev_host = strdup(host);
	    return fp2;
	}
    }
    return NULL;
}

FILE *
ftpPutURL(char *url, char *user, char *passwd, int *retcode)
{
#ifdef INET6
    return ftpPutURLAf(url, AF_UNSPEC, user, passwd, retcode);
#else
    return ftpPutURLAf(url, AF_INET, user, passwd, retcode);
#endif

}

FILE *
ftpPutURLAf(char *url, int af, char *user, char *passwd, int *retcode)
{
    char host[255], name[255];
    int port;
    static FILE *fp = NULL;
    FILE *fp2;

    if (retcode)
	*retcode = 0;
    if (fp) {	/* Close previous managed connection */
	fclose(fp);
	fp = NULL;
    }
    if (get_url_info(url, host, &port, name) == SUCCESS) {
	fp = ftpLoginAf(host, af, user, passwd, port, 0, retcode);
	if (fp) {
	    fp2 = ftpPut(fp, name);
	    if (!fp2) {
		if (retcode)
		    *retcode = ftpErrno(fp);
		fclose(fp);
		fp = NULL;
	    }
	    return fp2;
	}
    }
    return NULL;
}

/* Internal workhorse function for dissecting URLs.  Takes a URL as the first argument and returns the
   result of such disection in the host, user, passwd, port and name variables. */
static int
get_url_info(char *url_in, char *host_ret, int *port_ret, char *name_ret)
{
    char *name, *host, *cp, url[BUFSIZ];
    int port;

    name = host = NULL;
    /* XXX add http:// here or somewhere reasonable at some point XXX */
    if (strncmp("ftp://", url_in, 6) != 0)
	return FAILURE;
    /* We like to stomp a lot on the URL string in dissecting it, so copy it first */
    strncpy(url, url_in, BUFSIZ);
    host = url + 6;
    if ((cp = index(host, ':')) != NULL) {
	*(cp++) = '\0';
	port = strtol(cp, 0, 0);
    }
    else
	port = 0;	/* use default */
    if (port_ret)
	*port_ret = port;
    
    if ((name = index(cp ? cp : host, '/')) != NULL)
	*(name++) = '\0';
    if (host_ret)
	strcpy(host_ret, host);
    if (name && name_ret)
	strcpy(name_ret, name);
    return SUCCESS;
}

static FTP_t
ftp_new(void)
{
    FTP_t ftp;

    ftp = (FTP_t)malloc(sizeof *ftp);
    if (!ftp)
	return NULL;
    memset(ftp, 0, sizeof *ftp);
    ftp->fd_ctrl = -1;
    ftp->con_state = init;
    ftp->is_binary = FALSE;
    ftp->is_passive = FALSE;
    ftp->is_verbose = FALSE;
    ftp->error = 0;
    return ftp;
}

static int
ftp_read_method(void *vp, char *buf, int nbytes)
{
    int i, fd;
    FTP_t n = (FTP_t)vp;

    fd = n->fd_ctrl;
    i = (fd >= 0) ? read(fd, buf, nbytes) : EOF;
    return i;
}

static int
ftp_write_method(void *vp, const char *buf, int nbytes)
{
    int i, fd;
    FTP_t n = (FTP_t)vp;

    fd = n->fd_ctrl;
    i = (fd >= 0) ? write(fd, buf, nbytes) : EOF;
    return i;
}

static int
ftp_close_method(void *n)
{
    int i;

    i = ftp_close((FTP_t)n);
    free(n);
    return i;
}

/*
 * This function checks whether the FTP_PASSIVE_MODE environment
 * variable is set, and, if so, enforces the desired mode.
 */
static void
check_passive(FILE *fp)
{
    const char *cp = getenv("FTP_PASSIVE_MODE");

    if (cp != NULL)
    	ftpPassive(fp, strncasecmp(cp, "no", 2));
}

static void
ftp_timeout(int sig)
{
    FtpTimedOut = TRUE;
    /* Debug("ftp_pkg: ftp_timeout called - operation timed out"); */
}

static void
ftp_set_timeout(void)
{
    struct sigaction new;
    char *cp;
    int ival;

    FtpTimedOut = FALSE;
    sigemptyset(&new.sa_mask);
    new.sa_flags = 0;
    new.sa_handler = ftp_timeout;
    sigaction(SIGALRM, &new, NULL);
    cp = getenv("FTP_TIMEOUT");
    if (!cp || !(ival = atoi(cp)))
	ival = 120;
    alarm(ival);
}

static void
ftp_clear_timeout(void)
{
    struct sigaction new;

    alarm(0);
    sigemptyset(&new.sa_mask);
    new.sa_flags = 0;
    new.sa_handler = SIG_DFL;
    sigaction(SIGALRM, &new, NULL);
}

static int
writes(int fd, char *s)
{
    int n, i = strlen(s);

    ftp_set_timeout();
    n = write(fd, s, i);
    ftp_clear_timeout();
    if (FtpTimedOut || i != n)
	return TRUE;
    return FALSE;
}

static __inline char *
get_a_line(FTP_t ftp)
{
    static char buf[BUFSIZ];
    int i,j;

    /* Debug("ftp_pkg: trying to read a line from %d", ftp->fd_ctrl); */
    for(i = 0; i < BUFSIZ;) {
	ftp_set_timeout();
	j = read(ftp->fd_ctrl, buf + i, 1);
	ftp_clear_timeout();
	if (FtpTimedOut || j != 1)
	    return NULL;
	if (buf[i] == '\r' || buf[i] == '\n') {
	    if (!i)
		continue;
	    buf[i] = '\0';
	    if (ftp->is_verbose == TRUE)
		fprintf(stderr, "%s\n",buf+4);
	    return buf;
	}
	i++;
    }
    /* Debug("ftp_pkg: read string \"%s\" from %d", buf, ftp->fd_ctrl); */
    return buf;
}

static int
get_a_number(FTP_t ftp, char **q)
{
    char *p;
    int i = -1, j;

    while(1) {
	p = get_a_line(ftp);
	if (!p) {
	    ftp_close(ftp);
	    if (FtpTimedOut)
		return FTP_TIMED_OUT;
	    return FAILURE;
	}
	if (!(isdigit(p[0]) && isdigit(p[1]) && isdigit(p[2])))
	    continue;
	if (i == -1 && p[3] == '-') {
	    i = strtol(p, 0, 0);
	    continue;
	}
	if (p[3] != ' ' && p[3] != '\t')
	    continue;
	j = strtol(p, 0, 0);
	if (i == -1) {
	    if (q) *q = p+4;
	    /* Debug("ftp_pkg: read reply %d from server (%s)", j, p); */
	    return j;
	} else if (j == i) {
	    if (q) *q = p+4;
	    /* Debug("ftp_pkg: read reply %d from server (%s)", j, p); */
	    return j;
	}
    }
}

static int
ftp_close(FTP_t ftp)
{
    int i, rcode;

    rcode = FAILURE;
    if (ftp->con_state == isopen) {
	ftp->con_state = quit;
	/* If last operation timed out, don't try to quit - just close */
	if (ftp->error != FTP_TIMED_OUT)
	    i = cmd(ftp, "QUIT");
	else
	    i = FTP_QUIT_HAPPY;
	if (!check_code(ftp, i, FTP_QUIT_HAPPY))
	    rcode = SUCCESS;
	close(ftp->fd_ctrl);
	ftp->fd_ctrl = -1;
    }
    else if (ftp->con_state == quit)
	rcode = SUCCESS;
    return rcode;
}

static int
botch(char *func, char *botch_state)
{
    /* Debug("ftp_pkg: botch: %s(%s)", func, botch_state); */
    return FAILURE;
}

static int
cmd(FTP_t ftp, const char *fmt, ...)
{
    char p[BUFSIZ];
    int i;

    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(p, sizeof p, fmt, ap);
    va_end(ap);

    if (ftp->con_state == init)
	return botch("cmd", "open");

    strcat(p, "\r\n");
    if (ftp->is_verbose)
	fprintf(stderr, "Sending: %s", p);
    if (writes(ftp->fd_ctrl, p)) {
	if (FtpTimedOut)
	    return FTP_TIMED_OUT;
	return FAILURE;
    }
    while ((i = get_a_number(ftp, NULL)) == FTP_HAPPY_COMMENT);
    return i;
}

static int
ftp_login_session(FTP_t ftp, char *host, int af,
		  char *user, char *passwd, int port, int verbose)
{
    char pbuf[10];
    struct addrinfo	hints, *res, *res0;
    int			err;
    int 		s;
    int			i;

    if (networkInit() != SUCCESS)
	return FAILURE;

    if (ftp->con_state != init) {
	ftp_close(ftp);
	ftp->error = -1;
	return FAILURE;
    }

    if (!user)
	user = "ftp";

    if (!passwd)
	passwd = "setup@";

    if (!port)
	port = 21;

    snprintf(pbuf, sizeof(pbuf), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = af;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    err = getaddrinfo(host, pbuf, &hints, &res0);
    if (err) {
	ftp->error = 0;
	return FAILURE;
    }

    s = -1;
    for (res = res0; res; res = res->ai_next) {
	ai_unmapped(res);
	ftp->addrtype = res->ai_family;

	if ((s = socket(res->ai_family, res->ai_socktype,
			res->ai_protocol)) < 0)
	    continue;

	if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
	    (void)close(s);
	    s = -1;
	    continue;
	}

	break;
    }
    freeaddrinfo(res0);
    if (s < 0) {
	ftp->error = errno;
	return FAILURE;
    }

    ftp->fd_ctrl = s;
    ftp->con_state = isopen;
    ftp->is_verbose = verbose;

    i = cmd(ftp, "USER %s", user);
    if (i >= 300 && i < 400)
	i = cmd(ftp, "PASS %s", passwd);
    if (i >= 299 || i < 0) {
	ftp_close(ftp);
	if (i > 0)
	    ftp->error = i;
	return FAILURE;
    }
    return SUCCESS;
}

static int
ftp_file_op(FTP_t ftp, char *operation, char *file, FILE **fp, char *mode, off_t *seekto)
{
    socklen_t sinlen;
    int i,l,s;
    char *q;
    unsigned char addr[64];
    union sockaddr_cmn {
	struct sockaddr_in sin4;
	struct sockaddr_in6 sin6;
    } sin;
    char *cmdstr;

    if (!fp)
	return FAILURE;
    *fp = NULL;

    if (ftp->con_state != isopen)
	return botch("ftp_file_op", "open");

    if ((s = socket(ftp->addrtype, SOCK_STREAM, 0)) < 0) {
	ftp->error = errno;
	return FAILURE;
    }

    if (ftp->is_passive) {
	if (ftp->addrtype == AF_INET) {
            if (ftp->is_verbose)
		fprintf(stderr, "Sending PASV\n");
	    if (writes(ftp->fd_ctrl, "PASV\r\n")) {
		ftp_close(ftp);
		if (FtpTimedOut)
		    ftp->error = FTP_TIMED_OUT;
		return FTP_TIMED_OUT;
	    }
	    i = get_a_number(ftp, &q);
	    if (check_code(ftp, i, FTP_PASSIVE_HAPPY)) {
		ftp_close(ftp);
		return i;
	    }
	    cmdstr = "PASV";
	} else {
            if (ftp->is_verbose)
		fprintf(stderr, "Sending EPSV\n");
	    if (writes(ftp->fd_ctrl, "EPSV\r\n")) {
		ftp_close(ftp);
		if (FtpTimedOut)
		    ftp->error = FTP_TIMED_OUT;
		return FTP_TIMED_OUT;
	    }
	    i = get_a_number(ftp, &q);
	    if (check_code(ftp, i, FTP_EPASSIVE_HAPPY)) {
		if (ftp->is_verbose)
		    fprintf(stderr, "Sending LPSV\n");
		if (writes(ftp->fd_ctrl, "LPSV\r\n")) {
		    ftp_close(ftp);
		    if (FtpTimedOut)
			ftp->error = FTP_TIMED_OUT;
		    return FTP_TIMED_OUT;
		}
		i = get_a_number(ftp, &q);
		if (check_code(ftp, i, FTP_LPASSIVE_HAPPY)) {
		    ftp_close(ftp);
		    return i;
		}
		cmdstr = "LPSV";
	    } else
		cmdstr = "EPSV";
	}
	if (strcmp(cmdstr, "PASV") == 0 || strcmp(cmdstr, "LPSV") == 0) {
	    while (*q && !isdigit(*q))
		q++;
	    if (!*q) {
		ftp_close(ftp);
		return FAILURE;
	    }
	    q--;
	    l = (ftp->addrtype == AF_INET ? 6 : 21);
	    for (i = 0; i < l; i++) {
		q++;
		addr[i] = strtol(q, &q, 10);
	    }

	    sin.sin4.sin_family = ftp->addrtype;
	    if (ftp->addrtype == AF_INET6) {
		sin.sin6.sin6_len = sizeof(struct sockaddr_in6);
		bcopy(addr + 2, (char *)&sin.sin6.sin6_addr, 16);
		bcopy(addr + 19, (char *)&sin.sin6.sin6_port, 2);
	    } else {
		sin.sin4.sin_len = sizeof(struct sockaddr_in);
		bcopy(addr, (char *)&sin.sin4.sin_addr, 4);
		bcopy(addr + 4, (char *)&sin.sin4.sin_port, 2);
	    }
	} else if (strcmp(cmdstr, "EPSV") == 0) {
	    int port;
	    while (*q && *q != '(')		/* ) */
		q++;
	    if (!*q) {
		ftp_close(ftp);
		return FAILURE;
	    }
	    q++;
	    if (sscanf(q, "%c%c%c%d%c", &addr[0], &addr[1], &addr[2],
		    &port, &addr[3]) != 5
	     || addr[0] != addr[1] || addr[0] != addr[2] || addr[0] != addr[3]) {
		ftp_close(ftp);
		return FAILURE;
	    }
	    sinlen = sizeof(sin);
	    if (getpeername(ftp->fd_ctrl, (struct sockaddr *)&sin, &sinlen) < 0) {
		ftp_close(ftp);
		return FAILURE;
	    }
	    switch (sin.sin4.sin_family) {
	    case AF_INET:
		sin.sin4.sin_port = htons(port);
		break;
	    case AF_INET6:
		sin.sin6.sin6_port = htons(port);
		break;
	    default:
		ftp_close(ftp);
		return FAILURE;
	    }
	}

	if (connect(s, (struct sockaddr *)&sin, sin.sin4.sin_len) < 0) {
	    (void)close(s);
	    return FAILURE;
	}

	if (seekto && *seekto) {
	    i = cmd(ftp, "REST %d", *seekto);
	    if (i < 0 || FTP_TIMEOUT(i)) {
		close(s);
		ftp->error = i;
		*seekto = (off_t)0;
		return i;
	    }
	}
	i = cmd(ftp, "%s %s", operation, file);
	if (i < 0 || i > 299) {
	    close(s);
	    ftp->error = i;
	    return i;
	}
	*fp = fdopen(s, mode);
    }
    else {
	int fd,portrange;

#ifdef IPV6_PORTRANGE
	if (ftp->addrtype == AF_INET6) {
		portrange = IPV6_PORTRANGE_HIGH;
		if (setsockopt(s, IPPROTO_IPV6, IPV6_PORTRANGE, (char *)
			       &portrange, sizeof(portrange)) < 0) {
			close(s);   
			return FAILURE;
		}
	}
#endif
#ifdef IP_PORTRANGE
	if (ftp->addrtype == AF_INET) {
		portrange = IP_PORTRANGE_HIGH;
		if (setsockopt(s, IPPROTO_IP, IP_PORTRANGE, (char *)
			       &portrange, sizeof(portrange)) < 0) {
			close(s);   
			return FAILURE;
		}
	}
#endif

	sinlen = sizeof sin;
	getsockname(ftp->fd_ctrl, (struct sockaddr *)&sin, &sinlen);
	sin.sin4.sin_port = 0;
	i = ((struct sockaddr *)&sin)->sa_len;
	if (bind(s, (struct sockaddr *)&sin, i) < 0) {
	    close(s);	
	    return FAILURE;
	}
	sinlen = sizeof sin;
	getsockname(s, (struct sockaddr *)&sin, &sinlen);
	if (listen(s, 1) < 0) {
	    close(s);	
	    return FAILURE;
	}
	if (sin.sin4.sin_family == AF_INET) {
            u_long a;
	    a = ntohl(sin.sin4.sin_addr.s_addr);
	    i = cmd(ftp, "PORT %d,%d,%d,%d,%d,%d",
		    (a                   >> 24) & 0xff,
		    (a                   >> 16) & 0xff,
		    (a                   >>  8) & 0xff,
		    a                           & 0xff,
		    (ntohs(sin.sin4.sin_port) >>  8) & 0xff,
		    ntohs(sin.sin4.sin_port)         & 0xff);
	    if (check_code(ftp, i, FTP_PORT_HAPPY)) {
		close(s);
		return i;
	    }
	} else {
#define UC(b)	(((int)b)&0xff)
	    char *a;
	    char hname[INET6_ADDRSTRLEN];

	    sin.sin6.sin6_scope_id = 0;
	    if (getnameinfo((struct sockaddr *)&sin, sin.sin6.sin6_len,
			    hname, sizeof(hname),
			    NULL, 0, NI_NUMERICHOST) != 0) {
		goto try_lprt;
	    }
	    i = cmd(ftp, "EPRT |%d|%s|%d|", 2, hname,
		    htons(sin.sin6.sin6_port));
	    if (check_code(ftp, i, FTP_PORT_HAPPY)) {
try_lprt:
		a = (char *)&sin.sin6.sin6_addr;
		i = cmd(ftp,
"LPRT %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
			6, 16,
			UC(a[0]),UC(a[1]),UC(a[2]),UC(a[3]),
			UC(a[4]),UC(a[5]),UC(a[6]),UC(a[7]),
			UC(a[8]),UC(a[9]),UC(a[10]),UC(a[11]),
			UC(a[12]),UC(a[13]),UC(a[14]),UC(a[15]),
			2,
			(ntohs(sin.sin4.sin_port) >>  8) & 0xff,
			ntohs(sin.sin4.sin_port)         & 0xff);
		if (check_code(ftp, i, FTP_PORT_HAPPY)) {
		    close(s);
		    return i;
		}
	    }
	}
	if (seekto && *seekto) {
	    i = cmd(ftp, "REST %d", *seekto);
	    if (i < 0 || FTP_TIMEOUT(i)) {
		close(s);
		ftp->error = i;
		return i;
	    }
	    else if (i != 350)
		*seekto = (off_t)0;
	}
	i = cmd(ftp, "%s %s", operation, file);
	if (i < 0 || i > 299) {
	    close(s);
	    ftp->error = i;
	    return FAILURE;
	}
	fd = accept(s, 0, 0);
	if (fd < 0) {
	    close(s);
	    ftp->error = 401;
	    return FAILURE;
	}
	close(s);
	*fp = fdopen(fd, mode);
    }
    if (*fp)
	return SUCCESS;
    else
	return FAILURE;
}

static void
ai_unmapped(struct addrinfo *ai)
{
	struct sockaddr_in6 *sin6;
	struct sockaddr_in sin;

	if (ai->ai_family != AF_INET6)
		return;
	if (ai->ai_addrlen != sizeof(struct sockaddr_in6) ||
	    sizeof(sin) > ai->ai_addrlen)
		return;
	sin6 = (struct sockaddr_in6 *)ai->ai_addr;
	if (!IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
		return;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(struct sockaddr_in);
	memcpy(&sin.sin_addr, &sin6->sin6_addr.s6_addr[12],
	    sizeof(sin.sin_addr));
	sin.sin_port = sin6->sin6_port;

	ai->ai_family = AF_INET;
	memcpy(ai->ai_addr, &sin, sin.sin_len);
	ai->ai_addrlen = sin.sin_len;
}
