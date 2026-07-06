/*
 * Public domain
 * nameser.h compatibility shim
 */

#include_next <arpa/nameser.h>

#ifndef LIBCOMPAT_ARPA_NAMESER_H
#define LIBCOMPAT_ARPA_NAMESER_H

#ifndef C_IN
#define C_IN 1
#endif

#ifndef T_NS
#define T_NS 2
#endif

#endif
