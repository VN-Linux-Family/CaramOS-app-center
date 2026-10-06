"""The App Center store: categories, search, app grid and a detail page with install and remove."""

from __future__ import annotations

from collections.abc import Callable

from gi.repository import Adw, Gio, Gtk

from ..i18n import _

from ..catalog import App, Catalog, localized
from .cards import StoreTile
from .common import app_icon, badge, clamp, label, open_more_apps, pros_cons_box, scrolled, size_text, source_chip

ALL = "all"


class StoreView(Adw.Bin):
    def __init__(self, catalog: Catalog, installed: set[str], *, on_action: Callable[[App, str], None]) -> None:
        super().__init__()
        self.split = Adw.NavigationSplitView(min_sidebar_width=210, max_sidebar_width=260)
        self.set_child(self.split)
        self.catalog = catalog
        self.installed = installed
        self.on_action = on_action
        self.category = ALL
        self.query = ""

        self.split.set_sidebar(self._sidebar())

        self.flow = Gtk.FlowBox(selection_mode=Gtk.SelectionMode.NONE, homogeneous=True, max_children_per_line=3,
                                min_children_per_line=1, column_spacing=14, row_spacing=14, valign=Gtk.Align.START)
        grid_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14)
        grid_box.set_margin_top(20)
        grid_box.set_margin_bottom(24)
        grid_box.set_margin_start(20)
        grid_box.set_margin_end(20)
        self.grid_title = label("", "step-title")
        grid_box.append(self.grid_title)
        grid_box.append(self.flow)
        self.empty = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12, halign=Gtk.Align.START)
        self.empty.append(label(_("No suggested app matches your search."), "dim-label"))
        self.search_web = Gtk.Button(halign=Gtk.Align.START)
        self.search_web.add_css_class("pill")
        self.search_web.connect("clicked", lambda button: open_more_apps(button, self.query))
        self.empty.append(self.search_web)
        grid_box.append(self.empty)

        search = Gtk.SearchEntry(placeholder_text=_("Search apps"), width_request=280)
        search.connect("search-changed", self._on_search)
        grid_header = Adw.HeaderBar()
        grid_header.set_title_widget(search)
        grid_view = Adw.ToolbarView()
        grid_view.add_top_bar(grid_header)
        grid_view.set_content(scrolled(clamp(grid_box, 1100)))

        self.nav = Adw.NavigationView()
        self.nav.add(Adw.NavigationPage(title=_("Apps"), child=grid_view, tag="grid"))
        self.split.set_content(Adw.NavigationPage(title=_("Apps"), child=self.nav))
        self.refresh()

    def _sidebar(self) -> Adw.NavigationPage:
        listbox = Gtk.ListBox(selection_mode=Gtk.SelectionMode.SINGLE)
        listbox.add_css_class("navigation-sidebar")
        entries = [(ALL, _("All apps"), "view-grid-symbolic")]
        entries += [(c.id, localized(c.title), c.icon) for c in self.catalog.categories]
        for category_id, title, icon in entries:
            row = Gtk.ListBoxRow()
            row.category = category_id
            box = Gtk.Box(spacing=12)
            box.set_margin_top(6)
            box.set_margin_bottom(6)
            box.append(Gtk.Image.new_from_icon_name(icon))
            box.append(label(title, wrap=False))
            row.set_child(box)
            listbox.append(row)
        listbox.select_row(listbox.get_row_at_index(0))
        listbox.connect("row-selected", self._on_category)

        menu = Gio.Menu()
        menu.append(_("Run the app picker again"), "win.run-wizard")
        menu_button = Gtk.MenuButton(icon_name="open-menu-symbolic", menu_model=menu, tooltip_text=_("Menu"))
        header = Adw.HeaderBar()
        header.set_title_widget(Adw.WindowTitle(title=_("App Center")))
        header.pack_end(menu_button)
        view = Adw.ToolbarView()
        view.add_top_bar(header)
        view.set_content(scrolled(listbox))
        view.add_bottom_bar(self._more_apps_bar())
        return Adw.NavigationPage(title=_("App Center"), child=view)

    def _more_apps_bar(self) -> Gtk.Widget:
        button = Gtk.Button()
        button.add_css_class("flat")
        button.set_tooltip_text("apps.vietnamlinuxfamily.net")
        content = Gtk.Box(spacing=10)
        content.append(Gtk.Image.new_from_icon_name("web-browser-symbolic"))
        text = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        text.append(label(_("Find more apps"), "heading", wrap=False))
        text.append(label("apps.vietnamlinuxfamily.net", "caption", "dim-label", wrap=False))
        content.append(text)
        button.set_child(content)
        button.connect("clicked", lambda b: open_more_apps(b))
        bar = Gtk.Box()
        bar.set_margin_start(6)
        bar.set_margin_end(6)
        bar.set_margin_top(6)
        bar.set_margin_bottom(6)
        button.set_hexpand(True)
        bar.append(button)
        return bar

    # Grid ------------------------------------------------------------------------------------

    def visible_apps(self) -> list[App]:
        apps = list(self.catalog.apps.values()) if self.category == ALL else self.catalog.apps_in(self.category)
        if self.query:
            needle = self.query.casefold()
            apps = [a for a in apps if needle in a.name.casefold() or needle in a.summary_text().casefold()]
        return apps

    def refresh(self) -> None:
        while (child := self.flow.get_first_child()) is not None:
            self.flow.remove(child)
        apps = self.visible_apps()
        for app in apps:
            self.flow.append(StoreTile(app, app.id in self.installed or app.preinstalled, self.open_detail))
        title = _("All apps") if self.category == ALL else next(
            localized(c.title) for c in self.catalog.categories if c.id == self.category)
        self.grid_title.set_label(title)
        self.empty.set_visible(not apps)
        self.search_web.set_label(_("Search “%s” on apps.vietnamlinuxfamily.net") % self.query)

    def set_installed(self, installed: set[str]) -> None:
        self.installed = installed
        self.refresh()
        if self.nav.get_visible_page().get_tag() != "grid":
            self.nav.pop_to_tag("grid")

    def _on_category(self, _listbox, row) -> None:
        if row is None:
            return
        self.category = row.category
        self.nav.pop_to_tag("grid")
        self.refresh()
        self.split.set_show_content(True)

    def _on_search(self, entry) -> None:
        self.query = entry.get_text().strip()
        self.refresh()

    # Detail ----------------------------------------------------------------------------------

    def open_detail(self, app: App) -> None:
        installed = app.id in self.installed or app.preinstalled
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14)
        box.set_margin_top(24)
        box.set_margin_bottom(28)
        box.set_margin_start(24)
        box.set_margin_end(24)

        hero = Gtk.Box(spacing=20)
        hero.add_css_class("detail-hero")
        hero.append(app_icon(app, 96))
        titles = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6, valign=Gtk.Align.CENTER, hexpand=True)
        titles.append(label(app.name, "detail-name", wrap=False))
        titles.append(label(app.summary_text(), "dim-label"))
        chips = Gtk.Box(spacing=8)
        chips.append(source_chip(app))
        chips.append(label(size_text(app), "chip", wrap=False))
        if installed:
            chips.append(badge(_("Installed"), "installed"))
        titles.append(chips)
        hero.append(titles)

        if app.preinstalled:
            action = Gtk.Button(label=_("Comes with CaramOS"), sensitive=False)
        elif installed:
            action = Gtk.Button(label=_("Remove"))
            action.add_css_class("destructive-action")
            action.connect("clicked", lambda _b: self.on_action(app, "remove"))
        else:
            action = Gtk.Button(label=_("Install"))
            action.add_css_class("suggested-action")
            action.connect("clicked", lambda _b: self.on_action(app, "install"))
        action.add_css_class("pill")
        action.set_valign(Gtk.Align.CENTER)
        hero.append(action)
        box.append(hero)

        box.append(label(_("Strengths and weaknesses"), "section-heading"))
        box.append(pros_cons_box(app, spacing=8))
        if app.website:
            link = Gtk.LinkButton(uri=app.website, label=_("Website"), halign=Gtk.Align.START)
            link.set_margin_top(8)
            box.append(link)

        view = Adw.ToolbarView()
        view.add_top_bar(Adw.HeaderBar())
        view.set_content(scrolled(clamp(box, 860)))
        self.nav.push(Adw.NavigationPage(title=app.name, child=view, tag=f"app-{app.id}"))
