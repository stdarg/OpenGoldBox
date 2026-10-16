#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/character_pool.h"
#include "opengold/combat_demo.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

// Each class's final integration check (the SRD plan's "level-four
// integration" issues): a created character meets every level from 1 to 4 the
// way a player does. At each level it fights, the fight's wounds and spent uses
// come back to the party, the campaign is written and read again mid-adventure
// and the next fight plays out the same, it rests, and it advances with its
// class's choices.
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);
const bool verbose = std::getenv("OPENGOLD_ATTAINED_VERBOSE") != nullptr;

void check(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::unique_ptr<RulesModule> module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

Character created(const std::string &klass)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = klass;
    d.alignment = "neutral_good";
    d.background = "soldier";
    if (klass == "fighter")
        d.training = {{"class:fighter", {"acrobatics", "history"}},
                      {"class:fighter:fighting_style", {"defense"}}};
    if (klass == "wizard")
        d.training["class:wizard"] = {"arcana", "nature"};
    d.name = "Attained " + klass;
    d.rolled = true;
    for (auto &r : d.rolls)
        r = {{6, 5, 4, 1}, 3};
    return Character(*srd5::character_rules(), d, {});
}

// A ready-made Character Pool character of the class, as a player adds one.
Character ready_made(std::string_view klass)
{
    por::CharacterArt art;
    Image head;
    head.width = 88;
    head.height = 40;
    head.rgba.assign(88 * 40 * 4, 128);
    Image body;
    body.width = 88;
    body.height = 48;
    body.rgba.assign(88 * 48 * 4, 128);
    art.set_original_head(1, por::PortraitPart{"fixture", head});
    art.set_original_body(1, por::PortraitPart{"fixture", body});
    for (const auto &preset : character_pool(*srd5::character_rules(), art))
        if (preset.creation_data().character_class == klass)
            return preset;
    throw std::runtime_error("No pool character of the class");
}

Encounter encounter(const CampaignParty &party)
{
    Encounter e{{12, 8, std::vector<Terrain>(96)}, party.participants()};
    e.participants.push_back({99, "bandit", "Bandit", Side::opposition, {10, 6}});
    return e;
}

std::vector<ResourcePool> pools(const CampaignParty &party, MemberId id)
{
    const auto &m = party.member(id);
    return party.rule_module().recovery_info(m.character.sheet(), m.vitals).resources;
}

unsigned spent(const CampaignParty &party, MemberId id)
{
    unsigned total = 0;
    for (const auto &pool : pools(party, id))
        total += pool.capacity - pool.remaining;
    return total;
}

bool has_grant(const CampaignParty &party, MemberId id, std::string_view grant)
{
    const auto &grants = party.member(id).character.sheet().grants;
    return std::any_of(grants.begin(), grants.end(),
                       [&](const auto &g)
                       {
                           return g.id == grant;
                       });
}

// Plays a fight: the member uses the first offered class verb, then the demo
// AI plays both sides to the end.
std::unique_ptr<CombatSession> fight(CampaignParty &party, std::span<const std::string> verbs,
                                     std::uint64_t seed, const std::string &label)
{
    auto combat = party.rule_module().create(encounter(party), seed);
    bool used = false;
    for (int step = 0; step < 200; ++step)
    {
        const auto now = combat->snapshot();
        if (now.outcome != Outcome::ongoing)
            break;
        const auto legal_commands = combat->legal_commands();
        auto command = choose_demo_command(now, legal_commands);
        if (!used)
            for (const auto &legal : legal_commands)
                if (std::find(verbs.begin(), verbs.end(), legal.verb) != verbs.end())
                {
                    command = legal;
                    used = true;
                    break;
                }
        if (verbose && step < 12)
        {
            std::cerr << "  verbs:";
            for (const auto &legal : legal_commands)
                std::cerr << ' ' << legal.verb;
            std::cerr << "\n  -> " << command.verb << '\n';
        }
        check(combat->submit(command), label + ": the fight accepts a legal command");
    }
    check(used || verbs.empty(), label + ": the class's own action is offered in the fight");
    check(combat->snapshot().outcome != Outcome::ongoing, label + ": the fight ends");
    return combat;
}

CampaignParty reloaded(const CampaignParty &party, std::string_view label)
{
    const auto bytes = encode_campaign(party, nullptr, "attained");
    const auto rules = module();
    CampaignParty copy(module());
    copy.restore(
        decode_campaign(bytes, *srd5::character_rules(), *rules, "attained", nullptr).party);
    check(encode_campaign(copy, nullptr, "attained") == bytes,
          std::string(label) + ": the campaign reads back exactly");
    return copy;
}

