#!/bin/bash
# install-libreoffice.sh - VNLF App Store
echo "[VNLF] Cài đặt LibreOffice..."
if command -v apt &>/dev/null; then sudo apt update && sudo apt install -y libreoffice
elif command -v dnf &>/dev/null; then sudo dnf install -y libreoffice
elif command -v pacman &>/dev/null; then sudo pacman -S --noconfirm libreoffice
elif command -v zypper &>/dev/null; then sudo zypper install -y libreoffice
elif command -v flatpak &>/dev/null; then flatpak install -y flathub libreoffice
else echo "[VNLF] Không tìm thấy package manager!"; exit 1; fi
[ $? -eq 0 ] && echo "[VNLF] Cài đặt LibreOffice thành công!" || { echo "[VNLF] Cài đặt thất bại!"; exit 1; }
