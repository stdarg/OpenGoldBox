// The Buck Rogers games' script commands the session answers, where their
// table changed or added commands, and the current character's record as
// their scripts read and write it (docs/games/countdown.md, "Scripts"; the
// game in the engine, B3).
#include "opengold/rolf_tour.h"
#include "opengold/buck_rogers_characters.h"
#include "opengold/buck_rogers_rules.h"

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
                       skill_check_with_damage = 0x49, picture_mode = 0x4C, picture_command = 0x0E;
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

} // namespace opengold::por
