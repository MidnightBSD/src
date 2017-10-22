/*-
 * Copyright (c) 1996-2000 Distributed Processing Technology Corporation
 * Copyright (c) 2000-2001 Adaptec Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source form, with or without modification, are
 * permitted provided that redistributions of source code must retain the
 * above copyright notice, this list of conditions and the following disclaimer.
 *
 * This software is provided `as is' by Distributed Processing Technology and
 * any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose,
 * are disclaimed. In no event shall Distributed Processing Technology be
 * liable for any direct, indirect, incidental, special, exemplary or
 * consequential damages (including, but not limited to, procurement of
 * substitute goods or services; loss of use, data, or profits; or business
 * interruptions) however caused and on any theory of liability, whether in
 * contract, strict liability, or tort (including negligence or otherwise)
 * arising in any way out of the use of this driver software, even if advised
 * of the possibility of such damage.
 *
 * $FreeBSD$
 */

#ifndef __DPTSIG_H_
#define	__DPTSIG_H_
#ifdef _SINIX_ADDON
#include "dpt.h"
#endif
/* DPT SIGNATURE SPEC AND HEADER FILE				*/
/* Signature Version 1 (sorry no 'A')				*/

/* to make sure we are talking the same size under all OS's	*/
typedef unsigned char sigBYTE;
typedef unsigned short sigWORD;
#if (defined(_MULTI_DATAMODEL) && defined(sun) && !defined(_ILP32))
typedef uint32_t sigLONG;
#else
typedef unsigned long sigLONG;
#endif

/*
 * use sigWORDLittleEndian for:
 *  dsCapabilities
 *  dsDeviceSupp
 *  dsAdapterSupp
 *  dsApplication
 * use sigLONGLittleEndian for:
 *	dsOS
 * so that the sig can be standardised to Little Endian
 */
#if (defined(_DPT_BIG_ENDIAN))
# define sigWORDLittleEndian(x) ((((x)&0xFF)<<8)|(((x)>>8)&0xFF))
# define sigLONGLittleEndian(x) \
	((((x)&0xFF)<<24) |		\
	 (((x)&0xFF00)<<8) |	\
	 (((x)&0xFF0000L)>>8) | \
	 (((x)&0xFF000000L)>>24))
#else
# define sigWORDLittleEndian(x) (x)
# define sigLONGLittleEndian(x) (x)
#endif

/* must make sure the structure is not word or double-word aligned	*/
/* ---------------------------------------------------------------	*/
/* Borland will ignore the following pragma:				*/
/* Word alignment is OFF by default.  If in the, IDE make		*/
/* sure that Options | Compiler | Code Generation | Word Alignment	*/
/* is not checked.  If using BCC, do not use the -a option.		*/

#ifndef NO_PACK
#if defined(_DPT_AIX)
#pragma options align=packed
#else
#pragma pack(1)
#endif	/* aix */
#endif
/* For the Macintosh */
#ifdef STRUCTALIGNMENTSUPPORTED
#pragma options align=mac68k
#endif


/* Current Signature Version - sigBYTE dsSigVersion; */
/* ------------------------------------------------------------------ */
#define	SIG_VERSION 1

/* Processor Family - sigBYTE dsProcessorFamily;  DISTINCT VALUES */
/* ------------------------------------------------------------------ */
/* What type of processor the file is meant to run on. */
/* This will let us know whether to read sigWORDs as high/low or low/high. */
#define	PROC_INTEL	0x00	/* Intel 80x86 */
#define	PROC_MOTOROLA	0x01	/* Motorola 68K */
#define	PROC_MIPS4000	0x02	/* MIPS RISC 4000 */
#define	PROC_MIPS	PROC_MIPS4000 /* MIPS RISC */
#define	PROC_ALPHA	0x03	/* DEC Alpha */
#define	PROC_POWERPC	0x04	/* IBM Power PC */
#define	PROC_i960	0x05	/* Intel i960 */
#define	PROC_ULTRASPARC 0x06	/* SPARC processor */

/* Specific Minimim Processor - sigBYTE dsProcessor;	FLAG BITS */
/* ------------------------------------------------------------------ */
/* Different bit definitions dependent on processor_family */

