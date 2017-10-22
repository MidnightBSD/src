/*-
 * Copyright (c) 2003 David O'Brien.  All rights reserved.
 * Copyright (c) 2001 Jake Burkholder
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
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/elf32.h>
#include <sys/elf64.h>
#include <sys/endian.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	ED_DYN		(1<<0)
#define	ED_EHDR		(1<<1)
#define	ED_GOT		(1<<2)
#define	ED_HASH		(1<<3)
#define	ED_INTERP	(1<<4)
#define	ED_NOTE		(1<<5)
#define	ED_PHDR		(1<<6)
#define	ED_REL		(1<<7)
#define	ED_SHDR		(1<<8)
#define	ED_SYMTAB	(1<<9)
#define	ED_ALL		((1<<10)-1)

#define	elf_get_addr	elf_get_quad
#define	elf_get_off	elf_get_quad
#define	elf_get_size	elf_get_quad

enum elf_member {
	D_TAG = 1, D_PTR, D_VAL,

	E_CLASS, E_DATA, E_OSABI, E_TYPE, E_MACHINE, E_VERSION, E_ENTRY,
	E_PHOFF, E_SHOFF, E_FLAGS, E_EHSIZE, E_PHENTSIZE, E_PHNUM, E_SHENTSIZE,
	E_SHNUM, E_SHSTRNDX,

	N_NAMESZ, N_DESCSZ, N_TYPE,

	P_TYPE, P_OFFSET, P_VADDR, P_PADDR, P_FILESZ, P_MEMSZ, P_FLAGS,
	P_ALIGN,

	SH_NAME, SH_TYPE, SH_FLAGS, SH_ADDR, SH_OFFSET, SH_SIZE, SH_LINK,
	SH_INFO, SH_ADDRALIGN, SH_ENTSIZE,

	ST_NAME, ST_VALUE, ST_SIZE, ST_INFO, ST_SHNDX,

	R_OFFSET, R_INFO,

	RA_OFFSET, RA_INFO, RA_ADDEND
};

typedef enum elf_member elf_member_t;

int elf32_offsets[] = {
	0,

	offsetof(Elf32_Dyn, d_tag), offsetof(Elf32_Dyn, d_un.d_ptr),
	offsetof(Elf32_Dyn, d_un.d_val),

	offsetof(Elf32_Ehdr, e_ident[EI_CLASS]),
	offsetof(Elf32_Ehdr, e_ident[EI_DATA]),
	offsetof(Elf32_Ehdr, e_ident[EI_OSABI]),
	offsetof(Elf32_Ehdr, e_type), offsetof(Elf32_Ehdr, e_machine),
	offsetof(Elf32_Ehdr, e_version), offsetof(Elf32_Ehdr, e_entry),
	offsetof(Elf32_Ehdr, e_phoff), offsetof(Elf32_Ehdr, e_shoff),
	offsetof(Elf32_Ehdr, e_flags), offsetof(Elf32_Ehdr, e_ehsize),
	offsetof(Elf32_Ehdr, e_phentsize), offsetof(Elf32_Ehdr, e_phnum),
	offsetof(Elf32_Ehdr, e_shentsize), offsetof(Elf32_Ehdr, e_shnum),
	offsetof(Elf32_Ehdr, e_shstrndx),

	offsetof(Elf_Note, n_namesz), offsetof(Elf_Note, n_descsz),
	offsetof(Elf_Note, n_type),

	offsetof(Elf32_Phdr, p_type), offsetof(Elf32_Phdr, p_offset),
	offsetof(Elf32_Phdr, p_vaddr), offsetof(Elf32_Phdr, p_paddr),
	offsetof(Elf32_Phdr, p_filesz), offsetof(Elf32_Phdr, p_memsz),
	offsetof(Elf32_Phdr, p_flags), offsetof(Elf32_Phdr, p_align),

	offsetof(Elf32_Shdr, sh_name), offsetof(Elf32_Shdr, sh_type),
	offsetof(Elf32_Shdr, sh_flags), offsetof(Elf32_Shdr, sh_addr),
	offsetof(Elf32_Shdr, sh_offset), offsetof(Elf32_Shdr, sh_size),
	offsetof(Elf32_Shdr, sh_link), offsetof(Elf32_Shdr, sh_info),
	offsetof(Elf32_Shdr, sh_addralign), offsetof(Elf32_Shdr, sh_entsize),

	offsetof(Elf32_Sym, st_name), offsetof(Elf32_Sym, st_value),
	offsetof(Elf32_Sym, st_size), offsetof(Elf32_Sym, st_info),
	offsetof(Elf32_Sym, st_shndx),

	offsetof(Elf32_Rel, r_offset), offsetof(Elf32_Rel, r_info),

	offsetof(Elf32_Rela, r_offset), offsetof(Elf32_Rela, r_info),
	offsetof(Elf32_Rela, r_addend)
};

int elf64_offsets[] = {
	0,

	offsetof(Elf64_Dyn, d_tag), offsetof(Elf64_Dyn, d_un.d_ptr),
	offsetof(Elf64_Dyn, d_un.d_val),

	offsetof(Elf32_Ehdr, e_ident[EI_CLASS]),
	offsetof(Elf32_Ehdr, e_ident[EI_DATA]),
	offsetof(Elf32_Ehdr, e_ident[EI_OSABI]),
	offsetof(Elf64_Ehdr, e_type), offsetof(Elf64_Ehdr, e_machine),
	offsetof(Elf64_Ehdr, e_version), offsetof(Elf64_Ehdr, e_entry),
	offsetof(Elf64_Ehdr, e_phoff), offsetof(Elf64_Ehdr, e_shoff),
	offsetof(Elf64_Ehdr, e_flags), offsetof(Elf64_Ehdr, e_ehsize),
	offsetof(Elf64_Ehdr, e_phentsize), offsetof(Elf64_Ehdr, e_phnum),
	offsetof(Elf64_Ehdr, e_shentsize), offsetof(Elf64_Ehdr, e_shnum),
	offsetof(Elf64_Ehdr, e_shstrndx),

	offsetof(Elf_Note, n_namesz), offsetof(Elf_Note, n_descsz),
	offsetof(Elf_Note, n_type),

	offsetof(Elf64_Phdr, p_type), offsetof(Elf64_Phdr, p_offset),
	offsetof(Elf64_Phdr, p_vaddr), offsetof(Elf64_Phdr, p_paddr),
	offsetof(Elf64_Phdr, p_filesz), offsetof(Elf64_Phdr, p_memsz),
	offsetof(Elf64_Phdr, p_flags), offsetof(Elf64_Phdr, p_align),

	offsetof(Elf64_Shdr, sh_name), offsetof(Elf64_Shdr, sh_type),
	offsetof(Elf64_Shdr, sh_flags), offsetof(Elf64_Shdr, sh_addr),
	offsetof(Elf64_Shdr, sh_offset), offsetof(Elf64_Shdr, sh_size),
	offsetof(Elf64_Shdr, sh_link), offsetof(Elf64_Shdr, sh_info),
	offsetof(Elf64_Shdr, sh_addralign), offsetof(Elf64_Shdr, sh_entsize),

	offsetof(Elf64_Sym, st_name), offsetof(Elf64_Sym, st_value),
	offsetof(Elf64_Sym, st_size), offsetof(Elf64_Sym, st_info),
	offsetof(Elf64_Sym, st_shndx),

	offsetof(Elf64_Rel, r_offset), offsetof(Elf64_Rel, r_info),

	offsetof(Elf64_Rela, r_offset), offsetof(Elf64_Rela, r_info),
	offsetof(Elf64_Rela, r_addend)
};

/* http://www.sco.com/developers/gabi/latest/ch5.dynamic.html#tag_encodings */
static const char *
d_tags(u_int64_t tag) {
	switch (tag) {
	case 0: return "DT_NULL";
	case 1: return "DT_NEEDED";
	case 2: return "DT_PLTRELSZ";
	case 3: return "DT_PLTGOT";
	case 4: return "DT_HASH";
	case 5: return "DT_STRTAB";
	case 6: return "DT_SYMTAB";
	case 7: return "DT_RELA";
	case 8: return "DT_RELASZ";
	case 9: return "DT_RELAENT";
	case 10: return "DT_STRSZ";
	case 11: return "DT_SYMENT";
	case 12: return "DT_INIT";
	case 13: return "DT_FINI";
	case 14: return "DT_SONAME";
	case 15: return "DT_RPATH";
	case 16: return "DT_SYMBOLIC";
	case 17: return "DT_REL";
	case 18: return "DT_RELSZ";
	case 19: return "DT_RELENT";
	case 20: return "DT_PLTREL";
	case 21: return "DT_DEBUG";
	case 22: return "DT_TEXTREL";
	case 23: return "DT_JMPREL";
	case 24: return "DT_BIND_NOW";
	case 25: return "DT_INIT_ARRAY";
	case 26: return "DT_FINI_ARRAY";
	case 27: return "DT_INIT_ARRAYSZ";
	case 28: return "DT_FINI_ARRAYSZ";
	case 29: return "DT_RUNPATH";
	case 30: return "DT_FLAGS";
	case 32: return "DT_PREINIT_ARRAY"; /* XXX: DT_ENCODING */
	case 33: return "DT_PREINIT_ARRAYSZ";
	/* 0x6000000D - 0x6ffff000 operating system-specific semantics */
	case 0x6ffffdf5: return "DT_GNU_PRELINKED";
	case 0x6ffffdf6: return "DT_GNU_CONFLICTSZ";
	case 0x6ffffdf7: return "DT_GNU_LIBLISTSZ";
	case 0x6ffffdf8: return "DT_SUNW_CHECKSUM";
	case 0x6ffffdf9: return "DT_PLTPADSZ";
	case 0x6ffffdfa: return "DT_MOVEENT";
	case 0x6ffffdfb: return "DT_MOVESZ";
	case 0x6ffffdfc: return "DT_FEATURE";
	case 0x6ffffdfd: return "DT_POSFLAG_1";
	case 0x6ffffdfe: return "DT_SYMINSZ";
	case 0x6ffffdff: return "DT_SYMINENT (DT_VALRNGHI)";
	case 0x6ffffe00: return "DT_ADDRRNGLO";
	case 0x6ffffef8: return "DT_GNU_CONFLICT";
	case 0x6ffffef9: return "DT_GNU_LIBLIST";
	case 0x6ffffefa: return "DT_SUNW_CONFIG";
	case 0x6ffffefb: return "DT_SUNW_DEPAUDIT";
	case 0x6ffffefc: return "DT_SUNW_AUDIT";
	case 0x6ffffefd: return "DT_SUNW_PLTPAD";
	case 0x6ffffefe: return "DT_SUNW_MOVETAB";
	case 0x6ffffeff: return "DT_SYMINFO (DT_ADDRRNGHI)";
	case 0x6ffffff9: return "DT_RELACOUNT";
	case 0x6ffffffa: return "DT_RELCOUNT";
	case 0x6ffffffb: return "DT_FLAGS_1";
	case 0x6ffffffc: return "DT_VERDEF";
	case 0x6ffffffd: return "DT_VERDEFNUM";
	case 0x6ffffffe: return "DT_VERNEED";
	case 0x6fffffff: return "DT_VERNEEDNUM";
	case 0x6ffffff0: return "DT_GNU_VERSYM";
	/* 0x70000000 - 0x7fffffff processor-specific semantics */
	case 0x70000000: return "DT_IA_64_PLT_RESERVE";
	case 0x7ffffffd: return "DT_SUNW_AUXILIARY";
	case 0x7ffffffe: return "DT_SUNW_USED";
	case 0x7fffffff: return "DT_SUNW_FILTER";
	default: return "ERROR: TAG NOT DEFINED";
	}
}

