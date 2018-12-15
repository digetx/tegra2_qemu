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

#ifndef TEGRA_WIN_COMMON_H
#define TEGRA_WIN_COMMON_H

#include "regs.h"

#define WIN_OPTIONS_OFFSET 0x700
#define WIN_OPTIONS_RESET  0x00000000
typedef union win_options_u {
    struct {
        unsigned int h_direction:1;         /* Window B Horizontal (X) drawing Direction  0 = INCREMENT 1 = DECREMENT */
        unsigned int undefined_bit_1:1;
        unsigned int v_direction:1;         /* Window B Vertical (Y) drawing Direction  0 = INCREMENT 1 = DECREMENT */
        unsigned int undefined_bits_3_5:3;
        unsigned int color_expand:1;        /* Window B 12/15/16/18-to-24 bpp color expansion This bit should be enabled only for 12-bpp B4G4R4A4, 15-bpp B5G5R5A, 16-bpp B5G6R5, 18-bpp B6G6R6 color modes. If enabled the color conversion is performed prior to horizontal scaling.  0 = DISABLE 1 = ENABLE */
        unsigned int undefined_bit_7:1;
        unsigned int h_filter_enable:1;     /* Window B H Filter Enable This controls H scaling filter and is effective only for non-palletized color modes.  0 = DISABLE 1 = ENABLE */
        unsigned int undefined_bit_9:1;
        unsigned int v_filter_enable:1;     /* Window B V Filter Enable This controls V scaling filter and is effective only for non-palletized color modes. If V filter is disabled, only one line is read from memory for each output line.  0 = DISABLE 1 = ENABLE */
        unsigned int undefined_bit_11:1;
        unsigned int v_filter_optimize:1;   /* Window B V Filter Optimization This is effective only when vertical scaling filter is enabled. This can be used (enabled) to temporarily disable the vertical scaling filter when the vertical scaling DDA fraction is zero. In this case the next line is not fetched from memory to save bandwidth and power. This feature cannot be used in 420P/422R/422RA formats.  0 = DISABLE 1 = ENABLE */
        unsigned int undefined_bit_13:1;
        unsigned int v_filter_uv_align:1;   /* Window B V Filter UV Alignment This is effective only when vertical scaling filter is enabled and only on these formats YCbCr420P, YUV420P, YCbCr422R, YUV422R, YCbCr422RA YUV422RA. When UV alignment is enabled, the chroma components are aligned to the even number of luma component lines. When disabled the chroma components are aligned to half a pixel below the corresponding even number of luma component lines. It is usually disabled unless the incoming video stream specifically indicates otherwise.  0 = DISABLE 1 = ENABLE */
        unsigned int undefined_bit_15:1;
        unsigned int cp_enable:1;           /* Window B Color Palette Enable This controls the color palette and should be enabled for palletized color modes. For non-palletized color modes, the color palette can be enabled for gamma correction.  0 = DISABLE 1 = ENABLE */
        unsigned int undefined_bit_17:1;
        unsigned int csc_enable:1;          /* Window B Color Space Conversion Enable This controls the color space conversion and should be enabled for YCbCr/YUV color modes for conversion to B8G8R8 and for hue and saturation control. This can also be used for gain control for RGB color modes  0 = DISABLE 1 = ENABLE */
        unsigned int undefined_bit_19:1;
        unsigned int dv_enable:1;           /* Window B Digital Vibrance Enable  0 = DISABLE 1 = ENABLE */
        unsigned int undefined_bit_21:1;
        unsigned int yuv_range_expand:1;    /* Window B Enable range expansion in the cases where RANGEREDFRM is 1 from mpd. Formula: Y = clip(( Y-128)*2 + 128); Cb = clip((Cb-128)*2 + 128); Cr = clip((Cr-128)*2 + 128); where clip() function clips between 0 and 255. 0= disable 1= enable 0 = DISABLE 1 = ENABLE */
        unsigned int undefined_bits_23_29:7;
        unsigned int win_enable:1;          /* Window B Window enable  0 = DISABLE 1 = ENABLE */
        unsigned int undefined_bit_31:1;
    };

    uint32_t reg32;
} win_options_t;

