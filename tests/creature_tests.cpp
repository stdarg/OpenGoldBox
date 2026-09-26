#include "opengold/creature_catalog.h"
#include "opengold/creature_factory.h"
#include "opengold/map_catalog.h"
#include "opengold/formats.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>

using namespace opengold;
using namespace opengold::por;
namespace
{
using Bytes = std::vector<std::uint8_t>;
void require(bool condition, std::string_view message)
{
    if (!condition)
        throw std::runtime_error(std::string(message));
}
void u16(Bytes &b, std::size_t p, unsigned n)
{
    b[p] = n & 255;
    b[p + 1] = (n >> 8) & 255;
}
void u32(Bytes &b, std::size_t p, unsigned n)
{
    u16(b, p, n);
    u16(b, p + 2, n >> 16);
}
Bytes dax(const std::vector<DaxRecord> &records)
{
    Bytes bytes(2 + records.size() * 9);
    u16(bytes, 0, static_cast<unsigned>(records.size() * 9));
    for (std::size_t i = 0; i < records.size(); ++i)
    {
        const auto &r = records[i];
        const auto start = bytes.size();
        for (std::size_t p = 0; p < r.bytes.size(); p += 128)
        {
            const auto count = std::min<std::size_t>(128, r.bytes.size() - p);
            bytes.push_back(static_cast<std::uint8_t>(count - 1));
            bytes.insert(bytes.end(), r.bytes.begin() + p, r.bytes.begin() + p + count);
        }
        const auto header = 2 + 9 * i;
        bytes[header] = r.id;
        u32(bytes, header + 1, static_cast<unsigned>(start - 2 - records.size() * 9));
        u16(bytes, header + 5, static_cast<unsigned>(r.bytes.size()));
        u16(bytes, header + 7, static_cast<unsigned>(bytes.size() - start));
    }
    return bytes;
}
Bytes character(std::string_view name = "TEST CREATURE")
{
    Bytes b(285);
    b[0] = static_cast<std::uint8_t>(name.size());
    std::copy(name.begin(), name.end(), b.begin() + 1);
    b[16] = 18;
    b[17] = 12;
    b[18] = 10;
    b[19] = 16;
    b[20] = 18;
    b[21] = 15;
    b[22] = 100;
    b[45] = 47;
    b[47] = 2;
    b[50] = 36;
    b[109] = 10;
    b[110] = 11;
    b[111] = 12;
    b[112] = 12;
    b[113] = 13;
    b[152] = 4;
    b[161] = 3;
    b[162] = 2;
    b[163] = 1;
    b[164] = 2;
    b[165] = 4;
    b[166] = 6;
    b[167] = 4;
    b[168] = 255;
    b[169] = 62;
    b[170] = 1;
    b[199] = 2;
    u32(b, 172, 123456);
    u16(b, 184, 500);
    b[186] = 5;
    b[283] = 250; // Retain high raw HP, never assume every value above 127 is a dead character.
    return b;
}
void write(const std::filesystem::path &path, const Bytes &bytes)
{
    std::ofstream out(path, std::ios::binary);
    for (const auto b : bytes)
        out.put(static_cast<char>(b));
    if (!out)
        throw std::runtime_error("Cannot write test fixture");
}
class Fixture
{
  public:
    Fixture()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("opengold-creatures-" + std::to_string(suffix));
        require(std::filesystem::create_directory(path_), "Unique temporary directory");
    }
    ~Fixture()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    Fixture(const Fixture &) = delete;
    Fixture &operator=(const Fixture &) = delete;
    const std::filesystem::path &path() const
    {
        return path_;
    }
    void populate()
    {
        for (unsigned bank = 1; bank <= 8; ++bank)
        {
            const auto prefix = "MON" + std::to_string(bank);
            write(path_ / (prefix + "CHA.DAX"), dax({}));
            write(path_ / (prefix + "ITM.DAX"), dax({}));
            if (bank != 1 && bank != 3)
                write(path_ / (prefix + "SPC.DAX"), dax({}));
        }
        Bytes templates(2 + 128 * 16);
        auto p = 2 + 36 * 16;
        templates[p + 1] = 1;
        templates[p + 2] = 1;
        templates[p + 3] = 12;
        templates[p + 8] = 128;
        templates[p + 9] = 1;
        templates[p + 10] = 8;
        p = 2 + 55 * 16;
        templates[p] = 2;
        templates[p + 6] = 0xb7;
        p = 2 + 59 * 16;
        templates[p] = 1;
        templates[p + 6] = 0x81;
        p = 2 + 77 * 16;
        templates[p] = 0x4d;
        templates[p + 6] = 0xb2;
        p = 2 + 93 * 16;
        templates[p] = 9;
        templates[p + 6] = 0x80;
        write(path_ / "ITEMS", templates);
        write(path_ / "MON2CHA.DAX", dax({{31, character()}}));
        write(path_ / "MON5CHA.DAX",
              dax({{31, character()}})); // Same ID/name, different bank/equipment.
        Bytes items(126);
        items[46] = 36;
        items[50] = 1;
        items[52] = 1;
        items[63 + 46] = 55;
        items[63 + 50] = 1;
        items[63 + 52] = 0; // Carried armor.
        write(path_ / "MON2ITM.DAX", dax({{31, items}}));
        write(path_ / "MON2SPC.DAX",
              dax({{31, Bytes{0x64, 0, 0, 255, 0,    7,  0, 106, 25, 0x65, 0, 0, 255, 0,
                              0,    0, 0, 0,   0xee, 44, 1, 4,   2,  1,    2, 3, 4}}}));
    }

