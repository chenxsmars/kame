/*	$NetBSD: ibcs2_exec.c,v 1.28.2.2 2000/09/05 01:43:18 matt Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1998 Scott Bartram
 * Copyright (c) 1994 Adam Glass
 * Copyright (c) 1993, 1994 Christopher G. Demetriou
 * All rights reserved.
 *
 * originally from kern/exec_ecoff.c
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
 *      This product includes software developed by Scott Bartram.
 * 4. The name of the author may not be used to endorse or promote products
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

#define ELFSIZE		32
#include "opt_execfmt.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/exec.h>
#include <sys/exec_coff.h>
#include <sys/exec_elf.h>
#include <sys/resourcevar.h>

#include <sys/mman.h>
#include <sys/syscallargs.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_map.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/ibcs2_machdep.h>

#include <compat/ibcs2/ibcs2_types.h>
#include <compat/ibcs2/ibcs2_exec.h>
#include <compat/ibcs2/ibcs2_errno.h>
#include <compat/ibcs2/ibcs2_util.h>
#include <compat/ibcs2/ibcs2_syscall.h>


#ifdef EXEC_ELF32
#define IBCS2_ELF_AUX_ARGSIZ	howmany(sizeof(AuxInfo) * 8, sizeof(char *))
#endif

int exec_ibcs2_coff_prep_omagic __P((struct proc *, struct exec_package *,
				     struct coff_filehdr *, 
				     struct coff_aouthdr *));
int exec_ibcs2_coff_prep_nmagic __P((struct proc *, struct exec_package *,
				     struct coff_filehdr *, 
				     struct coff_aouthdr *));
int exec_ibcs2_coff_prep_zmagic __P((struct proc *, struct exec_package *,
				     struct coff_filehdr *, 
				     struct coff_aouthdr *));
int exec_ibcs2_coff_setup_stack __P((struct proc *, struct exec_package *));
void cpu_exec_ibcs2_coff_setup __P((int, struct proc *, struct exec_package *,
				    void *));

int exec_ibcs2_xout_prep_nmagic __P((struct proc *, struct exec_package *,
				     struct xexec *, struct xext *));
int exec_ibcs2_xout_prep_zmagic __P((struct proc *, struct exec_package *,
				     struct xexec *, struct xext *));
int exec_ibcs2_xout_setup_stack __P((struct proc *, struct exec_package *));
int coff_load_shlib __P((struct proc *, const char *, struct exec_package *));
static int coff_find_section __P((struct proc *, struct vnode *, 
				  struct coff_filehdr *, struct coff_scnhdr *,
				  int));
#ifdef EXEC_ELF32
static int ibcs2_elf32_signature __P((struct proc *p, struct exec_package *,
				      Elf32_Ehdr *));
#endif
	

extern struct sysent ibcs2_sysent[];
extern char *ibcs2_syscallnames[];
extern char ibcs2_sigcode[], ibcs2_esigcode[];

const char ibcs2_emul_path[] = "/emul/ibcs2";

#ifdef IBCS2_DEBUG
int ibcs2_debug = 1;
#else
int ibcs2_debug = 0;
#endif

struct emul emul_ibcs2_coff = {
	"ibcs2",
	native_to_ibcs2_errno,
	ibcs2_sendsig,
	0,
	IBCS2_SYS_MAXSYSCALL,
	ibcs2_sysent,
	ibcs2_syscallnames,
	0,
	copyargs,
	ibcs2_setregs,
	ibcs2_sigcode,
	ibcs2_esigcode,
};

struct emul emul_ibcs2_xout = {
	"ibcs2",
	native_to_ibcs2_errno,
	ibcs2_sendsig,
	0,
	IBCS2_SYS_MAXSYSCALL,
	ibcs2_sysent,
	ibcs2_syscallnames,
	0,
	copyargs,
	ibcs2_setregs,
	ibcs2_sigcode,
	ibcs2_esigcode,
};

#ifdef EXEC_ELF32
struct emul emul_ibcs2_elf = {
	"ibcs2",
	native_to_ibcs2_errno,
	ibcs2_sendsig,
	0,
	IBCS2_SYS_MAXSYSCALL,
	ibcs2_sysent,
	ibcs2_syscallnames,
	IBCS2_ELF_AUX_ARGSIZ,
	elf32_copyargs,
	ibcs2_setregs,
	ibcs2_sigcode,
	ibcs2_esigcode,
};
#endif /* EXEC_ELF32 */


