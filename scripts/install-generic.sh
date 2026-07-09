#!/bin/bash
# VNLF App Store - Generic install script
# Usage: install-generic.sh <package-name> [display-name]
PACKAGE="${1:-}"
NAME="${2:-$PACKAGE}"
if [ -z "$PACKAGE" ]; then echo "[VNLF] Thiếu tên gói!"; exit 1; fi
echo "[VNLF] Cài đặt: $NAME ($PACKAGE)"
if command -v apt &>/dev/null; then
    apt update && apt install -y "$PACKAGE"
elif command -v dnf &>/dev/null; then
    dnf install -y "$PACKAGE"
elif command -v pacman &>/dev/null; then
    pacman -S --noconfirm "$PACKAGE"
elif command -v zypper &>/dev/null; then
    zypper install -y "$PACKAGE"
else
    echo "[VNLF] Không tìm thấy package manager!"; exit 1
fi
[ $? -eq 0 ] && echo "[VNLF] Cài đặt $NAME thành công!" || { echo "[VNLF] Cài đặt thất bại!"; exit 1; }
