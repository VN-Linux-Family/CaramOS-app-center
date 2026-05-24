#!/usr/bin/env bash
# ============================================================
#  TEMPLATE SCRIPT – Ubuntu Store
#
#  Metadata (bắt buộc đặt ở đầu file, trước mọi code):
# @name        Tên phần mềm hiển thị trong Store
# @description Mô tả ngắn về phần mềm
# @version     1.0.0
# @category    development|office|multimedia|internet|utilities|games
# @icon        gtk-icon-name  (ví dụ: applications-development)
#
#  Các biến và hàm chuẩn bên dưới – sao chép template này khi
#  tạo script mới, thay giá trị theo phần mềm cụ thể.
# ============================================================

set -euo pipefail

# ─── BIẾN CẤU HÌNH ──────────────────────────────────────────
APP_NAME="Tên phần mềm"           # Tên đầy đủ
APP_VERSION="1.0.0"                # Phiên bản
DOWNLOAD_URL="https://..."         # URL tải file cài đặt
INSTALL_DIR="/opt/${APP_NAME}"     # Thư mục cài đặt
DESKTOP_DIR="/usr/share/applications"  # Thư mục desktop entry
ICON_PATH="/usr/share/pixmaps"     # Thư mục icon
TMP_DIR="/tmp/ubuntu-store-dl"     # Thư mục tạm
CREATE_SHORTCUT=true               # true/false: tạo shortcut .desktop
CREATE_BIN_LINK=true               # true/false: tạo symlink trong /usr/local/bin

# ─── HÀM TIỆN ÍCH ───────────────────────────────────────────

# In thông báo với màu sắc
log_info()    { echo "[INFO]  $*"; }
log_success() { echo "[OK]    $*"; }
log_error()   { echo "[ERROR] $*" >&2; }
log_step()    { echo ""; echo "==> $*"; }

# Kiểm tra lệnh tồn tại
require_cmd() {
    command -v "$1" &>/dev/null || {
        log_error "Cần cài '$1' trước. Chạy: sudo apt-get install $1"
        exit 1
    }
}

# Tải file, hiển thị tiến trình
download_file() {
    local url="$1"
    local dest="$2"
    log_info "Đang tải: $url"
    wget -q --show-progress -O "$dest" "$url" || \
    curl -L --progress-bar -o "$dest" "$url"
}

# Tạo thư mục cài đặt
create_install_dir() {
    mkdir -p "$INSTALL_DIR"
    log_info "Thư mục cài đặt: $INSTALL_DIR"
}

# Tạo file .desktop (shortcut)
create_desktop_shortcut() {
    local exec_path="$1"
    local icon="$2"
    local comment="${3:-$APP_NAME}"

    local desktop_file="$DESKTOP_DIR/${APP_NAME// /_}.desktop"
    cat > "$desktop_file" <<EOF
[Desktop Entry]
Name=$APP_NAME
Comment=$comment
Exec=$exec_path
Icon=$icon
Terminal=false
Type=Application
Categories=Application;
StartupNotify=true
EOF
    chmod +x "$desktop_file"
    log_success "Shortcut tạo tại: $desktop_file"
}

# Tạo symlink trong /usr/local/bin
create_bin_link() {
    local target="$1"
    local link_name="${2:-${APP_NAME// /-}}"
    ln -sf "$target" "/usr/local/bin/$link_name"
    log_success "Lệnh có thể dùng: $link_name"
}

# Dọn dẹp thư mục tạm
cleanup() {
    rm -rf "$TMP_DIR"
    log_info "Dọn dẹp hoàn tất."
}

# ─── SCRIPT CHÍNH ────────────────────────────────────────────
main() {
    log_step "Bắt đầu cài đặt $APP_NAME v$APP_VERSION"

    # Kiểm tra dependencies
    require_cmd wget

    # Tạo thư mục tạm
    mkdir -p "$TMP_DIR"

    # TODO: Thêm các bước cài đặt cụ thể ở đây
    # Ví dụ:
    #   download_file "$DOWNLOAD_URL" "$TMP_DIR/package.deb"
    #   sudo dpkg -i "$TMP_DIR/package.deb"
    #   create_install_dir
    #   if [ "$CREATE_SHORTCUT" = true ]; then
    #       create_desktop_shortcut "$INSTALL_DIR/bin/app" "app-icon"
    #   fi
    #   if [ "$CREATE_BIN_LINK" = true ]; then
    #       create_bin_link "$INSTALL_DIR/bin/app"
    #   fi

    cleanup
    log_success "$APP_NAME cài đặt thành công!"
}

main "$@"
