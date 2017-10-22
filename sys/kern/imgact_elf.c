/*-
 * Copyright (c) 2000 David O'Brien
 * Copyright (c) 1995-1996 S�ren Schmidt
 * Copyright (c) 1996 Peter Wemm
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
__FBSDID("$FreeBSD$");

#include "opt_capsicum.h"
#include "opt_compat.h"
#include "opt_core.h"

#include <sys/param.h>
#include <sys/capability.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/mman.h>
#include <sys/namei.h>
#include <sys/pioctl.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/sf_buf.h>
#include <sys/smp.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/vnode.h>
#include <sys/syslog.h>
#include <sys/eventhandler.h>

#include <net/zlib.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

#include <machine/elf.h>
#include <machine/md_var.h>

#define OLD_EI_BRAND	8

static int __elfN(check_header)(const Elf_Ehdr *hdr);
static Elf_Brandinfo *__elfN(get_brandinfo)(struct image_params *imgp,
    const char *interp, int interp_name_len, int32_t *osrel);
static int __elfN(load_file)(struct proc *p, const char *file, u_long *addr,
    u_long *entry, size_t pagesize);
static int __elfN(load_section)(struct vmspace *vmspace, vm_object_t object,
    vm_offset_t offset, caddr_t vmaddr, size_t memsz, size_t filsz,
    vm_prot_t prot, size_t pagesize);
static int __CONCAT(exec_, __elfN(imgact))(struct image_params *imgp);
static boolean_t __elfN(freebsd_trans_osrel)(const Elf_Note *note,
    int32_t *osrel);
static boolean_t kfreebsd_trans_osrel(const Elf_Note *note, int32_t *osrel);
static boolean_t __elfN(check_note)(struct image_params *imgp,
    Elf_Brandnote *checknote, int32_t *osrel);
static vm_prot_t __elfN(trans_prot)(Elf_Word);
static Elf_Word __elfN(untrans_prot)(vm_prot_t);

SYSCTL_NODE(_kern, OID_AUTO, __CONCAT(elf, __ELF_WORD_SIZE), CTLFLAG_RW, 0,
    "");

#ifdef COMPRESS_USER_CORES
static int compress_core(gzFile, char *, char *, unsigned int,
    struct thread * td);
#define CORE_BUF_SIZE	(16 * 1024)
#endif

int __elfN(fallback_brand) = -1;
SYSCTL_INT(__CONCAT(_kern_elf, __ELF_WORD_SIZE), OID_AUTO,
    fallback_brand, CTLFLAG_RW, &__elfN(fallback_brand), 0,
    __XSTRING(__CONCAT(ELF, __ELF_WORD_SIZE)) " brand of last resort");
TUNABLE_INT("kern.elf" __XSTRING(__ELF_WORD_SIZE) ".fallback_brand",
    &__elfN(fallback_brand));

static int elf_legacy_coredump = 0;
SYSCTL_INT(_debug, OID_AUTO, __elfN(legacy_coredump), CTLFLAG_RW, 
    &elf_legacy_coredump, 0, "");

int __elfN(nxstack) =
#if defined(__amd64__) || defined(__powerpc64__) /* both 64 and 32 bit */
	1;
#else
	0;
#endif
SYSCTL_INT(__CONCAT(_kern_elf, __ELF_WORD_SIZE), OID_AUTO,
    nxstack, CTLFLAG_RW, &__elfN(nxstack), 0,
    __XSTRING(__CONCAT(ELF, __ELF_WORD_SIZE)) ": enable non-executable stack");

#if __ELF_WORD_SIZE == 32
#if defined(__amd64__) || defined(__ia64__)
int i386_read_exec = 0;
SYSCTL_INT(_kern_elf32, OID_AUTO, read_exec, CTLFLAG_RW, &i386_read_exec, 0,
    "enable execution from readable segments");
#endif
#endif

static Elf_Brandinfo *elf_brand_list[MAX_BRANDS];

#define	trunc_page_ps(va, ps)	((va) & ~(ps - 1))
#define	round_page_ps(va, ps)	(((va) + (ps - 1)) & ~(ps - 1))
#define	aligned(a, t)	(trunc_page_ps((u_long)(a), sizeof(t)) == (u_long)(a))

static const char FREEBSD_ABI_VENDOR[] = "FreeBSD";

Elf_Brandnote __elfN(freebsd_brandnote) = {
	.hdr.n_namesz	= sizeof(FREEBSD_ABI_VENDOR),
	.hdr.n_descsz	= sizeof(int32_t),
	.hdr.n_type	= 1,
	.vendor		= FREEBSD_ABI_VENDOR,
	.flags		= BN_TRANSLATE_OSREL,
	.trans_osrel	= __elfN(freebsd_trans_osrel)
};

static boolean_t
__elfN(freebsd_trans_osrel)(const Elf_Note *note, int32_t *osrel)
{
	uintptr_t p;

	p = (uintptr_t)(note + 1);
	p += roundup2(note->n_namesz, sizeof(Elf32_Addr));
	*osrel = *(const int32_t *)(p);

	return (TRUE);
}

static const char GNU_ABI_VENDOR[] = "GNU";
static int GNU_KFREEBSD_ABI_DESC = 3;

Elf_Brandnote __elfN(kfreebsd_brandnote) = {
	.hdr.n_namesz	= sizeof(GNU_ABI_VENDOR),
	.hdr.n_descsz	= 16,	/* XXX at least 16 */
	.hdr.n_type	= 1,
	.vendor		= GNU_ABI_VENDOR,
	.flags		= BN_TRANSLATE_OSREL,
	.trans_osrel	= kfreebsd_trans_osrel
};

static boolean_t
kfreebsd_trans_osrel(const Elf_Note *note, int32_t *osrel)
{
	const Elf32_Word *desc;
	uintptr_t p;

	p = (uintptr_t)(note + 1);
	p += roundup2(note->n_namesz, sizeof(Elf32_Addr));

	desc = (const Elf32_Word *)p;
	if (desc[0] != GNU_KFREEBSD_ABI_DESC)
		return (FALSE);

	/*
	 * Debian GNU/kFreeBSD embed the earliest compatible kernel version
	 * (__FreeBSD_version: <major><two digit minor>Rxx) in the LSB way.
	 */
	*osrel = desc[1] * 100000 + desc[2] * 1000 + desc[3];

	return (TRUE);
}

int
__elfN(insert_brand_entry)(Elf_Brandinfo *entry)
{
	int i;

	for (i = 0; i < MAX_BRANDS; i++) {
		if (elf_brand_list[i] == NULL) {
			elf_brand_list[i] = entry;
			break;
		}
	}
	if (i == MAX_BRANDS) {
		printf("WARNING: %s: could not insert brandinfo entry: %p\n",
			__func__, entry);
		return (-1);
	}
	return (0);
}

int
__elfN(remove_brand_entry)(Elf_Brandinfo *entry)
{
	int i;

	for (i = 0; i < MAX_BRANDS; i++) {
		if (elf_brand_list[i] == entry) {
			elf_brand_list[i] = NULL;
			break;
		}
	}
	if (i == MAX_BRANDS)
		return (-1);
	return (0);
}

int
__elfN(brand_inuse)(Elf_Brandinfo *entry)
{
	struct proc *p;
	int rval = FALSE;

	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		if (p->p_sysent == entry->sysvec) {
			rval = TRUE;
			break;
		}
	}
	sx_sunlock(&allproc_lock);

	return (rval);
}

