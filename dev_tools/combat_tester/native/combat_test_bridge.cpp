#include "combat_test_bridge.h"
#include "combat_view.h"
#include "godot_nodes.h"
#include "godot_path.h"
#include "opengold/combat_demo.h"
#include "opengold/combat_body_catalog.h"
#include "opengold/character_pool.h"
#include "opengold/authored_items.h"
#include "opengold/combat_body_catalog.h"
#include "opengold/creature_catalog.h"
#include "opengold/encounter_formation.h"
#include "opengold/map_catalog.h"
#include "opengold/srd5.h"
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>

using namespace godot;
using namespace opengold;
using namespace opengold::por;
using namespace opengold::rules;

namespace
{
struct MonsterChoice { const char *id, *label, *definition; unsigned icon;
                       const char *leader; unsigned leader_icon; };
constexpr std::array monster_choices{
    MonsterChoice{"kobold", "Kobold", "slums-kobold", 0, "slums-kobold-leader", 1},
    MonsterChoice{"goblin", "Goblin", "slums-goblin", 2, "slums-goblin-leader", 3},
    MonsterChoice{"orc", "Orc", "slums-orc", 4, "slums-orc-leader", 5},
    MonsterChoice{"hobgoblin", "Hobgoblin", "slums-hobgoblin", 6, "", 0},
    MonsterChoice{"ogre", "Ogre", "ogre", 8, "", 0},
    MonsterChoice{"bugbear", "Bugbear", "slums-bugbear", 63, "", 0},
};
std::string utf8(const String &s) { auto b = s.utf8(); return {b.get_data(), std::size_t(b.length())}; }
String gs(const std::string &s) { return String::utf8(s.c_str()); }
std::string string_at(const Dictionary &d, const char *key, const char *fallback = "")
{ return utf8(String(d.get(key, fallback))); }
int int_at(const Dictionary &d, const char *key, int fallback)
{ return int(d.get(key, fallback)); }
std::filesystem::path resource(const char *relative)
{ return presentation::path_from_godot(ProjectSettings::get_singleton()->globalize_path(
      String("res://") + relative)); }
std::filesystem::path game_dir()
{
    const auto value = OS::get_singleton()->get_environment("OPENGOLD_GAME_DIR");
    if (value.is_empty()) throw std::runtime_error("Set OPENGOLD_GAME_DIR to the game files");
    return presentation::path_from_godot(value);
}
std::vector<std::uint8_t> archive(const std::filesystem::path &dir, std::string wanted)
{
    for (const auto &entry : std::filesystem::directory_iterator(dir))
    {
        auto name = entry.path().filename().string();
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::toupper(c); });
        if (name != wanted || !entry.is_regular_file()) continue;
        std::ifstream in(entry.path(), std::ios::binary);
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }
    throw std::runtime_error("Missing game archive " + wanted);
}
opengold::Image icon(std::span<const std::uint8_t> dax, unsigned record)
{
    auto image = decode_ega_combat_icon(dax, std::uint8_t(record), 0);
    if (!image) throw std::runtime_error("Missing original icon " + std::to_string(record));
    return std::move(image.image);
}
const MonsterChoice &monster(std::string_view id)
{
    auto it = std::find_if(monster_choices.begin(), monster_choices.end(),
                           [&](const auto &m) { return id == m.id; });
    if (it == monster_choices.end()) throw std::runtime_error("Unknown monster type");
    return *it;
}
void set_gear(CampaignParty &party, MemberId id, const Dictionary &spec)
{
    const Variant weapon_value = spec.get("weapon_types", Variant());
    const Variant gear_value = spec.get("gear_types", Variant());
    const Variant shield_value = spec.get("shield", Variant());
    const int armor_type = int_at(spec, "armor_type", -1);
    const bool replace_weapons = weapon_value.get_type() == Variant::ARRAY;
    const bool replace_gear = gear_value.get_type() == Variant::ARRAY;
    const bool replace_shield = shield_value.get_type() != Variant::NIL;
    if (!replace_weapons && !replace_gear && !replace_shield && armor_type < 0) return;
    auto state = party.checkpoint();
    auto &who = *std::find_if(state.roster.begin(), state.roster.end(),
                              [&](const auto &member) { return member.id == id; });
    std::vector<std::uint64_t> removed;
    for (const auto &item : who.character.inventory().items())
    {
        const int type = item.original_type;
        if ((replace_weapons && type > 0 && type < 50) ||
            (armor_type >= 0 && type >= 50 && type <= 58) ||
            (replace_shield && type == 59) ||
            (replace_gear && (type == original_item::flask_of_oil || type >= 200)))
            removed.push_back(item.id);
    }
    for (const auto item : removed)
    {
        who.character.remove_item(item);
        std::erase(who.equipped, item);
        who.item_sources.erase(item);
    }
    party.restore(std::move(state));
    const auto add = [&](int type, bool equip)
    {
        if (type == 0) return;
        if (type < 0 || type > 255) throw std::runtime_error("Invalid equipment type");
        Equipment item;
        item.stored.type = std::uint8_t(type);
        item.stored.stack_size = 1;
        party.purchase(id, item);
        if (equip)
        {
            const auto item_id = party.member(id).character.inventory().items().back().id;
            (void)party.equip(id, item_id);
        }
    };
    if (armor_type >= 0) add(armor_type, true);
    if (replace_weapons)
    {
        const Array weapons = weapon_value;
        for (int n = 0; n < weapons.size(); ++n) add(int(weapons[n]), n == 0);
    }
    if (replace_shield && bool(shield_value)) add(59, true);
    if (replace_gear)
        for (const Variant &type : Array(gear_value)) add(int(type), false);
}
CombatView *fight(Node *parent)
{ return Object::cast_to<CombatView>(parent->get_node_or_null("CombatTestFight")); }
} // namespace

