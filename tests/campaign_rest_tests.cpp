#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <limits>
#include <stdexcept>
using namespace opengold;
using namespace opengold::rules;

namespace
{
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

template <class F> void rejects(F f)
{
    bool caught = false;
    try
    {
        f();
    }
    catch (const std::exception &)
    {
        caught = true;
    }
    check(caught, "Invalid rest request must reject");
}

auto module()
{
    return srd5::load(std::filesystem::path(OPENGOLD_SOURCE_DIR) /
                      "data/rules/srd-5.2.1/combat.rules");
}

Character hero(std::string klass = "fighter", unsigned level = 4)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = klass;
    d.background = "soldier";
    d.alignment = "neutral_good";
    d.name = "Rest tester";
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    Character result(*srd5::character_rules(), d, {});
    VitalState scratch;
    for (unsigned n = 2; n <= level; ++n)
        check(result.advance(*module(), scratch), "Fixture level is supported");
    return result;
}

// A level-four human's vital record with every Second Wind and first-level
// spell slot spent; Hit Dice, Action Surge and Arcane Recovery stay unspent.
std::string spent_resources(std::string_view character_class)
{
    return character_class == "Wizard" ? "SRD9 0 0 3 0 0 0 4 0 0 0 \"\" 0 0 1 FX8 1 0 0"
           : "SRD9 0 0 0 0 0 0 4 0 0 0 \"\" 0 1 0 FX8 1 0 0";
}

// A level-four human Fighter at 0 HP, still rolling death saves.
std::string dying_resources(int successes, int failures)
{
    return "SRD9 0 0 0 " + std::to_string(successes) + ' ' + std::to_string(failures) +
           " 0 4 6000 0 0 \"\" 0 1 0 FX8 1 0 0";
}

std::string saved(const CampaignParty &p)
{
    return encode_campaign(p, nullptr, "campaign-rest");
}

CampaignParty loaded(std::string_view bytes)
{
    CampaignParty p(module());
    p.restore(decode_campaign(bytes, *srd5::character_rules(), *module(), "campaign-rest", nullptr)
              .party);
    return p;
}

unsigned winds(const PartyMember &member)
{
    const auto info = module()->recovery_info(member.character.sheet(), member.vitals);
    for (const auto &pool : info.resources)
        if (pool.id == "second_wind")
            return pool.remaining;
    throw std::runtime_error("Missing Second Wind pool");
}

// Deliberately non-SRD rest behavior proves Core applies module outcomes.
class AlternateRestRules final : public RulesModule
{
  public:
    Identity identity() const override
    {
        return module()->identity();
    }

    std::vector<std::string> supported_features() const override
    {
        return {};
    }

    std::unique_ptr<CombatSession> create(Encounter, std::uint64_t) const override
    {
        return {};
    }

    std::unique_ptr<CombatSession> restore(std::string_view) const override
    {
        return {};
    }

    CharacterProfile character_profile(const CharacterSheet &sheet,
                                       std::span<const std::string> gear) const override
    {
        return module()->character_profile(sheet, gear);
    }

    EquipmentChange equipment_change(const CharacterSheet &sheet, std::span<const std::string> gear,
                                     unsigned selected,
                                     EquipmentOperation operation) const override
    {
        return module()->equipment_change(sheet, gear, selected, operation);
    }

    RecoveryInfo recovery_info(const CharacterSheet &, const VitalState &) const override
    {
        RecoveryInfo r;
        r.can_rest = true;
        return r;
    }

    RestPolicy short_rest_policy() const override
    {
        return {2, 0};
    }

    RestPolicy long_rest_policy() const override
    {
        return {3, 0};
    }

    void recover_short_rest(VitalState &vitals, const CharacterSheet &) const override
    {
        vitals.hit_points = 5;
    }

    void recover(VitalState &vitals, const CharacterSheet &) const override
    {
        vitals.hit_points = 7;
    }
};

