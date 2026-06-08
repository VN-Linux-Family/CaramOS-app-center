#!/bin/bash
# Script cài đặt Firefox
# VNLF App Store - install-firefox.sh

APP_NAME="Firefox"

echo "[VNLF] Bắt đầu cài đặt $APP_NAME..."

# Kiểm tra package manager
if command -v apt &>/dev/null; then
    sudo apt update && sudo apt install -y firefox
elif command -v dnf &>/dev/null; then
    sudo dnf install -y firefox
elif command -v pacman &>/dev/null; then
    sudo pacman -S --noconfirm firefox
elif command -v zypper &>/dev/null; then
    sudo zypper install -y firefox
else
    echo "[VNLF] Không tìm thấy package manager phù hợp!"
    exit 1
fi

if [ $? -eq 0 ]; then
    echo "[VNLF] Cài đặt $APP_NAME thành công!"
else
    echo "[VNLF] Cài đặt $APP_NAME thất bại!"
    exit 1
fi
