#include "sheet_format.h"
#include "class_text.h"
#include "godot_text.h"
#include "character_sheet_text.h"
#include "inventory_label.h"
#include "character_text.h"
#include "hp_presentation.h"
#include "status_effect_text.h"
#include "training_control.h"
#include "opengold/srd5.h"
#include "opengold/original.h"
#include "opengold/buck_rogers_characters.h"
#include "opengold/buck_rogers_rules.h"
#include "game_resources.h"
#include <algorithm>
#include <array>
#include <span>
#include <utility>
#include <string>
#include <vector>

using namespace godot;
using namespace opengold;

namespace
{
using presentation::gs;
using presentation::rich_text_literal;
using presentation::signed_number;

// The party, when given, adds what it knows about the member: current HP,
// gold, AC and where each item is held.

// A table of labelled numbers, such as the saving throws.
std::string number_table(std::span<const char *const> labels, std::span<const int> numbers,
                         const char *suffix = "")
{
    std::string text = "[theme_table=abilities]";
    for (std::size_t i = 0; i < labels.size(); ++i)
    {
        text += "[cell]" + i18n::utf8(labels[i]) + "[/cell][cell]" + std::to_string(numbers[i]) +
                suffix + "[/cell][cell][/cell]";
    }
    return text + "[/theme_table]";
}

// What a sheet under the original rules shows beside the portrait: the
// levels of each class, age, Hit Points and THAC0, the scores, the five
// saving throws, and a thief's skills and a caster's spell slots.
std::string original_identity_and_attributes(const Character &character, const PartyMember *member,
                                             const CampaignParty *party)
{
    const auto &s = character.sheet();
    const auto &original = *s.original;
    std::string levels;
    for (const int level : original.class_levels)
    {
        if (level > 0)
        {
            levels += (levels.empty() ? "" : "/") + std::to_string(level);
        }
    }
    std::string text = i18n::formatted(
        "[theme_font=sheet_title]{name}[/theme_font]\nLevel {levels} / {race} / {gender} / {class}\n{alignment} / Age {age}\n\n",
        {{"name", gs(rich_text_literal(s.name))},
         {"levels", gs(levels)},
         {"race", i18n::text(s.race)},
         {"gender", i18n::text(s.gender)},
         {"class", i18n::text(s.character_class)},
         {"alignment", i18n::text(s.alignment)},
         {"age", original.age}});
    text += "[theme_emphasis]" +
            std::string(presentation::hp_text(member ? member->vitals.hit_points : s.hit_points,
                                              s.hit_points, member && member->vitals.dead, {},
                                              s.hp_messages)
                            .utf8()
                            .get_data()) +
            "[/theme_emphasis]";
    text += i18n::formatted("   THAC0 {thac0}", {{"thac0", original.thac0}});
    if (member)
    {
        text += i18n::formatted(original::krynn_game(original::game_of(s.identity))
                                    ? N_("   Party steel {gold}   XP {xp}")
                                    : N_("   Party gold {gold}   XP {xp}"),
                                {{"gold", party->purse_gold()}, {"xp", member->experience}});
    }
    if (member && member->vitals.dead)
    {
        text += "   " + i18n::utf8("Dead");
    }
    text += "\n\n[theme_title]" + i18n::utf8("Attributes") + "[/theme_title]";
    text += "\n[theme_table=abilities]";
    for (unsigned i = 0; i < 6; ++i)
    {
        text += "[cell]" + i18n::utf8(i18n::ability_names[i]) + "[/cell][cell]" +
                std::string(
                    i18n::original_score(s, opengold::rules::all_abilities[i]).utf8().get_data()) +
                "[/cell][cell][/cell]";
    }
    text += "[/theme_table]";
    static constexpr std::array<const char *, 5> saves{
        N_("Paralysis, poison or death magic"), N_("Petrification or polymorph"),
        N_("Rod, staff or wand"), N_("Breath weapon"), N_("Spell")};
    text += "\n\n[theme_title]" + i18n::utf8("Saving throws") + "[/theme_title]\n" +
            number_table(saves, original.saving_throws);
    if (std::ranges::any_of(original.thief_skills,
                            [](int chance)
                            {
                                return chance > 0;
                            }))
    {
        static constexpr std::array<const char *, 8> skills{
            N_("Pick pockets"),  N_("Open locks"),      N_("Find and remove traps"),
            N_("Move silently"), N_("Hide in shadows"), N_("Hear noise"),
            N_("Climb walls"),   N_("Read languages")};
        text += "\n\n[theme_title]" + i18n::utf8("Thief skills") + "[/theme_title]\n" +
                number_table(skills, original.thief_skills, "%");
    }
    static constexpr std::array<const char *, 9> spell_levels{
        N_("First level"),   N_("Second level"), N_("Third level"),
        N_("Fourth level"),  N_("Fifth level"),  N_("Sixth level"),
        N_("Seventh level"), N_("Eighth level"), N_("Ninth level")};
    for (const auto &[title, slots] :
         {std::pair{N_("Cleric spells a day"), original.cleric_slots},
          std::pair{N_("Druid spells a day"), original.druid_slots},
          std::pair{N_("Magic-user spells a day"), original.magic_user_slots}})
    {
        if (slots[0] > 0)
        {
            // The levels it casts: Pool of Radiance's three, Curse's up to
            // five, Secret's up to seven, Pools of Darkness's up to nine.
            const auto shown = static_cast<std::size_t>(
                std::max<std::ptrdiff_t>(3, std::ranges::find(slots, 0) - slots.begin()));
            text +=
                "\n\n[theme_title]" + i18n::utf8(title) + "[/theme_title]\n" +
                number_table(std::span(spell_levels).first(shown), std::span(slots).first(shown));
        }
    }
    return text;
}

// A Buck Rogers character's record as it is now: the party's, which fights,
// shops and training change, else the one its creation made.
std::optional<original::GroundRecord> buck_rogers_record(const Character &character,
                                                         const PartyMember *member,
                                                         const CampaignParty *party)
{
    return original::buck_rogers_record_of(member ? party->game_record(member->id)
                                                  : character.creation_data().rules_data);
}

// A Buck Rogers sheet beside its body icon: the career's level, age, Hit
// Points, experience and credits, the seven abilities, the eight saving
// throws, the skills it has and a Warrior's specializations.
std::string buck_rogers_identity_and_attributes(const Character &character,
                                                const original::GroundRecord &record,
                                                const PartyMember *member)
{
    const auto &s = character.sheet();
    const auto &c = record.creature;
    std::string text = i18n::formatted(
        "[theme_font=sheet_title]{name}[/theme_font]\nLevel {level} / {race} / {gender} / {career}\nAge {age}\n\n",
        {{"name", gs(rich_text_literal(s.name))},
         {"level", c.level},
         {"race", i18n::text(s.race)},
         {"gender", i18n::text(s.gender)},
         {"career", i18n::text(s.character_class)},
         {"age", static_cast<int>(c.age)}});
    text += "[theme_emphasis]" +
            std::string(presentation::hp_text(c.hit_points, c.hit_point_maximum,
                                              member && member->vitals.dead, {}, {})
                            .utf8()
                            .get_data()) +
            "[/theme_emphasis]";
    text += i18n::formatted("   Experience {xp}   Credits {credits}",
                            {{"xp", static_cast<std::int64_t>(c.experience)},
                             {"credits", static_cast<std::int64_t>(c.credits)}});
    static constexpr std::array<const char *, 7> abilities{
        N_("Strength"), N_("Dexterity"), N_("Constitution"), N_("Intelligence"),
        N_("Wisdom"),   N_("Charisma"),  N_("Tech")};
    std::array<int, 7> scores{};
    std::ranges::copy(c.current_abilities, scores.begin());
    text += "\n\n[theme_title]" + i18n::utf8("Attributes") + "[/theme_title]\n" +
            number_table(abilities, scores);
    static constexpr std::array<const char *, 8> saves{
        N_("Saving throw 1"), N_("Saving throw 2"), N_("Saving throw 3"), N_("Saving throw 4"),
        N_("Saving throw 5"), N_("Saving throw 6"), N_("Saving throw 7"), N_("Saving throw 8")};
    std::array<int, 8> needs{};
    std::ranges::copy(c.saves, needs.begin());
    text += "\n\n[theme_title]" + i18n::utf8("Saving throws") + "[/theme_title]\n" +
            number_table(saves, needs);
    // Each skill it has, with its level and the value checks roll against.
    std::string skills;
    for (unsigned number = 1; number <= original::skill_count; ++number)
    {
        const auto skill = original::skill_numbered(number);
        const int level = original::level_of(c.skills, skill);
        if (level == 0)
        {
            continue;
        }
        skills += "[cell]" + i18n::utf8(original::skill_definition(skill).name) + "[/cell][cell]" +
                  std::to_string(level) + "[/cell][cell]" +
                  std::to_string(original::skill_value(c, skill)) + "[/cell]";
    }
    text += "\n\n[theme_title]" + i18n::utf8("Skills") + "[/theme_title]\n" +
            (skills.empty() ? i18n::utf8("None")
                            : "[theme_table=abilities]" + skills + "[/theme_table]");
    return text;
}

// Below the body icon: Armor Class and movement, a Warrior's
// specializations and the items it carries, named as the game names them.
std::string buck_rogers_details(const original::GroundRecord &record)
{
    const auto &c = record.creature;
    std::string text = "\n\n" + i18n::formatted("Armor Class {ac}   Movement {movement}",
                                                {{"ac", c.base_armor_class},
                                                 {"movement", static_cast<int>(c.base_movement)}});
    const auto &stock = buck_rogers_stock(game_rules_files());
    std::string specializations;
    for (const auto type : original::specializable_weapon_types(stock.templates))
    {
        const int level = original::specialization_level(c, type);
        const auto weapon = std::ranges::find(stock.templates, type, &buck_rogers::Item::type);
        if (level > 0 && weapon != stock.templates.end())
        {
            specializations += "\n" +
                               rich_text_literal(buck_rogers_item_name(*weapon, stock.item_words)) +
                               " +" + std::to_string(level);
        }
    }
    if (!specializations.empty())
    {
        text += "\n\n[theme_title]" + i18n::utf8("Weapon specializations") + "[/theme_title]" +
                specializations;
    }
    text += "\n\n[theme_title]" + i18n::utf8("Inventory") + "[/theme_title]";
    if (record.items.empty())
    {
        text += "\n" + i18n::utf8("Empty");
    }
    for (const auto &item : record.items)
    {
        text += "\n" + rich_text_literal(buck_rogers_item_name(item, stock.item_words));
        if (item.quantity > 1)
        {
            text += " x" + std::to_string(item.quantity);
        }
        if ((item.flags & buck_rogers::Item::readied) != 0)
        {
            text += " / " + i18n::utf8("Readied");
        }
    }
    return text;
}

// The name and identity, hit points and attributes: what the town's sheet
// shows beside the portrait.
std::string identity_and_attributes(const Character &character, const PartyMember *member,
                                    const CampaignParty *party)
{
    const auto &s = character.sheet();
    if (const auto record = buck_rogers_record(character, member, party))
    {
        return buck_rogers_identity_and_attributes(character, *record, member);
    }
    if (s.original)
    {
        return original_identity_and_attributes(character, member, party);
    }
    std::string text = i18n::formatted(
        "[theme_font=sheet_title]{name}[/theme_font]\nLevel {level} / {race} / {gender} / {class}\n{alignment} / {background}\n\n",
        {{"name", gs(rich_text_literal(s.name))},
         {"level", s.level},
         {"race", i18n::text(s.race)},
         {"gender", i18n::text(s.gender)},
         {"class", i18n::class_summary(s)},
         {"alignment", i18n::text(s.alignment)},
         {"background", i18n::text(s.background)}});
    // The current class is not a future goal.
    String goals;
    const auto &draft = character.creation_data();
    const auto options = srd5::character_rules()->choices(rules::CreationField::character_class);
    for (const auto &id : draft.target_classes)
    {
        for (const auto &option : options)
        {
            if (option.id == id && id != draft.character_class)
            {
                if (!goals.is_empty())
                {
                    goals += ", ";
                }
                goals += i18n::text(option.label);
            }
        }
    }
    if (!goals.is_empty())
    {
        text += i18n::formatted("Future class goals: {classes}\n\n", {{"classes", goals}});
    }
    rules::TemporaryHitPoints temporary;
    if (member)
    {
        temporary = party->recovery_info(member->id).temporary_hp;
    }
    text += "[theme_emphasis]" +
            std::string(presentation::hp_text(member ? member->vitals.hit_points : s.hit_points,
                                              s.hit_points, member && member->vitals.dead,
                                              temporary, s.hp_messages)
                            .utf8()
                            .get_data()) +
            "[/theme_emphasis]";
    // A multiclass character's dice by size, such as "3d10 + 2d6".
    std::string dice;
    for (const auto &held : srd5::hit_dice(s))
    {
        dice += (dice.empty() ? "" : " + ") + std::to_string(held.count) + "d" +
                std::to_string(held.die);
    }
    text += i18n::formatted("   Hit Dice: {dice}", {{"dice", gs(dice)}});
    if (member)
    {
        text += i18n::formatted("   Party gold {gold}   XP {xp}",
                                {{"gold", party->purse_gold()}, {"xp", member->experience}});
    }
    if (member && member->vitals.dead)
    {
        text += "   " + i18n::utf8("Dead");
    }
    if (member && party->can_advance(member->id))
    {
        text += "   [theme_emphasis]" + i18n::utf8("Ready to level up") + "[/theme_emphasis]";
    }
    const auto display = [](const std::string &id)
    {
        std::string label = id;
        std::replace(label.begin(), label.end(), '_', ' ');
        return i18n::utf8(label);
    };
    // Names as the creation pages show them, falling back to the id.
    const auto named = [&](std::string_view label, const std::string &id)
    {
        return label.empty() ? gs(display(id)) : i18n::text(label);
    };
    if (!s.prepared_spells.empty())
    {
        String spells;
        for (const auto &spell : s.prepared_spells)
        {
            spells += named(opengold::srd5::spell_label(spell), spell) + "  ";
        }
        text += "\n" + i18n::formatted("Prepared spells: {spells}", {{"spells", spells}});
    }
    text += "\n[theme_font=sheet_note]" +
            std::string(i18n::render(s.hp_messages).utf8().get_data()) + "[/theme_font]";
    text += "\n\n[theme_title]" + i18n::utf8("Attributes") + "[/theme_title]";
    text += i18n::utf8(
        "\n[theme_table=abilities][theme_header]Attribute[/theme_header][theme_header]Score[/theme_header][theme_header_last]Saving throw[/theme_header_last]");
    // Worn items change every saving throw (MAGIC-4).
    int item_saves = 0;
    if (member)
    {
        try
        {
            item_saves = party->profile(member->id).item_save_bonus;
        }
        catch (const std::exception &)
        {
            // The AC line below reports why the profile is unavailable.
        }
    }
    for (unsigned i = 0; i < 6; ++i)
    {
        const auto ability = opengold::rules::all_abilities[i];
        const auto score = std::to_string(s.scores[ability]);
        const auto colored =
            s.modifiers[ability] == 0
                ? score
                : "[theme_color=" +
                      std::string(s.modifiers[ability] > 0 ? "score_positive" : "score_negative") +
                      "]" + score + "[/theme_color]";
        text += "[cell]" + i18n::utf8(i18n::ability_names[i]) + "[/cell][cell]" + colored +
                "[/cell][cell]" + signed_number(s.saving_throws[ability] + item_saves) +
                (s.save_proficiencies[ability] ? " *" : "") + "[/cell]";
    }
    text += "[/theme_table]\n" + i18n::utf8("* Proficient saving throw");
    if (item_saves)
    {
        text += "\n" + i18n::formatted("Saving throws include {bonus} from worn items.",
                                       {{"bonus", gs(signed_number(item_saves))}});
    }
    return text;
}

// AC, status effects, features, inventory and training: what the town's sheet
// shows below the portrait.
std::string details(const Character &character, const PartyMember *member,
                    const CampaignParty *party)
{
    const auto &s = character.sheet();
    if (const auto record = buck_rogers_record(character, member, party))
    {
        return buck_rogers_details(*record);
    }
    std::string text;
    if (member)
    {
        try
        {
            const auto p = party->profile(member->id);
            text += "\n\n" + i18n::formatted("AC {ac}   Speed {speed} ft",
                                             {{"ac", p.armor_class}, {"speed", p.movement_feet}});
        }
        catch (const std::exception &e)
        {
            text += "\n\n" + rich_text_literal(e.what());
        }
        if (!member->vitals.description.empty())
        {
            text += "\n" + rich_text_literal(member->vitals.description);
        }
    }
    if (member)
    {
        text += "\n\n[theme_title]" + i18n::utf8("Status Effects") + "[/theme_title]";
        const auto effects = party->status_effects(member->id);
        if (effects.empty())
        {
            text += "\n" + i18n::utf8("None");
        }
        for (const auto &effect : effects)
        {
            const auto plain = [](const String &value)
            {
                return rich_text_literal(value.utf8().get_data());
            };
            text += "\n" + plain(presentation::status_effect_text(effect, false)) +
                    "\n[theme_font=sheet_note]" + plain(i18n::render(effect.description)) +
                    "[/theme_font]";
        }
    }
    // The species' and class's features and choices, feats, cantrips and
    // spells, which a character under the original rules does not have.
    const auto details = s.original ? std::vector<opengold::srd5::SheetDetail>{}
                                    : opengold::srd5::sheet_details(s, character.creation_data());
    if (!details.empty())
    {
        text += "\n\n[theme_title]" + i18n::utf8("Features and choices") + "[/theme_title]";
    }
    for (const auto &detail : details)
    {
        String names;
        for (const auto &name : detail.names)
        {
            names += (names.is_empty() ? String() : String(", ")) + i18n::text(name);
        }
        text += "\n" + i18n::formatted("{heading}: {names}",
                                       {{"heading", i18n::text(detail.heading)}, {"names", names}});
    }
    text += "\n\n[theme_title]" + i18n::utf8("Inventory") + "[/theme_title]";
    if (character.inventory().empty())
    {
        text += "\n" + i18n::utf8("Empty");
    }
    std::vector<rules::Message> positions;
    if (member)
    {
        positions = party->profile(member->id).equipment_positions;
    }
    for (const auto &item : character.inventory().items())
    {
        text += "\n" +
                rich_text_literal((member ? inventory_label(*member, item) : inventory_label(item))
                                      .utf8()
                                      .get_data());
        if (member)
        {
            const auto held = std::find(member->equipped.begin(), member->equipped.end(), item.id);
            if (held != member->equipped.end())
            {
                text += " / " + i18n::utf8(positions.at(held - member->equipped.begin()).source);
            }
        }
    }
    text += presentation::training_summary(s.training, i18n::text).utf8().get_data();
    return text;
}
} // namespace

namespace presentation
{
String sheet_text(const Character &character)
{
    return gs(identity_and_attributes(character, nullptr, nullptr) +
              details(character, nullptr, nullptr));
}

String sheet_text(const CampaignParty &party, const PartyMember &member)
{
    const auto sections = sheet_sections(party, member);
    return sections.beside_portrait + sections.below_portrait;
}

SheetSections sheet_sections(const CampaignParty &party, const PartyMember &member)
{
    return {gs(identity_and_attributes(member.character, &member, &party)),
            gs(details(member.character, &member, &party))};
}
} // namespace presentation