void alternate_rules_boundary()
{
    CampaignParty party(std::make_unique<AlternateRestRules>());
    auto person = hero();
    const auto armor = person.inventory().add("chain_mail", "Alternate rest armor");
    const auto id = party.add_pc(std::move(person));
    party.equip(id, armor);
    const auto short_rest = party.rest(RestKind::short_rest);
    check(short_rest && short_rest->duration_minutes == 2 && short_rest->spending &&
          party.member(id).vitals.hit_points == 5 && party.state().time_minutes == 2,
          "Core applies the module's Short Rest duration and benefits");
    party.finish_short_rest(*short_rest->spending);
    const auto long_rest = party.rest(RestKind::long_rest);
    check(long_rest && long_rest->duration_minutes == 3 &&
          party.member(id).vitals.hit_points == 7 && party.state().time_minutes == 5,
          "Core applies the module's Long Rest duration and benefits");
    check(party.member(id).equipped == std::vector<std::uint64_t> {armor},
          "Resting never unequips gear");
}

void individual_eligibility()
{
    CampaignParty party(module());
    const auto f = party.add_pc(hero()), w = party.add_pc(hero("wizard"));
    const auto unconscious = party.add_pc(hero()), dead = party.add_pc(hero()),
               reserve = party.add_pc(hero());
    party.remove(reserve);
    const auto npc = party.recruit("rest-companion", hero());
    auto state = party.checkpoint();
    state.time_minutes = 1000;
    state.subminute_milliseconds = 3000;
    state.random_state = 29;
    for (auto &m : state.roster)
        m.vitals = {1, false, spent_resources(m.character.sheet().character_class)};
    state.roster[1].last_rest_minutes = 41;
    state.roster[1].last_rest_subminute_milliseconds = 4000;
    state.roster[2].vitals = {0, false, dying_resources(1, 2)};
    state.roster[3].vitals = {0, true, "SRD9 0 0 0 0 3 0 4 0 0 0 \"\" 0 1 0 FX8 1 0 0"};
    party.restore(state);
    const auto before = saved(party);
    const auto info = party.rest_info(RestKind::long_rest);
    check(info.size() == 5 && info[0].id == f && info[0].denial == RestDenial::none &&
          info[1].id == w && info[1].denial == RestDenial::cooldown &&
          info[1].wait_milliseconds == 61000 && info[2].id == unconscious &&
          info[2].denial == RestDenial::vitality && info[3].id == dead &&
          info[3].denial == RestDenial::vitality && info[4].id == npc &&
          info[4].denial == RestDenial::none && saved(party) == before,
          "Eligibility query is individual, exact and read-only; reserves are absent");
    const auto result = party.rest(RestKind::long_rest);
    check(result && result->members == std::vector<MemberId> {f, npc} && !result->spending &&
          result->duration_minutes == 480,
          "Only eligible active PCs and NPCs complete the Long Rest");
    check(party.state().time_minutes == 1480 && party.state().subminute_milliseconds == 3000 &&
          party.state().random_state == 11400714819323198514ULL,
          "One group rest advances eight hours once, including the known natural-one death save");
    check(party.member(f).last_rest_minutes == 1480 &&
          party.member(f).last_rest_subminute_milliseconds == 3000 &&
          party.member(npc).last_rest_minutes == 1480 &&
          party.member(f).vitals.hit_points == party.member(f).character.sheet().hit_points,
          "Only successful members receive HP and completion timestamps");
    for (auto id :
            {
                w, dead, reserve
            })
    {
        // Elapsed time rewrites the description, never the resources.
        const auto &vitals = party.member(id).vitals, &original = state.roster[id - 1].vitals;
        check(vitals.hit_points == original.hit_points && vitals.dead == original.dead &&
              vitals.resources == original.resources &&
              party.member(id).last_rest_minutes == state.roster[id - 1].last_rest_minutes,
              "Ineligible and reserve members gain no rest resources or cooldown reset");
    }

    check(party.member(unconscious).vitals.dead &&
          party.member(unconscious).vitals.resources ==
          "SRD9 0 0 0 1 4 0 4 0 0 0 \"\" 0 1 0 FX8 1 0 0" &&
          !party.member(unconscious).last_rest_minutes,
          "Ineligible mortality continues without replenishing resources or recording a rest");
    check(
        party.rest_info(RestKind::long_rest)[1].denial == RestDenial::none,
        "Eligibility is checked at the next rest start, not granted midway through the previous rest");
    auto copy = loaded(saved(party));
    const auto later = copy.rest(RestKind::long_rest);
    check(later && later->members == std::vector<MemberId> {w} &&
          copy.member(f).last_rest_minutes == 1480,
          "Mixed cooldowns persist across load");
    copy.keep_rest_spells(w);
    copy.remove(f);
    copy.advance_time_milliseconds(480ULL * 60000 - 1);
    copy.rejoin(f);
    auto status = copy.rest_info(RestKind::long_rest);
    const auto found = std::find_if(status.begin(), status.end(),
                                    [&](const auto & i)
    {
        return i.id == f;
    });
    check(found != status.end() && found->wait_milliseconds == 1,
          "Removal/rejoin preserves the cooldown down to one millisecond");
    copy.advance_time_milliseconds(1);
    status = copy.rest_info(RestKind::long_rest);
    check(std::find_if(
              status.begin(), status.end(),
              [&](const auto & i)
    {
        return i.id == f;
    })->denial == RestDenial::none,
    "Exact cooldown boundary permits recovery");
}

