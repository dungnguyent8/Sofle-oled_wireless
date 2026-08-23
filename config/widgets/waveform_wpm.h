/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/slist.h>

struct zmk_widget_waveform_wpm {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *bars[16]; // 16 cot bar, tu trai (cu) den phai (moi)
};

int zmk_widget_waveform_wpm_init(struct zmk_widget_waveform_wpm *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_waveform_wpm_obj(struct zmk_widget_waveform_wpm *widget);
