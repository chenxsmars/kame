/*
 * Copyright 2002 by Peter Grehan. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/powerpc/powermac/macio.c,v 1.1 2002/09/19 04:52:07 grehan Exp $
 */

/*
 * Driver for KeyLargo/Pangea, the MacPPC south bridge ASIC.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <machine/vmparam.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>

#include <machine/resource.h>

#include <dev/ofw/openfirm.h>

#include <powerpc/powermac/maciovar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

static MALLOC_DEFINE(M_MACIO, "macio", "macio device information");

static int  macio_probe(device_t);
static int  macio_attach(device_t);
static int  macio_print_child(device_t dev, device_t child);
static void macio_probe_nomatch(device_t, device_t);
static int  macio_read_ivar(device_t, device_t, int, uintptr_t *);
static int  macio_write_ivar(device_t, device_t, int, uintptr_t);
static struct   resource *macio_alloc_resource(device_t, device_t, int, int *,
					       u_long, u_long, u_long, u_int);
static int  macio_activate_resource(device_t, device_t, int, int,
				    struct resource *);
static int  macio_deactivate_resource(device_t, device_t, int, int,
				      struct resource *);
static int  macio_release_resource(device_t, device_t, int, int,
				   struct resource *);
static struct resource_list *macio_get_resource_list (device_t, device_t);

/*
 * Bus interface definition
 */
static device_method_t macio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,         macio_probe),
	DEVMETHOD(device_attach,        macio_attach),
	DEVMETHOD(device_detach,        bus_generic_detach),
	DEVMETHOD(device_shutdown,      bus_generic_shutdown),
	DEVMETHOD(device_suspend,       bus_generic_suspend),
	DEVMETHOD(device_resume,        bus_generic_resume),
	
	/* Bus interface */
	DEVMETHOD(bus_print_child,      macio_print_child),
	DEVMETHOD(bus_probe_nomatch,    macio_probe_nomatch),
	DEVMETHOD(bus_read_ivar,        macio_read_ivar),
	DEVMETHOD(bus_write_ivar,       macio_write_ivar),
	DEVMETHOD(bus_setup_intr,       bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,    bus_generic_teardown_intr),	

        DEVMETHOD(bus_alloc_resource,   macio_alloc_resource),
        DEVMETHOD(bus_release_resource, macio_release_resource),
        DEVMETHOD(bus_activate_resource, macio_activate_resource),
        DEVMETHOD(bus_deactivate_resource, macio_deactivate_resource),
        DEVMETHOD(bus_get_resource_list, macio_get_resource_list),	

	{ 0, 0 }
};

static driver_t macio_pci_driver = {
        "macio",
        macio_methods,
	sizeof(struct macio_softc)
};

devclass_t macio_devclass;

DRIVER_MODULE(macio, pci, macio_pci_driver, macio_devclass, 0, 0);


/*
 * PCI ID search table
 */
static struct macio_pci_dev {
        u_int32_t  mpd_devid;
	char    *mpd_desc;
} macio_pci_devlist[] = {
	{ 0x0025106b, "Pangea I/O Controller" },
	{ 0x0022106b, "KeyLargo I/O Controller" },
        { 0, NULL }
};

/*
 * Devices to exclude from the probe
 * XXX some of these may be required in the future...
 */
static char *macio_excl_name[] = {
	"interrupt-controller",
	"escc-legacy",
	"gpio",
	"timer",
        NULL
};

static int
macio_inlist(char *name)
{
        int i;

        for (i = 0; macio_excl_name[i] != NULL; i++)
                if (strcmp(name, macio_excl_name[i]) == 0)
                        return (1);
        return (0);
}


/*
 * Add an interrupt to the dev's resource list if present
 */
static void
macio_add_intr(phandle_t devnode, struct macio_devinfo *dinfo)
{
        u_int intr = -1;
	
        if ((OF_getprop(devnode, "interrupts", &intr, sizeof(intr)) != -1) ||
	    (OF_getprop(devnode, "AAPL,interrupts", 
			&intr, sizeof(intr) != -1))) {
                resource_list_add(&dinfo->mdi_resources, 
                                  SYS_RES_IRQ, 0, intr, intr, 1);
        }
        dinfo->mdi_interrupt = intr;
}


