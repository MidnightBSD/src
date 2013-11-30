/* A more-standard <time.h>.

   Copyright (C) 2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Don't get in the way of glibc when it includes time.h merely to
   declare a few standard symbols, rather than to declare all the
   symbols.  */
#if defined __need_time_t || defined __need_clock_t || defined __need_timespec

# if @HAVE_INCLUDE_NEXT@
#  include_next <time.h>
# else
#  include @ABSOLUTE_TIME_H@
# endif

#else
/* Normal invocation convention.  */

# if ! defined _GL_TIME_H

/* The include_next requires a split double-inclusion guard.  */
#  if @HAVE_INCLUDE_NEXT@
#   include_next <time.h>
#  else
#   include @ABSOLUTE_TIME_H@
#  endif

#  if ! defined _GL_TIME_H
#   define _GL_TIME_H

#   ifdef __cplusplus
extern "C" {
#   endif

/* Some systems don't define struct timespec (e.g., AIX 4.1, Ultrix 4.3).
   Or they define it with the wrong member names or define it in <sys/time.h>
   (e.g., FreeBSD circa 1997).  */
#   if ! @TIME_H_DEFINES_STRUCT_TIMESPEC@
#    if @SYS_TIME_H_DEFINES_STRUCT_TIMESPEC@
#     include <sys/time.h>
#    else
#     undef timespec
#     define timespec rpl_timespec
struct timespec
{
  time_t tv_sec;
  long int tv_nsec;
};
#    endif
#   endif

/* Sleep for at least RQTP seconds unless interrupted,  If interrupted,
   return -1 and store the remaining time into RMTP.  See
   <http://www.opengroup.org/susv3xsh/nanosleep.html>.  */
#   if @REPLACE_NANOSLEEP@
#    define nanosleep rpl_nanosleep
int nanosleep (struct timespec const *__rqtp, struct timespec *__rmtp);
#   endif

/* Convert TIMER to RESULT, assuming local time and UTC respectively.  See
   <http://www.opengroup.org/susv3xsh/localtime_r.html> and
   <http://www.opengroup.org/susv3xsh/gmtime_r.html>.  */
#   if @REPLACE_LOCALTIME_R@
#    undef localtime_r
#    define localtime_r rpl_localtime_r
#    undef gmtime_r
#    define gmtime_r rpl_gmtime_r
struct tm *localtime_r (time_t const *restrict __timer,
			struct tm *restrict __result);
struct tm *gmtime_r (time_t const *restrict __timer,
		     struct tm *restrict __result);
#   endif

/* Parse BUF as a time stamp, assuming FORMAT specifies its layout, and store
   the resulting broken-down time into TM.  See
   <http://www.opengroup.org/susv3xsh/strptime.html>.  */
#   if @REPLACE_STRPTIME@
#    undef strptime
#    define strptime rpl_strptime
char *strptime (char const *restrict __buf, char const *restrict __format,
		struct tm *restrict __tm);
#   endif

/* Convert TM to a time_t value, assuming UTC.  */
#   if @REPLACE_TIMEGM@
#    undef timegm
#    define timegm rpl_timegm
time_t timegm (struct tm *__tm);
#   endif

/* Encourage applications to avoid unsafe functions that can overrun
   buffers when given outlandish struct tm values.  Portable
   applications should use strftime (or even sprintf) instead.  */
#   if GNULIB_PORTCHECK
#    undef asctime
#    define asctime eschew_asctime
#    undef asctime_r
#    define asctime_r eschew_asctime_r
#    undef ctime
#    define ctime eschew_ctime
#    undef ctime_r
#    define ctime_r eschew_ctime_r
#   endif

#   ifdef __cplusplus
}
#   endif

#  endif /* _GL_TIME_H */
# endif /* _GL_TIME_H */
#endif
