#include "opengold/campaign_save.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <iostream>
#include <fstream>
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
                                       std::span<const std::string> gear,
                                       EquipmentState equipment) const override
    {
        return module()->character_profile(sheet, gear, equipment);
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
    RestProgress begin_rest(RestKind kind) const override
    {
        RestProgress p;
        p.kind = kind;
        p.work = RestWork::light_activity;
        return p;
    }
    RestTransition interrupt_rest(const RestProgress &before, RestInterruption cause) const override
    {
        if (before.kind == RestKind::long_rest)
            return {}; // Opposite of SRD: cancellation.
        auto p = before;
        p.interrupted = true;
        p.interruption = cause;
        return {p};
    }
    RestProgress resume_rest(const RestProgress &before) const override
    {
        auto p = before;
        p.interrupted = false;
        return p;
    }
    std::uint64_t remaining_rest(const RestProgress &) const override
    {
        return 120000;
    }
    RestTransition advance_rest(const RestProgress &, std::uint64_t, RestWork work) const override
    {
        check(work == RestWork::light_activity, "Core must use the module's default work");
        return {std::nullopt, RestBenefit::long_rest, 120000};
    }
    void recover(VitalState &vitals, const CharacterSheet &) const override
    {
        vitals.hit_points = 7;
    }
    std::vector<unsigned> released_equipment(const CharacterSheet &, const VitalState &,
                                             std::span<const std::string> gear) const override
    {
        return gear.empty() ? std::vector<unsigned>{} : std::vector<unsigned>{0};
    }
};
void alternate_rules_boundary()
{
    CampaignParty party(std::make_unique<AlternateRestRules>());
    auto person = hero();
    const auto armor = person.inventory().add("chain_mail", "Alternate rest armor");
    const auto id = party.add_pc(std::move(person));
    party.equip(id, armor);
    auto ticket = *party.begin_rest(RestKind::short_rest);
    check(party.member(id).equipped.empty() && party.state().detached_items.size() == 1 &&
              party.state().detached_items[0].item.definition_id == "chain_mail",
          "Core obeys module equipment releases even for awake Short Rest armor");
    party.interrupt_rest(ticket, RestInterruption::damage);
    check(party.state().rest_activity && party.state().rest_activity->interrupted &&
              !party.state().short_rest,
          "Core permits module-defined Short Rest resumption without SRD benefits");
    party.resume_rest(party.state().rest_activity->ticket);
    party.abandon_rest(party.state().rest_activity->ticket);
    ticket = *party.begin_rest(RestKind::long_rest);
    party.interrupt_rest(ticket, RestInterruption::damage);
    check(!party.state().rest_activity, "Core permits module-defined Long Rest cancellation");
    const auto result = party.rest(RestKind::short_rest);
    check(result && result->duration_minutes == 2 && party.member(id).vitals.hit_points == 7 &&
              !party.state().short_rest && party.state().time_minutes == 2,
          "Core applies module-defined completion benefits instead of branching on rest kind");
}
void freeze_activity_baseline()
{
    CampaignParty party(module());
    auto id = party.add_pc(hero());
    party.add_pc(hero("wizard"));
    auto state = party.checkpoint();
    state.time_minutes = 1000;
    state.subminute_milliseconds = 4321;
    state.random_state = 29;
    for (auto &member : state.roster)
        member.vitals = {1, false, "SRD1 0 0 0 0 0"};
    party.restore(state);
    (void)party.rest(RestKind::short_rest);
    check(party.state().short_rest->ticket.session == 1 && party.state().next_rest_session == 2,
          "Freeze requires pre-activity rest implementation");
    const auto write = [&](const char *name)
    {
        const auto bytes = saved(party);
        check(bytes.starts_with("OPENGOLD-CAMPAIGN 11\n") && party.identity().version == "0.6.40",
              "Freeze requires actual pre-activity writer");
        std::ofstream out(std::filesystem::path(OPENGOLD_SOURCE_DIR) / "tests/fixtures" / name,
                          std::ios::binary);
        out << bytes;
        check(bool(out), "Write prior rest fixture");
    };
    write("campaign-v11-rest-activity-before.ogs");
    (void)party.spend_hit_die(party.state().short_rest->ticket, id);
    write("campaign-v11-rest-activity-spent.ogs");
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
        m.vitals = {1, false, "SRD1 0 0 0 0 0"};
    state.roster[1].last_rest_minutes = 41;
    state.roster[1].last_rest_subminute_milliseconds = 4000;
    state.roster[2].vitals = {0, false, "SRD1 0 0 1 2 0"};
    state.roster[3].vitals = {0, true, "SRD1 0 0 0 3 0"};
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
    check(result && result->members == std::vector<MemberId>{f, npc} && !result->spending &&
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
    for (auto id : {w, dead, reserve})
        check(party.member(id).vitals == state.roster[id - 1].vitals &&
                  party.member(id).last_rest_minutes == state.roster[id - 1].last_rest_minutes,
              "Ineligible and reserve members gain no rest resources or cooldown reset");
    check(party.member(unconscious).vitals.dead &&
              party.member(unconscious).vitals.resources == "SRD1 0 0 1 4 0" &&
              !party.member(unconscious).last_rest_minutes,
          "Ineligible mortality continues without replenishing resources or recording a rest");
    check(
        party.rest_info(RestKind::long_rest)[1].denial == RestDenial::none,
        "Eligibility is checked at the next rest start, not granted midway through the previous rest");
    auto copy = loaded(saved(party));
    const auto later = copy.rest(RestKind::long_rest);
    check(later && later->members == std::vector<MemberId>{w} &&
              copy.member(f).last_rest_minutes == 1480,
          "Mixed cooldowns persist across load");
    copy.keep_rest_spells(w);
    copy.remove(f);
    copy.advance_time_milliseconds(480ULL * 60000 - 1);
    copy.rejoin(f);
    auto status = copy.rest_info(RestKind::long_rest);
    const auto found = std::find_if(status.begin(), status.end(),
                                    [&](const auto &i)
                                    {
                                        return i.id == f;
                                    });
    check(found != status.end() && found->wait_milliseconds == 1,
          "Removal/rejoin preserves the cooldown down to one millisecond");
    copy.advance_time_milliseconds(1);
    status = copy.rest_info(RestKind::long_rest);
    check(std::find_if(
              status.begin(), status.end(),
              [&](const auto &i)
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
        m.vitals = {1, false, "SRD1 0 0 0 0 0"};
    party.restore(state);
    rejects(
        [&]
        {
            (void)party.spend_hit_die({1, 1}, f);
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
            (void)party.spend_hit_die(ticket, reserve);
        });
    rejects(
        [&]
        {
            (void)party.spend_hit_die(ticket, 999);
        });
    rejects(
        [&]
        {
            party.remove(f);
        });
    rejects(
        [&]
        {
            party.complete_training(f, *srd5::character_rules(),
                                    {{"origin:languages", {"elvish", "orc"}}});
        });
    rejects(
        [&]
        {
            (void)party.rest();
        });
    check(
        saved(party) == after_rest,
        "Invalid targets, edits and repeated rests preserve the completed hour, resources and RNG");
    const auto first = party.spend_hit_die(ticket, f);
    check(first.roll == 4 && first.modifier == 2 && first.healing == 6 && first.remaining == 3 &&
              party.member(f).vitals.hit_points == 7 &&
              party.state().random_state == 11400714819323198527ULL,
          "First die commits its known roll, healing and one RNG draw");
    const auto after_first = saved(party);
    rejects(
        [&]
        {
            (void)party.spend_hit_die(ticket, f);
        });
    rejects(
        [&]
        {
            party.finish_short_rest(ticket);
        });
    check(saved(party) == after_first, "Duplicate roll and stale Finish callbacks are atomic");
    auto copy = loaded(after_first);
    check(saved(copy) == after_first,
          "Save/load retains the exact spending window and expenditure");
    rejects(
        [&]
        {
            (void)copy.spend_hit_die(ticket, f);
        });
    const auto next = copy.state().short_rest->ticket;
    const auto second = copy.spend_hit_die(next, f);
    const auto same = party.spend_hit_die(next, f);
    check(
        second.roll == 2 && second.healing == 4 && second.remaining == 2 &&
            same.roll == second.roll && saved(copy) == saved(party),
        "A new decision after reload consumes the known next die without repeating recharge or elapsed time");
    const auto finish = copy.state().short_rest->ticket;
    copy.finish_short_rest(finish);
    check(!copy.state().short_rest && copy.member(f).vitals.hit_points == 11 &&
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
            (void)copy.spend_hit_die(finish, f);
        });
    check(saved(copy) == finished, "Finished sessions cannot be reused");
    party.finish_short_rest(party.state().short_rest->ticket);
    const auto zero = copy.rest(RestKind::short_rest);
    copy.finish_short_rest(*zero->spending);
    check(copy.member(f).vitals.hit_points == 11 && winds(copy.member(f)) == 2 &&
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
                                      [](const auto &c)
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
    const auto last = one_die.spend_hit_die(*completed->spending, one);
    check(last.remaining == 0, "The only Hit Die can be spent at full HP");
    const auto depleted = saved(one_die);
    rejects(
        [&]
        {
            (void)one_die.spend_hit_die(one_die.state().short_rest->ticket, one);
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
            (void)party.spend_hit_die(ticket, id);
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
    const auto combat = module()->create({{8, 8, std::vector<std::uint8_t>(64)}, actors}, 42);
    party.begin_combat();
    party.apply_combat(combat->snapshot());
    check(!party.state().short_rest, "Successful initiative closes the spending window");
    rejects(
        [&]
        {
            (void)party.spend_hit_die(*rest->spending, id);
        });
    rejects(
        [&]
        {
            (void)party.rest();
        });
    party.end_combat();
    for (const bool short_rest : {false, true})
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
    state.roster[0].vitals = {0, false, "SRD1 0 0 1 2 0"};
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
        m.vitals = {1, false, "SRD3 0 0 0 0 0 0 FX1 2 1 1 1 77 99 \"Source caster\" 13 43000 2000"};
    for (const auto kind : {RestKind::short_rest, RestKind::long_rest})
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
            check(m.vitals.resources.ends_with("FX1 2 0"),
                  "Timed effects expire and safe rest completion stands able sleepers");
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
    state.roster[0].vitals = {1, false, "SRD1 0 0 0 0 0"};
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
    const auto die = party->spend_hit_die(party->state().short_rest->ticket, id);
    check(die.healing == 6, "Campaign service permits one committed die");
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
    check(resumed->member(id).vitals.hit_points == 7 && disk.town->script_variable(0x6c19) == 7,
          "Next script cannot overwrite committed Hit Die healing with stale HP");
    (void)resumed->begin_rest(RestKind::long_rest);
    for (unsigned phase = 0; phase < 2; ++phase)
    {
        const auto before = encode_campaign(*resumed, &*disk.town, "campaign-rest");
        check(!disk.town->explore(por::ExplorationCommand::turn_left) &&
                  !disk.town->explore(por::ExplorationCommand::forward) &&
                  !disk.town->explore(por::ExplorationCommand::look) &&
                  !disk.town->camp(RestKind::long_rest),
              "Active or interrupted rest blocks unrelated campaign events");
        check(encode_campaign(*resumed, &*disk.town, "campaign-rest") == before,
              "Blocked exploration preserves party and town state");
        if (!phase)
            resumed->interrupt_rest(resumed->state().rest_activity->ticket,
                                    RestInterruption::initiative);
    }
    resumed->abandon_rest(resumed->state().rest_activity->ticket);
    for (const auto kind : {RestKind::short_rest, RestKind::long_rest})
        for (const auto chance : {255u, 50u, 100u, 101u, 102u, 200u, 254u})
        {
            party->restore(state);
            auto script = program({9, 0, 1, 1, 0xd2, 0x6d, 9, 0, static_cast<std::uint8_t>(chance),
                                   1, 0xd3, 0x6d, 0});
            por::RolfTourSession blocked({}, script, {}, 0x9914, {}, resources);
            blocked.campaign_party(party);
            settle(blocked);
            check(blocked.camp(kind), "Both kinds enter the original camp checks");
            settle(blocked);
            auto expected = state.roster[0].vitals;
            if (chance == 100 || chance == 101)
            {
                module()->set_rest_work(expected, state.roster[0].character.sheet(),
                                        RestWork::light_activity);
                (void)module()->recover_at_safety(expected, state.roster[0].character.sheet(), {});
            }
            check(
                !party->state().short_rest && party->member(id).vitals == expected &&
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
void watch_equipment_and_rollback()
{
    for (const bool failure : {false, true})
    {
        auto party = std::make_shared<CampaignParty>(module());
        auto person = hero();
        const auto sword = person.inventory().add("longsword", "Watch camp sword", 1, 34);
        const auto owner = party->add_pc(std::move(person));
        party->equip(owner, sword);
        auto initial = party->checkpoint();
        initial.roster[0].vitals = {1, false, "SRD1 0 0 0 0 0"};
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
                  "Failed watch continuation rolls back sleep, equipment, time and RNG");
            continue;
        }
        check(party->state().time_minutes == 5 && !party->state().rest_activity &&
                  !party->state().short_rest && party->state().detached_items.empty(),
              "Obeying watch safely ends the five-minute camp");
        const auto &member = party->member(owner);
        const auto items = member.character.inventory().items();
        check(member.equipped.empty() && items.size() == 1 &&
                  items.front().name == "Watch camp sword" && items.front().id != sword,
              "Watch route really drops and recollects the same equipment into inventory");
        check(member.vitals.hit_points == 1 && winds(member) == 0 &&
                  party->state().random_state == 42,
              "Watch wake and collection grant no recovery or RNG draws");
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
    return "OPENGOLD-CAMPAIGN 11\n" + std::to_string(hash) + '\n' + body;
}
void resumption_services()
{
    for (const auto chance : {0u, 255u, 50u, 100u})
    {
        auto party = std::make_shared<CampaignParty>(module());
        party->add_pc(hero());
        auto script = program(
            {9, 0, 1, 1, 0xd2, 0x6d, 9, 0, static_cast<std::uint8_t>(chance), 1, 0xd3, 0x6d, 0});
        auto resources = std::make_shared<por::PhlanResources>();
        resources->programs[0] = script;
        por::RolfTourSession town({}, script, {}, 0x9914, {}, resources);
        town.campaign_party(party);
        settle(town);
        const auto start = party->begin_rest(RestKind::long_rest);
        (void)party->advance_rest(*start, 70 * 60000, RestWork::sleep);
        party->interrupt_rest(party->state().rest_activity->ticket, RestInterruption::initiative);
        check(!town.resume_camp(), "Pending dice prevent camp resumption");
        party->finish_short_rest(party->state().short_rest->ticket);
        const auto before = saved(*party);
        check(town.resume_camp(), "Resumption enters original pre-camp checks");
        settle(town);
        if (chance == 0)
        {
            check(!party->state().rest_activity && party->state().time_minutes == 540 &&
                      party->member(1).last_rest_minutes == 540,
                  "Safe resumption completes retained progress plus one hour exactly");
        }
        else if (chance == 100)
        {
            check(
                !party->state().rest_activity && party->state().time_minutes == 75 &&
                    !party->state().short_rest,
                "City watch ends resumed camping after a fresh five minutes without granting recovery");
        }
        else
            check(saved(*party) == before,
                  "Forbidden and unsupported resumption preserve all rest state and RNG");
    }
}
void malformed_continuation()
{
    CampaignParty party(module());
    const auto id = party.add_pc(hero());
    (void)party.rest(RestKind::short_rest);
    const auto good = saved(party);
    auto body = good.substr(good.find('\n', good.find('\n') + 1) + 1);
    const std::string tail = "3 1 2 1 60 0 1 1 ";
    check(body.ends_with(tail), "Independent fixture locates the version-ten continuation");
    const auto prefix = body.substr(0, body.size() - tail.size());
    for (const auto bad : {"0 0 ", "1 1 1 1 60 0 1 1 ", "2 1 0 1 60 0 1 1 ", "2 1 1 0 60 0 1 1 ",
                           "2 1 1 1 59 0 1 1 ", "2 1 1 1 60 1 1 1 ", "2 1 1 1 60 0 0 ",
                           "2 1 1 1 60 0 2 1 1 ", "2 1 1 1 60 0 1 999 "})
        rejects(
            [&]
            {
                (void)loaded(payload(prefix + bad));
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
            (void)party.spend_hit_die(party.state().short_rest->ticket, id);
        });
    check(saved(party) == exhausted, "Revision exhaustion cannot consume a die or RNG");
    party.finish_short_rest(party.state().short_rest->ticket);
}
#include "rest_activity_checks.h"
} // namespace
int main(int argc, char **argv)
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--freeze-rest-activity")
        {
            freeze_activity_baseline();
            return 0;
        }
        alternate_rules_boundary();
        rest_activity_checks::run();
        individual_eligibility();
        spending_and_continuation();
        expiry_and_atomicity();
        effects_once();
        campaign_services();
        watch_equipment_and_rollback();
        resumption_services();
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
