"""First-run app picker: one page with the recommended apps already ticked and an always-visible install bar.

Installed apps are not choices: their cards say so, and the ones the App Center installs have a Remove button.
"""

from __future__ import annotations

from collections.abc import Callable

from gi.repository import Adw, Gtk

from ..i18n import _, ngettext

from .. import firstrun
from ..catalog import App, Catalog, Step, localized, more_apps_url
from ..plan import Plan, default_selection, format_size, make_plan
from .cards import AppCard, InstalledCard, NoneCard
from .common import clamp, label, logo, open_more_apps, scrolled


class WizardView(Gtk.Box):
    def __init__(self, catalog: Catalog, installed: set[str], *, on_install: Callable[[list[str]], None],
                 on_remove: Callable[[App], None], on_finish: Callable[[], None],
                 on_later: Callable[[], None] | None, selection: dict[str, set[str]] | None = None) -> None:
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
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
        header.set_title_widget(Adw.WindowTitle(title=_("Set up your apps"), subtitle=_("CaramOS App Center")))
        if on_later is not None:
            # Autostart only: a picker the user opened closes with the window button.
            later = Gtk.Button(label=_("Later"))
            later.set_tooltip_text(_("Close now and ask again at the next login"))
            later.connect("clicked", lambda _b: on_later())
            header.pack_end(later)
        self.append(header)

        content = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=36)
        content.set_margin_top(24)
        content.set_margin_bottom(32)
        content.set_margin_start(24)
        content.set_margin_end(24)
        content.append(self._hero())
        for number, step in enumerate(catalog.steps, 1):
            content.append(self._section(step, number))
        content.append(self._more_apps())
        page = scrolled(clamp(content))
        page.add_css_class("welcome")
        self.append(page)

        self.bar = Gtk.Box(spacing=18)
        self.bar.add_css_class("install-bar")
        totals = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2, hexpand=True, valign=Gtk.Align.CENTER)
        self.total_label = label("", "bar-total")
        self.detail_label = label("", "dim-label", "bar-detail")
        totals.append(self.total_label)
        totals.append(self.detail_label)
        self.bar.append(totals)
        self.install_button = Gtk.Button(valign=Gtk.Align.CENTER)
        for css_class in ("suggested-action", "pill"):
            self.install_button.add_css_class(css_class)
        self.install_button.connect("clicked", self._on_install_clicked)
        self.bar.append(self.install_button)
        self.append(self.bar)
        self._sync_bar()

    # Page parts ------------------------------------------------------------------------------

    def _hero(self) -> Gtk.Widget:
        hero = Gtk.Box(spacing=20)
        hero.add_css_class("hero-card")
        image = logo(72)
        image.set_valign(Gtk.Align.START)
        hero.append(image)
        texts = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6, hexpand=True)
        texts.append(label(_("Welcome to CaramOS"), "hero-title"))
        texts.append(label(_("Pick the apps you need. CaramOS downloads and installs them for you."),
                           "hero-subtitle"))
        points = [
            ("object-select-symbolic", _("The recommended apps are already selected. Press Install at the bottom, "
                                         "or change any choice below.")),
            ("emblem-synchronizing-symbolic", _("Apps install in the background while you use the computer. Add "
                                                "more any time from the App Center in the menu.")),
        ]
        if firstrun.is_live_session():
            points.append(("dialog-information-symbolic", _("You are trying CaramOS from the USB: apps installed now "
                                                            "are lost when the computer turns off. Install CaramOS "
                                                            "to keep them.")))
        for icon, text in points:
            row = Gtk.Box(spacing=8)
            row.add_css_class("hero-point")
            row.set_margin_top(4)
            image = Gtk.Image.new_from_icon_name(icon)
            image.set_valign(Gtk.Align.START)
            image.set_margin_top(2)
            row.append(image)
            row.append(label(text, "dim-label"))
            texts.append(row)
        hero.append(texts)
        return hero

    def _section(self, step: Step, number: int) -> Gtk.Widget:
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        heading = Gtk.Box(spacing=12)
        heading.append(label(str(number), "section-number", wrap=False, xalign=0.5))
        title = label(localized(step.title), "section-title")
        title.set_valign(Gtk.Align.CENTER)
        heading.append(title)
        hint = label(_("Pick one") if step.choice == "single" else _("Pick any"), "section-hint", wrap=False)
        hint.set_valign(Gtk.Align.CENTER)
        hint.set_hexpand(True)
        hint.set_halign(Gtk.Align.START)
        heading.append(hint)
        heading.append(_see_more_button(step.web_category))
        box.append(heading)
        subtitle = label(localized(step.subtitle), "dim-label")
        subtitle.set_margin_bottom(12)
        box.append(subtitle)

        flow = Gtk.FlowBox(selection_mode=Gtk.SelectionMode.NONE, homogeneous=True, max_children_per_line=3,
                           min_children_per_line=1, column_spacing=16, row_spacing=16)
        group: Gtk.ToggleButton | None = None
        chosen = self.selection[step.id]
        for app_id in step.apps:
            app = self.catalog.app(app_id)
            if app_id in self.installed or app.preinstalled:
                flow.append(InstalledCard(app, None if app.preinstalled else self.on_remove))
                continue
            card = AppCard(app)
            if step.choice == "single":
                if group is None:
                    group = card
                else:
                    card.set_group(group)
            card.set_active(app_id in chosen)
            card.connect("toggled", self._on_card_toggled, step)
            flow.append(card)
        box.append(flow)

        if step.choice == "single" and step.allow_none:
            none_card = NoneCard(_("Not now, I will choose later"))
            if group is not None:
                none_card.set_group(group)
            none_card.set_active(not chosen)
            none_card.set_halign(Gtk.Align.START)
            none_card.set_margin_top(4)
            box.append(none_card)
        return box

    def _more_apps(self) -> Gtk.Widget:
        box = Gtk.Box(spacing=4, halign=Gtk.Align.CENTER)
        box.add_css_class("more-apps")
        text = label(_("Can't find the app you want?"), "dim-label", wrap=False)
        text.set_valign(Gtk.Align.CENTER)
        box.append(text)
        box.append(_see_more_button(""))
        return box

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
            self.total_label.set_label(ngettext("%d app", "%d apps", count) % count + " · " +
                                       _("About %s on disk") % format_size(plan.total_mb))
            self.install_button.set_label(ngettext("Install %d app", "Install %d apps", count) % count)
        else:
            self.total_label.set_label(_("No apps selected"))
            self.install_button.set_label(_("Finish"))
        if plan.runtime_mb:
            self.detail_label.set_label(_("includes %s of shared Flathub libraries, downloaded once")
                                        % format_size(plan.runtime_mb))
        else:
            self.detail_label.set_label(_("You can keep using the computer while the apps install."))

    def _on_install_clicked(self, _button) -> None:
        ids = self._plan().ids
        if ids:
            self.on_install(ids)
        else:
            self.on_finish()

    def _on_card_toggled(self, card, step: Step) -> None:
        if card.app is None:
            return
        chosen = self.selection[step.id]
        if card.get_active():
            chosen.add(card.app.id)
        else:
            chosen.discard(card.app.id)
        self._sync_bar()


def _see_more_button(category: str) -> Gtk.Button:
    button = Gtk.Button(valign=Gtk.Align.CENTER)
    button.add_css_class("flat")
    button.add_css_class("see-more")
    content = Gtk.Box(spacing=6)
    content.append(Gtk.Label(label=_("See more")))
    content.append(Gtk.Image.new_from_icon_name("go-next-symbolic"))
    button.set_child(content)
    button.set_tooltip_text(_("Open %s in the web browser") % more_apps_url(category=category))
    button.connect("clicked", lambda b: open_more_apps(b, category=category))
    return button
