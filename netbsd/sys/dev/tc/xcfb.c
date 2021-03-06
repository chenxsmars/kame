/* $NetBSD: xcfb.c,v 1.8 1999/03/29 07:22:02 nisimura Exp $ */

/*
 * Copyright (c) 1998 Tohru Nishimura.  All rights reserved.
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
 *      This product includes software developed by Tohru Nishimura
 *	for the NetBSD Project.
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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: xcfb.c,v 1.8 1999/03/29 07:22:02 nisimura Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/rcons/raster.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/wscons/wsdisplayvar.h>

#include <dev/tc/tcvar.h>
#include <dev/ic/ims332reg.h>

#include <uvm/uvm_extern.h>

struct fb_devconfig {
	vaddr_t dc_vaddr;		/* memory space virtual base address */
	paddr_t dc_paddr;		/* memory space physical base address */
	vsize_t dc_size;		/* size of slot memory */
	int	dc_wid;			/* width of frame buffer */
	int	dc_ht;			/* height of frame buffer */
	int	dc_depth;		/* depth, bits per pixel */
	int	dc_rowbytes;		/* bytes in a FB scan line */
	vaddr_t dc_videobase;		/* base of flat frame buffer */
	struct raster	dc_raster;	/* raster description */
	struct rcons	dc_rcons;	/* raster blitter control info */
	int	   dc_blanked;		/* currently has video disabled */
};

struct hwcmap {
#define	CMAP_SIZE	256	/* 256 R/G/B entries */
	u_int8_t r[CMAP_SIZE];
	u_int8_t g[CMAP_SIZE];
	u_int8_t b[CMAP_SIZE];
};

struct hwcursor {
	struct wsdisplay_curpos cc_pos;
	struct wsdisplay_curpos cc_hot;
	struct wsdisplay_curpos cc_size;
#define	CURSOR_MAX_SIZE	64
	u_int8_t cc_color[6];
	u_int64_t cc_image[64 + 64];
};

#define	XCFB_FB_OFFSET	0x2000000	/* from module's base */
#define	XCFB_FB_SIZE	 0x100000	/* frame buffer size */

#define	IMS332_ADDRESS	0xbc140000
#define	IMS332_RPTR	0xbc1c0000
#define	IMS332_WPTR	0xbc1e0000

struct xcfb_softc {
	struct device sc_dev;
	struct fb_devconfig *sc_dc;	/* device configuration */
	struct hwcmap sc_cmap;		/* software copy of colormap */
	struct hwcursor sc_cursor;	/* software copy of cursor */
	/* XXX MAXINE can take PMAG-DV virtical retrace interrupt XXX */
	int nscreens;
	/* cursor coordiate is located at upper-left corner */
};

int  xcfbmatch __P((struct device *, struct cfdata *, void *));
void xcfbattach __P((struct device *, struct device *, void *));

struct cfattach xcfb_ca = {
	sizeof(struct xcfb_softc), xcfbmatch, xcfbattach,
};

void xcfb_getdevconfig __P((tc_addr_t, struct fb_devconfig *));
struct fb_devconfig xcfb_console_dc;
tc_addr_t xcfb_consaddr;

struct wsdisplay_emulops xcfb_emulops = {
	rcons_cursor,
	rcons_mapchar,
	rcons_putchar,
	rcons_copycols,
	rcons_erasecols,
	rcons_copyrows,
	rcons_eraserows,
	rcons_alloc_attr
};

struct wsscreen_descr xcfb_stdscreen = {
	"std",
	0, 0,	/* will be filled in -- XXX shouldn't, it's global */
	&xcfb_emulops,
	0, 0,
	0
};

const struct wsscreen_descr *_xcfb_scrlist[] = {
	&xcfb_stdscreen,
};

struct wsscreen_list xcfb_screenlist = {
	sizeof(_xcfb_scrlist) / sizeof(struct wsscreen_descr *), _xcfb_scrlist
};

