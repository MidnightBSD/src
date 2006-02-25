/* RIPEMD160DRIVER.C - test driver for RIPEMD160
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libmd/rmddriver.c,v 1.3 2001/09/30 21:56:22 dillon Exp $");

/* Copyright (C) 1990-2, RSA Data Security, Inc. Created 1990. All
   rights reserved.

   RSA Data Security, Inc. makes no representations concerning either
   the merchantability of this software or the suitability of this
   software for any particular purpose. It is provided "as is"
   without express or implied warranty of any kind.

   These notices must be retained in any copies of any part of this
   documentation and/or software.
 */

#include <sys/types.h>

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "ripemd.h"

/* Digests a string and prints the result.
 */
static void RIPEMD160String (string)
char *string;
{
  char buf[2*20+1];

  printf ("RIPEMD160 (\"%s\") = %s\n", 
	string, RIPEMD160_Data(string,strlen(string),buf));
}

/* Digests a reference suite of strings and prints the results.
 */
main()
{
  printf ("RIPEMD160 test suite:\n");

  RIPEMD160String ("");
  RIPEMD160String ("abc");
  RIPEMD160String ("message digest");
  RIPEMD160String ("abcdefghijklmnopqrstuvwxyz");
  RIPEMD160String
    ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
  RIPEMD160String
    ("1234567890123456789012345678901234567890\
1234567890123456789012345678901234567890");
  return 0;
}