static const char *
e_machines(u_int mach)
{
	static char machdesc[64];

	switch (mach) {
	case EM_NONE:	return "EM_NONE";
	case EM_M32:	return "EM_M32";
	case EM_SPARC:	return "EM_SPARC";
	case EM_386:	return "EM_386";
	case EM_68K:	return "EM_68K";
	case EM_88K:	return "EM_88K";
	case EM_860:	return "EM_860";
	case EM_MIPS:	return "EM_MIPS";
	case EM_PPC:	return "EM_PPC";
	case EM_ARM:	return "EM_ARM";
	case EM_ALPHA:	return "EM_ALPHA (legacy)";
	case EM_SPARCV9:return "EM_SPARCV9";
	case EM_IA_64:	return "EM_IA_64";
	case EM_X86_64:	return "EM_X86_64";
	}
	snprintf(machdesc, sizeof(machdesc),
	    "(unknown machine) -- type 0x%x", mach);
	return (machdesc);
}

const char *e_types[] = {
	"ET_NONE", "ET_REL", "ET_EXEC", "ET_DYN", "ET_CORE"
};

const char *ei_versions[] = {
	"EV_NONE", "EV_CURRENT"
};

const char *ei_classes[] = {
	"ELFCLASSNONE", "ELFCLASS32", "ELFCLASS64"
};

