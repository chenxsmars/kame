/*	$NetBSD: if_lmc_media.c,v 1.2 2000/05/03 21:08:02 thorpej Exp $	*/

/*-
 * Copyright (c) 1997-1999 LAN Media Corporation (LMC)
 * All rights reserved.  www.lanmedia.com
 *
 * This code is written by Michael Graff <graff@vix.com> for LMC.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. All marketing or advertising materials mentioning features or
 *    use of this software must display the following acknowledgement:
 *      This product includes software developed by LAN Media Corporation
 *      and its contributors.
 * 4. Neither the name of LAN Media Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY LAN MEDIA CORPORATION AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>	/* only for declaration of wakeup() used by vm.h */
#if defined(__FreeBSD__)
#include <machine/clock.h>
#elif defined(__bsdi__) || defined(__NetBSD__)
#include <sys/device.h>
#endif

#if defined(__NetBSD__)
#include <dev/pci/pcidevs.h>
#include "rnd.h"
#if NRND > 0
#include <sys/rnd.h>
#endif
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/netisr.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>

#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <net/if_sppp.h>
#endif

#if defined(__bsdi__)
#if INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#include <net/netisr.h>
#include <net/if.h>
#include <net/netisr.h>
#include <net/if_types.h>
#include <net/if_p2p.h>
#include <net/if_c_hdlc.h>
#endif

#if defined(__FreeBSD__)
#include <vm/pmap.h>
#include <pci.h>
#if NPCI > 0
#include <pci/pcivar.h>
#include <pci/dc21040reg.h>
#endif
#endif /* __FreeBSD__ */

#if defined(__bsdi__)
#include <i386/pci/ic/dc21040.h>
#include <i386/isa/isa.h>
#include <i386/isa/icu.h>
#include <i386/isa/dma.h>
#include <i386/isa/isavar.h>
#include <i386/pci/pci.h>

#endif /* __bsdi__ */

#if defined(__NetBSD__)
#include <machine/bus.h>
#if defined(__alpha__)
#include <machine/intr.h>
#endif
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/ic/dc21040reg.h>
#endif /* __NetBSD__ */

/*
 * Sigh.  Every OS puts these in different places.
 */
#if defined(__NetBSD__)  
#include <dev/pci/if_lmc_types.h>
#include <dev/pci/if_lmcioctl.h>
#include <dev/pci/if_lmcvar.h>
#elif defined(__FreeBSD__)
#include "pci/if_lmc_types.h"
#include "pci/if_lmcioctl.h"
#include "pci/if_lmcvar.h"
#else /* BSDI */
#include "i386/pci/if_lmctypes.h"
#include "i386/pci/if_lmcioctl.h"
#include "i386/pci/if_lmcvar.h"
#endif

/*
 * For lack of a better place, put the T1 cable stuff here.
 */
char *lmc_t1_cables[] = {
	"V.10/RS423", "EIA530A", "reserved", "X.21", "V.35",
	"EIA449/EIA530/V.36", "V.28/EIA232", "none", NULL
};

/*
 * protocol independent method.
 */
static void	lmc_set_protocol(lmc_softc_t * const, lmc_ctl_t *);

/*
 * media independent methods to check on media status, link, light LEDs,
 * etc.
 */
static void	lmc_ds3_init(lmc_softc_t * const);
static void	lmc_ds3_default(lmc_softc_t * const);
static void	lmc_ds3_set_status(lmc_softc_t * const, lmc_ctl_t *);
static void	lmc_ds3_set_100ft(lmc_softc_t * const, int);
static int	lmc_ds3_get_link_status(lmc_softc_t * const);
static void	lmc_ds3_set_crc_length(lmc_softc_t * const, int);
static void	lmc_ds3_set_scram(lmc_softc_t * const, int);

static void	lmc_hssi_init(lmc_softc_t * const);
static void	lmc_hssi_default(lmc_softc_t * const);
static void	lmc_hssi_set_status(lmc_softc_t * const, lmc_ctl_t *);
static void	lmc_hssi_set_clock(lmc_softc_t * const, int);
static int	lmc_hssi_get_link_status(lmc_softc_t * const);
static void	lmc_hssi_set_link_status(lmc_softc_t * const, int);
static void	lmc_hssi_set_crc_length(lmc_softc_t * const, int);

