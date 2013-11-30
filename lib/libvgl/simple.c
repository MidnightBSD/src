/*-
 * Copyright (c) 1991-1997 S�ren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libvgl/simple.c,v 1.8 2002/06/02 20:05:39 schweikh Exp $");

#include <signal.h>
#include <sys/fbio.h>
#include "vgl.h"

static byte VGLSavePaletteRed[256];
static byte VGLSavePaletteGreen[256];
static byte VGLSavePaletteBlue[256];

#define ABS(a)		(((a)<0) ? -(a) : (a))
#define SGN(a)		(((a)<0) ? -1 : 1)
#define min(x, y)	(((x) < (y)) ? (x) : (y))
#define max(x, y)	(((x) > (y)) ? (x) : (y))

static void
color2mem(u_long color, byte *b, int len)
{
  switch (len) {
  case 4:
    b[3] = (color >> 24) & 0xff;
    /* fallthrough */
  case 3:
    b[2] = (color >> 16) & 0xff;
    /* fallthrough */
  case 2:
    b[1] = (color >> 8) & 0xff;
    /* fallthrough */
  case 1:
  default:
    b[0] = color & 0xff;
    break;
  }

  return;
}

static u_long
mem2color(byte *b, int len)
{
  u_long color = 0;

  switch (len) {
  case 4:
    color |= (b[3] & 0xff) << 24;
    /* fallthrough */
  case 3:
    color |= (b[2] & 0xff) << 16;
    /* fallthrough */
  case 2:
    color |= (b[1] & 0xff) << 8;
    /* fallthrough */
  case 1:
  default:
    color |= (b[0] & 0xff);
    break;
  }

  return color;
}

void
VGLSetXY(VGLBitmap *object, int x, int y, u_long color)
{
  int offset;
  byte b[4];

  VGLCheckSwitch();
  if (x>=0 && x<object->VXsize && y>=0 && y<object->VYsize) {
    if (!VGLMouseFreeze(x, y, 1, 1, color)) {
      switch (object->Type) {
      case MEMBUF:
      case VIDBUF8:
        object->Bitmap[y*object->VXsize+x]=((byte)color);
        break;
      case VIDBUF8S:
	object->Bitmap[VGLSetSegment(y*object->VXsize+x)]=((byte)color);
	break;
      case VIDBUF16:
      case VIDBUF24:
      case VIDBUF32:
	color2mem(color, b, object->PixelBytes);
        bcopy(b, &object->Bitmap[(y*object->VXsize+x) * object->PixelBytes],
		object->PixelBytes);
        break;
      case VIDBUF16S:
      case VIDBUF24S:
      case VIDBUF32S:
	color2mem(color, b, object->PixelBytes);
	offset = VGLSetSegment((y*object->VXsize+x) * object->PixelBytes);
	bcopy(b, &object->Bitmap[offset], object->PixelBytes);
	break;
      case VIDBUF8X:
        outb(0x3c4, 0x02);
        outb(0x3c5, 0x01 << (x&0x3));
	object->Bitmap[(unsigned)(VGLAdpInfo.va_line_width*y)+(x/4)] = ((byte)color);
	break;
      case VIDBUF4S:
	offset = VGLSetSegment(y*VGLAdpInfo.va_line_width + x/8);
	goto set_planar;
      case VIDBUF4:
	offset = y*VGLAdpInfo.va_line_width + x/8;
set_planar:
        outb(0x3c4, 0x02); outb(0x3c5, 0x0f);
        outb(0x3ce, 0x00); outb(0x3cf, (byte)color & 0x0f);	/* set/reset */
        outb(0x3ce, 0x01); outb(0x3cf, 0x0f);		/* set/reset enable */
        outb(0x3ce, 0x08); outb(0x3cf, 0x80 >> (x%8));	/* bit mask */
	object->Bitmap[offset] |= (byte)color;
      }
    }
    VGLMouseUnFreeze();
  }
}

