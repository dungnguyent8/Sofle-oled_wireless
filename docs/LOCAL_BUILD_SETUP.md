# Hướng dẫn build firmware ZMK local trên Windows (không cần GitHub Actions)

> Viết cho repo **Sofle-oled_wireless** — bàn phím PandaKB Sofle RGB MX v1 + ProMicro nRF52840
> (clone nice!nano v2, bootloader UF2). Áp dụng chung cho mọi shield ZMK build board `nice_nano_v2`.
>
> Làm theo đúng từng bước — đã verify thành công trên Windows 10/11 + Git Bash.
> Tổng thời gian ~30-45 phút (tùy tốc độ mạng khi tải SDK + source).

## Tổng quan cần cài gì

| Thành phần | Phiên bản đã verify | Ghi chú |
|---|---|---|
| Git for Windows | bất kỳ | có sẵn nếu dùng Git Bash |
| Python 3.12 | 3.12.4 | dùng venv riêng, KHÔNG dùng anaconda base |
| CMake | 3.31.10 | ⚠️ cài qua pip, bản 4.x bị Zephyr 3.5 từ chối |
| Ninja | 1.13 | pip |
| west | 1.5.0 | pip |
| setuptools | **< 81** | ⚠️ bản 81+ đã XÓA `pkg_resources` → nanopb build fail |
| protobuf + grpcio-tools | 7.x | cần cho ZMK Studio (nanopb generator) |
| Zephyr SDK 0.16.3 | monolithic | chứa ARM GCC 12.2 + mọi toolchain |
| dtc 1.7.2 | 1.7.2 | ⚠️ SDK Windows KHÔNG kèm dtc.exe — cài qua MSYS2 |

---

## Bước 1 — Chuẩn bị folder

Đặt repo và toolchain cạnh nhau cho gọn:

```
E:\project\keyboard\
├── Sofle-oled_wireless\      ← repo config (clone từ GitHub)
└── zmk-build-tools\          ← toolchain (tạo ở bước 2-4, KHÔNG commit lên git)
    ├── venv\
    └── zephyr-sdk-0.16.3\
```

## Bước 2 — Tạo venv Python + cài packages

Mở **Git Bash**:

```bash
python -m venv e:/project/keyboard/zmk-build-tools/venv

# Cài toolchain Python — CHÚ Ý thứ tự và version pin
e:/project/keyboard/zmk-build-tools/venv/Scripts/pip.exe install west "cmake<4" ninja pyelftools
e:/project/keyboard/zmk-build-tools/venv/Scripts/pip.exe install "setuptools<81"
e:/project/keyboard/zmk-build-tools/venv/Scripts/pip.exe install protobuf grpcio-tools

# Verify
e:/project/keyboard/zmk-build-tools/venv/Scripts/cmake.exe --version   # phải ra 3.x
e:/project/keyboard/zmk-build-tools/venv/Scripts/python.exe -c "import pkg_resources; import google.protobuf; print('OK')"
```

### ⚠️ 3 cái bẫy ở bước này (đã té cả 3)

1. **CMake 4.x không dùng được** — Zephyr 3.5 yêu cầu `cmake >=3.20 và <4`. Nếu máy đã cài
   CMake 4.x ngoài venv, bản pip `cmake<4` trong venv sẽ override khi PATH đúng.
2. **setuptools ≥81 xóa `pkg_resources`** → nanopb generator fail giữa chừng với
   `ModuleNotFoundError: No module named 'pkg_resources'`. Phải pin `<81`.
3. **Thiếu `protobuf`** → khi build có bật `CONFIG_ZMK_STUDIO=y`, generate .proto fail với
   `ModuleNotFoundError: No module named 'google'`. Thông báo lỗi của nanopb có hint
   `pip install protobuf grpcio-tools` — cứ làm theo.

## Bước 3 — Tải + giải nén Zephyr SDK

