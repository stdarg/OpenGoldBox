# Deviations from the original game

OpenGoldBox follows Pool of Radiance first ([PRD](PRD.md)). This page lists
every deliberate departure from the original game, with the decision that
records it. Changes the SRD makes to a rule (a spell left out, no spell
components) are not listed here; [SRD-DECISIONS.md](SRD-DECISIONS.md) records
those. Missing features and bugs are not deviations.

Add an entry whenever a new departure is decided.

## Rules and combat

- **Rules system** (2026-09-06). The original used AD&D combat calculations,
  class restrictions and spell memorization. OpenGoldBox uses SRD 5.2.1 behind
  a replaceable rules library. Campaign scripts and encounter identities and
  counts are kept through conversion adapters.
  [PRD §1](PRD.md#1-product-summary), [RULES.md](RULES.md).
- **Battle squares** (2026-10-15). In the original a combat square is 10
  feet: the manual's "Protection From Evil, 10' Radius" reaches "everyone
  within 1 square", and a Lightning Bolt (40 or 80 feet in AD&D) is "4 or 8
  squares long" (Manual pp. 26–27). OpenGoldBox keeps the original's
  battlefields square for square but counts each square as 5 feet, the SRD's
  grid, so every Speed, range and area is measured in 5-foot squares. One
  exception: Revivify reaches 10 feet, two squares
  ([LEVELS-2](SRD-DECISIONS.md#levels-2-2026-10-15-level-cap-15)).
  [RULES.md](RULES.md).
- **Monsters** (2026-10-02). The original's AD&D monster records become SRD
  stat blocks, unchanged.
  [MON-1](SRD-DECISIONS.md#mon-1-2026-10-02-original-monsters-as-srd-stat-blocks).
- **Encounter size** (2026-10-02). The original ran its groups as written.
  OpenGoldBox shrinks an encounter over the SRD budget or with more creatures
  than living characters, and never grows one. Experience and treasure stay the
  original encounter's. [MON-1](SRD-DECISIONS.md#mon-1-2026-10-02-original-monsters-as-srd-stat-blocks).
- **Difficulty** (2026-10-21). The original had no difficulty setting.
  OpenGoldBox has five levels, Easy to Infernal. Its monsters used their
  readied wands at all times and never a potion or scroll; here monsters use
  their potions, wands and scrolls on Hard and above and no items on Normal and
  below. [DIFF-1](SRD-DECISIONS.md#diff-1-2026-10-21-difficulty-and-monsters-that-use-their-items).
- **Empty fights** (2026-10-10). With no room for a whole band, the original
  started an empty fight the party won at once. OpenGoldBox places the band's
  first monster on the nearest free square the party can reach.
  [PLACE-1](SRD-DECISIONS.md#place-1-2026-10-10-starting-positions-as-the-original-game-had-them).
- **Flee button** (2026-10-08). The original had no flee command; a member
  fled by moving off the battlefield's edge, which OpenGoldBox keeps. A
  **Flee** button beside End turn hands the party to the AI to run for the
  edges. [FLEE-1](SRD-DECISIONS.md#flee-1-2026-10-08-fleeing-a-fight-as-the-original-game-did).
- **Flame Tongue** (2026-10-17). The original's Long Sword +2 Flame Tongue
  becomes the SRD's Flame Tongue with its +2 kept: a Bonus Action lights the
  blade for an extra 2d6 Fire damage. Like every item, it needs no
  attunement ([MAGIC-4](SRD-DECISIONS.md#magic-4-2026-10-20-no-attunement)).
  [MAGIC-1](SRD-DECISIONS.md#magic-1-2026-10-17-the-originals-enchanted-weapons-and-armor).
- **Charged items** (2026-10-19). The original's wands, potions and other
  charged items keep their charges but work as their SRD 5.2.1 counterparts:
  Potion Extra Healing's three small doses are three Potions of Greater
  Healing, the Necklace of Missiles' missiles are 8d6 Fireballs, the Javelin of
  Lightning's bolt returns after a Long Rest instead of being spent, and the
  Manual of Bodily Health gives +2 Constitution.
  [MAGIC-3](SRD-DECISIONS.md#magic-3-2026-10-19-wands-potions-and-other-charged-items).
- **Identifying with the Identify spell** (2026-10-18). In the original only
  a shop's ID revealed a magic item's name. OpenGoldBox keeps it and adds the
  SRD's Identify spell.
  [MAGIC-2](SRD-DECISIONS.md#magic-2-2026-10-18-identifying-magic-items).
- **Worn items** (2026-10-20). A Ring of Protection adds its bonus to saving
  throws as well as Armor Class, and it keeps working beside magic armor, which
  in the original cancelled it. Gauntlets of Ogre Power give the SRD's
  Strength 19 instead of 18/00. The second Cloak of Displacement record, which
  gives no Armor Class in the original, gives the same +2 as the first.
  [MAGIC-5](SRD-DECISIONS.md#magic-5-2026-10-20-rings-bracers-and-other-worn-items).
- **Spell scrolls** (2026-10-20). A scroll's spells are SRD 5.2.1 spells, with
  stand-ins for the five SRD 5.2.1 lacks, cast at the SRD scroll's DC. In the
  original a scroll had to be readied in both hands; here a spell is read
  from the inventory or the Items menu. Druids and Rangers read cleric
  scrolls. The original's magic-user and cleric spells SRD 5.2.1 lacks are
  Wizard and Cleric spells as stand-ins. Copying a spell into a Wizard's spellbook, free in the original,
  takes two hours a spell level and a bottle of Magic Ink, a new item, of the
  spell's level or higher, at 1000 gp a level. Casters also write scrolls of
  their own spells in camp, which the original never allowed. Read Magic reveals all the caster's scrolls at once instead of each
  scroll opened while it lasts.
  [MAGIC-6](SRD-DECISIONS.md#magic-6-2026-10-20-spell-scrolls).
- **Alignment-bound swords** (2026-10-22). The original's three bound swords
  served only their exact alignment (Lawful Good, Lawful Evil). Here only the
  good-evil half counts: any Good member wields the Lawful Good sword, any Evil
  member the Lawful Evil ones. The damage is the original's, as Force.
  [MAGIC-7](SRD-DECISIONS.md#magic-7-2026-10-22-alignment-bound-swords).
- **Fire and Acid** (2026-10-08). New Phlan's general store also sells Acid and
  Alchemist's Fire at SRD prices, and every starting kit has a Torch, so any
  party can keep a troll down.
  [GEAR-1](SRD-DECISIONS.md#gear-1-2026-10-08-fire-and-acid-for-every-party).

- **Computer play** (2026-10-21). The original's AI walked toward an enemy it
  could not see, rolled each ally's saving throw to judge an area spell (so
  monsters cast nearly regardless), and drew three random spells at each
  priority. OpenGoldBox's computer waits until it sees an enemy, keeps every
  caster's allies out of its areas without a hidden roll, picks one spell at
  random among those that make sense, treats cantrips as attacks, never casts a
  helpful spell twice on one creature, counts a weapon's melee attacks a turn
  in its rating, and makes its random choices from the fight's state so a fight
  replays the same. The original breathed by chance each round; a Breath Weapon
  here goes off only where it catches an enemy and no ally.
  [AI-1](SRD-DECISIONS.md#ai-1-2026-10-21-computer-play-as-the-original-game-had-it),
  [AI.md](AI.md).

## Party and parley

- **Party leader** (2026-10-08). The original asked who speaks at every parley.
  OpenGoldBox has a standing leader, who parleys with twice their Charisma and
  buys first in shops.
  [LEADER-1](SRD-DECISIONS.md#leader-1-2026-10-08-a-party-leader-speaks-for-the-party).
- **Ready-made characters** (2026-10-12). The original had only the characters
  a player created. OpenGoldBox adds a Character Pool of ready-made
  characters, who start with gold and no equipment like any new character.
  [START-1](SRD-DECISIONS.md#start-1-2026-10-11-starting-gold-as-the-original-game-gave-it).

## Money

- **Starting gold** (2026-10-11). Every class starts with the manual's
  3d6 × 10 gp, not START.EXE's per-class dice, whose use in Pool of Radiance is
  unconfirmed. [START-1](SRD-DECISIONS.md#start-1-2026-10-11-starting-gold-as-the-original-game-gave-it).
- **Party gold** (2026-10-13). The original kept a purse per character, pooled
  for party purchases. OpenGoldBox keeps one purse for the party that pays for
  everything; scripts see it as every member's money
  ([#232](https://github.com/stdarg/OpenGold/issues/232)). The user confirmed on
  2026-10-22 that the party's money is always pooled and coins weigh nothing,
  where the original counted them toward a character's load; the original's
  Take, Pool and Share commands are not needed.
  [TRAINING-HALL.md](TRAINING-HALL.md#decisions-2026-10-13),
  [PHLAN.md](PHLAN.md#coin-payments).

## Advancement and training

All decided 2026-10-13 and recorded in
[TRAINING-HALL.md](TRAINING-HALL.md#decisions-2026-10-13). The halls are
in the game: answering YES at a hall lists its services in the dialogue
([At the hall](TRAINING-HALL.md#at-the-hall)).

- **Levels anywhere.** The original raised levels only at a training hall, for
  1,000 gp. OpenGoldBox raises them from the roster's Advance button anywhere,
  up to level 15 for every class ([LEVELS-2](SRD-DECISIONS.md#levels-2-2026-10-15-level-cap-15)), in place of the hall's
  original limits of 8th level fighter, 9th level thief and 6th level cleric
  and magic-user ([CAP-1](SRD-DECISIONS.md#cap-1-2026-10-14-the-original-training-halls-limits)).
- **Halls sell experience.** One session per level grants half the experience
  to the next level, for 1,000 gp at level 1, doubling at each level.
- **Halls train skills.** Skills from the hall's classes' lists: proficiency
  250 gp, Expertise 500 gp, one per level. Tools and languages, also
  approved, wait on DM-1, which removed them from the game.
- **Experience kept.** The original clamped experience to just below the next
  threshold after training. OpenGoldBox keeps it all.
- **Hall classes.** The original's four halls train the 12 SRD classes by
  AD&D 2nd edition's class groups.

## Rest

- **Short and Long Rests** (2026-09-30). Camp offers the SRD's Short and Long
  Rests in place of the original's rest. An interrupted rest ends and grants
  nothing. Camp restrictions and the paid inn rest stay the original's.
  [Rest workflow approval](SRD-DECISIONS.md#rest-workflow-approval),
  [SIMPLIFY-1](SRD-DECISIONS.md#simplify-1-2026-09-30-tabletop-time-and-body-simulation).
- **Camp interruptions.** The original reset its interruption count on loading
  and rolled with the DOS random generator. OpenGoldBox saves the count and
  rolls with the saved script random state, so reloading cannot reroll a camp.
  [RECOVERY.md](RECOVERY.md#camp-interruption-evidence).

- **Podol Plaza** (2026-10-22). The Pitt's buccaneer, a 4th-level fighter
  in the original, is the SRD Pirate; wizard-locked doors open to Knock or a
  DC 35 Bash or Pick, where the original needed magic or exceptional Strength;
  robbery takes coins but no items; none of the free-for-all's monsters goes
  berserk; and a cancelled auction with no group rolled yet meets a fresh
  plaza group.
  [PODOL-1](SRD-DECISIONS.md#podol-1-2026-10-22-podol-plaza).
- **Temple services** (2026-10-22). The original's temples cast AD&D spells.
  Each service now casts an SRD 5.2.1 spell under its original name: Cure
  Serious and Cure Critical Wounds are Cure Wounds from a level-3 and a level-5
  slot, the three other cures Lesser Restoration, Stone to Flesh Greater
  Restoration, and Raise Dead returns a member with 1 Hit Point and −4 to D20
  Tests. On Easy and Normal the temples also cast Resurrection, which the
  original's did not. Each temple may have its own prices, edited with the
  Shop & Temple Manager; they start at the original's, which were the same
  everywhere.
  [TEMPLE-1](SRD-DECISIONS.md#temple-1-2026-10-22-temple-services).
- **No ammunition** (2026-09-25, 2026-10-23). Bows, crossbows and slings shoot
  without ammunition, and there are no arrows or quarrels: no shop, treasure or
  monster gives any, a random roll for them is rolled again, and Arrow +1 is
  gone. A shot has the bow's own magic. Darts and javelins stay as thrown
  weapons, and a throw never spends them.
  [AMMO-UNLIMITED](SRD-DECISIONS.md#unlimited-ranged-ammunition--approved-exception).
- **Shop stock and prices** (2026-10-23). Each New Phlan shop sells what its
  `data/shops` record lists, at its prices, with its shopkeeper's portrait,
  all edited with the Shop & Temple Manager. The records start from the
  original's lists and values, plus the gear the game adds to general stores.
  Stock limits are not kept yet: every item a shop sells is unlimited, as in
  the original.
  [#275](https://github.com/stdarg/OpenGold/issues/275).

## Interface and saves

- **Modern shell.** A Godot interface that follows the Gold Box screens'
  structure without their DOS-era limits.
  [PRD §5.2](PRD.md#52-modern-shell-original-game).
- **Save files.** OpenGoldBox uses its own versioned save format. Importing
  original saves is wanted but not yet supported. [PRD §14](PRD.md#14-save-games), [SAVES.md](SAVES.md).
- **New wall art.** The first-person view draws the new wall art in
  `data/art/walls`, at four times the original resolution, in place of each
  original appearance it covers. Appearances without new art keep the
  original pieces from the player's game files.
  [#272](https://github.com/stdarg/OpenGoldBox/issues/272),
  [data/art/walls](../data/art/walls/README.md).
- **Monsters from the Combat Sprite Catalog** (2026-10-23). A creature the
  catalog exported to `data/monsters` fights under the catalog's name, in its
  new art for the weapon set it readies, and carries only the original items
  the catalog kept: an item left out is neither used in the fight nor left
  behind. Gear the catalog added as an original item type is carried and left
  behind as a plain item of that type, when New Phlan's shop lists hold one. A
  human-shaped creature hits with the weapon it holds, at its stat block's
  bonuses ([MON-1](SRD-DECISIONS.md#mon-1-2026-10-02-original-monsters-as-srd-stat-blocks)).
  A creature without new art shows its original icon with its action
  pose. [#274](https://github.com/stdarg/OpenGoldBox/issues/274),
  [data/monsters](../data/monsters/README.md).
