/*
 * Widget HID indicators - hien thi Caps/Num/Scroll Lock tu host
 * Port tu dongle_display cua mctechnology17 cho Sofle OLED 128x32
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
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/hid_indicators.h>

#include "hid_indicators.h"

// bit tu HID spec: bit0 Num, bit1 Caps, bit2 Scroll
#define LED_NLCK 0x01
#define LED_CLCK 0x02
#define LED_SLCK 0x04

#define SYMBOL_NLCK LV_SYMBOL_OK      // hien con vech khi Num lock
#define SYMBOL_CLCK LV_SYMBOL_CLOSE   // X khi Caps lock - de nhan nhat
#define SYMBOL_SLCK LV_SYMBOL_UP      // mui ten khi Scroll lock

struct hid_indicators_state {
    uint8_t hid_indicators;
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static void set_hid_indicators(lv_obj_t *label, struct hid_indicators_state state) {
    char text[13] = {};

    if (state.hid_indicators & LED_CLCK) {
        strcat(text, SYMBOL_CLCK " ");
    }
    if (state.hid_indicators & LED_NLCK) {
        strcat(text, SYMBOL_NLCK " ");
    }
    if (state.hid_indicators & LED_SLCK) {
        strcat(text, SYMBOL_SLCK);
    }

    lv_label_set_text(label, text);
}

void hid_indicators_update_cb(struct hid_indicators_state state) {
    struct zmk_widget_hid_indicators *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_hid_indicators(widget->obj, state);
    }
}

static struct hid_indicators_state hid_indicators_get_state(const zmk_event_t *eh) {
    // FIX CRASH: khi init, macro gọi hàm này với eh=NULL — tuyệt đối không deref eh.
    // Lấy state hiện tại qua API chính hãng thay vì cast event.
    if (eh == NULL) {
        return (struct hid_indicators_state){
            .hid_indicators = zmk_hid_indicators_get_current_profile()};
    }
    struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);
    return (struct hid_indicators_state){.hid_indicators = ev->indicators};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_hid_indicators, struct hid_indicators_state,
                            hid_indicators_update_cb, hid_indicators_get_state)

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
// tren central: lang nghe moi indicator change de phan phat qua split
ZMK_SUBSCRIPTION(widget_hid_indicators, zmk_hid_indicators_changed);
#else
// tren peripheral: event duoc forward tu central qua split
ZMK_SUBSCRIPTION(widget_hid_indicators, zmk_hid_indicators_changed);
#endif

int zmk_widget_hid_indicators_init(struct zmk_widget_hid_indicators *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);
    lv_obj_set_style_text_font(widget->obj, lv_theme_get_font_small(parent), LV_PART_MAIN);
    lv_label_set_text(widget->obj, "");

    sys_slist_append(&widgets, &widget->node);

    widget_hid_indicators_init();

    return 0;
}

lv_obj_t *zmk_widget_hid_indicators_obj(struct zmk_widget_hid_indicators *widget) {
    return widget->obj;
}
