/*	$NetBSD: miidevs.h,v 1.5 1999/03/24 21:07:26 thorpej Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: miidevs,v 1.5 1999/03/24 21:07:04 thorpej Exp 
 */

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * List of known MII OUIs
 */

#define	MII_OUI_AMD	0x00606e	/* Advanced Micro Devices */
#define	MII_OUI_DAVICOM	0x006040	/* Davicom Semiconductor */
#define	MII_OUI_ICS	0x00057d	/* Integrated Circuit Systems */
#define	MII_OUI_INTEL	0x00aa00	/* Intel */
#define	MII_OUI_LEVEL1	0x1e0400	/* Level 1 */
#define	MII_OUI_NATSEMI	0x080017	/* National Semiconductor */
#define	MII_OUI_QUALSEMI	0x006051	/* Quality Semiconductor */
#define	MII_OUI_SEEQ	0x0005be	/* Seeq */
#define	MII_OUI_SIS	0x000760	/* Silicon Integrated Systems */
#define	MII_OUI_TI	0x100014	/* Texas Instruments */

/*
 * List of known models.  Grouped by oui.
 */

/* Advanced Micro Devices PHYs */
#define	MII_MODEL_AMD_79C873	0x0000
#define	MII_STR_AMD_79C873	"Am79C873 10/100 media interface"

/* Davicom Semiconductor PHYs */
#define	MII_MODEL_DAVICOM_DM9101	0x0000
#define	MII_STR_DAVICOM_DM9101	"DM9101 10/100 media interface"

/* Integrated Circuit Systems PHYs */
#define	MII_MODEL_ICS_1890	0x0002
#define	MII_STR_ICS_1890	"ICS1890 10/100 media interface"

/* Intel PHYs */
#define	MII_MODEL_INTEL_I82555	0x0015
#define	MII_STR_INTEL_I82555	"i82555 10/100 media interface"

/* Level 1 PHYs */
#define	MII_MODEL_LEVEL1_LXT970	0x0000
#define	MII_STR_LEVEL1_LXT970	"LXT970 10/100 media interface"

/* National Semiconductor PHYs */
#define	MII_MODEL_NATSEMI_DP83840	0x0000
#define	MII_STR_NATSEMI_DP83840	"DP83840 10/100 media interface"
#define	MII_MODEL_NATSEMI_DP83843	0x0001
#define	MII_STR_NATSEMI_DP83843	"DP83843 10/100 media interface"

/* Quality Semiconductor PHYs */
#define	MII_MODEL_QUALSEMI_QS6612	0x0000
#define	MII_STR_QUALSEMI_QS6612	"QS6612 10/100 media interface"

/* Seeq PHYs */
#define	MII_MODEL_SEEQ_80220	0x0003
#define	MII_STR_SEEQ_80220	"Seeq 80220 10/100 media interface"
#define	MII_MODEL_SEEQ_84220	0x0004
#define	MII_STR_SEEQ_84220	"Seeq 84220 10/100 media interface"

/* Silicon Integrated Systems PHYs */
#define	MII_MODEL_SIS_900	0x0000
#define	MII_STR_SIS_900	"SiS 900 10/100 media interface"

/* Texas Instruments PHYs */
#define	MII_MODEL_TI_TLAN10T	0x0001
#define	MII_STR_TI_TLAN10T	"ThunderLAN 10baseT media interface"
#define	MII_MODEL_TI_100VGPMI	0x0002
#define	MII_STR_TI_100VGPMI	"ThunderLAN 100VG-AnyLan media interface"
