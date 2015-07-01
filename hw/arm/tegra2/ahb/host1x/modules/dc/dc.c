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

#include <math.h>

#include "hw/sysbus.h"
#include "ui/console.h"
#include "framebuffer.h"

#include "host1x_syncpts.h"

#include "dc.h"
#include "iomap.h"
#include "tegra_trace.h"

#define TYPE_TEGRA_DC "tegra.dc"
#define TEGRA_DC(obj) OBJECT_CHECK(tegra_dc, (obj), TYPE_TEGRA_DC)
#define DEFINE_REG32(reg) reg##_t reg
#define WR_MASKED(r, d, m)  r = (r & ~m##_WRMASK) | (d & m##_WRMASK)

#define WIN_A   1
#define WIN_B   2
#define WIN_C   4

#define WIN_IDX ((s->cmd_display_window_header.reg32 & 0x70) >> 4)

#define WIN_BC_GET_SET(p1, p2, val, is_set)                                 \
    {                                                                       \
        switch (WIN_IDX) {                                                  \
        case WIN_B:                                                         \
            if (is_set)                                                     \
                s->win_b_assemly.p1.reg32 = val;                            \
            else                                                            \
                val = s->win_b_assemly.p1.reg32;                            \
            break;                                                          \
        case WIN_C:                                                         \
            if (is_set)                                                     \
                s->win_c_assemly.p2.reg32 = val;                            \
            else                                                            \
                val = s->win_c_assemly.p2.reg32;                            \
            break;                                                          \
        default:                                                            \
            break;                                                          \
        }                                                                   \
    }

#define WIN_BC_GET(p1, p2, val) WIN_BC_GET_SET(p1, p2, val, 0)
#define WIN_BC_SET(p1, p2, val) WIN_BC_GET_SET(p1, p2, val, 1)

#define TRACE_WIN_BC_WRITE(p1, p2, val)                                     \
    {                                                                       \
        int64_t trace_value = -1;                                           \
                                                                            \
        WIN_BC_GET(p1, p2, trace_value);                                    \
                                                                            \
        if (trace_value != -1)                                              \
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,    \
                        trace_value, value);                                \
    }

#define WIN_ABC_GET_SET(p1, p2, p3, val, is_set)                            \
    {                                                                       \
        switch (WIN_IDX) {                                                  \
        case WIN_A:                                                         \
            if (is_set)                                                     \
                s->win_a_assemly.p1.reg32 = val;                            \
            else                                                            \
                val = s->win_a_assemly.p1.reg32;                            \
            break;                                                          \
        case WIN_B:                                                         \
            if (is_set)                                                     \
                s->win_b_assemly.p2.reg32 = val;                            \
            else                                                            \
                val = s->win_b_assemly.p2.reg32;                            \
            break;                                                          \
        case WIN_C:                                                         \
            if (is_set)                                                     \
                s->win_c_assemly.p3.reg32 = val;                            \
            else                                                            \
                val = s->win_c_assemly.p3.reg32;                            \
            break;                                                          \
        default:                                                            \
            break;                                                          \
        }                                                                   \
    }

#define WIN_ABC_GET(p1, p2, p3, val) WIN_ABC_GET_SET(p1, p2, p3, val, 0)
#define WIN_ABC_SET(p1, p2, p3, val) WIN_ABC_GET_SET(p1, p2, p3, val, 1)

#define TRACE_WIN_ABC_WRITE(p1, p2, p3, val)                                \
    {                                                                       \
        int64_t trace_value = -1;                                           \
                                                                            \
        WIN_ABC_GET(p1, p2, p3, trace_value);                               \
                                                                            \
        if (trace_value != -1)                                              \
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,    \
                        trace_value, value);                                \
    }

static int tegra_dc_to_pixman(int format)
{
    switch (format) {
    case 0: // P1
        break;
        g_assert_not_reached();
    case 1: // P2
        g_assert_not_reached();
    case 2: // P4
        g_assert_not_reached();
    case 3: // P8
        g_assert_not_reached();
    case 4: // B4G4R4A4
        return PIXMAN_a4b4g4r4;
    case 5: // B5G5R5A
        return PIXMAN_a1b5g5r5;
    case 6: // B5G6R5
        return PIXMAN_b5g6r5;
    case 7: // AB5G5R5
        return PIXMAN_a1b5g5r5;
    case 12: // B8G8R8A8
        return PIXMAN_a8r8g8b8;
    case 13: // R8G8B8A8
        return PIXMAN_a8b8g8r8;
    case 14: // B6x2G6x2R6x2A8
        return PIXMAN_a8b8g8r8;
    case 15: // R6x2G6x2B6x2A8
        return PIXMAN_x14r6g6b6;
    case 16: // YCbCr422
        g_assert_not_reached();
    case 17: // YUV422
        g_assert_not_reached();
    case 18: // YCbCr420P
        g_assert_not_reached();
    case 19: // YUV420P
        g_assert_not_reached();
    case 20: // YCbCr422P
        g_assert_not_reached();
    case 21: // YUV422P
        g_assert_not_reached();
    case 22: // YCbCr422R
        g_assert_not_reached();
    case 23: // YUV422R
        g_assert_not_reached();
    case 24: // YCbCr422RA
        g_assert_not_reached();
    case 25: // YUV422RA
        g_assert_not_reached();
    }

    return PIXMAN_a8b8g8r8;
}

typedef struct tegra_dc_win_a_state {
    DEFINE_REG32(winc_a_color_palette);
    DEFINE_REG32(winc_a_palette_color_ext);
    DEFINE_REG32(win_a_win_options);
    DEFINE_REG32(win_a_byte_swap);
    DEFINE_REG32(win_a_buffer_control);
    DEFINE_REG32(win_a_color_depth);
    DEFINE_REG32(win_a_position);
    DEFINE_REG32(win_a_size);
    DEFINE_REG32(win_a_prescaled_size);
    DEFINE_REG32(win_a_h_initial_dda);
    DEFINE_REG32(win_a_v_initial_dda);
    DEFINE_REG32(win_a_dda_increment);
    DEFINE_REG32(win_a_line_stride);
    DEFINE_REG32(win_a_buf_stride);
    DEFINE_REG32(win_a_buffer_addr_mode);
    DEFINE_REG32(win_a_dv_control);
    DEFINE_REG32(win_a_blend_nokey);
    DEFINE_REG32(win_a_blend_1win);
    DEFINE_REG32(win_a_blend_2win_b);
    DEFINE_REG32(win_a_blend_2win_c);
    DEFINE_REG32(win_a_blend_3win_bc);
    DEFINE_REG32(win_a_hp_fetch_control);
    DEFINE_REG32(winbuf_a_start_addr);
    DEFINE_REG32(winbuf_a_start_addr_ns);
    DEFINE_REG32(winbuf_a_addr_h_offset);
    DEFINE_REG32(winbuf_a_addr_h_offset_ns);
    DEFINE_REG32(winbuf_a_addr_v_offset);
    DEFINE_REG32(winbuf_a_addr_v_offset_ns);
    DEFINE_REG32(winbuf_a_uflow_status);
} tegra_dc_win_a;

typedef struct tegra_dc_win_b_state {
    DEFINE_REG32(winc_b_color_palette);
    DEFINE_REG32(winc_b_palette_color_ext);
    DEFINE_REG32(winc_b_h_filter_p00);
    DEFINE_REG32(winc_b_h_filter_p01);
    DEFINE_REG32(winc_b_h_filter_p02);
    DEFINE_REG32(winc_b_h_filter_p03);
    DEFINE_REG32(winc_b_h_filter_p04);
    DEFINE_REG32(winc_b_h_filter_p05);
    DEFINE_REG32(winc_b_h_filter_p06);
    DEFINE_REG32(winc_b_h_filter_p07);
    DEFINE_REG32(winc_b_h_filter_p08);
    DEFINE_REG32(winc_b_h_filter_p09);
    DEFINE_REG32(winc_b_h_filter_p0a);
    DEFINE_REG32(winc_b_h_filter_p0b);
    DEFINE_REG32(winc_b_h_filter_p0c);
    DEFINE_REG32(winc_b_h_filter_p0d);
    DEFINE_REG32(winc_b_h_filter_p0e);
    DEFINE_REG32(winc_b_h_filter_p0f);
    DEFINE_REG32(winc_b_csc_yof);
    DEFINE_REG32(winc_b_csc_kyrgb);
    DEFINE_REG32(winc_b_csc_kur);
    DEFINE_REG32(winc_b_csc_kvr);
    DEFINE_REG32(winc_b_csc_kug);
    DEFINE_REG32(winc_b_csc_kvg);
    DEFINE_REG32(winc_b_csc_kub);
    DEFINE_REG32(winc_b_csc_kvb);
    DEFINE_REG32(winc_b_v_filter_p00);
    DEFINE_REG32(winc_b_v_filter_p01);
    DEFINE_REG32(winc_b_v_filter_p02);
    DEFINE_REG32(winc_b_v_filter_p03);
    DEFINE_REG32(winc_b_v_filter_p04);
    DEFINE_REG32(winc_b_v_filter_p05);
    DEFINE_REG32(winc_b_v_filter_p06);
    DEFINE_REG32(winc_b_v_filter_p07);
    DEFINE_REG32(winc_b_v_filter_p08);
    DEFINE_REG32(winc_b_v_filter_p09);
    DEFINE_REG32(winc_b_v_filter_p0a);
    DEFINE_REG32(winc_b_v_filter_p0b);
    DEFINE_REG32(winc_b_v_filter_p0c);
    DEFINE_REG32(winc_b_v_filter_p0d);
    DEFINE_REG32(winc_b_v_filter_p0e);
    DEFINE_REG32(winc_b_v_filter_p0f);
    DEFINE_REG32(win_b_win_options);
    DEFINE_REG32(win_b_byte_swap);
    DEFINE_REG32(win_b_buffer_control);
    DEFINE_REG32(win_b_color_depth);
    DEFINE_REG32(win_b_position);
    DEFINE_REG32(win_b_size);
    DEFINE_REG32(win_b_prescaled_size);
    DEFINE_REG32(win_b_h_initial_dda);
    DEFINE_REG32(win_b_v_initial_dda);
    DEFINE_REG32(win_b_dda_increment);
    DEFINE_REG32(win_b_line_stride);
    DEFINE_REG32(win_b_buf_stride);
    DEFINE_REG32(win_b_uv_buf_stride);
    DEFINE_REG32(win_b_buffer_addr_mode);
    DEFINE_REG32(win_b_dv_control);
    DEFINE_REG32(win_b_blend_nokey);
    DEFINE_REG32(win_b_blend_1win);
    DEFINE_REG32(win_b_blend_2win_a);
    DEFINE_REG32(win_b_blend_2win_c);
    DEFINE_REG32(win_b_blend_3win_ac);
    DEFINE_REG32(win_b_hp_fetch_control);
    DEFINE_REG32(winbuf_b_start_addr);
    DEFINE_REG32(winbuf_b_start_addr_ns);
    DEFINE_REG32(winbuf_b_start_addr_u);
    DEFINE_REG32(winbuf_b_start_addr_u_ns);
    DEFINE_REG32(winbuf_b_start_addr_v);
    DEFINE_REG32(winbuf_b_start_addr_v_ns);
    DEFINE_REG32(winbuf_b_addr_h_offset);
    DEFINE_REG32(winbuf_b_addr_h_offset_ns);
    DEFINE_REG32(winbuf_b_addr_v_offset);
    DEFINE_REG32(winbuf_b_addr_v_offset_ns);
    DEFINE_REG32(winbuf_b_uflow_status);
    DEFINE_REG32(winc_b_color_palette_1);
    DEFINE_REG32(winc_b_color_palette_2);
    DEFINE_REG32(winc_b_color_palette_3);
    DEFINE_REG32(winc_b_color_palette_4);
    DEFINE_REG32(winc_b_color_palette_5);
    DEFINE_REG32(winc_b_color_palette_6);
    DEFINE_REG32(winc_b_color_palette_7);
    DEFINE_REG32(winc_b_color_palette_8);
    DEFINE_REG32(winc_b_color_palette_9);
} tegra_dc_win_b;

typedef struct tegra_dc_win_c_state {
    DEFINE_REG32(winc_c_color_palette);
    DEFINE_REG32(winc_c_palette_color_ext);
    DEFINE_REG32(winc_c_h_filter_p00);
    DEFINE_REG32(winc_c_h_filter_p01);
    DEFINE_REG32(winc_c_h_filter_p02);
    DEFINE_REG32(winc_c_h_filter_p03);
    DEFINE_REG32(winc_c_h_filter_p04);
    DEFINE_REG32(winc_c_h_filter_p05);
    DEFINE_REG32(winc_c_h_filter_p06);
    DEFINE_REG32(winc_c_h_filter_p07);
    DEFINE_REG32(winc_c_h_filter_p08);
    DEFINE_REG32(winc_c_h_filter_p09);
    DEFINE_REG32(winc_c_h_filter_p0a);
    DEFINE_REG32(winc_c_h_filter_p0b);
    DEFINE_REG32(winc_c_h_filter_p0c);
    DEFINE_REG32(winc_c_h_filter_p0d);
    DEFINE_REG32(winc_c_h_filter_p0e);
    DEFINE_REG32(winc_c_h_filter_p0f);
    DEFINE_REG32(winc_c_csc_yof);
    DEFINE_REG32(winc_c_csc_kyrgb);
    DEFINE_REG32(winc_c_csc_kur);
    DEFINE_REG32(winc_c_csc_kvr);
    DEFINE_REG32(winc_c_csc_kug);
    DEFINE_REG32(winc_c_csc_kvg);
    DEFINE_REG32(winc_c_csc_kub);
    DEFINE_REG32(winc_c_csc_kvb);
    DEFINE_REG32(win_c_win_options);
    DEFINE_REG32(win_c_byte_swap);
    DEFINE_REG32(win_c_buffer_control);
    DEFINE_REG32(win_c_color_depth);
    DEFINE_REG32(win_c_position);
    DEFINE_REG32(win_c_size);
    DEFINE_REG32(win_c_prescaled_size);
    DEFINE_REG32(win_c_h_initial_dda);
    DEFINE_REG32(win_c_v_initial_dda);
    DEFINE_REG32(win_c_dda_increment);
    DEFINE_REG32(win_c_line_stride);
    DEFINE_REG32(win_c_buf_stride);
    DEFINE_REG32(win_c_uv_buf_stride);
    DEFINE_REG32(win_c_buffer_addr_mode);
    DEFINE_REG32(win_c_dv_control);
    DEFINE_REG32(win_c_blend_nokey);
    DEFINE_REG32(win_c_blend_1win);
    DEFINE_REG32(win_c_blend_2win_a);
    DEFINE_REG32(win_c_blend_2win_b);
    DEFINE_REG32(win_c_blend_3win_ab);
    DEFINE_REG32(win_c_hp_fetch_control);
    DEFINE_REG32(winbuf_c_start_addr);
    DEFINE_REG32(winbuf_c_start_addr_ns);
    DEFINE_REG32(winbuf_c_start_addr_u);
    DEFINE_REG32(winbuf_c_start_addr_u_ns);
    DEFINE_REG32(winbuf_c_start_addr_v);
    DEFINE_REG32(winbuf_c_start_addr_v_ns);
    DEFINE_REG32(winbuf_c_addr_h_offset);
    DEFINE_REG32(winbuf_c_addr_h_offset_ns);
    DEFINE_REG32(winbuf_c_addr_v_offset);
    DEFINE_REG32(winbuf_c_addr_v_offset_ns);
    DEFINE_REG32(winbuf_c_uflow_status);
} tegra_dc_win_c;

