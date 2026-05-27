#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// App Explorer — PackageManager
// Xử lý đọc .aepkg, cài đặt, gỡ cài đặt, trạng thái
// ─────────────────────────────────────────────────────────────────────────────

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <optional>
#include <atomic>
#include <thread>
#include <mutex>

namespace AppExplorer {

// ─────────────────────────────────────────────────────────────────────────────
// Enums
// ─────────────────────────────────────────────────────────────────────────────

enum class InstallMethod {
    APT, DNF, PACMAN, FLATPAK, SNAP,
    SCRIPT, APPIMAGE, DEB, RPM, TAR,
    UNKNOWN
};

enum class AppCategory {
    BROWSER, EDITOR, MEDIA, DEV, GAME,
    UTIL, SYS, NET, GRAPHIC, OFFICE, OTHER
};

enum class InstallStatus {
    NOT_INSTALLED,
    INSTALLED,
    INSTALLING,
    UNINSTALLING,
    ERROR,
    UNKNOWN
};

enum class OperationType {
    INSTALL,
    UNINSTALL
};

// ─────────────────────────────────────────────────────────────────────────────
// AppPackage — Dữ liệu đầy đủ của một ứng dụng từ .aepkg
// ─────────────────────────────────────────────────────────────────────────────

struct AppPackage {
    // [meta]
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::string long_desc;
    std::string usage;
    AppCategory category         = AppCategory::OTHER;
    std::string category_str;
    std::vector<std::string> tags;
    std::string author;
    std::string maintainer;
    std::string license;
    std::string homepage;
    std::string source_url;
    std::vector<std::string> arch;
    std::vector<std::string> os;
    std::string min_os_ver;

    // [source]
    std::string icon_online;
    std::string icon_offline;
    std::string icon_size        = "64x64";
    std::string icon_format      = "png";
    std::vector<std::string> screenshot_online;
    std::vector<std::string> screenshot_offline;
    std::string pkg_online;
    std::string pkg_offline;

    // [install]
    InstallMethod install_method = InstallMethod::UNKNOWN;
    std::vector<std::string> packages;
    std::string flatpak_id;
    std::string snap_name;
    std::string install_script;
    std::string install_script_file;
    std::vector<std::string> deps;
    bool sudo_required           = true;
    bool interactive             = false;
    std::string estimated_size;
    std::string download_size;
    std::string checksum_type;
    std::string checksum;
    std::string repo_key_url;
    std::string repo_entry;

    // [uninstall]
    InstallMethod uninstall_method = InstallMethod::UNKNOWN;
    std::vector<std::string> uninstall_packages;
    bool purge                   = false;
    std::string uninstall_script;
    std::string uninstall_script_file;
    bool remove_deps             = false;
    bool remove_data             = false;
    std::vector<std::string> data_paths;

    // [verify]
    std::string verify_binary;
    std::string verify_cmd;
    std::string verify_file;
    std::string verify_service;

    // [hooks]
    std::string hook_pre_install;
    std::string hook_post_install;
    std::string hook_pre_uninstall;
    std::string hook_post_uninstall;

    // [desktop]
    std::string desktop_name;
    std::string desktop_exec;
    std::string desktop_icon;
    std::string desktop_cats;
    bool create_desktop          = true;

    // Runtime state (không từ file)
    InstallStatus status         = InstallStatus::UNKNOWN;
    std::string pkg_file_path;   // Đường dẫn file .aepkg nguồn

    // ── Source resolution helpers ─────────────────────────────────
    // Quy tắc: offline > online, trống = bỏ qua
    std::string resolvedIconPath() const {
        if (!icon_offline.empty()) return icon_offline;
        if (!icon_online.empty())  return icon_online;
        return "";
    }
    bool iconIsOnline() const {
        return icon_offline.empty() && !icon_online.empty();
    }

    std::string resolvedPkgPath() const {
        if (!pkg_offline.empty()) return pkg_offline;
        if (!pkg_online.empty())  return pkg_online;
        return "";
    }
    bool pkgIsOnline() const {
        return pkg_offline.empty() && !pkg_online.empty();
    }

