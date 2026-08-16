# Copyright (c) 2026
# SPDX-License-Identifier: MIT

# Custom status screens cho Sofle OLED 128x32
# QUAN TRỌNG: ZMK KHÔNG tự compile file này từ config dir!
# Phải truyền -DZMK_EXTRA_MODULES=<path tới config> cho west build
# (ZMK app sẽ đưa vào ZEPHYR_EXTRA_MODULES → CMakeLists này được include)
# Không có nó, firmware chạy weak stub NULL → OLED nhiễu garbage.

if(CONFIG_ZMK_DISPLAY AND CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM)
  # Compile vào target "app" (giống ZMK core widgets) — target app đã có
  # đầy đủ include paths đúng cho zmk/*.h + lvgl.h, không bị lỗi LV_CONF_PATH
  # như khi dùng zephyr_library() riêng.
  # Lưu ý: mỗi file .c phải #include <lvgl.h> TRƯỚC zmk headers (macro "E"
  # trong keys.h phá LV_CONF_PATH "E:/..." trên Windows).

  if(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    # Màn trái: modifiers + HID indicators (không cần LV_USE_IMG)
    target_sources(app PRIVATE widgets/modifiers.c widgets/hid_indicators.c)
    target_sources(app PRIVATE status_screen_left.c)
  else()
    # Màn phải: bongo cat animimg (cần CONFIG_LV_USE_IMG + LV_USE_ANIMIMG)
    target_sources(app PRIVATE widgets/bongo_cat.c widgets/bongo_cat_images.c)
    target_sources(app PRIVATE status_screen_right.c)
  endif()
endif()
