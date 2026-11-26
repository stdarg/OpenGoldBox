#include "opengold/rolf_tour.h"
#include "area_loading.h"
#include "dark_queen_campaign.h"
#include "host_services.h"
#include "opengold/original_spells.h"
#include "opengold/original_rules.h"
#include "opengold/directions.h"
#include "opengold/diagnostic_log.h"
#include "opengold/creature_art.h"
#include "opengold/script_secrets.h"
#include "opengold/random_treasure.h"
#include "opengold/ecl_party_host.h"
#include "opengold/encounter_formation.h"
#include "opengold/scroll_key.h"
#include "opengold/authored_items.h"
#include "opengold/companions.h"
#include "opengold/game_traits.h"
#include <algorithm>
#include <set>

namespace opengold::por
{
using namespace host_services;

// Arrow volleys come from Norris the Gray's hidden kobold archers: each arrow is
// an attack roll with the SRD 5.2.1 Kobold Warrior's +4 ranged bonus.
constexpr int hidden_archer_attack_bonus = 4;
// The time a step takes, which ECL CLOCK also counts in.
constexpr std::chrono::seconds step_time{6};
// The 256-color engine's PLAY SOUND, a host command only there
// (docs/games/darkness.md, Scripts).
constexpr std::uint8_t vga_play_sound_command = 0x2C;
// The three commands Treasures of the Savage Frontier adds
// (docs/games/treasure.md, Scripts).
constexpr std::uint8_t treasures_load_wall_sets_command = 0x42,
                       treasures_find_named_item_command = 0x43,
                       treasures_take_named_item_command = 0x44;

const PhlanResources &RolfTourSession::area_resources() const
{
    if (game() == GoldBoxGame::dark_queen_of_krynn)
    {
        return *town_;
    }
    // Krynn's wilderness and outposts retain the previous dungeon map, but
    // their encounters, treasure and combat art come from disk 1.
    if ((game() == GoldBoxGame::champions_of_krynn ||
         game() == GoldBoxGame::death_knights_of_krynn) &&
        (current_script_ == 16 || current_script_ == 17))
    {
        return *town_;
    }
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
    if (event_checkpoint_ && event_checkpoint_->area == current_area_ &&
        snapshot_.pose.x < map_.width && snapshot_.pose.y < map_.height)
    {
        const auto arrival = snapshot_.pose.y * map_.width + snapshot_.pose.x;
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
    if (game() == GoldBoxGame::pool_of_radiance || game() == GoldBoxGame::dark_queen_of_krynn ||
        wall_art_.appearances.empty())
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

// The script commands the party answers; Curse adds DUMP (62) and DESTROY
// ITEMS (64), Secret AWARD EXPERIENCE (65).
void RolfTourSession::enable_party_commands()
{
    if (is_buck_rogers(game()))
    {
        enable_buck_rogers_party_commands();
        return;
    }
    const bool later = later_ecl_commands(game());
    for (const std::uint8_t op :
         std::initializer_list<std::uint8_t>{11, 29, 30, 34, 35, 41, 46, 52, 54, 59})
    {
        // The 256-color engine's 1Dh, 22h, 23h and 29h are commands the machine
        // runs itself (WAIT KEY, YES NO, CLEAR AND EXIT, unused).
        if (later && (op == 29 || op == 34 || op == 35 || op == 41))
        {
            continue;
        }
        machine_.enable_host(op);
    }
    if (game() != GoldBoxGame::pool_of_radiance)
    {
        machine_.enable_host(62);
        machine_.enable_host(64);
    }
    if (game() == GoldBoxGame::secret_of_the_silver_blades || later ||
        game() == GoldBoxGame::champions_of_krynn)
    {
        machine_.enable_host(65);
    }
    if (game() == GoldBoxGame::dark_queen_of_krynn)
    {
        machine_.enable_host(63);
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

// The 256-color engine's scripts (Pools of Darkness's and Treasures') read a
// fight's result with IF right after COMBAT: its PostCombat sets "equal" when
// the party won or came through, and "not equal" when it fled (postcbt.pas).
// The other games keep the flags and leave the result in a cell.
[[nodiscard]] std::optional<EclConditions> vga_combat_conditions(GoldBoxGame game, bool party_stood)
{
    if (!vga_engine(game))
    {
        return std::nullopt;
    }
    return EclConditions{party_stood, !party_stood, false, false, false, false};
}

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

Edge edge_ahead(const GeoMap &map, PartyPose pose)
{
    const int x = static_cast<int>(pose.x) + step_x(pose.facing),
              y = static_cast<int>(pose.y) + step_y(pose.facing);
    return {pose.x, pose.y, pose.facing, map.wrapped_x(x), map.wrapped_y(y), reversed(pose.facing)};
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
constexpr std::array original_save_abilities{
    rules::Ability::constitution, rules::Ability::constitution, rules::Ability::dexterity,
    rules::Ability::dexterity, rules::Ability::wisdom};
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
    static constexpr std::array types{
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
// What the camp says when its rest is over: under rules that rest any
// length, how long the party rested.
std::string rest_complete_line(const RestResult &rested, bool any_length)
{
    if (any_length && rested.kind == RestKind::long_rest)
    {
        constexpr std::uint64_t day = 24 * 60, hour = 60;
        return "The party rests " + std::to_string(rested.duration_minutes / day) + " days, " +
               std::to_string(rested.duration_minutes % day / hour) + " hours and " +
               std::to_string(rested.duration_minutes % hour) + " minutes.";
    }
    return rested.kind == RestKind::short_rest
               ? "Short rest complete: one hour passed; eligible members can spend Hit Dice."
               : "Long rest complete: eight hours passed; eligible members recovered HP and "
                 "supported resources.";
}

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

} // namespace

unsigned open_squares_ahead(const GeoMap &map, PartyPose pose)
{
    unsigned squares = 0;
    while (squares < 2 && !map.at(pose.x, pose.y).walls[index(pose.facing)])
    {
        ++squares;
        const auto ahead = edge_ahead(map, pose);
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
    // Each cell where the campaign's game keeps it (game_address): Pools of
    // The 256-color engine keeps its named cells apart, not in runs.
    const auto bind_range = [this](std::uint16_t first, std::uint16_t last)
    {
        for (auto a = first; a <= last; ++a)
        {
            machine_.bind_variable(game_cell(EclAddress{a}), 0);
        }
    };
    const bool pool_of_radiance = game() == GoldBoxGame::pool_of_radiance;
    // Curse's scripts keep their state anywhere in the game's zone (a
    // marauders' ambush marks 4DCBh); Pool of Radiance's use its first half.
    bind_range(0x4900, pool_of_radiance ? 0x4AFF : 0x4CFF);
    bind_range(0x6B00, 0x6C1C);
    bind_range(0x6DA8, 0x6EFF);
    bind_range(0x9800, 0x98FF);
    // The 256-color engine's own memory: its game cells and the current
    // character's record (docs/games/darkness.md, Script memory).
    if (vga_engine(game()))
    {
        constexpr std::uint16_t first_cell = 0x0001, last_cell = 0x0800;
        for (auto a = first_cell; a <= last_cell; ++a)
        {
            machine_.bind_variable(EclAddress{a}, 0);
        }
    }
    machine_.bind_variable(game_cell(EclAddress{0xB8}), 0);
    machine_.bind_variable(game_cell(EclAddress{0xB9}), 0);
    machine_.bind_variable(game_cell(ecl_clock_hour), 12);
    machine_.bind_variable(game_cell(ecl_clock_day), 1);
    if (game() == GoldBoxGame::dark_queen_of_krynn)
    {
        machine_.bind_variable(EclAddress{0x34}, 7);
        machine_.bind_variable(EclAddress{0x35}, 13);
        machine_.bind_variable(EclAddress{0x22}, 1);
        machine_.bind_variable(EclAddress{0x27}, 3);
        for (std::uint16_t moon = 0x1FA; moon <= 0x1FC; ++moon)
        {
            machine_.bind_variable(EclAddress{moon}, 2);
        }
    }
    if (game() == GoldBoxGame::champions_of_krynn)
    {
        // GameInit's original starting wilderness square.
        machine_.bind_variable(game_cell(ecl_overland_x), 7);
        machine_.bind_variable(game_cell(ecl_overland_y), 13);
    }
    // The disk the opening script came from: New Phlan's 3 in Pool of Radiance.
    machine_.bind_variable(game_cell(ecl_script_bank),
                           pool_of_radiance ? 3 : static_cast<std::uint16_t>(town_->bank));
    machine_.bind_variable(game_cell(ecl_party_size), 1);
    // The view's wall and ceiling/floor colour selectors (the reconstructions'
    // WriteGbvm), which Kuto's Well's setup writes when its plaza or catacombs
    // load; stored for the script, with no effect yet. Secret's disk 3 scripts
    // also write C05Ah, which its original ignores.
    machine_.bind_variable(game_cell(EclAddress{0xC059}), 0);
    machine_.bind_variable(game_cell(EclAddress{0xC05A}), 0);
    machine_.bind_variable(game_cell(EclAddress{0xC05F}), 0);
    // Gateway's scripts also name the party's facing 3B2h (its ReadGbvm and
    // WriteGbvm).
    if (game() == GoldBoxGame::gateway_to_the_savage_frontier)
    {
        machine_.alias_cell(EclAddress{0x3B2}, game_cell(ecl_party_facing));
    }
    for (const auto address : {ecl_overland_direction, ecl_overland_target_x, ecl_overland_target_y,
                               ecl_overland_terrain, ecl_overland_tile})
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
        // The Buck Rogers games' 3Ch does nothing and never ends (no script
        // reaches it); their own commands are enabled with the party's.
        if (op == 60 && is_buck_rogers(game()))
        {
            continue;
        }
        machine_.enable_host(op);
    }
    if (later_ecl_commands(game()))
    {
        machine_.enable_host(vga_play_sound_command);
    }
    if (vga_engine(game()))
    {
        // Address 0 lies outside the 256-color engine's script memory: it
        // reads 0 and keeps nothing (its GbvmZone), as Kalistes' Temple's OR
        // that stores to it after the inner chamber's fight needs (ECL1:72,
        // 988Bh).
        machine_.ignore_cells(EclAddress{0}, EclAddress{0});
    }
    if (game() == GoldBoxGame::treasures_of_the_savage_frontier)
    {
        for (const auto op : {treasures_load_wall_sets_command, treasures_find_named_item_command,
                              treasures_take_named_item_command})
        {
            machine_.enable_host(op);
        }
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
    if (is_buck_rogers(game()))
    {
        configure_buck_rogers();
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

RolfTourSession::GameClock RolfTourSession::game_clock() const
{
    if (!campaign_)
    {
        return {};
    }
    const auto minutes = campaign_->state().time_minutes;
    const auto minute_of_day = clock_minute_of_day(minutes);
    const auto days = minutes / 1440 + (minutes % 1440 + 720) / 1440;
    return {static_cast<unsigned>((days / 360) % 256), static_cast<unsigned>((days / 30) % 12 + 1),
            static_cast<unsigned>(days % 30 + 1), static_cast<unsigned>(minute_of_day / 60),
            static_cast<unsigned>(minute_of_day % 60)};
}

EclHostReply RolfTourSession::clock_reply() const
{
    EclHostReply reply;
    if (!campaign_)
    {
        return reply;
    }
    const auto clock = game_clock();
    reply.writes = {
        {game_cell(ecl_clock_minute_ones), static_cast<std::uint16_t>(clock.minute % 10)},
        {game_cell(ecl_clock_minute_tens), static_cast<std::uint16_t>(clock.minute / 10)},
        {game_cell(ecl_clock_hour), static_cast<std::uint16_t>(clock.hour)},
        {game_cell(ecl_clock_day), static_cast<std::uint16_t>(clock.day)},
        {game_cell(ecl_clock_month), static_cast<std::uint16_t>(clock.month)},
        {game_cell(ecl_clock_year), static_cast<std::uint16_t>(clock.year)}};
    if (game() == GoldBoxGame::death_knights_of_krynn)
    {
        for (const auto id : campaign_->state().slots)
        {
            if (!id || !campaign_->member(id).character.sheet().original)
            {
                continue;
            }
            const auto &numbers = *campaign_->member(id).character.sheet().original;
            for (std::size_t moon = 0; moon < 3; ++moon)
            {
                reply.writes.push_back(
                    {EclAddress{static_cast<std::uint16_t>(0x4CF9 + moon)},
                     static_cast<std::uint16_t>(numbers.krynn_moon_phases[moon])});
                reply.writes.push_back(
                    {EclAddress{static_cast<std::uint16_t>(0x4CFC + moon)},
                     static_cast<std::uint16_t>(numbers.krynn_moon_counters[moon])});
            }
            break;
        }
    }
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
    // A Buck Rogers character is its record (buck_rogers_host.cpp).
    if (campaign_ && is_buck_rogers(game()))
    {
        EclHostReply reply;
        add_buck_rogers_record(slot, reply);
        const auto &slots = campaign_->state().slots;
        reply.writes.push_back(
            {game_cell(ecl_party_size),
             static_cast<std::uint16_t>(std::ranges::count_if(slots,
                                                              [](MemberId id)
                                                              {
                                                                  return id != MemberId{};
                                                              }))});
        return reply;
    }
    if (campaign_)
    {
        auto reply = party_character_reply(*campaign_, slot);
        // The view's cells are Pool of Radiance's; the game keeps them elsewhere.
        for (auto &write : reply.writes)
        {
            write.address = game_cell(write.address);
        }
        const bool curse_record = game() == GoldBoxGame::curse_of_the_azure_bonds ||
                                  game() == GoldBoxGame::gateway_to_the_savage_frontier ||
                                  game() == GoldBoxGame::champions_of_krynn ||
                                  game() == GoldBoxGame::death_knights_of_krynn;
        if (curse_record && slot.index < 8)
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
                if (game() == GoldBoxGame::champions_of_krynn && member.character.sheet().original)
                {
                    const auto &sheet = member.character.sheet();
                    const auto &n = *sheet.original;
                    at(0x19) = static_cast<std::uint16_t>(sheet.scores[rules::Ability::charisma]);
                    at(0x61) = static_cast<std::uint16_t>(n.knight_order);
                    at(0x72) = static_cast<std::uint16_t>(
                        original::record_race(static_cast<original::Race>(n.race), game()));
                    at(0x73) = static_cast<std::uint16_t>(
                        n.profession == static_cast<int>(original::Profession::knight)
                            ? 7
                            : n.profession);
                    at(0x76) = static_cast<std::uint16_t>(sheet.hit_points);
                    at(0x9B) = static_cast<std::uint16_t>(n.saving_throws[1]);
                    at(0xA0) = static_cast<std::uint16_t>(sheet.level);
                    at(0xC9) = static_cast<std::uint16_t>(n.class_levels[5]);
                    at(0xCC) = static_cast<std::uint16_t>(n.class_levels[2]);
                    at(0xCD) = static_cast<std::uint16_t>(n.class_levels[7]);
                    at(0xD0) = static_cast<std::uint16_t>(n.class_levels[4]);
                }
                if (game() == GoldBoxGame::death_knights_of_krynn &&
                    member.character.sheet().original)
                {
                    const auto &sheet = member.character.sheet();
                    const auto &n = *sheet.original;
                    static constexpr std::array abilities{
                        rules::Ability::strength,     rules::Ability::intelligence,
                        rules::Ability::wisdom,       rules::Ability::dexterity,
                        rules::Ability::constitution, rules::Ability::charisma};
                    for (std::size_t ability = 0; ability < abilities.size(); ++ability)
                    {
                        at(static_cast<std::uint16_t>(0x14 + ability)) =
                            static_cast<std::uint16_t>(sheet.scores[abilities[ability]]);
                        at(static_cast<std::uint16_t>(0x65 + ability)) =
                            static_cast<std::uint16_t>(sheet.base[abilities[ability]]);
                    }
                    at(0x1A) = at(0x6B) = static_cast<std::uint16_t>(n.exceptional_strength);
                    at(0x71) = static_cast<std::uint16_t>(60 - n.thac0);
                    at(0x72) = static_cast<std::uint16_t>(
                        original::record_race(static_cast<original::Race>(n.race), game()));
                    at(0x74) = static_cast<std::uint16_t>(n.age);
                    at(0x76) = static_cast<std::uint16_t>(sheet.hit_points);
                    at(0xBB) = static_cast<std::uint16_t>(n.robe_order);
                    at(0xBC) = static_cast<std::uint16_t>(n.deity);
                    at(0xBD) = at(0xC1) = 0;
                    at(0xBF) = static_cast<std::uint16_t>(n.knight_order);
                    for (std::size_t save = 0; save < n.saving_throws.size(); ++save)
                    {
                        at(static_cast<std::uint16_t>(0x9A + save)) =
                            static_cast<std::uint16_t>(n.saving_throws[save]);
                    }
                    for (std::size_t skill = 0; skill < n.thief_skills.size(); ++skill)
                    {
                        at(static_cast<std::uint16_t>(0xA5 + skill)) =
                            static_cast<std::uint16_t>(n.thief_skills[skill]);
                    }
                    static constexpr std::array<std::uint16_t, 8> masks{2,   32, 8, 64,
                                                                        128, 1,  4, 16};
                    for (std::size_t cls = 0; cls < n.class_levels.size(); ++cls)
                    {
                        if (n.class_levels[cls] > 0)
                        {
                            at(0xEB) |= masks[cls];
                        }
                    }
                    const auto profile = campaign_->profile(id);
                    at(0x9F) = at(0x11A) = static_cast<std::uint16_t>(profile.movement_feet / 5);
                    at(0x10E) = static_cast<std::uint16_t>(40 + profile.melee_attack_bonus);
                    at(0x10F) = at(0x110) = static_cast<std::uint16_t>(60 - profile.armor_class);
                    const auto spells = campaign_->memorized_spells(id);
                    for (std::uint16_t spell_slot = 0; spell_slot < 74; ++spell_slot)
                    {
                        const auto number = spell_slot < spells.size()
                                                ? original::spell_number(spells[spell_slot], game())
                                                : std::nullopt;
                        at(static_cast<std::uint16_t>(0x1B + spell_slot)) =
                            number ? static_cast<std::uint16_t>(
                                         original::native_spell(*number, game()))
                                   : 0;
                    }
                }
                // Gateway's also read the Dexterity, which a climb rolls
                // against (the Kraken cell's grating, ECL2:2 0x8798).
                if (game() == GoldBoxGame::gateway_to_the_savage_frontier)
                {
                    at(0x17) = static_cast<std::uint16_t>(
                        member.character.sheet().scores[rules::Ability::dexterity]);
                }
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
        if (vga_engine(game()))
        {
            add_vga_record(slot, reply);
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
    if (campaign_ && is_buck_rogers(game()))
    {
        read_buck_rogers_character();
        return;
    }
    if (campaign_)
    {
        if (krynn_game(game()))
        {
            // Champions' SPELL only locates a prepared copy. The script's
            // later write into its selected character's spell window spends
            // that copy, after the player has actually chosen to cast it.
            const auto id = selected_character_.index < 8
                                ? campaign_->state().slots[selected_character_.index]
                                : MemberId{};
            const auto spells = id ? campaign_->memorized_spells(id) : std::vector<std::string>{};
            const bool death_knights = game() == GoldBoxGame::death_knights_of_krynn;
            const std::uint16_t start = death_knights ? 0x6B1B : 0x6B20;
            const bool dark_queen = game() == GoldBoxGame::dark_queen_of_krynn;
            const std::uint16_t count = dark_queen ? 137 : death_knights ? 74 : 58;
            for (std::uint16_t n = 0; n < count; ++n)
            {
                const auto address =
                    dark_queen ? EclAddress{static_cast<std::uint16_t>(0x41F + n)}
                               : game_cell(EclAddress{static_cast<std::uint16_t>(start + n)});
                if (!machine_.consume_script_write(address))
                {
                    continue;
                }
                const auto written = machine_.variable(address);
                // Native scripts mark a prepared entry used by setting
                // its high bit (the New Aurim prisoner's cure, ECL31:94A2).
                // ECL59's magic door can also overwrite a different entry
                // with a spent Dispel Magic byte after reusing its index.
                // Native hero_write accepts that replacement; consume the
                // overwritten entry, without adding a new available spell.
                const bool native_spent = dark_queen && (written & 0x80) &&
                                          original::canonical_spell(written & 0x7F, game()) != 0;
                if (written != 0 && (!dark_queen || written != 255) && !native_spent)
                {
                    throw EclError("Unsupported prepared spell write");
                }
                if (n < spells.size() && !campaign_->cast_exploration_spell(spells[n], id))
                {
                    throw EclError("Selected character cannot spend the prepared spell");
                }
            }
        }
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

std::uint16_t RolfTourSession::native_cell_flags(unsigned x, unsigned y) const
{
    const auto squares = std::size_t{map_.width} * map_.height;
    if (x >= map_.width || y >= map_.height || map_.raw.size() != 8 + 4 * squares)
    {
        return 0;
    }
    // Native GEO's last plane interleaves door bits with two floor bits.
    // area_cell_flags exposes the latter as world byte 3Dh; the tower uses
    // them to distinguish ordinary floors, shafts and flaming areas.
    const auto flags = map_.raw[8 + 3 * squares + y * map_.width + x];
    return static_cast<std::uint16_t>(((flags & 8) >> 2) | ((flags & 2) >> 1));
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
    if (game() == GoldBoxGame::dark_queen_of_krynn)
    {
        machine_.bind_variable(EclAddress{0x3D}, native_cell_flags(pose.x, pose.y));
    }
    // Gateway's scripts read the barrier ahead (its RefreshCurrentMapCell): the
    // side's barrier where there is a wall or on the overland, else open (1).
    if (game() == GoldBoxGame::gateway_to_the_savage_frontier)
    {
        constexpr EclAddress barrier_ahead{0x4CF6};
        const auto side = index(pose.facing);
        machine_.bind_variable(barrier_ahead,
                               cell.walls[side] || gateway_overland() ? cell.doors[side] : 1);
    }
    publish_pose();
}

unsigned RolfTourSession::room_ahead() const
{
    // Gateway's overland always has room for 2 (its MaxMonsterDist).
    return gateway_overland() ? 2 : open_squares_ahead(map_, snapshot_.pose);
}

bool RolfTourSession::gateway_overland() const
{
    return game() == GoldBoxGame::gateway_to_the_savage_frontier &&
           machine_.variable(game_cell(ecl_in_3d_area)) == 0;
}

void RolfTourSession::load_overland_symbols(const std::array<unsigned, 3> &banks)
{
    constexpr std::size_t slot_symbols = 70;
    WallTiles symbols(slot_symbols * banks.size());
    for (std::size_t slot = 0; slot < banks.size(); ++slot)
    {
        if (banks[slot] == detail::no_wall_bank)
        {
            continue;
        }
        const auto set = town_->symbol_sets.find(banks[slot]);
        if (set == town_->symbol_sets.end() || set->second.size() < slot_symbols)
        {
            throw EclError("Unable to load symbol set " + std::to_string(banks[slot]));
        }
        std::copy_n(set->second.begin(), slot_symbols,
                    symbols.begin() + static_cast<std::ptrdiff_t>(slot * slot_symbols));
    }
    overland_symbols_ = std::move(symbols);
    ++snapshot_.revision;
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
        const auto wrapped_x = map_.wrapped_x(x), wrapped_y = map_.wrapped_y(y);
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
        // Gateway's overland passes only an open way (barrier 1), with or
        // without a wall (its GetBarrierType).
        const bool stopped = gateway_overland()
                                 ? a.doors[index(side)] != 1
                                 : blocked(a.walls[index(side)], a.doors[index(side)]);
        // Treasures keeps the square the party stood on before every step,
        // whether or not it moves (its main loop sets savedX and savedY
        // before ResolveMovement): its overland's script, finding the party
        // still there, moves it by the square's terrain itself.
        const bool keeps_every_step = game() == GoldBoxGame::treasures_of_the_savage_frontier;
        if (stopped && !keeps_every_step)
        {
            return false;
        }
        machine_.bind_variable(game_cell(EclAddress{0x49F0}), static_cast<std::uint16_t>(pose.x));
        machine_.bind_variable(game_cell(EclAddress{0x49F1}), static_cast<std::uint16_t>(pose.y));
        if (stopped)
        {
            return false;
        }
        pose.x = wrapped_x;
        pose.y = wrapped_y;
        pick_tried_ = false;
        ++snapshot_.footsteps;
        sound_effect_played();
    }
    if (campaign_ && command == ExplorationCommand::forward)
    {
        // A step in Search mode takes ten minutes more (coab MovePartyForward).
        if (game() == GoldBoxGame::death_knights_of_krynn ||
            game() == GoldBoxGame::dark_queen_of_krynn)
        {
            pass_time(std::chrono::minutes{snapshot_.searching ? 10 : 1});
        }
        else
        {
            pass_time(step_time);
            if (snapshot_.searching)
            {
                pass_time(std::chrono::minutes{10});
            }
        }
    }
    bind_pose(pose);
    synchronize_clock();
    return true;
}

void RolfTourSession::begin_event(EventTrigger trigger, std::optional<std::size_t> entry,
                                  std::chrono::minutes takes)
{
    // The original refreshes the current square before accepting the next
    // command. A Treasure script may have moved the party within the map
    // without updating the event/wall cells (Leilon's farmhouse stairs).
    // Preserve the overland scripts' intentional off-map coordinates.
    if (game() == GoldBoxGame::treasures_of_the_savage_frontier &&
        machine_.variable(game_cell(ecl_party_x)) < map_.width &&
        machine_.variable(game_cell(ecl_party_y)) < map_.height)
    {
        bind_pose(snapshot_.pose);
    }
    // The Buck Rogers scripts read the difficulty level from zone 0.
    if (is_buck_rogers(game()))
    {
        // A retained-map passage can SAVE new coordinates without redrawing
        // the view (Matrix97:830Eh ->96). Refresh the actual 3D square before
        // the next command reads its wall/event; idle and restored saves keep
        // their original bytes until that command. Orbital scenes and native
        // VGA wilderness coordinates use their separate publication paths.
        const auto kind = machine_.variable(EclAddress{0x4BE6});
        if (kind == 0x10 || kind == 0x50)
        {
            const auto x = machine_.variable(game_cell(ecl_party_x));
            const auto y = machine_.variable(game_cell(ecl_party_y));
            const auto facing = machine_.variable(game_cell(ecl_party_facing));
            if (x < map_.width && y < map_.height && facing < 4)
            {
                bind_pose({x, y, static_cast<MapDirection>(facing)});
            }
        }
        // Zone2 starts at7A00h. The shared setup binds its upper half;
        // ordinary dock labels also SAVE/PRINT strings in its lower half.
        // Initialize missing cells at the first event, including after an
        // older save, while keeping existing scratch data and idle save bytes.
        for (std::uint16_t address = 0x7A00; address < 0x7B00; ++address)
        {
            try
            {
                (void)machine_.variable(EclAddress{address});
            }
            catch (const EclError &)
            {
                machine_.bind_variable(EclAddress{address}, 0);
            }
        }
        machine_.bind_variable(EclAddress{0x4BB6}, buck_rogers_difficulty_level());
    }
    if (const auto won = traits(game()).won_cell)
    {
        won_cell_at_event_start_ = machine_.variable(*won);
    }
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
    snapshot_.host_dialogue.clear();
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
                                        .dark_queen_combat_set = dark_queen_combat_set_,
                                        .dark_queen_vault_money = dark_queen_vault_money_,
                                        .dark_queen_vault_items = dark_queen_vault_items_,
                                        .map = map_,
                                        .pending_loot = pending_loot_,
                                        .snapshot = snapshot_,
                                        .selected_character = selected_character_};
    if (campaign_ && takes.count() > 0)
    {
        pass_time(takes);
        synchronize_clock();
    }
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
        off_map = (x < 0 || y < 0 || x >= static_cast<int>(map_.width) ||
                   y >= static_cast<int>(map_.height)) &&
                  cell.doors[index(p.facing)] <= 1 &&
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
    // A camp made from Treasures' open camp menu ran its entry and told the
    // romance as the menu opened (open_camp): it goes straight on to its rest.
    camp_from_menu_ = std::exchange(camp_menu_open_, false) && trigger == EventTrigger::camp;
    snapshot_.camp_menu_open = camp_opening_ = false;
    if (camp_from_menu_)
    {
        camp_resumes_ = true;
    }
    else if (!machine_.start(entry.value_or(static_cast<std::size_t>(trigger))))
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
    // As NEW ECL does, Gateway's script starts in a dungeon.
    if (game() == GoldBoxGame::gateway_to_the_savage_frontier)
    {
        machine_.bind_variable(game_cell(ecl_in_3d_area), 1);
    }
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

const std::vector<AnimationFrame> *RolfTourSession::monster_picture() const
{
    if (!showing_monster_picture_)
    {
        return nullptr;
    }
    return pic_frames(*monster_picture_id_);
}

const std::vector<AnimationFrame> *RolfTourSession::pic_frames(unsigned id) const
{
    const auto &animations = area_resources().animations;
    const auto own = animations.find(id);
    if (own == animations.end())
    {
        return nullptr;
    }
    constexpr EclAddress recoloured{0x4E}, colors_record{0x4F};
    if (game() != GoldBoxGame::treasures_of_the_savage_frontier ||
        machine_.variable(recoloured) != id)
    {
        return &own->second;
    }
    const auto colors = town_->picture_colors.find(machine_.variable(colors_record));
    const auto record = town_->pic_records.find(id);
    if (colors == town_->picture_colors.end() || record == town_->pic_records.end())
    {
        return &own->second;
    }
    if (!recoloured_picture_ || recoloured_picture_->picture != id ||
        recoloured_picture_->colors != colors->first)
    {
        // The color record's run is laid over the picture's own colors.
        auto table = vga_color_table(record->second.palette);
        const auto &run = colors->second;
        for (std::size_t n = 0; n < run.colors.size() && run.first + n < table.size(); ++n)
        {
            table[run.first + n] = run.colors[n];
        }
        std::vector<AnimationFrame> frames;
        for (const auto &frame : record->second.frames)
        {
            frames.push_back({frame.delay, vga_rgba(frame.image, table)});
        }
        recoloured_picture_ = RecolouredPicture{id, colors->first, std::move(frames)};
    }
    return &recoloured_picture_->frames;
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
    return record ? pic_frames(*record) : nullptr;
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
    auto art = creature_art(area_resources().combat_archive, 11, nullptr, false, game());
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
    unsigned experience = w[0] / 200 + w[1] / 20 + w[2] / 2 + w[3] +
                          w[4] * traits(game()).gem_experience + w[5] * 250 + w[6] * 2200;
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
    const auto members =
        std::ranges::count_if(campaign_->state().slots,
                              [this](MemberId id)
                              {
                                  if (!id)
                                  {
                                      return false;
                                  }
                                  if (game() != GoldBoxGame::dark_queen_of_krynn)
                                  {
                                      return true;
                                  }
                                  const auto &member = campaign_->member(id);
                                  return campaign_->rule_module().receives_experience(
                                      member.character.sheet(), member.vitals);
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

namespace
{
// The DOS 1.1 finale reuses Xoham's monster-count cell as the index of its
// recovery loop, without initializing it. Recognize that exact loop before
// supplying its missing initial value with the final combat's reply.
bool darkness_recovery_uses_monster_count(const EclProgram &program)
{
    try
    {
        const auto load = program.instruction(0x95BF);
        const auto increment = program.instruction(0x95D0);
        return ecl_opcode(load.opcode, GoldBoxGame::pools_of_darkness).name == "LOAD CHARACTER" &&
               load.operands.size() == 1 && load.operands[0].tag == 1 &&
               load.operands[0].value == 0xC4 &&
               ecl_opcode(increment.opcode, GoldBoxGame::pools_of_darkness).name == "ADD" &&
               increment.operands.size() == 3 && increment.operands[0].tag == 0 &&
               increment.operands[0].value == 1 && increment.operands[1].tag == 1 &&
               increment.operands[1].value == 0xC4 && increment.operands[2].tag == 1 &&
               increment.operands[2].value == 0xC4;
    }
    catch (const EclError &)
    {
        return false;
    }
}
} // namespace

bool RolfTourSession::resolve_combat(const rules::Snapshot &result)
{
    if (is_buck_rogers(game()))
    {
        return resolve_buck_rogers_combat(result);
    }
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
                (result.outcome == rules::Outcome::victory && unit.hit_points > 0 && !unit.fled &&
                 !result.round_limit_reached))
            {
                return false;
            }
            continue;
        }
        // A creature that fought beside the party until it was attacked, or
        // until a spell charmed it onto the monsters' side (Treasures'
        // dwarves in Freezefire's lair).
        if (unit.side == rules::Side::opposition && opengold::is_ally(unit.id))
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
            // down may be below 0 Hit Points, or unconscious with Hit Points
            // healed into it, which leave it out of the fight (fxengine.pas,
            // RestoreHitPoints). The rules' death state remains authoritative
            // when a lasting effect leaves positive Hit Points on a dead foe.
            if (unit.dead || unit.hit_points <= 0 || unit.surrendered ||
                unit.status == "unconscious")
            {
                ++defeated;
            }
            // One that fled pays nothing (postcbt.pas, DetermineLootExp), and
            // nor does one still standing when the fight ended for want of
            // attacks: it leaves the field.
            else if (unit.fled || result.round_limit_reached)
            {
                unpaid_records.push_back(staged_records_.at(unit.id - 1000));
            }
        }
        else if (unit.side == rules::Side::party && opengold::is_ally(unit.id))
        {
            continue;
        }
        // A monster still charmed onto the party's side as the fight ends
        // leaves as one that ran (the original's CombatDoneCleanup), worth
        // neither experience nor treasure.
        else if (unit.side == rules::Side::party && unit.id >= 1000 &&
                 unit.id < 1000 + staged_records_.size() && !expected_party.contains(unit.id))
        {
            if (!enemies.insert(unit.id).second)
            {
                return false;
            }
            unpaid_records.push_back(staged_records_.at(unit.id - 1000));
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
    // A fight won as the last member died leaves no one to go on: the party
    // is lost, as the original's game ends with its party dead.
    const bool none_left = std::ranges::none_of(campaign_->state().slots,
                                                [this](opengold::MemberId id)
                                                {
                                                    return id && !campaign_->member(id).vitals.dead;
                                                });
    const bool native_recovery = game() == GoldBoxGame::dark_queen_of_krynn &&
                                 machine_.variable(EclAddress{0x1D}) != 0 && !none_left;
    if ((result.outcome == rules::Outcome::defeat && !duel_lost && !native_recovery) ||
        (!champion && none_left))
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
    const bool native_lost = native_recovery && result.outcome == rules::Outcome::defeat;
    const bool pays_experience =
        game() != GoldBoxGame::dark_queen_of_krynn || machine_.variable(EclAddress{0x1F}) == 0;
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
            // Original members left on the field are lost regardless of a
            // charm changing their side (Native postcmbt.c, hero_drop). Lent
            // allies and charmed encounter monsters are not original members.
            for (const auto id : expected_party)
            {
                const auto unit =
                    std::ranges::find(result.combatants, id, &rules::CombatantView::id);
                if (unit == result.combatants.end() || !unit->fled)
                {
                    const auto name = campaign_->member(id).character.sheet().name;
                    campaign_->lose(id);
                    snapshot_.dialogue += "\n" + name + " is left behind and lost.";
                }
            }
            // The script's chosen member is one still in the party.
            if (!campaign_->state().slots.at(selected_character_.index))
            {
                selected_character_ = campaign_->state().selected_slot;
            }
            unsigned experience = 0;
            for (const auto &unit : result.combatants)
            {
                if (unit.side == rules::Side::opposition && enemies.contains(unit.id) &&
                    unit.hit_points <= 0 && staged_records_.at(unit.id - 1000) != mirror_record)
                {
                    experience += record_experience(staged_records_.at(unit.id - 1000));
                }
            }
            if (experience && pays_experience)
            {
                campaign_->award_experience(experience_share(experience), reward);
            }
        }
        else if (duel_lost || native_lost)
        {
            // No experience, and the opponent keeps its gear.
            if (duel_lost)
            {
                campaign_->recover_duelist(champion);
            }
            snapshot_.dialogue = "You have lost the fight.";
        }
        else if (mirror)
        {
            // Beating its own copy earns the duelist 100 experience a level
            // and nothing else (postcbt.pas DetermineLootExp).
            if (pays_experience)
            {
                campaign_->award_experience(
                    100 *
                        static_cast<unsigned>(campaign_->member(champion).character.sheet().level),
                    reward, champion);
            }
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
            // Pool divides and applies prime-score bonuses once to the whole
            // post-combat award, including treasure supplied by the script.
            const bool combine_script_experience =
                game() == GoldBoxGame::pool_of_radiance && plays_original_records();
            const bool has_monster_loot = !paying.empty();
            unsigned experience = combine_script_experience && staged_treasure_
                                      ? treasure_experience(*staged_treasure_)
                                      : 0;
            // Monsters that all got away leave no monster loot to pay.
            if (!paying.empty())
            {
                for (auto record : paying)
                {
                    experience += record_experience(record);
                }
                // Krynn's overland retains a dungeon map while its creatures
                // come from the root archive. Keep that source with unclaimed
                // loot as well, including after an ordinary save and reload.
                const auto loot_area = in_curse_wilderness() ? 0 : current_area_;
                auto loot = encounter_loot(loot_area, std::move(paying), reward + ":loot",
                                           machine_.variable(game_cell(EclAddress{0x6DE3})) != 1,
                                           std::move(spent));
                experience += treasure_experience(loot);
                pending_loot_.push_back(std::move(loot));
                pile_loot();
            }
            if (pays_experience && (has_monster_loot || experience))
            {
                campaign_->award_experience(champion ? experience : experience_share(experience),
                                            reward, champion);
            }
            // Treasure the script added to the fight is won with it.
            if (staged_treasure_)
            {
                award_staged_treasure(!combine_script_experience);
            }
            // A beaten turncoat's gear is the party's, and its record's
            // experience too.
            if (pays_experience && turncoat_loot.experience)
            {
                campaign_->award_experience(turncoat_loot.experience, reward + ":turncoats");
            }
            for (const auto &item : turncoat_loot.items)
            {
                treasure_pile_.push_back(item);
            }
        }
        if (native_recovery && !fled)
        {
            for (const auto id : campaign_->state().slots)
            {
                if (id)
                {
                    campaign_->recover_duelist(id);
                }
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
        const auto conditions = vga_combat_conditions(game(), !fled && !duel_lost && !native_lost);
        if (game() == GoldBoxGame::dark_queen_of_krynn)
        {
            // Native PostCombat clears its transient battle modifiers and
            // award/recovery flags, retaining anti-magic and underwater state.
            for (const std::uint16_t cell : {0x01, 0x2F, 0x1C, 0x1D, 0x1F})
            {
                results.push_back({EclAddress{cell}, 0});
            }
        }
        // The party leaves a maze fight facing the side it ran off, which
        // the script reads to find the next part of the maze (postcbt.pas).
        if (result.escape_side)
        {
            results.push_back(
                {game_cell(ecl_party_facing), static_cast<std::uint16_t>(*result.escape_side)});
        }
        // Pools of Darkness's fight's end clears its to-hit cells
        // (add_script_to_hit) and its party's movement modifier.
        if (game() == GoldBoxGame::pools_of_darkness)
        {
            results.push_back({darkness_monster_hit_mod, 0});
            results.push_back({darkness_party_hit_mod, 0});
            results.push_back({darkness_party_move_mod, 0});
            const auto &request = machine_.pending_request();
            if (result.outcome == rules::Outcome::victory && current_script_ == 75 && request &&
                request->id == combat_request_ && request->instruction &&
                request->instruction->address == 0x94CB && town_)
            {
                const auto program = town_->programs.find(75);
                if (program != town_->programs.end() &&
                    darkness_recovery_uses_monster_count(*program->second))
                {
                    results.push_back({EclAddress{0xC4}, 0});
                }
            }
        }
        // A Treasures crystal battle's crystal joins those the party holds
        // (cell 3Ch: 1 red, 2 green, 4 blue), and cell 3Eh tells the script
        // which it was (effhndlr.pas, EffRedCrystal).
        if (result.crystal)
        {
            constexpr EclAddress crystals_held{0x3C}, last_crystal{0x3E};
            results.push_back(
                {crystals_held, static_cast<std::uint16_t>(machine_.variable(crystals_held) |
                                                           1U << *result.crystal)});
            results.push_back({last_crystal, static_cast<std::uint16_t>(*result.crystal)});
        }
        // Treasures' romance: the loves as the fight moved them (cells 106h
        // and 107h; effhndlr.pas, EffInLove).
        if (result.romance)
        {
            results.push_back(
                {EclAddress{0x106}, static_cast<std::uint16_t>(result.romance->member_love)});
            results.push_back(
                {EclAddress{0x107}, static_cast<std::uint16_t>(result.romance->npc_love)});
        }
        // The items the monsters leave wait for the party to take them; the
        // script hears the result once the party leaves the treasure.
        if (!treasure_pile_.empty())
        {
            open_treasure(combat_request_, std::move(results));
            treasure_conditions_ = conditions;
        }
        else
        {
            auto reply = character_reply(selected_character_);
            reply.writes.insert(reply.writes.end(), results.begin(), results.end());
            reply.conditions = conditions;
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
    staged_undead_loads_ = 0;
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

// Curse's root area is its wilderness. Krynn's wilderness and outposts
// instead retain the previous district while running their own menus.
bool RolfTourSession::in_curse_wilderness() const
{
    return (game() != GoldBoxGame::pool_of_radiance && current_area_ == 0) ||
           ((game() == GoldBoxGame::champions_of_krynn ||
             game() == GoldBoxGame::death_knights_of_krynn) &&
            (current_script_ == 16 || current_script_ == 17));
}

bool RolfTourSession::in_champions_story_rest() const
{
    // ECL3:96 offers safe camps during the flight and in the windcaptain's
    // chamber. Its search entry resumes the story when the player leaves.
    return game() == GoldBoxGame::champions_of_krynn && current_script_ == 96 &&
           (machine_.variable(EclAddress{0x4C0A}) == 1 ||
            machine_.variable(EclAddress{0x4C07}) == 1);
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
    if (matrix_ending_completed())
    {
        // Matrix quits here; settle its event so the complete at-win team
        // can be saved without running a new search or area-entry script.
        snapshot_.game_won = true;
        snapshot_.tour_finished = true;
        snapshot_.phase = TourPhase::completed;
        snapshot_.choices.clear();
        snapshot_.continue_ticket = 0;
        snapshot_.dialogue +=
            (snapshot_.dialogue.empty() ? "" : "\n\n") + std::string("You have won.");
        event_stage_ = EventStage::none;
        event_checkpoint_.reset();
        pending_movement_.reset();
        transition_ = false;
        ++snapshot_.revision;
        return;
    }
    // Death Knights sets 4CD0 before the final battle. Only the authored
    // council's closing address completes it. Preserve the win while the
    // area-entry search starts its own invocation and clears the trace.
    // Later visits take another branch and cannot repeat the announcement.
    if (game() == GoldBoxGame::death_knights_of_krynn && current_script_ == 48 &&
        machine_.variable(EclAddress{0x4CD0}) == 1 &&
        std::ranges::find(machine_.trace(), 0x87FFu) != machine_.trace().end())
    {
        snapshot_.game_won = true;
    }
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
    if (event_stage_ == EventStage::step && snapshot_.solar_map && pending_movement_)
    {
        solar_map_step();
        search_destination();
        return;
    }
    if (event_stage_ == EventStage::step && vga_area_map())
    {
        if (pending_movement_)
        {
            vga_area_step();
        }
        search_destination();
        return;
    }
    if (event_stage_ == EventStage::step)
    {
        if (game() == GoldBoxGame::dark_queen_of_krynn && pending_movement_)
        {
            // Native game_loop records the square after the pre-move
            // script, even when that script transferred the party or
            // blocks MoveResolve. Its post-move entry compares these cells.
            machine_.bind_variable(game_cell(EclAddress{0x49F0}),
                                   static_cast<std::uint16_t>(snapshot_.pose.x));
            machine_.bind_variable(game_cell(EclAddress{0x49F1}),
                                   static_cast<std::uint16_t>(snapshot_.pose.y));
        }
        // A step entry run with no move (Treasures' after a cast) searches
        // where the party stands.
        if (pending_movement_ && !move_party(*pending_movement_))
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
        // Curse's wilderness offers its camp (PROGRAM 9) before its menu: the
        // camp comes first, and the menu when it ends.
        if (!((in_curse_wilderness() || in_champions_story_rest()) && snapshot_.camp_offered))
        {
            event_stage_ = EventStage::search;
            // The original redraws the view before the next entries, which
            // reads the event of the square the entry put the party on: the
            // Kraken headquarters' landing moves it after loading the map
            // (ECL4:13 0x8092), and its square's event is not the one before.
            machine_.bind_variable(game_cell(ecl_cell_event),
                                   map_.at(machine_.variable(game_cell(ecl_party_x)),
                                           machine_.variable(game_cell(ecl_party_y)))
                                       .event_raw);
            // Native RunNewScript invokes the pre-move entry once before
            // its arrival search, without moving the party. In particular,
            // wilderness entry consumes its departure lock here; deferring
            // that call until the player's first step traps Luminari's exits.
            const bool native = game() == GoldBoxGame::dark_queen_of_krynn;
            if (native)
            {
                event_stage_ = EventStage::step;
                pending_movement_.reset();
            }
            if (!machine_.start(native ? 0 : 1))
            {
                throw EclError("Cannot search building entrance");
            }
            return;
        }
    }
    if (event_stage_ == EventStage::pre_camp)
    {
        if (!campaign_)
        {
            throw EclError("Rest requires campaign rules");
        }
        // Treasures' camp tells its romance first (camp.pas, CampMainMenu),
        // a camp from its open menu having told it as the menu opened.
        if (!camp_from_menu_ && !std::exchange(romance_told_, false) && tell_romance())
        {
            return;
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
        if (std::exchange(camp_opening_, false))
        {
            // Opened, the camp menu waits for the player (open_camp).
            camp_menu_open_ = snapshot_.camp_menu_open = true;
        }
        // Native camp.c disables Rest only for a certain encounter at the
        // first check. Its quiet citadel rooms use interval 0, chance 255:
        // no checks run there, so the chance byte cannot forbid resting.
        else if (game() == GoldBoxGame::dark_queen_of_krynn ? interval == 1 && chance >= 100
                                                            : chance == 255)
        {
            camp_fixing_ = false;
            add_camp_line("Rest is not allowed here.");
        }
        else if (std::exchange(camp_fixing_, false))
        {
            // Treasures' camp Fix: an interruption ends it as it ends a rest.
            auto outcome = fix_party(
                [this](unsigned count)
                {
                    return machine_.engine_random(count);
                });
            synchronize_clock();
            for (const auto &word : outcome.words)
            {
                add_camp_line(english(word));
            }
            snapshot_.host_dialogue = std::move(outcome.words);
            if (outcome.interrupted_after)
            {
                event_stage_ = EventStage::camp_interrupted;
                if (!machine_.start(3))
                {
                    throw EclError("Cannot enter camp interruption script");
                }
                return;
            }
        }
        else if (camp_memorizing_)
        {
            // The casters chose their spells, and the rest went on for them:
            // an interruption within that time comes now (gametime.pas,
            // CampRest rolls it each step), with the same dice foreseen.
            const auto waited = *std::exchange(camp_memorizing_, std::nullopt);
            const auto went_on = static_cast<unsigned>(campaign_->last_rest_minutes() -
                                                       waited.rest.duration_minutes);
            const auto broken = went_on
                                    ? rest_interruption(interval, chance, went_on,
                                                        [this](unsigned count)
                                                        {
                                                            return machine_.engine_random(count);
                                                        })
                                    : std::nullopt;
            if (waited.interrupted || broken)
            {
                if (broken)
                {
                    log_diagnostic(LogPriority::info, "camp",
                                   "interrupted after " +
                                       std::to_string(waited.rest.duration_minutes + *broken) +
                                       " minutes");
                }
                interrupt_camp();
                return;
            }
            add_camp_line(rest_complete_line(waited.rest, true));
            for (const auto &w : character_reply(selected_character_).writes)
            {
                machine_.bind_variable(w.address, w.value);
            }
            synchronize_clock();
        }
        else
        {
            const bool any_length = campaign_->rule_module().rests_any_length();
            if (const auto rested = rest_interruption(interval, chance))
            {
                log_diagnostic(LogPriority::info, "camp",
                               "interrupted after " + std::to_string(*rested) + " minutes");
                // Under rules that rest any length the time rested heals and
                // finishes the spells whose preparation it covered
                // (gametime.pas, CampRest); otherwise an interrupted rest
                // grants nothing and leaves nothing to resume.
                const auto granted =
                    any_length ? campaign_->rest(std::chrono::minutes{*rested}) : std::nullopt;
                if (!granted)
                {
                    campaign_->advance_time(std::chrono::minutes{*rested});
                }
                else if (campaign_->state().spell_rest)
                {
                    campaign_->interrupt_rest_at(std::chrono::minutes{*rested});
                }
                synchronize_clock();
                if (granted && campaign_->state().spell_rest)
                {
                    camp_memorizing_ =
                        CampMemorizing{true, campaign_->state().time_minutes, *granted};
                    ++snapshot_.revision;
                    return;
                }
                interrupt_camp();
                return;
            }
            const auto rested = camp_rest();
            if (rested && any_length && campaign_->state().spell_rest)
            {
                foresee_rest_interruption(interval, chance);
                synchronize_clock();
                camp_memorizing_ = CampMemorizing{false, campaign_->state().time_minutes, *rested};
                ++snapshot_.revision;
                return;
            }
            if (rested)
            {
                add_camp_line(rest_complete_line(*rested, any_length));
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
        if (const auto rested = camp_rest())
        {
            snapshot_.dialogue +=
                "\n" + rest_complete_line(*rested, campaign_->rule_module().rests_any_length());
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
    // These offered safe camps wait for the player to leave; that decision,
    // rather than the first night's rest, resumes the native story.
    if (in_champions_story_rest() &&
        (event_stage_ == EventStage::pre_camp || event_stage_ == EventStage::camp_interrupted))
    {
        snapshot_.camp_offered = true;
    }
    // After a camp in Curse's wilderness, its menu returns.
    else if (in_curse_wilderness() &&
             (event_stage_ == EventStage::pre_camp || event_stage_ == EventStage::camp_interrupted))
    {
        event_stage_ = EventStage::search;
        if (!machine_.start(1))
        {
            throw EclError("Cannot return to the wilderness");
        }
        return;
    }
    if (game() == GoldBoxGame::death_knights_of_krynn && snapshot_.game_won)
    {
        notice("Lord Soth is banished, and Sturm's body is at rest. You have won.");
        return;
    }
    // Secret and Treasures have no ending of their own: the scripts tell of
    // the soul's freeing or of Freezefire's fall and the game goes on. The
    // event that does it wins the game as it ends (docs/DEVIATIONS.md), and the
    // win is shown until the player goes on.
    if (const auto won = traits(game()).won_cell;
        won && won_cell_at_event_start_ == 0 && machine_.variable(*won) == 1)
    {
        won_cell_at_event_start_ = 1;
        snapshot_.game_won = true;
        notice(game() == GoldBoxGame::secret_of_the_silver_blades
                   ? "Eldamar's soul is free, and the Dreadlord is no more. You have won."
                   : "You have won.");
        return;
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
    const bool wilderness = game() != GoldBoxGame::dark_queen_of_krynn && current_script_ >= 25 &&
                            current_script_ <= 27;
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
                   machine_.variable(game_cell(ecl_overland_x)) + directions::step_x[direction], 0,
                   OverlandWorld::window_columns - 1)),
               y = static_cast<unsigned>(std::clamp<int>(
                   machine_.variable(game_cell(ecl_overland_y)) + directions::step_y[direction], 0,
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
    const auto edge = edge_ahead(map_, snapshot_.pose);
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
    const auto edge = edge_ahead(map_, snapshot_.pose);
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
    const auto edge = edge_ahead(map_, snapshot_.pose);
    const auto unlock = [](std::uint8_t &door)
    {
        if (lock(door))
        {
            door = 1;
        }
    };
    unlock(map_.cells[edge.y * map_.width + edge.x].doors[index(edge.side)]);
    unlock(map_.cells[edge.next_y * map_.width + edge.next_x].doors[index(edge.other)]);
    if (!move_party(*pending_movement_))
    {
        throw EclError("The opened door is still blocked");
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
                         encounter_distance_ && !overland() && !gateway_overland() ? "Advance"
                                                                                   : "Parley"};
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
    if (choice == 3 && (encounter_distance_ == 0 || overland() || gateway_overland()))
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

void RolfTourSession::await_continue(std::uint64_t request)
{
    damage_request_ = request;
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
    if (buck_rogers_menu_)
    {
        return choose_in_buck_rogers_menu(choice);
    }
    if (romance_scene_)
    {
        return choose_in_romance(choice);
    }
    if (hall_visit_)
    {
        return choose_at_hall(choice);
    }
    if (dark_queen_vault_visit_)
    {
        return choose_dark_queen_vault(choice);
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
            snapshot_.game_won = false;
            reply.writes.push_back({game_cell(EclAddress{0x4AFD}), 255});
        }
        for (const auto &write : std::exchange(damage_writes_, {}))
        {
            reply.writes.push_back(write);
        }
        if (!machine_.resume_host(damage_request_, reply))
        {
            return false;
        }
        damage_request_ = 0;
        snapshot_.host_dialogue.clear();
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
        // Secret's win is shown until the player goes on.
        snapshot_.game_won = false;
        snapshot_.phase = TourPhase::completed;
    }
    else
    {
        // Gateway's engine prompts (a CALL) clear the text window once answered.
        const auto &pending = machine_.pending_request();
        const bool engine_prompt =
            pending && pending->instruction && pending->instruction->opcode == 45;
        if (!machine_.resume(menu_request_, choice))
        {
            return false;
        }
        if (game() == GoldBoxGame::countdown_to_doomsday)
        {
            // Countdown's authored closing continue prompt announces its win;
            // continuing resumes the original return to Salvation unchanged.
            snapshot_.game_won = false;
        }
        if (engine_prompt)
        {
            snapshot_.dialogue.clear();
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
    if (result.saved && result.damage == 0)
    {
        return "\n" + name + " avoids it" + save + ".";
    }
    // A save that only halves the damage (the 256-color engine's) still hurts.
    const auto saving = result.saved ? " saves" + save + " but"
                        : save_dc    ? " fails to avoid it" + save + " and"
                                     : std::string();
    auto report = "\n" + name + saving + " takes " + std::to_string(result.damage) + " damage" +
                  damage_dice_shown(result, sides) + ".";
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
    static constexpr std::array party_requests{11, 29, 30, 34, 35, 41, 46, 52, 54, 59, 62};
    if (!campaign_ &&
        std::find(party_requests.begin(), party_requests.end(), op) != party_requests.end())
    {
        throw EclError("This event needs the campaign party");
    }
    if (is_buck_rogers(game()) && handle_buck_rogers_host(request))
    {
        return true;
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
        // Gateway's LOAD CHARACTER with bit 7 makes the member at its position
        // current and, when the script has cleared the name and status,
        // removes that member (its EclLoadCharacter): Erek leaves with
        // Amanitas, though the record the script cleared is another's. The
        // cleared record is not read back into anyone.
        if (arg(0) >= 128 && game() == GoldBoxGame::gateway_to_the_savage_frontier && campaign_)
        {
            const PartySlot target{static_cast<unsigned>(arg(0) & 127)};
            const auto leaving =
                target.index < 8 ? campaign_->state().slots[target.index] : MemberId{};
            const bool cleared = machine_.variable(game_cell(ecl_character_record)) == 0 &&
                                 machine_.variable(game_cell(ecl_character_status)) == 0;
            if (cleared && leaving)
            {
                campaign_->remove(leaving);
                if (selected_character_.index >= 8 ||
                    !campaign_->state().slots[selected_character_.index])
                {
                    selected_character_ = campaign_->state().selected_slot;
                }
            }
            else
            {
                read_character();
                if (leaving)
                {
                    selected_character_ = target;
                }
            }
            reply = character_reply(selected_character_);
            break;
        }
        // The 256-color engine's LOAD CHARACTER reads only its argument's
        // low seven bits (eclvm.pas, EclLoadCharacter, in Pools of
        // Darkness's and Treasures' reconstructions): FIND NPC's high bit
        // left on selects the member, as Myth Drannor's way out does with
        // Nacacia (ECL1:66 0x8658).
        if (arg(0) >= 128 && vga_engine(game()))
        {
            read_character();
            selected_character_ = PartySlot{static_cast<unsigned>(arg(0) & 127)};
            reply = character_reply(selected_character_);
        }
        else if (arg(0) >= 128)
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
        const bool native = game() == GoldBoxGame::dark_queen_of_krynn;
        // Matrix's Venus ambush requests three squares even though its
        // distance sprites and the ground encounter use at most two. Keep
        // the existing corridor limit below, as for the other Buck setups.
        const bool buck_distance_three = is_buck_rogers(game()) && arg(1) == 3;
        if (arg(1) > 2 && !native && !buck_distance_three)
        {
            throw EclError("Unsupported encounter distance");
        }
        // Native limit 10h explicitly forces the farthest distance even
        // when the current corridor ends sooner (CMD_SetupMonster).
        encounter_distance_ =
            native && arg(1) == 0x10 ? 2U : std::min<unsigned>(arg(1), room_ahead());
        // Pools of Darkness's and Treasures' SETUP MONSTER names the sides
        // the monsters come from too (eclvm.pas, EclSetupMonster).
        if (request.arguments.size() > 3)
        {
            monster_sides_ = arg(3) & 0xFFU;
        }
        // Champions' DisplayMonster loads distance sprites only in the 3D
        // view. Wilderness scripts deliberately name sprites absent from SPRIT.
        if (krynn_game(game()) && vga_area_map())
        {
            sprites_ = {};
            snapshot_.sprite_frame = -1;
            snapshot_.sprite_id = arg(0);
            monster_picture_id_.reset();
            break;
        }
        // Gateway's monsters have no distance sprites (255; it has no SPRIT
        // files): only the close-up shows, face to face (its DisplayMonster).
        constexpr unsigned no_sprite = 255;
        if (arg(0) == no_sprite)
        {
            sprites_ = {};
            snapshot_.sprite_frame = -1;
            snapshot_.sprite_id = arg(0);
            picture_animation_id_.reset();
            picture_.reset();
            if (arg(2) == no_sprite)
            {
                monster_picture_id_.reset();
            }
            else
            {
                monster_picture_id_ = arg(2);
            }
            ++snapshot_.picture_revision;
            break;
        }
        // The 256-color engine's sprites are 256-color animations.
        const auto sprite = [this](unsigned id, unsigned distance)
        {
            if (const auto shared = town_->shared_sprites.find(id);
                shared != town_->shared_sprites.end() && distance < shared->second.size())
            {
                return ImageDecodeResult{FormatResult::ok, shared->second[distance]};
            }
            return vga_art(game()) ? vga_sprite(id, distance)
                                   : decode_ega_sprite(area_resources().sprite_archive,
                                                       static_cast<std::uint8_t>(id),
                                                       static_cast<std::uint8_t>(distance));
        };
        // The Pitt's buccaneer names sprite 19, which SPRIT1 lacks: met face
        // to face, the original shows the head (0x6DE1) and body portrait.
        const auto head_id = machine_.variable(game_cell(EclAddress{0x6DE1}));
        if (!sprite(arg(0), 0) && encounter_distance_ == 0 && head_id != 255)
        {
            snapshot_.sprite_frame = -1;
            snapshot_.sprite_id = arg(0);
            monster_picture_id_.reset();
            picture_animation_id_.reset();
            picture_ = close_up_portrait(area_resources(), head_id, arg(2));
            ++snapshot_.picture_revision;
            break;
        }
        // A Buck Rogers sprite its disk's SPRIT file lacks loads nothing, and
        // nothing shows (portrait.pas LoadAnimation of an empty record).
        if (is_buck_rogers(game()) && !sprite(arg(0), 0))
        {
            sprites_ = {};
            snapshot_.sprite_frame = -1;
            snapshot_.sprite_id = arg(0);
            monster_picture_id_ = arg(2);
            picture_.reset();
            ++snapshot_.picture_revision;
            break;
        }
        for (unsigned n = 0; n < 3; ++n)
        {
            auto image = sprite(arg(0), n);
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
        // The 256-color engine's 254 blanks the picture's box (its ShowPicture).
        constexpr unsigned blank_picture = 254;
        if (arg(0) == 255 || (vga_engine(game()) && arg(0) == blank_picture))
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
            const auto *frames = pic_frames(arg(0));
            if (frames && !frames->empty() && frames != &animation->second)
            {
                picture_ = frames->front().image;
            }
            else if (found != area_resources().pictures.end())
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
        std::optional<std::array<VgaColor, 256>> native_palette;
        if (area.dark_queen)
        {
            native_palette = ua::combat_palette(area.dark_queen->files, dark_queen_combat_set_,
                                                machine_.variable(EclAddress{0x22}) != 0);
        }
        // Treasures' LOAD MONSTER holds its creatures back while cell 44h is
        // set: they join the fight as that round ends, the ninth at the
        // latest (its EclLoadMonster and JoinDelayedCombatants).
        constexpr EclAddress treasures_arrival_round{0x44};
        constexpr unsigned latest_arrival = 9;
        const unsigned arrival =
            game() == GoldBoxGame::treasures_of_the_savage_frontier
                ? std::min(static_cast<unsigned>(machine_.variable(treasures_arrival_round)),
                           latest_arrival)
                : 0;
        const auto *exported = exported_creature(area, record);
        const auto name = creature_name(area, record, exported);
        // Pools of Darkness gives each member a try at turning undead for
        // each LOAD MONSTER that brings undead (its EclLoadMonster,
        // turnUndeadLimit).
        if (const auto creature = area.encounter_creatures.find(record);
            count > 0 && creature != area.encounter_creatures.end() &&
            creature->second.stored.undead_level != 0)
        {
            ++staged_undead_loads_;
        }
        for (unsigned i = 0; ally && i < count; ++i)
        {
            const auto id =
                static_cast<rules::EntityId>(opengold::first_ally_id + staged_allies_.size());
            auto staged =
                stage_area_creature(area, record, icon, exported, id, name, rules::Side::party,
                                    difficulty_, plays_original_records(), game(), native_palette);
            staged.fighter.arrival_round = arrival;
            staged_allies_.push_back(std::move(staged.fighter));
            staged_ally_art_.push_back(std::move(staged.art));
        }
        for (unsigned i = 0; !ally && i < count; ++i)
        {
            const auto id = static_cast<rules::EntityId>(1000 + staged_enemies_.size());
            auto staged = stage_area_creature(
                area, record, icon, exported, id,
                name + " " + std::to_string(staged_enemies_.size() + 1), rules::Side::opposition,
                difficulty_, plays_original_records(), game(), native_palette);
            staged.fighter.arrival_round = arrival;
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
        staged_undead_loads_ = 0;
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
        encounter_distance_ = std::min<unsigned>(arg(1), room_ahead());
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
        // Treasures' CHECK PARTY reads its operands and does nothing: its
        // four cells keep what they hold.
        if (game() == GoldBoxGame::treasures_of_the_savage_frontier ||
            game() == GoldBoxGame::dark_queen_of_krynn)
        {
            for (std::size_t n = 2; n < request.arguments.size(); ++n)
            {
                const auto cell = address_arg(static_cast<unsigned>(n));
                std::erase_if(reply.writes,
                              [cell](const EclMemoryWrite &write)
                              {
                                  return write.address == cell;
                              });
                reply.writes.push_back({cell, machine_.variable(cell)});
            }
            break;
        }
        auto values = check_party(*campaign_, pool_of_radiance_address(address_arg(0), game()),
                                  arg(1), game());
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
        if (game() == GoldBoxGame::death_knights_of_krynn ||
            game() == GoldBoxGame::dark_queen_of_krynn)
        {
            static constexpr std::array<std::int64_t, 7> seconds{6,     60,      600,     3600,
                                                                 86400, 2592000, 31104000};
            if (arg(1) >= seconds.size())
            {
                throw EclError("Invalid Death Knights clock unit");
            }
            campaign_->advance_time(std::chrono::seconds{seconds[arg(1)] * (arg(0) & 0xFF)});
        }
        else
        {
            campaign_->advance_time(step_time * arg(0));
        }
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
        // Secret's Hall of the Dead asks for a cleric's or a magic-user's
        // Dispel Magic (ECL1:34 0x8E6D); whoever has it memorized casts it,
        // and the script's own clearing of the spell's cell does nothing more.
        constexpr unsigned cleric_dispel = 41, magic_user_dispel = 46;
        // Gateway's squid tank asks for each fire spell (ECL2:2 0x9263): every
        // memorized one the water ruins is gone, one a question, until none is left.
        const bool gateway = game() == GoldBoxGame::gateway_to_the_savage_frontier;
        const auto asked =
            original::spell_id(gateway ? original::canonical_spell(static_cast<int>(arg(0)), game())
                                       : static_cast<int>(arg(0)));
        const bool dispel = (game() == GoldBoxGame::secret_of_the_silver_blades &&
                             (arg(0) == cleric_dispel || arg(0) == magic_user_dispel)) ||
                            (gateway && !asked.empty());
        if (krynn_game(game()))
        {
            read_character();
            reply = character_reply(selected_character_);
            const auto wanted =
                original::spell_id(original::canonical_spell(static_cast<int>(arg(0)), game()));
            std::uint16_t place = 255, slot = 0;
            const auto &slots = campaign_->state().slots;
            for (std::size_t n = 0; n < slots.size(); ++n)
            {
                if (!slots[n])
                {
                    continue;
                }
                slot = static_cast<std::uint16_t>(n);
                const auto spells = campaign_->memorized_spells(slots[n]);
                const auto found = std::ranges::find(spells, wanted);
                if (!wanted.empty() && found != spells.end())
                {
                    place = static_cast<std::uint16_t>(found - spells.begin());
                    break;
                }
            }
            reply.writes.push_back({address_arg(1), place});
            reply.writes.push_back({address_arg(2), slot});
            break;
        }
        // The 256-color engine asks for any spell: whoever has it memorized casts it.
        if (vga_engine(game()))
        {
            read_character();
            reply = vga_spell(arg(0), address_arg(1), address_arg(2));
            break;
        }
        if (arg(0) != knock && !dispel)
        {
            throw EclError("Unsupported SPELL " + std::to_string(arg(0)));
        }
        read_character();
        reply = character_reply(selected_character_);
        std::uint16_t place = 255, slot = 0;
        if (dispel)
        {
            if (const auto caster = campaign_->cast_exploration_spell(asked))
            {
                const auto &slots = campaign_->state().slots;
                place = 0;
                slot = static_cast<std::uint16_t>(std::find(slots.begin(), slots.end(), *caster) -
                                                  slots.begin());
            }
        }
        else if (campaign_->can_try_door(DoorMethod::knock))
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
        // MON3CHA, whose record 24 has no conversion. Secret keeps one
        // creature file for every disk.
        read_character();
        const unsigned bank = [&]
        {
            if (game() == GoldBoxGame::dark_queen_of_krynn)
            {
                return 1U; // The shared MONCHA.GLB creature table.
            }
            const auto creature_disks = game_disks(game()).monsters;
            return creature_disks.size() == 1 ? creature_disks.front()
                   : current_area_            ? area_resources().bank
                                              : 3;
        }();
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
            auto record_bytes = original->stored.raw;
            if (game() == GoldBoxGame::death_knights_of_krynn)
            {
                record_bytes.insert(record_bytes.end(), original->stored.raw_spell_block.begin(),
                                    original->stored.raw_spell_block.end());
            }
            if (const auto draft = creation.record_draft(record_bytes))
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
        // The later games' original adds no one to a full party
        // (AddCreatureToParty: eight members at most).
        const auto &slots = campaign_->state().slots;
        if (game() != GoldBoxGame::pool_of_radiance && slots[6] && slots[7])
        {
            break;
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
            // A record may mark two items readied in one place (Gateway's
            // Brinshaar, ECL3:9): the first is worn, the other stays packed.
            try
            {
                (void)campaign_->equip(joined, item);
            }
            catch (const std::runtime_error &)
            {
            }
        }
        // The script may be looking at the slot the companion just took. The
        // record it holds must show the new member, or reading it back would
        // find a member with no money and charge the party for the difference.
        reply = character_reply(selected_character_);
        break;
    }
    case 39:
    {
        if (later_ecl_commands(game()))
        {
            stage_vga_treasure(request);
            break;
        }
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
            staged_undead_loads_ = 0;
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
            // Curse fights out of doors (its 3D-area cell clear) on open ground
            // grown from its current place's terrain (cell 0x4E42), as the
            // original's SetupWildernessFloor does.
            const bool curse_outdoors =
                town_->curse_wilderness && machine_.variable(game_cell(ecl_in_3d_area)) == 0;
            const auto curse_place_flags = [&]() -> unsigned
            {
                constexpr EclAddress curse_current_place{0x4E42};
                const auto &flags = town_->curse_wilderness->place_flags;
                const auto place = machine_.variable(curse_current_place);
                return place < flags.size() ? flags[place] : 0U;
            };
            // Gateway fights on its overland on grass grown from the ground's
            // class (cell 4CA1h, which nothing sets), as its
            // BuildWildernessArena does.
            const bool gateway_outdoors =
                town_->gateway_wilderness &&
                (gateway_overland() || (krynn_game(game()) && vga_area_map()));
            const auto gateway_ground_class = [&]() -> unsigned
            {
                constexpr EclAddress gateway_ground{0x4CA1};
                const auto &classes = town_->gateway_wilderness->terrain_classes;
                // Champions' BuildWildernessArena explicitly selects terrain 15.
                if (krynn_game(game()))
                {
                    return classes.at(15);
                }
                const std::size_t ground = machine_.variable(gateway_ground) & 0xFFu;
                return ground < classes.size() ? classes[ground] : 0U;
            };
            std::vector<Image> native_terrain;
            auto field = [&]
            {
                if (town_->dark_queen)
                {
                    const bool dungeon = machine_.variable(EclAddress{0x22}) != 0;
                    auto native = dungeon
                                      ? dark_queen_dungeon_field(*town_->dark_queen, map_, p,
                                                                 dark_queen_combat_set_)
                                      : ua::wilderness_field(
                                            town_->dark_queen->files, dark_queen_combat_set_,
                                            [&](unsigned sides)
                                            {
                                                return machine_.host_random(request.id, sides) + 1;
                                            },
                                            dark_queen_wilderness_style(
                                                machine_.variable(EclAddress{0x1A2}),
                                                machine_.variable(EclAddress{0x33}) != 0));
                    native_terrain = std::move(native.terrain_art);
                    return std::move(native.field);
                }
                return overland() ? wilderness_battlefield(
                                        *town_->overland,
                                        town_->overland->at(
                                            town_->overland->window_start[travel_mode_ - 2] +
                                                machine_.variable(game_cell(ecl_overland_x)),
                                            machine_.variable(game_cell(ecl_overland_y))),
                                        roll)
                       : curse_outdoors
                           ? wilderness_battlefield(town_->curse_wilderness->combat_tiles,
                                                    curse_place_flags(), roll)
                       : gateway_outdoors
                           ? gateway_wilderness_battlefield(town_->gateway_wilderness->combat_tiles,
                                                            gateway_ground_class(), roll)
                           : dungeon_battlefield(map_, p.x, p.y);
            }();
            encounter_ = opengold::CampaignEncounter{
                std::move(field),
                std::move(staged_enemies_),
                std::move(staged_art_),
                overland() || curse_outdoors || gateway_outdoors ? town_->wilderness_art
                                                                 : area_resources().terrain_art,
                overland() ? wilderness_facing : p.facing,
                script_surprise(machine_.variable(game_cell(ecl_encounter_surprise)))};
            if (town_->dark_queen)
            {
                encounter_->terrain_art = std::move(native_terrain);
            }
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
            encounter_->outdoors = overland() || curse_outdoors || gateway_outdoors;
            if (town_->dark_queen)
            {
                encounter_->outdoors = machine_.variable(EclAddress{0x22}) == 0;
            }
            // Gateway's combat mode 1 (cell 4D01h), which its maze through
            // Ascore sets (ECL6:27 0x843A).
            constexpr EclAddress gateway_combat_mode{0x4D01};
            encounter_->escape_ends_fight = game() == GoldBoxGame::gateway_to_the_savage_frontier &&
                                            machine_.variable(gateway_combat_mode) == 1;
            // Treasures' crystal battles, which their scripts mark by cell
            // 3Dh (gameState's crystalsActive).
            constexpr EclAddress treasures_crystals_active{0x3D};
            encounter_->crystals = game() == GoldBoxGame::treasures_of_the_savage_frontier &&
                                   machine_.variable(treasures_crystals_active) != 0;
            // PARTY_MOVE_MODIFIER, which the fight's end clears (postcbt.pas).
            encounter_->party_move_modifier =
                static_cast<int>(machine_.variable(game_cell(EclAddress{0x6E72})) & 0xFF);
            // Treasures' cell 53h, which its scripts set where its yetis
            // surprise the party (effhndlr.pas, EffFrightenAura).
            encounter_->frightening_surprise =
                game() == GoldBoxGame::treasures_of_the_savage_frontier &&
                machine_.variable(EclAddress{0x53}) != 0;
            // Treasures' romance, once a script has paired a member and an
            // NPC (romance.pas, TagRelationshipPair, as each fight starts).
            encounter_->romance = treasures_romance();
            // Pools of Darkness's turning: a try for each LOAD MONSTER of
            // undead, and the penalty its scripts set in cell Fh
            // (turnUndeadPenalty), as CombatTurnUndead reads them.
            if (game() == GoldBoxGame::pools_of_darkness)
            {
                constexpr EclAddress darkness_turning_penalty{0x0F};
                encounter_->turning_tries = static_cast<int>(staged_undead_loads_);
                encounter_->turning_penalty =
                    static_cast<int>(machine_.variable(darkness_turning_penalty) & 0xFF);
                // Its cell 2 (antiMagic), which the finale's script sets for
                // Gothmenes' last stand: no one casts a spell or uses a magic
                // item (combatai.pas, SelectSpellToCast; combat.pas, the
                // Cast menu; viewchar.pas, the Use menu).
                encounter_->no_magic = magic_barred();
                // Its own cell for the party's movement modifier, which
                // Moander's heart sets (ECL1:53 0x9C8C).
                encounter_->party_move_modifier =
                    static_cast<int>(machine_.variable(darkness_party_move_mod) & 0xFF);
            }
            if (game() == GoldBoxGame::dark_queen_of_krynn)
            {
                // Native LOAD MONSTER counts undead loads, not copies, for
                // each member's turning attempts. Cell Fh reduces its column.
                encounter_->turning_tries = static_cast<int>(staged_undead_loads_);
                encounter_->turning_penalty =
                    static_cast<int>(machine_.variable(EclAddress{0x0F}) & 0xFF);
                encounter_->underwater = machine_.variable(EclAddress{0x003C}) != 0;
                encounter_->no_magic = magic_barred();
                encounter_->party_move_modifier =
                    static_cast<int>(machine_.variable(EclAddress{0x001C}) & 0xFF);
            }
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
                encounter_distance_ = std::min(encounter_distance_, room_ahead());
            }
            // On the wilderness map the original stands the monsters 2 squares
            // away, whatever SETUP MONSTER asked (PLACE-2); so does Gateway's
            // overland (its MaxMonsterDist).
            encounter_->distance = overland() || gateway_overland() ? 2u : encounter_distance_;
            // The monsters are dealt to the sides SETUP MONSTER named, in turn
            // (combatst.pas, PlaceAllCreatures).
            encounter_->enemy_quarters =
                por::dealt_quarters(monster_sides_, encounter_->enemies.size());
            // The script sets the encounter's morale before COMBAT.
            const auto morale_cell = game() == GoldBoxGame::pools_of_darkness
                                         ? darkness_break_point
                                         : game_cell(ecl_encounter_morale);
            encounter_->morale = std::min<unsigned>(machine_.variable(morale_cell), 100);
            if (plays_original_records())
            {
                apply_original_difficulty(*encounter_, difficulty_, game());
            }
            else
            {
                apply_difficulty(*encounter_, difficulty_,
                                 average_party_level(living_member_levels()));
            }
            add_script_to_hit();
            announce_camp_attack();
            combat_request_ = request.id;
            // The original shows the approached monster's close-up until a key press.
            showing_monster_picture_ =
                monster_picture_id_ && area_resources().animations.contains(*monster_picture_id_);
            snapshot_.phase = TourPhase::combat;
            ++snapshot_.revision;
            return true;
        }
        if (game() == GoldBoxGame::dark_queen_of_krynn && machine_.variable(EclAddress{0x31}) == 1)
        {
            if (!campaign_)
            {
                throw EclError("Vault service requires a campaign party");
            }
            read_character();
            open_dark_queen_vault(request.id);
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
            bool stocked = false;
            for (std::size_t n = 0; n < shops_.size(); ++n)
            {
                if (standing_at(shops_[n].archive, shops_[n].record, shops_[n].markers))
                {
                    treasure_ = shop_stocks_[n];
                    stocked = true;
                }
            }
            // Curse's shop sells the list its script's TREASURE named just before
            // (CMD_Treasure, then CityShop, in its reconstruction).
            if (!stocked && staged_treasure_ && game() != GoldBoxGame::pool_of_radiance)
            {
                treasure_ = staged_treasure_->items;
                staged_treasure_.reset();
            }
            enter_shop(request.id);
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
                treasure_conditions_ = vga_combat_conditions(game(), true);
                return true;
            }
            reply = character_reply(selected_character_);
            reply.conditions = vga_combat_conditions(game(), true);
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
        reply.conditions = vga_combat_conditions(game(), true);
        break;
    case 32:
    {
        const auto found = town_->programs.find(arg(0));
        if (found == town_->programs.end())
        {
            closed_way_ = "The way to another area is not in the game yet.";
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
        // Curse's wilderness keeps its back-from-camp flag in a cell Tilverton's
        // sage and Kybor's library set for themselves: arriving with it set,
        // the original skipped loading the roads from where the party stands,
        // and the first journey went where stale cells said (ORIGINAL-RULES.md,
        // Bugs). Coming from elsewhere, the party is not back from camp.
        constexpr unsigned curse_wilderness_script = 80;
        constexpr EclAddress curse_back_from_camp{0x4C13};
        if (game() == GoldBoxGame::curse_of_the_azure_bonds && arg(0) == curse_wilderness_script &&
            current_script_ != curse_wilderness_script)
        {
            reply.writes.push_back({curse_back_from_camp, 0});
        }
        // Gateway's every new script starts in a dungeon (its EclInit); the
        // overland's clears the cell itself.
        if (game() == GoldBoxGame::gateway_to_the_savage_frontier)
        {
            reply.writes.push_back({game_cell(ecl_in_3d_area), 1});
        }
        current_script_ = arg(0);
        snapshot_.script_id = current_script_;
        transition_ = true;
        pending_movement_.reset();
        // Curse's scripts of a disk with no map (the wilderness's, on disk 1)
        // play in the root area, which holds that disk's creatures.
        if (game() != GoldBoxGame::pool_of_radiance &&
            std::ranges::none_of(town_->districts,
                                 [bank](const auto &district)
                                 {
                                     return district.second->bank == bank;
                                 }))
        {
            change_area(0);
        }
        break;
    }
    case vga_play_sound_command:
        vga_play_sound(arg(0));
        break;
    case 33:
    {
        if (game() == GoldBoxGame::dark_queen_of_krynn)
        {
            // Native LOAD FILES names a GEO record and the combat tile set;
            // the map itself names all three wall sets.
            const auto map = arg(0);
            if (!town_->districts.contains(map))
            {
                throw EclError("Missing Dark Queen GEO " + std::to_string(map));
            }
            change_area(map);
            if (arg(1) != 255)
            {
                dark_queen_combat_set_ = arg(1);
            }
            reply.writes = {{game_cell(ecl_current_map), static_cast<std::uint16_t>(map)},
                            {EclAddress{0xC6}, static_cast<std::uint16_t>(map)},
                            {EclAddress{3}, 0}};
            reply.writes.push_back(
                {EclAddress{0x3D}, native_cell_flags(machine_.variable(game_cell(ecl_party_x)),
                                                     machine_.variable(game_cell(ecl_party_y)))});
            break;
        }
        if (vga_engine(game()))
        {
            reply = vga_load_files(arg(0), arg(1));
            break;
        }
        // The Buck Rogers games' map 127 is none: the outposts and the solar
        // map are pictures (eclvm.pas EclLoadFiles); in a Venus or Mars
        // wilderness no map loads.
        if (is_buck_rogers(game()))
        {
            constexpr std::uint16_t no_map = 127, venus_wilderness = 0xA2, mars_wilderness = 0xA8;
            const auto kind = machine_.variable(EclAddress{0x4BE6});
            if (arg(0) == 255 || kind == venus_wilderness || kind == mars_wilderness)
            {
                break;
            }
            if (arg(0) == no_map)
            {
                change_area(0);
                reply.writes = {{game_address(ecl_current_map, game()), no_map}};
                break;
            }
        }
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
    case 62:
    {
        // DUMP (Curse): the selected character leaves the party with what it
        // carries, as Alias and Dragonbait leave at the Pit of Moander's way
        // out (ovr003, CMD_Dump). Its slot stays empty, so a script that walks
        // the slots still meets everyone once.
        read_character();
        if (const auto id = campaign_->state().slots.at(selected_character_.index))
        {
            campaign_->remove(id);
        }
        reply = character_reply(selected_character_);
        break;
    }
    case treasures_load_wall_sets_command:
        reply = treasures_load_wall_sets(arg(0), {arg(1), arg(2), arg(3)});
        break;
    case treasures_find_named_item_command:
    case treasures_take_named_item_command:
    {
        // Treasures' FIND and TAKE NAMED ITEM: an item of the type (0 for
        // any) whose name words are these, the last first (a last word of 0
        // matches any name); TAKE takes the first such item from the party.
        const std::array words{static_cast<std::uint8_t>(arg(3)), static_cast<std::uint8_t>(arg(2)),
                               static_cast<std::uint8_t>(arg(1))};
        const bool found =
            campaign_ && campaign_->find_named_item(static_cast<std::uint8_t>(arg(0)), words,
                                                    op == treasures_take_named_item_command);
        if (op == treasures_find_named_item_command)
        {
            reply.conditions = EclConditions{found, !found, false, false, false, false};
        }
        break;
    }
    case 63:
    {
        if (game() != GoldBoxGame::dark_queen_of_krynn || !campaign_)
        {
            throw EclError("Native effect lookup requires a campaign rules module");
        }
        read_character();
        const auto id = selected_character_.index < 8
                            ? campaign_->state().slots[selected_character_.index]
                            : MemberId{};
        std::optional<bool> found = false;
        if (id)
        {
            found = campaign_->rule_module().script_has_effect(
                campaign_->profile(id), campaign_->member(id).vitals, arg(0));
        }
        if (!found)
        {
            throw EclError("The selected rules do not support native effect lookups");
        }
        reply = character_reply(selected_character_);
        reply.conditions = EclConditions{*found, !*found, false, false, false, false};
        break;
    }
    case 64:
        // DESTROY ITEMS (Curse): every item of the type in the party is gone.
        campaign_->destroy_items(static_cast<std::uint8_t>(arg(0)));
        break;
    case 65:
    {
        // AWARD EXPERIENCE (Secret, eclvm EclAwardExperience): the second
        // operand to every member, or with a first of 0 to the selected
        // character alone, each share divided among its classes. Pools of
        // Darkness and Treasures count the operand in hundreds (EclAwardExperience
        // multiplies it by 100): Thorne's fall is worth 100,000 a member.
        const bool whole_party = arg(0) != 0;
        MemberId only{};
        if (!whole_party)
        {
            read_character();
            only = campaign_->state().slots.at(selected_character_.index);
        }
        const unsigned award_unit = vga_engine(game()) ? 100 : 1;
        campaign_->grant_script_experience(arg(1) * award_unit, only);
        const auto line = whole_party || !only ? std::string("The party gains experience.")
                                               : campaign_->member(only).character.sheet().name +
                                                     " gains experience.";
        snapshot_.dialogue += (snapshot_.dialogue.empty() ? "" : "\n") + line;
        break;
    }
    case 60:
    {
        if (later_ecl_commands(game()))
        {
            open_vga_store(request);
            return true;
        }
        // PROTECTION: a line of rune glyphs at the operand's address, which the
        // original's code wheel translated; shown translated (SOKAL-1).
        const auto line = rune_text(machine_, address_arg(0));
        snapshot_.dialogue += line;
        rune_scroll_ += line + "\n";
        break;
    }
    case 55:
    {
        if (vga_engine(game()))
        {
            reply = vga_find_npc(request.arguments.at(0).text(), address_arg(1));
            break;
        }
        // The wilderness's sites reset the wall banks (127) before NEW ECL.
        const std::array<unsigned, 3> banks{arg(0), arg(1), arg(2)};
        if (banks == std::array<unsigned, 3>{127, 127, 127})
        {
            break;
        }
        if (gateway_overland())
        {
            load_overland_symbols(detail::load_pieces_banks(banks));
            break;
        }
        // In Curse a script loads the view's walls itself, as the original does
        // (eclvm.pas EclLoadFiles): those of its own disk.
        if (game() != GoldBoxGame::pool_of_radiance)
        {
            const auto disk = town_->script_banks.find(current_script_);
            const auto walls =
                disk == town_->script_banks.end()
                    ? town_->wall_sets.end()
                    : town_->wall_sets.find({disk->second, detail::load_pieces_banks(banks)});
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
        if (vga_engine(game()))
        {
            read_character();
            vga_damage(request);
            await_continue(request.id);
            return true;
        }
        // Arrow volleys: arg(0) arrows of arg(1)d arg(2) + arg(3). A high bit
        // marks the original's other forms (coab CMD_Damage): the dice dealt
        // to the member the script chose, with no saving throw (the Cadorna
        // well's fall), or to a random member who fails a saving throw with
        // the bonus in arg(0)'s low bits, of the type arg(4) names (Mendor's
        // green slime). The original names no damage type; script_damage_type
        // gives each call's.
        const auto address = request.instruction->address;
        const bool death_knights = game() == GoldBoxGame::death_knights_of_krynn;
        const bool volley = !(arg(0) & 0x80) && (death_knights || !(arg(4) & 0x80));
        const auto save_modifier = static_cast<int>(arg(0) & (death_knights ? 0x0F : 0x1F));
        // Curse's chosen-member calls name a save as category plus one;
        // zero means none (CMD_Damage). The tower's explosive runes use
        // 90h/84h: bit 10h still deals full damage when the save succeeds.
        const bool curse_chosen =
            (game() == GoldBoxGame::curse_of_the_azure_bonds || krynn_game(game())) &&
            (arg(0) & 0xC0) == 0x80 && (arg(4) & 0x80) &&
            (arg(4) & 7) <= original_save_abilities.size();
        const bool chosen_member =
            ((arg(0) & 0xC0) == 0x80 && (arg(4) & 0x87) == 0x80) || curse_chosen;
        const bool random_member = (arg(0) & (death_knights ? 0xC0 : 0xD0)) == 0x80 &&
                                   !(arg(4) & 0x80) &&
                                   (arg(4) & 7) < original_save_abilities.size();
        // Every member, each saving unless arg(0) has 0x20 (Kovel Mansion's gas).
        // In Curse and Secret 0x10 hurts a member who saves as much as one
        // who fails (CMD_Damage / EclDamage); the save cannot avoid damage.
        const bool hurt_anyway = (game() == GoldBoxGame::curse_of_the_azure_bonds ||
                                  game() == GoldBoxGame::secret_of_the_silver_blades ||
                                  game() == GoldBoxGame::champions_of_krynn) &&
                                 (arg(0) & 0x10) != 0;
        const bool whole_party = (arg(0) & 0xC0) == 0xC0 &&
                                 (death_knights || !(arg(0) & 0x10) || hurt_anyway) &&
                                 (arg(4) & 7) < original_save_abilities.size();
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
            harm.half_on_save = death_knights && (arg(0) & 0x10) != 0;
            if (whole_party)
            {
                if (!(arg(0) & 0x20) && !hurt_anyway)
                {
                    harm.save_ability = original_save_abilities[arg(4) & 7];
                    harm.save_dc = original_save_dc - save_modifier;
                    harm.original_save = static_cast<int>(arg(4) & 7);
                    harm.original_save_modifier = save_modifier;
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
                harm.save_dc = original_save_dc - save_modifier;
                harm.original_save = static_cast<int>(arg(4) & 7);
                harm.original_save_modifier = save_modifier;
                snapshot_.dialogue +=
                    hurt(campaign_->hazard_attack(harm), harm.save_dc, harm.sides);
            }
            else
            {
                if (curse_chosen && (arg(4) & 7) != 0 && (death_knights || !(arg(0) & 0x10)))
                {
                    const auto save = (arg(4) & 7) - 1;
                    harm.save_ability = original_save_abilities[save];
                    harm.save_dc = original_save_dc - save_modifier;
                    harm.original_save = static_cast<int>(save);
                    harm.original_save_modifier = save_modifier;
                }
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
        if (death_knights && !campaign_->spokesman())
        {
            // EclDamage stops the native event when no enabled character
            // remains. Its following COMBAT must never start an empty fight.
            snapshot_.phase = TourPhase::defeated;
            snapshot_.dialogue += "\nThe party is defeated.";
            snapshot_.choices.clear();
            snapshot_.continue_ticket = 0;
            ++snapshot_.revision;
            return true;
        }
        await_continue(request.id);
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
                                                         [](MemberId id)
                                                         {
                                                             return id != 0;
                                                         }));
            const std::uint64_t whole =
                arg(0) == 1 ? 100 : 100 * std::max<std::uint64_t>(1, members);
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
    case 45:
    {
        // Gateway's CALL 8810h changes the current character's coins up into
        // larger ones; here the party's one purse is changed up. The other
        // engine routines are the explorer's (handle_host).
        if (game() != GoldBoxGame::gateway_to_the_savage_frontier || !campaign_ ||
            address_arg(0) != EclAddress{ecl_engine_routines(game()).change_coins_up})
        {
            return false;
        }
        read_character();
        campaign_->set_wealth(change_coins_up(campaign_->state().wealth, campaign_->coin_rates()));
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
        if (arg(0) == 3 && game() == GoldBoxGame::dark_queen_of_krynn && campaign_)
        {
            read_character();
            if (!campaign_->drain_level(campaign_->state().slots.at(selected_character_.index)))
            {
                throw EclError("The selected rules do not support the native level drain");
            }
            reply = character_reply(selected_character_);
            break;
        }
        // Gateway's PROGRAM 12h brings every member to full Hit Points (the
        // wish Krevish saved for Ascore) and compares equal when it healed
        // anyone, else not equal, which its script tests (ECL6:28 0x8907).
        if (arg(0) == 0x12 && campaign_ && game() == GoldBoxGame::gateway_to_the_savage_frontier)
        {
            read_character();
            const bool healed = campaign_->heal_living_members();
            reply = character_reply(selected_character_);
            reply.conditions = EclConditions{healed, !healed, false, false, false, false};
            break;
        }
        // Tyranthraxus defeated (PROGRAM 8): the game is won (area byte 0x3FA,
        // the cell 0x4AFD). The original pauses on its closing words, healing
        // no one; then the script goes on, back to the civilized city. Pools
        // of Darkness's is PROGRAM 1, after Gothmenes falls and Bane's crystal
        // is dust (ECL1:75 0x966C), where its original leaves for the closing
        // film; here the win is told and the game goes on.
        if ((arg(0) == 8 || ((game() == GoldBoxGame::pools_of_darkness ||
                              game() == GoldBoxGame::dark_queen_of_krynn) &&
                             arg(0) == 1)) &&
            campaign_)
        {
            const bool darkness_won = game() == GoldBoxGame::pools_of_darkness;
            read_character();
            snapshot_.dialogue.clear();
            for (const auto &page : town_->closing_story)
            {
                snapshot_.dialogue += page + "\n\n";
            }
            // Curse's ending is its own (WinGame); Pool of Radiance's closing
            // story is followed by OpenGoldBox's line.
            snapshot_.dialogue +=
                game() == GoldBoxGame::pool_of_radiance
                    ? "Tyranthraxus is destroyed, and Phlan is free of him. You have won."
                : darkness_won ? "Gothmenes has fallen, and Bane's crystal is dust. You have won."
                               : "You have won.";
            victory_ = true;
            snapshot_.game_won = true;
            await_continue(request.id);
            return true;
        }
        if (game() == GoldBoxGame::treasures_of_the_savage_frontier &&
            (arg(0) >= 3 && arg(0) <= 5) && campaign_)
        {
            read_character();
            treasures_program(request, arg(0));
            return true;
        }
        if (arg(0) != 9 || !campaign_)
        {
            throw EclError("Training/camp service " + std::to_string(arg(0)));
        }
        // The original inn invokes its pre-camp subroutine before PROGRAM 9.
        read_character();
        // Pools of Darkness's PROGRAM 9 camps and then goes on with the
        // script, ending it only on the area map (eclvm.pas, EclProgram):
        // Elminster's rest in Limbo. Native PROGRAM 9 likewise resumes
        // unless its separate wilderness-view flag (24h) is exactly one.
        // Here the party rests a night, as at
        // Pool of Radiance's inn, and chooses its spells after it.
        if ((game() == GoldBoxGame::pools_of_darkness && !vga_area_map()) ||
            (game() == GoldBoxGame::dark_queen_of_krynn &&
             machine_.variable(EclAddress{0x24}) != 1) ||
            (game() == GoldBoxGame::death_knights_of_krynn &&
             machine_.variable(EclAddress{0x4CF7}) == 0))
        {
            if (!campaign_->rest())
            {
                campaign_->advance_time(std::chrono::hours{8});
            }
            reply = character_reply(selected_character_);
            for (const auto &w : clock_reply().writes)
            {
                reply.writes.push_back(w);
            }
            break;
        }
        // Elsewhere PROGRAM 9 opens the camp menu and ends the script (coab
        // TryEncamp): the nomad camp's lodging, whose night the kobolds may
        // break (ECL7:17 0xA5EF).
        // Curse's always opens the camp menu (TryEncamp in its reconstruction).
        if (game() != GoldBoxGame::pool_of_radiance ||
            machine_.variable(game_cell(ecl_rest_interruption_chance)) == 255 ||
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
    // Native and Matrix LOAD FILES may load a map before the script writes
    // its arrival square/facing. Matrix may still hold an eight-way world
    // heading here. Redraw or event completion publishes the settled3D pose;
    // validating the previous pose at this intermediate load breaks entry.
    if (op == 33 && game() != GoldBoxGame::dark_queen_of_krynn &&
        game() != GoldBoxGame::matrix_cubed)
    {
        publish_pose();
    }
    ++snapshot_.revision;
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
    static constexpr std::array<std::string_view, 4> names{" GOBLINS", " ORCS", " HOBGOBLINS",
                                                           " OGRES"};
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
    const auto set_edge =
        [this](unsigned x, unsigned y, MapDirection side, std::uint8_t wall, std::uint8_t door)
    {
        auto &cell = map_.cells[y * map_.width + x];
        cell.walls[index(side)] = wall;
        cell.doors[index(side)] = door;
        const int nx = static_cast<int>(x) + step_x(side), ny = static_cast<int>(y) + step_y(side);
        if (nx >= 0 && ny >= 0 && nx < static_cast<int>(map_.width) &&
            ny < static_cast<int>(map_.height))
        {
            auto &other =
                map_.cells[static_cast<unsigned>(ny) * map_.width + static_cast<unsigned>(nx)];
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
            map_.cells[y * map_.width + x].event_raw = 0;
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
    const auto edge = edge_ahead(map_, snapshot_.pose);
    for (const auto &shop : shops_)
    {
        if (!shop.entrance || !on_map(shop.archive, shop.record))
        {
            continue;
        }
        for (const auto &[x, y] : shop.markers)
        {
            const bool from_inside =
                edge.x == x && edge.y == y && edge.side == shop.entrance->secret_door;
            const bool from_outside =
                edge.next_x == x && edge.next_y == y && edge.other == shop.entrance->secret_door;
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
        return squares(ox, oy) < squares(x, y)
                   ? std::pair{squares(ox, oy), WandSign{ox, oy, reversed(side)}}
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
            static constexpr std::array<std::string_view, 4> found{
                "You find a secret door in the north wall.",
                "You find a secret door in the east wall.",
                "You find a secret door in the south wall.",
                "You find a secret door in the west wall."};
            snapshot_.dialogue += (snapshot_.dialogue.empty() ? "" : "\n") +
                                  std::string(found[index(reversed(side))]);
            return;
        }
    }
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
                                   bool original_record, GoldBoxGame game,
                                   std::optional<std::array<VgaColor, 256>> native_palette)
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
    auto fighter = staged_fighter(id, std::move(definition), std::move(name), side,
                                  creature->second, exported, difficulty);
    if (area.dark_queen)
    {
        if (!native_palette)
        {
            throw EclError("Native combat icon needs the battlefield palette");
        }
        const auto art = area.dark_queen->icons.icon(icon, *native_palette);
        if (!art)
        {
            throw EclError("Missing Dark Queen combat icon " + std::to_string(icon));
        }
        fighter.footprint = art->footprint;
        return {std::move(fighter), {id, art->ready, art->action}};
    }
    auto art = creature_art(area.combat_archive, icon, exported,
                            fighter.definition.ends_with("-archer"), game);
    if (!art)
    {
        throw EclError("Invalid original combat icon");
    }
    art->entity = id;
    return {std::move(fighter), std::move(*art)};
}

} // namespace opengold::por
