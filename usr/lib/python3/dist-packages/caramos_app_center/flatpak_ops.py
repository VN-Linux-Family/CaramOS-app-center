"""System-wide Flatpak install and removal for the helper (runs as root).

libflatpak (gir1.2-flatpak-1.0, part of Linux Mint) reports real progress; the flatpak command is the
fallback when the bindings are missing.
"""

from __future__ import annotations

import subprocess
from collections.abc import Callable

REMOTE = "flathub"
ARCH = "x86_64"
BRANCH = "stable"

ProgressCallback = Callable[[int, str], None]
LogCallback = Callable[[str], None]


class FlatpakError(RuntimeError):
    pass


def app_ref(app_id: str) -> str:
    return f"app/{app_id}/{ARCH}/{BRANCH}"


def _load_flatpak():
    try:
        import gi

        gi.require_version("Flatpak", "1.0")
        from gi.repository import Flatpak, GLib
    except (ImportError, ValueError):
        return None, None
    return Flatpak, GLib


def _run_transaction(app_id: str, uninstall: bool, on_progress: ProgressCallback, on_log: LogCallback) -> None:
    Flatpak, GLib = _load_flatpak()
    if Flatpak is None:
        _run_cli(app_id, uninstall, on_progress, on_log)
        return
    installation = Flatpak.Installation.new_system(None)
    transaction = Flatpak.Transaction.new_for_installation(installation, None)
    transaction.set_no_interaction(True)
    if uninstall:
        transaction.add_uninstall(app_ref(app_id))
    else:
        transaction.add_install(REMOTE, app_ref(app_id), None)

    errors: list[str] = []

    def on_new_operation(tx, operation, progress):
        operations = tx.get_operations()
        index = next((i for i, op in enumerate(operations) if op.get_ref() == operation.get_ref()), 0)
        count = max(1, len(operations))
        name = operation.get_ref().split("/")[1] if "/" in operation.get_ref() else operation.get_ref()
        on_log(f"{'Removing' if uninstall else 'Installing'} {name}")

        def changed(progress_obj):
            overall = (index + progress_obj.get_progress() / 100) / count * 100
            on_progress(int(overall), progress_obj.get_status() or "")

        progress.set_update_frequency(500)
        progress.connect("changed", changed)

    def on_operation_error(_tx, operation, error, _details):
        errors.append(f"{operation.get_ref()}: {error.message}")
        return False  # stop the transaction

    transaction.connect("new-operation", on_new_operation)
    transaction.connect("operation-error", on_operation_error)
    try:
        transaction.run(None)
    except GLib.Error as exc:
        if exc.matches(Flatpak.error_quark(), Flatpak.Error.ALREADY_INSTALLED):
            on_log(f"{app_id} is already installed")
        elif exc.matches(Flatpak.error_quark(), Flatpak.Error.NOT_INSTALLED) and uninstall:
            on_log(f"{app_id} is not installed")
        else:
            raise FlatpakError(errors[0] if errors else exc.message) from exc
    on_progress(100, "")


def _run_cli(app_id: str, uninstall: bool, on_progress: ProgressCallback, on_log: LogCallback) -> None:
    args = ["flatpak", "uninstall" if uninstall else "install", "--system", "--noninteractive", "-y"]
    args += [app_id] if uninstall else [REMOTE, app_id]
    process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    assert process.stdout is not None
    for line in process.stdout:
        on_log(line.rstrip())
    if process.wait() != 0:
        raise FlatpakError(f"flatpak exited with {process.returncode}")
    on_progress(100, "")


def install(app_id: str, on_progress: ProgressCallback, on_log: LogCallback) -> None:
    _run_transaction(app_id, False, on_progress, on_log)


def uninstall(app_id: str, on_progress: ProgressCallback, on_log: LogCallback) -> None:
    _run_transaction(app_id, True, on_progress, on_log)