  private:
    std::filesystem::path path_;
};
template <class F> void fails(F operation, std::string_view context)
{
    try
    {
        operation();
    }
    catch (const CatalogError &e)
    {
        require(std::string_view(e.what()).find(context) != std::string_view::npos,
                "Failure includes source context");
        return;
    }
    throw std::runtime_error("Expected catalog error: " + std::string(context));
}

void parsing()
{
    const auto c = decode_character(character());
    require(c.has_value(), "Character parses");
    require(c->max_hit_points == 36 && c->base_armor_class == -2 && c->base_thac0 == 13,
            "HP, signed descending AC, THAC0");
    require(c->base_attacks[0].attacks_per_round() == 1.5 && c->base_attacks[1].damage.count == 2,
            "Half attacks and second slot");
    require(c->base_attacks[1].damage.modifier == -1, "Signed damage modifier");
    require(c->experience == 123456 && c->base_xp_award == 500 && c->saves.spells == 13,
            "Multi-byte XP and saves");
    require(c->current.armor_class == 60 && c->current.hit_points_raw == 250,
            "Uninitialized/current bytes not substituted for base");
    require(!decode_character(Bytes(284)) && !decode_character(Bytes(286)),
            "Wrong character sizes rejected");
    auto b = character();
    b[0] = 16;
    require(!decode_character(b), "Invalid name length rejected");
    require(!decode_items(Bytes(62)) && !decode_effects(Bytes(8)),
            "Truncated supplemental records rejected");
    require(decode_items({})->empty() && decode_effects({})->empty(),
            "Empty supplemental records valid");
    require(!decode_item_templates(Bytes(19)), "Invalid template length rejected");
    auto archive = dax({{1, character()}, {2, Bytes{42}}});
    require(decode_dax_archive(archive).records.size() == 2, "Complete DAX enumeration");
    archive.pop_back();
    require(!decode_dax_archive(archive), "Truncated second record fails entire archive");
    require(!decode_dax_archive(dax({{1, {}}, {1, {}}})), "Duplicate DAX IDs rejected");
    Bytes repeated{9, 0, 7, 0, 0, 0, 0, 128, 0, 2, 0, 128, 42};
    const auto decoded = decode_dax_archive(repeated);
    require(decoded && decoded.records[0].bytes == Bytes(128, 42), "RLE -128 repeats 128 bytes");
    repeated[7] = 127;
    require(!decode_dax_archive(repeated), "RLE overflow rejected");
    repeated[7] = 129;
    require(!decode_dax_archive(repeated), "RLE underflow rejected");
    auto invalid_offset = dax({{1, {1}}});
    u32(invalid_offset, 3, 0xffffffff);
    require(!decode_dax_archive(invalid_offset), "Large offset rejected");
}

