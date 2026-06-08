#!/bin/bash
# install-thunderbird.sh - VNLF App Store
echo "[VNLF] Cài đặt Thunderbird..."
if command -v apt &>/dev/null; then sudo apt update && sudo apt install -y thunderbird
elif command -v dnf &>/dev/null; then sudo dnf install -y thunderbird
elif command -v pacman &>/dev/null; then sudo pacman -S --noconfirm thunderbird
elif command -v zypper &>/dev/null; then sudo zypper install -y thunderbird
elif command -v flatpak &>/dev/null; then flatpak install -y flathub thunderbird
else echo "[VNLF] Không tìm thấy package manager!"; exit 1; fi
[ $? -eq 0 ] && echo "[VNLF] Cài đặt Thunderbird thành công!" || { echo "[VNLF] Cài đặt thất bại!"; exit 1; }