/*
 * The SCO compiler adds the string "SCO" to the .notes section of all
 * binaries I've seen so far.
 *
 * XXX - probably should only compare the id in the actual ELF notes struct
 */

#ifdef EXEC_ELF32
#define SCO_SIGNATURE	"\004\0\0\0\014\0\0\0\001\0\0\0SCO\0"

static int
ibcs2_elf32_signature(p, epp, eh)
	struct proc *p;
	struct exec_package *epp;
	Elf32_Ehdr *eh;
{
	size_t shsize = sizeof(Elf32_Shdr) * eh->e_shnum;
	size_t i;
	static const char signature[] = SCO_SIGNATURE;
	char buf[sizeof(signature) - 1];
	Elf32_Shdr *sh;
	int error;

	sh = (Elf32_Shdr *)malloc(shsize, M_TEMP, M_WAITOK);

	if ((error = elf32_read_from(p, epp->ep_vp, eh->e_shoff,
	    (caddr_t)sh, shsize)) != 0)
		goto out;

	for (i = 0; i < eh->e_shnum; i++) {
		Elf32_Shdr *s = &sh[i];
		if (s->sh_type != SHT_NOTE ||
		    s->sh_flags != 0 ||
		    s->sh_size < sizeof(signature) - 1)
			continue;

		if ((error = elf32_read_from(p, epp->ep_vp, s->sh_offset,
		    (caddr_t)buf, sizeof(signature) - 1)) != 0)
			goto out;

		if (memcmp(buf, signature, sizeof(signature) - 1) == 0)
			goto out;
		else
			break;	/* only one .note section so quit */
	}
	error = EFTYPE;

out:
	free(sh, M_TEMP);
	return error;
}

/*
 * ibcs2_elf32_probe - search the executable for signs of SCO
 */

int
ibcs2_elf32_probe(p, epp, eh, itp, pos)
	struct proc *p;
	struct exec_package *epp;
	Elf32_Ehdr *eh;
	char *itp;
	Elf32_Addr *pos;
{
	const char *bp;
	int error;
	size_t len;

	if ((error = ibcs2_elf32_signature(p, epp, eh)) != 0)
                return error;

	if (itp[0]) {
		if ((error = emul_find(p, NULL, ibcs2_emul_path, itp, &bp, 0)))
			return error;
		if ((error = copystr(bp, itp, MAXPATHLEN, &len)))
			return error;
		free((void *)bp, M_TEMP);
	}
	epp->ep_emul = &emul_ibcs2_elf;
	*pos = ELF32_NO_ADDR;
	return 0;
}
#endif /* EXEC_ELF32 */

/*
 * exec_ibcs2_coff_makecmds(): Check if it's an coff-format executable.
 *
 * Given a proc pointer and an exec package pointer, see if the referent
 * of the epp is in coff format.  Check 'standard' magic numbers for
 * this architecture.  If that fails, return failure.
 *
 * This function is  responsible for creating a set of vmcmds which can be
 * used to build the process's vm space and inserting them into the exec
 * package.
 */

int
exec_ibcs2_coff_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int error;
	struct coff_filehdr *fp = epp->ep_hdr;
	struct coff_aouthdr *ap;

	if (epp->ep_hdrvalid < COFF_HDR_SIZE) {
		DPRINTF(("ibcs2: bad coff hdr size\n"));
		return ENOEXEC;
	}

	if (COFF_BADMAG(fp)) {
		DPRINTF(("ibcs2: bad coff magic\n"));
		return ENOEXEC;
	}
	
	ap = (void *)((char *)epp->ep_hdr + sizeof(struct coff_filehdr));
	switch (ap->a_magic) {
	case COFF_OMAGIC:
		error = exec_ibcs2_coff_prep_omagic(p, epp, fp, ap);
		break;
	case COFF_NMAGIC:
		error = exec_ibcs2_coff_prep_nmagic(p, epp, fp, ap);
		break;
	case COFF_ZMAGIC:
		error = exec_ibcs2_coff_prep_zmagic(p, epp, fp, ap);
		break;
	default:
		return ENOEXEC;
	}

	if (error == 0)
		epp->ep_emul = &emul_ibcs2_coff;

	if (error)
		kill_vmcmds(&epp->ep_vmcmds);

	return error;
}

/*
 * exec_ibcs2_coff_setup_stack(): Set up the stack segment for a coff
 * executable.
 *
 * Note that the ep_ssize parameter must be set to be the current stack
 * limit; this is adjusted in the body of execve() to yield the
 * appropriate stack segment usage once the argument length is
 * calculated.
 *
 * This function returns an int for uniformity with other (future) formats'
 * stack setup functions.  They might have errors to return.
 */

