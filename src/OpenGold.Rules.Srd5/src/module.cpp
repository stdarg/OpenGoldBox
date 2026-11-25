#include "module.h"
#include "session.h"
#include <cctype>
#include <chrono>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>

// The SRD campaign rules module: training, advancement, equipment, rests
// and temples; srd5.cpp reads the rules content it is built from.
namespace opengold::srd5::detail
{
using namespace rules;

// A one-group list with the group moved in: a braced initializer list would
// copy it (Effective Modern C++ Item 7).
std::vector<TrainingChoiceGroup> only(TrainingChoiceGroup group)
{
    std::vector<TrainingChoiceGroup> result;
    result.push_back(std::move(group));
    return result;
}

// A damage die's average; with Great Weapon Fighting a 1 or 2 counts as a 3.
double average_die(int sides, bool great_weapon_fighting)
{
    double total = 0;
    for (int face = 1; face <= sides; ++face)
    {
        total += great_weapon_fighting ? std::max(face, 3) : face;
    }
    return sides > 0 ? total / sides : 0;
}

// One melee weapon attack as auto-equip weighs it (EQUIP-1).
struct ExpectedAttack
{
    int attack_bonus{};
    Dice dice;
    int damage_bonus{};
    int critical_from{20};
    bool disadvantage{}, great_weapon_fighting{};
};

// The chance of a hit and the damage a hit is expected to deal, over every
// d20 face (pairs of faces with Disadvantage): a 1 misses, a 20 hits, and a
// Critical Hit doubles the dice.
struct AttackOdds
{
    double hit_chance{}, damage{};
};

AttackOdds attack_odds(const ExpectedAttack &attack, int target_armor_class)
{
    const double dice =
        attack.dice.count * average_die(attack.dice.sides, attack.great_weapon_fighting);
    AttackOdds odds;
    for (int face = 1; face <= 20; ++face)
    {
        // With Disadvantage the lower of two dice shows `face`.
        const double chance =
            attack.disadvantage
                ? ((21.0 - face) * (21.0 - face) - (20.0 - face) * (20.0 - face)) / 400.0
                : 1.0 / 20.0;
        const bool critical = face >= attack.critical_from;
        const bool hit = face != 1 && (critical || face == 20 ||
                                       face + attack.attack_bonus >= target_armor_class);
        if (hit)
        {
            odds.hit_chance += chance;
            odds.damage +=
                chance * std::max(0.0, (critical ? 2 * dice : dice) + attack.damage_bonus);
        }
    }
    return odds;
}

// A creature's melee weapon attack, from its definition, with the weapon's
// enchantment added to the attack and the damage.
ExpectedAttack melee_attack(const Definition &d)
{
    auto dice = d.melee;
    if (d.versatile_sides && d.weapon_hands == 2)
    {
        dice.sides = d.versatile_sides;
    }
    return {.attack_bonus = d.melee_bonus + d.melee_magic,
            .dice = dice,
            .damage_bonus = dice.bonus + d.melee_magic,
            .critical_from = d.critical_from,
            .disadvantage = d.str_dex_disadvantage || d.melee_heavy_disadvantage,
            .great_weapon_fighting =
                d.great_weapon_fighting && !d.weapon_label.empty() && d.weapon_hands == 2};
}

// The rules content `bytes` hold, without the monster ability rows its own
// `off` rows and `switched` leave out ("ability" or "ability creature").
Content read_content(std::string_view bytes, const std::set<std::string> &switched);

class Module final : public RulesModule
{
  public:
    Module(Content content, std::string bytes)
        : content_(std::make_shared<const Content>(std::move(content))), bytes_(std::move(bytes))
    {
    }

    // Re-reads the content without the abilities; its identity, which saves
    // carry, stays the content's.
    void switch_off_abilities(std::vector<std::string> abilities) override
    {
        content_ = std::make_shared<const Content>(
            read_content(bytes_, {std::make_move_iterator(abilities.begin()),
                                  std::make_move_iterator(abilities.end())}));
    }

    Identity identity() const override
    {
        return content_->identity;
    }

    std::unique_ptr<CombatSession> create(Encounter e, std::uint64_t seed) const override
    {
        return std::make_unique<Session>(content_, std::move(e), seed);
    }

    std::unique_ptr<CombatSession> restore(std::string_view checkpoint) const override
    {
        return Session::restore(content_, checkpoint);
    }

    unsigned experience_for_level(unsigned level) const override
    {
        // SRD 5.2.1 p. 23, through level 15.
        static constexpr std::array<unsigned, 16> thresholds{
            0,     0,     300,   900,   2700,   6500,   14000,  23000,
            34000, 48000, 64000, 85000, 100000, 120000, 140000, 165000};
        if (level < 1 || level > 15)
        {
            throw std::runtime_error("Unsupported character level");
        }
        return thresholds[level];
    }

    CreatureStats creature_stats(const Participant &creature) const override
    {
        if (!creature.character_profile.empty() ||
            !content_->definitions.contains(creature.definition))
        {
            throw std::runtime_error("No creature definition: " + creature.definition);
        }
        const auto d = fielded(*content_, creature);
        CreatureStats stats{d.ac, d.hp, d.initiative, d.speed, d.multiattack};
        stats.melee = {d.melee_bonus + d.melee_magic, d.melee.count, d.melee.sides,
                       d.melee.bonus + d.melee_magic,
                       std::string(detail::damage_name(d.melee_type))};
        if (d.range)
        {
            stats.ranged = AttackStats{d.ranged_bonus + d.ranged_magic,
                                       d.ranged.count,
                                       d.ranged.sides,
                                       d.ranged.bonus + d.ranged_magic,
                                       std::string(detail::damage_name(d.ranged_type)),
                                       d.range,
                                       d.long_range};
        }
        stats.saves = d.saves;
        stats.spell_slots = d.slots[1];
        stats.casting_bonus = d.casting;
        stats.spell_level = d.level;
        stats.spells = d.spells;
        return stats;
    }

    std::vector<TrainingChoiceGroup> training_options(const CharacterSheet &sheet) const override
    {
        // Each class's, at its own level (MULTI-1).
        const auto levels = detail::class_levels(sheet);
        std::vector<TrainingChoiceGroup> result;
        for (const auto &choice : detail::creation_choices(CreationField::character_class))
        {
            const auto klass = detail::class_from_id(choice.id);
            if (levels[klass])
            {
                for (auto &group :
                     class_training_options(sheet, klass, static_cast<unsigned>(levels[klass])))
                {
                    result.push_back(std::move(group));
                }
            }
        }
        return result;
    }

    // A class's choices a sheet's training may hold, at the class's `level`.
    static std::vector<TrainingChoiceGroup>
    class_training_options(const CharacterSheet &sheet, CharacterClass klass, unsigned level)
    {
        if (klass == CharacterClass::wizard && level >= 2)
        {
            return {detail::scholar_options(sheet.grants)};
        }
        if (klass == CharacterClass::fighter && level >= 4)
        {
            return later_masteries(klass, sheet, level);
        }
        if (klass == CharacterClass::sorcerer && level >= 2)
        {
            return {detail::metamagic_options()};
        }
        if (klass == CharacterClass::rogue && level >= 6)
        {
            return {detail::expertise_options(detail::expertise_picks[0], sheet.grants)};
        }
        if (klass == CharacterClass::ranger && level >= 9)
        {
            return {detail::expertise_options(detail::expertise_picks[1], sheet.grants)};
        }
        if (klass == CharacterClass::bard && level >= 3)
        {
            return {detail::lore_options(sheet.grants)};
        }
        if (klass == CharacterClass::druid && level >= 3)
        {
            return {detail::land_options()};
        }
        if (klass == CharacterClass::barbarian && level >= 3)
        {
            std::vector<TrainingChoiceGroup> groups{detail::primal_knowledge_options(sheet.grants)};
            for (auto &group : later_masteries(klass, sheet, level))
            {
                groups.push_back(std::move(group));
            }
            return groups;
        }
        return {};
    }

    std::vector<TrainingReplacementOptions> rest_training_options(const CharacterSheet &sheet,
                                                                  RestKind kind) const override
    {
        // Weapon Mastery, the Druid's land and the High Elf's cantrip change
        // after a Long Rest; the Hunter's Prey and Defensive Tactics after any
        // rest (SRD 5.2.1 pp. 46, 61, 85).
        std::vector<TrainingReplacementOptions> result;
        if (kind == RestKind::long_rest)
        {
            if (auto mastery = mastery_rest_choice(sheet))
            {
                result.push_back(std::move(*mastery));
            }
        }
        if (auto prey = subclass_rest_choice(sheet, detail::hunters_prey_options()))
        {
            result.push_back(std::move(*prey));
        }
        if (auto tactics = subclass_rest_choice(sheet, detail::defensive_tactics_options()))
        {
            result.push_back(std::move(*tactics));
        }
        // Fiendish Resilience's type changes after any rest too.
        if (auto resilience = subclass_rest_choice(sheet, detail::fiendish_resilience_options()))
        {
            result.push_back(std::move(*resilience));
        }
        if (kind == RestKind::long_rest)
        {
            if (auto land = subclass_rest_choice(sheet, detail::land_options()))
            {
                result.push_back(std::move(*land));
            }
            // A High Elf's Wizard cantrip changes after a Long Rest too.
            if (auto cantrip = subclass_rest_choice(sheet, detail::high_elf_cantrip_options()))
            {
                result.push_back(std::move(*cantrip));
            }
        }
        return result;
    }

    TrainingChoiceGroup hall_training_options(const CharacterSheet &sheet,
                                              std::span<const std::string> class_ids) const override
    {
        std::vector<detail::CharacterClass> classes;
        for (const auto &id : class_ids)
        {
            classes.push_back(detail::class_from_id(id));
        }
        return detail::hall_training_options(sheet.grants, classes);
    }

    void train_at_hall(CharacterSheet &sheet, std::string_view grant) const override
    {
        if (grant == bodily_health)
        {
            read_manual(sheet);
            return;
        }
        // A spell copied from a scroll, kept with the hall training so a save
        // rebuilds it (MAGIC-6).
        if (grant.starts_with(copied_spell))
        {
            const auto spell = grant.substr(copied_spell.size());
            if (!copyable_spell_level(sheet, spell))
            {
                throw std::runtime_error("That spell cannot be copied into the spellbook");
            }
            auto candidate = sheet;
            candidate.grants.push_back(detail::copied_spell_grant(
                spell, static_cast<unsigned>(detail::class_levels(sheet)[CharacterClass::wizard])));
            (void)character_profile(candidate, {});
            sheet = std::move(candidate);
            return;
        }
        auto candidate = sheet;
        // After the last skill or Expertise grant, so a save's replay, which
        // applies a level's spell choices first, rebuilds the same grant order.
        const auto last_skill =
            std::find_if(candidate.grants.rbegin(), candidate.grants.rend(),
                         [](const auto &g)
                         {
                             return g.id.starts_with("skill:") || g.id.starts_with("expertise:");
                         });
        candidate.grants.insert(last_skill.base(), FeatureGrant{std::string(grant),
                                                                std::string(detail::hall_training),
                                                                static_cast<unsigned>(sheet.level),
                                                                {}});
        candidate.training = detail::training_profile(
            candidate.grants, detail::ClassHistory(detail::level_class_ids(candidate)),
            candidate.race, detail::background_id(candidate.background), candidate.scores);
        (void)character_profile(candidate, {});
        sheet = std::move(candidate);
    }

    // The Manual of Bodily Health (MAGIC-3): Constitution and its maximum rise
    // by 2. The Hit Point maximum follows the new modifier as though the
    // character had it from level 1, as the SRD's Constitution does.
    void read_manual(CharacterSheet &sheet) const
    {
        auto candidate = sheet;
        const auto level = static_cast<unsigned>(sheet.level);
        candidate.grants.push_back(
            FeatureGrant{std::string(bodily_health), std::string(manual_source), level, {}});
        AbilityAdjustment manual{std::string(manual_source), "Manual of Bodily Health", level};
        manual.bonuses[Ability::constitution] = 2;
        manual.label_message = {"Manual of Bodily Health", {}};
        candidate.ability_adjustments.push_back(std::move(manual));
        candidate.bonuses[Ability::constitution] += 2;
        candidate.scores[Ability::constitution] += 2;
        candidate.modifiers[Ability::constitution] =
            ability_modifier(candidate.scores[Ability::constitution]);
        ++candidate.saving_throws[Ability::constitution];
        // A score 2 higher is a modifier 1 higher at every level.
        for (auto &modifier : candidate.hit_point_modifiers)
        {
            ++modifier;
        }
        candidate.hit_points =
            maximum_hit_points(detail::ClassHistory(detail::level_class_ids(candidate)),
                               candidate.race == "Dwarf", candidate.hit_point_modifiers);
        (void)character_profile(candidate, {});
        sheet = std::move(candidate);
    }

    TrainingChoices replace_rest_training(CharacterSheet &sheet,
                                          std::span<const std::string> selected) const override
    {
        // A Long Rest offers every choice a rest can change.
        const auto offered = rest_training_options(sheet, RestKind::long_rest);
        if (offered.empty())
        {
            throw std::runtime_error("No completed training choice to replace");
        }
        auto candidate = sheet;
        std::vector<std::string> unclaimed(selected.begin(), selected.end());
        TrainingChoices changed;
        for (const auto &choice : offered)
        {
            std::vector<std::string> picks;
            std::erase_if(unclaimed,
                          [&](const auto &id)
                          {
                              const bool mine = std::any_of(choice.group.options.begin(),
                                                            choice.group.options.end(),
                                                            [&](const auto &option)
                                                            {
                                                                return option.id == id;
                                                            });
                              if (mine)
                              {
                                  picks.push_back(id);
                              }
                              return mine;
                          });
            if (picks.empty())
            {
                continue;
            }
            if (choice.group.id == "weapon_mastery")
            {
                candidate.grants = detail::replace_masteries(
                    candidate.grants, detail::class_levels(candidate), picks);
            }
            else
            {
                replace_subclass_choice(candidate, choice.group.id, picks);
            }
            changed[choice.group.id] = std::move(picks);
        }
        if (!unclaimed.empty())
        {
            throw std::runtime_error("Not a choice this rest can change");
        }
        // A new land's Circle Spells are always prepared, so they leave the list.
        // Each class's at its own level (MULTI-1).
        std::vector<std::string> always;
        const auto levels = detail::class_levels(candidate);
        for (const auto held :
             detail::casting_classes(detail::ClassHistory(detail::level_class_ids(candidate))))
        {
            for (auto &id : detail::always_prepared_spells(
                     held, static_cast<unsigned>(levels[held]), candidate.grants))
            {
                always.push_back(std::move(id));
            }
        }
        std::erase_if(candidate.prepared_spells,
                      [&](const std::string &entry)
                      {
                          const auto id = entry.substr(0, entry.find(detail::prepared_tag));
                          return std::find(always.begin(), always.end(), id) != always.end();
                      });
        candidate.training = detail::training_profile(
            candidate.grants, detail::ClassHistory(detail::level_class_ids(candidate)),
            candidate.race, detail::background_id(candidate.background), candidate.scores);
        (void)character_profile(candidate, {});
        sheet = std::move(candidate);
        return changed;
    }

    AdvancementOptions advancement_options(const CharacterSheet &sheet) const override
    {
        return class_options(sheet, detail::class_of(sheet));
    }

    std::vector<AdvancementOption> advancement_classes(const CharacterSheet &sheet) const override
    {
        if (!advancement_options(sheet).level)
        {
            return {};
        }
        const auto levels = detail::class_levels(sheet);
        const auto start = detail::class_of(sheet);
        std::vector<CharacterClass> order{start};
        for (const auto held : {true, false})
        {
            for (const auto &choice : detail::creation_choices(CreationField::character_class))
            {
                const auto klass = detail::class_from_id(choice.id);
                if (klass != start && (levels[klass] > 0) == held)
                {
                    order.push_back(klass);
                }
            }
        }
        std::vector<AdvancementOption> result;
        for (const auto klass : order)
        {
            const auto options = class_options(sheet, klass);
            AdvancementOption option{std::string(detail::class_id(klass)),
                                     std::string(detail::class_label(klass)),
                                     {},
                                     options.level != 0};
            if (!option.available && !levels[klass])
            {
                option.description = multiclass_requirements(klass);
            }
            result.push_back(std::move(option));
        }
        return result;
    }

    // The class a level-up choice advances in: the one it names, or the
    // starting class.
    static CharacterClass advancing_class(const CharacterSheet &sheet,
                                          const AdvancementChoice &choice)
    {
        return choice.character_class.empty() ? detail::class_of(sheet)
                                              : detail::class_from_id(choice.character_class);
    }

    // Whether the scores meet a class's multiclass prerequisite: 13 in its
    // primary ability (Strength or Dexterity for a Fighter).
    static bool meets_requirements(const CharacterSheet &sheet, CharacterClass klass)
    {
        const auto required = detail::creation_rules().class_requirements(detail::class_id(klass));
        const auto met = [&](Ability ability)
        {
            return sheet.scores[ability] >= required.minimum;
        };
        return required.any
                   ? std::any_of(required.abilities.begin(), required.abilities.end(), met)
                   : std::all_of(required.abilities.begin(), required.abilities.end(), met);
    }

    // SRD 5.2.1 p. 24: a new class needs 13 in its primary ability and in
    // those of each class the character has (MULTI-1).
    static bool multiclass_qualified(const CharacterSheet &sheet, CharacterClass klass)
    {
        const auto levels = detail::class_levels(sheet);
        if (!meets_requirements(sheet, klass))
        {
            return false;
        }
        for (const auto &choice : detail::creation_choices(CreationField::character_class))
        {
            const auto held = detail::class_from_id(choice.id);
            if (levels[held] && !meets_requirements(sheet, held))
            {
                return false;
            }
        }
        return true;
    }

    // What a class's multiclass prerequisite asks, for a greyed-out choice.
    static std::string multiclass_requirements(CharacterClass klass)
    {
        return "Requires " +
               detail::creation_rules().class_requirements(detail::class_id(klass)).description +
               ", and 13 in the primary ability of each class the character has.";
    }

    // What the next level offers when taken in `klass`, a class the character
    // has or one it may multiclass into (MULTI-1); nothing at the cap. The
    // result's level is the character's next level, its class_level the
    // class's.
    AdvancementOptions class_options(const CharacterSheet &sheet, CharacterClass klass) const
    {
        // Each class advances to its cap (LEVELS-1), and the character's
        // levels together to the game's; class_of rejects a label no class has.
        const auto levels = detail::class_levels(sheet);
        if (static_cast<unsigned>(sheet.level) >= detail::level_cap(detail::class_of(sheet)) ||
            static_cast<unsigned>(levels[klass]) >= detail::level_cap(klass) ||
            (!levels[klass] && !multiclass_qualified(sheet, klass)))
        {
            return {};
        }
        auto result = class_level_options(sheet, klass, static_cast<unsigned>(levels[klass] + 1));
        result.class_level = result.level;
        result.level = static_cast<unsigned>(sheet.level + 1);
        result.character_class = std::string(detail::class_id(klass));
        // A class level's notes name the Proficiency Bonus its single-class
        // character reaches; a multiclass character's follows its total level.
        if (result.class_level != result.level)
        {
            for (const std::string_view note :
                 {"\nProficiency Bonus +3.", "\nProficiency Bonus +4.", "\nProficiency Bonus +5.",
                  "Proficiency Bonus +3.", "Proficiency Bonus +4.", "Proficiency Bonus +5."})
            {
                for (auto at = result.description.find(note); at != std::string::npos;
                     at = result.description.find(note))
                {
                    result.description.erase(at, note.size());
                }
            }
            if (result.level == 5 || result.level == 9 || result.level == 13)
            {
                result.description += (result.description.empty() ? "" : "\n") +
                                      std::string("Proficiency Bonus +") +
                                      std::to_string(2 + (result.level - 1) / 4) + ".";
            }
        }
        return result;
    }

