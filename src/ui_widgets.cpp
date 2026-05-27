// ─────────────────────────────────────────────────────────────────────────────
// App Explorer — ui_widgets.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "ui_widgets.hpp"
#include "theme_config.hpp"
#include <cstring>
#include <algorithm>
#include <sstream>

namespace AppExplorer {

// ─────────────────────────────────────────────────────────────────────────────
// ConsoleView
// ─────────────────────────────────────────────────────────────────────────────

ConsoleView::ConsoleView() {
    box_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(box_, "console-view");
    gtk_style_context_add_class(gtk_widget_get_style_context(box_), "console-view");

    // Header
    header_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_name(header_, "console-header");
    gtk_style_context_add_class(gtk_widget_get_style_context(header_), "console-header");
    gtk_widget_set_margin_start(header_, 0);

    label_title_ = gtk_label_new("Console");
    gtk_style_context_add_class(gtk_widget_get_style_context(label_title_), "console-header");
    gtk_widget_set_hexpand(label_title_, TRUE);
    gtk_label_set_xalign(GTK_LABEL(label_title_), 0.0);
    gtk_widget_set_margin_start(label_title_, 12);
    gtk_widget_set_margin_top(label_title_, 4);
    gtk_widget_set_margin_bottom(label_title_, 4);

    btn_toggle_ = gtk_button_new_with_label("▼");
    btn_clear_  = gtk_button_new_with_label("Clear");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_toggle_), "btn-icon");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_clear_),  "btn-small");

    gtk_box_pack_start(GTK_BOX(header_), label_title_, TRUE, TRUE, 0);
    gtk_box_pack_end  (GTK_BOX(header_), btn_toggle_, FALSE, FALSE, 4);
    gtk_box_pack_end  (GTK_BOX(header_), btn_clear_,  FALSE, FALSE, 4);

    // ScrolledWindow + TextView
    scroll_    = gtk_scrolled_window_new(nullptr, nullptr);
    text_view_ = gtk_text_view_new();
    buffer_    = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view_));

    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view_), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view_), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view_), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_view_), 12);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_view_), 12);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(text_view_), 8);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(text_view_), 8);

    gtk_widget_set_name(text_view_, "console-text");
    gtk_style_context_add_class(gtk_widget_get_style_context(text_view_), "console-text");

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll_), text_view_);
    gtk_widget_set_size_request(scroll_, -1, 200);

    gtk_box_pack_start(GTK_BOX(box_), header_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box_), scroll_, TRUE, TRUE, 0);

    setupTags();

    g_signal_connect(btn_toggle_, "clicked", G_CALLBACK(onToggleClicked), this);
    g_signal_connect(btn_clear_,  "clicked", G_CALLBACK(onClearClicked), this);

    gtk_widget_show_all(box_);
}

ConsoleView::~ConsoleView() {}

GtkWidget* ConsoleView::widget() { return box_; }

void ConsoleView::setupTags() {
    const auto& cfg = ThemeManager::instance().currentTheme();
    // Tags sẽ được update khi theme thay đổi
    tag_cmd_     = gtk_text_buffer_create_tag(buffer_, "cmd",
                    "foreground", cfg.console.text_cmd.c_str(),
                    "weight", PANGO_WEIGHT_BOLD, nullptr);
    tag_stdout_  = gtk_text_buffer_create_tag(buffer_, "stdout",
                    "foreground", cfg.console.text_stdout.c_str(), nullptr);
    tag_stderr_  = gtk_text_buffer_create_tag(buffer_, "stderr",
                    "foreground", cfg.console.text_stderr.c_str(), nullptr);
    tag_error_   = gtk_text_buffer_create_tag(buffer_, "error",
                    "foreground", cfg.console.text_error.c_str(),
                    "weight", PANGO_WEIGHT_BOLD, nullptr);
    tag_success_ = gtk_text_buffer_create_tag(buffer_, "success",
                    "foreground", cfg.console.text_success.c_str(), nullptr);
    tag_info_    = gtk_text_buffer_create_tag(buffer_, "info",
                    "foreground", cfg.console.text_info.c_str(), nullptr);
}