const char *ei_data[] = {
	"ELFDATANONE", "ELFDATA2LSB", "ELFDATA2MSB"
};

const char *ei_abis[] = {
	"ELFOSABI_SYSV", "ELFOSABI_HPUX", "ELFOSABI_NETBSD", "ELFOSABI_LINUX",
	"ELFOSABI_HURD", "ELFOSABI_86OPEN", "ELFOSABI_SOLARIS",
	"ELFOSABI_MONTEREY", "ELFOSABI_IRIX", "ELFOSABI_FREEBSD",
	"ELFOSABI_TRU64", "ELFOSABI_MODESTO", "ELFOSABI_OPENBSD"
};

const char *p_types[] = {
	"PT_NULL", "PT_LOAD", "PT_DYNAMIC", "PT_INTERP", "PT_NOTE",
	"PT_SHLIB", "PT_PHDR", "PT_TLS"
};

const char *p_flags[] = {
	"", "PF_X", "PF_W", "PF_X|PF_W", "PF_R", "PF_X|PF_R", "PF_W|PF_R",
	"PF_X|PF_W|PF_R"
};

/* http://www.sco.com/developers/gabi/latest/ch4.sheader.html#sh_type */
static const char *
sh_types(u_int64_t sht) {
	switch (sht) {
	case 0:	return "SHT_NULL";
	case 1: return "SHT_PROGBITS";
	case 2: return "SHT_SYMTAB";
	case 3: return "SHT_STRTAB";
	case 4: return "SHT_RELA";
	case 5: return "SHT_HASH";
	case 6: return "SHT_DYNAMIC";
	case 7: return "SHT_NOTE";
	case 8: return "SHT_NOBITS";
	case 9: return "SHT_REL";
	case 10: return "SHT_SHLIB";
	case 11: return "SHT_DYNSYM";
	case 14: return "SHT_INIT_ARRAY";
	case 15: return "SHT_FINI_ARRAY";
	case 16: return "SHT_PREINIT_ARRAY";
	case 17: return "SHT_GROUP";
	case 18: return "SHT_SYMTAB_SHNDX";
	/* 0x60000000 - 0x6fffffff operating system-specific semantics */
	case 0x6ffffff0: return "XXX:VERSYM";
	case 0x6ffffff4: return "SHT_SUNW_dof";
	case 0x6ffffff7: return "SHT_GNU_LIBLIST";
	case 0x6ffffffc: return "XXX:VERDEF";
	case 0x6ffffffd: return "SHT_SUNW(GNU)_verdef";
	case 0x6ffffffe: return "SHT_SUNW(GNU)_verneed";
	case 0x6fffffff: return "SHT_SUNW(GNU)_versym";
	/* 0x70000000 - 0x7fffffff processor-specific semantics */
	case 0x70000000: return "SHT_IA_64_EXT";
	case 0x70000001: return "SHT_IA_64_UNWIND";
	case 0x7ffffffd: return "XXX:AUXILIARY";
	case 0x7fffffff: return "XXX:FILTER";
	/* 0x80000000 - 0xffffffff application programs */
	default: return "ERROR: SHT NOT DEFINED";
	}
}

