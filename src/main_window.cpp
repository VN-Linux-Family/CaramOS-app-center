// ─────────────────────────────────────────────────────────────────────────────
// App Explorer — main_window.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "ui_widgets.hpp"
#include "package_manager.hpp"
#include "theme_config.hpp"
#include <gtk/gtk.h>
#include <algorithm>
#include <sstream>

namespace AppExplorer {

// ─────────────────────────────────────────────────────────────────────────────
// AppCard
// ─────────────────────────────────────────────────────────────────────────────

AppCard::AppCard(const AppPackage& pkg) : pkg_(pkg) {
    const auto& cfg = ThemeManager::instance().currentTheme();

    event_box_ = gtk_event_box_new();
    box_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(box_), "app-card");
    gtk_widget_set_size_request(box_, cfg.layout.card_width, cfg.layout.card_height);

    // Icon
    icon_ = gtk_image_new_from_icon_name("application-x-executable", GTK_ICON_SIZE_DIALOG);
    gtk_widget_set_size_request(icon_, cfg.layout.card_icon_size, cfg.layout.card_icon_size);
    gtk_widget_set_halign(icon_, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(icon_, 12);

    // Name
    label_name_ = gtk_label_new(pkg.name.c_str());
    gtk_style_context_add_class(gtk_widget_get_style_context(label_name_), "app-card-name");
    gtk_label_set_xalign(GTK_LABEL(label_name_), 0.5);
    gtk_label_set_justify(GTK_LABEL(label_name_), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(label_name_), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(label_name_), 20);
    gtk_widget_set_margin_top(label_name_, 8);
    gtk_widget_set_margin_start(label_name_, 8);
    gtk_widget_set_margin_end(label_name_, 8);

    // Description (truncated)
    std::string desc = pkg.description;
    if (desc.size() > 72) desc = desc.substr(0, 69) + "…";
    label_desc_ = gtk_label_new(desc.c_str());
    gtk_style_context_add_class(gtk_widget_get_style_context(label_desc_), "app-card-desc");
    gtk_label_set_xalign(GTK_LABEL(label_desc_), 0.5);
    gtk_label_set_justify(GTK_LABEL(label_desc_), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(label_desc_), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(label_desc_), 24);
    gtk_widget_set_margin_start(label_desc_, 8);
    gtk_widget_set_margin_end(label_desc_, 8);
    gtk_widget_set_margin_top(label_desc_, 4);

    // Spacer
    GtkWidget* spacer = gtk_label_new("");
    gtk_widget_set_vexpand(spacer, TRUE);

    // Category badge
    badge_cat_ = gtk_label_new(pkg.category_str.c_str());
    gtk_style_context_add_class(gtk_widget_get_style_context(badge_cat_), "badge");
    gtk_widget_set_halign(badge_cat_, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_bottom(badge_cat_, 10);
    gtk_widget_set_margin_top(badge_cat_, 4);
    gtk_widget_set_visible(badge_cat_, cfg.misc.show_category_badge);

    // Installed overlay
    overlay_installed_ = gtk_label_new("✓ Installed");
    gtk_style_context_add_class(gtk_widget_get_style_context(overlay_installed_), "installed-overlay");
    gtk_widget_set_halign(overlay_installed_, GTK_ALIGN_FILL);
    gtk_widget_hide(overlay_installed_);

    gtk_box_pack_start(GTK_BOX(box_), icon_,               FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box_), label_name_,         FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box_), label_desc_,         FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box_), spacer,              TRUE,  TRUE,  0);
    gtk_box_pack_start(GTK_BOX(box_), badge_cat_,          FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box_), overlay_installed_,  FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(event_box_), box_);

    g_signal_connect(event_box_, "button-press-event",  G_CALLBACK(onButtonPress),  this);
    g_signal_connect(event_box_, "enter-notify-event",  G_CALLBACK(onEnterNotify),  this);
    g_signal_connect(event_box_, "leave-notify-event",  G_CALLBACK(onLeaveNotify),  this);

    loadIcon();
    gtk_widget_show_all(event_box_);
}

AppCard::~AppCard() {}

GtkWidget* AppCard::widget() { return event_box_; }

void AppCard::loadIcon() {
    const auto& cfg = ThemeManager::instance().currentTheme();
    std::string path = pkg_.resolvedIconPath();
    if (!path.empty() && !pkg_.iconIsOnline()) {
        GdkPixbuf* pb = gdk_pixbuf_new_from_file_at_scale(
            path.c_str(), cfg.layout.card_icon_size, cfg.layout.card_icon_size, TRUE, nullptr);
        if (pb) {
            gtk_image_set_from_pixbuf(GTK_IMAGE(icon_), pb);
            g_object_unref(pb);
            return;
        }
    }
    gtk_image_set_from_icon_name(GTK_IMAGE(icon_),
        "application-x-executable", GTK_ICON_SIZE_DIALOG);
}

