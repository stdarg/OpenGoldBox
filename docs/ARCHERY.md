# Archery feat

[SRD 5.2.1 p. 87](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf#page=87)
gives Archery a +2 attack-roll bonus with Ranged weapons. It requires the Fighting
Style feature and is not repeatable. The weapon category matters: darts qualify;
throwing a dagger, handaxe, javelin, light hammer, spear or trident does not.
The bonus changes neither damage nor natural-roll critical/miss behavior.

This increment under [#78](https://github.com/stdarg/OpenGold/issues/78) adds
Archery to the existing level-four feat selector. Fighters have the prerequisite;
Clerics and Wizards see it unavailable. Current character creation gives all
twelve classes their existing entitlement data; it does not invent Fighting Style
for other classes. Selection replaces the level-four ability improvement, records
`feat:archery` with `class:fighter:ability_score_improvement` at level four and
retains the existing preview/confirmation behavior. No new layout is introduced.
The selector and character-sheet feat name are translated into Spanish.

The rules derive the bonus from the validated feat grant and equipped weapon
category. The combat log reports the resulting attack bonus. Unarmed fallback,
thrown melee weapons and spell attacks do not gain it. Range and other attack
Disadvantage still apply. Action Surge permits another eligible attack normally;
Savage Attacker's saved damage choice retains the Archery attack bonus.

Rules 0.6.28 / PC17 adds the Archery feature bit. Older profiles reject that bit
and Archery grants. Current profiles validate their mask against grant provenance,
prerequisite and nonrepeatability. Older campaign identities cannot smuggle in
Archery. Campaign format 11, FX2 and combat formats 13–15 remain unchanged.
Migration preserves prior choices and never substitutes Archery for an old feat.
Actual 0.6.27 campaign/combat fixtures verify identity-only migration; current
Archery campaigns and combat continuations reload canonically.

[Native checks](../tests/archery_tests.cpp) cover current entitlement selection,
rejected-command atomicity, independent attack bonuses and natural-roll outcomes
across all ten Ranged weapons, six thrown melee exclusions, long range, an exact
hit threshold, Savage Attacker, Action Surge and prior/current saves.
The existing Godot advancement check additionally selects and confirms Archery
for a Soldier Fighter and reloads the acquired grant alongside Savage Attacker.

Rules 0.6.53 completes the other supported source routes: starting Fighter
selection, Fighter replacement at gained levels 2–4, and Paladin/Ranger level-two
acquisition. All three classes can choose Archery as a distinct level-four feat
when they do not already possess it. Provenance, nonrepeatability and the actual
Ranged weapon effect share the same rules implementation.

See the [style-route packet](FIGHTING-STYLE-ROUTES.md) for current acceptance,
save formats and evidence. The class trackers retain other styles, mastery and
cantrip alternatives; higher-level/multiclass requirements remain separate.

Verification: 41 native/tool regression checks pass, followed by the final
focused Archery checks for stale-command atomicity, forged feature masks and
old-version checkpoint rejection. All 16 Godot runtime checks and seven native
fixture prerequisites pass. The actual advancement check confirms and reloads
Archery; English/Spanish dialogs were rendered and inspected at 1120×800 and
1920×1080. Main/demo extensions build and 750 localization messages validate.
The advancement test's existing bonus-source assertions now use translated
message templates so that the same checks can run in Spanish.