int	xcfbioctl __P((void *, u_long, caddr_t, int, struct proc *));
int	xcfbmmap __P((void *, off_t, int));

int	xcfb_alloc_screen __P((void *, const struct wsscreen_descr *,
				      void **, int *, int *, long *));
void	xcfb_free_screen __P((void *, void *));
void	xcfb_show_screen __P((void *, void *));

struct wsdisplay_accessops xcfb_accessops = {
	xcfbioctl,
	xcfbmmap,
	xcfb_alloc_screen,
	xcfb_free_screen,
	xcfb_show_screen,
	0 /* load_font */
};

int  xcfb_cnattach __P((tc_addr_t));
void xcfbinit __P((struct fb_devconfig *));
void xcfb_screenblank __P((struct xcfb_softc *));

static int  set_cmap __P((struct xcfb_softc *, struct wsdisplay_cmap *));
static int  get_cmap __P((struct xcfb_softc *, struct wsdisplay_cmap *));
static int  set_cursor __P((struct xcfb_softc *, struct wsdisplay_cursor *));
static int  get_cursor __P((struct xcfb_softc *, struct wsdisplay_cursor *));
static void set_curpos __P((struct xcfb_softc *, struct wsdisplay_curpos *));
void ims332_loadcmap __P((struct hwcmap *));
void ims332_set_cursor __P((struct xcfb_softc *));
void ims332_set_curpos __P((struct xcfb_softc *));
void ims332_load_curcmap __P((struct xcfb_softc *));
void ims332_load_curshape __P((struct xcfb_softc *));
u_int32_t ims332_read_reg __P((int));
void ims332_write_reg __P((int, u_int32_t));

/*
 * Compose 2 bit/pixel cursor image.  
 *   M M M M I I I I		M I M I M I M I
 *	[ before ]		   [ after ]
 *   3 2 1 0 3 2 1 0		3 3 2 2 1 1 0 0
 *   7 6 5 4 7 6 5 4		7 7 6 6 5 5 4 4
 */
const static u_int8_t shuffle[256] = {
	0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15,
	0x40, 0x41, 0x44, 0x45, 0x50, 0x51, 0x54, 0x55,
	0x02, 0x03, 0x06, 0x07, 0x12, 0x13, 0x16, 0x17,
	0x42, 0x43, 0x46, 0x47, 0x52, 0x53, 0x56, 0x57,
	0x08, 0x09, 0x0c, 0x0d, 0x18, 0x19, 0x1c, 0x1d,
	0x48, 0x49, 0x4c, 0x4d, 0x58, 0x59, 0x5c, 0x5d,
	0x0a, 0x0b, 0x0e, 0x0f, 0x1a, 0x1b, 0x1e, 0x1f,
	0x4a, 0x4b, 0x4e, 0x4f, 0x5a, 0x5b, 0x5e, 0x5f,
	0x20, 0x21, 0x24, 0x25, 0x30, 0x31, 0x34, 0x35,
	0x60, 0x61, 0x64, 0x65, 0x70, 0x71, 0x74, 0x75,
	0x22, 0x23, 0x26, 0x27, 0x32, 0x33, 0x36, 0x37,
	0x62, 0x63, 0x66, 0x67, 0x72, 0x73, 0x76, 0x77,
	0x28, 0x29, 0x2c, 0x2d, 0x38, 0x39, 0x3c, 0x3d,
	0x68, 0x69, 0x6c, 0x6d, 0x78, 0x79, 0x7c, 0x7d,
	0x2a, 0x2b, 0x2e, 0x2f, 0x3a, 0x3b, 0x3e, 0x3f,
	0x6a, 0x6b, 0x6e, 0x6f, 0x7a, 0x7b, 0x7e, 0x7f,
	0x80, 0x81, 0x84, 0x85, 0x90, 0x91, 0x94, 0x95,
	0xc0, 0xc1, 0xc4, 0xc5, 0xd0, 0xd1, 0xd4, 0xd5,
	0x82, 0x83, 0x86, 0x87, 0x92, 0x93, 0x96, 0x97,
	0xc2, 0xc3, 0xc6, 0xc7, 0xd2, 0xd3, 0xd6, 0xd7,
	0x88, 0x89, 0x8c, 0x8d, 0x98, 0x99, 0x9c, 0x9d,
	0xc8, 0xc9, 0xcc, 0xcd, 0xd8, 0xd9, 0xdc, 0xdd,
	0x8a, 0x8b, 0x8e, 0x8f, 0x9a, 0x9b, 0x9e, 0x9f,
	0xca, 0xcb, 0xce, 0xcf, 0xda, 0xdb, 0xde, 0xdf,
	0xa0, 0xa1, 0xa4, 0xa5, 0xb0, 0xb1, 0xb4, 0xb5,
	0xe0, 0xe1, 0xe4, 0xe5, 0xf0, 0xf1, 0xf4, 0xf5,
	0xa2, 0xa3, 0xa6, 0xa7, 0xb2, 0xb3, 0xb6, 0xb7,
	0xe2, 0xe3, 0xe6, 0xe7, 0xf2, 0xf3, 0xf6, 0xf7,
	0xa8, 0xa9, 0xac, 0xad, 0xb8, 0xb9, 0xbc, 0xbd,
	0xe8, 0xe9, 0xec, 0xed, 0xf8, 0xf9, 0xfc, 0xfd,
	0xaa, 0xab, 0xae, 0xaf, 0xba, 0xbb, 0xbe, 0xbf,
	0xea, 0xeb, 0xee, 0xef, 0xfa, 0xfb, 0xfe, 0xff,
};