```bash
cd e:/project/keyboard/zmk-build-tools

# Tải bản monolithic (~1GB nén) — dùng đúng URL này, tên file cũ sẽ ra "Not Found" 19 byte
curl -L -o zephyr-sdk-0.16.3_windows-x86_64.7z \
  https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.3/zephyr-sdk-0.16.3_windows-x86_64.7z

# Giải nén bằng 7-Zip (ra ~7.4GB)
"/c/Program Files/7-Zip/7z.exe" x -y zephyr-sdk-0.16.3_windows-x86_64.7z

# Verify ARM GCC
./zephyr-sdk-0.16.3/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc.exe --version
# → arm-zephyr-eabi-gcc (Zephyr SDK 0.16.3) 12.2.0

# Xóa file nén cho nhẹ máy (tùy)
rm zephyr-sdk-0.16.3_windows-x86_64.7z
```

## Bước 4 — Cài dtc (Device Tree Compiler)

**Bản SDK Windows KHÔNG có `dtc.exe`** (chỉ Linux có hosttools) → build sẽ fail ở
devicetree. Các nguồn sai lưng: pip package `dtc` (Python binding rỗng), repo lbmeng/dtc
(source 12 năm tuổi không có release).

**Cách đúng: MSYS2.** Nếu chưa có MSYS2, cài từ https://www.msys2.org (mặc định `C:\msys64`).

```bash
/c/msys64/usr/bin/pacman.exe -Sy --noconfirm          # refresh DB lần đầu
/c/msys64/usr/bin/pacman.exe -S --noconfirm mingw-w64-x86_64-dtc

# Verify
/c/msys64/mingw64/bin/dtc.exe --version   # → Version: DTC 1.7.2
```

⚠️ Tên package là `mingw-w64-x86_64-dtc` — KHÔNG phải `...-device-tree-compiler` (không tồn tại).

## Bước 5 — west init + update (tải source ZMK)

```bash
cd /e/project/keyboard/Sofle-oled_wireless

# ⚠️ init trỏ vào thư mục config/ (nơi chứa west.yml), KHÔNG phải gốc repo
export PATH="/e/project/keyboard/zmk-build-tools/venv/Scripts:$PATH"
west init -l config

# Tải source. Repo này đã có name-blocklist 29 module rác (segger ~300MB,
# trusted-firmware-a, espressif 1.5GB, các hal hãng khác...) — đừng xóa blocklist
# khỏi config/west.yml nếu không muốn tải thừa ~4GB
west update
```

Lần đầu tải ~2.5GB (zephyr 517M + lvgl 407M + picolibc 140M + hal/nordic...). Mạng VN về
GitHub dao động 30KB/s - 1.7MB/s, có thể mất 15-60 phút.

### Nếu west update bị gián đoạn giữa chừng

Chạy lại `west update` — nó idempotent, tự tải bù phần thiếu.
**Triệu chứng hỏng:** build fail ở Kconfig với warning `HAS_NRFX && 0` bị escalade thành
error → nghĩa là thiếu `modules/hal/nordic` → chạy lại west update là hết.

## Bước 6 — Build

Đầy đủ biến môi trường mỗi phiên Git Bash mới:

```bash
cd /e/project/keyboard/Sofle-oled_wireless
export PATH="/e/project/keyboard/zmk-build-tools/venv/Scripts:/c/msys64/mingw64/bin:$PATH"
export ZEPHYR_SDK_INSTALL_DIR=/e/project/keyboard/zmk-build-tools/zephyr-sdk-0.16.3
```

Build từng firmware (mirror đúng `build.yaml` của repo):

```bash
# Nửa trái (central) — có ZMK Studio + USB RPC + OLED 128x64 SH1106
# ⚠️ -DEXTRA_DTC_OVERLAY_FILE là BẮT BUỘC cho OLED trái (panel SH1106 1.3").
# Không có flag này → firmware quay về driver 128x32 SSD1306 → màn nhiễu.
rm -rf build/sofle_left
west build -s zmk/app -b nice_nano_v2 -d build/sofle_left -- \
  -DZMK_CONFIG="E:/project/keyboard/Sofle-oled_wireless/config" \
  -DSHIELD=sofle_left -DCONFIG_ZMK_STUDIO=y -S studio-rpc-usb-uart \
  -DEXTRA_DTC_OVERLAY_FILE="E:/project/keyboard/Sofle-oled_wireless/config/oled_128x64_left.overlay"

# Nửa phải (peripheral)
rm -rf build/sofle_right
west build -s zmk/app -b nice_nano_v2 -d build/sofle_right -- \
  -DZMK_CONFIG="E:/project/keyboard/Sofle-oled_wireless/config" \
  -DSHIELD=sofle_right

# Settings reset (dùng khi cần xóa bond BLE)
rm -rf build/settings_reset
west build -s zmk/app -b nice_nano_v2 -d build/settings_reset -- \
  -DZMK_CONFIG="E:/project/keyboard/Sofle-oled_wireless/config" \
  -DSHIELD=settings_reset
```

