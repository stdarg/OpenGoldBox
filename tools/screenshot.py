"""Request PNG screenshots from a running OpenGoldBox instance.

Use --directory to match OPENGOLD_SCREENSHOT_DIR when the game was launched
with a custom output folder. Otherwise use the game's normal user-data folder.
"""
import argparse
import json
import os
from pathlib import Path
import sys
import tempfile
import time
import uuid


def default_directory():
    override = os.environ.get("OPENGOLD_SCREENSHOT_DIR")
    if override:
        return Path(override)
    if sys.platform == "darwin":
        base = Path.home() / "Library/Application Support"
    elif os.name == "nt":
        return Path(os.environ["APPDATA"]) / "OpenGold/screenshots"
    else:
        base = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share"))
    return base / ("godot" if sys.platform.startswith("linux") else "Godot") / "app_userdata/OpenGold/screenshots"


def capture(directory, timeout):
    directory = directory.expanduser().resolve()
    if not directory.is_dir():
        raise RuntimeError(f"Screenshots folder is missing. Start the game first: {directory}")
    request = directory / "capture.request"
    token = uuid.uuid4().hex
    # Publish complete contents atomically and preserve any pending request.
    try:
        with tempfile.TemporaryDirectory(dir=directory, prefix=".capture-") as staging:
            prepared = Path(staging) / "request"
            prepared.write_text(token, encoding="utf-8")
            os.link(prepared, request)
    except FileExistsError as error:
        raise RuntimeError(f"A capture request is already pending: {request}") from error
    try:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                result = json.loads((directory / "latest.json").read_text(encoding="utf-8"))
            except (FileNotFoundError, json.JSONDecodeError):
                result = {}
            if result.get("request_id") == token:
                if not result.get("ok"):
                    raise RuntimeError(result.get("error", "Screenshot capture failed"))
                return result["files"]
            time.sleep(0.05)
        raise RuntimeError("Timed out waiting for a screenshot. Check that the graphical game is running and uses this folder.")
    finally:
        # Do not leave a timed-out request to capture unexpectedly on a later run.
        try:
            if request.read_text(encoding="utf-8") == token:
                request.unlink(missing_ok=True)
        except FileNotFoundError:
            pass  # The game consumed the request while the tool was finishing.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, default=default_directory())
    parser.add_argument("--timeout", type=float, default=10)
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    try:
        files = capture(args.directory, args.timeout)
        for file in files:
            print(file["path"])
    except (OSError, RuntimeError) as error:
        print(f"Screenshot: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