static Elf_Brandinfo *
__elfN(get_brandinfo)(struct image_params *imgp, const char *interp,
    int interp_name_len, int32_t *osrel)
{
	const Elf_Ehdr *hdr = (const Elf_Ehdr *)imgp->image_header;
	Elf_Brandinfo *bi;
	boolean_t ret;
	int i;

	/*
	 * We support four types of branding -- (1) the ELF EI_OSABI field
	 * that SCO added to the ELF spec, (2) FreeBSD 3.x's traditional string
	 * branding w/in the ELF header, (3) path of the `interp_path'
	 * field, and (4) the ".note.ABI-tag" ELF section.
	 */

	/* Look for an ".note.ABI-tag" ELF section */
	for (i = 0; i < MAX_BRANDS; i++) {
		bi = elf_brand_list[i];
		if (bi == NULL)
			continue;
		if (hdr->e_machine == bi->machine && (bi->flags &
		    (BI_BRAND_NOTE|BI_BRAND_NOTE_MANDATORY)) != 0) {
			ret = __elfN(check_note)(imgp, bi->brand_note, osrel);
			if (ret)
				return (bi);
		}
	}

	/* If the executable has a brand, search for it in the brand list. */
	for (i = 0; i < MAX_BRANDS; i++) {
		bi = elf_brand_list[i];
		if (bi == NULL || bi->flags & BI_BRAND_NOTE_MANDATORY)
			continue;
		if (hdr->e_machine == bi->machine &&
		    (hdr->e_ident[EI_OSABI] == bi->brand ||
		    strncmp((const char *)&hdr->e_ident[OLD_EI_BRAND],
		    bi->compat_3_brand, strlen(bi->compat_3_brand)) == 0))
			return (bi);
	}

	/* Lacking a known brand, search for a recognized interpreter. */
	if (interp != NULL) {
		for (i = 0; i < MAX_BRANDS; i++) {
			bi = elf_brand_list[i];
			if (bi == NULL || bi->flags & BI_BRAND_NOTE_MANDATORY)
				continue;
			if (hdr->e_machine == bi->machine &&
			    /* ELF image p_filesz includes terminating zero */
			    strlen(bi->interp_path) + 1 == interp_name_len &&
			    strncmp(interp, bi->interp_path, interp_name_len)
			    == 0)
				return (bi);
		}
	}

	/* Lacking a recognized interpreter, try the default brand */
	for (i = 0; i < MAX_BRANDS; i++) {
		bi = elf_brand_list[i];
		if (bi == NULL || bi->flags & BI_BRAND_NOTE_MANDATORY)
			continue;
		if (hdr->e_machine == bi->machine &&
		    __elfN(fallback_brand) == bi->brand)
			return (bi);
	}
	return (NULL);
}

static int
__elfN(check_header)(const Elf_Ehdr *hdr)
{
	Elf_Brandinfo *bi;
	int i;

	if (!IS_ELF(*hdr) ||
	    hdr->e_ident[EI_CLASS] != ELF_TARG_CLASS ||
	    hdr->e_ident[EI_DATA] != ELF_TARG_DATA ||
	    hdr->e_ident[EI_VERSION] != EV_CURRENT ||
	    hdr->e_phentsize != sizeof(Elf_Phdr) ||
	    hdr->e_version != ELF_TARG_VER)
		return (ENOEXEC);

	/*
	 * Make sure we have at least one brand for this machine.
	 */

	for (i = 0; i < MAX_BRANDS; i++) {
		bi = elf_brand_list[i];
		if (bi != NULL && bi->machine == hdr->e_machine)
			break;
	}
	if (i == MAX_BRANDS)
		return (ENOEXEC);

	return (0);
}

static int
__elfN(map_partial)(vm_map_t map, vm_object_t object, vm_ooffset_t offset,
    vm_offset_t start, vm_offset_t end, vm_prot_t prot)
{
	struct sf_buf *sf;
	int error;
	vm_offset_t off;

	/*
	 * Create the page if it doesn't exist yet. Ignore errors.
	 */
	vm_map_lock(map);
	vm_map_insert(map, NULL, 0, trunc_page(start), round_page(end),
	    VM_PROT_ALL, VM_PROT_ALL, 0);
	vm_map_unlock(map);

	/*
	 * Find the page from the underlying object.
	 */
	if (object) {
		sf = vm_imgact_map_page(object, offset);
		if (sf == NULL)
			return (KERN_FAILURE);
		off = offset - trunc_page(offset);
		error = copyout((caddr_t)sf_buf_kva(sf) + off, (caddr_t)start,
		    end - start);
		vm_imgact_unmap_page(sf);
		if (error) {
			return (KERN_FAILURE);
		}
	}

	return (KERN_SUCCESS);
}

static int
__elfN(map_insert)(vm_map_t map, vm_object_t object, vm_ooffset_t offset,
    vm_offset_t start, vm_offset_t end, vm_prot_t prot, int cow)
{
	struct sf_buf *sf;
	vm_offset_t off;
	vm_size_t sz;
	int error, rv;

	if (start != trunc_page(start)) {
		rv = __elfN(map_partial)(map, object, offset, start,
		    round_page(start), prot);
		if (rv)
			return (rv);
		offset += round_page(start) - start;
		start = round_page(start);
	}
	if (end != round_page(end)) {
		rv = __elfN(map_partial)(map, object, offset +
		    trunc_page(end) - start, trunc_page(end), end, prot);
		if (rv)
			return (rv);
		end = trunc_page(end);
	}
	if (end > start) {
		if (offset & PAGE_MASK) {
			/*
			 * The mapping is not page aligned. This means we have
			 * to copy the data. Sigh.
			 */
			rv = vm_map_find(map, NULL, 0, &start, end - start,
			    FALSE, prot | VM_PROT_WRITE, VM_PROT_ALL, 0);
			if (rv)
				return (rv);
			if (object == NULL)
				return (KERN_SUCCESS);
			for (; start < end; start += sz) {
				sf = vm_imgact_map_page(object, offset);
				if (sf == NULL)
					return (KERN_FAILURE);
				off = offset - trunc_page(offset);
				sz = end - start;
				if (sz > PAGE_SIZE - off)
					sz = PAGE_SIZE - off;
				error = copyout((caddr_t)sf_buf_kva(sf) + off,
				    (caddr_t)start, sz);
				vm_imgact_unmap_page(sf);
				if (error) {
					return (KERN_FAILURE);
				}
				offset += sz;
			}
			rv = KERN_SUCCESS;
		} else {
			vm_object_reference(object);
			vm_map_lock(map);
			rv = vm_map_insert(map, object, offset, start, end,
			    prot, VM_PROT_ALL, cow);
			vm_map_unlock(map);
			if (rv != KERN_SUCCESS)
				vm_object_deallocate(object);
		}
		return (rv);
	} else {
		return (KERN_SUCCESS);
	}
}