typedef struct tegra_dc_state {
    SysBusDevice parent_obj;

    DisplaySurface *win_a_surf;
    DisplaySurface *win_b_surf;
    DisplaySurface *win_c_surf;

    QemuConsole *con;
    qemu_irq irq;

    uint32_t disp_width;
    uint32_t disp_height;

    MemoryRegion iomem;
    DEFINE_REG32(cmd_general_incr_syncpt);
    DEFINE_REG32(cmd_general_incr_syncpt_cntrl);
    DEFINE_REG32(cmd_general_incr_syncpt_error);
    DEFINE_REG32(cmd_win_a_incr_syncpt);
    DEFINE_REG32(cmd_win_a_incr_syncpt_cntrl);
    DEFINE_REG32(cmd_win_a_incr_syncpt_error);
    DEFINE_REG32(cmd_win_b_incr_syncpt);
    DEFINE_REG32(cmd_win_b_incr_syncpt_cntrl);
    DEFINE_REG32(cmd_win_b_incr_syncpt_error);
    DEFINE_REG32(cmd_win_c_incr_syncpt);
    DEFINE_REG32(cmd_win_c_incr_syncpt_cntrl);
    DEFINE_REG32(cmd_win_c_incr_syncpt_error);
    DEFINE_REG32(cmd_cont_syncpt_vsync);
    DEFINE_REG32(cmd_ctxsw);
    DEFINE_REG32(cmd_display_command_option0);
    DEFINE_REG32(cmd_display_command);
    DEFINE_REG32(cmd_signal_raise);
    DEFINE_REG32(cmd_display_power_control);
    DEFINE_REG32(cmd_int_status);
    DEFINE_REG32(cmd_int_mask);
    DEFINE_REG32(cmd_int_enable);
    DEFINE_REG32(cmd_int_type);
    DEFINE_REG32(cmd_int_polarity);
    DEFINE_REG32(cmd_signal_raise1);
    DEFINE_REG32(cmd_signal_raise2);
    DEFINE_REG32(cmd_signal_raise3);
    DEFINE_REG32(cmd_state_access);
    DEFINE_REG32(cmd_state_control);
    DEFINE_REG32(cmd_display_window_header);
    DEFINE_REG32(cmd_reg_act_control);
    DEFINE_REG32(com_crc_control);
    DEFINE_REG32(com_crc_checksum);
    DEFINE_REG32(com_pin_output_enable0);
    DEFINE_REG32(com_pin_output_enable1);
    DEFINE_REG32(com_pin_output_enable2);
    DEFINE_REG32(com_pin_output_enable3);
    DEFINE_REG32(com_pin_output_polarity0);
    DEFINE_REG32(com_pin_output_polarity1);
    DEFINE_REG32(com_pin_output_polarity2);
    DEFINE_REG32(com_pin_output_polarity3);
    DEFINE_REG32(com_pin_output_data0);
    DEFINE_REG32(com_pin_output_data1);
    DEFINE_REG32(com_pin_output_data2);
    DEFINE_REG32(com_pin_output_data3);
    DEFINE_REG32(com_pin_input_enable0);
    DEFINE_REG32(com_pin_input_enable1);
    DEFINE_REG32(com_pin_input_enable2);
    DEFINE_REG32(com_pin_input_enable3);
    DEFINE_REG32(com_pin_input_data0);
    DEFINE_REG32(com_pin_input_data1);
    DEFINE_REG32(com_pin_output_select0);
    DEFINE_REG32(com_pin_output_select1);
    DEFINE_REG32(com_pin_output_select2);
    DEFINE_REG32(com_pin_output_select3);
    DEFINE_REG32(com_pin_output_select4);
    DEFINE_REG32(com_pin_output_select5);
    DEFINE_REG32(com_pin_output_select6);
    DEFINE_REG32(com_pin_misc_control);
    DEFINE_REG32(com_pm0_control);
    DEFINE_REG32(com_pm0_duty_cycle);
    DEFINE_REG32(com_pm1_control);
    DEFINE_REG32(com_pm1_duty_cycle);
    DEFINE_REG32(com_spi_control);
    DEFINE_REG32(com_spi_start_byte);
    DEFINE_REG32(com_hspi_write_data_ab);
    DEFINE_REG32(com_hspi_write_data_cd);
    DEFINE_REG32(com_hspi_cs_dc);
    DEFINE_REG32(com_scratch_register_a);
    DEFINE_REG32(com_scratch_register_b);
    DEFINE_REG32(com_gpio_ctrl);
    DEFINE_REG32(com_gpio_debounce_counter);
    DEFINE_REG32(com_crc_checksum_latched);
    DEFINE_REG32(disp_disp_signal_options0);
    DEFINE_REG32(disp_disp_signal_options1);
    DEFINE_REG32(disp_disp_win_options);
    DEFINE_REG32(disp_mem_high_priority);
    DEFINE_REG32(disp_mem_high_priority_timer);
    DEFINE_REG32(disp_disp_timing_options);
    DEFINE_REG32(disp_ref_to_sync);
    DEFINE_REG32(disp_sync_width);
    DEFINE_REG32(disp_back_porch);
    DEFINE_REG32(disp_disp_active);
    DEFINE_REG32(disp_front_porch);
    DEFINE_REG32(disp_h_pulse0_control);
    DEFINE_REG32(disp_h_pulse0_position_a);
    DEFINE_REG32(disp_h_pulse0_position_b);
    DEFINE_REG32(disp_h_pulse0_position_c);
    DEFINE_REG32(disp_h_pulse0_position_d);
    DEFINE_REG32(disp_h_pulse1_control);
    DEFINE_REG32(disp_h_pulse1_position_a);
    DEFINE_REG32(disp_h_pulse1_position_b);
    DEFINE_REG32(disp_h_pulse1_position_c);
    DEFINE_REG32(disp_h_pulse1_position_d);
    DEFINE_REG32(disp_h_pulse2_control);
    DEFINE_REG32(disp_h_pulse2_position_a);
    DEFINE_REG32(disp_h_pulse2_position_b);
    DEFINE_REG32(disp_h_pulse2_position_c);
    DEFINE_REG32(disp_h_pulse2_position_d);
    DEFINE_REG32(disp_v_pulse0_control);
    DEFINE_REG32(disp_v_pulse0_position_a);
    DEFINE_REG32(disp_v_pulse0_position_b);
    DEFINE_REG32(disp_v_pulse0_position_c);
    DEFINE_REG32(disp_v_pulse1_control);
    DEFINE_REG32(disp_v_pulse1_position_a);
    DEFINE_REG32(disp_v_pulse1_position_b);
    DEFINE_REG32(disp_v_pulse1_position_c);
    DEFINE_REG32(disp_v_pulse2_control);
    DEFINE_REG32(disp_v_pulse2_position_a);
    DEFINE_REG32(disp_v_pulse3_control);
    DEFINE_REG32(disp_v_pulse3_position_a);
    DEFINE_REG32(disp_m0_control);
    DEFINE_REG32(disp_m1_control);
    DEFINE_REG32(disp_di_control);
    DEFINE_REG32(disp_pp_control);
    DEFINE_REG32(disp_pp_select_a);
    DEFINE_REG32(disp_pp_select_b);
    DEFINE_REG32(disp_pp_select_c);
    DEFINE_REG32(disp_pp_select_d);
    DEFINE_REG32(disp_disp_clock_control);
    DEFINE_REG32(disp_disp_interface_control);
    DEFINE_REG32(disp_disp_color_control);
    DEFINE_REG32(disp_shift_clock_options);
    DEFINE_REG32(disp_data_enable_options);
    DEFINE_REG32(disp_serial_interface_options);
    DEFINE_REG32(disp_lcd_spi_options);
    DEFINE_REG32(disp_border_color);
    DEFINE_REG32(disp_color_key0_lower);
    DEFINE_REG32(disp_color_key0_upper);
    DEFINE_REG32(disp_color_key1_lower);
    DEFINE_REG32(disp_color_key1_upper);
    DEFINE_REG32(disp_cursor_foreground);
    DEFINE_REG32(disp_cursor_background);
    DEFINE_REG32(disp_cursor_start_addr);
    DEFINE_REG32(disp_cursor_start_addr_ns);
    DEFINE_REG32(disp_cursor_position);
    DEFINE_REG32(disp_cursor_position_ns);
    DEFINE_REG32(disp_init_seq_control);
    DEFINE_REG32(disp_spi_init_seq_data_a);
    DEFINE_REG32(disp_spi_init_seq_data_b);
    DEFINE_REG32(disp_spi_init_seq_data_c);
    DEFINE_REG32(disp_spi_init_seq_data_d);
    DEFINE_REG32(disp_dc_mccif_fifoctrl);
    DEFINE_REG32(disp_mccif_display0a_hyst);
    DEFINE_REG32(disp_mccif_display0b_hyst);
    DEFINE_REG32(disp_mccif_display0c_hyst);
    DEFINE_REG32(disp_mccif_display1b_hyst);
    DEFINE_REG32(disp_dac_crt_ctrl);
    DEFINE_REG32(disp_disp_misc_control);
    DEFINE_REG32(disp_sd_control);
    DEFINE_REG32(disp_sd_csc_coeff);
    DEFINE_REG32(disp_sd_lut);
    DEFINE_REG32(disp_sd_lut_1);
    DEFINE_REG32(disp_sd_lut_2);
    DEFINE_REG32(disp_sd_lut_3);
    DEFINE_REG32(disp_sd_lut_4);
    DEFINE_REG32(disp_sd_lut_5);
    DEFINE_REG32(disp_sd_lut_6);
    DEFINE_REG32(disp_sd_lut_7);
    DEFINE_REG32(disp_sd_lut_8);
    DEFINE_REG32(disp_sd_flicker_control);
    DEFINE_REG32(disp_sd_pixel_count);
    DEFINE_REG32(disp_sd_histogram);
    DEFINE_REG32(disp_sd_histogram_1);
    DEFINE_REG32(disp_sd_histogram_2);
    DEFINE_REG32(disp_sd_histogram_3);
    DEFINE_REG32(disp_sd_histogram_4);
    DEFINE_REG32(disp_sd_histogram_5);
    DEFINE_REG32(disp_sd_histogram_6);
    DEFINE_REG32(disp_sd_histogram_7);
    DEFINE_REG32(disp_sd_bl_parameters);
    DEFINE_REG32(disp_sd_bl_tf);
    DEFINE_REG32(disp_sd_bl_tf_1);
    DEFINE_REG32(disp_sd_bl_tf_2);
    DEFINE_REG32(disp_sd_bl_tf_3);
    DEFINE_REG32(disp_sd_bl_control);

    tegra_dc_win_a win_a_assemly, win_a_armed;
    tegra_dc_win_b win_b_assemly, win_b_armed;
    tegra_dc_win_c win_c_assemly, win_c_armed;
} tegra_dc;

