// One implementation for the game and demo; the host supplies rest_text().
#include "godot_nodes.h"
#include "spell_choice_controls.h"
#include "training_replacement_controls.h"
#include "vital_fixtures.h"
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/popup_menu.hpp>
#ifndef N_
#define N_(message) message
#endif
void RolfTourView::setup_rest()
{
    auto *spells = presentation::setup_spell_dialog(
                       *this, "RestSpells", callable_mp(this, &RolfTourView::rest_spell_keep),
                       callable_mp(this, &RolfTourView::rest_spell_apply), rest_text);
    spells->get_node<Button>("Cancel")->set_text(rest_text(N_("Keep current")));
    spells->get_node<OptionButton>("Replace")->connect(
        "item_selected", callable_mp(this, &RolfTourView::rest_spell_replaced));
    spells->get_node<OptionButton>("With")->connect(
        "item_selected", callable_mp(this, &RolfTourView::rest_spell_replaced));
    spells->connect("window_input", callable_mp(this, &RolfTourView::rest_spell_input));
    auto *training = presentation::setup_training_replacement(
                         *this, callable_mp(this, &RolfTourView::rest_training_keep),
                         callable_mp(this, &RolfTourView::rest_training_apply), rest_text);
    training->connect("window_input", callable_mp(this, &RolfTourView::rest_training_input));
    auto *training_save =
        presentation::add_control<Button>(*training, "Save", Rect2(24, 614, 234, 40));
    training_save->set_text(rest_text(N_("Save game")));
    training_save->connect("pressed", callable_mp(this, &RolfTourView::rest_save));
    auto owned = presentation::make_node<Window>();
    owned->set_name("RestDialog");
    owned->set_title(rest_text(N_("Rest")));
    owned->set_size(Vector2i(720, 640));
    owned->set_min_size(Vector2i(720, 640));
    owned->set_flag(Window::FLAG_RESIZE_DISABLED, true);
    owned->set_transient(true);
    owned->set_exclusive(true);
    owned->hide();
    auto *w = presentation::attach_child(*this, std::move(owned));
    w->connect("close_requested", callable_mp(this, &RolfTourView::rest_finish));
    w->connect("window_input", callable_mp(this, &RolfTourView::rest_input));
    auto *label = presentation::add_control<Label>(*w, "KindLabel", Rect2(24, 18, 160, 34));
    label->set_text(rest_text(N_("Rest type")));
    auto *kind = presentation::add_control<OptionButton>(*w, "Kind", Rect2(190, 18, 506, 36));
    kind->add_item(rest_text(N_("Short Rest")), 0);
    kind->add_item(rest_text(N_("Long Rest")), 1);
    kind->select(1);
    kind->connect("item_selected", callable_mp(this, &RolfTourView::rest_selected));
    auto *list = presentation::add_control<ItemList>(*w, "Members", Rect2(24, 68, 672, 192));
    list->connect("item_selected", callable_mp(this, &RolfTourView::rest_selected));
    auto *info = presentation::add_control<RichTextLabel>(*w, "Info", Rect2(24, 274, 672, 208));
    info->set_scroll_active(true);
    auto *result = presentation::add_control<Label>(*w, "Result", Rect2(24, 490, 672, 70));
    result->set("autowrap_mode", 3);
    auto button = [&](const char *name, const char *text, Rect2 rect, Callable callback)
    {
        auto *b = presentation::add_control<Button>(*w, name, rect);
        b->set_text(rest_text(text));
        b->connect("pressed", callback);
        return b;
    };
    button("Start", N_("Start"), Rect2(24, 580, 180, 40),
           callable_mp(this, &RolfTourView::rest_start));
    button("Heal", N_("Heal with Hit Dice"), Rect2(24, 580, 220, 40),
           callable_mp(this, &RolfTourView::rest_heal));
    button("Resume", N_("Resume Long Rest"), Rect2(24, 580, 220, 40),
           callable_mp(this, &RolfTourView::rest_resume));
    button("Save", N_("Save game"), Rect2(258, 580, 210, 40),
           callable_mp(this, &RolfTourView::rest_save));
    button("Finish", N_("Cancel"), Rect2(482, 580, 214, 40),
           callable_mp(this, &RolfTourView::rest_finish));
    auto *recovery_label =
        presentation::add_control<Label>(*w, "RecoveryLabel", Rect2(24, 412, 672, 28));
    recovery_label->set_text(rest_text(N_("Arcane Recovery")));
    presentation::add_control<OptionButton>(*w, "RecoveryChoice", Rect2(24, 446, 442, 36));
    button("Recover", N_("Recover slots"), Rect2(482, 446, 214, 36),
           callable_mp(this, &RolfTourView::rest_recover));
}

void RolfTourView::camp()
{
    if (!campaign_ || !session_ || !session_->can_leave() || campaign_->in_combat())
        return;
    rest_save_open_ = false;
    rest_result_ = String();
    refresh_rest();
    auto *w = get_node<Window>("RestDialog");
    w->popup_centered();
    w->get_node<OptionButton>("Kind")->grab_focus();
}

void RolfTourView::rest_selected(std::int64_t)
{
    auto *list = get_node<Window>("RestDialog")->get_node<ItemList>("Members");
    const auto selected = list->get_selected_items();
    if (!selected.is_empty())
        rest_member_ =
            static_cast<unsigned>(static_cast<int64_t>(list->get_item_metadata(selected[0])));
    refresh_rest();
}

