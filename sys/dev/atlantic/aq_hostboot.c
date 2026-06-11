/*
 * Copyright (c) 2026 Albert Song <albb0920@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitstring.h>
#include <sys/bus.h>
#include <sys/firmware.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_var.h>
#include <net/iflib.h>

#include "aq_common.h"
#include "aq_device.h"
#include "aq_hostboot.h"
#include "aq_hw.h"

#define AQ_HOSTBOOT_FW_AQC100X "if_atlantic_fw_80B1"
#define AQ_HOSTBOOT_FW_AQC10XX "if_atlantic_fw_87B1"
#define AQ_HOSTBOOT_FW_AQC11XX "if_atlantic_fw_91B1"
#define AQ_HOSTBOOT_FW_AQ2 "if_atlantic_fw_aq2"
#define AQ_HOSTBOOT_HINT_NAME "aq"

struct aq_hostboot_fw_entry {
	SLIST_ENTRY(aq_hostboot_fw_entry) link;
	const struct firmware *fw;
	char resolved_from_name[AQ_HOSTBOOT_NAME_LEN];
};
SLIST_HEAD(aq_hostboot_fw_head, aq_hostboot_fw_entry);

static const char *const aq_hostboot_builtin_images[] = {
	AQ_HOSTBOOT_FW_AQC100X,
	AQ_HOSTBOOT_FW_AQC10XX,
	AQ_HOSTBOOT_FW_AQC11XX,
	AQ_HOSTBOOT_FW_AQ2,
};

// All fw images we preloaded on module load
static struct aq_hostboot_fw_head aq_hostboot_fw_list = SLIST_HEAD_INITIALIZER(
    aq_hostboot_fw_list);

static bool aq_hostboot_force_tunable;
TUNABLE_BOOL("hw.aq.hostboot_force", &aq_hostboot_force_tunable);

static char aq_hostboot_fw_image_tunable[AQ_HOSTBOOT_NAME_LEN];
TUNABLE_STR("hw.aq.hostboot_fw_image", aq_hostboot_fw_image_tunable,
    sizeof(aq_hostboot_fw_image_tunable));

static char aq_hostboot_provisioning_selector_tunable[AQ_HOSTBOOT_NAME_LEN];
TUNABLE_STR("hw.aq.hostboot_provisioning_selector",
    aq_hostboot_provisioning_selector_tunable,
    sizeof(aq_hostboot_provisioning_selector_tunable));

static int
aq_hostboot_parse_u32(const char *val, uint32_t *out)
{
	char *endp;
	unsigned long v;

	if (val == NULL || val[0] == '\0')
		return (EINVAL);

	v = strtoul(val, &endp, 16);
	if (endp == val || *endp != '\0')
		return (EINVAL);
	if (v > UINT32_MAX)
		return (ERANGE);

	*out = (uint32_t)v;
	return (0);
}

static const struct firmware *
aq_hostboot_preloaded_fw_get(const char *image_name)
{
	struct aq_hostboot_fw_entry *entry;

	SLIST_FOREACH (entry, &aq_hostboot_fw_list, link) {
		if (strcmp(entry->resolved_from_name, image_name) == 0) {
			/* cppcheck-suppress uninitvar; SLIST entries are
			 * initialized before insertion. */
			return (entry->fw);
		}
	}

	return (NULL);
}

static int
aq_hostboot_preload_fw(const char *image_name)
{
	struct aq_hostboot_fw_entry *entry;
	const struct firmware *fw;

	if (image_name == NULL || image_name[0] == '\0')
		return (0);
	if (aq_hostboot_preloaded_fw_get(image_name) != NULL)
		return (0);

	fw = firmware_get_flags(image_name, FIRMWARE_GET_NOWARN);
	if (fw == NULL)
		return (0);

	entry = malloc(sizeof(*entry), M_DEVBUF, M_WAITOK | M_ZERO);
	entry->fw = fw;
	strlcpy(entry->resolved_from_name, image_name,
	    sizeof(entry->resolved_from_name));

	SLIST_INSERT_HEAD(&aq_hostboot_fw_list, entry, link);

	return (0);
}

static void
aq_hostboot_resolution_set(struct aq_dev *aq_dev, const char *image_name)
{
	strlcpy(aq_dev->hostboot_fw_image, image_name != NULL ? image_name : "",
	    sizeof(aq_dev->hostboot_fw_image));
}