void modifiers_and_effects()
{
    auto c = *decode_character(character());
    for (const auto [percent, hit, damage] : std::array<std::array<int, 3>, 11>{{{0, 1, 2},
                                                                                 {1, 1, 3},
                                                                                 {50, 1, 3},
                                                                                 {51, 2, 3},
                                                                                 {75, 2, 3},
                                                                                 {76, 2, 4},
                                                                                 {90, 2, 4},
                                                                                 {91, 2, 5},
                                                                                 {99, 2, 5},
                                                                                 {100, 3, 6},
                                                                                 {101, 0, 0}}})
    {
        c.abilities.exceptional_strength = static_cast<std::uint8_t>(percent);
        const auto m = ability_modifiers(c);
        if (percent == 101)
            require(!m.strength_damage, "Invalid percentile remains unknown");
        else
            require(m.strength_to_hit == hit && m.strength_damage == damage,
                    "Exceptional strength boundary");
    }
    c.abilities.strength = 19;
    c.strength_bonus_flag_raw = 0;
    auto m = ability_modifiers(c);
    require(m.strength_damage == 7 && m.strength_bonus_allowed_hint == false,
            "Monster applicability kept separate");
    require(m.dexterity_ac_adjustment == -2 && m.dexterity_missile_to_hit == 1,
            "DEX 16 combat reference");
    require(m.constitution_hp_per_hit_die == 4, "Fighter CON bonus");
    c.abilities.strength = 25;
    require(ability_modifiers(c).strength_damage == 14, "STR 25 reference damage");
    c.character_class = 5;
    require(ability_modifiers(c).constitution_hp_per_hit_die == 2, "Non-warrior CON cap");
    c.character_class = 8;
    require(!ability_modifiers(c).constitution_hp_per_hit_die &&
                ability_modifiers(c).constitution_hp_per_hit_die_by_class[0] == 2 &&
                ability_modifiers(c).constitution_hp_per_hit_die_by_class[2] == 4,
            "Multiclass CON retains each class rate, no misleading scalar");
    for (const auto [score, missile, ac] : std::array<std::array<int, 3>, 8>{{{3, -3, 4},
                                                                              {4, -2, 3},
                                                                              {5, -1, 2},
                                                                              {6, 0, 1},
                                                                              {7, 0, 0},
                                                                              {14, 0, 0},
                                                                              {15, 0, -1},
                                                                              {19, 3, -4}}})
    {
        c.abilities.dexterity = static_cast<std::uint8_t>(score);
        const auto dex = ability_modifiers(c);
        require(dex.dexterity_missile_to_hit == missile && dex.dexterity_ac_adjustment == ac,
                "DEX penalty/bonus boundaries");
    }
    c.abilities.dexterity = 0;
    require(!ability_modifiers(c).dexterity_ac_adjustment, "Unknown score not silently neutral");
    require(describe_effect(0x65).regeneration->hp_per_round == 3 &&
                describe_effect(0x65).regeneration->revival_delay_rounds->count == 3,
            "Pool troll regeneration");
    require(describe_effect(0x64).kind == EffectKind::vulnerability,
            "Troll fire/acid vulnerability");
    require(describe_effect(0x6a).resistance_percent == 100,
            "Pool magic resistance, not Curse's 15 percent");
    require(describe_effect(0x58).name == "Lightning breath",
            "Pool lightning breath, not GBE's guessed missile");
    require(describe_effect(0x73).incoming_damage_multiplier == 0.5,
            "Physical resistance magnitude");
    require(describe_effect(0x41).target_save_adjustment == 4 &&
                describe_effect(0x56).levels_drained == 2,
            "Attack effects");
    require(describe_effect(0xee).kind == EffectKind::unknown && describe_effect(0xee).code == 0xee,
            "Unknown code preserved");
}