void ConsoleView::appendLine(const LogLine& line) {
    appendRaw(line.level, line.text);
}

void ConsoleView::appendRaw(LogLevel level, const std::string& text) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer_, &end);

    GtkTextTag* tag = nullptr;
    switch (level) {
        case LogLevel::CMD:     tag = tag_cmd_;     break;
        case LogLevel::STDOUT:  tag = tag_stdout_;  break;
        case LogLevel::STDERR:  tag = tag_stderr_;  break;
        case LogLevel::ERROR:   tag = tag_error_;   break;
        case LogLevel::SUCCESS: tag = tag_success_; break;
        case LogLevel::INFO:    tag = tag_info_;    break;
    }

    std::string line = text + "\n";
    if (tag) {
        gtk_text_buffer_insert_with_tags(buffer_, &end,
            line.c_str(), (gint)line.size(), tag, nullptr);
    } else {
        gtk_text_buffer_insert(buffer_, &end, line.c_str(), (gint)line.size());
    }

    scrollToEnd();
}

void ConsoleView::clear() {
    gtk_text_buffer_set_text(buffer_, "", 0);
}

void ConsoleView::scrollToEnd() {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer_, &end);
    GtkTextMark* mark = gtk_text_buffer_create_mark(buffer_, nullptr, &end, FALSE);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(text_view_), mark);
    gtk_text_buffer_delete_mark(buffer_, mark);
}

void ConsoleView::setVisible(bool v) {
    expanded_ = v;
    if (v) {
        gtk_widget_show(scroll_);
        gtk_button_set_label(GTK_BUTTON(btn_toggle_), "▼");
    } else {
        gtk_widget_hide(scroll_);
        gtk_button_set_label(GTK_BUTTON(btn_toggle_), "▶");
    }
}

bool ConsoleView::isVisible() const { return expanded_; }

void ConsoleView::setTitle(const std::string& title) {
    gtk_label_set_text(GTK_LABEL(label_title_), title.c_str());
}

void ConsoleView::onToggleClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<ConsoleView*>(data);
    self->setVisible(!self->expanded_);
}

void ConsoleView::onClearClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<ConsoleView*>(data);
    self->clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// NavSidebar
// ─────────────────────────────────────────────────────────────────────────────

NavSidebar::NavSidebar() {
    box_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(box_), "nav-sidebar");
    gtk_widget_set_size_request(box_,
        ThemeManager::instance().currentTheme().layout.nav_width, -1);

    // Logo
    logo_box_   = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    logo_label_ = gtk_label_new("App Explorer");
    gtk_style_context_add_class(gtk_widget_get_style_context(logo_label_), "nav-logo");
    gtk_label_set_xalign(GTK_LABEL(logo_label_), 0.0);
    gtk_widget_set_margin_start(logo_label_, 16);
    gtk_widget_set_margin_top(logo_label_, 16);
    gtk_widget_set_margin_bottom(logo_label_, 8);
    gtk_box_pack_start(GTK_BOX(logo_box_), logo_label_, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box_), logo_box_, FALSE, FALSE, 0);

    // Separator after logo
    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_style_context_add_class(gtk_widget_get_style_context(sep), "nav-separator");
    gtk_box_pack_start(GTK_BOX(box_), sep, FALSE, FALSE, 4);

    // Items container
    items_box_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(items_box_, 8);
    gtk_widget_set_margin_end(items_box_, 8);
    gtk_widget_set_margin_top(items_box_, 4);
    gtk_box_pack_start(GTK_BOX(box_), items_box_, TRUE, TRUE, 0);

    gtk_widget_show_all(box_);
}

NavSidebar::~NavSidebar() {}

GtkWidget* NavSidebar::widget() { return box_; }