u_long
VGLGetXY(VGLBitmap *object, int x, int y)
{
  int offset;
  byte b[4];
#if 0
  int i;
  u_long color;
  byte mask;
#endif

  VGLCheckSwitch();
  if (x<0 || x>=object->VXsize || y<0 || y>=object->VYsize)
    return 0;
  switch (object->Type) {
    case MEMBUF:
    case VIDBUF8:
      return object->Bitmap[((y*object->VXsize)+x)];
    case VIDBUF8S:
      return object->Bitmap[VGLSetSegment(y*object->VXsize+x)];
    case VIDBUF16:
    case VIDBUF24:
    case VIDBUF32:
      bcopy(&object->Bitmap[(y*object->VXsize+x) * object->PixelBytes],
		b, object->PixelBytes);
      return (mem2color(b, object->PixelBytes));
    case VIDBUF16S:
    case VIDBUF24S:
    case VIDBUF32S:
	offset = VGLSetSegment((y*object->VXsize+x) * object->PixelBytes);
	bcopy(&object->Bitmap[offset], b, object->PixelBytes);

      return (mem2color(b, object->PixelBytes));
    case VIDBUF8X:
      outb(0x3ce, 0x04); outb(0x3cf, x & 0x3);
      return object->Bitmap[(unsigned)(VGLAdpInfo.va_line_width*y)+(x/4)];
    case VIDBUF4S:
      offset = VGLSetSegment(y*VGLAdpInfo.va_line_width + x/8);
      goto get_planar;
    case VIDBUF4:
      offset = y*VGLAdpInfo.va_line_width + x/8;
get_planar:
#if 1
      return (object->Bitmap[offset]&(0x80>>(x%8))) ? 1 : 0;	/* XXX */
#else
      color = 0;
      mask = 0x80 >> (x%8);
      for (i = 0; i < VGLModeInfo.vi_planes; i++) {
	outb(0x3ce, 0x04); outb(0x3cf, i);
	color |= (object->Bitmap[offset] & mask) ? (1 << i) : 0;
      }
      return color;
#endif
  }
  return 0;		/* XXX black? */
}

void
VGLLine(VGLBitmap *object, int x1, int y1, int x2, int y2, u_long color)
{
  int d, x, y, ax, ay, sx, sy, dx, dy;

  dx = x2-x1; ax = ABS(dx)<<1; sx = SGN(dx); x = x1;
  dy = y2-y1; ay = ABS(dy)<<1; sy = SGN(dy); y = y1;

  if (ax>ay) {					/* x dominant */
    d = ay-(ax>>1);
    for (;;) {
      VGLSetXY(object, x, y, color);
      if (x==x2)
	break;
      if (d>=0) {
	y += sy; d -= ax;
      }
      x += sx; d += ay;
    }
  }
  else {					/* y dominant */
    d = ax-(ay>>1);
    for (;;) {
      VGLSetXY(object, x, y, color);
      if (y==y2) 
	break;
      if (d>=0) {
	x += sx; d -= ay;
      }
      y += sy; d += ax;
    }
  }
}

void
VGLBox(VGLBitmap *object, int x1, int y1, int x2, int y2, u_long color)
{
  VGLLine(object, x1, y1, x2, y1, color);
  VGLLine(object, x2, y1, x2, y2, color);
  VGLLine(object, x2, y2, x1, y2, color);
  VGLLine(object, x1, y2, x1, y1, color);
}

void
VGLFilledBox(VGLBitmap *object, int x1, int y1, int x2, int y2, u_long color)
{
  int y;

  for (y=y1; y<=y2; y++) VGLLine(object, x1, y, x2, y, color);
}

void
inline set4pixels(VGLBitmap *object, int x, int y, int xc, int yc, u_long color) 
{
  if (x!=0) { 
    VGLSetXY(object, xc+x, yc+y, color); 
    VGLSetXY(object, xc-x, yc+y, color); 
    if (y!=0) { 
      VGLSetXY(object, xc+x, yc-y, color); 
      VGLSetXY(object, xc-x, yc-y, color); 
    } 
  } 
  else { 
    VGLSetXY(object, xc, yc+y, color); 
    if (y!=0) 
      VGLSetXY(object, xc, yc-y, color); 
  } 
}

