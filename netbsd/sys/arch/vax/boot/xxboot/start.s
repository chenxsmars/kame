/*	$NetBSD: start.s,v 1.7 2000/06/19 20:05:17 ragge Exp $ */
/*
 * Copyright (c) 1995 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by
 * Bertram Barth.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
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

 /* All bugs are subject to removal without further notice */
		

#define	_LOCORE

#include "sys/disklabel.h"

#include "../include/mtpr.h"
#include "../include/asm.h"		

_start:	.globl _start		# this is the symbolic name for the start
				# of code to be relocated. We can use this
				# to get the actual/real adress (pc-rel)
				# or to get the relocated address (abs).

.org	0x00			# uVAX booted from TK50 starts here
	brb	from_0x00	# continue behind dispatch-block

.org	0x02			# information used by uVAX-ROM
	.byte (LABELOFFSET + d_end_)/2 # offset in words to identification area 
	.byte	1		# this byte must be 1
	.word	0		# logical block number (word swapped) 
	.word	0		# of the secondary image

.org	0x08			#
	brb	from_0x08	# skip ...

.org	0x0C			# 11/750  & 8200 starts here
	movzbl	$1,_from	# We booted from "old" rom.
	brw	cont_750


from_0x00:			# uVAX from TK50 
	brw	start_uvax	# all uVAXen continue there

from_0x08:			# Any machine from VMB
	movzbl	$4,_from		# Booted from full VMB
	halt			# Cannot handle this...

# the complete area reserved for label
# must be empty (i.e. filled with zeroes).
# disklabel(8) checks that before installing
# the bootblocks over existing label.

/*
 * Parameter block for uVAX boot.
 */
#define VOLINFO         0	/* 1=single-sided  81=double-sided volumes */
#define SISIZE          16	/* size in blocks of secondary image */
#define SILOAD          0	/* load offset (usually 0) from the default */
#define SIOFF           0x200	/* byte offset into secondary image */

.org    LABELOFFSET + d_end_
	.byte	0x18		# must be 0x18 
	.byte	0x00		# must be 0x00 (MBZ) 
	.byte	0x00		# any value 
	.byte	0xFF - (0x18 + 0x00 + 0x00)	
		/* 4th byte holds 1s' complement of sum of previous 3 bytes */

	.byte	0x00		# must be 0x00 (MBZ) 
	.byte	VOLINFO
	.byte	0x00		# any value 
	.byte	0x00		# any value 

	.long	SISIZE		# size in blocks of secondary image 
	.long	SILOAD		# load offset (usually 0) 
	.long 	SIOFF		# byte offset into secondary image 
	.long	(SISIZE + SILOAD + SIOFF)	# sum of previous 3 


	.align	2
	.globl	_from
_from:	.long	0

/*
 * After bootblock (LBN0) has been loaded into the first page 
 * of good memory by 11/750's ROM-code (transfer address
 * of bootblock-code is: base of good memory + 0x0C) registers
 * are initialized as:
 * 	R0:	type of boot-device
 *			0:	Massbus device
 *			1:	RK06/RK07
 *			2:	RL02
 *			17:	UDA50
 *			35:	TK50
 *			64:	TU58
 *	R1:	(UBA) address of UNIBUS I/O-page
 *		(MBA) address of boot device's adapter
 * 	R2:	(UBA) address of the boot device's CSR
 *		(MBA) controller number of boot device
 *	R6:	address of driver subroutine in ROM
 *
 * cont_750 reads in LBN1-15 for further execution.
 */
cont_750:
	movl	$_start, sp	# move stack to avoid clobbering the code
	pushr	$0x131		# save clobbered registers
	clrl	r4		# r4 == # of blocks transferred
	movab	_start,r5	# r5 have base address for next transfer
	pushl	r5		# ...on stack also (Why?)
1:	incl	r4		# increment block count
	movl	r4,r8		# LBN is in r8 for rom routine
	addl2	$0x200,r5	# Increase address for next read
	cmpl	$16,r4		# read 15 blocks?
	beql	2f		# Yep
	movl	r5,(sp)		# move address to stack also
	jsb	(r6)		# read 512 bytes
	blbs	r0,1b		# jump if read succeeded
	halt			# otherwise die...
2:	tstl	(sp)+		# remove boring arg from stack
	popr	$0x131		# restore clobbered registers
	brw	start_all	# Ok, continue...

/* uVAX main entry is at the start of the second disk block.  This is
 * needed for multi-arch CD booting where multiple architecture need
 * to shove stuff in boot block 0.
 */
	.org	0x200		# uVAX booted from disk starts here

start_uvax:
	movzbl	$2,_from	# Booted from subset-VMB
	brb	start_all

/*
 * start_all: stack already at RELOC, we save registers, move ourself
 * to RELOC and loads boot.
 */
start_all:
	movl	$_start, sp		# move stack to a better 
	pushr	$0x1fff			# save all regs, used later.

	subl3	$_start, $_edata, r0	# get size of text+data (w/o bss)
	moval	_start, r1		# get actual base-address of code
	subl3	$_start, $_end, r2	# get complete size (incl. bss)
	movl	$_start, r3		# get relocated base-address of code
	movc5	r0, (r1), $0, r2, (r3)	# copy code to new location
	
	movpsl	-(sp)
	movl	$relocated, -(sp)	# return-address on top of stack 
	rei	 			# can be replaced with new address
relocated:				# now relocation is done !!!
	movl	sp, _bootregs
	calls	$0, _Xmain		# call Xmain (gcc workaround)which is 
	halt				# not intended to return ...

/*
 * hoppabort() is called when jumping to the newly loaded program.
 */
ENTRY(hoppabort, 0)
	movl    4(ap),r6
	movl	_rpb,r11
	mnegl	$1,ap		# Hack to figure out boot device.
	jmp	2(r6)
#	calls   $0,(r6)
	halt

ENTRY(unit_init, R6|R7|R8|R9|R10|R11)
	movl	4(ap),r0		# init routine address
	movl	8(ap),r9		# RPB in r9
	movl	12(ap),r1		# VMB argument list
	callg	(r1),(r0)
	ret

# A bunch of functions unwanted in boot blocks.
ENTRY(getchar, 0)
	halt

ENTRY(putchar, 0)
	ret

ENTRY(printf, 0)
	ret

ENTRY(panic, 0)
	halt
