/*
 * Public domain
 * sys/time.h compatibility shim
 */

#ifndef LIBCOMPAT_SYS_TIME_H
#define LIBCOMPAT_SYS_TIME_H

#include_next <sys/time.h>

#include <stdint.h>

int adjfreq(const int64_t *freq, int64_t *oldfreq);

#ifdef __sun
static inline int sun_adjtime(struct timeval *delta, struct timeval *olddelta)
{
	struct timeval zero = {0};
	int rc;

	/*
	 * adjtime on Solaris appears to handle a NULL delta differently than
	 * other OSes. Fill in a dummy value as necessary.
	 */
	if (delta)
		rc = adjtime(delta, olddelta);
	else
		rc = adjtime(&zero, olddelta);

	/*
	 * Old delta on Solaris frequently gets stuck with 1 ms left.
	 * Round down to 0 in this case so we do not get flapping clock sync.
	 */
	if (rc == 0 && olddelta &&
	    olddelta->tv_sec == 0 && olddelta->tv_usec == 1)
		olddelta->tv_usec = 0;

	return rc;
}
#define adjtime(d, o) sun_adjtime(d, o)
#endif

#ifndef timespecsub
/* Operations on timespecs. */
#define timespecclear(tsp)      (tsp)->tv_sec = (tsp)->tv_nsec = 0
#define timespecisset(tsp)      ((tsp)->tv_sec || (tsp)->tv_nsec)
#define timespecisvalid(tsp)                        \
    ((tsp)->tv_nsec >= 0 && (tsp)->tv_nsec < 1000000000L)
#define timespeccmp(tsp, usp, cmp)                  \
    (((tsp)->tv_sec == (usp)->tv_sec) ?             \
        ((tsp)->tv_nsec cmp (usp)->tv_nsec) :           \
        ((tsp)->tv_sec cmp (usp)->tv_sec))
#define timespecadd(tsp, usp, vsp)                  \
    do {                                \
        (vsp)->tv_sec = (tsp)->tv_sec + (usp)->tv_sec;      \
        (vsp)->tv_nsec = (tsp)->tv_nsec + (usp)->tv_nsec;   \
        if ((vsp)->tv_nsec >= 1000000000L) {            \
            (vsp)->tv_sec++;                \
            (vsp)->tv_nsec -= 1000000000L;          \
        }                           \
    } while (0)
#define timespecsub(tsp, usp, vsp)                  \
    do {                                \
        (vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;      \
        (vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;   \
        if ((vsp)->tv_nsec < 0) {               \
            (vsp)->tv_sec--;                \
            (vsp)->tv_nsec += 1000000000L;          \
        }                           \
    } while (0)
#endif

#endif
