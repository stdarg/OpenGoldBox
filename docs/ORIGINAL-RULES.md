# The original rules

OpenGoldBox plays by the original *Pool of Radiance* rules by default: AD&D 1st
edition as the game applied them. The SRD 5.2.1 rules (see [RULES.md](RULES.md))
are an option, chosen when a new game begins. This page
records the decisions and the plan. It grows as each part is built.

The reference for the original rules is the reconstructed source of
*Pool of Radiance* 1.3 (github.com/talonius/pool-of-radiance). It is derived
from SSI's code and grants no license, so OpenGoldBox learns the rules from it
and writes its own code and text. It never copies the source.

## Decisions

The user decided these on 2026-11-11 and 2026-11-12:

- **The original rules are the default.** The SRD rules are an option.
- **The difficulty levels keep their names** (Easy, Normal, Hard, Nightmare,
  Infernal), but each set of rules has its own effects for them. The balance
  settings built so far (DIFF-2's table, `encounter_challenge`, the `[monsters]`
  ability switches) belong to the SRD rules. Under the original rules
  Normal is the original's own play (2026-11-14), and the other levels
  change the monsters only (2026-11-15), but in an *Unlimited Adventures*
  design, which plays its own levels, OpenGoldBox's difficulty table or
  both; see [Difficulty](#difficulty).
- **Look takes ten minutes** in every game, as each original's map menu
  L does (AdvanceTime(1, 2)), asked for on 2026-11-14.
- **Obvious bugs in the original are fixed, not kept.** Since 2026-11-14 an
  obvious one is fixed without asking first, listed in [Bugs](#bugs-in-the-original) and told
  to the user; one that may not be a bug, or has more than one fair fix, is
  put to the user first.
- **2026-11-14 answers, later:** scribing is the original's: a spell is
  marked and scribed at the next rest, the rest's four or six base hours
  shared by all its spells, and a quarter hour a spell level. Haste cast at
  caster level 16 to 31 ages its targets as any Haste does (Bugs). *Secret*'s
  +1 to hit with swords and bows is for elves only (Bugs).
- **2026-11-14 answers:** *Secret*'s Barkskin casts as Heal, as its table
  has it (kept as the original); a *Secret* paladin casts at its full paladin
  level (the original's fixed level 6 is a bug); *Pools of Darkness*'s
  cleric and thief saves at level 19 take level 20's row (Bugs).
- **The rules are chosen for each game** (2026-11-12): a new game's first
  question is which rules it plays by, and the choice is kept for that save.
  The player does not switch mid-game. A character belongs to its game's rules,
  and the Character Pool offers only characters made under the same rules.
  The choice stays hidden, with the SRD rules in effect, until the original
  rules can play a fight.
- **Races:** the original rules offer only the original's six races.
- **The original rules' difficulty levels** (2026-11-15) change the
  monsters only; the party is unchanged, and Normal is the original exactly:

  | | Easy | Normal | Hard | Nightmare | Infernal |
  |---|---|---|---|---|---|
  | Monsters' to-hit (THAC0) | −2 | — | +1 | +2 | +3 |
  | Monsters' Hit Points | 75% | — | 125% | 150% | 200% |
  | Monsters' Armor Class | 2 worse | — | 1 better | 2 better | 3 better |

  Monster counts, spells, abilities and experience don't change. Where the
  original already scales its monsters' Hit Points by its own levels
  (*Secret* and the later games' LOAD MONSTER), those levels' own scaling
  gives the Hit Points and the table adds only to-hit and Armor Class. The
  routes play at Normal. The Options screen shows `encounter_challenge` and
  the `[monsters]` ability switches only under the SRD rules.
- **Dexterity above 18 is worth more in a fight** (2026-11-12): each step of
  its Armor Class and missile bonuses comes two points sooner than in the
  original, so an elf's natural 19 beats 18. Every departure from the original
  rules is listed in [DEVIATIONS.md](DEVIATIONS.md#the-original-rules).
- **Thief skills** (2026-11-12): the two out-of-step Dexterity adjustments are
  corrected, Dexterity beyond the original's table (9 to 19) uses its ends,
  and no chance falls below 0.
- **Aging is chosen for each game** (2026-11-12), when it starts, and kept for
  that save:

  | | Off | As the original (default) | AD&D |
  |---|---|---|---|
  | Age changes ability scores | Never | Once, by the starting age | By the current age |
  | Table | — | The original's | The original's, with AD&D's +1 Constitution from young adulthood on |
  | Ages with time | — | Haste only | Haste and every year the game's clock passes |

  - **Scores are worked out, not stored:** a character keeps its rolls and its
    starting age, so a Character Pool character fits any game's setting.
  - **Every character has an age,** rolled from the original's table even
    under the SRD rules, where it changes nothing.
  - **Under AD&D aging,** each older age group changes the scores again,
    keeping them between 3 and the race's maximum; they may fall below the
    class's minimum.
  - **No one dies of old age.**
- **Treasures' creation spells** (2026-11-14): a *Treasures* magic-user
  chooses its first- to third-level spells at creation, slots x 4 / 3 of each
  level, and knows Read Magic besides, as the original does: a page of
  choices after the roll on the creation screen.
- **Treasures' Gateway's money option** (2026-11-14): a *Treasures*-only game
  option, *Gateway's money* in Options. On, a character brought from
  *Gateway* keeps its own platinum, gems and jewelry; off, it starts with
  *Treasures*' 200 platinum.

## Curse of the Azure Bonds

*Curse* plays the same rules with its own tables (from its reconstruction,
`coab`), chosen by the game: the rules' identity names it, so a sheet, a fight
and a save each know which tables they use.

- **Classes:** humans may be paladins and rangers, half-elves rangers and
  cleric/rangers; a ranger is good, a paladin lawful good.
- **Creation:** each ability is the best of six 3d6+1; each class's first Hit
  Die is the better of two rolls. A character starts with 25,000 experience
  (12,500 or 8,333 for each class of a multi-class one) and is trained at once
  up to the levels it reaches, each training's Hit Die again the better of two
  rolls (`SilentTrainPlayer`); a magic-user learns Magic Missile at level 2,
  Stinking Cloud and Protection from Evil at 3, Knock at 4 and Fireball at 5.
  It starts with Detect Magic, Read Magic, Enlarge and Sleep (Shield in *Pool
  of Radiance*) and 300 platinum pieces instead of gold.
- **Tables:** experience to level 12 or 13; THAC0 and saving throws to level
  12 (a fighter's THAC0 first improves at level 3, a magic-user's is 21 to
  level 5, a paladin saves 2 better than a fighter); a dwarf fighter of 18
  Strength stops at level 9, an elf magic-user at 9 to 11 by Intelligence.
- **Training** past a class's Hit Dice adds a fixed amount: 3 a fighter or
  paladin, 2 a cleric, ranger or thief, 1 a magic-user.
- **Hit Points:** a single-class fighter, paladin or ranger adds 1 more for
  each level at Constitution 17, 2 at 18, 3 at 19 and 20, 4 at 21 to 23 and 5
  at 24 and 25; a first-level ranger's bonus counts twice, for its two dice.
- **Thief skills** go on to level 12.
- **Training** at a hall takes one class a visit: of those ready, the one
  whose next level needs the most experience (creation trains them all).
- **Paladins and rangers** strike three times in two rounds above levels 6
  and 7 as fighters do above 6, and gain experience's tenth more when all
  their prime scores are above 15 (Strength and Wisdom; Strength,
  Intelligence and Wisdom).
- **Spells:** *Curse* numbers its spells as *Pool of Radiance* does up to 56
  and keeps its item powers 57 and 59 to 65; its own (58 and 66 to 100: the
  fourth- and fifth-level spells, the druid's, and more powers) are numbered
  100 higher in the rules, so one table holds both, and a record's, item's or
  scroll's spell is read by its game. Slots go on to fifth-level spells and
  class level 12, with a cleric's 18 Wisdom adding one at fourth level.
- **Spells in fights** (`ovr023`): Cure Serious and Cure Critical Wounds
  (2d8+1, 3d8+3), Cause Serious and Cause Critical Wounds, Flame Strike
  (6d8 fire), Ice Storm (3d10), Cone of Cold (the caster's level and as many
  d4 along a path two wide to half its level), Hold Monster, Slay Living
  (death, or 2d8+1 on a save), Poison (the caster's touch poisons), Bestow
  Curse, and the monsters' blast and healing powers.
- **Effects** are read by the fight's numbers: *Curse*'s that work as *Pool
  of Radiance*'s of the same number keep it, its fumbling is helplessness, and
  the rest are 200 higher (`canonical_effect`). Its magic resistance lessens
  with a higher caster's level, as AD&D's does. Its defenses: resisting normal
  weapons, half damage, piercing blows doing 1, protection from magic, cold,
  electricity and dragon breath, the Minor Globe, resisting fire and cold,
  the salamander's heat and the shambling mound's growing on lightning.
- **Touches and holds:** a dracolich's touch paralyzes for the fight and
  its cold touch does 2d8; a shambling mound whose two blows both hit
  engulfs its victim, which suffocates in 2d4 of its turns while the mound
  crushes it with two blows of 2d8; an owlbear hugs on an attack roll of 18 or
  more and squeezes for 2d8.
- **Specials:** a black dragon breathes acid along 6 squares (never through
  its own) and a red one fire along 9, three wide, three times a fight, for
  their Hit Points; a hell hound breathes 7 of fire at one beside it, half the
  time; Tyranthraxus throws 16d6 of lightning in the first four rounds; a
  giant slug spits acid (Curse left it untyped, fixed); a gaze paralyzes for
  an hour; a beholder fires a disintegrating ray at 2 squares, a petrifying
  one at 3, death at 4 and a 2d8+1 wound at 5, then Fear, Slow and Sleep; an
  alias starts each fight invisible.
- **Conditions:** Fear sends those on a path 6 squares long and 3 wide
  running; Confusion takes at most 2d8 and has each roll what it does (run,
  stand confused, go berserk against the nearest creature, or act) before
  trying to shake it off; Fumble leaves one helpless, or slowed on a save;
  Feeblemind (a cleric saving 1 better, a magic-user 4 worse, any other 2
  worse) stops its casting; Sticks to Snakes keeps one under 6 Hit Dice busy;
  Entangle holds one still; Dispel Evil keeps the evil 7 worse off and sends
  away one it strikes that fails to save (whether a creature is evil is read
  from its alignment); Fire Shield, hot or cold, doubles what it is made of
  and saves 2 better against the other (picked at random until the player
  can choose); Dimension Door moves the caster out of any hold; Curse's blink
  hides one once it has acted; an animal cannot single out one invisible to
  animals.
- **Paladins and rangers:** a paladin is protected from evil and lays on
  hands once between a day's rests for 2 Hit Points a paladin level; a
  ranger adds its level to its blows against giants. Both cast spells
  (`RecalcSpellSlots`, `GetCasterLevel`): a paladin gains cleric slots from
  level 9 (one first-level at 9 and 10, a second-level at 11 and 12; in
  *Secret* a third-level at 13, a first at 14 and a fourth at 15) and knows
  every cleric spell of a level it has slots for, cast at its level less 8;
  a ranger gains druid slots from level 8 (first-level at 8 and 10,
  second-level at 12 and 14 in *Secret*) and magic-user slots from 9
  (first-level at 9 and 11, second-level at 13 and 15), knows every druid
  spell of a level it has slots for and casts them at its level less 7, and
  its magic-user spells at its level less 8. *Curse*'s ranger knows no
  magic-user spell but those it scribes;
  *Secret*'s knows every one of the levels it has slots for. A character
  with no cleric or magic-user levels that is no paladin above 8 nor ranger
  above 7 casts at level 6, as a monster does. The sheet shows the druid's
  slots. A paladin cures disease out of fights once a week, and once more
  for each five levels past the first (`PaladinCureDisease`).
- **From *Pool of Radiance*:** a character brought across keeps its scores
  (held within *Curse*'s limits for its race and gender), levels, Hit Points,
  age, experience and drained levels, its magic-user spell book but Animate
  Dead, and its items; it starts with 300 platinum and none of its money
  (`ConvertPoolRadPlayer`).
- **Item names:** an item is named by its record's name words in *Curse*'s
  own table, as its original names one each time it shows it
  (`GenerateName`). Its item lists' records also store name text, which the
  original never shows and which is sometimes in *Pool of Radiance*'s words
  (ITEM3.DAX calls the Wand of Defoliation "Wand of Beaker"), so OpenGoldBox
  drops it.
- **Blast** (the Wand of Defoliation's power, and monsters'): 6d6 along a
  path of three squares; a plant (creature type 18) takes it whole with no
  save (`ovr023`, `sub_61727`).
- **Fights out of doors** (a script's 3D-area cell clear: the wilderness, its
  roads and camps) are on open ground built as *Pool of Radiance*'s wilderness
  fields are, from *Curse*'s own battlefield tiles and the terrain flags of the
  area's current place (cell 0x4E42, which no script sets), both read from its
  START.EXE, and drawn with WILDCOM and RANDCOM (`ovr011`,
  `SetupWildernessFloor`).
- **Camp:** a cleric casts Cure Serious and Cure Critical Wounds (2d8+1,
  3d8+3) in camp, and Raise Dead brings back a dead member who is no elf
  and has Constitution left, with 1 Hit Point and a point of Constitution
  lost, but no Hit Points off its maximum (`cast_raise`).
- **Cloudkill** (`SpellCloudkill`, `ConsiderTerrainEffects`) lays a cloud on
  the square aimed at and the eight around it for the caster's level in
  rounds; one in it as it is laid, as it steps in or as a round ends dies of
  its poison at level 4 or below, saves against poison at −4 at level 5 and
  at 0 at level 6, and breathes it unharmed above.
- **Fire Shield**'s caster chooses hot or cold flames as it goes off (the
  `flames` command); the AI, and Quick, pick at random.
- **On the combat screen** the Items list offers the original rules' own
  actions: a spell to cast (its creatures or square then clicked on the
  field, with Done aiming and Abort the spell), an item's power, Guard,
  Delay, Turn undead and Bandage, and what a spell asks as it goes off, Fire
  Shield's flames and a Delayed Blast Fireball's delay. A training fight
  saved under the original rules loads under them.
- The magic-user's Remove Curse and the item power that animates the dead
  work in fights as the cleric's spells do.
- **Camp:** Cure Blindness, Cure Disease, both Remove Curses and Neutralize
  Poison take their conditions away (Conditions outside fights, plan step
  6); Neutralize Poison stands up one poison killed, at no cost.
- **Entangle** works only out of doors (`SpellEntangle`, outside 3D maps):
  each in its area that fails its save is held by the plants for 10 rounds,
  as its row has it (Bugs). A fight knows it is out of doors from its
  encounter (`rules::Encounter::outdoors`: the wilderness, its roads and
  camps, and Gateway's overland).
- A camp Remove Curse, as the temple's, lets its target put down a cursed
  item (`RulesModule::cursed_item`, by the record's flag).
- **The rakshasa** (its effect 130) struck by a quarrel of name word 135 is
  gone for good (`EffBanishByWeapon`).

## Secret of the Silver Blades

*Secret* plays *Curse*'s rules with longer tables (from its reconstruction),
chosen by the game as *Curse*'s are. In progress (2026-11-13):

- **Creation:** abilities and training Hit Dice as in *Curse*; a cleric's
  first level rolls two dice, the better of two rolls, as a ranger's does. A
  character starts with 200,000 experience a single class (100,000 a
  dual-class, 66,667 a triple-class one, rounded up) and is trained at once;
  only a thief brings gold (2d6), and no platinum. Races, classes, alignments,
  minimum scores, ability limits, ages and level limits are *Curse*'s.
- **Hit Points:** each class adds its own Constitution bonus, a fighter's,
  paladin's or ranger's with the extra from 17 up even in a multi-class
  character, once for each of a level's dice; the roll and the bonus are
  divided among the classes apart.
- **Tables:** experience to level 15 (thieves 18); THAC0 and saving throws to
  level 18 (`saveTbl`), with the paladin's faults fixed as *Curse*'s (Bugs);
  thief skills to level 18, Read Languages 5 below *Curse*'s at 11 and 12.
- **Training** past a class's Hit Dice adds *Curse*'s fixed amounts (Bugs).
  It costs nothing, and every class that is ready trains at once.
- **Spells at creation:** a magic-user knows Detect Magic, Read Magic,
  Shield and Sleep, and learns fixed sets as creation trains it: at level 6
  Enlarge, Magic Missile, Invisibility, Stinking Cloud, Fireball, Lightning
  Bolt and Protection from Normal Missiles; at 7 Friends, Protection from
  Evil, Detect Invisibility, Strength, Blink, Haste and Charm Monsters; at 8
  Charm Person, Burning Hands, Mirror Image, Slow and Fire Shield; at 9
  Invisibility 10' Radius, Protection from Evil 10' Radius, Confusion, Minor
  Globe of Invulnerability and Hold Monster.
- **Spell numbers:** *Secret* keeps *Curse*'s, but its own spells (Heal 36,
  Harm 56, Youth 63, Barkskin 90, Charm Person or Mammal 96, the druid's Cure
  Light Wounds 98, and 101 to 117) are numbered 200 higher in the rules.
- **Spell slots** go on to class level 15 and seventh-level spells (its
  classProgression, level by level): a cleric's sixth-level slots from level
  11, a magic-user's from 12 and its seventh from 14. The sixth and seventh
  need Wisdom 17 and 18 for a cleric, Intelligence 12 and 14 for a
  magic-user. The character sheet shows up to seven levels of slots.
- **Attacks:** a fighter or paladin strikes three times in two rounds above
  level 6 and four above 12; a ranger three above 7 and four above 14.
- **Its own spells** (2026-11-14, its `magic.pas`), known and memorized in
  its table's order, Heal and Harm where Animate Dead and Restoration were:
  Heal cures disease, blindness and feeblemind and heals every Hit Point but
  1d4 (none for one already that close), in fights and in camp; Harm
  diseases and leaves 1d4; the druid's Cure Light Wounds heals 1d8 and
  Charm Person or Mammal charms a mammal; Barkskin casts as Heal, as
  *Secret*'s table has it (`spellFns[90]`); Death Spell spends 4d20 × 2 on
  those within a square of its aim, each costing by its level (1 at level 0
  up to 20 at 7 and 8, 162 above), none of the unliving; Disintegrate and
  Flesh to Stone slay or petrify on a failed save against spells, but not an
  iron golem nor through an image, and magic resistance does not turn them;
  Stone to Flesh brings a statue back with 1 Hit Point; the Globe of
  Invulnerability; Mass Invisibility hides the caster's side within 2 of the
  aim but those with an enemy next to them; Power Word Stun stuns for 4d4
  rounds up to 30 Hit Points, 2d4 to 60, 1d4 to 90, and not above; the
  Delayed Blast Fireball bursts at once for the level and as many d6 of
  fire within 3 squares. A magic-user may learn sixth- and seventh-level
  spells as it trains.
- **The Delayed Blast Fireball's delay** (`SpellDelayedBlastFireball`,
  effect 100): its caster chooses none or 1 to 5 rounds (the original's 1 to
  50 segments, in tens) before it aims (the `blast_delay` command); a delayed
  blast waits on its square and then catches all within 3 for the caster's
  level and as many d6 of fire, half on a save. The computer's casters burst
  it at once (Bugs).
- Its paladins and rangers cast as *Curse*'s do (*Curse*, Paladins and
  rangers), a paladin at its full level.
- **Youth** (2026-11-14; its item power 63, `SpellYouth`) makes its user
  1d4+1 years younger, not below 18, in a fight or out of one (an age that
  changes, Plan step 3).

### *Secret*'s combat

*Secret* fights as *Curse* does, with these differences (its reconstruction's
`effhndlr.pas`, `fxengine.pas`, `magic.pas`, `combat.pas` and `cbtacts.pas`):

- **Creatures:** a record's type and target flags (bytes 349 and 350) take the
  place of its creature type: a dwarf strikes those flagged its foes 1 better
  and a gnome its own, giants strike a dwarf 4 worse and giants, ogres and
  bugbears a gnome; a ranger next to one flagged its foe (giants, ogres,
  ettins, bugbears) adds its level to a blow; Dispel Evil keeps one flagged
  evil (the hell hound) 7 worse off and sends it away, whatever its alignment;
  one flagged unliving (the iron golem, the Dreadlord, the otyughs) breathes a
  stinking cloud unharmed and cannot be slain by Slay Living or Death Spell.
  Its magic resistance is the record's percent (byte 421: the drider 15, the
  lizard king 20), lessening with a higher caster's level as *Curse*'s does;
  it turns a spell's damage only when the spell carries an effect too, so a
  Fireball goes through. Byte 242 is the undead level, as in *Curse*, and 0
  for every creature. *Secret* numbers its races differently (elf 1 to human
  6, monster 7); its records are read into the earlier games' numbers.
- **Spells by flags:** Charm Person takes one of the player's races or a
  monster flagged a person, Charm Person or Mammal one flagged a mammal; Hold
  Person never holds a monster, whatever its size; Snake Charm takes those
  flagged snakes, but not one charmed already.
- **Effects** are read by the fight's numbers (`canonical_effect`): below 57
  as *Curse*'s but for five (immunity to electricity, a gnome's blow at its
  foes, Barkskin, the half-elf's resistance and Dispel Evil's touch), the moved
  ones as the earlier games' that work alike (the immunities to fire, cold,
  breath, sleep and charm, paralysis and poison; resisting normal weapons as
  AD&D has it, from the player's races or below level 4; displacement, a ring
  of invisibility, blinking once it has acted), and *Secret*'s own 400 higher.
- **Touches**, from a blow that did damage only: strong poison at −2, a
  cockatrice's stone and a drider's paralysis for 10 rounds on either attack;
  poison on the second attack, or on the first of a creature with only one
  (Bugs); a victim poisonous itself saves better. A remorhaz swallows its
  victim whole on a natural 20.
- **Defenses:** an iron golem takes harm only from a weapon of +3 or better
  and no spell's effect at all; magical fire heals it and electricity slows it
  for 3 rounds. A giant slug takes none from a bludgeoning weapon. The Globe of
  Invulnerability stops spells to the fourth level, the fearless are never
  afraid, a luckstone saves 1 better, and Constitution adds to saves against
  poison, spells and wands.
- **Powers at the AI's turn**, each aimed as the AI aims a spell, at an enemy
  it sees: a petrifying gaze at one within 6 squares that is not helpless,
  turned back by a mirror if the gazer's can be; a pyrohydra's 8 of fire at its foe beside it from the second round;
  the umber hulk's gaze confusing one beside it for 1d10+2 rounds unless it
  saves against spells; a hell hound's 7 of fire at one beside it; a giant
  slug's 4d8 of acid at one within 6 squares (1 time in 10 in the first
  round, then 1 in 10 for each square nearer than 11); a white dragon's cold 7 squares long and a red one's
  fire 9 long, three wide (along a path as *Curse*'s breaths are), aimed at
  one within 6 squares, three a fight, never through its own side, for its
  Hit Points (half on a save) and ending its turn. Only the breath ends it.
- **At the fight's start:** a ring of invisibility (the Dreadlord's) hides its
  wearer, again at each round's end; a dragon's fear sends each enemy below
  level 5 that is not fearless running for the rest of the fight.
- **Rounds:** each round is ordered once as it starts, by initiative, a tie
  on a d2; one that delays acts last of all, and one that begins a spell moves
  back among those yet to act. There is no turning undead.
- **Backstab:** only a defender of one square next to the thief, with no
  weapon, a club, a dagger or a broad, long or short sword (item types 3, 4
  and 17 to 19) in any armor, against its Armor Class from behind less 4, for
  (level − 1) div 4 + 2 times the damage, multiplied after the blow's other
  effects.
- **Items and races:** its arrows and quarrels are item types 30 and 12,
  and an elf's bows and swords 18, 19 and 22 to 26 (`WeaponAdjust`); its
  races' innate effects are its own (`chargen.pas`): a gnome has no defense
  against giants, and no race saves better against poison.
- **The AI near clouds** (2026-11-14; `AiCanStep`): it balks at a step into a
  cloudkill when its movement is below 7, unless proof against poison and
  paralysis (its effect 99) or turned, and at a step into a stinking cloud
  unless it coughs already, a globe guards it (its effects 57 and 63), it is
  proof or turned, or it saves against paralysis.
- **The giant-bane weapon** (2026-11-14; its effect 75, `EffHolyWater`),
  readied, strikes one its record flags for it (target flag bit 0) 1
  better, for 6d6 and the wielder's Strength bonus in place
  of the weapon's own damage, before the defenses and a backstab's multiple.
- Its LOAD MONSTER's Hit Points by difficulty are played (Difficulty).

## Gateway to the Savage Frontier

*Gateway* plays *Curse*'s rules and tables, number for number (its
reconstruction), with these differences (2026-11-14):

- **Creation:** a character starts with 3,000 experience (1,500 or 1,000 for
  each class of a multi-class one), trained at once, and 35 platinum pieces.
- **Level caps:** every class stops at its own level whatever the race:
  clerics, druids and magic-users 6, rangers 7, fighters, paladins, thieves
  and monks 8 (its levelCaps). So no paladin or ranger casts spells.
- **Its spell table's four changed rows** (its spellTable): Prayer goes on up
  to four creatures within the caster's reach, each an aura for its side;
  Invisibility 10' Radius is aimed at a square beside the caster and reaches
  all within 6 squares of it; Blink may be cast out of fights and Strength in
  them (`original::spell(number, game)`).
- **A half-elf ranger** starts aged 26+3d8, from its own row.
- Training takes one class a visit, as in *Curse*.
- **Its temple's poison rules** (its temple.pas): Raise Dead brings back
  one that poison killed with no loss, and Neutralize Poison stands one up,
  as *Secret*'s does.
- **Strength** cast in a fight goes on its caster, as its row has it (the
  Strength spells, plan step 5).
- **Blink** may be cast in camp, carried into the next fight with the
  camp's other spells for a fight (plan step 5).
- **Ascore's maze of fights** (its combat mode 1, cell 4D01h): the first
  member to get away ends the fight, and the party leaves facing the side it
  ran off (2026-11-14).

## Pools of Darkness

*Pools of Darkness* plays *Secret*'s rules with tables that run to level 40
(its reconstruction, checked against the manual and the Adventurer's
Journal), chosen by the game as the others' are (2026-11-14):

- **Creation:** a character starts with 1,500,001 experience a single class,
  750,000 a dual-class and 500,000 a triple-class one, trained at once: a
  human fighter, cleric or magic-user reaches level 14, a paladin 12, a ranger
  13, a thief 16. Each starts with 200 platinum and no gold. A first-level
  cleric rolls one die again; only a ranger rolls two. A single-class
  character is ten years older than in *Secret*, and a half-elf ranger starts
  at 32. Races, classes, alignments, ability limits and level limits are
  *Secret*'s.
- **Hit Points** are reckoned as *Secret*'s, the Constitution bonus once for
  each of a level's dice.
- **Experience:** *Secret*'s to level 10; from 11 a level costs a fixed amount
  more (cleric 225,000, fighter 250,000, paladin 350,000, ranger 325,000,
  magic-user 375,000, thief 220,000) up to level 40. A paladin's level 11
  needs 1,050,000 and a ranger's 650,000, 1 less than in *Secret*.
- **THAC0** to level 21 (its `baseToHitTbl`): a fighter, paladin or ranger
  improves by 2 every second level to THAC0 4 at level 17; a magic-user's is
  20 from level 1 and improves at 6, 11, 16 and 21; clerics' and thieves' are
  *Secret*'s to level 18, then a cleric's 9 and a thief's 12, 12 and 10.
- **Saving throws:** *Secret*'s to level 18, then to level 21 its own rows:
  a cleric's and a thief's improve at 19 (Bugs), a fighter's, paladin's and
  ranger's stay, a magic-user's improves at 21.
- **Spell slots** of spell levels 1 to 7 go on to class level 29, with
  *Secret*'s Wisdom and Intelligence gates. A magic-user's eighth-level
  slots come at level 16 and its ninth-level ones at 18 (its
  `classLevelTable`), needing Intelligence 16 and 18. A paladin's cleric
  slots grow to level 20 (3 of each of levels 1 to 4) and a ranger's to 17,
  its third-level druid slots coming at 16 and 17; a ranger knows every
  druid and magic-user spell of a level it has slots of.
- **Spells at creation:** once trained, a magic-user knows every spell of
  levels 1 to 3, and with fourth-level slots Charm Monsters, Confusion,
  Dimension Door, Fear, Fire Shield, Ice Storm and Minor Globe of
  Invulnerability (its 81 to 85, 87 and 88), with fifth-level slots Cloudkill,
  Cone of Cold, Fire Touch and Iron Skin (its 91, 92, 118 and 119), and the
  sixth and seventh levels' sets; with ninth-level slots it knows every
  magic-user spell. A dual-class magic-user knows Enlarge, as in *Curse*.
  Training offers a magic-user a spell of any level its next level casts,
  the eighth and ninth too.
- **Its new spells** (its `magic.pas`), in and out of fights as its table
  allows:
  - *Cleric:* Blade Barrier (sixth level) whirls blades on four squares for
    three rounds a caster level, 8d8 to one in them, which magic resistance
    may turn and a save escapes only as they rise. Energy Drain drains a
    level of the one touched; Destruction destroys it for good unless magic
    resistance turns it. Resurrection, in camp, raises a dead member who is
    no elf and has Constitution left to every Hit Point, its poison gone,
    at no cost. Restoration, in camp, gives a drained member back one
    level: of the classes below the level a drain first took them from, the
    one whose next level needs the least experience (the later class on a
    tie) rises a level; the Hit Points the drains took, divided by the
    levels still lost, come back on its maximum and current Hit Points (on
    its rolled ones less the class's Constitution bonus, divided among the
    classes ever drained, below the levels of fixed Hit Points); and its
    experience is raised to what the restored level needs, as a drain here
    takes none. On a member never drained it does nothing. The level goes
    into the member's level history (2026-11-15).
  - *A ranger's druid spells* (third level): Cure Disease and Neutralize
    Poison, in camp, work as a cleric's; Hold Animal holds up to four,
    saving at -4 for one, -2 for two and -1 for three; Protection from
    Fire gives a ranger the druid's protection, which takes 12 points of
    fire a caster level and ends when spent, and anyone else Resist Fire
    with no end, as the original does.
  - *Magic-user:* Fire Touch (fifth level) burns what its bearer hits for
    2d6, halved by a save, for a round a level; Iron Skin leaves its
    caster's Armor Class 4 better for a round a level. Mass Charm (eighth)
    charms enemies within a square of its aim, up to twice the caster's
    level in Hit Dice, each saving at -2, passing over a large creature.
    Otto's Irresistible Dance must hit its touch; the dancer loses its
    turns, saves as if it rolled 1 and has an Armor Class 4 worse, for
    1d4+1 rounds. Mind Blank lasts a day and turns charm, confusion,
    holding, feeblemind and fear away. Power Word Blind blinds those within
    2 squares of its aim, the nearest first, up to 100 Hit Points of them,
    for 1d4+1 rounds, or ten times as long under 51 (Bugs). Meteor Swarm's
    four meteors land on the squares diagonal to its aim (on it where the
    caster cannot see them), each striking all on its way for 10d4 and
    bursting on all within a square for 10d4, halved by a save. Power Word
    Kill slays its first target of 11 to 60 most Hit Points, or else every
    one within a square of its aim of 10 or fewer. Monster Summoning gates
    in creatures waiting off the field; no fight of OpenGoldBox's keeps any
    (the original's only ones are its mirror fight's), so its summons is not
    answered.
  - Fire Touch, Iron Skin, Mind Blank and Protection from Fire may be cast
    in camp and last into the next fight.
- **Spell numbers** are *Secret*'s; its new spells, 101 to 109 and 118 to
  126, are numbered 200 higher too.
- **Training** costs nothing; attacks, thief skills and the experience
  bonuses are *Secret*'s.
- **Charisma and Strength:** every class needs a Charisma of 6 at least (a
  druid's 15 and a paladin's 17 stay), and a paladin or ranger of Strength 18
  rolls Exceptional Strength as a fighter does.
- **Starting equipment:** a new character gets up to six of `ITEM0`'s
  definitions by its class family (its `CreateStartingItems`), known for what
  they are. A fighter, paladin or ranger: a long sword +3, plate mail +3, a
  shield +2, a composite long bow +2 and 50 arrows +2; else a thief: bracers
  AC 4, a ring of protection +2, a long sword +3 and a sling +2; else a
  cleric: a mace +3, plate mail +3, a shield +2, a staff sling +2 and two
  cleric scrolls; anyone else: the thief's bracers and ring, a quarterstaff
  +3 and 50 darts +2. It readies each but the fourth, as the rules allow (a
  class that may not use an item, or full hands, leaves it in the pack), and
  counts what it carries (Bugs) (2026-11-14).
- **Its monsters' effects** are read by what each of its handlers does
  (`effhndlr.pas` and the `effectFns` it registers, `canonical_effect`),
  not by their names (2026-11-15):
  - *As Secret's:* every code whose handler does what *Secret*'s of the
    same number does, among them its Globe of Invulnerability (57), the
    petrifying and confusing gazes (58, 70), the need of a +3 weapon (60)
    and of a magic one (103), the poisons (64, 86), paralysis (88) and
    immunities to fire, cold and fear (93, 98, 92).
  - *As an earlier game's:* half harm from cutting and piercing weapons
    (66) is *Pool of Radiance*'s skeletons', the red and white dragons'
    breaths (84, 85) *Secret*'s fire and cold breaths, the drains of one
    and two levels (101, 102) *Pool of Radiance*'s, and 2d6 of fire with
    each hit (108) its own Fire Touch. The Rakshasa's protection (40: no
    spell below the eighth level), half harm from weapons below +3 (80)
    and +4 (81), the need of a +2 weapon (125: a monster's own blows harm
    it from 7 Hit Dice) and gaze immunity (126: no gaze stones or confuses
    it) do as the *Unlimited Adventures* kit's effects of the same numbers
    and are played as those (2026-11-15).
  - *Its own, played:* half harm from crushing weapons (67), 4d6 of fire
    with each hit (104), and dragon fear
    (100): each enemy that fails a save against petrification at -4 is
    terrified for the fight, 2 worse to hit and 2 less harmful a blow
    (120). The original tries the fear twice as a fight starts (its fight
    start runs two conditions that both hold it), so one that saves once
    faces it again; that is kept.
  - *Its own, not played yet:* a giant-slaying sword (75), the blessed
    bolt that slays a rakshasa (82), the big red, green, blue and black
    dragons' breaths (83, 90, 91, 96), the bulette's rake (99), the
    paralyzing and charming gazes (114, 124), Kalistes' new weapons
    (116), the beholders' rays (117, which the original plays as nothing)
    and the vorpal blade (115).
  - Its records' type flags, "immunities" and magic resistance are read
    (bytes 420, 421 and 491), *Secret*'s bits, so a creature is a snake, a
    mammal, a ranger's foe or unliving as in *Secret*.
