/*	$NetBSD: locore.S,v 1.7 2003/11/07 17:00:19 augustss Exp $	*/
/*	$OpenBSD: locore.S,v 1.4 1997/01/26 09:06:38 rahnds Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_ddb.h"
#include "opt_kgdb.h"
#include "fs_kernfs.h"
#include "opt_ipkdb.h"
#include "opt_multiprocessor.h"
#include "opt_openpic.h"
#include "opt_ppcparam.h"
#include "assym.h"

#include <sys/syscall.h>

#include <machine/param.h>
#include <machine/vmparam.h>
#include <machine/pmap.h>
#include <machine/psl.h>
#include <machine/trap.h>
#define _NOREGNAMES
#include <machine/asm.h>

#include <powerpc/spr.h>
#include <powerpc/oea/hid.h>

#include "ksyms.h"

/*
 * Some instructions gas doesn't understand (yet?)
 */
#define	bdneq	bdnzf 2,

/*
 * Globals
 */
GLOBAL(startsym)
	.long	0			/* start symbol table */
GLOBAL(endsym)
	.long	0			/* end symbol table */
GLOBAL(proc0paddr)
	.long	0			/* proc0 p_addr */

/*
 * This symbol is here for the benefit of kvm_mkdb, and is supposed to
 * mark the start of kernel text.
 */
	.text
	.globl	_C_LABEL(kernel_text)
_C_LABEL(kernel_text):

/*
 * Startup entry.  Note, this must be the first thing in the text
 * segment!
 */
	.text
	.globl	__start
__start:
	li	0,0
	mtmsr	0			/* Disable FPU/MMU/exceptions */
	isync

	/* Enable data and instruction caches */
/*
	mfspr	8,1008
	ori	8, 8, (HID0_ICE | HID0_DCE)@l
	mtspr	1008,8
*/

/* compute end of kernel memory */
	lis	4,_C_LABEL(end)@ha
	addi	4,4,_C_LABEL(end)@l

	INIT_CPUINFO(4,1,9,0)

	lis	3,__start@ha
	addi	3,3,__start@l

	xor	5,5,5
	xor	6,6,6
	bl	_C_LABEL(initppc)

	sync
	isync
	mfspr	8,SPR_HID0
	ori	8, 8, (HID0_ICE | HID0_DCE)@l
	isync
	mtspr	SPR_HID0,8
	sync
	isync

	bl	_C_LABEL(main)

loop:	b	loop			/* XXX not reached */

	.globl	_C_LABEL(enable_intr)
_C_LABEL(enable_intr):
	mfmsr	3
	ori	3,3,PSL_EE@l
	mtmsr	3
	blr

	.globl	_C_LABEL(disable_intr)
_C_LABEL(disable_intr):
	mfmsr	3
	andi.	3,3,~PSL_EE@l
	mtmsr	3
	blr

/*
 * Include common switch / setfault code
 */
#include <powerpc/powerpc/locore_subr.S>

/*
 * Include common trap / execption code
 */
#include <powerpc/powerpc/trap_subr.S>

/*
 * Include PIO routines
 */
#include <powerpc/powerpc/pio_subr.S>
