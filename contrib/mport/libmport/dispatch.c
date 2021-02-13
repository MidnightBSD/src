/*-
 * Copyright (c) 2015 Lucas Holt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include "mport.h"
#include "mport_dispatch.h"
#include "mport_private.h"

/**
 * package task queue
 */
dispatch_queue_t mportTaskSerial = NULL;

/**
 * libarchive operations queue
 */
dispatch_queue_t mportArchiveSerial = NULL;

/**
 * print callbacks queue
 */
dispatch_queue_t mportPrintSerial = NULL;

/**
 * sqlite operations queue
 */
dispatch_queue_t mportSQLSerial = NULL;

//static dispatch_once_t mportInitializeOnce;


void
mport_init_queues(void)
{

    mportArchiveSerial = dispatch_queue_create("org.midnightbsd.mport.archive", NULL);
    mportPrintSerial = dispatch_queue_create("org.midnightbsd.mport.print", NULL);
    mportSQLSerial = dispatch_queue_create("org.midnightbsd.mport.sql", NULL);
    mportTaskSerial= dispatch_queue_create("org.midnightbsd.mport.task", NULL);
}