void effect_contracts()
{
    // Exhaust the byte namespace: reserved codes must remain explicitly
    // unknown, and optional magnitudes must never acquire invented defaults.
    constexpr std::array<unsigned, 9> reserved{0x39, 0x3c, 0x3f, 0x4a, 0x4b,
                                               0x4e, 0x5c, 0x66, 0x69};
    const std::map<unsigned, int> saves{{0x40, 0}, {0x41, 4},  {0x42, 2}, {0x43, 0},
                                        {0x44, 0}, {0x45, -2}, {0x46, -2}};
    const std::map<unsigned, int> resistances{{0x6a, 100}, {0x6b, 90}, {0x7c, 30}};
    constexpr std::array<unsigned, 6> halves{0x5d, 0x5e, 0x72, 0x73, 0x74, 0x76};
    for (unsigned code = 0; code < 256; ++code)
    {
        const auto effect = describe_effect(static_cast<std::uint8_t>(code));
        const bool unknown =
            code >= 128 || std::find(reserved.begin(), reserved.end(), code) != reserved.end();
        require(effect.code == code && !effect.name.empty(),
                "Every effect preserves its code and display name");
        require((effect.kind == EffectKind::unknown) == unknown &&
                    (effect.name == "Unknown effect") == unknown,
                "Unrecognized Pool effect IDs retain explicit unknown metadata");
        require((effect.kind == EffectKind::none) == (code == 0),
                "Only zero is the no-effect sentinel");
        require(effect.target_save_adjustment ==
                    (saves.contains(code) ? std::optional(saves.at(code)) : std::nullopt),
                "Save adjustments preserve sign, including explicit zero versus absent");
        require(
            effect.resistance_percent ==
                (resistances.contains(code) ? std::optional(resistances.at(code)) : std::nullopt),
            "Only quantified resistances carry percentages");
        const bool half = std::find(halves.begin(), halves.end(), code) != halves.end();
        require(effect.incoming_damage_multiplier == (half ? std::optional(0.5) : std::nullopt),
                "Half damage metadata is limited to documented effects");
        if (half || resistances.contains(code))
            require(effect.kind == EffectKind::resistance && !effect.subject.empty(),
                    "Resistance identifies its damage or effect subject");
        const auto drain = code == 0x55   ? std::optional(1)
                           : code == 0x56 ? std::optional(2)
                                          : std::nullopt;
        require(effect.levels_drained == drain, "Only level-drain attacks carry a drain count");
        require(effect.regeneration.has_value() == (code == 0x62 || code == 0x65),
                "Generic regeneration does not invent a rate");
        if (effect.regeneration)
        {
            require(effect.kind == EffectKind::regeneration &&
                        effect.regeneration->hp_per_round == 3,
                    "Quantified regeneration restores three HP");
            const auto delay = effect.regeneration->revival_delay_rounds;
            require(delay.has_value() == (code == 0x65),
                    "Only troll regeneration has a revival delay");
            if (delay)
                require(delay->count == 3 && delay->sides == 6 && delay->modifier == 0,
                        "Troll revival takes 3d6 rounds");
        }
    }
    for (const auto pair : {std::pair{0x19, 0x47}, std::pair{0x23, 0x31}, std::pair{0x3b, 0x3e},
                            std::pair{0x67, 0x77}})
    {
        const auto first = describe_effect(pair.first), second = describe_effect(pair.second);
        require(first.code != second.code && first.name == second.name &&
                    first.kind == second.kind && first.subject == second.subject,
                "Effect aliases share meaning while retaining original identifiers");
    }
}

