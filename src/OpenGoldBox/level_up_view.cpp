#include "godot_nodes.h"
#include "localization.h"
#include "game_resources.h"
#include "character_creation_view.h"
#include "character_sheet_text.h"
#include "level_up_dialog.h"
#include "rolf_tour_view.h"
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/v_scroll_bar.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include "godot_path.h"
#include "guarded_handlers.h"
#include <algorithm>
#include <utility>

using namespace godot;
using presentation::required_node;

namespace
{
String gs(std::string_view s)
{
    return String::utf8(s.data(), s.size());
}

} // namespace

void CharacterCreationView::setup_advancement()
{
    required_node<ItemList>(*this, "PartyPanel/Roster")
    .set_theme_type_variation("LevelUpRoster");
    const auto args = OS::get_singleton()->get_cmdline_user_args();
    advancement_check_ = args.has("--advancement-check") || args.has("--champion-creator");
    advancement_review_ = args.has("--level-up-review");
    auto dialog = LevelUpDialog::create();
    auto *level_up = Object::cast_to<LevelUpDialog>(dialog.get());
    level_up->connect_host([this]() -> opengold::CampaignParty &
    {
        return *campaign_;
    },
    [this]
    {
        refresh_party();
        refresh_advancement_arrows();
        if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown")))
            town->resume_party();
        },
    [this](const std::exception & failure)
    {
        report_failure(failure);
    });
    presentation::attach_child(*this, std::move(dialog));
}

void CharacterCreationView::refresh_advancement_arrows()
{
    auto *list = &required_node<ItemList>(*this, "PartyPanel/Roster");
    if (!list->is_visible_in_tree())
        return;
    list->force_update_list_size();
    const auto &roster = campaign_->state().roster;
    for (std::size_t i = 0; i < roster.size(); ++i)
    {
        const auto &member = roster[i];
        const auto name = String("Advance") + String::num_uint64(member.id);
        auto *arrow = Object::cast_to<Button>(list->get_node_or_null(name));
        if (!arrow)
        {
            auto owned = presentation::make_node<Button>();
            owned->set_name(name);
            arrow = presentation::attach_child(*list, std::move(owned));
            arrow->set_size(Vector2(list->get_theme_constant("party_arrow_width", "OpenGoldMetrics"),
                                    list->get_theme_constant("party_arrow_height", "OpenGoldMetrics")));
            arrow->set_text(String::utf8("↑"));
            arrow->set_tooltip_text(
            i18n::format("Level up {name}", {{"name", gs(member.character.sheet().name)}}));
            arrow->set_theme_type_variation("PartyAdvance");
            arrow->connect(
                "pressed",
                presentation::guarded(this, &CharacterCreationView::open_advancement).bind(member.id));
        }
        arrow->set_tooltip_text(
        i18n::format("Level up {name}", {{"name", gs(member.character.sheet().name)}}));
        const auto rect = list->get_item_rect(static_cast<std::int32_t>(i));
        const float y = rect.position.y - list->get_v_scroll_bar()->get_value();
        const auto font = list->get_theme_font("font");
        const float width = Vector2(font->call("get_string_size", gs(member.character.sheet().name),
                                               0, -1, list->get_theme_font_size("font_size")))
                            .x;
        arrow->set_position(Vector2(std::min(width + list->get_theme_constant(
                "party_arrow_gap", "OpenGoldMetrics"),
                list->get_size().x - list->get_theme_constant(
                    "party_arrow_right_inset", "OpenGoldMetrics")),
                                    y + list->get_theme_constant("party_arrow_offset_y", "OpenGoldMetrics")));
        arrow->set_visible(campaign_->can_advance(member.id) && arrow->get_position().y >= 0 &&
                           arrow->get_position().y + arrow->get_size().y <= list->get_size().y);
    }
    for (int n = 0; n < list->get_child_count(); ++n)
        if (auto *arrow = Object::cast_to<Button>(list->get_child(n));
                arrow && String(arrow->get_name()).begins_with("Advance"))
        {
            const auto id = String(arrow->get_name()).substr(7).to_int();
            if (std::none_of(roster.begin(), roster.end(),
                             [&](const auto & m)
        {
            return m.id == id;
        }))
            arrow->hide();
        }
}

void CharacterCreationView::open_advancement(std::int64_t member)
{
    const auto id = static_cast<opengold::MemberId>(member);
    if (campaign_defeated_ || !campaign_->can_advance(id))
        return;
    if (auto *town = Object::cast_to<RolfTourView>(get_node_or_null("CampaignTown"));
            town && town->is_visible() && !town->can_leave())
        return;
    required_node<LevelUpDialog>(*this, "LevelUp").open(id);
}

