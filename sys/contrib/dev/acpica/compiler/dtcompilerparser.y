%{
/******************************************************************************
 *
 * Module Name: dtcompilerparser.y - Bison input file for table compiler parser
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#include <contrib/dev/acpica/compiler/aslcompiler.h>


#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dtcompilerparser")

void *                      AslLocalAllocate (unsigned int Size);

/* Bison/yacc configuration */

#undef alloca
#define alloca              AslLocalAllocate

int                         DtCompilerParserlex (void);
int                         DtCompilerParserparse (void);
void                        DtCompilerParsererror (char const *msg);
extern char                 *DtCompilerParsertext;
extern DT_FIELD             *AslGbl_CurrentField;

extern int                  DtLabelByteOffset;
extern UINT64               DtCompilerParserlineno; /* Current line number */

extern UINT32               DtTokenFirstLine;
extern UINT32               DtTokenFirstColumn;

/* Bison/yacc configuration */

#define yytname             DtCompilerParsername
#define YYDEBUG             1               /* Enable debug output */
#define YYERROR_VERBOSE     1               /* Verbose error messages */
#define YYFLAG              -32768

/* Define YYMALLOC/YYFREE to prevent redefinition errors  */

#define YYMALLOC            malloc
#define YYFREE              free

%}


%union {
    char                *s;
    DT_FIELD            *f;
    DT_TABLE_UNIT       *u;
}


%type  <f> Table
%token <u> DT_PARSEOP_DATA
%token <u> DT_PARSEOP_LABEL
%token <u> DT_PARSEOP_STRING_DATA
%token <u> DT_PARSEOP_LINE_CONTINUATION
%type  <u> Data
%type  <u> Datum
%type  <u> MultiLineData
%type  <u> MultiLineDataList


%%

Table
    :
    FieldList { }
    ;

FieldList
    : Field FieldList
    | Field
    ;

Field
    : DT_PARSEOP_LABEL ':' Data { DtCreateField ($1, $3, DtLabelByteOffset); }
    ;

Data
    : MultiLineDataList        { $$ = $1; }
    | Datum                    { $$ = $1; }
    | Datum MultiLineDataList  { $$ = $1; } /* combine the string with strcat */
    ;

MultiLineDataList
    : MultiLineDataList MultiLineData { $$ = DtCreateTableUnit (AcpiUtStrcat(AcpiUtStrcat($1->Value, " "), $2->Value), $1->Line, $1->Column); } /* combine the strings with strcat */
    | MultiLineData                   { $$ = $1; }
    ;

MultiLineData
    : DT_PARSEOP_LINE_CONTINUATION Datum { DbgPrint (ASL_PARSE_OUTPUT, "line continuation detected\n"); $$ = $2; }
    ;

Datum
    : DT_PARSEOP_DATA        {
                                 DbgPrint (ASL_PARSE_OUTPUT, "parser        data: [%s]\n", DtCompilerParserlval.s);
                                 $$ = DtCreateTableUnit (AcpiUtStrdup(DtCompilerParserlval.s), DtTokenFirstLine, DtTokenFirstColumn);
                             }
    | DT_PARSEOP_STRING_DATA {
                                 DbgPrint (ASL_PARSE_OUTPUT, "parser string data: [%s]\n", DtCompilerParserlval.s);
                                 $$ = DtCreateTableUnit (AcpiUtStrdup(DtCompilerParserlval.s), DtTokenFirstLine, DtTokenFirstColumn);
                             }
    ;


%%


/*
 * Local support functions, including parser entry point
 */
/******************************************************************************
 *
 * FUNCTION:    DtCompilerParsererror
 *
 * PARAMETERS:  Message             - Parser-generated error message
 *
 * RETURN:      None
 *
 * DESCRIPTION: Handler for parser errors
 *
 *****************************************************************************/

void
DtCompilerParsererror (
    char const              *Message)
{
    DtError (ASL_ERROR, ASL_MSG_SYNTAX,
        AslGbl_CurrentField, (char *) Message);
}

int
DtCompilerParserwrap(void)
{
  return (1);
}