void NavSidebar::setItems(const std::vector<NavItem>& items) {
    // Clear existing
    GList* children = gtk_container_get_children(GTK_CONTAINER(items_box_));
    for (GList* c = children; c; c = c->next)
        gtk_widget_destroy(GTK_WIDGET(c->data));
    g_list_free(children);
    nav_buttons_.clear();

    for (const auto& item : items) {
        if (item.is_separator) {
            GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
            gtk_style_context_add_class(gtk_widget_get_style_context(sep), "nav-separator");
            gtk_box_pack_start(GTK_BOX(items_box_), sep, FALSE, FALSE, 4);
            continue;
        }
        GtkWidget* btn = makeNavButton(item);
        NavButton nb;
        nb.item   = item;
        nb.btn    = btn;
        nav_buttons_.push_back(nb);
        gtk_box_pack_start(GTK_BOX(items_box_), btn, FALSE, FALSE, 0);
    }
    gtk_widget_show_all(items_box_);
}

GtkWidget* NavSidebar::makeNavButton(const NavItem& item) {
    GtkWidget* btn = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(btn), "nav-item");
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);

    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(hbox, 4);

    // Icon
    if (!item.icon_name.empty()) {
        GtkWidget* icon = gtk_image_new_from_icon_name(
            item.icon_name.c_str(), GTK_ICON_SIZE_SMALL_TOOLBAR);
        gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, FALSE, 0);
    }

    // Label
    GtkWidget* lbl = gtk_label_new(item.label.c_str());
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    gtk_widget_set_hexpand(lbl, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(btn), hbox);

    // Store item pointer for callback
    auto* item_copy = new NavItem(item);
    g_object_set_data_full(G_OBJECT(btn), "nav-item", item_copy,
        [](gpointer p){ delete static_cast<NavItem*>(p); });
    g_object_set_data(G_OBJECT(btn), "nav-sidebar", this);
    g_signal_connect(btn, "clicked", G_CALLBACK(onNavBtnClicked), this);

    return btn;
}

void NavSidebar::setActive(const std::string& id) {
    active_id_ = id;
    for (auto& nb : nav_buttons_) {
        bool is_active = (nb.item.id == id);
        auto ctx = gtk_widget_get_style_context(nb.btn);
        if (is_active) gtk_style_context_add_class(ctx, "active");
        else           gtk_style_context_remove_class(ctx, "active");
    }
}

void NavSidebar::setBadgeCount(const std::string& item_id, int count) {
    for (auto& nb : nav_buttons_) {
        if (nb.item.id == item_id && nb.badge) {
            gtk_label_set_text(GTK_LABEL(nb.badge), std::to_string(count).c_str());
            gtk_widget_set_visible(nb.badge, count > 0);
        }
    }
}

