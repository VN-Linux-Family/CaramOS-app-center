#include "main_window.h"
#include "types.h"
#include <gtk/gtk.h>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <map>

// ─── Struct truyền qua gpointer ────────────────────────────
struct CardData {
    MainWindow* win;
    std::string app_id;
};

struct ThemeDialogData {
    MainWindow* win;
    // Các GtkEntry* màu sắc
    GtkEntry* e_bg_primary;
    GtkEntry* e_bg_secondary;
    GtkEntry* e_bg_card;
    GtkEntry* e_accent;
    GtkEntry* e_text_primary;
    GtkEntry* e_text_secondary;
    GtkEntry* e_border;
    GtkEntry* e_success;
    GtkEntry* e_button_bg;
    GtkEntry* e_button_text;
};

// ─── idle-add helpers (thread-safe UI update) ────────────────
struct LogData {
    MainWindow* win;
    std::string text;
    bool is_error;
};
struct ProgData {
    MainWindow* win;
    std::string app_id;
    double fraction;
    std::string status;
};
struct DoneData {
    MainWindow* win;
    std::vector<InstallResult> results;
};

static gboolean idle_log(gpointer p) {
    auto* d = static_cast<LogData*>(p);
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(d->win->log_buffer_, &end);
    std::string line = d->text + "\n";
    if (d->is_error)
        gtk_text_buffer_insert_with_tags_by_name(d->win->log_buffer_, &end, line.c_str(), -1, "error", nullptr);
    else
        gtk_text_buffer_insert(d->win->log_buffer_, &end, line.c_str(), -1);

    // Scroll to end
    GtkTextMark* mark = gtk_text_buffer_get_mark(d->win->log_buffer_, "insert");
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(d->win->log_view_), mark, 0.0, FALSE, 0, 0);
    delete d;
    return G_SOURCE_REMOVE;
}

static gboolean idle_prog(gpointer p) {
    auto* d = static_cast<ProgData*>(p);
    auto it = d->win->card_progress_.find(d->app_id);
    if (it != d->win->card_progress_.end()) {
        if (d->fraction >= 0)
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(it->second), d->fraction);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(it->second), d->status.c_str());
        gtk_widget_show(it->second);
    }
    delete d;
    return G_SOURCE_REMOVE;
}

static gboolean idle_done(gpointer p) {
    auto* d = static_cast<DoneData*>(p);
    int ok = 0, fail = 0;
    for (auto& r : d->results) (r.success ? ok : fail)++;

    std::string msg = "Hoàn tất: " + std::to_string(ok) + " thành công";
    if (fail > 0) msg += ", " + std::to_string(fail) + " thất bại";

    gtk_label_set_text(GTK_LABEL(d->win->status_label_), msg.c_str());
    gtk_widget_set_sensitive(d->win->install_btn_, TRUE);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(d->win->progress_bar_), 1.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(d->win->progress_bar_), msg.c_str());

    // Refresh danh sách để cập nhật badge installed
    d->win->apps_ = d->win->loader_.load_all();
    d->win->populate_app_grid(d->win->current_category_, d->win->current_search_);

    delete d;
    return G_SOURCE_REMOVE;
}

// ─── Constructor ─────────────────────────────────────────────
MainWindow::MainWindow(GtkApplication* app) : gtk_app_(app) {
    apps_ = loader_.load_all();
    build_ui();
    theme_mgr_.apply(window_, Theme::DARK);
}

MainWindow::~MainWindow() {
    delete installer_;
}