int
exec_ibcs2_coff_setup_stack(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	/* DPRINTF(("enter exec_ibcs2_coff_setup_stack\n")); */

	epp->ep_maxsaddr = USRSTACK - MAXSSIZ;
	epp->ep_minsaddr = USRSTACK;
	epp->ep_ssize = p->p_rlimit[RLIMIT_STACK].rlim_cur;

	/*
	 * set up commands for stack.  note that this takes *two*, one to
	 * map the part of the stack which we can access, and one to map
	 * the part which we can't.
	 *
	 * arguably, it could be made into one, but that would require the
	 * addition of another mapping proc, which is unnecessary
	 *
	 * note that in memory, things assumed to be: 0 ....... ep_maxsaddr
	 * <stack> ep_minsaddr
	 */
	/* DPRINTF(("VMCMD: addr %x size %d\n", epp->ep_maxsaddr,
		 (epp->ep_minsaddr - epp->ep_ssize) - epp->ep_maxsaddr)); */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero,
		  ((epp->ep_minsaddr - epp->ep_ssize) - epp->ep_maxsaddr),
		  epp->ep_maxsaddr, NULLVP, 0, VM_PROT_NONE);
	/* DPRINTF(("VMCMD: addr %x size %d\n",
		 epp->ep_minsaddr - epp->ep_ssize,
		 epp->ep_ssize)); */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, epp->ep_ssize,
		  (epp->ep_minsaddr - epp->ep_ssize), NULLVP, 0,
		  VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return 0;
}


/*
 * exec_ibcs2_coff_prep_omagic(): Prepare a COFF OMAGIC binary's exec package
 */

int
exec_ibcs2_coff_prep_omagic(p, epp, fp, ap)
	struct proc *p;
	struct exec_package *epp;
	struct coff_filehdr *fp;
	struct coff_aouthdr *ap;
{
	epp->ep_taddr = COFF_SEGMENT_ALIGN(fp, ap, ap->a_tstart);
	epp->ep_tsize = ap->a_tsize;
	epp->ep_daddr = COFF_SEGMENT_ALIGN(fp, ap, ap->a_dstart);
	epp->ep_dsize = ap->a_dsize;
	epp->ep_entry = ap->a_entry;

	DPRINTF(("ibcs2_omagic: text=%#lx/%#lx, data=%#lx/%#lx, bss=%#lx/%#lx, entry=%#lx\n",
		epp->ep_taddr, epp->ep_tsize,
		epp->ep_daddr, epp->ep_dsize,
		ap->a_dstart + ap->a_dsize, ap->a_bsize,
		epp->ep_entry));

	/* set up command for text and data segments */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn,
		  ap->a_tsize + ap->a_dsize, epp->ep_taddr, epp->ep_vp,
		  COFF_TXTOFF(fp, ap),
		  VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	if (ap->a_bsize > 0) {
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, ap->a_bsize,
			  COFF_SEGMENT_ALIGN(fp, ap, ap->a_dstart + ap->a_dsize),
			  NULLVP, 0,
			  VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);
		epp->ep_dsize += ap->a_bsize;
	}
	/* The following is to make obreak(2) happy.  It expects daddr
	 * to on a page boundary and will round up dsize to a page
	 * address.
	 */
	if (trunc_page(epp->ep_daddr) != epp->ep_daddr) {
		epp->ep_dsize += epp->ep_daddr - trunc_page(epp->ep_daddr);
		epp->ep_daddr = trunc_page(epp->ep_daddr);
		if (epp->ep_taddr + epp->ep_tsize > epp->ep_daddr)
			epp->ep_tsize = epp->ep_daddr - epp->ep_taddr;
	}
	
	return exec_ibcs2_coff_setup_stack(p, epp);
}

/*
 * exec_ibcs2_coff_prep_nmagic(): Prepare a 'native' NMAGIC COFF binary's exec
 *                          package.
 */

int
exec_ibcs2_coff_prep_nmagic(p, epp, fp, ap)
	struct proc *p;
	struct exec_package *epp;
	struct coff_filehdr *fp;
	struct coff_aouthdr *ap;
{
	u_long tsize, tend, toverlap, doverlap;

	epp->ep_taddr = COFF_SEGMENT_ALIGN(fp, ap, ap->a_tstart);
	epp->ep_tsize = ap->a_tsize;
	epp->ep_daddr = ap->a_dstart;
	epp->ep_dsize = ap->a_dsize;
	epp->ep_entry = ap->a_entry;

