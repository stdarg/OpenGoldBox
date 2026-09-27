"""Extract/merge OpenGoldBox gettext catalogs, or validate them with --check.

Uses only Python's standard library. C++ messages use N_("...") or
i18n::text/utf8/format/formatted/plural; scenes use normal text properties.
Known rules data tables and structured Message templates are also extracted.
Original game files and demos are never scanned or copied.
"""
import argparse
import ast
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
LOCALE = ROOT / "src/OpenGoldBox/godot/locale"
LITERAL = r'"(?:[^"\\]|\\.)*"'


def quoted(value):
    return json.dumps(value, ensure_ascii=False)


def extract():
    messages = {}

    def add(source, path, line, plural=""):
        if source and re.search(r"[A-Za-z]", source):
            key = ("", source)
            entry = messages.setdefault(key, {"plural": plural, "refs": set()})
            if plural:
                entry["plural"] = plural
            entry["refs"].add(f"{path.relative_to(ROOT).as_posix()}:{line}")

    paths = sorted((ROOT / "src/OpenGoldBox").glob("*.cpp"))
    paths += sorted((ROOT / "src/OpenGoldBox").glob("*.h"))
    paths += sorted((ROOT / "src/OpenGoldBox/godot/scenes").glob("*.tscn"))
    paths += [ROOT / "src/OpenGold.Rules.Srd5/src/character_rules.cpp", ROOT / "src/OpenGold.Rules.Srd5/src/srd5.cpp", ROOT / "src/OpenGold.Rules.Srd5/src/mastery_resolution_impl.h"]
    for path in paths:
        content = path.read_text(encoding="utf-8-sig")
        patterns = [rf'\b(?:N_|i18n::(?:text|utf8|format|formatted))\(\s*({LITERAL})']
        if path.suffix == ".tscn":
            patterns = [rf'^(?:text|title|tooltip_text|placeholder_text) = ({LITERAL})']
        for pattern in patterns:
            for match in re.finditer(pattern, content, re.M):
                add(ast.literal_eval(match[1]), path, content.count("\n", 0, match.start()) + 1)
        for match in re.finditer(rf'i18n::plural\(\s*({LITERAL})\s*,\s*({LITERAL})', content):
            add(ast.literal_eval(match[1]), path, content.count("\n", 0, match.start()) + 1, ast.literal_eval(match[2]))
        if path.name in ("character_rules.cpp", "srd5.cpp", "mastery_resolution_impl.h"):
            for match in re.finditer(LITERAL, content):
                source=ast.literal_eval(match[0])
                if re.search(r"\{\w+\}",source):
                    add(source,path,content.count("\n",0,match.start())+1)
            # Creation/advancement data labels and descriptions, never the IDs.
            for match in re.finditer(rf'\{{{LITERAL},\s*({LITERAL}),\s*(?:\d+,\s*)?({LITERAL})\s*[,}}]', content):
                if not re.match(r"[A-Z]", ast.literal_eval(match[1])):
                    continue
                for group in (1, 2):
                    add(ast.literal_eval(match[group]), path, content.count("\n", 0, match.start()) + 1)
            # Structured presentation messages (template followed by arguments).
            for match in re.finditer(rf'\{{\s*({LITERAL}),\s*\{{', content):
                source=ast.literal_eval(match[1])
                if " " in source or "{" in source:
                    add(source, path, content.count("\n", 0, match.start()) + 1)
    manifest=ROOT / "src/OpenGoldBox/godot/locale/dynamic_sources.json"
    if manifest.exists():
        for source in json.loads(manifest.read_text(encoding="utf-8")):
            add(source,manifest,1)
    return messages