// ─── Build UI ────────────────────────────────────────────────
void MainWindow::build_ui() {
    window_ = gtk_application_window_new(gtk_app_);
    gtk_window_set_title(GTK_WINDOW(window_), APP_NAME " v" APP_VERSION);
    gtk_window_set_default_size(GTK_WINDOW(window_), 1100, 720);
    gtk_window_set_position(GTK_WINDOW(window_), GTK_WIN_POS_CENTER);
    gtk_widget_set_name(window_, "main-window");

    // Vertical box chính
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window_), vbox);

    // ── Header ──
    build_header();
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(g_object_get_data(G_OBJECT(window_), "header")), FALSE, FALSE, 0);

    // ── Horizontal paned: sidebar | main ──
    GtkWidget* hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(hpaned), 200);
    gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);

    build_sidebar();
    gtk_paned_add1(GTK_PANED(hpaned), GTK_WIDGET(g_object_get_data(G_OBJECT(window_), "sidebar")));

    // Right panel: grid + log
    GtkWidget* vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_set_position(GTK_PANED(vpaned), 480);
    gtk_paned_add2(GTK_PANED(hpaned), vpaned);

    build_app_grid();
    gtk_paned_add1(GTK_PANED(vpaned), GTK_WIDGET(g_object_get_data(G_OBJECT(window_), "grid_scroll")));

    build_log_panel();
    gtk_paned_add2(GTK_PANED(vpaned), GTK_WIDGET(g_object_get_data(G_OBJECT(window_), "log_panel")));

    gtk_widget_show_all(window_);
}

void MainWindow::build_header() {
    GtkWidget* hbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(hbar), "header");
    gtk_widget_set_margin_bottom(hbar, 0);

    // Tiêu đề
    GtkWidget* title = gtk_label_new(APP_NAME);
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "app-title");
    gtk_box_pack_start(GTK_BOX(hbar), title, FALSE, FALSE, 8);

    // Spacer
    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(hbar), spacer, TRUE, TRUE, 0);

    // Search
    search_entry_ = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry_), "Tìm kiếm phần mềm...");
    gtk_widget_set_size_request(search_entry_, 280, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(search_entry_), "search-entry");
    g_signal_connect(search_entry_, "search-changed", G_CALLBACK(on_search_changed), this);
    gtk_box_pack_start(GTK_BOX(hbar), search_entry_, FALSE, FALSE, 0);

    // Theme buttons
    GtkWidget* btn_light = gtk_button_new_with_label("☀ Sáng");
    GtkWidget* btn_dark  = gtk_button_new_with_label("🌙 Tối");
    GtkWidget* btn_cust  = gtk_button_new_with_label("🎨 Tuỳ chỉnh");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_light), "category-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_dark),  "category-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_cust),  "category-btn");
    g_signal_connect(btn_light, "clicked", G_CALLBACK(on_theme_light), this);
    g_signal_connect(btn_dark,  "clicked", G_CALLBACK(on_theme_dark),  this);
    g_signal_connect(btn_cust,  "clicked", G_CALLBACK(on_theme_custom), this);
    gtk_box_pack_start(GTK_BOX(hbar), btn_light, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbar), btn_dark,  FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbar), btn_cust,  FALSE, FALSE, 2);

    gtk_widget_set_margin_top(hbar, 0);

    g_object_set_data(G_OBJECT(window_), "header", hbar);
}