    // The options of `klass`'s level `level`, before class_options says
    // which character level it is.
    AdvancementOptions class_level_options(const CharacterSheet &sheet, CharacterClass klass,
                                           unsigned level) const
    {
        AdvancementOptions result;
        result.level = level;
        // A class taken after the first: its level-one choices (MULTI-1).
        if (level == 1)
        {
            result.training = detail::multiclass_options(klass, sheet.grants, {});
            result.description = "A new class: its level-one features and part of its training.";
            return result;
        }
        if (klass == CharacterClass::fighter ||
            ((klass == CharacterClass::paladin || klass == CharacterClass::ranger) &&
             result.level == 2))
        {
            if (klass == CharacterClass::fighter)
            {
                result.fighting_styles.push_back(
                    {"keep", "Keep current", "Retain the class-granted Fighting Style."});
            }
            for (auto style : detail::fighting_styles())
            {
                style.available =
                    (klass != CharacterClass::fighter ||
                     std::any_of(sheet.grants.begin(), sheet.grants.end(),
                                 [](const auto &g)
                                 {
                                     return g.source_id == "class:fighter:fighting_style";
                                 })) &&
                    std::none_of(sheet.grants.begin(), sheet.grants.end(),
                                 [&](const auto &g)
                                 {
                                     return g.id == "feat:" + style.id;
                                 });
                result.fighting_styles.push_back(std::move(style));
            }
            // SRD 5.2.1 p. 54: the Paladin's alternative to a Fighting Style feat.
            if (klass == CharacterClass::paladin)
            {
                result.fighting_styles.push_back(
                    {"blessed_warrior", "Blessed Warrior",
                     "Learn two Cleric cantrips; Charisma is your spellcasting ability for them."});
            }
            // SRD 5.2.1 p. 59: the Ranger's alternative.
            if (klass == CharacterClass::ranger)
            {
                result.fighting_styles.push_back(
                    {"druidic_warrior", "Druidic Warrior",
                     "Learn two Druid cantrips; Wisdom is your spellcasting ability for them."});
            }
        }
        // Scholar's Expertise needs one of its skills; a character with none
        // of them, as a Monk or Rogue taking Wizard may be, has nothing to pick.
        if (auto scholar = detail::scholar_options(sheet.grants);
            klass == CharacterClass::wizard && result.level == 2 && !scholar.options.empty())
        {
            result.training = only(std::move(scholar));
        }
        if (klass == CharacterClass::fighter && (result.level == 4 || result.level == 10))
        {
            result.training =
                only(detail::mastery_options(CharacterClass::fighter, result.level, sheet.grants));
        }
        for (const auto &pick : detail::expertise_picks)
        {
            if (klass == pick.klass && result.level == pick.level)
            {
                result.training = only(detail::expertise_options(pick, sheet.grants));
            }
        }
        // The Champion's Additional Fighting Style: a Fighting Style feat it
        // does not have yet.
        if (klass == CharacterClass::fighter && result.level == 7)
        {
            TrainingChoiceGroup group{std::string(detail::champion_style),
                                      "Additional Fighting Style",
                                      1,
                                      {},
                                      TrainingChoiceControl::single_selection};
            for (const auto &style : detail::fighting_styles())
            {
                if (!detail::has_grant(sheet.grants, "feat:" + style.id))
                {
                    group.options.push_back({style.id, style.label, style.description});
                }
            }
            result.training = only(std::move(group));
        }
        if (klass == CharacterClass::barbarian && result.level == 3)
        {
            result.training = only(detail::primal_knowledge_options(sheet.grants));
        }
        if (klass == CharacterClass::sorcerer && result.level == 2)
        {
            result.training = only(detail::metamagic_options());
        }
        if (klass == CharacterClass::sorcerer && result.level == 6)
        {
            result.training = only(detail::elemental_affinity_options());
        }
        if (klass == CharacterClass::warlock && result.level == 10)
        {
            result.training = only(detail::fiendish_resilience_options());
        }
        if (klass == CharacterClass::bard && result.level == 6)
        {
            result.training = only(detail::discovery_options(
                sheet.grants, detail::prepared_spell_ids(sheet.prepared_spells)));
        }
        if (klass == CharacterClass::druid && result.level == 7)
        {
            result.training = only(detail::elemental_fury_options());
        }
        // Mystic Arcanum beside the level's invocations.
        if (klass == CharacterClass::warlock &&
            (result.level == 11 || result.level == 13 || result.level == 15))
        {
            result.training.push_back(detail::arcanum_options(static_cast<unsigned>(result.level)));
        }
        if (klass == CharacterClass::sorcerer && result.level == 10)
        {
            result.training = only(detail::metamagic_options(10, sheet.grants));
        }
        if (klass == CharacterClass::bard && result.level == 3)
        {
            result.training = only(detail::lore_options(sheet.grants));
        }
        if (klass == CharacterClass::druid && result.level == 3)
        {
            result.training = only(detail::land_options());
        }
        if (klass == CharacterClass::warlock && result.level >= 2 &&
            detail::new_invocations(static_cast<unsigned>(result.level)))
        {
            result.training.push_back(detail::invocation_options(
                static_cast<unsigned>(result.level), sheet.grants,
                detail::invocations_source(static_cast<unsigned>(result.level))));
        }
        if (klass == CharacterClass::barbarian && (result.level == 4 || result.level == 10))
        {
            result.training = only(
                detail::mastery_options(CharacterClass::barbarian, result.level, sheet.grants));
        }
        if (klass == CharacterClass::ranger && result.level == 7)
        {
            result.training = only(detail::defensive_tactics_options());
        }
        if (klass == CharacterClass::cleric && result.level == 7)
        {
            result.training = only(detail::blessed_strikes_options());
        }
        // The Hunter is the SRD's only Ranger subclass; Hunter's Prey is its choice.
        if (klass == CharacterClass::ranger && result.level == 3)
        {
            result.training = only({"subclass:ranger:hunter", "Hunter's Prey", 1,
                                    detail::hunters_prey_options().options,
                                    TrainingChoiceControl::single_selection});
        }
        result.description =
            "Fixed-average HP growth. Resources gain only their new capacity;\nexisting expenditure remains.";
        if (klass == CharacterClass::cleric && result.level == 2)
        {
            result.description =
                "Channel Divinity: two uses, one back on a Short Rest, for Divine Spark (heal or harm 1d8 + Wisdom within 30 feet) or Turn Undead.";
        }
        if (klass == CharacterClass::wizard && result.level == 3)
        {
            result.description =
                "Evoker: Potent Cantrip deals half damage when a cantrip misses or is saved against.";
        }
        if (klass == CharacterClass::wizard && result.level == 5)
        {
            result.description =
                "Level 3 spell slots and Wizard spells: Animate Dead, Bestow Curse, Blink, Counterspell, Dispel Magic, Fear, Fireball, Haste, Hypnotic Pattern, Lightning Bolt, Magic Circle, Protection from Energy, Remove Curse, Sleet Storm, Slow, Stinking Cloud, Tiny Hut and Vampiric Touch.\nMemorize Spell: after a Short Rest, change one prepared spell for another in your spellbook.\nProficiency Bonus +3.";
        }
        if (klass == CharacterClass::wizard && result.level == 6)
        {
            result.description =
                "Sculpt Spells: up to 1 + the spell's level allies you can see in your Evocation areas are spared.";
        }
        if (klass == CharacterClass::wizard && result.level == 7)
        {
            result.description =
                "A level 4 spell slot and Wizard spells: Banishment, Black Tentacles, Blight, Charm Monster, Confusion, Dimension Door, Fire Shield, Greater Invisibility, Ice Storm, Phantasmal Killer, Polymorph, Resilient Sphere, Stoneskin, Vitriolic Sphere and Wall of Fire.";
        }
        if (klass == CharacterClass::wizard && result.level == 8)
        {
            result.description = "A feat or ability points.";
        }
        if (klass == CharacterClass::wizard && result.level == 9)
        {
            result.description =
                "A level 5 spell slot and Wizard spells: Animate Objects, Arcane Hand, Cloudkill, Cone of Cold, Conjure Elemental, Dominate Person, Hold Monster, Mislead, Summon Dragon, Telekinesis, Wall of Force and Wall of Stone.\nProficiency Bonus +4.";
        }
        if (klass == CharacterClass::wizard && result.level == 11)
        {
            result.description =
                "A level 6 spell slot and Wizard spells: Chain Lightning, Circle of Death, Contingency, Create Undead, Disintegrate, Eyebite, Flesh to Stone, Freezing Sphere, Globe of Invulnerability, Irresistible Dance, Magic Jar, Sunbeam, True Seeing and Wall of Ice.";
        }
        if (klass == CharacterClass::wizard && result.level == 12)
        {
            result.description = "A feat or ability points.";
        }
        if (klass == CharacterClass::warlock && result.level == 5)
        {
            result.description =
                "Two more Eldritch Invocations; Eldritch Smite and Thirsting Blade are open to Pact of the Blade.\nLevel 3 Pact Magic slots and Warlock spells: Counterspell, Dispel Magic, Fear, Hypnotic Pattern, Magic Circle, Remove Curse and Vampiric Touch; Fiend Spells: Fireball and Stinking Cloud are always prepared.\nProficiency Bonus +3.";
        }
        if (klass == CharacterClass::warlock && result.level == 6)
        {
            result.description =
                "Dark One's Own Luck: a failed save or ability check you could still pass gains 1d10, as many times as your Charisma modifier (at least once) per Long Rest.";
        }
        if (klass == CharacterClass::warlock && result.level == 7)
        {
            result.description =
                "One more Eldritch Invocation.\nLevel 4 Pact Magic slots and Warlock spells: Banishment, Blight, Charm Monster and Dimension Door; Fiend Spells: Fire Shield and Wall of Fire are always prepared.";
        }
        if (klass == CharacterClass::warlock && result.level == 8)
        {
            result.description = "A feat or ability points.";
        }
        if (klass == CharacterClass::warlock && result.level == 9)
        {
            result.description =
                "One more Eldritch Invocation; Lifedrinker is open to Pact of the Blade, Gift of the Protectors to Pact of the Tome.\nLevel 5 Pact Magic slots and Warlock spells: Hold Monster and Mislead; Fiend Spells: Insect Plague is always prepared.\nProficiency Bonus +4.";
        }
        if (klass == CharacterClass::warlock && result.level == 10)
        {
            result.description =
                "Fiendish Resilience: Resistance to a damage type you choose other than Force, changed after any rest.\nA fourth cantrip.";
        }
        if (klass == CharacterClass::druid && result.level == 5)
        {
            result.description =
                "Wild Resurgence: with no Wild Shape use left, a spell slot (no action, once a turn) gives one; once per Long Rest, a Wild Shape use gives a level 1 spell slot.\nLevel 3 spell slots and Druid spells: Call Lightning, Conjure Animals, Dispel Magic, Plant Growth, Protection from Energy, Revivify, Sleet Storm and Wind Wall; your land's next Circle Spell.\nProficiency Bonus +3.";
        }
        if (klass == CharacterClass::druid && result.level == 6)
        {
            result.description =
                "Natural Recovery: once per Long Rest, cast one of your levelled Circle Spells without a slot; after a Short Rest, recover spell slots of levels totalling half your Druid level.\nA third Wild Shape use.";
        }
        if (klass == CharacterClass::druid && result.level == 7)
        {
            result.description =
                "Elemental Fury: Potent Spellcasting (your Wisdom modifier added to Druid cantrip damage) or Primal Strike (once a turn, 1d8 Cold, Fire, Lightning or Thunder on a weapon or Beast form hit).\nA level 4 spell slot and Druid spells: Blight, Charm Monster, Confusion, Conjure Minor Elementals, Conjure Woodland Beings, Dominate Beast, Fire Shield, Freedom of Movement, Giant Insect, Ice Storm, Polymorph, Stoneskin and Wall of Fire; your land's next Circle Spell.";
        }
        if (klass == CharacterClass::druid && result.level == 8)
        {
            result.description = "A feat or ability points.\nWild Shape: the Tiger.";
        }
        if (klass == CharacterClass::druid && result.level == 9)
        {
            result.description =
                "A level 5 spell slot and Druid spells: Antilife Shell, Cone of Cold, Conjure Elemental, Contagion, Greater Restoration, Insect Plague, Mass Cure Wounds, Reincarnate and Wall of Stone; your land's next Circle Spell.\nProficiency Bonus +4.";
        }
        if (klass == CharacterClass::druid && result.level == 10)
        {
            result.description =
                "Nature's Ward: you cannot be Poisoned, and you resist your land's element: Fire (arid), Cold (polar), Lightning (temperate) or Poison (tropical).\nLand's Aid deals and heals 3d6. A fourth cantrip.";
        }
        if (klass == CharacterClass::druid && result.level == 11)
        {
            result.description =
                "A level 6 spell slot and Druid spells: Conjure Fey, Flesh to Stone, Heal, Heroes' Feast, Sunbeam and Wall of Thorns.";
        }
        if (klass == CharacterClass::druid && result.level == 12)
        {
            result.description = "A feat or ability points.";
        }
        if (klass == CharacterClass::druid && result.level == 13)
        {
            result.description =
                "A level 7 spell slot and Druid spells: Fire Storm, Regenerate and Reverse Gravity.\nProficiency Bonus +5.";
        }
        if (klass == CharacterClass::druid && result.level == 14)
        {
            result.description =
                "Nature's Sanctuary: a Magic action and a Wild Shape use raise a 15-foot cube of spectral trees for a minute; allies among them share your Nature's Ward Resistance, and a Bonus Action moves it 60 feet.\nLand's Aid deals and heals 4d6.";
        }
        if (klass == CharacterClass::druid && result.level == 15)
        {
            result.description =
                "A level 8 spell slot and Druid spells: Befuddlement, Earthquake, Incendiary Cloud, Sunburst and Tsunami.\nImproved Elemental Fury: Primal Strike deals 2d8, or Potent Spellcasting's Druid cantrips reach 300 feet farther.";
        }
        if (klass == CharacterClass::bard && result.level == 5)
        {
            result.description =
                "Font of Inspiration: Bardic Inspiration returns on a Short Rest, and a spell slot (no action) gives back a use. The Bardic Inspiration die is a d8.\nLevel 3 spell slots and Bard spells: Bestow Curse, Dispel Magic, Fear, Hypnotic Pattern, Mass Healing Word, Plant Growth, Slow, Stinking Cloud and Tiny Hut.\nProficiency Bonus +3.";
        }
        if (klass == CharacterClass::bard && result.level == 6)
        {
            result.description =
                "Magical Discoveries: two Cleric, Druid or Wizard spells, cantrips or of level 1 to 3, always prepared.";
        }
        if (klass == CharacterClass::bard && result.level == 7)
        {
            result.description =
                "Countercharm: when you or a creature within 30 feet fails a save against being Charmed or Frightened, your Reaction rerolls it with Advantage.\nA level 4 spell slot and Bard spells: Charm Monster, Confusion, Dimension Door, Freedom of Movement, Greater Invisibility, Phantasmal Killer and Polymorph.";
        }
        if (klass == CharacterClass::bard && (result.level == 8 || result.level == 12))
        {
            result.description = "A feat or ability points.";
        }
        if (klass == CharacterClass::bard && result.level == 9)
        {
            result.description =
                "Expertise in two more skills.\nA level 5 spell slot and Bard spells: Animate Objects, Dominate Person, Greater Restoration, Hold Monster, Mass Cure Wounds, Mislead and Raise Dead.\nProficiency Bonus +4.";
        }
        if (klass == CharacterClass::bard && result.level == 10)
        {
            result.description =
                "Magical Secrets: your prepared spells may also come from the Cleric, Druid and Wizard lists.\nThe Bardic Inspiration die is a d10. A fourth cantrip.";
        }
        if (klass == CharacterClass::bard && result.level == 11)
        {
            result.description =
                "A level 6 spell slot and Bard spells: Eyebite, Heroes' Feast, Irresistible Dance and True Seeing.";
        }
        if (klass == CharacterClass::bard && result.level == 13)
        {
            result.description =
                "A level 7 spell slot and Bard spells: Arcane Sword, Forcecage, Prismatic Spray, Regenerate and Resurrection.\nProficiency Bonus +5.";
        }
        if (klass == CharacterClass::bard && result.level == 14)
        {
            result.description =
                "Peerless Skill: when your own ability check or attack roll fails, the Bardic Inspiration die is added when it could fix the roll, spending a use only if it does.";
        }
        if (klass == CharacterClass::bard && result.level == 15)
        {
            result.description =
                "A level 8 spell slot and Bard spells: Befuddlement, Dominate Monster, Mind Blank and Power Word Stun.\nThe Bardic Inspiration die is a d12.";
        }
        if (klass == CharacterClass::warlock && result.level == 11)
        {
            result.description =
                "Mystic Arcanum: choose a level 6 Warlock spell to cast once per Long Rest without a spell slot.\nA third Pact Magic slot.";
        }
        if (klass == CharacterClass::warlock && result.level == 12)
        {
            result.description =
                "A feat or ability points.\nOne more Eldritch Invocation; Devouring Blade is open to Thirsting Blade.";
        }
        if (klass == CharacterClass::warlock && result.level == 13)
        {
            result.description =
                "Mystic Arcanum: choose a level 7 Warlock spell to cast once per Long Rest without a spell slot.\nProficiency Bonus +5.";
        }
        if (klass == CharacterClass::warlock && result.level == 14)
        {
            result.description =
                "Hurl Through Hell: once per turn, after an attack roll hits, the creature makes a Charisma save or is gone until the end of your next turn, Incapacitated, taking 8d10 Psychic damage unless it is a Fiend. Once per Long Rest, or for a Pact Magic slot.";
        }
        if (klass == CharacterClass::warlock && result.level == 15)
        {
            result.description =
                "Mystic Arcanum: choose a level 8 Warlock spell to cast once per Long Rest without a spell slot.\nOne more Eldritch Invocation; Witch Sight is open.";
        }
        if (klass == CharacterClass::sorcerer && result.level == 5)
        {
            result.description =
                "Sorcerous Restoration: after a Short Rest, regain Sorcery Points up to half your Sorcerer level, once per Long Rest.\nLevel 3 spell slots and Sorcerer spells: Blink, Counterspell, Dispel Magic, Fear, Fireball, Haste, Hypnotic Pattern, Lightning Bolt, Protection from Energy, Sleet Storm, Slow, Stinking Cloud and Vampiric Touch; Draconic Spells: Fear is always prepared. Font of Magic creates level-3 slots.\nProficiency Bonus +3.";
        }
        if (klass == CharacterClass::sorcerer && result.level == 6)
        {
            result.description =
                "Elemental Affinity: choose Acid, Cold, Fire, Lightning or Poison: Resistance to it, and your Charisma modifier added to the damage of your spells of that type.";
        }
        if (klass == CharacterClass::sorcerer && result.level == 7)
        {
            result.description =
                "Sorcery Incarnate: with no Innate Sorcery use left, 2 Sorcery Points start it; while it is active, two Metamagic options can change one spell.\nA level 4 spell slot and Sorcerer spells: Banishment, Blight, Charm Monster, Confusion, Dimension Door, Dominate Beast, Fire Shield, Greater Invisibility, Ice Storm, Polymorph, Stoneskin, Vitriolic Sphere and Wall of Fire; Draconic Spells: Charm Monster is always prepared. Font of Magic creates level-4 slots.";
        }
        if (klass == CharacterClass::sorcerer && (result.level == 8 || result.level == 12))
        {
            result.description = "A feat or ability points.";
        }
        if (klass == CharacterClass::sorcerer && result.level == 9)
        {
            result.description =
                "A level 5 spell slot and Sorcerer spells: Animate Objects, Arcane Hand, Cloudkill, Cone of Cold, Dominate Person, Hold Monster, Insect Plague, Telekinesis and Wall of Stone; Draconic Spells: Summon Dragon is always prepared. Font of Magic creates level-5 slots.\nProficiency Bonus +4.";
        }
        if (klass == CharacterClass::sorcerer && result.level == 10)
        {
            result.description = "Two more Metamagic options.\nA sixth cantrip.";
        }
        if (klass == CharacterClass::sorcerer && result.level == 11)
        {
            result.description =
                "A level 6 spell slot and Sorcerer spells: Chain Lightning, Circle of Death, Disintegrate, Eyebite, Flesh to Stone, Freezing Sphere, Globe of Invulnerability, Sunbeam and True Seeing.";
        }
        if (klass == CharacterClass::sorcerer && result.level == 13)
        {
            result.description =
                "A level 7 spell slot and Sorcerer spells: Delayed Blast Fireball, Finger of Death, Fire Storm, Prismatic Spray and Reverse Gravity.";
        }
        if (klass == CharacterClass::sorcerer && result.level == 14)
        {
            result.description =
                "Dragon Wings: a Bonus Action spreads wings for an hour: you ignore Difficult Terrain and may move through enemies' squares. Once per Long Rest, or for 3 Sorcery Points.";
        }
        if (klass == CharacterClass::sorcerer && result.level == 15)
        {
            result.description =
                "A level 8 spell slot and Sorcerer spells: Dominate Monster, Earthquake, Incendiary Cloud, Power Word Stun and Sunburst.";
        }
        if (klass == CharacterClass::wizard && result.level == 13)
        {
            result.description =
                "A level 7 spell slot and Wizard spells: Arcane Sword, Delayed Blast Fireball, Finger of Death, Forcecage, Prismatic Spray, Reverse Gravity and Simulacrum.";
        }
        if (klass == CharacterClass::wizard && result.level == 14)
        {
            result.description =
                "Overchannel: a damaging Wizard spell cast with a level 1-5 slot deals its maximum damage. Free once per Long Rest; each later use deals you Necrotic damage, more each time.";
        }
        if (klass == CharacterClass::wizard && result.level == 15)
        {
            result.description =
                "A level 8 spell slot and Wizard spells: Antimagic Field, Befuddlement, Dominate Monster, Incendiary Cloud, Maze, Mind Blank, Power Word Stun and Sunburst.";
        }
        if (klass == CharacterClass::wizard && result.level == 10)
        {
            result.description =
                "Empowered Evocation: your Intelligence modifier is added to the damage of your Wizard Evocation spells.\nA fifth cantrip.";
        }
        if (klass == CharacterClass::cleric && result.level == 5)
        {
            result.description =
                "Sear Undead: Turn Undead also deals Radiant damage, a d8 for each point of your Wisdom modifier, to each Undead that fails.\nLevel 3 spell slots and Cleric spells: Animate Dead, Beacon of Hope, Bestow Curse, Dispel Magic, Magic Circle, Protection from Energy, Remove Curse and Spirit Guardians; Life Domain: Mass Healing Word and Revivify are always prepared.\nProficiency Bonus +3.";
        }
        if (klass == CharacterClass::cleric && result.level == 7)
        {
            result.description =
                "Blessed Strikes: Divine Strike (once on each of your turns a weapon hit deals 1d8 more Radiant or Necrotic damage) or Potent Spellcasting (your Wisdom modifier added to your Cleric cantrips' damage).\nDivine Spark rolls 2d8. A level 4 spell slot and Cleric spells: Aura of Life, Banishment, Death Ward, Freedom of Movement and Guardian of Faith; Life Domain: Aura of Life and Death Ward are always prepared.";
        }
        if (klass == CharacterClass::cleric && result.level == 8)
        {
            result.description = "A feat or ability points.";
        }
        if (klass == CharacterClass::cleric && result.level == 9)
        {
            result.description =
                "Level 5 spell slots and Cleric spells: Contagion, Dispel Evil and Good, Flame Strike, Insect Plague and Raise Dead; Life Domain: Greater Restoration and Mass Cure Wounds are always prepared.\nProficiency Bonus +4.";
        }
        if (klass == CharacterClass::cleric && result.level == 11)
        {
            result.description =
                "A level 6 spell slot and Cleric spells: Blade Barrier, Create Undead, Harm, Heal, Heroes' Feast, Sunbeam, True Seeing and Word of Recall.\nCantrips deal more damage.";
        }
        if (klass == CharacterClass::cleric && result.level == 12)
        {
            result.description = "A feat or ability points.";
        }
        if (klass == CharacterClass::cleric && result.level == 13)
        {
            result.description =
                "A level 7 spell slot and Cleric spells: Conjure Celestial, Divine Word, Fire Storm, Regenerate and Resurrection.\nDivine Spark rolls 3d8.";
        }
        if (klass == CharacterClass::cleric && result.level == 14)
        {
            result.description =
                "Improved Blessed Strikes: Divine Strike deals 2d8; Potent Spellcasting also lets a Cleric cantrip that deals damage give you or a creature within 60 feet Temporary Hit Points equal to twice your Wisdom modifier.";
        }
        if (klass == CharacterClass::cleric && result.level == 15)
        {
            result.description =
                "A level 8 spell slot and Cleric spells: Antimagic Field, Earthquake, Holy Aura and Sunburst.";
        }
        if (klass == CharacterClass::cleric && result.level == 10)
        {
            result.description =
                "Divine Intervention: once per Long Rest, a Magic action casts any Cleric spell of level 5 or lower without a spell slot.\nA fifth cantrip.";
        }
        if (klass == CharacterClass::cleric && result.level == 6)
        {
            result.description =
                "Life Domain: Blessed Healer. When a spell slot's healing restores Hit Points to another creature, you regain 2 + the slot's level.\nA third Channel Divinity use.";
        }
        if (klass == CharacterClass::cleric && result.level == 3)
        {
            result.description =
                "Life Domain: Disciple of Life adds 2 + the slot level to healing spells; Preserve Life (Channel Divinity) restores five times your level among Bloodied allies within 30 feet; Bless, Cure Wounds and Lesser Restoration are always prepared.";
        }
        if (klass == CharacterClass::fighter && result.level == 3)
        {
            result.description =
                "Champion: weapon/unarmed criticals on 19–20.\nAdvantage on Initiative and Strength (Athletics).\nCritical hit: optional half-Speed move, no opportunity attacks.";
        }
        if (klass == CharacterClass::barbarian && result.level == 5)
        {
            result.description =
                "Extra Attack: two attacks with each Attack action.\nFast Movement: Speed +10 feet without Heavy armor.\nProficiency Bonus +3.";
        }
        if (klass == CharacterClass::barbarian && result.level == 6)
        {
            result.description =
                "Berserker: Mindless Rage. While raging you cannot be Charmed or Frightened, and entering a Rage ends both.\nA fourth Rage.";
        }
        if (klass == CharacterClass::barbarian && result.level == 7)
        {
            result.description =
                "Feral Instinct: Advantage on Initiative.\nInstinctive Pounce: move up to half your Speed as part of entering a Rage.";
        }
        if (klass == CharacterClass::barbarian && result.level == 9)
        {
            result.description =
                "Brutal Strike: after Reckless Attack, one Strength attack a turn may give up its Advantage; on a hit it deals 1d10 more damage and a Forceful Blow (a 15-foot push you may follow) or a Hamstring Blow (Speed 15 feet lower).\nRage Damage +3; Proficiency Bonus +4.";
        }
        if (klass == CharacterClass::barbarian && result.level == 10)
        {
            result.description =
                "Berserker: Retaliation. When a creature within 5 feet damages you, your Reaction makes a melee attack against it.\nA fourth Weapon Mastery.";
        }
        if (klass == CharacterClass::barbarian && result.level == 11)
        {
            result.description =
                "Relentless Rage: dropping to 0 Hit Points while raging without dying, a Constitution save (DC 10, 5 higher for each try since your last rest) leaves you twice your Barbarian level in Hit Points.";
        }
        if (klass == CharacterClass::barbarian && result.level == 12)
        {
            result.description = "A feat or ability points.\nA fifth Rage.";
        }
        if (klass == CharacterClass::barbarian && result.level == 13)
        {
            result.description =
                "Improved Brutal Strike: two more blows. Staggering Blow: Disadvantage on the target's next saving throw, and no Opportunity Attacks. Sundering Blow: +5 to the next attack roll another creature makes against it.\nProficiency Bonus +5.";
        }
        if (klass == CharacterClass::barbarian && result.level == 14)
        {
            result.description =
                "Berserker: Intimidating Presence. A Bonus Action: each enemy within 30 feet makes a Wisdom save or is Frightened for up to a minute. Once per Long Rest, or for a Rage use.";
        }
        if (klass == CharacterClass::barbarian && result.level == 15)
        {
            result.description =
                "Persistent Rage: when Initiative is rolled, regain every Rage use, once per Long Rest. Your Rage needs no extending.";
        }
        if (klass == CharacterClass::rogue && result.level == 5)
        {
            result.description =
                "Cunning Strike: when an attack would deal Sneak Attack damage, give up a die to Poison the target if you carry a Poisoner's Kit (Constitution save or Poisoned), Trip it (Dexterity save or Prone) or Withdraw (half your Speed without provoking).\nUncanny Dodge: your Reaction halves the damage of a hit.\nSneak Attack 3d6; Proficiency Bonus +3.";
        }
        if (klass == CharacterClass::rogue && result.level == 6)
        {
            result.description = "Expertise in two more skills.";
        }
        if (klass == CharacterClass::rogue && result.level == 7)
        {
            result.description =
                "Evasion: a Dexterity save against half damage takes none on a success and half on a failure.\nReliable Talent: a d20 below 10 on a proficient skill check counts as 10.\nSneak Attack 4d6.";
        }
        if (klass == CharacterClass::rogue && result.level == 9)
        {
            result.description =
                "Thief: Supreme Sneak. A hidden Rogue's Stealth Attack (a Cunning Strike) leaves it hidden.\nSneak Attack 5d6; Proficiency Bonus +4.";
        }
        if (klass == CharacterClass::rogue && (result.level == 10 || result.level == 12))
        {
            result.description = "A feat or ability points.";
        }
        if (klass == CharacterClass::rogue && result.level == 11)
        {
            result.description =
                "Improved Cunning Strike: one Sneak Attack may carry two Cunning Strike effects, paying for both.\nSneak Attack 6d6.";
        }
        if (klass == CharacterClass::rogue && result.level == 13)
        {
            result.description =
                "Thief: Use Magic Device, a 6 on 1d6 to keep a spent charge, and any scroll read with Intelligence.\nSneak Attack 7d6; Proficiency Bonus +5.";
        }
        if (klass == CharacterClass::rogue && result.level == 14)
        {
            result.description =
                "Devious Strikes: three more Cunning Strike effects. Daze (2 dice): a Constitution save or only one of moving, an action or a Bonus Action on its next turn. Knock Out (6 dice): a Constitution save or Unconscious for up to a minute, until damaged or shaken awake. Obscure (3 dice): a Dexterity save or Blinded until the end of its next turn.";
        }
        if (klass == CharacterClass::rogue && result.level == 15)
        {
            result.description =
                "Slippery Mind: proficiency in Wisdom and Charisma saving throws.\nSneak Attack 8d6.";
        }
        if (klass == CharacterClass::monk && result.level == 5)
        {
            result.description =
                "Extra Attack: two attacks with each Attack action.\nStunning Strike: once a turn, a hit with a Monk weapon or an Unarmed Strike may spend 1 Focus: Constitution save or Stunned until your next turn.\nMartial Arts d8; Proficiency Bonus +3.";
        }
        if (klass == CharacterClass::monk && result.level == 6)
        {
            result.description =
                "Empowered Strikes: Unarmed Strikes deal Force damage.\nOpen Hand: Wholeness of Body, a Bonus Action that heals the Martial Arts die + Wisdom, Wisdom modifier times per Long Rest.\nUnarmored Movement +15 feet.";
        }
        if (klass == CharacterClass::monk && result.level == 7)
        {
            result.description =
                "Evasion: a Dexterity save against half damage takes none on a success and half on a failure.";
        }
        if (klass == CharacterClass::monk && result.level == 9)
        {
            result.description = "Proficiency Bonus +4.";
        }
        if (klass == CharacterClass::monk && result.level == 10)
        {
            result.description =
                "Heightened Focus: Flurry of Blows makes three strikes, and Patient Defense with Focus gives two Martial Arts dice of Temporary Hit Points.\nSelf-Restoration: at the end of each of your turns, Charmed, Frightened or Poisoned ends.\nUnarmored Movement +20 feet.";
        }
        if (klass == CharacterClass::monk && result.level == 11)
        {
            result.description =
                "Open Hand: Fleet Step. After another Bonus Action, you may also use Step of the Wind.\nMartial Arts d10.";
        }
        if (klass == CharacterClass::monk && result.level == 13)
        {
            result.description =
                "Deflect Energy: Deflect Attacks works against any damage type.\nProficiency Bonus +5.";
        }
        if (klass == CharacterClass::monk && result.level == 14)
        {
            result.description =
                "Disciplined Survivor: proficiency in every saving throw.\nUnarmored Movement +25 feet.";
        }
        if (klass == CharacterClass::monk && result.level == 15)
        {
            result.description =
                "Perfect Focus: you start each fight with at least 4 Focus Points.";
        }
        if (klass == CharacterClass::fighter && result.level == 5)
        {
            result.description =
                "Extra Attack: two attacks with each Attack action.\nTactical Shift: after Second Wind, move up to half Speed without provoking Opportunity Attacks.\nProficiency Bonus +3.";
        }
        if (klass == CharacterClass::fighter && result.level == 7)
        {
            result.description = "Champion: a second Fighting Style feat.";
        }
        if (klass == CharacterClass::fighter && result.level == 9)
        {
            result.description =
                "Indomitable: once per Long Rest, reroll a failed saving throw, adding your Fighter level.\nTactical Master: an attack may use Push, Sap or Slow instead of its weapon's mastery.\nProficiency Bonus +4.";
        }
        if (klass == CharacterClass::fighter && result.level == 10)
        {
            result.description =
                "Champion: Heroic Inspiration at the start of each of your turns in a fight, when you lack it.\nA fifth Weapon Mastery, and a fourth Second Wind.";
        }
        if (klass == CharacterClass::fighter && result.level == 11)
        {
            result.description = "Two Extra Attacks: three attacks with each Attack action.";
        }
        if (klass == CharacterClass::fighter && result.level == 13)
        {
            result.description =
                "Indomitable: two uses per Long Rest.\nStudied Attacks: after you miss a creature, your next attack roll against it before the end of your next turn has Advantage.\nProficiency Bonus +5.";
        }
        if (klass == CharacterClass::fighter && result.level == 15)
        {
            result.description =
                "Champion: Superior Critical. Weapon and Unarmed Strike criticals on 18–20.";
        }
        if (klass == CharacterClass::rogue && (result.level == 3 || result.level == 4))
        {
            result.description =
                "Sneak Attack: 2d6. Steady Aim: Bonus Action; next attack roll has Advantage, Speed becomes 0.\nThief: Fast Hands uses a wand, potion or other magic item as a Bonus Action.";
        }
        if (klass == CharacterClass::paladin && result.level <= 4)
        {
            result.description =
                "Prepared spells, Lay On Hands and fixed HP advancement; Fighting Style or Blessed Warrior and Paladin's Smite at level two; Channel Divinity, the Oath of Devotion and Sacred Weapon at level three. Level four grants an available feat or ability points.";
        }
        if (klass == CharacterClass::ranger && result.level == 5)
        {
            result.description =
                "Extra Attack: two attacks with each Attack action.\nLevel 2 spell slots and Ranger spells; Ensnaring Strike from a level 2 slot deals 2d6 a turn.\nA third free Hunter's Mark (Favored Enemy); Proficiency Bonus +3.";
        }
        if (klass == CharacterClass::ranger && result.level == 6)
        {
            result.description = "Roving: Speed +10 feet without Heavy armor.";
        }
        if (klass == CharacterClass::ranger && result.level == 7)
        {
            result.description =
                "Hunter: Defensive Tactics. Escape the Horde gives Opportunity Attacks against you Disadvantage; Multiattack Defense gives a creature that hits you Disadvantage on its other attacks against you this turn. You can switch after a rest.\nA third level 2 spell slot.";
        }
        if (klass == CharacterClass::ranger && result.level == 8)
        {
            result.description = "A feat or ability points.";
        }
        if (klass == CharacterClass::ranger && result.level == 9)
        {
            result.description =
                "Expertise in two more skills.\nLevel 3 spell slots and Ranger spells: Conjure Animals, Dispel Magic, Plant Growth, Protection from Energy, Revivify and Wind Wall.\nA fourth free Hunter's Mark (Favored Enemy); Proficiency Bonus +4.";
        }
        if (klass == CharacterClass::ranger && result.level == 10)
        {
            result.description =
                "Tireless: a Magic action gives you 1d8 + your Wisdom modifier in Temporary Hit Points, as many times as your Wisdom modifier per Long Rest.";
        }
        if (klass == CharacterClass::ranger && result.level == 11)
        {
            result.description =
                "Hunter: Superior Hunter's Prey. Once a turn, after you damage the creature your Hunter's Mark marks, the mark's damage can also strike another creature within 30 feet of it.\nA second level 3 spell slot.";
        }
        if (klass == CharacterClass::ranger && result.level == 12)
        {
            result.description = "A feat or ability points.";
        }
        if (klass == CharacterClass::ranger && result.level == 13)
        {
            result.description =
                "Relentless Hunter: taking damage cannot break your Concentration on Hunter's Mark.\nA level 4 spell slot and Ranger spells: Conjure Woodland Beings, Dominate Beast, Freedom of Movement and Stoneskin.\nA fifth free Hunter's Mark (Favored Enemy); Proficiency Bonus +5.";
        }
        if (klass == CharacterClass::ranger && result.level == 14)
        {
            result.description =
                "Nature's Veil: a Bonus Action makes you Invisible until the end of your next turn, as many times as your Wisdom modifier per Long Rest.";
        }
        if (klass == CharacterClass::ranger && result.level == 15)
        {
            result.description =
                "Hunter: Superior Hunter's Defense. When an attack hits you, your Reaction gives you Resistance to its damage type for the rest of the turn.\nA second level 4 spell slot.";
        }
        if (klass == CharacterClass::paladin && result.level == 5)
        {
            result.description =
                "Extra Attack: two attacks with each Attack action.\nLevel 2 spell slots and Paladin spells, among them Shining Smite; Divine Smite from a level 2 slot deals 3d8. Aid is always prepared.\nProficiency Bonus +3.";
        }
        if (klass == CharacterClass::paladin && result.level == 6)
        {
            result.description =
                "Aura of Protection: you and allies within 10 feet add your Charisma modifier (at least +1) to saving throws, unless you are Incapacitated.";
        }
        if (klass == CharacterClass::paladin && result.level == 7)
        {
            result.description =
                "Oath of Devotion: Aura of Devotion. You and allies in your Aura of Protection cannot be Charmed.\nA third level 2 spell slot.";
        }
        if (klass == CharacterClass::paladin && result.level == 8)
        {
            result.description = "A feat or ability points.";
        }
        if (klass == CharacterClass::paladin && result.level == 9)
        {
            result.description =
                "Abjure Foes: a Magic action and a Channel Divinity use; up to your Charisma modifier (at least 1) of creatures you see within 60 feet make a Wisdom save or are Frightened for a minute or until damaged, doing only one of moving, an action or a Bonus Action on their turns.\nLevel 3 spell slots and Paladin spells; Oath of Devotion: Beacon of Hope and Dispel Magic are always prepared.\nProficiency Bonus +4.";
        }
        if (klass == CharacterClass::paladin && result.level == 10)
        {
            result.description =
                "Aura of Courage: you and allies in your Aura of Protection cannot be Frightened.";
        }
        if (klass == CharacterClass::paladin && result.level == 11)
        {
            result.description =
                "Radiant Strikes: your hits with a Melee weapon or an Unarmed Strike deal an extra 1d8 Radiant damage.\nA third Channel Divinity use and a third level 3 spell slot.";
        }
        if (klass == CharacterClass::paladin && result.level == 12)
        {
            result.description = "A feat or ability points.";
        }
        if (klass == CharacterClass::paladin && result.level == 13)
        {
            result.description =
                "A level 4 spell slot and Paladin spells: Aura of Life, Banishment and Death Ward. Oath of Devotion: Freedom of Movement and Guardian of Faith are always prepared.\nProficiency Bonus +5.";
        }
        if (klass == CharacterClass::paladin && result.level == 14)
        {
            result.description =
                "Restoring Touch: Lay On Hands can also end Blinded, Charmed, Frightened, Paralyzed or Stunned, 5 points from the pool each.";
        }
        if (klass == CharacterClass::paladin && result.level == 15)
        {
            result.description =
                "Oath of Devotion: Smite of Protection. When you cast Divine Smite, you and allies in your Aura of Protection gain +2 AC and +2 to Dexterity saves until your next turn.\nA second level 4 spell slot.";
        }
        if (klass == CharacterClass::druid && result.level <= 4)
        {
            result.description =
                "Prepared Druid spells and a Primal Order: Magician (an extra cantrip) or Warden (Martial weapons and Medium armor); the Circle of the Land at level three with its land's Circle Spells always prepared. Level four grants a third cantrip and an available feat or ability points.";
        }
        if (klass == CharacterClass::bard && result.level <= 4)
        {
            result.description =
                "Bardic spellcasting with prepared Bard spells; Jack of All Trades at level two; the College of Lore at level three with three more skills. Level four grants a third cantrip and an available feat or ability points.";
        }
        if (klass == CharacterClass::warlock && result.level <= 4)
        {
            result.description =
                "Pact Magic: prepared Warlock spells cast from slots of one level that return on a Short Rest; Magical Cunning at level two; the Fiend Patron at level three: Dark One's Blessing and Burning Hands, Command and Scorching Ray always prepared. Level four grants a third cantrip and an available feat or ability points.";
        }
        if (klass == CharacterClass::sorcerer && result.level <= 4)
        {
            result.description =
                "Prepared Sorcerer spells and Innate Sorcery; Font of Magic's Sorcery Points at level two; Draconic Sorcery at level three: Draconic Resilience (Hit Points and unarmored AC 10 + Dexterity + Charisma) and Chromatic Orb, Command and Dragon's Breath always prepared. Level four grants a fifth cantrip and an available feat or ability points.";
        }
        if (klass == CharacterClass::monk && result.level <= 4)
        {
            result.description =
                "Martial Arts and Unarmored Defense; Monk's Focus (Flurry of Blows, Patient Defense, Step of the Wind), Unarmored Movement and Uncanny Metabolism at level two; Deflect Attacks and the Warrior of the Open Hand at level three. Level four grants an available feat or ability points.";
        }
        if (klass == CharacterClass::barbarian && result.level <= 4)
        {
            result.description =
                "Rage, Unarmored Defense and Weapon Mastery; Danger Sense and Reckless Attack at level two; the Berserker with Frenzy, Primal Knowledge and a third Rage at level three. Level four grants an available feat or ability points and a third Weapon Mastery.";
        }
        if (klass == CharacterClass::ranger && result.level <= 4)
        {
            result.description =
                "Prepared spells with Favored Enemy and fixed HP advancement; Fighting Style or Druidic Warrior at level two; the Hunter with Hunter's Lore and Hunter's Prey at level three. Level four grants an available feat or ability points.";
        }
        if (detail::gains_feat(klass, result.level))
        {
            result.feats = {
                {"ability_score_improvement", "Ability points",
                 "Add 2 to one ability or 1 to two abilities; maximum 20."},
                {"defense", "Defense",
                 "+1 AC while wearing armor. Requires the Fighting Style feature.",
                 detail::has_grant(sheet.grants, "feature:fighting_style") &&
                     !detail::has_grant(sheet.grants, "feat:defense")},
                {"savage_attacker", "Savage Attacker",
                 "Once per turn on a weapon hit, roll weapon damage twice and keep the higher roll.",
                 !detail::has_grant(sheet.grants, "feat:savage_attacker")},
                {"archery", "Archery",
                 "+2 to attack rolls with Ranged weapons. Requires Fighting Style.",
                 detail::has_grant(sheet.grants, "feature:fighting_style") &&
                     !detail::has_grant(sheet.grants, "feat:archery")},
                {"great_weapon_fighting", "Great Weapon Fighting",
                 "Treat damage dice showing 1 or 2 as 3 with an eligible Melee weapon held in two hands.",
                 detail::has_grant(sheet.grants, "feature:fighting_style") &&
                     !detail::has_grant(sheet.grants, "feat:great_weapon_fighting")},
                {"two_weapon_fighting", "Two-Weapon Fighting",
                 "Add your ability modifier to the extra attack granted by the Light property.",
                 detail::has_grant(sheet.grants, "feature:fighting_style") &&
                     !detail::has_grant(sheet.grants, "feat:two_weapon_fighting")},
                {"alert", "Alert",
                 "Add proficiency to Initiative; optionally swap Initiative with an eligible ally before the first turn.",
                 !detail::has_grant(sheet.grants, "feat:alert")},
                // Repeatable in the SRD; the game takes it once from a feat
                // level (a Human's Versatile Skilled aside), since its skills are
                // recorded as one group, and not after Lessons of the First
                // Ones gave it, which then leaves it unoffered.
                {"skilled", "Skilled", "Gain proficiency in any three skills of your choice.",
                 std::none_of(sheet.grants.begin(), sheet.grants.end(),
                              [](const auto &g)
                              {
                                  return (g.id == "feat:skilled" &&
                                          g.source_id.ends_with(":ability_score_improvement")) ||
                                         g.id == "invocation:lessons_skilled";
                              })}};
        }
        if (detail::gains_feat(klass, result.level))
        {
            // After Savage Attacker, where the single Magic Initiate entry stood.
            const auto initiate = detail::magic_initiate_feats(sheet.grants);
            result.feats.insert(result.feats.begin() + 3, initiate.begin(), initiate.end());
        }
        if (klass == CharacterClass::paladin)
        {
            result.spells = {
                {"cure_wounds", "Cure Wounds", "Action; touch; heals 2d8 + Charisma modifier."},
                {"divine_smite", "Divine Smite",
                 "Bonus Action right after a melee hit: 2d8 Radiant damage, 3d8 against a Fiend or an Undead."},
                {"searing_smite", "Searing Smite",
                 "Bonus Action right after a melee hit: 1d6 Fire damage, then 1d6 at the start of each of the target's turns until it succeeds on a Constitution save."}};
        }
        if (klass == CharacterClass::cleric)
        {
            result.spells = {
                {"cure_wounds", "Cure Wounds", "Action; touch; heals 2d8 + Wisdom modifier."},
                {"healing_word", "Healing Word",
                 "Bonus action; 60 feet; heals 2d4 + Wisdom modifier."},
                {"blindness", "Blindness",
                 "Blindness/Deafness (blindness option): Constitution save; repeat at end of turn; up to 1 minute. Level 2 slot.",
                 result.level >= 3},
                {"inflict_wounds", "Inflict Wounds",
                 "Action; touch; Constitution save; 2d10 Necrotic damage, half on a success, +1d10 from a level 2 slot."},
                {"bless", "Bless",
                 "Action; 30 feet; up to three creatures add 1d4 to attack rolls and saving throws. Concentration, up to 1 minute."},
                {"protection_from_evil_and_good", "Protection from Evil and Good",
                 "Action; touch; Aberrations, Celestials, Elementals, Fey, Fiends and Undead attack the creature with Disadvantage. Concentration, up to 10 minutes."}};
        }
        if (klass == CharacterClass::wizard)
        {
            result.spells = {
                {"magic_missile", "Magic Missile", "Action; 120 feet; three darts at one target."},
                {"scorching_ray", "Scorching Ray",
                 "Action; 120 feet; three spell attacks at one target. Requires level 3.",
                 result.level >= 3},
                {"blindness", "Blindness",
                 "Blindness/Deafness (blindness option): Constitution save; repeat at end of turn; up to 1 minute. Level 2 slot.",
                 result.level >= 3},
                {"shield", "Shield", "Unavailable: spell reactions are not implemented.", false}};
        }
        return result;
    }

