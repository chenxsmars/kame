/*	$NetBSD: pmap.h,v 1.28 2000/04/28 19:25:55 soren Exp $	*/

/*
 * Copyright (c) 1987 Carnegie-Mellon University
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	@(#)pmap.h	8.1 (Berkeley) 6/10/93
 */

#ifndef	_PMAP_MACHINE_
#define	_PMAP_MACHINE_

#include <mips/cpuregs.h>	/* for KSEG0 below */

/*
 * The user address space is 2Gb (0x0 - 0x80000000).
 * User programs are laid out in memory as follows:
 *			address
 *	USRTEXT		0x00001000
 *	USRDATA		USRTEXT + text_size
 *	USRSTACK	0x7FFFFFFF
 *
 * The user address space is mapped using a two level structure where
 * virtual address bits 30..22 are used to index into a segment table which
 * points to a page worth of PTEs (4096 page can hold 1024 PTEs).
 * Bits 21..12 are then used to index a PTE which describes a page within
 * a segment.
 *
 * The wired entries in the TLB will contain the following:
 *	0-1	(UPAGES)	for curproc user struct and kernel stack.
 *
 * Note: The kernel doesn't use the same data structures as user programs.
 * All the PTE entries are stored in a single array in Sysmap which is
 * dynamically allocated at boot time.
 */

#define mips_trunc_seg(x)	((vaddr_t)(x) & ~SEGOFSET)
#define mips_round_seg(x)	(((vaddr_t)(x) + SEGOFSET) & ~SEGOFSET)
#define pmap_segmap(m, v)	((m)->pm_segtab->seg_tab[((v) >> SEGSHIFT)])

#define PMAP_SEGTABSIZE		512

union pt_entry;

struct segtab {
	union pt_entry	*seg_tab[PMAP_SEGTABSIZE];
};

/*
 * Machine dependent pmap structure.
 */
typedef struct pmap {
	int			pm_count;	/* pmap reference count */
	simple_lock_data_t	pm_lock;	/* lock on pmap */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	unsigned		pm_asid;	/* TLB address space tag */
	unsigned		pm_asidgen;	/* its generation number */
	struct segtab		*pm_segtab;	/* pointers to pages of PTEs */
} *pmap_t;

/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry_t, the list is pv_table.
 * XXX really should do this as a part of the higher level code.
 */
typedef struct pv_entry {
	struct pv_entry	*pv_next;	/* next pv_entry */
	struct pmap	*pv_pmap;	/* pmap where mapping lies */
	vaddr_t	pv_va;			/* virtual address for mapping */
	int		pv_flags;	/* some flags for the mapping */
} *pv_entry_t;

#define	PV_UNCACHED	0x0001		/* page is mapped uncached */
#define	PV_MODIFIED	0x0002		/* page has been modified */
#define	PV_REFERENCED	0x0004		/* page has been recently referenced */


#ifdef	_KERNEL

extern char *pmap_attributes;		/* reference and modify bits */
extern struct pmap kernel_pmap_store;

#define pmap_kernel()		(&kernel_pmap_store)
#define	pmap_wired_count(pmap) 	((pmap)->pm_stats.wired_count)
#define pmap_resident_count(pmap) ((pmap)->pm_stats.resident_count)

/*
 *	Bootstrap the system enough to run with virtual memory.
 */
void	pmap_bootstrap __P((void));

void	pmap_set_modified __P((paddr_t));

void	pmap_procwr __P((struct proc *, vaddr_t, size_t));
#define	PMAP_NEED_PROCWR

/*
 * pmap_prefer() helps reduce virtual-coherency exceptions in
 * the virtually-indexed cache on mips3 CPUs.
 */
#ifdef MIPS3
#define PMAP_PREFER(pa, va)             pmap_prefer((pa), (va))
void	pmap_prefer __P((vaddr_t, vaddr_t *));
#endif /* MIPS3 */

#define	PMAP_STEAL_MEMORY	/* enable pmap_steal_memory() */

/*
 * Alternate mapping hooks for pool pages.  Avoids thrashing the TLB.
 */
#define	PMAP_MAP_POOLPAGE(pa)	MIPS_PHYS_TO_KSEG0((pa))
#define	PMAP_UNMAP_POOLPAGE(va)	MIPS_KSEG0_TO_PHYS((va))

/*
 * Do idle page zero'ing uncached to avoid polluting the cache.
 */
void	pmap_zero_page_uncached __P((paddr_t));
#define PMAP_PAGEIDLEZERO(pa)	pmap_zero_page_uncached((pa))

/*
 * Kernel cache operations for the user-space API
 */
int mips_user_cacheflush __P((struct proc *p, vaddr_t va, int nbytes,
	int whichcache));
int mips_user_cachectl   __P((struct proc *p, vaddr_t va, int nbytes,
	int ctl));

#endif	/* _KERNEL */
#endif	/* _PMAP_MACHINE_ */