void NavSidebar::onNavBtnClicked(GtkButton* btn, gpointer data) {
    auto* self = static_cast<NavSidebar*>(data);
    auto* item = static_cast<NavItem*>(g_object_get_data(G_OBJECT(btn), "nav-item"));
    if (item && self->on_select_) {
        self->setActive(item->id);
        self->on_select_(*item);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Toolbar
// ─────────────────────────────────────────────────────────────────────────────

Toolbar::Toolbar() {
    box_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(box_), "toolbar");
    gtk_widget_set_size_request(box_, -1,
        ThemeManager::instance().currentTheme().layout.toolbar_height);

    // Title
    label_title_ = gtk_label_new("All Applications");
    gtk_style_context_add_class(gtk_widget_get_style_context(label_title_), "toolbar-title");
    gtk_label_set_xalign(GTK_LABEL(label_title_), 0.0);
    gtk_box_pack_start(GTK_BOX(box_), label_title_, FALSE, FALSE, 0);

    // Spacer
    GtkWidget* spacer = gtk_label_new("");
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(box_), spacer, TRUE, TRUE, 0);

    // Search
    search_entry_ = gtk_search_entry_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(search_entry_), "search-field");
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry_), "Search applications...");
    gtk_box_pack_start(GTK_BOX(box_), search_entry_, FALSE, FALSE, 0);

    // View toggle
    btn_grid_view_ = gtk_button_new_from_icon_name("view-grid-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    btn_list_view_ = gtk_button_new_from_icon_name("view-list-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_grid_view_), "btn-icon");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_list_view_), "btn-icon");
    gtk_widget_set_tooltip_text(btn_grid_view_, "Grid view");
    gtk_widget_set_tooltip_text(btn_list_view_, "List view");
    gtk_box_pack_start(GTK_BOX(box_), btn_grid_view_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box_), btn_list_view_, FALSE, FALSE, 0);

    // Refresh button
    btn_refresh_ = gtk_button_new_from_icon_name("view-refresh-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_refresh_), "btn-icon");
    gtk_widget_set_tooltip_text(btn_refresh_, "Refresh packages");
    gtk_box_pack_start(GTK_BOX(box_), btn_refresh_, FALSE, FALSE, 0);

    // Theme button
    btn_theme_ = gtk_button_new_from_icon_name("preferences-desktop-theme-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_theme_), "btn-icon");
    gtk_widget_set_tooltip_text(btn_theme_, "Change theme");
    gtk_box_pack_start(GTK_BOX(box_), btn_theme_, FALSE, FALSE, 0);

    // Signals
    g_signal_connect(search_entry_, "search-changed", G_CALLBACK(onSearchChanged), this);
    g_signal_connect(btn_grid_view_,"clicked", G_CALLBACK(onGridViewClicked), this);
    g_signal_connect(btn_list_view_,"clicked", G_CALLBACK(onListViewClicked), this);
    g_signal_connect(btn_refresh_,  "clicked", G_CALLBACK(onRefreshClicked), this);
    g_signal_connect(btn_theme_,    "clicked", G_CALLBACK(onThemeClicked), this);

    gtk_widget_show_all(box_);
}

Toolbar::~Toolbar() {}
GtkWidget* Toolbar::widget() { return box_; }

void Toolbar::setTitle(const std::string& title) {
    gtk_label_set_text(GTK_LABEL(label_title_), title.c_str());
}

std::string Toolbar::searchText() const {
    return gtk_entry_get_text(GTK_ENTRY(search_entry_));
}

void Toolbar::setViewMode(bool is_grid) {
    auto grid_ctx = gtk_widget_get_style_context(btn_grid_view_);
    auto list_ctx = gtk_widget_get_style_context(btn_list_view_);
    if (is_grid) {
        gtk_style_context_add_class(grid_ctx, "active");
        gtk_style_context_remove_class(list_ctx, "active");
    } else {
        gtk_style_context_remove_class(grid_ctx, "active");
        gtk_style_context_add_class(list_ctx, "active");
    }
}

void Toolbar::onSearchChanged(GtkSearchEntry* entry, gpointer data) {
    auto* self = static_cast<Toolbar*>(data);
    if (self->on_search_) {
        self->on_search_(gtk_entry_get_text(GTK_ENTRY(entry)));
    }
}

void Toolbar::onGridViewClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<Toolbar*>(data);
    self->setViewMode(true);
    if (self->on_view_toggle_) self->on_view_toggle_(true);
}

void Toolbar::onListViewClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<Toolbar*>(data);
    self->setViewMode(false);
    if (self->on_view_toggle_) self->on_view_toggle_(false);
}

void Toolbar::onRefreshClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<Toolbar*>(data);
    if (self->on_refresh_) self->on_refresh_();
}

void Toolbar::onThemeClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<Toolbar*>(data);
    if (self->on_theme_picker_) self->on_theme_picker_();
}

// ─────────────────────────────────────────────────────────────────────────────
// StatusBar
// ─────────────────────────────────────────────────────────────────────────────

