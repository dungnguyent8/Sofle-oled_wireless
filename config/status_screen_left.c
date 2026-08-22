/*
 * Custom status screen MAN TRAI (central)
 *
 * 2 che do layout:
 *  - MAC DINH (OLED ngang 128x64): icon output + modifiers + battery |
 *    HID indicators duoi trai | layer duoi phai
 *  - XOAY 90° CW (CONFIG_ZMK_DISPLAY_ROTATE_90_RIGHT, man logic doc 64x128):
 *    thanh doc theo thu tu tu tren xuong:
 *      [output icon]  [ 87% pin ]
 *      [  Sh + Ct +   ]   (modifiers, wrap)
 *      [  Ct + Al     ]
 *      [ X ✓ ↑ ]            (HID indicators)
 *      [ default ]          (layer name)
 * Port kieu truc tu dongle_display cho Sofle
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display/status_screen.h>

#include <zmk/display/widgets/output_status.h>
#include <zmk/display/widgets/battery_status.h>
#include <zmk/display/widgets/layer_status.h>

#include "widgets/modifiers.h"
#include "widgets/hid_indicators.h"

// Xoay 90° man trai (config/Kconfig.sofle: ZMK_DISPLAY_ROTATE_90_RIGHT)
int zmk_display_rotation_setup(void);

static struct zmk_widget_output_status output_status_widget;
static struct zmk_widget_battery_status battery_status_widget;
static struct zmk_widget_layer_status layer_status_widget;
static struct zmk_widget_modifiers modifiers_widget;
static struct zmk_widget_hid_indicators hid_indicators_widget;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;
    screen = lv_obj_create(NULL);

    // Ap dung xoay 90° TRUOC khi tao widget — screen size phai la 64x128
    zmk_display_rotation_setup();

#if IS_ENABLED(CONFIG_ZMK_DISPLAY_ROTATE_90_RIGHT)
    // ==== LAYOUT MAN DOC 64x128 (da xoay 90° CW) ====
    // Tat scrollbar + border mac dinh cua screen object — residue cua chung
    // hien thanh cham tran tren man (khong scroll duoc thi khong nen hien)
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(screen, 0, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // Cot chinh giua man, spacing deu, padding 2px cho thoang
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 2, LV_PART_MAIN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

#if IS_ENABLED(CONFIG_ZMK_WIDGET_BATTERY_STATUS)
    zmk_widget_battery_status_init(&battery_status_widget, screen);
    lv_obj_set_style_pad_top(zmk_widget_battery_status_obj(&battery_status_widget), 2,
                             LV_PART_MAIN);
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_OUTPUT_STATUS)
    zmk_widget_output_status_init(&output_status_widget, screen);
#endif

    // Modifiers (Sh + Ct + ...) — wrap tu dong tren man 60px rong
    zmk_widget_modifiers_init(&modifiers_widget, screen);

    // HID indicators (Caps/Num/Scroll) — giua man
    zmk_widget_hid_indicators_init(&hid_indicators_widget, screen);

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

    // Modifiers (S/C/A/G khi giu) — canh giua phia tren
    zmk_widget_modifiers_init(&modifiers_widget, screen);
    lv_obj_align(zmk_widget_modifiers_obj(&modifiers_widget), LV_ALIGN_TOP_MID, 0, 0);

    // HID indicators (Caps/Num/Scroll) — duoi trai
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