void AppCard::updateStatus(InstallStatus s) {
    status_ = s;
    const auto& cfg = ThemeManager::instance().currentTheme();
    if (s == InstallStatus::INSTALLED && cfg.misc.show_installed_badge) {
        gtk_widget_show(overlay_installed_);
    } else {
        gtk_widget_hide(overlay_installed_);
    }
}

void AppCard::setSelected(bool sel) {
    selected_ = sel;
    auto ctx = gtk_widget_get_style_context(box_);
    if (sel) gtk_style_context_add_class(ctx, "selected");
    else     gtk_style_context_remove_class(ctx, "selected");
}

gboolean AppCard::onButtonPress(GtkWidget*, GdkEventButton* ev, gpointer data) {
    if (ev->button == 1) {
        auto* self = static_cast<AppCard*>(data);
        if (self->on_click_) self->on_click_(self->pkg_.id);
    }
    return FALSE;
}

gboolean AppCard::onEnterNotify(GtkWidget* w, GdkEventCrossing*, gpointer) {
    GdkWindow* win = gtk_widget_get_window(w);
    if (win) {
        GdkCursor* cursor = gdk_cursor_new_from_name(
            gdk_display_get_default(), "pointer");
        gdk_window_set_cursor(win, cursor);
        g_object_unref(cursor);
    }
    return FALSE;
}

gboolean AppCard::onLeaveNotify(GtkWidget* w, GdkEventCrossing*, gpointer) {
    GdkWindow* win = gtk_widget_get_window(w);
    if (win) gdk_window_set_cursor(win, nullptr);
    return FALSE;
}

// ─────────────────────────────────────────────────────────────────────────────
// AppListRow
// ─────────────────────────────────────────────────────────────────────────────

AppListRow::AppListRow(const AppPackage& pkg) : pkg_(pkg) {
    const auto& cfg = ThemeManager::instance().currentTheme();

    event_box_ = gtk_event_box_new();
    row_box_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_style_context_add_class(gtk_widget_get_style_context(row_box_), "app-list-row");
    gtk_widget_set_size_request(row_box_, -1, cfg.layout.list_row_height);

    icon_ = gtk_image_new_from_icon_name("application-x-executable", GTK_ICON_SIZE_DND);
    gtk_widget_set_size_request(icon_, cfg.layout.list_icon_size, cfg.layout.list_icon_size);
    gtk_widget_set_margin_start(icon_, cfg.layout.list_padding_h);

    GtkWidget* info_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_valign(info_col, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(info_col, TRUE);

    label_name_    = gtk_label_new(pkg.name.c_str());
    label_version_ = gtk_label_new(("v" + pkg.version).c_str());
    label_desc_    = gtk_label_new(pkg.description.c_str());

    gtk_label_set_xalign(GTK_LABEL(label_name_),    0);
    gtk_label_set_xalign(GTK_LABEL(label_version_), 0);
    gtk_label_set_xalign(GTK_LABEL(label_desc_),    0);

    gtk_label_set_ellipsize(GTK_LABEL(label_desc_), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(label_desc_), 60);

    gtk_style_context_add_class(gtk_widget_get_style_context(label_name_),    "app-card-name");
    gtk_style_context_add_class(gtk_widget_get_style_context(label_version_), "text-muted");
    gtk_style_context_add_class(gtk_widget_get_style_context(label_desc_),    "app-card-desc");

    GtkWidget* name_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(name_row), label_name_,    FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(name_row), label_version_, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(info_col), name_row,    FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(info_col), label_desc_, FALSE, FALSE, 0);

    badge_status_ = gtk_label_new("");
    gtk_style_context_add_class(gtk_widget_get_style_context(badge_status_), "badge");
    gtk_widget_set_valign(badge_status_, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_end(badge_status_, cfg.layout.list_padding_h);
    gtk_widget_hide(badge_status_);

    gtk_box_pack_start(GTK_BOX(row_box_), icon_,        FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row_box_), info_col,     TRUE,  TRUE,  0);
    gtk_box_pack_end  (GTK_BOX(row_box_), badge_status_,FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(event_box_), row_box_);
    g_signal_connect(event_box_, "button-press-event", G_CALLBACK(onButtonPress), this);

    loadIcon();
    gtk_widget_show_all(event_box_);
    gtk_widget_hide(badge_status_);
}

AppListRow::~AppListRow() {}
GtkWidget* AppListRow::widget() { return event_box_; }

