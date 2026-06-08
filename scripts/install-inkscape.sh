#!/bin/bash
# install-inkscape.sh - VNLF App Store
echo "[VNLF] Cài đặt Inkscape..."
if command -v apt &>/dev/null; then sudo apt update && sudo apt install -y inkscape
elif command -v dnf &>/dev/null; then sudo dnf install -y inkscape
elif command -v pacman &>/dev/null; then sudo pacman -S --noconfirm inkscape
elif command -v zypper &>/dev/null; then sudo zypper install -y inkscape
elif command -v flatpak &>/dev/null; then flatpak install -y flathub inkscape
else echo "[VNLF] Không tìm thấy package manager!"; exit 1; fi
[ $? -eq 0 ] && echo "[VNLF] Cài đặt Inkscape thành công!" || { echo "[VNLF] Cài đặt thất bại!"; exit 1; }