int
xcfbmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct tc_attach_args *ta = aux;

	if (strncmp("PMAG-DV ", ta->ta_modname, TC_ROM_LLEN) != 0)
		return (0);

	return (1);
}

void
xcfb_getdevconfig(dense_addr, dc)
	tc_addr_t dense_addr;
	struct fb_devconfig *dc;
{
	struct raster *rap;
	struct rcons *rcp;
	int i;

	dc->dc_vaddr = dense_addr;
	dc->dc_paddr = MIPS_KSEG1_TO_PHYS(dc->dc_vaddr + XCFB_FB_OFFSET);

	dc->dc_wid = 1024;
	dc->dc_ht = 768;
	dc->dc_depth = 8;
	dc->dc_rowbytes = 1024;
	dc->dc_videobase = dc->dc_vaddr + XCFB_FB_OFFSET;
	dc->dc_blanked = 0;

	/* initialize colormap and cursor resource */
	xcfbinit(dc);

	/* clear the screen */
	for (i = 0; i < dc->dc_ht * dc->dc_rowbytes; i += sizeof(u_int32_t))
		*(u_int32_t *)(dc->dc_videobase + i) = 0;

	/* initialize the raster */
	rap = &dc->dc_raster;
	rap->width = dc->dc_wid;
	rap->height = dc->dc_ht;
	rap->depth = dc->dc_depth;
	rap->linelongs = dc->dc_rowbytes / sizeof(u_int32_t);
	rap->pixels = (u_int32_t *)dc->dc_videobase;

	/* initialize the raster console blitter */
	rcp = &dc->dc_rcons;
	rcp->rc_sp = rap;
	rcp->rc_crow = rcp->rc_ccol = -1;
	rcp->rc_crowp = &rcp->rc_crow;
	rcp->rc_ccolp = &rcp->rc_ccol;
	rcons_init(rcp, 34, 80);

	xcfb_stdscreen.nrows = dc->dc_rcons.rc_maxrow;
	xcfb_stdscreen.ncols = dc->dc_rcons.rc_maxcol;
}

