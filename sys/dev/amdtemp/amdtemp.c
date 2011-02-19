/*-
 * Copyright (c) 2008, 2009 Rui Paulo <rpaulo@FreeBSD.org>
 * Copyright (c) 2009 Norikatsu Shigemura <nork@FreeBSD.org>
 * Copyright (c) 2011 Lucas Holt <luke@foolishgames.com>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for the AMD K8/K10/K11 thermal sensors. Initially based on the
 * k8temp Linux driver.
 */

#include <sys/cdefs.h>
__MBSDID("$MidnightBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/sensors.h>

#include <machine/specialreg.h>
#include <machine/cpufunc.h>
#include <machine/md_var.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

extern int smp_cpus;

typedef enum {
	SENSOR0_CORE0,
	SENSOR0_CORE1,
	SENSOR1_CORE0,
	SENSOR1_CORE1,
	CORE0,
	CORE1
} amdsensor_t;

struct amdtemp_softc {
	struct ksensordev	sc_sensordev;
	struct ksensor		sc_sensors[4];
	device_t		sc_dev;
	int		sc_temps[4];
	int		sc_ntemps;
	int32_t (*sc_gettemp)(device_t, amdsensor_t);
};

#define VENDORID_AMD		0x1022
#define DEVICEID_AMD_MISC0F	0x1103
#define DEVICEID_AMD_MISC10	0x1203
#define DEVICEID_AMD_MISC11	0x1303

static struct amdtemp_product {
	uint16_t	amdtemp_vendorid;
	uint16_t	amdtemp_deviceid;
} amdtemp_products[] = {
	{ VENDORID_AMD,	DEVICEID_AMD_MISC0F },
	{ VENDORID_AMD,	DEVICEID_AMD_MISC10 },
	{ VENDORID_AMD,	DEVICEID_AMD_MISC11 },
	{ 0, 0 }
};

/*
 * Register control (K8 family)
 */
#define	AMDTEMP_REG0F		0xe4
#define	AMDTEMP_REG_SELSENSOR	0x40
#define	AMDTEMP_REG_SELCORE	0x04

/*
 * Register control (K10 & K11) family
 */
#define	AMDTEMP_REG		0xa4

#define	TZ_ZEROC		27315

				/* -49 C is the mininum temperature */
#define	AMDTEMP_OFFSET0F	(TZ_ZEROC-4900)
#define	AMDTEMP_OFFSET		(TZ_ZEROC)

/*
 * Device methods.
 */
static void 	amdtemp_identify(driver_t *driver, device_t parent);
static int	amdtemp_probe(device_t dev);
static int	amdtemp_attach(device_t dev);
static int	amdtemp_detach(device_t dev);
static int 	amdtemp_match(device_t dev);
static int32_t	amdtemp_gettemp0f(device_t dev, amdsensor_t sensor);
static int32_t	amdtemp_gettemp(device_t dev, amdsensor_t sensor);
static void	amdtemp_refresh(void *arg);

static device_method_t amdtemp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	amdtemp_identify),
	DEVMETHOD(device_probe,		amdtemp_probe),
	DEVMETHOD(device_attach,	amdtemp_attach),
	DEVMETHOD(device_detach,	amdtemp_detach),

	{0, 0}
};

static driver_t amdtemp_driver = {
	"amdtemp",
	amdtemp_methods,
	sizeof(struct amdtemp_softc),
};

static devclass_t amdtemp_devclass;
DRIVER_MODULE(amdtemp, hostb, amdtemp_driver, amdtemp_devclass, NULL, NULL);

static int
amdtemp_match(device_t dev)
{
	int i;
	uint16_t vendor, devid;
	
        vendor = pci_get_vendor(dev);
	devid = pci_get_device(dev);

	for (i = 0; amdtemp_products[i].amdtemp_vendorid != 0; i++) {
		if (vendor == amdtemp_products[i].amdtemp_vendorid &&
		    devid == amdtemp_products[i].amdtemp_deviceid)
			return (1);
	}

	return (0);
}

static void
amdtemp_identify(driver_t *driver, device_t parent)
{
	device_t child;

	/* Make sure we're not being doubly invoked. */
	if (device_find_child(parent, "amdtemp", -1) != NULL)
		return;

	/* Make sure AMD is the vendor */
	if (strcmp(cpu_vendor, "AuthenticAMD"))
		return;

	if (amdtemp_match(parent)) {
		child = device_add_child(parent, "amdtemp", -1);
		if (child == NULL)
			device_printf(parent, "add amdtemp child failed\n");
	}
    
}

