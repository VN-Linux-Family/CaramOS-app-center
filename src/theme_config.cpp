// ─────────────────────────────────────────────────────────────────────────────
// App Explorer — theme_config.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "theme_config.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <gtk/gtk.h>

namespace AppExplorer {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}


// ─────────────────────────────────────────────────────────────────────────────
// ThemeConfigParser
// ─────────────────────────────────────────────────────────────────────────────

ThemeConfigParser::ThemeConfigParser() = default;
ThemeConfigParser::~ThemeConfigParser() = default;

bool ThemeConfigParser::loadFromFile(const std::string& path, ThemeConfig& out) {
    std::ifstream f(path);
    if (!f.is_open()) {
        last_error_ = "Cannot open theme file: " + path;
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return loadFromString(ss.str(), out);
}

bool ThemeConfigParser::loadFromString(const std::string& content, ThemeConfig& out) {
    IniMap ini;
    if (!parseIni(content, ini)) return false;
    populateConfig(ini, out);
    return true;
}

bool ThemeConfigParser::parseIni(const std::string& content, IniMap& out) {
    std::istringstream ss(content);
    std::string line;
    std::string current_section;

    while (std::getline(ss, line)) {
        // Xử lý line continuation (\)
        while (!line.empty() && line.back() == '\\') {
            line.pop_back();
            std::string next;
            if (std::getline(ss, next)) {
                line += trim(next);
            }
        }

        line = trim(line);
        if (line.empty()) continue;

        // Strip inline comment (cẩn thận với hex color)
        // Chỉ strip nếu # đứng sau khoảng trắng và không phải hex value
        bool in_value = false;
        for (size_t i = 0; i < line.size(); i++) {
            if (line[i] == '=') { in_value = true; }
            if (line[i] == '#') {
                if (!in_value) {
                    // Comment ở đầu dòng
                    line = "";
                    break;
                } else {
                    // Trong value: chỉ strip nếu đứng sau khoảng trắng
                    if (i > 0 && (line[i-1] == ' ' || line[i-1] == '\t')) {
                        line = trim(line.substr(0, i));
                        break;
                    }
                }
            }
        }

        if (line.empty() || line[0] == ';') continue;

        // Section
        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            trim(current_section);
            continue;
        }

        // Key = Value
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (!key.empty()) {
            out[current_section][key] = val;
        }
    }
    return true;
}

std::string ThemeConfigParser::getString(const IniMap& ini, const std::string& sec,
                                          const std::string& key, const std::string& def) const {
    auto sit = ini.find(sec);
    if (sit == ini.end()) return def;
    auto kit = sit->second.find(key);
    if (kit == sit->second.end()) return def;
    return kit->second.empty() ? def : kit->second;
}

int ThemeConfigParser::getInt(const IniMap& ini, const std::string& sec,
                               const std::string& key, int def) const {
    std::string v = getString(ini, sec, key, "");
    if (v.empty()) return def;
    try { return std::stoi(v); } catch (...) { return def; }
}

bool ThemeConfigParser::getBool(const IniMap& ini, const std::string& sec,
                                 const std::string& key, bool def) const {
    std::string v = getString(ini, sec, key, "");
    if (v.empty()) return def;
    std::string lv = v;
    std::transform(lv.begin(), lv.end(), lv.begin(), ::tolower);
    if (lv == "true" || lv == "1" || lv == "yes") return true;
    if (lv == "false" || lv == "0" || lv == "no") return false;
    return def;
}

double ThemeConfigParser::getDouble(const IniMap& ini, const std::string& sec,
                                     const std::string& key, double def) const {
    std::string v = getString(ini, sec, key, "");
    if (v.empty()) return def;
    try { return std::stod(v); } catch (...) { return def; }
}