/* PROC_INTEL: */
#define	PROC_8086	0x01	/* Intel 8086 */
#define	PROC_286	0x02	/* Intel 80286 */
#define	PROC_386	0x04	/* Intel 80386 */
#define	PROC_486	0x08	/* Intel 80486 */
#define	PROC_PENTIUM	0x10	/* Intel 586 aka P5 aka Pentium */
#define	PROC_SEXIUM	0x20	/* Intel 686 aka P6 aka Pentium Pro or MMX */
#define	PROC_ITANIUM	0x40	/* Intel Itanium 64 bit */

/* PROC_i960: */
#define	PROC_960RX	0x01	/* Intel 80960RP/RD */
#define	PROC_960HX	0x02	/* Intel 80960HA/HD/HT */
#define	PROC_960RN	0x03	/* Intel 80960RN/RM */
#define	PROC_960RS	0x04	/* Intel 80960RS */
#define	PROC_80303	0x05	/* Intel 80303 (ZION) */

/* PROC_MOTOROLA: */
#define	PROC_68000	0x01	/* Motorola 68000 */
#define	PROC_68010	0x02	/* Motorola 68010 */
#define	PROC_68020	0x04	/* Motorola 68020 */
#define	PROC_68030	0x08	/* Motorola 68030 */
#define	PROC_68040	0x10	/* Motorola 68040 */

/* PROC_POWERPC */
#define	PROC_PPC601		0x01	/* PowerPC 601 */
#define	PROC_PPC603		0x02	/* PowerPC 603 */
#define	PROC_PPC604		0x04	/* PowerPC 604 */

/* PROC_MIPS */
#define	PROC_R4000	0x01	/* MIPS R4000 */
#define	PROC_RM7000	0x02	/* MIPS RM7000 */

/* Filetype - sigBYTE dsFiletype;	DISTINCT VALUES */
/* ------------------------------------------------------------------ */
#define	FT_EXECUTABLE	0	/* Executable Program */
#define	FT_SCRIPT	1	/* Script/Batch File??? */
#define	FT_HBADRVR	2	/* HBA Driver */
#define	FT_OTHERDRVR	3	/* Other Driver */
#define	FT_IFS		4	/* Installable Filesystem Driver */
#define	FT_ENGINE	5	/* DPT Engine */
#define	FT_COMPDRVR	6	/* Compressed Driver Disk */
#define	FT_LANGUAGE	7	/* Foreign Language file */
#define	FT_FIRMWARE	8	/* Downloadable or actual Firmware */
#define	FT_COMMMODL	9	/* Communications Module */
#define	FT_INT13	10	/* INT 13 style HBA Driver */
#define	FT_HELPFILE	11	/* Help file */
#define	FT_LOGGER	12	/* Event Logger */
#define	FT_INSTALL	13	/* An Install Program */
#define	FT_LIBRARY	14	/* Storage Manager Real-Mode Calls */
#define	FT_RESOURCE	15	/* Storage Manager Resource File */
#define	FT_MODEM_DB	16	/* Storage Manager Modem Database */
#define	FT_DMI		17	/* DMI component interface */

/* Filetype flags - sigBYTE dsFiletypeFlags;	FLAG BITS */
/* ------------------------------------------------------------------ */
#define	FTF_DLL		0x01	/* Dynamic Link Library */
#define	FTF_NLM		0x02	/* Netware Loadable Module */
#define	FTF_OVERLAYS	0x04	/* Uses overlays */
#define	FTF_DEBUG	0x08	/* Debug version */
#define	FTF_TSR		0x10	/* TSR */
#define	FTF_SYS		0x20	/* DOS Loadable driver */
#define	FTF_PROTECTED	0x40	/* Runs in protected mode */
#define	FTF_APP_SPEC	0x80	/* Application Specific */
#define	FTF_ROM		(FTF_SYS|FTF_TSR)	/* Special Case */

/* OEM - sigBYTE dsOEM;		DISTINCT VALUES */
/* ------------------------------------------------------------------ */
#define	OEM_DPT		0	/* DPT */
#define	OEM_ATT		1	/* ATT */
#define	OEM_NEC		2	/* NEC */
#define	OEM_ALPHA	3	/* Alphatronix */
#define	OEM_AST		4	/* AST */
#define	OEM_OLIVETTI	5	/* Olivetti */
#define	OEM_SNI		6	/* Siemens/Nixdorf */
#define	OEM_SUN		7	/* SUN Microsystems */
#define	OEM_ADAPTEC	8	/* Adaptec */

