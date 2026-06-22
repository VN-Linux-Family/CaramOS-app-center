#!/bin/bash
# VNLF App Store - VS Code install script
echo "[VNLF] Cài đặt Visual Studio Code..."
if command -v snap &>/dev/null; then
    snap install --classic code
elif command -v apt &>/dev/null; then
    wget -qO- https://packages.microsoft.com/keys/microsoft.asc | gpg --dearmor > /tmp/packages.microsoft.gpg
    install -o root -g root -m 644 /tmp/packages.microsoft.gpg /etc/apt/trusted.gpg.d/
    sh -c 'echo "deb [arch=amd64 signed-by=/etc/apt/trusted.gpg.d/packages.microsoft.gpg] https://packages.microsoft.com/repos/code stable main" > /etc/apt/sources.list.d/vscode.list'
    apt update && apt install -y code
elif command -v flatpak &>/dev/null; then
    flatpak install -y flathub com.visualstudio.code
else
    echo "[VNLF] Không tìm thấy phương thức cài đặt!"; exit 1
fi
[ $? -eq 0 ] && echo "[VNLF] Cài đặt VS Code thành công!" || { echo "[VNLF] Cài đặt thất bại!"; exit 1; }
