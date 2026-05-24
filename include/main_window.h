#pragma once
#include "types.h"
#include "theme_manager.h"
#include "app_loader.h"
#include "installer.h"
#include <gtk/gtk.h>
#include <vector>
#include <string>
#include <map>

class MainWindow {
public:
    MainWindow(GtkApplication* app);
    ~MainWindow();

    GtkWidget* widget() { return window_; }

    // Public state (needed by idle callbacks)
    GtkWidget*               window_{nullptr};
    GtkWidget*               app_grid_{nullptr};
    GtkWidget*               log_view_{nullptr};
    GtkTextBuffer*           log_buffer_{nullptr};
    GtkWidget*               progress_bar_{nullptr};
    GtkWidget*               install_btn_{nullptr};
    GtkWidget*               status_label_{nullptr};
    GtkWidget*               search_entry_{nullptr};

    ThemeManager             theme_mgr_;
    AppLoader                loader_;
    std::vector<AppInfo>     apps_;
    std::string              current_category_{"all"};
    std::string              current_search_;
    std::map<std::string, GtkWidget*>      card_progress_;
    std::map<std::string, GtkCheckButton*> card_checks_;
    Installer*               installer_{nullptr};

    void populate_app_grid(const std::string& cat = "all",
                           const std::string& search = "");

private:
    GtkApplication*          gtk_app_{nullptr};

    void build_ui();
    void build_header();
    void build_sidebar();
    void build_app_grid();
    void build_log_panel();
    void build_theme_dialog();
    GtkWidget* create_app_card(const AppInfo& app);

    static void on_install_clicked(GtkButton*, gpointer);
    static void on_select_all(GtkButton*, gpointer);
    static void on_deselect_all(GtkButton*, gpointer);
    static void on_search_changed(GtkSearchEntry*, gpointer);
    static void on_theme_light(GtkButton*, gpointer);
    static void on_theme_dark(GtkButton*, gpointer);
    static void on_theme_custom(GtkButton*, gpointer);
    static void on_card_toggled(GtkCheckButton*, gpointer);
};
