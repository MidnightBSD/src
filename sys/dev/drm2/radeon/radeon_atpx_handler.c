/*
 * Copyright (c) 2010 Red Hat Inc.
 * Author : Dave Airlie <airlied@redhat.com>
 *
 * Licensed under GPLv2
 *
 * ATPX support for both Intel/ATI
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/sys/dev/drm2/radeon/radeon_atpx_handler.c 254885 2013-08-25 19:37:15Z dumbbell $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/linker.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include <dev/drm2/drmP.h>
#include <dev/drm2/radeon/radeon_drm.h>
#include "radeon_acpi.h"
#include "radeon_drv.h"

#ifdef DUMBBELL_WIP
struct radeon_atpx_functions {
	bool px_params;
	bool power_cntl;
	bool disp_mux_cntl;
	bool i2c_mux_cntl;
	bool switch_start;
	bool switch_end;
	bool disp_connectors_mapping;
	bool disp_detetion_ports;
};

struct radeon_atpx {
	ACPI_HANDLE handle;
	struct radeon_atpx_functions functions;
};

static struct radeon_atpx_priv {
	bool atpx_detected;
	/* handle for device - and atpx */
	ACPI_HANDLE dhandle;
	struct radeon_atpx atpx;
} radeon_atpx_priv;

struct atpx_verify_interface {
	u16 size;		/* structure size in bytes (includes size field) */
	u16 version;		/* version */
	u32 function_bits;	/* supported functions bit vector */
} __packed;

struct atpx_power_control {
	u16 size;
	u8 dgpu_state;
} __packed;

struct atpx_mux {
	u16 size;
	u16 mux;
} __packed;

/**
 * radeon_atpx_call - call an ATPX method
 *
 * @handle: acpi handle
 * @function: the ATPX function to execute
 * @params: ATPX function params
 *
 * Executes the requested ATPX function (all asics).
 * Returns a pointer to the acpi output buffer.
 */
static ACPI_OBJECT *radeon_atpx_call(ACPI_HANDLE handle, int function,
					   ACPI_BUFFER *params)
{
	ACPI_STATUS status;
	ACPI_OBJECT atpx_arg_elements[2];
	ACPI_OBJECT_LIST atpx_arg;
	ACPI_BUFFER buffer = { ACPI_ALLOCATE_BUFFER, NULL };

	atpx_arg.Count = 2;
	atpx_arg.Pointer = &atpx_arg_elements[0];

	atpx_arg_elements[0].Type = ACPI_TYPE_INTEGER;
	atpx_arg_elements[0].Integer.Value = function;

	if (params) {
		atpx_arg_elements[1].Type = ACPI_TYPE_BUFFER;
		atpx_arg_elements[1].Buffer.Length = params->Length;
		atpx_arg_elements[1].Buffer.Pointer = params->Pointer;
	} else {
		/* We need a second fake parameter */
		atpx_arg_elements[1].Type = ACPI_TYPE_INTEGER;
		atpx_arg_elements[1].Integer.Value = 0;
	}

	status = AcpiEvaluateObject(handle, NULL, &atpx_arg, &buffer);

	/* Fail only if calling the method fails and ATPX is supported */
	if (ACPI_FAILURE(status) && status != AE_NOT_FOUND) {
		DRM_ERROR("failed to evaluate ATPX got %s\n",
		       AcpiFormatException(status));
		AcpiOsFree(buffer.Pointer);
		return NULL;
	}

	return buffer.Pointer;
}

/**
 * radeon_atpx_parse_functions - parse supported functions
 *
 * @f: supported functions struct
 * @mask: supported functions mask from ATPX
 *
 * Use the supported functions mask from ATPX function
 * ATPX_FUNCTION_VERIFY_INTERFACE to determine what functions
 * are supported (all asics).
 */