void
xcfbattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct xcfb_softc *sc = (struct xcfb_softc *)self;
	struct tc_attach_args *ta = aux;
	struct wsemuldisplaydev_attach_args waa;
	struct hwcmap *cm;
	int console, i;

	console = (ta->ta_addr == xcfb_consaddr);
	if (console) {
		sc->sc_dc = &xcfb_console_dc;
		sc->nscreens = 1;
	}
	else {
		sc->sc_dc = (struct fb_devconfig *)
		    malloc(sizeof(struct fb_devconfig), M_DEVBUF, M_WAITOK);
		xcfb_getdevconfig(ta->ta_addr, sc->sc_dc);
	}
	printf(": %d x %d, %dbpp\n", sc->sc_dc->dc_wid, sc->sc_dc->dc_ht,
	    sc->sc_dc->dc_depth);

	cm = &sc->sc_cmap;
	cm->r[0] = cm->g[0] = cm->b[0] = 0;
	for (i = 1; i < CMAP_SIZE; i++) {
		cm->r[i] = cm->g[i] = cm->b[i] = 0xff;
	}

	/* PMAG-DV emits no interrupt */

	waa.console = console;
	waa.scrdata = &xcfb_screenlist;
	waa.accessops = &xcfb_accessops;
	waa.accesscookie = sc;

	config_found(self, &waa, wsemuldisplaydevprint);
}

int
xcfbioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct xcfb_softc *sc = v;
	struct fb_devconfig *dc = sc->sc_dc;
	int turnoff, error;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_XCFB;
		return (0);

	case WSDISPLAYIO_GINFO:
#define	wsd_fbip ((struct wsdisplay_fbinfo *)data)
		wsd_fbip->height = sc->sc_dc->dc_ht;
		wsd_fbip->width = sc->sc_dc->dc_wid;
		wsd_fbip->depth = sc->sc_dc->dc_depth;
		wsd_fbip->cmsize = CMAP_SIZE;
#undef fbt
		return (0);

	case WSDISPLAYIO_GETCMAP:
		return get_cmap(sc, (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_PUTCMAP:
		error = set_cmap(sc, (struct wsdisplay_cmap *)data);
		if (error == 0)
			ims332_loadcmap(&sc->sc_cmap);
		return (error);

	case WSDISPLAYIO_SVIDEO:
		turnoff = *(int *)data == WSDISPLAYIO_VIDEO_OFF;
		if ((dc->dc_blanked == 0) ^ turnoff) {
			dc->dc_blanked = turnoff;
			xcfb_screenblank(sc);
		}
		return (0);

	case WSDISPLAYIO_GVIDEO:
		*(u_int *)data = dc->dc_blanked ?
		    WSDISPLAYIO_VIDEO_OFF : WSDISPLAYIO_VIDEO_ON;
		return (0);

	case WSDISPLAYIO_GCURPOS:
		*(struct wsdisplay_curpos *)data = sc->sc_cursor.cc_pos;
		return (0);

	case WSDISPLAYIO_SCURPOS:
		set_curpos(sc, (struct wsdisplay_curpos *)data);
		ims332_set_curpos(sc);
		return (0);

	case WSDISPLAYIO_GCURMAX:
		((struct wsdisplay_curpos *)data)->x =
		((struct wsdisplay_curpos *)data)->y = CURSOR_MAX_SIZE;
		return (0);

	case WSDISPLAYIO_GCURSOR:
		return get_cursor(sc, (struct wsdisplay_cursor *)data);

	case WSDISPLAYIO_SCURSOR:
		return set_cursor(sc, (struct wsdisplay_cursor *)data);
	}
	return ENOTTY;
}

int
xcfbmmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct xcfb_softc *sc = v;

	if (offset >= XCFB_FB_SIZE || offset < 0)
		return -1;
	return mips_btop(sc->sc_dc->dc_paddr + offset);
}

