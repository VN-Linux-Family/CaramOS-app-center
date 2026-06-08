#!/bin/bash
# install-vlc.sh
echo "[VNLF] Cài đặt VLC Media Player..."
if command -v apt &>/dev/null; then sudo apt update && sudo apt install -y vlc
elif command -v dnf &>/dev/null; then sudo dnf install -y vlc
elif command -v pacman &>/dev/null; then sudo pacman -S --noconfirm vlc
elif command -v zypper &>/dev/null; then sudo zypper install -y vlc
else echo "[VNLF] Không tìm thấy package manager!"; exit 1; fi
[ $? -eq 0 ] && echo "[VNLF] Cài đặt VLC thành công!" || { echo "[VNLF] Cài đặt thất bại!"; exit 1; }