const char *sh_flags[] = {
	"", "SHF_WRITE", "SHF_ALLOC", "SHF_WRITE|SHF_ALLOC", "SHF_EXECINSTR",
	"SHF_WRITE|SHF_EXECINSTR", "SHF_ALLOC|SHF_EXECINSTR",
	"SHF_WRITE|SHF_ALLOC|SHF_EXECINSTR"
};

const char *st_types[] = {
	"STT_NOTYPE", "STT_OBJECT", "STT_FUNC", "STT_SECTION", "STT_FILE"
};

const char *st_bindings[] = {
	"STB_LOCAL", "STB_GLOBAL", "STB_WEAK"
};

char *dynstr;
char *shstrtab;
char *strtab;
FILE *out;

u_int64_t elf_get_byte(Elf32_Ehdr *e, void *base, elf_member_t member);
u_int64_t elf_get_quarter(Elf32_Ehdr *e, void *base, elf_member_t member);
u_int64_t elf_get_half(Elf32_Ehdr *e, void *base, elf_member_t member);
u_int64_t elf_get_word(Elf32_Ehdr *e, void *base, elf_member_t member);
u_int64_t elf_get_quad(Elf32_Ehdr *e, void *base, elf_member_t member);

void elf_print_ehdr(Elf32_Ehdr *e);
void elf_print_phdr(Elf32_Ehdr *e, void *p);
void elf_print_shdr(Elf32_Ehdr *e, void *sh);
void elf_print_symtab(Elf32_Ehdr *e, void *sh, char *str);
void elf_print_dynamic(Elf32_Ehdr *e, void *sh);
void elf_print_rel(Elf32_Ehdr *e, void *r);
void elf_print_rela(Elf32_Ehdr *e, void *ra);
void elf_print_interp(Elf32_Ehdr *e, void *p);
void elf_print_got(Elf32_Ehdr *e, void *sh);
void elf_print_hash(Elf32_Ehdr *e, void *sh);
void elf_print_note(Elf32_Ehdr *e, void *sh);

void usage(void);

int
main(int ac, char **av)
{
	u_int64_t phoff;
	u_int64_t shoff;
	u_int64_t phentsize;
	u_int64_t phnum;
	u_int64_t shentsize;
	u_int64_t shnum;
	u_int64_t shstrndx;
	u_int64_t offset;
	u_int64_t name;
	u_int64_t type;
	struct stat sb;
	u_int flags;
	Elf32_Ehdr *e;
	void *p;
	void *sh;
	void *v;
	int fd;
	int ch;
	int i;

	out = stdout;
	flags = 0;
	while ((ch = getopt(ac, av, "acdeiGhnprsw:")) != -1)
		switch (ch) {
		case 'a':
			flags = ED_ALL;
			break;
		case 'c':
			flags |= ED_SHDR;
			break;
		case 'd':
			flags |= ED_DYN;
			break;
		case 'e':
			flags |= ED_EHDR;
			break;
		case 'i':
			flags |= ED_INTERP;
			break;
		case 'G':
			flags |= ED_GOT;
			break;
		case 'h':
			flags |= ED_HASH;
			break;
		case 'n':
			flags |= ED_NOTE;
			break;
		case 'p':
			flags |= ED_PHDR;
			break;
		case 'r':
			flags |= ED_REL;
			break;
		case 's':
			flags |= ED_SYMTAB;
			break;
		case 'w':
			if ((out = fopen(optarg, "w")) == NULL)
				err(1, "%s", optarg);
			break;
		case '?':
		default:
			usage();
		}
	ac -= optind;
	av += optind;
	if (ac == 0 || flags == 0)
		usage();
	if ((fd = open(*av, O_RDONLY)) < 0 ||
	    fstat(fd, &sb) < 0)
		err(1, "%s", *av);
	e = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (e == MAP_FAILED)
		err(1, NULL);
	if (!IS_ELF(*(Elf32_Ehdr *)e))
		errx(1, "not an elf file");
	phoff = elf_get_off(e, e, E_PHOFF);
	shoff = elf_get_off(e, e, E_SHOFF);
	phentsize = elf_get_quarter(e, e, E_PHENTSIZE);
	phnum = elf_get_quarter(e, e, E_PHNUM);
	shentsize = elf_get_quarter(e, e, E_SHENTSIZE);
	shnum = elf_get_quarter(e, e, E_SHNUM);
	shstrndx = elf_get_quarter(e, e, E_SHSTRNDX);
	p = (char *)e + phoff;
	sh = (char *)e + shoff;
	offset = elf_get_off(e, (char *)sh + shstrndx * shentsize, SH_OFFSET);
	shstrtab = (char *)e + offset;
	for (i = 0; (u_int64_t)i < shnum; i++) {
		name = elf_get_word(e, (char *)sh + i * shentsize, SH_NAME);
		offset = elf_get_off(e, (char *)sh + i * shentsize, SH_OFFSET);
		if (strcmp(shstrtab + name, ".strtab") == 0)
			strtab = (char *)e + offset;
		if (strcmp(shstrtab + name, ".dynstr") == 0)
			dynstr = (char *)e + offset;
	}
	if (flags & ED_EHDR)
		elf_print_ehdr(e);
	if (flags & ED_PHDR)
		elf_print_phdr(e, p);
	if (flags & ED_SHDR)
		elf_print_shdr(e, sh);
	for (i = 0; (u_int64_t)i < phnum; i++) {
		v = (char *)p + i * phentsize;
		type = elf_get_word(e, v, P_TYPE);
		switch (type) {
		case PT_INTERP:
			if (flags & ED_INTERP)
				elf_print_interp(e, v);
			break;
		case PT_NULL:
		case PT_LOAD:
		case PT_DYNAMIC:
		case PT_NOTE:
		case PT_SHLIB:
		case PT_PHDR:
			break;
		}
	}
	for (i = 0; (u_int64_t)i < shnum; i++) {
		v = (char *)sh + i * shentsize;
		type = elf_get_word(e, v, SH_TYPE);
		switch (type) {
		case SHT_SYMTAB:
			if (flags & ED_SYMTAB)
				elf_print_symtab(e, v, strtab);
			break;
		case SHT_DYNAMIC:
			if (flags & ED_DYN)
				elf_print_dynamic(e, v);
			break;
		case SHT_RELA:
			if (flags & ED_REL)
				elf_print_rela(e, v);
			break;
		case SHT_REL:
			if (flags & ED_REL)
				elf_print_rel(e, v);
			break;
		case SHT_NOTE:
			name = elf_get_word(e, v, SH_NAME);
			if (flags & ED_NOTE &&
			    strcmp(shstrtab + name, ".note.ABI-tag") == 0)
				elf_print_note(e, v);
			break;
		case SHT_DYNSYM:
			if (flags & ED_SYMTAB)
				elf_print_symtab(e, v, dynstr);
			break;
		case SHT_PROGBITS:
			name = elf_get_word(e, v, SH_NAME);
			if (flags & ED_GOT &&
			    strcmp(shstrtab + name, ".got") == 0)
				elf_print_got(e, v);
			break;
		case SHT_HASH:
			if (flags & ED_HASH)
				elf_print_hash(e, v);
			break;
		case SHT_NULL:
		case SHT_STRTAB:
		case SHT_NOBITS:
		case SHT_SHLIB:
			break;
		}
	}

	return 0;
}

