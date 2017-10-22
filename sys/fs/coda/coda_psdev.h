/*-
 * 
 *             Coda: an Experimental Distributed File System
 *                              Release 3.1
 * 
 *           Copyright (c) 1998 Carnegie Mellon University
 *                          All Rights Reserved
 * 
 * Permission  to  use, copy, modify and distribute this software and its
 * documentation is hereby granted,  provided  that  both  the  copyright
 * notice  and  this  permission  notice  appear  in  all  copies  of the
 * software, derivative works or  modified  versions,  and  any  portions
 * thereof, and that both notices appear in supporting documentation, and
 * that credit is given to Carnegie Mellon University  in  all  documents
 * and publicity pertaining to direct or indirect use of this code or its
 * derivatives.
 * 
 * CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
 * SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
 * FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
 * DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
 * RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
 * ANY DERIVATIVE WORK.
 * 
 * Carnegie  Mellon  encourages  users  of  this  software  to return any
 * improvements or extensions that  they  make,  and  to  grant  Carnegie
 * Mellon the rights to redistribute these changes without encumbrance.
 * 
 * 	@(#) src/sys/coda/coda_psdev.c,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $
 * $FreeBSD: release/7.0.0/sys/fs/coda/coda_psdev.h 171414 2007-07-12 20:40:38Z rwatson $
 * 
 */

int vc_nb_open(struct cdev *dev, int flag, int mode, struct thread *p);
int vc_nb_close (struct cdev *dev, int flag, int mode, struct thread *p);
int vc_nb_read(struct cdev *dev, struct uio *uiop, int flag);
int vc_nb_write(struct cdev *dev, struct uio *uiop, int flag);
int vc_nb_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, struct thread *p);
int vc_nb_poll(struct cdev *dev, int events, struct thread *p);
