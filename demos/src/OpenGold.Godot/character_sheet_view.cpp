#include "../../../src/OpenGoldBox/training_control.h"
#include "character_creation_view.h"
#include "opengold/srd5.h"
#include "../../../src/OpenGoldBox/godot_path.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <algorithm>

using namespace godot;
using namespace opengold;

namespace
{
const std::array<const char *, 6> names{"Strength",     "Dexterity", "Constitution",
    "Intelligence", "Wisdom",    "Charisma"};

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

struct DeleteNode
{
    void operator()(Node *node) const
    {
        memdelete(node);
    }
};
} // namespace

Variant CharacterCreationView::drag_roll(Vector2, int index)
{
    if (!creator_ || party_open_ || creator_->step() != CreationStep::attributes ||
            !creator_->draft().rolled || index < 0 || index >= 12)
        return {};
    const auto &d = creator_->draft();
    const unsigned roll = index < 6 ? index : d.assignment[index - 6];
    if (roll >= 6 || (index < 6 && std::find(d.assignment.begin(), d.assignment.end(), roll) !=
                      d.assignment.end()))
        return {};
    auto preview = std::unique_ptr<Label, DeleteNode>(memnew(Label));
    preview->set_text(gs(std::to_string(d.rolls[roll].total())));
    get_node<Control>(gs(std::string(index < 6 ? "Dice" : "Score") + std::to_string(index % 6)))
    ->set_drag_preview(preview.get());
    preview.release();
    Dictionary data;
    data["opengold_ability_roll"] = roll;
    return data;
}

bool CharacterCreationView::can_drop_roll(Vector2, const Variant &data, int index)
{
    if (!creator_ || party_open_ || creator_->step() != CreationStep::attributes ||
            !creator_->draft().rolled || index < 0 || index >= 6 ||
            data.get_type() != Variant::DICTIONARY)
        return false;
    const Dictionary payload = data;
    const Variant roll = payload.get("opengold_ability_roll", Variant());
    return roll.get_type() == Variant::INT && int(roll) >= 0 && int(roll) < 6;
}

void CharacterCreationView::drop_roll(Vector2 position, const Variant &data, int index)
{
    if (!can_drop_roll(position, data, index))
        return;
    const Dictionary payload = data;
    const unsigned roll = int(payload["opengold_ability_roll"]);
    perform(
        [&]
    {
        creator_->assign_roll(roll, static_cast<rules::Ability>(index));
        selected_score_ = -1;
    });
}

