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

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/config-file.h"
#include "qemu/option.h"
#include "cpu.h"
#include "exec/address-spaces.h"
#include "hw/boards.h"
#include "hw/sysbus.h"
#include "hw/arm/arm.h"
#include "hw/loader.h"
#include "hw/char/serial.h"
#include "hw/sd/sdhci.h"
#include "sysemu/sysemu.h"
#include "sysemu/cpus.h"

#include "hw/devices.h"
#include "net/net.h"

#include "devices.h"
#include "iomap.h"
#include "irqs.h"
#include "remote_io.h"
#include "tegra_cpu.h"
#include "tegra_trace.h"

#define DIRQ(X) qdev_get_gpio_in(tegra_irq_dispatcher_dev, X)
#define DIRQ_INT(X) qdev_get_gpio_in(tegra_irq_dispatcher_dev, X + INT_MAIN_NR)

#define BOOTLOADER_BASE 0x108000
#define BOOTROM_BASE    0xFFF00000
#define BOOTMON_BASE    0xF0010000

#define RW  0
#define RO  1

#define JMP_FIXUP   (sizeof(tegra_bootrom) / 4 - 2)

/* WARNING: HACK! FIXME: Copied from exec.c  */
struct CPUAddressSpace {
    CPUState *cpu;
    AddressSpace *as;
    struct AddressSpaceDispatch *memory_dispatch;
    MemoryListener tcg_as_listener;
};

static uint32_t tegra_bootmon[] = {
    0xe3a00206, /* ldr r0, =TEGRA_PG */
    0xe5901000, /* ldr r1, [r0]] */
    0xe59f0054, /* ldr r0, =TEGRA_PG_A9 */
    0xe1500001, /* cmp r0, r1 */
    0x1a00000f, /* bne boot         @ on AVP */
    0xee100fb0, /* mrc 15, 0, r0, cr0, cr0, {5} */
    0xe200000f, /* and r0, r0, #0xF */
    0xe3500000, /* cmp r0, #0 */
    0x0a00000b, /* beq boot */
    0xe59f303c, /* ldr r3, =bootX */
    0xe5930000, /* ldr r0, [r3] */
    0xe3500001, /* cmp r0, #1 */
    0x1a000007, /* bne boot */
    0xe59f2030, /* ldr r2, =TEGRA_ARM_INT_DIST_BASE */
    0xe3a01001, /* mov r1, #1 */
    0xe5821000, /* str r1, [r2]         @ set GICC_CTLR.Enable */
    0xe3a010ff, /* mov r1, #0xff */
    0xe5821004, /* str r1, [r2, #4]     @ set GIC_PMR.Priority to 0xff */
    0xf57ff04f, /* dsb */
    0xe3a01001, /* mov r1, #1 */
    0xe5831000, /* str r1, [r3] */
/* boot: */
    0xe59f0014, /* ldr r0, =TEGRA_EXCEPTION_VECTORS_BASE */
    0xe5900000, /* ldr r0, [r0] */
    0xe12fff10, /* bx  r0 */
    0x00000000, /* bootX */
    0x55555555, /* TEGRA_PG_A9 */
    0x00000060, /* TEGRA_PG */
    0x50041000, /* TEGRA_ARM_INT_DIST_BASE */
    0x6000f000, /* TEGRA_EXCEPTION_VECTORS_BASE */
};

