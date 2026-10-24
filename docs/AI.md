# Combat AI

Scope: [#266](https://github.com/stdarg/OpenGold/issues/266). How the computer
plays monsters and the party members on Quick, what the original Gold Box
engine did, and where we depart from it. The choices are recorded in
[AI-1](SRD-DECISIONS.md#ai-1-2026-10-21-computer-play-as-the-original-game-had-it);
[#269](https://github.com/stdarg/OpenGold/issues/269) added the features the
computer had never used.

Sources: our policy is `choose_from` and `choose_in_sight` in
`src/OpenGold.Core/src/combat_demo.cpp`, driven by `CombatView` in
`src/OpenGoldBox/combat_view.cpp`; the rules module decides morale, forced
movement and the order of every menu, and gives the policy the facts below.
The original is read from coab, the Curse of the Azure Bonds reimplementation
of the same engine (`engine/ovr010.cs` `PlayerQuickFight` and the routines it
calls, `ovr014.cs` `find_target`); no code is copied, and Pool of Radiance may
differ in tuning and spells. `tests/ai_policy_tests.cpp` checks each rule.

## Who the computer plays

One policy plays every monster and every Quick member, in the original as here.
The computer takes the turn when the actor is a monster, when the party is
fleeing (FLEE-1), or when a party member is on Quick (QUICK-1).

- **Quick** hands the acting member to the computer; **Q** hands over the whole
  party; **Take control** or Space hands every member back. The campaign keeps
  who is on Quick between fights.
- **Quick magic** (M) decides whether Quick members cast spells, cantrips
  included. Off, the policy is given the same menu without spell commands;
  class features that are not spells (Lay On Hands, Rage, Turn Undead, Second
  Wind) stay. Magic items and scrolls are used either way (#263, MAGIC-6), as
  in the original. Monsters always cast. In Flee mode, members who cannot flee
  fight on under the same Quick magic.
- **Reactions** belong to whoever reacts: a monster's or a Quick member's is
  answered by the computer, a member the player controls is asked. In Flee
  mode the computer answers for every member.
- The computer waits 0.65 seconds between actions so the player can follow.

The original differed in three small ways: its Magic toggle was switched off
at the start of every fight (QUICK-1 keeps it), the same AI also played
charmed, confused and berserk characters, and allied NPCs had their own morale.
Our allied NPCs are played by the player.

**Chance.** Where the original rolled, the computer chooses "at random" from
the fight's state (its revision and the creature), so the same fight replays
the same way and a test sees the same choice.

## The turn

The policy works down one list and takes the first command that applies:

1. Answer a pending choice: initiative, an optional effect, Temporary Hit
   Points.
2. Stand up from Prone.
3. A panicked monster, or a member in Flee mode, runs for the edge.
4. Finish aiming a spell it has begun, or take a free move.
5. Free riders: a Paladin's free Divine Smite, a smite with a slot on a
   Critical Hit, Hunter's Mark, Horde Breaker, Divine Strike, Spiritual Weapon;
   then Rage, Sacred Weapon, putting out flames at half Hit Points or less,
   escaping vines.
6. A reaction, or Second Wind or Wholeness of Body at half Hit Points or less.
7. Magic items and scrolls.
8. Healing: Lay On Hands and Divine Spark, Turn Undead and Preserve Life, cure
   spells, Stabilize, shaking a sleeper awake.
9. Light and Nick extra attacks at the same target. Then buffs and setup with
   their own rules: Innate Sorcery, Produce Flame,
   Shillelagh, Hex, Spiritual Weapon, Bardic Inspiration, the Druid's Moonbeam
   and Wild Shape.
10. A spell with a slot, by priority (below).
11. Fire and acid against a regenerating enemy (trolls).
12. A Breath Weapon, Dragon's Breath's exhale or a Basilisk's Petrifying Gaze
    where it is safe; Steady Aim
    when it can attack without moving; then attacks, in a fixed order (below).
13. With the Action spent: Action Surge with an enemy in view; Flurry of
    Blows, Martial Arts, moving Hex or Heat Metal; then End turn.
14. Otherwise Cunning Action's Dash or Adrenaline Rush when its target is out
    of reach even with all its movement; move toward its target, draw a bow, or
    take off a shield to shoot; Dodge with the Action unused and an enemy
    beside it; else End turn.

The original's turn ran: morale check, a magic item, finishing a slow spell,
Turn Undead, a spell, choosing a weapon, then moving and attacking.

## 1. Movement and sight

A creature goes for the enemies **it can see**. It walks, around walls, toward
its target while that target stands in view, otherwise toward the nearest enemy
in view. One that sees no enemy **holds its ground** until one comes into view,
even when no one on the field sees anyone (the user, 2026-10-21): then a fight
the computer plays alone waits for the player to take control. Automated runs,
with no player to break a standoff, go looking: the tests and the campaign
simulator play the party as a stand-in for the player that searches for the
enemy, and the game's own checks have every creature search.

The walk counts every step as one, so it neither avoids Difficult Terrain nor
Opportunity Attacks. Exceptions:

- **An unarmored Wizard or Sorcerer, or a monster magic-user, never closes in**,
  as the original's magic-users did not: it casts or shoots from where it
  stands, and fights only an enemy that comes to it.
- An orc's Aggressive moves it toward an enemy in view.
- Commanded, frightened, turned, incapacitated or lost creatures make the
  moves the rules force on them; the policy sees only that command.
- A panicked monster runs for an edge nearer to it than to the party; a
  fleeing member runs for the nearest edge. With an enemy beside it, it first
  Disengages (Cunning Action's if it has one); both Dash when out of movement.
- A creature whose target is out of reach even with all its movement Dashes
  with Cunning Action or Adrenaline Rush, keeping its Action for an attack.
- A Champion's free move is taken toward the enemy.

**Original.** It walked straight at its target with no pathfinding, trying
other headings in a fixed order when blocked, and gave up after being blocked
three times. With no enemy in sight it chose one anywhere on the field, behind
walls too, and walked toward it. A pure magic-user without armor stood guard
instead of advancing. Attacks of opportunity were ignored.

**Departures.** Our walk finds a way around walls; a creature that sees no
enemy waits instead of walking toward one it cannot see.

## 2. Target choice

A creature keeps attacking its target, the enemy its last command aimed at,
while it can reach it; otherwise it attacks an enemy in reach at random. As in
the original, it does not pick the weakest. The rules keep each creature's
target in the fight and in saves.

**Original.** The same: it kept its target while that target was visible and in
the fight, otherwise picked a random visible enemy, and enemies within reach
came first. It never read Hit Points or AC.

## 3. Attacks

The order is: Eldritch Blast, Sorcerous Burst and the flame weapons, then
**melee**, the other attack cantrips, then the **held ranged weapon**. With
no enemy beside it, a creature whose ranged weapon rates **more than half** its
melee weapon shoots instead of closing in: the held one, or a carried bow it
draws (taking off its shield if the bow needs both hands). A **thrown weapon**
is rated and thrown the same way, after the bow; with an enemy beside it, the
creature fights in melee. The rating is the
original's: dice count times sides, plus 8 for each magic plus, times the
weapon's attacks a turn (a Monk's Martial Arts strike counts for its melee).
Weapon masteries' optional effects are always used. Sneak Attack, Savage
Attacker and a Versatile grip apply automatically (AUTO-1).

The features added by #269:

- **Light and Nick** extra attacks are always taken, at the same target, right
  after the Attack action; Bonus Action healing and Second Wind come first.
- **Action Surge** is used as soon as the Action is spent, with an enemy in
  view.
- **Smites that cost a slot** (Divine Smite, Searing Smite, Shining Smite,
  Ensnaring Strike) only on a Critical Hit, from the lowest slot, so slots stay
  for spells; one that needs Concentration waits while the caster holds one.
- **Steady Aim** when the Rogue can attack without moving.
- **Breath Weapon**, **Dragon's Breath**'s exhale and a Basilisk's
  **Petrifying Gaze** before the attacks, while uses remain, where the area
  catches an enemy and no ally; Dragon's Breath itself is a helpful spell of
  priority 3.
- **Hide**, with the action or Cunning Action, is left to the player.

**Original.** It scored each usable weapon (damage dice, magic bonus, extra
attacks, one hand) and readied the best ranged one, a thrown weapon too,
whenever no enemy stood next to it and it scored more than half the best melee
weapon, switching back when an enemy closed; a thrown weapon next to an enemy
was used in melee. Breath and gaze attacks fired by chance each round.

**Departures.** The rating counts melee attacks a turn, which the original did
only for missile launchers, so a Monk fights with its fists. A Breath Weapon
goes off where it is safe, not by chance.

## 4. Spells

Each spell the computer casts with a slot has the original's priority, from 1
(Bless, Protection from Evil and Good) to 7 (Fireball, Hold Monster); a spell
with none is never cast by the computer. Each turn it rolls 1d7 and tries the
priorities from 7 down to 8 less the roll, so the strongest spells come first
and the weakest only on a high roll; at each priority it picks one of the
spells that make sense, at random, cast from its lowest slot:

- a harmful spell needs an enemy in view not already under it, and goes on its
  target if that one is offered, else an enemy at random;
- an **area spell** (Fireball, Burning Hands, Shatter) needs an enemy it can be
  aimed at without catching anyone of the caster's side, judged on the safe
  side of the area; it is aimed at that enemy and set off only when the drawn
  area holds an enemy and none of its side;
- a helpful spell (Haste, Mirror Image, Bless) goes on the caster or an ally
  not already under it; a spell for several creatures takes in more of the same
  side before it is cast;
- **a spell that needs Concentration waits while the caster holds one.**

The priorities follow the original's table where it has the spell, and its
nearest kin where it does not (`ai_spells` in `combat_demo.cpp`). Spirit
Guardians (5) and Dispel Magic (4), which a Priest casts (Grishnak, CADORNA-1),
have their nearest kin's place. A monster's daily innate spells are cast like
slot spells, spending a daily use; a Priest's Divine Aid casts its spells as a
Bonus Action, and its Healing Word heals by the healing rule below. Healing,
cantrips and the spells with their own rules (Moonbeam, Hex, Spiritual Weapon,
Hunter's Mark) stay outside the roll. Magic Missile's darts all go to one
enemy while they can.

**Original.** The same priorities and roll; it drew three random spells it had
memorized at each priority and cast the first that made sense. For an area it
rolled each ally's saving throw; monsters cast nearly regardless.

**Departures.** One random spell among those that make sense, instead of three
draws; cantrips, which the original did not have, are attacks; every caster,
monster or Quick member, keeps its allies out of its areas, using no hidden
roll; no helpful spell is cast twice on the same creature.

## 5. Magic items

Items come before healing and attacks, in the order the menu lists them,
whatever Quick magic says (#263):

- a healing scroll or potion on an ally at or below half Hit Points;
- any other scroll on a conscious enemy; a scroll that only helps an ally
  waits for the player;
- the Potion of Speed and Giant Strength on the user;
- the Wand of Paralysis on an enemy not already Paralyzed;
- Magic Missile items on any enemy;
- an area item only where it catches an enemy and no ally within its reach.

Dust of Disappearance is left to the player. Monsters bring their potions,
wands and scrolls into a fight on Hard and above, and use them by these rules;
only a monster that casts spells reads a scroll
([DIFF-1](SRD-DECISIONS.md#diff-1-2026-10-21-difficulty-and-monsters-that-use-their-items)).

**Original.** The same 1d7 priority as spells, applied to readied items with a
spell effect (never scrolls), with the same sense checks; usable with Magic
off. coab's loop keeps the first sensible item in inventory order.

## 6. Healing

Lay On Hands and Divine Spark, then the cure spells, go to the **most wounded**
ally, by share of its Hit Points, at **half its Hit Points or lower** (0
counts); items, Second Wind and Wholeness of Body use the same line. A
Burning creature puts the fire out at the same line; above it, it fights on. Stabilize only when no enemy
stands next to the healer; monsters never stabilize.

**Original.** Cure Light Wounds went to the adjacent ally with the fewest Hit
Points, or to a dying one when the others had 8 or more; a Quick character
bandaged the first dying member anywhere on the field.

## 7. Reactions and optional effects

The computer always makes Opportunity Attacks, and uses Deflect Attacks,
Hellish Rebuke, the Goliath boons, Uncanny Dodge, Superior Defense, Retaliation
and Counterspell when offered; any other prompt it declines. **Shield** is cast
against Magic Missile, and against a hit only when +5 AC would turn it into a
miss: the prompt shows the attack's total and the AC it hit, to the player as
well. Pending choices take the first option, which the rules order as the
default: keep initiative, use Tactical Mind, use every optional effect.
Temporary Hit Points: keep the current amount unless the new one is larger.

**Original.** Reactions as such did not exist.

## 8. Fleeing and morale

Monsters check morale at the start of each turn (MORALE-1), from their own
morale and their side's remaining Hit Points; broken, a monster flees in panic
if no opponent is faster, surrenders if one is and it has an Intelligence above
5, and otherwise fights on. It may rally. The party flees only when the player
presses Flee (FLEE-1).

**Original.** The same tests (this is where MORALE-1 came from), plus Bless
and Curse changing morale by 5. Party members never checked morale.

## 9. What the computer may know

The computer must not use what a player could not know: hidden rolls, a
hidden enemy's position, or anything the game does not show.

- **Hidden enemies** (HIDE-1) are removed from what the policy sees; with none
  in sight it Searches.
- **Sight:** it goes only for enemies it can see.
- **Hit Points and AC** are shown to the player, so the policy may read them;
  it no longer uses them to choose a target.
- **Dice:** the policy reads none, and rolls no hidden save to judge an area; a
  reaction is asked only after the hit is known, with the total shown.
- **Enemy states** it reads (burning, oiled, regenerating, conditions, status
  effects) are shown to the player too.
