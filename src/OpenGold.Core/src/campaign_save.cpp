#include "opengold/campaign_save.h"
#include "opengold/diagnostic_log.h"
#include "opengold/path_text.h"
#include "opengold/save_file.h"
#include "opengold/ua_design.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>

namespace opengold
{
namespace
{
constexpr std::size_t limit = 16 * 1024 * 1024;
constexpr std::string_view campaign_magic = "OPENGOLD-CAMPAIGN ";

struct BuckWilderness
{
    std::uint16_t kind{}, kind_instruction{}, bank{};
    std::array<std::uint16_t, 3> return_scripts{};
};

// These installed Buck wilderness controllers retain the previous 3D map.
// Their own initialization writes the area kind at these instruction sites.
std::optional<BuckWilderness> buck_wilderness(GoldBoxGame game, unsigned script)
{
    // Countdown ECL4:64 retains its previous map on LOAD FILES 127,
    // including the genuine village 65 root-cellar return to the camp.
    if (game == GoldBoxGame::countdown_to_doomsday && script == 64)
    {
        return BuckWilderness{0xA8, 0x8034, 4, {66}};
    }
    // ECL5:80 retains the village, base or ruins map. Its entry chooses
    // the corresponding world square from previous scripts81,82,83.
    if (game == GoldBoxGame::countdown_to_doomsday && script == 80)
    {
        return BuckWilderness{0xA2, 0x8028, 5, {81, 82, 83}};
    }
    if (game != GoldBoxGame::matrix_cubed)
    {
        return std::nullopt;
    }
    switch (script)
    {
    case 32:
        return BuckWilderness{0xA8, 0x8045, 1};
    case 48:
        return BuckWilderness{0xA2, 0x802B, 1};
    default:
        return std::nullopt;
    }
}

// Venus's initialization keeps the source controller until its pre-step
// entry at 827D writes 48. Authenticate only the exact installed arrival
// branches and their published squares; preserve the VM's original values.
bool matrix_venus_arrival(const por::EclProgram &program, unsigned script, unsigned previous,
                          unsigned x, unsigned y)
{
    if (script != 48)
    {
        return false;
    }

    struct Arrival
    {
        unsigned previous, x, y;
        std::uint16_t x_instruction, y_instruction;
    };

    static constexpr std::array arrivals{Arrival{19, 5, 2, 0x8E9B, 0x8EA1},
                                         Arrival{49, 34, 12, 0x8058, 0x805E},
                                         Arrival{50, 35, 9, 0x8099, 0x809F}};
    const auto source_save = [&](std::uint16_t address, unsigned value, unsigned cell)
    {
        const auto instruction = program.instruction(address);
        return instruction.opcode == 9 && instruction.operands.size() == 2 &&
               instruction.operands[0].tag == 0 && instruction.operands[0].value == value &&
               instruction.operands[1].tag == 1 && instruction.operands[1].value == cell;
    };
    for (const auto &arrival : arrivals)
    {
        if (previous != arrival.previous || x != arrival.x || y != arrival.y)
        {
            continue;
        }
        try
        {
            return source_save(arrival.x_instruction, arrival.x, 0x4BC3) &&
                   source_save(arrival.y_instruction, arrival.y, 0x4BC4);
        }
        catch (const por::EclError &)
        {
            return false;
        }
    }
    return false;
}

void require(bool ok, const char *message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
}

// Whether `left` could be what remains of the creatures' original `gear` after
// a fight (DIFF-1): each item is one of the original items' kinds, each kind
// at most as often as it was carried.
bool original_gear_left(const std::vector<por::Equipment> &gear,
                        const std::vector<por::Equipment> &left)
{
    std::vector<bool> matched(gear.size());
    for (const auto &item : left)
    {
        bool found = false;
        for (std::size_t n = 0; n < gear.size() && !found; ++n)
        {
            found = !matched[n] && gear[n].stored.type == item.stored.type;
            matched[n] = matched[n] || found;
        }
        if (!found)
        {
            return false;
        }
    }
    return true;
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

// The containers the codec visits, const or not: the writer sees const values.
// Arrays of a fixed size are written element by element, in order. An
// AbilityArray is one: its six values go out from Strength to Charisma.
template <class T> constexpr bool is_fixed_array = false;
template <class T, std::size_t N> constexpr bool is_fixed_array<std::array<T, N>> = true;
template <class T> constexpr bool is_fixed_array<rules::AbilityArray<T>> = true;
template <class T> constexpr bool is_std_vector = false;
template <class T> constexpr bool is_std_vector<std::vector<T>> = true;
template <class T> constexpr bool is_std_map = false;
template <class K, class V> constexpr bool is_std_map<std::map<K, V>> = true;
template <class T> constexpr bool is_std_optional = false;
template <class T> constexpr bool is_std_optional<std::optional<T>> = true;
} // namespace

// Explicit field encoding: no pointers, native object layouts or derived sheets.
// One function per type serves both directions, which keeps the two formats in
// step. The writer visits const values, so saving copies nothing it encodes
// (Effective C++ Item 3).
template <bool Reading> struct SaveCodec
{
    static constexpr bool reading = Reading;
    // What a visit receives: a value to fill when reading, one to encode when writing.
    template <class T> using Visited = std::conditional_t<reading, T &, const T &>;
    std::stringstream stream;
    const rules::CharacterRules *creation{};
    const rules::RulesModule *module{};
    // Set when the writer describes a save's layout instead of writing one
    // (campaign_save_schema): each value adds a line naming its type, each
    // collection or optional value the layout of one element, indented, and
    // each part only some saves have the condition it is written under.
    bool describing{};
    std::string schema;
    unsigned depth{};

    explicit SaveCodec(std::string_view bytes)
        requires reading
        : stream(std::string(bytes))
    {
        stream.imbue(std::locale::classic());
    }

    SaveCodec()
        requires (!reading)
    {
        stream.imbue(std::locale::classic());
    }

    template <class... T> void fields(T &...value)
    {
        (field(value), ...);
    }

    void describe(std::string_view line)
    {
        schema.append(2 * std::size_t{depth}, ' ').append(line).append("\n");
    }

    // Describing, a collection's line and the layout of one element under it.
    // The elements are copies, so the reader, which never describes, compiles
    // the call too.
    template <class... T> void describe_element(std::string_view collection, T... element)
    {
        describe(collection);
        ++depth;
        fields(element...);
        --depth;
    }

    // A part of the layout only some saves have. Writing or reading, it is
    // visited when present; describing, always, under a line naming its condition.
    class Part
    {
      public:
        Part(SaveCodec &codec, std::string_view condition, bool present)
            : codec_(codec), present_(present)
        {
            if (codec_.describing)
            {
                codec_.describe("if " + std::string(condition) + ":");
                ++codec_.depth;
            }
        }

        Part(const Part &) = delete;
        Part &operator=(const Part &) = delete;

        ~Part()
        {
            if (codec_.describing)
            {
                --codec_.depth;
            }
        }

        explicit operator bool() const noexcept
        {
            return present_ || codec_.describing;
        }

      private:
        SaveCodec &codec_;
        bool present_;
    };

    [[nodiscard]] Part part(std::string_view condition, bool present)
    {
        return Part(*this, condition, present);
    }

    // The name of an integer type in a described layout.
    template <class T> static std::string integer_type()
    {
        if constexpr (std::is_same_v<T, bool>)
        {
            return "bool";
        }
        else
        {
            return (std::is_signed_v<T> ? "int" : "uint") + std::to_string(8 * sizeof(T));
        }
    }

    template <class T>
        requires std::is_integral_v<T>
    void field(T &value)
    {
        if constexpr (reading)
        {
            if constexpr (std::is_signed_v<T>)
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
                std::uint64_t n{};
                const auto end = token.data() + token.size();
                const auto [stop, error] = std::from_chars(token.data(), end, n);
                require(stop == end && error == std::errc{} &&
                            n <= static_cast<std::uint64_t>(std::numeric_limits<T>::max()),
                        "Save integer out of range");
                value = static_cast<T>(n);
            }
        }
        else if (describing)
        {
            describe(integer_type<std::remove_const_t<T>>());
            return;
        }
        else if constexpr (std::is_signed_v<T>)
        {
            stream << static_cast<std::int64_t>(value) << ' ';
        }
        else
        {
            stream << static_cast<std::uint64_t>(value) << ' ';
        }
        require(static_cast<bool>(stream), "Truncated or invalid campaign save");
    }

