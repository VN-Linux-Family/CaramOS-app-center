"""Install (or remove) progress: one row per app, fed by the helper's events."""

from __future__ import annotations

from collections.abc import Callable
from typing import Any

from gi.repository import Adw, Gtk

from ..i18n import _, ngettext

from ..catalog import App
from ..runner import HelperRun
from .common import app_icon, clamp, label, scrolled

Results = dict[str, tuple[bool, str]]


class _AppRow:
    def __init__(self, app: App, waiting_text: str) -> None:
        self.row = Adw.ActionRow(title=app.name, subtitle=waiting_text)
        icon = app_icon(app, 40)
        icon.add_css_class("row-icon")
        self.row.add_prefix(icon)
        self.bar = Gtk.ProgressBar(valign=Gtk.Align.CENTER, width_request=150)
        self.bar.add_css_class("app-progress")
        self.status = Gtk.Image(pixel_size=20, visible=False)
        self.row.add_suffix(self.bar)
        self.row.add_suffix(self.status)
        self.fraction = 0.0

    def set_fraction(self, fraction: float) -> None:
        self.fraction = fraction
        self.bar.set_fraction(fraction)

    def finish(self, ok: bool, text: str) -> None:
        self.set_fraction(1.0 if ok else self.fraction)
        self.bar.set_visible(False)
        self.status.set_from_icon_name("emblem-ok-symbolic" if ok else "dialog-warning-symbolic")
        self.status.add_css_class("success" if ok else "warning")
        self.status.set_visible(True)
        self.row.set_subtitle(text)


class InstallView(Gtk.Box):
    def __init__(self, apps: list[App], *, action: str, on_done: Callable[[Results], None],
                 on_cancelled: Callable[[], None]) -> None:
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.apps = apps
        self.action = action
        self.on_done = on_done
        self.on_cancelled = on_cancelled
        self.results: Results = {}
        self.run: HelperRun | None = None
        installing = action == "install"

        header = Adw.HeaderBar()
        header.set_title_widget(Adw.WindowTitle(title=_("Installing apps") if installing else _("Removing app"),
                                                subtitle=_("CaramOS App Center")))
        self.append(header)

        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(30)
        box.set_margin_bottom(30)
        box.set_margin_start(24)
        box.set_margin_end(24)
        if installing:
            title = ngettext("Installing %d app", "Installing %d apps", len(apps)) % len(apps)
        else:
            title = _("Removing %s") % apps[0].name
        box.append(label(title, "step-title"))
        box.append(label(_("You can keep using the computer. Closing this window does not stop the installation."),
                         "dim-label"))
        self.overall = Gtk.ProgressBar()
        self.overall.add_css_class("overall")
        self.overall.set_margin_top(14)
        self.overall.set_margin_bottom(10)
        box.append(self.overall)

        group = Adw.PreferencesGroup()
        self.rows = {app.id: _AppRow(app, _("Waiting")) for app in apps}
        for row in self.rows.values():
            group.add(row.row)
        box.append(group)
        self.append(scrolled(clamp(box, 820)))

    def start(self) -> None:
        self.results = {}
        for row in self.rows.values():
            row.set_fraction(0.0)
        self.run = HelperRun(self.action, [app.id for app in self.apps], self._on_event, self._on_exit)

    def _update_overall(self) -> None:
        total = sum(row.fraction for row in self.rows.values())
        self.overall.set_fraction(total / max(1, len(self.rows)))

    def _on_event(self, event: dict[str, Any]) -> None:
        row = self.rows.get(str(event.get("id", "")))
        kind = event["event"]
        if row is None:
            return
        if kind == "start":
            row.row.set_subtitle(_("Downloading and installing…") if self.action == "install" else _("Removing…"))
        elif kind == "progress":
            row.set_fraction(event["percent"] / 100)
            self._update_overall()
        elif kind == "done":
            ok = bool(event.get("ok"))
            error = str(event.get("error") or "")
            self.results[event["id"]] = (ok, error)
            if ok:
                text = _("Installed") if self.action == "install" else _("Removed")
            else:
                text = _("Failed: %s") % error if error else _("Failed")
            row.finish(ok, text)
            self._update_overall()

    def _on_exit(self, code: int, auth_cancelled: bool) -> None:
        self.run = None
        if auth_cancelled and not self.results:
            self.on_cancelled()
            return
        for app in self.apps:
            if app.id not in self.results:
                self.results[app.id] = (False, _("not started"))
                self.rows[app.id].finish(False, _("Not installed"))
        self.overall.set_fraction(1.0)
        self.on_done(self.results)

    @property
    def running(self) -> bool:
        return self.run is not None
