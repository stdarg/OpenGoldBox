#include "training_control.h"
#include "godot_nodes.h"
#include "hp_presentation.h"
#include "game_resources.h"
#include "character_text.h"
#include "character_creation_view.h"
#include "opengold/srd5.h"
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
    auto preview = presentation::make_node<Label>();
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
            creator_->assign_roll(roll, index);
            selected_score_ = -1;
        });
}

String CharacterCreationView::sheet_text(const Character &character,
                                         const PartyMember *member) const
{
    const auto &s = character.sheet();
    std::string text = i18n::formatted(
        "[font_size=24]{name}[/font_size]\nLevel {level} / {race} / {gender} / {class}\n{alignment} / {background}\n\n",
        {{"name", gs(literal(s.name))},
         {"level", s.level},
         {"race", i18n::text(s.race)},
         {"gender", i18n::text(s.gender)},
         {"class", i18n::text(s.character_class)},
         {"alignment", i18n::text(s.alignment)},
         {"background", i18n::text(s.background)}});
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
        temporary = campaign_->recovery_info(member->id).temporary_hp;
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
                                {{"gold", member->wealth[3]}, {"xp", member->experience}});
    if (member && member->vitals.dead)
        text += "   " + i18n::utf8("Dead");
    if (member && campaign_->can_advance(member->id))
        text += "   [b]" + i18n::utf8("Ready to level up") + "[/b]";
    const auto display = [](const std::string &id)
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
        text += "[cell]" + i18n::utf8(names[i]) + "[/cell][cell]" + colored + "[/cell][cell]" +
                number(s.saving_throws[i]) + (s.save_proficiencies[i] ? " *" : "") + "[/cell]";
    }
    text += "[/table]\n" + i18n::utf8("* Proficient saving throw");
    if (member)
    {
        try
        {
            const auto p = campaign_->profile(member->id);
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
    for (const auto &item : character.inventory().items())
        text += "\n" + literal(i18n::utf8(item.name)) + " x" + std::to_string(item.quantity);
    text += presentation::training_summary(s.training,
                                           [](std::string_view source)
                                           {
                                               return i18n::text(source);
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
    std::string text = "[b]" + i18n::utf8("Ability score adjustments") + "[/b]\n";
    for (unsigned i = 0; i < 6; ++i)
    {
        if (s.bonuses[i] == 0)
            continue;
        text += "[b]" + i18n::utf8(names[i]) + "[/b]\n" +
                i18n::formatted("Rolled score: {score}", {{"score", s.base[i]}}) + "\n";
        for (const auto &source : s.ability_adjustments)
            if (source.bonuses[i])
                text += i18n::formatted("{source} ({bonus})",
                                        {{"source", i18n::render(source.label_message)},
                                         {"bonus", gs(number(source.bonuses[i]))}}) +
                        "\n";
        text += i18n::formatted("Final score: {score}", {{"score", s.scores[i]}}) + "\n\n";
    }
    text += "\n[b]" + i18n::formatted("Race / {race}", {{"race", i18n::text(s.race)}}) + "[/b]\n" +
            i18n::render(s.racial_messages).utf8().get_data();
    text += "\n\n[b]" +
            i18n::formatted("Class / {class}", {{"class", i18n::text(s.character_class)}}) +
            "[/b]\n" + i18n::render(s.class_messages).utf8().get_data();
    text +=
        "\n\n[b]" +
        i18n::formatted("Background / {background}", {{"background", i18n::text(s.background)}}) +
        "[/b]\n" + i18n::render(s.background_messages).utf8().get_data();
    try
    {
        const auto pack = std::filesystem::u8path(game_rules_file().utf8().get_data());
        const auto profile =
            member ? campaign_->profile(member->id) : srd5::load(pack)->character_profile(s, {});
        text += "\n\n[b]" + i18n::utf8("Items") + "[/b]\n" +
                i18n::render(profile.item_messages).utf8().get_data();
        text += "\n\n[b]" + i18n::utf8("Spells") + "[/b]\n" +
                i18n::render(profile.spell_messages).utf8().get_data();
        text += "\n\n" + i18n::utf8(profile.description);
    }
    catch (const std::exception &e)
    {
        text += i18n::utf8(
            "\n\n[b]Items[/b]\nNo equipped item modifiers.\n\n[b]Spells[/b]\nNo active spell modifiers. Persistent spell effects are not implemented.");
        text += "\n\n" + literal(e.what());
    }
    get_node<RichTextLabel>("ModifiersModal/Text")->set_text(gs(text));
    get_node<Label>("ModifiersModal/Title")
        ->set_text(i18n::format("{name} / Modifiers", {{"name", gs(s.name)}}));
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
    get_node<Label>("SavingThrowsModal/Title")
        ->set_text(i18n::format("{name} / Saving Throws", {{"name", gs(s.name)}}));
    auto *label = get_node<RichTextLabel>("SavingThrowsModal/Text");
    if (!value.is_valid_int() || value.to_int() < 1 || value.to_int() > 999)
    {
        label->set_text(i18n::text(N_("Enter a whole-number target DC from 1 to 999.")));
        return;
    }
    const int dc = static_cast<int>(value.to_int());
    const bool disadvantage =
        member && campaign_->profile(member->id).strength_dexterity_disadvantage;
    std::string text = i18n::formatted(
        "Roll a d20 and add the saving throw bonus. Meet or exceed DC {dc} to save.\n\n",
        {{"dc", dc}});
    for (unsigned i = 0; i < 6; ++i)
    {
        const int needed = srd5::minimum_save_roll(dc, s.saving_throws[i]);
        const auto target = needed > 20 ? i18n::text("Cannot reach this DC on a d20")
                            : needed == 1
                                ? i18n::text("Any d20 roll saves")
                                : i18n::format("Roll {needed} or higher", {{"needed", needed}});
        text += i18n::formatted(
            "[b]{ability} save: {bonus} | {target}[/b]\n{modifier} from {ability} score {score} (score minus 10, divided by 2, rounded down).\n",
            {{"ability", i18n::text(names[i])},
             {"bonus", gs(number(s.saving_throws[i]))},
             {"target", target},
             {"modifier", gs(number(s.modifiers[i]))},
             {"score", s.scores[i]}});
        text += s.save_proficiencies[i]
                    ? i18n::formatted("{bonus} from {class} saving throw proficiency.",
                                      {{"bonus", gs(number(s.saving_throws[i] - s.modifiers[i]))},
                                       {"class", i18n::text(s.character_class)}})
                    : i18n::formatted(
                          "+0 proficiency: {class} does not grant proficiency in this save.",
                          {{"class", i18n::text(s.character_class)}});
        if (disadvantage && i < 2)
            text += "\n" +
                    i18n::utf8(
                        "Disadvantage from untrained armor: roll two d20s and use the lower roll.");
        text += "\n\n";
    }
    text += i18n::utf8(
        "No additional racial, item or spell bonuses are currently applied to these saves. Conditional traits and persistent spell effects are not implemented.\n\nOrdinary saving throws: a natural 1 or 20 does not automatically fail or succeed. Death saves use separate rules.");
    label->set_text(gs(text));
}