#define WIN_BYTE_SWAP_OFFSET 0x701
#define WIN_BYTE_SWAP_RESET  0x00000000
typedef union win_byte_swap_u {
    struct {
        unsigned int byte_swap:2;           /* Window B Byte Swap This controls byte swap of frame data read from memory prior to any data processing in the display module. 00= no byte swap (3 2 1 0) 01= byte swap for each 2-byte word (2 3 0 1) 10= byte swap for each 4-byte word (0 1 2 3) 11= word swap for each 4-byte word (1 0 3 2) 0 = NOSWAP 1 = SWAP2 2 = SWAP4 3 = SWAP4HW */
        unsigned int undefined_bits_2_31:30;
    };

    uint32_t reg32;
} win_byte_swap_t;

#define WIN_BUFFER_CONTROL_OFFSET 0x702
#define WIN_BUFFER_CONTROL_RESET  0x00000000
typedef union win_buffer_control_u {
    struct {
        unsigned int buffer_control:3;      /* Window B Buffer Control 0= Host (software) controlled 1= Video Input controlled 2= Encoder Pre-Processor controlled 3= MPEG Encoder controlled 4= StretchBLT or 2D other= reserved If window buffer selection is not controlled by host (software) then buffer start indexes are sent by the respective module specified by this parameter, and in this case, the buffer start address registers are used to specify frame stride and buffer offset for the calculated start address. 0 = HOST 1 = VI 2 = EPP 4 = SB2D 3 = MPEGE */
        unsigned int undefined_bits_3_31:29;
    };

    uint32_t reg32;
} win_buffer_control_t;

#define WIN_COLOR_DEPTH_OFFSET 0x703
#define WIN_COLOR_DEPTH_RESET  0x00000000
typedef union win_color_depth_u {
    struct {
        unsigned int color_depth:5;         /* Window B Color Depth Supported color depths are: P8 = 8-bpp (palletized) B4G4R4A4 = 12-bpp B4G4R4 B5G5R5A = 15-bpp B5G5R5 AB5G5R5 = 15-bpp B5G5R5 B5G6R5 = 16-bpp B5G6R5 B8G8R8A8 = 32-bpp B8G8R8A8 R8G8B8A8 = 32-bpp R8G8B8A8 B6x2G6x2R6x2A8 = 32-bpp B6G6R6A8 R6x2G6x2B6x2A8 = 32-bpp R6G6B6A8 YCbCr422 = 16-bpp YCbCr422 packed YUV422 = 16-bpp YUV422 YCbCr420P = 16-bpp YCbCr420 planar YUV420P = 16-bpp YUV420 planar YCbCr422P = 16-bpp YCbCr422 planar YUV422P = 16-bpp YUV422 planar YCbCr422R = 16-bpp YCbCr422 rotated planar YUV422R = 16-bpp YUV422 rotated planar YCbCr422RA= 16-bpp YCbCr422 rotated planar with chroma averaging YUV422RA = 16-bpp YUV422 rotated planar with chroma averaging 0 = P1 1 = P2 2 = P4 3 = P8 4 = B4G4R4A4 5 = B5G5R5A 6 = B5G6R5 7 = AB5G5R5 12 = B8G8R8A8 13 = R8G8B8A8 14 = B6x2G6x2R6x2A8 15 = R6x2G6x2B6x2A8 16 = YCbCr422 17 = YUV422 18 = YCbCr420P 19 = YUV420P 20 = YCbCr422P 21 = YUV422P 22 = YCbCr422R 23 = YUV422R 24 = YCbCr422RA 25 = YUV422RA */
        unsigned int undefined_bits_5_31:27;
    };

    uint32_t reg32;
} win_color_depth_t;