StatusBar::StatusBar() {
    box_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(box_), "statusbar");

    label_msg_  = gtk_label_new("Ready");
    gtk_label_set_xalign(GTK_LABEL(label_msg_), 0.0);
    gtk_widget_set_hexpand(label_msg_, TRUE);

    progress_ = gtk_progress_bar_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(progress_), "statusbar-progress");
    gtk_widget_set_size_request(progress_, 120, -1);
    gtk_widget_hide(progress_);

    spinner_ = gtk_spinner_new();
    gtk_widget_hide(spinner_);

    label_count_ = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(label_count_), 1.0);

    gtk_box_pack_start(GTK_BOX(box_), label_msg_,  TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box_), progress_,   FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box_), spinner_,    FALSE, FALSE, 0);
    gtk_box_pack_end  (GTK_BOX(box_), label_count_, FALSE, FALSE, 0);

    gtk_widget_show_all(box_);
    gtk_widget_hide(spinner_);
    gtk_widget_hide(progress_);
}

StatusBar::~StatusBar() {}
GtkWidget* StatusBar::widget() { return box_; }

void StatusBar::setMessage(const std::string& msg) {
    gtk_label_set_text(GTK_LABEL(label_msg_), msg.c_str());
}

void StatusBar::setProgress(double fraction) {
    if (fraction < 0) {
        gtk_widget_hide(progress_);
    } else {
        gtk_widget_show(progress_);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_),
            std::max(0.0, std::min(1.0, fraction)));
    }
}

void StatusBar::showSpinner(bool show) {
    if (show) {
        gtk_widget_show(spinner_);
        gtk_spinner_start(GTK_SPINNER(spinner_));
    } else {
        gtk_spinner_stop(GTK_SPINNER(spinner_));
        gtk_widget_hide(spinner_);
    }
}

void StatusBar::setAppCount(int total, int installed) {
    std::string txt = std::to_string(total) + " packages  |  "
                    + std::to_string(installed) + " installed";
    gtk_label_set_text(GTK_LABEL(label_count_), txt.c_str());
}

// ─────────────────────────────────────────────────────────────────────────────
// DetailPanel
// ─────────────────────────────────────────────────────────────────────────────

DetailPanel::DetailPanel() {
    root_box_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(root_box_), "detail-panel");
    buildLayout();
}

DetailPanel::~DetailPanel() {}

GtkWidget* DetailPanel::widget() { return root_box_; }

