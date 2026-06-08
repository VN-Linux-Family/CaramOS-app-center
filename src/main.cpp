/*
 * VNLF App Store - GTK3 C++ Application
 * Kho ứng dụng Linux tiếng Việt
 * Dựa trên giao diện VNLF App Explorer
 */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <fontconfig/fontconfig.h>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <map>
#include <memory>
#include <iostream>

// ─── Font loader ────────────────────────────────────────────────────────────
// Đăng ký tất cả file .ttf/.otf trong thư mục fonts/ cạnh binary với
// Fontconfig. Sau đó Pango/GTK có thể dùng font theo tên family.

static std::string g_fonts_dir;  // set từ main()

static void load_app_fonts() {
    if (g_fonts_dir.empty()) return;

    // Khởi tạo fontconfig nếu chưa
    FcInit();

    FcConfig *cfg = FcConfigGetCurrent();

    // Thêm toàn bộ thư mục vào app font path
    if (FcConfigAppFontAddDir(cfg,
            reinterpret_cast<const FcChar8*>(g_fonts_dir.c_str()))) {
        std::cout << "[Font] Đã đăng ký thư mục: " << g_fonts_dir << "\n";
    } else {
        std::cerr << "[Font] Không đăng ký được thư mục: " << g_fonts_dir << "\n";
        // Fallback: đăng ký từng file riêng lẻ
        GError *gerr = nullptr;
        GDir *dir = g_dir_open(g_fonts_dir.c_str(), 0, &gerr);
        if (dir) {
            const gchar *fname;
            while ((fname = g_dir_read_name(dir)) != nullptr) {
                std::string fn(fname);
                bool is_font = (fn.size() > 4) &&
                    (fn.substr(fn.size()-4) == ".ttf" ||
                     fn.substr(fn.size()-4) == ".otf");
                if (is_font) {
                    std::string path = g_fonts_dir + "/" + fn;
                    FcConfigAppFontAddFile(cfg,
                        reinterpret_cast<const FcChar8*>(path.c_str()));
                    std::cout << "[Font] Đã đăng ký: " << fn << "\n";
                }
            }
            g_dir_close(dir);
        }
        if (gerr) g_error_free(gerr);
    }

    // Build pattern cache
    FcConfigBuildFonts(cfg);
}


// ─── JSON parser đơn giản không dùng thư viện ngoài ───────────────────────

struct AppInfo {
    std::string id;
    std::string name;
    std::string summary;
    std::string description;
    std::string category;
    std::string icon;
    std::string version;
    std::string author;
    std::string license;
    std::string website;
    std::string script;
    std::vector<std::string> tags;
};

static std::string json_get_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos);
    if (pos == std::string::npos) return "";
    size_t start = pos + 1;
    size_t end = json.find('"', start);
    while (end != std::string::npos && json[end-1] == '\\') {
        end = json.find('"', end + 1);
    }
    if (end == std::string::npos) return "";
    std::string val = json.substr(start, end - start);
    // unescape
    std::string result;
    for (size_t i = 0; i < val.size(); i++) {
        if (val[i] == '\\' && i+1 < val.size()) {
            switch (val[i+1]) {
                case 'n': result += '\n'; i++; break;
                case 't': result += '\t'; i++; break;
                case '"': result += '"'; i++; break;
                case '\\': result += '\\'; i++; break;
                default: result += val[i];
            }
        } else {
            result += val[i];
        }
    }
    return result;
}

static std::vector<std::string> json_get_array(const std::string& json, const std::string& key) {
    std::vector<std::string> result;
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return result;
    pos = json.find('[', pos);
    if (pos == std::string::npos) return result;
    size_t end = json.find(']', pos);
    if (end == std::string::npos) return result;
    std::string arr = json.substr(pos+1, end-pos-1);
    size_t p = 0;
    while (p < arr.size()) {
        size_t s = arr.find('"', p);
        if (s == std::string::npos) break;
        size_t e = arr.find('"', s+1);
        if (e == std::string::npos) break;
        result.push_back(arr.substr(s+1, e-s-1));
        p = e + 1;
    }
    return result;
}

// ─── Parse one JSON object into AppInfo ────────────────────────────────────

static AppInfo parse_app_object(const std::string& obj) {
    AppInfo app;
    app.id          = json_get_string(obj, "id");
    app.name        = json_get_string(obj, "name");
    app.summary     = json_get_string(obj, "summary");
    app.description = json_get_string(obj, "description");
    app.category    = json_get_string(obj, "category");
    app.icon        = json_get_string(obj, "icon");
    app.version     = json_get_string(obj, "version");
    app.author      = json_get_string(obj, "author");
    app.license     = json_get_string(obj, "license");
    app.website     = json_get_string(obj, "website");
    app.script      = json_get_string(obj, "script");
    app.tags        = json_get_array(obj, "tags");
    return app;
}

// Serialize AppInfo back to a JSON object string
static std::string app_to_json(const AppInfo& app) {
    auto esc = [](const std::string& s) {
        std::string r;
        for (char c : s) {
            if (c == '"')  r += "\\\"";
            else if (c == '\\') r += "\\\\";
            else if (c == '\n') r += "\\n";
            else r += c;
        }
        return r;
    };
    std::string j = "{\n";
    j += "  \"id\": \""          + esc(app.id)          + "\",\n";
    j += "  \"name\": \""        + esc(app.name)        + "\",\n";
    j += "  \"summary\": \""     + esc(app.summary)     + "\",\n";
    j += "  \"description\": \"" + esc(app.description) + "\",\n";
    j += "  \"category\": \""    + esc(app.category)    + "\",\n";
    j += "  \"icon\": \""        + esc(app.icon)        + "\",\n";
    j += "  \"version\": \""     + esc(app.version)     + "\",\n";
    j += "  \"author\": \""      + esc(app.author)      + "\",\n";
    j += "  \"license\": \""     + esc(app.license)     + "\",\n";
    j += "  \"website\": \""     + esc(app.website)     + "\",\n";
    j += "  \"script\": \""      + esc(app.script)      + "\",\n";
    j += "  \"tags\": [";
    for (size_t i = 0; i < app.tags.size(); i++) {
        j += "\"" + esc(app.tags[i]) + "\"";
        if (i + 1 < app.tags.size()) j += ", ";
    }
    j += "]\n}";
    return j;
}

// ─── Load apps — supports both single apps.json AND per-app JSON files ─────
//
// Layout A (legacy): data/apps.json  — one big array
// Layout B (new):    data/apps/       — one <id>.json per app
//                    data/apps.json   — optional index ["firefox","vlc",...]
//                                       or omitted (auto-scan directory)

static AppInfo parse_single_json_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return AppInfo{};
    std::string content((std::istreambuf_iterator<char>(f)),
                          std::istreambuf_iterator<char>());
    f.close();
    // Find first { ... } block
    size_t s = content.find('{');
    if (s == std::string::npos) return AppInfo{};
    int depth = 0; size_t end = std::string::npos;
    for (size_t i = s; i < content.size(); i++) {
        if (content[i] == '{') depth++;
        else if (content[i] == '}') { depth--; if (depth == 0) { end = i; break; } }
    }
    if (end == std::string::npos) return AppInfo{};
    return parse_app_object(content.substr(s, end - s + 1));
}

static std::vector<AppInfo> parse_apps_json(const std::string& data_dir) {
    std::vector<AppInfo> apps;

    // ── Layout B: per-app JSON files in data/apps/ ──
    std::string apps_subdir = data_dir + "/apps";
    {
        // Check if the directory exists by trying to open a sentinel
        // (No dirent.h usage on minimal env — use system ls)
        // We'll use g_dir from GLib which is always available with GTK
        GError *gerr = nullptr;
        GDir *dir = g_dir_open(apps_subdir.c_str(), 0, &gerr);
        if (dir) {
            // Collect filenames, sort for deterministic order
            std::vector<std::string> files;
            const gchar *fname;
            while ((fname = g_dir_read_name(dir)) != nullptr) {
                std::string fn(fname);
                if (fn.size() > 5 && fn.substr(fn.size()-5) == ".json")
                    files.push_back(fn);
            }
            g_dir_close(dir);
            std::sort(files.begin(), files.end());
            for (auto& fn : files) {
                AppInfo app = parse_single_json_file(apps_subdir + "/" + fn);
                if (!app.id.empty()) apps.push_back(app);
            }
            if (!apps.empty()) return apps;  // prefer per-file layout
        }
        if (gerr) g_error_free(gerr);
    }

    // ── Layout A: single data/apps.json array ──
    std::string filepath = data_dir + "/apps.json";
    std::ifstream f(filepath);
    if (!f.is_open()) {
        std::cerr << "Không thể mở: " << filepath << std::endl;
        return apps;
    }
    std::string content((std::istreambuf_iterator<char>(f)),
                          std::istreambuf_iterator<char>());
    f.close();

    int depth = 0;
    size_t obj_start = std::string::npos;
    for (size_t i = 0; i < content.size(); i++) {
        if (content[i] == '{') {
            if (depth == 0) obj_start = i;
            depth++;
        } else if (content[i] == '}') {
            depth--;
            if (depth == 0 && obj_start != std::string::npos) {
                AppInfo app = parse_app_object(
                    content.substr(obj_start, i - obj_start + 1));
                if (!app.id.empty()) apps.push_back(app);
                obj_start = std::string::npos;
            }
        }
    }
    return apps;
}

// ─── Category mapping ──────────────────────────────────────────────────────

static std::map<std::string, std::string> CATEGORY_LABELS = {
    {"", "Tất cả"},
    {"internet", "Internet"},
    {"multimedia", "Đa phương tiện"},
    {"office", "Văn phòng"},
    {"graphics", "Đồ họa"},
    {"development", "Lập trình"},
    {"system", "Hệ thống"},
    {"games", "Trò chơi"},
    {"education", "Giáo dục"},
    {"utilities", "Tiện ích"},
};

