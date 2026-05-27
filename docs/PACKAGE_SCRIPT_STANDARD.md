# App Explorer — Package Script Standard (v1.0)

## Overview

Mỗi ứng dụng trong App Explorer được mô tả bởi một file `.aepkg` (App Explorer Package).  
Đây là file text theo chuẩn INI-like có phần `[meta]`, `[install]`, `[uninstall]`, `[verify]`, và các phần mở rộng tùy chọn.

---

## File Extension

```
<app-id>.aepkg
```

Ví dụ: `firefox.aepkg`, `neovim.aepkg`, `obs-studio.aepkg`

---

## Cấu trúc tổng quát

```
[meta]
  ...thông tin ứng dụng...

[source]
  ...nguồn dữ liệu (online/offline)...

[install]
  ...lệnh cài đặt...

[uninstall]
  ...lệnh gỡ cài đặt...

[verify]
  ...kiểm tra sau cài đặt...

[hooks]
  ...pre/post install hooks...

[desktop]
  ...thông tin .desktop entry...
```

---

## Chi tiết từng phần

### [meta] — Metadata bắt buộc

| Key              | Type     | Bắt buộc | Mô tả                                                      |
|------------------|----------|----------|------------------------------------------------------------|
| `id`             | string   | ✓        | ID duy nhất, lowercase, no space (vd: `firefox`)          |
| `name`           | string   | ✓        | Tên hiển thị (vd: `Mozilla Firefox`)                      |
| `version`        | string   | ✓        | Phiên bản (vd: `125.0.1`)                                 |
| `description`    | string   | ✓        | Mô tả ngắn (1 dòng, ≤ 160 ký tự)                         |
| `long_desc`      | text     |          | Mô tả dài, hỗ trợ `\n` để xuống dòng                     |
| `usage`          | text     |          | Hướng dẫn sử dụng sau cài đặt                             |
| `category`       | enum     | ✓        | `browser`, `editor`, `media`, `dev`, `game`, `util`, `sys`, `net`, `graphic`, `office`, `other` |
| `tags`           | list     |          | Tags phân cách bởi `;` (vd: `browser;web;mozilla`)        |
| `author`         | string   |          | Tác giả hoặc tổ chức (vd: `Mozilla Foundation`)          |
| `maintainer`     | string   |          | Người duy trì package này                                  |
| `license`        | string   |          | Loại giấy phép (vd: `MPL-2.0`, `MIT`, `GPL-3.0`)         |
| `homepage`       | url      |          | Trang chủ ứng dụng                                         |
| `source_url`     | url      |          | Link code nguồn (GitHub, etc.)                            |
| `arch`           | list     |          | Kiến trúc hỗ trợ: `x86_64;arm64;armhf` (mặc định: all)  |
| `os`             | list     |          | OS hỗ trợ: `linux;freebsd` (mặc định: linux)             |
| `min_os_ver`     | string   |          | Phiên bản OS tối thiểu                                    |

### [source] — Nguồn dữ liệu

| Key              | Type     | Mô tả                                                      |
|------------------|----------|------------------------------------------------------------|
| `icon_online`    | url      | URL icon ứng dụng (ưu tiên thấp hơn offline)             |
| `icon_offline`   | path     | Đường dẫn local tới icon (ưu tiên cao hơn online)        |
| `icon_size`      | string   | Kích thước icon: `48x48`, `64x64`, `128x128`, `256x256`  |
| `icon_format`    | enum     | `png`, `svg`, `xpm`                                        |
| `screenshot_online` | url[] | URLs ảnh chụp màn hình, phân cách bởi `;`               |
| `screenshot_offline`| path[]| Paths local ảnh chụp màn hình                            |
| `pkg_online`     | url      | URL tải gói cài đặt (ưu tiên thấp hơn offline)          |
| `pkg_offline`    | path     | Đường dẫn local tới gói cài đặt (ưu tiên cao hơn)       |

**Quy tắc ưu tiên nguồn:**
- Nếu `*_offline` có giá trị → dùng offline (dù online có hay không)
- Nếu `*_offline` trống + `*_online` có giá trị → dùng online
- Nếu cả hai trống → bỏ qua hoặc báo lỗi

### [install] — Cài đặt

| Key              | Type     | Bắt buộc | Mô tả                                                      |
|------------------|----------|----------|------------------------------------------------------------|
| `method`         | enum     | ✓        | Phương thức: `apt`, `dnf`, `pacman`, `flatpak`, `snap`, `script`, `appimage`, `deb`, `rpm`, `tar` |
| `packages`       | list     |          | Tên gói (cho apt/dnf/pacman), phân cách bởi ` `           |
| `flatpak_id`     | string   |          | Flatpak app ID (vd: `org.mozilla.firefox`)                |
| `snap_name`      | string   |          | Snap package name                                          |
| `script`         | text     |          | Shell script inline (cho method=script)                   |
| `script_file`    | path     |          | Đường dẫn file script ngoài                               |
| `deps`           | list     |          | Gói phụ thuộc cần cài trước, phân cách bởi ` `            |
| `sudo_required`  | bool     |          | Yêu cầu sudo (mặc định: true)                            |
| `interactive`    | bool     |          | Script có tương tác người dùng không (mặc định: false)   |
| `estimated_size` | string   |          | Kích thước ước tính sau cài (vd: `256MB`)                |
| `download_size`  | string   |          | Kích thước download (vd: `64MB`)                          |
| `checksum_type`  | enum     |          | `sha256`, `sha512`, `md5`                                 |
| `checksum`       | string   |          | Giá trị checksum của file cài đặt                         |
| `repo_key_url`   | url      |          | URL GPG/PGP key của repo (nếu cần thêm repo ngoài)      |
| `repo_entry`     | string   |          | Dòng thêm vào sources.list/repo file                      |

