# Rogue

The Rogue (Thief) as the game plays it from level 5 to 10, from SRD 5.2.1
pp. 61–64. Levels 1–4 are in [Sneak Attack](SNEAK-ATTACK.md),
[Cunning Action](CUNNING-ACTION.md), [Rogue attacks](ROGUE-ATTACKS.md) and
[Hide](HIDE.md); the plan is [levels 5–10](LEVELS-5-10.md).

In the campaign a Rogue stops at level 9, the Thieves' hall's limit
([CAP-1](SRD-DECISIONS.md#cap-1-2026-10-14-the-original-training-halls-limits)).
The rules go to level 10.

| Level | Gains |
| ---: | --- |
| 5 | Cunning Strike, Uncanny Dodge; Sneak Attack 3d6; Proficiency Bonus +3 |
| 6 | Expertise in two more skills |
| 7 | Evasion, Reliable Talent; Sneak Attack 4d6 |
| 8 | A feat or ability points |
| 9 | Thief: Supreme Sneak; Sneak Attack 5d6; Proficiency Bonus +4 |
| 10 | A feat or ability points |

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

Verification: `opengold_rogue_levels_tests` (the campaign's stop at 9, feats,
Expertise, save and reload, Trip and Withdraw and their Sneak Attack die, no
Cunning Strike without Sneak Attack, Uncanny Dodge, Evasion against a Dragonborn's
breath, Reliable Talent while hiding, Stealth Attack from hiding).
