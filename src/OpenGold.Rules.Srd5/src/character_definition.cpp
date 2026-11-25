#include "combat_model.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <limits>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>

namespace opengold::srd5::detail
{
using namespace rules;

std::vector<FeatureGrant> without_manuals(std::vector<FeatureGrant> grants)
{
    std::erase_if(grants,
                  [](const auto &g)
                  {
                      return g.id == bodily_health;
                  });
    return grants;
}

int maximum_hit_points(const detail::ClassHistory &history, bool dwarf,
                       std::span<const int> modifiers)
{
    if (modifiers.size() != history.level() ||
        modifiers.size() > detail::level_cap(history.starting()) ||
        std::any_of(modifiers.begin(), modifiers.end(),
                    // A Manual of Bodily Health can raise Constitution to 30.
                    [](int n)
                    {
                        return n < -4 || n > 10;
                    }))
    {
        throw std::runtime_error("Invalid HP advancement history");
    }
    const int sorcerer = history.levels()[CharacterClass::sorcerer];
    int hp = detail::class_hit_die(history.starting()) + modifiers.front() +
             (dwarf ? static_cast<int>(modifiers.size()) : 0) + (sorcerer >= 3 ? sorcerer : 0);
    for (std::size_t i = 1; i < modifiers.size(); ++i)
    {
        const auto level = static_cast<unsigned>(i + 1);
        const auto taken = history.taken_at(level);
        const int increase = modifiers[i] - modifiers[i - 1];
        // Constitution rises only with a feat level's ability points.
        if (increase < 0 || increase > 1 ||
            (increase && !detail::gains_feat(taken, history.class_level_at(level))))
        {
            throw std::runtime_error("Invalid Constitution advancement history");
        }
        // SRD p. 23: gain HP first, then apply a new modifier per attained level.
        hp += std::max(1, detail::class_hit_die(taken) / 2 + 1 + modifiers[i - 1]) +
              increase * static_cast<int>(level);
    }
    return hp;
}

Definition character_definition(std::string_view bytes,
                                std::optional<std::span<const std::string>> equipment_override)
{
    if (bytes.size() > 8192)
    {
        throw std::runtime_error("Character profile exceeds limit");
    }
    std::istringstream in{std::string(bytes)};
    std::string magic, class_label, race;
    AbilityArray<int> scores{};
    unsigned count{}, level{}, features{}, listed{};
    in >> magic >> level >> features >> listed;
    // Untrusted input: bound the list and require every id to be a supported
    // spell before anything else looks at it.
    if (!in || magic != profile_magic || listed > detail::spell_table.size())
    {
        throw std::runtime_error("Invalid character profile");
    }
    std::vector<std::string> stored_spells;
    for (unsigned n = 0; n < listed; ++n)
    {
        std::string id;
        in >> id;
        if (!in || !detail::find_spell(id) || detail::knows_spell(stored_spells, id))
        {
            throw std::runtime_error("Invalid character profile");
        }
        stored_spells.push_back(std::move(id));
    }
    in >> std::quoted(class_label) >> std::quoted(race);
    for (auto &score : scores)
    {
        in >> score;
    }
    if (!in || level < 1 || level > 20 || features > 63 ||
        std::any_of(scores.begin(), scores.end(),
                    [](int n)
                    {
                        return n < 3 || n > 20;
                    }))
    {
        throw std::runtime_error("Invalid character profile");
    }
    const auto character_class = detail::class_from_label(class_label);
    if (!character_class)
    {
        throw std::runtime_error("Unknown class");
    }
    const CharacterClass klass = *character_class;
    if (level > detail::level_cap(klass))
    {
        throw std::runtime_error("Invalid character profile");
    }
    const auto &races = detail::creation_choices(CreationField::race);
    if (std::none_of(races.begin(), races.end(),
                     [&](const auto &r)
                     {
                         return r.label == race;
                     }))
    {
        throw std::runtime_error("Unknown species");
    }
    const int str = ability_modifier(scores[Ability::strength]),
              dex = ability_modifier(scores[Ability::dexterity]),
              con = ability_modifier(scores[Ability::constitution]);
    // The Proficiency Bonus: +2 through level 4, +3 from 5, +4 from 9.
    const int proficiency = 2 + (static_cast<int>(level) - 1) / 4;
    std::vector<int> hp_modifiers(level, con);
    for (auto &modifier : hp_modifiers)
    {
        in >> modifier;
    }
    // The class of each level (MULTI-1): the first is the starting class, and
    // no class goes past its cap.
    detail::ClassLevels levels;
    std::vector<std::string> level_ids;
    for (unsigned n = 0; n < level; ++n)
    {
        auto &id = level_ids.emplace_back();
        in >> id;
        if (!in)
        {
            throw std::runtime_error("Invalid character profile");
        }
        const auto taken = detail::class_from_id(id);
        if (n == 0 && taken != klass)
        {
            throw std::runtime_error("Invalid character profile");
        }
        levels.add(taken);
        if (static_cast<unsigned>(levels[taken]) > detail::level_cap(taken))
        {
            throw std::runtime_error("Invalid character profile");
        }
    }
    // The level in a class (MULTI-1): a single-class character has all of
    // `level` in its own.
    const auto at = [&levels](CharacterClass held)
    {
        return static_cast<unsigned>(levels[held]);
    };
    in >> count;
    // A weapon, armor and a shield, and two rings, bracers, a cloak, gauntlets
    // and a necklace (MAGIC-4).
    if (!in || count > 9 || hp_modifiers.back() != con)
    {
        throw std::runtime_error("Invalid character HP history or equipment count");
    }
    const int die = klass == CharacterClass::barbarian ? 12
                    : (klass == CharacterClass::fighter || klass == CharacterClass::paladin ||
                       klass == CharacterClass::ranger)
                        ? 10
                    : (klass == CharacterClass::wizard || klass == CharacterClass::sorcerer) ? 6
                                                                                             : 8;
    Definition d;
    d.hit_die = die;
    d.constitution = con;
    d.dwarf = race == "Dwarf";
    d.gnomish_cunning = race == "Gnome";
    d.relentless = race == "Orc";
    d.resourceful = race == "Human";
    d.lucky = d.brave = race == "Halfling";
    d.fey_ancestry = race == "Elf";
    d.elf = race == "Elf";
    d.nimble = race == "Halfling";
    d.naturally_stealthy = race == "Halfling";
    // Gnomes and Halflings are Small.
    if (race == "Gnome" || race == "Halfling")
    {
        d.size = 1;
    }
    // Elves do not sleep, so Sleep cannot touch them.
    d.sleepless = race == "Elf";
    d.rages = at(CharacterClass::barbarian) > 0 ? (at(CharacterClass::barbarian) >= 12  ? 5
                                                   : at(CharacterClass::barbarian) >= 6 ? 4
                                                   : at(CharacterClass::barbarian) >= 3 ? 3
                                                                                        : 2)
                                                : 0;
    d.wild_shapes = at(CharacterClass::druid) >= 2 ? (at(CharacterClass::druid) >= 6 ? 3 : 2) : 0;
    d.wild_resurgence = at(CharacterClass::druid) >= 5;
    d.natural_recovery = at(CharacterClass::druid) >= 6 ? 1 : 0;
    d.natures_ward = at(CharacterClass::druid) >= 10;
    d.natures_sanctuary = at(CharacterClass::druid) >= 14;
    d.lands_aid = at(CharacterClass::druid) >= 3;
    d.rage_damage =
        at(CharacterClass::barbarian) > 0 ? (at(CharacterClass::barbarian) >= 9 ? 3 : 2) : 0;
    d.strength = str;
    d.danger_sense = d.reckless = at(CharacterClass::barbarian) >= 2;
    d.frenzy = at(CharacterClass::barbarian) >= 3;
    d.focus = at(CharacterClass::monk) >= 2 ? static_cast<int>(at(CharacterClass::monk)) : 0;
    d.metabolism = at(CharacterClass::monk) >= 2 ? 1 : 0;
    d.innate_sorcery = at(CharacterClass::sorcerer) > 0 ? 2 : 0;
    d.sorcerous_restoration = at(CharacterClass::sorcerer) >= 5 ? 1 : 0;
    d.sorcery_incarnate = at(CharacterClass::sorcerer) >= 7;
    d.dragon_wings = at(CharacterClass::sorcerer) >= 14;
    d.sorcery_points =
        at(CharacterClass::sorcerer) >= 2 ? static_cast<int>(at(CharacterClass::sorcerer)) : 0;
    d.pact_magic = at(CharacterClass::warlock) > 0;
    d.reads_arcane_scrolls = reads_arcane_scrolls(klass);
    d.reads_divine_scrolls = reads_divine_scrolls(klass);
    d.magical_cunning = at(CharacterClass::warlock) >= 2 ? 1 : 0;
    d.dark_luck = at(CharacterClass::warlock) >= 6
                      ? std::max(1, ability_modifier(scores[Ability::charisma]))
                      : 0;
    d.hurl_through_hell = at(CharacterClass::warlock) >= 14;
    d.bardic_inspiration =
        at(CharacterClass::bard) > 0 ? std::max(1, ability_modifier(scores[Ability::charisma])) : 0;
    d.cutting_words = at(CharacterClass::bard) >= 3;
    d.bardic_die = at(CharacterClass::bard) == 0    ? 6
                   : at(CharacterClass::bard) >= 15 ? 12
                   : at(CharacterClass::bard) >= 10 ? 10
                   : at(CharacterClass::bard) >= 5  ? 8
                                                    : 6;
    d.font_of_inspiration = at(CharacterClass::bard) >= 5;
    d.countercharm = at(CharacterClass::bard) >= 7;
    d.peerless_skill = at(CharacterClass::bard) >= 14;
    d.dark_ones_blessing = at(CharacterClass::warlock) >= 3
                               ? std::max(1, ability_modifier(scores[Ability::charisma]) +
                                                 static_cast<int>(at(CharacterClass::warlock)))
                               : 0;
    d.deflect = d.open_hand = at(CharacterClass::monk) >= 3;
    d.focus_dc = 8 + proficiency + ability_modifier(scores[Ability::wisdom]);
    d.dexterity = dex;
    d.rushes = race == "Orc" ? proficiency : 0;
    for (const auto ability : all_abilities)
    {
        d.saves[ability_index(ability)] =
            ability_modifier(scores[ability]) +
            (detail::save_proficient(klass, levels, ability) ? proficiency : 0);
    }
    d.alert = (features & 32) != 0;
    d.ac = 10 + dex;
    // Jack of All Trades adds half the Proficiency Bonus to Initiative, as
    // SRD-DECISIONS keeps it, unless Alert already adds the whole bonus.
    d.initiative = dex + (d.alert                         ? proficiency
                          : at(CharacterClass::bard) >= 2 ? proficiency / 2
                                                          : 0);
    d.speed = race == "Goliath" ? 35 : 30;
    d.level = level;
    d.class_levels = levels;
    for (const auto &id : level_ids)
    {
        d.level_dice.push_back(detail::class_hit_die(detail::class_from_id(id)));
    }
    std::sort(d.level_dice.begin(), d.level_dice.end());
    d.melee_bonus = proficiency + str;
    d.melee = {0, 0, std::max(0, 1 + str)};
    d.champion = at(CharacterClass::fighter) >= 3;
    d.critical_from = !d.champion ? 20 : at(CharacterClass::fighter) >= 15 ? 18 : 19;
    // Extra Attack from two classes does not stack (SRD 5.2.1 p. 25).
    static constexpr std::array extra_attack_classes{
        CharacterClass::fighter, CharacterClass::barbarian, CharacterClass::monk,
        CharacterClass::paladin, CharacterClass::ranger};
    d.extra_attacks = std::any_of(extra_attack_classes.begin(), extra_attack_classes.end(),
                                  [&](CharacterClass held)
                                  {
                                      return at(held) >= 5;
                                  })
                          ? 1
                          : 0;
    // Two Extra Attacks (Fighter 11): three attacks with each Attack action.
    if (at(CharacterClass::fighter) >= 11)
    {
        d.extra_attacks = 2;
    }
    d.studied_attacks = at(CharacterClass::fighter) >= 13;
    const unsigned barbarian = at(CharacterClass::barbarian);
    d.mindless_rage = barbarian >= 6;
    d.feral_instinct = d.instinctive_pounce = barbarian >= 7;
    d.brutal_strike = barbarian >= 9;
    // Relentless Rage's DC climbs 10, 15, 20, 25, 30; past that no save passes.
    d.relentless_rage = barbarian >= 11 ? 5 : 0;
    d.improved_brutal = barbarian >= 13;
    d.intimidating_presence = barbarian >= 14 ? 1 : 0;
    d.persistent_rage = barbarian >= 15 ? 1 : 0;
    d.retaliation = barbarian >= 10;
    const unsigned rogue = at(CharacterClass::rogue);
    d.cunning_strike = d.uncanny_dodge = rogue >= 5;
    d.evasion = rogue >= 7 || at(CharacterClass::monk) >= 7;
    d.martial_die = at(CharacterClass::monk) == 0    ? 6
                    : at(CharacterClass::monk) >= 11 ? 10
                    : at(CharacterClass::monk) >= 5  ? 8
                                                     : 6;
    d.wisdom = ability_modifier(scores[Ability::wisdom]);
    d.stunning_strike = at(CharacterClass::monk) >= 5;
    d.empowered_strikes = at(CharacterClass::monk) >= 6;
    d.heightened_focus = d.self_restoration = at(CharacterClass::monk) >= 10;
    d.fleet_step = at(CharacterClass::monk) >= 11;
    d.deflect_energy = at(CharacterClass::monk) >= 13;
    d.disciplined_survivor = at(CharacterClass::monk) >= 14;
    d.perfect_focus = at(CharacterClass::monk) >= 15;
    d.aura_of_protection = at(CharacterClass::paladin) >= 6
                               ? std::max(1, ability_modifier(scores[Ability::charisma]))
                               : 0;
    d.aura_of_devotion = at(CharacterClass::paladin) >= 7;
    d.abjure_foes = at(CharacterClass::paladin) >= 9
                        ? std::max(1, ability_modifier(scores[Ability::charisma]))
                        : 0;
    d.aura_of_courage = at(CharacterClass::paladin) >= 10;
    d.radiant_strikes = at(CharacterClass::paladin) >= 11;
    d.restoring_touch = at(CharacterClass::paladin) >= 14;
    d.smite_of_protection = at(CharacterClass::paladin) >= 15;
    d.wholeness =
        at(CharacterClass::monk) >= 6 ? std::max(1, ability_modifier(scores[Ability::wisdom])) : 0;
    d.supreme_sneak = rogue >= 9;
    d.improved_cunning = rogue >= 11;
    d.devious_strikes = rogue >= 14;
    d.tactical_shift = at(CharacterClass::fighter) >= 5;
    d.tactical_master = at(CharacterClass::fighter) >= 9;
    d.heroic_warrior = at(CharacterClass::fighter) >= 10;
    d.indomitable = at(CharacterClass::fighter) == 0    ? 0
                    : at(CharacterClass::fighter) >= 13 ? 2
                    : at(CharacterClass::fighter) >= 9  ? 1
                                                        : 0;
    d.arcane = at(CharacterClass::wizard) > 0 ? 1 : 0;
    d.lay_on_hands = at(CharacterClass::paladin) > 0 ? 5 * at(CharacterClass::paladin) : 0;
    // Channel Divinity: two uses, three from Paladin level 11.
    // A Cleric / Paladin has the higher of the two classes' uses (MULTI-1).
    d.channel_divinity = at(CharacterClass::paladin) >= 11 || at(CharacterClass::cleric) >= 6  ? 3
                         : at(CharacterClass::paladin) >= 3 || at(CharacterClass::cleric) >= 2 ? 2
                                                                                               : 0;
    d.sacred_weapon = at(CharacterClass::paladin) >= 3;
    d.divine_spark = at(CharacterClass::cleric) >= 2;
    d.sear_undead = at(CharacterClass::cleric) >= 5;
    d.blessed_healer = at(CharacterClass::cleric) >= 6;
    d.divine_intervention = at(CharacterClass::cleric) >= 10 ? 1 : 0;
    d.life_domain = at(CharacterClass::cleric) >= 3;
    d.evoker = at(CharacterClass::wizard) >= 3;
    d.sculptor = at(CharacterClass::wizard) >= 6;
    d.empowered = at(CharacterClass::wizard) >= 10;
    d.overchannel = at(CharacterClass::wizard) >= 14;
    d.free_smite = at(CharacterClass::paladin) >= 2 ? 1 : 0;
    // Favored Enemy: 2 free Hunter's Marks, 3 from level 5, 4 from 9, 5 from 13.
    d.favored_enemy = at(CharacterClass::ranger) == 0    ? 0
                      : at(CharacterClass::ranger) >= 13 ? 5
                      : at(CharacterClass::ranger) >= 9  ? 4
                      : at(CharacterClass::ranger) >= 5  ? 3
                                                         : 2;
    const unsigned ranger = at(CharacterClass::ranger);
    const int wisdom_uses = std::max(1, ability_modifier(scores[Ability::wisdom]));
    d.tireless = ranger >= 10 ? wisdom_uses : 0;
    d.superior_prey = ranger >= 11;
    d.relentless_hunter = ranger >= 13;
    d.natures_veil = ranger >= 14 ? wisdom_uses : 0;
    d.superior_defense = ranger >= 15;
    d.medicine = ability_modifier(scores[Ability::wisdom]);
    d.tactical_mind = at(CharacterClass::fighter) >= 2;
    d.cunning = at(CharacterClass::rogue) >= 2;
    d.sneak_level = at(CharacterClass::rogue) > 0 ? at(CharacterClass::rogue) : 0;
    d.great_weapon_fighting = (features & 8) != 0;
    d.two_weapon_fighting = (features & 16) != 0;
    d.surges = at(CharacterClass::fighter) >= 2 ? 1 : 0;
    // Second Wind: 2 uses, 3 from level 4, 4 from level 10.
    d.winds = at(CharacterClass::fighter) > 0 ? (at(CharacterClass::fighter) >= 10  ? 4
                                                 : at(CharacterClass::fighter) >= 4 ? 3
                                                                                    : 2)
                                              : 0;
    // Spell slots (SRD 5.2.1 p. 25, MULTI-1): one Spellcasting class's own
    // table; two or more add their levels, a Paladin's and a Ranger's at half
    // rounded up, on the Multiclass Spellcaster table, the full casters'.
    // Pact Magic's slots join them.
    std::vector<CharacterClass> spellcasting;
    unsigned combined = 0;
    for (const auto held : {CharacterClass::bard, CharacterClass::cleric, CharacterClass::druid,
                            CharacterClass::sorcerer, CharacterClass::wizard,
                            CharacterClass::paladin, CharacterClass::ranger})
    {
        if (at(held))
        {
            spellcasting.push_back(held);
            combined += held == CharacterClass::paladin || held == CharacterClass::ranger
                            ? (at(held) + 1) / 2
                            : at(held);
        }
    }
    d.slots = spellcasting.size() == 1 ? spell_slots(spellcasting.front(), at(spellcasting.front()))
              : spellcasting.size() > 1 ? spell_slots(CharacterClass::wizard, combined)
                                        : SpellSlots{};
    if (at(CharacterClass::warlock))
    {
        const auto pact = spell_slots(CharacterClass::warlock, at(CharacterClass::warlock));
        d.pact_slot_level = static_cast<int>(pact_level(pact));
        d.pact_slots = pact[static_cast<std::size_t>(d.pact_slot_level)];
        d.pact_magic = spellcasting.empty();
        d.slots[static_cast<std::size_t>(d.pact_slot_level)] += d.pact_slots;
    }
    // The spellcasting ability of the first spellcasting class; another
    // class's spells use its own (MULTI-1).
    const auto ability_of = [](CharacterClass held)
    {
        return held == CharacterClass::cleric || held == CharacterClass::ranger ||
                       held == CharacterClass::druid
                   ? Ability::wisdom
               : held == CharacterClass::warlock || held == CharacterClass::sorcerer ||
                       held == CharacterClass::paladin || held == CharacterClass::bard
                   ? Ability::charisma
                   : Ability::intelligence;
    };
    const auto casters = detail::casting_classes(detail::ClassHistory(level_ids));
    const auto main_caster = casters.empty() ? klass : casters.front();
    const Ability casting_ability = ability_of(main_caster);
    d.casting = proficiency + ability_modifier(scores[casting_ability]);
    if ((features & 1) && !at(CharacterClass::fighter) && !at(CharacterClass::paladin) &&
        !at(CharacterClass::ranger))
    {
        throw std::runtime_error("Invalid prepared spells or feat prerequisites");
    }
    d.spells = stored_spells;
    d.savage = (features & 2) != 0;
    d.known_cantrips = detail::spells_of_level(d.spells, true);

    bool weapon = false, armor = false, shield = false;
    bool monk_weapons = true; // every wielded weapon is a Simple Melee or Light Martial one
    unsigned hands = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        std::string key;
        in >> std::quoted(key);
        d.equipment_keys.push_back(std::move(key));
    }
    if (equipment_override)
    {
        d.equipment_keys.assign(equipment_override->begin(), equipment_override->end());
    }
    // Gauntlets of Ogre Power: Strength 19 unless it is higher (MAGIC-4). The
    // modifier follows after the gear; the score counts for armor and Heavy
    // weapons here.
    const bool ogre_power =
        std::any_of(d.equipment_keys.begin(), d.equipment_keys.end(),
                    [](const auto &key)
                    {
                        return rules::base_item_key(key) == "gauntlets_of_ogre_power";
                    });
    auto worn_scores = scores;
    if (ogre_power)
    {
        worn_scores[Ability::strength] = std::max(worn_scores[Ability::strength], 19);
    }
    // Grants come before equipment because a Divine Order can add training.
    std::string background;
    in >> std::quoted(background);
    const auto grants = detail::read_grants(in);
    d.hp = maximum_hit_points(detail::ClassHistory(level_ids), race == "Dwarf", hp_modifiers);
    d.fast_hands = std::any_of(grants.begin(), grants.end(),
                               [](const auto &g)
                               {
                                   return g.id == "feature:fast_hands";
                               });
    d.use_magic_device = std::any_of(grants.begin(), grants.end(),
                                     [](const auto &g)
                                     {
                                         return g.id == "feature:use_magic_device";
                                     });
    // A species' creation choices: size, a Wood Elf's speed and a Tiefling's
    // or Dragonborn's resistance.
    const auto chose = [&](std::string_view id)
    {
        return std::any_of(grants.begin(), grants.end(),
                           [&](const auto &g)
                           {
                               return g.id == id;
                           });
    };
    if (chose("size:small"))
    {
        d.size = 1;
    }
    if (chose("lineage:wood_elf"))
    {
        d.speed = 35;
    }
    using detail::DamageType;
    for (const auto &[choice, type] :
         std::initializer_list<std::pair<std::string_view, DamageType>>{
             {"legacy:abyssal", DamageType::poison},
             {"legacy:chthonic", DamageType::necrotic},
             {"legacy:infernal", DamageType::fire},
             {"dragon:black", DamageType::acid},
             {"dragon:copper", DamageType::acid},
             {"dragon:blue", DamageType::lightning},
             {"dragon:bronze", DamageType::lightning},
             {"dragon:brass", DamageType::fire},
             {"dragon:gold", DamageType::fire},
             {"dragon:red", DamageType::fire},
             {"dragon:green", DamageType::poison},
             {"dragon:silver", DamageType::cold},
             {"dragon:white", DamageType::cold}})
    {
        if (chose(choice))
        {
            d.affinities.push_back(
                {detail::AffinityKind::resistance, type, "species/" + std::string(choice)});
            // Breath Weapon: as many uses as the Proficiency Bonus.
            if (choice.starts_with("dragon:"))
            {
                d.breath = type;
                d.trait_uses = proficiency;
            }
        }
    }
    // Giant Ancestry: as many uses as the Proficiency Bonus.
    for (const auto &[choice, boon] : std::initializer_list<std::pair<std::string_view, GiantBoon>>{
             {"giant:cloud", GiantBoon::cloud},
             {"giant:fire", GiantBoon::fire},
             {"giant:frost", GiantBoon::frost},
             {"giant:hill", GiantBoon::hill},
             {"giant:stone", GiantBoon::stone},
             {"giant:storm", GiantBoon::storm}})
    {
        if (chose(choice))
        {
            d.giant = boon;
            d.trait_uses = proficiency;
        }
    }
    // Magic Initiate's and a lineage's spells: their own ability, and a free
    // cast of the leveled one each Long Rest.
    auto gifts = detail::magic_initiate_spells(grants);
    for (auto &lineage : detail::lineage_spells(grants, level))
    {
        gifts.push_back(std::move(lineage));
    }
    for (const auto &feat : gifts)
    {
        for (const auto &spell : feat.spells)
        {
            d.initiate_casting.emplace(spell, proficiency + ability_modifier(scores[feat.ability]));
        }
        if (!feat.free_spell.empty())
        {
            d.initiate_spells.push_back(feat.free_spell);
        }
        if (feat.feat.starts_with("species:"))
        {
            d.lineage_spells.insert(feat.free_spell);
        }
    }
    // A multiclass character casts each class's spells with that class's
    // ability (SRD 5.2.1 p. 25): a cantrip with the class that taught it, a
    // prepared spell with the best of the classes whose list has it, as the
    // profile does not say which prepared it (MULTI-1).
    if (casters.size() > 1)
    {
        const auto class_bonus = [&](CharacterClass held)
        {
            return proficiency + ability_modifier(scores[ability_of(held)]);
        };
        for (const auto &id : stored_spells)
        {
            if (d.initiate_casting.contains(id))
            {
                continue;
            }
            std::optional<int> best;
            for (const auto &g : grants)
            {
                if (g.id == "spell:" + id && g.source_id.starts_with("class:"))
                {
                    const auto rest = std::string_view(g.source_id).substr(6);
                    best = class_bonus(detail::class_from_id(rest.substr(0, rest.find(':'))));
                }
            }
            for (const auto held : casters)
            {
                if (!best && detail::on_class_list(held, id))
                {
                    best = class_bonus(held);
                }
                else if (detail::on_class_list(held, id))
                {
                    best = std::max(*best, class_bonus(held));
                }
            }
            if (best && *best != d.casting)
            {
                d.initiate_casting.emplace(id, *best);
            }
        }
    }
    // A stored spell is one of the classes' at its level, or one Magic
    // Initiate teaches (MULTI-1).
    std::vector<std::string> allowed;
    for (const auto &choice : detail::creation_choices(CreationField::character_class))
    {
        const auto held = detail::class_from_id(choice.id);
        if (at(held))
        {
            for (auto &id : allowed_spells(held, at(held)))
            {
                allowed.push_back(std::move(id));
            }
        }
    }
    const auto storable = [&](const std::string &id)
    {
        return detail::knows_spell(allowed, id) || d.initiate_casting.contains(id) ||
               bard_secret(CharacterClass::bard, at(CharacterClass::bard), grants, id) ||
               detail::warrior_cantrip(grants, id);
    };
    if (!std::all_of(stored_spells.begin(), stored_spells.end(), storable))
    {
        throw std::runtime_error("Invalid prepared spells or feat prerequisites");
    }
    const auto invoked = [&](std::string_view id)
    {
        return std::any_of(grants.begin(), grants.end(),
                           [&](const auto &g)
                           {
                               return g.id == "invocation:" + std::string(id);
                           });
    };
    for (const auto &grant : grants)
    {
        for (auto &[prefix, cantrips] : std::initializer_list<
                 std::pair<std::string_view, std::set<std::string, std::less<>> *>>{
                 {"invocation:agonizing_blast:", &d.agonizing},
                 {"invocation:eldritch_spear:", &d.eldritch_spear},
                 {"invocation:repelling_blast:", &d.repelling}})
        {
            if (grant.id.starts_with(prefix))
            {
                cantrips->insert(grant.id.substr(prefix.size()));
            }
        }
    }
    d.armor_of_shadows = invoked("armor_of_shadows");
    d.devils_sight = invoked("devils_sight");
    d.eldritch_mind = invoked("eldritch_mind");
    d.fiendish_vigor = invoked("fiendish_vigor");
    d.pact_of_the_blade = invoked("pact_of_the_blade");
    // Thirsting Blade: Extra Attack with the pact weapon.
    d.thirsting_blade = invoked("thirsting_blade");
    if (d.thirsting_blade)
    {
        d.extra_attacks = std::max(d.extra_attacks, 1);
    }
    // Devouring Blade: three attacks.
    if (invoked("devouring_blade"))
    {
        d.extra_attacks = std::max(d.extra_attacks, 2);
    }
    d.eldritch_smite = invoked("eldritch_smite");
    d.lifedrinker = invoked("lifedrinker");
    d.witch_sight = invoked("witch_sight");
    d.gift_of_protectors = invoked("gift_of_the_protectors");
    // Lessons of the First Ones: an Origin feat.
    d.savage |= invoked("lessons_savage_attacker");
    if (invoked("lessons_alert"))
    {
        d.alert = true;
        d.initiative = dex + proficiency;
    }
    int shield_magic = 0;
    // Worn items (MAGIC-4): the better Ring of Protection, the better bracers'
    // Armor Class, and flat Armor Class and save changes.
    int ring_bonus = 0, bracers_ac = 0, worn_ac = 0, worn_saves = 0;
    std::array<unsigned, 6> worn_count{};
    for (const auto &key : d.equipment_keys)
    {
        const auto enchantment = rules::parse_item_key(key);
        if (const auto *item = detail::weapon(key))
        {
            monk_weapons &= item->dice && !item->ranged && (!item->martial || item->light);
            if (weapon)
            {
                if (d.other_weapon)
                {
                    throw std::runtime_error("Only two weapons may be equipped");
                }
                d.other_weapon = true;
                hands += item->hands;
                continue;
            }
            weapon = true;
            d.weapon_label = weapon_label(key);
            d.shillelagh_weapon = enchantment.base == "club" || enchantment.base == "quarterstaff";
            d.finesse = item->finesse;
            d.ranged_weapon = item->ranged;
            d.ranged_loading = item->ranged && item->loading;
            d.ranged_missile = weapon_missile(*item);
            d.weapon_hands = item->hands;
            d.versatile_sides = item->versatile_sides;
            hands += d.weapon_hands;
            // Pact of the Blade: a melee pact weapon uses Charisma if better,
            // with proficiency.
            const bool pact = d.pact_of_the_blade && !item->ranged;
            const int modifier = std::max(item->finesse  ? std::max(str, dex)
                                          : item->ranged ? dex
                                                         : str,
                                          pact ? ability_modifier(scores[Ability::charisma]) : -5);
            const int bonus = (trained(klass, grants, key) || pact ? proficiency : 0) + modifier;
            if ((item->dice || item->fixed_damage) && !item->ranged)
            {
                d.melee_ability = modifier;
                d.melee_bonus = bonus;
                d.melee = {item->dice, item->sides,
                           item->fixed_damage ? item->fixed_damage : modifier};
                d.melee_type = item->type;
                d.melee_magic = enchantment.bonus;
                d.bane_undead = enchantment.power == rules::ItemPower::bane_undead;
                d.flame_tongue = enchantment.power == rules::ItemPower::flame_tongue;
                d.reach = melee_reach;
                d.melee_heavy_disadvantage = item->heavy_disadvantage(worn_scores);
            }
            if (item->range)
            {
                d.ranged_ability = modifier;
                d.ranged_bonus = bonus + ((item->ranged && (features & 4)) ? 2 : 0);
                d.ranged = {item->dice, item->sides,
                            item->fixed_damage ? item->fixed_damage : modifier};
                d.ranged_type = item->type;
                d.ranged_magic = enchantment.bonus;
                d.range = item->range;
                d.long_range = item->long_range;
                d.ranged_heavy_disadvantage = item->heavy_disadvantage(worn_scores);
            }
        }
        else if (const auto *armor_item = detail::armor(key);
                 armor_item && armor_item->category != detail::ArmorCategory::shield)
        {
            if (armor)
            {
                throw std::runtime_error("Only one armor may be equipped");
            }
            if (!trained(klass, grants, key))
            {
                d.str_dex_disadvantage = true;
                d.spells.clear();
            }
            armor = true;
            d.heavy_armor = armor_item->category == detail::ArmorCategory::heavy;
            d.ac =
                armor_item->base_ac + armor_item->dexterity_contribution(dex) + enchantment.bonus;
            d.stealth_disadvantage = armor_item->stealth_disadvantage;
            if (worn_scores[Ability::strength] < armor_item->strength)
            {
                d.speed -= 10;
            }
        }
        else if (detail::is_shield(key))
        {
            if (shield)
            {
                throw std::runtime_error("Only one shield may be equipped");
            }
            shield = true;
            shield_magic = enchantment.bonus;
            ++hands;
        }
        else if (key == magic_lock_picks)
        {
            // Carried, and in the gear only for the check to pick a lock.
            d.pick_advantage = true;
        }
        else if (key == stone_of_good_luck)
        {
            d.check_bonus = 1;
            worn_saves += 1;
        }
        else if (const auto *worn = detail::worn_item(key))
        {
            auto &worn_of_kind = worn_count[static_cast<std::size_t>(worn->kind)];
            if (++worn_of_kind > (worn->kind == detail::WornKind::ring ? 2U : 1U))
            {
                throw std::runtime_error(worn->kind == detail::WornKind::ring
                                             ? "Only two rings may be worn"
                                             : "Only one of each worn item may be worn");
            }
            const auto base = enchantment.base;
            if (base == "ring_of_protection")
            {
                ring_bonus = std::max(ring_bonus, enchantment.bonus);
            }
            else if (base == "bracers")
            {
                bracers_ac = 10 + enchantment.bonus + dex;
            }
            else if (base == "cloak_of_displacement")
            {
                worn_ac += 2;
                d.displacement = true;
            }
            else if (base == "necklace")
            {
                worn_ac += enchantment.bonus;
                worn_saves += enchantment.bonus;
            }
            d.stealth_advantage |= base == "boots_of_elvenkind" || base == "cloak_of_elvenkind";
            d.hard_to_perceive |= base == "cloak_of_elvenkind";
            d.fire_ward |= base == "ring_of_fire_resistance";
        }
        else
        {
            throw std::runtime_error("Unsupported equipment conversion: " + key);
        }
    }
    d.shield = shield;
    // The grip follows the other hand at attack time: a Versatile weapon is
    // wielded two-handed exactly when no shield or second weapon is held.
    if (d.versatile_sides && !shield && !d.other_weapon)
    {
        hands = hands - d.weapon_hands + 2;
        d.weapon_hands = 2;
    }
    const auto training =
        detail::training_profile(grants, detail::ClassHistory(level_ids), race, background, scores);
    d.medicine = std::find_if(training.skills.begin(), training.skills.end(),
                              [](const auto &skill)
                              {
                                  return skill.id == "medicine";
                              })
                     ->bonus;
    d.athletics = std::find_if(training.skills.begin(), training.skills.end(),
                               [](const auto &skill)
                               {
                                   return skill.id == "athletics";
                               })
                      ->bonus;
    d.investigation = std::find_if(training.skills.begin(), training.skills.end(),
                                   [](const auto &skill)
                                   {
                                       return skill.id == "investigation";
                                   })
                          ->bonus;
    d.arcana = std::find_if(training.skills.begin(), training.skills.end(),
                            [](const auto &skill)
                            {
                                return skill.id == "arcana";
                            })
                   ->bonus;
    d.perception = std::find_if(training.skills.begin(), training.skills.end(),
                                [](const auto &skill)
                                {
                                    return skill.id == "perception";
                                })
                       ->bonus;
    d.stealth = std::find_if(training.skills.begin(), training.skills.end(),
                             [](const auto &skill)
                             {
                                 return skill.id == "stealth";
                             })
                    ->bonus;
    // A Stone of Good Luck: Initiative is a Dexterity check too.
    d.perception += d.check_bonus;
    d.stealth += d.check_bonus;
    d.initiative += d.check_bonus;
    if (at(CharacterClass::rogue) >= 7)
    {
        for (const auto &skill : training.skills)
        {
            if (skill.proficient)
            {
                d.reliable.insert(skill.id);
            }
        }
    }
    for (const auto &grant : grants)
    {
        if (detail::is_mastery_grant(grant))
        {
            d.masteries.push_back(grant.id.substr(8));
        }
        if (grant.id.starts_with("metamagic:"))
        {
            d.metamagic.push_back(grant.id.substr(10));
        }
        d.hunters_lore |= grant.id == "feature:hunters_lore";
        d.colossus_slayer |= grant.id == "prey:colossus_slayer";
        d.horde_breaker |= grant.id == "prey:horde_breaker";
        d.escape_the_horde |= grant.id == "tactics:escape_the_horde";
        if (grant.id == "strikes:divine_strike")
        {
            d.divine_strike = at(CharacterClass::cleric) >= 14 ? 2 : 1;
        }
        d.potent_spellcasting |= grant.id == "strikes:potent_spellcasting";
        d.multiattack_defense |= grant.id == "tactics:multiattack_defense";
        if (grant.id.starts_with("arcanum:"))
        {
            d.arcanum.insert(grant.id.substr(8));
        }
        if (grant.id.starts_with("land:"))
        {
            d.land = grant.id.substr(5);
        }
        d.druid_potent |= grant.id == "fury:potent_spellcasting";
        if (grant.id == "fury:primal_strike")
        {
            d.primal_strike = at(CharacterClass::druid) >= 15 ? 2 : 1;
        }
        // Fiendish Resilience: Resistance to its type.
        if (grant.id.starts_with("resilience:"))
        {
            d.affinities.push_back({detail::AffinityKind::resistance,
                                    detail::damage_type(grant.id.substr(11)),
                                    "feature:fiendish_resilience"});
        }
        // Elemental Affinity: Resistance to its type.
        if (grant.id.starts_with("affinity:"))
        {
            d.elemental_affinity = detail::damage_type(grant.id.substr(9));
            d.affinities.push_back({detail::AffinityKind::resistance, d.elemental_affinity,
                                    "feature:elemental_affinity"});
        }
    }
    for (const auto &id : detail::circle_spells(at(CharacterClass::druid), d.land))
    {
        if (detail::find_spell(id)->level)
        {
            d.circle.insert(id);
        }
    }
    // Nature's Ward (Land 10): Resistance by the current land.
    if (d.natures_ward && !d.land.empty())
    {
        const auto type = d.land == "arid"        ? detail::DamageType::fire
                          : d.land == "polar"     ? detail::DamageType::cold
                          : d.land == "temperate" ? detail::DamageType::lightning
                                                  : detail::DamageType::poison;
        d.affinities.push_back({detail::AffinityKind::resistance, type, "feature:natures_ward"});
    }
    // A multiclass character's stored list does not say which class each
    // spell is prepared for; the sheet it was made from was checked, and each
    // spell against the classes' lists above (MULTI-1).
    if (levels.count() == 1)
    {
        std::vector<std::string> prepared;
        if (detail::prepares_spells(klass))
        {
            // Always-prepared spells are stored with the others but never prepared.
            prepared = detail::spells_of_level(stored_spells, false);
            for (const auto &id : detail::always_prepared_spells(klass, level, grants))
            {
                std::erase(prepared, id);
            }
        }
        const auto access = detail::spell_access(grants, detail::ClassHistory(level_ids), prepared);
        // Compare as sets: the grants and the stored list must describe the same
        // spells, independent of the order either was written in.
        const auto same_spells = [](std::vector<std::string> left, std::vector<std::string> right)
        {
            std::sort(left.begin(), left.end());
            std::sort(right.begin(), right.end());
            return left == right;
        };
        if (!same_spells(detail::casting_ids(access), stored_spells))
        {
            throw std::runtime_error("Character casting access disagrees with spell grants");
        }
    }
    const auto features_only =
        without_manuals(detail::without_spell_grants(detail::without_training(grants)));
    const auto effects = detail::validate_grants(features_only, detail::ClassHistory(level_ids),
                                                 detail::race_id(race), background);
    if (effects.feats != features)
    {
        throw std::runtime_error("Character effects disagree with acquired grants");
    }
    // Each level's Constitution modifier: the score before any feat, plus
    // what the Ability Score Improvements gained by that level gave it.
    for (unsigned i = 0; i < level; ++i)
    {
        int score = scores[Ability::constitution] - effects.abilities[Ability::constitution];
        for (const auto &g : grants)
        {
            if (g.id == "feat:ability_score_improvement" && g.level <= i + 1)
            {
                if (const auto raised = g.choices.find("constitution"); raised != g.choices.end())
                {
                    score += raised->second == "2" ? 2 : 1;
                }
            }
        }
        if (hp_modifiers[i] != ability_modifier(score))
        {
            throw std::runtime_error("HP history disagrees with acquired ability choices");
        }
    }
    if (race == "Dwarf")
    {
        d.affinities.push_back({detail::AffinityKind::resistance, detail::DamageType::poison,
                                "species:dwarf/trait:dwarven_resilience"});
    }
    if (hands > 2)
    {
        throw std::runtime_error(
            "Not enough free hands. Unequip the shield or two-handed weapon first.");
    }
    // Bracers are worn instead of armor: the better Armor Class counts.
    d.ac = std::max(d.ac, bracers_ac);
    worn_ac += ring_bonus;
    worn_saves += ring_bonus;
    for (auto &save : d.saves)
    {
        save += worn_saves;
    }
    d.item_saves = worn_saves;
    if (!armor && at(CharacterClass::barbarian) > 0)
    {
        d.ac = std::max(d.ac, 10 + dex + con);
    }
    // Draconic Resilience: dragon-like scales.
    if (!armor && at(CharacterClass::sorcerer) >= 3)
    {
        d.ac = std::max(d.ac, 10 + dex + ability_modifier(scores[Ability::charisma]));
    }
    if (!armor && !shield && at(CharacterClass::monk) > 0)
    {
        d.ac = std::max(d.ac, 10 + dex + ability_modifier(scores[Ability::wisdom]));
    }
    // Martial Arts: the better of Strength and Dexterity for attack and damage
    // rolls, and the Martial Arts die (a d6 through level four) when larger.
    d.martial_arts = at(CharacterClass::monk) > 0 && !armor && !shield && monk_weapons;
    // Unarmored Movement: 10 feet more without armor or a Shield, 15 from level
    // 6, 20 from 10 and 25 from 14.
    if (at(CharacterClass::monk) >= 2 && !armor && !shield)
    {
        const auto monk = at(CharacterClass::monk);
        d.speed += monk >= 14 ? 25 : monk >= 10 ? 20 : monk >= 6 ? 15 : 10;
    }
    // Fast Movement and Roving: 10 feet more without Heavy armor.
    if ((at(CharacterClass::barbarian) >= 5 || at(CharacterClass::ranger) >= 6) && !d.heavy_armor)
    {
        d.speed += 10;
    }
    if (d.martial_arts)
    {
        d.martial_modifier = std::max(str, dex);
        if (!weapon)
        {
            d.melee = {1, d.martial_die, d.martial_modifier};
            d.melee_bonus = proficiency + d.martial_modifier;
        }
        else
        {
            d.melee_bonus += d.martial_modifier - d.melee_ability;
            d.melee.bonus = d.martial_modifier;
            d.melee.sides = std::max(d.melee.sides, d.martial_die);
        }
        d.melee_ability = d.martial_modifier;
    }
    // Empowered Strikes: Unarmed Strikes deal Force damage, which almost
    // nothing resists, so it is always chosen.
    if (d.empowered_strikes && !weapon)
    {
        d.melee_type = detail::DamageType::force;
    }
    const int shield_ac = shield && trained(klass, grants, "shield") ? 2 + shield_magic : 0;
    d.ac += shield_ac + worn_ac;
    // Mage Armor's AC for a creature wearing no armor.
    if (!armor)
    {
        d.mage_armor_ac = std::max(13 + dex, bracers_ac) + shield_ac + worn_ac;
        d.unarmored_arcane_caster = at(CharacterClass::wizard) || at(CharacterClass::sorcerer);
    }
    if (armor && (features & 1))
    {
        ++d.ac;
    }
    // The alignment, for spells that change with it (Spirit Guardians).
    std::string alignment;
    in >> std::quoted(alignment);
    if (!in || alignment.size() > 40)
    {
        throw std::runtime_error("Invalid character profile alignment");
    }
    d.evil =
        alignment.find("evil") != std::string::npos || alignment.find("Evil") != std::string::npos;
    in >> std::ws;
    if (!in.eof())
    {
        throw std::runtime_error("Invalid character profile fields");
    }
    // Gauntlets of Ogre Power: Strength 19, a +4 modifier.
    return ogre_power ? strengthened(std::move(d), 4) : d;
}
} // namespace opengold::srd5::detail
