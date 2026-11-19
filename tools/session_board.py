#!/usr/bin/env python3
"""The session board: what each Claude or Codex session, on any machine, is
working on now.

Each session works in its own git worktree and claims its task before
starting: the task, the areas it touches (files, folders or systems) and the
shared version numbers it will raise (campaign format, rules version). An
overlap with another session's claim is a warning, never a block.

A claim is an open GitHub issue with the label `claim` in the repository's
origin, kept through the GitHub CLI (`gh`, logged in with `gh auth login`).
Its body shows the claim to people and carries it for this tool in a hidden
comment; releasing the claim closes the issue. When GitHub cannot be reached,
a claim, update or release waits in .sessions/ at the main checkout's root
(ignored by git) and goes to GitHub the next time a command or hook reaches it.

    tools/session_board.py list
    tools/session_board.py claim --task "Wand of Secrets traps" --issue 4 \\
        --areas "src/OpenGold.Core/src/phlan_session.cpp, Kovel Mansion" \\
        --versions "campaign format"
    tools/session_board.py update --areas "..."
    tools/session_board.py update                  (marks the claim active)
    tools/session_board.py release
    tools/session_board.py release --claim 123     (another session's stale claim)

Sessions also talk to each other, Claude and Codex alike. A conversation is an
open GitHub issue with the label `message`, and each message is a comment on
it; closing the conversation closes the issue. A session is addressed by its
claim's issue number or its name; the user can read and join on GitHub.

    tools/session_board.py send --to 287 "May I take GoldBoxGame ID 10?"
    tools/session_board.py reply 301 "Yes; 9 stays mine."
    tools/session_board.py inbox                   (unread messages, then the open conversations)
    tools/session_board.py inbox --conversation 301
    tools/session_board.py wait --timeout 10m      (until a message arrives)
    tools/session_board.py close 301

Claude Code runs `hook` at session start and on each prompt
(.claude/settings.json): it shows the other sessions' claims when they change,
shows new messages for this session and keeps its claim fresh. Uses only
Python's standard library and gh.
"""
import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import time

LABEL = "claim"
# A claim idle this long is shown as stale; one idle much longer is a closed
# session's leftover, and the next session to read the board closes it.
STALE_SECONDS = 4 * 3600
FORGOTTEN_SECONDS = 3 * 24 * 3600
# The hook asks GitHub at most this often; commands always ask.
FETCH_SECONDS = 120
# The hook marks its own claim active at most this often.
HEARTBEAT_SECONDS = 30 * 60
DATA_START = "<!-- session-board"
DATA_END = "-->"
MESSAGE_LABEL = "message"
CONVERSATION_START = "<!-- session-conversation"
MESSAGE_START = "<!-- session-message"
# How often `wait` asks GitHub for a new message.
WAIT_POLL_SECONDS = 30


class GitHubUnavailable(Exception):
    """gh is missing, not logged in, or GitHub cannot be reached."""


def git(*args, cwd=None):
    result = subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True)
    return result.stdout.strip() if result.returncode == 0 else ""


def gh(*args, cwd=None):
    """Runs the GitHub CLI. OPENGOLD_GH names another program for tests."""
    program = os.environ.get("OPENGOLD_GH", "gh")
    command = [sys.executable, program] if program.endswith(".py") else [program]
    try:
        result = subprocess.run([*command, *args], cwd=cwd, capture_output=True, text=True, timeout=20)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise GitHubUnavailable(str(error)) from error
    if result.returncode != 0:
        raise GitHubUnavailable((result.stderr or result.stdout).strip() or "gh failed")
    return result.stdout


def board_folder(cwd=None):
    """.sessions/ beside the repository's common git directory, shared by
    every worktree: the claims waiting for GitHub and the hook's cache.
    OPENGOLD_SESSION_BOARD overrides it for tests."""
    if os.environ.get("OPENGOLD_SESSION_BOARD"):
        return Path(os.environ["OPENGOLD_SESSION_BOARD"])
    common = git("rev-parse", "--path-format=absolute", "--git-common-dir", cwd=cwd)
    if not common:
        return None
    return Path(common).parent / ".sessions"


