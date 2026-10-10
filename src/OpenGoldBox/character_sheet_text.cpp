#include "character_sheet_text.h"
#include "character_text.h"
#include "hp_presentation.h"
#include "training_control.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <string>
#include <vector>

using namespace godot;
using namespace opengold;

namespace
{
std::string number(int n)
{
    return (n >= 0 ? "+" : "") + std::to_string(n);
}

std::string literal(std::string_view value)
{
    std::string text;
    for (char c : value)
        text += c == '[' ? "[lb]" : std::string(1, c);
    return text;
}

String gs(std::string_view text)
{
    return String::utf8(text.data(), text.size());
}

// The party, when given, adds what it knows about the member: current HP,
// gold, AC and where each item is held.
String formatted_sheet(const Character &character, const PartyMember *member,
                       const CampaignParty *party)
{
    const auto &s = character.sheet();
    std::string text = i18n::formatted(
                           "[font_size=24]{name}[/font_size]\nLevel {level} / {race} / {gender} / {class}\n{alignment} / {background}\n\n",
    {
        {"name", gs(literal(s.name))},
        {"level", s.level},
        {"race", i18n::text(s.race)},
        {"gender", i18n::text(s.gender)},
        {"class", i18n::text(s.character_class)},
        {"alignment", i18n::text(s.alignment)},
        {"background", i18n::text(s.background)}
    });
    if (!character.creation_data().target_classes.empty())
    {
        String goals;
        const auto options =
            srd5::character_rules()->choices(rules::CreationField::character_class);
        for (const auto &id : character.creation_data().target_classes)
            for (const auto &option : options)
                if (option.id == id)
                {
                    if (!goals.is_empty())
                        goals += ", ";
                    goals += i18n::text(option.label);
                }
        text += i18n::formatted("Future class goals: {classes}\n\n", {{"classes", goals}});
    }
    rules::TemporaryHitPoints temporary;
    if (member)
        temporary = party->recovery_info(member->id).temporary_hp;
    text += "[b]" +
            std::string(presentation::hp_text(member ? member->vitals.hit_points : s.hit_points,
                s.hit_points, member && member->vitals.dead,
                temporary, s.hp_messages)
                        .utf8()
                        .get_data()) +
            "[/b]";
    text += i18n::formatted("   Hit Dice: {level}d{die}", {{"level", s.level}, {"die", s.hit_die}});
    if (member)
        text += i18n::formatted("   Gold {gold}   XP {xp}",
    {{"gold", coins(member->wealth, Coin::gold)}, {"xp", member->experience}});
    if (member && member->vitals.dead)
        text += "   " + i18n::utf8("Dead");
    if (member && party->can_advance(member->id))
        text += "   [b]" + i18n::utf8("Ready to level up") + "[/b]";
    const auto display = [](const std::string & id)
    {
        if (id == "archery")
            return i18n::utf8(N_("archery"));
        std::string label = id;
        std::replace(label.begin(), label.end(), '_', ' ');
        return i18n::utf8(label);
    };
    String feats;
    for (const auto &grant : s.grants)
        if (grant.id.starts_with("feat:"))
            feats += gs(display(grant.id.substr(5))) + "  ";
    if (!feats.is_empty())
        text += "\n" + i18n::formatted("Feat: {feats}", {{"feats", feats}});
    if (!s.prepared_spells.empty())
    {
        String spells;
        for (const auto &spell : s.prepared_spells)
            spells += gs(display(spell)) + "  ";
        text += "\n" + i18n::formatted("Prepared spells: {spells}", {{"spells", spells}});
    }
    text += "\n[font_size=14]" + std::string(i18n::render(s.hp_messages).utf8().get_data()) +
            "[/font_size]";
    text += i18n::utf8(
                "\n\n[table=3][cell][b]Attribute     [/b][/cell][cell][b]Score     [/b][/cell][cell][b]Saving throw[/b][/cell]");
    for (unsigned i = 0; i < 6; ++i)
    {
        const auto score = std::to_string(s.scores[i]);
        const auto colored =
            s.modifiers[i] == 0
            ? score
            : "[color=" + std::string(s.modifiers[i] > 0 ? "#f3d55b" : "#f08080") + "]" +
            score + "[/color]";
        text += "[cell]" + i18n::utf8(i18n::ability_names[i]) + "[/cell][cell]" + colored +
                "[/cell][cell]" + number(s.saving_throws[i]) +
                (s.save_proficiencies[i] ? " *" : "") + "[/cell]";
    }
    text += "[/table]\n" + i18n::utf8("* Proficient saving throw");
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
            text += "\n\n" + literal(e.what());
        }
        if (!member->vitals.description.empty())
            text += "\n" + literal(member->vitals.description);
    }
    text += "\n\n[b]" + i18n::utf8("Inventory") + "[/b]";
    if (character.inventory().empty())
        text += "\n" + i18n::utf8("Empty");
    const auto positions =
        member ? party->profile(member->id).equipment_positions : std::vector<rules::Message> {};
    for (const auto &item : character.inventory().items())
    {
        text += "\n" + literal(i18n::utf8(item.name)) + " x" + std::to_string(item.quantity);
        if (member)
        {
            const auto held = std::find(member->equipped.begin(), member->equipped.end(), item.id);
            if (held != member->equipped.end())
                text += " / " + i18n::utf8(positions.at(held - member->equipped.begin()).source);
        }
    }
    text += presentation::training_summary(s.training,
                                           i18n::text)
            .utf8()
            .get_data();
    return gs(text);
}
} // namespace

namespace presentation
{
String sheet_text(const Character &character)
{
    return formatted_sheet(character, nullptr, nullptr);
}

String sheet_text(const CampaignParty &party, const PartyMember &member)
{
    return formatted_sheet(member.character, &member, &party);
}
} // namespace presentation