void AppListRow::loadIcon() {
    std::string path = pkg_.resolvedIconPath();
    if (!path.empty() && !pkg_.iconIsOnline()) {
        GdkPixbuf* pb = gdk_pixbuf_new_from_file_at_scale(path.c_str(), 48, 48, TRUE, nullptr);
        if (pb) { gtk_image_set_from_pixbuf(GTK_IMAGE(icon_), pb); g_object_unref(pb); return; }
    }
    gtk_image_set_from_icon_name(GTK_IMAGE(icon_), "application-x-executable", GTK_ICON_SIZE_DND);
}

void AppListRow::updateStatus(InstallStatus s) {
    if (s == InstallStatus::INSTALLED) {
        gtk_label_set_text(GTK_LABEL(badge_status_), "Installed");
        gtk_style_context_add_class(gtk_widget_get_style_context(badge_status_), "badge-installed");
        gtk_widget_show(badge_status_);
    } else if (s == InstallStatus::INSTALLING || s == InstallStatus::UNINSTALLING) {
        gtk_label_set_text(GTK_LABEL(badge_status_),
            s == InstallStatus::INSTALLING ? "Installing…" : "Removing…");
        gtk_widget_show(badge_status_);
    } else {
        gtk_widget_hide(badge_status_);
    }
}

void AppListRow::setSelected(bool sel) {
    selected_ = sel;
    auto ctx = gtk_widget_get_style_context(row_box_);
    if (sel) gtk_style_context_add_class(ctx, "selected");
    else     gtk_style_context_remove_class(ctx, "selected");
}

gboolean AppListRow::onButtonPress(GtkWidget*, GdkEventButton* ev, gpointer data) {
    if (ev->button == 1) {
        auto* self = static_cast<AppListRow*>(data);
        if (self->on_click_) self->on_click_(self->pkg_.id);
    }
    return FALSE;
}

// ─────────────────────────────────────────────────────────────────────────────
// ThemePickerDialog
// ─────────────────────────────────────────────────────────────────────────────

ThemePickerDialog::ThemePickerDialog(GtkWindow* parent) {
    dialog_ = gtk_dialog_new_with_buttons("Choose Theme",
        parent,
        (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Apply",  GTK_RESPONSE_OK,
        nullptr);
    gtk_window_set_default_size(GTK_WINDOW(dialog_), 420, 360);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog_));
    gtk_style_context_add_class(gtk_widget_get_style_context(content), "dialog-content");
    gtk_container_set_border_width(GTK_CONTAINER(content), 16);
    gtk_box_set_spacing(GTK_BOX(content), 12);

    GtkWidget* lbl = gtk_label_new("Select a theme file (.cfg):");
    gtk_label_set_xalign(GTK_LABEL(lbl), 0);
    gtk_box_pack_start(GTK_BOX(content), lbl, FALSE, FALSE, 0);

    // List box in scrolled window
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_IN);
    gtk_widget_set_size_request(scroll, -1, 200);

    list_box_ = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list_box_), GTK_SELECTION_SINGLE);
    gtk_container_add(GTK_CONTAINER(scroll), list_box_);
    gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);

    // Browse button
    btn_browse_ = gtk_button_new_with_label("Browse for .cfg file…");
    gtk_box_pack_start(GTK_BOX(content), btn_browse_, FALSE, FALSE, 0);

    // Preview label
    preview_label_ = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(preview_label_), 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(preview_label_), "text-muted");
    gtk_label_set_line_wrap(GTK_LABEL(preview_label_), TRUE);
    gtk_box_pack_start(GTK_BOX(content), preview_label_, FALSE, FALSE, 0);

    g_signal_connect(list_box_,  "row-activated",  G_CALLBACK(onRowActivated),  this);
    g_signal_connect(btn_browse_,"clicked",         G_CALLBACK(onBrowseClicked), this);

    // Load themes from default dir
    const char* home = getenv("HOME");
    if (home) {
        themes_dir_ = std::string(home) + "/.config/app-explorer/themes";
    }
    populateList();

    gtk_widget_show_all(dialog_);
}

ThemePickerDialog::~ThemePickerDialog() {}

