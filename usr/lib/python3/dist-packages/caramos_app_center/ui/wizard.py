"""First-run app picker: one page with the recommended apps already ticked and the install bar at the bottom.

Each step of the catalog is a section with a boxed list: radio buttons when the step takes one app, check boxes
when it takes any. Installed apps are not choices: their rows say so, and the ones the App Center installs have a
Remove button. A row's info button shows what the app is good at and what to know about it.
"""

from __future__ import annotations

from collections.abc import Callable

from gi.repository import Adw, Gtk

from ..i18n import _, ngettext

from .. import firstrun
from ..catalog import App, Catalog, Step, localized, more_apps_url
from ..plan import Plan, default_selection, format_size, make_plan
from .common import (MORE_APPS_SITE, boxed_list, clamp, label, link_label, logo, page_box, page_title, scrolled,
                     secondary, section)
from .detail import detail_dialog
from .rows import ChoiceRow, InstalledRow, NoneRow


class WizardView(Adw.Bin):
    def __init__(self, catalog: Catalog, installed: set[str], *, on_install: Callable[[list[str]], None],
                 on_remove: Callable[[App], None], on_finish: Callable[[], None],
                 on_later: Callable[[], None] | None, selection: dict[str, set[str]] | None = None) -> None:
        super().__init__()
        self.catalog = catalog
        self.installed = installed
        self.on_install = on_install
        self.on_remove = on_remove
        self.on_finish = on_finish
        # The choices made before an app was removed stay, without what is installed now.
        if selection is None:
            self.selection = default_selection(catalog, installed)
        else:
            self.selection = {step.id: {app_id for app_id in selection.get(step.id, set()) if app_id not in installed}
                              for step in catalog.steps}

        header = Adw.HeaderBar()
        header.set_title_widget(Adw.WindowTitle(title=_("Choose apps for this computer")))
        if on_later is not None:
            # Autostart only: a picker the user opened closes with the window button.
            later = Gtk.Button(label=_("Later"))
            later.set_tooltip_text(_("Close now and ask again at the next login"))
            later.connect("clicked", lambda _button: on_later())
            header.pack_end(later)

        # The bar first: building the sections ticks the choices, which updates it.
        bar = self._bar()
        content = page_box()
        content.append(self._intro())
        for step in catalog.steps:
            content.append(self._section(step))
        content.append(link_label(_("Can't find the app you need?"), _("Find more on %s") % MORE_APPS_SITE,
                                  more_apps_url()))

        view = Adw.ToolbarView(bottom_bar_style=Adw.ToolbarStyle.RAISED_BORDER)
        view.add_top_bar(header)
        view.set_content(scrolled(clamp(content)))
        view.add_bottom_bar(bar)
        self.set_child(view)
        self._sync_bar()

    # Page parts ------------------------------------------------------------------------------

    def _intro(self) -> Gtk.Widget:
        box = Gtk.Box(spacing=16)
        image = logo(48)
        image.set_valign(Gtk.Align.START)
        box.append(image)
        texts = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4, hexpand=True)
        texts.append(page_title(_("Pick the apps you need")))
        texts.append(label(_("CaramOS downloads and installs them in the background while you keep using the "
                             "computer. Add or remove apps later in the App Center."), "paragraph"))
        if firstrun.is_live_session():
            texts.append(secondary(_("You are trying CaramOS from the USB: apps installed now are lost when the "
                                     "computer turns off. Install CaramOS to keep them."), "paragraph"))
        box.append(texts)
        return box

    def _section(self, step: Step) -> Gtk.Widget:
        listbox = boxed_list()
        # The check boxes and the space where an installed app has none are one column.
        checks = Gtk.SizeGroup(mode=Gtk.SizeGroupMode.HORIZONTAL)
        single = step.choice == "single"
        group: Gtk.CheckButton | None = None
        chosen = self.selection[step.id]
        for app_id in step.apps:
            app = self.catalog.app(app_id)
            if app_id in self.installed or app.preinstalled:
                listbox.append(InstalledRow(app, None if app.preinstalled else self.on_remove, check_sizes=checks))
                continue
            row = ChoiceRow(app, single=single, group=group if single else None, active=app_id in chosen,
                            on_info=self._show_info, check_sizes=checks)
            if single and group is None:
                group = row.check
            row.check.connect("toggled", self._on_toggled, step, app_id)
            listbox.append(row)
        if single and step.allow_none:
            listbox.append(NoneRow(_("Not now"), group=group, active=not chosen, check_sizes=checks))
        listbox.connect("row-activated", self._on_row_activated)
        hint = _("Pick one") if single else _("Pick as many as you like")
        # "See more" opens the step's category on the VNLF app site; the sentence stays secondary, the link a link.
        uri = more_apps_url(category=step.web_category)
        description = link_label(localized(step.subtitle), _("See more"), uri, "secondary-text", "secondary",
                                 tooltip=_("Open %s in the web browser") % uri.split("://", 1)[-1])
        return section(localized(step.title), listbox, hint=hint, description=description)

    def _bar(self) -> Gtk.Widget:
        inner = Gtk.Box(spacing=18)
        inner.add_css_class("action-bar")
        totals = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2, hexpand=True, valign=Gtk.Align.CENTER)
        # One line each: the bar does not grow for wrapped text, so a narrow window ends them with "…".
        self.total_label = label("", "row-title", lines=1)
        self.detail_label = secondary("", lines=1)
        totals.append(self.total_label)
        totals.append(self.detail_label)
        inner.append(totals)
        self.install_button = Gtk.Button(valign=Gtk.Align.CENTER)
        self.install_button.add_css_class("suggested-action")
        self.install_button.connect("clicked", self._on_install_clicked)
        inner.append(self.install_button)
        # Same column as the page, so the text and the button line up with the lists.
        return clamp(inner)

    def _show_info(self, app: App) -> None:
        detail_dialog(app).present(self)

    # Selection and totals --------------------------------------------------------------------

    def selected_ids(self) -> list[str]:
        ids: list[str] = []
        for step in self.catalog.steps:
            ids.extend(app_id for app_id in step.apps if app_id in self.selection[step.id])
        return ids

    def _plan(self) -> Plan:
        return make_plan(self.catalog, self.selected_ids(), self.installed)

    def _sync_bar(self) -> None:
        plan = self._plan()
        count = len(plan.install)
        if count:
            self.total_label.set_label(ngettext("%(count)d app, about %(size)s", "%(count)d apps, about %(size)s",
                                                count) % {"count": count, "size": format_size(plan.total_mb)})
            self.install_button.set_label(ngettext("Install %d app", "Install %d apps", count) % count)
        else:
            self.total_label.set_label(_("No apps selected"))
            self.install_button.set_label(_("Done"))
        self.detail_label.set_visible(bool(plan.runtime_mb))
        if plan.runtime_mb:
            self.detail_label.set_label(_("Includes %s shared by the apps from Flathub, downloaded once.")
                                        % format_size(plan.runtime_mb))
            self.detail_label.set_tooltip_text(self.detail_label.get_label())

    def _on_install_clicked(self, _button) -> None:
        ids = self._plan().ids
        if ids:
            self.on_install(ids)
        else:
            self.on_finish()

    def _on_row_activated(self, _listbox, row) -> None:
        if isinstance(row, (ChoiceRow, NoneRow)):
            row.activate_choice()

    def _on_toggled(self, check: Gtk.CheckButton, step: Step, app_id: str) -> None:
        chosen = self.selection[step.id]
        if check.get_active():
            chosen.add(app_id)
        else:
            chosen.discard(app_id)
        self._sync_bar()