void
VGLEllipse(VGLBitmap *object, int xc, int yc, int a, int b, u_long color)
{
  int x = 0, y = b, asq = a*a, asq2 = a*a*2, bsq = b*b;
  int bsq2 = b*b*2, d = bsq-asq*b+asq/4, dx = 0, dy = asq2*b;

  while (dx<dy) {
    set4pixels(object, x, y, xc, yc, color);
    if (d>0) {
      y--; dy-=asq2; d-=dy;
    }
    x++; dx+=bsq2; d+=bsq+dx;
  }
  d+=(3*(asq-bsq)/2-(dx+dy))/2;
  while (y>=0) {
    set4pixels(object, x, y, xc, yc, color);
    if (d<0) {
      x++; dx+=bsq2; d+=dx;
    }
    y--; dy-=asq2; d+=asq-dy;
  }
}

void
inline set2lines(VGLBitmap *object, int x, int y, int xc, int yc, u_long color) 
{
  if (x!=0) { 
    VGLLine(object, xc+x, yc+y, xc-x, yc+y, color); 
    if (y!=0) 
      VGLLine(object, xc+x, yc-y, xc-x, yc-y, color); 
  } 
  else { 
    VGLLine(object, xc, yc+y, xc, yc-y, color); 
  } 
}

void
VGLFilledEllipse(VGLBitmap *object, int xc, int yc, int a, int b, u_long color)
{
  int x = 0, y = b, asq = a*a, asq2 = a*a*2, bsq = b*b;
  int bsq2 = b*b*2, d = bsq-asq*b+asq/4, dx = 0, dy = asq2*b;

  while (dx<dy) {
    set2lines(object, x, y, xc, yc, color);
    if (d>0) {
      y--; dy-=asq2; d-=dy;
    }
    x++; dx+=bsq2; d+=bsq+dx;
  }
  d+=(3*(asq-bsq)/2-(dx+dy))/2;
  while (y>=0) {
    set2lines(object, x, y, xc, yc, color);
    if (d<0) {
      x++; dx+=bsq2; d+=dx;
    }
    y--; dy-=asq2; d+=asq-dy;
  }
}

void
VGLClear(VGLBitmap *object, u_long color)
{
  int offset;
  int len;
  int i, total = 0;
  byte b[4];

  VGLCheckSwitch();
  VGLMouseFreeze(0, 0, object->Xsize, object->Ysize, color); /* XXX */
  switch (object->Type) {
  case MEMBUF:
  case VIDBUF8:
    memset(object->Bitmap, (byte)color, object->VXsize*object->VYsize);
    break;

  case VIDBUF8S:
    for (offset = 0; offset < object->VXsize*object->VYsize; ) {
      VGLSetSegment(offset);
      len = min(object->VXsize*object->VYsize - offset,
		VGLAdpInfo.va_window_size);
      memset(object->Bitmap, (byte)color, len);
      offset += len;
    }
    break;
  case VIDBUF16:
  case VIDBUF24:
  case VIDBUF32:
    color2mem(color, b, object->PixelBytes);
    total = object->VXsize*object->VYsize*object->PixelBytes;
    for (i = 0; i < total; i += object->PixelBytes)
      bcopy(b, object->Bitmap + i, object->PixelBytes);
    break;

  case VIDBUF16S:
  case VIDBUF24S:
  case VIDBUF32S:
    color2mem(color, b, object->PixelBytes);
    total = object->VXsize*object->VYsize*object->PixelBytes;
    for (offset = 0; offset < total; ) {
      VGLSetSegment(offset);
      len = min(total - offset, VGLAdpInfo.va_window_size);
      for (i = 0; i < len; i += object->PixelBytes)
	bcopy(b, object->Bitmap + offset + i, object->PixelBytes);
      offset += len;
    }
    break;

  case VIDBUF8X:
    /* XXX works only for Xsize % 4 = 0 */
    outb(0x3c6, 0xff);
    outb(0x3c4, 0x02); outb(0x3c5, 0x0f);
    memset(object->Bitmap, (byte)color, VGLAdpInfo.va_line_width*object->VYsize);
    break;

  case VIDBUF4:
  case VIDBUF4S:
    /* XXX works only for Xsize % 8 = 0 */
    outb(0x3c4, 0x02); outb(0x3c5, 0x0f);
    outb(0x3ce, 0x05); outb(0x3cf, 0x02);		/* mode 2 */
    outb(0x3ce, 0x01); outb(0x3cf, 0x00);		/* set/reset enable */
    outb(0x3ce, 0x08); outb(0x3cf, 0xff);		/* bit mask */
    for (offset = 0; offset < VGLAdpInfo.va_line_width*object->VYsize; ) {
      VGLSetSegment(offset);
      len = min(object->VXsize*object->VYsize - offset,
		VGLAdpInfo.va_window_size);
      memset(object->Bitmap, (byte)color, len);
      offset += len;
    }
    outb(0x3ce, 0x05); outb(0x3cf, 0x00);
    break;
  }
  VGLMouseUnFreeze();
}