struct LevelPlan
{
    std::vector<std::string> verbs;  // A class action to use in this level's fight.
    std::vector<std::string> grants; // Grants the character holds at this level.
};

void attained_levels(const std::string &klass, const std::vector<LevelPlan> &plan,
                     bool pool = false)
{
    CampaignParty party(module());
    const auto id = party.add_pc(pool ? ready_made(klass) : created(klass));
    if (pool)
        outfit_pool_member(party, id);
    for (unsigned level = 1; level <= 4; ++level)
    {
        const auto label = klass + " level " + std::to_string(level);
        const auto &step = plan.at(level - 1);
        check(party.member(id).character.sheet().level == static_cast<int>(level),
              label + ": the character is at this level");
        for (const auto &grant : step.grants)
            check(has_grant(party, id, grant), label + ": holds " + grant);
        if (verbose)
        {
            std::cerr << label << " grants:";
            for (const auto &g : party.member(id).character.sheet().grants)
                std::cerr << ' ' << g.id;
            std::cerr << "\n  pools:";
            for (const auto &pool : pools(party, id))
                std::cerr << ' ' << pool.id << ' ' << pool.remaining << '/' << pool.capacity;
            std::cerr << '\n';
        }

        // The fight's wounds and spent uses come back to the party.
        auto combat = fight(party, step.verbs, 40 + level, label);
        party.begin_combat();
        party.apply_combat(combat->snapshot());
        party.end_combat();
        // A Rogue's actions spend nothing; every other class's do.
        check(step.verbs.empty() || pools(party, id).empty() || spent(party, id) > 0,
              label + ": the class action's use stays spent after the fight");

        // A save mid-adventure reads back, and the next fight plays out the same.
        auto copy = reloaded(party, label);
        check(spent(copy, id) == spent(party, id), label + ": spent uses survive the reload");
        auto first = party.rule_module().create(encounter(party), 7),
             second = copy.rule_module().create(encounter(copy), 7);
        for (int i = 0; i < 40 && first->snapshot().outcome == Outcome::ongoing; ++i)
        {
            const auto command = choose_demo_command(*first);
            check(first->submit(command) && second->submit(command) &&
                      first->save() == second->save(),
                  label + ": the next fight continues identically after reload");
        }

        // A Short Rest, then a Long Rest a day later, restore the character.
        auto state = party.checkpoint();
        if (state.roster.front().vitals.hit_points <= 0)
            party.rule_module().set_hit_points(state.roster.front().vitals,
                                               state.roster.front().character.sheet(), 1);
        party.restore(std::move(state));
        const auto before_rest = pools(party, id);
        check(bool(party.rest(RestKind::short_rest)), label + ": a Short Rest completes");
        for (const auto &pool : before_rest)
            if (pool.short_rest_recovery && pool.remaining < pool.capacity)
                for (const auto &after : pools(party, id))
                    if (after.id == pool.id)
                        check(after.remaining > pool.remaining,
                              label + ": a Short Rest recovers " + pool.id);
        if (party.state().short_rest)
            party.finish_short_rest(party.state().short_rest->ticket);
        // A Hunter Ranger may change its Hunter's Prey after a Short Rest too.
        if (party.state().training_rest)
            party.keep_rest_training(party.state().training_rest->ticket, id);
        party.advance_time(std::chrono::hours{24});
        check(bool(party.rest(RestKind::long_rest)), label + ": a Long Rest completes");
        if (party.state().spell_rest)
            party.keep_rest_spells(id);
        if (party.state().training_rest)
            party.keep_rest_training(party.state().training_rest->ticket, id);
        check(spent(party, id) == 0 &&
                  party.member(id).vitals.hit_points == party.hit_point_maximum(id),
              label + ": the Long Rest restores every use and Hit Point");
        (void)reloaded(party, label + " rested");

        if (level == 4)
            break;
        party.award_experience(party.rule_module().experience_for_level(level + 1) -
                                   party.member(id).experience,
                               "attained:" + klass + ":" + std::to_string(level));
        party.advance(id, party.default_advancement(id));
    }
}
} // namespace

