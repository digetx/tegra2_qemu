/*
 * ARM NVIDIA Tegra2 emulation.
 *
 * Copyright (c) 2015 Dmitry Osipenko <digetx@gmail.com>
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

#ifndef TEGRA_VDE_H
#define TEGRA_VDE_H

#define TYPE_TEGRA_VDE_MBE "tegra.mbe"
#define TEGRA_VDE_MBE(obj) OBJECT_CHECK(tegra_mbe, (obj), TYPE_TEGRA_VDE_MBE)

typedef struct tegra_mbe_state {
    SysBusDevice parent_obj;

    uint32_t regs[64];
    MemoryRegion iomem;
} tegra_mbe;

#define TYPE_TEGRA_VDE_MCE "tegra.mce"
#define TEGRA_VDE_MCE(obj) OBJECT_CHECK(tegra_mce, (obj), TYPE_TEGRA_VDE_MCE)

typedef struct tegra_mce_state {
    SysBusDevice parent_obj;

    uint32_t regs[64];
    MemoryRegion iomem;
} tegra_mce;

#define TYPE_TEGRA_VDE_SXE "tegra.sxe"
#define TEGRA_VDE_SXE(obj) OBJECT_CHECK(tegra_sxe, (obj), TYPE_TEGRA_VDE_SXE)

typedef struct tegra_sxe_state {
    SysBusDevice parent_obj;

    uint32_t regs[1024];
    MemoryRegion iomem;
} tegra_sxe;

#define TYPE_TEGRA_VDE_TFE "tegra.tfe"
#define TEGRA_VDE_TFE(obj) OBJECT_CHECK(tegra_tfe, (obj), TYPE_TEGRA_VDE_TFE)

typedef struct tegra_tfe_state {
    SysBusDevice parent_obj;

    uint32_t regs[64];
    MemoryRegion iomem;
} tegra_tfe;

#define TYPE_TEGRA_VDE_VDMA "tegra.vdma"
#define TEGRA_VDE_VDMA(obj) OBJECT_CHECK(tegra_vdma, (obj), TYPE_TEGRA_VDE_VDMA)

typedef struct tegra_vdma_state {
    SysBusDevice parent_obj;

    uint32_t regs[64];
    MemoryRegion iomem;
} tegra_vdma;

#define TYPE_TEGRA_VDE_FRAMEID "tegra.frameid"
#define TEGRA_VDE_FRAMEID(obj) OBJECT_CHECK(tegra_frameid, (obj), TYPE_TEGRA_VDE_FRAMEID)

typedef struct tegra_frameid_state {
    SysBusDevice parent_obj;

    uint32_t regs[768];
    MemoryRegion iomem;
} tegra_frameid;

#endif // TEGRA_VDE_H
