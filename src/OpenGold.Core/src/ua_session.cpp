#include "opengold/ua_session.h"

#include "archive_io.h"
#include "opengold/original_spells.h"
#include "opengold/path_text.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace opengold::ua
{
namespace
{

using por::MapDirection;
using por::TourPhase;

constexpr std::size_t event_count = 100, event_size = 20;
// The area header's entry points: eight of y, x, facing and a spare byte.
constexpr std::size_t header_spots = 0xE;
// A fight's monsters take ids from here, as the DAX games' do.
constexpr rules::EntityId first_monster_id = 1000;

[[nodiscard]] por::MapDirection kit_facing(unsigned facing)
{
    return por::MapDirection{(facing & 7) / 2 % 4};
}

[[nodiscard]] std::uint16_t word_at(std::span<const std::uint8_t> bytes, std::size_t at)
{
    return static_cast<std::uint16_t>(bytes[at] | bytes[at + 1] << 8);
}

// A file of the design's folder, or else of the kit's first disk.
[[nodiscard]] std::vector<std::uint8_t> read_item_file(const DesignFiles &files,
                                                       const std::string &name)
{
    for (const auto &folder : {files.design(), files.kit() / "DISK1"})
    {
        const auto path = folder / name;
        if (!folder.empty() && std::filesystem::is_regular_file(path))
        {
            return por::detail::read_archive_bytes<std::runtime_error>(
                path, name + " is too large", "Cannot open " + name, "Cannot read " + name);
        }
    }
    throw std::runtime_error("The kit's " + name + " is missing");
}

[[nodiscard]] std::vector<por::ItemTemplate> read_item_types(const DesignFiles &files)
{
    auto types = por::decode_item_templates(read_item_file(files, "ITEMS.DAT"),
                                            GoldBoxGame::unlimited_adventures);
    if (!types)
    {
        throw std::runtime_error("ITEMS.DAT does not decode");
    }
    return std::move(*types);
}

[[nodiscard]] Monster read_monster(const DesignFiles &files, unsigned number)
{
    const auto bytes = files.data("MONST", number);
    if (!bytes)
    {
        throw std::runtime_error("The design has no monster " + std::to_string(number));
    }
    auto monster = decode_monster(*bytes);
    if (!monster)
    {
        throw std::runtime_error("Monster " + std::to_string(number) + " does not decode");
    }
    return std::move(*monster);
}

// A character record's levels (one by class, the kit's class order: cleric,
// knight, fighter, paladin, ranger, magic-user, thief) by the original rules'
// class slots (ClassSlot: cleric, druid, fighter, paladin, ranger,
// magic-user, thief, monk).
[[nodiscard]] std::array<int, 8> class_slots(const Monster &monster)
{
    std::array<int, 8> slots{};
    const auto &levels = monster.levels;
    slots[0] = levels[0];
    for (std::size_t n = 2; n < 7; ++n)
    {
        slots[n] = levels[n];
    }
    // The kit's knight (Krynn's) fights as a fighter.
    slots[2] = std::max<int>(slots[2], levels[1]);
    return slots;
}

} // namespace

std::string monster_key(unsigned number)
{
    return "UA:MONST:" + std::to_string(number);
}

original::Combatant monster_combatant(const Monster &monster)
{
    const auto &raw = monster.raw;
    original::Combatant fighter;
    // The kit keeps THAC0 and Armor Class as 60 less the value.
    fighter.thac0 = 60 - raw[0x7F];
    fighter.armor_class = 60 - raw[0xB3];
    fighter.rear_armor_class = fighter.armor_class;
    fighter.max_hit_points = std::max<int>(raw[0x81], 1);
    fighter.hit_points = fighter.max_hit_points;
    fighter.movement = raw[0x88];
    for (std::size_t hand = 0; hand < 2; ++hand)
    {
        fighter.attacks[hand] = {raw[0xAB + hand], raw[0xAD + hand], raw[0xAF + hand],
                                 static_cast<std::int8_t>(raw[0xB1 + hand])};
    }
    for (std::size_t save = 0; save < 5; ++save)
    {
        fighter.saving_throws[save] = raw[0x83 + save];
    }
    fighter.class_levels = class_slots(monster);
    fighter.level =
        std::max(raw[0x89], *std::max_element(monster.levels.begin(), monster.levels.end()));
    fighter.attack_level = std::max(fighter.level, 1);
    fighter.morale = raw[0x93];
    fighter.intelligence = raw[0x72];
    fighter.strength = raw[0x70];
    fighter.constitution = raw[0x78];
    fighter.alignment = raw[0x5D];
    fighter.undead_level = raw[0x5A];
    fighter.magic_resistance = raw[0xC4];
    // Size: the low three bits of the icon's dimensions; above 1 is large.
    fighter.large = (raw[0x82] & 7) > 1;
    // The kit's monsters are race 6; the original rules know them as 0.
    fighter.race = raw[0x58] == 6 ? 0 : 7;
    return fighter;
}

original::Content load_design_content(const DesignFiles &files)
{
    original::Content content;
    content.game = GoldBoxGame::unlimited_adventures;
    if (const auto monsters = files.library("MONST.GLB"))
    {
        const auto index = decode_index(*monsters);
        for (const auto &entry : index ? *index : std::vector<IndexEntry>{})
        {
            content.bestiary.emplace(monster_key(entry.number),
                                     monster_combatant(read_monster(files, entry.number)));
        }
    }
    content.item_types = read_item_types(files);
    return content;
}

DesignItems load_design_items(const DesignFiles &files)
{
    auto items = decode_items(read_item_file(files, "ITEM.DAT"));
    if (!items)
    {
        throw std::runtime_error("ITEM.DAT does not decode");
    }
    return {std::move(*items), read_item_types(files)};
}

std::optional<por::Equipment> design_item(const DesignItems &items, unsigned number,
                                          bool identified)
{
    if (number == 0 || number > items.items.size())
    {
        return std::nullopt;
    }
    const auto &record = items.items[number - 1];
    // The kit marks an unused record with type FFh.
    if (record.type >= items.types.size())
    {
        return std::nullopt;
    }
    por::Equipment item;
    auto &stored = item.stored;
    stored.type = record.type;
    stored.name_components = record.name_words;
    stored.magic_bonus = record.magic_bonus;
    stored.save_bonus = record.save_bonus;
    stored.cursed_raw = record.cursed;
    stored.hidden_name_words = identified ? 0 : record.hidden_name_words & 7;
    stored.weight = record.weight;
    stored.value = record.value;
    stored.stack_size = record.count;
    stored.effect_codes = record.effects;
    item.base = items.types[record.type];
    item.bonuses = por::equipment_bonuses(stored, item.base);
    return item;
}

original::StartingStock starting_stock(const GameSettings &game)
{
    return {static_cast<int>(std::min<std::uint32_t>(game.start_experience, 100000000)),
            static_cast<int>(std::min<std::uint32_t>(game.start_platinum, 1000000))};
}

std::vector<unsigned> starting_items(unsigned set, const rules::CharacterSheet &sheet)
{
    // ITEM.DAT numbers by class group and set 1 to 8 (create.c, gear_start).
    using Sets = std::array<std::vector<unsigned>, 8>;
    static const Sets fighters{{{0x17, 0x29},
                                {0x1B, 0x2A, 0x2B},
                                {0x1C, 0x24, 0x2B, 0x06, 0x01},
                                {0x1E, 0x28, 0x05, 0x01},
                                {0x42, 0x4B, 0x35, 0x32, 0x4E, 0x49},
                                {0x60, 0x69, 0x6C, 0x50, 0x53, 0x67},
                                {0x7E, 0x87, 0x6E, 0x71, 0x8A, 0x85},
                                {0x9C, 0xA5, 0xA8, 0xA3, 0x8C, 0x8F}}};
    static const Sets clerics{{{0x08, 0x26},
                               {0x10, 0x29, 0x2B},
                               {0x10, 0x25, 0x2B, 0x0E},
                               {0x0C, 0x19, 0x28, 0x2B},
                               {0x3C, 0x47, 0x4E, 0x41},
                               {0x5A, 0x65, 0x58, 0x6C, 0x67},
                               {0x78, 0x83, 0x7D, 0x8A, 0x85},
                               {0x93, 0xA1, 0xA8, 0x9B, 0xA3}}};
    static const Sets thieves{{{0x08},
                               {0x1D, 0x27},
                               {0x1B, 0x27, 0x16},
                               {0x1C, 0x04, 0x01, 0x2C},
                               {0x42, 0x4F, 0x34, 0x32},
                               {0x60, 0x5D, 0x6D},
                               {0x7E, 0x7B, 0x8B},
                               {0x9C, 0x99, 0xD4, 0xA0}}};
    static const Sets magic_users{{{0x0B},
                                   {0x0B, 0x0A},
                                   {0x0B, 0x18, 0x23},
                                   {0x0B, 0x18, 0x22, 0x23},
                                   {0x37, 0x38, 0xD2, 0x46},
                                   {0x5E, 0x56, 0xD3, 0x64},
                                   {0x73, 0x74, 0xD4, 0x82},
                                   {0x9A, 0x2F, 0x2F, 0xD4, 0xA0}}};
    if (set == 0 || set > 8 || !sheet.original)
    {
        return {};
    }
    const auto &levels = sheet.original->class_levels;
    // The original rules' class slots: cleric, druid, fighter, paladin,
    // ranger, magic-user, thief, monk.
    const bool fighter = levels[2] > 0 || levels[3] > 0 || levels[4] > 0;
    const auto &group = fighter         ? fighters
                        : levels[0] > 0 ? clerics
                        : levels[6] > 0 ? thieves
                                        : magic_users;
    return group[set - 1];
}

void outfit_new_character(CampaignParty &party, MemberId id, const DesignFiles &files)
{
    const auto record = files.data("GAME", 1);
    const auto game = record ? decode_game_settings(*record) : std::nullopt;
    if (!game)
    {
        throw std::runtime_error("The design's game record does not decode");
    }
    party.add_coins(Coin::gems, game->start_gems);
    party.add_coins(Coin::jewelry, game->start_jewelry);
    const auto items = load_design_items(files);
    const auto &sheet = party.member(id).character.sheet();
    for (const auto number : starting_items(game->start_equipment, sheet))
    {
        auto item = design_item(items, number, true);
        if (!item)
        {
            continue;
        }
        // Arrows, bolts and darts come forty to a bundle (create.c, gear_add).
        if (number == 0x50 || number == 0x56 || number == 0x5B)
        {
            item->stored.stack_size = 40;
        }
        party.take_item(id, *item);
        const auto taken = party.member(id).character.inventory().items().back().id;
        (void)party.auto_equip(id, taken, UnidentifiedMagic::equip);
    }
}

UaSession UaSession::start(DesignFiles files)
{
    const auto record = files.data("GAME", 1);
    if (!record)
    {
        throw std::runtime_error("The design has no game record");
    }
    auto game = decode_game_settings(*record);
    if (!game)
    {
        throw std::runtime_error("The design's game record does not decode");
    }
    return UaSession(std::move(files), std::move(*game));
}

UaSession::UaSession(DesignFiles files, GameSettings game)
    : files_(std::move(files)), game_(std::move(game)), items_(load_design_items(files_))
{
    load_area(game_.start_area);
    // The kit's entry points are 1-based in the game record.
    const auto entry = std::clamp<unsigned>(game_.start_entry, 1, 8) - 1;
    const auto &header = area_.area.header;
    const std::size_t at = header_spots + 4 * entry;
    move_party(std::min<unsigned>(header[at + 1], area_.area.columns - 1U),
               std::min<unsigned>(header[at], area_.area.rows - 1U), kit_facing(header[at + 2]));
    // A design has no guided opening, as Pool of Radiance's Rolf gives.
    snapshot_.tour_finished = true;
    snapshot_.phase = TourPhase::completed;
    refresh_snapshot();
}

void UaSession::restart()
{
    auto files = files_;
    *this = start(std::move(files));
}

void UaSession::load_area(unsigned number)
{
    // The kit numbers areas from 1 to 63.
    number &= 0x3F;
    const auto bytes = files_.data("GEO", number);
    if (!bytes)
    {
        throw std::runtime_error("The design has no area " + std::to_string(number));
    }
    auto area = decode_area(*bytes);
    if (!area)
    {
        throw std::runtime_error("Area " + std::to_string(number) + " does not decode");
    }
    area_ = {number, std::move(*area)};
    view_art_ = area_.overland() ? ViewArt{} : load_view_art(files_, area_.area);
    snapshot_.area_id = number;
    snapshot_.visited.reset();
    snapshot_.seen.reset();
}

void UaSession::campaign_party(std::shared_ptr<CampaignParty> party)
{
    party_ = std::move(party);
    if (!started_)
    {
        started_ = true;
        // On the first entry, the start square's event runs.
        run_events(square_event(snapshot_.pose.x, snapshot_.pose.y));
    }
}

EventRecord UaSession::record(unsigned number) const
{
    if (number == 0 || number > event_count)
    {
        throw std::out_of_range("No such event");
    }
    EventRecord event{};
    std::copy_n(area_.area.events.begin() + static_cast<std::ptrdiff_t>((number - 1) * event_size),
                event_size, event.begin());
    return event;
}

unsigned UaSession::square_event(unsigned x, unsigned y) const
{
    if (x >= area_.area.columns || y >= area_.area.rows)
    {
        return 0;
    }
    const auto event = area_.area.squares[std::size_t{y} * area_.area.columns + x].event;
    return event <= event_count ? event : 0;
}

std::string UaSession::text(unsigned slot, bool names) const
{
    const auto &texts = area_.area.texts.texts;
    if (slot == 0 || slot > texts.size())
    {
        return {};
    }
    auto words = texts[slot - 1];
    if (!names || !party_ || !party_->selected())
    {
        return words;
    }
    const auto name = party_->member(party_->selected()).character.sheet().name;
    for (int n = 0; n < 2; ++n)
    {
        const auto caret = words.find('^');
        if (caret == std::string::npos)
        {
            break;
        }
        words.replace(caret, 1, name);
    }
    return words;
}

void UaSession::move_party(unsigned x, unsigned y, MapDirection facing)
{
    snapshot_.pose = {x, y, facing};
    if (x < 16 && y < 16)
    {
        snapshot_.visited.set(y * 16 + x);
    }
}

void UaSession::step_back()
{
    if (step_back_pose_)
    {
        snapshot_.pose.x = step_back_pose_->x;
        snapshot_.pose.y = step_back_pose_->y;
    }
    step_back_pose_ = snapshot_.pose;
}

void UaSession::place(unsigned area, unsigned x, unsigned y, MapDirection facing)
{
    if (running_)
    {
        throw std::logic_error("An event is running");
    }
    if (area != area_.number)
    {
        load_area(area);
    }
    if (x >= area_.area.columns || y >= area_.area.rows)
    {
        throw std::out_of_range("The square is outside the area");
    }
    move_party(x, y, facing);
    step_back_pose_ = snapshot_.pose;
    snapshot_.phase = TourPhase::completed;
    refresh_snapshot();
}

void UaSession::look()
{
    if (running_ || party_lost_)
    {
        return;
    }
    looking_ = true;
    advance_minutes(10);
    run_events(square_event(snapshot_.pose.x, snapshot_.pose.y));
    looking_ = false;
}

bool UaSession::can_step(MapDirection direction) const
{
    const auto &square =
        area_.area.squares[std::size_t{snapshot_.pose.y} * area_.area.columns + snapshot_.pose.x];
    const auto wall = square.walls[por::index(direction)];
    const auto kind = wall_kind(wall);
    // Out of doors any obstacle stops the party.
    if (area_.overland())
    {
        return kind == WallKind::open;
    }
    switch (kind)
    {
    case WallKind::open:
    case WallKind::open_secret:
        return true;
    case WallKind::blocked:
    case WallKind::false_door:
        return false;
    default:
        break;
    }
    if (kind >= WallKind::first_key && kind <= WallKind::last_key)
    {
        // A key's door opens while the party holds that key (its flag).
        return quest_flags_[static_cast<unsigned>(kind) -
                            static_cast<unsigned>(WallKind::first_key)] != 0;
    }
    // A locked door waits for a bash, a pick or Knock, which come later.
    return false;
}

bool UaSession::explore(por::ExplorationCommand command)
{
    using por::ExplorationCommand;
    if (running_ || party_lost_ || encounter_)
    {
        return false;
    }
    auto &pose = snapshot_.pose;
    switch (command)
    {
    case ExplorationCommand::turn_left:
        pose.facing = por::turned_left(pose.facing);
        break;
    case ExplorationCommand::turn_right:
        pose.facing = por::turned_right(pose.facing);
        break;
    case ExplorationCommand::turn_around:
        pose.facing = por::reversed(pose.facing);
        break;
    case ExplorationCommand::look:
        look();
        return true;
    case ExplorationCommand::forward:
    {
        if (!can_step(pose.facing))
        {
            return false;
        }
        step_back_pose_ = pose;
        const int columns = area_.area.columns, rows = area_.area.rows;
        // A step off the area's edge comes in at the other, as in the kit.
        const auto x = (static_cast<int>(pose.x) + por::step_x(pose.facing) + columns) % columns;
        const auto y = (static_cast<int>(pose.y) + por::step_y(pose.facing) + rows) % rows;
        move_party(static_cast<unsigned>(x), static_cast<unsigned>(y), pose.facing);
        ++snapshot_.footsteps;
        advance_minutes(area_.overland() ? 12 * 60 : (snapshot_.searching ? 10 : 1));
        tavern_tales_.clear();
        run_events(square_event(pose.x, pose.y));
        return true;
    }
    default:
        return false;
    }
    refresh_snapshot();
    return true;
}

Image UaSession::observe_view()
{
    const auto &pose = snapshot_.pose;
    if (area_.overland())
    {
        throw std::logic_error("An overland area has no 3D view");
    }
    return render_view(area_.area, view_art_, pose.x, pose.y, pose.facing,
                       hour() < 6 || hour() >= 20);
}

std::optional<Image> UaSession::view_picture()
{
    if (area_.overland())
    {
        return std::nullopt;
    }
    return observe_view();
}

std::string UaSession::area_name() const
{
    // The header's name runs from +76h to the wandering monster at +106h.
    constexpr std::size_t name_at = 0x76, name_end = 0x106;
    const auto &header = area_.area.header;
    const auto *first = header.data() + name_at;
    const auto *last = std::find(first, header.data() + name_end, std::uint8_t{0});
    return {first, last};
}

std::uint8_t UaSession::quest_flag(unsigned number) const
{
    return quest_flags_.at(number);
}

void UaSession::advance_minutes(unsigned minutes)
{
    if (party_)
    {
        party_->advance_time(std::chrono::minutes(minutes));
    }
    else
    {
        clock_minutes_ += minutes;
    }
}

unsigned UaSession::hour() const
{
    const auto minutes = party_ ? party_->state().time_minutes : clock_minutes_;
    return static_cast<unsigned>(minutes / 60 % 24);
}

std::vector<MemberId> UaSession::party_members() const
{
    std::vector<MemberId> members;
    if (!party_)
    {
        return members;
    }
    for (const auto id : party_->state().slots)
    {
        if (id)
        {
            members.push_back(id);
        }
    }
    return members;
}

std::vector<MemberId> UaSession::active_members() const
{
    auto members = party_members();
    std::erase_if(members,
                  [this](MemberId id)
                  {
                      const auto &vitals = party_->member(id).vitals;
                      return vitals.dead || vitals.hit_points <= 0;
                  });
    return members;
}

bool UaSession::party_has_class_group(unsigned group) const
{
    // The kit's groups, by the levels a member has: 0 clerics, 2 fighters,
    // 3 paladins, 4 rangers, 5 magic-users, 6 thieves.
    static const std::array<std::string_view, 7> classes{
        "cleric", "", "fighter", "paladin", "ranger", "magic_user", "thief"};
    if (group >= classes.size() || classes[group].empty())
    {
        return false;
    }
    return std::ranges::any_of(party_members(),
                               [&](MemberId id)
                               {
                                   const auto &levels =
                                       party_->member(id).character.sheet().level_classes;
                                   return std::ranges::find(levels, classes[group]) != levels.end();
                               });
}

bool UaSession::party_has_race(unsigned race) const
{
    // The kit's races: 0 elf, 1 half-elf, 2 dwarf, 3 gnome, 4 halfling, 5 human.
    static const std::array<std::string_view, 6> races{"Elf",   "Half-Elf", "Dwarf",
                                                       "Gnome", "Halfling", "Human"};
    if (race >= races.size())
    {
        return false;
    }
    return std::ranges::any_of(party_members(),
                               [&](MemberId id)
                               {
                                   return party_->member(id).character.sheet().race == races[race];
                               });
}

unsigned UaSession::roll(unsigned sides)
{
    dice_ = dice_ * 1103515245U + 12345U;
    return sides == 0 ? 0 : (dice_ >> 16) % sides + 1;
}

unsigned UaSession::roll_percent()
{
    return roll(100) - 1;
}

void UaSession::damage_member(const EventRecord &event, MemberId member)
{
    if (!party_)
    {
        return;
    }
    rules::HazardAttack attack;
    attack.automatic = true;
    attack.dice = event[9];
    attack.sides = event[10];
    attack.damage_bonus = event[11];
    const unsigned defence = (event[7] & 0x30) >> 4;
    // A saving throw that halves the damage (defence 1) or spares the member
    // it (2); defence 0 rolls none, as a save changes nothing.
    if (defence == 1 || defence == 2)
    {
        attack.original_save = (event[13] & 0x70) >> 4;
        attack.original_save_modifier = event[13] & 0xF;
        attack.save_dc = 1;
        attack.half_on_save = defence == 1;
    }
    else if (defence == 3)
    {
        // An attack roll with the figure byte 12 gives, as THAC0.
        attack.automatic = false;
        attack.original_attack = 60 - event[12];
    }
    (void)party_->hazard_attack(member, attack);
}

void UaSession::hand_over_treasure()
{
    if (!party_)
    {
        return;
    }
    constexpr std::array<Coin, 3> money{Coin::platinum, Coin::gems, Coin::jewelry};
    for (const auto coin : money)
    {
        const auto count = treasure_wealth_[static_cast<std::size_t>(coin)];
        if (count != 0)
        {
            party_->add_coins(coin, count);
        }
    }
    for (const auto number : treasure_items_)
    {
        auto item =
            number == 0xFF ? random_scroll() : design_item(items_, number, treasure_identified_);
        if (!item)
        {
            continue;
        }
        // The first member with room takes it; with no room it stays behind.
        for (const auto id : active_members())
        {
            try
            {
                party_->take_item(id, *item);
                break;
            }
            catch (const std::runtime_error &)
            {
                // Too heavy for this member: the next one tries.
            }
        }
    }
    treasure_wealth_ = {};
    treasure_items_.clear();
}

// Item FFh of a treasure: a scroll of three magic-user spells picked at
// random (shop.c, scroll_make).
std::optional<por::Equipment> UaSession::random_scroll()
{
    constexpr std::uint8_t scroll_type = 0x27;
    if (scroll_type >= items_.types.size())
    {
        return std::nullopt;
    }
    por::Equipment scroll;
    auto &stored = scroll.stored;
    stored.type = scroll_type;
    stored.name_components = {0x66, 0x27, 0x28};
    stored.magic_bonus = 1;
    stored.weight = 1;
    stored.value = 3000;
    stored.hidden_name_words = 4;
    for (auto &number : stored.effect_codes)
    {
        do
        {
            number = static_cast<std::uint8_t>(roll(126));
        } while (!original::spell(number) ||
                 original::spell(number)->caster_class != original::SpellClass::magic_user);
    }
    scroll.base = items_.types[scroll_type];
    scroll.bonuses = por::equipment_bonuses(stored, scroll.base);
    return scroll;
}

// Kinds 1 and 33: the event's groups of monsters (all of them, or one picked
// at random), up to six of a count and a MONST number each.
void UaSession::combat_event(const EventRecord &event, bool pick_one)
{
    show_event_text(event,
                    [this, event, pick_one]
                    {
                        begin_combat(event, pick_one);
                    });
}

void UaSession::begin_combat(const EventRecord &event, bool pick_one)
{
    std::vector<std::pair<unsigned, unsigned>> groups; // MONST number, count.
    for (std::size_t group = 0; group < 6; ++group)
    {
        const unsigned count = event[8 + 2 * group] & 0x1F, number = event[9 + 2 * group];
        if (count != 0 && number != 0)
        {
            groups.emplace_back(number, count);
        }
    }
    if (pick_one && !groups.empty())
    {
        groups = {groups[roll(static_cast<unsigned>(groups.size())) - 1]};
    }
    if (groups.empty() || !party_)
    {
        return;
    }
    CampaignEncounter encounter;
    // An open field for now; the kit's own battlefields (DUNGCOM, WILDCOM)
    // come later (docs/games/ua.md).
    auto &board = encounter.field.geometry;
    board.width = 50;
    board.height = 25;
    board.terrain.assign(static_cast<std::size_t>(board.width * board.height),
                         rules::Terrain::open);
    encounter.field.tiles.assign(board.terrain.size(), 0);
    encounter.facing = snapshot_.pose.facing;
    // An encounter's choice decides the surprise; else the fight's own bits.
    encounter.surprise =
        encounter_surprise_.value_or(Surprise{static_cast<std::uint8_t>((event[8] & 0xC0) >> 6)});
    encounter_surprise_.reset();
    encounter.distance = std::min(2U, static_cast<unsigned>((event[14] & 0x60) >> 5));
    encounter.morale = std::min(100U, event[7] & 0x7FU);
    rules::EntityId next = first_monster_id;
    encounter_experience_ = 0;
    // The kit fields at most 50 monsters.
    constexpr unsigned most_monsters = 50;
    for (const auto &[number, count] : groups)
    {
        const auto monster = read_monster(files_, number);
        for (unsigned n = 0; n < count && next < first_monster_id + most_monsters; ++n)
        {
            rules::Participant fighter{
                next++, monster_key(number), monster.name, rules::Side::opposition, {}};
            const unsigned own = monster.raw[0x93] >= 0x80 ? (monster.raw[0x93] & 0x7FU) * 2 : 0;
            fighter.morale = own > 102 ? 0 : std::min(own, 100U);
            fighter.intelligence = std::min<unsigned>(monster.raw[0x72], 30);
            encounter.enemies.push_back(std::move(fighter));
            encounter_experience_ += word_at(monster.raw, 0x54);
        }
    }
    party_can_lose_ = (event[12] & 0x40) == 0;
    encounter_ = std::move(encounter);
    pending_.wait = Wait::combat;
    snapshot_.phase = TourPhase::combat;
}

bool UaSession::reject_combat(std::string diagnostic)
{
    if (pending_.wait != Wait::combat || !encounter_ || (party_ && party_->in_combat()))
    {
        return false;
    }
    encounter_.reset();
    pending_ = {};
    running_ = false;
    snapshot_.phase = TourPhase::faulted;
    snapshot_.diagnostic = std::move(diagnostic);
    refresh_snapshot();
    return true;
}

bool UaSession::resolve_combat(const rules::Snapshot &outcome)
{
    if (pending_.wait != Wait::combat || !encounter_ || !party_ ||
        outcome.outcome == rules::Outcome::ongoing || party_->in_combat())
    {
        return false;
    }
    encounter_.reset();
    pending_ = {};
    snapshot_.phase = TourPhase::running;
    if (outcome.outcome == rules::Outcome::victory)
    {
        // Each member still standing shares the monsters' experience.
        const auto standing = active_members();
        if (!standing.empty())
        {
            const auto share = encounter_experience_ / static_cast<unsigned>(standing.size());
            for (const auto id : standing)
            {
                party_->grant_script_experience(share, id);
            }
        }
        hand_over_treasure();
    }
    else if (outcome.outcome == rules::Outcome::defeat && party_can_lose_)
    {
        party_lost_ = true;
    }
    pump();
    return true;
}

void UaSession::refresh_snapshot()
{
    ++snapshot_.revision;
    // A lost party and a refused fight end the adventure where it stands.
    if (!running_ && pending_.wait == Wait::nothing && snapshot_.phase != TourPhase::defeated &&
        snapshot_.phase != TourPhase::faulted)
    {
        snapshot_.phase = TourPhase::completed;
    }
}

} // namespace opengold::ua