String CharacterCreationView::sheet_text(const Character &character,
        const PartyMember *member) const
{
    const auto &s = character.sheet();
    std::string text = "[font_size=24]" + literal(s.name) + "[/font_size]\nLevel " +
                       std::to_string(s.level) + " " + s.race + " " + s.gender + " " +
                       s.character_class + "\n" + s.alignment + " / " + s.background + "\n\n";
    if (!character.creation_data().target_classes.empty())
    {
        text += "Future class goals: ";
        bool first = true;
        const auto options =
            srd5::character_rules()->choices(rules::CreationField::character_class);
        for (const auto &id : character.creation_data().target_classes)
            for (const auto &option : options)
                if (option.id == id)
                {
                    if (!first)
                        text += ", ";
                    text += option.label;
                    first = false;
                }
        text += "\n\n";
    }
    const int hp_modifier = s.hit_points - s.hit_die;
    const auto hp_number = [&](int hp)
    {
        const auto value = std::to_string(hp);
        return hp_modifier == 0 ? value
               : "[color=" + std::string(hp_modifier > 0 ? "#f3d55b" : "#f08080") +
               "]" + value + "[/color]";
    };
    text += "[b]HP " + hp_number(member ? member->vitals.hit_points : s.hit_points) + " / " +
            hp_number(s.hit_points) + "[/b]   Hit Dice: " + std::to_string(s.level) + "d" +
            std::to_string(s.hit_die);
    if (member)
        text +=
            "   Gold " + std::to_string(member->wealth[3]) + (member->vitals.dead ? "   Dead" : "");
    if (member)
        text += "   XP " + std::to_string(member->experience);
    if (member && campaign_->can_advance(member->id))
        text += "   [b]Ready to level up[/b]";
    const auto display = [](const std::string & id)
    {
        std::string label = id;
        std::replace(label.begin(), label.end(), '_', ' ');
        return label;
    };
    std::string feats;
    for (const auto &grant : s.grants)
        if (grant.id.starts_with("feat:"))
            feats += display(grant.id.substr(5)) + "  ";
    if (!feats.empty())
        text += "\nFeat: " + feats;
    if (!s.prepared_spells.empty())
    {
        text += "\nPrepared spells: ";
        for (const auto &spell : s.prepared_spells)
            text += display(spell) + "  ";
    }
    text += "\n[font_size=14]" + literal(s.hp_explanation) + "[/font_size]";
    text +=
        "\n\n[table=3][cell][b]Attribute     [/b][/cell][cell][b]Score     [/b][/cell][cell][b]Saving throw[/b][/cell]";
    for (unsigned i = 0; i < 6; ++i)
    {
        const auto score = std::to_string(s.scores[i]);
        const auto colored =
            s.modifiers[i] == 0
            ? score
            : "[color=" + std::string(s.modifiers[i] > 0 ? "#f3d55b" : "#f08080") + "]" +
            score + "[/color]";
        text += "[cell]" + std::string(names[i]) + "[/cell][cell]" + colored + "[/cell][cell]" +
                number(s.saving_throws[i]) + (s.save_proficiencies[i] ? " *" : "") + "[/cell]";
    }
    text += "[/table]\n* Proficient saving throw";
    if (member)
    {
        try
        {
            const auto p = campaign_->profile(member->id);
            text += "\n\nAC " + std::to_string(p.armor_class) + "   Speed " +
                    std::to_string(p.movement_feet) + " ft";
        }
        catch (const std::exception &e)
        {
            text += "\n\n" + literal(e.what());
        }
        if (!member->vitals.description.empty())
            text += "\n" + literal(member->vitals.description);
    }
    text += "\n\n[b]Inventory[/b]";
    if (character.inventory().empty())
        text += "\nEmpty";
    const auto positions =
        member ? campaign_->profile(member->id).equipment_positions : std::vector<rules::Message> {};
    for (const auto &item : character.inventory().items())
    {
        text += "\n" + literal(std::string(item.name)) + " x" + std::to_string(item.quantity);
        if (member)
        {
            const auto held = std::find(member->equipped.begin(), member->equipped.end(), item.id);
            if (held != member->equipped.end())
                text += " / " + std::string(positions.at(held - member->equipped.begin()).source);
        }
    }
    text += presentation::training_summary(s.training,
                                           [](std::string_view source)
    {
        return gs(source);
    })
    .utf8()
    .get_data();
    return gs(text);
}

void CharacterCreationView::show_modifiers()
{
    const PartyMember *member = nullptr; // Borrowed only for this synchronous rendering.
    if (party_open_ && !campaign_->state().roster.empty())
        member = &campaign_->state().roster.at(roster_index_);
    if (!member && !completed_)
        return;
    const auto &s = member ? member->character.sheet() : completed_->sheet();
    std::string text = "[b]Ability score adjustments[/b]\n";
    for (unsigned i = 0; i < 6; ++i)
    {
        if (s.bonuses[i] == 0)
            continue;
        text += "[b]" + std::string(names[i]) + "[/b]\nRolled score: " + std::to_string(s.base[i]) +
                "\n";
        for (const auto &source : s.ability_adjustments)
            if (source.bonuses[i])
                text += source.label + " (" + number(source.bonuses[i]) + ")\n";
        text += "Final score: " + std::to_string(s.scores[i]) + "\n\n";
    }
    text += "\n[b]Race / " + s.race + "[/b]\n" + s.racial_modifiers;
    text += "\n\n[b]Class / " + s.character_class + "[/b]\n" + s.class_modifiers;
    text += "\n\n[b]Background / " + s.background + "[/b]\n" + s.background_modifiers;
    try
    {
        const auto pack = presentation::path_from_godot(
                              ProjectSettings::get_singleton()
                              ->globalize_path("res://../../data/rules/srd-5.2.1/combat.rules"));
        const auto profile =
            member ? campaign_->profile(member->id) : srd5::load(pack)->character_profile(s, {});
        text += "\n\n[b]Items[/b]\n" + profile.item_modifiers + "\n\n[b]Spells[/b]\n" +
                profile.spell_modifiers + "\n\n" + profile.description;
    }
    catch (const std::exception &e)
    {
        text +=
            "\n\n[b]Items[/b]\nNo equipped item modifiers.\n\n[b]Spells[/b]\nNo active spell modifiers. Persistent spell effects are not implemented.";
        text += "\n\n" + literal(e.what());
    }
    get_node<RichTextLabel>("ModifiersModal/Text")->set_text(gs(text));
    get_node<Label>("ModifiersModal/Title")->set_text(gs(s.name + " / Modifiers"));
    auto *modal = get_node<Window>("ModifiersModal");
    modal->popup_centered();
    get_node<Button>("ModifiersModal/Close")->grab_focus();
}

