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

#include "exec/address-spaces.h"
#include "hw/boards.h"
#include "hw/sysbus.h"
#include "hw/arm/arm.h"
#include "hw/loader.h"
#include "hw/char/serial.h"
#include "hw/usb/hcd-ehci.h"
#include "qemu/error-report.h"
#include "sysemu/sysemu.h"
#include "sysemu/char.h"

#include "hw/devices.h"
#include "net/net.h"

#include "devices.h"
#include "iomap.h"
#include "irqs.h"
#include "tegra_cpu.h"
#include "tegra_trace.h"

#define DIRQ(X) qdev_get_gpio_in(tegra_irq_dispatcher_dev, X)
#define DIRQ_INT(X) qdev_get_gpio_in(tegra_irq_dispatcher_dev, X + INT_MAIN_NR)

static void memory_region_add_and_init_ram(MemoryRegion *mr, const char *name,
                                           hwaddr offset, uint64_t size, int ro)
{
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    memory_region_init_ram(ram, NULL, name, size, &error_abort);
    vmstate_register_ram_global(ram);
    memory_region_set_readonly(ram, ro);
    memory_region_add_subregion(mr, offset, ram);
}

static void tegra2_create_cpus(void)
{
    ObjectClass *cpu_oc;
    int i;

    cpu_oc = cpu_class_by_name(TYPE_ARM_CPU, "cortex-a9");
    assert(cpu_oc != NULL);

    for (i = 0; i < TEGRA2_A9_NCORES; i++) {
        Object *cpuobj = object_new(object_class_get_name(cpu_oc));

        object_property_set_int(cpuobj, TEGRA_ARM_PERIF_BASE, "reset-cbar", &error_abort);
        object_property_set_bool(cpuobj, true, "reset-hivecs", &error_abort);
        object_property_set_bool(cpuobj, true, "start-powered-off", &error_abort);
        object_property_set_bool(cpuobj, true, "realized", &error_abort);
    }

    /* AVP */
    cpu_oc = cpu_class_by_name(TYPE_ARM_CPU, "arm7tdmi");
    assert(cpu_oc != NULL);

    Object *cpuobj = object_new(object_class_get_name(cpu_oc));
    object_property_set_bool(cpuobj, true, "start-powered-off", &error_abort);
    object_property_set_bool(cpuobj, true, "realized", &error_abort);
}