void DetailPanel::buildLayout() {
    scroll_      = gtk_scrolled_window_new(nullptr, nullptr);
    content_box_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(content_box_, 4);
    gtk_widget_set_margin_end(content_box_, 4);
    gtk_widget_set_margin_top(content_box_, 4);
    gtk_widget_set_margin_bottom(content_box_, 4);

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll_), content_box_);
    gtk_widget_set_vexpand(scroll_, TRUE);

    // Icon + Name header
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    icon_widget_  = gtk_image_new_from_icon_name("application-x-executable", GTK_ICON_SIZE_DIALOG);
    gtk_widget_set_size_request(icon_widget_, 64, 64);

    GtkWidget* title_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    label_name_    = gtk_label_new("Select an application");
    label_version_ = gtk_label_new("");
    label_author_  = gtk_label_new("");

    gtk_style_context_add_class(gtk_widget_get_style_context(label_name_),    "detail-app-name");
    gtk_style_context_add_class(gtk_widget_get_style_context(label_version_), "text-muted");
    gtk_style_context_add_class(gtk_widget_get_style_context(label_author_),  "text-muted");

    gtk_label_set_xalign(GTK_LABEL(label_name_),    0);
    gtk_label_set_xalign(GTK_LABEL(label_version_), 0);
    gtk_label_set_xalign(GTK_LABEL(label_author_),  0);
    gtk_label_set_line_wrap(GTK_LABEL(label_name_), TRUE);

    // Badges row
    GtkWidget* badges = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    badge_cat_     = gtk_label_new("");
    badge_license_ = gtk_label_new("");
    gtk_style_context_add_class(gtk_widget_get_style_context(badge_cat_),     "badge");
    gtk_style_context_add_class(gtk_widget_get_style_context(badge_license_), "badge");
    gtk_box_pack_start(GTK_BOX(badges), badge_cat_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(badges), badge_license_, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(title_col), label_name_,    FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(title_col), label_version_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(title_col), label_author_,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(title_col), badges,         FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(header), icon_widget_,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), title_col,     TRUE, TRUE, 0);

    // Description
    GtkWidget* sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    label_desc_title_ = gtk_label_new("Description");
    gtk_style_context_add_class(gtk_widget_get_style_context(label_desc_title_), "detail-section-title");
    gtk_label_set_xalign(GTK_LABEL(label_desc_title_), 0);

    label_desc_ = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(label_desc_), 0);
    gtk_label_set_line_wrap(GTK_LABEL(label_desc_), TRUE);
    gtk_label_set_selectable(GTK_LABEL(label_desc_), TRUE);

    label_longdesc_ = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(label_longdesc_), 0);
    gtk_label_set_line_wrap(GTK_LABEL(label_longdesc_), TRUE);
    gtk_label_set_selectable(GTK_LABEL(label_longdesc_), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(label_longdesc_), "text-muted");

    // Usage
    GtkWidget* sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    label_usage_title_ = gtk_label_new("Usage");
    gtk_style_context_add_class(gtk_widget_get_style_context(label_usage_title_), "detail-section-title");
    gtk_label_set_xalign(GTK_LABEL(label_usage_title_), 0);

    label_usage_ = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(label_usage_), 0);
    gtk_label_set_line_wrap(GTK_LABEL(label_usage_), TRUE);
    gtk_label_set_selectable(GTK_LABEL(label_usage_), TRUE);

    // Info grid
    grid_info_ = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid_info_), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid_info_), 4);

    // Tags
    tags_flow_ = gtk_flow_box_new();
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(tags_flow_), 5);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(tags_flow_), GTK_SELECTION_NONE);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(tags_flow_), 4);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(tags_flow_), 4);

    // Action buttons
    GtkWidget* sep3 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    action_box_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(action_box_, 8);

    btn_install_   = gtk_button_new_with_label("Install");
    btn_uninstall_ = gtk_button_new_with_label("Uninstall");
    btn_homepage_  = gtk_button_new_with_label("Homepage");
    spinner_       = gtk_spinner_new();
    label_status_  = gtk_label_new("");

    gtk_style_context_add_class(gtk_widget_get_style_context(btn_install_),   "btn-primary");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_uninstall_), "btn-danger");
    gtk_style_context_add_class(gtk_widget_get_style_context(label_status_),  "text-muted");

    gtk_box_pack_start(GTK_BOX(action_box_), btn_install_,   FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(action_box_), btn_uninstall_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(action_box_), btn_homepage_,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(action_box_), spinner_,       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(action_box_), label_status_,  FALSE, FALSE, 0);

    // Pack everything
    gtk_box_pack_start(GTK_BOX(content_box_), header,            FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content_box_), sep1,              FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(content_box_), label_desc_title_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content_box_), label_desc_,       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content_box_), label_longdesc_,   FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content_box_), sep2,              FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(content_box_), label_usage_title_,FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content_box_), label_usage_,      FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content_box_), grid_info_,        FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(content_box_), tags_flow_,        FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content_box_), sep3,              FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(content_box_), action_box_,       FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(root_box_), scroll_, TRUE, TRUE, 0);

    // Signals
    g_signal_connect(btn_install_,   "clicked", G_CALLBACK(onInstallClicked),   this);
    g_signal_connect(btn_uninstall_, "clicked", G_CALLBACK(onUninstallClicked), this);
    g_signal_connect(btn_homepage_,  "clicked", G_CALLBACK(onHomepageClicked),  this);

    gtk_widget_show_all(root_box_);
    gtk_widget_hide(spinner_);
    gtk_widget_hide(btn_uninstall_);
    gtk_widget_hide(btn_homepage_);
    gtk_widget_hide(label_usage_title_);
    gtk_widget_hide(label_usage_);
    gtk_widget_set_sensitive(btn_install_, FALSE);
}

