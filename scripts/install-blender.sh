#!/bin/bash
# install-blender.sh - VNLF App Store
echo "[VNLF] Cài đặt Blender..."
if command -v apt &>/dev/null; then sudo apt update && sudo apt install -y blender
elif command -v dnf &>/dev/null; then sudo dnf install -y blender
elif command -v pacman &>/dev/null; then sudo pacman -S --noconfirm blender
elif command -v zypper &>/dev/null; then sudo zypper install -y blender
elif command -v flatpak &>/dev/null; then flatpak install -y flathub blender
else echo "[VNLF] Không tìm thấy package manager!"; exit 1; fi
[ $? -eq 0 ] && echo "[VNLF] Cài đặt Blender thành công!" || { echo "[VNLF] Cài đặt thất bại!"; exit 1; }
