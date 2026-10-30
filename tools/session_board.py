#!/usr/bin/env python3
"""The session board: what each Claude or Codex session is working on now.

Each session works in its own git worktree and claims its task here before
starting: the task, the areas it touches (files, folders or systems) and the
shared version numbers it will raise (campaign format, rules version). An
overlap with another session's claim is a warning, never a block.

The board is one JSON file per session in .sessions/ at the main checkout's
root, which every worktree finds through git's common directory; .gitignore
keeps it out of commits. Uses only Python's standard library.

    tools/session_board.py list
    tools/session_board.py claim --task "Wand of Secrets traps" --issue 4 \\
        --areas "src/OpenGold.Core/src/phlan_session.cpp, Kovel Mansion" \\
        --versions "campaign format"
    tools/session_board.py update --areas "..."
    tools/session_board.py release

Claude Code runs `hook` at session start and on each prompt
(.claude/settings.json): it shows the other sessions' claims when they change
and keeps this session's claim fresh.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

# A claim idle this long is shown as stale; one idle much longer is a closed
# session's leftover and is removed.
STALE_SECONDS = 4 * 3600
FORGOTTEN_SECONDS = 3 * 24 * 3600


def git(*args, cwd=None):
    result = subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True)
    return result.stdout.strip() if result.returncode == 0 else ""


def board_folder(cwd=None):
    """.sessions/ beside the repository's common git directory, shared by
    every worktree. OPENGOLD_SESSION_BOARD overrides it for tests."""
    if os.environ.get("OPENGOLD_SESSION_BOARD"):
        return Path(os.environ["OPENGOLD_SESSION_BOARD"])
    common = git("rev-parse", "--path-format=absolute", "--git-common-dir", cwd=cwd)
    if not common:
        return None
    return Path(common).parent / ".sessions"


def own_id(explicit=None, worktree=""):
    """A Claude session is known by its session id; anything else by the
    worktree it works in."""
    if explicit:
        return explicit
    if os.environ.get("CLAUDE_CODE_SESSION_ID"):
        return os.environ["CLAUDE_CODE_SESSION_ID"]
    return "worktree-" + Path(worktree or git("rev-parse", "--show-toplevel") or os.getcwd()).name


def safe_name(session_id):
    return "".join(c if c.isalnum() or c in "-_." else "_" for c in session_id) + ".json"


def split_list(text):
    return [part.strip() for part in (text or "").split(",") if part.strip()]


def read_claims(folder, now=None):
    """Every claim on the board, oldest first. Forgotten claims are removed."""
    now = now or time.time()
    claims = []
    if not folder or not folder.is_dir():
        return claims
    for path in sorted(folder.glob("*.json")):
        try:
            claim = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            continue
        if now - claim.get("updated", 0) > FORGOTTEN_SECONDS:
            path.unlink(missing_ok=True)
            continue
        claims.append(claim)
    return sorted(claims, key=lambda claim: claim.get("started", 0))


def write_claim(folder, claim):
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / safe_name(claim["id"])
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(claim, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def idle_text(seconds):
    if seconds < 3600:
        return f"{int(seconds // 60)} min"
    return f"{seconds / 3600:.0f} h"


def describe(claim, now=None):
    now = now or time.time()
    idle = now - claim.get("updated", now)
    who = claim.get("name") or claim["id"][:8]
    where = Path(claim.get("worktree", "?")).name
    if claim.get("branch"):
        where += " @ " + claim["branch"]
    state = f"idle {idle_text(idle)}" + (", STALE" if idle > STALE_SECONDS else "")
    task = claim.get("task", "?")
    if claim.get("issue"):
        task += f" (#{claim['issue']})"
    line = f"- {who} ({claim.get('agent', '?')}, {where}, {state}): {task}"
    if claim.get("areas"):
        line += "\n    areas: " + ", ".join(claim["areas"])
    if claim.get("versions"):
        line += "\n    raises: " + ", ".join(claim["versions"])
    return line


def touches(first, second):
    """Whether two areas overlap: the same name, or a path inside the other."""
    a, b = first.lower().rstrip("/"), second.lower().rstrip("/")
    return a == b or a.startswith(b + "/") or b.startswith(a + "/")


def overlaps(mine, other):
    """What two claims share: areas, version numbers or the worktree."""
    shared = [f"area {a}" for a in mine.get("areas", [])
              if any(touches(a, b) for b in other.get("areas", []))]
    versions = {v.lower() for v in other.get("versions", [])}
    shared += [f"version {v}" for v in mine.get("versions", []) if v.lower() in versions]
    if mine.get("worktree") and mine.get("worktree") == other.get("worktree"):
        shared.append("worktree " + mine["worktree"])
    return shared


def overlap_warnings(mine, claims):
    warnings = []
    for other in claims:
        if other["id"] == mine["id"]:
            continue
        shared = overlaps(mine, other)
        if shared:
            who = other.get("name") or other["id"][:8]
            warnings.append(f"WARNING: overlaps {who} ({other.get('task', '?')}): " + "; ".join(shared)
                            + ". Message that session or ask the user before going on.")
    return warnings


def board_text(claims, exclude=None, now=None):
    others = [claim for claim in claims if claim["id"] != exclude]
    if not others:
        return "Session board: no other session has a claim."
    return "Session board, other sessions' claims:\n" + "\n".join(describe(c, now) for c in others)


def claim_command(args):
    folder = board_folder()
    worktree = str(Path(args.worktree).resolve()) if args.worktree else git("rev-parse", "--show-toplevel")
    session_id = own_id(args.id, worktree)
    claims = read_claims(folder)
    existing = next((c for c in claims if c["id"] == session_id), None)
    if args.command == "update" and not existing:
        raise SystemExit("This session has no claim; use claim first.")
    now = time.time()
    if args.command == "claim":
        # A new claim replaces the old one but keeps the session's name.
        claim = {"id": session_id, "started": now, "worktree": worktree,
                 "branch": git("branch", "--show-current", cwd=worktree or None),
                 "agent": "claude" if os.environ.get("CLAUDE_CODE_SESSION_ID") else "other"}
        if existing and existing.get("name"):
            claim["name"] = existing["name"]
    else:
        claim = dict(existing)
        if args.worktree:
            claim["worktree"] = worktree
            claim["branch"] = git("branch", "--show-current", cwd=worktree)
    for field in ("task", "name", "issue", "agent"):
        if getattr(args, field):
            claim[field] = getattr(args, field)
    for field in ("areas", "versions"):
        if getattr(args, field) is not None:
            claim[field] = split_list(getattr(args, field))
    if not claim.get("task"):
        raise SystemExit("A claim needs --task.")
    claim["updated"] = now
    write_claim(folder, claim)
    print("Claimed:\n" + describe(claim, now))
    for warning in overlap_warnings(claim, claims):
        print(warning)


def release_command(args):
    folder = board_folder()
    session_id = own_id(args.id)
    path = folder / safe_name(session_id) if folder else None
    if not path or not path.exists():
        raise SystemExit("No claim to release for " + session_id + ".")
    path.unlink()
    print("Released " + session_id + ".")


def list_command(args):
    claims = read_claims(board_folder())
    if not claims:
        print("Session board: no claims.")
        return
    mine = own_id(args.id)
    for claim in claims:
        print(describe(claim) + ("  <- this session" if claim["id"] == mine else ""))


def hook_command(_args):
    """Claude Code's SessionStart and UserPromptSubmit hook. Prints only what
    the session should know, and never fails the prompt."""
    try:
        event = json.loads(sys.stdin.read() or "{}")
        folder = board_folder(event.get("cwd") or None)
        if not folder:
            return
        session_id = event.get("session_id") or own_id()
        claims = read_claims(folder)
        mine = next((c for c in claims if c["id"] == session_id), None)
        if mine:
            mine["updated"] = time.time()
            write_claim(folder, mine)
        others = [c for c in claims if c["id"] != session_id]
        # The board is shown when someone claims, changes or releases a task,
        # not on every prompt.
        signature = hashlib.sha256(json.dumps(
            [[c["id"], c.get("task"), c.get("areas"), c.get("versions"), c.get("worktree"),
              c.get("updated", 0) > time.time() - STALE_SECONDS] for c in others]).encode()).hexdigest()
        seen = folder / (".seen-" + safe_name(session_id))
        if event.get("hook_event_name") == "SessionStart" or not seen.exists() \
                or seen.read_text(encoding="utf-8") != signature:
            print(board_text(claims, session_id))
            for warning in overlap_warnings(mine, claims) if mine else []:
                print(warning)
            folder.mkdir(parents=True, exist_ok=True)
            seen.write_text(signature, encoding="utf-8")
        if not mine:
            print("This session has no claim on the session board. Before starting work, run "
                  "tools/session_board.py claim --task ... --areas ... from its worktree (CLAUDE.md §9).")
    except Exception as error:  # A broken board must never stop a prompt.
        print(f"Session board unavailable: {error}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("claim", "update"):
        command = commands.add_parser(name)
        command.add_argument("--task", help="what the session is doing, in a few words")
        command.add_argument("--areas", help="comma-separated files, folders or systems it touches")
        command.add_argument("--versions", help="comma-separated shared numbers it raises")
        command.add_argument("--issue", help="the GitHub issue number, if any")
        command.add_argument("--name", help="the session's name, as ListAgents shows it")
        command.add_argument("--agent", help="claude or codex")
        command.add_argument("--worktree", help="the session's worktree (default: the current one)")
        command.add_argument("--id", help=argparse.SUPPRESS)
        command.set_defaults(run=claim_command)
    for name, run in (("release", release_command), ("list", list_command), ("hook", hook_command)):
        command = commands.add_parser(name)
        command.add_argument("--id", help="another session's id, to release a stale claim")
        command.set_defaults(run=run)
    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()