    void field(Visited<std::string> value)
    {
        if constexpr (reading)
        {
            stream >> std::quoted(value);
        }
        else if (describing)
        {
            describe("string");
            return;
        }
        else
        {
            stream << std::quoted(value) << ' ';
        }
        require(static_cast<bool>(stream) && value.size() <= limit, "Invalid save string");
    }

    template <class Array>
        requires is_fixed_array<std::remove_const_t<Array>>
    void field(Array &values)
    {
        if (describing)
        {
            const auto size = std::distance(std::begin(values), std::end(values));
            describe_element("array " + std::to_string(size), *std::begin(values));
            return;
        }
        for (auto &v : values)
        {
            field(v);
        }
    }

    template <class Vector>
        requires is_std_vector<std::remove_const_t<Vector>>
    void field(Vector &values)
    {
        if (describing)
        {
            describe_element("list", typename Vector::value_type{});
            return;
        }
        std::uint64_t count = values.size();
        field(count);
        require(count <= 65536, "Save collection too large");
        if constexpr (reading)
        {
            values.clear();
            for (std::uint64_t i = 0; i < count; ++i)
            {
                typename Vector::value_type v{};
                field(v);
                values.push_back(std::move(v));
            }
        }
        else
        {
            for (auto &v : values)
            {
                field(v);
            }
        }
    }

    template <class Map>
        requires is_std_map<std::remove_const_t<Map>>
    void field(Map &values)
    {
        if (describing)
        {
            describe_element("map", typename Map::key_type{}, typename Map::mapped_type{});
            return;
        }
        std::uint64_t count = values.size();
        field(count);
        require(count <= 65536, "Save map too large");
        if constexpr (reading)
        {
            values.clear();
            for (std::uint64_t i = 0; i < count; ++i)
            {
                typename Map::key_type k{};
                typename Map::mapped_type v{};
                fields(k, v);
                require(values.emplace(k, std::move(v)).second, "Duplicate save map key");
            }
        }
        else
        {
            for (const auto &[key, v] : values)
            {
                fields(key, v);
            }
        }
    }

    template <class Optional>
        requires is_std_optional<std::remove_const_t<Optional>>
    void field(Optional &value)
    {
        if (describing)
        {
            describe_element("optional", typename Optional::value_type{});
            return;
        }
        bool present = value.has_value();
        field(present);
        if constexpr (reading)
        {
            if (present)
            {
                value.emplace();
            }
            else
            {
                value.reset();
            }
        }
        if (present)
        {
            field(*value);
        }
    }

    void field(Visited<PartySlot> v)
    {
        field(v.index);
    }

    void field(Visited<rules::Identity> v)
    {
        fields(v.module, v.version, v.content);
    }

    void field(Visited<rules::AbilityRoll> v)
    {
        fields(v.dice, v.discarded);
    }

    void field(Visited<rules::FeatureGrant> v)
    {
        fields(v.id, v.source_id, v.level, v.choices);
    }

    void field(Visited<rules::AdvancementChoice> v)
    {
        fields(v.character_class, v.feat, v.abilities, v.spells, v.training, v.spell_learning,
               v.fighting_style, v.cantrip_swap, v.spell_swap, v.option_swap, v.discovery_swap,
               v.rules_data);
    }

    void field(Visited<rules::LevelSwap> v)
    {
        fields(v.replaced, v.replacement);
    }

    void field(Visited<rules::CharacterDraft> v)
    {
        fields(v.race, v.gender, v.character_class, v.alignment, v.background, v.name,
               v.target_classes, v.rolls, v.assignment, v.adjustment, v.rolled, v.training,
               v.cantrips, v.spells, v.rules_data);
    }

    void field(Visited<rules::SpellChoices> v)
    {
        fields(v.learning, v.prepared, v.replace_cantrip, v.replacement);
    }

    void field(Visited<SpellChoiceEdit> v)
    {
        fields(v.level, v.rest_session, v.choices);
    }

    void field(Visited<TrainingChoiceEdit> v)
    {
        fields(v.level, v.rest_session, v.selections);
    }

    void field(Visited<HallTraining> v)
    {
        fields(v.level, v.grant);
    }

    void field(Visited<por::CharacterAppearance> v)
    {
        // 0 short, 1 regular, 2 large: the former tall flag's 0 and 1 read unchanged.
        auto icon_size = static_cast<unsigned>(v.icon_size);
        fields(v.portrait_head, v.portrait_body, v.combat_head, v.combat_body, icon_size, v.colors,
               v.portrait);
        if constexpr (reading)
        {
            require(icon_size <= static_cast<unsigned>(por::IconSize::large), "Invalid icon size");
            v.icon_size = static_cast<por::IconSize>(icon_size);
        }
    }