int main()
{
    try
    {
        // Explores one class: prints what it holds and is offered at each level.
        if (const auto *only = std::getenv("OPENGOLD_ATTAINED_CLASS"))
        {
            attained_levels(only, std::vector<LevelPlan>(4), true);
            std::cout << "Attained-level checks passed\n";
            return 0;
        }
        attained_levels(
            "fighter",
            {{{"second_wind"}, {"feature:second_wind", "feat:defense"}},
             {{"action_surge"}, {"feature:action_surge", "feature:tactical_mind"}},
             {{"action_surge"},
              {"subclass:champion", "feature:improved_critical", "feature:remarkable_athlete"}},
             {{"action_surge"}, {"feat:ability_score_improvement"}}});
        attained_levels("wizard",
                        {{{"magic_missile"}, {"feature:arcane_recovery", "spell:magic_missile"}},
                         {{"magic_missile"}, {"expertise:arcana"}},
                         {{"magic_missile"}, {"subclass:evoker", "feature:potent_cantrip"}},
                         {{"magic_missile"}, {"feat:ability_score_improvement"}}});
        // The other classes start as ready-made Character Pool characters with
        // their class kit.
        const std::vector<std::string> asi{"feat:ability_score_improvement"};
        attained_levels(
            "barbarian",
            {{{"rage"}, {"feature:unarmored_defense"}},
             {{"rage"}, {"feature:danger_sense", "feature:reckless_attack"}},
             {{"rage"}, {"subclass:berserker", "feature:frenzy", "feature:primal_knowledge"}},
             {{"rage"}, asi}},
            true);
        const std::vector<std::string> bard_spells{"heroism", "command_halt", "healing_word"};
        attained_levels(
            "bard",
            {{bard_spells, {"feature:bardic_inspiration", "feature:spellcasting"}},
             {bard_spells, {"feature:jack_of_all_trades"}},
             {bard_spells,
              {"subclass:lore", "feature:cutting_words", "feature:bonus_proficiencies"}},
             {bard_spells, asi}},
            true);
        const std::vector<std::string> cleric_spells{"bless", "guiding_bolt", "shield_of_faith"};
        attained_levels("cleric",
                        {{cleric_spells, {"feature:spellcasting"}},
                         {cleric_spells, {"feature:channel_divinity"}},
                         {cleric_spells,
                          {"subclass:life", "feature:disciple_of_life", "feature:preserve_life"}},
                         {cleric_spells, asi}},
                        true);
        attained_levels(
            "druid",
            {{{"longstrider", "protection_from_evil_and_good"}, {"feature:spellcasting"}},
             {{"wild_shape_wolf"}, {"feature:wild_shape"}},
             {{"wild_shape_wolf"}, {"subclass:land", "feature:circle_spells", "feature:lands_aid"}},
             {{"wild_shape_wolf"}, asi}},
            true);
        attained_levels(
            "monk",
            {{{}, {"feature:unarmored_defense"}},
             {{"flurry_of_blows"}, {"feature:monks_focus", "feature:uncanny_metabolism"}},
             {{"flurry_of_blows"},
              {"subclass:open_hand", "feature:deflect_attacks", "feature:open_hand_technique"}},
             {{"flurry_of_blows"}, asi}},
            true);
        attained_levels(
            "paladin",
            {{{"divine_smite"}, {"feature:spellcasting"}},
             {{"divine_smite", "shield_of_faith", "lay_on_hands"}, {"feature:fighting_style"}},
             {{"sacred_weapon"},
              {"subclass:devotion", "feature:channel_divinity", "feature:sacred_weapon"}},
             {{"sacred_weapon"}, asi}},
            true);
        const std::vector<std::string> mark{"hunters_mark_free", "hunters_mark"};
        attained_levels(
            "ranger",
            {{mark, {"feature:spellcasting"}},
             {mark, {"feature:fighting_style"}},
             {mark, {"subclass:hunter", "feature:hunters_lore", "prey:colossus_slayer"}},
             {mark, asi}},
            true);
        attained_levels(
            "rogue",
            {{{}, {"feature:sneak_attack"}},
             {{"cunning_dash", "cunning_disengage"}, {"feature:cunning_action"}},
             {{"steady_aim"}, {"subclass:thief", "feature:fast_hands", "feature:steady_aim"}},
             {{"steady_aim"}, asi}},
            true);
        attained_levels(
            "sorcerer",
            {{{"innate_sorcery"}, {"feature:innate_sorcery"}},
             {{"innate_sorcery"}, {"feature:font_of_magic", "feature:metamagic"}},
             {{"innate_sorcery"},
              {"subclass:draconic", "feature:draconic_resilience", "feature:draconic_spells"}},
             {{"innate_sorcery"}, asi}},
            true);
        const std::vector<std::string> warlock_spells{"protection_from_evil_and_good",
                                                      "expeditious_retreat", "hold_person"};
        attained_levels("warlock",
                        {{warlock_spells, {"feature:pact_magic", "spell:eldritch_blast"}},
                         {warlock_spells, {"feature:magical_cunning"}},
                         {warlock_spells,
                          {"subclass:fiend", "feature:dark_ones_blessing", "feature:fiend_spells"}},
                         {warlock_spells, asi}},
                        true);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Attained-level checks passed\n";
}