void spending_and_continuation()
{
    CampaignParty party(module());
    const auto f = party.add_pc(hero()), w = party.add_pc(hero("wizard"));
    const auto reserve = party.add_pc(hero());
    party.remove(reserve);
    auto state = party.checkpoint();
    state.subminute_milliseconds = 1234;
    for (auto &m : state.roster)
        m.vitals = {1, false, spent_resources(m.character.sheet().character_class)};
    party.restore(state);
    rejects(
        [&]
    {
        (void)party.heal_with_hit_dice({1, 1}, f);
    });
    const auto result = party.rest(RestKind::short_rest);
    check(result && result->spending.has_value(), "Completed hour opens a spending session");
    const auto ticket = *result->spending;
    const auto after_rest = saved(party);
    check(
        party.state().time_minutes == 60 && party.state().subminute_milliseconds == 1234 &&
        party.member(f).vitals.hit_points == 1 && winds(party.member(f)) == 1 &&
        winds(party.member(reserve)) == 0 && !party.member(f).last_rest_minutes,
        "Short Rest recharges one Second Wind without healing, reserves or Long Rest cooldown changes");
    check(party.rest_info(RestKind::long_rest)[0].denial == RestDenial::spending,
          "Query reports the active spending window");
    rejects(
        [&]
    {
        (void)party.heal_with_hit_dice(ticket, reserve);
    });
    rejects(
        [&]
    {
        (void)party.heal_with_hit_dice(ticket, 999);
    });
    rejects(
        [&]
    {
        party.remove(f);
    });
    rejects(
        [&]
    {
        (void)party.rest();
    });
    check(
        saved(party) == after_rest,
        "Invalid targets, removal and repeated rests preserve the completed hour, resources and RNG");
    const auto rolls = party.heal_with_hit_dice(ticket, f);
    const auto &healed = party.member(f);
    int healing = 0;
    for (const auto &roll : rolls)
        healing += roll.healing;
    check(rolls.size() >= 2 && rolls[0].roll == 4 && rolls[0].modifier == 2 &&
          rolls[0].healing == 6 && rolls[0].remaining == 3 && rolls[1].roll == 2 &&
          rolls[1].healing == 4 && rolls[1].remaining == 2,
          "Dice are spent one at a time with their known rolls and healing");
    check((rolls.back().remaining == 0 ||
           healed.vitals.hit_points == healed.character.sheet().hit_points) &&
          healed.vitals.hit_points == 1 + healing &&
          party.state().random_state ==
          11400714819323198527ULL + (rolls.size() - 1) * 0x9e3779b97f4a7c15ULL,
          "One action heals until full HP or out of dice, with one RNG draw per die");
    const auto after_heal = saved(party);
    rejects(
        [&]
    {
        (void)party.heal_with_hit_dice(ticket, f);
    });
    rejects(
        [&]
    {
        party.finish_short_rest(ticket);
    });
    check(saved(party) == after_heal, "Duplicate heal and stale Finish callbacks are atomic");
    auto copy = loaded(after_heal);
    check(saved(copy) == after_heal,
          "Save/load retains the exact spending window and expenditure");
    rejects(
        [&]
    {
        (void)copy.heal_with_hit_dice(copy.state().short_rest->ticket, f);
    });
    check(saved(copy) == after_heal, "Nothing left to heal spends no die or RNG");
    const auto finish = copy.state().short_rest->ticket;
    copy.finish_short_rest(finish);
    check(!copy.state().short_rest && copy.member(f).vitals.hit_points == 1 + healing &&
          copy.state().time_minutes == 60 && winds(copy.member(f)) == 1,
          "Finish keeps all spent dice and healing and never repeats the rest");
    const auto finished = saved(copy);
    rejects(
        [&]
    {
        copy.finish_short_rest(finish);
    });
    rejects(
        [&]
    {
        (void)copy.heal_with_hit_dice(finish, f);
    });
    check(saved(copy) == finished, "Finished sessions cannot be reused");
    party.finish_short_rest(party.state().short_rest->ticket);
    const auto zero = copy.rest(RestKind::short_rest);
    copy.finish_short_rest(*zero->spending);
    check(copy.member(f).vitals.hit_points == 1 + healing && winds(copy.member(f)) == 2 &&
          copy.state().random_state == party.state().random_state,
          "A separate completed hour can finish with zero dice and still recharge one Second Wind");
    // Compare the next encounter before and after a save, including spent dice.
    auto restored = loaded(saved(copy));
    auto actors = copy.participants();
    actors.push_back({999, "bandit", "Enemy", 1, {6, 6}});
    auto other = restored.participants();
    other.push_back(actors.back());
    auto rules = module();
    auto combat = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 42);
    auto continued = rules->create({{8, 8, std::vector<std::uint8_t>(64)}, other}, 42);
    for (unsigned n = 0; n < 10; ++n)
    {
        const auto commands = combat->legal_commands();
        const auto end = std::find_if(commands.begin(), commands.end(),
                                      [](const auto & c)
        {
            return c.verb == "end";
        });
        check(end != commands.end() && combat->submit(*end) && continued->submit(*end) &&
              combat->save() == continued->save(),
              "Next encounter has identical resources and deterministic continuation");
    }
}

