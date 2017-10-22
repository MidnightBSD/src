/****************************************************************************
 * Copyright (c) 1998-2005,2006 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/***************************************************************************
*                                                                          *
*  Author : Juergen Pfeifer                                                *
*                                                                          *
***************************************************************************/

#include "form.priv.h"

MODULE_ID("$Id: fty_alpha.c,v 1.21 2006/04/22 21:33:05 tom Exp $")

#define thisARG alphaARG

typedef struct
  {
    int width;
  }
thisARG;

/*---------------------------------------------------------------------------
|   Facility      :  libnform
|   Function      :  static void *Make_This_Type(va_list *ap)
|
|   Description   :  Allocate structure for alpha type argument.
|
|   Return Values :  Pointer to argument structure or NULL on error
+--------------------------------------------------------------------------*/
static void *
Make_This_Type(va_list *ap)
{
  thisARG *argp = (thisARG *) malloc(sizeof(thisARG));

  if (argp)
    argp->width = va_arg(*ap, int);

  return ((void *)argp);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform
|   Function      :  static void *Copy_This_Type(const void * argp)
|
|   Description   :  Copy structure for alpha type argument.
|
|   Return Values :  Pointer to argument structure or NULL on error.
+--------------------------------------------------------------------------*/
static void *
Copy_This_Type(const void *argp)
{
  const thisARG *ap = (const thisARG *)argp;
  thisARG *result = (thisARG *) malloc(sizeof(thisARG));

  if (result)
    *result = *ap;

  return ((void *)result);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform
|   Function      :  static void Free_This_Type(void *argp)
|
|   Description   :  Free structure for alpha type argument.
|
|   Return Values :  -
+--------------------------------------------------------------------------*/
static void
Free_This_Type(void *argp)
{
  if (argp)
    free(argp);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform
|   Function      :  static bool Check_This_Character(
|                                      int c,
|                                      const void *argp)
|
|   Description   :  Check a character for the alpha type.
|
|   Return Values :  TRUE  - character is valid
|                    FALSE - character is invalid
+--------------------------------------------------------------------------*/
static bool
Check_This_Character(int c, const void *argp GCC_UNUSED)
{
#if USE_WIDEC_SUPPORT
  if (iswalpha((wint_t) c))
    return TRUE;
#endif
  return (isalpha(UChar(c)) ? TRUE : FALSE);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform
|   Function      :  static bool Check_This_Field(
|                                      FIELD *field,
|                                      const void *argp)
|
|   Description   :  Validate buffer content to be a valid alpha value
|
|   Return Values :  TRUE  - field is valid
|                    FALSE - field is invalid
+--------------------------------------------------------------------------*/
static bool
Check_This_Field(FIELD *field, const void *argp)
{
  int width = ((const thisARG *)argp)->width;
  unsigned char *bp = (unsigned char *)field_buffer(field, 0);
  bool result = (width < 0);

  Check_CTYPE_Field(result, bp, width, Check_This_Character);
  return (result);
}

static FIELDTYPE typeTHIS =
{
  _HAS_ARGS | _RESIDENT,
  1,				/* this is mutable, so we can't be const */
  (FIELDTYPE *)0,
  (FIELDTYPE *)0,
  Make_This_Type,
  Copy_This_Type,
  Free_This_Type,
  Check_This_Field,
  Check_This_Character,
  NULL,
  NULL
};

NCURSES_EXPORT_VAR(FIELDTYPE*) TYPE_ALPHA = &typeTHIS;

/* fty_alpha.c ends here */
