/* readfile.h */
/* $FreeBSD: release/10.0.0/libexec/bootpd/readfile.h 97416 2002-05-28 18:31:41Z alfred $ */

#include "bptypes.h"
#include "hash.h"

extern boolean hwlookcmp(hash_datum *, hash_datum *);
extern boolean iplookcmp(hash_datum *, hash_datum *);
extern boolean nmcmp(hash_datum *, hash_datum *);
extern void readtab(int);
extern void rdtab_init(void);
