#include "opengold/campaign_save.h"
#include "opengold/save_file.h"
#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>

namespace opengold
{
namespace
{
constexpr std::size_t limit = 16 * 1024 * 1024;

void require(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::uint64_t fingerprint(std::string_view data)
{
    std::uint64_t n = 14695981039346656037ULL;
    for (unsigned char c : data)
    {
        n ^= c;
        n *= 1099511628211ULL;
    }
    return n;
}
} // namespace

// Explicit field encoding: no pointers, native object layouts or derived sheets.
struct SaveCodec
{
    bool reading{};
    unsigned version{11};
    std::stringstream stream;
    const rules::CharacterRules *creation{};
    const rules::RulesModule *module {};
    rules::Identity saved_identity;

    explicit SaveCodec(std::string_view bytes) : reading(true), stream(std::string(bytes))
    {
        stream.imbue(std::locale::classic());
    }

    SaveCodec()
    {
        stream.imbue(std::locale::classic());
    }

    template <class... T> void fields(T &...value)
    {
        (field(value), ...);
    }

    template <class T>
    requires std::is_integral_v<T>
    void field(T &value)
    {
        if (reading)
        {
            if constexpr(std::is_signed_v<T>)
            {
                std::int64_t n{};
                stream >> n;
                require(n >= std::numeric_limits<T>::min() && n <= std::numeric_limits<T>::max(),
                        "Save integer out of range");
                value = static_cast<T>(n);
            }
            else
            {
                std::string token;
                stream >> token;
                require(!token.empty() &&
                        token.find_first_not_of("0123456789") == std::string::npos,
                        "Invalid save integer");
                std::size_t used{};
                auto n = std::stoull(token, &used);
                require(used == token.size() &&
                        n <= static_cast<std::uint64_t>(std::numeric_limits<T>::max()),
                        "Save integer out of range");
                value = static_cast<T>(n);
            }
        }
        else if constexpr(std::is_signed_v<T>)
            stream << static_cast<std::int64_t>(value) << ' ';
        else
            stream << static_cast<std::uint64_t>(value) << ' ';
        require(bool(stream), "Truncated or invalid campaign save");
    }

    void field(std::string &value)
    {
        if (reading)
            stream >> std::quoted(value);
        else
            stream << std::quoted(value) << ' ';
        require(bool(stream) && value.size() <= limit, "Invalid save string");
    }

    template <class T, std::size_t N> void field(std::array<T, N> &values)
    {
        for (auto &v : values)
            field(v);
    }

    template <class T> void field(std::vector<T> &values)
    {
        std::uint64_t count = values.size();
        field(count);
        require(count <= 65536, "Save collection too large");
        if (reading)
        {
            values.clear();
            for (std::uint64_t i = 0; i < count; ++i)
            {
                T v{};
                field(v);
                values.push_back(std::move(v));
            }
        }
        else
            for (auto &v : values)
                field(v);
    }

    template <class K, class V> void field(std::map<K, V> &values)
    {
        std::uint64_t count = values.size();
        field(count);
        require(count <= 65536, "Save map too large");
        if (reading)
        {
            values.clear();
            for (std::uint64_t i = 0; i < count; ++i)
            {
                K k{};
                V v{};
                fields(k, v);
                require(values.emplace(k, std::move(v)).second, "Duplicate save map key");
            }
        }
        else
            for (auto &[key, v] : values)
            {
                auto k = key;
                fields(k, v);
            }
    }

    template <class T> void field(std::optional<T> &value)
    {
        bool present = value.has_value();
        field(present);
        if (reading)
        {
            if (present)
                value.emplace();
            else
                value.reset();
        }
        if (present)
            field(*value);
    }

    void field(rules::Identity &v)
    {
        fields(v.module, v.version, v.content);
    }

    void field(rules::AbilityRoll &v)
    {
        fields(v.dice, v.discarded);
    }

    void field(rules::FeatureGrant &v)
    {
        fields(v.id, v.source_id, v.level, v.choices);
    }

