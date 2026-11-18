// The 256-color engine's script commands the session answers where its numbers
// mean other commands in the other games (docs/games/darkness.md, Scripts).
#include "opengold/rolf_tour.h"
#include "area_loading.h"
#include "opengold/original.h"
#include "opengold/original_spells.h"
#include "opengold/vga_picture.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace opengold::por
{
namespace
{

// The map number from which LOAD FILES loads no map, and the wall set that
// keeps the current one.
constexpr unsigned first_non_map = 0x7F, keep_walls = 255;
// PLAY SOUND's footstep, its sound effects (2 to 14, of which the driver plays
// 2 to 12) and its music, whose pieces start from 80h.
constexpr unsigned footstep_sound = 10, first_sound_effect = 2, last_sound_effect = 14,
                   last_played_sound_effect = 12, first_music_sound = 0x80;

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

std::optional<Equipment> RolfTourSession::vga_item(unsigned number, std::uint64_t request)
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

void RolfTourSession::stage_vga_treasure(const EclRequest &request)
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
        if (auto item = vga_item(number(4 + n), request.id))
        {
            item->index = loot.items.size();
            loot.items.push_back(std::move(*item));
        }
    }
    staged_treasure_ = std::move(loot);
}

void RolfTourSession::open_vga_store(const EclRequest &request)
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
        if (auto item = vga_item(number, request.id))
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

bool RolfTourSession::vga_area_map() const
{
    return vga_engine(game()) && machine_.variable(game_cell(ecl_in_3d_area)) == 0;
}

void RolfTourSession::vga_area_step()
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

EclHostReply RolfTourSession::vga_spell(unsigned spell, EclAddress place, EclAddress member)
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

void RolfTourSession::vga_damage(const EclRequest &request)
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

ImageDecodeResult RolfTourSession::vga_sprite(unsigned id, unsigned distance) const
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

EclHostReply RolfTourSession::vga_find_npc(const std::string &name, EclAddress place) const
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

void RolfTourSession::add_vga_record(PartySlot slot, EclHostReply &reply) const
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

EclHostReply RolfTourSession::vga_load_map(unsigned map)
{
    EclHostReply reply;
    // Indoors only: the area map keeps its own picture (its EclLoadFiles).
    if (map >= first_non_map || machine_.variable(game_cell(ecl_in_3d_area)) == 0)
    {
        return reply;
    }
    if (!town_->districts.contains(map))
    {
        throw EclError("Resources outside the supported areas (map " + std::to_string(map) + ")");
    }
    change_area(map);
    // A square off the map has no wall and no event (its GetWallType and
    // GetMapEvent): Treasures' overland may load the next map before it moves
    // the party onto it.
    const auto x = machine_.variable(game_cell(ecl_party_x)),
               y = machine_.variable(game_cell(ecl_party_y));
    const auto facing = machine_.variable(game_cell(ecl_party_facing)) % 4;
    const bool on_map = x < GeoMap::width && y < GeoMap::height;
    reply.writes = {{game_cell(ecl_wall_ahead),
                     static_cast<std::uint16_t>(on_map ? map_.at(x, y).walls[facing] : 0)},
                    {game_cell(ecl_cell_event),
                     static_cast<std::uint16_t>(on_map ? map_.at(x, y).event_raw : 0)},
                    {game_cell(ecl_current_map), static_cast<std::uint16_t>(map)}};
    return reply;
}

void RolfTourSession::vga_load_wall_slots(const std::array<unsigned, 3> &slots)
{
    const auto set = town_->wall_sets.find({town_->bank, slots});
    if (set == town_->wall_sets.end())
    {
        throw EclError("Unknown wall sets " + std::to_string(slots[0]) + ", " +
                       std::to_string(slots[1]) + ", " + std::to_string(slots[2]));
    }
    wall_slots_ = slots;
    wall_art_ = set->second;
    ++snapshot_.revision;
}

EclHostReply RolfTourSession::vga_load_files(unsigned map, unsigned walls)
{
    auto reply = vga_load_map(map);
    if (walls == keep_walls)
    {
        return reply;
    }
    // On Treasures' overland the set is the map's picture (OVER), drawn from
    // above (its LoadWallSet).
    if (treasures_overland())
    {
        treasures_overland_picture_ = walls;
        return reply;
    }
    // The set fills the first slot and empties the others, unless it is there
    // already.
    if (walls != wall_slots_[0])
    {
        vga_load_wall_slots({walls, detail::no_wall_bank, detail::no_wall_bank});
    }
    return reply;
}

EclHostReply RolfTourSession::treasures_load_wall_sets(unsigned map,
                                                       const std::array<unsigned, 3> &sets)
{
    auto reply = vga_load_map(map);
    // On the overland each slot's set loads the map's picture in turn.
    if (treasures_overland())
    {
        for (const auto set : sets)
        {
            if (set != 0 && set != keep_walls)
            {
                treasures_overland_picture_ = set;
            }
        }
        return reply;
    }
    vga_load_wall_slots(sets);
    return reply;
}

bool RolfTourSession::treasures_overland() const
{
    constexpr EclAddress overland_shown{0x40};
    return game() == GoldBoxGame::treasures_of_the_savage_frontier &&
           machine_.variable(overland_shown) != 0;
}

bool RolfTourSession::turn_colors(double seconds)
{
    if (!treasures_overland())
    {
        return false;
    }
    const auto picture = town_->overland_pictures.find(treasures_overland_picture_);
    if (picture == town_->overland_pictures.end() || !picture->second.cycle ||
        picture->second.cycle->delay == 0)
    {
        return false;
    }
    const double step_seconds = picture->second.cycle->delay / 100.0;
    overland_color_seconds_ += seconds;
    if (overland_color_seconds_ < step_seconds)
    {
        return false;
    }
    overland_color_seconds_ = std::fmod(overland_color_seconds_, step_seconds);
    ++overland_color_steps_;
    ++snapshot_.picture_revision;
    return true;
}