static const char *
aq_hostboot_resolve_builtin(struct aq_hw *hw)
{
	if (AQ_HW_IS_AQ2(hw))
		return (AQ_HOSTBOOT_FW_AQ2);

	switch (hw->chip_id) {
	case AQ_CHIP_AQC107X:
	case AQ_CHIP_AQC108X:
	case AQ_CHIP_AQC109X:
		return (AQ_HOSTBOOT_FW_AQC10XX);
	case AQ_CHIP_AQCC111X:
	case AQ_CHIP_AQCC112X:
	case AQ_CHIP_AQC111EX:
	case AQ_CHIP_AQC112EX:
		return (AQ_HOSTBOOT_FW_AQC11XX);
	case AQ_CHIP_AQC100X:
		return (AQ_HOSTBOOT_FW_AQC100X);
	default:
		return (NULL);
	}
}

static int
aq_hostboot_resolve_name(struct aq_hw *hw, char *buf, size_t len)
{
	struct aq_dev *aq_dev = hw->aq_dev;
	const char *image_name = NULL;

	if (aq_dev->hostboot_config.fw_image[0] != '\0') {
		image_name = aq_dev->hostboot_config.fw_image;
	} else {
		image_name = aq_hostboot_resolve_builtin(hw);
	}

	if (image_name == NULL) {
		aq_hostboot_resolution_set(aq_dev, "");
		return (ENOENT);
	}

	strlcpy(buf, image_name, len);
	aq_hostboot_resolution_set(aq_dev, image_name);

	return (0);
}

static void
aq_hostboot_config_parse(struct aq_dev *aq_dev)
{
	device_t dev;
	const char *val;
	int int_val;
	uint32_t u32;

	dev = aq_dev->dev;
	memset(&aq_dev->hostboot_config, 0, sizeof(aq_dev->hostboot_config));
	memset(aq_dev->hostboot_fw_image, 0, sizeof(aq_dev->hostboot_fw_image));

	aq_dev->hostboot_config.force = aq_hostboot_force_tunable;
	strlcpy(aq_dev->hostboot_config.fw_image, aq_hostboot_fw_image_tunable,
	    sizeof(aq_dev->hostboot_config.fw_image));

	if (aq_hostboot_provisioning_selector_tunable[0] != '\0' &&
	    aq_hostboot_parse_u32(aq_hostboot_provisioning_selector_tunable,
		&u32) == 0) {
		aq_dev->hostboot_config.provisioning_override = true;
		aq_dev->hostboot_config.provisioning_selector = u32;
	}

	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
		"hostboot_force", &int_val) == 0)
		aq_dev->hostboot_config.force = int_val != 0;
	else if (resource_int_value(AQ_HOSTBOOT_HINT_NAME, device_get_unit(dev),
		     "hostboot_force", &int_val) == 0)
		aq_dev->hostboot_config.force = int_val != 0;

	if (resource_string_value(device_get_name(dev), device_get_unit(dev),
		"hostboot_fw_image", &val) == 0)
		strlcpy(aq_dev->hostboot_config.fw_image, val,
		    sizeof(aq_dev->hostboot_config.fw_image));
	else if (resource_string_value(AQ_HOSTBOOT_HINT_NAME,
		     device_get_unit(dev), "hostboot_fw_image", &val) == 0)
		strlcpy(aq_dev->hostboot_config.fw_image, val,
		    sizeof(aq_dev->hostboot_config.fw_image));

	if (resource_string_value(device_get_name(dev), device_get_unit(dev),
		"hostboot_provisioning_selector", &val) == 0 &&
	    aq_hostboot_parse_u32(val, &u32) == 0) {
		aq_dev->hostboot_config.provisioning_override = true;
		aq_dev->hostboot_config.provisioning_selector = u32;
	} else if (resource_string_value(AQ_HOSTBOOT_HINT_NAME,
		       device_get_unit(dev), "hostboot_provisioning_selector",
		       &val) == 0 &&
	    aq_hostboot_parse_u32(val, &u32) == 0) {
		aq_dev->hostboot_config.provisioning_override = true;
		aq_dev->hostboot_config.provisioning_selector = u32;
	}
}

void
aq_hostboot_init(struct aq_dev *aq_dev)
{
	aq_hostboot_config_parse(aq_dev);
	aq_hostboot_refresh_status(&aq_dev->hw);
}

