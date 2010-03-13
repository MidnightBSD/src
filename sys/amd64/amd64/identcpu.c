/*-
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * Copyright (c) 1997 KATO Takenori.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: Id: machdep.c,v 1.193 1996/06/18 01:22:04 bde Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/amd64/amd64/identcpu.c,v 1.154.2.15 2009/12/20 01:00:41 avg Exp $");

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/eventhandler.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/power.h>

#include <machine/asmacros.h>
#include <machine/clock.h>
#include <machine/cputypes.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/md_var.h>

#include <amd64/isa/icu.h>

/* XXX - should be in header file: */
void printcpuinfo(void);
void identify_cpu(void);
void earlysetcpuclass(void);
void panicifcpuunsupported(void);

static u_int find_cpu_vendor_id(void);
static void print_AMD_info(void);
static void print_AMD_assoc(int i);
static void print_via_padlock_info(void);

int	cpu_class;
char machine[] = "amd64";
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, 
    machine, 0, "Machine class");

static char cpu_model[128];
SYSCTL_STRING(_hw, HW_MODEL, model, CTLFLAG_RD, 
    cpu_model, 0, "Machine model");

static int hw_clockrate;
SYSCTL_INT(_hw, OID_AUTO, clockrate, CTLFLAG_RD, 
    &hw_clockrate, 0, "CPU instruction clock rate");

static char cpu_brand[48];

static struct {
	char	*cpu_name;
	int	cpu_class;
} amd64_cpus[] = {
	{ "Clawhammer",		CPUCLASS_K8 },		/* CPU_CLAWHAMMER */
	{ "Sledgehammer",	CPUCLASS_K8 },		/* CPU_SLEDGEHAMMER */
};

static struct {
	char	*vendor;
	u_int	vendor_id;
} cpu_vendors[] = {
	{ INTEL_VENDOR_ID,	CPU_VENDOR_INTEL },	/* GenuineIntel */
	{ AMD_VENDOR_ID,	CPU_VENDOR_AMD },	/* AuthenticAMD */
	{ CENTAUR_VENDOR_ID,	CPU_VENDOR_CENTAUR },	/* CentaurHauls */
};