Image RolfTourSession::treasures_overland_view() const
{
    // The window of 11 by 11 squares of the map's picture around the party,
    // kept on the 16 by 16 map, with the party's arrow (its DrawOverMap), in
    // the colors of the OVER record after the cell 42h's number.
    constexpr EclAddress overland_palette{0x42};
    constexpr unsigned cells = 11, cell_pixels = 8, highest_corner = 16 - cells;
    const auto palette = machine_.variable(overland_palette) + 1U;
    const auto picture = town_->overland_pictures.find(treasures_overland_picture_);
    const auto colors = town_->overland_colors.find(palette);
    if (picture == town_->overland_pictures.end() || colors == town_->overland_colors.end())
    {
        throw EclError("Unknown overland picture " + std::to_string(treasures_overland_picture_));
    }
    const auto &cycle = picture->second.cycle;
    const auto map =
        vga_rgba(picture->second.pixels,
                 cycle ? cycled(colors->second, *cycle, overland_color_steps_) : colors->second);
    const auto pose = snapshot_.pose;
    const auto left = std::min(pose.x > 5 ? pose.x - 5 : 0U, highest_corner);
    const auto top = std::min(pose.y > 5 ? pose.y - 5 : 0U, highest_corner);
    Image view;
    view.width = view.height = static_cast<std::uint16_t>(cells * cell_pixels);
    view.rgba.resize(std::size_t{view.width} * view.height * 4);
    for (unsigned y = 0; y < view.height; ++y)
    {
        const auto from = (std::size_t{top * cell_pixels + y} * map.width + left * cell_pixels) * 4;
        std::copy_n(map.rgba.begin() + static_cast<std::ptrdiff_t>(from), view.width * 4U,
                    view.rgba.begin() +
                        static_cast<std::ptrdiff_t>(std::size_t{y} * view.width * 4));
    }
    const auto &arrow = town_->overland_arrows.at(index(pose.facing));
    const auto arrow_left = (pose.x - left) * cell_pixels, arrow_top = (pose.y - top) * cell_pixels;
    for (unsigned y = 0; y < arrow.height; ++y)
    {
        for (unsigned x = 0; x < arrow.width; ++x)
        {
            const auto from = (std::size_t{y} * arrow.width + x) * 4;
            if (arrow.rgba[from + 3] != 0)
            {
                const auto to = (std::size_t{arrow_top + y} * view.width + arrow_left + x) * 4;
                std::copy_n(arrow.rgba.begin() + static_cast<std::ptrdiff_t>(from), 4,
                            view.rgba.begin() + static_cast<std::ptrdiff_t>(to));
            }
        }
    }
    return view;
}

void RolfTourSession::cast_outside_combat(MemberId caster, MemberId patient,
                                          const std::string &action)
{
    log_player("member " + std::to_string(caster) + " casts " + action);
    if (!campaign_ || campaign_->in_combat())
    {
        throw std::runtime_error("A spell is cast outside combat");
    }
    if (magic_barred())
    {
        throw std::runtime_error("No one can cast spells in this area");
    }
    const auto selected = selected_character_.index < campaign_->state().slots.size()
                              ? campaign_->state().slots[selected_character_.index]
                              : MemberId{};
    const auto before_hp = selected ? campaign_->member(selected).vitals.hit_points : 0;
    campaign_->use_camp_action(caster, patient, action);
    // A journey/camp menu can retain a loaded character record. Keep its HP
    // current after the player's spell, so the next LOAD cannot undo healing.
    if (selected && machine_.pending_request() &&
        campaign_->member(selected).vitals.hit_points != before_hp)
    {
        const std::array updates{EclMemoryWrite{
            game_cell(ecl_character_hit_points),
            static_cast<std::uint16_t>(campaign_->member(selected).vitals.hit_points)}};
        if (!machine_.refresh_host_variables(machine_.pending_request()->id, updates))
        {
            throw EclError("Cannot refresh the selected character after casting");
        }
    }
    constexpr EclAddress last_spell{0x4B};
    constexpr std::uint16_t awaiting_spell = 0xFF;
    constexpr std::string_view cast_prefix = "cast:";
    const auto canonical = opengold::original::spell_number(
        action.starts_with(cast_prefix) ? action.substr(cast_prefix.size()) : action);
    if (game() != GoldBoxGame::treasures_of_the_savage_frontier || !canonical || !can_leave() ||
        machine_.variable(last_spell) != awaiting_spell)
    {
        return;
    }
    // The script reads the spell by the game's own number.
    for (int number = 1; number <= 0xFF; ++number)
    {
        if (opengold::original::canonical_spell(number, game()) == *canonical)
        {
            machine_.bind_variable(last_spell, static_cast<std::uint16_t>(number));
            break;
        }
    }
    pending_movement_.reset();
    begin_event(EventTrigger::step);
    advance(0);
}

void RolfTourSession::treasures_program(const EclRequest &request, unsigned number)
{
    constexpr unsigned luck_reading = 3, fix = 4;
    if (number == fix)
    {
        // The Fix waits only to tell what it did.
        auto outcome = fix_party(
            [this, &request](unsigned count)
            {
                return machine_.host_random(request.id, count);
            });
        if (outcome.interrupted_after)
        {
            outcome.words.push_back({"Your repose is suddenly interrupted!", {}});
        }
        auto reply = character_reply(selected_character_);
        for (const auto &write : clock_reply().writes)
        {
            reply.writes.push_back(write);
        }
        if (outcome.words.empty())
        {
            if (!machine_.resume_host(request.id, reply))
            {
                throw EclError("Fix reply rejected");
            }
            return;
        }
        damage_writes_ = reply.writes;
        snapshot_.host_dialogue = outcome.words;
        snapshot_.dialogue.clear();
        for (const auto &word : outcome.words)
        {
            auto line = word.source;
            if (const auto at = line.find("{name}"); at != std::string::npos)
            {
                line.replace(at, 6, word.arguments.front().value);
            }
            snapshot_.dialogue += (snapshot_.dialogue.empty() ? "" : "\n") + line;
        }
    }
    else if (number == luck_reading)
    {
        snapshot_.dialogue = next_luck_reading();
    }
    else
    {
        repair_party();
    }
    damage_request_ = request.id;
    snapshot_.phase = TourPhase::awaiting_continue;
    snapshot_.choices = {"Continue"};
    snapshot_.continue_ticket = ++next_ticket_;
    ++snapshot_.revision;
}