def read_po(path):
    entries = {}
    if not path.exists():
        return entries
    entry, field = {}, None

    def finish():
        if "msgid" in entry:
            key = (entry.get("msgctxt", ""), entry["msgid"])
            if key in entries:
                raise ValueError(f"Duplicate message in {path}: {key}")
            entries[key] = dict(entry)

    for line in path.read_text(encoding="utf-8-sig").splitlines() + [""]:
        if not line.strip():
            finish()
            entry, field = {}, None
        elif line.startswith("#, ") and "fuzzy" in line:
            entry["fuzzy"] = True
        elif line.startswith("#"):
            continue
        elif line.startswith('"'):
            if field is None:
                raise ValueError(f"Unexpected continuation in {path}")
            entry[field] += ast.literal_eval(line)
        else:
            field, value = line.split(" ", 1)
            entry[field] = ast.literal_eval(value)
    return entries


def catalog(messages, language, previous):
    header = ("Project-Id-Version: OpenGoldBox\n" + f"Language: {language}\n" +
              "MIME-Version: 1.0\nContent-Type: text/plain; charset=UTF-8\nContent-Transfer-Encoding: 8bit\n" +
              "Plural-Forms: nplurals=2; plural=(n != 1);\n")
    lines = ["# OpenGoldBox translations. See docs/LOCALIZATION.md.", 'msgid ""', "msgstr " + quoted(header), ""]
    for key, info in sorted(messages.items()):
        context, source = key
        old = previous.get(key, {})
        lines += ["#: " + " ".join(sorted(info["refs"]))]
        if "{" in source:
            lines += ["#. Preserve {named} placeholders and any [BBCode] tags; values may be reordered."]
        if old.get("fuzzy"):
            lines += ["#, fuzzy"]
        if context:
            lines += ["msgctxt " + quoted(context)]
        lines += ["msgid " + quoted(source)]
        if info["plural"]:
            lines += ["msgid_plural " + quoted(info["plural"])]
            for i, value in enumerate((source, info["plural"])):
                lines += [f"msgstr[{i}] " + quoted(value if language == "en" else old.get(f"msgstr[{i}]", ""))]
        else:
            lines += ["msgstr " + quoted(source if language == "en" else old.get("msgstr", ""))]
        lines += [""]
    return "\n".join(lines)


def validate(messages, language):
    entries = read_po(LOCALE / f"{language}.po")
    errors = []
    for key, info in messages.items():
        entry = entries.get(key, {})
        sources = (key[1], info["plural"]) if info["plural"] else (key[1],)
        for i, source in enumerate(sources):
            value = entry.get(f"msgstr[{i}]" if info["plural"] else "msgstr", "")
            if not value or entry.get("fuzzy"):
                errors.append(f"{language}: missing translation: {source}")
                continue
            if sorted(re.findall(r'\{\w+\}', source)) != sorted(re.findall(r'\{\w+\}', value)):
                errors.append(f"{language}: placeholder mismatch: {source}")
            if sorted(re.findall(r'\[/?[a-z_]+(?:=[^\]]+)?\]', source)) != sorted(re.findall(r'\[/?[a-z_]+(?:=[^\]]+)?\]', value)):
                errors.append(f"{language}: BBCode mismatch: {source}")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    messages = extract()
    if args.check:
        expected = catalog(messages, "", {})
        errors = []
        if not (LOCALE / "messages.pot").exists() or (LOCALE / "messages.pot").read_text(encoding="utf-8") != expected:
            errors.append("messages.pot is stale; run tools/localization.py")
        for language in ("en", "es"):
            errors += validate(messages, language)
        if errors:
            raise SystemExit("\n".join(errors))
        print(f"Localization: {len(messages)} messages; English and Spanish complete; placeholders and BBCode valid.")
    else:
        LOCALE.mkdir(parents=True, exist_ok=True)
        (LOCALE / "messages.pot").write_text(catalog(messages, "", {}), encoding="utf-8")
        for language in ("en", "es"):
            path = LOCALE / f"{language}.po"
            path.write_text(catalog(messages, language, read_po(path)), encoding="utf-8")
        print(f"Updated {len(messages)} messages in {LOCALE}")


if __name__ == "__main__":
    main()
