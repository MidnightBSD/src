/*-
 * Copyright 2002 FreeBSD, Inc. All rights reserved.
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

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>
#include <dev/sound/pcm/ac97_patch.h>

SND_DECLARE_FILE("$FreeBSD: src/sys/dev/sound/pcm/ac97_patch.c,v 1.3.2.1 2005/12/30 19:55:54 netchild Exp $");

void ad1886_patch(struct ac97_info* codec)
{
#define AC97_AD_JACK_SPDIF 0x72
	/*
	 *    Presario700 workaround
	 *     for Jack Sense/SPDIF Register misetting causing
	 *    no audible output
	 *    by Santiago Nullo 04/05/2002
	 */
	ac97_wrcd(codec, AC97_AD_JACK_SPDIF, 0x0010);
}

void ad198x_patch(struct ac97_info* codec)
{
	ac97_wrcd(codec, 0x76, ac97_rdcd(codec, 0x76) | 0x0420);
}

void cmi9739_patch(struct ac97_info* codec)
{
	/*
	 * Few laptops (notably ASUS W1000N) need extra register
	 * initialization to power up the internal speakers.
	 */
	ac97_wrcd(codec, AC97_REG_POWER, 0x000f);
	ac97_wrcd(codec, AC97_MIXEXT_CLFE, 0x0000);
	ac97_wrcd(codec, 0x64, 0x7110);
}