static void radeon_atpx_parse_functions(struct radeon_atpx_functions *f, u32 mask)
{
	f->px_params = mask & ATPX_GET_PX_PARAMETERS_SUPPORTED;
	f->power_cntl = mask & ATPX_POWER_CONTROL_SUPPORTED;
	f->disp_mux_cntl = mask & ATPX_DISPLAY_MUX_CONTROL_SUPPORTED;
	f->i2c_mux_cntl = mask & ATPX_I2C_MUX_CONTROL_SUPPORTED;
	f->switch_start = mask & ATPX_GRAPHICS_DEVICE_SWITCH_START_NOTIFICATION_SUPPORTED;
	f->switch_end = mask & ATPX_GRAPHICS_DEVICE_SWITCH_END_NOTIFICATION_SUPPORTED;
	f->disp_connectors_mapping = mask & ATPX_GET_DISPLAY_CONNECTORS_MAPPING_SUPPORTED;
	f->disp_detetion_ports = mask & ATPX_GET_DISPLAY_DETECTION_PORTS_SUPPORTED;
}

/**
 * radeon_atpx_verify_interface - verify ATPX
 *
 * @handle: acpi handle
 * @atpx: radeon atpx struct
 *
 * Execute the ATPX_FUNCTION_VERIFY_INTERFACE ATPX function
 * to initialize ATPX and determine what features are supported
 * (all asics).
 * returns 0 on success, error on failure.
 */
static int radeon_atpx_verify_interface(struct radeon_atpx *atpx)
{
	ACPI_OBJECT *info;
	struct atpx_verify_interface output;
	size_t size;
	int err = 0;

	info = radeon_atpx_call(atpx->handle, ATPX_FUNCTION_VERIFY_INTERFACE, NULL);
	if (!info)
		return -EIO;

	memset(&output, 0, sizeof(output));

	size = *(u16 *) info->Buffer.Pointer;
	if (size < 8) {
		DRM_ERROR("ATPX buffer is too small: %zu\n", size);
		err = -EINVAL;
		goto out;
	}
	size = min(sizeof(output), size);

	memcpy(&output, info->Buffer.Pointer, size);

	/* TODO: check version? */
	DRM_INFO("ATPX version %u\n", output.version);

	radeon_atpx_parse_functions(&atpx->functions, output.function_bits);

out:
	AcpiOsFree(info);
	return err;
}

/**
 * radeon_atpx_set_discrete_state - power up/down discrete GPU
 *
 * @atpx: atpx info struct
 * @state: discrete GPU state (0 = power down, 1 = power up)
 *
 * Execute the ATPX_FUNCTION_POWER_CONTROL ATPX function to
 * power down/up the discrete GPU (all asics).
 * Returns 0 on success, error on failure.
 */
static int radeon_atpx_set_discrete_state(struct radeon_atpx *atpx, u8 state)
{
	ACPI_BUFFER params;
	ACPI_OBJECT *info;
	struct atpx_power_control input;

	if (atpx->functions.power_cntl) {
		input.size = 3;
		input.dgpu_state = state;
		params.Length = input.size;
		params.Pointer = &input;
		info = radeon_atpx_call(atpx->handle,
					ATPX_FUNCTION_POWER_CONTROL,
					&params);
		if (!info)
			return -EIO;
		AcpiOsFree(info);
	}
	return 0;
}

/**
 * radeon_atpx_switch_disp_mux - switch display mux
 *
 * @atpx: atpx info struct
 * @mux_id: mux state (0 = integrated GPU, 1 = discrete GPU)
 *
 * Execute the ATPX_FUNCTION_DISPLAY_MUX_CONTROL ATPX function to
 * switch the display mux between the discrete GPU and integrated GPU
 * (all asics).
 * Returns 0 on success, error on failure.
 */
static int radeon_atpx_switch_disp_mux(struct radeon_atpx *atpx, u16 mux_id)
{
	ACPI_BUFFER params;
	ACPI_OBJECT *info;
	struct atpx_mux input;

	if (atpx->functions.disp_mux_cntl) {
		input.size = 4;
		input.mux = mux_id;
		params.Length = input.size;
		params.Pointer = &input;
		info = radeon_atpx_call(atpx->handle,
					ATPX_FUNCTION_DISPLAY_MUX_CONTROL,
					&params);
		if (!info)
			return -EIO;
		AcpiOsFree(info);
	}
	return 0;
}

/**
 * radeon_atpx_switch_i2c_mux - switch i2c/hpd mux
 *
 * @atpx: atpx info struct
 * @mux_id: mux state (0 = integrated GPU, 1 = discrete GPU)
 *
 * Execute the ATPX_FUNCTION_I2C_MUX_CONTROL ATPX function to
 * switch the i2c/hpd mux between the discrete GPU and integrated GPU
 * (all asics).
 * Returns 0 on success, error on failure.
 */
