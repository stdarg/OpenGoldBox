# Translation and language support

OpenGoldBox uses Godot's TranslationServer and UTF-8 gettext PO catalogs.
English (`en`) and Spanish (`es`, displayed as **Español**) are supported.
The implementation belongs to the game in `src/OpenGoldBox`; the demos keep
their existing entry points and presentation.

## Choosing the language

From **cmd.exe**:

```bat
win-package\opengoldbox.exe --reset-lang
win-package\opengoldbox.exe --reset-lang --splash
```

`--reset-lang` opens a centered language dialog before the optional splash screens.
Select **English** or **Español** with the mouse or arrow keys, then press Enter
or **Continue** (shown as **Continuar** in Spanish). Highlighting a language
immediately translates the dialog title, instructions, button and any error into
that language. Language names remain in their native form. Previewing does not
save a preference; confirmation saves the choice and immediately applies it to
startup and the game. A missing language preference also opens this dialog,
before any game-folder selection or checksum validation. Otherwise startup uses the saved choice
or `OPENGOLD_LANG` override. A language value is not required after
the flag. Closing this dialog with X or Ctrl+X exits through the normal graceful
shutdown path. If the preference cannot be saved, the dialog displays
an error in the highlighted language and remains open.

Both the game path and language live in `settings.cfg` beside the executable.
See [CONFIGURATION.md](CONFIGURATION.md) for first-run setup, `--reset-game-path`,
environment overrides and MD5 compatibility warnings. `--lang` is removed.

`--splash` remains independent: without it, selection proceeds directly to
character creation; with it, both translated lettering overlays fade in using
the existing key-to-advance flow. From interactive CMD, run
`win-package\opengoldbox.console.exe` with the same flags to wait for the game
to exit.

## Files and workflow

| File | Purpose |
| --- | --- |
| `src/OpenGoldBox/godot/locale/messages.pot` | Generated source template and code/scene references |
| `src/OpenGoldBox/godot/locale/en.po` | English source wording |
| `src/OpenGoldBox/godot/locale/es.po` | Spanish interface and rules messages |
| `src/OpenGoldBox/godot/locale/dynamic_sources.json` | Source messages supplied dynamically by native APIs |
| `src/OpenGoldBox/godot/locale/campaign.es.po` | Separate campaign translation overrides; initially empty |
| `src/OpenGoldBox/localization.h` | Native presentation helpers |
| `src/OpenGold.Rules/include/opengold/message.h` | Engine-independent message template and argument values |
| `tools/localization.py` | Standard-library Python extraction, catalog merge and validation |

From **cmd.exe**, after the normal build has provisioned Python:

```bat
build\_deps\python\python.exe tools\localization.py
build\_deps\python\python.exe tools\localization.py --check
build-opengoldbox.cmd
```

Edit Spanish `msgstr` values in a text editor or a gettext editor. Preserve
`{named}` placeholders and BBCode tags; reorder the placeholders as needed for
Spanish grammar. Plural entries have separate singular and plural translations.
The checker rejects missing/fuzzy entries, missing or changed placeholders, and
changed BBCode tags. The English catalog is regenerated from source wording.
The campaign catalog is maintained separately and is never overwritten by the
interface extractor. Regeneration refuses to drop a message that is in
`messages.pot` but no longer extracted, and lists it: a reformat or a
concatenated template can hide a message from the extractor, and regenerating
would silently delete its translations. When the removal is intended, rerun
with `--allow-removals`. Run the build to import changed PO files and rebuild the
Windows package.

The extractor scans marked game C++ messages, scene text properties, creation
and advancement data tables, structured rules messages and the dynamic-source
manifest. It does **not** extract every arbitrary C++ string or scan original
game files. When adding a new native API message, mark its presentation use or
add its source wording to the manifest. Do not add identifiers merely because
they are strings.

## Architecture

- Translate whole sentences before inserting values. Use `i18n::format()` with
  named arguments instead of joining English sentence fragments.
- Use `i18n::plural()` for counts. `N_("message")` marks a static source string
  for extraction without translating it when the program initializes.
