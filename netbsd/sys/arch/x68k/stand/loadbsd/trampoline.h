/*
 *	definitions for trampoline code
 *
 *	written by Yasha (ITOH Yasufumi)
 *	public domain
 *
 *	$NetBSD: trampoline.h,v 1.1 1998/09/01 19:55:33 itohy Exp $
 */

#define MPU_68030	3
#define MPU_68040	4
#define MPU_68060	6

#define AREA_SET_REG	0x00E86001	/* (B) supervisor protection reg */

#define EXSPC		0x00EA0000	/* external SCSI board */
#define EXSPC_BDID	(EXSPC + 1)	/* (B) SCSI board bdid reg */

#define SIZE_TMPSTACK	8192

#ifndef __ASSEMBLER__
#include "../common/execkern.h"

struct tramparg {
	unsigned	bsr_inst;
#define TRAMP_BSR	0x61000000	/* bsr xxx */
	void		*tmp_stack;
	int		mpu_type;
	struct execkern_arg	xk;
};

extern char trampoline[], end_trampoline[];
#endif
