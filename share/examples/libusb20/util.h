/* $MidnightBSD$ */
/* ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42) (by Poul-Henning Kamp):
 * <joerg@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.        Joerg Wunsch
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD: stable/10/share/examples/libusb20/util.h 269879 2014-08-12 14:53:02Z emaste $
 */

#include <stdint.h>
#include <libusb20.h>

void print_formatted(uint8_t *buf, uint32_t len);
