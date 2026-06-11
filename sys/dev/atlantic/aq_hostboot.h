/*
 * Copyright (c) 2026 Albert Song <albb0920@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef AQ_HOSTBOOT_H
#define AQ_HOSTBOOT_H

struct aq_dev;
struct aq_hw;
struct firmware;
struct sysctl_ctx_list;
struct sysctl_oid_list;

#define AQ_HOSTBOOT_IMAGE_REQUIRED 4096

void aq_hostboot_init(struct aq_dev *aq_dev);
void aq_hostboot_refresh_status(struct aq_hw *hw);
void aq_hostboot_add_sysctls(struct aq_dev *aq_dev, struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child);
int aq_hostboot_module_load(void);
int aq_hostboot_module_unload(void);

bool aq_hostboot_force(const struct aq_hw *hw);
bool aq_hostboot_provisioning_override(const struct aq_hw *hw);
uint32_t aq_hostboot_provisioning_selector(const struct aq_hw *hw);

int aq_hostboot_request_fw(struct aq_hw *hw);
void aq_hostboot_release_fw(struct aq_hw *hw);

int aq_hostboot_legacy(struct aq_hw *hw);
int aq_hostboot_aq2(struct aq_hw *hw);

#endif /* AQ_HOSTBOOT_H */
