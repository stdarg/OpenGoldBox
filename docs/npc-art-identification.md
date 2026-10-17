# NPC art identification

## Results from the local Pool of Radiance installation

The September 5, 2026 review inventory had 17 unresolved groups: eight combat
icons and nine encounter sprites. All 17 now have script-reference candidates;
this does not mark them confirmed or establish a unique identity for each image.
Human art is frequently reused for generic characters and named NPCs.

The index inspects 29 ECL records and currently decodes 15,864 possible-path
instructions. It reports seven diagnostics and 60 dynamic art-selection
instructions, of which four combat instructions have bounded parallel-table
analysis (16 possible table-index combinations). Counts describe this particular
installation, not every release. Unknown conditions can lead static traversal
into otherwise unreachable bytes; diagnostics remain visible rather than being
silently treated as successful disassembly.

Representative archive references below retain their original IDs. Duplicates
in other archives remain grouped by the review tool. All archive selection is
inferred from the ECL bank or explicitly reported as ambiguous.

| Art | Evidence-supported candidates or uses | Evidence |
| --- | --- | --- |
| CPIC1 / 11 | Shared human fighter; includes Mad Man, Skullcrusher, Princess Fatima and generic fighters | Multiple LOAD MONSTER instructions use different character IDs with icon 11; Mad Man: ECL2/15 @AAF3 |
| CPIC1 / 16 | Shared fighter/officer art | ECL1/18 @A9D7 loads character 41 with icon 16; other occurrences name captain/commandant records |
| CPIC2 / 14 | Shared magic-user art | ECL2/9 @A29E loads character 89 with icon 14; other scripts supply named and generic casters |
| CPIC2 / 94, also CPIC4 / 24 | Magic-user art | ECL2/20 @A0A5 loads character 94 with icon 94; archive-specific records remain important |
| CPIC3 / 12 | Shared thief/other human art | ECL3/14 @9C82 and @9C8A load characters 51 and 45 with icon 12; other uses include a cleric |
| CPIC3 / 13 | Shared cleric/other human art | ECL5/6 @A4C7 and @A4CE load cleric records 91 and 90 with icon 13; tavern tables reuse this art |
| CPIC3 / 15 | Nomad, corporal, dwarven-fighter candidates | Multiple explicit load operands; shared art, not one personal identity |
| CPIC3 / 24 | Sixth-level thief in tavern brawl | ECL3/0 @A78C, table index 5: character 51 and icon 24 |
| SPRIT2 / 12 | Rolf, trader, and other shared civilian uses | ECL3/0 @B0AD selects sprite 12 before Rolf's introduction; ECL2/9 @AE2E has trader dialogue |
| SPRIT2 / 15 | Mad Man and city watch | ECL2/15 @A7B1; recruitment at @A9F4 adds NPC 25. ECL3/11 @99B6 has watch dialogue |
| SPRIT2 / 16 | Shared armored-human encounter art | Numerous encounter selections and character contexts; no unique identity inferred |
| SPRIT2 / 24 | Shared magic-user encounter art | Multiple caster/envoy encounters; accompanying creatures can also occur on possible branches |
| SPRIT3 / 7 | Tavern brawler | ECL3/0 @A69C selects sprite 7, followed by the brawl announcement |
| SPRIT3 / 11 | Thief/bandit encounter uses | ECL3/14 @AF12, @AC45 and other selections accompany thief encounters |
| SPRIT3 / 13 | Dwarf/fighter encounter candidate | Script context includes a dwarven fighter and other combatants; exact identity remains ambiguous |
| SPRIT5 / 22 | Scribe | ECL5/4 @B160 selects sprite 22; @B176 describes a scribe entering the room |
| SPRIT5 / 29 | Priest of Bane / acolytes encounter | ECL5/6 @A416 selects sprite 29; @A41D describes the priest and acolytes; combat loads cleric records |

Names here distinguish art roles from people. In particular, a picture of a
person accompanied by several enemies does not prove that every enemy named on
an encounter branch is the person pictured. The review UI labels such links as
possible encounter-path candidates and shows the dialogue for assessment.

## Identical combat art in other archives

The combat sprite catalog now identifies five additional archive copies by exact
comparison of both decompressed ready/action records, including their headers and
all frame data. These five pairs have no direct reference in the current decoded
script index, but their artwork is identical to referenced pairs in other banks.

| Archive copy | Identical copies | Shared art identity and known users |
| --- | --- | --- |
| CPIC1 / 11 | CPIC2 through CPIC8 / 11 | Shared fighter art; Mad Man, Skullcrusher, Hassad, Princess Fatima, TYRANITHRAXUS and generic fighters |
| CPIC4 / 14 | CPIC2, CPIC3, CPIC5, CPIC6, CPIC7 / 14 | Shared spellcaster art; Al-Hyam Dazid, Genheeris, Yarash, Shaman, Envoy and generic magic users |
| CPIC6 / 4 | CPIC1, CPIC2, CPIC4 / 4 | Orc art |
| CPIC6 / 12 | CPIC3, CPIC5, CPIC7 / 12 | Shared humanoid art used for thieves, a cleric and Nomad |
| CPIC6 / 13 | CPIC3, CPIC5, CPIC7 / 13 | Shared humanoid art used for clerics, a thief and a fighter |