void expiry_and_atomicity()
{
    CampaignParty one_die(module());
    const auto one = one_die.add_pc(hero("fighter", 1));
    const auto completed = one_die.rest(RestKind::short_rest);
    const auto full = saved(one_die);
    rejects(
        [&]
    {
        (void)one_die.heal_with_hit_dice(*completed->spending, one);
    });
    check(saved(one_die) == full, "A character at full HP spends no Hit Die, HP or RNG");
    auto wounded = one_die.checkpoint();
    wounded.roster.front().vitals.hit_points = 1;
    one_die.restore(wounded);
    const auto last = one_die.heal_with_hit_dice(one_die.state().short_rest->ticket, one);
    check(last.size() == 1 && last.back().remaining == 0,
          "Healing stops when the only Hit Die is spent");
    const auto depleted = saved(one_die);
    rejects(
        [&]
    {
        (void)one_die.heal_with_hit_dice(one_die.state().short_rest->ticket, one);
    });
    check(saved(one_die) == depleted, "An exhausted pool preserves the fresh ticket, HP and RNG");
    one_die.finish_short_rest(one_die.state().short_rest->ticket);
    CampaignParty party(module());
    const auto id = party.add_pc(hero());
    auto rest = party.rest(RestKind::short_rest);
    const auto ticket = *rest->spending;
    const auto before = saved(party);
    party.advance_time_milliseconds(0);
    check(saved(party) == before, "Zero elapsed time does not expire spending");
    party.advance_time_milliseconds(1);
    check(!party.state().short_rest, "Positive elapsed time expires spending");
    const auto expired = saved(party);
    rejects(
        [&]
    {
        (void)party.heal_with_hit_dice(ticket, id);
    });
    check(saved(party) == expired, "Expired request preserves state and RNG");
    rest = party.rest(RestKind::short_rest);
    check(rest->spending->session > ticket.session, "New rests never reuse session IDs");
    const auto pending = saved(party);
    party.begin_combat();
    check(party.rest_info(RestKind::short_rest)[0].denial == RestDenial::combat,
          "Pending combat prevents resting");
    Snapshot invalid;
    rejects(
        [&]
    {
        party.apply_combat(invalid);
    });
    party.end_combat();
    check(saved(party) == pending,
          "Failed combat initialization preserves the pending window and committed dice");
    auto actors = party.participants();
    actors.push_back({999, "bandit", "Enemy", 1, {6, 6}});
    const auto combat = module()->create(
    { {8, 8, std::vector<std::uint8_t>(64)}, actors
    }, 42);
    party.begin_combat();
    party.apply_combat(combat->snapshot());
    check(!party.state().short_rest, "Successful initiative closes the spending window");
    rejects(
        [&]
    {
        (void)party.heal_with_hit_dice(*rest->spending, id);
    });
    rejects(
        [&]
    {
        (void)party.rest();
    });
    party.end_combat();
    for (const bool short_rest :
            {
                false, true
            })
    {
        auto state = party.checkpoint();
        state.time_minutes = std::numeric_limits<std::uint64_t>::max();
        state.roster[0].last_rest_minutes.reset();
        party.restore(state);
        const auto overflow = saved(party);
        rejects(
            [&]
        {
            (void)party.rest(short_rest ? RestKind::short_rest : RestKind::long_rest);
        });
        check(saved(party) == overflow,
              "Clock overflow cannot partially apply resources or timers");
    }
    auto state = party.checkpoint();
    state.time_minutes = 0;
    state.next_rest_session = std::numeric_limits<std::uint64_t>::max();
    party.restore(state);
    const auto exhausted = saved(party);
    rejects(
        [&]
    {
        (void)party.rest(RestKind::short_rest);
    });
    check(saved(party) == exhausted, "Session counter exhaustion is atomic");
    state.next_rest_session = 1;
    state.roster[0].vitals = {0, false, dying_resources(1, 2)};
    party.restore(state);
    const auto unconscious = saved(party);
    check(!party.rest(RestKind::short_rest) && !party.rest() && saved(party) == unconscious,
          "No eligible member denies both kinds without time or RNG");
    party.remove(id);
    check(!party.rest(RestKind::short_rest) && !party.rest(),
          "Empty parties cannot complete rests");
}

