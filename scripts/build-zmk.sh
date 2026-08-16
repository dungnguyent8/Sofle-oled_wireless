#!/usr/bin/env bash
# ============================================================
# build-zmk.sh — Build firmware ZMK local cho Sofle (nice_nano_v2)
#
# Yeu cau da lam theo docs/LOCAL_BUILD_SETUP.md:
#   - venv tai   <keyboard-folder>/zmk-build-tools/venv
#   - SDK tai    <keyboard-folder>/zmk-build-tools/zephyr-sdk-0.16.3
#   - dtc qua MSYS2 tai C:\msys64\mingw64\bin
#
# Cach dung:  bash scripts/build-zmk.sh [left|right|reset|all]
# Mac dinh:   all (ca 3 firmware)
# ============================================================
set -e

# --- Duong dan cong cu — sua 4 dong nay neu dat folder khac ---
REPO="$(cd "$(dirname "$0")/.." && pwd)"                          # goc repo nay
KEYBOARD_ROOT="$(cd "$REPO/.." && pwd)"                           # folder cha chua ca repo + zmk-build-tools
VENV_BIN="$KEYBOARD_ROOT/zmk-build-tools/venv/Scripts"
SDK_ROOT="$KEYBOARD_ROOT/zmk-build-tools/zephyr-sdk-0.16.3"
DTC_BIN="/c/msys64/mingw64/bin"

export PATH="$VENV_BIN:$DTC_BIN:$PATH"
export ZEPHYR_SDK_INSTALL_DIR="$SDK_ROOT"

cd "$REPO"

# Kiem tra toolchain ton tai
if [ ! -f "$VENV_BIN/west.exe" ]; then
  echo "LOI: Khong thay west trong $VENV_BIN"
  echo "  -> Lam theo docs/LOCAL_BUILD_SETUP.md buoc 2 (tao venv + pip install)"
  exit 1
fi
if [ ! -d "$SDK_ROOT/arm-zephyr-eabi" ]; then
  echo "LOI: Khong thay SDK tai $SDK_ROOT"
  echo "  -> Lam theo docs/LOCAL_BUILD_SETUP.md buoc 3 (tai + giai nen Zephyr SDK)"
  exit 1
fi
if [ ! -f "$DTC_BIN/dtc.exe" ]; then
  echo "LOI: Khong thay dtc.exe tai $DTC_BIN"
  echo "  -> Lam theo docs/LOCAL_BUILD_SETUP.md buoc 4 (pacman -S mingw-w64-x86_64-dtc)"
  exit 1
fi

ZMK_CONFIG_DIR="$REPO/config"

TARGET="${1:-all}"

build () {
  local name="$1"; shift
  # Tach rieng snippet (-S phai dung TRUOC dau -- de west hieu, chu khong phai cmake)
  local snippet=""
  local cmake_args=()
  for arg in "$@"; do
    case "$arg" in
      -S:*) snippet="${arg#-S:}" ;;
      *) cmake_args+=("$arg") ;;
    esac
  done
  echo ""
  echo "=============================================="
  echo "  BUILD: $name (snippet: ${snippet:-none})"
  echo "=============================================="
  rm -rf "build/$name"
  local sflag=()
  [ -n "$snippet" ] && sflag=(-S "$snippet")
  "$VENV_BIN/west.exe" build -s zmk/app -b nice_nano_v2 -d "build/$name" "${sflag[@]}" -- \
    "-DZMK_CONFIG=$ZMK_CONFIG_DIR" "${cmake_args[@]}"
}

case "$TARGET" in
  left)
    build sofle_left -S:studio-rpc-usb-uart -DSHIELD=sofle_left -DCONFIG_ZMK_STUDIO=y \
      "-DZMK_EXTRA_MODULES=$ZMK_CONFIG_DIR"
    ;;
  right)
    build sofle_right -DSHIELD=sofle_right "-DZMK_EXTRA_MODULES=$ZMK_CONFIG_DIR"
    ;;
  reset)
    build settings_reset -DSHIELD=settings_reset
    ;;
  all|*)
    build sofle_left -S:studio-rpc-usb-uart -DSHIELD=sofle_left -DCONFIG_ZMK_STUDIO=y \
      "-DZMK_EXTRA_MODULES=$ZMK_CONFIG_DIR"
    build sofle_right -DSHIELD=sofle_right "-DZMK_EXTRA_MODULES=$ZMK_CONFIG_DIR"
    build settings_reset -DSHIELD=settings_reset
    ;;
esac

echo ""
echo "=============================================="
echo "  XONG! File UF2:"
echo "=============================================="
find "$REPO/build" -name "*.uf2" -exec ls -lh {} \;