void ThemeConfigParser::populateConfig(const IniMap& ini, ThemeConfig& c) {
    auto S = [&](const std::string& s, const std::string& k, const std::string& d = "") {
        return getString(ini, s, k, d);
    };
    auto I = [&](const std::string& s, const std::string& k, int d = 0) {
        return getInt(ini, s, k, d);
    };
    auto B = [&](const std::string& s, const std::string& k, bool d = false) {
        return getBool(ini, s, k, d);
    };
    auto D = [&](const std::string& s, const std::string& k, double d = 0.0) {
        return getDouble(ini, s, k, d);
    };

    // [theme]
    c.name          = S("theme","name","unnamed");
    c.version       = S("theme","version","1.0");
    c.author        = S("theme","author","");
    c.description   = S("theme","description","");
    c.dark_mode     = B("theme","dark_mode", false);
    c.follow_system = B("theme","follow_system", false);

    // [colors.base]
    c.base.bg_primary       = S("colors.base","bg_primary",       c.base.bg_primary);
    c.base.bg_secondary     = S("colors.base","bg_secondary",     c.base.bg_secondary);
    c.base.bg_card          = S("colors.base","bg_card",          c.base.bg_card);
    c.base.bg_card_hover    = S("colors.base","bg_card_hover",    c.base.bg_card_hover);
    c.base.bg_card_selected = S("colors.base","bg_card_selected", c.base.bg_card_selected);
    c.base.bg_elevated      = S("colors.base","bg_elevated",      c.base.bg_elevated);
    c.base.bg_sunken        = S("colors.base","bg_sunken",        c.base.bg_sunken);
    c.base.border_light     = S("colors.base","border_light",     c.base.border_light);
    c.base.border_normal    = S("colors.base","border_normal",    c.base.border_normal);
    c.base.border_focus     = S("colors.base","border_focus",     c.base.border_focus);
    c.base.border_error     = S("colors.base","border_error",     c.base.border_error);

    // [colors.text]
    c.text.primary      = S("colors.text","primary",   c.text.primary);
    c.text.secondary    = S("colors.text","secondary",  c.text.secondary);
    c.text.disabled     = S("colors.text","disabled",   c.text.disabled);
    c.text.on_dark      = S("colors.text","on_dark",    c.text.on_dark);
    c.text.link         = S("colors.text","link",       c.text.link);
    c.text.link_hover   = S("colors.text","link_hover", c.text.link_hover);
    c.text.error        = S("colors.text","error",      c.text.error);
    c.text.success      = S("colors.text","success",    c.text.success);
    c.text.warning      = S("colors.text","warning",    c.text.warning);
    c.text.info         = S("colors.text","info",       c.text.info);

    // [colors.accent]
    c.accent.primary = S("colors.accent","primary", c.accent.primary);
    c.accent.hover   = S("colors.accent","hover",   c.accent.hover);
    c.accent.pressed = S("colors.accent","pressed", c.accent.pressed);
    c.accent.light   = S("colors.accent","light",   c.accent.light);
    c.accent.border  = S("colors.accent","border",  c.accent.border);

    // [colors.nav]
    c.nav.bg               = S("colors.nav","bg",               c.nav.bg);
    c.nav.item_hover_bg    = S("colors.nav","item_hover_bg",    c.nav.item_hover_bg);
    c.nav.item_active_bg   = S("colors.nav","item_active_bg",   c.nav.item_active_bg);
    c.nav.item_text        = S("colors.nav","item_text",        c.nav.item_text);
    c.nav.item_hover_text  = S("colors.nav","item_hover_text",  c.nav.item_hover_text);
    c.nav.item_active_text = S("colors.nav","item_active_text", c.nav.item_active_text);
    c.nav.border           = S("colors.nav","border",           c.nav.border);
    c.nav.shadow           = S("colors.nav","shadow",           c.nav.shadow);

    // [colors.button]
    auto& btn = c.button;
    btn.bg              = S("colors.button","bg",              btn.bg);
    btn.bg_hover        = S("colors.button","bg_hover",        btn.bg_hover);
    btn.bg_pressed      = S("colors.button","bg_pressed",      btn.bg_pressed);
    btn.bg_disabled     = S("colors.button","bg_disabled",     btn.bg_disabled);
    btn.text            = S("colors.button","text",            btn.text);
    btn.text_hover      = S("colors.button","text_hover",      btn.text_hover);
    btn.text_pressed    = S("colors.button","text_pressed",    btn.text_pressed);
    btn.text_disabled   = S("colors.button","text_disabled",   btn.text_disabled);
    btn.border          = S("colors.button","border",          btn.border);
    btn.border_hover    = S("colors.button","border_hover",    btn.border_hover);
    btn.border_pressed  = S("colors.button","border_pressed",  btn.border_pressed);
    btn.border_disabled = S("colors.button","border_disabled", btn.border_disabled);
    btn.danger_bg           = S("colors.button","danger_bg",           btn.danger_bg);
    btn.danger_bg_hover     = S("colors.button","danger_bg_hover",     btn.danger_bg_hover);
    btn.danger_text         = S("colors.button","danger_text",         btn.danger_text);
    btn.danger_text_hover   = S("colors.button","danger_text_hover",   btn.danger_text_hover);
    btn.danger_border       = S("colors.button","danger_border",       btn.danger_border);
    btn.danger_border_hover = S("colors.button","danger_border_hover", btn.danger_border_hover);
    btn.primary_bg          = S("colors.button","primary_bg",          btn.primary_bg);
    btn.primary_bg_hover    = S("colors.button","primary_bg_hover",    btn.primary_bg_hover);
    btn.primary_text        = S("colors.button","primary_text",        btn.primary_text);
    btn.primary_text_hover  = S("colors.button","primary_text_hover",  btn.primary_text_hover);
    btn.primary_border      = S("colors.button","primary_border",      btn.primary_border);
    btn.primary_border_hover= S("colors.button","primary_border_hover",btn.primary_border_hover);

    // [colors.statusbar]
    c.statusbar.bg            = S("colors.statusbar","bg",            c.statusbar.bg);
    c.statusbar.text          = S("colors.statusbar","text",          c.statusbar.text);
    c.statusbar.border        = S("colors.statusbar","border",        c.statusbar.border);
    c.statusbar.progress_bg   = S("colors.statusbar","progress_bg",   c.statusbar.progress_bg);
    c.statusbar.progress_fill = S("colors.statusbar","progress_fill", c.statusbar.progress_fill);

    // [colors.console]
    auto& con = c.console;
    con.bg           = S("colors.console","bg",           con.bg);
    con.text         = S("colors.console","text",         con.text);
    con.text_cmd     = S("colors.console","text_cmd",     con.text_cmd);
    con.text_stdout  = S("colors.console","text_stdout",  con.text_stdout);
    con.text_stderr  = S("colors.console","text_stderr",  con.text_stderr);
    con.text_error   = S("colors.console","text_error",   con.text_error);
    con.text_success = S("colors.console","text_success", con.text_success);
    con.text_info    = S("colors.console","text_info",    con.text_info);
    con.border       = S("colors.console","border",       con.border);
    con.header_bg    = S("colors.console","header_bg",    con.header_bg);
    con.header_text  = S("colors.console","header_text",  con.header_text);

    // [colors.badge]
    c.badge.bg               = S("colors.badge","bg",               c.badge.bg);
    c.badge.text             = S("colors.badge","text",             c.badge.text);
    c.badge.border           = S("colors.badge","border",           c.badge.border);
    c.badge.installed_bg     = S("colors.badge","installed_bg",     c.badge.installed_bg);
    c.badge.installed_text   = S("colors.badge","installed_text",   c.badge.installed_text);
    c.badge.installed_border = S("colors.badge","installed_border", c.badge.installed_border);

    // [colors.scrollbar]
    c.scrollbar.track        = S("colors.scrollbar","track",        c.scrollbar.track);
    c.scrollbar.thumb        = S("colors.scrollbar","thumb",        c.scrollbar.thumb);
    c.scrollbar.thumb_hover  = S("colors.scrollbar","thumb_hover",  c.scrollbar.thumb_hover);
    c.scrollbar.thumb_active = S("colors.scrollbar","thumb_active", c.scrollbar.thumb_active);

    // [colors.tooltip]
    c.tooltip.bg     = S("colors.tooltip","bg",     c.tooltip.bg);
    c.tooltip.text   = S("colors.tooltip","text",   c.tooltip.text);
    c.tooltip.border = S("colors.tooltip","border", c.tooltip.border);

    // [colors.dialog]
    c.dialog.bg          = S("colors.dialog","bg",          c.dialog.bg);
    c.dialog.overlay     = S("colors.dialog","overlay",     c.dialog.overlay);
    c.dialog.header_bg   = S("colors.dialog","header_bg",   c.dialog.header_bg);
    c.dialog.header_text = S("colors.dialog","header_text", c.dialog.header_text);
    c.dialog.border      = S("colors.dialog","border",      c.dialog.border);
    c.dialog.shadow      = S("colors.dialog","shadow",      c.dialog.shadow);

    // [colors.search]
    c.search.bg           = S("colors.search","bg",           c.search.bg);
    c.search.text         = S("colors.search","text",         c.search.text);
    c.search.placeholder  = S("colors.search","placeholder",  c.search.placeholder);
    c.search.border       = S("colors.search","border",       c.search.border);
    c.search.border_focus = S("colors.search","border_focus", c.search.border_focus);
    c.search.icon         = S("colors.search","icon",         c.search.icon);
    c.search.bg_focus     = S("colors.search","bg_focus",     c.search.bg_focus);

    // [typography]
    auto& t = c.typography;
    t.font_ui           = S("typography","font_ui",           t.font_ui);
    t.font_ui_size      = I("typography","font_ui_size",      t.font_ui_size);
    t.font_title        = S("typography","font_title",        t.font_title);
    t.font_title_size   = I("typography","font_title_size",   t.font_title_size);
    t.font_title_weight = S("typography","font_title_weight", t.font_title_weight);
    t.font_subtitle     = S("typography","font_subtitle",     t.font_subtitle);
    t.font_subtitle_size= I("typography","font_subtitle_size",t.font_subtitle_size);
    t.font_mono         = S("typography","font_mono",         t.font_mono);
    t.font_mono_size    = I("typography","font_mono_size",    t.font_mono_size);
    t.font_button       = S("typography","font_button",       t.font_button);
    t.font_button_size  = I("typography","font_button_size",  t.font_button_size);
    t.font_button_weight= S("typography","font_button_weight",t.font_button_weight);
    t.font_nav          = S("typography","font_nav",          t.font_nav);
    t.font_nav_size     = I("typography","font_nav_size",     t.font_nav_size);
    t.font_nav_weight   = S("typography","font_nav_weight",   t.font_nav_weight);
    t.font_badge        = S("typography","font_badge",        t.font_badge);
    t.font_badge_size   = I("typography","font_badge_size",   t.font_badge_size);

    // [layout]
    auto& l = c.layout;
    l.window_width       = I("layout","window_width",       l.window_width);
    l.window_height      = I("layout","window_height",      l.window_height);
    l.window_min_width   = I("layout","window_min_width",   l.window_min_width);
    l.window_min_height  = I("layout","window_min_height",  l.window_min_height);
    l.nav_width          = I("layout","nav_width",          l.nav_width);
    l.nav_item_height    = I("layout","nav_item_height",    l.nav_item_height);
    l.nav_item_padding_h = I("layout","nav_item_padding_h", l.nav_item_padding_h);
    l.nav_item_padding_v = I("layout","nav_item_padding_v", l.nav_item_padding_v);
    l.content_padding    = I("layout","content_padding",    l.content_padding);
    l.content_gap        = I("layout","content_gap",        l.content_gap);
    l.card_width         = I("layout","card_width",         l.card_width);
    l.card_height        = I("layout","card_height",        l.card_height);
    l.card_padding       = I("layout","card_padding",       l.card_padding);
    l.card_gap           = I("layout","card_gap",           l.card_gap);
    l.card_icon_size     = I("layout","card_icon_size",     l.card_icon_size);
    l.list_row_height    = I("layout","list_row_height",    l.list_row_height);
    l.list_icon_size     = I("layout","list_icon_size",     l.list_icon_size);
    l.list_padding_h     = I("layout","list_padding_h",     l.list_padding_h);
    l.list_padding_v     = I("layout","list_padding_v",     l.list_padding_v);
    l.detail_panel_width = I("layout","detail_panel_width", l.detail_panel_width);
    l.console_height     = I("layout","console_height",     l.console_height);
    l.console_min_height = I("layout","console_min_height", l.console_min_height);
    l.console_max_height = I("layout","console_max_height", l.console_max_height);
    l.statusbar_height   = I("layout","statusbar_height",   l.statusbar_height);
    l.toolbar_height     = I("layout","toolbar_height",     l.toolbar_height);
    l.search_width       = I("layout","search_width",       l.search_width);
    l.search_height      = I("layout","search_height",      l.search_height);

    // [radius]
    auto& r = c.radius;
    r.window          = I("radius","window",          r.window);
    r.card            = I("radius","card",            r.card);
    r.button          = I("radius","button",          r.button);
    r.button_small    = I("radius","button_small",    r.button_small);
    r.input           = I("radius","input",           r.input);
    r.tooltip         = I("radius","tooltip",         r.tooltip);
    r.dialog          = I("radius","dialog",          r.dialog);
    r.console         = I("radius","console",         r.console);
    r.image           = I("radius","image",           r.image);
    r.nav_item        = I("radius","nav_item",        r.nav_item);
    r.progress        = I("radius","progress",        r.progress);
    r.scrollbar_thumb = I("radius","scrollbar_thumb", r.scrollbar_thumb);

    // [animation]
    c.animation.enabled        = B("animation","enabled",        c.animation.enabled);
    c.animation.hover_duration = I("animation","hover_duration", c.animation.hover_duration);
    c.animation.transition     = I("animation","transition",     c.animation.transition);
    c.animation.page_fade      = I("animation","page_fade",      c.animation.page_fade);
    c.animation.easing         = S("animation","easing",         c.animation.easing);

    // [misc]
    c.misc.show_console_auto          = B("misc","show_console_auto",           c.misc.show_console_auto);
    c.misc.collapse_console_on_finish = B("misc","collapse_console_on_finish",  c.misc.collapse_console_on_finish);
    c.misc.default_view               = S("misc","default_view",                c.misc.default_view);
    c.misc.grid_columns               = I("misc","grid_columns",                c.misc.grid_columns);
    c.misc.show_category_badge        = B("misc","show_category_badge",         c.misc.show_category_badge);
    c.misc.show_installed_badge       = B("misc","show_installed_badge",        c.misc.show_installed_badge);
    c.misc.uninstalled_icon_opacity   = D("misc","uninstalled_icon_opacity",    c.misc.uninstalled_icon_opacity);

    // [gtk_extra]
    c.gtk_extra.extra_css_file = S("gtk_extra","extra_css_file","");
    c.gtk_extra.inline_css     = S("gtk_extra","inline_css","");
}