void
elf_print_ehdr(Elf32_Ehdr *e)
{
	u_int64_t class;
	u_int64_t data;
	u_int64_t osabi;
	u_int64_t type;
	u_int64_t machine;
	u_int64_t version;
	u_int64_t entry;
	u_int64_t phoff;
	u_int64_t shoff;
	u_int64_t flags;
	u_int64_t ehsize;
	u_int64_t phentsize;
	u_int64_t phnum;
	u_int64_t shentsize;
	u_int64_t shnum;
	u_int64_t shstrndx;

	class = elf_get_byte(e, e, E_CLASS);
	data = elf_get_byte(e, e, E_DATA);
	osabi = elf_get_byte(e, e, E_OSABI);
	type = elf_get_quarter(e, e, E_TYPE);
	machine = elf_get_quarter(e, e, E_MACHINE);
	version = elf_get_word(e, e, E_VERSION);
	entry = elf_get_addr(e, e, E_ENTRY);
	phoff = elf_get_off(e, e, E_PHOFF);
	shoff = elf_get_off(e, e, E_SHOFF);
	flags = elf_get_word(e, e, E_FLAGS);
	ehsize = elf_get_quarter(e, e, E_EHSIZE);
	phentsize = elf_get_quarter(e, e, E_PHENTSIZE);
	phnum = elf_get_quarter(e, e, E_PHNUM);
	shentsize = elf_get_quarter(e, e, E_SHENTSIZE);
	shnum = elf_get_quarter(e, e, E_SHNUM);
	shstrndx = elf_get_quarter(e, e, E_SHSTRNDX);
	fprintf(out, "\nelf header:\n");
	fprintf(out, "\n");
	fprintf(out, "\te_ident: %s %s %s\n", ei_classes[class], ei_data[data],
	    ei_abis[osabi]);
	fprintf(out, "\te_type: %s\n", e_types[type]);
	fprintf(out, "\te_machine: %s\n", e_machines(machine));
	fprintf(out, "\te_version: %s\n", ei_versions[version]);
	fprintf(out, "\te_entry: %#jx\n", (intmax_t)entry);
	fprintf(out, "\te_phoff: %jd\n", (intmax_t)phoff);
	fprintf(out, "\te_shoff: %jd\n", (intmax_t)shoff);
	fprintf(out, "\te_flags: %jd\n", (intmax_t)flags);
	fprintf(out, "\te_ehsize: %jd\n", (intmax_t)ehsize);
	fprintf(out, "\te_phentsize: %jd\n", (intmax_t)phentsize);
	fprintf(out, "\te_phnum: %jd\n", (intmax_t)phnum);
	fprintf(out, "\te_shentsize: %jd\n", (intmax_t)shentsize);
	fprintf(out, "\te_shnum: %jd\n", (intmax_t)shnum);
	fprintf(out, "\te_shstrndx: %jd\n", (intmax_t)shstrndx);
}