static const VMStateDescription vmstate_tegra_dc_win_a = {
    .name = "tegra-dc-win-a",
    .version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(winc_a_color_palette.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(winc_a_palette_color_ext.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_win_options.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_byte_swap.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_buffer_control.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_color_depth.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_position.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_size.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_prescaled_size.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_h_initial_dda.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_v_initial_dda.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_dda_increment.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_line_stride.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_buf_stride.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_buffer_addr_mode.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_dv_control.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_blend_nokey.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_blend_1win.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_blend_2win_b.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_blend_2win_c.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_blend_3win_bc.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(win_a_hp_fetch_control.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(winbuf_a_start_addr.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(winbuf_a_start_addr_ns.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(winbuf_a_addr_h_offset.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(winbuf_a_addr_h_offset_ns.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(winbuf_a_addr_v_offset.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(winbuf_a_addr_v_offset_ns.reg32, tegra_dc_win_a),
        VMSTATE_UINT32(winbuf_a_uflow_status.reg32, tegra_dc_win_a),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_tegra_dc_win_b = {
    .name = "tegra-dc-win-b",
    .version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(winc_b_color_palette.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_palette_color_ext.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_h_filter_p00.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_h_filter_p01.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_h_filter_p02.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_h_filter_p03.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_h_filter_p04.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_h_filter_p05.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_h_filter_p06.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_h_filter_p07.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_h_filter_p08.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_h_filter_p09.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_h_filter_p0a.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_h_filter_p0b.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_h_filter_p0c.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_h_filter_p0d.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_h_filter_p0e.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_h_filter_p0f.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_csc_yof.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_csc_kyrgb.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_csc_kur.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_csc_kvr.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_csc_kug.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_csc_kvg.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_csc_kub.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_csc_kvb.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_v_filter_p00.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_v_filter_p01.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_v_filter_p02.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_v_filter_p03.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_v_filter_p04.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_v_filter_p05.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_v_filter_p06.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_v_filter_p07.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_v_filter_p08.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_v_filter_p09.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_v_filter_p0a.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_v_filter_p0b.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_v_filter_p0c.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_v_filter_p0d.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_v_filter_p0e.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_v_filter_p0f.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_win_options.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_byte_swap.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_buffer_control.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_color_depth.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_position.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_size.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_prescaled_size.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_h_initial_dda.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_v_initial_dda.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_dda_increment.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_line_stride.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_buf_stride.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_uv_buf_stride.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_buffer_addr_mode.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_dv_control.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_blend_nokey.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_blend_1win.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_blend_2win_a.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_blend_2win_c.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_blend_3win_ac.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(win_b_hp_fetch_control.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winbuf_b_start_addr.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winbuf_b_start_addr_ns.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winbuf_b_start_addr_u.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winbuf_b_start_addr_u_ns.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winbuf_b_start_addr_v.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winbuf_b_start_addr_v_ns.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winbuf_b_addr_h_offset.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winbuf_b_addr_h_offset_ns.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winbuf_b_addr_v_offset.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winbuf_b_addr_v_offset_ns.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winbuf_b_uflow_status.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_color_palette_1.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_color_palette_2.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_color_palette_3.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_color_palette_4.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_color_palette_5.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_color_palette_6.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_color_palette_7.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_color_palette_8.reg32, tegra_dc_win_b),
        VMSTATE_UINT32(winc_b_color_palette_9.reg32, tegra_dc_win_b),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_tegra_dc_win_c = {
    .name = "tegra-dc-win-c",
    .version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(winc_c_color_palette.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_palette_color_ext.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_h_filter_p00.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_h_filter_p01.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_h_filter_p02.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_h_filter_p03.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_h_filter_p04.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_h_filter_p05.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_h_filter_p06.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_h_filter_p07.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_h_filter_p08.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_h_filter_p09.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_h_filter_p0a.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_h_filter_p0b.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_h_filter_p0c.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_h_filter_p0d.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_h_filter_p0e.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_h_filter_p0f.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_csc_yof.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_csc_kyrgb.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_csc_kur.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_csc_kvr.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_csc_kug.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_csc_kvg.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_csc_kub.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winc_c_csc_kvb.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_win_options.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_byte_swap.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_buffer_control.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_color_depth.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_position.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_size.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_prescaled_size.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_h_initial_dda.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_v_initial_dda.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_dda_increment.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_line_stride.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_buf_stride.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_uv_buf_stride.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_buffer_addr_mode.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_dv_control.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_blend_nokey.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_blend_1win.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_blend_2win_a.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_blend_2win_b.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_blend_3win_ab.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(win_c_hp_fetch_control.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winbuf_c_start_addr.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winbuf_c_start_addr_ns.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winbuf_c_start_addr_u.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winbuf_c_start_addr_u_ns.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winbuf_c_start_addr_v.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winbuf_c_start_addr_v_ns.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winbuf_c_addr_h_offset.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winbuf_c_addr_h_offset_ns.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winbuf_c_addr_v_offset.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winbuf_c_addr_v_offset_ns.reg32, tegra_dc_win_c),
        VMSTATE_UINT32(winbuf_c_uflow_status.reg32, tegra_dc_win_c),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_tegra_dc = {
    .name = "tegra.dc",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(cmd_general_incr_syncpt.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_general_incr_syncpt_cntrl.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_general_incr_syncpt_error.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_win_a_incr_syncpt.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_win_a_incr_syncpt_cntrl.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_win_a_incr_syncpt_error.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_win_b_incr_syncpt.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_win_b_incr_syncpt_cntrl.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_win_b_incr_syncpt_error.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_win_c_incr_syncpt.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_win_c_incr_syncpt_cntrl.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_win_c_incr_syncpt_error.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_cont_syncpt_vsync.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_ctxsw.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_display_command_option0.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_display_command.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_signal_raise.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_display_power_control.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_int_status.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_int_mask.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_int_enable.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_int_type.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_int_polarity.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_signal_raise1.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_signal_raise2.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_signal_raise3.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_state_access.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_state_control.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_display_window_header.reg32, tegra_dc),
        VMSTATE_UINT32(cmd_reg_act_control.reg32, tegra_dc),
        VMSTATE_UINT32(com_crc_control.reg32, tegra_dc),
        VMSTATE_UINT32(com_crc_checksum.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_enable0.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_enable1.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_enable2.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_enable3.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_polarity0.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_polarity1.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_polarity2.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_polarity3.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_data0.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_data1.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_data2.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_data3.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_input_enable0.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_input_enable1.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_input_enable2.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_input_enable3.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_input_data0.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_input_data1.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_select0.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_select1.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_select2.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_select3.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_select4.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_select5.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_output_select6.reg32, tegra_dc),
        VMSTATE_UINT32(com_pin_misc_control.reg32, tegra_dc),
        VMSTATE_UINT32(com_pm0_control.reg32, tegra_dc),
        VMSTATE_UINT32(com_pm0_duty_cycle.reg32, tegra_dc),
        VMSTATE_UINT32(com_pm1_control.reg32, tegra_dc),
        VMSTATE_UINT32(com_pm1_duty_cycle.reg32, tegra_dc),
        VMSTATE_UINT32(com_spi_control.reg32, tegra_dc),
        VMSTATE_UINT32(com_spi_start_byte.reg32, tegra_dc),
        VMSTATE_UINT32(com_hspi_write_data_ab.reg32, tegra_dc),
        VMSTATE_UINT32(com_hspi_write_data_cd.reg32, tegra_dc),
        VMSTATE_UINT32(com_hspi_cs_dc.reg32, tegra_dc),
        VMSTATE_UINT32(com_scratch_register_a.reg32, tegra_dc),
        VMSTATE_UINT32(com_scratch_register_b.reg32, tegra_dc),
        VMSTATE_UINT32(com_gpio_ctrl.reg32, tegra_dc),
        VMSTATE_UINT32(com_gpio_debounce_counter.reg32, tegra_dc),
        VMSTATE_UINT32(com_crc_checksum_latched.reg32, tegra_dc),
        VMSTATE_UINT32(disp_disp_signal_options0.reg32, tegra_dc),
        VMSTATE_UINT32(disp_disp_signal_options1.reg32, tegra_dc),
        VMSTATE_UINT32(disp_disp_win_options.reg32, tegra_dc),
        VMSTATE_UINT32(disp_mem_high_priority.reg32, tegra_dc),
        VMSTATE_UINT32(disp_mem_high_priority_timer.reg32, tegra_dc),
        VMSTATE_UINT32(disp_disp_timing_options.reg32, tegra_dc),
        VMSTATE_UINT32(disp_ref_to_sync.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sync_width.reg32, tegra_dc),
        VMSTATE_UINT32(disp_back_porch.reg32, tegra_dc),
        VMSTATE_UINT32(disp_disp_active.reg32, tegra_dc),
        VMSTATE_UINT32(disp_front_porch.reg32, tegra_dc),
        VMSTATE_UINT32(disp_h_pulse0_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_h_pulse0_position_a.reg32, tegra_dc),
        VMSTATE_UINT32(disp_h_pulse0_position_b.reg32, tegra_dc),
        VMSTATE_UINT32(disp_h_pulse0_position_c.reg32, tegra_dc),
        VMSTATE_UINT32(disp_h_pulse0_position_d.reg32, tegra_dc),
        VMSTATE_UINT32(disp_h_pulse1_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_h_pulse1_position_a.reg32, tegra_dc),
        VMSTATE_UINT32(disp_h_pulse1_position_b.reg32, tegra_dc),
        VMSTATE_UINT32(disp_h_pulse1_position_c.reg32, tegra_dc),
        VMSTATE_UINT32(disp_h_pulse1_position_d.reg32, tegra_dc),
        VMSTATE_UINT32(disp_h_pulse2_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_h_pulse2_position_a.reg32, tegra_dc),
        VMSTATE_UINT32(disp_h_pulse2_position_b.reg32, tegra_dc),
        VMSTATE_UINT32(disp_h_pulse2_position_c.reg32, tegra_dc),
        VMSTATE_UINT32(disp_h_pulse2_position_d.reg32, tegra_dc),
        VMSTATE_UINT32(disp_v_pulse0_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_v_pulse0_position_a.reg32, tegra_dc),
        VMSTATE_UINT32(disp_v_pulse0_position_b.reg32, tegra_dc),
        VMSTATE_UINT32(disp_v_pulse0_position_c.reg32, tegra_dc),
        VMSTATE_UINT32(disp_v_pulse1_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_v_pulse1_position_a.reg32, tegra_dc),
        VMSTATE_UINT32(disp_v_pulse1_position_b.reg32, tegra_dc),
        VMSTATE_UINT32(disp_v_pulse1_position_c.reg32, tegra_dc),
        VMSTATE_UINT32(disp_v_pulse2_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_v_pulse2_position_a.reg32, tegra_dc),
        VMSTATE_UINT32(disp_v_pulse3_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_v_pulse3_position_a.reg32, tegra_dc),
        VMSTATE_UINT32(disp_m0_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_m1_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_di_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_pp_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_pp_select_a.reg32, tegra_dc),
        VMSTATE_UINT32(disp_pp_select_b.reg32, tegra_dc),
        VMSTATE_UINT32(disp_pp_select_c.reg32, tegra_dc),
        VMSTATE_UINT32(disp_pp_select_d.reg32, tegra_dc),
        VMSTATE_UINT32(disp_disp_clock_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_disp_interface_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_disp_color_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_shift_clock_options.reg32, tegra_dc),
        VMSTATE_UINT32(disp_data_enable_options.reg32, tegra_dc),
        VMSTATE_UINT32(disp_serial_interface_options.reg32, tegra_dc),
        VMSTATE_UINT32(disp_lcd_spi_options.reg32, tegra_dc),
        VMSTATE_UINT32(disp_border_color.reg32, tegra_dc),
        VMSTATE_UINT32(disp_color_key0_lower.reg32, tegra_dc),
        VMSTATE_UINT32(disp_color_key0_upper.reg32, tegra_dc),
        VMSTATE_UINT32(disp_color_key1_lower.reg32, tegra_dc),
        VMSTATE_UINT32(disp_color_key1_upper.reg32, tegra_dc),
        VMSTATE_UINT32(disp_cursor_foreground.reg32, tegra_dc),
        VMSTATE_UINT32(disp_cursor_background.reg32, tegra_dc),
        VMSTATE_UINT32(disp_cursor_start_addr.reg32, tegra_dc),
        VMSTATE_UINT32(disp_cursor_start_addr_ns.reg32, tegra_dc),
        VMSTATE_UINT32(disp_cursor_position.reg32, tegra_dc),
        VMSTATE_UINT32(disp_cursor_position_ns.reg32, tegra_dc),
        VMSTATE_UINT32(disp_init_seq_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_spi_init_seq_data_a.reg32, tegra_dc),
        VMSTATE_UINT32(disp_spi_init_seq_data_b.reg32, tegra_dc),
        VMSTATE_UINT32(disp_spi_init_seq_data_c.reg32, tegra_dc),
        VMSTATE_UINT32(disp_spi_init_seq_data_d.reg32, tegra_dc),
        VMSTATE_UINT32(disp_dc_mccif_fifoctrl.reg32, tegra_dc),
        VMSTATE_UINT32(disp_mccif_display0a_hyst.reg32, tegra_dc),
        VMSTATE_UINT32(disp_mccif_display0b_hyst.reg32, tegra_dc),
        VMSTATE_UINT32(disp_mccif_display0c_hyst.reg32, tegra_dc),
        VMSTATE_UINT32(disp_mccif_display1b_hyst.reg32, tegra_dc),
        VMSTATE_UINT32(disp_dac_crt_ctrl.reg32, tegra_dc),
        VMSTATE_UINT32(disp_disp_misc_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_csc_coeff.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_lut.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_lut_1.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_lut_2.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_lut_3.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_lut_4.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_lut_5.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_lut_6.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_lut_7.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_lut_8.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_flicker_control.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_pixel_count.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_histogram.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_histogram_1.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_histogram_2.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_histogram_3.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_histogram_4.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_histogram_5.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_histogram_6.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_histogram_7.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_bl_parameters.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_bl_tf.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_bl_tf_1.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_bl_tf_2.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_bl_tf_3.reg32, tegra_dc),
        VMSTATE_UINT32(disp_sd_bl_control.reg32, tegra_dc),

        VMSTATE_STRUCT(win_a_armed, tegra_dc, 0, vmstate_tegra_dc_win_a,
                       tegra_dc_win_a),
        VMSTATE_STRUCT(win_b_armed, tegra_dc, 0, vmstate_tegra_dc_win_b,
                       tegra_dc_win_b),
        VMSTATE_STRUCT(win_c_armed, tegra_dc, 0, vmstate_tegra_dc_win_c,
                       tegra_dc_win_c),

        VMSTATE_STRUCT(win_a_assemly, tegra_dc, 0, vmstate_tegra_dc_win_a,
                       tegra_dc_win_a),
        VMSTATE_STRUCT(win_b_assemly, tegra_dc, 0, vmstate_tegra_dc_win_b,
                       tegra_dc_win_b),
        VMSTATE_STRUCT(win_c_assemly, tegra_dc, 0, vmstate_tegra_dc_win_c,
                       tegra_dc_win_c),

        VMSTATE_END_OF_LIST()
    }
};

static uint64_t tegra_dc_priv_read(void *opaque, hwaddr offset,
                                   unsigned size)
{
    tegra_dc *s = opaque;
    uint64_t ret = 0;

    assert(size == 4);

    offset >>= 2;

    switch (offset) {
    case CMD_GENERAL_INCR_SYNCPT_OFFSET:
        ret = s->cmd_general_incr_syncpt.reg32;
        break;
    case CMD_GENERAL_INCR_SYNCPT_CNTRL_OFFSET:
        ret = s->cmd_general_incr_syncpt_cntrl.reg32;
        break;
    case CMD_GENERAL_INCR_SYNCPT_ERROR_OFFSET:
        ret = s->cmd_general_incr_syncpt_error.reg32;
        break;
    case CMD_WIN_A_INCR_SYNCPT_OFFSET:
        ret = s->cmd_win_a_incr_syncpt.reg32;
        break;
    case CMD_WIN_A_INCR_SYNCPT_CNTRL_OFFSET:
        ret = s->cmd_win_a_incr_syncpt_cntrl.reg32;
        break;
    case CMD_WIN_A_INCR_SYNCPT_ERROR_OFFSET:
        ret = s->cmd_win_a_incr_syncpt_error.reg32;
        break;
    case CMD_WIN_B_INCR_SYNCPT_OFFSET:
        ret = s->cmd_win_b_incr_syncpt.reg32;
        break;
    case CMD_WIN_B_INCR_SYNCPT_CNTRL_OFFSET:
        ret = s->cmd_win_b_incr_syncpt_cntrl.reg32;
        break;
    case CMD_WIN_B_INCR_SYNCPT_ERROR_OFFSET:
        ret = s->cmd_win_b_incr_syncpt_error.reg32;
        break;
    case CMD_WIN_C_INCR_SYNCPT_OFFSET:
        ret = s->cmd_win_c_incr_syncpt.reg32;
        break;
    case CMD_WIN_C_INCR_SYNCPT_CNTRL_OFFSET:
        ret = s->cmd_win_c_incr_syncpt_cntrl.reg32;
        break;
    case CMD_WIN_C_INCR_SYNCPT_ERROR_OFFSET:
        ret = s->cmd_win_c_incr_syncpt_error.reg32;
        break;
    case CMD_CONT_SYNCPT_VSYNC_OFFSET:
        ret = s->cmd_cont_syncpt_vsync.reg32;
        break;
    case CMD_CTXSW_OFFSET:
        ret = s->cmd_ctxsw.reg32;
        break;
    case CMD_DISPLAY_COMMAND_OPTION0_OFFSET:
        ret = s->cmd_display_command_option0.reg32;
        break;
    case CMD_DISPLAY_COMMAND_OFFSET:
        ret = s->cmd_display_command.reg32;
        break;
    case CMD_SIGNAL_RAISE_OFFSET:
        ret = s->cmd_signal_raise.reg32;
        break;
    case CMD_DISPLAY_POWER_CONTROL_OFFSET:
        ret = s->cmd_display_power_control.reg32;
        break;
    case CMD_INT_STATUS_OFFSET:
        ret = s->cmd_int_status.reg32;
        break;
    case CMD_INT_MASK_OFFSET:
        ret = s->cmd_int_mask.reg32;
        break;
    case CMD_INT_ENABLE_OFFSET:
        ret = s->cmd_int_enable.reg32;
        break;
    case CMD_INT_TYPE_OFFSET:
        ret = s->cmd_int_type.reg32;
        break;
    case CMD_INT_POLARITY_OFFSET:
        ret = s->cmd_int_polarity.reg32;
        break;
    case CMD_SIGNAL_RAISE1_OFFSET:
        ret = s->cmd_signal_raise1.reg32;
        break;
    case CMD_SIGNAL_RAISE2_OFFSET:
        ret = s->cmd_signal_raise2.reg32;
        break;
    case CMD_SIGNAL_RAISE3_OFFSET:
        ret = s->cmd_signal_raise3.reg32;
        break;
    case CMD_STATE_ACCESS_OFFSET:
        ret = s->cmd_state_access.reg32;
        break;
    case CMD_STATE_CONTROL_OFFSET:
        ret = s->cmd_state_control.reg32;
        break;
    case CMD_DISPLAY_WINDOW_HEADER_OFFSET:
        ret = s->cmd_display_window_header.reg32;
        break;
    case CMD_REG_ACT_CONTROL_OFFSET:
        ret = s->cmd_reg_act_control.reg32;
        break;
    case COM_CRC_CONTROL_OFFSET:
        ret = s->com_crc_control.reg32;
        break;
    case COM_CRC_CHECKSUM_OFFSET:
        ret = s->com_crc_checksum.reg32;
        break;
    case COM_PIN_OUTPUT_ENABLE0_OFFSET:
        ret = s->com_pin_output_enable0.reg32;
        break;
    case COM_PIN_OUTPUT_ENABLE1_OFFSET:
        ret = s->com_pin_output_enable1.reg32;
        break;
    case COM_PIN_OUTPUT_ENABLE2_OFFSET:
        ret = s->com_pin_output_enable2.reg32;
        break;
    case COM_PIN_OUTPUT_ENABLE3_OFFSET:
        ret = s->com_pin_output_enable3.reg32;
        break;
    case COM_PIN_OUTPUT_POLARITY0_OFFSET:
        ret = s->com_pin_output_polarity0.reg32;
        break;
    case COM_PIN_OUTPUT_POLARITY1_OFFSET:
        ret = s->com_pin_output_polarity1.reg32;
        break;
    case COM_PIN_OUTPUT_POLARITY2_OFFSET:
        ret = s->com_pin_output_polarity2.reg32;
        break;
    case COM_PIN_OUTPUT_POLARITY3_OFFSET:
        ret = s->com_pin_output_polarity3.reg32;
        break;
    case COM_PIN_OUTPUT_DATA0_OFFSET:
        ret = s->com_pin_output_data0.reg32;
        break;
    case COM_PIN_OUTPUT_DATA1_OFFSET:
        ret = s->com_pin_output_data1.reg32;
        break;
    case COM_PIN_OUTPUT_DATA2_OFFSET:
        ret = s->com_pin_output_data2.reg32;
        break;
    case COM_PIN_OUTPUT_DATA3_OFFSET:
        ret = s->com_pin_output_data3.reg32;
        break;
    case COM_PIN_INPUT_ENABLE0_OFFSET:
        ret = s->com_pin_input_enable0.reg32;
        break;
    case COM_PIN_INPUT_ENABLE1_OFFSET:
        ret = s->com_pin_input_enable1.reg32;
        break;
    case COM_PIN_INPUT_ENABLE2_OFFSET:
        ret = s->com_pin_input_enable2.reg32;
        break;
    case COM_PIN_INPUT_ENABLE3_OFFSET:
        ret = s->com_pin_input_enable3.reg32;
        break;
    case COM_PIN_INPUT_DATA0_OFFSET:
        ret = s->com_pin_input_data0.reg32;
        break;
    case COM_PIN_INPUT_DATA1_OFFSET:
        ret = s->com_pin_input_data1.reg32;
        break;
    case COM_PIN_OUTPUT_SELECT0_OFFSET:
        ret = s->com_pin_output_select0.reg32;
        break;
    case COM_PIN_OUTPUT_SELECT1_OFFSET:
        ret = s->com_pin_output_select1.reg32;
        break;
    case COM_PIN_OUTPUT_SELECT2_OFFSET:
        ret = s->com_pin_output_select2.reg32;
        break;
    case COM_PIN_OUTPUT_SELECT3_OFFSET:
        ret = s->com_pin_output_select3.reg32;
        break;
    case COM_PIN_OUTPUT_SELECT4_OFFSET:
        ret = s->com_pin_output_select4.reg32;
        break;
    case COM_PIN_OUTPUT_SELECT5_OFFSET:
        ret = s->com_pin_output_select5.reg32;
        break;
    case COM_PIN_OUTPUT_SELECT6_OFFSET:
        ret = s->com_pin_output_select6.reg32;
        break;
    case COM_PIN_MISC_CONTROL_OFFSET:
        ret = s->com_pin_misc_control.reg32;
        break;
    case COM_PM0_CONTROL_OFFSET:
        ret = s->com_pm0_control.reg32;
        break;
    case COM_PM0_DUTY_CYCLE_OFFSET:
        ret = s->com_pm0_duty_cycle.reg32;
        break;
    case COM_PM1_CONTROL_OFFSET:
        ret = s->com_pm1_control.reg32;
        break;
    case COM_PM1_DUTY_CYCLE_OFFSET:
        ret = s->com_pm1_duty_cycle.reg32;
        break;
    case COM_SPI_CONTROL_OFFSET:
        ret = s->com_spi_control.reg32;
        break;
    case COM_SPI_START_BYTE_OFFSET:
        ret = s->com_spi_start_byte.reg32;
        break;
    case COM_HSPI_WRITE_DATA_AB_OFFSET:
        ret = s->com_hspi_write_data_ab.reg32;
        break;
    case COM_HSPI_WRITE_DATA_CD_OFFSET:
        ret = s->com_hspi_write_data_cd.reg32;
        break;
    case COM_HSPI_CS_DC_OFFSET:
        ret = s->com_hspi_cs_dc.reg32;
        break;
    case COM_SCRATCH_REGISTER_A_OFFSET:
        ret = s->com_scratch_register_a.reg32;
        break;
    case COM_SCRATCH_REGISTER_B_OFFSET:
        ret = s->com_scratch_register_b.reg32;
        break;
    case COM_GPIO_CTRL_OFFSET:
        ret = s->com_gpio_ctrl.reg32;
        break;
    case COM_GPIO_DEBOUNCE_COUNTER_OFFSET:
        ret = s->com_gpio_debounce_counter.reg32;
        break;
    case COM_CRC_CHECKSUM_LATCHED_OFFSET:
        ret = s->com_crc_checksum_latched.reg32;
        break;
    case DISP_DISP_SIGNAL_OPTIONS0_OFFSET:
        ret = s->disp_disp_signal_options0.reg32;
        break;
    case DISP_DISP_SIGNAL_OPTIONS1_OFFSET:
        ret = s->disp_disp_signal_options1.reg32;
        break;
    case DISP_DISP_WIN_OPTIONS_OFFSET:
        ret = s->disp_disp_win_options.reg32;
        break;
    case DISP_MEM_HIGH_PRIORITY_OFFSET:
        ret = s->disp_mem_high_priority.reg32;
        break;
    case DISP_MEM_HIGH_PRIORITY_TIMER_OFFSET:
        ret = s->disp_mem_high_priority_timer.reg32;
        break;
    case DISP_DISP_TIMING_OPTIONS_OFFSET:
        ret = s->disp_disp_timing_options.reg32;
        break;
    case DISP_REF_TO_SYNC_OFFSET:
        ret = s->disp_ref_to_sync.reg32;
        break;
    case DISP_SYNC_WIDTH_OFFSET:
        ret = s->disp_sync_width.reg32;
        break;
    case DISP_BACK_PORCH_OFFSET:
        ret = s->disp_back_porch.reg32;
        break;
    case DISP_DISP_ACTIVE_OFFSET:
        ret = s->disp_disp_active.reg32;
        break;
    case DISP_FRONT_PORCH_OFFSET:
        ret = s->disp_front_porch.reg32;
        break;
    case DISP_H_PULSE0_CONTROL_OFFSET:
        ret = s->disp_h_pulse0_control.reg32;
        break;
    case DISP_H_PULSE0_POSITION_A_OFFSET:
        ret = s->disp_h_pulse0_position_a.reg32;
        break;
    case DISP_H_PULSE0_POSITION_B_OFFSET:
        ret = s->disp_h_pulse0_position_b.reg32;
        break;
    case DISP_H_PULSE0_POSITION_C_OFFSET:
        ret = s->disp_h_pulse0_position_c.reg32;
        break;
    case DISP_H_PULSE0_POSITION_D_OFFSET:
        ret = s->disp_h_pulse0_position_d.reg32;
        break;
    case DISP_H_PULSE1_CONTROL_OFFSET:
        ret = s->disp_h_pulse1_control.reg32;
        break;
    case DISP_H_PULSE1_POSITION_A_OFFSET:
        ret = s->disp_h_pulse1_position_a.reg32;
        break;
    case DISP_H_PULSE1_POSITION_B_OFFSET:
        ret = s->disp_h_pulse1_position_b.reg32;
        break;
    case DISP_H_PULSE1_POSITION_C_OFFSET:
        ret = s->disp_h_pulse1_position_c.reg32;
        break;
    case DISP_H_PULSE1_POSITION_D_OFFSET:
        ret = s->disp_h_pulse1_position_d.reg32;
        break;
    case DISP_H_PULSE2_CONTROL_OFFSET:
        ret = s->disp_h_pulse2_control.reg32;
        break;
    case DISP_H_PULSE2_POSITION_A_OFFSET:
        ret = s->disp_h_pulse2_position_a.reg32;
        break;
    case DISP_H_PULSE2_POSITION_B_OFFSET:
        ret = s->disp_h_pulse2_position_b.reg32;
        break;
    case DISP_H_PULSE2_POSITION_C_OFFSET:
        ret = s->disp_h_pulse2_position_c.reg32;
        break;
    case DISP_H_PULSE2_POSITION_D_OFFSET:
        ret = s->disp_h_pulse2_position_d.reg32;
        break;
    case DISP_V_PULSE0_CONTROL_OFFSET:
        ret = s->disp_v_pulse0_control.reg32;
        break;
    case DISP_V_PULSE0_POSITION_A_OFFSET:
        ret = s->disp_v_pulse0_position_a.reg32;
        break;
    case DISP_V_PULSE0_POSITION_B_OFFSET:
        ret = s->disp_v_pulse0_position_b.reg32;
        break;
    case DISP_V_PULSE0_POSITION_C_OFFSET:
        ret = s->disp_v_pulse0_position_c.reg32;
        break;
    case DISP_V_PULSE1_CONTROL_OFFSET:
        ret = s->disp_v_pulse1_control.reg32;
        break;
    case DISP_V_PULSE1_POSITION_A_OFFSET:
        ret = s->disp_v_pulse1_position_a.reg32;
        break;
    case DISP_V_PULSE1_POSITION_B_OFFSET:
        ret = s->disp_v_pulse1_position_b.reg32;
        break;
    case DISP_V_PULSE1_POSITION_C_OFFSET:
        ret = s->disp_v_pulse1_position_c.reg32;
        break;
    case DISP_V_PULSE2_CONTROL_OFFSET:
        ret = s->disp_v_pulse2_control.reg32;
        break;
    case DISP_V_PULSE2_POSITION_A_OFFSET:
        ret = s->disp_v_pulse2_position_a.reg32;
        break;
    case DISP_V_PULSE3_CONTROL_OFFSET:
        ret = s->disp_v_pulse3_control.reg32;
        break;
    case DISP_V_PULSE3_POSITION_A_OFFSET:
        ret = s->disp_v_pulse3_position_a.reg32;
        break;
    case DISP_M0_CONTROL_OFFSET:
        ret = s->disp_m0_control.reg32;
        break;
    case DISP_M1_CONTROL_OFFSET:
        ret = s->disp_m1_control.reg32;
        break;
    case DISP_DI_CONTROL_OFFSET:
        ret = s->disp_di_control.reg32;
        break;
    case DISP_PP_CONTROL_OFFSET:
        ret = s->disp_pp_control.reg32;
        break;
    case DISP_PP_SELECT_A_OFFSET:
        ret = s->disp_pp_select_a.reg32;
        break;
    case DISP_PP_SELECT_B_OFFSET:
        ret = s->disp_pp_select_b.reg32;
        break;
    case DISP_PP_SELECT_C_OFFSET:
        ret = s->disp_pp_select_c.reg32;
        break;
    case DISP_PP_SELECT_D_OFFSET:
        ret = s->disp_pp_select_d.reg32;
        break;
    case DISP_DISP_CLOCK_CONTROL_OFFSET:
        ret = s->disp_disp_clock_control.reg32;
        break;
    case DISP_DISP_INTERFACE_CONTROL_OFFSET:
        ret = s->disp_disp_interface_control.reg32;
        break;
    case DISP_DISP_COLOR_CONTROL_OFFSET:
        ret = s->disp_disp_color_control.reg32;
        break;
    case DISP_SHIFT_CLOCK_OPTIONS_OFFSET:
        ret = s->disp_shift_clock_options.reg32;
        break;
    case DISP_DATA_ENABLE_OPTIONS_OFFSET:
        ret = s->disp_data_enable_options.reg32;
        break;
    case DISP_SERIAL_INTERFACE_OPTIONS_OFFSET:
        ret = s->disp_serial_interface_options.reg32;
        break;
    case DISP_LCD_SPI_OPTIONS_OFFSET:
        ret = s->disp_lcd_spi_options.reg32;
        break;
    case DISP_BORDER_COLOR_OFFSET:
        ret = s->disp_border_color.reg32;
        break;
    case DISP_COLOR_KEY0_LOWER_OFFSET:
        ret = s->disp_color_key0_lower.reg32;
        break;
    case DISP_COLOR_KEY0_UPPER_OFFSET:
        ret = s->disp_color_key0_upper.reg32;
        break;
    case DISP_COLOR_KEY1_LOWER_OFFSET:
        ret = s->disp_color_key1_lower.reg32;
        break;
    case DISP_COLOR_KEY1_UPPER_OFFSET:
        ret = s->disp_color_key1_upper.reg32;
        break;
    case DISP_CURSOR_FOREGROUND_OFFSET:
        ret = s->disp_cursor_foreground.reg32;
        break;
    case DISP_CURSOR_BACKGROUND_OFFSET:
        ret = s->disp_cursor_background.reg32;
        break;
    case DISP_CURSOR_START_ADDR_OFFSET:
        ret = s->disp_cursor_start_addr.reg32;
        break;
    case DISP_CURSOR_START_ADDR_NS_OFFSET:
        ret = s->disp_cursor_start_addr_ns.reg32;
        break;
    case DISP_CURSOR_POSITION_OFFSET:
        ret = s->disp_cursor_position.reg32;
        break;
    case DISP_CURSOR_POSITION_NS_OFFSET:
        ret = s->disp_cursor_position_ns.reg32;
        break;
    case DISP_INIT_SEQ_CONTROL_OFFSET:
        ret = s->disp_init_seq_control.reg32;
        break;
    case DISP_SPI_INIT_SEQ_DATA_A_OFFSET:
        ret = s->disp_spi_init_seq_data_a.reg32;
        break;
    case DISP_SPI_INIT_SEQ_DATA_B_OFFSET:
        ret = s->disp_spi_init_seq_data_b.reg32;
        break;
    case DISP_SPI_INIT_SEQ_DATA_C_OFFSET:
        ret = s->disp_spi_init_seq_data_c.reg32;
        break;
    case DISP_SPI_INIT_SEQ_DATA_D_OFFSET:
        ret = s->disp_spi_init_seq_data_d.reg32;
        break;
    case DISP_DC_MCCIF_FIFOCTRL_OFFSET:
        ret = s->disp_dc_mccif_fifoctrl.reg32;
        break;
    case DISP_MCCIF_DISPLAY0A_HYST_OFFSET:
        ret = s->disp_mccif_display0a_hyst.reg32;
        break;
    case DISP_MCCIF_DISPLAY0B_HYST_OFFSET:
        ret = s->disp_mccif_display0b_hyst.reg32;
        break;
    case DISP_MCCIF_DISPLAY0C_HYST_OFFSET:
        ret = s->disp_mccif_display0c_hyst.reg32;
        break;
    case DISP_MCCIF_DISPLAY1B_HYST_OFFSET:
        ret = s->disp_mccif_display1b_hyst.reg32;
        break;
    case DISP_DAC_CRT_CTRL_OFFSET:
        ret = s->disp_dac_crt_ctrl.reg32;
        break;
    case DISP_DISP_MISC_CONTROL_OFFSET:
        ret = s->disp_disp_misc_control.reg32;
        break;
    case WINC_COLOR_PALETTE_OFFSET:
        WIN_ABC_GET(winc_a_color_palette, winc_b_color_palette,
                    winc_c_color_palette, ret);
        break;
    case WINC_PALETTE_COLOR_EXT_OFFSET:
        WIN_ABC_GET(winc_a_palette_color_ext, winc_b_palette_color_ext,
                    winc_c_palette_color_ext, ret);
        break;
    case WINC_H_FILTER_P00_OFFSET:
        WIN_BC_GET(winc_b_h_filter_p00, winc_c_h_filter_p00, ret);
        break;
    case WINC_H_FILTER_P01_OFFSET:
        WIN_BC_GET(winc_b_h_filter_p01, winc_c_h_filter_p01, ret);
        break;
    case WINC_H_FILTER_P02_OFFSET:
        WIN_BC_GET(winc_b_h_filter_p02, winc_c_h_filter_p02, ret);
        break;
    case WINC_H_FILTER_P03_OFFSET:
        WIN_BC_GET(winc_b_h_filter_p03, winc_c_h_filter_p03, ret);
        break;
    case WINC_H_FILTER_P04_OFFSET:
        WIN_BC_GET(winc_b_h_filter_p04, winc_c_h_filter_p04, ret);
        break;
    case WINC_H_FILTER_P05_OFFSET:
        WIN_BC_GET(winc_b_h_filter_p05, winc_c_h_filter_p05, ret);
        break;
    case WINC_H_FILTER_P06_OFFSET:
        WIN_BC_GET(winc_b_h_filter_p06, winc_c_h_filter_p06, ret);
        break;
    case WINC_H_FILTER_P07_OFFSET:
        WIN_BC_GET(winc_b_h_filter_p07, winc_c_h_filter_p07, ret);
        break;
    case WINC_H_FILTER_P08_OFFSET:
        WIN_BC_GET(winc_b_h_filter_p08, winc_c_h_filter_p08, ret);
        break;
    case WINC_H_FILTER_P09_OFFSET:
        WIN_BC_GET(winc_b_h_filter_p09, winc_c_h_filter_p09, ret);
        break;
    case WINC_H_FILTER_P0A_OFFSET:
        WIN_BC_GET(winc_b_h_filter_p0a, winc_c_h_filter_p0a, ret);
        break;
    case WINC_H_FILTER_P0B_OFFSET:
        WIN_BC_GET(winc_b_h_filter_p0b, winc_c_h_filter_p0b, ret);
        break;
    case WINC_H_FILTER_P0C_OFFSET:
        WIN_BC_GET(winc_b_h_filter_p0c, winc_c_h_filter_p0c, ret);
        break;
    case WINC_H_FILTER_P0D_OFFSET:
        WIN_BC_GET(winc_b_h_filter_p0d, winc_c_h_filter_p0d, ret);
        break;
    case WINC_H_FILTER_P0E_OFFSET:
        WIN_BC_GET(winc_b_h_filter_p0e, winc_c_h_filter_p0e, ret);
        break;
    case WINC_H_FILTER_P0F_OFFSET:
        WIN_BC_GET(winc_b_h_filter_p0f, winc_c_h_filter_p0f, ret);
        break;
    case WINC_CSC_YOF_OFFSET:
        WIN_BC_GET(winc_b_csc_yof, winc_c_csc_yof, ret);
        break;
    case WINC_CSC_KYRGB_OFFSET:
        WIN_BC_GET(winc_b_csc_kyrgb, winc_c_csc_kyrgb, ret);
        break;
    case WINC_CSC_KUR_OFFSET:
        WIN_BC_GET(winc_b_csc_kur, winc_c_csc_kur, ret);
        break;
    case WINC_CSC_KVR_OFFSET:
        WIN_BC_GET(winc_b_csc_kvr, winc_c_csc_kvr, ret);
        break;
    case WINC_CSC_KUG_OFFSET:
        WIN_BC_GET(winc_b_csc_kug, winc_c_csc_kug, ret);
        break;
    case WINC_CSC_KVG_OFFSET:
        WIN_BC_GET(winc_b_csc_kvg, winc_c_csc_kvg, ret);
        break;
    case WINC_CSC_KUB_OFFSET:
        WIN_BC_GET(winc_b_csc_kub, winc_c_csc_kub, ret);
        break;
    case WINC_CSC_KVB_OFFSET:
        WIN_BC_GET(winc_b_csc_kvb, winc_c_csc_kvb, ret);
        break;
    case WINC_V_FILTER_P00_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_v_filter_p00.reg32;
        break;
    case WINC_V_FILTER_P01_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_v_filter_p01.reg32;
        break;
    case WINC_V_FILTER_P02_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_v_filter_p02.reg32;
        break;
    case WINC_V_FILTER_P03_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_v_filter_p03.reg32;
        break;
    case WINC_V_FILTER_P04_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_v_filter_p04.reg32;
        break;
    case WINC_V_FILTER_P05_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_v_filter_p05.reg32;
        break;
    case WINC_V_FILTER_P06_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_v_filter_p06.reg32;
        break;
    case WINC_V_FILTER_P07_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_v_filter_p07.reg32;
        break;
    case WINC_V_FILTER_P08_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_v_filter_p08.reg32;
        break;
    case WINC_V_FILTER_P09_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_v_filter_p09.reg32;
        break;
    case WINC_V_FILTER_P0A_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_v_filter_p0a.reg32;
        break;
    case WINC_V_FILTER_P0B_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_v_filter_p0b.reg32;
        break;
    case WINC_V_FILTER_P0C_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_v_filter_p0c.reg32;
        break;
    case WINC_V_FILTER_P0D_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_v_filter_p0d.reg32;
        break;
    case WINC_V_FILTER_P0E_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_v_filter_p0e.reg32;
        break;
    case WINC_V_FILTER_P0F_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_v_filter_p0f.reg32;
        break;
    case WIN_WIN_OPTIONS_OFFSET:
        WIN_ABC_GET(win_a_win_options, win_b_win_options,
                    win_c_win_options, ret);
        break;
    case WIN_BYTE_SWAP_OFFSET:
        WIN_ABC_GET(win_a_byte_swap, win_b_byte_swap,
                    win_c_byte_swap, ret);
        break;
    case WIN_BUFFER_CONTROL_OFFSET:
        WIN_ABC_GET(win_a_buffer_control, win_b_buffer_control,
                    win_c_buffer_control, ret);
        break;
    case WIN_COLOR_DEPTH_OFFSET:
        WIN_ABC_GET(win_a_color_depth, win_b_color_depth,
                    win_c_color_depth, ret);
        break;
    case WIN_POSITION_OFFSET:
        WIN_ABC_GET(win_a_position, win_b_position,
                    win_c_position, ret);
        break;
    case WIN_SIZE_OFFSET:
        WIN_ABC_GET(win_a_size, win_b_size,
                    win_c_size, ret);
        break;
    case WIN_PRESCALED_SIZE_OFFSET:
        WIN_ABC_GET(win_a_prescaled_size, win_b_prescaled_size,
                    win_c_prescaled_size, ret);
        break;
    case WIN_H_INITIAL_DDA_OFFSET:
        WIN_ABC_GET(win_a_h_initial_dda, win_b_h_initial_dda,
                    win_c_h_initial_dda, ret);
        break;
    case WIN_V_INITIAL_DDA_OFFSET:
        WIN_ABC_GET(win_a_v_initial_dda, win_b_v_initial_dda,
                    win_c_v_initial_dda, ret);
        break;
    case WIN_DDA_INCREMENT_OFFSET:
        WIN_ABC_GET(win_a_dda_increment, win_b_dda_increment,
                    win_c_dda_increment, ret);
        break;
    case WIN_LINE_STRIDE_OFFSET:
        WIN_ABC_GET(win_a_line_stride, win_b_line_stride,
                    win_c_line_stride, ret);
        break;
    case WIN_BUF_STRIDE_OFFSET:
        WIN_ABC_GET(win_a_buf_stride, win_b_buf_stride,
                    win_c_buf_stride, ret);
        break;
    case WIN_UV_BUF_STRIDE_OFFSET:
        WIN_BC_GET(win_b_uv_buf_stride, win_c_uv_buf_stride, ret);
        break;
    case WIN_BUFFER_ADDR_MODE_OFFSET:
        WIN_ABC_GET(win_a_buffer_addr_mode, win_b_buffer_addr_mode,
                    win_c_buffer_addr_mode, ret);
        break;
    case WIN_DV_CONTROL_OFFSET:
        WIN_ABC_GET(win_a_dv_control, win_b_dv_control,
                    win_c_dv_control, ret);
        break;
    case WIN_BLEND_NOKEY_OFFSET:
        WIN_ABC_GET(win_a_blend_nokey, win_b_blend_nokey,
                    win_c_blend_nokey, ret);
        break;
    case WIN_BLEND_1WIN_OFFSET:
        WIN_ABC_GET(win_a_blend_1win, win_b_blend_1win,
                    win_c_blend_1win, ret);
        break;
    case WIN_BLEND_2WIN_X_OFFSET:
        WIN_ABC_GET(win_a_blend_2win_b, win_b_blend_2win_a,
                    win_c_blend_2win_a, ret);
        break;
    case WIN_BLEND_2WIN_Y_OFFSET:
        WIN_ABC_GET(win_a_blend_2win_c, win_b_blend_2win_c,
                    win_c_blend_2win_b, ret);
        break;
    case WIN_BLEND_3WIN_XY_OFFSET:
        WIN_ABC_GET(win_a_blend_3win_bc, win_b_blend_3win_ac,
                    win_c_blend_3win_ab, ret);
        break;
    case WIN_HP_FETCH_CONTROL_OFFSET:
        WIN_ABC_GET(win_a_hp_fetch_control, win_b_hp_fetch_control,
                    win_c_hp_fetch_control, ret);
        break;
    case WINBUF_START_ADDR_OFFSET:
        WIN_ABC_GET(winbuf_a_start_addr, winbuf_b_start_addr,
                    winbuf_c_start_addr, ret);
        break;
    case WINBUF_START_ADDR_NS_OFFSET:
        WIN_ABC_GET(winbuf_a_start_addr_ns, winbuf_b_start_addr_ns,
                    winbuf_c_start_addr_ns, ret);
        break;
    case WINBUF_START_ADDR_U_OFFSET:
        WIN_BC_GET(winbuf_b_start_addr_u, winbuf_c_start_addr_u, ret);
        break;
    case WINBUF_START_ADDR_U_NS_OFFSET:
        WIN_BC_GET(winbuf_b_start_addr_u_ns, winbuf_c_start_addr_u_ns, ret);
        break;
    case WINBUF_START_ADDR_V_OFFSET:
        WIN_BC_GET(winbuf_b_start_addr_v, winbuf_c_start_addr_v, ret);
        break;
    case WINBUF_START_ADDR_V_NS_OFFSET:
        WIN_BC_GET(winbuf_b_start_addr_v_ns, winbuf_c_start_addr_v_ns, ret);
        break;
    case WINBUF_ADDR_H_OFFSET_OFFSET:
        WIN_ABC_GET(winbuf_a_addr_h_offset, winbuf_b_addr_h_offset,
                    winbuf_c_addr_h_offset, ret);
        break;
    case WINBUF_ADDR_H_OFFSET_NS_OFFSET:
        WIN_ABC_GET(winbuf_a_addr_h_offset_ns, winbuf_b_addr_h_offset_ns,
                    winbuf_c_addr_h_offset_ns, ret);
        break;
    case WINBUF_ADDR_V_OFFSET_OFFSET:
        WIN_ABC_GET(winbuf_a_addr_v_offset, winbuf_b_addr_v_offset,
                    winbuf_c_addr_v_offset, ret);
        break;
    case WINBUF_ADDR_V_OFFSET_NS_OFFSET:
        WIN_ABC_GET(winbuf_a_addr_v_offset_ns, winbuf_b_addr_v_offset_ns,
                    winbuf_c_addr_v_offset_ns, ret);
        break;
    case WINBUF_UFLOW_STATUS_OFFSET:
        WIN_ABC_GET(winbuf_a_uflow_status, winbuf_b_uflow_status,
                    winbuf_c_uflow_status, ret);
        break;
    case DISP_SD_CONTROL_OFFSET:
        ret = s->disp_sd_control.reg32;
        break;
    case DISP_SD_CSC_COEFF_OFFSET:
        ret = s->disp_sd_csc_coeff.reg32;
        break;
    case DISP_SD_LUT_OFFSET:
        ret = s->disp_sd_lut.reg32;
        break;
    case DISP_SD_LUT_1_OFFSET:
        ret = s->disp_sd_lut_1.reg32;
        break;
    case DISP_SD_LUT_2_OFFSET:
        ret = s->disp_sd_lut_2.reg32;
        break;
    case DISP_SD_LUT_3_OFFSET:
        ret = s->disp_sd_lut_3.reg32;
        break;
    case DISP_SD_LUT_4_OFFSET:
        ret = s->disp_sd_lut_4.reg32;
        break;
    case DISP_SD_LUT_5_OFFSET:
        ret = s->disp_sd_lut_5.reg32;
        break;
    case DISP_SD_LUT_6_OFFSET:
        ret = s->disp_sd_lut_6.reg32;
        break;
    case DISP_SD_LUT_7_OFFSET:
        ret = s->disp_sd_lut_7.reg32;
        break;
    case DISP_SD_LUT_8_OFFSET:
        ret = s->disp_sd_lut_8.reg32;
        break;
    case DISP_SD_FLICKER_CONTROL_OFFSET:
        ret = s->disp_sd_flicker_control.reg32;
        break;
    case DISP_SD_PIXEL_COUNT_OFFSET:
        ret = s->disp_sd_pixel_count.reg32;
        break;
    case DISP_SD_HISTOGRAM_OFFSET:
        ret = s->disp_sd_histogram.reg32;
        break;
    case DISP_SD_HISTOGRAM_1_OFFSET:
        ret = s->disp_sd_histogram_1.reg32;
        break;
    case DISP_SD_HISTOGRAM_2_OFFSET:
        ret = s->disp_sd_histogram_2.reg32;
        break;
    case DISP_SD_HISTOGRAM_3_OFFSET:
        ret = s->disp_sd_histogram_3.reg32;
        break;
    case DISP_SD_HISTOGRAM_4_OFFSET:
        ret = s->disp_sd_histogram_4.reg32;
        break;
    case DISP_SD_HISTOGRAM_5_OFFSET:
        ret = s->disp_sd_histogram_5.reg32;
        break;
    case DISP_SD_HISTOGRAM_6_OFFSET:
        ret = s->disp_sd_histogram_6.reg32;
        break;
    case DISP_SD_HISTOGRAM_7_OFFSET:
        ret = s->disp_sd_histogram_7.reg32;
        break;
    case DISP_SD_BL_PARAMETERS_OFFSET:
        ret = s->disp_sd_bl_parameters.reg32;
        break;
    case DISP_SD_BL_TF_OFFSET:
        ret = s->disp_sd_bl_tf.reg32;
        break;
    case DISP_SD_BL_TF_1_OFFSET:
        ret = s->disp_sd_bl_tf_1.reg32;
        break;
    case DISP_SD_BL_TF_2_OFFSET:
        ret = s->disp_sd_bl_tf_2.reg32;
        break;
    case DISP_SD_BL_TF_3_OFFSET:
        ret = s->disp_sd_bl_tf_3.reg32;
        break;
    case DISP_SD_BL_CONTROL_OFFSET:
        ret = s->disp_sd_bl_control.reg32;
        break;
    case WINC_COLOR_PALETTE_1_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_color_palette_1.reg32;
        break;
    case WINC_COLOR_PALETTE_2_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_color_palette_2.reg32;
        break;
    case WINC_COLOR_PALETTE_3_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_color_palette_3.reg32;
        break;
    case WINC_COLOR_PALETTE_4_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_color_palette_4.reg32;
        break;
    case WINC_COLOR_PALETTE_5_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_color_palette_5.reg32;
        break;
    case WINC_COLOR_PALETTE_6_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_color_palette_6.reg32;
        break;
    case WINC_COLOR_PALETTE_7_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_color_palette_7.reg32;
        break;
    case WINC_COLOR_PALETTE_8_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_color_palette_8.reg32;
        break;
    case WINC_COLOR_PALETTE_9_OFFSET:
        if (WIN_IDX == WIN_B)
            ret = s->win_b_assemly.winc_b_color_palette_9.reg32;
        break;
    default:
        break;
    }

    TRACE_READ(s->iomem.addr, offset, ret);

    return ret;
}

static void tegra_dc_priv_write(void *opaque, hwaddr offset,
                                uint64_t value, unsigned size)
{
    tegra_dc *s = opaque;

    assert(size == 4);

    offset >>= 2;

    switch (offset) {
    case CMD_GENERAL_INCR_SYNCPT_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_general_incr_syncpt.reg32, value);
        s->cmd_general_incr_syncpt.reg32 = value;

        host1x_incr_syncpt(s->cmd_general_incr_syncpt.general_indx);
        break;
    case CMD_GENERAL_INCR_SYNCPT_CNTRL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_general_incr_syncpt_cntrl.reg32, value);
        s->cmd_general_incr_syncpt_cntrl.reg32 = value;
        break;
    case CMD_GENERAL_INCR_SYNCPT_ERROR_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_general_incr_syncpt_error.reg32, value);
        s->cmd_general_incr_syncpt_error.reg32 = value;
        break;
    case CMD_WIN_A_INCR_SYNCPT_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_win_a_incr_syncpt.reg32, value);
        s->cmd_win_a_incr_syncpt.reg32 = value;
        break;
    case CMD_WIN_A_INCR_SYNCPT_CNTRL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_win_a_incr_syncpt_cntrl.reg32, value);
        s->cmd_win_a_incr_syncpt_cntrl.reg32 = value;
        break;
    case CMD_WIN_A_INCR_SYNCPT_ERROR_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_win_a_incr_syncpt_error.reg32, value);
        s->cmd_win_a_incr_syncpt_error.reg32 = value;
        break;
    case CMD_WIN_B_INCR_SYNCPT_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_win_b_incr_syncpt.reg32, value);
        s->cmd_win_b_incr_syncpt.reg32 = value;
        break;
    case CMD_WIN_B_INCR_SYNCPT_CNTRL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_win_b_incr_syncpt_cntrl.reg32, value);
        s->cmd_win_b_incr_syncpt_cntrl.reg32 = value;
        break;
    case CMD_WIN_B_INCR_SYNCPT_ERROR_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_win_b_incr_syncpt_error.reg32, value);
        s->cmd_win_b_incr_syncpt_error.reg32 = value;
        break;
    case CMD_WIN_C_INCR_SYNCPT_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_win_c_incr_syncpt.reg32, value);
        s->cmd_win_c_incr_syncpt.reg32 = value;
        break;
    case CMD_WIN_C_INCR_SYNCPT_CNTRL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_win_c_incr_syncpt_cntrl.reg32, value);
        s->cmd_win_c_incr_syncpt_cntrl.reg32 = value;
        break;
    case CMD_WIN_C_INCR_SYNCPT_ERROR_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_win_c_incr_syncpt_error.reg32, value);
        s->cmd_win_c_incr_syncpt_error.reg32 = value;
        break;
    case CMD_CONT_SYNCPT_VSYNC_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_cont_syncpt_vsync.reg32, value);
        s->cmd_cont_syncpt_vsync.reg32 = value;
        break;
    case CMD_CTXSW_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_ctxsw.reg32, value);
        s->cmd_ctxsw.reg32 = value;
        break;
    case CMD_DISPLAY_COMMAND_OPTION0_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_display_command_option0.reg32, value);
        s->cmd_display_command_option0.reg32 = value;
        break;
    case CMD_DISPLAY_COMMAND_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_display_command.reg32, value);
        s->cmd_display_command.reg32 = value;
        break;
    case CMD_SIGNAL_RAISE_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_signal_raise.reg32, value);
        s->cmd_signal_raise.reg32 = value;
        break;
    case CMD_DISPLAY_POWER_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_display_power_control.reg32, value);
        s->cmd_display_power_control.reg32 = value;
        break;
    case CMD_INT_STATUS_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_int_status.reg32, value);
        s->cmd_int_status.reg32 &= (~value & 0x1DFF9F);