	/* Do the text and data pages overlap?
	 */
	tend = epp->ep_taddr + epp->ep_tsize - 1;
	if (trunc_page(tend) == trunc_page(epp->ep_daddr)) {
		/* If the first page of text is the first page of data,
		 * then we consider it all data.
		 */
		if (trunc_page(epp->ep_taddr) == trunc_page(epp->ep_daddr)) {
			tsize = 0;
		} else {
			tsize = trunc_page(tend) - epp->ep_taddr;
		}

		/* If the text and data file and VA offsets are the
		 * same, simply bring the data segment to start on
		 * the start of the page.
		 */
		if (epp->ep_daddr - epp->ep_taddr ==
		    COFF_DATOFF(fp, ap) - COFF_TXTOFF(fp, ap)) {
			u_long diff = epp->ep_daddr - trunc_page(epp->ep_daddr);
			epp->ep_daddr -= diff;
			epp->ep_dsize += diff;
			epp->ep_tsize = tsize;
			toverlap = 0;
			doverlap = 0;
		} else {
			/* otherwise copy the individual pieces */
			toverlap = epp->ep_tsize - tsize;
			doverlap = round_page(epp->ep_daddr) - epp->ep_daddr;
			if (doverlap > epp->ep_dsize)
				doverlap = epp->ep_dsize;
		}
	} else {
		tsize = epp->ep_tsize;
		toverlap = 0;
		doverlap = 0;
	}

	DPRINTF(("nmagic_vmcmds:"));
	if (tsize > 0) {
		/* set up command for text segment */
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, tsize,
			  epp->ep_taddr, epp->ep_vp, COFF_TXTOFF(fp, ap),
			  VM_PROT_READ|VM_PROT_EXECUTE);
		DPRINTF((" map_readvn(%#lx/%#lx@%#lx)",
			epp->ep_taddr, tsize, (u_long) COFF_TXTOFF(fp, ap)));
	}
	if (toverlap > 0) {
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, toverlap,
			  epp->ep_taddr + tsize, epp->ep_vp,
			  COFF_TXTOFF(fp, ap) + tsize,
			  VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);
		DPRINTF((" map_readvn(%#lx/%#lx@%#lx)",
			epp->ep_taddr + tsize, toverlap,
			COFF_TXTOFF(fp, ap) + tsize));
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_readvn, doverlap,
			  epp->ep_daddr, epp->ep_vp,
			  COFF_DATOFF(fp, ap),
			  VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);
		DPRINTF((" readvn(%#lx/%#lx@%#lx)", epp->ep_daddr, doverlap,
			COFF_DATOFF(fp, ap)));
	}

	if (epp->ep_dsize > doverlap) {
		/* set up command for data segment */
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn,
			  epp->ep_dsize - doverlap, epp->ep_daddr + doverlap,
			  epp->ep_vp, COFF_DATOFF(fp, ap) + doverlap,
			  VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);
		DPRINTF((" map_readvn(%#lx/%#lx@%#lx)",
			epp->ep_daddr + doverlap, epp->ep_dsize - doverlap,
			COFF_DATOFF(fp, ap) + doverlap));
	}

	/* Handle page remainders for pagedvn.
	 */

	/* set up command for bss segment */
	if (ap->a_bsize > 0) {
		u_long dend = round_page(epp->ep_daddr + epp->ep_dsize);
		u_long dspace = dend - (epp->ep_daddr + epp->ep_dsize);
		if (ap->a_bsize > dspace) {
			NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero,
				  ap->a_bsize - dspace, dend, NULLVP, 0,
				  VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);
			DPRINTF((" map_zero(%#lx/%#lx)",
				dend, ap->a_bsize - dspace));
		}
		epp->ep_dsize += ap->a_bsize;
	}
	DPRINTF(("\n"));
	/* The following is to make obreak(2) happy.  It expects daddr
	 * to on a page boundary and will round up dsize to a page
	 * address.
	 */
	if (trunc_page(epp->ep_daddr) != epp->ep_daddr) {
		epp->ep_dsize += epp->ep_daddr - trunc_page(epp->ep_daddr);
		epp->ep_daddr = trunc_page(epp->ep_daddr);
		if (epp->ep_taddr + epp->ep_tsize > epp->ep_daddr)
			epp->ep_tsize = epp->ep_daddr - epp->ep_taddr;
	}

	return exec_ibcs2_coff_setup_stack(p, epp);
}