int
xcfb_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct xcfb_softc *sc = v;
	long defattr;

	if (sc->nscreens > 0)
		return (ENOMEM);

	*cookiep = &sc->sc_dc->dc_rcons; /* one and only for now */
	*curxp = 0;
	*curyp = 0;
	rcons_alloc_attr(&sc->sc_dc->dc_rcons, 0, 0, 0, &defattr);
	*attrp = defattr;
	sc->nscreens++;
	return (0);
}

void
xcfb_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct xcfb_softc *sc = v;

	if (sc->sc_dc == &xcfb_console_dc)
		panic("xcfb_free_screen: console");

	sc->nscreens--;
}

void
xcfb_show_screen(v, cookie)
	void *v;
	void *cookie;
{
}

int
xcfb_cnattach(addr)
        tc_addr_t addr;
{
        struct fb_devconfig *dcp = &xcfb_console_dc;
        long defattr;

        xcfb_getdevconfig(addr, dcp);
 
        rcons_alloc_attr(&dcp->dc_rcons, 0, 0, 0, &defattr);

        wsdisplay_cnattach(&xcfb_stdscreen, &dcp->dc_rcons,
                           0, 0, defattr);
        xcfb_consaddr = addr;
        return(0);
}

void
xcfbinit(dc)
	struct fb_devconfig *dc;
{
	u_int32_t csr;
	int i;

	csr = IMS332_BPP_8 | IMS332_CSR_A_DMA_DISABLE
		| IMS332_CSR_A_VTG_ENABLE | IMS332_CSR_A_DISABLE_CURSOR;
	ims332_write_reg(IMS332_REG_CSR_A, csr);

	ims332_write_reg(IMS332_REG_COLOR_MASK, 0xffffff);

	/* build sane colormap */
	ims332_write_reg(IMS332_REG_LUT_BASE, 0);
	for (i = 1; i < CMAP_SIZE; i++)
		ims332_write_reg(IMS332_REG_LUT_BASE + i, 0xffffff);

	/* clear out cursor image */
	for (i = 0; i < 512; i++)
		ims332_write_reg(IMS332_REG_CURSOR_RAM + i, 0);

	/*
	 * 2 bit/pixel cursor.  Assign MSB for cursor mask and LSB for
	 * cursor image.  LUT_1 for mask color, while LUT_2 for
	 * image color.  LUT_0 will be never used.
	 */
	ims332_write_reg(IMS332_REG_CURSOR_LUT_0, 0);
	ims332_write_reg(IMS332_REG_CURSOR_LUT_1, 0xffffff);
	ims332_write_reg(IMS332_REG_CURSOR_LUT_2, 0xffffff);
}

void
xcfb_screenblank(sc)
	struct xcfb_softc *sc;
{
	struct fb_devconfig *dc = sc->sc_dc;
	u_int16_t csr;

	if (dc->dc_blanked) {
		/* blank screen */
		ims332_write_reg(IMS332_REG_LUT_BASE, 0);
		ims332_write_reg(IMS332_REG_COLOR_MASK, 0);
		csr = ims332_read_reg(IMS332_REG_CSR_A);
		csr |= IMS332_CSR_A_DISABLE_CURSOR;
		ims332_write_reg(IMS332_REG_CSR_A, csr);
	}
	else {
		/* restore current colormap */
		ims332_loadcmap(&sc->sc_cmap);
		/* turnon hardware cursor */
		csr = ims332_read_reg(IMS332_REG_CSR_A);
		csr &= ~IMS332_CSR_A_DISABLE_CURSOR;
		ims332_write_reg(IMS332_REG_CSR_A, csr);
	}
}

static int
get_cmap(sc, p)
	struct xcfb_softc *sc;
	struct wsdisplay_cmap *p;
{
	u_int index = p->index, count = p->count;

	if (index >= CMAP_SIZE || (index + count) > CMAP_SIZE)
		return (EINVAL);

