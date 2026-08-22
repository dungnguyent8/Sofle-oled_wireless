/*
 * display_rotation.c — Xoay noi dung OLED trai 90° ben phai (top -> phai)
 * => UI logic thanh man DUNG 64 x 128, vat ly van 128 x 64.
 *
 * Vi sao khong dung Devicetree/LVGL san co:
 *   - SH1106/SSD1306 chi co lenh lat 0°/180° (segment-remap / com-invdir),
 *     KHONG co lenh xoay 90°.
 *   - LVGL sw_rotate (LV_DISP_ROT_90) chi dung cho buffer mau; man mono dung
 *     set_px_cb (VTILED) se bi ghi rac.
 *
 * Cach lam: sau khi LVGL glue init, lay driver default, doi hor_res=64,
 * ver_res=128 (man logic dung), giu set_px_cb + rounder_cb goc cua Zephyr
 * (render binh thuong vao buffer VTILED 64 doc), roi thay flush_cb bang ham
 * transpose: doc pixel logic (lx,ly) -> ghi pixel vat ly (px=ly, py=63-lx).
 * Buffer vat ly la full-frame va persistent; khi flush_is_last moi ghi
 * 1024 byte xuong SH1106 qua display_write (1 lan/frame).
 *
 * Goi tu status_screen_left.c ( dau tien trong zmk_display_status_screen)
 * — truoc khi widgetbat dau tao, de kich thuoc screen = 64x128.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// PHẢI include lvgl.h TRƯỚC mọi zmk header: keys.h định nghĩa macro "E"
// phá LV_CONF_PATH "E:/..." trên Windows (xem docs/LOCAL_BUILD_SETUP.md)
#include <lvgl.h>

#define LOGICAL_W 64  // man logic (sau xoay): rong 64
#define LOGICAL_H 128 // man logic (sau xoay): cao 128

#define PHYS_W 128 // man vat ly SH1106
#define PHYS_H 64
#define PHYS_BUF_SIZE (PHYS_W * PHYS_H / 8) // 1024 bytes

static const struct device *display_dev;
static uint8_t phys_buf[PHYS_BUF_SIZE];

/*
 * Mapping pixel logic -> vat ly (xoay CW 90°: canh top cu thanh canh phai):
 *   px = (LOGICAL_H - 1) - ly ;  py = lx
 * (check: goc tren-trai logic (0,0) -> (127,0) = tren-phai vat ly ✓)
 *
 * Buffer logic: VTILED LSB-first (bit = y%8, byte-row = y/8, buf_w = width
 * cua area) — dung quy uoc set_px_cb goc cua Zephyr glue (toa do TIEU DOI
 * theo goc area, xem lv_draw_sw_blend.c: lv_area_move(-buf_area->x1,...)).
 * Buffer vat ly: page-major SH1106 (byte = px + (py/8)*128, bit = py%8).
 */
static void rotated_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area,
                             lv_color_t *color_p) {
    const int aw = lv_area_get_width(area);
    const uint8_t *buf = (const uint8_t *)color_p;

    for (int ly = area->y1; ly <= area->y2; ly++) {
        const int brow = (ly - area->y1) / 8;               // y1 aligned 8 boi rounder
        const uint8_t bit = (uint8_t)((ly - area->y1) % 8); // LSB-first VTILED
        const uint8_t *row = &buf[brow * aw];

        for (int lx = area->x1; lx <= area->x2; lx++) {
            const bool on = (row[lx - area->x1] >> bit) & 1;
            const int px = (LOGICAL_H - 1) - ly;
            const int py = lx;
            uint8_t *dst = &phys_buf[px + (py / 8) * PHYS_W];
            if (on) {
                *dst |= (uint8_t)BIT(py % 8);
            } else {
                *dst &= (uint8_t)~BIT(py % 8);
            }
        }
    }

#if IS_ENABLED(CONFIG_ZMK_DISPLAY_ROTATE_DIAG)
    // DIAGNOSTIC: de dich pattern kiem tra DUONG TRUYEN (xoa khi xong):
    // ghi de toan bo phys_buf thanh pattern 4 o vuong goc + khung rong
    // (khong doc tu LVGL) — neu pattern hien SACH thi pipeline LVGL
    // se la thu pham; neu pattern NHIEM cham thi loi tai transpose hoac
    // driver GRAM SH1106.
    memset(phys_buf, 0, PHYS_BUF_SIZE);
    // 4 o vuong 8x8 o 4 goc vat ly (trai-tren, phai-tren, trai-duoi, phai-duoi)
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            phys_buf[x + (y / 8) * PHYS_W] |= (uint8_t)BIT(y % 8);
            phys_buf[(127 - x) + (y / 8) * PHYS_W] |= (uint8_t)BIT(y % 8);
            phys_buf[x + ((63 - y) / 8) * PHYS_W] |= (uint8_t)BIT((63 - y) % 8);
            phys_buf[(127 - x) + ((63 - y) / 8) * PHYS_W] |= (uint8_t)BIT((63 - y) % 8);
        }
#endif

    // Khi het frame (area cuoi) moi ghi ca man xuong SH1106 — 1 lan I2C/frame
    if (lv_disp_flush_is_last(disp_drv)) {
        struct display_buffer_descriptor desc = {
            .buf_size = PHYS_BUF_SIZE,
            .width = PHYS_W,
            .height = PHYS_H,
            .pitch = PHYS_W,
        };
        display_write(display_dev, 0, 0, &desc, phys_buf);
    }

    lv_disp_flush_ready(disp_drv);
}

int zmk_display_rotation_setup(void) {
    display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev)) {
        LOG_ERR("rotate90: display not ready");
        return -ENODEV;
    }

    lv_disp_t *disp = lv_disp_get_default();
    if (disp == NULL || disp->driver == NULL) {
        LOG_ERR("rotate90: no lvgl display");
        return -ENODEV;
    }

    lv_disp_drv_t *drv = disp->driver;
    drv->hor_res = LOGICAL_W;
    drv->ver_res = LOGICAL_H;
    drv->flush_cb = rotated_flush_cb;
    drv->full_refresh = 0;

    // Resize tat ca screens ve 64x128 + invalidate toan bo man
    lv_disp_drv_update(disp, drv);

    LOG_INF("rotate90: display logic 64x128 -> phys 128x64");
    return 0;
}
