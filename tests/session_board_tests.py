"""tools/session_board.py as sessions and Claude Code's hook use it: claims
kept as GitHub issues labelled `claim` (through a stand-in for gh), overlap
warnings, the hook's board, cache and reminders, stale and forgotten claims,
claims waiting on the machine while GitHub is unreachable, and conversations
between sessions kept as issues labelled `message`."""
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import time
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "tools/session_board.py"
FAKE_GH = Path(__file__).resolve().parent / "fake_gh.py"


class SessionBoardTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.board = Path(self.folder.name) / ".sessions"
        self.github = Path(self.folder.name) / "github.json"
        self.offline = False

    def tearDown(self):
        self.folder.cleanup()

    def worktree(self, name):
        path = Path(self.folder.name) / name
        path.mkdir(exist_ok=True)
        return str(path)

    def run_board(self, session, *args, stdin=""):
        environment = dict(os.environ, OPENGOLD_SESSION_BOARD=str(self.board), OPENGOLD_GH=str(FAKE_GH),
                           FAKE_GH_STATE=str(self.github), OPENGOLD_MESSAGE_POLL="0.1")
        environment.pop("CLAUDE_CODE_SESSION_ID", None)
        environment.pop("FAKE_GH_OFFLINE", None)
        if self.offline:
            environment["FAKE_GH_OFFLINE"] = "1"
        if session:
            environment["CLAUDE_CODE_SESSION_ID"] = session
        result = subprocess.run([sys.executable, str(SCRIPT), *args], input=stdin, env=environment,
                                capture_output=True, text=True)
        return result.returncode, result.stdout + result.stderr

    def hook(self, session, event):
        return self.run_board(None, "hook", stdin=json.dumps(
            {"session_id": session, "hook_event_name": event, "cwd": self.folder.name}))[1]

    def state(self):
        return json.loads(self.github.read_text()) if self.github.exists() else {"issues": [], "calls": []}

    def issues(self, state="open", label="claim"):
        return [issue for issue in self.state()["issues"] if issue["state"] == state and label in issue["labels"]]

    def conversations(self, state="open"):
        return self.issues(state, "message")

    def two_sessions(self):
        """first (named opengold-08) holds claim #1; second holds claim #2."""
        self.run_board("first", "claim", "--task", "Dark Queen", "--name", "opengold-08")
        self.run_board("second", "claim", "--task", "Buck Rogers")

    def stored(self, issue):
        body = issue["body"]
        start = body.index("<!-- session-board\n") + len("<!-- session-board\n")
        return json.loads(body[start:body.index("\n-->", start)])

    def age_claim(self, task, seconds):
        """Makes a claim look idle, as if written that long ago on any machine."""
        state = self.state()
        issue = next(i for i in state["issues"] if i["state"] == "open" and self.stored(i)["task"] == task)
        claim = self.stored(issue)
        claim["updated"] = time.time() - seconds
        issue["body"] = issue["body"][:issue["body"].index("<!-- session-board")] \
            + "<!-- session-board\n" + json.dumps(claim) + "\n-->\n"
        self.github.write_text(json.dumps(state))

    def age_cache(self):
        """Makes the hook's last look at GitHub (or try) an hour old."""
        cache = self.board / ".github-claims.json"
        if not cache.exists():
            return
        data = json.loads(cache.read_text())
        for key in ("fetched", "tried"):
            data[key] = data.get(key, 0) - 3600
        cache.write_text(json.dumps(data))

    def test_a_claim_is_an_issue_labelled_claim(self):
        code, _ = self.run_board("first", "claim", "--task", "Wand traps", "--name", "opengold-08",
                                 "--areas", "src/OpenGold.Core, Kovel Mansion",
                                 "--versions", "campaign format", "--issue", "4")
        self.assertEqual(code, 0)
        [issue] = self.issues()
        self.assertEqual(issue["labels"], ["claim"])
        self.assertEqual(issue["title"], "Claim: Wand traps (opengold-08)")
        for words in ("**Task:** Wand traps", "**Work issue:** #4", "**Areas:** src/OpenGold.Core, Kovel Mansion",
                      "**Raises:** campaign format", "**Last active:**"):
            self.assertIn(words, issue["body"])
        self.assertEqual(self.stored(issue)["host"], platform.node())

    def test_claims_show_on_the_board_and_overlaps_warn(self):
        self.run_board("first", "claim", "--task", "Wand traps", "--name", "opengold-08",
                       "--areas", "src/OpenGold.Core, Kovel Mansion",
                       "--versions", "campaign format", "--issue", "4")
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
        self.assertIn("opengold-08 (claude on", listing)
        self.assertIn("[claim #1]: Wand traps (#4)", listing)
        self.assertIn("raises: campaign format", listing)
        self.assertIn("Shop prices", listing)
        self.assertIn("<- this session", listing)

    def test_a_task_cannot_end_the_hidden_data(self):
        self.run_board("first", "claim", "--task", "Arrows --> and <!-- comments")
        _, listing = self.run_board("first", "list")
        self.assertIn("Arrows --> and <!-- comments", listing)

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

    def test_update_edits_the_issue_a_new_claim_opens_another_and_release_closes(self):
        self.assertEqual(self.run_board("first", "update", "--task", "x")[0], 1)
        self.run_board("first", "claim", "--task", "Traps", "--areas", "a, b", "--name", "opengold-08")
        self.run_board("first", "update", "--areas", "c")
        [issue] = self.issues()
        claim = self.stored(issue)
        self.assertEqual((claim["task"], claim["areas"], claim["name"]), ("Traps", ["c"], "opengold-08"))
        self.run_board("first", "claim", "--task", "Next task")
        [old] = self.issues("closed")
        [new] = self.issues()
        self.assertEqual((old["number"], old["reason"]), (issue["number"], "completed"))
        claim = self.stored(new)
        self.assertEqual((claim["task"], claim.get("areas"), claim["name"]), ("Next task", None, "opengold-08"))
        code, _ = self.run_board("first", "release")
        self.assertEqual(code, 0)
        self.assertEqual(self.issues(), [])
        self.assertEqual(self.run_board("first", "release")[0], 1)

    def test_a_second_open_claim_of_a_session_is_closed(self):
        # GitHub's list can miss a just-opened issue, so a claim made right
        # after another may not close the first.
        self.run_board("first", "claim", "--task", "Traps")
        state = self.state()
        older = dict(state["issues"][0], number=50)
        older["body"] = older["body"].replace('"started": ', '"started": 1')
        state["issues"].append(older)
        self.github.write_text(json.dumps(state))
        _, listing = self.run_board("first", "list")
        self.assertEqual(listing.count("Traps"), 1)
        self.assertEqual([issue["number"] for issue in self.issues()], [50])

    def test_release_by_number_frees_another_sessions_claim(self):
        self.run_board("first", "claim", "--task", "Old work")
        code, output = self.run_board("second", "release", "--claim", "1")
        self.assertEqual(code, 0)
        self.assertIn("Released claim #1", output)
        self.assertEqual(self.issues(), [])

    def test_codex_is_known_by_its_machine_and_worktree(self):
        self.run_board(None, "claim", "--task", "Frog art", "--agent", "codex",
                       "--worktree", self.folder.name)
        claim = self.stored(self.issues()[0])
        self.assertEqual(claim["id"], f"worktree-{platform.node()}-{Path(self.folder.name).name}")
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

    def test_hook_asks_github_at_most_every_two_minutes(self):
        self.run_board("first", "claim", "--task", "Traps")
        self.hook("first", "SessionStart")
        # Another machine claims; this machine's cache does not know yet.
        state = self.state()
        other = dict(self.issues()[0], number=99, title="Claim: Far work")
        other["body"] = other["body"].replace('"task": "Traps"', '"task": "Far work"').replace(
            '"id": "first"', '"id": "far"')
        state["issues"].append(other)
        self.github.write_text(json.dumps(state))
        calls = len(self.state()["calls"])
        self.assertNotIn("Far work", self.hook("first", "UserPromptSubmit"))
        self.assertEqual(len(self.state()["calls"]), calls)
        self.age_cache()
        self.assertIn("Far work", self.hook("first", "UserPromptSubmit"))

    def test_hook_keeps_a_claim_fresh_and_closes_forgotten_ones(self):
        self.run_board("first", "claim", "--task", "Traps")
        self.run_board("second", "claim", "--task", "Old work")
        self.age_claim("Traps", 3600)
        self.age_claim("Old work", 5 * 3600)
        self.age_cache()
        output = self.hook("first", "UserPromptSubmit")
        self.assertIn("idle 5 h, STALE", output)
        mine = next(i for i in self.issues() if self.stored(i)["task"] == "Traps")
        self.assertGreater(self.stored(mine)["updated"], time.time() - 60)
        self.age_claim("Old work", 4 * 24 * 3600)
        self.age_cache()
        self.assertIn("no other session has a claim", self.hook("first", "UserPromptSubmit"))
        [forgotten] = self.issues("closed")
        self.assertEqual(forgotten["reason"], "not planned")
        self.assertIn("idle for over 3 days", forgotten["comments"][0]["body"])

    def test_offline_claims_and_releases_wait_on_the_machine(self):
        self.offline = True
        code, output = self.run_board("first", "claim", "--task", "Traps")
        self.assertEqual(code, 0)
        self.assertIn("GitHub unreachable", output)
        _, listing = self.run_board("first", "list")
        self.assertIn("[not yet on GitHub]: Traps", listing)
        self.offline = False
        _, listing = self.run_board("first", "list")
        self.assertIn("[claim #1]: Traps", listing)
        self.assertEqual(len(self.issues()), 1)
        self.assertEqual(list(self.board.glob("pending-*")), [])
        self.offline = True
        self.assertEqual(self.run_board("first", "release")[0], 0)
        self.assertIn("no claims", self.run_board("first", "list")[1])
        self.offline = False
        self.age_cache()
        self.hook("first", "UserPromptSubmit")
        self.assertEqual(self.issues(), [])
        self.assertEqual(list(self.board.glob("pending-*")), [])

    def test_a_message_opens_a_conversation_issue_the_recipient_reads(self):
        self.two_sessions()
        code, output = self.run_board("second", "send", "--to", "1", "--subject", "Game numbers",
                                      "May I take GoldBoxGame IDs 10 and 11?")
        self.assertEqual(code, 0, output)
        self.assertIn("Sent to opengold-08 in conversation #3", output)
        [conversation] = self.conversations()
        self.assertEqual(conversation["labels"], ["message"])
        self.assertIn("Message: Game numbers (", conversation["title"])
        self.assertIn("**Subject:** Game numbers", conversation["body"])
        [comment] = conversation["comments"]
        self.assertIn("May I take GoldBoxGame IDs 10 and 11?", comment["body"])
        self.assertIn("to opengold-08:", comment["body"])
        _, inbox = self.run_board("first", "inbox")
        self.assertIn('#3 "Game numbers" from claude on', inbox)
        self.assertIn("    May I take GoldBoxGame IDs 10 and 11?", inbox)
        self.assertIn("Open conversations:", inbox)
        self.assertIn("No new messages.", self.run_board("first", "inbox")[1])
        self.assertIn("No new messages.", self.run_board("second", "inbox")[1])
        self.assertEqual(self.run_board("third", "inbox")[1].strip(), "No new messages.")

    def test_replies_and_new_messages_stay_in_one_conversation(self):
        self.two_sessions()
        self.run_board("second", "send", "--to", "opengold-08", "--subject", "Rebase", "First question")
        code, output = self.run_board("first", "reply", "3", "An answer")
        self.assertEqual(code, 0, output)
        self.assertIn("An answer", self.run_board("second", "inbox")[1])
        self.run_board("second", "send", "--to", "1", "Second question")
        [conversation] = self.conversations()
        self.assertEqual(len(conversation["comments"]), 3)
        _, inbox = self.run_board("first", "inbox")
        self.assertIn("Second question", inbox)
        self.assertNotIn("First question", inbox)
        _, whole = self.run_board("first", "inbox", "--conversation", "3")
        for text in ("First question", "An answer", "Second question"):
            self.assertIn(text, whole)

    def test_a_third_session_joins_by_replying(self):
        self.two_sessions()
        self.run_board("second", "send", "--to", "1", "Who owns the menu?")
        self.run_board("third", "reply", "3", "I do")
        for session in ("first", "second"):
            self.assertIn("I do", self.run_board(session, "inbox")[1])
        self.run_board("first", "reply", "3", "Thanks")
        self.assertIn("Thanks", self.run_board("third", "inbox")[1])

    def test_hook_shows_a_new_message_once(self):
        self.two_sessions()
        self.hook("first", "SessionStart")
        self.run_board("second", "send", "--to", "1", "Rebase before you push")
        self.age_cache()
        (self.board / ".github-messages.json").unlink(missing_ok=True)
        prompt = self.hook("first", "UserPromptSubmit")
        self.assertIn("Messages for this session", prompt)
        self.assertIn("Rebase before you push", prompt)
        self.assertNotIn("Rebase before you push", self.hook("first", "UserPromptSubmit"))
        self.assertNotIn("Rebase before you push", self.hook("second", "UserPromptSubmit"))

    def test_a_comment_written_on_github_is_the_users(self):
        self.two_sessions()
        self.run_board("second", "send", "--to", "1", "Shall I take ID 10?")
        state = self.state()
        issue = next(i for i in state["issues"] if "message" in i["labels"])
        issue["comments"].append({"id": "IC_user", "body": "Yes, go ahead.", "author": {"login": "someone"},
                                  "createdAt": "2026-11-19T18:00:00Z"})
        self.github.write_text(json.dumps(state))
        for session in ("first", "second"):
            _, inbox = self.run_board(session, "inbox")
            self.assertIn("from the user (someone on GitHub)", inbox)
            self.assertIn("Yes, go ahead.", inbox)

    def test_wait_returns_with_a_message_or_times_out(self):
        self.two_sessions()
        code, output = self.run_board("first", "wait", "--timeout", "0.3s")
        self.assertEqual(code, 1)
        self.assertIn("No new message within 0.3s", output)
        self.run_board("second", "send", "--to", "1", "Pushed")
        code, output = self.run_board("first", "wait", "--timeout", "5s")
        self.assertEqual(code, 0)
        self.assertIn("Pushed", output)

    def test_close_ends_a_conversation_and_forgotten_ones_close(self):
        self.two_sessions()
        self.run_board("second", "send", "--to", "1", "Done?")
        code, _ = self.run_board("first", "close", "3", "Settled")
        self.assertEqual(code, 0)
        [closed] = self.conversations("closed")
        self.assertIn("Settled", closed["comments"][-1]["body"])
        self.assertEqual(self.run_board("first", "reply", "3", "More")[0], 1)
        self.run_board("second", "send", "--to", "1", "A new topic")
        state = self.state()
        issue = next(i for i in state["issues"] if "message" in i["labels"] and i["state"] == "open")
        for comment in issue["comments"]:
            comment["body"] = comment["body"].replace('"sent": ', '"sent": -')
        issue["body"] = issue["body"].replace('"started": ', '"started": -')
        self.github.write_text(json.dumps(state))
        self.run_board("first", "inbox")
        self.assertEqual(self.conversations(), [])
        self.assertEqual(self.conversations("closed")[-1]["reason"], "not planned")

    def test_messages_need_github_and_a_known_recipient(self):
        self.two_sessions()
        code, output = self.run_board("second", "send", "--to", "99", "Hello")
        self.assertEqual(code, 1)
        self.assertIn("No session on the board is 99", output)
        self.offline = True
        code, output = self.run_board("second", "send", "--to", "1", "Hello")
        self.assertEqual(code, 1)
        self.assertIn("GitHub unreachable", output)
        self.offline = False
        self.assertEqual(self.conversations(), [])

    def test_hook_never_fails_a_prompt(self):
        code, output = self.run_board(None, "hook", stdin="not json")
        self.assertEqual(code, 0)
        self.assertIn("Session board unavailable", output)


if __name__ == "__main__":
    unittest.main()
