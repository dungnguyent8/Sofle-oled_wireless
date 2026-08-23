import re

with open('e:/project/keyboard/Sofle-oled_wireless/modules/lib/gui/lvgl/src/font/lv_font_montserrat_12.c',
          encoding='utf-8', errors='ignore') as f:
    head = f.read(700)
m = re.search(r'FontAwesome5.*?-r ([0-9,]+)', head)
font_codes = set(m.group(1).split(','))

with open('e:/project/keyboard/Sofle-oled_wireless/modules/lib/gui/lvgl/src/font/lv_symbol_def.h',
          encoding='utf-8', errors='ignore') as f:
    for line in f:
        mm = re.search(r'#define LV_SYMBOL_(\w+)\s+"[^"]*"/\*(\d+)', line)
        if mm and mm.group(2) in font_codes:
            print(mm.group(2), 'LV_SYMBOL_' + mm.group(1))