static std::map<std::string, std::string> CATEGORY_ICONS = {
    {"", "view-grid-symbolic"},
    {"internet", "network-wireless-symbolic"},
    {"multimedia", "multimedia-player-symbolic"},
    {"office", "x-office-document-symbolic"},
    {"graphics", "applications-graphics-symbolic"},
    {"development", "edit-copy-symbolic"},
    {"system", "preferences-system-symbolic"},
    {"games", "applications-games-symbolic"},
    {"education", "applications-science-symbolic"},
    {"utilities", "applications-utilities-symbolic"},
};

// ─── Application State ─────────────────────────────────────────────────────

struct AppState {
    std::vector<AppInfo> all_apps;
    std::vector<AppInfo> filtered_apps;
    std::string current_category;
    std::string search_query;
    std::string scripts_dir;
    std::string data_dir;
    std::string icons_dir;   // data/icons/

    // UI elements
    GtkWidget *window;
    GtkWidget *search_entry;
    GtkWidget *apps_grid;
    GtkWidget *apps_scrolled;
    GtkWidget *category_box;
    GtkWidget *header_label;
    GtkWidget *count_label;
    GtkWidget *stack;
    GtkWidget *detail_box;

    // Sidebar — two-mode (collapsed/expanded)
    GtkWidget *sidebar_box;           // the outer sidebar container
    GtkWidget *sidebar_stack;         // switches between "full" and "compact" content
    GtkWidget *sidebar_separator;
    bool       sidebar_collapsed;     // true = icon-only (48px), false = full (200px)

    // Detail view widgets
    GtkWidget *detail_icon;
    GtkWidget *detail_name;
    GtkWidget *detail_version;
    GtkWidget *detail_author;
    GtkWidget *detail_summary;
    GtkWidget *detail_description;
    GtkWidget *detail_category;
    GtkWidget *detail_license;
    GtkWidget *detail_tags;
    GtkWidget *install_btn;
    GtkWidget *install_spinner;
    GtkWidget *install_status;

    AppInfo *current_detail_app;
    bool installing;

    AppState() : sidebar_box(nullptr), sidebar_stack(nullptr),
                 sidebar_separator(nullptr), sidebar_collapsed(false),
                 current_detail_app(nullptr), installing(false) {}
};

static AppState g_state;

// Save a single app back to its per-app JSON file
static bool save_app_json(const AppInfo& app) {
    std::string apps_subdir = g_state.data_dir + "/apps";
    // Create directory if it doesn't exist
    g_mkdir_with_parents(apps_subdir.c_str(), 0755);
    std::string path = apps_subdir + "/" + app.id + ".json";
    std::ofstream f(path);
    if (!f.is_open()) {
        std::cerr << "Cannot write: " << path << std::endl;
        return false;
    }
    f << app_to_json(app);
    f.close();
    std::cout << "Saved: " << path << std::endl;
    return true;
}

// ─── Icon loading (offline PNG first, then GTK theme) ──────────────────────

// Returns a GdkPixbuf* (caller owns ref) or nullptr.
// Searches: data/icons/<id>.png, data/icons/<id>.jpg,
//           data/icons/<icon_name>.png, then falls back to GTK icon.
static GdkPixbuf* load_app_icon_pixbuf(const AppInfo& app, int size) {
    std::vector<std::string> candidates = {
        g_state.icons_dir + "/" + app.id + ".png",
        g_state.icons_dir + "/" + app.id + ".jpg",
        g_state.icons_dir + "/" + app.id + ".svg",
        g_state.icons_dir + "/" + app.icon + ".png",
        g_state.icons_dir + "/" + app.icon + ".jpg",
    };
    for (auto& path : candidates) {
        if (path.find("//") != std::string::npos) continue; // skip empty icon name
        GError *err = nullptr;
        GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(
            path.c_str(), size, size, TRUE, &err);
        if (pb) return pb;
        if (err) g_error_free(err);
    }
    // Fallback: GTK icon theme
    GtkIconTheme *theme = gtk_icon_theme_get_default();
    const char *icon_name = app.icon.empty() ? "application-x-executable"
                                              : app.icon.c_str();
    GError *err = nullptr;
    GdkPixbuf *pb = gtk_icon_theme_load_icon(theme, icon_name, size,
                                              GTK_ICON_LOOKUP_FORCE_SIZE, &err);
    if (!pb) {
        if (err) g_error_free(err);
        pb = gtk_icon_theme_load_icon(theme, "application-x-executable",
                                       size, GTK_ICON_LOOKUP_FORCE_SIZE, nullptr);
    }
    return pb;  // may be nullptr if theme is broken
}

// Helper: create a GtkImage for an app at given pixel size
static GtkWidget* make_app_image(const AppInfo& app, int size) {
    GdkPixbuf *pb = load_app_icon_pixbuf(app, size);
    if (pb) {
        GtkWidget *img = gtk_image_new_from_pixbuf(pb);
        g_object_unref(pb);
        return img;
    }
    // absolute fallback
    GtkWidget *img = gtk_image_new_from_icon_name(
        "application-x-executable", GTK_ICON_SIZE_INVALID);
    gtk_image_set_pixel_size(GTK_IMAGE(img), size);
    return img;
}

// Copy a file from src to dst
static bool copy_file(const std::string& src, const std::string& dst) {
    std::ifstream in(src, std::ios::binary);
    if (!in) return false;
    std::ofstream out(dst, std::ios::binary);
    if (!out) return false;
    out << in.rdbuf();
    return true;
}


// ─── Utilities ─────────────────────────────────────────────────────────────

static std::string utf8_lower(const std::string& s) {
    // Simple ASCII lowercase + preserve UTF-8 multibytes
    std::string result = s;
    for (auto& c : result) {
        if (c >= 'A' && c <= 'Z') c = c + 32;
    }
    return result;
}

static bool app_matches_filter(const AppInfo& app,
                                const std::string& cat,
                                const std::string& query) {
    if (!cat.empty() && app.category != cat) return false;
    if (query.empty()) return true;
    std::string q = utf8_lower(query);
    if (utf8_lower(app.name).find(q) != std::string::npos) return true;
    if (utf8_lower(app.summary).find(q) != std::string::npos) return true;
    if (utf8_lower(app.description).find(q) != std::string::npos) return true;
    for (auto& t : app.tags)
        if (utf8_lower(t).find(q) != std::string::npos) return true;
    return false;
}

static void apply_filter() {
    g_state.filtered_apps.clear();
    for (auto& app : g_state.all_apps) {
        if (app_matches_filter(app, g_state.current_category, g_state.search_query))
            g_state.filtered_apps.push_back(app);
    }
}

// ─── CSS Styling ───────────────────────────────────────────────────────────

static const char* APP_CSS = R"CSS(
* {
    font-family:  "Cantarell", "Noto Sans", sans-serif;
}

window {
    background-color: #f5f3ed;
}

/* ── Custom titlebar ── */
.custom-titlebar {
    background: #3a6146;
    padding: 0 8px;
    min-height: 46px;
    border-bottom: 1px solid #2a4f36;
    box-shadow: 0 2px 6px rgba(0,0,0,0.22);
}

.titlebar-title {
    color: #ffffff;
    font-size: 15px;
    font-weight: bold;
    letter-spacing: 0.3px;
}

.titlebar-subtitle {
    color: rgba(255,255,255,0.7);
    font-size: 10px;
}

/* ── Search box ── */
.search-box {
    background: rgba(255,255,255,1);
    border: 1.5px solid rgba(255,255,255,0.35);
    border-radius: 20px;
    padding: 5px 14px;
    color: #000000;
    caret-color: #000000;
    min-width: 240px;
}
.search-box:focus {
    background: rgba(255,255,255,1);
    border-color: rgba(255,255,255,0.7);
    box-shadow: none;
    outline: none;
    color: #000000;
}

/* ── Sidebar — shared ── */
.sidebar {
    background-color: #2e4e38;
    border-right: 1px solid #1e3828;
}

.sidebar-title {
    font-size: 9px;
    font-weight: bold;
    color: rgba(255,255,255,0.45);
    letter-spacing: 1.6px;
    text-transform: uppercase;
    padding: 14px 14px 6px 14px;
}

/* Expand/collapse toggle at bottom of sidebar */
.sidebar-toggle-btn {
    background: rgba(255,255,255,0.07);
    border: none;
    border-top: 1px solid rgba(255,255,255,0.1);
    border-radius: 0;
    padding: 10px;
    color: rgba(255,255,255,0.6);
    font-size: 16px;
    min-height: 40px;
}
.sidebar-toggle-btn:hover {
    background: rgba(255,255,255,0.13);
    color: white;
}

/* ── Sidebar full mode buttons ── */
.category-btn {
    background: transparent;
    border: none;
    border-radius: 8px;
    padding: 9px 12px;
    margin: 1px 6px;
    text-align: left;
    color: rgba(255,255,255,0.82);
    font-size: 12px;
}
.category-btn:hover  {
    background: rgba(255,255,255,0.1);
    color: white;
}
.category-btn.active {
    background: rgba(255,255,255,0.18);
    color: white;
    font-weight: bold;
}

.category-count {
    font-size: 10px;
    color: rgba(255,255,255,0.45);
    background: rgba(255,255,255,0.1);
    border-radius: 10px;
    padding: 1px 6px;
}

/* ── Sidebar compact mode buttons ── */
.category-btn-compact {
    background: transparent;
    border: none;
    border-radius: 8px;
    padding: 10px 0;
    margin: 1px 4px;
    color: rgba(255,255,255,0.7);
    min-width: 40px;
}
.category-btn-compact:hover {
    background: rgba(255,255,255,0.12);
    color: white;
}
.category-btn-compact.active {
    background: rgba(255,255,255,0.2);
    color: white;
}

/* ── App Cards ── */
.app-card {
    background: #fffffe;
    border-radius: 12px;
    border: 1px solid #e2ddd4;
    padding: 16px;
}
.app-card:hover {
    border-color: #3a6146;
    box-shadow: 0 4px 16px rgba(58,97,70,0.13);
}

