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

## Cadorna Textile House monsters

The Cadorna Textile House's monsters are SRD 5.2.1 stat blocks
([audit](../../../docs/audits/cadorna-textile-house.md),
[CADORNA-1](../../../docs/SRD-DECISIONS.md#cadorna-1-2026-10-24-the-cadorna-textile-house)).
Each Open5E response was fetched on 2026-10-24 (`srd-2024`) and is retained here:

| Rows | Stat block | Response | SHA-256 |
| --- | --- | --- | --- |
| `skeleton` | Skeleton | `open5e-skeleton.json` | `943A7325C9A0F8149737A2FD8934404383441A8FFF1998574F459545D190A1D5` |
| `giant-frog` | Giant Frog | `open5e-giant-frog.json` | `098AABC1C09555D238C17AB17A6E0DFCE7994DA2CA52F25AEC008FF5C1C5CD4C` |
| `giant-scorpion`, `large-scorpion` | Giant Scorpion | `open5e-giant-scorpion.json` | `A6074EE3FDAAA4CF1A85513F576AA83734D9CD124DC84F13541D99729BD99530` |
| `wight` | Wight | `open5e-wight.json` | `E6EDD76FB2CAE5880136D6E000F14C7EB85173A93962A4199500D7C90B7017B8` |
| `grishnak` | Priest | `open5e-priest.json` | `5BFA33E9EE1713A6BBBCEA57B48164694FB71A227D8EAE06093567D6E7AE1DBC` |
| `skullcrusher` | Tough | `open5e-tough.json` | `91473AABCF59135925AD6A3EB286CD25152B1A49A6C2A22A06433E8A1F0E6372` |

The responses leave damage vulnerabilities, resistances and immunities and the
Priest's daily uses empty; those come from the SRD 5.2.1 PDF: the Skeleton's
Vulnerability to Bludgeoning and Immunity to Poison, the Wight's Resistance to
Necrotic and Immunity to Poison, and the Priest's Spirit Guardians once a day
and Divine Aid three times a day. By decision the Wight and the Large Scorpion
keep the original's Hit Points (23 and 10), the Giant Frog has no Grapple or
Swallow, and the Giant Scorpion's Claws no grapple. New rows: `last_attack`
(the Multiattack's last attack, the Sting), `multiattack_ranged`, `life_drain`,
`sunlight_sensitivity` (also on the kobolds), `innate` (daily spell pools) and
`evil`.

## Mendor's Library monsters

Mendor's Library's monsters are SRD 5.2.1 stat blocks
([audit](../../../docs/audits/mendors-library.md),
[MENDOR-1](../../../docs/SRD-DECISIONS.md#mendor-1-2026-10-24-mendors-library)).
Each Open5E response was fetched on 2026-10-24 (`srd-2024`) and is retained here:

| Rows | Stat block | Response | SHA-256 |
| --- | --- | --- | --- |
| `specter` | Specter | `open5e-specter.json` | `A104A532C248E82A811C10A8B4A125963C7F359D7C144799CF799378B91FA5AB` |
| `basilisk` | Basilisk | `open5e-basilisk.json` | `C87406EC4020A6F07E81464F88F51ABABC43D648BC5C142438C38B13BDEAF354` |
| `mad-man` | Guard | `open5e-guard.json` | `62B49565F6250D60D710E60003CEAF78B7EF44153BAEE2F7064D23D1EB80C18C` |

The Specter's Resistances, Immunities and condition Immunities come from the
SRD 5.2.1 PDF. Each keeps the original's Hit Points (38, 31 and 7). New rows:
`life_drain_attack`, `flies`, `incorporeal`, `condition_immunity` (SRD
condition names) and `petrifying_gaze creature dc recharge`.

## Central Wilderness monsters

The Central Wilderness's monsters are SRD 5.2.1 stat blocks
([audit](../../../docs/audits/central-wilderness.md),
[OVERLAND-1](../../../docs/SRD-DECISIONS.md#overland-1-2026-10-24-the-wilderness-map)).
Each Open5E response was fetched on 2026-10-24 (`srd-2024`) and is retained here:

| Rows | Stat block | Response | SHA-256 |
| --- | --- | --- | --- |
| `drider` | Drider | `open5e-drider.json` | `CF516165FDACEDFE5BDD07FC6F15C55E84163880F9BC858BD029E602F9D693A2` |
| `stirge` | Stirge | `open5e-stirge.json` | `FC86A8852699B6511D775069B4897EE299E738F61DA3C948667838EFA36560A2` |
| `bandit-captain` | Bandit Captain | `open5e-bandit-captain.json` | `755DEF9D6872843EA75FC6551D60B475AD40AF0018D3A4E68C3793DD2CF3CA38` |
| `displacer-beast` | Owlbear | `open5e-owlbear.json` | `A2454F0E7DA6BD3A3ADB6EE074D9C0A32D2384E3EF8A311403746B590BB1B4CF` |
| `wilderness-bandit` | Bandit | `open5e-bandit.json` | (above) |

Each keeps the original's Hit Points. New rows: `attach creature count sides
type` (the drain while attached), `web_walker`, and `innate` uses
`rechargeN`.

## Kovel Mansion monsters

Kovel Mansion's thieves are SRD 5.2.1 stat blocks
([audit](../../../docs/audits/kovel-mansion.md),
[KOVEL-1](../../../docs/SRD-DECISIONS.md#kovel-1-2026-10-24-kovel-mansion)).
The Open5E response was fetched on 2026-10-24 (`srd-2024`) and is retained here:

| Rows | Stat block | Response | SHA-256 |
| --- | --- | --- | --- |
| `kovel-thief` | Bandit | `open5e-bandit.json` | (above) |
| `kovel-master-thief` | Spy | `open5e-spy.json` | `38C5CE5513F9CC469DA515529B923B4C88708AF06E9D706E83CC6BDEFF35E762` |

Each keeps the original's Hit Points (4 and 28). New row: `cunning_action
creature`, the Rogue's Cunning Action.

## Temple of Bane monsters

The Temple of Bane's monsters reuse retained responses
([audit](../../../docs/audits/temple-of-bane.md),
[BANE-1](../../../docs/SRD-DECISIONS.md#bane-1-2026-10-24-the-temple-of-bane)):

| Rows | Stat block | Response | SHA-256 |
| --- | --- | --- | --- |
| `bane-cleric` | Priest | `open5e-priest.json` | (above) |
| `bane-orc-archer` | Orc (SRD 5.1) | `open5e-orc.json` | (above) |

The cleric keeps the original's 25 Hit Points; the archers shoot a Shortbow
(+3, 1d6 + 1 Piercing, 80/320 feet) in place of the Orc's Javelin.

## Nomad camp monsters

The nomad camp's monsters ([audit](../../../docs/audits/nomad-camp.md),
[NOMAD-1](../../../docs/SRD-DECISIONS.md#nomad-1-2026-10-24-the-nomad-camp)).
The Warrior Veteran's Open5E response was fetched on 2026-10-24 (`srd-2024`):

| Rows | Stat block | Response | SHA-256 |
| --- | --- | --- | --- |
| `hassad` | Warrior Veteran | `open5e-warrior-veteran.json` | `6AED9B1464F18369A80638420B2ECFCECB209832D5E437659B7F2A20D4622257` |
| `nomad-guard` | Bandit Captain | `open5e-bandit-captain.json` | (above) |
| `nomad-shaman` | the `slums-magic-user` Wizard profile | none | none |

Each keeps the original's Hit Points (45, 30 and 20); the Warrior Veteran's
and the Bandit Captain's Parry are omitted.

## Pyramid monsters

The Pyramid's monsters ([audit](../../../docs/audits/pyramid.md),
[PYRAMID-1](../../../docs/SRD-DECISIONS.md#pyramid-1-2026-10-24-the-pyramid)).
Each Open5E response was fetched on 2026-10-24 (`srd-2024`):

| Rows | Stat block | Response | SHA-256 |
| --- | --- | --- | --- |
| `minotaur` | Minotaur of Baphomet | `open5e-minotaur-of-baphomet.json` | `5F861981DC0B7F5ADCC823BD38483EB118C93D563F81E0A344CEF7993EBE01AA` |
| `trapped-acolyte` | Priest Acolyte | `open5e-priest-acolyte.json` | `44D66C1471AD22E6CFAF646187B62AA689AA671ACDA27740CACF7AC40AB87041` |
| `yarash` | Mage | `open5e-mage.json` | `2742260A80AAA1BD26AEF3A445FE230F91BD93EDE94A2D11638D839469B338C2` |
| `trapped-fighter`, `trapped-veteran` | Warrior Veteran | `open5e-warrior-veteran.json` | (above) |

Each keeps the original's Hit Points (33, 8, 27, 45 and 50). The Minotaur's
Gore, and the Mage's Fly and Protective Magic, are not modeled.

## Wilderness window monsters

The western and eastern wilderness windows' monsters
([west](../../../docs/audits/western-wilderness.md),
[east](../../../docs/audits/eastern-wilderness.md)). Each Open5E response was
fetched on 2026-10-25 (`srd-2024`):

| Rows | Stat block | Response | SHA-256 |
| --- | --- | --- | --- |
| `quickling` | Sprite | `open5e-sprite.json` | `F84D181922F983953E081C78947FB4BB3794BBE3AA73D6678AAB33A9E07CFA80` |
| `ankheg` | Ankheg | `open5e-ankheg.json` | `9656358FB2D77D4090876B514763BF3DECDE5BE0D8550C2D904FD7B171E75054` |
| `centaur` | Centaur Trooper | `open5e-centaur-trooper.json` | `0EDB6E44FCB9820868911176F8340C288974BE2933AC0495AC242D5B0800970B` |
| `venomous-snake` | Giant Venomous Snake | `open5e-giant-venomous-snake.json` | `05E93CF9FE16A5C9379D7738A555BA2DC0F020655FA3C554339A65710F5F764D` |
| `tiger` | Tiger | `open5e-tiger.json` | `541CD5AC63B5C32675B863ED283BC618F06E1523C45B09DD04E66A9F6A67AE6D` |
| `wild-boar` | Boar | `open5e-boar.json` | `375BDCEBEE3D8516BBE05E94281F4D578975542E743363AF9770E400D6D8570F` |
| `hippogriff` | Hippogriff | `open5e-hippogriff.json` | `862C3FA8A3B91CD45DCBC7D20C5A613294487E12E34049AA20BF74C0A306948C` |
| `wyvern` | Wyvern | `open5e-wyvern.json` | `27F3EC5A4E09EFE8F1609901413FBD418E3605B7C885992CBEEABC3818251F95` |
| `phase-spider` | Phase Spider | `open5e-phase-spider.json` | `E3C63FF10C063B2C08E55772FF0AAD6F6A6987AB57139AB25022C3E9FEE9CD5E` |
| `zhent-corporal` | Guard | `open5e-guard.json` | (above) |
| `giant-mantis` | Giant Scorpion's Claws | `open5e-giant-scorpion.json` | (above) |
| `thri-kreen` | Warrior Veteran | `open5e-warrior-veteran.json` | (above) |

Each keeps the original's Hit Points. New rows: `bloodied_fury`, `charge`,
`prone_on_hit`, `prone_large` and `nimble_escape`.

## Kobold caves monsters

The kobold caves' efreeti ([audit](../../../docs/audits/kobold-caves.md)). The
Open5E response was fetched on 2026-10-25 (`srd-2024`):

| Rows | Stat block | Response | SHA-256 |
| --- | --- | --- | --- |
| `efreeti` | Efreeti | `open5e-efreeti.json` | `B753821A4C618D2DBC9E9D43B17FDF11F7DAA88BAABEF9AFE7E2CED90C0CE763` |

It keeps the original's 55 Hit Points.

## Valhingen Graveyard monsters

Valhingen Graveyard's undead and wolves
([audit](../../../docs/audits/valhingen-graveyard.md)). Each Open5E response was
fetched on 2026-10-25 (`srd-2024`):

| Rows | Stat block | Response | SHA-256 |
| --- | --- | --- | --- |
| `wraith` | Wraith | `open5e-wraith.json` | `0485B8649AC9463F89314E1B34A0F0E872C2B98374EFF4B8043D8D3789C0E98E` |
| `giant-skeleton` | Minotaur Skeleton | `open5e-minotaur-skeleton.json` | `03C0A26E0750667E63B8018B94ED52FC057080A2EB9A05D1F4CF13103E87487E` |
| `juju-zombie` | Ogre Zombie | `open5e-ogre-zombie.json` | `79F55B379EFB00AE68C88DE72F1206DDC2FA1116E54DC55BAD38451E6EAF48E0` |
| `mummy` | Mummy | `open5e-mummy.json` | `BF89F266789B7D87E2E913500169B0DA28DE46FE3D0E4FE36552732026987B25` |
| `vampire`, `weakened-vampire` | Vampire | `open5e-vampire.json` | `97A9678B91671AFC175FD86925C1BB53CB7413BBD01441CA5185942E6668E02C` |
| `wolf` | Wolf | `open5e-wolf.json` | `F68A8D97592A10620F342A2CA97CD449F62D18C8D338098EBABE594B08E434B8` |

Each keeps the original's Hit Points. Where this response and the SRD 5.2.1
PDF differ, the rows follow the PDF: the Wraith's Bludgeoning, Piercing and
Slashing Resistances, the Minotaur Skeleton's Bludgeoning Vulnerability and
the Mummy's Fire Vulnerability.

## Stojanow Gate monsters

Stojanow Gate's giants ([audit](../../../docs/audits/stojanow-gate.md)). Each
Open5E response was fetched on 2026-10-25 (`srd-2024`):

| Rows | Stat block | Response | SHA-256 |
| --- | --- | --- | --- |
| `hill-giant` | Hill Giant | `open5e-hill-giant.json` | `E80F67FCB3E710E1E448478462D794079D704E6C91F0DEC0FB1D9256F45BBC7D` |
| `ettin` | Ettin | `open5e-ettin.json` | `B566FCA1D317770D6DE4262C105B995073FE97E54577752716E307ADE21B8862` |

Each keeps the original's Hit Points.

## Valjevo old town monsters

The old town's fire giants ([audit](../../../docs/audits/valjevo-old-town.md)).
The Open5E response was fetched on 2026-10-25 (`srd-2024`):

| Rows | Stat block | Response | SHA-256 |
| --- | --- | --- | --- |
| `fire-giant` | Fire Giant | `open5e-fire-giant.json` | `AB2611D8384D2FCD65DA9667CD5B6E3D1E35EB384661CB1B5B4CE020B27BA31A` |

It keeps the original's 59 Hit Points.

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
