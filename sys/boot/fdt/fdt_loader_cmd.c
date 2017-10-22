/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <stand.h>
#include <fdt.h>
#include <libfdt.h>
#include <sys/param.h>
#include <sys/linker.h>
#include <machine/elf.h>

#include "bootstrap.h"
#include "glue.h"

#define DEBUG

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);	\
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

#define FDT_CWD_LEN	256
#define FDT_MAX_DEPTH	6

#define FDT_PROP_SEP	" = "

#define STR(number) #number
#define STRINGIFY(number) STR(number)

#define COPYOUT(s,d,l)	archsw.arch_copyout((vm_offset_t)(s), d, l)

#define FDT_STATIC_DTB_SYMBOL	"fdt_static_dtb"

static struct fdt_header *fdtp = NULL;

static int fdt_cmd_nyi(int argc, char *argv[]);

static int fdt_cmd_mkprop(int argc, char *argv[]);
static int fdt_cmd_cd(int argc, char *argv[]);
static int fdt_cmd_hdr(int argc, char *argv[]);
static int fdt_cmd_ls(int argc, char *argv[]);
static int fdt_cmd_prop(int argc, char *argv[]);
static int fdt_cmd_pwd(int argc, char *argv[]);
static int fdt_cmd_rm(int argc, char *argv[]);
static int fdt_cmd_mknode(int argc, char *argv[]);

typedef int cmdf_t(int, char *[]);

struct cmdtab {
	char	*name;
	cmdf_t	*handler;
};

static const struct cmdtab commands[] = {
	{ "alias", &fdt_cmd_nyi },
	{ "cd", &fdt_cmd_cd },
	{ "header", &fdt_cmd_hdr },
	{ "ls", &fdt_cmd_ls },
	{ "mknode", &fdt_cmd_mknode },
	{ "mkprop", &fdt_cmd_mkprop },
	{ "mres", &fdt_cmd_nyi },
	{ "prop", &fdt_cmd_prop },
	{ "pwd", &fdt_cmd_pwd },
	{ "rm", &fdt_cmd_rm },
	{ NULL, NULL }
};

static char cwd[FDT_CWD_LEN] = "/";

static vm_offset_t
fdt_find_static_dtb(void)
{
	Elf_Sym sym;
	vm_offset_t dyntab, esym;
	uint64_t offs;
	struct preloaded_file *kfp;
	struct file_metadata *md;
	Elf_Sym *symtab;
	Elf_Dyn *dyn;
	char *strtab, *strp;
	int i, sym_count;

	symtab = NULL;
	dyntab = esym = 0;
	strtab = strp = NULL;

	offs = __elfN(relocation_offset);

	kfp = file_findfile(NULL, NULL);
	if (kfp == NULL)
		return (0);

	md = file_findmetadata(kfp, MODINFOMD_ESYM);
	if (md == NULL)
		return (0);
	COPYOUT(md->md_data, &esym, sizeof(esym));

	md = file_findmetadata(kfp, MODINFOMD_DYNAMIC);
	if (md == NULL)
		return (0);
	COPYOUT(md->md_data, &dyntab, sizeof(dyntab));

	dyntab += offs;

	/* Locate STRTAB and DYNTAB */
	for (dyn = (Elf_Dyn *)dyntab; dyn->d_tag != DT_NULL; dyn++) {
		if (dyn->d_tag == DT_STRTAB) {
			strtab = (char *)(uintptr_t)(dyn->d_un.d_ptr + offs);
			continue;
		} else if (dyn->d_tag == DT_SYMTAB) {
			symtab = (Elf_Sym *)(uintptr_t)
			    (dyn->d_un.d_ptr + offs);
			continue;
		}
	}

	if (symtab == NULL || strtab == NULL) {
		/*
		 * No symtab? No strtab? That should not happen here,
		 * and should have been verified during __elfN(loadimage).
		 * This must be some kind of a bug.
		 */
		return (0);
	}

	sym_count = (int)((Elf_Sym *)esym - symtab) / sizeof(Elf_Sym);

	/*
	 * The most efficent way to find a symbol would be to calculate a
	 * hash, find proper bucket and chain, and thus find a symbol.
	 * However, that would involve code duplication (e.g. for hash
	 * function). So we're using simpler and a bit slower way: we're
	 * iterating through symbols, searching for the one which name is
	 * 'equal' to 'fdt_static_dtb'. To speed up the process a little bit,
	 * we are eliminating symbols type of which is not STT_NOTYPE, or(and)
	 * those which binding attribute is not STB_GLOBAL.
	 */
	for (i = 0; i < sym_count; i++) {
		COPYOUT(symtab + i, &sym, sizeof(sym));
		if (ELF_ST_BIND(sym.st_info) != STB_GLOBAL ||
		    ELF_ST_TYPE(sym.st_info) != STT_NOTYPE)
			continue;

		strp = strdupout((vm_offset_t)(strtab + sym.st_name));
		if (strcmp(strp, FDT_STATIC_DTB_SYMBOL) == 0) {
			/* Found a match ! */
			free(strp);
			return ((vm_offset_t)(sym.st_value + offs));
		}
		free(strp);
	}
	return (0);
}

