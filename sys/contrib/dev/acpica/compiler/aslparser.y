%{
/******************************************************************************
 *
 * Module Name: aslparser.y - Master Bison/Yacc input file for iASL
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslparse")

/*
 * Global Notes:
 *
 * October 2005: The following list terms have been optimized (from the
 * original ASL grammar in the ACPI specification) to force the immediate
 * reduction of each list item so that the parse stack use doesn't increase on
 * each list element and possibly overflow on very large lists (>4000 items).
 * This dramatically reduces use of the parse stack overall.
 *
 *      ArgList, TermList, ByteList, DWordList, PackageList,
 *      ResourceMacroList, and FieldUnitList
 */

void *
AslLocalAllocate (
    unsigned int            Size);


/* Bison/yacc configuration */

#define static
#undef malloc
#define malloc              AslLocalAllocate
#undef alloca
#define alloca              AslLocalAllocate
#define yytname             AslCompilername

#define YYINITDEPTH         600             /* State stack depth */
#define YYDEBUG             1               /* Enable debug output */
#define YYERROR_VERBOSE     1               /* Verbose error messages */
#define YYFLAG              -32768

/* Define YYMALLOC/YYFREE to prevent redefinition errors  */

#define YYMALLOC            AslLocalAllocate
#define YYFREE              ACPI_FREE
%}

/*
 * Declare the type of values in the grammar
 */
%union {
    UINT64              i;
    char                *s;
    ACPI_PARSE_OBJECT   *n;
}

/*
 * These shift/reduce conflicts are expected. There should be zero
 * reduce/reduce conflicts.
 */
%expect 134

/*! [Begin] no source code translation */

/*
 * The M4 macro processor is used to bring in the parser items,
 * in order to keep this master file smaller, and to break up
 * the various parser items.
 */
m4_define(NoEcho)

/* Token types */

m4_include(asltokens.y)

/* Production types/names */

m4_include(asltypes.y)
%%

/* Production rules */

m4_include(aslrules.y)
m4_include(aslprimaries.y)
m4_include(aslcstyle.y)
m4_include(aslkeywords.y)
m4_include(aslresources.y)
m4_include(aslhelpers.y)
%%

/*! [End] no source code translation !*/

/* Local support functions in C */

m4_include(aslsupport.y)