- **Its fights play *Secret*'s rules where its reconstruction does as
  *Secret*'s** (`combat.pas`, `cbtacts.pas`, `magic.pas`, 2026-11-15): the
  round's order made once as it begins, a delay going last, a spell begun
  moving its caster down the order; a backstab only on a one-square
  defender beside the thief, 4 off the rear Armor Class and multiplied after
  the blow's effects (by 5 at most in *Pools of Darkness*), with *Secret*'s
  weapons and two more (item types 63 and 67); a blow's effects only with
  damage; Hold Person never holding a monster; Snake Charm and the ranger's
  bonus by the record's flags; the undead and unliving breathing a stinking
  cloud unharmed. Charm Person takes a monster its flags call a mammal, of
  one square (its `SpellCharmPerson`). Turning undead stays the earlier
  games' (*Secret* has none, *Pools of Darkness* has it), and so does the
  AI's step into a cloud (its `AiCanStep` neither saves nor counts movement
  as *Secret*'s does).
- **The temple's Restoration** (2026-11-15; temple.pas, `Restoration`,
  eleventh in its `HealMenu`) gives a drained member back a level as the
  spell does, for nothing, as *Pools of Darkness*'s and *Treasures*'
  temples ask nothing (`rules::TempleService::restoration`); it is offered
  only to one a drain has taken below its peak.
