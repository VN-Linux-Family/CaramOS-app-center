# VNLF App Store

Kho ứng dụng Linux tiếng Việt — ứng dụng desktop viết bằng **C++ + GTK3**.

Giao diện lấy cảm hứng từ [VNLF App Explorer](https://apps.vietnamlinuxfamily.net/), nhưng chạy hoàn toàn trên desktop Linux.

---

## Tính năng

- 🔍 **Tìm kiếm** ứng dụng theo tên, mô tả, thẻ
- 📂 **Lọc theo danh mục**: Internet, Đa phương tiện, Văn phòng, Đồ họa, Lập trình, Hệ thống...
- 📋 **Xem chi tiết** ứng dụng: mô tả, tác giả, phiên bản, giấy phép, website
- ⚙️ **Cài đặt** bằng nút "Cài đặt" — gọi script bash có sẵn
- 🖥️ **Cửa sổ terminal inline** hiển thị tiến trình cài đặt thời gian thực
- 📦 **Dữ liệu ứng dụng** từ file `data/apps.json` — dễ mở rộng

---

## Cấu trúc dự án

```
vnlf-store/
├── src/
│   └── main.cpp          # Toàn bộ source code C++/GTK3
├── data/
│   └── apps.json         # Danh sách ứng dụng (dễ chỉnh sửa)
├── scripts/
│   ├── install-generic.sh      # Script cài đặt tổng quát
│   ├── install-firefox.sh
│   ├── install-vlc.sh
│   ├── install-libreoffice.sh
│   ├── install-gimp.sh
│   ├── install-vscode.sh
│   ├── install-obs.sh
│   └── ... (1 script/app)
├── Makefile
└── README.md
```

---

## Yêu cầu

```bash
# Ubuntu/Debian
sudo apt install libgtk-3-dev pkg-config g++ make libvte-2.91-dev

# Fedora/RHEL
sudo dnf install gtk3-devel gcc-c++ make pkgconf vte291-devel

# Arch Linux
sudo pacman -S gtk3 gcc make pkgconf vte3
```

---

## Build & Chạy

```bash
# Build
make

# Chạy trực tiếp
./vnlf-store

# Hoặc build + chạy luôn
make run

# Cài đặt vào hệ thống
sudo make install
```

---

## Thêm ứng dụng mới

### 1. Thêm vào `data/apps.json`

```json
{
  "id": "myapp",
  "name": "Tên ứng dụng",
  "summary": "Mô tả ngắn",
  "description": "Mô tả đầy đủ về ứng dụng...",
  "category": "internet",
  "icon": "tên-icon-gtk",
  "version": "1.0.0",
  "author": "Tác giả",
  "license": "GPL-3.0",
  "website": "https://...",
  "script": "install-myapp.sh",
  "tags": ["thẻ1", "thẻ2"]
}
```

**Danh mục hợp lệ:** `internet`, `multimedia`, `office`, `graphics`, `development`, `system`, `games`, `education`, `utilities`

**Tên icon:** Dùng tên icon GTK chuẩn (vd: `firefox`, `vlc`, `gimp`, `code`...). Xem danh sách tại https://specifications.freedesktop.org/icon-naming-spec/latest/

### 2. Tạo script cài đặt `scripts/install-myapp.sh`

```bash
#!/bin/bash
echo "[VNLF] Cài đặt Tên App..."

if command -v apt &>/dev/null; then
    sudo apt install -y myapp-package
elif command -v dnf &>/dev/null; then
    sudo dnf install -y myapp-package
elif command -v pacman &>/dev/null; then
    sudo pacman -S --noconfirm myapp-package
else
    echo "[VNLF] Không tìm thấy package manager!"
    exit 1
fi

[ $? -eq 0 ] && echo "[VNLF] Cài đặt thành công!" || exit 1
```

```bash
chmod +x scripts/install-myapp.sh
```

---

## Cơ chế cài đặt

Khi người dùng nhấn **Cài đặt**:

1. App tìm file `scripts/<script>` tương ứng với ứng dụng
2. Nếu không tìm thấy → dùng `install-generic.sh` với `<app-id>` làm tham số
3. Chạy script qua `pkexec` (nếu có) để xác thực quyền root, hoặc bash trực tiếp
4. Output của script hiển thị real-time trong cửa sổ terminal

---

## Tùy chỉnh giao diện

Toàn bộ CSS nằm trong hằng số `APP_CSS` trong `src/main.cpp`. Chỉnh màu sắc, font, bo góc... tại đó.

---

*VNLF App Store — Cộng đồng Linux Việt Nam 🇻🇳*