void RolfTourView::refresh_rest()
{
    refresh_rest_spells();
    refresh_rest_training();
    if (campaign_ && (campaign_->state().spell_rest || campaign_->state().training_rest))
    {
        get_node<Window>("RestDialog")->hide();
        return;
    }
    auto *w = get_node<Window>("RestDialog");
    if (!campaign_ || !session_ || campaign_->in_combat() ||
            (!session_->can_leave() &&
             !(session_->pending_encounter() && campaign_->state().short_rest)))
    {
        w->hide();
        return;
    }
    const auto &state = campaign_->state();
    const bool spending = state.short_rest.has_value(), retained = state.rest_activity.has_value();
    auto *kind = w->get_node<OptionButton>("Kind");
    kind->set_disabled(spending || retained);
    if (retained)
        kind->select(1);
    else if (spending)
        kind->select(0);
    const auto selected_kind = kind->get_selected_id() == 0 ? opengold::RestKind::short_rest
                               : opengold::RestKind::long_rest;
    const auto infos = campaign_->rest_info(selected_kind);
    auto *list = w->get_node<ItemList>("Members");
    list->clear();
    if (!rest_member_ || std::none_of(infos.begin(), infos.end(),
                                      [&](const auto & i)
{
    return i.id == rest_member_;
}))
    rest_member_ = infos.empty() ? 0 : infos.front().id;
    String details;
    bool eligible = false, healable = false;
    auto *recovery = w->get_node<OptionButton>("RecoveryChoice");
    const String previous =
        recovery->get_selected() >= 0 ? String(recovery->get_selected_metadata()) : String();
    recovery->clear();
    bool recovery_visible = false;
    for (const auto &info : infos)
    {
        const auto &m = campaign_->member(info.id);
        const auto &r = info.recovery;
        const bool earned = spending && std::find(state.short_rest->members.begin(),
            state.short_rest->members.end(),
            info.id) != state.short_rest->members.end();
        const bool can_start = info.denial == opengold::RestDenial::none;
        eligible |= can_start;
        auto row = String::utf8(m.character.sheet().name.c_str()) + "   " + rest_text(N_("HP")) +
                   " " + String::num_int64(m.vitals.hit_points) + "/" +
                   String::num_int64(m.character.sheet().hit_points) + "   " +
                   rest_text(N_("Hit Dice")) + " " + String::num_int64(r.hit_dice) + "/" +
                   String::num_int64(r.hit_dice_max) + "d" + String::num_int64(r.hit_die);
        const int index = list->add_item(row);
        list->set_item_metadata(index, static_cast<int64_t>(info.id));
        if (info.id != rest_member_)
            continue;
        list->select(index);
        details = String::utf8(m.character.sheet().name.c_str()) + "\n";
        if (spending)
            details +=
                rest_text(earned ? N_("Short Rest completed. Heal with Hit Dice spends dice until full HP or none remain.")
                          : N_("This member did not complete the rest."));
        else if (retained)
            details += rest_text(N_(
                                     "The Long Rest is interrupted. Resume after resolving the interruption, or end the rest."));
        else if (can_start)
            details += rest_text(N_("Eligible to rest."));
        else if (info.denial == opengold::RestDenial::cooldown)
            details += rest_text(N_("Long Rest cooldown remaining (minutes):")) + " " +
                       String::num_int64((info.wait_milliseconds + 59999) / 60000);
        else
            details += rest_text(N_("Rest requires at least 1 HP."));
        details += "\n\n" + rest_text(N_("Resource recovery")) + "\n";
        for (const auto &pool : r.resources)
        {
            details += rest_text(pool.label.source) + ": " + String::num_int64(pool.remaining) +
                       "/" + String::num_int64(pool.capacity) + "   ";
            details += rest_text(N_("Short Rest")) + " +" +
                       String::num_int64(
                           std::min(pool.short_rest_recovery, pool.capacity - pool.remaining)) +
                       "; " + rest_text(N_("Long Rest")) + " " + String::num_int64(pool.capacity) +
                       "\n";
        }
        healable = earned && r.can_rest && r.hit_dice > 0 &&
                   m.vitals.hit_points < m.character.sheet().hit_points;
        recovery_visible = earned && std::any_of(r.resources.begin(), r.resources.end(),
            [](const auto & pool)
        {
            return pool.id == "arcane_recovery";
        });
        if (recovery_visible)
            for (const auto &choice : r.choices)
            {
                const int index = recovery->get_item_count();
                recovery->add_item(rest_text(choice.label.source));
                const auto id = String::utf8(choice.id.c_str());
                recovery->set_item_metadata(index, id);
                if (id == previous)
                    recovery->select(index);
            }
    }
    if (retained)
    {
        const auto &a = *state.rest_activity;
        details = rest_text(N_("Rest progress (minutes):")) + " " +
                  String::num_int64(a.elapsed_milliseconds / 60000) + "\n" +
                  rest_text(N_("Additional required time (minutes):")) + " " +
                  String::num_int64(a.extension_milliseconds / 60000) + "\n" +
                  rest_text(N_("Remaining rest (minutes):")) + " " +
                  String::num_int64((campaign_->remaining_rest_milliseconds() + 59999) / 60000) +
                  "\n\n" + details;
    }
    w->get_node<RichTextLabel>("Info")->set_text(details);
    w->get_node<Label>("Result")->set_text(rest_result_);
    w->get_node<RichTextLabel>("Info")->set_size(Vector2(672, recovery_visible ? 132 : 208));
    w->get_node<Label>("RecoveryLabel")->set_visible(recovery_visible);
    recovery->set_visible(recovery_visible);
    recovery->set_disabled(recovery->get_item_count() == 0);
    w->get_node<Button>("Recover")->set_visible(recovery_visible);
    w->get_node<Button>("Recover")->set_disabled(recovery->get_item_count() == 0);
    w->get_node<Button>("Start")->set_visible(!spending && !retained);
    w->get_node<Button>("Start")->set_disabled(!eligible);
    w->get_node<Button>("Heal")->set_visible(spending);
    w->get_node<Button>("Heal")->set_disabled(!healable);
    w->get_node<Button>("Resume")->set_visible(retained && !spending);
    w->get_node<Button>("Resume")->set_disabled(!retained || !state.rest_activity->interrupted);
    w->get_node<Button>("Save")->set_visible(embedded_party_ && session_->can_leave() &&
            (spending || retained));
    w->get_node<Button>("Finish")->set_text(rest_text(spending   ? N_("Finish")
            : retained ? N_("End Rest")
            : N_("Cancel")));
    if ((spending || retained) && !w->is_visible() && !rest_save_open_ && is_visible_in_tree())
    {
        w->popup_centered();
        list->grab_focus();
    }
}