- **Still to come:** Mass Charm's test of the record's
  charmable flag (it passes over only a large creature and the caster's
  side for now); the monsters' effects not played yet (above); its own
  turning (paladins from level 3, a limit of tries a fight) and cloud
  steps; and bringing a *Secret* party across (its LoadChar), which the
  original offers.

## Treasures of the Savage Frontier

*Treasures* runs on the engine of *Pools of Darkness*, and its tables are that
game's, number for number (its reconstruction's `classLevelTable`,
`xpIncrement`, `thac0Tbl`, `classSaves`, `raceCaps` and `firstLevelHitDice`
match): experience, THAC0, saving throws and spell slots to level 40 (above).
Its differences (2026-11-14):

- **Creation:** a character starts with 22,600 experience, shared among its
  classes and rounded down (11,300 for two, 7,533 for three), trained at
  once, and 200 platinum (its `chargen.pas`): a human fighter starts at level
  5, a thief at 6.
- **Level caps:** besides its races' limits, every class stops at its own
  level (its `trainingLimits`): clerics 10, druids 12, the rest 11.
- **Training** takes one class a visit, as in *Curse* and *Gateway*: the ready
  class whose next level needs the most experience (Bugs), for 200 platinum.
- **Spells:** its spell table is *Pools of Darkness*'s, entry for entry (its
  127th entry is unused), so its spells are numbered and cast as that game's.
  At creation a magic-user learns its engine's sets for the fourth level and
  up. For levels 1 to 3 it knows Read Magic and chooses a third more spells
  than the level's slots (slots x 4 / 3), on a page of choices after the
  roll (Decisions); it knows no other spells of those levels, not the four a
  *Pool of Radiance* magic-user starts with.
- **Detect Magic** may be cast outside combat, as in the originals, spent
  with nothing more to lay: *Treasures*' scripts may wait for a spell
  (Cortarra, [treasure.md](games/treasure.md), What the route needed).
- **A *Gateway* character comes across** as its LoadChar takes one in
  (`CampaignParty::bring_across` with `gateway_items_for_treasures`): its
  scores, levels, Hit Points, experience and learned spells, and its items,
  each name word found again among *Treasures*' own by its letters and its
  type renumbered by the program's table (`legacyTypeMap`), unreadied; an
  item whose word or type *Treasures* lacks, or of effect 98, is left behind.
  The original keeps the character's own platinum, gems and jewelry: with
  the *Gateway's money* option on (Decisions), the money given to
  `bring_across` comes into the purse; off, the default, the character
  arrives with *Treasures*' 200 platinum, as a *Pool of Radiance* character
  arrives in *Curse* with its 300. The party screen's *Add from Gateway*
  (2026-11-15) passes the character's share of its save's platinum, gems
  and jewelry, the purse divided evenly among the save's player characters,
  since OpenGoldBox keeps a party's money in one purse.

## Unlimited Adventures

*Unlimited Adventures* is SSI's C rewrite, not the Pascal engine, but its
creation tables are *Curse*'s: the same classes by race (humans may be
paladins and rangers, half-elves cleric/rangers; no druids), each score the
best of six rolls of 3d6 + 1 (2026-11-14). Its differences, from its
reconstruction ([ua.md](games/ua.md), A party under the kit's rules, has the
whole list):

- **No half-orcs.**
- **The design starts the party:** its game record gives a new character's
  experience (the kit's default 40,001, shared among its classes and trained
  at once), money (200 platinum, and gems and jewelry) and one of eight
  sets of equipment by class group; OpenGoldBox follows the record
  (2026-11-14).
- **Prices are platinum,** at one of twenty rates an event sets (a
  hundredth to a hundred times the list price): training 1,000, a temple's
  cures 20 to 2,000, a shop's items their value, identifying 20. The party
  pays them from its one purse at 1 platinum to 10 gold, as the user
  confirmed (2026-11-14).
