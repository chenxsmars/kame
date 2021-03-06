/*	$NetBSD: pmap.c,v 1.6 2002/03/27 04:47:32 chs Exp $	*/

/*
 *
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * Copyright 2001 (c) Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This is the i386 pmap modified and generalized to support x86-64
 * as well. The idea is to hide the upper N levels of the page tables
 * inside pmap_get_ptp, pmap_free_ptp and pmap_growkernel. The rest
 * is mostly untouched, except that it uses some more generalized
 * macros and interfaces.
 *
 * This pmap has been tested on the i386 as well, and it can be easily
 * adapted to PAE.
 *
 * fvdl@wasabisystems.com 18-Jun-2001
 */

/*
 * pmap.c: i386 pmap module rewrite
 * Chuck Cranor <chuck@ccrc.wustl.edu>
 * 11-Aug-97
 *
 * history of this pmap module: in addition to my own input, i used
 *    the following references for this rewrite of the i386 pmap:
 *
 * [1] the NetBSD i386 pmap.   this pmap appears to be based on the
 *     BSD hp300 pmap done by Mike Hibler at University of Utah.
 *     it was then ported to the i386 by William Jolitz of UUNET
 *     Technologies, Inc.   Then Charles M. Hannum of the NetBSD
 *     project fixed some bugs and provided some speed ups.
 *
 * [2] the FreeBSD i386 pmap.   this pmap seems to be the
 *     Hibler/Jolitz pmap, as modified for FreeBSD by John S. Dyson
 *     and David Greenman.
 *
 * [3] the Mach pmap.   this pmap, from CMU, seems to have migrated
 *     between several processors.   the VAX version was done by
 *     Avadis Tevanian, Jr., and Michael Wayne Young.    the i386
 *     version was done by Lance Berc, Mike Kupfer, Bob Baron,
 *     David Golub, and Richard Draves.    the alpha version was
 *     done by Alessandro Forin (CMU/Mach) and Chris Demetriou
 *     (NetBSD/alpha).
 */

#ifndef __x86_64__
#include "opt_cputype.h"
#endif
#include "opt_user_ldt.h"
#include "opt_largepages.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/user.h>
#include <sys/kernel.h>

#include <uvm/uvm.h>

#include <machine/cpu.h>
#include <machine/specialreg.h>
#include <machine/gdt.h>

#include <dev/isa/isareg.h>
#include <machine/isa_machdep.h>

/*
 * general info:
 *
 *  - for an explanation of how the i386 MMU hardware works see
 *    the comments in <machine/pte.h>.
 *
 *  - for an explanation of the general memory structure used by
 *    this pmap (including the recursive mapping), see the comments
 *    in <machine/pmap.h>.
 *
 * this file contains the code for the "pmap module."   the module's
 * job is to manage the hardware's virtual to physical address mappings.
 * note that there are two levels of mapping in the VM system:
 *
 *  [1] the upper layer of the VM system uses vm_map's and vm_map_entry's
 *      to map ranges of virtual address space to objects/files.  for
 *      example, the vm_map may say: "map VA 0x1000 to 0x22000 read-only
 *      to the file /bin/ls starting at offset zero."   note that
 *      the upper layer mapping is not concerned with how individual
 *      vm_pages are mapped.
 *
 *  [2] the lower layer of the VM system (the pmap) maintains the mappings
 *      from virtual addresses.   it is concerned with which vm_page is
 *      mapped where.   for example, when you run /bin/ls and start
 *      at page 0x1000 the fault routine may lookup the correct page
 *      of the /bin/ls file and then ask the pmap layer to establish
 *      a mapping for it.
 *
 * note that information in the lower layer of the VM system can be
 * thrown away since it can easily be reconstructed from the info
 * in the upper layer.
 *
 * data structures we use include:
 *
 *  - struct pmap: describes the address space of one thread
 *  - struct pv_entry: describes one <PMAP,VA> mapping of a PA
 *  - struct pv_head: there is one pv_head per managed page of
 *	physical memory.   the pv_head points to a list of pv_entry
 *	structures which describe all the <PMAP,VA> pairs that this
 *      page is mapped in.    this is critical for page based operations
 *      such as pmap_page_protect() [change protection on _all_ mappings
 *      of a page]
 *  - pv_page/pv_page_info: pv_entry's are allocated out of pv_page's.
 *      if we run out of pv_entry's we allocate a new pv_page and free
 *      its pv_entrys.
 * - pmap_remove_record: a list of virtual addresses whose mappings
 *	have been changed.   used for TLB flushing.
 */

/*
 * memory allocation
 *
 *  - there are three data structures that we must dynamically allocate:
 *
 * [A] new process' page directory page (PDP)
 *	- plan 1: done at pmap_create() we use
 *	  uvm_km_alloc(kernel_map, PAGE_SIZE)  [fka kmem_alloc] to do this
 *	  allocation.
 *
 * if we are low in free physical memory then we sleep in
 * uvm_km_alloc -- in this case this is ok since we are creating
 * a new pmap and should not be holding any locks.
 *
 * if the kernel is totally out of virtual space
 * (i.e. uvm_km_alloc returns NULL), then we panic.
 *
 * XXX: the fork code currently has no way to return an "out of
 * memory, try again" error code since uvm_fork [fka vm_fork]
 * is a void function.
 *
 * [B] new page tables pages (PTP)
 * 	call uvm_pagealloc()
 * 		=> success: zero page, add to pm_pdir
 * 		=> failure: we are out of free vm_pages, let pmap_enter()
 *		   tell UVM about it.
 *
 * note: for kernel PTPs, we start with NKPTP of them.   as we map
 * kernel memory (at uvm_map time) we check to see if we've grown
 * the kernel pmap.   if so, we call the optional function
 * pmap_growkernel() to grow the kernel PTPs in advance.
 *
 * [C] pv_entry structures
 *	- plan 1: try to allocate one off the free list
 *		=> success: done!
 *		=> failure: no more free pv_entrys on the list
 *	- plan 2: try to allocate a new pv_page to add a chunk of
 *	pv_entrys to the free list
 *		[a] obtain a free, unmapped, VA in kmem_map.  either
 *		we have one saved from a previous call, or we allocate
 *		one now using a "vm_map_lock_try" in uvm_map
 *		=> success: we have an unmapped VA, continue to [b]
 *		=> failure: unable to lock kmem_map or out of VA in it.
 *			move on to plan 3.
 *		[b] allocate a page for the VA
 *		=> success: map it in, free the pv_entry's, DONE!
 *		=> failure: no free vm_pages, etc.
 *			save VA for later call to [a], go to plan 3.
 *	If we fail, we simply let pmap_enter() tell UVM about it.
 */

/*
 * locking
 *
 * we have the following locks that we must contend with:
 *
 * "normal" locks:
 *
 *  - pmap_main_lock
 *    this lock is used to prevent deadlock and/or provide mutex
 *    access to the pmap system.   most operations lock the pmap
 *    structure first, then they lock the pv_lists (if needed).
 *    however, some operations such as pmap_page_protect lock
 *    the pv_lists and then lock pmaps.   in order to prevent a
 *    cycle, we require a mutex lock when locking the pv_lists
 *    first.   thus, the "pmap = >pv_list" lockers must gain a
 *    read-lock on pmap_main_lock before locking the pmap.   and
 *    the "pv_list => pmap" lockers must gain a write-lock on
 *    pmap_main_lock before locking.    since only one thread
 *    can write-lock a lock at a time, this provides mutex.
 *
 * "simple" locks:
 *
 * - pmap lock (per pmap, part of uvm_object)
 *   this lock protects the fields in the pmap structure including
 *   the non-kernel PDEs in the PDP, and the PTEs.  it also locks
 *   in the alternate PTE space (since that is determined by the
 *   entry in the PDP).
 *
 * - pvh_lock (per pv_head)
 *   this lock protects the pv_entry list which is chained off the
 *   pv_head structure for a specific managed PA.   it is locked
 *   when traversing the list (e.g. adding/removing mappings,
 *   syncing R/M bits, etc.)
 *
 * - pvalloc_lock
 *   this lock protects the data structures which are used to manage
 *   the free list of pv_entry structures.
 *
 * - pmaps_lock
 *   this lock protects the list of active pmaps (headed by "pmaps").
 *   we lock it when adding or removing pmaps from this list.
 *
 * - pmap_copy_page_lock
 *   locks the tmp kernel PTE mappings we used to copy data
 *
 * - pmap_zero_page_lock
 *   locks the tmp kernel PTE mapping we use to zero a page
 *
 * - pmap_tmpptp_lock
 *   locks the tmp kernel PTE mapping we use to look at a PTP
 *   in another process
 *
 * XXX: would be nice to have per-CPU VAs for the above 4
 */

/*
 * locking data structures
 */

vaddr_t ptp_masks[] = PTP_MASK_INITIALIZER;
int ptp_shifts[] = PTP_SHIFT_INITIALIZER;
unsigned long nkptp[] = NKPTP_INITIALIZER;
unsigned long nkptpmax[] = NKPTPMAX_INITIALIZER;
unsigned long nbpd[] = NBPD_INITIALIZER;
pd_entry_t *normal_pdes[] = PDES_INITIALIZER;
pd_entry_t *alternate_pdes[] = APDES_INITIALIZER;

/* int nkpde = NKPTP; */

static struct lock pmap_main_lock;
static struct simplelock pvalloc_lock;
static struct simplelock pmaps_lock;
static struct simplelock pmap_copy_page_lock;
static struct simplelock pmap_zero_page_lock;
static struct simplelock pmap_tmpptp_lock;

#define PMAP_MAP_TO_HEAD_LOCK() \
     (void) spinlockmgr(&pmap_main_lock, LK_SHARED, NULL)
#define PMAP_MAP_TO_HEAD_UNLOCK() \
     (void) spinlockmgr(&pmap_main_lock, LK_RELEASE, NULL)

#define PMAP_HEAD_TO_MAP_LOCK() \
     (void) spinlockmgr(&pmap_main_lock, LK_EXCLUSIVE, NULL)
#define PMAP_HEAD_TO_MAP_UNLOCK() \
     (void) spinlockmgr(&pmap_main_lock, LK_RELEASE, NULL)

/*
 * global data structures
 */

struct pmap kernel_pmap_store;	/* the kernel's pmap (proc0) */

/*
 * nkpde is the number of kernel PTPs allocated for the kernel at
 * boot time (NKPTP is a compile time override).   this number can
 * grow dynamically as needed (but once allocated, we never free
 * kernel PTPs).
 */

/*
 * pmap_pg_g: if our processor supports PG_G in the PTE then we
 * set pmap_pg_g to PG_G (otherwise it is zero).
 */

int pmap_pg_g = 0;

#ifdef LARGEPAGES
/*
 * pmap_largepages: if our processor supports PG_PS and we are
 * using it, this is set to TRUE.
 */

int pmap_largepages;
#endif

/*
 * i386 physical memory comes in a big contig chunk with a small
 * hole toward the front of it...  the following 4 paddr_t's
 * (shared with machdep.c) describe the physical address space
 * of this machine.
 */
paddr_t avail_start;	/* PA of first available physical page */
paddr_t avail_end;	/* PA of last available physical page */

/*
 * other data structures
 */

static pt_entry_t protection_codes[8];     /* maps MI prot to i386 prot code */
static boolean_t pmap_initialized = FALSE; /* pmap_init done yet? */

/*
 * the following two vaddr_t's are used during system startup
 * to keep track of how much of the kernel's VM space we have used.
 * once the system is started, the management of the remaining kernel
 * VM space is turned over to the kernel_map vm_map.
 */

static vaddr_t virtual_avail;	/* VA of first free KVA */
static vaddr_t virtual_end;	/* VA of last free KVA */


/*
 * pv_page management structures: locked by pvalloc_lock
 */

TAILQ_HEAD(pv_pagelist, pv_page);
static struct pv_pagelist pv_freepages;	/* list of pv_pages with free entrys */
static struct pv_pagelist pv_unusedpgs; /* list of unused pv_pages */
static int pv_nfpvents;			/* # of free pv entries */
static struct pv_page *pv_initpage;	/* bootstrap page from kernel_map */
static vaddr_t pv_cachedva;		/* cached VA for later use */

#define PVE_LOWAT (PVE_PER_PVPAGE / 2)	/* free pv_entry low water mark */
#define PVE_HIWAT (PVE_LOWAT + (PVE_PER_PVPAGE * 2))
					/* high water mark */

/*
 * linked list of all non-kernel pmaps
 */

static struct pmap_head pmaps;

/*
 * pool that pmap structures are allocated from
 */

struct pool pmap_pmap_pool;

/*
 * pool and cache that PDPs are allocated from
 */

struct pool pmap_pdp_pool;
struct pool_cache pmap_pdp_cache;

int	pmap_pdp_ctor(void *, void *, int);

/*
 * special VAs and the PTEs that map them
 */

static pt_entry_t *csrc_pte, *cdst_pte, *zero_pte, *ptp_pte;
static caddr_t csrcp, cdstp, zerop, ptpp;
caddr_t vmmap; /* XXX: used by mem.c... it should really uvm_map_reserve it */

extern vaddr_t msgbuf_vaddr;
extern paddr_t msgbuf_paddr;

extern vaddr_t idt_vaddr;			/* we allocate IDT early */
extern paddr_t idt_paddr;

#if defined(I586_CPU)
/* stuff to fix the pentium f00f bug */
extern vaddr_t pentium_idt_vaddr;
#endif


/*
 * local prototypes
 */