/*
 * coff_find_section - load specified section header
 *
 * TODO - optimize by reading all section headers in at once
 */

static int
coff_find_section(p, vp, fp, sh, s_type)
	struct proc *p;
	struct vnode *vp;
	struct coff_filehdr *fp;
	struct coff_scnhdr *sh;
	int s_type;
{
	int i, pos, siz, error;
	size_t resid;

	pos = COFF_HDR_SIZE;
	for (i = 0; i < fp->f_nscns; i++, pos += sizeof(struct coff_scnhdr)) {
		siz = sizeof(struct coff_scnhdr);
		error = vn_rdwr(UIO_READ, vp, (caddr_t) sh,
		    siz, pos, UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred,
		    &resid, p);
		if (error) {
			DPRINTF(("section hdr %d read error %d\n", i, error));
			return error;
		}
		siz -= resid;
		if (siz != sizeof(struct coff_scnhdr)) {
			DPRINTF(("incomplete read: hdr %d ask=%d, rem=%lu got %d\n",
				 s_type, sizeof(struct coff_scnhdr),
				 (u_long) resid, siz));
			return ENOEXEC;
		}
		/* DPRINTF(("found section: %x\n", sh->s_flags)); */
		if (sh->s_flags == s_type)
			return 0;
	}
	return ENOEXEC;
}

/*
 * exec_ibcs2_coff_prep_zmagic(): Prepare a COFF ZMAGIC binary's exec package
 *
 * First, set the various offsets/lengths in the exec package.
 *
 * Then, mark the text image busy (so it can be demand paged) or error
 * out if this is not possible.  Finally, set up vmcmds for the
 * text, data, bss, and stack segments.
 */

int
exec_ibcs2_coff_prep_zmagic(p, epp, fp, ap)
	struct proc *p;
	struct exec_package *epp;
	struct coff_filehdr *fp;
	struct coff_aouthdr *ap;
{
	int error;
	u_long offset;
	long dsize, baddr, bsize;
	struct coff_scnhdr sh;
	
	/* DPRINTF(("enter exec_ibcs2_coff_prep_zmagic\n")); */

	/* set up command for text segment */
	error = coff_find_section(p, epp->ep_vp, fp, &sh, COFF_STYP_TEXT);
	if (error) {		
		DPRINTF(("can't find text section: %d\n", error));
		return error;
	}
	/* DPRINTF(("COFF text addr %x size %d offset %d\n", sh.s_vaddr,
		 sh.s_size, sh.s_scnptr)); */
	epp->ep_taddr = COFF_ALIGN(sh.s_vaddr);
	offset = sh.s_scnptr - (sh.s_vaddr - epp->ep_taddr);
	epp->ep_tsize = sh.s_size + (sh.s_vaddr - epp->ep_taddr);

#ifdef notyet
	/*
	 * check if vnode is in open for writing, because we want to
	 * demand-page out of it.  if it is, don't do it, for various
	 * reasons
n	 */
	if ((ap->a_tsize != 0 || ap->a_dsize != 0) &&
	    epp->ep_vp->v_writecount != 0) {
#ifdef DIAGNOSTIC
		if (epp->ep_vp->v_flag & VTEXT)
			panic("exec: a VTEXT vnode has writecount != 0\n");
#endif
		return ETXTBSY;
	}
	vn_marktext(epp->ep_vp);
#endif
	
	/* DPRINTF(("VMCMD: addr %x size %d offset %d\n", epp->ep_taddr,
		 epp->ep_tsize, offset)); */
#ifdef notyet
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, epp->ep_tsize,
		  epp->ep_taddr, epp->ep_vp, offset,
		  VM_PROT_READ|VM_PROT_EXECUTE);
#else
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, epp->ep_tsize,
		  epp->ep_taddr, epp->ep_vp, offset,
		  VM_PROT_READ|VM_PROT_EXECUTE);
#endif

	/* set up command for data segment */
	error = coff_find_section(p, epp->ep_vp, fp, &sh, COFF_STYP_DATA);
	if (error) {
		DPRINTF(("can't find data section: %d\n", error));
		return error;
	}
	/* DPRINTF(("COFF data addr %x size %d offset %d\n", sh.s_vaddr,
		 sh.s_size, sh.s_scnptr)); */
	epp->ep_daddr = COFF_ALIGN(sh.s_vaddr);
	offset = sh.s_scnptr - (sh.s_vaddr - epp->ep_daddr);
	dsize = sh.s_size + (sh.s_vaddr - epp->ep_daddr);
	epp->ep_dsize = dsize + ap->a_bsize;

	/* DPRINTF(("VMCMD: addr %x size %d offset %d\n", epp->ep_daddr,
		 dsize, offset)); */