- **Its own experience table:** *Curse*'s to level 11 for every class (the
  paladin's and ranger's eleventh 1,050,000 and 650,000), then a fixed
  amount a level to level 40 (2026-11-14).
- **A ranger's magic-user spells:** each time its ranger level rises past 8
  it learns one of a level its new ranger level casts, and only then: the
  kit gave one to a ranger past 8 whichever class rose (the user,
  2026-11-14; Bugs).
- **Its level caps by race are *Curse*'s,** to level 40 (2026-11-14).
- **Its THAC0 and saving throws are *Pools of Darkness*'s,** to level 21
  (2026-11-14), the kit's own empty thief row fixed (Bugs).
- **Its races' innate effects are the kit's** (the user, 2026-11-15): no
  save against poison by Constitution, and a gnome strikes its foes but has
  no defense against giants (ua.md).
- **Its fights order each round once,** ties on a d2, as *Secret*'s do
  (2026-11-14).
- **Its difficulty** (the user, 2026-11-15: "C, but make it a configurable
  option"): the game's Difficulty plays in a design as the Options screen's
  Design difficulty says, the kit's five levels (the party's side +2 to -2
  to hit, the monsters' Hit Points x (level + 1) / 4; Easy its level 1,
  Normal its 3, Hard its 4, Nightmare and Infernal its 5), OpenGoldBox's
  difficulty table for the monsters (to-hit, Hit Points and Armor Class;
  [Difficulty](#difficulty)), or both, the default: the kit's Hit Points
  and party to-hit with the table's to-hit and Armor Class (ua.md, Fights).
- **Still to come:** its critical hits on a natural 17 or more, which only a
  vorpal weapon uses.

## Next

The work asked for next (2026-11-14, brought up to date 2026-11-15), in
order, with what each needs. Done since: the difficulty levels, the halls'
class masks, AD&D aging by the clock, the temples' Restoration, Spiritual
Hammer's lasting hammer and the later games' effect numbers.

1. The other "Still to come" items above that are no one game's campaign or
   route work, in their order in this file:
   - The original's rest of any length and its preparation time for
     memorizing (step 5; scribing takes its own, Scribing). What the
     original does (`gametime.pas`, `CampRest`, `CampRestMenu`,
     `AdvanceSpellPrep`, `AdvanceSpellPrepBase`, `CheckRestingHeal`;
     `camp.pas`, `RestForSpellPrep`, `CampSpellPrepTime`,
     `CampMemorizeMenu`): Memorize marks spells pending (bit 0x80 of the
     memorized slot) before the rest; Rest proposes the party's longest
     preparation (4 hours, 6 with a pending spell above level 2, plus a
     quarter hour a pending level, memorizing and scribing alike), which
     the player raises or lowers by days, hours and five minutes, and stops
     at any key. It rests in five-minute steps: each step passes the
     clock, every 288 steps (a day) each member heals 1, each member's base
     hours count down by the hour and then its pending spells finish one at
     a time, a scribed spell first, 15 minutes a level each, and the area's
     rest encounter may break it. What OpenGoldBox does now: the camp's
     long rest is a day (at least the scribing preparation), the short rest
     an hour, and spells are chosen after the long rest
     (`CampaignParty::rest`, `long_rest_benefits` in campaign_rest.cpp,
     `OriginalModule::long_rest_policy`). It needs a rest length on the
     campaign's rest (`CampaignParty::rest` and its RestPolicy) with the
     heal by whole days of rest, memorizing chosen before it and finished by
     each member's preparation, the camps that call it
     (`RolfTourSession` in phlan_session.cpp, `rest_interruption`;
     ua_session.cpp; vga_host.cpp, other sessions' files, small hooks only)
     and the rest dialog's length (days, hours, minutes) in the Godot view.
2. *Gateway*'s alike-numbered effects that differ from *Curse*'s and
   *Gateway*'s and *Treasures*' own effects (Effects of the later games),
   which do nothing yet.
3. *Unlimited Adventures*' difficulty: its session calls
   `original::monster_difficulty(level, true)` with each design's own
   setting (the coordinator, 2026-11-15).

## What the original rules cover

These are the parts of the reconstructed source that hold rules, about 8,000
lines in all:

- **Characters** (`chargen.pas`, `charhlp.pas`, `classcal.pas`, the tables in
  `gdconsts.pas`):
  - six races to choose from; the tables also hold a seventh, the half-orc,
    which the race menu skips;
  - Cleric, Fighter, Magic-User and Thief, with the multi-classes each race
    allows;
  - each ability the best of six 3d6 rolls, adjusted for age, kept within the
    race's limits and raised to the class's minimums;
  - the *Modify character* screen before the first experience;
  - Hit Points, starting gold and starting spells.
- **Advancement:** the experience tables, the level limits, and training (one
  level per visit, 1,000 gold pieces).
- **Combat** (`combat.pas`, `cbtacts.pas`, `combatai.pas`, `fxengine.pas`):
  - d6 initiative, THAC0 and descending Armor Class;
  - half-attacks, fighters' sweeps against weak foes, backstab and attacks
    from behind;
  - range bands;
  - dying to −10 and bandaging, Guard and Delay;
  - fleeing, morale, and the monsters' tactics.
- **Saving throws:** five categories, by class and level.
- **Magic** (`magic.pas`, `effhndlr.pas`): about 45 cleric and magic-user
  spells of levels 1–3, memorized in camp and scribed from scrolls, and about a
  hundred effects.
- **Monsters:** taken whole from the game's files (`MON*CHA`, `MON*SPC` and
  `MON*ITM`), with no conversion.
- **Items:** taken from the `ITEMS` file.
- **Other:** thief skills, turning undead, the temple's services and prices,
  and resting.

## Plan

1. **The original rules' character rules**, a library of their own
   (`src/OpenGold.Rules.Original`) with tests: the tables, creation,
   THAC0, saving throws, levels and spell slots.
2. **Choosing the rules.**
   - The new game's first question: the rules, then (under the original
     rules) the Aging setting.
   - The game's direct calls to the SRD module replaced by the selected rules.
   - Each save keeping its rules, and the Character Pool keeping each
     character to its rules.
   - The difficulty settings split per set of rules.

   Done (2026-11-13). The question shows since the original rules play
   fights on screen (combat step 7): after a game is chosen, a "New game"
   window asks the rules, the SRD's offered first until the original rules
   play from start to end and become the default (step 7), and under the
   original rules the aging, the original's first. Closing it returns to the
   games; `--rules=srd` or `--rules=original` answers it for a check.
   - **The game's rules** (`src/OpenGoldBox/game_rules.h`): `GameRules`, the
     rules a game plays by and its aging, makes a game's party, its rules
     module and character rules; `new_game_rules` is what a new game plays by
     (the SRD for now).
   - **The party carries its rules:** a game's `CampaignParty` holds its
     character rules beside its module, and the game's screens, fights,
     creator, reincarnation, companions and reloads use them, not the SRD's.
   - **Saves keep their rules:** campaign format 49 keeps the rules' setting
     (`CharacterRules::game_setting`, the aging's name); a load reads the
     save's rules first and loads it under them, so a save made under the
     original rules loads under them in a game of either.
   - **The Character Pool** is made under the campaign's rules, again when a
     loaded game brings other rules. Its characters are the SRD's; the
     original rules' come with step 3.
   - **Difficulty:** in a game under the original rules, the Options screen
     hides the encounter challenge and the monster abilities, and says every
     difficulty plays as the original did.
   - Still the SRD's, for step 3 (the sheet and creation) and step 5 (magic):
     the sheet's details, hit dice and save rolls, spell and charge labels,
     equipment notes, the companions' drafts and the check fixtures, which
     are SRD characters.
3. **Character creation, the sheet and level-ups** under the original rules.

   Done (2026-11-13):
   - **Creation** follows the original's pages: name, race and gender; a class
     the race allows; an alignment the class allows; the character rolled whole
     (each score where it fell, with its Hit Points, age and gold), kept or
     rolled again; the combat icon; the portrait; the sheet. Another race or
     class rolls again. Classes and alignments not open are in gray.
   - **The sheet** shows the levels of each class, age, Hit Points, THAC0, the
     scores with Strength's percentile at 18, the five saving throws, a
     thief's skills and a caster's spells a day.
   - **Level-ups are the original's training** (`chargen.pas`, `TrainingMenu`):
     every class whose next level's experience the character has, and that its
     race's limit lets rise, gains a level at once; the trained classes' Hit
     Dice are rolled and, with the Constitution bonus of all its classes,
     divided among them (at least 1); experience beyond the level after next is
     cut to just short of it. The roll is kept with the level (campaign format
     50). A sheet's level counts its trainings; its class levels say what it is.
   - **Experience** is shared as the original awards it (`postcbt.pas`,
     `AwardExperience`): a tenth more for a single-class cleric, fighter,
     magic-user or thief whose prime ability is above 15; half for two
     classes, a third for three.
   - Checks start a game under the original rules with `--original-rules`
     until the new game's question is shown.
   - **An age that changes after creation** (2026-11-14): a character keeps
     its starting age, from which its scores are worked out, and each change
     since is an entry of its level history ("age" and the years), which a
     save keeps. A member's numbers carry its age into a fight (OpenGoldBox's
     marker effect 602), where Haste ages it a year (`EffHasted`) and Youth
     takes 1d4+1 years off (`SpellYouth`); after the fight the game keeps the
     change (`lasting_changes`), as it does Youth used out of a fight. Under
     the original's aging the scores stay; under AD&D aging each age group
     reached or left changes them again, between 3 and the race's maximum
     (exceptional Strength is lost below 18). Under AD&D aging each year the
     campaign's clock passes (12 months of 30 days, the original's calendar)
     adds a year, an "age" entry like Haste's (`RulesModule::clock_changes`,
     2026-11-15); the original's own clock ages no one until its year count
     overflows (`ValidateClock`), so under its aging the clock adds none.
   - Later: the training hall's fee and the classes each hall trains (step
     6), and a magic-user's new spell at each level (step 5). The Character
     Pool has no original-rules characters yet.
4. **Combat** under the original rules, with the original monsters' own
   statistics.
5. **Magic:** spells, memorizing, scribing and resting.

   In part (2026-11-13):
   - **Known spells:** a cleric knows every cleric spell of the levels it has
     slots for; a magic-user starts with Detect Magic, Read Magic, Shield and
     Sleep (`CreateCharacterMenu`) and learns one magic-user spell of a level
     it will cast at each of its trainings (`TrainingMenu`).
   - **Memorizing:** after the camp's day of rest each member with slots
     memorizes as many spells as they hold, the same spell in several if the
     player likes (`CampMemorizeMenu`). An hour's rest memorizes nothing.
   - **Spent spells:** a member fights with its memorized spells; those it
     casts stay spent through later fights until the next day's rest (the
     member's state keeps what its last fight left).
   - **Camp:** a cleric casts Cure Light Wounds in camp, healing a member 1d8
     (`NoncombatCastMenu`).
   - The camp's cures of conditions (Conditions outside fights, step 6).
   - **Camp spells for a fight** (2026-11-14; `NoncombatCastMenu`): Bless,
     the Protections, Resist Cold and Fire, Shield, Detect Invisibility,
     Invisibility and its 10' Radius, Mirror Image, Prayer, Haste (on as many
     as the caster's level) and Protection from Normal Missiles, and Blink
     where the game's table allows (Gateway's), cast in camp, carry their
     effects into the next fight by their minutes, running down as the
     campaign's clock passes (Time between rests, plan step 6).
   - **Scribing** (2026-11-14; `CampScribeMenu`, `CheckScribeDone`): a
     member awake and alive scribes a spell from a scroll it carries and can
     read (known, or by Read Magic, or a cleric's scroll for a cleric) into
     its book, when the spell is not in the book and the member has slots of
     its class and level (a *Curse* ranger its magic-user spells so). It
     takes no ink: the spell is marked on the scroll (its spell byte's
     pending flag, which a save keeps), not again while marked, and scribed
     as the member's next day's rest ends (`CheckScribeDone`), the rest lasting
     at least the party's longest preparation (`RestForSpellPrep`,
     `CampSpellPrepTime`): four hours, six with a spell above the second
     level, shared by all its spells, and a quarter hour a spell level. An
     hour's rest scribes nothing. The spell leaves the scroll, which is gone
     with its last; the book keeps it through saves and drained levels. Read Magic is cast
     in camp on its caster and lasts into the next fight. A spell read from a
     scroll in a fight leaves the scroll's others on it. The inventory's Use
     offers the scroll's spells to scribe.
   - Still to come: the original's rest of any length and its preparation
     time for memorizing (`CampSpellPrepTime`).
   - **The Strength spells** (2026-11-14; `magic.pas`, `SpellEnlarge`,
     `SpellReduce`, `SpellStrength`, `SpellGiantStrength`; `fxengine.pas`,
     `RecalcAbility`): Enlarge gives 18/00, 18/01, 18/51, 18/76, 18/91 and
     18/00 at caster levels 1 to 6, and from *Curse* on 19, 20, 21 and 22 at
     7 to 10; Reduce takes an Enlarge away unless saved against; Strength
     adds a d8 for a fighter (and from *Curse* on a paladin or ranger), a d6
     for a cleric or thief and a d4 for anyone else, a warrior's points past
     18 turning into 10 of exceptional Strength each, to 18/00, anyone
     else's stopping at 18; the Potion of Giant Strength gives 21 for the
     fight. A creature's Strength is the best its own and its effects give,
     worked out again as each comes and goes, and moves the to-hit and
     damage of blows that take Strength. Cast in camp, Enlarge and Strength
     last into the next fight and through the clock's time by their minutes. The earlier games' effects keep the
     Strength to go back to, *Curse*'s and *Secret*'s the Strength they give;
     OpenGoldBox keeps the Strength they give in all.