/* Operating System  - sigLONG dsOS;	FLAG BITS */
/* ------------------------------------------------------------------ */
#define	OS_DOS		0x00000001 /* PC/MS-DOS				*/
#define	OS_WINDOWS	0x00000002 /* Microsoft Windows 3.x		*/
#define	OS_WINDOWS_NT	0x00000004 /* Microsoft Windows NT		*/
#define	OS_OS2M		0x00000008 /* OS/2 1.2.x,MS 1.3.0,IBM 1.3.x - Monolithic */
#define	OS_OS2L		0x00000010 /* Microsoft OS/2 1.301 - LADDR	*/
#define	OS_OS22x	0x00000020 /* IBM OS/2 2.x			*/
#define	OS_NW286	0x00000040 /* Novell NetWare 286		*/
#define	OS_NW386	0x00000080 /* Novell NetWare 386		*/
#define	OS_GEN_UNIX	0x00000100 /* Generic Unix			*/
#define	OS_SCO_UNIX	0x00000200 /* SCO Unix				*/
#define	OS_ATT_UNIX	0x00000400 /* ATT Unix				*/
#define	OS_UNIXWARE	0x00000800 /* USL Unix				*/
#define	OS_INT_UNIX	0x00001000 /* Interactive Unix			*/
#define	OS_SOLARIS	0x00002000 /* SunSoft Solaris			*/
#define	OS_QNX		0x00004000 /* QNX for Tom Moch			*/
#define	OS_NEXTSTEP	0x00008000 /* NeXTSTEP/OPENSTEP/MACH		*/
#define	OS_BANYAN	0x00010000 /* Banyan Vines			*/
#define	OS_OLIVETTI_UNIX 0x00020000/* Olivetti Unix			*/
#define	OS_MAC_OS		0x00040000 /* Mac OS				*/
#define	OS_WINDOWS_95	0x00080000 /* Microsoft Windows '95		*/
#define	OS_NW4x			0x00100000 /* Novell Netware 4.x		*/
#define	OS_BSDI_UNIX	0x00200000 /* BSDi Unix BSD/OS 2.0 and up	*/
#define	OS_AIX_UNIX	0x00400000 /* AIX Unix				*/
#define	OS_FREE_BSD		0x00800000 /* FreeBSD Unix			*/
#define	OS_LINUX		0x01000000 /* Linux				*/
#define	OS_DGUX_UNIX	0x02000000 /* Data General Unix			*/
#define	OS_SINIX_N	0x04000000 /* SNI SINIX-N			*/
#define	OS_PLAN9		0x08000000 /* ATT Plan 9			*/
#define	OS_TSX			0x10000000 /* SNH TSX-32			*/
#define	OS_WINDOWS_98	0x20000000 /* Microsoft Windows '98	*/
#define	OS_NW5x			0x40000000 /* Novell Netware 5x */

#define	OS_OTHER	0x80000000 /* Other				*/

/* Capabilities - sigWORD dsCapabilities;	 FLAG BITS */
/* ------------------------------------------------------------------ */
#define	CAP_RAID0	0x0001	/* RAID-0 */
#define	CAP_RAID1	0x0002	/* RAID-1 */
#define	CAP_RAID3	0x0004	/* RAID-3 */
#define	CAP_RAID5	0x0008	/* RAID-5 */
#define	CAP_SPAN	0x0010	/* Spanning */
#define	CAP_PASS	0x0020	/* Provides passthrough */
#define	CAP_OVERLAP	0x0040	/* Passthrough supports overlapped commands */
#define	CAP_ASPI	0x0080	/* Supports ASPI Command Requests */
#define	CAP_ABOVE16MB	0x0100	/* ISA Driver supports greater than 16MB */
#define	CAP_EXTEND	0x8000	/* Extended info appears after description */
#ifdef SNI_MIPS
#define	CAP_CACHEMODE	0x1000	/* dpt_force_cache is set in driver */
#endif

/* Devices Supported - sigWORD dsDeviceSupp;	FLAG BITS */
/* ------------------------------------------------------------------ */
#define	DEV_DASD	0x0001	/* DASD (hard drives) */
#define	DEV_TAPE	0x0002	/* Tape drives */
#define	DEV_PRINTER	0x0004	/* Printers */
#define	DEV_PROC	0x0008	/* Processors */
#define	DEV_WORM	0x0010	/* WORM drives */
#define	DEV_CDROM	0x0020	/* CD-ROM drives */
#define	DEV_SCANNER	0x0040	/* Scanners */
#define	DEV_OPTICAL	0x0080	/* Optical Drives */
#define	DEV_JUKEBOX	0x0100	/* Jukebox */
#define	DEV_COMM	0x0200	/* Communications Devices */
#define	DEV_OTHER	0x0400	/* Other Devices */
#define	DEV_ALL		0xFFFF	/* All SCSI Devices */

