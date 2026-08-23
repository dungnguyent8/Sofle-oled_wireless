# Hướng dẫn build firmware ZMK local trên Windows (không cần GitHub Actions)

> Viết cho repo **Sofle-oled_wireless** — bàn phím PandaKB Sofle RGB MX v1 + ProMicro nRF52840
> (clone nice!nano v2, bootloader UF2). Áp dụng chung cho mọi shield ZMK build board `nice_nano_v2`.
>
> Làm theo đúng từng bước — đã verify thành công trên Windows 10/11 + Git Bash.
> Tổng thời gian ~30-45 phút (tùy tốc độ mạng khi tải SDK + source).

## ⚠️ Repo này KHÔNG tự chứa mọi thứ cần để build — clone mới phải đọc trước

Repo chỉ chứa **config + code hiển thị** (~vài MB). Mọi thứ dưới đây phải tự chuẩn bị
(không commit được vì quá lớn hoặc thuộc máy cụ thể):

| Thứ | Vì sao không có trong repo | Lấy ở đâu |
|---|---|---|
| **Source ZMK/Zephyr/modules** (~2.5GB) | west kéo về; `.gitignore` chặn `/zephyr/`, `/modules/`... | Bước 5 (`west init -l config && west update`) |
| **Patch 9 file trong cây ZMK** ⚠️ QUAN TRỌNG | Patch sửa thẳng source trong `zmk/` (curve pin LiPo, mV...) — mất khi `west update` | ✅ Đã backup: `patches/0001-*.patch` TRONG repo — xem "Khôi phục patch" dưới |
| **Toolchain** (venv Python + Zephyr SDK 0.16.3 + dtc) | ~3GB, cài ngoài repo tại `E:\project\keyboard\zmk-build-tools\` | Bước 2-4 |
| **Thư mục `zmk/` gitlink rỗng** | Repo track `zmk` dạng SHA pointer (mode 160000) — clone về là thư mục RỖNG, không phải submodule thật | Bỏ qua, west update tạo đúng; hoặc `git -C zmk checkout c8157f01` nếu đã có source |

### Khôi phục patch ZMK sau `west update` (BẮT BUỘC nếu không curve pin sẽ sai)

`west update` reset source `zmk/` về bản manifest gốc → mất 9 file patch (curve LiPo, mV threshold,
event millivolts...). Repo đã backup dạng git-am series:

```bash
# Cách 1 — checkout đúng commit đã patch (nhanh, an toàn):
git -C zmk checkout local/sofle-patches
# (nếu branch mất: git -C zmk checkout c8157f01)