void ThemePickerDialog::populateList() {
    // Clear
    GList* children = gtk_container_get_children(GTK_CONTAINER(list_box_));
    for (GList* c = children; c; c=c->next) gtk_widget_destroy(GTK_WIDGET(c->data));
    g_list_free(children);

    // Add bundled themes first
    auto& tm = ThemeManager::instance();
    std::vector<std::string> paths = tm.listThemeFiles(themes_dir_);

    // Also look in /usr/share/app-explorer/themes
    auto sys_paths = tm.listThemeFiles("/usr/share/app-explorer/themes");
    paths.insert(paths.end(), sys_paths.begin(), sys_paths.end());

    for (const auto& path : paths) {
        GtkWidget* row = gtk_list_box_row_new();
        GtkWidget* lbl = gtk_label_new(path.c_str());
        gtk_label_set_xalign(GTK_LABEL(lbl), 0);
        gtk_widget_set_margin_start(lbl, 8);
        gtk_widget_set_margin_end(lbl, 8);
        gtk_widget_set_margin_top(lbl, 6);
        gtk_widget_set_margin_bottom(lbl, 6);
        gtk_container_add(GTK_CONTAINER(row), lbl);
        g_object_set_data_full(G_OBJECT(row), "theme-path",
            g_strdup(path.c_str()), g_free);
        gtk_container_add(GTK_CONTAINER(list_box_), row);
    }

    if (paths.empty()) {
        GtkWidget* row = gtk_list_box_row_new();
        GtkWidget* lbl = gtk_label_new("No themes found. Use Browse to load a .cfg file.");
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "text-muted");
        gtk_widget_set_margin_start(lbl, 8);
        gtk_widget_set_margin_top(lbl, 6);
        gtk_widget_set_margin_bottom(lbl, 6);
        gtk_container_add(GTK_CONTAINER(row), lbl);
        gtk_container_add(GTK_CONTAINER(list_box_), row);
    }
    gtk_widget_show_all(list_box_);
}

std::string ThemePickerDialog::run() {
    gint response = gtk_dialog_run(GTK_DIALOG(dialog_));
    std::string result;
    if (response == GTK_RESPONSE_OK && !selected_path_.empty()) {
        result = selected_path_;
    }
    gtk_widget_destroy(dialog_);
    return result;
}

void ThemePickerDialog::onRowActivated(GtkListBox*, GtkListBoxRow* row, gpointer data) {
    auto* self = static_cast<ThemePickerDialog*>(data);
    const char* path = (const char*)g_object_get_data(G_OBJECT(row), "theme-path");
    if (path) {
        self->selected_path_ = path;
        self->loadPreview(path);
    }
}

void ThemePickerDialog::loadPreview(const std::string& path) {
    ThemeConfigParser parser;
    ThemeConfig cfg;
    if (parser.loadFromFile(path, cfg)) {
        std::string info = cfg.name + " v" + cfg.version;
        if (!cfg.author.empty()) info += " by " + cfg.author;
        if (!cfg.description.empty()) info += "\n" + cfg.description;
        gtk_label_set_text(GTK_LABEL(preview_label_), info.c_str());
    }
}

void ThemePickerDialog::onBrowseClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<ThemePickerDialog*>(data);
    GtkWidget* chooser = gtk_file_chooser_dialog_new("Open Theme File",
        GTK_WINDOW(self->dialog_),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open",   GTK_RESPONSE_ACCEPT,
        nullptr);

    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "App Explorer Theme (*.cfg)");
    gtk_file_filter_add_pattern(filter, "*.cfg");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

    if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        char* fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
        self->selected_path_ = fname;
        self->loadPreview(fname);
        g_free(fname);
    }
    gtk_widget_destroy(chooser);
}

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow
// ─────────────────────────────────────────────────────────────────────────────

MainWindow::MainWindow(GtkApplication* app) : app_(app) {
    window_ = gtk_application_window_new(app);
    gtk_widget_set_name(window_, "app-explorer-window");
    gtk_style_context_add_class(gtk_widget_get_style_context(window_), "app-explorer-window");

    const auto& cfg = ThemeManager::instance().currentTheme();
    gtk_window_set_title(GTK_WINDOW(window_), "App Explorer");
    gtk_window_set_default_size(GTK_WINDOW(window_),
        cfg.layout.window_width, cfg.layout.window_height);
    gtk_window_set_position(GTK_WINDOW(window_), GTK_WIN_POS_CENTER);
    gtk_widget_set_size_request(window_,
        cfg.layout.window_min_width, cfg.layout.window_min_height);

    g_signal_connect(window_, "delete-event", G_CALLBACK(onDeleteEvent), this);

    buildLayout();
    connectSignals();

    // Listen for theme changes
    ThemeManager::instance().onThemeChanged([this](const ThemeConfig& c) {
        applyTheme(c);
    });
}

MainWindow::~MainWindow() {}

void MainWindow::show() {
    gtk_widget_show_all(window_);
}

void MainWindow::setRepository(PackageRepository* repo) {
    repo_ = repo;
    if (repo_) {
        populateApps("all");
        // Update status bar
        int total = (int)repo_->count();
        int installed = 0;
        for (auto& pkg : repo_->allPackages()) {
            if (InstallManager::instance().checkStatus(pkg) == InstallStatus::INSTALLED)
                installed++;
        }
        statusbar_->setAppCount(total, installed);
        statusbar_->setMessage("Loaded " + std::to_string(total) + " packages");
    }
}

