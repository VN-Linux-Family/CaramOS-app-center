#!/bin/bash
# VNLF App Store - OBS Studio install script
echo "[VNLF] Cài đặt OBS Studio..."
if command -v apt &>/dev/null; then
    add-apt-repository -y ppa:obsproject/obs-studio 2>/dev/null || true
    apt update && apt install -y obs-studio
elif command -v dnf &>/dev/null; then
    dnf install -y obs-studio
elif command -v pacman &>/dev/null; then
    pacman -S --noconfirm obs-studio
elif command -v flatpak &>/dev/null; then
    flatpak install -y flathub com.obsproject.Studio
else
    echo "[VNLF] Không tìm thấy phương thức cài đặt!"; exit 1
fi
[ $? -eq 0 ] && echo "[VNLF] Cài đặt OBS Studio thành công!" || { echo "[VNLF] Cài đặt thất bại!"; exit 1; }
