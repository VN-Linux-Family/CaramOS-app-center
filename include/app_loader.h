#pragma once
#include "types.h"
#include <vector>
#include <string>

// ============================================================
//  AppLoader đọc toàn bộ file .sh trong SCRIPTS_DIR,
//  parse metadata từ các comment đặc biệt đầu file, và
//  trả về danh sách AppInfo.
//
//  Cú pháp metadata trong script:
//    # @name        Tên phần mềm
//    # @description Mô tả ngắn
//    # @version     1.0.0
//    # @category    development|office|multimedia|internet|utilities|games
//    # @icon        gtk-icon-name
// ============================================================
class AppLoader {
public:
    AppLoader();

    // Tải danh sách app từ SCRIPTS_DIR
    std::vector<AppInfo> load_all();

    // Kiểm tra app đã được cài chưa (file marker)
    bool is_installed(const std::string& app_id) const;

    // Đường dẫn thư mục scripts
    static std::string scripts_dir();

private:
    std::string parse_meta(const std::string& content, const std::string& key) const;
    AppInfo     parse_script(const std::string& path) const;
};