void MainWindow::build_sidebar() {
    GtkWidget* sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(sidebar), "sidebar");
    gtk_widget_set_size_request(sidebar, 180, -1);

    GtkWidget* lbl = gtk_label_new("DANH MỤC");
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "section-label");
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(sidebar), lbl, FALSE, FALSE, 0);

    const std::vector<std::pair<std::string,std::string>> cats = {
        {"all",         "🗂 Tất cả"},
        {"development", "💻 Lập trình"},
        {"office",      "📄 Văn phòng"},
        {"multimedia",  "🎬 Đa phương tiện"},
        {"internet",    "🌐 Internet"},
        {"utilities",   "🔧 Tiện ích"},
        {"games",       "🎮 Trò chơi"},
    };

    for (const auto& [id, label] : cats) {
        GtkWidget* btn = gtk_button_new_with_label(label.c_str());
        gtk_style_context_add_class(gtk_widget_get_style_context(btn), "category-btn");
        gtk_widget_set_halign(btn, GTK_ALIGN_FILL);
        if (id == "all")
            gtk_style_context_add_class(gtk_widget_get_style_context(btn), "active");

        struct CatBtnData { MainWindow* win; std::string cat; };
        auto* data = new CatBtnData{this, id};
        g_signal_connect_data(btn, "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer p) {
                auto* d = static_cast<CatBtnData*>(p);
                d->win->current_category_ = d->cat;
                d->win->populate_app_grid(d->cat, d->win->current_search_);
            }),
            data,
            GClosureNotify([](gpointer p, GClosure*) { delete static_cast<CatBtnData*>(p); }),
            static_cast<GConnectFlags>(0));

        gtk_box_pack_start(GTK_BOX(sidebar), btn, FALSE, FALSE, 0);
    }

    // Select All / Deselect All
    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(sidebar), sep, FALSE, FALSE, 8);

    GtkWidget* lbl2 = gtk_label_new("LỰA CHỌN");
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl2), "section-label");
    gtk_widget_set_halign(lbl2, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(sidebar), lbl2, FALSE, FALSE, 0);

    GtkWidget* sel_all = gtk_button_new_with_label("☑ Chọn tất cả");
    GtkWidget* desel   = gtk_button_new_with_label("☐ Bỏ chọn");
    gtk_style_context_add_class(gtk_widget_get_style_context(sel_all), "category-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(desel),   "category-btn");
    g_signal_connect(sel_all, "clicked", G_CALLBACK(on_select_all),   this);
    g_signal_connect(desel,   "clicked", G_CALLBACK(on_deselect_all), this);
    gtk_box_pack_start(GTK_BOX(sidebar), sel_all, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), desel,   FALSE, FALSE, 0);

    // Install button + progress ở cuối sidebar
    GtkWidget* bottom_spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), bottom_spacer, TRUE, TRUE, 0);

    GtkWidget* lbl_sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(sidebar), lbl_sep, FALSE, FALSE, 4);

    progress_bar_ = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progress_bar_), TRUE);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar_), "Sẵn sàng");
    gtk_widget_set_margin_start(progress_bar_, 8);
    gtk_widget_set_margin_end(progress_bar_, 8);
    gtk_box_pack_start(GTK_BOX(sidebar), progress_bar_, FALSE, FALSE, 4);

    install_btn_ = gtk_button_new_with_label("⬇ Cài đặt");
    gtk_style_context_add_class(gtk_widget_get_style_context(install_btn_), "install-btn");
    gtk_widget_set_margin_start(install_btn_, 8);
    gtk_widget_set_margin_end(install_btn_, 8);
    gtk_widget_set_margin_bottom(install_btn_, 8);
    g_signal_connect(install_btn_, "clicked", G_CALLBACK(on_install_clicked), this);
    gtk_box_pack_start(GTK_BOX(sidebar), install_btn_, FALSE, FALSE, 4);

    status_label_ = gtk_label_new("Chọn phần mềm để cài đặt");
    gtk_style_context_add_class(gtk_widget_get_style_context(status_label_), "app-version");
    gtk_widget_set_margin_bottom(status_label_, 8);
    gtk_label_set_line_wrap(GTK_LABEL(status_label_), TRUE);
    gtk_widget_set_halign(status_label_, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(sidebar), status_label_, FALSE, FALSE, 0);

    g_object_set_data(G_OBJECT(window_), "sidebar", sidebar);
}

void MainWindow::build_app_grid() {
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    app_grid_ = gtk_flow_box_new();
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(app_grid_), 3);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(app_grid_), 1);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(app_grid_), 4);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(app_grid_), 4);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(app_grid_), GTK_SELECTION_NONE);
    gtk_widget_set_margin_start(app_grid_, 8);
    gtk_widget_set_margin_end(app_grid_, 8);
    gtk_widget_set_margin_top(app_grid_, 8);

    gtk_container_add(GTK_CONTAINER(scroll), app_grid_);
    populate_app_grid();

    g_object_set_data(G_OBJECT(window_), "grid_scroll", scroll);
}

