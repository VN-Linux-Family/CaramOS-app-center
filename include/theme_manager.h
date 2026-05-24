#pragma once
#include "types.h"
#include <gtk/gtk.h>
#include <string>

class ThemeManager {
public:
    ThemeManager();
    ~ThemeManager();

    void apply(GtkWidget* window, Theme theme, const CustomThemeColors* custom = nullptr);
    void set_custom_colors(const CustomThemeColors& colors);
    const CustomThemeColors& get_custom_colors() const { return custom_colors_; }

    Theme current_theme() const { return current_; }

    // Trả về CSS string theo theme
    std::string build_css(Theme theme, const CustomThemeColors* custom = nullptr) const;

private:
    Theme             current_{Theme::DARK};
    CustomThemeColors custom_colors_;
    GtkCssProvider*   provider_{nullptr};

    std::string light_css() const;
    std::string dark_css() const;
    std::string custom_css(const CustomThemeColors& c) const;
};