void RolfTourView::rest_start()
{
    try
    {
        auto *w = get_node<Window>("RestDialog");
        const auto kind = w->get_node<OptionButton>("Kind")->get_selected_id() == 0
                          ? opengold::RestKind::short_rest
                          : opengold::RestKind::long_rest;
        w->hide();
        rest_result_ = String();
        if (session_)
            session_->camp(kind);
        refresh();
    }
    catch (const std::exception &e)
    {
        rest_result_ = rest_text(e.what());
        refresh_rest();
        get_node<Window>("RestDialog")->popup_centered();
    }
}

void RolfTourView::rest_heal()
{
    try
    {
        if (!campaign_ || !campaign_->state().short_rest)
            return;
        const auto rolls =
            campaign_->heal_with_hit_dice(campaign_->state().short_rest->ticket, rest_member_);
        session_->commit_rest_recovery();
        // Every automatic roll is listed so the player sees how the HP came back.
        rest_result_ = String();
        for (const auto &roll : rolls)
            rest_result_ += rest_text(N_("Roll:")) + " d" + String::num_int64(roll.die) + " " +
                            String::num_int64(roll.roll) + " + (" +
                            String::num_int64(roll.modifier) + ")   " +
                            rest_text(N_("HP restored:")) + " " +
                            String::num_int64(roll.healing) + ";   ";
        rest_result_ += rest_text(N_("Hit Dice remaining:")) + " " +
                        String::num_int64(rolls.back().remaining);
        refresh_rest();
    }
    catch (const std::exception &e)
    {
        rest_result_ = rest_text(e.what());
        refresh_rest();
    }
}

void RolfTourView::rest_recover()
{
    try
    {
        if (!campaign_ || !session_ || campaign_->in_combat() || !campaign_->state().short_rest)
            return;
        auto *choice = get_node<Window>("RestDialog")->get_node<OptionButton>("RecoveryChoice");
        if (choice->is_disabled() || choice->get_selected() < 0)
            return;
        const String id = choice->get_selected_metadata();
        const auto result = campaign_->recover_rest_choice(campaign_->state().short_rest->ticket,
            rest_member_, id.utf8().get_data());
        session_->commit_rest_recovery();
        rest_result_ = rest_text(result.source);
        for (const auto &argument : result.arguments)
            rest_result_ =
                rest_result_.replace(String::utf8(("{" + argument.name + "}").c_str()),
                                     argument.translate ? rest_text(argument.value)
                                     : String::utf8(argument.value.c_str()));
        refresh_rest();
    }
    catch (const std::exception &e)
    {
        rest_result_ = rest_text(e.what());
        refresh_rest();
    }
}

void RolfTourView::rest_finish()
{
    try
    {
        if (campaign_)
        {
            if (campaign_->state().short_rest)
                campaign_->finish_short_rest(campaign_->state().short_rest->ticket);
            else if (campaign_->state().rest_activity)
                campaign_->abandon_rest(campaign_->state().rest_activity->ticket);
        }
        if (session_)
            session_->commit_rest_recovery();
        get_node<Window>("RestDialog")->hide();
        rest_save_open_ = false;
        rest_result_ = String();
        refresh();
    }
    catch (const std::exception &e)
    {
        rest_result_ = rest_text(e.what());
        refresh_rest();
    }
}

void RolfTourView::rest_resume()
{
    try
    {
        get_node<Window>("RestDialog")->hide();
        rest_result_ =
            rest_text(N_("Rest could not resume here. Resolve the interruption or end the rest."));
        if (session_)
            session_->resume_camp();
        refresh();
    }
    catch (const std::exception &e)
    {
        rest_result_ = rest_text(e.what());
        refresh_rest();
    }
}

void RolfTourView::rest_save()
{
    if (!embedded_party_ || !session_ || !session_->can_leave() || !campaign_ ||
            campaign_->in_combat())
        return;
    rest_save_open_ = true;
    get_node<Window>("RestDialog")->hide();
    get_node<Window>("RestTraining")->hide();
    request_save(true);
}

void RolfTourView::rest_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventKey> key = event;
    if (key.is_valid() && key->is_pressed() && key->get_keycode() == Key::KEY_ESCAPE)
    {
        rest_finish();
        get_node<Window>("RestDialog")->set_input_as_handled();
    }
}

// Runtime acceptance drives the same signals and controls as a player.
#include "opengold/campaign_save.h"