void MainWindow::build_log_panel() {
    GtkWidget* panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(panel, 8);
    gtk_widget_set_margin_end(panel, 8);
    gtk_widget_set_margin_top(panel, 4);

    GtkWidget* lbl = gtk_label_new("📋 Nhật ký cài đặt");
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "panel-label");
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(panel), lbl, FALSE, FALSE, 0);

    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, -1, 160);

    log_buffer_ = gtk_text_buffer_new(nullptr);
    // Tag cho lỗi
    gtk_text_buffer_create_tag(log_buffer_, "error", "foreground", "#f7768e", nullptr);

    log_view_ = gtk_text_view_new_with_buffer(log_buffer_);
    gtk_style_context_add_class(gtk_widget_get_style_context(log_view_), "log-view");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(log_view_), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(log_view_), FALSE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(log_view_), 8);
    gtk_container_add(GTK_CONTAINER(scroll), log_view_);
    gtk_box_pack_start(GTK_BOX(panel), scroll, TRUE, TRUE, 0);

    // Initial message
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(log_buffer_, &end);
    gtk_text_buffer_insert(log_buffer_, &end,
        "Ubuntu Store khởi động. Scripts directory: " SCRIPTS_DIR "\n", -1);

    g_object_set_data(G_OBJECT(window_), "log_panel", panel);
}

// ─── App Card ────────────────────────────────────────────────
GtkWidget* MainWindow::create_app_card(const AppInfo& app) {
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "app-card");
    gtk_widget_set_size_request(card, 280, -1);

    // Row 1: icon + tên + badge
    GtkWidget* row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* icon = gtk_image_new_from_icon_name(app.icon_name.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(row1), icon, FALSE, FALSE, 0);

    GtkWidget* vname = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget* name_lbl = gtk_label_new(app.name.c_str());
    gtk_style_context_add_class(gtk_widget_get_style_context(name_lbl), "app-name");
    gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
    GtkWidget* ver_lbl = gtk_label_new(("v" + app.version).c_str());
    gtk_style_context_add_class(gtk_widget_get_style_context(ver_lbl), "app-version");
    gtk_widget_set_halign(ver_lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vname), name_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vname), ver_lbl,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row1), vname, TRUE, TRUE, 0);

    if (app.installed) {
        GtkWidget* badge = gtk_label_new("✔ Đã cài");
        gtk_style_context_add_class(gtk_widget_get_style_context(badge), "badge-installed");
        gtk_widget_set_valign(badge, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(row1), badge, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(card), row1, FALSE, FALSE, 0);

    // Description
    GtkWidget* desc_lbl = gtk_label_new(app.description.c_str());
    gtk_style_context_add_class(gtk_widget_get_style_context(desc_lbl), "app-desc");
    gtk_widget_set_halign(desc_lbl, GTK_ALIGN_START);
    gtk_label_set_line_wrap(GTK_LABEL(desc_lbl), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(desc_lbl), 35);
    gtk_box_pack_start(GTK_BOX(card), desc_lbl, FALSE, FALSE, 0);

    // Progress bar (ẩn ban đầu)
    GtkWidget* prog = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(prog), TRUE);
    gtk_widget_set_no_show_all(prog, TRUE);
    gtk_box_pack_start(GTK_BOX(card), prog, FALSE, FALSE, 0);
    card_progress_[app.id] = prog;

    // Checkbox chọn
    GtkWidget* check = gtk_check_button_new_with_label("Chọn để cài đặt");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), app.selected);
    auto* cd = new CardData{this, app.id};
    g_signal_connect_data(check, "toggled",
        G_CALLBACK(on_card_toggled),
        cd,
        [](gpointer p, GClosure*) { delete static_cast<CardData*>(p); },
        static_cast<GConnectFlags>(0));
    gtk_box_pack_start(GTK_BOX(card), check, FALSE, FALSE, 0);
    card_checks_[app.id] = GTK_CHECK_BUTTON(check);

    return card;
}