    AdvancementOptions advancement_options(const CharacterSheet &sheet,
                                           const AdvancementChoice &choice) const override
    {
        const auto klass = advancing_class(sheet, choice);
        auto options = class_options(sheet, klass);
        // A new class's Rogue Expertise follows the skill picked with it.
        if (options.level && options.class_level == 1)
        {
            options.training = detail::multiclass_options(klass, sheet.grants, choice.training);
        }
        if (options.level)
        {
            options.cantrip_swap = detail::cantrip_swap_offer(sheet, klass);
            options.spell_swap = detail::spell_swap_offer(sheet, klass);
            options.option_swap = detail::option_swap_offer(sheet, klass);
            options.discovery_swap = detail::discovery_swap_offer(sheet, klass);
        }
        // Skilled's proficiency page exists only while Skilled is the selection,
        // so switching away from it drops the group and its unconfirmed picks.
        if (choice.feat == "skilled" && options.level &&
            detail::gains_feat(klass, options.class_level))
        {
            options.training.push_back(detail::skilled_options(sheet.grants, options.level));
        }
        // Lessons of the First Ones' Skilled asks for its skills when it is
        // taken, as a new invocation or a replacement.
        if (options.level && takes_lessons_skilled(choice))
        {
            options.training.push_back(
                detail::lessons_skilled_options(sheet.grants, options.level));
        }
        if (choice.fighting_style && klass == CharacterClass::fighter)
        {
            constexpr std::string_view source = "class:fighter:fighting_style";
            for (auto &feat : options.feats)
            {
                if (feat.id == "defense" || feat.id == "archery" ||
                    feat.id == "great_weapon_fighting" || feat.id == "two_weapon_fighting")
                {
                    feat.available =
                        feat.id != *choice.fighting_style &&
                        std::none_of(sheet.grants.begin(), sheet.grants.end(),
                                     [&](const auto &g)
                                     {
                                         return g.id == "feat:" + feat.id && g.source_id != source;
                                     });
                }
            }
        }
        return options;
    }

    AdvancementChoice default_advancement(const CharacterSheet &sheet) const override
    {
        const auto klass = detail::class_of(sheet);
        AdvancementChoice choice;
        const auto options = advancement_options(sheet);
        if (!options.level)
        {
            return choice;
        }
        if (options.class_level == 2 &&
            (klass == CharacterClass::paladin || klass == CharacterClass::ranger))
        {
            choice.fighting_style = "defense";
        }
        choice.spells = detail::class_prepared_spells(sheet.prepared_spells, klass, klass);
        for (const auto &group : options.training)
        {
            for (const auto &option : group.options)
            {
                if (choice.training[group.id].size() < group.count)
                {
                    choice.training[group.id].push_back(option.id);
                }
            }
        }
        if (choice.spells.empty() && klass == CharacterClass::wizard)
        {
            choice.spells = {"magic_missile"};
        }
        if (detail::prepares_spells(klass))
        {
            auto next = sheet;
            next.level = options.level;
            next.level_classes = detail::level_class_ids(sheet);
            next.level_classes.emplace_back(detail::class_id(klass));
            choice.spell_learning.emplace();
            for (const auto &group :
                 detail::spell_choice_options(next, SpellChoiceContext::advancement).learning)
            {
                auto &values = (*choice.spell_learning)[group.id];
                for (const auto &option : group.options)
                {
                    if (values.size() == group.count)
                    {
                        break;
                    }
                    values.push_back(option.id);
                }
            }
            detail::apply_spell_choices(next, SpellChoices{*choice.spell_learning, {}, {}, {}},
                                        SpellChoiceContext::advancement,
                                        ChoiceCompleteness::partial);
            const auto preparation =
                detail::spell_choice_options(next, SpellChoiceContext::advancement);
            choice.spells = preparation.locked_prepared;
            for (const auto &spell : preparation.preparation)
            {
                if (choice.spells.size() < preparation.prepared_count &&
                    std::find(choice.spells.begin(), choice.spells.end(), spell.id) ==
                        choice.spells.end())
                {
                    choice.spells.push_back(spell.id);
                }
            }
        }
        if (detail::gains_feat(klass, options.class_level))
        {
            choice.feat = "ability_score_improvement";
            const Ability primary =
                (klass == CharacterClass::fighter || klass == CharacterClass::paladin)
                    ? Ability::strength
                : (klass == CharacterClass::cleric || klass == CharacterClass::druid)
                    ? Ability::wisdom
                : (klass == CharacterClass::rogue || klass == CharacterClass::ranger)
                    ? Ability::dexterity
                : (klass == CharacterClass::sorcerer || klass == CharacterClass::warlock ||
                   klass == CharacterClass::bard)
                    ? Ability::charisma
                    : Ability::intelligence;
            // Two points, from the primary ability onwards in sheet order,
            // wrapping past Charisma, never above 20.
            unsigned remaining = 2;
            for (std::size_t n = 0; n < all_abilities.size() && remaining; ++n)
            {
                const auto ability =
                    all_abilities[(ability_index(primary) + n) % all_abilities.size()];
                choice.abilities[ability] = std::min(
                    remaining, static_cast<unsigned>(std::max(0, 20 - sheet.scores[ability])));
                remaining -= choice.abilities[ability];
            }
        }
        return choice;
    }

    CharacterSheet spell_choice_sheet(const CharacterSheet &sheet,
                                      const AdvancementChoice &choice) const override
    {
        const auto klass = advancing_class(sheet, choice);
        // The level in the class it is taken in (MULTI-1).
        const auto class_level = static_cast<unsigned>(detail::class_levels(sheet)[klass] + 1);
        auto next = sheet;
        ++next.level;
        next.level_classes = detail::level_class_ids(sheet);
        next.level_classes.emplace_back(detail::class_id(klass));
        // A class taken after the first: its level-one features, which its
        // spell choices need, and part of its training (MULTI-1).
        if (class_level == 1)
        {
            next.grants.push_back(
                {std::string(detail::multiclass_prefix) + std::string(detail::class_id(klass)),
                 "class:" + std::string(detail::class_id(klass)),
                 static_cast<unsigned>(next.level),
                 {}});
            for (auto &grant :
                 detail::class_level_grants(klass, 1, static_cast<unsigned>(next.level)))
            {
                next.grants.push_back(std::move(grant));
            }
        }
        if ((klass == CharacterClass::paladin || klass == CharacterClass::ranger) &&
            class_level == 2)
        {
            next.grants.push_back({"feature:fighting_style",
                                   "class:" + std::string(detail::class_id(klass)),
                                   static_cast<unsigned>(next.level),
                                   {}});
        }
        if (choice.fighting_style)
        {
            const auto source = "class:" + std::string(detail::class_id(klass)) + ":fighting_style";
            std::erase_if(next.grants,
                          [&](const auto &g)
                          {
                              return g.source_id == source;
                          });
            next.grants.push_back({fighting_style_grant(*choice.fighting_style),
                                   source,
                                   static_cast<unsigned>(next.level),
                                   {}});
        }
        // A cantrip, spell, invocation or Metamagic option replaced on gaining
        // the level changes what the level's spell choices offer.
        detail::apply_cantrip_swap(next, choice.cantrip_swap,
                                   detail::cantrip_swap_offer(sheet, klass));
        detail::apply_spell_swap(next, choice.spell_swap, detail::spell_swap_offer(sheet, klass),
                                 klass);
        detail::apply_option_swap(next, choice.option_swap,
                                  detail::option_swap_offer(sheet, klass));
        detail::apply_discovery_swap(next, choice.discovery_swap,
                                     detail::discovery_swap_offer(sheet, klass));
        const auto held = [&](std::string_view id)
        {
            return std::any_of(next.grants.begin(), next.grants.end(),
                               [&](const auto &g)
                               {
                                   return g.id == id;
                               });
        };
        // A blast invocation needs the cantrip it improves.
        for (const auto &g : next.grants)
        {
            for (const std::string_view blast :
                 {"invocation:agonizing_blast:", "invocation:eldritch_spear:",
                  "invocation:repelling_blast:"})
            {
                if (g.id.starts_with(blast) && !held("spell:" + g.id.substr(blast.size())))
                {
                    throw std::runtime_error("A blast invocation needs the cantrip it improves.");
                }
            }
        }
        // Magic Initiate's spells are chosen with the level's other spells,
        // whether the feat comes at level four or from Lessons of the First Ones.
        if (choice.feat.starts_with("magic_initiate_") && detail::gains_feat(klass, class_level))
        {
            next.grants.push_back(detail::advancement_grant(klass, next.level, choice));
        }
        // A Lore Bard's Magical Discoveries and a Land Druid's circle spells
        // are always prepared, so the level's preparations leave them out.
        for (const auto &[source, prefix] : {std::pair<std::string_view, std::string_view>{
                                                 "subclass:bard:discoveries", "discovery:"},
                                             {"subclass:druid:land", "land:"}})
        {
            const auto picked = choice.training.find(std::string(source));
            if (picked == choice.training.end())
            {
                continue;
            }
            for (const auto &id : picked->second)
            {
                next.grants.push_back({std::string(prefix) + id,
                                       std::string(source),
                                       static_cast<unsigned>(next.level),
                                       {}});
            }
        }
        // So are an invocation's that teaches spells: Lessons of the First
        // Ones' Magic Initiate and the Pact of the Tome's Book of Shadows, from
        // the level's invocations or a new Warlock level's (MULTI-1).
        for (const auto &invocations :
             {detail::invocations_source(class_level), std::string("class:warlock:invocations")})
        {
            const auto picked = choice.training.find(invocations);
            if (picked == choice.training.end())
            {
                continue;
            }
            for (const auto &id : picked->second)
            {
                if (id.starts_with("lessons_magic_initiate_") || id == "pact_of_the_tome")
                {
                    next.grants.push_back(
                        {"invocation:" + id, invocations, static_cast<unsigned>(next.level), {}});
                }
            }
        }
        return next;
    }