6. **Training, the temple, thief skills, turning undead and conditions,** and
   the original scripts reading the original values.

   In part (2026-11-13):
   - **Training** is the original's (`TrainingMenu`): a level is gained only
     at a hall that teaches one of the member's classes, for 1,000 gp, and
     raises the classes that hall teaches whose experience the member has (the
     fighters' hall fighters, paladins and rangers; the clerics' clerics and
     druids). The hall turns away the unconscious, the poor, the wrong class
     and the inexperienced as the original words it, says what the member
     will become, and lets a magic-user choose its new spell. The later
     games' halls train the classes their script's class mask names (cell
     6DA8h, `TRAIN_MASK`; its bits are *Curse*'s `classBits`: 1 magic-users,
     2 clerics and druids, 4 thieves and monks, 8 fighters, 10h paladins, 20h
     rangers), any mask (`original_mask_classes`, 2026-11-15); *Pool of
     Radiance*'s named halls are their masks too (`original_class_mask`).
   - **Thief skills:** a thief may pick locks, as the original offers Pick to
     a party with one; scripts read a half-elf's own race code.
   - **The temple** heals as the original's (`temple.pas`): Cure Light,
     Serious and Critical Wounds 1d8, 2d8+1 and 3d8+3, and Raise Dead brings
     the dead back with 1 Hit Point (`RaiseDead`); it offers no
     Resurrection, which is OpenGoldBox's own. The raised member loses a
     point of Constitution while it has one, and Hit Points off its maximum
     reckoned with the Constitution it is left with, as the original reckons
     them: at Constitution 14 or more, what its maximum holds above the Hit
     Points its dice rolled (the original's hpRolled, which the sheet keeps
     from creation, training and drains; a character brought from another
     game counts its maximum less one level's Constitution bonus, as
     *Curse*'s ConvertPoolRadPlayer does), divided by a bonus summed over its
     class levels (a fighter's Constitution above 14 a level, any other
     class's 1 a level at 14 or 15 and 2 above); none at 17 or more for a
     character without fighter levels. The division takes far fewer Hit
     Points than the Constitution bonus lost, but its intent is unclear, so
     it is kept. The cost goes into the member's level history, so a save
     keeps it.
   - **Conditions outside fights** (2026-11-14): blindness, disease, poison,
     Bestow Curse and feeblemind stay on a member after its fight and through
     a day's rest (a Bestow Curse running out with its time), and its next
     fight starts with them; a disease whose time comes takes hold as the
     wasting of Strength and Hit Points (`EffDiseased`). The temple's Cure
     Blindness, Cure Disease, Neutralize Poison and Remove Curse take them
     away, and Raise Dead the poison its dead member died of; *Secret*'s
     temple Heal cures blindness, disease and feeblemind too, and *Secret*'s
     and *Gateway*'s Neutralize Poison stand up one poison killed, at no cost.
     Keoghtum's Ointment takes poison, else disease, away outside a fight as
     in one. A disease that has taken hold wastes its member as the clock
     passes, a point of Strength each hour to 3 and a Hit Point each ten
     minutes to 1, and leaves it helpless until it is cured, which gives the
     Strength back (`EffDiseaseStrength`, `EffDiseaseHitPoints`). Slow Poison
     in camp stands one poison killed up for an hour a caster level, and the
     poison kills it again as the spell runs out.
   - **Stone** (2026-11-14): a member a fight turns to stone stays a statue
     after it (the original's status PETRIFIED; its state's description
     says "Turned to stone."): it starts every later fight out of it, heals
     nothing as the party rests, casts nothing in camp, and no cure of
     wounds or Raise Dead helps it ("can only be healed by a stonemason");
     the temple's Stone to Flesh brings it back okay at 1 Hit Point
     (`StoneToFlesh`).
   - **Time between rests** (2026-11-14; `gametime.pas`): the time the
     campaign's clock passes outside fights (walking, scripts' waits, camp
     work and rests alike; `RulesModule::pass_time`) runs every member's
     lasting conditions down as the original counts it: camp spells, Bless
     ending after its 6 minutes of walking, Slow Poison, Bestow Curse, a
     disease's time to take hold, and the paladin's day of laying on hands
     and week of cures. A disease that has taken hold wastes by the hours and
     ten minutes the clock crosses. A day's rest is the day's time passing
     and then its 1 Hit Point and memorizing; an hour's rest only passes the
     hour.
   - **Companions:** an NPC who joins (ADD NPC) is its original record: its
     scores, class levels, Hit Points, spells known and memorized, and its
     own gear. The rules keep the record whole in its draft, so a save makes
     it again the same.
   - **Items outside fights:** the Potion of Healing heals 2d4+2 and
     Keoghtum's Ointment 1d4+8, as in a fight; the rules say which items can
     be used out of one and give the inventory its notes.
   - **Scripts' harm** is the original's (`EclDamage`): an arrow hits when
     its d20 is above 1 and the roll with the script's figure (60 less a
     THAC0) tops the member's stored Armor Class (`AttackRollNoAttacker`), a
     saving throw of the script's category and modifier avoids the damage
     (`RollSavingThrow`), and a member falls at 0 and dies at −10.
   - **Coins are worth what the original's are** (`gdconsts.pas`,
     `coinValues`): 20 silver, 2 electrum or 1/5 platinum to the gold piece
     (the SRD's 10, 2 and 1/10 under the SRD), in prices, fees and what a
     script sees of the purse.
   - **Encounters fight whole:** every monster a script loads fights, as in
     the original; the SRD's fitting of a fight to the party's levels is the
     SRD's alone.
   - **Level drain lasts** (`effhndlr.pas`, the level drain): the levels and
     Hit Points a fight drains stay drained, kept in the character's level
     history; the original's count of them goes down by one for each class a
     later training raises (`TrainingMenu`). *Pools of Darkness*'s
     Restoration gives them back one at a time (Pools of Darkness).
   - **A charmed member** who ends a fight on the monsters' side is still the
     party's.
7. **The monsters' tactics and the original rules' difficulty levels.** The
   original rules become the default once they play from start to end.

## Combat (plan step 4)

Started 2026-11-12, needed by every later game (docs/GAMES.md). What the
original does, from *Pool of Radiance*'s combat units (`combat.pas`,
`cbtacts.pas`, `combatai.pas`, `fxengine.pas`, `charhlp.pas`, `postcbt.pas`):

- **The round.** Each creature's initiative is d6 plus its Dexterity missile
  adjustment, at least 1; a side taken by surprise loses 6 in the first round.
  The highest initiative left acts next, ties settled by a fresh d100. A
  creature acts until its actions end. Delay lets it act again later in the
  round; Guard ends its turn ready to strike. At the round's end the clock
  moves a minute, each dying creature bleeds 1 and dies past −9, and a side
  with no one able to act loses.
- **Movement** counts half-moves, twice the creature's movement: a straight
  step costs 2, a diagonal 3, times the ground's cost. Heavy armor caps it.
  Leaving an enemy's side gives it a free attack, against the mover's rear
  Armor Class; stepping next to a guarding enemy draws its strike.
- **To hit:** d20 against THAC0 minus the target's Armor Class; a natural 1
  misses and a natural 20 hits. A creature struck from several sides is hit
  from behind with its rear Armor Class (no Dexterity or shield). A thief in
  light armor with a light weapon backstabs such a target for multiplied
  damage. Missiles lose accuracy with distance in bands.
- **Attacks** are rates per two rounds; a monster has two attacks with their
  own dice; a readied weapon replaces the first, with Strength and its magic.
  A fighter sweeps through level-0 enemies around it, one attack each up to
  its level. Damage stops a spell being cast. Attacking a helpless creature
  kills it.
- **Down:** 0 Hit Points is unconscious, below 0 dying, more than 9 below 0
  dead. Bandage saves the first dying member. After a won fight the dying
  wake unconscious.
- **Morale:** monsters only, from the record's morale and the share of Hit
  Points lost, and the group's; a broken monster flees or, when it cannot
  outrun its enemies and is not stupid, surrenders.
- **Spells** with a casting time take effect at the caster's next action and
  are lost to damage before then. Saving throws: d20 with its bonuses against
  the category's number; 1 fails and 20 saves.
- **The original's AI** takes a stance, checks morale, then tries an item, a
  spell it began, turning undead, a spell, its best weapon, a target and its
  moves and attacks, and guards when nothing else is possible.

*Curse of the Azure Bonds* keeps this loop and turn order and adds monster
attacks (engulfing, eye rays, hugs, claws and rakes), dual-class levels and
many spells.

**Where it lives:** `src/OpenGold.Rules.Original` gains the combat session and
the rules module behind the interfaces the SRD rules implement (`RulesModule`,
`CombatSession`), so the game's fights, party and screens drive either. Monsters
fight with their records' own values (THAC0, Armor Class, Hit Points, attacks,
movement, saving throws, special abilities) and award their records'
experience; party members' numbers come from their original-rules sheets and
gear.

**Steps,** each with its tests:

0. Groundwork: Strength's to-hit and damage, armor movement, attack rates and
   the to-hit rule in the tables; the original's item types; the movement grid
   shared, with the original's step costs. *Done 2026-11-12:* the tables in
   `original_rules.h`, the item types' property bits
   (`ItemTemplate::properties`), and `opengold/movement_grid.h` with its step
   costs (`StepCosts`).
1. A melee fight between original-rules characters and a monster record
   resolves: initiative, turns, movement, to-hit, damage, unconscious, dying
   and death, the result, saving and restoring mid-fight. *Done
   2026-11-12:* `original::rules_module()` (`original_combat.h`) runs a fight
   behind the game's rules interface; a character fights unarmed as the
   original's characters start (1d2 with Strength), a monster with its
   record's numbers (`combatant_from_record`). `opengold_original_combat_tests`
   plays two fighters against three kobolds, checks every Hit Point against
   the log, and replays a fight from its seed and from a mid-fight save. A
   creature down is out of the fight and no target; the original's
   "helpless" (sleep, paralysis) comes with the effects in step 6. A fight
   ends only as a round ends (`EndOfRound`): the creatures with turns left
   still take them after a side is down, and the dying bleed first. On a full
   tie of initiative and d100 the later creature acts first (`NextCreature`).
2. The real monster records, their readied weapons and their statistics. *Done
   2026-11-12:* `combatant_from_record(record, readied)` works out a
   creature's numbers as the original does at the start of every fight and
   turn (`RecalcStats`): Strength unarmed or with a weapon that takes it, the
   weapon's dice and magic, an elf's +1 with swords and bows, ammunition's
   magic, the best armor, shield, ring and protective magic, Armor Class from
   behind, and armor's weight on movement. Every one of *Pool of Radiance*'s
   172 records fights with sensible numbers, and the Slums' kobolds fight a
   party to the end.
3. The rest of melee: free attacks, guarding, delaying, attacks from behind,
   backstabs, sweeps, bandaging. *Done 2026-11-12:* creatures face one of
   eight directions and turn toward an attacker until a third attacks them in
   a turn; the attacks on a creature and their angles count toward an attack
   from behind (its Armor Class from behind) and a thief's backstab (that
   Armor Class less 2, level div 4 + 2 times the damage). Stepping away from
   an enemy draws its free attack; a guard strikes an enemy coming next to it;
   Delay acts again at initiative 1; a fighter among more level-0 enemies than
   its attacks sweeps through up to its level of them; bandaging needs the
   dying member next to the one bandaging (the user, 2026-11-12). Guard is
   offered to every creature until step 4 tells ranged weapons apart: the
   original offers it only without one, or with a throwable one.
4. Missiles: rates of fire, ammunition, distance, thrown weapons. *Done
   2026-11-12:* a readied weapon whose type's range is above 1 is a missile
   weapon (`original::Missile`). It reaches its range less 1, counted as the
   original counts a line (2 a straight step, 3 a diagonal one, halved), at
   a target it sees, with no penalty for distance (the user, 2026-11-12). It
   attacks at its rate of fire (at least once a round), no more often than it
   has shots. A bow shoots the readied arrows, a crossbow the quarrels, a
   sling needs nothing, and a thrown weapon is its own ammunition: each shot
   spends one, and the last thrown weapon leaves its thrower empty-handed.
   With an enemy next to it a creature cannot shoot, unless its weapon is one
   it throws with Strength (a dagger, hand axe, spear or javelin), which then
   strikes as in melee; only such a weapon, or none, can guard. Sight weighs
   the original's heights (step 7). One thing waits for step 7: the shots
   spent leave the party's inventory, as the original drops a thrown weapon
   in the loot.