std::string RolfTourSession::next_luck_reading()
{
    // The crown's last reading comes once its work is done (the luck state at
    // 1, which Freezefire's fall sets), and then the state is 2; with the
    // fallback cell at FFh the reading is 12; otherwise it tells the first
    // Lucky Paper mission not yet done (cells 127h to 130h at 1 when done),
    // or 11 when all are.
    constexpr EclAddress luck_state{0x25C}, luck_fallback{0x1F8};
    constexpr std::uint16_t first_mission_cell = 0x127, missions = 10;
    unsigned record = 0;
    if (machine_.variable(luck_state) == 1)
    {
        damage_writes_.push_back({luck_state, 2});
    }
    else if (machine_.variable(luck_fallback) == 0xFF)
    {
        record = 12;
    }
    else
    {
        record = 1;
        while (record <= missions && machine_.variable(EclAddress{static_cast<std::uint16_t>(
                                         first_mission_cell + record - 1)}) == 1)
        {
            ++record;
        }
    }
    const auto found = town_->luck_texts.find(record);
    if (found == town_->luck_texts.end())
    {
        throw EclError("No luck reading " + std::to_string(record));
    }
    // Its pages, each waiting for a key in the original, in capitals.
    std::string reading;
    for (const char letter : found->second)
    {
        constexpr char page_end = 12;
        if (letter == page_end)
        {
            reading += "\n\n";
        }
        else if (letter != '\r' && letter != '\n')
        {
            reading += static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
        }
    }
    return reading;
}

void RolfTourSession::repair_party()
{
    // Every member's feeblemind, blindness, disease and poison cured, the
    // dead but elves resurrected, the stoned made flesh, every drained level
    // restored, and every Hit Point back.
    using rules::TempleService;
    for (const auto id : campaign_->state().slots)
    {
        if (!id)
        {
            continue;
        }
        for (const auto service : {TempleService::cure_feeblemind, TempleService::cure_blindness,
                                   TempleService::cure_disease, TempleService::neutralize_poison,
                                   TempleService::resurrection, TempleService::stone_to_flesh})
        {
            if (campaign_->temple_service_helps(id, service))
            {
                campaign_->temple_service(id, service, 0);
            }
        }
        while (campaign_->temple_service_helps(id, TempleService::restoration))
        {
            campaign_->temple_service(id, TempleService::restoration, 0);
        }
    }
    (void)campaign_->heal_living_members();
}

bool RolfTourSession::camp_offers_fix() const
{
    // The original's menu hides Rest and Fix only where every rest step is
    // interrupted; OpenGoldBox's camp refuses a rest where none is allowed.
    constexpr std::uint16_t every_step = 1, always = 100;
    const bool fixes = game() == GoldBoxGame::treasures_of_the_savage_frontier ||
                       game() == GoldBoxGame::secret_of_the_silver_blades;
    return fixes && campaign_ && can_leave() &&
           (machine_.variable(game_cell(ecl_rest_interruption_chance)) < always ||
            machine_.variable(game_cell(ecl_rest_check_interval)) != every_step);
}

bool RolfTourSession::camp_fix()
{
    if (!camp_offers_fix())
    {
        return false;
    }
    camp_fixing_ = true;
    if (!camp(RestKind::long_rest))
    {
        camp_fixing_ = false;
        return false;
    }
    return true;
}

void RolfTourSession::add_script_to_hit()
{
    if (game() != GoldBoxGame::pools_of_darkness || !encounter_)
    {
        return;
    }
    // Each cell holds a signed byte: 250 is 6 worse.
    const auto signed_cell = [this](EclAddress cell)
    {
        return static_cast<int>(static_cast<std::int8_t>(machine_.variable(cell) & 0xFF));
    };
    encounter_->party_to_hit += signed_cell(darkness_party_hit_mod);
    const int monsters_to_hit = signed_cell(darkness_monster_hit_mod);
    for (auto &enemy : encounter_->enemies)
    {
        enemy.original_strength.to_hit += monsters_to_hit;
    }
}

bool RolfTourSession::magic_barred() const
{
    constexpr EclAddress darkness_anti_magic{0x02};
    return game() == GoldBoxGame::pools_of_darkness && machine_.variable(darkness_anti_magic) != 0;
}

bool RolfTourSession::camp_offers_crown() const
{
    constexpr EclAddress crown{0x3F}, luck_state{0x25C};
    constexpr std::uint16_t last_reading_given = 2;
    return game() == GoldBoxGame::treasures_of_the_savage_frontier && campaign_ && can_leave() &&
           machine_.variable(crown) != 0 && machine_.variable(luck_state) != last_reading_given;
}

unsigned RolfTourSession::camp_crystals() const
{
    constexpr EclAddress crystals_held{0x3C};
    constexpr unsigned every_crystal = 7;
    return game() == GoldBoxGame::treasures_of_the_savage_frontier
               ? machine_.variable(crystals_held) & every_crystal
               : 0;
}

bool RolfTourSession::camp_crown()
{
    if (!camp_offers_crown())
    {
        return false;
    }
    log_player("asks the crown for the luck");
    snapshot_.host_dialogue.clear();
    snapshot_.dialogue = next_luck_reading();
    for (const auto &write : std::exchange(damage_writes_, {}))
    {
        machine_.bind_variable(write.address, write.value);
    }
    ++snapshot_.revision;
    return true;
}

std::optional<unsigned> RolfTourSession::fix_rest(unsigned minutes, const Dice &dice)
{
    if (minutes == 0)
    {
        return std::nullopt;
    }
    const auto interval = machine_.variable(game_cell(ecl_rest_check_interval)),
               chance = machine_.variable(game_cell(ecl_rest_interruption_chance));
    const auto interrupted = rest_interruption(interval, chance, minutes, dice);
    campaign_->advance_time(std::chrono::minutes{interrupted.value_or(minutes)});
    return interrupted;
}