	if (!uvm_useracc(p->red, count, B_WRITE) ||
	    !uvm_useracc(p->green, count, B_WRITE) ||
	    !uvm_useracc(p->blue, count, B_WRITE))
		return (EFAULT);

	copyout(&sc->sc_cmap.r[index], p->red, count);
	copyout(&sc->sc_cmap.g[index], p->green, count);
	copyout(&sc->sc_cmap.b[index], p->blue, count);

	return (0);
}

static int
set_cmap(sc, p)
	struct xcfb_softc *sc;
	struct wsdisplay_cmap *p;
{
	u_int index = p->index, count = p->count;

	if (index >= CMAP_SIZE || (index + count) > CMAP_SIZE)
		return (EINVAL);

	if (!uvm_useracc(p->red, count, B_READ) ||
	    !uvm_useracc(p->green, count, B_READ) ||
	    !uvm_useracc(p->blue, count, B_READ))
		return (EFAULT);

	copyin(p->red, &sc->sc_cmap.r[index], count);
	copyin(p->green, &sc->sc_cmap.g[index], count);
	copyin(p->blue, &sc->sc_cmap.b[index], count);

	return (0);
}

static int
set_cursor(sc, p)
	struct xcfb_softc *sc;
	struct wsdisplay_cursor *p;
{
#define	cc (&sc->sc_cursor)
	int v, index, count;
	u_int32_t csr;

	v = p->which;
	if (v & WSDISPLAY_CURSOR_DOCMAP) {
		index = p->cmap.index;
		count = p->cmap.count;

		if (index >= 2 || index + count > 2)
			return (EINVAL);
		if (!uvm_useracc(p->cmap.red, count, B_READ) ||
		    !uvm_useracc(p->cmap.green, count, B_READ) ||
		    !uvm_useracc(p->cmap.blue, count, B_READ))
			return (EFAULT);

		copyin(p->cmap.red, &cc->cc_color[index], count);
		copyin(p->cmap.green, &cc->cc_color[index + 2], count);
		copyin(p->cmap.blue, &cc->cc_color[index + 4], count);
		ims332_load_curcmap(sc);
	}
	if (v & WSDISPLAY_CURSOR_DOSHAPE) {
		if (p->size.x > CURSOR_MAX_SIZE || p->size.y > CURSOR_MAX_SIZE)
			return (EINVAL);
		count = ((p->size.x < 33) ? 4 : 8) * p->size.y;
		if (!uvm_useracc(p->image, count, B_READ) ||
		    !uvm_useracc(p->mask, count, B_READ))
			return (EFAULT);
		cc->cc_size = p->size;
		memset(cc->cc_image, 0, sizeof cc->cc_image);
		copyin(p->image, cc->cc_image, count);
		copyin(p->mask, cc->cc_image+CURSOR_MAX_SIZE, count);
		ims332_load_curshape(sc);
	}
	if (v & WSDISPLAY_CURSOR_DOCUR) {
		cc->cc_hot = p->hot;
		csr = ims332_read_reg(IMS332_REG_CSR_A);
		if (p->enable)
			csr &= ~IMS332_CSR_A_DISABLE_CURSOR;
		else
			csr |= IMS332_CSR_A_DISABLE_CURSOR;
		ims332_write_reg(IMS332_REG_CSR_A, csr);
	}
	if (v & WSDISPLAY_CURSOR_DOPOS) {
		set_curpos(sc, &p->pos);
		ims332_set_curpos(sc);
	}

	return (0);
#undef cc
}

static int
get_cursor(sc, p)
	struct xcfb_softc *sc;
	struct wsdisplay_cursor *p;
{
	return (ENOTTY); /* XXX */
}

static void
set_curpos(sc, curpos)
	struct xcfb_softc *sc;
	struct wsdisplay_curpos *curpos;
{
	struct fb_devconfig *dc = sc->sc_dc;
	int x = curpos->x, y = curpos->y;

