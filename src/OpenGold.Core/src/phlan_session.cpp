#include "opengold/rolf_tour.h"
#include "opengold/diagnostic_log.h"
#include "opengold/creature_art.h"
#include "opengold/script_secrets.h"
#include "opengold/random_treasure.h"
#include "opengold/ecl_party_host.h"
#include "opengold/scroll_key.h"
#include "opengold/authored_items.h"
#include "opengold/companions.h"
#include <algorithm>
#include <set>

namespace opengold::por
{
// Arrow volleys come from Norris the Gray's hidden kobold archers: each arrow is
// an attack roll with the SRD 5.2.1 Kobold Warrior's +4 ranged bonus.
constexpr int hidden_archer_attack_bonus = 4;
// The time a step takes, which ECL CLOCK also counts in.
constexpr std::chrono::seconds step_time{6};

const PhlanResources &RolfTourSession::area_resources() const
{
    return current_area_ ? *town_->districts.at(current_area_) : *town_;
}

const EncounterOverride *
RolfTourSession::encounter_override(const EclInstruction &instruction) const
{
    if (!town_)
    {
        return nullptr;
    }
    const auto bank = town_->script_banks.find(current_script_);
    if (bank == town_->script_banks.end())
    {
        return nullptr;
    }
    const auto found =
        town_->encounter_overrides.find({bank->second, current_script_, instruction.address});
    return found == town_->encounter_overrides.end() ? nullptr : &found->second;
}

const ExportedMonster *RolfTourSession::exported_creature(const PhlanResources &area,
                                                          unsigned record) const
{
    return town_ && town_->monsters ? town_->monsters->creature(area.bank, record) : nullptr;
}

void RolfTourSession::change_area(unsigned id)
{
    if (id == current_area_)
    {
        return;
    }
    const auto &resource = id ? *town_->districts.at(id) : *town_;
    if (!resource.map)
    {
        throw EclError("District map is missing");
    }
    // A gate's script writes the party's square in the new area before it
    // changes areas, so that square was marked in the old area's map memory.
    // It stays there only if the party had been there before this event.
    if (event_checkpoint_ && event_checkpoint_->area == current_area_)
    {
        const auto arrival = snapshot_.pose.y * 16 + snapshot_.pose.x;
        snapshot_.visited[arrival] = event_checkpoint_->snapshot.visited[arrival];
        snapshot_.seen[arrival] = event_checkpoint_->snapshot.seen[arrival];
    }
    visited_areas_[current_area_] = snapshot_.visited;
    seen_areas_[current_area_] = snapshot_.seen;
    plaza_group_rolled_ = false;
    current_area_ = id;
    snapshot_.area_id = id;
    map_ = *resource.map;
    // Curse's walls are what its scripts loaded last (LOAD PIECES), whatever the map.
    if (game() == GoldBoxGame::pool_of_radiance || wall_art_.appearances.empty())
    {
        wall_art_ = resource.wall_art;
    }
    apply_entrances();
    snapshot_.visited = visited_areas_[id];
    snapshot_.seen = seen_areas_[id];
    picture_.reset();
    ++snapshot_.picture_revision;
    snapshot_.sprite_frame = -1;
    ++snapshot_.revision;
}

// The script commands the party answers; Curse adds DESTROY ITEMS (64).
void RolfTourSession::enable_party_commands()
{
    for (const std::uint8_t op :
         std::initializer_list<std::uint8_t>{11, 29, 30, 34, 35, 41, 46, 52, 54, 59})
    {
        machine_.enable_host(op);
    }
    if (game() != GoldBoxGame::pool_of_radiance)
    {
        machine_.enable_host(64);
    }
}

void RolfTourSession::campaign_party(std::shared_ptr<opengold::CampaignParty> party)
{
    campaign_ = std::move(party);
    restart();
}

void RolfTourSession::attach_restored_party(std::shared_ptr<opengold::CampaignParty> party)
{
    campaign_ = std::move(party);
    if (campaign_)
    {
        enable_party_commands();
    }
}

namespace
{

// GEO door code 2. The original engine answers it with "Locked." and Bash/Pick/
// Knock/Exit; code 3 (wizard locked) needs magic or exceptional AD&D Strength.
constexpr std::uint8_t locked_door = 2, wizard_locked_door = 3;
// Authored SRD conversion of the original Strength-table bash: a Medium (DC 15)
// Strength (Athletics) check. A wizard-locked door takes DC 35, the user's
// choice (PODOL-1); Knock always opens either.
constexpr int locked_door_difficulty = 15, wizard_locked_difficulty = 35;

[[nodiscard]] constexpr bool lock(std::uint8_t door) noexcept
{
    return door == locked_door || door == wizard_locked_door;
}

struct Edge
{
    unsigned x{}, y{};
    MapDirection side{};
    unsigned next_x{}, next_y{};
    MapDirection other{};
};

Edge edge_ahead(PartyPose pose)
{
    const int x = static_cast<int>(pose.x) + step_x(pose.facing),
              y = static_cast<int>(pose.y) + step_y(pose.facing);
    return {pose.x,
            pose.y,
            pose.facing,
            static_cast<unsigned>((x + 16) % 16),
            static_cast<unsigned>((y + 16) % 16),
            reversed(pose.facing)};
}

// The script's surprise bits (0x6DCB). SURPRISE writes only 0 to 3, but a
// script may store any number there.
opengold::Surprise script_surprise(std::uint16_t bits)
{
    if (bits > static_cast<std::uint16_t>(opengold::Surprise::both))
    {
        throw EclError("Unsupported encounter surprise " + std::to_string(bits));
    }
    return static_cast<opengold::Surprise>(bits);
}

using opengold::rules::Message;

// The English text of a host message, for logs and tests.
std::string english(const Message &message)
{
    auto text = message.source;
    for (const auto &argument : message.arguments)
    {
        for (std::size_t at = text.find("{" + argument.name + "}"); at != std::string::npos;
             at = text.find("{" + argument.name + "}"))
        {
            text.replace(at, argument.name.size() + 2, argument.value);
        }
    }
    return text;
}

std::string english(const std::vector<Message> &messages)
{
    std::string text;
    for (const auto &message : messages)
    {
        text += (text.empty() ? "" : "\n") + english(message);
    }
    return text;
}

// Thousands separated as the halls' fees are written ("1,000").
std::string amount(std::uint32_t value)
{
    auto text = std::to_string(value);
    for (auto at = text.size(); at > 3; at -= 3)
    {
        text.insert(at - 3, ",");
    }
    return text;
}

Message hall_welcome(opengold::TrainingHall hall)
{
    using opengold::TrainingHall;
    switch (hall)
    {
    case TrainingHall::fighters:
        return {"The Fighters' hall trains Fighters, Barbarians, Paladins and Rangers.", {}};
    case TrainingHall::clerics:
        return {"The Clerics' hall trains Clerics and Druids.", {}};
    case TrainingHall::magic_users:
        return {"The Magic-users' hall trains Wizards, Sorcerers and Warlocks.", {}};
    case TrainingHall::thieves:
        return {"The Thieves' hall trains Rogues, Bards and Monks.", {}};
    case TrainingHall::every_class:
        return {"The hall trains every class.", {}};
    }
    return {};
}

// Why a member cannot buy a session or lesson, for the dialogue.
// The original's close-up portrait: an 88 x 40 HEAD record above an 88 x 48
// BODY record.
Image close_up_portrait(const PhlanResources &resources, unsigned head_id, unsigned body_id)
{
    const auto head = resources.heads.find(head_id), body = resources.bodies.find(body_id);
    if (head == resources.heads.end() || body == resources.bodies.end() ||
        head->second.width != 88 || body->second.width != 88 || head->second.height != 40 ||
        body->second.height != 48)
    {
        throw EclError("Unsupported town portrait composition");
    }
    Image portrait;
    portrait.width = portrait.height = 88;
    portrait.rgba = head->second.rgba;
    portrait.rgba.insert(portrait.rgba.end(), body->second.rgba.begin(), body->second.rgba.end());
    return portrait;
}

// City Hall's script (ECL3:8), and the Cadorna Textile House's quest cells it
// reads: the family treasure (1 sealed, 128 opened) and its reward slot.
constexpr unsigned city_hall_script = 8;
// The Pyramid's poisoned rivers open once its sorcerer is beaten.
constexpr EclAddress pyramid_valley_healed{0x4AA2};
constexpr EclAddress cadorna_box{0x4AC8}, cadorna_payment{0x4AB8};

// The minute of the day on the campaign clock, which starts at noon.
std::uint64_t clock_minute_of_day(std::uint64_t minutes)
{
    return (minutes % 1440 + 720) % 1440;
}

// The areas next to the loaded ones that the game cannot enter yet.
// The original's saving throws (paralysis, poison or death; petrification or
// polymorph; rod, staff or wand; breath weapon; spell) as SRD abilities, and
// the DC of one with no bonus (MENDOR-1).
constexpr std::array original_save_abilities{rules::Ability::constitution,
                                             rules::Ability::constitution,
                                             rules::Ability::dexterity, rules::Ability::dexterity,
                                             rules::Ability::wisdom};
constexpr int original_save_dc = 13;

// The damage of a DAMAGE call that hurts members directly, by its script and
// address: the original names no damage type, nor what a volley throws.
struct ScriptDamage
{
    unsigned script;
    std::uint16_t address;
    std::string_view type;
    std::string_view missile = "arrow";
};

std::optional<ScriptDamage> listed_damage(unsigned script, std::uint16_t address)
{
    constexpr std::array types{
        ScriptDamage{2, 0xADB1, "bludgeoning"}, // the Cadorna well's fall
        ScriptDamage{15, 0x9D31, "acid"},       // Mendor's green slime
        // Stojanow Gate (GATE-1).
        ScriptDamage{9, 0xA155, "bludgeoning", "rock"}, // from the gate
        ScriptDamage{9, 0xA94E, "bludgeoning"},         // a bugbear's punch
        // The western wilderness's sites (SITES-1).
        ScriptDamage{19, 0xA0DF, "fire"}, // Diogenes's fireball
        ScriptDamage{28, 0xA4B1, "slashing", "sword stroke"},
        // Valjevo Castle (CASTLE-1).
        ScriptDamage{3, 0xA597, "bludgeoning", "bucket of sand"},
        ScriptDamage{3, 0xA9C0, "bludgeoning", "boulder"},
        ScriptDamage{3, 0xAB76, "bludgeoning", "falling stone"},
        ScriptDamage{3, 0xAF93, "bludgeoning"}, // drowning
        ScriptDamage{4, 0xAAD4, "fire", "scalding pot"},
        ScriptDamage{5, 0x9BC3, "poison"}, // a poisonous thorn
        ScriptDamage{5, 0xA190, "bludgeoning", "boulder"},
        ScriptDamage{7, 0x9D4F, "bludgeoning"}, // the trap door's fall
        // Valjevo's Temple of Bane (TOWN-1).
        ScriptDamage{6, 0xA579, "necrotic"}, // Bane's blessing
        ScriptDamage{6, 0xA62F, "fire"},     // the priest's fireball
        ScriptDamage{6, 0xA63A, "fire"}, ScriptDamage{6, 0xA809, "necrotic"}, // the evil gaze
        // Valhingen Graveyard's towers (GRAVE-1).
        ScriptDamage{10, 0xA53E, "poison"},    // noxious fumes
        ScriptDamage{10, 0xAA5F, "lightning"}, // a sharp crack
        // The kobold caves' traps and rocks (KOBOLD-1).
        ScriptDamage{13, 0x9CF3, "bludgeoning"}, // the roof
        ScriptDamage{13, 0x9E73, "piercing"},    // spikes
        ScriptDamage{13, 0xA1A4, "piercing", "ballista bolt"},
        ScriptDamage{13, 0xB48A, "bludgeoning", "rock"},
        // Kovel Mansion's traps and thieves (KOVEL-1).
        ScriptDamage{14, 0x9A98, "slashing"}, // a scything blade
        ScriptDamage{14, 0x9ADD, "poison"},   // poison gas
        ScriptDamage{14, 0x9B2A, "slashing"}, // a net of blades
        ScriptDamage{14, 0x9C64, "piercing"}, // a backstab
        ScriptDamage{14, 0xA239, "piercing"}, // a needle
        ScriptDamage{14, 0xA450, "poison"},   // yellow gas
        ScriptDamage{14, 0xA5F7, "poison"},   // a poisoned needle
        ScriptDamage{14, 0xA708, "piercing"}, // a dart
        ScriptDamage{14, 0xA7EA, "poison"},   // noxious powder
        ScriptDamage{14, 0xA8D4, "piercing"}, // a shower of darts
        ScriptDamage{14, 0xB566, "bludgeoning", "sling bullet"},
        // The lizard men's javelins (LIZARD-1).
        ScriptDamage{16, 0xA837, "piercing", "javelin"},
        ScriptDamage{16, 0xAAB8, "piercing", "javelin"},
        ScriptDamage{16, 0xAD0C, "piercing", "javelin"},
        ScriptDamage{16, 0xAD17, "piercing", "javelin"},
        // The Pyramid's pumping equipment blowing up, the
        // password door's clawed monster, and the
        // enraged slaves' barrels of the black contaminant.
        ScriptDamage{23, 0xA8E0, "bludgeoning"}, ScriptDamage{23, 0xA559, "slashing"},
        ScriptDamage{23, 0xAB0A, "poison"}, ScriptDamage{23, 0xAB15, "poison"}};
    for (const auto &known : types)
    {
        if (known.script == script && known.address == address)
        {
            return known;
        }
    }
    return std::nullopt;
}

// " (2d6: 3 + 4, + 1 = 8)": the dice behind a hazard's damage, before resistance.
std::string damage_dice_shown(const rules::HazardAttackResult &result, unsigned sides)
{
    if (result.damage_dice.empty())
    {
        return {};
    }
    std::string text =
        " (" + std::to_string(result.damage_dice.size()) + "d" + std::to_string(sides) + ":";
    int sum = 0;
    for (std::size_t n = 0; n < result.damage_dice.size(); ++n)
    {
        text += (n ? " + " : " ") + std::to_string(result.damage_dice[n]);
        sum += result.damage_dice[n];
    }
    if (const int added = result.damage_rolled - sum)
    {
        text += std::string(added > 0 ? ", + " : ", - ") + std::to_string(std::abs(added));
    }
    return text + " = " + std::to_string(result.damage_rolled) + ")";
}

std::string script_damage_type(unsigned script, std::uint16_t address)
{
    if (const auto damage = listed_damage(script, address))
    {
        return std::string(damage->type);
    }
    throw EclError("No damage type for DAMAGE at " + std::to_string(address));
}

// The letters of a line of rune glyphs ending at a zero byte. Elvish runes
// take two glyph tiles a letter, A at tile 0x40 and S on from 0x67; dwarvish
// runes one tile a letter, A at 0x79 (the Pyramid's NOKNOK, Valjevo's RHODIA).
std::string rune_text(const EclMachine &machine, EclAddress address)
{
    constexpr std::uint16_t dwarvish_a = 0x79;
    std::string text;
    for (unsigned n = 0; n < 64;)
    {
        const auto glyph =
            machine.variable(EclAddress{static_cast<std::uint16_t>(address.location + n)});
        if (!glyph)
        {
            return text;
        }
        if (glyph >= dwarvish_a && glyph < dwarvish_a + 26)
        {
            text += static_cast<char>('A' + (glyph - dwarvish_a));
            ++n;
            continue;
        }
        if (glyph < 0x40 || (glyph > 0x63 && glyph < 0x67) || glyph > 0x76)
        {
            throw EclError("Unknown rune glyph " + std::to_string(glyph));
        }
        text += glyph >= 0x67 ? static_cast<char>('S' + (glyph - 0x67) / 2)
                              : static_cast<char>('A' + (glyph - 0x40) / 2);
        n += 2;
    }
    throw EclError("Unterminated rune text");
}

Message refusal(opengold::TrainingDenial denial, const std::string &name, std::uint32_t fee)
{
    using opengold::TrainingDenial;
    switch (denial)
    {
    case TrainingDenial::ready_to_advance:
        return {
            "{name} has the experience for the next level: advance from the party roster first.",
            {{"name", name}}};
    case TrainingDenial::already_trained:
        return {"{name} has already trained in this at this level.", {{"name", name}}};
    case TrainingDenial::highest_level:
        return {"{name} is at the highest level.", {{"name", name}}};
    case TrainingDenial::gold:
        return {"The party's gold cannot pay {fee} gp.", {{"fee", amount(fee)}}};
    default:
        return {"{name} cannot train here.", {{"name", name}}};
    }
}

} // namespace

unsigned open_squares_ahead(const GeoMap &map, PartyPose pose)
{
    unsigned squares = 0;
    while (squares < 2 && !map.at(pose.x, pose.y).walls[index(pose.facing)])
    {
        ++squares;
        const auto ahead = edge_ahead(pose);
        pose.x = ahead.next_x;
        pose.y = ahead.next_y;
    }
    return squares;
}

void RolfTourSession::configure_town()
{
    // Campaign flags, area-local flags, host registers and scratch strings have
    // explicit lifetimes. These are logical VM cells, never native addresses.
    // The Cadorna Textile House's well searches 0x4900-0x497F for a falling
    // charm on the climber (ECL4:2 0xACF5); no script writes there, so it
    // finds none.
    // Each zone where the campaign's game keeps it (game_address).
    const auto bind_range = [this](std::uint16_t first, std::uint16_t last)
    {
        const auto from = game_cell(EclAddress{first}).location,
                   to = game_cell(EclAddress{last}).location;
        for (auto a = from; a <= to; ++a)
        {
            machine_.bind_variable(EclAddress{a}, 0);
        }
    };
    const bool pool_of_radiance = game() == GoldBoxGame::pool_of_radiance;
    // Curse's scripts keep their state anywhere in the game's zone (a
    // marauders' ambush marks 4DCBh); Pool of Radiance's use its first half.
    bind_range(0x4900, pool_of_radiance ? 0x4AFF : 0x4CFF);
    bind_range(0x6B00, 0x6C1C);
    bind_range(0x6DA8, 0x6EFF);
    bind_range(0x9800, 0x98FF);
    machine_.bind_variable(game_cell(EclAddress{0xB8}), 0);
    machine_.bind_variable(game_cell(EclAddress{0xB9}), 0);
    machine_.bind_variable(game_cell(ecl_clock_hour), 12);
    machine_.bind_variable(game_cell(ecl_clock_day), 1);
    // The disk the opening script came from: New Phlan's 3 in Pool of Radiance.
    machine_.bind_variable(game_cell(ecl_script_bank),
                           pool_of_radiance ? 3 : static_cast<std::uint16_t>(town_->bank));
    machine_.bind_variable(game_cell(ecl_party_size), 1);
    // Kuto's Well's setup writes these two engine cells (65 or 8, and a table
    // value) when its plaza or catacombs load. Their meaning is unverified;
    // they are stored for the script and have no effect yet.
    machine_.bind_variable(game_cell(EclAddress{0xC059}), 0);
    machine_.bind_variable(game_cell(EclAddress{0xC05F}), 0);
    for (const auto address : {ecl_overland_direction, ecl_overland_target_x,
                               ecl_overland_target_y, ecl_overland_terrain, ecl_overland_tile})
    {
        machine_.bind_variable(game_cell(address), 0);
    }
    if (campaign_)
    {
        selected_character_ = campaign_->state().selected_slot;
    }
    for (const auto &w : character_reply(selected_character_).writes)
    {
        machine_.bind_variable(w.address, w.value);
    }
    for (const std::uint8_t op :
         std::initializer_list<std::uint8_t>{10, 28, 32, 33, 36, 39, 40, 50, 55, 56, 57, 60})
    {
        machine_.enable_host(op);
    }
    // When the Pyramid falls the Central Wilderness's script patches the
    // original's colours of 36 wilderness tiles in two buffers (ECL7:26
    // 0xAD54); the art here keeps them (PYRAMID-1).
    if (pool_of_radiance)
    {
        machine_.ignore_cells(EclAddress{0xD000}, EclAddress{0xD7FF});
        machine_.ignore_cells(EclAddress{0x6300}, EclAddress{0x6AFF});
    }
    if (campaign_)
    {
        enable_party_commands();
    }
    synchronize_clock();
}

void RolfTourSession::synchronize_clock()
{
    if (adopt_script_clock())
    {
        for (const auto &w : character_reply(selected_character_).writes)
        {
            machine_.bind_variable(w.address, w.value);
        }
    }
    for (const auto &w : clock_reply().writes)
    {
        machine_.bind_variable(w.address, w.value);
    }
}

// A script may set the clock: the nomads' feast lasts until hour 18 (ECL7:17
// 0xA980). Time moves on to the next such hour and minute, as in the original,
// whose clock those cells are.
// Time can change a member by itself: a Mummy's curse takes its hit points as a
// day ends. The record a script reads must follow, or reading it back would look
// like the script healing that member past the lowered maximum.
void RolfTourSession::pass_time(std::chrono::milliseconds elapsed)
{
    campaign_->advance_time(elapsed);
    for (const auto &w : character_reply(selected_character_).writes)
    {
        machine_.bind_variable(w.address, w.value);
    }
}

// True when the clock moved, so the caller must refresh the record a script reads.
bool RolfTourSession::adopt_script_clock()
{
    bool scripted_clock = false;
    const auto hour = game_cell(ecl_clock_hour), tens = game_cell(ecl_clock_minute_tens),
               ones = game_cell(ecl_clock_minute_ones);
    for (const auto clock_cell : {hour, tens, ones})
    {
        scripted_clock = machine_.consume_script_write(game_cell(clock_cell)) || scripted_clock;
    }
    if (!scripted_clock || !campaign_)
    {
        return false;
    }
    const std::uint64_t scripted =
        machine_.variable(hour) * 60U + machine_.variable(tens) * 10U + machine_.variable(ones);
    if (scripted >= 1440)
    {
        throw EclError("Invalid script clock");
    }
    const auto now = clock_minute_of_day(campaign_->state().time_minutes);
    read_character();
    campaign_->advance_time(std::chrono::minutes{(scripted + 1440 - now) % 1440});
    return true;
}

EclHostReply RolfTourSession::clock_reply() const
{
    EclHostReply reply;
    if (!campaign_)
    {
        return reply;
    }
    const auto minutes = campaign_->state().time_minutes;
    const auto minute_of_day = clock_minute_of_day(minutes);
    const auto days = minutes / 1440 + (minutes % 1440 + 720) / 1440;
    reply.writes = {
        {game_cell(ecl_clock_minute_ones), static_cast<std::uint16_t>(minute_of_day % 10)},
        {game_cell(ecl_clock_minute_tens), static_cast<std::uint16_t>((minute_of_day % 60) / 10)},
        {game_cell(ecl_clock_hour), static_cast<std::uint16_t>(minute_of_day / 60)},
        {game_cell(ecl_clock_day), static_cast<std::uint16_t>(days % 30 + 1)},
        {game_cell(ecl_clock_month), static_cast<std::uint16_t>((days / 30) % 12 + 1)},
        {game_cell(ecl_clock_year), static_cast<std::uint16_t>((days / 360) % 256)}};
    return reply;
}

// The original makes creature record 24 hostile as it joins, whatever the
// area (eclvm.pas EclAddNpc): Valhingen Graveyard's mystic, who turns on the
// party and fights for the vampire.
constexpr unsigned joins_against_party = 24;

bool RolfTourSession::turncoat(MemberId id) const
{
    const auto &source = campaign_->member(id).npc_source;
    return turncoats_.contains(id) ||
           (source.starts_with("por:MON") &&
            source.ends_with("CHA:" + std::to_string(joins_against_party)));
}

EclHostReply RolfTourSession::character_reply(PartySlot slot) const
{
    if (campaign_)
    {
        auto reply = party_character_reply(*campaign_, slot);
        // The view's cells are Pool of Radiance's; the game keeps them elsewhere.
        for (auto &write : reply.writes)
        {
            write.address = game_cell(write.address);
        }
        if (game() == GoldBoxGame::curse_of_the_azure_bonds && slot.index < 8)
        {
            // Curse's scripts also read the character's Intelligence and gender.
            if (const auto id = campaign_->state().slots[slot.index])
            {
                const auto &member = campaign_->member(id);
                const auto at = [&reply, this](std::uint16_t offset) -> std::uint16_t &
                {
                    const auto address = game_cell(EclAddress(
                        static_cast<std::uint16_t>(ecl_character_record.location + offset)));
                    return std::ranges::find(reply.writes, address, &EclMemoryWrite::address)
                        ->value;
                };
                at(0x15) = static_cast<std::uint16_t>(
                    member.character.sheet().scores[rules::Ability::intelligence]);
                at(0xD6) = member.character.creation_data().gender == "female" ? 1 : 0;
            }
        }
        // The party's size, which the scripts loop over member by member
        // (Tyranthraxus's vote) and test before a companion joins.
        const auto &slots = campaign_->state().slots;
        reply.writes.push_back(
            {game_cell(ecl_party_size),
             static_cast<std::uint16_t>(std::ranges::count_if(slots,
                                                              [](MemberId id)
                                                              {
                                                                  return id != MemberId{};
                                                              }))});
        // The record's allegiance byte: 0x81 for a member fighting for the
        // monsters (the original's hostile and computer-controlled), else 0.
        if (const auto id = slot.index < 8 ? campaign_->state().slots[slot.index] : MemberId{};
            id && turncoat(id))
        {
            std::ranges::find(reply.writes, game_cell(ecl_character_allegiance),
                              &EclMemoryWrite::address)
                ->value = 0x81;
        }
        return reply;
    }
    EclHostReply reply;
    std::array<std::uint16_t, 285> fields{};
    if (slot.index == 0)
    {
        for (std::size_t n = 0; n < party_.name.size() && n < 15; ++n)
        {
            fields[n] = party_.name[n];
        }
        fields[0x18] = 14; // Verified GBVM Constitution field; no guessed raw-record offsets.
        fields[0x100] = 1;
        fields[0x119] = static_cast<std::uint16_t>(party_.hit_points);
        for (unsigned n = 0; n < ecl_coin_addresses.size(); ++n)
        {
            fields[ecl_coin_addresses[n].location - ecl_character_record.location] =
                static_cast<std::uint16_t>(party_.wealth[n]);
        }
    }
    for (unsigned n = 0; n < fields.size(); ++n)
    {
        reply.writes.push_back(
            {game_cell(EclAddress(static_cast<std::uint16_t>(ecl_character_record.location + n))),
             fields[n]});
    }
    reply.writes.push_back({game_cell(EclAddress{0x6DB1}), static_cast<std::uint16_t>(slot.index)});
    reply.writes.push_back({game_cell(EclAddress{0x6DB4}), static_cast<std::uint16_t>(slot.index)});
    return reply;
}

bool RolfTourSession::dismissed_companion() const
{
    if (!campaign_ || selected_character_.index >= 8)
    {
        return false;
    }
    const auto id = campaign_->state().slots[selected_character_.index];
    return id && !campaign_->member(id).npc_source.empty() &&
           machine_.variable(game_cell(ecl_character_record)) == 0 &&
           machine_.variable(game_cell(ecl_character_status)) == 0;
}

void RolfTourSession::read_character()
{
    if (campaign_)
    {
        const auto exchange = campaign_->read_character(selected_character_, machine_, game());
        // A script that sets the allegiance byte to 0x81 turns the member
        // against the party (Tyranthraxus's vote, ECL5:7 0xA771); 0 or 0x80
        // turns it back.
        if (const auto id = selected_character_.index < 8
                                ? campaign_->state().slots[selected_character_.index]
                                : MemberId{})
        {
            const auto allegiance = machine_.variable(game_cell(ecl_character_allegiance));
            if (allegiance == 0x81)
            {
                turncoats_.insert(id);
            }
            else if (allegiance == 0 || allegiance == 0x80)
            {
                turncoats_.erase(id);
            }
        }
        if (exchange)
        {
            const auto payer = campaign_->state().slots[selected_character_.index];
            const auto &name = campaign_->member(payer).character.sheet().name;
            snapshot_.payments.push_back({name, *exchange});
        }
        return;
    }
    if (selected_character_.index != 0)
    {
        return;
    }
    for (unsigned n = 0; n < ecl_coin_addresses.size(); ++n)
    {
        party_.wealth[n] = machine_.variable(game_cell(ecl_coin_addresses[n]));
    }
    party_.hit_points = machine_.variable(game_cell(ecl_character_hit_points));
}

void RolfTourSession::bind_pose(PartyPose pose)
{
    machine_.bind_variable(game_cell(ecl_party_x), static_cast<std::uint16_t>(pose.x));
    machine_.bind_variable(game_cell(ecl_party_y), static_cast<std::uint16_t>(pose.y));
    machine_.bind_variable(game_cell(ecl_party_facing),
                           static_cast<std::uint16_t>(index(pose.facing)));
    const auto &cell = map_.at(pose.x, pose.y);
    machine_.bind_variable(game_cell(ecl_wall_ahead), cell.walls[index(pose.facing)]);
    machine_.bind_variable(game_cell(ecl_cell_event), cell.event_raw);
    publish_pose();
}

bool RolfTourSession::move_party(ExplorationCommand command)
{
    auto pose = snapshot_.pose;
    if (command == ExplorationCommand::turn_left)
    {
        pose.facing = turned_left(pose.facing);
    }
    else if (command == ExplorationCommand::turn_right)
    {
        pose.facing = turned_right(pose.facing);
    }
    else if (command == ExplorationCommand::turn_around)
    {
        pose.facing = reversed(pose.facing);
    }
    else if (command == ExplorationCommand::forward)
    {
        if (machine_.variable(game_cell(EclAddress{0x6DC9})) == 255)
        {
            return false;
        }
        const int x = static_cast<int>(pose.x) + step_x(pose.facing),
                  y = static_cast<int>(pose.y) + step_y(pose.facing);
        const auto wrapped_x = (x + 16) % 16, wrapped_y = (y + 16) % 16;
        const auto &a = map_.at(pose.x, pose.y);
        const auto side = pose.facing;
        // Ordinary doors are traversable. Locks retain their distinct codes.
        // Only the party's side of the edge stops a step (coab
        // TryStepForward): Stojanow Gate's passage opens southward from its
        // north side though its gates stay shut from the south.
        const auto blocked = [](unsigned wall, unsigned door)
        {
            return door > 1 || (wall && !door);
        };
        if (blocked(a.walls[index(side)], a.doors[index(side)]))
        {
            return false;
        }
        machine_.bind_variable(game_cell(EclAddress{0x49F0}), static_cast<std::uint16_t>(pose.x));
        machine_.bind_variable(game_cell(EclAddress{0x49F1}), static_cast<std::uint16_t>(pose.y));
        pose.x = wrapped_x;
        pose.y = wrapped_y;
        pick_tried_ = false;
        ++snapshot_.footsteps;
    }
    if (campaign_ && command == ExplorationCommand::forward)
    {
        // A step in Search mode takes ten minutes more (coab MovePartyForward).
        pass_time(step_time);
        if (snapshot_.searching)
        {
            pass_time(std::chrono::minutes{10});
        }
    }
    bind_pose(pose);
    synchronize_clock();
    return true;
}

void RolfTourSession::begin_event(EventTrigger trigger, std::optional<std::size_t> entry)
{
    // Loot left unclaimed until the party had room is named with this event.
    snapshot_.loot_taken.clear();
    snapshot_.gear_changes.clear();
    claim_loot();
    synchronize_clock();
    snapshot_.payments.clear();
    snapshot_.door_checks.clear();
    snapshot_.door_opened = false;
    snapshot_.secret_door_searches.clear();
    snapshot_.secret_door_found = false;
    snapshot_.camp_offered = false;
    if (diagnostic_log_enabled())
    {
        log_diagnostic(LogPriority::info, "event",
                       "script " + std::to_string(current_script_) + " begins in area " +
                           std::to_string(snapshot_.area_id) + " at " +
                           std::to_string(snapshot_.pose.x) + "," +
                           std::to_string(snapshot_.pose.y));
    }
    std::optional<opengold::PartyState> campaign;
    if (campaign_)
    {
        selected_character_ = campaign_->state().selected_slot;
        for (const auto &w : character_reply(selected_character_).writes)
        {
            machine_.bind_variable(w.address, w.value);
        }
        campaign = campaign_->checkpoint();
    }
    event_checkpoint_ = EventCheckpoint{.machine = machine_,
                                        .party = party_,
                                        .campaign = std::move(campaign),
                                        .script = current_script_,
                                        .area = current_area_,
                                        .visited_areas = visited_areas_,
                                        .seen_areas = seen_areas_,
                                        .entrances = entrances_,
                                        .wand_traps = wand_traps_,
                                        .rest_checks = rest_checks_,
                                        .travel_mode = travel_mode_,
                                        .map = map_,
                                        .pending_loot = pending_loot_,
                                        .snapshot = snapshot_,
                                        .selected_character = selected_character_};
    event_stage_ = trigger == EventTrigger::step   ? EventStage::step
                   : trigger == EventTrigger::camp ? EventStage::pre_camp
                                                   : EventStage::search;
    machine_.bind_variable(game_cell(EclAddress{0x6DC9}), 0);
    encounter_outcome_.clear();
    meeting_pose_.reset();
    robbed_ = false;
    member_chosen_ = false;
    bool off_map = false;
    if (trigger == EventTrigger::step && pending_movement_ && !overland())
    {
        const auto p = snapshot_.pose;
        const int x = static_cast<int>(p.x) + step_x(p.facing),
                  y = static_cast<int>(p.y) + step_y(p.facing);
        const auto &cell = map_.at(p.x, p.y);
        off_map = (x < 0 || y < 0 || x >= 16 || y >= 16) && cell.doors[index(p.facing)] <= 1 &&
                  (!cell.walls[index(p.facing)] || cell.doors[index(p.facing)]);
    }
    machine_.bind_variable(game_cell(EclAddress{0x6DD5}), off_map ? 1 : 0);
    // The original runs Look's search with its search flag at 1, as each step
    // in Search mode reads it (coab ovr003): the scripts test only 0 and 1.
    const bool searching =
        trigger == EventTrigger::look || (trigger == EventTrigger::step && snapshot_.searching);
    machine_.bind_variable(game_cell(EclAddress{0x6DCA}), searching ? 1 : 0);
    event_searched_ = searching;
    // The spokesman's reaction score (0x6DCF), which the scripts add to reaction
    // rolls: the original's table on Charisma, read for the leader (LEADER-1).
    if (campaign_)
    {
        if (const auto speaker = campaign_->spokesman())
        {
            machine_.bind_variable(
                game_cell(EclAddress{0x6DCF}),
                static_cast<std::uint16_t>(reaction_score(campaign_->reaction_charisma(speaker))));
        }
    }
    if (!machine_.start(entry.value_or(static_cast<std::size_t>(trigger))))
    {
        fail("Unable to start town script");
        return;
    }
    ++snapshot_.event_runs;
    snapshot_.phase = TourPhase::running;
    snapshot_.diagnostic.clear();
    snapshot_.choices.clear();
    ++snapshot_.revision;
}

void RolfTourSession::recall(opengold::MemberId caster, const std::string &action)
{
    log_player("member " + std::to_string(caster) + " casts " + action);
    if (!campaign_ || campaign_->in_combat() || !can_leave())
    {
        throw std::runtime_error("Word of Recall needs a quiet moment outside events");
    }
    const bool at_temple = temple_square_ && !current_area_ &&
                           temple_square_->script == current_script_ &&
                           temple_square_->pose == snapshot_.pose;
    if (!at_temple && !sanctuary_)
    {
        throw std::runtime_error(
            "No sanctuary is prepared: cast Word of Recall in a temple of New Phlan first.");
    }
    campaign_->use_camp_action(caster, caster, action);
    if (at_temple)
    {
        sanctuary_ = temple_square_;
        snapshot_.dialogue = "This temple is now the party's sanctuary for Word of Recall.";
        ++snapshot_.revision;
        return;
    }
    snapshot_.dialogue = "Word of Recall takes the party back to its sanctuary.";
    if (!current_area_ && current_script_ == sanctuary_->script)
    {
        bind_pose(sanctuary_->pose);
        return;
    }
    // From a district: the town's script takes over, as when the party walks
    // back through a gate, and its entry runs on the sanctuary's square.
    const auto program = town_->programs.find(sanctuary_->script);
    if (program == town_->programs.end())
    {
        throw std::runtime_error("The sanctuary's town script is missing");
    }
    machine_.switch_program(program->second);
    machine_.bind_variable(game_cell(EclAddress{0x49F2}),
                           static_cast<std::uint16_t>(current_script_));
    machine_.bind_variable(game_cell(ecl_script_bank), 3);
    current_script_ = sanctuary_->script;
    snapshot_.script_id = current_script_;
    change_area(0);
    bind_pose(sanctuary_->pose);
    begin_event(EventTrigger::step, 4);
    event_stage_ = EventStage::area_entry;
}

std::vector<unsigned> RolfTourSession::teleport_destinations() const
{
    std::vector<unsigned> maps;
    if (!town_)
    {
        return maps;
    }
    for (const auto &spot : teleport_spots)
    {
        if (spot.map && !town_->districts.contains(spot.map))
        {
            continue;
        }
        const auto saved = visited_areas_.find(spot.map);
        const bool been = spot.map == current_area_
                              ? snapshot_.visited.any()
                              : saved != visited_areas_.end() && saved->second.any();
        if (been)
        {
            maps.push_back(spot.map);
        }
    }
    return maps;
}

void RolfTourSession::teleport(opengold::MemberId caster, const std::string &action, unsigned map)
{
    log_player("member " + std::to_string(caster) + " casts " + action + " to map " +
               std::to_string(map));
    if (action != "teleport" && !action.starts_with("teleport_"))
    {
        throw std::runtime_error("Not a Teleport");
    }
    if (!campaign_ || campaign_->in_combat() || !can_leave())
    {
        throw std::runtime_error("Teleport needs a quiet moment outside events");
    }
    const auto destinations = teleport_destinations();
    if (std::find(destinations.begin(), destinations.end(), map) == destinations.end())
    {
        throw std::runtime_error("Teleport goes only to a place the party has been.");
    }
    const auto &spot = *teleport_spot(map);
    const TeleportArrival arrival{map, PartyPose{spot.x, spot.y, MapDirection::north}};
    const unsigned script = map ? town_->districts.at(map)->script : 0;
    campaign_->use_camp_action(caster, caster, action);
    snapshot_.dialogue = "Teleport takes the party away.";
    if (script == current_script_)
    {
        arrive(arrival);
        update_travel_mode();
        return;
    }
    enter_script(script, arrival);
}

void RolfTourSession::enter_script(unsigned script, const TeleportArrival &arrival)
{
    // Another script's map: its script takes over and its entry runs, as when
    // the party walks in, and then the party stands on the spot.
    const auto program = town_->programs.find(script);
    if (program == town_->programs.end())
    {
        throw std::runtime_error("The map's script is missing");
    }
    const auto bank = town_->script_banks.find(script);
    machine_.switch_program(program->second);
    machine_.bind_variable(game_cell(EclAddress{0x49F2}),
                           static_cast<std::uint16_t>(current_script_));
    machine_.bind_variable(
        game_cell(ecl_script_bank),
        static_cast<std::uint16_t>(bank == town_->script_banks.end() ? 3 : bank->second));
    current_script_ = script;
    snapshot_.script_id = current_script_;
    teleport_arrival_ = arrival;
    arrive(arrival);
    begin_event(EventTrigger::step, 4);
    event_stage_ = EventStage::area_entry;
}

void RolfTourSession::arrive(const TeleportArrival &arrival)
{
    change_area(arrival.map);
    if (arrival.map)
    {
        machine_.bind_variable(game_cell(ecl_current_map), static_cast<std::uint16_t>(arrival.map));
    }
    // A wilderness script's cave or grove is a 3D area, not the wilderness map.
    if (current_script_ >= 25 && current_script_ <= 27)
    {
        machine_.bind_variable(game_cell(ecl_in_3d_area), 1);
    }
    bind_pose(arrival.pose);
}

void RolfTourSession::claim_loot()
{
    if (!campaign_)
    {
        return;
    }
    for (auto it = pending_loot_.begin(); it != pending_loot_.end();)
    {
        const auto before = held_items();
        if (campaign_->award_loot(it->wealth, it->items, it->reward_id))
        {
            equip_gained(before);
            LootTaken taken{.wealth = it->wealth};
            for (const auto &item : it->items)
            {
                taken.items.push_back(item.label());
            }
            snapshot_.loot_taken.push_back(std::move(taken));
            it = pending_loot_.erase(it);
            ++snapshot_.revision;
        }
        else
        {
            ++it;
        }
    }
}

void RolfTourSession::pile_loot()
{
    if (!campaign_)
    {
        return;
    }
    for (auto it = pending_loot_.begin(); it != pending_loot_.end();)
    {
        const auto &claimed = campaign_->state().claimed_rewards;
        const bool repeat =
            std::find(claimed.begin(), claimed.end(), it->reward_id) != claimed.end();
        if (!campaign_->award_loot(it->wealth, {}, it->reward_id))
        {
            ++it;
            continue;
        }
        if (!repeat)
        {
            snapshot_.loot_taken.push_back({.wealth = it->wealth});
            for (auto &item : it->items)
            {
                treasure_pile_.push_back(std::move(item));
            }
        }
        it = pending_loot_.erase(it);
        ++snapshot_.revision;
    }
}

void RolfTourSession::open_treasure(std::uint64_t request, std::vector<EclMemoryWrite> writes)
{
    log_player("finds a treasure of " + std::to_string(treasure_pile_.size()) + " items");
    treasure_request_ = request;
    treasure_writes_ = std::move(writes);
    snapshot_.phase = TourPhase::treasure;
    snapshot_.continue_ticket = ++next_ticket_;
    snapshot_.choices.clear();
    snapshot_.diagnostic.clear();
    ++snapshot_.revision;
}

bool RolfTourSession::take_treasure(std::uint64_t ticket, std::size_t item)
{
    if (snapshot_.phase != TourPhase::treasure || !ticket || ticket != snapshot_.continue_ticket ||
        item >= treasure_pile_.size() || !campaign_ || !campaign_->selected())
    {
        return false;
    }
    const auto taker = campaign_->selected();
    log_player("takes " + treasure_pile_[item].label() + " (item " + std::to_string(item + 1) +
               " of " + std::to_string(treasure_pile_.size()) + ")");
    snapshot_.gear_changes.clear();
    const auto before = held_items();
    try
    {
        campaign_->take_item(taker, treasure_pile_[item]);
    }
    catch (const std::exception &e)
    {
        snapshot_.diagnostic = e.what();
        ++snapshot_.revision;
        return false;
    }
    equip_gained(before);
    treasure_pile_.erase(treasure_pile_.begin() + static_cast<std::ptrdiff_t>(item));
    snapshot_.diagnostic.clear();
    // A new ticket: the list the view shows has changed.
    snapshot_.continue_ticket = ++next_ticket_;
    ++snapshot_.revision;
    return true;
}

bool RolfTourSession::leave_treasure(std::uint64_t ticket)
{
    if (snapshot_.phase != TourPhase::treasure || !ticket || ticket != snapshot_.continue_ticket)
    {
        return false;
    }
    log_player(treasure_pile_.empty()
                   ? "leaves the treasure"
                   : "leaves " + std::to_string(treasure_pile_.size()) + " items behind");
    // The script reads its character afresh: the member may have taken items.
    auto reply = character_reply(selected_character_);
    reply.writes.insert(reply.writes.end(), treasure_writes_.begin(), treasure_writes_.end());
    if (!machine_.resume_host(treasure_request_, reply))
    {
        fail("The script did not take the treasure's end");
        return true;
    }
    treasure_pile_.clear();
    treasure_writes_.clear();
    treasure_request_ = 0;
    snapshot_.continue_ticket = 0;
    snapshot_.diagnostic.clear();
    snapshot_.phase = TourPhase::running;
    ++snapshot_.revision;
    advance(0);
    return true;
}

const std::vector<AnimationFrame> *RolfTourSession::monster_picture() const
{
    if (!showing_monster_picture_)
    {
        return nullptr;
    }
    return &area_resources().animations.at(*monster_picture_id_);
}

std::optional<unsigned> RolfTourSession::scene_animation_record() const
{
    if (showing_monster_picture_)
    {
        return monster_picture_id_;
    }
    // The close-up has played: the fight takes over.
    if (snapshot_.phase == TourPhase::combat)
    {
        return std::nullopt;
    }
    const auto beside_the_party =
        snapshot_.sprite_frame == 0 && machine_.variable(game_cell(EclAddress{0x6DE1})) == 255;
    const auto id = picture_           ? picture_animation_id_
                    : beside_the_party ? monster_picture_id_
                                       : std::nullopt;
    if (!id)
    {
        return std::nullopt;
    }
    const auto &animations = area_resources().animations;
    const auto found = animations.find(*id);
    return found != animations.end() && found->second.size() > 1 ? id : std::nullopt;
}

const std::vector<AnimationFrame> *RolfTourSession::scene_animation() const
{
    if (showing_monster_picture_)
    {
        return monster_picture();
    }
    const auto record = scene_animation_record();
    return record ? &area_resources().animations.at(*record) : nullptr;
}

std::optional<std::pair<unsigned, unsigned>> RolfTourSession::scene_animation_source() const
{
    const auto record = scene_animation_record();
    if (!record)
    {
        return std::nullopt;
    }
    return std::pair{area_resources().bank, *record};
}

bool RolfTourSession::start_encounter()
{
    if (snapshot_.phase != TourPhase::combat || !showing_monster_picture_)
    {
        return false;
    }
    showing_monster_picture_ = false;
    ++snapshot_.revision;
    return true;
}

bool RolfTourSession::reject_combat(std::string diagnostic)
{
    if (snapshot_.phase != TourPhase::combat || !encounter_ || !combat_request_ || !campaign_ ||
        campaign_->in_combat())
    {
        return false;
    }
    fail(std::move(diagnostic));
    return true;
}

// The arena's copy has no creature record of its own.
constexpr unsigned mirror_record = std::numeric_limits<unsigned>::max();

void RolfTourSession::stage_mirror()
{
    // The fight builds the copy from the duelist; here it gets its place
    // among the enemies and the original's combat icon 11.
    auto art = creature_art(area_resources().combat_archive, 11, nullptr, false);
    if (!art)
    {
        throw EclError("Invalid original combat icon");
    }
    art->entity = opengold::mirror_id;
    staged_enemies_.push_back({.id = opengold::mirror_id, .name = "Rolf"});
    staged_art_.push_back(std::move(*art));
    staged_records_.push_back(mirror_record);
}

unsigned RolfTourSession::record_experience(unsigned record) const
{
    if (!plays_original_records())
    {
        return area_resources().conversions.at(record).award_xp;
    }
    // The original's award: the record's own and so much for each of its Hit
    // Points (postcbt.pas, DetermineLootExp).
    const auto &stored = area_resources().encounter_creatures.at(record).stored;
    return stored.base_xp_award + unsigned{stored.xp_per_hit_point} * stored.rolled_hit_points;
}

unsigned RolfTourSession::treasure_experience(const PendingLoot &loot) const
{
    if (!plays_original_records())
    {
        return 0;
    }
    // The original's worth of the monsters' coins, gems and jewelry, and 400
    // for each plus of a magic item (postcbt.pas, DetermineLootExp).
    const auto &w = loot.wealth;
    unsigned experience =
        w[0] / 200 + w[1] / 20 + w[2] / 2 + w[3] + w[4] * 5 + w[5] * 250 + w[6] * 2200;
    for (const auto &item : loot.items)
    {
        if (item.stored.magic_bonus > 0)
        {
            experience += static_cast<unsigned>(item.stored.magic_bonus) * 400;
        }
    }
    return experience;
}

unsigned RolfTourSession::experience_share(unsigned total) const
{
    if (!plays_original_records())
    {
        return total;
    }
    // The original divides a fight's experience among the party's members
    // (postcbt.pas, DetermineLootExp); the rules then share each member's
    // among its classes.
    const auto members = std::ranges::count_if(campaign_->state().slots,
                                               [](MemberId id)
                                               {
                                                   return id != MemberId{};
                                               });
    return members ? total / static_cast<unsigned>(members) : total;
}

RolfTourSession::TurncoatLoot RolfTourSession::part_with_turncoats(const rules::Snapshot &result)
{
    TurncoatLoot loot;
    for (const auto id : encounter_->turncoats)
    {
        const auto unit = std::ranges::find(result.combatants, id, &rules::CombatantView::id);
        // Under the original rules a creature down may be below 0.
        const bool beaten = unit != result.combatants.end() && unit->hit_points <= 0;
        const auto &member = campaign_->member(id);
        if (beaten && result.outcome == rules::Outcome::victory)
        {
            ++loot.defeated;
            for (const auto &item : member.character.inventory().items())
            {
                if (const auto source = member.item_sources.find(item.id);
                    source != member.item_sources.end())
                {
                    loot.items.push_back(source->second);
                }
            }
            // A companion's original record pays its experience, as the
            // monster it fought as.
            const auto &source = member.npc_source;
            const auto prefix = "por:MON" + std::to_string(area_resources().bank) + "CHA:";
            if (source.starts_with(prefix))
            {
                const auto record = static_cast<unsigned>(std::stoul(source.substr(prefix.size())));
                if (plays_original_records() || area_resources().conversions.contains(record))
                {
                    loot.experience += record_experience(record);
                }
            }
        }
        else if (beaten)
        {
            ++loot.defeated;
        }
        // The original takes every hostile creature off its list after a fight.
        if (beaten)
        {
            campaign_->lose(id);
        }
        else
        {
            campaign_->remove(id);
        }
        turncoats_.erase(id);
    }
    return loot;
}

bool RolfTourSession::resolve_combat(const rules::Snapshot &result)
{
    if (diagnostic_log_enabled())
    {
        log_diagnostic(LogPriority::info, "fight",
                       "ends in area " + std::to_string(snapshot_.area_id) + ", outcome " +
                           std::to_string(static_cast<int>(result.outcome)));
    }
    if (snapshot_.phase != TourPhase::combat || !encounter_ || !combat_request_ || !campaign_ ||
        campaign_->in_combat() || result.outcome == rules::Outcome::ongoing ||
        result.identity != campaign_->identity())
    {
        return false;
    }
    unsigned defeated = 0;
    // Monsters that ran off the field, or that the original placement had no
    // room for (PLACE-1), are worth neither experience nor treasure.
    std::vector<unsigned> unpaid_records;
    std::set<rules::EntityId> enemies, party_ids;
    // Companions who turned on the party when one of its members attacked
    // its own side are its turncoats now, and leave it after the fight.
    if (result.allies_attacked)
    {
        for (const auto &unit : result.combatants)
        {
            const auto &slots = campaign_->state().slots;
            if (unit.side == rules::Side::opposition &&
                std::ranges::find(slots, unit.id) != slots.end() &&
                std::ranges::find(encounter_->turncoats, unit.id) == encounter_->turncoats.end())
            {
                encounter_->turncoats.push_back(unit.id);
            }
        }
    }
    // In a duel only the champion fights; the rest of the party stands by.
    const auto champion = encounter_->champion;
    const auto &turned = encounter_->turncoats;
    std::set<rules::EntityId> expected_party, turncoats_seen;
    for (auto id : campaign_->state().slots)
    {
        if (id && (!champion || id == champion) && std::ranges::find(turned, id) == turned.end())
        {
            expected_party.insert(id);
        }
    }
    for (const auto &unit : result.combatants)
    {
        // A member who fought for the monsters is beaten, or got away with them.
        if (unit.side == rules::Side::opposition &&
            std::ranges::find(turned, unit.id) != turned.end())
        {
            if (!turncoats_seen.insert(unit.id).second ||
                (result.outcome == rules::Outcome::victory && unit.hit_points > 0 && !unit.fled))
            {
                return false;
            }
            continue;
        }
        // A creature that fought beside the party until it was attacked.
        if (unit.side == rules::Side::opposition && opengold::is_ally(unit.id) &&
            result.allies_attacked)
        {
            continue;
        }
        // A member charmed onto the monsters' side is still the party's.
        const bool charmed_member =
            unit.side == rules::Side::opposition && expected_party.contains(unit.id);
        if (unit.side == rules::Side::opposition && !charmed_member)
        {
            if (unit.id < 1000 || unit.id >= 1000 + staged_records_.size() ||
                !enemies.insert(unit.id).second)
            {
                return false;
            }
            // A monster that surrendered is beaten, and pays as the dead do
            // (postcbt.pas, DetermineLootExp); under the original rules one
            // down may be below 0 Hit Points.
            if (unit.hit_points <= 0 || unit.surrendered)
            {
                ++defeated;
            }
            else if (unit.fled)
            {
                unpaid_records.push_back(staged_records_.at(unit.id - 1000));
            }
        }
        else if (unit.side == rules::Side::party && opengold::is_ally(unit.id))
        {
            continue;
        }
        else if ((unit.side != rules::Side::party && !charmed_member) ||
                 !expected_party.contains(unit.id) || !party_ids.insert(unit.id).second ||
                 campaign_->member(unit.id).vitals != unit.persistent)
        {
            return false;
        }
    }
    // Members who were already dead never joined the fight.
    for (auto id : expected_party)
    {
        if (!party_ids.contains(id) && !campaign_->member(id).vitals.dead)
        {
            return false;
        }
    }
    if (enemies.empty() || (result.outcome == rules::Outcome::victory &&
                            defeated + unpaid_records.size() != enemies.size()))
    {
        return false;
    }
    for (std::size_t n = 0; n < staged_records_.size(); ++n)
    {
        if (!enemies.contains(static_cast<rules::EntityId>(1000 + n)))
        {
            unpaid_records.push_back(staged_records_[n]);
        }
    }
    // A lost duel is not the party's defeat: the champion is left with 1 Hit
    // Point and the script hears 128 (coab ovr006).
    const bool duel_lost = champion && result.outcome == rules::Outcome::defeat;
    if (result.outcome == rules::Outcome::defeat && !duel_lost)
    {
        snapshot_.phase = TourPhase::defeated;
        ++snapshot_.revision;
        return true;
    }
    const bool mirror = std::exchange(mirror_duel_, false);
    const bool first =
        current_area_ == 20 && encounter_records_ == std::vector<unsigned>{13, 4, 4, 4};
    const auto reward = first ? std::string("por:ECL2:20:search1:orcs:v1")
                              : "por:ECL" + std::to_string(area_resources().bank) + ":" +
                                    std::to_string(current_script_) +
                                    ":roaming:" + std::to_string(++next_ticket_);
    const bool fled = result.outcome == rules::Outcome::fled;
    // Members are lost and rewards claimed before the original script hears the
    // result; any failure from here rolls the whole event back rather than
    // leaving those half applied (Effective C++ Item 29).
    try
    {
        // Those who fought for the monsters leave the party first, so they
        // share in nothing it wins.
        const auto turncoat_loot = part_with_turncoats(result);
        defeated += turncoat_loot.defeated;
        if (fled)
        {
            // The party fled (the original's rule): members left on the field are
            // lost for good, and only the monsters it killed are worth experience.
            // The treasure stays with the monsters.
            snapshot_.dialogue = "Your party flees the battle.";
            for (const auto &unit : result.combatants)
            {
                if (unit.side == rules::Side::party && !unit.fled)
                {
                    campaign_->lose(unit.id);
                    snapshot_.dialogue += "\n" + unit.name + " is left behind and lost.";
                }
            }
            unsigned experience = 0;
            for (const auto &unit : result.combatants)
            {
                if (unit.side == rules::Side::opposition && unit.hit_points <= 0 &&
                    staged_records_.at(unit.id - 1000) != mirror_record)
                {
                    experience += record_experience(staged_records_.at(unit.id - 1000));
                }
            }
            if (experience)
            {
                campaign_->award_experience(experience_share(experience), reward);
            }
        }
        else if (duel_lost)
        {
            // No experience, and the opponent keeps its gear.
            campaign_->recover_duelist(champion);
            snapshot_.dialogue = "You have lost the fight.";
        }
        else if (mirror)
        {
            // Beating its own copy earns the duelist 100 experience a level
            // and nothing else (postcbt.pas DetermineLootExp).
            campaign_->award_experience(
                100 * static_cast<unsigned>(campaign_->member(champion).character.sheet().level),
                reward, champion);
        }
        else
        {
            // Experience and loot stay the original encounter's, however many fought,
            // less a record for each monster that got away or was left out.
            auto paying = encounter_records_;
            for (const auto record : unpaid_records)
            {
                if (const auto it = std::find(paying.begin(), paying.end(), record);
                    it != paying.end())
                {
                    paying.erase(it);
                }
            }
            // What the monsters still on the field used of their gear is gone
            // from the loot (DIFF-1).
            std::vector<SpentGear> spent;
            for (const auto &unit : result.combatants)
            {
                if (unit.side == rules::Side::opposition && !unit.fled &&
                    !unit.item_uses_left.empty())
                {
                    SpentGear gear{staged_records_.at(unit.id - 1000), {}};
                    for (const auto &[item, left] : unit.item_uses_left)
                    {
                        gear.uses_left.emplace_back(item - 1, left);
                    }
                    spent.push_back(std::move(gear));
                }
            }
            // Monsters that all got away leave nothing to pay.
            if (!paying.empty())
            {
                unsigned experience = 0;
                for (auto record : paying)
                {
                    experience += record_experience(record);
                }
                auto loot = encounter_loot(current_area_, std::move(paying), reward + ":loot",
                                           machine_.variable(game_cell(EclAddress{0x6DE3})) != 1,
                                           std::move(spent));
                experience += treasure_experience(loot);
                campaign_->award_experience(champion ? experience : experience_share(experience),
                                            reward, champion);
                pending_loot_.push_back(std::move(loot));
                pile_loot();
            }
            // Treasure the script added to the fight is won with it.
            if (staged_treasure_)
            {
                award_staged_treasure();
            }
            // A beaten turncoat's gear is the party's, and its record's
            // experience too.
            if (turncoat_loot.experience)
            {
                campaign_->award_experience(turncoat_loot.experience, reward + ":turncoats");
            }
            for (const auto &item : turncoat_loot.items)
            {
                treasure_pile_.push_back(item);
            }
        }
        // The original's combat results (coab ovr006): 0 won, 128 (0x80) a
        // duel lost, 129 (0x81) the party fled; scripts test "above 128" for a
        // flight.
        std::vector<EclMemoryWrite> results;
        for (auto write : std::array<EclMemoryWrite, 8>{
                 {{game_cell(ecl_combat_result), static_cast<std::uint16_t>(fled        ? 129
                                                                            : duel_lost ? 128
                                                                                        : 0)},
                  {game_cell(ecl_monsters_defeated), static_cast<std::uint16_t>(defeated)},
                  {game_cell(ecl_encounter_surprise), 0},
                  {game_cell(EclAddress{0x6DE3}), 0},
                  {game_cell(EclAddress{0x6E70}), 0},
                  {game_cell(EclAddress{0x6E71}), 0},
                  {game_cell(EclAddress{0x6E72}), 0},
                  // Whether a member attacked its own side (Attack Ally),
                  // which the nomads hold against the party (ECL7:17 0x9ABA).
                  {game_cell(ecl_allies_attacked),
                   static_cast<std::uint16_t>(result.allies_attacked)}}})
        {
            results.push_back(write);
        }
        // The items the monsters leave wait for the party to take them; the
        // script hears the result once the party leaves the treasure.
        if (!treasure_pile_.empty())
        {
            open_treasure(combat_request_, std::move(results));
        }
        else
        {
            auto reply = character_reply(selected_character_);
            reply.writes.insert(reply.writes.end(), results.begin(), results.end());
            if (!machine_.resume_host(combat_request_, reply))
            {
                throw EclError("Combat result rejected by original script");
            }
        }
    }
    catch (const std::exception &error)
    {
        fail(error.what());
        return true;
    }
    combat_request_ = 0;
    encounter_.reset();
    monster_picture_id_.reset();
    staged_enemies_.clear();
    staged_art_.clear();
    staged_records_.clear();
    staged_allies_.clear();
    staged_ally_art_.clear();
    // A later script fault must never restore pre-combat HP or erase a victory.
    event_checkpoint_.reset();
    if (!pending_loot_.empty())
    {
        snapshot_.dialogue += "\nLoot is retained until your party has room in its purses.";
    }
    if (snapshot_.phase == TourPhase::treasure)
    {
        return true;
    }
    snapshot_.phase = TourPhase::running;
    ++snapshot_.revision;
    advance(0);
    return true;
}

namespace
{
// A creature's gear by MONnITM place. With an export, an original item the
// Combat Sprite Catalog left out is not carried (#274).
// Gear the catalog added to a creature (#274), each as a plain item of its
// original type from New Phlan's shop lists. A type no list sells is left out.
std::vector<Equipment> added_gear(const ExportedMonster *exported, const PhlanResources &town)
{
    std::vector<Equipment> gear;
    for (const auto &added : exported ? exported->equipment : std::vector<MonsterItem>{})
    {
        if (added.source_index >= 0 || added.source_type < 0)
        {
            continue;
        }
        const auto plain = [&]() -> const Equipment *
        {
            for (const auto &[record, list] : town.treasure)
            {
                for (const auto &item : list)
                {
                    if (item.stored.type == added.source_type && !item.stored.magic_bonus &&
                        !item.stored.cursed_raw)
                    {
                        return &item;
                    }
                }
            }
            return nullptr;
        }();
        if (plain)
        {
            gear.push_back(*plain);
        }
    }
    return gear;
}

// The item key of the item a creature fights with: its original item, or a
// plain item of the type the catalog added. Empty for none.
} // namespace

PendingLoot RolfTourSession::encounter_loot(unsigned area, std::vector<unsigned> records,
                                            std::string reward, bool items,
                                            std::vector<SpentGear> spent) const
{
    // New Phlan's own fights (area 0) are the town's; the rest a district's.
    const auto district = town_->districts.find(area);
    const PhlanResources *resources =
        area == 0 ? town_.get()
                  : (district == town_->districts.end() ? nullptr : district->second.get());
    if (!resources || records.empty() || records.size() > 56 || reward.empty() ||
        reward.size() > 160 ||
        std::any_of(records.begin(), records.end(),
                    [&](auto id)
                    {
                        return !resources->encounter_creatures.contains(id);
                    }))
    {
        throw EclError("Invalid pending encounter loot");
    }
    PendingLoot loot;
    loot.reward_id = std::move(reward);
    loot.records = std::move(records);
    loot.include_items = items;
    loot.area = area;
    for (auto id : loot.records)
    {
        const auto &creature = resources->encounter_creatures.at(id);
        for (unsigned coin = 0; coin < 7; ++coin)
        {
            loot.wealth[coin] += creature.stored.wealth[coin];
        }
        if (!items)
        {
            continue;
        }
        auto gear = carried_gear(creature, exported_creature(*resources, id));
        // One monster of this record used its gear in the fight (DIFF-1).
        if (const auto used = std::find_if(spent.begin(), spent.end(),
                                           [&](const auto &s)
                                           {
                                               return s.record == id;
                                           });
            used != spent.end())
        {
            for (const auto &[index, left] : used->uses_left)
            {
                if (index >= gear.size() || !gear[index])
                {
                    throw EclError("Invalid spent monster gear");
                }
                gear[index] = item_after_uses(std::move(*gear[index]), left);
            }
            spent.erase(used);
        }
        for (auto &item : gear)
        {
            // There is no ammunition: a monster's arrows are not left behind.
            if (item && !opengold::original_item::is_ammunition(item->stored.type))
            {
                loot.items.push_back(std::move(*item));
            }
        }
        for (auto &item : added_gear(exported_creature(*resources, id), *town_))
        {
            item.index = loot.items.size();
            loot.items.push_back(std::move(item));
        }
    }
    return loot;
}

void RolfTourSession::finish_event()
{
    read_character();
    // Another script may follow in this event; it must see the settled purse.
    for (const auto &w : character_reply(selected_character_).writes)
    {
        machine_.bind_variable(w.address, w.value);
    }
    publish_pose();
    if (transition_)
    {
        transition_ = false;
        event_stage_ = EventStage::area_entry;
        if (!machine_.start(4))
        {
            throw EclError("Cannot enter town building script");
        }
        return;
    }
    if (event_stage_ == EventStage::step && overland())
    {
        overland_step();
        search_destination();
        return;
    }
    if (event_stage_ == EventStage::step)
    {
        if (!move_party(*pending_movement_))
        {
            if (locked_door_ahead())
            {
                show_locked_door();
                return;
            }
            snapshot_.dialogue = "The way is blocked.";
        }
        search_destination();
        return;
    }
    if (event_stage_ == EventStage::area_entry)
    {
        if (teleport_arrival_)
        {
            arrive(*std::exchange(teleport_arrival_, std::nullopt));
        }
        update_travel_mode();
        event_stage_ = EventStage::search;
        if (!machine_.start(1))
        {
            throw EclError("Cannot search building entrance");
        }
        return;
    }
    if (event_stage_ == EventStage::pre_camp)
    {
        if (!campaign_)
        {
            throw EclError("Rest requires campaign rules");
        }
        const auto interval = machine_.variable(game_cell(ecl_rest_check_interval)),
                   chance = machine_.variable(game_cell(ecl_rest_interruption_chance));
        const auto add_camp_line = [this](std::string_view line)
        {
            if (!snapshot_.dialogue.empty())
            {
                snapshot_.dialogue += '\n';
            }
            snapshot_.dialogue += line;
            log_diagnostic(LogPriority::info, "camp", line);
        };
        if (chance == 255)
        {
            add_camp_line("Rest is not allowed here.");
        }
        else
        {
            // An interrupted rest grants nothing and leaves nothing to resume;
            // the party rests again.
            if (const auto rested = rest_interruption(interval, chance))
            {
                log_diagnostic(LogPriority::info, "camp",
                               "interrupted after " + std::to_string(*rested) + " minutes");
                campaign_->advance_time(std::chrono::minutes{*rested});
                synchronize_clock();
                event_stage_ = EventStage::camp_interrupted;
                if (!machine_.start(3))
                {
                    throw EclError("Cannot enter camp interruption script");
                }
                return;
            }
            if (campaign_->rest(camp_kind_))
            {
                add_camp_line(
                    camp_kind_ == RestKind::short_rest
                        ? "Short rest complete: one hour passed; eligible members can spend Hit Dice."
                        : "Long rest complete: eight hours passed; eligible members recovered HP and supported resources.");
            }
            else
            {
                add_camp_line("Rest denied: no active member is eligible.");
            }
            for (const auto &w : character_reply(selected_character_).writes)
            {
                machine_.bind_variable(w.address, w.value);
            }
            synchronize_clock();
        }
    }
    // A party the script moved away from a meeting (Flee, or Run after a
    // parley) got away, unless it was robbed and sent off.
    if (meeting_pose_ && encounter_outcome_.empty() && !robbed_ &&
        (meeting_pose_->x != snapshot_.pose.x || meeting_pose_->y != snapshot_.pose.y))
    {
        encounter_outcome_ = "You get away.";
    }
    if (!encounter_outcome_.empty())
    {
        snapshot_.dialogue += (snapshot_.dialogue.empty() ? "" : "\n") + encounter_outcome_;
    }
    encounter_outcome_.clear();
    meeting_pose_.reset();
    robbed_ = false;
    if (event_stage_ == EventStage::camp_interrupted && rest_warded_)
    {
        rest_warded_ = false;
        snapshot_.dialogue += (snapshot_.dialogue.empty() ? "" : "\n") +
                              std::string("The Magic Circle keeps the creatures away.");
        if (campaign_->rest(camp_kind_))
        {
            snapshot_.dialogue +=
                camp_kind_ == RestKind::short_rest
                    ? "\nShort rest complete: one hour passed; eligible members can spend Hit Dice."
                    : "\nLong rest complete: eight hours passed; eligible members recovered HP and supported resources.";
        }
        synchronize_clock();
    }
    else if (event_stage_ == EventStage::camp_interrupted)
    {
        snapshot_.dialogue += (snapshot_.dialogue.empty() ? "" : "\n") +
                              std::string("The rest was interrupted. Rest again to recover.");
    }
    // A clock the event's script set is the campaign's before anything saves.
    synchronize_clock();
    event_checkpoint_.reset();
    if (std::exchange(step_out_after_shop_, false))
    {
        step_out_of_shop();
    }
    snapshot_.phase = TourPhase::completed;
    snapshot_.choices.clear();
    snapshot_.continue_ticket = 0;
    ++snapshot_.revision;
    if (event_stage_ == EventStage::search && !overland())
    {
        search_for_secret_door();
        open_own_shop();
    }
}

// After a script's entry, the original travels the wilderness map when the
// script is a wilderness's (25 west, 26 central, 27 east) and left its 3D-area
// cell clear; otherwise it is in a 3D area.
void RolfTourSession::update_travel_mode()
{
    const bool wilderness = current_script_ >= 25 && current_script_ <= 27;
    if (wilderness && machine_.variable(game_cell(ecl_in_3d_area)) == 0)
    {
        if (!town_->overland)
        {
            throw EclError("The wilderness map needs START.EXE");
        }
        travel_mode_ = current_script_ - 23;
    }
    else
    {
        travel_mode_ = 1;
    }
    publish_pose();
}

// The original's StepOverland: a step in the party's direction, kept on the
// window, unless the square's terrain is impassable or a poisoned river
// before the Pyramid's sorcerer falls; it takes half a day.
bool RolfTourSession::overland_step()
{
    if (machine_.variable(game_cell(EclAddress{0x6DC9})) == 255)
    {
        return false;
    }
    const auto &world = *town_->overland;
    const auto direction = machine_.variable(game_cell(ecl_overland_direction)) % 8;
    const auto x = static_cast<unsigned>(std::clamp<int>(
                   machine_.variable(game_cell(ecl_overland_x)) + overland_dx[direction], 0,
                   OverlandWorld::window_columns - 1)),
               y = static_cast<unsigned>(std::clamp<int>(
                   machine_.variable(game_cell(ecl_overland_y)) + overland_dy[direction], 0,
                   OverlandWorld::rows - 1));
    switch (overland_passage(world.at(world.window_start[travel_mode_ - 2] + x, y)))
    {
    case OverlandPassage::closed:
        return false;
    case OverlandPassage::river:
        if (machine_.variable(game_cell(pyramid_valley_healed)) == 0)
        {
            snapshot_.dialogue = "The river's stench drives you back.";
            return false;
        }
        break;
    case OverlandPassage::open:
        break;
    }
    machine_.bind_variable(game_cell(ecl_overland_x), static_cast<std::uint16_t>(x));
    machine_.bind_variable(game_cell(ecl_overland_y), static_cast<std::uint16_t>(y));
    if (campaign_)
    {
        pass_time(std::chrono::hours{12});
    }
    synchronize_clock();
    publish_pose();
    return true;
}

// The original engine rests in five-minute steps. Each step counts toward the
// area's check interval (0x6DD2); on reaching it, the count restarts and d100 at
// or below the chance (0x6DD3) interrupts. The count carries over between rests.
// Returns the minutes rested before an interruption, or nothing when none occurs.
std::optional<unsigned> RolfTourSession::rest_interruption(unsigned interval, unsigned chance)
{
    if (!interval)
    {
        return std::nullopt;
    }
    const auto &rules = campaign_->rule_module();
    const auto policy =
        camp_kind_ == RestKind::short_rest ? rules.short_rest_policy() : rules.long_rest_policy();
    constexpr unsigned step_minutes = 5;
    for (unsigned rested = step_minutes; rested <= policy.duration_minutes; rested += step_minutes)
    {
        if (++rest_checks_ < interval)
        {
            continue;
        }
        rest_checks_ = 0;
        if (machine_.engine_random(100) + 1 <= chance)
        {
            return rested;
        }
    }
    return std::nullopt;
}

// As in the original, the party's cell is searched even when the step failed.
void RolfTourSession::search_destination()
{
    pending_movement_.reset();
    event_stage_ = EventStage::search;
    if (!machine_.start(1))
    {
        throw EclError("Cannot search destination");
    }
}

bool RolfTourSession::locked_door_ahead() const
{
    if (!campaign_ || !pending_movement_ || machine_.variable(game_cell(EclAddress{0x6DC9})) == 255)
    {
        return false;
    }
    const auto edge = edge_ahead(snapshot_.pose);
    const auto &from = map_.at(edge.x, edge.y);
    const auto &to = map_.at(edge.next_x, edge.next_y);
    const auto passable_or_locked = [](unsigned wall, std::uint8_t door)
    {
        return !wall || door == 1 || lock(door);
    };
    return (lock(from.doors[index(edge.side)]) || lock(to.doors[index(edge.other)])) &&
           passable_or_locked(from.walls[index(edge.side)], from.doors[index(edge.side)]) &&
           passable_or_locked(to.walls[index(edge.other)], to.doors[index(edge.other)]);
}

bool RolfTourSession::wizard_locked_ahead() const
{
    const auto edge = edge_ahead(snapshot_.pose);
    return map_.at(edge.x, edge.y).doors[index(edge.side)] == wizard_locked_door ||
           map_.at(edge.next_x, edge.next_y).doors[index(edge.other)] == wizard_locked_door;
}

void RolfTourSession::show_locked_door()
{
    door_menu_ = true;
    door_choices_.clear();
    snapshot_.choices.clear();
    // A hidden entrance's lock is picked or opened by Knock (SECRET-1).
    if (!entrance_ahead())
    {
        door_choices_.push_back(DoorMethod::bash);
        snapshot_.choices.push_back("Bash");
    }
    if (!pick_tried_ && campaign_->can_try_door(DoorMethod::pick))
    {
        door_choices_.push_back(DoorMethod::pick);
        snapshot_.choices.push_back("Pick");
    }
    if (campaign_->can_try_door(DoorMethod::knock))
    {
        door_choices_.push_back(DoorMethod::knock);
        snapshot_.choices.push_back("Knock");
    }
    snapshot_.choices.push_back("Exit");
    snapshot_.dialogue = wizard_locked_ahead() ? "Wizard locked." : "Locked.";
    snapshot_.phase = TourPhase::awaiting_continue;
    snapshot_.continue_ticket = ++next_ticket_;
    ++snapshot_.revision;
}

void RolfTourSession::try_locked_door(DoorMethod method)
{
    if (method == DoorMethod::pick)
    {
        pick_tried_ = true;
    }
    const int difficulty =
        wizard_locked_ahead() ? wizard_locked_difficulty : locked_door_difficulty;
    const auto attempts = campaign_->try_door(method, difficulty);
    for (const auto &attempt : attempts)
    {
        snapshot_.door_checks.push_back({campaign_->member(attempt.member).character.sheet().name,
                                         method, attempt.roll.die, attempt.roll.total,
                                         attempt.difficulty, attempt.thieves_tools,
                                         attempt.magic_lock_picks});
    }
    if (attempts.empty() || attempts.back().roll.total < attempts.back().difficulty)
    {
        return;
    }
    snapshot_.door_opened = true;
    // A hidden entrance stays open for good (SECRET-1).
    if (const auto *shop = entrance_ahead())
    {
        entrances_[shop->id] = EntranceState::open;
    }
    // The original unlocks both faces in its loaded map; reloading the district relocks them.
    const auto edge = edge_ahead(snapshot_.pose);
    const auto unlock = [](std::uint8_t &door)
    {
        if (lock(door))
        {
            door = 1;
        }
    };
    unlock(map_.cells[edge.y * 16 + edge.x].doors[index(edge.side)]);
    unlock(map_.cells[edge.next_y * 16 + edge.next_x].doors[index(edge.other)]);
    if (!move_party(*pending_movement_))
    {
        throw EclError("The opened door is still blocked");
    }
}

std::string RolfTourSession::treasure_identity(std::uint16_t address) const
{
    // A TREASURE instruction is identified by its script and original address.
    constexpr char digits[] = "0123456789abcdef";
    std::string hex;
    for (int shift = 12; shift >= 0; shift -= 4)
    {
        hex += digits[(address >> shift) & 15];
    }
    const std::string archive = "ECL" + std::to_string(current_area_ ? area_resources().bank : 3);
    return "por:" + archive + ":" + std::to_string(current_script_) + ":treasure:" + hex + ":v1";
}

void RolfTourSession::stage_treasure(const EclRequest &request)
{
    // Operands: copper, silver, electrum, gold, platinum, gems, jewelry, then an
    // item code: an ITEMn list below 128, 128 + n random items, or 255 for none.
    const auto items = request.arguments.at(7).number();
    if (!campaign_)
    {
        throw EclError("Treasure awards require a campaign party");
    }
    PendingLoot loot;
    for (unsigned coin = 0; coin < 7; ++coin)
    {
        loot.wealth[coin] = request.arguments.at(coin).number();
    }
    loot.reward_id = treasure_identity(request.instruction->address);
    // An instruction whose list comes from a script cell gives each list once:
    // the Temple of Bane's three trapdoors share one (ECL1:24 0xAA34).
    if (request.instruction->operands.at(7).tag != 0)
    {
        loot.reward_id += ":list:" + std::to_string(items);
    }
    loot.include_items = false;
    if (items < 128)
    {
        // A fixed list from the area's item archive.
        const auto &lists = area_resources().treasure;
        const auto found = lists.find(items);
        if (found == lists.end())
        {
            throw EclError("Unknown treasure list " + std::to_string(items));
        }
        for (auto item : found->second)
        {
            // There is no ammunition, so a list's arrows and quarrels are not found.
            if (opengold::original_item::is_ammunition(item.stored.type))
            {
                continue;
            }
            item.index = loot.items.size();
            loot.items.push_back(std::move(item));
        }
    }
    else if (items != 255)
    {
        // Rolled on the script's own saved random stream, as the original did here.
        const auto roll = [&](unsigned sides)
        {
            return machine_.host_random(request.id, sides) + 1;
        };
        for (auto &record : random_treasure_items(items - 128, roll))
        {
            Equipment item;
            item.index = loot.items.size();
            const auto &templates = town_->item_templates;
            if (!templates.empty())
            {
                if (record.type >= templates.size())
                {
                    throw EclError("Missing item template for generated treasure");
                }
                item.base = templates[record.type];
            }
            item.bonuses = equipment_bonuses(record, item.base);
            item.stored = std::move(record);
            loot.items.push_back(std::move(item));
        }
    }
    staged_treasure_ = std::move(loot);
}

void RolfTourSession::award_staged_treasure()
{
    auto loot = std::move(*staged_treasure_);
    staged_treasure_.reset();
    const auto &claimed = campaign_->state().claimed_rewards;
    const auto awarded = [&](const std::string &id)
    {
        return std::find(claimed.begin(), claimed.end(), id) != claimed.end() ||
               std::any_of(pending_loot_.begin(), pending_loot_.end(),
                           [&](const auto &pending)
                           {
                               return pending.reward_id == id;
                           });
    };
    // The script decides whether a treasure repeats, as the original's flags
    // did: City Hall pays every reward through one instruction. Each award
    // from an instruction already used gets an identity of its own.
    const auto first_award = loot.reward_id;
    for (unsigned award = 2; awarded(loot.reward_id); ++award)
    {
        loot.reward_id = first_award + ":award:" + std::to_string(award);
    }
    pending_loot_.push_back(std::move(loot));
    pile_loot();
    if (!pending_loot_.empty())
    {
        snapshot_.dialogue += "\nLoot is retained until your party has room in its purses.";
    }
}

EncounterTurn encounter_turn(std::size_t choice, unsigned response, unsigned distance,
                             bool party_escapes, bool monsters_outrun)
{
    constexpr std::size_t fight = 0, wait = 1, flee = 2, advance = 3, parley = 4;
    // Adjacent monsters answer with `adjacent`; others come a step closer.
    const auto close_in = [&](std::optional<unsigned> adjacent)
    {
        return distance == 0 ? EncounterTurn{adjacent, false} : EncounterTurn{std::nullopt, true};
    };
    const auto fight_or_flight = [&]
    {
        return EncounterTurn{choice == flee ? 2U : 1U, false};
    };
    switch (response)
    {
    case 0:
        // Only this answer tests the party's speed.
        return {choice == flee && party_escapes ? 2U : 1U, false};
    case 1:
        if (choice == wait)
        {
            return {};
        }
        if (choice == advance || choice == parley)
        {
            return close_in(choice == parley ? std::optional<unsigned>{3U} : std::nullopt);
        }
        return fight_or_flight();
    case 2:
        // The monsters leave, unless the party attacks monsters it can outrun.
        return {choice == fight && !monsters_outrun ? 1U : 0U, false};
    case 3:
        if (choice == wait || choice == advance)
        {
            return close_in(std::nullopt);
        }
        if (choice == parley)
        {
            return close_in(3U);
        }
        return fight_or_flight();
    case 4:
        if (choice == fight || choice == flee)
        {
            return fight_or_flight();
        }
        return close_in(3U);
    default:
        throw EclError("Invalid encounter response");
    }
}

void RolfTourSession::show_encounter_menu()
{
    // The first operand names the distance sprite, the third the close-up
    // picture (coab CMD_EncounterMenu); Sokal Keep's Ferran Martinez has a
    // picture of his own (19) and the Spectre's sprite (17).
    // A sprite with fewer distance frames (the western wilderness's centaur
    // has two) shows its farthest one beyond them.
    const auto &args = encounter_menu_->arguments;
    for (unsigned frame = 0; frame < 3; ++frame)
    {
        auto image = decode_ega_sprite(area_resources().sprite_archive,
                                       static_cast<std::uint8_t>(args[0].number()),
                                       static_cast<std::uint8_t>(frame));
        if (!image && frame > 0)
        {
            sprites_[frame] = sprites_[frame - 1];
            continue;
        }
        if (!image)
        {
            throw EclError("Unsupported roaming encounter sprite");
        }
        sprites_[frame] = std::move(image.image);
    }
    snapshot_.sprite_id = args[0].number();
    snapshot_.sprite_frame = encounter_distance_;
    monster_picture_id_ = args[2].number();
    snapshot_.dialogue = join_script_text({}, args[9 + encounter_distance_].text());
    if (snapshot_.dialogue.empty())
    {
        snapshot_.dialogue = join_script_text({}, args[9].text());
    }
    announce_camp_attack();
    if (!meeting_pose_)
    {
        meeting_pose_ = snapshot_.pose;
    }
    // As in the original, Parley replaces Advance once the monsters are
    // adjacent, and on the wilderness map, where no one advances.
    snapshot_.choices = {"Fight", "Wait", "Flee",
                         encounter_distance_ && !overland() ? "Advance" : "Parley"};
    snapshot_.phase = TourPhase::awaiting_continue;
    snapshot_.continue_ticket = ++next_ticket_;
    ++snapshot_.revision;
}

std::vector<unsigned> RolfTourSession::living_member_levels() const
{
    std::vector<unsigned> levels;
    for (const auto id : campaign_->state().slots)
    {
        if (id && !campaign_->member(id).vitals.dead)
        {
            levels.push_back(campaign_->member(id).character.sheet().level);
        }
    }
    return levels;
}

// Shrinks the staged original encounter to the party's SRD XP budget at the
// encounter challenge and to one creature per living character, the size the
// party can face without being swarmed. Each LOAD MONSTER group keeps its first.
// Under the original rules the whole encounter fights, as in the original.
void RolfTourSession::fit_staged_encounter()
{
    staged_band_.clear();
    std::vector<unsigned> counts(staged_records_.size(),
                                 static_cast<unsigned>(staged_records_.size()));
    if (!plays_original_records())
    {
        const auto groups = staged_groups(staged_records_, area_resources().conversions);
        counts = fit_encounter_to_difficulty(groups, living_member_levels(), encounter_challenge_,
                                             difficulty_);
    }
    for (std::size_t n = 0; n < staged_records_.size() && !plays_original_records(); ++n)
    {
        const auto record = staged_records_[n];
        const auto &creature = area_resources().encounter_creatures.at(record);
        const auto *exported = exported_creature(area_resources(), record);
        auto member = staged_enemies_[n];
        member.name = exported ? exported->name : creature.stored.name;
        member.inventory = fighting_gear(creature, exported);
        staged_band_.push_back(
            {std::move(member), record, area_resources().conversions.at(record).fit_xp});
    }
    std::vector<rules::Participant> enemies;
    std::vector<opengold::CombatArt> art;
    std::vector<unsigned> records;
    for (std::size_t n = 0, group = 0, kept = 0; n < staged_records_.size(); ++n)
    {
        if (n && staged_records_[n] != staged_records_[n - 1])
        {
            ++group;
            kept = 0;
        }
        if (kept++ >= counts[group])
        {
            continue;
        }
        const auto id = static_cast<rules::EntityId>(1000 + enemies.size());
        const auto &creature = area_resources().encounter_creatures.at(staged_records_[n]);
        enemies.push_back(std::move(staged_enemies_[n]));
        enemies.back().id = id;
        const auto *exported = exported_creature(area_resources(), staged_records_[n]);
        enemies.back().name = (exported ? exported->name : creature.stored.name) + " " +
                              std::to_string(enemies.size());
        art.push_back(std::move(staged_art_[n]));
        art.back().entity = id;
        records.push_back(staged_records_[n]);
    }
    staged_enemies_ = std::move(enemies);
    staged_art_ = std::move(art);
    staged_records_ = std::move(records);
}

bool RolfTourSession::camp_warded() const
{
    if (!campaign_)
    {
        return false;
    }
    const auto &state = campaign_->state();
    std::vector<rules::CharacterSheet> sheets;
    std::vector<rules::VitalState> vitals;
    for (const auto id : state.slots)
    {
        if (id)
        {
            const auto &member = campaign_->member(id);
            sheets.push_back(member.character.sheet());
            vitals.push_back(member.vitals);
        }
    }
    std::vector<rules::CampTarget> party;
    for (std::size_t n = 0; n < sheets.size(); ++n)
    {
        party.push_back({sheets[n], vitals[n]});
    }
    std::vector<std::string> definitions;
    for (const auto &enemy : staged_enemies_)
    {
        definitions.push_back(enemy.definition);
    }
    return campaign_->rule_module().keeps_away(party, definitions);
}

// Monsters that interrupt a camp are announced ahead of the script's own text.
void RolfTourSession::announce_camp_attack()
{
    constexpr std::string_view attack = "Your camp is attacked!";
    if (event_stage_ == EventStage::camp_interrupted && !snapshot_.dialogue.starts_with(attack))
    {
        snapshot_.dialogue = std::string(attack) + "\n" + snapshot_.dialogue;
    }
}

// Names who parleys: the spokesman whose reaction score the event started with.
void RolfTourSession::announce_spokesman()
{
    if (!campaign_)
    {
        return;
    }
    if (const auto speaker = campaign_->spokesman())
    {
        snapshot_.dialogue +=
            "\n" + campaign_->member(speaker).character.sheet().name + " speaks for the party.";
    }
}

bool RolfTourSession::choose_encounter(std::size_t choice)
{
    if (!encounter_menu_ || choice > 3)
    {
        return false;
    }
    // The fourth choice is Advance at a distance and Parley when adjacent or
    // on the wilderness map.
    if (choice == 3 && (encounter_distance_ == 0 || overland()))
    {
        choice = 4;
    }
    const auto &request = *encounter_menu_;
    const auto &args = request.arguments;
    const auto response = args[4 + choice].number();
    int slowest = 1000, fastest = 0;
    for (auto id : campaign_->state().slots)
    {
        // The party carries its dead with it, as in the original, so they never slow it.
        if (id && !campaign_->member(id).vitals.dead)
        {
            // Conversion boundary: original movement 12 corresponds to a modern
            // ordinary 30-foot creature. Unconscious members cannot escape on foot.
            const int movement = campaign_->member(id).vitals.hit_points <= 0
                                     ? 0
                                     : campaign_->profile(id).movement_feet * 2 / 5;
            slowest = std::min(slowest, movement);
            fastest = std::max(fastest, movement);
        }
    }
    // On Easy a flight always succeeds (DIFF-2).
    const auto turn = encounter_turn(choice, response, encounter_distance_,
                                     slowest >= static_cast<int>(args[12].number()) ||
                                         spares_the_party(difficulty_),
                                     static_cast<int>(args[13].number()) > fastest);
    if (!turn.result)
    {
        // The meeting goes on: the monsters came a step closer, or both sides
        // waited.
        encounter_distance_ -= turn.closer ? 1 : 0;
        show_encounter_menu();
        return true;
    }
    const auto result = turn.result;
    EclHostReply reply;
    reply.writes = {{args[3].address(), static_cast<std::uint16_t>(*result)}};
    if (!machine_.resume_host(request.id, reply))
    {
        return false;
    }
    // The script ends the meeting quietly (0); the event's end says so.
    encounter_outcome_ = *result == 0 ? "The monsters go on their way." : "";
    encounter_menu_.reset();
    snapshot_.choices.clear();
    snapshot_.continue_ticket = 0;
    snapshot_.phase = TourPhase::running;
    ++snapshot_.revision;
    return true;
}

void RolfTourSession::notice(std::string message)
{
    message_only_ = true;
    snapshot_.dialogue = std::move(message);
    snapshot_.phase = TourPhase::awaiting_continue;
    snapshot_.choices = {"Continue"};
    snapshot_.continue_ticket = ++next_ticket_;
    ++snapshot_.revision;
}

bool RolfTourSession::choose(std::uint64_t ticket, std::size_t choice)
{
    if (snapshot_.phase != TourPhase::awaiting_continue || !ticket ||
        ticket != snapshot_.continue_ticket || choice >= snapshot_.choices.size())
    {
        return false;
    }
    log_player("chooses \"" + snapshot_.choices[choice] + "\"");
    if (hall_visit_)
    {
        return choose_at_hall(choice);
    }
    if (temple_visit_)
    {
        return choose_at_temple(choice);
    }
    if (damage_request_)
    {
        auto reply = character_reply(selected_character_);
        if (std::exchange(victory_, false))
        {
            reply.writes.push_back({game_cell(EclAddress{0x4AFD}), 255});
        }
        if (!machine_.resume_host(damage_request_, reply))
        {
            return false;
        }
        damage_request_ = 0;
        snapshot_.phase = TourPhase::running;
    }
    else if (encounter_menu_)
    {
        return choose_encounter(choice);
    }
    else if (who_request_)
    {
        const auto slot = who_slots_.at(choice);
        auto reply = character_reply(slot);
        if (!machine_.resume_host(who_request_, reply))
        {
            return false;
        }
        campaign_->select(slot);
        selected_character_ = slot;
        member_chosen_ = true;
        who_request_ = 0;
        who_slots_.clear();
        snapshot_.choosing_member = false;
        snapshot_.phase = TourPhase::running;
    }
    else if (door_menu_)
    {
        door_menu_ = false;
        try
        {
            if (choice < door_choices_.size())
            {
                try_locked_door(door_choices_[choice]);
            }
            search_destination();
        }
        catch (const std::exception &e)
        {
            fail(e.what());
            return true;
        }
        snapshot_.phase = TourPhase::running;
    }
    else if (message_only_)
    {
        message_only_ = false;
        snapshot_.phase = TourPhase::completed;
    }
    else
    {
        if (!machine_.resume(menu_request_, choice))
        {
            return false;
        }
        snapshot_.phase = TourPhase::running;
    }
    snapshot_.choices.clear();
    snapshot_.continue_ticket = 0;
    ++snapshot_.revision;
    return true;
}

bool RolfTourSession::input(std::uint64_t ticket, std::string_view value)
{
    if (snapshot_.phase != TourPhase::awaiting_input || !ticket ||
        ticket != snapshot_.continue_ticket)
    {
        return false;
    }
    log_player("answers \"" + std::string(value) + "\"");
    if (!machine_.resume_input(menu_request_, value))
    {
        snapshot_.diagnostic = snapshot_.number_input ? "Enter a whole number from 0 to 65535."
                                                      : "Enter up to 40 characters.";
        ++snapshot_.revision;
        return false;
    }
    snapshot_.diagnostic.clear();
    snapshot_.phase = TourPhase::running;
    snapshot_.continue_ticket = 0;
    ++snapshot_.revision;
    return true;
}

bool RolfTourSession::buy(std::uint64_t ticket, std::size_t item)
{
    if (snapshot_.phase != TourPhase::shopping || !ticket || ticket != snapshot_.continue_ticket ||
        item >= treasure_.size())
    {
        return false;
    }
    const auto &offered = treasure_[item];
    log_player("buys " + offered.label() + " (item " + std::to_string(item + 1) + " of " +
               std::to_string(treasure_.size()) + ")");
    if (campaign_)
    {
        snapshot_.gear_changes.clear();
        const auto before = held_items();
        try
        {
            campaign_->purchase(campaign_->selected(), offered);
            snapshot_.diagnostic = "Bought " + offered.label();
        }
        catch (const std::exception &e)
        {
            snapshot_.diagnostic = e.what();
            ++snapshot_.revision;
            return false;
        }
        equip_gained(before);
        ++snapshot_.revision;
        return true;
    }
    const auto price = offered.stored.value;
    if (party_.inventory.size() >= 16 || gold_value(party_.wealth) < price)
    {
        snapshot_.diagnostic =
            party_.inventory.size() >= 16 ? "Inventory is full (16 items)." : "Not enough gold.";
        ++snapshot_.revision;
        return false;
    }
    auto purchased = offered;
    purchased.index = party_.inventory.size();
    purchased.stored.readied_raw = 0;
    party_.inventory.push_back(std::move(purchased)); // Allocate before charging.
    party_.wealth = pay_gold(party_.wealth, price).purse;
    snapshot_.diagnostic = "Bought " + offered.label() + " for " + std::to_string(price) + " gp.";
    ++snapshot_.revision;
    return true;
}

RolfTourSession::HeldItems RolfTourSession::held_items() const
{
    HeldItems held;
    for (const auto &member : campaign_->state().roster)
    {
        for (const auto &item : member.character.inventory().items())
        {
            held.emplace_back(member.id, item.id);
        }
    }
    return held;
}

void RolfTourSession::equip_gained(const HeldItems &before)
{
    if (!auto_equip_)
    {
        return;
    }
    for (const auto &[member, item] : held_items())
    {
        if (std::find(before.begin(), before.end(), std::pair{member, item}) != before.end())
        {
            continue;
        }
        if (auto change = campaign_->auto_equip(member, item, *auto_equip_))
        {
            if (diagnostic_log_enabled())
            {
                std::string line = change->name;
                for (const auto &name : change->equipped)
                {
                    line += " equips " + name + ";";
                }
                for (const auto &name : change->unequipped)
                {
                    line += " packs " + name + ";";
                }
                if (!change->burned_by.empty())
                {
                    line += " is burned by " + change->burned_by + " for " +
                            std::to_string(change->burn_damage);
                }
                log_diagnostic(LogPriority::info, "auto-equip", line);
            }
            snapshot_.gear_changes.push_back(std::move(*change));
        }
    }
}

bool RolfTourSession::leave_shop(std::uint64_t ticket)
{
    if (snapshot_.phase != TourPhase::shopping || !ticket || ticket != snapshot_.continue_ticket)
    {
        return false;
    }
    log_player("leaves the shop");
    if (!shop_request_)
    {
        // OpenGoldBox's own shop: no script waits on it.
        step_out_of_shop();
        snapshot_.continue_ticket = 0;
        snapshot_.phase = TourPhase::completed;
        ++snapshot_.revision;
        return true;
    }
    const auto selected = campaign_ ? campaign_->state().selected_slot : PartySlot{};
    auto reply = character_reply(selected);
    reply.writes.push_back({game_cell(ecl_shop_service), 0});
    if (!machine_.resume_host(shop_request_, reply))
    {
        return false;
    }
    selected_character_ = selected;
    shop_request_ = 0;
    step_out_after_shop_ = true;
    snapshot_.continue_ticket = 0;
    snapshot_.phase = TourPhase::running;
    ++snapshot_.revision;
    return true;
}

void RolfTourSession::step_out_of_shop()
{
    const auto shop = snapshot_.pose;
    // The square the party last stepped from (0x49F0, 0x49F1), when it is
    // beside the shop; otherwise the square behind the party, which came in
    // facing forward.
    const PartyPose from{machine_.variable(game_cell(EclAddress{0x49F0})),
                         machine_.variable(game_cell(EclAddress{0x49F1})), shop.facing};
    for (const auto side :
         {MapDirection::north, MapDirection::east, MapDirection::south, MapDirection::west})
    {
        const auto x = (static_cast<int>(shop.x) + step_x(side) + 16) % 16;
        const auto y = (static_cast<int>(shop.y) + step_y(side) + 16) % 16;
        if (static_cast<unsigned>(x) == from.x && static_cast<unsigned>(y) == from.y)
        {
            bind_pose({from.x, from.y, side});
            return;
        }
    }
    const auto behind = reversed(shop.facing);
    bind_pose({static_cast<unsigned>((static_cast<int>(shop.x) + step_x(behind) + 16) % 16),
               static_cast<unsigned>((static_cast<int>(shop.y) + step_y(behind) + 16) % 16),
               behind});
}

std::string RolfTourSession::shoot_arrows(unsigned arrows, std::string_view missile,
                                          const rules::HazardAttack &arrow)
{
    const bool vowel = std::string_view("aeiou").find(missile.front()) != std::string_view::npos;
    const auto article = (vowel ? "\nAn " : "\nA ") + std::string(missile);
    std::string report;
    for (unsigned n = 0; n < arrows; ++n)
    {
        const auto hit = campaign_->hazard_attack(arrow);
        if (!hit)
        {
            break;
        }
        const auto &result = hit->result;
        const auto &member = campaign_->member(hit->target);
        const auto &name = member.character.sheet().name;
        const auto roll = " (d20 " + std::to_string(result.natural) + " + " +
                          std::to_string(arrow.attack_bonus) + " vs AC " +
                          std::to_string(result.armor_class) + ")";
        if (!result.hit)
        {
            report += article + " misses " + name + roll + ".";
            continue;
        }
        report += article + (result.critical ? " critically hits " : " hits ") + name + roll +
                  " for " + std::to_string(result.damage) + " damage" +
                  damage_dice_shown(result, arrow.sides) + ".";
        if (member.vitals.dead)
        {
            report += " " + name + " dies.";
        }
        else if (!result.death_saves.empty())
        {
            report += " " + name +
                      (member.vitals.hit_points ? " falls, then regains 1 HP."
                                                : " falls unconscious and is stable.");
        }
    }
    return report;
}

// A DAMAGE call's harm to one member: the saving throw that avoided it, or
// the damage taken.
std::string RolfTourSession::hurt(const std::optional<HazardHit> &hit, int save_dc,
                                  unsigned sides) const
{
    if (!hit)
    {
        return {};
    }
    const auto &result = hit->result;
    const auto &member = campaign_->member(hit->target);
    const auto &name = member.character.sheet().name;
    const auto save = " (saving throw " + std::to_string(result.save_total) + " vs DC " +
                      std::to_string(save_dc) + ")";
    if (result.saved)
    {
        return "\n" + name + " avoids it" + save + ".";
    }
    auto report = "\n" + name + (save_dc ? " fails to avoid it" + save + " and" : "") + " takes " +
                  std::to_string(result.damage) + " damage" + damage_dice_shown(result, sides) +
                  ".";
    if (member.vitals.dead)
    {
        report += " " + name + " dies.";
    }
    else if (!result.death_saves.empty())
    {
        report += " " + name +
                  (member.vitals.hit_points ? " falls, then regains 1 HP."
                                            : " falls unconscious and is stable.");
    }
    return report;
}

bool RolfTourSession::handle_town_host(const EclRequest &request)
{
    const auto op = request.instruction->opcode;
    EclHostReply reply;
    // Operand n as a number; address_arg reads the operands that name a script cell.
    const auto arg = [&](unsigned n)
    {
        return request.arguments.at(n).number();
    };
    const auto address_arg = [&](unsigned n)
    {
        return request.arguments.at(n).address();
    };
    // The party's own requests need a campaign party: a session copied without
    // one (detached_copy) refuses them rather than reading none.
    const bool clock_moved = adopt_script_clock();
    constexpr std::array party_requests{11, 29, 30, 34, 35, 41, 46, 52, 54, 59};
    if (!campaign_ && std::find(party_requests.begin(), party_requests.end(), op) !=
                          party_requests.end())
    {
        throw EclError("This event needs the campaign party");
    }
    if (diagnostic_log_enabled())
    {
        log_diagnostic(
            LogPriority::debug, "script",
            "script " + std::to_string(current_script_) + " asks the host for op " +
                std::to_string(op) +
                (request.instruction ? " at " + std::to_string(request.instruction->address)
                                     : std::string{}) +
                ", record slot " + std::to_string(selected_character_.index) + " with " +
                std::to_string(machine_.variable(game_cell(ecl_character_hit_points))) + " HP");
    }
    switch (op)
    {
    case 10:
        // A staged monster's record (slot 8 and up), which the tavern
        // free-for-all loads to send monsters berserk. None goes berserk here
        // (PODOL-1), so reading or storing one changes nothing.
        if ((arg(0) & 127) >= 8 && (arg(0) & 127) < 8 + staged_enemies_.size())
        {
            break;
        }
        if (arg(0) >= 128)
        {
            if (static_cast<unsigned>(arg(0) & 127) != selected_character_.index)
            {
                throw EclError("Selected character store mismatch");
            }
            // A script that blanks a companion's name and status before
            // storing the record sends the companion away (Dirten stays at
            // the cleansed Temple of Bane, ECL1:24 0xACC3).
            if (dismissed_companion())
            {
                campaign_->remove(campaign_->state().slots[selected_character_.index]);
            }
            else
            {
                read_character();
            }
            reply = character_reply(selected_character_);
        }
        else
        {
            read_character();
            selected_character_ = PartySlot{arg(0)};
            reply = character_reply(selected_character_);
            // LOAD CHARACTER is also used to scan slots. WHO, not that VM
            // cursor, changes the player's chosen party member.
        }
        break;
    case 57:
        read_character();
        if (campaign_)
        {
            who_slots_.clear();
            snapshot_.choices.clear();
            for (unsigned slot = 0; slot < 8; ++slot)
            {
                if (auto id = campaign_->state().slots[slot])
                {
                    who_slots_.push_back(PartySlot{slot});
                    snapshot_.choices.push_back(campaign_->member(id).character.sheet().name);
                }
            }
            if (who_slots_.empty())
            {
                throw EclError("WHO requires a party member");
            }
            who_request_ = request.id;
            snapshot_.phase = TourPhase::awaiting_continue;
            snapshot_.continue_ticket = ++next_ticket_;
            snapshot_.choosing_member = true;
            // WHO names its own question, after whatever the script has said.
            if (snapshot_.dialogue.find_first_not_of(" \n") == std::string::npos)
            {
                snapshot_.dialogue = request.arguments.at(0).text();
            }
            else
            {
                snapshot_.dialogue += "\n" + request.arguments.at(0).text();
            }
            ++snapshot_.revision;
            return true;
        }
        selected_character_ = {};
        reply = character_reply(selected_character_);
        break;
    case 12:
    {
        // Encounter distance images use the already verified SPRIT3 format.
        std::array<Image, 3> decoded;
        if (arg(1) > 2)
        {
            throw EclError("Unsupported encounter distance");
        }
        encounter_distance_ = std::min<unsigned>(arg(1), open_squares_ahead(map_, snapshot_.pose));
        // The Pitt's buccaneer names sprite 19, which SPRIT1 lacks: met face
        // to face, the original shows the head (0x6DE1) and body portrait.
        const auto head_id = machine_.variable(game_cell(EclAddress{0x6DE1}));
        if (!decode_ega_sprite(area_resources().sprite_archive,
                               static_cast<std::uint8_t>(arg(0)), 0) &&
            encounter_distance_ == 0 && head_id != 255)
        {
            snapshot_.sprite_frame = -1;
            snapshot_.sprite_id = arg(0);
            monster_picture_id_.reset();
            picture_animation_id_.reset();
            picture_ = close_up_portrait(area_resources(), head_id, arg(2));
            ++snapshot_.picture_revision;
            break;
        }
        for (unsigned n = 0; n < 3; ++n)
        {
            auto image = decode_ega_sprite(area_resources().sprite_archive,
                                           static_cast<std::uint8_t>(arg(0)),
                                           static_cast<std::uint8_t>(n));
            // Beyond a sprite's last distance frame its farthest one shows.
            if (!image && n > 0)
            {
                decoded[n] = decoded[n - 1];
                continue;
            }
            if (!image)
            {
                throw EclError("Unsupported encounter sprite " + std::to_string(arg(0)));
            }
            decoded[n] = std::move(image.image);
        }
        sprites_ = std::move(decoded);
        snapshot_.sprite_frame = static_cast<int>(encounter_distance_);
        snapshot_.sprite_id = arg(0);
        monster_picture_id_ = arg(2);
        picture_.reset();
        ++snapshot_.picture_revision;
        break;
    }
    case 14:
    {
        snapshot_.sprite_frame = -1;
        picture_.reset();
        picture_animation_id_.reset();
        ++snapshot_.picture_revision;
        if (arg(0) == 255)
        {
            break;
        }
        // A cancelled auction meets the plaza group rolled last; when none was
        // rolled this visit, one is rolled as the auction opens (PODOL-1).
        if (current_script_ == 18 && arg(0) == 16 && !plaza_group_rolled_)
        {
            roll_plaza_group(request, reply);
        }
        const auto head_id = machine_.variable(game_cell(EclAddress{0x6DE1}));
        if (head_id == 255)
        {
            const auto found = area_resources().pictures.find(arg(0));
            const auto animation = area_resources().animations.find(arg(0));
            if (animation != area_resources().animations.end())
            {
                picture_animation_id_ = arg(0);
            }
            if (found != area_resources().pictures.end())
            {
                picture_ = found->second;
            }
            else if (animation != area_resources().animations.end() && !animation->second.empty())
            {
                picture_ = animation->second.front().image;
            }
            else
            {
                throw EclError("Unsupported town picture " + std::to_string(arg(0)));
            }
        }
        else
        {
            picture_ = close_up_portrait(area_resources(), head_id, arg(0));
        }
        break;
    }
    case 11:
    {
        constexpr auto monster_limit = fight_monster_limit;
        // A roaming group's size follows the party's strength, so a party with
        // most of its members down can be met by no one at all: the script
        // loads nothing, and its COMBAT then has nothing to fight.
        if (!arg(1))
        {
            break;
        }
        // An override (data/encounters/overrides.json) changes what this
        // instruction loads; the count is still sized to the party.
        const auto *change = encounter_override(*request.instruction);
        const unsigned record = change && change->record ? *change->record : arg(0);
        const unsigned loaded = change && change->count ? *change->count : arg(1);
        const unsigned icon = change && change->icon ? *change->icon : arg(2);
        const auto count = static_cast<unsigned>(std::min<std::size_t>(
            loaded, monster_limit - std::min(monster_limit, staged_enemies_.size())));
        if (!plays_original_records() && !area_resources().conversions.contains(record))
        {
            throw EclError("Encounter needs an explicit supported creature conversion: record " +
                           std::to_string(record) + ", count " + std::to_string(count) + ", icon " +
                           std::to_string(icon));
        }
        const auto &area = area_resources();
        const bool ally = fights_for_party(area, record);
        const auto *exported = exported_creature(area, record);
        const auto name = creature_name(area, record, exported);
        for (unsigned i = 0; ally && i < count; ++i)
        {
            const auto id =
                static_cast<rules::EntityId>(opengold::first_ally_id + staged_allies_.size());
            auto staged =
                stage_area_creature(area, record, icon, exported, id, name, rules::Side::party,
                                    difficulty_, plays_original_records());
            staged_allies_.push_back(std::move(staged.fighter));
            staged_ally_art_.push_back(std::move(staged.art));
        }
        for (unsigned i = 0; !ally && i < count; ++i)
        {
            const auto id = static_cast<rules::EntityId>(1000 + staged_enemies_.size());
            auto staged =
                stage_area_creature(area, record, icon, exported, id,
                                    name + " " + std::to_string(staged_enemies_.size() + 1),
                                    rules::Side::opposition, difficulty_, plays_original_records());
            staged_enemies_.push_back(std::move(staged.fighter));
            staged_art_.push_back(std::move(staged.art));
            staged_records_.push_back(record);
        }
        break;
    }
    case 28:
        treasure_.clear();
        staged_treasure_.reset();
        staged_enemies_.clear();
        staged_art_.clear();
        staged_records_.clear();
        staged_allies_.clear();
        staged_ally_art_.clear();
        break;
    case 34:
    {
        // PARTY SURPRISE: 1 in the first output when a Ranger travels with the
        // party, which the SURPRISE that follows takes from the party's chance
        // of being surprised; the second is always 0 (the original's eclvm.pas).
        std::map<EclAddress, std::uint16_t> outputs;
        outputs[address_arg(0)] = campaign_ && has_ranger(*campaign_) ? 1 : 0;
        outputs[address_arg(1)] = 0;
        for (auto [address, value] : outputs)
        {
            reply.writes.push_back({address, value});
        }
        break;
    }
    case 35:
    {
        const int party_threshold = 2 + static_cast<int>(arg(3)) -
                                    static_cast<int>(machine_.variable(game_cell(address_arg(0))));
        const int monster_threshold =
            2 + static_cast<int>(machine_.variable(game_cell(address_arg(1)))) -
            static_cast<int>(arg(2));
        const bool party =
            static_cast<int>(machine_.host_random(request.id, 6)) + 1 <= party_threshold;
        const bool monsters =
            static_cast<int>(machine_.host_random(request.id, 6)) + 1 <= monster_threshold;
        reply.writes.push_back({ecl_encounter_surprise,
                                static_cast<std::uint16_t>((party ? 1 : 0) | (monsters ? 2 : 0))});
        break;
    }
    case 41:
        if (!current_area_ || arg(1) > 2)
        {
            throw EclError("Unsupported encounter menu context");
        }
        encounter_menu_ = request;
        encounter_distance_ = std::min<unsigned>(arg(1), open_squares_ahead(map_, snapshot_.pose));
        show_encounter_menu();
        return true;
    case 29:
        // Podol Plaza's group roll asks the party's strength (ECL1:18 0xA00D).
        if (current_script_ == 18 && request.instruction && request.instruction->address == 0xA00D)
        {
            plaza_group_rolled_ = true;
        }
        read_character();
        reply = character_reply(selected_character_);
        reply.writes.push_back(
            {address_arg(0), static_cast<std::uint16_t>(party_strength(*campaign_))});
        break;
    case 30:
    {
        read_character();
        reply = character_reply(selected_character_);
        auto values =
            check_party(*campaign_, pool_of_radiance_address(address_arg(0), game()), arg(1));
        // A trap the Wand of Secrets pointed out is seen as Find Traps sees it.
        constexpr unsigned find_traps_effect = 19;
        if (address_arg(0) == EclAddress{0} && arg(1) == find_traps_effect && on_wand_trap())
        {
            values[3] = 1;
        }
        std::map<EclAddress, std::uint16_t> outputs;
        for (unsigned n = 0; n < 4; ++n)
        {
            outputs[address_arg(n + 2)] = static_cast<std::uint16_t>(values[n]);
        }
        for (const auto &[address, value] : outputs)
        {
            reply.writes.push_back({address, value});
        }
        break;
    }
    case 52:
        // ECL CLOCK: time passes as for arg(0) steps, while a script walks the
        // party (the nomad camp's escort, ECL7:17 0x9D37).
        read_character();
        campaign_->advance_time(step_time * arg(0));
        reply = character_reply(selected_character_);
        for (const auto &w : clock_reply().writes)
        {
            reply.writes.push_back(w);
        }
        break;
    case 59:
    {
        // SPELL: which member can cast the original's spell arg(0), as the
        // spell's place in that member's list (255 for nobody) and the
        // member's slot. Only Knock (31) is asked, by Kovel Mansion's locks
        // (ECL3:14 0x9ECA); its caster spends the spell now. The original
        // looked only at the first member's spells, by a slip in its code;
        // every member is asked here (KOVEL-1).
        constexpr unsigned knock = 31;
        if (arg(0) != knock)
        {
            throw EclError("Unsupported SPELL " + std::to_string(arg(0)));
        }
        read_character();
        reply = character_reply(selected_character_);
        std::uint16_t place = 255, slot = 0;
        if (campaign_->can_try_door(DoorMethod::knock))
        {
            const auto caster = campaign_->try_door(DoorMethod::knock, 0).front().member;
            const auto &slots = campaign_->state().slots;
            place = 0;
            slot = static_cast<std::uint16_t>(std::find(slots.begin(), slots.end(), caster) -
                                              slots.begin());
        }
        reply.writes.push_back({address_arg(1), place});
        reply.writes.push_back({address_arg(2), slot});
        break;
    }
    case 54:
    {
        // ADD NPC names a record of the area's MONnCHA archive: New Phlan's is
        // MON3CHA, whose record 24 has no conversion.
        read_character();
        const unsigned bank = current_area_ ? area_resources().bank : 3;
        const auto &profiles = current_area_ ? area_resources().npc_profiles : town_->npc_profiles;
        std::optional<Character> companion;
        const Creature *original = nullptr;
        if (plays_original_records() && town_->creatures)
        {
            if (const auto found = town_->creatures->find(
                    {static_cast<std::uint8_t>(bank), static_cast<std::uint8_t>(arg(0))}))
            {
                original = &found->get();
            }
        }
        if (original)
        {
            // Under rules that play the original's records the NPC joins as
            // its record holds it.
            const auto &creation = *campaign_->character_rules();
            if (const auto draft = creation.record_draft(original->stored.raw))
            {
                companion = Character(creation, *draft, {});
            }
        }
        else if (const auto found = profiles.find(arg(0));
                 found != profiles.end() && !(bank == 3 && arg(0) == 24))
        {
            companion = found->second;
        }
        else if (companion_rules_)
        {
            companion = original_companion(bank, arg(0), *companions_, *companion_rules_,
                                           campaign_->rule_module());
        }
        if (!companion)
        {
            throw EclError("NPC needs an explicit supported conversion/allegiance profile");
        }
        const auto joined =
            campaign_->recruit("por:MON" + std::to_string(bank) + "CHA:" + std::to_string(arg(0)),
                               std::move(*companion), arg(1));
        // Under the original rules it brings its record's own gear, with what
        // the record has readied in use.
        std::vector<std::uint64_t> readied;
        const auto held = [&]
        {
            std::vector<std::uint64_t> ids;
            for (const auto &item : campaign_->member(joined).character.inventory().items())
            {
                ids.push_back(item.id);
            }
            return ids;
        };
        if (original)
        {
            for (const auto &item : original->equipment)
            {
                const auto before = held();
                campaign_->take_item(joined, item);
                for (const auto id : held())
                {
                    if (item.stored.readied() && std::ranges::find(before, id) == before.end())
                    {
                        readied.push_back(id);
                    }
                }
            }
        }
        // A companion arrives with its gear on (Dirten's mace and armor).
        const auto &member = campaign_->member(joined);
        std::vector<std::uint64_t> unequipped;
        for (const auto &item : member.character.inventory().items())
        {
            if (std::find(member.equipped.begin(), member.equipped.end(), item.id) ==
                    member.equipped.end() &&
                (!original || std::ranges::find(readied, item.id) != readied.end()))
            {
                unequipped.push_back(item.id);
            }
        }
        for (const auto item : unequipped)
        {
            (void)campaign_->equip(joined, item);
        }
        // The script may be looking at the slot the companion just took. The
        // record it holds must show the new member, or reading it back would
        // find a member with no money and charge the party for the difference.
        reply = character_reply(selected_character_);
        break;
    }
    case 39:
    {
        bool money = false;
        for (unsigned n = 0; n < 7; ++n)
        {
            money = money || arg(n);
        }
        // In an area, or in a town building's own script (City Hall's
        // enchanted weapon for the graveyard commission, ECL3:8 0xA780), a
        // fixed item list is script treasure like money; the town script's
        // lists stock its shops.
        if (money || ((current_area_ || current_script_ != 0) && arg(7) != 255))
        {
            stage_treasure(request);
            break;
        }
        if (arg(7) == 255)
        {
            break;
        }
        const auto found = town_->treasure.find(arg(7));
        if (found == town_->treasure.end())
        {
            throw EclError("Unsupported town treasure list");
        }
        // A shop list without its arrows and quarrels: there is no ammunition.
        std::ranges::copy_if(found->second, std::back_inserter(treasure_),
                             [](const auto &item)
                             {
                                 return !opengold::original_item::is_ammunition(item.stored.type);
                             });
        break;
    }
    case 36:
        // A Magic Circle drawn in camp keeps its creature type away: no fight,
        // and the rest goes on.
        if (current_area_ && !staged_enemies_.empty() &&
            event_stage_ == EventStage::camp_interrupted && camp_warded())
        {
            rest_warded_ = true;
            staged_enemies_.clear();
            staged_art_.clear();
            staged_records_.clear();
            reply = character_reply(selected_character_);
            reply.writes.push_back({game_cell(ecl_combat_result), 0});
            reply.writes.push_back({game_cell(ecl_monsters_defeated), 0});
            break;
        }
        if (mirror_duel_ && campaign_)
        {
            stage_mirror();
        }
        // New Phlan's own fights too: the city watch's, Tempus's priests'.
        if (!staged_enemies_.empty())
        {
            read_character();
            encounter_records_ = staged_records_;
            // The arena's copy is the duelist's match, whatever the party.
            if (!mirror_duel_)
            {
                fit_staged_encounter();
            }
            const auto p = snapshot_.pose;
            // On the wilderness map the field grows from the terrain under the
            // party, drawn with the wilderness set, north up.
            const auto roll = [&](int count, int sides)
            {
                int total = 0;
                for (int die = 0; die < count; ++die)
                {
                    total += static_cast<int>(
                                 machine_.host_random(request.id, static_cast<unsigned>(sides))) +
                             1;
                }
                return total;
            };
            // The original faces a wilderness fight the way the party stepped;
            // a diagonal step faces the direction to its left (PLACE-2).
            const auto wilderness_facing = static_cast<MapDirection>(
                machine_.variable(game_cell(ecl_overland_direction)) % 8 / 2);
            auto field =
                overland()
                    ? wilderness_battlefield(
                          *town_->overland,
                          town_->overland->at(town_->overland->window_start[travel_mode_ - 2] +
                                                  machine_.variable(game_cell(ecl_overland_x)),
                                              machine_.variable(game_cell(ecl_overland_y))),
                          roll)
                    : dungeon_battlefield(map_, p.x, p.y);
            encounter_ = opengold::CampaignEncounter{
                std::move(field),
                std::move(staged_enemies_),
                std::move(staged_art_),
                overland() ? town_->wilderness_art : area_resources().terrain_art,
                overland() ? wilderness_facing : p.facing,
                script_surprise(machine_.variable(game_cell(ecl_encounter_surprise)))};
            encounter_->band = std::move(staged_band_);
            staged_band_.clear();
            encounter_->allies = std::move(staged_allies_);
            staged_allies_.clear();
            for (auto &art : staged_ally_art_)
            {
                encounter_->art.push_back(std::move(art));
            }
            staged_ally_art_.clear();
            encounter_->party_resting = event_stage_ == EventStage::camp_interrupted;
            const auto hour = clock_minute_of_day(campaign_->state().time_minutes) / 60;
            encounter_->daylight = hour >= 6 && hour < 18;
            if (std::exchange(duel_, false))
            {
                encounter_->mirror = mirror_duel_;
                encounter_->champion = campaign_->state().slots.at(selected_character_.index);
                if (!encounter_->champion)
                {
                    throw EclError("A duel needs a chosen member");
                }
            }
            else
            {
                std::size_t living = 0;
                for (const auto id : campaign_->state().slots)
                {
                    if (id && !campaign_->member(id).vitals.dead)
                    {
                        ++living;
                        if (turncoat(id))
                        {
                            encounter_->turncoats.push_back(id);
                        }
                    }
                }
                // With every member gone over to the monsters, no one is left
                // to fight them: the party is lost.
                if (living && encounter_->turncoats.size() == living)
                {
                    snapshot_.phase = TourPhase::defeated;
                    ++snapshot_.revision;
                    return true;
                }
            }
            if (!overland())
            {
                encounter_distance_ = std::min(encounter_distance_, open_squares_ahead(map_, p));
            }
            // On the wilderness map the original stands the monsters 2 squares
            // away, whatever SETUP MONSTER asked (PLACE-2).
            encounter_->distance = overland() ? 2u : encounter_distance_;
            // The script sets the encounter's morale before COMBAT.
            encounter_->morale =
                std::min<unsigned>(machine_.variable(game_cell(ecl_encounter_morale)), 100);
            apply_difficulty(*encounter_, difficulty_, average_party_level(living_member_levels()));
            announce_camp_attack();
            combat_request_ = request.id;
            // The original shows the approached monster's close-up until a key press.
            showing_monster_picture_ =
                monster_picture_id_ && area_resources().animations.contains(*monster_picture_id_);
            snapshot_.phase = TourPhase::combat;
            ++snapshot_.revision;
            return true;
        }
        if (machine_.variable(game_cell(ecl_temple_service)) == 1)
        {
            if (!campaign_)
            {
                throw EclError("Temple service requires a campaign party");
            }
            read_character();
            // Word of Recall may prepare its sanctuary on this square.
            if (!current_area_)
            {
                temple_square_ = Sanctuary{current_script_, snapshot_.pose};
            }
            open_temple(request.id);
            return true;
        }
        if (machine_.variable(game_cell(ecl_shop_service)) == 1)
        {
            // Another buyer can still be chosen.
            choose_shopper();
            if (campaign_ && !campaign_->state().slots.at(campaign_->state().selected_slot.index))
            {
                throw EclError("Shopping requires a selected party member");
            }
            read_character();
            // The shop data/shops places on this square sells its record's items.
            for (std::size_t n = 0; n < shops_.size(); ++n)
            {
                if (standing_at(shops_[n].archive, shops_[n].record, shops_[n].markers))
                {
                    treasure_ = shop_stocks_[n];
                }
            }
            shop_request_ = request.id;
            snapshot_.phase = TourPhase::shopping;
            snapshot_.continue_ticket = ++next_ticket_;
            snapshot_.choices.clear();
            snapshot_.diagnostic.clear();
            ++snapshot_.revision;
            return true;
        }
        if (staged_treasure_)
        {
            // With no monsters loaded, the original COMBAT only hands out the treasure.
            read_character();
            award_staged_treasure();
            if (!treasure_pile_.empty())
            {
                open_treasure(request.id, {});
                return true;
            }
            reply = character_reply(selected_character_);
            break;
        }
        if (machine_.variable(game_cell(ecl_temple_service)) == 1)
        {
            throw EclError("Temple healing service");
        }
        // With no one loaded, the original's COMBAT only ends the encounter
        // (eclvm.pas, EclCombat): a roaming group sized to a weak party can
        // be no one at all.
        reply = character_reply(selected_character_);
        reply.writes.push_back({game_cell(ecl_combat_result), 0});
        reply.writes.push_back({game_cell(ecl_monsters_defeated), 0});
        break;
    case 32:
    {
        const auto found = town_->programs.find(arg(0));
        if (found == town_->programs.end())
        {
            closed_way_ =
                "The way to another area is not in the game yet.";
            throw EclError(closed_way_);
        }
        reply.next_program = found->second;
        // 0x6E12 names the new script's archive bank: ECL3 for the town.
        const auto script_bank = town_->script_banks.find(arg(0));
        const unsigned bank = script_bank == town_->script_banks.end() ? 3 : script_bank->second;
        reply.writes = {
            {game_cell(EclAddress{0x49F2}), static_cast<std::uint16_t>(current_script_)},
            {game_cell(ecl_script_bank), static_cast<std::uint16_t>(bank)}};
        // Cadorna's payment (CADORNA-1): the clerk's reward slot 18 pays for the
        // family treasure brought back sealed, which no original script sets.
        if (arg(0) == city_hall_script && machine_.variable(game_cell(cadorna_box)) == 1 &&
            machine_.variable(game_cell(cadorna_payment)) == 0)
        {
            reply.writes.push_back({cadorna_payment, 254});
        }
        current_script_ = arg(0);
        snapshot_.script_id = current_script_;
        transition_ = true;
        pending_movement_.reset();
        break;
    }
    case 33:
    {
        // Original gate transition refresh before NEW ECL selects its district.
        if (arg(0) == 255 && arg(1) == 255 && arg(2) == 127)
        {
            break;
        }
        // A site of the wilderness map clears the view (127) before NEW ECL.
        if (arg(0) == 127 && arg(1) == 127 && arg(2) == 127)
        {
            break;
        }
        // A wilderness script with its 3D-area cell clear shows the
        // wilderness map (LOAD FILES 4,4,0 to 6,6,0); its own district holds
        // its art and creatures.
        if (current_script_ >= 25 && current_script_ <= 27 &&
            machine_.variable(game_cell(ecl_in_3d_area)) == 0 && arg(0) == arg(1) && arg(2) == 0)
        {
            for (const auto &[id, district] : town_->districts)
            {
                if (district->loaded_by(current_script_))
                {
                    change_area(id);
                    break;
                }
            }
            break;
        }
        // In Curse a script loads any map of its disk; 255 keeps the current one.
        if (game() != GoldBoxGame::pool_of_radiance)
        {
            if (arg(0) == 255)
            {
                break;
            }
            if (!town_->districts.contains(arg(0)))
            {
                throw EclError("Resources outside the supported areas (map " +
                               std::to_string(arg(0)) + ")");
            }
            change_area(arg(0));
            const auto &here = map_.at(machine_.variable(game_cell(ecl_party_x)),
                                       machine_.variable(game_cell(ecl_party_y)));
            const auto facing = machine_.variable(game_cell(ecl_party_facing));
            reply.writes = {
                {game_cell(ecl_wall_ahead), here.walls[facing % 4]},
                {game_cell(ecl_cell_event), here.event_raw},
                {game_address(ecl_current_map, game()), static_cast<std::uint16_t>(arg(0))}};
            break;
        }
        // Map 255 keeps the current map: Sokal Keep's elf scroll loads the
        // rune glyphs (bank 16) and then its own art again (bank 2).
        if (current_area_ && arg(0) == 255 && (arg(1) == 16 || arg(1) == 2) && arg(2) == 255)
        {
            break;
        }
        // A district's scripts load its maps: the Slums map 20, Kuto's Well
        // its plaza (29) and catacombs (32), Valjevo Castle's scripts their
        // shared quarters.
        const auto district = town_->districts.find(arg(0));
        // Leaving for the wilderness, the Temple of Bane's block loads a
        // wilderness script's map (ECL1:24 0x997B) before NEW ECL to that
        // script, which loads what it shows itself.
        if (district != town_->districts.end() && !district->second->loaded_by(current_script_) &&
            district->second->script >= 25 && district->second->script <= 27 && arg(1) == 2 &&
            arg(2) == 255)
        {
            break;
        }
        if (district != town_->districts.end() && district->second->loaded_by(current_script_) &&
            (arg(1) == 2 || arg(1) == 255) && arg(2) == 255)
        {
            change_area(arg(0));
        }
        else if (arg(0) == 0 && arg(1) == 0 && arg(2) == 0)
        {
            change_area(0);
        }
        else
        {
            throw EclError("Resources outside the supported areas (map " + std::to_string(arg(0)) +
                           ")");
        }
        const auto &cell = map_.at(machine_.variable(game_cell(ecl_party_x)),
                                   machine_.variable(game_cell(ecl_party_y)));
        const auto facing = machine_.variable(game_cell(ecl_party_facing));
        if (facing >= 4)
        {
            throw EclError("Invalid district entry facing");
        }
        reply.writes = {{game_cell(ecl_wall_ahead), cell.walls[facing]},
                        {game_cell(ecl_cell_event), cell.event_raw}};
        // The original keeps the loaded map's number, which a script with two
        // maps reads to tell them apart (the Temple of Bane, ECL1:24 0x993D).
        if (current_area_)
        {
            reply.writes.push_back(
                {game_address(ecl_current_map, game()), static_cast<std::uint16_t>(current_area_)});
        }
        break;
    }
    case 64:
        // DESTROY ITEMS (Curse): every item of the type in the party is gone.
        campaign_->destroy_items(static_cast<std::uint8_t>(arg(0)));
        break;
    case 60:
    {
        // PROTECTION: a line of rune glyphs at the operand's address, which the
        // original's code wheel translated; shown translated (SOKAL-1).
        const auto line = rune_text(machine_, address_arg(0));
        snapshot_.dialogue += line;
        rune_scroll_ += line + "\n";
        break;
    }
    case 55:
    {
        // The wilderness's sites reset the wall banks (127) before NEW ECL.
        const std::array<unsigned, 3> banks{arg(0), arg(1), arg(2)};
        if (banks == std::array<unsigned, 3>{127, 127, 127})
        {
            break;
        }
        // In Curse a script loads the view's walls itself, as the original does
        // (eclvm.pas EclLoadFiles): those of its own disk.
        if (game() != GoldBoxGame::pool_of_radiance)
        {
            const auto disk = town_->script_banks.find(current_script_);
            const auto walls = disk == town_->script_banks.end()
                                   ? town_->wall_sets.end()
                                   : town_->wall_sets.find({disk->second, banks});
            if (walls == town_->wall_sets.end())
            {
                throw EclError("Unknown wall banks for script " + std::to_string(current_script_));
            }
            wall_art_ = walls->second;
            ++snapshot_.revision;
            break;
        }
        // A script may load its wall banks before its map (Kovel Mansion's
        // entry, ECL3:14 0xB592), so they are checked against its own
        // districts, not the area it came from.
        const bool own_district =
            std::any_of(town_->districts.begin(), town_->districts.end(),
                        [&](const auto &district)
                        {
                            return district.second->loaded_by(current_script_) &&
                                   district.second->pieces == banks;
                        });
        if (!own_district)
        {
            throw EclError(current_area_ ? "Unverified district wall resource profile"
                                         : "Unverified town wall resource profile");
        }
        break;
    }
    case 50:
    {
        const bool found = campaign_ ? party_has_item(*campaign_, arg(0))
                                     : std::any_of(party_.inventory.begin(), party_.inventory.end(),
                                                   [&](const auto &i)
                                                   {
                                                       return i.stored.type == arg(0);
                                                   });
        reply.conditions = EclConditions{found, !found, false, false, false, false};
        break;
    }
    case 46:
    {
        // Arrow volleys: arg(0) arrows of arg(1)d arg(2) + arg(3). A high bit
        // marks the original's other forms (coab CMD_Damage): the dice dealt
        // to the member the script chose, with no saving throw (the Cadorna
        // well's fall), or to a random member who fails a saving throw with
        // the bonus in arg(0)'s low bits, of the type arg(4) names (Mendor's
        // green slime). The original names no damage type; script_damage_type
        // gives each call's.
        const auto address = request.instruction->address;
        const bool volley = !(arg(0) & 0x80) && !(arg(4) & 0x80);
        const bool chosen_member = (arg(0) & 0xC0) == 0x80 && (arg(4) & 0x87) == 0x80;
        const bool random_member =
            (arg(0) & 0xD0) == 0x80 && !(arg(4) & 0x80) && (arg(4) & 7) < original_save_abilities.size();
        // Every member, each saving unless arg(0) has 0x20 (Kovel Mansion's gas).
        const bool whole_party =
            (arg(0) & 0xD0) == 0xC0 && (arg(4) & 7) < original_save_abilities.size();
        if (!campaign_ || (!volley && !chosen_member && !random_member && !whole_party))
        {
            throw EclError("Unsupported DAMAGE form");
        }
        read_character();
        if (volley)
        {
            // Arrows unless the call names another missile (Kovel Mansion's
            // sling bullets, the lizard men's javelins).
            const auto damage =
                listed_damage(current_script_, address).value_or(ScriptDamage{0, 0, "piercing"});
            rules::HazardAttack volley_attack{hidden_archer_attack_bonus, arg(1), arg(2),
                                              static_cast<int>(arg(3)), std::string(damage.type)};
            volley_attack.original_attack = static_cast<int>(arg(4));
            snapshot_.dialogue += shoot_arrows(arg(0), damage.missile, volley_attack);
        }
        else
        {
            // Only the SRD's resistances need a damage type.
            rules::HazardAttack harm{0, arg(1), arg(2), static_cast<int>(arg(3)),
                                     plays_original_records()
                                         ? std::string()
                                         : script_damage_type(current_script_, address)};
            harm.automatic = true;
            if (whole_party)
            {
                if (!(arg(0) & 0x20))
                {
                    harm.save_ability = original_save_abilities[arg(4) & 7];
                    harm.save_dc = original_save_dc - static_cast<int>(arg(0) & 0x1F);
                    harm.original_save = static_cast<int>(arg(4) & 7);
                    harm.original_save_modifier = static_cast<int>(arg(0) & 0x1F);
                }
                for (const auto id : campaign_->state().slots)
                {
                    if (id)
                    {
                        const auto result = campaign_->hazard_attack(id, harm);
                        snapshot_.dialogue +=
                            hurt(result ? std::optional<HazardHit>{{id, *result}} : std::nullopt,
                                 harm.save_dc, harm.sides);
                    }
                }
            }
            else if (random_member)
            {
                harm.save_ability = original_save_abilities[arg(4) & 7];
                harm.save_dc = original_save_dc - static_cast<int>(arg(0) & 0x1F);
                harm.original_save = static_cast<int>(arg(4) & 7);
                harm.original_save_modifier = static_cast<int>(arg(0) & 0x1F);
                snapshot_.dialogue +=
                    hurt(campaign_->hazard_attack(harm), harm.save_dc, harm.sides);
            }
            else
            {
                const auto id = campaign_->state().slots.at(selected_character_.index);
                if (!id)
                {
                    throw EclError("DAMAGE needs a chosen member");
                }
                const auto result = campaign_->hazard_attack(id, harm);
                snapshot_.dialogue +=
                    hurt(result ? std::optional<HazardHit>{{id, *result}} : std::nullopt,
                         harm.save_dc, harm.sides);
            }
        }
        damage_request_ = request.id;
        snapshot_.phase = TourPhase::awaiting_continue;
        snapshot_.choices = {"Continue"};
        snapshot_.continue_ticket = ++next_ticket_;
        ++snapshot_.revision;
        return true;
    }
    case 40:
    {
        // Surrender (Kuto's Well): arg(0) 1 robs the whole party of arg(1)% of its
        // coins; arg(0) 0 robs the chosen member (the kobold caves' pool, ECL8:13
        // 0x9AF6), which here takes that member's share of the party's purse.
        // The original also takes arg(2)% of the items; by the user's choice only
        // money is taken.
        if (!campaign_ || arg(0) > 1 || arg(1) > 100)
        {
            throw EclError("Unsupported ROB form");
        }
        read_character();
        {
            const auto &slots = campaign_->state().slots;
            const auto members =
                static_cast<std::uint64_t>(std::count_if(slots.begin(), slots.end(),
                                                         [](MemberId id) { return id != 0; }));
            const std::uint64_t whole = arg(0) == 1 ? 100 : 100 * std::max<std::uint64_t>(1, members);
            auto wealth = campaign_->state().wealth;
            for (auto &coins : wealth)
            {
                coins = static_cast<std::uint32_t>(std::uint64_t{coins} * (whole - arg(1)) / whole);
            }
            campaign_->set_wealth(wealth);
        }
        robbed_ = true;
        reply = character_reply(selected_character_);
        break;
    }
    case 56:
        if (arg(0) == 0 && campaign_)
        {
            read_character();
            open_hall(request.id);
            return true;
        }
        // Tyranthraxus defeated (PROGRAM 8): the game is won (area byte 0x3FA,
        // the cell 0x4AFD). The original pauses on its closing words, healing
        // no one; then the script goes on, back to the civilized city.
        if (arg(0) == 8 && campaign_)
        {
            read_character();
            snapshot_.dialogue.clear();
            for (const auto &page : town_->closing_story)
            {
                snapshot_.dialogue += page + "\n\n";
            }
            snapshot_.dialogue += "Tyranthraxus is destroyed, and Phlan is free of him. You have won.";
            victory_ = true;
            damage_request_ = request.id;
            snapshot_.phase = TourPhase::awaiting_continue;
            snapshot_.choices = {"Continue"};
            snapshot_.continue_ticket = ++next_ticket_;
            ++snapshot_.revision;
            return true;
        }
        if (arg(0) != 9 || !campaign_)
        {
            throw EclError("Training/camp service " + std::to_string(arg(0)));
        }
        // The original inn invokes its pre-camp subroutine before PROGRAM 9.
        read_character();
        // Elsewhere PROGRAM 9 opens the camp menu and ends the script (coab
        // TryEncamp): the nomad camp's lodging, whose night the kobolds may
        // break (ECL7:17 0xA5EF).
        if (machine_.variable(game_cell(ecl_rest_interruption_chance)) == 255 ||
            (machine_.variable(game_cell(ecl_rest_check_interval)) &&
             machine_.variable(game_cell(ecl_rest_interruption_chance))))
        {
            snapshot_.camp_offered = true;
            reply = character_reply(selected_character_);
            reply.exit = true;
            break;
        }
        // No one benefits from a second Long Rest within a day (SRD 5.2.1),
        // but the night at the inn still passes, as the original's always did.
        if (const bool rested = campaign_->rest(); rested)
        {
            snapshot_.dialogue += "\nLong rest complete: eight hours passed; eligible members "
                                  "recovered HP and supported resources.";
        }
        else
        {
            campaign_->advance_time(std::chrono::hours{8});
            snapshot_.dialogue +=
                "\nEight hours passed; no one can benefit from another Long Rest so soon.";
        }
        reply = character_reply(selected_character_);
        for (const auto &w : clock_reply().writes)
        {
            reply.writes.push_back(w);
        }
        break;
    default:
        return false;
    }
    if (clock_moved)
    {
        // First, so that what this request itself wrote stands.
        auto record = character_reply(selected_character_).writes;
        reply.writes.insert(reply.writes.begin(), record.begin(), record.end());
    }
    if (!machine_.resume_host(request.id, reply))
    {
        throw EclError("Town host reply rejected");
    }
    if (op == 33)
    {
        publish_pose();
    }
    ++snapshot_.revision;
    return true;
}

void RolfTourSession::open_hall(std::uint64_t request)
{
    // Pool of Radiance's halls name their classes; Curse's set a class mask,
    // every class (127) in each of its halls.
    const auto hall = game() == GoldBoxGame::pool_of_radiance
                          ? opengold::training_hall_named(machine_.string(ecl_training_hall_name))
                      : machine_.variable(game_cell(ecl_training_class_mask)) == 127
                          ? std::optional{opengold::TrainingHall::every_class}
                          : std::nullopt;
    if (!hall)
    {
        throw EclError("Training hall without a known class word");
    }
    hall_visit_ = HallVisit{request, *hall, 0, {}};
    show_hall({});
}

void RolfTourSession::show_hall(std::vector<Message> notice)
{
    using opengold::TrainingDenial;
    if (campaign_->rule_module().trains_at_halls())
    {
        show_original_hall(std::move(notice));
        return;
    }
    auto &visit = *hall_visit_;
    const auto name_of = [&](opengold::MemberId id)
    {
        return campaign_->member(id).character.sheet().name;
    };
    auto dialogue = std::move(notice);
    std::vector<Message> choices;
    visit.choices.clear();
    const auto add = [&](Message label, HallChoice choice)
    {
        choices.push_back(std::move(label));
        visit.choices.push_back(std::move(choice));
    };
    if (visit.member)
    {
        // One line fits above the list; the party's gold shows beside it.
        if (dialogue.empty())
        {
            dialogue.push_back({"Choose a lesson for {name}.", {{"name", name_of(visit.member)}}});
        }
        for (const auto &lesson : campaign_->skill_lessons(visit.member, visit.hall).options)
        {
            add({lesson.id.starts_with("expertise:") ? "{skill} Expertise ({fee} gp)"
                                                     : "{skill} ({fee} gp)",
                 {{"skill", lesson.label, true},
                  {"fee", amount(opengold::skill_training_fee(lesson.id))}}},
                {HallChoice::Kind::lesson, visit.member, lesson.id});
        }
        add({"Back", {}}, {HallChoice::Kind::back});
    }
    else
    {
        if (dialogue.empty())
        {
            dialogue.push_back(hall_welcome(visit.hall));
        }
        for (const auto id : campaign_->state().slots)
        {
            if (!id)
            {
                continue;
            }
            const auto offer = campaign_->experience_offer(id, visit.hall);
            if (offer.denial == TrainingDenial::other_hall ||
                offer.denial == TrainingDenial::not_active)
            {
                continue;
            }
            const auto name = name_of(id);
            auto session = [&]() -> Message
            {
                switch (offer.denial)
                {
                case TrainingDenial::ready_to_advance:
                    return {"{name}: session of experience (advance a level first)",
                            {{"name", name}}};
                case TrainingDenial::already_trained:
                    return {"{name}: session of experience (bought at this level)",
                            {{"name", name}}};
                case TrainingDenial::highest_level:
                    return {"{name}: session of experience (highest level)", {{"name", name}}};
                default:
                    return {"{name}: session of experience, {experience} XP ({fee} gp)",
                            {{"name", name},
                             {"experience", amount(offer.experience)},
                             {"fee", amount(offer.fee)}}};
                }
            }();
            add(std::move(session), {HallChoice::Kind::session, id});
            const auto lessons = campaign_->skill_lessons(id, visit.hall);
            if (lessons.options.empty())
            {
                continue;
            }
            const bool learned =
                campaign_->skill_training_denial(id, visit.hall) == TrainingDenial::already_trained;
            add(Message{learned ? "{name}: skill lesson (learned at this level)"
                                : "{name}: skill lesson",
                        {{"name", name}}},
                {HallChoice::Kind::lessons, id});
        }
        add({"Leave", {}}, {HallChoice::Kind::leave});
    }
    snapshot_.host_dialogue = dialogue;
    snapshot_.dialogue = english(dialogue);
    snapshot_.host_choices = choices;
    snapshot_.choices.clear();
    for (const auto &choice : choices)
    {
        snapshot_.choices.push_back(english(choice));
    }
    snapshot_.diagnostic.clear();
    snapshot_.phase = TourPhase::awaiting_continue;
    snapshot_.continue_ticket = ++next_ticket_;
    ++snapshot_.revision;
}

bool RolfTourSession::choose_at_hall(std::size_t choice)
{
    using opengold::TrainingDenial;
    auto &visit = *hall_visit_;
    const auto picked = visit.choices.at(choice);
    const auto name =
        picked.member ? campaign_->member(picked.member).character.sheet().name : std::string{};
    switch (picked.kind)
    {
    case HallChoice::Kind::train:
    case HallChoice::Kind::confirm:
    case HallChoice::Kind::learn:
        return choose_at_original_hall(picked);
    case HallChoice::Kind::leave:
    {
        if (!machine_.resume_host(visit.request, character_reply(selected_character_)))
        {
            throw EclError("Training hall reply rejected");
        }
        hall_visit_.reset();
        snapshot_.host_dialogue.clear();
        snapshot_.host_choices.clear();
        snapshot_.dialogue.clear();
        snapshot_.phase = TourPhase::running;
        ++snapshot_.revision;
        return true;
    }
    case HallChoice::Kind::back:
        visit.member = 0;
        show_hall({});
        return true;
    case HallChoice::Kind::lessons:
    {
        const auto denial = campaign_->skill_training_denial(picked.member, visit.hall);
        if (denial != TrainingDenial::none && denial != TrainingDenial::gold)
        {
            show_hall({refusal(denial, name, 0)});
            return true;
        }
        visit.member = picked.member;
        show_hall({});
        return true;
    }
    case HallChoice::Kind::session:
    {
        const auto offer = campaign_->experience_offer(picked.member, visit.hall);
        if (offer.denial != TrainingDenial::none)
        {
            show_hall({refusal(offer.denial, name, offer.fee)});
            return true;
        }
        campaign_->buy_experience(picked.member, visit.hall);
        show_hall({{"{name} gains {experience} experience.",
                    {{"name", name}, {"experience", amount(offer.experience)}}}});
        return true;
    }
    case HallChoice::Kind::lesson:
    {
        const auto fee = opengold::skill_training_fee(picked.grant);
        auto denial = campaign_->skill_training_denial(picked.member, visit.hall);
        if (denial == TrainingDenial::none && gold_value(campaign_->state().wealth) < fee)
        {
            denial = TrainingDenial::gold;
        }
        if (denial != TrainingDenial::none)
        {
            show_hall({refusal(denial, name, fee)});
            return true;
        }
        const auto lessons = campaign_->skill_lessons(picked.member, visit.hall).options;
        const auto lesson = std::find_if(lessons.begin(), lessons.end(),
                                         [&](const auto &option)
                                         {
                                             return option.id == picked.grant;
                                         });
        campaign_->train_skill(picked.member, visit.hall, picked.grant);
        visit.member = 0;
        show_hall({{picked.grant.starts_with("expertise:") ? "{name} gains Expertise in {skill}."
                                                           : "{name} learns {skill}.",
                    {{"name", name}, {"skill", lesson->label, true}}}});
        return true;
    }
    }
    return false;
}

namespace
{
// What the original's hall says when it turns a member away (chargen.pas,
// TrainingMenu).
Message original_refusal(opengold::TrainingDenial denial)
{
    using opengold::TrainingDenial;
    switch (denial)
    {
    case TrainingDenial::gold:
        return {"Training costs 1000 gp.", {}};
    case TrainingDenial::other_hall:
        return {"We don't train that class here.", {}};
    case TrainingDenial::experience:
        return {"Not enough experience.", {}};
    default:
        return {"We only train conscious people.", {}};
    }
}

// The original's class slots by their stable IDs, with their names.
constexpr std::array<std::pair<std::string_view, std::string_view>, 8> original_classes{
    {{"cleric", "Cleric"},
     {"druid", "Druid"},
     {"fighter", "Fighter"},
     {"paladin", "Paladin"},
     {"ranger", "Ranger"},
     {"magic_user", "Magic-User"},
     {"thief", "Thief"},
     {"monk", "Monk"}}};
} // namespace

void RolfTourSession::show_hall_menu(std::vector<Message> dialogue, std::vector<Message> choices)
{
    snapshot_.host_dialogue = dialogue;
    snapshot_.dialogue = english(dialogue);
    snapshot_.host_choices = choices;
    snapshot_.choices.clear();
    for (const auto &choice : choices)
    {
        snapshot_.choices.push_back(english(choice));
    }
    snapshot_.diagnostic.clear();
    snapshot_.phase = TourPhase::awaiting_continue;
    snapshot_.continue_ticket = ++next_ticket_;
    ++snapshot_.revision;
}

void RolfTourSession::show_original_hall(std::vector<Message> notice)
{
    auto &visit = *hall_visit_;
    auto dialogue = std::move(notice);
    std::vector<Message> choices;
    visit.choices.clear();
    const auto add = [&](Message label, HallChoice choice)
    {
        choices.push_back(std::move(label));
        visit.choices.push_back(std::move(choice));
    };
    if (visit.member)
    {
        add({"Train", {}}, {HallChoice::Kind::confirm, visit.member});
        add({"Back", {}}, {HallChoice::Kind::back});
    }
    else
    {
        if (dialogue.empty())
        {
            dialogue.push_back({"Training costs 1000 gp. Who will train?", {}});
        }
        for (const auto id : campaign_->state().slots)
        {
            if (id)
            {
                add({"{name}", {{"name", campaign_->member(id).character.sheet().name}}},
                    {HallChoice::Kind::train, id});
            }
        }
        add({"Leave", {}}, {HallChoice::Kind::leave});
    }
    show_hall_menu(std::move(dialogue), std::move(choices));
}

bool RolfTourSession::choose_at_original_hall(const HallChoice &picked)
{
    using opengold::TrainingDenial;
    auto &visit = *hall_visit_;
    const auto &member = campaign_->member(picked.member);
    const auto name = member.character.sheet().name;
    const auto offer = campaign_->hall_training(picked.member, visit.hall);
    if (offer.denial != TrainingDenial::none)
    {
        visit.member = 0;
        show_hall({original_refusal(offer.denial)});
        return true;
    }
    if (picked.kind == HallChoice::Kind::train)
    {
        // "Gareth will become: a level 3 Fighter, a level 2 Thief."
        std::vector<Message> becomes{{"{name} will become:", {{"name", name}}}};
        const auto &levels = member.character.sheet().original->class_levels;
        for (const auto &klass : offer.classes)
        {
            const auto found = std::ranges::find(
                original_classes, klass, &std::pair<std::string_view, std::string_view>::first);
            const auto slot = static_cast<std::size_t>(found - original_classes.begin());
            becomes.push_back({"a level {level} {class}",
                               {{"level", amount(static_cast<unsigned>(levels.at(slot) + 1))},
                                {"class", std::string(found->second), true}}});
        }
        visit.member = picked.member;
        show_hall(std::move(becomes));
        return true;
    }
    // A magic-user's level learns a spell (chargen.pas, TrainingMenu).
    const bool learns = std::ranges::find(offer.classes, "magic_user") != offer.classes.end();
    const auto options =
        learns ? campaign_
                     ->advancement_options(picked.member,
                                           campaign_->default_advancement(picked.member))
                     .spells
               : std::vector<rules::AdvancementOption>{};
    if (picked.kind == HallChoice::Kind::confirm && !options.empty())
    {
        std::vector<Message> choices;
        visit.choices.clear();
        for (const auto &option : options)
        {
            choices.push_back({"{spell}", {{"spell", option.label, true}}});
            visit.choices.push_back({HallChoice::Kind::learn, picked.member, option.id});
        }
        show_hall_menu({{"Choose a spell for {name} to learn.", {{"name", name}}}},
                       std::move(choices));
        return true;
    }
    std::vector<std::string> spells;
    if (picked.kind == HallChoice::Kind::learn)
    {
        spells.push_back(picked.grant);
    }
    campaign_->train_at_hall_level(picked.member, visit.hall, std::move(spells));
    visit.member = 0;
    show_hall({{"Congratulations... {name} has trained.", {{"name", name}}}});
    return true;
}

// Podol Plaza's roaming-group roll (ECL1:18 0x9F86), from the script's own
// tables: the group's kind, record, size, leaders, speed, parley modifier and
// name, as the script would have rolled them.
void RolfTourSession::roll_plaza_group(const EclRequest &request, EclHostReply &reply)
{
    const auto table = [&](std::uint16_t base, unsigned index) -> unsigned
    {
        return machine_.variable(game_cell(EclAddress{static_cast<std::uint16_t>(base + index)}));
    };
    const unsigned pick = machine_.host_random(request.id, 9);
    const unsigned kind = table(0xB260, pick);
    constexpr std::array<std::string_view, 4> names{" GOBLINS", " ORCS", " HOBGOBLINS", " OGRES"};
    const auto name = names[pick < 4 ? 0 : pick < 7 ? 1 : pick < 8 ? 2 : 3];
    unsigned size = party_strength(*campaign_) / std::max(1u, table(0xB275, kind)) / 3;
    size += machine_.host_random(request.id, size + 1);
    // A mission party whose cover is blown meets twice as many.
    if (machine_.variable(game_cell(EclAddress{0x4A34})) == 64 &&
        machine_.variable(game_cell(EclAddress{0x4A35})) == 255)
    {
        size *= 2;
    }
    size = std::max(size, 2u);
    unsigned leaders = 0;
    if (size >= table(0xB287, kind))
    {
        leaders = table(0xB28D, kind);
        size = std::min(size, 50u);
    }
    for (const auto &[address, value] :
         std::array<std::pair<unsigned, unsigned>, 6>{{{0x4A01, size},
                                                            {0x4A02, table(0xB269, kind)},
                                                            {0x4A03, leaders},
                                                            {0x4A04, kind},
                                                            {0x4A08, table(0xB26F, kind)},
                                                            {0x4A0B, table(0xB27B, kind)}}})
    {
        reply.writes.push_back(
            {EclAddress{static_cast<std::uint16_t>(address)}, static_cast<std::uint16_t>(value)});
    }
    reply.strings.emplace_back(EclAddress{0x9800}, std::string(name));
    plaza_group_rolled_ = true;
}

bool RolfTourSession::on_map(const std::string &archive, unsigned record) const
{
    const auto here = "GEO" + std::to_string(current_area_ ? area_resources().bank : 3) + ".DAX";
    return archive == here && record == current_area_;
}

bool RolfTourSession::standing_at(const std::string &archive, unsigned record,
                                  const std::vector<std::pair<unsigned, unsigned>> &markers) const
{
    return on_map(archive, record) &&
           std::find(markers.begin(), markers.end(),
                     std::pair{snapshot_.pose.x, snapshot_.pose.y}) != markers.end();
}

void RolfTourSession::apply_entrances()
{
    if (!town_)
    {
        return;
    }
    // Sets one side of a square and the facing side of its neighbour.
    const auto set_edge = [this](unsigned x, unsigned y, MapDirection side, std::uint8_t wall,
                                 std::uint8_t door)
    {
        auto &cell = map_.cells[y * 16 + x];
        cell.walls[index(side)] = wall;
        cell.doors[index(side)] = door;
        const int nx = static_cast<int>(x) + step_x(side), ny = static_cast<int>(y) + step_y(side);
        if (nx >= 0 && ny >= 0 && nx < 16 && ny < 16)
        {
            auto &other = map_.cells[static_cast<unsigned>(ny) * 16 + static_cast<unsigned>(nx)];
            other.walls[index(reversed(side))] = wall;
            other.doors[index(reversed(side))] = door;
        }
    };
    for (const auto &shop : shops_)
    {
        if (!shop.entrance || !on_map(shop.archive, shop.record))
        {
            continue;
        }
        const auto &entrance = *shop.entrance;
        const auto state = entrances_[shop.id];
        for (const auto &[x, y] : shop.markers)
        {
            map_.cells[y * 16 + x].event_raw = 0;
            for (const auto &[side, wall] : entrance.walls)
            {
                set_edge(x, y, side, wall, 0);
            }
            if (state != EntranceState::hidden)
            {
                set_edge(x, y, entrance.secret_door, entrance.door_wall,
                         state == EntranceState::found ? locked_door : 1);
            }
        }
    }
}

const opengold::ShopSite *RolfTourSession::entrance_ahead() const
{
    const auto edge = edge_ahead(snapshot_.pose);
    for (const auto &shop : shops_)
    {
        if (!shop.entrance || !on_map(shop.archive, shop.record))
        {
            continue;
        }
        for (const auto &[x, y] : shop.markers)
        {
            const bool from_inside = edge.x == x && edge.y == y && edge.side == shop.entrance->secret_door;
            const bool from_outside = edge.next_x == x && edge.next_y == y &&
                                      edge.other == shop.entrance->secret_door;
            if (from_inside || from_outside)
            {
                return &shop;
            }
        }
    }
    return nullptr;
}

bool RolfTourSession::on_wand_trap() const
{
    return wand_traps_.contains(current_area_ * 256 + snapshot_.pose.y * 16 + snapshot_.pose.x);
}

std::optional<WandSign> RolfTourSession::point_wand_of_secrets(MemberId member, std::uint64_t item)
{
    if (!campaign_ || !can_leave())
    {
        throw std::runtime_error("The wand can be used only while exploring.");
    }
    campaign_->use_wand_of_secrets(member, item);
    // Squares count as the SRD counts them, diagonals too; of two squares as
    // near, the one fewer steps away is nearer.
    constexpr int reach = 6;
    const auto squares = [this](unsigned x, unsigned y)
    {
        const int dx = std::abs(static_cast<int>(x) - static_cast<int>(snapshot_.pose.x)),
                  dy = std::abs(static_cast<int>(y) - static_cast<int>(snapshot_.pose.y));
        return std::pair{std::max(dx, dy), dx + dy};
    };
    // A door is as near as the nearer of its two squares, from which the
    // sign names its wall.
    const auto door_sign = [&](unsigned x, unsigned y, MapDirection side)
    {
        const auto ox = static_cast<unsigned>(static_cast<int>(x) + step_x(side)),
                   oy = static_cast<unsigned>(static_cast<int>(y) + step_y(side));
        return squares(ox, oy) < squares(x, y) ? std::pair{squares(ox, oy), WandSign{ox, oy, reversed(side)}}
                                               : std::pair{squares(x, y), WandSign{x, y, side}};
    };
    std::optional<WandSign> sign;
    std::pair nearest{reach + 1, 0};
    const ScriptSecretDoor *script_door = nullptr;
    const opengold::ShopSite *shop = nullptr;
    const ScriptTrap *trap = nullptr;
    const auto consider = [&](std::pair<std::pair<int, int>, WandSign> candidate)
    {
        if (candidate.first >= nearest)
        {
            return false;
        }
        nearest = candidate.first;
        sign = candidate.second;
        script_door = nullptr;
        shop = nullptr;
        trap = nullptr;
        return true;
    };
    for (const auto &door : script_secret_doors)
    {
        const auto found = machine_.variable(game_cell(EclAddress{door.flag}));
        if (!overland() && door.map == current_area_ && (found & door.found) != door.found &&
            consider(door_sign(door.x, door.y, door.side)))
        {
            script_door = &door;
        }
    }
    for (const auto &candidate : script_traps)
    {
        const auto state =
            machine_.variable(game_cell(EclAddress{candidate.flag})) & candidate.mask;
        const auto key = candidate.map * 256 + candidate.y * 16 + candidate.x;
        if (!overland() && candidate.map == current_area_ && state != candidate.sprung &&
            !wand_traps_.contains(key) &&
            consider({squares(candidate.x, candidate.y),
                      WandSign{candidate.x, candidate.y, MapDirection::north, true}}))
        {
            trap = &candidate;
        }
    }
    for (const auto &site : shops_)
    {
        if (!site.entrance || !on_map(site.archive, site.record) ||
            entrances_[site.id] != EntranceState::hidden)
        {
            continue;
        }
        for (const auto &[x, y] : site.markers)
        {
            if (consider(door_sign(x, y, site.entrance->secret_door)))
            {
                shop = &site;
            }
        }
    }
    if (script_door)
    {
        const EclAddress flag{script_door->flag};
        machine_.bind_variable(game_cell(flag),
                               machine_.variable(game_cell(flag)) | script_door->found);
    }
    else if (trap)
    {
        wand_traps_.insert(trap->map * 256 + trap->y * 16 + trap->x);
    }
    else if (shop)
    {
        // The door now shows in the wall.
        entrances_[shop->id] = EntranceState::found;
        apply_entrances();
        ++snapshot_.picture_revision;
        ++snapshot_.revision;
    }
    return sign;
}

void RolfTourSession::search_for_secret_door()
{
    if (!campaign_)
    {
        return;
    }
    for (const auto &shop : shops_)
    {
        if (!shop.entrance || !on_map(shop.archive, shop.record) ||
            entrances_[shop.id] != EntranceState::hidden)
        {
            continue;
        }
        const auto side = shop.entrance->secret_door;
        for (const auto &[x, y] : shop.markers)
        {
            const int outside_x = static_cast<int>(x) + step_x(side),
                      outside_y = static_cast<int>(y) + step_y(side);
            if (outside_x != static_cast<int>(snapshot_.pose.x) ||
                outside_y != static_cast<int>(snapshot_.pose.y))
            {
                continue;
            }
            const auto rolls = campaign_->search_for_secret_door(event_searched_);
            for (const auto &roll : rolls)
            {
                snapshot_.secret_door_searches.push_back(
                    {campaign_->member(roll.member).character.sheet().name, roll.die, roll.needed});
            }
            if (rolls.empty() || rolls.back().die > rolls.back().needed)
            {
                return;
            }
            snapshot_.secret_door_found = true;
            entrances_[shop.id] = EntranceState::found;
            apply_entrances();
            constexpr std::array<std::string_view, 4> found{
                "You find a secret door in the north wall.", "You find a secret door in the east wall.",
                "You find a secret door in the south wall.", "You find a secret door in the west wall."};
            snapshot_.dialogue += (snapshot_.dialogue.empty() ? "" : "\n") +
                                  std::string(found[index(reversed(side))]);
            return;
        }
    }
}

void RolfTourSession::open_own_shop()
{
    for (std::size_t n = 0; n < shops_.size(); ++n)
    {
        if (!shops_[n].entrance || !standing_at(shops_[n].archive, shops_[n].record, shops_[n].markers))
        {
            continue;
        }
        choose_shopper();
        treasure_ = shop_stocks_[n];
        shop_request_ = 0;
        snapshot_.phase = TourPhase::shopping;
        snapshot_.continue_ticket = ++next_ticket_;
        snapshot_.choices.clear();
        snapshot_.diagnostic.clear();
        ++snapshot_.revision;
        return;
    }
}

void RolfTourSession::choose_shopper()
{
    if (!campaign_ || member_chosen_)
    {
        return;
    }
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        if (campaign_->state().slots[slot] &&
            campaign_->state().slots[slot] == campaign_->leader())
        {
            campaign_->select(PartySlot{slot});
        }
    }
}