static uint32_t tegra_bootrom[] = {
    0xea000006, /* b reset_addr */
    0xea000007, /* b undefined_addr */
    0xea000008, /* b svc_addr */
    0xea000009, /* b prefetch_addr */
    0xea00000a, /* b abort_addr */
    0xea00000b, /* b reserved_vector */
    0xea00000c, /* b irq_addr */
    0xea00000d, /* b fiq_handler */
/* reset_addr: */
    0xe59f0044, /* ldr r0, =DUMMY */
    0xe12fff10, /* bx  r0 */
/* undefined_addr: */
    0xe3a01001, /* mov r1, #1 */
    0xea00000a, /* b reboot */
/* svc_addr: */
    0xe3a01002, /* mov r1, #2 */
    0xea000008, /* b reboot */
/* prefetch_addr: */
    0xe3a01003, /* mov r1, #3 */
    0xea000006, /* b reboot */
/* abort_addr: */
    0xe3a01004, /* mov r1, #4 */
    0xea000004, /* b reboot */
/* reserved_vector: */
    0xe3a01005, /* mov r1, #5 */
    0xea000002, /* b reboot */
/* irq_addr: */
    0xe3a01006, /* mov r1, #6 */
    0xea000000, /* b reboot */
/* fiq_handler: */
    0xe3a01007, /* mov r1, #7 */
/* reboot: */
    0xe3a02010, /* mov r2, #0x10 */
    0xe59f0008, /* ldr r0, =TEGRA_PMC_BASE */
    0xe5802000, /* str r2, [r0] */
    0xeafffffb, /* b reboot */
    0xffffffff, /* DUMMY */
    TEGRA_PMC_BASE,
};

static void memory_region_add_and_init_ram(MemoryRegion *mr, const char *name,
                                           hwaddr offset, uint64_t size, int ro)
{
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    memory_region_init_ram(ram, NULL, name, size, &error_abort);
    memory_region_set_readonly(ram, ro);
    memory_region_add_subregion(mr, offset, ram);
}

static void cop_memory_region_add_alias(MemoryRegion *mr, const char *name,
                                        MemoryRegion *sysmem, hwaddr cop_offset,
                                         hwaddr sys_offset, uint64_t size)
{
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    memory_region_init_alias(ram, NULL, name, sysmem, sys_offset, size);
    memory_region_add_subregion(mr, cop_offset, ram);
}

static void tegra2_create_cpus(void)
{
    int i;

    for (i = 0; i < TEGRA2_A9_NCORES; i++) {
        Object *cpuobj = object_new(ARM_CPU_TYPE_NAME("cortex-a9"));

        object_property_set_int(cpuobj, TEGRA_ARM_PERIF_BASE, "reset-cbar", &error_abort);
        object_property_set_bool(cpuobj, false, "has_el3", &error_abort);
        object_property_set_bool(cpuobj, true, "reset-hivecs", &error_abort);
        object_property_set_bool(cpuobj, true, "start-powered-off", &error_abort);
        object_property_set_bool(cpuobj, true, "realized", &error_abort);
    }

    /* AVP(COP) Audio Video Processor */
    Object *cpuobj = object_new(ARM_CPU_TYPE_NAME("arm7tdmi"));
    object_property_set_bool(cpuobj, true, "start-powered-off", &error_abort);
    object_property_set_bool(cpuobj, true, "realized", &error_abort);

    set_is_tegra_cpu(TEGRA2_A9_CORE0);
    set_is_tegra_cpu(TEGRA2_A9_CORE1);
    set_is_tegra_cpu(TEGRA2_COP);
}

static void load_memory_images(MachineState *machine)
{
    QemuOpts *opts = qemu_find_opts_singleton("tegra");
    const char *bootloader_path = qemu_opt_get(opts, "bootloader");
    const char *iram_path = qemu_opt_get(opts, "iram");
    const char *dtb_path = qemu_opt_get(qemu_get_machine_opts(), "dtb");
    int tmp;

    if (bootloader_path == NULL) {
        fprintf(stderr, "-bootloader not specified\n");
        exit(1);
    }

    /* TODO: load bootloader from emmc */
    tegra_bootrom[JMP_FIXUP] = BOOTLOADER_BASE;

    for (tmp = 0; tmp < ARRAY_SIZE(tegra_bootrom); tmp++)
        tegra_bootrom[tmp] = tswap32(tegra_bootrom[tmp]);

    /* Load bootloader */
    assert(load_image_targphys(bootloader_path, BOOTLOADER_BASE,
                               machine->ram_size - BOOTLOADER_BASE) > 0);

    if (iram_path != NULL) {
        /* Load BIT */
        assert(load_image_targphys(iram_path, TEGRA_IRAM_BASE,
                                   TEGRA_IRAM_SIZE) > 0);
    }

    /* Load IROM */
    rom_add_blob_fixed("bootrom", tegra_bootrom, sizeof(tegra_bootrom),
                       BOOTROM_BASE);

    for (tmp = 0; tmp < ARRAY_SIZE(tegra_bootmon); tmp++)
        tegra_bootmon[tmp] = tswap32(tegra_bootmon[tmp]);

    /* Load boot monitor */
    rom_add_blob_fixed("bootmon", tegra_bootmon, sizeof(tegra_bootmon),
                       BOOTMON_BASE);

    if (machine->kernel_filename != NULL) {
        tmp = load_image_targphys(machine->kernel_filename, 0x1000000,
                                  machine->ram_size);
        assert(tmp > 0);

        if (dtb_path != NULL)
            assert(load_image_targphys(dtb_path, 0x1000000 + tmp,
                                       machine->ram_size) > 0);
    }
}