//         if (!s->cmd_int_status.reg32)
//             TRACE_IRQ_LOWER(s->iomem.addr, s->irq);
        break;
    case CMD_INT_MASK_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_int_mask.reg32, value);
        s->cmd_int_mask.reg32 = value;
        break;
    case CMD_INT_ENABLE_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_int_enable.reg32, value);
        s->cmd_int_enable.reg32 = value;
        break;
    case CMD_INT_TYPE_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_int_type.reg32, value);
        s->cmd_int_type.reg32 = value;
        break;
    case CMD_INT_POLARITY_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_int_polarity.reg32, value);
        s->cmd_int_polarity.reg32 = value;
        break;
    case CMD_SIGNAL_RAISE1_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_signal_raise1.reg32, value);
        s->cmd_signal_raise1.reg32 = value;
        break;
    case CMD_SIGNAL_RAISE2_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_signal_raise2.reg32, value);
        s->cmd_signal_raise2.reg32 = value;
        break;
    case CMD_SIGNAL_RAISE3_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_signal_raise3.reg32, value);
        s->cmd_signal_raise3.reg32 = value;
        break;
    case CMD_STATE_ACCESS_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_state_access.reg32, value);
        s->cmd_state_access.reg32 = value;
        break;
    case CMD_STATE_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_state_control.reg32, value);
        s->cmd_state_control.reg32 = value;