# Cách 2 — apply patch lên bản ZMK mới hơn (khi nâng version, có thể conflict):
git -C zmk am ../patches/0001-*.patch
```

Nếu build mà % pin hiển thị "nhảy cấp" lớn giữa 3.7-3.9V hoặc widget trái báo mV lạ —
99% là chưa khôi phục patch.

### Hardware riêng của bản mod này (khác stock Sofle)

- **OLED trái 1.3" 128x64 SH1106** (stock là 0.96" 128x32 SSD1306) — xoay dọc 90°
- **OLED phải giữ stock** 128x32 SSD1306
- Pin LiPo 1S mỗi nửa (đọc qua VDDH/5) — curve 14 điểm trong patch + widget

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

# ⚠️ west update XÓA patch 9 file trong zmk/ (curve pin, mV...) —
# chạy thêm lệnh này SAU MỖI lần west update:
git -C zmk checkout local/sofle-patches   # hoặc: git -C zmk am ../patches/0001-*.patch
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
- Cắm/rút cáp TRRS khi bàn phím đang có điện — luôn ngắt nguồn (tắt switch
  pin hoặc rút cáp) trước khi cắm/rút TRRS.

### Nếu 2 nửa không pair được nhau (bond cũ xung đột)

1. Nạp `settings_reset/zephyr/zmk.uf2` cho nửa trái → **ngay lập tức** vào bootloader lại
   (double-reset, tránh nó kịp bond tiếp).
2. Làm tương tự nửa phải.
3. Nạp lại firmware chính cho cả 2 → reset cùng lúc.

### Cách màn hình custom hoạt động (QUAN TRỌNG nếu sửa)

- Code custom nằm ở `config/status_screen_left.c` (màn trái: S/C/A/G modifiers,
  HID indicators, pin + mV, bongo cat, trang info ADJUST — xem mục riêng) và
  `config/status_screen_right.c` (màn phải: bongo cat, pin, WiFi kết nối).
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
  3. Font hiện tại **Montserrat 12** (đã thử 8 — quá nhỏ, revert 23/08;
     Montserrat vẫn đủ glyph FontAwesome: USB/WIFI/BATTERY — verify bằng
     `scripts/list_font_symbols.py`).
  4. Tắt scrollbar/border screen trong `status_screen_left.c` (đã có trong code).
- SH1106/SSD1306 **không thể** xoay 90° bằng lệnh hardware (chỉ có lật 0°/180° qua
  `segment-remap`/`com-invdir`); LVGL `sw_rotate` cũng không dùng được cho màn mono
  `set_px_cb` — nên mới phải transpose software như trên.

## Trang info layer ADJUST (giữ raise+lower) — màn trái

> Thêm 23/08/2026 (sửa cuối 23/08 cùng ngày). Khi layer ADJUST active, màn trái
> đổi thành trang **thông số kết nối/pin**; nhả phím thì về layout thường.
> Đã từng có CPU%/RAM% MCU — đã BỎ theo yêu cầu (số liệu MCU vô nghĩa cho user,
> và đo đúng bị chặn: `lv_mem_monitor()` không hoạt động vì Zephyr LVGL dùng
  `LV_MEM_CUSTOM=y` + heap static không expose; CPU% thì `k_thread_foreach`
  cần `CONFIG_THREAD_MONITOR`... chi tiết xem git history nếu cần lại).

### Layout 2 chế độ

**Layer thường (default/lower/raise)**: pin trái `🔋 85%` (icon mức + % theo
curve LiPo — mV thật chỉ hiện khi ADJUST) → output `▶ USB`/BT profile
(**luôn hiện mọi layer**) → modifiers `S+C+A+G` → bongo cat → CAPS icon 👁
(chỉ khi bật) → tên layer.

**Layer ADJUST**: pin trái đầu màn đổi thành 2 dòng `🔋 85%` / `3.92V`
(dòng 2 thêm ⚡ khi đang sạc), output + bongo + modifiers + CAPS ẩn hết,
thay bằng container info:

```
🔋 85%
3.92V         ← pin TRÁI: % + mV thật (2 dòng — 1 dòng tràn 64px)
🔋 72%        ← pin NỬA PHẢI: % theo curve (✗ -- khi mất kết nối split)
↻ Up 3d2h     ← uptime từ boot
adjust        ← tên layer
```

Khi adjust: bongo cat + modifiers + CAPS đều ẩn (`LV_OBJ_FLAG_HIDDEN`) — trang info thuần.

### Cách hoạt động (quan trọng nếu sửa)

- **Ẩn/hiện theo layer**: `ZMK_LISTENER` bắt `zmk_layer_state_changed`, soi
  `ev->layer == 3` (ADJUST). Toggle trong `lv_async_call` để chạy ở display thread.
  Đổi thứ tự layer trong `sofle.keymap` → phải sửa `ADJUST_LAYER_INDEX` trong
  `status_screen_left.c`.
- **Pin phải**: widget `config/widgets/peripheral_battery.c` subscribe
  `zmk_peripheral_battery_state_changed` (event chỉ được raise khi
  `CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y` — đã bật trong
  `sofle_left.conf`; xem `zmk/app/src/split/central.c`). Refresh theo
  `CONFIG_ZMK_BATTERY_REPORT_INTERVAL` (60s). Mất kết nối → listener
  `zmk_split_peripheral_status_changed` đưa về `✗ --`.
- **mV pin trái** (`widgets/battery_full.c`): đọc
  `SENSOR_CHAN_GAUGE_VOLTAGE` (⚠️ KHÔNG phải `SENSOR_CHAN_VOLTAGE` — driver
  vbatt chỉ chấp nhận kênh `GAUGE_*`, gọi sai channel thì fetch trả -ENOTSUP
  và mV luôn 0 — đã té) từ node `vbatt` (VDDH/5), lọc trung bình 5 mẫu chống
  voltage sag. mV là số đo THẬT — với pin 5000mAh tự độ, dựa mV để tự phán
  đoán mức thật thay vì % curve tuyến tính của ZMK.
- Container info dùng `LV_SIZE_CONTENT` (chiều cao theo nội dung) — không đặt
  chiều cao cứng + `lv_obj_center` kẻo dòng cuối bị layer label che (đã té).
- Uptime chỉ refresh 1s/lần **khi adjust đang hiện** (check `adjust_active`
  đầu callback) — không tốn điện khi không xem.

## Pin: curve LiPo + mV 2 nửa (23/08/2026) — mV phải ĐÃ VÔ HIỆU

> **Trạng thái hiện tại (v3, cuối 23/08)**: % pin **cả 2 nửa theo curve LiPo
> 14 điểm mới** (đã verify chain). **mV nửa phải đã BỎ** — characteristic
> riêng (UUID 0x07) không được central discovery ra sau khi flash + bond
> lại + settings reset (nguyên nhân gốc chưa xác định, nghi GATT cache /
> discovery race). Code central giữ trong `#if 0` để re-enable sau.
> mV pin **trái vẫn hoạt động** (đọc trực tiếp ADC, không qua BLE).

### Tóm tắt hành vi mới

| Vị trí | Layer thường | Layer ADJUST |
|---|---|---|
| Màn trái — pin trái | `🔋 85%` (curve LiPo) | `🔋 85%` + dòng 2 `3.92V` (ADC trực tiếp) |
| Màn trái — pin phải | ẩn | `🔋 72%` (curve LiPo, qua BAS BLE) |
| Màn phải (128x32) | BAS % curve mới | không đổi |

- % trái tính từ **curve LiPo 14 điểm** áp lên mV đã lọc trung bình 5 mẫu —
  không còn dùng % tuyến tính của ZMK core (sai ±10-15% giữa dải plateau).