#define WIN_POSITION_OFFSET 0x704
#define WIN_POSITION_RESET  0x00000000
typedef union win_position_u {
    struct {
        unsigned int h_position:13;         /* Window B H Position This is specified with respect to the left edge of active display area */
        unsigned int undefined_bits_13_15:3;
        unsigned int v_position:13;         /* Window B V Position This is specified with respect to the top edge of active display area */
        unsigned int undefined_bits_29_31:3;
    };

    uint32_t reg32;
} win_position_t;

#define WIN_SIZE_OFFSET 0x705
#define WIN_SIZE_RESET  0x00000000
typedef union win_size_u {
    struct {
        unsigned int h_size:13;             /* Window B H Size (pixels) This is the horizontal size after scaling */
        unsigned int undefined_bits_13_15:3;
        unsigned int v_size:13;             /* Window B V Size (lines) This is the vertical size after scaling */
        unsigned int undefined_bits_29_31:3;
    };

    uint32_t reg32;
} win_size_t;

#define WIN_PRESCALED_SIZE_OFFSET 0x706
#define WIN_PRESCALED_SIZE_RESET  0x00000000
typedef union win_prescaled_size_u {
    struct {
        unsigned int h_prescaled_size:15;   /* Window B H Pre-scaled Size (bytes) In 420P and 422P formats, it must be even */
        unsigned int undefined_bit_15:1;
        unsigned int v_prescaled_size:13;   /* Window B V Pre-scaled Size (lines) In 420P/422R/422RA formats, it must be even */
        unsigned int undefined_bits_29_31:3;
    };

    uint32_t reg32;
} win_prescaled_size_t;

#define WIN_H_INITIAL_DDA_OFFSET 0x707
#define WIN_H_INITIAL_DDA_RESET  0x00000000
typedef union win_h_initial_dda_u {
    struct {
        unsigned int h_initial_dda:16;      /* Window B H Initial DDA (4.12) This is typically programmed to 0.0 */
        unsigned int undefined_bits_16_31:16;
    };

    uint32_t reg32;
} win_h_initial_dda_t;

#define WIN_V_INITIAL_DDA_OFFSET 0x708
#define WIN_V_INITIAL_DDA_RESET  0x00000000
typedef union win_v_initial_dda_u {
    struct {
        unsigned int v_initial_dda:16;      /* Window B V Initial DDA (4.12) This is typically programmed to 0.0 for both non-interlaced and interlaced sources */
        unsigned int undefined_bits_16_31:16;
    };

    uint32_t reg32;
} win_v_initial_dda_t;

#define WIN_DDA_INCREMENT_OFFSET 0x709
#define WIN_DDA_INCREMENT_RESET  0x00000000
typedef union win_dda_increment_u {
    struct {
        unsigned int h_dda_increment:16;    /* Window B Horizontal DDA Increment (4.12) This should be set to 1.0 if there is no scaling. The maximum value for downscaling depends on the number of bytes per pixel. For 4-byte/pixel modes (32-bpp) the maximum value is 4.0 and for all other modes the maximum value is 8.0 */
        unsigned int v_dda_increment:16;    /* Window B Vertical DDA Increment (4.12) This should be set to 1.0 if there is no scaling. Maximum value is 15.0 regardless of the number of bytes per pixel */
    };

    uint32_t reg32;
} win_dda_increment_t;

#define WIN_LINE_STRIDE_OFFSET 0x70A
#define WIN_LINE_STRIDE_RESET  0x00000000
typedef union win_line_stride_u {
    struct {
        unsigned int line_stride:16;        /* Window B Line Stride This is stride (in bytes) for all non-planar data formats. If the memory surface is tiled, the stride needs to be a multiple of 16. If H_DIRECTION of window B is set to DECREMENT, the stride also needs to be a multiple of 16. For planar YUV or YCbCr data formats, this is stride (in bytes) for the luma plane with the restriction that it must be multiples of 8 (16 if tiled or in horizontal flipping) For tiled surface this value may affect starting address of a window. Refer to the comment of START_ADDR for detail */
        unsigned int uv_line_stride:16;     /* Window B Line Stride for Chroma This is stride (in bytes) for planar YUV or YCbCr data formats for the chroma plane, with the restriction that it must be programmed to be multiples of 4 (16 if tiled or in horizontal flipping) This is not used (ignored) for other non-planar data formats */
    };

    uint32_t reg32;
} win_line_stride_t;