static int
__elfN(load_section)(struct vmspace *vmspace,
	vm_object_t object, vm_offset_t offset,
	caddr_t vmaddr, size_t memsz, size_t filsz, vm_prot_t prot,
	size_t pagesize)
{
	struct sf_buf *sf;
	size_t map_len;
	vm_offset_t map_addr;
	int error, rv, cow;
	size_t copy_len;
	vm_offset_t file_addr;

	/*
	 * It's necessary to fail if the filsz + offset taken from the
	 * header is greater than the actual file pager object's size.
	 * If we were to allow this, then the vm_map_find() below would
	 * walk right off the end of the file object and into the ether.
	 *
	 * While I'm here, might as well check for something else that
	 * is invalid: filsz cannot be greater than memsz.
	 */
	if ((off_t)filsz + offset > object->un_pager.vnp.vnp_size ||
	    filsz > memsz) {
		uprintf("elf_load_section: truncated ELF file\n");
		return (ENOEXEC);
	}

	map_addr = trunc_page_ps((vm_offset_t)vmaddr, pagesize);
	file_addr = trunc_page_ps(offset, pagesize);

	/*
	 * We have two choices.  We can either clear the data in the last page
	 * of an oversized mapping, or we can start the anon mapping a page
	 * early and copy the initialized data into that first page.  We
	 * choose the second..
	 */
	if (memsz > filsz)
		map_len = trunc_page_ps(offset + filsz, pagesize) - file_addr;
	else
		map_len = round_page_ps(offset + filsz, pagesize) - file_addr;

	if (map_len != 0) {
		/* cow flags: don't dump readonly sections in core */
		cow = MAP_COPY_ON_WRITE | MAP_PREFAULT |
		    (prot & VM_PROT_WRITE ? 0 : MAP_DISABLE_COREDUMP);

		rv = __elfN(map_insert)(&vmspace->vm_map,
				      object,
				      file_addr,	/* file offset */
				      map_addr,		/* virtual start */
				      map_addr + map_len,/* virtual end */
				      prot,
				      cow);
		if (rv != KERN_SUCCESS)
			return (EINVAL);

		/* we can stop now if we've covered it all */
		if (memsz == filsz) {
			return (0);
		}
	}


	/*
	 * We have to get the remaining bit of the file into the first part
	 * of the oversized map segment.  This is normally because the .data
	 * segment in the file is extended to provide bss.  It's a neat idea
	 * to try and save a page, but it's a pain in the behind to implement.
	 */
	copy_len = (offset + filsz) - trunc_page_ps(offset + filsz, pagesize);
	map_addr = trunc_page_ps((vm_offset_t)vmaddr + filsz, pagesize);
	map_len = round_page_ps((vm_offset_t)vmaddr + memsz, pagesize) -
	    map_addr;

	/* This had damn well better be true! */
	if (map_len != 0) {
		rv = __elfN(map_insert)(&vmspace->vm_map, NULL, 0, map_addr,
		    map_addr + map_len, VM_PROT_ALL, 0);
		if (rv != KERN_SUCCESS) {
			return (EINVAL);
		}
	}

	if (copy_len != 0) {
		vm_offset_t off;

		sf = vm_imgact_map_page(object, offset + filsz);
		if (sf == NULL)
			return (EIO);

		/* send the page fragment to user space */
		off = trunc_page_ps(offset + filsz, pagesize) -
		    trunc_page(offset + filsz);
		error = copyout((caddr_t)sf_buf_kva(sf) + off,
		    (caddr_t)map_addr, copy_len);
		vm_imgact_unmap_page(sf);
		if (error) {
			return (error);
		}
	}

	/*
	 * set it to the specified protection.
	 * XXX had better undo the damage from pasting over the cracks here!
	 */
	vm_map_protect(&vmspace->vm_map, trunc_page(map_addr),
	    round_page(map_addr + map_len),  prot, FALSE);

	return (0);
}

/*
 * Load the file "file" into memory.  It may be either a shared object
 * or an executable.
 *
 * The "addr" reference parameter is in/out.  On entry, it specifies
 * the address where a shared object should be loaded.  If the file is
 * an executable, this value is ignored.  On exit, "addr" specifies
 * where the file was actually loaded.
 *
 * The "entry" reference parameter is out only.  On exit, it specifies
 * the entry point for the loaded file.
 */
static int
__elfN(load_file)(struct proc *p, const char *file, u_long *addr,
	u_long *entry, size_t pagesize)
{
	struct {
		struct nameidata nd;
		struct vattr attr;
		struct image_params image_params;
	} *tempdata;
	const Elf_Ehdr *hdr = NULL;
	const Elf_Phdr *phdr = NULL;
	struct nameidata *nd;
	struct vmspace *vmspace = p->p_vmspace;
	struct vattr *attr;
	struct image_params *imgp;
	vm_prot_t prot;
	u_long rbase;
	u_long base_addr = 0;
	int vfslocked, error, i, numsegs;

#ifdef CAPABILITY_MODE
	/*
	 * XXXJA: This check can go away once we are sufficiently confident
	 * that the checks in namei() are correct.
	 */
	if (IN_CAPABILITY_MODE(curthread))
		return (ECAPMODE);
#endif

	tempdata = malloc(sizeof(*tempdata), M_TEMP, M_WAITOK);
	nd = &tempdata->nd;
	attr = &tempdata->attr;
	imgp = &tempdata->image_params;

	/*
	 * Initialize part of the common data
	 */
	imgp->proc = p;
	imgp->attr = attr;
	imgp->firstpage = NULL;
	imgp->image_header = NULL;
	imgp->object = NULL;
	imgp->execlabel = NULL;

	NDINIT(nd, LOOKUP, MPSAFE|LOCKLEAF|FOLLOW, UIO_SYSSPACE, file,
	    curthread);
	vfslocked = 0;
	if ((error = namei(nd)) != 0) {
		nd->ni_vp = NULL;
		goto fail;
	}
	vfslocked = NDHASGIANT(nd);
	NDFREE(nd, NDF_ONLY_PNBUF);
	imgp->vp = nd->ni_vp;

	/*
	 * Check permissions, modes, uid, etc on the file, and "open" it.
	 */
	error = exec_check_permissions(imgp);
	if (error)
		goto fail;

	error = exec_map_first_page(imgp);
	if (error)
		goto fail;

	/*
	 * Also make certain that the interpreter stays the same, so set
	 * its VV_TEXT flag, too.
	 */
	nd->ni_vp->v_vflag |= VV_TEXT;

	imgp->object = nd->ni_vp->v_object;

	hdr = (const Elf_Ehdr *)imgp->image_header;
	if ((error = __elfN(check_header)(hdr)) != 0)
		goto fail;
	if (hdr->e_type == ET_DYN)
		rbase = *addr;
	else if (hdr->e_type == ET_EXEC)
		rbase = 0;
	else {
		error = ENOEXEC;
		goto fail;
	}

	/* Only support headers that fit within first page for now      */
	/*    (multiplication of two Elf_Half fields will not overflow) */
	if ((hdr->e_phoff > PAGE_SIZE) ||
	    (hdr->e_phentsize * hdr->e_phnum) > PAGE_SIZE - hdr->e_phoff) {
		error = ENOEXEC;
		goto fail;
	}

	phdr = (const Elf_Phdr *)(imgp->image_header + hdr->e_phoff);
	if (!aligned(phdr, Elf_Addr)) {
		error = ENOEXEC;
		goto fail;
	}

	for (i = 0, numsegs = 0; i < hdr->e_phnum; i++) {
		if (phdr[i].p_type == PT_LOAD && phdr[i].p_memsz != 0) {
			/* Loadable segment */
			prot = __elfN(trans_prot)(phdr[i].p_flags);
			if ((error = __elfN(load_section)(vmspace,
			    imgp->object, phdr[i].p_offset,
			    (caddr_t)(uintptr_t)phdr[i].p_vaddr + rbase,
			    phdr[i].p_memsz, phdr[i].p_filesz, prot,
			    pagesize)) != 0)
				goto fail;
			/*
			 * Establish the base address if this is the
			 * first segment.
			 */
			if (numsegs == 0)
  				base_addr = trunc_page(phdr[i].p_vaddr +
				    rbase);
			numsegs++;
		}
	}
	*addr = base_addr;
	*entry = (unsigned long)hdr->e_entry + rbase;

fail:
	if (imgp->firstpage)
		exec_unmap_first_page(imgp);

	if (nd->ni_vp)
		vput(nd->ni_vp);

	VFS_UNLOCK_GIANT(vfslocked);
	free(tempdata, M_TEMP);

	return (error);
}