static void tegra2_init(MachineState *machine)
{
    MemoryRegion *cop_sysmem = g_new(MemoryRegion, 1);
    AddressSpace *cop_as = g_new(AddressSpace, 1);
    MemoryRegion *sysmem = get_system_memory();
    SysBusDevice *irq_dispatcher, *gic, *lic;
    DeviceState *cpudev;
    CPUState *cs;
    int i, j;

    /* Main RAM */
    assert(machine->ram_size <= TEGRA_DRAM_SIZE);
    memory_region_add_and_init_ram(sysmem, "tegra.dram",
                                   TEGRA_DRAM_BASE, machine->ram_size, RW);

    memory_region_add_and_init_ram(sysmem, "tegra.hi-vec",
                                   0xffff0000, SZ_64K, RW);

    memory_region_add_and_init_ram(sysmem, "tegra.bootmon",
                                   BOOTMON_BASE, TARGET_PAGE_SIZE, RO);

    /* Internal static RAM */
    memory_region_add_and_init_ram(sysmem, "tegra.iram",
                                   TEGRA_IRAM_BASE, TEGRA_IRAM_SIZE, RW);

    /* Map 0x400-0x40000 of IRAM to remote device.  */
//     memory_region_add_and_init_ram(sysmem, "tegra.iram",
//                                    TEGRA_IRAM_BASE, TEGRA_RESET_HANDLER_SIZE, RW);
//     sysbus_create_simple("tegra.remote_iram",
//                          TEGRA_IRAM_BASE + TEGRA_RESET_HANDLER_SIZE, NULL);

    memory_region_add_and_init_ram(sysmem, "tegra.irom",
                                   BOOTROM_BASE, 0xC000, RO);

    /* Secure boot stub */
    memory_region_add_and_init_ram(sysmem, "tegra.secure_boot",
                                   TEGRA_SB_BASE, TEGRA_SB_SIZE, RW);

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

    /* CPU IRQ+FIQ */
    sysbus_connect_irq(lic, 0, DIRQ_INT(ARM_CPU_IRQ + 2));
    sysbus_connect_irq(lic, 1, DIRQ_INT(ARM_CPU_FIQ + 2));

    /* COP IRQ+FIQ */
    sysbus_connect_irq(lic, 2, DIRQ_INT(ARM_CPU_IRQ + 4));
    sysbus_connect_irq(lic, 3, DIRQ_INT(ARM_CPU_FIQ + 4));

    /* Cache controller */
    sysbus_create_simple("l2x0", TEGRA_ARM_PL310_BASE, NULL);

    /* Exception vectors */
    tegra_evp_dev = sysbus_create_simple("tegra.evp",
                                         TEGRA_EXCEPTION_VECTORS_BASE, NULL);

    /* Embedded memory controller */
    tegra_emc_dev = sysbus_create_simple("tegra.emc", TEGRA_EMC_BASE, NULL);

    /* Memory controller */
    tegra_mc_dev = sysbus_create_simple("tegra.mc", TEGRA_MC_BASE, NULL);

    /* AHB DMA controller */
    tegra_ahb_dma_dev = sysbus_create_simple("tegra.ahb_dma",
                                             TEGRA_AHB_DMA_BASE,
                                             DIRQ(INT_AHB_DMA));

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

    /* CPU Flow controller */
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

    /* Power managment controller */
    tegra_pmc_dev = sysbus_create_simple("tegra.pmc", TEGRA_PMC_BASE, NULL);

    /* Real time clock */
    tegra_rtc_dev = sysbus_create_simple("tegra.rtc",
                                         TEGRA_RTC_BASE, DIRQ(INT_RTC));

    /* SDHC4 controller */
    {
        DeviceState *carddev;
        BlockBackend *blk;
        DriveInfo *di;

        tegra_sdhci4_dev = qdev_create(NULL, TYPE_SYSBUS_SDHCI);
        qdev_prop_set_uint8(tegra_sdhci4_dev, "sd-spec-version", 2);
        qdev_prop_set_uint32(tegra_sdhci4_dev, "capareg", 0x5780A8B);
        qdev_init_nofail(tegra_sdhci4_dev);

        sysbus_mmio_map(SYS_BUS_DEVICE(tegra_sdhci4_dev), 0, TEGRA_SDMMC4_BASE);
        sysbus_connect_irq(SYS_BUS_DEVICE(tegra_sdhci4_dev), 0, DIRQ(INT_SDMMC4));

        di = drive_get(IF_SD, 0, 0);
        blk = di ? blk_by_legacy_dinfo(di) : NULL;
        carddev = qdev_create(qdev_get_child_bus(tegra_sdhci4_dev, "sd-bus"), TYPE_SD_CARD);
        qdev_prop_set_drive(carddev, "drive", blk, &error_abort);
        qdev_prop_set_bit(carddev, "emmc", false);
        qdev_init_nofail(carddev);

//         tegra_sdhci4_dev = sysbus_create_simple("tegra.sdhci",
//                                                 TEGRA_SDMMC4_BASE, DIRQ(INT_SDMMC4));
    }

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
                                     serial_hd(0),
                                     DEVICE_LITTLE_ENDIAN);

    /* USB2 controllers */
    tegra_ehci1_dev = sysbus_create_simple("tegra.usb",
                                           TEGRA_USB_BASE, DIRQ(INT_USB));