void
elf_print_phdr(Elf32_Ehdr *e, void *p)
{
	u_int64_t phentsize;
	u_int64_t phnum;
	u_int64_t type;
	u_int64_t offset;
	u_int64_t vaddr;
	u_int64_t paddr;
	u_int64_t filesz;
	u_int64_t memsz;
	u_int64_t flags;
	u_int64_t align;
	void *v;
	int i;

	phentsize = elf_get_quarter(e, e, E_PHENTSIZE);
	phnum = elf_get_quarter(e, e, E_PHNUM);
	fprintf(out, "\nprogram header:\n");
	for (i = 0; (u_int64_t)i < phnum; i++) {
		v = (char *)p + i * phentsize;
		type = elf_get_word(e, v, P_TYPE);
		offset = elf_get_off(e, v, P_OFFSET);
		vaddr = elf_get_addr(e, v, P_VADDR);
		paddr = elf_get_addr(e, v, P_PADDR);
		filesz = elf_get_size(e, v, P_FILESZ);
		memsz = elf_get_size(e, v, P_MEMSZ);
		flags = elf_get_word(e, v, P_FLAGS);
		align = elf_get_size(e, v, P_ALIGN);
		fprintf(out, "\n");
		fprintf(out, "entry: %d\n", i);
		fprintf(out, "\tp_type: %s\n", p_types[type & 0x7]);
		fprintf(out, "\tp_offset: %jd\n", (intmax_t)offset);
		fprintf(out, "\tp_vaddr: %#jx\n", (intmax_t)vaddr);
		fprintf(out, "\tp_paddr: %#jx\n", (intmax_t)paddr);
		fprintf(out, "\tp_filesz: %jd\n", (intmax_t)filesz);
		fprintf(out, "\tp_memsz: %jd\n", (intmax_t)memsz);
		fprintf(out, "\tp_flags: %s\n", p_flags[flags]);
		fprintf(out, "\tp_align: %jd\n", (intmax_t)align);
	}
}

void
elf_print_shdr(Elf32_Ehdr *e, void *sh)
{
	u_int64_t shentsize;
	u_int64_t shnum;
	u_int64_t name;
	u_int64_t type;
	u_int64_t flags;
	u_int64_t addr;
	u_int64_t offset;
	u_int64_t size;
	u_int64_t shlink;
	u_int64_t info;
	u_int64_t addralign;
	u_int64_t entsize;
	void *v;
	int i;

	shentsize = elf_get_quarter(e, e, E_SHENTSIZE);
	shnum = elf_get_quarter(e, e, E_SHNUM);
	fprintf(out, "\nsection header:\n");
	for (i = 0; (u_int64_t)i < shnum; i++) {
		v = (char *)sh + i * shentsize;
		name = elf_get_word(e, v, SH_NAME);
		type = elf_get_word(e, v, SH_TYPE);
		flags = elf_get_word(e, v, SH_FLAGS);
		addr = elf_get_addr(e, v, SH_ADDR);
		offset = elf_get_off(e, v, SH_OFFSET);
		size = elf_get_size(e, v, SH_SIZE);
		shlink = elf_get_word(e, v, SH_LINK);
		info = elf_get_word(e, v, SH_INFO);
		addralign = elf_get_size(e, v, SH_ADDRALIGN);
		entsize = elf_get_size(e, v, SH_ENTSIZE);
		fprintf(out, "\n");
		fprintf(out, "entry: %d\n", i);
		fprintf(out, "\tsh_name: %s\n", shstrtab + name);
		fprintf(out, "\tsh_type: %s\n", sh_types(type));
		fprintf(out, "\tsh_flags: %s\n", sh_flags[flags & 0x7]);
		fprintf(out, "\tsh_addr: %#jx\n", addr);
		fprintf(out, "\tsh_offset: %jd\n", (intmax_t)offset);
		fprintf(out, "\tsh_size: %jd\n", (intmax_t)size);
		fprintf(out, "\tsh_link: %jd\n", (intmax_t)shlink);
		fprintf(out, "\tsh_info: %jd\n", (intmax_t)info);
		fprintf(out, "\tsh_addralign: %jd\n", (intmax_t)addralign);
		fprintf(out, "\tsh_entsize: %jd\n", (intmax_t)entsize);
	}
}

void
elf_print_symtab(Elf32_Ehdr *e, void *sh, char *str)
{
	u_int64_t offset;
	u_int64_t entsize;
	u_int64_t size;
	u_int64_t name;
	u_int64_t value;
	u_int64_t info;
	u_int64_t shndx;
	void *st;
	int len;
	int i;

	offset = elf_get_off(e, sh, SH_OFFSET);
	entsize = elf_get_size(e, sh, SH_ENTSIZE);
	size = elf_get_size(e, sh, SH_SIZE);
	name = elf_get_word(e, sh, SH_NAME);
	len = size / entsize;
	fprintf(out, "\nsymbol table (%s):\n", shstrtab + name);
	for (i = 0; i < len; i++) {
		st = (char *)e + offset + i * entsize;
		name = elf_get_word(e, st, ST_NAME);
		value = elf_get_addr(e, st, ST_VALUE);
		size = elf_get_size(e, st, ST_SIZE);
		info = elf_get_byte(e, st, ST_INFO);
		shndx = elf_get_quarter(e, st, ST_SHNDX);
		fprintf(out, "\n");
		fprintf(out, "entry: %d\n", i);
		fprintf(out, "\tst_name: %s\n", str + name);
		fprintf(out, "\tst_value: %#jx\n", value);
		fprintf(out, "\tst_size: %jd\n", (intmax_t)size);
		fprintf(out, "\tst_info: %s %s\n",
		    st_types[ELF32_ST_TYPE(info)],
		    st_bindings[ELF32_ST_BIND(info)]);
		fprintf(out, "\tst_shndx: %jd\n", (intmax_t)shndx);
	}
}