void RolfTourView::check_rest_controls()
{
    try
    {
        auto check = [](bool ok, const char *message)
        {
            if (!ok)
                throw std::runtime_error(message);
        };
        auto *w = get_node<Window>("RestDialog");
        if (rest_check_stage_ == 0)
        {
            check(campaign_ && campaign_->selected(),
                  "Rest check requires the real campaign party");
            const auto id = campaign_->selected();
            campaign_->award_experience(300, "rest-ui-check");
            if (campaign_->can_advance(id))
                campaign_->advance(id, campaign_->default_advancement(id));
            auto draft = campaign_->member(id).character.creation_data();
            draft.name = "Fallen companion";
            const auto companion = campaign_->add_pc(
                                       opengold::Character(*opengold::srd5::character_rules(), draft, {}));
            auto state = campaign_->checkpoint();
            state.roster.front().vitals.hit_points = 1;
            for (auto &m : state.roster)
                if (m.id == companion)
                    m.vitals = {0, true,
                                presentation::srd_vitals(m.character.sheet(),
                    {.failures = 3, .hit_dice = 1}),
                    "Dead"};
            campaign_->restore(state);
            std::vector<std::uint8_t> bytes{0, 0};
            for (int n = 0; n < 5; ++n)
                bytes.insert(bytes.end(), {1, 1, 0x15, 0x99});
            bytes.insert(bytes.end(), {0, 0});
            auto p = std::make_shared<const opengold::por::EclProgram>(
                         opengold::por::EclProgram::decode(bytes, "rest controls"));
            auto resources = std::make_shared<opengold::por::PhlanResources>();
            resources->programs[0] = p;
            const opengold::Image placeholder{1, 1, 0, 0, {0, 0, 0, 255}};
            session_.emplace(opengold::por::GeoMap{}, p,
                             std::array<opengold::Image, 3> {placeholder, placeholder, placeholder},
                             0x9914, opengold::por::WallArtSet{}, resources);
            session_->campaign_party(campaign_);
            session_->advance(1);
            camp();
            check(w->is_visible(), "Camp opens the Rest picker");
            w->get_node<OptionButton>("Kind")->select(0);
            w->get_node<OptionButton>("Kind")->emit_signal("item_selected", 0);
            check(!w->get_node<Button>("Start")->is_disabled(),
                  "Eligible Short Rest enables Start");
        }
        else if (rest_check_stage_ == 12)
        {
            w->get_node<Button>("Start")->emit_signal("pressed");
            check(campaign_->state().short_rest && w->is_visible(),
                  "Completing the hour opens the spending controls");
            check(campaign_->state().time_minutes == 60, "Picker completes exactly one hour");
            auto *members = w->get_node<ItemList>("Members");
            members->select(1);
            members->emit_signal("item_selected", 1);
            check(w->get_node<Button>("Heal")->is_disabled(),
                  "Ineligible member cannot spend Hit Dice");
            members->select(0);
            members->emit_signal("item_selected", 0);
            check(!w->get_node<Button>("Heal")->is_disabled(),
                  "Wounded eligible character can heal with Hit Dice");
            w->get_node<Button>("Heal")->grab_focus();
            for (bool down :
                    {
                        true, false
                    })
            {
                Ref<InputEventKey> key;
                key.instantiate();
                key->set_keycode(Key::KEY_SPACE);
                key->set_pressed(down);
                w->push_input(key, true);
            }
            const auto &healed = campaign_->member(rest_member_);
            check(w->get_node<Label>("Result")->get_text().contains(rest_text(N_("HP restored:"))) &&
                  healed.vitals.hit_points > 1,
                  "One action shows each die's healing result");
            check(healed.vitals.hit_points == healed.character.sheet().hit_points ||
                  campaign_->recovery_info(rest_member_).hit_dice == 0,
                  "Healing stops only at full HP or with no dice left");
            check(w->get_node<Button>("Heal")->is_disabled(),
                  "Nothing is left to heal after one action");
            auto rules = opengold::srd5::character_rules();
            const auto saved = opengold::encode_campaign(*campaign_, nullptr, "rest-ui");
            // Decode through the same campaign codec used by the save dialog.
            auto module =
                opengold::srd5::load(std::filesystem::path(rest_rules_path().utf8().get_data()));
            campaign_->restore(
                opengold::decode_campaign(saved, *rules, *module, "rest-ui", nullptr).party);
            refresh_rest();
            check(campaign_->state().short_rest.has_value(),
                  "Reload retains the completed Short Rest");
        }
        else if (rest_check_stage_ == 24)
        {
            Ref<InputEventKey> escape;
            escape.instantiate();
            escape->set_keycode(Key::KEY_ESCAPE);
            escape->set_pressed(true);
            w->emit_signal("window_input", escape);
            check(!campaign_->state().short_rest && !w->is_visible(),
                  "Escape finishes committed spending and closes the window");
            const auto begin = campaign_->begin_rest(opengold::RestKind::long_rest);
            (void)campaign_->advance_rest(*begin, 70 * 60000, opengold::RestWork::sleep);
            campaign_->interrupt_rest(campaign_->state().rest_activity->ticket,
                                      opengold::RestInterruption::initiative);
            campaign_->finish_short_rest(campaign_->state().short_rest->ticket);
            refresh();
            check(w->is_visible() && w->get_node<Button>("Resume")->is_visible(),
                  "An interrupted Long Rest displays Resume and End Rest");
        }
        else if (rest_check_stage_ == 36)
        {
            w->get_node<Button>("Resume")->emit_signal("pressed");
            check(!campaign_->state().rest_activity && campaign_->state().time_minutes == 600,
                  "Resume completes original permission check and remaining rest once");
            camp();
            w->get_node<OptionButton>("Kind")->select(1);
            w->get_node<OptionButton>("Kind")->emit_signal("item_selected", 1);
            check(w->get_node<Button>("Start")->is_disabled(), "Long Rest cooldown disables Start");
            w->get_node<Button>("Finish")->emit_signal("pressed");
        }
        else if (rest_check_stage_ == 48)
        {
            // A delayed original event reaches the real combat handoff during rest.
            const auto program = [](std::vector<std::uint8_t> body)
            {
                std::vector<std::uint8_t> bytes{0, 0};
                for (int n = 0; n < 5; ++n)
                    bytes.insert(bytes.end(), {1, 1, 0x15, 0x99});
                bytes.push_back(0);
                bytes.insert(bytes.end(), body.begin(), body.end());
                return std::make_shared<const opengold::por::EclProgram>(
                           opengold::por::EclProgram::decode(bytes, "rest UI event"));
            };
            auto gate = program({32, 0, 20, 0});
            auto resources = std::make_shared<opengold::por::PhlanResources>();
            resources->map = opengold::por::GeoMap{};
            resources->programs[0] = gate;
            resources->programs[20] =
                program({58, 33, 0, 20, 0, 2, 0, 255, 11, 0, 4, 0, 1, 0, 4, 36, 0});
            auto district = std::make_shared<opengold::por::PhlanResources>();
            district->map = opengold::por::GeoMap{};
            district->encounter_creatures[4].stored.name = "Test orc";
            district->combat_archive = {9, 0, 4, 0, 0, 0, 0, 25, 0, 26, 0, 24};
            district->combat_archive.resize(37, 0);
            district->combat_archive[12] = 1;
            district->combat_archive[14] = 2;
            district->combat_archive[20] = 1;
            resources->districts[20] = district;
            const opengold::Image pixel{1, 1, 0, 0, {0, 0, 0, 255}};
            session_.emplace(opengold::por::GeoMap{}, gate,
                             std::array<opengold::Image, 3> {pixel, pixel, pixel}, 0x9914,
                             opengold::por::WallArtSet{}, resources);
            session_->campaign_party(campaign_);
            session_->advance(1);
            campaign_->advance_time(960);
            check(session_->explore(opengold::por::ExplorationCommand::look),
                  "Start delayed encounter");
            const auto begin = campaign_->begin_rest(opengold::RestKind::long_rest);
            check(begin.has_value(), "Rest eligible after cooldown");
            (void)campaign_->advance_rest(*begin, 70 * 60000, opengold::RestWork::sleep);
            for (unsigned n = 0;
                    n < 100 && session_->snapshot().phase == opengold::por::TourPhase::running; ++n)
                session_->advance(.5);
            refresh();
            check(session_->pending_encounter() && campaign_->state().short_rest && w->is_visible(),
                  ("Pending encounter presents earned Hit Dice choices: " +
                   session_->snapshot().dialogue + session_->snapshot().diagnostic)
                  .c_str());
            check(!w->get_node<Button>("Save")->is_visible(),
                  "Encounter handoff never offers a combat save");
        }
        else if (rest_check_stage_ == 60)
        {
            auto wounded = campaign_->checkpoint();
            for (auto &m : wounded.roster)
                if (m.id == rest_member_)
                    m.vitals.hit_points = 1;
            campaign_->restore(wounded);
            refresh_rest();
            w->get_node<Button>("Heal")->emit_signal("pressed");
            const auto spent = campaign_->member(rest_member_).vitals;
            check(spent.hit_points > 1, "Heal with Hit Dice restores HP before the encounter");
            const auto rng = campaign_->state().random_state;
            w->get_node<Button>("Finish")->emit_signal("pressed");
            check(session_->pending_encounter() && !campaign_->state().short_rest &&
                  !w->is_visible(),
                  "Finishing recovery releases encounter handoff");
            check(session_->reject_combat("Fixture initialization rejected"),
                  "Reject pending encounter after player choice");
            check(campaign_->member(rest_member_).vitals == spent &&
                  campaign_->state().random_state == rng,
                  "Failed encounter does not undo player recovery");
        }
        else if (rest_check_stage_ == 64)
        {
            if (campaign_->state().rest_activity)
                campaign_->abandon_rest(campaign_->state().rest_activity->ticket);
            opengold::rules::CharacterDraft draft;
            draft.race = "human";
            draft.gender = "female";
            draft.character_class = "wizard";
            draft.background = "sage";
            draft.alignment = "neutral_good";
            draft.name = "Arcane Recovery Wizard";
            draft.rolled = true;
            for (auto &roll : draft.rolls)
                roll = {{6, 5, 4, 1}, 3};
            rest_member_ = campaign_->add_pc(
                               opengold::Character(*opengold::srd5::character_rules(), draft, {}));
            campaign_->award_experience(2700, "arcane-ui");
            for (unsigned n = 0; n < 2; ++n)
                campaign_->advance(rest_member_, campaign_->default_advancement(rest_member_));
            auto state = campaign_->checkpoint();
            for (auto &member : state.roster)
                if (member.id == rest_member_)
                    member.vitals.resources = presentation::srd_vitals(
                                                  member.character.sheet(),
                    {.slots = 2, .slots2 = 1, .hit_dice = 3});
            campaign_->restore(state);
            std::vector<std::uint8_t> bytes{0, 0};
            for (int n = 0; n < 5; ++n)
                bytes.insert(bytes.end(), {1, 1, 0x15, 0x99});
            bytes.insert(bytes.end(), {0, 0});
            auto program = std::make_shared<const opengold::por::EclProgram>(
                               opengold::por::EclProgram::decode(bytes, "arcane UI camp"));
            auto resources = std::make_shared<opengold::por::PhlanResources>();
            resources->programs[0] = program;
            const opengold::Image pixel{1, 1, 0, 0, {0, 0, 0, 255}};
            session_.emplace(opengold::por::GeoMap{}, program,
                             std::array<opengold::Image, 3> {pixel, pixel, pixel}, 0x9914,
                             opengold::por::WallArtSet{}, resources);
            session_->campaign_party(campaign_);
            session_->advance(1);
            camp();
            auto *kind = w->get_node<OptionButton>("Kind");
            kind->select(0);
            kind->emit_signal("item_selected", 0);
            check(!w->get_node<Button>("Recover")->is_visible(),
                  "Arcane Recovery is unavailable before completing the rest");
            w->get_node<Button>("Start")->emit_signal("pressed");
            check(w->is_visible() &&
                  w->get_node<OptionButton>("RecoveryChoice")->get_item_count() == 3 &&
                  !w->get_node<Button>("Recover")->is_disabled(),
                  "Completed Short Rest offers exactly the three legal Wizard allocations");
            const auto saved = opengold::encode_campaign(*campaign_, nullptr, "arcane-ui");
            auto rules =
                opengold::srd5::load(std::filesystem::path(rest_rules_path().utf8().get_data()));
            campaign_->restore(opengold::decode_campaign(saved, *opengold::srd5::character_rules(),
                    *rules, "arcane-ui", nullptr)
                               .party);
            refresh_rest();
            check(w->get_node<OptionButton>("RecoveryChoice")->get_item_count() == 3,
                  "Reload preserves unused recovery eligibility");
        }
        else if (rest_check_stage_ == 68)
        {
            Ref<InputEventKey> escape;
            escape.instantiate();
            escape->set_keycode(Key::KEY_ESCAPE);
            escape->set_pressed(true);
            w->emit_signal("window_input", escape);
            check(!campaign_->state().short_rest && !w->is_visible(),
                  "Escape closes an unused recovery opportunity");
            for (const auto &pool : campaign_->recovery_info(rest_member_).resources)
                if (pool.id == "arcane_recovery")
                    check(pool.remaining == 1, "Declining recovery preserves its use");
            camp();
            w->get_node<Button>("Start")->emit_signal("pressed");
            check(w->get_node<OptionButton>("RecoveryChoice")->get_item_count() == 3,
                  "A later Short Rest can use the preserved feature");
        }
        else if (rest_check_stage_ == 72)
        {
            auto *choices = w->get_node<OptionButton>("RecoveryChoice");
            choices->grab_focus();
            for (bool down :
                    {
                        true, false
                    })
            {
                Ref<InputEventKey> key;
                key.instantiate();
                key->set_keycode(Key::KEY_SPACE);
                key->set_pressed(down);
                w->push_input(key, true);
            }
        }
        else if (rest_check_stage_ == 74)
        {
            auto *popup = w->get_node<OptionButton>("RecoveryChoice")->get_popup();
            check(popup->is_visible(), "Keyboard opens the recovery dropdown");
            popup->set_focused_item(0);
            for (auto code :
                    {
                        Key::KEY_DOWN, Key::KEY_DOWN, Key::KEY_ENTER
                    })
                for (bool down :
                        {
                            true, false
                        })
                {
                    Ref<InputEventKey> key;
                    key.instantiate();
                    key->set_keycode(code);
                    key->set_pressed(down);
                    get_viewport()->push_input(key, true);
                }
        }
        else if (rest_check_stage_ == 76)
        {
            auto *choices = w->get_node<OptionButton>("RecoveryChoice");
            check(String(choices->get_selected_metadata()) == "arcane_recovery:0:1",
                  ("Keyboard selects a level-two slot; selected index " +
                   std::to_string(choices->get_selected()))
                  .c_str());
            w->get_node<Button>("Recover")->grab_focus();
            for (bool down :
                    {
                        true, false
                    })
            {
                Ref<InputEventKey> key;
                key.instantiate();
                key->set_keycode(Key::KEY_SPACE);
                key->set_pressed(down);
                w->push_input(key, true);
            }
            const auto info = campaign_->recovery_info(rest_member_);
            for (const auto &pool : info.resources)
            {
                if (pool.id == "arcane_recovery")
                    check(pool.remaining == 0, "Keyboard recovery consumes the feature use");
                if (pool.id == "spell_slot:2")
                    check(pool.remaining == 2,
                          "Keyboard recovery restores the selected spell slot");
                if (pool.id == "spell_slot:1")
                    check(pool.remaining == 2, "Unselected slot pool is unchanged");
            }
            check(choices->is_disabled() && w->get_node<Button>("Recover")->is_disabled() &&
                  !w->get_node<Label>("Result")->get_text().is_empty(),
                  "Used recovery disables controls and displays the result");
            const auto saved = opengold::encode_campaign(*campaign_, nullptr, "arcane-ui");
            auto rules =
                opengold::srd5::load(std::filesystem::path(rest_rules_path().utf8().get_data()));
            campaign_->restore(opengold::decode_campaign(saved, *opengold::srd5::character_rules(),
                    *rules, "arcane-ui", nullptr)
                               .party);
            refresh_rest();
            w->get_node<Button>("Recover")->emit_signal("pressed");
            check(opengold::encode_campaign(*campaign_, nullptr, "arcane-ui") == saved,
                  "Reload and duplicate activation cannot refresh or spend recovery again");
        }
        else if (rest_check_stage_ == 84)
        {
            w->get_node<Button>("Finish")->emit_signal("pressed");
            camp();
            w->get_node<Button>("Start")->emit_signal("pressed");
            check(w->get_node<Button>("Recover")->is_disabled(),
                  "A second Short Rest does not refresh Arcane Recovery");
            auto *members = w->get_node<ItemList>("Members");
            members->select(0);
            members->emit_signal("item_selected", 0);
            check(!w->get_node<Button>("Recover")->is_visible(),
                  "Non-Wizard selection hides the recovery row");
            w->get_node<Button>("Finish")->emit_signal("pressed");
        }
        else if (rest_check_stage_ == 88)
        {
            auto draft = campaign_->member(rest_member_).character.creation_data();
            draft.name = "Second Wizard";
            // The selected non-Wizard from the previous check is not the recovery owner.
            for (const auto &member : campaign_->state().roster)
                if (campaign_->rule_module()
                        .spell_access(member.character.sheet())
                        .spellbook_choices)
                {
                    draft = member.character.creation_data();
                    break;
                }
            draft.name = "Second Wizard";
            campaign_->add_pc(opengold::Character(*opengold::srd5::character_rules(), draft, {}));
            camp();
            w->get_node<OptionButton>("Kind")->select(1);
            w->get_node<OptionButton>("Kind")->emit_signal("item_selected", 1);
            w->get_node<Button>("Start")->emit_signal("pressed");
            auto *spell = get_node<Window>("RestSpells");
            check(spell->is_visible() && campaign_->state().spell_rest &&
                  campaign_->state().spell_rest->members.size() == 2,
                  "Completed Long Rest presents each eligible Wizard");
            const auto bytes = opengold::encode_campaign(*campaign_, nullptr, "spell-rest-ui");
            auto rules =
                opengold::srd5::load(std::filesystem::path(rest_rules_path().utf8().get_data()));
            campaign_->restore(opengold::decode_campaign(bytes, *opengold::srd5::character_rules(),
                    *rules, "spell-rest-ui", nullptr)
                               .party);
            refresh_rest();
            auto *replace = spell->get_node<OptionButton>("Replace");
            replace->select(1);
            replace->emit_signal("item_selected", 1);
            auto *with = spell->get_node<OptionButton>("With");
            for (int i = 0; i < with->get_item_count(); ++i)
                if (String(with->get_item_metadata(i)) == "ray_of_frost")
                {
                    with->select(i);
                    with->emit_signal("item_selected", i);
                    break;
                }
            check(!spell->get_node<Button>("Apply")->is_disabled(),
                  "Preparation and one replacement are valid after reload");
        }
        else if (rest_check_stage_ == 100)
        {
            auto *spell = get_node<Window>("RestSpells");
            const auto id = rest_spell_member_;
            const auto expected = campaign_->preview_spell_choices(id, rest_spell_choice_);
            spell->get_node<Button>("Apply")->grab_focus();
            for (bool down :
                    {
                        true, false
                    })
            {
                Ref<InputEventKey> key;
                key.instantiate();
                key->set_keycode(Key::KEY_SPACE);
                key->set_pressed(down);
                spell->push_input(key, true);
            }
            check(campaign_->member(id).character.sheet().grants ==
                  expected.character.sheet().grants &&
                  campaign_->member(id).vitals == expected.vitals,
                  "Keyboard commits only approved spell changes");
            check(campaign_->state().spell_rest &&
                  campaign_->state().spell_rest->members.size() == 1 &&
                  rest_spell_member_ != id && spell->is_visible(),
                  "Next Wizard receives a separate once-only choice");
        }
        else if (rest_check_stage_ == 104)
        {
            auto *spell = get_node<Window>("RestSpells");
            const auto id = rest_spell_member_;
            const auto before = campaign_->member(id).character.sheet().grants;
            Ref<InputEventKey> escape;
            escape.instantiate();
            escape->set_keycode(Key::KEY_ESCAPE);
            escape->set_pressed(true);
            spell->emit_signal("window_input", escape);
            check(!spell->is_visible() && !campaign_->state().spell_rest &&
                  campaign_->member(id).character.sheet().grants == before,
                  "Escape keeps current spells and releases exploration after the last Wizard");
            UtilityFunctions::print(
                "Godot rest controls passed: existing recovery, Arcane Recovery, Wizard preparation/replacement, sequential Wizards, keyboard, limits and save continuation.");
            get_tree()->quit();
        }
        if (rest_check_stage_ == 10 || rest_check_stage_ == 22 || rest_check_stage_ == 34 ||
                rest_check_stage_ == 70 || rest_check_stage_ == 80 || rest_check_stage_ == 94)
        {
            for (const auto &arg : OS::get_singleton()->get_cmdline_user_args())
                if (String(arg).begins_with("--rest-captures="))
                {
                    const auto directory = String(arg).trim_prefix("--rest-captures=");
                    std::filesystem::create_directories(
                        std::filesystem::path(directory.utf8().get_data()));
                    const auto image = get_viewport()->get_texture()->get_image();
                    check(image.is_valid() &&
                          image->save_png(directory.path_join(
                                              String::num_int64(rest_check_stage_) + ".png")) == OK,
                          "Rest screenshot saved");
                }
        }
        ++rest_check_stage_;
    }
    catch (const std::exception &e)
    {
        UtilityFunctions::printerr("Rest control check failed: ", String::utf8(e.what()));
        get_tree()->quit(1);
    }
}