    void field(Visited<InventoryItem> v)
    {
        fields(v.id, v.definition_id, v.name, v.quantity, v.original_type);
    }

    void field(Visited<Inventory> v)
    {
        fields(v.next_id_, v.items_);
        if constexpr (reading)
        {
            std::set<std::uint64_t> ids;
            require(v.next_id_ != 0, "Invalid next inventory ID");
            for (const auto &i : v.items_)
            {
                require(i.id && i.id < v.next_id_ && ids.insert(i.id).second && i.quantity &&
                            !i.definition_id.empty() && !i.name.empty(),
                        "Invalid inventory entry");
            }
        }
    }

    void field(Visited<por::DamageDice> v)
    {
        fields(v.count, v.sides, v.modifier);
    }

    void field(Visited<por::ItemRecord> v)
    {
        fields(v.raw, v.stored_name, v.type, v.name_components, v.magic_bonus, v.save_bonus,
               v.readied_raw, v.hidden_name_words, v.cursed_raw, v.weight, v.value, v.stack_size,
               v.effect_codes);
    }

    void field(Visited<por::ItemTemplate> v)
    {
        fields(v.raw, v.worn_location, v.hands, v.rate_of_fire, v.protection_raw, v.damage_type,
               v.melee_flag, v.large_damage, v.small_medium_damage, v.range, v.class_restrictions,
               v.properties);
    }

    void field(Visited<por::EquipmentBonuses> v)
    {
        fields(v.weapon_to_hit, v.weapon_damage, v.armor_base_ac, v.ac_adjustment, v.save_bonus);
    }

    void field(Visited<por::Equipment> v)
    {
        fields(v.index, v.stored, v.base, v.bonuses, v.decoded_name, v.unidentified_name);
    }

    void field(Visited<RestTicket> v)
    {
        fields(v.session, v.revision);
    }

    void field(Visited<ShortRestSession> v)
    {
        fields(v.ticket, v.completed_minutes, v.completed_subminute_milliseconds, v.members,
               v.rested_minutes, v.interrupted_minutes);
    }

    void rest(Visited<PartyState> v)
    {
        fields(v.next_rest_session, v.short_rest, v.spell_rest, v.training_rest,
               v.training_after_short_rest, v.spells_after_short_rest);
    }

    void field(Visited<GearPreference> v)
    {
        auto value = static_cast<std::uint8_t>(v);
        field(value);
        if constexpr (reading)
        {
            require(value <= static_cast<std::uint8_t>(GearPreference::damage),
                    "Invalid gear preference");
            v = static_cast<GearPreference>(value);
        }
    }

    void field(Visited<rules::VitalState> v)
    {
        fields(v.hit_points, v.dead, v.resources, v.description);
    }

    void member(Visited<PartyMember> v)
    {
        fields(v.id, v.npc_source, v.vitals, v.equipped, v.morale, v.experience,
               v.last_rest_minutes, v.item_sources, v.creation_source, v.quick,
               v.hall_experience_level, v.gear_preference);
        // Grants are derived by replaying creation and advancement. Storing them
        // lets a load detect rules that would rebuild a different character.
        auto grants = v.character.sheet().grants;
        field(grants);
        require(grants == v.character.sheet().grants,
                "Saved grants disagree with creation or advancement choices");
    }

    void field(Visited<PartyState> v)
    {
        fields(v.slots, v.next_id, v.selected_slot, v.leader, v.quick_magic, v.wealth,
               v.time_minutes, v.detect_magic_until, v.friends_member, v.friends_bonus,
               v.friends_until, v.find_traps_until, v.random_state.value, v.claimed_rewards);
        std::map<MemberId, unsigned> rest_offsets;
        if constexpr (!reading)
        {
            for (const auto &member : v.roster)
            {
                if (member.last_rest_subminute_milliseconds)
                {
                    rest_offsets.emplace(member.id, member.last_rest_subminute_milliseconds);
                }
            }
        }
        fields(v.subminute_milliseconds, v.next_combat_scope, rest_offsets);
        std::uint64_t count = v.roster.size();
        if (describing)
        {
            // One member stands for all, so there must be one to visit.
            require(count != 0, "Describing a save needs a party member");
            describe("list");
            ++depth;
            count = 1;
        }
        else
        {
            field(count);
        }
        require(count <= 128, "Too many saved party members");
        if constexpr (reading)
        {
            v.roster.clear();
            for (std::uint64_t i = 0; i < count; ++i)
            {
                rules::CharacterDraft draft;
                por::CharacterAppearance appearance;
                int level{};
                fields(draft, appearance, level);
                // The rules module bounds each class's level; this bounds the format:
                // Pools of Darkness's tables run to level 40.
                constexpr unsigned highest_level = 40;
                require(level >= 1 && level <= static_cast<int>(highest_level),
                        "Unsupported saved character level");
                std::vector<rules::AdvancementChoice> history;
                std::vector<SpellChoiceEdit> edits;
                std::vector<TrainingChoiceEdit> training;
                std::vector<HallTraining> hall;
                fields(history, edits, training, hall);
                // History includes original-rule calendar changes as well
                // as levels, drains and raising. Long Krynn campaigns can
                // legitimately exceed a thousand entries. The collection
                // codec already bounds this to 65,536, and replay below
                // checks the resulting character level.
                // Entries keep the order they were made in, not of their levels:
                // a level drain (the original rules') may bring a character back
                // to a level it has left (Character::replay).
                const auto valid_level = [](unsigned entry_level)
                {
                    return entry_level >= 1 && entry_level <= highest_level;
                };
                std::uint64_t prior_session = 0;
                for (const auto &edit : training)
                {
                    require(valid_level(edit.level) && edit.rest_session > prior_session,
                            "Invalid training replacement history");
                    prior_session = edit.rest_session;
                }
                for (const auto &trained : hall)
                {
                    require(valid_level(trained.level), "Invalid training hall history");
                }
                std::uint64_t rest = 0;
                for (const auto &edit : edits)
                {
                    require(valid_level(edit.level), "Invalid spell-choice history level");
                    require(edit.rest_session > rest, "Repeated spell-choice rest");
                    rest = edit.rest_session;
                }
                // Built once the history is known to be sound: evaluating the
                // character is the expensive part (Effective C++ Item 26).
                Character character(*creation, std::move(draft), std::move(appearance));
                character.replay(*module, history, edits, training, hall);
                require(character.sheet().level == level,
                        "Saved advancement history disagrees with level");
                Inventory inventory;
                field(inventory);
                character.replace_inventory(std::move(inventory));
                PartyMember m{0, std::move(character)};
                member(m);
                v.roster.push_back(std::move(m));
            }
        }
        else
        {
            for (std::uint64_t i = 0; i < count; ++i)
            {
                const auto &m = v.roster[i];
                const auto &draft = m.character.creation_data();
                const auto &appearance = m.character.appearance();
                auto level = m.character.sheet().level;
                fields(draft, appearance, level);
                const auto &history = m.character.advancements();
                const auto &edits = m.character.spell_edits();
                const auto &training = m.character.training_edits();
                const auto &hall = m.character.hall_training();
                fields(history, edits, training, hall);
                field(m.character.inventory());
                member(m);
            }
        }
        if (describing)
        {
            --depth;
        }
        if constexpr (reading)
        {
            for (const auto &[id, offset] : rest_offsets)
            {
                auto member = std::find_if(v.roster.begin(), v.roster.end(),
                                           [&](const auto &m)
                                           {
                                               return m.id == id;
                                           });
                require(member != v.roster.end() && member->last_rest_minutes && offset < 60000,
                        "Invalid rest time offset");
                member->last_rest_subminute_milliseconds = offset;
            }
        }
    }