void MainWindow::buildLayout() {
    // Root: horizontal box [ nav | content ]
    main_hbox_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    // ── Navigation sidebar ──────────────────────────────────────
    nav_ = std::make_unique<NavSidebar>();
    buildNav();
    gtk_box_pack_start(GTK_BOX(main_hbox_), nav_->widget(), FALSE, FALSE, 0);

    // ── Content area ────────────────────────────────────────────
    content_vbox_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(content_vbox_, TRUE);

    // Toolbar
    toolbar_ = std::make_unique<Toolbar>();
    gtk_box_pack_start(GTK_BOX(content_vbox_), toolbar_->widget(), FALSE, FALSE, 0);

    // Center paned: [ app list | detail panel ]
    center_paned_ = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(center_paned_, TRUE);

    // Left: toolbar + views + console (vertical box)
    left_vbox_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(left_vbox_, TRUE);

    // Stack: grid view / list view
    stack_views_ = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack_views_),
        GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(stack_views_), 100);
    gtk_widget_set_vexpand(stack_views_, TRUE);

    // Grid scroll
    scroll_grid_ = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_grid_),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    grid_flow_ = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(grid_flow_), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(grid_flow_), 1);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(grid_flow_),
        ThemeManager::instance().currentTheme().misc.grid_columns);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(grid_flow_),
        ThemeManager::instance().currentTheme().layout.card_gap);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(grid_flow_),
        ThemeManager::instance().currentTheme().layout.card_gap);
    gtk_widget_set_margin_start(grid_flow_,
        ThemeManager::instance().currentTheme().layout.content_padding);
    gtk_widget_set_margin_end(grid_flow_,
        ThemeManager::instance().currentTheme().layout.content_padding);
    gtk_widget_set_margin_top(grid_flow_,
        ThemeManager::instance().currentTheme().layout.content_padding);
    gtk_widget_set_margin_bottom(grid_flow_,
        ThemeManager::instance().currentTheme().layout.content_padding);
    gtk_container_add(GTK_CONTAINER(scroll_grid_), grid_flow_);

    // List scroll
    scroll_list_ = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_list_),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    list_box_ = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list_box_), GTK_SELECTION_NONE);
    gtk_container_add(GTK_CONTAINER(scroll_list_), list_box_);

    gtk_stack_add_named(GTK_STACK(stack_views_), scroll_grid_, "grid");
    gtk_stack_add_named(GTK_STACK(stack_views_), scroll_list_, "list");
    gtk_stack_set_visible_child_name(GTK_STACK(stack_views_), "grid");

    // Console
    console_ = std::make_unique<ConsoleView>();
    gtk_widget_set_size_request(console_->widget(), -1,
        ThemeManager::instance().currentTheme().layout.console_height);
    if (!ThemeManager::instance().currentTheme().misc.show_console_auto)
        console_->setVisible(false);

    gtk_box_pack_start(GTK_BOX(left_vbox_), stack_views_,       TRUE,  TRUE,  0);
    gtk_box_pack_start(GTK_BOX(left_vbox_), console_->widget(), FALSE, FALSE, 0);

    // Detail panel
    detail_panel_ = std::make_unique<DetailPanel>();

    gtk_paned_pack1(GTK_PANED(center_paned_), left_vbox_,             TRUE,  TRUE);
    gtk_paned_pack2(GTK_PANED(center_paned_), detail_panel_->widget(), FALSE, FALSE);

    // Status bar
    statusbar_ = std::make_unique<StatusBar>();

    gtk_box_pack_start(GTK_BOX(content_vbox_), center_paned_,       TRUE,  TRUE,  0);
    gtk_box_pack_start(GTK_BOX(content_vbox_), statusbar_->widget(), FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_hbox_), content_vbox_, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(window_), main_hbox_);
}

void MainWindow::buildNav() {
    std::vector<NavItem> items = {
        {"all",      "All Applications", "view-grid-symbolic",          AppCategory::OTHER},
        {"installed","Installed",         "emblem-installed-symbolic",   AppCategory::OTHER},
        // separator
        {"", "", "", AppCategory::OTHER, true},
        {"browser",  "Browsers",          "web-browser-symbolic",        AppCategory::BROWSER},
        {"dev",      "Development",       "applications-development",    AppCategory::DEV},
        {"media",    "Multimedia",        "applications-multimedia",     AppCategory::MEDIA},
        {"graphic",  "Graphics",          "applications-graphics",       AppCategory::GRAPHIC},
        {"office",   "Office",            "x-office-document",           AppCategory::OFFICE},
        {"game",     "Games",             "applications-games",          AppCategory::GAME},
        {"net",      "Network",           "network-workgroup-symbolic",  AppCategory::NET},
        {"util",     "Utilities",         "applications-utilities",      AppCategory::UTIL},
        {"sys",      "System",            "preferences-system",          AppCategory::SYS},
        {"other",    "Other",             "applications-other",          AppCategory::OTHER},
        // separator
        {"", "", "", AppCategory::OTHER, true},
        {"settings", "Settings",          "preferences-desktop",         AppCategory::OTHER},
    };
    nav_->setItems(items);
    nav_->setActive("all");
}