    void field(rules::AdvancementChoice &v)
    {
        fields(v.feat, v.abilities, v.spells);
        if (version >= 15)
            field(v.training);
        if (version >= 16)
            field(v.spell_learning);
        if (version >= 17)
            field(v.fighting_style);
    }

    void field(rules::CharacterDraft &v)
    {
        fields(v.race, v.gender, v.character_class, v.alignment, v.background, v.name,
               v.target_classes, v.rolls, v.assignment, v.adjustment, v.rolled);
        if (version >= 9)
            field(v.training);
        if (version >= 11)
            field(v.cantrips);
        if (version >= 16)
            field(v.spells);
    }

    void field(rules::SpellChoices &v)
    {
        fields(v.learning, v.prepared, v.replace_cantrip, v.replacement);
    }

    void field(SpellChoiceEdit &v)
    {
        fields(v.level, v.rest_session, v.choices);
    }

    void field(TrainingChoiceEdit &v)
    {
        fields(v.level, v.rest_session, v.selections);
    }

    void field(por::CharacterAppearance &v)
    {
        fields(v.portrait_head, v.portrait_body, v.combat_head, v.combat_body, v.tall, v.colors);
        if (version >= 4)
            field(v.portrait);
    }

    void field(InventoryItem &v)
    {
        fields(v.id, v.definition_id, v.name, v.quantity, v.original_type);
    }

    void field(Inventory &v)
    {
        fields(v.next_id_, v.items_);
        if (reading)
        {
            std::set<std::uint64_t> ids;
            require(v.next_id_ != 0, "Invalid next inventory ID");
            for (const auto &i : v.items_)
                require(i.id && i.id < v.next_id_ && ids.insert(i.id).second && i.quantity &&
                        !i.definition_id.empty() && !i.name.empty(),
                        "Invalid inventory entry");
        }
    }

    void field(por::DamageDice &v)
    {
        fields(v.count, v.sides, v.modifier);
    }

    void field(por::ItemRecord &v)
    {
        fields(v.raw, v.stored_name, v.type, v.name_components, v.magic_bonus, v.save_bonus,
               v.readied_raw, v.revealed_components, v.cursed_raw, v.weight, v.value, v.stack_size,
               v.effect_codes);
    }

    void field(por::ItemTemplate &v)
    {
        fields(v.raw, v.worn_location, v.hands, v.rate_of_fire, v.protection_raw, v.damage_type,
               v.melee_flag, v.large_damage, v.small_medium_damage, v.range, v.class_restrictions,
               v.ammunition_type);
    }

    void field(por::EquipmentBonuses &v)
    {
        fields(v.weapon_to_hit, v.weapon_damage, v.armor_base_ac, v.ac_adjustment, v.save_bonus);
    }

    void field(por::Equipment &v)
    {
        fields(v.index, v.stored, v.base, v.bonuses);
    }

    void field(RestTicket &v)
    {
        fields(v.session, v.revision);
    }

    void field(ShortRestSession &v)
    {
        fields(v.ticket, v.completed_minutes, v.completed_subminute_milliseconds, v.members);
    }

    void field(RestActivity &v)
    {
        unsigned kind = static_cast<unsigned>(v.kind), work = static_cast<unsigned>(v.work),
                 interruption = static_cast<unsigned>(v.interruption);
        fields(v.ticket, kind, v.started_minutes, v.started_subminute_milliseconds,
               v.elapsed_milliseconds, v.segment_milliseconds, v.sleep_milliseconds,
               v.light_milliseconds, v.exertion_milliseconds, v.extension_milliseconds,
               v.interrupted, work, interruption, v.members);
        if (reading)
        {
            v.kind = static_cast<RestKind>(kind);
            v.work = static_cast<RestWork>(work);
            v.interruption = static_cast<RestInterruption>(interruption);
        }
    }

