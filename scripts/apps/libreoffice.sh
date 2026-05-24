#!/usr/bin/env bash
# @name        LibreOffice
# @description Bộ ứng dụng văn phòng mã nguồn mở toàn diện
# @version     latest
# @category    office
# @icon        libreoffice-startcenter

set -euo pipefail

APP_NAME="LibreOffice"
INSTALL_FULL=true      # true = cài toàn bộ, false = chỉ cài phần cốt lõi
INSTALL_LANGPACK=true  # true = cài gói ngôn ngữ tiếng Việt
CREATE_SHORTCUT=true
CREATE_BIN_LINK=false

log_info()    { echo "[INFO]  $*"; }
log_success() { echo "[OK]    $*"; }
log_step()    { echo ""; echo "==> $*"; }

main() {
    log_step "Cài đặt $APP_NAME"
    sudo apt-get update -qq

    if [ "$INSTALL_FULL" = true ]; then
        sudo apt-get install -y libreoffice
    else
        sudo apt-get install -y libreoffice-core libreoffice-writer \
            libreoffice-calc libreoffice-impress
    fi

    if [ "$INSTALL_LANGPACK" = true ]; then
        log_info "Cài gói ngôn ngữ tiếng Việt..."
        sudo apt-get install -y libreoffice-l10n-vi || true
    fi

    log_success "$APP_NAME đã được cài đặt!"
}

main "$@"