#define WIN_BUF_STRIDE_OFFSET 0x70B
#define WIN_BUF_STRIDE_RESET  0x00000000
typedef union win_buf_stride_u {
    struct {
        unsigned int buf_stride:32;         /* Window B Buffer stride Buffer stride is used to calculate the buffer addresses when the window is triggered by non-host modules. Refer to the comment of of START_ADDR for programming guide. For YUV planar pixel format, this specifies buffer stride for the Y plane. The value is in bytes */
    };

    uint32_t reg32;
} win_buf_stride_t;

#define WIN_UV_BUF_STRIDE_OFFSET 0x70C
#define WIN_UV_BUF_STRIDE_RESET  0x00000000
typedef union win_uv_buf_stride_u {
    struct {
        unsigned int uv_buf_stride:32;      /* Window B This value is in bytes. For YUV planar pixel format, this specifies buffer stride for the U/V plane */
    };

    uint32_t reg32;
} win_uv_buf_stride_t;

#define WIN_BUFFER_ADDR_MODE_OFFSET 0x70D
#define WIN_BUFFER_ADDR_MODE_RESET  0x00000000
typedef union win_buffer_addr_mode_u {
    struct {
        unsigned int tile_mode:1;           /* Window B Memory surface tiling mode For YUV planar pixel format, this specifies tiling mode for the Y plane. 0 = LINEAR 1 = TILED */
        unsigned int undefined_bits_1_15:15;
        unsigned int uv_tile_mode:1;        /* Window B Memory surface tiling mode For YUV planar pixel format, this specifies tiling mode for the U/V plane. 0 = LINEAR 1 = TILED */
        unsigned int undefined_bits_17_31:15;
    };

    uint32_t reg32;
} win_buffer_addr_mode_t;

#define WIN_BLEND_NOKEY_OFFSET 0x70F
#define WIN_BLEND_NOKEY_RESET  0x00000000
typedef union win_blend_nokey_u {
    struct {
        unsigned int blend_control_nokey:1; /* Window blend control for color key not match areas. 0 = Fix weight using window blend weight 0 for color key not matched. 1 = Alpha weight using the alpha value. This is only valid if the window color format includes an alpha value. For 1-bit alpha value, if the alpha is 0 window blend weight 0 is used and if alpha is 1, window blend weight 1 is used. 0 = FIX_WEIGHT 1 = ALPHA_WEIGHT */
        unsigned int undefined_bits_1_7:7;
        unsigned int blend_weight0_nokey:8; /* Window blend weight 0 for color key not match areas. For alpha weight, this is used for 1-bit alpha with value of 0 */
        unsigned int blend_weight1_nokey:8; /* Window blend weight 1 for color key not match areas. This is used only for alpha weight with 1-bit alpha and alpha value of 1 */
        unsigned int undefined_bits_24_31:8;
    };

    uint32_t reg32;
} win_blend_nokey_t;