    void field(DetachedPartyItem &v)
    {
        fields(v.scope, v.token, v.original_owner, v.holder, v.cell.x, v.cell.y, v.item,
               v.original);
        if (version >= 14)
            field(v.rest_session);
    }

    void rest(PartyState &v)
    {
        if (version >= 10)
            fields(v.next_rest_session, v.short_rest);
        if (version >= 12)
            field(v.rest_activity);
        if (version >= 13)
            field(v.detached_items);
        if (version >= 16)
            field(v.spell_rest);
        if (version >= 18)
            field(v.training_rest);
    }

    void field(rules::VitalState &v)
    {
        fields(v.hit_points, v.dead, v.resources, v.description);
    }

    void member(PartyMember &v)
    {
        fields(v.id, v.npc_source, v.vitals, v.wealth, v.equipped, v.morale, v.experience,
               v.last_rest_minutes, v.item_sources, v.creation_source);
        if (version >= 7)
            field(v.equipment.weapon_hands);
        if (version >= 8)
        {
            auto grants = v.character.sheet().grants;
            field(grants);
            if (reading)
                module->validate_saved_grants(saved_identity, v.character.sheet(), grants);
            else
                require(grants == v.character.sheet().grants,
                        "Saved grants disagree with creation or advancement choices");
        }
        // Older releases stored ordinary equipment as unsupported. Migrate only
        // the exact old key backed by matching original item provenance.
        if (reading)
            for (auto &item : v.character.inventory().items_)
            {
                const auto source = v.item_sources.find(item.id);
                if (source != v.item_sources.end() &&
                        item.original_type == source->second.stored.type &&
                        item.definition_id == "por:unsupported:" + std::to_string(item.original_type))
                    item.definition_id = equipment_conversion(source->second);
            }
    }

