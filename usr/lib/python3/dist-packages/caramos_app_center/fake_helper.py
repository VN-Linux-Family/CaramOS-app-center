"""Development stand-in for the privileged helper: emits the same events without installing anything.

Used only when CARAMOS_APP_CENTER_FAKE_HELPER=1, to work on the UI without root or network.
Apps listed in CARAMOS_APP_CENTER_FAKE_FAIL (comma separated) fail, to show the error state.
"""

from __future__ import annotations

import os
import sys
import time

from .events import emit


def main(argv: list[str]) -> int:
    action, ids = argv[0], argv[1:]
    fail = set(filter(None, os.environ.get("CARAMOS_APP_CENTER_FAKE_FAIL", "").split(",")))
    step_delay = float(os.environ.get("CARAMOS_APP_CENTER_FAKE_DELAY", "0.08"))
    failed = []
    for index, app_id in enumerate(ids, 1):
        emit("start", id=app_id, index=index, total=len(ids), action=action)
        for percent in range(0, 101, 5):
            emit("progress", id=app_id, percent=percent, message="")
            time.sleep(step_delay)
        if app_id in fail:
            emit("done", id=app_id, ok=False, error="simulated failure")
            failed.append(app_id)
        else:
            emit("done", id=app_id, ok=True)
    emit("finished", ok=not failed, failed=failed)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