static void
macio_add_reg(phandle_t devnode, struct macio_devinfo *dinfo)
{
        u_int size;
	u_int start;
	u_int end;
	
        size = OF_getprop(devnode, "reg", dinfo->mdi_reg,
                          sizeof(dinfo->mdi_reg));
	
        if (size != -1) {

		/*
		 * Only do a single range for the moment...
		 */
                dinfo->mdi_nregs = 1;
		start = dinfo->mdi_reg[0];
		end = start + dinfo->mdi_reg[1] - 1;
		resource_list_add(&dinfo->mdi_resources, SYS_RES_MEMORY, 0,
				  start, end, end - start + 1);
        } else {
		dinfo->mdi_nregs = -1;
	}
}

/*
 * PCI probe
 */
static int
macio_probe(device_t dev)
{
        int i;
        u_int32_t devid;
	
        devid = pci_get_devid(dev);
        for (i = 0; macio_pci_devlist[i].mpd_desc != NULL; i++) {
                if (devid == macio_pci_devlist[i].mpd_devid) {
                        device_set_desc(dev, macio_pci_devlist[i].mpd_desc);
                        return (0);
                }
        }
	
        return (ENXIO);	
}

/*
 * PCI attach: scan OpenFirmware child nodes, and attach these as children
 * of the macio bus
 */
static int 
macio_attach(device_t dev)
{
	struct macio_softc *sc;
        struct macio_devinfo *dinfo;
        phandle_t  root;
	phandle_t  child;
        device_t cdev;
        u_int reg[3];
	char *name, *type;

	sc = device_get_softc(dev);
	root = sc->sc_node = OF_finddevice("mac-io");
	
	/*
	 * Locate the device node and it's base address
	 */
	if (OF_getprop(root, "assigned-addresses", 
		       reg, sizeof(reg)) < sizeof(reg)) {
		return (ENXIO);
	}

	sc->sc_base = reg[2];
	sc->sc_size = MACIO_REG_SIZE;

	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "IOBus Device Memory";
	if (rman_init(&sc->sc_mem_rman) != 0) {
		device_printf(dev,
			      "failed to init mem range resources\n");
		return (ENXIO);
	}
	rman_manage_region(&sc->sc_mem_rman, 0, sc->sc_size);	

	/*
	 * Iterate through the sub-devices
	 */
	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		OF_getprop_alloc(child, "name", 1, (void **)&name);
		OF_getprop_alloc(child, "device_type", 1, (void **)&type);

		if (macio_inlist(name)) {
			free(name, M_OFWPROP);
			free(type, M_OFWPROP);
			continue;
		}

		cdev = device_add_child(dev, NULL, -1);
		if (cdev != NULL) {
			dinfo = malloc(sizeof(*dinfo), M_MACIO, M_WAITOK);
			memset(dinfo, 0, sizeof(*dinfo));
			resource_list_init(&dinfo->mdi_resources);
			dinfo->mdi_node = child;
			dinfo->mdi_name = name;
			dinfo->mdi_device_type = type;
			macio_add_intr(child, dinfo);
			macio_add_reg(child, dinfo);
			device_set_ivars(cdev, dinfo);
		} else {
			free(name, M_OFWPROP);
			free(type, M_OFWPROP);
		}
	}

	return (bus_generic_attach(dev));
}


static int
macio_print_child(device_t dev, device_t child)
{
        struct macio_devinfo *dinfo;
        struct resource_list *rl;
        int retval = 0;

        dinfo = device_get_ivars(child);
        rl = &dinfo->mdi_resources;

        retval += bus_print_child_header(dev, child);

        retval += resource_list_print_type(rl, "mem", SYS_RES_MEMORY, "%#lx");
        retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%ld");

        retval += bus_print_child_footer(dev, child);

        return (retval);
}


static void
macio_probe_nomatch(device_t dev, device_t child)
{
	u_int *regs;

	if (bootverbose) {
		regs = macio_get_regs(child);

		device_printf(dev, "<%s, %s>", macio_get_devtype(child),
			      macio_get_name(child));
		printf("at offset 0x%x (no driver attached)\n", regs[0]);
	}
}


