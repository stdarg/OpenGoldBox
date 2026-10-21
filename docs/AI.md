# Combat AI

Scope: [#266](https://github.com/stdarg/OpenGold/issues/266). How the computer
plays monsters and the party members on Quick, what the original Gold Box
engine did, where we depart from it, and the decisions still open.

Sources: our policy is `choose_from` and `choose_in_sight` in
`src/OpenGold.Core/src/combat_demo.cpp`, driven by `CombatView` in
`src/OpenGoldBox/combat_view.cpp`; the rules module decides morale, forced
movement and the order of every menu. The original is read from coab, the Curse
of the Azure Bonds reimplementation of the same engine (`engine/ovr010.cs`
`PlayerQuickFight` and the routines it calls, `ovr014.cs` `find_target`); no
code is copied, and Pool of Radiance may differ in tuning and spells.

## Who the computer plays

One policy plays every monster and every Quick member, in the original as here.
The computer takes the turn when the actor is a monster, when the party is
fleeing (FLEE-1), or when a party member is on Quick (QUICK-1).

- **Quick** hands the acting member to the computer; **Q** hands over the whole
  party; **Take control** or Space hands every member back. The campaign keeps
  who is on Quick between fights.
- **Quick magic** (M) decides whether Quick members cast spells, cantrips
  included. Off, the policy is given the same menu without spell commands
  (`without_spells`); class features that are not spells (Lay On Hands, Rage,
  Turn Undead, Second Wind) stay. Magic items and scrolls are used either way
  (#263, MAGIC-6), as in the original. Monsters always cast.
- **Reactions** belong to whoever reacts: a monster's or a Quick member's is
  answered by the computer, a member the player controls is asked. In Flee
  mode the computer answers for every member.
- The computer waits 0.65 seconds between actions so the player can follow.

The original differed in three small ways: its Magic toggle was switched off
at the start of every fight (QUICK-1 keeps it), the same AI also played
charmed, confused and berserk characters, and allied NPCs had their own morale.
Our allied NPCs are played by the player.

## The turn

Our policy works down one list and takes the first command that applies:

1. Answer a pending choice: initiative, an optional effect, Temporary Hit
   Points.
2. Stand up from Prone.
3. A panicked monster, or a member in Flee mode, runs for the edge.
4. Finish aiming a spell it has begun (Magic Missile's darts, Moonbeam).
5. Free riders: a Paladin's free Divine Smite, Hunter's Mark, Horde Breaker,
   Divine Strike, Spiritual Weapon; then Rage, Sacred Weapon, escaping vines.
6. A reaction, or Second Wind at half Hit Points or less.
7. Magic items and scrolls.
8. Healing: Lay On Hands and Divine Spark, Turn Undead and Preserve Life, cure
   spells, Stabilize, shaking a sleeper awake.
9. Buffs and setup: Innate Sorcery, Produce Flame, Shillelagh, Hex, Spiritual
   Weapon, Bardic Inspiration, the Druid's Moonbeam and Wild Shape.
10. Control spells: Blindness, Hold Person, Hideous Laughter, Ray of
    Enfeeblement.
11. Fire and acid against a regenerating enemy (trolls).
12. Attacks, in a fixed order (below).
13. With the Action spent: Flurry of Blows, Martial Arts, moving Hex or Heat
    Metal, then End turn.
14. Otherwise move toward the nearest enemy, draw a bow, or take off a shield
    to shoot; else End turn.

The original's turn ran: morale check, a magic item, finishing a slow spell,
Turn Undead, a spell, choosing a weapon, then moving and attacking.

## 1. Movement

**Now.** A creature with any attack in range attacks; otherwise it moves to
the square that brings it nearest, by walking distance around walls, to the
nearest living enemy. The walk counts every step as one, so it neither avoids
Difficult Terrain nor Opportunity Attacks, and it never Disengages. Ranged
attackers and casters close in like everyone else. Exceptions:

- An orc's Aggressive moves it toward an enemy as soon as it has no attack.
- A creature that knows a ranged cantrip and sees no enemy steps to the nearest
  square with a line of sight to one.
- Commanded, frightened, turned, incapacitated or lost creatures make the
  moves the rules force on them; the policy sees only that command.
- A panicked monster runs for an edge nearer to it than to the party; a
  fleeing member runs for the nearest edge. Both Dash when out of movement.

**Original.** It walked straight at its target with no pathfinding, trying
other headings in a fixed order when blocked, and gave up after being blocked
three times. It also ignored attacks of opportunity. One stand-off rule: a pure
magic-user without armor never advanced and stood guard instead. A bow user
walked until a target was in range.

**Departure.** Our walk finds a way around walls where the original could get
stuck; the magic-user's stand-off is not copied (question 3).

## 2. Target choice

**Now.** For each attack in turn, the enemy with the fewest current Hit Points
among those the attack can reach (ties: initiative order). Movement goes
toward the nearest enemy, which may not be the one it later attacks. No
preference for an enemy already engaged, a caster or a threat.

**Original.** Kept its current target while it was visible and in the fight;
otherwise picked a random visible enemy. Enemies within reach of its weapon
came before the target it was walking to. It never read Hit Points or AC.

**Departure.** Ours focuses the weakest enemy; the original chose at random
(question 1). Our game shows every creature's Hit Points to the player, so
reading them is not hidden knowledge here.

## 3. Attacks

**Now.** The order is: slot attack spells (Magic Missile, Scorching Ray, Acid
Arrow, Mind Spike, Inflict Wounds, Guiding Bolt, Dissonant Whispers), Eldritch
Blast and other strong cantrips, flame weapons, **melee**, the remaining attack
cantrips, then the **held ranged weapon**. A creature that cannot get closer
draws a carried bow, taking off its shield if the bow needs both hands.
Weapon masteries' optional effects are always used. Sneak Attack, Savage
Attacker and a Versatile grip apply automatically (AUTO-1).

Never chosen today: thrown weapons, Two-Weapon Fighting (Light and Nick
attacks), Action Surge, smites that cost a slot, Cunning Action, Steady Aim,
Dodge, Disengage, Hide, Breath Weapon, Dragon's Breath, putting out flames.

**Original.** It scored every weapon it could use (damage dice, magic bonus,
extra attacks, one hand) and readied the best ranged weapon whenever no enemy
stood next to it and the bow scored at least half as well as the best melee
weapon, switching back to melee when an enemy closed. Breath and gaze
attacks fired by chance each round.

**Departure.** Ours keeps its weapon and draws the bow only when it cannot get
closer (question 6); many class features are never used (question 7).

## 4. Spells

**Now.** Only the spells named in the lists above, at their base level (the
level-2 Magic Missile and cure spells only when the level-1 cast is not
offered). Area spells other than Moonbeam are never started: no Fireball,
Burning Hands or Shatter. Moonbeam is placed only where no ally stands on or
next to its center. Control spells go on the first enemy showing no condition,
whether or not the caster is already Concentrating. Magic Missile's darts all
go to one enemy while they can.

**Original.** Each spell had a priority from 0 (never cast by the AI) to 7
(Fireball, Hold Monster). The caster rolled 1d7 and, from priority 7 down to
the lowest that roll allowed, tried three random spells it had memorized,
casting the first that made sense: an enemy in range for a harmful spell, a
wounded ally for Cure Light Wounds, and for an area spell no ally at risk (it
rolled each ally's saving throw; monsters cast nearly regardless). Harmful
spells went on a random visible enemy; helpful ones on the caster.

**Departure.** Ours is a fixed order without chance and never casts area
spells (questions 4 and 5); it can break its own Concentration (question 8).

## 5. Magic items

**Now.** Items come before healing and attacks, in the order the menu lists
them, whatever Quick magic says (#263):

- a healing scroll or potion on an ally at or below half Hit Points;
- any other scroll on a conscious enemy; a scroll that only helps an ally
  waits for the player;
- the Potion of Speed and Giant Strength on the user;
- the Wand of Paralysis on an enemy not already Paralyzed;
- Magic Missile items on any enemy;
- an area item only where it catches an enemy and no ally within its reach.

Dust of Disappearance is left to the player. Monsters carry no items into a
fight yet (#268).

**Original.** The same 1d7 priority as spells, applied to readied items with a
spell effect (never scrolls), with the same sense checks; usable with Magic
off. coab's loop keeps the first sensible item in inventory order, which may
be a translation slip.

**Departure.** None that matters: both use the first sensible item.

## 6. Healing

**Now.** Lay On Hands and Divine Spark, then the cure spells, go to the first
ally the menu lists below half its Hit Points (0 counts). Stabilize only when
no enemy stands next to the healer; monsters never stabilize.

**Original.** Cure Light Wounds went to the adjacent ally with the fewest Hit
Points, or to a dying one when the others had 8 or more; a Quick character
bandaged the first dying member anywhere on the field.

**Departure.** Ours heals the first wounded ally listed, not the most wounded
(question 9), and uses "below half" for spells but "at or below half" for
items and Second Wind (question 10).

## 7. Reactions and optional effects

**Now.** The computer always makes Opportunity Attacks, and always uses
Shield, Deflect Attacks, Hellish Rebuke and the Goliath boons when offered.
Uncanny Dodge, Superior Defense, Retaliate and Counterspell are used too, but
only because the rules list each before Decline and the policy falls back to
the first command. Pending choices take the first option, which the rules
order as the default: keep initiative, use Tactical Mind, use every optional
effect. Temporary Hit Points: keep the current amount unless the new one is
larger.

Two answers look wrong. Shield is cast on every hit, even when +5 AC cannot
turn it into a miss (question 11). A Champion who earns a free move takes it,
then ends it at once, because the move menu lists Finish first (question 12).

**Original.** Reactions as such did not exist.

## 8. Fleeing and morale

**Now (MORALE-1, FLEE-1).** Monsters check morale at the start of each turn,
from their own morale and their side's remaining Hit Points; broken, a monster
flees in panic if no opponent is faster, surrenders if one is and it has an
Intelligence above 5, and otherwise fights on. It may rally. The party flees
only when the player presses Flee; members who cannot flee fight on, and then
cast spells even with Quick magic off (question 13).

**Original.** The same tests (this is where MORALE-1 came from), plus Bless
and Curse changing morale by 5. Party members never checked morale.

**Departure.** Bless and Curse are not adopted (MORALE-1).

## 9. What the computer may know

The computer must not use what a player could not know: hidden rolls, a
hidden enemy's position, or anything the game does not show.

- **Hidden enemies** (HIDE-1) are removed from what the policy sees; with none
  in sight it Searches.
- **Hit Points and AC** are shown to the player, so the policy may read them.
- **Dice:** the policy reads none; a reaction is asked only after the hit is
  known, as the player is asked.
- **Unseen enemies:** the walk toward the nearest enemy counts enemies behind
  walls that the creature cannot see (question 2). The original did the same.
- **Enemy states** it reads (burning, oiled, regenerating, conditions) are
  shown to the player too.

## Open questions

1. Should the computer attack the weakest enemy it can reach (now), or keep
   its current target and otherwise pick at random, as the original did?
2. Should a creature that sees no enemy walk toward enemies it cannot see
   (now, and the original), or hold its ground until one comes into view?
3. Should an unarmored arcane caster stay back and guard, as the original's
   magic-users did, instead of closing in?
4. Should spell choice keep its fixed order (now), or follow the original's
   priorities with a 1d7 roll so stronger spells usually come first?
5. Should the computer cast area spells such as Fireball when no ally would
   be caught?
6. Should a fighter switch to its best ranged weapon whenever no enemy is
   next to it, as the original did, instead of only when it cannot get
   closer?
7. Should the computer use the features it never uses today: Two-Weapon
   Fighting, Action Surge, slot smites, Cunning Action, Dodge and Disengage,
   thrown weapons, Breath Weapon?
8. Should a caster already Concentrating skip spells that would end it, such
   as Hold Person while Moonbeam is up?
9. Should healing go to the most wounded ally in reach, as the original's did,
   instead of the first one listed?
10. Should every healing choice use the same line, at or below half Hit Points?
11. Should Shield be cast only when +5 AC would turn the hit into a miss?
12. Should a Champion's free move be taken toward an enemy instead of ended at
    once?
13. In Flee mode, should members who cannot flee obey Quick magic?
