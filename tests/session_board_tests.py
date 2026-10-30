"""tools/session_board.py as sessions and Claude Code's hook use it: claims,
overlap warnings, the hook's board and reminders, stale and forgotten claims."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "tools/session_board.py"


class SessionBoardTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.board = Path(self.folder.name) / ".sessions"

    def tearDown(self):
        self.folder.cleanup()

    def worktree(self, name):
        path = Path(self.folder.name) / name
        path.mkdir(exist_ok=True)
        return str(path)

    def run_board(self, session, *args, stdin=""):
        environment = dict(os.environ, OPENGOLD_SESSION_BOARD=str(self.board))
        environment.pop("CLAUDE_CODE_SESSION_ID", None)
        if session:
            environment["CLAUDE_CODE_SESSION_ID"] = session
        result = subprocess.run([sys.executable, str(SCRIPT), *args], input=stdin, env=environment,
                                capture_output=True, text=True)
        return result.returncode, result.stdout + result.stderr

    def hook(self, session, event):
        return self.run_board(None, "hook", stdin=json.dumps(
            {"session_id": session, "hook_event_name": event, "cwd": self.folder.name}))[1]

    def test_claims_show_on_the_board_and_overlaps_warn(self):
        code, _ = self.run_board("first", "claim", "--task", "Wand traps", "--name", "opengold-08",
                                 "--areas", "src/OpenGold.Core, Kovel Mansion",
                                 "--versions", "campaign format", "--issue", "4")
        self.assertEqual(code, 0)
        code, output = self.run_board("second", "claim", "--task", "Shop prices",
                                      "--areas", "src/OpenGold.Core/src/shop_data.cpp, kovel mansion",
                                      "--versions", "Campaign Format, rules version")
        self.assertEqual(code, 0)
        self.assertIn("WARNING: overlaps opengold-08 (Wand traps)", output)
        self.assertIn("area src/OpenGold.Core/src/shop_data.cpp", output)
        self.assertIn("area kovel mansion", output)
        self.assertIn("version Campaign Format", output)
        self.assertNotIn("rules version;", output)
        _, listing = self.run_board("second", "list")
        self.assertIn("opengold-08 (claude", listing)
        self.assertIn("Wand traps (#4)", listing)
        self.assertIn("raises: campaign format", listing)
        self.assertIn("Shop prices", listing)
        self.assertIn("<- this session", listing)

    def test_separate_areas_in_separate_worktrees_do_not_warn(self):
        self.run_board("first", "claim", "--task", "Combat art", "--areas", "src/OpenGoldBox",
                       "--worktree", self.worktree("art"))
        _, output = self.run_board("second", "claim", "--task", "Rules", "--areas", "src/OpenGold.Rules",
                                   "--worktree", self.worktree("rules"))
        self.assertNotIn("WARNING", output)

    def test_sharing_a_worktree_warns(self):
        self.run_board("first", "claim", "--task", "Combat art", "--worktree", self.worktree("art"))
        _, output = self.run_board("second", "claim", "--task", "Rules", "--worktree", self.worktree("art"))
        self.assertIn("WARNING: overlaps first (Combat art): worktree", output)

    def test_update_changes_only_what_it_names_and_release_removes(self):
        self.assertEqual(self.run_board("first", "update", "--task", "x")[0], 1)
        self.run_board("first", "claim", "--task", "Traps", "--areas", "a, b", "--name", "opengold-08")
        self.run_board("first", "update", "--areas", "c")
        claim = json.loads(next(self.board.glob("*.json")).read_text())
        self.assertEqual((claim["task"], claim["areas"], claim["name"]), ("Traps", ["c"], "opengold-08"))
        self.run_board("first", "claim", "--task", "Next task")
        claim = json.loads(next(self.board.glob("*.json")).read_text())
        self.assertEqual((claim["task"], claim.get("areas"), claim["name"]), ("Next task", None, "opengold-08"))
        code, _ = self.run_board("first", "release")
        self.assertEqual(code, 0)
        self.assertEqual(list(self.board.glob("*.json")), [])
        self.assertEqual(self.run_board("first", "release")[0], 1)

    def test_codex_is_known_by_its_worktree(self):
        self.run_board(None, "claim", "--task", "Frog art", "--agent", "codex",
                       "--worktree", self.folder.name)
        claim = json.loads(next(self.board.glob("*.json")).read_text())
        self.assertEqual(claim["id"], "worktree-" + Path(self.folder.name).name)
        self.assertEqual(claim["agent"], "codex")

    def test_hook_shows_the_board_when_it_changes(self):
        start = self.hook("first", "SessionStart")
        self.assertIn("no other session has a claim", start)
        self.assertIn("This session has no claim", start)
        self.run_board("first", "claim", "--task", "Traps", "--areas", "Kovel Mansion")
        self.assertEqual(self.hook("first", "UserPromptSubmit").strip(), "")
        self.run_board("second", "claim", "--task", "Kovel loot", "--areas", "kovel mansion")
        prompt = self.hook("first", "UserPromptSubmit")
        self.assertIn("Kovel loot", prompt)
        self.assertIn("WARNING: overlaps", prompt)
        self.assertEqual(self.hook("first", "UserPromptSubmit").strip(), "")
        self.assertIn("Kovel loot", self.hook("first", "SessionStart"))
        self.run_board("second", "release")
        self.assertIn("no other session has a claim", self.hook("first", "UserPromptSubmit"))

    def test_hook_keeps_a_claim_fresh_and_drops_forgotten_ones(self):
        self.run_board("first", "claim", "--task", "Traps")
        self.run_board("second", "claim", "--task", "Old work")
        paths = {json.loads(p.read_text())["task"]: p for p in self.board.glob("*.json")}
        for task, age in (("Traps", 3600), ("Old work", 5 * 3600)):
            claim = json.loads(paths[task].read_text())
            claim["updated"] = time.time() - age
            paths[task].write_text(json.dumps(claim))
        output = self.hook("first", "UserPromptSubmit")
        self.assertIn("idle 5 h, STALE", output)
        self.assertGreater(json.loads(paths["Traps"].read_text())["updated"], time.time() - 60)
        claim = json.loads(paths["Old work"].read_text())
        claim["updated"] = time.time() - 4 * 24 * 3600
        paths["Old work"].write_text(json.dumps(claim))
        self.assertIn("no other session has a claim", self.hook("first", "UserPromptSubmit"))
        self.assertFalse(paths["Old work"].exists())

    def test_hook_never_fails_a_prompt(self):
        code, output = self.run_board(None, "hook", stdin="not json")
        self.assertEqual(code, 0)
        self.assertIn("Session board unavailable", output)


if __name__ == "__main__":
    unittest.main()