static void	lmc_t1_init(lmc_softc_t * const);
static void	lmc_t1_default(lmc_softc_t * const);
static void	lmc_t1_set_status(lmc_softc_t * const, lmc_ctl_t *);
static void	lmc_t1_set_clock(lmc_softc_t * const, int);
static void	lmc_t1_set_speed(lmc_softc_t * const, lmc_ctl_t *);
static int	lmc_t1_get_link_status(lmc_softc_t * const);
static void	lmc_t1_set_link_status(lmc_softc_t * const, int);
static void	lmc_t1_set_crc_length(lmc_softc_t * const, int);

static void	lmc_dummy_set_1(lmc_softc_t * const, int);
static void	lmc_dummy_set2_1(lmc_softc_t * const, lmc_ctl_t *);

static inline void write_av9110_bit(lmc_softc_t *, int);
static void	write_av9110(lmc_softc_t *, u_int32_t, u_int32_t, u_int32_t,
			     u_int32_t, u_int32_t);

lmc_media_t lmc_ds3_media = {
	lmc_ds3_init,			/* special media init stuff */
	lmc_ds3_default,		/* reset to default state */
	lmc_ds3_set_status,		/* reset status to state provided */
	lmc_dummy_set_1,		/* set clock source */
	lmc_dummy_set2_1,		/* set line speed */
	lmc_ds3_set_100ft,		/* set cable length */
	lmc_ds3_set_scram,		/* set scrambler */
	lmc_ds3_get_link_status,	/* get link status */
	lmc_dummy_set_1,		/* set link status */
	lmc_ds3_set_crc_length,		/* set CRC length */
};

lmc_media_t lmc_hssi_media = {
	lmc_hssi_init,			/* special media init stuff */
	lmc_hssi_default,		/* reset to default state */
	lmc_hssi_set_status,		/* reset status to state provided */
	lmc_hssi_set_clock,		/* set clock source */
	lmc_dummy_set2_1,		/* set line speed */
	lmc_dummy_set_1,		/* set cable length */
	lmc_dummy_set_1,		/* set scrambler */
	lmc_hssi_get_link_status,	/* get link status */
	lmc_hssi_set_link_status,	/* set link status */
	lmc_hssi_set_crc_length,	/* set CRC length */
};

lmc_media_t lmc_t1_media = {
	lmc_t1_init,			/* special media init stuff */
	lmc_t1_default,			/* reset to default state */
	lmc_t1_set_status,		/* reset status to state provided */
	lmc_t1_set_clock,		/* set clock source */
	lmc_t1_set_speed,		/* set line speed */
	lmc_dummy_set_1,		/* set cable length */
	lmc_dummy_set_1,		/* set scrambler */
	lmc_t1_get_link_status,		/* get link status */
	lmc_t1_set_link_status,		/* set link status */
	lmc_t1_set_crc_length,		/* set CRC length */
};

static void
lmc_dummy_set_1(lmc_softc_t * const sc, int a)
{
}

static void
lmc_dummy_set2_1(lmc_softc_t * const sc, lmc_ctl_t *a)
{
}

/*
 *  HSSI methods
 */

static void
lmc_hssi_init(lmc_softc_t * const sc)
{
	sc->ictl.cardtype = LMC_CTL_CARDTYPE_LMC5200;

	lmc_gpio_mkoutput(sc, LMC_GEP_HSSI_CLOCK);
}

static void
lmc_hssi_default(lmc_softc_t * const sc)
{
	sc->lmc_miireg16 = LMC_MII16_LED_ALL;

	sc->lmc_media->set_link_status(sc, 0);
	sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_EXT);
	sc->lmc_media->set_crc_length(sc, LMC_CTL_CRC_LENGTH_16);
}

/*
 * Given a user provided state, set ourselves up to match it.  This will
 * always reset the card if needed.
 */
static void
lmc_hssi_set_status(lmc_softc_t * const sc, lmc_ctl_t *ctl)
{
	if (ctl == NULL) {
		sc->lmc_media->set_clock_source(sc, sc->ictl.clock_source);
		lmc_set_protocol(sc, NULL);

		return;
	}

	/*
	 * check for change in clock source
	 */
	if (ctl->clock_source && !sc->ictl.clock_source)
		sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_INT);
	else if (!ctl->clock_source && sc->ictl.clock_source)
		sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_EXT);

	lmc_set_protocol(sc, ctl);
}

