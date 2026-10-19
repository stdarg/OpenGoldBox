# Rogue

The Rogue (Thief) as the game plays it from level 5 to 15, from SRD 5.2.1
pp. 61–64. Levels 1–4 are in [Sneak Attack](SNEAK-ATTACK.md),
[Cunning Action](CUNNING-ACTION.md), [Rogue attacks](ROGUE-ATTACKS.md) and
[Hide](HIDE.md); the plan is [levels 5–10](LEVELS-5-10.md), and the facts for
11–15 are in the [inventory](LEVELS-11-15-INVENTORY.md).

A Rogue reaches level 15, the game's cap and the Training Hall's limit for
every class ([LEVELS-2](SRD-DECISIONS.md#levels-2-2026-10-15-level-cap-15)).

| Level | Gains |
| ---: | --- |
| 5 | Cunning Strike, Uncanny Dodge; Sneak Attack 3d6; Proficiency Bonus +3 |
| 6 | Expertise in two more skills |
| 7 | Evasion, Reliable Talent; Sneak Attack 4d6 |
| 8 | A feat or ability points |
| 9 | Thief: Supreme Sneak; Sneak Attack 5d6; Proficiency Bonus +4 |
| 10 | A feat or ability points |
| 11 | Improved Cunning Strike; Sneak Attack 6d6 |
| 12 | A feat or ability points |
| 13 | Thief: Use Magic Device (keeps a wand charge on a 6); Sneak Attack 7d6; Proficiency Bonus +5 |
| 14 | Devious Strikes |
| 15 | Slippery Mind; Sneak Attack 8d6 |

- **Cunning Strike:** when an attack would deal Sneak Attack damage, the A
  cycle offers it with **Trip** (a Large or smaller target makes a Dexterity
  save against 8 + Dexterity modifier + Proficiency Bonus or falls Prone) and
  **Withdraw** (after the attack, half the Speed of free movement that provokes
  nothing), each for one Sneak Attack die. The effect needs the attack to deal
  Sneak Attack damage. **Poison** waits for a Poisoner's Kit to buy
  ([#233](https://github.com/stdarg/OpenGold/issues/233)).
- **Uncanny Dodge:** when an attack hits the Rogue, React halves its damage
  with the Reaction; a Blinded Rogue cannot use it.
- **Expertise (6):** a Training page choice of two proficient skills.
- **Evasion:** against a Dexterity save for half damage, a success takes none
  and a failure half, unless Incapacitated.
- **Reliable Talent:** a d20 below 10 on a check with a proficient skill counts
  as 10: Stealth to hide, Athletics to break free, Medicine to stabilize, and
  the campaign's skill checks. The log says when it applies.
- **Supreme Sneak:** a hidden Thief may choose **Stealth Attack**, a Cunning
  Strike: if it deals Sneak Attack damage, the attack does not end hiding. The
  game has no cover, so it works whenever the Rogue is hidden
  ([HIDE-1](SRD-DECISIONS.md#hide-1-2026-10-14-finding-hidden-creatures)).
- **Cunning Strike costs:** Trip, Withdraw and Stealth Attack cost one Sneak
  Attack die, Daze two, Obscure three and Knock Out six. An effect is offered
  only when the dice can pay for it.
- **Improved Cunning Strike:** the A cycle also offers each pair of effects
  the dice can pay for, such as "Melee attack (Trip, Withdraw)"
  ([LEVELS-2](SRD-DECISIONS.md#levels-2-2026-10-15-level-cap-15)). Withdraw's
  move comes after the other effect.
- **Use Magic Device:** a 6 on 1d6 keeps a wand's spent charge (MAGIC-3). No
  item needs attunement, and Spell Scrolls wait for
  [#265](https://github.com/stdarg/OpenGold/issues/265).
- **Fast Hands:** uses a wand, the Necklace of Missiles or Dust of
  Disappearance as a Bonus Action
  ([MAGIC-3](SRD-DECISIONS.md#magic-3-2026-10-19-wands-potions-and-other-charged-items)).
- **Devious Strikes:** **Daze**: a Constitution save, or on its next turn the
  creature does only one of moving, an action or a Bonus Action; the first it
  does takes away the others. **Knock Out**: a Constitution save, or
  Unconscious and Prone for up to a minute, ended by damage, by an ally's
  Action to shake it awake, or by a Constitution save at the end of each of its
  turns. **Obscure**: a Dexterity save, or Blinded until the end of its next
  turn.
- **Slippery Mind:** proficiency in Wisdom and Charisma saves.

Verification: `opengold_rogue_levels_tests` (the campaign to 15, grants,
feats, Expertise, save proficiencies, save and reload,
Trip and Withdraw and their Sneak Attack die, no Cunning Strike without Sneak
Attack, Uncanny Dodge, Evasion against a Dragonborn's breath, Reliable Talent
while hiding, Stealth Attack from hiding, paired effects, Daze, Knock Out and
Obscure with checkpoints).