    bool advance_character(CharacterSheet &sheet, VitalState &state,
                           const AdvancementChoice &choice) const override
    {
        const auto klass = advancing_class(sheet, choice);
        const auto options = advancement_options(sheet, choice);
        if (!options.level)
        {
            return false;
        }
        // The level in the class the level is taken in (MULTI-1).
        const auto class_level = options.class_level;
        const bool half_style =
            (klass == CharacterClass::paladin || klass == CharacterClass::ranger) &&
            class_level == 2;
        if (half_style && !choice.fighting_style)
        {
            throw std::runtime_error("Choose a Fighting Style");
        }
        if (choice.fighting_style &&
            std::none_of(options.fighting_styles.begin(), options.fighting_styles.end(),
                         [&](const auto &f)
                         {
                             return f.id == *choice.fighting_style && f.id != "keep" && f.available;
                         }))
        {
            throw std::runtime_error("Choose an eligible Fighting Style");
        }
        // A Champion 7 changing its style takes a different one as its
        // Additional Fighting Style: one feat cannot come twice.
        if (const auto extra = choice.training.find(std::string(detail::champion_style));
            choice.fighting_style && extra != choice.training.end() &&
            std::find(extra->second.begin(), extra->second.end(), *choice.fighting_style) !=
                extra->second.end())
        {
            throw std::runtime_error(
                "Choose an Additional Fighting Style other than the new Fighting Style");
        }
        const auto old = character_definition(character_profile(sheet, {}).data);
        unsigned points = 0;
        for (auto n : choice.abilities)
        {
            if (n > 2)
            {
                throw std::runtime_error("An ability increase cannot exceed 2");
            }
            points += n;
        }
        if (detail::gains_feat(klass, class_level))
        {
            const auto feat = std::find_if(options.feats.begin(), options.feats.end(),
                                           [&](const auto &f)
                                           {
                                               return f.id == choice.feat && f.available;
                                           });
            if (feat == options.feats.end())
            {
                throw std::runtime_error("Choose an available feat or ability points");
            }
            if (points != (choice.feat == "ability_score_improvement" ? 2u : 0u))
            {
                throw std::runtime_error("Assign exactly two ability points, or choose a feat");
            }
            // The training loop below checks the count only when the group is
            // present, so require the picks rather than granting Skilled empty.
            if (choice.feat == "skilled" && !choice.training.contains("feat:skilled"))
            {
                throw std::runtime_error("Choose exactly three Skilled proficiencies");
            }
        }
        else if (!choice.feat.empty() || points)
        {
            throw std::runtime_error(
                "Feats and ability points come only at the class's feat levels");
        }
        // Classes that prepare spells validate their choices through
        // apply_spell_choices below, against the whole class list.
        std::set<std::string> selected;
        for (const auto &spell : choice.spells)
        {
            if (!selected.insert(spell).second ||
                (!detail::prepares_spells(klass) &&
                 std::none_of(options.spells.begin(), options.spells.end(),
                              [&](const auto &s)
                              {
                                  return s.id == spell && s.available;
                              })))
            {
                throw std::runtime_error("Choose only available, distinct spells");
            }
        }
        if (!options.spells.empty() && choice.spells.empty())
        {
            throw std::runtime_error("Choose at least one supported spell");
        }
        if (takes_lessons_skilled(choice) &&
            !choice.training.contains("invocation:lessons_skilled"))
        {
            throw std::runtime_error("Choose exactly three Skilled proficiencies");
        }
        // A new class's level-one choices are all made with it (MULTI-1).
        if (class_level == 1 && std::any_of(options.training.begin(), options.training.end(),
                                            [&](const auto &group)
                                            {
                                                return !choice.training.contains(group.id);
                                            }))
        {
            throw std::runtime_error("Choose the new class's training");
        }
        // A swapped-in invocation cannot also be one of the level's new picks.
        if (const auto picks = choice.training.find(detail::invocations_source(class_level));
            picks != choice.training.end() && !choice.option_swap.replacement.empty() &&
            std::find(picks->second.begin(), picks->second.end(), choice.option_swap.replacement) !=
                picks->second.end())
        {
            throw std::runtime_error("An invocation cannot be both swapped in and chosen");
        }
        auto actor = actor_from(old, state);
        auto next = spell_choice_sheet(sheet, choice);
        // A blast invocation picked on this level needs the cantrip it
        // improves, which a cantrip swapped away on the same level is not.
        for (const auto &[source, picks] : choice.training)
        {
            for (const auto &pick : picks)
            {
                for (const std::string_view blast :
                     {"agonizing_blast:", "eldritch_spear:", "repelling_blast:"})
                {
                    if (source.starts_with("class:warlock:invocations") &&
                        pick.starts_with(blast) &&
                        !detail::has_grant(next.grants, "spell:" + pick.substr(blast.size())))
                    {
                        throw std::runtime_error(
                            "A blast invocation needs the cantrip it improves.");
                    }
                }
            }
        }
        // The list after a spell swap; the level's choices then add to it.
        const auto prepared_before = next.prepared_spells;
        for (const auto &[id, values] : choice.training)
        {
            const auto group = std::find_if(options.training.begin(), options.training.end(),
                                            [&](const auto &g)
                                            {
                                                return g.id == id;
                                            });
            const auto entry_prefix =
                class_level == 1 ? detail::multiclass_grant_prefix(id) : std::nullopt;
            if (group == options.training.end() || values.size() != group->count ||
                std::set<std::string>(values.begin(), values.end()).size() != values.size() ||
                std::any_of(values.begin(), values.end(),
                            [&](const auto &value)
                            {
                                return std::none_of(group->options.begin(), group->options.end(),
                                                    [&](const auto &o)
                                                    {
                                                        return o.id == value;
                                                    });
                            }))
            {
                throw std::runtime_error("Invalid advancement training choice");
            }
            for (const auto &value : values)
            {
                FeatureGrant grant{
                    // A new class's level-one choices (MULTI-1).
                    entry_prefix ? std::string(*entry_prefix) + value
                    // Skilled spans both catalogs, so its options carry the prefixed id.
                    : id == "feat:skilled"                         ? value
                    : id == "class:wizard:scholar"                 ? "expertise:" + value
                    : id == "subclass:ranger:hunter"               ? "prey:" + value
                    : id == "class:barbarian:primal_knowledge"     ? "skill:" + value
                    : id.starts_with("class:sorcerer:metamagic")   ? "metamagic:" + value
                    : id == detail::elemental_affinity             ? "affinity:" + value
                    : id == detail::fiendish_resilience            ? "resilience:" + value
                    : id.starts_with("class:warlock:arcanum:")     ? "arcanum:" + value
                    : id == "subclass:bard:discoveries"            ? "discovery:" + value
                    : id == detail::elemental_fury                 ? "fury:" + value
                    : id.starts_with("class:warlock:invocations:") ? "invocation:" + value
                    : id == "invocation:lessons_skilled"           ? value
                    : id == "subclass:bard:lore"                   ? "skill:" + value
                    : id == "subclass:druid:land"                  ? "land:" + value
                    : id == detail::champion_style                 ? "feat:" + value
                    : id == detail::defensive_tactics              ? "tactics:" + value
                    : id == detail::blessed_strikes                ? "strikes:" + value
                    : detail::expertise_pick(id)                   ? "expertise:" + value
                                                                   : "mastery:" + value,
                    id,
                    static_cast<unsigned>(next.level),
                    {}};
                // spell_choice_sheet already granted Lessons' Magic Initiate.
                if (std::find(next.grants.begin(), next.grants.end(), grant) == next.grants.end())
                {
                    next.grants.push_back(std::move(grant));
                }
            }
        }
        // spell_choice_sheet granted a new class's level-one features.
        if (class_level > 1)
        {
            for (auto &grant :
                 detail::class_level_grants(klass, class_level, static_cast<unsigned>(next.level)))
            {
                next.grants.push_back(std::move(grant));
            }
        }
        for (const auto ability : all_abilities)
        {
            next.save_proficiencies[ability] = detail::save_proficient(
                detail::class_of(next), detail::class_levels(next), ability);
        }
        for (const auto ability : all_abilities)
        {
            next.scores[ability] += choice.abilities[ability];
            next.bonuses[ability] += choice.abilities[ability];
            if (next.scores[ability] > 20)
            {
                throw std::runtime_error("Ability scores cannot exceed 20");
            }
            next.modifiers[ability] = ability_modifier(next.scores[ability]);
            next.saving_throws[ability] =
                next.modifiers[ability] +
                (next.save_proficiencies[ability] ? 2 + (static_cast<int>(next.level) - 1) / 4 : 0);
        }
        if (points)
        {
            AbilityAdjustment adjustment{"feat:" + choice.feat,
                                         "Level " + std::to_string(next.level) +
                                             " Ability Score Improvement",
                                         static_cast<unsigned>(next.level)};
            for (const auto ability : all_abilities)
            {
                adjustment.bonuses[ability] = static_cast<int>(choice.abilities[ability]);
            }
            adjustment.label_message = {"Level {level} Ability Score Improvement",
                                        {{"level", std::to_string(next.level)}}};
            next.ability_adjustments.push_back(std::move(adjustment));
        }
        // spell_choice_sheet already granted Magic Initiate, whose spells it
        // offers. The same feat from another source (a Human's Versatile
        // Skilled) is a second grant.
        if (auto feat = detail::advancement_grant(klass, next.level, choice);
            !choice.feat.empty() &&
            std::find(next.grants.begin(), next.grants.end(), feat) == next.grants.end())
        {
            next.grants.push_back(std::move(feat));
        }
        // A multiclass character's other classes keep their prepared spells;
        // the level's class prepares through its spell choices (MULTI-1).
        next.prepared_spells =
            detail::class_levels(next).count() > 1 ? prepared_before : choice.spells;
        if (choice.spell_learning)
        {
            next.prepared_spells = prepared_before;
            // A class without spells of its own learns only Magic Initiate's.
            const auto prepared =
                detail::prepares_spells(klass) ? std::optional(choice.spells) : std::nullopt;
            detail::apply_spell_choices(
                next, SpellChoices{*choice.spell_learning, prepared, {}, {}},
                SpellChoiceContext::advancement, ChoiceCompleteness::complete);
        }
        else if (detail::prepares_spells(klass) ||
                 !detail::spell_choice_options(next, SpellChoiceContext::advancement)
                      .learning.empty())
        {
            throw std::runtime_error("Independent spell learning choices are required");
        }
        next.training = detail::training_profile(
            next.grants, detail::ClassHistory(next.level_classes), next.race,
            detail::background_id(next.background), next.scores);
        next.hit_point_modifiers.push_back(next.modifiers[Ability::constitution]);
        next.hit_points = maximum_hit_points(detail::ClassHistory(next.level_classes),
                                             next.race == "Dwarf", next.hit_point_modifiers);
        if (next.race == "Dwarf")
        {
            next.racial_modifiers.replace(0, next.racial_modifiers.find('\n'),
                                          "Dwarven Toughness: +" + std::to_string(next.level) +
                                              " maximum HP.");
            for (auto &message : next.racial_messages)
            {
                if (message.source == "Dwarven Toughness: +{hp} maximum HP.")
                {
                    message.arguments = {{"hp", std::to_string(next.level)}};
                }
            }
        }
        const int growth = next.hit_points - sheet.hit_points;
        next.hp_explanation = "Level " + std::to_string(next.level) + ": " +
                              std::to_string(next.hit_points) + " maximum HP; gain " +
                              std::to_string(growth) + ".";
        next.hp_messages = {{"Level {level}: {hp} maximum HP; gain {growth}.",
                             {{"level", std::to_string(next.level)},
                              {"hp", std::to_string(next.hit_points)},
                              {"growth", std::to_string(growth)}}}};
        if (klass == CharacterClass::rogue && class_level == 2)
        {
            next.class_modifiers +=
                "\nCunning Action: Dash, Disengage or Hide as a Bonus Action on your turn.";
            next.class_messages.push_back(
                {"Cunning Action: Dash, Disengage or Hide as a Bonus Action on your turn.", {}});
        }
        if (klass == CharacterClass::rogue && class_level == 3)
        {
            next.class_modifiers +=
                "\nSneak Attack: 2d6. Steady Aim: Bonus Action; next attack roll has Advantage, Speed becomes 0.";
            next.class_messages.push_back(
                {"Sneak Attack: 2d6. Steady Aim: Bonus Action; next attack roll has Advantage, Speed becomes 0.",
                 {}});
        }
        if (klass == CharacterClass::paladin || klass == CharacterClass::ranger ||
            klass == CharacterClass::barbarian || klass == CharacterClass::monk ||
            klass == CharacterClass::sorcerer || klass == CharacterClass::warlock ||
            klass == CharacterClass::bard || klass == CharacterClass::druid)
        {
            const std::string note =
                klass == CharacterClass::druid
                    ? "Prepared Druid spells and a Primal Order: Magician (an extra cantrip) or Warden (Martial weapons and Medium armor); the Circle of the Land at level three with its land's Circle Spells always prepared. Level four grants a third cantrip and an available feat or ability points."
                : klass == CharacterClass::bard
                    ? "Bardic spellcasting with prepared Bard spells; Jack of All Trades at level two; the College of Lore at level three with three more skills. Level four grants a third cantrip and an available feat or ability points."
                : klass == CharacterClass::warlock
                    ? "Pact Magic: prepared Warlock spells cast from slots of one level that return on a Short Rest; Magical Cunning at level two; the Fiend Patron at level three: Dark One's Blessing and Burning Hands, Command and Scorching Ray always prepared. Level four grants a third cantrip and an available feat or ability points."
                : klass == CharacterClass::sorcerer
                    ? "Prepared Sorcerer spells and Innate Sorcery; Font of Magic's Sorcery Points at level two; Draconic Sorcery at level three: Draconic Resilience (Hit Points and unarmored AC 10 + Dexterity + Charisma) and Chromatic Orb, Command and Dragon's Breath always prepared. Level four grants a fifth cantrip and an available feat or ability points."
                : klass == CharacterClass::monk
                    ? "Martial Arts and Unarmored Defense; Monk's Focus (Flurry of Blows, Patient Defense, Step of the Wind), Unarmored Movement and Uncanny Metabolism at level two; Deflect Attacks and the Warrior of the Open Hand at level three. Level four grants an available feat or ability points."
                : klass == CharacterClass::barbarian
                    ? "Rage, Unarmored Defense and Weapon Mastery; Danger Sense and Reckless Attack at level two; the Berserker with Frenzy, Primal Knowledge and a third Rage at level three. Level four grants an available feat or ability points and a third Weapon Mastery."
                : klass == CharacterClass::paladin
                    ? "Prepared spells, Lay On Hands and fixed HP advancement; Fighting Style or Blessed Warrior and Paladin's Smite at level two; Channel Divinity, the Oath of Devotion and Sacred Weapon at level three. Level four grants an available feat or ability points."
                    : "Prepared spells with Favored Enemy and fixed HP advancement; Fighting Style or Druidic Warrior at level two; the Hunter with Hunter's Lore and Hunter's Prey at level three. Level four grants an available feat or ability points.";
            next.class_modifiers += "\n" + note;
            next.class_messages.push_back({note, {}});
        }
        else
        {
            next.class_modifiers +=
                "\nLevel " + std::to_string(next.level) +
                ": HP and spell-slot advancement applied. Additional class and subclass features remain unavailable.";
            next.class_messages.push_back(
                {"Level {level}: HP and spell-slot advancement applied. Additional class and subclass features remain unavailable.",
                 {{"level", std::to_string(next.level)}}});
        }
        if (klass == CharacterClass::fighter && class_level == 2)
        {
            next.class_modifiers +=
                "\nAction Surge: one additional action, except Magic, on your turn. One use per Short or Long Rest.";
            next.class_messages.push_back(
                {"Action Surge: one additional action, except Magic, on your turn. One use per Short or Long Rest.",
                 {}});
        }
        actor.definition = character_definition(character_profile(next, {}).data);
        if (actor.life.hp > 0)
        {
            actor.life.hp += growth;
        }
        // Pact Magic's slots rise a level together (level 2 at Warlock level
        // 3); the slots already spent stay spent.
        const int spent_slots = slot_total(old.slots) - slot_total(actor.slots);
        if (actor.definition.pact_magic &&
            pact_level(actor.definition.slots) != pact_level(old.slots))
        {
            const auto level = pact_level(actor.definition.slots);
            actor.slots = {};
            actor.slots[level] = std::max(0, actor.definition.slots[level] - spent_slots);
        }
        else
        {
            for (unsigned level = 1; level <= max_slot_level; ++level)
            {
                actor.slots[level] += actor.definition.slots[level] - old.slots[level];
            }
        }
        actor.winds += actor.definition.winds - old.winds;
        actor.rushes += actor.definition.rushes - old.rushes;
        grow(actor.surges, surge_capacity(actor.definition), surge_capacity(old));
        grow(actor.arcane, arcane_capacity(actor.definition), arcane_capacity(old));
        grow(actor.lay_on_hands, lay_capacity(actor.definition), lay_capacity(old));
        grow(actor.free_casts, free_cast_capacity(actor.definition), free_cast_capacity(old));
        // A feat taken now arrives with its free cast; earlier ones keep their bits.
        actor.initiate_casts |= initiate_capacity(actor.definition) & ~initiate_capacity(old);
        grow(actor.channel_divinity, channel_capacity(actor.definition), channel_capacity(old));
        actor.hit_dice += actor.definition.level - old.level;
        auto continuation = vitals(actor);
        sheet = std::move(next);
        state = std::move(continuation);
        return true;
    }

    void validate_character_state(const CharacterSheet &sheet,
                                  const VitalState &state) const override
    {
        auto actor = camp_actor(sheet, state);
    }

    // SRD Long Rest: eight hours, then sixteen hours before the next one begins.
    RestPolicy long_rest_policy() const override
    {
        return {480, 960};
    }

    RestPolicy short_rest_policy() const override
    {
        return {60, 0};
    }

    void elapse(std::span<Participant> participants, std::chrono::milliseconds elapsed,
                RandomState &random_state) const override
    {
        if (elapsed < std::chrono::milliseconds::zero())
        {
            throw std::invalid_argument("Time cannot run backwards");
        }
        const auto milliseconds = static_cast<std::uint64_t>(elapsed.count());
        // Work on owned candidates so malformed state cannot partly advance a
        // party or consume its RNG. No Godot or campaign data enters the rules.
        std::vector<Actor> actors;
        actors.reserve(participants.size());
        for (const auto &p : participants)
        {
            Actor a;
            a.source = p;
            a.definition = p.character_profile.empty() ? content_->definitions.at(p.definition)
                                                       : character_definition(p.character_profile);
            a.life.hp = a.definition.hp;
            a.winds = a.definition.winds;
            a.slots = a.definition.slots;
            a.hit_dice = a.definition.hit_die ? a.definition.level : 0;
            if (p.state)
            {
                restore_vitals(a, *p.state);
            }
            actors.push_back(std::move(a));
        }
        std::vector<detail::RecoverySubject> subjects;
        for (auto &a : actors)
        {
            subjects.push_back(
                {{a.source.id, a.effects, a.definition.saves, a.life.dead,
                  a.definition.str_dex_disadvantage, false, a.definition.gnomish_cunning,
                  a.definition.lucky, a.definition.brave, a.definition.dwarf},
                 a.life});
        }
        auto rng = random_state.value;
        // A Mummy's curse lowers the Hit Point maximum by 3d6 as each day
        // ends; at 0 the creature turns to dust. The days are counted before
        // the effects' clocks run, so the curse is never timed out.
        constexpr std::uint64_t day_ms = 86400000;
        for (auto &a : actors)
        {
            std::optional<detail::EffectSource> dust_from;
            std::string mummy;
            for (auto &e : a.effects.active)
            {
                if (e.kind != detail::EffectKind::rotting_curse || a.life.dead)
                {
                    continue;
                }
                std::uint64_t next_day = e.remaining_ms;
                for (; next_day <= milliseconds && !dust_from; next_day += day_ms)
                {
                    e.dc += roll_die(rng, 6) + roll_die(rng, 6) + roll_die(rng, 6);
                    if (a.definition.hp + detail::hit_point_bonus(a.effects) <= 0)
                    {
                        dust_from = detail::EffectSource{e.source_scope, e.source_actor};
                        mummy = e.source_name;
                    }
                }
                e.remaining_ms = static_cast<unsigned>(
                    std::min<std::uint64_t>(next_day, std::numeric_limits<unsigned>::max()));
            }
            if (dust_from)
            {
                a.life.hp = 0;
                a.life.dead = true;
                a.life.recovery = {};
                detail::apply_spell_benefit(a.effects, *dust_from, mummy, detail::EffectKind::dust,
                                            0);
            }
        }
        detail::elapse_recovery(subjects, milliseconds, rng);
        // An ended Aid lowers the maximum, and Hit Points above it with it.
        for (auto &a : actors)
        {
            a.life.hp = std::min(a.life.hp, max_hp(a));
        }
        if (!milliseconds)
        {
            return;
        }
        std::vector<VitalState> next;
        next.reserve(actors.size());
        for (const auto &a : actors)
        {
            next.push_back(vitals(a));
        }
        for (std::size_t i = 0; i < participants.size(); ++i)
        {
            if (participants[i].state &&
                // A fresh character's empty state stays empty unless dying.
                ((participants[i].state->hit_points == 0 && !participants[i].state->dead) ||
                 !participants[i].state->resources.empty()))
            {
                participants[i].state = std::move(next[i]);
            }
        }
        random_state.value = rng;
    }