.app-name    { font-size: 14px; font-weight: bold; color: #1a2e22; }
.app-summary { font-size: 12px; color: #6b6358; line-height: 1.5; }
.app-version { font-size: 11px; color: #9a9285; }
.app-author  { font-size: 11px; color: #9a9285; }

/* ── Category badges ── */
.cat-badge       { font-size: 10px; border-radius: 6px; padding: 2px 8px; color: white; background-color: #3a6146; }
.cat-internet    { background-color: #00838f; }
.cat-multimedia  { background-color: #ad1457; }
.cat-office      { background-color: #2e7d32; }
.cat-graphics    { background-color: #6a1b9a; }
.cat-development { background-color: #bf360c; }
.cat-system      { background-color: #4e5d6a; }
.cat-games       { background-color: #b71c1c; }
.cat-education   { background-color: #283593; }
.cat-utilities   { background-color: #4e342e; }

/* ── Install button ── */
.install-btn {
    background: linear-gradient(135deg, #3a6146, #2b4e37);
    color: white; border: none; border-radius: 8px;
    padding: 9px 22px; font-size: 13px; font-weight: bold; min-width: 120px;
}
.install-btn:hover {
    background: linear-gradient(135deg, #2e4e38, #1e3828);
    box-shadow: 0 3px 10px rgba(58,97,70,0.4);
}
.install-btn:disabled { background: #ccc; color: #888; }

/* ── Detail view ── */
.detail-header   { background: #fffffe; border-bottom: 1px solid #e2ddd4; }
.detail-name     { font-size: 26px; font-weight: bold; color: #1a2e22; }
.detail-summary  { font-size: 14px; color: #5a5348; margin-top: 4px; }
.detail-description { font-size: 14px; color: #3d3830; line-height: 1.75; }
.detail-meta-label {
    font-size: 10px; font-weight: bold; color: #9a9285;
    text-transform: uppercase; letter-spacing: 0.9px;
}
.detail-meta-value { font-size: 13px; color: #3a3228; margin-top: 2px; }

.tag-chip {
    background: #d4e8da; color: #2b4e37;
    border-radius: 14px; padding: 3px 11px; font-size: 11px; margin: 2px;
}

/* ── Back button (inside detail view, not titlebar) ── */
.back-btn {
    background: transparent;
    border: none;
    color: #3a6146;
    border-radius: 6px;
    padding: 6px 12px;
    font-size: 13px;
    font-weight: bold;
}
.back-btn:hover {
    background: #e8f0eb;
    color: #2b4e37;
}

/* ── Sudo dialog ── */
.sudo-dialog-title    { font-size: 15px; font-weight: bold; color: #1a2e22; }
.sudo-dialog-subtitle { font-size: 12px; color: #6b6358; line-height: 1.5; }
.sudo-password-entry {
    border: 1.5px solid #ccc; border-radius: 8px;
    padding: 8px 12px; font-size: 14px; min-width: 240px;
    background: white; color: #1a1a1a;
}
.sudo-password-entry:focus {
    border-color: #3a6146;
    box-shadow: 0 0 0 2px rgba(58,97,70,0.2);
}
.sudo-error-label { color: #c62828; font-size: 12px; }
.sudo-ok-btn {
    background: linear-gradient(135deg, #3a6146, #2b4e37);
    color: white; border: none; border-radius: 8px; padding: 8px 20px; font-weight: bold;
}
.sudo-ok-btn:hover { background: linear-gradient(135deg, #2e4e38, #1e3828); }
.sudo-cancel-btn {
    background: white; color: #3a3228;
    border: 1px solid #ccc; border-radius: 8px; padding: 8px 16px;
}
.sudo-cancel-btn:hover { background: #f0ece5; }

/* ── Terminal ── */
.install-dialog-title { font-size: 15px; font-weight: bold; color: #1a2e22; }
.terminal-box {
    background: #1a231e; color: #7ee8a2;
    border-radius: 8px; padding: 12px;
    font-family: "FiraCode Nerd Font Mono", "FiraCode Nerd Font", "Fira Code", monospace;
    font-size: 12px;
}
.progress-bar { min-height: 6px; border-radius: 3px; }
.progress-bar progress {
    background: linear-gradient(90deg, #3a6146, #7ee8a2);
    border-radius: 3px;
}

/* ── Misc ── */
.status-bar {
    background: #f0ece5; border-top: 1px solid #ddd8cc;
    padding: 6px 14px; font-size: 11px; color: #7a7264;
}
.empty-state      { color: #9a9285; font-size: 15px; padding: 60px; }
.empty-state-icon { color: #c8c4ba; }

scrolledwindow { border: none; }
scrollbar { background: transparent; border: none; }
scrollbar slider {
    background: rgba(58,97,70,0.18); border-radius: 6px;
    min-width: 6px; min-height: 6px;
}
scrollbar slider:hover { background: rgba(58,97,70,0.35); }
)CSS";


// ─── Forward declarations ──────────────────────────────────────────────────

static void rebuild_apps_grid();
static void show_app_detail(AppInfo* app);
static void show_apps_list();

// ─── Install functionality ─────────────────────────────────────────────────

struct InstallData {
    AppInfo app;
    GtkWidget *dialog;
    GtkWidget *text_view;
    GtkWidget *progress;
    GtkWidget *status_label;
    GtkWidget *close_btn;
    GIOChannel *channel;
    GPid pid;
    bool success;
    bool finished;
    
    InstallData() : dialog(nullptr), text_view(nullptr),
                    progress(nullptr), status_label(nullptr),
                    close_btn(nullptr), channel(nullptr),
                    pid(0), success(false), finished(false) {}
};

static gboolean on_install_output(GIOChannel *channel, GIOCondition cond, gpointer user_data) {
    InstallData *data = (InstallData*)user_data;
    if (cond & G_IO_HUP) {
        // Process finished
        data->finished = true;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(data->progress), 1.0);
        
        if (data->success) {
            gtk_label_set_text(GTK_LABEL(data->status_label), 
                               "✓ Cài đặt thành công!");
            GtkStyleContext *ctx = gtk_widget_get_style_context(data->status_label);
            gtk_style_context_add_class(ctx, "install-success-msg");
        } else {
            gtk_label_set_text(GTK_LABEL(data->status_label),
                               "✗ Cài đặt thất bại!");
        }
        
        gtk_widget_set_sensitive(data->close_btn, TRUE);
        g_io_channel_unref(channel);
        return FALSE;
    }
    
    if (cond & G_IO_IN) {
        gchar *line = nullptr;
        gsize length = 0;
        GError *err = nullptr;
        
        GIOStatus status = g_io_channel_read_line(channel, &line, &length, nullptr, &err);
        if (status == G_IO_STATUS_NORMAL && line) {
            // Check for success marker
            if (strstr(line, "thành công") || strstr(line, "Successfully")) {
                data->success = true;
            }
            
            GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->text_view));
            GtkTextIter end;
            gtk_text_buffer_get_end_iter(buf, &end);
            gtk_text_buffer_insert(buf, &end, line, -1);
            
            // Auto-scroll
            GtkTextMark *mark = gtk_text_buffer_get_mark(buf, "insert");
            gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(data->text_view), mark);
            
            // Pulse progress
            gtk_progress_bar_pulse(GTK_PROGRESS_BAR(data->progress));
            
            g_free(line);
        }
        if (err) g_error_free(err);
    }
    
    return TRUE;
}

static void on_child_exit(GPid pid, gint status, gpointer user_data) {
    InstallData *data = (InstallData*)user_data;
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        data->success = true;
    }
    g_spawn_close_pid(pid);
}

// ─── Sudo Password Dialog ──────────────────────────────────────────────────

struct SudoDialogData {
    GtkWidget *dialog;
    GtkWidget *entry;
    GtkWidget *error_label;
    GtkWidget *ok_btn;
    AppInfo    app;
    std::string script_path;
    bool        has_script;
    bool        confirmed;   // true = user clicked OK
    std::string password;
};

// Forward declaration
static void launch_install_with_password(const AppInfo& app,
                                          const std::string& script_path,
                                          bool has_script,
                                          const std::string& password);

static void on_sudo_ok_clicked(GtkButton*, gpointer user_data) {
    SudoDialogData *d = (SudoDialogData*)user_data;
    d->password  = gtk_entry_get_text(GTK_ENTRY(d->entry));
    d->confirmed = true;
    gtk_widget_destroy(d->dialog);
}

static void on_sudo_cancel_clicked(GtkButton*, gpointer user_data) {
    SudoDialogData *d = (SudoDialogData*)user_data;
    d->confirmed = false;
    gtk_widget_destroy(d->dialog);
}

static void on_sudo_entry_activate(GtkEntry*, gpointer user_data) {
    SudoDialogData *d = (SudoDialogData*)user_data;
    gtk_button_clicked(GTK_BUTTON(d->ok_btn));
}

// Called after the password dialog is destroyed
static void on_sudo_dialog_destroyed(GtkWidget*, gpointer user_data) {
    SudoDialogData *d = (SudoDialogData*)user_data;
    if (d->confirmed && !d->password.empty()) {
        launch_install_with_password(d->app, d->script_path, d->has_script, d->password);
    }
    delete d;
}

static void show_sudo_dialog(const AppInfo& app,
                              const std::string& script_path,
                              bool has_script) {
    SudoDialogData *d = new SudoDialogData();
    d->app         = app;
    d->script_path = script_path;
    d->has_script  = has_script;
    d->confirmed   = false;

    // ── Dialog window ──
    GtkWidget *dlg = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    d->dialog = dlg;
    gtk_window_set_title(GTK_WINDOW(dlg), "Xác nhận quyền quản trị");
    gtk_window_set_resizable(GTK_WINDOW(dlg), FALSE);
    gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(g_state.window));
    gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
    gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 380, -1);
    gtk_container_set_border_width(GTK_CONTAINER(dlg), 0);

    g_signal_connect(dlg, "destroy", G_CALLBACK(on_sudo_dialog_destroyed), d);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(dlg), vbox);

    // ── Top section: icon + title ──
    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_margin_start(top, 24);
    gtk_widget_set_margin_end(top, 24);
    gtk_widget_set_margin_top(top, 24);
    gtk_widget_set_margin_bottom(top, 16);
    gtk_box_pack_start(GTK_BOX(vbox), top, FALSE, FALSE, 0);

    // Lock icon
    GtkWidget *lock_icon = gtk_image_new_from_icon_name(
        "dialog-password-symbolic", GTK_ICON_SIZE_DIALOG);
    if (!gtk_image_get_pixbuf(GTK_IMAGE(lock_icon)) &&
        !gtk_image_get_animation(GTK_IMAGE(lock_icon))) {
        // Fallback icon name
        gtk_image_set_from_icon_name(GTK_IMAGE(lock_icon),
            "security-high-symbolic", GTK_ICON_SIZE_DIALOG);
    }
    GtkStyleContext *ictx = gtk_widget_get_style_context(lock_icon);
    gtk_style_context_add_class(ictx, "sudo-lock-icon");
    gtk_box_pack_start(GTK_BOX(top), lock_icon, FALSE, FALSE, 0);

    GtkWidget *text_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_valign(text_vbox, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(top), text_vbox, TRUE, TRUE, 0);

    GtkWidget *title_lbl = gtk_label_new(("Cài đặt " + app.name).c_str());
    gtk_widget_set_halign(title_lbl, GTK_ALIGN_START);
    GtkStyleContext *tlctx = gtk_widget_get_style_context(title_lbl);
    gtk_style_context_add_class(tlctx, "sudo-dialog-title");
    gtk_box_pack_start(GTK_BOX(text_vbox), title_lbl, FALSE, FALSE, 0);

    GtkWidget *sub_lbl = gtk_label_new(
        "Cài đặt ứng dụng yêu cầu quyền quản trị.\nVui lòng nhập mật khẩu sudo của bạn.");
    gtk_label_set_line_wrap(GTK_LABEL(sub_lbl), TRUE);
    gtk_widget_set_halign(sub_lbl, GTK_ALIGN_START);
    GtkStyleContext *slctx = gtk_widget_get_style_context(sub_lbl);
    gtk_style_context_add_class(slctx, "sudo-dialog-subtitle");
    gtk_box_pack_start(GTK_BOX(text_vbox), sub_lbl, FALSE, FALSE, 0);

    // ── Separator ──
    gtk_box_pack_start(GTK_BOX(vbox),
                       gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),
                       FALSE, FALSE, 0);

    // ── Password area ──
    GtkWidget *pwd_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(pwd_area, 24);
    gtk_widget_set_margin_end(pwd_area, 24);
    gtk_widget_set_margin_top(pwd_area, 16);
    gtk_widget_set_margin_bottom(pwd_area, 8);
    gtk_box_pack_start(GTK_BOX(vbox), pwd_area, FALSE, FALSE, 0);

    // Username row
    GtkWidget *user_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(pwd_area), user_row, FALSE, FALSE, 0);

    GtkWidget *user_lbl = gtk_label_new("Người dùng:");
    gtk_widget_set_size_request(user_lbl, 100, -1);
    gtk_widget_set_halign(user_lbl, GTK_ALIGN_END);
    GtkStyleContext *ulctx = gtk_widget_get_style_context(user_lbl);
    gtk_style_context_add_class(ulctx, "sudo-dialog-subtitle");
    gtk_box_pack_start(GTK_BOX(user_row), user_lbl, FALSE, FALSE, 0);

    // Get current username
    const char *username = g_get_user_name();
    GtkWidget *user_val = gtk_label_new(username ? username : "user");
    gtk_widget_set_halign(user_val, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(user_row), user_val, FALSE, FALSE, 0);

    // Password row
    GtkWidget *pwd_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(pwd_area), pwd_row, FALSE, FALSE, 0);

    GtkWidget *pwd_lbl = gtk_label_new("Mật khẩu:");
    gtk_widget_set_size_request(pwd_lbl, 100, -1);
    gtk_widget_set_halign(pwd_lbl, GTK_ALIGN_END);
    GtkStyleContext *plctx = gtk_widget_get_style_context(pwd_lbl);
    gtk_style_context_add_class(plctx, "sudo-dialog-subtitle");
    gtk_box_pack_start(GTK_BOX(pwd_row), pwd_lbl, FALSE, FALSE, 0);

    GtkWidget *entry = gtk_entry_new();
    d->entry = entry;
    gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);     // hide chars
    gtk_entry_set_invisible_char(GTK_ENTRY(entry), 0x25CF); // ● bullet
    gtk_entry_set_input_purpose(GTK_ENTRY(entry), GTK_INPUT_PURPOSE_PASSWORD);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Nhập mật khẩu...");
    GtkStyleContext *ectx = gtk_widget_get_style_context(entry);
    gtk_style_context_add_class(ectx, "sudo-password-entry");
    gtk_box_pack_start(GTK_BOX(pwd_row), entry, TRUE, TRUE, 0);

    // Error label (hidden by default)
    GtkWidget *error_lbl = gtk_label_new("");
    d->error_label = error_lbl;
    gtk_widget_set_halign(error_lbl, GTK_ALIGN_START);
    gtk_widget_set_margin_start(error_lbl, 114); // align under entry
    GtkStyleContext *erctx = gtk_widget_get_style_context(error_lbl);
    gtk_style_context_add_class(erctx, "sudo-error-label");
    gtk_box_pack_start(GTK_BOX(pwd_area), error_lbl, FALSE, FALSE, 0);

    // ── Separator ──
    gtk_box_pack_start(GTK_BOX(vbox),
                       gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),
                       FALSE, FALSE, 0);

    // ── Button row ──
    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(btn_row, 16);
    gtk_widget_set_margin_end(btn_row, 16);
    gtk_widget_set_margin_top(btn_row, 10);
    gtk_widget_set_margin_bottom(btn_row, 14);
    gtk_widget_set_halign(btn_row, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(vbox), btn_row, FALSE, FALSE, 0);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Hủy");
    GtkStyleContext *cbctx = gtk_widget_get_style_context(cancel_btn);
    gtk_style_context_add_class(cbctx, "sudo-cancel-btn");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_sudo_cancel_clicked), d);
    gtk_box_pack_start(GTK_BOX(btn_row), cancel_btn, FALSE, FALSE, 0);

    GtkWidget *ok_btn = gtk_button_new_with_label("Xác nhận");
    d->ok_btn = ok_btn;
    GtkStyleContext *obctx = gtk_widget_get_style_context(ok_btn);
    gtk_style_context_add_class(obctx, "sudo-ok-btn");
    g_signal_connect(ok_btn, "clicked", G_CALLBACK(on_sudo_ok_clicked), d);
    gtk_box_pack_start(GTK_BOX(btn_row), ok_btn, FALSE, FALSE, 0);

    // Enter key activates OK
    g_signal_connect(entry, "activate", G_CALLBACK(on_sudo_entry_activate), d);

    // Set default widget
    gtk_window_set_default(GTK_WINDOW(dlg), ok_btn);

    gtk_widget_show_all(dlg);
    gtk_widget_grab_focus(entry);
}

// ─── Install window (terminal output) ─────────────────────────────────────

static void launch_install_with_password(const AppInfo& app,
                                          const std::string& script_path,
                                          bool has_script,
                                          const std::string& password) {
    InstallData *data = new InstallData();
    data->app     = app;
    data->success = false;
    data->finished = false;

    // ── Install dialog ──
    GtkWidget *dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    data->dialog = dialog;
    gtk_window_set_title(GTK_WINDOW(dialog), ("Cài đặt " + app.name).c_str());
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 440);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(g_state.window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(dialog), vbox);

    // Header
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(header, 20);
    gtk_widget_set_margin_end(header, 20);
    gtk_widget_set_margin_top(header, 16);
    gtk_widget_set_margin_bottom(header, 16);
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);

    GtkWidget *icon = gtk_image_new_from_icon_name(
        app.icon.empty() ? "application-x-executable" : app.icon.c_str(),
        GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start(GTK_BOX(header), icon, FALSE, FALSE, 0);

    GtkWidget *title_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_pack_start(GTK_BOX(header), title_vbox, TRUE, TRUE, 0);

    GtkWidget *title = gtk_label_new(("Đang cài đặt " + app.name).c_str());
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    GtkStyleContext *ctx = gtk_widget_get_style_context(title);
    gtk_style_context_add_class(ctx, "install-dialog-title");
    gtk_box_pack_start(GTK_BOX(title_vbox), title, FALSE, FALSE, 0);

    GtkWidget *subtitle = gtk_label_new(("Phiên bản: " + app.version + "  •  Tác giả: " + app.author).c_str());
    gtk_widget_set_halign(subtitle, GTK_ALIGN_START);
    GtkStyleContext *ctx2 = gtk_widget_get_style_context(subtitle);
    gtk_style_context_add_class(ctx2, "app-author");
    gtk_box_pack_start(GTK_BOX(title_vbox), subtitle, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox),
                       gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),
                       FALSE, FALSE, 0);

    // Terminal output
    GtkWidget *scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_widget_set_margin_start(scrolled, 16);
    gtk_widget_set_margin_end(scrolled, 16);
    gtk_widget_set_margin_top(scrolled, 12);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    GtkWidget *text_view = gtk_text_view_new();
    data->text_view = text_view;
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    GtkStyleContext *tv_ctx = gtk_widget_get_style_context(text_view);
    gtk_style_context_add_class(tv_ctx, "terminal-box");
    gtk_container_add(GTK_CONTAINER(scrolled), text_view);

    // Progress
    GtkWidget *progress = gtk_progress_bar_new();
    data->progress = progress;
    gtk_widget_set_margin_start(progress, 16);
    gtk_widget_set_margin_end(progress, 16);
    gtk_widget_set_margin_top(progress, 8);
    GtkStyleContext *pb_ctx = gtk_widget_get_style_context(progress);
    gtk_style_context_add_class(pb_ctx, "progress-bar");
    gtk_box_pack_start(GTK_BOX(vbox), progress, FALSE, FALSE, 0);

    // Status label
    GtkWidget *status_label = gtk_label_new("Đang chạy script cài đặt...");
    data->status_label = status_label;
    gtk_widget_set_margin_start(status_label, 16);
    gtk_widget_set_margin_end(status_label, 16);
    gtk_widget_set_margin_top(status_label, 6);
    gtk_widget_set_margin_bottom(status_label, 6);
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox),
                       gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),
                       FALSE, FALSE, 0);

    // Close button
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(btn_box, 16);
    gtk_widget_set_margin_end(btn_box, 16);
    gtk_widget_set_margin_top(btn_box, 8);
    gtk_widget_set_margin_bottom(btn_box, 12);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_box_pack_end(GTK_BOX(vbox), btn_box, FALSE, FALSE, 0);

    GtkWidget *close_btn = gtk_button_new_with_label("Đóng");
    data->close_btn = close_btn;
    gtk_widget_set_sensitive(close_btn, FALSE);
    g_signal_connect_swapped(close_btn, "clicked",
                              G_CALLBACK(gtk_widget_destroy), dialog);
    gtk_box_pack_end(GTK_BOX(btn_box), close_btn, FALSE, FALSE, 0);

    gtk_widget_show_all(dialog);

    // Initial terminal message
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextIter iter;
    gtk_text_buffer_get_start_iter(buf, &iter);
    std::string init_msg = "[VNLF Store] Cài đặt: " + app.name + " v" + app.version + "\n";
    init_msg += "[VNLF Store] Script: " + script_path + "\n";
    init_msg += "─────────────────────────────────────────────\n";
    gtk_text_buffer_insert(buf, &iter, init_msg.c_str(), -1);

    // ── Build command ──
    // We pipe the password to sudo -S so sudo reads it from stdin
    // Full command: echo '<password>' | sudo -S bash '<script>' [args] 2>&1
    std::string args;
    if (!has_script) {
        args = " \"" + app.id + "\" \"" + app.name + "\"";
    }

    // Escape single quotes in password: replace ' with '\''
    std::string safe_pwd;
    for (char c : password) {
        if (c == '\'') safe_pwd += "'\\''";
        else           safe_pwd += c;
    }

    std::string full_cmd =
        "echo '" + safe_pwd + "' | sudo -S bash \"" + script_path + "\"" + args + " 2>&1";

    const gchar *shell_argv[] = { "/bin/bash", "-c", full_cmd.c_str(), nullptr };

    gint stdout_fd = -1;
    GError *err = nullptr;

    bool spawned = g_spawn_async_with_pipes(
        nullptr,
        const_cast<gchar**>(shell_argv),
        nullptr,
        G_SPAWN_DO_NOT_REAP_CHILD,
        nullptr, nullptr,
        &data->pid,
        nullptr,      // no stdin needed (password via echo | pipe in shell)
        &stdout_fd,
        nullptr,
        &err
    );

    if (!spawned) {
        std::string errmsg = "[VNLF] Lỗi khởi chạy: ";
        if (err) { errmsg += err->message; g_error_free(err); }
        errmsg += "\n";
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(buf, &end);
        gtk_text_buffer_insert(buf, &end, errmsg.c_str(), -1);
        gtk_label_set_text(GTK_LABEL(status_label), "✗ Không thể chạy script!");
        gtk_widget_set_sensitive(close_btn, TRUE);
        return;
    }

    g_child_watch_add(data->pid, on_child_exit, data);

    data->channel = g_io_channel_unix_new(stdout_fd);
    g_io_channel_set_encoding(data->channel, nullptr, nullptr);
    g_io_channel_set_flags(data->channel, G_IO_FLAG_NONBLOCK, nullptr);
    g_io_channel_set_buffered(data->channel, FALSE);
    g_io_add_watch(data->channel,
                   (GIOCondition)(G_IO_IN | G_IO_HUP),
                   on_install_output, data);
}

static void run_install(const AppInfo& app) {
    // Resolve script path
    std::string script_path = g_state.scripts_dir + "/" + app.script;
    std::ifstream test(script_path);
    bool has_script = test.good();
    test.close();
    if (!has_script) {
        script_path = g_state.scripts_dir + "/install-generic.sh";
    }

    // Show password dialog first; actual install starts after user confirms
    show_sudo_dialog(app, script_path, has_script);
}

// ─── App Detail View ───────────────────────────────────────────────────────

static void on_install_clicked(GtkButton*, gpointer user_data) {
    AppInfo *app = (AppInfo*)user_data;
    if (!app) return;
    run_install(*app);
}

static void on_back_clicked(GtkButton*, gpointer) {
    show_apps_list();
}

static void show_app_detail(AppInfo *app) {
    if (!app) return;
    g_state.current_detail_app = app;

    // Clear detail box
    GList *children = gtk_container_get_children(GTK_CONTAINER(g_state.detail_box));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    // ── Back button row (inside detail view) ──
    GtkWidget *back_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_start(back_row, 12);
    gtk_widget_set_margin_top(back_row, 10);
    gtk_widget_set_margin_bottom(back_row, 2);
    gtk_box_pack_start(GTK_BOX(g_state.detail_box), back_row, FALSE, FALSE, 0);

    GtkWidget *back_btn = gtk_button_new_with_label("← Quay lại");
    GtkStyleContext *bbctx = gtk_widget_get_style_context(back_btn);
    gtk_style_context_add_class(bbctx, "back-btn");
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_clicked), nullptr);
    gtk_box_pack_start(GTK_BOX(back_row), back_btn, FALSE, FALSE, 0);

    // ── Detail Header ──
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    GtkStyleContext *hctx = gtk_widget_get_style_context(header);
    gtk_style_context_add_class(hctx, "detail-header");
    gtk_widget_set_margin_start(header, 24);
    gtk_widget_set_margin_end(header, 24);
    gtk_widget_set_margin_top(header, 16);
    gtk_widget_set_margin_bottom(header, 24);
    gtk_box_pack_start(GTK_BOX(g_state.detail_box), header, FALSE, FALSE, 0);

    // ── Icon (plain, no overlay) ──
    GtkWidget *icon = make_app_image(*app, 96);
    gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(header), icon, FALSE, FALSE, 0);
    
    // App info
    GtkWidget *info_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_valign(info_vbox, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(header), info_vbox, TRUE, TRUE, 0);
    
    GtkWidget *name_lbl = gtk_label_new(app->name.c_str());
    gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
    GtkStyleContext *nlctx = gtk_widget_get_style_context(name_lbl);
    gtk_style_context_add_class(nlctx, "detail-name");
    gtk_box_pack_start(GTK_BOX(info_vbox), name_lbl, FALSE, FALSE, 0);
    
    GtkWidget *summary_lbl = gtk_label_new(app->summary.c_str());
    gtk_label_set_line_wrap(GTK_LABEL(summary_lbl), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(summary_lbl), PANGO_WRAP_WORD_CHAR);
    gtk_widget_set_halign(summary_lbl, GTK_ALIGN_START);
    GtkStyleContext *slctx = gtk_widget_get_style_context(summary_lbl);
    gtk_style_context_add_class(slctx, "detail-summary");
    gtk_box_pack_start(GTK_BOX(info_vbox), summary_lbl, FALSE, FALSE, 0);
    
    // Meta row
    GtkWidget *meta_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_margin_top(meta_row, 4);
    gtk_box_pack_start(GTK_BOX(info_vbox), meta_row, FALSE, FALSE, 0);
    
    // Version badge
    std::string ver_str = "v" + app->version;
    GtkWidget *ver_lbl = gtk_label_new(ver_str.c_str());
    GtkStyleContext *vctx = gtk_widget_get_style_context(ver_lbl);
    gtk_style_context_add_class(vctx, "app-version");
    gtk_box_pack_start(GTK_BOX(meta_row), ver_lbl, FALSE, FALSE, 0);
    
    GtkWidget *sep_lbl = gtk_label_new("•");
    GtkStyleContext *spctx = gtk_widget_get_style_context(sep_lbl);
    gtk_style_context_add_class(spctx, "app-version");
    gtk_box_pack_start(GTK_BOX(meta_row), sep_lbl, FALSE, FALSE, 0);
    
    GtkWidget *auth_lbl = gtk_label_new(app->author.c_str());
    GtkStyleContext *actx = gtk_widget_get_style_context(auth_lbl);
    gtk_style_context_add_class(actx, "app-author");
    gtk_box_pack_start(GTK_BOX(meta_row), auth_lbl, FALSE, FALSE, 0);
    
    GtkWidget *sep2_lbl = gtk_label_new("•");
    GtkStyleContext *sp2ctx = gtk_widget_get_style_context(sep2_lbl);
    gtk_style_context_add_class(sp2ctx, "app-version");
    gtk_box_pack_start(GTK_BOX(meta_row), sep2_lbl, FALSE, FALSE, 0);
    
    GtkWidget *lic_lbl = gtk_label_new(app->license.c_str());
    GtkStyleContext *lctx = gtk_widget_get_style_context(lic_lbl);
    gtk_style_context_add_class(lctx, "app-version");
    gtk_box_pack_start(GTK_BOX(meta_row), lic_lbl, FALSE, FALSE, 0);
    
    // Install button
    GtkWidget *btn_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_valign(btn_vbox, GTK_ALIGN_CENTER);
    gtk_box_pack_end(GTK_BOX(header), btn_vbox, FALSE, FALSE, 0);
    
    GtkWidget *install_btn = gtk_button_new();
    GtkWidget *btn_content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(btn_content, 4);
    gtk_widget_set_margin_end(btn_content, 4);
    GtkWidget *btn_icon = gtk_image_new_from_icon_name("system-software-install-symbolic", GTK_ICON_SIZE_BUTTON);
    GtkWidget *btn_lbl = gtk_label_new("Cài đặt");
    gtk_box_pack_start(GTK_BOX(btn_content), btn_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_content), btn_lbl, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(install_btn), btn_content);
    
    GtkStyleContext *ibctx = gtk_widget_get_style_context(install_btn);
    gtk_style_context_add_class(ibctx, "install-btn");
    
    g_signal_connect(install_btn, "clicked", G_CALLBACK(on_install_clicked), app);
    gtk_box_pack_start(GTK_BOX(btn_vbox), install_btn, FALSE, FALSE, 0);
    
    // ── Separator ──
    gtk_box_pack_start(GTK_BOX(g_state.detail_box),
                       gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),
                       FALSE, FALSE, 0);
    
    // ── Content area ──
    GtkWidget *content_scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(content_scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(g_state.detail_box), content_scrolled, TRUE, TRUE, 0);
    
    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(content_scrolled), content_box);
    
    // Left: description
    GtkWidget *desc_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(desc_box, 24);
    gtk_widget_set_margin_end(desc_box, 24);
    gtk_widget_set_margin_top(desc_box, 20);
    gtk_widget_set_margin_bottom(desc_box, 20);
    gtk_box_pack_start(GTK_BOX(content_box), desc_box, TRUE, TRUE, 0);
    
    // Description title
    GtkWidget *desc_title = gtk_label_new("Mô tả");
    gtk_widget_set_halign(desc_title, GTK_ALIGN_START);
    GtkStyleContext *dtctx = gtk_widget_get_style_context(desc_title);
    gtk_style_context_add_class(dtctx, "detail-meta-label");
    gtk_box_pack_start(GTK_BOX(desc_box), desc_title, FALSE, FALSE, 0);
    
    GtkWidget *desc_lbl = gtk_label_new(app->description.c_str());
    gtk_label_set_line_wrap(GTK_LABEL(desc_lbl), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(desc_lbl), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_xalign(GTK_LABEL(desc_lbl), 0.0);
    gtk_label_set_max_width_chars(GTK_LABEL(desc_lbl), 60);
    GtkStyleContext *dlctx = gtk_widget_get_style_context(desc_lbl);
    gtk_style_context_add_class(dlctx, "detail-description");
    gtk_widget_set_margin_start(desc_lbl, 0);
    gtk_widget_set_margin_end(desc_lbl, 0);
    gtk_widget_set_margin_top(desc_lbl, 0);
    gtk_widget_set_margin_bottom(desc_lbl, 0);
    gtk_box_pack_start(GTK_BOX(desc_box), desc_lbl, FALSE, FALSE, 0);
    
    // Tags
    if (!app->tags.empty()) {
        GtkWidget *tags_title = gtk_label_new("Thẻ");
        gtk_widget_set_halign(tags_title, GTK_ALIGN_START);
        GtkWidget *tags_flow = gtk_flow_box_new();
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(tags_flow), 10);
        gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(tags_flow), GTK_SELECTION_NONE);
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(tags_flow), 4);
        gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(tags_flow), 4);
        
        GtkStyleContext *ttctx = gtk_widget_get_style_context(tags_title);
        gtk_style_context_add_class(ttctx, "detail-meta-label");
        gtk_box_pack_start(GTK_BOX(desc_box), tags_title, FALSE, FALSE, 0);
        
        for (auto& tag : app->tags) {
            GtkWidget *chip = gtk_label_new(tag.c_str());
            GtkStyleContext *cctx = gtk_widget_get_style_context(chip);
            gtk_style_context_add_class(cctx, "tag-chip");
            gtk_flow_box_insert(GTK_FLOW_BOX(tags_flow), chip, -1);
        }
        gtk_box_pack_start(GTK_BOX(desc_box), tags_flow, FALSE, FALSE, 0);
    }
    
    // Right: metadata panel
    GtkWidget *meta_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(meta_panel, 240, -1);
    gtk_widget_set_margin_end(meta_panel, 24);
    gtk_widget_set_margin_top(meta_panel, 20);
    
    // Background for panel
    GtkWidget *meta_frame = gtk_frame_new(nullptr);
    gtk_frame_set_shadow_type(GTK_FRAME(meta_frame), GTK_SHADOW_NONE);
    gtk_box_pack_end(GTK_BOX(content_box), meta_panel, FALSE, FALSE, 0);
    
    // Meta items
    struct MetaItem { std::string label, value; };
    std::vector<MetaItem> meta_items = {
        {"Phiên bản", app->version},
        {"Tác giả", app->author},
        {"Giấy phép", app->license},
        {"Danh mục", CATEGORY_LABELS.count(app->category) ? 
                      CATEGORY_LABELS[app->category] : app->category},
        {"Website", app->website},
        {"Script", app->script},
    };
    
    for (auto& item : meta_items) {
        GtkWidget *item_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_margin_bottom(item_box, 14);
        
        GtkWidget *ml = gtk_label_new(item.label.c_str());
        gtk_widget_set_halign(ml, GTK_ALIGN_START);
        GtkStyleContext *mlctx = gtk_widget_get_style_context(ml);
        gtk_style_context_add_class(mlctx, "detail-meta-label");
        gtk_box_pack_start(GTK_BOX(item_box), ml, FALSE, FALSE, 0);
        
        GtkWidget *vl;
        if (item.label == "Website") {
            vl = gtk_link_button_new_with_label(item.value.c_str(), item.value.c_str());
            gtk_widget_set_halign(vl, GTK_ALIGN_START);
        } else {
            vl = gtk_label_new(item.value.c_str());
            gtk_widget_set_halign(vl, GTK_ALIGN_START);
            gtk_label_set_line_wrap(GTK_LABEL(vl), TRUE);
            GtkStyleContext *vlctx = gtk_widget_get_style_context(vl);
            gtk_style_context_add_class(vlctx, "detail-meta-value");
        }
        gtk_box_pack_start(GTK_BOX(item_box), vl, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(meta_panel), item_box, FALSE, FALSE, 0);
    }
    
    gtk_widget_show_all(g_state.detail_box);
    gtk_stack_set_visible_child_name(GTK_STACK(g_state.stack), "detail");
    
    // Update headerbar back button visibility
    // (done via stack page change)
}

static void show_apps_list() {
    g_state.current_detail_app = nullptr;
    gtk_stack_set_visible_child_name(GTK_STACK(g_state.stack), "grid");
    rebuild_apps_grid();
}

// ─── App Card ─────────────────────────────────────────────────────────────

struct CardClickData {
    AppInfo *app;
};

static gboolean on_card_button_press(GtkWidget*, GdkEventButton *event, gpointer user_data) {
    if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
        CardClickData *data = (CardClickData*)user_data;
        show_app_detail(data->app);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_card_enter(GtkWidget *widget, GdkEventCrossing*, gpointer) {
    GdkDisplay *display = gdk_display_get_default();
    GdkCursor *cursor = gdk_cursor_new_for_display(display, GDK_HAND2);
    gdk_window_set_cursor(gtk_widget_get_window(widget), cursor);
    g_object_unref(cursor);
    return FALSE;
}

static gboolean on_card_leave(GtkWidget *widget, GdkEventCrossing*, gpointer) {
    gdk_window_set_cursor(gtk_widget_get_window(widget), nullptr);
    return FALSE;
}

static GtkWidget* create_app_card(AppInfo *app) {
    GtkWidget *card = gtk_event_box_new();
    gtk_widget_add_events(card, GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
    
    GtkWidget *card_inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkStyleContext *ctx = gtk_widget_get_style_context(card_inner);
    gtk_style_context_add_class(ctx, "app-card");
    gtk_container_add(GTK_CONTAINER(card), card_inner);
    
    // Top row: icon + name
    GtkWidget *top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(card_inner), top_row, FALSE, FALSE, 0);
    
    GtkWidget *icon = make_app_image(*app, 48);
    gtk_box_pack_start(GTK_BOX(top_row), icon, FALSE, FALSE, 0);
    
    GtkWidget *name_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_valign(name_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(top_row), name_box, TRUE, TRUE, 0);
    
    GtkWidget *name_lbl = gtk_label_new(app->name.c_str());
    gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(name_lbl), PANGO_ELLIPSIZE_END);
    GtkStyleContext *nlctx = gtk_widget_get_style_context(name_lbl);
    gtk_style_context_add_class(nlctx, "app-name");
    gtk_box_pack_start(GTK_BOX(name_box), name_lbl, FALSE, FALSE, 0);
    
    GtkWidget *auth_lbl = gtk_label_new(app->author.c_str());
    gtk_widget_set_halign(auth_lbl, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(auth_lbl), PANGO_ELLIPSIZE_END);
    GtkStyleContext *alctx = gtk_widget_get_style_context(auth_lbl);
    gtk_style_context_add_class(alctx, "app-author");
    gtk_box_pack_start(GTK_BOX(name_box), auth_lbl, FALSE, FALSE, 0);
    
    // Summary
    GtkWidget *summary_lbl = gtk_label_new(app->summary.c_str());
    gtk_label_set_line_wrap(GTK_LABEL(summary_lbl), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(summary_lbl), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_lines(GTK_LABEL(summary_lbl), 2);
    gtk_label_set_ellipsize(GTK_LABEL(summary_lbl), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign(summary_lbl, GTK_ALIGN_START);
    GtkStyleContext *slctx = gtk_widget_get_style_context(summary_lbl);
    gtk_style_context_add_class(slctx, "app-summary");
    gtk_box_pack_start(GTK_BOX(card_inner), summary_lbl, FALSE, FALSE, 0);
    
    // Bottom row: category badge + version
    GtkWidget *bottom_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(bottom_row, 4);
    gtk_box_pack_start(GTK_BOX(card_inner), bottom_row, FALSE, FALSE, 0);
    
    // Category badge
    std::string cat_text = CATEGORY_LABELS.count(app->category) ?
                           CATEGORY_LABELS[app->category] : app->category;
    GtkWidget *cat_lbl = gtk_label_new(cat_text.c_str());
    GtkStyleContext *cctx = gtk_widget_get_style_context(cat_lbl);
    gtk_style_context_add_class(cctx, "cat-badge");
    gtk_style_context_add_class(cctx, ("cat-" + app->category).c_str());
    gtk_box_pack_start(GTK_BOX(bottom_row), cat_lbl, FALSE, FALSE, 0);
    
    // Version
    GtkWidget *ver_lbl = gtk_label_new(("v" + app->version).c_str());
    GtkStyleContext *vctx = gtk_widget_get_style_context(ver_lbl);
    gtk_style_context_add_class(vctx, "app-version");
    gtk_box_pack_end(GTK_BOX(bottom_row), ver_lbl, FALSE, FALSE, 0);
    
    // Click handler
    CardClickData *click_data = new CardClickData{app};
    g_signal_connect(card, "button-press-event", G_CALLBACK(on_card_button_press), click_data);
    g_signal_connect(card, "enter-notify-event", G_CALLBACK(on_card_enter), nullptr);
    g_signal_connect(card, "leave-notify-event", G_CALLBACK(on_card_leave), nullptr);
    
    return card;
}

// ─── Grid rebuild ──────────────────────────────────────────────────────────

static void rebuild_apps_grid() {
    // Clear existing children
    GList *children = gtk_container_get_children(GTK_CONTAINER(g_state.apps_grid));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);
    
    if (g_state.filtered_apps.empty()) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
        gtk_widget_set_valign(empty_box, GTK_ALIGN_CENTER);
        gtk_widget_set_halign(empty_box, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top(empty_box, 80);
        
        GtkWidget *empty_icon = gtk_image_new_from_icon_name("system-search-symbolic", GTK_ICON_SIZE_DIALOG);
        GtkStyleContext *ictx = gtk_widget_get_style_context(empty_icon);
        gtk_style_context_add_class(ictx, "empty-state-icon");
        gtk_box_pack_start(GTK_BOX(empty_box), empty_icon, FALSE, FALSE, 0);
        
        GtkWidget *empty_lbl = gtk_label_new("Không tìm thấy ứng dụng phù hợp");
        GtkStyleContext *lctx = gtk_widget_get_style_context(empty_lbl);
        gtk_style_context_add_class(lctx, "empty-state");
        gtk_box_pack_start(GTK_BOX(empty_box), empty_lbl, FALSE, FALSE, 0);
        
        gtk_grid_attach(GTK_GRID(g_state.apps_grid), empty_box, 0, 0, 3, 1);
        gtk_widget_show_all(g_state.apps_grid);
        return;
    }
    
    int col = 0, row = 0;
    const int COLS = 3;
    
    for (size_t i = 0; i < g_state.filtered_apps.size(); i++) {
        AppInfo *app = &g_state.filtered_apps[i];
        GtkWidget *card = create_app_card(app);
        gtk_widget_set_size_request(card, 300, 150);
        gtk_grid_attach(GTK_GRID(g_state.apps_grid), card, col, row, 1, 1);
        
        col++;
        if (col >= COLS) { col = 0; row++; }
    }
    
    // Update count label
    std::string count_text = std::to_string(g_state.filtered_apps.size()) + " ứng dụng";
    gtk_label_set_text(GTK_LABEL(g_state.count_label), count_text.c_str());
    
    gtk_widget_show_all(g_state.apps_grid);
}

// ─── Search & Category Handlers ────────────────────────────────────────────

static void on_search_changed(GtkSearchEntry *entry, gpointer) {
    g_state.search_query = gtk_entry_get_text(GTK_ENTRY(entry));
    apply_filter();
    
    if (gtk_stack_get_visible_child_name(GTK_STACK(g_state.stack)) == std::string("detail")) {
        show_apps_list();
    } else {
        rebuild_apps_grid();
    }
}

struct CategoryButtonData {
    std::string category;
    GtkWidget *button;
};

static std::vector<GtkWidget*> g_category_buttons;

static void on_category_clicked(GtkButton *btn, gpointer user_data) {
    CategoryButtonData *data = (CategoryButtonData*)user_data;
    g_state.current_category = data->category;
    
    // Update active state
    for (auto *b : g_category_buttons) {
        GtkStyleContext *ctx = gtk_widget_get_style_context(b);
        gtk_style_context_remove_class(ctx, "active");
    }
    GtkStyleContext *ctx = gtk_widget_get_style_context(GTK_WIDGET(btn));
    gtk_style_context_add_class(ctx, "active");
    
    apply_filter();
    
    if (gtk_stack_get_visible_child_name(GTK_STACK(g_state.stack)) == std::string("detail")) {
        show_apps_list();
    } else {
        rebuild_apps_grid();
    }
}

// ─── Window setup ──────────────────────────────────────────────────────────

// Forward declaration needed by sidebar toggle
static void rebuild_sidebar();

// ── Sidebar toggle ──
static void on_toggle_sidebar(GtkButton*, gpointer) {
    g_state.sidebar_collapsed = !g_state.sidebar_collapsed;
    rebuild_sidebar();
}

// ── Build / rebuild sidebar contents ──
// Called once at startup and again whenever collapsed state changes.
static void rebuild_sidebar() {
    bool compact = g_state.sidebar_collapsed;

    // Target width
    int sidebar_w = compact ? 54 : 200;
    gtk_widget_set_size_request(g_state.sidebar_box, sidebar_w, -1);

    // Clear existing children
    GList *ch = gtk_container_get_children(GTK_CONTAINER(g_state.sidebar_box));
    for (GList *l = ch; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(ch);
    g_category_buttons.clear();

    // Count apps per category
    std::map<std::string, int> cat_count;
    cat_count[""] = (int)g_state.all_apps.size();
    for (auto& app : g_state.all_apps) cat_count[app.category]++;

    std::vector<std::string> categories = {
        "", "internet", "multimedia", "office", "graphics",
        "development", "system", "games", "education", "utilities"
    };

    if (!compact) {
        // ── Full mode ──
        GtkWidget *title = gtk_label_new("DANH MỤC");
        gtk_widget_set_halign(title, GTK_ALIGN_START);
        GtkStyleContext *tc = gtk_widget_get_style_context(title);
        gtk_style_context_add_class(tc, "sidebar-title");
        gtk_box_pack_start(GTK_BOX(g_state.sidebar_box), title, FALSE, FALSE, 0);

        for (auto& cat : categories) {
            if (cat != "" && !cat_count.count(cat)) continue;
            std::string label    = CATEGORY_LABELS.count(cat) ? CATEGORY_LABELS[cat] : cat;
            std::string icon_name = CATEGORY_ICONS.count(cat) ? CATEGORY_ICONS[cat] : "applications-other-symbolic";

            GtkWidget *btn = gtk_button_new();
            GtkWidget *inner = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

            GtkWidget *ico = gtk_image_new_from_icon_name(icon_name.c_str(), GTK_ICON_SIZE_SMALL_TOOLBAR);
            GtkWidget *lbl = gtk_label_new(label.c_str());
            gtk_widget_set_halign(lbl, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(inner), ico, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(inner), lbl, TRUE, TRUE, 0);

            if (cat_count.count(cat)) {
                GtkWidget *cnt = gtk_label_new(std::to_string(cat_count[cat]).c_str());
                GtkStyleContext *cc = gtk_widget_get_style_context(cnt);
                gtk_style_context_add_class(cc, "category-count");
                gtk_box_pack_end(GTK_BOX(inner), cnt, FALSE, FALSE, 0);
            }
            gtk_container_add(GTK_CONTAINER(btn), inner);

            GtkStyleContext *bc = gtk_widget_get_style_context(btn);
            gtk_style_context_add_class(bc, "category-btn");
            if (cat == g_state.current_category)
                gtk_style_context_add_class(bc, "active");

            g_category_buttons.push_back(btn);
            CategoryButtonData *d = new CategoryButtonData{cat, btn};
            g_signal_connect(btn, "clicked", G_CALLBACK(on_category_clicked), d);
            gtk_box_pack_start(GTK_BOX(g_state.sidebar_box), btn, FALSE, FALSE, 0);
        }
    } else {
        // ── Compact (icon-only) mode ──
        for (auto& cat : categories) {
            if (cat != "" && !cat_count.count(cat)) continue;
            std::string icon_name = CATEGORY_ICONS.count(cat) ? CATEGORY_ICONS[cat] : "applications-other-symbolic";
            std::string label     = CATEGORY_LABELS.count(cat) ? CATEGORY_LABELS[cat] : cat;

            GtkWidget *btn = gtk_button_new();
            GtkWidget *ico = gtk_image_new_from_icon_name(icon_name.c_str(), GTK_ICON_SIZE_SMALL_TOOLBAR);
            gtk_container_add(GTK_CONTAINER(btn), ico);
            gtk_widget_set_tooltip_text(btn, label.c_str());

            GtkStyleContext *bc = gtk_widget_get_style_context(btn);
            gtk_style_context_add_class(bc, "category-btn-compact");
            if (cat == g_state.current_category)
                gtk_style_context_add_class(bc, "active");

            g_category_buttons.push_back(btn);
            CategoryButtonData *d = new CategoryButtonData{cat, btn};
            g_signal_connect(btn, "clicked", G_CALLBACK(on_category_clicked), d);
            gtk_box_pack_start(GTK_BOX(g_state.sidebar_box), btn, FALSE, FALSE, 0);
        }
    }

    // ── Toggle button at the bottom ──
    // Spacer
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(g_state.sidebar_box), spacer, TRUE, TRUE, 0);

    GtkWidget *tog_btn = gtk_button_new();
    // Arrow: ‹ collapse / › expand
    const char *arrow = compact ? "›" : "‹";
    GtkWidget *arrow_lbl = gtk_label_new(arrow);
    gtk_container_add(GTK_CONTAINER(tog_btn), arrow_lbl);
    GtkStyleContext *tc2 = gtk_widget_get_style_context(tog_btn);
    gtk_style_context_add_class(tc2, "sidebar-toggle-btn");
    gtk_widget_set_tooltip_text(tog_btn, compact ? "Mở rộng" : "Thu gọn");
    g_signal_connect(tog_btn, "clicked", G_CALLBACK(on_toggle_sidebar), nullptr);
    gtk_box_pack_end(GTK_BOX(g_state.sidebar_box), tog_btn, FALSE, FALSE, 0);

    gtk_widget_show_all(g_state.sidebar_box);
}

static void setup_window() {
    // Apply CSS
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, APP_CSS, -1, nullptr);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    // ── Main window — no system decoration (we draw our own titlebar) ──
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_state.window = window;
    gtk_window_set_title(GTK_WINDOW(window), "VNLF App Store");
    gtk_window_set_default_size(GTK_WINDOW(window), 1100, 700);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

    // ── Custom Header Bar ──
    GtkWidget *header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
    GtkStyleContext *hbctx = gtk_widget_get_style_context(header_bar);
    gtk_style_context_add_class(hbctx, "custom-titlebar");

    // App title (center)
    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(title_box, GTK_ALIGN_CENTER);

    GtkWidget *title_lbl = gtk_label_new("VNLF App Store");
    GtkStyleContext *tlctx = gtk_widget_get_style_context(title_lbl);
    gtk_style_context_add_class(tlctx, "titlebar-title");
    gtk_box_pack_start(GTK_BOX(title_box), title_lbl, FALSE, FALSE, 0);

    GtkWidget *sub_lbl = gtk_label_new("Kho ứng dụng Linux tiếng Việt");
    GtkStyleContext *slctx = gtk_widget_get_style_context(sub_lbl);
    gtk_style_context_add_class(slctx, "titlebar-subtitle");
    gtk_box_pack_start(GTK_BOX(title_box), sub_lbl, FALSE, FALSE, 0);
    gtk_header_bar_set_custom_title(GTK_HEADER_BAR(header_bar), title_box);

    // Search entry (right side)
    GtkWidget *search = gtk_search_entry_new();
    g_state.search_entry = search;
    gtk_entry_set_placeholder_text(GTK_ENTRY(search), "Tìm kiếm...");
    gtk_widget_set_size_request(search, 240, -1);
    GtkStyleContext *sctx = gtk_widget_get_style_context(search);
    gtk_style_context_add_class(sctx, "search-box");
    g_signal_connect(search, "search-changed", G_CALLBACK(on_search_changed), nullptr);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), search);

    gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);

    // ── Main layout ──
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(main_box), content_box, TRUE, TRUE, 0);

    // ── Sidebar container ──
    GtkWidget *sidebar_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    g_state.sidebar_box = sidebar_box;
    GtkStyleContext *sbctx = gtk_widget_get_style_context(sidebar_box);
    gtk_style_context_add_class(sbctx, "sidebar");
    gtk_widget_set_size_request(sidebar_box, 200, -1);
    gtk_box_pack_start(GTK_BOX(content_box), sidebar_box, FALSE, FALSE, 0);

    // Sidebar separator
    GtkWidget *sidebar_sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    g_state.sidebar_separator = sidebar_sep;
    gtk_box_pack_start(GTK_BOX(content_box), sidebar_sep, FALSE, FALSE, 0);

    // ── Right panel ──
    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(content_box), right_box, TRUE, TRUE, 0);

    // Top bar with count label
    GtkWidget *top_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(top_bar, 16);
    gtk_widget_set_margin_end(top_bar, 16);
    gtk_widget_set_margin_top(top_bar, 10);
    gtk_widget_set_margin_bottom(top_bar, 10);
    gtk_box_pack_start(GTK_BOX(right_box), top_bar, FALSE, FALSE, 0);

    g_state.count_label = gtk_label_new("");
    GtkStyleContext *clctx = gtk_widget_get_style_context(g_state.count_label);
    gtk_style_context_add_class(clctx, "app-author");
    gtk_widget_set_halign(g_state.count_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(top_bar), g_state.count_label, TRUE, TRUE, 0);

    // ── Stack (grid vs detail) ──
    GtkWidget *stack = gtk_stack_new();
    g_state.stack = stack;
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 150);
    gtk_box_pack_start(GTK_BOX(right_box), stack, TRUE, TRUE, 0);

    // Grid page
    GtkWidget *grid_scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(grid_scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    g_state.apps_scrolled = grid_scrolled;
    gtk_stack_add_named(GTK_STACK(stack), grid_scrolled, "grid");

    GtkWidget *grid = gtk_grid_new();
    g_state.apps_grid = grid;
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_widget_set_margin_start(grid, 16);
    gtk_widget_set_margin_end(grid, 16);
    gtk_widget_set_margin_top(grid, 4);
    gtk_widget_set_margin_bottom(grid, 16);
    gtk_container_add(GTK_CONTAINER(grid_scrolled), grid);

    // Detail page
    GtkWidget *detail_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    g_state.detail_box = detail_box;
    gtk_stack_add_named(GTK_STACK(stack), detail_box, "detail");

    // Status bar
    GtkWidget *status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkStyleContext *stbctx = gtk_widget_get_style_context(status_bar);
    gtk_style_context_add_class(stbctx, "status-bar");
    gtk_box_pack_end(GTK_BOX(main_box), status_bar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(status_bar),
        gtk_label_new("VNLF App Store — Kho ứng dụng Linux tiếng Việt"),
        FALSE, FALSE, 0);

    // ── Build sidebar & show everything ──
    rebuild_sidebar();   // fills sidebar_box for the first time

    gtk_widget_show_all(window);

    // Start on the grid page
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "grid");

    // Initial grid build
    apply_filter();
    rebuild_apps_grid();
}