def own_id(explicit=None, worktree=""):
    """A Claude session is known by its session id; anything else by its
    machine and the worktree it works in."""
    if explicit:
        return explicit
    if os.environ.get("CLAUDE_CODE_SESSION_ID"):
        return os.environ["CLAUDE_CODE_SESSION_ID"]
    folder = Path(worktree or git("rev-parse", "--show-toplevel") or os.getcwd()).name
    return f"worktree-{platform.node()}-{folder}"


def safe_name(session_id):
    return "".join(c if c.isalnum() or c in "-_." else "_" for c in session_id) + ".json"


def split_list(text):
    return [part.strip() for part in (text or "").split(",") if part.strip()]


def last_part(path):
    """A path's last part, whichever machine wrote it."""
    return re.split(r"[\\/]", path.rstrip("\\/"))[-1] if path else "?"


def load_json(path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None


def save_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(value, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)


# The claim's issue

def claim_title(claim):
    return f"Claim: {claim['task']} ({claim.get('name') or claim['id'][:8]})"


def claim_body(claim):
    when = time.strftime("%Y-%m-%d %H:%M UTC", time.gmtime(claim["updated"]))
    lines = [f"**Task:** {claim['task']}",
             f"**Session:** {claim.get('name') or claim['id']} ({claim.get('agent', '?')} on {claim.get('host', '?')})",
             f"**Worktree:** {last_part(claim.get('worktree', ''))}"
             + (f" @ {claim['branch']}" if claim.get("branch") else "")]
    if claim.get("issue"):
        lines.append(f"**Work issue:** #{str(claim['issue']).lstrip('#')}")
    if claim.get("areas"):
        lines.append("**Areas:** " + ", ".join(claim["areas"]))
    if claim.get("versions"):
        lines.append("**Raises:** " + ", ".join(claim["versions"]))
    lines.append(f"**Last active:** {when}")
    stored = {key: value for key, value in claim.items() if key not in ("number", "pending")}
    note = "Kept by tools/session_board.py; release the claim to close this issue."
    return "\n".join(lines) + f"\n\n_{note}_\n\n" + hidden_data(DATA_START, stored)


def hidden_data(start, value):
    # No ">" inside the comment, so a task's or message's words cannot end it.
    data = json.dumps(value, sort_keys=True).replace(">", "\\u003e")
    return f"{start}\n{data}\n{DATA_END}\n"


def read_hidden_data(body, start_marker):
    """The dictionary a body carries after start_marker, or None."""
    body = body or ""
    start = body.rfind(start_marker + "\n")
    end = body.find("\n" + DATA_END, start)
    if start < 0 or end < 0:
        return None
    try:
        value = json.loads(body[start + len(start_marker) + 1:end])
    except ValueError:
        return None
    return value if isinstance(value, dict) else None


def claim_from_issue(issue):
    claim = read_hidden_data(issue.get("body"), DATA_START)
    if not claim or "id" not in claim:
        return None
    claim["number"] = issue["number"]
    return claim


def fetch_claims(cwd):
    output = gh("issue", "list", "--label", LABEL, "--state", "open", "--limit", "500",
                "--json", "number,body", cwd=cwd)
    return [claim for claim in map(claim_from_issue, json.loads(output or "[]")) if claim]


def publish(claim, cwd):
    """Writes the claim to its issue, opening one if it has none. Returns its number."""
    title, body = claim_title(claim), claim_body(claim)
    if claim.get("number"):
        gh("issue", "edit", str(claim["number"]), "--title", title, "--body", body, cwd=cwd)
        return claim["number"]
    url = gh("issue", "create", "--title", title, "--body", body, "--label", LABEL, cwd=cwd).strip()
    return int(url.rstrip("/").split("/")[-1])


def close_issue(number, cwd, reason="completed", comment=None):
    args = ["issue", "close", str(number), "--reason", reason]
    if comment:
        args += ["--comment", comment]
    gh(*args, cwd=cwd)


# Claims waiting in .sessions/ for GitHub

def pending_path(folder, session_id):
    return folder / ("pending-" + safe_name(session_id))


def pending_records(folder):
    if not folder or not folder.is_dir():
        return []
    return [record for record in map(load_json, sorted(folder.glob("pending-*.json"))) if record]


def keep_for_later(folder, action, claim):
    save_json(pending_path(folder, claim["id"]), {"action": action, "claim": claim})


def send_pending(folder, github_claims, cwd):
    """Sends every waiting claim and release to GitHub. True if any was sent."""
    sent = False
    by_id = {claim["id"]: claim for claim in github_claims}
    for record in pending_records(folder):
        claim = record["claim"]
        on_github = by_id.get(claim["id"])
        if record["action"] == "release":
            number = claim.get("number") or (on_github or {}).get("number")
            if number:
                close_issue(number, cwd)
        else:
            if on_github and claim.get("number") is None:
                # The session claimed anew while offline: its old claim ends.
                close_issue(on_github["number"], cwd)
            claim.pop("pending", None)
            publish(claim, cwd)
        pending_path(folder, claim["id"]).unlink(missing_ok=True)
        sent = True
    return sent


def with_pending(claims, folder):
    """GitHub's claims as the waiting claims and releases will leave them."""
    claims = {claim["id"]: claim for claim in claims}
    for record in pending_records(folder):
        claim = record["claim"]
        if record["action"] == "release":
            claims.pop(claim["id"], None)
        else:
            claims[claim["id"]] = dict(claim, pending=True)
    return sorted(claims.values(), key=lambda claim: claim.get("started", 0))


def close_duplicates(claims, cwd):
    """GitHub's issue list can lag a new issue by a few seconds, so a session
    can end up with two open claims: the newest stays, the others close."""
    newest = {}
    for claim in sorted(claims, key=lambda claim: claim.get("started", 0)):
        older = newest.get(claim["id"])
        if older:
            close_issue(older["number"], cwd)
        newest[claim["id"]] = claim
    return list(newest.values())


def close_forgotten(claims, cwd, now):
    kept = []
    for claim in claims:
        if now - claim.get("updated", 0) > FORGOTTEN_SECONDS:
            close_issue(claim["number"], cwd, "not planned",
                        "Closed by tools/session_board.py: the claim was idle for over 3 days.")
        else:
            kept.append(claim)
    return kept


def cache_path(folder):
    return folder / ".github-claims.json"


def forget_cache(folder):
    cache_path(folder).unlink(missing_ok=True)


def read_claims(folder, cwd=None, fresh=True, now=None):
    """Every claim, oldest first, and a note when GitHub could not be reached.
    Waiting claims go to GitHub first; forgotten claims are closed."""
    now = now or time.time()
    cache = load_json(cache_path(folder)) or {}
    if not fresh and now - max(cache.get("fetched", 0), cache.get("tried", 0)) <= FETCH_SECONDS:
        return with_pending(cache.get("claims", []), folder), ""
    try:
        claims = fetch_claims(cwd)
        if send_pending(folder, claims, cwd):
            claims = fetch_claims(cwd)
        claims = close_forgotten(close_duplicates(claims, cwd), cwd, now)
    except GitHubUnavailable as error:
        # The hook tries again only after FETCH_SECONDS, so prompts stay quick offline.
        save_json(cache_path(folder), dict(cache, tried=now))
        when = time.strftime("%H:%M UTC", time.gmtime(cache["fetched"])) if cache.get("fetched") else "never"
        return with_pending(cache.get("claims", []), folder), \
            f"GitHub unreachable ({error}); showing its claims as last read ({when}) and this machine's waiting ones."
    save_json(cache_path(folder), {"fetched": now, "claims": claims})
    return with_pending(claims, folder), ""


def save_claim(folder, claim, cwd):
    """Publishes the claim, or keeps it for later when GitHub is unreachable.
    Returns a note when it was kept."""
    try:
        claim["number"] = publish(claim, cwd)
    except GitHubUnavailable as error:
        keep_for_later(folder, "save", claim)
        return f"GitHub unreachable ({error}); the claim waits on this machine and goes to GitHub later."
    finally:
        forget_cache(folder)
    pending_path(folder, claim["id"]).unlink(missing_ok=True)
    return ""


# Conversations between sessions

def session_label(session_id, claim=None):
    """How a session is named in conversations: its claim's name, or what it
    is and where it works."""
    if not claim:
        return session_id
    return claim.get("name") or (f"{claim.get('agent', '?')} on {claim.get('host', '?')}, "
                                 f"{last_part(claim.get('worktree', ''))}")


def this_session(args, claims):
    session_id = own_id(args.id)
    claim = next((c for c in claims if c["id"] == session_id), None)
    if claim:
        return {"id": session_id, "label": session_label(session_id, claim)}
    agent = args.agent or ("claude" if os.environ.get("CLAUDE_CODE_SESSION_ID") else "other")
    folder = last_part(git("rev-parse", "--show-toplevel") or os.getcwd())
    return {"id": session_id, "label": f"{agent} on {platform.node()}, {folder}"}


def find_session(text, claims):
    """The session that `text` names: a claim's issue number, a claim's name,
    or a session id."""
    wanted = str(text).strip().lstrip("#")
    for claim in claims:
        if (wanted.isdigit() and claim.get("number") == int(wanted)) \
                or wanted.lower() in ((claim.get("name") or "").lower(), claim["id"].lower()):
            return {"id": claim["id"], "label": session_label(claim["id"], claim)}
    if wanted.isdigit() or " " in wanted:
        raise SystemExit(f"No session on the board is {text}; tools/session_board.py list shows them.")
    return {"id": wanted, "label": wanted}


def github_time(text):
    try:
        return datetime.fromisoformat(text.replace("Z", "+00:00")).timestamp()
    except (AttributeError, ValueError):
        return 0


def message_from_comment(comment):
    """A comment as a message. A comment made on GitHub without the tool is
    the user's."""
    body = comment.get("body") or ""
    data = read_hidden_data(body, MESSAGE_START)
    if not data:
        login = (comment.get("author") or {}).get("login", "")
        return {"id": comment.get("id"), "from": "user", "label": f"the user ({login} on GitHub)",
                "sent": github_time(comment.get("createdAt")), "text": body.strip()}
    text = body[:body.rfind(MESSAGE_START)].split("\n\n", 1)[-1].strip()
    return {"id": comment.get("id"), "from": data.get("from"), "label": data.get("label", "?"),
            "sent": data.get("sent", 0), "text": text}


def conversation_from_issue(issue):
    data = read_hidden_data(issue.get("body"), CONVERSATION_START)
    if not data or not data.get("participants"):
        return None
    data["number"] = issue["number"]
    data["messages"] = [message_from_comment(comment) for comment in issue.get("comments") or []]
    return data


def fetch_conversations(cwd):
    output = gh("issue", "list", "--label", MESSAGE_LABEL, "--state", "open", "--limit", "200",
                "--json", "number,body,comments", cwd=cwd)
    return [c for c in map(conversation_from_issue, json.loads(output or "[]")) if c]


def last_activity(conversation):
    return max([conversation.get("started", 0)] + [m["sent"] for m in conversation["messages"]])


def close_forgotten_conversations(conversations, cwd, now):
    kept = []
    for conversation in conversations:
        if now - last_activity(conversation) > FORGOTTEN_SECONDS:
            close_issue(conversation["number"], cwd, "not planned",
                        "Closed by tools/session_board.py: no message for over 3 days.")
        else:
            kept.append(conversation)
    return kept


def messages_cache_path(folder):
    return folder / ".github-messages.json"


def read_conversations(folder, cwd=None, fresh=True, now=None):
    """The open conversations; the hook asks GitHub at most every FETCH_SECONDS."""
    now = now or time.time()
    cache = load_json(messages_cache_path(folder)) or {}
    if not fresh and now - max(cache.get("fetched", 0), cache.get("tried", 0)) <= FETCH_SECONDS:
        return cache.get("conversations", [])
    try:
        conversations = close_forgotten_conversations(fetch_conversations(cwd), cwd, now)
    except GitHubUnavailable:
        if not fresh:
            save_json(messages_cache_path(folder), dict(cache, tried=now))
            return cache.get("conversations", [])
        raise
    save_json(messages_cache_path(folder), {"fetched": now, "conversations": conversations})
    return conversations


def takes_part(conversation, session_id):
    return any(p["id"] == session_id for p in conversation["participants"])


def read_path(folder, session_id):
    return folder / (".read-" + safe_name(session_id))


def unread_messages(conversations, session_id, folder):
    """(conversation, message) pairs this session has not yet seen."""
    seen = set((load_json(read_path(folder, session_id)) or {}).get("read", []))
    return [(conversation, message) for conversation in conversations if takes_part(conversation, session_id)
            for message in conversation["messages"]
            if message["from"] != session_id and message["id"] not in seen]


def mark_read(folder, session_id, conversations, messages):
    """Remembers the messages as seen, keeping only ids still in open conversations."""
    current = {m["id"] for c in conversations for m in c["messages"]}
    seen = set((load_json(read_path(folder, session_id)) or {}).get("read", []))
    seen = (seen | {m["id"] for m in messages}) & current
    save_json(read_path(folder, session_id), {"read": sorted(seen)})


def when_text(seconds):
    return time.strftime("%Y-%m-%d %H:%M UTC", time.gmtime(seconds))


def message_text(conversation, message):
    text = "\n".join("    " + line for line in message["text"].splitlines()) or "    (empty)"
    return (f"- #{conversation['number']} \"{conversation.get('subject', '?')}\" from {message['label']} "
            f"({when_text(message['sent'])}):\n{text}")


def unread_text(unread):
    return ("Messages for this session (answer with tools/session_board.py reply <number> \"...\"):\n"
            + "\n".join(message_text(c, m) for c, m in unread))


def conversation_title(subject, participants):
    return f"Message: {subject} ({' and '.join(p['label'] for p in participants)})"


def conversation_body(conversation):
    lines = [f"**Subject:** {conversation['subject']}",
             "**Between:** " + "; ".join(f"{p['label']} (`{p['id']}`)" for p in conversation["participants"]),
             f"**Started:** {when_text(conversation['started'])}"]
    note = ("A conversation between sessions, kept by tools/session_board.py. Each message is a comment; "
            "a comment written here on GitHub reaches the sessions as the user's.")
    stored = {key: conversation[key] for key in ("subject", "participants", "started")}
    return "\n".join(lines) + f"\n\n_{note}_\n\n" + hidden_data(CONVERSATION_START, stored)


def post_message(number, sender, recipients, text, cwd=None):
    header = f"**{sender['label']}** to {', '.join(r['label'] for r in recipients) or 'the conversation'}:"
    data = {"from": sender["id"], "label": sender["label"], "sent": time.time()}
    gh("issue", "comment", str(number), "--body", f"{header}\n\n{text.strip()}\n\n"
       + hidden_data(MESSAGE_START, data), cwd=cwd)


def open_conversation(subject, participants, cwd=None):
    conversation = {"subject": subject, "participants": participants, "started": time.time()}
    url = gh("issue", "create", "--title", conversation_title(subject, participants),
             "--body", conversation_body(conversation), "--label", MESSAGE_LABEL, cwd=cwd).strip()
    return int(url.rstrip("/").split("/")[-1])


def forget_messages_cache(folder):
    messages_cache_path(folder).unlink(missing_ok=True)


# Showing the board

def idle_text(seconds):
    if seconds < 3600:
        return f"{int(seconds // 60)} min"
    return f"{seconds / 3600:.0f} h"


def describe(claim, now=None):
    now = now or time.time()
    idle = now - claim.get("updated", now)
    who = claim.get("name") or claim["id"][:8]
    where = last_part(claim.get("worktree", ""))
    if claim.get("branch"):
        where += " @ " + claim["branch"]
    state = f"idle {idle_text(idle)}" + (", STALE" if idle > STALE_SECONDS else "")
    task = claim.get("task", "?")
    if claim.get("issue"):
        task += f" (#{str(claim['issue']).lstrip('#')})"
    place = " [not yet on GitHub]" if claim.get("pending") else (
        f" [claim #{claim['number']}]" if claim.get("number") else "")
    host = f" on {claim['host']}" if claim.get("host") else ""
    line = f"- {who} ({claim.get('agent', '?')}{host}, {where}, {state}){place}: {task}"
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
    same_machine = mine.get("host") == other.get("host")
    if same_machine and mine.get("worktree") and mine.get("worktree") == other.get("worktree"):
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


# Commands

def claim_command(args):
    folder = board_folder()
    worktree = str(Path(args.worktree).resolve()) if args.worktree else git("rev-parse", "--show-toplevel")
    session_id = own_id(args.id, worktree)
    claims, note = read_claims(folder)
    if note:
        print(note)
    existing = next((c for c in claims if c["id"] == session_id), None)
    if args.command == "update" and not existing:
        raise SystemExit("This session has no claim; use claim first.")
    now = time.time()
    if args.command == "claim":
        # A new claim is a new issue; it keeps the session's name.
        claim = {"id": session_id, "started": now, "worktree": worktree, "host": platform.node(),
                 "branch": git("branch", "--show-current", cwd=worktree or None),
                 "agent": "claude" if os.environ.get("CLAUDE_CODE_SESSION_ID") else "other"}
        if existing and existing.get("name"):
            claim["name"] = existing["name"]
    else:
        claim = {key: value for key, value in existing.items() if key != "pending"}
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
    if args.command == "claim" and existing and existing.get("number"):
        try:
            close_issue(existing["number"], None)
        except GitHubUnavailable:
            pass  # The waiting claim closes it when it reaches GitHub.
    note = save_claim(folder, claim, None)
    print(("Claimed" if args.command == "claim" else "Updated") + ":\n" + describe(claim, now))
    if note:
        print(note)
    for warning in overlap_warnings(claim, claims):
        print(warning)


def release_command(args):
    folder = board_folder()
    claims, note = read_claims(folder)
    if args.claim:
        number = int(str(args.claim).lstrip("#"))
        claim = next((c for c in claims if c.get("number") == number), None)
        target = f"claim #{number}"
    else:
        session_id = own_id(args.id)
        claim = next((c for c in claims if c["id"] == session_id), None)
        target = session_id
    if not claim:
        raise SystemExit(("" if not note else note + "\n") + "No claim to release for " + target + ".")
    if not claim.get("number"):
        pending_path(folder, claim["id"]).unlink(missing_ok=True)
    else:
        try:
            close_issue(claim["number"], None)
            pending_path(folder, claim["id"]).unlink(missing_ok=True)
        except GitHubUnavailable as error:
            keep_for_later(folder, "release", claim)
            print(f"GitHub unreachable ({error}); the release waits on this machine and goes to GitHub later.")
    forget_cache(folder)
    print("Released " + target + ".")


def list_command(args):
    claims, note = read_claims(board_folder())
    if note:
        print(note)
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
        cwd = event.get("cwd") or None
        folder = board_folder(cwd)
        if not folder:
            return
        session_id = event.get("session_id") or own_id()
        claims, note = read_claims(folder, cwd, fresh=False)
        mine = next((c for c in claims if c["id"] == session_id), None)
        if mine and not mine.get("pending") and time.time() - mine.get("updated", 0) > HEARTBEAT_SECONDS:
            mine["updated"] = time.time()
            save_claim(folder, mine, cwd)
        others = [c for c in claims if c["id"] != session_id]
        # The board is shown when someone claims, changes or releases a task,
        # not on every prompt.
        signature = hashlib.sha256(json.dumps(
            [[c["id"], c.get("task"), c.get("areas"), c.get("versions"), c.get("worktree"),
              c.get("updated", 0) > time.time() - STALE_SECONDS] for c in others]).encode()).hexdigest()
        seen = folder / (".seen-" + safe_name(session_id))
        if event.get("hook_event_name") == "SessionStart" or not seen.exists() \
                or seen.read_text(encoding="utf-8") != signature:
            if note:
                print(note)
            print(board_text(claims, session_id))
            for warning in overlap_warnings(mine, claims) if mine else []:
                print(warning)
            folder.mkdir(parents=True, exist_ok=True)
            seen.write_text(signature, encoding="utf-8")
        if not mine:
            print("This session has no claim on the session board. Before starting work, run "
                  "tools/session_board.py claim --task ... --areas ... from its worktree (CLAUDE.md §9).")
        conversations = read_conversations(folder, cwd, fresh=False)
        unread = unread_messages(conversations, session_id, folder)
        if unread:
            print(unread_text(unread))
            mark_read(folder, session_id, conversations, [message for _, message in unread])
    except Exception as error:  # A broken board must never stop a prompt.
        print(f"Session board unavailable: {error}")


def reachable(function):
    """Runs a message command, which needs GitHub: there is no one to talk to offline."""
    def run(args):
        try:
            function(args)
        except GitHubUnavailable as error:
            raise SystemExit(f"GitHub unreachable ({error}); nothing was sent or read.")
    return run


def find_conversation(conversations, number):
    wanted = int(str(number).lstrip("#"))
    conversation = next((c for c in conversations if c["number"] == wanted), None)
    if not conversation:
        raise SystemExit(f"No open conversation #{wanted}; tools/session_board.py inbox lists yours.")
    return conversation


@reachable
def send_command(args):
    folder = board_folder()
    claims, _ = read_claims(folder)
    sender = this_session(args, claims)
    recipient = find_session(args.to, claims)
    if recipient["id"] == sender["id"]:
        raise SystemExit("That is this session.")
    conversations = read_conversations(folder)
    pair = {sender["id"], recipient["id"]}
    conversation = next((c for c in conversations
                         if {p["id"] for p in c["participants"]} == pair), None)
    if conversation:
        number = conversation["number"]
    else:
        subject = args.subject or " ".join(args.text.split())[:60]
        number = open_conversation(subject, [sender, recipient])
    post_message(number, sender, [recipient], args.text)
    forget_messages_cache(folder)
    print(f"Sent to {recipient['label']} in conversation #{number}.")


@reachable
def reply_command(args):
    folder = board_folder()
    claims, _ = read_claims(folder)
    sender = this_session(args, claims)
    conversations = read_conversations(folder)
    conversation = find_conversation(conversations, args.conversation)
    if not takes_part(conversation, sender["id"]):
        # A session that joins is added, so it hears the replies.
        conversation["participants"].append(sender)
        gh("issue", "edit", str(conversation["number"]), "--body", conversation_body(conversation))
    others = [p for p in conversation["participants"] if p["id"] != sender["id"]]
    post_message(conversation["number"], sender, others, args.text)
    mark_read(folder, sender["id"], conversations, conversation["messages"])
    forget_messages_cache(folder)
    print(f"Replied in conversation #{conversation['number']}.")


def show_conversation(conversation):
    print(f"#{conversation['number']} \"{conversation['subject']}\", between "
          + "; ".join(p["label"] for p in conversation["participants"]) + ":")
    for message in conversation["messages"]:
        print(f"- {message['label']} ({when_text(message['sent'])}):")
        print("\n".join("    " + line for line in message["text"].splitlines()) or "    (empty)")


@reachable
def inbox_command(args):
    folder = board_folder()
    session_id = own_id(args.id)
    conversations = read_conversations(folder)
    if args.conversation:
        conversation = find_conversation(conversations, args.conversation)
        show_conversation(conversation)
        mark_read(folder, session_id, conversations, conversation["messages"])
        return
    unread = unread_messages(conversations, session_id, folder)
    print(unread_text(unread) if unread else "No new messages.")
    mark_read(folder, session_id, conversations, [message for _, message in unread])
    mine = [c for c in conversations if takes_part(c, session_id)]
    if mine:
        print("Open conversations:")
    for conversation in mine:
        others = [p["label"] for p in conversation["participants"] if p["id"] != session_id]
        print(f"- #{conversation['number']} \"{conversation['subject']}\" with {', '.join(others)}, "
              f"last message {when_text(last_activity(conversation))}")


def duration_seconds(text):
    match = re.fullmatch(r"\s*(\d+(?:\.\d+)?)\s*([smh]?)\s*", text or "")
    if not match:
        raise SystemExit(f"Not a duration: {text} (try 90s, 10m or 1h).")
    return float(match.group(1)) * {"": 1, "s": 1, "m": 60, "h": 3600}[match.group(2)]


@reachable
def wait_command(args):
    folder = board_folder()
    session_id = own_id(args.id)
    poll = float(os.environ.get("OPENGOLD_MESSAGE_POLL", WAIT_POLL_SECONDS))
    deadline = time.time() + duration_seconds(args.timeout)
    while True:
        conversations = read_conversations(folder)
        unread = [(c, m) for c, m in unread_messages(conversations, session_id, folder)
                  if not args.conversation or c["number"] == int(str(args.conversation).lstrip("#"))]
        if unread:
            print(unread_text(unread))
            mark_read(folder, session_id, conversations, [message for _, message in unread])
            return
        if time.time() + poll > deadline:
            raise SystemExit(f"No new message within {args.timeout}.")
        time.sleep(poll)


@reachable
def close_command(args):
    folder = board_folder()
    claims, _ = read_claims(folder)
    sender = this_session(args, claims)
    conversation = find_conversation(read_conversations(folder), args.conversation)
    if args.text:
        others = [p for p in conversation["participants"] if p["id"] != sender["id"]]
        post_message(conversation["number"], sender, others, args.text)
    close_issue(conversation["number"], None)
    forget_messages_cache(folder)
    print(f"Closed conversation #{conversation['number']}.")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("claim", "update"):
        command = commands.add_parser(name)
        command.add_argument("--task", help="what the session is doing, in a few words")
        command.add_argument("--areas", help="comma-separated files, folders or systems it touches")
        command.add_argument("--versions", help="comma-separated shared numbers it raises")
        command.add_argument("--issue", help="the GitHub issue number of the work, if any")
        command.add_argument("--name", help="the session's name, as ListAgents shows it")
        command.add_argument("--agent", help="claude or codex")
        command.add_argument("--worktree", help="the session's worktree (default: the current one)")
        command.add_argument("--id", help=argparse.SUPPRESS)
        command.set_defaults(run=claim_command)
    for name, run in (("release", release_command), ("list", list_command), ("hook", hook_command)):
        command = commands.add_parser(name)
        command.add_argument("--id", help=argparse.SUPPRESS)
        if name == "release":
            command.add_argument("--claim", help="another session's claim issue number, to release a stale claim")
        command.set_defaults(run=run)
    send = commands.add_parser("send", help="message another session")
    send.add_argument("--to", required=True, help="the session: its claim's issue number, its name or its id")
    send.add_argument("--subject", help="the conversation's subject (default: the message's start)")
    send.add_argument("text")
    send.set_defaults(run=send_command)
    reply = commands.add_parser("reply", help="answer in a conversation")
    reply.add_argument("conversation", help="the conversation's issue number")
    reply.add_argument("text")
    reply.set_defaults(run=reply_command)
    inbox = commands.add_parser("inbox", help="new messages and open conversations")
    inbox.add_argument("--conversation", help="show this whole conversation")
    inbox.set_defaults(run=inbox_command)
    wait = commands.add_parser("wait", help="wait for a new message")
    wait.add_argument("--timeout", default="10m", help="how long to wait: 90s, 10m, 1h (default 10m)")
    wait.add_argument("--conversation", help="wait only for this conversation")
    wait.set_defaults(run=wait_command)
    close = commands.add_parser("close", help="end a settled conversation")
    close.add_argument("conversation", help="the conversation's issue number")
    close.add_argument("text", nargs="?", help="a last message")
    close.set_defaults(run=close_command)
    for command in (send, reply, inbox, wait, close):
        command.add_argument("--id", help=argparse.SUPPRESS)
        command.add_argument("--agent", help="claude or codex, for a session with no claim")
    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()