/* Adapters Families Supported - sigWORD dsAdapterSupp; FLAG BITS */
/* ------------------------------------------------------------------ */
#define	ADF_2001	0x0001	/* PM2001	    */
#define	ADF_2012A	0x0002	/* PM2012A	    */
#define	ADF_PLUS_ISA	0x0004	/* PM2011,PM2021    */
#define	ADF_PLUS_EISA	0x0008	/* PM2012B,PM2022   */
#define	ADF_SC3_ISA	0x0010	/* PM2021	    */
#define	ADF_SC3_EISA	0x0020	/* PM2022,PM2122, etc */
#define	ADF_SC3_PCI	0x0040	/* SmartCache III PCI */
#define	ADF_SC4_ISA	0x0080	/* SmartCache IV ISA */
#define	ADF_SC4_EISA	0x0100	/* SmartCache IV EISA */
#define	ADF_SC4_PCI	0x0200	/* SmartCache IV PCI */
#define	ADF_SC5_PCI	0x0400	/* Fifth Generation I2O products */
/*
 *	Combinations of products
 */
#define	ADF_ALL_2000	(ADF_2001|ADF_2012A)
#define	ADF_ALL_PLUS	(ADF_PLUS_ISA|ADF_PLUS_EISA)
#define	ADF_ALL_SC3	(ADF_SC3_ISA|ADF_SC3_EISA|ADF_SC3_PCI)
#define	ADF_ALL_SC4	(ADF_SC4_ISA|ADF_SC4_EISA|ADF_SC4_PCI)
#define	ADF_ALL_SC5	(ADF_SC5_PCI)
/* All EATA Cacheing Products */
#define	ADF_ALL_CACHE	(ADF_ALL_PLUS|ADF_ALL_SC3|ADF_ALL_SC4)
/* All EATA Bus Mastering Products */
#define	ADF_ALL_MASTER	(ADF_2012A|ADF_ALL_CACHE)
/* All EATA Adapter Products */
#define	ADF_ALL_EATA	(ADF_2001|ADF_ALL_MASTER)
#define	ADF_ALL		ADF_ALL_EATA

/* Application - sigWORD dsApplication;		FLAG BITS */
/* ------------------------------------------------------------------ */
#define	APP_DPTMGR	0x0001	/* DPT Storage Manager */
#define	APP_ENGINE	0x0002	/* DPT Engine */
#define	APP_SYTOS	0x0004	/* Sytron Sytos Plus */
#define	APP_CHEYENNE	0x0008	/* Cheyenne ARCServe + ARCSolo */
#define	APP_MSCDEX	0x0010	/* Microsoft CD-ROM extensions */
#define	APP_NOVABACK	0x0020	/* NovaStor Novaback */
#define	APP_AIM		0x0040	/* Archive Information Manager */

/* Requirements - sigBYTE dsRequirements;	  FLAG BITS		*/
/* ------------------------------------------------------------------	*/
#define	REQ_SMARTROM	0x01	/* Requires SmartROM to be present	*/
#define	REQ_DPTDDL	0x02	/* Requires DPTDDL.SYS to be loaded	*/
#define	REQ_HBA_DRIVER	0x04	/* Requires an HBA driver to be loaded	*/
#define	REQ_ASPI_TRAN	0x08	/* Requires an ASPI Transport Modules	*/
#define	REQ_ENGINE	0x10	/* Requires a DPT Engine to be loaded	*/
#define	REQ_COMM_ENG	0x20	/* Requires a DPT Communications Engine */