void effects_once()
{
    auto rules = module();
    CampaignParty party(module());
    const auto pc = party.add_pc(hero());
    party.recruit("effect-npc", hero());
    const auto reserve = party.add_pc(hero());
    party.remove(reserve);
    auto state = party.checkpoint();
    state.subminute_milliseconds = 4321;
    for (auto &m : state.roster)
        m.vitals = {1,
                    false,
                    "SRD9 0 0 0 0 0 0 4 0 0 0 \"\" 0 1 0 FX8 2 1 1 1 77 99 \"Source caster\" 13 "
                    "43000 2000 0"
                   };
    for (const auto kind :
            {
                RestKind::short_rest, RestKind::long_rest
            })
    {
        party.restore(state);
        CampaignParty elapsed(module());
        elapsed.restore(state);
        elapsed.advance_time(kind == RestKind::short_rest ? 60 : 480);
        check(party.rest(kind).has_value(), "Rest with active effects completes");
        check(party.state().time_minutes == elapsed.state().time_minutes &&
              party.state().subminute_milliseconds == 4321 &&
              party.state().random_state == elapsed.state().random_state &&
              party.state().random_state != state.random_state,
              "Rest performs exactly the same timed recovery rolls as one elapsed interval");
        for (const auto &m : party.state().roster)
            check(m.vitals.resources.ends_with("FX8 2 0 0"),
                  "Timed effects expire during the rest");
        check(party.member(reserve).vitals == elapsed.member(reserve).vitals,
              "Reserve effects advance without receiving rest recharge");
        if (party.state().short_rest)
            party.finish_short_rest(party.state().short_rest->ticket);
    }
    // A malformed member rejects before any other member can receive benefits.
    state.roster[1].vitals.resources = "invalid";
    party.restore(state);
    const auto before = saved(party);
    rejects(
        [&]
    {
        (void)party.rest(RestKind::short_rest);
    });
    rejects(
        [&]
    {
        (void)party.rest(RestKind::long_rest);
    });
    check(saved(party) == before && party.member(pc).vitals == state.roster[0].vitals,
          "Invalid resources preserve the whole transaction and recovery RNG");
}

using Bytes = std::vector<std::uint8_t>;

std::shared_ptr<const por::EclProgram> program(Bytes body)
{
    Bytes bytes{0, 0};
    for (int n = 0; n < 5; ++n)
        bytes.insert(bytes.end(), {1, 1, 0x15, 0x99});
    bytes.push_back(0);
    bytes.insert(bytes.end(), body.begin(), body.end());
    return std::make_shared<const por::EclProgram>(por::EclProgram::decode(bytes, "rest host"));
}

void settle(por::RolfTourSession &town)
{
    for (unsigned n = 0; n < 100 && town.snapshot().phase == por::TourPhase::running; ++n)
        town.advance(.5);
    check(town.snapshot().phase != por::TourPhase::faulted, "Rest fixture script fault");
}