//         if (!s->cmd_state_control.general_update)
//             break;

        if (s->cmd_state_control.win_a_update) {
            s->win_a_armed = s->win_a_assemly;

            qemu_free_displaysurface(s->win_a_surf);

            s->win_a_surf = qemu_create_displaysurface_guestmem(
                    s->win_a_armed.win_a_size.h_size,
                    s->win_a_armed.win_a_size.v_size,
        tegra_dc_to_pixman(s->win_a_armed.win_a_color_depth.color_depth),
                    s->win_a_armed.win_a_line_stride.line_stride,
                    s->win_a_armed.winbuf_a_start_addr.reg32);
        }

        if (s->cmd_state_control.win_b_update){
            s->win_b_armed = s->win_b_assemly;

            qemu_free_displaysurface(s->win_b_surf);

            s->win_b_surf = qemu_create_displaysurface_guestmem(
                    s->win_b_armed.win_b_size.h_size,
                    s->win_b_armed.win_b_size.v_size,
        tegra_dc_to_pixman(s->win_b_armed.win_b_color_depth.color_depth),
                    s->win_b_armed.win_b_line_stride.line_stride,
                    s->win_b_armed.winbuf_b_start_addr.reg32);
        }

        if (s->cmd_state_control.win_c_update) {
            s->win_c_armed = s->win_c_assemly;

            qemu_free_displaysurface(s->win_c_surf);

            s->win_c_surf = qemu_create_displaysurface_guestmem(
                    s->win_c_armed.win_c_size.h_size,
                    s->win_c_armed.win_c_size.v_size,
        tegra_dc_to_pixman(s->win_c_armed.win_c_color_depth.color_depth),
                    s->win_c_armed.win_c_line_stride.line_stride,
                    s->win_c_armed.winbuf_c_start_addr.reg32);
        }
        break;
    case CMD_DISPLAY_WINDOW_HEADER_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_display_window_header.reg32, value);
        s->cmd_display_window_header.reg32 = value;
        break;
    case CMD_REG_ACT_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->cmd_reg_act_control.reg32, value);
        s->cmd_reg_act_control.reg32 = value;
        break;
    case COM_CRC_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_crc_control.reg32, value);
        s->com_crc_control.reg32 = value;
        break;
    case COM_PIN_OUTPUT_ENABLE0_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_enable0.reg32, value);
        s->com_pin_output_enable0.reg32 = value;
        break;
    case COM_PIN_OUTPUT_ENABLE1_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_enable1.reg32, value);
        s->com_pin_output_enable1.reg32 = value;
        break;
    case COM_PIN_OUTPUT_ENABLE2_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_enable2.reg32, value);
        s->com_pin_output_enable2.reg32 = value;
        break;
    case COM_PIN_OUTPUT_ENABLE3_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_enable3.reg32, value);
        s->com_pin_output_enable3.reg32 = value;
        break;
    case COM_PIN_OUTPUT_POLARITY0_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_polarity0.reg32, value);
        s->com_pin_output_polarity0.reg32 = value;
        break;
    case COM_PIN_OUTPUT_POLARITY1_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_polarity1.reg32, value);
        s->com_pin_output_polarity1.reg32 = value;
        break;
    case COM_PIN_OUTPUT_POLARITY2_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_polarity2.reg32, value);
        s->com_pin_output_polarity2.reg32 = value;
        break;
    case COM_PIN_OUTPUT_POLARITY3_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_polarity3.reg32, value);
        s->com_pin_output_polarity3.reg32 = value;
        break;
    case COM_PIN_OUTPUT_DATA0_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_data0.reg32, value);
        s->com_pin_output_data0.reg32 = value;
        break;
    case COM_PIN_OUTPUT_DATA1_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_data1.reg32, value);
        s->com_pin_output_data1.reg32 = value;
        break;
    case COM_PIN_OUTPUT_DATA2_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_data2.reg32, value);
        s->com_pin_output_data2.reg32 = value;
        break;
    case COM_PIN_OUTPUT_DATA3_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_data3.reg32, value);
        s->com_pin_output_data3.reg32 = value;
        break;
    case COM_PIN_INPUT_ENABLE0_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_input_enable0.reg32, value);
        s->com_pin_input_enable0.reg32 = value;
        break;
    case COM_PIN_INPUT_ENABLE1_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_input_enable1.reg32, value);
        s->com_pin_input_enable1.reg32 = value;
        break;
    case COM_PIN_INPUT_ENABLE2_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_input_enable2.reg32, value);
        s->com_pin_input_enable2.reg32 = value;
        break;
    case COM_PIN_INPUT_ENABLE3_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_input_enable3.reg32, value);
        s->com_pin_input_enable3.reg32 = value;
        break;
    case COM_PIN_OUTPUT_SELECT0_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_select0.reg32, value);
        s->com_pin_output_select0.reg32 = value;
        break;
    case COM_PIN_OUTPUT_SELECT1_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_select1.reg32, value);
        s->com_pin_output_select1.reg32 = value;
        break;
    case COM_PIN_OUTPUT_SELECT2_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_select2.reg32, value);
        s->com_pin_output_select2.reg32 = value;
        break;
    case COM_PIN_OUTPUT_SELECT3_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_select3.reg32, value);
        s->com_pin_output_select3.reg32 = value;
        break;
    case COM_PIN_OUTPUT_SELECT4_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_select4.reg32, value);
        s->com_pin_output_select4.reg32 = value;
        break;
    case COM_PIN_OUTPUT_SELECT5_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_select5.reg32, value);
        s->com_pin_output_select5.reg32 = value;
        break;
    case COM_PIN_OUTPUT_SELECT6_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_output_select6.reg32, value);
        s->com_pin_output_select6.reg32 = value;
        break;
    case COM_PIN_MISC_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pin_misc_control.reg32, value);
        s->com_pin_misc_control.reg32 = value;
        break;
    case COM_PM0_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pm0_control.reg32, value);
        s->com_pm0_control.reg32 = value;
        break;
    case COM_PM0_DUTY_CYCLE_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pm0_duty_cycle.reg32, value);
        s->com_pm0_duty_cycle.reg32 = value;
        break;
    case COM_PM1_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pm1_control.reg32, value);
        s->com_pm1_control.reg32 = value;
        break;
    case COM_PM1_DUTY_CYCLE_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_pm1_duty_cycle.reg32, value);
        s->com_pm1_duty_cycle.reg32 = value;
        break;
    case COM_SPI_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_spi_control.reg32, value);
        s->com_spi_control.reg32 = value;
        break;
    case COM_SPI_START_BYTE_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_spi_start_byte.reg32, value);
        s->com_spi_start_byte.reg32 = value;
        break;
    case COM_HSPI_WRITE_DATA_AB_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_hspi_write_data_ab.reg32, value);
        s->com_hspi_write_data_ab.reg32 = value;
        break;
    case COM_HSPI_WRITE_DATA_CD_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_hspi_write_data_cd.reg32, value);
        s->com_hspi_write_data_cd.reg32 = value;
        break;
    case COM_HSPI_CS_DC_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_hspi_cs_dc.reg32, value);
        s->com_hspi_cs_dc.reg32 = value;
        break;
    case COM_SCRATCH_REGISTER_A_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_scratch_register_a.reg32, value);
        s->com_scratch_register_a.reg32 = value;
        break;
    case COM_SCRATCH_REGISTER_B_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_scratch_register_b.reg32, value);
        s->com_scratch_register_b.reg32 = value;
        break;
    case COM_GPIO_CTRL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_gpio_ctrl.reg32, value);
        s->com_gpio_ctrl.reg32 = value;
        break;
    case COM_GPIO_DEBOUNCE_COUNTER_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->com_gpio_debounce_counter.reg32, value);
        s->com_gpio_debounce_counter.reg32 = value;
        break;
    case DISP_DISP_SIGNAL_OPTIONS0_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_disp_signal_options0.reg32, value);
        s->disp_disp_signal_options0.reg32 = value;
        break;
    case DISP_DISP_SIGNAL_OPTIONS1_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_disp_signal_options1.reg32, value);
        s->disp_disp_signal_options1.reg32 = value;
        break;
    case DISP_DISP_WIN_OPTIONS_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_disp_win_options.reg32, value);
        s->disp_disp_win_options.reg32 = value;
        break;
    case DISP_MEM_HIGH_PRIORITY_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_mem_high_priority.reg32, value);
        s->disp_mem_high_priority.reg32 = value;
        break;
    case DISP_MEM_HIGH_PRIORITY_TIMER_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_mem_high_priority_timer.reg32, value);
        s->disp_mem_high_priority_timer.reg32 = value;
        break;
    case DISP_DISP_TIMING_OPTIONS_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_disp_timing_options.reg32, value);
        s->disp_disp_timing_options.reg32 = value;
        break;
    case DISP_REF_TO_SYNC_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_ref_to_sync.reg32, value);
        s->disp_ref_to_sync.reg32 = value;
        break;
    case DISP_SYNC_WIDTH_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sync_width.reg32, value);
        s->disp_sync_width.reg32 = value;
        break;
    case DISP_BACK_PORCH_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_back_porch.reg32, value);
        s->disp_back_porch.reg32 = value;
        break;
    case DISP_DISP_ACTIVE_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_disp_active.reg32, value);
        s->disp_disp_active.reg32 = value;

        qemu_console_resize(s->con,
                            s->disp_disp_active.h_disp_active,
                            s->disp_disp_active.v_disp_active);
        break;
    case DISP_FRONT_PORCH_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_front_porch.reg32, value);
        s->disp_front_porch.reg32 = value;
        break;
    case DISP_H_PULSE0_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_h_pulse0_control.reg32, value);
        s->disp_h_pulse0_control.reg32 = value;
        break;
    case DISP_H_PULSE0_POSITION_A_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_h_pulse0_position_a.reg32, value);
        s->disp_h_pulse0_position_a.reg32 = value;
        break;
    case DISP_H_PULSE0_POSITION_B_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_h_pulse0_position_b.reg32, value);
        s->disp_h_pulse0_position_b.reg32 = value;
        break;
    case DISP_H_PULSE0_POSITION_C_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_h_pulse0_position_c.reg32, value);
        s->disp_h_pulse0_position_c.reg32 = value;
        break;
    case DISP_H_PULSE0_POSITION_D_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_h_pulse0_position_d.reg32, value);
        s->disp_h_pulse0_position_d.reg32 = value;
        break;
    case DISP_H_PULSE1_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_h_pulse1_control.reg32, value);
        s->disp_h_pulse1_control.reg32 = value;
        break;
    case DISP_H_PULSE1_POSITION_A_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_h_pulse1_position_a.reg32, value);
        s->disp_h_pulse1_position_a.reg32 = value;
        break;
    case DISP_H_PULSE1_POSITION_B_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_h_pulse1_position_b.reg32, value);
        s->disp_h_pulse1_position_b.reg32 = value;
        break;
    case DISP_H_PULSE1_POSITION_C_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_h_pulse1_position_c.reg32, value);
        s->disp_h_pulse1_position_c.reg32 = value;
        break;
    case DISP_H_PULSE1_POSITION_D_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_h_pulse1_position_d.reg32, value);
        s->disp_h_pulse1_position_d.reg32 = value;
        break;
    case DISP_H_PULSE2_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_h_pulse2_control.reg32, value);
        s->disp_h_pulse2_control.reg32 = value;
        break;
    case DISP_H_PULSE2_POSITION_A_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_h_pulse2_position_a.reg32, value);
        s->disp_h_pulse2_position_a.reg32 = value;
        break;
    case DISP_H_PULSE2_POSITION_B_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_h_pulse2_position_b.reg32, value);
        s->disp_h_pulse2_position_b.reg32 = value;
        break;
    case DISP_H_PULSE2_POSITION_C_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_h_pulse2_position_c.reg32, value);
        s->disp_h_pulse2_position_c.reg32 = value;
        break;
    case DISP_H_PULSE2_POSITION_D_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_h_pulse2_position_d.reg32, value);
        s->disp_h_pulse2_position_d.reg32 = value;
        break;
    case DISP_V_PULSE0_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_v_pulse0_control.reg32, value);
        s->disp_v_pulse0_control.reg32 = value;
        break;
    case DISP_V_PULSE0_POSITION_A_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_v_pulse0_position_a.reg32, value);
        s->disp_v_pulse0_position_a.reg32 = value;
        break;
    case DISP_V_PULSE0_POSITION_B_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_v_pulse0_position_b.reg32, value);
        s->disp_v_pulse0_position_b.reg32 = value;
        break;
    case DISP_V_PULSE0_POSITION_C_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_v_pulse0_position_c.reg32, value);
        s->disp_v_pulse0_position_c.reg32 = value;
        break;
    case DISP_V_PULSE1_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_v_pulse1_control.reg32, value);
        s->disp_v_pulse1_control.reg32 = value;
        break;
    case DISP_V_PULSE1_POSITION_A_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_v_pulse1_position_a.reg32, value);
        s->disp_v_pulse1_position_a.reg32 = value;
        break;
    case DISP_V_PULSE1_POSITION_B_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_v_pulse1_position_b.reg32, value);
        s->disp_v_pulse1_position_b.reg32 = value;
        break;
    case DISP_V_PULSE1_POSITION_C_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_v_pulse1_position_c.reg32, value);
        s->disp_v_pulse1_position_c.reg32 = value;
        break;
    case DISP_V_PULSE2_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_v_pulse2_control.reg32, value);
        s->disp_v_pulse2_control.reg32 = value;
        break;
    case DISP_V_PULSE2_POSITION_A_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_v_pulse2_position_a.reg32, value);
        s->disp_v_pulse2_position_a.reg32 = value;
        break;
    case DISP_V_PULSE3_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_v_pulse3_control.reg32, value);
        s->disp_v_pulse3_control.reg32 = value;
        break;
    case DISP_V_PULSE3_POSITION_A_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_v_pulse3_position_a.reg32, value);
        s->disp_v_pulse3_position_a.reg32 = value;
        break;
    case DISP_M0_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_m0_control.reg32, value);
        s->disp_m0_control.reg32 = value;
        break;
    case DISP_M1_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_m1_control.reg32, value);
        s->disp_m1_control.reg32 = value;
        break;
    case DISP_DI_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_di_control.reg32, value);
        s->disp_di_control.reg32 = value;
        break;
    case DISP_PP_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_pp_control.reg32, value);
        s->disp_pp_control.reg32 = value;
        break;
    case DISP_PP_SELECT_A_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_pp_select_a.reg32, value);
        s->disp_pp_select_a.reg32 = value;
        break;
    case DISP_PP_SELECT_B_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_pp_select_b.reg32, value);
        s->disp_pp_select_b.reg32 = value;
        break;
    case DISP_PP_SELECT_C_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_pp_select_c.reg32, value);
        s->disp_pp_select_c.reg32 = value;
        break;
    case DISP_PP_SELECT_D_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_pp_select_d.reg32, value);
        s->disp_pp_select_d.reg32 = value;
        break;
    case DISP_DISP_CLOCK_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_disp_clock_control.reg32, value);
        s->disp_disp_clock_control.reg32 = value;
        break;
    case DISP_DISP_INTERFACE_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_disp_interface_control.reg32, value);
        s->disp_disp_interface_control.reg32 = value;
        break;
    case DISP_DISP_COLOR_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_disp_color_control.reg32, value);
        s->disp_disp_color_control.reg32 = value;
        break;
    case DISP_SHIFT_CLOCK_OPTIONS_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_shift_clock_options.reg32, value);
        s->disp_shift_clock_options.reg32 = value;
        break;
    case DISP_DATA_ENABLE_OPTIONS_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_data_enable_options.reg32, value);
        s->disp_data_enable_options.reg32 = value;
        break;
    case DISP_SERIAL_INTERFACE_OPTIONS_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_serial_interface_options.reg32, value);
        s->disp_serial_interface_options.reg32 = value;
        break;
    case DISP_LCD_SPI_OPTIONS_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_lcd_spi_options.reg32, value);
        s->disp_lcd_spi_options.reg32 = value;
        break;
    case DISP_BORDER_COLOR_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_border_color.reg32, value);
        s->disp_border_color.reg32 = value;
        break;
    case DISP_COLOR_KEY0_LOWER_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_color_key0_lower.reg32, value);
        s->disp_color_key0_lower.reg32 = value;
        break;
    case DISP_COLOR_KEY0_UPPER_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_color_key0_upper.reg32, value);
        s->disp_color_key0_upper.reg32 = value;
        break;
    case DISP_COLOR_KEY1_LOWER_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_color_key1_lower.reg32, value);
        s->disp_color_key1_lower.reg32 = value;
        break;
    case DISP_COLOR_KEY1_UPPER_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_color_key1_upper.reg32, value);
        s->disp_color_key1_upper.reg32 = value;
        break;
    case DISP_CURSOR_FOREGROUND_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_cursor_foreground.reg32, value);
        s->disp_cursor_foreground.reg32 = value;
        break;
    case DISP_CURSOR_BACKGROUND_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_cursor_background.reg32, value);
        s->disp_cursor_background.reg32 = value;
        break;
    case DISP_CURSOR_START_ADDR_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_cursor_start_addr.reg32, value);
        s->disp_cursor_start_addr.reg32 = value;
        break;
    case DISP_CURSOR_START_ADDR_NS_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_cursor_start_addr_ns.reg32, value);
        s->disp_cursor_start_addr_ns.reg32 = value;
        break;
    case DISP_CURSOR_POSITION_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_cursor_position.reg32, value);
        s->disp_cursor_position.reg32 = value;
        break;
    case DISP_CURSOR_POSITION_NS_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_cursor_position_ns.reg32, value);
        s->disp_cursor_position_ns.reg32 = value;
        break;
    case DISP_INIT_SEQ_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_init_seq_control.reg32, value);
        s->disp_init_seq_control.reg32 = value;
        break;
    case DISP_SPI_INIT_SEQ_DATA_A_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_spi_init_seq_data_a.reg32, value);
        s->disp_spi_init_seq_data_a.reg32 = value;
        break;
    case DISP_SPI_INIT_SEQ_DATA_B_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_spi_init_seq_data_b.reg32, value);
        s->disp_spi_init_seq_data_b.reg32 = value;
        break;
    case DISP_SPI_INIT_SEQ_DATA_C_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_spi_init_seq_data_c.reg32, value);
        s->disp_spi_init_seq_data_c.reg32 = value;
        break;
    case DISP_SPI_INIT_SEQ_DATA_D_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_spi_init_seq_data_d.reg32, value);
        s->disp_spi_init_seq_data_d.reg32 = value;
        break;
    case DISP_DC_MCCIF_FIFOCTRL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_dc_mccif_fifoctrl.reg32, value);
        s->disp_dc_mccif_fifoctrl.reg32 = value;
        break;
    case DISP_MCCIF_DISPLAY0A_HYST_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_mccif_display0a_hyst.reg32, value);
        s->disp_mccif_display0a_hyst.reg32 = value;
        break;
    case DISP_MCCIF_DISPLAY0B_HYST_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_mccif_display0b_hyst.reg32, value);
        s->disp_mccif_display0b_hyst.reg32 = value;
        break;
    case DISP_MCCIF_DISPLAY0C_HYST_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_mccif_display0c_hyst.reg32, value);
        s->disp_mccif_display0c_hyst.reg32 = value;
        break;
    case DISP_MCCIF_DISPLAY1B_HYST_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_mccif_display1b_hyst.reg32, value);
        s->disp_mccif_display1b_hyst.reg32 = value;
        break;
    case DISP_DAC_CRT_CTRL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_dac_crt_ctrl.reg32, value);
        s->disp_dac_crt_ctrl.reg32 = value;
        break;
    case DISP_DISP_MISC_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_disp_misc_control.reg32, value);
        s->disp_disp_misc_control.reg32 = value;
        break;
    case WINC_COLOR_PALETTE_OFFSET:
        TRACE_WIN_ABC_WRITE(winc_a_color_palette, winc_b_color_palette,
                            winc_c_color_palette, value);
        WIN_ABC_SET(winc_a_color_palette, winc_b_color_palette,
                    winc_c_color_palette, value);
        break;
    case WINC_PALETTE_COLOR_EXT_OFFSET:
        TRACE_WIN_ABC_WRITE(winc_a_palette_color_ext, winc_b_palette_color_ext,
                            winc_c_palette_color_ext, value);
        WIN_ABC_SET(winc_a_palette_color_ext, winc_b_palette_color_ext,
                    winc_c_palette_color_ext, value);
        break;
    case WINC_H_FILTER_P00_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_h_filter_p00, winc_c_h_filter_p00, value);
        WIN_BC_SET(winc_b_h_filter_p00, winc_c_h_filter_p00, value);
        break;
    case WINC_H_FILTER_P01_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_h_filter_p01, winc_c_h_filter_p01, value);
        WIN_BC_SET(winc_b_h_filter_p01, winc_c_h_filter_p01, value);
        break;
    case WINC_H_FILTER_P02_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_h_filter_p02, winc_c_h_filter_p02, value);
        WIN_BC_SET(winc_b_h_filter_p02, winc_c_h_filter_p02, value);
        break;
    case WINC_H_FILTER_P03_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_h_filter_p03, winc_c_h_filter_p03, value);
        WIN_BC_SET(winc_b_h_filter_p03, winc_c_h_filter_p03, value);
        break;
    case WINC_H_FILTER_P04_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_h_filter_p04, winc_c_h_filter_p04, value);
        WIN_BC_SET(winc_b_h_filter_p04, winc_c_h_filter_p04, value);
        break;
    case WINC_H_FILTER_P05_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_h_filter_p05, winc_c_h_filter_p05, value);
        WIN_BC_SET(winc_b_h_filter_p05, winc_c_h_filter_p05, value);
        break;
    case WINC_H_FILTER_P06_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_h_filter_p06, winc_c_h_filter_p06, value);
        WIN_BC_SET(winc_b_h_filter_p06, winc_c_h_filter_p06, value);
        break;
    case WINC_H_FILTER_P07_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_h_filter_p07, winc_c_h_filter_p07, value);
        WIN_BC_SET(winc_b_h_filter_p07, winc_c_h_filter_p07, value);
        break;
    case WINC_H_FILTER_P08_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_h_filter_p08, winc_c_h_filter_p08, value);
        WIN_BC_SET(winc_b_h_filter_p08, winc_c_h_filter_p08, value);
        break;
    case WINC_H_FILTER_P09_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_h_filter_p09, winc_c_h_filter_p09, value);
        WIN_BC_SET(winc_b_h_filter_p09, winc_c_h_filter_p09, value);
        break;
    case WINC_H_FILTER_P0A_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_h_filter_p0a, winc_c_h_filter_p0a, value);
        WIN_BC_SET(winc_b_h_filter_p0a, winc_c_h_filter_p0a, value);
        break;
    case WINC_H_FILTER_P0B_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_h_filter_p0b, winc_c_h_filter_p0b, value);
        WIN_BC_SET(winc_b_h_filter_p0b, winc_c_h_filter_p0b, value);
        break;
    case WINC_H_FILTER_P0C_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_h_filter_p0c, winc_c_h_filter_p0c, value);
        WIN_BC_SET(winc_b_h_filter_p0c, winc_c_h_filter_p0c, value);
        break;
    case WINC_H_FILTER_P0D_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_h_filter_p0d, winc_c_h_filter_p0d, value);
        WIN_BC_SET(winc_b_h_filter_p0d, winc_c_h_filter_p0d, value);
        break;
    case WINC_H_FILTER_P0E_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_h_filter_p0e, winc_c_h_filter_p0e, value);
        WIN_BC_SET(winc_b_h_filter_p0e, winc_c_h_filter_p0e, value);
        break;
    case WINC_H_FILTER_P0F_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_h_filter_p0f, winc_c_h_filter_p0f, value);
        WIN_BC_SET(winc_b_h_filter_p0f, winc_c_h_filter_p0f, value);
        break;
    case WINC_CSC_YOF_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_csc_yof, winc_c_csc_yof, value);
        WIN_BC_SET(winc_b_csc_yof, winc_c_csc_yof, value);
        break;
    case WINC_CSC_KYRGB_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_csc_kyrgb, winc_c_csc_kyrgb, value);
        WIN_BC_SET(winc_b_csc_kyrgb, winc_c_csc_kyrgb, value);
        break;
    case WINC_CSC_KUR_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_csc_kur, winc_c_csc_kur, value);
        WIN_BC_SET(winc_b_csc_kur, winc_c_csc_kur, value);
        break;
    case WINC_CSC_KVR_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_csc_kvr, winc_c_csc_kvr, value);
        WIN_BC_SET(winc_b_csc_kvr, winc_c_csc_kvr, value);
        break;
    case WINC_CSC_KUG_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_csc_kug, winc_c_csc_kug, value);
        WIN_BC_SET(winc_b_csc_kug, winc_c_csc_kug, value);
        break;
    case WINC_CSC_KVG_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_csc_kvg, winc_c_csc_kvg, value);
        WIN_BC_SET(winc_b_csc_kvg, winc_c_csc_kvg, value);
        break;
    case WINC_CSC_KUB_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_csc_kub, winc_c_csc_kub, value);
        WIN_BC_SET(winc_b_csc_kub, winc_c_csc_kub, value);
        break;
    case WINC_CSC_KVB_OFFSET:
        TRACE_WIN_BC_WRITE(winc_b_csc_kvb, winc_c_csc_kvb, value);
        WIN_BC_SET(winc_b_csc_kvb, winc_c_csc_kvb, value);
        break;
    case WINC_V_FILTER_P00_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_v_filter_p00.reg32, value);
            s->win_b_assemly.winc_b_v_filter_p00.reg32 = value;
        }
        break;
    case WINC_V_FILTER_P01_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_v_filter_p01.reg32, value);
            s->win_b_assemly.winc_b_v_filter_p01.reg32 = value;
        }
        break;
    case WINC_V_FILTER_P02_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_v_filter_p02.reg32, value);
            s->win_b_assemly.winc_b_v_filter_p02.reg32 = value;
        }
        break;
    case WINC_V_FILTER_P03_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_v_filter_p03.reg32, value);
            s->win_b_assemly.winc_b_v_filter_p03.reg32 = value;
        }
        break;
    case WINC_V_FILTER_P04_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_v_filter_p04.reg32, value);
            s->win_b_assemly.winc_b_v_filter_p04.reg32 = value;
        }
        break;
    case WINC_V_FILTER_P05_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_v_filter_p05.reg32, value);
            s->win_b_assemly.winc_b_v_filter_p05.reg32 = value;
        }
        break;
    case WINC_V_FILTER_P06_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_v_filter_p06.reg32, value);
            s->win_b_assemly.winc_b_v_filter_p06.reg32 = value;
        }
        break;
    case WINC_V_FILTER_P07_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_v_filter_p07.reg32, value);
            s->win_b_assemly.winc_b_v_filter_p07.reg32 = value;
        }
        break;
    case WINC_V_FILTER_P08_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_v_filter_p08.reg32, value);
            s->win_b_assemly.winc_b_v_filter_p08.reg32 = value;
        }
        break;
    case WINC_V_FILTER_P09_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_v_filter_p09.reg32, value);
            s->win_b_assemly.winc_b_v_filter_p09.reg32 = value;
        }
        break;
    case WINC_V_FILTER_P0A_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_v_filter_p0a.reg32, value);
            s->win_b_assemly.winc_b_v_filter_p0a.reg32 = value;
        }
        break;
    case WINC_V_FILTER_P0B_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_v_filter_p0b.reg32, value);
            s->win_b_assemly.winc_b_v_filter_p0b.reg32 = value;
        }
        break;
    case WINC_V_FILTER_P0C_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_v_filter_p0c.reg32, value);
            s->win_b_assemly.winc_b_v_filter_p0c.reg32 = value;
        }
        break;
    case WINC_V_FILTER_P0D_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_v_filter_p0d.reg32, value);
            s->win_b_assemly.winc_b_v_filter_p0d.reg32 = value;
        }
        break;
    case WINC_V_FILTER_P0E_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_v_filter_p0e.reg32, value);
            s->win_b_assemly.winc_b_v_filter_p0e.reg32 = value;
        }
        break;
    case WINC_V_FILTER_P0F_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_v_filter_p0f.reg32, value);
            s->win_b_assemly.winc_b_v_filter_p0f.reg32 = value;
        }
        break;
    case WIN_WIN_OPTIONS_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_win_options, win_b_win_options,
                            win_c_win_options, value);
        WIN_ABC_SET(win_a_win_options, win_b_win_options,
                    win_c_win_options, value);
        break;
    case WIN_BYTE_SWAP_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_byte_swap, win_b_byte_swap,
                            win_c_byte_swap, value);
        WIN_ABC_SET(win_a_byte_swap, win_b_byte_swap,
                    win_c_byte_swap, value);
        break;
    case WIN_BUFFER_CONTROL_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_buffer_control, win_b_buffer_control,
                            win_c_buffer_control, value);
        WIN_ABC_SET(win_a_buffer_control, win_b_buffer_control,
                    win_c_buffer_control, value);
        break;
    case WIN_COLOR_DEPTH_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_color_depth, win_b_color_depth,
                            win_c_color_depth, value);
        WIN_ABC_SET(win_a_color_depth, win_b_color_depth,
                    win_c_color_depth, value);
        break;
    case WIN_POSITION_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_position, win_b_position,
                            win_c_position, value);
        WIN_ABC_SET(win_a_position, win_b_position,
                    win_c_position, value);
        break;
    case WIN_SIZE_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_size, win_b_size,
                            win_c_size, value);
        WIN_ABC_SET(win_a_size, win_b_size,
                    win_c_size, value);
        break;
    case WIN_PRESCALED_SIZE_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_prescaled_size, win_b_prescaled_size,
                            win_c_prescaled_size, value);
        WIN_ABC_SET(win_a_prescaled_size, win_b_prescaled_size,
                    win_c_prescaled_size, value);
        break;
    case WIN_H_INITIAL_DDA_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_h_initial_dda, win_b_h_initial_dda,
                            win_c_h_initial_dda, value);
        WIN_ABC_SET(win_a_h_initial_dda, win_b_h_initial_dda,
                    win_c_h_initial_dda, value);
        break;
    case WIN_V_INITIAL_DDA_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_v_initial_dda, win_b_v_initial_dda,
                            win_c_v_initial_dda, value);
        WIN_ABC_SET(win_a_v_initial_dda, win_b_v_initial_dda,
                    win_c_v_initial_dda, value);
        break;
    case WIN_DDA_INCREMENT_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_dda_increment, win_b_dda_increment,
                            win_c_dda_increment, value);
        WIN_ABC_SET(win_a_dda_increment, win_b_dda_increment,
                    win_c_dda_increment, value);
        break;
    case WIN_LINE_STRIDE_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_line_stride, win_b_line_stride,
                            win_c_line_stride, value);
        WIN_ABC_SET(win_a_line_stride, win_b_line_stride,
                    win_c_line_stride, value);
        break;
    case WIN_BUF_STRIDE_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_buf_stride, win_b_buf_stride,
                            win_c_buf_stride, value);
        WIN_ABC_SET(win_a_buf_stride, win_b_buf_stride,
                    win_c_buf_stride, value);
        break;
    case WIN_UV_BUF_STRIDE_OFFSET:
        TRACE_WIN_BC_WRITE(win_b_uv_buf_stride, win_c_uv_buf_stride, value);
        WIN_BC_SET(win_b_uv_buf_stride, win_c_uv_buf_stride, value);
        break;
    case WIN_BUFFER_ADDR_MODE_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_buffer_addr_mode, win_b_buffer_addr_mode,
                            win_c_buffer_addr_mode, value);
        WIN_ABC_SET(win_a_buffer_addr_mode, win_b_buffer_addr_mode,
                    win_c_buffer_addr_mode, value);
        break;
    case WIN_DV_CONTROL_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_dv_control, win_b_dv_control,
                            win_c_dv_control, value);
        WIN_ABC_SET(win_a_dv_control, win_b_dv_control,
                    win_c_dv_control, value);
        break;
    case WIN_BLEND_NOKEY_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_blend_nokey, win_b_blend_nokey,
                            win_c_blend_nokey, value);
        WIN_ABC_SET(win_a_blend_nokey, win_b_blend_nokey,
                    win_c_blend_nokey, value);
        break;
    case WIN_BLEND_1WIN_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_blend_1win, win_b_blend_1win,
                            win_c_blend_1win, value);
        WIN_ABC_SET(win_a_blend_1win, win_b_blend_1win,
                    win_c_blend_1win, value);
        break;
    case WIN_BLEND_2WIN_X_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_blend_2win_b, win_b_blend_2win_a,
                            win_c_blend_2win_a, value);
        WIN_ABC_SET(win_a_blend_2win_b, win_b_blend_2win_a,
                    win_c_blend_2win_a, value);
        break;
    case WIN_BLEND_2WIN_Y_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_blend_2win_c, win_b_blend_2win_c,
                            win_c_blend_2win_b, value);
        WIN_ABC_SET(win_a_blend_2win_c, win_b_blend_2win_c,
                    win_c_blend_2win_b, value);
        break;
    case WIN_BLEND_3WIN_XY_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_blend_3win_bc, win_b_blend_3win_ac,
                            win_c_blend_3win_ab, value);
        WIN_ABC_SET(win_a_blend_3win_bc, win_b_blend_3win_ac,
                    win_c_blend_3win_ab, value);
        break;
    case WIN_HP_FETCH_CONTROL_OFFSET:
        TRACE_WIN_ABC_WRITE(win_a_hp_fetch_control, win_b_hp_fetch_control,
                            win_c_hp_fetch_control, value);
        WIN_ABC_SET(win_a_hp_fetch_control, win_b_hp_fetch_control,
                    win_c_hp_fetch_control, value);
        break;
    case WINBUF_START_ADDR_OFFSET:
        TRACE_WIN_ABC_WRITE(winbuf_a_start_addr, winbuf_b_start_addr,
                            winbuf_c_start_addr, value);
        WIN_ABC_SET(winbuf_a_start_addr, winbuf_b_start_addr,
                    winbuf_c_start_addr, value);
        break;
    case WINBUF_START_ADDR_NS_OFFSET:
        TRACE_WIN_ABC_WRITE(winbuf_a_start_addr_ns, winbuf_b_start_addr_ns,
                            winbuf_c_start_addr_ns, value);
        WIN_ABC_SET(winbuf_a_start_addr_ns, winbuf_b_start_addr_ns,
                    winbuf_c_start_addr_ns, value);
        break;
    case WINBUF_START_ADDR_U_OFFSET:
        TRACE_WIN_BC_WRITE(winbuf_b_start_addr_u, winbuf_c_start_addr_u, value);
        WIN_BC_SET(winbuf_b_start_addr_u, winbuf_c_start_addr_u, value);
        break;
    case WINBUF_START_ADDR_U_NS_OFFSET:
        TRACE_WIN_BC_WRITE(winbuf_b_start_addr_u_ns, winbuf_c_start_addr_u_ns, value);
        WIN_BC_SET(winbuf_b_start_addr_u_ns, winbuf_c_start_addr_u_ns, value);
        break;
    case WINBUF_START_ADDR_V_OFFSET:
        TRACE_WIN_BC_WRITE(winbuf_b_start_addr_v, winbuf_c_start_addr_v, value);
        WIN_BC_SET(winbuf_b_start_addr_v, winbuf_c_start_addr_v, value);
        break;
    case WINBUF_START_ADDR_V_NS_OFFSET:
        TRACE_WIN_BC_WRITE(winbuf_b_start_addr_v_ns, winbuf_c_start_addr_v_ns, value);
        WIN_BC_SET(winbuf_b_start_addr_v_ns, winbuf_c_start_addr_v_ns, value);
        break;
    case WINBUF_ADDR_H_OFFSET_OFFSET:
        TRACE_WIN_ABC_WRITE(winbuf_a_addr_h_offset, winbuf_b_addr_h_offset,
                            winbuf_c_addr_h_offset, value);
        WIN_ABC_SET(winbuf_a_addr_h_offset, winbuf_b_addr_h_offset,
                    winbuf_c_addr_h_offset, value);
        break;
    case WINBUF_ADDR_H_OFFSET_NS_OFFSET:
        TRACE_WIN_ABC_WRITE(winbuf_a_addr_h_offset_ns, winbuf_b_addr_h_offset_ns,
                            winbuf_c_addr_h_offset_ns, value);
        WIN_ABC_SET(winbuf_a_addr_h_offset_ns, winbuf_b_addr_h_offset_ns,
                    winbuf_c_addr_h_offset_ns, value);
        break;
    case WINBUF_ADDR_V_OFFSET_OFFSET:
        TRACE_WIN_ABC_WRITE(winbuf_a_addr_v_offset, winbuf_b_addr_v_offset,
                            winbuf_c_addr_v_offset, value);
        WIN_ABC_SET(winbuf_a_addr_v_offset, winbuf_b_addr_v_offset,
                    winbuf_c_addr_v_offset, value);
        break;
    case WINBUF_ADDR_V_OFFSET_NS_OFFSET:
        TRACE_WIN_ABC_WRITE(winbuf_a_addr_v_offset_ns, winbuf_b_addr_v_offset_ns,
                            winbuf_c_addr_v_offset_ns, value);
        WIN_ABC_SET(winbuf_a_addr_v_offset_ns, winbuf_b_addr_v_offset_ns,
                    winbuf_c_addr_v_offset_ns, value);
        break;
    case WINBUF_UFLOW_STATUS_OFFSET:
        TRACE_WIN_ABC_WRITE(winbuf_a_uflow_status, winbuf_b_uflow_status,
                            winbuf_c_uflow_status, value);
        WIN_ABC_SET(winbuf_a_uflow_status, winbuf_b_uflow_status,
                    winbuf_c_uflow_status, value);
        break;
    case DISP_SD_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_control.reg32, value);
        s->disp_sd_control.reg32 = value;
        break;
    case DISP_SD_CSC_COEFF_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_csc_coeff.reg32, value);
        s->disp_sd_csc_coeff.reg32 = value;
        break;
    case DISP_SD_LUT_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_lut.reg32, value);
        s->disp_sd_lut.reg32 = value;
        break;
    case DISP_SD_LUT_1_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_lut_1.reg32, value);
        s->disp_sd_lut_1.reg32 = value;
        break;
    case DISP_SD_LUT_2_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_lut_2.reg32, value);
        s->disp_sd_lut_2.reg32 = value;
        break;
    case DISP_SD_LUT_3_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_lut_3.reg32, value);
        s->disp_sd_lut_3.reg32 = value;
        break;
    case DISP_SD_LUT_4_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_lut_4.reg32, value);
        s->disp_sd_lut_4.reg32 = value;
        break;
    case DISP_SD_LUT_5_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_lut_5.reg32, value);
        s->disp_sd_lut_5.reg32 = value;
        break;
    case DISP_SD_LUT_6_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_lut_6.reg32, value);
        s->disp_sd_lut_6.reg32 = value;
        break;
    case DISP_SD_LUT_7_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_lut_7.reg32, value);
        s->disp_sd_lut_7.reg32 = value;
        break;
    case DISP_SD_LUT_8_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_lut_8.reg32, value);
        s->disp_sd_lut_8.reg32 = value;
        break;
    case DISP_SD_FLICKER_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_flicker_control.reg32, value);
        s->disp_sd_flicker_control.reg32 = value;
        break;
    case DISP_SD_PIXEL_COUNT_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_pixel_count.reg32, value & DISP_SD_PIXEL_COUNT_WRMASK);
        WR_MASKED(s->disp_sd_pixel_count.reg32, value, DISP_SD_PIXEL_COUNT);
        break;
    case DISP_SD_BL_PARAMETERS_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_bl_parameters.reg32, value);
        s->disp_sd_bl_parameters.reg32 = value;
        break;
    case DISP_SD_BL_TF_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_bl_tf.reg32, value);
        s->disp_sd_bl_tf.reg32 = value;
        break;
    case DISP_SD_BL_TF_1_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_bl_tf_1.reg32, value);
        s->disp_sd_bl_tf_1.reg32 = value;
        break;
    case DISP_SD_BL_TF_2_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_bl_tf_2.reg32, value);
        s->disp_sd_bl_tf_2.reg32 = value;
        break;
    case DISP_SD_BL_TF_3_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_bl_tf_3.reg32, value);
        s->disp_sd_bl_tf_3.reg32 = value;
        break;
    case DISP_SD_BL_CONTROL_OFFSET:
        TRACE_WRITE(s->iomem.addr, offset, s->disp_sd_bl_control.reg32, value & DISP_SD_BL_CONTROL_WRMASK);
        WR_MASKED(s->disp_sd_bl_control.reg32, value, DISP_SD_BL_CONTROL);
        break;
    case WINC_COLOR_PALETTE_1_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_color_palette_1.reg32, value);
            s->win_b_assemly.winc_b_color_palette_1.reg32 = value;
        }
        break;
    case WINC_COLOR_PALETTE_2_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_color_palette_2.reg32, value);
            s->win_b_assemly.winc_b_color_palette_2.reg32 = value;
        }
        break;
    case WINC_COLOR_PALETTE_3_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_color_palette_3.reg32, value);
            s->win_b_assemly.winc_b_color_palette_3.reg32 = value;
        }
        break;
    case WINC_COLOR_PALETTE_4_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_color_palette_4.reg32, value);
            s->win_b_assemly.winc_b_color_palette_4.reg32 = value;
        }
        break;
    case WINC_COLOR_PALETTE_5_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_color_palette_5.reg32, value);
            s->win_b_assemly.winc_b_color_palette_5.reg32 = value;
        }
        break;
    case WINC_COLOR_PALETTE_6_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_color_palette_6.reg32, value);
            s->win_b_assemly.winc_b_color_palette_6.reg32 = value;
        }
        break;
    case WINC_COLOR_PALETTE_7_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_color_palette_7.reg32, value);
            s->win_b_assemly.winc_b_color_palette_7.reg32 = value;
        }
        break;
    case WINC_COLOR_PALETTE_8_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_color_palette_8.reg32, value);
            s->win_b_assemly.winc_b_color_palette_8.reg32 = value;
        }
        break;
    case WINC_COLOR_PALETTE_9_OFFSET:
        if (WIN_IDX == WIN_B) {
            TRACE_WRITE(s->iomem.addr, offset + (WIN_IDX >> 1) * 0x1000,
                        s->win_b_assemly.winc_b_color_palette_9.reg32, value);
            s->win_b_assemly.winc_b_color_palette_9.reg32 = value;
        }
        break;
    default:
        TRACE_WRITE(s->iomem.addr, offset, 0, value);
        break;
    }
}