static int radeon_atpx_switch_i2c_mux(struct radeon_atpx *atpx, u16 mux_id)
{
	ACPI_BUFFER params;
	ACPI_OBJECT *info;
	struct atpx_mux input;

	if (atpx->functions.i2c_mux_cntl) {
		input.size = 4;
		input.mux = mux_id;
		params.Length = input.size;
		params.Pointer = &input;
		info = radeon_atpx_call(atpx->handle,
					ATPX_FUNCTION_I2C_MUX_CONTROL,
					&params);
		if (!info)
			return -EIO;
		AcpiOsFree(info);
	}
	return 0;
}

/**
 * radeon_atpx_switch_start - notify the sbios of a GPU switch
 *
 * @atpx: atpx info struct
 * @mux_id: mux state (0 = integrated GPU, 1 = discrete GPU)
 *
 * Execute the ATPX_FUNCTION_GRAPHICS_DEVICE_SWITCH_START_NOTIFICATION ATPX
 * function to notify the sbios that a switch between the discrete GPU and
 * integrated GPU has begun (all asics).
 * Returns 0 on success, error on failure.
 */
static int radeon_atpx_switch_start(struct radeon_atpx *atpx, u16 mux_id)
{
	ACPI_BUFFER params;
	ACPI_OBJECT *info;
	struct atpx_mux input;

	if (atpx->functions.switch_start) {
		input.size = 4;
		input.mux = mux_id;
		params.Length = input.size;
		params.Pointer = &input;
		info = radeon_atpx_call(atpx->handle,
					ATPX_FUNCTION_GRAPHICS_DEVICE_SWITCH_START_NOTIFICATION,
					&params);
		if (!info)
			return -EIO;
		AcpiOsFree(info);
	}
	return 0;
}

/**
 * radeon_atpx_switch_end - notify the sbios of a GPU switch
 *
 * @atpx: atpx info struct
 * @mux_id: mux state (0 = integrated GPU, 1 = discrete GPU)
 *
 * Execute the ATPX_FUNCTION_GRAPHICS_DEVICE_SWITCH_END_NOTIFICATION ATPX
 * function to notify the sbios that a switch between the discrete GPU and
 * integrated GPU has ended (all asics).
 * Returns 0 on success, error on failure.
 */
static int radeon_atpx_switch_end(struct radeon_atpx *atpx, u16 mux_id)
{
	ACPI_BUFFER params;
	ACPI_OBJECT *info;
	struct atpx_mux input;

	if (atpx->functions.switch_end) {
		input.size = 4;
		input.mux = mux_id;
		params.Length = input.size;
		params.Pointer = &input;
		info = radeon_atpx_call(atpx->handle,
					ATPX_FUNCTION_GRAPHICS_DEVICE_SWITCH_END_NOTIFICATION,
					&params);
		if (!info)
			return -EIO;
		AcpiOsFree(info);
	}
	return 0;
}

/**
 * radeon_atpx_switchto - switch to the requested GPU
 *
 * @id: GPU to switch to
 *
 * Execute the necessary ATPX functions to switch between the discrete GPU and
 * integrated GPU (all asics).
 * Returns 0 on success, error on failure.
 */
static int radeon_atpx_switchto(enum vga_switcheroo_client_id id)
{
	u16 gpu_id;

	if (id == VGA_SWITCHEROO_IGD)
		gpu_id = ATPX_INTEGRATED_GPU;
	else
		gpu_id = ATPX_DISCRETE_GPU;

	radeon_atpx_switch_start(&radeon_atpx_priv.atpx, gpu_id);
	radeon_atpx_switch_disp_mux(&radeon_atpx_priv.atpx, gpu_id);
	radeon_atpx_switch_i2c_mux(&radeon_atpx_priv.atpx, gpu_id);
	radeon_atpx_switch_end(&radeon_atpx_priv.atpx, gpu_id);

	return 0;
}

/**
 * radeon_atpx_power_state - power down/up the requested GPU
 *
 * @id: GPU to power down/up
 * @state: requested power state (0 = off, 1 = on)
 *
 * Execute the necessary ATPX function to power down/up the discrete GPU
 * (all asics).
 * Returns 0 on success, error on failure.
 */
