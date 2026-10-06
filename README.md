# App Center CaramOS

App Center (`caramos-app-center`) có hai chế độ:

- **Trình chọn ứng dụng lần đầu**: tự mở ở lần đăng nhập đầu tiên của mỗi user trên máy cài từ ISO CaramOS. Người dùng chọn bộ văn phòng, trình duyệt, ứng dụng liên lạc và tiện ích; mỗi lựa chọn ghi rõ ưu và nhược điểm.
- **App Center**: mở từ menu (System > App Center CaramOS) để tìm, cài và gỡ ứng dụng bất cứ lúc nào. Menu ⋮ có mục mở lại trình chọn ứng dụng.

App Center chỉ gợi ý các ứng dụng cơ bản. Ứng dụng khác nằm trên [apps.vietnamlinuxfamily.net](https://apps.vietnamlinuxfamily.net/): nút "Tìm thêm ứng dụng" ở cuối thanh bên, và khi tìm không thấy thì App Center mở kết quả tìm cùng từ khoá trên trang đó.

Giao diện: Python + GTK4 + libadwaita, chạy trên CaramOS (Linux Mint 22.3 / Ubuntu 24.04). Bản C++/GTK3 cũ vẫn còn trong lịch sử git (tag `legacy-cpp`).

## Phát triển

```bash
make test      # unit test (không cần GTK)
make run       # chạy giao diện từ mã nguồn, helper giả lập (không cài gì thật)
make build     # build dist/caramos-app-center_<version>_all.deb
make pot       # cập nhật po/caramos-app-center/*.pot và gộp vào vi.po sau khi sửa chuỗi giao diện
```

Gói build cần `debhelper dpkg-dev gettext python3`; chạy giao diện cần `gir1.2-gtk-4.0 gir1.2-adw-1`.

Chuỗi giao diện viết bằng tiếng Anh trong code (`_("...")`), bản tiếng Việt ở `po/caramos-app-center/vi.po`. Tên, mô tả, ưu/nhược điểm của ứng dụng nằm trong `catalog.json` dạng `{"vi": ..., "en": ...}`. Test chặn chữ tiếng Việt viết thẳng trong giao diện và chuỗi chưa dịch.

## Phát hành

App Center có phiên bản riêng (không đi theo phiên bản CaramOS) và lên cùng PPA `ppa:vietnamlinuxfamily/caram-os`:

```bash
make changelog VERSION=0.2.0    # mở mục mới trong debian/changelog, rồi ghi các thay đổi vào đó
make release VERSION=0.2.0      # test, build, ký GPG, upload lên PPA
```

Máy CaramOS tự nhận bản mới trong vòng một ngày: lần kiểm tra OTA hằng ngày (`caramos-ota --check`) cập nhật cả `caramos-ota` lẫn `caramos-app-center`. Thêm hay sửa ứng dụng gợi ý chỉ cần release App Center, không cần ra bản CaramOS mới.

## Trong CaramOS

- Repo này là submodule `packages/caramos-app-center` của [CaramOS](https://github.com/VN-Linux-Family/CaramOS). Build ISO dựng gói từ submodule và cài vào rootfs.
- ISO tạo `/etc/caramos-app-center/first-run` (overlay `config/includes.chroot`) để bật trình chọn ứng dụng lần đầu.
- `caramos-ota` khai báo `Recommends: caramos-app-center`, nên máy đã cài nhận App Center qua OTA (chỉ có mục trong menu, không bật trình chọn lần đầu).

## Cài ứng dụng như thế nào

| Cách cài (`install` trong catalog) | Dùng khi | Ví dụ |
|---|---|---|
| `flatpak` | Flathub có bản chính chủ (verified) | ONLYOFFICE, LibreOffice, Brave, Telegram |
| `recipe` | Phải dùng gói chính hãng của nhà phát hành, hoặc Flathub chỉ có bản cộng đồng/cũ | WPS, Chrome, Edge, Cốc Cốc, Zoom, Zalo bản gọi được |
| `apt` | Gói trong kho Ubuntu/Mint là đủ | VLC |
| `preinstalled` | Có sẵn trong CaramOS, chỉ để hiển thị | Firefox |

Flatpak được cài cho toàn hệ thống (mọi user), qua libflatpak để có % tiến độ thật. App Flatpak gõ tiếng Việt bằng Lotus bình thường và thấy font của hệ thống (đã thử ONLYOFFICE, LibreOffice trên CaramOS 1.0.19).

## Bảo mật

```
caramos-app-center (user)  ──pkexec──>  /usr/lib/caramos-app-center/caramos-app-center-helper (root)
        chỉ gửi ID ứng dụng                   tra cách cài trong /usr/share/caramos-app-center/catalog.json
        đọc tiến độ dạng JSON lines           chạy flatpak / apt / recipe, in sự kiện ra stdout
```

- Helper chỉ nhận ID khớp `^[a-z0-9][a-z0-9-]{0,40}$` và tra mọi thứ trong catalog của gói (thuộc root). Không có lệnh hay URL nào đi từ phía user sang root.
- Polkit action `net.vietnamlinuxfamily.caramos-app-center.manage` (`auth_admin_keep`): hỏi mật khẩu một lần cho cả lượt cài.
- Công thức chỉ tải qua HTTPS; Zalo được đối chiếu sha256 GitHub công bố. Test `test_app_center_packaging` chặn URL `http://`.
- Biến `CARAMOS_APP_CENTER_DATA` và `CARAMOS_APP_CENTER_FAKE_HELPER` chỉ để chạy thử giao diện; helper root không đọc biến môi trường nào.

## Thêm một ứng dụng

1. Thêm mục vào `usr/share/caramos-app-center/catalog.json`:
   ```json
   {
     "id": "ten-app",
     "name": "Tên hiển thị",
     "category": "tools",
     "icon": "ten-app.png",
     "summary": {"vi": "...", "en": "..."},
     "pros": {"vi": ["..."], "en": ["..."]},
     "cons": {"vi": ["..."], "en": ["..."]},
     "size_mb": 300,
     "website": "https://...",
     "install": {"flatpak": "org.example.App"}
   }
   ```
   - `badge` (không bắt buộc): `recommended` hoặc `popular`.
   - `detect` bắt buộc với `recipe` và `preinstalled`: `{"deb": "ten-goi"}` hoặc `{"zalo_flavour": "full"}`.
   - `size_mb`: dung lượng sau khi cài (ước lượng). Runtime Flathub dùng chung tính riêng (`FLATPAK_RUNTIME_MB` trong `plan.py`).
2. Thêm icon PNG 128×128 vào `usr/share/caramos-app-center/icons/` (với app Flathub, lấy từ `https://flathub.org/api/v2/appstream/<id>` trường `icon`), hoặc đặt `icon_name` theo theme icon.
3. Muốn hiện ở trình chọn lần đầu: thêm ID vào `apps` của một bước trong `steps` (`default` là các mục được chọn sẵn).
4. Chạy `make test`, rồi `make run` để xem thẻ mới trong trình chọn.

## Viết công thức cài (`recipe`)

File thực thi trong `usr/share/caramos-app-center/recipes/<tên>`, nhận đúng một tham số `install` hoặc `remove`, chạy bằng root.

```bash
#!/bin/bash
. /usr/share/caramos-app-center/recipes/lib/common.sh
require_action "${1:-}"

if [ "$1" = remove ]; then
    remove_packages ten-goi
    exit 0
fi

download "https://example.com/app.deb" "$WORK_DIR/app.deb"   # báo tiến độ 0-80 %
install_deb "$WORK_DIR/app.deb"                              # 80-100 %
```

- Dòng `PROGRESS <0-100>` trên stdout thành thanh tiến độ; các dòng khác vào nhật ký.
- Thoát khác 0 là báo lỗi cho app đó; các app còn lại vẫn cài tiếp.
- Ưu tiên gói `.deb` tự thêm kho apt của hãng (Chrome, Edge, Cốc Cốc) để app tự cập nhật cùng hệ thống.

## Lần đầu đăng nhập

- `etc/xdg/autostart/caramos-app-center-first-run.desktop` chạy `caramos-app-center --first-run`.
- Chỉ mở khi có `/etc/caramos-app-center/first-run` (ISO tạo qua overlay `config/includes.chroot`), không phải live session (`boot=casper`), uid ≥ 1000 và user chưa có `~/.config/caramos-app-center/first-run-done`.
- Máy nhận App Center qua OTA không có file cờ nên chỉ thấy mục trong menu.
- "Để sau" hoặc không có mạng: không ghi dấu, lần đăng nhập sau hỏi lại. Cài xong hoặc bấm "Hoàn tất": ghi dấu.

## Chạy thử trên VM

Cài `.deb` vào VM rồi mở **trong phiên desktop** (Alt+F2 → `caramos-app-center --first-run`). Mở qua SSH thì `pkexec` không có polkit agent nên báo huỷ xác thực ngay. Tạo cờ để thử trình chọn lần đầu:

```bash
sudo mkdir -p /etc/caramos-app-center && sudo touch /etc/caramos-app-center/first-run
rm -f ~/.config/caramos-app-center/first-run-done
```