void MainWindow::populate_app_grid(const std::string& cat, const std::string& search) {
    // Xóa children cũ
    GList* children = gtk_container_get_children(GTK_CONTAINER(app_grid_));
    for (GList* l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);
    card_progress_.clear();
    card_checks_.clear();

    std::string low_search = search;
    std::transform(low_search.begin(), low_search.end(), low_search.begin(), ::tolower);

    for (const auto& app : apps_) {
        // Filter category
        if (cat != "all" && app.category != cat) continue;
        // Filter search
        if (!low_search.empty()) {
            std::string low_name = app.name;
            std::transform(low_name.begin(), low_name.end(), low_name.begin(), ::tolower);
            if (low_name.find(low_search) == std::string::npos) continue;
        }

        GtkWidget* card = create_app_card(app);
        gtk_flow_box_insert(GTK_FLOW_BOX(app_grid_), card, -1);
    }

    gtk_widget_show_all(app_grid_);

    // Update status label
    int selected_count = 0;
    for (const auto& a : apps_) if (a.selected) selected_count++;
    if (selected_count > 0) {
        gtk_label_set_text(GTK_LABEL(status_label_),
            (std::to_string(selected_count) + " phần mềm được chọn").c_str());
    } else {
        gtk_label_set_text(GTK_LABEL(status_label_), "Chọn phần mềm để cài đặt");
    }
}

// ─── Callbacks ───────────────────────────────────────────────
void MainWindow::on_install_clicked(GtkButton*, gpointer data) {
    auto* win = static_cast<MainWindow*>(data);

    std::vector<AppInfo> selected;
    for (const auto& a : win->apps_)
        if (a.selected) selected.push_back(a);

    if (selected.empty()) {
        GtkWidget* dlg = gtk_message_dialog_new(GTK_WINDOW(win->window_),
            GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
            "Vui lòng chọn ít nhất một phần mềm để cài đặt.");
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        return;
    }

    gtk_widget_set_sensitive(win->install_btn_, FALSE);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(win->progress_bar_), 0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(win->progress_bar_), "Đang cài đặt...");

    delete win->installer_;
    win->installer_ = new Installer(
        [win](const std::string& msg, bool err) {
            g_idle_add(idle_log, new LogData{win, msg, err});
        },
        [win](const std::string& id, double frac, const std::string& st) {
            g_idle_add(idle_prog, new ProgData{win, id, frac, st});
            // Update overall progress bar
            int total = 0, done_cnt = 0;
            for (const auto& a : win->apps_) if (a.selected) { total++; if (frac >= 1.0 || frac < 0) done_cnt++; }
            if (total > 0) {
                double overall = (double)done_cnt / total;
                struct BarUpdate { MainWindow* w; double v; };
                g_idle_add([](gpointer p) -> gboolean {
                    auto* u = static_cast<BarUpdate*>(p);
                    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(u->w->progress_bar_), u->v);
                    delete u; return G_SOURCE_REMOVE;
                }, new BarUpdate{win, overall});
            }
        },
        [win](const std::vector<InstallResult>& res) {
            g_idle_add(idle_done, new DoneData{win, res});
        }
    );

    win->installer_->start(win->apps_);
}

void MainWindow::on_select_all(GtkButton*, gpointer data) {
    auto* win = static_cast<MainWindow*>(data);
    for (auto& a : win->apps_) a.selected = true;
    win->populate_app_grid(win->current_category_, win->current_search_);
}

void MainWindow::on_deselect_all(GtkButton*, gpointer data) {
    auto* win = static_cast<MainWindow*>(data);
    for (auto& a : win->apps_) a.selected = false;
    win->populate_app_grid(win->current_category_, win->current_search_);
}

void MainWindow::on_search_changed(GtkSearchEntry* entry, gpointer data) {
    auto* win = static_cast<MainWindow*>(data);
    win->current_search_ = gtk_entry_get_text(GTK_ENTRY(entry));
    win->populate_app_grid(win->current_category_, win->current_search_);
}

void MainWindow::on_card_toggled(GtkCheckButton* btn, gpointer data) {
    auto* cd = static_cast<CardData*>(data);
    bool active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
    for (auto& a : cd->win->apps_) {
        if (a.id == cd->app_id) { a.selected = active; break; }
    }
    // Update status
    int cnt = 0;
    for (const auto& a : cd->win->apps_) if (a.selected) cnt++;
    if (cnt > 0)
        gtk_label_set_text(GTK_LABEL(cd->win->status_label_),
            (std::to_string(cnt) + " phần mềm được chọn").c_str());
    else
        gtk_label_set_text(GTK_LABEL(cd->win->status_label_), "Chọn phần mềm để cài đặt");
}