static int
__CONCAT(exec_, __elfN(imgact))(struct image_params *imgp)
{
	const Elf_Ehdr *hdr = (const Elf_Ehdr *)imgp->image_header;
	const Elf_Phdr *phdr;
	Elf_Auxargs *elf_auxargs;
	struct vmspace *vmspace;
	vm_prot_t prot;
	u_long text_size = 0, data_size = 0, total_size = 0;
	u_long text_addr = 0, data_addr = 0;
	u_long seg_size, seg_addr;
	u_long addr, baddr, et_dyn_addr, entry = 0, proghdr = 0;
	int32_t osrel = 0;
	int error = 0, i, n, interp_name_len = 0;
	const char *interp = NULL, *newinterp = NULL;
	Elf_Brandinfo *brand_info;
	char *path;
	struct sysentvec *sv;

	/*
	 * Do we have a valid ELF header ?
	 *
	 * Only allow ET_EXEC & ET_DYN here, reject ET_DYN later
	 * if particular brand doesn't support it.
	 */
	if (__elfN(check_header)(hdr) != 0 ||
	    (hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN))
		return (-1);

	/*
	 * From here on down, we return an errno, not -1, as we've
	 * detected an ELF file.
	 */

	if ((hdr->e_phoff > PAGE_SIZE) ||
	    (hdr->e_phoff + hdr->e_phentsize * hdr->e_phnum) > PAGE_SIZE) {
		/* Only support headers in first page for now */
		return (ENOEXEC);
	}
	phdr = (const Elf_Phdr *)(imgp->image_header + hdr->e_phoff);
	if (!aligned(phdr, Elf_Addr))
		return (ENOEXEC);
	n = 0;
	baddr = 0;
	for (i = 0; i < hdr->e_phnum; i++) {
		switch (phdr[i].p_type) {
		case PT_LOAD:
			if (n == 0)
				baddr = phdr[i].p_vaddr;
			n++;
			break;
		case PT_INTERP:
			/* Path to interpreter */
			if (phdr[i].p_filesz > MAXPATHLEN ||
			    phdr[i].p_offset >= PAGE_SIZE ||
			    phdr[i].p_offset + phdr[i].p_filesz >= PAGE_SIZE)
				return (ENOEXEC);
			interp = imgp->image_header + phdr[i].p_offset;
			interp_name_len = phdr[i].p_filesz;
			break;
		case PT_GNU_STACK:
			if (__elfN(nxstack))
				imgp->stack_prot =
				    __elfN(trans_prot)(phdr[i].p_flags);
			break;
		}
	}

	brand_info = __elfN(get_brandinfo)(imgp, interp, interp_name_len,
	    &osrel);
	if (brand_info == NULL) {
		uprintf("ELF binary type \"%u\" not known.\n",
		    hdr->e_ident[EI_OSABI]);
		return (ENOEXEC);
	}
	if (hdr->e_type == ET_DYN) {
		if ((brand_info->flags & BI_CAN_EXEC_DYN) == 0)
			return (ENOEXEC);
		/*
		 * Honour the base load address from the dso if it is
		 * non-zero for some reason.
		 */
		if (baddr == 0)
			et_dyn_addr = ET_DYN_LOAD_ADDR;
		else
			et_dyn_addr = 0;
	} else
		et_dyn_addr = 0;
	sv = brand_info->sysvec;
	if (interp != NULL && brand_info->interp_newpath != NULL)
		newinterp = brand_info->interp_newpath;

	/*
	 * Avoid a possible deadlock if the current address space is destroyed
	 * and that address space maps the locked vnode.  In the common case,
	 * the locked vnode's v_usecount is decremented but remains greater
	 * than zero.  Consequently, the vnode lock is not needed by vrele().
	 * However, in cases where the vnode lock is external, such as nullfs,
	 * v_usecount may become zero.
	 */
	VOP_UNLOCK(imgp->vp, 0);

	error = exec_new_vmspace(imgp, sv);
	imgp->proc->p_sysent = sv;

	vn_lock(imgp->vp, LK_EXCLUSIVE | LK_RETRY);
	if (error)
		return (error);

	vmspace = imgp->proc->p_vmspace;

	for (i = 0; i < hdr->e_phnum; i++) {
		switch (phdr[i].p_type) {
		case PT_LOAD:	/* Loadable segment */
			if (phdr[i].p_memsz == 0)
				break;
			prot = __elfN(trans_prot)(phdr[i].p_flags);

#if defined(__ia64__) && __ELF_WORD_SIZE == 32 && defined(IA32_ME_HARDER)
			/*
			 * Some x86 binaries assume read == executable,
			 * notably the M3 runtime and therefore cvsup
			 */
			if (prot & VM_PROT_READ)
				prot |= VM_PROT_EXECUTE;
#endif

			if ((error = __elfN(load_section)(vmspace,
			    imgp->object, phdr[i].p_offset,
			    (caddr_t)(uintptr_t)phdr[i].p_vaddr + et_dyn_addr,
			    phdr[i].p_memsz, phdr[i].p_filesz, prot,
			    sv->sv_pagesize)) != 0)
				return (error);

			/*
			 * If this segment contains the program headers,
			 * remember their virtual address for the AT_PHDR
			 * aux entry. Static binaries don't usually include
			 * a PT_PHDR entry.
			 */
			if (phdr[i].p_offset == 0 &&
			    hdr->e_phoff + hdr->e_phnum * hdr->e_phentsize
				<= phdr[i].p_filesz)
				proghdr = phdr[i].p_vaddr + hdr->e_phoff +
				    et_dyn_addr;

			seg_addr = trunc_page(phdr[i].p_vaddr + et_dyn_addr);
			seg_size = round_page(phdr[i].p_memsz +
			    phdr[i].p_vaddr + et_dyn_addr - seg_addr);

			/*
			 * Make the largest executable segment the official
			 * text segment and all others data.
			 *
			 * Note that obreak() assumes that data_addr + 
			 * data_size == end of data load area, and the ELF
			 * file format expects segments to be sorted by
			 * address.  If multiple data segments exist, the
			 * last one will be used.
			 */

			if (phdr[i].p_flags & PF_X && text_size < seg_size) {
				text_size = seg_size;
				text_addr = seg_addr;
			} else {
				data_size = seg_size;
				data_addr = seg_addr;
			}
			total_size += seg_size;
			break;
		case PT_PHDR: 	/* Program header table info */
			proghdr = phdr[i].p_vaddr + et_dyn_addr;
			break;
		default:
			break;
		}
	}
	
	if (data_addr == 0 && data_size == 0) {
		data_addr = text_addr;
		data_size = text_size;
	}

	entry = (u_long)hdr->e_entry + et_dyn_addr;

	/*
	 * Check limits.  It should be safe to check the
	 * limits after loading the segments since we do
	 * not actually fault in all the segments pages.
	 */
	PROC_LOCK(imgp->proc);
	if (data_size > lim_cur(imgp->proc, RLIMIT_DATA) ||
	    text_size > maxtsiz ||
	    total_size > lim_cur(imgp->proc, RLIMIT_VMEM) ||
	    racct_set(imgp->proc, RACCT_DATA, data_size) != 0 ||
	    racct_set(imgp->proc, RACCT_VMEM, total_size) != 0) {
		PROC_UNLOCK(imgp->proc);
		return (ENOMEM);
	}

	vmspace->vm_tsize = text_size >> PAGE_SHIFT;
	vmspace->vm_taddr = (caddr_t)(uintptr_t)text_addr;
	vmspace->vm_dsize = data_size >> PAGE_SHIFT;
	vmspace->vm_daddr = (caddr_t)(uintptr_t)data_addr;

	/*
	 * We load the dynamic linker where a userland call
	 * to mmap(0, ...) would put it.  The rationale behind this
	 * calculation is that it leaves room for the heap to grow to
	 * its maximum allowed size.
	 */
	addr = round_page((vm_offset_t)imgp->proc->p_vmspace->vm_daddr +
	    lim_max(imgp->proc, RLIMIT_DATA));
	PROC_UNLOCK(imgp->proc);

	imgp->entry_addr = entry;

	if (interp != NULL) {
		int have_interp = FALSE;
		VOP_UNLOCK(imgp->vp, 0);
		if (brand_info->emul_path != NULL &&
		    brand_info->emul_path[0] != '\0') {
			path = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
			snprintf(path, MAXPATHLEN, "%s%s",
			    brand_info->emul_path, interp);
			error = __elfN(load_file)(imgp->proc, path, &addr,
			    &imgp->entry_addr, sv->sv_pagesize);
			free(path, M_TEMP);
			if (error == 0)
				have_interp = TRUE;
		}
		if (!have_interp && newinterp != NULL) {
			error = __elfN(load_file)(imgp->proc, newinterp, &addr,
			    &imgp->entry_addr, sv->sv_pagesize);
			if (error == 0)
				have_interp = TRUE;
		}
		if (!have_interp) {
			error = __elfN(load_file)(imgp->proc, interp, &addr,
			    &imgp->entry_addr, sv->sv_pagesize);
		}
		vn_lock(imgp->vp, LK_EXCLUSIVE | LK_RETRY);
		if (error != 0) {
			uprintf("ELF interpreter %s not found\n", interp);
			return (error);
		}
	} else
		addr = et_dyn_addr;

	/*
	 * Construct auxargs table (used by the fixup routine)
	 */
	elf_auxargs = malloc(sizeof(Elf_Auxargs), M_TEMP, M_WAITOK);
	elf_auxargs->execfd = -1;
	elf_auxargs->phdr = proghdr;
	elf_auxargs->phent = hdr->e_phentsize;
	elf_auxargs->phnum = hdr->e_phnum;
	elf_auxargs->pagesz = PAGE_SIZE;
	elf_auxargs->base = addr;
	elf_auxargs->flags = 0;
	elf_auxargs->entry = entry;

	imgp->auxargs = elf_auxargs;
	imgp->interpreted = 0;
	imgp->reloc_base = addr;
	imgp->proc->p_osrel = osrel;

	return (error);
}

