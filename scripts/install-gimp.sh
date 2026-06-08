#!/bin/bash
# install-gimp.sh - VNLF App Store
echo "[VNLF] Cài đặt GIMP..."
if command -v apt &>/dev/null; then sudo apt update && sudo apt install -y gimp
elif command -v dnf &>/dev/null; then sudo dnf install -y gimp
elif command -v pacman &>/dev/null; then sudo pacman -S --noconfirm gimp
elif command -v zypper &>/dev/null; then sudo zypper install -y gimp
elif command -v flatpak &>/dev/null; then flatpak install -y flathub gimp
else echo "[VNLF] Không tìm thấy package manager!"; exit 1; fi
[ $? -eq 0 ] && echo "[VNLF] Cài đặt GIMP thành công!" || { echo "[VNLF] Cài đặt thất bại!"; exit 1; }