static void tegra_dc_win_a_reset(tegra_dc_win_a *win)
{
    win->winc_a_color_palette.reg32 = WINC_A_COLOR_PALETTE_RESET;
    win->winc_a_palette_color_ext.reg32 = WINC_A_PALETTE_COLOR_EXT_RESET;
    win->win_a_win_options.reg32 = WIN_A_WIN_OPTIONS_RESET;
    win->win_a_byte_swap.reg32 = WIN_A_BYTE_SWAP_RESET;
    win->win_a_buffer_control.reg32 = WIN_A_BUFFER_CONTROL_RESET;
    win->win_a_color_depth.reg32 = WIN_A_COLOR_DEPTH_RESET;
    win->win_a_position.reg32 = WIN_A_POSITION_RESET;
    win->win_a_size.reg32 = WIN_A_SIZE_RESET;
    win->win_a_prescaled_size.reg32 = WIN_A_PRESCALED_SIZE_RESET;
    win->win_a_h_initial_dda.reg32 = WIN_A_H_INITIAL_DDA_RESET;
    win->win_a_v_initial_dda.reg32 = WIN_A_V_INITIAL_DDA_RESET;
    win->win_a_dda_increment.reg32 = WIN_A_DDA_INCREMENT_RESET;
    win->win_a_line_stride.reg32 = WIN_A_LINE_STRIDE_RESET;
    win->win_a_buf_stride.reg32 = WIN_A_BUF_STRIDE_RESET;
    win->win_a_buffer_addr_mode.reg32 = WIN_A_BUFFER_ADDR_MODE_RESET;
    win->win_a_dv_control.reg32 = WIN_A_DV_CONTROL_RESET;
    win->win_a_blend_nokey.reg32 = WIN_A_BLEND_NOKEY_RESET;
    win->win_a_blend_1win.reg32 = WIN_A_BLEND_1WIN_RESET;
    win->win_a_blend_2win_b.reg32 = WIN_A_BLEND_2WIN_B_RESET;
    win->win_a_blend_2win_c.reg32 = WIN_A_BLEND_2WIN_C_RESET;
    win->win_a_blend_3win_bc.reg32 = WIN_A_BLEND_3WIN_BC_RESET;
    win->win_a_hp_fetch_control.reg32 = WIN_A_HP_FETCH_CONTROL_RESET;
    win->winbuf_a_start_addr.reg32 = WINBUF_A_START_ADDR_RESET;
    win->winbuf_a_start_addr_ns.reg32 = WINBUF_A_START_ADDR_NS_RESET;
    win->winbuf_a_addr_h_offset.reg32 = WINBUF_A_ADDR_H_OFFSET_RESET;
    win->winbuf_a_addr_h_offset_ns.reg32 = WINBUF_A_ADDR_H_OFFSET_NS_RESET;
    win->winbuf_a_addr_v_offset.reg32 = WINBUF_A_ADDR_V_OFFSET_RESET;
    win->winbuf_a_addr_v_offset_ns.reg32 = WINBUF_A_ADDR_V_OFFSET_NS_RESET;
    win->winbuf_a_uflow_status.reg32 = WINBUF_A_UFLOW_STATUS_RESET;
}

