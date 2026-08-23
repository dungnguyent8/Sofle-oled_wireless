/*
 * Custom status screen MAN TRAI (central)
 *
 * 2 che do layout:
 *  - MAC DINH (OLED ngang 128x64, khi ROTATE_90 tat): icon output + modifiers
 *    + battery | HID indicators duoi trai | layer duoi phai
 *  - XOAY 90° CW (CONFIG_ZMK_DISPLAY_ROTATE_90_RIGHT, man logic doc 64x128):
 *    thanh doc theo thu tu tu tren xuong:
 *      [ 🔋 85% ⚡ ]     (pin trái: icon mức + % curve LiPo; ADJUST → 2 dòng %\nmV)
 *      [ ▶ USB/BT ]      (output — hiện ở layer thường, ẩn khi ADJUST)
 *      [  S+C+A+G  ]   (modifiers 1 ký tự, đủ 1 hàng)
 *      [ ( bongo ) ]   (bongo cat 50x26 — mèo gõ phím theo keystroke)
 *      [ 👁 ]              (CAPS icon — chỉ khi bật)
 *      [ default ]        (layer name)
 *    Khi ADJUST (giữ raise+lower): ẩn bongo/modifiers/CAPS/output, chèn
 *    trang info: pin phải (% curve LiPo) + uptime + "adjust"
 *    (23/08 đã bỏ CPU%/RAM% MCU theo yêu cầu; mV phải đã bỏ — char BLE
 *    không discovery được, xem docs)
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display/status_screen.h>
#include <zmk/events/layer_state_changed.h>

#include <zmk/display/widgets/output_status.h>
#include <zmk/display/widgets/battery_status.h>
#include <zmk/display/widgets/layer_status.h>

#include "widgets/modifiers.h"
#include "widgets/hid_indicators.h"
#include "widgets/battery_full.h"
#include "widgets/peripheral_battery.h"
#include "widgets/bongo_cat.h"

// Xoay 90° man trai (config/Kconfig.sofle: ZMK_DISPLAY_ROTATE_90_RIGHT)
int zmk_display_rotation_setup(void);

static struct zmk_widget_output_status output_status_widget;
#if !IS_ENABLED(CONFIG_ZMK_DISPLAY_ROTATE_90_RIGHT)
static struct zmk_widget_battery_status battery_status_widget; // layout ngang dung widget goc
#endif
static struct zmk_widget_battery_full battery_full_widget; // layout doc: icon + mV
static struct zmk_widget_layer_status layer_status_widget;
static struct zmk_widget_modifiers modifiers_widget;
static struct zmk_widget_hid_indicators hid_indicators_widget;
static struct zmk_widget_bongo_cat bongo_cat_widget;
static struct zmk_widget_peripheral_battery peripheral_battery_widget;

// ==== Trang info layer ADJUST ====
// Chi hien khi ADJUST (index 3 — nhan raise+lower cung luc): output trai,
// pin phai, uptime. Bongo cat + modifiers + CAPS an di trong luc do.
#define ADJUST_LAYER_INDEX 3
static lv_obj_t *right_info_container;
static lv_obj_t *bongo_cat_obj;
static lv_obj_t *uptime_label;
static lv_obj_t *modifiers_obj;
static lv_obj_t *caps_obj;
static lv_obj_t *output_obj; // hiện layer thường, ẩn khi adjust (đổi chỗ trang info)
static bool adjust_active = false;

static void update_right_info_visibility() {
    if (right_info_container == NULL) {
        return;
    }
    if (adjust_active) {
        lv_obj_clear_flag(right_info_container, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(right_info_container, LV_OBJ_FLAG_HIDDEN);
    }
    if (bongo_cat_obj != NULL) {
        if (adjust_active) {
            lv_obj_add_flag(bongo_cat_obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(bongo_cat_obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (modifiers_obj != NULL) {
        if (adjust_active) {
            lv_obj_add_flag(modifiers_obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(modifiers_obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (caps_obj != NULL) {
        if (adjust_active) {
            lv_obj_add_flag(caps_obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(caps_obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
    // Output (USB/BT): an khi adjust — doi cho cho % + mV nua phai
    // (yeu cau 23/08; o layer thuong van luon hien binh thuong)
    if (output_obj != NULL) {
        if (adjust_active) {
            lv_obj_add_flag(output_obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(output_obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Uptime "Up 3d2h" / "Up 5h12m" / "Up 12m" — refresh 1s/lan CHI khi adjust
static void uptime_refresh_cb(lv_timer_t *timer) {
    if (!adjust_active || uptime_label == NULL) {
        return;
    }
    const uint32_t s = (uint32_t)(k_uptime_get() / 1000);
    const uint32_t d = s / 86400;
    const uint32_t h = (s % 86400) / 3600;
    const uint32_t m = (s % 3600) / 60;
    char text[16] = {};
    if (d > 0) {
        snprintf(text, sizeof(text), LV_SYMBOL_REFRESH " Up %ud%uh", d, h);
    } else if (h > 0) {
        snprintf(text, sizeof(text), LV_SYMBOL_REFRESH " Up %uh%um", h, m);
    } else {
        snprintf(text, sizeof(text), LV_SYMBOL_REFRESH " Up %um", m);
    }
    lv_label_set_text(uptime_label, text);
}

static int layer_state_handler(const zmk_event_t *eh) {
    const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    if (ev->layer == ADJUST_LAYER_INDEX) {
        adjust_active = ev->state;
        // defer qua LVGL — chay tai display thread cho an toan
        if (zmk_display_is_initialized()) {
            lv_async_call(update_right_info_visibility, NULL);
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(right_info_layer, layer_state_handler);
ZMK_SUBSCRIPTION(right_info_layer, zmk_layer_state_changed);

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;
    screen = lv_obj_create(NULL);

    // Ap dung xoay 90° TRUOC khi tao widget — screen size phai la 64x128
    zmk_display_rotation_setup();

#if IS_ENABLED(CONFIG_ZMK_DISPLAY_ROTATE_90_RIGHT)
    // ==== LAYOUT MAN DOC 64x128 (da xoay 90° CW) ====
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(screen, 0, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // Cot chinh giua man, spacing deu, padding 2px
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 2, LV_PART_MAIN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // Pin trai: icon muc + % (curve LiPo); layer ADJUST thanh 2 dong "%\nmV"
    zmk_widget_battery_full_init(&battery_full_widget, screen);
    lv_obj_set_style_pad_top(zmk_widget_battery_full_obj(&battery_full_widget), 2, LV_PART_MAIN);

    // Output (USB/BT + profile): LUON hien o layer thuong; vao adjust
    // thi an di doi cho (xem update_right_info_visibility)
#if IS_ENABLED(CONFIG_ZMK_WIDGET_OUTPUT_STATUS)
    zmk_widget_output_status_init(&output_status_widget, screen);
    output_obj = zmk_widget_output_status_obj(&output_status_widget);
#endif

    // Trang info chi hien khi ADJUST — chieu cao theo noi dung,
    // khong center cung dinh de tran duoi (da te: uptime bi adjust che)
    right_info_container = lv_obj_create(screen);
    lv_obj_set_size(right_info_container, 60, LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(right_info_container, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(right_info_container, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(right_info_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(right_info_container, 0, LV_PART_MAIN);
    lv_obj_clear_flag(right_info_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(right_info_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right_info_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(right_info_container, LV_OBJ_FLAG_HIDDEN); // an mac dinh

    zmk_widget_peripheral_battery_init(&peripheral_battery_widget, right_info_container);

    uptime_label = lv_label_create(right_info_container);
    lv_label_set_text(uptime_label, "");
    static lv_timer_t *uptime_timer = NULL;
    if (uptime_timer == NULL) {
        uptime_timer = lv_timer_create(uptime_refresh_cb, 1000, NULL);
    }

    // Modifiers (S+C+A+G) — an khi ADJUST
    zmk_widget_modifiers_init(&modifiers_widget, screen);
    modifiers_obj = zmk_widget_modifiers_obj(&modifiers_widget);

    // Bongo cat — an khi ADJUST
    zmk_widget_bongo_cat_init(&bongo_cat_widget, screen);
    bongo_cat_obj = zmk_widget_bongo_cat_obj(&bongo_cat_widget);

    // HID indicators (chi CAPS icon) — an khi ADJUST
    zmk_widget_hid_indicators_init(&hid_indicators_widget, screen);
    caps_obj = zmk_widget_hid_indicators_obj(&hid_indicators_widget);

#if IS_ENABLED(CONFIG_ZMK_WIDGET_LAYER_STATUS)
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_set_style_pad_bottom(zmk_widget_layer_status_obj(&layer_status_widget), 2,
                                LV_PART_MAIN);
#endif

#else
    // ==== LAYOUT MAC DINH NGANG 128x64 ====
#if IS_ENABLED(CONFIG_ZMK_WIDGET_BATTERY_STATUS)
    zmk_widget_battery_status_init(&battery_status_widget, screen);
    lv_obj_align(zmk_widget_battery_status_obj(&battery_status_widget), LV_ALIGN_TOP_RIGHT, 0, 0);
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_OUTPUT_STATUS)
    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_LEFT, 0, 0);
#endif

    zmk_widget_modifiers_init(&modifiers_widget, screen);
    lv_obj_align(zmk_widget_modifiers_obj(&modifiers_widget), LV_ALIGN_TOP_MID, 0, 0);

    zmk_widget_hid_indicators_init(&hid_indicators_widget, screen);
    lv_obj_align(zmk_widget_hid_indicators_obj(&hid_indicators_widget), LV_ALIGN_BOTTOM_LEFT, 0,
                 0);

#if IS_ENABLED(CONFIG_ZMK_WIDGET_LAYER_STATUS)
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#endif
#endif /* CONFIG_ZMK_DISPLAY_ROTATE_90_RIGHT */

    return screen;
}