#define WIN_BLEND_1WIN_OFFSET 0x710
#define WIN_BLEND_1WIN_RESET  0x00000000
typedef union win_blend_1win_u {
    struct {
        unsigned int ckey_enable_1win:2;    /* Window color key enable 0 = Color Key 0 and 1 Disable 1 = Color Key 0 Enable 2 = Color Key 1 Enable 3 = Color Key 0 and 1 Enable 0 = NOKEY 1 = CKEY0 2 = CKEY1 3 = CKEY01 */
        unsigned int blend_control_1win:1;  /* Window blend control in area where it does not overlap with other windows and either color key disabled or color key enabled with key matched. 0 = Fix weight using the window blend weight 0 if color key disabled or color key 0 enabled with key matched, or using the window blend weight 1 if color key 1 enabled with key matched. 1 = Alpha weight using the alpha value. This is only valid if the window color format includes an alpha value. For 1-bit alpha value, if the alpha is 0 window blend weight 0 is used and if alpha is 1, window blend weight 1 is used. 0 = FIX_WEIGHT 1 = ALPHA_WEIGHT */
        unsigned int undefined_bits_3_7:5;
        unsigned int blend_weight0_1win:8;  /* Window blend weight 0 for color key disabled or color key enabled with key matched areas. For alpha weight, this is used for 1-bit alpha with value of 0 */
        unsigned int blend_weight1_1win:8;  /* Window blend weight 1 for color key disabled or color key enabled with key matched areas. This is used only for alpha weight with 1-bit alpha and alpha value of 1 */
        unsigned int undefined_bits_24_31:8;
    };

    uint32_t reg32;
} win_blend_1win_t;

#define WIN_BLEND_2WIN_B_OFFSET 0x711
#define WIN_BLEND_2WIN_B_RESET  0x00000000
typedef union win_blend_2win_b_u {
    struct {
        unsigned int ckey_enable_2win_a:2;  /* Window color key enable 0 = Color Key 0 and 1 Disable 1 = Color Key 0 Enable 2 = Color Key 1 Enable 3 = Color Key 0 and 1 Enable 0 = NOKEY 1 = CKEY0 2 = CKEY1 3 = CKEY01 */
        unsigned int blend_control_2win_a:2;/* Window blend control in area where either color key disabled or color key enabled with key matched. 0 = Fix weight using the window blend weight 0 if color key disabled or color key 0 enabled with key matched, or using the window blend weight 1 if color key 1 enabled with key matched. 1 = Alpha weight using the alpha value. This is only valid if the window color format includes an alpha value. For 1-bit alpha value, if the alpha is 0 window blend weight 0 is used and if alpha is 1, window blend weight 1 is used. 2 = Dependent weight, in this case, the window blend weight is 1 - the weights of the other window that overlaps with this window. Only 1 of the overlapping windows may have this setting. 0 = FIX_WEIGHT 1 = ALPHA_WEIGHT 2 = DEPENDENT_WEIGHT */
        unsigned int undefined_bits_4_7:4;
        unsigned int blend_weight0_2win_a:8;/* Window blend weight 0 for color key disabled or color key enabled with key matched areas. For alpha weight, this is used for 1-bit alpha with value of 0 */
        unsigned int blend_weight1_2win_a:8;/* Window blend weight 1 for color key disabled or color key enabled with key matched areas. This is used only for alpha weight with 1-bit alpha and alpha value of 1 */
        unsigned int undefined_bits_24_31:8;
    };

    uint32_t reg32;
} win_blend_2win_b_t;

#define WIN_BLEND_2WIN_C_OFFSET 0x712
#define WIN_BLEND_2WIN_C_RESET  0x00000000
typedef union win_blend_2win_c_u {
    struct {
        unsigned int ckey_enable_2win_c:2;  /* Window color key enable 0 = Color Key 0 and 1 Disable 1 = Color Key 0 Enable 2 = Color Key 1 Enable 3 = Color Key 0 and 1 Enable 0 = NOKEY 1 = CKEY0 2 = CKEY1 3 = CKEY01 */
        unsigned int blend_control_2win_c:2;/* Window blend control in area where either color key disabled or color key enabled with key matched. 0 = Fix weight using the window blend weight 0 if color key disabled or color key 0 enabled with key matched, or using the window blend weight 1 if color key 1 enabled with key matched. 1 = Alpha weight using the alpha value. This is only valid if the window color format includes an alpha value. For 1-bit alpha value, if the alpha is 0 window blend weight 0 is used and if alpha is 1, window blend weight 1 is used. 2 = Dependent weight, in this case, the window blend weight is 1 - the weights of the other window that overlaps with this window. Only 1 of the overlapping windows may have this setting. 0 = FIX_WEIGHT 1 = ALPHA_WEIGHT 2 = DEPENDENT_WEIGHT */
        unsigned int undefined_bits_4_7:4;
        unsigned int blend_weight0_2win_c:8;/* Window blend weight 0 for color key disabled or color key enabled with key matched areas. For alpha weight, this is used for 1-bit alpha with value of 0 */
        unsigned int blend_weight1_2win_c:8;/* Window blend weight 1 for color key disabled or color key enabled with key matched areas. This is used only for alpha weight with 1-bit alpha and alpha value of 1 */
        unsigned int undefined_bits_24_31:8;
    };

    uint32_t reg32;
} win_blend_2win_c_t;