/* ------------------------------------------------------------------	*/
/* Requirements - sigWORD dsFirmware;	      FLAG BITS			*/
/* ------------------------------------------------------------------	*/
#define	dsFirmware dsApplication
#define	FW_DNLDSIZE16_OLD	0x0000	  /* 0..3 DownLoader Size 16K - TO SUPPORT OLD IMAGES */
#define	FW_DNLDSIZE16k	  0x0000    /* 0..3 DownLoader Size 16k		    */
#define	FW_DNLDSIZE16	  0x0001    /* 0..3 DownLoader Size 16K		*/
#define	FW_DNLDSIZE32	  0x0002    /* 0..3 DownLoader Size 32K		*/
#define	FW_DNLDSIZE64	  0x0004    /* 0..3 DownLoader Size 64K		*/
#define	FW_DNLDSIZE0	  0x000f    /* 0..3 DownLoader Size 0K - NONE	*/
#define	FW_DNLDSIZE_NONE	0x000F	  /* 0..3 DownLoader Size - NONE      */

		/* Code Offset is position of the code within the ROM CODE Segment */
#define	FW_DNLDR_TOP	  0x0000	/* 12 DownLoader Position (0=Top, 1=Bottom) */
#define	FW_DNLDR_BTM	  0x1000	/* 12 DownLoader Position (0=Top, 1=Bottom) Dominator */

#define	FW_LOAD_BTM		  0x0000	/* 13 Code Offset (0=Btm, 1=Top) MIPS	*/
#define	FW_LOAD_TOP		  0x2000	/* 13 Code Offset (0=Btm, 1=Top) i960	*/

#define	FW_SIG_VERSION1	  0x0000    /* 15..14 Version Bits 0=Ver1		*/
#define	FW_SIG_VERSION2	  0x4000	/* 15..14 Version Bits 1=Ver2	    */

/*
				0..3   Downloader Size (Value * 16K)

				4
				5
				6
				7

				8
				9
				10
				11

				12		Downloader Position (0=Top of Image  1= Bottom of Image (Dominator) )
				13		Load Offset (0=BTM (MIPS) -- 1=TOP (960) )
				14..15	F/W Sig Version (0=Ver1)
*/

/* ------------------------------------------------------------------	*/
/* Sub System Vendor IDs - The PCI Sub system and vendor IDs for each	*/
/* Adaptec Raid controller						*/
/* ------------------------------------------------------------------	*/
#define	PM1554U2_SUB_ID		 0xC0011044
#define	PM1654U2_SUB_ID		 0xC0021044
#define	PM1564U3_1_SUB_ID    0xC0031044
#define	PM1564U3_2_SUB_ID    0xC0041044
#define	PM1554U2_NOACPI_SUB_ID	    0xC0051044
#define	PM2554U2_SUB_ID	     0xC00A1044
#define	PM2654U2_SUB_ID	     0xC00B1044
#define	PM2664U3_1_SUB_ID    0xC00C1044
#define	PM2664U3_2_SUB_ID    0xC00D1044
#define	PM2554U2_NOACPI_SUB_ID	    0xC00E1044
#define	PM2654U2_NOACPI_SUB_ID	    0xC00F1044
#define	PM3754U2_SUB_ID	     0xC0141044
#define	PM3755U2B_SUB_ID     0xC0151044
#define	PM3755F_SUB_ID	     0xC0161044
#define	PM3757U2_1_SUB_ID    0xC01E1044
#define	PM3757U2_2_SUB_ID    0xC01F1044
#define	PM3767U3_2_SUB_ID    0xC0201044
#define	PM3767U3_4_SUB_ID    0xC0211044
#define	PM2865U3_1_SUB_ID    0xC0281044
#define	PM2865U3_2_SUB_ID    0xC0291044
#define	PM2865F_SUB_ID	     0xC02A1044
#define	ADPT2000S_1_SUB_ID	 0xC03C1044
#define	ADPT2000S_2_SUB_ID	 0xC03D1044
#define	ADPT2000F_SUB_ID	 0xC03E1044
#define	ADPT3000S_1_SUB_ID	 0xC0461044
#define	ADPT3000S_2_SUB_ID	 0xC0471044
#define	ADPT3000F_SUB_ID	 0xC0481044
#define	ADPT5000S_1_SUB_ID	 0xC0501044
#define	ADPT5000S_2_SUB_ID	 0xC0511044
#define	ADPT5000F_SUB_ID	 0xC0521044
#define	ADPT1000UDMA_SUB_ID	 0xC05A1044
#define	ADPT1000UDMA_DAC_SUB_ID	 0xC05B1044
#define	ADPTI2O_DEVICE_ID	 0xa501
#define	ADPTDOMINATOR_DEVICE_ID	 0xa511
#define	ADPTDOMINATOR_SUB_ID_START   0xC0321044
#define	ADPTDOMINATOR_SUB_ID_END     0xC03b1044