- `i18n::prepare_ui()` translates scene-authored text once and disables Godot's
  automatic translation on that native-owned UI. Native refresh methods then
  supply translated strings explicitly. This avoids translating a player name
  or formatting a message twice during pseudo-localization.
- Rules emit `rules::Message` values containing a source template and named
  arguments. An argument is explicitly either literal data or a translatable
  display label. The rules module never depends on Godot or on the current
  locale. Character explanations and new combat log events use this boundary.
- Keep race/class IDs, resource IDs, command verbs, portrait metadata and saved
  character names unchanged. Portrait filters match metadata, not translated
  labels. Names and save-slot names are literal user data, even when they match
  an English class name. Substitution examines the template once; braces in a
  name are never interpreted as placeholders. Rich-text character names are
  escaped before presentation.
- Language preference is application configuration (`settings.cfg` beside the executable,
  section `interface`, key `language`), not campaign state. On startup the saved
  supported language takes precedence over the system language; otherwise use
  Spanish for a Spanish system locale and English as fallback.

## Campaign text and compatibility

Original ECL dialogue remains locally decoded from the player's game files.
The presentation layer looks up a translation using the decoded text as `msgid`
and a stable resource context as `msgctxt`:

```text
por/area/<area_id>/script/<script_id>/dialogue
por/area/<area_id>/script/<script_id>/choices
por/combat/dialogue
```

Place campaign-specific Spanish entries in `campaign.es.po`. Choice indices,
script variables and source text used by the native scheduler are unchanged;
only their displayed text is translated. Missing entries retain the original
text. A full Spanish translation of the original campaign is **not** included.
Do not commit decoded original dialogue or modify/redistribute DAX files as part
of catalog generation.

The existing checkpoint format and English diagnostic log are preserved. New
combat events have structured translated presentation. Historical log lines
loaded from older checkpoints have only their original wording and fall back
to it when no exact translation exists. Some native diagnostics and original
item descriptions can likewise fall back to their source wording.

## Splash artwork

Both screens and languages share **one** unchanged background:
`art/OpenGoldBoxSplashBackground.png`.

| Language | Engine lettering | Game lettering |
| --- | --- | --- |
| English | `art/OpenGoldBoxEngineLettering.png` | `art/OpenGoldBoxGameLettering.png` |
| Español | `art/OpenGoldBoxEngineLettering.es.png` | `art/OpenGoldBoxGameLettering.es.png` |

The build copies these files into `res://bin/splashes/`. The chosen locale
selects only the lettering texture; both Spanish screens retain the same
background and the existing 0.6-second text fades. Spanish uses the approved
title **Estanque de Resplandor**. These are transparent 1672 × 941 source PNGs,
displayed proportionally in the 1920 × 1080 viewport, without warping or local
resampling. Like the existing splash PNGs, they remain local assets under the
repository's PNG ignore rule. See
[Spanish splash provenance and prompts](../art/OpenGoldBoxScreens.spanish-prompts.md).

## Validation

`tests/language_dialog_tests.gd` checks `--reset-lang` with and without `--splash`,
keyboard and button confirmation, preference persistence, and the first-splash
input boundary. Its `--language-restore` mode verifies the saved Spanish choice
in a fresh process; `--language-close` and `--language-ctrl-x` check scene teardown.
`--language-capture=<absolute-directory>` captures the selector and Spanish
splashes and compares the background pixels with lettering hidden.

Run `tests/localization_tests.gd` against the game project. It checks Spanish
choices and prerequisites, portrait filtering by stable metadata, complete
character creation, literal names with braces/BBCode, translated sheets and
rule explanations, singular/plural forms, English reload and source fallback.
Its optional `--localization-capture=<absolute-directory>` argument captures
Spanish screens and an accented pseudo-localized English screen for visual
review. Pseudo-localization is a development check, not a third player language.

The current font covers English and Spanish, including accented letters and ñ.
Check every new translation at the standard 1920 × 1080 viewport and at the
1120 × 800 minimum window. Prefer natural concise labels; do not clip essential
instructions to accommodate longer wording.

Reference: [Godot's gettext workflow](https://docs.godotengine.org/en/stable/tutorials/i18n/localization_using_gettext.html).