#define WIN_BLEND_3WIN_AC_OFFSET 0x713
#define WIN_BLEND_3WIN_AC_RESET  0x00000000
typedef union win_blend_3win_ac_u {
    struct {
        unsigned int ckey_enable_3win_ac:2; /* Window color key enable 0 = Color Key 0 and 1 Disable 1 = Color Key 0 Enable 2 = Color Key 1 Enable 3 = Color Key 0 and 1 Enable 0 = NOKEY 1 = CKEY0 2 = CKEY1 3 = CKEY01 */
        unsigned int blend_control_3win_ac:2;/* Window blend control in area where either color key disabled or color key enabled with key matched. 0 = Fix weight using the window blend weight 0 if color key disabled or color key 0 enabled with key matched, or using the window blend weight 1 if color key 1 enabled with key matched. 1 = Alpha weight using the alpha value. This is only valid if the window color format includes an alpha value. For 1-bit alpha value, if the alpha is 0 window blend weight 0 is used and if alpha is 1, window blend weight 1 is used. 2 = Dependent weight, in this case, the window blend weight is 1 - the weights of the other window that overlaps with this window. Only 1 of the overlapping windows may have this setting. 0 = FIX_WEIGHT 1 = ALPHA_WEIGHT 2 = DEPENDENT_WEIGHT */
        unsigned int undefined_bits_4_7:4;
        unsigned int blend_weight0_3win_ac:8;/* Window blend weight 0 for color key disabled or color key enabled with key matched areas. For alpha weight, this is used for 1-bit alpha with value of 0 */
        unsigned int blend_weight1_3win_ac:8;/* Window blend weight 1 for color key disabled or color key enabled with key matched areas. This is used only for alpha weight with 1-bit alpha and alpha value of 1 */
        unsigned int undefined_bits_24_31:8;
    };

    uint32_t reg32;
} win_blend_3win_ac_t;

#define WIN_HP_FETCH_CONTROL_OFFSET 0x714
#define WIN_HP_FETCH_CONTROL_RESET  0x00000000
typedef union win_hp_fetch_control_u {
    struct {
        unsigned int cycles_per_word:16;    /* Window B clock cycles per memory fetch word. The value of this field is essentially a measure of the data consumption rate for window B. It is computed as follows: B_CYCLES_PER_WORD = B_DDA_INCREMENT.B_H_DDA_INCREMENT / (bytes per pixel) Note that the format for this value is a fixed-point fractional value with 8 bits of integer precision and 8 bits of fractional precision. In other words, it is an '8.8' number. For example, if there is no scaling of the input image, the DDA increment will be 4096. With 32-bit RGBA pixels there will be 4 bytes per pixel, so CYCLES_PER_WORD will be ... 4096 / 4 = 1024, or 4.0 expressed in the 8.8 format. Any scaling performed on the pixels will change the rate at which pixels are consumed. Scaling up will increase the value of DDA increment and will therefore increase the number of cycles between memory fetches. Conversely, scaling down will decrease the value of DDA increment and memory fetches will occur more frequently */
        unsigned int words_per_line:15;     /* Window B memory fetch words per scan line. This value is in memory fetch words: Multiples of 16 bytes for Tegra 2 Processor Series devices. It is computed as follows: B_WORDS_PER_LINE = (B_SIZE.B_H_SIZE * (bytes per pixel) + 15) >> 4 bytes per pixel is determined by the pixel format */
        unsigned int fetch_info_enable:1;   /* Enables the sending of the Window B fetch information. For compatibility with earlier devices, this defaults to DISABLE. 0 = DISABLE : This bit should be enabled only for 12-bpp  1 = ENABLE */
    };

    uint32_t reg32;
} win_hp_fetch_control_t;

