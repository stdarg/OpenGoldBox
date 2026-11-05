#include "opengold/srd5.h"
#include "character_class.h"
#include "feature_grants.h"
#include "spell_access.h"
#include "spell_table.h"
#include "training.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <utility>

namespace opengold::srd5
{
using namespace rules;

namespace
{
// Names of what no choice group lists: class features, species traits,
// subclasses and boons, by grant id.
constexpr auto fixed_labels = std::to_array<std::pair<std::string_view, std::string_view>>({
    {"feature:abjure_foes", "Abjure Foes"},
    {"feature:action_surge", "Action Surge"},
    {"feature:additional_fighting_style", "Additional Fighting Style"},
    {"feature:arcane_recovery", "Arcane Recovery"},
    {"feature:aura_of_courage", "Aura of Courage"},
    {"feature:aura_of_devotion", "Aura of Devotion"},
    {"feature:aura_of_protection", "Aura of Protection"},
    {"feature:bardic_inspiration", "Bardic Inspiration"},
    {"feature:blessed_healer", "Blessed Healer"},
    {"feature:blessed_warrior", "Blessed Warrior"},
    {"feature:bonus_proficiencies", "Bonus Proficiencies"},
    {"feature:brutal_strike", "Brutal Strike"},
    {"feature:channel_divinity", "Channel Divinity"},
    {"feature:circle_spells", "Circle Spells"},
    {"feature:countercharm", "Countercharm"},
    {"feature:cunning_action", "Cunning Action"},
    {"feature:cunning_strike", "Cunning Strike"},
    {"feature:cutting_words", "Cutting Words"},
    {"feature:danger_sense", "Danger Sense"},
    {"feature:dark_ones_blessing", "Dark One's Blessing"},
    {"feature:dark_ones_own_luck", "Dark One's Own Luck"},
    {"feature:deflect_attacks", "Deflect Attacks"},
    {"feature:deflect_energy", "Deflect Energy"},
    {"feature:devious_strikes", "Devious Strikes"},
    {"feature:disciple_of_life", "Disciple of Life"},
    {"feature:disciplined_survivor", "Disciplined Survivor"},
    {"feature:divine_intervention", "Divine Intervention"},
    {"feature:draconic_resilience", "Draconic Resilience"},
    {"feature:draconic_spells", "Draconic Spells"},
    {"feature:dragon_wings", "Dragon Wings"},
    {"feature:druidic_warrior", "Druidic Warrior"},
    {"feature:elemental_affinity", "Elemental Affinity"},
    {"feature:empowered_evocation", "Empowered Evocation"},
    {"feature:empowered_strikes", "Empowered Strikes"},
    {"feature:evasion", "Evasion"},
    {"feature:extra_attack", "Extra Attack"},
    {"feature:fast_hands", "Fast Hands"},
    {"feature:fast_movement", "Fast Movement"},
    {"feature:feral_instinct", "Feral Instinct"},
    {"feature:fiend_spells", "Fiend Spells"},
    {"feature:fiendish_resilience", "Fiendish Resilience"},
    {"feature:fighting_style", "Fighting Style"},
    {"feature:fleet_step", "Fleet Step"},
    {"feature:font_of_inspiration", "Font of Inspiration"},
    {"feature:font_of_magic", "Font of Magic"},
    {"feature:frenzy", "Frenzy"},
    {"feature:heightened_focus", "Heightened Focus"},
    {"feature:heroic_warrior", "Heroic Warrior"},
    {"feature:hunters_lore", "Hunter's Lore"},
    {"feature:hurl_through_hell", "Hurl Through Hell"},
    {"feature:improved_blessed_strikes", "Improved Blessed Strikes"},
    {"feature:improved_brutal_strike", "Improved Brutal Strike"},
    {"feature:improved_critical", "Improved Critical"},
    {"feature:improved_cunning_strike", "Improved Cunning Strike"},
    {"feature:indomitable", "Indomitable"},
    {"feature:innate_sorcery", "Innate Sorcery"},
    {"feature:instinctive_pounce", "Instinctive Pounce"},
    {"feature:intimidating_presence", "Intimidating Presence"},
    {"feature:jack_of_all_trades", "Jack of All Trades"},
    {"feature:lands_aid", "Land's Aid"},
    {"feature:magical_cunning", "Magical Cunning"},
    {"feature:magical_secrets", "Magical Secrets"},
    {"feature:memorize_spell", "Memorize Spell"},
    {"feature:metamagic", "Metamagic"},
    {"feature:mindless_rage", "Mindless Rage"},
    {"feature:monks_focus", "Monk's Focus"},
    {"feature:natural_recovery", "Natural Recovery"},
    {"feature:natures_sanctuary", "Nature's Sanctuary"},
    {"feature:natures_veil", "Nature's Veil"},
    {"feature:natures_ward", "Nature's Ward"},
    {"feature:open_hand_technique", "Open Hand Technique"},
    {"feature:overchannel", "Overchannel"},
    {"feature:pact_magic", "Pact Magic"},
    {"feature:peerless_skill", "Peerless Skill"},
    {"feature:perfect_focus", "Perfect Focus"},
    {"feature:persistent_rage", "Persistent Rage"},
    {"feature:potent_cantrip", "Potent Cantrip"},
    {"feature:preserve_life", "Preserve Life"},
    {"feature:primal_knowledge", "Primal Knowledge"},
    {"feature:radiant_strikes", "Radiant Strikes"},
    {"feature:rage", "Rage"},
    {"feature:reckless_attack", "Reckless Attack"},
    {"feature:relentless_hunter", "Relentless Hunter"},
    {"feature:relentless_rage", "Relentless Rage"},
    {"feature:reliable_talent", "Reliable Talent"},
    {"feature:remarkable_athlete", "Remarkable Athlete"},
    {"feature:restoring_touch", "Restoring Touch"},
    {"feature:retaliation", "Retaliation"},
    {"feature:roving", "Roving"},
    {"feature:sacred_weapon", "Sacred Weapon"},
    {"feature:sculpt_spells", "Sculpt Spells"},
    {"feature:sear_undead", "Sear Undead"},
    {"feature:second_wind", "Second Wind"},
    {"feature:self_restoration", "Self-Restoration"},
    {"feature:slippery_mind", "Slippery Mind"},
    {"feature:smite_of_protection", "Smite of Protection"},
    {"feature:sneak_attack", "Sneak Attack"},
    {"feature:sorcerous_restoration", "Sorcerous Restoration"},
    {"feature:sorcery_incarnate", "Sorcery Incarnate"},
    {"feature:spellcasting", "Spellcasting"},
    {"feature:steady_aim", "Steady Aim"},
    {"feature:studied_attacks", "Studied Attacks"},
    {"feature:stunning_strike", "Stunning Strike"},
    {"feature:superior_critical", "Superior Critical"},
    {"feature:superior_hunters_defense", "Superior Hunter's Defense"},
    {"feature:superior_hunters_prey", "Superior Hunter's Prey"},
    {"feature:supreme_sneak", "Supreme Sneak"},
    {"feature:tactical_master", "Tactical Master"},
    {"feature:tactical_mind", "Tactical Mind"},
    {"feature:tactical_shift", "Tactical Shift"},
    {"feature:tireless", "Tireless"},
    {"feature:two_extra_attacks", "Two Extra Attacks"},
    {"feature:unarmored_defense", "Unarmored Defense"},
    {"feature:unarmored_movement", "Unarmored Movement"},
    {"feature:uncanny_dodge", "Uncanny Dodge"},
    {"feature:uncanny_metabolism", "Uncanny Metabolism"},
    {"feature:use_magic_device", "Use Magic Device"},
    {"feature:wholeness_of_body", "Wholeness of Body"},
    {"feature:wild_resurgence", "Wild Resurgence"},
    {"feature:wild_shape", "Wild Shape"},
    {"trait:adrenaline_rush", "Adrenaline Rush"},
    {"trait:dwarven_resilience", "Dwarven Resilience"},
    {"trait:dwarven_toughness", "Dwarven Toughness"},
    {"trait:powerful_build", "Powerful Build"},
    {"trait:speed", "Speed 35 feet"},
    {"subclass:berserker", "Path of the Berserker"},
    {"subclass:champion", "Champion"},
    {"subclass:devotion", "Oath of Devotion"},
    {"subclass:draconic", "Draconic Sorcery"},
    {"subclass:evoker", "Evoker"},
    {"subclass:fiend", "Fiend Patron"},
    {"subclass:hunter", "Hunter"},
    {"subclass:land", "Circle of the Land"},
    {"subclass:life", "Life Domain"},
    {"subclass:lore", "College of Lore"},
    {"subclass:open_hand", "Warrior of the Open Hand"},
    {"subclass:thief", "Thief"},
    {"boon:bodily_health", "Manual of Bodily Health"},
});

// Where a heading goes on the sheet; headings of one kind keep the order
// they are first met in.
enum class Kind
{
    species,
    subclass,
    feature,
    class_choice,
    feat,
    cantrip,
    spell,
    other_spell
};

std::string_view fixed_label(std::string_view id)
{
    const auto found = std::find_if(fixed_labels.begin(), fixed_labels.end(),
                                    [&](const auto &entry)
                                    {
                                        return entry.first == id;
                                    });
    return found == fixed_labels.end() ? std::string_view{} : found->second;
}

std::string named_spell(std::string_view id)
{
    const auto label = spell_label(id);
    return label.empty() ? std::string(id) : std::string(label);
}

class Details
{
  public:
    void add(Kind kind, std::string_view heading, std::string name)
    {
        auto found = std::find_if(entries_.begin(), entries_.end(),
                                  [&](const auto &entry)
                                  {
                                      return entry.second.heading == heading;
                                  });
        if (found == entries_.end())
        {
            entries_.push_back({kind, {std::string(heading), {}}});
            found = std::prev(entries_.end());
        }
        auto &names = found->second.names;
        if (std::find(names.begin(), names.end(), name) == names.end())
        {
            names.push_back(std::move(name));
        }
    }