/*
 * 1 == internal, 0 == external
 */
static void
lmc_hssi_set_clock(lmc_softc_t * const sc, int ie)
{
	if (ie == LMC_CTL_CLOCK_SOURCE_EXT) {
		sc->lmc_gpio |= LMC_GEP_HSSI_CLOCK;
		LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);
		sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_EXT;
		printf(LMC_PRINTF_FMT ": clock external\n",
		       LMC_PRINTF_ARGS);
	} else {
		sc->lmc_gpio &= ~(LMC_GEP_HSSI_CLOCK);
		LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);
		sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_INT;
		printf(LMC_PRINTF_FMT ": clock internal\n",
		       LMC_PRINTF_ARGS);
	}
}

/*
 * return hardware link status.
 * 0 == link is down, 1 == link is up.
 */
static int
lmc_hssi_get_link_status(lmc_softc_t * const sc)
{
	u_int16_t link_status;

	link_status = lmc_mii_readreg(sc, 0, 16);

	if ((link_status & LMC_MII16_HSSI_CA) == LMC_MII16_HSSI_CA)
		return 1;
	else
		return 0;
}

static void
lmc_hssi_set_link_status(lmc_softc_t * const sc, int state)
{
	if (state)
		sc->lmc_miireg16 |= LMC_MII16_HSSI_TA;
	else
		sc->lmc_miireg16 &= ~LMC_MII16_HSSI_TA;

	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

/*
 * 0 == 16bit, 1 == 32bit
 */
static void
lmc_hssi_set_crc_length(lmc_softc_t * const sc, int state)
{
	if (state == LMC_CTL_CRC_LENGTH_32) {
		/* 32 bit */
		sc->lmc_miireg16 |= LMC_MII16_HSSI_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_32;
	} else {
		/* 16 bit */
		sc->lmc_miireg16 &= ~LMC_MII16_HSSI_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_16;
	}

	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}


/*
 *  DS3 methods
 */

/*
 * Set cable length
 */
static void
lmc_ds3_set_100ft(lmc_softc_t * const sc, int ie)
{
	if (ie == LMC_CTL_CABLE_LENGTH_GT_100FT) {
		sc->lmc_miireg16 &= ~LMC_MII16_DS3_ZERO;
		sc->ictl.cable_length = LMC_CTL_CABLE_LENGTH_GT_100FT;
	} else if (ie == LMC_CTL_CABLE_LENGTH_LT_100FT) {
		sc->lmc_miireg16 |= LMC_MII16_DS3_ZERO;
		sc->ictl.cable_length = LMC_CTL_CABLE_LENGTH_LT_100FT;
	}
	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

static void
lmc_ds3_default(lmc_softc_t * const sc)
{
	sc->lmc_miireg16 = LMC_MII16_LED_ALL;

	sc->lmc_media->set_link_status(sc, 0);
	sc->lmc_media->set_cable_length(sc, LMC_CTL_CABLE_LENGTH_LT_100FT);
	sc->lmc_media->set_scrambler(sc, LMC_CTL_OFF);
	sc->lmc_media->set_crc_length(sc, LMC_CTL_CRC_LENGTH_16);
}

/*
 * Given a user provided state, set ourselves up to match it.  This will
 * always reset the card if needed.
 */
static void
lmc_ds3_set_status(lmc_softc_t * const sc, lmc_ctl_t *ctl)
{
	if (ctl == NULL) {
		sc->lmc_media->set_cable_length(sc, sc->ictl.cable_length);
		sc->lmc_media->set_scrambler(sc, sc->ictl.scrambler_onoff);
		lmc_set_protocol(sc, NULL);

		return;
	}

	/*
	 * check for change in cable length setting
	 */
	if (ctl->cable_length && !sc->ictl.cable_length)
		lmc_ds3_set_100ft(sc, LMC_CTL_CABLE_LENGTH_GT_100FT);
	else if (!ctl->cable_length && sc->ictl.cable_length)
		lmc_ds3_set_100ft(sc, LMC_CTL_CABLE_LENGTH_LT_100FT);

	/*
	 * Check for change in scrambler setting (requires reset)
	 */
	if (ctl->scrambler_onoff && !sc->ictl.scrambler_onoff)
		lmc_ds3_set_scram(sc, LMC_CTL_ON);
	else if (!ctl->scrambler_onoff && sc->ictl.scrambler_onoff)
		lmc_ds3_set_scram(sc, LMC_CTL_OFF);

	lmc_set_protocol(sc, ctl);
}

static void
lmc_ds3_init(lmc_softc_t * const sc)
{
	int i;

	sc->ictl.cardtype = LMC_CTL_CARDTYPE_LMC5245;

	/* writes zeros everywhere */
	for (i = 0 ; i < 21 ; i++) {
		lmc_mii_writereg(sc, 0, 17, i);
		lmc_mii_writereg(sc, 0, 18, 0);
	}

	/* set some essential bits */
	lmc_mii_writereg(sc, 0, 17, 1);
	lmc_mii_writereg(sc, 0, 18, 0x05);	/* ser, xtx */

	lmc_mii_writereg(sc, 0, 17, 5);
	lmc_mii_writereg(sc, 0, 18, 0x80);	/* emode */

	lmc_mii_writereg(sc, 0, 17, 14);
	lmc_mii_writereg(sc, 0, 18, 0x30);	/* rcgen, tcgen */

	/* clear counters and latched bits */
	for (i = 0 ; i < 21 ; i++) {
		lmc_mii_writereg(sc, 0, 17, i);
		lmc_mii_readreg(sc, 0, 18);
	}
}

/*
 * 1 == DS3 payload scrambled, 0 == not scrambled
 */
static void
lmc_ds3_set_scram(lmc_softc_t * const sc, int ie)
{
	if (ie == LMC_CTL_ON) {
		sc->lmc_miireg16 |= LMC_MII16_DS3_SCRAM;
		sc->ictl.scrambler_onoff = LMC_CTL_ON;
	} else {
		sc->lmc_miireg16 &= ~LMC_MII16_DS3_SCRAM;
		sc->ictl.scrambler_onoff = LMC_CTL_OFF;
	}
	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

/*
 * return hardware link status.
 * 0 == link is down, 1 == link is up.
 */
static int
lmc_ds3_get_link_status(lmc_softc_t * const sc)
{
	u_int16_t link_status;

	lmc_mii_writereg(sc, 0, 17, 7);
	link_status = lmc_mii_readreg(sc, 0, 18);

	if ((link_status & LMC_FRAMER_REG0_DLOS) == 0)
		return 1;
	else
		return 0;
}

/*
 * 0 == 16bit, 1 == 32bit
 */
static void
lmc_ds3_set_crc_length(lmc_softc_t * const sc, int state)
{
	if (state == LMC_CTL_CRC_LENGTH_32) {
		/* 32 bit */
		sc->lmc_miireg16 |= LMC_MII16_DS3_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_32;
	} else {
		/* 16 bit */
		sc->lmc_miireg16 &= ~LMC_MII16_DS3_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_16;
	}

	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}


/*
 *  T1 methods
 */

static void
lmc_t1_init(lmc_softc_t * const sc)
{
	u_int16_t mii17;
	int cable;

	sc->ictl.cardtype = LMC_CTL_CARDTYPE_LMC1000;

	mii17 = lmc_mii_readreg(sc, 0, 17);

	cable = (mii17 & LMC_MII17_T1_CABLE_MASK) >> LMC_MII17_T1_CABLE_SHIFT;
	sc->ictl.cable_type = cable;

	lmc_gpio_mkoutput(sc, LMC_GEP_T1_TXCLOCK);
}

static void
lmc_t1_default(lmc_softc_t * const sc)
{
	sc->lmc_miireg16 = LMC_MII16_LED_ALL;

	/*
	 * make TXCLOCK always be an output
	 */
	lmc_gpio_mkoutput(sc, LMC_GEP_T1_TXCLOCK);

	sc->lmc_media->set_link_status(sc, 0);
	sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_EXT);
	sc->lmc_media->set_speed(sc, NULL);
	sc->lmc_media->set_crc_length(sc, LMC_CTL_CRC_LENGTH_16);
}

/*
 * Given a user provided state, set ourselves up to match it.  This will
 * always reset the card if needed.
 */
static void
lmc_t1_set_status(lmc_softc_t * const sc, lmc_ctl_t *ctl)
{
	if (ctl == NULL) {
		sc->lmc_media->set_clock_source(sc, sc->ictl.clock_source);
		sc->lmc_media->set_speed(sc, &sc->ictl);
		lmc_set_protocol(sc, NULL);

		return;
	}

	/*
	 * check for change in clock source
	 */
	if (ctl->clock_source == LMC_CTL_CLOCK_SOURCE_INT
	    && sc->ictl.clock_source == LMC_CTL_CLOCK_SOURCE_EXT)
		sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_INT);
	else if (ctl->clock_source == LMC_CTL_CLOCK_SOURCE_EXT
		 && sc->ictl.clock_source == LMC_CTL_CLOCK_SOURCE_INT)
		sc->lmc_media->set_clock_source(sc, LMC_CTL_CLOCK_SOURCE_EXT);

	if (ctl->clock_rate != sc->ictl.clock_rate)
		sc->lmc_media->set_speed(sc, ctl);

	lmc_set_protocol(sc, ctl);
}

/*
 * 1 == internal, 0 == external
 */
static void
lmc_t1_set_clock(lmc_softc_t * const sc, int ie)
{
	if (ie == LMC_CTL_CLOCK_SOURCE_EXT) {
		sc->lmc_gpio &= ~(LMC_GEP_T1_TXCLOCK);
		LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);
		sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_EXT;
		printf(LMC_PRINTF_FMT ": clock external\n",
		       LMC_PRINTF_ARGS);
	} else {
		sc->lmc_gpio |= LMC_GEP_T1_TXCLOCK;
		LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);
		sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_INT;
		printf(LMC_PRINTF_FMT ": clock internal\n",
		       LMC_PRINTF_ARGS);
	}
}

