# Creature Stats Editor: requirements

How the edits reach the game: [the interface](creature-stats-editor-interface.md).

Requirements for a developer tool that changes a monster's or NPC's Hit Points,
level and gear, and the encounters they fight in. Nothing is built yet. Written
2026-11-03, after Tyranthraxus's guards had to be lowered from the original's 87
Hit Points to the SRD Knight's 52 by hand
([DEVIATIONS](../../docs/DEVIATIONS.md)).

Decided (the user, 2026-11-03): it is a Godot tool, like the
[Combat Sprite Catalog](combat-sprite-catalog.md), and Codex builds it.
Encounters are editable too, including their fight rating.

## Why

Changing one of these numbers today means finding the right file, knowing its
format, and then remembering everything that has to follow the change. For the
guards that meant:

- the stat line in [combat.rules](../../data/rules/srd-5.2.1/combat.rules) and its
  comment;
- the rules version in `srd5.cpp`, and in [SAVES.md](../../docs/SAVES.md);
- an entry in [DEVIATIONS.md](../../docs/DEVIATIONS.md);
- the conversion table in the area's audit;
- a test.

It also meant knowing what the game actually uses. The guards' saved gear lists
plate armor +2 and a ring of protection +3, but a monster's Armor Class comes
from its stat line, so the armor and the ring change nothing in a fight. The tool
should make all of this visible and do the bookkeeping.

## Where the values live now

| What | Where | Read by |
| --- | --- | --- |
| A monster's AC, Hit Points, attacks, saves and abilities | `creature` and following rows in `data/rules/srd-5.2.1/combat.rules` | the rules module |
| Which stat line an original record uses, and the XP that sizes the fight and the XP it awards | the conversion tables in `src/OpenGold.Core/src/rolf_tour.cpp` (e.g. `{85, "castle-knight", 700, 700}`) | the game |
| A monster's gear: held weapons, potions, wands, scrolls | the Combat Sprite Catalog's `json/` records, exported to `data/monsters/` | the game |
| Metal armor, for Heat Metal | `equipment` rows in `combat.rules` ([CREATURE-EQUIPMENT](../../docs/CREATURE-EQUIPMENT.md)) | the rules module |
| A companion's class, level, scores and gear (Genheeris, Dirten, Fatima and the others) | code in `src/OpenGold.Core/src/companions.cpp` | the game |
| What each difficulty does to Hit Points and counts | `src/OpenGold.Core/include/opengold/difficulty.h` (DIFF-2) | the game |
| Which creatures a fight has, and how many | the original's scripts (`LOAD MONSTER` record and count, e.g. ECL5.DAX:7 at 0xA60D loads twelve of record 85), and a few script tables such as Podol Plaza's roaming groups | the game, from the player's install |
| How many of them the party meets | the encounter sizing: the party's SRD XP budget at the encounter challenge, at most one creature per living member, then the difficulty (`src/OpenGold.Core/src/encounter_budget.cpp`) | the game |

A monster has no level of its own. "Level" applies to:
- companions, who are real characters with a class level;
- the few stat lines that cast spells. A spellcaster's level is its spell slots
  and casting bonus, such as the castle's mages, which use the nomad shaman's
  level-4 Wizard profile.

## Requirements

### Choosing a creature

1. List every original creature record the game converts, by area and record
   (`MON5CHA.DAX:85`). Show its catalog name, the stat line it uses and the
   original's Hit Points, read from the player's install.
2. List the companions, with class, level, scores and gear.
3. Search by name, stat-line key or record.

### Showing what the game uses