#define	suword __CONCAT(suword, __ELF_WORD_SIZE)

int
__elfN(freebsd_fixup)(register_t **stack_base, struct image_params *imgp)
{
	Elf_Auxargs *args = (Elf_Auxargs *)imgp->auxargs;
	Elf_Addr *base;
	Elf_Addr *pos;

	base = (Elf_Addr *)*stack_base;
	pos = base + (imgp->args->argc + imgp->args->envc + 2);

	if (args->execfd != -1)
		AUXARGS_ENTRY(pos, AT_EXECFD, args->execfd);
	AUXARGS_ENTRY(pos, AT_PHDR, args->phdr);
	AUXARGS_ENTRY(pos, AT_PHENT, args->phent);
	AUXARGS_ENTRY(pos, AT_PHNUM, args->phnum);
	AUXARGS_ENTRY(pos, AT_PAGESZ, args->pagesz);
	AUXARGS_ENTRY(pos, AT_FLAGS, args->flags);
	AUXARGS_ENTRY(pos, AT_ENTRY, args->entry);
	AUXARGS_ENTRY(pos, AT_BASE, args->base);
	if (imgp->execpathp != 0)
		AUXARGS_ENTRY(pos, AT_EXECPATH, imgp->execpathp);
	AUXARGS_ENTRY(pos, AT_OSRELDATE, osreldate);
	if (imgp->canary != 0) {
		AUXARGS_ENTRY(pos, AT_CANARY, imgp->canary);
		AUXARGS_ENTRY(pos, AT_CANARYLEN, imgp->canarylen);
	}
	AUXARGS_ENTRY(pos, AT_NCPUS, mp_ncpus);
	if (imgp->pagesizes != 0) {
		AUXARGS_ENTRY(pos, AT_PAGESIZES, imgp->pagesizes);
		AUXARGS_ENTRY(pos, AT_PAGESIZESLEN, imgp->pagesizeslen);
	}
	AUXARGS_ENTRY(pos, AT_STACKPROT, imgp->sysent->sv_shared_page_obj
	    != NULL && imgp->stack_prot != 0 ? imgp->stack_prot :
	    imgp->sysent->sv_stackprot);
	AUXARGS_ENTRY(pos, AT_NULL, 0);

	free(imgp->auxargs, M_TEMP);
	imgp->auxargs = NULL;

	base--;
	suword(base, (long)imgp->args->argc);
	*stack_base = (register_t *)base;
	return (0);
}

/*
 * Code for generating ELF core dumps.
 */

typedef void (*segment_callback)(vm_map_entry_t, void *);

/* Closure for cb_put_phdr(). */
struct phdr_closure {
	Elf_Phdr *phdr;		/* Program header to fill in */
	Elf_Off offset;		/* Offset of segment in core file */
};

/* Closure for cb_size_segment(). */
struct sseg_closure {
	int count;		/* Count of writable segments. */
	size_t size;		/* Total size of all writable segments. */
};

static void cb_put_phdr(vm_map_entry_t, void *);
static void cb_size_segment(vm_map_entry_t, void *);
static void each_writable_segment(struct thread *, segment_callback, void *);
static int __elfN(corehdr)(struct thread *, struct vnode *, struct ucred *,
    int, void *, size_t, gzFile);
static void __elfN(puthdr)(struct thread *, void *, size_t *, int);
static void __elfN(putnote)(void *, size_t *, const char *, int,
    const void *, size_t);

#ifdef COMPRESS_USER_CORES
extern int compress_user_cores;
extern int compress_user_cores_gzlevel;
#endif

static int
core_output(struct vnode *vp, void *base, size_t len, off_t offset,
    struct ucred *active_cred, struct ucred *file_cred,
    struct thread *td, char *core_buf, gzFile gzfile) {

	int error;
	if (gzfile) {
#ifdef COMPRESS_USER_CORES
		error = compress_core(gzfile, base, core_buf, len, td);
#else
		panic("shouldn't be here");
#endif
	} else {
		error = vn_rdwr_inchunks(UIO_WRITE, vp, base, len, offset,
		    UIO_USERSPACE, IO_UNIT | IO_DIRECT, active_cred, file_cred,
		    NULL, td);
	}
	return (error);
}

