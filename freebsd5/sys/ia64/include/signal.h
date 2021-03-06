/* $FreeBSD: src/sys/ia64/include/signal.h,v 1.11 2002/10/25 19:10:57 peter Exp $ */
/* From: NetBSD: signal.h,v 1.3 1997/04/06 08:47:43 cgd Exp */

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef _MACHINE_SIGNAL_H_
#define	_MACHINE_SIGNAL_H_

#include <sys/cdefs.h>
#include <sys/_sigset.h>

typedef long	sig_atomic_t;

#if __BSD_VISIBLE
/* portable macros for SIGFPE/ARITHTRAP */
#define FPE_INTOVF	1	/* integer overflow */
#define FPE_INTDIV	2	/* integer divide by zero */
#define FPE_FLTDIV	3	/* floating point divide by zero */
#define FPE_FLTOVF	4	/* floating point overflow */
#define FPE_FLTUND	5	/* floating point underflow */
#define FPE_FLTRES	6	/* floating point inexact result */
#define FPE_FLTINV	7	/* invalid floating point operation */
#define FPE_FLTSUB	8	/* subscript out of range */

#define BUS_SEGM_FAULT	30	/* segment protection base */
#endif

#if __XSI_VISIBLE
/*
 * Minimum signal stack size. The current signal frame
 * for IA-64 is 2656 bytes large.
 */
#define MINSIGSTKSZ     (3072 * 4)
#endif

#if __BSD_VISIBLE
#ifndef _IA64_FPREG_DEFINED

struct ia64_fpreg {
	unsigned long	fpr_bits[2];
} __aligned(16);

#define _IA64_FPREG_DEFINED

#endif
#endif


/*
 * Information pushed on stack when a signal is delivered.
 * This is used by the kernel to restore state following
 * execution of the signal handler.  It is also made available
 * to the handler to allow it to restore state properly if
 * a non-standard exit is performed.
 *
 * Note that sc_regs[] and sc_fpregs[]+sc_fpcr are inline
 * representations of 'struct reg' and 'struct fpreg', respectively.
 */

#if __BSD_VISIBLE
/*
 * The sequence of the fields should match those in
 * mcontext_t. Keep them in sync!
 */
struct sigcontext {
	struct __sigset	sc_mask;		/* signal mask to restore */
	unsigned long	sc_onstack;
	unsigned long	sc_flags;
	unsigned long	sc_nat;
	unsigned long	sc_sp;
	unsigned long	sc_ip;
	unsigned long	sc_cfm;
	unsigned long	sc_um;
	unsigned long	sc_ar_rsc;
	unsigned long	sc_ar_bsp;
	unsigned long	sc_ar_rnat;
	unsigned long	sc_ar_ccv;
	unsigned long	sc_ar_unat;
	unsigned long	sc_ar_fpsr;
	unsigned long	sc_ar_pfs;
	unsigned long	sc_pr;
	unsigned long	sc_br[8];
	unsigned long	sc_gr[32];
	struct ia64_fpreg sc_fr[128];
};
#endif /* __BSD_VISIBLE */

#endif /* !_MACHINE_SIGNAL_H_*/
