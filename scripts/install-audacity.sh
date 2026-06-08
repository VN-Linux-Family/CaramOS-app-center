#!/bin/bash
# install-audacity.sh - VNLF App Store
echo "[VNLF] Cài đặt Audacity..."
if command -v apt &>/dev/null; then sudo apt update && sudo apt install -y audacity
elif command -v dnf &>/dev/null; then sudo dnf install -y audacity
elif command -v pacman &>/dev/null; then sudo pacman -S --noconfirm audacity
elif command -v zypper &>/dev/null; then sudo zypper install -y audacity
elif command -v flatpak &>/dev/null; then flatpak install -y flathub audacity
else echo "[VNLF] Không tìm thấy package manager!"; exit 1; fi
[ $? -eq 0 ] && echo "[VNLF] Cài đặt Audacity thành công!" || { echo "[VNLF] Cài đặt thất bại!"; exit 1; }
