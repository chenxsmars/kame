/*	$NetBSD: support.s,v 1.2 1999/11/10 00:01:32 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1980, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: locore.s 1.66 92/12/22$
 *
 *	@(#)locore.s	8.6 (Berkeley) 5/27/94
 */

/*
 * Miscellaneous support routines common to all m68k ports.
 */

/*
 * NOTICE: This is not a standalone file.  To use it, #include it in
 * your port's locore.s, like so:
 *
 *	#include <m68k/m68k/support.s>
 */

/*
 * non-local gotos
 */
ENTRY(setjmp)
	movl	%sp@(4),%a0	| savearea pointer
	moveml	#0xFCFC,%a0@	| save d2-d7/a2-a7
	movl	%sp@,%a0@(48)	| and return address
	moveq	#0,%d0		| return 0
	rts

ENTRY(longjmp)
	movl	%sp@(4),%a0
	moveml	%a0@+,#0xFCFC
	movl	%a0@,%sp@
	moveq	#1,%d0
	rts

/*
 * the queue functions
 */
ENTRY(_insque)
	movw	%sr,%d0
	movw	#PSL_HIGHIPL,%sr	| atomic
	movl	%sp@(8),%a0		| where to insert (after)
	movl	%sp@(4),%a1		| element to insert (e)
	movl	%a0@,%a1@		| e->next = after->next
	movl	%a0,%a1@(4)		| e->prev = after
	movl	%a1,%a0@		| after->next = e
	movl	%a1@,%a0
	movl	%a1,%a0@(4)		| e->next->prev = e
	movw	%d0,%sr
	rts

ENTRY(_remque)
	movw	%sr,%d0
	movw	#PSL_HIGHIPL,%sr	| atomic
	movl	%sp@(4),%a0		| element to remove (e)
	movl	%a0@,%a1
	movl	%a0@(4),%a0
	movl	%a0,%a1@(4)		| e->next->prev = e->prev
	movl	%a1,%a0@		| e->prev->next = e->next
	movw	%d0,%sr
	rts