//     tegra_ehci2_dev = sysbus_create_simple("tegra.usb",
//                                            TEGRA_USB2_BASE, DIRQ(INT_USB2));
    tegra_ehci3_dev = sysbus_create_simple("tegra.usb",
                                           TEGRA_USB3_BASE, DIRQ(INT_USB3));

    /* Unified Command Queue */
    tegra_ucq_dev = sysbus_create_simple("tegra.dummy256", 0x60010000, NULL);

    /* Bit Stream Engine Audio */
    tegra_bsea_dev = sysbus_create_simple("tegra.bsea", 0x60011000,
                                          DIRQ(INT_VDE_BSE_A));

    /* Syntax Engine */
    tegra_sxe_dev = sysbus_create_simple("tegra.sxe", 0x6001A000, NULL);

    /* BSE Video */
    tegra_bsev_dev = sysbus_create_simple("tegra.bsev", 0x6001B000,
                                          DIRQ(INT_VDE_BSE_V));

    /* Macroblock Engine */
    tegra_mbe_dev = sysbus_create_simple("tegra.mbe", 0x6001C000, NULL);

    /* Post-processing Engine */
    tegra_ppe_dev = sysbus_create_simple("tegra.dummy256", 0x6001C200, NULL);

    /* Motion Compensation Engine */
    tegra_mce_dev = sysbus_create_simple("tegra.mce", 0x6001C400, NULL);

    /* Transform Engine */
    tegra_tfe_dev = sysbus_create_simple("tegra.tfe", 0x6001C600, NULL);

    /* Pixel Prediction Block?? */
    tegra_ppb_dev = sysbus_create_simple("tegra.dummy256", 0x6001C800, NULL);

    /* Video DMA */
    tegra_vdma_dev = sysbus_create_simple("tegra.vdma", 0x6001CA00, NULL);

    /* Unified Command Queue */
    tegra_ucq2_dev = sysbus_create_simple("tegra.dummy256", 0x6001CC00, NULL);

    /* BSE Audio */
    tegra_bsea2_dev = sysbus_create_simple("tegra.dummy_2k", 0x6001D000, NULL);

    /* FRAMEID */
    tegra_frameid_dev = sysbus_create_simple("tegra.frameid", 0x6001D800, NULL);