int
__elfN(coredump)(struct thread *td, struct vnode *vp, off_t limit, int flags)
{
	struct ucred *cred = td->td_ucred;
	int error = 0;
	struct sseg_closure seginfo;
	void *hdr;
	size_t hdrsize;

	gzFile gzfile = Z_NULL;
	char *core_buf = NULL;
#ifdef COMPRESS_USER_CORES
	char gzopen_flags[8];
	char *p;
	int doing_compress = flags & IMGACT_CORE_COMPRESS;
#endif

	hdr = NULL;

#ifdef COMPRESS_USER_CORES
        if (doing_compress) {
                p = gzopen_flags;
                *p++ = 'w';
                if (compress_user_cores_gzlevel >= 0 &&
                    compress_user_cores_gzlevel <= 9)
                        *p++ = '0' + compress_user_cores_gzlevel;
                *p = 0;
                gzfile = gz_open("", gzopen_flags, vp);
                if (gzfile == Z_NULL) {
                        error = EFAULT;
                        goto done;
                }
                core_buf = malloc(CORE_BUF_SIZE, M_TEMP, M_WAITOK | M_ZERO);
                if (!core_buf) {
                        error = ENOMEM;
                        goto done;
                }
        }
#endif

	/* Size the program segments. */
	seginfo.count = 0;
	seginfo.size = 0;
	each_writable_segment(td, cb_size_segment, &seginfo);

	/*
	 * Calculate the size of the core file header area by making
	 * a dry run of generating it.  Nothing is written, but the
	 * size is calculated.
	 */
	hdrsize = 0;
	__elfN(puthdr)(td, (void *)NULL, &hdrsize, seginfo.count);

#ifdef RACCT
	PROC_LOCK(td->td_proc);
	error = racct_add(td->td_proc, RACCT_CORE, hdrsize + seginfo.size);
	PROC_UNLOCK(td->td_proc);
	if (error != 0) {
		error = EFAULT;
		goto done;
	}
#endif
	if (hdrsize + seginfo.size >= limit) {
		error = EFAULT;
		goto done;
	}

	/*
	 * Allocate memory for building the header, fill it up,
	 * and write it out.
	 */
	hdr = malloc(hdrsize, M_TEMP, M_WAITOK);
	if (hdr == NULL) {
		error = EINVAL;
		goto done;
	}
	error = __elfN(corehdr)(td, vp, cred, seginfo.count, hdr, hdrsize,
	    gzfile);

	/* Write the contents of all of the writable segments. */
	if (error == 0) {
		Elf_Phdr *php;
		off_t offset;
		int i;

		php = (Elf_Phdr *)((char *)hdr + sizeof(Elf_Ehdr)) + 1;
		offset = hdrsize;
		for (i = 0; i < seginfo.count; i++) {
			error = core_output(vp, (caddr_t)(uintptr_t)php->p_vaddr,
			    php->p_filesz, offset, cred, NOCRED, curthread, core_buf, gzfile);
			if (error != 0)
				break;
			offset += php->p_filesz;
			php++;
		}
	}
	if (error) {
		log(LOG_WARNING,
		    "Failed to write core file for process %s (error %d)\n",
		    curproc->p_comm, error);
	}

done:
#ifdef COMPRESS_USER_CORES
	if (core_buf)
		free(core_buf, M_TEMP);
	if (gzfile)
		gzclose(gzfile);
#endif

	free(hdr, M_TEMP);

	return (error);
}

/*
 * A callback for each_writable_segment() to write out the segment's
 * program header entry.
 */
static void
cb_put_phdr(entry, closure)
	vm_map_entry_t entry;
	void *closure;
{
	struct phdr_closure *phc = (struct phdr_closure *)closure;
	Elf_Phdr *phdr = phc->phdr;

	phc->offset = round_page(phc->offset);

	phdr->p_type = PT_LOAD;
	phdr->p_offset = phc->offset;
	phdr->p_vaddr = entry->start;
	phdr->p_paddr = 0;
	phdr->p_filesz = phdr->p_memsz = entry->end - entry->start;
	phdr->p_align = PAGE_SIZE;
	phdr->p_flags = __elfN(untrans_prot)(entry->protection);

	phc->offset += phdr->p_filesz;
	phc->phdr++;
}

/*
 * A callback for each_writable_segment() to gather information about
 * the number of segments and their total size.
 */
static void
cb_size_segment(entry, closure)
	vm_map_entry_t entry;
	void *closure;
{
	struct sseg_closure *ssc = (struct sseg_closure *)closure;

	ssc->count++;
	ssc->size += entry->end - entry->start;
}

/*
 * For each writable segment in the process's memory map, call the given
 * function with a pointer to the map entry and some arbitrary
 * caller-supplied data.
 */
static void
each_writable_segment(td, func, closure)
	struct thread *td;
	segment_callback func;
	void *closure;
{
	struct proc *p = td->td_proc;
	vm_map_t map = &p->p_vmspace->vm_map;
	vm_map_entry_t entry;
	vm_object_t backing_object, object;
	boolean_t ignore_entry;

	vm_map_lock_read(map);
	for (entry = map->header.next; entry != &map->header;
	    entry = entry->next) {
		/*
		 * Don't dump inaccessible mappings, deal with legacy
		 * coredump mode.
		 *
		 * Note that read-only segments related to the elf binary
		 * are marked MAP_ENTRY_NOCOREDUMP now so we no longer
		 * need to arbitrarily ignore such segments.
		 */
		if (elf_legacy_coredump) {
			if ((entry->protection & VM_PROT_RW) != VM_PROT_RW)
				continue;
		} else {
			if ((entry->protection & VM_PROT_ALL) == 0)
				continue;
		}

		/*
		 * Dont include memory segment in the coredump if
		 * MAP_NOCORE is set in mmap(2) or MADV_NOCORE in
		 * madvise(2).  Do not dump submaps (i.e. parts of the
		 * kernel map).
		 */
		if (entry->eflags & (MAP_ENTRY_NOCOREDUMP|MAP_ENTRY_IS_SUB_MAP))
			continue;

		if ((object = entry->object.vm_object) == NULL)
			continue;

		/* Ignore memory-mapped devices and such things. */
		VM_OBJECT_LOCK(object);
		while ((backing_object = object->backing_object) != NULL) {
			VM_OBJECT_LOCK(backing_object);
			VM_OBJECT_UNLOCK(object);
			object = backing_object;
		}
		ignore_entry = object->type != OBJT_DEFAULT &&
		    object->type != OBJT_SWAP && object->type != OBJT_VNODE;
		VM_OBJECT_UNLOCK(object);
		if (ignore_entry)
			continue;

		(*func)(entry, closure);
	}
	vm_map_unlock_read(map);
}

/*
 * Write the core file header to the file, including padding up to
 * the page boundary.
 */
