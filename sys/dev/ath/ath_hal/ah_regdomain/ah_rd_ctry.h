/*-
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 2002-2009 Sam Leffler, Errno Consulting
 * Copyright (c) 2005-2011 Atheros Communications, Inc.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#ifndef	__AH_REGDOMAIN_CTRY_H__
#define	__AH_REGDOMAIN_CTRY_H__

#define	DEF_REGDMN	FCC1_FCCA

/*
 * This table maps country ISO codes from net80211 into regulatory
 * domains which the ath regulatory domain code understands.
 */
static COUNTRY_CODE_TO_ENUM_RD allCountries[] = {
	{ CTRY_DEBUG,       NO_ENUMRD },
	{ CTRY_DEFAULT,     DEF_REGDMN },
	{ CTRY_ALBANIA,     NULL1_WORLD },
	{ CTRY_ALGERIA,     NULL1_WORLD },
	{ CTRY_ARGENTINA,   APL3_WORLD },
	{ CTRY_ARMENIA,     ETSI4_WORLD },
	{ CTRY_AUSTRALIA,   FCC3_WORLD },
	{ CTRY_AUSTRIA,     ETSI1_WORLD },
	{ CTRY_AZERBAIJAN,  ETSI4_WORLD },
	{ CTRY_BAHRAIN,     APL6_WORLD },
	{ CTRY_BELARUS,     NULL1_WORLD },
	{ CTRY_BELGIUM,     ETSI1_WORLD },
	{ CTRY_BELIZE,      APL1_ETSIC },
	{ CTRY_BOLIVIA,     APL1_ETSIC },
	{ CTRY_BRAZIL,      FCC3_WORLD },
	{ CTRY_BRUNEI_DARUSSALAM,APL1_WORLD },
	{ CTRY_BULGARIA,    ETSI6_WORLD },
	{ CTRY_CANADA,      FCC2_FCCA },
	{ CTRY_CHILE,       APL6_WORLD },
	{ CTRY_CHINA,       APL1_WORLD },
	{ CTRY_COLOMBIA,    FCC1_FCCA },
	{ CTRY_COSTA_RICA,  NULL1_WORLD },
	{ CTRY_CROATIA,     ETSI3_WORLD },
	{ CTRY_CYPRUS,      ETSI1_WORLD },
	{ CTRY_CZECH,       ETSI1_WORLD },
	{ CTRY_DENMARK,     ETSI1_WORLD },
	{ CTRY_DOMINICAN_REPUBLIC,FCC1_FCCA },
	{ CTRY_ECUADOR,     NULL1_WORLD },
	{ CTRY_EGYPT,       ETSI3_WORLD },
	{ CTRY_EL_SALVADOR, NULL1_WORLD },
	{ CTRY_ESTONIA,     ETSI1_WORLD },
	{ CTRY_FINLAND,     ETSI1_WORLD },
	{ CTRY_FRANCE,      ETSI1_WORLD },
	{ CTRY_FRANCE2,     ETSI3_WORLD },
	{ CTRY_GEORGIA,     ETSI4_WORLD },
	{ CTRY_GERMANY,     ETSI1_WORLD },
	{ CTRY_GREECE,      ETSI1_WORLD },
	{ CTRY_GUATEMALA,   FCC1_FCCA },
	{ CTRY_HONDURAS,    NULL1_WORLD },
	{ CTRY_HONG_KONG,   FCC2_WORLD },
	{ CTRY_HUNGARY,     ETSI1_WORLD },
	{ CTRY_ICELAND,     ETSI1_WORLD },
	{ CTRY_INDIA,       APL6_WORLD },
	{ CTRY_INDONESIA,   APL1_WORLD },
	{ CTRY_IRAN,        APL1_WORLD },
	{ CTRY_IRELAND,     ETSI1_WORLD },
	{ CTRY_ISRAEL,      NULL1_WORLD },
	{ CTRY_ITALY,       ETSI1_WORLD },
	{ CTRY_JAPAN,       MKK1_MKKA },
	{ CTRY_JAPAN1,      MKK1_MKKB },
	{ CTRY_JAPAN2,      MKK1_FCCA },
	{ CTRY_JAPAN3,      MKK2_MKKA },
	{ CTRY_JAPAN4,      MKK1_MKKA1 },
	{ CTRY_JAPAN5,      MKK1_MKKA2 },
	{ CTRY_JAPAN6,      MKK1_MKKC },

	{ CTRY_JAPAN7,      MKK3_MKKB },
	{ CTRY_JAPAN8,      MKK3_MKKA2 },
	{ CTRY_JAPAN9,      MKK3_MKKC },

	{ CTRY_JAPAN10,     MKK4_MKKB },
	{ CTRY_JAPAN11,     MKK4_MKKA2 },
	{ CTRY_JAPAN12,     MKK4_MKKC },

	{ CTRY_JAPAN13,     MKK5_MKKB },
	{ CTRY_JAPAN14,     MKK5_MKKA2 },
	{ CTRY_JAPAN15,     MKK5_MKKC },

	{ CTRY_JAPAN16,     MKK6_MKKB },
	{ CTRY_JAPAN17,     MKK6_MKKA2 },
	{ CTRY_JAPAN18,     MKK6_MKKC },

	{ CTRY_JAPAN19,     MKK7_MKKB },
	{ CTRY_JAPAN20,     MKK7_MKKA2 },
	{ CTRY_JAPAN21,     MKK7_MKKC },

	{ CTRY_JAPAN22,     MKK8_MKKB },
	{ CTRY_JAPAN23,     MKK8_MKKA2 },
	{ CTRY_JAPAN24,     MKK8_MKKC },

	{ CTRY_JORDAN,      APL4_WORLD },
	{ CTRY_KAZAKHSTAN,  NULL1_WORLD },
	{ CTRY_KOREA_NORTH, APL2_WORLD },
	{ CTRY_KOREA_ROC,   APL2_WORLD },
	{ CTRY_KOREA_ROC2,  APL2_WORLD },
	{ CTRY_KOREA_ROC3,  APL9_WORLD },
	{ CTRY_KUWAIT,      NULL1_WORLD },
	{ CTRY_LATVIA,      ETSI1_WORLD },
	{ CTRY_LEBANON,     NULL1_WORLD },
	{ CTRY_LIECHTENSTEIN,ETSI1_WORLD },
	{ CTRY_LITHUANIA,   ETSI1_WORLD },
	{ CTRY_LUXEMBOURG,  ETSI1_WORLD },
	{ CTRY_MACAU,       FCC2_WORLD },
	{ CTRY_MACEDONIA,   NULL1_WORLD },
	{ CTRY_MALAYSIA,    APL8_WORLD },
	{ CTRY_MALTA,       ETSI1_WORLD },
	{ CTRY_MEXICO,      FCC1_FCCA },
	{ CTRY_MONACO,      ETSI4_WORLD },
	{ CTRY_MOROCCO,     NULL1_WORLD },
	{ CTRY_NETHERLANDS, ETSI1_WORLD },
	{ CTRY_NEW_ZEALAND, FCC2_ETSIC },
	{ CTRY_NORWAY,      ETSI1_WORLD },
	{ CTRY_OMAN,        APL6_WORLD },
	{ CTRY_PAKISTAN,    NULL1_WORLD },
	{ CTRY_PANAMA,      FCC1_FCCA },
	{ CTRY_PERU,        APL1_WORLD },
	{ CTRY_PHILIPPINES, FCC3_WORLD },
	{ CTRY_POLAND,      ETSI1_WORLD },
	{ CTRY_PORTUGAL,    ETSI1_WORLD },
	{ CTRY_PUERTO_RICO, FCC1_FCCA },
	{ CTRY_QATAR,       NULL1_WORLD },
	{ CTRY_ROMANIA,     NULL1_WORLD },
	{ CTRY_RUSSIA,      NULL1_WORLD },
	{ CTRY_SAUDI_ARABIA,FCC2_WORLD },
	{ CTRY_SINGAPORE,   APL6_WORLD },
	{ CTRY_SLOVAKIA,    ETSI1_WORLD },
	{ CTRY_SLOVENIA,    ETSI1_WORLD },
	{ CTRY_SOUTH_AFRICA,FCC3_WORLD },
	{ CTRY_SPAIN,       ETSI1_WORLD },
	{ CTRY_SWEDEN,      ETSI1_WORLD },
	{ CTRY_SWITZERLAND, ETSI1_WORLD },
	{ CTRY_SYRIA,       NULL1_WORLD },
	{ CTRY_TAIWAN,      APL3_FCCA },
	{ CTRY_THAILAND,    FCC3_WORLD },
	{ CTRY_TRINIDAD_Y_TOBAGO,ETSI4_WORLD },
	{ CTRY_TUNISIA,     ETSI3_WORLD },
	{ CTRY_TURKEY,      ETSI3_WORLD },
	{ CTRY_UKRAINE,     NULL1_WORLD },
	{ CTRY_UAE,         NULL1_WORLD },
	{ CTRY_UNITED_KINGDOM, ETSI1_WORLD },
	{ CTRY_UNITED_STATES, FCC1_FCCA },
	{ CTRY_UNITED_STATES_FCC49,FCC4_FCCA },
	{ CTRY_URUGUAY,     FCC1_WORLD },
	{ CTRY_UZBEKISTAN,  FCC3_FCCA },
	{ CTRY_VENEZUELA,   APL2_ETSIC },
	{ CTRY_VIET_NAM,    NULL1_WORLD },
	{ CTRY_ZIMBABWE,    NULL1_WORLD }
};

#endif