void MainWindow::on_theme_light(GtkButton*, gpointer data) {
    auto* win = static_cast<MainWindow*>(data);
    win->theme_mgr_.apply(win->window_, Theme::LIGHT);
}
void MainWindow::on_theme_dark(GtkButton*, gpointer data) {
    auto* win = static_cast<MainWindow*>(data);
    win->theme_mgr_.apply(win->window_, Theme::DARK);
}

// ── Custom Theme Dialog ──────────────────────────────────────
void MainWindow::on_theme_custom(GtkButton*, gpointer data) {
    auto* win = static_cast<MainWindow*>(data);
    win->build_theme_dialog();
}

void MainWindow::build_theme_dialog() {
    GtkWidget* dlg = gtk_dialog_new_with_buttons(
        "🎨 Tuỳ chỉnh giao diện",
        GTK_WINDOW(window_),
        GTK_DIALOG_MODAL,
        "Áp dụng", GTK_RESPONSE_APPLY,
        "Huỷ",     GTK_RESPONSE_CANCEL,
        nullptr);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 420, -1);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_container_set_border_width(GTK_CONTAINER(content), 16);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_add(GTK_CONTAINER(content), grid);

    const CustomThemeColors& cur = theme_mgr_.get_custom_colors();

    auto make_row = [&](const char* label, const char* val, int row) -> GtkEntry* {
        GtkWidget* lbl = gtk_label_new(label);
        gtk_widget_set_halign(lbl, GTK_ALIGN_END);
        GtkWidget* entry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry), val);
        gtk_widget_set_hexpand(entry, TRUE);
        gtk_grid_attach(GTK_GRID(grid), lbl,   0, row, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), entry, 1, row, 1, 1);
        return GTK_ENTRY(entry);
    };

    auto* d = new ThemeDialogData();
    d->win            = this;
    d->e_bg_primary   = make_row("Nền chính:",        cur.bg_primary.c_str(),    0);
    d->e_bg_secondary = make_row("Nền phụ:",          cur.bg_secondary.c_str(),  1);
    d->e_bg_card      = make_row("Nền thẻ:",          cur.bg_card.c_str(),       2);
    d->e_accent       = make_row("Màu nhấn:",         cur.accent.c_str(),        3);
    d->e_text_primary = make_row("Chữ chính:",        cur.text_primary.c_str(),  4);
    d->e_text_secondary=make_row("Chữ phụ:",          cur.text_secondary.c_str(),5);
    d->e_border       = make_row("Viền:",             cur.border.c_str(),        6);
    d->e_success      = make_row("Màu thành công:",   cur.success.c_str(),       7);
    d->e_button_bg    = make_row("Nút nền:",          cur.button_bg.c_str(),     8);
    d->e_button_text  = make_row("Nút chữ:",          cur.button_text.c_str(),   9);

    gtk_widget_show_all(dlg);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_APPLY) {
        CustomThemeColors c;
        c.bg_primary    = gtk_entry_get_text(d->e_bg_primary);
        c.bg_secondary  = gtk_entry_get_text(d->e_bg_secondary);
        c.bg_card       = gtk_entry_get_text(d->e_bg_card);
        c.accent        = gtk_entry_get_text(d->e_accent);
        c.text_primary  = gtk_entry_get_text(d->e_text_primary);
        c.text_secondary= gtk_entry_get_text(d->e_text_secondary);
        c.border        = gtk_entry_get_text(d->e_border);
        c.success       = gtk_entry_get_text(d->e_success);
        c.button_bg     = gtk_entry_get_text(d->e_button_bg);
        c.button_text   = gtk_entry_get_text(d->e_button_text);
        theme_mgr_.set_custom_colors(c);
        theme_mgr_.apply(window_, Theme::CUSTOM, &c);
    }

    delete d;
    gtk_widget_destroy(dlg);
}
