%{
/******************************************************************************
 *
 * Module Name: dtcompilerparser.l - Flex input file for table compiler lexer
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "dtcompilerparser.y.h"


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("dtcompilerscanner")

/* handle locations */

int DtCompilerParsercolumn = 1;
int DtLabelByteOffset = 0;
int DtCompilerParserByteOffset = 0;

UINT32 DtTokenFirstLine = 0;
UINT32 DtTokenFirstColumn = 0;

#define YY_USER_ACTION \
            DtTokenFirstLine = DtCompilerParserlineno; \
            DtTokenFirstColumn = DtCompilerParsercolumn; \
            DtCompilerParsercolumn += DtCompilerParserleng; \
            DtCompilerParserByteOffset += DtCompilerParserleng; \
            DbgPrint (ASL_PARSE_OUTPUT,\
                "user action occurred. DtCompilerParserlloc.first_line: %u\n",\
                DtTokenFirstLine);
%}

%option nounput noinput yylineno

    /* Indicates a state used for parsing multiline C comments */
%x ML_COMMENT
%x DATA_STATE

WhiteSpace      [ \t\v\r]+
NewLines        [\n]+

    /* Avoid ", \n, and [] as a part of label name. These are not valid characters of a label name */
LabelName       [^ ":\n\[\]]([^":\n\[\]]*[^" :\n\[\]])?

    /* Avoid ", \n, \\, and [] as a part of data. These are not valid characters of data */
Data            [^ \\":\n\[\]]([^":\n\[\]\\]*[^" :\n\[\]\\])?

Text            [^ ":\n][^":\n]*
Comment         \[[^\n\[\]]*\]
CommentField    {LabelName}{WhiteSpace}*:{WhiteSpace}{Comment}?$


%%

<DATA_STATE>{WhiteSpace}"\\\n" {
        DbgPrint(ASL_PARSE_OUTPUT,"Continuation matched\n");
        return (DT_PARSEOP_LINE_CONTINUATION);
    }

":" {
        DbgPrint(ASL_PARSE_OUTPUT, ": Matched\n");
        BEGIN (DATA_STATE);
        return (':');
    }

<INITIAL,DATA_STATE>{WhiteSpace} { DbgPrint(ASL_PARSE_OUTPUT,"Whitespace matched\n"); }

<INITIAL,DATA_STATE>{Comment}    { DbgPrint(ASL_PARSE_OUTPUT,"Comment matched\n"); }

"/*"                     { BEGIN (ML_COMMENT); }
<ML_COMMENT>"*/"         { BEGIN (INITIAL); }
<ML_COMMENT>"*/\n"       { BEGIN (INITIAL); }
<ML_COMMENT>([^*]|\n)+|. /* Ignore */
"//".*                   /* Ignore */


<DATA_STATE>{Data} {
      char *s;
      int size = strlen (DtCompilerParsertext);
      s=UtLocalCacheCalloc (size + 1);
      AcpiUtSafeStrncpy (s, DtCompilerParsertext, size + 1);
      DtCompilerParserlval.s = s;
      DbgPrint (ASL_PARSE_OUTPUT, "Data: %s\n", s);
      return (DT_PARSEOP_DATA);
}

{CommentField}  /* ignore */

{LabelName} {
    char *s;
    int size = strlen (DtCompilerParsertext);
    s=UtLocalCacheCalloc (size + 1);
    AcpiUtSafeStrncpy (s, DtCompilerParsertext, size + 1);
    DtCompilerParserlval.u = (DT_TABLE_UNIT *) UtLocalCacheCalloc (sizeof (DT_TABLE_UNIT));
    DtCompilerParserlval.u->Value = s;
    DtCompilerParserlval.u->Line = DtCompilerParserlineno;
    DtCompilerParserlval.u->Column = DtCompilerParsercolumn;
    DtLabelByteOffset = DtCompilerParserByteOffset;
    DbgPrint (ASL_PARSE_OUTPUT, "Label: %s\n", s);
    return (DT_PARSEOP_LABEL);
}


<DATA_STATE>\"{Text}?\" { // remove outer quotes from the string, they are unnecessary
    char *s;
    int size = strlen (DtCompilerParsertext);
    s=UtLocalCacheCalloc (size - 1);
    AcpiUtSafeStrncpy (s, DtCompilerParsertext + 1, size - 1);
    DtCompilerParserlval.s = s;
    DbgPrint (ASL_PARSE_OUTPUT, "String Data: %s\n", s);
    BEGIN (INITIAL);
    return (DT_PARSEOP_STRING_DATA);
}


<INITIAL,DATA_STATE>{NewLines} {
    DbgPrint(ASL_PARSE_OUTPUT,
        "Newline matched (data state). Current line number: %u\n",DtCompilerParserlineno);
    BEGIN (INITIAL); DtCompilerParsercolumn = 1;
}


%%


/*
 * Local support functions
 */

void
DtCompilerInitLexer (
    FILE                *inFile)
{
    yyin = inFile;
}

void
DtCompilerTerminateLexer (
    void)
{
    /*
     * Flex/Bison increments the lineno for the EOF so decrement by 1 to get
     * the correct number of lines.
     */
    AslGbl_CurrentLineNumber = DtCompilerParserlineno - 1;
    AslGbl_InputByteCount = DtCompilerParserByteOffset;
}