#ifdef notyet
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, dsize,
		  epp->ep_daddr, epp->ep_vp, offset,
		  VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);
#else
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn,
		  dsize, epp->ep_daddr, epp->ep_vp, offset,
		  VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);
#endif

	/* set up command for bss segment */
	baddr = round_page(epp->ep_daddr + dsize);
	bsize = epp->ep_daddr + epp->ep_dsize - baddr;
	if (bsize > 0) {
		/* DPRINTF(("VMCMD: addr %x size %d offset %d\n",
			 baddr, bsize, 0)); */
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero,
			  bsize, baddr, NULLVP, 0,
			  VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);
	}

	/* load any shared libraries */
	error = coff_find_section(p, epp->ep_vp, fp, &sh, COFF_STYP_SHLIB);
	if (!error) {
		size_t resid;
		struct coff_slhdr *slhdr;
		char *buf, *bufp;
		int len = sh.s_size, path_index, entry_len;

#if 0
		if (len > COFF_SHLIBSEC_MAXSIZE) {
			return ENOEXEC;
		}
#endif
		buf = (char *) malloc(len, M_TEMP, M_WAITOK);
		if (buf == NULL)
			return ENOEXEC;

		/* DPRINTF(("COFF shlib size %d offset %d\n",
			 sh.s_size, sh.s_scnptr)); */

		error = vn_rdwr(UIO_READ, epp->ep_vp, (caddr_t) buf,
				len, sh.s_scnptr,
				UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred,
				&resid, p);
		if (error) {
			DPRINTF(("shlib section read error %d\n", error));
			free(buf, M_TEMP);
			return ENOEXEC;
		}
		bufp = buf;
		while (len) {
			slhdr = (struct coff_slhdr *)bufp;
			path_index = slhdr->path_index * sizeof(long);
			entry_len = slhdr->entry_len * sizeof(long);

			if (entry_len > len) {
				free(buf, M_TEMP);
				return ENOEXEC;
			}

			/* DPRINTF(("path_index: %d entry_len: %d name: %s\n",
				 path_index, entry_len, slhdr->sl_name)); */

			error = coff_load_shlib(p, slhdr->sl_name, epp);
			if (error) {
				free(buf, M_TEMP);
				return ENOEXEC;
			}
			bufp += entry_len;
			len -= entry_len;
		}
		free(buf, M_TEMP);
	}
		
	/* set up entry point */
	epp->ep_entry = ap->a_entry;

	DPRINTF(("ibcs2_zmagic: text addr: %#lx size: %#lx data addr: %#lx size: %#lx entry: %#lx\n",
		 epp->ep_taddr, epp->ep_tsize,
		 epp->ep_daddr, epp->ep_dsize,
		 epp->ep_entry));

	/* The following is to make obreak(2) happy.  It expects daddr
	 * to on a page boundary and will round up dsize to a page
	 * address.
	 */
	if (trunc_page(epp->ep_daddr) != epp->ep_daddr) {
		epp->ep_dsize += epp->ep_daddr - trunc_page(epp->ep_daddr);
		epp->ep_daddr = trunc_page(epp->ep_daddr);
		if (epp->ep_taddr + epp->ep_tsize > epp->ep_daddr)
			epp->ep_tsize = epp->ep_daddr - epp->ep_taddr;
	}

	
	return exec_ibcs2_coff_setup_stack(p, epp);
}

int
coff_load_shlib(p, path, epp)
	struct proc *p;
	const char *path;
	struct exec_package *epp;
{
	int error, siz;
	int taddr, tsize, daddr, dsize, offset;
	size_t resid;
	struct nameidata nd;
	struct coff_filehdr fh, *fhp = &fh;
	struct coff_scnhdr sh, *shp = &sh;

