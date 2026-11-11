#include "party_test_bridge.h"
#include "campaign_combat.h"
#include "character_sheet_text.h"
#include "class_text.h"
#include "combat_view.h"
#include "game_encounters.h"
#include "game_monsters.h"
#include "game_resources.h"
#include "game_wall_art.h"
#include "godot_nodes.h"
#include "godot_path.h"
#include "level_up_dialog.h"
#include "seeded_battlefield.h"
#include "opengold/character_art.h"
#include "opengold/creature_art.h"
#include "opengold/character_pool.h"
#include "opengold/combat_body_catalog.h"
#include "opengold/encounter_budget.h"
#include "opengold/srd5.h"
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

using namespace godot;
using namespace opengold;

namespace
{
// The game places a fight's two sides one exploration square apart, as most of
// the original's scripted fights do.
constexpr unsigned fight_distance = 1;
constexpr int highest_level = 15;
constexpr int largest_party = 8;

String gs(const std::string &text)
{
    return String::utf8(text.c_str());
}

std::filesystem::path game_dir()
{
    const auto value = OS::get_singleton()->get_environment("OPENGOLD_GAME_DIR");
    if (value.is_empty())
    {
        throw std::runtime_error("Set OPENGOLD_GAME_DIR to the game files");
    }
    return presentation::path_from_godot(value);
}

std::unique_ptr<rules::RulesModule> game_rules()
{
    return srd5::load(presentation::path_from_godot(game_rules_file()));
}

String choice_label(const std::vector<rules::CreationChoice> &choices, const std::string &id)
{
    const auto found = std::find_if(choices.begin(), choices.end(),
                                    [&](const auto &choice)
                                    {
                                        return choice.id == id;
                                    });
    return found == choices.end() ? gs(id) : gs(found->label);
}

CombatView *fight_view(Node &host)
{
    return Object::cast_to<CombatView>(host.get_node_or_null("CampaignCombat"));
}

// The combat icons LOAD MONSTERs draw each creature record with, keyed by
// (archive bank, record), in the order the scripts name them: every area
// script of a bank loads the same MON<bank>CHA records. A record loaded only
// from a variable has none.
std::map<std::pair<unsigned, unsigned>, std::vector<unsigned>>
loaded_icons(const por::PhlanResources &town)
{
    std::map<std::pair<unsigned, unsigned>, std::vector<unsigned>> icons;
    for (const auto &[script, program] : town.programs)
    {
        const auto bank = town.script_banks.find(script);
        if (bank == town.script_banks.end())
        {
            continue;
        }
        for (const auto &instruction : por::walk_reachable(*program).instructions)
        {
            const auto load = por::written_load_monster(instruction);
            if (!load || !load->icon)
            {
                continue;
            }
            auto &known = icons[{bank->second, load->record}];
            if (std::find(known.begin(), known.end(), *load->icon) == known.end())
            {
                known.push_back(*load->icon);
            }
        }
    }
    return icons;
}

// "Area" for an area of one map; "Area, map N" when it has several.
String area_label(const std::string &name, unsigned map, bool several_maps)
{
    return several_maps ? gs(name + ", map " + std::to_string(map)) : gs(name);
}
} // namespace

void PartyTestBridge::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("catalog"), &PartyTestBridge::catalog);
    ClassDB::bind_method(D_METHOD("build_party", "pool_indices"), &PartyTestBridge::build_party);
    ClassDB::bind_method(D_METHOD("members"), &PartyTestBridge::members);
    ClassDB::bind_method(D_METHOD("classes"), &PartyTestBridge::classes);
    ClassDB::bind_method(D_METHOD("set_level", "level"), &PartyTestBridge::set_level);
    ClassDB::bind_method(D_METHOD("open_level_up"), &PartyTestBridge::open_level_up);
    ClassDB::bind_method(D_METHOD("advance_with_defaults"),
                         &PartyTestBridge::advance_with_defaults);
    ClassDB::bind_method(D_METHOD("sheet", "index"), &PartyTestBridge::sheet);
    ClassDB::bind_method(D_METHOD("start_fight", "configuration"), &PartyTestBridge::start_fight);
    ClassDB::bind_method(D_METHOD("outcome"), &PartyTestBridge::outcome);
    ClassDB::bind_method(D_METHOD("leave"), &PartyTestBridge::leave);
    ADD_SIGNAL(MethodInfo("member_advanced"));
    ADD_SIGNAL(MethodInfo("failed", PropertyInfo(Variant::STRING, "message")));
}

