/*
 * Widget modifiers - hien thi modifier dang giu (Shift/Ctrl/Alt/GUI)
 * Port tu dongle_display cua mctechnology17, don gian hoa cho Sofle OLED 128x32
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <lvgl.h>
#include <zmk/display.h>

struct zmk_widget_modifiers {
    sys_snode_t node;
    lv_obj_t *obj;
};

int zmk_widget_modifiers_init(struct zmk_widget_modifiers *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_modifiers_obj(struct zmk_widget_modifiers *widget);