    // Screenshot paths resolved (offline ưu tiên per-index)
    std::vector<std::string> resolvedScreenshots() const {
        if (!screenshot_offline.empty()) return screenshot_offline;
        return screenshot_online;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// AepkgParser — Đọc file .aepkg → AppPackage
// ─────────────────────────────────────────────────────────────────────────────

class AepkgParser {
public:
    AepkgParser();
    ~AepkgParser();

    bool parseFile(const std::string& path, AppPackage& out_pkg);
    bool parseString(const std::string& content, AppPackage& out_pkg);

    const std::string& lastError() const { return last_error_; }

private:
    using IniMap = std::unordered_map<std::string,
                   std::unordered_map<std::string, std::string>>;

    bool            parseIni(const std::string& content, IniMap& out);
    void            populate(const IniMap& ini, AppPackage& pkg);
    InstallMethod   parseMethod(const std::string& s) const;
    AppCategory     parseCategory(const std::string& s) const;
    std::vector<std::string> splitList(const std::string& s, char delim = ';') const;

    std::string     getString(const IniMap&, const std::string& sec,
                              const std::string& key, const std::string& def = "") const;
    bool            getBool(const IniMap&, const std::string& sec,
                            const std::string& key, bool def = false) const;

    std::string last_error_;
};

// ─────────────────────────────────────────────────────────────────────────────
// OperationLog — Dòng log từ một tiến trình cài/gỡ
// ─────────────────────────────────────────────────────────────────────────────

enum class LogLevel { CMD, STDOUT, STDERR, SUCCESS, ERROR, INFO };

struct LogLine {
    LogLevel    level;
    std::string text;
    // Timestamp seconds since epoch
    long long   timestamp;
};

// ─────────────────────────────────────────────────────────────────────────────
// OperationContext — Trạng thái một operation đang chạy
// ─────────────────────────────────────────────────────────────────────────────

struct OperationContext {
    std::string     app_id;
    OperationType   type;
    std::atomic<bool> running{false};
    std::atomic<bool> finished{false};
    std::atomic<bool> success{false};
    std::atomic<int>  exit_code{-1};

    std::vector<LogLine>    log_lines;
    mutable std::mutex      log_mutex;

    std::string             log_file_path;   // Đường dẫn file log (chỉ ghi khi lỗi)

    // Callback mỗi khi có dòng log mới (gọi từ main thread qua g_idle_add)
    std::function<void(const LogLine&)>  on_log_line;
    // Callback khi hoàn thành
    std::function<void(bool success, int exit_code)> on_finished;

    void appendLog(LogLevel level, const std::string& text);
};

// ─────────────────────────────────────────────────────────────────────────────
// PackageRepository — Tập hợp các AppPackage từ thư mục
// ─────────────────────────────────────────────────────────────────────────────

class PackageRepository {
public:
    PackageRepository();
    ~PackageRepository();

    // Load tất cả .aepkg trong thư mục
    bool loadDirectory(const std::string& dir_path);

    // Load thêm .aepkg từ URL (danh sách) — download về cache
    bool loadRemoteIndex(const std::string& index_url, const std::string& cache_dir);

    // Lấy tất cả packages
    const std::vector<AppPackage>& allPackages() const { return packages_; }

    // Tìm theo id
    const AppPackage* findById(const std::string& id) const;

    // Tìm theo category
    std::vector<const AppPackage*> findByCategory(AppCategory cat) const;

    // Tìm kiếm text (name, desc, tags)
    std::vector<const AppPackage*> search(const std::string& query) const;

    // Số lượng packages
    size_t count() const { return packages_.size(); }

    // Reload
    void reload();

    const std::string& lastError() const { return last_error_; }

private:
    std::vector<AppPackage> packages_;
    std::string             loaded_dir_;
    std::string             last_error_;

    bool loadSingleFile(const std::string& path);
};

// ─────────────────────────────────────────────────────────────────────────────
// InstallManager — Thực thi cài/gỡ
// ─────────────────────────────────────────────────────────────────────────────

class InstallManager {
public:
    static InstallManager& instance();

    // Kiểm tra trạng thái cài đặt (chạy verify command/binary)
    InstallStatus checkStatus(const AppPackage& pkg);

    // Cài đặt (async, chạy trong thread riêng)
    // Trả về false nếu đang có operation chạy với app này
    bool install(const AppPackage& pkg,
                 std::function<void(const LogLine&)> on_log,
                 std::function<void(bool, int)>      on_done);

    // Gỡ cài đặt (async)
    bool uninstall(const AppPackage& pkg,
                   std::function<void(const LogLine&)> on_log,
                   std::function<void(bool, int)>      on_done);

    // Kiểm tra có operation đang chạy không
    bool isRunning(const std::string& app_id) const;

    // Dừng operation (gửi SIGTERM)
    void cancel(const std::string& app_id);

    // Kiểm tra binary tồn tại trên PATH
    static bool commandExists(const std::string& cmd);

    // Detect package manager có trên hệ thống
    static InstallMethod detectSystemPM();

private:
    InstallManager() = default;

    void runOperation(std::shared_ptr<OperationContext> ctx,
                      const std::vector<std::string>& cmd_args,
                      const std::string& working_dir = "");

    void buildInstallCmd(const AppPackage& pkg,
                         std::vector<std::string>& out_cmds);
    void buildUninstallCmd(const AppPackage& pkg,
                           std::vector<std::string>& out_cmds);

    std::string expandHomePath(const std::string& path) const;
    bool        downloadFile(const std::string& url,
                             const std::string& dest_path) const;
    bool        verifyChecksum(const std::string& file_path,
                               const std::string& type,
                               const std::string& expected) const;

    mutable std::mutex                                          ops_mutex_;
    std::unordered_map<std::string,
        std::shared_ptr<OperationContext>>                      active_ops_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Logger — Ghi log cho App Explorer
// ─────────────────────────────────────────────────────────────────────────────

enum class AppLogLevel { DEBUG, INFO, WARNING, ERROR };

class Logger {
public:
    static Logger& instance();

    // Khởi tạo với thư mục log
    void init(const std::string& log_dir);

    // Log GUI (luôn ghi)
    void logGUI(AppLogLevel level, const std::string& msg);

    // Bắt đầu log cho một operation
    // Trả về path file log tạm
    std::string beginOperationLog(const std::string& app_id, OperationType type);

    // Ghi dòng vào operation log
    void writeOperationLog(const std::string& log_path, const LogLine& line);

    // Kết thúc operation log:
    // - Nếu success=true → xóa file log
    // - Nếu success=false → giữ file log lại
    void finishOperationLog(const std::string& log_path, bool success);

    const std::string& logDir() const { return log_dir_; }

private:
    Logger() = default;
    std::string log_dir_;
    std::string gui_log_path_;
    mutable std::mutex write_mutex_;

    std::string timestamp() const;
    std::string levelStr(AppLogLevel l) const;
};

} // namespace AppExplorer