// ─────────────────────────────────────────────────────────────────────────────
// ThemeCssGenerator
// ─────────────────────────────────────────────────────────────────────────────

ThemeCssGenerator::ThemeCssGenerator() = default;
ThemeCssGenerator::~ThemeCssGenerator() = default;

std::string ThemeCssGenerator::transition(const ThemeConfig& cfg, const std::string& props) {
    if (!cfg.animation.enabled) return "";
    return props + " " + std::to_string(cfg.animation.hover_duration) + "ms " + cfg.animation.easing;
}

std::string ThemeCssGenerator::generate(const ThemeConfig& cfg) {
    std::string css;
    css += generateBase(cfg);
    css += generateNav(cfg);
    css += generateButtons(cfg);
    css += generateCards(cfg);
    css += generateSearch(cfg);
    css += generateConsole(cfg);
    css += generateStatusbar(cfg);
    css += generateScrollbar(cfg);
    css += generateTooltip(cfg);
    css += generateDialog(cfg);
    css += generateBadge(cfg);
    css += generateAnimations(cfg);
    css += generateExtra(cfg);
    return css;
}

std::string ThemeCssGenerator::generateBase(const ThemeConfig& cfg) {
    const auto& b = cfg.base;
    const auto& t = cfg.text;
    const auto& ty = cfg.typography;
    std::ostringstream css;
    css << "/* ── Base ─────────────────────────────── */\n";
    css << "* {\n";
    css << "  -GtkWidget-focus-line-width: 1;\n";
    css << "  -GtkWidget-focus-padding: 0;\n";
    css << "  outline: none;\n";
    css << "}\n";
    css << "window, .app-explorer-window {\n";
    css << "  background-color: " << b.bg_primary << ";\n";
    css << "  color: " << t.primary << ";\n";
    css << "  font-family: \"" << ty.font_ui << "\";\n";
    css << "  font-size: " << ty.font_ui_size << "pt;\n";
    if (cfg.radius.window > 0)
        css << "  border-radius: " << px(cfg.radius.window) << ";\n";
    css << "}\n";
    css << ".content-area {\n";
    css << "  background-color: " << b.bg_primary << ";\n";
    css << "  padding: " << px(cfg.layout.content_padding) << ";\n";
    css << "}\n";
    css << "separator { background-color: " << b.border_light << "; min-height: 1px; min-width: 1px; }\n";
    css << ".app-title {\n";
    css << "  font-family: \"" << ty.font_title << "\";\n";
    css << "  font-size: " << ty.font_title_size << "pt;\n";
    css << "  font-weight: " << ty.font_title_weight << ";\n";
    css << "  color: " << t.primary << ";\n";
    css << "}\n";
    css << ".app-subtitle {\n";
    css << "  font-family: \"" << ty.font_subtitle << "\";\n";
    css << "  font-size: " << ty.font_subtitle_size << "pt;\n";
    css << "  color: " << t.secondary << ";\n";
    css << "}\n";
    css << ".text-muted { color: " << t.secondary << "; }\n";
    css << ".text-disabled { color: " << t.disabled << "; }\n";
    css << ".text-error { color: " << t.error << "; }\n";
    css << ".text-success { color: " << t.success << "; }\n";
    css << ".text-warning { color: " << t.warning << "; }\n";
    css << ".text-link {\n";
    css << "  color: " << t.link << ";\n";
    css << "  text-decoration: underline;\n";
    css << "}\n";
    css << ".text-link:hover { color: " << t.link_hover << "; }\n";
    return css.str();
}