void campaign_services()
{
    auto party = std::make_shared<CampaignParty>(module());
    const auto id = party->add_pc(hero());
    auto state = party->checkpoint();
    state.roster[0].vitals = {1, false, spent_resources("Fighter")};
    party->restore(state);
    auto resources = std::make_shared<por::PhlanResources>();
    auto p = program({0});
    resources->programs[0] = p;
    por::RolfTourSession town({}, p, {}, 0x9914, {}, resources);
    town.campaign_party(party);
    settle(town);
    check(town.camp(RestKind::short_rest), "Short Rest enters the original pre-camp service");
    settle(town);
    check(town.can_leave() && party->state().short_rest && party->state().time_minutes == 60 &&
          town.script_variable(0x49c9) == 13,
          "Safe camp grants a completed hour and updates original clock registers");
    check(!town.explore(por::ExplorationCommand::forward) && !town.camp(RestKind::long_rest),
          "Campaign events cannot run over pending spending");
    const auto rolls = party->heal_with_hit_dice(party->state().short_rest->ticket, id);
    const int healed_hp = party->member(id).vitals.hit_points;
    check(rolls.front().healing == 6, "Campaign service permits committed Hit Dice healing");
    const auto bytes = encode_campaign(*party, &town, "campaign-rest");
    auto disk = decode_campaign(bytes, *srd5::character_rules(), *module(), "campaign-rest", &town);
    auto resumed = std::make_shared<CampaignParty>(module());
    resumed->restore(disk.party);
    disk.town->attach_restored_party(resumed);
    check(encode_campaign(*resumed, &*disk.town, "campaign-rest") == bytes,
          "Idle town saves preserve the pending spending window");
    resumed->finish_short_rest(resumed->state().short_rest->ticket);
    check(disk.town->explore(por::ExplorationCommand::look), "Finish permits exploration");
    settle(*disk.town);
    check(resumed->member(id).vitals.hit_points == healed_hp &&
          disk.town->script_variable(0x6c19) == healed_hp,
          "Next script cannot overwrite committed Hit Die healing with stale HP");
    for (const auto kind :
            {
                RestKind::short_rest, RestKind::long_rest
            })
        for (const auto chance :
                {
                    255u, 50u, 100u, 101u, 102u, 200u, 254u
                })
        {
            party->restore(state);
            auto script = program({9, 0, 1, 1, 0xd2, 0x6d, 9, 0, static_cast<std::uint8_t>(chance),
                                   1, 0xd3, 0x6d, 0});
            por::RolfTourSession blocked({}, script, {}, 0x9914, {}, resources);
            blocked.campaign_party(party);
            settle(blocked);
            check(blocked.camp(kind), "Both kinds enter the original camp checks");
            settle(blocked);
            check(
                !party->state().short_rest &&
                party->member(id).vitals.hit_points == state.roster[0].vitals.hit_points &&
                party->member(id).vitals.resources == state.roster[0].vitals.resources &&
                party->state().random_state == 42 &&
                party->state().time_minutes == ((chance == 100 || chance == 101) ? 5 : 0),
                "Forbidden, unsupported and five-minute interrupted camps grant neither resources nor spending rights");
            if (chance != 255 && chance != 100 && chance != 101)
                check(
                    !blocked.script_diagnostics().empty(),
                    "Unverified interruption profiles reject explicitly instead of becoming city-watch events");
        }
    // Inn completion is still an event transaction. An unsupported instruction
    // after recovery restores the pre-event clock, resources, cooldowns and RNG.
    party->restore(state);
    auto failed = program({56, 0, 9, 56, 0, 0, 0});
    por::RolfTourSession inn({}, failed, {}, 0x9914, {}, resources);
    inn.campaign_party(party);
    settle(inn);
    const auto before = saved(*party);
    check(inn.explore(por::ExplorationCommand::look), "Inn rollback fixture starts");
    settle(inn);
    check(saved(*party) == before && !inn.script_diagnostics().empty(),
          "A failed inn continuation rolls back the entire rest transaction");
}

