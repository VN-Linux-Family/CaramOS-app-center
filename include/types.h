#pragma once
#include <string>
#include <vector>
#include <functional>

// ============================================================
//  Đường dẫn thư mục chứa tất cả script cài đặt phần mềm
// ============================================================
#define SCRIPTS_DIR  "./scripts/apps"

// ============================================================
//  Phiên bản ứng dụng
// ============================================================
#define APP_VERSION  "1.0.0"
#define APP_NAME     "Ubuntu Store"

enum class Theme {
    LIGHT,
    DARK,
    CUSTOM
};

enum class Category {
    ALL,
    DEVELOPMENT,
    OFFICE,
    MULTIMEDIA,
    INTERNET,
    UTILITIES,
    GAMES
};

struct CustomThemeColors {
    std::string bg_primary    = "#1a1a2e";
    std::string bg_secondary  = "#16213e";
    std::string bg_card       = "#0f3460";
    std::string accent        = "#e94560";
    std::string text_primary  = "#eaeaea";
    std::string text_secondary = "#a0a0b0";
    std::string border        = "#2a2a4a";
    std::string success       = "#4caf50";
    std::string warning       = "#ff9800";
    std::string button_bg     = "#e94560";
    std::string button_text   = "#ffffff";
};

struct AppInfo {
    std::string id;              // ID duy nhất, trùng tên file script (không có .sh)
    std::string name;            // Tên hiển thị
    std::string description;     // Mô tả ngắn
    std::string version;         // Phiên bản
    std::string category;        // Danh mục
    std::string icon_name;       // Tên icon GTK
    std::string script_path;     // Đường dẫn đầy đủ tới script
    bool        selected  = false;
    bool        installed = false;
};

// Callback khi log output
using LogCallback = std::function<void(const std::string&, bool is_error)>;