RolfTourSession::FixOutcome RolfTourSession::fix_party(const Dice &dice)
{
    if (game() == GoldBoxGame::secret_of_the_silver_blades)
    {
        return secret_fix_party(dice);
    }
    using rules::TempleService;
    FixOutcome outcome;
    const auto members = campaign_->state().slots;
    const auto missing_hit_points = [&]
    {
        int missing = 0;
        for (const auto id : members)
        {
            if (id)
            {
                missing += campaign_->hit_point_maximum(id) -
                           std::max(campaign_->member(id).vitals.hit_points, 0);
            }
        }
        return missing;
    };
    // The original's Fix does nothing for a party missing no Hit Points.
    const int missing = missing_hit_points();
    if (missing == 0)
    {
        return outcome;
    }
    const auto knows = [&](MemberId id, std::string_view spell_id)
    {
        const auto number = opengold::original::spell_number(spell_id, game());
        const auto &known = campaign_->member(id).character.sheet().original->known_spells;
        return number && std::ranges::find(known, *number) != known.end();
    };
    // A member casts while it stands: alive, awake and not stone.
    const auto standing = [&](MemberId id)
    {
        if (!id || !campaign_->member(id).character.sheet().original)
        {
            return false;
        }
        const auto &vitals = campaign_->member(id).vitals;
        return !vitals.dead && vitals.hit_points > 0 &&
               !campaign_->temple_service_helps(id, TempleService::stone_to_flesh);
    };
    const auto slots = [](int count)
    {
        return static_cast<unsigned>(std::max(count, 0));
    };

    // The cures a member's slots allow; the slot arrays count from level 1.
    struct Cures
    {
        unsigned raise_dead = 0, resurrection = 0, neutralize_poison = 0, remove_curse = 0,
                 cure_blindness = 0, stone_to_flesh = 0, heal = 0, restoration = 0;
    } cures;

    // The levels a member has lost and Restoration would give back.
    const auto levels_drained = [&](MemberId id)
    {
        const auto &numbers = *campaign_->member(id).character.sheet().original;
        unsigned drained = 0;
        for (std::size_t at = 0; at < numbers.class_levels.size(); ++at)
        {
            drained += slots(numbers.peak_class_levels[at] - numbers.class_levels[at]);
        }
        return drained;
    };

    // The healing the casters can give: an average for each slot of a cure
    // they know, and later a roll for each slot of every cure level.
    int healing = 0;
    unsigned light = 0, serious = 0, critical = 0, heals = 0, longest = 0;
    for (const auto id : members)
    {
        if (!standing(id))
        {
            continue;
        }
        const auto &numbers = *campaign_->member(id).character.sheet().original;
        const auto &cleric = numbers.cleric_slots;
        const auto &mage = numbers.magic_user_slots;
        healing += (knows(id, "cleric:cure_light_wounds") ? cleric[0] * 9 : 0) +
                   (knows(id, "cleric:cure_serious_wounds") ? cleric[3] * 17 : 0) +
                   (knows(id, "cleric:cure_critical_wounds") ? cleric[4] * 27 : 0) +
                   (knows(id, "cleric:heal") ? cleric[5] * 50 : 0);
        cures.neutralize_poison += knows(id, "cleric:neutralize_poison") ? slots(cleric[3]) : 0;
        cures.raise_dead += knows(id, "cleric:raise_dead") ? slots(cleric[4]) : 0;
        cures.remove_curse += (knows(id, "cleric:remove_curse") ? slots(cleric[2]) : 0) +
                              (knows(id, "magic_user:remove_curse") ? slots(mage[3]) : 0);
        cures.stone_to_flesh += knows(id, "magic_user:stone_to_flesh") ? slots(mage[5]) : 0;
        cures.cure_blindness += knows(id, "cleric:cure_blindness") ? slots(cleric[2]) : 0;
        cures.resurrection += knows(id, "cleric:resurrection") ? slots(cleric[5]) : 0;
        cures.heal += knows(id, "cleric:heal") ? slots(cleric[5]) : 0;
        if (campaign_->member(id).character.sheet().scores[rules::Ability::wisdom] <= 3)
        {
            continue;
        }
        // The casting time: a quarter hour for each first-level slot, half an
        // hour for each second-level druid slot, an hour, an hour and a
        // quarter and an hour and a half for each fourth- to sixth-level one.
        light += slots(cleric[0]) + slots(numbers.druid_slots[1]);
        serious += slots(cleric[3]);
        critical += slots(cleric[4]);
        heals += slots(cleric[5]);
        longest = std::max(longest, slots(cleric[0]) * 15 + slots(numbers.druid_slots[1]) * 30 +
                                        slots(cleric[3]) * 60 + slots(cleric[4]) * 75 +
                                        slots(cleric[5]) * 90);
    }
    // Healing more than the party needs takes as much less time.
    if (healing > missing)
    {
        longest /= static_cast<unsigned>(healing / missing);
    }
    const auto name_of = [&](MemberId id)
    {
        return campaign_->member(id).character.sheet().name;
    };
    const auto say = [&](std::string source, MemberId id)
    {
        outcome.words.push_back({std::move(source), {{"name", name_of(id)}}});
    };
    // Each cure goes to the members it helps, in the party's order, while
    // its count lasts.
    const auto cure_party = [&]
    {
        for (const auto id : members)
        {
            if (!id)
            {
                continue;
            }
            const auto cure = [&](unsigned &count, TempleService service, std::string words)
            {
                if (count == 0 || !campaign_->temple_service_helps(id, service))
                {
                    return;
                }
                campaign_->temple_service(id, service, 0);
                --count;
                say(std::move(words), id);
            };
            // Restoration gives back a level for each slot; a Heal cures
            // feeblemind, and blindness with it, leaving Cure Blindness's
            // slot for another.
            while (cures.restoration > 0 &&
                   campaign_->temple_service_helps(id, TempleService::restoration))
            {
                campaign_->temple_service(id, TempleService::restoration, 0);
                --cures.restoration;
            }
            const bool feebleminded = cures.heal > 0 && campaign_->temple_service_helps(
                                                            id, TempleService::cure_feeblemind);
            cure(cures.heal, TempleService::cure_feeblemind, "{name} is healed");
            cure(cures.stone_to_flesh, TempleService::stone_to_flesh,
                 "{name} is changed back to flesh");
            cure(cures.neutralize_poison, TempleService::neutralize_poison, "{name} is detoxified");
            if (cures.resurrection > 0)
            {
                cure(cures.resurrection, TempleService::resurrection,
                     "{name} has been resurrected");
            }
            else
            {
                cure(cures.raise_dead, TempleService::raise_dead,
                     "{name} has been raised from the dead");
            }
            cure(cures.remove_curse, TempleService::remove_curse, "{name} is relieved of a curse");
            if (!feebleminded)
            {
                cure(cures.cure_blindness, TempleService::cure_blindness, "{name} regains sight");
            }
        }
    };
    outcome.interrupted_after = fix_rest(longest, dice);
    if (outcome.interrupted_after)
    {
        return outcome;
    }
    const auto roll = [&dice](unsigned count)
    {
        int total = 0;
        for (unsigned die = 0; die < count; ++die)
        {
            total += static_cast<int>(dice(8)) + 1;
        }
        return total;
    };
    for (unsigned slot = 0; slot < light; ++slot)
    {
        healing += roll(1);
    }
    for (unsigned slot = 0; slot < serious; ++slot)
    {
        healing += roll(2) + 1;
    }
    for (unsigned slot = 0; slot < critical; ++slot)
    {
        healing += roll(3) + 3;
    }
    for (unsigned slot = 0; slot < heals; ++slot)
    {
        healing += roll(4) + 3;
    }
    cure_party();
    for (const auto id : members)
    {
        if (id)
        {
            healing -= campaign_->restore_hit_points(id, healing);
        }
    }
    // A second rest for the cures the first left undone, as many of each as
    // are needed and the slots allow.
    unsigned dead = 0, stoned = 0, poisoned = 0, blind = 0, cursed = 0, feebleminded = 0,
             drained = 0;
    for (const auto id : members)
    {
        if (!id)
        {
            continue;
        }
        drained += levels_drained(id);
        feebleminded += campaign_->temple_service_helps(id, TempleService::cure_feeblemind) ? 1 : 0;
        dead += campaign_->temple_service_helps(id, TempleService::raise_dead) ? 1 : 0;
        stoned += campaign_->temple_service_helps(id, TempleService::stone_to_flesh) ? 1 : 0;
        poisoned += campaign_->temple_service_helps(id, TempleService::neutralize_poison) ? 1 : 0;
        blind += campaign_->temple_service_helps(id, TempleService::cure_blindness) ? 1 : 0;
        cursed += campaign_->temple_service_helps(id, TempleService::remove_curse) ? 1 : 0;
    }
    for (const auto id : members)
    {
        if (!standing(id))
        {
            continue;
        }
        const auto &numbers = *campaign_->member(id).character.sheet().original;
        cures.restoration += slots(numbers.cleric_slots[6]);
        cures.resurrection += slots(numbers.cleric_slots[6]);
        cures.heal += slots(numbers.cleric_slots[5]);
        cures.stone_to_flesh += slots(numbers.magic_user_slots[5]);
        cures.raise_dead += slots(numbers.cleric_slots[4]);
        cures.neutralize_poison += slots(numbers.cleric_slots[3]);
        cures.remove_curse += slots(numbers.magic_user_slots[3]) + slots(numbers.cleric_slots[2]);
        cures.cure_blindness += slots(numbers.cleric_slots[2]);
    }
    const unsigned raising = cures.resurrection > 0 ? std::min(cures.resurrection, dead) * 105
                                                    : std::min(cures.raise_dead, dead) * 75;
    const unsigned curing =
        raising + std::min(cures.restoration, drained) * 105 +
        std::min(cures.heal, feebleminded) * 90 + std::min(cures.stone_to_flesh, stoned) * 90 +
        std::min(cures.neutralize_poison, poisoned) * 60 +
        std::min(cures.cure_blindness, blind) * 45 + std::min(cures.remove_curse, cursed) * 45;
    outcome.interrupted_after = fix_rest(curing, dice);
    if (!outcome.interrupted_after)
    {
        cure_party();
    }
    return outcome;
}