void CharacterCreationView::advancement_check()
{
    if (advancement_frames_ > 6000)
        throw std::runtime_error("Advancement check timed out");
    if (++advancement_frames_ % 4)
        return;
    refresh_advancement_arrows();
    const auto press = [&](const String &path)
    {
        auto *button = &required_node<Button>(*this, path);
        if (button->is_disabled())
            throw std::runtime_error("Disabled advancement check control: " +
                                     std::string(path.utf8().get_data()));
        button->emit_signal("pressed");
    };
    const auto arrow = [](opengold::MemberId id)
    {
        return String("PartyPanel/Roster/Advance") + String::num_uint64(id);
    };
    const auto capture_dialog = [&](const char *name)
    {
        if (!capture_)
            return;
        const auto image = required_node<Window>(*this, "LevelUp").get_texture()->get_image();
        if (image.is_null() || image->save_png(ProjectSettings::get_singleton()->globalize_path(
                String("user://checks/") + name)) != OK)
            throw std::runtime_error("Level-up capture failed");
    };
    const auto select = [&](const String &path, int index)
    {
        auto *option = &required_node<OptionButton>(*this, path);
        option->select(index);
        option->emit_signal("item_selected", index);
    };
    const auto id = campaign_->state().roster.empty() ? 0 : campaign_->state().slots[0];
    switch (advancement_stage_)
    {
    case 0:
    {
        if (OS::get_singleton()->get_cmdline_user_args().has("--champion-creator"))
        {
            opengold::rules::CharacterDraft draft;
            draft.race = "human";
            draft.gender = "female";
            draft.character_class = "fighter";
            draft.background = "sage";
            draft.name = "Champion review";
            draft.alignment = "neutral_good";
            draft.rolled = true;
            for (auto &roll : draft.rolls)
                roll = {{6, 5, 4, 1}, 3};
            const auto member = campaign_->add_pc(
                                    opengold::Character(*opengold::srd5::character_rules(), draft, {}));
            campaign_->award_experience(2700, "fixture:champion-review");
            campaign_->advance(member, campaign_->default_advancement(member));
            party_action(PartyAction::open);
            refresh_party();
            refresh_advancement_arrows();
            open_advancement(member);
            advancement_check_ = false;
            advancement_review_ = false;
            return;
        }
        for (const char *klass :
                {"wizard", "fighter", "cleric", "fighter"
                })
        {
            opengold::rules::CharacterDraft draft;
            draft.race =
                advancement_check_ && std::string_view(klass) == "wizard" ? "dwarf" : "human";
            draft.gender = "female";
            draft.character_class = klass;
            draft.alignment = "neutral_good";
            draft.background = klass == std::string_view("fighter") ? "soldier" : "sage";
            draft.name = std::string(klass == std::string_view("wizard")    ? "Mira"
                                     : klass == std::string_view("fighter") ? "Tessa"
                                     : "Lena") +
                                                               " / " + klass;
            draft.rolled = true;
            for (auto &roll : draft.rolls)
                roll = {{6, 5, 4, 1}, 3};
            campaign_->add_pc(opengold::Character(*opengold::srd5::character_rules(), draft, {}));
        }
        campaign_->award_experience(2700, "fixture:level-up-review");
        const auto slots = campaign_->state().slots;
        for (const auto member : slots)
            if (member)
                for (unsigned level = 2; level <= 3; ++level)
                    campaign_->advance(member, campaign_->default_advancement(member));
        party_action(PartyAction::open);
        error_ =
            "Review party: each character is ready for level 4. Click the arrow beside a name.";
        refresh_party();
        refresh_advancement_arrows();
        if (advancement_review_)
        {
            advancement_review_ = false;
            return;
        }
        break;
    }
    case 1:
    {
        auto *list = &required_node<ItemList>(*this, "PartyPanel/Roster");
        for (unsigned i = 0; i < 4; ++i)
        {
            auto *button = &required_node<Button>(*this, arrow(campaign_->state().slots[i]));
            if (std::cmp_not_equal(
                        list->get_item_at_position(button->get_position() + button->get_size() / 2,
                                                   true),
                        i))
                throw std::runtime_error("Level-up arrow is not beside its own character row");
        }
        capture("level-up-arrows.png");
        press(arrow(id));
        break;
    }
    case 2:
    {
        capture_dialog("level-up-wizard.png");
        const auto before = opengold::encode_campaign(*campaign_, nullptr, "ui-check");
        press("LevelUp/Cancel");
        if (opengold::encode_campaign(*campaign_, nullptr, "ui-check") != before)
            throw std::runtime_error("Cancel mutated campaign");
        press(arrow(id));
        for (int i = 0; i < 6; ++i)
            select(String("LevelUp/Ability") + String::num_int64(i), 0);
        if (!required_node<Button>(*this, "LevelUp/Confirm").is_disabled())
            throw std::runtime_error("Incomplete points can be confirmed");
        select("LevelUp/Ability2", 2);
        required_node<CheckBox>(*this, "LevelUp/Spell1").set_pressed(true);
        break;
    }
    case 3:
        capture_dialog("level-up-choices.png");
        press("LevelUp/Confirm");
        if (!required_node<Control>(*this, "LevelUp/SpellChoicesPage").is_visible())
            throw std::runtime_error("A Wizard's spell page follows the first page");
        press("LevelUp/Confirm");
        if (campaign_->member(id).character.sheet().level != 4 ||
                // The SRD Wizard prepares seven spells at level 4.
                campaign_->member(id).character.sheet().prepared_spells.size() != 7 ||
                required_node<Button>(*this, arrow(id)).is_visible())
            throw std::runtime_error("Wizard confirmation did not apply choices and hide arrow");
        show_modifiers();
        {
            const auto text = required_node<RichTextLabel>(*this, "ModifiersModal/Text").get_text();
            if (!text.contains(
            i18n::format("{background} background", {{"background", i18n::text("Sage")}}) +
            " (+2)\n" +
            i18n::format("Level {level} Ability Score Improvement", {{"level", 4}}) +
            " (+2)\n" + i18n::format("Final score: {score}", {{"score", 19}})) ||
            text.contains(
            i18n::format("{background} background", {{"background", i18n::text("Sage")}}) +
            " (+4)"))
            throw std::runtime_error(
                "Modifier dialog must separate background and level-four feat sources");
            if (!text.contains(i18n::format("Dwarven Toughness: +{hp} maximum HP.", {{"hp", 4}})))
            throw std::runtime_error(
                "Racial section must show the attained Dwarven Toughness contribution");
            const auto saved = opengold::encode_campaign(*campaign_, nullptr, "bonus-ui-check");
            const auto module =
                opengold::srd5::load(presentation::path_from_godot(game_rules_file()));
            auto restored = opengold::decode_campaign(saved, *opengold::srd5::character_rules(),
                *module, "bonus-ui-check", nullptr);
            campaign_->restore(std::move(restored.party));
            show_modifiers();
            if (required_node<RichTextLabel>(*this, "ModifiersModal/Text").get_text() != text)
                throw std::runtime_error(
                    "Saved bonus sources must reconstruct the same modifier dialog");
        }
        close_modifiers();
        party_action(PartyAction::explore);
        break;
    case 4:
    {
        auto *town = &required_node<RolfTourView>(*this, "CampaignTown");
        if (!town->can_leave())
        {
            auto *next = &required_node<Button>(*town, "Continue");
            if (next->is_visible() && !next->is_disabled())
                next->emit_signal("pressed");
            return;
        }
        town->resume_party();
        auto *button = &required_node<Button>(*town, "PartyList/Rows/Member1/Advance");
        if (!button->is_visible())
            throw std::runtime_error("Town level-up arrow is missing");
        button->emit_signal("pressed");
        if (!required_node<Window>(*this, "LevelUp").is_visible())
            throw std::runtime_error("Town arrow did not open advancement");
        select("LevelUp/Feat", 1);
        // A level-four Fighter also masters a fourth weapon.
        select("LevelUp/AdvancementTraining", 1);
        break;
    }
    case 5:
    {
        capture_dialog("level-up-fighter.png");
        press("LevelUp/Confirm");
        const auto &fighter = campaign_->member(campaign_->state().slots[1]).character;
        const auto has = [&](const opengold::rules::FeatureGrant & grant)
        {
            return std::find(fighter.sheet().grants.begin(), fighter.sheet().grants.end(), grant) !=
                   fighter.sheet().grants.end();
        };
        if (!has({"feat:defense", "class:fighter:ability_score_improvement", 4, {}}) ||
                !has({"feat:savage_attacker", "background:soldier", 1, {}}))
            throw std::runtime_error(
                "Fighter must retain separate creation and advancement grants");
        const auto text = presentation::sheet_text(fighter);
        if (!text.contains(i18n::text("savage attacker")) || !text.contains(i18n::text("defense")))
            throw std::runtime_error("Sheet must display both acquired feats");
    }
    required_node<Button>(required_node<RolfTourView>(*this, "CampaignTown"),
                          "PartyList/Rows/Member2/Advance")
    .emit_signal("pressed");
    select("LevelUp/Feat", 2);
    required_node<CheckBox>(*this, "LevelUp/Spell1").set_pressed(true);
    break;
    case 6:
    {
        capture_dialog("level-up-cleric.png");
        press("LevelUp/Confirm");
        if (!required_node<Control>(*this, "LevelUp/SpellChoicesPage").is_visible())
            throw std::runtime_error("A Cleric's spell page follows the first page");
        press("LevelUp/Confirm");
        const auto cleric_id = campaign_->state().slots[2];
        const auto has_feat = [&]
        {
            const auto &grants = campaign_->member(cleric_id).character.sheet().grants;
            return std::find(grants.begin(), grants.end(),
            opengold::rules::FeatureGrant{"feat:savage_attacker",
                "class:cleric:ability_score_improvement",
                4,
                {}}) != grants.end();
        };
        // The SRD Cleric prepares seven spells at level 4.
        if (!has_feat() ||
                campaign_->member(cleric_id).character.sheet().prepared_spells.size() != 7)
            throw std::runtime_error(
                "Cleric selection must grant Savage Attacker with its source and chosen spells");
        const auto saved = opengold::encode_campaign(*campaign_, nullptr, "feat-ui-check");
        const auto module =
            opengold::srd5::load(presentation::path_from_godot(game_rules_file()));
        auto restored = opengold::decode_campaign(saved, *opengold::srd5::character_rules(),
            *module, "feat-ui-check", nullptr);
        campaign_->restore(std::move(restored.party));
        if (!has_feat() || opengold::encode_campaign(*campaign_, nullptr, "feat-ui-check") != saved)
            throw std::runtime_error("UI-acquired feat must survive campaign reload");
        required_node<Button>(required_node<RolfTourView>(*this, "CampaignTown"),
                              "PartyList/Rows/Member3/Advance")
        .emit_signal("pressed");
        auto *feats = &required_node<OptionButton>(*this, "LevelUp/Feat");
        const auto &offered = required_node<LevelUpDialog>(*this, "LevelUp").options().feats;
        const auto archery =
            std::find_if(offered.begin(), offered.end(),
                         [](const auto & f)
        {
            return f.id == "archery";
        });
        if (archery == offered.end() || !archery->available)
            throw std::runtime_error("Fighter Archery must be selectable");
        select("LevelUp/Feat", static_cast<int>(archery - offered.begin()));
        if (feats->get_item_text(feats->get_selected()) != i18n::text("Archery"))
            throw std::runtime_error("Archery choice must be translated");
        select("LevelUp/AdvancementTraining", 1);
        break;
    }
    case 7:
    {
        capture_dialog("level-up-archery.png");
        press("LevelUp/Confirm");
        const auto archer = campaign_->state().slots[3];
        const auto &sheet = campaign_->member(archer).character.sheet();
        if (std::find(sheet.grants.begin(), sheet.grants.end(),
                      opengold::rules::FeatureGrant
    {
        "feat:archery", "class:fighter:ability_score_improvement", 4, {}}) ==
    sheet.grants.end())
        throw std::runtime_error("Confirmed Archery lacks its entitlement grant");
        if (!presentation::sheet_text(campaign_->member(archer).character)
                .contains(i18n::text("archery")))
            throw std::runtime_error("Sheet must display translated Archery");
        const auto bytes = opengold::encode_campaign(*campaign_, nullptr, "archery-ui-check");
        const auto module =
            opengold::srd5::load(presentation::path_from_godot(game_rules_file()));
        auto restored = opengold::decode_campaign(bytes, *opengold::srd5::character_rules(),
            *module, "archery-ui-check", nullptr);
        campaign_->restore(std::move(restored.party));
        if (opengold::encode_campaign(*campaign_, nullptr, "archery-ui-check") != bytes)
            throw std::runtime_error("UI-acquired Archery must survive reload");
        party_action(PartyAction::return_to_party);
        capture("level-up-complete.png");
        UtilityFunctions::print(
            "Godot advancement passed: roster and town arrows, HP preview, Cancel rollback, invalid-point prevention, level-four feat and spell confirmations.");
        advancement_check_ = false;
        get_tree()->quit(0);
        break;
    }
    }
    ++advancement_stage_;
}