void CombatTestBridge::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("catalog"), &CombatTestBridge::catalog);
    ClassDB::bind_method(D_METHOD("spell_options", "member"), &CombatTestBridge::spell_options);
    ClassDB::bind_method(D_METHOD("start", "configuration"), &CombatTestBridge::start);
    ClassDB::bind_method(D_METHOD("outcome"), &CombatTestBridge::outcome);
    ClassDB::bind_method(D_METHOD("leave"), &CombatTestBridge::leave);
}

Dictionary CombatTestBridge::catalog() const
{
    Dictionary result;
    try
    {
        auto creation = srd5::character_rules();
        Array classes, species, maps, monsters, weapons, armors, gear;
        Dictionary class_defaults;
        for (const auto &c : creation->choices(CreationField::character_class)) classes.push_back(gs(c.id));
        for (const auto &c : creation->choices(CreationField::race)) species.push_back(gs(c.id));
        const auto art = CharacterArt::load(game_dir());
        const auto pool = character_pool(*creation, art);
        auto rules = srd5::load(resource("data/rules/srd-5.2.1/combat.rules"));
        for (const auto &candidate : pool)
        {
            const auto key = gs(candidate.creation_data().character_class);
            if (class_defaults.has(key)) continue;
            Dictionary defaults;
            defaults["name"] = gs(candidate.sheet().name);
            defaults["species"] = gs(candidate.creation_data().race);
            Array scores;
            for (unsigned a = 0; a < 6; ++a)
                scores.push_back(candidate.sheet().base[Ability(a)]);
            defaults["abilities"] = scores;
            auto advanced = candidate;
            VitalState vitals{candidate.sheet().hit_points, false, {}};
            while (advanced.sheet().level < 3) advanced.advance(*rules, vitals);
            for (const auto &grant : advanced.sheet().grants)
                if (grant.id.starts_with("subclass:")) defaults["subclass"] = gs(grant.id.substr(9));
            class_defaults[key] = defaults;
        }
        const auto map_catalog = MapCatalog::load(game_dir());
        for (const auto &[id, _] : map_catalog.all())
            maps.push_back(gs(id.archive + ":" + std::to_string(id.record)));
        for (const auto &m : monster_choices)
        {
            Dictionary row;
            row["id"] = m.id; row["label"] = m.label; row["leaders"] = m.leader[0] != 0;
            monsters.push_back(row);
        }
        const auto body = CombatBodyCatalog::load(resource("data/art/combat-body-looks.tsv"),
                                                  resource("data/art/combat-weapon-options.tsv"));
        std::set<int> seen;
        for (const auto &option : body.options())
        {
            if (option.original_type <= 0 || option.original_type >= 50 ||
                option.original_type == original_item::quarrels || !seen.insert(option.original_type).second)
                continue;
            Dictionary row; row["type"] = option.original_type; row["label"] = gs(option.label);
            weapons.push_back(row);
        }
        for (const auto &[type, label] : std::array<std::pair<int, const char *>, 3>{
                 {{0, "None"}, {50, "Leather armor"}, {55, "Chain mail"}}})
        { Dictionary row; row["type"] = type; row["label"] = label; armors.push_back(row); }
        for (const auto &[type, label] : std::array<std::pair<int, const char *>, 4>{
                 {{int(authored_item::torch), "Torch"}, {int(original_item::flask_of_oil), "Oil"},
                  {int(authored_item::alchemists_fire), "Alchemist's Fire"},
                  {int(authored_item::acid), "Acid"}}})
        { Dictionary row; row["type"] = type; row["label"] = label; gear.push_back(row); }
        result["classes"] = classes; result["species"] = species;
        result["class_defaults"] = class_defaults;
        result["maps"] = maps; result["monsters"] = monsters;
        result["weapons"] = weapons; result["armors"] = armors; result["gear"] = gear;
    }
    catch (const std::exception &e) { result["error"] = gs(e.what()); }
    return result;
}

