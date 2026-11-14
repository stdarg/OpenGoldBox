// Pools of Darkness's script commands the session answers where its numbers
// mean other commands in the other games (docs/games/darkness.md, Scripts).
#include "opengold/rolf_tour.h"
#include "area_loading.h"
#include "opengold/original_spells.h"
#include "opengold/vga_picture.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace opengold::por
{
namespace
{

// The map number from which LOAD FILES loads no map, and the wall set that
// keeps the current one.
constexpr unsigned first_non_map = 0x7F, keep_walls = 255;
// PLAY SOUND's footstep (its music starts from 80h).
constexpr unsigned footstep_sound = 10;

// The current character's record, from 401h: the bytes of its fields the
// scripts read (docs/games/darkness.md, Creatures).
constexpr std::uint16_t record_cell = 0x401;
constexpr std::size_t record_size = 510;

namespace field
{
constexpr std::size_t name = 0, abilities = 16, exceptional_strength = 28, race = 173,
                      character_class = 174, age = 176, maximum_hit_points = 178,
                      thief_skills = 315, npc = 327, coins = 331, class_levels = 337, gender = 358,
                      alignment = 359, status = 493, in_play = 494, side = 495, hit_points = 508;
} // namespace field

// Its races' numbers (C_Race): elf 0, half-elf 1, dwarf 2, gnome 3, halfling
// 4, human 5, by the original rules' races (dwarf, elf, gnome, half-elf,
// halfling, half-orc, human). It has no half-orcs: one counts as human.
constexpr std::array<std::uint16_t, 7> race_numbers{2, 0, 3, 1, 4, 5, 5};
// Its statuses: 0 okay, 6 dead.
constexpr std::uint16_t okay = 0, dead = 6;

} // namespace

std::optional<Equipment> RolfTourSession::darkness_item(unsigned number, std::uint64_t request)
{
    constexpr unsigned random_scroll = 0xFF;
    if (number != random_scroll)
    {
        const auto found = town_->item_definitions.find(number);
        return found == town_->item_definitions.end() ? std::nullopt : std::optional{found->second};
    }
    // A scroll of three magic-user spells drawn at random from its 126 (its
    // EclBuildRandomScroll): a scroll's type and words, worth 3,000.
    constexpr std::uint8_t scroll_type = 39;
    constexpr int spell_count = 126, first_new_magic_user_spell = 118;
    ItemRecord scroll;
    scroll.type = scroll_type;
    scroll.name_components = {102, 39, 40};
    scroll.magic_bonus = 1;
    scroll.weight = 1;
    scroll.value = 3000;
    for (auto &held : scroll.effect_codes)
    {
        while (true)
        {
            const auto roll = static_cast<int>(machine_.host_random(request, spell_count)) + 1;
            // Its spells past Secret's, 118 to 126, are all magic-user spells.
            const auto *known =
                opengold::original::spell(opengold::original::canonical_spell(roll, game()));
            if ((known && known->caster_class == opengold::original::SpellClass::magic_user) ||
                roll >= first_new_magic_user_spell)
            {
                held = static_cast<std::uint8_t>(roll);
                break;
            }
        }
    }
    Equipment item;
    if (scroll.type < town_->item_templates.size())
    {
        item.base = town_->item_templates[scroll.type];
    }
    item.bonuses = equipment_bonuses(scroll, item.base);
    item.stored = std::move(scroll);
    return item;
}

void RolfTourSession::stage_darkness_treasure(const EclRequest &request)
{
    if (!campaign_)
    {
        throw EclError("Treasure awards require a campaign party");
    }
    // Operands: platinum, gems, jewelry, the number of items, then the items.
    const auto number = [&request](std::size_t n)
    {
        return request.arguments.at(n).number();
    };
    PendingLoot loot;
    constexpr std::size_t platinum = 4;
    for (std::size_t n = 0; n < 3; ++n)
    {
        loot.wealth[platinum + n] = number(n);
    }
    loot.reward_id = treasure_identity(request.instruction->address);
    loot.include_items = false;
    for (std::size_t n = 0; n < number(3); ++n)
    {
        if (auto item = darkness_item(number(4 + n), request.id))
        {
            item->index = loot.items.size();
            loot.items.push_back(std::move(*item));
        }
    }
    staged_treasure_ = std::move(loot);
}

void RolfTourSession::open_darkness_store(const EclRequest &request)
{
    // Stores 1 to 11; any other number opens the first (its EclShop).
    auto store = request.arguments.at(0).number();
    if (store < 1 || store > town_->stores.size())
    {
        store = 1;
    }
    treasure_.clear();
    for (const auto number : town_->stores.at(store - 1))
    {
        if (auto item = darkness_item(number, request.id))
        {
            // A store's goods are known for what they are.
            item->identify();
            item->index = treasure_.size();
            treasure_.push_back(std::move(*item));
        }
    }
    choose_shopper();
    read_character();
    shop_request_ = request.id;
    snapshot_.phase = TourPhase::shopping;
    snapshot_.continue_ticket = ++next_ticket_;
    snapshot_.choices.clear();
    snapshot_.diagnostic.clear();
    ++snapshot_.revision;
}

bool RolfTourSession::darkness_area_map() const
{
    return game() == GoldBoxGame::pools_of_darkness &&
           machine_.variable(game_cell(ecl_in_3d_area)) == 0;
}

void RolfTourSession::darkness_area_step()
{
    // The party's square on the area map, 38 columns by 15 rows (its
    // MovePartyWilderness), and the square it left.
    constexpr EclAddress column{0x0025}, row{0x0026}, previous_column{0x0017}, previous_row{0x0018};
    constexpr int columns = 38, rows = 15;
    // North and on clockwise to north-west (its dirDeltaX and dirDeltaY).
    constexpr std::array<int, 8> step_columns{0, 1, 1, 1, 0, -1, -1, -1},
        step_rows{-1, -1, 0, 1, 1, 1, 0, -1};
    const auto direction = static_cast<std::size_t>(*pending_movement_) -
                           static_cast<std::size_t>(ExplorationCommand::north);
    const auto from_column = machine_.variable(column), from_row = machine_.variable(row);
    machine_.bind_variable(previous_column, from_column);
    machine_.bind_variable(previous_row, from_row);
    machine_.bind_variable(column, static_cast<std::uint16_t>(std::clamp(
                                       from_column + step_columns.at(direction), 0, columns - 1)));
    machine_.bind_variable(row, static_cast<std::uint16_t>(
                                    std::clamp(from_row + step_rows.at(direction), 0, rows - 1)));
    // A step on the area map takes half a day.
    if (campaign_)
    {
        pass_time(std::chrono::hours{12});
    }
    synchronize_clock();
    ++snapshot_.revision;
}

EclHostReply RolfTourSession::darkness_spell(unsigned spell, EclAddress place, EclAddress member)
{
    constexpr std::uint16_t nobody = 0xFF;
    auto reply = character_reply(selected_character_);
    std::uint16_t found = nobody, caster_slot = 0;
    const auto id = opengold::original::spell_id(
        opengold::original::canonical_spell(static_cast<int>(spell), game()));
    if (!id.empty())
    {
        if (const auto caster = campaign_->cast_exploration_spell(id))
        {
            const auto &slots = campaign_->state().slots;
            found = 0;
            caster_slot =
                static_cast<std::uint16_t>(std::ranges::find(slots, *caster) - slots.begin());
        }
    }
    reply.writes.push_back({place, found});
    reply.writes.push_back({member, caster_slot});
    return reply;
}

void RolfTourSession::darkness_damage(const EclRequest &request)
{
    // DAMAGE: attack, dice, sides, bonus, value (its EclDamage). With attack's
    // 80h its low four bits are the save's modifier and value's low three bits
    // the save: 40h hurts the whole party (20h with no save), else value's 80h
    // the current character (its save is value's bits less 1, none at 0), else
    // a random member; a save halves the damage with 10h and avoids it
    // without. Without 80h attack is a number of attacks on random members,
    // each to hit with value.
    constexpr unsigned special = 0x80, whole_party = 0x40, no_save = 0x20, halves = 0x10,
                       chosen = 0x80;
    // The difficulty a save is shown against, as the other games' scripts.
    constexpr int shown_save_dc = 13;
    const auto number = [&request](std::size_t n)
    {
        return request.arguments.at(n).number();
    };
    const auto attack = number(0), value = number(4);
    rules::HazardAttack harm{0, number(1), number(2), static_cast<int>(number(3)), {}};
    harm.automatic = true;
    if (!(attack & special))
    {
        harm.automatic = false;
        harm.original_attack = static_cast<int>(value);
        snapshot_.dialogue += shoot_arrows(attack, "blow", harm);
        return;
    }
    const auto save_against = [&](int save)
    {
        harm.original_save = save;
        harm.original_save_modifier = static_cast<int>(attack & 0x0F);
        harm.save_dc = shown_save_dc - harm.original_save_modifier;
        harm.half_on_save = (attack & halves) != 0;
    };
    const auto hurt_member = [&](MemberId id)
    {
        const auto result = campaign_->hazard_attack(id, harm);
        snapshot_.dialogue += hurt(result ? std::optional<HazardHit>{{id, *result}} : std::nullopt,
                                   harm.save_dc, harm.sides);
    };
    if (attack & whole_party)
    {
        if (!(attack & no_save))
        {
            save_against(static_cast<int>(value & 7));
        }
        for (const auto id : campaign_->state().slots)
        {
            if (id)
            {
                hurt_member(id);
            }
        }
        return;
    }
    if (value & chosen)
    {
        if ((value & 7) != 0)
        {
            save_against(static_cast<int>(value & 7) - 1);
        }
        const auto id = campaign_->state().slots.at(selected_character_.index);
        if (!id)
        {
            throw EclError("DAMAGE needs a chosen member");
        }
        hurt_member(id);
        return;
    }
    save_against(static_cast<int>(value & 7));
    snapshot_.dialogue += hurt(campaign_->hazard_attack(harm), harm.save_dc, harm.sides);
}

ImageDecodeResult RolfTourSession::darkness_sprite(unsigned id, unsigned distance) const
{
    // Its frames are the monster near, at middle distance and far; the
    // original swaps colors 0 and 13 and draws through 13, so 0 shows through.
    constexpr std::uint8_t see_through = 0;
    const auto record =
        decode_dax_record(area_resources().sprite_archive, static_cast<std::uint8_t>(id));
    const auto animation = record ? decode_vga_animation(*record) : std::nullopt;
    if (!animation || distance >= animation->frames.size())
    {
        return {FormatResult::not_found, {}};
    }
    return {FormatResult::ok, vga_rgba(animation->frames[distance].image,
                                       vga_color_table(animation->palette), see_through)};
}

EclHostReply RolfTourSession::darkness_find_npc(const std::string &name, EclAddress place) const
{
    // The member's place in the party, 80h more when it is out of play, or
    // FFh when no companion has that name (its EclFindNpc).
    constexpr std::uint16_t nobody = 0xFF, out_of_play = 0x80;
    std::uint16_t found = nobody;
    const auto &slots = campaign_->state().slots;
    for (std::size_t n = 0; n < slots.size() && found == nobody; ++n)
    {
        if (!slots[n] || campaign_->member(slots[n]).npc_source.empty())
        {
            continue;
        }
        const auto &member = campaign_->member(slots[n]);
        auto member_name = member.character.sheet().name;
        std::ranges::transform(member_name, member_name.begin(),
                               [](unsigned char c)
                               {
                                   return static_cast<char>(std::toupper(c));
                               });
        if (member_name == name)
        {
            found = static_cast<std::uint16_t>(n | (member.vitals.dead ? out_of_play : 0));
        }
    }
    EclHostReply reply;
    reply.writes = {{place, found}};
    return reply;
}

void RolfTourSession::add_darkness_record(PartySlot slot, EclHostReply &reply) const
{
    std::array<std::uint16_t, record_size> record{};
    const auto id = slot.index < 8 ? campaign_->state().slots[slot.index] : MemberId{};
    if (id)
    {
        const auto &member = campaign_->member(id);
        const auto &sheet = member.character.sheet();
        for (std::size_t n = 0; n < sheet.name.size() && n < 15; ++n)
        {
            record[field::name + n] =
                static_cast<std::uint16_t>(std::toupper(static_cast<unsigned char>(sheet.name[n])));
        }
        // Each ability's permanent and current score, in the original's order.
        using rules::Ability;
        const std::array<Ability, 6> abilities{Ability::strength,     Ability::intelligence,
                                               Ability::wisdom,       Ability::dexterity,
                                               Ability::constitution, Ability::charisma};
        for (std::size_t n = 0; n < abilities.size(); ++n)
        {
            const auto score = static_cast<std::uint16_t>(sheet.scores[abilities[n]]);
            record[field::abilities + 2 * n] = record[field::abilities + 2 * n + 1] = score;
        }
        record[field::maximum_hit_points] = static_cast<std::uint16_t>(sheet.hit_points);
        if (const auto &original = sheet.original)
        {
            record[field::exceptional_strength] = record[field::exceptional_strength + 1] =
                static_cast<std::uint16_t>(original->exceptional_strength);
            record[field::race] = race_numbers.at(static_cast<std::size_t>(original->race));
            // Its classes are numbered as the original rules' professions.
            record[field::character_class] = static_cast<std::uint16_t>(original->profession);
            record[field::age] = static_cast<std::uint16_t>(original->age);
            for (std::size_t n = 0; n < 7; ++n)
            {
                record[field::class_levels + n] =
                    static_cast<std::uint16_t>(original->class_levels[n]);
                record[field::thief_skills + n] =
                    static_cast<std::uint16_t>(original->thief_skills[n]);
            }
            record[field::thief_skills + 7] = static_cast<std::uint16_t>(original->thief_skills[7]);
            record[field::alignment] = static_cast<std::uint16_t>(original->alignment);
        }
        record[field::gender] = member.character.creation_data().gender == "female" ? 1 : 0;
        record[field::npc] = member.npc_source.empty() ? 0 : 0x80;
        // Every member shows the party's platinum, gems and jewelry: the party
        // owns them all.
        const auto coins = script_coins(campaign_->state().wealth, campaign_->coin_rates());
        constexpr std::size_t platinum = 4;
        for (std::size_t n = 0; n < 3; ++n)
        {
            record[field::coins + 2 * n] = static_cast<std::uint16_t>(coins[platinum + n]);
        }
        record[field::status] = member.vitals.dead ? dead : okay;
        record[field::in_play] = member.vitals.dead ? 0 : 1;
        record[field::side] = turncoat(id) ? 1 : 0;
        record[field::hit_points] = static_cast<std::uint16_t>(member.vitals.hit_points);
    }
    // The record's cells the other games' fields were given are given again
    // here, once each: a reply names a cell only once.
    std::erase_if(reply.writes,
                  [](const EclMemoryWrite &write)
                  {
                      return write.address.location >= record_cell &&
                             write.address.location < record_cell + record_size;
                  });
    for (std::size_t n = 0; n < record.size(); ++n)
    {
        reply.writes.push_back(
            {EclAddress(static_cast<std::uint16_t>(record_cell + n)), record[n]});
    }
}

EclHostReply RolfTourSession::darkness_load_files(unsigned map, unsigned walls)
{
    EclHostReply reply;
    // Indoors only: the area map keeps its own picture (its EclLoadFiles).
    if (map < first_non_map && machine_.variable(game_cell(ecl_in_3d_area)) != 0)
    {
        if (!town_->districts.contains(map))
        {
            throw EclError("Resources outside the supported areas (map " + std::to_string(map) +
                           ")");
        }
        change_area(map);
        const auto &here = map_.at(machine_.variable(game_cell(ecl_party_x)),
                                   machine_.variable(game_cell(ecl_party_y)));
        const auto facing = machine_.variable(game_cell(ecl_party_facing));
        reply.writes = {{game_cell(ecl_wall_ahead), here.walls[facing % 4]},
                        {game_cell(ecl_cell_event), here.event_raw},
                        {game_cell(ecl_current_map), static_cast<std::uint16_t>(map)}};
    }
    if (walls != keep_walls)
    {
        const auto set = town_->wall_sets.find(
            {town_->bank, {walls, detail::no_wall_bank, detail::no_wall_bank}});
        if (set == town_->wall_sets.end())
        {
            throw EclError("Unknown wall set " + std::to_string(walls));
        }
        wall_art_ = set->second;
        ++snapshot_.revision;
    }
    return reply;
}

void RolfTourSession::darkness_play_sound(unsigned sound)
{
    // The footstep plays as the other games' does; the other sound effects
    // and the music are not played yet.
    if (sound == footstep_sound)
    {
        ++snapshot_.footsteps;
    }
}

} // namespace opengold::por
