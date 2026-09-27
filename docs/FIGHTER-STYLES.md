# Fighter starting Fighting Style

[SRD 5.2.1 pp. 47, 87–88](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf)
gives Fighters one Fighting Style feat at level one. Archery adds +2 to attack
rolls with Ranged weapons; Defense adds +1 AC while wearing armor. Neither feat
is repeatable. Defense is the recommended starting choice in the SRD.

The approved Training dropdown appears above languages and requires one of the
currently implemented styles before Next. Keyboard selection retains focus;
Back and background changes preserve the choice. Changing away from Fighter
removes it. Presets receive a pre-generated choice. The shared C++ training
service owns the entitlement and validation; Godot uses its option metadata.

The selected feat records `class:fighter:fighting_style` at level one. It is
separate from the level-four ability improvement/feat entitlement. A Fighter
can acquire both styles from distinct sources, but cannot take the same feat
twice. Choosing a starting style does not prevent a later Constitution increase.
The existing armor and ranged-attack rules apply the effects.

Rules 0.6.29 / PC18 validate the new source. Campaign 11, FX2 and combat 13–15
are unchanged. Prior profiles retain their original validation. Old campaign
characters retain their choices and have a pending starting style; migration
never assigns one or converts a level-four feat into a starting feat. Completion
through the existing core training API preserves advancement, wounds and spent
resources. The player-facing Review Training flow is delivered; see [training review](TRAINING.md).

Tests use actual 0.6.28 campaign/combat files from `dda8d0c`, with four Fighters:
level one, level-four Defense, level-four Archery and level-four Constitution
increase. Migration preserves file bodies except module identity and checksum.
Native checks also cover prerequisites across all twelve classes, duplicate and
invalid choices, atomic replacement, independent AC/attack expectations,
Constitution history, current save round trips and rejection of future grants
under old version identities. Godot checks cover keyboard selection, Next,
Back, class changes and English/Spanish layouts at both supported test sizes.

Rules 0.6.53 adds Great Weapon Fighting to these same controls and supports
replacing the class style at gained Fighter levels 2–4. Keep current is the
default. A missing historical starting selection remains pending in Review
Training; replacement requires an existing class-granted style. Completing other
training later replays advancement choices and preserves the replacement.

The [style-route packet](FIGHTING-STYLE-ROUTES.md) also covers actual Paladin and
Ranger level-two entitlements and level-four feat selection. #85 is now completed by Two-Weapon Fighting and the
[Weapon Mastery integration](WEAPON-MASTERY.md#optional-hit-mastery-integration).
#140/#147 retain their remaining class and cantrip alternatives. Full class support
is not claimed.

Verification: all 41 native/tool checks pass across the regression run and the
focused rerun of six updated training-completion fixtures. All 16 Godot runtime
checks and seven native prerequisites pass, with the two dependent resource UI
checks rerun after their fixtures rebuilt. Main/demo builds and full party flows
pass through creation, shops, combat, level two, recovery and later combat.
Demo creation also passes. Keyboard training checks pass headlessly and with
rendering; English/Spanish layouts were inspected at 1120×800 and 1920×1080.
All 755 localization messages validate. The full-party test's temporary Wizard
fixture clears the Fighter-only choice, preserving the real class restriction.