void RolfTourSession::open_temple(std::uint64_t request)
{
    // The temple data/shops places on this square, else the original's list.
    auto offerings = opengold::original_temple_offerings();
    for (const auto &site : temples_)
    {
        if (standing_at(site.archive, site.record, site.markers))
        {
            offerings = site.offerings;
        }
    }
    // Resurrection is for Easy and Normal only.
    if (difficulty_ > opengold::Difficulty::normal)
    {
        std::erase_if(offerings,
                      [](const auto &offering)
                      {
                          return offering.service == opengold::rules::TempleService::resurrection;
                      });
    }
    temple_visit_ = TempleVisit{request, std::move(offerings), {}, 0, {}};
    show_temple({});
}

void RolfTourSession::show_temple(std::vector<Message> notice)
{
    using opengold::rules::TempleService;
    auto &visit = *temple_visit_;
    auto dialogue = std::move(notice);
    std::vector<Message> choices;
    visit.choices.clear();
    const auto add = [&](Message label, TempleChoice choice)
    {
        choices.push_back(std::move(label));
        visit.choices.push_back(std::move(choice));
    };
    const auto spell = [](const opengold::TempleOffering &offering)
    {
        return opengold::rules::MessageArgument{
            "spell", std::string(opengold::temple_service_info(offering.service).name), true};
    };
    if (visit.member)
    {
        const auto &offering = *visit.chosen;
        dialogue.push_back({"The clerics will cast {spell} on {name} for {fee} gp.",
                            {spell(offering),
                             {"name", campaign_->member(visit.member).character.sheet().name},
                             {"fee", amount(offering.price_gp)}}});
        add({"Pay", {}}, {TempleChoice::Kind::confirm, visit.member, offering});
        add({"Back", {}}, {TempleChoice::Kind::back});
    }
    else if (visit.chosen)
    {
        const auto &offering = *visit.chosen;
        if (dialogue.empty())
        {
            dialogue.push_back({"Choose who receives {spell}.", {spell(offering)}});
        }
        for (const auto id : campaign_->state().slots)
        {
            if (!id)
            {
                continue;
            }
            const auto name = campaign_->member(id).character.sheet().name;
            if (campaign_->temple_service_helps(id, offering.service))
            {
                add({"{name}", {{"name", name}}}, {TempleChoice::Kind::member, id, offering});
            }
            else if (offering.service == TempleService::raise_dead &&
                     campaign_->temple_service_helps(id, TempleService::resurrection))
            {
                add({"{name} (beyond Raise Dead)", {{"name", name}}},
                    {TempleChoice::Kind::beyond_raising, id, offering});
            }
        }
        add({"Back", {}}, {TempleChoice::Kind::back});
    }
    else
    {
        if (dialogue.empty())
        {
            dialogue.push_back({"The clerics will cast these spells for the party.", {}});
        }
        for (const auto &offering : visit.offerings)
        {
            add({"{spell} ({fee} gp)", {spell(offering), {"fee", amount(offering.price_gp)}}},
                {TempleChoice::Kind::service, 0, offering});
        }
        add({"Leave", {}}, {TempleChoice::Kind::leave});
    }
    snapshot_.host_dialogue = dialogue;
    snapshot_.dialogue = english(dialogue);
    snapshot_.host_choices = choices;
    snapshot_.choices.clear();
    for (const auto &choice : choices)
    {
        snapshot_.choices.push_back(english(choice));
    }
    snapshot_.diagnostic.clear();
    snapshot_.phase = TourPhase::awaiting_continue;
    snapshot_.continue_ticket = ++next_ticket_;
    ++snapshot_.revision;
}

