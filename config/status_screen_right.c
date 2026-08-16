/*
 * Custom status screen MAN PHAI (peripheral) — BONGO CAT!
 * Layout man 128x32:
 *   [ meo bongo 50x26 canh giua ]           [ 87% pin ]
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display/status_screen.h>

#include <zmk/display/widgets/battery_status.h>
#include <zmk/display/widgets/peripheral_status.h>

#include "widgets/bongo_cat.h"

static struct zmk_widget_battery_status battery_status_widget;
static struct zmk_widget_peripheral_status peripheral_status_widget;
static struct zmk_widget_bongo_cat bongo_cat_widget;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;
    screen = lv_obj_create(NULL);

#if IS_ENABLED(CONFIG_ZMK_WIDGET_BATTERY_STATUS)
    zmk_widget_battery_status_init(&battery_status_widget, screen);
    lv_obj_align(zmk_widget_battery_status_obj(&battery_status_widget), LV_ALIGN_TOP_RIGHT, 0, 0);
#endif

    // Trang thai ket noi ve central: WiFi + check/X (goc tren trai)
    zmk_widget_peripheral_status_init(&peripheral_status_widget, screen);
    lv_obj_align(zmk_widget_peripheral_status_obj(&peripheral_status_widget), LV_ALIGN_TOP_LEFT,
                 0, 0);

    // Meo danh may chinh giua
    zmk_widget_bongo_cat_init(&bongo_cat_widget, screen);
    lv_obj_align(zmk_widget_bongo_cat_obj(&bongo_cat_widget), LV_ALIGN_CENTER, 0, 0);

    return screen;
}