void watch_interruption_and_rollback()
{
    for (const bool failure :
            {
                false, true
            })
    {
        auto party = std::make_shared<CampaignParty>(module());
        auto person = hero();
        const auto sword = person.inventory().add("longsword", "Watch camp sword", 1, 34);
        const auto owner = party->add_pc(std::move(person));
        party->equip(owner, sword);
        auto initial = party->checkpoint();
        initial.roster[0].vitals = {1, false, spent_resources("Fighter")};
        party->restore(initial);
        Bytes bytes{0, 0};
        for (unsigned n = 0; n < 5; ++n)
            bytes.insert(bytes.end(), {1, 1, 0x15, 0x99});
        bytes.push_back(0);
        const Bytes pre{9, 0, 1, 1, 0xd2, 0x6d, 9, 0, 101, 1, 0xd3, 0x6d, 0};
        const unsigned arrival = 0x9915 + pre.size();
        bytes[16] = arrival & 255;
        bytes[17] = arrival >> 8;
        bytes.insert(bytes.end(), pre.begin(), pre.end());
        if (failure)
            bytes.insert(bytes.end(), {56, 0, 0, 0});
        else
            bytes.push_back(0);
        const auto script = std::make_shared<const por::EclProgram>(
                                por::EclProgram::decode(bytes, "watch recovery"));
        auto resources = std::make_shared<por::PhlanResources>();
        resources->programs[0] = script;
        por::RolfTourSession town({}, script, {}, 0x9914, {}, resources);
        town.campaign_party(party);
        settle(town);
        const auto before = saved(*party);
        check(town.camp(RestKind::long_rest), "Start city-watch camp");
        settle(town);
        if (failure)
        {
            check(saved(*party) == before && !town.script_diagnostics().empty(),
                  "Failed watch continuation rolls back time and RNG");
            continue;
        }
        check(party->state().time_minutes == 5 && !party->state().short_rest &&
              town.snapshot().dialogue.find("The rest was interrupted.") != std::string::npos,
              "The watch interrupts the rest after five minutes and says so");
        const auto &member = party->member(owner);
        check(member.equipped == std::vector<std::uint64_t> {sword},
              "An interrupted rest leaves equipment untouched");
        check(member.vitals.hit_points == 1 && winds(member) == 0 &&
              party->state().random_state == 42,
              "An interrupted rest grants no recovery and draws no RNG");
    }
}

std::string payload(std::string body)
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : body)
    {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return "OPENGOLD-CAMPAIGN 20\n" + std::to_string(hash) + '\n' + body;
}

void malformed_continuation()
{
    CampaignParty party(module());
    const auto id = party.add_pc(hero());
    (void)party.rest(RestKind::short_rest);
    const auto good = saved(party);
    auto body = good.substr(good.find('\n', good.find('\n') + 1) + 1);
    // The Short Rest continuation is followed by no spell or training rest records.
    const std::string tail = "2 1 1 1 60 0 1 1 ", later = "0 0 ";
    check(body.ends_with(tail + later), "Independent fixture locates the Short Rest continuation");
    const auto prefix = body.substr(0, body.size() - tail.size() - later.size());
    for (const auto bad :
            {"0 0 ", "1 1 1 1 60 0 1 1 ", "2 1 0 1 60 0 1 1 ", "2 1 1 0 60 0 1 1 ",
             "2 1 1 1 59 0 1 1 ", "2 1 1 1 60 1 1 1 ", "2 1 1 1 60 0 0 ",
             "2 1 1 1 60 0 2 1 1 ", "2 1 1 1 60 0 1 999 "
            })
        rejects(
            [&]
    {
        (void)loaded(payload(prefix + bad + later));
    });
    check(saved(party) == good,
          "Correct-checksum malformed continuations cannot replace the live campaign");
    auto state = party.checkpoint();
    state.short_rest->ticket.revision = std::numeric_limits<std::uint64_t>::max();
    party.restore(state);
    const auto exhausted = saved(party);
    rejects(
        [&]
    {
        (void)party.heal_with_hit_dice(party.state().short_rest->ticket, id);
    });
    check(saved(party) == exhausted, "Revision exhaustion cannot consume a die or RNG");
    party.finish_short_rest(party.state().short_rest->ticket);
}

} // namespace

int main()
{
    try
    {
        alternate_rules_boundary();
        individual_eligibility();
        spending_and_continuation();
        expiry_and_atomicity();
        effects_once();
        campaign_services();
        watch_interruption_and_rollback();
        malformed_continuation();
        std::cout << "Campaign rest tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
