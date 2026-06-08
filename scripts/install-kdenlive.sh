#!/bin/bash
# install-kdenlive.sh - VNLF App Store
echo "[VNLF] Cài đặt Kdenlive..."
if command -v apt &>/dev/null; then sudo apt update && sudo apt install -y kdenlive
elif command -v dnf &>/dev/null; then sudo dnf install -y kdenlive
elif command -v pacman &>/dev/null; then sudo pacman -S --noconfirm kdenlive
elif command -v zypper &>/dev/null; then sudo zypper install -y kdenlive
elif command -v flatpak &>/dev/null; then flatpak install -y flathub kdenlive
else echo "[VNLF] Không tìm thấy package manager!"; exit 1; fi
[ $? -eq 0 ] && echo "[VNLF] Cài đặt Kdenlive thành công!" || { echo "[VNLF] Cài đặt thất bại!"; exit 1; }