static int
fdt_setup_fdtp()
{
	struct preloaded_file *bfp;
	int err;

	/*
	 * Find the device tree blob.
	 */
	bfp = file_findfile(NULL, "dtb");
	if (bfp == NULL) {
		if ((fdtp = (struct fdt_header *)fdt_find_static_dtb()) == 0) {
			command_errmsg = "no device tree blob found!";
			return (CMD_ERROR);
		}
	} else {
		/* Dynamic blob has precedence over static. */
		fdtp = (struct fdt_header *)bfp->f_addr;
	}

	/*
	 * Validate the blob.
	 */
	err = fdt_check_header(fdtp);
	if (err < 0) {
		if (err == -FDT_ERR_BADVERSION)
			sprintf(command_errbuf,
			    "incompatible blob version: %d, should be: %d",
			    fdt_version(fdtp), FDT_LAST_SUPPORTED_VERSION);

		else
			sprintf(command_errbuf, "error validating blob: %s",
			    fdt_strerror(err));
		return (CMD_ERROR);
	}
	return (CMD_OK);
}

#define fdt_strtovect(str, cellbuf, lim, cellsize) _fdt_strtovect((str), \
    (cellbuf), (lim), (cellsize), 0);

/* Force using base 16 */
#define fdt_strtovectx(str, cellbuf, lim, cellsize) _fdt_strtovect((str), \
    (cellbuf), (lim), (cellsize), 16);

static int
_fdt_strtovect(char *str, void *cellbuf, int lim, unsigned char cellsize,
    uint8_t base)
{
	char *buf = str;
	char *end = str + strlen(str) - 2;
	uint32_t *u32buf = NULL;
	uint8_t *u8buf = NULL;
	int cnt = 0;

	if (cellsize == sizeof(uint32_t))
		u32buf = (uint32_t *)cellbuf;
	else
		u8buf = (uint8_t *)cellbuf;

	if (lim == 0)
		return (0);

	while (buf < end) {

		/* Skip white whitespace(s)/separators */
		while (!isxdigit(*buf) && buf < end)
			buf++;

		if (u32buf != NULL)
			u32buf[cnt] =
			    cpu_to_fdt32((uint32_t)strtol(buf, NULL, base));

		else
			u8buf[cnt] = (uint8_t)strtol(buf, NULL, base);

		if (cnt + 1 <= lim - 1)
			cnt++;
		else
			break;
		buf++;
		/* Find another number */
		while ((isxdigit(*buf) || *buf == 'x') && buf < end)
			buf++;
	}
	return (cnt);
}

#define	TMP_MAX_ETH	8

void
fixup_ethernet(const char *env, char *ethstr, int *eth_no, int len)
{
	char *end, *str;
	uint8_t tmp_addr[6];
	int i, n;

	/* Extract interface number */
	i = strtol(env + 3, &end, 10);
	if (end == (env + 3))
		/* 'ethaddr' means interface 0 address */
		n = 0;
	else
		n = i;

	if (n > TMP_MAX_ETH)
		return;

	str = ub_env_get(env);

	/* Convert macaddr string into a vector of uints */
	fdt_strtovectx(str, &tmp_addr, 6, sizeof(uint8_t));
	if (n != 0) {
		i = strlen(env) - 7;
		strncpy(ethstr + 8, env + 3, i);
	}
	/* Set actual property to a value from vect */
	fdt_setprop(fdtp, fdt_path_offset(fdtp, ethstr),
	    "local-mac-address", &tmp_addr, 6 * sizeof(uint8_t));

	/* Clear ethernet..XXXX.. string */
	bzero(ethstr + 8, len - 8);

	if (n + 1 > *eth_no)
		*eth_no = n + 1;
}