void catalog()
{
    Fixture f;
    f.populate();
    const auto c = CreatureCatalog::load(f.path());
    require(c.all().size() == 2 && c.find_by_name("test creature").size() == 2,
            "All duplicate-name variants retained");
    require(!c.find({1, 31}) && c.find_by_name("missing").empty(), "Absent lookups explicit");
    const auto &creature = c.find({2, 31})->get();
    require(creature.id.key() == "MON2CHA.DAX:31", "Stable source identity");
    require(creature.equipment.size() == 2 && c.find({5, 31})->get().equipment.empty(),
            "Equipment never leaks between banks");
    require(creature.equipment[0].bonuses.weapon_to_hit == 1 &&
                creature.equipment[0].base.large_damage.sides == 12,
            "Weapon bonus and damage template");
    require(creature.equipment[1].bonuses.armor_base_ac == 5 &&
                creature.equipment[1].bonuses.ac_adjustment == -1,
            "Armor decoding and magic contribution");
    require(!creature.equipment[1].stored.readied(), "Carried equipment not silently equipped");
    require(creature.effects.size() == 3 && creature.effects.back().stored.duration_raw == 300 &&
                creature.effects.back().stored.table_flag == 2 &&
                creature.effects.back().stored.next_pointer_raw == 0x04030201,
            "Effect duration, parameters and disk pointers preserved");
    require(!creature.interpretation_notes.empty(), "Unknown effect diagnosed");
    std::filesystem::rename(f.path() / "MON1CHA.DAX", f.path() / "mon1cha.dax");
    require(CreatureCatalog::load(f.path()).all().size() == 2, "Case-insensitive DOS filenames");
    std::filesystem::rename(f.path() / "mon1cha.dax", f.path() / "MON1CHA.DAX");
    write(f.path() / "MON2SPC.DAX", dax({{31, Bytes(8)}}));
    fails(
        [&]
        {
            (void)CreatureCatalog::load(f.path());
        },
        "MON2SPC.DAX:31");
    require(c.all().size() == 2, "Existing snapshot survives failed load");
    f.populate();
    Bytes protection(4 * 63);
    protection[46] = 59;
    protection[50] = 255;
    protection[54] = 1; // Cursed shield -1.
    protection[63 + 46] = 77;
    protection[63 + 50] = 4; // Bracers AC 6.
    protection[126 + 46] = 93;
    protection[126 + 50] = 2;
    protection[126 + 51] = 2;
    protection[189 + 46] = 79;
    protection[189 + 50] = 10; // Wand charges, not +10 attack.
    write(f.path() / "MON2ITM.DAX", dax({{31, protection}}));
    const auto equipped = CreatureCatalog::load(f.path());
    const auto &gear = equipped.find({2, 31})->get().equipment;
    require(gear[0].stored.magic_bonus == -1 && gear[0].bonuses.ac_adjustment == 0,
            "Cursed shield signed bonus");
    require(gear[1].bonuses.armor_base_ac == 10 && gear[1].bonuses.ac_adjustment == -4,
            "Armor substitute protection");
    require(gear[2].bonuses.ac_adjustment == -2 && gear[2].bonuses.save_bonus == 2,
            "Ring protection contributions");
    require(!gear[3].bonuses.weapon_to_hit && !gear[3].bonuses.weapon_damage,
            "Charges are not weapon bonuses");
    f.populate();
    write(f.path() / "MON2ITM.DAX", dax({{200, Bytes(63)}}));
    fails(
        [&]
        {
            (void)CreatureCatalog::load(f.path());
        },
        "MON2ITM.DAX:200");
    f.populate();
    write(f.path() / "MON2ITM.DAX", dax({{31, Bytes(62)}}));
    fails(
        [&]
        {
            (void)CreatureCatalog::load(f.path());
        },
        "MON2ITM.DAX:31");
    f.populate();
    auto invalid_item = Bytes(63);
    invalid_item[46] = 255;
    write(f.path() / "MON2ITM.DAX", dax({{31, invalid_item}}));
    fails(
        [&]
        {
            (void)CreatureCatalog::load(f.path());
        },
        "Missing item template 255");
    f.populate();
    std::filesystem::remove(f.path() / "MON8CHA.DAX");
    fails(
        [&]
        {
            (void)CreatureCatalog::load(f.path());
        },
        "MON8CHA.DAX");
}

