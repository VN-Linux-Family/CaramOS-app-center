#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// App Explorer — ThemeConfig
// Đọc file .cfg và chuyển đổi sang GTK CSS string
// ─────────────────────────────────────────────────────────────────────────────

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace AppExplorer {

// Cấu trúc lưu toàn bộ theme
struct ThemeConfig {
    // [theme]
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    bool dark_mode       = false;
    bool follow_system   = false;

    // [colors.base]
    struct BaseColors {
        std::string bg_primary      = "#FAF6EF";
        std::string bg_secondary    = "#F0EBE1";
        std::string bg_card         = "#FFFFFF";
        std::string bg_card_hover   = "#F5F0E8";
        std::string bg_card_selected= "#E8F5E9";
        std::string bg_elevated     = "#FFFFFF";
        std::string bg_sunken       = "#EEEBE3";
        std::string border_light    = "#D8D0C4";
        std::string border_normal   = "#C4BAA8";
        std::string border_focus    = "#4A7C59";
        std::string border_error    = "#C0392B";
    } base;

    // [colors.text]
    struct TextColors {
        std::string primary     = "#2C2416";
        std::string secondary   = "#6B5D4A";
        std::string disabled    = "#ADA090";
        std::string on_dark     = "#FAF6EF";
        std::string link        = "#2E6B45";
        std::string link_hover  = "#1A4A2E";
        std::string error       = "#C0392B";
        std::string success     = "#2E6B45";
        std::string warning     = "#B7770D";
        std::string info        = "#1A6B8A";
    } text;

    // [colors.accent]
    struct AccentColors {
        std::string primary = "#2E6B45";
        std::string hover   = "#1A4A2E";
        std::string pressed = "#0F2E1A";
        std::string light   = "#E8F5E9";
        std::string border  = "#4A7C59";
    } accent;

    // [colors.nav]
    struct NavColors {
        std::string bg              = "#2E6B45";
        std::string item_hover_bg   = "#FFFFFF";
        std::string item_active_bg  = "#1A4A2E";
        std::string item_text       = "#FAF6EF";
        std::string item_hover_text = "#2C2416";
        std::string item_active_text= "#FAF6EF";
        std::string border          = "#1A4A2E";
        std::string shadow          = "#00000026";
    } nav;

    // [colors.button]
    struct ButtonColors {
        std::string bg              = "#FFFFFF";
        std::string bg_hover        = "#2E6B45";
        std::string bg_pressed      = "#1A4A2E";
        std::string bg_disabled     = "#E8E4DC";
        std::string text            = "#2C2416";
        std::string text_hover      = "#FFFFFF";
        std::string text_pressed    = "#FFFFFF";
        std::string text_disabled   = "#ADA090";
        std::string border          = "#C4BAA8";
        std::string border_hover    = "#2E6B45";
        std::string border_pressed  = "#1A4A2E";
        std::string border_disabled = "#D8D0C4";
        // danger
        std::string danger_bg           = "#FFFFFF";
        std::string danger_bg_hover     = "#C0392B";
        std::string danger_text         = "#C0392B";
        std::string danger_text_hover   = "#FFFFFF";
        std::string danger_border       = "#C0392B";
        std::string danger_border_hover = "#8E2219";
        // primary
        std::string primary_bg          = "#2E6B45";
        std::string primary_bg_hover    = "#1A4A2E";
        std::string primary_text        = "#FFFFFF";
        std::string primary_text_hover  = "#FFFFFF";
        std::string primary_border      = "#1A4A2E";
        std::string primary_border_hover= "#0F2E1A";
    } button;

    // [colors.statusbar]
    struct StatusbarColors {
        std::string bg           = "#EDE8DF";
        std::string text         = "#6B5D4A";
        std::string border       = "#D8D0C4";
        std::string progress_bg  = "#D8D0C4";
        std::string progress_fill= "#2E6B45";
    } statusbar;

    // [colors.console]
    struct ConsoleColors {
        std::string bg          = "#1A1612";
        std::string text        = "#E8DFD0";
        std::string text_cmd    = "#A8D5B5";
        std::string text_stdout = "#E8DFD0";
        std::string text_stderr = "#F0C080";
        std::string text_error  = "#F08080";
        std::string text_success= "#80D0A0";
        std::string text_info   = "#90A098";
        std::string border      = "#2E6B45";
        std::string header_bg   = "#0F0E0B";
        std::string header_text = "#A8D5B5";
    } console;

    // [colors.badge]
    struct BadgeColors {
        std::string bg              = "#E8EDE0";
        std::string text            = "#3A5A30";
        std::string border          = "#B0C8A8";
        std::string installed_bg    = "#E8F5E9";
        std::string installed_text  = "#2E6B45";
        std::string installed_border= "#4A9A5A";
    } badge;

    // [colors.scrollbar]
    struct ScrollbarColors {
        std::string track       = "#EDE8DF";
        std::string thumb       = "#C4BAA8";
        std::string thumb_hover = "#A09080";
        std::string thumb_active= "#2E6B45";
    } scrollbar;

    // [colors.tooltip]
    struct TooltipColors {
        std::string bg     = "#2C2416";
        std::string text   = "#FAF6EF";
        std::string border = "#4A3A28";
    } tooltip;

    // [colors.dialog]
    struct DialogColors {
        std::string bg          = "#FFFFFF";
        std::string overlay     = "#00000066";
        std::string header_bg   = "#F0EBE1";
        std::string header_text = "#2C2416";
        std::string border      = "#C4BAA8";
        std::string shadow      = "#00000033";
    } dialog;

    // [colors.search]
    struct SearchColors {
        std::string bg          = "#FFFFFF";
        std::string text        = "#2C2416";
        std::string placeholder = "#ADA090";
        std::string border      = "#C4BAA8";
        std::string border_focus= "#2E6B45";
        std::string icon        = "#8A7A68";
        std::string bg_focus    = "#FFFFFF";
    } search;

    // [typography]
    struct Typography {
        std::string font_ui             = "Cantarell";
        int         font_ui_size        = 10;
        std::string font_title          = "Cantarell";
        int         font_title_size     = 14;
        std::string font_title_weight   = "bold";
        std::string font_subtitle       = "Cantarell";
        int         font_subtitle_size  = 9;
        std::string font_mono           = "Monospace";
        int         font_mono_size      = 9;
        std::string font_button         = "Cantarell";
        int         font_button_size    = 10;
        std::string font_button_weight  = "normal";
        std::string font_nav            = "Cantarell";
        int         font_nav_size       = 10;
        std::string font_nav_weight     = "normal";
        std::string font_badge          = "Cantarell";
        int         font_badge_size     = 8;
    } typography;

    // [layout]
    struct Layout {
        int window_width        = 1100;
        int window_height       = 720;
        int window_min_width    = 800;
        int window_min_height   = 560;
        int nav_width           = 200;
        int nav_item_height     = 40;
        int nav_item_padding_h  = 16;
        int nav_item_padding_v  = 8;
        int nav_icon_size       = 16;
        int content_padding     = 20;
        int content_gap         = 16;
        int card_width          = 220;
        int card_height         = 280;
        int card_padding        = 16;
        int card_gap            = 12;
        int card_icon_size      = 64;
        int list_row_height     = 72;
        int list_icon_size      = 48;
        int list_padding_h      = 16;
        int list_padding_v      = 8;
        int detail_panel_width  = 380;
        int console_height      = 200;
        int console_min_height  = 80;
        int console_max_height  = 400;
        int statusbar_height    = 28;
        int toolbar_height      = 48;
        int search_width        = 280;
        int search_height       = 32;
    } layout;

    // [radius]
    struct Radius {
        int window          = 0;
        int card            = 8;
        int button          = 4;
        int button_small    = 3;
        int input           = 4;
        int tooltip         = 4;
        int dialog          = 8;
        int console         = 4;
        int image           = 6;
        int nav_item        = 4;
        int progress        = 3;
        int scrollbar_thumb = 3;
    } radius;

    // [shadows]
    struct Shadows {
        std::string card        = "0 1px 3px alpha(black, 0.1), 0 1px 2px alpha(black, 0.1)";
        std::string card_hover  = "0 4px 8px alpha(black, 0.1), 0 2px 4px alpha(black, 0.1)";
        std::string button      = "none";
        std::string dialog      = "0 8px 32px alpha(black, 0.2)";
        std::string nav         = "2px 0 8px alpha(black, 0.08)";
        std::string toolbar     = "0 1px 4px alpha(black, 0.1)";
    } shadows;

    // [animation]
    struct Animation {
        bool        enabled         = true;
        int         hover_duration  = 150;
        int         transition      = 200;
        int         page_fade       = 100;
        std::string easing          = "ease-out";
    } animation;

    // [misc]
    struct Misc {
        bool        show_console_auto           = true;
        bool        collapse_console_on_finish  = false;
        std::string default_view                = "grid";
        int         grid_columns                = 4;
        bool        show_category_badge         = true;
        bool        show_installed_badge        = true;
        double      uninstalled_icon_opacity    = 0.85;
    } misc;

    // [gtk_extra]
    struct GtkExtra {
        std::string extra_css_file;
        std::string inline_css;
    } gtk_extra;
};

// Parser: đọc .cfg → ThemeConfig
class ThemeConfigParser {
public:
    ThemeConfigParser();
    ~ThemeConfigParser();

    // Load từ file
    bool loadFromFile(const std::string& path, ThemeConfig& out_config);

    // Load từ string
    bool loadFromString(const std::string& content, ThemeConfig& out_config);

    // Lấy error message
    const std::string& getLastError() const { return last_error_; }

private:
    using IniMap = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;
    
    bool parseIni(const std::string& content, IniMap& out_map);
    void populateConfig(const IniMap& ini, ThemeConfig& cfg);
    
    std::string getString(const IniMap& ini, const std::string& section,
                          const std::string& key, const std::string& def = "") const;
    int getInt(const IniMap& ini, const std::string& section,
               const std::string& key, int def = 0) const;
    bool getBool(const IniMap& ini, const std::string& section,
                 const std::string& key, bool def = false) const;
    double getDouble(const IniMap& ini, const std::string& section,
                     const std::string& key, double def = 0.0) const;

    std::string last_error_;
};

// CSS Generator: ThemeConfig → GTK CSS string
class ThemeCssGenerator {
public:
    ThemeCssGenerator();
    ~ThemeCssGenerator();

    // Tạo CSS từ ThemeConfig
    std::string generate(const ThemeConfig& cfg);

    // Apply CSS vào GTK application
    static void applyToGtk(const std::string& css);

private:
    std::string generateBase(const ThemeConfig& cfg);
    std::string generateNav(const ThemeConfig& cfg);
    std::string generateButtons(const ThemeConfig& cfg);
    std::string generateCards(const ThemeConfig& cfg);
    std::string generateConsole(const ThemeConfig& cfg);
    std::string generateStatusbar(const ThemeConfig& cfg);
    std::string generateSearch(const ThemeConfig& cfg);
    std::string generateScrollbar(const ThemeConfig& cfg);
    std::string generateTooltip(const ThemeConfig& cfg);
    std::string generateDialog(const ThemeConfig& cfg);
    std::string generateBadge(const ThemeConfig& cfg);
    std::string generateAnimations(const ThemeConfig& cfg);
    std::string generateExtra(const ThemeConfig& cfg);

    // Helper: px suffix
    static std::string px(int v) { return std::to_string(v) + "px"; }
    // Helper: transition string
    std::string transition(const ThemeConfig& cfg, const std::string& props = "all");
};

// Theme Manager: quản lý load/save/switch theme
class ThemeManager {
public:
    static ThemeManager& instance();

    // Load và apply theme từ file
    bool loadTheme(const std::string& cfg_path);
    
    // Load và apply theme mặc định (classic-ivory)
    void loadDefaultTheme();
    
    // Apply theme hiện tại vào GTK
    void applyCurrentTheme();
    
    // Lấy theme hiện tại
    const ThemeConfig& currentTheme() const { return current_theme_; }
    
    // Lấy CSS đang dùng
    const std::string& currentCss() const { return current_css_; }
    
    // Liệt kê các file .cfg trong thư mục themes
    std::vector<std::string> listThemeFiles(const std::string& dir) const;

    // Callback khi theme thay đổi
    void onThemeChanged(std::function<void(const ThemeConfig&)> cb) {
        on_changed_callbacks_.push_back(cb);
    }

private:
    ThemeManager() = default;
    ThemeConfig current_theme_;
    std::string current_css_;
    std::vector<std::function<void(const ThemeConfig&)>> on_changed_callbacks_;
};

} // namespace AppExplorer