void
fixup_cpubusfreqs(unsigned long cpufreq, unsigned long busfreq)
{
	int lo, o = 0, o2, maxo = 0, depth;
	const uint32_t zero = 0;

	/* We want to modify every subnode of /cpus */
	o = fdt_path_offset(fdtp, "/cpus");

	/* maxo should contain offset of node next to /cpus */
	depth = 0;
	maxo = o;
	while (depth != -1)
		maxo = fdt_next_node(fdtp, maxo, &depth);

	/* Find CPU frequency properties */
	o = fdt_node_offset_by_prop_value(fdtp, o, "clock-frequency",
	    &zero, sizeof(uint32_t));

	o2 = fdt_node_offset_by_prop_value(fdtp, o, "bus-frequency", &zero,
	    sizeof(uint32_t));

	lo = MIN(o, o2);

	while (o != -FDT_ERR_NOTFOUND && o2 != -FDT_ERR_NOTFOUND) {

		o = fdt_node_offset_by_prop_value(fdtp, lo,
		    "clock-frequency", &zero, sizeof(uint32_t));

		o2 = fdt_node_offset_by_prop_value(fdtp, lo, "bus-frequency",
		    &zero, sizeof(uint32_t));

		/* We're only interested in /cpus subnode(s) */
		if (lo > maxo)
			break;

		fdt_setprop_inplace_cell(fdtp, lo, "clock-frequency",
		    (uint32_t)cpufreq);

		fdt_setprop_inplace_cell(fdtp, lo, "bus-frequency",
		    (uint32_t)busfreq);

		lo = MIN(o, o2);
	}
}

int
fdt_reg_valid(uint32_t *reg, int len, int addr_cells, int size_cells)
{
	int cells_in_tuple, i, tuples, tuple_size;
	uint32_t cur_start, cur_size;

	cells_in_tuple = (addr_cells + size_cells);
	tuple_size = cells_in_tuple * sizeof(uint32_t);
	tuples = len / tuple_size;
	if (tuples == 0)
		return (EINVAL);

	for (i = 0; i < tuples; i++) {
		if (addr_cells == 2)
			cur_start = fdt64_to_cpu(reg[i * cells_in_tuple]);
		else
			cur_start = fdt32_to_cpu(reg[i * cells_in_tuple]);

		if (size_cells == 2)
			cur_size = fdt64_to_cpu(reg[i * cells_in_tuple + 2]);
		else
			cur_size = fdt32_to_cpu(reg[i * cells_in_tuple + 1]);

		if (cur_size == 0)
			return (EINVAL);

		debugf(" reg#%d (start: 0x%0x size: 0x%0x) valid!\n",
		    i, cur_start, cur_size);
	}
	return (0);
}

void
fixup_memory(struct sys_info *si)
{
	struct mem_region *curmr;
	uint32_t addr_cells, size_cells;
	uint32_t *addr_cellsp, *reg,  *size_cellsp;
	int err, i, len, memory, realmrno, root;
	uint8_t *buf, *sb;

	root = fdt_path_offset(fdtp, "/");
	if (root < 0) {
		sprintf(command_errbuf, "Could not find root node !");
		return;
	}

	memory = fdt_path_offset(fdtp, "/memory");
	if (memory <= 0) {
		/* Create proper '/memory' node. */
		memory = fdt_add_subnode(fdtp, root, "memory");
		if (memory <= 0) {
			sprintf(command_errbuf, "Could not fixup '/memory' "
			    "node, error code : %d!\n", memory);
			return;
		}

		err = fdt_setprop(fdtp, memory, "device_type", "memory",
		    sizeof("memory"));

		if (err < 0)
			return;
	}

	addr_cellsp = (uint32_t *)fdt_getprop(fdtp, root, "#address-cells",
	    NULL);
	size_cellsp = (uint32_t *)fdt_getprop(fdtp, root, "#size-cells", NULL);

	if (addr_cellsp == NULL || size_cellsp == NULL) {
		sprintf(command_errbuf, "Could not fixup '/memory' node : "
		    "%s %s property not found in root node!\n",
		    (!addr_cellsp) ? "#address-cells" : "",
		    (!size_cellsp) ? "#size-cells" : "");
		return;
	}

	addr_cells = fdt32_to_cpu(*addr_cellsp);
	size_cells = fdt32_to_cpu(*size_cellsp);

	/* Count valid memory regions entries in sysinfo. */
	realmrno = si->mr_no;
	for (i = 0; i < si->mr_no; i++)
		if (si->mr[i].start == 0 && si->mr[i].size == 0)
			realmrno--;

	if (realmrno == 0) {
		sprintf(command_errbuf, "Could not fixup '/memory' node : "
		    "sysinfo doesn't contain valid memory regions info!\n");
		return;
	}

	if ((reg = (uint32_t *)fdt_getprop(fdtp, memory, "reg",
	    &len)) != NULL) {

		if (fdt_reg_valid(reg, len, addr_cells, size_cells) == 0)
			/*
			 * Do not apply fixup if existing 'reg' property
			 * seems to be valid.
			 */
			return;
	}

	len = (addr_cells + size_cells) * realmrno * sizeof(uint32_t);
	sb = buf = (uint8_t *)malloc(len);
	if (!buf)
		return;

	bzero(buf, len);

	for (i = 0; i < si->mr_no; i++) {
		curmr = &si->mr[i];
		if (curmr->size != 0) {
			/* Ensure endianess, and put cells into a buffer */
			if (addr_cells == 2)
				*(uint64_t *)buf =
				    cpu_to_fdt64(curmr->start);
			else
				*(uint32_t *)buf =
				    cpu_to_fdt32(curmr->start);

			buf += sizeof(uint32_t) * addr_cells;
			if (size_cells == 2)
				*(uint64_t *)buf =
				    cpu_to_fdt64(curmr->size);
			else
				*(uint32_t *)buf =
				    cpu_to_fdt32(curmr->size);

			buf += sizeof(uint32_t) * size_cells;
		}
	}

	/* Set property */
	if ((err = fdt_setprop(fdtp, memory, "reg", sb, len)) < 0)
		sprintf(command_errbuf, "Could not fixup '/memory' node.\n");
}

