/* linux/drivers/video/exynos_decon/panel/dsim_backlight.c
 *
 * Header file for Samsung MIPI-DSI Backlight driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 * Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#include <linux/backlight.h>

#include "../dsim.h"
#include "../decon.h"
#include "dsim_backlight.h"
#include "panel_info.h"

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#endif


#ifdef CONFIG_PANEL_AID_DIMMING

static unsigned int get_actual_br_value(struct dsim_device *dsim, int index)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_br_err;
	}

	if (index > MAX_BR_INFO)
		index = MAX_BR_INFO;

	return dimming_info[index].br;

get_br_err:
	return 0;
}
static unsigned char *get_gamma_from_index(struct dsim_device *dsim, int index)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_gamma_err;
	}

	if (index > MAX_BR_INFO)
		index = MAX_BR_INFO;

	return (unsigned char *)dimming_info[index].gamma;

get_gamma_err:
	return NULL;
}

static unsigned char *get_aid_from_index(struct dsim_device *dsim, int index)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_aid_err;
	}

	if (index > MAX_BR_INFO)
		index = MAX_BR_INFO;

	return (u8 *)dimming_info[index].aid;

get_aid_err:
	return NULL;
}

static unsigned char *get_elvss_from_index(struct dsim_device *dsim, int index, int acl)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_elvess_err;
	}

	if(acl)
		return (unsigned char *)dimming_info[index].elvAcl;
	else
		return (unsigned char *)dimming_info[index].elv;

get_elvess_err:
	return NULL;
}


static void dsim_panel_gamma_ctrl(struct dsim_device *dsim)
{
	u8 *gamma = NULL;
	gamma = get_gamma_from_index(dsim, dsim->priv.br_index);
	if (gamma == NULL) {
		dsim_err("%s :faied to get gamma\n", __func__);
		return;
	}

	if (dsim_write_hl_data(dsim, gamma, GAMMA_CMD_CNT) < 0)
		dsim_err("%s : failed to write gamma \n", __func__);
}

static void dsim_panel_aid_ctrl(struct dsim_device *dsim)
{
	u8 *aid = NULL;
	aid = get_aid_from_index(dsim, dsim->priv.br_index);
	if (aid == NULL) {
		dsim_err("%s : faield to get aid value\n", __func__);
		return;
	}
	if (dsim_write_hl_data(dsim, aid, AID_CMD_CNT) < 0)
		dsim_err("%s : failed to write gamma \n", __func__);
}

static void dsim_panel_set_elvss(struct dsim_device *dsim)
{
	u8 *elvss = NULL;
	u8 elvss_val[5] = {0, };

	elvss = get_elvss_from_index(dsim, dsim->priv.br_index, dsim->priv.acl_enable);
	if (elvss == NULL) {
		dsim_err("%s : failed to get elvss value\n", __func__);
		return;
	}

	elvss_val[0] = elvss[0];
	elvss_val[1] = elvss[1];
	if (dsim->priv.temperature <= 0)
		elvss_val[1] = elvss_val[1] + 0x04;
	elvss_val[2] = elvss[2];
	elvss_val[3] = dsim->priv.elvss[2];
	elvss_val[4] = dsim->priv.elvss[3];
	if (dsim->priv.temperature < -25)
		elvss_val[4] = elvss_val[4] - 0x05;

	if (dsim_write_hl_data(dsim, elvss_val, ELVSS_LEN + 1) < 0)
		dsim_err("%s : failed to write elvss \n", __func__);
}


static int dsim_panel_set_acl(struct dsim_device *dsim, int force)
{
	int ret = 0, level = ACL_STATUS_15P;
	struct panel_private *panel = &dsim->priv;

	if (panel == NULL) {
			dsim_err("%s : panel is NULL\n", __func__);
			goto exit;
	}

	if (dsim->priv.siop_enable || LEVEL_IS_HBM(dsim->priv.auto_brightness))  // auto acl or hbm is acl on
		goto acl_update;

	if (!dsim->priv.acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if(force || dsim->priv.current_acl != panel->acl_cutoff_tbl[level][1]) {
		if((ret = dsim_write_hl_data(dsim,  panel->acl_opr_tbl[level], 2)) < 0) {
			dsim_err("fail to write acl opr command.\n");
			goto exit;
		}
		if((ret = dsim_write_hl_data(dsim, panel->acl_cutoff_tbl[level], 2)) < 0) {
			dsim_err("fail to write acl command.\n");
			goto exit;
		}
		dsim->priv.current_acl = panel->acl_cutoff_tbl[level][1];
		dsim_info("acl: %d, auto_brightness: %d\n", dsim->priv.current_acl, dsim->priv.auto_brightness);
	}
exit:
	if (!ret)
		ret = -EPERM;
	return ret;
}

static int dsim_panel_set_tset(struct dsim_device *dsim, int force)
{
	int ret = 0;
	int tset = 0;
	unsigned char SEQ_TSET[TSET_LEN] = {TSET_REG, };

	//return ret;
	tset = ((dsim->priv.temperature >= 0) ? 0 : BIT(7)) | abs(dsim->priv.temperature);

	if(force || dsim->priv.tset[0] != tset) {
		dsim->priv.tset[0] = SEQ_TSET[1] = tset;
		dsim->priv.tset[1] = SEQ_TSET[2] = 0x0A;

		if (dsim->priv.panel_type == LCD_EA8064G) {
			if ((ret = dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET))) < 0) {
				dsim_err("fail to write tset command.\n");
				ret = -EPERM;
			}
		} else {
			if ((ret = dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET) - 1 )) < 0) {
				dsim_err("fail to write tset command.\n");
				ret = -EPERM;
			}
		}
		dsim_info("temperature: %d, tset: %d\n", dsim->priv.temperature, SEQ_TSET[1]);
	}
	return ret;
}

#ifdef CONFIG_PANEL_S6E3HA2_DYNAMIC
static int dsim_panel_set_vint(struct dsim_device *dsim, int force)
{
	int ret = 0;
	int nit = 0;
	int i, level;
	int arraySize = ARRAY_SIZE(VINT_DIM_TABLE);
	unsigned char SEQ_VINT[VINT_LEN] = {VINT_REG, 0x8B, 0x00};

	nit = dsim->priv.br_tbl[dsim->priv.bd->props.brightness];

	level = arraySize - 1;

	for (i = 0; i < arraySize; i++) {
		if (nit <= VINT_DIM_TABLE[i]) {
			level = i;
			goto set_vint;
		}
	}
set_vint:
	if(force || dsim->priv.current_vint != VINT_TABLE[level]) {
		SEQ_VINT[VINT_LEN - 1] = VINT_TABLE[level];
		if ((ret = dsim_write_hl_data(dsim, SEQ_VINT, ARRAY_SIZE(SEQ_VINT))) < 0) {
			dsim_err("fail to write vint command.\n");
			ret = -EPERM;
		}
		dsim->priv.current_vint = VINT_TABLE[level];
		dsim_info("vint: %02x\n", dsim->priv.current_vint);
	}
	return ret;
}
#endif

static int dsim_panel_set_hbm(struct dsim_device *dsim, int force)
{
	int ret = 0, level = LEVEL_IS_HBM(dsim->priv.auto_brightness);
	struct panel_private *panel = &dsim->priv;

	if(dsim->support_lvds)
		return ret;

	if (panel == NULL) {
		dsim_err("%s : panel is NULL\n", __func__);
		goto exit;
	}

	if(force || dsim->priv.current_hbm != panel->hbm_tbl[level][1]) {
		dsim->priv.current_hbm = panel->hbm_tbl[level][1];
		if((ret = dsim_write_hl_data(dsim, panel->hbm_tbl[level], ARRAY_SIZE(SEQ_HBM_OFF))) < 0) {
			dsim_err("fail to write hbm command.\n");
			ret = -EPERM;
		}
		dsim_info("hbm: %d, auto_brightness: %d\n", dsim->priv.current_hbm, dsim->priv.auto_brightness);
	}
exit:
	return ret;
}

static int low_level_set_brightness(struct dsim_device *dsim ,int force)
{
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);

	dsim_panel_gamma_ctrl(dsim);

	dsim_panel_aid_ctrl(dsim);

	dsim_panel_set_elvss(dsim);

#ifdef CONFIG_PANEL_S6E3HA2_DYNAMIC
	dsim_panel_set_vint(dsim, force);
#endif

	if (dsim->priv.panel_type == LCD_EA8064G) {
		if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_EA8064G, ARRAY_SIZE(SEQ_GAMMA_UPDATE_EA8064G)) < 0)
			dsim_err("%s : failed to write gamma \n", __func__);
	} else {
		if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE)) < 0)
			dsim_err("%s : failed to write gamma \n", __func__);

		if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L)) < 0)
			dsim_err("%s : failed to write gamma \n", __func__);
	}

	dsim_panel_set_acl(dsim, force);

	dsim_panel_set_tset(dsim, force);

	dsim_panel_set_hbm(dsim, force);

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) < 0)
			dsim_err("%s : fail to write F0 on command.\n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("%s : fail to write F0 on command\n", __func__);

	return 0;
}

static int get_acutal_br_index(struct dsim_device *dsim, int br)
{
	int i;
	int min;
	int gap;
	int index = 0;
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming_info is NULL\n", __func__);
		return 0;
	}

	min = MAX_BRIGHTNESS;

	for (i = 0; i < MAX_BR_INFO; i++) {
		if (br > dimming_info[i].br)
			gap = br - dimming_info[i].br;
		else
			gap = dimming_info[i].br - br;

		if (gap == 0) {
			index = i;
			break;
		}

		if (gap < min) {
			min = gap;
			index = i;
		}
	}
	return index;
}

#endif

#ifndef CONFIG_PANEL_AID_DIMMING
#if 0
static int dsim_panel_set_cabc(struct dsim_device *dsim, int force) {
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	struct decon_device *decon = decon_int_drvdata;

	if (panel == NULL) {
		dsim_err("%s : panel is NULL\n", __func__);
		goto exit;
	}

	if (panel->auto_brightness > 0 && panel->auto_brightness < 6)
		panel->acl_enable = 1;
	else
		panel->acl_enable = 0;

	if ((panel->acl_enable == panel->current_acl) && !force)
		return 0;

	if (panel->acl_enable) { //cabc on
		if (dsim_write_hl_data(dsim, SEQ_CABC_ON, ARRAY_SIZE(SEQ_CABC_ON)) < 0)
			dsim_err("%s : failed to write SEQ_CD_ON \n", __func__);

		if (decon->state == DECON_STATE_ON) //during wake up time, avoid needless delay
			msleep(500); // for CABC transforming dimming , H/W & DDI company guide

		if (dsim_write_hl_data(dsim, SEQ_CD_ON, ARRAY_SIZE(SEQ_CD_ON)) < 0)
			dsim_err("%s : failed to write SEQ_CD_ON \n", __func__);

	} else { //cabc off
		if (dsim_write_hl_data(dsim, SEQ_CD_OFF, ARRAY_SIZE(SEQ_CD_OFF)) < 0)
			dsim_err("%s : failed to write SEQ_CD_ON \n", __func__);

		if (dsim_write_hl_data(dsim, SEQ_CABC_OFF, ARRAY_SIZE(SEQ_CABC_OFF)) < 0)
			dsim_err("%s : failed to write SEQ_CD_ON \n", __func__);
	}

	panel->current_acl = panel->acl_enable;
	dsim_info("cabc: %d, auto_brightness: %d\n", dsim->priv.current_acl, dsim->priv.auto_brightness);

exit:
	if (!ret)
		ret = -EPERM;
	return ret;
}

static int dsim_panel_set_outdoor_mode(struct dsim_device *dsim, int force) {
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	if (panel == NULL) {
			dsim_err("%s : panel is NULL\n", __func__);
			goto exit;
	}
	if (((panel->current_hbm && panel->auto_brightness == 6)
		|| (!panel->current_hbm && panel->auto_brightness < 6)) && !force)
		return ret;

	if (panel->current_hbm && panel->auto_brightness < 6) {
		sky82896_array_write(SKY82896_eprom_drv_outdoor_off_arr, ARRAY_SIZE(SKY82896_eprom_drv_outdoor_off_arr)); //bl cabc off
	} else if (!panel->current_hbm && panel->auto_brightness == 6) {
		sky82896_array_write(SKY82896_eprom_drv_outdoor_on_arr, ARRAY_SIZE(SKY82896_eprom_drv_outdoor_on_arr)); //bl cabc on
	}

	if (panel->auto_brightness == 6)
		panel->current_hbm = 1;
	else
		panel->current_hbm = 0;

	dsim_info("BL_IC outdoor_mode: %d, auto_brightness: %d\n", dsim->priv.current_hbm, dsim->priv.auto_brightness);

exit:
	if (!ret)
		ret = -EPERM;
	return ret;
}
#endif
#endif

int dsim_panel_set_brightness(struct dsim_device *dsim, int force)
{
	int ret = 0;
#ifndef CONFIG_PANEL_AID_DIMMING
#ifdef CONFIG_PANEL_S6D7AA0X62_DYNAMIC
#define BRIGHTNESS_REG 0x51

	struct panel_private *panel = &dsim->priv;
	unsigned char bl_reg[2];

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active state..\n", __func__);
		goto set_br_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F1\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
	}

	bl_reg[0] = BRIGHTNESS_REG;
	if (panel->bd->props.brightness >= 2)
		bl_reg[1] = panel->bd->props.brightness;
	else
		bl_reg[1] = 0x02;

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active state..\n", __func__);
		return ret;
	}

	dsim_info("%s: platform BL : %d panel BL reg : %d \n", __func__, panel->bd->props.brightness, bl_reg[1]);

	if (dsim_write_hl_data(dsim, bl_reg, ARRAY_SIZE(bl_reg)) < 0)
		dsim_err("%s : fail to write brightness cmd.\n", __func__);

#if 0
	dsim_panel_set_cabc(dsim, force);

	dsim_panel_set_outdoor_mode(dsim, force);
#endif
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F1, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F1));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F1\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
	}

set_br_exit:
	return ret;
#else
	struct panel_private *panel = &dsim->priv;

	if (dsim->support_lvds)
		panel->ops->lvds_pwm_set(dsim, panel->bd->props.brightness, panel->auto_brightness);
	else
		dsim_info("%s:this panel does not support dimming \n", __func__);
#endif
#else
	struct dim_data *dimming;
	struct panel_private *panel = &dsim->priv;
	int p_br = panel->bd->props.brightness;
	int acutal_br = 0;
	int real_br = 0;
	int prev_index = panel->br_index;

	dimming = (struct dim_data *)panel->dim_data;
	if ((dimming == NULL) || (panel->br_tbl == NULL)) {
		dsim_info("%s : this panel does not support dimming\n", __func__);
		return ret;
	}

	acutal_br = panel->br_tbl[p_br];
	panel->br_index = get_acutal_br_index(dsim, acutal_br);
	real_br = get_actual_br_value(dsim, panel->br_index);

	if (LEVEL_IS_HBM(panel->auto_brightness) && (p_br == panel->bd->props.max_brightness)) {
		panel->br_index = HBM_INDEX;
		panel->acl_enable = 1;				// hbm is acl on
	}
	if(panel->siop_enable)					// check auto acl
		panel->acl_enable = 1;

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active state..\n", __func__);
		goto set_br_exit;
	}

	dsim_info("%s : platform : %d, : mapping : %d, real : %d, index : %d\n",
		__func__, p_br, acutal_br, real_br, panel->br_index);

	if (!force && panel->br_index == prev_index)
		goto set_br_exit;

	if ((acutal_br == 0) || (real_br == 0))
		goto set_br_exit;

	mutex_lock(&panel->lock);

	ret = low_level_set_brightness(dsim, force);
	if (ret) {
		dsim_err("%s failed to set brightness : %d\n", __func__, acutal_br);
	}
	mutex_unlock(&panel->lock);

set_br_exit:
#endif
	return ret;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}


static int panel_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct panel_private *priv = bl_get_data(bd);
	struct dsim_device *dsim;

	dsim = container_of(priv, struct dsim_device, priv);

	if (brightness < UI_MIN_BRIGHTNESS || brightness > UI_MAX_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		ret = -EINVAL;
		goto exit_set;
	}

	ret = dsim_panel_set_brightness(dsim, 0);
	if (ret) {
		dsim_err("%s : fail to set brightness\n", __func__);
		goto exit_set;
	}
exit_set:
	return ret;

}

static const struct backlight_ops panel_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_set_brightness,
};


int dsim_backlight_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	panel->bd = backlight_device_register("panel", dsim->dev, &dsim->priv,
					&panel_backlight_ops, NULL);
	if (IS_ERR(panel->bd)) {
		dsim_err("%s:failed register backlight\n", __func__);
		ret = PTR_ERR(panel->bd);
	}

	panel->bd->props.max_brightness = UI_MAX_BRIGHTNESS;
	panel->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;

#ifdef CONFIG_LCD_HMT
	panel->hmt_on = HMT_OFF;
	panel->hmt_brightness = DEFAULT_HMT_BRIGHTNESS;
	panel->hmt_br_index = 0;
#endif
	return ret;
}
