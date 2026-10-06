"""First-run app picker: welcome, one page per catalog step, then a summary before installing."""

from __future__ import annotations

from collections.abc import Callable

from gi.repository import Adw, Gtk

from ..i18n import _, ngettext

from ..catalog import Catalog, Step, localized
from ..plan import default_selection, format_size, make_plan
from .cards import AppCard, NoneCard
from .common import app_icon, clamp, label, logo, scrolled, source_text


class WizardView(Gtk.Box):
    def __init__(self, catalog: Catalog, installed: set[str], *, on_install: Callable[[list[str]], None],
                 on_finish: Callable[[], None], on_later: Callable[[], None]) -> None:
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.catalog = catalog
        self.installed = installed
        self.on_install = on_install
        self.on_finish = on_finish
        self.selection = default_selection(catalog, installed)
        self.index = 0

        header = Adw.HeaderBar()
        header.set_title_widget(Adw.WindowTitle(title=_("Set up your apps"), subtitle=_("CaramOS App Center")))
        later = Gtk.Button(label=_("Later"))
        later.set_tooltip_text(_("Close now and ask again at the next login"))
        later.connect("clicked", lambda _b: on_later())
        header.pack_end(later)
        self.append(header)

        self.carousel = Adw.Carousel(vexpand=True, interactive=False, allow_scroll_wheel=False, allow_mouse_drag=False)
        self.carousel.connect("page-changed", self._on_page_changed)
        self.append(self.carousel)

        self.carousel.append(self._welcome_page())
        for number, step in enumerate(catalog.steps, 1):
            self.carousel.append(self._step_page(step, number))
        self.summary_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=18)
        self.carousel.append(self._summary_page())

        self.bar = Gtk.CenterBox()
        self.bar.add_css_class("wizard-bar")
        self.back_button = Gtk.Button(label=_("Back"))
        self.back_button.connect("clicked", lambda _b: self.go(self.index - 1))
        self.next_button = Gtk.Button(label=_("Continue"))
        self.next_button.add_css_class("suggested-action")
        self.next_button.add_css_class("pill")
        self.next_button.connect("clicked", self._on_next)
        self.bar.set_start_widget(self.back_button)
        self.bar.set_center_widget(Adw.CarouselIndicatorDots(carousel=self.carousel))
        self.bar.set_end_widget(self.next_button)
        self.append(self.bar)
        self._sync_bar()

    # Pages -----------------------------------------------------------------------------------

    def _welcome_page(self) -> Gtk.Widget:
        page = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=22, valign=Gtk.Align.CENTER,
                       halign=Gtk.Align.CENTER, hexpand=True, vexpand=True)
        page.set_margin_start(32)
        page.set_margin_end(32)
        page.append(logo(104))
        page.append(label(_("Welcome to CaramOS"), "hero-title", xalign=0.5))
        page.append(label(_("Pick the apps you need. CaramOS downloads and installs them for you."),
                          "hero-subtitle", "dim-label", xalign=0.5))
        points = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12, halign=Gtk.Align.CENTER)
        points.add_css_class("welcome-points")
        for icon, text in (
            ("view-grid-symbolic", _("Office suite, browsers, chat and tools in a few steps")),
            ("dialog-information-symbolic", _("Each choice lists its strengths and weaknesses")),
            ("emblem-synchronizing-symbolic", _("Installs in the background while you use the computer")),
        ):
            row = Gtk.Box(spacing=12)
            image = Gtk.Image.new_from_icon_name(icon)
            image.set_pixel_size(20)
            row.append(image)
            row.append(label(text, wrap=False))
            points.append(row)
        page.append(points)
        start = Gtk.Button(label=_("Choose apps"), halign=Gtk.Align.CENTER)
        for css_class in ("suggested-action", "pill", "large"):
            start.add_css_class(css_class)
        start.connect("clicked", lambda _b: self.go(1))
        page.append(start)
        wrapper = Gtk.Box(vexpand=True, hexpand=True)
        wrapper.add_css_class("welcome")
        wrapper.append(page)
        return wrapper

    def _step_page(self, step: Step, number: int) -> Gtk.Widget:
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        box.set_margin_top(28)
        box.set_margin_bottom(28)
        box.set_margin_start(24)
        box.set_margin_end(24)
        box.append(label(_("STEP %(number)d OF %(total)d") % {"number": number, "total": len(self.catalog.steps)},
                         "step-eyebrow"))
        box.append(label(localized(step.title), "step-title"))
        subtitle = label(localized(step.subtitle), "dim-label")
        subtitle.set_margin_bottom(14)
        box.append(subtitle)

        flow = Gtk.FlowBox(selection_mode=Gtk.SelectionMode.NONE, homogeneous=True, max_children_per_line=3,
                           min_children_per_line=1, column_spacing=18, row_spacing=18)
        group: Gtk.ToggleButton | None = None
        chosen = self.selection[step.id]
        for app_id in step.apps:
            app = self.catalog.app(app_id)
            card = AppCard(app, app_id in self.installed or app.preinstalled)
            if step.choice == "single" and not card.installed:
                if group is None:
                    group = card
                else:
                    card.set_group(group)
            card.set_active(app_id in chosen and not card.installed)
            card.connect("toggled", self._on_card_toggled, step)
            flow.append(card)
        box.append(flow)

        if step.choice == "single" and step.allow_none:
            none_card = NoneCard(_("Not now, I will choose later"))
            if group is not None:
                none_card.set_group(group)
            none_card.set_active(not chosen)
            none_card.set_halign(Gtk.Align.START)
            none_card.set_margin_top(6)
            box.append(none_card)
        return scrolled(clamp(box))

    def _summary_page(self) -> Gtk.Widget:
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        box.set_margin_top(28)
        box.set_margin_bottom(28)
        box.set_margin_start(24)
        box.set_margin_end(24)
        box.append(label(_("CONFIRM"), "step-eyebrow"))
        box.append(label(_("Ready to install"), "step-title"))
        box.append(self.summary_box)
        return scrolled(clamp(box, 820))

    def _rebuild_summary(self) -> None:
        while (child := self.summary_box.get_first_child()) is not None:
            self.summary_box.remove(child)
        plan = make_plan(self.catalog, self.selected_ids(), self.installed)
        if not plan.install:
            self.summary_box.append(label(_("You did not pick any apps. You can install apps any time from the "
                                            "App Center in the menu."), "dim-label"))
            return
        self.summary_box.append(label(_("Check your choices, then install. You can keep using the computer while "
                                        "the apps install."), "dim-label"))
        total = Gtk.Box(spacing=18)
        total.add_css_class("total-card")
        total.set_margin_top(10)
        numbers = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2, hexpand=True)
        numbers.append(label(_("About %s on disk") % format_size(plan.total_mb), "total-size", wrap=False))
        count = ngettext("%d app", "%d apps", len(plan.install)) % len(plan.install)
        if plan.runtime_mb:
            count += " · " + _("includes %s of shared Flathub libraries, downloaded once") % format_size(plan.runtime_mb)
        numbers.append(label(count, "dim-label"))
        total.append(numbers)
        self.summary_box.append(total)

        group = Adw.PreferencesGroup()
        group.set_margin_top(8)
        for app in plan.install:
            row = Adw.ActionRow(title=app.name, subtitle=f"{source_text(app)} · {format_size(app.size_mb)}")
            icon = app_icon(app, 40)
            icon.add_css_class("row-icon")
            row.add_prefix(icon)
            group.add(row)
        self.summary_box.append(group)

    # Navigation ------------------------------------------------------------------------------

    def selected_ids(self) -> list[str]:
        ids: list[str] = []
        for step in self.catalog.steps:
            ids.extend(app_id for app_id in step.apps if app_id in self.selection[step.id])
        return ids

    def go(self, index: int) -> None:
        index = max(0, min(index, self.carousel.get_n_pages() - 1))
        if index == self.carousel.get_n_pages() - 1:
            self._rebuild_summary()
        self.carousel.scroll_to(self.carousel.get_nth_page(index), True)
        self.index = index
        self._sync_bar()

    def _on_page_changed(self, _carousel, index: int) -> None:
        self.index = index
        self._sync_bar()

    def _sync_bar(self) -> None:
        last = self.carousel.get_n_pages() - 1
        self.bar.set_visible(self.index > 0)
        if self.index == last:
            count = len(make_plan(self.catalog, self.selected_ids(), self.installed).install)
            self.next_button.set_label(ngettext("Install %d app", "Install %d apps", count) % count if count else _("Finish"))
        else:
            self.next_button.set_label(_("Continue"))

    def _on_next(self, _button) -> None:
        if self.index < self.carousel.get_n_pages() - 1:
            self.go(self.index + 1)
            return
        ids = make_plan(self.catalog, self.selected_ids(), self.installed).ids
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
