#!/usr/bin/env bash
# @name        Firefox
# @description Trình duyệt web nhanh và bảo mật từ Mozilla
# @version     latest
# @category    internet
# @icon        web-browser

set -euo pipefail

APP_NAME="Firefox"
INSTALL_METHOD="apt"   # apt | snap
CREATE_SHORTCUT=true
CREATE_BIN_LINK=false

log_info()    { echo "[INFO]  $*"; }
log_success() { echo "[OK]    $*"; }
log_step()    { echo ""; echo "==> $*"; }

main() {
    log_step "Cài đặt $APP_NAME"

    case "$INSTALL_METHOD" in
        apt)
            # Gỡ snap Firefox nếu có, cài bằng apt
            sudo snap remove firefox 2>/dev/null || true
            sudo add-apt-repository -y ppa:mozillateam/ppa
            echo '
Package: *
Pin: release o=LP-PPA-mozillateam
Pin-Priority: 1001
' | sudo tee /etc/apt/preferences.d/mozilla-firefox > /dev/null
            sudo apt-get update -qq
            sudo apt-get install -y firefox
            ;;
        snap)
            sudo snap install firefox
            ;;
    esac

    log_success "$APP_NAME đã được cài đặt!"
}

main "$@"
