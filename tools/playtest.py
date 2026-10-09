"""Run a play-test in the game window with fresh saves.

Each play-test plays saves written by the game's own fixture writers. Saves
carry the rules version, so ones left from an older build no longer load. This
writes them again from the same build as the game, runs the play-test with a
scratch home folder (the player's own saves and settings are untouched) and
prints its report.

    python3 tools/playtest.py morale
    python3 tools/playtest.py slums --game-dir /path/to/POOLRAD
    python3 tools/playtest.py gear -- --campaign --combat-demo ...

Play-tests: actions, gear, morale (the play-test fights) and slums (the Slums'
set encounters, which needs the original files). Arguments after -- go to the
play-test script. Build the game first (cmake --build --preset macos-universal).
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
PLAYTESTS = {
    "actions": "playtest_actions.gd",
    "gear": "playtest_gear.gd",
    "morale": "playtest_morale.gd",
    "slums": "playtest_slums.gd",
}


def default_godot():
    app = Path("/Applications/Godot_mono.app/Contents/MacOS/Godot")
    return os.environ.get("GODOT") or (str(app) if app.exists() else "godot")


def run(command, env=None):
    result = subprocess.run(command, env=env)
    if result.returncode:
        sys.exit(f"Failed ({result.returncode}): {' '.join(map(str, command))}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("playtest", choices=sorted(PLAYTESTS))
    parser.add_argument("--build", default=str(ROOT / "build/macos-universal"),
                        help="the game build whose fixture writers and library are used")
    parser.add_argument("--game-dir", default=os.environ.get("OPENGOLD_GAME_DIR", ""),
                        help="the original Pool of Radiance folder")
    parser.add_argument("--out", help="where the report and screenshots go (default: a new temporary folder)")
    parser.add_argument("--godot", default=default_godot())
    parser.add_argument("script_args", nargs="*", help="arguments for the play-test script, after --")
    args = parser.parse_args()

    build = Path(args.build)
    out = Path(args.out) if args.out else Path(tempfile.mkdtemp(prefix=f"playtest-{args.playtest}-"))
    home = out / "home"
    shutil.rmtree(home, ignore_errors=True)
    home.mkdir(parents=True)
    script_args = list(args.script_args)

    if args.playtest == "slums":
        if not args.game_dir:
            sys.exit("The Slums play-test needs the original files: --game-dir or OPENGOLD_GAME_DIR")
        fixtures = out / "slums-fixtures"
        shutil.rmtree(fixtures, ignore_errors=True)
        run([build / "opengold_expedition_tests"],
            dict(os.environ, OPENGOLD_GAME_DIR=args.game_dir, OPENGOLD_SLUMS_FIXTURES=str(fixtures)))
        script_args.append(f"--slums-fixtures={fixtures}")
    else:
        run([build / "opengold_playtest_fixtures"])
        script_args.append(f"--playtest-fixtures={build / 'playtest-fixtures'}")
    script_args.append(f"--playtest-out={out}")

    env = dict(os.environ, HOME=str(home), APPDATA=str(home), XDG_DATA_HOME=str(home),
               OPENGOLD_GAME_DIR=args.game_dir)
    script = ROOT / "tests" / PLAYTESTS[args.playtest]
    result = subprocess.run([args.godot, "--path", str(ROOT / "src/OpenGoldBox/godot"),
                             "--script", str(script), "--", *script_args],
                            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    report = out / "report.txt"
    print(report.read_text(encoding="utf-8") if report.exists() else "No report was written.")
    print(f"Report and screenshots: {out}")
    sys.exit(result.returncode or (0 if report.exists() else 1))


if __name__ == "__main__":
    main()
