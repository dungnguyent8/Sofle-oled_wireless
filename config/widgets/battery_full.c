/*
 * Widget battery_full - icon pin theo muc + phan tram, mot dong giong
 * "[icon] 87%" (widget goc ZMK chi hien HOAC icon HOAC %, khong ca hai —
 * yeu cau rieng cua man trai doc).
 * Icon theo muc: FULL/3/2/1/EMPTY + dang sac hien LV_SYMBOL_CHARGE.
 * Nguon state: listen battery_state_changed + usb_conn_state_changed.
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>

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
#include <zmk/events/layer_state_changed.h>

#include "battery_full.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

// Trung binh truot 5 mau mV — tri voltage sag khi BLE TX / OLED refresh
#define MV_FILTER_LEN 5
static uint16_t mv_samples[MV_FILTER_LEN];
static uint8_t mv_sample_count = 0;

/*
 * Curve LiPo 1S piecewise (mV -> %) — thay cho linear `mv*2/15-459`
 * cua ZMK. LiPo thuc te co plateau dai 3.7-3.9V (chiem ~70% dung luong)
 * nen duong thang sai +-10-15% o giua dai. Bang diem tong hop tu
 * discharge graph LiPo 1S chuan (Adafruit) + noi suy tuyen tinh giua cac diem.
 * Co the tinh chinh theo pin 5000mAh cu the sau 1-2 chu ky sac/xoa
 * (soi mV thuc te o trang adjust roi doi bang duoi).
 */
#define LIPO_POINTS 14
static const uint16_t lipo_mv[LIPO_POINTS] = {3450, 3500, 3550, 3600, 3650, 3700,
                                              3750, 3800, 3850, 3900, 3950, 4000, 4100, 4200};
static const uint8_t lipo_pct[LIPO_POINTS] = {0,  2,  5,  9,  16, 25,
                                              36, 48, 60, 68, 74, 80, 92, 100};

static uint8_t lipo_mv_to_pct(uint16_t mv) {
    if (mv >= lipo_mv[LIPO_POINTS - 1]) {
        return 100;
    }
    if (mv <= lipo_mv[0]) {
        return 0;
    }
    for (int i = 1; i < LIPO_POINTS; i++) {
        if (mv < lipo_mv[i]) {
            // noi suy tuyen tinh trong doan [i-1, i]
            const uint32_t span_mv = lipo_mv[i] - lipo_mv[i - 1];
            const uint32_t span_pct = lipo_pct[i] - lipo_pct[i - 1];
            const uint32_t off = mv - lipo_mv[i - 1];
            return (uint8_t)(lipo_pct[i - 1] + (off * span_pct) / span_mv);
        }
    }
    return 100;
}

// Layer ADJUST (index 3 — khớp ADJUST_LAYER_INDEX trong status_screen_left.c):
// khi active thi label pin trai thanh 2 dong "%\nmV", thoat thi ve "icon %"
#define BATT_ADJUST_LAYER 3
static bool show_mv_mode = false;

struct battery_full_state {
    uint8_t level;     // % theo curve LiPo moi (khong dung % cua ZMK core)
    bool usb_present;
    uint16_t millivolts;
    bool show_mv;      // true = layer adjust → hien ca % va mV (2 dong)
};

static void set_battery_full(lv_obj_t *label, struct battery_full_state state) {
    // Buffer: "icon 100%\n4.20V \u26a1" worst-case ~19B
    char text[24] = {};

    uint8_t level = state.level;
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

    // Mac dinh: 1 dong "icon %". Layer ADJUST: 2 dong "%\nmV" —
    // 1 dong "icon % V" = ~79px TRAN man doc 64px (do adv_w Montserrat 12,
    // verify 23/08). Charge icon xuong cuoi dong 2 khi adjust,
    // giu o dong 1 khi default.
    if (state.show_mv) {
        snprintf(text, sizeof(text), "%s %u%%\n%u.%02uV", icon, level,
                 state.millivolts / 1000, (state.millivolts % 1000) / 10);
        if (state.usb_present) {
            strcat(text, " " LV_SYMBOL_CHARGE);
        }
    } else {
        snprintf(text, sizeof(text), "%s %u%%", icon, level);
        if (state.usb_present) {
            strcat(text, " " LV_SYMBOL_CHARGE);
        }
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

    // Layer event cung di qua get_state nay (widget sub layer event) —
    // cap nhat co show_mv truc tiep tu event, label ve lai NGAY khi
    // vao/thoat adjust khong phai doi battery report 60s ke tiep
    const struct zmk_layer_state_changed *layer_ev = as_zmk_layer_state_changed(eh);
    if (layer_ev != NULL && layer_ev->layer == BATT_ADJUST_LAYER) {
        show_mv_mode = layer_ev->state;
    }

    uint16_t mv = 0;
#if DT_HAS_CHOSEN(zmk_battery)
    // Doc SENSOR_CHAN_GAUGE_VOLTAGE (KHONG PHAI SENSOR_CHAN_VOLTAGE!) —
    // driver vbatt chi chap nhan GAUGE_* chans; goi sai channel thi fetch
    // tra -ENOTSUP → mV luon 0 (da te: hien "0.00V 95%")
    static const struct device *batt_dev = NULL;
    if (batt_dev == NULL) {
        batt_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_battery));
    }
    if (device_is_ready(batt_dev)) {
        struct sensor_value val;
        if (sensor_sample_fetch_chan(batt_dev, SENSOR_CHAN_GAUGE_VOLTAGE) == 0 &&
            sensor_channel_get(batt_dev, SENSOR_CHAN_GAUGE_VOLTAGE, &val) == 0) {
            mv = (uint16_t)(val.val1 * 1000 + val.val2 / 1000);
        }
    }
#endif
    // Loc trung binh truot: chong %/mV nhay loan do voltage sag
    if (mv > 0) {
        if (mv_sample_count < MV_FILTER_LEN) {
            mv_samples[mv_sample_count++] = mv;
        } else {
            for (int i = 1; i < MV_FILTER_LEN; i++) {
                mv_samples[i - 1] = mv_samples[i];
            }
            mv_samples[MV_FILTER_LEN - 1] = mv;
        }
    }
    uint32_t sum = 0;
    for (int i = 0; i < mv_sample_count; i++) {
        sum += mv_samples[i];
    }
    uint16_t filtered_mv = mv_sample_count ? (uint16_t)(sum / mv_sample_count) : 0;

    return (struct battery_full_state){
        // % tu curve LiPo moi tinh tren mV da loc (bo % linear cua ZMK core)
        .level = filtered_mv ? lipo_mv_to_pct(filtered_mv)
                             : ((ev != NULL) ? ev->state_of_charge
                                             : zmk_battery_state_of_charge()),
        .usb_present = zmk_usb_is_powered(),
        .millivolts = filtered_mv,
        .show_mv = show_mv_mode,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_full, struct battery_full_state,
                            battery_full_update_cb, battery_full_get_state)

ZMK_SUBSCRIPTION(widget_battery_full, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(widget_battery_full, zmk_usb_conn_state_changed);
ZMK_SUBSCRIPTION(widget_battery_full, zmk_layer_state_changed);

int zmk_widget_battery_full_init(struct zmk_widget_battery_full *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);

    sys_slist_append(&widgets, &widget->node);

    widget_battery_full_init();
    return 0;
}

lv_obj_t *zmk_widget_battery_full_obj(struct zmk_widget_battery_full *widget) {
    return widget->obj;
}