void
printcpuinfo(void)
{
	u_int regs[4], i;
	char *brand;

	cpu_class = amd64_cpus[cpu].cpu_class;
	printf("CPU: ");
	strncpy(cpu_model, amd64_cpus[cpu].cpu_name, sizeof (cpu_model));

	/* Check for extended CPUID information and a processor name. */
	if (cpu_exthigh >= 0x80000004) {
		brand = cpu_brand;
		for (i = 0x80000002; i < 0x80000005; i++) {
			do_cpuid(i, regs);
			memcpy(brand, regs, sizeof(regs));
			brand += sizeof(regs);
		}
	}

	switch (cpu_vendor_id) {
	case CPU_VENDOR_INTEL:
		/* Please make up your mind folks! */
		strcat(cpu_model, "EM64T");
		break;
	case CPU_VENDOR_AMD:
		/*
		 * Values taken from AMD Processor Recognition
		 * http://www.amd.com/K6/k6docs/pdf/20734g.pdf
		 * (also describes ``Features'' encodings.
		 */
		strcpy(cpu_model, "AMD ");
		if ((cpu_id & 0xf00) == 0xf00)
			strcat(cpu_model, "AMD64 Processor");
		else
			strcat(cpu_model, "Unknown");
		break;
	case CPU_VENDOR_CENTAUR:
		strcpy(cpu_model, "VIA ");
		if ((cpu_id & 0xff0) == 0x6f0)
			strcat(cpu_model, "Nano Processor");
		else
			strcat(cpu_model, "Unknown");
		break;
	default:
		strcat(cpu_model, "Unknown");
		break;
	}

	/*
	 * Replace cpu_model with cpu_brand minus leading spaces if
	 * we have one.
	 */
	brand = cpu_brand;
	while (*brand == ' ')
		++brand;
	if (*brand != '\0')
		strcpy(cpu_model, brand);

	printf("%s (", cpu_model);
	switch(cpu_class) {
	case CPUCLASS_K8:
		hw_clockrate = (tsc_freq + 5000) / 1000000;
		printf("%jd.%02d-MHz ",
		       (intmax_t)(tsc_freq + 4999) / 1000000,
		       (u_int)((tsc_freq + 4999) / 10000) % 100);
		printf("K8");
		break;
	default:
		printf("Unknown");	/* will panic below... */
	}
	printf("-class CPU)\n");
	if (*cpu_vendor)
		printf("  Origin = \"%s\"", cpu_vendor);
	if (cpu_id)
		printf("  Id = 0x%x", cpu_id);

	if (cpu_vendor_id == CPU_VENDOR_INTEL ||
	    cpu_vendor_id == CPU_VENDOR_AMD ||
	    cpu_vendor_id == CPU_VENDOR_CENTAUR) {
		printf("  Stepping = %u", cpu_id & 0xf);
		if (cpu_high > 0) {
			u_int cmp = 1, htt = 1;

			/*
			 * Here we should probably set up flags indicating
			 * whether or not various features are available.
			 * The interesting ones are probably VME, PSE, PAE,
			 * and PGE.  The code already assumes without bothering
			 * to check that all CPUs >= Pentium have a TSC and
			 * MSRs.
			 */
			printf("\n  Features=0x%b", cpu_feature,
			"\020"
			"\001FPU"	/* Integral FPU */
			"\002VME"	/* Extended VM86 mode support */
			"\003DE"	/* Debugging Extensions (CR4.DE) */
			"\004PSE"	/* 4MByte page tables */
			"\005TSC"	/* Timestamp counter */
			"\006MSR"	/* Machine specific registers */
			"\007PAE"	/* Physical address extension */
			"\010MCE"	/* Machine Check support */
			"\011CX8"	/* CMPEXCH8 instruction */
			"\012APIC"	/* SMP local APIC */
			"\013oldMTRR"	/* Previous implementation of MTRR */
			"\014SEP"	/* Fast System Call */
			"\015MTRR"	/* Memory Type Range Registers */
			"\016PGE"	/* PG_G (global bit) support */
			"\017MCA"	/* Machine Check Architecture */
			"\020CMOV"	/* CMOV instruction */
			"\021PAT"	/* Page attributes table */
			"\022PSE36"	/* 36 bit address space support */
			"\023PN"	/* Processor Serial number */
			"\024CLFLUSH"	/* Has the CLFLUSH instruction */
			"\025<b20>"
			"\026DTS"	/* Debug Trace Store */
			"\027ACPI"	/* ACPI support */
			"\030MMX"	/* MMX instructions */
			"\031FXSR"	/* FXSAVE/FXRSTOR */
			"\032SSE"	/* Streaming SIMD Extensions */
			"\033SSE2"	/* Streaming SIMD Extensions #2 */
			"\034SS"	/* Self snoop */
			"\035HTT"	/* Hyperthreading (see EBX bit 16-23) */
			"\036TM"	/* Thermal Monitor clock slowdown */
			"\037IA64"	/* CPU can execute IA64 instructions */
			"\040PBE"	/* Pending Break Enable */
			);

			if (cpu_feature2 != 0) {
				printf("\n  Features2=0x%b", cpu_feature2,
				"\020"
				"\001SSE3"	/* SSE3 */
				"\002<b1>"
				"\003DTES64"	/* 64-bit Debug Trace */
				"\004MON"	/* MONITOR/MWAIT Instructions */
				"\005DS_CPL"	/* CPL Qualified Debug Store */
				"\006VMX"	/* Virtual Machine Extensions */
				"\007SMX"	/* Safer Mode Extensions */
				"\010EST"	/* Enhanced SpeedStep */
				"\011TM2"	/* Thermal Monitor 2 */
				"\012SSSE3"	/* SSSE3 */
				"\013CNXT-ID"	/* L1 context ID available */
				"\014<b11>"
				"\015<b12>"
				"\016CX16"	/* CMPXCHG16B Instruction */
				"\017xTPR"	/* Send Task Priority Messages*/
				"\020PDCM"	/* Perf/Debug Capability MSR */
				"\021<b16>"
				"\022<b17>"
				"\023DCA"	/* Direct Cache Access */
				"\024SSE4.1"
				"\025SSE4.2"
				"\026x2APIC"	/* xAPIC Extensions */
				"\027MOVBE"
				"\030POPCNT"
				"\031<b24>"
				"\032<b25>"
				"\033XSAVE"
				"\034OSXSAVE"
				"\035<b28>"
				"\036<b29>"
				"\037<b30>"
				"\040<b31>"
				);
			}

			/*
			 * AMD64 Architecture Programmer's Manual Volume 3:
			 * General-Purpose and System Instructions
			 * http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/24594.pdf
			 *
			 * IA-32 Intel Architecture Software Developer's Manual,
			 * Volume 2A: Instruction Set Reference, A-M
			 * ftp://download.intel.com/design/Pentium4/manuals/25366617.pdf
			 */
			if (amd_feature != 0) {
				printf("\n  AMD Features=0x%b", amd_feature,
				"\020"		/* in hex */
				"\001<s0>"	/* Same */
				"\002<s1>"	/* Same */
				"\003<s2>"	/* Same */
				"\004<s3>"	/* Same */
				"\005<s4>"	/* Same */
				"\006<s5>"	/* Same */
				"\007<s6>"	/* Same */
				"\010<s7>"	/* Same */
				"\011<s8>"	/* Same */
				"\012<s9>"	/* Same */
				"\013<b10>"	/* Undefined */
				"\014SYSCALL"	/* Have SYSCALL/SYSRET */
				"\015<s12>"	/* Same */
				"\016<s13>"	/* Same */
				"\017<s14>"	/* Same */
				"\020<s15>"	/* Same */
				"\021<s16>"	/* Same */
				"\022<s17>"	/* Same */
				"\023<b18>"	/* Reserved, unknown */
				"\024MP"	/* Multiprocessor Capable */
				"\025NX"	/* Has EFER.NXE, NX */
				"\026<b21>"	/* Undefined */
				"\027MMX+"	/* AMD MMX Extensions */
				"\030<s23>"	/* Same */
				"\031<s24>"	/* Same */
				"\032FFXSR"	/* Fast FXSAVE/FXRSTOR */
				"\033Page1GB"	/* 1-GB large page support */
				"\034RDTSCP"	/* RDTSCP */
				"\035<b28>"	/* Undefined */
				"\036LM"	/* 64 bit long mode */
				"\0373DNow!+"	/* AMD 3DNow! Extensions */
				"\0403DNow!"	/* AMD 3DNow! */
				);
			}

			if (amd_feature2 != 0) {
				printf("\n  AMD Features2=0x%b", amd_feature2,
				"\020"
				"\001LAHF"	/* LAHF/SAHF in long mode */
				"\002CMP"	/* CMP legacy */
				"\003SVM"	/* Secure Virtual Mode */
				"\004ExtAPIC"	/* Extended APIC register */
				"\005CR8"	/* CR8 in legacy mode */
				"\006ABM"	/* LZCNT instruction */
				"\007SSE4A"	/* SSE4A */
				"\010MAS"	/* Misaligned SSE mode */
				"\011Prefetch"	/* 3DNow! Prefetch/PrefetchW */
				"\012OSVW"	/* OS visible workaround */
				"\013IBS"	/* Instruction based sampling */
				"\014SSE5"	/* SSE5 */
				"\015SKINIT"	/* SKINIT/STGI */
				"\016WDT"	/* Watchdog timer */
				"\017<b14>"
				"\020<b15>"
				"\021<b16>"
				"\022<b17>"
				"\023<b18>"
				"\024<b19>"
				"\025<b20>"
				"\026<b21>"
				"\027<b22>"
				"\030<b23>"
				"\031<b24>"
				"\032<b25>"
				"\033<b26>"
				"\034<b27>"
				"\035<b28>"
				"\036<b29>"
				"\037<b30>"
				"\040<b31>"
				);
			}

			if (cpu_vendor_id == CPU_VENDOR_CENTAUR)
				print_via_padlock_info();

			if ((cpu_feature & CPUID_HTT) &&
			    cpu_vendor_id == CPU_VENDOR_AMD)
				cpu_feature &= ~CPUID_HTT;

			/*
			 * If this CPU supports P-state invariant TSC then
			 * mention the capability.
			 */
			switch (cpu_vendor_id) {
			case CPU_VENDOR_AMD:
				if ((amd_pminfo & AMDPM_TSC_INVARIANT) ||
				    CPUID_TO_FAMILY(cpu_id) >= 0x10 ||
				    cpu_id == 0x60fb2)
					tsc_is_invariant = 1;
				break;
			case CPU_VENDOR_INTEL:
				if ((amd_pminfo & AMDPM_TSC_INVARIANT) ||
				    (CPUID_TO_FAMILY(cpu_id) == 0x6 &&
				    CPUID_TO_MODEL(cpu_id) >= 0xe) ||
				    (CPUID_TO_FAMILY(cpu_id) == 0xf &&
				    CPUID_TO_MODEL(cpu_id) >= 0x3))
					tsc_is_invariant = 1;
				break;
			case CPU_VENDOR_CENTAUR:
				if (CPUID_TO_FAMILY(cpu_id) == 0x6 &&
				    CPUID_TO_MODEL(cpu_id) >= 0xf &&
				    (rdmsr(0x1203) & 0x100000000ULL) == 0)
					tsc_is_invariant = 1;
				break;
			}
			if (tsc_is_invariant)
				printf("\n  TSC: P-state invariant");

			/*
			 * If this CPU supports HTT or CMP then mention the
			 * number of physical/logical cores it contains.
			 */
			if (cpu_feature & CPUID_HTT)
				htt = (cpu_procinfo & CPUID_HTT_CORES) >> 16;
			if (cpu_vendor_id == CPU_VENDOR_AMD &&
			    (amd_feature2 & AMDID2_CMP))
				cmp = (cpu_procinfo2 & AMDID_CMP_CORES) + 1;
			else if (cpu_vendor_id == CPU_VENDOR_INTEL &&
			    (cpu_high >= 4)) {
				cpuid_count(4, 0, regs);
				if ((regs[0] & 0x1f) != 0)
					cmp = ((regs[0] >> 26) & 0x3f) + 1;
			}
			if (cmp > 1)
				printf("\n  Cores per package: %d", cmp);
			if ((htt / cmp) > 1)
				printf("\n  Logical CPUs per core: %d",
				    htt / cmp);
		}
	}
	/* Avoid ugly blank lines: only print newline when we have to. */
	if (*cpu_vendor || cpu_id)
		printf("\n");

	if (!bootverbose)
		return;

	if (cpu_vendor_id == CPU_VENDOR_AMD)
		print_AMD_info();
}

