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

#include "qemu/notify.h"

enum {
    TEGRA2_A9_CORE0 = 0,
    TEGRA2_A9_CORE1,
    TEGRA2_COP,
    TEGRA2_A9_NCORES = TEGRA2_COP,
    TEGRA2_NCPUS
};

void tegra_cpu_halt(int cpu_id);
void tegra_cpu_unhalt(int cpu_id);
void tegra_cpu_reset_assert(int cpu_id);
void tegra_cpu_reset_deassert(int cpu_id, int flow);
int tegra_cpu_is_powergated(int cpu_id);
void tegra_cpu_powergate(int cpu_id);
void tegra_cpu_unpowergate(int cpu_id);
uint32_t tegra_get_wfe_bitmap(void);
void tegra_flow_wfe_handle(int cpu_id);
void tegra_cpu_reset_init(void);
int tegra_sibling_cpu(int cpu_id);
int tegra_cpu_halted(int cpu_id);
void set_is_tegra_cpu(int cpu_id);
