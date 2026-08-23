/*
 * Widget waveform_wpm - bieu do toc do go phim kieu audio-visualizer
 * 16 cot bar (moi cot 3px + 1px gap = 64px rong dung man doc), moi cot
 * la WPM trong 1 cua so 500ms gan nhat; moi cua so moi, toan bo chart
 * tr跳水 phai sang trai (cot moi xuat hien ben phai).
 * Nguon du lieu: dem keystroke tu zmk_position_state_changed (chinh xac
 * tren ca central), tinh WPM = so phim x 12 / so phut (chuẩn 5 ky tu/tu).
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
#include <zmk/events/position_state_changed.h>

#include "waveform_wpm.h"

#define BAR_COUNT 16    // so cot bar
#define BAR_WIDTH 3     // rong moi cot (px)
#define BAR_GAP 1       // khoang cach giua cot (px)
#define MAX_BAR_H 40    // chieu cao toi da bar (px) — de chọt cho widget khac
#define WINDOW_MS 500   // 1 cua so = 500ms; WPM tinh tren cua so nay

// vung WPM toi da de scale bar: 120 WPM = full height (go rat nhanh)
#define WPM_FULL_SCALE 120

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

// lich su WPM moi cua so (ring). index 0 = cu nhat, cuoi = moi nhat
static uint8_t wpm_history[BAR_COUNT];
static uint32_t window_keystrokes = 0;
static int64_t window_start = 0;

struct waveform_state {
    uint32_t dummy; // state khong can thiet — ticks do event dem trong get_state
};

static void waveform_update(struct zmk_widget_waveform_wpm *widget) {
    // cap nhat chieu cao tung bar theo history
    for (int i = 0; i < BAR_COUNT; i++) {
        const uint8_t wpm = wpm_history[i];
        // scale: WPM_FULL_SCALE -> MAX_BAR_H px, min 2px khi co hoat dong
        // (1px qua mo tren man OLED)
        const lv_coord_t h =
            wpm == 0 ? 1 : LV_MAX(2, (lv_coord_t)((uint32_t)wpm * MAX_BAR_H / WPM_FULL_SCALE));
        lv_obj_set_height(widget->bars[i], h);
        // QUAN TRONG: phai re-align sau khi doi height (align tinh tu top-left,
        // doi height khong tu neo day) — va offset phai la cua TUNG bar,
        // BOTTOM_MID 0,0 se chon het 16 bar ve giua man
        lv_obj_align(widget->bars[i], LV_ALIGN_BOTTOM_LEFT, i * (BAR_WIDTH + BAR_GAP), 0);
    }
}

static void waveform_update_cb(struct waveform_state state) {
    // Khong lam gi tren event — bar update theo timer 500ms (history shift
    // moi cua so). Event chi can bien dem tang trong get_state.
    struct zmk_widget_waveform_wpm *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { waveform_update(widget); }
}

static struct waveform_state waveform_get_state(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev != NULL && ev->state) {
        window_keystrokes++;
    }
    return (struct waveform_state){.dummy = 0};
}

// May widget chinh hang ZMK: listener chay trong display work queue
ZMK_DISPLAY_WIDGET_LISTENER(widget_waveform_wpm, struct waveform_state,
                            waveform_update_cb, waveform_get_state)

ZMK_SUBSCRIPTION(widget_waveform_wpm, zmk_position_state_changed);

// moi cua so WINDOW_MS: tinh WPM cua cua so vua roi, day vao history,
// reset bo dem. Timer chay trong LVGL context nen khong can lock.
static void waveform_timer_cb(lv_timer_t *timer) {
    const int64_t now = k_uptime_get();

    if (window_start == 0) {
        window_start = now;
        return;
    }

    const int64_t elapsed = now - window_start;
    if (elapsed < WINDOW_MS) {
        return;
    }

    // WPM = keystrokes x (60s / minute) / 5 (ky tu/tu) / (elapsed/1000s)
    // = keystrokes x 12 x 1000 / elapsed
    const uint32_t wpm = window_keystrokes * 12000U / (uint32_t)elapsed;
    const uint8_t clamped = wpm > 255 ? 255 : (uint8_t)wpm;

    // dich history: bo cu nhat, them moi nhat vao cuoi
    for (int i = 0; i < BAR_COUNT - 1; i++) {
        wpm_history[i] = wpm_history[i + 1];
    }
    wpm_history[BAR_COUNT - 1] = clamped;

    window_keystrokes = 0;
    window_start = now;

    struct zmk_widget_waveform_wpm *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { waveform_update(widget); }
}

int zmk_widget_waveform_wpm_init(struct zmk_widget_waveform_wpm *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 64, MAX_BAR_H + 4);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);
    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < BAR_COUNT; i++) {
        widget->bars[i] = lv_obj_create(widget->obj);
        lv_obj_set_size(widget->bars[i], BAR_WIDTH, 1);
        lv_obj_set_style_radius(widget->bars[i], 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(widget->bars[i], 0, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(widget->bars[i], LV_OPA_COVER, LV_PART_MAIN);
        // BG PHAI LA MAU DEN: theme mono default la nen trang — bar trang
        // tren nen trang = vo han (day la ly do waveform "khong hien thi")
        lv_obj_set_style_bg_color(widget->bars[i], lv_color_black(), LV_PART_MAIN);
        lv_obj_align(widget->bars[i], LV_ALIGN_BOTTOM_LEFT, i * (BAR_WIDTH + BAR_GAP), 0);
        lv_obj_clear_flag(widget->bars[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    sys_slist_append(&widgets, &widget->node);

    static lv_timer_t *timer = NULL;
    if (timer == NULL) {
        window_start = k_uptime_get();
        timer = lv_timer_create(waveform_timer_cb, WINDOW_MS, NULL);
    }

    widget_waveform_wpm_init();
    return 0;
}

lv_obj_t *zmk_widget_waveform_wpm_obj(struct zmk_widget_waveform_wpm *widget) {
    return widget->obj;
}