    // A script's variables, written as a map's entries are (the count, then
    // each address and value in address order) from the machine's sorted
    // table.
    template <class Table> void variable_table(Table &table)
    {
        if (describing)
        {
            describe_element("map", std::uint16_t{}, std::uint16_t{});
            return;
        }
        std::uint64_t count = table.size();
        field(count);
        require(count <= 65536, "Save map too large");
        if constexpr (reading)
        {
            table.clear();
            for (std::uint64_t i = 0; i < count; ++i)
            {
                std::uint16_t address{}, value{};
                fields(address, value);
                table.emplace_back(address, value);
            }
            std::sort(table.begin(), table.end());
            require(std::adjacent_find(table.begin(), table.end(),
                                       [](const auto &a, const auto &b)
                                       {
                                           return a.first == b.first;
                                       }) == table.end(),
                    "Duplicate save map key");
        }
        else
        {
            for (const auto &[address, value] : table)
            {
                fields(address, value);
            }
        }
    }

    void machine(Visited<por::EclMachine> v)
    {
        require(v.state_ == por::EclState::idle || v.state_ == por::EclState::completed || reading,
                "Cannot save a pending script");
        variable_table(v.variables_);
        fields(v.image_, v.instruction_spans_, v.conditions_, v.next_request_,
               v.total_instructions_);
        std::string rng;
        if constexpr (!reading)
        {
            rng = v.random_.state_text();
        }
        field(rng);
        if constexpr (reading)
        {
            require(v.image_.size() == v.program_->raw().size(), "Script image size mismatch");
            const auto dice = MersenneTwister::from_state_text(rng);
            require(dice.has_value(), "Invalid saved script RNG");
            v.random_ = *dice;
            v.state_ = por::EclState::completed;
            v.pending_.reset();
            v.destination_.reset();
            v.menu_values_.clear();
            v.stack_.clear();
            v.trace_.clear();
            v.diagnostic_.clear();
        }
    }

    // The town session's fields (RolfTourSession::save).
    // The program a machine runs, and its place in it (Matrix's terminal
    // PROGRAM 2).
    static const por::EclProgram &program(const por::EclMachine &machine)
    {
        return *machine.program_;
    }

    void machine_address(Visited<por::EclMachine> v)
    {
        auto address = v.address();
        field(address);
        if constexpr (reading)
        {
            v.pc_ = address;
        }
    }