static struct pv_entry	*pmap_add_pvpage __P((struct pv_page *, boolean_t));
static struct pv_entry	*pmap_alloc_pv __P((struct pmap *, int)); /* see codes below */
#define ALLOCPV_NEED	0	/* need PV now */
#define ALLOCPV_TRY	1	/* just try to allocate, don't steal */
#define ALLOCPV_NONEED	2	/* don't need PV, just growing cache */
static struct pv_entry	*pmap_alloc_pvpage __P((struct pmap *, int));
static void		 pmap_enter_pv __P((struct pv_head *,
					    struct pv_entry *, struct pmap *,
					    vaddr_t, struct vm_page *));
static void		 pmap_free_pv __P((struct pmap *, struct pv_entry *));
static void		 pmap_free_pvs __P((struct pmap *, struct pv_entry *));
static void		 pmap_free_pv_doit __P((struct pv_entry *));
static void		 pmap_free_pvpage __P((void));
static struct vm_page	*pmap_get_ptp __P((struct pmap *, vaddr_t,
					   pd_entry_t **));
static struct vm_page	*pmap_find_ptp __P((struct pmap *, vaddr_t, paddr_t,
					    int));
static void		 pmap_freepage __P((struct pmap *, struct vm_page *,
					    int));
static void		 pmap_free_ptp __P((struct pmap *, struct vm_page *,
					    vaddr_t, pt_entry_t *,
					    pd_entry_t **));
static boolean_t	 pmap_is_curpmap __P((struct pmap *));
static void		 pmap_map_ptes __P((struct pmap *, pt_entry_t **,
					    pd_entry_t ***));
static struct pv_entry	*pmap_remove_pv __P((struct pv_head *, struct pmap *,
					     vaddr_t));
static void		 pmap_do_remove __P((struct pmap *, vaddr_t,
						vaddr_t, int));
static boolean_t	 pmap_remove_pte __P((struct pmap *, struct vm_page *,
					      pt_entry_t *, vaddr_t, int));
static void		 pmap_remove_ptes __P((struct pmap *,
					       struct pmap_remove_record *,
					       struct vm_page *, vaddr_t,
					       vaddr_t, vaddr_t, int));
#define PMAP_REMOVE_ALL		0	/* remove all mappings */
#define PMAP_REMOVE_SKIPWIRED	1	/* skip wired mappings */
static vaddr_t		 pmap_tmpmap_pa __P((paddr_t));
static pt_entry_t	*pmap_tmpmap_pvepte __P((struct pv_entry *));
static void		 pmap_tmpunmap_pa __P((void));
static void		 pmap_tmpunmap_pvepte __P((struct pv_entry *));
static void		pmap_unmap_ptes __P((struct pmap *));
static boolean_t	pmap_get_physpage __P((vaddr_t, int, paddr_t *));
static boolean_t	pmap_pdes_valid __P((vaddr_t, pd_entry_t **,
					     pd_entry_t *));
static void		pmap_alloc_level __P((pd_entry_t **, vaddr_t, int,
					      unsigned long *));

#ifdef DIAGNOSTIC
static void		pmap_dump_obj(struct pmap *, int);
#endif
#if 0
static void 		pmap_check_ptp(struct vm_page *, int, vaddr_t);
#endif


/*
 * p m a p   i n l i n e   h e l p e r   f u n c t i o n s
 */

/*
 * pmap_is_curpmap: is this pmap the one currently loaded [in %cr3]?
 *		of course the kernel is always loaded
 */

__inline static boolean_t
pmap_is_curpmap(pmap)
	struct pmap *pmap;
{
	return((pmap == pmap_kernel()) ||
	       (pmap->pm_pdirpa == (paddr_t) rcr3()));
}

/*
 * pmap_tmpmap_pa: map a page in for tmp usage
 *
 * => returns with pmap_tmpptp_lock held
 */

__inline static vaddr_t
pmap_tmpmap_pa(pa)
	paddr_t pa;
{
	simple_lock(&pmap_tmpptp_lock);
#if defined(DIAGNOSTIC)
	if (*ptp_pte)
		panic("pmap_tmpmap_pa: ptp_pte in use?");
#endif
	*ptp_pte = PG_V | PG_RW | pa;    /* always a new mapping */
	return((vaddr_t)ptpp);
}

/*
 * pmap_tmpunmap_pa: unmap a tmp use page (undoes pmap_tmpmap_pa)
 *
 * => we release pmap_tmpptp_lock
 */

__inline static void
pmap_tmpunmap_pa()
{
#if defined(DIAGNOSTIC)
	if (!pmap_valid_entry(*ptp_pte))
		panic("pmap_tmpunmap_pa: our pte invalid?");
#endif
	*ptp_pte = 0;		/* zap! */
	pmap_update_pg((vaddr_t)ptpp);
	simple_unlock(&pmap_tmpptp_lock);
}

#if 0
static void
pmap_check_ptp(struct vm_page *ptp, int level, vaddr_t va)
{
	int i, count;
	u_int64_t *p;

	p = (u_int64_t *)pmap_tmpmap_pa(VM_PAGE_TO_PHYS(ptp));
	for (count = i = 0; i < 512; i++)
		if (p[i] != 0)
			count++;
	pmap_tmpunmap_pa();
	if (count != ptp->wire_count - 1) {
		printf("va %lx level %d: ptp bad: %d should be %d\n",
		    va, level, ptp->wire_count - 1, count);
		__asm__("xchgw %bx,%bx\n");
	}
}
#endif

/*
 * pmap_tmpmap_pvepte: get a quick mapping of a PTE for a pv_entry
 *
 * => do NOT use this on kernel mappings [why?  because pv_ptp may be NULL]
 * => we may grab pmap_tmpptp_lock and return with it held
 */

__inline static pt_entry_t *
pmap_tmpmap_pvepte(pve)
	struct pv_entry *pve;
{
#ifdef DIAGNOSTIC
	if (pve->pv_pmap == pmap_kernel())
		panic("pmap_tmpmap_pvepte: attempt to map kernel");
#endif

	/* is it current pmap?  use direct mapping... */
	if (pmap_is_curpmap(pve->pv_pmap))
		return(vtopte(pve->pv_va));

	return(((pt_entry_t *)pmap_tmpmap_pa(VM_PAGE_TO_PHYS(pve->pv_ptp)))
	       + ptei((unsigned long)pve->pv_va));
}

/*
 * pmap_tmpunmap_pvepte: release a mapping obtained with pmap_tmpmap_pvepte
 *
 * => we will release pmap_tmpptp_lock if we hold it
 */

__inline static void
pmap_tmpunmap_pvepte(pve)
	struct pv_entry *pve;
{
	/* was it current pmap?   if so, return */
	if (pmap_is_curpmap(pve->pv_pmap))
		return;

	pmap_tmpunmap_pa();
}

/*
 * pmap_map_ptes: map a pmap's PTEs into KVM and lock them in
 *
 * => we lock enough pmaps to keep things locked in
 * => must be undone with pmap_unmap_ptes before returning
 */

__inline static void
pmap_map_ptes(pmap, ptepp, pdeppp)
	struct pmap *pmap;
	pt_entry_t **ptepp;
	pd_entry_t ***pdeppp;
{
	pd_entry_t opde, npde;
	int off = 0, mcase = 0;
#if 0
	int i;
#endif

	/* the kernel's pmap is always accessible */
	if (pmap == pmap_kernel()) {
		*ptepp = PTE_BASE;
		*pdeppp = normal_pdes;
		off = PDIR_SLOT_KERN;
#if 0
		normal_pdes[PTP_LEVELS - 2] = pmap->pm_pdir;
#endif
		mcase = 1;
		return;
	}

	/* if curpmap then we are always mapped */
	if (pmap_is_curpmap(pmap)) {
		simple_lock(&pmap->pm_lock);
		*ptepp = PTE_BASE;
		*pdeppp = normal_pdes;
#if 0
		normal_pdes[PTP_LEVELS - 2] = pmap->pm_pdir;
#endif
		mcase = 2;
		return;
	}

	/* need to lock both curpmap and pmap: use ordered locking */
	if ((unsigned long) pmap < (unsigned long) curpcb->pcb_pmap) {
		simple_lock(&pmap->pm_lock);
		simple_lock(&curpcb->pcb_pmap->pm_lock);
	} else {
		simple_lock(&curpcb->pcb_pmap->pm_lock);
		simple_lock(&pmap->pm_lock);
	}

	mcase = 3;

	/* need to load a new alternate pt space into curpmap? */
	opde = *APDP_PDE;
	if (!pmap_valid_entry(opde) || (opde & PG_FRAME) != pmap->pm_pdirpa) {
		npde = (pd_entry_t) (pmap->pm_pdirpa | PG_RW | PG_V);
#if 0
		pmap->pm_pdir[PDIR_SLOT_APTE] = npde;
#endif
		*APDP_PDE = npde;
		if (pmap_valid_entry(opde))
			tlbflush();
	}
	*ptepp = APTE_BASE;
	*pdeppp = alternate_pdes;
#if 0
	alternate_pdes[PTP_LEVELS - 2] = pmap->pm_pdir;
	for (i = off; i < NTOPLEVEL_PDES - off; i++) {
		if ((*pdeppp)[0][i] != pmap->pm_pdir[i]) {
			printf("%x vs. %x at off %d\n",
			    (unsigned)(*pdeppp)[0][i], pmap->pm_pdir[i], i);
			printf("pdirpa %lx PDP_BASE %p APDP_BASE %p "
			       "pdeppp[0] %p\n", 
			    (unsigned long)pmap->pm_pdirpa, PDP_BASE, APDP_BASE,
				(*pdeppp)[0]);
			panic("pdir mismatch case %d off", mcase);
		}
	}
#endif
}

/*
 * pmap_unmap_ptes: unlock the PTE mapping of "pmap"
 */

__inline static void
pmap_unmap_ptes(pmap)
	struct pmap *pmap;
{
	if (pmap == pmap_kernel()) {
		return;
	}
	if (pmap_is_curpmap(pmap)) {
		simple_unlock(&pmap->pm_lock);
	} else {
		simple_unlock(&pmap->pm_lock);
		simple_unlock(&curpcb->pcb_pmap->pm_lock);
	}
}

/*
 * p m a p   k e n t e r   f u n c t i o n s
 *
 * functions to quickly enter/remove pages from the kernel address
 * space.   pmap_kremove is exported to MI kernel.  we make use of
 * the recursive PTE mappings.
 */

/*
 * pmap_kenter_pa: enter a kernel mapping without R/M (pv_entry) tracking
 *
 * => no need to lock anything, assume va is already allocated
 * => should be faster than normal pmap enter function
 */

void
pmap_kenter_pa(va, pa, prot)
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
{
	pt_entry_t *pte, opte;

	if (va < VM_MIN_KERNEL_ADDRESS)
		pte = vtopte(va);
	else
		pte = kvtopte(va);
	opte = *pte;
#ifdef LARGEPAGES
	/* XXX For now... */
	if (opte & PG_PS)
		panic("pmap_kenter_pa: PG_PS");
#endif
	*pte = pa | ((prot & VM_PROT_WRITE)? PG_RW : PG_RO) |
		PG_V | pmap_pg_g;	/* zap! */
	if (pmap_valid_entry(opte))
		pmap_update_pg(va);
}

/*
 * pmap_kremove: remove a kernel mapping(s) without R/M (pv_entry) tracking
 *
 * => no need to lock anything
 * => caller must dispose of any vm_page mapped in the va range
 * => note: not an inline function
 * => we assume the va is page aligned and the len is a multiple of PAGE_SIZE
 * => we assume kernel only unmaps valid addresses and thus don't bother
 *    checking the valid bit before doing TLB flushing
 */

void
pmap_kremove(va, len)
	vaddr_t va;
	vsize_t len;
{
	pt_entry_t *pte;

	len >>= PAGE_SHIFT;
	for ( /* null */ ; len ; len--, va += PAGE_SIZE) {
		if (va < VM_MIN_KERNEL_ADDRESS)
			pte = vtopte(va);
		else
			pte = kvtopte(va);
#ifdef LARGEPAGES
		/* XXX For now... */
		if (*pte & PG_PS)
			panic("pmap_kremove: PG_PS");
#endif
#ifdef DIAGNOSTIC
		if (*pte & PG_PVLIST)
			panic("pmap_kremove: PG_PVLIST mapping for 0x%lx\n",
			      va);
#endif
		*pte = 0;		/* zap! */
#if defined(I386_CPU)
		if (cpu_class != CPUCLASS_386)
#endif
			pmap_update_pg(va);
	}
#if defined(I386_CPU)
	if (cpu_class == CPUCLASS_386)
		tlbflush();
#endif
}

/*
 * p m a p   i n i t   f u n c t i o n s
 *
 * pmap_bootstrap and pmap_init are called during system startup
 * to init the pmap module.   pmap_bootstrap() does a low level
 * init just to get things rolling.   pmap_init() finishes the job.
 */

/*
 * pmap_bootstrap: get the system in a state where it can run with VM
 *	properly enabled (called before main()).   the VM system is
 *      fully init'd later...
 *
 * => on i386, locore.s has already enabled the MMU by allocating
 *	a PDP for the kernel, and nkpde PTP's for the kernel.
 * => kva_start is the first free virtual address in kernel space
 */

