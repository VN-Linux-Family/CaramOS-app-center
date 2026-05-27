#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// App Explorer — UI Widgets (GTK 3)
// ─────────────────────────────────────────────────────────────────────────────

#include "package_manager.hpp"
#include "theme_config.hpp"
#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace AppExplorer {

// ─────────────────────────────────────────────────────────────────────────────
// ConsoleView — Read-only console hiển thị output cài đặt
// ─────────────────────────────────────────────────────────────────────────────

class ConsoleView {
public:
    ConsoleView();
    ~ConsoleView();

    GtkWidget* widget();   // GtkBox container

    // Thêm dòng log
    void appendLine(const LogLine& line);
    void appendRaw(LogLevel level, const std::string& text);

    // Xóa toàn bộ
    void clear();

    // Cuộn xuống cuối
    void scrollToEnd();

    // Collapse/expand
    void setVisible(bool v);
    bool isVisible() const;

    // Set tiêu đề header
    void setTitle(const std::string& title);

private:
    GtkWidget* box_;
    GtkWidget* header_;
    GtkWidget* label_title_;
    GtkWidget* btn_toggle_;
    GtkWidget* btn_clear_;
    GtkWidget* scroll_;
    GtkWidget* text_view_;
    GtkTextBuffer* buffer_;

    // Text tags cho màu sắc từng loại log
    GtkTextTag* tag_cmd_;
    GtkTextTag* tag_stdout_;
    GtkTextTag* tag_stderr_;
    GtkTextTag* tag_error_;
    GtkTextTag* tag_success_;
    GtkTextTag* tag_info_;

    bool expanded_ = true;

    void setupTags();
    void applyTheme(const ThemeConfig& cfg);

    static void onToggleClicked(GtkButton*, gpointer data);
    static void onClearClicked(GtkButton*, gpointer data);
};

// ─────────────────────────────────────────────────────────────────────────────
// AppCard — Card hiển thị một ứng dụng (grid view)
// ─────────────────────────────────────────────────────────────────────────────

class AppCard {
public:
    explicit AppCard(const AppPackage& pkg);
    ~AppCard();

    GtkWidget* widget();

    void updateStatus(InstallStatus status);
    void setSelected(bool selected);

    const std::string& appId() const { return pkg_.id; }

    // Callback khi click card
    void onClick(std::function<void(const std::string& id)> cb) {
        on_click_ = cb;
    }

private:
    const AppPackage& pkg_;
    GtkWidget* box_;
    GtkWidget* icon_;
    GtkWidget* label_name_;
    GtkWidget* label_desc_;
    GtkWidget* badge_cat_;
    GtkWidget* overlay_installed_;
    GtkWidget* event_box_;

    bool selected_ = false;
    InstallStatus status_ = InstallStatus::UNKNOWN;

    std::function<void(const std::string&)> on_click_;

    void loadIcon();
    void applyTheme(const ThemeConfig& cfg);

    static gboolean onButtonPress(GtkWidget*, GdkEventButton*, gpointer data);
    static gboolean onEnterNotify(GtkWidget*, GdkEventCrossing*, gpointer data);
    static gboolean onLeaveNotify(GtkWidget*, GdkEventCrossing*, gpointer data);
};

// ─────────────────────────────────────────────────────────────────────────────
// AppListRow — Row hiển thị một ứng dụng (list view)
// ─────────────────────────────────────────────────────────────────────────────

class AppListRow {
public:
    explicit AppListRow(const AppPackage& pkg);
    ~AppListRow();

    GtkWidget* widget();
    void updateStatus(InstallStatus status);
    void setSelected(bool selected);
    const std::string& appId() const { return pkg_.id; }

    void onClick(std::function<void(const std::string&)> cb) {
        on_click_ = cb;
    }

private:
    const AppPackage& pkg_;
    GtkWidget* row_box_;
    GtkWidget* icon_;
    GtkWidget* label_name_;
    GtkWidget* label_desc_;
    GtkWidget* label_version_;
    GtkWidget* badge_status_;
    GtkWidget* event_box_;

    bool selected_ = false;

    std::function<void(const std::string&)> on_click_;

    void loadIcon();
    static gboolean onButtonPress(GtkWidget*, GdkEventButton*, gpointer data);
};

// ─────────────────────────────────────────────────────────────────────────────
// DetailPanel — Panel chi tiết bên phải khi chọn ứng dụng
// ─────────────────────────────────────────────────────────────────────────────

class DetailPanel {
public:
    DetailPanel();
    ~DetailPanel();

    GtkWidget* widget();

    // Hiển thị thông tin app
    void showApp(const AppPackage& pkg, InstallStatus status);

    // Cập nhật trạng thái nút
    void updateStatus(InstallStatus status);

    // Clear (không có app nào được chọn)
    void clear();