    // The town session's fields (RolfTourSession::save).
    void town(Visited<por::RolfTourSession> v)
    {
        por::RolfTourSession::save(*this, v);
    }
};

namespace por
{
template <class Codec, class Camp> void CampService::save(Codec &codec, Camp &camp)
{
    codec.field(camp.rest_checks_);
}

template <class Codec, class Vault> void VaultService::save(Codec &codec, Vault &vault)
{
    codec.fields(vault.dark_queen_vault_money_, vault.dark_queen_vault_items_);
    require(vault.dark_queen_vault_items_.size() <= 50, "Native vault is over capacity");
}

template <class Codec, class Treasure, class EncounterLoot>
void TreasureService::save(Codec &codec, Treasure &treasure, const EncounterLoot &encounter_loot)
{
    std::uint64_t pending = treasure.pending_loot_.size();
    // Describing a save's layout, one entry stands for all.
    if (codec.describing)
    {
        codec.describe("list");
        ++codec.depth;
        pending = 1;
    }
    else
    {
        codec.field(pending);
    }
    require(pending <= 1024, "Too many pending rewards");
    if constexpr (Codec::reading)
    {
        treasure.pending_loot_.clear();
    }
    for (std::uint64_t index = 0; index < pending; ++index)
    {
        std::vector<unsigned> records;
        std::string reward;
        bool items = true;
        std::array<unsigned, 7> wealth{};
        unsigned loot_area{};
        if (!Codec::reading && !codec.describing)
        {
            const auto &loot = treasure.pending_loot_[index];
            records = loot.records;
            reward = loot.reward_id;
            items = loot.include_items;
            wealth = loot.wealth;
            loot_area = loot.area;
        }
        codec.fields(records, reward, items, wealth, loot_area);
        // Script treasure has no creature records, and monsters may have
        // used some of their gear (DIFF-1): the items are saved.
        std::vector<por::Equipment> saved_items;
        if (!Codec::reading && !codec.describing)
        {
            saved_items = treasure.pending_loot_[index].items;
        }
        codec.field(saved_items);
        if (records.empty())
        {
            if constexpr (Codec::reading)
            {
                require(!reward.empty() && reward.size() <= 160 && !items &&
                            saved_items.size() <= 127,
                        "Invalid pending script treasure");
                treasure.pending_loot_.push_back(
                    {wealth, std::move(saved_items), std::move(reward), {}, false});
            }
        }
        else if constexpr (Codec::reading)
        {
            treasure.pending_loot_.push_back(
                encounter_loot(loot_area, std::move(records), std::move(reward), items));
            auto &loot = treasure.pending_loot_.back();
            require(loot.wealth == wealth && original_gear_left(loot.items, saved_items),
                    "Saved loot does not match its original creatures");
            loot.items = std::move(saved_items);
        }
    }
    if (codec.describing)
    {
        --codec.depth;
    }
}

template <class Codec, class Doors> void DoorHost::save(Codec &codec, Doors &doors)
{
    // The hidden entrances found or opened (SECRET-1), by their shop's id.
    std::map<std::string, unsigned> entrances;
    if constexpr (!Codec::reading)
    {
        for (const auto &[id, state] : doors.entrances_)
        {
            entrances[id] = static_cast<unsigned>(state);
        }
    }
    codec.field(entrances);
    if constexpr (Codec::reading)
    {
        doors.entrances_.clear();
        for (const auto &[id, state] : entrances)
        {
            require(state <= static_cast<unsigned>(EntranceState::open), "Invalid saved entrance");
            doors.entrances_[id] = static_cast<EntranceState>(state);
        }
    }
    // The traps a Wand of Secrets pointed out (SECRET-1).
    std::vector<unsigned> wand_traps(doors.wand_traps_.begin(), doors.wand_traps_.end());
    codec.field(wand_traps);
    if constexpr (Codec::reading)
    {
        require(wand_traps.size() <= 256 * 256, "Too many saved wand traps");
        doors.wand_traps_ = std::set<unsigned>(wand_traps.begin(), wand_traps.end());
    }
}

template <class Codec, class Tour> void RolfTourSession::save(Codec &codec, Tour &tour)
{
    require(Codec::reading || (tour.can_leave() && tour.snapshot_.tour_finished &&
                               !tour.event_checkpoint_ && !tour.pending_movement_),
            "Save only during idle town exploration");
    codec.fields(tour.current_script_, tour.selected_character_, tour.next_ticket_);
    CampService::save(codec, tour.camp_service_);
    codec.fields(tour.travel_mode_, tour.snapshot_.searching);
    if (const auto native =
            codec.part("Dark Queen of Krynn", tour.game() == GoldBoxGame::dark_queen_of_krynn))
    {
        codec.field(tour.dark_queen_combat_set_);
        require(tour.dark_queen_combat_set_ <= 255, "Invalid native combat tile set");
        VaultService::save(codec, tour.vault_service_);
    }
    // On the wilderness map only in a wilderness script, with its data.
    require(tour.travel_mode_ >= 1 && tour.travel_mode_ <= 4 &&
                (tour.travel_mode_ == 1 || (tour.current_script_ == tour.travel_mode_ + 23 &&
                                            tour.town_ && tour.town_->overland)),
            "Unsupported saved travel mode");
    unsigned area = tour.current_area_;
    codec.field(area);
    const auto wilderness = [&]()
    {
        const auto controller = buck_wilderness(tour.game(), tour.current_script_);
        if (tour.game() != GoldBoxGame::countdown_to_doomsday)
        {
            return controller;
        }
        // Countdown's first descent has no retained district and keeps
        // previous script 34. Preserve that ordinary supported save; only
        // its later retained-map mismatch needs this additional receipt.
        if (!tour.town_ || area == 0)
        {
            return std::optional<BuckWilderness>{};
        }
        const auto district = tour.town_->districts.find(area);
        if (district != tour.town_->districts.end() &&
            district->second->loaded_by(tour.current_script_))
        {
            return std::optional<BuckWilderness>{};
        }
        return controller;
    }();
    // Only Dark Queen has larger maps. Every other game keeps the original
    // 256 characters.
    const std::size_t knowledge_size =
        tour.game() == GoldBoxGame::dark_queen_of_krynn
            ? por::GeoMap::max_cells
            : por::GeoMap::default_width * por::GeoMap::default_height;
    const auto stored_knowledge = [knowledge_size](const por::MapKnowledge &cells)
    {
        const auto bits = cells.to_string();
        return bits.substr(bits.size() - knowledge_size);
    };
    std::map<unsigned, std::string> explored;
    if constexpr (!Codec::reading)
    {
        for (const auto &[id, cells] : tour.visited_areas_)
        {
            explored[id] = stored_knowledge(cells);
        }
        explored[tour.current_area_] = stored_knowledge(tour.snapshot_.visited);
    }
    codec.field(explored);
    if constexpr (Codec::reading)
    {
        require(tour.town_ && (area == 0 || tour.town_->districts.contains(area)),
                "Unsupported saved district");
        // Pools of Darkness's area map runs its own script (17) over the
        // map the party left, which that script does not load.
        const bool darkness = tour.game() == GoldBoxGame::pools_of_darkness;
        // Champions' wilderness (16) and outposts (17) also retain
        // the last 3D district while running their own scripts.
        const bool champions_overland = (tour.game() == GoldBoxGame::champions_of_krynn ||
                                         tour.game() == GoldBoxGame::death_knights_of_krynn) &&
                                        (tour.current_script_ == 16 || tour.current_script_ == 17);
        require(area == 0 ? !std::any_of(tour.town_->districts.begin(), tour.town_->districts.end(),
                                         [&](const auto &d)
                                         {
                                             return d.second->loaded_by(tour.current_script_);
                                         })
                          : darkness || champions_overland || wilderness.has_value() ||
                                tour.town_->districts.at(area)->loaded_by(tour.current_script_),
                "Saved district and script disagree");
        tour.current_area_ = 0;
        tour.snapshot_.area_id = 0;
        if (tour.town_->map)
        {
            tour.map_ = *tour.town_->map;
            tour.wall_art_ = tour.town_->wall_art;
        }
        tour.change_area(area);
        tour.visited_areas_.clear();
        for (const auto &[id, cells] : explored)
        {
            require((id == 0 || tour.town_->districts.contains(id)) &&
                        cells.size() == knowledge_size &&
                        cells.find_first_not_of("01") == std::string::npos,
                    "Invalid saved district exploration");
            tour.visited_areas_[id] = por::MapKnowledge(cells);
        }
    }
    // Word of Recall's sanctuary: a town script and a square (LEVELS-3).
    bool sanctuary = tour.sanctuary_.has_value();
    unsigned sanctuary_script = sanctuary ? tour.sanctuary_->script : 0,
             sanctuary_x = sanctuary ? tour.sanctuary_->pose.x : 0,
             sanctuary_y = sanctuary ? tour.sanctuary_->pose.y : 0,
             sanctuary_facing =
                 sanctuary ? static_cast<unsigned>(por::index(tour.sanctuary_->pose.facing)) : 0;
    codec.fields(sanctuary, sanctuary_script, sanctuary_x, sanctuary_y, sanctuary_facing);
    if constexpr (Codec::reading)
    {
        unsigned sanctuary_width = por::GeoMap::default_width,
                 sanctuary_height = por::GeoMap::default_height;
        if (tour.game() == GoldBoxGame::dark_queen_of_krynn)
        {
            for (const auto &[id, district] : tour.town_->districts)
            {
                if (district->loaded_by(sanctuary_script) && district->map)
                {
                    sanctuary_width = district->map->width;
                    sanctuary_height = district->map->height;
                    break;
                }
            }
        }
        require(!sanctuary || (tour.town_->programs.contains(sanctuary_script) &&
                               sanctuary_x < sanctuary_width && sanctuary_y < sanctuary_height &&
                               sanctuary_facing < 4),
                "Invalid saved sanctuary");
        tour.sanctuary_.reset();
        if (sanctuary)
        {
            tour.sanctuary_ =
                Sanctuary{sanctuary_script,
                          {sanctuary_x, sanctuary_y,
                           *por::map_direction(static_cast<std::uint16_t>(sanctuary_facing))}};
        }
    }
    DoorHost::save(codec, tour.doors_);
    if constexpr (Codec::reading)
    {
        tour.doors_.apply_entrances(tour);
    }
    TreasureService::save(
        codec, tour.treasure_service_,
        [&tour](unsigned area, std::vector<unsigned> records, std::string reward, bool items)
        {
            return tour.encounter_loot(area, std::move(records), std::move(reward), items);
        });
    if constexpr (Codec::reading)
    {
        // A script's scan of the party may stop on slot 8, which no member
        // fills (ecl_scanned_slots).
        require(tour.town_ && tour.town_->programs.contains(tour.current_script_) &&
                    tour.selected_character_.index <= ecl_scanned_slots &&
                    tour.camp_service_.rest_checks() <= std::numeric_limits<std::uint16_t>::max(),
                "Unsupported saved town context");
        tour.machine_ = por::EclMachine(tour.town_->programs.at(tour.current_script_));
        tour.enable_encounter_commands();
        tour.configure_town();
    }
    codec.machine(tour.machine_);
    if constexpr (Codec::reading)
    {
        if (wilderness)
        {
            // Only defer the map association until the restored VM can
            // authenticate this installed controller and its presentation.
            const auto bank = tour.town_->script_banks.find(tour.current_script_);
            bool controller_matches = false;
            try
            {
                const auto instruction =
                    Codec::program(tour.machine_).instruction(wilderness->kind_instruction);
                controller_matches = Codec::program(tour.machine_).game() == tour.game() &&
                                     instruction.opcode == 9 && instruction.operands.size() == 2 &&
                                     instruction.operands[0].tag == 0 &&
                                     instruction.operands[0].value == wilderness->kind &&
                                     instruction.operands[1].tag == 1 &&
                                     instruction.operands[1].value == 0x4BE6;
            }
            catch (const por::EclError &)
            {
                // A different installed program is not this controller.
            }
            require(controller_matches && bank != tour.town_->script_banks.end() &&
                        bank->second == wilderness->bank &&
                        tour.machine_.variable(tour.game_cell(por::ecl_script_bank)) ==
                            wilderness->bank &&
                        (tour.machine_.variable(por::EclAddress{0x4BF2}) == tour.current_script_ ||
                         (tour.machine_.variable(por::EclAddress{0x4BF2}) != 0 &&
                          std::ranges::find(wilderness->return_scripts,
                                            tour.machine_.variable(por::EclAddress{0x4BF2})) !=
                              wilderness->return_scripts.end()) ||
                         (tour.game() == GoldBoxGame::matrix_cubed &&
                          matrix_venus_arrival(Codec::program(tour.machine_), tour.current_script_,
                                               tour.machine_.variable(por::EclAddress{0x4BF2}),
                                               tour.machine_.variable(por::EclAddress{0x4BC3}),
                                               tour.machine_.variable(por::EclAddress{0x4BC4})))) &&
                        tour.machine_.variable(por::EclAddress{0x4BE6}) == wilderness->kind &&
                        tour.machine_.variable(por::EclAddress{0x4BA6}) == 4,
                    tour.game() == GoldBoxGame::matrix_cubed
                        ? "Invalid saved Matrix wilderness context"
                        : "Saved district and script disagree");
        }
    }
    bool matrix_won = false;
    const bool matrix = tour.game() == GoldBoxGame::matrix_cubed;
    if (const auto matrix_part = codec.part("Matrix Cubed", matrix))
    {
        if constexpr (!Codec::reading)
        {
            matrix_won = matrix && tour.matrix_ending_completed();
        }
        codec.field(matrix_won);
        if (const auto won = codec.part("Matrix Cubed won", matrix_won))
        {
            codec.machine_address(tour.machine_);
            if constexpr (Codec::reading)
            {
                require(tour.matrix_ending_completed(), "Invalid Matrix terminal victory receipt");
            }
        }
    }
    if constexpr (Codec::reading)
    {
        tour.encounters_.forget_for_load();
        tour.treasure_service_.forget_staged();
        tour.doors_.forget_menu();
    }
    auto &s = tour.snapshot_;
    codec.fields(s.dialogue, s.prompts, s.redraws, s.footsteps, s.event_runs);
    std::string visited = stored_knowledge(s.visited);
    codec.field(visited);
    if constexpr (Codec::reading)
    {
        require(visited.size() == knowledge_size &&
                    visited.find_first_not_of("01") == std::string::npos,
                "Invalid visited map");
        s.visited = por::MapKnowledge(visited);
    }
    std::map<unsigned, std::string> known;
    if constexpr (!Codec::reading)
    {
        for (const auto &[id, cells] : tour.seen_areas_)
        {
            known[id] = stored_knowledge(cells);
        }
        known[tour.current_area_] = stored_knowledge(s.seen);
    }
    codec.field(known);
    if constexpr (Codec::reading)
    {
        tour.seen_areas_.clear();
        for (const auto &[id, cells] : known)
        {
            require((id == 0 || tour.town_->districts.contains(id)) &&
                        cells.size() == knowledge_size &&
                        cells.find_first_not_of("01") == std::string::npos,
                    "Invalid saved map knowledge");
            tour.seen_areas_[id] = por::MapKnowledge(cells);
        }
        require(tour.seen_areas_.contains(tour.current_area_), "Missing current map knowledge");
        s.seen = tour.seen_areas_.at(tour.current_area_);
        require((s.visited & ~s.seen).none(), "Visited cells must be known");
        for (const auto &[id, cells] : tour.visited_areas_)
        {
            require(tour.seen_areas_.contains(id) && (cells & ~tour.seen_areas_.at(id)).none(),
                    "Missing visited district knowledge");
        }
    }
    if constexpr (Codec::reading)
    {
        s.phase = por::TourPhase::completed;
        s.tour_finished = true;
        s.script_id = tour.current_script_;
        s.sprite_frame = -1;
        s.choices.clear();
        s.continue_ticket = 0;
        s.diagnostic.clear();
        ++s.picture_revision;
        tour.picture_.reset();
        tour.event_checkpoint_.reset();
        tour.pending_movement_.reset();
        tour.shop_service_.close();
        tour.who_slots_.clear();
        tour.member_chosen_ = false;
        tour.vault_service_.close();
        tour.temple_service_.close();
        tour.hall_service_.close();
        tour.menu_request_ = tour.delayed_request_ = tour.who_request_ = tour.damage_request_ = 0;
        tour.damage_writes_.clear();
        tour.victory_ = false;
        tour.remaining_delay_ = 0;
        tour.transition_ = tour.message_only_ = false;
        tour.event_stage_ = EventStage::none;
        tour.publish_pose();
        if (matrix)
        {
            s.game_won = matrix_won;
        }
        tour.campaign_.reset();
    }
}
} // namespace por

using SaveReader = SaveCodec<true>;
using SaveWriter = SaveCodec<false>;

namespace
{
// Saving must encode the caller's state in place rather than a copy of it.
template <class Codec>
concept encodes_const_campaign =
    requires (Codec &codec, const PartyState &party, const por::RolfTourSession &town) {
        codec.field(party);
        codec.rest(party);
        codec.town(town);
    };

static_assert(encodes_const_campaign<SaveWriter>);
static_assert(!encodes_const_campaign<SaveReader>, "Only the writer may visit const state");
} // namespace

namespace
{
// A campaign's fields after its header, written or, describing, walked.
void write_campaign(SaveWriter &out, const CampaignParty &party, const por::RolfTourSession *town,
                    std::string_view assets, GoldBoxGame game, const DesignSave *design)
{
    auto identity = party.identity();
    std::string asset(assets);
    const auto &state = party.state();
    auto game_number = static_cast<unsigned>(game);
    auto setting = party.game_setting();
    out.fields(identity, asset, game_number, setting);
    const DesignSave no_design;
    const auto &played = design ? *design : no_design;
    if (const auto designed = out.part("Unlimited Adventures design", design != nullptr))
    {
        out.field(played.folder);
    }
    out.field(state);
    bool has_town = town != nullptr;
    out.field(has_town);
    if (const auto in_town = out.part("town", has_town))
    {
        out.town(*town);
    }
    if (const auto designed = out.part("Unlimited Adventures design", design != nullptr))
    {
        out.field(played.state);
    }
    out.rest(state);
}

std::string with_header(std::string_view body)
{
    return std::string(campaign_magic) + std::to_string(save_version) + "\n" +
           std::to_string(fingerprint(body)) + "\n" + std::string(body);
}

// The refusal of a save of version `saved` (save_version_message).
std::string other_version_refusal(std::string_view saved)
{
    std::string message(save_version_message);
    for (const auto &[name, value] :
         {std::pair<std::string_view, std::string>{"{saved}", std::string(saved)},
          {"{version}", std::to_string(save_version)}})
    {
        message.replace(message.find(name), name.size(), value);
    }
    return message;
}
} // namespace

std::string encode_campaign(const CampaignParty &party, const por::RolfTourSession *town,
                            std::string_view assets, GoldBoxGame game, const DesignSave *design)
{
    const bool design_game = game == GoldBoxGame::unlimited_adventures;
    require(design_game == (design != nullptr) && !(design && town),
            "Only an Unlimited Adventures campaign records a design, and no town with it");
    require(!party.in_combat(), "Cannot save during combat");
    SaveWriter out;
    write_campaign(out, party, town, assets, game, design);
    auto body = out.stream.str();
    require(body.size() <= limit, "Campaign save too large");
    return with_header(body);
}

std::string campaign_save_schema(const CampaignParty &party, const por::RolfTourSession &town)
{
    SaveWriter out;
    out.describing = true;
    write_campaign(out, party, &town, {}, GoldBoxGame::pool_of_radiance, nullptr);
    return std::string(campaign_magic) + std::to_string(save_version) +
           "\nuint64 checksum of what follows\n" + out.schema;
}

std::optional<std::string> refused_save_version(std::string_view message)
{
    const auto refusal = other_version_refusal("{saved}");
    const auto at = refusal.find("{saved}");
    const auto before = std::string_view(refusal).substr(0, at);
    const auto after = std::string_view(refusal).substr(at + std::string_view("{saved}").size());
    if (message.size() <= before.size() + after.size() || !message.starts_with(before) ||
        !message.ends_with(after))
    {
        return std::nullopt;
    }
    const auto saved = message.substr(before.size(), message.size() - before.size() - after.size());
    if (saved.find_first_not_of("0123456789") != std::string_view::npos)
    {
        return std::nullopt;
    }
    return std::string(saved);
}

namespace
{
// The save's body, once its header and checksum hold. The version comes first:
// a save of any version but save_version is refused before anything else is read.
std::string_view checked_save(std::string_view bytes)
{
    require(bytes.size() <= limit, "Campaign save too large");
    const auto header_end = bytes.find('\n');
    require(bytes.starts_with(campaign_magic) && header_end != bytes.npos,
            "Unsupported campaign save version");
    const auto version = bytes.substr(campaign_magic.size(), header_end - campaign_magic.size());
    require(!version.empty() && version.size() <= 9 &&
                version.find_first_not_of("0123456789") == std::string_view::npos,
            "Unsupported campaign save version");
    if (version != std::to_string(save_version))
    {
        throw std::runtime_error(other_version_refusal(version));
    }
    const auto end = bytes.find('\n', header_end + 1);
    require(end != bytes.npos, "Truncated campaign save");
    const auto body = bytes.substr(end + 1);
    require(bytes.substr(header_end + 1, end - header_end - 1) == std::to_string(fingerprint(body)),
            "Campaign save checksum mismatch");
    return body;
}

void validate_saved_member(const PartyMember &member, const rules::RulesModule &module)
{
    module.validate_character_state(member.character.sheet(), member.vitals);
    for (const auto &[id, source] : member.item_sources)
    {
        const auto item = member.character.inventory().find(id);
        require(item.has_value() && item->get().definition_id == item_definition(module, source) &&
                    item->get().original_type == source.stored.type,
                "Saved item provenance mismatch");
    }
    for (const auto &item : member.character.inventory().items())
    {
        if (!member.item_sources.contains(item.id))
        {
            if (module.equipment_info(item.definition_id).slot == rules::EquipmentSlot::carried)
            {
                continue;
            }
            const std::array<std::string, 1> definition{item.definition_id};
            (void)module.character_profile(member.character.sheet(), definition);
        }
    }
    std::vector<std::string> gear;
    for (auto id : member.equipped)
    {
        gear.push_back(member.character.inventory().find(id)->get().definition_id);
    }
    (void)module.character_profile(member.character.sheet(), gear);
}
} // namespace

SavedCampaign decode_campaign(std::string_view bytes, const rules::CharacterRules &creation,
                              const rules::RulesModule &module, std::string_view assets,
                              const por::RolfTourSession *town_template)
{
    std::function<por::RolfTourSession()> make_town;
    if (town_template)
    {
        make_town = [town_template]
        {
            return town_template->detached_copy();
        };
    }
    return decode_campaign(bytes, creation, module, assets, make_town);
}

SavedCampaign decode_campaign(std::string_view bytes, const rules::CharacterRules &creation,
                              const rules::RulesModule &module, std::string_view assets,
                              const std::function<por::RolfTourSession()> &make_town_template)
{
    SaveReader in(checked_save(bytes));
    in.creation = &creation;
    in.module = &module;
    rules::Identity identity;
    std::string asset;
    in.fields(identity, asset);
    // Only the save version decides whether a save loads (docs/SAVES.md): the
    // rules module is checked because a save is read by the module it names,
    // not the module's version or content.
    require(identity.module == module.identity().module, "Campaign rules module mismatch");
    require(asset == assets, "Campaign original asset identity mismatch");
    SavedCampaign result;
    unsigned game_number{};
    in.field(game_number);
    const auto game = game_numbered(game_number);
    require(game.has_value(), "Unknown campaign game");
    result.game = *game;
    std::string setting;
    in.field(setting);
    require(setting == creation.game_setting(), "Campaign rules setting mismatch");
    const bool design = result.game == GoldBoxGame::unlimited_adventures;
    if (design)
    {
        result.design.emplace();
        in.field(result.design->folder);
    }
    in.field(result.party);
    CampaignParty::validate(result.party);
    for (const auto &m : result.party.roster)
    {
        validate_saved_member(m, module);
    }
    bool has_town{};
    in.field(has_town);
    if (has_town)
    {
        require(static_cast<bool>(make_town_template), "This save requires town resources");
        result.town = make_town_template();
        in.town(*result.town);
    }
    if (design)
    {
        require(!has_town, "A design's save has no town");
        in.field(result.design->state);
    }
    in.rest(result.party);
    CampaignParty::validate(result.party);
    CampaignParty::validate_rest_choices(result.party, module);
    in.stream >> std::ws;
    require(in.stream.eof(), "Trailing campaign save data");
    return result;
}

CampaignRules saved_campaign_rules(std::string_view bytes)
{
    SaveReader in(checked_save(bytes));
    CampaignRules saved;
    std::string asset;
    unsigned game_number{};
    in.fields(saved.identity, asset, game_number, saved.setting);
    const auto game = game_numbered(game_number);
    require(game.has_value(), "Unknown campaign game");
    saved.game = *game;
    if (saved.game == GoldBoxGame::unlimited_adventures)
    {
        in.field(saved.design_folder);
    }
    return saved;
}

void reseed_after_load(SavedCampaign &saved, Difficulty difficulty, std::uint64_t fresh)
{
    if (difficulty == Difficulty::easy)
    {
        return;
    }
    if (diagnostic_log_enabled())
    {
        log_diagnostic(LogPriority::info, "save", "new dice after load: " + std::to_string(fresh));
    }
    saved.party.random_state = RandomState{fresh};
    if (saved.town)
    {
        saved.town->reseed_dice(static_cast<std::uint32_t>(fresh ^ (fresh >> 32)));
    }
}

std::string read_campaign_file(const std::filesystem::path &path)
{
    return read_save_file(path, limit);
}

std::string campaign_asset_identity(const std::filesystem::path &directory)
{
    const ua::DesignFiles native(directory, directory);
    if (native.file_bytes("DQK.EXE"))
    {
        std::ostringstream out;
        for (const char *name :
             {"DQK.EXE",    "ITEMS.DAT", "ITEM.DAT", "ECL.GLB",     "GEO.GLB",     "MONCHA.GLB",
              "PICA.TLB",   "PICB.TLB",  "PICC.TLB", "BIGPIC.TLB",  "BACK.TLB",    "SPRIT.TLB",
              "8X8DB.TLB",  "8X8DC.TLB", "CPIC.TLB", "DUNGCOM.TLB", "WILDCOM.TLB", "COMSPR.TLB",
              "ALWAYS.TLB", "FINAL.TLB", "GEN.TLB",  "CBODY.TLB",   "GAME.FON"})
        {
            const auto bytes = native.file_bytes(name);
            if (!bytes)
            {
                throw std::runtime_error(std::string{"Missing Dark Queen asset "} + name);
            }
            const std::string_view content{reinterpret_cast<const char *>(bytes->data()),
                                           bytes->size()};
            out << "DQK:" << name << ':' << fingerprint(content) << ';';
        }
        return out.str();
    }
    // Unlimited Adventures' kit has no DAX archives: its saves hold the
    // kit's item types and items (docs/games/ua.md, Saving and loading a game).
    if (ua::is_kit_folder(directory))
    {
        std::ostringstream out;
        for (const char *name : {"ITEMS.DAT", "ITEM.DAT"})
        {
            out << "UA:" << name << ':'
                << fingerprint(read_campaign_file(directory / "DISK1" / name)) << ';';
        }
        return out.str();
    }
    std::map<std::string, std::uint64_t> files;
    for (const auto &entry : std::filesystem::directory_iterator(directory))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        auto name = path_text(entry.path().filename());
        for (auto &c : name)
        {
            if (c >= 'a' && c <= 'z')
            {
                c -= 32;
            }
        }
        // Gateway names its item types ITEMS.1 (item_types_file).
        if (name.ends_with(".DAX") || name == "ITEMS" || name == "ITEMS.1")
        {
            require(!files.contains(name), "Ambiguous original asset filename");
            files.emplace(name, fingerprint(read_campaign_file(entry.path())));
        }
    }
    // The games number their script and map files differently (Treasures
    // has only ECL1 and GEO1, Gateway ECL2 to ECL6): any one of each will do.
    const auto has_any = [&](std::string_view stem)
    {
        return std::ranges::any_of(files,
                                   [&](const auto &file)
                                   {
                                       return file.first.starts_with(stem) &&
                                              file.first.ends_with(".DAX");
                                   });
    };
    require(has_any("ECL") && has_any("GEO") &&
                (files.contains("ITEMS") || files.contains("ITEMS.1")),
            "Missing campaign assets");
    std::ostringstream out;
    for (auto &[name, hash] : files)
    {
        out << name << ':' << hash << ';';
    }
    return out.str();
}

void write_campaign_file(const std::filesystem::path &path, std::string_view bytes)
{
    write_save_file(path, bytes, limit);
}
} // namespace opengold