std::string ThemeCssGenerator::generateNav(const ThemeConfig& cfg) {
    const auto& n = cfg.nav;
    const auto& ty = cfg.typography;
    const auto& r  = cfg.radius;
    const auto& l  = cfg.layout;
    std::ostringstream css;
    css << "/* ── Navigation Sidebar ───────────────── */\n";
    css << ".nav-sidebar {\n";
    css << "  background-color: " << n.bg << ";\n";
    css << "  min-width: " << px(l.nav_width) << ";\n";
    css << "  max-width: " << px(l.nav_width) << ";\n";
    css << "  border-right: 1px solid " << n.border << ";\n";
    css << "}\n";
    css << ".nav-logo {\n";
    css << "  padding: 16px " << px(l.nav_item_padding_h) << " 8px;\n";
    css << "  color: " << n.item_text << ";\n";
    css << "  font-family: \"" << ty.font_title << "\";\n";
    css << "  font-size: " << (ty.font_title_size + 2) << "pt;\n";
    css << "  font-weight: bold;\n";
    css << "  letter-spacing: 1px;\n";
    css << "}\n";
    css << ".nav-item {\n";
    css << "  background-color: transparent;\n";
    css << "  color: " << n.item_text << ";\n";
    css << "  border: none;\n";
    css << "  border-radius: " << px(r.nav_item) << ";\n";
    css << "  padding: " << px(l.nav_item_padding_v) << " " << px(l.nav_item_padding_h) << ";\n";
    css << "  min-height: " << px(l.nav_item_height) << ";\n";
    css << "  font-family: \"" << ty.font_nav << "\";\n";
    css << "  font-size: " << ty.font_nav_size << "pt;\n";
    css << "  font-weight: " << ty.font_nav_weight << ";\n";
    css << "  text-align: left;\n";
    css << "  transition: " << transition(cfg, "background-color, color") << ";\n";
    css << "}\n";
    css << ".nav-item:hover {\n";
    css << "  background-color: " << n.item_hover_bg << ";\n";
    css << "  color: " << n.item_hover_text << ";\n";
    css << "}\n";
    css << ".nav-item.active, .nav-item:active {\n";
    css << "  background-color: " << n.item_active_bg << ";\n";
    css << "  color: " << n.item_active_text << ";\n";
    css << "}\n";
    css << ".nav-separator {\n";
    css << "  background-color: " << n.border << ";\n";
    css << "  min-height: 1px;\n";
    css << "  margin: 4px " << px(l.nav_item_padding_h) << ";\n";
    css << "}\n";
    css << ".nav-badge {\n";
    css << "  background-color: " << cfg.accent.light << ";\n";
    css << "  color: " << cfg.accent.primary << ";\n";
    css << "  border-radius: 10px;\n";
    css << "  padding: 1px 6px;\n";
    css << "  font-size: 8pt;\n";
    css << "  font-weight: bold;\n";
    css << "}\n";
    return css.str();
}

