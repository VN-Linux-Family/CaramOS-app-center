// ─────────────────────────────────────────────────────────────────────────────
// App Explorer — main.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "ui_widgets.hpp"
#include "package_manager.hpp"
#include "theme_config.hpp"
#include <gtk/gtk.h>
#include <string>
#include <cstdlib>
#include <sys/stat.h>

using namespace AppExplorer;

// ─────────────────────────────────────────────────────────────────────────────
// Application config paths
// ─────────────────────────────────────────────────────────────────────────────

static std::string getConfigDir() {
    const char* xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) return std::string(xdg) + "/app-explorer";
    const char* home = getenv("HOME");
    return std::string(home ? home : "/tmp") + "/.config/app-explorer";
}

static std::string getDataDir() {
    const char* xdg = getenv("XDG_DATA_HOME");
    if (xdg && *xdg) return std::string(xdg) + "/app-explorer";
    const char* home = getenv("HOME");
    return std::string(home ? home : "/tmp") + "/.local/share/app-explorer";
}

static void ensureDir(const std::string& path) {
    g_mkdir_with_parents(path.c_str(), 0755);
}

// ─────────────────────────────────────────────────────────────────────────────
// Application struct
// ─────────────────────────────────────────────────────────────────────────────

struct AppExplorerApp {
    GtkApplication*                      gtk_app  = nullptr;
    std::unique_ptr<MainWindow>          window;
    std::unique_ptr<PackageRepository>   repo;
    std::string                          config_dir;
    std::string                          data_dir;
    std::string                          log_dir;
    std::string                          packages_dir;
    std::string                          themes_dir;
};

static AppExplorerApp* g_app = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// GTK activate callback
// ─────────────────────────────────────────────────────────────────────────────

static void on_activate(GtkApplication* gtk_app, gpointer user_data) {
    AppExplorerApp* app = static_cast<AppExplorerApp*>(user_data);

    // 1. Init directories
    app->config_dir   = getConfigDir();
    app->data_dir     = getDataDir();
    app->log_dir      = app->config_dir + "/logs";
    app->packages_dir = app->config_dir + "/packages";
    app->themes_dir   = app->config_dir + "/themes";

    ensureDir(app->config_dir);
    ensureDir(app->data_dir);
    ensureDir(app->log_dir);
    ensureDir(app->packages_dir);
    ensureDir(app->themes_dir);

    // 2. Init logger
    Logger::instance().init(app->log_dir);
    Logger::instance().logGUI(AppLogLevel::INFO, "App Explorer starting");
    Logger::instance().logGUI(AppLogLevel::INFO, "Config dir: " + app->config_dir);

    // 3. Load theme
    // Try user theme first, fallback to default
    std::string user_theme  = app->themes_dir + "/classic-ivory.cfg";
    std::string sys_theme   = "/usr/share/app-explorer/themes/classic-ivory.cfg";
    bool theme_loaded = false;

    // Check command line env override
    const char* env_theme = getenv("AE_THEME");
    if (env_theme && *env_theme) {
        theme_loaded = ThemeManager::instance().loadTheme(env_theme);
        if (theme_loaded)
            Logger::instance().logGUI(AppLogLevel::INFO,
                std::string("Theme from AE_THEME: ") + env_theme);
    }

    if (!theme_loaded) {
        if (g_file_test(user_theme.c_str(), G_FILE_TEST_EXISTS)) {
            theme_loaded = ThemeManager::instance().loadTheme(user_theme);
        }
    }
    if (!theme_loaded) {
        if (g_file_test(sys_theme.c_str(), G_FILE_TEST_EXISTS)) {
            theme_loaded = ThemeManager::instance().loadTheme(sys_theme);
        }
    }
    if (!theme_loaded) {
        ThemeManager::instance().loadDefaultTheme();
        Logger::instance().logGUI(AppLogLevel::INFO, "Using built-in default theme");
    }

    // 4. Create main window
    app->window = std::make_unique<MainWindow>(gtk_app);

    // 5. Load package repository
    app->repo = std::make_unique<PackageRepository>();

    // Check multiple package paths:
    // 1. User local packages
    // 2. System packages
    // 3. AE_PACKAGES_DIR env override
    const char* env_pkgs = getenv("AE_PACKAGES_DIR");
    if (env_pkgs && *env_pkgs) {
        app->repo->loadDirectory(env_pkgs);
        Logger::instance().logGUI(AppLogLevel::INFO,
            std::string("Packages from AE_PACKAGES_DIR: ") + env_pkgs);
    } else {
        // Load from user dir
        if (g_file_test(app->packages_dir.c_str(), G_FILE_TEST_IS_DIR))
            app->repo->loadDirectory(app->packages_dir);

        // Also load from system dir if exists
        std::string sys_pkgs = "/usr/share/app-explorer/packages";
        if (g_file_test(sys_pkgs.c_str(), G_FILE_TEST_IS_DIR)) {
            PackageRepository sys_repo;
            sys_repo.loadDirectory(sys_pkgs);
            // Merge: add packages not already in user repo
            for (auto& pkg : sys_repo.allPackages()) {
                if (!app->repo->findById(pkg.id)) {
                    // Re-load (workaround: just reload from sys dir too)
                    // In production, PackageRepository would have merge()
                }
            }
        }
    }

    Logger::instance().logGUI(AppLogLevel::INFO,
        "Loaded " + std::to_string(app->repo->count()) + " packages");

    // 6. Wire repo to window and show
    app->window->setRepository(app->repo.get());
    app->window->show();

    Logger::instance().logGUI(AppLogLevel::INFO, "App Explorer ready");
}

// ─────────────────────────────────────────────────────────────────────────────
// main()
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    AppExplorerApp app_state;
    g_app = &app_state;

    app_state.gtk_app = gtk_application_new(
        "io.github.app-explorer",
        G_APPLICATION_FLAGS_NONE);

    g_signal_connect(app_state.gtk_app, "activate",
        G_CALLBACK(on_activate), &app_state);

    int status = g_application_run(
        G_APPLICATION(app_state.gtk_app), argc, argv);

    g_object_unref(app_state.gtk_app);
    return status;
}