static void tegra2_init(MachineState *machine)
{
    MemoryRegion *sysmem = get_system_memory();
    SysBusDevice *irq_dispatcher, *gic, *lic;
    DeviceState *cpudev;
    int i, j;

    /* Main RAM */
    assert(machine->ram_size <= TEGRA_DRAM_SIZE);
    memory_region_add_and_init_ram(sysmem, "tegra.dram",
                                   TEGRA_DRAM_BASE, machine->ram_size, 0);

    memory_region_add_and_init_ram(sysmem, "tegra.hi-vec",
                                   0xffff0000, SZ_64K, 0);
    memory_region_add_and_init_ram(sysmem, "tegra.bootmon",
                                   0xf0010000, TARGET_PAGE_SIZE, 1);

    /* Create the actual CPUs */
    tegra2_create_cpus();

    /* A9MPCore (SCU) private memory region */
    tegra_a9mpcore_dev = qdev_create(NULL, "a9mpcore_priv");
    qdev_prop_set_uint32(tegra_a9mpcore_dev, "num-cpu", TEGRA2_A9_NCORES);
    qdev_prop_set_uint32(tegra_a9mpcore_dev, "num-irq", INT_GIC_NR);
    qdev_init_nofail(tegra_a9mpcore_dev);
    gic = SYS_BUS_DEVICE(tegra_a9mpcore_dev);
    sysbus_mmio_map(gic, 0, TEGRA_ARM_PERIF_BASE);

    /* Legacy interrupt controller */
    tegra_ictlr_dev = qdev_create(NULL, "tegra.ictlr");
    qdev_init_nofail(tegra_ictlr_dev);
    lic = SYS_BUS_DEVICE(tegra_ictlr_dev);
    sysbus_mmio_map(lic, 0, TEGRA_PRIMARY_ICTLR_BASE);

    tegra_irq_dispatcher_dev = qdev_create(NULL, "tegra.irq_dispatcher");
    qdev_init_nofail(tegra_irq_dispatcher_dev);
    irq_dispatcher = SYS_BUS_DEVICE(tegra_irq_dispatcher_dev);

    for (i = 0, j = 0; i < TEGRA2_NCPUS; i++) {
        cpudev = DEVICE(qemu_get_cpu(i));
        sysbus_connect_irq(irq_dispatcher, j++,
                                        qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
        sysbus_connect_irq(irq_dispatcher, j++,
                                        qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
    }

    sysbus_connect_irq(gic, TEGRA2_A9_CORE0, DIRQ_INT(ARM_CPU_IRQ));
    sysbus_connect_irq(gic, TEGRA2_A9_CORE1, DIRQ_INT(ARM_CPU_IRQ + 1));

    /* CPU irq+fiq */
    sysbus_connect_irq(lic, 0, DIRQ_INT(ARM_CPU_IRQ + 2));
    sysbus_connect_irq(lic, 1, DIRQ_INT(ARM_CPU_FIQ + 2));

    /* COP irq+fiq */
    sysbus_connect_irq(lic, 2, DIRQ_INT(ARM_CPU_IRQ + 4));
    sysbus_connect_irq(lic, 3, DIRQ_INT(ARM_CPU_FIQ + 4));

    /* Cache controller */
    sysbus_create_simple("l2x0", TEGRA_ARM_PL310_BASE, NULL);

    /* COP Internal RAM */
    memory_region_add_and_init_ram(sysmem, "tegra.iram",
                                   TEGRA_IRAM_BASE, TEGRA_IRAM_SIZE, 0);

    memory_region_add_and_init_ram(sysmem, "tegra.irom", 0xfff00000, 0xC000, 1);

    /* Exception vectors */
    tegra_evp_dev = sysbus_create_simple("tegra.evp",
                                         TEGRA_EXCEPTION_VECTORS_BASE, NULL);

    /* EMC controller */
    tegra_emc_dev = sysbus_create_simple("tegra.emc", TEGRA_EMC_BASE, NULL);

    /* MC controller */
    tegra_mc_dev = sysbus_create_simple("tegra.mc", TEGRA_MC_BASE, NULL);

    /* AHB Gizmo controller */
    tegra_ahb_gizmo_dev = sysbus_create_simple("tegra.ahb_gizmo",
                                               TEGRA_AHB_GIZMO_BASE, NULL);

    /* APB FUSE controller */
    tegra_fuse_dev = sysbus_create_simple("tegra.fuse", TEGRA_FUSE_BASE, NULL);

    /* APB DMA controller */
    tegra_apb_dma_dev = sysbus_create_simple("tegra.apb_dma",
                                             TEGRA_APB_DMA_BASE, NULL);

    /* APB bus controller */
    tegra_apb_misc_dev = sysbus_create_simple("tegra.apb_misc",
                                              TEGRA_APB_MISC_BASE, NULL);

    /* Clock and reset controller */
    tegra_car_dev = sysbus_create_simple("tegra.car",
                                         TEGRA_CLK_RESET_BASE, NULL);

    /* Flow controller */
    tegra_flow_dev = sysbus_create_varargs("tegra.flow", TEGRA_FLOW_CTRL_BASE,
                                           DIRQ(INT_FLOW_RSM0),
                                           DIRQ(INT_FLOW_RSM1),
                                           NULL);

    /* GPIO controllers */
    tegra_gpios_dev = sysbus_create_varargs("tegra.gpio", TEGRA_GPIO_BASE,
                                            DIRQ(INT_GPIO1), DIRQ(INT_GPIO2),
                                            DIRQ(INT_GPIO3), DIRQ(INT_GPIO4),
                                            DIRQ(INT_GPIO5), DIRQ(INT_GPIO6),
                                            DIRQ(INT_GPIO7), NULL);

    /* PMC controller */
    tegra_pmc_dev = sysbus_create_simple("tegra.pmc", TEGRA_PMC_BASE, NULL);

    /* RTC */
    tegra_rtc_dev = sysbus_create_simple("tegra.rtc",
                                         TEGRA_RTC_BASE, DIRQ(INT_RTC));

    /* SDHC4 controller */
    tegra_sdhci4_dev = qdev_create(NULL, "generic-sdhci");
    qdev_prop_set_uint32(tegra_sdhci4_dev, "capareg", 0x5780A81);
    qdev_init_nofail(tegra_sdhci4_dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(tegra_sdhci4_dev), 0, TEGRA_SDMMC4_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(tegra_sdhci4_dev), 0, DIRQ(INT_SDMMC4));
//     tegra_sdhci4_dev = sysbus_create_simple("tegra.sdhci",
//                                             TEGRA_SDMMC4_BASE, DIRQ(INT_SDMMC4));

    /* Timer1 */
    tegra_timer1_dev = sysbus_create_simple("tegra.timer",
                                            TEGRA_TMR1_BASE, DIRQ(INT_TMR1));

    /* Timer2 */
    tegra_timer2_dev = sysbus_create_simple("tegra.timer",
                                            TEGRA_TMR2_BASE, DIRQ(INT_TMR2));

    /* TimerUS */
    tegra_timer_us_dev = sysbus_create_simple("tegra.timer_us",
                                              TEGRA_TMRUS_BASE, NULL);

    /* Timer3 */
    tegra_timer3_dev = sysbus_create_simple("tegra.timer",
                                            TEGRA_TMR3_BASE, DIRQ(INT_TMR3));

    /* Timer4 */
    tegra_timer4_dev = sysbus_create_simple("tegra.timer",
                                            TEGRA_TMR4_BASE, DIRQ(INT_TMR4));

    /* UARTD controller */
//     sysbus_create_simple("tegra.uart", TEGRA_UARTA_BASE, DIRQ(INT_UARTA));
    tegra_uartd_dev = serial_mm_init(sysmem, TEGRA_UARTD_BASE, 2,
                                     DIRQ(INT_UARTD), 115200,
                                     qemu_char_get_next_serial(),
                                     DEVICE_LITTLE_ENDIAN);

    /* Secure boot stub */
    memory_region_add_and_init_ram(sysmem, "tegra.secure_boot",
                                   TEGRA_SB_BASE, TEGRA_SB_SIZE, 0);

    /* USB2 controllers */
    tegra_ehci1_dev = sysbus_create_simple(TYPE_TEGRA2_EHCI,
                                           TEGRA_USB_BASE, DIRQ(INT_USB));
//     tegra_ehci2_dev = sysbus_create_simple(TYPE_TEGRA2_EHCI,
//                                            TEGRA_USB2_BASE, DIRQ(INT_USB2));
    tegra_ehci3_dev = sysbus_create_simple(TYPE_TEGRA2_EHCI,
                                           TEGRA_USB3_BASE, DIRQ(INT_USB3));

    /* TODO: share BSE with VD/AD */
    tegra_bsea_dev = qdev_create(NULL, "tegra.bse");
    object_property_set_bool(tegra_bsea_dev, true, "key_sched_gen_supported", &error_abort);
    object_property_set_int(tegra_bsea_dev, 80, "hw_key_sched_length", &error_abort);
    qdev_init_nofail(tegra_bsea_dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(tegra_bsea_dev), 0, TEGRA_BSEA_BASE + 0x1000);
    sysbus_connect_irq(SYS_BUS_DEVICE(tegra_bsea_dev), 0, DIRQ(INT_VDE_BSE_A));

    tegra_bsev_dev = sysbus_create_simple("tegra.bse", TEGRA_VDE_BASE + 0x1000,
                                          DIRQ(INT_VDE_BSE_V));

    /* I2C controllers */
    tegra_idc1_dev = sysbus_create_simple("tegra-i2c",
                                          TEGRA_I2C_BASE, DIRQ(INT_I2C));
    tegra_idc2_dev = sysbus_create_simple("tegra-i2c",
                                          TEGRA_I2C2_BASE, DIRQ(INT_I2C2));
    tegra_idc3_dev = sysbus_create_simple("tegra-i2c",
                                          TEGRA_I2C3_BASE, DIRQ(INT_I2C3));
    tegra_dvc_dev = qdev_create(NULL, "tegra-i2c");
    object_property_set_bool(tegra_dvc_dev, true, "is_dvc", &error_abort);
    qdev_init_nofail(tegra_dvc_dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(tegra_dvc_dev), 0, TEGRA_DVC_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(tegra_dvc_dev), 0, DIRQ(INT_DVC));

    /* host1x CDMA and syncpts */
    tegra_grhost_dev = sysbus_create_varargs("tegra.grhost",
                                             TEGRA_GRHOST_BASE,
                                             DIRQ(INT_HOST1X_MPCORE_SYNCPT),
                                             DIRQ(INT_HOST1X_MPCORE_GENERAL),
                                             NULL);

    /* GPU 2d */
    tegra_gr2d_dev = sysbus_create_simple("tegra.gr2d", TEGRA_GR2D_BASE, NULL);

    /* Display1 controller */
    tegra_dc1_dev = sysbus_create_simple("tegra.dc", TEGRA_DISPLAY_BASE,
                                         DIRQ(INT_DISPLAY_GENERAL));

    /* PG tag */
    sysbus_create_simple("tegra.pg", 0x60000000, NULL);

    /* PIO ethernet */
    if (nd_table[0].used)
        lan9118_init(&nd_table[0], 0xA0000000, DIRQ(INT_SW_RESERVED));

    /* Arbitration */
    tegra_arb_sema_dev = sysbus_create_varargs("tegra.arb_sema",
                                               TEGRA_ARB_SEMA_BASE,
                                               DIRQ(INT_GNT_0),
                                               DIRQ(INT_GNT_1),
                                               NULL);
    tegra_boot(machine);
}

static void tegra2_reset(void)
{
    tegra_trace_init();
    qemu_devices_reset();

    tegra_cpu_reset_deassert(TEGRA2_COP);
}

static QEMUMachine tegra2_alpha_machine = {
    .name = "tegra2-alpha",
    .desc = "ARM NVIDIA Tegra2",
    .init = tegra2_init,
    .reset = tegra2_reset,
    .max_cpus = TEGRA2_NCPUS,
};

static void tegra2_machine_init(void)
{
    qemu_register_machine(&tegra2_alpha_machine);
}

machine_init(tegra2_machine_init);