static int
__elfN(corehdr)(td, vp, cred, numsegs, hdr, hdrsize, gzfile)
	struct thread *td;
	struct vnode *vp;
	struct ucred *cred;
	int numsegs;
	size_t hdrsize;
	void *hdr;
	gzFile gzfile;
{
	size_t off;

	/* Fill in the header. */
	bzero(hdr, hdrsize);
	off = 0;
	__elfN(puthdr)(td, hdr, &off, numsegs);

	if (!gzfile) {
		/* Write it to the core file. */
		return (vn_rdwr_inchunks(UIO_WRITE, vp, hdr, hdrsize, (off_t)0,
			UIO_SYSSPACE, IO_UNIT | IO_DIRECT, cred, NOCRED, NULL,
			td));
	} else {
#ifdef COMPRESS_USER_CORES
		if (gzwrite(gzfile, hdr, hdrsize) != hdrsize) {
			log(LOG_WARNING,
			    "Failed to compress core file header for process"
			    " %s.\n", curproc->p_comm);
			return (EFAULT);
		}
		else {
			return (0);
		}
#else
		panic("shouldn't be here");
#endif
	}
}

#if defined(COMPAT_FREEBSD32) && __ELF_WORD_SIZE == 32
#include <compat/freebsd32/freebsd32.h>

typedef struct prstatus32 elf_prstatus_t;
typedef struct prpsinfo32 elf_prpsinfo_t;
typedef struct fpreg32 elf_prfpregset_t;
typedef struct fpreg32 elf_fpregset_t;
typedef struct reg32 elf_gregset_t;
typedef struct thrmisc32 elf_thrmisc_t;
#else
typedef prstatus_t elf_prstatus_t;
typedef prpsinfo_t elf_prpsinfo_t;
typedef prfpregset_t elf_prfpregset_t;
typedef prfpregset_t elf_fpregset_t;
typedef gregset_t elf_gregset_t;
typedef thrmisc_t elf_thrmisc_t;
#endif

static void
__elfN(puthdr)(struct thread *td, void *dst, size_t *off, int numsegs)
{
	struct {
		elf_prstatus_t status;
		elf_prfpregset_t fpregset;
		elf_prpsinfo_t psinfo;
		elf_thrmisc_t thrmisc;
	} *tempdata;
	elf_prstatus_t *status;
	elf_prfpregset_t *fpregset;
	elf_prpsinfo_t *psinfo;
	elf_thrmisc_t *thrmisc;
	struct proc *p;
	struct thread *thr;
	size_t ehoff, noteoff, notesz, phoff;

	p = td->td_proc;

	ehoff = *off;
	*off += sizeof(Elf_Ehdr);

	phoff = *off;
	*off += (numsegs + 1) * sizeof(Elf_Phdr);

	noteoff = *off;
	/*
	 * Don't allocate space for the notes if we're just calculating
	 * the size of the header. We also don't collect the data.
	 */
	if (dst != NULL) {
		tempdata = malloc(sizeof(*tempdata), M_TEMP, M_ZERO|M_WAITOK);
		status = &tempdata->status;
		fpregset = &tempdata->fpregset;
		psinfo = &tempdata->psinfo;
		thrmisc = &tempdata->thrmisc;
	} else {
		tempdata = NULL;
		status = NULL;
		fpregset = NULL;
		psinfo = NULL;
		thrmisc = NULL;
	}

	if (dst != NULL) {
		psinfo->pr_version = PRPSINFO_VERSION;
		psinfo->pr_psinfosz = sizeof(elf_prpsinfo_t);
		strlcpy(psinfo->pr_fname, p->p_comm, sizeof(psinfo->pr_fname));
		/*
		 * XXX - We don't fill in the command line arguments properly
		 * yet.
		 */
		strlcpy(psinfo->pr_psargs, p->p_comm,
		    sizeof(psinfo->pr_psargs));
	}
	__elfN(putnote)(dst, off, "FreeBSD", NT_PRPSINFO, psinfo,
	    sizeof *psinfo);

	/*
	 * To have the debugger select the right thread (LWP) as the initial
	 * thread, we dump the state of the thread passed to us in td first.
	 * This is the thread that causes the core dump and thus likely to
	 * be the right thread one wants to have selected in the debugger.
	 */
	thr = td;
	while (thr != NULL) {
		if (dst != NULL) {
			status->pr_version = PRSTATUS_VERSION;
			status->pr_statussz = sizeof(elf_prstatus_t);
			status->pr_gregsetsz = sizeof(elf_gregset_t);
			status->pr_fpregsetsz = sizeof(elf_fpregset_t);
			status->pr_osreldate = osreldate;
			status->pr_cursig = p->p_sig;
			status->pr_pid = thr->td_tid;
#if defined(COMPAT_FREEBSD32) && __ELF_WORD_SIZE == 32
			fill_regs32(thr, &status->pr_reg);
			fill_fpregs32(thr, fpregset);
#else
			fill_regs(thr, &status->pr_reg);
			fill_fpregs(thr, fpregset);
#endif
			memset(&thrmisc->_pad, 0, sizeof (thrmisc->_pad));
			strcpy(thrmisc->pr_tname, thr->td_name);
		}
		__elfN(putnote)(dst, off, "FreeBSD", NT_PRSTATUS, status,
		    sizeof *status);
		__elfN(putnote)(dst, off, "FreeBSD", NT_FPREGSET, fpregset,
		    sizeof *fpregset);
		__elfN(putnote)(dst, off, "FreeBSD", NT_THRMISC, thrmisc,
		    sizeof *thrmisc);
		/*
		 * Allow for MD specific notes, as well as any MD
		 * specific preparations for writing MI notes.
		 */
		__elfN(dump_thread)(thr, dst, off);

		thr = (thr == td) ? TAILQ_FIRST(&p->p_threads) :
		    TAILQ_NEXT(thr, td_plist);
		if (thr == td)
			thr = TAILQ_NEXT(thr, td_plist);
	}

	notesz = *off - noteoff;

	if (dst != NULL)
		free(tempdata, M_TEMP);

	/* Align up to a page boundary for the program segments. */
	*off = round_page(*off);

	if (dst != NULL) {
		Elf_Ehdr *ehdr;
		Elf_Phdr *phdr;
		struct phdr_closure phc;

		/*
		 * Fill in the ELF header.
		 */
		ehdr = (Elf_Ehdr *)((char *)dst + ehoff);
		ehdr->e_ident[EI_MAG0] = ELFMAG0;
		ehdr->e_ident[EI_MAG1] = ELFMAG1;
		ehdr->e_ident[EI_MAG2] = ELFMAG2;
		ehdr->e_ident[EI_MAG3] = ELFMAG3;
		ehdr->e_ident[EI_CLASS] = ELF_CLASS;
		ehdr->e_ident[EI_DATA] = ELF_DATA;
		ehdr->e_ident[EI_VERSION] = EV_CURRENT;
		ehdr->e_ident[EI_OSABI] = ELFOSABI_FREEBSD;
		ehdr->e_ident[EI_ABIVERSION] = 0;
		ehdr->e_ident[EI_PAD] = 0;
		ehdr->e_type = ET_CORE;
#if defined(COMPAT_FREEBSD32) && __ELF_WORD_SIZE == 32
		ehdr->e_machine = ELF_ARCH32;
#else
		ehdr->e_machine = ELF_ARCH;
#endif
		ehdr->e_version = EV_CURRENT;
		ehdr->e_entry = 0;
		ehdr->e_phoff = phoff;
		ehdr->e_flags = 0;
		ehdr->e_ehsize = sizeof(Elf_Ehdr);
		ehdr->e_phentsize = sizeof(Elf_Phdr);
		ehdr->e_phnum = numsegs + 1;
		ehdr->e_shentsize = sizeof(Elf_Shdr);
		ehdr->e_shnum = 0;
		ehdr->e_shstrndx = SHN_UNDEF;

		/*
		 * Fill in the program header entries.
		 */
		phdr = (Elf_Phdr *)((char *)dst + phoff);

		/* The note segement. */
		phdr->p_type = PT_NOTE;
		phdr->p_offset = noteoff;
		phdr->p_vaddr = 0;
		phdr->p_paddr = 0;
		phdr->p_filesz = notesz;
		phdr->p_memsz = 0;
		phdr->p_flags = 0;
		phdr->p_align = 0;
		phdr++;

		/* All the writable segments from the program. */
		phc.phdr = phdr;
		phc.offset = *off;
		each_writable_segment(td, cb_put_phdr, &phc);
	}
}

