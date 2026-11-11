/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The MidnightBSD Project
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _EFIRISCVBOOT_H
#define _EFIRISCVBOOT_H

/*
 * RISCV_EFI_BOOT_PROTOCOL, from the RISC-V UEFI Protocol Specification.
 * Firmware such as EDK2 reports the boot hart through this protocol
 * instead of the /chosen/boot-hartid device tree property.
 */
#define RISCV_EFI_BOOT_PROTOCOL_GUID \
    { 0xccd15fec, 0x6f73, 0x4eec, {0x83, 0x95, 0x3e, 0x69, 0xe4, 0xb9, 0x40, 0xbf} }

INTERFACE_DECL(_RISCV_EFI_BOOT_PROTOCOL);

typedef
EFI_STATUS
(EFIAPI *EFI_GET_BOOT_HARTID) (
    IN struct _RISCV_EFI_BOOT_PROTOCOL	*This,
    OUT UINTN				*BootHartId
    );

typedef struct _RISCV_EFI_BOOT_PROTOCOL {
	UINT64			Revision;
	EFI_GET_BOOT_HARTID	GetBootHartId;
} RISCV_EFI_BOOT_PROTOCOL;

#endif /* _EFIRISCVBOOT_H */
