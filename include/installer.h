#pragma once
#include "types.h"
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <functional>

struct InstallResult {
    std::string app_id;
    bool        success{false};
    std::string message;
};

using ProgressCallback = std::function<void(const std::string& app_id, double fraction, const std::string& status)>;
using DoneCallback     = std::function<void(const std::vector<InstallResult>&)>;

class Installer {
public:
    Installer(LogCallback log_cb, ProgressCallback prog_cb, DoneCallback done_cb);
    ~Installer();

    // Bắt đầu cài đặt các app được chọn (chạy thread riêng)
    void start(const std::vector<AppInfo>& apps);
    void cancel();
    bool is_running() const { return running_; }

private:
    void run_install(const std::vector<AppInfo> apps);
    bool run_script(const AppInfo& app);

    LogCallback      log_cb_;
    ProgressCallback prog_cb_;
    DoneCallback     done_cb_;

    std::thread      worker_;
    std::atomic_bool running_{false};
    std::atomic_bool cancel_{false};
};
