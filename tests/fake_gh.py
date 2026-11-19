"""A stand-in for the GitHub CLI's issue commands, for session_board_tests.

Issues live in the JSON file FAKE_GH_STATE, and each call is logged there;
FAKE_GH_OFFLINE makes every call fail as gh does when GitHub is unreachable."""
import json
import os
from pathlib import Path
import sys


def option(args, name):
    return args[args.index(name) + 1] if name in args else None


def find(state, number):
    return next(issue for issue in state["issues"] if issue["number"] == int(number))


def run(args, state):
    if args[:2] == ["issue", "list"]:
        label, wanted = option(args, "--label"), option(args, "--state")
        print(json.dumps([{"number": issue["number"], "body": issue["body"]} for issue in state["issues"]
                          if label in issue["labels"] and issue["state"] == wanted]))
    elif args[:2] == ["issue", "create"]:
        number = state["next"]
        state["next"] += 1
        state["issues"].append({"number": number, "title": option(args, "--title"),
                                "body": option(args, "--body"), "labels": [option(args, "--label")],
                                "state": "open", "reason": None, "comments": []})
        print(f"https://github.com/example/repo/issues/{number}")
    elif args[:2] == ["issue", "edit"]:
        issue = find(state, args[2])
        issue["title"], issue["body"] = option(args, "--title"), option(args, "--body")
    elif args[:2] == ["issue", "close"]:
        issue = find(state, args[2])
        issue["state"], issue["reason"] = "closed", option(args, "--reason")
        if option(args, "--comment"):
            issue["comments"].append(option(args, "--comment"))
    else:
        print("unknown command: " + " ".join(args), file=sys.stderr)
        return 2
    return 0


def main():
    if os.environ.get("FAKE_GH_OFFLINE"):
        print("error connecting to api.github.com", file=sys.stderr)
        return 1
    path = Path(os.environ["FAKE_GH_STATE"])
    state = json.loads(path.read_text()) if path.exists() else {"next": 1, "issues": [], "calls": []}
    args = sys.argv[1:]
    state["calls"].append(args[:2])
    code = run(args, state)
    path.write_text(json.dumps(state))
    return code


if __name__ == "__main__":
    sys.exit(main())