std::string ThemeCssGenerator::generateButtons(const ThemeConfig& cfg) {
    const auto& b  = cfg.button;
    const auto& r  = cfg.radius;
    const auto& ty = cfg.typography;
    std::ostringstream css;
    css << "/* ── Buttons ──────────────────────────── */\n";
    css << "button, .btn {\n";
    css << "  background-color: " << b.bg << ";\n";
    css << "  color: " << b.text << ";\n";
    css << "  border: 1px solid " << b.border << ";\n";
    css << "  border-radius: " << px(r.button) << ";\n";
    css << "  padding: 6px 14px;\n";
    css << "  font-family: \"" << ty.font_button << "\";\n";
    css << "  font-size: " << ty.font_button_size << "pt;\n";
    css << "  font-weight: " << ty.font_button_weight << ";\n";
    css << "  transition: " << transition(cfg, "background-color, color, border-color") << ";\n";
    css << "  box-shadow: none;\n";
    css << "  -gtk-icon-shadow: none;\n";
    css << "  text-shadow: none;\n";
    css << "}\n";
    css << "button:hover, .btn:hover {\n";
    css << "  background-color: " << b.bg_hover << ";\n";
    css << "  color: " << b.text_hover << ";\n";
    css << "  border-color: " << b.border_hover << ";\n";
    css << "}\n";
    css << "button:active, .btn:active {\n";
    css << "  background-color: " << b.bg_pressed << ";\n";
    css << "  color: " << b.text_pressed << ";\n";
    css << "  border-color: " << b.border_pressed << ";\n";
    css << "}\n";
    css << "button:disabled, .btn:disabled {\n";
    css << "  background-color: " << b.bg_disabled << ";\n";
    css << "  color: " << b.text_disabled << ";\n";
    css << "  border-color: " << b.border_disabled << ";\n";
    css << "}\n";
    css << "button:focus, .btn:focus {\n";
    css << "  border-color: " << cfg.base.border_focus << ";\n";
    css << "  outline: 2px solid " << cfg.base.border_focus << ";\n";
    css << "  outline-offset: 1px;\n";
    css << "}\n";
    // Primary button
    css << ".btn-primary {\n";
    css << "  background-color: " << b.primary_bg << ";\n";
    css << "  color: " << b.primary_text << ";\n";
    css << "  border-color: " << b.primary_border << ";\n";
    css << "  font-weight: bold;\n";
    css << "}\n";
    css << ".btn-primary:hover {\n";
    css << "  background-color: " << b.primary_bg_hover << ";\n";
    css << "  color: " << b.primary_text_hover << ";\n";
    css << "  border-color: " << b.primary_border_hover << ";\n";
    css << "}\n";
    // Danger button
    css << ".btn-danger {\n";
    css << "  background-color: " << b.danger_bg << ";\n";
    css << "  color: " << b.danger_text << ";\n";
    css << "  border-color: " << b.danger_border << ";\n";
    css << "}\n";
    css << ".btn-danger:hover {\n";
    css << "  background-color: " << b.danger_bg_hover << ";\n";
    css << "  color: " << b.danger_text_hover << ";\n";
    css << "  border-color: " << b.danger_border_hover << ";\n";
    css << "}\n";
    // Small button
    css << ".btn-small {\n";
    css << "  padding: 3px 8px;\n";
    css << "  border-radius: " << px(r.button_small) << ";\n";
    css << "  font-size: " << (ty.font_button_size - 1) << "pt;\n";
    css << "}\n";
    // Icon-only button
    css << ".btn-icon {\n";
    css << "  padding: 4px;\n";
    css << "  min-width: 28px;\n";
    css << "  min-height: 28px;\n";
    css << "  border-radius: " << px(r.button) << ";\n";
    css << "}\n";
    return css.str();
}