void PartyTestBridge::load_game_data()
{
    const auto dir = game_dir();
    const auto creation = srd5::character_rules();
    pool_ = character_pool(*creation, por::CharacterArt::load(dir));
    const auto &encounters = game_encounters();
    town_ =
        por::RolfTourSession::load(dir, encounters, game_wall_art(), game_monsters()).resources();
    // Valjevo Castle's maps have scripts of their own under one name, so maps
    // are counted by the name they are listed under.
    std::map<std::string, unsigned> maps_per_name;
    for (const auto &[id, district] : town_->districts)
    {
        if (const auto *area = encounters.area(district->script); area && district->map)
        {
            ++maps_per_name[area->name];
        }
    }
    const auto icons = loaded_icons(*town_);
    // The maps of one bank share its creatures; each is listed once.
    std::set<std::pair<unsigned, unsigned>> listed;
    for (const auto &[id, district] : town_->districts)
    {
        const auto *area = encounters.area(district->script);
        if (!area || !district->map)
        {
            continue;
        }
        areas_.push_back({id, area_label(area->name, id, maps_per_name[area->name] > 1)});
        for (const auto &[record, conversion] : district->conversions)
        {
            if (!district->encounter_creatures.contains(record) ||
                !listed.insert({district->bank, record}).second)
            {
                continue;
            }
            // A creature only loaded from a variable, as wandering groups are,
            // has no icon a script names: the original's icons follow their
            // records, so it is drawn with its record's icon when there is one.
            const auto found = icons.find({district->bank, record});
            const auto candidates = found == icons.end() || found->second.empty()
                                        ? std::vector<unsigned>{record}
                                        : found->second;
            const auto *exported =
                town_->monsters ? town_->monsters->creature(district->bank, record) : nullptr;
            const auto name = area->name + ": " + por::creature_name(*district, record, exported);
            for (const auto icon : candidates)
            {
                if (!por::creature_art(district->combat_archive, icon, exported, false))
                {
                    left_out_.push_back(gs(name));
                    continue;
                }
                creatures_.push_back(
                    {id, record, icon,
                     gs(candidates.size() > 1 ? name + " (icon " + std::to_string(icon) + ")"
                                              : name)});
            }
        }
    }
    auto dialog = LevelUpDialog::create();
    auto *level_up = Object::cast_to<LevelUpDialog>(dialog.get());
    level_up->connect_host(
        [this]() -> CampaignParty &
        {
            return party();
        },
        [this]
        {
            member_advanced();
        },
        [this](const std::exception &failure)
        {
            emit_signal("failed", gs(failure.what()));
        });
    presentation::attach_child(*this, std::move(dialog));
}

Dictionary PartyTestBridge::catalog()
{
    Dictionary result;
    try
    {
        if (!town_)
        {
            load_game_data();
        }
        const auto creation = srd5::character_rules();
        const auto classes = creation->choices(rules::CreationField::character_class);
        const auto species = creation->choices(rules::CreationField::race);
        Array pool, areas, creatures;
        for (const auto &character : pool_)
        {
            const auto &draft = character.creation_data();
            pool.push_back(gs(character.sheet().name) + " - " + choice_label(species, draft.race) +
                           " " + choice_label(classes, draft.character_class));
        }
        for (std::size_t n = 0; n < areas_.size(); ++n)
        {
            areas.push_back(areas_[n].label);
        }
        for (std::size_t n = 0; n < creatures_.size(); ++n)
        {
            creatures.push_back(creatures_[n].label);
        }
        Array difficulties;
        for (const auto name : difficulty_names)
        {
            difficulties.push_back(gs(std::string(name)));
        }
        result["pool"] = pool;
        result["areas"] = areas;
        result["creatures"] = creatures;
        result["difficulties"] = difficulties;
        Array left_out;
        for (const auto &label : left_out_)
        {
            left_out.push_back(label);
        }
        result["left_out"] = left_out;
    }
    catch (const std::exception &e)
    {
        result["error"] = gs(e.what());
    }
    return result;
}

CampaignParty &PartyTestBridge::party() const
{
    if (!party_)
    {
        throw std::runtime_error("Build a party first");
    }
    return *party_;
}

String PartyTestBridge::build_party(const Array &pool_indices)
{
    try
    {
        if (pool_indices.is_empty() || pool_indices.size() > largest_party)
        {
            throw std::runtime_error("Choose one to eight pool characters");
        }
        auto next = std::make_shared<CampaignParty>(game_rules());
        next->open_npc_positions_to_player_characters();
        for (const Variant &value : pool_indices)
        {
            const int index = value;
            if (index < 0 || static_cast<std::size_t>(index) >= pool_.size())
            {
                throw std::runtime_error("Unknown pool character");
            }
            const auto id = add_pool_member(*next, pool_[static_cast<std::size_t>(index)],
                                            static_cast<unsigned>(index));
            // Pool characters join with nothing; a fight needs their class's kit.
            outfit_pool_member(*next, id);
        }
        party_ = std::move(next);
        return {};
    }
    catch (const std::exception &e)
    {
        return gs(e.what());
    }
}

