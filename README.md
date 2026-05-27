# App Explorer

Software center cho Linux, viết bằng C++ với GTK 3.
Giao diện tối giản, hỗ trợ ricing đầy đủ qua file `.cfg`.

---

## Tính năng

- **Duyệt & tìm kiếm** ứng dụng theo category, text search
- **Cài đặt & gỡ cài đặt** với nhiều phương thức: `apt`, `dnf`, `pacman`, `flatpak`, `snap`, `script`, `appimage`, `deb`, `rpm`
- **Console view** read-only, hiển thị output thực tế của quá trình cài/gỡ
- **Hệ thống theme** đầy đủ: load `.cfg` → generate GTK CSS → apply ngay lập tức
- **Log system**: GUI log luôn ghi; operation log chỉ giữ khi có lỗi
- **Package scripts** chuẩn `.aepkg`: metadata, install, uninstall, verify, hooks, desktop entry
- **Nguồn dữ liệu**: offline ưu tiên hơn online; tự động fallback

---

## Build

### Yêu cầu

```
g++ >= 9  (C++17)
cmake >= 3.16
pkg-config
libgtk-3-dev
```

### Debian/Ubuntu

```bash
sudo apt install build-essential cmake pkg-config libgtk-3-dev
```

### Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./app-explorer
```

### Install

```bash
sudo make install
```

---

## Cấu trúc thư mục

```
~/.config/app-explorer/
├── logs/
│   ├── gui.log                     # Log GUI, luôn ghi
│   ├── install_firefox_20240101.log  # Chỉ giữ khi lỗi
│   └── uninstall_vlc_20240101.log
├── packages/
│   ├── firefox.aepkg
│   ├── neovim.aepkg
│   └── ...
└── themes/
    ├── classic-ivory.cfg           # Theme mặc định
    ├── forest-dark.cfg
    ├── nord-frost.cfg
    ├── catppuccin-mocha.cfg
    └── your-custom.cfg

/usr/share/app-explorer/
├── packages/                       # System packages
└── themes/                         # System themes
```

---

## Package Script (.aepkg)

Mỗi ứng dụng = một file `<id>.aepkg`. Xem chi tiết tại:
[docs/PACKAGE_SCRIPT_STANDARD.md](docs/PACKAGE_SCRIPT_STANDARD.md)

**Ví dụ nhanh:**

```ini
[meta]
id          = neovim
name        = Neovim
version     = 0.9.5
description = Hyperextensible Vim-based text editor
category    = editor
tags        = editor;vim;terminal;dev

[source]
icon_online  = https://example.com/nvim.svg
icon_offline =
# Quy tắc: offline ưu tiên; nếu offline trống thì dùng online

[install]
method      = apt
packages    = neovim
checksum_type = sha256
checksum    = abc123...

[uninstall]
method      = apt
packages    = neovim

[verify]
binary      = /usr/bin/nvim
```

---

## Theme & Ricing (.cfg)

Tất cả màu sắc, font, kích thước, bo cong, shadow đều có thể custom:

```ini
[theme]
name = my-rice
dark_mode = true

[colors.base]
bg_primary   = #1A1A2E
bg_card      = #16213E

[colors.accent]
primary      = #E94560
hover        = #FF6B81

[typography]
font_ui      = "JetBrains Mono"
font_ui_size = 10
font_mono    = "CaskaydiaCove Nerd Font"

[radius]
card         = 12
button       = 8

[gtk_extra]
inline_css = .nav-logo { letter-spacing: 2px; }
```

### Apply theme

- **Qua UI**: click nút 🎨 trên toolbar → chọn file `.cfg`
- **Qua env**: `AE_THEME=/path/to/theme.cfg app-explorer`
- **Mặc định**: `~/.config/app-explorer/themes/classic-ivory.cfg`

### Themes có sẵn

| File | Phong cách |
|---|---|
| `classic-ivory.cfg` | Trắng ngà, cổ điển, mặc định |
| `forest-dark.cfg` | Tối, xanh lá rừng |
| `nord-frost.cfg` | Nord palette, arctic blue |
| `catppuccin-mocha.cfg` | Catppuccin Mocha, pastel tím |

---

## Biến môi trường

| Biến | Mô tả |
|---|---|
| `AE_THEME` | Override đường dẫn file theme |
| `AE_PACKAGES_DIR` | Override thư mục packages |

---

## Log

- `~/.config/app-explorer/logs/gui.log` — luôn ghi, mọi hành động GUI
- `~/.config/app-explorer/logs/install_<id>_<timestamp>.log` — chỉ giữ khi install THẤT BẠI
- `~/.config/app-explorer/logs/uninstall_<id>_<timestamp>.log` — chỉ giữ khi uninstall THẤT BẠI

---

## License

MIT