#define WINBUF_START_ADDR_OFFSET 0x800
#define WINBUF_START_ADDR_RESET  0x00000000
typedef union winbuf_start_addr_u {
    struct {
        unsigned int start_addr:32;         /* Window B Start Address This is a byte address. The LSB is not used for 16-bpp non-planar pixel format and the last 2 LSB are not used for 32-bpp non-planar pixel format. For YUV planar pixel format, this specifies start address for the Y plane */
    };

    uint32_t reg32;
} winbuf_start_addr_t;

#define WINBUF_START_ADDR_NS_OFFSET 0x801
#define WINBUF_START_ADDR_NS_RESET  0x00000000
typedef union winbuf_start_addr_ns_u {
    struct {
        unsigned int start_addr_ns:32;      /* Window B Shadowed Start Address This is ARM set shadow of Start Address */
    };

    uint32_t reg32;
} winbuf_start_addr_ns_t;

#define WINBUF_START_ADDR_U_OFFSET 0x802
#define WINBUF_START_ADDR_U_RESET  0x00000000
typedef union winbuf_start_addr_u_u {
    struct {
        unsigned int start_addr_u:32;       /* Window B Start Address for U plane This is a byte address */
    };

    uint32_t reg32;
} winbuf_start_addr_u_t;

#define WINBUF_START_ADDR_U_NS_OFFSET 0x803
#define WINBUF_START_ADDR_U_NS_RESET  0x00000000
typedef union winbuf_start_addr_u_ns_u {
    struct {
        unsigned int start_addr_u_ns:32;    /* Window B Shadowed Start Address for U plane This is ARM set shadow register of U start address */
    };

    uint32_t reg32;
} winbuf_start_addr_u_ns_t;

#define WINBUF_START_ADDR_V_OFFSET 0x804
#define WINBUF_START_ADDR_V_RESET  0x00000000
typedef union winbuf_start_addr_v_u {
    struct {
        unsigned int start_addr_v:32;       /* Window B Start Address for V plane This is a byte address */
    };

    uint32_t reg32;
} winbuf_start_addr_v_t;

#define WINBUF_START_ADDR_V_NS_OFFSET 0x805
#define WINBUF_START_ADDR_V_NS_RESET  0x00000000
typedef union winbuf_start_addr_v_ns_u {
    struct {
        unsigned int start_addr_v_ns:32;    /* Window B Shadowed Start Address for V plane This is ARM set shadow register of U start address */
    };

    uint32_t reg32;
} winbuf_start_addr_v_ns_t;

#define WINBUF_ADDR_H_OFFSET_OFFSET 0x806
#define WINBUF_ADDR_H_OFFSET_RESET  0x00000000
typedef union winbuf_addr_h_offset_u {
    struct {
        unsigned int addr_h_offset:32;      /* Window B Horizontal address offset This is a byte address. The LSB is not used for 16-bpp non-planar pixel format and the last 2 LSB are not used for 32-bpp non-planar pixel format.  For YUV planar pixel format, this specifies horizontal offset of Y plane. The horizontal offsets of U/V plane is derived by HW */
    };

    uint32_t reg32;
} winbuf_addr_h_offset_t;