void DetailPanel::showApp(const AppPackage& pkg, InstallStatus status) {
    current_pkg_    = &pkg;
    current_status_ = status;

    gtk_label_set_text(GTK_LABEL(label_name_),    pkg.name.c_str());
    gtk_label_set_text(GTK_LABEL(label_version_),
        ("v" + pkg.version).c_str());
    gtk_label_set_text(GTK_LABEL(label_author_),
        pkg.author.empty() ? "" : ("by " + pkg.author).c_str());
    gtk_label_set_text(GTK_LABEL(label_desc_),     pkg.description.c_str());
    gtk_label_set_text(GTK_LABEL(label_longdesc_), pkg.long_desc.c_str());
    gtk_widget_set_visible(label_longdesc_, !pkg.long_desc.empty());

    // Category badge
    gtk_label_set_text(GTK_LABEL(badge_cat_), pkg.category_str.c_str());
    gtk_widget_set_visible(badge_cat_, !pkg.category_str.empty());

    // License badge
    gtk_label_set_text(GTK_LABEL(badge_license_), pkg.license.c_str());
    gtk_widget_set_visible(badge_license_, !pkg.license.empty());

    // Usage
    if (!pkg.usage.empty()) {
        gtk_label_set_text(GTK_LABEL(label_usage_), pkg.usage.c_str());
        gtk_widget_show(label_usage_title_);
        gtk_widget_show(label_usage_);
    } else {
        gtk_widget_hide(label_usage_title_);
        gtk_widget_hide(label_usage_);
    }

    // Info grid
    populateInfoGrid(pkg);

    // Tags
    populateTags(pkg);

    // Homepage button
    gtk_widget_set_visible(btn_homepage_, !pkg.homepage.empty());

    // Load icon
    loadIcon(pkg);

    // Refresh action buttons
    updateStatus(status);

    gtk_widget_show_all(root_box_);
    gtk_widget_hide(spinner_);
}

void DetailPanel::loadIcon(const AppPackage& pkg) {
    std::string path = pkg.resolvedIconPath();
    if (!path.empty() && !pkg.iconIsOnline()) {
        GdkPixbuf* pb = gdk_pixbuf_new_from_file_at_scale(
            path.c_str(), 64, 64, TRUE, nullptr);
        if (pb) {
            gtk_image_set_from_pixbuf(GTK_IMAGE(icon_widget_), pb);
            g_object_unref(pb);
            return;
        }
    }
    gtk_image_set_from_icon_name(GTK_IMAGE(icon_widget_),
        "application-x-executable", GTK_ICON_SIZE_DIALOG);
}

void DetailPanel::populateInfoGrid(const AppPackage& pkg) {
    // Clear
    GList* children = gtk_container_get_children(GTK_CONTAINER(grid_info_));
    for (GList* c = children; c; c=c->next)
        gtk_widget_destroy(GTK_WIDGET(c->data));
    g_list_free(children);

    auto addRow = [&](int row, const std::string& key, const std::string& val) {
        if (val.empty()) return;
        GtkWidget* klbl = gtk_label_new(key.c_str());
        GtkWidget* vlbl = gtk_label_new(val.c_str());
        gtk_label_set_xalign(GTK_LABEL(klbl), 0);
        gtk_label_set_xalign(GTK_LABEL(vlbl), 0);
        gtk_label_set_line_wrap(GTK_LABEL(vlbl), TRUE);
        gtk_label_set_selectable(GTK_LABEL(vlbl), TRUE);
        gtk_style_context_add_class(gtk_widget_get_style_context(klbl), "text-muted");
        gtk_grid_attach(GTK_GRID(grid_info_), klbl, 0, row, 1, 1);
        gtk_grid_attach(GTK_GRID(grid_info_), vlbl, 1, row, 1, 1);
    };

    int row = 0;
    addRow(row++, "Method",    pkg.category_str);
    addRow(row++, "Size",      pkg.estimated_size);
    addRow(row++, "Download",  pkg.download_size);
    addRow(row++, "Maintainer",pkg.maintainer);
    addRow(row++, "License",   pkg.license);

    gtk_widget_show_all(grid_info_);
}