//     sysbus_create_varargs("tegra.bse_remote", 0x60010000,
//                           DIRQ(INT_VDE_UCQ_ERROR),
//                           DIRQ(INT_VDE_SYNC_TOKEN),
//                           DIRQ(INT_VDE_BSE_V),
//                           DIRQ(INT_VDE_BSE_A),
//                           DIRQ(INT_VDE_SXE), NULL);

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

    /* Host1x IO */
    tegra_grhost_dev = sysbus_create_varargs("tegra.grhost",
                                             TEGRA_GRHOST_BASE,
                                             DIRQ(INT_HOST1X_COP_SYNCPT),
                                             DIRQ(INT_HOST1X_MPCORE_SYNCPT),
                                             DIRQ(INT_HOST1X_COP_GENERAL),
                                             DIRQ(INT_HOST1X_MPCORE_GENERAL),
                                             NULL);

    /* Host1x */
    tegra_host1x_dev = sysbus_create_simple("tegra.host1x", TEGRA_HOST1X_BASE,
                                            NULL);

    /* GPU 2d */
    tegra_gr2d_dev = sysbus_create_simple("tegra.gr2d", TEGRA_GR2D_BASE, NULL);

    /* Display1 controller */
    tegra_dc1_dev = sysbus_create_simple("tegra.dc", TEGRA_DISPLAY_BASE,
                                         DIRQ(INT_DISPLAY_GENERAL));

    /* Process generator tag */
    sysbus_create_simple("tegra.pg", 0x60000000, NULL);

    /* PIO ethernet */
    if (nd_table[0].used)
        lan9118_init(&nd_table[0], 0xA0000000, DIRQ(INT_SW_RESERVED));

    /* Multi-CPU shared resources access arbitration */
    tegra_arb_sema_dev = sysbus_create_varargs("tegra.arb_sema",
                                               TEGRA_ARB_SEMA_BASE,
                                               DIRQ(INT_GNT_0),
                                               DIRQ(INT_GNT_1),
                                               NULL);

    tegra_res_sema_dev = sysbus_create_varargs("tegra.res_sema",
                                               TEGRA_RES_SEMA_BASE,
                                               DIRQ(INT_SHR_SEM_INBOX_IBF),
                                               DIRQ(INT_SHR_SEM_INBOX_IBE),
                                               DIRQ(INT_SHR_SEM_OUTBOX_IBF),
                                               DIRQ(INT_SHR_SEM_OUTBOX_IBE),
                                               NULL);

    /* AVP "MMU" TLB controls.  */
    tegra_cop_mmu_dev = sysbus_create_simple("tegra.cop_mmu", 0xF0000000, NULL);

    /* COP's address map differs a bit from A9.  */
    memory_region_init(cop_sysmem, NULL, "tegra.cop-memory", UINT64_MAX);
    address_space_init(cop_as, cop_sysmem, "tegra.cop-address space");

    memory_region_add_and_init_ram(cop_sysmem, "tegra.cop-ivectors",
                                   0x00000000, 0x40, RW);

    memory_region_add_and_init_ram(cop_sysmem, "tegra.cop-hi-vec",
                                   0xffff0000, SZ_64K, RW);

    cop_memory_region_add_alias(cop_sysmem, "tegra.cop-DRAM", sysmem,
                                0x00000040,
                                0x00000040, TEGRA_DRAM_SIZE - 0x40);

    cop_memory_region_add_alias(cop_sysmem, "tegra.cop-IRAM", sysmem,
                                TEGRA_IRAM_BASE,
                                TEGRA_IRAM_BASE, TEGRA_IRAM_SIZE);

    cop_memory_region_add_alias(cop_sysmem, "tegra.cop-GRHOST", sysmem,
                                TEGRA_GRHOST_BASE,
                                TEGRA_GRHOST_BASE, TEGRA_GRHOST_SIZE);

    cop_memory_region_add_alias(cop_sysmem, "tegra.cop-HOST1X", sysmem,
                                TEGRA_HOST1X_BASE,
                                TEGRA_HOST1X_BASE, TEGRA_HOST1X_SIZE);

    cop_memory_region_add_alias(cop_sysmem, "tegra.cop-GART", sysmem,
                                TEGRA_GART_BASE,
                                TEGRA_GART_BASE, TEGRA_GART_SIZE);

    cop_memory_region_add_alias(cop_sysmem, "tegra.cop-PPSB", sysmem,
                                0x60000000,
                                0x60000000, SZ_256M);

    cop_memory_region_add_alias(cop_sysmem, "tegra.cop-APB", sysmem,
                                0x70000000,
                                0x70000000, SZ_256M);

    cop_memory_region_add_alias(cop_sysmem, "tegra.cop-DRAM UC", sysmem,
                                0x80000000,
                                0x00000000, TEGRA_DRAM_SIZE);

    cop_memory_region_add_alias(cop_sysmem, "tegra.cop-AHB", sysmem,
                                0xC0000000,
                                0xC0000000, SZ_128M + SZ_1K + SZ_512);

    cop_memory_region_add_alias(cop_sysmem, "tegra.cop-IROM", sysmem,
                                BOOTROM_BASE,
                                BOOTROM_BASE, 0xC000);

    cop_memory_region_add_alias(cop_sysmem, "tegra.cop-bootmon", sysmem,
                                BOOTMON_BASE,
                                BOOTMON_BASE, TARGET_PAGE_SIZE);

    cop_memory_region_add_alias(cop_sysmem, "tegra.cop-mmu", sysmem,
                                0xF0000000,
                                0xF0000000, SZ_64K);

    /* Map 0x2F600000-0x1F600000 to remote device.  */
