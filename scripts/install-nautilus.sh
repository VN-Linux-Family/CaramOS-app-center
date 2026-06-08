#!/bin/bash
# install-nautilus.sh - VNLF App Store
echo "[VNLF] Cài đặt Files (Nautilus)..."
if command -v apt &>/dev/null; then sudo apt update && sudo apt install -y nautilus
elif command -v dnf &>/dev/null; then sudo dnf install -y nautilus
elif command -v pacman &>/dev/null; then sudo pacman -S --noconfirm nautilus
elif command -v zypper &>/dev/null; then sudo zypper install -y nautilus
elif command -v flatpak &>/dev/null; then flatpak install -y flathub nautilus
else echo "[VNLF] Không tìm thấy package manager!"; exit 1; fi
[ $? -eq 0 ] && echo "[VNLF] Cài đặt Files (Nautilus) thành công!" || { echo "[VNLF] Cài đặt thất bại!"; exit 1; }