void
pmap_bootstrap(kva_start)
	vaddr_t kva_start;
{
	struct pmap *kpm;
	vaddr_t kva;
	pt_entry_t *pte;
	int i;
	unsigned long p1i;

	/*
	 * set up our local static global vars that keep track of the
	 * usage of KVM before kernel_map is set up
	 */

	virtual_avail = kva_start;		/* first free KVA */
	virtual_end = VM_MAX_KERNEL_ADDRESS;	/* last KVA */

	/*
	 * set up protection_codes: we need to be able to convert from
	 * a MI protection code (some combo of VM_PROT...) to something
	 * we can jam into a i386 PTE.
	 */

	protection_codes[VM_PROT_NONE] = 0;  			/* --- */
	protection_codes[VM_PROT_EXECUTE] = PG_RO;		/* --x */
	protection_codes[VM_PROT_READ] = PG_RO;			/* -r- */
	protection_codes[VM_PROT_READ|VM_PROT_EXECUTE] = PG_RO;	/* -rx */
	protection_codes[VM_PROT_WRITE] = PG_RW;		/* w-- */
	protection_codes[VM_PROT_WRITE|VM_PROT_EXECUTE] = PG_RW;/* w-x */
	protection_codes[VM_PROT_WRITE|VM_PROT_READ] = PG_RW;	/* wr- */
	protection_codes[VM_PROT_ALL] = PG_RW;			/* wrx */

	/*
	 * now we init the kernel's pmap
	 *
	 * the kernel pmap's pm_obj is not used for much.   however, in
	 * user pmaps the pm_obj contains the list of active PTPs.
	 * the pm_obj currently does not have a pager.   it might be possible
	 * to add a pager that would allow a process to read-only mmap its
	 * own page tables (fast user level vtophys?).   this may or may not
	 * be useful.
	 */

	kpm = pmap_kernel();
	for (i = 0; i < PTP_LEVELS - 1; i++) {
		simple_lock_init(&kpm->pm_obj[i].vmobjlock);
		kpm->pm_obj[i].pgops = NULL;
		TAILQ_INIT(&kpm->pm_obj[i].memq);
		kpm->pm_obj[i].uo_npages = 0;
		kpm->pm_obj[i].uo_refs = 1;
		kpm->pm_ptphint[i] = NULL;
	}
	memset(&kpm->pm_list, 0, sizeof(kpm->pm_list));  /* pm_list not used */
	kpm->pm_pdir = (pd_entry_t *)(proc0.p_addr->u_pcb.pcb_cr3 + KERNBASE);
	kpm->pm_pdirpa = (u_int32_t) proc0.p_addr->u_pcb.pcb_cr3;
	kpm->pm_stats.wired_count = kpm->pm_stats.resident_count =
		btop(kva_start - VM_MIN_KERNEL_ADDRESS);

	/*
	 * the above is just a rough estimate and not critical to the proper
	 * operation of the system.
	 */

	curpcb->pcb_pmap = kpm;	/* proc0's pcb */

	/*
	 * enable global TLB entries if they are supported
	 */

	if (cpu_feature & CPUID_PGE) {
		lcr4(rcr4() | CR4_PGE);	/* enable hardware (via %cr4) */
		pmap_pg_g = PG_G;		/* enable software */

		/* add PG_G attribute to already mapped kernel pages */
		for (kva = VM_MIN_KERNEL_ADDRESS ; kva < virtual_avail ;
		     kva += PAGE_SIZE) {
			p1i = pl1_i(kva);
			if (pmap_valid_entry(PTE_BASE[p1i]))
				PTE_BASE[p1i] |= PG_G;
		}
	}

#ifdef LARGEPAGES
	/*
	 * enable large pages of they are supported.
	 */

	if (cpu_feature & CPUID_PSE) {
		paddr_t pa;
		vaddr_t kva_end;
		pd_entry_t *pde;
		extern char _etext;

		lcr4(rcr4() | CR4_PSE);	/* enable hardware (via %cr4) */
		pmap_largepages = 1;	/* enable software */

		/*
		 * the TLB must be flushed after enabling large pages
		 * on Pentium CPUs, according to section 3.6.2.2 of
		 * "Intel Architecture Software Developer's Manual,
		 * Volume 3: System Programming".
		 */
		tlbflush();

		/*
		 * now, remap the kernel text using large pages.  we
		 * assume that the linker has properly aligned the
		 * .data segment to a 4MB boundary.
		 */
		kva_end = roundup((vaddr_t)&_etext, NBPD);
		for (pa = 0, kva = KERNBASE; kva < kva_end;
		     kva += NBPD, pa += NBPD) {
			pde = &kpm->pm_pdir[pdei(kva)];
			*pde = pa | pmap_pg_g | PG_PS |
			    PG_KR | PG_V;	/* zap! */
			tlbflush();
		}
	}
#endif /* LARGEPAGES */

	/*
	 * now we allocate the "special" VAs which are used for tmp mappings
	 * by the pmap (and other modules).    we allocate the VAs by advancing
	 * virtual_avail (note that there are no pages mapped at these VAs).
	 * we find the PTE that maps the allocated VA via the linear PTE
	 * mapping.
	 */

	pte = PTE_BASE + pl1_i(virtual_avail);

	csrcp = (caddr_t) virtual_avail;  csrc_pte = pte;	/* allocate */
	virtual_avail += PAGE_SIZE; pte++;			/* advance */

	cdstp = (caddr_t) virtual_avail;  cdst_pte = pte;
	virtual_avail += PAGE_SIZE; pte++;

	zerop = (caddr_t) virtual_avail;  zero_pte = pte;
	virtual_avail += PAGE_SIZE; pte++;

	ptpp = (caddr_t) virtual_avail;  ptp_pte = pte;
	virtual_avail += PAGE_SIZE; pte++;

	/* XXX: vmmap used by mem.c... should be uvm_map_reserve */
	vmmap = (char *)virtual_avail;			/* don't need pte */
	virtual_avail += PAGE_SIZE; pte++;

	msgbuf_vaddr = virtual_avail;			/* don't need pte */
	virtual_avail += round_page(MSGBUFSIZE); pte++;

	idt_vaddr = virtual_avail;			/* don't need pte */
	virtual_avail += 2 * PAGE_SIZE; pte += 2;
	idt_paddr = avail_start;			/* steal a page */
	avail_start += 2 * PAGE_SIZE;

#if defined(I586_CPU)
	/* pentium f00f bug stuff */
	pentium_idt_vaddr = virtual_avail;		/* don't need pte */
	virtual_avail += PAGE_SIZE; pte++;
#endif

	/*
	 * now we reserve some VM for mapping pages when doing a crash dump
	 */

	virtual_avail = reserve_dumppages(virtual_avail);

	/*
	 * init the static-global locks and global lists.
	 */

	spinlockinit(&pmap_main_lock, "pmaplk", 0);
	simple_lock_init(&pvalloc_lock);
	simple_lock_init(&pmaps_lock);
	simple_lock_init(&pmap_copy_page_lock);
	simple_lock_init(&pmap_zero_page_lock);
	simple_lock_init(&pmap_tmpptp_lock);
	LIST_INIT(&pmaps);
	TAILQ_INIT(&pv_freepages);
	TAILQ_INIT(&pv_unusedpgs);

	/*
	 * initialize the pmap pool.
	 */

	pool_init(&pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0, "pmappl",
		  &pool_allocator_nointr);

	/*
	 * initialize the PDE pool and cache.
	 */

	pool_init(&pmap_pdp_pool, PAGE_SIZE, 0, 0, 0, "pdppl",
		  &pool_allocator_nointr);
	pool_cache_init(&pmap_pdp_cache, &pmap_pdp_pool,
			pmap_pdp_ctor, NULL, NULL);

	/*
	 * ensure the TLB is sync'd with reality by flushing it...
	 */

	tlbflush();
}

/*
 * pmap_init: called from uvm_init, our job is to get the pmap
 * system ready to manage mappings... this mainly means initing
 * the pv_entry stuff.
 */

void
pmap_init()
{
	int npages, lcv, i;
	vaddr_t addr;
	vsize_t s;

	/*
	 * compute the number of pages we have and then allocate RAM
	 * for each pages' pv_head and saved attributes.
	 */

	npages = 0;
	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++)
		npages += (vm_physmem[lcv].end - vm_physmem[lcv].start);
	s = (vsize_t) (sizeof(struct pv_head) * npages +
		       sizeof(char) * npages);
	s = round_page(s); /* round up */
	addr = (vaddr_t) uvm_km_zalloc(kernel_map, s);
	if (addr == 0)
		panic("pmap_init: unable to allocate pv_heads");

	/*
	 * init all pv_head's and attrs in one memset
	 */

	/* allocate pv_head stuff first */
	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++) {
		vm_physmem[lcv].pmseg.pvhead = (struct pv_head *) addr;
		addr = (vaddr_t)(vm_physmem[lcv].pmseg.pvhead +
				 (vm_physmem[lcv].end - vm_physmem[lcv].start));
		for (i = 0;
		     i < (vm_physmem[lcv].end - vm_physmem[lcv].start); i++) {
			simple_lock_init(
			    &vm_physmem[lcv].pmseg.pvhead[i].pvh_lock);
		}
	}

	/* now allocate attrs */
	for (lcv = 0 ; lcv < vm_nphysseg ; lcv++) {
		vm_physmem[lcv].pmseg.attrs = (char *) addr;
		addr = (vaddr_t)(vm_physmem[lcv].pmseg.attrs +
				 (vm_physmem[lcv].end - vm_physmem[lcv].start));
	}

	/*
	 * now we need to free enough pv_entry structures to allow us to get
	 * the kmem_map allocated and inited (done after this
	 * function is finished).  to do this we allocate one bootstrap page out
	 * of kernel_map and use it to provide an initial pool of pv_entry
	 * structures.   we never free this page.
	 */

	pv_initpage = (struct pv_page *) uvm_km_alloc(kernel_map, PAGE_SIZE);
	if (pv_initpage == NULL)
		panic("pmap_init: pv_initpage");
	pv_cachedva = 0;   /* a VA we have allocated but not used yet */
	pv_nfpvents = 0;
	(void) pmap_add_pvpage(pv_initpage, FALSE);

	/*
	 * done: pmap module is up (and ready for business)
	 */

	pmap_initialized = TRUE;
}

/*
 * p v _ e n t r y   f u n c t i o n s
 */

/*
 * pv_entry allocation functions:
 *   the main pv_entry allocation functions are:
 *     pmap_alloc_pv: allocate a pv_entry structure
 *     pmap_free_pv: free one pv_entry
 *     pmap_free_pvs: free a list of pv_entrys
 *
 * the rest are helper functions
 */

/*
 * pmap_alloc_pv: inline function to allocate a pv_entry structure
 * => we lock pvalloc_lock
 * => if we fail, we call out to pmap_alloc_pvpage
 * => 3 modes:
 *    ALLOCPV_NEED   = we really need a pv_entry, even if we have to steal it
 *    ALLOCPV_TRY    = we want a pv_entry, but not enough to steal
 *    ALLOCPV_NONEED = we are trying to grow our free list, don't really need
 *			one now
 *
 * "try" is for optional functions like pmap_copy().
 */

__inline static struct pv_entry *
pmap_alloc_pv(pmap, mode)
	struct pmap *pmap;
	int mode;
{
	struct pv_page *pvpage;
	struct pv_entry *pv;

	simple_lock(&pvalloc_lock);

	if (pv_freepages.tqh_first != NULL) {
		pvpage = pv_freepages.tqh_first;
		pvpage->pvinfo.pvpi_nfree--;
		if (pvpage->pvinfo.pvpi_nfree == 0) {
			/* nothing left in this one? */
			TAILQ_REMOVE(&pv_freepages, pvpage, pvinfo.pvpi_list);
		}
		pv = pvpage->pvinfo.pvpi_pvfree;
#ifdef DIAGNOSTIC
		if (pv == NULL)
			panic("pmap_alloc_pv: pvpi_nfree off");
#endif
		pvpage->pvinfo.pvpi_pvfree = pv->pv_next;
		pv_nfpvents--;  /* took one from pool */
	} else {
		pv = NULL;		/* need more of them */
	}

	/*
	 * if below low water mark or we didn't get a pv_entry we try and
	 * create more pv_entrys ...
	 */

	if (pv_nfpvents < PVE_LOWAT || pv == NULL) {
		if (pv == NULL)
			pv = pmap_alloc_pvpage(pmap, (mode == ALLOCPV_TRY) ?
					       mode : ALLOCPV_NEED);
		else
			(void) pmap_alloc_pvpage(pmap, ALLOCPV_NONEED);
	}

	simple_unlock(&pvalloc_lock);
	return(pv);
}

/*
 * pmap_alloc_pvpage: maybe allocate a new pvpage
 *
 * if need_entry is false: try and allocate a new pv_page
 * if need_entry is true: try and allocate a new pv_page and return a
 *	new pv_entry from it.   if we are unable to allocate a pv_page
 *	we make a last ditch effort to steal a pv_page from some other
 *	mapping.    if that fails, we panic...
 *
 * => we assume that the caller holds pvalloc_lock
 */