void
fixup_stdout(const char *env)
{
	const char *str;
	char *ptr;
	int serialno;
	int len, no, sero;
	const struct fdt_property *prop;
	char *tmp[10];

	str = ub_env_get(env);
	ptr = (char *)str + strlen(str) - 1;
	while (ptr > str && isdigit(*(str - 1)))
		str--;

	if (ptr == str)
		return;

	serialno = (int)strtol(ptr, NULL, 0);
	no = fdt_path_offset(fdtp, "/chosen");
	if (no < 0)
		return;

	prop = fdt_get_property(fdtp, no, "stdout", &len);

	/* If /chosen/stdout does not extist, create it */
	if (prop == NULL || (prop != NULL && len == 0)) {

		bzero(tmp, 10 * sizeof(char));
		strcpy((char *)&tmp, "serial");
		if (strlen(ptr) > 3)
			/* Serial number too long */
			return;

		strncpy((char *)tmp + 6, ptr, 3);
		sero = fdt_path_offset(fdtp, (const char *)tmp);
		if (sero < 0)
			/*
			 * If serial device we're trying to assign
			 * stdout to doesn't exist in DT -- return.
			 */
			return;

		fdt_setprop(fdtp, no, "stdout", &tmp,
		    strlen((char *)&tmp) + 1);
		fdt_setprop(fdtp, no, "stdin", &tmp,
		    strlen((char *)&tmp) + 1);
	}
}

/*
 * Locate the blob, fix it up and return its location.
 */
void *
fdt_fixup(void)
{
	const char *env;
	char *ethstr;
	int chosen, err, eth_no, len;
	struct sys_info *si;

	env = NULL;
	eth_no = 0;
	ethstr = NULL;
	len = 0;

	err = fdt_setup_fdtp();
	if (err) {
		sprintf(command_errbuf, "No valid device tree blob found!");
		return (NULL);
	}

	/* Create /chosen node (if not exists) */
	if ((chosen = fdt_subnode_offset(fdtp, 0, "chosen")) ==
	    -FDT_ERR_NOTFOUND)
		chosen = fdt_add_subnode(fdtp, 0, "chosen");

	/* Value assigned to fixup-applied does not matter. */
	if (fdt_getprop(fdtp, chosen, "fixup-applied", NULL))
		goto success;

	/* Acquire sys_info */
	si = ub_get_sys_info();

	while ((env = ub_env_enum(env)) != NULL) {
		if (strncmp(env, "eth", 3) == 0 &&
		    strncmp(env + (strlen(env) - 4), "addr", 4) == 0) {
			/*
			 * Handle Ethernet addrs: parse uboot env eth%daddr
			 */

			if (!eth_no) {
				/*
				 * Check how many chars we will need to store
				 * maximal eth iface number.
				 */
				len = strlen(STRINGIFY(TMP_MAX_ETH)) +
				    strlen("ethernet");

				/*
				 * Reserve mem for string "ethernet" and len
				 * chars for iface no.
				 */
				ethstr = (char *)malloc(len * sizeof(char));
				bzero(ethstr, len * sizeof(char));
				strcpy(ethstr, "ethernet0");
			}

			/* Modify blob */
			fixup_ethernet(env, ethstr, &eth_no, len);

		} else if (strcmp(env, "consoledev") == 0)
			fixup_stdout(env);
	}

	/* Modify cpu(s) and bus clock frequenties in /cpus node [Hz] */
	fixup_cpubusfreqs(si->clk_cpu, si->clk_bus);

	/* Fixup memory regions */
	fixup_memory(si);

	fdt_setprop(fdtp, chosen, "fixup-applied", NULL, 0);

success:
	return (fdtp);
}