void RolfTourView::refresh_rest_spells()
{
    auto *w = get_node<Window>("RestSpells");
    if (!campaign_ || !campaign_->state().spell_rest || campaign_->in_combat())
    {
        w->hide();
        rest_spell_member_ = 0;
        return;
    }
    const auto id = campaign_->state().spell_rest->members.front();
    const auto &sheet = campaign_->member(id).character.sheet();
    const auto options = campaign_->spell_choice_options(id);
    if (rest_spell_member_ != id)
    {
        rest_spell_member_ = id;
        rest_spell_choice_ = {};
        rest_spell_choice_.prepared = sheet.prepared_spells;
    }
    w->get_node<Label>("Title")->set_text(presentation::training_string(sheet.name) + " / " +
                                          rest_text(N_("Long Rest")));
    presentation::spell_known(*w, campaign_->rule_module().spell_access(sheet), rest_text);
    presentation::refresh_spell_groups(
        *w->get_node<VBoxContainer>("Choices/Rows"), options, rest_spell_choice_,
        callable_mp(this, &RolfTourView::rest_spell_toggled), rest_text);
    for (bool replacing :
            {
                true, false
            })
    {
        auto *dropdown = w->get_node<OptionButton>(replacing ? "Replace" : "With");
        dropdown->clear();
        dropdown->add_item(rest_text(N_("Keep current")));
        dropdown->set_item_metadata(0, String());
        const auto &list = replacing ? options.replaceable : options.replacements;
        const auto &selected =
            replacing ? rest_spell_choice_.replace_cantrip : rest_spell_choice_.replacement;
        for (const auto &option : list)
        {
            const int index = dropdown->get_item_count();
            dropdown->add_item(rest_text(option.label));
            dropdown->set_item_metadata(index, presentation::training_string(option.id));
            if (selected == option.id)
                dropdown->select(index);
        }
        dropdown->set_disabled(!options.may_replace || list.empty());
    }
    try
    {
        (void)campaign_->preview_spell_choices(id, rest_spell_choice_);
        w->get_node<Button>("Apply")->set_disabled(false);
        w->get_node<Label>("Error")->set_text({});
    }
    catch (const std::exception &e)
    {
        w->get_node<Button>("Apply")->set_disabled(true);
        w->get_node<Label>("Error")->set_text(rest_text(e.what()));
    }
    if (!w->is_visible() && !rest_save_open_ && is_visible_in_tree())
    {
        get_node<Window>("RestDialog")->hide();
        w->popup_centered();
        w->get_node<Button>("Cancel")->grab_focus();
    }
}

