/*-
 * Copyright (c) 2013 Adrian Chadd <adrian@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/10.0.0/sys/mips/atheros/ar934x_chip.c 253511 2013-07-21 03:56:57Z adrian $");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/kdb.h>
#include <sys/reboot.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#include <net/ethernet.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/cpuregs.h>
#include <machine/hwfunc.h>
#include <machine/md_var.h>
#include <machine/trap.h>
#include <machine/vmparam.h>

#include <mips/atheros/ar71xxreg.h>
#include <mips/atheros/ar934xreg.h>

#include <mips/atheros/ar71xx_cpudef.h>
#include <mips/atheros/ar71xx_setup.h>

#include <mips/atheros/ar71xx_chip.h>
#include <mips/atheros/ar934x_chip.h>

static void
ar934x_chip_detect_mem_size(void)
{
}

static uint32_t
ar934x_get_pll_freq(uint32_t ref, uint32_t ref_div, uint32_t nint,
    uint32_t nfrac, uint32_t frac, uint32_t out_div)
{
	uint64_t t;
	uint32_t ret;

	t = u_ar71xx_refclk;
	t *= nint;
	t = t / ref_div;
	ret = t;

	t = u_ar71xx_refclk;
	t *= nfrac;
	t = t / (ref_div * frac);
	ret += t;

	ret /= (1 << out_div);
	return (ret);
}

static void
ar934x_chip_detect_sys_frequency(void)
{
	uint32_t pll, out_div, ref_div, nint, nfrac, frac, clk_ctrl, postdiv;
	uint32_t cpu_pll, ddr_pll;
	uint32_t bootstrap;

	bootstrap = ATH_READ_REG(AR934X_RESET_REG_BOOTSTRAP);
	if (bootstrap & AR934X_BOOTSTRAP_REF_CLK_40)
		u_ar71xx_refclk = 40 * 1000 * 1000;
	else
		u_ar71xx_refclk = 25 * 1000 * 1000;

	pll = ATH_READ_REG(AR934X_SRIF_CPU_DPLL2_REG);
	if (pll & AR934X_SRIF_DPLL2_LOCAL_PLL) {
		out_div = (pll >> AR934X_SRIF_DPLL2_OUTDIV_SHIFT) &
		    AR934X_SRIF_DPLL2_OUTDIV_MASK;
		pll = ATH_READ_REG(AR934X_SRIF_CPU_DPLL1_REG);
		nint = (pll >> AR934X_SRIF_DPLL1_NINT_SHIFT) &
		    AR934X_SRIF_DPLL1_NINT_MASK;
		nfrac = pll & AR934X_SRIF_DPLL1_NFRAC_MASK;
		ref_div = (pll >> AR934X_SRIF_DPLL1_REFDIV_SHIFT) &
		    AR934X_SRIF_DPLL1_REFDIV_MASK;
		frac = 1 << 18;
	} else {
		pll = ATH_READ_REG(AR934X_PLL_CPU_CONFIG_REG);
		out_div = (pll >> AR934X_PLL_CPU_CONFIG_OUTDIV_SHIFT) &
			AR934X_PLL_CPU_CONFIG_OUTDIV_MASK;
		ref_div = (pll >> AR934X_PLL_CPU_CONFIG_REFDIV_SHIFT) &
			  AR934X_PLL_CPU_CONFIG_REFDIV_MASK;
		nint = (pll >> AR934X_PLL_CPU_CONFIG_NINT_SHIFT) &
		       AR934X_PLL_CPU_CONFIG_NINT_MASK;
		nfrac = (pll >> AR934X_PLL_CPU_CONFIG_NFRAC_SHIFT) &
			AR934X_PLL_CPU_CONFIG_NFRAC_MASK;
		frac = 1 << 6;
	}

	cpu_pll = ar934x_get_pll_freq(u_ar71xx_refclk, ref_div, nint,
	    nfrac, frac, out_div);

	pll = ATH_READ_REG(AR934X_SRIF_DDR_DPLL2_REG);
	if (pll & AR934X_SRIF_DPLL2_LOCAL_PLL) {
		out_div = (pll >> AR934X_SRIF_DPLL2_OUTDIV_SHIFT) &
		    AR934X_SRIF_DPLL2_OUTDIV_MASK;
		pll = ATH_READ_REG(AR934X_SRIF_DDR_DPLL1_REG);
		nint = (pll >> AR934X_SRIF_DPLL1_NINT_SHIFT) &
		    AR934X_SRIF_DPLL1_NINT_MASK;
		nfrac = pll & AR934X_SRIF_DPLL1_NFRAC_MASK;
		ref_div = (pll >> AR934X_SRIF_DPLL1_REFDIV_SHIFT) &
		    AR934X_SRIF_DPLL1_REFDIV_MASK;
		frac = 1 << 18;
	} else {
		pll = ATH_READ_REG(AR934X_PLL_DDR_CONFIG_REG);
		out_div = (pll >> AR934X_PLL_DDR_CONFIG_OUTDIV_SHIFT) &
		    AR934X_PLL_DDR_CONFIG_OUTDIV_MASK;
		ref_div = (pll >> AR934X_PLL_DDR_CONFIG_REFDIV_SHIFT) &
		    AR934X_PLL_DDR_CONFIG_REFDIV_MASK;
		nint = (pll >> AR934X_PLL_DDR_CONFIG_NINT_SHIFT) &
		    AR934X_PLL_DDR_CONFIG_NINT_MASK;
		nfrac = (pll >> AR934X_PLL_DDR_CONFIG_NFRAC_SHIFT) &
		    AR934X_PLL_DDR_CONFIG_NFRAC_MASK;
		frac = 1 << 10;
	}

	ddr_pll = ar934x_get_pll_freq(u_ar71xx_refclk, ref_div, nint,
	    nfrac, frac, out_div);

	clk_ctrl = ATH_READ_REG(AR934X_PLL_CPU_DDR_CLK_CTRL_REG);

	postdiv = (clk_ctrl >> AR934X_PLL_CPU_DDR_CLK_CTRL_CPU_POST_DIV_SHIFT) &
	    AR934X_PLL_CPU_DDR_CLK_CTRL_CPU_POST_DIV_MASK;

	if (clk_ctrl & AR934X_PLL_CPU_DDR_CLK_CTRL_CPU_PLL_BYPASS)
	    u_ar71xx_cpu_freq = u_ar71xx_refclk;
	else if (clk_ctrl & AR934X_PLL_CPU_DDR_CLK_CTRL_CPUCLK_FROM_CPUPLL)
		u_ar71xx_cpu_freq = cpu_pll / (postdiv + 1);
	else
		u_ar71xx_cpu_freq = ddr_pll / (postdiv + 1);

	postdiv = (clk_ctrl >> AR934X_PLL_CPU_DDR_CLK_CTRL_DDR_POST_DIV_SHIFT) &
	    AR934X_PLL_CPU_DDR_CLK_CTRL_DDR_POST_DIV_MASK;

	if (clk_ctrl & AR934X_PLL_CPU_DDR_CLK_CTRL_DDR_PLL_BYPASS)
		u_ar71xx_ddr_freq = u_ar71xx_refclk;
	else if (clk_ctrl & AR934X_PLL_CPU_DDR_CLK_CTRL_DDRCLK_FROM_DDRPLL)
		u_ar71xx_ddr_freq = ddr_pll / (postdiv + 1);
	else
		u_ar71xx_ddr_freq = cpu_pll / (postdiv + 1);

	postdiv = (clk_ctrl >> AR934X_PLL_CPU_DDR_CLK_CTRL_AHB_POST_DIV_SHIFT) &
		  AR934X_PLL_CPU_DDR_CLK_CTRL_AHB_POST_DIV_MASK;

	if (clk_ctrl & AR934X_PLL_CPU_DDR_CLK_CTRL_AHB_PLL_BYPASS)
		u_ar71xx_ahb_freq = u_ar71xx_refclk;
	else if (clk_ctrl & AR934X_PLL_CPU_DDR_CLK_CTRL_AHBCLK_FROM_DDRPLL)
		u_ar71xx_ahb_freq = ddr_pll / (postdiv + 1);
	else
		u_ar71xx_ahb_freq = cpu_pll / (postdiv + 1);

	u_ar71xx_wdt_freq = u_ar71xx_refclk;
	u_ar71xx_uart_freq = u_ar71xx_refclk;
}

static void
ar934x_chip_device_stop(uint32_t mask)
{
	uint32_t reg;

	reg = ATH_READ_REG(AR934X_RESET_REG_RESET_MODULE);
	ATH_WRITE_REG(AR934X_RESET_REG_RESET_MODULE, reg | mask);
}

static void
ar934x_chip_device_start(uint32_t mask)
{
	uint32_t reg;

	reg = ATH_READ_REG(AR934X_RESET_REG_RESET_MODULE);
	ATH_WRITE_REG(AR934X_RESET_REG_RESET_MODULE, reg & ~mask);
}

static int
ar934x_chip_device_stopped(uint32_t mask)
{
	uint32_t reg;

	reg = ATH_READ_REG(AR934X_RESET_REG_RESET_MODULE);
	return ((reg & mask) == mask);
}

static void
ar934x_chip_set_mii_speed(uint32_t unit, uint32_t speed)
{

	/* XXX TODO */
	return;
}