5. Morale, fleeing and surrender. *Done 2026-11-12:* a creature whose
   record's NPC byte is 0x80 or more checks its morale as its turn starts
   (the player's characters never do). It holds while its morale (twice the
   byte's low 7 bits, 0 past 102) is at least the share of Hit Points it has
   lost, and an enemy of the party also holds while the opposition's Hit
   Points left (in steps of 5, as of the round's start) are at least 100 less
   the encounter's morale. Broken, it flees in panic when no enemy is faster
   (the reconstruction's comment says "twice as fast", but its arithmetic
   compares the movements themselves, and this follows the arithmetic),
   surrenders with Intelligence 6 or more, and fights on otherwise; it may
   rally at its next turn. A party NPC without a morale of its own takes the
   encounter's. A creature on the field's edge with a step left can flee: it
   gets away with no enemy in sight or more movement than the fastest enemy,
   or on 1 in 2 when they tie. With none of the party left fighting, the
   party has fled when one of its own characters got away, and is defeated
   otherwise. Where a panicked creature runs is the AI's (step 9), along the
   party's facing (step 7). Left for later: what the game does after a
   flight: the members left behind are lost, as in the original.
6. Saving throws, then the monsters' special abilities (MONnSPC). In parts:
   - 6a. *Done 2026-11-12:* a creature's effects (a monster's from its
     record) with their minutes, which a round ages by one; those that end at
     0 Hit Points end as a creature goes down or leaves the field. Saving
     throws: a natural 1 fails, a 20 saves, otherwise the roll with the
     readied items' save bonus and the modifier against the creature's
     number. A helpless creature (asleep, paralysed, nauseous or charmed by a
     snake) loses its turns and makes no free attack or guard strike; an
     attack on it is one blow of its Hit Points and 5 more, which leaves it
     dying, and one made helpless partway through an attack is hit by the
     rest.
   - 6b-1. *Done 2026-11-12:* what a hit that leaves its target standing
     carries, by the attack that hit (a secondary attack resolves before the
     primary): poison, which kills unless saved against; paralysis (a ghoul's
     sparing elves); mummy rot; a stirge's drinking until it has had 12 to 15
     and flies off; a holding bite on a victim of Strength below 19, which
     bites from behind with 3d4 each turn; and a rear-claw rake after both
     claws hit. Immunity to paralysis and poison saves but on a natural 1.
   - 6b-2. *Done 2026-11-12:* level drain, with no save. Each level takes
     the Hit Points of one of the creature's levels from its maximum and its
     current; below level 2 it is dead forever. Otherwise the class whose
     level took the most experience loses a level, with its THAC0 and saves,
     and a victim left with no Hit Points is killed. A creature's numbers as
     the fight leaves them (levels, effects still running) go back to the
     game with its Hit Points (`VitalState::resources`).
   - 6c. *Done 2026-11-12:* defenses. A creature's blows land with its
     striking item (a thrown weapon, a launcher's arrows or quarrels, else
     its readied weapon; none for natural attacks), whose magic, silver and
     weapon class the defenses weigh: magic needed against characters and
     creatures below level 4, silver or magic, a wraith's (half from
     silver), halving by slashing, piercing, bludgeoning or magic, missile
     and boulder evasion from afar, holy water and oil. A troll regenerates 3
     a round from the third round after its first wound and rises again,
     whole, 3d6 rounds after a weapon downs it; plain regeneration heals 3 a
     round. One that fights while down stands again with 6 less its wounds
     and falls dead 1d4+1 rounds later. Displacement turns aside the first
     attack each round, blinking every attack until the blinker has acted.
     An invisible (camouflaged) creature cannot be singled out but by one
     that sees the invisible, and is 4 harder to hit; attacking makes it
     visible. A gazing monster cannot single out one holding up a mirror or
     the wooden holy symbol. A mummy's fear paralyses its enemies for 1d4
     rounds as the fight starts, unless saved against (+1 for a side of 6, +2
     for a human) or immune to paralysis.
   - The gazes, the lightning breath and the ankheg's acid fire only when
     the original's AI acts for a monster with a target (MONSTER_SPECIAL), so
     they come with step 9.
7. Into the game: fights in the campaign under the original rules, on screen.
   *In part, 2026-11-13:*
   - **Items stay the original's:** under the original rules an item's key is
     its record (`por::original_item_key`, without its readied byte and
     count, which the inventory keeps). The module reads the item types and
     the monster records from the game's folder (`original::Content`).
     Readying follows `ToggleReadyItem`: the class's items only, two hands at
     most, one item in each place but two rings, one stack each of arrows and
     quarrels, and a cursed item stays readied. A character's numbers are its
     sheet made into an original record, run with its readied items through
     `RecalcStats` as a monster's are. Its coins are the party's, so they
     weigh nothing (docs/DEVIATIONS.md).
   - **Town fights:** an area's monsters fight as their own records. Each is
     worth its record's experience (its award, and so much a Hit Point) shared
     among the party's members (`DetermineLootExp`), and then among each
     member's classes. A monster below 0 Hit Points, or one that
     surrendered, is beaten.
   - **Checks outside fights** take the original's chances: picking a lock by
     Open Locks, climbing by Climb Walls and forcing a door by Strength's
     open-doors table (`BashDoor`). The original's own door menu comes with
     step 6.
   - **Rest:** the camp's long rest is a day, which heals each member 1 Hit
     Point (`CheckRestingHeal`); the short rest heals nothing. The original's
     rest of any length, and memorizing, come with magic (plan step 5).
   - The Character Pool offers original-rules characters: four of each class
     a race may be, rolled the same each time.
   - `opengold_original_campaign_tests` and the game's
     `opengold_godot_creation_expedition_original` play the Slums under the
     original rules, the second on screen.
   - **Counts:** a fight takes a member's readied stacks and charges from
     its inventory (a charged item of the original's counts its charges) and
     reports what is left, which the inventory keeps: the arrows, quarrels or
     thrown weapons a missile spends, a potion drunk, a wand's charge. Arrows
     and quarrels are found by their item type (73, 28), as `RecalcStats`
     files them; step 4 had looked for them by their type's place, so no bow
     found its arrows.
   - **Treasure's experience:** a won fight's monsters' coins, gems and
     jewelry, and 400 for each plus of a magic item, add to its experience
     (`DetermineLootExp`).
   - **Armor Class** counts down as the original's does on every screen, the
     shop's preview and auto-equip's comparison included.
   - **Readying mid-fight** (2026-11-14; `ToggleReadyItem`, `RecalcStats`,
     `RecalcAttacksLeft`): a fight gets each member's record and carried
     items through the rules seam (`CharacterProfile::record`,
     `Participant::character_record`, the inventory's names), and each
     monster's from its record and MONnITM items (`Content::gear`). On its
     turn a member may ready an item or put one away on the combat screen's
     Items list ("Ready" and "Put away"), as the original allows readying
     (the class's items, two hands, one in each place but two rings, one
     stack each of arrows and quarrels, a cursed item kept); it takes no
     action. The creature's numbers are worked out again from its record and
     items, what the fight has done to it (drained levels, effects, Strength)
     kept, and one that has not attacked yet this round gets the new
     weapon's attacks, one that has no more than those. The original's AI
     readies its best weapon and shield at its turn (`EquipWeaponAndArmor`,
     `CalcWeaponPriority`) and readies another weapon when an enemy is next
     to its bow. The fight reports the items readied and what is left of
     each, which the inventory keeps; a save mid-fight keeps them (combat
     checkpoint 21). Secret's and the later records' item limits (their class
     bits) are read from them.
   - **Readied items' own effects** (2026-11-14; `ToggleReadyItem`,
     `ToggleItemEffect`, `EffGrantItemEffect`): an item readied lays the
     effect its byte 61 names on its bearer for as long as it stays readied,
     in a fight and from its start, and one put away mid-fight takes it with
     it. In *Pool of Radiance* the special byte (62) names a handler, and
     those that grant are 128 to 130, 133, 134, 136, 138 and 139 (the Ring
     of Fire Resistance's 61, the Cloak of Displacement's 89); from *Curse*
     on the byte's 0x80 alone does, the effect read by the fight's numbers
     (rings of invisibility, cloaks of displacement, *Secret*'s luckstone and
     giant-bane sword). A record that holds the effect already, as a
     monster's saved with its items does, keeps one. The effects the fight
     does not play yet do nothing: *Pool of Radiance*'s undead slayer (3) and
     flame tongue (6), *Curse*'s spell failure, dragon slayer, bane weapon
     and berserk (74 to 77). *Gateway*, *Pools of Darkness* and *Unlimited
     Adventures* grant by the byte's 0x80 alone, as *Curse* does (the kit's
     `item_effect`); *Treasures* by 0x0F and 0x8F too, but never Spiritual
     Hammer's own effect (23); each read by its game's numbers (Effects of the
     later games, below). The special byte's other kinds (Strength, spell
     slots, the aligned weapon) are their own items.
   - **Effects of the later games** (2026-11-15; `canonical_effect`, their
     `effhndlr.pas`, judged by what each handler does): *Gateway*'s are
     *Curse*'s, code for code, but for 160, which ends Spiritual Hammer as
     *Pool of Radiance*'s 137 does, and its own 126, 148 to 159 and 161
     (`gateway_effect_offset` higher). *Treasures*' are *Pools of
     Darkness*'s to 127 (its engine's, `darkness_canonical_effect`); past
     them its own (128 to 160, `vga_effect_offset` higher), but for 156,
     which holds its bearer still as *Curse*'s 136 does. Before, these games'
     records were read by *Pool of Radiance*'s numbers: *Treasures*' carrion
     crawler, whose 88 is a paralyzing touch, breathed lightning. The fight
     plays only the effects it knows by these numbers; *Gateway*'s and
     *Treasures*' own do nothing yet, nor do the ways *Gateway*'s
     alike-numbered ones differ from *Curse*'s (its shambling mound engulfs on
     1 to 4 of a d10, not on two hits; its hug makes two attacks). *Gateway*'s
     route raises at Sundabar's temple the member its shambling mounds now
     smother.
   - **The party's facing** (2026-11-14; `combatst.pas`,
     `InitCreatureCombatData`; `combatai.pas`, `AiMoveStep`): the fight
     knows the way the party faced as it met its foes
     (`rules::Encounter::party_facing`, kept by combat checkpoint 22). Each
     creature starts turned along it, the party's enemies back toward it,
     and a fleeing one of the party's enemies runs along it, one of the
     party's side the other way.
     The original turns a party facing north to north-west and one facing
     south to south-east on the field (its `quadDir`), and so does this;
     *Pools of Darkness*'s and *Treasures*' fleeing turns one step further
     clockwise (north stays north, east gives south-east), as their
     `AiMoveStep` adds 1 (2026-11-15).
   - **Heights for sight** (2026-11-14; `pathing.pas`, `WalkLine`): the
     original's terrain tables give each battlefield tile a clearance (how
     high one standing on it sees from) and a blocking height, read with
     their costs and frames from the games' programs (*Pool of Radiance*'s
     START.EXE, *Curse*'s START.EXE, *Gateway*'s GAME.EXE) onto the
     wilderness fields (`rules::Battlefield::clearance` and `blocking`).
     Sight walks the original's line square by square (its `PathFind` and
     `NextPathStep`), both ends too, and a square stops it whose blocking is
     above the clearance of the looker's square: in *Pool of Radiance* some
     ground no one can cross does not stop an arrow, and in *Curse* one on
     its hills (clearance 2) sees over trees and rocks. A dungeon's field has
     no heights: its walls block, as the original's do. The original weighs
     only the looker's square, though its line walk carries a height from
     the looker's square to the same square's again; this follows it.
     Spells' aims and areas use the same line.
   - **PARTY_MOVE_MODIFIER** (2026-11-14; `cbtacts.pas`,
     `CalcCreatureMovement`): what a script puts in the cell (0x6E72 in
     *Pool of Radiance*, its place in the later games' memory) is added to
     the movement of every creature on the party's side for the fight, a
     byte that wraps, so 253 takes 3 away; a movement outside 1 to 96 is 1,
     as the original's is, before Haste and Slow. The fight's end clears it,
     as before. *Secret*'s boots of speed (its effect 74) double a
     creature's movement (`RecalcMovement`).
   - Out of doors a fireball catches only those within 2 squares of its aim
     (`SpellFireball`).
8. Spells in combat. In parts:
   - 8a. *Done 2026-11-12:* the casting framework, with the first-level
     spells (`original_spells.h` holds the original's spell table). Cast
     offers each memorized spell; one with a casting time takes casting
     time / 3 off the caster's initiative and goes off at its next turn in
     the same round, aimed then, and damage before then loses it with its
     memorized copy. Targets: the caster, separate creatures in range and
     sight, or everyone within a radius of a square, allies too, found by
     lines from that square. Each target saves if the spell allows, a touch
     spell needs an attack roll, one damage roll is shared, and the effect
     goes through the immunities and magic resistance; an effect a creature
     has already is replaced by a longer one. Bless, Curse, Cure Light
     Wounds, Cause Light Wounds, Protection from Evil and Good, Resist Cold,
     Burning Hands, Charm Person, Magic Missile, Shield, Shocking Grasp and
     Sleep work; Detect Magic changes nothing in a fight; Enlarge and Reduce
     came with the Strength spells (plan step 5). A character has its race's innate effects:
     the Constitution saves of dwarves, gnomes and halflings and the elf's
     and half-elf's resistance to sleep and charm. A dwarf strikes orcs,
     goblins and hobgoblins (and GAGOOL and MACE) 1 better and a gnome
     kobolds and goblins; giants (ogres, trolls, fire and hill giants) strike
     both 4 worse, and bugbears and gnolls a gnome, by the monster's record
     name (`kin_of`).
   - 8b-1. *Done 2026-11-12:* the second- and third-level spells that work
     through the resolver and effects: Hold Person (the fewer held, the
     harder to save; not a creature of type above 1 or a large one), Resist
     Fire, Silence 15' Radius (no casting within a square of the silenced;
     its save is rolled but, as the original compiled it, changes nothing),
     Detect Invisibility, Invisibility and Invisibility 10' Radius, Mirror
     Image, Ray of Enfeeblement (a quarter less weapon damage), Cure and
     Cause Blindness (4 worse to hit, to be hit and to save), Cause Disease
     (nothing in the fight), Prayer (+1 to its side's attacks and saves
     within 6 squares, -1 to the other side's), Remove and Bestow Curse (4
     worse attacks and saves), Blink, Fireball (one roll for all within 3 of
     the aim, the caster's own too), Haste and Slow (double or half the
     rates of movement and attacks, up to the caster's level of creatures,
     one under the other spell only freed of it), Protection from Evil and
     Good 10' Radius (an aura of a square) and Protection from Normal
     Missiles. Haste ages its creature a year the first time it works on it
     (`EffHasted`, as the round starts), camp Haste too (an age that changes,
     Plan step 3); a fireball out of doors reaches 2 and not 3 (step 7).
   - 8b-2. *Done 2026-11-13:* Lightning Bolt (one roll at the aim and along
     8 squares beyond it, away from the caster, turned back by a wall, its
     length counted as the original counts it), Stinking Cloud (a cloud of up
     to four squares for the caster's level in rounds; those in it save
     against paralysis or are helpless with nausea, and cough, unable to
     cast and easier to hit, when they save), Dispel Magic (each effect below
     strength 255 on a d100 by the caster's level against the effect's),
     Animate Dead (the human dead rise whole for the caster, and collapse at
     0), Slow Poison (one killed by poison stands with 1 Hit Point, and dies
     if the poison outlasts the spell) and Snake Charm. Enlarge, Reduce and
     Strength came with magic (plan step 5).
   - 8b-3. *Done 2026-11-14:* Spiritual Hammer (`SpellSpiritualHammer`,
     `EffSpiritualHammer`) gives its caster the spell's effect (23) for a
     round a level and a hammer +1 named "Spiritual Hammer" while it lasts,
     unless it has one or carries 16 items ("{name} gains an item."). Each
     game makes it its own way: *Pool of Radiance*'s (item type 20) comes
     unreadied, and put away it ends the spell (its special byte's handler
     137, `EffHammerGone`); *Curse*'s and *Gateway*'s (type 20) come readied
     as `ToggleReadyItem` allows, and stay put away; *Secret*'s and the later
     games' (type 6) come readied to a hand with no weapon, and put away end
     the spell. When the spell ends, by its time, Dispel Magic or the hammer
     put away, the hammer is gone. A spell still running as the fight ends
     outlasts it by its minutes (2026-11-15), as the original's effect does:
     the hammer joins its caster's items, readied as the fight left it
     (`CombatantView::items_gained`), and goes as the spell runs out with the
     campaign's clock (`RulesModule::item_lasts`) or in a later fight. The
     original knows the hammer by its type and name word (`is_spiritual_hammer`).
     *Secret* and the later games mean to ready it to an empty weapon hand but
     never do (Bugs).
   - 8c. *Done 2026-11-12:* turning undead, once a fight for one with cleric
     levels: one d20 for the whole attempt against up to 1d12 undead enemies
     in sight, the lowest kinds first and the nearest of those, by the
     original's table; each it beats is turned (forced to flee for the rest
     of the fight) or destroyed, and the first it fails ends the attempt.
     Destruction on a roll goes on to at least six; destruction without one
     (a 0 in the table) does not, as in the original.
   - 8d. *Done 2026-11-13:* spells from items. A creature's usable items
     (`UsableItem`): a scroll (an item of equip slots 11 to 13) with up to
     three spells, or another item with a spell or one of the original's
     item powers (57 to 67) and its charges. A readied one is used at once,
     a real spell at level 6 and a power at 12, and its turn ends; a scroll's
     spells are offered once it is known (or by Read Magic, or a cleric's
     scroll to a cleric) and each is read once; a stack loses one of it,
     another item a charge, gone at 0, and an item of no charges never runs
     out. The powers: the Potion of Speed, Keoghtum's Ointment, the Javelin
     of Lightning, the Wands of Paralyzation and of Magic Missiles (which
     Shield does not stop), the Potion of Healing, Dust of Disappearance
     (never singled out, 4 harder to hit) and the Necklace of Missiles; the
     Potion of Giant Strength came with Strength (plan step 5).