Array PartyTestBridge::members() const
{
    Array result;
    if (!party_)
    {
        return result;
    }
    for (const auto id : party_->state().slots)
    {
        if (!id)
        {
            continue;
        }
        const auto &member = party_->member(id);
        const auto &sheet = member.character.sheet();
        Dictionary row;
        row["name"] = gs(sheet.name);
        row["classes"] = i18n::class_summary(sheet);
        row["level"] = static_cast<int>(sheet.level);
        row["hit_points"] = member.vitals.hit_points;
        row["maximum_hit_points"] = sheet.hit_points;
        row["experience"] = static_cast<std::int64_t>(member.experience);
        row["can_advance"] = party_->can_advance(id);
        Array class_ids, scores, saves, proficient;
        for (const auto &class_id : sheet.level_classes)
        {
            class_ids.push_back(gs(class_id));
        }
        for (const auto ability : rules::all_abilities)
        {
            scores.push_back(sheet.scores[ability]);
            saves.push_back(sheet.saving_throws[ability]);
            proficient.push_back(sheet.save_proficiencies[ability]);
        }
        row["class_ids"] = class_ids;
        row["race"] = gs(sheet.race);
        row["scores"] = scores;
        row["saving_throws"] = saves;
        row["save_proficiencies"] = proficient;
        const auto recovery = party_->recovery_info(id);
        row["hit_dice"] = recovery.hit_dice_max;
        Dictionary resources;
        for (const auto &pool : recovery.resources)
        {
            resources[gs(pool.id)] = pool.capacity;
        }
        row["resources"] = resources;
        result.push_back(row);
    }
    return result;
}

Array PartyTestBridge::classes() const
{
    const auto creation = srd5::character_rules();
    Array result;
    for (const auto &choice : creation->choices(rules::CreationField::character_class))
    {
        const auto requirements = creation->class_requirements(choice.id);
        Array abilities;
        for (const auto ability : requirements.abilities)
        {
            abilities.push_back(static_cast<int>(
                std::find(rules::all_abilities.begin(), rules::all_abilities.end(), ability) -
                rules::all_abilities.begin()));
        }
        Dictionary row;
        row["id"] = gs(choice.id);
        row["label"] = i18n::class_label(choice.id);
        row["abilities"] = abilities;
        row["any"] = requirements.any;
        result.push_back(row);
    }
    return result;
}

String PartyTestBridge::set_level(int level)
{
    try
    {
        if (level < 1 || level > highest_level)
        {
            throw std::runtime_error("Choose a level from 1 to 15");
        }
        auto state = party().checkpoint();
        for (const auto id : state.slots)
        {
            if (!id)
            {
                continue;
            }
            auto &member = *std::find_if(state.roster.begin(), state.roster.end(),
                                         [id](const auto &m)
                                         {
                                             return m.id == id;
                                         });
            if (member.character.sheet().level > static_cast<unsigned>(level))
            {
                throw std::runtime_error("The party is above that level; build it again first");
            }
            member.experience = std::max<std::uint64_t>(
                member.experience,
                party().rule_module().experience_for_level(static_cast<unsigned>(level)));
        }
        party().restore(std::move(state));
        return {};
    }
    catch (const std::exception &e)
    {
        return gs(e.what());
    }
}

String PartyTestBridge::open_level_up()
{
    for (const auto id : party().state().slots)
    {
        if (id && party().can_advance(id))
        {
            Object::cast_to<LevelUpDialog>(get_node_or_null("LevelUp"))->open(id);
            return gs(party().member(id).character.sheet().name);
        }
    }
    return {};
}

String PartyTestBridge::advance_with_defaults()
{
    try
    {
        if (auto *dialog = Object::cast_to<Window>(get_node_or_null("LevelUp")))
        {
            dialog->hide();
        }
        for (const auto id : party().state().slots)
        {
            while (id && party().can_advance(id))
            {
                party().advance(id, party().default_advancement(id));
            }
        }
        return {};
    }
    catch (const std::exception &e)
    {
        return gs(e.what());
    }
}

void PartyTestBridge::member_advanced()
{
    emit_signal("member_advanced");
}

String PartyTestBridge::sheet(int index) const
{
    std::vector<MemberId> active;
    for (const auto id : party().state().slots)
    {
        if (id)
        {
            active.push_back(id);
        }
    }
    if (index < 0 || static_cast<std::size_t>(index) >= active.size())
    {
        return {};
    }
    return presentation::sheet_text(party(),
                                    party().member(active[static_cast<std::size_t>(index)]));
}

