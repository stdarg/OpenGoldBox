#include "opengold/hall_service.h"
#include "opengold/town_service_host.h"
#include "host_services.h"

#include <algorithm>
#include <array>

namespace opengold::por
{
using namespace host_services;

namespace
{
Message hall_welcome(opengold::TrainingHall hall)
{
    using opengold::TrainingHall;
    switch (hall)
    {
    case TrainingHall::fighters:
        return {"The Fighters' hall trains Fighters, Barbarians, Paladins and Rangers.", {}};
    case TrainingHall::clerics:
        return {"The Clerics' hall trains Clerics and Druids.", {}};
    case TrainingHall::magic_users:
        return {"The Magic-users' hall trains Wizards, Sorcerers and Warlocks.", {}};
    case TrainingHall::thieves:
        return {"The Thieves' hall trains Rogues, Bards and Monks.", {}};
    case TrainingHall::every_class:
        return {"The hall trains every class.", {}};
    }
    return {};
}

// Why a member cannot buy a session or lesson, for the dialogue.
Message refusal(opengold::TrainingDenial denial, const std::string &name, std::uint32_t fee)
{
    using opengold::TrainingDenial;
    switch (denial)
    {
    case TrainingDenial::ready_to_advance:
        return {
            "{name} has the experience for the next level: advance from the party roster first.",
            {{"name", name}}};
    case TrainingDenial::already_trained:
        return {"{name} has already trained in this at this level.", {{"name", name}}};
    case TrainingDenial::highest_level:
        return {"{name} is at the highest level.", {{"name", name}}};
    case TrainingDenial::gold:
        return {"The party's gold cannot pay {fee} gp.", {{"fee", amount(fee)}}};
    default:
        return {"{name} cannot train here.", {{"name", name}}};
    }
}
} // namespace

void HallService::open_hall(TownServiceHost &host, std::uint64_t request)
{
    // Pool of Radiance's halls name their classes; the later games' set a
    // class mask (TRAIN_MASK), every class (127) in each of Curse's halls.
    if (host.game() == GoldBoxGame::pool_of_radiance)
    {
        const auto hall =
            opengold::training_hall_named(host.script_machine().string(ecl_training_hall_name));
        if (!hall)
        {
            throw EclError("Training hall without a known class word");
        }
        hall_visit_ = HallVisit{request, *hall, 0, {}, opengold::original_class_mask(*hall)};
    }
    else
    {
        const unsigned mask =
            host.script_machine().variable(host.game_cell(ecl_training_class_mask)) & 0x7FU;
        hall_visit_ = HallVisit{request, opengold::TrainingHall::every_class, 0, {}, mask};
    }
    show_hall(host, {});
}

void HallService::show_hall(TownServiceHost &host, std::vector<Message> notice)
{
    using opengold::TrainingDenial;
    if (host.campaign()->rule_module().trains_at_halls())
    {
        show_original_hall(host, std::move(notice));
        return;
    }
    auto &visit = *hall_visit_;
    const auto name_of = [&](opengold::MemberId id)
    {
        return host.campaign()->member(id).character.sheet().name;
    };
    auto dialogue = std::move(notice);
    std::vector<Message> choices;
    visit.choices.clear();
    const auto add = [&](Message label, HallChoice choice)
    {
        choices.push_back(std::move(label));
        visit.choices.push_back(std::move(choice));
    };
    if (visit.member)
    {
        // One line fits above the list; the party's gold shows beside it.
        if (dialogue.empty())
        {
            dialogue.push_back({"Choose a lesson for {name}.", {{"name", name_of(visit.member)}}});
        }
        for (const auto &lesson : host.campaign()->skill_lessons(visit.member, visit.hall).options)
        {
            add({lesson.id.starts_with("expertise:") ? "{skill} Expertise ({fee} gp)"
                                                     : "{skill} ({fee} gp)",
                 {{"skill", lesson.label, true},
                  {"fee", amount(opengold::skill_training_fee(lesson.id))}}},
                {HallChoice::Kind::lesson, visit.member, lesson.id});
        }
        add({"Back", {}}, {HallChoice::Kind::back});
    }
    else
    {
        if (dialogue.empty())
        {
            dialogue.push_back(hall_welcome(visit.hall));
        }
        for (const auto id : host.campaign()->state().slots)
        {
            if (!id)
            {
                continue;
            }
            const auto offer = host.campaign()->experience_offer(id, visit.hall);
            if (offer.denial == TrainingDenial::other_hall ||
                offer.denial == TrainingDenial::not_active)
            {
                continue;
            }
            const auto name = name_of(id);
            auto session = [&]() -> Message
            {
                switch (offer.denial)
                {
                case TrainingDenial::ready_to_advance:
                    return {"{name}: session of experience (advance a level first)",
                            {{"name", name}}};
                case TrainingDenial::already_trained:
                    return {"{name}: session of experience (bought at this level)",
                            {{"name", name}}};
                case TrainingDenial::highest_level:
                    return {"{name}: session of experience (highest level)", {{"name", name}}};
                default:
                    return {"{name}: session of experience, {experience} XP ({fee} gp)",
                            {{"name", name},
                             {"experience", amount(offer.experience)},
                             {"fee", amount(offer.fee)}}};
                }
            }();
            add(std::move(session), {HallChoice::Kind::session, id});
            const auto lessons = host.campaign()->skill_lessons(id, visit.hall);
            if (lessons.options.empty())
            {
                continue;
            }
            const bool learned = host.campaign()->skill_training_denial(id, visit.hall) ==
                                 TrainingDenial::already_trained;
            add(Message{learned ? "{name}: skill lesson (learned at this level)"
                                : "{name}: skill lesson",
                        {{"name", name}}},
                {HallChoice::Kind::lessons, id});
        }
        add({"Leave", {}}, {HallChoice::Kind::leave});
    }
    host.show_host_menu(std::move(dialogue), std::move(choices));
}

bool HallService::choose_at_hall(TownServiceHost &host, std::size_t choice)
{
    using opengold::TrainingDenial;
    auto &visit = *hall_visit_;
    const auto picked = visit.choices.at(choice);
    const auto name = picked.member ? host.campaign()->member(picked.member).character.sheet().name
                                    : std::string{};
    switch (picked.kind)
    {
    case HallChoice::Kind::train:
    case HallChoice::Kind::confirm:
    case HallChoice::Kind::learn:
        return choose_at_original_hall(host, picked);
    case HallChoice::Kind::leave:
    {
        if (!host.script_machine().resume_host(visit.request,
                                               host.character_reply(host.selected_character())))
        {
            throw EclError("Training hall reply rejected");
        }
        hall_visit_.reset();
        host.screen().host_dialogue.clear();
        host.screen().host_choices.clear();
        host.screen().dialogue.clear();
        host.screen().phase = TourPhase::running;
        ++host.screen().revision;
        return true;
    }
    case HallChoice::Kind::back:
        visit.member = 0;
        show_hall(host, {});
        return true;
    case HallChoice::Kind::lessons:
    {
        const auto denial = host.campaign()->skill_training_denial(picked.member, visit.hall);
        if (denial != TrainingDenial::none && denial != TrainingDenial::gold)
        {
            show_hall(host, {refusal(denial, name, 0)});
            return true;
        }
        visit.member = picked.member;
        show_hall(host, {});
        return true;
    }
    case HallChoice::Kind::session:
    {
        const auto offer = host.campaign()->experience_offer(picked.member, visit.hall);
        if (offer.denial != TrainingDenial::none)
        {
            show_hall(host, {refusal(offer.denial, name, offer.fee)});
            return true;
        }
        host.campaign()->buy_experience(picked.member, visit.hall);
        show_hall(host, {{"{name} gains {experience} experience.",
                          {{"name", name}, {"experience", amount(offer.experience)}}}});
        return true;
    }
    case HallChoice::Kind::lesson:
    {
        if (host.campaign()->rule_module().trains_at_halls())
        {
            host.campaign()->train_hall_lesson(picked.member, visit.class_mask, picked.grant);
            visit.member = 0;
            show_hall(host, {{"{name}'s petition was accepted.", {{"name", name}}}});
            return true;
        }
        const auto fee = opengold::skill_training_fee(picked.grant);
        auto denial = host.campaign()->skill_training_denial(picked.member, visit.hall);
        if (denial == TrainingDenial::none && host.campaign()->purse_gold() < fee)
        {
            denial = TrainingDenial::gold;
        }
        if (denial != TrainingDenial::none)
        {
            show_hall(host, {refusal(denial, name, fee)});
            return true;
        }
        const auto lessons = host.campaign()->skill_lessons(picked.member, visit.hall).options;
        const auto lesson = std::find_if(lessons.begin(), lessons.end(),
                                         [&](const auto &option)
                                         {
                                             return option.id == picked.grant;
                                         });
        host.campaign()->train_skill(picked.member, visit.hall, picked.grant);
        visit.member = 0;
        show_hall(host,
                  {{picked.grant.starts_with("expertise:") ? "{name} gains Expertise in {skill}."
                                                           : "{name} learns {skill}.",
                    {{"name", name}, {"skill", lesson->label, true}}}});
        return true;
    }
    }
    return false;
}

namespace
{
// What the original's hall says when it turns a member away (chargen.pas,
// TrainingMenu).
Message original_refusal(opengold::TrainingDenial denial)
{
    using opengold::TrainingDenial;
    switch (denial)
    {
    case TrainingDenial::gold:
        return {"Training costs 1000 gp.", {}};
    case TrainingDenial::other_hall:
        return {"We don't train that class here.", {}};
    case TrainingDenial::experience:
        return {"Not enough experience.", {}};
    default:
        return {"We only train conscious people.", {}};
    }
}

// The original's class slots by their stable IDs, with their names.
constexpr std::array<std::pair<std::string_view, std::string_view>, 8> original_classes{
    {{"cleric", "Cleric"},
     {"druid", "Druid"},
     {"fighter", "Fighter"},
     {"paladin", "Paladin"},
     {"ranger", "Ranger"},
     {"magic_user", "Magic-User"},
     {"thief", "Thief"},
     {"monk", "Monk"}}};
} // namespace

void HallService::show_original_hall(TownServiceHost &host, std::vector<Message> notice)
{
    auto &visit = *hall_visit_;
    auto dialogue = std::move(notice);
    std::vector<Message> choices;
    visit.choices.clear();
    const auto add = [&](Message label, HallChoice choice)
    {
        choices.push_back(std::move(label));
        visit.choices.push_back(std::move(choice));
    };
    if (visit.member)
    {
        add({"Train", {}}, {HallChoice::Kind::confirm, visit.member});
        add({"Back", {}}, {HallChoice::Kind::back});
    }
    else
    {
        // Pools of Darkness trains for nothing, and its hall names no price
        // (chargen.pas, TrainingMenu).
        if (dialogue.empty() &&
            (host.game() == GoldBoxGame::pools_of_darkness || krynn_game(host.game())))
        {
            dialogue.push_back({"Who will train?", {}});
        }
        if (dialogue.empty())
        {
            dialogue.push_back({"Training costs 1000 gp. Who will train?", {}});
        }
        for (const auto id : host.campaign()->state().slots)
        {
            if (id)
            {
                add({"{name}", {{"name", host.campaign()->member(id).character.sheet().name}}},
                    {HallChoice::Kind::train, id});
                for (const auto &lesson :
                     host.campaign()->hall_lessons(id, visit.class_mask).options)
                {
                    add({"{name}: {lesson}",
                         {{"name", host.campaign()->member(id).character.sheet().name},
                          {"lesson", lesson.label, true}}},
                        {HallChoice::Kind::lesson, id, lesson.id});
                }
            }
        }
        add({"Leave", {}}, {HallChoice::Kind::leave});
    }
    host.show_host_menu(std::move(dialogue), std::move(choices));
}

bool HallService::choose_at_original_hall(TownServiceHost &host, const HallChoice &picked)
{
    using opengold::TrainingDenial;
    auto &visit = *hall_visit_;
    const auto &member = host.campaign()->member(picked.member);
    const auto name = member.character.sheet().name;
    const auto offer = host.campaign()->hall_training(picked.member, visit.class_mask);
    if (offer.denial != TrainingDenial::none)
    {
        visit.member = 0;
        show_hall(host, {original_refusal(offer.denial)});
        return true;
    }
    if (picked.kind == HallChoice::Kind::train)
    {
        // "Gareth will become: a level 3 Fighter, a level 2 Thief."
        std::vector<Message> becomes{{"{name} will become:", {{"name", name}}}};
        const auto &levels = member.character.sheet().original->class_levels;
        for (const auto &klass : offer.classes)
        {
            const auto found = std::ranges::find(
                original_classes, klass, &std::pair<std::string_view, std::string_view>::first);
            const auto slot =
                klass == "knight" ? 7U : static_cast<std::size_t>(found - original_classes.begin());
            const auto label = klass == "knight" ? std::string_view{"Knight"} : found->second;
            becomes.push_back({"a level {level} {class}",
                               {{"level", amount(static_cast<unsigned>(levels.at(slot) + 1))},
                                {"class", std::string(label), true}}});
        }
        visit.member = picked.member;
        show_hall(host, std::move(becomes));
        return true;
    }
    // A magic-user's level learns a spell (chargen.pas, TrainingMenu).
    const bool learns = std::ranges::find(offer.classes, "magic_user") != offer.classes.end();
    const auto options =
        learns ? host.campaign()
                     ->advancement_options(picked.member,
                                           host.campaign()->default_advancement(picked.member))
                     .spells
               : std::vector<rules::AdvancementOption>{};
    if (picked.kind == HallChoice::Kind::confirm && !options.empty())
    {
        std::vector<Message> choices;
        visit.choices.clear();
        for (const auto &option : options)
        {
            choices.push_back({"{spell}", {{"spell", option.label, true}}});
            visit.choices.push_back({HallChoice::Kind::learn, picked.member, option.id});
        }
        host.show_host_menu({{"Choose a spell for {name} to learn.", {{"name", name}}}},
                            std::move(choices));
        return true;
    }
    std::vector<std::string> spells;
    if (picked.kind == HallChoice::Kind::learn)
    {
        spells.push_back(picked.grant);
    }
    host.campaign()->train_at_hall_level(picked.member, visit.class_mask, std::move(spells));
    visit.member = 0;
    show_hall(host, {{"Congratulations... {name} has trained.", {{"name", name}}}});
    return true;
}

} // namespace opengold::por