	/*
	 * 1. open shlib file
	 * 2. read filehdr
	 * 3. map text, data, and bss out of it using VM_*
	 */
	IBCS2_CHECK_ALT_EXIST(p, NULL, path);	/* path is on kernel stack */
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, path, p);
	/* first get the vnode */
	if ((error = namei(&nd)) != 0) {
		DPRINTF(("coff_load_shlib: can't find library %s\n", path));
		return error;
	}

	siz = sizeof(struct coff_filehdr);
	error = vn_rdwr(UIO_READ, nd.ni_vp, (caddr_t) fhp, siz, 0,
	    UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &resid, p);
	if (error) {
	    DPRINTF(("filehdr read error %d\n", error));
	    vrele(nd.ni_vp);
	    return error;
	}
	siz -= resid;
	if (siz != sizeof(struct coff_filehdr)) {
	    DPRINTF(("coff_load_shlib: incomplete read: ask=%d, rem=%lu got %d\n",
		     sizeof(struct coff_filehdr), (u_long) resid, siz));
	    vrele(nd.ni_vp);
	    return ENOEXEC;
	}

	/* load text */
	error = coff_find_section(p, nd.ni_vp, fhp, shp, COFF_STYP_TEXT);
	if (error) {
	    DPRINTF(("can't find shlib text section\n"));
	    vrele(nd.ni_vp);
	    return error;
	}
	/* DPRINTF(("COFF text addr %x size %d offset %d\n", sh.s_vaddr,
		 sh.s_size, sh.s_scnptr)); */
	taddr = COFF_ALIGN(shp->s_vaddr);
	offset = shp->s_scnptr - (shp->s_vaddr - taddr);
	tsize = shp->s_size + (shp->s_vaddr - taddr);
	/* DPRINTF(("VMCMD: addr %x size %d offset %d\n", taddr, tsize, offset)); */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, tsize, taddr,
		  nd.ni_vp, offset,
		  VM_PROT_READ|VM_PROT_EXECUTE);

	/* load data */
	error = coff_find_section(p, nd.ni_vp, fhp, shp, COFF_STYP_DATA);
	if (error) {
	    DPRINTF(("can't find shlib data section\n"));
	    vrele(nd.ni_vp);
	    return error;
	}
	/* DPRINTF(("COFF data addr %x size %d offset %d\n", shp->s_vaddr,
		 shp->s_size, shp->s_scnptr)); */
	daddr = COFF_ALIGN(shp->s_vaddr);
	offset = shp->s_scnptr - (shp->s_vaddr - daddr);
	dsize = shp->s_size + (shp->s_vaddr - daddr);
	/* epp->ep_dsize = dsize + ap->a_bsize; */

	/* DPRINTF(("VMCMD: addr %x size %d offset %d\n", daddr, dsize, offset)); */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn,
		  dsize, daddr, nd.ni_vp, offset,
		  VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* load bss */
	error = coff_find_section(p, nd.ni_vp, fhp, shp, COFF_STYP_BSS);
	if (!error) {
		int baddr = round_page(daddr + dsize);
		int bsize = daddr + dsize + shp->s_size - baddr;
		if (bsize > 0) {
			/* DPRINTF(("VMCMD: addr %x size %d offset %d\n",
			   baddr, bsize, 0)); */
			NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero,
				  bsize, baddr, NULLVP, 0,
				  VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);
	    }
	}
	vrele(nd.ni_vp);

	return 0;
}

int
exec_ibcs2_xout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int error;
	struct xexec *xp = epp->ep_hdr;
	struct xext *xep;

	if (epp->ep_hdrvalid < XOUT_HDR_SIZE)
		return ENOEXEC;

	if ((xp->x_magic != XOUT_MAGIC) || (xp->x_cpu != XC_386))
		return ENOEXEC;
	if ((xp->x_renv & (XE_ABS | XE_VMOD)) || !(xp->x_renv & XE_EXEC))
		return ENOEXEC;

	xep = (void *)((char *)epp->ep_hdr + sizeof(struct xexec));
#ifdef notyet
	if (xp->x_renv & XE_PURE)
		error = exec_ibcs2_xout_prep_zmagic(p, epp, xp, xep);
	else
#endif
		error = exec_ibcs2_xout_prep_nmagic(p, epp, xp, xep);

	if (error == 0)
		epp->ep_emul = &emul_ibcs2_xout;

	if (error)
		kill_vmcmds(&epp->ep_vmcmds);

	return error;
}

/*
 * exec_ibcs2_xout_prep_nmagic(): Prepare a pure x.out binary's exec package
 *
 */

int
exec_ibcs2_xout_prep_nmagic(p, epp, xp, xep)
	struct proc *p;
	struct exec_package *epp;
	struct xexec *xp;
	struct xext *xep;
{
	int error, nseg, i;
	long baddr, bsize;
	struct xseg *xs;
	size_t resid;

