#ifndef OPENGOLDBOX_MONSTER_ABILITY_OPTIONS_H
#define OPENGOLDBOX_MONSTER_ABILITY_OPTIONS_H
#include "localization.h"
#include <array>

namespace i18n
{
// A row of the Options screen's monster abilities: the rules' ability name
// (settings.cfg's [monsters] key, MONSTER-2), its label and the monsters
// that have it.
struct MonsterAbilityOption
{
    const char *ability, *label, *monsters;
};

inline constexpr std::array monster_ability_options{
    MonsterAbilityOption{"legendary", N_("Legendary Actions"), N_("Tyranthraxus, the vampire")},
    MonsterAbilityOption{"legendary_resistance", N_("Legendary Resistance"),
                         N_("Tyranthraxus, the vampire")},
    MonsterAbilityOption{"breath", N_("Breath weapons"),
                         N_("Tyranthraxus (Lightning Breath), ankhegs (Acid Spray)")},
    MonsterAbilityOption{"repulsion_breath", N_("Repulsion Breath"), N_("Tyranthraxus")},
    MonsterAbilityOption{"petrifying_gaze", N_("Petrifying Gaze"), N_("basilisks, the medusa")},
    MonsterAbilityOption{"parry", N_("Parry"),
                         N_("knights, bandit captains, veterans, buccaneer mates")},
    MonsterAbilityOption{"riposte", N_("Riposte"), N_("the buccaneer captain")},
    MonsterAbilityOption{"captains_charm", N_("Captain's Charm"), N_("the buccaneer captain")},
    MonsterAbilityOption{"hit_advantage", N_("Rapier follow-up (Advantage)"),
                         N_("the buccaneer captain")},
    MonsterAbilityOption{"enthralling_panache", N_("Enthralling Panache"), N_("buccaneers")},
    MonsterAbilityOption{"bite", N_("Vampire's Bite"), N_("the vampire")},
    MonsterAbilityOption{"innate", N_("Innate spells"),
                         N_("priests, the drider, Yarash, the vampire's Charm")},
    MonsterAbilityOption{"protective_magic", N_("Protective Magic"), N_("Yarash")},
    MonsterAbilityOption{"magic_resistance", N_("Magic Resistance"), N_("efreeti")},
    MonsterAbilityOption{"dreadful_glare", N_("Dreadful Glare"), N_("mummies")},
    MonsterAbilityOption{"rotting_curse", N_("Rotting Fist curse"), N_("mummies")},
    MonsterAbilityOption{"life_drain", N_("Life Drain"), N_("wights")},
    MonsterAbilityOption{"life_drain_attack", N_("Draining hits"), N_("specters, wraiths")},
    MonsterAbilityOption{"create_specter", N_("Create Specter"), N_("wraiths")},
    MonsterAbilityOption{"incorporeal", N_("Incorporeal Movement"), N_("specters, wraiths")},
    MonsterAbilityOption{"regeneration", N_("Regeneration"), N_("trolls")},
    MonsterAbilityOption{"undead_fortitude", N_("Undead Fortitude"), N_("zombies, juju zombies")},
    MonsterAbilityOption{"pack_tactics", N_("Pack Tactics"),
                         N_("kobolds, hobgoblins, wolves, Skullcrusher")},
    MonsterAbilityOption{"sunlight_sensitivity", N_("Sunlight Sensitivity"),
                         N_("kobolds, wights, specters, wraiths, the drider")},
    MonsterAbilityOption{"poisoning", N_("Poisoning hits"), N_("hill giants, wyverns")},
    MonsterAbilityOption{"hampering", N_("Hampering hits"), N_("ettins, fire giants")},
    MonsterAbilityOption{"pushing", N_("Pushing hits"), N_("fire giants")},
    MonsterAbilityOption{"prone_on_hit", N_("Knock-down bites"), N_("wolves")},
    MonsterAbilityOption{"prone_large", N_("Knock-down blows on Large foes"),
                         N_("tigers, hill giants, ettins")},
    MonsterAbilityOption{"last_attack", N_("A different last attack"),
                         N_("scorpions, wyverns, ettins, the medusa")},
    MonsterAbilityOption{"charming", N_("Charming arrows"), N_("quicklings")},
    MonsterAbilityOption{"flyby", N_("Flyby"), N_("hippogriffs")},
    MonsterAbilityOption{"charge", N_("Charge"), N_("wild boars")},
    MonsterAbilityOption{"bonus_charge", N_("Charge (Bonus Action)"), N_("trolls")},
    MonsterAbilityOption{"rampage", N_("Rampage"), N_("gnolls")},
    MonsterAbilityOption{"trampling_charge", N_("Trampling Charge"), N_("centaurs")},
    MonsterAbilityOption{"gore", N_("Charging Gore"), N_("minotaurs, giant skeletons")},
    MonsterAbilityOption{"paralyzing_claw", N_("Paralyzing Claw"), N_("ghouls")},
    MonsterAbilityOption{"paralyzing_venom", N_("Paralyzing venom"), N_("phase spiders")},
    MonsterAbilityOption{"ethereal_jaunt", N_("Ethereal Jaunt"), N_("phase spiders")},
    MonsterAbilityOption{"bloodied_fury", N_("Bloodied Fury"), N_("wild boars")},
    MonsterAbilityOption{"nimble_escape", N_("Nimble Escape"), N_("tigers")},
    MonsterAbilityOption{"cunning_action", N_("Cunning Action"), N_("the master thief")},
    MonsterAbilityOption{"attach", N_("Draining attachment"), N_("stirges")},
    MonsterAbilityOption{"web_walker", N_("Web Walker"), N_("the drider, phase spiders")},
    MonsterAbilityOption{"advantage_damage", N_("Extra damage with Advantage"), N_("goblins")}};

// The Options screen's choices for an ability: the lowest difficulty it is on
// at, or never. Their settings.cfg values are the lower-case words.
inline constexpr std::array<const char *, 6> ability_from_names{
    N_("Easy"), N_("Normal"), N_("Hard"), N_("Nightmare"), N_("Infernal"), N_("Never")};
} // namespace i18n
#endif
