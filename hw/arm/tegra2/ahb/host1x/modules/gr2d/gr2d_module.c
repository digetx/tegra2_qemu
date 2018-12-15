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
#include "hw/sysbus.h"

#include "gr2d.h"
#include "host1x_syncpts.h"
#include "host1x_module.h"

#include "tegra_trace.h"

#define WR_MASKED(r, d, m)  r = (r & ~m##_WRMASK) | (d & m##_WRMASK)

void gr2d_write(struct host1x_module *module, uint32_t offset, uint32_t data)
{
    gr2d_regs *regs = module->opaque;
    gr2d_ctx *ctx = NULL;
    uint8_t ctx_referred;

    TRACE_WRITE(module->class_id, offset, data, data);

    switch (offset) {
    case 0x0 ... 0x4C:
        /* Just select context to use and fall through */
        ctx_referred = regs->g2sb_switch_g2currentcontext.curr_context;
        ctx = &regs->ctx[ctx_referred];
        break;
    case 0x1000 ... 0x9000:
        /* Same here ^ */
        ctx_referred = offset >> 13;
        ctx = &regs->ctx[ctx_referred];
        break;
    case G2SB_SWITCH_G2INTERRUPT_OFFSET:
        regs->g2sb_switch_g2interrupt.reg32 = data;
        return;
    case G2SB_SWITCH_G2INTENABLE_OFFSET:
        regs->g2sb_switch_g2intenable.reg32 = data;
        return;
    case G2SB_SWITCH_G2CURRENTCONTEXT_OFFSET:
        WR_MASKED(regs->g2sb_switch_g2currentcontext.reg32, data, G2SB_SWITCH_G2CURRENTCONTEXT);
        return;
    case G2SB_SWITCH_G2NXTCXTSWITCH_OFFSET:
        WR_MASKED(regs->g2sb_switch_g2nxtcxtswitch.reg32, data, G2SB_SWITCH_G2NXTCXTSWITCH);
        return;
    case G2SB_SWITCH_G2GLOBALCONTROL_OFFSET:
        regs->g2sb_switch_g2globalcontrol.reg32 = data;
        return;
    case G2SB_SWITCH_G2GLOBALCONTROLB_OFFSET:
        regs->g2sb_switch_g2globalcontrolb.reg32 = data;
        return;
    case G2SB_SWITCH_G2WORKINGSTAT_OFFSET:
        WR_MASKED(regs->g2sb_switch_g2workingstat.reg32, data, G2SB_SWITCH_G2WORKINGSTAT);
        return;
    case G2SB_SWITCH_G2BUFTHRESHOLD_OFFSET:
        regs->g2sb_switch_g2bufthreshold.reg32 = data;
        return;
    case G2SB_SWITCH_CLKEN_OVERIDE_OFFSET:
        regs->g2sb_switch_clken_overide.reg32 = data;
        return;
    case G2SB_SWITCH_G2_MCCIF_FIFOCTRL_OFFSET:
        regs->g2sb_switch_g2_mccif_fifoctrl.reg32 = data;
        return;
    case G2SB_SWITCH_TIMEOUT_WCOAL_G2_OFFSET:
        regs->g2sb_switch_timeout_wcoal_g2.reg32 = data;
        return;
    default:
        g_assert_not_reached();
        return;
    }

    switch (offset & 0x7f) {
    case G2SB_INCR_SYNCPT_OFFSET:
    {
        g2sb_incr_syncpt method = { .reg32 = data };

        host1x_incr_syncpt(method.indx);
        return;
    }
    case G2SB_INCR_SYNCPT_CNTRL_OFFSET:
        ctx->g2sb_incr_syncpt_cntrl.reg32 = data;
        break;
    case G2SB_INCR_SYNCPT_ERROR_OFFSET:
        ctx->g2sb_incr_syncpt_error.reg32 = data;
        break;
    case G2SB_G2CLASSCHANNEL_REGONLY_OFFSET:
        WR_MASKED(ctx->g2sb_g2classchannel_regonly.reg32, data, G2SB_G2CLASSCHANNEL_REGONLY);
        break;
    case G2SB_G2TRIGGER_OFFSET:
        ctx->g2sb_g2trigger.reg32 = data;
        break;
    case G2SB_G2TRIGGER1_OFFSET:
        ctx->g2sb_g2trigger1.reg32 = data;
        break;
    case G2SB_G2TRIGGER2_OFFSET:
        ctx->g2sb_g2trigger2.reg32 = data;
        break;
    case G2SB_G2CMDSEL_OFFSET:
        WR_MASKED(ctx->g2sb_g2cmdsel.reg32, data, G2SB_G2CMDSEL);
        break;
    case G2SB_G2RAISE_OFFSET:
        ctx->g2sb_g2raise.reg32 = data;
        break;
    case G2SB_G2HOSTSET_OFFSET:
        ctx->g2sb_g2hostset.reg32 = data;
        break;
    case G2SB_G2HOSTFIFO_OFFSET:
        ctx->g2sb_g2hostfifo.reg32 = data;
        break;
    case G2SB_G2VDDA_OFFSET:
        ctx->g2sb_g2vdda.reg32 = data;
        break;
    case G2SB_G2VDDAINI_OFFSET:
        ctx->g2sb_g2vddaini.reg32 = data;
        break;
    case G2SB_G2HDDA_OFFSET:
        ctx->g2sb_g2hdda.reg32 = data;
        break;
    case G2SB_G2HDDAINILS_OFFSET:
        ctx->g2sb_g2hddainils.reg32 = data;
        break;
    case G2SB_G2CSCFIRST_OFFSET:
        ctx->g2sb_g2cscfirst.reg32 = data;
        break;
    case G2SB_G2CSCSECOND_OFFSET:
        ctx->g2sb_g2cscsecond.reg32 = data;
        break;
    case G2SB_G2CSCTHIRD_OFFSET:
        ctx->g2sb_g2cscthird.reg32 = data;
        break;
    case G2SB_G2CMKEYL_OFFSET:
        ctx->g2sb_g2cmkeyl.reg32 = data;
        break;
    case G2SB_G2CMKEYU_OFFSET:
        ctx->g2sb_g2cmkeyu.reg32 = data;
        break;
    case G2SB_G2UBA_A_OFFSET:
        ctx->g2sb_g2uba_a.reg32 = data;
        break;
    case G2SB_G2VBA_A_OFFSET:
        ctx->g2sb_g2vba_a.reg32 = data;
        break;
    case G2SB_G2SBFORMAT_OFFSET:
        ctx->g2sb_g2sbformat.reg32 = data;
        break;
    case G2SB_G2CONTROLSB_OFFSET:
        ctx->g2sb_g2controlsb.reg32 = data;
        break;
    case G2SB_G2CONTROLSECOND_OFFSET:
        ctx->g2sb_g2controlsecond.reg32 = data;
        break;
    case G2SB_G2CONTROLMAIN_OFFSET:
        ctx->g2sb_g2controlmain.reg32 = data;
        break;
    case G2SB_G2ROPFADE_OFFSET:
        ctx->g2sb_g2ropfade.reg32 = data;
        break;
    case G2SB_G2ALPHABLEND_OFFSET:
        ctx->g2sb_g2alphablend.reg32 = data;
        break;
    case G2SB_G2CLIPLEFTTOP_OFFSET:
        ctx->g2sb_g2cliplefttop.reg32 = data;
        break;
    case G2SB_G2CLIPRIGHTBOT_OFFSET:
        ctx->g2sb_g2cliprightbot.reg32 = data;
        break;
    case G2SB_G2PATPACK_OFFSET:
        ctx->g2sb_g2patpack.reg32 = data;
        break;
    case G2SB_G2PATPACK_SIZE_OFFSET:
        ctx->g2sb_g2patpack_size.reg32 = data;
        break;
    case G2SB_G2PATBA_OFFSET:
        ctx->g2sb_g2patba.reg32 = data;
        break;
    case G2SB_G2PATOS_OFFSET:
        ctx->g2sb_g2patos.reg32 = data;
        break;
    case G2SB_G2PATBGC_OFFSET:
        ctx->g2sb_g2patbgc.reg32 = data;
        break;
    case G2SB_G2PATFGC_OFFSET:
        ctx->g2sb_g2patfgc.reg32 = data;
        break;
    case G2SB_G2PATKEY_OFFSET:
        ctx->g2sb_g2patkey.reg32 = data;
        break;
    case G2SB_G2DSTBA_OFFSET:
        ctx->g2sb_g2dstba.reg32 = data;
        break;
    case G2SB_G2DSTBA_B_OFFSET:
        ctx->g2sb_g2dstba_b.reg32 = data;
        break;
    case G2SB_G2DSTBA_C_OFFSET:
        ctx->g2sb_g2dstba_c.reg32 = data;
        break;
    case G2SB_G2DSTST_OFFSET:
        ctx->g2sb_g2dstst.reg32 = data;
        break;
    case G2SB_G2SRCPACK_OFFSET:
        ctx->g2sb_g2srcpack.reg32 = data;
        break;
    case G2SB_G2SRCPACK_SIZE_OFFSET:
        ctx->g2sb_g2srcpack_size.reg32 = data;
        break;
    case G2SB_G2SRCBA_OFFSET:
        ctx->g2sb_g2srcba.reg32 = data;
        break;
    case G2SB_G2SRCBA_B_OFFSET:
        ctx->g2sb_g2srcba_b.reg32 = data;
        break;
    case G2SB_G2SRCST_OFFSET:
        ctx->g2sb_g2srcst.reg32 = data;
        break;
    case G2SB_G2SRCBGC_OFFSET:
        ctx->g2sb_g2srcbgc.reg32 = data;
        break;
    case G2SB_G2SRCFGC_OFFSET:
        ctx->g2sb_g2srcfgc.reg32 = data;
        break;
    case G2SB_G2SRCKEY_OFFSET:
        ctx->g2sb_g2srckey.reg32 = data;
        break;
    case G2SB_G2SRCSIZE_OFFSET:
        ctx->g2sb_g2srcsize.reg32 = data;
        break;
    case G2SB_G2DSTSIZE_OFFSET:
        ctx->g2sb_g2dstsize.reg32 = data;
        break;
    case G2SB_G2SRCPS_OFFSET:
        ctx->g2sb_g2srcps.reg32 = data;
        break;
    case G2SB_G2DSTPS_OFFSET:
        ctx->g2sb_g2dstps.reg32 = data;
        break;
    case G2SB_G2CBDES_OFFSET:
        ctx->g2sb_g2cbdes.reg32 = data;
        break;
    case G2SB_G2CBSTRIDE_OFFSET:
        ctx->g2sb_g2cbstride.reg32 = data;
        break;
    case G2SB_G2LINESETTING_OFFSET:
        ctx->g2sb_g2linesetting.reg32 = data;
        break;
    case G2SB_G2LINEDELTAN_OFFSET:
        ctx->g2sb_g2linedeltan.reg32 = data;
        break;
    case G2SB_G2LINEDELTAM_OFFSET:
        ctx->g2sb_g2linedeltam.reg32 = data;
        break;
    case G2SB_G2LINEPOS_OFFSET:
        ctx->g2sb_g2linepos.reg32 = data;
        break;
    case G2SB_G2LINELEN_OFFSET:
        ctx->g2sb_g2linelen.reg32 = data;
        break;
    case G2SB_G2CSCFOURTH_OFFSET:
        ctx->g2sb_g2cscfourth.reg32 = data;
        break;
    case G2SB_G2SRCST_B_OFFSET:
        ctx->g2sb_g2srcst_b.reg32 = data;
        break;
    case G2SB_G2UVSTRIDE_OFFSET:
        ctx->g2sb_g2uvstride.reg32 = data;
        break;
    case G2SB_G2CBDES2_OFFSET:
        ctx->g2sb_g2cbdes2.reg32 = data;
        break;
    case G2SB_G2TILEMODE_OFFSET:
        ctx->g2sb_g2tilemode.reg32 = data;
        break;
    case G2SB_G2PATBASE_OFFSET:
        ctx->g2sb_g2patbase.reg32 = data;
        break;
    case G2SB_G2SRCBA_SB_SURFBASE_OFFSET:
        ctx->g2sb_g2srcba_sb_surfbase.reg32 = data;
        break;
    case G2SB_G2DSTBA_SB_SURFBASE_OFFSET:
        ctx->g2sb_g2dstba_sb_surfbase.reg32 = data;
        break;
    case G2SB_G2DSTBA_B_SB_SURFBASE_OFFSET:
        ctx->g2sb_g2dstba_b_sb_surfbase.reg32 = data;
        break;
    case G2SB_G2VBA_A_SB_SURFBASE_OFFSET:
        ctx->g2sb_g2vba_a_sb_surfbase.reg32 = data;
        break;
    case G2SB_G2UBA_A_SB_SURFBASE_OFFSET:
        ctx->g2sb_g2uba_a_sb_surfbase.reg32 = data;
        break;
    default:
        g_assert_not_reached();
        break;
    }

    if (regs->g2sb_switch_g2currentcontext.curr_context != ctx_referred)
        return;

    if ((ctx->g2sb_g2trigger.trigger & 0x7f) == (offset & 0x7f) ||
        (ctx->g2sb_g2trigger1.trigger1 & 0x7f) == (offset & 0x7f) ||
        (ctx->g2sb_g2trigger2.trigger2 & 0x7f) == (offset & 0x7f))
    {
        process_2d(ctx, module->class_id == 0x52);
    }
}