void
panicifcpuunsupported(void)
{

#ifndef HAMMER
#error "You need to specify a cpu type"
#endif
	/*
	 * Now that we have told the user what they have,
	 * let them know if that machine type isn't configured.
	 */
	switch (cpu_class) {
	case CPUCLASS_X86:
#ifndef HAMMER
	case CPUCLASS_K8:
#endif
		panic("CPU class not configured");
	default:
		break;
	}
}


/* Update TSC freq with the value indicated by the caller. */
static void
tsc_freq_changed(void *arg, const struct cf_level *level, int status)
{
	/*
	 * If there was an error during the transition or
	 * TSC is P-state invariant, don't do anything.
	 */
	if (status != 0 || tsc_is_invariant)
		return;

	/* Total setting for this level gives the new frequency in MHz. */
	hw_clockrate = level->total_set.freq;
}

EVENTHANDLER_DEFINE(cpufreq_post_change, tsc_freq_changed, NULL,
    EVENTHANDLER_PRI_ANY);

/*
 * Final stage of CPU identification.
 */
void
identify_cpu(void)
{
	u_int regs[4];

	do_cpuid(0, regs);
	cpu_high = regs[0];
	((u_int *)&cpu_vendor)[0] = regs[1];
	((u_int *)&cpu_vendor)[1] = regs[3];
	((u_int *)&cpu_vendor)[2] = regs[2];
	cpu_vendor[12] = '\0';
	cpu_vendor_id = find_cpu_vendor_id();

	do_cpuid(1, regs);
	cpu_id = regs[0];
	cpu_procinfo = regs[1];
	cpu_feature = regs[3];
	cpu_feature2 = regs[2];

	/*
	 * Clear "Limit CPUID Maxval" bit and get the largest standard CPUID
	 * function number again if it is set from BIOS.  It is necessary
	 * for probing correct CPU topology later.
	 * XXX This is only done on the BSP package.
	 */
	if (cpu_vendor_id == CPU_VENDOR_INTEL && cpu_high > 0 && cpu_high < 4) {
		uint64_t msr;
		msr = rdmsr(MSR_IA32_MISC_ENABLE);
		if ((msr & 0x400000ULL) != 0) {
			wrmsr(MSR_IA32_MISC_ENABLE, msr & ~0x400000ULL);
			do_cpuid(0, regs);
			cpu_high = regs[0];
		}
	}

	if (cpu_vendor_id == CPU_VENDOR_INTEL ||
	    cpu_vendor_id == CPU_VENDOR_AMD ||
	    cpu_vendor_id == CPU_VENDOR_CENTAUR) {
		do_cpuid(0x80000000, regs);
		cpu_exthigh = regs[0];
	}
	if (cpu_exthigh >= 0x80000001) {
		do_cpuid(0x80000001, regs);
		amd_feature = regs[3] & ~(cpu_feature & 0x0183f3ff);
		amd_feature2 = regs[2];
	}
	if (cpu_exthigh >= 0x80000007) {
		do_cpuid(0x80000007, regs);
		amd_pminfo = regs[3];
	}
	if (cpu_exthigh >= 0x80000008) {
		do_cpuid(0x80000008, regs);
		cpu_procinfo2 = regs[2];
	}

	/* XXX */
	cpu = CPU_CLAWHAMMER;
}