void MainWindow::connectSignals() {
    // Nav selection
    nav_->onSelect([this](const NavItem& item) {
        current_section_ = item.id;
        toolbar_->setTitle(item.label.empty() ? "All Applications" : item.label);
        if (item.id == "settings") {
            openThemePicker();
            nav_->setActive(current_section_);
            return;
        }
        deselectAll();
        detail_panel_->clear();
        populateApps(current_section_, current_search_);
        Logger::instance().logGUI(AppLogLevel::INFO,
            "Navigation: " + item.id);
    });

    // Search
    toolbar_->onSearch([this](const std::string& q) {
        current_search_ = q;
        populateApps(current_section_, q);
    });

    // View toggle
    toolbar_->onViewToggle([this](bool is_grid) {
        is_grid_view_ = is_grid;
        gtk_stack_set_visible_child_name(GTK_STACK(stack_views_),
            is_grid ? "grid" : "list");
        Logger::instance().logGUI(AppLogLevel::INFO,
            std::string("View: ") + (is_grid ? "grid" : "list"));
    });

    // Refresh
    toolbar_->onRefresh([this]() {
        if (repo_) {
            statusbar_->setMessage("Refreshing packages…");
            statusbar_->showSpinner(true);
            repo_->reload();
            populateApps(current_section_, current_search_);
            statusbar_->showSpinner(false);
            statusbar_->setMessage("Refreshed " + std::to_string(repo_->count()) + " packages");
            Logger::instance().logGUI(AppLogLevel::INFO, "Repository refreshed");
        }
    });

    // Theme picker
    toolbar_->onThemePickerOpen([this]() {
        openThemePicker();
    });

    // Install / uninstall from detail panel
    detail_panel_->onInstall([this](const std::string& id) {
        beginInstall(id);
    });
    detail_panel_->onUninstall([this](const std::string& id) {
        beginUninstall(id);
    });
}

// ─── Populate ───────────────────────────────────────────────────────────────

std::vector<const AppPackage*> MainWindow::filterPackages(
    const std::string& section, const std::string& search)
{
    if (!repo_) return {};

    std::vector<const AppPackage*> result;

    if (!search.empty()) {
        result = repo_->search(search);
    } else if (section == "all") {
        for (auto& p : repo_->allPackages()) result.push_back(&p);
    } else if (section == "installed") {
        for (auto& p : repo_->allPackages()) {
            if (InstallManager::instance().checkStatus(p) == InstallStatus::INSTALLED)
                result.push_back(&p);
        }
    } else {
        // Category
        static std::unordered_map<std::string, AppCategory> cat_map = {
            {"browser", AppCategory::BROWSER}, {"dev",    AppCategory::DEV},
            {"media",   AppCategory::MEDIA},   {"graphic",AppCategory::GRAPHIC},
            {"office",  AppCategory::OFFICE},  {"game",   AppCategory::GAME},
            {"net",     AppCategory::NET},     {"util",   AppCategory::UTIL},
            {"sys",     AppCategory::SYS},     {"other",  AppCategory::OTHER},
        };
        auto it = cat_map.find(section);
        if (it != cat_map.end()) {
            result = repo_->findByCategory(it->second);
        }
    }
    return result;
}

void MainWindow::populateApps(const std::string& section, const std::string& search) {
    auto pkgs = filterPackages(section, search);
    clearViews();
    populateGridView(pkgs);
    populateListView(pkgs);

    // Update status bar count
    if (repo_) {
        int total = (int)repo_->count();
        int installed = 0;
        for (auto& p : repo_->allPackages()) {
            if (InstallManager::instance().checkStatus(p) == InstallStatus::INSTALLED)
                installed++;
        }
        statusbar_->setAppCount(total, installed);
    }
}

void MainWindow::clearViews() {
    grid_cards_.clear();
    list_rows_.clear();

    GList* gc = gtk_container_get_children(GTK_CONTAINER(grid_flow_));
    for (GList* c = gc; c; c=c->next) gtk_widget_destroy(GTK_WIDGET(c->data));
    g_list_free(gc);

    GList* lc = gtk_container_get_children(GTK_CONTAINER(list_box_));
    for (GList* c = lc; c; c=c->next) gtk_widget_destroy(GTK_WIDGET(c->data));
    g_list_free(lc);
}

