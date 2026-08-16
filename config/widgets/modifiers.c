/*
 * Widget modifiers - hien thi modifier DANG GIU (Shift/Ctrl/Alt/GUI)
 * Polling zmk_hid_get_explicit_mods() moi 100ms thay vi theo event
 * keycode_state_changed (event nay chay TRUOC khi hid cap nhat -> hien
 * tre 1 nhịp: nhan khong thay, nha moi thay).
 * Format: "Sh + Ct + Al + Gi" — đúng tổ hợp đang giữ theo thời gian thực.
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// PHẢI include lvgl.h TRƯỚC mọi zmk header: keys.h định nghĩa macro "E"
// (keycode phím E) sẽ phá LV_CONF_PATH "E:/..." của Windows → fatal error
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#include <dt-bindings/zmk/modifiers.h>

#include "modifiers.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static void set_modifiers(lv_obj_t *label, zmk_mod_flags_t mods) {
    // Dang cho: Sh=shift Ct=ctrl Al=alt Gi=gui (Windows/super key)
    char text[40] = {};

    if (mods & (MOD_LSFT | MOD_RSFT)) {
        strcpy(text, "Sh");
    }
    if (mods & (MOD_LCTL | MOD_RCTL)) {
        strcat(text, (text[0] != '\0') ? " + Ct" : "Ct");
    }
    if (mods & (MOD_LALT | MOD_RALT)) {
        strcat(text, (text[0] != '\0') ? " + Al" : "Al");
    }
    if (mods & (MOD_LGUI | MOD_RGUI)) {
        strcat(text, (text[0] != '\0') ? " + Gi" : "Gi");
    }

    lv_label_set_text(label, text);
}

// Poll moi 100ms: doc truc tiep state modifier tu HID — luôn đúng
// "đang giữ", không lệch nhịp, không phụ thuộc event timing
static void modifiers_poll_cb(lv_timer_t *timer) {
    zmk_mod_flags_t mods = zmk_hid_get_explicit_mods();

    struct zmk_widget_modifiers *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_modifiers(widget->obj, mods); }
}

int zmk_widget_modifiers_init(struct zmk_widget_modifiers *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);
    lv_obj_set_style_text_font(widget->obj, lv_theme_get_font_small(parent), LV_PART_MAIN);
    lv_label_set_text(widget->obj, "");

    sys_slist_append(&widgets, &widget->node);

    // Tick 100ms — nhanh hơn mắt người, cảm giác tức thì
    static lv_timer_t *poll_timer = NULL;
    if (poll_timer == NULL) {
        poll_timer = lv_timer_create(modifiers_poll_cb, 100, NULL);
    }

    return 0;
}

lv_obj_t *zmk_widget_modifiers_obj(struct zmk_widget_modifiers *widget) { return widget->obj; }