	/* read in segment table */
	xs = (struct xseg *)malloc(xep->xe_segsize, M_TEMP, M_WAITOK);
	error = vn_rdwr(UIO_READ, epp->ep_vp, (caddr_t)xs,
			xep->xe_segsize, xep->xe_segpos,
			UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred,
			&resid, p);
	if (error) {
		DPRINTF(("segment table read error %d\n", error));
		free(xs, M_TEMP);
		return ENOEXEC;
	}

	for (nseg = xep->xe_segsize / sizeof(*xs), i = 0; i < nseg; i++) {
		switch (xs[i].xs_type) {
		case XS_TTEXT:	/* text segment */

			DPRINTF(("text addr %lx psize %ld vsize %ld off %ld\n",
				 xs[i].xs_rbase, xs[i].xs_psize,
				 xs[i].xs_vsize, xs[i].xs_filpos));

			epp->ep_taddr = xs[i].xs_rbase;	/* XXX - align ??? */
			epp->ep_tsize = xs[i].xs_vsize;

			DPRINTF(("VMCMD: addr %lx size %ld offset %ld\n",
				 epp->ep_taddr, epp->ep_tsize,
				 xs[i].xs_filpos));
			NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn,
				  epp->ep_tsize, epp->ep_taddr,
				  epp->ep_vp, xs[i].xs_filpos,
				  VM_PROT_READ|VM_PROT_EXECUTE);
			break;

		case XS_TDATA:	/* data segment */

			DPRINTF(("data addr %lx psize %ld vsize %ld off %ld\n",
				 xs[i].xs_rbase, xs[i].xs_psize,
				 xs[i].xs_vsize, xs[i].xs_filpos));

			epp->ep_daddr = xs[i].xs_rbase;	/* XXX - align ??? */
			epp->ep_dsize = xs[i].xs_vsize;

			DPRINTF(("VMCMD: addr %lx size %ld offset %ld\n",
				 epp->ep_daddr, xs[i].xs_psize,
				 xs[i].xs_filpos));
			NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn,
				  xs[i].xs_psize, epp->ep_daddr,
				  epp->ep_vp, xs[i].xs_filpos,
				  VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

			/* set up command for bss segment */
			baddr = round_page(epp->ep_daddr + xs[i].xs_psize);
			bsize = epp->ep_daddr + epp->ep_dsize - baddr;
			if (bsize > 0) {
				DPRINTF(("VMCMD: bss addr %lx size %ld off %d\n",
					 baddr, bsize, 0));
				NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero,
					  bsize, baddr, NULLVP, 0,
					  VM_PROT_READ|VM_PROT_WRITE|
					  VM_PROT_EXECUTE);
			}
			break;

		default:
			break;
		}
	}

	/* set up entry point */
	epp->ep_entry = xp->x_entry;

	DPRINTF(("text addr: %lx size: %ld data addr: %lx size: %ld entry: %lx\n",
		 epp->ep_taddr, epp->ep_tsize,
		 epp->ep_daddr, epp->ep_dsize,
		 epp->ep_entry));
	
	free(xs, M_TEMP);
	return exec_ibcs2_xout_setup_stack(p, epp);
}

/*
 * exec_ibcs2_xout_setup_stack(): Set up the stack segment for a x.out
 * executable.
 *
 * Note that the ep_ssize parameter must be set to be the current stack
 * limit; this is adjusted in the body of execve() to yield the
 * appropriate stack segment usage once the argument length is
 * calculated.
 *
 * This function returns an int for uniformity with other (future) formats'
 * stack setup functions.  They might have errors to return.
 */

int
exec_ibcs2_xout_setup_stack(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	epp->ep_maxsaddr = USRSTACK - MAXSSIZ;
	epp->ep_minsaddr = USRSTACK;
	epp->ep_ssize = p->p_rlimit[RLIMIT_STACK].rlim_cur;

	/*
	 * set up commands for stack.  note that this takes *two*, one to
	 * map the part of the stack which we can access, and one to map
	 * the part which we can't.
	 *
	 * arguably, it could be made into one, but that would require the
	 * addition of another mapping proc, which is unnecessary
	 *
	 * note that in memory, things assumed to be: 0 ....... ep_maxsaddr
	 * <stack> ep_minsaddr
	 */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero,
		  ((epp->ep_minsaddr - epp->ep_ssize) - epp->ep_maxsaddr),
		  epp->ep_maxsaddr, NULLVP, 0, VM_PROT_NONE);
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, epp->ep_ssize,
		  (epp->ep_minsaddr - epp->ep_ssize), NULLVP, 0,
		  VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return 0;
}
