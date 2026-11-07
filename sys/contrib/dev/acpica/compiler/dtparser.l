%{
/******************************************************************************
 *
 * Module Name: dtparser.l - Flex input file for table compiler lexer
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "dtparser.y.h"

#define YY_NO_INPUT     /* No file input, we use strings only */

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("dtscanner")
%}

%option noyywrap
%option nounput

Number          [0-9a-fA-F]+
HexNumber       0[xX][0-9a-fA-F]+
DecimalNumber   0[dD][0-9]+
LabelRef        $[a-zA-Z][0-9a-zA-Z]*
WhiteSpace      [ \t\v\r]+
NewLine         [\n]

%%

\(              return (OP_EXP_PAREN_OPEN);
\)              return (OP_EXP_PAREN_CLOSE);
\~              return (OP_EXP_ONES_COMPLIMENT);
\!              return (OP_EXP_LOGICAL_NOT);
\*              return (OP_EXP_MULTIPLY);
\/              return (OP_EXP_DIVIDE);
\%              return (OP_EXP_MODULO);
\+              return (OP_EXP_ADD);
\-              return (OP_EXP_SUBTRACT);
">>"            return (OP_EXP_SHIFT_RIGHT);
"<<"            return (OP_EXP_SHIFT_LEFT);
\<              return (OP_EXP_LESS);
\>              return (OP_EXP_GREATER);
"<="            return (OP_EXP_LESS_EQUAL);
">="            return (OP_EXP_GREATER_EQUAL);
"=="            return (OP_EXP_EQUAL);
"!="            return (OP_EXP_NOT_EQUAL);
\&              return (OP_EXP_AND);
\^              return (OP_EXP_XOR);
\|              return (OP_EXP_OR);
"&&"            return (OP_EXP_LOGICAL_AND);
"||"            return (OP_EXP_LOGICAL_OR);
<<EOF>>         return (OP_EXP_EOF); /* null end-of-string */

{LabelRef}      return (OP_EXP_LABEL);
{Number}        return (OP_EXP_NUMBER);
{HexNumber}     return (OP_EXP_HEX_NUMBER);
{NewLine}       return (OP_EXP_NEW_LINE);
{WhiteSpace}    /* Ignore */

.               return (OP_EXP_EOF);

%%

/*
 * Local support functions
 */
static YY_BUFFER_STATE         LexBuffer;

/******************************************************************************
 *
 * FUNCTION:    DtInitLexer, DtTerminateLexer
 *
 * PARAMETERS:  String              - Input string to be parsed
 *
 * RETURN:      None
 *
 * DESCRIPTION: Initialization and termination routines for lexer. Lexer needs
 *              a buffer to handle strings instead of a file.
 *
 *****************************************************************************/

int
DtInitLexer (
    char                    *String)
{
    LexBuffer = yy_scan_string (String);
    return (LexBuffer == NULL);
}

void
DtTerminateLexer (
    void)
{
    yy_delete_buffer (LexBuffer);
}