// ─── Main ──────────────────────────────────────────────────────────────────

static std::string get_executable_dir() {
    char buf[4096] = {0};
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf)-1);
    if (n <= 0) return ".";
    std::string path(buf, n);
    size_t slash = path.rfind('/');
    if (slash == std::string::npos) return ".";
    return path.substr(0, slash);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Determine paths
    std::string exe_dir = get_executable_dir();
    
    // Try to find data relative to executable
    // Support: same dir, or ../data, or from source tree
    std::vector<std::string> data_candidates = {
        exe_dir + "/data",
        exe_dir + "/../data",
        exe_dir,
        "data",
        ".",
    };
    
    for (auto& dir : data_candidates) {
        // Accept dir if it has apps.json OR an apps/ subdirectory
        std::ifstream test1(dir + "/apps.json");
        GDir *test2 = g_dir_open((dir + "/apps").c_str(), 0, nullptr);
        bool ok = test1.good() || test2;
        if (test2) g_dir_close(test2);
        if (ok) { g_state.data_dir = dir; break; }
    }

    if (g_state.data_dir.empty()) {
        g_state.data_dir = exe_dir + "/data";
    }

    // Icons dir = data/icons/
    g_state.icons_dir = g_state.data_dir + "/icons";
    g_mkdir_with_parents(g_state.icons_dir.c_str(), 0755);

    // Scripts dir
    std::vector<std::string> script_candidates = {
        exe_dir + "/scripts",
        exe_dir + "/../scripts",
        "scripts",
        ".",
    };

    for (auto& dir : script_candidates) {
        std::ifstream test(dir + "/install-generic.sh");
        if (test.good()) { g_state.scripts_dir = dir; break; }
    }

    if (g_state.scripts_dir.empty()) {
        g_state.scripts_dir = exe_dir + "/scripts";
    }

    // Load apps (supports per-file layout AND single apps.json)
    g_state.all_apps = parse_apps_json(g_state.data_dir);

    if (g_state.all_apps.empty()) {
        std::cerr << "Cảnh báo: Không tải được ứng dụng — dùng dữ liệu mẫu\n";
        AppInfo sample;
        sample.id = "firefox"; sample.name = "Firefox";
        sample.summary = "Trình duyệt web nhanh và bảo mật";
        sample.description = "Mozilla Firefox là trình duyệt web mã nguồn mở miễn phí.";
        sample.category = "internet"; sample.icon = "firefox";
        sample.version = "124.0"; sample.author = "Mozilla Foundation";
        sample.license = "MPL-2.0"; sample.website = "https://www.mozilla.org/";
        sample.script = "install-firefox.sh";
        sample.tags = {"trình duyệt", "web"};
        g_state.all_apps.push_back(sample);
    }

    std::cout << "VNLF App Store khởi động\n";
    std::cout << "Data dir:    " << g_state.data_dir    << "\n";
    std::cout << "Icons dir:   " << g_state.icons_dir   << "\n";
    std::cout << "Scripts dir: " << g_state.scripts_dir << "\n";
    std::cout << "Ứng dụng:    " << g_state.all_apps.size() << "\n";

    // ── Fonts: tìm thư mục fonts/ cạnh binary hoặc trong source tree ──
    std::vector<std::string> font_candidates = {
        exe_dir + "/fonts",
        exe_dir + "/../fonts",
        "fonts",
        ".",
    };
    for (auto& dir : font_candidates) {
        GDir *d = g_dir_open(dir.c_str(), 0, nullptr);
        if (d) {
            // Check if any .ttf exists inside
            bool has_font = false;
            const gchar *fn;
            while ((fn = g_dir_read_name(d)) != nullptr) {
                std::string s(fn);
                if (s.size() > 4 && (s.substr(s.size()-4) == ".ttf" ||
                                      s.substr(s.size()-4) == ".otf")) {
                    has_font = true; break;
                }
            }
            g_dir_close(d);
            if (has_font) { g_fonts_dir = dir; break; }
        }
    }
    std::cout << "Fonts dir:   " << (g_fonts_dir.empty() ? "(không tìm thấy, dùng font hệ thống)" : g_fonts_dir) << "\n";
    load_app_fonts();

    setup_window();
    gtk_main();
    return 0;
}