RolfTourSession::FixOutcome RolfTourSession::secret_fix_party(const Dice &dice)
{
    using rules::TempleService;
    FixOutcome outcome;
    const auto members = campaign_->state().slots;
    int missing = 0;
    for (const auto id : members)
    {
        if (id)
        {
            missing += campaign_->hit_point_maximum(id) -
                       std::max(campaign_->member(id).vitals.hit_points, 0);
        }
    }
    // The original's Fix does nothing for a party missing no Hit Points.
    if (missing == 0)
    {
        return outcome;
    }
    // A member casts while its status is okay: alive, awake and not stone.
    const auto standing = [&](MemberId id)
    {
        if (!id || !campaign_->member(id).character.sheet().original)
        {
            return false;
        }
        const auto &vitals = campaign_->member(id).vitals;
        return !vitals.dead && vitals.hit_points > 0 &&
               !campaign_->temple_service_helps(id, TempleService::stone_to_flesh);
    };
    const auto slots = [](int count)
    {
        return static_cast<unsigned>(std::max(count, 0));
    };
    const auto roll = [&dice](unsigned count, int plus)
    {
        int total = plus;
        for (unsigned die = 0; die < count; ++die)
        {
            total += static_cast<int>(dice(8)) + 1;
        }
        return total;
    };

    // The cures the Fix can apply, from the spells memorized and, for its
    // second rest, the slots.
    struct Cures
    {
        unsigned heal = 0, stone_to_flesh = 0, neutralize_poison = 0, raise_dead = 0,
                 remove_curse = 0, cure_disease = 0, cure_blindness = 0;
    } cures;

    // Each cure memorized by a standing member is rolled into the healing
    // (RollMemorizedCures), and each status cure counted.
    int healing = 0;
    for (const auto id : members)
    {
        if (!standing(id))
        {
            continue;
        }
        for (const auto &spell : campaign_->memorized_spells(id))
        {
            if (spell == "cleric:cure_light_wounds" || spell == "druid:cure_light_wounds")
            {
                healing += roll(1, 0);
            }
            else if (spell == "cleric:cure_serious_wounds")
            {
                healing += roll(2, 1);
            }
            else if (spell == "cleric:cure_critical_wounds")
            {
                healing += roll(3, 3);
            }
            else if (spell == "cleric:heal")
            {
                healing += roll(4, 5);
                ++cures.heal;
            }
            cures.neutralize_poison += spell == "cleric:neutralize_poison" ? 1 : 0;
            cures.raise_dead += spell == "cleric:raise_dead" ? 1 : 0;
            cures.remove_curse +=
                spell == "cleric:remove_curse" || spell == "magic_user:remove_curse" ? 1 : 0;
            cures.stone_to_flesh += spell == "magic_user:stone_to_flesh" ? 1 : 0;
            cures.cure_blindness += spell == "cleric:cure_blindness" ? 1 : 0;
            cures.cure_disease += spell == "cleric:cure_disease" ? 1 : 0;
        }
    }

    // The rest's length (ComputeFixTime): for each standing member of
    // Wisdom above 3, four hours with first-level cures (or second-level
    // druid ones), six with fourth- or fifth-level, eight with sixth-level,
    // and a quarter hour, half an hour, an hour, an hour and a quarter and
    // an hour and a half for each slot of them; the longest member's. A
    // party missing fewer Hit Points than the members' minutes rests as
    // many times less.
    unsigned light = 0, serious = 0, critical = 0, heals = 0, longest = 0, minutes = 0;
    for (const auto id : members)
    {
        if (!standing(id) ||
            campaign_->member(id).character.sheet().scores[rules::Ability::wisdom] <= 3)
        {
            continue;
        }
        const auto &numbers = *campaign_->member(id).character.sheet().original;
        const auto &cleric = numbers.cleric_slots;
        const unsigned first = slots(cleric[0]), druid = slots(numbers.druid_slots[1]),
                       fourth = slots(cleric[3]), fifth = slots(cleric[4]),
                       sixth = slots(cleric[5]);
        light += first + druid;
        serious += fourth;
        critical += fifth;
        heals += sixth;
        unsigned base = 0;
        if (first > 0)
        {
            base = 240;
            minutes += 27;
        }
        if (druid > 0)
        {
            base = 240;
            minutes += 27;
        }
        if (fourth + fifth > 0)
        {
            base = 360;
            minutes += fifth > 0 ? 78 : 34;
        }
        if (sixth > 0)
        {
            base = 480;
            minutes += 90;
        }
        base += first * 15 + druid * 30 + fourth * 60 + fifth * 75 + sixth * 90;
        longest = std::max(longest, base);
    }
    if (missing < static_cast<int>(minutes))
    {
        longest /= minutes / static_cast<unsigned>(missing);
    }

    const auto name_of = [&](MemberId id)
    {
        return campaign_->member(id).character.sheet().name;
    };
    const auto say = [&](std::string source, MemberId id)
    {
        outcome.words.push_back({std::move(source), {{"name", name_of(id)}}});
    };
    // The cures counted go to the members they help, in the party's order
    // (CureStatusEffects): a Heal to the feebleminded, with their blindness
    // and disease; then Stone to Flesh, Neutralize Poison, Raise Dead and
    // Remove Curse; and to a member no Heal cured, Cure Disease or else Cure
    // Blindness.
    const auto cure_party = [&]
    {
        for (const auto id : members)
        {
            if (!id)
            {
                continue;
            }
            const auto cure = [&](unsigned &count, TempleService service, std::string words)
            {
                if (count == 0 || !campaign_->temple_service_helps(id, service))
                {
                    return false;
                }
                campaign_->temple_service(id, service, 0);
                --count;
                say(std::move(words), id);
                return true;
            };
            const bool healed =
                cure(cures.heal, TempleService::cure_feeblemind, "{name} is healed");
            if (healed && campaign_->temple_service_helps(id, TempleService::cure_disease))
            {
                campaign_->temple_service(id, TempleService::cure_disease, 0);
            }
            cure(cures.stone_to_flesh, TempleService::stone_to_flesh,
                 "{name} is changed back to flesh");
            cure(cures.neutralize_poison, TempleService::neutralize_poison, "{name} is detoxified");
            cure(cures.raise_dead, TempleService::raise_dead,
                 "{name} has been raised from the dead");
            cure(cures.remove_curse, TempleService::remove_curse, "{name} is relieved of a curse");
            if (!healed && !cure(cures.cure_disease, TempleService::cure_disease,
                                 "{name} is cured of disease"))
            {
                cure(cures.cure_blindness, TempleService::cure_blindness, "{name} regains sight");
            }
        }
    };
    outcome.interrupted_after = fix_rest(longest, dice);
    if (outcome.interrupted_after)
    {
        return outcome;
    }
    // One roll for each cure slot (RollCureSlots), a Heal's 4d8+3.
    for (unsigned slot = 0; slot < light; ++slot)
    {
        healing += roll(1, 0);
    }
    for (unsigned slot = 0; slot < serious; ++slot)
    {
        healing += roll(2, 1);
    }
    for (unsigned slot = 0; slot < critical; ++slot)
    {
        healing += roll(3, 3);
    }
    for (unsigned slot = 0; slot < heals; ++slot)
    {
        healing += roll(4, 3);
    }
    cure_party();
    // The healing is spent on the living in the party's order, waking the
    // unconscious (HealParty).
    for (const auto id : members)
    {
        if (id && !campaign_->temple_service_helps(id, TempleService::stone_to_flesh))
        {
            healing -= campaign_->restore_hit_points(id, healing);
        }
    }
    // A second rest for the cures left to do, the standing members' slots
    // added to the counts (ComputeCureTime): an hour and a half for each
    // Heal on the feebleminded and each Stone to Flesh, an hour and a
    // quarter for each Raise Dead, an hour for each Neutralize Poison and
    // three quarters for each Cure Disease, Cure Blindness and Remove Curse.
    unsigned dead = 0, stoned = 0, poisoned = 0, feebleminded = 0, blind = 0, diseased = 0,
             cursed = 0;
    for (const auto id : members)
    {
        if (!id)
        {
            continue;
        }
        dead += campaign_->temple_service_helps(id, TempleService::raise_dead) ? 1 : 0;
        stoned += campaign_->temple_service_helps(id, TempleService::stone_to_flesh) ? 1 : 0;
        cursed += campaign_->temple_service_helps(id, TempleService::remove_curse) ? 1 : 0;
        poisoned += campaign_->temple_service_helps(id, TempleService::neutralize_poison) ? 1 : 0;
        if (campaign_->temple_service_helps(id, TempleService::cure_feeblemind))
        {
            ++feebleminded;
        }
        else
        {
            blind += campaign_->temple_service_helps(id, TempleService::cure_blindness) ? 1 : 0;
            diseased += campaign_->temple_service_helps(id, TempleService::cure_disease) ? 1 : 0;
        }
    }
    for (const auto id : members)
    {
        if (!standing(id))
        {
            continue;
        }
        const auto &numbers = *campaign_->member(id).character.sheet().original;
        cures.heal += slots(numbers.cleric_slots[5]);
        cures.stone_to_flesh += slots(numbers.magic_user_slots[5]);
        cures.raise_dead += slots(numbers.cleric_slots[4]);
        cures.neutralize_poison += slots(numbers.cleric_slots[3]);
        cures.remove_curse += slots(numbers.magic_user_slots[3]) + slots(numbers.cleric_slots[2]);
        cures.cure_blindness += slots(numbers.cleric_slots[2]);
        cures.cure_disease += slots(numbers.cleric_slots[2]);
    }
    const unsigned curing =
        std::min(cures.heal, feebleminded) * 90 + std::min(cures.stone_to_flesh, stoned) * 90 +
        std::min(cures.raise_dead, dead) * 75 + std::min(cures.neutralize_poison, poisoned) * 60 +
        std::min(cures.cure_disease, diseased) * 45 + std::min(cures.cure_blindness, blind) * 45 +
        std::min(cures.remove_curse, cursed) * 45;
    outcome.interrupted_after = fix_rest(curing, dice);
    if (!outcome.interrupted_after)
    {
        cure_party();
    }
    return outcome;
}