	if (y < 0)
		y = 0;
	else if (y > dc->dc_ht)
		y = dc->dc_ht;
	if (x < 0)
		x = 0;
	else if (x > dc->dc_wid)
		x = dc->dc_wid;
	sc->sc_cursor.cc_pos.x = x;
	sc->sc_cursor.cc_pos.y = y;
}

void
ims332_loadcmap(cm)
	struct hwcmap *cm;
{
	int i;
	u_int32_t rgb;
	
	for (i = 0; i < CMAP_SIZE; i++) {
		rgb = cm->b[i] << 16 | cm->g[i] << 8 | cm->r[i];
		ims332_write_reg(IMS332_REG_LUT_BASE + i, rgb);
	}
}

void
ims332_set_curpos(sc)
	struct xcfb_softc *sc;
{
	struct wsdisplay_curpos *curpos = &sc->sc_cursor.cc_pos;
	u_int32_t pos;

	pos = (curpos->x & 0xfff) << 12 | (curpos->y & 0xfff);
	ims332_write_reg(IMS332_REG_CURSOR_LOC, pos);
}

void
ims332_load_curcmap(sc)
	struct xcfb_softc *sc;
{
	u_int8_t *cp = sc->sc_cursor.cc_color;
	u_int32_t rgb;

	/* cursor background */
	rgb = cp[5] << 16 | cp[3] << 8 | cp[1];
	ims332_write_reg(IMS332_REG_CURSOR_LUT_1, rgb);

	/* cursor foreground */
	rgb = cp[4] << 16 | cp[2] << 8 | cp[0];
	ims332_write_reg(IMS332_REG_CURSOR_LUT_2, rgb);
}

void
ims332_load_curshape(sc)
	struct xcfb_softc *sc;
{
	unsigned i, img, msk, bits;
	u_int8_t u, *ip, *mp;

	ip = (u_int8_t *)sc->sc_cursor.cc_image;
	mp = (u_int8_t *)(sc->sc_cursor.cc_image+CURSOR_MAX_SIZE);

	i = 0;
	/* 64 pixel scan line is consisted with 8 halfward cursor ram */
	while (i < sc->sc_cursor.cc_size.y * 8) {
		/* pad right half 32 pixel when smaller than 33 */
		if ((i & 0x4) && sc->sc_cursor.cc_size.x < 33)
			bits = 0;
		else {
			img = *ip++;
			msk = *mp++;
			img &= msk;	/* cookie off image */
			u = (msk & 0x0f) << 4 | (img & 0x0f);
			bits = shuffle[u];
			u = (msk & 0xf0) | (img & 0xf0) >> 4;
			bits = (shuffle[u] << 8) | bits;
		}
		ims332_write_reg(IMS332_REG_CURSOR_RAM + i, bits);
		i += 1;
	}
	/* pad unoccupied scan lines */
	while (i < CURSOR_MAX_SIZE * 8) {
		ims332_write_reg(IMS332_REG_CURSOR_RAM + i, 0);
		i += 1;
	}
}

u_int32_t
ims332_read_reg(regno)
	int regno;
{
	caddr_t imsreg = (caddr_t)IMS332_ADDRESS;
	caddr_t rptr = (caddr_t)IMS332_RPTR + (regno << 4);
	u_int v0, v1;

	v1 = *(volatile u_int32_t *)imsreg;
	v0 = *(volatile u_int16_t *)rptr;
	return (v1 & 0xff00) << 8 | v0;
}

void
ims332_write_reg(regno, val)
	int regno;
	u_int32_t val;
{
	caddr_t imsreg = (caddr_t)IMS332_ADDRESS;
	caddr_t wptr = (caddr_t)IMS332_WPTR + (regno << 4);

	*(volatile u_int32_t *)imsreg = (val & 0xff0000) >> 8;
	*(volatile u_int16_t *)wptr = val;
}
