/*
 * layer_rgb.c — LED doi mau theo layer dang active (16/09)
 *
 * Cach hoat dong:
 *  - Listener (chay tren CENTRAL = nua trai) lang nghe zmk_layer_state_changed
 *  - Lay highest active layer → chon mau nen tu bang layer_colors[]
 *  - Invoke &rgb_ug RGB_COLOR_HSB(h, s, b) qua zmk_behavior_invoke_binding.
 *    Behavior rgb_ug co locality GLOBAL → ZMK tu forward xuong PERIPHERAL
 *    (nua phai, noi co LED thuc) qua split transport — khong can code phia phai.
 *  - set_hsb KHONG ghi flash (da doc source rgb_underglow.c:377) → doi mau
 *    theo layer ca ngay khong mon flash. Flash chi ghi khi chinh tay (debounce).
 *
 * Sentinel sat/brightness = 0xFF → giu nguyen gia tri user dang co
 * (ho tro keymap binding RGB_COLOR_HSB(h, 0xFF, 0xFF)). Bo phan invoke
 * duoc thuc hien qua macro binding chu khong goi set_hsb truc tiep, de
 * di dung nguyen loi forwarding split cua ZMK.
 *
 * Per-LED gradient: engine (rgb_underglow.c — da patch) tu cong offset
 * hue tung LED → 5 LED lech mau dan trong cung tone layer.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/behavior.h>
#include <zmk/rgb_underglow.h>
#include <dt-bindings/zmk/rgb.h>

// Bang mau theo layer index (khop thu tu keymap: 0=default 1=lower
// 2=raise 3=adjust). Chi HUE do layer quyet dinh — sat/brightness lay
// gia tri hien tai (user chinh tay van duoc ton trong).
// calc_hue(0) tra nguyen state.color (h+360 % 360 = giu nguyen).
static const uint16_t layer_hues[] = {
    210, // default: xanh duong nhe
    120, // lower:    xanh la
    40,  // raise:    vang cam
    0,   // adjust:   do (canh bao config)
};

static int layer_rgb_listener(const zmk_event_t *eh) {
    const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    zmk_keymap_layer_index_t highest = zmk_keymap_highest_layer_active();
    if (highest >= sizeof(layer_hues) / sizeof(layer_hues[0])) {
        return ZMK_EV_EVENT_BUBBLE; // layer ngoai bang → bo qua
    }

    // Giu nguyen sat/brightness nguoi dung dang co (state.color dong bo
    // 2 nua moi khi behavior GLOBAL chay — nen doc o central la dung)
    struct zmk_led_hsb cur = zmk_rgb_underglow_calc_hue(0);

    // ⚠️ Force selection ve LED0: set_hsb ghi vao "selected_led" — neu
    // user dang chon LED1-4 ma doi layer, mau layer se lan vao bang
    // rieng cua LED do (sai y dinh). Chon lai LED0 truoc khi set mau.
    struct zmk_behavior_binding sel_reset = {
        .behavior_dev = "rgb_ug",
        .param1 = RGB_LED_SEL_CMD,
        .param2 = 0,
    };

    struct zmk_behavior_binding binding = {
        .behavior_dev = "rgb_ug",
        .param1 = RGB_COLOR_HSB_CMD,
        .param2 = RGB_COLOR_HSB_VAL(layer_hues[highest], cur.s, cur.b),
    };

    struct zmk_behavior_binding_event event = {
        .position = INT32_MAX, // khong gan vi tri vat ly (khong phai phim that)
        .timestamp = k_uptime_get(),
        .layer = highest,
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
    };

    zmk_behavior_invoke_binding(&sel_reset, event, true);
    zmk_behavior_invoke_binding(&sel_reset, event, false);
    zmk_behavior_invoke_binding(&binding, event, true);
    zmk_behavior_invoke_binding(&binding, event, false);

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(layer_rgb, layer_rgb_listener);
ZMK_SUBSCRIPTION(layer_rgb, zmk_layer_state_changed);
