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
#include "sysemu/sysemu.h"

#include "tcg-op.h"

#include "devices.h"
#include "iomap.h"
#include "tegra_cpu.h"
#include "tegra_trace.h"

#include "tegra_cpu_priv.h"

// #undef TPRINT
// #define TPRINT(...) {}

static int tegra_A9_powergated;
static int tegra_AVP_powergated;

static int tcpu_in_reset[TEGRA2_NCPUS];

static void tegra_dump_cpus_pc(void)
{
#if defined(TEGRA_TRACE) && 0
    CPUState *cs;

    const char * tegra_cpu_name[TEGRA2_NCPUS] = {
        [TEGRA2_A9_CORE0] = "CPU0",
        [TEGRA2_A9_CORE1] = "CPU1",
        [TEGRA2_COP]  = "COP",
    };

    TPRINT("*** dump cpus pc ***\n");

    CPU_FOREACH(cs) {
        ARMCPU *cpu = ARM_CPU(cs);

        TPRINT("%s: pc= 0x%08X halted=%d powered_off=%d\n",
               tegra_cpu_name[cs->cpu_index], cpu->env.regs[15],
               cs->halted, cpu->powered_off);
    }

    TPRINT("----------------------------------\n");
#endif
}

static void tegra_cpu_pwrgate_reset(void *opaque)
{
    tegra_A9_powergated = 0;
    tegra_AVP_powergated = 0;

    tegra_cpu_hlt_clr();
}

void tegra_cpu_reset_init(void)
{
    qemu_register_reset(tegra_cpu_pwrgate_reset, NULL);
}

int tegra_cpu_reset_asserted(int cpu_id)
{
    return tcpu_in_reset[cpu_id];
}

void tegra_cpu_reset_assert(int cpu_id)
{
    CPUState *cs = qemu_get_cpu(cpu_id);
    ARMCPU *cpu = ARM_CPU(cs);

    TPRINT("%s cpu %d tcpu_in_reset: %d\n",
           __func__, cpu_id, tcpu_in_reset[cpu_id]);

    cpu_reset(cs);
    cpu->env.thumb = 0;
    CPU_GET_CLASS(cpu)->set_pc(cs, 0xf0010000);

    tegra_cpu_stop(cpu_id);
    tcpu_in_reset[cpu_id] = 1;

    if (tcg_enabled() && qemu_cpu_is_self(cs))
        tcg_gen_exit_tb(0);
}

void tegra_cpu_reset_deassert(int cpu_id)
{
    CPUState *cs = qemu_get_cpu(cpu_id);
    ARMCPU *cpu = ARM_CPU(cs);

    TPRINT("%s cpu %d tcpu_in_reset: %d\n",
           __func__, cpu_id, tcpu_in_reset[cpu_id]);

    if (!cpu->powered_off)
        return;

    tcpu_in_reset[cpu_id] = 0;
    tegra_cpu_run(cpu_id);
}

int tegra_cpu_is_powergated(int cpu_id)
{
    int ret;

    switch (cpu_id) {
    case TEGRA2_A9_CORE0:
    case TEGRA2_A9_CORE1:
        ret = tegra_A9_powergated;
        break;
    case TEGRA2_COP:
        ret = tegra_AVP_powergated;
        break;
    default:
        g_assert_not_reached();
    }

    return ret;
}

static void tegra_cpu_powergateA9(void)
{
    CPUState *cs1 = qemu_get_cpu(TEGRA2_A9_CORE1);

    TPRINT("%s\n", __func__);

    tegra_dump_cpus_pc();

    assert(!tegra_A9_powergated);
    assert(cs1->halted);

    tegra_cpu_reset_assert(TEGRA2_A9_CORE0);
    tegra_a9mpcore_reset();
    tegra_A9_powergated = 1;
}

static void tegra_cpu_unpowergateA9(void)
{
    TPRINT("%s powergated=%d\n", __func__, tegra_A9_powergated);

    assert(tegra_A9_powergated);

    tegra_cpu_reset_deassert(TEGRA2_A9_CORE0);
    tegra_A9_powergated = 0;
}

static void tegra_cpu_powergateAVP(void)
{
    TPRINT("%s powergated=%d\n", __func__, tegra_AVP_powergated);

    assert(!tegra_AVP_powergated);

    tegra_cpu_reset_assert(TEGRA2_COP);
    tegra_AVP_powergated = 1;
}

static void tegra_cpu_unpowergateAVP(void)
{
    TPRINT("%s\n", __func__);

    assert(tegra_AVP_powergated);

    tegra_cpu_reset_deassert(TEGRA2_COP);
    tegra_AVP_powergated = 0;
}

void tegra_cpu_powergate(int cpu_id)
{
    switch (cpu_id) {
    case TEGRA2_A9_CORE0:
    case TEGRA2_A9_CORE1:
        tegra_cpu_powergateA9();
        break;
    case TEGRA2_COP:
        tegra_cpu_powergateAVP();
        break;
    default:
        g_assert_not_reached();
    }
}

void tegra_cpu_unpowergate(int cpu_id)
{
    switch (cpu_id) {
    case TEGRA2_A9_CORE0:
    case TEGRA2_A9_CORE1:
        tegra_cpu_unpowergateA9();
        break;
    case TEGRA2_COP:
        tegra_cpu_unpowergateAVP();
        break;
    default:
        g_assert_not_reached();
    }
}