//     sysbus_create_simple("tegra.remote_mem", 0x2F600000, NULL);
//     cop_memory_region_add_alias(cop_sysmem, "tegra.cop-remote_mem", sysmem,
//                                 0x2F600000,
//                                 0x2F600000, 0x10000000);

    cs = qemu_get_cpu(TEGRA2_COP);
    cs->as = cop_as;

    /* Override default AS.  */
    memory_listener_unregister(&cs->cpu_ases[0].tcg_as_listener);
    cs->cpu_ases[0].as = cop_as;
    memory_listener_register(&cs->cpu_ases[0].tcg_as_listener, cop_as);

    load_memory_images(machine);

    tegra_cpu_reset_init();
}

static void tegra2_reset(void)
{
//     remote_io_init("10.1.1.3:45312");
    tegra_trace_init();
    qemu_devices_reset();

    tegra_cpu_reset_deassert(TEGRA2_COP, 1);
}

static QemuOptsList qemu_tegra_opts = {
    .name = "tegra",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qemu_tegra_opts.head),
    .desc = {
        {
            .name = "bootloader",
            .type = QEMU_OPT_STRING,
        },
        {
            .name = "iram",
            .type = QEMU_OPT_STRING,
        },
        { /* end of list */ }
    }
};

static void tegra2_register_config(void)
{
    qemu_add_opts(&qemu_tegra_opts);
}

opts_init(tegra2_register_config);

static void tegra2_machine_init(MachineClass *mc)
{
    mc->desc = "ARM NVIDIA Tegra2";
    mc->init = tegra2_init;
    mc->reset = tegra2_reset;
    mc->default_cpus = TEGRA2_NCPUS;
    mc->min_cpus = TEGRA2_NCPUS;
    mc->max_cpus = TEGRA2_NCPUS;
    mc->ignore_memory_transaction_failures = true;
}

DEFINE_MACHINE("tegra2-alpha", tegra2_machine_init)
