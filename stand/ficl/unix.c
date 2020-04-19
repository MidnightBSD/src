/* $FreeBSD: stable/11/stand/ficl/unix.c 167850 2007-03-23 22:26:01Z jkim $ */

#include <string.h>
#include <netinet/in.h>

#include "ficl.h"



unsigned long ficlNtohl(unsigned long number)
{
    return ntohl(number);
}




void ficlCompilePlatform(FICL_DICT *dp)
{
    return;
}