std::string ThemeCssGenerator::generateCards(const ThemeConfig& cfg) {
    const auto& b = cfg.base;
    const auto& r = cfg.radius;
    const auto& l = cfg.layout;
    const auto& t = cfg.text;
    const auto& ty = cfg.typography;
    std::ostringstream css;
    css << "/* ── App Cards ────────────────────────── */\n";
    css << ".app-card {\n";
    css << "  background-color: " << b.bg_card << ";\n";
    css << "  border: 1px solid " << b.border_light << ";\n";
    css << "  border-radius: " << px(r.card) << ";\n";
    css << "  padding: " << px(l.card_padding) << ";\n";
    css << "  min-width: " << px(l.card_width) << ";\n";
    css << "  max-width: " << px(l.card_width) << ";\n";
    css << "  min-height: " << px(l.card_height) << ";\n";
    css << "  transition: " << transition(cfg, "background-color, border-color, box-shadow") << ";\n";
    css << "}\n";
    css << ".app-card:hover {\n";
    css << "  background-color: " << b.bg_card_hover << ";\n";
    css << "  border-color: " << b.border_normal << ";\n";
    css << "}\n";
    css << ".app-card.selected {\n";
    css << "  background-color: " << b.bg_card_selected << ";\n";
    css << "  border-color: " << cfg.accent.border << ";\n";
    css << "  border-width: 1px;\n";
    css << "}\n";
    css << ".app-card-name {\n";
    css << "  font-family: \"" << ty.font_ui << "\";\n";
    css << "  font-size: " << ty.font_ui_size << "pt;\n";
    css << "  font-weight: bold;\n";
    css << "  color: " << t.primary << ";\n";
    css << "  margin-top: 8px;\n";
    css << "}\n";
    css << ".app-card-desc {\n";
    css << "  font-family: \"" << ty.font_subtitle << "\";\n";
    css << "  font-size: " << ty.font_subtitle_size << "pt;\n";
    css << "  color: " << t.secondary << ";\n";
    css << "  margin-top: 4px;\n";
    css << "}\n";
    // List row
    css << ".app-list-row {\n";
    css << "  background-color: " << b.bg_card << ";\n";
    css << "  border-bottom: 1px solid " << b.border_light << ";\n";
    css << "  padding: " << px(l.list_padding_v) << " " << px(l.list_padding_h) << ";\n";
    css << "  min-height: " << px(l.list_row_height) << ";\n";
    css << "  transition: " << transition(cfg, "background-color") << ";\n";
    css << "}\n";
    css << ".app-list-row:hover {\n";
    css << "  background-color: " << b.bg_card_hover << ";\n";
    css << "}\n";
    css << ".app-list-row.selected {\n";
    css << "  background-color: " << b.bg_card_selected << ";\n";
    css << "}\n";
    // Detail panel
    css << ".detail-panel {\n";
    css << "  background-color: " << b.bg_elevated << ";\n";
    css << "  border-left: 1px solid " << b.border_light << ";\n";
    css << "  min-width: " << px(l.detail_panel_width) << ";\n";
    css << "  max-width: " << px(l.detail_panel_width) << ";\n";
    css << "  padding: " << px(l.content_padding) << ";\n";
    css << "}\n";
    css << ".detail-app-name {\n";
    css << "  font-family: \"" << ty.font_title << "\";\n";
    css << "  font-size: " << (ty.font_title_size + 2) << "pt;\n";
    css << "  font-weight: " << ty.font_title_weight << ";\n";
    css << "  color: " << t.primary << ";\n";
    css << "}\n";
    css << ".detail-section-title {\n";
    css << "  font-weight: bold;\n";
    css << "  color: " << t.secondary << ";\n";
    css << "  font-size: " << (ty.font_ui_size - 1) << "pt;\n";
    css << "  text-transform: uppercase;\n";
    css << "  letter-spacing: 0.5px;\n";
    css << "  margin-top: 12px;\n";
    css << "  margin-bottom: 4px;\n";
    css << "}\n";
    return css.str();
}

std::string ThemeCssGenerator::generateSearch(const ThemeConfig& cfg) {
    const auto& s  = cfg.search;
    const auto& r  = cfg.radius;
    const auto& l  = cfg.layout;
    const auto& ty = cfg.typography;
    std::ostringstream css;
    css << "/* ── Search ───────────────────────────── */\n";
    css << ".toolbar {\n";
    css << "  background-color: " << cfg.base.bg_secondary << ";\n";
    css << "  border-bottom: 1px solid " << cfg.base.border_light << ";\n";
    css << "  min-height: " << px(l.toolbar_height) << ";\n";
    css << "  padding: 8px " << px(l.content_padding) << ";\n";
    css << "}\n";
    css << ".toolbar-title {\n";
    css << "  font-family: \"" << ty.font_title << "\";\n";
    css << "  font-size: " << ty.font_title_size << "pt;\n";
    css << "  font-weight: " << ty.font_title_weight << ";\n";
    css << "  color: " << cfg.text.primary << ";\n";
    css << "}\n";
    css << "searchentry, .search-field {\n";
    css << "  background-color: " << s.bg << ";\n";
    css << "  color: " << s.text << ";\n";
    css << "  border: 1px solid " << s.border << ";\n";
    css << "  border-radius: " << px(r.input) << ";\n";
    css << "  padding: 4px 10px;\n";
    css << "  min-width: " << px(l.search_width) << ";\n";
    css << "  min-height: " << px(l.search_height) << ";\n";
    css << "  font-family: \"" << ty.font_ui << "\";\n";
    css << "  font-size: " << ty.font_ui_size << "pt;\n";
    css << "  transition: " << transition(cfg, "border-color, background-color") << ";\n";
    css << "}\n";
    css << "searchentry:focus, .search-field:focus {\n";
    css << "  background-color: " << s.bg_focus << ";\n";
    css << "  border-color: " << s.border_focus << ";\n";
    css << "  outline: none;\n";
    css << "}\n";
    css << "searchentry placeholder { color: " << s.placeholder << "; }\n";
    return css.str();
}

