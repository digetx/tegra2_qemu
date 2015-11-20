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

#include "hw/arm/arm.h"
#include "exec/helper-proto.h"

#include "devices.h"
#include "iomap.h"
#include "tegra_cpu.h"
#include "tegra_trace.h"

#define HALT_WFE    0xff

#define IS_SIBLING(a, b)    (tegra_cpu_siblings_bitmap[a] & (1 << b))

static const uint16_t tegra_cpu_siblings_bitmap[TEGRA2_NCPUS] = {
    [TEGRA2_A9_CORE0] = (1 << TEGRA2_A9_CORE1),
    [TEGRA2_A9_CORE1] = (1 << TEGRA2_A9_CORE0),
    [TEGRA2_COP]  = 0,
};

static NotifierList wfe_notifier_list =
            NOTIFIER_LIST_INITIALIZER(wfe_notifier_list);

void tegra_register_wfe_notifier(Notifier *notifier)
{
    notifier_list_add(&wfe_notifier_list, notifier);
}

void tegra_unregister_wfe_notifier(Notifier *notifier)
{
    notifier_remove(notifier);
}

uint32_t tegra_get_wfe_bitmap(void)
{
    CPUState *cs;
    uint32_t wfe_bitmap = 0;

    CPU_FOREACH(cs) {
        if (cs->halted == HALT_WFE)
            wfe_bitmap |= 1 << cs->cpu_index;
    }

    return wfe_bitmap;
}

void HELPER(sev)(CPUARMState *env)
{
    CPUState *cs = CPU(arm_env_get_cpu(env));
    CPUState *csX;

    CPU_FOREACH(csX) {
        ARMCPU *cpu = ARM_CPU(csX);

        if (csX == cs || cpu->powered_off)
            continue;

        if (!IS_SIBLING(cs->cpu_index, csX->cpu_index))
            continue;

        if (csX->halted == HALT_WFE) {
//             TPRINT("SEV: kicking cpu %d\n", csX->cpu_index);
            csX->halted = 0;
            qemu_cpu_kick(csX);
        }
    }

    cpu_loop_exit(cs);
}

void HELPER(wfe)(CPUARMState *env)
{
    CPUState *cs = CPU(arm_env_get_cpu(env));

//     TPRINT("WFE: cpu %d\n", cs->cpu_index);

    cs->exception_index = EXCP_HLT;
    cs->halted = HALT_WFE;

    notifier_list_notify(&wfe_notifier_list, cs);

    cpu_loop_exit(cs);
}