void DetailPanel::populateTags(const AppPackage& pkg) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(tags_flow_));
    for (GList* c = children; c; c=c->next) gtk_widget_destroy(GTK_WIDGET(c->data));
    g_list_free(children);

    for (const auto& tag : pkg.tags) {
        GtkWidget* lbl = gtk_label_new(tag.c_str());
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "tag");
        gtk_container_add(GTK_CONTAINER(tags_flow_), lbl);
    }
    gtk_widget_show_all(tags_flow_);
}

void DetailPanel::updateStatus(InstallStatus status) {
    current_status_ = status;
    gtk_widget_hide(spinner_);
    gtk_label_set_text(GTK_LABEL(label_status_), "");

    switch (status) {
        case InstallStatus::NOT_INSTALLED:
            gtk_widget_set_sensitive(btn_install_, TRUE);
            gtk_button_set_label(GTK_BUTTON(btn_install_), "Install");
            gtk_widget_show(btn_install_);
            gtk_widget_hide(btn_uninstall_);
            break;
        case InstallStatus::INSTALLED:
            gtk_widget_set_sensitive(btn_install_, FALSE);
            gtk_button_set_label(GTK_BUTTON(btn_install_), "Installed ✓");
            gtk_widget_show(btn_install_);
            gtk_widget_show(btn_uninstall_);
            break;
        case InstallStatus::INSTALLING:
            gtk_widget_set_sensitive(btn_install_, FALSE);
            gtk_button_set_label(GTK_BUTTON(btn_install_), "Installing...");
            gtk_widget_show(btn_install_);
            gtk_widget_hide(btn_uninstall_);
            gtk_widget_show(spinner_);
            gtk_spinner_start(GTK_SPINNER(spinner_));
            gtk_label_set_text(GTK_LABEL(label_status_), "Installing...");
            break;
        case InstallStatus::UNINSTALLING:
            gtk_widget_set_sensitive(btn_uninstall_, FALSE);
            gtk_widget_show(spinner_);
            gtk_spinner_start(GTK_SPINNER(spinner_));
            gtk_label_set_text(GTK_LABEL(label_status_), "Uninstalling...");
            break;
        case InstallStatus::ERROR:
            gtk_widget_set_sensitive(btn_install_, TRUE);
            gtk_button_set_label(GTK_BUTTON(btn_install_), "Retry Install");
            gtk_label_set_text(GTK_LABEL(label_status_), "Error occurred");
            gtk_style_context_add_class(gtk_widget_get_style_context(label_status_), "text-error");
            break;
        default:
            gtk_widget_set_sensitive(btn_install_, FALSE);
            break;
    }
}

void DetailPanel::clear() {
    current_pkg_ = nullptr;
    gtk_label_set_text(GTK_LABEL(label_name_),    "Select an application");
    gtk_label_set_text(GTK_LABEL(label_version_), "");
    gtk_label_set_text(GTK_LABEL(label_author_),  "");
    gtk_label_set_text(GTK_LABEL(label_desc_),    "");
    gtk_label_set_text(GTK_LABEL(label_longdesc_),"");
    gtk_widget_set_sensitive(btn_install_, FALSE);
}

void DetailPanel::onInstallClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<DetailPanel*>(data);
    if (self->current_pkg_ && self->on_install_)
        self->on_install_(self->current_pkg_->id);
}

void DetailPanel::onUninstallClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<DetailPanel*>(data);
    if (self->current_pkg_ && self->on_uninstall_)
        self->on_uninstall_(self->current_pkg_->id);
}

void DetailPanel::onHomepageClicked(GtkButton*, gpointer data) {
    auto* self = static_cast<DetailPanel*>(data);
    if (self->current_pkg_ && !self->current_pkg_->homepage.empty()) {
        gtk_show_uri_on_window(nullptr,
            self->current_pkg_->homepage.c_str(), GDK_CURRENT_TIME, nullptr);
    }
}

} // namespace AppExplorer
