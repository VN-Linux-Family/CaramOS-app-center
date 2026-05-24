#!/usr/bin/env bash
# @name        Docker Engine
# @description Nền tảng container hoá ứng dụng phổ biến nhất
# @version     latest
# @category    development
# @icon        applications-system

set -euo pipefail

APP_NAME="Docker Engine"
INSTALL_DIR="/etc/docker"
ADD_USER_TO_GROUP=true   # true = thêm user hiện tại vào nhóm docker
CREATE_SHORTCUT=false    # Docker là công cụ CLI
CREATE_BIN_LINK=false

log_info()    { echo "[INFO]  $*"; }
log_success() { echo "[OK]    $*"; }
log_step()    { echo ""; echo "==> $*"; }

main() {
    log_step "Cài đặt $APP_NAME"

    # Gỡ phiên bản cũ nếu có
    sudo apt-get remove -y docker docker-engine docker.io containerd runc 2>/dev/null || true

    log_info "Cài đặt dependencies..."
    sudo apt-get update -qq
    sudo apt-get install -y ca-certificates curl gnupg lsb-release

    log_info "Thêm GPG key Docker..."
    sudo mkdir -p /etc/apt/keyrings
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg \
        | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg

    echo \
      "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
      https://download.docker.com/linux/ubuntu \
      $(lsb_release -cs) stable" \
      | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

    sudo apt-get update -qq
    sudo apt-get install -y docker-ce docker-ce-cli containerd.io \
        docker-buildx-plugin docker-compose-plugin

    if [ "$ADD_USER_TO_GROUP" = true ]; then
        sudo usermod -aG docker "$USER"
        log_info "Đã thêm $USER vào nhóm docker (cần đăng xuất/đăng nhập lại)"
    fi

    sudo systemctl enable docker
    sudo systemctl start docker

    log_success "$APP_NAME đã được cài đặt! Kiểm tra: docker --version"
}

main "$@"