The identity of the duplicated artwork is established by matching record bytes.
The known users come from the other copies' static script references, with the
archive-selection limits described above. This does not establish that these five
particular archive copies load during play or assign each shared image to one NPC.
They retain their separate source identities and **Unused** status in the catalog.
No gear is inferred or combined across users of shared artwork.

The five metadata files are `cpic1_dax_11.json`, `cpic4_dax_14.json`,
`cpic6_dax_4.json`, `cpic6_dax_12.json` and `cpic6_dax_13.json` in
`dev_tools/combat_sprite_catalog/json`. The catalog rebuilds duplicate evidence
from the installed assets, saves it separately from direct creature links and
preserves edited names. Its `--catalog-check` covers the five identities and their
usage flags; `--metadata-check` covers save/reload and name preservation.

## The previously missing combat icon

ECL3/0 uses GETTABLE instructions to read parallel character and icon tables at
VM addresses B622 and B628. Randomness and party-strength checks determine a
bounded index; the third pair of lookups can reach index 5. That entry contains
character ID 51 and CPIC ID 24, used by LOAD MONSTER at A78C. MON3CHA/51 is named
6TH LVL THIEF. This explains a link that a constant-only opcode index misses.

The analysis keeps the two tables' indices correlated. It does not combine
every possible character with every possible icon. Calls, unknown definitions,
table writes, overlap with code, out-of-range indices, and unbounded values
prevent this inference. The result remains a possible static table binding.

## Implementation and saved evidence

- `demos/godot/scripts/por_ecl_decoder.gd`: strips the two-byte record prefix, reads
  the five entry jumps at VM origin 9900, decodes typed operands and packed text,
  and traverses branches and conditional instruction skips. Pool opcode 34 has
  one operand; the inspected reference implementation's two-operand version
  misaligns the local ECL7/17 routine at 9D37.
- `demos/godot/scripts/por_ecl_tables.gd`: bounded backwards definition analysis for
  adjacent parallel table reads. Random results are represented conservatively
  as an inclusive range, without claiming every branch combination occurs.
- `demos/godot/scripts/art_script_evidence.gd`: indexes art selections, follows bounded
  possible paths with matched subroutine returns, and gathers literal dialogue,
  recruitment and combat references. Context traversal stops at scene changes,
  resource loads, native calls, combat, and explicit exits. Depth/size limits are
  reported. Return-only helper routines may need their callers inspected manually.
- `art_group_review.gd`: adds the Script evidence tab and searchable dialogue
  roles. Script candidates supersede weak same-ID suggestions; explicit reviews
  and legacy rejections still take precedence. No automatic confirmation occurs.

Run `demos\review-art.cmd` normally. The index is rebuilt for the configured local
installation and written to `user://art-script-evidence.json`. Group inventory
entries link to script locations; detailed evidence contains archive candidates,
VM addresses, decompressed-record offsets, decoded operands, a script-content
fingerprint, possible identities, and dialogue. Review names, categories and
decisions remain separate files.

For reproducible offline output:

```powershell
godot_console --headless --path demos/godot --script ../../tools/research_ecl_art.gd
godot_console --headless --path demos/godot --script ../../tests/ecl_art_tests.gd
```

The research command writes `user-data/ecl-art-research.json`,
`user-data/art-script-evidence.json`, and `user-data/unresolved-art-evidence.md`.
These contain locally extracted original-game data and remain ignored.

## Verification still required

An isolated copy of the game and saves was launched for an original-game check.
It reached the code-wheel prompt. The included code-wheel utility did not render
in the background session, so no encounter/resource trace was completed. Neither
the installed game nor its saves were changed. Nothing is labeled runtime-verified.

The most useful next gameplay checks are:

1. Rolf's introduction and a tavern brawl in Phlan (ECL3/0): confirm encounter
   sprites 12 and 7, then the table-selected combat icon 24 for a sufficiently
   strong party. Record which resource bank actually loads.
2. Mad Man (ECL2/15): compare the encounter picture, the recruit's name, and the
   combat icon. Expected candidates are SPRIT2/15, MON2CHA/25 and CPIC2/11.
3. The scribe and Bane-priest encounters (ECL5/4 and ECL5/6): verify sprite 22
   and sprite 29 while the corresponding dialogue is displayed.

For each observation, retain the save/area, screenshot, exact archive/record if
traced, and whether the evidence proves a personal name or only an encounter role.
Remaining general work includes full resource-bank selection, runtime-dependent
references, assembled character graphics and precise map-coordinate attribution.

## Sources

Format behavior was inspected in Gold Box Explorer revision
`eac30abaa6ee66aea6f5d65ebe6d676b10015a8f`, particularly
[Program.cs](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/DaxEcl/Program.cs),
[Memory.cs](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/DaxEcl/Memory.cs), and
[Commands.cs](https://github.com/bsimser/Gold-Box-Explorer/blob/eac30abaa6ee66aea6f5d65ebe6d676b10015a8f/src/Common/Plugins/DaxEcl/Commands.cs).
The GDScript inspector is an independent implementation of the inspected format.
See also [the earlier source audit](asset-source-audit.md).
