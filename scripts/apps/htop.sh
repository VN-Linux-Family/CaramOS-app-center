#!/usr/bin/env bash
# @name        htop
# @description Trình quản lý tiến trình tương tác cho terminal
# @version     latest
# @category    utilities
# @icon        utilities-system-monitor

set -euo pipefail

APP_NAME="htop"
CREATE_SHORTCUT=false  # Terminal app – không cần .desktop
CREATE_BIN_LINK=false  # Lệnh đã có sẵn

log_success() { echo "[OK]    $*"; }
log_step()    { echo ""; echo "==> $*"; }

main() {
    log_step "Cài đặt $APP_NAME"
    sudo apt-get update -qq
    sudo apt-get install -y htop
    log_success "$APP_NAME đã được cài đặt! Chạy lệnh: htop"
}

main "$@"