static int radeon_atpx_power_state(enum vga_switcheroo_client_id id,
				   enum vga_switcheroo_state state)
{
	/* on w500 ACPI can't change intel gpu state */
	if (id == VGA_SWITCHEROO_IGD)
		return 0;

	radeon_atpx_set_discrete_state(&radeon_atpx_priv.atpx, state);
	return 0;
}

/**
 * radeon_atpx_pci_probe_handle - look up the ATPX handle
 *
 * @pdev: pci device
 *
 * Look up the ATPX handles (all asics).
 * Returns true if the handles are found, false if not.
 */
static bool radeon_atpx_pci_probe_handle(struct pci_dev *pdev)
{
	ACPI_HANDLE dhandle, atpx_handle;
	ACPI_STATUS status;

	dhandle = DEVICE_ACPI_HANDLE(&pdev->dev);
	if (!dhandle)
		return false;

	status = AcpiGetHandle(dhandle, "ATPX", &atpx_handle);
	if (ACPI_FAILURE(status))
		return false;

	radeon_atpx_priv.dhandle = dhandle;
	radeon_atpx_priv.atpx.handle = atpx_handle;
	return true;
}

/**
 * radeon_atpx_init - verify the ATPX interface
 *
 * Verify the ATPX interface (all asics).
 * Returns 0 on success, error on failure.
 */
static int radeon_atpx_init(void)
{
	/* set up the ATPX handle */
	return radeon_atpx_verify_interface(&radeon_atpx_priv.atpx);
}

/**
 * radeon_atpx_get_client_id - get the client id
 *
 * @pdev: pci device
 *
 * look up whether we are the integrated or discrete GPU (all asics).
 * Returns the client id.
 */
static int radeon_atpx_get_client_id(struct pci_dev *pdev)
{
	if (radeon_atpx_priv.dhandle == DEVICE_ACPI_HANDLE(&pdev->dev))
		return VGA_SWITCHEROO_IGD;
	else
		return VGA_SWITCHEROO_DIS;
}

static struct vga_switcheroo_handler radeon_atpx_handler = {
	.switchto = radeon_atpx_switchto,
	.power_state = radeon_atpx_power_state,
	.init = radeon_atpx_init,
	.get_client_id = radeon_atpx_get_client_id,
};

/**
 * radeon_atpx_detect - detect whether we have PX
 *
 * Check if we have a PX system (all asics).
 * Returns true if we have a PX system, false if not.
 */
static bool radeon_atpx_detect(void)
{
	char acpi_method_name[255] = { 0 };
	ACPI_BUFFER buffer = {sizeof(acpi_method_name), acpi_method_name};
	struct pci_dev *pdev = NULL;
	bool has_atpx = false;
	int vga_count = 0;

	while ((pdev = pci_get_class(PCI_CLASS_DISPLAY_VGA << 8, pdev)) != NULL) {
		vga_count++;

		has_atpx |= (radeon_atpx_pci_probe_handle(pdev) == true);
	}

	if (has_atpx && vga_count == 2) {
		AcpiGetName(radeon_atpx_priv.atpx.handle, ACPI_FULL_PATHNAME, &buffer);
		DRM_INFO("VGA switcheroo: detected switching method %s handle\n",
		       acpi_method_name);
		radeon_atpx_priv.atpx_detected = true;
		return true;
	}
	return false;
}
#endif /* DUMBBELL_WIP */

/**
 * radeon_register_atpx_handler - register with vga_switcheroo
 *
 * Register the PX callbacks with vga_switcheroo (all asics).
 */
void radeon_register_atpx_handler(void)
{
#ifdef DUMBBELL_WIP
	bool r;

	/* detect if we have any ATPX + 2 VGA in the system */
	r = radeon_atpx_detect();
	if (!r)
		return;

	vga_switcheroo_register_handler(&radeon_atpx_handler);
#endif /* DUMBBELL_WIP */
}

/**
 * radeon_unregister_atpx_handler - unregister with vga_switcheroo
 *
 * Unregister the PX callbacks with vga_switcheroo (all asics).
 */
void radeon_unregister_atpx_handler(void)
{
#ifdef DUMBBELL_WIP
	vga_switcheroo_unregister_handler();
#endif /* DUMBBELL_WIP */
}
