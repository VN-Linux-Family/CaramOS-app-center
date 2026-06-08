#!/bin/bash
# install-vscode.sh - VNLF App Store
echo "[VNLF] Cài đặt Visual Studio Code..."
if command -v snap &>/dev/null; then
    sudo snap install --classic code
elif command -v apt &>/dev/null; then
    wget -qO- https://packages.microsoft.com/keys/microsoft.asc | gpg --dearmor > packages.microsoft.gpg
    sudo install -o root -g root -m 644 packages.microsoft.gpg /etc/apt/trusted.gpg.d/
    sudo sh -c 'echo "deb [arch=amd64 signed-by=/etc/apt/trusted.gpg.d/packages.microsoft.gpg] https://packages.microsoft.com/repos/code stable main" > /etc/apt/sources.list.d/vscode.list'
    sudo apt update && sudo apt install -y code
    rm -f packages.microsoft.gpg
elif command -v flatpak &>/dev/null; then
    flatpak install -y flathub com.visualstudio.code
else
    echo "[VNLF] Không tìm thấy phương thức cài đặt phù hợp!"; exit 1
fi
[ $? -eq 0 ] && echo "[VNLF] Cài đặt VS Code thành công!" || { echo "[VNLF] Cài đặt thất bại!"; exit 1; }
