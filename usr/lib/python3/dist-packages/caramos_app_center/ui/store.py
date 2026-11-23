"""The App Center store: categories in the sidebar, the apps as boxed lists, search, and each app's page.

It folds into one column on narrow windows: the categories become the first page.
"""

from __future__ import annotations

import unicodedata
from collections.abc import Callable

from gi.repository import Adw, Gio, Gtk

from ..i18n import _

from ..catalog import App, Catalog, localized, more_apps_url
from .common import (MORE_APPS_SITE, boxed_list, clamp, label, link_label, open_more_apps, page_box, page_title,
                     scrolled, secondary, section, themed_image)
from .detail import detail_content
from .rows import StoreRow, installed_controls

ALL = "all"


def _fold(text: str) -> str:
    """Lower case without Vietnamese accents, so a search typed without them still finds the app."""

    # chr(0x111) is the Vietnamese d with a stroke, which has no decomposed form.
    decomposed = unicodedata.normalize("NFD", text.casefold().replace(chr(0x111), "d"))
    return "".join(char for char in decomposed if not unicodedata.combining(char))


class StoreView(Adw.BreakpointBin):
    def __init__(self, catalog: Catalog, installed: set[str], *, on_action: Callable[[App, str], None]) -> None:
        super().__init__(width_request=360, height_request=480)
        self.catalog = catalog
        self.installed = installed
        self.on_action = on_action
        self.category = ALL
        self.query = ""

        self.split = Adw.NavigationSplitView(min_sidebar_width=250, max_sidebar_width=280, show_content=True)
        self.set_child(self.split)
        self.split.set_sidebar(self._sidebar())

        self.holder = Adw.Bin()
        self.scroller = scrolled(clamp(self.holder))
        self.search = Gtk.SearchEntry(placeholder_text=_("Search apps"), hexpand=True)
        self.search.connect("search-changed", self._on_search)
        header = Adw.HeaderBar()
        header.set_title_widget(Adw.Clamp(maximum_size=320, child=self.search))
        view = Adw.ToolbarView()
        view.add_top_bar(header)
        view.set_content(self.scroller)

        self.nav = Adw.NavigationView()
        self.nav.add(Adw.NavigationPage(title=_("Apps"), child=view, tag="list"))
        self.split.set_content(Adw.NavigationPage(title=_("Apps"), child=self.nav))

        narrow = Adw.Breakpoint.new(Adw.BreakpointCondition.parse("max-width: 640sp"))
        narrow.add_setter(self.split, "collapsed", True)
        self.add_breakpoint(narrow)
        self.refresh()

    def _sidebar(self) -> Adw.NavigationPage:
        listbox = Gtk.ListBox(selection_mode=Gtk.SelectionMode.SINGLE)
        listbox.add_css_class("navigation-sidebar")
        self.categories = listbox
        entries = [(ALL, _("All apps"))]
        entries += [(c.id, localized(c.title)) for c in self.catalog.categories if self.catalog.apps_in(c.id)]
        for category_id, title in entries:
            row = Gtk.ListBoxRow()
            row.category = category_id
            row.set_child(label(title, lines=1))
            listbox.append(row)
        listbox.select_row(listbox.get_row_at_index(0))
        listbox.connect("row-selected", self._on_category)
        # Selecting the category that is already selected shows its apps on a narrow window too.
        listbox.connect("row-activated", lambda *_args: self.split.set_show_content(True))

        menu = Gio.Menu()
        menu.append(_("Run the app picker again"), "win.run-wizard")
        menu_button = Gtk.MenuButton(icon_name="open-menu-symbolic", menu_model=menu, tooltip_text=_("Menu"))
        header = Adw.HeaderBar()
        header.set_title_widget(Adw.WindowTitle(title=_("App Center")))
        header.pack_end(menu_button)
        view = Adw.ToolbarView()
        view.add_top_bar(header)
        view.set_content(scrolled(listbox))
        view.add_bottom_bar(self._more_apps())
        return Adw.NavigationPage(title=_("App Center"), child=view)

    def _more_apps(self) -> Gtk.Widget:
        button = Gtk.Button(hexpand=True)
        button.add_css_class("flat")
        button.add_css_class("more-apps")
        content = Gtk.Box(spacing=12)
        texts = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2, hexpand=True)
        texts.append(label(_("Find more apps"), "row-title", lines=1))
        texts.append(secondary(MORE_APPS_SITE, lines=1))
        content.append(texts)
        icon = themed_image("adw-external-link-symbolic", "external-link-symbolic")
        icon.set_valign(Gtk.Align.CENTER)
        content.append(icon)
        button.set_child(content)
        button.set_tooltip_text(_("Open %s in the web browser") % MORE_APPS_SITE)
        button.connect("clicked", lambda widget: open_more_apps(widget))
        bar = Gtk.Box()
        bar.add_css_class("sidebar-footer")
        bar.append(button)
        return bar

    # Lists -----------------------------------------------------------------------------------

    def visible_apps(self) -> list[App]:
        """The apps of the selected category, or the apps that match the search in every category."""

        if self.query:
            needle = _fold(self.query)
            return [app for app in self.catalog.apps.values()
                    if needle in _fold(app.name) or needle in _fold(app.summary_text())]
        if self.category == ALL:
            return list(self.catalog.apps.values())
        return self.catalog.apps_in(self.category)

    def _list(self, apps: list[App]) -> Gtk.ListBox:
        listbox = boxed_list()
        for app in apps:
            listbox.append(StoreRow(app, app.id in self.installed, on_install=self._install,
                                    on_remove=self._remove))
        listbox.connect("row-activated", lambda _listbox, row: self.open_detail(row.app))
        return listbox

    def refresh(self) -> None:
        column = page_box()
        apps = self.visible_apps()
        if self.query:
            column.append(page_title(_("Results for “%s”") % self.query))
            if apps:
                column.append(self._list(apps))
            else:
                column.append(link_label(_("No suggested app matches “%s”.") % self.query,
                                         _("Search %s") % MORE_APPS_SITE, more_apps_url(self.query)))
        elif self.category == ALL:
            column.append(page_title(_("All apps")))
            for category in self.catalog.categories:
                in_category = self.catalog.apps_in(category.id)
                if in_category:
                    column.append(section(localized(category.title), self._list(in_category)))
        else:
            title = next(localized(c.title) for c in self.catalog.categories if c.id == self.category)
            column.append(page_title(title))
            column.append(self._list(apps))
        self.holder.set_child(column)
        self.scroller.get_vadjustment().set_value(0)

    def set_installed(self, installed: set[str]) -> None:
        self.installed = installed
        self.refresh()
        if self.nav.get_visible_page().get_tag() != "list":
            self.nav.pop_to_tag("list")

    def _on_category(self, _listbox, row) -> None:
        if row is None:
            return
        self.category = row.category
        if self.query:
            self.query = ""
            self.search.set_text("")
        self.nav.pop_to_tag("list")
        self.refresh()
        self.split.set_show_content(True)

    def _on_search(self, entry) -> None:
        self.query = entry.get_text().strip()
        # Search covers every category: no category is selected meanwhile, and the last one comes back after.
        if self.query:
            self.categories.unselect_all()
        else:
            row = self.categories.get_first_child()
            while row is not None and getattr(row, "category", None) != self.category:
                row = row.get_next_sibling()
            if row is not None:
                self.categories.select_row(row)
        self.nav.pop_to_tag("list")
        self.refresh()

    def _install(self, app: App) -> None:
        self.on_action(app, "install")

    def _remove(self, app: App) -> None:
        self.on_action(app, "remove")

    # App page --------------------------------------------------------------------------------

    def open_detail(self, app: App) -> None:
        if app.preinstalled:
            action = None
        elif app.id in self.installed:
            action = installed_controls(app, self._remove)
        else:
            action = Gtk.Button(label=_("Install"))
            action.add_css_class("suggested-action")
            action.connect("clicked", lambda _button: self._install(app))
        view = Adw.ToolbarView()
        view.add_top_bar(Adw.HeaderBar(show_title=False))
        view.set_content(scrolled(clamp(detail_content(app, action))))
        self.nav.push(Adw.NavigationPage(title=app.name, child=view, tag=f"app-{app.id}"))