static struct pv_entry *
pmap_alloc_pvpage(pmap, mode)
	struct pmap *pmap;
	int mode;
{
	struct vm_page *pg;
	struct pv_page *pvpage;
	struct pv_entry *pv;
	int s;

	/*
	 * if we need_entry and we've got unused pv_pages, allocate from there
	 */

	if (mode != ALLOCPV_NONEED && pv_unusedpgs.tqh_first != NULL) {

		/* move it to pv_freepages list */
		pvpage = pv_unusedpgs.tqh_first;
		TAILQ_REMOVE(&pv_unusedpgs, pvpage, pvinfo.pvpi_list);
		TAILQ_INSERT_HEAD(&pv_freepages, pvpage, pvinfo.pvpi_list);

		/* allocate a pv_entry */
		pvpage->pvinfo.pvpi_nfree--;	/* can't go to zero */
		pv = pvpage->pvinfo.pvpi_pvfree;
#ifdef DIAGNOSTIC
		if (pv == NULL)
			panic("pmap_alloc_pvpage: pvpi_nfree off");
#endif
		pvpage->pvinfo.pvpi_pvfree = pv->pv_next;

		pv_nfpvents--;  /* took one from pool */
		return(pv);
	}

	/*
	 *  see if we've got a cached unmapped VA that we can map a page in.
	 * if not, try to allocate one.
	 */

	if (pv_cachedva == 0) {
		s = splvm();   /* must protect kmem_map with splvm! */
		pv_cachedva = uvm_km_kmemalloc(kmem_map, NULL,
		    PAGE_SIZE, UVM_KMF_TRYLOCK|UVM_KMF_VALLOC);
		splx(s);
		if (pv_cachedva == 0) {
			return (NULL);
		}
	}

	/*
	 * we have a VA, now let's try and allocate a page.
	 */

	pg = uvm_pagealloc(NULL, pv_cachedva - vm_map_min(kernel_map), NULL,
	    UVM_PGA_USERESERVE);
	if (pg)
		pg->flags &= ~PG_BUSY;	/* never busy */
	splx(s);

	if (pg == NULL)
		return (NULL);

	/*
	 * add a mapping for our new pv_page and free its entrys (save one!)
	 *
	 * NOTE: If we are allocating a PV page for the kernel pmap, the
	 * pmap is already locked!  (...but entering the mapping is safe...)
	 */

	pmap_kenter_pa(pv_cachedva, VM_PAGE_TO_PHYS(pg), VM_PROT_ALL);
	pmap_update(pmap_kernel());
	pvpage = (struct pv_page *)pv_cachedva;
	pv_cachedva = 0;
	return (pmap_add_pvpage(pvpage, mode != ALLOCPV_NONEED));
}

/*
 * pmap_add_pvpage: add a pv_page's pv_entrys to the free list
 *
 * => caller must hold pvalloc_lock
 * => if need_entry is true, we allocate and return one pv_entry
 */

static struct pv_entry *
pmap_add_pvpage(pvp, need_entry)
	struct pv_page *pvp;
	boolean_t need_entry;
{
	int tofree, lcv;

	/* do we need to return one? */
	tofree = (need_entry) ? PVE_PER_PVPAGE - 1 : PVE_PER_PVPAGE;

	pvp->pvinfo.pvpi_pvfree = NULL;
	pvp->pvinfo.pvpi_nfree = tofree;
	for (lcv = 0 ; lcv < tofree ; lcv++) {
		pvp->pvents[lcv].pv_next = pvp->pvinfo.pvpi_pvfree;
		pvp->pvinfo.pvpi_pvfree = &pvp->pvents[lcv];
	}
	if (need_entry)
		TAILQ_INSERT_TAIL(&pv_freepages, pvp, pvinfo.pvpi_list);
	else
		TAILQ_INSERT_TAIL(&pv_unusedpgs, pvp, pvinfo.pvpi_list);
	pv_nfpvents += tofree;
	return((need_entry) ? &pvp->pvents[lcv] : NULL);
}

/*
 * pmap_free_pv_doit: actually free a pv_entry
 *
 * => do not call this directly!  instead use either
 *    1. pmap_free_pv ==> free a single pv_entry
 *    2. pmap_free_pvs => free a list of pv_entrys
 * => we must be holding pvalloc_lock
 */

__inline static void
pmap_free_pv_doit(pv)
	struct pv_entry *pv;
{
	struct pv_page *pvp;

	pvp = (struct pv_page *) x86_trunc_page(pv);
	pv_nfpvents++;
	pvp->pvinfo.pvpi_nfree++;

	/* nfree == 1 => fully allocated page just became partly allocated */
	if (pvp->pvinfo.pvpi_nfree == 1) {
		TAILQ_INSERT_HEAD(&pv_freepages, pvp, pvinfo.pvpi_list);
	}

	/* free it */
	pv->pv_next = pvp->pvinfo.pvpi_pvfree;
	pvp->pvinfo.pvpi_pvfree = pv;

	/*
	 * are all pv_page's pv_entry's free?  move it to unused queue.
	 */

	if (pvp->pvinfo.pvpi_nfree == PVE_PER_PVPAGE) {
		TAILQ_REMOVE(&pv_freepages, pvp, pvinfo.pvpi_list);
		TAILQ_INSERT_HEAD(&pv_unusedpgs, pvp, pvinfo.pvpi_list);
	}
}

/*
 * pmap_free_pv: free a single pv_entry
 *
 * => we gain the pvalloc_lock
 */

__inline static void
pmap_free_pv(pmap, pv)
	struct pmap *pmap;
	struct pv_entry *pv;
{
	simple_lock(&pvalloc_lock);
	pmap_free_pv_doit(pv);

	/*
	 * Can't free the PV page if the PV entries were associated with
	 * the kernel pmap; the pmap is already locked.
	 */
	if (pv_nfpvents > PVE_HIWAT && pv_unusedpgs.tqh_first != NULL &&
	    pmap != pmap_kernel())
		pmap_free_pvpage();

	simple_unlock(&pvalloc_lock);
}

/*
 * pmap_free_pvs: free a list of pv_entrys
 *
 * => we gain the pvalloc_lock
 */

__inline static void
pmap_free_pvs(pmap, pvs)
	struct pmap *pmap;
	struct pv_entry *pvs;
{
	struct pv_entry *nextpv;

	simple_lock(&pvalloc_lock);

	for ( /* null */ ; pvs != NULL ; pvs = nextpv) {
		nextpv = pvs->pv_next;
		pmap_free_pv_doit(pvs);
	}

	/*
	 * Can't free the PV page if the PV entries were associated with
	 * the kernel pmap; the pmap is already locked.
	 */
	if (pv_nfpvents > PVE_HIWAT && pv_unusedpgs.tqh_first != NULL &&
	    pmap != pmap_kernel())
		pmap_free_pvpage();

	simple_unlock(&pvalloc_lock);
}


/*
 * pmap_free_pvpage: try and free an unused pv_page structure
 *
 * => assume caller is holding the pvalloc_lock and that
 *	there is a page on the pv_unusedpgs list
 * => if we can't get a lock on the kmem_map we try again later
 */

static void
pmap_free_pvpage()
{
	int s;
	struct vm_map *map;
	struct vm_map_entry *dead_entries;
	struct pv_page *pvp;

	s = splvm(); /* protect kmem_map */
	pvp = TAILQ_FIRST(&pv_unusedpgs);

	/*
	 * note: watch out for pv_initpage which is allocated out of
	 * kernel_map rather than kmem_map.
	 */

	if (pvp == pv_initpage)
		map = kernel_map;
	else
		map = kmem_map;
	if (vm_map_lock_try(map)) {

		/* remove pvp from pv_unusedpgs */
		TAILQ_REMOVE(&pv_unusedpgs, pvp, pvinfo.pvpi_list);

		/* unmap the page */
		dead_entries = NULL;
		uvm_unmap_remove(map, (vaddr_t)pvp, ((vaddr_t)pvp) + PAGE_SIZE,
		    &dead_entries);
		vm_map_unlock(map);

		if (dead_entries != NULL)
			uvm_unmap_detach(dead_entries, 0);

		pv_nfpvents -= PVE_PER_PVPAGE;  /* update free count */
	}
	if (pvp == pv_initpage)
		/* no more initpage, we've freed it */
		pv_initpage = NULL;

	splx(s);
}

/*
 * main pv_entry manipulation functions:
 *   pmap_enter_pv: enter a mapping onto a pv_head list
 *   pmap_remove_pv: remove a mappiing from a pv_head list
 *
 * NOTE: pmap_enter_pv expects to lock the pvh itself
 *       pmap_remove_pv expects te caller to lock the pvh before calling
 */

/*
 * pmap_enter_pv: enter a mapping onto a pv_head lst
 *
 * => caller should hold the proper lock on pmap_main_lock
 * => caller should have pmap locked
 * => we will gain the lock on the pv_head and allocate the new pv_entry
 * => caller should adjust ptp's wire_count before calling
 */

__inline static void
pmap_enter_pv(pvh, pve, pmap, va, ptp)
	struct pv_head *pvh;
	struct pv_entry *pve;	/* preallocated pve for us to use */
	struct pmap *pmap;
	vaddr_t va;
	struct vm_page *ptp;	/* PTP in pmap that maps this VA */
{
	pve->pv_pmap = pmap;
	pve->pv_va = va;
	pve->pv_ptp = ptp;			/* NULL for kernel pmap */
	simple_lock(&pvh->pvh_lock);		/* lock pv_head */
	pve->pv_next = pvh->pvh_list;		/* add to ... */
	pvh->pvh_list = pve;			/* ... locked list */
	simple_unlock(&pvh->pvh_lock);		/* unlock, done! */
}

/*
 * pmap_remove_pv: try to remove a mapping from a pv_list
 *
 * => caller should hold proper lock on pmap_main_lock
 * => pmap should be locked
 * => caller should hold lock on pv_head [so that attrs can be adjusted]
 * => caller should adjust ptp's wire_count and free PTP if needed
 * => we return the removed pve
 */

__inline static struct pv_entry *
pmap_remove_pv(pvh, pmap, va)
	struct pv_head *pvh;
	struct pmap *pmap;
	vaddr_t va;
{
	struct pv_entry *pve, **prevptr;

	prevptr = &pvh->pvh_list;		/* previous pv_entry pointer */
	pve = *prevptr;
	while (pve) {
		if (pve->pv_pmap == pmap && pve->pv_va == va) {	/* match? */
			*prevptr = pve->pv_next;		/* remove it! */
			break;
		}
		prevptr = &pve->pv_next;		/* previous pointer */
		pve = pve->pv_next;			/* advance */
	}
	return(pve);				/* return removed pve */
}

/*
 * p t p   f u n c t i o n s
 */

static __inline struct vm_page *
pmap_find_ptp(struct pmap *pmap, vaddr_t va, paddr_t pa, int level)
{
#if 1
	int lidx = level - 1;

	if (pa != (paddr_t)-1 && pmap->pm_ptphint[lidx] &&
	    pa == VM_PAGE_TO_PHYS(pmap->pm_ptphint[lidx])) {
		return (pmap->pm_ptphint[lidx]);
	}
	return uvm_pagelookup(&pmap->pm_obj[lidx], ptp_va2o(va, level));
#else
	return PHYS_TO_VM_PAGE(pa);
#endif
}

static __inline void
pmap_freepage(struct pmap *pmap, struct vm_page *ptp, int level)
{
	int lidx;

	lidx = level - 1;

	pmap->pm_stats.resident_count--;
	if (pmap->pm_ptphint[lidx] == ptp)
		pmap->pm_ptphint[lidx] = pmap->pm_obj[lidx].memq.tqh_first;
	ptp->wire_count = 0;
	uvm_pagefree(ptp);
}

static void
pmap_free_ptp(struct pmap *pmap, struct vm_page *ptp, vaddr_t va,
	      pt_entry_t *ptes, pd_entry_t **pdes)
{
	unsigned long index;
	int level;
	vaddr_t invaladdr;
	level = 1;

	do {
		pmap_freepage(pmap, ptp, level);
		index = pl_i(va, level + 1);
		pdes[level - 1][index] = 0;
#if defined(I386_CPU)
		if (cpu_class != CPUCLASS_386)
#endif
		{
			invaladdr = level == 1 ? (vaddr_t)ptes :
			    (vaddr_t)pdes[level - 2];
			pmap_update_pg(invaladdr + index * NBPG);
		}
		if (level < PTP_LEVELS - 1) {
			ptp = pmap_find_ptp(pmap, va, (paddr_t)-1, level + 1);
			ptp->wire_count--;
#if 0
			pmap_check_ptp(ptp, level, va);
#endif
			if (ptp->wire_count > 1)
				break;
		}
	} while (++level < PTP_LEVELS);
}

/*
 * pmap_get_ptp: get a PTP (if there isn't one, allocate a new one)
 *
 * => pmap should NOT be pmap_kernel()
 * => pmap should be locked
 */


static struct vm_page *
pmap_get_ptp(struct pmap *pmap, vaddr_t va, pd_entry_t **pdes)
{
	struct vm_page *ptp, *pptp;
	int i;
	unsigned long index;
	pd_entry_t *pva;
	paddr_t ppa, pa;

	ptp = NULL;
	pa = (paddr_t)-1;

	/*
	 * Loop through all page table levels seeing if we need to
	 * add a new page to that level.
	 */
	for (i = PTP_LEVELS; i > 1; i--) {
		/*
		 * Save values from previous round.
		 */
		pptp = ptp;
		ppa = pa;

		index = pl_i(va, i);
		pva = pdes[i - 2];

		if (pmap_valid_entry(pva[index])) {
			ppa = pva[index] & PG_FRAME;
			ptp = NULL;
			continue;
		}

		ptp = uvm_pagealloc(&pmap->pm_obj[i-2],
		    ptp_va2o(va, i - 1), NULL,
		    UVM_PGA_USERESERVE|UVM_PGA_ZERO);

		if (ptp == NULL)
			return NULL;

		ptp->flags &= ~PG_BUSY; /* never busy */
		ptp->wire_count = 1;
		pmap->pm_ptphint[i - 2] = ptp;
		pa = VM_PAGE_TO_PHYS(ptp);
		pva[index] = (pd_entry_t) (pa | PG_u | PG_RW | PG_V);
		pmap->pm_stats.resident_count++;
		/*
		 * If we're not in the top level, increase the
		 * wire count of the parent page.
		 */
		if (i < PTP_LEVELS) {
			if (pptp == NULL)
				pptp = pmap_find_ptp(pmap, va, ppa, i);
#ifdef DIAGNOSTIC
			if (pptp == NULL)
				panic("pde page disappeared");
#endif
			pptp->wire_count++;
#if 0
			pmap_check_ptp(pptp, i, va);
#endif
		}
	}

	/*
	 * ptp is not NULL if we just allocated a new ptp. If it's
	 * still NULL, we must look up the existing one.
	 */
	if (ptp == NULL) {
		ptp = pmap_find_ptp(pmap, va, ppa, 1);
#ifdef DIAGNOSTIC
		if (ptp == NULL) {
			pmap_dump_obj(pmap, 1);
			printf("va %lx ppa %lx\n", (unsigned long)va,
			    (unsigned long)ppa);
			panic("pmap_get_ptp: unmanaged user PTP");
		}
#if 0
		pmap_check_ptp(ptp, 1, va);
#endif
#endif
	}

	pmap->pm_ptphint[0] = ptp;
	return(ptp);
}