uint32_t gr2d_read(struct host1x_module *module, uint32_t offset)
{
    gr2d_regs *regs = module->opaque;
    gr2d_ctx *ctx = NULL;
    uint32_t ret = 0;

    switch (offset) {
    case 0x0 ... 0x4C:
        ctx = &regs->ctx[regs->g2sb_switch_g2currentcontext.curr_context];
        break;
    case 0x1000 ... 0x9000:
        ctx = &regs->ctx[offset >> 13];
        break;
    case G2SB_SWITCH_G2INTERRUPT_OFFSET:
        ret = regs->g2sb_switch_g2interrupt.reg32;
        goto out;
    case G2SB_SWITCH_G2INTENABLE_OFFSET:
        ret = regs->g2sb_switch_g2intenable.reg32;
        goto out;
    case G2SB_SWITCH_G2CURRENTCONTEXT_OFFSET:
        ret = regs->g2sb_switch_g2currentcontext.reg32;
        goto out;
    case G2SB_SWITCH_G2NXTCXTSWITCH_OFFSET:
        ret = regs->g2sb_switch_g2nxtcxtswitch.reg32;
        goto out;
    case G2SB_SWITCH_G2GLOBALCONTROL_OFFSET:
        ret = regs->g2sb_switch_g2globalcontrol.reg32;
        goto out;
    case G2SB_SWITCH_G2GLOBALCONTROLB_OFFSET:
        ret = regs->g2sb_switch_g2globalcontrolb.reg32;
        goto out;
    case G2SB_SWITCH_G2WORKINGSTAT_OFFSET:
        ret = regs->g2sb_switch_g2workingstat.reg32;
        goto out;
    case G2SB_SWITCH_G2BUFTHRESHOLD_OFFSET:
        ret = regs->g2sb_switch_g2bufthreshold.reg32;
        goto out;
    case G2SB_SWITCH_CLKEN_OVERIDE_OFFSET:
        ret = regs->g2sb_switch_clken_overide.reg32;
        goto out;
    case G2SB_SWITCH_G2_MCCIF_FIFOCTRL_OFFSET:
        ret = regs->g2sb_switch_g2_mccif_fifoctrl.reg32;
        goto out;
    case G2SB_SWITCH_TIMEOUT_WCOAL_G2_OFFSET:
        ret = regs->g2sb_switch_timeout_wcoal_g2.reg32;
        goto out;
    default:
        g_assert_not_reached();
        goto out;
    }

    switch (offset & 0x7f) {
    case G2SB_INCR_SYNCPT_CNTRL_OFFSET:
        ret = ctx->g2sb_incr_syncpt_cntrl.reg32;
        break;
    case G2SB_INCR_SYNCPT_ERROR_OFFSET:
        ret = ctx->g2sb_incr_syncpt_error.reg32;
        break;
    case G2SB_G2CLASSCHANNEL_REGONLY_OFFSET:
        ret = ctx->g2sb_g2classchannel_regonly.reg32;
        break;
    case G2SB_G2TRIGGER_OFFSET:
        ret = ctx->g2sb_g2trigger.reg32;
        break;
    case G2SB_G2TRIGGER1_OFFSET:
        ret = ctx->g2sb_g2trigger1.reg32;
        break;
    case G2SB_G2TRIGGER2_OFFSET:
        ret = ctx->g2sb_g2trigger2.reg32;
        break;
    case G2SB_G2CMDSEL_OFFSET:
        ret = ctx->g2sb_g2cmdsel.reg32;
        break;
    case G2SB_G2RAISE_OFFSET:
        ret = ctx->g2sb_g2raise.reg32;
        break;
    case G2SB_G2HOSTSET_OFFSET:
        ret = ctx->g2sb_g2hostset.reg32;
        break;
    case G2SB_G2HOSTFIFO_OFFSET:
        ret = ctx->g2sb_g2hostfifo.reg32;
        break;
    case G2SB_G2VDDA_OFFSET:
        ret = ctx->g2sb_g2vdda.reg32;
        break;
    case G2SB_G2VDDAINI_OFFSET:
        ret = ctx->g2sb_g2vddaini.reg32;
        break;
    case G2SB_G2HDDA_OFFSET:
        ret = ctx->g2sb_g2hdda.reg32;
        break;
    case G2SB_G2HDDAINILS_OFFSET:
        ret = ctx->g2sb_g2hddainils.reg32;
        break;
    case G2SB_G2CSCFIRST_OFFSET:
        ret = ctx->g2sb_g2cscfirst.reg32;
        break;
    case G2SB_G2CSCSECOND_OFFSET:
        ret = ctx->g2sb_g2cscsecond.reg32;
        break;
    case G2SB_G2CSCTHIRD_OFFSET:
        ret = ctx->g2sb_g2cscthird.reg32;
        break;
    case G2SB_G2CMKEYL_OFFSET:
        ret = ctx->g2sb_g2cmkeyl.reg32;
        break;
    case G2SB_G2CMKEYU_OFFSET:
        ret = ctx->g2sb_g2cmkeyu.reg32;
        break;
    case G2SB_G2UBA_A_OFFSET:
        ret = ctx->g2sb_g2uba_a.reg32;
        break;
    case G2SB_G2VBA_A_OFFSET:
        ret = ctx->g2sb_g2vba_a.reg32;
        break;
    case G2SB_G2SBFORMAT_OFFSET:
        ret = ctx->g2sb_g2sbformat.reg32;
        break;
    case G2SB_G2CONTROLSB_OFFSET:
        ret = ctx->g2sb_g2controlsb.reg32;
        break;
    case G2SB_G2CONTROLSECOND_OFFSET:
        ret = ctx->g2sb_g2controlsecond.reg32;
        break;
    case G2SB_G2CONTROLMAIN_OFFSET:
        ret = ctx->g2sb_g2controlmain.reg32;
        break;
    case G2SB_G2ROPFADE_OFFSET:
        ret = ctx->g2sb_g2ropfade.reg32;
        break;
    case G2SB_G2ALPHABLEND_OFFSET:
        ret = ctx->g2sb_g2alphablend.reg32;
        break;
    case G2SB_G2CLIPLEFTTOP_OFFSET:
        ret = ctx->g2sb_g2cliplefttop.reg32;
        break;
    case G2SB_G2CLIPRIGHTBOT_OFFSET:
        ret = ctx->g2sb_g2cliprightbot.reg32;
        break;
    case G2SB_G2PATPACK_OFFSET:
        ret = ctx->g2sb_g2patpack.reg32;
        break;
    case G2SB_G2PATPACK_SIZE_OFFSET:
        ret = ctx->g2sb_g2patpack_size.reg32;
        break;
    case G2SB_G2PATBA_OFFSET:
        ret = ctx->g2sb_g2patba.reg32;
        break;
    case G2SB_G2PATOS_OFFSET:
        ret = ctx->g2sb_g2patos.reg32;
        break;
    case G2SB_G2PATBGC_OFFSET:
        ret = ctx->g2sb_g2patbgc.reg32;
        break;
    case G2SB_G2PATFGC_OFFSET:
        ret = ctx->g2sb_g2patfgc.reg32;
        break;
    case G2SB_G2PATKEY_OFFSET:
        ret = ctx->g2sb_g2patkey.reg32;
        break;
    case G2SB_G2DSTBA_OFFSET:
        ret = ctx->g2sb_g2dstba.reg32;
        break;
    case G2SB_G2DSTBA_B_OFFSET:
        ret = ctx->g2sb_g2dstba_b.reg32;
        break;
    case G2SB_G2DSTBA_C_OFFSET:
        ret = ctx->g2sb_g2dstba_c.reg32;
        break;
    case G2SB_G2DSTST_OFFSET:
        ret = ctx->g2sb_g2dstst.reg32;
        break;
    case G2SB_G2SRCPACK_OFFSET:
        ret = ctx->g2sb_g2srcpack.reg32;
        break;
    case G2SB_G2SRCPACK_SIZE_OFFSET:
        ret = ctx->g2sb_g2srcpack_size.reg32;
        break;
    case G2SB_G2SRCBA_OFFSET:
        ret = ctx->g2sb_g2srcba.reg32;
        break;
    case G2SB_G2SRCBA_B_OFFSET:
        ret = ctx->g2sb_g2srcba_b.reg32;
        break;
    case G2SB_G2SRCST_OFFSET:
        ret = ctx->g2sb_g2srcst.reg32;
        break;
    case G2SB_G2SRCBGC_OFFSET:
        ret = ctx->g2sb_g2srcbgc.reg32;
        break;
    case G2SB_G2SRCFGC_OFFSET:
        ret = ctx->g2sb_g2srcfgc.reg32;
        break;
    case G2SB_G2SRCKEY_OFFSET:
        ret = ctx->g2sb_g2srckey.reg32;
        break;
    case G2SB_G2SRCSIZE_OFFSET:
        ret = ctx->g2sb_g2srcsize.reg32;
        break;
    case G2SB_G2DSTSIZE_OFFSET:
        ret = ctx->g2sb_g2dstsize.reg32;
        break;
    case G2SB_G2SRCPS_OFFSET:
        ret = ctx->g2sb_g2srcps.reg32;
        break;
    case G2SB_G2DSTPS_OFFSET:
        ret = ctx->g2sb_g2dstps.reg32;
        break;
    case G2SB_G2CBDES_OFFSET:
        ret = ctx->g2sb_g2cbdes.reg32;
        break;
    case G2SB_G2CBSTRIDE_OFFSET:
        ret = ctx->g2sb_g2cbstride.reg32;
        break;
    case G2SB_G2LINESETTING_OFFSET:
        ret = ctx->g2sb_g2linesetting.reg32;
        break;
    case G2SB_G2LINEDELTAN_OFFSET:
        ret = ctx->g2sb_g2linedeltan.reg32;
        break;
    case G2SB_G2LINEDELTAM_OFFSET:
        ret = ctx->g2sb_g2linedeltam.reg32;
        break;
    case G2SB_G2LINEPOS_OFFSET:
        ret = ctx->g2sb_g2linepos.reg32;
        break;
    case G2SB_G2LINELEN_OFFSET:
        ret = ctx->g2sb_g2linelen.reg32;
        break;
    case G2SB_G2CSCFOURTH_OFFSET:
        ret = ctx->g2sb_g2cscfourth.reg32;
        break;
    case G2SB_G2SRCST_B_OFFSET:
        ret = ctx->g2sb_g2srcst_b.reg32;
        break;
    case G2SB_G2UVSTRIDE_OFFSET:
        ret = ctx->g2sb_g2uvstride.reg32;
        break;
    case G2SB_G2CBDES2_OFFSET:
        ret = ctx->g2sb_g2cbdes2.reg32;
        break;
    case G2SB_G2TILEMODE_OFFSET:
        ret = ctx->g2sb_g2tilemode.reg32;
        break;
    case G2SB_G2PATBASE_OFFSET:
        ret = ctx->g2sb_g2patbase.reg32;
        break;
    case G2SB_G2SRCBA_SB_SURFBASE_OFFSET:
        ret = ctx->g2sb_g2srcba_sb_surfbase.reg32;
        break;
    case G2SB_G2DSTBA_SB_SURFBASE_OFFSET:
        ret = ctx->g2sb_g2dstba_sb_surfbase.reg32;
        break;
    case G2SB_G2DSTBA_B_SB_SURFBASE_OFFSET:
        ret = ctx->g2sb_g2dstba_b_sb_surfbase.reg32;
        break;
    case G2SB_G2VBA_A_SB_SURFBASE_OFFSET:
        ret = ctx->g2sb_g2vba_a_sb_surfbase.reg32;
        break;
    case G2SB_G2UBA_A_SB_SURFBASE_OFFSET:
        ret = ctx->g2sb_g2uba_a_sb_surfbase.reg32;
        break;
    default:
        g_assert_not_reached();
        break;
    }

out:
    TRACE_READ(module->class_id, offset, ret);

    return ret;
}
