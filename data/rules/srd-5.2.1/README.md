# SRD 5.2.1 demonstration content

`combat.rules` is a small, offline, curated combat pack, revision
`srd-5.2.1-demo.1`. The runtime validates it directly; it makes no HTTP requests.
Open5E provides reference data, while OpenGoldBox implements executable behavior.

`open5e-bandit.json` is the complete Open5E response fetched on 2026-09-06 from:

https://api.open5e.com/v2/creatures/?document__key=srd-2024&name__iexact=Bandit

The document filter deliberately excludes other editions and third-party
sources. Open5E labels this source "System Reference Document 5.2"; the curated
fields and implemented mechanics target the official 5.2.1 PDF. Original
download SHA-256:

`02B3D648E395CCEABDD238CE8AA1199B5C5B205A2F9F83E3A2DA93F97386EEEB`

The Bandit profile retains AC 12, HP 11, initiative +1, speed 30, scimitar +3
for 1d6+1, and light crossbow +3 for 1d8+1 at 80/320 feet. Damage types in
this response require checking the action text: the structured primary type is
null while the extra-damage-type field contains the type. The current module
does not yet resolve typed damage.

## Slums monsters

The original Slums monsters are SRD stat blocks
([MON-1](../../../docs/SRD-DECISIONS.md#mon-1-2026-10-02-original-monsters-as-srd-stat-blocks)).
Each Open5E response was fetched on 2026-10-02 and is retained here:

| Rows | Stat block | Source | Response | SHA-256 |
| --- | --- | --- | --- | --- |
| `slums-kobold*` | Kobold Warrior | SRD 5.2 (`srd-2024`) | `open5e-kobold-warrior.json` | `9929C60A397A73A5D61E75DC50FD9FECFF42EF6F299C6267B210F2B76D16B6B6` |
| `slums-goblin*` | Goblin Warrior | SRD 5.2 (`srd-2024`) | `open5e-goblin-warrior.json` | `8FCFA3B9263B1CF4BC1CD6F7B36AC0892FAFCFFA8A4A2FB48EDC18DD2FC1E2D3` |
| `slums-orc*` | Orc | SRD 5.1 (`srd-2014`) | `open5e-orc.json` | `8AD2F367656EBB0AC1DADE6C28D619FEB6FE16F844699D4208C78D3B3E3CEE54` |
| `slums-bugbear` | Bugbear Warrior | SRD 5.2 (`srd-2024`) | `open5e-bugbear-warrior.json` | `4FEE28307486692A34C1583EC70757337D21BD929911950BC241592F4A10F994` |

Queries: `https://api.open5e.com/v2/creatures/?document__key=<source>&name__iexact=<name>`.
The 2024 SRD has no orc, so the Orc is the one exception to this pack's 5.2.1
source. Rows keep each block's AC, HP, initiative, speed, attacks, saving
throws, damage types and size. Supplemental rows add the supported traits:
`pack_tactics` (Kobold Warrior), `advantage_damage` (Goblin Warrior's extra
1d4) and `aggressive` (Orc). By decision, Sunlight Sensitivity and Nimble Escape
are omitted, and the Bugbear's Grab is a 5-foot attack without a grapple, its
Light Hammer a thrown attack without the grappled Advantage.

A `-leader` row is its base stat block wearing the armor its original record
readies (AC 16 for kobold, goblin and orc leaders; `slums-kobold-leader-sword`,
record 11, readies only studded leather and keeps AC 14). Only
`slums-orc-leader-archer`, for the orc leader whose combat icon shows a bow,
replaces the Javelin with a Longbow (+3, 1d8+1, 150/600).

## Fixtures

`vanguard`, `scout`, `adept`, and `healer` are authored combat fixtures, not
complete SRD class builds. None of these profiles claims to be an imported
Open5E monster stat block. The party casters deliberately carry only two
level-1 slots for this standalone example. Shared campaign characters instead
use [manual level-1–4 advancement](../../../docs/ADVANCEMENT.md), including
level-two slots and selected supported spells/feats.

To update content, fetch an explicitly filtered source, retain the response and
provenance, compare supported fields with the official SRD, revise the curated
pack, and run the native tests. Unknown mechanics need implementation or an
explicitly authored fixture; they must not disappear silently in an importer.
The first milestone does not include a general importer or database dependency.

The header defines a format version and content revision. Each `creature` line
has the fields listed in the pack comment. `casting_bonus` includes proficiency
2 for these level-1–4 fixtures. Spell mask bits are Fire Bolt=1, Cure Wounds=2,
Magic Missile=4. Optional `saves <key> <STR> <DEX> <CON> <INT> <WIS> <CHA>`
rows supply explicit saving throw bonuses. Optional `spellcasting <key>
<level-two-slots> <spell-mask>` rows add level-two casting; the additional mask
bits are Healing Word=8, Scorching Ray=16 and Blindness=32. Rows must follow the
creature they extend and may occur only once per kind. `blindness-adept` is the
authored existing-scene condition fixture. These supplemental rows preserve
loading of legacy authored packs, whose omitted save bonuses default to zero.
The module identity includes a fingerprint of the whole pack
after CRLF normalization; changing definitions invalidates old checkpoints.

See [NOTICE.md](NOTICE.md) for the required attribution. SRD-derived content is
CC BY 4.0; the repository's code license does not replace that attribution.

Sources: [official SRD](https://www.dndbeyond.com/srd),
[SRD 5.2.1 PDF](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf),
[Open5E API documentation](https://open5e.com/api-docs).
