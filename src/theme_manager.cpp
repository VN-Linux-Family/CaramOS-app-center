#include "theme_manager.h"
#include <gtk/gtk.h>
#include <sstream>

ThemeManager::ThemeManager()
    : provider_(gtk_css_provider_new()) {}

ThemeManager::~ThemeManager() {
    if (provider_) g_object_unref(provider_);
}

void ThemeManager::set_custom_colors(const CustomThemeColors& c) {
    custom_colors_ = c;
}

std::string ThemeManager::build_css(Theme t, const CustomThemeColors* c) const {
    switch (t) {
        case Theme::LIGHT:  return light_css();
        case Theme::DARK:   return dark_css();
        case Theme::CUSTOM: return c ? custom_css(*c) : custom_css(custom_colors_);
    }
    return dark_css();
}

void ThemeManager::apply(GtkWidget* window, Theme t, const CustomThemeColors* c) {
    current_ = t;
    std::string css = build_css(t, c);

    GError* err = nullptr;
    gtk_css_provider_load_from_data(provider_, css.c_str(), -1, &err);
    if (err) {
        g_warning("CSS error: %s", err->message);
        g_error_free(err);
    }

    GdkScreen* screen = gtk_widget_get_screen(window);
    gtk_style_context_add_provider_for_screen(
        screen,
        GTK_STYLE_PROVIDER(provider_),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

// ── Helper macro ─────────────────────────────────────────────
#define CSS_BODY(BG1, BG2, CARD, ACCENT, TXT1, TXT2, BORDER, SUCCESS, WARN, BTNBG, BTNTXT) \
    "window, .main-window { background-color: " BG1 "; }\n" \
    ".sidebar { background-color: " BG2 "; border-right: 1px solid " BORDER "; }\n" \
    ".header { background-color: " BG2 "; border-bottom: 1px solid " BORDER "; padding: 8px 16px; }\n" \
    ".app-card { background-color: " CARD "; border-radius: 12px; border: 1px solid " BORDER ";\n" \
    "  padding: 16px; margin: 6px; transition: all 200ms ease; }\n" \
    ".app-card:hover { border-color: " ACCENT "; box-shadow: 0 4px 20px rgba(0,0,0,0.3); }\n" \
    ".app-card.selected { border-color: " ACCENT "; background-color: " BG2 "; }\n" \
    ".app-name { color: " TXT1 "; font-weight: bold; font-size: 14px; }\n" \
    ".app-version { color: " TXT2 "; font-size: 11px; }\n" \
    ".app-desc { color: " TXT2 "; font-size: 12px; }\n" \
    ".category-btn { color: " TXT1 "; background: transparent; border: none;\n" \
    "  padding: 8px 16px; border-radius: 8px; }\n" \
    ".category-btn:hover { background-color: " CARD "; }\n" \
    ".category-btn.active { background-color: " ACCENT "; color: " BTNTXT "; }\n" \
    ".install-btn { background-color: " ACCENT "; color: " BTNTXT ";\n" \
    "  border-radius: 8px; padding: 10px 24px; font-weight: bold; border: none; font-size: 14px; }\n" \
    ".install-btn:hover { opacity: 0.9; }\n" \
    ".install-btn:disabled { opacity: 0.5; }\n" \
    ".log-view { background-color: " BG2 "; color: " TXT1 "; font-family: monospace; font-size: 12px; }\n" \
    ".search-entry { background-color: " CARD "; color: " TXT1 "; border: 1px solid " BORDER ";\n" \
    "  border-radius: 8px; padding: 6px 12px; }\n" \
    ".app-title { color: " ACCENT "; font-size: 22px; font-weight: bold; }\n" \
    ".badge-installed { background-color: " SUCCESS "; color: #fff; border-radius: 4px;\n" \
    "  padding: 2px 6px; font-size: 10px; font-weight: bold; }\n" \
    "checkbutton check { background-color: " CARD "; border: 1px solid " BORDER "; border-radius: 4px; }\n" \
    "checkbutton:checked check { background-color: " ACCENT "; border-color: " ACCENT "; }\n" \
    ".section-label { color: " TXT2 "; font-size: 11px; font-weight: bold;\n" \
    "  letter-spacing: 1px; padding: 8px 16px 4px; }\n" \
    ".panel-label { color: " TXT1 "; font-size: 13px; font-weight: bold; }\n"

std::string ThemeManager::dark_css() const {
    return CSS_BODY(
        "#12131a", "#1a1b26", "#1e2030",
        "#7aa2f7", "#c0caf5", "#565f89",
        "#2f3249", "#9ece6a", "#e0af68",
        "#7aa2f7", "#1a1b26"
    );
}

std::string ThemeManager::light_css() const {
    return CSS_BODY(
        "#f5f5f7", "#ffffff", "#e8eaf0",
        "#1a73e8", "#1c1e26", "#6e7382",
        "#d0d3de", "#34a853", "#f9ab00",
        "#1a73e8", "#ffffff"
    );
}

std::string ThemeManager::custom_css(const CustomThemeColors& c) const {
    // Build bằng cách thay trực tiếp – tái dùng macro không tiện với biến runtime
    std::ostringstream ss;
    ss << "window, .main-window { background-color: " << c.bg_primary << "; }\n"
       << ".sidebar { background-color: " << c.bg_secondary << "; border-right: 1px solid " << c.border << "; }\n"
       << ".header { background-color: " << c.bg_secondary << "; border-bottom: 1px solid " << c.border << "; padding: 8px 16px; }\n"
       << ".app-card { background-color: " << c.bg_card << "; border-radius: 12px; border: 1px solid " << c.border << ";\n"
       << "  padding: 16px; margin: 6px; }\n"
       << ".app-card:hover { border-color: " << c.accent << "; }\n"
       << ".app-card.selected { border-color: " << c.accent << "; background-color: " << c.bg_secondary << "; }\n"
       << ".app-name { color: " << c.text_primary << "; font-weight: bold; font-size: 14px; }\n"
       << ".app-version { color: " << c.text_secondary << "; font-size: 11px; }\n"
       << ".app-desc { color: " << c.text_secondary << "; font-size: 12px; }\n"
       << ".category-btn { color: " << c.text_primary << "; background: transparent; border: none; padding: 8px 16px; border-radius: 8px; }\n"
       << ".category-btn:hover { background-color: " << c.bg_card << "; }\n"
       << ".category-btn.active { background-color: " << c.accent << "; color: " << c.button_text << "; }\n"
       << ".install-btn { background-color: " << c.button_bg << "; color: " << c.button_text << ";\n"
       << "  border-radius: 8px; padding: 10px 24px; font-weight: bold; border: none; font-size: 14px; }\n"
       << ".install-btn:hover { opacity: 0.9; }\n"
       << ".install-btn:disabled { opacity: 0.5; }\n"
       << ".log-view { background-color: " << c.bg_secondary << "; color: " << c.text_primary << "; font-family: monospace; font-size: 12px; }\n"
       << ".search-entry { background-color: " << c.bg_card << "; color: " << c.text_primary << "; border: 1px solid " << c.border << "; border-radius: 8px; padding: 6px 12px; }\n"
       << ".app-title { color: " << c.accent << "; font-size: 22px; font-weight: bold; }\n"
       << ".badge-installed { background-color: " << c.success << "; color: #fff; border-radius: 4px; padding: 2px 6px; font-size: 10px; font-weight: bold; }\n"
       << "checkbutton check { background-color: " << c.bg_card << "; border: 1px solid " << c.border << "; border-radius: 4px; }\n"
       << "checkbutton:checked check { background-color: " << c.accent << "; border-color: " << c.accent << "; }\n"
       << ".section-label { color: " << c.text_secondary << "; font-size: 11px; font-weight: bold; letter-spacing: 1px; padding: 8px 16px 4px; }\n"
       << ".panel-label { color: " << c.text_primary << "; font-size: 13px; font-weight: bold; }\n";
    return ss.str();
}