- Pin phải: chỉ % qua BAS (60s/lần theo `REPORT_INTERVAL`), tính bằng
  curve mới ở driver `nrf_vddh` → `lithium_ion_mv_to_pct` (đã patch).
- **Event pin giờ raise khi % đổi HOẶC mV đổi ≥ 30mV** (patch v2 trong
  `battery.c`): trước đây chỉ raise khi % đổi → trên plateau LiPo % đứng
  yên hàng giờ. 30mV = ngưỡng trên noise ADC + voltage sag.
- **Format 2 DÒNG khi adjust** (`icon %\nmV`): 1 dòng `icon 85% 3.92V`
  = ~79px theo adv_w Montserrat 12 → **tràn màn dọc 64px/60px khả dụng**
  (đã đo bằng script parse `glyph_dsc[]` + `unicode_list_1` của font,
  23/08 — nhớ đo lại nếu đổi font).
- **Nâng cấp firmware BẮT BUỘC đồng thời 2 nửa** — struct event + GATT
  service đổi, chạy song song phiên bản cũ sẽ mất mV (không mất chức năng
  chính, mV phải chỉ hiện `--`).

### Cách hoạt động (8 file patch trong cây `zmk/`)

Đây là patch thẳng vào ZMK v0.3.0 local — **mất khi `west update` / nâng
version ZMK**, phải patch lại thủ công (xem danh sách + mô tả dưới):

1. `zmk/app/include/zmk/split/transport/types.h` — struct
   `battery_event` thêm trường `uint16_t millivolts`.
2. `zmk/app/include/zmk/events/battery_state_changed.h` — cả 2 event
   `zmk_battery_state_changed` + `zmk_peripheral_battery_state_changed`
   thêm trường `uint16_t millivolts`.
3. `zmk/app/src/battery.c` — khi fetch `STATE_OF_CHARGE` cũng đọc
   `SENSOR_CHAN_GAUGE_VOLTAGE` điền `last_millivolts`, raise event kèm mV.
4. `zmk/app/src/split/peripheral.c` — report pin gửi kèm `.millivolts`.
5. `zmk/app/include/zmk/split/bluetooth/uuid.h` — characteristic mV mới
   `ZMK_SPLIT_BT_CHAR_BATTERY_MV_UUID = ZMK_BT_SPLIT_UUID(0x00000007)`.
6. `zmk/app/src/split/bluetooth/service.c` — phía peripheral: định nghĩa
   characteristic mV (read + notify + CCC), listener `battery_state_changed`
   cập nhật giá trị + `bt_gatt_notify_uuid()` cho subscriber. ⚠️ Signature
   Zephyr 3.5: `(conn, uuid, attr, data, len)` — thiếu `attr` là compile fail.
7. `zmk/app/src/split/bluetooth/central.c` — phía central: code mV nằm
   trong `#if 0` (VÔ HIỆU từ v3 — discovery không tìm ra char). Khi bật
   lại: else-if subscribe PHẢI cùng chuỗi if/else với BAS — đóng chain
   bằng `}` rồi mở `else if` mới sẽ lỗi `unterminated #if`/compile fail.
8. `zmk/app/src/split/central.c` — handler `BATTERY_EVENT` giữ
   `peripheral_soc[]` per-source; **level=255 = bản update mV-only, giữ % cũ**
   (BAS % notify và mV notify đến độc lập).

Ngoài ra (patch giai đoạn trước, cùng loạt): `zmk/app/module/drivers/sensor/
battery/battery_common.c` — thay `lithium_ion_mv_to_pct()` bằng curve LiPo
14 điểm giống hệt widget trái → **% BAS gửi Windows + % hiển thị 2 nửa đều
theo curve mới**.

### Calibrate curve cho pin cụ thể

Bảng 14 điểm `lipo_mv[]/lipo_pct[]` nằm ở 2 chỗ (giữ đồng bộ khi sửa):
`config/widgets/battery_full.c` (màn trái) + `battery_common.c` (driver
chia sẻ). Sau 1-2 chu kỳ sạc/xả, bật layer ADJUST xem mV thật ở các mốc
% rồi chỉnh bảng — đặc biệt 2 đầu (3.45-3.6V = 0-9%, 4.1-4.2V = 92-100%).

### Troubleshooting tính năng pin

- **% phải không hiện / `✗ --` mãi**: kiểm tra bond 2 nửa (settings reset
  cả 2 + pair lại); % chạy đường BAS stock nên firmware lệch version vẫn OK.
- **% trái nhảy cấp** khi vừa gõ nặng/mới BLE TX: đã có filter 5 mẫu; nếu
  vẫn thấy giật, tăng `MV_FILTER_LEN` (5 → 8).
- **mV phải (char UUID 0x07) không discovery được** — đã té cả 3 lần, chưa
  rõ gốc (đã thử: flash đồng bộ 2 nửa, settings reset + bond lại, initial
  read sau subscribe). Code giữ trong `#if 0` ở `central.c`; nếu muốn
  điều tra lại thì bật `#if 1` + xem LOG_DBG "Found battery mV characteristics".

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
