/******************************************************************************
 *
 * Module Name: cvcompiler - ASL-/ASL+ converter functions
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acdebug.h>
#include <contrib/dev/acpica/include/acconvert.h>


/* Local prototypes */

static void
CvPrintInclude(
    ACPI_FILE_NODE          *FNode,
    UINT32                  Level);

static BOOLEAN
CvListIsSingleton (
    ACPI_COMMENT_NODE       *CommentList);


/*******************************************************************************
 *
 * FUNCTION:    CvPrintOneCommentList
 *
 * PARAMETERS:  CommentList
 *              Level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Prints all comments within the given list.
 *              This is referred as ASL_CV_PRINT_ONE_COMMENT_LIST.
 *
 ******************************************************************************/

void
CvPrintOneCommentList (
    ACPI_COMMENT_NODE       *CommentList,
    UINT32                  Level)
{
    ACPI_COMMENT_NODE       *Current = CommentList;
    ACPI_COMMENT_NODE       *Previous;


    while (Current)
    {
        Previous = Current;
        if (Current->Comment)
        {
            AcpiDmIndent(Level);
            AcpiOsPrintf("%s\n", Current->Comment);
            Current->Comment = NULL;
        }

        Current = Current->Next;
        AcpiOsReleaseObject(AcpiGbl_RegCommentCache, Previous);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CvListIsSingleton
 *
 * PARAMETERS:  CommentList     - check to see if this is a single item list.
 *
 * RETURN:      BOOLEAN
 *
 * DESCRIPTION: Returns TRUE if CommentList only contains 1 node.
 *
 ******************************************************************************/

static BOOLEAN
CvListIsSingleton (
    ACPI_COMMENT_NODE       *CommentList)

{

    if (!CommentList)
    {
        return (FALSE);
    }
    else if (CommentList->Next)
    {
        return (FALSE);
    }

    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    CvPrintOneCommentType
 *
 * PARAMETERS:  Op
 *              CommentType
 *              EndStr - String to print after printing the comment
 *              Level  - indentation level for comment lists.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Prints all comments of CommentType within the given Op and
 *              clears the printed comment from the Op.
 *              This is referred as ASL_CV_PRINT_ONE_COMMENT.
 *
 ******************************************************************************/

void
CvPrintOneCommentType (
    ACPI_PARSE_OBJECT       *Op,
    UINT8                   CommentType,
    char*                   EndStr,
    UINT32                  Level)
{
    BOOLEAN                 CommentExists = FALSE;
    char                    **CommentToPrint = NULL;


    switch (CommentType)
    {
    case AML_COMMENT_STANDARD:

        if (CvListIsSingleton (Op->Common.CommentList))
        {
            CvPrintOneCommentList (Op->Common.CommentList, Level);
            AcpiOsPrintf ("\n");
        }
        else
        {
            CvPrintOneCommentList (Op->Common.CommentList, Level);
        }

        Op->Common.CommentList = NULL;
        return;

    case AML_COMMENT_ENDBLK:

        if (Op->Common.EndBlkComment)
        {
            CvPrintOneCommentList (Op->Common.EndBlkComment, Level);
            Op->Common.EndBlkComment = NULL;
            AcpiDmIndent(Level);
        }
        return;

    case AMLCOMMENT_INLINE:

        CommentToPrint = &Op->Common.InlineComment;
        break;

    case AML_COMMENT_END_NODE:

        CommentToPrint = &Op->Common.EndNodeComment;
        break;

    case AML_NAMECOMMENT:

        CommentToPrint = &Op->Common.NameComment;
        break;

    case AML_COMMENT_CLOSE_BRACE:

        CommentToPrint = &Op->Common.CloseBraceComment;
        break;

    default:
        return;
    }

    if (*CommentToPrint)
    {
        CommentExists = TRUE;
        AcpiOsPrintf ("%s", *CommentToPrint);
        *CommentToPrint = NULL;
    }

    if (CommentExists && EndStr)
    {
        AcpiOsPrintf ("%s", EndStr);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CvCloseBraceWriteComment
 *
 * PARAMETERS:  Op
 *              Level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print a close brace } and any open brace comments associated
 *              with this parse object.
 *              This is referred as ASL_CV_CLOSE_BRACE.
 *
 ******************************************************************************/

void
CvCloseBraceWriteComment(
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level)
{

    if (!AcpiGbl_CaptureComments)
    {
        AcpiOsPrintf ("}");
        return;
    }

    CvPrintOneCommentType (Op, AML_COMMENT_ENDBLK, NULL, Level);
    AcpiOsPrintf ("}");
    CvPrintOneCommentType (Op, AML_COMMENT_CLOSE_BRACE, NULL, Level);
}


/*******************************************************************************
 *
 * FUNCTION:    CvCloseParenWriteComment
 *
 * PARAMETERS:  Op
 *              Level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print a closing paren ) and any end node comments associated
 *              with this parse object.
 *              This is referred as ASL_CV_CLOSE_PAREN.
 *
 ******************************************************************************/

void
CvCloseParenWriteComment(
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level)
{

    if (!AcpiGbl_CaptureComments)
    {
        AcpiOsPrintf (")");
        return;
    }

    /*
     * If this op has a BLOCK_BRACE, then output the comment when the
     * disassembler calls CvCloseBraceWriteComment
     */
    if (AcpiDmBlockType (Op) == BLOCK_PAREN)
    {
        CvPrintOneCommentType (Op, AML_COMMENT_ENDBLK, NULL, Level);
    }

    AcpiOsPrintf (")");

    if (Op->Common.EndNodeComment)
    {
        CvPrintOneCommentType (Op, AML_COMMENT_END_NODE, NULL, Level);
    }
    else if ((Op->Common.Parent->Common.AmlOpcode == AML_IF_OP) &&
         Op->Common.Parent->Common.EndNodeComment)
    {
        CvPrintOneCommentType (Op->Common.Parent,
            AML_COMMENT_END_NODE, NULL, Level);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CvFileHasSwitched
 *
 * PARAMETERS:  Op
 *
 * RETURN:      BOOLEAN
 *
 * DESCRIPTION: Determine whether if a file has switched.
 *              TRUE - file has switched.
 *              FALSE - file has not switched.
 *              This is referred as ASL_CV_FILE_HAS_SWITCHED.
 *
 ******************************************************************************/

BOOLEAN
CvFileHasSwitched(
    ACPI_PARSE_OBJECT       *Op)
{

    if (Op->Common.CvFilename   &&
        AcpiGbl_CurrentFilename &&
        AcpiUtStricmp(Op->Common.CvFilename, AcpiGbl_CurrentFilename))
    {
        return (TRUE);
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    CvPrintInclude
 *
 * PARAMETERS:  FNode - Write an Include statement for the file that is pointed
 *                      by FNode->File.
 *              Level - indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Write the ASL Include statement for FNode->File in the file
 *              indicated by FNode->Parent->File. Note this function emits
 *              actual ASL code rather than comments. This switches the output
 *              file to FNode->Parent->File.
 *
 ******************************************************************************/

static void
CvPrintInclude(
    ACPI_FILE_NODE          *FNode,
    UINT32                  Level)
{

    if (!FNode || FNode->IncludeWritten)
    {
        return;
    }

    CvDbgPrint ("Writing include for %s within %s\n",
        FNode->Filename, FNode->Parent->Filename);
    AcpiOsRedirectOutput (FNode->Parent->File);
    CvPrintOneCommentList (FNode->IncludeComment, Level);

    AcpiDmIndent (Level);
    AcpiOsPrintf ("Include (\"%s\")\n", FNode->Filename);
    CvDbgPrint ("emitted the following: Include (\"%s\")\n",
        FNode->Filename);
    FNode->IncludeWritten = TRUE;
}


/*******************************************************************************
 *
 * FUNCTION:    CvSwitchFiles
 *
 * PARAMETERS:  Level                   - indentation level
 *              Op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Switch the outputfile and write ASL Include statement. Note,
 *              this function emits actual ASL code rather than comments.
 *              This is referred as ASL_CV_SWITCH_FILES.
 *
 ******************************************************************************/

void
CvSwitchFiles(
    UINT32                  Level,
    ACPI_PARSE_OBJECT       *Op)
{
    char                    *Filename = Op->Common.CvFilename;
    ACPI_FILE_NODE          *FNode;
    ACPI_FILE_NODE          *Current;


    CvDbgPrint ("Switching from %s to %s\n", AcpiGbl_CurrentFilename,
        Filename);
    FNode = CvFilenameExists (Filename, AcpiGbl_FileTreeRoot);
    if (!FNode)
    {
        /*
         * At this point, each Filename should exist in AcpiGbl_FileTreeRoot
         * if it does not exist, then abort.
         */
        FlDeleteFile (ASL_FILE_AML_OUTPUT);
        sprintf (AslGbl_MsgBuffer, "\"Cannot find %s\" - %s",
            Filename, strerror (errno));
        AslCommonError (ASL_ERROR, ASL_MSG_OPEN, 0, 0, 0, 0,
            NULL, AslGbl_MsgBuffer);
        AslAbort ();
    }

    Current = FNode;

    /*
     * If the previous file is a descendent of the current file,
     * make sure that Include statements from the current file
     * to the previous have been emitted.
     */
    while (Current &&
           Current->Parent &&
           AcpiUtStricmp (Current->Filename, AcpiGbl_CurrentFilename))
    {
        CvPrintInclude (Current, Level);
        Current = Current->Parent;
    }

    if (FNode)
    {
        /* Redirect output to Op->Common.CvFilename */

        AcpiOsRedirectOutput (FNode->File);
        AcpiGbl_CurrentFilename = FNode->Filename;
    }
}