4. For a monster, show its effective combat values: AC, Hit Points, attacks with
   to-hit and damage (after its held weapon replaces the stat line's dice), saves
   and abilities. Build these values with the game's own rules module and
   loaders, not by re-reading the files a second way.
5. Mark gear the fight ignores. A monster's armor, rings and other worn items
   do not change its AC; only held weapons, potions, wands and scrolls count.
   The rest shows as loot only.
6. Show Hit Points at each difficulty (Easy 60% to Infernal 320% as of
   2026-11-03) and the fight
   rating (XP) that sizes encounters.
7. List every record that shares the stat line. Changing `castle-knight` also
   changes the 8th level fighters in Valjevo's old town.

### Changing a creature

8. Hit Points: set a stat line's Hit Points. Offer the original's value and the
   SRD stat block's value, with their sources (e.g. "original 87", "SRD Knight
   52").
9. Split a stat line: when the change should affect only some records, copy the
   stat line under a new key and point those records at it.
10. Level, for companions: set the class level. The tool rebuilds the companion
    the way the game does, with the rules' default choices for each level it
    reaches, and shows the new Hit Points, features and spells.
11. Level, for spellcasting stat lines: choose a profile level (spell slots and
    casting bonus) from the profiles the rules already define. The tool does not
    invent new profiles.
12. Gear, for monsters: add, remove or replace held weapons, potions, wands and
    scrolls, using the items the game converts. The Combat Sprite Catalog
    already edits this list; the tool either uses the catalog's editor or writes
    the same record and runs the same export. There must not be two gear
    editors that disagree.
13. Gear, for companions: add, remove and equip items from the game's item
    list, as the shop and inventory screens allow.
14. Refuse values the rules would reject: Hit Points of 0, a second armor, a
    third ring, a weapon the creature has no hands for.

### Keeping the repository consistent

15. Write only to the files the game reads (`data/`, including the data files of
    requirements 28 to 30). The game never reads `dev_tools/`.
16. When a change alters the rules content, raise the rules version in
    `srd5.cpp` and in `docs/SAVES.md`, and say that saves made before the
    change can no longer be loaded (pre-1.0 policy).
17. When a value departs from the original's, require a one-line reason and
    add or update the creature's entry in `docs/DEVIATIONS.md`, and the
    conversion table in the area's audit in `docs/audits/`.
18. Update the comment beside the stat line in `combat.rules`.
19. Show the change as a diff before saving and save nothing until the user
    confirms.

### Measuring a change

20. Offer to replay the fights that use the creature in the
    [AI lab](ai-lab.md), before and after, and show wins and Hit Points left at
    each difficulty. The AI lab's scenario packs carry the rules version, so
    the tool rebuilds the pack, or plays the "before" from the old stat line.

### Encounters

21. List every fight the game stages: area, script and place (record and
    address, and the map square where the party meets it), with the original's
    groups, records and counts and the text that announces it. Fights the
    script rolls from tables, such as Podol Plaza's roaming groups, list their
    tables.
22. For a chosen fight and a reference party (a party from an AI lab scenario,
    or levels and size the user enters), show what the game makes of it at each
    difficulty: the XP budget, each group's rating, and how many creatures of
    each group are met. Computed by the game's own sizing code. The
    Tyranthraxus guards were this case: twelve loaded, eight met.
23. Fight rating: edit each creature's two values separately: the XP that sizes
    the fight and the XP the fight awards. Show which encounters each value
    changes.
24. Composition: change a fight's groups (another record, or another stat
    line for a record) and counts. The original's script stays as it is; the
    change is an override the game applies when that script loads the fight.
25. Show every override beside the original's value, and allow removing it to
    go back to the original.
26. An encounter change is a departure from the original, and follows
    requirements 17 and 19. A changed award also changes the party's
    experience, which DEVIATIONS says stays the original encounter's; the tool
    says so before saving.
27. Requirement 20 applies: replay the changed fight in the AI lab before and
    after.

### Changes the game needs first

28. Companions are code today. Move their definitions (class, level, scores,
    gear, original record) to a data file the game reads, for example
    `data/companions/`, keeping the game's behavior unchanged. Needed for
    requirements 10 and 13.
29. The fight ratings are code today (the conversion tables in `rolf_tour.cpp`).
    Move them to a data file the game reads, keeping every value. Needed for
    requirement 23.
30. The game has no encounter overrides. Add a data file of overrides, keyed by
    script and address, that the game applies when a script loads monsters, and
    rejects an override whose script, address or record does not exist. Needed
    for requirement 24.

Each of these is a change to the game, with its own tests, and comes before the
tool's part that needs it.

## Checks

- A native test loads every stat line the tool can write and builds each
  companion from its data file.
- A round trip: load, change nothing, save; the files are byte for byte the same.
- The tool's displayed values match a fight's values for the same creature: one
  monster per area, staged through the game's encounter code.
- Writing a change to the rules raises the rules version, and the tool will not
  save a departure from the original without its DEVIATIONS entry.
- For every fight, the tool's count of creatures met matches what the game
  stages for the same party, script and difficulty.
- With no overrides, every fight stages exactly as before requirements 29 and
  30 were built.