static int
amdtemp_probe(device_t dev)
{
	uint32_t regs[4];
	
	if (resource_disabled("amdtemp", 0))
		return (ENXIO);

	do_cpuid(1, regs);
	switch (regs[0]) {
	case 0xf40:
	case 0xf50:
	case 0xf51:
		return (ENXIO);
	}
	device_set_desc(dev, "AMD K8 Thermal Sensors");
	
	return (BUS_PROBE_GENERIC);
}

static int
amdtemp_attach(device_t dev)
{
	struct amdtemp_softc *sc = device_get_softc(dev);
	device_t pdev;
	int i;

	sc->sc_dev = dev;
	pdev = device_get_parent(dev);

	if (pci_get_device(dev) == DEVICEID_AMD_MISC0F)
		sc->sc_gettemp = amdtemp_gettemp0f;
	else
		sc->sc_gettemp = amdtemp_gettemp;

	sc->sc_ntemps = 2;

	/*
	 * Add hw.sensors.cpuN.temp0 MIB.
	 */
	strlcpy(sc->sc_sensordev.xname, device_get_nameunit(pdev),
	    sizeof(sc->sc_sensordev.xname));

	for (i = 0; i < sc->sc_ntemps; i++) {
		sc->sc_sensors[i].type = SENSOR_TEMP;
		sensor_attach(&sc->sc_sensordev, &sc->sc_sensors[i]);
	}

	if (sensor_task_register(sc, amdtemp_refresh, 5)) {
		device_printf(dev, "unable to register update task\n");
		return (ENXIO);
	}
	sensordev_install(&sc->sc_sensordev);

	return (0);
}

static int
amdtemp_detach(device_t dev)
{
	struct amdtemp_softc *sc = device_get_softc(dev);

	sensordev_deinstall(&sc->sc_sensordev);
	sensor_task_unregister(sc);
	
	return (0);
}

static int32_t
amdtemp_gettemp0f(device_t dev, amdsensor_t sensor)
{
	uint8_t cfg;
	uint32_t temp;
	
	cfg = pci_read_config(dev, AMDTEMP_REG0F, 1);
	switch (sensor) {
	case SENSOR0_CORE0:
		cfg &= ~(AMDTEMP_REG_SELSENSOR | AMDTEMP_REG_SELCORE);
		break;
	case SENSOR0_CORE1:
		cfg &= ~AMDTEMP_REG_SELSENSOR;
		cfg |= AMDTEMP_REG_SELCORE;
		break;
	case SENSOR1_CORE0:
		cfg &= ~AMDTEMP_REG_SELCORE;
		cfg |= AMDTEMP_REG_SELSENSOR;
		break;
	case SENSOR1_CORE1:
		cfg |= (AMDTEMP_REG_SELSENSOR | AMDTEMP_REG_SELCORE);
		break;
	default:
		cfg = 0;
		break;
	}
	pci_write_config(dev, AMDTEMP_REG0F, cfg, 1);
	temp = pci_read_config(dev, AMDTEMP_REG0F, 4);
	temp = ((temp >> 16) & 0xff) * 100 + AMDTEMP_OFFSET0F;
	
	return (temp);
}

static int32_t
amdtemp_gettemp(device_t dev, amdsensor_t sensor)
{
	uint32_t temp;

	temp = pci_read_config(dev, AMDTEMP_REG, 4);
	temp = (temp >> 21) * 100 / 8 + AMDTEMP_OFFSET;
	return (temp);
}

static void
amdtemp_refresh(void *arg)
{
	struct amdtemp_softc *sc = arg;
	device_t dev = sc->sc_dev;
	struct ksensor *s = sc->sc_sensors;
	int32_t temp, auxtemp[2];
	int i;

	for (i = 0; i < sc->sc_ntemps; i++)
	{
		switch (i) {
			case 0:
				auxtemp[0] = sc->sc_gettemp(dev, SENSOR0_CORE0);
				auxtemp[1] = sc->sc_gettemp(dev, SENSOR1_CORE0);
				temp = imax(auxtemp[0], auxtemp[1]);
				break;
			case 1:
				auxtemp[0] = sc->sc_gettemp(dev, SENSOR0_CORE1);
				auxtemp[1] = sc->sc_gettemp(dev, SENSOR1_CORE1);
				temp = imax(auxtemp[0], auxtemp[1]);
				break;
			default:
				temp = sc->sc_gettemp(dev, CORE0);
				break;
		}
		s[i].flags &= ~SENSOR_FINVALID;
		s[i].value = temp * 10000;
	}
}
