#include "installer.h"
#include <cstdio>
#include <array>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

Installer::Installer(LogCallback log_cb, ProgressCallback prog_cb, DoneCallback done_cb)
    : log_cb_(log_cb), prog_cb_(prog_cb), done_cb_(done_cb) {}

Installer::~Installer() {
    cancel();
    if (worker_.joinable()) worker_.join();
}

void Installer::start(const std::vector<AppInfo>& apps) {
    if (running_) return;
    cancel_ = false;
    running_ = true;
    // Copy danh sách để thread giữ lại
    std::vector<AppInfo> selected;
    for (const auto& a : apps)
        if (a.selected) selected.push_back(a);

    worker_ = std::thread(&Installer::run_install, this, selected);
}

void Installer::cancel() {
    cancel_ = true;
}

void Installer::run_install(const std::vector<AppInfo> apps) {
    std::vector<InstallResult> results;

    for (size_t i = 0; i < apps.size(); ++i) {
        if (cancel_) {
            log_cb_("⚠ Hủy cài đặt bởi người dùng.", true);
            break;
        }

        const AppInfo& app = apps[i];
        log_cb_("▶ Đang cài đặt: " + app.name + " ...", false);
        prog_cb_(app.id, 0.0, "Đang cài đặt...");

        bool ok = run_script(app);

        InstallResult r;
        r.app_id  = app.id;
        r.success = ok;
        r.message = ok ? "Cài đặt thành công" : "Cài đặt thất bại";

        if (ok) {
            log_cb_("✔ " + app.name + " – Hoàn tất!", false);
            // Tạo marker
            fs::create_directories("/tmp/ubuntu-store-installed");
            std::ofstream("/tmp/ubuntu-store-installed/" + app.id);
        } else {
            log_cb_("✘ " + app.name + " – Lỗi!", true);
        }

        prog_cb_(app.id, ok ? 1.0 : -1.0, r.message);
        results.push_back(r);
    }

    running_ = false;
    done_cb_(results);
}

bool Installer::run_script(const AppInfo& app) {
    // Kiểm tra file script tồn tại
    if (!fs::exists(app.script_path)) {
        log_cb_("✘ Không tìm thấy script: " + app.script_path, true);
        return false;
    }

    // Chạy script với bash, bắt stdout + stderr
    std::string cmd = "bash \"" + app.script_path + "\" 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        log_cb_("✘ Không thể chạy script.", true);
        return false;
    }

    std::array<char, 256> buf;
    while (!cancel_ && fgets(buf.data(), buf.size(), pipe)) {
        std::string line(buf.data());
        // Bỏ newline cuối
        if (!line.empty() && line.back() == '\n') line.pop_back();
        log_cb_("  " + line, false);
    }

    int ret = pclose(pipe);
    return (ret == 0) && !cancel_;
}