void RolfTourView::rest_spell_toggled(bool selected, String group, String option)
{
    presentation::toggle_spell(rest_spell_choice_, selected, group.utf8().get_data(),
                               option.utf8().get_data());
    refresh_rest_spells();
}

void RolfTourView::rest_spell_replaced(std::int64_t)
{
    auto *w = get_node<Window>("RestSpells");
    const String old = w->get_node<OptionButton>("Replace")->get_selected_metadata(),
                 next = w->get_node<OptionButton>("With")->get_selected_metadata();
    rest_spell_choice_.replace_cantrip = old.utf8().get_data();
    rest_spell_choice_.replacement = next.utf8().get_data();
    refresh_rest_spells();
}

void RolfTourView::rest_spell_apply()
{
    try
    {
        campaign_->choose_spells(rest_spell_member_, rest_spell_choice_);
        rest_spell_member_ = 0;
        if (session_)
            session_->commit_rest_recovery();
        refresh();
    }
    catch (const std::exception &e)
    {
        get_node<Label>("RestSpells/Error")->set_text(rest_text(e.what()));
    }
}

void RolfTourView::rest_spell_keep()
{
    try
    {
        if (rest_spell_member_)
            campaign_->keep_rest_spells(rest_spell_member_);
        rest_spell_member_ = 0;
        if (session_)
            session_->commit_rest_recovery();
        refresh();
    }
    catch (const std::exception &e)
    {
        get_node<Label>("RestSpells/Error")->set_text(rest_text(e.what()));
    }
}

