/*
 * Widget bongo cat - meo danh may theo toc do go
 * Port tu dongle_display cua mctechnology17 cho Sofle OLED 128x32
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <lvgl.h>
#include <zmk/display.h>

struct zmk_widget_bongo_cat {
    sys_snode_t node;
    lv_obj_t *obj;
};

int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget);
