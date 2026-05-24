## Cấu trúc dự án

```
ubuntu-store/
├── Makefile
├── include/
│   ├── types.h          ← định nghĩa AppInfo, Theme, CustomThemeColors, #define SCRIPTS_DIR
│   ├── main_window.h    ← cửa sổ chính GTK3
│   ├── theme_manager.h  ← quản lý 3 theme
│   ├── app_loader.h     ← đọc & parse scripts
│   └── installer.h      ← chạy script, stream output
├── src/
│   ├── main.cpp
│   ├── main_window.cpp  ← toàn bộ UI GTK3
│   ├── theme_manager.cpp
│   ├── app_loader.cpp
│   └── installer.cpp
└── scripts/
    ├── TEMPLATE.sh      ← mẫu tạo script mới
    └── apps/            ← ← ĐÂY LÀ VỊ TRÍ ĐẶT SCRIPT (SCRIPTS_DIR)
        ├── vscode.sh
        ├── vlc.sh
        ├── firefox.sh
        ├── gimp.sh
        ├── docker.sh
        ├── htop.sh
        └── libreoffice.sh
```


## Cách build và chạy

```bash
cd ubuntu-store

# Cài dependencies (chỉ lần đầu)
make install-deps

# Build
make

# Chạy
make run
```

---

## Hệ thống Script – Metadata bắt buộc

Mỗi file `.sh` trong `scripts/apps/` cần có header như sau:

```bash
# @name        Tên hiển thị trong Store
# @description Mô tả ngắn
# @version     1.0.0
# @category    development|office|multimedia|internet|utilities|games
# @icon        gtk-icon-name
```

Các **biến chuẩn** trong mỗi script:

| Biến | Ý nghĩa |
|------|---------|
| `APP_NAME` | Tên phần mềm |
| `DOWNLOAD_URL` | Link tải |
| `INSTALL_DIR` | Thư mục cài đặt |
| `CREATE_SHORTCUT` | `true/false` tạo file `.desktop` |
| `CREATE_BIN_LINK` | `true/false` tạo symlink trong `/usr/local/bin` |

**Thêm phần mềm mới**: chỉ cần copy `TEMPLATE.sh` vào `scripts/apps/`, đổi tên thành `<id>.sh`, điền metadata và biến — app tự động xuất hiện trong Store sau khi chạy lại.

## Một số điều cần làm

- Thêm uninstall và dọn rác.
- Thêm bộ icons cho đẹp.
- Thêm khả năng import/export list ứng dụng cần/đã cài (dành cho những ai có như cầu cài đặt hàng loạt hoặc cài lại OS mà lười gõ lệnh).
- Thêm khả năng cập nhật ứng dụng.
- Thêm ...
- Điều chỉnh lại màu sắc ứng dụng theo bảng màu của CaramOS.

