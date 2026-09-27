// Native acceptance hooks follow the existing rest control checks. They drive
// the live campaign and the same button signals used by the player.
void RolfTourView::check_mastery_rest_controls()
{
    try
    {
        const auto check = [](bool ok, const char *message)
        {
            if (!ok)
                throw std::runtime_error(message);
        };
        auto *w = get_node<Window>("RestTraining");
        if (rest_check_stage_ == 0)
        {
            auto rules = opengold::srd5::character_rules();
            auto draft = campaign_->member(campaign_->selected()).character.creation_data();
            campaign_ = std::make_shared<opengold::CampaignParty>(
                            opengold::srd5::load(std::filesystem::path(rest_rules_path().utf8().get_data())));
            unsigned index = 0;
            for (const std::string klass :
                    {"fighter", "barbarian", "rogue", "paladin", "ranger", "wizard"
                    })
            {
                draft.character_class = klass;
                draft.name = klass;
                draft.training.clear();
                draft.cantrips.reset();
                draft.spells.reset();
                draft.target_classes.clear();
                for (const auto &original : rules->training_options(draft))
                {
                    const auto groups = rules->training_options(draft);
                    const auto &group = *std::find_if(groups.begin(), groups.end(),
                                                      [&](const auto & g)
                    {
                        return g.id == original.id;
                    });
                    auto &selected = draft.training[group.id];
                    for (const auto &o : group.options)
                        if (selected.size() < group.count &&
                                std::find(selected.begin(), selected.end(), o.id) == selected.end())
                            selected.push_back(o.id);
                }
                opengold::Character h(*rules, draft, {});
                const auto id = index == 3 || index == 4
                                ? campaign_->recruit("mastery-ui:" + klass, std::move(h))
                                : campaign_->add_pc(std::move(h));
                if (klass == "fighter")
                {
                    campaign_->award_experience(2700, "mastery-ui-levels");
                    while (campaign_->member(id).character.sheet().level < 4)
                        campaign_->advance(id, campaign_->default_advancement(id));
                }
                ++index;
            }
            std::vector<std::uint8_t> bytes{0, 0};
            for (int n = 0; n < 5; ++n)
                bytes.insert(bytes.end(), {1, 1, 0x15, 0x99});
            bytes.insert(bytes.end(), {0, 0});
            auto program = std::make_shared<const opengold::por::EclProgram>(
                               opengold::por::EclProgram::decode(bytes, "mastery rest controls"));
            auto resources = std::make_shared<opengold::por::PhlanResources>();
            resources->programs[0] = program;
            const opengold::Image placeholder{1, 1, 0, 0, {0, 0, 0, 255}};
            session_.emplace(opengold::por::GeoMap{}, program,
                             std::array<opengold::Image, 3> {placeholder, placeholder, placeholder},
                             0x9914, opengold::por::WallArtSet{}, resources);
            session_->campaign_party(campaign_);
            session_->advance(1);
            camp();
            auto *picker = get_node<Window>("RestDialog");
            picker->get_node<OptionButton>("Kind")->select(1);
            picker->get_node<OptionButton>("Kind")->emit_signal("item_selected", 1);
            picker->get_node<Button>("Start")->emit_signal("pressed");
            check(campaign_->state().spell_rest && campaign_->state().training_rest &&
                  campaign_->state().training_rest->members.size() == 5,
                  "Completed camp creates independent spell and mastery windows");
            check(get_node<Window>("RestSpells")->is_visible() && !w->is_visible(),
                  "Spell choices precede mastery choices");
            get_node<Button>("RestSpells/Cancel")->emit_signal("pressed");
            check(w->is_visible() && !campaign_->state().spell_rest,
                  "Mastery opens after spell choices");
        }
        if (rest_check_stage_ >= 4 && rest_check_stage_ < 44)
        {
            const unsigned phase = (rest_check_stage_ - 4) % 8;
            if (phase == 0)
            {
                const auto bytes =
                    opengold::encode_campaign(*campaign_, nullptr, "mastery-rest-ui");
                campaign_->restore(
                    opengold::decode_campaign(bytes, *opengold::srd5::character_rules(),
                                              campaign_->rule_module(), "mastery-rest-ui", nullptr)
                    .party);
                rest_training_member_ = 0;
                refresh_rest();
                check(w->is_visible() && !w->get_node<Button>("Apply")->is_disabled(),
                      "Reload restores a valid selected mastery set");
                auto *box = w->get_node<CheckBox>(
                                "Choices/Rows/" + presentation::training_string(rest_training_choice_.front()));
                box->grab_focus();
            }
            else if (phase == 1 || phase == 2)
            {
                Ref<InputEventKey> event;
                event.instantiate();
                event->set_keycode(KEY_SPACE);
                event->set_pressed(phase == 1);
                w->push_input(event, true);
            }
            else if (phase == 3)
            {
                const auto original = *campaign_->rule_module().rest_training_options(
                                          campaign_->member(rest_training_member_).character.sheet());
                auto *box = w->get_node<CheckBox>(
                                "Choices/Rows/" + presentation::training_string(original.selected.front()));
                check(box->has_focus(), "Keyboard deselection preserves checkbox focus");
                check(!box->is_pressed(), "Keyboard Space toggles the selected checkbox");
                check(w->get_node<Button>("Apply")->is_disabled(),
                      "Incomplete mastery selections disable Apply");
                const auto options = *campaign_->rule_module().rest_training_options(
                                         campaign_->member(rest_training_member_).character.sheet());
                for (const auto &o : options.group.options)
                    if (std::find(options.selected.begin(), options.selected.end(), o.id) ==
                            options.selected.end())
                    {
                        auto *other = w->get_node<CheckBox>("Choices/Rows/" +
                                                            presentation::training_string(o.id));
                        other->set_pressed(true);
                        break;
                    }
                check(!w->get_node<Button>("Apply")->is_disabled(),
                      "A legal replacement enables Apply");
            }
            else if (phase == 4)
            {
                for (const auto &arg : OS::get_singleton()->get_cmdline_user_args())
                    if (String(arg).begins_with("--mastery-rest-captures="))
                    {
                        const auto dir = String(arg).trim_prefix("--mastery-rest-captures=");
                        std::filesystem::create_directories(
                            std::filesystem::path(dir.utf8().get_data()));
                        const auto image = get_viewport()->get_texture()->get_image();
                        check(image.is_valid() &&
                              image->save_png(dir.path_join(
                                                  String::num_uint64(rest_check_stage_) + ".png")) == OK,
                              "Mastery rest capture saved");
                    }
            }
            else if (phase == 5)
            {
                w->get_node<Button>("Apply")->grab_focus();
            }
            else if (phase == 6)
            {
                const auto id = rest_training_member_;
                const auto before = campaign_->member(id);
                if (rest_check_stage_ == 42)
                {
                    Ref<InputEventKey> event;
                    event.instantiate();
                    event->set_keycode(KEY_ESCAPE);
                    event->set_pressed(true);
                    w->emit_signal("window_input", event);
                    check(campaign_->member(id).character.sheet().grants ==
                          before.character.sheet().grants,
                          "Escape discards edits and keeps current grants");
                }
                else
                {
                    const auto expected = campaign_->preview_rest_training(
                                              rest_training_ticket_, id, rest_training_choice_);
                    w->get_node<Button>("Apply")->grab_focus();
                    for (bool down :
                            {
                                true, false
                            })
                    {
                        Ref<InputEventKey> event;
                        event.instantiate();
                        event->set_keycode(KEY_SPACE);
                        event->set_pressed(down);
                        w->push_input(event, true);
                    }
                    check(campaign_->member(id).character.sheet().grants ==
                          expected.character.sheet().grants &&
                          campaign_->member(id).vitals == before.vitals,
                          "Keyboard Apply commits exactly the native preview and preserves vitals");
                }
            }
        }
        if (rest_check_stage_ == 46)
        {
            check(!campaign_->state().training_rest && !w->is_visible(),
                  "Last decision releases exploration");
            UtilityFunctions::print("Weapon Mastery rest controls passed");
            get_tree()->quit();
        }
        ++rest_check_stage_;
    }
    catch (const std::exception &e)
    {
        UtilityFunctions::printerr("Weapon Mastery rest controls failed: ", String::utf8(e.what()));
        get_tree()->quit(1);
    }
}
