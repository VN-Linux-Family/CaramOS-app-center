#!/usr/bin/env bash
# @name        GIMP
# @description Phần mềm chỉnh sửa ảnh chuyên nghiệp mã nguồn mở
# @version     latest
# @category    multimedia
# @icon        applications-graphics

set -euo pipefail

APP_NAME="GIMP"
PPA="ppa:ubuntuhandbook1/gimp"   # PPA để có phiên bản mới nhất (tuỳ chọn)
USE_PPA=false                     # true = cài từ PPA, false = cài từ apt
CREATE_SHORTCUT=true
CREATE_BIN_LINK=false

log_info()    { echo "[INFO]  $*"; }
log_success() { echo "[OK]    $*"; }
log_step()    { echo ""; echo "==> $*"; }

main() {
    log_step "Cài đặt $APP_NAME"

    if [ "$USE_PPA" = true ]; then
        log_info "Thêm PPA: $PPA"
        sudo add-apt-repository -y "$PPA"
    fi

    sudo apt-get update -qq
    sudo apt-get install -y gimp

    log_success "$APP_NAME đã được cài đặt!"
}

main "$@"