int
command_fdt_internal(int argc, char *argv[])
{
	cmdf_t *cmdh;
	char *cmd;
	int i, err;

	if (argc < 2) {
		command_errmsg = "usage is 'fdt <command> [<args>]";
		return (CMD_ERROR);
	}

	/*
	 * Check if uboot env vars were parsed already. If not, do it now.
	 */
	if (fdt_fixup() == NULL)
		return (CMD_ERROR);

	/*
	 * Validate fdt <command>.
	 */
	cmd = strdup(argv[1]);
	i = 0;
	cmdh = NULL;
	while (!(commands[i].name == NULL)) {
		if (strcmp(cmd, commands[i].name) == 0) {
			/* found it */
			cmdh = commands[i].handler;
			break;
		}
		i++;
	}
	if (cmdh == NULL) {
		command_errmsg = "unknown command";
		return (CMD_ERROR);
	}

	/*
	 * Call command handler.
	 */
	err = (*cmdh)(argc, argv);

	return (err);
}

static int
fdt_cmd_cd(int argc, char *argv[])
{
	char *path;
	char tmp[FDT_CWD_LEN];
	int len, o;

	path = (argc > 2) ? argv[2] : "/";

	if (path[0] == '/') {
		len = strlen(path);
		if (len >= FDT_CWD_LEN)
			goto fail;
	} else {
		/* Handle path specification relative to cwd */
		len = strlen(cwd) + strlen(path) + 1;
		if (len >= FDT_CWD_LEN)
			goto fail;

		strcpy(tmp, cwd);
		strcat(tmp, "/");
		strcat(tmp, path);
		path = tmp;
	}

	o = fdt_path_offset(fdtp, path);
	if (o < 0) {
		sprintf(command_errbuf, "could not find node: '%s'", path);
		return (CMD_ERROR);
	}

	strcpy(cwd, path);
	return (CMD_OK);

fail:
	sprintf(command_errbuf, "path too long: %d, max allowed: %d",
	    len, FDT_CWD_LEN - 1);
	return (CMD_ERROR);
}

static int
fdt_cmd_hdr(int argc __unused, char *argv[] __unused)
{
	char line[80];
	int ver;

	if (fdtp == NULL) {
		command_errmsg = "no device tree blob pointer?!";
		return (CMD_ERROR);
	}

	ver = fdt_version(fdtp);
	pager_open();
	sprintf(line, "\nFlattened device tree header (%p):\n", fdtp);
	pager_output(line);
	sprintf(line, " magic                   = 0x%08x\n", fdt_magic(fdtp));
	pager_output(line);
	sprintf(line, " size                    = %d\n", fdt_totalsize(fdtp));
	pager_output(line);
	sprintf(line, " off_dt_struct           = 0x%08x\n",
	    fdt_off_dt_struct(fdtp));
	pager_output(line);
	sprintf(line, " off_dt_strings          = 0x%08x\n",
	    fdt_off_dt_strings(fdtp));
	pager_output(line);
	sprintf(line, " off_mem_rsvmap          = 0x%08x\n",
	    fdt_off_mem_rsvmap(fdtp));
	pager_output(line);
	sprintf(line, " version                 = %d\n", ver); 
	pager_output(line);
	sprintf(line, " last compatible version = %d\n",
	    fdt_last_comp_version(fdtp));
	pager_output(line);
	if (ver >= 2) {
		sprintf(line, " boot_cpuid              = %d\n",
		    fdt_boot_cpuid_phys(fdtp));
		pager_output(line);
	}
	if (ver >= 3) {
		sprintf(line, " size_dt_strings         = %d\n",
		    fdt_size_dt_strings(fdtp));
		pager_output(line);
	}
	if (ver >= 17) {
		sprintf(line, " size_dt_struct          = %d\n",
		    fdt_size_dt_struct(fdtp));
		pager_output(line);
	}
	pager_close();

	return (CMD_OK);
}

static int
fdt_cmd_ls(int argc, char *argv[])
{
	const char *prevname[FDT_MAX_DEPTH] = { NULL };
	const char *name;
	char *path;
	int i, o, depth, len;

	path = (argc > 2) ? argv[2] : NULL;
	if (path == NULL)
		path = cwd;

	o = fdt_path_offset(fdtp, path);
	if (o < 0) {
		sprintf(command_errbuf, "could not find node: '%s'", path);
		return (CMD_ERROR);
	}

	for (depth = 0;
	    (o >= 0) && (depth >= 0);
	    o = fdt_next_node(fdtp, o, &depth)) {

		name = fdt_get_name(fdtp, o, &len);

		if (depth > FDT_MAX_DEPTH) {
			printf("max depth exceeded: %d\n", depth);
			continue;
		}

		prevname[depth] = name;

		/* Skip root (i = 1) when printing devices */
		for (i = 1; i <= depth; i++) {
			if (prevname[i] == NULL)
				break;

			if (strcmp(cwd, "/") == 0)
				printf("/");
			printf("%s", prevname[i]);
		}
		printf("\n");
	}

	return (CMD_OK);
}

static __inline int
isprint(int c)
{

	return (c >= ' ' && c <= 0x7e);
}