9. The original's AI. *Done 2026-11-13:* the command `quick` plays a
   creature's turn as the original's AI does (`CombatAiControl`): its
   approach pattern, then an item, a spell begun earlier, turning undead, a
   spell (for monsters and NPCs, and for a party member only with the
   party's Quick magic on: `quick_magic`, Curse's ALT M), and else the
   fight: a lock on an enemy kept while it may be singled out, else a random
   enemy in sight, else one through walls; up to 20 passes of a step or an
   attack on an enemy in its weapon's reach; steps by the original's six
   approach patterns, changing pattern, dropping the lock and stopping when
   stuck, where a monster or NPC that has not fought this turn takes the
   shortest way to an enemy instead (a departure,
   [DEVIATIONS.md](DEVIATIONS.md#the-original-rules)); a lone magic-user that is not fleeing never walks; with nothing
   to do it guards (or ends, with a missile weapon). A fleeing creature runs
   first and tries to get away at the edge. The monsters' specials fire as
   it starts to move and attack: the petrifying gaze (stoned; a mirror turns
   a reflectable gaze back), the charming gaze, the lightning breath (always
   in the first round, then half the time, for its maximum Hit Points, 3, 2
   or 1 breaths) and the ankheg's acid, once a fight; and the fire and acid
   touches (79, 80) burn on a hit. The AI readies its best weapon and shield
   at each turn (`EquipWeaponAndArmor`) and readies another weapon when an
   enemy is next to its bow, which ends its turn (step 7: many monsters carry
   their weapons unreadied, Pool of Radiance's goblin guards their short
   swords). A fleeing creature runs along the party's facing (step 7). Each
   step is a stop, so it never
   ends on another creature's square. The game's computer turns under these
   rules (monsters, allies and party members on Quick) are this AI's: the
   shared command chooser takes `quick` (or `quick_magic`) whenever the rules
   offer it.

## Encumbrance

What a creature carries slows it: every item, by its weight times its
quantity, and every coin, one coins' weight each. A readied bag of holding
carries 5,000 coins' weight for nothing, though never less than what is
readied (`charhlp.pas`, `RecalcStats`). Strength allows a load before it
slows (`WeightAllowance`):

| Strength | 3 or less | 4–5 | 6–7 | 8–11 | 12–13 | 14–15 | 16 | 17 | 18 | 18/01–50 | 18/51–75 | 18/76–90 | 18/91–99 | 18/00 | 19 | 20 | 21 | 22 | 23 | 24 | 25 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Allowance | −350 | −250 | −150 | 0 | 100 | 200 | 350 | 500 | 750 | 1,000 | 1,250 | 1,500 | 2,000 | 3,000 | 4,000 | 5,000 | 6,000 | 7,500 | 9,000 | 12,000 | 15,000 |

Beyond it, the load sets a slowest movement (`EncumbranceAdjust`); a load
never speeds a creature up:

| Coins over the allowance | 0–512 | 513–768 | 769–1,024 | More |
|---|---|---|---|---|
| Movement at most | its own | 9 | 6 | 3 |

## Difficulty

The game's difficulty setting (`settings.cfg`'s difficulty, the Options
screen) chooses a level; under the original rules it changes the monsters a
fight meets and nothing else (Decisions, 2026-11-15): their THAC0, Hit Points
and Armor Class. One function holds the table, `original_monster_strength`
(difficulty.h), which *Unlimited Adventures* also uses with each design's
own setting. The game applies it as a script's COMBAT starts
(`apply_original_difficulty`), to the monsters of the opposing side only, and
the fight gives each its Hit Points by the percent (at least 1), its THAC0
and both its Armor Classes (`rules::Participant::original_strength`; the
SRD's `hit_points_percent` and `attack_bonus` stay the SRD's).

- **Hit Points by the game's own levels:** *Secret*, *Gateway*, *Pools of
  Darkness* and *Treasures* scale a hostile monster's Hit Points as LOAD
  MONSTER loads it by their camp's level setting (Novice to Champion; cell
  4CF4h in *Secret*, *Gateway*'s DIFFICULTY, the game state's level in the
  256-color games): (level + 2) / 4 of them, at least 1
  (`original::scales_monster_hit_points`, `game_hit_points_percent`). Squire,
  Veteran (their default), Adept and Champion are Easy, Normal, Hard and
  Nightmare, which their quarters match; Infernal goes two levels past
  Champion, twice the Hit Points. Their Novice, half the Hit Points, has no
  level here.
- *Pool of Radiance* and *Curse* had no levels: the camp's Alter menu offers
  only Order, Drop, Speed, Icon and Pics. The table's Hit Points are theirs.

The originals also have the levers their designers used to tune fights:

- **Party strength.** The scripts read the party's strength (the PARTY STRENGTH
  command: Hit Points, Armor Class, THAC0 and cleric and magic-user levels) and
  size encounters from it.
- **To-hit and movement.** A script can raise or lower the monsters' THAC0 and
  movement for a fight.
- **Morale.** Monsters flee or surrender when their morale breaks.

The levels leave these as the scripts set them.

**OpenGoldBox's difficulty table** (the user, 2026-11-15) changes the
monsters only: Easy -2 to hit, 75% Hit Points and Armor Class 2 worse;
Normal as recorded; Hard +1, 125% and 1 better; Nightmare +2, 150% and 2
better; Infernal +3, 200% and 3 better (`original_monster_strength`,
difficulty.h). A fight under the original rules lays a creature's
`rules::Participant::original_strength` on it as it begins (its Hit Points,
at least 1, its THAC0 and its Armor Class) and a `CampaignEncounter`'s
`party_to_hit` on the party's side. So far only *Unlimited Adventures*
plays it (Unlimited Adventures, above); a game whose original already
scales Hit Points takes only the table's to-hit and Armor Class.

## Bugs in the original

