# Combat AI

Scope: [#266](https://github.com/stdarg/OpenGold/issues/266). How the computer
plays monsters and the party members on Quick, what the original Gold Box
engine did, and where we depart from it. The choices are recorded in
[AI-1](SRD-DECISIONS.md#ai-1-2026-10-21-computer-play-as-the-original-game-had-it);
[#269](https://github.com/stdarg/OpenGold/issues/269) adds the features the
computer does not use yet.

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
5. Free riders: a Paladin's free Divine Smite, Hunter's Mark, Horde Breaker,
   Divine Strike, Spiritual Weapon; then Rage, Sacred Weapon, escaping vines.
6. A reaction, or Second Wind at half Hit Points or less.
7. Magic items and scrolls.
8. Healing: Lay On Hands and Divine Spark, Turn Undead and Preserve Life, cure
   spells, Stabilize, shaking a sleeper awake.
9. Buffs and setup with their own rules: Innate Sorcery, Produce Flame,
   Shillelagh, Hex, Spiritual Weapon, Bardic Inspiration, the Druid's Moonbeam
   and Wild Shape.
10. A spell with a slot, by priority (below).
11. Fire and acid against a regenerating enemy (trolls).
12. Attacks, in a fixed order (below).
13. With the Action spent: Flurry of Blows, Martial Arts, moving Hex or Heat
    Metal, then End turn.
14. Otherwise move toward its target, draw a bow, or take off a shield to
    shoot; else End turn.

The original's turn ran: morale check, a magic item, finishing a slow spell,
Turn Undead, a spell, choosing a weapon, then moving and attacking.

## 1. Movement and sight

A creature goes for the enemies **it can see**. It walks, around walls, toward
its target while that target stands in view, otherwise toward the nearest enemy
in view. One that sees no enemy **holds its ground** until one comes into view.
When no creature on either side can see an enemy, holding would stall the fight
for good, so then the computer advances on them all, as the original always
did.

The walk counts every step as one, so it neither avoids Difficult Terrain nor
Opportunity Attacks, and it never Disengages (#269). Exceptions:

- **An unarmored Wizard or Sorcerer, or a monster magic-user, never closes in**,
  as the original's magic-users did not: it casts or shoots from where it
  stands, and fights only an enemy that comes to it.
- An orc's Aggressive moves it toward an enemy in view.
- When no one on the field sees anyone, a creature that knows a ranged cantrip
  steps to the nearest square with a line of sight to an enemy.
- Commanded, frightened, turned, incapacitated or lost creatures make the
  moves the rules force on them; the policy sees only that command.
- A panicked monster runs for an edge nearer to it than to the party; a
  fleeing member runs for the nearest edge. Both Dash when out of movement.
- A Champion's free move is taken toward the enemy.

**Original.** It walked straight at its target with no pathfinding, trying
other headings in a fixed order when blocked, and gave up after being blocked
three times. With no enemy in sight it chose one anywhere on the field, behind
walls too, and walked toward it. A pure magic-user without armor stood guard
instead of advancing. Attacks of opportunity were ignored.

**Departures.** Our walk finds a way around walls; a creature that sees no
enemy waits instead of walking toward one it cannot see, unless no one on the
field can see anyone.

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
draws (taking off its shield if the bow needs both hands). The rating is the
original's: dice count times sides, plus 8 for each magic plus, times the
weapon's attacks a turn (a Monk's Martial Arts strike counts for its melee).
Weapon masteries' optional effects are always used. Sneak Attack, Savage
Attacker and a Versatile grip apply automatically (AUTO-1).

Not used yet (#269): thrown weapons, Two-Weapon Fighting, Action Surge, smites
that cost a slot, Cunning Action, Steady Aim, Dodge, Disengage, Hide, Breath
Weapon, Dragon's Breath, putting out flames.

**Original.** It scored each usable weapon (damage dice, magic bonus, extra
attacks, one hand) and readied the best ranged one whenever no enemy stood
next to it and it scored more than half the best melee weapon, switching back
when an enemy closed. Breath and gaze attacks fired by chance each round.

**Departure.** The rating counts melee attacks a turn, which the original did
only for missile launchers, so a Monk fights with its fists.

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
nearest kin where it does not (`ai_spells` in `combat_demo.cpp`). Healing,
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

Dust of Disappearance is left to the player. Monsters carry no items into a
fight yet (#268).

**Original.** The same 1d7 priority as spells, applied to readied items with a
spell effect (never scrolls), with the same sense checks; usable with Magic
off. coab's loop keeps the first sensible item in inventory order.

## 6. Healing

Lay On Hands and Divine Spark, then the cure spells, go to the **most wounded**
ally, by share of its Hit Points, at **half its Hit Points or lower** (0
counts); items and Second Wind use the same line. Stabilize only when no enemy
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
- **Sight:** it goes only for enemies it can see, save when no one on the field
  sees anyone.
- **Hit Points and AC** are shown to the player, so the policy may read them;
  it no longer uses them to choose a target.
- **Dice:** the policy reads none, and rolls no hidden save to judge an area; a
  reaction is asked only after the hit is known, with the total shown.
- **Enemy states** it reads (burning, oiled, regenerating, conditions, status
  effects) are shown to the player too.
