#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <gdbm.h>
#include <fcntl.h>

typedef struct {
	GDBM_FILE 	dbp ;
	SV *    filter_fetch_key ;
	SV *    filter_store_key ;
	SV *    filter_fetch_value ;
	SV *    filter_store_value ;
	int     filtering ;
	} GDBM_File_type;

typedef GDBM_File_type * GDBM_File ;
typedef datum datum_key ;
typedef datum datum_value ;
typedef datum datum_key_copy;

#define GDBM_BLOCKSIZE 0 /* gdbm defaults to stat blocksize */

typedef void (*FATALFUNC)();

#ifndef GDBM_FAST
static int
not_here(char *s)
{
    croak("GDBM_File::%s not implemented on this architecture", s);
    return -1;
}
#endif

/* GDBM allocates the datum with system malloc() and expects the user
 * to free() it.  So we either have to free() it immediately, or have
 * perl free() it when it deallocates the SV, depending on whether
 * perl uses malloc()/free() or not. */
static void
output_datum(pTHX_ SV *arg, char *str, int size)
{
	sv_setpvn(arg, str, size);
	free(str);
}

/* Versions of gdbm prior to 1.7x might not have the gdbm_sync,
   gdbm_exists, and gdbm_setopt functions.  Apparently Slackware
   (Linux) 2.1 contains gdbm-1.5 (which dates back to 1991).
*/
#ifndef GDBM_FAST
#define gdbm_exists(db,key) not_here("gdbm_exists")
#define gdbm_sync(db) (void) not_here("gdbm_sync")
#define gdbm_setopt(db,optflag,optval,optlen) not_here("gdbm_setopt")
#endif

#include "const-c.inc"

MODULE = GDBM_File	PACKAGE = GDBM_File	PREFIX = gdbm_

INCLUDE: const-xs.inc

GDBM_File
gdbm_TIEHASH(dbtype, name, read_write, mode, fatal_func = (FATALFUNC)croak)
	char *		dbtype
	char *		name
	int		read_write
	int		mode
	FATALFUNC	fatal_func
	CODE:
	{
	    GDBM_FILE  	dbp ;

	    RETVAL = NULL ;
	    if ((dbp =  gdbm_open(name, GDBM_BLOCKSIZE, read_write, mode, fatal_func))) {
	        RETVAL = (GDBM_File)safemalloc(sizeof(GDBM_File_type)) ;
    	        Zero(RETVAL, 1, GDBM_File_type) ;
		RETVAL->dbp = dbp ;
	    }
	    
	}
	OUTPUT:
	  RETVAL
	

#define gdbm_close(db)			gdbm_close(db->dbp)
void
gdbm_close(db)
	GDBM_File	db
	CLEANUP:

void
gdbm_DESTROY(db)
	GDBM_File	db
	CODE:
	gdbm_close(db);
	safefree(db);

#define gdbm_FETCH(db,key)			gdbm_fetch(db->dbp,key)
datum_value
gdbm_FETCH(db, key)
	GDBM_File	db
	datum_key_copy	key

#define gdbm_STORE(db,key,value,flags)		gdbm_store(db->dbp,key,value,flags)
int
gdbm_STORE(db, key, value, flags = GDBM_REPLACE)
	GDBM_File	db
	datum_key	key
	datum_value	value
	int		flags
    CLEANUP:
	if (RETVAL) {
	    if (RETVAL < 0 && errno == EPERM)
		croak("No write permission to gdbm file");
	    croak("gdbm store returned %d, errno %d, key \"%.*s\"",
			RETVAL,errno,key.dsize,key.dptr);
	}

#define gdbm_DELETE(db,key)			gdbm_delete(db->dbp,key)
int
gdbm_DELETE(db, key)
	GDBM_File	db
	datum_key	key

#define gdbm_FIRSTKEY(db)			gdbm_firstkey(db->dbp)
datum_key
gdbm_FIRSTKEY(db)
	GDBM_File	db

#define gdbm_NEXTKEY(db,key)			gdbm_nextkey(db->dbp,key)
datum_key
gdbm_NEXTKEY(db, key)
	GDBM_File	db
	datum_key	key 

#define gdbm_reorganize(db)			gdbm_reorganize(db->dbp)
int
gdbm_reorganize(db)
	GDBM_File	db


#define gdbm_sync(db)				gdbm_sync(db->dbp)
void
gdbm_sync(db)
	GDBM_File	db

#define gdbm_EXISTS(db,key)			gdbm_exists(db->dbp,key)
int
gdbm_EXISTS(db, key)
	GDBM_File	db
	datum_key	key

#define gdbm_setopt(db,optflag, optval, optlen)	gdbm_setopt(db->dbp,optflag, optval, optlen)
int
gdbm_setopt (db, optflag, optval, optlen)
	GDBM_File	db
	int		optflag
	int		&optval
	int		optlen


SV *
filter_fetch_key(db, code)
	GDBM_File	db
	SV *		code
	SV *		RETVAL = &PL_sv_undef ;
	CODE:
	    DBM_setFilter(db->filter_fetch_key, code) ;

SV *
filter_store_key(db, code)
	GDBM_File	db
	SV *		code
	SV *		RETVAL =  &PL_sv_undef ;
	CODE:
	    DBM_setFilter(db->filter_store_key, code) ;

SV *
filter_fetch_value(db, code)
	GDBM_File	db
	SV *		code
	SV *		RETVAL =  &PL_sv_undef ;
	CODE:
	    DBM_setFilter(db->filter_fetch_value, code) ;

SV *
filter_store_value(db, code)
	GDBM_File	db
	SV *		code
	SV *		RETVAL =  &PL_sv_undef ;
	CODE:
	    DBM_setFilter(db->filter_store_value, code) ;

