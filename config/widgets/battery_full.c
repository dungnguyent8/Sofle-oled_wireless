/*
 * Widget battery_full - icon pin theo muc + phan tram, mot dong giong
 * "[icon] 87%" (widget goc ZMK chi hien HOAC icon HOAC %, khong ca hai —
 * yeu cau rieng cua man trai doc).
 * Icon theo muc: FULL/3/2/1/EMPTY + dang sac hien LV_SYMBOL_CHARGE.
 * Nguon state: listen battery_state_changed + usb_conn_state_changed.
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// PHẢI include lvgl.h TRƯỚC mọi zmk header: keys.h định nghĩa macro "E"
// phá LV_CONF_PATH "E:/..." trên Windows → fatal error
#include <lvgl.h>

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/usb.h>
#include <zmk/event_manager.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/battery_state_changed.h>

#include "battery_full.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct battery_full_state {
    uint8_t level;
    bool usb_present;
};

static void set_battery_full(lv_obj_t *label, struct battery_full_state state) {
    char text[16] = {};

    uint8_t level = state.level;
    if (level > 95) {
        strcpy(text, LV_SYMBOL_BATTERY_FULL);
    } else if (level > 65) {
        strcpy(text, LV_SYMBOL_BATTERY_3);
    } else if (level > 35) {
        strcpy(text, LV_SYMBOL_BATTERY_2);
    } else if (level > 5) {
        strcpy(text, LV_SYMBOL_BATTERY_1);
    } else {
        strcpy(text, LV_SYMBOL_BATTERY_EMPTY);
    }

    char perc[6] = {};
    snprintf(perc, sizeof(perc), " %u%%", level);
    strcat(text, perc);

    if (state.usb_present) {
        strcat(text, " " LV_SYMBOL_CHARGE);
    }

    lv_label_set_text(label, text);
}

void battery_full_update_cb(struct battery_full_state state) {
    struct zmk_widget_battery_full *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_battery_full(widget->obj, state);
    }
}

static struct battery_full_state battery_full_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);

    return (struct battery_full_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
        .usb_present = zmk_usb_is_powered(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_full, struct battery_full_state,
                            battery_full_update_cb, battery_full_get_state)

ZMK_SUBSCRIPTION(widget_battery_full, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(widget_battery_full, zmk_usb_conn_state_changed);

int zmk_widget_battery_full_init(struct zmk_widget_battery_full *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);

    sys_slist_append(&widgets, &widget->node);

    widget_battery_full_init();
    return 0;
}

lv_obj_t *zmk_widget_battery_full_obj(struct zmk_widget_battery_full *widget) {
    return widget->obj;
}