### [uninstall] — Gỡ cài đặt

| Key              | Type     | Mô tả                                                      |
|------------------|----------|------------------------------------------------------------|
| `method`         | enum     | Giống [install].method                                     |
| `packages`       | list     | Gói cần gỡ (thường giống install)                         |
| `purge`          | bool     | Xoá cả config files không (mặc định: false)              |
| `script`         | text     | Script gỡ inline                                           |
| `script_file`    | path     | File script gỡ ngoài                                       |
| `remove_deps`    | bool     | Gỡ luôn các dep không còn dùng (mặc định: false)        |
| `remove_data`    | bool     | Xoá data người dùng không (mặc định: false)              |
| `data_paths`     | list     | Danh sách path data người dùng, phân cách bởi `;`         |

### [verify] — Xác minh cài đặt

| Key              | Type     | Mô tả                                                      |
|------------------|----------|------------------------------------------------------------|
| `binary`         | path     | Đường dẫn binary cần tồn tại sau cài (vd: `/usr/bin/firefox`) |
| `check_cmd`      | string   | Lệnh kiểm tra, exit 0 = thành công (vd: `firefox --version`) |
| `check_file`     | path     | File cần tồn tại sau cài                                   |
| `check_service`  | string   | Systemd service cần chạy                                   |

### [hooks] — Pre/Post hooks

| Key              | Type     | Mô tả                                                      |
|------------------|----------|------------------------------------------------------------|
| `pre_install`    | text     | Script chạy trước khi cài                                  |
| `post_install`   | text     | Script chạy sau khi cài thành công                        |
| `pre_uninstall`  | text     | Script chạy trước khi gỡ                                   |
| `post_uninstall` | text     | Script chạy sau khi gỡ thành công                         |

### [desktop] — Desktop Entry

| Key              | Type     | Mô tả                                                      |
|------------------|----------|------------------------------------------------------------|
| `desktop_name`   | string   | Tên trong .desktop file                                    |
| `desktop_exec`   | string   | Lệnh chạy (vd: `firefox %u`)                              |
| `desktop_icon`   | string   | Tên icon hệ thống                                          |
| `desktop_cats`   | string   | Categories cho .desktop                                    |
| `create_desktop` | bool     | Tạo .desktop entry không (mặc định: true nếu có exec)   |

---

## Ví dụ đầy đủ

```ini
[meta]
id          = firefox
name        = Mozilla Firefox
version     = 125.0.1
description = Trình duyệt web mã nguồn mở, nhanh và bảo mật
long_desc   = Mozilla Firefox là trình duyệt web miễn phí, mã nguồn mở.\nHỗ trợ đầy đủ các tiêu chuẩn web hiện đại, bảo vệ quyền riêng tư người dùng.\nCó hệ sinh thái extensions phong phú trên addons.mozilla.org.
usage       = Sau cài đặt, chạy: firefox\nHoặc tìm trong Application Menu → Internet → Firefox
category    = browser
tags        = browser;web;mozilla;internet
author      = Mozilla Foundation
maintainer  = community
license     = MPL-2.0
homepage    = https://www.mozilla.org/firefox
arch        = x86_64;arm64

[source]
icon_online         = https://upload.wikimedia.org/wikipedia/commons/a/a0/Firefox_logo%2C_2019.svg
icon_offline        =
icon_size           = 128x128
icon_format         = svg
pkg_online          =
pkg_offline         =

[install]
method          = apt
packages        = firefox
deps            =
sudo_required   = true
interactive     = false
estimated_size  = 280MB
download_size   = 75MB

[uninstall]
method          = apt
packages        = firefox
purge           = false
remove_deps     = true
remove_data     = false
data_paths      = ~/.mozilla/firefox;~/.cache/mozilla

[verify]
binary          = /usr/bin/firefox
check_cmd       = firefox --version

[hooks]
pre_install     =
post_install    = echo "Firefox installed successfully. Run: firefox"
pre_uninstall   =
post_uninstall  = rm -rf ~/.cache/mozilla/firefox

[desktop]
desktop_name    = Firefox Web Browser
desktop_exec    = firefox %u
desktop_icon    = firefox
desktop_cats    = Network;WebBrowser
create_desktop  = true
```

---

## Quy ước đặt tên file

- Tên file = `id` field + `.aepkg`
- Lưu tại: `~/.config/app-explorer/packages/` (local) hoặc đường dẫn repo
- Encoding: UTF-8
- Line ending: LF (Unix)

---

## Validation Rules

1. `id` chỉ chứa ký tự `[a-z0-9_-]`
2. `version` theo SemVer hoặc distro version
3. `method` phải là một trong các giá trị enum định nghĩa
4. Nếu `method=script` thì phải có `script` hoặc `script_file`
5. Nếu `method=flatpak` thì phải có `flatpak_id`
6. `checksum` bắt buộc nếu `pkg_online` có giá trị
7. `icon_size` theo format `WxH` (vd: `128x128`)
