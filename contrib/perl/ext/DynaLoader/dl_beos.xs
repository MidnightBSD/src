/*
 * dl_beos.xs, by Tom Spindler
 * based on dl_dlopen.xs, by Paul Marquess
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <be/kernel/image.h>
#include <OS.h>
#include <stdlib.h>
#include <limits.h>

#define dlerror() strerror(errno)

#include "dlutils.c"	/* SaveError() etc	*/

static void
dl_private_init(pTHX)
{
    (void)dl_generic_private_init(aTHX);
}

MODULE = DynaLoader	PACKAGE = DynaLoader

BOOT:
    (void)dl_private_init(aTHX);


void *
dl_load_file(filename, flags=0)
    char *	filename
    int		flags
    CODE:
{   image_id bogo;
    char *path;
    path = malloc(PATH_MAX);
    if (*filename != '/') {
      getcwd(path, PATH_MAX);
      strcat(path, "/");
      strcat(path, filename);
    } else {
      strcpy(path, filename);
    }

    DLDEBUG(1,PerlIO_printf(Perl_debug_log, "dl_load_file(%s,%x):\n", path, flags));
    bogo = load_add_on(path);
    DLDEBUG(2,PerlIO_printf(Perl_debug_log, " libref=%lx\n", (unsigned long) RETVAL));
    ST(0) = sv_newmortal() ;
    if (bogo < 0) {
	SaveError(aTHX_ "%s", strerror(bogo));
	PerlIO_printf(Perl_debug_log, "load_add_on(%s) : %d (%s)\n", path, bogo, strerror(bogo));
    } else {
	RETVAL = (void *) bogo;
	sv_setiv( ST(0), PTR2IV(RETVAL) );
    }
    free(path);
}

void *
dl_find_symbol(libhandle, symbolname)
    void *	libhandle
    char *	symbolname
    CODE:
    status_t retcode;
    void *adr = 0;
#ifdef DLSYM_NEEDS_UNDERSCORE
    symbolname = Perl_form_nocontext("_%s", symbolname);
#endif
    RETVAL = NULL;
    DLDEBUG(2, PerlIO_printf(Perl_debug_log,
			     "dl_find_symbol(handle=%lx, symbol=%s)\n",
			     (unsigned long) libhandle, symbolname));
    retcode = get_image_symbol((image_id) libhandle, symbolname,
                               B_SYMBOL_TYPE_TEXT, (void **) &adr);
    RETVAL = adr;
    DLDEBUG(2, PerlIO_printf(Perl_debug_log,
			     "  symbolref = %lx\n", (unsigned long) RETVAL));
    ST(0) = sv_newmortal() ;
    if (RETVAL == NULL) {
	SaveError(aTHX_ "%s", strerror(retcode)) ;
	PerlIO_printf(Perl_debug_log, "retcode = %p (%s)\n", retcode, strerror(retcode));
    } else
	sv_setiv( ST(0), PTR2IV(RETVAL));


void
dl_undef_symbols()
    PPCODE:



# These functions should not need changing on any platform:

void
dl_install_xsub(perl_name, symref, filename="$Package")
    char *		perl_name
    void *		symref 
    const char *	filename
    CODE:
    DLDEBUG(2,PerlIO_printf(Perl_debug_log, "dl_install_xsub(name=%s, symref=%lx)\n",
		perl_name, (unsigned long) symref));
    ST(0) = sv_2mortal(newRV((SV*)newXS_flags(perl_name,
					      (void(*)(pTHX_ CV *))symref,
					      filename, NULL,
					      XS_DYNAMIC_FILENAME)));


char *
dl_error()
    CODE:
    dMY_CXT;
    RETVAL = dl_last_error ;
    OUTPUT:
    RETVAL

#if defined(USE_ITHREADS)

void
CLONE(...)
    CODE:
    MY_CXT_CLONE;

    /* MY_CXT_CLONE just does a memcpy on the whole structure, so to avoid
     * using Perl variables that belong to another thread, we create our 
     * own for this thread.
     */
    MY_CXT.x_dl_last_error = newSVpvn("", 0);

#endif

# end.
