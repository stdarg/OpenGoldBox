// The Buck Rogers games' script commands the session answers, where their
// table changed or added commands, and the current character's record as
// their scripts read and write it (docs/games/countdown.md, "Scripts"; the
// game in the engine, B3).
#include "opengold/rolf_tour.h"
#include "opengold/buck_rogers_characters.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/buck_rogers_battlefield.h"
#include "opengold/buck_rogers_upkeep.h"
#include "opengold/creature_art.h"
#include "opengold/encounter_formation.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace opengold::por
{
namespace
{
// The current character's record in zone 1 (C_Gbvm.CURRENT_CHARACTER): its
// name, then its record's bytes 10h to B9h as they are (eclmenu.pas
// SpecialReadZ1), then the fields the original serves at their own places.
constexpr std::uint16_t record_cell = 0x7C00;
constexpr std::uint16_t first_record_byte = 0x10, last_record_byte = 0xB9;
// The record's numbers of more than a byte (types.pas TCharacter): the
// credits and experience (four bytes), the Hit Point maximum and the
// experience the creature is worth (a word each).
constexpr std::uint16_t credits = 0x2B, experience = 0x2F, hit_point_maximum = 0x45,
                        experience_award = 0xB2;
// isCompanion, read as 80h while it is set.
constexpr std::uint16_t companion = 0x4C;
// weaponSpecializations[1..10] at C4h to CDh.
constexpr std::uint16_t first_specialization = 0xC4;
constexpr std::size_t specialization_offset = 186; // In the record.
constexpr std::uint16_t status = 0x100, side_flags = 0x10C, attack_bonus = 0x10E,
                        armor_class = 0x10F, hit_points = 0x119, current_movement = 0x11B,
                        party_place = 0x2B1;
// The record's own offsets of the fields the original serves elsewhere.
constexpr std::size_t record_status = 212, record_side = 214, record_quickfight = 215,
                      record_attack_bonus = 216, record_armor_class = 217, record_hit_points = 227,
                      record_current_movement = 229;

[[nodiscard]] constexpr EclAddress cell(std::uint16_t offset) noexcept
{
    return EclAddress(static_cast<std::uint16_t>(record_cell + offset));
}

[[nodiscard]] std::uint32_t read_u32(const std::vector<std::uint8_t> &bytes, std::size_t at)
{
    return bytes[at] | bytes[at + 1] << 8 | bytes[at + 2] << 16 |
           static_cast<std::uint32_t>(bytes[at + 3]) << 24;
}

void write_u16(std::vector<std::uint8_t> &bytes, std::size_t at, unsigned value)
{
    bytes[at] = static_cast<std::uint8_t>(value & 0xFF);
    bytes[at + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

// Whether a record offset is one of a number's later bytes, which the
// number's own cell holds.
[[nodiscard]] constexpr bool inside_number(std::uint16_t offset) noexcept
{
    return (offset > credits && offset < credits + 4) ||
           (offset > experience && offset < experience + 4) || offset == hit_point_maximum + 1 ||
           offset == experience_award + 1;
}

} // namespace

void RolfTourSession::enable_buck_rogers_party_commands()
{
    // LOAD MONSTER, SET SPEAKER, CHECK PARTY, SHIP COMBAT, the skill checks,
    // DAMAGE, ECL CLOCK, ADD NPC, DUMP, DESTROY ITEMS, AWARD EXPERIENCE,
    // PLAY SOUND, DISTANCE AHEAD, SWAP ITEM LISTS, SKILL CHECK WITH DAMAGE,
    // SOLO COMBAT, SHOP and PICTURE MODE.
    for (const std::uint8_t op : {0x0B, 0x1D, 0x1E, 0x1F, 0x22, 0x23, 0x2E, 0x34, 0x36, 0x3E, 0x40,
                                  0x41, 0x43, 0x45, 0x48, 0x49, 0x4A, 0x4B, 0x4C})
    {
        machine_.enable_host(op);
    }
}

void RolfTourSession::add_buck_rogers_record(PartySlot slot, EclHostReply &reply) const
{
    const auto id = campaign_ && slot.index < 8 ? campaign_->state().slots[slot.index] : MemberId{};
    const auto record = id ? original::buck_rogers_record_of(campaign_->game_record(id))
                           : std::optional<original::GroundRecord>{};
    std::vector<std::uint8_t> bytes(buck_rogers::Creature::size);
    if (record)
    {
        bytes = buck_rogers::encode_creature(record->creature);
    }
    std::vector<EclMemoryWrite> writes;
    const auto put = [&writes](std::uint16_t offset, unsigned value)
    {
        writes.push_back({cell(offset), static_cast<std::uint16_t>(value)});
    };
    // The name, as the scripts compare and print it, ended by 0.
    const std::string name = record ? record->creature.name : std::string{};
    for (std::uint16_t n = 0; n < first_record_byte; ++n)
    {
        put(n, n < name.size()
                   ? static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(name[n])))
                   : 0U);
    }
    for (std::uint16_t offset = first_record_byte; offset <= last_record_byte; ++offset)
    {
        if (inside_number(offset))
        {
            continue;
        }
        put(offset, bytes[offset]);
    }
    const auto low_word = [&bytes](std::size_t at)
    {
        return static_cast<unsigned>(bytes[at] | bytes[at + 1] << 8);
    };
    for (const auto number : {credits, experience})
    {
        std::erase_if(writes,
                      [number](const EclMemoryWrite &write)
                      {
                          return write.address == cell(number);
                      });
        put(number, low_word(number));
        put(static_cast<std::uint16_t>(number + 2), low_word(number + 2u));
    }
    std::erase_if(writes,
                  [](const EclMemoryWrite &write)
                  {
                      return write.address == cell(hit_point_maximum) ||
                             write.address == cell(experience_award) ||
                             write.address == cell(companion);
                  });
    put(hit_point_maximum, low_word(hit_point_maximum));
    put(experience_award, low_word(experience_award));
    put(companion, bytes[companion] != 0 ? 0x80U : 0U);
    for (std::uint16_t n = 0; n < 10; ++n)
    {
        put(static_cast<std::uint16_t>(first_specialization + n), bytes[specialization_offset + n]);
    }
    put(status, bytes[record_status]);
    put(side_flags, (bytes[record_side] & 1U) | (bytes[companion] != 0 ? 0x40U : 0U) |
                        (bytes[record_quickfight] != 0 ? 0x80U : 0U));
    put(attack_bonus, bytes[record_attack_bonus]);
    put(armor_class, bytes[record_armor_class]);
    put(static_cast<std::uint16_t>(armor_class + 1), bytes[record_armor_class + 1]);
    put(hit_points, low_word(record_hit_points));
    put(current_movement, bytes[record_current_movement]);
    put(party_place, slot.index);
    // A reply names a cell once: these replace the other games' fields.
    std::erase_if(reply.writes,
                  [&writes](const EclMemoryWrite &write)
                  {
                      return std::ranges::find(writes, write.address, &EclMemoryWrite::address) !=
                             writes.end();
                  });
    reply.writes.insert(reply.writes.end(), writes.begin(), writes.end());
}

void RolfTourSession::read_buck_rogers_character()
{
    const auto id = campaign_ && selected_character_.index < 8
                        ? campaign_->state().slots[selected_character_.index]
                        : MemberId{};
    if (!id)
    {
        return;
    }
    auto record = original::buck_rogers_record_of(campaign_->game_record(id));
    if (!record)
    {
        return;
    }
    // Only what the script wrote changes the record (eclmenu.pas
    // SpecialWriteZ1): its bytes 10h to B9h, its status, side flags, attack
    // bonus, Hit Points and movement.
    auto bytes = buck_rogers::encode_creature(record->creature);
    bool changed = false;
    const auto written = [this, &changed](std::uint16_t offset) -> std::optional<std::uint16_t>
    {
        if (!machine_.consume_script_write(cell(offset)))
        {
            return std::nullopt;
        }
        changed = true;
        return machine_.variable(cell(offset));
    };
    for (std::uint16_t offset = first_record_byte; offset <= last_record_byte; ++offset)
    {
        if (inside_number(offset) || offset == credits || offset == experience ||
            offset == hit_point_maximum || offset == experience_award)
        {
            continue;
        }
        if (const auto value = written(offset))
        {
            bytes[offset] = static_cast<std::uint8_t>(*value & 0xFF);
        }
    }
    for (const auto number : {credits, experience})
    {
        const auto low = written(number);
        const auto high = written(static_cast<std::uint16_t>(number + 2));
        if (low || high)
        {
            const auto value = read_u32(bytes, number);
            write_u16(bytes, number, low.value_or(static_cast<std::uint16_t>(value & 0xFFFF)));
            write_u16(bytes, number + 2u, high.value_or(static_cast<std::uint16_t>(value >> 16)));
        }
    }
    for (const auto word : {hit_point_maximum, experience_award})
    {
        if (const auto value = written(word))
        {
            write_u16(bytes, word, *value);
        }
    }
    auto creature = buck_rogers::decode_creature(bytes);
    if (!creature)
    {
        throw EclError("A script wrote an unreadable Buck Rogers record");
    }
    if (const auto value = written(status))
    {
        original::set_status(*creature, static_cast<original::BuckRogersStatus>(*value & 0x7F));
    }
    if (const auto value = written(side_flags))
    {
        creature->side = *value & 1;
        creature->companion = static_cast<std::uint8_t>(*value & 0x40);
        creature->quickfight = static_cast<std::uint8_t>(*value & 0x80);
    }
    if (const auto value = written(attack_bonus))
    {
        creature->attack_bonus = static_cast<std::uint8_t>(*value);
    }
    if (const auto value = written(hit_points))
    {
        creature->hit_points = *value;
    }
    if (const auto value = written(current_movement))
    {
        creature->current_movement = static_cast<std::uint8_t>(*value);
    }
    if (!changed)
    {
        return;
    }
    record->creature = std::move(*creature);
    campaign_->keep_game_record(id, original::encode_ground_record(*record));
}

namespace
{
// The commands below, by the Buck Rogers table's numbers (ecl.cpp
// buck_rogers_opcodes).
constexpr std::uint8_t set_speaker = 0x1D, ship_combat = 0x1F, skill_check = 0x22,
                       skill_check_with_text = 0x23, call = 0x2D, damage = 0x2E, find_item = 0x32,
                       destroy_items = 0x40, award_experience = 0x41, play_sound = 0x43,
                       distance_ahead = 0x45, swap_item_lists = 0x48,
                       skill_check_with_damage = 0x49, picture_mode = 0x4C, picture_command = 0x0E,
                       load_monster = 0x0B, combat_command = 0x24, solo_combat = 0x4A,
                       treasure = 0x27, shop = 0x4B, add_npc = 0x36, program = 0x38;
// Zone 0's cells these commands keep (types.pas TZone0): the speaker cells
// SET SPEAKER clears, the picture mode, whether the items are stashed, the
// area kind and the ship fight's outcome.
constexpr EclAddress speaker_cells{0x4BB4}, picture_mode_cell{0x4BA6}, items_stashed{0x4BB8},
    area_kind{0x4BE6}, ship_combat_outcome{0x4BBA};
// The area kind of the solar map (C_Area.ORBIT) and PICTURE MODE's mode for
// it (C_Ifc.ORBIT_MAP).
constexpr std::uint16_t orbit_area = 0x70, orbit_picture_mode = 2;
// eclSoundCommands: the sound command each PLAY SOUND operand sends (gdconsts.pas).
constexpr std::array<std::uint8_t, 21> sound_commands{0, 12, 9, 16, 7,  10, 11, 18, 14, 3, 4,
                                                      5, 6,  8, 13, 17, 16, 19, 19, 19, 20};
constexpr unsigned footstep_command = 10;

// Whether an operand names a cell a command writes to (a byte, word or
// four-byte cell): a literal there names none.
[[nodiscard]] bool names_cell(const EclOperand &operand) noexcept
{
    return operand.tag == 1 || operand.tag == 3 || operand.tag == 5;
}

// The team's members by their places in the party, with their records.
struct TeamPlace
{
    unsigned slot{};
    MemberId id{};
    original::GroundRecord record;
};

[[nodiscard]] std::vector<TeamPlace> team_of(const CampaignParty &party)
{
    std::vector<TeamPlace> team;
    for (unsigned slot = 0; slot < party.state().slots.size(); ++slot)
    {
        const auto id = party.state().slots[slot];
        if (!id)
        {
            continue;
        }
        if (auto record = original::buck_rogers_record_of(party.game_record(id)))
        {
            team.push_back({slot, id, std::move(*record)});
        }
    }
    return team;
}

[[nodiscard]] bool can_act(const buck_rogers::Creature &creature) noexcept
{
    return original::status_of(creature) == original::BuckRogersStatus::okay;
}

[[nodiscard]] std::string upper(std::string text)
{
    for (auto &c : text)
    {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return text;
}
} // namespace

bool RolfTourSession::handle_buck_rogers_host(const EclRequest &request)
{
    const auto &instruction = *request.instruction;
    const auto &operands = instruction.operands;
    const auto op = instruction.opcode;
    const auto value = [&](std::size_t n)
    {
        return machine_.operand_value(operands.at(n));
    };
    // The script's dice, for the rules' rolls: what a reload of the event
    // rolls again the same.
    const auto dice = [&]
    {
        return RandomState{
            (static_cast<std::uint64_t>(machine_.host_random(request.id, 65536)) << 16) |
            machine_.host_random(request.id, 65536)};
    };
    const auto archive = "ECL" + std::to_string(town_->script_banks.at(current_script_)) + ".DAX";
    EclHostReply reply;
    const auto write_to = [&reply](const EclOperand &operand, std::int64_t number)
    {
        if (!names_cell(operand))
        {
            return;
        }
        for (const auto &cell :
             EclMachine::sized_writes(EclAddress{operand.value}, number, operand.tag))
        {
            std::erase_if(reply.writes,
                          [&cell](const EclMemoryWrite &write)
                          {
                              return write.address == cell.address;
                          });
            reply.writes.push_back(cell);
        }
    };
    // A member's record as the command changed it, its Hit Points and death
    // with it.
    const auto keep = [this](const TeamPlace &member)
    {
        campaign_->keep_game_record(member.id, original::encode_ground_record(member.record));
    };
    switch (op)
    {
    case set_speaker:
        reply.writes = {{speaker_cells, 0}, {EclAddress{0x4BB5}, 0}};
        break;
    case play_sound:
    {
        // Above 127 Countdown plays nothing (eclvm.pas EclPlaySound).
        const auto number = static_cast<std::size_t>(value(0) & 0xFF);
        if (number < sound_commands.size())
        {
            const unsigned command = sound_commands[number];
            if (command == footstep_command)
            {
                ++snapshot_.footsteps;
            }
            else if (command != 0)
            {
                snapshot_.script_sound = command;
                ++snapshot_.script_sounds;
            }
            sound_effect_played();
        }
        break;
    }
    case call:
        // The sound selector is never set, so its routine plays nothing
        // (eclvm.pas EclCall, GDVars.selectorSound); the others are the
        // other games' (handle_host).
        if (request.arguments.at(0).address() != EclAddress{ecl_engine_routines(game()).sound})
        {
            return false;
        }
        break;
    case distance_ahead:
    {
        // How far the walls ahead let a monster stand, up to 5; 2 where the
        // area kind is 0 (eclmenu.pas MaxMonsterDist).
        const auto facing = static_cast<unsigned>(value(1) & 3);
        unsigned distance = 0;
        if (machine_.variable(area_kind) == 0)
        {
            distance = 2;
            reply.writes.push_back({game_cell(EclAddress{0x6DC1}), 2});
        }
        else
        {
            auto x = static_cast<int>(machine_.variable(game_cell(ecl_party_x)));
            auto y = static_cast<int>(machine_.variable(game_cell(ecl_party_y)));
            while (distance < 5 && x >= 0 && y >= 0 && x < 16 && y < 16 &&
                   map_.at(static_cast<unsigned>(x), static_cast<unsigned>(y)).walls[facing] == 0)
            {
                ++distance;
                x += step_x(static_cast<MapDirection>(facing));
                y += step_y(static_cast<MapDirection>(facing));
                x = (x + 16) % 16;
                y = (y + 16) % 16;
            }
        }
        write_to(operands.at(0), distance);
        break;
    }
    case skill_check:
    case skill_check_with_text:
    case skill_check_with_damage:
    {
        read_character();
        auto team = team_of(*campaign_);
        const auto skill = original::skill_numbered(
            original::script_check_skill(game(), archive, current_script_, instruction.address,
                                         static_cast<unsigned>(value(0) & 0xFF)));
        const bool whole_party = value(1) != 0;
        const auto difficulty = static_cast<int>(value(2) & 0xFF);
        const auto current = std::ranges::find(team, selected_character_.index, &TeamPlace::slot);
        if (op == skill_check_with_damage)
        {
            // Each member who fails takes the damage (EclSkillDamage): every
            // member, or the current character alone.
            auto random = dice();
            for (auto &member : team)
            {
                if (!whole_party && member.slot != selected_character_.index)
                {
                    continue;
                }
                const std::array values{original::skill_value(member.record.creature, skill)};
                const auto check = original::check_skill(values, difficulty, random);
                if (check.grade >= original::SkillCheckGrade::success ||
                    !can_act(member.record.creature))
                {
                    continue;
                }
                int harm = static_cast<int>(value(5) & 0xFF);
                for (std::int64_t die = 0; die < (value(3) & 0xFF); ++die)
                {
                    harm += roll_die(random, static_cast<int>(value(4) & 0xFF));
                }
                int dying = 0;
                original::inflict_ground_damage(member.record.creature, harm, dying);
                keep(member);
                snapshot_.dialogue =
                    join_script_text(std::move(snapshot_.dialogue),
                                     upper(member.record.creature.name) + " IS HIT FOR " +
                                         std::to_string(harm) + " POINTS OF DAMAGE.");
            }
            reply = character_reply(selected_character_);
            break;
        }
        std::vector<int> values;
        std::vector<const TeamPlace *> rollers;
        for (const auto &member : team)
        {
            if (whole_party || member.slot == selected_character_.index)
            {
                values.push_back(original::skill_value(member.record.creature, skill));
                rollers.push_back(&member);
            }
        }
        if (rollers.empty())
        {
            throw EclError("A skill check needs a member");
        }
        auto random = dice();
        const auto check = original::check_skill(values, difficulty, random);
        const auto &roller = *rollers.at(check.roller);
        const bool succeeded = check.grade >= original::SkillCheckGrade::success;
        if (op == skill_check_with_text)
        {
            const auto &name = original::skill_definition(skill).name;
            snapshot_.dialogue =
                join_script_text(std::move(snapshot_.dialogue),
                                 succeeded ? upper(roller.record.creature.name) + " SUCCEEDS AT " +
                                                 upper(std::string(name)) + "."
                                           : (whole_party ? std::string("THE TEAM")
                                                          : upper(roller.record.creature.name)) +
                                                 " FAILS AT " + upper(std::string(name)) + ".");
        }
        (void)current;
        reply = character_reply(selected_character_);
        write_to(operands.at(1), roller.slot);
        write_to(operands.at(2), static_cast<std::int64_t>(check.grade));
        break;
    }
    case award_experience:
    {
        // Each member who can act, or the current character (EclAwardExperience).
        read_character();
        const bool whole_party = value(0) != 0;
        const auto amount = static_cast<std::uint32_t>(value(1) & 0xFFFF);
        std::string gainer = "THE TEAM";
        for (auto &member : team_of(*campaign_))
        {
            if (!whole_party && member.slot == selected_character_.index)
            {
                gainer = upper(member.record.creature.name);
            }
            if ((whole_party || member.slot == selected_character_.index) &&
                member.record.creature.enabled != 0)
            {
                member.record.creature.experience += amount;
                keep(member);
            }
        }
        snapshot_.dialogue =
            join_script_text(std::move(snapshot_.dialogue), gainer + " GAINS EXPERIENCE.");
        reply = character_reply(selected_character_);
        break;
    }
    case find_item:
    {
        // Equal when a member carries an item of the type (EclFindItem).
        const auto type = static_cast<std::uint8_t>(value(0) & 0xFF);
        bool found = false;
        for (const auto &member : team_of(*campaign_))
        {
            found = found || std::ranges::any_of(member.record.items,
                                                 [type](const buck_rogers::Item &item)
                                                 {
                                                     return item.type == type;
                                                 });
        }
        reply.conditions = EclConditions{found, !found, false, false, false, false};
        break;
    }
    case destroy_items:
    {
        // Items of the type leave the current character or each member;
        // with the type's bit 80h only the first found, a bundle losing one
        // (EclDestroyItems).
        read_character();
        const bool whole_party = value(0) != 0;
        const auto named = static_cast<std::uint8_t>(value(1) & 0xFF);
        const bool first_only = (named & 0x80) != 0;
        const auto type = static_cast<std::uint8_t>(named & 0x7F);
        bool done = false;
        for (auto &member : team_of(*campaign_))
        {
            if (done || (!whole_party && member.slot != selected_character_.index))
            {
                continue;
            }
            auto &items = member.record.items;
            for (auto item = items.begin(); item != items.end() && !done;)
            {
                if (item->type != type)
                {
                    ++item;
                    continue;
                }
                if ((item->flags & original::not_joinable) == 0 && item->quantity > 1 && first_only)
                {
                    --item->quantity;
                    ++item;
                }
                else
                {
                    item = items.erase(item);
                }
                done = first_only;
            }
            keep(member);
        }
        reply = character_reply(selected_character_);
        break;
    }
    case swap_item_lists:
    {
        // 0 stashes every member's items in its second list, anything else
        // gives them back, ahead of what is there (EclTransferCharacterLists).
        read_character();
        const bool stash = value(0) == 0;
        for (auto &member : team_of(*campaign_))
        {
            auto &from = stash ? member.record.items : member.record.alternate_items;
            auto &to = stash ? member.record.alternate_items : member.record.items;
            to.insert(to.begin(), from.begin(), from.end());
            from.clear();
            keep(member);
        }
        reply = character_reply(selected_character_);
        reply.writes.push_back({items_stashed, static_cast<std::uint16_t>(stash ? 1 : 0)});
        break;
    }
    case damage:
        buck_rogers_damage(request);
        return true;
    case load_monster:
        stage_buck_rogers_creatures(request);
        break;
    case combat_command:
    case solo_combat:
        buck_rogers_combat(request, op == solo_combat);
        return true;
    case treasure:
    {
        // Credits to the pool and the templates to the booty (EclTreasure),
        // which the next fight's end offers.
        buck_rogers_pool_ += static_cast<std::uint32_t>(value(0));
        for (std::size_t n = 2; n < operands.size(); ++n)
        {
            const auto number = static_cast<std::size_t>(value(n) & 0xFF);
            const auto &templates = town_->buck_rogers_templates;
            if (number >= 1 && number <= templates.size() &&
                templates[number - 1].type != buck_rogers::Item::no_type)
            {
                buck_rogers_booty_.insert(buck_rogers_booty_.begin(), templates[number - 1]);
            }
        }
        break;
    }
    case shop:
    {
        // The list's templates, its last first, then the shop (EclShop).
        read_character();
        buck_rogers_booty_.clear();
        buck_rogers_pool_ = 0;
        BuckRogersMenu menu;
        menu.kind = BuckRogersMenu::Kind::shop;
        menu.request = request.id;
        menu.stock = original::shop_stock(static_cast<unsigned>(value(0) & 0xFF),
                                          town_->buck_rogers_templates);
        buck_rogers_menu_ = std::move(menu);
        show_buck_rogers_menu(BuckRogersMenu::Kind::shop);
        return true;
    }
    case add_npc:
    {
        // The creature joins as an NPC, its morale half the operand
        // (eclvm.pas EclAddNpc).
        read_character();
        const auto number = static_cast<unsigned>(value(0) & 0xFF);
        const auto found = town_->buck_rogers_creatures.find(number);
        if (found == town_->buck_rogers_creatures.end())
        {
            throw EclError("ADD NPC names no creature " + std::to_string(number));
        }
        auto record = original::GroundRecord{found->second.creature, found->second.items,
                                             town_->buck_rogers_item_types};
        record.creature.npc = static_cast<std::uint8_t>(((value(1) & 0xFF) / 2) | 0x80);
        const auto id = campaign_->recruit("buck-rogers:" + std::to_string(number),
                                           Character(*campaign_->character_rules(),
                                                     original::buck_rogers_draft(record),
                                                     CharacterAppearance{}),
                                           static_cast<unsigned>(record.creature.npc & 0x7F));
        campaign_->keep_game_record(id, original::encode_ground_record(record));
        for (unsigned slot = 0; slot < campaign_->state().slots.size(); ++slot)
        {
            if (campaign_->state().slots[slot] == id)
            {
                selected_character_ = PartySlot{slot};
            }
        }
        reply = character_reply(selected_character_);
        break;
    }
    case program:
    {
        // 0 the team menu, with training; 9 the rest menu; the others
        // nothing (eclvm.pas EclProgram).
        read_character();
        const auto number = value(0) & 0xFF;
        if (number == 0 || number == 9)
        {
            BuckRogersMenu menu;
            menu.kind = number == 0 ? BuckRogersMenu::Kind::team : BuckRogersMenu::Kind::rest;
            menu.request = request.id;
            buck_rogers_menu_ = std::move(menu);
            show_buck_rogers_menu(buck_rogers_menu_->kind);
            return true;
        }
        break;
    }
    case ship_combat:
    {
        // The ship fight is the ship-combat screen's: the script waits for
        // its outcome (finish_ship_combat).
        read_character();
        snapshot_.ship_combat = ShipCombatCall{
            static_cast<unsigned>(value(0) & 0xFF), static_cast<unsigned>(value(1) & 0xFF),
            static_cast<unsigned>(value(2) & 0xFF), static_cast<unsigned>(value(3) & 0xFF)};
        ship_combat_request_ = request.id;
        snapshot_.continue_ticket = ++next_ticket_;
        snapshot_.phase = TourPhase::ship_combat;
        ++snapshot_.revision;
        return true;
    }
    case picture_mode:
    {
        // The mode (0 the 3D view, 2 the solar map, 3 a picture, 4 the
        // wilderness, 1 as it is), then the picture (eclvm.pas
        // EclSetPictureMode).
        const auto mode = static_cast<std::uint16_t>(value(0) & 0xFF);
        const std::array writes{EclMemoryWrite{picture_mode_cell, mode}};
        if (!machine_.refresh_host_variables(request.id, writes))
        {
            throw EclError("PICTURE MODE's cell is not the script's");
        }
        update_solar_map();
        auto shown = request;
        shown.instruction->opcode = picture_command;
        shown.instruction->operands = {operands.at(1)};
        shown.arguments = {request.arguments.at(1)};
        return handle_town_host(shown);
    }
    default:
        return false;
    }
    if (!machine_.resume_host(request.id, reply))
    {
        throw EclError("Buck Rogers host reply rejected");
    }
    ++snapshot_.revision;
    return true;
}

void RolfTourSession::update_solar_map()
{
    snapshot_.solar_map = machine_.variable(picture_mode_cell) == orbit_picture_mode ||
                          machine_.variable(area_kind) == orbit_area;
}

bool RolfTourSession::finish_ship_combat(std::uint64_t ticket, ShipCombatOutcome outcome)
{
    if (snapshot_.phase != TourPhase::ship_combat || ticket != snapshot_.continue_ticket ||
        !ship_combat_request_)
    {
        return false;
    }
    EclHostReply reply = character_reply(selected_character_);
    reply.writes.push_back(
        {ship_combat_outcome, static_cast<std::uint16_t>(static_cast<unsigned>(outcome))});
    if (!machine_.resume_host(std::exchange(ship_combat_request_, 0), reply))
    {
        throw EclError("Ship fight reply rejected");
    }
    snapshot_.ship_combat.reset();
    snapshot_.phase = TourPhase::running;
    ++snapshot_.revision;
    advance(0);
    return true;
}

// DAMAGE (eclvm.pas EclDamage): dice of damage to the whole team, the
// current character or a member at random, each saving by the record's
// saving throws; or a number of attacks on members at random, each rolled
// against the member's Armor Class.
void RolfTourSession::buck_rogers_damage(const EclRequest &request)
{
    read_character();
    const auto value = [&](std::size_t n)
    {
        return static_cast<unsigned>(machine_.operand_value(request.instruction->operands.at(n)) &
                                     0xFF);
    };
    auto random =
        RandomState{(static_cast<std::uint64_t>(machine_.host_random(request.id, 65536)) << 16) |
                    machine_.host_random(request.id, 65536)};
    const auto attack = value(0), count = value(1), sides = value(2), bonus = value(3),
               target = value(4);
    const auto roll = [&]
    {
        int total = static_cast<int>(bonus);
        for (unsigned die = 0; die < count; ++die)
        {
            total += roll_die(random, static_cast<int>(std::max(sides, 1U)));
        }
        return total;
    };
    auto team = team_of(*campaign_);
    if (team.empty())
    {
        throw EclError("DAMAGE needs a team");
    }
    auto amount = roll();
    const bool damage_on_save = (attack & 0x10) != 0;
    const auto hurt = [&](TeamPlace &member, int harm)
    {
        // Only a member who can act is hurt (eclmenu.pas EclInflictDamage).
        if (!can_act(member.record.creature))
        {
            return;
        }
        const bool dies = member.record.creature.hit_points + 10 < harm;
        int dying = 0;
        original::inflict_ground_damage(member.record.creature, harm, dying);
        campaign_->keep_game_record(member.id, original::encode_ground_record(member.record));
        snapshot_.dialogue = join_script_text(
            std::move(snapshot_.dialogue),
            upper(member.record.creature.name) +
                (dies ? " DIES." : " IS HIT FOR " + std::to_string(harm) + " POINTS OF DAMAGE."));
    };
    const auto saves = [&](const TeamPlace &member, int category)
    {
        return original::roll_ground_save(member.record.creature, category,
                                          static_cast<std::int8_t>(attack & 0x1F), false, random);
    };
    const auto at_random = [&]() -> TeamPlace &
    {
        return team.at(
            static_cast<std::size_t>(roll_die(random, static_cast<int>(team.size())) - 1));
    };
    if ((attack & 0x80) != 0)
    {
        const auto category = static_cast<int>(target & 7);
        if ((attack & 0x40) != 0)
        {
            for (auto &member : team)
            {
                if ((attack & 0x20) != 0 || !saves(member, category) || damage_on_save)
                {
                    hurt(member, amount);
                }
            }
        }
        else if ((target & 0x80) != 0)
        {
            const auto current =
                std::ranges::find(team, selected_character_.index, &TeamPlace::slot);
            if (current != team.end() &&
                (category == 0 || !saves(*current, category - 1) || damage_on_save))
            {
                hurt(*current, amount);
            }
        }
        else
        {
            auto &member = at_random();
            if (!saves(member, category) || damage_on_save)
            {
                hurt(member, amount);
            }
        }
    }
    else
    {
        for (unsigned n = 0; n < attack; ++n)
        {
            auto &member = at_random();
            const int natural = roll_die(random, 20);
            const int rolled = natural == 20 ? 100 : natural;
            if (natural > 1 &&
                rolled + static_cast<int>(target) > member.record.creature.armor_class[0])
            {
                hurt(member, amount);
            }
            amount = roll();
        }
    }
    const auto after = team_of(*campaign_);
    if (std::ranges::none_of(after,
                             [](const TeamPlace &member)
                             {
                                 return member.record.creature.enabled != 0;
                             }))
    {
        snapshot_.dialogue =
            join_script_text(std::move(snapshot_.dialogue), "THE WHOLE TEAM IS KILLED!");
        snapshot_.phase = TourPhase::defeated;
        ++snapshot_.revision;
        return;
    }
    if (!machine_.resume_host(request.id, character_reply(selected_character_)))
    {
        throw EclError("DAMAGE's reply rejected");
    }
    ++snapshot_.revision;
}

namespace
{
// Zone 1's cells the fight reads and its end writes (types.pas TZone1): the
// encounter's morale, the result, the creatures defeated, the to-hit and
// movement the script gave, whether the fallen enemies' gear is booty,
// whether the fallen get up again, and zone 0's cell that skips treatment.
constexpr EclAddress morale_cell{0x7EC6}, combat_result_cell{0x7EC7}, defeated_cell{0x7EC8},
    monster_to_hit{0x7F70}, party_to_hit{0x7F71}, movement_bonus{0x7F72},
    skips_combat_items{0x7EE3}, revives_team{0x7EE6}, skips_treatment{0x4B80},
    wilderness_ground{0x4BB7};
// How a fight ended (postcbt.pas RESULT_*).
constexpr std::uint16_t result_won = 0, result_team_beaten = 0x80, result_team_fled = 0x81;
// The most creatures on the party list (eclvm.pas EclLoadMonster).
constexpr std::size_t creature_limit = 63;

// The member a menu serves: the current character, or, while the script's
// current character is a creature it loaded, the party's selected member,
// or the first.
[[nodiscard]] unsigned acting_slot(const CampaignParty &party, PartySlot current)
{
    const auto &slots = party.state().slots;
    for (const auto slot : {current.index, party.state().selected_slot.index})
    {
        if (slot < slots.size() && slots[slot])
        {
            return slot;
        }
    }
    for (unsigned slot = 0; slot < slots.size(); ++slot)
    {
        if (slots[slot])
        {
            return slot;
        }
    }
    throw EclError("A menu needs a member of the team");
}

[[nodiscard]] std::vector<original::TeamMember> team_members(const std::vector<TeamPlace> &team)
{
    std::vector<original::TeamMember> members;
    for (const auto &place : team)
    {
        members.push_back({place.record.creature, place.record.items, false, false});
    }
    return members;
}
} // namespace

std::uint16_t RolfTourSession::buck_rogers_difficulty_level() const noexcept
{
    // The settings run Novice to Admiral, as the levels do.
    return static_cast<std::uint16_t>(difficulty_setting(difficulty_));
}

void RolfTourSession::configure_buck_rogers()
{
    // GameInit: Trooper, the area kind 1, message speed 4 (START_MESSAGE_SPEED).
    machine_.bind_variable(EclAddress{0x4BB6}, buck_rogers_difficulty_level());
    machine_.bind_variable(area_kind, 1);
    machine_.bind_variable(EclAddress{0x4BFC}, 4);
    // The ship's 108 bytes from 4D00h: its numbers of two bytes (the name's
    // 16 bytes, then words to 2Fh) as words, the rest as bytes (types.pas TShip).
    constexpr std::uint16_t ship = 0x4D00;
    constexpr std::size_t first_word = 16, last_word = 48;
    const auto &bytes = town_->buck_rogers_party_ship;
    for (std::size_t at = 0; at < bytes.size(); ++at)
    {
        const auto cell = EclAddress(static_cast<std::uint16_t>(ship + at));
        if (at >= first_word && at <= last_word && at % 2 == 0)
        {
            machine_.bind_variable(cell,
                                   static_cast<std::uint16_t>(bytes[at] | bytes[at + 1] << 8));
        }
        else
        {
            machine_.bind_variable(cell, bytes[at]);
        }
    }
}

std::string RolfTourSession::buck_rogers_item_name(const buck_rogers::Item &item) const
{
    // Its words in reverse order (charhlp.pas PrintItemString).
    std::string name;
    const auto &words = town_->buck_rogers_item_words;
    for (const auto part : item.name_parts)
    {
        if (part != 0 && part < words.size())
        {
            name = words[part] + (name.empty() ? "" : " ") + name;
        }
    }
    if (item.plus != 0)
    {
        name += (item.plus > 0 ? " +" : " ") + std::to_string(item.plus);
    }
    return name.empty() ? std::string("?") : name;
}

void RolfTourSession::stage_buck_rogers_creatures(const EclRequest &request)
{
    const auto value = [&](std::size_t n)
    {
        return static_cast<unsigned>(machine_.operand_value(request.instruction->operands.at(n)) &
                                     0xFF);
    };
    const auto number = value(0), count = value(1), icon = value(2);
    const auto found = town_->buck_rogers_creatures.find(number);
    if (found == town_->buck_rogers_creatures.end())
    {
        throw EclError("LOAD MONSTER names no creature " + std::to_string(number));
    }
    const auto disk = town_->script_banks.at(current_script_);
    const auto icons = town_->buck_rogers_icons.find(disk);
    auto art = icons == town_->buck_rogers_icons.end()
                   ? std::nullopt
                   : creature_art(icons->second, icon, nullptr, false, game());
    if (!art)
    {
        throw EclError("Invalid Buck Rogers combat icon " + std::to_string(icon));
    }
    const auto &creature = found->second.creature;
    // C_Side.TEAM: a creature that fights beside the team.
    const bool ally = creature.side == 0;
    const auto profile = original::encode_ground_record(
        {creature, found->second.items, town_->buck_rogers_item_types});
    for (unsigned n = 0; n < count && staged_enemies_.size() + staged_allies_.size() +
                                              campaign_->state().roster.size() <
                                          creature_limit;
         ++n)
    {
        rules::Participant fighter;
        const auto id =
            ally ? static_cast<rules::EntityId>(opengold::first_ally_id + staged_allies_.size())
                 : static_cast<rules::EntityId>(1000 + staged_enemies_.size());
        fighter.id = id;
        fighter.definition = "buck-rogers:" + std::to_string(number);
        fighter.name =
            ally ? creature.name : creature.name + " " + std::to_string(staged_enemies_.size() + 1);
        fighter.side = ally ? rules::Side::party : rules::Side::opposition;
        fighter.character_profile = profile;
        fighter.intelligence =
            creature.abilities[static_cast<std::size_t>(original::BuckRogersAbility::intelligence)];
        fighter.morale = creature.npc & 0x7F;
        auto drawn = *art;
        drawn.entity = id;
        if (ally)
        {
            staged_allies_.push_back(std::move(fighter));
            staged_ally_art_.push_back(std::move(drawn));
        }
        else
        {
            staged_enemies_.push_back(std::move(fighter));
            staged_art_.push_back(std::move(drawn));
            staged_records_.push_back(number);
        }
    }
}

void RolfTourSession::buck_rogers_combat(const EclRequest &request, bool solo)
{
    read_character();
    if (staged_enemies_.empty() && !solo)
    {
        // No one hostile: the fight's end runs at once (EclCombat).
        finish_buck_rogers_fight(request.id, result_won, 0);
        return;
    }
    const auto kind = machine_.variable(area_kind) & 0xFF;
    const auto set = buck_rogers_tile_set(kind);
    const auto &tiles = town_->buck_rogers_tile_sets.at(std::string(set.file));
    const auto pose = snapshot_.pose;
    const auto seed = (static_cast<std::uint32_t>(machine_.host_random(request.id, 65536)) << 16) |
                      machine_.host_random(request.id, 65536);
    auto arena = buck_rogers_battlefield(map_, pose.x, pose.y, kind, current_script_,
                                         machine_.variable(wilderness_ground) & 0xFF, seed,
                                         static_cast<unsigned>(tiles.size()));
    encounter_records_ = staged_records_;
    encounter_ = opengold::CampaignEncounter{
        std::move(arena.field),
        std::move(staged_enemies_),
        std::move(staged_art_),
        tiles,
        pose.facing,
        static_cast<opengold::Surprise>(machine_.variable(game_cell(ecl_encounter_surprise)) & 3)};
    staged_enemies_.clear();
    staged_art_.clear();
    encounter_->allies = std::move(staged_allies_);
    staged_allies_.clear();
    for (auto &art : staged_ally_art_)
    {
        encounter_->art.push_back(std::move(art));
    }
    staged_ally_art_.clear();
    // The team fights as its CHARS icons, the record's and 128 on
    // (saveload.pas AddCharToParty, maptiles.pas LoadIconAnim).
    for (const auto &member : team_of(*campaign_))
    {
        if (auto art = creature_art(town_->buck_rogers_team_icons, member.record.creature.icon,
                                    nullptr, false, game()))
        {
            art->entity = member.id;
            encounter_->art.push_back(std::move(*art));
        }
    }
    // Bit 10h of the area's kind is zero gravity (CbtActs, CombatAi).
    encounter_->zero_gravity = (kind & 0x10) != 0;
    encounter_->outdoors = kind == mars_wilderness || kind == venus_wilderness;
    if (solo)
    {
        // SOLO COMBAT: the current character alone (EclSoloCombat).
        encounter_->champion =
            campaign_->state().slots.at(acting_slot(*campaign_, selected_character_));
        if (!encounter_->champion)
        {
            throw EclError("SOLO COMBAT needs a current character");
        }
    }
    encounter_distance_ = std::min({encounter_distance_, room_ahead(), 2U});
    encounter_->distance = encounter_distance_;
    encounter_->enemy_quarters = dealt_quarters(monster_sides_, encounter_->enemies.size());
    encounter_->morale = std::min<unsigned>(machine_.variable(morale_cell) & 0xFF, 100);
    apply_original_difficulty(*encounter_, difficulty_, game());
    combat_request_ = request.id;
    showing_monster_picture_ =
        monster_picture_id_ && area_resources().animations.contains(*monster_picture_id_);
    snapshot_.phase = TourPhase::combat;
    ++snapshot_.revision;
}

bool RolfTourSession::resolve_buck_rogers_combat(const rules::Snapshot &result)
{
    if (snapshot_.phase != TourPhase::combat || !encounter_ || !combat_request_ || !campaign_ ||
        campaign_->in_combat() || result.outcome == rules::Outcome::ongoing ||
        result.identity != campaign_->identity())
    {
        return false;
    }
    // The fallen enemies (postcbt.pas DetermineLootExp): their experience,
    // their credits to the pool and, unless the script said not, their gear
    // to the booty, not readied.
    std::uint32_t experience = 0;
    unsigned defeated = 0;
    const bool solo = encounter_->champion != 0;
    const bool takes_gear = (machine_.variable(skips_combat_items) & 0xFF) != 1;
    for (const auto &unit : result.combatants)
    {
        if (unit.side != rules::Side::opposition || unit.id < 1000)
        {
            continue;
        }
        const auto record = original::buck_rogers_record_of(unit.persistent.resources);
        const bool down = unit.dead || unit.hit_points <= 0 || !unit.conscious;
        if (!record || unit.fled || !down)
        {
            continue;
        }
        ++defeated;
        experience += record->creature.experience_award;
        if (!solo)
        {
            buck_rogers_pool_ += record->creature.credits;
            for (auto item : record->items)
            {
                if (takes_gear && (item.flags & 0x40) == 0)
                {
                    item.flags =
                        static_cast<std::uint8_t>(item.flags & ~buck_rogers::Item::readied);
                    buck_rogers_booty_.insert(buck_rogers_booty_.begin(), item);
                }
            }
        }
    }
    auto team = team_of(*campaign_);
    const bool won = result.outcome == rules::Outcome::victory;
    const bool fled = result.outcome == rules::Outcome::fled;
    std::uint16_t combat_result = won ? result_won : fled ? result_team_fled : result_team_beaten;
    if (!won && !fled)
    {
        // The team is destroyed (postcbt.pas PostCombat).
        snapshot_.dialogue = "YOUR TEAM HAS BEEN DESTROYED.";
        snapshot_.phase = TourPhase::defeated;
        encounter_.reset();
        ++snapshot_.revision;
        return true;
    }
    if (won && experience > 0)
    {
        // Shared by the members who can still act (AwardExperience), all of
        // it to a solo fighter.
        std::size_t sharing = 0;
        for (const auto &member : team)
        {
            sharing += member.record.creature.enabled != 0 ? 1 : 0;
        }
        const auto share =
            solo ? experience
                 : static_cast<std::uint32_t>(experience / std::max<std::size_t>(sharing, 1));
        for (auto &member : team)
        {
            if (solo ? member.id == encounter_->champion : member.record.creature.enabled != 0)
            {
                member.record.creature.experience += share;
            }
        }
        snapshot_.dialogue = join_script_text(std::move(snapshot_.dialogue),
                                              "EACH MEMBER RECEIVES " + std::to_string(share) +
                                                  " EXPERIENCE POINTS.");
    }
    // The wounded (DetermineCombatResults), then the medics (TreatInjuries).
    auto members = team_members(team);
    if (!fled)
    {
        original::recover_after_fight(members, (machine_.variable(revives_team) & 0xFF) != 0);
    }
    if ((machine_.variable(skips_treatment) & 0xFF) == 0)
    {
        auto random = RandomState{
            (static_cast<std::uint64_t>(machine_.host_random(combat_request_, 65536)) << 16) |
            machine_.host_random(combat_request_, 65536)};
        // The antidote is used whenever one is carried, as the player is
        // asked first; the walk's and the game's screens ask in B3's next part.
        const auto report = original::treat_injuries(
            members,
            [](std::size_t, std::size_t)
            {
                return true;
            },
            random);
        (void)report;
    }
    for (std::size_t n = 0; n < team.size(); ++n)
    {
        team[n].record.creature = members[n].character;
        team[n].record.items = members[n].items;
        campaign_->keep_game_record(team[n].id, original::encode_ground_record(team[n].record));
    }
    if (fled)
    {
        // Members left on the field are left behind (DetermineCombatResults,
        // CharGen.RemoveCreature).
        for (const auto &unit : result.combatants)
        {
            if (unit.side == rules::Side::party && unit.id < 1000 && !unit.fled &&
                std::ranges::find(campaign_->state().slots, unit.id) !=
                    campaign_->state().slots.end())
            {
                campaign_->remove(unit.id);
            }
        }
        buck_rogers_booty_.clear();
        buck_rogers_pool_ = 0;
    }
    const auto request = std::exchange(combat_request_, 0);
    encounter_.reset();
    snapshot_.phase = TourPhase::running;
    finish_buck_rogers_fight(request, combat_result, defeated);
    return true;
}

void RolfTourSession::finish_buck_rogers_fight(std::uint64_t request, std::uint16_t combat_result,
                                               unsigned defeated)
{
    // The script's own changes for this fight end with it (PostCombat).
    const std::array<EclMemoryWrite, 7> cleared{
        {{combat_result_cell, combat_result},
         {defeated_cell, static_cast<std::uint16_t>(defeated)},
         {monster_to_hit, 0},
         {party_to_hit, 0},
         {movement_bonus, 0},
         {skips_combat_items, 0},
         {revives_team, 0}}};
    if (!machine_.refresh_host_variables(request, cleared))
    {
        throw EclError("The fight's end cells are not the script's");
    }
    if (!buck_rogers_booty_.empty() || buck_rogers_pool_ != 0)
    {
        BuckRogersMenu menu;
        menu.kind = BuckRogersMenu::Kind::booty;
        menu.request = request;
        buck_rogers_menu_ = std::move(menu);
        show_buck_rogers_menu(BuckRogersMenu::Kind::booty);
        return;
    }
    if (!machine_.resume_host(request, character_reply(selected_character_)))
    {
        throw EclError("The fight's end was rejected");
    }
    ++snapshot_.revision;
}

void RolfTourSession::show_buck_rogers_menu(BuckRogersMenu::Kind kind,
                                            std::vector<rules::Message> notice)
{
    auto &menu = *buck_rogers_menu_;
    menu.kind = kind;
    menu.choices.clear();
    std::vector<rules::Message> choices;
    using Action = BuckRogersMenu::Action;
    const auto add = [&](rules::Message label, Action action, std::size_t index = 0)
    {
        choices.push_back(std::move(label));
        menu.choices.push_back({action, index});
    };
    const auto id = campaign_->state().slots.at(acting_slot(*campaign_, selected_character_));
    const auto shopper = id ? original::buck_rogers_record_of(campaign_->game_record(id))
                            : std::optional<original::GroundRecord>{};
    const auto credits = [&]
    {
        return rules::Message{"{name} has {credits} credits; the pool holds {pool}.",
                              {{"name", shopper ? shopper->creature.name : std::string{}},
                               {"credits", std::to_string(shopper ? shopper->creature.credits : 0)},
                               {"pool", std::to_string(buck_rogers_pool_)}}};
    };
    std::vector<rules::Message> dialogue = std::move(notice);
    const auto multiplier = static_cast<std::uint8_t>(machine_.variable(EclAddress{0x7F6D}) & 0xFF);
    switch (kind)
    {
    case BuckRogersMenu::Kind::booty:
        dialogue.push_back({"The booty:", {}});
        for (std::size_t n = 0; n < buck_rogers_booty_.size(); ++n)
        {
            add({"Take {item}", {{"item", buck_rogers_item_name(buck_rogers_booty_[n])}}},
                Action::take_item, n);
        }
        if (buck_rogers_pool_ != 0)
        {
            dialogue.push_back(credits());
            add({"Take the credits", {}}, Action::take_credits);
            add({"Divvy", {}}, Action::share_credits);
        }
        add({"Pool", {}}, Action::pool_credits);
        add({"Exit", {}}, Action::leave);
        break;
    case BuckRogersMenu::Kind::shop:
        dialogue.push_back(credits());
        add({"Buy", {}}, Action::open_buy);
        add({"Sell", {}}, Action::open_sell);
        add({"Ammo", {}}, Action::open_ammunition);
        add({"Pool", {}}, Action::pool_credits);
        if (buck_rogers_pool_ != 0)
        {
            add({"Take the credits", {}}, Action::take_credits);
            add({"Divvy", {}}, Action::share_credits);
        }
        add({"Exit", {}}, Action::leave);
        break;
    case BuckRogersMenu::Kind::shop_buy:
        dialogue.push_back(credits());
        for (std::size_t n = 0; n < menu.stock.size(); ++n)
        {
            add({"{item}: {price} credits",
                 {{"item", buck_rogers_item_name(menu.stock[n])},
                  {"price", std::to_string(original::shop_price(menu.stock[n], multiplier))}}},
                Action::buy, n);
        }
        add({"Back", {}}, Action::back);
        break;
    case BuckRogersMenu::Kind::shop_sell:
        dialogue.push_back(credits());
        for (std::size_t n = 0; shopper && n < shopper->items.size(); ++n)
        {
            add({"{item}: {price} credits",
                 {{"item", buck_rogers_item_name(shopper->items[n])},
                  {"price", std::to_string(original::sale_price(shopper->items[n]))}}},
                Action::sell, n);
        }
        add({"Back", {}}, Action::back);
        break;
    case BuckRogersMenu::Kind::shop_ammunition:
        dialogue.push_back(credits());
        for (std::size_t n = 0; shopper && n < shopper->items.size(); ++n)
        {
            const auto &weapon = shopper->items[n];
            if (!original::takes_ammunition(weapon, town_->buck_rogers_item_types))
            {
                continue;
            }
            const auto clip =
                original::ammunition_offer(weapon, town_->buck_rogers_item_types, false);
            const auto fill =
                original::ammunition_offer(weapon, town_->buck_rogers_item_types, true);
            add({"{item}: {rounds} rounds for {price} credits",
                 {{"item", buck_rogers_item_name(weapon)},
                  {"rounds", std::to_string(clip.rounds)},
                  {"price", std::to_string(clip.cost)}}},
                Action::ammunition_clip, n);
            add({"{item}: {rounds} rounds for {price} credits",
                 {{"item", buck_rogers_item_name(weapon)},
                  {"rounds", std::to_string(fill.rounds)},
                  {"price", std::to_string(fill.cost)}}},
                Action::ammunition_fill, n);
        }
        add({"Back", {}}, Action::back);
        break;
    case BuckRogersMenu::Kind::team:
        dialogue.push_back({"Who will train?", {}});
        for (std::size_t slot = 0; slot < campaign_->state().slots.size(); ++slot)
        {
            const auto member = campaign_->state().slots[slot];
            const auto record =
                member ? original::buck_rogers_record_of(campaign_->game_record(member))
                       : std::optional<original::GroundRecord>{};
            if (record && original::training_refusal(record->creature, game()) ==
                              original::TrainingRefusal::none)
            {
                add({"Train {name}", {{"name", record->creature.name}}}, Action::train, slot);
            }
        }
        add({"Exit", {}}, Action::leave);
        break;
    case BuckRogersMenu::Kind::rest:
        add({"Rest a day", {}}, Action::rest_a_day);
        add({"Exit", {}}, Action::leave);
        break;
    }
    show_hall_menu(std::move(dialogue), std::move(choices));
}

bool RolfTourSession::choose_in_buck_rogers_menu(std::size_t choice)
{
    auto &menu = *buck_rogers_menu_;
    if (choice >= menu.choices.size())
    {
        return false;
    }
    using Action = BuckRogersMenu::Action;
    using Kind = BuckRogersMenu::Kind;
    const auto picked = menu.choices[choice];
    const auto id = campaign_->state().slots.at(acting_slot(*campaign_, selected_character_));
    auto team = team_of(*campaign_);
    const auto current =
        std::ranges::find(team, acting_slot(*campaign_, selected_character_), &TeamPlace::slot);
    std::vector<rules::Message> notice;
    const auto keep_team = [&](std::vector<original::TeamMember> &members)
    {
        for (std::size_t n = 0; n < team.size(); ++n)
        {
            team[n].record.creature = members[n].character;
            team[n].record.items = members[n].items;
            campaign_->keep_game_record(team[n].id, original::encode_ground_record(team[n].record));
        }
    };
    const auto keep_current = [&](const original::TeamMember &member)
    {
        current->record.creature = member.character;
        current->record.items = member.items;
        campaign_->keep_game_record(current->id, original::encode_ground_record(current->record));
    };
    const auto multiplier = static_cast<std::uint8_t>(machine_.variable(EclAddress{0x7F6D}) & 0xFF);
    original::CreditPool pool{buck_rogers_pool_};
    auto next = menu.kind;
    switch (picked.action)
    {
    case Action::take_item:
    {
        if (current == team.end())
        {
            return false;
        }
        // The current character takes it, joined to a like bundle
        // (shop.pas GiveItemCopyToCreature).
        original::TeamMember member{current->record.creature, current->record.items, false, false};
        original::CreditPool free{0};
        auto item = buck_rogers_booty_.at(picked.index);
        const auto value = item.value;
        item.value = 0;
        if (original::buy_item(member, item, 0, free) == original::PurchaseResult::overloaded)
        {
            notice.push_back({"{name} cannot carry any more.", {{"name", member.character.name}}});
            break;
        }
        member.items.back().value = value;
        keep_current(member);
        buck_rogers_booty_.erase(buck_rogers_booty_.begin() +
                                 static_cast<std::ptrdiff_t>(picked.index));
        break;
    }
    case Action::take_credits:
        if (current == team.end())
        {
            return false;
        }
        {
            original::TeamMember member{current->record.creature, current->record.items, false,
                                        false};
            original::take_credits(member.character, pool, pool.credits);
            keep_current(member);
        }
        break;
    case Action::share_credits:
    case Action::pool_credits:
    {
        auto members = team_members(team);
        if (picked.action == Action::share_credits)
        {
            original::share_credits(members, pool);
        }
        else
        {
            original::pool_credits(members, pool);
        }
        keep_team(members);
        break;
    }
    case Action::open_buy:
        next = Kind::shop_buy;
        break;
    case Action::open_sell:
        next = Kind::shop_sell;
        break;
    case Action::open_ammunition:
        next = Kind::shop_ammunition;
        break;
    case Action::back:
        next = Kind::shop;
        break;
    case Action::buy:
    case Action::sell:
    case Action::ammunition_clip:
    case Action::ammunition_fill:
    {
        if (current == team.end())
        {
            return false;
        }
        original::TeamMember member{current->record.creature, current->record.items, false, false};
        if (picked.action == Action::buy)
        {
            const auto bought =
                original::buy_item(member, menu.stock.at(picked.index), multiplier, pool);
            if (bought == original::PurchaseResult::not_enough_money)
            {
                notice.push_back({"Not enough credits.", {}});
            }
            else if (bought == original::PurchaseResult::overloaded)
            {
                notice.push_back(
                    {"{name} cannot carry any more.", {{"name", member.character.name}}});
            }
        }
        else if (picked.action == Action::sell)
        {
            original::sell_item(member, picked.index);
        }
        else
        {
            const auto bought =
                original::buy_ammunition(member, picked.index, town_->buck_rogers_item_types,
                                         picked.action == Action::ammunition_fill, pool);
            if (bought == original::AmmunitionResult::not_enough_money)
            {
                notice.push_back({"Not enough credits.", {}});
            }
            else if (bought == original::AmmunitionResult::already_full)
            {
                notice.push_back({"The weapon is full.", {}});
            }
        }
        keep_current(member);
        break;
    }
    case Action::train:
    {
        // One level, free (chargen.pas TrainingMenu); the skill points it
        // gives are spent on the skill screen.
        auto member =
            std::ranges::find(team, static_cast<unsigned>(picked.index), &TeamPlace::slot);
        if (member == team.end())
        {
            return false;
        }
        RandomState random{
            (static_cast<std::uint64_t>(machine_.host_random(menu.request, 65536)) << 16) |
            machine_.host_random(menu.request, 65536)};
        original::train_buck_rogers_character(member->record.creature, game(), random);
        campaign_->keep_game_record(member->id, original::encode_ground_record(member->record));
        notice.push_back({"{name} trains to level {level}.",
                          {{"name", member->record.creature.name},
                           {"level", std::to_string(member->record.creature.level)}}});
        break;
    }
    case Action::rest_a_day:
    {
        // A day's rest (gametime.pas RestAndHeal).
        auto members = team_members(team);
        original::rest_and_heal(members, original::rest_days);
        keep_team(members);
        campaign_->advance_time(std::chrono::hours{24});
        notice.push_back({"The team rests a day.", {}});
        break;
    }
    case Action::leave:
        buck_rogers_pool_ = pool.credits;
        close_buck_rogers_menu();
        return true;
    }
    (void)id;
    buck_rogers_pool_ = pool.credits;
    show_buck_rogers_menu(next, std::move(notice));
    return true;
}

void RolfTourSession::close_buck_rogers_menu()
{
    const auto request = buck_rogers_menu_->request;
    // Booty or credits left behind are lost (postcbt.pas LootMenu, shop.pas).
    buck_rogers_booty_.clear();
    buck_rogers_pool_ = 0;
    buck_rogers_menu_.reset();
    snapshot_.host_dialogue.clear();
    snapshot_.host_choices.clear();
    snapshot_.choices.clear();
    snapshot_.phase = TourPhase::running;
    if (!machine_.resume_host(request, character_reply(selected_character_)))
    {
        throw EclError("The menu's end was rejected");
    }
    ++snapshot_.revision;
    advance(0);
}

} // namespace opengold::por
