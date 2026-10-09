"""Capture the game's screens and compare them before and after a change.

    python3 tools/ui_snapshots.py capture before [--game-dir /path/to/POOLRAD]
    ... change the game and rebuild ...
    python3 tools/ui_snapshots.py capture after [--game-dir /path/to/POOLRAD]
    python3 tools/ui_snapshots.py compare before after

capture runs the UI audit (tests/ui_audit.gd) in a window with fresh saves and
keeps a screenshot of every screen it audits, at three window sizes and in
English and Spanish. compare lists the screens that changed, appeared or went,
and writes for each changed one an image of the new screen with the changed
pixels in red. Screenshots hold the original game's art, so they stay in
user-data/ui-snapshots and never go in the repository. Build the game first.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True  # importing playtest.py leaves nothing behind
from playtest import ROOT, default_godot, run  # noqa: E402

SNAPSHOTS = ROOT / "user-data/ui-snapshots"


def capture(args):
    build = Path(args.build)
    folder = SNAPSHOTS / args.name
    shutil.rmtree(folder, ignore_errors=True)
    folder.mkdir(parents=True)
    work = Path(tempfile.mkdtemp(prefix="ui-snapshots-"))
    (work / "home").mkdir()
    run([build / "opengold_playtest_fixtures"])
    # The same rolls in character creation each time, so its pages compare.
    script_args = [f"--playtest-fixtures={build / 'playtest-fixtures'}", f"--capture={folder}",
                   "--fixed-seed"]
    if args.game_dir:
        fixtures = work / "slums-fixtures"
        run([build / "opengold_expedition_tests"],
            dict(os.environ, OPENGOLD_GAME_DIR=args.game_dir, OPENGOLD_SLUMS_FIXTURES=str(fixtures)))
        script_args.append(f"--slums-fixtures={fixtures}")
    home = str(work / "home")
    env = dict(os.environ, HOME=home, APPDATA=home, XDG_DATA_HOME=home,
               OPENGOLD_GAME_DIR=args.game_dir, OPENGOLD_LANG="en")
    result = subprocess.run([args.godot, "--path", str(ROOT / "src/OpenGoldBox/godot"),
                             "--script", str(ROOT / "tests/ui_audit.gd"), "--", *script_args],
                            env=env, capture_output=True, text=True)
    shutil.rmtree(work, ignore_errors=True)
    for line in result.stdout.splitlines():
        if line.startswith("UI"):
            print(line)
    count = len(list(folder.glob("*.png")))
    print(f"{count} screenshots in {folder}")
    if not count:
        sys.exit("No screenshots were taken.")


def compare(args):
    before, after = SNAPSHOTS / args.before, SNAPSHOTS / args.after
    for folder in (before, after):
        if not folder.is_dir():
            sys.exit(f"No snapshots named {folder.name}: capture them first.")
    out = SNAPSHOTS / f"{args.before}-vs-{args.after}"
    shutil.rmtree(out, ignore_errors=True)
    # Godot loads the game's project; a scratch home keeps it off the player's data.
    with tempfile.TemporaryDirectory(prefix="ui-compare-") as home:
        run([args.godot, "--headless", "--path", str(ROOT / "src/OpenGoldBox/godot"),
             "--script", str(ROOT / "tests/ui_compare.gd"), "--",
             f"--before={before}", f"--after={after}", f"--out={out}"],
            dict(os.environ, HOME=home, APPDATA=home, XDG_DATA_HOME=home))
    print(f"Changed screens, with the changes in red: {out}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--godot", default=default_godot())
    commands = parser.add_subparsers(dest="command", required=True)
    take = commands.add_parser("capture", help="screenshot every audited screen")
    take.add_argument("name", help="what to call this set, such as before or after")
    take.add_argument("--build", default=str(ROOT / "build/macos-universal"))
    take.add_argument("--game-dir", default=os.environ.get("OPENGOLD_GAME_DIR", ""),
                      help="the original files, for character creation, the town and campaign fights")
    take.set_defaults(action=capture)
    diff = commands.add_parser("compare", help="list what changed between two sets")
    diff.add_argument("before")
    diff.add_argument("after")
    diff.set_defaults(action=compare)
    args = parser.parse_args()
    args.action(args)


if __name__ == "__main__":
    main()
