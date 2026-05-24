#!/usr/bin/env bash
# @name        VLC Media Player
# @description Trình phát đa phương tiện mạnh mẽ hỗ trợ mọi định dạng
# @version     latest
# @category    multimedia
# @icon        applications-multimedia

set -euo pipefail

APP_NAME="VLC Media Player"
INSTALL_METHOD="apt"   # apt | snap | flatpak
CREATE_SHORTCUT=true   # tự tạo khi cài apt
CREATE_BIN_LINK=false  # đã có lệnh 'vlc' sẵn

log_info()    { echo "[INFO]  $*"; }
log_success() { echo "[OK]    $*"; }
log_step()    { echo ""; echo "==> $*"; }

main() {
    log_step "Cài đặt $APP_NAME"

    case "$INSTALL_METHOD" in
        apt)
            sudo apt-get update -qq
            sudo apt-get install -y vlc
            ;;
        snap)
            sudo snap install vlc
            ;;
        flatpak)
            flatpak install -y flathub org.videolan.VLC
            ;;
    esac

    log_success "$APP_NAME đã được cài đặt!"
}

main "$@"