void MainWindow::populateGridView(const std::vector<const AppPackage*>& pkgs) {
    for (auto* pkg : pkgs) {
        auto card = std::make_unique<AppCard>(*pkg);
        InstallStatus status = InstallManager::instance().checkStatus(*pkg);
        card->updateStatus(status);

        card->onClick([this](const std::string& id) {
            selectApp(id);
        });

        gtk_container_add(GTK_CONTAINER(grid_flow_), card->widget());
        grid_cards_[pkg->id] = std::move(card);
    }
    gtk_widget_show_all(grid_flow_);
}

void MainWindow::populateListView(const std::vector<const AppPackage*>& pkgs) {
    for (auto* pkg : pkgs) {
        auto row = std::make_unique<AppListRow>(*pkg);
        InstallStatus status = InstallManager::instance().checkStatus(*pkg);
        row->updateStatus(status);

        row->onClick([this](const std::string& id) {
            selectApp(id);
        });

        // Wrap in GtkListBoxRow
        GtkWidget* lb_row = gtk_list_box_row_new();
        gtk_style_context_add_class(gtk_widget_get_style_context(lb_row), "app-list-row-wrapper");
        gtk_container_add(GTK_CONTAINER(lb_row), row->widget());
        gtk_container_add(GTK_CONTAINER(list_box_), lb_row);
        list_rows_[pkg->id] = std::move(row);
    }
    gtk_widget_show_all(list_box_);
}

// ─── Selection ──────────────────────────────────────────────────────────────

void MainWindow::selectApp(const std::string& id) {
    if (!repo_) return;
    deselectAll();
    selected_app_id_ = id;

    // Mark selected in grid
    auto git = grid_cards_.find(id);
    if (git != grid_cards_.end()) git->second->setSelected(true);

    // Mark selected in list
    auto lit = list_rows_.find(id);
    if (lit != list_rows_.end()) lit->second->setSelected(true);

    // Show in detail panel
    const AppPackage* pkg = repo_->findById(id);
    if (pkg) {
        InstallStatus status = InstallManager::instance().checkStatus(*pkg);
        detail_panel_->showApp(*pkg, status);
        statusbar_->setMessage(pkg->name);
        Logger::instance().logGUI(AppLogLevel::INFO, "Selected: " + pkg->name);
    }
}

void MainWindow::deselectAll() {
    for (auto& kv : grid_cards_) kv.second->setSelected(false);
    for (auto& kv : list_rows_)  kv.second->setSelected(false);
    selected_app_id_.clear();
}

// ─── Operations ─────────────────────────────────────────────────────────────

void MainWindow::beginInstall(const std::string& id) {
    if (!repo_) return;
    const AppPackage* pkg = repo_->findById(id);
    if (!pkg) return;

    console_->setVisible(true);
    console_->clear();
    console_->setTitle("Installing: " + pkg->name);
    console_->appendRaw(LogLevel::INFO,
        "━━━ Installing " + pkg->name + " v" + pkg->version + " ━━━");

    statusbar_->setMessage("Installing " + pkg->name + "…");
    statusbar_->showSpinner(true);
    statusbar_->setProgress(0.0);

    detail_panel_->updateStatus(InstallStatus::INSTALLING);

    auto git = grid_cards_.find(id);
    if (git != grid_cards_.end()) git->second->updateStatus(InstallStatus::INSTALLING);
    auto lit = list_rows_.find(id);
    if (lit != list_rows_.end()) lit->second->updateStatus(InstallStatus::INSTALLING);

    bool started = InstallManager::instance().install(*pkg,
        [this](const LogLine& line) {
            console_->appendLine(line);
        },
        [this, id](bool success, int exit_code) {
            onOperationFinished(id, OperationType::INSTALL, success, exit_code);
        }
    );

    if (!started) {
        console_->appendRaw(LogLevel::ERROR, "Another operation is already running for this app.");
        statusbar_->setMessage("Operation already running");
        statusbar_->showSpinner(false);
        statusbar_->setProgress(-1);
    }
}

void MainWindow::beginUninstall(const std::string& id) {
    if (!repo_) return;
    const AppPackage* pkg = repo_->findById(id);
    if (!pkg) return;

    // Confirmation dialog
    GtkWidget* confirm = gtk_message_dialog_new(
        GTK_WINDOW(window_),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "Uninstall %s?", pkg->name.c_str());
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(confirm),
        "This will remove %s from your system.", pkg->name.c_str());
    gint resp = gtk_dialog_run(GTK_DIALOG(confirm));
    gtk_widget_destroy(confirm);
    if (resp != GTK_RESPONSE_YES) return;

    console_->setVisible(true);
    console_->clear();
    console_->setTitle("Uninstalling: " + pkg->name);
    console_->appendRaw(LogLevel::INFO,
        "━━━ Uninstalling " + pkg->name + " ━━━");

    statusbar_->setMessage("Uninstalling " + pkg->name + "…");
    statusbar_->showSpinner(true);
    statusbar_->setProgress(0.0);

    detail_panel_->updateStatus(InstallStatus::UNINSTALLING);

    auto git = grid_cards_.find(id);
    if (git != grid_cards_.end()) git->second->updateStatus(InstallStatus::UNINSTALLING);
    auto lit = list_rows_.find(id);
    if (lit != list_rows_.end()) lit->second->updateStatus(InstallStatus::UNINSTALLING);

    InstallManager::instance().uninstall(*pkg,
        [this](const LogLine& line) {
            console_->appendLine(line);
        },
        [this, id](bool success, int exit_code) {
            onOperationFinished(id, OperationType::UNINSTALL, success, exit_code);
        }
    );
}

