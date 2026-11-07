/******************************************************************************
 *
 * Module Name: acapps - common include for ACPI applications/tools
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#ifndef _ACCONVERT
#define _ACCONVERT

/* Definitions for comment state */

#define ASL_COMMENT_STANDARD    1
#define ASLCOMMENT_INLINE       2
#define ASL_COMMENT_OPEN_PAREN  3
#define ASL_COMMENT_CLOSE_PAREN 4
#define ASL_COMMENT_CLOSE_BRACE 5

/* Definitions for comment print function*/

#define AML_COMMENT_STANDARD    1
#define AMLCOMMENT_INLINE       2
#define AML_COMMENT_END_NODE    3
#define AML_NAMECOMMENT         4
#define AML_COMMENT_CLOSE_BRACE 5
#define AML_COMMENT_ENDBLK      6
#define AML_COMMENT_INCLUDE     7


#ifdef ACPI_ASL_COMPILER
/*
 * cvcompiler
 */
void
CvProcessComment (
    ASL_COMMENT_STATE       CurrentState,
    char                    *StringBuffer,
    int                     c1);

void
CvProcessCommentType2 (
    ASL_COMMENT_STATE       CurrentState,
    char                    *StringBuffer);

UINT32
CvCalculateCommentLengths(
   ACPI_PARSE_OBJECT        *Op);

void
CvProcessCommentState (
    char                    input);

char*
CvAppendInlineComment (
    char                    *InlineComment,
    char                    *ToAdd);

void
CvAddToCommentList (
    char*                   ToAdd);

void
CvPlaceComment (
    UINT8                   Type,
    char                    *CommentString);

UINT32
CvParseOpBlockType (
    ACPI_PARSE_OBJECT       *Op);

ACPI_COMMENT_NODE*
CvCommentNodeCalloc (
    void);

void
CgWriteAmlDefBlockComment (
    ACPI_PARSE_OBJECT       *Op);

void
CgWriteOneAmlComment (
    ACPI_PARSE_OBJECT       *Op,
    char*                   CommentToPrint,
    UINT8                   InputOption);

void
CgWriteAmlComment (
    ACPI_PARSE_OBJECT       *Op);


/*
 * cvparser
 */
void
CvInitFileTree (
    ACPI_TABLE_HEADER       *Table,
    FILE                    *RootFile);

void
CvClearOpComments (
    ACPI_PARSE_OBJECT       *Op);

ACPI_FILE_NODE*
CvFilenameExists (
    char                    *Filename,
    ACPI_FILE_NODE           *Head);

void
CvLabelFileNode (
    ACPI_PARSE_OBJECT       *Op);

void
CvCaptureListComments (
    ACPI_PARSE_STATE        *ParserState,
    ACPI_COMMENT_NODE       *ListHead,
    ACPI_COMMENT_NODE       *ListTail);

void
CvCaptureCommentsOnly (
    ACPI_PARSE_STATE        *ParserState);

void
CvCaptureComments (
    ACPI_WALK_STATE         *WalkState);

void
CvTransferComments (
    ACPI_PARSE_OBJECT       *Op);

/*
 * cvdisasm
 */
void
CvSwitchFiles (
    UINT32                  level,
    ACPI_PARSE_OBJECT       *op);

BOOLEAN
CvFileHasSwitched (
    ACPI_PARSE_OBJECT       *Op);


void
CvCloseParenWriteComment (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level);

void
CvCloseBraceWriteComment (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level);

void
CvPrintOneCommentList (
    ACPI_COMMENT_NODE       *CommentList,
    UINT32                  Level);

void
CvPrintOneCommentType (
    ACPI_PARSE_OBJECT       *Op,
    UINT8                   CommentType,
    char*                   EndStr,
    UINT32                  Level);


#endif

#endif /* _ACCONVERT */
