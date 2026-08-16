/*
 * Widget HID indicators - hien thi Caps/Num/Scroll Lock tu host
 * Port tu dongle_display cua mctechnology17 cho Sofle OLED 128x32
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <lvgl.h>
#include <zmk/display.h>

struct zmk_widget_hid_indicators {
    sys_snode_t node;
    lv_obj_t *obj;
};

int zmk_widget_hid_indicators_init(struct zmk_widget_hid_indicators *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_hid_indicators_obj(struct zmk_widget_hid_indicators *widget);