void instances()
{
    Fixture f;
    f.populate();
    auto factory = CreatureFactory::load(f.path());
    auto first = factory.create({2, 31});
    auto second = factory.create({2, 31});
    require(first.hit_points() == 36 && first.max_hit_points() == 36,
            "Spawn uses max HP, not raw current HP 250");
    require(&first.definition() == &second.definition(), "Instances share immutable stats");
    require(first.definition().equipment.size() == 2 && first.definition().effects.size() == 3,
            "Instances expose full equipment and effects");
    require(factory.create({5, 31}).definition().equipment.empty(),
            "Factory preserves bank variants");
    fails(
        [&]
        {
            (void)factory.create({1, 255});
        },
        "MON1CHA.DAX:255");
    fails(
        [&]
        {
            (void)factory.create({2, 31}, 0);
        },
        "Positive instance HP");
    fails(
        [&]
        {
            (void)factory.create({2, 31}, -1);
        },
        "Positive instance HP");
    require(factory.create({2, 31}, 42).max_hit_points() == 42, "Explicit encounter HP");
    require(first.available_actions().empty() && !first.try_attack(0), "No actions before turn");
    require(first.begin_turn(1, {{2, 1}, 6}), "Begin allocated turn");
    require(first.available_actions().size() == 4, "Two attacks, movement, end turn");
    require(!first.begin_turn(2, {{9, 9}, 99}), "Active turn cannot replenish budget");
    require(first.try_attack(0) && first.try_attack(0) && !first.try_attack(0),
            "Primary attack budget exhausted");
    require(first.try_attack(1) && !first.try_attack(1) && !first.try_attack(99),
            "Secondary attack budget and bounds");
    require(!first.try_move(0) && !first.try_move(7) && first.try_move(4) && first.try_move(2) &&
                !first.try_move(1),
            "Movement consumes only valid distances");
    require(first.available_actions().size() == 1, "Only end turn remains");
    first.end_turn();
    require(!first.begin_turn(1, {{2, 1}, 6}), "Repeated turn cannot refill");
    require(first.begin_turn(2, {{1, 0}, 3}), "Next turn can have different rules budget");
    auto snapshot = first;
    require(first.take_damage(10) == 10 && first.hit_points() == 26 && second.hit_points() == 36 &&
                snapshot.hit_points() == 36,
            "Damage isolated across instances and snapshots");
    require(first.heal(std::numeric_limits<int>::max()) == 10 && first.hit_points() == 36,
            "Healing clamps without overflow");
    require(first.take_damage(std::numeric_limits<int>::max()) == 36 && first.hit_points() == 0 &&
                first.available_actions().empty(),
            "Lethal damage clamps and cancels turn");
    require(!first.try_move(1) && !first.try_attack(0), "Zero HP disables actions");
    require(first.heal(1) == 1 && !first.can_act(), "Healing cannot restore a cancelled turn");
    require(first.begin_turn(3, {{1, 0}, 1}), "Recovered creature begins next turn");
    first.set_incapacitated(true);
    require(!first.can_act() && !first.begin_turn(4, {{1, 0}, 1}), "Incapacitation blocks turn");
    first.set_incapacitated(false);
    require(!first.begin_turn(4, {{1, 0}, 1}) && first.begin_turn(5, {{1, 0}, 1}),
            "Recovery cannot replay skipped turn");
    for (const bool healing : {false, true})
    {
        bool rejected = false;
        try
        {
            if (healing)
                first.heal(-1);
            else
                first.take_damage(-1);
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        require(rejected && first.hit_points() == 1,
                "Negative HP operations rejected without mutation");
    }
    auto survivor = [&]
    {
        auto local = CreatureFactory::load(f.path());
        return local.create({2, 31});
    }();
    require(survivor.definition().stored.name == "TEST CREATURE", "Instance outlives factory");
    auto empty = character();
    empty[50] = 0;
    empty[161] = 0;
    write(f.path() / "MON5CHA.DAX", dax({{31, empty}}));
    const auto zero_factory = CreatureFactory::load(f.path());
    fails(
        [&]
        {
            (void)zero_factory.create({5, 31});
        },
        "Positive instance HP");
    auto overridden = zero_factory.create({5, 31}, 8);
    bool rejected = false;
    try
    {
        (void)overridden.begin_turn(1, {{1, 0}, 0});
    }
    catch (const std::invalid_argument &)
    {
        rejected = true;
    }
    require(rejected && overridden.begin_turn(1, {{0, 0}, 0}),
            "Invalid attack budget leaves turn unchanged");
}

void maps()
{
    Bytes bytes(1029);
    bytes[0] = 17;
    bytes[1] = 99;
    bytes.back() = 123;
    bytes[2] = 0x12;
    bytes[258] = 0x34;
    bytes[514] = 0x85;
    bytes[770] = 0xe4;
    bytes[257] = 0xab;
    bytes[513] = 0xcd;
    bytes[769] = 127;
    bytes[1025] = 0x1b;
    const auto decoded = decode_geo_map(bytes);
    require(decoded && decoded->raw == bytes, "GEO header and trailing bytes retained");
    const auto &a = decoded->at(0, 0);
    require(a.walls == std::array<std::uint8_t, 4>{1, 2, 3, 4} &&
                a.doors == std::array<std::uint8_t, 4>{0, 1, 2, 3},
            "GEO cardinal nibble and door bit order");
    require(a.event_number() == 5 && a.event_high_bit() && a.event_raw == 0x85,
            "Event high bit retained separately");
    const auto &b = decoded->at(15, 15);
    require(b.walls == std::array<std::uint8_t, 4>{10, 11, 12, 13} &&
                b.doors == std::array<std::uint8_t, 4>{3, 2, 1, 0} && b.event_number() == 127 &&
                !b.event_high_bit(),
            "Last GEO cell decoded without transpose");
    for (const auto point : {std::array<unsigned, 2>{16, 0}, {0, 16}})
    {
        bool rejected = false;
        try
        {
            (void)decoded->at(point[0], point[1]);
        }
        catch (const std::out_of_range &)
        {
            rejected = true;
        }
        require(rejected, "GEO coordinate bounds");
    }
    require(!decode_geo_map(Bytes(1025)) && !decode_geo_map({}), "Truncated GEO rejected");
    Fixture f;
    write(f.path() / "geo1.dax", dax({{7, bytes}, {9, Bytes(1026)}}));
    write(f.path() / "GEO2.DAX", dax({{7, Bytes(1026)}}));
    write(f.path() / "GEO-not-a-map.DAX", Bytes(1));
    const auto loaded = MapCatalog::load(f.path());
    require(loaded.all().size() == 3 &&
                loaded.find({"GEO1.DAX", 7})->get().at(0, 0).event_raw == 0x85 &&
                loaded.find({"GEO2.DAX", 7})->get().at(0, 0).event_raw == 0,
            "Map archive identity and case normalization");
    require(!loaded.find({"GEO1.DAX", 8}), "Absent map explicit");
    write(f.path() / "GEO2.DAX", dax({{7, Bytes(1025)}}));
    bool rejected = false;
    try
    {
        (void)MapCatalog::load(f.path());
    }
    catch (const MapError &e)
    {
        rejected = std::string_view(e.what()).find("GEO2.DAX:7") != std::string_view::npos;
    }
    require(rejected && loaded.all().size() == 3, "Bad map diagnosed; previous catalog unchanged");
    write(f.path() / "GEO2.DAX", dax({{7, bytes}, {7, bytes}}));
    rejected = false;
    try
    {
        (void)MapCatalog::load(f.path());
    }
    catch (const MapError &)
    {
        rejected = true;
    }
    require(rejected, "Duplicate DAX record rejected");
    Fixture empty;
    rejected = false;
    try
    {
        (void)MapCatalog::load(empty.path());
    }
    catch (const MapError &)
    {
        rejected = true;
    }
    require(rejected, "Empty map directory rejected");
}

void installed()
{
    const char *directory = std::getenv("OPENGOLD_GAME_DIR");
    if (!directory)
    {
        std::cout << "Installed-game checks skipped (set OPENGOLD_GAME_DIR).\n";
        return;
    }
    const auto c = CreatureCatalog::load(directory);
    require(c.all().size() == 172, "Stock Pool of Radiance has 172 records");
    const auto &troll = c.find({2, 31})->get();
    require(troll.stored.name == "TROLL" && troll.stored.max_hit_points == 36 &&
                troll.stored.base_armor_class == 4 && troll.stored.base_thac0 == 13,
            "Installed troll base stats");
    require(troll.stored.base_attacks[0].attacks_per_round() == 2 &&
                troll.stored.base_attacks[0].damage.modifier == 4 &&
                troll.stored.base_attacks[1].damage.count == 2,
            "Installed troll attacks not double-adjusted by STR");
    require(troll.effects.size() == 2 &&
                troll.effects[0].definition.kind == EffectKind::vulnerability &&
                troll.effects[1].definition.regeneration->hp_per_round == 3,
            "Installed troll effects");
    const auto &fighter = c.find({1, 41})->get();
    require(fighter.equipment.size() == 3, "Installed fighter inventory");
    const auto sword = std::find_if(fighter.equipment.begin(), fighter.equipment.end(),
                                    [](const auto &i)
                                    {
                                        return i.stored.type == 36;
                                    });
    require(sword != fighter.equipment.end() && sword->bonuses.weapon_to_hit == 1 &&
                sword->base.small_medium_damage.sides == 8,
            "Installed fighter longsword +1");
    require(c.find({7, 42})->get().stored.name == "HASSAD" &&
                c.find({7, 42})->get().stored.max_hit_points == 45,
            "Named NPC included");
    std::cout << "Installed-game checks passed for all " << c.all().size() << " records.\n";
}
} // namespace

int main()
{
    try
    {
        parsing();
        modifiers_and_effects();
        effect_contracts();
        catalog();
        instances();
        maps();
        installed();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