/*
 * XXX TODO !!
 */
static void
ar934x_chip_set_pll_ge(int unit, int speed, uint32_t pll)
{

	switch (unit) {
	case 0:
		/* XXX TODO */
		break;
	case 1:
		/* XXX TODO */
		break;
	default:
		printf("%s: invalid PLL set for arge unit: %d\n",
		    __func__, unit);
		return;
	}
}

static void
ar934x_chip_ddr_flush_ge(int unit)
{

	switch (unit) {
	case 0:
		ar71xx_ddr_flush(AR934X_DDR_REG_FLUSH_GE0);
		break;
	case 1:
		ar71xx_ddr_flush(AR934X_DDR_REG_FLUSH_GE1);
		break;
	default:
		printf("%s: invalid DDR flush for arge unit: %d\n",
		    __func__, unit);
		return;
	}
}

static void
ar934x_chip_ddr_flush_ip2(void)
{

	ar71xx_ddr_flush(AR934X_DDR_REG_FLUSH_WMAC);
}

static uint32_t
ar934x_chip_get_eth_pll(unsigned int mac, int speed)
{
#if 0
	uint32_t pll;

	switch (speed) {
	case 10:
		pll = AR933X_PLL_VAL_10;
		break;
	case 100:
		pll = AR933X_PLL_VAL_100;
		break;
	case 1000:
		pll = AR933X_PLL_VAL_1000;
		break;
	default:
		printf("%s%d: invalid speed %d\n", __func__, mac, speed);
		pll = 0;
	}
	return (pll);
#endif
	return (0);
}

