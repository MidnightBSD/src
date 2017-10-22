/****************************************************************************
 * Copyright (c) 1998-2004,2006 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

/*
**	lib_twait.c
**
**	The routine _nc_timed_wait().
**
**	(This file was originally written by Eric Raymond; however except for
**	comments, none of the original code remains - T.Dickey).
*/

#include <curses.priv.h>

#ifdef __BEOS__
#undef false
#undef true
#include <OS.h>
#endif

#if USE_FUNC_POLL
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
#elif HAVE_SELECT
# if HAVE_SYS_TIME_H && HAVE_SYS_TIME_SELECT
#  include <sys/time.h>
# endif
# if HAVE_SYS_SELECT_H
#  include <sys/select.h>
# endif
#endif

MODULE_ID("$Id: lib_twait.c,v 1.51 2006/05/27 21:57:43 tom Exp $")

static long
_nc_gettime(bool first)
{
    long res;

#if HAVE_GETTIMEOFDAY
# define PRECISE_GETTIME 1
    static struct timeval t0;
    struct timeval t1;
    gettimeofday(&t1, (struct timezone *) 0);
    if (first) {
	t0 = t1;
	res = 0;
    } else {
	/* .tv_sec and .tv_usec are unsigned, be careful when subtracting */
	if (t0.tv_usec > t1.tv_usec) {	/* Convert 1s in 1e6 microsecs */
	    t1.tv_usec += 1000000;
	    t1.tv_sec--;
	}
	res = (t1.tv_sec - t0.tv_sec) * 1000
	    + (t1.tv_usec - t0.tv_usec) / 1000;
    }
#else
# define PRECISE_GETTIME 0
    static time_t t0;
    time_t t1 = time((time_t *) 0);
    if (first) {
	t0 = t1;
    }
    res = (t1 - t0) * 1000;
#endif
    TR(TRACE_IEVENT, ("%s time: %ld msec", first ? "get" : "elapsed", res));
    return res;
}

#ifdef NCURSES_WGETCH_EVENTS
NCURSES_EXPORT(int)
_nc_eventlist_timeout(_nc_eventlist * evl)
{
    int event_delay = -1;
    int n;

    if (evl != 0) {

	for (n = 0; n < evl->count; ++n) {
	    _nc_event *ev = evl->events[n];

	    if (ev->type == _NC_EVENT_TIMEOUT_MSEC) {
		event_delay = ev->data.timeout_msec;
		if (event_delay < 0)
		    event_delay = INT_MAX;	/* FIXME Is this defined? */
	    }
	}
    }
    return event_delay;
}
#endif /* NCURSES_WGETCH_EVENTS */

/*
 * Wait a specified number of milliseconds, returning nonzero if the timer
 * didn't expire before there is activity on the specified file descriptors.
 * The file-descriptors are specified by the mode:
 *	0 - none (absolute time)
 *	1 - ncurses' normal input-descriptor
 *	2 - mouse descriptor, if any
 *	3 - either input or mouse.
 *
 * Experimental:  if NCURSES_WGETCH_EVENTS is defined, (mode & 4) determines
 * whether to pay attention to evl argument.  If set, the smallest of
 * millisecond and of timeout of evl is taken.
 *
 * We return a mask that corresponds to the mode (e.g., 2 for mouse activity).
 *
 * If the milliseconds given are -1, the wait blocks until activity on the
 * descriptors.
 */
