# Monk starting tool proficiency

[#215](https://github.com/stdarg/OpenGold/issues/215) implements the starting tool
choice from SRD 5.2.1 Core Monk Traits (p. 49). A Monk chooses one type of
Artisan's Tools or Musical Instrument. Equipment pp. 93–94 lists seventeen
artisan tools and ten instrument variants; each requires a separate proficiency.
[Official SRD](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).

The artisan options are Alchemist's Supplies, Brewer's Supplies, Calligrapher's
Supplies, Carpenter's Tools, Cartographer's Tools, Cobbler's Tools, Cook's
Utensils, Glassblower's Tools, Jeweler's Tools, Leatherworker's Tools, Mason's
Tools, Painter's Supplies, Potter's Tools, Smith's Tools, Tinker's Tools,
Weaver's Tools and Woodcarver's Tools. Instruments reuse the complete
[Bard instrument catalog](BARD-INSTRUMENTS.md). Thieves' Tools and other tools
outside these categories are not Monk starting choices.

## Behavior

The existing Training checkbox controls require one selection, enforce the
limit and preserve keyboard focus and Back selections. Presets generate a
choice. The sheet lists `tool:<id>` with level-one `class:monk:tools` provenance.
Background overlaps preserve both sources, add proficiency once and show one
tool entry. A proficient skill used with a proficient tool adds Advantage using
the existing rules-owned check query.

Monk and Bard tool groups share Core's existing choice-continuity metadata.
Switching Bard to Monk keeps the first selected instrument; switching back keeps
that instrument and requires the remaining Bard selections. Artisan tools do
not transfer into Bard's instrument-only entitlement. Background changes retain
valid tool selections. Rules own all catalogs; Core contains no class-specific
transfer code.

Profile recipes validate the new policy.

## Verification

Native checks cover every catalog option, sources, overlap, bonuses/Advantage,
current saves, forged sources/versions, invalid choices, preset generation and
class-change preservation.
Godot checks cover keyboard selection, limits, Back and completed sheet sources.
All 41 native/tool checks and 16 Godot runtime checks (plus seven native
prerequisites) pass. Main/demo extensions build and 805 English/Spanish messages
validate. The graphical Training test passes; English/Spanish tool lists were
inspected at 1120×800 and 1920×1080, including scrolling to the last instrument.
Artifacts: `/tmp/opengold-monk-renders`; logs: `/tmp/opengold-monk-regression.log`
and `/tmp/opengold-monk-render.log`.

Starting equipment ownership, crafting and Utilize actions, Martial Arts and
other Monk features remain separate. Multiclass entry does not award this
starting proficiency. This increment does not complete the full Monk tracker.