static void
ar934x_chip_init_usb_peripheral(void)
{
	uint32_t reg;

	reg = ATH_READ_REG(AR934X_RESET_REG_BOOTSTRAP);
	if (reg & AR934X_BOOTSTRAP_USB_MODE_DEVICE)
		return;

	ar71xx_device_stop(AR934X_RESET_USBSUS_OVERRIDE);
	DELAY(100);

	ar71xx_device_start(AR934X_RESET_USB_PHY);
	DELAY(100);

	ar71xx_device_start(AR934X_RESET_USB_PHY_ANALOG);
	DELAY(100);

	ar71xx_device_start(AR934X_RESET_USB_HOST);
	DELAY(100);
}

struct ar71xx_cpu_def ar934x_chip_def = {
	&ar934x_chip_detect_mem_size,
	&ar934x_chip_detect_sys_frequency,
	&ar934x_chip_device_stop,
	&ar934x_chip_device_start,
	&ar934x_chip_device_stopped,
	&ar934x_chip_set_pll_ge,
	&ar934x_chip_set_mii_speed,
	&ar71xx_chip_set_mii_if,
	&ar934x_chip_ddr_flush_ge,
	&ar934x_chip_get_eth_pll,
	&ar934x_chip_ddr_flush_ip2,
	&ar934x_chip_init_usb_peripheral
};