void RolfTourSession::vga_play_sound(unsigned sound)
{
    // From 80h a piece of the game's music starts, played again from its
    // start if it is the one playing (eclvm.pas, EclPlaySound; music.pas,
    // StartMusic).
    if (sound >= first_music_sound)
    {
        snapshot_.music = sound - first_music_sound;
        ++snapshot_.music_starts;
        return;
    }
    if (sound < first_sound_effect || sound > last_sound_effect)
    {
        return;
    }
    // The footstep plays as the party's step does; the driver plays the other
    // effects up to 12 (sounddrv.pas, SoundCommand), and 13 and 14 only stop
    // the music.
    if (sound == footstep_sound)
    {
        ++snapshot_.footsteps;
    }
    else if (sound <= last_played_sound_effect)
    {
        snapshot_.script_sound = sound;
        ++snapshot_.script_sounds;
    }
    sound_effect_played();
}

void RolfTourSession::sound_effect_played()
{
    snapshot_.music.reset();
}

namespace
{
// Treasures' romance cells (its gameState): the NPC's number, which a script
// sets as the NPC joins (ECL 17), the two loves, the bands they were in at
// the last camp, and the two names.
constexpr EclAddress romance_npc_number{0x105}, member_love_cell{0x106}, npc_love_cell{0x107},
    npc_band_cell{0x4C}, member_band_cell{0x4D}, member_name_cell{0xE5}, npc_name_cell{0xF5};
// The picture the proposal is made over (PIC 131).
constexpr unsigned proposal_picture = 131;

std::string capitals(std::string text)
{
    std::ranges::transform(text, text.begin(),
                           [](unsigned char c)
                           {
                               return static_cast<char>(std::toupper(c));
                           });
    return text;
}

// A love's band of 25, 0 to 4 (romance.pas, RelationBand).
unsigned love_band(unsigned love)
{
    return std::min(love / 25, 4U);
}

// The scene for the member's and the NPC's bands, 0 for none (0 and 0) and
// for the proposal (4 and 4), which the caller tells apart. The original's
// table told 4 and 3 with 3 and 3's scene (112) where every other pair is
// told alike either way round (113; an obvious bug, fixed).
unsigned romance_entry(unsigned member_band, unsigned npc_band)
{
    constexpr std::array<std::array<unsigned, 5>, 5> entries{{{0, 101, 102, 103, 104},
                                                              {101, 105, 106, 107, 108},
                                                              {102, 106, 109, 110, 111},
                                                              {103, 107, 110, 112, 113},
                                                              {104, 108, 111, 113, 0}}};
    return entries.at(member_band).at(npc_band);
}
} // namespace

