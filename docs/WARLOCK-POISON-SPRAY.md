# Level-one Warlock Poison Spray

Rules 0.6.39 delivers [#227](https://github.com/stdarg/OpenGold/issues/227), a child
of [Poison Spray #202](https://github.com/stdarg/OpenGold/issues/202) and
[Pact Magic #160](https://github.com/stdarg/OpenGold/issues/160).
Authority: [SRD 5.2.1 pp.75,153](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf).

The approved Q26 Warlock Spell Choices pattern now lists Eldritch Blast and Poison
Spray. Players can fill both starting cantrip choices; new presets pre-generate
both. Missing choices remain pending under the existing selection policy.
No independent control layout or behavior was introduced.

Poison Spray records a level-one `class:warlock:pact_magic` cantrip grant. The
existing spell rules use the Warlock's Charisma plus proficiency for a ranged
spell attack against a living creature within 30 feet, dealing 1d12 Poison on a
hit. Criticals, typed defenses, line of effect, adjacent-hostile Disadvantage,
Somatic hands and armor restrictions use the existing implementation. Only the
Magic action is spent. The approved Spell dropdown/Cast can choose either learned
cantrip; ally targeting preserves party selection and keyboard access.

Profile recipes admit the Warlock Poison Spray spell/source combination. No player combat saving was
introduced; normal rest/save behavior retains both newly chosen spells.

Evidence: [native Warlock grant/attack/persistence checks](../tests/warlock_poison_checks.h)
run in the existing [cantrip harness](../tests/eldritch_blast_tests.cpp), alongside
[creator](../tests/warlock_cantrip_view_tests.gd) and
[combat](../tests/eldritch_view_tests.gd) checks. Native cases independently verify
Charisma 18 rather than Intelligence 15, d12 12 versus the same seed's d10 8,
critical/miss/typed defenses, rejection without mutation, explicit choices,
PC/NPC campaign handoff and Short/Long Rest save-reload.

This is the currently playable **Warlock level-one** source, not proof of Warlock
levels two through four or full Pact Magic. Those remain #160. Parent #202 retains
Druid #152, Sorcerer #132, High Elf #67, Abyssal Tiefling #73, Magic Initiate #75,
Tome #161, Magician #152 and Druidic Warrior #147 integrations, plus remaining
speech/component enforcement #39. The Wizard levels 1–4 path remains documented
in [Poison Spray](POISON-SPRAY.md). Higher scaling stays #176–178.

Verification: all 43 native/tool checks and 19 headless Godot checks plus ten
native prerequisites pass. The asset-backed Warlock creator and bilingual combat
checks pass, with English/Spanish renders at 1120×800 and 1920×1080. Both native
extensions build; 825 localization messages validate. Logs:
`/tmp/warlock-poison-regression.log`, `/tmp/warlock-poison-creator.log`,
`/tmp/warlock-poison-combat-render.log`; renders:
`/tmp/opengold-warlock-poison-renders`.
