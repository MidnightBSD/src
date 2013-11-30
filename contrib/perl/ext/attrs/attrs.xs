#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

static cv_flags_t
get_flag(const char *attr)
{
    if (strnEQ(attr, "method", 6))
	return CVf_METHOD;
    else if (strnEQ(attr, "locked", 6))
	return CVf_LOCKED;
    else
	return 0;
}

MODULE = attrs		PACKAGE = attrs

void
import(...)
    ALIAS:
	unimport = 1
    PREINIT:
	int i;
    PPCODE:
       if (items < 1)
           Perl_croak(aTHX_ "Usage: %s(Class, ...)", GvNAME(CvGV(cv)));
	if (!PL_compcv || !(cv = CvOUTSIDE(PL_compcv)))
	    croak("can't set attributes outside a subroutine scope");
	if (ckWARN(WARN_DEPRECATED))
	    Perl_warner(aTHX_ packWARN(WARN_DEPRECATED),
			"pragma \"attrs\" is deprecated, "
			"use \"sub NAME : ATTRS\" instead");
	for (i = 1; i < items; i++) {
	    const char * const attr = SvPV_nolen(ST(i));
	    const cv_flags_t flag = get_flag(attr);
	    if (!flag)
		croak("invalid attribute name %s", attr);
	    if (ix)
    		CvFLAGS(cv) &= ~flag;
	    else
		CvFLAGS(cv) |= flag;
	}

void
get(sub)
SV *	sub
    PPCODE:
	if (SvROK(sub)) {
	    sub = SvRV(sub);
	    if (SvTYPE(sub) != SVt_PVCV)
		sub = Nullsv;
	}
	else {
	    const char * const name = SvPV_nolen(sub);
	    sub = (SV*)perl_get_cv(name, FALSE);
	}
	if (!sub)
	    croak("invalid subroutine reference or name");
	if (CvFLAGS(sub) & CVf_METHOD)
	    XPUSHs(sv_2mortal(newSVpvn("method", 6)));
	if (CvFLAGS(sub) & CVf_LOCKED)
	    XPUSHs(sv_2mortal(newSVpvn("locked", 6)));