    void recover(VitalState &state, const CharacterSheet &sheet) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        auto actor = actor_from(d, state);
        if (actor.life.dead || actor.life.hp < 1)
        {
            throw std::runtime_error("Long rest requires at least one HP at its start");
        }
        // A simulacrum takes no rests.
        if (detail::has_effect(actor.effects, detail::EffectKind::simulacrum))
        {
            return;
        }
        (void)detail::heal_life(actor.life, max_hp(actor), max_hp(actor),
                                !detail::healing_blocked(actor.effects));
        // A Long Rest makes Prayer of Healing able to help the creature again,
        // and gives back Relentless Endurance, a species trait's uses and a
        // free Overchannel.
        std::erase_if(actor.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::prayer_of_healing ||
                                 e.kind == detail::EffectKind::javelin_spent ||
                                 e.kind == detail::EffectKind::relentless_used ||
                                 e.kind == detail::EffectKind::protectors_used ||
                                 e.kind == detail::EffectKind::overchanneled ||
                                 e.kind == detail::EffectKind::wings_used ||
                                 e.kind == detail::EffectKind::luck_used ||
                                 e.kind == detail::EffectKind::hurl_used ||
                                 e.kind == detail::EffectKind::arcanum_used ||
                                 e.kind == detail::EffectKind::resurged ||
                                 e.kind == detail::EffectKind::natural_cast ||
                                 e.kind == detail::EffectKind::trait_uses ||
                                 e.kind == detail::EffectKind::heroic_inspiration ||
                                 e.kind == detail::EffectKind::life_drained;
                      });
        // Back from the dead: the penalty eases by 1 with each Long Rest.
        for (auto &e : actor.effects.active)
        {
            if (e.kind == detail::EffectKind::returned)
            {
                --e.dc;
            }
        }
        std::erase_if(actor.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::returned && e.dc <= 0;
                      });
        // Resourceful: a Human finishes the Long Rest with Heroic Inspiration.
        if (d.resourceful && detail::can_apply(actor.effects))
        {
            // Camp effects carry scope 1: there is no encounter to source them.
            detail::apply_spell_benefit(actor.effects, detail::EffectSource{.scope = 1, .actor = 1},
                                        sheet.name, detail::EffectKind::heroic_inspiration, 0);
        }
        actor.winds = d.winds;
        actor.slots = d.slots;
        actor.hit_dice = d.hit_die ? d.level : 0;
        actor.life.successes = actor.life.failures = 0;
        actor.life.stable = false;
        actor.life.recovery = {};
        actor.life.temporary_hp = {};
        actor.rushes = d.rushes;
        actor.surges = surge_capacity(d);
        actor.arcane = arcane_capacity(d);
        actor.lay_on_hands = lay_capacity(d);
        actor.free_casts = free_cast_capacity(d);
        actor.channel_divinity = channel_capacity(d);
        actor.initiate_casts = initiate_capacity(d);
        state = vitals(actor);
    }

    RecoveryInfo recovery_info(const CharacterSheet &sheet, const VitalState &state) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        auto actor = actor_from(d, state);
        // A simulacrum spends no Hit Dice: it takes no rests.
        const bool simulacrum = detail::has_effect(actor.effects, detail::EffectKind::simulacrum);
        RecoveryInfo result{static_cast<unsigned>(next_hit_die(d, actor.hit_dice)),
                            simulacrum ? 0u : static_cast<unsigned>(actor.hit_dice),
                            static_cast<unsigned>(d.level),
                            !actor.life.dead && actor.life.hp > 0,
                            {},
                            actor.life.temporary_hp};
        for (const auto &descriptor : resource_descriptors)
        {
            if (capacity_of(descriptor, d))
            {
                result.resources.push_back(resource_pool(descriptor, actor, d));
            }
        }
        add_initiate_pools(result.resources, actor, d);
        add_trait_pool(result.resources, actor, d);
        for (const auto &choice :
             arcane_allocations(static_cast<unsigned>(recovery_class_level(d) + 1) / 2))
        {
            if (can_recover(actor, choice) && !simulacrum)
            {
                result.choices.push_back({choice.id, choice.label});
            }
        }
        // Sorcerous Restoration (Sorcerer 5): Sorcery Points up to half the
        // Sorcerer level, once per Long Rest.
        if (d.sorcerous_restoration && actor.arcane[CharacterClass::sorcerer] > 0 &&
            actor.lay_on_hands[CharacterClass::sorcerer] < d.sorcery_points && !simulacrum)
        {
            result.choices.push_back(
                {"sorcerous_restoration",
                 {"Sorcerous Restoration: regain {points} Sorcery Points",
                  {{"points",
                    std::to_string(std::min(d.class_levels[CharacterClass::sorcerer] / 2,
                                            d.sorcery_points -
                                                actor.lay_on_hands[CharacterClass::sorcerer]))}}}});
        }
        return result;
    }

    void grant_temporary_hit_points(VitalState &state, const CharacterSheet &sheet,
                                    const TemporaryHitPoints &offered,
                                    TemporaryHpChoice choice) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        auto actor = actor_from(d, state);
        detail::grant_temporary_hp(actor.life, offered, choice);
        auto next = vitals(actor);
        state = std::move(next);
    }

    void recover_short_rest(VitalState &state, const CharacterSheet &sheet) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        auto actor = actor_from(d, state);
        if (actor.life.dead || actor.life.hp < 1)
        {
            throw std::runtime_error("Short rest requires at least one HP at its start");
        }
        if (detail::has_effect(actor.effects, detail::EffectKind::simulacrum))
        {
            return;
        }
        actor.winds = std::min(d.winds, actor.winds + 1);
        actor.rushes = d.rushes;
        actor.surges = surge_capacity(d);
        // Each class's Channel Divinity, Rage or Wild Shape regains one use.
        const auto channels = channel_capacity(d);
        for (std::size_t n = 0; n < channels.by_class.size(); ++n)
        {
            actor.channel_divinity.by_class[n] =
                std::min(channels.by_class[n], actor.channel_divinity.by_class[n] + 1);
        }
        // Font of Inspiration (Bard 5): Bardic Inspiration returns on a Short Rest.
        if (d.font_of_inspiration)
        {
            actor.arcane[CharacterClass::bard] = d.bardic_inspiration;
        }
        // Pact Magic slots all return on a Short Rest; among another class's
        // slots of their level, as many as Pact Magic gives (MULTI-1).
        if (d.pact_magic)
        {
            actor.slots = d.slots;
        }
        else if (d.pact_slots)
        {
            const auto level = static_cast<std::size_t>(d.pact_slot_level);
            actor.slots[level] = std::min(d.slots[level], actor.slots[level] + d.pact_slots);
        }
        // Relentless Rage's DC drops back to 10.
        if (d.relentless_rage)
        {
            actor.lay_on_hands[CharacterClass::barbarian] = d.relentless_rage;
        }
        state = vitals(actor);
    }

    Message recover_rest_choice(VitalState &state, const CharacterSheet &sheet,
                                std::string_view choice_id) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        auto actor = actor_from(d, state);
        if (choice_id == "sorcerous_restoration")
        {
            if (!d.sorcerous_restoration || actor.arcane[CharacterClass::sorcerer] < 1 ||
                actor.lay_on_hands[CharacterClass::sorcerer] >= d.sorcery_points)
            {
                throw std::runtime_error("This rest recovery choice is unavailable");
            }
            const int regained =
                std::min(d.class_levels[CharacterClass::sorcerer] / 2,
                         d.sorcery_points - actor.lay_on_hands[CharacterClass::sorcerer]);
            actor.lay_on_hands[CharacterClass::sorcerer] += regained;
            --actor.arcane[CharacterClass::sorcerer];
            state = vitals(actor);
            return {"Sorcerous Restoration restored {points} Sorcery Points.",
                    {{"points", std::to_string(regained)}}};
        }
        const auto choices =
            arcane_allocations(static_cast<unsigned>(recovery_class_level(d) + 1) / 2);
        const auto choice = std::find_if(choices.begin(), choices.end(),
                                         [&](const auto &value)
                                         {
                                             return value.id == choice_id;
                                         });
        if (choice == choices.end() || !can_recover(actor, *choice))
        {
            throw std::runtime_error("This rest recovery choice is unavailable");
        }
        for (unsigned level = 1; level <= 5; ++level)
        {
            actor.slots[level] += static_cast<int>(choice->slots[level - 1]);
        }
        // A Wizard's Arcane Recovery first, then a Land Druid's Natural Recovery.
        --actor
              .arcane[d.arcane && actor.arcane[CharacterClass::wizard] > 0 ? CharacterClass::wizard
                                                                           : CharacterClass::druid];
        auto next = vitals(actor);
        const bool small = choice->slots[2] + choice->slots[3] + choice->slots[4] == 0;
        const bool natural = d.natural_recovery != 0;
        state = std::move(next);
        if (small)
        {
            return Message{
                natural
                    ? "Natural Recovery restored {first} level-one and {second} level-two spell slots."
                    : "Arcane Recovery restored {first} level-one and {second} level-two spell slots.",
                {{"first", std::to_string(choice->slots[0])},
                 {"second", std::to_string(choice->slots[1])}}};
        }

        return Message{natural ? "Natural Recovery restored spell slots of levels {levels}."
                               : "Arcane Recovery restored spell slots of levels {levels}.",
                       {{"levels", recovered_levels(choice->slots)}}};
    }

    HitDieResult spend_hit_die(VitalState &state, const CharacterSheet &sheet,
                               RandomState &random_state) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        auto actor = actor_from(d, state);
        if (actor.life.dead || actor.life.hp < 1 || actor.hit_dice < 1 ||
            detail::has_effect(actor.effects, detail::EffectKind::simulacrum))
        {
            throw std::runtime_error("No Hit Die can be spent by this character");
        }
        auto rng = random_state.value;
        const int die = next_hit_die(d, actor.hit_dice);
        const int rolled = roll_die(rng, die);
        const int healing =
            detail::heal_life(actor.life, std::max(1, rolled + d.constitution), max_hp(actor),
                              !detail::healing_blocked(actor.effects));
        --actor.hit_dice;
        HitDieResult result{static_cast<unsigned>(die), rolled, d.constitution, healing,
                            static_cast<unsigned>(actor.hit_dice)};
        auto next = vitals(actor);
        state = std::move(next);
        random_state.value = rng;
        return result;
    }

    void set_hit_points(VitalState &state, const CharacterSheet &sheet, int hp) const override
    {
        if (hp < 0 || hp > sheet.hit_points || (state.dead && hp))
        {
            throw std::runtime_error("Unsupported script HP change");
        }
        auto actor = camp_actor(sheet, state);
        if (hp == state.hit_points ||
            (hp > state.hit_points && detail::healing_blocked(actor.effects)))
        {
            return;
        }
        detail::set_life_hit_points(actor.life, hp, max_hp(actor));
        state = vitals(actor);
    }

    HazardAttackResult hazard_attack(VitalState &state, const CharacterSheet &sheet,
                                     std::span<const std::string> gear, const HazardAttack &attack,
                                     RandomState &random_state) const override
    {
        auto actor = actor_from(character_definition(character_profile(sheet, gear).data), state);
        if (actor.life.dead || actor.life.hp < 1)
        {
            throw std::runtime_error("A hazard attacks a conscious member");
        }
        auto rng = random_state.value;
        HazardAttackResult result;
        result.armor_class = armor_class_of(actor);
        if (attack.automatic)
        {
            result.hit = true;
        }
        else
        {
            result.natural = roll_die(rng, 20);
            result.total = result.natural + attack.attack_bonus;
            result.critical = result.natural == 20;
            result.hit =
                result.critical || (result.natural != 1 && result.total >= result.armor_class);
        }
        if (result.hit && attack.save_dc)
        {
            const auto save = detail::saving_throw(
                attack.save_ability,
                actor.definition.saves[static_cast<unsigned>(attack.save_ability)], attack.save_dc,
                {}, rng);
            result.save_total = save.natural + save.bonus;
            result.saved = save.success;
            result.hit = !save.success;
        }
        if (result.hit)
        {
            int rolled = attack.damage_bonus;
            for (unsigned n = 0; n < attack.dice * (result.critical ? 2 : 1); ++n)
            {
                result.damage_dice.push_back(roll_die(rng, static_cast<int>(attack.sides)));
                rolled += result.damage_dice.back();
            }
            result.damage_rolled = std::max(0, rolled);
            const detail::DamagePart part{detail::damage_type(attack.damage_type),
                                          std::max(0, rolled)};
            const auto &d = actor.form ? *actor.form : actor.definition;
            result.damage = detail::resolve_damage({&part, 1}, d.affinities).total;
            detail::damage_life(actor.life, result.damage, max_hp(actor));
            // As after a victory, the dying member's saves are rolled at once.
            while (actor.life.hp == 0 && !actor.life.dead && !actor.life.stable)
            {
                result.death_saves.push_back(
                    detail::death_save(actor.life, rng, !detail::healing_blocked(actor.effects),
                                       actor.definition.lucky));
            }
        }
        state = vitals(actor);
        random_state.value = rng;
        return result;
    }

    // A character's actor outside combat, its resources read from `state`.
    Actor camp_actor(const CharacterSheet &sheet, const VitalState &state) const
    {
        return actor_from(character_definition(character_profile(sheet, {}).data), state);
    }

    unsigned camp_action_gold(const CharacterSheet &target, std::string_view action) const override
    {
        // Revivify's offering: 300 gp times the revived member's level (LEVELS-1).
        const auto *spell = detail::find_spell(action);
        return spell && (spell->rider == detail::Rider::revivify || spell->id == "raise_dead" ||
                         spell->id == "resurrection" || spell->id == "reincarnate")
                   ? static_cast<unsigned>(300 * target.level)
                   : 0;
    }

    unsigned camp_action_gold(const CharacterSheet &target, const VitalState &state,
                              std::string_view action) const override
    {
        // A simulacrum's repair: 100 gp for each Hit Point it lacks.
        if (action != "simulacrum_repair")
        {
            return camp_action_gold(target, action);
        }
        const auto patient = camp_actor(target, state);
        return 100u * static_cast<unsigned>(std::max(0, max_hp(patient) - patient.life.hp));
    }

    std::string reincarnation_species(RandomState &random_state) const override
    {
        // A d10, rolled again on a 1. Same SplitMix64 as combat.
        static constexpr std::array<std::string_view, 9> species{
            "dragonborn", "dwarf", "elf", "gnome",   "goliath",
            "halfling",   "human", "orc", "tiefling"};
        auto rng = random_state.value;
        int rolled = 1;
        while (rolled == 1)
        {
            rolled = roll_die(rng, 10);
        }
        random_state.value = rng;
        return std::string(species[static_cast<std::size_t>(rolled - 2)]);
    }

    VitalState fitted_state(const CharacterSheet &before, const VitalState &state,
                            const CharacterSheet &after) const override
    {
        auto actor = camp_actor(before, state);
        actor.definition = character_definition(character_profile(after, {}).data);
        const auto &d = actor.definition;
        // A species' own casts (a Tiefling's legacy spells) go with the old body.
        cap(actor.free_casts, free_cast_capacity(d));
        actor.initiate_casts &= initiate_capacity(d);
        cap(actor.lay_on_hands, lay_capacity(d));
        actor.life.hp = std::min(actor.life.hp, max_hp(actor));
        return vitals(actor);
    }

    VitalState simulacrum_state(const CharacterSheet &original) const override
    {
        // The original's statistics with fresh resources, and half its Hit
        // Point maximum, rounded down.
        auto copy = camp_actor(original, {});
        const int maximum = max_hp(copy);
        // Camp effects carry scope 1: there is no encounter to source them.
        detail::apply_spell_benefit(copy.effects, detail::EffectSource{.scope = 1, .actor = 1},
                                    original.name, detail::EffectKind::simulacrum,
                                    maximum - maximum / 2);
        copy.life.hp = max_hp(copy);
        return vitals(copy);
    }

    bool keeps_away(std::span<const CampTarget> party,
                    std::span<const std::string> definitions) const override
    {
        std::set<std::string_view> warded;
        for (const auto &member : party)
        {
            for (const auto &e : camp_actor(member.sheet, member.state).effects.active)
            {
                if (e.kind == detail::EffectKind::circle_ward)
                {
                    warded.insert(detail::ward_types.at(static_cast<std::size_t>(e.dc)));
                }
                // Tiny Hut keeps every creature out.
                else if (e.kind == detail::EffectKind::sheltered)
                {
                    return !definitions.empty();
                }
            }
        }
        return !definitions.empty() &&
               std::all_of(definitions.begin(), definitions.end(),
                           [&](const auto &key)
                           {
                               const auto found = content_->definitions.find(key);
                               return found != content_->definitions.end() &&
                                      warded.contains(found->second.creature_type);
                           });
    }

    bool temple_service_helps(const CharacterSheet &sheet, const VitalState &state,
                              TempleService service) const override
    {
        return temple_helps(camp_actor(sheet, state), service);
    }

    void temple_service(VitalState &state, const CharacterSheet &sheet, TempleService service,
                        RandomState &random_state) const override
    {
        auto patient = camp_actor(sheet, state);
        if (!temple_helps(patient, service))
        {
            throw std::runtime_error("The temple's service would not help the member");
        }
        // The authored temple caster has Wisdom +3. Same SplitMix64 as combat.
        auto rng = random_state.value;
        const auto end_effects = [&](auto &&ends)
        {
            std::erase_if(patient.effects.active,
                          [&](const auto &e)
                          {
                              return ends(e.kind);
                          });
        };
        switch (service)
        {
        case TempleService::cure_light_wounds:
        case TempleService::cure_serious_wounds:
        case TempleService::cure_critical_wounds:
        {
            // Cure Wounds from a level-1, -3 or -5 slot: 2d8 a slot level.
            const int dice = service == TempleService::cure_light_wounds     ? 2
                             : service == TempleService::cure_serious_wounds ? 6
                                                                             : 10;
            int amount = 3;
            for (int i = 0; i < dice; ++i)
            {
                amount += roll_die(rng, 8);
            }
            (void)detail::heal_life(patient.life, amount, max_hp(patient),
                                    !detail::healing_blocked(patient.effects));
            break;
        }
        case TempleService::cure_blindness:
            end_effects(
                [](detail::EffectKind kind)
                {
                    return gives_condition(kind, "blinded");
                });
            break;
        case TempleService::cure_disease:
            end_effects(
                [](detail::EffectKind kind)
                {
                    return kind == detail::EffectKind::contagion;
                });
            break;
        case TempleService::neutralize_poison:
            end_effects(poison);
            break;
        case TempleService::remove_curse:
            end_effects(curse);
            break;
        case TempleService::stone_to_flesh:
            end_effects(
                [](detail::EffectKind kind)
                {
                    return kind == detail::EffectKind::petrified ||
                           kind == detail::EffectKind::stoning;
                });
            break;
        case TempleService::raise_dead:
        case TempleService::resurrection:
        {
            const bool whole = service == TempleService::resurrection;
            bring_back(patient, whole ? max_hp(patient) : 1);
            // As in camp: −4 to D20 Tests, easing by 1 a Long Rest.
            if (detail::can_apply(patient.effects))
            {
                detail::apply_spell_benefit(patient.effects,
                                            detail::EffectSource{.scope = 1, .actor = 1}, "Temple",
                                            detail::EffectKind::returned, 4);
            }
            break;
        }
        case TempleService::heal:
        case TempleService::restoration:
        case TempleService::cure_feeblemind:
            // Curse's Heal, the later games' Restoration and Treasures' Fix's
            // cure of feeblemind help no one under the SRD until their SRD
            // versions (docs/GAMES.md, phase 9), so the check above has
            // refused them.
            break;
        }
        state = vitals(patient);
        random_state.value = rng;
    }

    int hit_point_maximum(const CharacterSheet &sheet, const VitalState &state) const override
    {
        // Unreadable vitals keep the sheet's maximum; validating them is left to
        // the rules that use them, which reject them there.
        try
        {
            return max_hp(camp_actor(sheet, state));
        }
        catch (const std::exception &)
        {
            return sheet.hit_points;
        }
    }

    std::vector<rules::StatusEffectView> status_effects(const CharacterSheet &sheet,
                                                        const VitalState &state) const override
    {
        // Its own effects, such as a Rage or Aid it cast on itself, name no source.
        auto actor = camp_actor(sheet, state);
        actor.source.name = sheet.name;
        return status_effect_views(actor, false);
    }

    // Healing outside combat (CLASS-3): Lay On Hands and the healing spells.
    // A free hand is not required; there is time to stow a shield.
    std::vector<CampAction> camp_actions(const CharacterSheet &sheet,
                                         const VitalState &state) const override
    {
        return camp_actions_of(camp_actor(sheet, state));
    }

    // The camp actions of `actor`: a member as camp_actor() builds it, or one
    // reading a scroll (MAGIC-6).
    std::vector<CampAction> camp_actions_of(const Actor &actor) const
    {
        const auto &d = actor.definition;
        std::vector<CampAction> actions;
        if (actor.life.dead || actor.life.hp == 0)
        {
            return actions;
        }
        if (d.lay_on_hands && actor.lay_on_hands[CharacterClass::paladin] > 0)
        {
            actions.push_back({"lay_on_hands", {"Lay On Hands", {}}});
        }
        if (d.lay_on_hands && actor.lay_on_hands[CharacterClass::paladin] >= 5)
        {
            actions.push_back({"lay_on_hands_cure", {"Lay On Hands: cure Poisoned (5)", {}}});
        }
        // Magical Cunning: a one-minute rite, so a camp action; it needs no
        // target and so takes the whole-party path.
        const auto pact = static_cast<std::size_t>(d.pact_slot_level);
        if (d.magical_cunning && actor.arcane[CharacterClass::warlock] > 0 &&
            actor.slots[pact] < d.slots[pact])
        {
            actions.push_back({"magical_cunning", {"Magical Cunning", {}}, true});
        }
        // Adrenaline Rush outside combat: its Dash goes nowhere, but its
        // Temporary Hit Points last until a Long Rest. With some already, the
        // player keeps them or takes the new ones, as in combat.
        if (d.rushes && actor.rushes > 0)
        {
            if (!actor.life.temporary_hp.amount)
            {
                actions.push_back({"adrenaline_rush", {"Adrenaline Rush", {}}, true});
            }
            else
            {
                actions.push_back({"adrenaline_rush_keep",
                                   {"Adrenaline Rush: keep current Temporary HP", {}},
                                   true});
                actions.push_back(
                    {"adrenaline_rush_use", {"Adrenaline Rush: use new Temporary HP", {}}, true});
            }
        }
        if (d.str_dex_disadvantage)
        {
            return actions;
        }
        // Revivify in camp, right after a fight, on a member dead less than a
        // minute; the target list holds the dead.
        if (const auto &raise = *detail::find_spell("revivify");
            detail::knows_spell(d.spells, raise.id))
        {
            for (const unsigned level : cast_levels(actor, raise))
            {
                auto [verb, label] = cast_at(raise, level, "revivify", "Revivify");
                actions.push_back({std::move(verb), {std::move(label), {}}});
            }
        }
        // Magic Circle in camp: a ward against one creature type, an hour and
        // one more for each slot level above 3, for the whole party's rest.
        if (const auto &circle = *detail::find_spell("magic_circle");
            detail::knows_spell(d.spells, circle.id))
        {
            for (const unsigned level : cast_levels(actor, circle))
            {
                for (std::size_t type = 0; type < detail::ward_types.size(); ++type)
                {
                    auto [verb, label] = cast_at(
                        circle, level, "magic_circle_" + std::string(detail::ward_types[type]),
                        "Magic Circle: " + ward_label(type));
                    actions.push_back({std::move(verb), {std::move(label), {}}, true});
                }
            }
        }
        for (const auto &spell : detail::spell_table)
        {
            // Aid's and Death Ward's eight hours outlast a fight, so they are
            // cast in camp too; so is Remove Curse, as in the original, to put
            // down a cursed item (MAGIC-1).
            // Protection From Normal Missiles' hour outlasts a fight too (MAGIC-6).
            if ((spell.pattern != detail::SpellPattern::heal &&
                 spell.pattern != detail::SpellPattern::camp && spell.rider != detail::Rider::aid &&
                 spell.rider != detail::Rider::death_ward &&
                 spell.rider != detail::Rider::regenerate &&
                 spell.rider != detail::Rider::remove_curse &&
                 spell.rider != detail::Rider::missile_ward) ||
                !detail::knows_spell(d.spells, spell.id))
            {
                continue;
            }
            // Contingency: each spell of level 5 or lower the caster can cast
            // on itself with an action, with each trigger (LEVELS-3).
            if (spell.id == "contingency")
            {
                if (!lowest_slot(actor, 6))
                {
                    continue;
                }
                // A slot for Contingency, and another for the stored spell.
                auto after = actor;
                --after.slots[lowest_slot(actor, 6)];
                for (const auto &stored : detail::spell_table)
                {
                    if (contingent(stored) && detail::knows_spell(d.spells, stored.id) &&
                        lowest_slot(after, stored.level))
                    {
                        for (std::size_t trigger = 0; trigger < detail::contingency_triggers.size();
                             ++trigger)
                        {
                            actions.push_back(
                                {"contingency_" + std::string(stored.id) + "_" +
                                     std::string(detail::contingency_triggers[trigger]),
                                 {"Contingency: {spell} ({trigger})",
                                  {{"spell", std::string(stored.label), true},
                                   {"trigger", std::string(contingency_label(trigger)), true}}},
                                 true});
                        }
                    }
                }
                continue;
            }
            // Simulacrum: a copy of a member, and the repair of one; a
            // simulacrum does neither.
            if (spell.id == "simulacrum")
            {
                if (detail::has_effect(actor.effects, detail::EffectKind::simulacrum))
                {
                    continue;
                }
                actions.push_back(
                    {"simulacrum_repair", {"Repair a simulacrum (100 gp per Hit Point)", {}}});
            }
            const bool party = spell.instances > 1;
            for (const unsigned level : cast_levels(actor, spell))
            {
                auto [verb, label] =
                    cast_at(spell, level, std::string(spell.id), std::string(spell.label));
                actions.push_back({std::move(verb), {std::move(label), {}}, party});
            }
            const std::string label(spell.label);
            if (actor.initiate_casts & initiate_bit(d, spell.id))
            {
                actions.push_back({std::string(spell.id) + std::string(detail::initiate_suffix),
                                   {label + " (Magic Initiate)", {}},
                                   party});
            }
        }
        return actions;
    }

    void use_camp_action(const CharacterSheet &user, VitalState &user_state,
                         const CharacterSheet &target, VitalState &target_state,
                         std::string_view action, RandomState &random_state) const override
    {
        use_camp_action_of(camp_actor(user, user_state), user.name, user_state, target,
                           target_state, action, random_state);
    }

    // use_camp_action() for `caster`, a member or one reading a scroll.
    void use_camp_action_of(Actor caster, const std::string &user_name, VitalState &user_state,
                            const CharacterSheet &target, VitalState &target_state,
                            std::string_view action, RandomState &random_state) const
    {
        const auto offered = camp_actions_of(caster);
        if (std::none_of(offered.begin(), offered.end(),
                         [&](const auto &a)
                         {
                             return a.id == action && !a.whole_party;
                         }))
        {
            throw std::runtime_error("That spell or feature cannot be used now");
        }
        const bool self = &user_state == &target_state;
        auto healed = self ? caster : camp_actor(target, target_state);
        auto &patient = self ? caster : healed;
        if (action == "lay_on_hands_cure")
        {
            // 5 points end Poisoned on a living member (SRD 5.2.1 p. 61).
            if (patient.life.dead || !has_condition(patient, "poisoned"))
            {
                throw std::runtime_error("Lay On Hands cures only a Poisoned member");
            }
            caster.lay_on_hands[CharacterClass::paladin] -= 5;
            std::erase_if(patient.effects.active,
                          [](const auto &e)
                          {
                              return gives_condition(e.kind, "poisoned");
                          });
            user_state = vitals(caster);
            if (!self)
            {
                target_state = vitals(healed);
            }
            return;
        }
        if (const auto *raise = detail::find_spell(action);
            raise && raise->rider == detail::Rider::revivify)
        {
            if (self || !patient.life.dead ||
                !detail::has_effect(patient.effects, detail::EffectKind::fallen) ||
                detail::has_effect(patient.effects, detail::EffectKind::dust))
            {
                throw std::runtime_error("Revivify needs a member dead less than a minute");
            }
            pay_for_spell(caster, action, *raise);
            std::erase_if(patient.effects.active,
                          [](const auto &e)
                          {
                              return e.kind == detail::EffectKind::fallen;
                          });
            patient.life.dead = false;
            patient.life.successes = patient.life.failures = 0;
            patient.life.stable = false;
            patient.life.recovery = {};
            patient.life.hp = 1;
            user_state = vitals(caster);
            target_state = vitals(healed);
            return;
        }
        // Simulacrum: a copy of a living member that is no simulacrum itself;
        // the campaign adds the copy to the party (LEVELS-3).
        if (const auto *copy = detail::find_spell(action); copy && copy->id == "simulacrum")
        {
            if (patient.life.dead ||
                detail::has_effect(patient.effects, detail::EffectKind::simulacrum))
            {
                throw std::runtime_error("Simulacrum copies a living member that is no simulacrum");
            }
            pay_for_spell(caster, action, *copy);
            user_state = vitals(caster);
            return;
        }
        // A repair restores all the simulacrum's Hit Points, 100 gp each.
        if (action == "simulacrum_repair")
        {
            if (!detail::has_effect(patient.effects, detail::EffectKind::simulacrum) ||
                patient.life.dead || patient.life.hp >= max_hp(patient))
            {
                throw std::runtime_error("Only a damaged simulacrum can be repaired");
            }
            patient.life.hp = max_hp(patient);
            target_state = vitals(healed);
            return;
        }
        // Resurrection: an hour's casting on any dead member, who returns with
        // all its Hit Points (LEVELS-3: even one turned to dust). Reincarnate
        // forms a new body, whole and without Raise Dead's penalty, for a
        // member dead up to ten days; a simulacrum is no Humanoid.
        const auto *raise = detail::find_spell(action);
        const bool resurrection = raise && raise->id == "resurrection";
        const bool reincarnation = raise && raise->id == "reincarnate";
        if (raise && (raise->id == "raise_dead" || resurrection || reincarnation))
        {
            if (self || !patient.life.dead ||
                (!resurrection &&
                 (!detail::has_effect(patient.effects, detail::EffectKind::deceased) ||
                  detail::has_effect(patient.effects, detail::EffectKind::dust))) ||
                (reincarnation &&
                 detail::has_effect(patient.effects, detail::EffectKind::simulacrum)))
            {
                throw std::runtime_error(resurrection ? "Resurrection needs a dead member"
                                         : reincarnation
                                             ? "Reincarnate needs a Humanoid dead up to ten days"
                                             : "Raise Dead needs a member dead up to ten days");
            }
            pay_for_spell(caster, action, *raise);
            bring_back(patient, resurrection || reincarnation ? max_hp(patient) : 1);
            if (reincarnation)
            {
                user_state = vitals(caster);
                target_state = vitals(healed);
                return;
            }
            // Camp effects carry scope 1: there is no encounter to source them.
            if (detail::can_apply(patient.effects))
            {
                detail::apply_spell_benefit(patient.effects,
                                            detail::EffectSource{.scope = 1, .actor = 1}, user_name,
                                            detail::EffectKind::returned, 4);
            }
            user_state = vitals(caster);
            target_state = vitals(healed);
            return;
        }
        // Identify spends its slot; the campaign reveals the item it touches.
        if (const auto *identify = detail::find_spell(action);
            identify && identify->id == "identify")
        {
            pay_for_spell(caster, action, *identify);
            user_state = vitals(caster);
            return;
        }
        // Remove Curse ends the member's curses; the campaign frees it from
        // its cursed items.
        if (const auto *uncurse = detail::find_spell(action);
            uncurse && uncurse->rider == detail::Rider::remove_curse)
        {
            if (patient.life.dead)
            {
                throw std::runtime_error("Remove Curse needs a living member");
            }
            pay_for_spell(caster, action, *uncurse);
            std::erase_if(patient.effects.active,
                          [](const auto &e)
                          {
                              return curse(e.kind);
                          });
            user_state = vitals(caster);
            if (!self)
            {
                target_state = vitals(healed);
            }
            return;
        }
        if (const auto *ward = detail::find_spell(action);
            ward && ward->rider == detail::Rider::missile_ward)
        {
            if (patient.life.dead ||
                detail::has_effect(patient.effects, detail::EffectKind::missile_ward) ||
                !detail::can_apply(patient.effects))
            {
                throw std::runtime_error(
                    "Protection From Normal Missiles needs a living member without it");
            }
            pay_for_spell(caster, action, *ward);
            // Camp effects carry scope 1: there is no encounter to source them.
            detail::apply_spell_benefit(patient.effects,
                                        detail::EffectSource{.scope = 1, .actor = 1}, user_name,
                                        detail::EffectKind::missile_ward, 0);
            user_state = vitals(caster);
            if (!self)
            {
                target_state = vitals(healed);
            }
            return;
        }
        if (const auto *ward = detail::find_spell(action);
            ward && ward->rider == detail::Rider::death_ward)
        {
            if (patient.life.dead ||
                detail::has_effect(patient.effects, detail::EffectKind::death_ward) ||
                !detail::can_apply(patient.effects))
            {
                throw std::runtime_error("Death Ward needs a living member without it");
            }
            pay_for_spell(caster, action, *ward);
            // Camp effects carry scope 1: there is no encounter to source them.
            detail::apply_spell_benefit(patient.effects,
                                        detail::EffectSource{.scope = 1, .actor = 1}, user_name,
                                        detail::EffectKind::death_ward, 0);
            user_state = vitals(caster);
            if (!self)
            {
                target_state = vitals(healed);
            }
            return;
        }
        const int maximum = max_hp(patient);
        if (patient.life.dead || patient.life.hp >= maximum)
        {
            throw std::runtime_error("Healing requires a wounded living member");
        }
        if (detail::has_effect(patient.effects, detail::EffectKind::simulacrum))
        {
            throw std::runtime_error("Only a repair restores a simulacrum's Hit Points");
        }
        const bool can_heal = !detail::healing_blocked(patient.effects);
        if (const auto *regenerate = detail::find_spell(action);
            regenerate && regenerate->rider == detail::Rider::regenerate)
        {
            // An hour of regaining 1 Hit Point a turn heals a member fully.
            pay_for_spell(caster, action, *regenerate);
            (void)detail::heal_life(patient.life, maximum - patient.life.hp, maximum, can_heal);
            user_state = vitals(caster);
            if (!self)
            {
                target_state = vitals(healed);
            }
            return;
        }
        auto rng = random_state.value;
        if (action == "lay_on_hands")
        {
            // Only the Hit Points actually restored are spent from the pool.
            caster.lay_on_hands[CharacterClass::paladin] -= detail::heal_life(
                patient.life,
                std::min(caster.lay_on_hands[CharacterClass::paladin], maximum - patient.life.hp),
                maximum, can_heal);
        }
        else if (const auto &spell = *detail::find_spell(action); spell.id == "goodberry")
        {
            pay_for_spell(caster, action, spell);
            (void)detail::heal_life(patient.life, std::min(10, maximum - patient.life.hp), maximum,
                                    can_heal);
        }
        else
        {
            pay_for_spell(caster, action, spell);
            // Disciple of Life adds 2 + the slot's level.
            int amount = caster.definition.life_domain
                             ? 2 + static_cast<int>(slot_level_of(action, spell))
                             : 0;
            if (spell.id == "heal")
            {
                // Heal restores 70, 10 more for each slot level above 6, as in
                // a fight, and ends Blinded, Poisoned and Befuddlement.
                amount += 70 + 10 * static_cast<int>(upcast_steps(action, spell));
                std::erase_if(patient.effects.active,
                              [](const auto &e)
                              {
                                  return gives_condition(e.kind, "blinded") ||
                                         gives_condition(e.kind, "poisoned") ||
                                         e.kind == detail::EffectKind::befuddled;
                              });
            }
            else
            {
                amount += casting_modifier(caster.definition, spell.id);
                const int count = spell.dice.count + static_cast<int>(upcast_steps(action, spell) *
                                                                      spell.upcast.extra_dice);
                for (int n = 0; n < count; ++n)
                {
                    amount += roll_die(rng, spell.dice.sides);
                }
            }
            (void)detail::heal_life(patient.life, std::max(0, amount), maximum, can_heal);
        }
        user_state = vitals(caster);
        if (!self)
        {
            target_state = vitals(healed);
        }
        random_state.value = rng;
    }

    // A member reading the scroll's `entry` (MAGIC-6): as camp_actor() builds
    // it, knowing the spell and with a slot of the level the scroll casts it
    // at, which reading spends in place of the member's own. Empty when the
    // member cannot read the scroll.
    std::optional<Actor> scroll_reader(const CharacterSheet &sheet, const VitalState &state,
                                       const rules::ScrollKey &scroll,
                                       const rules::ScrollSpell &entry) const
    {
        auto reader = camp_actor(sheet, state);
        auto &d = reader.definition;
        const auto *spell = detail::find_spell(entry.id);
        if (!spell || scroll.unidentified ||
            (!(scroll.arcane ? d.reads_arcane_scrolls : d.reads_divine_scrolls) &&
             !d.use_magic_device))
        {
            return std::nullopt;
        }
        if (!detail::knows_spell(d.spells, spell->id))
        {
            d.spells.emplace_back(spell->id);
        }
        // A Warlock reads a scroll's spell at the scroll's level, not its
        // Pact Magic slots' level.
        d.pact_magic = false;
        if (const unsigned level = std::max(entry.level, spell->level))
        {
            ++reader.slots[level];
            d.slots[level] = std::max(d.slots[level], reader.slots[level]);
        }
        return reader;
    }

    // Whether `action` reads the scroll's `entry`: its spell at its level.
    static bool reads_entry(std::string_view action, const rules::ScrollSpell &entry)
    {
        const auto *spell = detail::find_spell(action);
        return spell && spell->id == entry.id &&
               slot_level_of(action, *spell) == std::max(entry.level, spell->level);
    }

    std::vector<CampAction> scroll_actions(const CharacterSheet &sheet, const VitalState &state,
                                           std::string_view key) const override
    {
        std::vector<CampAction> actions;
        const auto scroll = rules::parse_scroll_key(key);
        if (!scroll)
        {
            return actions;
        }
        // Each action names the spell's place on the scroll: "1:cure_wounds".
        for (unsigned n = 0; n < scroll->spells.size(); ++n)
        {
            const auto reader = scroll_reader(sheet, state, *scroll, scroll->spells[n]);
            if (!reader)
            {
                continue;
            }
            for (auto &action : camp_actions_of(*reader))
            {
                if (reads_entry(action.id, scroll->spells[n]))
                {
                    action.id = std::to_string(n) + ":" + action.id;
                    actions.push_back(std::move(action));
                }
            }
        }
        return actions;
    }

    // The reader of one of scroll_actions(), and the camp action it uses.
    std::pair<Actor, std::string_view> scroll_caster(const CharacterSheet &sheet,
                                                     const VitalState &state, std::string_view key,
                                                     std::string_view action) const
    {
        const auto scroll = rules::parse_scroll_key(key);
        const auto colon = action.find(':');
        if (scroll && colon == 1 && action[0] >= '0' &&
            static_cast<std::size_t>(action[0] - '0') < scroll->spells.size())
        {
            const auto &entry = scroll->spells[static_cast<std::size_t>(action[0] - '0')];
            const auto verb = action.substr(2);
            if (auto reader = scroll_reader(sheet, state, *scroll, entry);
                reader && reads_entry(verb, entry))
            {
                return {std::move(*reader), verb};
            }
        }
        throw std::runtime_error("That spell cannot be read from the scroll now");
    }

    // Use Magic Device's Arcana check outside combat: true when it fails.
    static bool scroll_check_fails(const Actor &reader, std::string_view verb,
                                   RandomState &random_state)
    {
        const auto &spell = *detail::find_spell(verb);
        const unsigned level = slot_level_of(verb, spell);
        if (!needs_arcana_check(reader.definition, level))
        {
            return false;
        }
        return detail::d20({false, suffers_poisoned(reader), false}, random_state.value) +
                   reader.definition.arcana <
               10 + static_cast<int>(level);
    }

    bool use_scroll(const CharacterSheet &user, VitalState &user_state,
                    const CharacterSheet &target, VitalState &target_state, std::string_view key,
                    std::string_view action, RandomState &random_state) const override
    {
        auto [reader, verb] = scroll_caster(user, user_state, key, action);
        if (scroll_check_fails(reader, verb, random_state))
        {
            return false;
        }
        use_camp_action_of(std::move(reader), user.name, user_state, target, target_state, verb,
                           random_state);
        return true;
    }

    bool use_party_scroll(const CharacterSheet &user, VitalState &user_state,
                          std::span<CampTarget> party, std::string_view key,
                          std::string_view action, RandomState &random_state) const override
    {
        auto [reader, verb] = scroll_caster(user, user_state, key, action);
        if (scroll_check_fails(reader, verb, random_state))
        {
            return false;
        }
        use_party_camp_action_of(std::move(reader), user.name, user_state, party, verb,
                                 random_state);
        return true;
    }

    std::optional<unsigned> copyable_spell_level(const CharacterSheet &sheet,
                                                 std::string_view spell) const override
    {
        const auto *found = detail::find_spell(spell);
        if (!found || found->id != spell || !found->level ||
            detail::class_levels(sheet)[CharacterClass::wizard] == 0 ||
            !detail::on_class_list(detail::CharacterClass::wizard, spell) ||
            found->level >
                (static_cast<unsigned>(detail::class_levels(sheet)[CharacterClass::wizard]) + 1) /
                    2)
        {
            return std::nullopt;
        }
        const auto book = spell_access(sheet).spellbook;
        if (std::any_of(book.begin(), book.end(),
                        [&](const auto &known)
                        {
                            return known.id == spell;
                        }))
        {
            return std::nullopt;
        }
        return found->level;
    }

    std::vector<ScrollWriting> writable_spells(const CharacterSheet &sheet) const override
    {
        const auto d = character_definition(character_profile(sheet, {}).data);
        std::vector<ScrollWriting> writable;
        // The scroll is of the writer's kind of magic, as the scrolls it reads.
        if (!d.reads_arcane_scrolls && !d.reads_divine_scrolls)
        {
            return writable;
        }
        unsigned highest_slot = 0;
        for (unsigned level = 1; level <= max_slot_level; ++level)
        {
            highest_slot = d.slots[level] > 0 ? level : highest_slot;
        }
        for (const auto &id : d.spells)
        {
            const auto *spell = detail::find_spell(id);
            const auto casters =
                detail::casting_classes(detail::ClassHistory(detail::level_class_ids(sheet)));
            if (spell && spell->level &&
                std::any_of(casters.begin(), casters.end(),
                            [&](CharacterClass held)
                            {
                                return detail::on_class_list(held, id);
                            }) &&
                (spell->level <= highest_slot || d.arcanum.contains(id)))
            {
                writable.push_back({id, spell->level, d.reads_arcane_scrolls});
            }
        }
        std::stable_sort(writable.begin(), writable.end(),
                         [](const auto &a, const auto &b)
                         {
                             return a.level < b.level;
                         });
        return writable;
    }

    bool usable_outside_combat(std::string_view item) const override
    {
        return srd5::usable_outside_combat(item);
    }

    std::string equipment_note(const CharacterSheet &sheet, std::string_view item) const override
    {
        return srd5::equipment_note(sheet, item);
    }

    void drink_potion(const CharacterSheet &sheet, VitalState &state, std::string_view item,
                      RandomState &random_state) const override
    {
        const auto *potion = detail::magic_item(item);
        if (!potion || potion->use != detail::ItemUse::drink)
        {
            throw std::runtime_error("Only a potion is drunk");
        }
        auto drinker = camp_actor(sheet, state);
        if (drinker.life.dead)
        {
            throw std::runtime_error("The dead cannot drink");
        }
        const auto &spell = *detail::find_spell(potion->spell);
        if (spell.pattern == detail::SpellPattern::heal)
        {
            const int maximum = max_hp(drinker);
            if (drinker.life.hp >= maximum)
            {
                throw std::runtime_error("Healing requires a wounded living member");
            }
            auto rng = random_state.value;
            int amount = spell.dice.bonus;
            for (int n = 0; n < spell.dice.count; ++n)
            {
                amount += roll_die(rng, spell.dice.sides);
            }
            (void)detail::heal_life(drinker.life, amount, maximum,
                                    !detail::healing_blocked(drinker.effects));
            random_state.value = rng;
        }
        else if (spell.rider == detail::Rider::giant_strength)
        {
            if (detail::has_effect(drinker.effects, detail::EffectKind::giant_strength))
            {
                throw std::runtime_error("Its Strength is already a giant's");
            }
            // Effects that last past a fight carry scope 1, as camp effects do.
            detail::apply_spell_benefit(drinker.effects,
                                        detail::EffectSource{.scope = 1, .actor = 1}, sheet.name,
                                        detail::EffectKind::giant_strength, 0);
        }
        else
        {
            throw std::runtime_error("That potion works only in combat");
        }
        state = vitals(drinker);
    }

    bool can_cast_exploration_spell(const CharacterSheet &sheet, const VitalState &state,
                                    std::string_view id) const override
    {
        const auto actor = camp_actor(sheet, state);
        const auto *spell = detail::find_spell(id);
        return spell && spell->pattern == detail::SpellPattern::exploration && !actor.life.dead &&
               actor.life.hp > 0 && !actor.definition.str_dex_disadvantage &&
               detail::knows_spell(actor.definition.spells, id) &&
               actor.slots[spell->level >= 2 ? 2 : 1] > 0;
    }

    void cast_exploration_spell(const CharacterSheet &sheet, VitalState &state,
                                std::string_view id) const override
    {
        if (!can_cast_exploration_spell(sheet, state, id))
        {
            throw std::runtime_error("That spell cannot be cast now");
        }
        auto caster = camp_actor(sheet, state);
        --caster.slots[detail::find_spell(id)->level >= 2 ? 2 : 1];
        state = vitals(caster);
    }

    // Aid in camp: the game chooses, as Pool of Radiance's group spells do,
    // the three members with the lowest Hit Point maximum not already aided
    // (AID-1).
    static void camp_aid(const std::string &caster_name, std::vector<Actor *> members, int raise)
    {
        std::erase_if(members,
                      [](const Actor *a)
                      {
                          return a->life.dead ||
                                 detail::has_effect(a->effects, detail::EffectKind::aid) ||
                                 !detail::can_apply(a->effects);
                      });
        if (members.empty())
        {
            throw std::runtime_error("No member can be helped by it now");
        }
        std::stable_sort(members.begin(), members.end(),
                         [](const Actor *x, const Actor *y)
                         {
                             return max_hp(*x) < max_hp(*y);
                         });
        if (members.size() > 3)
        {
            members.resize(3);
        }
        for (auto *member : members)
        {
            // Camp effects carry scope 1: there is no encounter to source them.
            detail::apply_spell_benefit(member->effects,
                                        detail::EffectSource{.scope = 1, .actor = 1}, caster_name,
                                        detail::EffectKind::aid, raise);
            (void)detail::heal_life(member->life, raise, max_hp(*member),
                                    !detail::healing_blocked(member->effects));
        }
    }

    // Prayer of Healing: the five most hurt members it has not healed since
    // their last Long Rest each regain 2d8 + the spellcasting modifier.
    void use_party_camp_action(const CharacterSheet &user, VitalState &user_state,
                               std::span<CampTarget> party, std::string_view action,
                               RandomState &random_state) const override
    {
        use_party_camp_action_of(camp_actor(user, user_state), user.name, user_state, party, action,
                                 random_state);
    }

    // use_party_camp_action() for `caster`, a member or one reading a scroll.
    void use_party_camp_action_of(Actor caster, const std::string &user_name,
                                  VitalState &user_state, std::span<CampTarget> party,
                                  std::string_view action, RandomState &random_state) const
    {
        const auto offered = camp_actions_of(caster);
        if (std::none_of(offered.begin(), offered.end(),
                         [&](const auto &a)
                         {
                             return a.id == action && a.whole_party;
                         }))
        {
            throw std::runtime_error("That spell or feature cannot be used now");
        }
        if (action.starts_with("magic_circle_"))
        {
            const auto &circle = *detail::find_spell(action);
            pay_for_spell(caster, action, circle);
            const auto type = static_cast<int>(ward_of(action));
            std::erase_if(caster.effects.active,
                          [&](const auto &e)
                          {
                              return e.kind == detail::EffectKind::circle_ward && e.dc == type;
                          });
            // Camp effects carry scope 1: there is no encounter to source them.
            detail::apply_spell_benefit(caster.effects,
                                        detail::EffectSource{.scope = 1, .actor = 1}, user_name,
                                        detail::EffectKind::circle_ward, type);
            caster.effects.active.back().remaining_ms =
                static_cast<unsigned>(3600000 * (slot_level_of(action, circle) - 2));
            user_state = vitals(caster);
            return;
        }
        // Detect Magic spends its slot; the campaign marks magic items for its
        // ten minutes. Friends, Read Magic and Find Traps likewise leave their
        // effect to the campaign (MAGIC-6).
        if (const auto *detect = detail::find_spell(action);
            detect && (detect->id == "detect_magic" || detect->id == "friends" ||
                       detect->id == "read_magic" || detect->id == "find_traps"))
        {
            pay_for_spell(caster, action, *detect);
            user_state = vitals(caster);
            return;
        }
        if (action == "magical_cunning")
        {
            // Back come expended Pact Magic slots, up to half the maximum
            // (rounded up), at their own level.
            const auto &d = caster.definition;
            const auto level = static_cast<std::size_t>(d.pact_slot_level);
            caster.slots[level] =
                std::min(d.slots[level], caster.slots[level] + (d.pact_slots + 1) / 2);
            --caster.arcane[CharacterClass::warlock];
            user_state = vitals(caster);
            return;
        }
        if (action.starts_with("adrenaline_rush"))
        {
            // Keeping the current Temporary Hit Points still spends the use.
            --caster.rushes;
            detail::grant_temporary_hp(
                caster.life, {caster.definition.rushes, std::string(rush_source)},
                action == "adrenaline_rush_keep" ? TemporaryHpChoice::keep_current
                                                 : TemporaryHpChoice::use_new);
            user_state = vitals(caster);
            return;
        }
        std::vector<std::pair<Actor, VitalState *>> members;
        for (const auto &member : party)
        {
            if (&member.state.get() != &user_state)
            {
                members.emplace_back(camp_actor(member.sheet, member.state), &member.state.get());
            }
        }
        std::vector<Actor *> chosen{&caster};
        for (auto &[actor, state] : members)
        {
            chosen.push_back(&actor);
        }
        if (std::none_of(party.begin(), party.end(),
                         [&](const auto &member)
                         {
                             return &member.state.get() == &user_state;
                         }))
        {
            chosen.erase(chosen.begin());
        }
        const auto &spell = *detail::find_spell(action);
        if (spell.id == "word_of_recall" || spell.id == "teleport")
        {
            // The campaign moves the party (LEVELS-3); the rules spend the slot.
            pay_for_spell(caster, action, spell);
            user_state = vitals(caster);
            return;
        }
        if (spell.id == "contingency")
        {
            // Both slots are spent; the stored spell waits on the caster.
            const auto rest = action.substr(std::string_view("contingency_").size());
            const auto split = rest.rfind('_');
            const auto *stored = detail::find_spell(rest.substr(0, split));
            const auto trigger =
                std::find(detail::contingency_triggers.begin(), detail::contingency_triggers.end(),
                          rest.substr(split + 1));
            if (!stored || !contingent(*stored) || trigger == detail::contingency_triggers.end())
            {
                throw std::runtime_error("That contingency cannot be set");
            }
            pay_for_spell(caster, cast_at(spell, lowest_slot(caster, 6), "contingency", "").first,
                          spell);
            pay_for_spell(
                caster,
                cast_at(*stored, lowest_slot(caster, stored->level), std::string(stored->id), "")
                    .first,
                *stored);
            std::erase_if(caster.effects.active,
                          [](const auto &e)
                          {
                              return e.kind == detail::EffectKind::contingency;
                          });
            // Camp effects carry scope 1: there is no encounter to source them.
            if (detail::can_apply(caster.effects))
            {
                detail::apply_spell_benefit(
                    caster.effects, detail::EffectSource{.scope = 1, .actor = 1}, user_name,
                    detail::EffectKind::contingency,
                    static_cast<int>(stored - detail::spell_table.data()) * 4 +
                        static_cast<int>(trigger - detail::contingency_triggers.begin()));
            }
            user_state = vitals(caster);
            return;
        }
        if (spell.id == "tiny_hut")
        {
            // Eight hours in which no rest is interrupted (LEVELS-1).
            pay_for_spell(caster, action, spell);
            std::erase_if(caster.effects.active,
                          [](const auto &e)
                          {
                              return e.kind == detail::EffectKind::sheltered;
                          });
            // Camp effects carry scope 1: there is no encounter to source them.
            if (detail::can_apply(caster.effects))
            {
                detail::apply_spell_benefit(caster.effects,
                                            detail::EffectSource{.scope = 1, .actor = 1}, user_name,
                                            detail::EffectKind::sheltered, 0);
            }
            user_state = vitals(caster);
            return;
        }
        if (spell.id == "heroes_feast")
        {
            // Up to twelve members, each with 2d10 more Hit Points at most and
            // as many regained, Poison Resistance and Immunity to Frightened
            // and Poisoned for a day.
            std::erase_if(chosen,
                          [](const Actor *a)
                          {
                              return a->life.dead || !detail::can_apply(a->effects);
                          });
            if (chosen.empty())
            {
                throw std::runtime_error("No member can partake of the feast now");
            }
            pay_for_spell(caster, action, spell);
            auto rng = random_state.value;
            for (auto *member : chosen)
            {
                const int raise = roll_die(rng, 10) + roll_die(rng, 10);
                std::erase_if(member->effects.active,
                              [](const auto &e)
                              {
                                  return e.kind == detail::EffectKind::feasted ||
                                         gives_condition(e.kind, "poisoned");
                              });
                // Camp effects carry scope 1: there is no encounter to source them.
                detail::apply_spell_benefit(member->effects,
                                            detail::EffectSource{.scope = 1, .actor = 1}, user_name,
                                            detail::EffectKind::feasted, raise);
                (void)detail::heal_life(member->life, raise, max_hp(*member),
                                        !detail::healing_blocked(member->effects));
            }
            user_state = vitals(caster);
            for (auto &[actor, state] : members)
            {
                *state = vitals(actor);
            }
            random_state.value = rng;
            return;
        }
        if (spell.rider == detail::Rider::aid)
        {
            camp_aid(user_name, chosen, 5 + 5 * static_cast<int>(upcast_steps(action, spell)));
            pay_for_spell(caster, action, spell);
            user_state = vitals(caster);
            for (auto &[actor, state] : members)
            {
                *state = vitals(actor);
            }
            return;
        }
        std::erase_if(chosen,
                      [](const Actor *a)
                      {
                          return a->life.dead || a->life.hp >= max_hp(*a) ||
                                 detail::has_effect(a->effects,
                                                    detail::EffectKind::prayer_of_healing);
                      });
        if (chosen.empty())
        {
            throw std::runtime_error("No member can be healed by it now");
        }
        std::sort(chosen.begin(), chosen.end(),
                  [](const Actor *x, const Actor *y)
                  {
                      return x->life.hp * max_hp(*y) < y->life.hp * max_hp(*x);
                  });
        if (chosen.size() > 5)
        {
            chosen.resize(5);
        }
        pay_for_spell(caster, action, spell);
        auto rng = random_state.value;
        for (auto *patient : chosen)
        {
            // Disciple of Life adds 2 + the slot's level.
            int amount =
                casting_modifier(caster.definition) +
                (caster.definition.life_domain ? 2 + static_cast<int>(slot_level_of(action, spell))
                                               : 0);
            const int count = spell.dice.count + static_cast<int>(upcast_steps(action, spell) *
                                                                  spell.upcast.extra_dice);
            for (int n = 0; n < count; ++n)
            {
                amount += roll_die(rng, spell.dice.sides);
            }
            (void)detail::heal_life(patient->life, std::max(0, amount), max_hp(*patient),
                                    !detail::healing_blocked(patient->effects));
            // Camp effects carry scope 1: there is no encounter to source them.
            detail::apply_spell_benefit(patient->effects,
                                        detail::EffectSource{.scope = 1, .actor = 1}, user_name,
                                        detail::EffectKind::prayer_of_healing, 0);
        }
        user_state = vitals(caster);
        for (auto &[actor, state] : members)
        {
            *state = vitals(actor);
        }
        random_state.value = rng;
    }

    SpellChoiceOptions spell_choice_options(const CharacterSheet &sheet,
                                            SpellChoiceContext context) const override
    {
        return detail::spell_choice_options(sheet, context);
    }

    void apply_spell_choices(CharacterSheet &sheet, const SpellChoices &choice,
                             SpellChoiceContext context,
                             ChoiceCompleteness completeness) const override
    {
        detail::apply_spell_choices(sheet, choice, context, completeness);
    }

    SpellAccess spell_access(const CharacterSheet &sheet) const override
    {
        return detail::spell_access(sheet.grants,
                                    detail::ClassHistory(detail::level_class_ids(sheet)),
                                    sheet.prepared_spells);
    }

    EquipmentInfo equipment_info(std::string_view key) const override
    {
        auto info = equipment_slot(key);
        info.weight_hundredths = srd_weight_hundredths(key);
        return info;
    }

    // SRD 5.2.1 Carrying Capacity: Strength times 15 pounds for a Small or
    // Medium creature. A Goliath's Powerful Build counts it one size larger,
    // which doubles it.
    unsigned carrying_capacity_pounds(const CharacterSheet &sheet) const override
    {
        const bool powerful_build = std::any_of(sheet.grants.begin(), sheet.grants.end(),
                                                [](const auto &grant)
                                                {
                                                    return grant.id == "trait:powerful_build";
                                                });
        const auto strength = std::max(0, sheet.scores[rules::Ability::strength]);
        return static_cast<unsigned>(strength) * (powerful_build ? 30u : 15u);
    }

    // Untrained weapons, armor and shields, a scroll of the kind the class
    // cannot read, an item with no SRD conversion, and an identified sword
    // bound to another alignment are not fully usable. What is not yet
    // identified says nothing of its magic.
    bool can_use_item(const CharacterSheet &sheet, std::string_view key) const override
    {
        if (key.starts_with("por:unsupported:"))
        {
            return false;
        }
        if (const auto scroll = rules::parse_scroll_key(key))
        {
            // Any of the character's classes reads it (MULTI-1).
            const auto levels = detail::class_levels(sheet);
            for (const auto &choice : detail::creation_choices(CreationField::character_class))
            {
                const auto held = detail::class_from_id(choice.id);
                if (levels[held] &&
                    (scroll->arcane ? reads_arcane_scrolls(held) : reads_divine_scrolls(held)))
                {
                    return true;
                }
            }
            return detail::has_grant(sheet.grants, "feature:use_magic_device");
        }
        if (const auto item = rules::parse_item_key(key); !item.unidentified)
        {
            auto alignment = sheet.alignment;
            std::transform(alignment.begin(), alignment.end(), alignment.begin(),
                           [](unsigned char c)
                           {
                               return static_cast<char>(std::tolower(c));
                           });
            if ((item.power == rules::ItemPower::good && !alignment.ends_with("good")) ||
                (item.power == rules::ItemPower::evil && !alignment.ends_with("evil")))
            {
                return false;
            }
        }
        if (detail::weapon(key) || detail::armor(key))
        {
            return trained(detail::class_of(sheet), sheet.grants, key);
        }
        return true;
    }

    // The SRD's weight for a weapon, armor, a shield or adventuring gear the
    // game converts to; none for a magic item, which the SRD gives no weight.
    static std::optional<unsigned> srd_weight_hundredths(std::string_view key)
    {
        if (const auto *item = detail::weapon(key))
        {
            return item->weight_quarters * 25;
        }
        if (const auto *item = detail::armor(key))
        {
            return item->weight_pounds * 100;
        }
        // A flask of Acid, Alchemist's Fire or Oil, a Torch and Thieves'
        // Tools weigh 1 pound each, a Poisoner's Kit 2.
        if (detail::thrown_gear(key) || key == "torch" || key == thieves_tools)
        {
            return 100;
        }
        if (key == poisoners_kit)
        {
            return 200;
        }
        return std::nullopt;
    }

    static EquipmentInfo equipment_slot(std::string_view key)
    {
        // Magic Ink is spent writing or copying a scroll (MAGIC-6); a scroll
        // is read from the pack, and a Poisoner's Kit, Basic Poison, Thieves'
        // Tools or a Wand of Secrets used from it.
        if (detail::thrown_gear(key) || rules::magic_ink_level(key) ||
            rules::parse_scroll_key(key) || key == poisoners_kit || key == basic_poison ||
            key == thieves_tools || key == magic_lock_picks || key == stone_of_good_luck ||
            key == "wand_of_secrets")
        {
            return {EquipmentSlot::carried, 0};
        }
        if (const auto *item = detail::weapon(key))
        {
            return {.slot = EquipmentSlot::weapon, .hands = item->hands, .ranged = item->ranged};
        }
        if (detail::is_shield(key))
        {
            return {EquipmentSlot::shield, 1};
        }
        if (detail::armor(key))
        {
            return {EquipmentSlot::armor, 0};
        }
        if (detail::worn_item(key))
        {
            return {EquipmentSlot::worn, 0};
        }
        return {};
    }

    std::vector<EquipmentChoice> equipment_choices(const CharacterSheet &sheet,
                                                   std::span<const std::string> candidates,
                                                   unsigned selected) const override
    {
        if (selected >= candidates.size())
        {
            throw std::runtime_error("Unknown equipment candidate");
        }
        const auto *chosen = detail::weapon(candidates[selected]);
        if (!chosen || chosen->hands != 1)
        {
            return {};
        }
        std::vector<unsigned> held;
        for (unsigned i = 0; i < candidates.size(); ++i)
        {
            if (i != selected && detail::weapon(candidates[i]))
            {
                held.push_back(i);
            }
        }
        if (held.empty())
        {
            return {};
        }
        std::vector<EquipmentChoice> result;
        for (unsigned n = 0; n < 2; ++n)
        {
            const auto op = n ? EquipmentOperation::equip_other : EquipmentOperation::equip_main;
            EquipmentChoice choice{
                op,
                {n ? "Other hand — {item}" : "Main hand — {item}",
                 {{"item", n < held.size() ? weapon_label(candidates[held[n]]) : "Empty", true}}},
                {},
                true};
            choice.explanation = {"Equip {item} in this hand; replace its current weapon.",
                                  {{"item", weapon_label(candidates[selected]), true}}};
            try
            {
                (void)equipment_change(sheet, candidates, selected, op);
            }
            catch (const std::runtime_error &e)
            {
                choice.available = false;
                choice.explanation = {e.what(), {}};
            }
            result.push_back(std::move(choice));
        }
        return result;
    }

    EquipmentChange equipment_change(const CharacterSheet &sheet,
                                     std::span<const std::string> candidates, unsigned selected,
                                     EquipmentOperation operation) const override
    {
        if (selected >= candidates.size())
        {
            throw std::runtime_error("Unknown equipment candidate");
        }
        const auto slot = equipment_info(candidates[selected]).slot;
        if (operation != EquipmentOperation::unequip && slot == EquipmentSlot::carried)
        {
            throw std::runtime_error("This item is carried, not equipped");
        }
        EquipmentChange result;
        const bool hand = operation == EquipmentOperation::equip_main ||
                          operation == EquipmentOperation::equip_other;
        if (hand)
        {
            const auto *weapon = detail::weapon(candidates[selected]);
            if (!weapon || weapon->hands != 1)
            {
                throw std::runtime_error("Choose a one-handed weapon");
            }
            std::vector<unsigned> held;
            for (unsigned i = 0; i < candidates.size(); ++i)
            {
                if (i != selected)
                {
                    if (detail::weapon(candidates[i]))
                    {
                        held.push_back(i);
                    }
                    else
                    {
                        result.indices.push_back(i);
                    }
                }
            }
            if (held.empty() || held.size() > 2)
            {
                throw std::runtime_error("Invalid equipped hand selection");
            }
            result.indices.push_back(operation == EquipmentOperation::equip_main ? selected
                                                                                 : held[0]);
            if (operation == EquipmentOperation::equip_other)
            {
                result.indices.push_back(selected);
            }
            else if (held.size() > 1)
            {
                result.indices.push_back(held[1]);
            }
            result.separate_selected_unit = true;
        }
        else
        {
            for (unsigned i = 0; i < candidates.size(); ++i)
            {
                if (operation == EquipmentOperation::unequip && i == selected)
                {
                    continue;
                }
                if (operation == EquipmentOperation::equip && slot == EquipmentSlot::weapon &&
                    i != selected && equipment_info(candidates[i]).slot == EquipmentSlot::weapon)
                {
                    continue;
                }
                result.indices.push_back(i);
            }
        }
        std::vector<std::string> gear;
        for (auto i : result.indices)
        {
            gear.push_back(candidates[i]);
        }
        (void)character_profile(sheet, gear);
        return result;
    }

    // A round's melee attacks against the target (EQUIP-1): the Attack
    // action's attacks with the first weapon held, a Sneak Attack once with a
    // Finesse weapon, and the Bonus Action attack with a second Light weapon
    // when the first is Light too.
    GearRating gear_rating(const CharacterSheet &sheet, std::span<const std::string> gear,
                           int target_armor_class) const override
    {
        const auto profile = character_profile(sheet, gear);
        const auto d = character_definition(profile.data);
        GearRating rating{.armor_class = profile.armor_class};
        const auto main = attack_odds(melee_attack(d), target_armor_class);
        const int attacks = 1 + d.extra_attacks;
        rating.round_damage = attacks * main.damage;
        if (d.sneak_level && d.finesse && !d.ranged_weapon)
        {
            const double missed_all = std::pow(1.0 - main.hit_chance, attacks);
            rating.round_damage += (1.0 - missed_all) *
                                   detail::sneak_attack_dice(d.sneak_level).count *
                                   average_die(6, false);
        }
        std::vector<std::size_t> weapons;
        for (std::size_t i = 0; i < gear.size(); ++i)
        {
            if (detail::weapon(gear[i]))
            {
                weapons.push_back(i);
            }
        }
        if (weapons.size() == 2 && detail::weapon(gear[weapons[0]])->light &&
            detail::weapon(gear[weapons[1]])->light)
        {
            // The gear without the first weapon holds the second as its own.
            std::vector<std::string> other_hand(gear.begin(), gear.end());
            other_hand.erase(other_hand.begin() + static_cast<std::ptrdiff_t>(weapons[0]));
            auto attack =
                melee_attack(character_definition(character_profile(sheet, other_hand).data));
            // Without Two-Weapon Fighting the ability adds no positive damage.
            if (!d.two_weapon_fighting)
            {
                attack.damage_bonus -= attack.dice.bonus - std::min(0, attack.dice.bonus);
            }
            rating.round_damage += attack_odds(attack, target_armor_class).damage;
        }
        return rating;
    }

    std::optional<WeaponAttackView> weapon_attack(const CharacterSheet &sheet,
                                                  std::span<const std::string> gear) const override
    {
        const auto key = std::ranges::find_if(gear,
                                              [](const std::string &item)
                                              {
                                                  return detail::weapon(item) != nullptr;
                                              });
        if (key == gear.end())
        {
            return std::nullopt;
        }
        const auto *weapon = detail::weapon(*key);
        const auto d = character_definition(character_profile(sheet, gear).data);
        WeaponAttackView view{.trained = trained(detail::class_of(sheet), sheet.grants, *key)};
        if (weapon->ranged)
        {
            view.attack_bonus = d.ranged_bonus + d.ranged_magic;
            view.dice = d.ranged.count;
            view.sides = d.ranged.sides;
            view.damage_bonus = d.ranged.bonus + d.ranged_magic;
            view.damage_type = detail::damage_name(d.ranged_type);
            view.disadvantage = d.str_dex_disadvantage || d.ranged_heavy_disadvantage;
            return view;
        }
        // A Versatile weapon's one-handed die, and its die with both hands
        // beside it.
        const auto attack = melee_attack(d);
        view.attack_bonus = attack.attack_bonus;
        view.dice = d.melee.count;
        view.sides = d.melee.sides;
        view.two_handed_sides = d.versatile_sides;
        view.damage_bonus = attack.damage_bonus;
        view.disadvantage = attack.disadvantage;
        view.damage_type = detail::damage_name(d.melee_type);
        return view;
    }

    bool suits(const CharacterSheet &sheet, std::string_view key) const override
    {
        const auto *armor = detail::armor(key);
        return !armor || (trained(detail::class_of(sheet), sheet.grants, key) &&
                          sheet.scores[Ability::strength] >= armor->strength);
    }

    AbilityCheckModifier ability_check(const CharacterSheet &sheet,
                                       std::span<const std::string> gear, Ability ability,
                                       std::string_view skill) const override
    {
        const auto d = character_definition(character_profile(sheet, gear).data);
        auto result = detail::creation_rules().ability_check(sheet, ability, skill);
        const bool strength_or_dexterity =
            ability == Ability::strength || ability == Ability::dexterity;
        result.disadvantage =
            (strength_or_dexterity && d.str_dex_disadvantage) ||
            (ability == Ability::dexterity && skill == "stealth" && d.stealth_disadvantage);
        result.advantage |= (skill == "stealth" && d.stealth_advantage) ||
                            (skill == "sleight_of_hand" && d.pick_advantage);
        result.total += d.check_bonus;
        return result;
    }

    unsigned climbing_chance(const CharacterSheet &sheet, const VitalState &state,
                             std::span<const std::string> gear) const override
    {
        const bool heavier =
            std::any_of(gear.begin(), gear.end(),
                        [](const std::string &key)
                        {
                            const auto *armor = detail::armor(key);
                            return armor && (armor->category == detail::ArmorCategory::medium ||
                                             armor->category == detail::ArmorCategory::heavy);
                        });
        if (!detail::class_levels(sheet)[CharacterClass::rogue] || heavier)
        {
            return 0;
        }
        constexpr int climb_dc = 15;
        return check_chance(sheet, state, gear, Ability::strength, "athletics", climb_dc);
    }

    unsigned check_chance(const CharacterSheet &sheet, const VitalState &state,
                          std::span<const std::string> gear, Ability ability,
                          std::string_view skill, int difficulty) const override
    {
        // Weigh the d20 rolls (pairs, with Advantage or Disadvantage) that
        // reach the difficulty, as roll_ability_check rolls them: a Halfling's
        // 1 is rolled again, and Reliable Talent makes a proficient roll at
        // least 10.
        const auto modifier = ability_check(sheet, gear, ability, skill);
        const bool poisoned = suffers_poisoned(camp_actor(sheet, state));
        const bool advantage = modifier.advantage && !(modifier.disadvantage || poisoned);
        const bool disadvantage = !modifier.advantage && (modifier.disadvantage || poisoned);
        const bool lucky = character_definition(character_profile(sheet, gear).data).lucky;
        const bool reliable =
            detail::class_levels(sheet)[CharacterClass::rogue] >= 7 && modifier.proficiency > 0;
        // Out of 400 for one die: 20 a face, or 1 for a lucky 1 and 21 for the rest.
        const auto weight = [&](int face) -> std::uint64_t
        {
            return !lucky ? 20 : face == 1 ? 1 : 21;
        };
        std::uint64_t successes = 0;
        for (int first = 1; first <= 20; ++first)
        {
            for (int second = 1; second <= 20; ++second)
            {
                const int die = advantage      ? std::max(first, second)
                                : disadvantage ? std::min(first, second)
                                               : first;
                if ((reliable ? std::max(die, 10) : die) + modifier.total >= difficulty)
                {
                    successes += weight(first) * weight(second);
                }
            }
        }
        return static_cast<unsigned>((successes * 100 + 80000) / 160000);
    }

    AbilityCheckRoll roll_ability_check(const CharacterSheet &sheet, const VitalState &state,
                                        std::span<const std::string> gear, Ability ability,
                                        std::string_view skill,
                                        RandomState &random_state) const override
    {
        const auto modifier = ability_check(sheet, gear, ability, skill);
        const bool lucky = character_definition(character_profile(sheet, gear).data).lucky;
        const bool poisoned = suffers_poisoned(camp_actor(sheet, state));
        auto rng = random_state.value;
        // Advantage and Disadvantage cancel; either alone rolls a second d20.
        int die = detail::d20({modifier.advantage, modifier.disadvantage || poisoned, lucky}, rng);
        // Reliable Talent: a proficient check's d20 counts as at least 10.
        if (detail::class_levels(sheet)[CharacterClass::rogue] >= 7 && modifier.proficiency > 0)
        {
            die = std::max(die, 10);
        }
        random_state.value = rng;
        return {die, die + modifier.total};
    }

    CharacterProfile character_profile(const CharacterSheet &sheet,
                                       std::span<const std::string> gear) const override
    {
        if (sheet.identity != detail::creation_rules().identity() || sheet.level < 1)
        {
            throw std::runtime_error("Unsupported character rules identity or level");
        }
        const auto klass = detail::class_of(sheet);
        if (static_cast<unsigned>(sheet.level) > detail::level_cap(klass))
        {
            throw std::runtime_error("Unsupported character rules identity or level");
        }
        (void)detail::training_profile(
            sheet.grants, detail::ClassHistory(detail::level_class_ids(sheet)), sheet.race,
            detail::background_id(sheet.background), sheet.scores);
        const auto features_only =
            without_manuals(detail::without_spell_grants(detail::without_training(sheet.grants)));
        const auto access = spell_access(sheet);
        const auto effects = detail::validate_grants(
            features_only, detail::ClassHistory(detail::level_class_ids(sheet)),
            detail::race_id(sheet.race), detail::background_id(sheet.background));
        const unsigned features = effects.feats;
        if (std::cmp_not_equal(sheet.hit_point_modifiers.size(), sheet.level))
        {
            throw std::runtime_error("HP history does not match character advancement");
        }
        // After the background's, one adjustment for each Ability Score
        // Improvement, in level order, together adding what the grants record.
        std::vector<unsigned> asi_levels;
        for (const auto &g : sheet.grants)
        {
            if (g.id == "feat:ability_score_improvement")
            {
                asi_levels.push_back(g.level);
            }
        }
        std::sort(asi_levels.begin(), asi_levels.end());
        // A Manual of Bodily Health read (MAGIC-3) adds its own adjustment.
        const auto manuals =
            static_cast<std::size_t>(std::count_if(sheet.grants.begin(), sheet.grants.end(),
                                                   [](const auto &g)
                                                   {
                                                       return g.id == bodily_health;
                                                   }));
        if (sheet.ability_adjustments.size() != 1 + asi_levels.size() + manuals)
        {
            throw std::runtime_error("Ability sources disagree with acquired grants");
        }
        AbilityArray<int> raised{};
        AbilityArray<int> read{};
        std::size_t next_asi = 0;
        for (std::size_t i = 1; i < sheet.ability_adjustments.size(); ++i)
        {
            const auto &adjustment = sheet.ability_adjustments[i];
            auto &sum = adjustment.source_id == manual_source ? read : raised;
            if (adjustment.source_id == manual_source)
            {
                AbilityArray<int> manual{};
                manual[Ability::constitution] = 2;
                if (adjustment.bonuses != manual)
                {
                    throw std::runtime_error("Ability sources disagree with acquired choices");
                }
            }
            else if (adjustment.source_id != "feat:ability_score_improvement" ||
                     next_asi >= asi_levels.size() || adjustment.level != asi_levels[next_asi++])
            {
                throw std::runtime_error("Ability sources disagree with acquired choices");
            }
            for (const auto ability : all_abilities)
            {
                sum[ability] += adjustment.bonuses[ability];
            }
        }
        if (raised != effects.abilities)
        {
            throw std::runtime_error("Ability sources disagree with acquired choices");
        }
        for (const auto ability : all_abilities)
        {
            const auto bonuses = sheet.ability_adjustments.front().bonuses[ability] +
                                 effects.abilities[ability] + read[ability];
            if (sheet.bonuses[ability] != bonuses ||
                sheet.scores[ability] != sheet.base[ability] + bonuses)
            {
                throw std::runtime_error("Ability totals disagree with acquired choices");
            }
        }
        auto spells = detail::casting_ids(access);
        std::set<std::string> selected;
        const auto record = [&](std::string id)
        {
            if (!detail::knows_spell(spells, id))
            {
                spells.push_back(std::move(id));
            }
        };
        // Preparable spells come from the eligibility table. Cantrips are
        // filtered out: they are known, never prepared.
        // Each class's prepared spells come from its own table at its own
        // level (MULTI-1).
        const auto levels = detail::class_levels(sheet);
        for (const auto prepared_by :
             detail::casting_classes(detail::ClassHistory(detail::level_class_ids(sheet))))
        {
            const auto level = static_cast<unsigned>(levels[prepared_by]);
            const auto preparable =
                detail::spells_of_level(allowed_spells(prepared_by, level), false);
            for (const auto &spell :
                 detail::class_prepared_spells(sheet.prepared_spells, prepared_by, klass))
            {
                if (!selected.insert(spell).second)
                {
                    throw std::runtime_error("Duplicate prepared spell");
                }
                if (!detail::knows_spell(preparable, spell) &&
                    !bard_secret(prepared_by, level, sheet.grants, spell))
                {
                    throw std::runtime_error("Unsupported prepared spell");
                }
                record(spell);
            }
        }
        if (selected.size() != sheet.prepared_spells.size())
        {
            throw std::runtime_error("Unsupported prepared spell");
        }
        std::ostringstream out;
        out << profile_magic << ' ' << sheet.level << ' ' << features << ' ' << spells.size();
        for (const auto &id : spells)
        {
            out << ' ' << id;
        }
        out << ' ' << std::quoted(sheet.character_class) << ' ' << std::quoted(sheet.race);
        for (auto score : sheet.scores)
        {
            out << ' ' << score;
        }
        for (auto modifier : sheet.hit_point_modifiers)
        {
            out << ' ' << modifier;
        }
        // The class of each level (MULTI-1).
        for (const auto &id : detail::level_class_ids(sheet))
        {
            out << ' ' << id;
        }
        out << ' ' << gear.size();
        for (const auto &item : gear)
        {
            out << ' ' << std::quoted(item);
        }
        out << ' ' << std::quoted(detail::background_id(sheet.background));
        detail::write_grants(out, sheet.grants);
        out << ' ' << std::quoted(sheet.alignment);
        auto data = out.str();
        const auto d = character_definition(data);
        const int proficiency = 2 + (static_cast<int>(sheet.level) - 1) / 4;
        const auto bonus = std::to_string(proficiency);
        if (d.hp != sheet.hit_points)
        {
            throw std::runtime_error("Character HP does not match rules profile");
        }
        CharacterProfile result{
            std::move(data),
            d.hp,
            d.ac,
            "Level 1-4 subset: HP, selected feats, supported prepared spells and level-one/two slots. Additional class/subclass and species features remain unavailable.",
            d.speed,
            d.melee_bonus + d.melee_magic};
        result.strength_dexterity_disadvantage = d.str_dex_disadvantage;
        result.weapon_hands = d.weapon_hands;
        result.item_save_bonus = d.item_saves;
        unsigned hand_index = 0;
        for (const auto &key : gear)
        {
            result.equipment_positions.push_back({detail::weapon(key)
                                                      ? (hand_index++ ? "Other hand" : "Main hand")
                                                  : detail::worn_item(key) ? "Worn"
                                                                           : "Equipped",
                                                  {}});
        }
        if (d.versatile_sides)
        {
            const auto sides = d.weapon_hands == 2 ? d.versatile_sides : d.melee.sides;
            result.item_messages.push_back(
                {"Weapon grip: {hands} hands; melee damage 1d{sides} + ability modifier.",
                 {{"hands", std::to_string(d.weapon_hands)}, {"sides", std::to_string(sides)}}});
            result.item_modifiers += "Weapon grip: " + std::to_string(d.weapon_hands) +
                                     " hands; melee damage 1d" + std::to_string(sides) +
                                     " + ability modifier.\n";
        }
        for (const auto &key : gear)
        {
            if (detail::is_shield(key))
            {
                result.item_modifiers += trained(klass, sheet.grants, key)
                                             ? "Source: equipped Shield: +2 AC.\n"
                                             : "Source: equipped Shield: +0 AC (untrained).\n";
            }
            else if (key == "leather")
            {
                result.item_modifiers += "Source: equipped Leather armor and Dexterity score " +
                                         std::to_string(sheet.scores[Ability::dexterity]) +
                                         ". AC becomes 11 + Dexterity modifier (" +
                                         std::to_string(sheet.modifiers[Ability::dexterity]) +
                                         ").\n";
            }
            else if (key == "chain_mail")
            {
                result.item_modifiers +=
                    "Source: equipped Chain mail. AC becomes 16; speed -10 feet below Strength 13 (current Strength " +
                    std::to_string(sheet.scores[Ability::strength]) + ").\n";
            }
            else if (const auto *item = detail::armor(key))
            {
                result.item_modifiers +=
                    "Source: equipped " + std::string(item->label) + " (" +
                    std::string(detail::armor_category_label(item->category)) + "). Base AC " +
                    std::to_string(item->base_ac) + "; applied Dexterity modifier " +
                    std::to_string(
                        item->dexterity_contribution(sheet.modifiers[Ability::dexterity])) +
                    "; armor AC " +
                    std::to_string(item->base_ac + item->dexterity_contribution(
                                                       sheet.modifiers[Ability::dexterity])) +
                    ".\n";
            }
            else if (key == "wand")
            {
                result.item_modifiers +=
                    "Source: equipped Wand. Held focus; melee uses unarmed strike.\n";
            }
            else if (const auto *fixed_weapon = detail::weapon(key);
                     fixed_weapon && fixed_weapon->fixed_damage)
            {
                result.item_modifiers +=
                    "Source: equipped " + weapon_label(key) + ". Attack uses " +
                    attack_ability(key) + " modifier" +
                    (trained(klass, sheet.grants, key) ? " +" + bonus + " class proficiency"
                                                       : std::string(" without proficiency")) +
                    "; damage is fixed at " + std::to_string(fixed_weapon->fixed_damage) +
                    " without an ability modifier.\n";
            }
            else
            {
                result.item_modifiers +=
                    "Source: equipped " + weapon_label(key) + " and " + sheet.character_class +
                    " weapon proficiency. Weapon attack uses " + attack_ability(key) + " modifier" +
                    (trained(klass, sheet.grants, key) ? " +" + bonus + " class proficiency"
                                                       : std::string(" without proficiency")) +
                    "; damage adds that ability modifier.\n";
            }
        }
        for (const auto &key : gear)
        {
            result.item_modifiers +=
                (key == stone_of_good_luck ? "Source: carried " : "Source: equipped ") +
                weapon_label(key) + ". " + equipment_note(sheet, key) + "\n";
        }
        if (features & 1)
        {
            result.item_modifiers += "Defense feat: +1 AC while wearing armor.\n";
        }
        if (features & 8)
        {
            result.item_modifiers +=
                "Great Weapon Fighting: eligible Melee weapon damage dice showing 1 or 2 count as 3 while held in two hands.\n";
        }
        if (features & 32)
        {
            result.item_modifiers +=
                "Alert: add proficiency to Initiative; optionally swap Initiative with an eligible ally before the first turn.\n";
        }
        if (features & 2)
        {
            result.item_modifiers +=
                "Savage Attacker: once per turn on a weapon hit, weapon damage is rolled twice and the higher result is kept automatically.\n";
        }
        if (gear.empty())
        {
            result.item_modifiers =
                "No equipment modifiers. Source: unarmed strike rules and Strength score " +
                std::to_string(sheet.scores[Ability::strength]) +
                ". Attack uses Strength modifier +" + bonus +
                " proficiency; damage is 1 + Strength modifier (minimum 0).";
        }
        result.spell_modifiers = "Active conditions are shown in the character status.";
        if (klass == CharacterClass::wizard)
        {
            result.spell_modifiers =
                "Source: Fire Bolt and Wizard spellcasting, Intelligence score " +
                std::to_string(sheet.scores[Ability::intelligence]) +
                ". Attack: Intelligence modifier +" + bonus +
                " proficiency = " + std::to_string(d.casting) +
                ". Magic Missile has no ability modifier to damage.\n" + result.spell_modifiers;
        }
        if (klass == CharacterClass::cleric)
        {
            result.spell_modifiers = "Source: Cure Wounds and Cleric spellcasting, Wisdom score " +
                                     std::to_string(sheet.scores[Ability::wisdom]) +
                                     ". Healing: 2d8 + Wisdom modifier (" +
                                     std::to_string(d.casting - proficiency) + ").\n" +
                                     result.spell_modifiers;
        }
        if (d.str_dex_disadvantage)
        {
            result.spell_modifiers =
                "Cannot cast spells while wearing untrained armor.\n" + result.spell_modifiers;
        }
        // Language-independent presentation of the same computed rule results.
        for (const auto &key : gear)
        {
            if (detail::is_shield(key))
            {
                result.item_messages.push_back({trained(klass, sheet.grants, key)
                                                    ? "Source: equipped Shield: +2 AC."
                                                    : "Source: equipped Shield: +0 AC (untrained).",
                                                {}});
            }
            else if (key == "leather")
            {
                result.item_messages.push_back(
                    {"Source: equipped Leather armor and Dexterity score {score}. AC becomes 11 + Dexterity modifier ({modifier}).",
                     {{"score", std::to_string(sheet.scores[Ability::dexterity])},
                      {"modifier", std::to_string(sheet.modifiers[Ability::dexterity])}}});
            }
            else if (key == "chain_mail")
            {
                result.item_messages.push_back(
                    {"Source: equipped Chain mail. AC becomes 16; speed -10 feet below Strength 13 (current Strength {score}).",
                     {{"score", std::to_string(sheet.scores[Ability::strength])}}});
            }
            else if (const auto *item = detail::armor(key))
            {
                result.item_messages.push_back(
                    {"Source: equipped {item} ({category}). Base AC {base}; applied Dexterity modifier {dexterity}; armor AC {ac}.",
                     {{"item", std::string(item->label), true},
                      {"category", std::string(detail::armor_category_label(item->category)), true},
                      {"base", std::to_string(item->base_ac)},
                      {"dexterity", std::to_string(item->dexterity_contribution(
                                        sheet.modifiers[Ability::dexterity]))},
                      {"ac",
                       std::to_string(item->base_ac + item->dexterity_contribution(
                                                          sheet.modifiers[Ability::dexterity]))}}});
            }
            else if (key == "wand")
            {
                result.item_messages.push_back(
                    {"Source: equipped Wand. Held focus; melee uses unarmed strike.", {}});
            }
            else if (const auto *fixed_weapon = detail::weapon(key);
                     fixed_weapon && fixed_weapon->fixed_damage)
            {
                result.item_messages.push_back(
                    {trained(klass, sheet.grants, key)
                         ? "Source: equipped {item}. Attack uses {ability} modifier +{bonus} class proficiency; damage is fixed at {damage} without an ability modifier."
                         : "Source: equipped {item}. Attack uses {ability} modifier without proficiency; damage is fixed at {damage} without an ability modifier.",
                     {{"item", weapon_label(key), true},
                      {"ability", attack_ability(key), true},
                      {"bonus", bonus},
                      {"damage", std::to_string(fixed_weapon->fixed_damage)}}});
            }
            else
            {
                result.item_messages.push_back(
                    {trained(klass, sheet.grants, key)
                         ? "Source: equipped {item} and {class} weapon proficiency. Weapon attack uses {ability} modifier +{bonus} class proficiency; damage adds that ability modifier."
                         : "Source: equipped {item} and {class} weapon proficiency. Weapon attack uses {ability} modifier without proficiency; damage adds that ability modifier.",
                     {{"item", weapon_label(key), true},
                      {"class", sheet.character_class, true},
                      {"ability", attack_ability(key), true},
                      {"bonus", bonus}}});
            }
            result.item_messages.push_back({equipment_note(sheet, key), {}});
            for (auto &message : enchantment_messages(key))
            {
                result.item_modifiers += english(message) + "\n";
                result.item_messages.push_back(std::move(message));
            }
            if (const auto *item = detail::armor(key))
            {
                const auto note = [&](std::string text)
                {
                    result.item_modifiers += text + "\n";
                    result.item_messages.push_back({std::move(text), {}});
                };
                if (item->category == detail::ArmorCategory::medium)
                {
                    note(
                        "Medium armor limits the Dexterity modifier added to AC to +2; negative modifiers still apply.");
                }
                if (item->category == detail::ArmorCategory::heavy)
                {
                    note("Heavy armor ignores the Dexterity modifier when calculating AC.");
                }
                if (item->stealth_disadvantage)
                {
                    note("This armor imposes Disadvantage on Dexterity (Stealth) checks.");
                }
                if (item->strength && key != "chain_mail")
                {
                    const bool penalty = sheet.scores[Ability::strength] < item->strength;
                    result.item_modifiers +=
                        "Source: equipped " + std::string(item->label) + ". Requires Strength " +
                        std::to_string(item->strength) + "; current score " +
                        std::to_string(sheet.scores[Ability::strength]) + ". " +
                        (penalty ? "Speed reduced by 10 feet." : "Requirement met.") + "\n";
                    result.item_messages.push_back(
                        {penalty
                             ? "Source: equipped {item}. Requires Strength {required}; current score {score}. Speed reduced by 10 feet."
                             : "Source: equipped {item}. Requires Strength {required}; current score {score}. Requirement met.",
                         {{"item", std::string(item->label), true},
                          {"required", std::to_string(item->strength)},
                          {"score", std::to_string(sheet.scores[Ability::strength])}}});
                }
            }
            if (const auto *item = detail::weapon(key); item && item->heavy)
            {
                const std::string ability = item->ranged ? "Dexterity" : "Strength";
                const auto governing = item->ranged ? Ability::dexterity : Ability::strength;
                const auto score = std::to_string(sheet.scores[governing]);
                const bool penalty = item->heavy_disadvantage(sheet.scores);
                result.item_modifiers +=
                    "Source: equipped " + weapon_label(key) + " (Heavy). Requires " + ability +
                    " 13; current score " + score + ". " +
                    (penalty ? "Attacks with this weapon have Disadvantage." : "Requirement met.") +
                    "\n";
                result.item_messages.push_back(
                    {penalty
                         ? "Source: equipped {item} (Heavy). Requires {ability} 13; current score {score}. Attacks with this weapon have Disadvantage."
                         : "Source: equipped {item} (Heavy). Requires {ability} 13; current score {score}. Requirement met.",
                     {{"item", weapon_label(key), true},
                      {"ability", ability, true},
                      {"score", score}}});
            }
        }
        if (features & 1)
        {
            result.item_messages.push_back({"Defense feat: +1 AC while wearing armor.", {}});
        }
        if (features & 8)
        {
            result.item_messages.push_back(
                {"Great Weapon Fighting: eligible Melee weapon damage dice showing 1 or 2 count as 3 while held in two hands.",
                 {}});
        }
        if (features & 32)
        {
            result.item_messages.push_back(
                {"Alert: add proficiency to Initiative; optionally swap Initiative with an eligible ally before the first turn.",
                 {}});
        }
        if (features & 2)
        {
            result.item_messages.push_back(
                {"Savage Attacker: once per turn on a weapon hit, weapon damage is rolled twice and the higher result is kept automatically.",
                 {}});
        }
        if (gear.empty())
        {
            result.item_messages.push_back(
                {"No equipment modifiers. Source: unarmed strike rules and Strength score {score}. Attack uses Strength modifier +{bonus} proficiency; damage is 1 + Strength modifier (minimum 0).",
                 {{"score", std::to_string(sheet.scores[Ability::strength])}, {"bonus", bonus}}});
        }
        if (d.str_dex_disadvantage)
        {
            result.spell_messages.push_back(
                {"Cannot cast spells while wearing untrained armor.", {}});
        }
        if (klass == CharacterClass::wizard)
        {
            result.spell_messages.push_back(
                {"Source: Fire Bolt and Wizard spellcasting, Intelligence score {score}. Attack: Intelligence modifier +{bonus} proficiency = {attack}. Magic Missile has no ability modifier to damage.",
                 {{"score", std::to_string(sheet.scores[Ability::intelligence])},
                  {"bonus", bonus},
                  {"attack", std::to_string(d.casting)}}});
        }
        if (klass == CharacterClass::cleric)
        {
            result.spell_messages.push_back(
                {"Source: Cure Wounds and Cleric spellcasting, Wisdom score {score}. Healing: 2d8 + Wisdom modifier ({modifier}).",
                 {{"score", std::to_string(sheet.scores[Ability::wisdom])},
                  {"modifier", std::to_string(d.casting - proficiency)}}});
        }
        if (access.cantrip_choices)
        {
            for (const auto &spell : access.cantrips)
            {
                result.spell_modifiers += "\nKnown cantrip: " + spell.label + ".";
                result.spell_messages.push_back(
                    {"Known cantrip: {spell}.", {{"spell", spell.label, true}}});
            }
        }
        if (klass == CharacterClass::sorcerer)
        {
            result.spell_modifiers +=
                "\nSorcerer cantrips: Charisma score " +
                std::to_string(sheet.scores[Ability::charisma]) + "; spell attack bonus " +
                std::to_string(d.casting) + "; pending choices " +
                std::to_string(access.cantrip_choices - access.cantrips.size()) + ".";
            result.spell_messages.push_back(
                {"Sorcerer cantrips: Charisma score {score}; spell attack bonus {attack}; pending choices {cantrips}.",
                 {{"score", std::to_string(sheet.scores[Ability::charisma])},
                  {"attack", std::to_string(d.casting)},
                  {"cantrips", std::to_string(access.cantrip_choices - access.cantrips.size())}}});
        }
        if (klass == CharacterClass::warlock)
        {
            result.spell_modifiers +=
                "\nPact Magic cantrips: Charisma score " +
                std::to_string(sheet.scores[Ability::charisma]) + "; spell attack bonus " +
                std::to_string(d.casting) + "; pending choices " +
                std::to_string(access.cantrip_choices - access.cantrips.size()) + ".";
            result.spell_messages.push_back(
                {"Pact Magic cantrips: Charisma score {score}; spell attack bonus {attack}; pending choices {cantrips}.",
                 {{"score", std::to_string(sheet.scores[Ability::charisma])},
                  {"attack", std::to_string(d.casting)},
                  {"cantrips", std::to_string(access.cantrip_choices - access.cantrips.size())}}});
        }
        if (klass == CharacterClass::cleric)
        {
            result.spell_modifiers +=
                "\nPending Cleric choices: " +
                std::to_string(access.cantrip_choices - access.cantrips.size()) + " cantrips, " +
                std::to_string(access.prepared_choices - access.prepared.size()) +
                " prepared spells.";
            result.spell_messages.push_back(
                {"Pending Cleric choices: {cantrips} cantrips, {prepared} prepared spells.",
                 {{"cantrips", std::to_string(access.cantrip_choices - access.cantrips.size())},
                  {"prepared", std::to_string(access.prepared_choices - access.prepared.size())}}});
        }
        if (klass == CharacterClass::ranger)
        {
            result.spell_modifiers +=
                "\nRanger spellcasting: Wisdom score " +
                std::to_string(sheet.scores[Ability::wisdom]) + "; pending prepared spells: " +
                std::to_string(access.prepared_choices - access.prepared.size()) + ".";
            result.spell_messages.push_back(
                {"Ranger spellcasting: Wisdom score {score}; pending prepared spells: {prepared}.",
                 {{"score", std::to_string(sheet.scores[Ability::wisdom])},
                  {"prepared", std::to_string(access.prepared_choices - access.prepared.size())}}});
        }
        if (klass == CharacterClass::paladin)
        {
            result.spell_modifiers +=
                "\nPaladin spellcasting: Charisma score " +
                std::to_string(sheet.scores[Ability::charisma]) + "; pending prepared spells: " +
                std::to_string(access.prepared_choices - access.prepared.size()) + ".";
            result.spell_messages.push_back(
                {"Paladin spellcasting: Charisma score {score}; pending prepared spells: {prepared}.",
                 {{"score", std::to_string(sheet.scores[Ability::charisma])},
                  {"prepared", std::to_string(access.prepared_choices - access.prepared.size())}}});
        }
        if (klass == CharacterClass::wizard)
        {
            for (const auto &spell : access.spellbook)
            {
                result.spell_modifiers += "\nSpellbook: " + spell.label +
                                          " (learned at Wizard level " +
                                          std::to_string(spell.acquired_level) + ").";
                result.spell_messages.push_back(
                    {"Spellbook: {spell} (learned at Wizard level {level}).",
                     {{"spell", spell.label, true},
                      {"level", std::to_string(spell.acquired_level)}}});
            }
            result.spell_modifiers +=
                "\nPending Wizard choices: " +
                std::to_string(access.cantrip_choices - access.cantrips.size()) + " cantrips, " +
                std::to_string(access.spellbook_choices - access.spellbook.size()) +
                " spellbook spells, " +
                std::to_string(access.prepared_choices - access.prepared.size()) +
                " prepared spells.";
            result.spell_messages.push_back(
                {"Pending Wizard choices: {cantrips} cantrips, {book} spellbook spells, {prepared} prepared spells.",
                 {{"cantrips", std::to_string(access.cantrip_choices - access.cantrips.size())},
                  {"book", std::to_string(access.spellbook_choices - access.spellbook.size())},
                  {"prepared", std::to_string(access.prepared_choices - access.prepared.size())}}});
        }
        // Magic Initiate: each spell it teaches, with its feat's spell attack bonus.
        for (const auto &spell : access.initiate)
        {
            const auto attack = std::to_string(casting_bonus(d, spell.id));
            const auto list = spell.source_id.substr(spell.source_id.rfind('_') + 1);
            const std::string feat = list == "cleric"  ? "Magic Initiate (Cleric)"
                                     : list == "druid" ? "Magic Initiate (Druid)"
                                                       : "Magic Initiate (Wizard)";
            result.spell_modifiers +=
                "\n" + feat + ": " + spell.label + "; spell attack bonus " + attack + ".";
            result.spell_messages.push_back(
                {"{feat}: {spell}; spell attack bonus {attack}.",
                 {{"feat", feat, true}, {"spell", spell.label, true}, {"attack", attack}}});
        }
        result.spell_messages.push_back(
            {"Active conditions are shown in the character status.", {}});
        return result;
    }

  private:
    std::shared_ptr<const Content> content_;
    std::string bytes_;

    // The Weapon Mastery kinds learned after level 1, through the sheet's level.
    static std::vector<TrainingChoiceGroup>
    later_masteries(CharacterClass klass, const CharacterSheet &sheet, unsigned level)
    {
        std::vector<TrainingChoiceGroup> groups;
        for (const auto acquired : detail::mastery_levels)
        {
            if (acquired > 1 && acquired <= level)
            {
                if (auto group = detail::mastery_options(klass, acquired, sheet.grants);
                    group.count)
                {
                    groups.push_back(std::move(group));
                }
            }
        }
        return groups;
    }

    // Weapon Mastery's picks, once every one is chosen.
    static std::optional<TrainingReplacementOptions>
    mastery_rest_choice(const CharacterSheet &sheet)
    {
        // Every class's kinds together (MULTI-1), the starting class's first.
        const auto levels = detail::class_levels(sheet);
        std::vector<CharacterClass> classes{detail::class_of(sheet)};
        for (const auto &choice : detail::creation_choices(CreationField::character_class))
        {
            const auto klass = detail::class_from_id(choice.id);
            if (levels[klass] && klass != classes.front())
            {
                classes.push_back(klass);
            }
        }
        std::optional<TrainingChoiceGroup> merged;
        for (const auto klass : classes)
        {
            auto own = detail::mastery_options(klass, 1);
            if (!own.count)
            {
                continue;
            }
            for (const auto acquired : detail::mastery_levels)
            {
                if (acquired > 1 && static_cast<int>(acquired) <= levels[klass])
                {
                    own.count += detail::mastery_options(klass, acquired).count;
                }
            }
            if (!merged)
            {
                merged = std::move(own);
                continue;
            }
            merged->count += own.count;
            for (auto &option : own.options)
            {
                if (std::none_of(merged->options.begin(), merged->options.end(),
                                 [&](const auto &held)
                                 {
                                     return held.id == option.id;
                                 }))
                {
                    merged->options.push_back(std::move(option));
                }
            }
        }
        if (!merged)
        {
            return {};
        }
        auto group = std::move(*merged);
        const auto choices = detail::mastery_choices(sheet.grants, levels);
        std::vector<std::string> selected;
        for (const auto &[source, values] : choices)
        {
            selected.insert(selected.end(), values.begin(), values.end());
        }
        if (selected.size() != group.count)
        {
            return {}; // Nothing to replace until every mastery is chosen.
        }
        group.id = "weapon_mastery";
        return TrainingReplacementOptions{std::move(group), std::move(selected),
                                          detail::mastery_replacements(levels)};
    }

    // A subclass's single choice, stored as a grant sourced from its group
    // ("prey:colossus_slayer" from "subclass:ranger:hunter"), once it is made.
    static constexpr std::string_view subclass_choice_prefix(std::string_view group)
    {
        return group == "subclass:ranger:hunter"      ? "prey:"
               : group == detail::defensive_tactics   ? "tactics:"
               : group == detail::fiendish_resilience ? "resilience:"
               : group == detail::high_elf_cantrip    ? "cantrip:"
                                                      : "land:";
    }

    static std::optional<TrainingReplacementOptions>
    subclass_rest_choice(const CharacterSheet &sheet, TrainingChoiceGroup group)
    {
        const auto prefix = subclass_choice_prefix(group.id);
        const auto made =
            std::find_if(sheet.grants.begin(), sheet.grants.end(),
                         [&](const auto &g)
                         {
                             return g.source_id == group.id && g.id.starts_with(prefix);
                         });
        if (made == sheet.grants.end())
        {
            return {};
        }
        std::vector<std::string> selected{made->id.substr(prefix.size())};
        return TrainingReplacementOptions{std::move(group), std::move(selected), 1};
    }

    static void replace_subclass_choice(CharacterSheet &sheet, const std::string &group,
                                        const std::vector<std::string> &picks)
    {
        if (picks.size() != 1)
        {
            throw std::runtime_error("Choose one option");
        }
        const auto prefix = subclass_choice_prefix(group);
        for (auto &g : sheet.grants)
        {
            if (g.source_id == group && g.id.starts_with(prefix))
            {
                g.id = std::string(prefix) + picks.front();
            }
        }
    }
};

std::unique_ptr<RulesModule> make_module(std::string_view content_bytes)
{
    return std::make_unique<Module>(read_content(content_bytes, {}), std::string(content_bytes));
}

} // namespace opengold::srd5::detail
