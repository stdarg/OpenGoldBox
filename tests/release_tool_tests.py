"""Exercise tools/release.py's checks and GitHub steps without building or uploading."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import time
import unittest

spec = importlib.util.spec_from_file_location("release", Path(__file__).resolve().parents[1] / "tools/release.py")
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)

COMMIT = "a" * 40
ENVIRONMENT = {"OPENGOLD_GAME_DIR": "/games/POOLRAD"}


class FakeCommands:
    """Answers each command as a checkout and GitHub in the given state would."""

    def __init__(self, root, release_view=None, dirty=False, branch="main", origin=COMMIT,
                 build_fails=False, build_writes=True,
                 package="build/linux/OpenGoldBox-1.2.3-linux-x86_64.tar.gz"):
        self.root = root
        self.package = package
        self.release_view = release_view
        self.answers = {
            ("git", "status", "--porcelain"): " M build.sh" if dirty else "",
            ("git", "rev-parse", "--abbrev-ref", "HEAD"): branch,
            ("git", "rev-parse", "HEAD"): COMMIT,
            ("git", "rev-parse", "origin/main"): origin,
        }
        self.build_fails = build_fails
        self.build_writes = build_writes
        self.commands = []

    def __call__(self, args, capture=True):
        self.commands.append(list(args))
        key = tuple(args)
        if key in self.answers:
            return self.result(stdout=self.answers[key])
        if args[0] in ("./build.sh", "cmd"):
            # A build that does not write leaves an old package behind. Windows
            # stamps files from a coarser clock than time.time(), so the times
            # are set well clear of the build's start.
            package = self.root / self.package
            package.parent.mkdir(parents=True, exist_ok=True)
            package.write_bytes(b"package")
            stamp = time.time() + (60 if self.build_writes else -60)
            os.utime(package, (stamp, stamp))
            return self.result(code=1 if self.build_fails else 0)
        if args[:3] == ["gh", "release", "view"] and "isDraft,targetCommitish" in args:
            if self.release_view is None:
                return self.result(code=1, stderr="release not found")
            return self.result(stdout=self.release_view)
        return self.result(stdout="https://github.com/owner/repo/releases/tag/untagged")

    @staticmethod
    def result(code=0, stdout="", stderr=""):
        return subprocess.CompletedProcess([], code, stdout, stderr)

    def gh_release(self, action):
        return [command for command in self.commands if command[:3] == ["gh", "release", action]]


class ReleaseToolTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        (self.root / "CMakeLists.txt").write_text("project(OpenGoldBox VERSION 1.2.3 LANGUAGES CXX)\n")
        self.real_root = release.ROOT
        release.ROOT = self.root

    def tearDown(self):
        release.ROOT = self.real_root
        self.directory.cleanup()

    def release_with(self, commands, environment=ENVIRONMENT, system="Linux"):
        release.release(system, environment, commands)

    def test_version_comes_from_project(self):
        self.assertEqual(release.project_version("project(OpenGoldBox VERSION 0.4.10 LANGUAGES CXX)"), "0.4.10")
        with self.assertRaisesRegex(release.ReleaseError, "no OpenGoldBox version"):
            release.project_version("project(Other)")

    def test_only_macos_linux_and_windows(self):
        self.assertEqual(release.platform_for("Darwin").name, "macOS")
        self.assertIn("--package", release.platform_for("Darwin").build_command)
        with self.assertRaisesRegex(release.ReleaseError, "not FreeBSD"):
            release.platform_for("FreeBSD")

    def test_windows_builds_with_its_script_and_uploads_the_zip(self):
        commands = FakeCommands(self.root, package="build/game/OpenGoldBox-1.2.3-windows-x86_64.zip")
        self.release_with(commands, system="Windows")
        self.assertIn(["cmd", "/c", r".\build-opengoldbox.cmd"], commands.commands)
        created = commands.gh_release("create")
        self.assertEqual(len(created), 1)
        self.assertTrue(created[0][-1].endswith("OpenGoldBox-1.2.3-windows-x86_64.zip"))

    def test_first_platform_creates_the_draft_with_its_package(self):
        commands = FakeCommands(self.root)
        self.release_with(commands)
        created = commands.gh_release("create")
        self.assertEqual(len(created), 1)
        self.assertIn("--draft", created[0])
        self.assertEqual(created[0][created[0].index("--target") + 1], COMMIT)
        self.assertTrue(created[0][-1].endswith("OpenGoldBox-1.2.3-linux-x86_64.tar.gz"))
        self.assertEqual(commands.gh_release("upload"), [])
        self.assertEqual(commands.gh_release("edit"), [], "never publishes")
        self.assertFalse(any("symbols" in part for command in commands.commands for part in command))

    def test_next_platform_adds_to_the_same_draft(self):
        commands = FakeCommands(self.root, release_view=f"true {COMMIT}")
        self.release_with(commands)
        self.assertEqual(commands.gh_release("create"), [])
        uploaded = commands.gh_release("upload")
        self.assertEqual(len(uploaded), 1)
        self.assertIn("--clobber", uploaded[0])

    def test_published_release_is_left_alone(self):
        commands = FakeCommands(self.root, release_view=f"false {COMMIT}")
        with self.assertRaisesRegex(release.ReleaseError, "already published"):
            self.release_with(commands)
        self.assertEqual(commands.gh_release("upload"), [])

    def test_draft_from_another_commit_is_refused(self):
        commands = FakeCommands(self.root, release_view=f"true {'b' * 40}")
        with self.assertRaisesRegex(release.ReleaseError, "one commit"):
            self.release_with(commands)
        self.assertEqual(commands.gh_release("upload"), [])

    def test_checkout_must_be_clean_pushed_main(self):
        for state, message in ((dict(dirty=True), "uncommitted"), (dict(branch="topic"), "on topic"),
                               (dict(origin="c" * 40), "differs from origin/main")):
            with self.subTest(state=state):
                commands = FakeCommands(self.root, **state)
                with self.assertRaisesRegex(release.ReleaseError, message):
                    self.release_with(commands)
                self.assertFalse(any(command[0] == "./build.sh" for command in commands.commands))

    def test_game_files_are_needed_before_building(self):
        commands = FakeCommands(self.root)
        with self.assertRaisesRegex(release.ReleaseError, "OPENGOLD_GAME_DIR"):
            self.release_with(commands, environment={})
        self.assertFalse(any(command[0] == "./build.sh" for command in commands.commands))

    def test_failed_or_stale_build_uploads_nothing(self):
        for state, message in ((dict(build_fails=True), "nothing was uploaded"),
                               (dict(build_writes=False), "did not write")):
            with self.subTest(state=state):
                commands = FakeCommands(self.root, **state)
                with self.assertRaisesRegex(release.ReleaseError, message):
                    self.release_with(commands)
                self.assertEqual(commands.gh_release("create") + commands.gh_release("upload"), [])


if __name__ == "__main__":
    unittest.main()