/*
 * p m a p  l i f e c y c l e   f u n c t i o n s
 */

/*
 * pmap_pdp_ctor: constructor for the PDP cache.
 */

int
pmap_pdp_ctor(void *arg, void *object, int flags)
{
	pd_entry_t *pdir = object;
	paddr_t pdirpa;
	int npde;

	/*
	 * NOTE: The `pmap_lock' is held when the PDP is allocated.
	 * WE MUST NOT BLOCK!
	 */

	/* fetch the physical address of the page directory. */
	(void) pmap_extract(pmap_kernel(), (vaddr_t) pdir, &pdirpa);

	/* zero init area */
	memset(pdir, 0, PDIR_SLOT_PTE * sizeof(pd_entry_t));

	/* put in recursibve PDE to map the PTEs */
	pdir[PDIR_SLOT_PTE] = pdirpa | PG_V | PG_KW;

	npde = nkptp[PTP_LEVELS - 1];

	/* put in kernel VM PDEs */
	memcpy(&pdir[PDIR_SLOT_KERN], &PDP_BASE[PDIR_SLOT_KERN],
	    npde * sizeof(pd_entry_t));

	/* zero the rest */
	memset(&pdir[PDIR_SLOT_KERN + npde], 0,
	    (NTOPLEVEL_PDES - (PDIR_SLOT_KERN + npde)) * sizeof(pd_entry_t));

	return (0);
}

/*
 * pmap_create: create a pmap
 *
 * => note: old pmap interface took a "size" args which allowed for
 *	the creation of "software only" pmaps (not in bsd).
 */

struct pmap *
pmap_create()
{
	struct pmap *pmap;
	int i;

	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK);

	/* init uvm_object */
	for (i = 0; i < PTP_LEVELS - 1; i++) {
		simple_lock_init(&pmap->pm_obj[i].vmobjlock);
		pmap->pm_obj[i].pgops = NULL;	/* not a mappable object */
		TAILQ_INIT(&pmap->pm_obj[i].memq);
		pmap->pm_obj[i].uo_npages = 0;
		pmap->pm_obj[i].uo_refs = 1;
		pmap->pm_ptphint[i] = NULL;
	}
	pmap->pm_stats.wired_count = 0;
	pmap->pm_stats.resident_count = 1;	/* count the PDP allocd below */
	pmap->pm_flags = 0;

	/* init the LDT */
	pmap->pm_ldt = NULL;
	pmap->pm_ldt_len = 0;
	pmap->pm_ldt_sel = GSYSSEL(GLDT_SEL, SEL_KPL);

	/* allocate PDP */

	/*
	 * we need to lock pmaps_lock to prevent nkpde from changing on
	 * us.  note that there is no need to splvm to protect us from
	 * malloc since malloc allocates out of a submap and we should
	 * have already allocated kernel PTPs to cover the range...
	 *
	 * NOTE: WE MUST NOT BLOCK WHILE HOLDING THE `pmap_lock'!
	 */

	simple_lock(&pmaps_lock);

	/* XXX Need a generic "I want memory" wchan */
	while ((pmap->pm_pdir =
	    pool_cache_get(&pmap_pdp_cache, PR_NOWAIT)) == NULL)
		(void) ltsleep(&lbolt, PVM, "pmapcr", hz >> 3, &pmaps_lock);

	pmap->pm_pdirpa = pmap->pm_pdir[PDIR_SLOT_PTE] & PG_FRAME;

	LIST_INSERT_HEAD(&pmaps, pmap, pm_list);

	simple_unlock(&pmaps_lock);

	return (pmap);
}

/*
 * pmap_destroy: drop reference count on pmap.   free pmap if
 *	reference count goes to zero.
 */

void
pmap_destroy(pmap)
	struct pmap *pmap;
{
	struct vm_page *pg;
	int refs;
	int i;

	/*
	 * drop reference count
	 */

	simple_lock(&pmap->pm_lock);
	refs = --pmap->pm_obj[0].uo_refs;
	simple_unlock(&pmap->pm_lock);
	if (refs > 0) {
		return;
	}

	/*
	 * reference count is zero, free pmap resources and then free pmap.
	 */

	/*
	 * remove it from global list of pmaps
	 */

	simple_lock(&pmaps_lock);
	LIST_REMOVE(pmap, pm_list);
	simple_unlock(&pmaps_lock);

	/*
	 * free any remaining PTPs
	 */

	for (i = 0; i < PTP_LEVELS - 1; i++) {
		while (pmap->pm_obj[i].memq.tqh_first != NULL) {
			pg = pmap->pm_obj[i].memq.tqh_first;
#ifdef DIAGNOSTIC
			if (pg->flags & PG_BUSY)
				panic("pmap_release: busy page table page");
#endif
			/* pmap_page_protect?  currently no need for it. */

			pg->wire_count = 0;
			uvm_pagefree(pg);
		}
	}

	/* XXX: need to flush it out of other processor's APTE space? */
	pool_cache_put(&pmap_pdp_cache, pmap->pm_pdir);

#ifdef USER_LDT
	if (pmap->pm_flags & PMF_USER_LDT) {
		/*
		 * no need to switch the LDT; this address space is gone,
		 * nothing is using it.
		 */
		ldt_free(pmap);
		uvm_km_free(kernel_map, (vaddr_t)pmap->pm_ldt,
			    pmap->pm_ldt_len);
	}
#endif

	pool_put(&pmap_pmap_pool, pmap);
}

/*
 *	Add a reference to the specified pmap.
 */

void
pmap_reference(pmap)
	struct pmap *pmap;
{
	simple_lock(&pmap->pm_lock);
	pmap->pm_obj[0].uo_refs++;
	simple_unlock(&pmap->pm_lock);
}

#if defined(PMAP_FORK)
/*
 * pmap_fork: perform any necessary data structure manipulation when
 * a VM space is forked.
 */

void
pmap_fork(pmap1, pmap2)
	struct pmap *pmap1, *pmap2;
{
	simple_lock(&pmap1->pm_lock);
	simple_lock(&pmap2->pm_lock);

#ifdef USER_LDT
	/* Copy the LDT, if necessary. */
	if (pmap1->pm_flags & PMF_USER_LDT) {
		char *new_ldt;
		size_t len;

		len = pmap1->pm_ldt_len;
		new_ldt = (char *)uvm_km_alloc(kernel_map, len);
		memcpy(new_ldt, pmap1->pm_ldt, len);
		pmap2->pm_ldt = new_ldt;
		pmap2->pm_ldt_len = pmap1->pm_ldt_len;
		pmap2->pm_flags |= PMF_USER_LDT;
		ldt_alloc(pmap2, new_ldt, len);
	}
#endif /* USER_LDT */

	simple_unlock(&pmap2->pm_lock);
	simple_unlock(&pmap1->pm_lock);
}
#endif /* PMAP_FORK */

#ifdef USER_LDT
/*
 * pmap_ldt_cleanup: if the pmap has a local LDT, deallocate it, and
 * restore the default.
 */

void
pmap_ldt_cleanup(p)
	struct proc *p;
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	pmap_t pmap = p->p_vmspace->vm_map.pmap;
	char *old_ldt = NULL;
	size_t len = 0;

	simple_lock(&pmap->pm_lock);

	if (pmap->pm_flags & PMF_USER_LDT) {
		ldt_free(pmap);
		pmap->pm_ldt_sel = GSYSSEL(GLDT_SEL, SEL_KPL);
		pcb->pcb_ldt_sel = pmap->pm_ldt_sel;
		if (pcb == curpcb)
			lldt(pcb->pcb_ldt_sel);
		old_ldt = pmap->pm_ldt;
		len = pmap->pm_ldt_len;
		pmap->pm_ldt = NULL;
		pmap->pm_ldt_len = 0;
		pmap->pm_flags &= ~PMF_USER_LDT;
	}

	simple_unlock(&pmap->pm_lock);

	if (old_ldt != NULL)
		uvm_km_free(kernel_map, (vaddr_t)old_ldt, len);
}
#endif /* USER_LDT */

/*
 * pmap_activate: activate a process' pmap (fill in %cr3 and LDT info)
 *
 * => called from cpu_switch()
 * => if proc is the curproc, then load it into the MMU
 */

void
pmap_activate(p)
	struct proc *p;
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct pmap *pmap = p->p_vmspace->vm_map.pmap;

	pcb->pcb_pmap = pmap;
	pcb->pcb_ldt_sel = pmap->pm_ldt_sel;
	pcb->pcb_cr3 = pmap->pm_pdirpa;
	if (p == curproc) {
#if 0
		printf("pmap_activate: pid %d cr3 %lx\n", p->p_pid,
		    pcb->pcb_cr3);
#endif
		lcr3(pcb->pcb_cr3);
	}
	if (pcb == curpcb)
		lldt(pcb->pcb_ldt_sel);
}

/*
 * pmap_deactivate: deactivate a process' pmap
 *
 * => XXX: what should this do, if anything?
 */

void
pmap_deactivate(p)
	struct proc *p;
{
}

/*
 * end of lifecycle functions
 */

/*
 * some misc. functions
 */

static boolean_t
pmap_pdes_valid(vaddr_t va, pd_entry_t **pdes, pd_entry_t *lastpde)
{
	int i;
	unsigned long index;
	pd_entry_t pde;

	for (i = PTP_LEVELS; i > 1; i--) {
		index = pl_i(va, i);
		pde = pdes[i - 2][index];
		if ((pde & PG_V) == 0)
			return FALSE;
	}
	if (lastpde != NULL)
		*lastpde = pde;
	return TRUE;
}

/*
 * pmap_extract: extract a PA for the given VA
 */

boolean_t
pmap_extract(pmap, va, pap)
	struct pmap *pmap;
	vaddr_t va;
	paddr_t *pap;
{
	pt_entry_t *ptes, pte;
	pd_entry_t pde, **pdes;

	pmap_map_ptes(pmap, &ptes, &pdes);
	if (pmap_pdes_valid(va, pdes, &pde) == FALSE) {
		pmap_unmap_ptes(pmap);
		return FALSE;
	}
	pte = ptes[pl1_i(va)];
	pmap_unmap_ptes(pmap);

#ifdef LARGEPAGES
	if (pde & PG_PS) {
		if (pap != NULL)
			*pap = (pde & PG_LGFRAME) | (va & ~PG_LGFRAME);
		return (TRUE);
	}
#endif


	if (__predict_true((pte & PG_V) != 0)) {
		if (pap != NULL)
			*pap = (pte & PG_FRAME) | (va & ~PG_FRAME);
		return (TRUE);
	}

	return FALSE;
}


/*
 * vtophys: virtual address to physical address.  For use by
 * machine-dependent code only.
 */

paddr_t
vtophys(va)
	vaddr_t va;
{
	paddr_t pa;

	if (pmap_extract(pmap_kernel(), va, &pa) == TRUE)
		return (pa);
	return (0);
}


/*
 * pmap_virtual_space: used during bootup [pmap_steal_memory] to
 *	determine the bounds of the kernel virtual addess space.
 */

void
pmap_virtual_space(startp, endp)
	vaddr_t *startp;
	vaddr_t *endp;
{
	*startp = virtual_avail;
	*endp = virtual_end;
}

/*
 * pmap_map: map a range of PAs into kvm
 *
 * => used during crash dump
 * => XXX: pmap_map() should be phased out?
 */

