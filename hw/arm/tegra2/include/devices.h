/*
 * ARM NVIDIA Tegra2 emulation.
 *
 * Copyright (c) 2014-2015 Dmitry Osipenko <digetx@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TEGRA_DEVICES_H
#define TEGRA_DEVICES_H

extern void * tegra_a9mpcore_dev;
extern void * tegra_apb_dma_dev;
extern void * tegra_ahb_gizmo_dev;
extern void * tegra_apb_misc_dev;
extern void * tegra_car_dev;
extern void * tegra_emc_dev;
extern void * tegra_flow_dev;
extern void * tegra_fuse_dev;
extern void * tegra_gpios_dev;
extern void * tegra_mc_dev;
extern void * tegra_pmc_dev;
extern void * tegra_rtc_dev;
extern void * tegra_sdhci4_dev;
extern void * tegra_timer1_dev;
extern void * tegra_timer2_dev;
extern void * tegra_timer_us_dev;
extern void * tegra_timer3_dev;
extern void * tegra_timer4_dev;
extern void * tegra_uarta_dev;
extern void * tegra_uartd_dev;
extern void * tegra_dc1_dev;
extern void * tegra_ehci1_dev;
extern void * tegra_ehci2_dev;
extern void * tegra_ehci3_dev;
extern void * tegra_bsea_dev;
extern void * tegra_bsev_dev;
extern void * tegra_idc1_dev;
extern void * tegra_idc2_dev;
extern void * tegra_idc3_dev;
extern void * tegra_dvc_dev;
extern void * tegra_grhost_dev;
extern void * tegra_arb_sema_dev;
extern void * tegra_arb_gnt_ictlr_dev;
extern void * tegra_irq_dispatcher_dev;
extern void * tegra_evp_dev;
extern void * tegra_ictlr_dev;
extern void * tegra_gr2d_dev;
extern void * tegra_host1x_dev;
extern void * tegra_cop_mmu_dev;
extern void * tegra_res_sema_dev;
extern void * tegra_ucq_dev;
extern void * tegra_sxe_dev;
extern void * tegra_mbe_dev;
extern void * tegra_ppe_dev;
extern void * tegra_mce_dev;
extern void * tegra_tfe_dev;
extern void * tegra_ppb_dev;
extern void * tegra_vdma_dev;
extern void * tegra_ucq2_dev;
extern void * tegra_bsea2_dev;
extern void * tegra_frameid_dev;
extern void *tegra_ahb_dma_dev;

void tegra_a9mpcore_reset(void);
void tegra_device_reset(void *dev_);

#endif // TEGRA_DEVICES_H