/* ------------------------------------------------------------------	*/
/* ------------------------------------------------------------------	*/
/* ------------------------------------------------------------------	*/

/*
 * You may adjust dsDescription_size with an override to a value less than
 * 50 so that the structure allocates less real space.
 */
#if (!defined(dsDescription_size))
# define dsDescription_size 50
#endif

typedef struct dpt_sig {
    char    dsSignature[6];	 /* ALWAYS "dPtSiG" */
    sigBYTE dsSigVersion;	 /* signature version (currently 1) */
    sigBYTE dsProcessorFamily;	 /* what type of processor */
    sigBYTE dsProcessor;	 /* precise processor */
    sigBYTE dsFiletype;		 /* type of file */
    sigBYTE dsFiletypeFlags;	 /* flags to specify load type, etc. */
    sigBYTE dsOEM;		 /* OEM file was created for */
    sigLONG dsOS;		 /* which Operating systems */
    sigWORD dsCapabilities;	 /* RAID levels, etc. */
    sigWORD dsDeviceSupp;	 /* Types of SCSI devices supported */
    sigWORD dsAdapterSupp;	 /* DPT adapter families supported */
    sigWORD dsApplication;	 /* applications file is for */
    sigBYTE dsRequirements;	 /* Other driver dependencies */
    sigBYTE dsVersion;		 /* 1 */
    sigBYTE dsRevision;		 /* 'J' */
    sigBYTE dsSubRevision;	 /* '9'	  ' ' if N/A */
    sigBYTE dsMonth;		 /* creation month */
    sigBYTE dsDay;		 /* creation day */
    sigBYTE dsYear;		 /* creation year since 1980 (1993=13) */
    /* description (NULL terminated) */
    char  dsDescription[dsDescription_size];
} dpt_sig_S;
/* 32 bytes minimum - with no description.  Put NULL at description[0] */
/* 81 bytes maximum - with 49 character description plus NULL. */

#if defined __bsdi__
#ifndef PACK
#define	PACK __attribute__ ((packed))
#endif
typedef struct dpt_sig_Packed {
    char    dsSignature[6] PACK;      /* ALWAYS "dPtSiG" */
    sigBYTE dsSigVersion PACK;	      /* signature version (currently 1) */
    sigBYTE dsProcessorFamily PACK;   /* what type of processor */
    sigBYTE dsProcessor PACK;	      /* precise processor */
    sigBYTE dsFiletype PACK;	      /* type of file */
    sigBYTE dsFiletypeFlags PACK;     /* flags to specify load type, etc. */
    sigBYTE dsOEM PACK;		      /* OEM file was created for */
    sigLONG dsOS PACK;		      /* which Operating systems */
    sigWORD dsCapabilities PACK;      /* RAID levels, etc. */
    sigWORD dsDeviceSupp PACK;	      /* Types of SCSI devices supported */
    sigWORD dsAdapterSupp PACK;	      /* DPT adapter families supported */
    sigWORD dsApplication PACK;	      /* applications file is for */
    sigBYTE dsRequirements PACK;      /* Other driver dependencies */
    sigBYTE dsVersion PACK;	      /* 1 */
    sigBYTE dsRevision PACK;	      /* 'J' */
    sigBYTE dsSubRevision PACK;	      /* '9'   ' ' if N/A */
    sigBYTE dsMonth PACK;	      /* creation month */
    sigBYTE dsDay PACK;		      /* creation day */
    sigBYTE dsYear PACK;	      /* creation year since 1980 (1993=13) */
    /* description (NULL terminated) */
    char  dsDescription[dsDescription_size] PACK;
} dpt_sig_S_Packed;
#define	PACKED_SIG_SIZE sizeof(dpt_sig_S_Packed)
#endif
/* This line added at Roycroft's request */
/* Microsoft's NT compiler gets confused if you do a pack and don't */
/* restore it. */

#ifndef NO_UNPACK
#if defined(_DPT_AIX)
#pragma options align=reset
#elif defined(UNPACK_FOUR)
#pragma pack(4)
#else
#pragma pack()
#endif	/* aix */
#endif
/* For the Macintosh */
#ifdef STRUCTALIGNMENTSUPPORTED
#pragma options align=reset
#endif

#endif