bool RolfTourSession::choose_at_temple(std::size_t choice)
{
    auto &visit = *temple_visit_;
    const auto picked = visit.choices.at(choice);
    const auto name =
        picked.member ? campaign_->member(picked.member).character.sheet().name : std::string{};
    const opengold::rules::MessageArgument spell{
        "spell", std::string(opengold::temple_service_info(picked.offering.service).name), true};
    switch (picked.kind)
    {
    case TempleChoice::Kind::leave:
    {
        auto reply = character_reply(selected_character_);
        reply.writes.push_back({game_cell(ecl_temple_service), 0});
        if (!machine_.resume_host(visit.request, reply))
        {
            throw EclError("Temple reply rejected");
        }
        temple_visit_.reset();
        snapshot_.host_dialogue.clear();
        snapshot_.host_choices.clear();
        snapshot_.choices.clear();
        snapshot_.dialogue.clear();
        snapshot_.phase = TourPhase::running;
        ++snapshot_.revision;
        return true;
    }
    case TempleChoice::Kind::back:
        if (visit.member)
        {
            visit.member = 0;
        }
        else
        {
            visit.chosen.reset();
        }
        show_temple({});
        return true;
    case TempleChoice::Kind::service:
    {
        const auto slots = campaign_->state().slots;
        const bool needed = std::any_of(
            slots.begin(), slots.end(),
            [&](opengold::MemberId id)
            {
                return id &&
                       (campaign_->temple_service_helps(id, picked.offering.service) ||
                        (picked.offering.service == opengold::rules::TempleService::raise_dead &&
                         campaign_->temple_service_helps(
                             id, opengold::rules::TempleService::resurrection)));
            });
        if (!needed)
        {
            show_temple({{"No one in the party needs {spell}.", {spell}}});
            return true;
        }
        visit.chosen = picked.offering;
        show_temple({});
        return true;
    }
    case TempleChoice::Kind::beyond_raising:
        show_temple({{"Raise Dead cannot bring back {name}, who has been dead too long or is dust.",
                      {{"name", name}}}});
        return true;
    case TempleChoice::Kind::member:
        if (gold_value(campaign_->state().wealth) < picked.offering.price_gp)
        {
            show_temple({{"The party's gold cannot pay {fee} gp.",
                          {{"fee", amount(picked.offering.price_gp)}}}});
            return true;
        }
        visit.member = picked.member;
        show_temple({});
        return true;
    case TempleChoice::Kind::confirm:
        campaign_->temple_service(picked.member, picked.offering.service, picked.offering.price_gp);
        visit.member = 0;
        visit.chosen.reset();
        show_temple({{"The clerics cast {spell} on {name}.", {spell, {"name", name}}}});
        return true;
    }
    return false;
}