int
aq_hostboot_module_load(void)
{
	const char *image_name;
	size_t i;
	int anchor, unit;
	int err;

#if __FreeBSD_version >= 1400000 && __FreeBSD_version < 1501000
	printf("if_atlantic: 'could not load binary firmware' warnings "
	       "below are harmless (kernel bug, see D54955)\n");
#endif

	for (i = 0; i < nitems(aq_hostboot_builtin_images); i++) {
		err = aq_hostboot_preload_fw(aq_hostboot_builtin_images[i]);
		if (err != 0)
			return (err);
	}

	err = aq_hostboot_preload_fw(aq_hostboot_fw_image_tunable);
	if (err != 0)
		return (err);

	anchor = 0;
	while (resource_find_dev(&anchor, AQ_HOSTBOOT_HINT_NAME, &unit,
		   "hostboot_fw_image", NULL) == 0) {
		if (resource_string_value(AQ_HOSTBOOT_HINT_NAME, unit,
			"hostboot_fw_image", &image_name) != 0)
			continue;

		err = aq_hostboot_preload_fw(image_name);
		if (err != 0)
			return (err);
	}

	return (0);
}

int
aq_hostboot_module_unload(void)
{
	struct aq_hostboot_fw_entry *entry;

	for (;;) {
		entry = SLIST_FIRST(&aq_hostboot_fw_list);
		if (entry != NULL)
			SLIST_REMOVE_HEAD(&aq_hostboot_fw_list, link);
		if (entry == NULL)
			break;

		firmware_put(entry->fw, FIRMWARE_UNLOAD);
		free(entry, M_DEVBUF);
	}

	return (0);
}

void
aq_hostboot_refresh_status(struct aq_hw *hw)
{
	char image_name[AQ_HOSTBOOT_NAME_LEN];

	if (aq_hostboot_resolve_name(hw, image_name, sizeof(image_name)) != 0)
		aq_hostboot_resolution_set(hw->aq_dev, "");
}

void
aq_hostboot_add_sysctls(struct aq_dev *aq_dev, struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child)
{
	SYSCTL_ADD_BOOL(ctx, child, OID_AUTO, "hostboot_force", CTLFLAG_RD,
	    &aq_dev->hostboot_config.force, 0,
	    "Force host boot when firmware supports it");
	SYSCTL_ADD_STRING(ctx, child, OID_AUTO, "hostboot_fw_image", CTLFLAG_RD,
	    aq_dev->hostboot_fw_image, sizeof(aq_dev->hostboot_fw_image),
	    "Effective host boot firmware name");
}

bool
aq_hostboot_force(const struct aq_hw *hw)
{
	const struct aq_dev *aq_dev = hw->aq_dev;

	return (aq_dev->hostboot_config.force);
}

bool
aq_hostboot_provisioning_override(const struct aq_hw *hw)
{
	const struct aq_dev *aq_dev = hw->aq_dev;

	return (aq_dev->hostboot_config.provisioning_override);
}

uint32_t
aq_hostboot_provisioning_selector(const struct aq_hw *hw)
{
	const struct aq_dev *aq_dev = hw->aq_dev;

	if (aq_dev->hostboot_config.provisioning_override)
		return (aq_dev->hostboot_config.provisioning_selector);

	return (((uint32_t)hw->subsystem_device_id << 16) |
	    hw->subsystem_vendor_id);
}

int
aq_hostboot_request_fw(struct aq_hw *hw)
{
	struct aq_dev *aq_dev = hw->aq_dev;
	char image_name[AQ_HOSTBOOT_NAME_LEN];
	int err;

	if (hw->hostboot_fw != NULL)
		return (0);

	err = aq_hostboot_resolve_name(hw, image_name, sizeof(image_name));
	if (err != 0) {
		device_printf(aq_dev->dev,
		    "host boot requested but no firmware image was resolved\n");
		return (ENOENT);
	}

	hw->hostboot_fw = aq_hostboot_preloaded_fw_get(image_name);
	if (hw->hostboot_fw == NULL) {
		device_printf(aq_dev->dev,
		    "host boot firmware image '%s' was not preloaded at module load\n",
		    image_name);
		return (ENOENT);
	}

	return (0);
}

void
aq_hostboot_release_fw(struct aq_hw *hw)
{
	hw->hostboot_fw = NULL;
}
