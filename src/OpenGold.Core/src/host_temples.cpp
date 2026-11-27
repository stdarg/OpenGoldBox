#include "opengold/rolf_tour.h"
#include "host_services.h"

#include <algorithm>

namespace opengold::por
{
using namespace host_services;

namespace
{
// The temple's priced sentences, kept whole per coin because the translations
// differ in more than the coin word: Krynn prices in steel, the Realms in gp.
struct TemplePriceTexts
{
    const char *cast_offer;
    const char *service_label;
    const char *cannot_pay;
};

[[nodiscard]] const TemplePriceTexts &temple_price_texts(GoldBoxGame game) noexcept
{
    static constexpr TemplePriceTexts steel{
        "The clerics will cast {spell} on {name} for {fee} steel.", "{spell} ({fee} steel)",
        "The party's steel cannot pay {fee} steel."};
    static constexpr TemplePriceTexts gold{"The clerics will cast {spell} on {name} for {fee} gp.",
                                           "{spell} ({fee} gp)",
                                           "The party's gold cannot pay {fee} gp."};
    return krynn_game(game) ? steel : gold;
}

} // namespace

void RolfTourSession::open_temple(std::uint64_t request)
{
    // The temple data/shops places on this square, else the original's list.
    auto offerings = opengold::original_temple_offerings(game());
    for (const auto &site : temples_)
    {
        if (standing_at(site.archive, site.record, site.markers))
        {
            offerings = site.offerings;
        }
    }
    // Resurrection is for Easy and Normal only, and none under rules that
    // have no such service (the original's).
    const auto &rules = campaign_->rule_module();
    std::erase_if(offerings,
                  [&](const auto &offering)
                  {
                      return !rules.offers_temple_service(offering.service) ||
                             (offering.service == opengold::rules::TempleService::resurrection &&
                              difficulty_ > opengold::Difficulty::normal);
                  });
    // Secret's HealMenu takes no money. Daggerford's released Eldath
    // priests also waive every donation (ECL1:39, rescue flag 265h).
    const bool freed_eldath = game() == GoldBoxGame::treasures_of_the_savage_frontier &&
                              snapshot_.area_id == 39 && machine_.variable(EclAddress{0x265}) != 0;
    if (game() == GoldBoxGame::secret_of_the_silver_blades ||
        game() == GoldBoxGame::death_knights_of_krynn ||
        game() == GoldBoxGame::dark_queen_of_krynn || freed_eldath)
    {
        for (auto &offering : offerings)
        {
            offering.price_gp = 0;
        }
    }
    temple_visit_ = TempleVisit{request, std::move(offerings), {}, 0, {}};
    show_temple({});
}

void RolfTourSession::show_temple(std::vector<Message> notice)
{
    using opengold::rules::TempleService;
    auto &visit = *temple_visit_;
    auto dialogue = std::move(notice);
    std::vector<Message> choices;
    visit.choices.clear();
    const auto add = [&](Message label, TempleChoice choice)
    {
        choices.push_back(std::move(label));
        visit.choices.push_back(std::move(choice));
    };
    const auto spell = [](const opengold::TempleOffering &offering)
    {
        return opengold::rules::MessageArgument{
            "spell", std::string(opengold::temple_service_info(offering.service).name), true};
    };
    if (visit.member)
    {
        const auto &offering = *visit.chosen;
        dialogue.push_back({temple_price_texts(game()).cast_offer,
                            {spell(offering),
                             {"name", campaign_->member(visit.member).character.sheet().name},
                             {"fee", amount(offering.price_gp)}}});
        add({"Pay", {}}, {TempleChoice::Kind::confirm, visit.member, offering});
        add({"Back", {}}, {TempleChoice::Kind::back});
    }
    else if (visit.chosen)
    {
        const auto &offering = *visit.chosen;
        if (dialogue.empty())
        {
            dialogue.push_back({"Choose who receives {spell}.", {spell(offering)}});
        }
        for (const auto id : campaign_->state().slots)
        {
            if (!id)
            {
                continue;
            }
            const auto name = campaign_->member(id).character.sheet().name;
            if (campaign_->temple_service_helps(id, offering.service))
            {
                add({"{name}", {{"name", name}}}, {TempleChoice::Kind::member, id, offering});
            }
            else if (offering.service == TempleService::raise_dead &&
                     campaign_->temple_service_helps(id, TempleService::resurrection))
            {
                add({"{name} (beyond Raise Dead)", {{"name", name}}},
                    {TempleChoice::Kind::beyond_raising, id, offering});
            }
        }
        add({"Back", {}}, {TempleChoice::Kind::back});
    }
    else
    {
        if (dialogue.empty())
        {
            dialogue.push_back({"The clerics will cast these spells for the party.", {}});
        }
        for (const auto &offering : visit.offerings)
        {
            add({temple_price_texts(game()).service_label,
                 {spell(offering), {"fee", amount(offering.price_gp)}}},
                {TempleChoice::Kind::service, 0, offering});
        }
        add({"Leave", {}}, {TempleChoice::Kind::leave});
    }
    show_hall_menu(std::move(dialogue), std::move(choices));
}

bool RolfTourSession::choose_at_temple(std::size_t choice)
{
    auto &visit = *temple_visit_;
    const auto picked = visit.choices.at(choice);
    const auto name =
        picked.member ? campaign_->member(picked.member).character.sheet().name : std::string{};
    const opengold::rules::MessageArgument spell{
        "spell", std::string(opengold::temple_service_info(picked.offering.service).name), true};
    switch (picked.kind)
    {
    case TempleChoice::Kind::leave:
    {
        auto reply = character_reply(selected_character_);
        reply.writes.push_back({game_cell(ecl_temple_service), 0});
        if (!machine_.resume_host(visit.request, reply))
        {
            throw EclError("Temple reply rejected");
        }
        temple_visit_.reset();
        snapshot_.host_dialogue.clear();
        snapshot_.host_choices.clear();
        snapshot_.choices.clear();
        snapshot_.dialogue.clear();
        snapshot_.phase = TourPhase::running;
        ++snapshot_.revision;
        return true;
    }
    case TempleChoice::Kind::back:
        if (visit.member)
        {
            visit.member = 0;
        }
        else
        {
            visit.chosen.reset();
        }
        show_temple({});
        return true;
    case TempleChoice::Kind::service:
    {
        const auto slots = campaign_->state().slots;
        const bool needed = std::any_of(
            slots.begin(), slots.end(),
            [&](opengold::MemberId id)
            {
                return id &&
                       (campaign_->temple_service_helps(id, picked.offering.service) ||
                        (picked.offering.service == opengold::rules::TempleService::raise_dead &&
                         campaign_->temple_service_helps(
                             id, opengold::rules::TempleService::resurrection)));
            });
        if (!needed)
        {
            show_temple({{"No one in the party needs {spell}.", {spell}}});
            return true;
        }
        visit.chosen = picked.offering;
        show_temple({});
        return true;
    }
    case TempleChoice::Kind::beyond_raising:
        show_temple({{"Raise Dead cannot bring back {name}, who has been dead too long or is dust.",
                      {{"name", name}}}});
        return true;
    case TempleChoice::Kind::member:
        if (campaign_->purse_gold() < picked.offering.price_gp)
        {
            show_temple({{temple_price_texts(game()).cannot_pay,
                          {{"fee", amount(picked.offering.price_gp)}}}});
            return true;
        }
        visit.member = picked.member;
        show_temple({});
        return true;
    case TempleChoice::Kind::confirm:
        campaign_->temple_service(picked.member, picked.offering.service, picked.offering.price_gp);
        visit.member = 0;
        visit.chosen.reset();
        show_temple({{"The clerics cast {spell} on {name}.", {spell, {"name", name}}}});
        return true;
    }
    return false;
}

} // namespace opengold::por