static int
fdt_isprint(const void *data, int len, int *count)
{
	const char *d;
	char ch;
	int yesno, i;

	if (len == 0)
		return (0);

	d = (const char *)data;
	if (d[len - 1] != '\0')
		return (0);

	*count = 0;
	yesno = 1;
	for (i = 0; i < len; i++) {
		ch = *(d + i);
		if (isprint(ch) || (ch == '\0' && i > 0)) {
			/* Count strings */
			if (ch == '\0')
				(*count)++;
			continue;
		}

		yesno = 0;
		break;
	}

	return (yesno);
}

static int
fdt_data_str(const void *data, int len, int count, char **buf)
{
	char *b, *tmp;
	const char *d;
	int buf_len, i, l;

	/*
	 * Calculate the length for the string and allocate memory.
	 *
	 * Note that 'len' already includes at least one terminator.
	 */
	buf_len = len;
	if (count > 1) {
		/*
		 * Each token had already a terminator buried in 'len', but we
		 * only need one eventually, don't count space for these.
		 */
		buf_len -= count - 1;

		/* Each consecutive token requires a ", " separator. */
		buf_len += count * 2;
	}

	/* Add some space for surrounding double quotes. */
	buf_len += count * 2;

	/* Note that string being put in 'tmp' may be as big as 'buf_len'. */
	b = (char *)malloc(buf_len);
	tmp = (char *)malloc(buf_len);
	if (b == NULL)
		goto error;

	if (tmp == NULL) {
		free(b);
		goto error;
	}

	b[0] = '\0';

	/*
	 * Now that we have space, format the string.
	 */
	i = 0;
	do {
		d = (const char *)data + i;
		l = strlen(d) + 1;

		sprintf(tmp, "\"%s\"%s", d,
		    (i + l) < len ?  ", " : "");
		strcat(b, tmp);

		i += l;

	} while (i < len);
	*buf = b;

	free(tmp);

	return (0);
error:
	return (1);
}

static int
fdt_data_cell(const void *data, int len, char **buf)
{
	char *b, *tmp;
	const uint32_t *c;
	int count, i, l;

	/* Number of cells */
	count = len / 4;

	/*
	 * Calculate the length for the string and allocate memory.
	 */

	/* Each byte translates to 2 output characters */
	l = len * 2;
	if (count > 1) {
		/* Each consecutive cell requires a " " separator. */
		l += (count - 1) * 1;
	}
	/* Each cell will have a "0x" prefix */
	l += count * 2;
	/* Space for surrounding <> and terminator */
	l += 3;

	b = (char *)malloc(l);
	tmp = (char *)malloc(l);
	if (b == NULL)
		goto error;

	if (tmp == NULL) {
		free(b);
		goto error;
	}

	b[0] = '\0';
	strcat(b, "<");

	for (i = 0; i < len; i += 4) {
		c = (const uint32_t *)((const uint8_t *)data + i);
		sprintf(tmp, "0x%08x%s", fdt32_to_cpu(*c),
		    i < (len - 4) ? " " : "");
		strcat(b, tmp);
	}
	strcat(b, ">");
	*buf = b;

	free(tmp);

	return (0);
error:
	return (1);
}

static int
fdt_data_bytes(const void *data, int len, char **buf)
{
	char *b, *tmp;
	const char *d;
	int i, l;

	/*
	 * Calculate the length for the string and allocate memory.
	 */

	/* Each byte translates to 2 output characters */
	l = len * 2;
	if (len > 1)
		/* Each consecutive byte requires a " " separator. */
		l += (len - 1) * 1;
	/* Each byte will have a "0x" prefix */
	l += len * 2;
	/* Space for surrounding [] and terminator. */
	l += 3;

	b = (char *)malloc(l);
	tmp = (char *)malloc(l);
	if (b == NULL)
		goto error;

	if (tmp == NULL) {
		free(b);
		goto error;
	}

	b[0] = '\0';
	strcat(b, "[");

	for (i = 0, d = data; i < len; i++) {
		sprintf(tmp, "0x%02x%s", d[i], i < len - 1 ? " " : "");
		strcat(b, tmp);
	}
	strcat(b, "]");
	*buf = b;

	free(tmp);

	return (0);
error:
	return (1);
}

static int
fdt_data_fmt(const void *data, int len, char **buf)
{
	int count;

	if (len == 0) {
		*buf = NULL;
		return (1);
	}

	if (fdt_isprint(data, len, &count))
		return (fdt_data_str(data, len, count, buf));

	else if ((len % 4) == 0)
		return (fdt_data_cell(data, len, buf));

	else
		return (fdt_data_bytes(data, len, buf));
}

