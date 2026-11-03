# Game interfaces needed by Creature Stats Editor

The editor is confined to `dev_tools/`. Its requirements are in
`../docs/creature-stats-editor-requirements.md` (currently in the
`OpenGold-guards` worktree). The game changes below belong to Claude. They are
needed before the editor can make authoritative monster edits.

1. Move the per-area original-record conversions now local to
   `src/OpenGold.Core/src/rolf_tour.cpp` into a versioned game-read data file.
   Preserve the current bank/record, stat-line key, encounter-fit XP, and award
   XP for every row, including shared Valjevo Castle conversions. Expose a Core
   loader/query for all rows and their area names. The tour must consume that
   same data. This lets the editor list every converted record, show all users
   of a stat line, split a stat line for selected records, and edit fight XP
   without parsing C++ source or maintaining a second mapping.
2. Expose a rules-backed query for a staged monster's effective combat profile:
   AC, maximum HP, melee and ranged attacks with to-hit and damage after held
   weapons replace dice, saves, abilities, and spellcasting profile. It should
   use the same rules calculations and staged gear path as a real fight. The
   current public `CombatantView` supplies HP and AC but omits the complete
   effective attack and save profile. The editor must not duplicate SRD logic.
3. Keep companion migration separate. Companion level and gear edits remain
   out of the first monster-only delivery until definitions are in game-read
   data and can be built through the game's character rules.

Acceptance for (1): loading the new data yields the exact current conversion
rows and fight XP values, and existing encounter tests remain unchanged.
Acceptance for (2): one representative monster per area reports the same
profile in the editor query and an encounter staged by the game. No save format
or rules result should change from these read-only APIs.