void
elf_print_dynamic(Elf32_Ehdr *e, void *sh)
{
	u_int64_t offset;
	u_int64_t entsize;
	u_int64_t size;
	int64_t tag;
	u_int64_t ptr;
	u_int64_t val;
	void *d;
	int i;

	offset = elf_get_off(e, sh, SH_OFFSET);
	entsize = elf_get_size(e, sh, SH_ENTSIZE);
	size = elf_get_size(e, sh, SH_SIZE);
	fprintf(out, "\ndynamic:\n");
	for (i = 0; (u_int64_t)i < size / entsize; i++) {
		d = (char *)e + offset + i * entsize;
		tag = elf_get_size(e, d, D_TAG);
		ptr = elf_get_size(e, d, D_PTR);
		val = elf_get_addr(e, d, D_VAL);
		fprintf(out, "\n");
		fprintf(out, "entry: %d\n", i);
		fprintf(out, "\td_tag: %s\n", d_tags(tag));
		switch (tag) {
		case DT_NEEDED:
		case DT_SONAME:
		case DT_RPATH:
			fprintf(out, "\td_val: %s\n", dynstr + val);
			break;
		case DT_PLTRELSZ:
		case DT_RELA:
		case DT_RELASZ:
		case DT_RELAENT:
		case DT_STRSZ:
		case DT_SYMENT:
		case DT_RELSZ:
		case DT_RELENT:
		case DT_PLTREL:
			fprintf(out, "\td_val: %jd\n", (intmax_t)val);
			break;
		case DT_PLTGOT:
		case DT_HASH:
		case DT_STRTAB:
		case DT_SYMTAB:
		case DT_INIT:
		case DT_FINI:
		case DT_REL:
		case DT_JMPREL:
			fprintf(out, "\td_ptr: %#jx\n", ptr);
			break;
		case DT_NULL:
		case DT_SYMBOLIC:
		case DT_DEBUG:
		case DT_TEXTREL:
			break;
		}
	}
}

void
elf_print_rela(Elf32_Ehdr *e, void *sh)
{
	u_int64_t offset;
	u_int64_t entsize;
	u_int64_t size;
	u_int64_t name;
	u_int64_t info;
	int64_t addend;
	void *ra;
	void *v;
	int i;

	offset = elf_get_off(e, sh, SH_OFFSET);
	entsize = elf_get_size(e, sh, SH_ENTSIZE);
	size = elf_get_size(e, sh, SH_SIZE);
	name = elf_get_word(e, sh, SH_NAME);
	v = (char *)e + offset;
	fprintf(out, "\nrelocation with addend (%s):\n", shstrtab + name);
	for (i = 0; (u_int64_t)i < size / entsize; i++) {
		ra = (char *)v + i * entsize;
		offset = elf_get_addr(e, ra, RA_OFFSET);
		info = elf_get_word(e, ra, RA_INFO);
		addend = elf_get_off(e, ra, RA_ADDEND);
		fprintf(out, "\n");
		fprintf(out, "entry: %d\n", i);
		fprintf(out, "\tr_offset: %#jx\n", offset);
		fprintf(out, "\tr_info: %jd\n", (intmax_t)info);
		fprintf(out, "\tr_addend: %jd\n", (intmax_t)addend);
	}
}

void
elf_print_rel(Elf32_Ehdr *e, void *sh)
{
	u_int64_t offset;
	u_int64_t entsize;
	u_int64_t size;
	u_int64_t name;
	u_int64_t info;
	void *r;
	void *v;
	int i;

	offset = elf_get_off(e, sh, SH_OFFSET);
	entsize = elf_get_size(e, sh, SH_ENTSIZE);
	size = elf_get_size(e, sh, SH_SIZE);
	name = elf_get_word(e, sh, SH_NAME);
	v = (char *)e + offset;
	fprintf(out, "\nrelocation (%s):\n", shstrtab + name);
	for (i = 0; (u_int64_t)i < size / entsize; i++) {
		r = (char *)v + i * entsize;
		offset = elf_get_addr(e, r, R_OFFSET);
		info = elf_get_word(e, r, R_INFO);
		fprintf(out, "\n");
		fprintf(out, "entry: %d\n", i);
		fprintf(out, "\tr_offset: %#jx\n", offset);
		fprintf(out, "\tr_info: %jd\n", (intmax_t)info);
	}
}

void
elf_print_interp(Elf32_Ehdr *e, void *p)
{
	u_int64_t offset;
	char *s;

	offset = elf_get_off(e, p, P_OFFSET);
	s = (char *)e + offset;
	fprintf(out, "\ninterp:\n");
	fprintf(out, "\t%s\n", s);
}

void
elf_print_got(Elf32_Ehdr *e, void *sh)
{
	u_int64_t offset;
	u_int64_t addralign;
	u_int64_t size;
	u_int64_t addr;
	void *v;
	int i;

	offset = elf_get_off(e, sh, SH_OFFSET);
	addralign = elf_get_size(e, sh, SH_ADDRALIGN);
	size = elf_get_size(e, sh, SH_SIZE);
	v = (char *)e + offset;
	fprintf(out, "\nglobal offset table:\n");
	for (i = 0; (u_int64_t)i < size / addralign; i++) {
		addr = elf_get_addr(e, (char *)v + i * addralign, 0);
		fprintf(out, "\n");
		fprintf(out, "entry: %d\n", i);
		fprintf(out, "\t%#jx\n", addr);
	}
}

void
elf_print_hash(Elf32_Ehdr *e __unused, void *sh __unused)
{
}