std::string ThemeCssGenerator::generateConsole(const ThemeConfig& cfg) {
    const auto& con = cfg.console;
    const auto& r   = cfg.radius;
    const auto& ty  = cfg.typography;
    std::ostringstream css;
    css << "/* ── Console ──────────────────────────── */\n";
    css << ".console-view {\n";
    css << "  background-color: " << con.bg << ";\n";
    css << "  border-top: 1px solid " << con.border << ";\n";
    css << "  border-radius: 0 0 " << px(r.console) << " " << px(r.console) << ";\n";
    css << "}\n";
    css << ".console-header {\n";
    css << "  background-color: " << con.header_bg << ";\n";
    css << "  color: " << con.header_text << ";\n";
    css << "  padding: 4px 12px;\n";
    css << "  border-bottom: 1px solid " << con.border << ";\n";
    css << "  font-family: \"" << ty.font_mono << "\";\n";
    css << "  font-size: " << (ty.font_mono_size - 1) << "pt;\n";
    css << "  font-weight: bold;\n";
    css << "  letter-spacing: 0.5px;\n";
    css << "}\n";
    css << ".console-text {\n";
    css << "  background-color: " << con.bg << ";\n";
    css << "  color: " << con.text << ";\n";
    css << "  font-family: \"" << ty.font_mono << "\";\n";
    css << "  font-size: " << ty.font_mono_size << "pt;\n";
    css << "  padding: 8px 12px;\n";
    css << "  border: none;\n";
    css << "  caret-color: " << con.text_cmd << ";\n";
    css << "}\n";
    // Text tags phải set qua GtkTextTag trong code C++
    // Nhưng các class helper cho console
    css << ".console-line-cmd     { color: " << con.text_cmd     << "; }\n";
    css << ".console-line-stdout  { color: " << con.text_stdout  << "; }\n";
    css << ".console-line-stderr  { color: " << con.text_stderr  << "; }\n";
    css << ".console-line-error   { color: " << con.text_error   << "; }\n";
    css << ".console-line-success { color: " << con.text_success << "; }\n";
    css << ".console-line-info    { color: " << con.text_info    << "; }\n";
    return css.str();
}

std::string ThemeCssGenerator::generateStatusbar(const ThemeConfig& cfg) {
    const auto& s  = cfg.statusbar;
    const auto& ty = cfg.typography;
    std::ostringstream css;
    css << "/* ── Status Bar ───────────────────────── */\n";
    css << ".statusbar {\n";
    css << "  background-color: " << s.bg << ";\n";
    css << "  color: " << s.text << ";\n";
    css << "  border-top: 1px solid " << s.border << ";\n";
    css << "  min-height: " << px(cfg.layout.statusbar_height) << ";\n";
    css << "  padding: 0 " << px(cfg.layout.content_padding) << ";\n";
    css << "  font-family: \"" << ty.font_subtitle << "\";\n";
    css << "  font-size: " << ty.font_subtitle_size << "pt;\n";
    css << "}\n";
    css << "progressbar.statusbar-progress trough {\n";
    css << "  background-color: " << s.progress_bg << ";\n";
    css << "  border-radius: " << px(cfg.radius.progress) << ";\n";
    css << "  min-height: 4px;\n";
    css << "}\n";
    css << "progressbar.statusbar-progress progress {\n";
    css << "  background-color: " << s.progress_fill << ";\n";
    css << "  border-radius: " << px(cfg.radius.progress) << ";\n";
    css << "  min-height: 4px;\n";
    css << "  transition: " << transition(cfg, "all") << ";\n";
    css << "}\n";
    return css.str();
}

std::string ThemeCssGenerator::generateScrollbar(const ThemeConfig& cfg) {
    const auto& s = cfg.scrollbar;
    std::ostringstream css;
    css << "/* ── Scrollbar ────────────────────────── */\n";
    css << "scrollbar {\n";
    css << "  background-color: " << s.track << ";\n";
    css << "  border: none;\n";
    css << "}\n";
    css << "scrollbar slider {\n";
    css << "  background-color: " << s.thumb << ";\n";
    css << "  border-radius: " << px(cfg.radius.scrollbar_thumb) << ";\n";
    css << "  min-width: 6px;\n";
    css << "  min-height: 6px;\n";
    css << "  transition: " << transition(cfg, "background-color") << ";\n";
    css << "}\n";
    css << "scrollbar slider:hover { background-color: " << s.thumb_hover << "; }\n";
    css << "scrollbar slider:active { background-color: " << s.thumb_active << "; }\n";
    css << "scrollbar.vertical { border-left: 1px solid " << cfg.base.border_light << "; }\n";
    css << "scrollbar.horizontal { border-top: 1px solid " << cfg.base.border_light << "; }\n";
    return css.str();
}

std::string ThemeCssGenerator::generateTooltip(const ThemeConfig& cfg) {
    const auto& t = cfg.tooltip;
    std::ostringstream css;
    css << "/* ── Tooltip ──────────────────────────── */\n";
    css << "tooltip {\n";
    css << "  background-color: " << t.bg << ";\n";
    css << "  color: " << t.text << ";\n";
    css << "  border: 1px solid " << t.border << ";\n";
    css << "  border-radius: " << px(cfg.radius.tooltip) << ";\n";
    css << "  padding: 4px 8px;\n";
    css << "  font-size: " << (cfg.typography.font_ui_size - 1) << "pt;\n";
    css << "}\n";
    css << "tooltip decoration { background-color: transparent; box-shadow: none; }\n";
    return css.str();
}

std::string ThemeCssGenerator::generateDialog(const ThemeConfig& cfg) {
    const auto& d = cfg.dialog;
    std::ostringstream css;
    css << "/* ── Dialog ───────────────────────────── */\n";
    css << "dialog {\n";
    css << "  background-color: " << d.bg << ";\n";
    css << "  border: 1px solid " << d.border << ";\n";
    css << "  border-radius: " << px(cfg.radius.dialog) << ";\n";
    css << "}\n";
    css << ".dialog-header {\n";
    css << "  background-color: " << d.header_bg << ";\n";
    css << "  color: " << d.header_text << ";\n";
    css << "  padding: 12px 16px;\n";
    css << "  border-bottom: 1px solid " << d.border << ";\n";
    css << "  border-radius: " << px(cfg.radius.dialog) << " " << px(cfg.radius.dialog) << " 0 0;\n";
    css << "  font-weight: bold;\n";
    css << "}\n";
    css << ".dialog-content { padding: 16px; }\n";
    css << ".dialog-actions {\n";
    css << "  padding: 8px 16px 12px;\n";
    css << "  border-top: 1px solid " << d.border << ";\n";
    css << "}\n";
    css << "messagedialog .message-dialog-content { background-color: " << d.bg << "; }\n";
    return css.str();
}