| Bug | Direction |
|---|---|
| Creation never applies racial ability adjustments: a leftover variable makes every character human (`chargen.pas`, the ability roll) | Fixed (the user, 2026-11-11) |
| *Unlimited Adventures:* a ranger above level 8 learns a magic-user spell at every training, its ranger level risen or not (`train.c`, `train_hero`) | Only when its ranger level rises (the user, 2026-11-14) |
| *Unlimited Adventures:* Protection from Evil's -2 to an evil attacker (monster effects 8 and 45) is set before the attack roll, which overwrites it (`fxcombat.c`) | The fight's own effects 8 and 45 apply it |
| *Unlimited Adventures:* a displacing monster (effect 89) turns aside one attack a fight, its effect readied only as the fight starts | Once a round, as the fight's effect 89 |
| *Unlimited Adventures:* the shambling mound (effect 173) heals a flat 8 from electricity, past its maximum, its 1d8 thrown away; the owlbear's hug (220) never squeezes | 1d8, and the hug squeezes, as the fight's 284 and 296 |
| *Unlimited Adventures:* a troll (effect 216) tests a blow's element against 0, so acid never keeps it down | Fire and acid keep it down, as the fight's 100 |
| *Unlimited Adventures:* bashing a door judges the wizard lock's harder table by wall kinds 5 and 6 (a secret wizard lock and the first key's door), so a plain wizard lock (4) bashes as an ordinary lock (`move.c`, `door_bash`) | Kinds 4 and 5, the two wizard locks |
| *Unlimited Adventures:* dead and unconscious members bash doors and cast Knock (`move.c`, `door_bash`, `spell_caster`) | Only members who can act |
| *Unlimited Adventures:* the thief's row of the saving throw table is empty, the thief's numbers sitting in the unused knight's row, so a thief makes every saving throw (`chartab.c`, `save_table`; `herostat.c`) | A thief saves by the thief's numbers, Pools of Darkness's |
| *Unlimited Adventures:* its saving throws at level 19 repeat level 18's for clerics and thieves, as Pools of Darkness's do | Level 20's, as there (the user's answer for Pools of Darkness) |
| *Unlimited Adventures:* a rest encounter with a chance but no minutes between rolls divides by zero (`rest.c`, `encounter_roll`) | It never rolls |
| *Unlimited Adventures:* a price rate above 19 reads past the kit's rate tables (`temple.c`, `rate_price`) | Taken as rate 19, a hundred times the price |
| *Unlimited Adventures:* a wrong password says "wrong, try agin." (`evcmds.c`, the password event) | Spelt "Wrong, try again." |
| *Unlimited Adventures:* an NPC refused by a full party still has its Hit Points set, on whichever member was current (`evcmds.c`, the NPC event) | A full party changes no one |
| *Unlimited Adventures:* a ghast's stench (effect 137) gives its -2 to attacks (148) to the ghast itself, though it names each enemy that fails its save as affected (`effects.c`, `fx_adjacent_save_aura`) | Each enemy that fails its save takes it |
| *Unlimited Adventures:* a gorgon spends one of its four breaths (203) at every turn of its AI, whether it breathes or not (`effects.c`, `fx_gorgon_breath`) | Only a breath spends one, as the kit's dragons' do |
| *Unlimited Adventures:* a mummy's and an efreeti's fire by the die (168, 219) count the dice of whatever was rolled last, so a breath, which rolls none, is weighed by an earlier roll's (`effects.c`, `damage_dice_count`) | A spell's own dice; damage with none is weighed by none |
| *Unlimited Adventures:* the black pudding "is Disolving" armor (`effects.c`, `fx_armor_acid`) | Spelt "is dissolving" |
| *Unlimited Adventures:* a monster readies its items one after another without counting what it has readied: its hands stay at the record's, and a second ring, arrows and quarrels never take their places, so it may ready two two-handed weapons and a shield, or any number of rings (`saveload.c`, `mon_load`) | Each readied item takes its hands and its place |
| Age gives no young adult the +1 Constitution of AD&D's table, which every older age group keeps (`chargen.pas`, the ability roll). The other changes match AD&D's table added up | The Aging setting: As the original keeps it, AD&D adds it (the user, 2026-11-12) |
| The thief skill table's Dexterity adjustments break their own order at Dexterity 10 (pick pockets −19, between −15 at 9 and −5 at 11) and 16 (open locks −5, between 0 at 15 and +10 at 17) (`gdconsts.pas`, `dexAdj`) | Corrected to −10 and +5 (the user, 2026-11-12) |
| A thief's chance is kept in a byte, so a Dexterity penalty above it wraps around: a level 1 thief of Dexterity 9 moves silently 251% of the time (`classcal.pas`, the thief skills) | Stops at 0 |
| A hit's damage is added up in a byte, so dice plus a negative modifier below zero wrap around to more than 250 damage (`cbtacts.pas`, `CalcWeaponDamage`) | Counts as 0 damage (the user, 2026-11-12) |
| Armor of 151 to 399 coins sets movement to 9, and heavier armor to 6, whatever the creature's own movement, so a creature slower than that moves faster in armor (`charhlp.pas`, `ArmorMoveAdjust`) | Armor slows only by its weight in the load, which only slows (the user, 2026-11-12) |
| A missile weapon of range 3 or less counts every target as beyond two range bands, so its target's Armor Class is 5 better even next to the shooter (`cbtacts.pas`, `ApplyRangePenalty`) | No distance penalty within a weapon's range (the user, 2026-11-12) |
| Bandage saves the first dying party member anywhere on the field (`combat.pas`, `BandageDying`) | The one who bandages must stand next to the dying member (the user, 2026-11-12) |
| A saving throw is added up in a byte, so a total below 0 (a low roll with penalties) wraps around and saves (`fxengine.pas`, `RollSavingThrow`) | A total below 0 fails |
| A drider's paralysing touch passes a save penalty of 2 that the paralysis ignores, so it is no harder to save against than a thri-kreen's (`effhndlr.pas`, `EffDriderPara`) | The penalty of 2 counts |
| Displacement is readied only as the fight starts, so it turns aside one attack a fight, where its notes say one a round (`effhndlr.pas`, `EffDisplaced`) | One a round |
| Evading a missile or boulder takes the hit from the primary attacks' count whatever attack it was, a byte that wraps below 0 (`effhndlr.pas`, the missile evasion) | From the count of the attack evaded |
| A free attack's check that an attacker sees the invisible reads the creature leaving instead (`cbtacts.pas`, `AttacksOfOpportunity`) | The attacker's own sight |
| Magic Missile throws level div 2 missiles, none at level 1, where the manual promises one at levels 1 and 2 (`magic.pas`, the missile) | One missile for every two levels from level 1, as the manual says |
| Protection from Evil or Good takes 2 from an evil or good attacker's roll before that roll is made, which overwrites it (`effhndlr.pas`, `EffProtEvil`) | The 2 counts |
| A dwarf's or gnome's defense against giants judges the defender's own target instead of the attacker, and reads eight names from its list of five (`effhndlr.pas`, `EffDwarfGiantDefense`) | The attacker, among the five giants |
| Mirror Image takes only a spell aimed at its caster, never a blow, where the manual says the images take attacks (`effhndlr.pas`, `EffMirrorImage`) | The images take blows too |
| Dispel Magic works on the first creature in its area, once for each creature there (`magic.pas`, the dispel) | Each creature in the area once |
| Giving up an item's aim in a fight still spends its charge or scroll spell (`viewchar.pas`, `UseItem`) | The turn ends, nothing is spent |
| A monster's AI skips any locked target on the party's side when it looks for an enemy in reach, so monsters never strike their lock first (`combatai.pas`, `AiMoveAttack`) | The lock counts on either side |
| The AI strikes an enemy next to it that it may not single out, invisible, blinking or dusted (`combatai.pas`, `AiMoveAttack`) | Only one it may single out |
| When nothing is in reach and no enemy can be found, the AI ends its turn twice, the second time wiping its guard (`combatai.pas`, `AiMoveAttack`) | It guards |
| The AI's 20-pass limit lets the 21st pass run on (`combatai.pas`, `AiMoveAttack`) | 20 passes |
| A combat-only ally without a morale of its own gets the encounter's morale unhalved, so it fails every check from 52 up and flees or surrenders at once (`combatst.pas`, `InitCreatureCombatData`) | The encounter's morale |
| The charming gaze's reach is an unset value (`effhndlr.pas`, `EffCharmGaze`) | What the gazer sees |
| A dragon with no enemy in reach of its breath breathes on itself and spends the breath (`magic.pas`, `EffBreatheLightning`) | It holds its breath |
| The ankheg's acid is not marked as acid (`effhndlr.pas`, `EffSpitAcid`) | Acid |
| A level drain divides by the victim's class levels, so a creature without any (a monster) divides by zero (`effhndlr.pas`, the level drain) | It loses its highest level's share |
| A level drain keeps the drained class's level as the victim's highest level, so a fighter 5 / thief 6 drained in fighter counts as level 4 (`effhndlr.pas`, the level drain) | The highest of its classes |
| Each hit of a holding bite's own bite starts another hold, stacking holds on the biter and immobility on the victim, which can outlast the fight (`effhndlr.pas`, `EffBiteHold`) | One hold at a time |
| Strength 19 and above has no row in the open-doors table, so a member that strong never forces an ordinary door (`explore.pas`, `BashDoor`) | It forces one as 18/00 does, 5 in 6 |
| *Curse* and *Secret:* past a class's Hit Dice, training's fixed Hit Points (3 a fighter or paladin, 2 a cleric, ranger or thief, 1 a magic-user) replace the other classes' rolls instead of adding to them (`ovr018`, `sub_509E0`) | Added to them |
| *Curse*, *Secret* and *Unlimited Adventures:* the paladin's saving throws, otherwise 2 better than a fighter's, are 1 worse against petrification at levels 5 and 6 and against death at level 8 than that (`ovr026`, `SaveThrowValues`) | 2 better throughout |
| *Curse* and *Secret:* their starting-age tables have no row for a half-elf ranger, which starts aged 0 (`Gbl.cs`, `halfelf_ages` in its reconstruction) | A half-elf fighter's age |
| *Secret:* poison rides only a creature's second attack, so the centipede, the giant snake and spider, the medusa and the Sargatha, which attack only with their first, never poison (`fxengine.pas`, its conditions 2 and 3) | A creature with no second attack poisons with its first |
| *Secret* and *Curse:* magic resistance's chance is kept in a byte, so a caster above level 11 plus a fifth of the resistance wraps it and is always resisted: the drider is proof against casters of level 15 and up (`fxengine.pas`, `CheckSpellResistance`) | The chance only lessens with the caster's level |
| *Secret:* cold vulnerability doubles cold damage by whether the game was saved, not whether the creature saved (`effhndlr.pas`, `EffColdVulnerable`) | Doubled unless the creature saved |
| *Secret:* the Constitution bonus to saves adds an unset value at Constitution 3 and from 21 up (`effhndlr.pas`, `EffConPsnSave`, `EffConSpellSave`) | No bonus there |
| *Secret:* a giant slug with no enemy in reach still spits and misses, at whichever creature was aimed at last (`effhndlr.pas`, `EffSpitAtTarget`) | It does not spit |
| *Secret:* its thief skill adjustments give humans the half-orc's (−5 Pick Pockets, −10 Read Languages and the rest), so a human thief starts worse than in the earlier games (`raceAdj` in its reconstruction) | No adjustment for humans, as before |
| *Secret:* a paladin with no cleric or magic-user levels casts its spells at level 6 whatever its own, as its check for a caster leaves the paladin out where *Curse*'s has it (`charhlp.pas`, `GetCasterLevel`) | At its paladin level, as *Secret* reckons a paladin's spells otherwise |
| *Secret:* the spells a magic-user learns while trained at creation include, at level 6, Cure Light Wounds, a cleric's spell (`chargen.pas`, TrainingMenu) | Left out |
| *Gateway:* Neutralize Poison at the temple stands up any member at 0 Hit Points or dead, poisoned or not, once the player pays for it anyway (`temple.pas`, `NeutralizePoison`) | Only one that is poisoned |
| Scribing marks the first byte equal to the spell's number on any item the character carries, readable or not: when a wand or potion comes first, its byte (charges or power) is changed and the scroll's spell is never scribed (`camp.pas`, `CampScribeMenu`) | Only a spell on a scroll the character can read |
| Haste marks the creature it has aged with bit 0x10 of its strength, which is the caster's level, so a caster of level 16 to 31 sets it already and its Haste never ages anyone (`effhndlr.pas`, `EffHasted`) | It ages them as any Haste does (the user, 2026-11-14) |
| Enlarge cast above level 6 in *Pool of Radiance*, or above 11 in *Curse*, gives whatever Strength the last Enlarge left, as no table row sets it (`magic.pas`, `SpellEnlarge`) | The most the table gives |
| *Pool of Radiance:* Strength cast on a creature of no class it names, and in *Curse* on a paladin or ranger, adds a die never rolled (`magic.pas`, `SpellStrength`) | A d4, and a warrior's d8 |
| *Curse:* Entangle's hold lasts as long as the spell table's row past its end says, an unset value (`magic.pas`, `SpellEntangle`, its duration taken for spell 136) | Entangle's own 10 rounds |
| *Secret:* the Delayed Blast Fireball asks for its delay at the keyboard whoever casts it, the computer's creatures too (`magic.pas`, `SpellDelayedBlastFireball`) | The computer bursts it at once |
| *Secret:* its +1 to hit with long and short swords and bows goes to every race (`charhlp.pas`, `WeaponAdjust`), where the earlier games give it to elves | Elves only (the user, 2026-11-14) |
| *Secret*, *Pools of Darkness* and *Treasures:* Spiritual Hammer readies its hammer when no weapon is readied, but its search for the new hammer steps past it, so it never readies it (`effhndlr.pas`, `EffSpiritualHammer`) | Readied to the empty hand |
| *Pools of Darkness* and *Treasures:* LOAD MONSTER keeps a monster's scaled Hit Points in a byte, so Adept or Champion wraps a monster of more than 204 or 170 around to a few (`eclvm.pas`, `EclLoadMonster`) | No wrap |
| *Curse*, *Gateway* and *Treasures:* training picks the class whose next level needs the most experience, but reads every class at the level of the last one it looked at (TrainingMenu) | Each class at its own level |
| *Curse:* Hold Monster holds only what Hold Person can, as the spell its resolver exempts from the creature-type rule is Dimension Door (0x53) and not Hold Monster (0x5E) (`ovr023`, `MultiTargetedSpell`) | Any creature |
| *Curse:* Ice Storm and Cone of Cold deal acid damage, so resisting cold does not lessen them (`ovr023`, `SpellIceStorm`, `SpellConeOfCold`) | Cold |
| *Curse:* a shambling mound struck by electricity rolls 1d8, throws the roll away and gains 8 Hit Points (`ovr013`, its absorbing of lightning) | It gains the 1d8 |
| *Curse:* an owlbear's hug pins its victim, but the squeeze that should follow is in no list of the effects the fight calls, so it never hurts (`ovr014`, `AffectOwlbearHugRoundAttack`) | It squeezes for 2d8 each turn |
| *Curse:* a giant slug's acid sets no damage type, so it takes the last attack's (`ovr023`, `AffectSpitAcid`) | Acid |
| *Curse:* Faerie Fire improves its target's Armor Class by 2, and sets any of 2 or better to 0, instead of making it easier to hit (`ovr013`, `FaerieFire`) | 2 worse |
| *Curse:* Charm Monsters, like Hold Monster, takes only what Charm Person can (`ovr023`, `MultiTargetedSpell`) | Any creature |
| *Curse:* the dracolich's shield against fear, enfeeblement and feeblemind is looked at only once those have landed, so it does almost nothing (`ovr013`, `AffectDracolichA`) | It is immune to them |
| *Pools of Darkness:* a new character's count of items is set to three whatever its starting equipment gave, so a fighter's five items and a cleric's six count as three (`chargen.pas`, `CreateStartingItems`) | Counts what it carries |
| *Pools of Darkness:* the clerics in Manshoon's tower ask how many times the party has entered Zhentil Keep and add 2 to the answer's place (0 for "1") before comparing it with the count, so no true answer passes and the party fights their elementals (ECL1:49, 816Ah) | The script adds 1: "1", "2" and "MANY" mean 1, 2 and 3 |
| *Pools of Darkness:* Power Word Blind works out its blindness's time (1d4+1 rounds, ten times as long under 51 Hit Points) and then blinds for the spell's table time, 0, which never ends (`magic.pas`, `SpellPowerWordBlind`) | The time it works out |
| *Pools of Darkness:* its saving-throw table gives a cleric and a thief at level 19 the row of levels 16 to 18 and 13 to 16, though the rows on either side follow the classes' steps of three and four levels (`saveTbl` in its reconstruction) | Level 20's row from 19 |
| *Curse:* the wilderness script (ECL1 80) marks a camp with a cell Tilverton's sage and Kybor's library also set for themselves (0x4C13), so a party that paid the sage reaches the wilderness as if back from camp: it skips loading the roads from Tilverton, and the first journey goes where stale cells say | Arriving from another script, the party is not back from camp |
