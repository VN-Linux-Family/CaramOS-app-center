#!/bin/bash
# install-obs.sh - VNLF App Store
echo "[VNLF] Cài đặt OBS Studio..."
if command -v apt &>/dev/null; then
    sudo add-apt-repository -y ppa:obsproject/obs-studio 2>/dev/null || true
    sudo apt update && sudo apt install -y obs-studio
elif command -v dnf &>/dev/null; then sudo dnf install -y obs-studio
elif command -v pacman &>/dev/null; then sudo pacman -S --noconfirm obs-studio
elif command -v flatpak &>/dev/null; then flatpak install -y flathub com.obsproject.Studio
else echo "[VNLF] Không tìm thấy package manager!"; exit 1; fi
[ $? -eq 0 ] && echo "[VNLF] Cài đặt OBS Studio thành công!" || { echo "[VNLF] Cài đặt thất bại!"; exit 1; }