static void
lmc_t1_set_speed(lmc_softc_t * const sc, lmc_ctl_t *ctl)
{
	lmc_ctl_t *ictl = &sc->ictl;
	lmc_av9110_t *av;

	if (ctl == NULL) {
		av = &ictl->cardspec.t1;
		ictl->clock_rate = 100000;
		av->f = ictl->clock_rate;
		av->n = 8;
		av->m = 25;
		av->v = 0;
		av->x = 0;
		av->r = 2;

		write_av9110(sc, av->n, av->m, av->v, av->x, av->r);
		return;
	}

	av = &ctl->cardspec.t1;

	if (av->f == 0)
		return;

	ictl->clock_rate = av->f;  /* really, this is the rate we are */
	ictl->cardspec.t1 = *av;

	write_av9110(sc, av->n, av->m, av->v, av->x, av->r);
}

/*
 * return hardware link status.
 * 0 == link is down, 1 == link is up.
 */
static int
lmc_t1_get_link_status(lmc_softc_t * const sc)
{
	u_int16_t link_status;

	/*
	 * missing CTS?  Hmm.  If we require CTS on, we may never get the
	 * link to come up, so omit it in this test.
	 *
	 * Also, it seems that with a loopback cable, DCD isn't asserted,
	 * so just check for things like this:
	 *	DSR _must_ be asserted.
	 *	One of DCD or CTS must be asserted.
	 */
	link_status = lmc_mii_readreg(sc, 0, 16);

	if ((link_status & LMC_MII16_T1_DSR) == 0)
		return (0);

	if ((link_status & (LMC_MII16_T1_CTS | LMC_MII16_T1_DCD)) == 0)
		return (0);

	return (1);
}

