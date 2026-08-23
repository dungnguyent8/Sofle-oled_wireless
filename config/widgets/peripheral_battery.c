/*
 * Widget peripheral_battery - pin nua PHAI hien tren man trai (central)
 * Nguon: event zmk_peripheral_battery_state_changed (can
 * CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y trong sofle_left.conf;
 * peripheral tu notify BAS qua GATS moi 60s theo CONFIG_ZMK_BATTERY_REPORT_INTERVAL).
 * Hien thi: "R: 87%" — R = phai. level=0 khi mat ket noi → hien "R: --".
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// PHẢI include lvgl.h TRƯỚC mọi zmk header: keys.h định nghĩa macro "E"
// phá LV_CONF_PATH "E:/..." trên Windows → fatal error
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>

#include "peripheral_battery.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct peripheral_battery_state {
    uint8_t level;      // 255 = chua biet / mat ket noi
    uint16_t millivolts; // 0 = chua co du lieu mV tu characteristic rieng
};

static uint8_t last_level = 255;
static uint16_t last_mv = 0;

static void set_peripheral_battery(lv_obj_t *label, struct peripheral_battery_state state) {
    // 23/08 v3: BO hien thi mV nua phai — characteristic rieng (UUID 0x07)
    // khong duoc central discovery sau khi bond lai (nguyen nhan chua ro,
    // da thử settings reset + flash lai). Chi con % theo curve LiPo moi —
    // duong % la stock ZMK: nrf_vddh -> lithium_ion_mv_to_pct (da patch
    // curve) -> BAS -> central -> event. Event mV-only (level=255) co the
    // van toi neu co notify → fallback last_level, khong hien ✗ sai.
    char text[12] = {};
    uint8_t level = (state.level != 255) ? state.level : last_level;
    if (level == 255) {
        snprintf(text, sizeof(text), LV_SYMBOL_CLOSE " --");
        lv_label_set_text(label, text);
        return;
    }
    const char *icon = LV_SYMBOL_BATTERY_EMPTY;
    if (level > 95) {
        icon = LV_SYMBOL_BATTERY_FULL;
    } else if (level > 65) {
        icon = LV_SYMBOL_BATTERY_3;
    } else if (level > 35) {
        icon = LV_SYMBOL_BATTERY_2;
    } else if (level > 5) {
        icon = LV_SYMBOL_BATTERY_1;
    }
    snprintf(text, sizeof(text), "%s %u%%", icon, level);
    lv_label_set_text(label, text);
}

static void peripheral_battery_update_cb(struct peripheral_battery_state state) {
    if (state.level != 255) {
        last_level = state.level;
    }
    if (state.millivolts > 0) {
        last_mv = state.millivolts;
    }
    struct zmk_widget_peripheral_battery *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_peripheral_battery(widget->obj, state);
    }
}

static struct peripheral_battery_state peripheral_battery_get_state(const zmk_event_t *eh) {
    // Event pin tu peripheral: BAS truyen % (level), characteristic rieng
    // 0x07 truyen mV (khi do level=255 = "khong doi %"). Trang thai ket noi
    // split mat → level ve 255 = "chua biet" (phan biet voi pin 0% that).
    const struct zmk_peripheral_battery_state_changed *batt_ev =
        as_zmk_peripheral_battery_state_changed(eh);
    if (batt_ev != NULL) {
        if (batt_ev->state_of_charge != 255) {
            last_level = batt_ev->state_of_charge;
        }
        if (batt_ev->millivolts > 0) {
            last_mv = batt_ev->millivolts;
        }
    }
    const struct zmk_split_peripheral_status_changed *status_ev =
        as_zmk_split_peripheral_status_changed(eh);
    if (status_ev != NULL && !status_ev->connected) {
        last_level = 255; // mat ket noi → hien --
        last_mv = 0;
    }
    return (struct peripheral_battery_state){.level = last_level, .millivolts = last_mv};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_battery, struct peripheral_battery_state,
                            peripheral_battery_update_cb, peripheral_battery_get_state)

ZMK_SUBSCRIPTION(widget_peripheral_battery, zmk_peripheral_battery_state_changed);
ZMK_SUBSCRIPTION(widget_peripheral_battery, zmk_split_peripheral_status_changed);

int zmk_widget_peripheral_battery_init(struct zmk_widget_peripheral_battery *widget,
                                       lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);

    sys_slist_append(&widgets, &widget->node);

    widget_peripheral_battery_init();
    return 0;
}

lv_obj_t *zmk_widget_peripheral_battery_obj(struct zmk_widget_peripheral_battery *widget) {
    return widget->obj;
}