static int
macio_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
        struct macio_devinfo *dinfo;

        if ((dinfo = device_get_ivars(child)) == 0)
                return (ENOENT);

        switch (which) {
        case MACIO_IVAR_NODE:
                *result = dinfo->mdi_node;
                break;
        case MACIO_IVAR_NAME:
                *result = (uintptr_t)dinfo->mdi_name;
                break;
        case MACIO_IVAR_DEVTYPE:
                *result = (uintptr_t)dinfo->mdi_device_type;
                break;
        case MACIO_IVAR_NREGS:
                *result = dinfo->mdi_nregs;
                break;
        case MACIO_IVAR_REGS:
                *result = (uintptr_t) &dinfo->mdi_reg[0];
                break;
        default:
                return (ENOENT);
        }

        return (0);
}


static int
macio_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	return (EINVAL);
}


static struct resource *
macio_alloc_resource(device_t bus, device_t child, int type, int *rid,
		     u_long start, u_long end, u_long count, u_int flags)
{
	struct macio_softc *sc;
	int  needactivate;
	struct  resource *rv;
	struct  rman *rm;
	bus_space_tag_t tagval;

	sc = device_get_softc(bus);

	needactivate = flags & RF_ACTIVE;
	flags &= ~RF_ACTIVE;

	switch (type) {
	case SYS_RES_MEMORY:
	case SYS_RES_IOPORT:
		rm = &sc->sc_mem_rman;
		tagval = PPC_BUS_SPACE_MEM;
		if (flags & PPC_BUS_SPARSE4)
			tagval |= 4;
		break;
	case SYS_RES_IRQ:
		return (bus_alloc_resource(bus, type, rid, start, end, count,
					   flags));
		break;
	default:
		device_printf(bus, "unknown resource request from %s\n",
			      device_get_nameunit(child));
		return (NULL);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == NULL) {
		device_printf(bus, "failed to reserve resource for %s\n",
			      device_get_nameunit(child));
		return (NULL);
	}

	rman_set_bustag(rv, tagval);
	rman_set_bushandle(rv, rman_get_start(rv));

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv) != 0) {
                        device_printf(bus,
				      "failed to activate resource for %s\n",
				      device_get_nameunit(child));
			rman_release_resource(rv);
			return (NULL);
                }
        }

	return (rv);	
}


static int
macio_release_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *res)
{
	if (rman_get_flags(res) & RF_ACTIVE) {
		int error = bus_deactivate_resource(child, type, rid, res);
		if (error)
			return error;
	}

	return (rman_release_resource(res));
}


static int
macio_activate_resource(device_t bus, device_t child, int type, int rid,
			   struct resource *res)
{
	struct macio_softc *sc;
	void    *p;

	sc = device_get_softc(bus);

	if (type == SYS_RES_IRQ)
                return (bus_activate_resource(bus, type, rid, res));

	if ((type == SYS_RES_MEMORY) || (type == SYS_RES_IOPORT)) {
		p = pmap_mapdev((vm_offset_t)rman_get_start(res) + sc->sc_base,
				(vm_size_t)rman_get_size(res));
		if (p == NULL)
			return (ENOMEM);
		rman_set_virtual(res, p);
		rman_set_bushandle(res, (u_long)p);
	}

	return (rman_activate_resource(res));
}


static int
macio_deactivate_resource(device_t bus, device_t child, int type, int rid,
			  struct resource *res)
{
        /*
         * If this is a memory resource, unmap it.
         */
        if ((type == SYS_RES_MEMORY) || (type == SYS_RES_IOPORT)) {
		u_int32_t psize;

		psize = rman_get_size(res);
		pmap_unmapdev((vm_offset_t)rman_get_virtual(res), psize);
	}

	return (rman_deactivate_resource(res));
}


static struct resource_list *
macio_get_resource_list (device_t dev, device_t child)
{
	struct macio_devinfo *dinfo = device_get_ivars(child);
	struct resource_list *rl = &dinfo->mdi_resources;

	if (!rl)
		return (NULL);

	return (rl);
}