static void
__elfN(putnote)(void *dst, size_t *off, const char *name, int type,
    const void *desc, size_t descsz)
{
	Elf_Note note;

	note.n_namesz = strlen(name) + 1;
	note.n_descsz = descsz;
	note.n_type = type;
	if (dst != NULL)
		bcopy(&note, (char *)dst + *off, sizeof note);
	*off += sizeof note;
	if (dst != NULL)
		bcopy(name, (char *)dst + *off, note.n_namesz);
	*off += roundup2(note.n_namesz, sizeof(Elf_Size));
	if (dst != NULL)
		bcopy(desc, (char *)dst + *off, note.n_descsz);
	*off += roundup2(note.n_descsz, sizeof(Elf_Size));
}

static boolean_t
__elfN(parse_notes)(struct image_params *imgp, Elf_Brandnote *checknote,
    int32_t *osrel, const Elf_Phdr *pnote)
{
	const Elf_Note *note, *note0, *note_end;
	const char *note_name;
	int i;

	if (pnote == NULL || pnote->p_offset >= PAGE_SIZE ||
	    pnote->p_filesz > PAGE_SIZE ||
	    pnote->p_offset + pnote->p_filesz >= PAGE_SIZE)
		return (FALSE);

	note = note0 = (const Elf_Note *)(imgp->image_header + pnote->p_offset);
	note_end = (const Elf_Note *)(imgp->image_header +
	    pnote->p_offset + pnote->p_filesz);
	for (i = 0; i < 100 && note >= note0 && note < note_end; i++) {
		if (!aligned(note, Elf32_Addr) || (const char *)note_end -
		    (const char *)note < sizeof(Elf_Note))
			return (FALSE);
		if (note->n_namesz != checknote->hdr.n_namesz ||
		    note->n_descsz != checknote->hdr.n_descsz ||
		    note->n_type != checknote->hdr.n_type)
			goto nextnote;
		note_name = (const char *)(note + 1);
		if (note_name + checknote->hdr.n_namesz >=
		    (const char *)note_end || strncmp(checknote->vendor,
		    note_name, checknote->hdr.n_namesz) != 0)
			goto nextnote;

		/*
		 * Fetch the osreldate for binary
		 * from the ELF OSABI-note if necessary.
		 */
		if ((checknote->flags & BN_TRANSLATE_OSREL) != 0 &&
		    checknote->trans_osrel != NULL)
			return (checknote->trans_osrel(note, osrel));
		return (TRUE);

nextnote:
		note = (const Elf_Note *)((const char *)(note + 1) +
		    roundup2(note->n_namesz, sizeof(Elf32_Addr)) +
		    roundup2(note->n_descsz, sizeof(Elf32_Addr)));
	}

	return (FALSE);
}

/*
 * Try to find the appropriate ABI-note section for checknote,
 * fetch the osreldate for binary from the ELF OSABI-note. Only the
 * first page of the image is searched, the same as for headers.
 */
static boolean_t
__elfN(check_note)(struct image_params *imgp, Elf_Brandnote *checknote,
    int32_t *osrel)
{
	const Elf_Phdr *phdr;
	const Elf_Ehdr *hdr;
	int i;

	hdr = (const Elf_Ehdr *)imgp->image_header;
	phdr = (const Elf_Phdr *)(imgp->image_header + hdr->e_phoff);

	for (i = 0; i < hdr->e_phnum; i++) {
		if (phdr[i].p_type == PT_NOTE &&
		    __elfN(parse_notes)(imgp, checknote, osrel, &phdr[i]))
			return (TRUE);
	}
	return (FALSE);

}

/*
 * Tell kern_execve.c about it, with a little help from the linker.
 */
static struct execsw __elfN(execsw) = {
	__CONCAT(exec_, __elfN(imgact)),
	__XSTRING(__CONCAT(ELF, __ELF_WORD_SIZE))
};
EXEC_SET(__CONCAT(elf, __ELF_WORD_SIZE), __elfN(execsw));

#ifdef COMPRESS_USER_CORES
/*
 * Compress and write out a core segment for a user process.
 *
 * 'inbuf' is the starting address of a VM segment in the process' address
 * space that is to be compressed and written out to the core file.  'dest_buf'
 * is a buffer in the kernel's address space.  The segment is copied from 
 * 'inbuf' to 'dest_buf' first before being processed by the compression
 * routine gzwrite().  This copying is necessary because the content of the VM
 * segment may change between the compression pass and the crc-computation pass
 * in gzwrite().  This is because realtime threads may preempt the UNIX kernel.
 */
static int
compress_core (gzFile file, char *inbuf, char *dest_buf, unsigned int len,
    struct thread *td)
{
	int len_compressed;
	int error = 0;
	unsigned int chunk_len;

	while (len) {
		chunk_len = (len > CORE_BUF_SIZE) ? CORE_BUF_SIZE : len;
		copyin(inbuf, dest_buf, chunk_len);
		len_compressed = gzwrite(file, dest_buf, chunk_len);

		EVENTHANDLER_INVOKE(app_coredump_progress, td, len_compressed);

		if ((unsigned int)len_compressed != chunk_len) {
			log(LOG_WARNING,
			    "compress_core: length mismatch (0x%x returned, "
			    "0x%x expected)\n", len_compressed, chunk_len);
			EVENTHANDLER_INVOKE(app_coredump_error, td,
			    "compress_core: length mismatch %x -> %x",
			    chunk_len, len_compressed);
			error = EFAULT;
			break;
		}
		inbuf += chunk_len;
		len -= chunk_len;
		maybe_yield();
	}

	return (error);
}
#endif /* COMPRESS_USER_CORES */

static vm_prot_t
__elfN(trans_prot)(Elf_Word flags)
{
	vm_prot_t prot;

	prot = 0;
	if (flags & PF_X)
		prot |= VM_PROT_EXECUTE;
	if (flags & PF_W)
		prot |= VM_PROT_WRITE;
	if (flags & PF_R)
		prot |= VM_PROT_READ;
#if __ELF_WORD_SIZE == 32
#if defined(__amd64__) || defined(__ia64__)
	if (i386_read_exec && (flags & PF_R))
		prot |= VM_PROT_EXECUTE;
#endif
#endif
	return (prot);
}

static Elf_Word
__elfN(untrans_prot)(vm_prot_t prot)
{
	Elf_Word flags;

	flags = 0;
	if (prot & VM_PROT_EXECUTE)
		flags |= PF_X;
	if (prot & VM_PROT_READ)
		flags |= PF_R;
	if (prot & VM_PROT_WRITE)
		flags |= PF_W;
	return (flags);
}