Kết quả mong đợi (so với bản đã verify 16/08/2026):

```
sofle_left:         FLASH 41.07%  RAM 27.93%   → build/sofle_left/zephyr/zmk.uf2   (~651KB)
sofle_right:                                          build/sofle_right/zephyr/zmk.uf2  (~536KB)
settings_reset:    FLASH  5.69%  RAM  4.41%   → build/settings_reset/zephyr/zmk.uf2 (~91KB)
```

Lần build đầu ~5-10 phút/firmware; từ lần sau (đã có cache) ~1-3 phút.

Mọi lần sửa `config/sofle.keymap` hay `config/sofle.conf` chỉ cần chạy lại lệnh build
tương ứng — **giữ nguyên build dir, đừng rm** nếu muốn tận dụng cache (chỉ rm khi đổi
hoàn toàn config/snippet).

### Hoặc dùng script có sẵn trong repo

```bash
bash scripts/build-zmk.sh all      # cả 3 firmware
bash scripts/build-zmk.sh left     # chỉ nửa trái
bash scripts/build-zmk.sh right    # chỉ nửa phải
bash scripts/build-zmk.sh reset    # chỉ settings reset
```

Script tự dò path gốc (repo và `zmk-build-tools` phải nằm **cạnh nhau** trong cùng folder cha,
như cấu trúc ở Bước 1) và tự set PATH + `ZEPHYR_SDK_INSTALL_DIR`. Nếu đặt chỗ khác, sửa các
biến path ở đầu file `scripts/build-zmk.sh`. Script cũng kiểm tra toolchain còn đủ không và
chỉ đường ngược lại đúng bước docs này nếu thiếu.

## Bước 7 — Nạp firmware

1. Cắm USB vào nửa trái, nhấn nút **reset 2 lần nhanh** → ổ đĩa UF2 hiện lên
   (kiểm tra `INFO_UF2.TXT` ghi `Model: nice!nano`).
2. Kéo thả `build/sofle_left/zephyr/zmk.uf2` vào ổ → tự nạp, ổ tự eject.
3. Lặp lại với nửa phải + `sofle_right/zephyr/zmk.uf2`.
4. Reset cả 2 nửa gần như cùng lúc (rút/cắm điện cả 2) → chúng tự pair BLE với nhau.
5. Pair bàn phím với máy tính như thiết bị Bluetooth thường.

### ⚠️ KHÔNG bao giờ

- Nạp `CURRENT.UF2` (file backup bootloader tự sinh) của nửa này sang nửa kia —
  2 nửa cần 2 firmware khác nhau (central vs peripheral).
- Cắm/rút cáp TRRS khi keyboards đang通电.

### Nếu 2 nửa không pair được nhau (bond cũ xung đột)

1. Nạp `settings_reset/zephyr/zmk.uf2` cho nửa trái → **ngay lập tức** vào bootloader lại
   (double-reset, tránh nó kịp bond tiếp).
2. Làm tương tự nửa phải.
3. Nạp lại firmware chính cho cả 2 → reset cùng lúc.

### Cách màn hình custom hoạt động (QUAN TRỌNG nếu sửa)

- Code custom nằm ở `config/status_screen_left.c` (màn trái: S/C/A/G modifiers,
  HID indicators, pin, output, layer) và `config/status_screen_right.c`
  (màn phải: bongo cat, pin, WiFi kết nối).
