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
  [PRD §1](PRD.md#1-product-summary), [RULES.md](RULES.md). Since 2026-11-11
  the SRD rules are an option: the original rules are the default, and a new
  game's first question is which rules it plays by
  ([ORIGINAL-RULES.md](ORIGINAL-RULES.md), being built). Departures within the
  original rules are listed under [The original rules](#the-original-rules).
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
- **Targets** (2026-10-29). In the original a creature kept its target, else
  took a random enemy in view, and never read Hit Points or AC. OpenGoldBox
  keeps that on Easy and Normal; above Normal every creature the computer
  plays goes after the weakest enemy, and on Nightmare and Infernal they all
  go after the same one and finish downed characters.
  [CLOSE-1](SRD-DECISIONS.md#close-1-2026-10-29-the-last-srd-choices).
- **Ogre leader's squares** (2026-10-29). Each monster fills the squares its
  record names, as in the original, but the ogre leader's record names one
  square while its art stands two tall. OpenGoldBox gives it two squares, one
  above the other, like every other ogre.
  [FOOT-1](SRD-DECISIONS.md#foot-1-2026-10-26-creatures-fill-the-squares-their-art-fills).
- **Tyranthraxus's guards** (2026-11-03). The original's 8th level fighters
  have 87 Hit Points. OpenGoldBox gives them the SRD Knight's 52: the encounter
  sizing rates each at the Knight's 700 XP, so a level-10 party of eight met
  eight 87-Hit-Point guards and won one fight in two to one in eight. With 52 the
  party wins nine in ten or more and keeps about half its Hit Points for
  Tyranthraxus. The old town's 8th level fighters are the same creature.
  [CASTLE-1](SRD-DECISIONS.md#castle-1-2026-10-25-valjevo-castle).
- **Encounter size** (2026-10-02). The original ran its groups as written.
  OpenGoldBox shrinks an encounter over the SRD budget or with more creatures
  than living characters, and never grows one. Experience and treasure stay the
  original encounter's. [MON-1](SRD-DECISIONS.md#mon-1-2026-10-02-original-monsters-as-srd-stat-blocks).
- **Difficulty** (2026-10-21). The original had no difficulty setting.
  OpenGoldBox has five levels, Easy to Infernal. Its monsters used their
  readied wands at all times and never a potion or scroll; here monsters use
  their potions, wands and scrolls on Hard and above and no items on Normal and
  below. [DIFF-1](SRD-DECISIONS.md#diff-1-2026-10-21-difficulty-and-monsters-that-use-their-items).
  Each level also sets how many monsters a fight has, their Hit Points, attack rolls
  and morale, and Easy spares the party after a win and always lets it flee (2026-11-02,
  [DIFF-2](SRD-DECISIONS.md#diff-2-2026-11-02-each-level-changes-the-fights-not-only-the-tactics)).
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
- **Magic items are seen to be magic** (2026-11-05). In the original an
  unidentified magic sword was only a "Long Sword". OpenGoldBox names it "Long
  Sword (unidentified magic)" until it is identified; its bonus and powers stay
  hidden.
  [EQUIP-1](SRD-DECISIONS.md#equip-1-2026-11-05-auto-equip-better-gear).
- **Auto-equip** (2026-11-05). In the original a character put on gear only
  when the player readied it. OpenGoldBox, unless the Options screen turns it
  off, puts a better weapon, armor, shield, ring, necklace, bracers, cloak,
  gauntlets or boots on the character who gains it, by
  each character's Maximize defense or Maximize damage.
  [EQUIP-1](SRD-DECISIONS.md#equip-1-2026-11-05-auto-equip-better-gear).
- **Selling and dropping** (2026-11-05). The original's Sell and Drop work
  on any item but a cursed one, which stays even when it is only in the pack
  and not yet known to be cursed. A shop pays half what the shops sell the
  item for, not half the item's own stored value: a monster's Short Sword
  fetches the same as a bought one (2026-11-07). The higher Charisma of the
  seller and the leader adds or takes 10% a point, a shop that sells nothing
  of the item's type pays 25% less, and an unidentified item fetches half its
  unenchanted version's price. A partly used stack sells for its share, and a
  stack goes whole.
  [SELL-1](SRD-DECISIONS.md#the-sale-price).
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
- **Poisoner's Kit, Basic Poison and Thieves' Tools** (2026-10-22, 2026-10-24).
  Three SRD items the original lacks: a Rogue carrying a Poisoner's Kit can
  poison with Cunning Strike, a vial of Basic Poison coats a weapon, and a Rogue
  carrying Thieves' Tools has twice the chance to pick a lock
  ([LOCK-1](SRD-DECISIONS.md#lock-1-2026-10-24-thieves-tools-double-the-chance-to-pick-a-lock)).
  No shop sells them by default; the Shop & Temple Manager can add them to any
  shop, and can make a shop a Rogue's Shop or a Magic User's Shop
  ([#271](https://github.com/stdarg/OpenGoldBox/issues/271)).

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
  [AI.md](AI.md). Weapon choice rises with the difficulty (2026-10-31): the
  original's rule is Normal; Easy chooses at random; Hard and up use the weapon
  with the highest expected damage, its chance to hit and the close-range
  Disadvantage counted; Nightmare also keeps its distance; Infernal also picks
  the target where the attack stops the most harm and always casts its strongest
  spells. A party member on Quick always plays at Infernal, where the original's
  Quick characters played as its monsters did; above Normal a creature walks only
  for an enemy it can strike this turn, or else the nearest.
  On Nightmare and Infernal a downed character is struck only when no standing
  enemy can be (2026-11-01). The original's creature gave up after being
  blocked three times; one here whose way in is held by its own side goes
  around the creatures to another free side of the enemy (2026-11-03).
- **Quick ends with the fight** (2026-11-07). The original kept a member on
  Quick in later fights until the player took control. OpenGoldBox returns
  every member to the player when a fight ends; the Options screen's Keep
  Quick after a fight (off by default) keeps the original's way
  ([QUICK-1](SRD-DECISIONS.md#quick-1-2026-10-09-quick-combat-as-the-original-game-had-it),
  [CONFIGURATION](CONFIGURATION.md)).

- **Hearing in combat** (2026-10-31). The original's monsters went only for what
  they could see. A creature here also hears enemies within 100 feet, through
  walls ([AI.md](AI.md#1-movement-and-sight)).
- **Dice shown** (2026-11-03, the user). The original showed no dice, only who
  hit and how hard. The fight's log shows each attack's d20 and, after a damage
  total, the dice behind it ("hits for 14 damage (2d8: 4 + 6, + 4 = 14)"); out of
  fights the panel shows the party's rolls: searches, door checks, arrows and
  traps, with their saving throws. Rolls the original kept hidden, such as
  wandering-monster checks, stay hidden and go only to the log file
  ([TECH.md](TECH.md#diagnostic-log)).
- **A reload's dice** (2026-11-01). On Easy every roll replays after a reload.
  Above Easy a load draws new dice from the clock, so what comes next cannot be
  known ([RULES.md](RULES.md)).
- **A monster that cannot get away charges in panic** (2026-11-01, MORALE-2). In the
  original a broken monster that failed to get away stayed and fought on, and could
  stand at the far edge of the field while nothing happened. Here it tries once a
  turn, and after two turns of getting nowhere it runs at the party, wherever the
  party is, and strikes at random at whoever is in reach, friend or foe, with
  Disadvantage, while attacks on it have Advantage. It has no Bonus Actions or
  Reactions and casts no spells. Confused creatures gave the model, without their
  turn-wasting d10.

## The original rules

Departures from the original game's own rules, when OpenGoldBox plays by them
([ORIGINAL-RULES.md](ORIGINAL-RULES.md)).

- **Racial ability adjustments** (the user, 2026-11-11). The original's
  creation never applied them: a leftover variable made every character roll
  as a human. OpenGoldBox adds them to each 3d6 roll as the original meant
  to: a dwarf +1 Constitution and −1 Charisma, an elf +1 Dexterity and −1
  Constitution, a halfling −1 Strength and +1 Dexterity, a half-orc +1
  Strength, +1 Constitution and −2 Charisma. An elf can now have a Dexterity
  of 19, and a dwarf a Constitution of 19.
- **Dexterity above 18** (the user, 2026-11-12). In the original, 19 and 20
  are worth no more in a fight than 18. OpenGoldBox starts each higher step two
  points sooner:

  | Dexterity | 18 | 19–21 | 22–25 |
  |---|---|---|---|
  | Armor Class | 4 better | 5 better | 6 better |
  | Missile to-hit and initiative | +3 | +4 | +5 |

  The original gave 5 better and +4 from 21, and 6 better and +5 from 24.
- **Thief skills and Dexterity** (the user, 2026-11-12). Pick Pockets at
  Dexterity 10 is −10 (the original's −19) and Open Locks at 16 is +5 (its −5),
  in step with their neighbors. The original's table covers Dexterity 9 to 19;
  lower scores use its 9 row and higher ones its 19 row. No chance falls below
  0, where the original wrapped around past 200%.
- **Aging** (the user, 2026-11-12). Each game chooses Off, As the original or
  AD&D when it starts ([ORIGINAL-RULES.md](ORIGINAL-RULES.md#decisions)). The
  original only changed the scores at creation, by the starting age, and left
  out AD&D's +1 Constitution for young adults. Off and AD&D depart from it:
  Off ignores age, and AD&D adds that Constitution and changes the scores again
  as a character grows older, also with the game's years.
- **Damage below zero** (the user, 2026-11-12). The original adds a hit's
  dice and its modifier in a byte, so a total below zero (a 1d4−2 attack
  rolling 1) wraps around to more than 250 damage and kills anything; its own
  check for damage below zero can never succeed. OpenGoldBox counts such a hit
  as 0 damage.
- **Armor and movement** (the user, 2026-11-12). The original set a creature's
  movement to 9 in armor of 151 to 399 coins and 6 in heavier armor, whatever
  its own movement, so a slower creature moved faster in armor. Armor slows a
  creature only by its weight in the load it carries; the load's tables
  ([ORIGINAL-RULES.md](ORIGINAL-RULES.md#encumbrance)) only ever slow.
- **Distance and missiles** (the user, 2026-11-12). The original made a
  missile's target harder to hit with distance, and for a weapon of range 3 or
  less it counted every target as far, even next to the shooter. A target
  within the weapon's range has no distance penalty.
- **Bandaging** (the user, 2026-11-12). The original's Bandage saved the first
  dying party member anywhere on the field. The one who bandages must stand
  next to the dying member.

## Party and parley

- **Party leader** (2026-10-08). The original asked who speaks at every parley.
  OpenGoldBox has a standing leader, who parleys with the original's reaction
  score for their Charisma and buys first in shops.
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
  ([#232](https://github.com/stdarg/OpenGoldBox/issues/232)). The user confirmed on
  2026-10-22 that the party's money is always pooled and coins weigh nothing,
  where the original counted them toward a character's load; the original's
  Take, Pool and Share commands are not needed. Every shop, temple, inn and
  training hall takes any coin and gives the correct change (user's decision,
  2026-11-04), and the town shows the party's money as its coins' worth in gold.
  [TRAINING-HALL.md](TRAINING-HALL.md#decisions-2026-10-13),
  [PHLAN.md](PHLAN.md#coin-payments).
- **Treasure pile** (2026-11-05, user's request). The original's after-combat
  Treasure Menu (View, Take, Pool, Share, Detect, Exit) becomes a shop-like
  list of the items: the selected member takes the highlighted one. Coins go
  straight to the party's purse. Leaving with items left asks first, as the
  original did. [EXPEDITION.md](EXPEDITION.md).

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
  AD&D 2nd edition's class groups. A multiclass character may use the hall of
  any of its classes ([MULTI-1](SRD-DECISIONS.md#multi-1-2026-11-05-multiclassing)).
- **Multiclassing** (2026-11-05). In the original only non-human characters
  could be multi-class: they chose their classes at creation, split every
  experience award between them and averaged their hit points. OpenGoldBox
  follows SRD 5.2.1 instead: any character, of any species, may take its next
  level in another class whose 13s it meets, with one experience pool and
  each class's own die
  ([MULTI-1](SRD-DECISIONS.md#multi-1-2026-11-05-multiclassing)). The original's
  scripts see such a character as each of its classes.

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
- **Cadorna Textile House** (2026-10-24). Its monsters are SRD stat blocks; the
  Wight and the Large Scorpion keep the original's Hit Points (23 and 10, the
  latter on the Giant Scorpion's stat block). Only a Rogue in Light or no
  armor can climb down the dry well, with a Strength (Athletics) check, where
  the original rolled a thief's climb-walls chance. The clerk pays Cadorna's
  20 gems for the family treasure brought back sealed, which the cluebook
  promises but the original's scripts never set.
  [CADORNA-1](SRD-DECISIONS.md#cadorna-1-2026-10-24-the-cadorna-textile-house).
- **Mendor's Library** (2026-10-24). Its monsters are SRD stat blocks with the
  original's Hit Points; the Spectre drains Hit Point maximums instead of
  levels, and the Basilisk's gaze Restrains before it Petrifies. The mad
  fighter who joins is a level-1 Fighter. The green slime allows an SRD
  saving throw (Constitution, DC 13) where the original rolled its own.
  [MENDOR-1](SRD-DECISIONS.md#mendor-1-2026-10-24-mendors-library).
- **The wilderness map** (2026-10-24). The displacer beast, which the SRD
  lacks, fights as an Owlbear; wilderness fights are drawn on the original's
  field.
  [OVERLAND-1](SRD-DECISIONS.md#overland-1-2026-10-24-the-wilderness-map).
- **Kovel Mansion** (2026-10-24). Casting Knock on a locked cabinet needs a
  member with the spell. The original looked only at the first member's
  memorized spells, a slip in its code; any member may cast it here (the
  user's choice, 2026-11-08). Thief skills the
  scripts roll are a Rogue's Sleight of Hand checks. A trap that hurts the
  whole party rolls each member's damage on its own.
  [KOVEL-1](SRD-DECISIONS.md#kovel-1-2026-10-24-kovel-mansion).
- **Dirten's armor** (2026-10-24). The bishop's priest wears a Chain Shirt,
  the chain an SRD Cleric is trained in, where the original gave him chain
  armor. [BANE-1](SRD-DECISIONS.md#bane-1-2026-10-24-the-temple-of-bane).
- **The healed valley** (2026-10-24). When Yarash's pollution stops, the
  wilderness's rivers open, but its valley tiles keep their art; the
  original recoloured them.
  [PYRAMID-1](SRD-DECISIONS.md#pyramid-1-2026-10-24-the-pyramid).
- **One member robbed** (2026-10-25). A script that robs one member (the
  kobold caves' pool) takes that member's share of the party's shared purse;
  the original took from that member's own coins and items.
  [KOBOLD-1](SRD-DECISIONS.md#kobold-1-2026-10-25-the-kobold-caves).
- **Rune scrolls** (2026-10-24). Words a script shows in runes (Sokal Keep's
  elf scroll) appear translated; the original needed the box's code wheel.
  [SOKAL-1](SRD-DECISIONS.md#sokal-1-2026-10-24-sokal-keep).
- **Sunlight Sensitivity** (2026-10-24). Kobolds and Wights have Disadvantage
  on attack rolls and ability checks on a square open to the sky by day; the
  original had no sunlight.
  [CADORNA-1](SRD-DECISIONS.md#cadorna-1-2026-10-24-the-cadorna-textile-house).
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
  [#275](https://github.com/stdarg/OpenGoldBox/issues/275).
- **The Rogue's Shop** (2026-10-29). The original has no rogue's shop and no
  secret door in New Phlan. OpenGoldBox opens one at (4,13), the back half of
  the inn at (4,12): a wall now closes it from the inn, whose event no longer
  runs there, and its way in is a secret, locked door in the south wall, from
  the alley. The party finds the door with the original's search rolls, a
  Rogue picks it or a member casts Knock, and once open it stays open. It
  sells Thieves' Tools, the Poisoner's Kit and Basic Poison, and magic items,
  which no shop of the original sells: Magic Lock Picks (OpenGoldBox's own),
  the original's Dagger, Short Sword and Short Bow +1 and Dust of
  Disappearance, Leather Armor +1, and the SRD's Boots and Cloak of
  Elvenkind, Stone of Good Luck and Wand of Secrets, which also points out
  the Pyramid's and Kuto's Well's secret doors and Kovel Mansion's and the
  kobold caves' traps.
  [SECRET-1](SRD-DECISIONS.md#secret-1-2026-10-29-the-rogues-shop-and-its-secret-door),
  [#271](https://github.com/stdarg/OpenGoldBox/issues/271).

## Interface and saves

- **Shops name their keepers and tell what an item does** (2026-11-08). The
  original's shops list their stock in one column under the town's heading.
  OpenGoldBox names each shopkeeper (a random name, editable in the Shop &
  Temple Manager) and shows the name and the shop's type over the portrait,
  lists the stock in three columns, buys an item on a double click, and tells
  on hover a weapon's roll to hit Armor Class 13 and damage, or armor's Armor
  Class, for the chosen buyer ([PARTY](PARTY.md), [PHLAN](PHLAN.md)).
- **Shops list their stock alphabetically, and leaving one puts the party
  outside** (2026-11-09). The original lists a shop's items in its list's
  order and leaves the party standing in the shop's square. OpenGoldBox sorts
  the list by name, reading down each column, and after Leave shop the party stands on the square it came
  in from, its back to the door ([PHLAN](PHLAN.md)).

- **Portraits change at any time** (2026-11-11). The original chose a
  character's head and body once, at creation; only its combat icon could be
  changed later (Encamp, Alter, Icon). OpenGoldBox's **Change portrait…** on a
  member's sheet changes its portrait whenever the player likes. A character
  that never built an original portrait (one made while new portraits show, or
  saved before they could be chosen) gets a random head and body
  ([PARTY](PARTY.md)).

- **Tyranthraxus's name** (2026-11-01). The original's record for his human
  form misspells him TYRANITHRAXUS. OpenGoldBox spells it Tyranthraxus.
- **New art** (2026-10-29). The original drew its own walls and combat icons.
  OpenGoldBox shows new walls in the 3D view and new figures for monsters in a
  fight, and (2026-11-06) new animated close-ups wherever the Combat Sprite
  Catalog has art drawn from a close-up's written description; the Options
  screen's 3D view art and Combat art bring back the original game's, at once. Party members keep their figures either way.
  Characters have new portraits too, and (2026-11-11) the Options screen's
  Portraits bring back the original's head-and-body portraits, which also show
  for any new portrait that cannot be loaded
  ([#274](https://github.com/stdarg/OpenGoldBox/issues/274),
  [CONFIGURATION](CONFIGURATION.md)). Since 2026-11-12 the new art is a
  separate, optional download, chosen on the Options screen's **New art
  folder**; without it the game shows the original game's art, and every
  character its original portrait.
- **Modern shell.** A Godot interface that follows the Gold Box screens'
  structure without their DOS-era limits.
  [PRD §5.2](PRD.md#52-modern-shell-original-game).
- **Save files.** OpenGoldBox uses its own versioned save format. Importing
  original saves is wanted but not yet supported. [PRD §14](PRD.md#14-save-games), [SAVES.md](SAVES.md).
- **New wall art.** The first-person view draws the new wall art from the new
  art download (`new-art-pack/walls` in the repository), at four times the original resolution, in place of each
  original appearance it covers. Appearances without new art keep the
  original pieces from the player's game files.
  [#272](https://github.com/stdarg/OpenGoldBox/issues/272),
  [new-art-pack/walls](../new-art-pack/walls/README.md).
- **Party members show the weapon they attack with** (2026-11-02). A character holding
  a sword draws its carried bow to shoot, drawn by the same equipment compositor as
  its other sprites, and shows its sword again when it strikes, without the shield if
  it took that off. The original showed one icon throughout a fight.
- **Monsters from the Combat Sprite Catalog** (2026-10-23). A creature the
  catalog exported to `data/monsters` fights under the catalog's name, in its
  new art for the weapon set it readies, and switches to the set it attacks
  with: its sword and shield to strike, its bow to shoot (2026-10-30), where
  the original showed one icon throughout. It carries only the original items
  the catalog kept: an item left out is neither used in the fight nor left
  behind. Gear the catalog added as an original item type is carried and left
  behind as a plain item of that type, when New Phlan's shop lists hold one. A
  human-shaped creature hits with the weapon it holds, at its stat block's
  bonuses ([MON-1](SRD-DECISIONS.md#mon-1-2026-10-02-original-monsters-as-srd-stat-blocks)).
  It shoots or throws only with a ranged weapon in its gear (2026-10-30),
  though its SRD stat block has a shot; spells and a giant's rocks need none.
  One holding a shield spends its Action taking it off before it shoots a
  weapon that needs both hands, as a character does (2026-11-01), and has 2 lower
  Armor Class with it off (2026-11-02), though not every stat block's Armor Class
  counted the catalog's shield. The shield stays off for the fight; until the
  catalog has art without it, the creature shows it again when it next strikes
  in melee.
  Quicklings carry nothing, so they fight in melee only and never charm, though
  the SRD Sprite standing in for them has a charming bow.
  A creature without new art shows its original icon with its action
  pose. [#274](https://github.com/stdarg/OpenGoldBox/issues/274),
  [data/monsters](../data/monsters/README.md).
- **Missiles that miss fly on** (2026-10-30). Shots and throws fly with the
  original's `COMSPR.DAX` art: an arrow for bows and crossbows, the axe for every
  thrown weapon, a stone for slings, a flask for thrown oil, acid and alchemist's
  fire. A hit ends on the target. A miss sails four
  squares past it, which is new. The next creature acts once the shot has landed.
- **Giants throw the rock they hold** (2026-11-03). A thrown boulder is the rock
  from the hill giant's new ready pose, cut from that picture and drawn twice the
  size it is in the giant's hand, tumbling a quarter turn as it flies, not the original's
  `COMSPR.DAX` boulder. The demo, which ships no new monster art, keeps the
  original's.
- **Every spell has its own look** (2026-10-31). Each spell cast in a fight plays
  its own animated effect, drawn by the game rather than taken from the original's
  art: Magic Missile sends glowing darts, Fireball a comet that bursts across its
  whole area, Sleep drifting Z's, Hideous Laughter a stream of grinning faces. An
  effect fills the squares the spell really affects, spreading out from its center
  or its caster, and a ranged spell attack that misses flies past its target. Like a
  shot, the next creature waits for it to finish. The table is
  `src/OpenGoldBox/spell_effects.h`, the drawings `scenes/spell_effects.gd`.