#define WINBUF_ADDR_H_OFFSET_NS_OFFSET 0x807
#define WINBUF_ADDR_H_OFFSET_NS_RESET  0x00000000
typedef union winbuf_addr_h_offset_ns_u {
    struct {
        unsigned int addr_h_offset_ns:32;   /* Window B Shadowed Horizontal address offset This is ARM set shadow of ADDR_H_OFFSET */
    };

    uint32_t reg32;
} winbuf_addr_h_offset_ns_t;

#define WINBUF_ADDR_V_OFFSET_OFFSET 0x808
#define WINBUF_ADDR_V_OFFSET_RESET  0x00000000
typedef union winbuf_addr_v_offset_u {
    struct {
        unsigned int addr_v_offset:32;      /* Window B Vertical address offset This is a byte address. The LSB is not used for 16-bpp non-planar pixel format and the last 2 LSB are not used for 32-bpp non-planar pixel format.  For YUV planar pixel format, this specifies vertical offset of Y plane. Vertical offsets of U/V plane is derived by HW */
    };

    uint32_t reg32;
} winbuf_addr_v_offset_t;

#define WINBUF_ADDR_V_OFFSET_NS_OFFSET 0x809
#define WINBUF_ADDR_V_OFFSET_NS_RESET  0x00000000
typedef union winbuf_addr_v_offset_ns_u {
    struct {
        unsigned int addr_v_offset_ns:32;   /* Window B Shadowed Vertical address offset This is ARM set shadow of ADDR_V_OFFSET */
    };

    uint32_t reg32;
} winbuf_addr_v_offset_ns_t;

#define WINBUF_UFLOW_STATUS_OFFSET 0x80A
#define WINBUF_UFLOW_STATUS_RESET  0x00000000
typedef union winbuf_uflow_status_u {
    struct {
        unsigned int uflow_count:24;        /* Underflow count. This field indicates the number of contiguous groups of output pixels for which there was no data in the FIFO. For example, if the valid from the FIFO is low for 10 consecutive cycles and then goes high, the counter will increment by one. Reset to zero on write */
        unsigned int undefined_bits_24_29:6;
        unsigned int count_oflow:1;         /* Flag bit that indicates that the underflow event counter has overflowed. There were too many events. If COUNT_OFLOW is set, UFLOW_COUNT is meaningless. Cleared on write */
        unsigned int undefined_bit_31:1;
    };

    uint32_t reg32;
} winbuf_uflow_status_t;

typedef struct win_common_regs {
    DEFINE_REG32(win_options);
    DEFINE_REG32(win_byte_swap);
    DEFINE_REG32(win_buffer_control);
    DEFINE_REG32(win_color_depth);
    DEFINE_REG32(win_position);
    DEFINE_REG32(win_size);
    DEFINE_REG32(win_prescaled_size);
    DEFINE_REG32(win_h_initial_dda);
    DEFINE_REG32(win_v_initial_dda);
    DEFINE_REG32(win_dda_increment);
    DEFINE_REG32(win_line_stride);
    DEFINE_REG32(win_buf_stride);
    DEFINE_REG32(win_buffer_addr_mode);
    DEFINE_REG32(win_blend_nokey);
    DEFINE_REG32(win_blend_1win);
    DEFINE_REG32(win_blend_2win_b);
    DEFINE_REG32(win_blend_2win_c);
    DEFINE_REG32(win_blend_3win_ac);
    DEFINE_REG32(win_hp_fetch_control);
    DEFINE_REG32(win_uv_buf_stride);
    DEFINE_REG32(winbuf_start_addr_u);
    DEFINE_REG32(winbuf_start_addr_v);
    DEFINE_REG32(winbuf_start_addr);
    DEFINE_REG32(winbuf_addr_h_offset);
    DEFINE_REG32(winbuf_addr_v_offset);
    DEFINE_REG32(winbuf_uflow_status);
} win_common_regs;

extern regs_io_handler win_common_handler;

#endif // TEGRA_WIN_COMMON_H