vaddr_t
pmap_map(va, spa, epa, prot)
	vaddr_t va;
	paddr_t spa, epa;
	vm_prot_t prot;
{
	while (spa < epa) {
		pmap_enter(pmap_kernel(), va, spa, prot, 0);
		va += PAGE_SIZE;
		spa += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
	return va;
}

/*
 * pmap_zero_page: zero a page
 */

void
pmap_zero_page(pa)
	paddr_t pa;
{

	simple_lock(&pmap_zero_page_lock);
	*zero_pte = (pa & PG_FRAME) | PG_V | PG_RW;	/* map in */
	pmap_update_pg((vaddr_t)zerop);			/* flush TLB */
	memset(zerop, 0, PAGE_SIZE);			/* zero */
	simple_unlock(&pmap_zero_page_lock);
}

/*
 * pmap_pageidlezero: the same, for the idle loop page zero'er.
 * Returns TRUE if the page was zero'd, FALSE if we aborted for
 * some reason.
 */

boolean_t
pmap_pageidlezero(pa)
	paddr_t pa;
{
	boolean_t rv = TRUE;
	int i, *ptr;

	simple_lock(&pmap_zero_page_lock);

	*zero_pte = (pa & PG_FRAME) | PG_V | PG_RW;	/* map in */
	pmap_update_pg((vaddr_t)zerop);			/* flush TLB */

	for (i = 0, ptr = (int *) zerop; i < PAGE_SIZE / sizeof(int); i++) {
		if (sched_whichqs != 0) {
			/*
			 * A process has become ready.  Abort now,
			 * so we don't keep it waiting while we
			 * do slow memory access to finish this
			 * page.
			 */
			rv = FALSE;
			break;
		}
		*ptr++ = 0;
	}

	simple_unlock(&pmap_zero_page_lock);

	return (rv);
}

/*
 * pmap_copy_page: copy a page
 */

void
pmap_copy_page(srcpa, dstpa)
	paddr_t srcpa, dstpa;
{
	simple_lock(&pmap_copy_page_lock);
#ifdef DIAGNOSTIC
	if (*csrc_pte || *cdst_pte)
		panic("pmap_copy_page: lock botch");
#endif

	*csrc_pte = (srcpa & PG_FRAME) | PG_V | PG_RW;
	*cdst_pte = (dstpa & PG_FRAME) | PG_V | PG_RW;
	memcpy(cdstp, csrcp, PAGE_SIZE);
	*csrc_pte = *cdst_pte = 0;			/* zap! */
	pmap_update_2pg((vaddr_t)csrcp, (vaddr_t)cdstp);
	simple_unlock(&pmap_copy_page_lock);
}

/*
 * p m a p   r e m o v e   f u n c t i o n s
 *
 * functions that remove mappings
 */

/*
 * pmap_remove_ptes: remove PTEs from a PTP
 *
 * => must have proper locking on pmap_master_lock
 * => caller must hold pmap's lock
 * => PTP must be mapped into KVA
 * => PTP should be null if pmap == pmap_kernel()
 */

static void
pmap_remove_ptes(pmap, pmap_rr, ptp, ptpva, startva, endva, flags)
	struct pmap *pmap;
	struct pmap_remove_record *pmap_rr;
	struct vm_page *ptp;
	vaddr_t ptpva;
	vaddr_t startva, endva;
	int flags;
{
	struct pv_entry *pv_tofree = NULL;	/* list of pv_entrys to free */
	struct pv_entry *pve;
	pt_entry_t *pte = (pt_entry_t *) ptpva;
	pt_entry_t opte;
	int bank, off;

	/*
	 * note that ptpva points to the PTE that maps startva.   this may
	 * or may not be the first PTE in the PTP.
	 *
	 * we loop through the PTP while there are still PTEs to look at
	 * and the wire_count is greater than 1 (because we use the wire_count
	 * to keep track of the number of real PTEs in the PTP).
	 */

	for (/*null*/; startva < endva && (ptp == NULL || ptp->wire_count > 1)
			     ; pte++, startva += PAGE_SIZE) {
		if (!pmap_valid_entry(*pte))
			continue;			/* VA not mapped */
		if ((flags & PMAP_REMOVE_SKIPWIRED) && (*pte & PG_W)) {
			continue;
		}

		opte = *pte;		/* save the old PTE */
		*pte = 0;			/* zap! */
		if (opte & PG_W)
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;

		if (pmap_rr) {		/* worried about tlb flushing? */
			if (opte & PG_G) {
				/* PG_G requires this */
				pmap_update_pg(startva);
			} else {
				if (pmap_rr->prr_npages < PMAP_RR_MAX) {
					pmap_rr->prr_vas[pmap_rr->prr_npages++]
						= startva;
				} else {
					if (pmap_rr->prr_npages == PMAP_RR_MAX)
						/* signal an overflow */
						pmap_rr->prr_npages++;
				}
			}
		}
		if (ptp)
			ptp->wire_count--;		/* dropping a PTE */

		/*
		 * if we are not on a pv_head list we are done.
		 */

		if ((opte & PG_PVLIST) == 0) {
#ifdef DIAGNOSTIC
			if (vm_physseg_find(btop(opte & PG_FRAME), &off)
			    != -1)
				panic("pmap_remove_ptes: managed page without "
				      "PG_PVLIST for 0x%lx", startva);
#endif
			continue;
		}

		bank = vm_physseg_find(btop(opte & PG_FRAME), &off);
#ifdef DIAGNOSTIC
		if (bank == -1)
			panic("pmap_remove_ptes: unmanaged page marked "
			      "PG_PVLIST, va = 0x%lx, pa = 0x%lx",
			      startva, (u_long)(opte & PG_FRAME));
#endif

		/* sync R/M bits */
		simple_lock(&vm_physmem[bank].pmseg.pvhead[off].pvh_lock);
		vm_physmem[bank].pmseg.attrs[off] |= (opte & (PG_U|PG_M));
		pve = pmap_remove_pv(&vm_physmem[bank].pmseg.pvhead[off], pmap,
				     startva);
		simple_unlock(&vm_physmem[bank].pmseg.pvhead[off].pvh_lock);

		if (pve) {
			pve->pv_next = pv_tofree;
			pv_tofree = pve;
		}

		/* end of "for" loop: time for next pte */
	}
	if (pv_tofree)
		pmap_free_pvs(pmap, pv_tofree);
}


/*
 * pmap_remove_pte: remove a single PTE from a PTP
 *
 * => must have proper locking on pmap_master_lock
 * => caller must hold pmap's lock
 * => PTP must be mapped into KVA
 * => PTP should be null if pmap == pmap_kernel()
 * => returns true if we removed a mapping
 */

static boolean_t
pmap_remove_pte(pmap, ptp, pte, va, flags)
	struct pmap *pmap;
	struct vm_page *ptp;
	pt_entry_t *pte;
	vaddr_t va;
	int flags;
{
	pt_entry_t opte;
	int bank, off;
	struct pv_entry *pve;

	if (!pmap_valid_entry(*pte))
		return(FALSE);		/* VA not mapped */
	if ((flags & PMAP_REMOVE_SKIPWIRED) && (*pte & PG_W)) {
		return(FALSE);
	}

	opte = *pte;			/* save the old PTE */
	*pte = 0;			/* zap! */

	if (opte & PG_W)
		pmap->pm_stats.wired_count--;
	pmap->pm_stats.resident_count--;

	if (ptp)
		ptp->wire_count--;		/* dropping a PTE */

	if (pmap_is_curpmap(pmap))
		pmap_update_pg(va);		/* flush TLB */

	/*
	 * if we are not on a pv_head list we are done.
	 */

	if ((opte & PG_PVLIST) == 0) {
#ifdef DIAGNOSTIC
		if (vm_physseg_find(btop(opte & PG_FRAME), &off) != -1)
			panic("pmap_remove_pte: managed page without "
			      "PG_PVLIST for 0x%lx", va);
#endif
		return(TRUE);
	}

	bank = vm_physseg_find(btop(opte & PG_FRAME), &off);
#ifdef DIAGNOSTIC
	if (bank == -1)
		panic("pmap_remove_pte: unmanaged page marked "
		    "PG_PVLIST, va = 0x%lx, pa = 0x%lx", va,
		    (u_long)(opte & PG_FRAME));
#endif

	/* sync R/M bits */
	simple_lock(&vm_physmem[bank].pmseg.pvhead[off].pvh_lock);
	vm_physmem[bank].pmseg.attrs[off] |= (opte & (PG_U|PG_M));
	pve = pmap_remove_pv(&vm_physmem[bank].pmseg.pvhead[off], pmap, va);
	simple_unlock(&vm_physmem[bank].pmseg.pvhead[off].pvh_lock);

	if (pve)
		pmap_free_pv(pmap, pve);
	return(TRUE);
}

/*
 * pmap_remove: top level mapping removal function
 *
 * => caller should not be holding any pmap locks
 */

void
pmap_remove(pmap, sva, eva)
	struct pmap *pmap;
	vaddr_t sva, eva;
{
	pmap_do_remove(pmap, sva, eva, PMAP_REMOVE_ALL);
}

/*
 * pmap_do_remove: mapping removal guts
 *
 * => caller should not be holding any pmap locks
 */

static void
pmap_do_remove(pmap, sva, eva, flags)
	struct pmap *pmap;
	vaddr_t sva, eva;
	int flags;
{
	pt_entry_t *ptes;
	pd_entry_t **pdes, pde;
	boolean_t result;
	paddr_t ptppa;
	vaddr_t blkendva;
	struct vm_page *ptp;
	struct pmap_remove_record pmap_rr, *prr;

	/*
	 * we lock in the pmap => pv_head direction
	 */

	PMAP_MAP_TO_HEAD_LOCK();
	pmap_map_ptes(pmap, &ptes, &pdes);	/* locks pmap */
#if 0
	if (sva < VM_MAXUSER_ADDRESS)
		printf("remove va %lx - %lx at pte %p\n", sva, eva,
		    &ptes[pl1_i(sva)]);
#endif

	/*
	 * removing one page?  take shortcut function.
	 */

	if (sva + PAGE_SIZE == eva) {

		if (pmap_pdes_valid(sva, pdes, &pde)) {

			/* PA of the PTP */
			ptppa = pde & PG_FRAME;

			/* get PTP if non-kernel mapping */

			if (pmap == pmap_kernel()) {
				/* we never free kernel PTPs */
				ptp = NULL;
			} else {
				ptp = pmap_find_ptp(pmap, sva, ptppa, 1);
#ifdef DIAGNOSTIC
				if (ptp == NULL) {
					pmap_dump_obj(pmap, 1);
					printf("va %lx ppa %lx\n",
					    (unsigned long)sva,
					    (unsigned long)ptppa);
					panic("pmap_remove: unmanaged "
					      "PTP detected");
				}
#endif
			}

			/* do it! */
			result = pmap_remove_pte(pmap, ptp,
			    &ptes[pl1_i(sva)], sva, flags);

			/*
			 * if mapping removed and the PTP is no longer
			 * being used, free it!
			 */

			if (result && ptp && ptp->wire_count <= 1)
				pmap_free_ptp(pmap, ptp, sva, ptes, pdes);
		}

		pmap_unmap_ptes(pmap);		/* unlock pmap */
		PMAP_MAP_TO_HEAD_UNLOCK();
		return;
	}

	/*
	 * removing a range of pages: we unmap in PTP sized blocks
	 *
	 * if we are the currently loaded pmap, we use prr to keep track
	 * of the VAs we unload so that we can flush them out of the tlb.
	 */

	if (pmap_is_curpmap(pmap)) {
		prr = &pmap_rr;
		prr->prr_npages = 0;
	} else {
		prr = NULL;
	}

	for (/* null */ ; sva < eva ; sva = blkendva) {

		/* determine range of block */
		blkendva = round_pdr(sva+1);
		if (blkendva > eva)
			blkendva = eva;

		/*
		 * XXXCDC: our PTE mappings should never be removed
		 * with pmap_remove!  if we allow this (and why would
		 * we?) then we end up freeing the pmap's page
		 * directory page (PDP) before we are finished using
		 * it when we hit in in the recursive mapping.  this
		 * is BAD.
		 *
		 * long term solution is to move the PTEs out of user
		 * address space.  and into kernel address space (up
		 * with APTE).  then we can set VM_MAXUSER_ADDRESS to
		 * be VM_MAX_ADDRESS.
		 */

		if (pl_i(sva, PTP_LEVELS) == PDIR_SLOT_PTE)
			/* XXXCDC: ugly hack to avoid freeing PDP here */
			continue;

		if (!pmap_pdes_valid(sva, pdes, &pde))
			continue;

		/* PA of the PTP */
		ptppa = pde & PG_FRAME;

		/* get PTP if non-kernel mapping */
		if (pmap == pmap_kernel()) {
			/* we never free kernel PTPs */
			ptp = NULL;
		} else {
			ptp = pmap_find_ptp(pmap, sva, ptppa, 1);
#ifdef DIAGNOSTIC
			if (ptp == NULL) {
				pmap_dump_obj(pmap, 1);
				printf("va %lx ppa %lx\n",
				    (unsigned long)sva,
				    (unsigned long)ptppa);
				panic("pmap_remove: unmanaged PTP "
				      "detected");
			}
#endif
		}
		pmap_remove_ptes(pmap, prr, ptp,
		    (vaddr_t)&ptes[pl1_i(sva)], sva, blkendva, flags);

		/* if PTP is no longer being used, free it! */
		if (ptp && ptp->wire_count <= 1) {
			pmap_free_ptp(pmap, ptp, sva, ptes,pdes);
#if defined(I386_CPU)
			/* cancel possible pending pmap update on i386 */
			if (cpu_class == CPUCLASS_386 && prr)
				prr->prr_npages = 0;
#endif
		}
	}

	/*
	 * if we kept a removal record and removed some pages update the TLB
	 */

	if (prr && prr->prr_npages) {
#if defined(I386_CPU)
		if (cpu_class == CPUCLASS_386) {
			tlbflush();
		} else
#endif
		{ /* not I386 */
			if (prr->prr_npages > PMAP_RR_MAX) {
				tlbflush();
			} else {
				while (prr->prr_npages) {
					pmap_update_pg(
					    prr->prr_vas[--prr->prr_npages]);
				}
			}
		} /* not I386 */
	}
	pmap_unmap_ptes(pmap);
	PMAP_MAP_TO_HEAD_UNLOCK();
}

/*
 * pmap_page_remove: remove a managed vm_page from all pmaps that map it
 *
 * => we set pv_head => pmap locking
 * => R/M bits are sync'd back to attrs
 */

void
pmap_page_remove(pg)
	struct vm_page *pg;
{
	int bank, off;
	struct pv_head *pvh;
	struct pv_entry *pve, *npve, **prevptr, *killlist = NULL;
	pt_entry_t *ptes, opte;
	pd_entry_t **pdes;
#if defined(I386_CPU)
	boolean_t needs_update = FALSE;
#endif
#ifdef DIAGNOSTIC
	pd_entry_t pde;
#endif

	/* XXX: vm_page should either contain pv_head or have a pointer to it */
	bank = vm_physseg_find(atop(VM_PAGE_TO_PHYS(pg)), &off);
	if (bank == -1) {
		printf("pmap_page_remove: unmanaged page?\n");
		return;
	}

	pvh = &vm_physmem[bank].pmseg.pvhead[off];
	if (pvh->pvh_list == NULL) {
		return;
	}

	/* set pv_head => pmap locking */
	PMAP_HEAD_TO_MAP_LOCK();

	/* XXX: needed if we hold head->map lock? */
	simple_lock(&pvh->pvh_lock);

	for (prevptr = &pvh->pvh_list, pve = pvh->pvh_list;
	    pve != NULL; pve = npve) {
		npve = pve->pv_next;
		pmap_map_ptes(pve->pv_pmap, &ptes, &pdes);	/* locks pmap */

#ifdef DIAGNOSTIC
		if (pve->pv_ptp && pmap_pdes_valid(pve->pv_va, pdes, &pde) &&
		   (pde & PG_FRAME) != VM_PAGE_TO_PHYS(pve->pv_ptp)) {
			printf("pmap_page_remove: pg=%p: va=%lx, pv_ptp=%p\n",
			       pg, pve->pv_va, pve->pv_ptp);
			printf("pmap_page_remove: PTP's phys addr: "
			       "actual=%lx, recorded=%lx\n",
			       (unsigned long)(pde & PG_FRAME),
				VM_PAGE_TO_PHYS(pve->pv_ptp));
			panic("pmap_page_remove: mapped managed page has "
			      "invalid pv_ptp field");
		}
#endif
		opte = ptes[pl1_i(pve->pv_va)];
		ptes[pl1_i(pve->pv_va)] = 0;		/* zap! */

		if (opte & PG_W)
			pve->pv_pmap->pm_stats.wired_count--;
		pve->pv_pmap->pm_stats.resident_count--;

		if (pmap_is_curpmap(pve->pv_pmap)) {
#if defined(I386_CPU)
			if (cpu_class == CPUCLASS_386)
				needs_update = TRUE;
			else
#endif
				pmap_update_pg(pve->pv_va);
		}

		/* sync R/M bits */
		vm_physmem[bank].pmseg.attrs[off] |= (opte & (PG_U|PG_M));

		/* update the PTP reference count.  free if last reference. */
		if (pve->pv_ptp) {
			pve->pv_ptp->wire_count--;
			if (pve->pv_ptp->wire_count <= 1) {
				pmap_free_ptp(pve->pv_pmap, pve->pv_ptp,
					      pve->pv_va, ptes, pdes);
#if defined(I386_CPU)
				needs_update = FALSE;
#endif
			}
		}
		pmap_unmap_ptes(pve->pv_pmap);		/* unlocks pmap */
		*prevptr = npve;			/* remove it */
		pve->pv_next = killlist;		/* mark it for death */
		killlist = pve;
	}
	pmap_free_pvs(NULL, killlist);
	pvh->pvh_list = NULL;
	simple_unlock(&pvh->pvh_lock);
	PMAP_HEAD_TO_MAP_UNLOCK();
#if defined(I386_CPU)
	if (needs_update)
		tlbflush();
#endif
}

/*
 * p m a p   a t t r i b u t e  f u n c t i o n s
 * functions that test/change managed page's attributes
 * since a page can be mapped multiple times we must check each PTE that
 * maps it by going down the pv lists.
 */

/*
 * pmap_test_attrs: test a page's attributes
 *
 * => we set pv_head => pmap locking
 */

boolean_t
pmap_test_attrs(pg, testbits)
	struct vm_page *pg;
	int testbits;
{
	int bank, off;
	char *myattrs;
	struct pv_head *pvh;
	struct pv_entry *pve;
	pt_entry_t *ptes, pte;
	pd_entry_t **pdes;

	/* XXX: vm_page should either contain pv_head or have a pointer to it */
	bank = vm_physseg_find(atop(VM_PAGE_TO_PHYS(pg)), &off);
	if (bank == -1) {
		printf("pmap_test_attrs: unmanaged page?\n");
		return(FALSE);
	}

	/*
	 * before locking: see if attributes are already set and if so,
	 * return!
	 */

	myattrs = &vm_physmem[bank].pmseg.attrs[off];
	if (*myattrs & testbits)
		return(TRUE);

	/* test to see if there is a list before bothering to lock */
	pvh = &vm_physmem[bank].pmseg.pvhead[off];
	if (pvh->pvh_list == NULL) {
		return(FALSE);
	}

	/* nope, gonna have to do it the hard way */
	PMAP_HEAD_TO_MAP_LOCK();
	/* XXX: needed if we hold head->map lock? */
	simple_lock(&pvh->pvh_lock);

	for (pve = pvh->pvh_list; pve != NULL && (*myattrs & testbits) == 0;
	     pve = pve->pv_next) {
		pmap_map_ptes(pve->pv_pmap, &ptes, &pdes);
		pte = ptes[pl1_i(pve->pv_va)];
		pmap_unmap_ptes(pve->pv_pmap);
		*myattrs |= pte;
	}

	/*
	 * note that we will exit the for loop with a non-null pve if
	 * we have found the bits we are testing for.
	 */

	simple_unlock(&pvh->pvh_lock);
	PMAP_HEAD_TO_MAP_UNLOCK();
	return((*myattrs & testbits) != 0);
}

/*
 * pmap_change_attrs: change a page's attributes
 *
 * => we set pv_head => pmap locking
 * => we return TRUE if we cleared one of the bits we were asked to
 */

boolean_t
pmap_change_attrs(pg, setbits, clearbits)
	struct vm_page *pg;
	int setbits, clearbits;
{
	u_int32_t result;
	int bank, off;
	struct pv_head *pvh;
	struct pv_entry *pve;
	pt_entry_t *ptes, npte;
	pd_entry_t **pdes;
	char *myattrs;
#if defined(I386_CPU)
	boolean_t needs_update = FALSE;
#endif

	/* XXX: vm_page should either contain pv_head or have a pointer to it */
	bank = vm_physseg_find(atop(VM_PAGE_TO_PHYS(pg)), &off);
	if (bank == -1) {
		printf("pmap_change_attrs: unmanaged page?\n");
		return(FALSE);
	}

	PMAP_HEAD_TO_MAP_LOCK();
	pvh = &vm_physmem[bank].pmseg.pvhead[off];
	/* XXX: needed if we hold head->map lock? */
	simple_lock(&pvh->pvh_lock);

	myattrs = &vm_physmem[bank].pmseg.attrs[off];
	result = *myattrs & clearbits;
	*myattrs = (*myattrs | setbits) & ~clearbits;

	for (pve = pvh->pvh_list; pve != NULL; pve = pve->pv_next) {
		pmap_map_ptes(pve->pv_pmap, &ptes, &pdes);	/* locks pmap */
#ifdef DIAGNOSTIC
		if (!pmap_pdes_valid(pve->pv_va, pdes, NULL))
			panic("pmap_change_attrs: mapping without PTP "
			      "detected");
#endif

		npte = ptes[pl1_i(pve->pv_va)];
		result |= (npte & clearbits);
		npte = (npte | setbits) & ~clearbits;
		if (ptes[pl1_i(pve->pv_va)] != npte) {
			ptes[pl1_i(pve->pv_va)] = npte;	/* zap! */

			if (pmap_is_curpmap(pve->pv_pmap)) {
#if defined(I386_CPU)
				if (cpu_class == CPUCLASS_386)
					needs_update = TRUE;
				else
#endif
					pmap_update_pg(pve->pv_va);
			}
		}
		pmap_unmap_ptes(pve->pv_pmap);		/* unlocks pmap */
	}

	simple_unlock(&pvh->pvh_lock);
	PMAP_HEAD_TO_MAP_UNLOCK();

#if defined(I386_CPU)
	if (needs_update)
		tlbflush();
#endif
	return(result != 0);
}

/*
 * p m a p   p r o t e c t i o n   f u n c t i o n s
 */

/*
 * pmap_page_protect: change the protection of all recorded mappings
 *	of a managed page
 *
 * => NOTE: this is an inline function in pmap.h
 */

/* see pmap.h */

/*
 * pmap_protect: set the protection in of the pages in a pmap
 *
 * => NOTE: this is an inline function in pmap.h
 */

/* see pmap.h */

/*
 * pmap_write_protect: write-protect pages in a pmap
 */

void
pmap_write_protect(pmap, sva, eva, prot)
	struct pmap *pmap;
	vaddr_t sva, eva;
	vm_prot_t prot;
{
	pt_entry_t *ptes, *spte, *epte, npte;
	pd_entry_t **pdes;
	struct pmap_remove_record pmap_rr, *prr;
	vaddr_t blockend, va;
	u_int32_t md_prot;

	pmap_map_ptes(pmap, &ptes, &pdes);		/* locks pmap */

	/* need to worry about TLB? [TLB stores protection bits] */
	if (pmap_is_curpmap(pmap)) {
		prr = &pmap_rr;
		prr->prr_npages = 0;
	} else {
		prr = NULL;
	}

	/* should be ok, but just in case ... */
	sva &= PG_FRAME;
	eva &= PG_FRAME;

	for (/* null */ ; sva < eva ; sva = blockend) {

		blockend = (sva & L2_FRAME) + NBPD_L2;
		if (blockend > eva)
			blockend = eva;

		/*
		 * XXXCDC: our PTE mappings should never be write-protected!
		 *
		 * long term solution is to move the PTEs out of user
		 * address space.  and into kernel address space (up
		 * with APTE).  then we can set VM_MAXUSER_ADDRESS to
		 * be VM_MAX_ADDRESS.
		 */

		/* XXXCDC: ugly hack to avoid freeing PDP here */
		if (pl_i(sva, PTP_LEVELS) == PDIR_SLOT_PTE)
			continue;

		/* empty block? */
		if (!pmap_pdes_valid(sva, pdes, NULL))
			continue;

		md_prot = protection_codes[prot];
		if (sva < VM_MAXUSER_ADDRESS)
			md_prot |= PG_u;
		else if (sva < VM_MAX_ADDRESS)
			/* XXX: write-prot our PTES? never! */
			md_prot |= (PG_u | PG_RW);

		spte = &ptes[pl1_i(sva)];
		epte = &ptes[pl1_i(blockend)];

		for (/*null */; spte < epte ; spte++) {

			if (!pmap_valid_entry(*spte))	/* no mapping? */
				continue;

			npte = (*spte & ~PG_PROT) | md_prot;

			if (npte != *spte) {
				*spte = npte;		/* zap! */

				if (prr) {    /* worried about tlb flushing? */
					va = ptob(spte - ptes);
					if (npte & PG_G) {
						/* PG_G requires this */
						pmap_update_pg(va);
					} else {
						if (prr->prr_npages <
						    PMAP_RR_MAX) {
							prr->prr_vas[
							    prr->prr_npages++] =
								va;
						} else {
						    if (prr->prr_npages ==
							PMAP_RR_MAX)
							/* signal an overflow */
							    prr->prr_npages++;
						}
					}
				}	/* if (prr) */
			}	/* npte != *spte */
		}	/* for loop */
	}

	/*
	 * if we kept a removal record and removed some pages update the TLB
	 */

	if (prr && prr->prr_npages) {
#if defined(I386_CPU)
		if (cpu_class == CPUCLASS_386) {
			tlbflush();
		} else
#endif
		{ /* not I386 */
			if (prr->prr_npages > PMAP_RR_MAX) {
				tlbflush();
			} else {
				while (prr->prr_npages) {
					pmap_update_pg(prr->prr_vas[
						       --prr->prr_npages]);
				}
			}
		} /* not I386 */
	}
	pmap_unmap_ptes(pmap);		/* unlocks pmap */
}

/*
 * end of protection functions
 */

/*
 * pmap_unwire: clear the wired bit in the PTE
 *
 * => mapping should already be in map
 */

void
pmap_unwire(pmap, va)
	struct pmap *pmap;
	vaddr_t va;
{
	pt_entry_t *ptes;
	pd_entry_t **pdes;

	pmap_map_ptes(pmap, &ptes, &pdes);		/* locks pmap */

	if (pmap_pdes_valid(va, pdes, NULL)) {

#ifdef DIAGNOSTIC
		if (!pmap_valid_entry(ptes[pl1_i(va)]))
			panic("pmap_unwire: invalid (unmapped) va 0x%lx", va);
#endif
		if ((ptes[pl1_i(va)] & PG_W) != 0) {
			ptes[pl1_i(va)] &= ~PG_W;
			pmap->pm_stats.wired_count--;
		}
#ifdef DIAGNOSTIC
		else {
			printf("pmap_unwire: wiring for pmap %p va 0x%lx "
			       "didn't change!\n", pmap, va);
		}
#endif
	}
#ifdef DIAGNOSTIC
	else {
		panic("pmap_unwire: invalid PDE");
	}
#endif
	pmap_unmap_ptes(pmap);		/* unlocks map */
}

/*
 * pmap_collect: free resources held by a pmap
 *
 * => optional function.
 * => called when a process is swapped out to free memory.
 */

void
pmap_collect(pmap)
	struct pmap *pmap;
{
	/*
	 * free all of the pt pages by removing the physical mappings
	 * for its entire address space.
	 */

	pmap_do_remove(pmap, VM_MIN_ADDRESS, VM_MAX_ADDRESS,
	    PMAP_REMOVE_SKIPWIRED);
}

/*
 * pmap_copy: copy mappings from one pmap to another
 *
 * => optional function
 * void pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
 */

/*
 * defined as macro call in pmap.h
 */

/*
 * pmap_enter: enter a mapping into a pmap
 *
 * => must be done "now" ... no lazy-evaluation
 * => we set pmap => pv_head locking
 */

int
pmap_enter(pmap, va, pa, prot, flags)
	struct pmap *pmap;
	vaddr_t va;
	paddr_t pa;
	vm_prot_t prot;
	int flags;
{
	pt_entry_t *ptes, opte, npte;
	pd_entry_t **pdes;
	struct vm_page *ptp;
	struct pv_head *pvh;
	struct pv_entry *pve;
	int bank, off, error;
	int ptpdelta, wireddelta, resdelta;
	boolean_t wired = (flags & PMAP_WIRED) != 0;

#ifdef DIAGNOSTIC
	/* sanity check: totally out of range? */
	if (va >= VM_MAX_KERNEL_ADDRESS)
		panic("pmap_enter: too big");

	if (va == (vaddr_t) PDP_BASE || va == (vaddr_t) APDP_BASE)
		panic("pmap_enter: trying to map over PDP/APDP!");

	/* sanity check: kernel PTPs should already have been pre-allocated */
	if (va >= VM_MIN_KERNEL_ADDRESS &&
	    !pmap_valid_entry(pmap->pm_pdir[pl_i(va, PTP_LEVELS)]))
		panic("pmap_enter: missing kernel PTP for va %lx!", va);

#endif

	/* get lock */
	PMAP_MAP_TO_HEAD_LOCK();

	/*
	 * map in ptes and get a pointer to our PTP (unless we are the kernel)
	 */

	pmap_map_ptes(pmap, &ptes, &pdes);		/* locks pmap */

	if (pmap == pmap_kernel()) {
		ptp = NULL;
	} else {
		ptp = pmap_get_ptp(pmap, va, pdes);
		if (ptp == NULL) {
			if (flags & PMAP_CANFAIL) {
				error = ENOMEM;
				goto out;
			}
			panic("pmap_enter: get ptp failed");
		}
	}
#if 0
	if (va < VM_MAXUSER_ADDRESS)
		printf("enter va %lx at pte %p\n", va, &ptes[pl1_i(va)]);
#endif
	opte = ptes[pl1_i(va)];		/* old PTE */

	/*
	 * is there currently a valid mapping at our VA?
	 */

	if (pmap_valid_entry(opte)) {
		/*
		 * first, calculate pm_stats updates.  resident count will not
		 * change since we are replacing/changing a valid mapping.
		 * wired count might change...
		 */

		resdelta = 0;
		if (wired && (opte & PG_W) == 0)
			wireddelta = 1;
		else if (!wired && (opte & PG_W) != 0)
			wireddelta = -1;
		else
			wireddelta = 0;
		ptpdelta = 0;

		/*
		 * is the currently mapped PA the same as the one we
		 * want to map?
		 */

		if ((opte & PG_FRAME) == pa) {

			/* if this is on the PVLIST, sync R/M bit */
			if (opte & PG_PVLIST) {
				bank = vm_physseg_find(atop(pa), &off);
#ifdef DIAGNOSTIC
				if (bank == -1)
					panic("pmap_enter: same pa PG_PVLIST "
					      "mapping with unmanaged page "
					      "pa = 0x%lx (0x%lx)", pa,
					      atop(pa));
#endif
				pvh = &vm_physmem[bank].pmseg.pvhead[off];
				simple_lock(&pvh->pvh_lock);
				vm_physmem[bank].pmseg.attrs[off] |= opte;
				simple_unlock(&pvh->pvh_lock);
			} else {
				pvh = NULL;	/* ensure !PG_PVLIST */
			}
			goto enter_now;
		}

		/*
		 * changing PAs: we must remove the old one first
		 */

		/*
		 * if current mapping is on a pvlist,
		 * remove it (sync R/M bits)
		 */

		if (opte & PG_PVLIST) {
			bank = vm_physseg_find(atop(opte & PG_FRAME), &off);
#ifdef DIAGNOSTIC
			if (bank == -1)
				panic("pmap_enter: PG_PVLIST mapping with "
				      "unmanaged page "
				      "pa = 0x%lx (0x%lx)", pa, atop(pa));
#endif
			pvh = &vm_physmem[bank].pmseg.pvhead[off];
			simple_lock(&pvh->pvh_lock);
			pve = pmap_remove_pv(pvh, pmap, va);
			vm_physmem[bank].pmseg.attrs[off] |= opte;
			simple_unlock(&pvh->pvh_lock);
		} else {
			pve = NULL;
		}
	} else {	/* opte not valid */
		pve = NULL;
		resdelta = 1;
		if (wired)
			wireddelta = 1;
		else
			wireddelta = 0;
		if (ptp)
			ptpdelta = 1;
		else
			ptpdelta = 0;
	}

	/*
	 * pve is either NULL or points to a now-free pv_entry structure
	 * (the latter case is if we called pmap_remove_pv above).
	 *
	 * if this entry is to be on a pvlist, enter it now.
	 */

	bank = vm_physseg_find(atop(pa), &off);
	if (pmap_initialized && bank != -1) {
		pvh = &vm_physmem[bank].pmseg.pvhead[off];
		if (pve == NULL) {
			pve = pmap_alloc_pv(pmap, ALLOCPV_NEED);
			if (pve == NULL) {
				if (flags & PMAP_CANFAIL) {
					error = ENOMEM;
					goto out;
				}
				panic("pmap_enter: no pv entries available");
			}
		}
		/* lock pvh when adding */
		pmap_enter_pv(pvh, pve, pmap, va, ptp);
	} else {

		/* new mapping is not PG_PVLIST.   free pve if we've got one */
		pvh = NULL;		/* ensure !PG_PVLIST */
		if (pve)
			pmap_free_pv(pmap, pve);
	}

enter_now:
	/*
	 * at this point pvh is !NULL if we want the PG_PVLIST bit set
	 */

	pmap->pm_stats.resident_count += resdelta;
	pmap->pm_stats.wired_count += wireddelta;
	if (ptp)
		ptp->wire_count += ptpdelta;
	npte = pa | protection_codes[prot] | PG_V;
	if (pvh)
		npte |= PG_PVLIST;
	if (wired)
		npte |= PG_W;
	if (va < VM_MAXUSER_ADDRESS)
		npte |= PG_u;
	else if (va < VM_MAX_ADDRESS)
		npte |= (PG_u | PG_RW);	/* XXXCDC: no longer needed? */
	if (pmap == pmap_kernel())
		npte |= pmap_pg_g;

	ptes[pl1_i(va)] = npte;		/* zap! */

	if ((opte & ~(PG_M|PG_U)) != npte && pmap_is_curpmap(pmap))
		pmap_update_pg(va);

	error = 0;

out:
	pmap_unmap_ptes(pmap);
	PMAP_MAP_TO_HEAD_UNLOCK();

	return error;
}

static __inline boolean_t
pmap_get_physpage(va, level, paddrp)
	vaddr_t va;
	int level;
	paddr_t *paddrp;
{
	struct vm_page *ptp;
	struct pmap *kpm = pmap_kernel();

	if (uvm.page_init_done == FALSE) {
		/*
		 * we're growing the kernel pmap early (from
		 * uvm_pageboot_alloc()).  this case must be
		 * handled a little differently.
		 */

		if (uvm_page_physget(paddrp) == FALSE)
			panic("pmap_get_physpage: out of memory");
		pmap_zero_page(*paddrp);
	} else {
		ptp = uvm_pagealloc(&kpm->pm_obj[level - 1],
				    ptp_va2o(va, level), NULL,
				    UVM_PGA_USERESERVE|UVM_PGA_ZERO);
		if (ptp == NULL)
			panic("pmap_get_physpage: out of memory");
		ptp->flags &= ~PG_BUSY;
		ptp->wire_count = 1;
		*paddrp = VM_PAGE_TO_PHYS(ptp);
	}
	kpm->pm_stats.resident_count++;
	return TRUE;
}

/*
 * Allocate the amount of specified ptps for a ptp level, and populate
 * all levels below accordingly, mapping virtual addresses starting at
 * kva.
 *
 * Used by pmap_growkernel.
 */
static void
pmap_alloc_level(pdes, kva, lvl, needed_ptps)
	pd_entry_t **pdes;
	vaddr_t kva;
	int lvl;
	unsigned long *needed_ptps;
{
	unsigned long i;
	vaddr_t va;
	paddr_t pa;
	unsigned long index, endindex;
	int level;
	pd_entry_t *pdep;

	for (level = lvl; level > 1; level--) {
		if (level == PTP_LEVELS)
			pdep = pmap_kernel()->pm_pdir;
		else
			pdep = pdes[level - 2];
		va = kva;
		index = pl_i(kva, level);
		endindex = index + needed_ptps[level - 1];

		for (i = index; i < endindex; i++) {
			pmap_get_physpage(va, level - 1, &pa);
			pdep[i] = pa | PG_RW | PG_V;
			nkptp[level - 1]++;
			va += nbpd[level - 1];
		}
#if 0
		if (level == 2 && (endindex - index) > 5) {
		printf("alloc_level: va %lx - %lx, pde %p - %p\n",
		    kva, va, &pdep[index], &pdep[endindex]);
			if (flipje == 0)
				flipje = 1;
			else
				panic("bah humbug");
		}
#endif
	}
}

/*
 * pmap_growkernel: increase usage of KVM space
 *
 * => we allocate new PTPs for the kernel and install them in all
 *	the pmaps on the system.
 */

vaddr_t
pmap_growkernel(maxkvaddr)
	vaddr_t maxkvaddr;
{
	struct pmap *kpm = pmap_kernel(), *pm;
	int s, i;
	unsigned newpdes;
	vaddr_t curmax;
	unsigned long needed_kptp[PTP_LEVELS], target_nptp, old;

	curmax = VM_MIN_KERNEL_ADDRESS + nkptp[1] * NBPD_L2;
	if (maxkvaddr <= curmax)
		return curmax;
	maxkvaddr = round_pdr(maxkvaddr);
	old = nkptp[PTP_LEVELS - 1];
	/*
	 * This loop could be optimized more, but pmap_growkernel()
	 * is called infrequently.
	 */
	for (i = PTP_LEVELS - 1; i >= 1; i--) {
		target_nptp = pl_i(maxkvaddr, i + 1) -
		    pl_i(VM_MIN_KERNEL_ADDRESS, i + 1);
		/*
		 * XXX only need to check toplevel.
		 */
#if 0
		printf("pmap_growkernel: lvl %d targ %lu max %lu cur %lu\n",
		    i + 1, target_nptp, nkptpmax[i], nkptp[i]);
#endif
		if (target_nptp > nkptpmax[i])
			panic("out of KVA space");
		needed_kptp[i] = target_nptp - nkptp[i];
	}


	s = splhigh();	/* to be safe */
	simple_lock(&kpm->pm_lock);
	pmap_alloc_level(normal_pdes, curmax, PTP_LEVELS,
	    needed_kptp);

	/*
	 * If the number of top level entries changed, update all
	 * pmaps.
	 */
	if (needed_kptp[PTP_LEVELS - 1] != 0) {
		newpdes = nkptp[PTP_LEVELS - 1] - old;
		simple_lock(&pmaps_lock);
		for (pm = pmaps.lh_first; pm != NULL;
		     pm = pm->pm_list.le_next) {
			memcpy(&pm->pm_pdir[PDIR_SLOT_KERN + old],
			       &kpm->pm_pdir[PDIR_SLOT_KERN + old],
			       newpdes * sizeof (pd_entry_t));
		}

		/* Invalidate the PDP cache. */
		pool_cache_invalidate(&pmap_pdp_cache);

		simple_unlock(&pmaps_lock);
	}
	simple_unlock(&kpm->pm_lock);
	splx(s);

	return maxkvaddr;
}

#ifdef DEBUG
void pmap_dump __P((struct pmap *, vaddr_t, vaddr_t));

/*
 * pmap_dump: dump all the mappings from a pmap
 *
 * => caller should not be holding any pmap locks
 */

void
pmap_dump(pmap, sva, eva)
	struct pmap *pmap;
	vaddr_t sva, eva;
{
	pt_entry_t *ptes, *pte;
	pd_entry_t **pdes;
	vaddr_t blkendva;

	/*
	 * if end is out of range truncate.
	 * if (end == start) update to max.
	 */

	if (eva > VM_MAXUSER_ADDRESS || eva <= sva)
		eva = VM_MAXUSER_ADDRESS;

	/*
	 * we lock in the pmap => pv_head direction
	 */

	PMAP_MAP_TO_HEAD_LOCK();
	pmap_map_ptes(pmap, &ptes, &pdes);	/* locks pmap */

	/*
	 * dumping a range of pages: we dump in PTP sized blocks (4MB)
	 */

	for (/* null */ ; sva < eva ; sva = blkendva) {

		/* determine range of block */
		blkendva = round_pdr(sva+1);
		if (blkendva > eva)
			blkendva = eva;

		/* valid block? */
		if (!pmap_pdes_valid(sva, pdes, NULL))
			continue;

		pte = &ptes[pl1_i(sva)];
		for (/* null */; sva < blkendva ; sva += PAGE_SIZE, pte++) {
			if (!pmap_valid_entry(*pte))
				continue;
			printf("va %#lx -> pa %#lx (pte=%#lx)\n",
			       sva, *pte, *pte & PG_FRAME);
		}
	}
	pmap_unmap_ptes(pmap);
	PMAP_MAP_TO_HEAD_UNLOCK();
}
#endif

#ifdef DIAGNOSTIC
static void
pmap_dump_obj(struct pmap *pmap, int level)
{
	struct vm_page *pg;
	int i;

	for (i = 0; i < PDIR_SLOT_KERN; i++)
		if (pmap->pm_pdir[i] & PG_V)
			printf("%d: %lx\n", i,
			    (unsigned long)pmap->pm_pdir[i] & PG_FRAME);

	for (pg = TAILQ_FIRST(&pmap->pm_obj[level - 1].memq);
	     pg != NULL;
	     pg = TAILQ_NEXT(pg, listq))
		printf("off %llx paddr %lx flags %x\n",
	 	    (unsigned long long)pg->offset,
		    pg->phys_addr, pg->flags);
}
#endif