void MainWindow::onOperationFinished(const std::string& id, OperationType type,
                                      bool success, int exit_code)
{
    const auto* pkg = repo_ ? repo_->findById(id) : nullptr;
    std::string op_str = (type == OperationType::INSTALL) ? "install" : "uninstall";
    std::string name   = pkg ? pkg->name : id;

    statusbar_->showSpinner(false);
    statusbar_->setProgress(-1);

    if (success) {
        std::string msg = name + " " + op_str + " completed successfully.";
        statusbar_->setMessage(msg);
        console_->appendRaw(LogLevel::SUCCESS, "━━━ Done: " + msg + " ━━━");
        Logger::instance().logGUI(AppLogLevel::INFO, msg);

        InstallStatus new_status = (type == OperationType::INSTALL)
            ? InstallStatus::INSTALLED : InstallStatus::NOT_INSTALLED;

        auto git = grid_cards_.find(id);
        if (git != grid_cards_.end()) git->second->updateStatus(new_status);
        auto lit = list_rows_.find(id);
        if (lit != list_rows_.end()) lit->second->updateStatus(new_status);

        if (id == selected_app_id_)
            detail_panel_->updateStatus(new_status);

        if (ThemeManager::instance().currentTheme().misc.collapse_console_on_finish)
            console_->setVisible(false);

    } else {
        std::string msg = name + " " + op_str + " FAILED (exit " +
                          std::to_string(exit_code) + ")";
        statusbar_->setMessage(msg);
        console_->appendRaw(LogLevel::ERROR, "━━━ Error: " + msg + " ━━━");
        Logger::instance().logGUI(AppLogLevel::ERROR, msg);

        if (id == selected_app_id_)
            detail_panel_->updateStatus(InstallStatus::ERROR);
    }
}

// ─── Theme ──────────────────────────────────────────────────────────────────

void MainWindow::openThemePicker() {
    ThemePickerDialog picker(GTK_WINDOW(window_));
    std::string path = picker.run();
    if (!path.empty()) {
        if (!ThemeManager::instance().loadTheme(path)) {
            GtkWidget* err = gtk_message_dialog_new(
                GTK_WINDOW(window_), GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                "Failed to load theme:\n%s", path.c_str());
            gtk_dialog_run(GTK_DIALOG(err));
            gtk_widget_destroy(err);
        } else {
            Logger::instance().logGUI(AppLogLevel::INFO, "Theme loaded: " + path);
            statusbar_->setMessage("Theme applied: " +
                ThemeManager::instance().currentTheme().name);
        }
    }
}

void MainWindow::applyTheme(const ThemeConfig& cfg) {
    // Update window size constraints
    gtk_widget_set_size_request(window_, cfg.layout.window_min_width, cfg.layout.window_min_height);
    // Update nav width
    gtk_widget_set_size_request(nav_->widget(), cfg.layout.nav_width, -1);
    // Update console height
    gtk_widget_set_size_request(console_->widget(), -1, cfg.layout.console_height);
    // Update flow box columns
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(grid_flow_), cfg.misc.grid_columns);
}

std::string MainWindow::categoryLabel(AppCategory cat) const {
    switch(cat) {
        case AppCategory::BROWSER: return "Browser";
        case AppCategory::EDITOR:  return "Editor";
        case AppCategory::MEDIA:   return "Media";
        case AppCategory::DEV:     return "Development";
        case AppCategory::GAME:    return "Game";
        case AppCategory::UTIL:    return "Utility";
        case AppCategory::SYS:     return "System";
        case AppCategory::NET:     return "Network";
        case AppCategory::GRAPHIC: return "Graphics";
        case AppCategory::OFFICE:  return "Office";
        default:                   return "Other";
    }
}

void MainWindow::onDeleteEvent(GtkWidget*, GdkEvent*, gpointer) {
    Logger::instance().logGUI(AppLogLevel::INFO, "App Explorer closed");
}

} // namespace AppExplorer