    void field(PartyState &v)
    {
        fields(v.slots, v.next_id, v.selected, v.time_minutes, v.random_state, v.claimed_rewards);
        std::map<MemberId, unsigned> rest_offsets;
        if (version >= 6)
        {
            if (!reading)
                for (const auto &member : v.roster)
                    if (member.last_rest_subminute_milliseconds)
                        rest_offsets.emplace(member.id, member.last_rest_subminute_milliseconds);
            fields(v.subminute_milliseconds, v.next_combat_scope, rest_offsets);
        }
        std::uint64_t count = v.roster.size();
        field(count);
        require(count <= 128, "Too many saved party members");
        if (reading)
        {
            v.roster.clear();
            for (std::uint64_t i = 0; i < count; ++i)
            {
                rules::CharacterDraft draft;
                por::CharacterAppearance appearance;
                int level{};
                fields(draft, appearance, level);
                require(level >= 1 && level <= (version >= 3 ? 4 : 2),
                        "Unsupported saved character level");
                Character character(*creation, std::move(draft), appearance);
                rules::VitalState scratch;
                if (version >= 3)
                {
                    std::vector<rules::AdvancementChoice> history;
                    field(history);
                    require(history.size() == level - 1,
                            "Saved advancement history disagrees with level");
                    std::vector<SpellChoiceEdit> edits;
                    if (version >= 16)
                        field(edits);
                    std::vector<TrainingChoiceEdit> training;
                    if (version >= 18)
                        field(training);
                    unsigned prior_level = 1;
                    std::uint64_t prior_session = 0;
                    for (const auto &edit : training)
                    {
                        require(edit.level >= prior_level && edit.level <= unsigned(level) &&
                                edit.rest_session > prior_session,
                                "Invalid training replacement history");
                        prior_level = edit.level;
                        prior_session = edit.rest_session;
                    }
                    unsigned previous = 1;
                    std::uint64_t rest = 0;
                    for (const auto &edit : edits)
                    {
                        require(edit.level >= previous && edit.level <= unsigned(level),
                                "Invalid spell-choice history level");
                        previous = edit.level;
                        if (edit.rest_session)
                        {
                            require(edit.rest_session > rest, "Repeated spell-choice rest");
                            rest = edit.rest_session;
                        }
                    }
                    auto replay = [&]
                    {
for (const auto &edit : edits)
                        if (edit.level == unsigned(character.sheet().level))
                                character.choose_spells(*module, edit.choices, edit.rest_session,
                                                        false);
for (const auto &edit : training)
                                if (edit.level == unsigned(character.sheet().level))
                                        character.replace_rest_training(*module, edit.selections,
                                                                        edit.rest_session);
                                    };
                    replay();
                    for (const auto &choice : history)
                    {
                        require(character.advance(*module, scratch, choice),
                                "Unsupported saved advancement choice");
                        replay();
                    }
                }
                else
                    while (character.sheet().level < level)
                    {
                        auto choice = module->default_advancement(character.sheet());
                        choice.training.clear();
                        choice.spell_learning.reset();
                        require(character.advance(*module, scratch, choice),
                                "Unsupported saved advancement");
                    }
                field(character.inventory());
                PartyMember m{0, std::move(character)};
                member(m);
                if (version < 7)
                {
                    std::vector<std::string> gear;
                    for (auto id : m.equipped)
                    {
                        const auto item = m.character.inventory().find(id);
                        require(item.has_value(), "Equipped item is missing");
                        gear.push_back(item->get().definition_id);
                    }
                    m.equipment = module->migrate_equipment(gear);
                }
                module->migrate_character_state(saved_identity, m.character.sheet(), m.vitals);
                v.roster.push_back(std::move(m));
            }
        }
        else
            for (auto &m : v.roster)
            {
                auto draft = m.character.creation_data();
                auto appearance = m.character.appearance();
                auto level = m.character.sheet().level;
                fields(draft, appearance, level);
                auto history = m.character.advancements();
                field(history);
                if (version >= 16)
                {
                    auto edits = m.character.spell_edits();
                    field(edits);
                }
                if (version >= 18)
                {
                    auto edits = m.character.training_edits();
                    field(edits);
                }
                field(m.character.inventory());
                member(m);
            }
        if (reading)
            for (const auto &[id, offset] : rest_offsets)
            {
                auto member = std::find_if(v.roster.begin(), v.roster.end(),
                                           [&](const auto & m)
                {
                    return m.id == id;
                });
                require(member != v.roster.end() && member->last_rest_minutes && offset < 60000,
                        "Invalid rest time offset");
                member->last_rest_subminute_milliseconds = offset;
            }
    }

    void machine(por::EclMachine &v)
    {
        require(v.state_ == por::EclState::idle || v.state_ == por::EclState::completed || reading,
                "Cannot save a pending script");
        fields(v.variables_, v.image_, v.instruction_spans_, v.conditions_, v.next_request_,
               v.total_instructions_);
        std::string rng;
        if (!reading)
        {
            std::ostringstream out;
            out.imbue(std::locale::classic());
            out << v.random_;
            rng = out.str();
        }
        field(rng);
        if (reading)
        {
            require(v.image_.size() == v.program_->raw().size(), "Script image size mismatch");
            std::istringstream in(rng);
            in.imbue(std::locale::classic());
            in >> v.random_;
            require(bool(in), "Invalid saved script RNG");
            in >> std::ws;
            require(in.eof(), "Trailing script RNG data");
            v.state_ = por::EclState::completed;
            v.pending_.reset();
            v.destination_.reset();
            v.menu_values_.clear();
            v.stack_.clear();
            v.trace_.clear();
            v.diagnostic_.clear();
        }
    }