static u_int
find_cpu_vendor_id(void)
{
	int	i;

	for (i = 0; i < sizeof(cpu_vendors) / sizeof(cpu_vendors[0]); i++)
		if (strcmp(cpu_vendor, cpu_vendors[i].vendor) == 0)
			return (cpu_vendors[i].vendor_id);
	return (0);
}

static void
print_AMD_assoc(int i)
{
	if (i == 255)
		printf(", fully associative\n");
	else
		printf(", %d-way associative\n", i);
}

static void
print_AMD_l2_assoc(int i)
{
	switch (i & 0x0f) {
	case 0: printf(", disabled/not present\n"); break;
	case 1: printf(", direct mapped\n"); break;
	case 2: printf(", 2-way associative\n"); break;
	case 4: printf(", 4-way associative\n"); break;
	case 6: printf(", 8-way associative\n"); break;
	case 8: printf(", 16-way associative\n"); break;
	case 15: printf(", fully associative\n"); break;
	default: printf(", reserved configuration\n"); break;
	}
}

static void
print_AMD_info(void)
{
	u_int regs[4];

	if (cpu_exthigh < 0x80000005)
		return;

	do_cpuid(0x80000005, regs);
	printf("L1 2MB data TLB: %d entries", (regs[0] >> 16) & 0xff);
	print_AMD_assoc(regs[0] >> 24);

	printf("L1 2MB instruction TLB: %d entries", regs[0] & 0xff);
	print_AMD_assoc((regs[0] >> 8) & 0xff);

	printf("L1 4KB data TLB: %d entries", (regs[1] >> 16) & 0xff);
	print_AMD_assoc(regs[1] >> 24);

	printf("L1 4KB instruction TLB: %d entries", regs[1] & 0xff);
	print_AMD_assoc((regs[1] >> 8) & 0xff);

	printf("L1 data cache: %d kbytes", regs[2] >> 24);
	printf(", %d bytes/line", regs[2] & 0xff);
	printf(", %d lines/tag", (regs[2] >> 8) & 0xff);
	print_AMD_assoc((regs[2] >> 16) & 0xff);

	printf("L1 instruction cache: %d kbytes", regs[3] >> 24);
	printf(", %d bytes/line", regs[3] & 0xff);
	printf(", %d lines/tag", (regs[3] >> 8) & 0xff);
	print_AMD_assoc((regs[3] >> 16) & 0xff);

	if (cpu_exthigh >= 0x80000006) {
		do_cpuid(0x80000006, regs);
		if ((regs[0] >> 16) != 0) {
			printf("L2 2MB data TLB: %d entries",
			    (regs[0] >> 16) & 0xfff);
			print_AMD_l2_assoc(regs[0] >> 28);
			printf("L2 2MB instruction TLB: %d entries",
			    regs[0] & 0xfff);
			print_AMD_l2_assoc((regs[0] >> 28) & 0xf);
		} else {
			printf("L2 2MB unified TLB: %d entries",
			    regs[0] & 0xfff);
			print_AMD_l2_assoc((regs[0] >> 28) & 0xf);
		}
		if ((regs[1] >> 16) != 0) {
			printf("L2 4KB data TLB: %d entries",
			    (regs[1] >> 16) & 0xfff);
			print_AMD_l2_assoc(regs[1] >> 28);

			printf("L2 4KB instruction TLB: %d entries",
			    (regs[1] >> 16) & 0xfff);
			print_AMD_l2_assoc((regs[1] >> 28) & 0xf);
		} else {
			printf("L2 4KB unified TLB: %d entries",
			    (regs[1] >> 16) & 0xfff);
			print_AMD_l2_assoc((regs[1] >> 28) & 0xf);
		}
		printf("L2 unified cache: %d kbytes", regs[2] >> 16);
		printf(", %d bytes/line", regs[2] & 0xff);
		printf(", %d lines/tag", (regs[2] >> 8) & 0x0f);
		print_AMD_l2_assoc((regs[2] >> 12) & 0x0f);	
	}

	/*
	 * Opteron Rev E shows a bug as in very rare occasions a read memory 
	 * barrier is not performed as expected if it is followed by a 
	 * non-atomic read-modify-write instruction.  
	 * As long as that bug pops up very rarely (intensive machine usage
	 * on other operating systems generally generates one unexplainable 
	 * crash any 2 months) and as long as a model specific fix would be
	 * impratical at this stage, print out a warning string if the broken
	 * model and family are identified.
	 */
	if (CPUID_TO_FAMILY(cpu_id) == 0xf &&
	    CPUID_TO_MODEL(cpu_id) >= 0x20 && CPUID_TO_MODEL(cpu_id) <= 0x3f)
		printf("WARNING: This architecture revision has known SMP "
		    "hardware bugs which may cause random instability\n");
}

static void
print_via_padlock_info(void)
{
	u_int regs[4];

	/* Check for supported models. */
	switch (cpu_id & 0xff0) {
	case 0x690:
		if ((cpu_id & 0xf) < 3)
			return;
	case 0x6a0:
	case 0x6d0:
	case 0x6f0:
		break;
	default:
		return;
	}
	
	do_cpuid(0xc0000000, regs);
	if (regs[0] >= 0xc0000001)
		do_cpuid(0xc0000001, regs);
	else
		return;

	printf("\n  VIA Padlock Features=0x%b", regs[3],
	"\020"
	"\003RNG"		/* RNG */
	"\007AES"		/* ACE */
	"\011AES-CTR"		/* ACE2 */
	"\013SHA1,SHA256"	/* PHE */
	"\015RSA"		/* PMM */
	);
}
