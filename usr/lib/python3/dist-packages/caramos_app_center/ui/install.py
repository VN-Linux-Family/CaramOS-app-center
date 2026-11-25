"""Install (or remove) progress: one row per app, fed by the helper's events."""

from __future__ import annotations

from collections import deque
from collections.abc import Callable
from typing import Any

from gi.repository import Adw, Gtk

from ..i18n import _, ngettext

from .. import problem
from ..catalog import App
from ..runner import HelperRun
from .common import boxed_list, clamp, label, page_box, page_title, scrolled
from .rows import ProgressRow

Results = dict[str, tuple[bool, str]]


class InstallView(Adw.Bin):
    def __init__(self, apps: list[App], *, action: str, on_done: Callable[[Results], None],
                 on_cancelled: Callable[[], None], helper_action: str = "") -> None:
        super().__init__()
        self.apps = apps
        self.action = action
        # What the helper is asked to do: action itself, or flathub-install / flathub-remove for an app from Flathub
        # that the catalog does not have (an install link).
        self.helper_action = helper_action or action
        self.on_done = on_done
        self.on_cancelled = on_cancelled
        self.results: Results = {}
        # Each app's helper output, for asking VNLF for help when it fails (problem.py).
        self.logs: dict[str, deque[str]] = {}
        # What the helper reported for each failed app, as it said it (the done page's Details).
        self.reasons: dict[str, str] = {}
        self.run: HelperRun | None = None
        installing = action == "install"

        column = page_box()
        intro = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
        if not installing:
            title = _("Removing %s") % apps[0].name
        elif len(apps) == 1:
            title = _("Installing %s") % apps[0].name
        else:
            title = ngettext("Installing %d app", "Installing %d apps", len(apps)) % len(apps)
        intro.append(page_title(title))
        if installing:
            note = _("You can keep using the computer. Closing this window does not stop the installation.")
        else:
            note = _("You can keep using the computer. Closing this window does not stop the removal.")
        intro.append(label(note, "paragraph"))
        column.append(intro)

        listbox = boxed_list()
        self.rows = {app.id: ProgressRow(app, _("Waiting")) for app in apps}
        for row in self.rows.values():
            listbox.append(row)
        column.append(listbox)

        view = Adw.ToolbarView()
        view.add_top_bar(Adw.HeaderBar())
        view.set_content(scrolled(clamp(column)))
        self.set_child(view)

    def start(self) -> None:
        self.results = {}
        self.logs = {}
        self.reasons = {}
        self.run = HelperRun(self.helper_action, [app.id for app in self.apps], self._on_event, self._on_exit)

    def _working_text(self, percent: int | None = None) -> str:
        if self.action == "install":
            return _("Installing…") if percent is None else _("Installing… %d%%") % percent
        return _("Removing…") if percent is None else _("Removing… %d%%") % percent

    def _failed_text(self) -> str:
        return _("Could not install") if self.action == "install" else _("Could not remove")

    def _on_event(self, event: dict[str, Any]) -> None:
        row = self.rows.get(str(event.get("id", "")))
        kind = event["event"]
        if row is None:
            return
        if kind == "log":
            self.logs.setdefault(str(event["id"]), deque(maxlen=problem.MAX_LINES)).append(str(event.get("line", "")))
        elif kind == "start":
            row.busy(self._working_text())
        elif kind == "progress":
            percent = int(event["percent"])
            row.progress(percent / 100, self._working_text(percent))
        elif kind == "done":
            ok = bool(event.get("ok"))
            error = problem.failure_text(event)
            self.results[event["id"]] = (ok, error)
            if ok:
                row.finish(True, _("Installed") if self.action == "install" else _("Removed"))
            else:
                self.reasons[event["id"]] = str(event.get("error") or error or _("unknown error"))
                row.finish(False, self._failed_text())

    def _on_exit(self, code: int, auth_cancelled: bool) -> None:
        self.run = None
        if auth_cancelled and not self.results:
            self.on_cancelled()
            return
        for app in self.apps:
            if app.id not in self.results:
                self.results[app.id] = (False, _("not started"))
                self.reasons[app.id] = _("not started")
                self.rows[app.id].finish(False, self._failed_text())
        self.on_done(self.results)

    @property
    def running(self) -> bool:
        return self.run is not None
