#!/usr/bin/env python3
"""Builds this platform's package and uploads it to a draft GitHub release.

Run on macOS, Linux or Windows from a clean checkout of main that matches
origin/main:

    tools/release.py            (macOS and Linux)
    python tools/release.py     (Windows)

It builds and runs every test (./build.sh --test-all, with --package on
macOS; build-opengoldbox.cmd on Windows), so OPENGOLD_GAME_DIR must point at
the original game files for the package checks to run. It then uploads the package players download to the
draft release v<version>, the version being project()'s in CMakeLists.txt:
it creates the draft at the current commit, or adds to the one another
platform made, so each platform's package lands on one release.

It never publishes and never touches a published release: check the draft on
GitHub, write its notes, then publish it there, which also creates the tag.
The debug symbols archive is not uploaded; keep it with the release yourself.
Needs the GitHub CLI (gh), logged in. Uses only Python's standard library.
"""
import argparse
from dataclasses import dataclass
import os
from pathlib import Path
import platform as host
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


class ReleaseError(Exception):
    """A reason not to release; the message says what to fix."""


@dataclass(frozen=True)
class Platform:
    name: str
    build_command: tuple
    build_folder: str
    package_name: str
    symbols_name: str

    def package(self, version):
        return ROOT / self.build_folder / self.package_name.format(version=version)

    def symbols(self, version):
        return ROOT / self.build_folder / self.symbols_name.format(version=version)


PLATFORMS = {
    "Darwin": Platform("macOS", ("./build.sh", "--test-all", "--package"), "build/macos-universal",
                       "OpenGoldBox-{version}-macos.dmg", "OpenGoldBox-{version}-macos-symbols.zip"),
    "Linux": Platform("Linux", ("./build.sh", "--test-all"), "build/linux",
                      "OpenGoldBox-{version}-linux-x86_64.tar.gz",
                      "OpenGoldBox-{version}-linux-symbols.tar.gz"),
    # build-opengoldbox.cmd builds the package and runs every test.
    "Windows": Platform("Windows", ("cmd", "/c", "build-opengoldbox.cmd"), "build/game",
                        "OpenGoldBox-{version}-windows-x86_64.zip",
                        "OpenGoldBox-{version}-windows-symbols.zip"),
}


def run_command(args, capture=True):
    """Runs a command in the checkout; captured output is returned as text."""
    return subprocess.run(args, cwd=ROOT, capture_output=capture, text=True)


def platform_for(system):
    if system not in PLATFORMS:
        raise ReleaseError(f"tools/release.py runs on macOS, Linux and Windows, not {system}")
    return PLATFORMS[system]


def project_version(cmake_lists):
    found = re.search(r"project\(OpenGoldBox VERSION (\d+\.\d+\.\d+)", cmake_lists)
    if not found:
        raise ReleaseError("CMakeLists.txt names no OpenGoldBox version in project()")
    return found.group(1)


def output_of(run, args):
    result = run(args)
    if result.returncode != 0:
        raise ReleaseError(f"{' '.join(args)} failed: {result.stderr.strip()}")
    return result.stdout.strip()


def released_commit(run):
    """The commit to release: HEAD, which must be a clean main that matches origin/main."""
    if output_of(run, ["git", "status", "--porcelain"]):
        raise ReleaseError("The checkout has uncommitted changes; a release is built from a commit")
    branch = output_of(run, ["git", "rev-parse", "--abbrev-ref", "HEAD"])
    if branch != "main":
        raise ReleaseError(f"The checkout is on {branch}; releases are built from main")
    output_of(run, ["git", "fetch", "--quiet", "origin", "main"])
    head = output_of(run, ["git", "rev-parse", "HEAD"])
    if head != output_of(run, ["git", "rev-parse", "origin/main"]):
        raise ReleaseError("main differs from origin/main; pull or push first")
    return head


def check_prerequisites(run, environment):
    if run(["gh", "auth", "status"]).returncode != 0:
        raise ReleaseError("The GitHub CLI is not logged in: run 'gh auth login'")
    if not environment.get("OPENGOLD_GAME_DIR"):
        raise ReleaseError("OPENGOLD_GAME_DIR is not set, so the package checks would be skipped")


def build_package(target, version, run):
    """Builds and tests; returns the package, which must come from this build."""
    started = time.time()
    if run(list(target.build_command), capture=False).returncode != 0:
        raise ReleaseError("The build or a test failed; nothing was uploaded")
    package = target.package(version)
    if not package.is_file() or package.stat().st_mtime < started:
        raise ReleaseError(f"The build did not write {package}")
    return package


def draft_state(tag, run):
    """None when the release does not exist, else (is_draft, target commit)."""
    result = run(["gh", "release", "view", tag, "--json", "isDraft,targetCommitish",
                  "--jq", '"\\(.isDraft) \\(.targetCommitish)"'])
    if result.returncode != 0:
        if "not found" in result.stderr.lower():
            return None
        raise ReleaseError(f"Cannot read release {tag}: {result.stderr.strip()}")
    is_draft, target = result.stdout.split()
    return is_draft == "true", target


def upload(tag, version, commit, package, run):
    state = draft_state(tag, run)
    if state is None:
        output_of(run, ["gh", "release", "create", tag, "--draft", "--target", commit,
                        "--title", f"OpenGoldBox {version}",
                        "--notes", f"Built from {commit[:10]}.", str(package)])
        return
    is_draft, target = state
    if not is_draft:
        raise ReleaseError(f"{tag} is already published; raise the version in CMakeLists.txt")
    if target != commit:
        raise ReleaseError(f"The draft {tag} was made from {target[:10]}, not {commit[:10]}; "
                           "build every platform from one commit")
    output_of(run, ["gh", "release", "upload", tag, str(package), "--clobber"])


def release(system, environment, run=run_command):
    target = platform_for(system)
    version = project_version((ROOT / "CMakeLists.txt").read_text(encoding="utf-8"))
    tag = f"v{version}"
    commit = released_commit(run)
    check_prerequisites(run, environment)
    package = build_package(target, version, run)
    upload(tag, version, commit, package, run)
    url = output_of(run, ["gh", "release", "view", tag, "--json", "url", "--jq", ".url"])
    print(f"Uploaded the {target.name} package to the draft {tag}: {url}")
    print(f"Keep the debug symbols, not uploaded: {target.symbols(version)}")


def main():
    argparse.ArgumentParser(description=__doc__,
                            formatter_class=argparse.RawDescriptionHelpFormatter).parse_args()
    try:
        release(host.system(), os.environ)
    except ReleaseError as error:
        print(f"release: {error}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
