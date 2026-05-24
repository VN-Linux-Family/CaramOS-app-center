#!/usr/bin/env bash
# @name        Visual Studio Code
# @description Trình soạn thảo code mạnh mẽ từ Microsoft
# @version     latest
# @category    development
# @icon        applications-development

set -euo pipefail

APP_NAME="Visual Studio Code"
DOWNLOAD_URL="https://code.visualstudio.com/sha/download?build=stable&os=linux-deb-x64"
TMP_DIR="/tmp/ubuntu-store-dl/vscode"
CREATE_SHORTCUT=true   # Tự động tạo khi cài .deb
CREATE_BIN_LINK=true

log_info()    { echo "[INFO]  $*"; }
log_success() { echo "[OK]    $*"; }
log_error()   { echo "[ERROR] $*" >&2; }
log_step()    { echo ""; echo "==> $*"; }

main() {
    log_step "Cài đặt $APP_NAME"

    # Cài qua apt (cách khuyên dùng – thêm repo Microsoft)
    log_info "Thêm repository Microsoft..."
    wget -qO- https://packages.microsoft.com/keys/microsoft.asc \
        | gpg --dearmor > /tmp/microsoft.gpg
    sudo install -D -o root -g root -m 644 \
        /tmp/microsoft.gpg /etc/apt/keyrings/packages.microsoft.gpg

    sudo sh -c 'echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/packages.microsoft.gpg] \
        https://packages.microsoft.com/repos/code stable main" \
        > /etc/apt/sources.list.d/vscode.list'

    log_info "Cập nhật danh sách gói..."
    sudo apt-get update -qq

    log_info "Cài đặt code..."
    sudo apt-get install -y code

    log_success "$APP_NAME đã được cài đặt!"
    echo "Chạy lệnh: code"
}

main "$@"