void
elf_print_note(Elf32_Ehdr *e, void *sh)
{
	u_int64_t offset;
	u_int64_t size;
	u_int64_t name;
	u_int32_t namesz;
	u_int32_t descsz;
	u_int32_t type;
	u_int32_t desc;
	char *n, *s;

	offset = elf_get_off(e, sh, SH_OFFSET);
	size = elf_get_size(e, sh, SH_SIZE);
	name = elf_get_word(e, sh, SH_NAME);
	n = (char *)e + offset;
	fprintf(out, "\nnote (%s):\n", shstrtab + name);
 	while (n < ((char *)e + offset + size)) {
		namesz = elf_get_word(e, n, N_NAMESZ);
		descsz = elf_get_word(e, n, N_DESCSZ);
		type = elf_get_word(e, n, N_TYPE);
 		s = n + sizeof(Elf_Note);
 		desc = elf_get_word(e, n + sizeof(Elf_Note) + namesz, 0);
		fprintf(out, "\t%s %d\n", s, desc);
		n += sizeof(Elf_Note) + namesz + descsz;
	}
}

u_int64_t
elf_get_byte(Elf32_Ehdr *e, void *base, elf_member_t member)
{
	u_int64_t val;

	val = 0;
	switch (e->e_ident[EI_CLASS]) {
	case ELFCLASS32:
		val = ((char *)base)[elf32_offsets[member]];
		break;
	case ELFCLASS64:
		val = ((char *)base)[elf64_offsets[member]];
		break;
	case ELFCLASSNONE:
		errx(1, "invalid class");
	}

	return val;
}

u_int64_t
elf_get_quarter(Elf32_Ehdr *e, void *base, elf_member_t member)
{
	u_int64_t val;

	val = 0;
	switch (e->e_ident[EI_CLASS]) {
	case ELFCLASS32:
		base = (char *)base + elf32_offsets[member];
		switch (e->e_ident[EI_DATA]) {
		case ELFDATA2MSB:
			val = be16dec(base);
			break;
		case ELFDATA2LSB:
			val = le16dec(base);
			break;
		case ELFDATANONE:
			errx(1, "invalid data format");
		}
		break;
	case ELFCLASS64:
		base = (char *)base + elf64_offsets[member];
		switch (e->e_ident[EI_DATA]) {
		case ELFDATA2MSB:
			val = be16dec(base);
			break;
		case ELFDATA2LSB:
			val = le16dec(base);
			break;
		case ELFDATANONE:
			errx(1, "invalid data format");
		}
		break;
	case ELFCLASSNONE:
		errx(1, "invalid class");
	}

	return val;
}

u_int64_t
elf_get_half(Elf32_Ehdr *e, void *base, elf_member_t member)
{
	u_int64_t val;

	val = 0;
	switch (e->e_ident[EI_CLASS]) {
	case ELFCLASS32:
		base = (char *)base + elf32_offsets[member];
		switch (e->e_ident[EI_DATA]) {
		case ELFDATA2MSB:
			val = be16dec(base);
			break;
		case ELFDATA2LSB:
			val = le16dec(base);
			break;
		case ELFDATANONE:
			errx(1, "invalid data format");
		}
		break;
	case ELFCLASS64:
		base = (char *)base + elf64_offsets[member];
		switch (e->e_ident[EI_DATA]) {
		case ELFDATA2MSB:
			val = be32dec(base);
			break;
		case ELFDATA2LSB:
			val = le32dec(base);
			break;
		case ELFDATANONE:
			errx(1, "invalid data format");
		}
		break;
	case ELFCLASSNONE:
		errx(1, "invalid class");
	}

	return val;
}

u_int64_t
elf_get_word(Elf32_Ehdr *e, void *base, elf_member_t member)
{
	u_int64_t val;

	val = 0;
	switch (e->e_ident[EI_CLASS]) {
	case ELFCLASS32:
		base = (char *)base + elf32_offsets[member];
		switch (e->e_ident[EI_DATA]) {
		case ELFDATA2MSB:
			val = be32dec(base);
			break;
		case ELFDATA2LSB:
			val = le32dec(base);
			break;
		case ELFDATANONE:
			errx(1, "invalid data format");
		}
		break;
	case ELFCLASS64:
		base = (char *)base + elf64_offsets[member];
		switch (e->e_ident[EI_DATA]) {
		case ELFDATA2MSB:
			val = be32dec(base);
			break;
		case ELFDATA2LSB:
			val = le32dec(base);
			break;
		case ELFDATANONE:
			errx(1, "invalid data format");
		}
		break;
	case ELFCLASSNONE:
		errx(1, "invalid class");
	}

	return val;
}

u_int64_t
elf_get_quad(Elf32_Ehdr *e, void *base, elf_member_t member)
{
	u_int64_t val;

	val = 0;
	switch (e->e_ident[EI_CLASS]) {
	case ELFCLASS32:
		base = (char *)base + elf32_offsets[member];
		switch (e->e_ident[EI_DATA]) {
		case ELFDATA2MSB:
			val = be32dec(base);
			break;
		case ELFDATA2LSB:
			val = le32dec(base);
			break;
		case ELFDATANONE:
			errx(1, "invalid data format");
		}
		break;
	case ELFCLASS64:
		base = (char *)base + elf64_offsets[member];
		switch (e->e_ident[EI_DATA]) {
		case ELFDATA2MSB:
			val = be64dec(base);
			break;
		case ELFDATA2LSB:
			val = le64dec(base);
			break;
		case ELFDATANONE:
			errx(1, "invalid data format");
		}
		break;
	case ELFCLASSNONE:
		errx(1, "invalid class");
	}

	return val;
}

void
usage(void)
{
	fprintf(stderr, "usage: elfdump -a | -cdeGhinprs [-w file] file\n");
	exit(1);
}