std::optional<rules::Romance> RolfTourSession::treasures_romance() const
{
    if (game() != GoldBoxGame::treasures_of_the_savage_frontier || !town_ ||
        machine_.variable(romance_npc_number) == 0)
    {
        return std::nullopt;
    }
    return rules::Romance{machine_.string(member_name_cell), machine_.string(npc_name_cell),
                          static_cast<int>(machine_.variable(member_love_cell) & 0xFF),
                          static_cast<int>(machine_.variable(npc_love_cell) & 0xFF)};
}

bool RolfTourSession::tell_romance()
{
    const auto romance = treasures_romance();
    if (!romance || !campaign_)
    {
        return false;
    }
    // The members of the pair's names, the last of each (TagRelationshipPair).
    MemberId member{}, npc{};
    for (const auto id : campaign_->state().slots)
    {
        if (!id)
        {
            continue;
        }
        const auto name = capitals(campaign_->member(id).character.sheet().name);
        if (name == capitals(romance->npc))
        {
            npc = id;
        }
        if (name == capitals(romance->member))
        {
            member = id;
        }
    }
    const auto standing = [this](MemberId id)
    {
        const auto &vitals = campaign_->member(id).vitals;
        return !vitals.dead && vitals.hit_points > 0;
    };
    if (!member || !npc || !standing(member) || !standing(npc))
    {
        return false;
    }
    const auto member_band = love_band(static_cast<unsigned>(romance->member_love));
    const auto npc_band = love_band(static_cast<unsigned>(romance->npc_love));
    if (member_band == machine_.variable(member_band_cell) &&
        npc_band == machine_.variable(npc_band_cell))
    {
        return false;
    }
    machine_.bind_variable(member_band_cell, static_cast<std::uint16_t>(member_band));
    machine_.bind_variable(npc_band_cell, static_cast<std::uint16_t>(npc_band));
    using Kind = RomanceStep::Kind;
    RomanceScene scene{{}, member, npc};
    constexpr unsigned top = 4, confession = 104, proposal = 114;
    if (member_band == top && npc_band == top)
    {
        scene.steps.push_back({Kind::proposal, proposal});
    }
    else if (const auto entry = romance_entry(member_band, npc_band))
    {
        scene.steps.push_back({Kind::text, entry});
        // One who loves completely and is not loved leaves the party.
        if (entry == confession)
        {
            scene.steps.push_back({Kind::npc_leaves, 0});
        }
    }
    if (scene.steps.empty())
    {
        return false;
    }
    romance_scene_ = std::move(scene);
    show_romance_step();
    return true;
}