String PartyTestBridge::start_fight(const Dictionary &configuration)
{
    try
    {
        if (fight_view(*get_parent()))
        {
            throw std::runtime_error("Finish the current fight first");
        }
        const int area_index = configuration.get("area", -1);
        if (area_index < 0 || static_cast<std::size_t>(area_index) >= areas_.size())
        {
            throw std::runtime_error("Choose an area");
        }
        const auto &area =
            *town_->districts.at(areas_[static_cast<std::size_t>(area_index)].district);
        const auto difficulty =
            parse_difficulty(String(configuration.get("difficulty", "normal")).utf8().get_data());
        if (!difficulty)
        {
            throw std::runtime_error("Unknown difficulty");
        }
        const Array groups = configuration.get("groups", Array());
        if (groups.is_empty())
        {
            throw std::runtime_error("Add at least one monster or NPC");
        }
        CampaignEncounter encounter;
        std::vector<rules::Footprint> footprints;
        for (const Variant &entry : groups)
        {
            const Dictionary group = entry;
            const int creature_index = group.get("creature", -1);
            const int count = group.get("count", 0);
            if (creature_index < 0 ||
                static_cast<std::size_t>(creature_index) >= creatures_.size() || count < 1)
            {
                throw std::runtime_error("Invalid monster group");
            }
            const auto &choice = creatures_[static_cast<std::size_t>(creature_index)];
            const auto &home = *town_->districts.at(choice.district);
            const auto *exported =
                town_->monsters ? town_->monsters->creature(home.bank, choice.record) : nullptr;
            const bool ally = por::fights_for_party(home, choice.record);
            const auto name = por::creature_name(home, choice.record, exported);
            for (int n = 0; n < count; ++n)
            {
                if (encounter.enemies.size() + encounter.allies.size() >= por::fight_monster_limit)
                {
                    throw std::runtime_error("A fight holds at most 56 creatures");
                }
                const auto id =
                    static_cast<rules::EntityId>(ally ? first_ally_id + encounter.allies.size()
                                                      : 1000 + encounter.enemies.size());
                auto staged = por::stage_area_creature(
                    home, choice.record, choice.icon, exported, id,
                    ally ? name : name + " " + std::to_string(encounter.enemies.size() + 1),
                    ally ? rules::Side::party : rules::Side::opposition, *difficulty);
                if (!ally)
                {
                    footprints.push_back(staged.fighter.footprint);
                }
                (ally ? encounter.allies : encounter.enemies).push_back(std::move(staged.fighter));
                encounter.art.push_back(std::move(staged.art));
            }
        }
        if (encounter.enemies.empty())
        {
            throw std::runtime_error("Add a monster or NPC that fights the party");
        }
        auto fighters = std::make_shared<CampaignParty>(game_rules());
        fighters->open_npc_positions_to_player_characters();
        fighters->restore(party().checkpoint());
        const auto battlefield = seeded_battlefield(
            *area.map, static_cast<std::uint64_t>(std::int64_t(configuration.get("seed", 1))),
            fight_distance, fighters->participants().size(), footprints);
        if (!battlefield)
        {
            throw std::runtime_error("No square of this map fits the fight");
        }
        encounter.field = battlefield->field;
        encounter.facing = battlefield->facing;
        encounter.distance = fight_distance;
        encounter.terrain_art = area.terrain_art;
        encounter.daylight = true;
        std::vector<unsigned> levels;
        for (const auto id : fighters->state().slots)
        {
            if (id)
            {
                levels.push_back(fighters->member(id).character.sheet().level);
            }
        }
        apply_difficulty(encounter, *difficulty, average_party_level(levels));
        const auto dir = game_dir();
        const auto body =
            por::CombatBodyCatalog::load(presentation::path_from_godot(game_combat_body_file()),
                                         presentation::path_from_godot(game_combat_weapon_file()));
        presentation::attach_child(*get_parent(),
                                   campaign_combat_scene(fighters, por::CharacterArt::load(dir),
                                                         &body, std::move(encounter)));
        return {};
    }
    catch (const std::exception &e)
    {
        return gs(e.what());
    }
}

String PartyTestBridge::outcome() const
{
    const auto *combat = fight_view(*get_parent());
    if (!combat)
    {
        return {};
    }
    const auto result = combat->completed_outcome();
    if (!result)
    {
        return {};
    }
    return result->outcome == rules::Outcome::victory  ? "Victory"
           : result->outcome == rules::Outcome::defeat ? "Defeat"
                                                       : "Finished";
}

void PartyTestBridge::leave()
{
    if (auto *combat = fight_view(*get_parent()))
    {
        get_parent()->remove_child(combat);
        combat->queue_free();
    }
}