Dictionary CombatTestBridge::spell_options(const Dictionary &member) const
{
    Dictionary result;
    try
    {
        const auto dir = game_dir();
        const auto rules_file = resource("data/rules/srd-5.2.1/combat.rules");
        auto rules = srd5::load(rules_file);
        auto setup = make_combat_demo(srd5::load(rules_file), *srd5::character_rules(), dir,
                                      {.body_catalog_file = resource("data/art/combat-body-looks.tsv"),
                                       .classes = {string_at(member, "class")}});
        auto &party = *setup.party;
        const auto id = party.state().slots[0];
        const auto creation = srd5::character_rules();
        const auto &draft = party.member(id).character.creation_data();
        Array starting_groups;
        const auto cantrip_group = creation->cantrip_options(draft);
        Array cantrips;
        for (const auto &option : cantrip_group.options) cantrips.push_back(gs(option.id));
        result["cantrips"] = cantrips;
        result["cantrip_count"] = int(cantrip_group.count);
        for (const auto &group : creation->spell_choice_options(draft).learning)
        {
            Dictionary row; row["id"] = gs(group.id); row["label"] = gs(group.label);
            row["count"] = int(group.count);
            Array choices;
            for (const auto &option : group.options) choices.push_back(gs(option.id));
            row["options"] = choices; starting_groups.push_back(row);
        }
        result["starting_groups"] = starting_groups;
        const int level = int_at(member, "level", 1);
        if (level < 1 || level > 15) throw std::runtime_error("Level must be 1-15");
        auto state = party.checkpoint();
        state.roster[0].experience = rules->experience_for_level(unsigned(level));
        party.restore(std::move(state));
        while (party.member(id).character.sheet().level < level)
            party.advance(id, party.default_advancement(id));
        const auto options = rules->spell_choice_options(party.member(id).character.sheet(),
                                                         SpellChoiceContext::long_rest);
        Array prepared;
        for (const auto &option : options.preparation) prepared.push_back(gs(option.id));
        result["prepared"] = prepared;
        result["count"] = int(options.prepared_count);
    }
    catch (const std::exception &e) { result["error"] = gs(e.what()); }
    return result;
}