void RolfTourView::rest_spell_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventKey> key = event;
    if (key.is_valid() && key->is_pressed() && !key->is_echo() && key->get_keycode() == KEY_ESCAPE)
    {
        get_node<Window>("RestSpells")->set_input_as_handled();
        rest_spell_keep();
    }
}

void RolfTourView::refresh_rest_training()
{
    auto *w = get_node<Window>("RestTraining");
    if (!campaign_ || campaign_->in_combat() || campaign_->state().spell_rest ||
            !campaign_->state().training_rest)
    {
        w->hide();
        rest_training_member_ = 0;
        return;
    }
    const auto &rest = *campaign_->state().training_rest;
    const auto id = rest.members.front();
    const auto &sheet = campaign_->member(id).character.sheet();
    const auto options = campaign_->rule_module().rest_training_options(sheet);
    if (!options)
        throw std::runtime_error("Missing rest training options");
    if (rest_training_member_ != id || rest_training_ticket_ != rest.ticket)
    {
        rest_training_member_ = id;
        rest_training_ticket_ = rest.ticket;
        rest_training_choice_ = options->selected;
    }
    w->set_title(rest_text(options->group.label));
    w->get_node<Label>("Title")->set_text(presentation::training_string(sheet.name) + " / " +
                                          rest_text(options->group.label));
    presentation::refresh_training_replacement(
        *w, *options, rest_training_choice_,
        callable_mp(this, &RolfTourView::rest_training_toggled), rest_text);
    try
    {
        (void)campaign_->preview_rest_training(rest.ticket, id, rest_training_choice_);
        w->get_node<Button>("Apply")->set_disabled(false);
        w->get_node<Label>("Error")->set_text({});
    }
    catch (const std::exception &e)
    {
        w->get_node<Button>("Apply")->set_disabled(true);
        w->get_node<Label>("Error")->set_text(rest_text(e.what()));
    }
    w->get_node<Button>("Save")->set_visible(embedded_party_);
    w->get_node<Button>("Save")->set_disabled(!session_ || !session_->can_leave());
    if (!w->is_visible() && !rest_save_open_ && is_visible_in_tree())
    {
        get_node<Window>("RestDialog")->hide();
        w->popup_centered();
        w->get_node<Button>("Cancel")->grab_focus();
    }
}