static void tegra_dc_win_b_reset(tegra_dc_win_b *win)
{
    win->winc_b_color_palette.reg32 = WINC_B_COLOR_PALETTE_RESET;
    win->winc_b_palette_color_ext.reg32 = WINC_B_PALETTE_COLOR_EXT_RESET;
    win->winc_b_h_filter_p00.reg32 = WINC_B_H_FILTER_P00_RESET;
    win->winc_b_h_filter_p01.reg32 = WINC_B_H_FILTER_P01_RESET;
    win->winc_b_h_filter_p02.reg32 = WINC_B_H_FILTER_P02_RESET;
    win->winc_b_h_filter_p03.reg32 = WINC_B_H_FILTER_P03_RESET;
    win->winc_b_h_filter_p04.reg32 = WINC_B_H_FILTER_P04_RESET;
    win->winc_b_h_filter_p05.reg32 = WINC_B_H_FILTER_P05_RESET;
    win->winc_b_h_filter_p06.reg32 = WINC_B_H_FILTER_P06_RESET;
    win->winc_b_h_filter_p07.reg32 = WINC_B_H_FILTER_P07_RESET;
    win->winc_b_h_filter_p08.reg32 = WINC_B_H_FILTER_P08_RESET;
    win->winc_b_h_filter_p09.reg32 = WINC_B_H_FILTER_P09_RESET;
    win->winc_b_h_filter_p0a.reg32 = WINC_B_H_FILTER_P0A_RESET;
    win->winc_b_h_filter_p0b.reg32 = WINC_B_H_FILTER_P0B_RESET;
    win->winc_b_h_filter_p0c.reg32 = WINC_B_H_FILTER_P0C_RESET;
    win->winc_b_h_filter_p0d.reg32 = WINC_B_H_FILTER_P0D_RESET;
    win->winc_b_h_filter_p0e.reg32 = WINC_B_H_FILTER_P0E_RESET;
    win->winc_b_h_filter_p0f.reg32 = WINC_B_H_FILTER_P0F_RESET;
    win->winc_b_csc_yof.reg32 = WINC_B_CSC_YOF_RESET;
    win->winc_b_csc_kyrgb.reg32 = WINC_B_CSC_KYRGB_RESET;
    win->winc_b_csc_kur.reg32 = WINC_B_CSC_KUR_RESET;
    win->winc_b_csc_kvr.reg32 = WINC_B_CSC_KVR_RESET;
    win->winc_b_csc_kug.reg32 = WINC_B_CSC_KUG_RESET;
    win->winc_b_csc_kvg.reg32 = WINC_B_CSC_KVG_RESET;
    win->winc_b_csc_kub.reg32 = WINC_B_CSC_KUB_RESET;
    win->winc_b_csc_kvb.reg32 = WINC_B_CSC_KVB_RESET;
    win->winc_b_v_filter_p00.reg32 = WINC_B_V_FILTER_P00_RESET;
    win->winc_b_v_filter_p01.reg32 = WINC_B_V_FILTER_P01_RESET;
    win->winc_b_v_filter_p02.reg32 = WINC_B_V_FILTER_P02_RESET;
    win->winc_b_v_filter_p03.reg32 = WINC_B_V_FILTER_P03_RESET;
    win->winc_b_v_filter_p04.reg32 = WINC_B_V_FILTER_P04_RESET;
    win->winc_b_v_filter_p05.reg32 = WINC_B_V_FILTER_P05_RESET;
    win->winc_b_v_filter_p06.reg32 = WINC_B_V_FILTER_P06_RESET;
    win->winc_b_v_filter_p07.reg32 = WINC_B_V_FILTER_P07_RESET;
    win->winc_b_v_filter_p08.reg32 = WINC_B_V_FILTER_P08_RESET;
    win->winc_b_v_filter_p09.reg32 = WINC_B_V_FILTER_P09_RESET;
    win->winc_b_v_filter_p0a.reg32 = WINC_B_V_FILTER_P0A_RESET;
    win->winc_b_v_filter_p0b.reg32 = WINC_B_V_FILTER_P0B_RESET;
    win->winc_b_v_filter_p0c.reg32 = WINC_B_V_FILTER_P0C_RESET;
    win->winc_b_v_filter_p0d.reg32 = WINC_B_V_FILTER_P0D_RESET;
    win->winc_b_v_filter_p0e.reg32 = WINC_B_V_FILTER_P0E_RESET;
    win->winc_b_v_filter_p0f.reg32 = WINC_B_V_FILTER_P0F_RESET;
    win->win_b_win_options.reg32 = WIN_B_WIN_OPTIONS_RESET;
    win->win_b_byte_swap.reg32 = WIN_B_BYTE_SWAP_RESET;
    win->win_b_buffer_control.reg32 = WIN_B_BUFFER_CONTROL_RESET;
    win->win_b_color_depth.reg32 = WIN_B_COLOR_DEPTH_RESET;
    win->win_b_position.reg32 = WIN_B_POSITION_RESET;
    win->win_b_size.reg32 = WIN_B_SIZE_RESET;
    win->win_b_prescaled_size.reg32 = WIN_B_PRESCALED_SIZE_RESET;
    win->win_b_h_initial_dda.reg32 = WIN_B_H_INITIAL_DDA_RESET;
    win->win_b_v_initial_dda.reg32 = WIN_B_V_INITIAL_DDA_RESET;
    win->win_b_dda_increment.reg32 = WIN_B_DDA_INCREMENT_RESET;
    win->win_b_line_stride.reg32 = WIN_B_LINE_STRIDE_RESET;
    win->win_b_buf_stride.reg32 = WIN_B_BUF_STRIDE_RESET;
    win->win_b_uv_buf_stride.reg32 = WIN_B_UV_BUF_STRIDE_RESET;
    win->win_b_buffer_addr_mode.reg32 = WIN_B_BUFFER_ADDR_MODE_RESET;
    win->win_b_dv_control.reg32 = WIN_B_DV_CONTROL_RESET;
    win->win_b_blend_nokey.reg32 = WIN_B_BLEND_NOKEY_RESET;
    win->win_b_blend_1win.reg32 = WIN_B_BLEND_1WIN_RESET;
    win->win_b_blend_2win_a.reg32 = WIN_B_BLEND_2WIN_A_RESET;
    win->win_b_blend_2win_c.reg32 = WIN_B_BLEND_2WIN_C_RESET;
    win->win_b_blend_3win_ac.reg32 = WIN_B_BLEND_3WIN_AC_RESET;
    win->win_b_hp_fetch_control.reg32 = WIN_B_HP_FETCH_CONTROL_RESET;
    win->winbuf_b_start_addr.reg32 = WINBUF_B_START_ADDR_RESET;
    win->winbuf_b_start_addr_ns.reg32 = WINBUF_B_START_ADDR_NS_RESET;
    win->winbuf_b_start_addr_u.reg32 = WINBUF_B_START_ADDR_U_RESET;
    win->winbuf_b_start_addr_u_ns.reg32 = WINBUF_B_START_ADDR_U_NS_RESET;
    win->winbuf_b_start_addr_v.reg32 = WINBUF_B_START_ADDR_V_RESET;
    win->winbuf_b_start_addr_v_ns.reg32 = WINBUF_B_START_ADDR_V_NS_RESET;
    win->winbuf_b_addr_h_offset.reg32 = WINBUF_B_ADDR_H_OFFSET_RESET;
    win->winbuf_b_addr_h_offset_ns.reg32 = WINBUF_B_ADDR_H_OFFSET_NS_RESET;
    win->winbuf_b_addr_v_offset.reg32 = WINBUF_B_ADDR_V_OFFSET_RESET;
    win->winbuf_b_addr_v_offset_ns.reg32 = WINBUF_B_ADDR_V_OFFSET_NS_RESET;
    win->winbuf_b_uflow_status.reg32 = WINBUF_B_UFLOW_STATUS_RESET;
    win->winc_b_color_palette_1.reg32 = WINC_B_COLOR_PALETTE_1_RESET;
    win->winc_b_color_palette_2.reg32 = WINC_B_COLOR_PALETTE_2_RESET;
    win->winc_b_color_palette_3.reg32 = WINC_B_COLOR_PALETTE_3_RESET;
    win->winc_b_color_palette_4.reg32 = WINC_B_COLOR_PALETTE_4_RESET;
    win->winc_b_color_palette_5.reg32 = WINC_B_COLOR_PALETTE_5_RESET;
    win->winc_b_color_palette_6.reg32 = WINC_B_COLOR_PALETTE_6_RESET;
    win->winc_b_color_palette_7.reg32 = WINC_B_COLOR_PALETTE_7_RESET;
    win->winc_b_color_palette_8.reg32 = WINC_B_COLOR_PALETTE_8_RESET;
    win->winc_b_color_palette_9.reg32 = WINC_B_COLOR_PALETTE_9_RESET;
}

static void tegra_dc_win_c_reset(tegra_dc_win_c *win)
{
    win->winc_c_color_palette.reg32 = WINC_C_COLOR_PALETTE_RESET;
    win->winc_c_palette_color_ext.reg32 = WINC_C_PALETTE_COLOR_EXT_RESET;
    win->winc_c_h_filter_p00.reg32 = WINC_C_H_FILTER_P00_RESET;
    win->winc_c_h_filter_p01.reg32 = WINC_C_H_FILTER_P01_RESET;
    win->winc_c_h_filter_p02.reg32 = WINC_C_H_FILTER_P02_RESET;
    win->winc_c_h_filter_p03.reg32 = WINC_C_H_FILTER_P03_RESET;
    win->winc_c_h_filter_p04.reg32 = WINC_C_H_FILTER_P04_RESET;
    win->winc_c_h_filter_p05.reg32 = WINC_C_H_FILTER_P05_RESET;
    win->winc_c_h_filter_p06.reg32 = WINC_C_H_FILTER_P06_RESET;
    win->winc_c_h_filter_p07.reg32 = WINC_C_H_FILTER_P07_RESET;
    win->winc_c_h_filter_p08.reg32 = WINC_C_H_FILTER_P08_RESET;
    win->winc_c_h_filter_p09.reg32 = WINC_C_H_FILTER_P09_RESET;
    win->winc_c_h_filter_p0a.reg32 = WINC_C_H_FILTER_P0A_RESET;
    win->winc_c_h_filter_p0b.reg32 = WINC_C_H_FILTER_P0B_RESET;
    win->winc_c_h_filter_p0c.reg32 = WINC_C_H_FILTER_P0C_RESET;
    win->winc_c_h_filter_p0d.reg32 = WINC_C_H_FILTER_P0D_RESET;
    win->winc_c_h_filter_p0e.reg32 = WINC_C_H_FILTER_P0E_RESET;
    win->winc_c_h_filter_p0f.reg32 = WINC_C_H_FILTER_P0F_RESET;
    win->winc_c_csc_yof.reg32 = WINC_C_CSC_YOF_RESET;
    win->winc_c_csc_kyrgb.reg32 = WINC_C_CSC_KYRGB_RESET;
    win->winc_c_csc_kur.reg32 = WINC_C_CSC_KUR_RESET;
    win->winc_c_csc_kvr.reg32 = WINC_C_CSC_KVR_RESET;
    win->winc_c_csc_kug.reg32 = WINC_C_CSC_KUG_RESET;
    win->winc_c_csc_kvg.reg32 = WINC_C_CSC_KVG_RESET;
    win->winc_c_csc_kub.reg32 = WINC_C_CSC_KUB_RESET;
    win->winc_c_csc_kvb.reg32 = WINC_C_CSC_KVB_RESET;
    win->win_c_win_options.reg32 = WIN_C_WIN_OPTIONS_RESET;
    win->win_c_byte_swap.reg32 = WIN_C_BYTE_SWAP_RESET;
    win->win_c_buffer_control.reg32 = WIN_C_BUFFER_CONTROL_RESET;
    win->win_c_color_depth.reg32 = WIN_C_COLOR_DEPTH_RESET;
    win->win_c_position.reg32 = WIN_C_POSITION_RESET;
    win->win_c_size.reg32 = WIN_C_SIZE_RESET;
    win->win_c_prescaled_size.reg32 = WIN_C_PRESCALED_SIZE_RESET;
    win->win_c_h_initial_dda.reg32 = WIN_C_H_INITIAL_DDA_RESET;
    win->win_c_v_initial_dda.reg32 = WIN_C_V_INITIAL_DDA_RESET;
    win->win_c_dda_increment.reg32 = WIN_C_DDA_INCREMENT_RESET;
    win->win_c_line_stride.reg32 = WIN_C_LINE_STRIDE_RESET;
    win->win_c_buf_stride.reg32 = WIN_C_BUF_STRIDE_RESET;
    win->win_c_uv_buf_stride.reg32 = WIN_C_UV_BUF_STRIDE_RESET;
    win->win_c_buffer_addr_mode.reg32 = WIN_C_BUFFER_ADDR_MODE_RESET;
    win->win_c_dv_control.reg32 = WIN_C_DV_CONTROL_RESET;
    win->win_c_blend_nokey.reg32 = WIN_C_BLEND_NOKEY_RESET;
    win->win_c_blend_1win.reg32 = WIN_C_BLEND_1WIN_RESET;
    win->win_c_blend_2win_a.reg32 = WIN_C_BLEND_2WIN_A_RESET;
    win->win_c_blend_2win_b.reg32 = WIN_C_BLEND_2WIN_B_RESET;
    win->win_c_blend_3win_ab.reg32 = WIN_C_BLEND_3WIN_AB_RESET;
    win->win_c_hp_fetch_control.reg32 = WIN_C_HP_FETCH_CONTROL_RESET;
    win->winbuf_c_start_addr.reg32 = WINBUF_C_START_ADDR_RESET;
    win->winbuf_c_start_addr_ns.reg32 = WINBUF_C_START_ADDR_NS_RESET;
    win->winbuf_c_start_addr_u.reg32 = WINBUF_C_START_ADDR_U_RESET;
    win->winbuf_c_start_addr_u_ns.reg32 = WINBUF_C_START_ADDR_U_NS_RESET;
    win->winbuf_c_start_addr_v.reg32 = WINBUF_C_START_ADDR_V_RESET;
    win->winbuf_c_start_addr_v_ns.reg32 = WINBUF_C_START_ADDR_V_NS_RESET;
    win->winbuf_c_addr_h_offset.reg32 = WINBUF_C_ADDR_H_OFFSET_RESET;
    win->winbuf_c_addr_h_offset_ns.reg32 = WINBUF_C_ADDR_H_OFFSET_NS_RESET;
    win->winbuf_c_addr_v_offset.reg32 = WINBUF_C_ADDR_V_OFFSET_RESET;
    win->winbuf_c_addr_v_offset_ns.reg32 = WINBUF_C_ADDR_V_OFFSET_NS_RESET;
    win->winbuf_c_uflow_status.reg32 = WINBUF_C_UFLOW_STATUS_RESET;
}