void
VGLRestorePalette()
{
  int i;

  outb(0x3C6, 0xFF);
  inb(0x3DA); 
  outb(0x3C8, 0x00);
  for (i=0; i<256; i++) {
    outb(0x3C9, VGLSavePaletteRed[i]);
    inb(0x84);
    outb(0x3C9, VGLSavePaletteGreen[i]);
    inb(0x84);
    outb(0x3C9, VGLSavePaletteBlue[i]);
    inb(0x84);
  }
  inb(0x3DA);
  outb(0x3C0, 0x20);
}

void
VGLSavePalette()
{
  int i;

  outb(0x3C6, 0xFF);
  inb(0x3DA);
  outb(0x3C7, 0x00);
  for (i=0; i<256; i++) {
    VGLSavePaletteRed[i] = inb(0x3C9);
    inb(0x84);
    VGLSavePaletteGreen[i] = inb(0x3C9);
    inb(0x84);
    VGLSavePaletteBlue[i] = inb(0x3C9);
    inb(0x84);
  }
  inb(0x3DA);
  outb(0x3C0, 0x20);
}

void
VGLSetPalette(byte *red, byte *green, byte *blue)
{
  int i;
  
  for (i=0; i<256; i++) {
    VGLSavePaletteRed[i] = red[i];
    VGLSavePaletteGreen[i] = green[i];
    VGLSavePaletteBlue[i] = blue[i];
  }
  VGLCheckSwitch();
  outb(0x3C6, 0xFF);
  inb(0x3DA); 
  outb(0x3C8, 0x00);
  for (i=0; i<256; i++) {
    outb(0x3C9, VGLSavePaletteRed[i]);
    inb(0x84);
    outb(0x3C9, VGLSavePaletteGreen[i]);
    inb(0x84);
    outb(0x3C9, VGLSavePaletteBlue[i]);
    inb(0x84);
  }
  inb(0x3DA);
  outb(0x3C0, 0x20);
}

void
VGLSetPaletteIndex(byte color, byte red, byte green, byte blue)
{
  VGLSavePaletteRed[color] = red;
  VGLSavePaletteGreen[color] = green;
  VGLSavePaletteBlue[color] = blue;
  VGLCheckSwitch();
  outb(0x3C6, 0xFF);
  inb(0x3DA);
  outb(0x3C8, color); 
  outb(0x3C9, red); outb(0x3C9, green); outb(0x3C9, blue);
  inb(0x3DA);
  outb(0x3C0, 0x20);
}

void
VGLSetBorder(byte color)
{
  VGLCheckSwitch();
  inb(0x3DA);
  outb(0x3C0,0x11); outb(0x3C0, color); 
  inb(0x3DA);
  outb(0x3C0, 0x20);
}

void
VGLBlankDisplay(int blank)
{
  byte val;

  VGLCheckSwitch();
  outb(0x3C4, 0x01); val = inb(0x3C5); outb(0x3C4, 0x01);
  outb(0x3C5, ((blank) ? (val |= 0x20) : (val &= 0xDF)));
}
