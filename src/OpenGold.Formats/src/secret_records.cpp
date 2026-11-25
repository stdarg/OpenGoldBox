#include "opengold/secret_records.h"
#include "little_endian.h"
#include "record_fields.h"

#include <algorithm>
#include <array>

namespace opengold::secret
{
namespace
{

constexpr std::size_t creature_size = 439;

// The spell slot lists by their place in the record: cleric, druid, an unused
// list, magic-user; each holds seven spell levels.
constexpr std::size_t spell_slots_offset = 306, levels_a_list = 7;
constexpr std::array<std::size_t, 3> slot_lists{0, 1, 3};

// Seven classes (cleric, druid, fighter, paladin, ranger, magic-user, thief);
// Secret has no monks.
constexpr std::size_t classes = 7;

// Secret numbers its races elf 1, half-elf 2, dwarf 3, gnome 4, halfling 5,
// human 6 and monster 7 (0 is unused); the shared fields keep the earlier
// games' numbers, monster 0 and dwarf, elf, gnome, half-elf, halfling,
// half-orc and human 1 to 7.
std::uint8_t shared_race(std::uint8_t race)
{
    static constexpr std::array<std::uint8_t, 8> shared{0, 2, 4, 1, 3, 5, 7, 0};
    return race < shared.size() ? shared[race] : 0;
}

} // namespace

std::optional<por::CharacterRecord> decode_creature(std::span<const std::uint8_t> b)
{
    if (b.size() != creature_size || b[0] > 15)
    {
        return std::nullopt;
    }
    por::CharacterRecord c;
    c.game = GoldBoxGame::secret_of_the_silver_blades;
    c.raw.assign(b.begin(), b.end());
    c.name.assign(b.begin() + 1, b.begin() + 1 + b[0]);
    c.abilities = format::paired_abilities(b, 16, format::AbilityScore::permanent);
    c.current_abilities = format::paired_abilities(b, 16, format::AbilityScore::current);
    c.memorized_spells.assign(b.begin() + 30, b.begin() + 30 + 75);
    c.spell_byte_44_raw = b[105];
    c.base_thac0 = 60 - b[106];
    c.race = shared_race(b[107]);
    c.character_class = b[108];
    c.age = format::read_u16(b, 110);
    c.max_hit_points = b[112];
    c.spell_book.assign(b.begin() + 113, b.begin() + 113 + 117);
    c.attack_level = b[230];
    c.icon_size_raw = b[231];
    c.saves = format::saving_throws(b, 232);
    c.base_movement = b[237];
    c.highest_level_raw = b[238];
    c.drained_levels = b[240];
    c.drained_hit_points = b[241];
    // The undead level, as in Curse, then the eight thief skills.
    c.undead_level = b[242];
    format::copy_bytes(b, 243, c.thief_skills);
    c.npc_flag = b[255];
    for (std::size_t i = 0; i < c.wealth.size(); ++i)
    {
        c.wealth[i] = format::read_u16(b, 259 + 2 * i);
    }
    c.levels = format::class_levels(b, 273, classes);
    c.levels_before_class_change = format::class_levels(b, 280, classes);
    c.gender = b[287];
    c.alignment = b[288];
    c.base_attacks = format::base_attacks(b, 289);
    c.base_armor_class = 60 - b[297];
    c.strength_bonus_flag_raw = b[298];
    c.experience = format::read_u32(b, 300);
    // The classes' usability bits, which an item type's restrictions read.
    c.item_limit = b[304];
    c.rolled_hit_points = b[305];
    for (std::size_t list = 0; list < c.spell_slots.size(); ++list)
    {
        const auto first = spell_slots_offset + levels_a_list * slot_lists[list];
        std::copy_n(b.begin() + static_cast<std::ptrdiff_t>(first), c.spell_slots[list].size(),
                    c.spell_slots[list].begin());
    }
    c.base_xp_award = format::read_u16(b, 334);
    c.xp_per_hit_point = b[336];
    c.creature_type_flags = b[349];
    c.target_flags = b[350];
    c.item_count = b[352];
    c.hands_equipped = b[409];
    c.save_bonus = format::signed_byte(b[410]);
    c.encumbrance = format::read_u16(b, 411);
    c.magic_resistance = b[421];
    c.status = b[422];
    c.enabled = b[423];
    c.hostile = b[424];
    c.quick_fight = b[425];
    c.current = format::current_stats(b, 426);
    return c;
}

} // namespace opengold::secret