static void tegra_dc_priv_reset(DeviceState *dev)
{
    tegra_dc *s = TEGRA_DC(dev);

    s->cmd_general_incr_syncpt.reg32 = CMD_GENERAL_INCR_SYNCPT_RESET;
    s->cmd_general_incr_syncpt_cntrl.reg32 = CMD_GENERAL_INCR_SYNCPT_CNTRL_RESET;
    s->cmd_general_incr_syncpt_error.reg32 = CMD_GENERAL_INCR_SYNCPT_ERROR_RESET;
    s->cmd_win_a_incr_syncpt.reg32 = CMD_WIN_A_INCR_SYNCPT_RESET;
    s->cmd_win_a_incr_syncpt_cntrl.reg32 = CMD_WIN_A_INCR_SYNCPT_CNTRL_RESET;
    s->cmd_win_a_incr_syncpt_error.reg32 = CMD_WIN_A_INCR_SYNCPT_ERROR_RESET;
    s->cmd_win_b_incr_syncpt.reg32 = CMD_WIN_B_INCR_SYNCPT_RESET;
    s->cmd_win_b_incr_syncpt_cntrl.reg32 = CMD_WIN_B_INCR_SYNCPT_CNTRL_RESET;
    s->cmd_win_b_incr_syncpt_error.reg32 = CMD_WIN_B_INCR_SYNCPT_ERROR_RESET;
    s->cmd_win_c_incr_syncpt.reg32 = CMD_WIN_C_INCR_SYNCPT_RESET;
    s->cmd_win_c_incr_syncpt_cntrl.reg32 = CMD_WIN_C_INCR_SYNCPT_CNTRL_RESET;
    s->cmd_win_c_incr_syncpt_error.reg32 = CMD_WIN_C_INCR_SYNCPT_ERROR_RESET;
    s->cmd_cont_syncpt_vsync.reg32 = CMD_CONT_SYNCPT_VSYNC_RESET;
    s->cmd_ctxsw.reg32 = CMD_CTXSW_RESET;
    s->cmd_display_command_option0.reg32 = CMD_DISPLAY_COMMAND_OPTION0_RESET;
    s->cmd_display_command.reg32 = CMD_DISPLAY_COMMAND_RESET;
    s->cmd_signal_raise.reg32 = CMD_SIGNAL_RAISE_RESET;
    s->cmd_display_power_control.reg32 = CMD_DISPLAY_POWER_CONTROL_RESET;
    s->cmd_int_status.reg32 = CMD_INT_STATUS_RESET;
    s->cmd_int_mask.reg32 = CMD_INT_MASK_RESET;
    s->cmd_int_enable.reg32 = CMD_INT_ENABLE_RESET;
    s->cmd_int_type.reg32 = CMD_INT_TYPE_RESET;
    s->cmd_int_polarity.reg32 = CMD_INT_POLARITY_RESET;
    s->cmd_signal_raise1.reg32 = CMD_SIGNAL_RAISE1_RESET;
    s->cmd_signal_raise2.reg32 = CMD_SIGNAL_RAISE2_RESET;
    s->cmd_signal_raise3.reg32 = CMD_SIGNAL_RAISE3_RESET;
    s->cmd_state_access.reg32 = CMD_STATE_ACCESS_RESET;
    s->cmd_state_control.reg32 = CMD_STATE_CONTROL_RESET;
    s->cmd_display_window_header.reg32 = CMD_DISPLAY_WINDOW_HEADER_RESET;
    s->cmd_reg_act_control.reg32 = CMD_REG_ACT_CONTROL_RESET;
    s->com_crc_control.reg32 = COM_CRC_CONTROL_RESET;
    s->com_crc_checksum.reg32 = COM_CRC_CHECKSUM_RESET;
    s->com_pin_output_enable0.reg32 = COM_PIN_OUTPUT_ENABLE0_RESET;
    s->com_pin_output_enable1.reg32 = COM_PIN_OUTPUT_ENABLE1_RESET;
    s->com_pin_output_enable2.reg32 = COM_PIN_OUTPUT_ENABLE2_RESET;
    s->com_pin_output_enable3.reg32 = COM_PIN_OUTPUT_ENABLE3_RESET;
    s->com_pin_output_polarity0.reg32 = COM_PIN_OUTPUT_POLARITY0_RESET;
    s->com_pin_output_polarity1.reg32 = COM_PIN_OUTPUT_POLARITY1_RESET;
    s->com_pin_output_polarity2.reg32 = COM_PIN_OUTPUT_POLARITY2_RESET;
    s->com_pin_output_polarity3.reg32 = COM_PIN_OUTPUT_POLARITY3_RESET;
    s->com_pin_output_data0.reg32 = COM_PIN_OUTPUT_DATA0_RESET;
    s->com_pin_output_data1.reg32 = COM_PIN_OUTPUT_DATA1_RESET;
    s->com_pin_output_data2.reg32 = COM_PIN_OUTPUT_DATA2_RESET;
    s->com_pin_output_data3.reg32 = COM_PIN_OUTPUT_DATA3_RESET;
    s->com_pin_input_enable0.reg32 = COM_PIN_INPUT_ENABLE0_RESET;
    s->com_pin_input_enable1.reg32 = COM_PIN_INPUT_ENABLE1_RESET;
    s->com_pin_input_enable2.reg32 = COM_PIN_INPUT_ENABLE2_RESET;
    s->com_pin_input_enable3.reg32 = COM_PIN_INPUT_ENABLE3_RESET;
    s->com_pin_input_data0.reg32 = COM_PIN_INPUT_DATA0_RESET;
    s->com_pin_input_data1.reg32 = COM_PIN_INPUT_DATA1_RESET;
    s->com_pin_output_select0.reg32 = COM_PIN_OUTPUT_SELECT0_RESET;
    s->com_pin_output_select1.reg32 = COM_PIN_OUTPUT_SELECT1_RESET;
    s->com_pin_output_select2.reg32 = COM_PIN_OUTPUT_SELECT2_RESET;
    s->com_pin_output_select3.reg32 = COM_PIN_OUTPUT_SELECT3_RESET;
    s->com_pin_output_select4.reg32 = COM_PIN_OUTPUT_SELECT4_RESET;
    s->com_pin_output_select5.reg32 = COM_PIN_OUTPUT_SELECT5_RESET;
    s->com_pin_output_select6.reg32 = COM_PIN_OUTPUT_SELECT6_RESET;
    s->com_pin_misc_control.reg32 = COM_PIN_MISC_CONTROL_RESET;
    s->com_pm0_control.reg32 = COM_PM0_CONTROL_RESET;
    s->com_pm0_duty_cycle.reg32 = COM_PM0_DUTY_CYCLE_RESET;
    s->com_pm1_control.reg32 = COM_PM1_CONTROL_RESET;
    s->com_pm1_duty_cycle.reg32 = COM_PM1_DUTY_CYCLE_RESET;
    s->com_spi_control.reg32 = COM_SPI_CONTROL_RESET;
    s->com_spi_start_byte.reg32 = COM_SPI_START_BYTE_RESET;
    s->com_hspi_write_data_ab.reg32 = COM_HSPI_WRITE_DATA_AB_RESET;
    s->com_hspi_write_data_cd.reg32 = COM_HSPI_WRITE_DATA_CD_RESET;
    s->com_hspi_cs_dc.reg32 = COM_HSPI_CS_DC_RESET;
    s->com_scratch_register_a.reg32 = COM_SCRATCH_REGISTER_A_RESET;
    s->com_scratch_register_b.reg32 = COM_SCRATCH_REGISTER_B_RESET;
    s->com_gpio_ctrl.reg32 = COM_GPIO_CTRL_RESET;
    s->com_gpio_debounce_counter.reg32 = COM_GPIO_DEBOUNCE_COUNTER_RESET;
    s->com_crc_checksum_latched.reg32 = COM_CRC_CHECKSUM_LATCHED_RESET;
    s->disp_disp_signal_options0.reg32 = DISP_DISP_SIGNAL_OPTIONS0_RESET;
    s->disp_disp_signal_options1.reg32 = DISP_DISP_SIGNAL_OPTIONS1_RESET;
    s->disp_disp_win_options.reg32 = DISP_DISP_WIN_OPTIONS_RESET;
    s->disp_mem_high_priority.reg32 = DISP_MEM_HIGH_PRIORITY_RESET;
    s->disp_mem_high_priority_timer.reg32 = DISP_MEM_HIGH_PRIORITY_TIMER_RESET;
    s->disp_disp_timing_options.reg32 = DISP_DISP_TIMING_OPTIONS_RESET;
    s->disp_ref_to_sync.reg32 = DISP_REF_TO_SYNC_RESET;
    s->disp_sync_width.reg32 = DISP_SYNC_WIDTH_RESET;
    s->disp_back_porch.reg32 = DISP_BACK_PORCH_RESET;
    s->disp_disp_active.reg32 = DISP_DISP_ACTIVE_RESET;
    s->disp_front_porch.reg32 = DISP_FRONT_PORCH_RESET;
    s->disp_h_pulse0_control.reg32 = DISP_H_PULSE0_CONTROL_RESET;
    s->disp_h_pulse0_position_a.reg32 = DISP_H_PULSE0_POSITION_A_RESET;
    s->disp_h_pulse0_position_b.reg32 = DISP_H_PULSE0_POSITION_B_RESET;
    s->disp_h_pulse0_position_c.reg32 = DISP_H_PULSE0_POSITION_C_RESET;
    s->disp_h_pulse0_position_d.reg32 = DISP_H_PULSE0_POSITION_D_RESET;
    s->disp_h_pulse1_control.reg32 = DISP_H_PULSE1_CONTROL_RESET;
    s->disp_h_pulse1_position_a.reg32 = DISP_H_PULSE1_POSITION_A_RESET;
    s->disp_h_pulse1_position_b.reg32 = DISP_H_PULSE1_POSITION_B_RESET;
    s->disp_h_pulse1_position_c.reg32 = DISP_H_PULSE1_POSITION_C_RESET;
    s->disp_h_pulse1_position_d.reg32 = DISP_H_PULSE1_POSITION_D_RESET;
    s->disp_h_pulse2_control.reg32 = DISP_H_PULSE2_CONTROL_RESET;
    s->disp_h_pulse2_position_a.reg32 = DISP_H_PULSE2_POSITION_A_RESET;
    s->disp_h_pulse2_position_b.reg32 = DISP_H_PULSE2_POSITION_B_RESET;
    s->disp_h_pulse2_position_c.reg32 = DISP_H_PULSE2_POSITION_C_RESET;
    s->disp_h_pulse2_position_d.reg32 = DISP_H_PULSE2_POSITION_D_RESET;
    s->disp_v_pulse0_control.reg32 = DISP_V_PULSE0_CONTROL_RESET;
    s->disp_v_pulse0_position_a.reg32 = DISP_V_PULSE0_POSITION_A_RESET;
    s->disp_v_pulse0_position_b.reg32 = DISP_V_PULSE0_POSITION_B_RESET;
    s->disp_v_pulse0_position_c.reg32 = DISP_V_PULSE0_POSITION_C_RESET;
    s->disp_v_pulse1_control.reg32 = DISP_V_PULSE1_CONTROL_RESET;
    s->disp_v_pulse1_position_a.reg32 = DISP_V_PULSE1_POSITION_A_RESET;
    s->disp_v_pulse1_position_b.reg32 = DISP_V_PULSE1_POSITION_B_RESET;
    s->disp_v_pulse1_position_c.reg32 = DISP_V_PULSE1_POSITION_C_RESET;
    s->disp_v_pulse2_control.reg32 = DISP_V_PULSE2_CONTROL_RESET;
    s->disp_v_pulse2_position_a.reg32 = DISP_V_PULSE2_POSITION_A_RESET;
    s->disp_v_pulse3_control.reg32 = DISP_V_PULSE3_CONTROL_RESET;
    s->disp_v_pulse3_position_a.reg32 = DISP_V_PULSE3_POSITION_A_RESET;
    s->disp_m0_control.reg32 = DISP_M0_CONTROL_RESET;
    s->disp_m1_control.reg32 = DISP_M1_CONTROL_RESET;
    s->disp_di_control.reg32 = DISP_DI_CONTROL_RESET;
    s->disp_pp_control.reg32 = DISP_PP_CONTROL_RESET;
    s->disp_pp_select_a.reg32 = DISP_PP_SELECT_A_RESET;
    s->disp_pp_select_b.reg32 = DISP_PP_SELECT_B_RESET;
    s->disp_pp_select_c.reg32 = DISP_PP_SELECT_C_RESET;
    s->disp_pp_select_d.reg32 = DISP_PP_SELECT_D_RESET;
    s->disp_disp_clock_control.reg32 = DISP_DISP_CLOCK_CONTROL_RESET;
    s->disp_disp_interface_control.reg32 = DISP_DISP_INTERFACE_CONTROL_RESET;
    s->disp_disp_color_control.reg32 = DISP_DISP_COLOR_CONTROL_RESET;
    s->disp_shift_clock_options.reg32 = DISP_SHIFT_CLOCK_OPTIONS_RESET;
    s->disp_data_enable_options.reg32 = DISP_DATA_ENABLE_OPTIONS_RESET;
    s->disp_serial_interface_options.reg32 = DISP_SERIAL_INTERFACE_OPTIONS_RESET;
    s->disp_lcd_spi_options.reg32 = DISP_LCD_SPI_OPTIONS_RESET;
    s->disp_border_color.reg32 = DISP_BORDER_COLOR_RESET;
    s->disp_color_key0_lower.reg32 = DISP_COLOR_KEY0_LOWER_RESET;
    s->disp_color_key0_upper.reg32 = DISP_COLOR_KEY0_UPPER_RESET;
    s->disp_color_key1_lower.reg32 = DISP_COLOR_KEY1_LOWER_RESET;
    s->disp_color_key1_upper.reg32 = DISP_COLOR_KEY1_UPPER_RESET;
    s->disp_cursor_foreground.reg32 = DISP_CURSOR_FOREGROUND_RESET;
    s->disp_cursor_background.reg32 = DISP_CURSOR_BACKGROUND_RESET;
    s->disp_cursor_start_addr.reg32 = DISP_CURSOR_START_ADDR_RESET;
    s->disp_cursor_start_addr_ns.reg32 = DISP_CURSOR_START_ADDR_NS_RESET;
    s->disp_cursor_position.reg32 = DISP_CURSOR_POSITION_RESET;
    s->disp_cursor_position_ns.reg32 = DISP_CURSOR_POSITION_NS_RESET;
    s->disp_init_seq_control.reg32 = DISP_INIT_SEQ_CONTROL_RESET;
    s->disp_spi_init_seq_data_a.reg32 = DISP_SPI_INIT_SEQ_DATA_A_RESET;
    s->disp_spi_init_seq_data_b.reg32 = DISP_SPI_INIT_SEQ_DATA_B_RESET;
    s->disp_spi_init_seq_data_c.reg32 = DISP_SPI_INIT_SEQ_DATA_C_RESET;
    s->disp_spi_init_seq_data_d.reg32 = DISP_SPI_INIT_SEQ_DATA_D_RESET;
    s->disp_dc_mccif_fifoctrl.reg32 = DISP_DC_MCCIF_FIFOCTRL_RESET;
    s->disp_mccif_display0a_hyst.reg32 = DISP_MCCIF_DISPLAY0A_HYST_RESET;
    s->disp_mccif_display0b_hyst.reg32 = DISP_MCCIF_DISPLAY0B_HYST_RESET;
    s->disp_mccif_display0c_hyst.reg32 = DISP_MCCIF_DISPLAY0C_HYST_RESET;
    s->disp_mccif_display1b_hyst.reg32 = DISP_MCCIF_DISPLAY1B_HYST_RESET;
    s->disp_dac_crt_ctrl.reg32 = DISP_DAC_CRT_CTRL_RESET;
    s->disp_disp_misc_control.reg32 = DISP_DISP_MISC_CONTROL_RESET;
    s->disp_sd_control.reg32 = DISP_SD_CONTROL_RESET;
    s->disp_sd_csc_coeff.reg32 = DISP_SD_CSC_COEFF_RESET;
    s->disp_sd_lut.reg32 = DISP_SD_LUT_RESET;
    s->disp_sd_lut_1.reg32 = DISP_SD_LUT_1_RESET;
    s->disp_sd_lut_2.reg32 = DISP_SD_LUT_2_RESET;
    s->disp_sd_lut_3.reg32 = DISP_SD_LUT_3_RESET;
    s->disp_sd_lut_4.reg32 = DISP_SD_LUT_4_RESET;
    s->disp_sd_lut_5.reg32 = DISP_SD_LUT_5_RESET;
    s->disp_sd_lut_6.reg32 = DISP_SD_LUT_6_RESET;
    s->disp_sd_lut_7.reg32 = DISP_SD_LUT_7_RESET;
    s->disp_sd_lut_8.reg32 = DISP_SD_LUT_8_RESET;
    s->disp_sd_flicker_control.reg32 = DISP_SD_FLICKER_CONTROL_RESET;
    s->disp_sd_pixel_count.reg32 = DISP_SD_PIXEL_COUNT_RESET;
    s->disp_sd_histogram.reg32 = DISP_SD_HISTOGRAM_RESET;
    s->disp_sd_histogram_1.reg32 = DISP_SD_HISTOGRAM_1_RESET;
    s->disp_sd_histogram_2.reg32 = DISP_SD_HISTOGRAM_2_RESET;
    s->disp_sd_histogram_3.reg32 = DISP_SD_HISTOGRAM_3_RESET;
    s->disp_sd_histogram_4.reg32 = DISP_SD_HISTOGRAM_4_RESET;
    s->disp_sd_histogram_5.reg32 = DISP_SD_HISTOGRAM_5_RESET;
    s->disp_sd_histogram_6.reg32 = DISP_SD_HISTOGRAM_6_RESET;
    s->disp_sd_histogram_7.reg32 = DISP_SD_HISTOGRAM_7_RESET;
    s->disp_sd_bl_parameters.reg32 = DISP_SD_BL_PARAMETERS_RESET;
    s->disp_sd_bl_tf.reg32 = DISP_SD_BL_TF_RESET;
    s->disp_sd_bl_tf_1.reg32 = DISP_SD_BL_TF_1_RESET;
    s->disp_sd_bl_tf_2.reg32 = DISP_SD_BL_TF_2_RESET;
    s->disp_sd_bl_tf_3.reg32 = DISP_SD_BL_TF_3_RESET;
    s->disp_sd_bl_control.reg32 = DISP_SD_BL_CONTROL_RESET;

    tegra_dc_win_a_reset(&s->win_a_assemly);
    tegra_dc_win_a_reset(&s->win_a_armed);

    tegra_dc_win_b_reset(&s->win_b_assemly);
    tegra_dc_win_b_reset(&s->win_b_armed);

    tegra_dc_win_c_reset(&s->win_c_assemly);
    tegra_dc_win_c_reset(&s->win_c_armed);
}

static void tegra_dc_compose(void *opaque)
{
    tegra_dc *s = opaque;

    if (s->win_a_armed.win_a_win_options.win_enable)
        pixman_image_composite(PIXMAN_OP_SRC,
                               s->win_a_surf->image, NULL,
                               qemu_console_surface(s->con)->image,
                               0, 0, 0, 0,
                               s->win_a_armed.win_a_position.h_position,
                               s->win_a_armed.win_a_position.v_position,
                               s->win_a_armed.win_a_size.h_size,
                               s->win_a_armed.win_a_size.v_size);

    if (s->win_b_armed.win_b_win_options.win_enable)
        pixman_image_composite(PIXMAN_OP_SRC,
                               s->win_b_surf->image, NULL,
                               qemu_console_surface(s->con)->image,
                               0, 0, 0, 0,
                               s->win_b_armed.win_b_position.h_position,
                               s->win_b_armed.win_b_position.v_position,
                               s->win_b_armed.win_b_size.h_size,
                               s->win_b_armed.win_b_size.v_size);

    dpy_gfx_update(s->con, 0, 0, s->disp_width, s->disp_height);
}

static const GraphicHwOps tegra_dc_ops = {
    .gfx_update = tegra_dc_compose,
};

static const MemoryRegionOps tegra_dc_mem_ops = {
    .read = tegra_dc_priv_read,
    .write = tegra_dc_priv_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static int tegra_dc_priv_init(SysBusDevice *dev)
{
    tegra_dc *s = TEGRA_DC(dev);

    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq);

    memory_region_init_io(&s->iomem, OBJECT(dev), &tegra_dc_mem_ops, s,
                          "tegra.dc", TEGRA_DISPLAY_SIZE);
    sysbus_init_mmio(dev, &s->iomem);

    s->con = graphic_console_init(DEVICE(dev), 0, &tegra_dc_ops, s);
    qemu_console_resize(s->con, s->disp_width, s->disp_height);

    return 0;
}

static Property tegra_dc_properties[] = {
    DEFINE_PROP_UINT32("display_width",  tegra_dc, disp_width, 1366),
    DEFINE_PROP_UINT32("display_height", tegra_dc, disp_height, 768),
    DEFINE_PROP_END_OF_LIST(),
};

static void tegra_dc_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    k->init = tegra_dc_priv_init;
    dc->vmsd = &vmstate_tegra_dc;
    dc->reset = tegra_dc_priv_reset;
    dc->props = tegra_dc_properties;
}

static const TypeInfo tegra_dc_info = {
    .name = TYPE_TEGRA_DC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(tegra_dc),
    .class_init = tegra_dc_class_init,
};

static void tegra_dc_register_types(void)
{
    type_register_static(&tegra_dc_info);
}

type_init(tegra_dc_register_types)