void RolfTourView::rest_training_toggled(bool selected, String option)
{
    const std::string id = option.utf8().get_data();
    if (selected)
    {
        if (std::find(rest_training_choice_.begin(), rest_training_choice_.end(), id) ==
                rest_training_choice_.end())
            rest_training_choice_.push_back(id);
    }
    else
        std::erase(rest_training_choice_, id);
    refresh_rest_training();
}

void RolfTourView::rest_training_apply()
{
    try
    {
        campaign_->replace_rest_training(rest_training_ticket_, rest_training_member_,
                                         rest_training_choice_);
        rest_training_member_ = 0;
        if (session_)
            session_->commit_rest_recovery();
        refresh();
    }
    catch (const std::exception &e)
    {
        get_node<Label>("RestTraining/Error")->set_text(rest_text(e.what()));
    }
}

void RolfTourView::rest_training_keep()
{
    try
    {
        if (rest_training_member_)
            campaign_->keep_rest_training(rest_training_ticket_, rest_training_member_);
        rest_training_member_ = 0;
        if (session_)
            session_->commit_rest_recovery();
        refresh();
    }
    catch (const std::exception &e)
    {
        get_node<Label>("RestTraining/Error")->set_text(rest_text(e.what()));
    }
}

void RolfTourView::rest_training_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventKey> key = event;
    if (key.is_valid() && key->is_pressed() && !key->is_echo() && key->get_keycode() == KEY_ESCAPE)
    {
        get_node<Window>("RestTraining")->set_input_as_handled();
        rest_training_keep();
    }
}

#include "mastery_rest_view_checks.h"