- **ZMK KHÔNG tự compile code từ folder config/** — file `config/zephyr/module.yml`
  biến folder này thành Zephyr module, và build script truyền
  `-DZMK_EXTRA_MODULES=config/` để code được nhúng. Nếu build mà màn hìnhindi
  không đổi theo code → thiếu flag này (triệu chứng: OLED nhiễu garbage).
- Mỗi file `.c` trong `config/widgets/` phải `#include <lvgl.h>` **trước** mọi
  header ZMK — macro `E` (keycode phím E) trong `keys.h` sẽ phá đường dẫn
  `E:/...` trên Windows khi LVGL include config.
- Đổi hiển thị của 2 nửa: sửa `config/sofle_left.conf` / `config/sofle_right.conf`
  (mỗi nửa load file riêng của nó + `sofle.conf` chung).

## Đã thay OLED trái 128x32 → 128x64 (1.3 inch) — SH1106

> Ghi chú verify 22/08/2026. Nửa **trái** giờ dùng panel 1.3" 128x64;
> nửa **phải** vẫn OLED 0.96" 128x32 gốc (2 nửa KHÔNG interchange được firmware OLED config).

### Chuỗi vấn đề đã té (đọc để không lặp lại)

1. **Panel 1.3 inch 128x64 ≈ luôn là driver SH1106**, không phải SSD1306 như panel 0.96".
   Hai chip gần giống nhau trừ phần *addressing*: SSD1306 có lệnh `Set Column/Page Address
   (0x21/0x22)`, SH1106 **không có** → firmware ssd1306fb ghi dữ liệu lệch vùng RAM.
   Triệu chứng: màn nhiễu trắng đen loạn xạ + vài chữ vẫn lóe được + nhấp nháy theo layer
   (MCU chạy bình thường, bàn phím vẫn gõ được). KHÔNG phải lỗi phần cứng/solder.
2. Giữ nguyên config shield 128x32 cũng sai tiếp: `height=32`, `multiplex-ratio=31`,
   `com-sequential` đều là tham số của panel 32 dòng — panel 64 dòng cần `height=64`,
   `multiplex-ratio=63`, và KHÔNG dùng `com-sequential` (COM pins Alternative).

### Cách đã fix (cấu trúc hiện tại)

- File `config/oled_128x64_left.overlay` override node `&oled`:
  đổi `compatible = "sinowealth,sh1106"` (driver Zephyr có sẵn, giao thức page-based đúng),
  `height = <64>`, `multiplex-ratio = <63>`, `/delete-property/ com-sequential`.
- **Bẫy đặt tên file**: KHÔNG được đặt tên `sofle_left.overlay` / `sofle_left_nice_nano_v2.overlay`
  trong `config/` — build system ZMK tự nạp các file theo tên đó **trước** shield dtsi,
  tại thời điểm đó label `&oled` chưa tồn tại → lỗi devicetree
  `parse error: undefined node label 'oled'`.
- Vì vậy `scripts/build-zmk.sh` (target `left` và `all`) truyền thêm
  `-DEXTRA_DTC_OVERLAY_FILE=$ZMK_CONFIG/oled_128x64_left.overlay` → Zephyr áp file này
  **cuối cùng** (sau board dts + shield + keymap), khi `&oled` đã định nghĩa.
  Target `right`/`reset` KHÔNG truyền — nửa phải giữ OLED 128x32 gốc.

### Nếu sau này đổi OLED nửa phải sang 128x64

Copy `oled_128x64_left.overlay` thành `oled_128x64_right.overlay` và truyền
`-DEXTRA_DTC_OVERLAY_FILE` tương ứng trong case `right` của `scripts/build-zmk.sh`.
Lưu ý widget bongo cat của nửa phải đang vẽ layout cho 32px — cần chỉnh lại
`status_screen_right.c` cho 64px.

### Dùng màn OLED gốc 128x32 mặc định (nếu bạn KHÔNG thay panel như trên)

Mặc định repo này đang build cho OLED trái 1.3" 128x64 (biến thể của chủ repo).
Nếu bàn của bạn còn màn gốc 0.96" 128x32, edit 3 chỗ sau rồi build lại như bình thường:

1. **`build.yaml`** (build GitHub Actions): ở mục `sofle_left`, xóa phần
   `-DEXTRA_DTC_OVERLAY_FILE=config/oled_128x64_left.overlay` khỏi `cmake-args`
   (giữ lại `-DCONFIG_ZMK_STUDIO=y`).
2. **`scripts/build-zmk.sh`** (build local): xóa dòng
   `"-DEXTRA_DTC_OVERLAY_FILE=$ZMK_CONFIG_DIR/oled_128x64_left.overlay"` ở cả 2 case `left` và `all`.
3. Nửa phải không cần đụng gì — vẫn 128x32 mặc định.

Không cần xóa file `config/oled_128x64_left.overlay` (flag không truyền thì file bị bỏ qua,
không tự nạp). Không đụng `zmk/`, `sofle.conf` hay bất kỳ file nào khác.

### Nếu màn SH1106 vẫn lệch ~2 pixel ngang — ĐÃ TÉ, ĐÃ FIX (verify 23/08/2026)

**SH1106 có GRAM 132 cột nhưng panel chỉ hiển thị 128.** Panel 1.3" của repo này map
window GRAM[2..129], trong khi driver mặc định ghi cols 0..127:

- 2 cột cuối (GRAM[128..129]) **không bao giờ được ghi** → giữ garbage khởi động
- Triệu chứng khi xoay 90° CW: hiện thành **2 dòng nhiễu trắng đen nằm ngoài khung UI,
  ở mép cạnh 64px của màn** (2 cột GRAM bị lộ). Nội dung render của mình thì sạch.
- **Fix: `segment-offset = <2>;`** trong `oled_128x64_left.overlay` — dịch vùng ghi ra
  GRAM[2..129] khớp window panel. Nhầm lẫn phải tránh: `display-offset` (trục COM/dọc)
  KHÔNG tác dụng với lỗi này — noise nằm trục cột thì chỉ `segment-offset` ăn.

### Cách debug màn hình bằng pattern diagnostic (target `diag`)

Khi màn có artifact mà không rõ lỗi nằm ở tầng nào (LVGL render / transpose / driver):

```bash
bash scripts/build-zmk.sh diag
```

Bản này **bỏ qua toàn bộ LVGL render** — vẽ thẳng vào buffer vật lý trong flush callback:
nền trắng + 4 ô vuông đen 8x8 ở 4 góc vật lý (không widget, không text). Flash
`build/sofle_diag/zephyr/zmk.uf2` rồi đối chiếu:

| Quan sát | Kết luận |
|---|---|
| 4 ô vuông sạch, artifact vẫn ở ngoài khung | lỗi **map GRAM panel** → chỉnh `segment-offset`/`display-offset` trong overlay |
| 4 ô vuông sạch, artifact biến mất | lỗi ở **LVGL render/widget** (không phải display path) |
| Ô vuông méo/sai vị trí | lỗi **math transpose** trong `display_rotation.c` |

Công cụ này giữ lại trong repo: Kconfig `CONFIG_ZMK_DISPLAY_ROTATE_DIAG` (default n,
enable bởi target `diag` của build script). Khỏi cần viết lại khi gặp lỗi hiển thị sau này.

## Xoay màn OLED trái 90° (top → cạnh phải) — cấu trúc hiện tại

Panel trái được **gắn xoay dọc**: UI logic là màn dọc **64×128**, vật lý vẫn 128×64.

- **`config/display_rotation.c`**: sau khi LVGL init, đổi `hor_res=64/ver_res=128`,
  thay `flush_cb` bằng hàm transpose tự viết (mapping `px=127−ly, py=lx`), buffer
  full-frame 1KB persistent — chỉ ghi I2C 1 lần/frame. Gọi từ
  `zmk_display_status_screen()` TRƯỚC khi tạo widget.
- **Bật/tắt**: `CONFIG_ZMK_DISPLAY_ROTATE_90_RIGHT` trong `sofle_left.conf`
  (đang bật). Tắt = layout ngang 128×64 cũ trong `#else` của `status_screen_left.c`.
- **Bắt buộc đi kèm khi xoay** (đã té, đừng lặp lại):
  1. `CONFIG_LV_Z_VDB_SIZE=100` — VDB nhỏ hơn chia frame thành nhiều chunk,
     vùng nối chunk chứa byte stale từ chunk trước → artifact vệt chấm sau transpose.
  2. `CONFIG_LV_USE_FLEX=y` — layout dọc dùng flex column.
  3. `CONFIG_ZMK_LV_FONT_DEFAULT_SMALL_MONTSERRAT_8=y` — font nhỏ cho màn 64px
     (Montserrat 8 vẫn đủ glyph FontAwesome: USB/WIFI/BATTERY — đã verify).
  4. Tắt scrollbar/border screen trong `status_screen_left.c` (đã có trong code).
- SH1106/SSD1306 **không thể** xoay 90° bằng lệnh hardware (chỉ có lật 0°/180° qua
  `segment-remap`/`com-invdir`); LVGL `sw_rotate` cũng không dùng được cho màn mono
  `set_px_cb` — nên mới phải transpose software như trên.

## Sửa lỗi nhanh (troubleshooting)

| Triệu chứng | Nguyên nhân | Cách fix |
|---|---|---|
| `CMake Error: ... requires CMake >=3.20 <4` | dùng nhầm CMake 4.x của máy | kiểm tra `which cmake` — PATH phải có venv/Scripts đứng TRƯỚC |
| `ModuleNotFoundError: pkg_resources` | setuptools ≥81 | `pip install --force-reinstall "setuptools<81"` |
| `ModuleNotFoundError: google` | thiếu protobuf | `pip install protobuf grpcio-tools` |
| Kconfig abort `HAS_NRFX && 0` | `west update` dở (thiếu hal_nordic) | chạy lại `west update` |
| `dtc: command not found` / devicetree fail | thiếu dtc | Bước 4 — pacman MSYS2 |
| `west init` báo `no west.yml found` | trỏ sai thư mục | `west init -l config` (từ gốc repo) |
| Build sai tên board/chưa có UF2 | thiếu `-DZMK_CONFIG` | copy nguyên lệnh ở Bước 6 |
| OLED trái nhiễu trắng đen loạn xạ, vài chữ lóe được, bàn phím vẫn gõ OK | panel 1.3" là **SH1106** nhưng firmware chạy driver ssd1306fb | giữ overlay `oled_128x64_left.overlay` (`compatible = "sinowealth,sh1106"`) + build qua `scripts/build-zmk.sh left` |
| Devicetree error `undefined node label 'oled'` | file overlay đặt tên `{shield}.overlay` trong `config/` → bị nạp trước shield | đổi tên file (vd `oled_128x64_left.overlay`) + truyền `-DEXTRA_DTC_OVERLAY_FILE` |
| OLED 128x64 hiện nội dung nhưng bị nén/dúp ở nửa trên | thiếu `height=64`/`multiplex-ratio=63` hoặc dư `com-sequential` | kiểm tra overlay đủ 3 thay đổi như mục "Đã thay OLED trái" |
| 1-2 dòng/cột nhiễu nằm NGOÀI khung UI trên cạnh màn (nội dung vẫn sạch) | panel SH1106 map window GRAM lệch — vùng không được ghi giữ garbage | `segment-offset = <2>;` trong `oled_128x64_left.overlay` (mục "lệch ~2 pixel"); xác nhận bằng `build-zmk.sh diag` |
| Vệt chấm rác theo băng 8px SAU khi bật xoay 90° | VDB < 100% chia frame thành chunk, vùng nối chứa byte stale | `CONFIG_LV_Z_VDB_SIZE=100` trong `sofle_left.conf` |

## Đổi keymap không cần build lại

Firmware trái đã bật **ZMK Studio**: cắm USB nửa trái → mở https://zmk.studio bằng
Chrome/Edge → chọn USB → sửa keymap trực tiếp trên firmware đang chạy (repo đặt
`CONFIG_ZMK_STUDIO_LOCKING=n` nên không cần phím unlock). Chỉ cần build lại khi đổi
`.conf` або cấu trúc keymap file.