    // Callbacks
    void onInstall(std::function<void(const std::string&)> cb) {
        on_install_ = cb;
    }
    void onUninstall(std::function<void(const std::string&)> cb) {
        on_uninstall_ = cb;
    }

private:
    GtkWidget* root_box_;
    GtkWidget* scroll_;
    GtkWidget* content_box_;

    // Header
    GtkWidget* icon_widget_;
    GtkWidget* label_name_;
    GtkWidget* label_version_;
    GtkWidget* label_author_;
    GtkWidget* badge_cat_;
    GtkWidget* badge_license_;

    // Description
    GtkWidget* label_desc_title_;
    GtkWidget* label_desc_;
    GtkWidget* label_longdesc_;

    // Usage
    GtkWidget* label_usage_title_;
    GtkWidget* label_usage_;

    // Info table
    GtkWidget* grid_info_;

    // Screenshots
    GtkWidget* screenshot_box_;

    // Tags
    GtkWidget* tags_flow_;

    // Actions
    GtkWidget* action_box_;
    GtkWidget* btn_install_;
    GtkWidget* btn_uninstall_;
    GtkWidget* btn_homepage_;
    GtkWidget* spinner_;
    GtkWidget* label_status_;

    const AppPackage* current_pkg_ = nullptr;
    InstallStatus     current_status_ = InstallStatus::UNKNOWN;

    std::function<void(const std::string&)> on_install_;
    std::function<void(const std::string&)> on_uninstall_;

    void buildLayout();
    void loadIcon(const AppPackage& pkg);
    void populateInfoGrid(const AppPackage& pkg);
    void populateTags(const AppPackage& pkg);
    void loadScreenshots(const AppPackage& pkg);
    void refreshActionButtons();

    static void onInstallClicked(GtkButton*, gpointer data);
    static void onUninstallClicked(GtkButton*, gpointer data);
    static void onHomepageClicked(GtkButton*, gpointer data);
};

// ─────────────────────────────────────────────────────────────────────────────
// NavSidebar — Thanh navigation bên trái
// ─────────────────────────────────────────────────────────────────────────────

struct NavItem {
    std::string     id;
    std::string     label;
    std::string     icon_name;   // GTK icon name
    AppCategory     category;    // Chỉ dùng nếu là category item
    bool            is_separator = false;
};

class NavSidebar {
public:
    NavSidebar();
    ~NavSidebar();

    GtkWidget* widget();

    // Set danh sách items
    void setItems(const std::vector<NavItem>& items);

    // Set item active
    void setActive(const std::string& id);

    // Callback khi click item
    void onSelect(std::function<void(const NavItem&)> cb) {
        on_select_ = cb;
    }

    // Cập nhật badge count (vd: số app installed)
    void setBadgeCount(const std::string& item_id, int count);

private:
    GtkWidget* box_;
    GtkWidget* logo_box_;
    GtkWidget* logo_label_;
    GtkWidget* items_box_;

    struct NavButton {
        NavItem       item;
        GtkWidget*    btn;
        GtkWidget*    label;
        GtkWidget*    icon;
        GtkWidget*    badge;
        bool          active = false;
    };

    std::vector<NavButton> nav_buttons_;
    std::string active_id_;

    std::function<void(const NavItem&)> on_select_;

    GtkWidget* makeNavButton(const NavItem& item);
    void       applyActiveStyle(NavButton& nb, bool active);

    static void onNavBtnClicked(GtkButton*, gpointer data);
};

// ─────────────────────────────────────────────────────────────────────────────
// Toolbar — Thanh công cụ trên cùng của content area
// ─────────────────────────────────────────────────────────────────────────────

class Toolbar {
public:
    Toolbar();
    ~Toolbar();

    GtkWidget* widget();

    // Set tiêu đề category/section
    void setTitle(const std::string& title);

    // Lấy nội dung search
    std::string searchText() const;

    // Callback search changed
    void onSearch(std::function<void(const std::string&)> cb) {
        on_search_ = cb;
    }

    // Callback toggle view (grid/list)
    void onViewToggle(std::function<void(bool is_grid)> cb) {
        on_view_toggle_ = cb;
    }

    // Cập nhật trạng thái view button
    void setViewMode(bool is_grid);

    // Callback refresh
    void onRefresh(std::function<void()> cb) {
        on_refresh_ = cb;
    }

    // Callback theme picker
    void onThemePickerOpen(std::function<void()> cb) {
        on_theme_picker_ = cb;
    }

private:
    GtkWidget* box_;
    GtkWidget* label_title_;
    GtkWidget* search_entry_;
    GtkWidget* btn_grid_view_;
    GtkWidget* btn_list_view_;
    GtkWidget* btn_refresh_;
    GtkWidget* btn_theme_;

    std::function<void(const std::string&)> on_search_;
    std::function<void(bool)>               on_view_toggle_;
    std::function<void()>                   on_refresh_;
    std::function<void()>                   on_theme_picker_;