bool fights_for_party(const PhlanResources &area, unsigned record)
{
    // The record's hostile byte (Pool of Radiance's 0x10E) is the side a
    // creature fights on (coab combat_team): 0, the party's, only for
    // Valhingen Graveyard's efreeti, who helps a party carrying his bottle.
    return area.encounter_creatures.at(record).stored.hostile == 0;
}

std::string creature_name(const PhlanResources &area, unsigned record,
                          const ExportedMonster *exported)
{
    return exported ? exported->name : area.encounter_creatures.at(record).stored.name;
}

StagedCreature stage_area_creature(const PhlanResources &area, unsigned record, unsigned icon,
                                   const ExportedMonster *exported, rules::EntityId id,
                                   std::string name, rules::Side side, Difficulty difficulty,
                                   bool original_record)
{
    const auto conversion = area.conversions.find(record);
    const auto creature = area.encounter_creatures.find(record);
    if (creature == area.encounter_creatures.end() ||
        (!original_record && conversion == area.conversions.end()))
    {
        throw EclError("Encounter needs an explicit supported creature conversion: record " +
                       std::to_string(record));
    }
    // Rules that play the original's records fight the record itself.
    auto definition = original_record ? creature->second.id.key() : conversion->second.definition;
    // A leader readies a bow only when its art shows one: of the Slums combat
    // icons, only the orc leader's icon 5 does.
    if (definition == "slums-orc-leader" && icon == 5)
    {
        definition = "slums-orc-leader-archer";
    }
    auto art = creature_art(area.combat_archive, icon, exported, definition.ends_with("-archer"));
    if (!art)
    {
        throw EclError("Invalid original combat icon");
    }
    art->entity = id;
    return {staged_fighter(id, std::move(definition), std::move(name), side, creature->second,
                           exported, difficulty),
            std::move(*art)};
}

} // namespace opengold::por