    [[nodiscard]] bool names(std::string_view name) const
    {
        return std::any_of(entries_.begin(), entries_.end(),
                           [&](const auto &entry)
                           {
                               const auto &names = entry.second.names;
                               return std::find(names.begin(), names.end(), name) != names.end();
                           });
    }

    [[nodiscard]] std::vector<SheetDetail> ordered() &&
    {
        std::stable_sort(entries_.begin(), entries_.end(),
                         [](const auto &a, const auto &b)
                         {
                             return a.first < b.first;
                         });
        std::vector<SheetDetail> result;
        for (auto &entry : entries_)
        {
            result.push_back(std::move(entry.second));
        }
        return result;
    }

  private:
    std::vector<std::pair<Kind, SheetDetail>> entries_;
};

// Every choice group whose picks become grants, with all its options: the
// species', the creation class choices and those of later levels.
std::vector<TrainingChoiceGroup> choice_groups(const CharacterDraft &creation)
{
    auto groups = detail::training_options(creation);
    for (auto group :
         {detail::hunters_prey_options(), detail::defensive_tactics_options(),
          detail::elemental_fury_options(), detail::blessed_strikes_options(),
          detail::land_options(), detail::metamagic_options(2, {}),
          detail::metamagic_options(10, {}), detail::fiendish_resilience_options(),
          detail::elemental_affinity_options(), detail::discovery_options({}),
          detail::arcanum_options(11), detail::arcanum_options(13), detail::arcanum_options(15)})
    {
        groups.push_back(std::move(group));
    }
    return groups;
}
} // namespace

namespace
{
// The class a grant's source names ("class:rogue", "subclass:rogue:thief"),
// if any.
std::optional<detail::CharacterClass> source_class(std::string_view source_id)
{
    for (const std::string_view prefix : {"class:", "subclass:"})
    {
        if (source_id.starts_with(prefix))
        {
            const auto rest = source_id.substr(prefix.size());
            for (const auto &choice : detail::creation_choices(CreationField::character_class))
            {
                if (rest.substr(0, rest.find(':')) == choice.id)
                {
                    return detail::class_from_id(choice.id);
                }
            }
        }
    }
    return std::nullopt;
}

// A multiclass sheet's heading for one class's features (MULTI-1).
std::string_view class_features_heading(detail::CharacterClass klass)
{
    using enum detail::CharacterClass;
    switch (klass)
    {
    case barbarian:
        return "Barbarian features";
    case bard:
        return "Bard features";
    case cleric:
        return "Cleric features";
    case druid:
        return "Druid features";
    case fighter:
        return "Fighter features";
    case monk:
        return "Monk features";
    case paladin:
        return "Paladin features";
    case ranger:
        return "Ranger features";
    case rogue:
        return "Rogue features";
    case sorcerer:
        return "Sorcerer features";
    case warlock:
        return "Warlock features";
    default:
        return "Wizard features";
    }
}
} // namespace

std::vector<SheetDetail> sheet_details(const CharacterSheet &sheet, const CharacterDraft &creation)
{
    auto groups = choice_groups(creation);
    // A later class's level-one choices are named by their groups too.
    const auto levels = detail::class_levels(sheet);
    const bool multiclassed = levels.count() > 1;
    for (const auto &choice : detail::creation_choices(CreationField::character_class))
    {
        const auto held = detail::class_from_id(choice.id);
        if (levels[held] && choice.id != creation.character_class)
        {
            for (auto &group : detail::multiclass_options(held, {}, {}))
            {
                groups.push_back(std::move(group));
            }
        }
    }
    const auto invocations = detail::every_invocation(sheet.grants);
    Details details;
    for (const auto &grant : sheet.grants)
    {
        const auto colon = grant.id.find(':');
        const auto prefix = std::string_view(grant.id).substr(0, colon + 1);
        const auto id = std::string_view(grant.id).substr(colon + 1);
        // Skills, Expertise and Weapon Mastery are in the sheet's training.
        if (prefix == "skill:" || prefix == "expertise:" || prefix == "mastery:")
        {
            continue;
        }
        // A class taken after the first, with part of its training.
        if (prefix == "multiclass:")
        {
            details.add(Kind::feature, "Multiclass training",
                        std::string(detail::class_label(detail::class_from_id(id))));
            continue;
        }
        if (const auto fixed = fixed_label(grant.id); !fixed.empty())
        {
            const auto kind = prefix == "trait:"      ? Kind::species
                              : prefix == "subclass:" ? Kind::subclass
                                                      : Kind::feature;
            const auto owner = multiclassed ? source_class(grant.source_id) : std::nullopt;
            details.add(kind,
                        prefix == "trait:"      ? "Species traits"
                        : prefix == "subclass:" ? "Subclass"
                        : prefix == "boon:"     ? "Boons"
                        : owner                 ? class_features_heading(*owner)
                                                : "Features",
                        std::string(fixed));
            continue;
        }
        if (prefix == "invocation:")
        {
            const auto found = std::find_if(invocations.options.begin(), invocations.options.end(),
                                            [&](const auto &option)
                                            {
                                                return option.id == id;
                                            });
            details.add(Kind::class_choice, invocations.label,
                        found == invocations.options.end() ? std::string(id) : found->label);
            continue;
        }
        if (prefix == "spell:" || grant.id.starts_with("boon:scroll:"))
        {
            const auto spell = grant.id.substr(grant.id.rfind(':') + 1);
            const auto *found = detail::find_spell(spell);
            const bool cantrip = found && found->level == 0;
            const auto feat = std::string_view(grant.source_id).starts_with("feat:")
                                  ? feat_label(grant.source_id.substr(5))
                                  : std::string_view{};
            details.add(!feat.empty() ? Kind::other_spell
                        : cantrip     ? Kind::cantrip
                                      : Kind::spell,
                        !feat.empty() ? feat
                        : cantrip     ? "Cantrips"
                        // A Wizard's spells, starting or later (MULTI-1).
                        : grant.source_id == "class:wizard:spellcasting" ||
                                grant.source_id == "item:scroll"
                            ? "Spellbook"
                            : "Spells known",
                        named_spell(spell));
            continue;
        }
        // A choice is named by the group it was picked from.
        const auto group = std::find_if(groups.begin(), groups.end(),
                                        [&](const auto &g)
                                        {
                                            return g.id == grant.source_id;
                                        });
        if (group != groups.end())
        {
            const auto option = std::find_if(group->options.begin(), group->options.end(),
                                             [&](const auto &o)
                                             {
                                                 return o.id == id;
                                             });
            if (option != group->options.end())
            {
                details.add(grant.source_id.starts_with("species:") ? Kind::species
                                                                    : Kind::class_choice,
                            group->label, option->label);
                continue;
            }
        }
        // Magic Initiate's spellcasting ability, picked with its spells.
        if (prefix == "spellcasting_ability:")
        {
            auto ability = std::string(id);
            ability.front() =
                static_cast<char>(std::toupper(static_cast<unsigned char>(ability.front())));
            details.add(Kind::other_spell, "Magic Initiate spellcasting ability",
                        std::move(ability));
            continue;
        }
        if (prefix == "feat:")
        {
            const auto label = feat_label(id);
            details.add(Kind::feat, "Feats", label.empty() ? std::string(id) : std::string(label));
            continue;
        }
        if (prefix == "discovery:" || prefix == "arcanum:")
        {
            details.add(Kind::other_spell, group == groups.end() ? "Spells known" : group->label,
                        named_spell(id));
            continue;
        }
        details.add(Kind::feature, "Other", grant.id);
    }
    // The species' own spells (a Tiefling's legacy) that are not a choice.
    for (const auto &taught : detail::lineage_spells(sheet.grants, sheet.level))
    {
        for (const auto &spell : taught.spells)
        {
            if (!details.names(named_spell(spell)))
            {
                details.add(Kind::other_spell, "Species spells", named_spell(spell));
            }
        }
    }
    return std::move(details).ordered();
}
} // namespace opengold::srd5