NCURSES_EXPORT(int)
_nc_timed_wait(int mode,
	       int milliseconds,
	       int *timeleft
	       EVENTLIST_2nd(_nc_eventlist * evl))
{
    int fd;
    int count;
    int result;

#ifdef NCURSES_WGETCH_EVENTS
    int timeout_is_event = 0;
    int n;
#endif

#if USE_FUNC_POLL
#define MIN_FDS 2
    struct pollfd fd_list[MIN_FDS];
    struct pollfd *fds = fd_list;
#elif defined(__BEOS__)
#elif HAVE_SELECT
    static fd_set set;
#endif

    long starttime, returntime;

    TR(TRACE_IEVENT, ("start twait: %d milliseconds, mode: %d",
		      milliseconds, mode));

#ifdef NCURSES_WGETCH_EVENTS
    if (mode & 4) {
	int event_delay = _nc_eventlist_timeout(evl);

	if (event_delay >= 0
	    && (milliseconds >= event_delay || milliseconds < 0)) {
	    milliseconds = event_delay;
	    timeout_is_event = 1;
	}
    }
#endif

#if PRECISE_GETTIME
  retry:
#endif
    starttime = _nc_gettime(TRUE);

    count = 0;

#ifdef NCURSES_WGETCH_EVENTS
    if ((mode & 4) && evl)
	evl->result_flags = 0;
#endif

#if USE_FUNC_POLL
    memset(fd_list, 0, sizeof(fd_list));

#ifdef NCURSES_WGETCH_EVENTS
    if ((mode & 4) && evl)
	fds = typeMalloc(struct pollfd, MIN_FDS + evl->count);
#endif

    if (mode & 1) {
	fds[count].fd = SP->_ifd;
	fds[count].events = POLLIN;
	count++;
    }
    if ((mode & 2)
	&& (fd = SP->_mouse_fd) >= 0) {
	fds[count].fd = fd;
	fds[count].events = POLLIN;
	count++;
    }
#ifdef NCURSES_WGETCH_EVENTS
    if ((mode & 4) && evl) {
	for (n = 0; n < evl->count; ++n) {
	    _nc_event *ev = evl->events[n];

	    if (ev->type == _NC_EVENT_FILE
		&& (ev->data.fev.flags & _NC_EVENT_FILE_READABLE)) {
		fds[count].fd = ev->data.fev.fd;
		fds[count].events = POLLIN;
		count++;
	    }
	}
    }
#endif

    result = poll(fds, (unsigned) count, milliseconds);

#ifdef NCURSES_WGETCH_EVENTS
    if ((mode & 4) && evl) {
	int c;

	if (!result)
	    count = 0;

	for (n = 0; n < evl->count; ++n) {
	    _nc_event *ev = evl->events[n];

	    if (ev->type == _NC_EVENT_FILE
		&& (ev->data.fev.flags & _NC_EVENT_FILE_READABLE)) {
		ev->data.fev.result = 0;
		for (c = 0; c < count; c++)
		    if (fds[c].fd == ev->data.fev.fd
			&& fds[c].revents & POLLIN) {
			ev->data.fev.result |= _NC_EVENT_FILE_READABLE;
			evl->result_flags |= _NC_EVENT_FILE_READABLE;
		    }
	    } else if (ev->type == _NC_EVENT_TIMEOUT_MSEC
		       && !result && timeout_is_event) {
		evl->result_flags |= _NC_EVENT_TIMEOUT_MSEC;
	    }
	}
    }

    if (fds != fd_list)
	free((char *) fds);

#endif

#elif defined(__BEOS__)
    /*
     * BeOS's select() is declared in socket.h, so the configure script does
     * not see it.  That's just as well, since that function works only for
     * sockets.  This (using snooze and ioctl) was distilled from Be's patch
     * for ncurses which uses a separate thread to simulate select().
     *
     * FIXME: the return values from the ioctl aren't very clear if we get
     * interrupted.
     *
     * FIXME: this assumes mode&1 if milliseconds < 0 (see lib_getch.c).
     */
    result = 0;
    if (mode & 1) {
	int step = (milliseconds < 0) ? 0 : 5000;
	bigtime_t d;
	bigtime_t useconds = milliseconds * 1000;
	int n, howmany;

	if (useconds <= 0)	/* we're here to go _through_ the loop */
	    useconds = 1;

	for (d = 0; d < useconds; d += step) {
	    n = 0;
	    howmany = ioctl(0, 'ichr', &n);
	    if (howmany >= 0 && n > 0) {
		result = 1;
		break;
	    }
	    if (useconds > 1 && step > 0) {
		snooze(step);
		milliseconds -= (step / 1000);
		if (milliseconds <= 0) {
		    milliseconds = 0;
		    break;
		}
	    }
	}
    } else if (milliseconds > 0) {
	snooze(milliseconds * 1000);
	milliseconds = 0;
    }
#elif HAVE_SELECT
    /*
     * select() modifies the fd_set arguments; do this in the
     * loop.
     */
    FD_ZERO(&set);

    if (mode & 1) {
	FD_SET(SP->_ifd, &set);
	count = SP->_ifd + 1;
    }
    if ((mode & 2)
	&& (fd = SP->_mouse_fd) >= 0) {
	FD_SET(fd, &set);
	count = max(fd, count) + 1;
    }
#ifdef NCURSES_WGETCH_EVENTS
    if ((mode & 4) && evl) {
	for (n = 0; n < evl->count; ++n) {
	    _nc_event *ev = evl->events[n];

	    if (ev->type == _NC_EVENT_FILE
		&& (ev->data.fev.flags & _NC_EVENT_FILE_READABLE)) {
		FD_SET(ev->data.fev.fd, &set);
		count = max(ev->data.fev.fd + 1, count);
	    }
	}
    }
#endif

    if (milliseconds >= 0) {
	struct timeval ntimeout;
	ntimeout.tv_sec = milliseconds / 1000;
	ntimeout.tv_usec = (milliseconds % 1000) * 1000;
	result = select(count, &set, NULL, NULL, &ntimeout);
    } else {
	result = select(count, &set, NULL, NULL, NULL);
    }

#ifdef NCURSES_WGETCH_EVENTS
    if ((mode & 4) && evl) {
	evl->result_flags = 0;
	for (n = 0; n < evl->count; ++n) {
	    _nc_event *ev = evl->events[n];

	    if (ev->type == _NC_EVENT_FILE
		&& (ev->data.fev.flags & _NC_EVENT_FILE_READABLE)) {
		ev->data.fev.result = 0;
		if (FD_ISSET(ev->data.fev.fd, &set)) {
		    ev->data.fev.result |= _NC_EVENT_FILE_READABLE;
		    evl->result_flags |= _NC_EVENT_FILE_READABLE;
		}
	    } else if (ev->type == _NC_EVENT_TIMEOUT_MSEC
		       && !result && timeout_is_event)
		evl->result_flags |= _NC_EVENT_TIMEOUT_MSEC;
	}
    }
#endif

#endif /* USE_FUNC_POLL, etc */

    returntime = _nc_gettime(FALSE);

    if (milliseconds >= 0)
	milliseconds -= (returntime - starttime);

#ifdef NCURSES_WGETCH_EVENTS
    if (evl) {
	evl->result_flags = 0;
	for (n = 0; n < evl->count; ++n) {
	    _nc_event *ev = evl->events[n];

	    if (ev->type == _NC_EVENT_TIMEOUT_MSEC) {
		long diff = (returntime - starttime);
		if (ev->data.timeout_msec <= diff)
		    ev->data.timeout_msec = 0;
		else
		    ev->data.timeout_msec -= diff;
	    }

	}
    }
#endif

#if PRECISE_GETTIME && HAVE_NANOSLEEP
    /*
     * If the timeout hasn't expired, and we've gotten no data,
     * this is probably a system where 'select()' needs to be left
     * alone so that it can complete.  Make this process sleep,
     * then come back for more.
     */
    if (result == 0 && milliseconds > 100) {
	napms(100);		/* FIXME: this won't be right if I recur! */
	milliseconds -= 100;
	goto retry;
    }
#endif

    /* return approximate time left in milliseconds */
    if (timeleft)
	*timeleft = milliseconds;

    TR(TRACE_IEVENT, ("end twait: returned %d (%d), remaining time %d msec",
		      result, errno, milliseconds));

    /*
     * Both 'poll()' and 'select()' return the number of file descriptors
     * that are active.  Translate this back to the mask that denotes which
     * file-descriptors, so that we don't need all of this system-specific
     * code everywhere.
     */
    if (result != 0) {
	if (result > 0) {
	    result = 0;
#if USE_FUNC_POLL
	    for (count = 0; count < MIN_FDS; count++) {
		if ((mode & (1 << count))
		    && (fds[count].revents & POLLIN)) {
		    result |= (1 << count);
		}
	    }
#elif defined(__BEOS__)
	    result = 1;		/* redundant, but simple */
#elif HAVE_SELECT
	    if ((mode & 2)
		&& (fd = SP->_mouse_fd) >= 0
		&& FD_ISSET(fd, &set))
		result |= 2;
	    if ((mode & 1)
		&& FD_ISSET(SP->_ifd, &set))
		result |= 1;
#endif
	} else
	    result = 0;
    }
#ifdef NCURSES_WGETCH_EVENTS
    if ((mode & 4) && evl && evl->result_flags)
	result |= 4;
#endif

    return (result);
}
