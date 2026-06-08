#!/bin/bash
# Script cài đặt tổng quát - VNLF App Store
# Sử dụng: ./install-generic.sh <tên-gói>

PACKAGE="${1:-$APP_ID}"
APP_NAME="${2:-$PACKAGE}"

echo "[VNLF] Bắt đầu cài đặt $APP_NAME ($PACKAGE)..."

if command -v apt &>/dev/null; then
    sudo apt update && sudo apt install -y "$PACKAGE"
elif command -v dnf &>/dev/null; then
    sudo dnf install -y "$PACKAGE"
elif command -v pacman &>/dev/null; then
    sudo pacman -S --noconfirm "$PACKAGE"
elif command -v zypper &>/dev/null; then
    sudo zypper install -y "$PACKAGE"
elif command -v flatpak &>/dev/null; then
    flatpak install -y flathub "$PACKAGE"
else
    echo "[VNLF] Không tìm thấy package manager!"
    exit 1
fi

if [ $? -eq 0 ]; then
    echo "[VNLF] Cài đặt $APP_NAME thành công!"
else
    echo "[VNLF] Cài đặt $APP_NAME thất bại!"
    exit 1
fi