static void
lmc_t1_set_link_status(lmc_softc_t * const sc, int state)
{
	if (state) {
		sc->lmc_miireg16 |= (LMC_MII16_T1_DTR | LMC_MII16_T1_RTS);
		printf(LMC_PRINTF_FMT ": asserting DTR and RTS\n",
		       LMC_PRINTF_ARGS);
	} else {
		sc->lmc_miireg16 &= ~(LMC_MII16_T1_DTR | LMC_MII16_T1_RTS);
		printf(LMC_PRINTF_FMT ": deasserting DTR and RTS\n",
		       LMC_PRINTF_ARGS);
	}

	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

/*
 * 0 == 16bit, 1 == 32bit
 */
static void
lmc_t1_set_crc_length(lmc_softc_t * const sc, int state)
{
	if (state == LMC_CTL_CRC_LENGTH_32) {
		/* 32 bit */
		sc->lmc_miireg16 |= LMC_MII16_T1_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_32;
	} else {
		/* 16 bit */
		sc->lmc_miireg16 &= ~LMC_MII16_T1_CRC;
		sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_16;
	}

	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

/*
 * These are bits to program the T1 frequency generator
 */
static inline void
write_av9110_bit(lmc_softc_t *sc, int c)
{
	/*
	 * set the data bit as we need it.
	 */
	sc->lmc_gpio &= ~(LMC_GEP_SERIALCLK);
	if (c & 0x01)
		sc->lmc_gpio |= LMC_GEP_SERIAL;
	else
		sc->lmc_gpio &= ~(LMC_GEP_SERIAL);
	LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);

	/*
	 * set the clock to high
	 */
	sc->lmc_gpio |= LMC_GEP_SERIALCLK;
	LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);

	/*
	 * set the clock to low again.
	 */
	sc->lmc_gpio &= ~(LMC_GEP_SERIALCLK);
	LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);
}

