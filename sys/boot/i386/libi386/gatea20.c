/*
 * $NetBSD: gatea20.c,v 1.2 1997/10/29 00:32:49 fvdl Exp $
 */

/* extracted from freebsd:sys/i386/boot/biosboot/io.c */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/boot/i386/libi386/gatea20.c,v 1.4 2003/08/25 23:28:31 obrien Exp $");

#include <stand.h>
#include <machine/cpufunc.h>

#include <bootstrap.h>

#include "libi386.h"

#define K_RDWR 		0x60		/* keyboard data & cmds (read/write) */
#define K_STATUS 	0x64		/* keyboard status */
#define K_CMD	 	0x64		/* keybd ctlr command (write-only) */

#define K_OBUF_FUL 	0x01		/* output buffer full */
#define K_IBUF_FUL 	0x02		/* input buffer full */

#define KC_CMD_WIN	0xd0		/* read  output port */
#define KC_CMD_WOUT	0xd1		/* write output port */
#define KB_A20		0x9f		/* enable A20,
					   reset (!),
					   enable output buffer full interrupt
					   enable data line
					   disable clock line */

/*
 * Gate A20 for high memory
 */
static unsigned char	x_20 = KB_A20;
void gateA20()
{
	__asm("pushfl ; cli");
#ifdef	IBM_L40
	outb(0x92, 0x2);
#else	/* !IBM_L40 */
	while (inb(K_STATUS) & K_IBUF_FUL);
	while (inb(K_STATUS) & K_OBUF_FUL)
		(void)inb(K_RDWR);

	outb(K_CMD, KC_CMD_WOUT);
	delay(100);
	while (inb(K_STATUS) & K_IBUF_FUL);
	outb(K_RDWR, x_20);
	delay(100);
	while (inb(K_STATUS) & K_IBUF_FUL);
#endif	/* IBM_L40 */
	__asm("popfl");
}
