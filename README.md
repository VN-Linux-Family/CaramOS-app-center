# App Center CaramOS

App Center (`caramos-app-center`) là **trình chọn ứng dụng**: một trang chia nhóm văn phòng, trình duyệt, liên lạc và tiện ích, bộ đề xuất được chọn sẵn, mỗi ứng dụng ghi rõ ưu và nhược điểm. Ứng dụng đã cài có nút **Gỡ** ngay trên thẻ của nó (hỏi lại trước khi gỡ; ứng dụng có sẵn trong CaramOS như Firefox thì không có). Nó tự mở ở lần đầu (live session từ USB và lần đăng nhập đầu sau khi cài), và mở bằng biểu tượng trên Desktop hoặc menu (System > App Center CaramOS) bất cứ lúc nào.

App Center chỉ gợi ý các ứng dụng cơ bản. Ứng dụng khác nằm trên [apps.vietnamlinuxfamily.net](https://apps.vietnamlinuxfamily.net/): nút "Xem thêm" của từng nhóm mở đúng danh mục trên trang đó, dòng cuối trang mở trang chính.

Trang danh sách ứng dụng có ô tìm kiếm (`ui/store.py`) đang tạm ẩn: sau này nó lấy danh sách từ API của apps.vietnamlinuxfamily.net. Lập trình viên vẫn mở được bằng `caramos-app-center --store`.

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

App Center có phiên bản riêng (không đi theo phiên bản CaramOS) và **không lên PPA**: mỗi bản là một GitHub Release.

```bash
make changelog VERSION=0.2.0    # mở mục mới trong debian/changelog, rồi ghi các thay đổi vào đó
git commit -am "Release 0.2.0"
make release VERSION=0.2.0      # test, build, gắn tag v0.2.0
git push origin HEAD v0.2.0     # GitHub Actions build, ký và đăng release
```

Workflow `.github/workflows/release.yml` chạy khi có tag `vX.Y.Z` (phải trùng phiên bản trong `debian/changelog`): test, build `caramos-app-center_X.Y.Z_all.deb`, viết `latest.json` (phiên bản, tên file, SHA-256) bằng `tools/release-manifest.py`, ký thành `latest.json.sig` rồi đăng cả ba lên release.

Máy CaramOS tự nhận bản mới trong vòng một ngày: lần kiểm tra OTA hằng ngày (`caramos-ota --check`, chạy bằng root) tải `latest.json` của release mới nhất, kiểm tra chữ ký bằng chìa công khai có sẵn trong `caramos-ota` (`/usr/share/caramos-ota/keys/caramos-app-center-release.gpg`), rồi cài `.deb` khi SHA-256 khớp và phiên bản mới hơn bản đang cài. Thêm hay sửa ứng dụng gợi ý chỉ cần release App Center, không cần ra bản CaramOS mới.

### Chìa ký

- Chìa riêng cho App Center (Ed25519, chỉ dùng để ký), fingerprint `9326538C708A3A6533D98EAFCC6853D8DA2155B6`. Phần công khai: `keys/caramos-app-center-release.asc`.
- Phần bí mật nằm trong secret `APP_CENTER_SIGNING_KEY` của repo (Settings > Secrets and variables > Actions). Thiếu secret thì workflow dừng, không đăng release.
- Đổi chìa: tạo chìa mới, cập nhật secret, `keys/` ở đây và `usr/share/caramos-ota/keys/` trong `caramos-ota`, rồi ra bản OTA trước khi phát hành App Center bằng chìa mới.

## Trong CaramOS

- Repo này là submodule `packages/caramos-app-center` của [CaramOS](https://github.com/VN-Linux-Family/CaramOS). Build ISO dựng gói từ submodule và cài vào rootfs.
- ISO tạo `/etc/caramos-app-center/first-run` (overlay `config/includes.chroot`) để bật trình chọn ứng dụng lần đầu.
- Máy đã cài nhận App Center qua OTA: migration `20261009120000_add_app_center_launcher` của `caramos-ota` cài bản mới nhất từ GitHub Release (nếu máy chưa có) và đặt biểu tượng lên Desktop. Từ đó lần kiểm tra OTA hằng ngày giữ nó luôn mới. Trên máy này trình chọn không tự bật lần đầu; người dùng mở bằng biểu tượng Desktop hoặc menu.

## Cài ứng dụng như thế nào

| Cách cài (`install` trong catalog) | Dùng khi | Ví dụ |
|---|---|---|
| `flatpak` | Flathub có bản chính chủ (verified) | ONLYOFFICE, LibreOffice, Brave, Telegram |
| `recipe` | Phải dùng gói chính hãng của nhà phát hành, hoặc Flathub chỉ có bản cộng đồng/cũ | WPS, Chrome, Edge, Cốc Cốc, Zoom, Zalo |
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
- Chỉ mở khi có `/etc/caramos-app-center/first-run` (ISO tạo qua overlay `config/includes.chroot`) và user chưa có `~/.config/caramos-app-center/first-run-done`: trong live session từ USB (user `caram`, home nằm trong RAM nên mỗi lần boot USB đều hiện) và ở lần đăng nhập đầu sau khi cài (uid ≥ 1000). Trong live session trang chọn nhắc ứng dụng cài lúc đó sẽ mất khi tắt máy.
- Khi đóng trình chọn (nút "Để sau" hoặc nút X), App Center nhắc cách mở lại: biểu tượng trên Desktop hoặc menu Start.
- Máy nhận App Center qua OTA không có file cờ nên chỉ thấy mục trong menu.
- "Để sau" hoặc không có mạng: không ghi dấu, lần đăng nhập sau hỏi lại. Cài xong hoặc bấm "Hoàn tất": ghi dấu.

## Chạy thử trên VM

Cài `.deb` vào VM rồi mở **trong phiên desktop** (Alt+F2 → `caramos-app-center --first-run`). Mở qua SSH thì `pkexec` không có polkit agent nên báo huỷ xác thực ngay. Tạo cờ để thử trình chọn lần đầu:

```bash
sudo mkdir -p /etc/caramos-app-center && sudo touch /etc/caramos-app-center/first-run
rm -f ~/.config/caramos-app-center/first-run-done
```