    void town(por::RolfTourSession &v)
    {
        require(reading || (v.can_leave() && v.snapshot_.tour_finished && !v.checkpoint_ &&
                            !v.pending_movement_),
                "Save only during idle town exploration");
        fields(v.current_script_, v.selected_character_, v.next_ticket_);
        if (version >= 2)
        {
            unsigned area = v.current_area_;
            field(area);
            std::map<unsigned, std::string> explored;
            if (!reading)
            {
                for (const auto &[id, cells] : v.visited_areas_)
                    explored[id] = cells.to_string();
                explored[v.current_area_] = v.snapshot_.visited.to_string();
            }
            field(explored);
            if (reading)
            {
                require(v.town_ && (area == 0 || v.town_->districts.contains(area)),
                        "Unsupported saved district");
                require((v.current_script_ == 20) == (area == 20),
                        "Saved district and script disagree");
                v.current_area_ = 0;
                v.snapshot_.area_id = 0;
                if (v.town_->map)
                {
                    v.map_ = *v.town_->map;
                    v.wall_art_ = v.town_->wall_art;
                }
                v.change_area(area);
                v.visited_areas_.clear();
                for (const auto &[id, cells] : explored)
                {
                    require((id == 0 || v.town_->districts.contains(id)) && cells.size() == 256 &&
                            cells.find_first_not_of("01") == std::string::npos,
                            "Invalid saved district exploration");
                    v.visited_areas_[id] = std::bitset<256>(cells);
                }
            }
            std::uint64_t pending = v.pending_loot_.size();
            field(pending);
            require(pending <= 1024, "Too many pending rewards");
            if (reading)
                v.pending_loot_.clear();
            for (std::uint64_t index = 0; index < pending; ++index)
            {
                std::vector<unsigned> records;
                std::string reward;
                bool items = true;
                if (!reading)
                {
                    const auto &loot = v.pending_loot_[index];
                    records = loot.records;
                    reward = loot.reward_id;
                    items = loot.include_items;
                }
                fields(records, reward, items);
                if (reading)
                {
                    require(v.town_->districts.contains(20),
                            "Pending Slums loot requires original resources");
                    v.pending_loot_.push_back(
                        v.slums_loot(std::move(records), std::move(reward), items));
                }
            }
        }
        else if (reading)
        {
            require(v.current_script_ != 20, "Version-one saves cannot contain the Slums");
            v.current_area_ = 0;
            v.snapshot_.area_id = 0;
            v.visited_areas_.clear();
            v.pending_loot_.clear();
            if (v.town_->map)
            {
                v.map_ = *v.town_->map;
                v.wall_art_ = v.town_->wall_art;
            }
        }
        if (reading)
        {
            require(v.town_ && v.town_->programs.contains(v.current_script_) &&
                    v.selected_character_ < 8,
                    "Unsupported saved town context");
            v.machine_ = por::EclMachine(v.town_->programs.at(v.current_script_));
            for (const std::uint8_t op :
                    {
                        12, 13, 14, 45, 49, 58
                    })
                v.machine_.enable_host(op);
            v.configure_town();
        }
        machine(v.machine_);
        if (reading)
        {
            v.combat_request_ = 0;
            v.staged_enemies_.clear();
            v.staged_art_.clear();
            v.encounter_.reset();
            v.monster_picture_id_.reset();
            v.showing_monster_picture_ = false;
        }
        auto &s = v.snapshot_;
        fields(s.dialogue, s.prompts, s.redraws, s.footsteps, s.event_runs);
        std::string visited = s.visited.to_string();
        field(visited);
        if (reading)
        {
            require(visited.size() == 256 && visited.find_first_not_of("01") == std::string::npos,
                    "Invalid visited map");
            s.visited = std::bitset<256>(visited);
        }
        if (version >= 5)
        {
            std::map<unsigned, std::string> known;
            if (!reading)
            {
                for (const auto &[id, cells] : v.seen_areas_)
                    known[id] = cells.to_string();
                known[v.current_area_] = s.seen.to_string();
            }
            field(known);
            if (reading)
            {
                v.seen_areas_.clear();
                for (const auto &[id, cells] : known)
                {
                    require((id == 0 || v.town_->districts.contains(id)) && cells.size() == 256 &&
                            cells.find_first_not_of("01") == std::string::npos,
                            "Invalid saved map knowledge");
                    v.seen_areas_[id] = std::bitset<256>(cells);
                }
                require(v.seen_areas_.contains(v.current_area_), "Missing current map knowledge");
                s.seen = v.seen_areas_.at(v.current_area_);
                require((s.visited & ~s.seen).none(), "Visited cells must be known");
                for (const auto &[id, cells] : v.visited_areas_)
                    require(v.seen_areas_.contains(id) && (cells & ~v.seen_areas_.at(id)).none(),
                            "Missing visited district knowledge");
            }
        }
        else if (reading)
        {
            // Older saves remember visits only. Keep them, then discover the
            // current sightline when the restored 3D view is actually shown.
            v.seen_areas_ = v.visited_areas_;
            s.seen = s.visited;
        }
        if (reading)
        {
            s.phase = por::TourPhase::completed;
            s.tour_finished = true;
            s.script_id = v.current_script_;
            s.sprite_frame = -1;
            s.choices.clear();
            s.continue_ticket = 0;
            s.diagnostic.clear();
            ++s.picture_revision;
            v.picture_.reset();
            v.checkpoint_.reset();
            v.saved_campaign_.reset();
            v.pending_movement_.reset();
            v.treasure_.clear();
            v.who_slots_.clear();
            v.temple_targets_.clear();
            v.menu_request_ = v.delayed_request_ = v.who_request_ = v.temple_request_ =
            v.shop_request_ = 0;
            v.remaining_delay_ = 0;
            v.transition_ = v.message_only_ = false;
            v.event_stage_ = 0;
            v.publish_pose();
            v.campaign_.reset();
        }
    }
};

std::string encode_campaign(const CampaignParty &party, const por::RolfTourSession *town,
                            std::string_view assets)
{
    require(!party.in_combat(), "Cannot save during combat");
    SaveCodec out;
    out.version =
        party.state().training_rest ||
        std::any_of(party.state().roster.begin(), party.state().roster.end(),
                    [](const auto & member)
    {
        return !member.character.training_edits().empty();
    })
    ? 18
    : std::any_of(party.state().roster.begin(), party.state().roster.end(),
                  [](const auto & member)
    {
        return std::any_of(member.character.advancements().begin(),
                           member.character.advancements().end(),
                           [](const auto & choice)
        {
            return choice.fighting_style.has_value();
        });
    })
    ? 17
    : party.state().spell_rest ||
           std::any_of(party.state().roster.begin(), party.state().roster.end(),
                       [](const auto & member)
    {
        return member.character.creation_data().spells.has_value() ||
               !member.character.spell_edits().empty() ||
               std::any_of(member.character.advancements().begin(),
                           member.character.advancements().end(),
                           [](const auto & choice)
        {
            return choice.spell_learning.has_value();
        });
    })
    ? 16
    : std::any_of(party.state().roster.begin(), party.state().roster.end(),
                  [](const auto & member)
    {
        return std::any_of(member.character.advancements().begin(),
                           member.character.advancements().end(),
                           [](const auto & choice)
        {
            return !choice.training.empty();
        });
    })
    ? 15
    : std::any_of(party.state().detached_items.begin(), party.state().detached_items.end(),
                  [](const auto & item)
    {
        return item.rest_session != 0;
    })
    ? 14
    : !party.state().detached_items.empty() ? 13
            : party.state().rest_activity           ? 12
            : 11;
    auto identity = party.identity();
    std::string asset(assets);
    auto state = party.checkpoint();
    out.fields(identity, asset, state);
    bool has_town = town != nullptr;
    out.field(has_town);
    if (town)
    {
        auto copy = *town;
        out.town(copy);
    }
    out.rest(state);
    auto body = out.stream.str();
    require(body.size() <= limit, "Campaign save too large");
    return "OPENGOLD-CAMPAIGN " + std::to_string(out.version) + "\n" +
           std::to_string(fingerprint(body)) + "\n" + body;
}

namespace
{
void validate_saved_member(const PartyMember &member, const rules::RulesModule &module)
{
    module.validate_character_state(member.character.sheet(), member.vitals);
    for (const auto &[id, source] : member.item_sources)
    {
        const auto item = member.character.inventory().find(id);
        require(item.has_value() && item->get().definition_id == equipment_conversion(source) &&
                item->get().original_type == source.stored.type,
                "Saved item provenance mismatch");
    }
    for (const auto &item : member.character.inventory().items())
        if (!member.item_sources.contains(item.id))
        {
            if (module.equipment_info(item.definition_id).slot == rules::EquipmentSlot::carried)
                continue;
            const std::array<std::string, 1> definition{item.definition_id};
            (void)module.character_profile(member.character.sheet(), definition);
        }
    std::vector<std::string> gear;
    for (auto id : member.equipped)
        gear.push_back(member.character.inventory().find(id)->get().definition_id);
    (void)module.character_profile(member.character.sheet(), gear, member.equipment);
}
} // namespace

SavedCampaign decode_campaign(std::string_view bytes, const rules::CharacterRules &creation,
                              const rules::RulesModule &module, std::string_view assets,
                              const por::RolfTourSession *town_template)
{
    require(bytes.size() <= limit, "Campaign save too large");
    unsigned version{};
    std::size_t header_size{};
    for (unsigned v = 1; v <= 18; ++v)
    {
        const auto header = "OPENGOLD-CAMPAIGN " + std::to_string(v) + '\n';
        if (bytes.starts_with(header))
        {
            version = v;
            header_size = header.size();
            break;
        }
    }
    require(version != 0, "Unsupported campaign save version");
    const auto end = bytes.find('\n', header_size);
    require(end != bytes.npos, "Truncated campaign save");
    const auto body = bytes.substr(end + 1);
    require(bytes.substr(header_size, end - header_size) == std::to_string(fingerprint(body)),
            "Campaign save checksum mismatch");
    SaveCodec in(body);
    in.version = version;
    in.creation = &creation;
    in.module = &module;
    rules::Identity identity;
    std::string asset;
    in.fields(identity, asset);
    require(module.accepts_campaign_identity(identity), "Campaign rules/content version mismatch");
    require(asset == assets, "Campaign original asset identity mismatch");
    in.saved_identity = identity;
    SavedCampaign result;
    in.field(result.party);
    CampaignParty::validate(result.party);
    for (const auto &m : result.party.roster)
        validate_saved_member(m, module);
    bool has_town{};
    in.field(has_town);
    if (has_town)
    {
        require(town_template != nullptr, "This save requires town resources");
        result.town = *town_template;
        in.town(*result.town);
    }
    in.rest(result.party);
    CampaignParty::validate(result.party);
    CampaignParty::validate_rest_activity(result.party, module);
    in.stream >> std::ws;
    require(in.stream.eof(), "Trailing campaign save data");
    return result;
}

std::string read_campaign_file(const std::filesystem::path &path)
{
    return read_save_file(path, limit);
}

std::string campaign_asset_identity(const std::filesystem::path &directory)
{
    std::map<std::string, std::uint64_t> files;
    for (const auto &entry : std::filesystem::directory_iterator(directory))
    {
        if (!entry.is_regular_file())
            continue;
        auto name = entry.path().filename().string();
        for (auto &c : name)
            if (c >= 'a' && c <= 'z')
                c -= 32;
        if (name.ends_with(".DAX") || name == "ITEMS")
        {
            require(!files.contains(name), "Ambiguous original asset filename");
            files.emplace(name, fingerprint(read_campaign_file(entry.path())));
        }
    }
    require(files.contains("ECL3.DAX") && files.contains("GEO3.DAX") && files.contains("ITEMS"),
            "Missing campaign assets");
    std::ostringstream out;
    for (auto &[name, hash] : files)
        out << name << ':' << hash << ';';
    return out.str();
}

void write_campaign_file(const std::filesystem::path &path, std::string_view bytes)
{
    write_save_file(path, bytes, limit);
}
} // namespace opengold