String CombatTestBridge::start(const Dictionary &config)
{
    try
    {
        if (fight(get_parent())) throw std::runtime_error("Finish the current fight first");
        const auto dir = game_dir();
        const Array members = config.get("members", Array());
        if (members.size() != 6) throw std::runtime_error("Exactly six members are required");
        std::vector<std::string> classes;
        for (const Variant &entry : members) classes.push_back(string_at(Dictionary(entry), "class"));
        auto setup = make_combat_demo(srd5::load(resource("data/rules/srd-5.2.1/combat.rules")),
                                      *srd5::character_rules(), dir,
                                      {.body_catalog_file = resource("data/art/combat-body-looks.tsv"),
                                       .classes = classes});
        auto party = std::move(setup.party);
        auto rules = srd5::load(resource("data/rules/srd-5.2.1/combat.rules"));
        auto creation = srd5::character_rules();
        auto state = party->checkpoint();
        for (int n = 0; n < 6; ++n)
        {
            const Dictionary spec = members[n];
            auto &who = state.roster[n];
            auto draft = who.character.creation_data();
            const auto name = string_at(spec, "name");
            if (!name.empty()) draft.name = name;
            const Array abilities = spec.get("abilities", Array());
            if (!abilities.is_empty())
            {
                if (abilities.size() != 6) throw std::runtime_error("Six base scores are required");
                for (int a = 0; a < 6; ++a)
                {
                    const int score = int(abilities[a]);
                    if (score < 3 || score > 18) throw std::runtime_error("Base scores must be 3-18");
                    bool found = false;
                    for (int x = 1; x <= 6 && !found; ++x)
                    for (int y = 1; y <= 6 && !found; ++y)
                    for (int z = 1; z <= 6 && !found; ++z)
                        if (x + y + z == score)
                        {
                            draft.rolls[draft.assignment[Ability(a)]] = {{{x, y, z, 1}}, 3};
                            found = true;
                        }
                }
            }
            const auto species = string_at(spec, "species");
            if (!species.empty() && species != draft.race)
            {
                draft.race = species;
                draft.training.clear(); draft.cantrips.reset(); draft.spells.reset();
                for (const auto &requested : creation->training_options(draft))
                {
                    const auto groups = creation->training_options(draft);
                    auto group = std::find_if(groups.begin(), groups.end(),
                                              [&](const auto &g) { return g.id == requested.id; });
                    if (group == groups.end() || group->options.size() < group->count)
                        throw std::runtime_error("Species has incomplete training options");
                    for (unsigned i = 0; i < group->count; ++i)
                        draft.training[group->id].push_back(group->options[i].id);
                }
                const auto cantrips = creation->cantrip_options(draft);
                if (cantrips.count)
                {
                    draft.cantrips.emplace();
                    for (unsigned i = 0; i < cantrips.count; ++i)
                        draft.cantrips->push_back(cantrips.options.at(i).id);
                }
                const auto spells = creation->spell_choice_options(draft);
                if (spells.may_prepare || !spells.learning.empty())
                {
                    draft.spells.emplace();
                    for (const auto &group : spells.learning)
                    {
                        auto &choices = draft.spells->learning[group.id];
                        for (const auto &option : group.options)
                        {
                            if (choices.size() == group.count) break;
                            choices.push_back(option.id);
                        }
                    }
                    if (spells.may_prepare)
                    {
                        draft.spells->prepared.emplace();
                        for (unsigned i = 0; i < spells.prepared_count; ++i)
                            draft.spells->prepared->push_back(spells.preparation.at(i).id);
                    }
                }
            }
            const Variant cantrip_value = spec.get("cantrips", Variant());
            if (cantrip_value.get_type() == Variant::ARRAY)
            {
                draft.cantrips.emplace(); draft.cantrips->clear();
                for (const Variant &spell : Array(cantrip_value))
                    draft.cantrips->push_back(utf8(String(spell)));
            }
            const Variant learned_value = spec.get("starting_spells", Variant());
            if (learned_value.get_type() == Variant::DICTIONARY)
            {
                if (!draft.spells) draft.spells.emplace();
                const Dictionary groups = learned_value;
                for (const Variant &key : groups.keys())
                {
                    auto &chosen = draft.spells->learning[utf8(String(key))];
                    chosen.clear();
                    for (const Variant &spell : Array(groups[key])) chosen.push_back(utf8(String(spell)));
                }
            }
            auto appearance = who.character.appearance();
            if (!species.empty())
            {
                appearance.icon_size = species == "gnome" ? IconSize::small :
                                       species == "goliath" ? IconSize::large : IconSize::regular;
                appearance.portrait_head = matching_portrait_head(species, draft.gender)
                                           .value_or(appearance.portrait_head);
            }
            who.character = who.character.recreated(*creation, *rules, std::move(draft));
            who.character.appearance(appearance);
            who.vitals = {who.character.sheet().hit_points, false, {}};
            const int level = int_at(spec, "level", 1);
            if (level < 1 || level > 15) throw std::runtime_error("Level must be 1-15");
            who.experience = rules->experience_for_level(unsigned(level));
        }
        party->restore(std::move(state));
        for (int n = 0; n < 6; ++n)
        {
            const Dictionary spec = members[n];
            const auto id = party->state().slots[n];
            const int level = int_at(spec, "level", 1);
            while (party->member(id).character.sheet().level < level)
                party->advance(id, party->default_advancement(id));
            const Variant prepared_value = spec.get("prepared_spells", Variant());
            if (prepared_value.get_type() == Variant::ARRAY)
            {
                const Array prepared = prepared_value;
                auto next = party->checkpoint();
                auto &who = next.roster[n];
                SpellChoices choices; choices.prepared.emplace();
                for (const Variant &spell : prepared) choices.prepared->push_back(utf8(String(spell)));
                who.character.choose_spells(*rules, choices, 1, SpellChoiceContext::long_rest,
                                            ChoiceCompleteness::partial);
                next.next_rest_session = std::max<std::uint64_t>(next.next_rest_session, 2);
                party->restore(std::move(next));
            }
            set_gear(*party, id, spec);
            if (bool(spec.get("quick", false))) party->set_quick(id);
        }
        party->set_quick_magic(bool(config.get("quick_magic", false)));
        const auto map_text = string_at(config, "map");
        const auto colon = map_text.rfind(':');
        if (colon == std::string::npos) throw std::runtime_error("Select a game map");
        const MapId map_id{map_text.substr(0, colon), std::uint8_t(std::stoul(map_text.substr(colon + 1)))};
        const auto maps = MapCatalog::load(dir);
        const auto map = maps.find(map_id);
        if (!map) throw std::runtime_error("Selected game map is unavailable");
        const auto &choice = monster(string_at(config, "monster", "orc"));
        const int count = int_at(config, "count", 6), leaders = int_at(config, "leaders", 0);
        if (count < 1 || count > 40 || leaders < 0 || leaders > count || (leaders && !choice.leader[0]))
            throw std::runtime_error("Invalid monster or leader count");
        const auto creatures = CreatureCatalog::load(dir);
        const auto cpic = archive(dir, "CPIC2.DAX");
        CampaignEncounter encounter;
        std::vector<Footprint> footprints;
        for (int n = 0; n < count; ++n)
        {
            const bool leader = n < leaders;
            const unsigned record = leader ? choice.leader_icon : choice.icon;
            const auto source = creatures.find({2, std::uint8_t(record)});
            if (!source) throw std::runtime_error("Monster record missing from game files");
            const auto id = EntityId(1000 + n);
            Participant who{id, leader ? choice.leader : choice.definition,
                            std::string(leader ? "Leader " : "") + choice.label + " " + std::to_string(n + 1),
                            opengold::rules::Side::opposition, {}};
            who.footprint = combat_footprint(source->get().stored);
            footprints.push_back(who.footprint);
            encounter.enemies.push_back(std::move(who));
            encounter.art.push_back({id, icon(cpic, record), icon(cpic, record + 128)});
        }
        const auto dungcom = archive(dir, "DUNGCOM.DAX");
        for (unsigned tile = 0; tile < 25; ++tile)
        {
            auto image = decode_ega_combat_icon(dungcom, 1, std::uint8_t(tile));
            if (!image) throw std::runtime_error("Invalid dungeon combat artwork");
            encounter.terrain_art.push_back(std::move(image.image));
        }
        const auto seed = std::uint64_t(std::int64_t(config.get("seed", 1)));
        std::mt19937_64 rng(seed);
        std::array<unsigned, 256> cells{}; std::iota(cells.begin(), cells.end(), 0);
        std::shuffle(cells.begin(), cells.end(), rng);
        bool placed = false;
        for (unsigned cell : cells)
        {
            for (unsigned facing = 0; facing < 4; ++facing)
            {
                auto field = dungeon_battlefield(map->get(), cell % 16, cell / 16);
                const auto direction = MapDirection((facing + seed) % 4);
                const auto positions = encounter_formation(field, direction, 1, 6, footprints);
                const auto complete = [](const auto &v) { return std::all_of(v.begin(), v.end(),
                    [](const auto &p) { return p.has_value(); }); };
                if (!complete(positions.party) || !complete(positions.enemies)) continue;
                encounter.field = std::move(field); encounter.facing = direction; encounter.distance = 1;
                std::array<unsigned, 6> strong{0, 1, 2, 3, 4, 5}, near{0, 1, 2, 3, 4, 5};
                std::stable_sort(strong.begin(), strong.end(), [&](unsigned a, unsigned b)
                {
                    const auto x = party->profile(party->state().slots[a]);
                    const auto y = party->profile(party->state().slots[b]);
                    return x.armor_class != y.armor_class ? x.armor_class > y.armor_class :
                           x.hit_points > y.hit_points;
                });
                const auto enemy = *positions.enemies.front();
                std::stable_sort(near.begin(), near.end(), [&](unsigned a, unsigned b)
                {
                    const auto distance = [&](unsigned n) { const auto p = *positions.party[n];
                        return std::abs(p.x - enemy.x) + std::abs(p.y - enemy.y); };
                    return distance(a) < distance(b);
                });
                encounter.positions.resize(std::size_t(6 + count));
                for (unsigned n = 0; n < 6; ++n) encounter.positions[strong[n]] = *positions.party[near[n]];
                for (int n = 0; n < count; ++n) encounter.positions[6 + n] = *positions.enemies[n];
                placed = true; break;
            }
            if (placed) break;
        }
        if (!placed) throw std::runtime_error("No position in this map fits the encounter");
        auto art = CharacterArt::load(dir);
        const auto body = CombatBodyCatalog::load(resource("data/art/combat-body-looks.tsv"),
                                                  resource("data/art/combat-weapon-options.tsv"));
        std::vector<CombatArt> images;
        for (const auto &who : party->participants())
        {
            const auto selected = resolve_combat_appearance(party->member(who.id), body);
            images.push_back({who.id, selected.icon(art, IconPose::ready),
                              selected.icon(art, IconPose::action),
                              selected.selection.matched ? std::string{} : selected.selection.label});
        }
        auto owned = presentation::instantiate_scene("res://scenes/combat_demo.tscn");
        auto *combat = Object::cast_to<CombatView>(owned.get());
        if (!combat) throw std::runtime_error("Game combat scene unavailable");
        combat->set_name("CombatTestFight");
        combat->campaign_party(party, std::move(images));
        combat->campaign_encounter(std::move(encounter));
        combat->prepare_combat();
        presentation::attach_child(*get_parent(), std::move(owned));
        return {};
    }
    catch (const std::exception &e) { return gs(e.what()); }
}

String CombatTestBridge::outcome() const
{
    const auto *combat = fight(get_parent());
    if (!combat) return {};
    const auto result = combat->completed_outcome();
    if (!result) return {};
    return result->outcome == Outcome::victory ? "Victory" :
           result->outcome == Outcome::defeat ? "Defeat" : "Finished";
}

void CombatTestBridge::leave()
{
    if (auto *combat = fight(get_parent()))
    {
        get_parent()->remove_child(combat);
        combat->queue_free();
    }
}
