"""JSON-lines protocol between the privileged helper (stdout) and the App Center UI."""

from __future__ import annotations

import json
import sys
from typing import Any, TextIO

EVENTS = ("start", "progress", "log", "done", "finished")


def emit(event: str, stream: TextIO | None = None, **fields: Any) -> None:
    if event not in EVENTS:
        raise ValueError(f"unknown event {event}")
    out = sys.stdout if stream is None else stream
    out.write(json.dumps({"event": event, **fields}, ensure_ascii=False) + "\n")
    out.flush()


def parse_event(line: str) -> dict[str, Any] | None:
    """Return the event dict for a protocol line, None for anything else (pkexec noise, blank lines)."""

    line = line.strip()
    if not line.startswith("{"):
        return None
    try:
        data = json.loads(line)
    except ValueError:
        return None
    if not isinstance(data, dict) or data.get("event") not in EVENTS:
        return None
    if data["event"] == "progress":
        try:
            data["percent"] = max(0, min(100, int(data.get("percent", 0))))
        except (TypeError, ValueError):
            return None
    return data
