/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/sys/slist.h>

struct zmk_widget_peripheral_battery {
    sys_snode_t node;
    lv_obj_t *obj;
};

int zmk_widget_peripheral_battery_init(struct zmk_widget_peripheral_battery *widget,
                                       lv_obj_t *parent);
lv_obj_t *zmk_widget_peripheral_battery_obj(struct zmk_widget_peripheral_battery *widget);
