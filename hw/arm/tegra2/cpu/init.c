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

#include "qemu/config-file.h"
#include "hw/arm/arm.h"
#include "hw/cpu/a9mpcore.h"
#include "hw/loader.h"
#include "sysemu/sysemu.h"
#include "exec/address-spaces.h"
#include "exec/helper-proto.h"

#include "devices.h"
#include "iomap.h"
#include "tegra_cpu.h"
#include "tegra_trace.h"

#include "tegra_cpu_priv.h"

#define BOOTLOADER_BASE 0x108000
#define BOOTROM_BASE    0xFFF00000

#define JMP_FIXUP   (sizeof(tegra_bootrom) / 4 - 2)

static uint32_t tegra_bootmon[] = {
    0xe3a00206, /* mov     r0, #1610612736 ; 0x60000000 */
    0xe5901000, /* ldr     r1, [r0] */
    0xe59f0054, /* ldr     r0, [pc, #84]   ; 0x64 */
    0xe1500001, /* cmp     r0, r1 */
    0x1a00000f, /* bne     0x54 */
    0xee100fb0, /* mrc     15, 0, r0, cr0, cr0, {5} */
    0xe200000f, /* and     r0, r0, #15 */
    0xe3500000, /* cmp     r0, #0 */
    0x0a00000b, /* beq     0x54 */
    0xe59f303c, /* ldr     r3, [pc, #60]   ; 0x68 */
    0xe5930000, /* ldr     r0, [r3] */
    0xe3500001, /* cmp     r0, #1 */
    0x1a000007, /* bne     0x54 */
    0xe59f2030, /* ldr     r2, [pc, #48]   ; 0x6c */
    0xe3a01001, /* mov     r1, #1 */
    0xe5821000, /* str     r1, [r2] */
    0xe3a010ff, /* mov     r1, #255,  */
    0xe5821004, /* str     r1, [r2, #4] */
    0xf57ff04f, /* dsb     sy */
    0xe3a01001, /* mov     r1, #1 */
    0xe5831000, /* str     r1, [r3] */
    0xe59f0014, /* ldr     r0, [pc, #20]   ; 0x70 */
    0xe5900000, /* ldr     r0, [r0] */
    0xe12fff10, /* bx      r0 */
    0x00000000, /* andeq   r0, r0, r1 */
    0x55555555, /* ldrbpl  r5, [r5, #-1365], */
    0x00000060, /* andeq   r0, r0, r0, rrx */
    0x50041000, /* andpl   r1, r4, r0 */
    0x6000f000, /* andvs   pc, r0, r0*/
};

static uint32_t tegra_bootrom[] = {
    0xea000006, /* b       0x20 */
    0xea000007, /* b       0x28 */
    0xea000008, /* b       0x30 */
    0xea000009, /* b       0x38 */
    0xea00000a, /* b       0x40 */
    0xea00000b, /* b       0x48 */
    0xea00000c, /* b       0x50 */
    0xea00000d, /* b       0x58 */
    0xe59f0044, /* ldr     r0, [pc, #68]   ; 0x6c */
    0xe12fff10, /* bx      r0 */
    0xe3a01001, /* mov     r1, #1 */
    0xea00000a, /* b       0x5c */
    0xe3a01002, /* mov     r1, #2 */
    0xea000008, /* b       0x5c */
    0xe3a01003, /* mov     r1, #3 */
    0xea000006, /* b       0x5c */
    0xe3a01004, /* mov     r1, #4 */
    0xea000004, /* b       0x5c */
    0xe3a01005, /* mov     r1, #5 */
    0xea000002, /* b       0x5c */
    0xe3a01006, /* mov     r1, #6 */
    0xea000000, /* b       0x5c */
    0xe3a01007, /* mov     r1, #7 */
    0xe3a02010, /* mov     r2, #16 */
    0xe59f0008, /* ldr     r0, [pc, #8]    ; 0x70 */
    0xe5802000, /* str     r2, [r0] */
    0xeafffffb, /* b       0x5c */
    0xffffffff,
    TEGRA_PMC_BASE,
};

static void tegra_do_cpu_reset(void *opaque)
{
    CPUState *cs = opaque;
    int cpu_id = cs->cpu_index;

    assert(cpu_id < TEGRA2_NCPUS);

    tegra_cpu_reset_assert(cpu_id);
}

void tegra_boot(MachineState *machine)
{
    QemuOpts *opts = qemu_find_opts_singleton("tegra");
    const char *bootloader_path = qemu_opt_get(opts, "bootloader");
    const char *iram_path = qemu_opt_get(opts, "iram");
    const char *dtb_path = qemu_opt_get(qemu_get_machine_opts(), "dtb");
    CPUState *cs;
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
                       0xf0010000);

    if (machine->kernel_filename != NULL) {
        tmp = load_image_targphys(machine->kernel_filename, 0x1000000,
                                  machine->ram_size);
        assert(tmp > 0);

        if (dtb_path != NULL)
            assert(load_image_targphys(dtb_path, 0x1000000 + tmp,
                                       machine->ram_size) > 0);
    }

    tegra_cpu_reset_init();

    CPU_FOREACH(cs)
        qemu_register_reset(tegra_do_cpu_reset, cs);
}
