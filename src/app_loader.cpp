#include "app_loader.h"
#include "types.h"
#include <glib.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>

namespace fs = std::filesystem;

AppLoader::AppLoader() {}

std::string AppLoader::scripts_dir() {
    return SCRIPTS_DIR;
}

std::string AppLoader::parse_meta(const std::string& content, const std::string& key) const {
    std::string tag = "# @" + key;
    std::istringstream ss(content);
    std::string line;
    while (std::getline(ss, line)) {
        // Chỉ đọc trong phần header (các dòng bắt đầu bằng #)
        if (line.empty()) continue;
        if (line[0] != '#') break;
        if (line.rfind(tag, 0) == 0) {
            std::string val = line.substr(tag.size());
            // Trim leading spaces
            size_t start = val.find_first_not_of(" \t");
            if (start == std::string::npos) return "";
            size_t end = val.find_last_not_of(" \t\r\n");
            return val.substr(start, end - start + 1);
        }
    }
    return "";
}

AppInfo AppLoader::parse_script(const std::string& path) const {
    AppInfo info;
    info.script_path = path;

    // ID = tên file không có .sh
    fs::path p(path);
    info.id = p.stem().string();

    // Đọc nội dung file
    std::ifstream f(path);
    if (!f.is_open()) return info;
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    info.name        = parse_meta(content, "name");
    info.description = parse_meta(content, "description");
    info.version     = parse_meta(content, "version");
    info.category    = parse_meta(content, "category");
    info.icon_name   = parse_meta(content, "icon");

    // Fallback nếu thiếu metadata
    if (info.name.empty())     info.name     = info.id;
    if (info.version.empty())  info.version  = "N/A";
    if (info.category.empty()) info.category = "utilities";
    if (info.icon_name.empty()) info.icon_name = "application-x-executable";

    info.installed = is_installed(info.id);

    return info;
}

std::vector<AppInfo> AppLoader::load_all() {
    std::vector<AppInfo> result;
    std::string dir = scripts_dir();

    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        g_warning("Scripts directory not found: %s", dir.c_str());
        return result;
    }

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".sh") continue;

        AppInfo info = parse_script(entry.path().string());
        if (!info.id.empty()) result.push_back(info);
    }

    // Sắp xếp theo tên
    std::sort(result.begin(), result.end(),
              [](const AppInfo& a, const AppInfo& b) { return a.name < b.name; });

    return result;
}

bool AppLoader::is_installed(const std::string& app_id) const {
    // Marker file tại /tmp/ubuntu-store-installed/<id>
    std::string marker = "/tmp/ubuntu-store-installed/" + app_id;
    return fs::exists(marker);
}
