# Complete character portraits

The roster contains 324 independently composed portraits: one for every
combination of 12 classes, nine in-game races, and three genders. The initial
six were approved for style, the next 30 established class coverage, a further
36 expanded the choices in every class, and the final 252 fill every missing
combination. These files are used by the Character Review demo, character pool,
and party previews. The selector offers optional gender, class, and race filters.

Each PNG contains a complete chest-up character on a black background. Faces,
physiques, poses, and clothing are individually designed rather than assembled
from interchangeable heads and bodies. Each class has 27 portraits, one per
race and gender. Nonbinary briefs request androgynous presentation. Bard
portraits include storytelling, song, dance, poetry, acting, juggling, and
instrumental performance.
Real-world ancestry-inspired facial and hair characteristics vary across fantasy
races and are not game-rule categories or metadata fields.

`portraits.json` maps each PNG basename, including its extension, to exactly
`Class`, `Gender`, and `Race`, using the character creator's display names.
`prompts.json` records the original six prompts and `roster-prompts.json` records
the first 30 expansion prompts. `class-expansion-prompts.json` records the next
36 portraits, with different costumes, poses, ages, physiques, and class
interpretations. `full-coverage-prompts.json` records the final 252. All use the built-in image generator. The
original game's local contact sheet informed general palette, framing, and
readability of the approved six; the expansion uses those approved portraits
as a rendering-style reference. No extracted original assets are included here.

The four original Orc portraits were subsequently revised to visibly green skin at the
user's request, retaining their individual ancestry-inspired facial features,
hair, expressions, costumes, and poses. `orc-green-revision-prompts.json` records
the built-in image-generation edit prompts. Their basenames and catalog metadata
remain the same. Later Orcs were generated with green skin from the start.
Earlier generation prompts document the original versions.

All 324 PNGs retain the generator's 1254 x 1254 full-resolution output. Pixel-art styling does
not guarantee a mathematically uniform low-resolution pixel grid. Local review
previews render each at 88 x 88 as well as enlarged sizes; these are visual
readability checks, not a claim of a separately
hand-tuned 88 x 88 asset set.

Coverage includes Barbarian, Bard, Cleric, Druid, Fighter, Monk, Paladin,
Ranger, Rogue, Sorcerer, Warlock, and Wizard; Dragonborn, Dwarf, Elf, Gnome,
Goliath, Halfling, Human, Orc, and Tiefling. Metadata is descriptive and does
not impose restrictions on character creation.

Validation: all 324 PNGs decode, have unique file hashes, and correspond exactly
to catalog basenames and recorded prompts. Metadata fields and values match the
character creator; class, race, and gender coverage was checked. All portraits
were visually inspected on class contact sheets at portrait-picker size.