static int
fdt_prop(int offset)
{
	char *line, *buf;
	const struct fdt_property *prop;
	const char *name;
	const void *data;
	int len, rv;

	line = NULL;
	prop = fdt_offset_ptr(fdtp, offset, sizeof(*prop));
	if (prop == NULL)
		return (1);

	name = fdt_string(fdtp, fdt32_to_cpu(prop->nameoff));
	len = fdt32_to_cpu(prop->len);

	rv = 0;
	buf = NULL;
	if (len == 0) {
		/* Property without value */
		line = (char *)malloc(strlen(name) + 2);
		if (line == NULL) {
			rv = 2;
			goto out2;
		}
		sprintf(line, "%s\n", name);
		goto out1;
	}

	/*
	 * Process property with value
	 */
	data = prop->data;

	if (fdt_data_fmt(data, len, &buf) != 0) {
		rv = 3;
		goto out2;
	}

	line = (char *)malloc(strlen(name) + strlen(FDT_PROP_SEP) +
	    strlen(buf) + 2);
	if (line == NULL) {
		sprintf(command_errbuf, "could not allocate space for string");
		rv = 4;
		goto out2;
	}

	sprintf(line, "%s" FDT_PROP_SEP "%s\n", name, buf);

out1:
	pager_open();
	pager_output(line);
	pager_close();

out2:
	if (buf)
		free(buf);

	if (line)
		free(line);

	return (rv);
}

static int
fdt_modprop(int nodeoff, char *propname, void *value, char mode)
{
	uint32_t cells[100];
	char *buf;
	int len, rv;
	const struct fdt_property *p;

	p = fdt_get_property(fdtp, nodeoff, propname, NULL);

	if (p != NULL) {
		if (mode == 1) {
			 /* Adding inexistant value in mode 1 is forbidden */
			sprintf(command_errbuf, "property already exists!");
			return (CMD_ERROR);
		}
	} else if (mode == 0) {
		sprintf(command_errbuf, "property does not exist!");
		return (CMD_ERROR);
	}
	len = strlen(value);
	rv = 0;
	buf = (char *)value;

	switch (*buf) {
	case '&':
		/* phandles */
		break;
	case '<':
		/* Data cells */
		len = fdt_strtovect(buf, (void *)&cells, 100,
		    sizeof(uint32_t));

		rv = fdt_setprop(fdtp, nodeoff, propname, &cells,
		    len * sizeof(uint32_t));
		break;
	case '[':
		/* Data bytes */
		len = fdt_strtovect(buf, (void *)&cells, 100,
		    sizeof(uint8_t));

		rv = fdt_setprop(fdtp, nodeoff, propname, &cells,
		    len * sizeof(uint8_t));
		break;
	case '"':
	default:
		/* Default -- string */
		rv = fdt_setprop_string(fdtp, nodeoff, propname, value);
		break;
	}

	if (rv != 0) {
		if (rv == -FDT_ERR_NOSPACE)
			sprintf(command_errbuf,
			    "Device tree blob is too small!\n");
		else
			sprintf(command_errbuf,
			    "Could not add/modify property!\n");
	}
	return (rv);
}

/* Merge strings from argv into a single string */
static int
fdt_merge_strings(int argc, char *argv[], int start, char **buffer)
{
	char *buf;
	int i, idx, sz;

	*buffer = NULL;
	sz = 0;

	for (i = start; i < argc; i++)
		sz += strlen(argv[i]);

	/* Additional bytes for whitespaces between args */
	sz += argc - start;

	buf = (char *)malloc(sizeof(char) * sz);
	bzero(buf, sizeof(char) * sz);

	if (buf == NULL) {
		sprintf(command_errbuf, "could not allocate space "
		    "for string");
		return (1);
	}

	idx = 0;
	for (i = start, idx = 0; i < argc; i++) {
		strcpy(buf + idx, argv[i]);
		idx += strlen(argv[i]);
		buf[idx] = ' ';
		idx++;
	}
	buf[sz - 1] = '\0';
	*buffer = buf;
	return (0);
}

/* Extract offset and name of node/property from a given path */
static int
fdt_extract_nameloc(char **pathp, char **namep, int *nodeoff)
{
	int o;
	char *path = *pathp, *name = NULL, *subpath = NULL;

	subpath = strrchr(path, '/');
	if (subpath == NULL) {
		o = fdt_path_offset(fdtp, cwd);
		name = path;
		path = (char *)&cwd;
	} else {
		*subpath = '\0';
		if (strlen(path) == 0)
			path = cwd;

		name = subpath + 1;
		o = fdt_path_offset(fdtp, path);
	}

	if (strlen(name) == 0) {
		sprintf(command_errbuf, "name not specified");
		return (1);
	}
	if (o < 0) {
		sprintf(command_errbuf, "could not find node: '%s'", path);
		return (1);
	}
	*namep = name;
	*nodeoff = o;
	*pathp = path;
	return (0);
}