static void
write_av9110(lmc_softc_t *sc, u_int32_t n, u_int32_t m, u_int32_t v,
	     u_int32_t x, u_int32_t r)
{
	int i;

#if 0
	printf(LMC_PRINTF_FMT ": speed %u, %d %d %d %d %d\n",
	       LMC_PRINTF_ARGS, sc->ictl.clock_rate,
	       n, m, v, x, r);
#endif

	sc->lmc_gpio |= LMC_GEP_T1_GENERATOR;
	sc->lmc_gpio &= ~(LMC_GEP_SERIAL | LMC_GEP_SERIALCLK);
	LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);

	/*
	 * Set the TXCLOCK, GENERATOR, SERIAL, and SERIALCLK
	 * as outputs.
	 */
	lmc_gpio_mkoutput(sc, (LMC_GEP_SERIAL | LMC_GEP_SERIALCLK
			       | LMC_GEP_T1_GENERATOR));

	sc->lmc_gpio &= ~(LMC_GEP_T1_GENERATOR);
	LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);

	/*
	 * a shifting we will go...
	 */
	for (i = 0 ; i < 7 ; i++)
		write_av9110_bit(sc, n >> i);
	for (i = 0 ; i < 7 ; i++)
		write_av9110_bit(sc, m >> i);
	for (i = 0 ; i < 1 ; i++)
		write_av9110_bit(sc, v >> i);
	for (i = 0 ; i < 2 ; i++)
		write_av9110_bit(sc, x >> i);
	for (i = 0 ; i < 2 ; i++)
		write_av9110_bit(sc, r >> i);
	for (i = 0 ; i < 5 ; i++)
		write_av9110_bit(sc, 0x17 >> i);

	/*
	 * stop driving serial-related signals
	 */
	lmc_gpio_mkinput(sc,
			 (LMC_GEP_SERIAL | LMC_GEP_SERIALCLK
			  | LMC_GEP_T1_GENERATOR));
}

static void
lmc_set_protocol(lmc_softc_t * const sc, lmc_ctl_t *ctl)
{
	if (ctl == 0) {
		sc->ictl.keepalive_onoff = LMC_CTL_ON;

		return;
	}

#if defined(__NetBSD__) || defined(__FreeBSD__)
	if (ctl->keepalive_onoff != sc->ictl.keepalive_onoff) {
		switch (ctl->keepalive_onoff) {
		case LMC_CTL_ON:
			printf(LMC_PRINTF_FMT ": enabling keepalive\n",
			       LMC_PRINTF_ARGS);
			sc->ictl.keepalive_onoff = LMC_CTL_ON;
			sc->lmc_sppp.pp_flags = PP_CISCO | PP_KEEPALIVE;
			break;
		case LMC_CTL_OFF:
			printf(LMC_PRINTF_FMT ": disabling keepalive\n",
			       LMC_PRINTF_ARGS);
			sc->ictl.keepalive_onoff = LMC_CTL_OFF;
			sc->lmc_sppp.pp_flags = PP_CISCO;
		}
	}
#endif
}