    static void onSearchChanged(GtkSearchEntry*, gpointer data);
    static void onGridViewClicked(GtkButton*, gpointer data);
    static void onListViewClicked(GtkButton*, gpointer data);
    static void onRefreshClicked(GtkButton*, gpointer data);
    static void onThemeClicked(GtkButton*, gpointer data);
};

// ─────────────────────────────────────────────────────────────────────────────
// StatusBar — Thanh trạng thái dưới cùng
// ─────────────────────────────────────────────────────────────────────────────

class StatusBar {
public:
    StatusBar();
    ~StatusBar();

    GtkWidget* widget();

    void setMessage(const std::string& msg);
    void setProgress(double fraction);   // 0.0 – 1.0, < 0 = hide
    void showSpinner(bool show);

    // Hiển thị tổng số app / installed
    void setAppCount(int total, int installed);

private:
    GtkWidget* box_;
    GtkWidget* label_msg_;
    GtkWidget* progress_;
    GtkWidget* spinner_;
    GtkWidget* label_count_;
};

// ─────────────────────────────────────────────────────────────────────────────
// ThemePickerDialog — Dialog chọn / load theme
// ─────────────────────────────────────────────────────────────────────────────

class ThemePickerDialog {
public:
    explicit ThemePickerDialog(GtkWindow* parent);
    ~ThemePickerDialog();

    // Hiện dialog, trả về path theme được chọn (hoặc "" nếu cancel)
    std::string run();

private:
    GtkWidget* dialog_;
    GtkWidget* list_box_;
    GtkWidget* btn_browse_;
    GtkWidget* preview_label_;

    std::string themes_dir_;
    std::string selected_path_;

    void populateList();
    void loadPreview(const std::string& path);

    static void onRowActivated(GtkListBox*, GtkListBoxRow*, gpointer data);
    static void onBrowseClicked(GtkButton*, gpointer data);
};

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow — Cửa sổ chính
// ─────────────────────────────────────────────────────────────────────────────

class MainWindow {
public:
    explicit MainWindow(GtkApplication* app);
    ~MainWindow();

    GtkWidget* window() { return window_; }

    void show();
    void setRepository(PackageRepository* repo);

private:
    GtkWidget*          window_;
    GtkApplication*     app_;
    PackageRepository*  repo_ = nullptr;

    // Layout containers
    GtkWidget*          main_hbox_;     // Nav | content_vbox
    GtkWidget*          content_vbox_;  // Toolbar / paned / statusbar
    GtkWidget*          center_paned_;  // app list | detail panel
    GtkWidget*          left_vbox_;     // toolbar + app area + console

    // Widgets
    std::unique_ptr<NavSidebar>  nav_;
    std::unique_ptr<Toolbar>     toolbar_;
    std::unique_ptr<StatusBar>   statusbar_;
    std::unique_ptr<DetailPanel> detail_panel_;
    std::unique_ptr<ConsoleView> console_;

    // App area (grid / list stacked)
    GtkWidget*          stack_views_;
    GtkWidget*          scroll_grid_;
    GtkWidget*          scroll_list_;
    GtkWidget*          grid_flow_;     // GtkFlowBox
    GtkWidget*          list_box_;      // GtkListBox

    // State
    std::string         current_section_ = "all";
    std::string         current_search_;
    bool                is_grid_view_    = true;
    std::string         selected_app_id_;

    // Tracking widgets per app id
    std::unordered_map<std::string, std::unique_ptr<AppCard>>    grid_cards_;
    std::unordered_map<std::string, std::unique_ptr<AppListRow>> list_rows_;

    // ── Setup ────────────────────────────────────────────────────
    void buildLayout();
    void buildNav();
    void connectSignals();

    // ── Population ───────────────────────────────────────────────
    void populateApps(const std::string& section, const std::string& search = "");
    void populateGridView(const std::vector<const AppPackage*>& pkgs);
    void populateListView(const std::vector<const AppPackage*>& pkgs);
    void clearViews();

    // ── Selection ────────────────────────────────────────────────
    void selectApp(const std::string& id);
    void deselectAll();

    // ── Operations ───────────────────────────────────────────────
    void beginInstall(const std::string& id);
    void beginUninstall(const std::string& id);
    void onOperationLog(const LogLine& line);
    void onOperationFinished(const std::string& id, OperationType type,
                             bool success, int exit_code);

    // ── Theme ────────────────────────────────────────────────────
    void openThemePicker();
    void applyTheme(const ThemeConfig& cfg);

    // ── Helpers ──────────────────────────────────────────────────
    std::vector<const AppPackage*> filterPackages(const std::string& section,
                                                  const std::string& search);
    std::string categoryLabel(AppCategory cat) const;
    std::string statusLabel(InstallStatus s) const;

    static void onDeleteEvent(GtkWidget*, GdkEvent*, gpointer data);
};

} // namespace AppExplorer