void RolfTourSession::show_romance_step()
{
    using Kind = RomanceStep::Kind;
    auto &scene = *romance_scene_;
    while (!scene.steps.empty())
    {
        const auto step = scene.steps.front();
        if (step.kind == Kind::npc_leaves)
        {
            scene.steps.erase(scene.steps.begin());
            campaign_->remove(scene.npc);
            continue;
        }
        if (step.kind == Kind::refused)
        {
            scene.steps.erase(scene.steps.begin());
            campaign_->keep_lasting_change(scene.member, original::romance_refused());
            continue;
        }
        snapshot_.host_dialogue.clear();
        snapshot_.host_choices.clear();
        snapshot_.dialogue = romance_text(step.entry);
        if (step.kind == Kind::proposal)
        {
            // The pair stands before the party over its picture.
            if (const auto *frames = pic_frames(proposal_picture); frames && !frames->empty())
            {
                picture_ = frames->front().image;
                picture_animation_id_ = proposal_picture;
                ++snapshot_.picture_revision;
            }
            snapshot_.choices = {"Accept them", "Refuse them"};
            snapshot_.host_choices = {{"Accept them", {}}, {"Refuse them", {}}};
        }
        else
        {
            snapshot_.choices = {"Continue"};
        }
        snapshot_.phase = TourPhase::awaiting_continue;
        snapshot_.continue_ticket = ++next_ticket_;
        ++snapshot_.revision;
        return;
    }
    // The scene is over: the camp goes on.
    romance_scene_.reset();
    if (picture_animation_id_ == proposal_picture)
    {
        picture_.reset();
        picture_animation_id_.reset();
        ++snapshot_.picture_revision;
    }
    snapshot_.dialogue.clear();
    snapshot_.choices.clear();
    snapshot_.host_choices.clear();
    snapshot_.continue_ticket = 0;
    snapshot_.phase = TourPhase::running;
    romance_told_ = camp_resumes_ = true;
    ++snapshot_.revision;
}

bool RolfTourSession::choose_in_romance(std::size_t choice)
{
    using Kind = RomanceStep::Kind;
    auto &steps = romance_scene_->steps;
    const auto step = steps.front();
    steps.erase(steps.begin());
    if (step.kind == Kind::proposal)
    {
        constexpr unsigned accepted = 115, parting = 116, anger = 117;
        // Accepted, the couple embraces the party; refused, the NPC leaves
        // and the member never forgives the party, losing 6 Charisma.
        const std::vector<RomanceStep> follows =
            choice == 0 ? std::vector<RomanceStep>{{Kind::text, accepted}}
                        : std::vector<RomanceStep>{{Kind::text, parting},
                                                   {Kind::npc_leaves, 0},
                                                   {Kind::text, anger},
                                                   {Kind::refused, 0}};
        steps.insert(steps.begin(), follows.begin(), follows.end());
    }
    show_romance_step();
    return true;
}

std::string RolfTourSession::romance_text(unsigned entry)
{
    const auto found = town_->luck_texts.find(entry);
    if (found == town_->luck_texts.end())
    {
        throw EclError("No romance scene " + std::to_string(entry));
    }
    const auto romance = *treasures_romance();
    const auto &scene = *romance_scene_;
    const auto member_name = capitals(romance.member), npc_name = capitals(romance.npc);
    // The one who loves more, and the other (romance.pas, StrongerName).
    const bool member_stronger = romance.member_love > romance.npc_love;
    const auto &slots = campaign_->state().slots;
    const auto in_party = [&slots](MemberId id)
    {
        return id && std::ranges::find(slots, id) != slots.end();
    };
    const auto pronoun = [&](MemberId id)
    {
        return in_party(id) && campaign_->member(id).character.creation_data().gender == "female"
                   ? std::string("HER")
                   : std::string("HIS");
    };
    // A third member: the first standing one neither of the pair nor named
    // last time; else the one named last, if it stands; else the leader
    // (romance.pas, OtherPartyName).
    const auto bystander = [&]() -> std::string
    {
        const auto stands = [&](MemberId id)
        {
            return in_party(id) && !campaign_->member(id).vitals.dead &&
                   campaign_->member(id).vitals.hit_points > 0;
        };
        for (const auto id : slots)
        {
            if (stands(id) && id != scene.member && id != scene.npc && id != romance_bystander_)
            {
                romance_bystander_ = id;
                return capitals(campaign_->member(id).character.sheet().name);
            }
        }
        if (stands(romance_bystander_) && romance_bystander_ != scene.member)
        {
            return capitals(campaign_->member(romance_bystander_).character.sheet().name);
        }
        const auto leader = std::ranges::find_if(slots,
                                                 [](MemberId id)
                                                 {
                                                     return id != MemberId{};
                                                 });
        return leader == slots.end() ? std::string()
                                     : capitals(campaign_->member(*leader).character.sheet().name);
    };
    std::string text;
    for (const char code : found->second)
    {
        switch (code)
        {
        case 2:
            text += member_stronger ? member_name : npc_name;
            break;
        case 3:
            text += member_stronger ? npc_name : member_name;
            break;
        case 4:
            text += npc_name;
            break;
        case 5:
            text += member_name;
            break;
        case 6:
            text += bystander();
            break;
        case 7:
            text += pronoun(member_stronger ? scene.member : scene.npc);
            break;
        case 8:
            text += pronoun(member_stronger ? scene.npc : scene.member);
            break;
        case 12:
            // The original waits for a key and clears its window.
            text += "\n\n";
            break;
        case 13:
            text += ' ';
            break;
        case 10:
        case 26:
            break;
        default:
            text += static_cast<char>(std::toupper(static_cast<unsigned char>(code)));
        }
    }
    // Its lines run on as the window wraps them, a page's spaces at its ends
    // and between words single.
    std::string tidy;
    for (const char c : text)
    {
        if (c == ' ' && (tidy.empty() || tidy.back() == ' ' || tidy.back() == '\n'))
        {
            continue;
        }
        if (c == '\n' && !tidy.empty() && tidy.back() == ' ')
        {
            tidy.pop_back();
        }
        tidy += c;
    }
    while (!tidy.empty() && (tidy.back() == ' ' || tidy.back() == '\n'))
    {
        tidy.pop_back();
    }
    return tidy;
}

} // namespace opengold::por