std::string ThemeCssGenerator::generateBadge(const ThemeConfig& cfg) {
    const auto& b  = cfg.badge;
    const auto& r  = cfg.radius;
    const auto& ty = cfg.typography;
    std::ostringstream css;
    css << "/* ── Badges & Tags ────────────────────── */\n";
    css << ".badge {\n";
    css << "  background-color: " << b.bg << ";\n";
    css << "  color: " << b.text << ";\n";
    css << "  border: 1px solid " << b.border << ";\n";
    css << "  border-radius: " << px(r.button_small) << ";\n";
    css << "  padding: 1px 6px;\n";
    css << "  font-family: \"" << ty.font_badge << "\";\n";
    css << "  font-size: " << ty.font_badge_size << "pt;\n";
    css << "  font-weight: bold;\n";
    css << "}\n";
    css << ".badge-installed {\n";
    css << "  background-color: " << b.installed_bg << ";\n";
    css << "  color: " << b.installed_text << ";\n";
    css << "  border-color: " << b.installed_border << ";\n";
    css << "}\n";
    css << ".tag {\n";
    css << "  background-color: " << cfg.base.bg_sunken << ";\n";
    css << "  color: " << cfg.text.secondary << ";\n";
    css << "  border: 1px solid " << cfg.base.border_light << ";\n";
    css << "  border-radius: " << px(r.button_small) << ";\n";
    css << "  padding: 1px 5px;\n";
    css << "  font-size: " << (ty.font_badge_size - 1) << "pt;\n";
    css << "}\n";
    css << ".installed-overlay {\n";
    css << "  background-color: " << b.installed_bg << ";\n";
    css << "  color: " << b.installed_text << ";\n";
    css << "  border: 1px solid " << b.installed_border << ";\n";
    css << "  border-radius: 0 0 " << px(r.button_small) << " " << px(r.button_small) << ";\n";
    css << "  padding: 2px 4px;\n";
    css << "  font-size: 7pt;\n";
    css << "  font-weight: bold;\n";
    css << "}\n";
    return css.str();
}

std::string ThemeCssGenerator::generateAnimations(const ThemeConfig& cfg) {
    if (!cfg.animation.enabled) return "";
    std::ostringstream css;
    css << "/* ── Animations ───────────────────────── */\n";
    int fade = cfg.animation.page_fade;
    std::string ease = cfg.animation.easing;
    css << "@keyframes ae-fade-in {\n";
    css << "  from { opacity: 0; }\n";
    css << "  to   { opacity: 1; }\n";
    css << "}\n";
    css << ".ae-fade-in {\n";
    css << "  animation: ae-fade-in " << fade << "ms " << ease << ";\n";
    css << "}\n";
    css << "spinner { color: " << cfg.accent.primary << "; }\n";
    css << "spinner.spinning { animation: spin 0.8s linear infinite; }\n";
    return css.str();
}

std::string ThemeCssGenerator::generateExtra(const ThemeConfig& cfg) {
    std::string css;
    // Inline CSS từ cfg
    if (!cfg.gtk_extra.inline_css.empty()) {
        css += "\n/* ── gtk_extra.inline_css ─────────────── */\n";
        css += cfg.gtk_extra.inline_css + "\n";
    }
    // File CSS ngoài
    if (!cfg.gtk_extra.extra_css_file.empty()) {
        std::ifstream f(cfg.gtk_extra.extra_css_file);
        if (f.is_open()) {
            css += "\n/* ── gtk_extra.extra_css_file ─────────── */\n";
            std::stringstream ss;
            ss << f.rdbuf();
            css += ss.str() + "\n";
        }
    }
    return css;
}

// Apply CSS vào GTK
void ThemeCssGenerator::applyToGtk(const std::string& css) {
    GtkCssProvider* provider = gtk_css_provider_new();
    GError* err = nullptr;
    gtk_css_provider_load_from_data(provider, css.c_str(), (gssize)css.size(), &err);
    if (err) {
        g_warning("App Explorer CSS error: %s", err->message);
        g_error_free(err);
    }
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);
}

// ─────────────────────────────────────────────────────────────────────────────
// ThemeManager
// ─────────────────────────────────────────────────────────────────────────────

ThemeManager& ThemeManager::instance() {
    static ThemeManager inst;
    return inst;
}

bool ThemeManager::loadTheme(const std::string& cfg_path) {
    ThemeConfigParser parser;
    ThemeConfig new_cfg;
    if (!parser.loadFromFile(cfg_path, new_cfg)) {
        g_warning("Failed to load theme: %s", parser.getLastError().c_str());
        return false;
    }
    current_theme_ = new_cfg;
    ThemeCssGenerator gen;
    current_css_ = gen.generate(current_theme_);
    ThemeCssGenerator::applyToGtk(current_css_);
    for (auto& cb : on_changed_callbacks_) cb(current_theme_);
    return true;
}

void ThemeManager::loadDefaultTheme() {
    // Tạo default theme (classic-ivory) bằng struct mặc định
    ThemeConfig def;
    def.name = "classic-ivory";
    ThemeCssGenerator gen;
    current_theme_ = def;
    current_css_   = gen.generate(def);
    ThemeCssGenerator::applyToGtk(current_css_);
    for (auto& cb : on_changed_callbacks_) cb(current_theme_);
}

void ThemeManager::applyCurrentTheme() {
    ThemeCssGenerator::applyToGtk(current_css_);
}

std::vector<std::string> ThemeManager::listThemeFiles(const std::string& dir) const {
    std::vector<std::string> result;
    GDir* d = g_dir_open(dir.c_str(), 0, nullptr);
    if (!d) return result;
    const gchar* fname;
    while ((fname = g_dir_read_name(d)) != nullptr) {
        std::string name(fname);
        if (name.size() > 4 && name.substr(name.size()-4) == ".cfg") {
            result.push_back(dir + "/" + name);
        }
    }
    g_dir_close(d);
    return result;
}

} // namespace AppExplorer