void CharacterCreationView::close_modifiers()
{
    get_node<Window>("ModifiersModal")->hide();
}

void CharacterCreationView::show_saving_throws()
{
    update_saving_throws(get_node<LineEdit>("SavingThrowsModal/DC")->get_text());
    get_node<Window>("SavingThrowsModal")->popup_centered();
    get_node<LineEdit>("SavingThrowsModal/DC")->grab_focus();
}

void CharacterCreationView::close_saving_throws()
{
    get_node<Window>("SavingThrowsModal")->hide();
}

void CharacterCreationView::update_saving_throws(String value)
{
    const PartyMember *member = nullptr; // Borrowed for synchronous rendering.
    if (party_open_ && !campaign_->state().roster.empty())
        member = &campaign_->state().roster.at(roster_index_);
    if (!member && !completed_)
        return;
    const auto &s = member ? member->character.sheet() : completed_->sheet();
    get_node<Label>("SavingThrowsModal/Title")->set_text(gs(s.name + " / Saving Throws"));
    auto *label = get_node<RichTextLabel>("SavingThrowsModal/Text");
    if (!value.is_valid_int() || value.to_int() < 1 || value.to_int() > 999)
    {
        label->set_text("Enter a whole-number target DC from 1 to 999.");
        return;
    }
    const int dc = static_cast<int>(value.to_int());
    const bool disadvantage =
        member && campaign_->profile(member->id).strength_dexterity_disadvantage;
    std::string text = "Roll a d20 and add the saving throw bonus. Meet or exceed DC " +
                       std::to_string(dc) + " to save.\n\n";
    for (unsigned i = 0; i < 6; ++i)
    {
        const int needed = srd5::minimum_save_roll(dc, s.saving_throws[i]);
        text += "[b]" + std::string(names[i]) + " save: " + number(s.saving_throws[i]) + " | ";
        text += needed > 20   ? "Cannot reach this DC on a d20"
                : needed == 1 ? "Any d20 roll saves"
                : "Roll " + std::to_string(needed) + " or higher";
        text += "[/b]\n" + number(s.modifiers[i]) + " from " + names[i] + " score " +
                std::to_string(s.scores[i]) + " (score minus 10, divided by 2, rounded down).\n";
        text += s.save_proficiencies[i] ? number(s.saving_throws[i] - s.modifiers[i]) + " from " +
                s.character_class + " saving throw proficiency."
                : "+0 proficiency: " + s.character_class +
                            " does not grant proficiency in this save.";
        if (disadvantage && i < 2)
            text += "\nDisadvantage from untrained armor: roll two d20s and use the lower roll.";
        text += "\n\n";
    }
    text +=
        "No additional racial, item or spell bonuses are currently applied to these saves. Conditional traits and persistent spell effects are not implemented.\n\nOrdinary saving throws: a natural 1 or 20 does not automatically fail or succeed. Death saves use separate rules.";
    label->set_text(gs(text));
}