static int
fdt_cmd_prop(int argc, char *argv[])
{
	char *path, *propname, *value;
	int o, next, depth, rv;
	uint32_t tag;

	path = (argc > 2) ? argv[2] : NULL;

	value = NULL;

	if (argc > 3) {
		/* Merge property value strings into one */
		if (fdt_merge_strings(argc, argv, 3, &value) != 0)
			return (CMD_ERROR);
	} else
		value = NULL;

	if (path == NULL)
		path = cwd;

	rv = CMD_OK;

	if (value) {
		/* If value is specified -- try to modify prop. */
		if (fdt_extract_nameloc(&path, &propname, &o) != 0)
			return (CMD_ERROR);

		rv = fdt_modprop(o, propname, value, 0);
		if (rv)
			return (CMD_ERROR);
		return (CMD_OK);

	}
	/* User wants to display properties */
	o = fdt_path_offset(fdtp, path);

	if (o < 0) {
		sprintf(command_errbuf, "could not find node: '%s'", path);
		rv = CMD_ERROR;
		goto out;
	}

	depth = 0;
	while (depth >= 0) {
		tag = fdt_next_tag(fdtp, o, &next);
		switch (tag) {
		case FDT_NOP:
			break;
		case FDT_PROP:
			if (depth > 1)
				/* Don't process properties of nested nodes */
				break;

			if (fdt_prop(o) != 0) {
				sprintf(command_errbuf, "could not process "
				    "property");
				rv = CMD_ERROR;
				goto out;
			}
			break;
		case FDT_BEGIN_NODE:
			depth++;
			if (depth > FDT_MAX_DEPTH) {
				printf("warning: nesting too deep: %d\n",
				    depth);
				goto out;
			}
			break;
		case FDT_END_NODE:
			depth--;
			if (depth == 0)
				/*
				 * This is the end of our starting node, force
				 * the loop finish.
				 */
				depth--;
			break;
		}
		o = next;
	}
out:
	return (rv);
}

static int
fdt_cmd_mkprop(int argc, char *argv[])
{
	int o;
	char *path, *propname, *value;

	path = (argc > 2) ? argv[2] : NULL;

	value = NULL;

	if (argc > 3) {
		/* Merge property value strings into one */
		if (fdt_merge_strings(argc, argv, 3, &value) != 0)
			return (CMD_ERROR);
	} else
		value = NULL;

	if (fdt_extract_nameloc(&path, &propname, &o) != 0)
		return (CMD_ERROR);

	if (fdt_modprop(o, propname, value, 1))
		return (CMD_ERROR);

	return (CMD_OK);
}

static int
fdt_cmd_rm(int argc, char *argv[])
{
	int o, rv;
	char *path = NULL, *propname;

	if (argc > 2)
		path = argv[2];
	else {
		sprintf(command_errbuf, "no node/property name specified");
		return (CMD_ERROR);
	}

	o = fdt_path_offset(fdtp, path);
	if (o < 0) {
		/* If node not found -- try to find & delete property */
		if (fdt_extract_nameloc(&path, &propname, &o) != 0)
			return (CMD_ERROR);

		if ((rv = fdt_delprop(fdtp, o, propname)) != 0) {
			sprintf(command_errbuf, "could not delete"
			    "%s\n", (rv == -FDT_ERR_NOTFOUND) ?
			    "(property/node does not exist)" : "");
			return (CMD_ERROR);

		} else
			return (CMD_OK);
	}
	/* If node exists -- remove node */
	rv = fdt_del_node(fdtp, o);
	if (rv) {
		sprintf(command_errbuf, "could not delete node");
		return (CMD_ERROR);
	}
	return (CMD_OK);
}

static int
fdt_cmd_mknode(int argc, char *argv[])
{
	int o, rv;
	char *path = NULL, *nodename = NULL;

	if (argc > 2)
		path = argv[2];
	else {
		sprintf(command_errbuf, "no node name specified");
		return (CMD_ERROR);
	}

	if (fdt_extract_nameloc(&path, &nodename, &o) != 0)
		return (CMD_ERROR);

	rv = fdt_add_subnode(fdtp, o, nodename);

	if (rv < 0) {
		if (rv == -FDT_ERR_NOSPACE)
			sprintf(command_errbuf,
			    "Device tree blob is too small!\n");
		else
			sprintf(command_errbuf,
			    "Could not add node!\n");
		return (CMD_ERROR);
	}
	return (CMD_OK);
}

static int
fdt_cmd_pwd(int argc, char *argv[])
{
	char line[FDT_CWD_LEN];

	pager_open();
	sprintf(line, "%s\n", cwd);
	pager_output(line);
	pager_close();
	return (CMD_OK);
}

static int
fdt_cmd_nyi(int argc, char *argv[])
{

	printf("command not yet implemented\n");
	return (CMD_ERROR);
}
