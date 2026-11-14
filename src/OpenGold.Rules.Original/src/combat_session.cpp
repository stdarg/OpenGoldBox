#include "combat_session.h"

#include "opengold/dice.h"
#include "opengold/movement_grid.h"
#include "opengold/original.h"
#include "opengold/original_rules.h"
#include "opengold/original_spells.h"

#include <algorithm>
#include <charconv>
#include <numeric>
#include <optional>
#include <span>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// A fight under the original rules, as Pool of Radiance's combat units play it
// (docs/ORIGINAL-RULES.md, "Combat"): melee and missiles.
namespace opengold::original::detail
{
namespace
{

using original::ClassSlot;
using rules::Cell;
using rules::EntityId;
using rules::Message;
using rules::Side;

// The original counts movement in half-moves: 2 a straight step, 3 a diagonal one.
constexpr rules::StepCosts half_moves{2, 3, 2, 4, 0};
constexpr std::string_view checkpoint_format = "OGORIGINALCOMBAT 20";

enum class Condition : unsigned
{
    okay,
    unconscious,
    dying,
    dead,
    // Off the field: it got away with its Hit Points, or gave up with none.
    fled,
    surrendered,
    // Dead forever, past raising: drained below level 2.
    gone,
    // Turned to stone by a gaze.
    stoned
};

// The record's NPC byte from 0x80 up marks a creature whose morale counts;
// below it are the player's characters, who never check (combatai.pas, CheckMorale).
constexpr unsigned non_player = 0x80;

// The effects that leave a creature helpless: nausea, snake charm, paralysis
// and sleep (charhlp.pas, IsHelpless).
constexpr std::array<int, 4> helpless_effects{31, 51, 52, 53};

// The effects a creature loses as it goes down or leaves the field, the first
// of each code (fxengine.pas, RemoveEffectsHp0).
constexpr std::array<int, 16> effects_ending_at_zero{7,  11, 30, 31, 32, 51,  52, 53,
                                                     54, 58, 59, 95, 98, 137, 74, 75};

// Effect codes the fight itself uses.
namespace effect
{
// Curse's holds: a shambling mound's engulfing (339) and its victim's
// suffocating (213), and an owlbear's hug (344).
constexpr int engulfing = 339, suffocating = 213, hugging = 344;
constexpr int bite_hold = 74, blood_drain = 75, rot = 50, poisoned = 55, mummy_rot = 57,
              paralysis = 52, immobile = 58, see_invisible = 24, invisible = 25, blink = 37,
              regenerating_soon = 59, camouflage = 72, mummy_fear = 82, displacement = 89,
              fighting_on = 95, regeneration = 98, fights_while_down = 99, troll_burn = 100,
              troll_heal = 101, troll_return = 102, gaze_block = 126, bless = 1, curse = 2,
              protection_from_evil = 8, protection_from_good = 9, resist_cold = 10, charm = 11,
              shield = 17, resist_fire = 20, sleep = 53, silence = 21, mirror_image = 28,
              enfeebled = 29, blind = 33, bestow_curse = 36, hasted = 39, missile_protection = 41,
              slowed = 42, protection_from_evil_aura = 45, protection_from_good_aura = 46,
              prayer = 49, coughing = 30, nausea = 31, stinking_cloud = 40, cloudkill = 291,
              enlarged = 12, strengthened = 38, strength_wasted = original::strength_wasted,
              delayed_blast = 500, animated = 32, snake_charm = 51, slow_poison_tick = 15,
              slow_poison = 22, revival = 78, read_magic = 16, dust = 71,
              age_in_years = original::age_in_years;
// Haste keeps whether it has aged its creature in this bit of its strength,
// which no caster's level reaches. The original's bit 0x10 is set already by
// a caster of level 16 to 31, whose Haste so never aged (Bugs).
constexpr int hasted_aged = 0x40;
// Displacement keeps whether it has turned an attack aside in this bit of its strength.
constexpr int displaced = 0x10;
// A ring of invisibility's renewal (Pool of Radiance's 56, Secret's too), a
// gaze a mirror turns back (Pool of Radiance's 127) and Curse's fear (342).
constexpr int renew_invisibility = 56, reflectable_gaze = 127, fear = 342;
// Secret of the Silver Blades' own (canonical_effect, its code plus
// secret_effect_offset).
constexpr int globe_of_invulnerability = 457, petrifying_gaze = 458, needs_plus_three = 460,
              poison_touch = 464, stone_touch = 465, swallows_whole = 466, fire_beside = 467,
              confusing_gaze = 470, luck = 478, iron_golem = 479, fire_at_target = 480,
              spits_acid = 481, dragon_fear = 482, cold_breath = 483, strong_poison_touch = 486,
              ghoul_touch = 488, bashing_proof = 490, fearless = 492, hardy = 494,
              fire_breath = 504, stunned = 506;
} // namespace effect

// What a hit carries, by the attacker's effects, in the original's order: a
// primary attack's hit and a secondary one's (effhndlr.pas, HIT_PHASE_1 and
// HIT_PHASE_2).
constexpr std::array<int, 18> primary_hit_effects{
    85, 86, 87, 68, 79, 80, 76, 73, 77,
    // Curse's (canonical_effect).
    257, 296, 322, 323, 345,
    // Secret's (fxengine.pas, its conditions 2 and 3, in its reconstruction).
    effect::poison_touch, effect::stone_touch, effect::strong_poison_touch, effect::ghoul_touch};
constexpr std::array<int, 15> secondary_hit_effects{
    64, 65, 66, 67, 68, 69, 70, 79, 85, 86, 87,
    // Secret's.
    effect::poison_touch, effect::stone_touch, effect::strong_poison_touch, effect::ghoul_touch};

// An item a creature carries, which it may ready or put away in a fight
// (viewchar.pas, ToggleReadyItem): its record, readied or not, with what is
// left of it, and its type; its inventory entry (0 for a monster's own) and
// name; its count as the fight began (held_count); and whether it is spent.
struct Held
{
    CarriedItem carried;
    std::uint64_t inventory_id{};
    std::string name;
    int start_count{};
    bool gone{};
    // Its place among the creature's readied items, which keeps their order.
    int readied_order{-1};
};

struct Fighter
{
    EntityId id{};
    std::string name, definition;
    // What its numbers are worked out from again as it readies or puts away
    // an item (charhlp.pas, RecalcStats): its record and the items it carries,
    // and the items its numbers were last worked out from, in their order
    // (UsableItem::ordinal), by their places among the held. A creature with
    // no record readies nothing.
    std::optional<por::CharacterRecord> record;
    std::vector<Held> held;
    std::vector<int> worked_from;
    // Its numbers count the items it has not readied too, as a monster's
    // record loads them.
    bool weighs_unreadied{};
    // The fight changed which items it has readied.
    bool readied_changed{};
    // A party member's items as its inventory knows them: each usable item's
    // inventory ID by its place (UsableItem::ordinal), and the item its
    // missile shoots, whose counts left the snapshot reports.
    std::vector<std::pair<int, std::uint64_t>> item_ids;
    std::uint64_t shot_item{};
    Side side{Side::party};
    Cell cell;
    rules::Footprint footprint;
    bool facing_left{};
    Combatant numbers;
    int hit_points{};
    Condition condition{Condition::okay};
    // How far below 0 a dying creature's Hit Points are; past 9 it is dead.
    int overkill{};
    // 0 means it does not act (again) this round.
    int initiative{};
    int half_moves{};
    std::array<int, 2> attacks_left{};
    // The attack form it strikes with next: the second (1) before the first (0).
    int attack_phase{1};
    // How many level-0 enemies it may still sweep through: its attack level,
    // until its first attack of the round.
    int sweep_budget{};
    bool surprised{};
    // Facing, 0 north and clockwise to 7 north-west.
    int direction{};
    // Attacks on it since its own turn began, and the sum of their angles from
    // its facing (0 to 4), mod 8 (cbtacts.pas, NoteAttackDirection).
    int flank_count{}, flank_spread{};
    bool guarding{};
    // Its morale broke at its turn's start: it runs for the edge.
    bool fleeing{};
    // Forced to flee for the rest of the fight: a stirge that has drunk its
    // fill, or undead a cleric turned.
    bool turned{};
    // It has tried to turn undead in this fight: once a fight.
    bool tried_turning{};
    // Casting: whether it may still start a spell this round (damage and
    // silence stop it), the spell it has begun that goes off at its next
    // turn, and the spell it is aiming now with the creatures picked so far
    // and the picks it has left.
    bool cast_allowed{true};
    int pending_spell{}, aiming{}, picks_left{};
    // The rounds a Delayed Blast Fireball it is aiming waits before it goes
    // off, -1 before the caster has chosen (Secret's SpellDelayedBlastFireball).
    int blast_delay{-1};
    std::vector<EntityId> picked;
    // Its side before a charm took it over.
    Side own_side{Side::party};
    // Whether it may still use an item this round (silence and coughing stop
    // it), and the item it is using now, by its ordinal, -1 for none.
    bool item_use_allowed{true};
    int using_item{-1};
    // The original's AI: the approach pattern it walks by (1 to 6, 0 before
    // its first turn) and the enemy it has locked onto (combatai.pas).
    int stance{};
    EntityId lock{};
    // It has attacked this turn.
    bool fought{};
    // A creature the computer plays whose turn left it no way to fight or get
    // away walks the shortest way to an enemy (route_to_enemy) until it
    // fights again.
    bool routing{};
    // What it was before Animate Dead raised it, which it is again as it collapses.
    std::optional<Combatant> before_animation;

    [[nodiscard]] bool able() const noexcept
    {
        return condition == Condition::okay;
    }

    // Down but not dead: out of the fight, where it may still be stood on.
    [[nodiscard]] bool downed() const noexcept
    {
        return condition == Condition::unconscious || condition == Condition::dying;
    }

    [[nodiscard]] int attacks() const noexcept
    {
        return attacks_left[0] + attacks_left[1];
    }

    [[nodiscard]] bool has(int code) const noexcept
    {
        return std::any_of(numbers.effects.begin(), numbers.effects.end(),
                           [code](const Effect &e)
                           {
                               return e.code == code;
                           });
    }

    [[nodiscard]] bool helpless() const noexcept
    {
        return std::any_of(helpless_effects.begin(), helpless_effects.end(),
                           [this](int code)
                           {
                               return has(code);
                           });
    }

    // Removes the first effect of the code, if any.
    void remove(int code)
    {
        auto &effects = numbers.effects;
        const auto found = std::find_if(effects.begin(), effects.end(),
                                        [code](const Effect &e)
                                        {
                                            return e.code == code;
                                        });
        if (found != effects.end())
        {
            effects.erase(found);
        }
    }

    [[nodiscard]] const Effect *find_effect(int code) const
    {
        return const_cast<Fighter *>(this)->find_effect(code);
    }

    [[nodiscard]] Effect *find_effect(int code)
    {
        const auto found = std::find_if(numbers.effects.begin(), numbers.effects.end(),
                                        [code](const Effect &e)
                                        {
                                            return e.code == code;
                                        });
        return found == numbers.effects.end() ? nullptr : &*found;
    }
};

// Whether two creatures stand side by side, any square of one next to any of the other's.
bool adjacent(const Fighter &a, const Fighter &b)
{
    const auto gap = [](int first, int size, int other_first, int other_size)
    {
        return std::max(
            {0, other_first - (first + size - 1), first - (other_first + other_size - 1)});
    };
    const int dx = gap(a.cell.x, a.footprint.width, b.cell.x, b.footprint.width);
    const int dy = gap(a.cell.y, a.footprint.height, b.cell.y, b.footprint.height);
    return std::max(dx, dy) == 1;
}

// The squares a creature covers.
std::vector<Cell> squares_of(const Fighter &f)
{
    std::vector<Cell> squares;
    for (int dy = 0; dy < f.footprint.height; ++dy)
    {
        for (int dx = 0; dx < f.footprint.width; ++dx)
        {
            squares.push_back({f.cell.x + dx, f.cell.y + dy});
        }
    }
    return squares;
}

// The original's distance between two creatures: a straight line's steps
// counted 2 straight and 3 diagonal, halved, between their nearest squares
// (charhlp.pas, CalcDistance; pathing.pas, NextPathStep).
int distance(const Fighter &a, const Fighter &b)
{
    int nearest = 1000;
    for (const auto from : squares_of(a))
    {
        for (const auto to : squares_of(b))
        {
            const int dx = std::abs(to.x - from.x), dy = std::abs(to.y - from.y);
            nearest = std::min(nearest, (2 * std::max(dx, dy) + std::min(dx, dy)) / 2);
        }
    }
    return nearest;
}

// Whether a line runs from a square of one creature to a square of the other
// that walls do not block; creatures do not (pathing.pas, BuildTargetList).
bool sees(const rules::Battlefield &board, const Fighter &a, const Fighter &b)
{
    for (const auto from : squares_of(a))
    {
        for (const auto to : squares_of(b))
        {
            if (rules::has_line_of_sight(board, from, to))
            {
                return true;
            }
        }
    }
    return false;
}

// The original's eight directions: 0 north and clockwise to 7 north-west.
constexpr std::array<int, 8> step_x{0, 1, 1, 1, 0, -1, -1, -1};
constexpr std::array<int, 8> step_y{-1, -1, 0, 1, 1, 1, 0, -1};

// Whether a direction leads from one cell toward another: the other cell lies
// in the 90-degree cone around the cell one step that way (pathing.pas, IsDirToward).
bool leads_toward(Cell from, Cell to, int direction)
{
    const int nx = from.x + step_x[static_cast<std::size_t>(direction)];
    const int ny = from.y + step_y[static_cast<std::size_t>(direction)];
    const int tx = to.x, ty = to.y;
    if (from == to || (nx == tx && ny == ty))
    {
        return true;
    }
    switch (direction)
    {
    case 0:
        return (tx >= nx && ty <= nx - tx + ny) || (tx <= nx && ty <= tx - nx + ny);
    case 1:
        return (tx >= nx && ty <= nx - tx + ny) || (tx >= nx + ny - ty && ty <= ny);
    case 2:
        return (tx >= nx + ny - ty && ty <= ny) || (tx >= nx + ty - ny && ty >= ny);
    case 3:
        return (tx >= nx + ty - ny && ty >= ny) || (tx >= nx && ty >= tx - nx + ny);
    case 4:
        return (tx >= nx && ty >= tx - nx + ny) || (tx <= nx && ty >= nx - tx + ny);
    case 5:
        return (tx <= nx && ty >= nx - tx + ny) || (tx <= nx + ny - ty && ty >= ny);
    case 6:
        return (tx <= nx + ny - ty && ty >= ny) || (tx <= nx + ty - ny && ty <= ny);
    default:
        return (tx <= nx + ty - ny && ty <= ny) || (tx <= nx && ty <= tx - nx + ny);
    }
}

// The first direction that leads from one cell toward another (cbtacts.pas, DirTowardTarget).
int direction_toward(Cell from, Cell to)
{
    for (int direction = 0; direction < 8; ++direction)
    {
        if (leads_toward(from, to, direction))
        {
            return direction;
        }
    }
    return 0;
}

// The English of a message: its template with each {name} replaced.
std::string english(const Message &message)
{
    auto text = message.source;
    for (const auto &argument : message.arguments)
    {
        const auto key = "{" + argument.name + "}";
        for (auto at = text.find(key); at != std::string::npos; at = text.find(key))
        {
            text.replace(at, key.size(), argument.value);
        }
    }
    return text;
}

// A checkpoint's text: numbers and length-prefixed strings, space-separated.
class Writer
{
  public:
    template <class... T> Writer &operator()(const T &...values)
    {
        (put(values), ...);
        return *this;
    }

    [[nodiscard]] std::string text() const
    {
        return out_.str();
    }

  private:
    std::ostringstream out_;

    void put(const std::string &text)
    {
        out_ << text.size() << ':' << text << ' ';
    }

    void put(bool value)
    {
        out_ << (value ? 1 : 0) << ' ';
    }

    template <class T>
        requires std::is_integral_v<T>
    void put(T value)
    {
        out_ << +value << ' ';
    }
};

class Reader
{
  public:
    explicit Reader(std::string_view text) : text_(text)
    {
    }

    // Unsigned numbers read as unsigned: the random state uses all 64 bits.
    template <class Number> Number number()
    {
        skip();
        Number value{};
        const auto [end, error] =
            std::from_chars(text_.data() + at_, text_.data() + text_.size(), value);
        if (error != std::errc{})
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        at_ = static_cast<std::size_t>(end - text_.data());
        return value;
    }

    template <class T> T as()
    {
        if constexpr (std::is_same_v<T, std::uint64_t>)
        {
            return number<std::uint64_t>();
        }
        return static_cast<T>(number<long long>());
    }

    std::string text()
    {
        const auto size = static_cast<std::size_t>(number<long long>());
        if (at_ >= text_.size() || text_[at_] != ':' || size > text_.size() - at_ - 1)
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        std::string value(text_.substr(at_ + 1, size));
        at_ += size + 1;
        return value;
    }

    [[nodiscard]] bool finished()
    {
        skip();
        return at_ == text_.size();
    }

  private:
    std::string_view text_;
    std::size_t at_{};

    void skip()
    {
        while (at_ < text_.size() && text_[at_] == ' ')
        {
            ++at_;
        }
    }
};

int attack_modifier(const Fighter &attacker, const Fighter &defender, GoldBoxGame game);

// Whether Dispel Evil takes a creature for evil: by its alignment in Curse,
// by its record's flag in Secret (effhndlr.pas, EffDispelEvil and
// EffDispelEvilTouch, in its reconstruction).
bool dispel_evil_takes(const Fighter &f, GoldBoxGame game)
{
    if (game == GoldBoxGame::secret_of_the_silver_blades)
    {
        return (f.numbers.traits & trait::dispel_evil) != 0;
    }
    const int alignment = f.numbers.alignment;
    return alignment == 2 || alignment == 5 || alignment == 8;
}

// Haste doubles a creature's rates of movement and attacks and Slow halves
// them; together they cancel (effhndlr.pas, EffHasted, EffSlowed).
int paced(const Fighter &f, int rate)
{
    const bool hasted = f.has(effect::hasted), slowed = f.has(effect::slowed);
    if (hasted && !slowed)
    {
        return rate * 2;
    }
    return slowed && !hasted ? rate / 2 : rate;
}

// Its movement in squares this round, as the original weighs flight and
// morale with it (cbtacts.pas, CalcCreatureMovement).
int movement_of(const Fighter &f)
{
    return paced(f, 2 * f.numbers.movement) / 2;
}

// A caster's level for a spell: its cleric or magic-user level, 12 for an
// item's own power (magic.pas, GetCasterLevel). Curse and the games after it
// count a paladin's cleric spells at its level less 8 (Secret at its level),
// a ranger's druid spells at its level less 7 and its magic-user spells at
// its level less 8, and cast at level 6 for one with no cleric or magic-user
// levels that is no paladin above 8 nor ranger above 7.
int caster_level(const Fighter &caster, const Spell &spell, GoldBoxGame game)
{
    // From an item a spell is cast at level 6, an item's own power at 12
    // (magic.pas, GetCasterLevel).
    if (caster.using_item >= 0)
    {
        return spell.caster_class == SpellClass::item ? 12 : 6;
    }
    const auto level = [&caster](ClassSlot slot)
    {
        return caster.numbers.class_levels[static_cast<std::size_t>(slot)];
    };
    if (spell.caster_class == SpellClass::item)
    {
        return 12;
    }
    if (game == GoldBoxGame::pool_of_radiance)
    {
        return level(spell.caster_class == SpellClass::cleric ? ClassSlot::cleric
                                                              : ClassSlot::magic_user);
    }
    const int paladin = level(ClassSlot::paladin), ranger = level(ClassSlot::ranger);
    if (level(ClassSlot::cleric) == 0 && level(ClassSlot::magic_user) == 0 && paladin < 9 &&
        ranger < 8)
    {
        return 6;
    }
    const int paladin_less = game == GoldBoxGame::secret_of_the_silver_blades ? 0 : 8;
    switch (spell.caster_class)
    {
    case SpellClass::cleric:
        return std::max(level(ClassSlot::cleric), paladin - paladin_less);
    case SpellClass::druid:
        return std::max(ranger - 7, 0);
    default:
        return std::max(level(ClassSlot::magic_user), ranger - 8);
    }
}

// A spell's range in squares at a level: a touch is 1 (magic.pas).
int spell_range(const Spell &spell, int level)
{
    if (spell.range_fixed == 255)
    {
        return 1;
    }
    const int range = spell.range_fixed + spell.range_per_level * level;
    return range == 0 && spell.area != 0 ? 1 : range;
}

// The original's doubled distance between two squares along a line: 2 a
// straight step, 3 a diagonal one (pathing.pas, NextPathStep).
int doubled_distance(Cell a, Cell b)
{
    const int dx = std::abs(a.x - b.x), dy = std::abs(a.y - b.y);
    return 2 * std::max(dx, dy) + std::min(dx, dy);
}

// Curse's Fire Shield, whose caster chooses its flames as it goes off, and
// the choice: the AI's (and Quick's) at random (SpellFireShield).
constexpr int fire_shield = 185, delayed_blast_spell = 315;
enum class Flames : unsigned
{
    random,
    hot,
    cold
};

// The spells built so far; the rest are not offered yet.
bool spell_built(int number)
{
    constexpr std::array built{
        26, 27, 36, 41, 46, 57, 58, 59, 60, 61, 62, 63, 64, 65, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        12, 13, 15, 16, 17, 19, 20, 21, 23, 24, 25, 29, 30, 32, 33, 35, 37, 38, 40, 42, 34, 43, 44,
        45, 47, 48, 49, 50, 51, 52, 53, 54, 55,
        // Curse's (spells.cpp, curse_spells).
        158, 166, 168, 169, 170, 171, 172, 173, 174, 176, 177, 178, 179, 180, 181, 182, 183, 184,
        185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200,
        // Secret's (spells.cpp, secret_spells).
        236, 256, 290, 296, 298, 310, 311, 312, 313, 314, 315, 316, 317};
    return std::find(built.begin(), built.end(), number) != built.end();
}

// A straight line walked a square at a time, its doubled distance growing by
// 2 a straight step and 3 a diagonal one (pathing.pas, PathFind and NextPathStep).
struct LineWalk
{
    Cell start, end, at;
    int across{}, down{}, step_x{}, step_y{}, error{}, diagonal_step{}, straight_step{}, distance{};

    LineWalk(Cell from, Cell to) : start(from), end(to), at(from)
    {
        across = std::abs(to.x - from.x);
        down = std::abs(to.y - from.y);
        step_x = (to.x > from.x) - (to.x < from.x);
        step_y = (to.y > from.y) - (to.y < from.y);
        const int major = std::max(across, down), minor = std::min(across, down);
        error = 2 * minor - major;
        diagonal_step = 2 * (minor - major);
        straight_step = 2 * minor;
    }

    // One step on; false at the line's end.
    bool next()
    {
        const bool x_major = across > down;
        if ((x_major && at.x == end.x) || (!x_major && at.y == end.y))
        {
            return false;
        }
        if (error >= 0)
        {
            (x_major ? at.y : at.x) += x_major ? step_y : step_x;
            error += diagonal_step;
            distance += 3;
        }
        else
        {
            error += straight_step;
            distance += 2;
        }
        (x_major ? at.x : at.y) += x_major ? step_x : step_y;
        return true;
    }
};

// A cloud on the field: the caster's, its number among the caster's clouds of
// its kind, and the squares it covers where the ground can be walked. A
// stinking cloud covers its corner and the squares east, south-east and south
// of it (magic.pas, SpellStinkingCloud); Curse's Cloudkill the square aimed
// at, kept as its corner, and the eight around (SpellCloudkill).
enum class CloudKind
{
    stinking,
    deadly
};

struct Cloud
{
    EntityId caster{};
    int index{};
    Cell corner;
    std::array<bool, 9> covers{};
    CloudKind kind{CloudKind::stinking};

    [[nodiscard]] std::vector<Cell> squares() const
    {
        if (kind == CloudKind::stinking)
        {
            return {corner, Cell{corner.x + 1, corner.y}, Cell{corner.x + 1, corner.y + 1},
                    Cell{corner.x, corner.y + 1}};
        }
        std::vector<Cell> around{corner};
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (dx != 0 || dy != 0)
                {
                    around.push_back({corner.x + dx, corner.y + dy});
                }
            }
        }
        return around;
    }
};

class Session final : public rules::CombatSession
{
  public:
    Session(rules::Identity identity, rules::Battlefield board, std::vector<Fighter> fighters,
            std::uint64_t seed, unsigned morale, bool outdoors, bool escape_ends_fight)
        : identity_(std::move(identity)), board_(std::move(board)), fighters_(std::move(fighters)),
          random_{seed}, morale_threshold_(std::min(morale, 100U)), outdoors_(outdoors),
          escape_ends_fight_(escape_ends_fight)
    {
        rules::validate_battlefield(board_);
        log({"Combat begins.", {}});
        combat_start();
        update_hostile_hit_points();
        // A fight ends only as a round ends, unless a side has no one able to
        // fight from the start (combat.pas, CombatMain and EndOfRound).
        settle_outcome();
        if (outcome_ == rules::Outcome::ongoing)
        {
            begin_round();
            next_actor();
        }
    }

    static std::unique_ptr<Session> restored(rules::Identity identity, std::string_view checkpoint,
                                             const Content &content);

    [[nodiscard]] rules::Snapshot snapshot() const override;
    [[nodiscard]] std::vector<rules::Command> legal_commands() const override;
    [[nodiscard]] std::vector<Cell> movement_reach(EntityId actor) const override;
    bool submit(const rules::Command &command) override;
    void turn_against_party(const std::vector<EntityId> &turning) override;
    [[nodiscard]] std::string save() const override;

  private:
    Session() = default;

    rules::Identity identity_;
    rules::Battlefield board_;
    std::vector<Fighter> fighters_;
    RandomState random_;
    unsigned round_{};
    std::uint64_t revision_{1};
    EntityId actor_{};
    rules::Outcome outcome_{rules::Outcome::ongoing};
    std::vector<rules::LogEntry> log_;
    // The encounter's morale, 0 to 100, which the opposition's whole strength
    // is weighed against (the script's MORALE_THRESHOLD).
    unsigned morale_threshold_{100};
    // Out of doors (rules::Encounter::outdoors).
    bool outdoors_{};
    // Gateway's maze fights: a member's escape ends them (rules::Encounter).
    bool escape_ends_fight_{};
    // The side the member who ended such a fight ran off.
    std::optional<unsigned> escape_side_;
    std::vector<Cloud> clouds_;

    // Delayed Blast Fireballs set and not yet gone off: the caster's, its
    // number among the caster's, the square, and the caster's level.
    struct DelayedBlast
    {
        EntityId caster{};
        int index{};
        Cell cell;
        int level{};
    };

    std::vector<DelayedBlast> blasts_;
    // The opposition's Hit Points left of its whole, in steps of 5, as of the
    // round's start (cbtacts.pas, UpdateHostileHpPercent).
    int hostile_hit_points_percent_{100};
    // Secret of the Silver Blades orders the round once as it begins: those
    // yet to act this round, first first (combat.pas, OrderCreatures, in its
    // reconstruction). Empty in the earlier games, which pick the highest
    // initiative left each time.
    std::vector<EntityId> turn_order_;

    [[nodiscard]] bool secret() const
    {
        return game_of(identity_) == GoldBoxGame::secret_of_the_silver_blades;
    }

    // A spell as this fight's game has it (Gateway changes four rows).
    [[nodiscard]] const Spell *game_spell(int number) const
    {
        return spell(number, game_of(identity_));
    }

    int roll(int sides)
    {
        return roll_die(random_, sides);
    }

    [[nodiscard]] Fighter *find(EntityId id);
    [[nodiscard]] const Fighter *find(EntityId id) const;
    [[nodiscard]] rules::MovementGrid grid_for(const Fighter &mover) const;
    void log(Message message);
    void begin_round();
    void age_the_hasted();
    void make_younger(Fighter &user);
    [[nodiscard]] std::vector<Held> held_now(const Fighter &f) const;
    [[nodiscard]] Combatant numbers_with(const Fighter &f, const std::vector<Held> &held,
                                         std::vector<int> &order) const;
    void rework_gear(Fighter &f, const std::vector<Held> &before_held,
                     std::vector<Held> after_held);
    void toggle_readied(Fighter &f, std::size_t index);
    void equip_best(Fighter &f);
    [[nodiscard]] int weapon_priority(const Fighter &f, const Held &weapon, int other_hands) const;
    void next_actor();
    void end_round();
    void end_actions(Fighter &fighter);
    void note_attack(const Fighter &attacker, Fighter &defender);
    void make_attack(Fighter &attacker, Fighter &defender, bool from_behind);
    [[nodiscard]] std::optional<std::string> attack_verb(const Fighter &attacker,
                                                         const Fighter &target) const;
    [[nodiscard]] bool enemy_next_to(const Fighter &f) const;
    void shoot(Fighter &attacker, Fighter &target);
    bool sweep(Fighter &attacker, Fighter &defender);
    void step_to(Fighter &mover, Cell next);
    void free_attacks(Fighter &mover, Cell next);
    void guards_strike(Fighter &mover);
    [[nodiscard]] bool can_bandage(const Fighter &bandager, const Fighter &dying) const;
    void inflict(Fighter &target, int damage);
    void slay_helpless(Fighter &attacker, Fighter &defender);
    void end_effects_at_zero(Fighter &f);
    void release(Fighter &holder, int code);
    void on_hit(Fighter &attacker, Fighter &defender, int phase, int primary_hits, int natural);
    void poison(Fighter &victim, int modifier);
    void paralyze(Fighter &victim, int minutes, int modifier);
    void kill(Fighter &victim, std::string_view message = "{name} is killed.");
    void drain_levels(Fighter &victim, int levels);
    void rake(Fighter &attacker, Fighter &victim);
    void attached_turn(Fighter &f);
    void curse_turn_start(Fighter &f);
    void turn_undead(Fighter &cleric);
    void begin_cast(Fighter &caster, int spell_number);
    void start_aiming(Fighter &caster, int spell_number);
    void resolve_spell(Fighter &caster, std::vector<EntityId> targets, Cell aim,
                       Flames flames = Flames::random);
    void lose_spell(Fighter &caster);
    void spend_item(Fighter &user, int ordinal, int number);
    [[nodiscard]] std::vector<rules::Command> aiming_commands(const Fighter &caster) const;
    [[nodiscard]] bool reachable_by_spell(const Fighter &caster, const Fighter &target) const;
    [[nodiscard]] std::vector<EntityId> within(Cell aim, int radius) const;
    void cast_generic(Fighter &caster, int number, int level, const std::vector<EntityId> &targets,
                      int damage, int element, int save_modifier = 0, int minutes = -1);
    [[nodiscard]] bool silenced(const Fighter &f) const;
    [[nodiscard]] int prayer_bonus(const Fighter &f) const;
    [[nodiscard]] bool protected_from(const Fighter &f, int code, int aura) const;
    bool mirror_absorbs(Fighter &target);
    [[nodiscard]] bool aura_protects(const Fighter &attacker, const Fighter &defender) const;
    bool add_with_message(Fighter &target, Effect added, bool negates, bool saved, int element,
                          int caster_level, const Spell *spell);
    void nonweapon_damage(Fighter &target, int damage, int save_result, bool saved, int element,
                          int caster_level, int spell_number, bool single_target);
    void charm(Fighter &target, Side charmer);
    void cast_sleep(int level, Cell aim);
    [[nodiscard]] std::vector<EntityId> path_area(const Fighter &caster, Cell aim, int max_range,
                                                  int width) const;
    void lay_cloud(Fighter &caster, int level, Cell aim, CloudKind kind);
    void remove_with_handler(Fighter &f, std::size_t node);
    void dispel(Fighter &target, int level);
    void animate_dead(Fighter &caster, int level);
    void slow_poison(Fighter &target, int level);
    void revive(Fighter &f);
    [[nodiscard]] bool in_cloud(const Fighter &f, CloudKind kind) const;
    void breathe_cloud(Fighter &f);
    void breathe_stinking_cloud(Fighter &f);
    void breathe_cloudkill(Fighter &f);
    bool lightning_at(Cell cell, int damage, int level, int save = 4);
    void lightning_path(Cell origin, Cell aim, int length, int damage, int level, bool bounce,
                        int save = 4);
    void monster_specials(Fighter &f);
    void curse_specials(Fighter &f);
    void secret_specials(Fighter &f);
    void petrifying_gaze(Fighter &gazer);
    bool secret_breath(Fighter &dragon, int code, int length, int element);
    [[nodiscard]] Fighter *secret_target(Fighter &f, int range, bool skip_helpless);
    void turn_to_stone(Fighter &victim);
    void cast_secret_spell(Fighter &caster, int number, int level,
                           const std::vector<EntityId> &targets, Cell aim);
    void heal_all_but_1d4(Fighter &target);
    void cast_delayed_blast(Fighter &caster, int level, const std::vector<EntityId> &targets,
                            Cell aim);
    void delayed_blast_goes_off(Fighter &caster, int index);
    [[nodiscard]] std::vector<EntityId> fireball_reach(const std::vector<EntityId> &targets,
                                                       Cell aim) const;
    void recount_strength(Fighter &f);
    void cast_strength_spell(Fighter &caster, int number, int level,
                             const std::vector<EntityId> &targets);
    void cast_death_spell(const std::vector<EntityId> &targets);
    [[nodiscard]] bool turns_aside_slaying(Fighter &target);
    void secret_poison(Fighter &victim, int modifier);
    void swallow_whole(Fighter &swallower, Fighter &victim);
    void iron_golem_absorbs(Fighter &golem, int damage, int element);
    void dragon_fear(const Fighter &dragon);
    void order_turns();
    void reorder_after_cast(Fighter &caster);
    [[nodiscard]] bool breathes_clouds_freely(const Fighter &f) const;
    [[nodiscard]] bool refuses_spell(const Fighter &target, int number) const;
    void beholder_rays(Fighter &f);
    [[nodiscard]] int uses(Fighter &f, int code);
    void spend_use(Fighter &f, int code);
    [[nodiscard]] EntityId standing_at(Cell cell) const;
    void combat_start();
    void round_end_effects();
    void expire(Fighter &f, const Effect &ended);
    void went_down(Fighter &f);
    [[nodiscard]] bool cell_free(const Fighter &f) const;
    void stand_up(Fighter &f, int hit_points, std::string_view message);
    [[nodiscard]] bool can_target(const Fighter &attacker, const Fighter &target) const;
    [[nodiscard]] bool turned_aside(Fighter &defender);
    [[nodiscard]] int weapon_damage_defenses(const Fighter &attacker, Fighter &defender, int damage,
                                             int phase, int &primary_hits);
    void age_effects();
    [[nodiscard]] bool saving_throw(const Fighter &f, int save, int modifier,
                                    const Fighter *caster = nullptr, int element = 0);
    void update_hostile_hit_points();
    [[nodiscard]] int highest_enemy_movement(const Fighter &f) const;
    bool check_morale(Fighter &f);
    void attempt_flee(Fighter &f);
    void end_fight_by_escape(const Fighter &escaped);
    // The original's AI (combatai.pas).
    // A party member on Quick casts only with Quick magic on (`magic`).
    void ai_turn(Fighter &f, bool magic);
    bool ai_use_item(Fighter &f);
    bool ai_cast(Fighter &f, bool magic);
    [[nodiscard]] bool ai_decides(const Fighter &f, int number, int least_priority) const;
    bool ai_aim(Fighter &f);
    [[nodiscard]] Fighter *cure_search(const Fighter &f);
    bool acquire_target(Fighter &f, int range, bool relaxed, bool force_new);
    [[nodiscard]] std::vector<EntityId> enemies_within(const Fighter &f, int range,
                                                       bool sight) const;
    bool ai_move_attack(Fighter &f);
    bool ai_step(Fighter &f, int &last_direction, int &stuck);
    bool take_ai_step(Fighter &f, int direction, int &last_direction);
    bool can_step(const Fighter &f, int direction, bool &edge) const;
    // Whether Secret's AI balks at a step into a cloud: not at all, unless it
    // saves against paralysis, or always.
    enum class CloudBalk
    {
        none,
        unless_saved,
        always
    };
    [[nodiscard]] CloudBalk cloud_balk(const Fighter &f, const Fighter &moved) const;

    struct Route
    {
        int direction{};
        EntityId target{};
    };

    [[nodiscard]] std::optional<Route> route_to_enemy(const Fighter &f) const;
    void finish_round(Fighter &f);
    [[nodiscard]] bool missile_in_hand(const Fighter &f) const;
    void put_out_of_play(Fighter &f, Condition condition);
    [[nodiscard]] std::vector<Cell> off_field_steps(const Fighter &f) const;
    void settle_outcome();
};

Fighter *Session::find(EntityId id)
{
    const auto found = std::find_if(fighters_.begin(), fighters_.end(),
                                    [id](const Fighter &f)
                                    {
                                        return f.id == id;
                                    });
    return found == fighters_.end() ? nullptr : &*found;
}

const Fighter *Session::find(EntityId id) const
{
    return const_cast<Session *>(this)->find(id);
}

void Session::log(Message message)
{
    log_.push_back({english(message), std::move(message)});
}

// Every creature able to act rolls initiative: d6 with its Dexterity's
// missile adjustment, at least 1; a surprised one loses 6 in the first round,
// and a result outside 0 to 20 lets it not act (combat.pas, BeginRound).
void Session::begin_round()
{
    ++round_;
    for (auto &f : fighters_)
    {
        f.initiative = 0;
        if (!f.able())
        {
            continue;
        }
        f.initiative = std::max(roll(6) + f.numbers.initiative_bonus, 1);
        if (round_ == 1 && f.surprised)
        {
            f.initiative -= 6;
            if (f.initiative < 0 || f.initiative > 20)
            {
                f.initiative = 0;
            }
        }
        f.half_moves = f.has(effect::immobile) ? 0 : paced(f, 2 * f.numbers.movement);
        f.cast_allowed = true;
        f.item_use_allowed = true;
        f.using_item = -1;
        f.pending_spell = f.aiming = f.picks_left = 0;
        f.picked.clear();
        f.attack_phase = 1;
        f.sweep_budget = f.numbers.attack_level;
        for (std::size_t form = 0; form < f.attacks_left.size(); ++form)
        {
            f.attacks_left[form] = attacks_this_round(
                paced(f, f.numbers.attacks[form].rate_per_two_rounds), static_cast<int>(round_));
        }
        // A loaded missile weapon attacks at its own rate, no more often than
        // it has shots (cbtacts.pas, RecalcAttacksLeft).
        if (const auto &missile = f.numbers.missile; missile && missile->loaded)
        {
            f.attacks_left[0] = attacks_this_round(paced(f, missile->rate_per_two_rounds),
                                                   static_cast<int>(round_));
            if (!missile->needs_no_ammunition)
            {
                f.attacks_left[0] = std::min(f.attacks_left[0], std::max(missile->quantity, 1));
            }
        }
    }
    age_the_hasted();
    if (secret())
    {
        order_turns();
    }
}

// Secret's order for the round: each creature in turn goes before the first
// already placed whose initiative is lower, or the same on a d2 of 1
// (combat.pas, OrderCreatures, in its reconstruction).
void Session::order_turns()
{
    turn_order_.clear();
    for (const auto &f : fighters_)
    {
        auto place = turn_order_.begin();
        while (place != turn_order_.end())
        {
            const int other = find(*place)->initiative;
            if (f.initiative > other || (f.initiative == other && roll(2) == 1))
            {
                break;
            }
            ++place;
        }
        turn_order_.insert(place, f.id);
    }
}

// A Secret caster who begins a spell moves back among those yet to act, past
// each whose initiative is higher than its own, or the same on a d2 of 1
// (cbtacts.pas, CastSpellCombat, in its reconstruction).
void Session::reorder_after_cast(Fighter &caster)
{
    auto place = turn_order_.begin();
    while (place != turn_order_.end())
    {
        const int other = find(*place)->initiative;
        if (other < caster.initiative || (other == caster.initiative && roll(2) != 1))
        {
            break;
        }
        ++place;
    }
    turn_order_.insert(place, caster.id);
}

// The highest initiative left acts next; a tie goes to the higher of fresh
// d100 rolls (combat.pas, NextCreature). In Secret the next in the round's
// order still able to act does. With none left the round ends.
void Session::next_actor()
{
    actor_ = 0;
    while (outcome_ == rules::Outcome::ongoing)
    {
        Fighter *best = nullptr;
        int best_tiebreak = 0;
        while (secret() && !best && !turn_order_.empty())
        {
            auto *first = find(turn_order_.front());
            turn_order_.erase(turn_order_.begin());
            if (first->able() && first->initiative > 0)
            {
                best = first;
            }
        }
        for (auto &f : fighters_)
        {
            // Secret's order has picked already.
            if (secret() || !f.able() || f.initiative <= 0)
            {
                continue;
            }
            if (!best || f.initiative > best->initiative)
            {
                best = &f;
                best_tiebreak = roll(100);
            }
            else if (f.initiative == best->initiative)
            {
                const auto tiebreak = roll(100);
                // A full tie goes to the later creature (combat.pas, NextCreature).
                if (tiebreak >= best_tiebreak)
                {
                    best = &f;
                    best_tiebreak = tiebreak;
                }
            }
        }
        if (best)
        {
            // A turn starts afresh: no flanking counted and no guard kept
            // (combat.pas, the creature's turn).
            best->flank_count = best->flank_spread = 0;
            best->guarding = false;
            // A helpless creature's turn ends as it starts (effhndlr.pas, the
            // helpless effects at TURN_START).
            // So does a stunned one's in Secret (Power Word Stun's 106, EffHelpless
            // at its ACTION_START), though it is not helpless.
            if (best->helpless() || best->has(effect::stunned))
            {
                end_actions(*best);
                continue;
            }
            // A holding bite or a drinking stirge spends its turn on its victim
            // (effhndlr.pas, ACTION_START).
            attached_turn(*best);
            curse_turn_start(*best);
            if (best->initiative <= 0 || !best->able() || check_morale(*best))
            {
                continue;
            }
            // Silence within a square stops its casting for the round
            // (effhndlr.pas, EffSilence at ACTION_START).
            if (silenced(*best) && best->cast_allowed)
            {
                best->cast_allowed = false;
                best->item_use_allowed = false;
                log({"{name} is silenced.", {{"name", best->name}}});
            }
            // Coughing stops casting too (effhndlr.pas, EffCoughing).
            if (best->has(effect::coughing) && best->cast_allowed)
            {
                best->cast_allowed = false;
                best->item_use_allowed = false;
                log({"{name} is coughing.", {{"name", best->name}}});
            }
            // A spell begun earlier in the round goes off now (combat.pas,
            // ManualControl): its targets are chosen as it does.
            if (best->pending_spell)
            {
                const int pending = best->pending_spell;
                best->pending_spell = 0;
                start_aiming(*best, pending);
                if (best->initiative <= 0)
                {
                    continue;
                }
            }
            actor_ = best->id;
            return;
        }
        end_round();
    }
}

// The round ends: each dying creature bleeds a point and dies past 9 below 0
// (combat.pas, EndOfRound).
void Session::end_round()
{
    age_effects();
    update_hostile_hit_points();
    round_end_effects();
    for (auto &f : fighters_)
    {
        breathe_cloud(f);
    }
    for (auto &f : fighters_)
    {
        if (f.condition == Condition::dying && ++f.overkill > 9)
        {
            f.condition = Condition::dead;
            log({"{name} dies of its wounds.", {{"name", f.name}}});
        }
    }
    settle_outcome();
    if (outcome_ == rules::Outcome::ongoing)
    {
        begin_round();
    }
}

// Its actions end: no initiative, guard or movement left, though the attacks
// it has not made stay for a guard's strike or a free attack (charhlp.pas, EndActions).
void Session::end_actions(Fighter &fighter)
{
    fighter.initiative = 0;
    fighter.pending_spell = 0;
    fighter.guarding = false;
    fighter.half_moves = 0;
}

// Counts an attack on the defender for flanking: one more attacker, and the
// angle between its facing and the attack (0 to 4) added to its spread, mod 8
// (cbtacts.pas, NoteAttackDirection).
void Session::note_attack(const Fighter &attacker, Fighter &defender)
{
    ++defender.flank_count;
    int angle = (direction_toward(defender.cell, attacker.cell) - defender.direction + 8) % 8;
    if (angle > 4)
    {
        angle = 8 - angle;
    }
    defender.flank_spread = (defender.flank_spread + angle) % 8;
}

// An attack (cbtacts.pas, MakeAttack and RollAttacks). The defender turns to
// face the attacker unless it is struck from behind or already attacked three
// times. A thief backstabs a defender attacked more than once from the side it
// faces away from, against its Armor Class from behind less 2, for its level
// div 4 + 2 times the damage; others strike from behind there once the attacks
// on it have spread wide. Attacks run from the attacker's current form down to
// the first until the defender goes down; with none left its actions end.
void Session::make_attack(Fighter &attacker, Fighter &defender, bool from_behind)
{
    if (defender.flank_count < 3 && !from_behind)
    {
        defender.direction = direction_toward(defender.cell, attacker.cell);
    }
    attacker.direction = direction_toward(attacker.cell, defender.cell);
    attacker.lock = defender.id;
    attacker.facing_left = defender.cell.x < attacker.cell.x;
    const bool behind_its_back =
        defender.flank_count > 1 &&
        direction_toward(attacker.cell, defender.cell) == defender.direction;
    // Secret's thief backstabs only a defender of one square next to it
    // (cbtacts.pas, IsBackstab, in its reconstruction).
    const bool one_square = defender.footprint.width == 1 && defender.footprint.height == 1;
    const bool backstab = attacker.numbers.backstabs && behind_its_back &&
                          (!secret() || (one_square && adjacent(attacker, defender)));
    const bool behind = from_behind || (behind_its_back && defender.flank_spread > 4);
    // Shield keeps the front Armor Class at 3 or better (effhndlr.pas, EffShield).
    const int front = defender.has(effect::shield) ? std::min(defender.numbers.armor_class, 3)
                                                   : defender.numbers.armor_class;
    // Blindness leaves both Armor Classes 4 worse (effhndlr.pas, EffBlind);
    // coughing leaves the rear one 2 worse, no worse than 10, and the front as
    // bad (EffCoughing).
    // Faerie Fire leaves both 2 worse (Curse made them better, docs/ORIGINAL-RULES.md, Bugs).
    const int blind = (defender.has(effect::blind) ? 4 : 0) + (defender.has(207) ? 2 : 0);
    const bool coughs = defender.has(effect::coughing);
    const int rear = coughs ? std::max(std::min(defender.numbers.rear_armor_class + 2, 10),
                                       defender.numbers.rear_armor_class)
                            : defender.numbers.rear_armor_class;
    // Secret takes 4 from the Armor Class from behind for a backstab (RollAttacks).
    const int backstab_armor_class = rear + (secret() ? 4 : 2);
    const int armor_class = (backstab ? backstab_armor_class
                             : behind ? rear
                             : coughs ? rear
                                      : front) +
                            blind;
    if (defender.helpless())
    {
        slay_helpless(attacker, defender);
        return;
    }
    if (backstab)
    {
        log({"{attacker} backstabs {name}.",
             {{"attacker", attacker.name}, {"name", defender.name}}});
    }
    int primary_hits = 0;
    for (int phase = attacker.attack_phase; phase >= 0 && defender.able(); --phase)
    {
        auto &left = attacker.attacks_left[static_cast<std::size_t>(phase)];
        auto attack = attacker.numbers.attacks[static_cast<std::size_t>(phase)];
        if (phase == 0 && defender.numbers.large && attacker.numbers.large_attack)
        {
            attack = *attacker.numbers.large_attack;
        }
        // Each attack is spent before it is rolled, so that what a hit carries
        // (a rake) may take over the rest.
        while (left > 0 && defender.able())
        {
            --left;
            attacker.attack_phase = phase;
            const auto natural = roll(20);
            // Attacking makes it visible (fxengine.pas, AttackRoll).
            while (attacker.has(effect::invisible))
            {
                attacker.remove(effect::invisible);
            }
            const int modifier = attack_modifier(attacker, defender, game_of(identity_)) +
                                 prayer_bonus(attacker) -
                                 (aura_protects(attacker, defender) ? 2 : 0);
            // One made helpless partway through is hit by the rest (cbtacts.pas, RollAttacks).
            const bool aside = natural > 1 && turned_aside(defender);
            if ((aside || !attack_hits(natural, attacker.numbers.thac0, armor_class, modifier)) &&
                !defender.helpless())
            {
                log({"{attacker} misses {name}.",
                     {{"attacker", attacker.name}, {"name", defender.name}}});
                continue;
            }
            // A Secret creature that swallows whole does so on a natural 20,
            // before the blow lands (EffSwallowWhole, at its damage).
            if (attacker.has(effect::swallows_whole) && natural == 20)
            {
                swallow_whole(attacker, defender);
                continue;
            }
            int dice = 0;
            for (int die = 0; die < attack.dice; ++die)
            {
                dice += roll(std::max(attack.sides, 1));
            }
            auto damage = hit_damage(dice, attack.modifier);
            // Secret multiplies a backstab after the effects on the blow, so the
            // multiple covers a ranger's bonus too (CalcWeaponDamage), with
            // (level - 1) div 4 + 2.
            if (backstab && !secret())
            {
                damage *= attacker.numbers.thief_level / 4 + 2;
            }
            damage = weapon_damage_defenses(attacker, defender, damage, phase, primary_hits);
            if (backstab && secret())
            {
                damage *= (attacker.numbers.thief_level - 1) / 4 + 2;
            }
            log({"{attacker} hits {name} for {damage}.",
                 {{"attacker", attacker.name},
                  {"name", defender.name},
                  {"damage", std::to_string(damage)}}});
            primary_hits += phase == 0 ? 1 : 0;
            inflict(defender, damage);
            // In Secret only a blow that did damage carries anything (RollAttacks).
            if (defender.able() && (damage > 0 || !secret()))
            {
                on_hit(attacker, defender, phase, primary_hits, natural);
            }
        }
    }
    attacker.sweep_budget = 0;
    if (attacker.attacks() == 0)
    {
        end_actions(attacker);
    }
}

// What an attacker's and a defender's effects do to its attack roll
// (fxengine.pas, AttackRoll): Bless +1, Curse -1, an invisible defender 4
// harder, and a protected one 2 harder for an attacker of the alignment it is
// protected from. The original lost that 2 to a roll made after it (an
// obvious bug, fixed).
int attack_modifier(const Fighter &attacker, const Fighter &defender, GoldBoxGame game)
{
    int modifier = (attacker.has(effect::bless) ? 1 : 0) - (attacker.has(effect::curse) ? 1 : 0);
    // Blindness and Bestow Curse take 4 from the attacker's rolls (effhndlr.pas, 33, 36).
    modifier -= attacker.has(effect::blind) ? 4 : 0;
    modifier -= attacker.has(effect::bestow_curse) ? 4 : 0;
    if (defender.has(effect::invisible) || defender.has(effect::dust))
    {
        modifier -= 4;
    }
    // A dwarf or gnome strikes its foes 1 better, and giants, bugbears and
    // gnolls strike it 4 worse (effhndlr.pas, 18, 26, 47, 48). The original
    // judged the giant by the defender's own target instead of the attacker,
    // reading eight names from its list of five (obvious bugs, fixed).
    const int foe = defender.numbers.kin, kind = attacker.numbers.kin;
    modifier += attacker.has(26) && (foe & kin::dwarf_foe) ? 1 : 0;
    modifier += attacker.has(18) && (foe & kin::gnome_foe) ? 1 : 0;
    modifier -= defender.has(47) && (kind & kin::giant) ? 4 : 0;
    modifier -= defender.has(48) && (kind & kin::bugbear) ? 4 : 0;
    const int alignment = attacker.numbers.alignment;
    const bool evil = alignment == 2 || alignment == 5 || alignment == 8;
    const bool good = alignment == 0 || alignment == 3 || alignment == 6;
    if ((evil && defender.has(effect::protection_from_evil)) ||
        (good && defender.has(effect::protection_from_good)))
    {
        modifier -= 2;
    }
    // Dispel Evil keeps the evil 7 worse off at its bearer (Curse, DispelEvil).
    modifier -= dispel_evil_takes(attacker, game) && defender.has(204) ? 7 : 0;
    return modifier;
}

// The attack the attacker can make on the target, if any (cbtacts.pas,
// AimTargetMenu): melee next to it, without a missile weapon or with one it
// can throw; otherwise a loaded missile weapon shoots a target it sees within
// range, unless an enemy stands next to it and the weapon is not throwable.
std::optional<std::string> Session::attack_verb(const Fighter &attacker,
                                                const Fighter &target) const
{
    if (!can_target(attacker, target))
    {
        return std::nullopt;
    }
    const auto &missile = attacker.numbers.missile;
    const bool next_to = adjacent(attacker, target);
    if (!missile)
    {
        return next_to ? std::optional<std::string>("melee") : std::nullopt;
    }
    if (!missile->loaded || (!missile->throwable && enemy_next_to(attacker)))
    {
        return std::nullopt;
    }
    if (next_to)
    {
        return "melee";
    }
    if (distance(attacker, target) > missile->range || !sees(board_, attacker, target))
    {
        return std::nullopt;
    }
    return missile->thrown ? "throw" : "ranged";
}

bool Session::enemy_next_to(const Fighter &f) const
{
    return std::any_of(fighters_.begin(), fighters_.end(),
                       [&f](const Fighter &other)
                       {
                           return other.side != f.side && other.able() && adjacent(f, other);
                       });
}

// A missile attack, which spends a shot for each attack made: a thrown
// weapon's last throw leaves its thrower empty-handed, and a launcher's last
// arrow or quarrel leaves it nothing to shoot (cbtacts.pas, MakeAttack).
void Session::shoot(Fighter &attacker, Fighter &target)
{
    const int before = attacker.attacks_left[0];
    make_attack(attacker, target, false);
    auto &missile = *attacker.numbers.missile;
    if (missile.needs_no_ammunition)
    {
        return;
    }
    if (missile.quantity > 0)
    {
        missile.quantity = std::max(missile.quantity - (before - attacker.attacks_left[0]), 0);
    }
    if (missile.quantity > 0)
    {
        return;
    }
    if (!missile.thrown)
    {
        missile.loaded = false;
        return;
    }
    auto &numbers = attacker.numbers;
    numbers.thac0 = missile.empty_handed_thac0;
    numbers.attacks[0] = missile.empty_handed_attack;
    numbers.backstabs = missile.empty_handed_backstabs;
    numbers.large_attack.reset();
    numbers.missile.reset();
    numbers.striker.reset();
    numbers.weapon_type = -1;
}

// A fighter sweeps through level-0 enemies: when the defender is one, next to
// it, and more of them stand around it than it has attacks this round, it
// strikes each once, the defender first, up to its attack level (cbtacts.pas,
// SweepAttack).
bool Session::sweep(Fighter &attacker, Fighter &defender)
{
    if (attacker.attacks_left[0] >= attacker.sweep_budget || defender.numbers.level != 0 ||
        !adjacent(attacker, defender))
    {
        return false;
    }
    std::vector<Fighter *> weak{&defender};
    for (auto &f : fighters_)
    {
        if (&f != &defender && f.side != attacker.side && f.able() && f.numbers.level == 0 &&
            adjacent(attacker, f))
        {
            weak.push_back(&f);
        }
    }
    if (static_cast<int>(weak.size()) <= attacker.attacks_left[0])
    {
        return false;
    }
    weak.resize(
        std::min<std::size_t>(weak.size(), static_cast<std::size_t>(attacker.sweep_budget)));
    log({"{attacker} sweeps.", {{"attacker", attacker.name}}});
    for (auto *target : weak)
    {
        note_attack(attacker, *target);
        attacker.attacks_left = {1, 0};
        attacker.attack_phase = 0;
        make_attack(attacker, *target, false);
    }
    end_actions(attacker);
    return true;
}

// Each enemy next to the mover now but not after its step strikes once, as
// from behind, while the mover stays up: unless it no longer acts this round,
// has already been attacked and has the mover out of the five directions in
// front of it (cbtacts.pas, AttacksOfOpportunity).
void Session::free_attacks(Fighter &mover, Cell next)
{
    auto stepped = mover;
    stepped.cell = next;
    for (auto &enemy : fighters_)
    {
        if (!mover.able() || enemy.side == mover.side || !enemy.able() || enemy.helpless() ||
            enemy.has(effect::bite_hold) || enemy.has(effect::blood_drain) ||
            !can_target(enemy, mover) || !adjacent(enemy, mover) || adjacent(enemy, stepped))
        {
            continue;
        }
        bool facing = false;
        for (int scan = enemy.direction + 6; scan <= enemy.direction + 10; ++scan)
        {
            facing = facing || leads_toward(enemy.cell, mover.cell, scan % 8);
        }
        if (enemy.initiative <= 0 && enemy.flank_count > 0 && !facing)
        {
            continue;
        }
        int phase = enemy.numbers.attacks[0].rate_per_two_rounds == 0 ? 1 : 0;
        for (int form = 0; form < 2; ++form)
        {
            if (enemy.attacks_left[static_cast<std::size_t>(form)] > 0)
            {
                phase = form;
            }
        }
        auto &left = enemy.attacks_left[static_cast<std::size_t>(phase)];
        left = std::max(left, 1);
        enemy.attack_phase = phase;
        log({"{attacker} strikes {name} leaving.",
             {{"attacker", enemy.name}, {"name", mover.name}}});
        make_attack(enemy, mover, true);
    }
}

// A guarding enemy the mover comes next to strikes it once and stops guarding
// (cbtacts.pas, GuardersAttack).
void Session::guards_strike(Fighter &mover)
{
    for (auto &guard : fighters_)
    {
        if (!mover.able() || !guard.guarding || guard.side == mover.side || !guard.able() ||
            guard.helpless() || !adjacent(guard, mover))
        {
            continue;
        }
        guard.guarding = false;
        if (guard.attacks() == 0)
        {
            continue;
        }
        note_attack(guard, mover);
        log({"{attacker} strikes from guard.", {{"attacker", guard.name}}});
        make_attack(guard, mover, false);
    }
}

// One step: the enemies left behind strike first, then the mover pays 2
// half-moves, or 3 on a diagonal, times the ground's cost, its flanking counts
// clear, and guards it comes next to strike (cbtacts.pas, CombatMoveCreature).
void Session::step_to(Fighter &mover, Cell next)
{
    free_attacks(mover, next);
    if (!mover.able())
    {
        mover.half_moves = 0;
        return;
    }
    const auto cost = grid_for(mover).step_cost(mover.cell, next);
    if (!cost || *cost > mover.half_moves)
    {
        mover.half_moves = 0;
        return;
    }
    mover.half_moves -= *cost;
    mover.direction = direction_toward(mover.cell, next);
    mover.facing_left = next.x < mover.cell.x || (next.x == mover.cell.x && mover.facing_left);
    mover.cell = next;
    mover.flank_count = mover.flank_spread = 0;
    breathe_cloud(mover);
    guards_strike(mover);
    if (!mover.able())
    {
        mover.half_moves = 0;
    }
}

// Bandaging saves a dying party member next to the one bandaging (the user,
// 2026-11-12; the original reached any on the field).
bool Session::can_bandage(const Fighter &bandager, const Fighter &dying) const
{
    return bandager.side == Side::party && dying.side == Side::party &&
           dying.condition == Condition::dying && adjacent(bandager, dying);
}

// Exactly 0 Hit Points leaves a creature unconscious, below 0 dying, and
// more than 9 below 0 dead (charhlp.pas, InflictDamage).
void Session::inflict(Fighter &target, int damage)
{
    // Any damage stops casting this round and loses a spell being cast
    // (cbtacts.pas, ProcessAttackResult).
    if (damage > 0)
    {
        target.cast_allowed = false;
        if (target.pending_spell || target.aiming)
        {
            lose_spell(target);
        }
    }
    if (target.hit_points >= damage)
    {
        target.hit_points -= damage;
        if (target.hit_points > 0)
        {
            return;
        }
        // The animated dead die at exactly 0 (charhlp.pas, InflictDamage).
        target.condition = target.has(effect::animated) ? Condition::dead : Condition::unconscious;
        log({target.condition == Condition::dead ? "{name} is killed."
                                                 : "{name} falls unconscious.",
             {{"name", target.name}}});
    }
    else
    {
        target.overkill = damage - target.hit_points;
        target.hit_points = 0;
        target.condition = target.overkill > 9 ? Condition::dead : Condition::dying;
        log({target.condition == Condition::dead ? "{name} is killed." : "{name} is dying.",
             {{"name", target.name}}});
    }
    target.initiative = 0;
    end_effects_at_zero(target);
    went_down(target);
}

// It loses the effects that end at 0 Hit Points; a hold or a drain it had
// lets its victim go (fxengine.pas, RemoveEffectsHp0).
void Session::end_effects_at_zero(Fighter &f)
{
    for (const auto code : effects_ending_at_zero)
    {
        if (code == effect::bite_hold || code == effect::blood_drain)
        {
            release(f, code);
            release(f, effect::engulfing);
            release(f, effect::hugging);
        }
        else if ((code == effect::charm || code == effect::animated) && f.has(code))
        {
            const auto *node = f.find_effect(code);
            remove_with_handler(f, static_cast<std::size_t>(node - f.numbers.effects.data()));
        }
        else
        {
            f.remove(code);
        }
    }
}

// A hold or a drain ends: its victim, whose combat number its strength keeps,
// may move again.
void Session::release(Fighter &holder, int code)
{
    const auto *hold = holder.find_effect(code);
    if (!hold)
    {
        return;
    }
    if (auto *victim = find(static_cast<EntityId>(hold->strength)))
    {
        victim->remove(effect::immobile);
        victim->remove(effect::suffocating);
    }
    holder.remove(code);
}

// What a hit that leaves its target standing carries (cbtacts.pas, RollAttacks;
// effhndlr.pas). The original stacked a new hold at each hit of a holding
// bite; one hold at a time here (an obvious bug, fixed).
void Session::on_hit(Fighter &attacker, Fighter &defender, int phase, int primary_hits, int natural)
{
    const auto effects = phase == 0 ? std::span<const int>(primary_hit_effects)
                                    : std::span<const int>(secondary_hit_effects);
    for (const auto code : effects)
    {
        if (code == 0 || !attacker.has(code) || !defender.able())
        {
            continue;
        }
        switch (code)
        {
        case 64:
            poison(defender, 0);
            break;
        case effect::poison_touch:
            // Secret's poison rides the second attack form. A creature with
            // none (a centipede, a giant snake or spider, a medusa) would never
            // poison, so its first form carries it (an obvious bug, fixed).
            if (phase == 0 && attacker.numbers.attacks[1].rate_per_two_rounds > 0)
            {
                break;
            }
            secret_poison(defender, 0);
            break;
        case effect::strong_poison_touch:
            secret_poison(defender, -2);
            break;
        case effect::stone_touch:
            // A cockatrice's touch, saved against as petrification (EffFleshToStone).
            if (!saving_throw(defender, 1, 0, &attacker))
            {
                turn_to_stone(defender);
            }
            break;
        case effect::ghoul_touch:
            // Held 10 rounds unless saved against (EffGhoulPara).
            paralyze(defender, 10, 0);
            break;
        case 65:
            poison(defender, 4);
            break;
        case 66:
            poison(defender, 2);
            break;
        case 70:
            poison(defender, -2);
            break;
        case 67:
            paralyze(defender, roll(8) + roll(8), 0);
            break;
        case 68:
            // Race 2 is the elf (half-elves are not spared).
            if (defender.numbers.race != 2)
            {
                paralyze(defender, 63, 0);
            }
            break;
        case 69:
            // The original passed -2 and its paralysis ignored it (an obvious bug, fixed).
            paralyze(defender, roll(9) + 10, -2);
            break;
        case 79:
            // Fire on a hit, 2d10 and magical (effhndlr.pas, EffFireTouch).
            nonweapon_damage(defender, roll(10) + roll(10), 0, false, 9, 0, 0, false);
            break;
        case 80:
            // Acid on a hit, 1d4 (EffAcidTouch).
            nonweapon_damage(defender, roll(4), 0, false, 16, 0, 0, false);
            break;
        case 85:
            drain_levels(defender, 1);
            break;
        case 86:
            drain_levels(defender, 2);
            break;
        case 87:
            // Mummy rot: no healing for 10 days, and death in 1d6 months
            // unless cured (effhndlr.pas, EffRotTouch).
            if (!defender.has(effect::rot))
            {
                defender.numbers.effects.push_back({effect::rot, 14400, 0, true});
                defender.numbers.effects.push_back(
                    {effect::mummy_rot, 43200, roll(6) * 16 + 15, true});
                log({"{name} is diseased.", {{"name", defender.name}}});
            }
            break;
        case 76:
            // A stirge drinks 12 to 15 Hit Points' worth, its strength the
            // victim's, which cannot move meanwhile (effhndlr.pas, EffBloodAttach).
            attacker.numbers.effects.push_back(
                {effect::blood_drain, 12 * 16 + 15, static_cast<int>(defender.id), true});
            defender.numbers.effects.push_back({effect::immobile, 0, 0, false});
            defender.half_moves = 0;
            break;
        case 73:
            if (primary_hits == 2)
            {
                rake(attacker, defender);
            }
            break;
        case 345:
        {
            // Dispel Evil: an evil creature it strikes that fails its save is
            // sent away, and the spell ends (Curse, sp_dispel_evil).
            if (!dispel_evil_takes(defender, game_of(identity_)))
            {
                break;
            }
            if (saving_throw(defender, 4, 0, &attacker))
            {
                log({"{name} resists dispel evil.", {{"name", defender.name}}});
                break;
            }
            log({"{name} is dispelled.", {{"name", defender.name}}});
            defender.condition = Condition::gone;
            defender.hit_points = 0;
            defender.initiative = 0;
            end_effects_at_zero(defender);
            went_down(defender);
            attacker.remove(204);
            attacker.remove(345);
            break;
        }
        case 322:
            // A dracolich's touch paralyzes for the fight (Curse, ovr013).
            paralyze(defender, 0, 0);
            break;
        case 323:
            // A cold touch, 2d8 of magical cold (Curse, ovr013).
            nonweapon_damage(defender, roll(8) + roll(8), 0, false, 10, 0, 0, false);
            break;
        case 296:
            // An owlbear hugs on an attack roll of 18 or more and holds its
            // victim (Curse, AffectOwlbearHugAttackCheck).
            if (natural >= 18 && !attacker.has(effect::hugging))
            {
                attacker.numbers.effects.push_back(
                    {effect::hugging, 0, static_cast<int>(defender.id), true});
                defender.numbers.effects.push_back({effect::immobile, 0, 0, false});
                defender.half_moves = 0;
                log({"{attacker} hugs {name}.",
                     {{"attacker", attacker.name}, {"name", defender.name}}});
            }
            break;
        case 257:
            // A shambling mound whose two blows both hit engulfs its victim,
            // which suffocates in 2d4 of its turns (Curse, engulfs).
            if (primary_hits == 2 && !defender.has(effect::immobile) &&
                !defender.has(effect::suffocating) && !attacker.has(effect::engulfing))
            {
                attacker.numbers.effects.push_back(
                    {effect::engulfing, 0, static_cast<int>(defender.id), true});
                defender.numbers.effects.push_back({effect::immobile, 0, 0, false});
                defender.numbers.effects.push_back(
                    {effect::suffocating, 0, roll(4) + roll(4), false});
                defender.half_moves = 0;
                log({"{attacker} engulfs {name}.",
                     {{"attacker", attacker.name}, {"name", defender.name}}});
            }
            break;
        case 77:
            if (defender.numbers.strength < 19 && !attacker.has(effect::bite_hold))
            {
                attacker.numbers.effects.push_back(
                    {effect::bite_hold, 0, static_cast<int>(defender.id), true});
                defender.numbers.effects.push_back({effect::immobile, 0, 0, false});
                defender.half_moves = 0;
                log({"{name} is held fast.", {{"name", defender.name}}});
            }
            break;
        default:
            break;
        }
    }
}

// A poisoned touch kills unless saved against (save 0, paralysis and poison).
void Session::poison(Fighter &victim, int modifier)
{
    if (saving_throw(victim, 0, modifier))
    {
        return;
    }
    log({"{name} is poisoned.", {{"name", victim.name}}});
    victim.numbers.effects.push_back({effect::poisoned, 0, 0, false});
    kill(victim);
}

void Session::paralyze(Fighter &victim, int minutes, int modifier)
{
    if (saving_throw(victim, 0, modifier))
    {
        return;
    }
    log({"{name} is paralyzed.", {{"name", victim.name}}});
    victim.numbers.effects.push_back({effect::paralysis, minutes, 12, false});
}

// Secret's poisoned blow (effhndlr.pas, PoisonTarget, in its reconstruction):
// a victim poisonous itself saves better by its own poison's strength
// against plain poison, and with no penalty against strong poison.
void Session::secret_poison(Fighter &victim, int modifier)
{
    if (const auto *own = victim.find_effect(effect::poison_touch))
    {
        modifier = modifier < 0 ? 0 : modifier + own->strength;
    }
    poison(victim, modifier);
}

// Turned to stone: out of the fight for good.
void Session::turn_to_stone(Fighter &victim)
{
    victim.condition = Condition::stoned;
    victim.hit_points = 0;
    victim.initiative = 0;
    log({"{name} is stoned.", {{"name", victim.name}}});
    end_effects_at_zero(victim);
    went_down(victim);
}

// A Secret remorhaz swallows its victim whole: dead at once (effhndlr.pas,
// EffSwallowWhole, in its reconstruction).
void Session::swallow_whole(Fighter &swallower, Fighter &victim)
{
    log({"{name} is swallowed whole.", {{"name", victim.name}}});
    victim.condition = Condition::dead;
    victim.hit_points = 0;
    victim.initiative = 0;
    end_effects_at_zero(victim);
    went_down(victim);
    log({roll(100) <= 50 ? "{name} belches in satisfaction." : "{victim} is digested instantly.",
         {{"name", swallower.name}, {"victim", victim.name}}});
}

// A level drain, no save (effhndlr.pas, EffDrainOne and EffDrainTwo). Each
// level takes the Hit Points of a level (its maximum over its levels) from
// its maximum and its current; one below level 2 is dead forever. Otherwise
// the class whose level took the most experience (the later on a tie) loses
// a level, its THAC0 and saving throws following the class tables, and one
// left with no Hit Points is killed. The original divided by zero for a
// creature without class levels, which loses its highest level's share here;
// and it kept the drained class's level as the highest, where the highest of
// its classes is kept here (both obvious bugs, fixed).
void Session::drain_levels(Fighter &victim, int levels)
{
    log({levels == 1 ? "{name} is drained a level." : "{name} is drained 2 levels.",
         {{"name", victim.name}}});
    auto &numbers = victim.numbers;
    for (int n = 0; n < levels && victim.condition != Condition::gone; ++n)
    {
        auto &classes = numbers.class_levels;
        const int total = std::accumulate(classes.begin(), classes.end(), 0);
        const int per_level = numbers.max_hit_points / std::max({total, numbers.level, 1});
        numbers.max_hit_points = std::max(numbers.max_hit_points - per_level, 1);
        victim.hit_points = std::max(victim.hit_points - per_level, 0);
        if (numbers.level < 2 || victim.has(effect::animated))
        {
            victim.condition = Condition::gone;
            victim.hit_points = 0;
            victim.initiative = 0;
            log({"{name} is dead forever.", {{"name", victim.name}}});
            end_effects_at_zero(victim);
            return;
        }
        std::size_t drained = classes.size();
        int most = -1;
        for (std::size_t slot = 0; slot < classes.size(); ++slot)
        {
            if (classes[slot] <= 0)
            {
                continue;
            }
            const int experience = experience_for_level(static_cast<ClassSlot>(slot), classes[slot],
                                                        game_of(identity_))
                                       .value_or(0);
            if (experience >= most)
            {
                most = experience;
                drained = slot;
            }
        }
        if (drained < classes.size())
        {
            const auto before = classes;
            --classes[drained];
            const auto game = game_of(identity_);
            numbers.thac0 += thac0(classes, game) - thac0(before, game);
            const auto saves_before = saving_throws(before, game);
            const auto saves_after = saving_throws(classes, game);
            for (std::size_t save = 0; save < numbers.saving_throws.size(); ++save)
            {
                numbers.saving_throws[save] += saves_after[save] - saves_before[save];
            }
            numbers.level = *std::max_element(classes.begin(), classes.end());
        }
        else
        {
            --numbers.level;
        }
        if (victim.hit_points == 0 && victim.condition != Condition::dead)
        {
            kill(victim);
            return;
        }
    }
}

// Casting (combat.pas, CastSpellCombat): a spell with a casting time takes it
// off the caster's initiative and goes off at its next turn this round; one
// without goes off now.
void Session::begin_cast(Fighter &caster, int spell_number)
{
    const auto &known = *game_spell(spell_number);
    const int delay = known.casting_time / 3;
    if (delay == 0)
    {
        start_aiming(caster, spell_number);
        return;
    }
    log({"{name} begins casting {spell}.",
         {{"name", caster.name}, {"spell", std::string(known.name), true}}});
    caster.pending_spell = spell_number;
    caster.initiative = std::max(caster.initiative - delay, 1);
    if (secret())
    {
        reorder_after_cast(caster);
    }
    actor_ = 0;
}

// Choosing targets as the spell goes off (cbtacts.pas, SelectCombatTargets):
// a spell on the caster goes off at once; others wait for their aim.
void Session::start_aiming(Fighter &caster, int spell_number)
{
    const auto &known = *game_spell(spell_number);
    const int area = known.area & 0x0F;
    caster.aiming = spell_number;
    caster.picked.clear();
    caster.blast_delay = -1;
    // Fire Shield waits for its flames to be chosen (SpellFireShield).
    if (spell_number == fire_shield)
    {
        caster.picks_left = 0;
        return;
    }
    if (area == 0)
    {
        resolve_spell(caster, {caster.id}, caster.cell);
        return;
    }
    caster.picks_left = area <= 7 ? (area & 3) + 1 : 1;
}

// Whether the caster can single out a creature for its spell: within range,
// in sight, and no veto (cbtacts.pas, AimTargetMenu). Healing reaches the
// downed but not the dead, Secret's Stone to Flesh a statue.
bool Session::reachable_by_spell(const Fighter &caster, const Fighter &target) const
{
    constexpr int heal = 236, druid_cure = 298, stone_to_flesh = 313;
    const auto &known = *game_spell(caster.aiming);
    const bool heals = caster.aiming == 3 || caster.aiming == heal || caster.aiming == druid_cure;
    const bool slows_poison =
        caster.aiming == 26 && target.has(effect::poisoned) && target.condition == Condition::dead;
    const bool unstones = caster.aiming == stone_to_flesh && target.condition == Condition::stoned;
    if (!(target.able() || (heals && target.downed()) || slows_poison || unstones) ||
        std::find(caster.picked.begin(), caster.picked.end(), target.id) != caster.picked.end())
    {
        return false;
    }
    return distance(caster, target) <=
               spell_range(known, caster_level(caster, known, game_of(identity_))) &&
           sees(board_, caster, target) && can_target(caster, target);
}

std::vector<rules::Command> Session::aiming_commands(const Fighter &caster) const
{
    std::vector<rules::Command> commands;
    const auto &known = *game_spell(caster.aiming);
    const int area = known.area & 0x0F;
    const auto command = [&](std::string verb)
    {
        rules::Command c;
        c.revision = revision_;
        c.actor = caster.id;
        c.verb = std::move(verb);
        c.label = std::string(known.name);
        c.item = static_cast<unsigned>(caster.aiming);
        return c;
    };
    if (caster.aiming == delayed_blast_spell && caster.blast_delay < 0)
    {
        // How many rounds it waits (the original's 1 to 50 segments, in tens).
        constexpr std::array<const char *, 6> delays{"No delay",       "Delay 1 round",
                                                     "Delay 2 rounds", "Delay 3 rounds",
                                                     "Delay 4 rounds", "Delay 5 rounds"};
        for (std::size_t rounds = 0; rounds < delays.size(); ++rounds)
        {
            auto choice = command("blast_delay");
            choice.label = delays[rounds];
            choice.item = static_cast<unsigned>(rounds);
            commands.push_back(choice);
        }
    }
    else if (caster.aiming == fire_shield)
    {
        // The flames, hot or cold (SpellFireShield's menu).
        for (const auto flames : {Flames::hot, Flames::cold})
        {
            auto choice = command("flames");
            choice.label = flames == Flames::hot ? "Hot flames" : "Cold flames";
            choice.item = static_cast<unsigned>(flames);
            commands.push_back(choice);
        }
    }
    else if (area >= 8 && area <= 14)
    {
        // A square in range and in sight, open ground (cbtacts.pas, ManualAimCursor).
        const int range = spell_range(known, caster_level(caster, known, game_of(identity_)));
        for (int y = 0; y < board_.height; ++y)
        {
            for (int x = 0; x < board_.width; ++x)
            {
                const Cell cell{x, y};
                if (board_.at(cell) == rules::Terrain::obstacle ||
                    doubled_distance(caster.cell, cell) / 2 > range ||
                    !rules::has_line_of_sight(board_, caster.cell, cell))
                {
                    continue;
                }
                auto aim = command("aim");
                aim.destination = cell;
                commands.push_back(aim);
            }
        }
    }
    else
    {
        for (const auto &f : fighters_)
        {
            if (reachable_by_spell(caster, f))
            {
                auto aim = command("aim");
                aim.target = f.id;
                aim.destination = f.cell;
                aim.intent = known.offensive ? rules::Intent::hostile : rules::Intent::friendly;
                commands.push_back(aim);
            }
        }
        if (!caster.picked.empty())
        {
            commands.push_back(command("aim_done"));
        }
    }
    commands.push_back(command("abort"));
    // The AI aims a spell that comes due on its turn (combat.pas, ManualControl).
    commands.push_back(command("quick"));
    return commands;
}

// Everyone standing within a radius of a square that a line from it reaches,
// allies and the caster too; the nearest first (cbtacts.pas, SelectCombatTargets).
std::vector<EntityId> Session::within(Cell aim, int radius) const
{
    std::vector<std::pair<int, EntityId>> found;
    for (const auto &f : fighters_)
    {
        if (!f.able())
        {
            continue;
        }
        int nearest = 1000;
        for (const auto square : squares_of(f))
        {
            if (rules::has_line_of_sight(board_, aim, square))
            {
                nearest = std::min(nearest, doubled_distance(aim, square));
            }
        }
        if (nearest <= 2 * radius + 1)
        {
            found.emplace_back(nearest, f.id);
        }
    }
    std::stable_sort(found.begin(), found.end(),
                     [](const auto &a, const auto &b)
                     {
                         return a.first < b.first;
                     });
    std::vector<EntityId> ids;
    for (const auto &[gap, id] : found)
    {
        ids.push_back(id);
    }
    return ids;
}

// A spell lost: its memorized copy is gone (cbtacts.pas, ProcessAttackResult).
void Session::lose_spell(Fighter &caster)
{
    const int lost = caster.aiming ? caster.aiming : caster.pending_spell;
    auto &spells = caster.numbers.spells;
    if (const auto copy = std::find(spells.begin(), spells.end(), lost); copy != spells.end())
    {
        spells.erase(copy);
    }
    if (caster.pending_spell || caster.aiming)
    {
        log({"{name} loses a spell.", {{"name", caster.name}}});
    }
    caster.pending_spell = caster.aiming = caster.picks_left = 0;
    caster.picked.clear();
}

// The spell goes off at its targets (magic.pas, CastSpell): the caster turns
// to its aim and shows itself, the memorized copy is spent, and its actions end.
bool magic_resists(const Fighter &target, int caster_level, int roll_100);

void Session::resolve_spell(Fighter &caster, std::vector<EntityId> targets, Cell aim, Flames flames)
{
    const int number = caster.aiming;
    const auto &known = *game_spell(number);
    const int level = caster_level(caster, known, game_of(identity_));
    caster.aiming = caster.picks_left = 0;
    caster.picked.clear();
    if (aim != caster.cell)
    {
        caster.direction = direction_toward(caster.cell, aim);
        caster.facing_left = aim.x < caster.cell.x;
    }
    while (caster.has(effect::invisible))
    {
        caster.remove(effect::invisible);
    }
    const int item = caster.using_item;
    if (item >= 0)
    {
        spend_item(caster, item, number);
    }
    else
    {
        auto &spells = caster.numbers.spells;
        if (const auto copy = std::find(spells.begin(), spells.end(), number); copy != spells.end())
        {
            spells.erase(copy);
        }
    }
    log({"{name} casts {spell}.",
         {{"name", caster.name}, {"spell", std::string(known.name), true}}});
    const auto roll_dice = [this](int count, int sides)
    {
        int total = 0;
        for (int n = 0; n < count; ++n)
        {
            total += roll(sides);
        }
        return total;
    };
    std::erase_if(targets,
                  [&](EntityId id)
                  {
                      const auto &target = *find(id);
                      if (!refuses_spell(target, number))
                      {
                          return false;
                      }
                      log({"{name} is unaffected.", {{"name", target.name}}});
                      return true;
                  });
    std::vector<EntityId> kept;
    switch (number)
    {
    case 1:
        // Bless: the caster's side, but not one with an enemy next to it.
        for (const auto id : targets)
        {
            const auto &f = *find(id);
            if (f.side == caster.side && !enemy_next_to(f))
            {
                kept.push_back(id);
            }
        }
        cast_generic(caster, number, level, kept, 0, 0);
        break;
    case 2:
        for (const auto id : targets)
        {
            if (find(id)->side != caster.side)
            {
                kept.push_back(id);
            }
        }
        cast_generic(caster, number, level, kept, 0, 0);
        break;
    case 3:
    case 298:
        // Cure Light Wounds (the cleric's, and Secret's druid's): 1d8, not
        // through rot; the dying stop bleeding but stay down (magic.pas, the cure).
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.has(effect::rot) || !(f.able() || f.downed()))
            {
                continue;
            }
            f.hit_points = std::min(f.hit_points + roll(8), f.numbers.max_hit_points);
            if (f.condition == Condition::dying)
            {
                f.condition = Condition::unconscious;
                f.overkill = 0;
            }
            log({"{name} is healed.", {{"name", f.name}}});
        }
        break;
    case 4:
        cast_generic(caster, number, level, targets, roll(8), 8);
        break;
    case 9:
        // Burning Hands: the caster's level in fire.
        cast_generic(caster, number, level, targets, level, 9);
        break;
    case 10:
        // Charm Person cannot take a creature of type above 1 or a large one
        // (magic.pas, the charm); Secret judges by race and flags instead
        // (refuses_spell).
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (!secret() && (f.numbers.creature_type > 1 || f.numbers.large))
            {
                log({"{name} is unaffected.", {{"name", f.name}}});
                continue;
            }
            kept.push_back(id);
        }
        cast_generic(caster, number, level, kept, 0, 0);
        break;
    case 15:
    {
        // Magic Missile: a missile of 1d4+1 for every two levels, the first at
        // level 1. The original's level div 2 gave none at level 1, against
        // its manual (an obvious bug, fixed).
        const int missiles = (level + 1) / 2;
        cast_generic(caster, number, level, targets, roll_dice(missiles, 4) + missiles, 8);
        break;
    }
    case 20:
        cast_generic(caster, number, level, targets, roll(8) + level, 12);
        break;
    case 21:
        cast_sleep(level, aim);
        break;
    case 23:
    case 49:
    {
        // Hold Person: the fewer held, the harder to save, each in turn from
        // the last picked; a creature of type above 1 or a large one is not
        // held (magic.pas, the hold). In Secret a monster is never held,
        // whatever its size (magic.pas, its multi-target resolver).
        const int count = static_cast<int>(targets.size());
        const int modifier = count == 1 ? (number == 23 ? -2 : -3) : count == 2 ? -1 : 0;
        for (auto id = targets.rbegin(); id != targets.rend(); ++id)
        {
            auto &f = *find(*id);
            const bool held_off =
                secret() ? f.numbers.race == 0 : f.numbers.creature_type > 1 || f.numbers.large;
            if (held_off)
            {
                log({"{name} is unaffected.", {{"name", f.name}}});
                continue;
            }
            cast_generic(caster, number, level, {*id}, 0, 0, modifier);
        }
        break;
    }
    case 32:
    {
        // Mirror Image: 1d4 images.
        const int minutes = known.minutes_fixed + known.minutes_per_level * level;
        add_with_message(caster, {effect::mirror_image, minutes, roll(4), false}, false, false, 0,
                         level, &known);
        break;
    }
    case 37:
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.has(effect::blind))
            {
                f.remove(effect::blind);
                log({"{name} can see.", {{"name", f.name}}});
            }
        }
        break;
    case 40:
        // Cause Disease lasts 1d6 x 10 minutes before the disease takes hold.
        cast_generic(caster, number, level, targets, 0, 0, 0, roll(6) * 10);
        break;
    case 42:
        // Prayer on the caster, or in Gateway on those it touches: its
        // strength keeps the caster's side in bit 4.
        for (const auto id : targets)
        {
            (void)add_with_message(*find(id),
                                   {effect::prayer, level,
                                    (caster.side == Side::opposition ? 0x10 : 0) + level, false},
                                   false, false, 0, level, &known);
        }
        break;
    case 43:
    case 189:
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.has(effect::bestow_curse))
            {
                f.remove(effect::bestow_curse);
                log({"{name} is un-cursed.", {{"name", f.name}}});
            }
        }
        break;
    case 34:
        lay_cloud(caster, level, aim, CloudKind::stinking);
        break;
    case 191:
        lay_cloud(caster, level, aim, CloudKind::deadly);
        break;
    case 26:
        for (const auto id : targets)
        {
            slow_poison(*find(id), level);
        }
        break;
    case 27:
    {
        // Snake Charm: every snake on the field, either side, whose Hit Points
        // the caster's own can pay for (magic.pas, the snake charm). Secret
        // knows a snake by its flags and passes over one charmed already
        // (SpellSnakeCharm).
        int pool = caster.hit_points;
        for (auto &f : fighters_)
        {
            const bool snake =
                secret() ? (f.numbers.traits & trait::snake) && !f.has(effect::snake_charm)
                         : f.numbers.creature_type == 14;
            if (!f.able() || !snake || f.hit_points > pool)
            {
                continue;
            }
            pool -= f.hit_points;
            add_with_message(f, {effect::snake_charm, 0, level, false}, false, false, 0, level,
                             &known);
        }
        break;
    }
    case 36:
    case 190:
        animate_dead(caster, level);
        break;
    case 57:
        // A Potion of Speed cures Slow, or hastes for 5d4 rounds.
        if (caster.has(effect::slowed))
        {
            caster.remove(effect::slowed);
            log({"{name} is cured.", {{"name", caster.name}}});
        }
        else
        {
            add_with_message(caster, {effect::hasted, roll_dice(5, 4), 12, false}, false, false, 0,
                             level, &known);
        }
        break;
    case 58:
        // Keoghtum's Ointment: poison out (the dead stay dead), else disease,
        // else 1d4+8 healed.
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.has(effect::poisoned))
            {
                f.remove(effect::poisoned);
                f.remove(effect::slow_poison);
            }
            else if (f.has(effect::rot) || f.has(34) || f.has(43))
            {
                for (const int code : {34, 43, 44, 31, effect::rot, effect::mummy_rot})
                {
                    f.remove(code);
                }
                log({"{name} is cured.", {{"name", f.name}}});
            }
            else if (f.able())
            {
                f.hit_points = std::min(f.hit_points + roll(4) + 8, f.numbers.max_hit_points);
                log({"{name} is healed.", {{"name", f.name}}});
            }
        }
        break;
    case 60:
        // A Javelin of Lightning: 1d6+20 at its mark, then a bolt of 3 squares
        // of 20 beyond it (magic.pas, SpellJavelinOfLightning).
        lightning_at(aim, roll(6) + 20, level);
        lightning_path(caster.cell, aim, 3, 20, level, false);
        break;
    case 61:
        cast_generic(caster, number, level, targets, 0, 0, 0, roll_dice(5, 4));
        break;
    case 62:
        caster.hit_points =
            std::min(caster.hit_points + roll_dice(2, 4) + 2, caster.numbers.max_hit_points);
        log({"{name} is healed.", {{"name", caster.name}}});
        break;
    case 63:
        // Dust of Disappearance for 2d10 x 10 rounds in a fight.
        cast_generic(caster, number, level, targets, 0, 0, 0, roll_dice(2, 10) * 10);
        break;
    case 64:
        // A Necklace of Missiles: a fireball of 3, 5 or 7 dice.
        cast_generic(caster, number, level, fireball_reach(targets, aim),
                     roll_dice(roll(3) * 2 + 1, 6), 9);
        break;
    case 65:
        // A Wand of Magic Missiles: 2d4+2, which Shield does not stop.
        cast_generic(caster, number, level, targets, roll_dice(2, 4) + 2, 8);
        break;
    case 41:
    case 46:
        // Dispel Magic on everyone within a square of the aim. The original
        // dispelled the first of them once for each (an obvious bug, fixed).
        for (const auto id : targets)
        {
            dispel(*find(id), level);
        }
        break;
    case 51:
    {
        // Lightning Bolt: the caster's level in d6, one roll, at the aim and
        // then along a bolt of 8 squares (magic.pas, SpellLightningBolt).
        const int damage = roll_dice(level, 6);
        lightning_at(aim, damage, level);
        lightning_path(caster.cell, aim, 8, damage, level, true);
        break;
    }
    case 47:
        // Fireball: the caster's level in d6, one roll for all, saved for half.
        cast_generic(caster, number, level, fireball_reach(targets, aim), roll_dice(level, 6), 9);
        break;
    case 178:
        // Entangle: out of doors, each that fails its save is held by the
        // plants (Curse's SpellEntangle); in a dungeon nothing grows.
        if (!outdoors_)
        {
            log({"Nothing happens.", {}});
            break;
        }
        for (const auto id : targets)
        {
            auto &target = *find(id);
            const bool saved = saving_throw(target, 4, 0, &caster);
            (void)add_with_message(target, {336, known.minutes_fixed, level, false}, true, saved, 0,
                                   level, &known);
        }
        break;
    case 48:
    case 55:
    {
        // Haste the caster's side, or Slow its enemies, up to its level of
        // them, the nearest first; one under the opposite spell is only freed
        // of it (magic.pas, Haste and Slow).
        const bool haste = number == 48;
        int left = level;
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if ((f.side == caster.side) != haste || left <= 0)
            {
                continue;
            }
            --left;
            const int opposite = haste ? effect::slowed : effect::hasted;
            if (f.has(opposite))
            {
                f.remove(opposite);
                log({"{name} is cured.", {{"name", f.name}}});
                continue;
            }
            cast_generic(caster, number, level, {id}, 0, 0);
        }
        break;
    }
    case 181:
        // Charm Monsters: Charm Person on any creature (Curse let it charm
        // only what Charm Person can, as it does Hold Monster).
        cast_generic(caster, number, level, targets, 0, 0);
        break;
    case 182:
    {
        // Confusion: at most 2d8 of those in its area.
        auto confused = targets;
        confused.resize(
            std::min<std::size_t>(confused.size(), static_cast<std::size_t>(roll(8) + roll(8))));
        cast_generic(caster, number, level, confused, 0, 0);
        break;
    }
    case 184:
        // Fear: those on a path 6 squares long and 3 wide that fail to save
        // flee (Curse, SpellFear).
        for (const auto id : path_area(caster, aim, 6, 3))
        {
            auto &f = *find(id);
            if (!saving_throw(f, 4, 0, &caster) &&
                add_with_message(f, {342, known.minutes_per_level * level, level, true}, false,
                                 false, 0, level, &known))
            {
                f.fleeing = true;
            }
        }
        break;
    case 186:
        // Fumble: a failed save leaves it helpless, a made one slowed
        // (Curse, SpellFumble).
        for (const auto id : targets)
        {
            auto &f = *find(id);
            const int minutes = known.minutes_per_level * level;
            const bool saved = saving_throw(f, 4, 0, &caster);
            (void)add_with_message(f, {saved ? effect::slowed : 31, minutes, level, false}, false,
                                   false, 0, level, &known);
        }
        break;
    case 193:
        // Feeblemind: a cleric saves 1 better, a magic-user 4 worse, any
        // other 2 worse; one feebleminded casts nothing (Curse, AffectFeebleMind).
        for (const auto id : targets)
        {
            auto &f = *find(id);
            const auto &l = f.numbers.class_levels;
            const int modifier = l[static_cast<std::size_t>(ClassSlot::cleric)] > 0       ? 1
                                 : l[static_cast<std::size_t>(ClassSlot::magic_user)] > 0 ? -4
                                                                                          : -2;
            cast_generic(caster, number, level, {id}, 0, 0, modifier);
            if (f.has(268))
            {
                f.numbers.spells.clear();
                f.cast_allowed = false;
            }
        }
        break;
    case 170:
        // Sticks to Snakes: those under 6 Hit Dice fight snakes for a while;
        // stronger ones smash them flat (Curse, SpellSticksToSnakes).
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.numbers.level >= 6)
            {
                log({"{name} smashes them flat.", {{"name", f.name}}});
                continue;
            }
            (void)add_with_message(f, {203, 0, level, false}, false, false, 0, level, &known);
        }
        break;
    case 173:
        // Dispel Evil: on the caster, whose blows send the evil away.
        (void)add_with_message(caster, {204, known.minutes_fixed, level, false}, false, false, 0,
                               level, &known);
        caster.numbers.effects.push_back({345, known.minutes_fixed, level, false});
        break;
    case 185:
    {
        // Fire Shield, hot or cold as its caster chose (the AI and Quick at
        // random, hot on a d10 above 5), with protection from electricity.
        const int minutes = known.minutes_fixed + known.minutes_per_level * level;
        const bool hot = flames == Flames::random ? roll(10) > 5 : flames == Flames::hot;
        (void)add_with_message(caster, {hot ? 250 : 254, minutes, level, false}, false, false, 0,
                               level, &known);
        caster.numbers.effects.push_back({335, minutes, level, false});
        break;
    }
    case 183:
    {
        // Dimension Door: to the square aimed at, free of any hold.
        if (standing_at(aim) || !board_.contains(aim) || board_.at(aim) == rules::Terrain::obstacle)
        {
            break;
        }
        for (auto &other : fighters_)
        {
            for (const int hold : {effect::hugging, effect::engulfing, effect::bite_hold})
            {
                if (const auto *held = other.find_effect(hold);
                    held && held->strength == static_cast<int>(caster.id))
                {
                    release(other, hold);
                }
            }
        }
        caster.remove(effect::immobile);
        caster.cell = aim;
        log({"{name} teleports.", {{"name", caster.name}}});
        break;
    }
    case 158:
    case 171:
    case 199:
    {
        // Curse's cures (ovr023): Cure Serious Wounds 2d8+1, Cure Critical
        // Wounds 3d8+3, a healing power 2d4+2.
        const int healed = number == 158   ? roll_dice(2, 8) + 1
                           : number == 171 ? roll_dice(3, 8) + 3
                                           : roll_dice(2, 4) + 2;
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.has(effect::rot) || !(f.able() || f.downed()))
            {
                continue;
            }
            f.hit_points = std::min(f.hit_points + healed, f.numbers.max_hit_points);
            if (f.condition == Condition::dying)
            {
                f.condition = Condition::unconscious;
                f.overkill = 0;
            }
            log({"{name} is healed.", {{"name", f.name}}});
        }
        break;
    }
    case 166:
        cast_generic(caster, number, level, targets, roll_dice(2, 8) + 1, 8);
        break;
    case 172:
        cast_generic(caster, number, level, targets, roll_dice(3, 8) + 3, 8);
        break;
    case 174:
        // Flame Strike: 6d8 of fire.
        cast_generic(caster, number, level, targets, roll_dice(6, 8), 9);
        break;
    case 187:
        // Ice Storm: 3d10. Curse marks it acid (docs/ORIGINAL-RULES.md, Bugs).
        cast_generic(caster, number, level, targets, roll_dice(3, 10), 2);
        break;
    case 192:
    {
        // Cone of Cold: the caster's level and as many d4, along a path two
        // wide to half its level. Curse marks it acid (docs/ORIGINAL-RULES.md, Bugs).
        cast_generic(caster, number, level, path_area(caster, aim, std::max((level + 1) / 2, 1), 2),
                     level + roll_dice(level, 4), 2);
        break;
    }
    case 198:
    {
        // A blast (Curse's Wand of Defoliation, a monster's): 6d6 along a path
        // to 3 squares; a plant takes it whole, with no save (ovr023, sub_61727).
        constexpr int plant = 18;
        const int damage = roll_dice(6, 6);
        std::vector<EntityId> others;
        for (const auto id : path_area(caster, aim, 3, 1))
        {
            auto &target = *find(id);
            if (target.numbers.creature_type == plant)
            {
                nonweapon_damage(target, damage, game_spell(number)->save_result, false, 8, level,
                                 number, false);
            }
            else
            {
                others.push_back(id);
            }
        }
        cast_generic(caster, number, level, others, damage, 8);
        break;
    }
    case 194:
    {
        // Hold Monster: Hold Person's saves, on any creature (Curse let it
        // hold only what Hold Person can, docs/ORIGINAL-RULES.md, Bugs).
        const int count = static_cast<int>(targets.size());
        const int modifier = count == 1 ? -3 : count == 2 ? -1 : 0;
        for (auto id = targets.rbegin(); id != targets.rend(); ++id)
        {
            cast_generic(caster, number, level, {*id}, 0, 0, modifier);
        }
        break;
    }
    case 176:
    {
        // Slay Living: unless magic resistance turns it, a failed save
        // kills; a saved one takes 2d8+1 (ovr023, SpellSlayLiving).
        for (const auto id : targets)
        {
            auto &target = *find(id);
            if (magic_resists(target, level, roll(100)))
            {
                log({"{name} is unaffected.", {{"name", target.name}}});
                continue;
            }
            if (!saving_throw(target, 4, 0, &caster))
            {
                log({"{name} is slain.", {{"name", target.name}}});
                kill(target);
                continue;
            }
            nonweapon_damage(target, roll_dice(2, 8) + 1, 0, false, 8, level, number, true);
        }
        break;
    }
    case 168:
        // Poison: the caster's touch carries poison (ovr023, SpellPoison).
        add_with_message(caster, {64, 0, level, false}, false, false, 0, level, &known);
        break;
    case 12:
    case 13:
    case 35:
    case 59:
        cast_strength_spell(caster, number, level, targets);
        break;
    case 236:
    case 256:
    case 290:
    case 310:
    case 311:
    case 313:
    case 314:
    case 315:
    case 316:
    case 317:
        cast_secret_spell(caster, number, level, targets, aim);
        break;
    case 263:
        make_younger(caster);
        break;
    default:
        cast_generic(caster, number, level, targets, 0, 0);
        break;
    }
    caster.using_item = -1;
    end_actions(caster);
}

// Haste ages its creature a year the first time it works on it, which a bit
// of its strength marks (effhndlr.pas, EffHasted): as the round's numbers are
// worked out (RecalcStats). A member's age is its marker's.
void Session::age_the_hasted()
{
    for (auto &f : fighters_)
    {
        auto *haste = f.find_effect(effect::hasted);
        if (!haste || (haste->strength & effect::hasted_aged) != 0)
        {
            continue;
        }
        haste->strength |= effect::hasted_aged;
        if (auto *age = f.find_effect(effect::age_in_years))
        {
            ++age->strength;
        }
        log({"{name} ages.", {{"name", f.name}}});
    }
}

namespace
{
// An item that holds spells to read: its type sits in slots 11 to 13
// (magic.pas, ItemReadable).
bool readable_item(const CarriedItem &c)
{
    return c.type.worn_location > 10 && c.type.worn_location < 14;
}

// What the inventory counts of an item: a scroll the bits of its spells left,
// a charged item its charges, a stack its size, anything else one.
int held_count(const CarriedItem &c)
{
    const auto &bytes = c.item.effect_codes;
    if (readable_item(c))
    {
        int bits = 0;
        for (std::size_t at = 0; at < bytes.size(); ++at)
        {
            bits |= bytes[at] != 0 ? 1 << at : 0;
        }
        return bits;
    }
    if (bytes[1] > 0 && bytes[2] < 0x80 && bytes[0] > 0)
    {
        return bytes[0];
    }
    return std::max<int>(c.item.stack_size, 1);
}

void set_readied(Held &held, bool readied)
{
    held.carried.item.readied_raw = readied ? 1 : 0;
    held.carried.item.raw[52] = held.carried.item.readied_raw;
}

// Readies an item last among the readied, or puts it away.
void place_readied(std::vector<Held> &held, std::size_t at, bool readied)
{
    int last = -1;
    for (const auto &other : held)
    {
        last = std::max(last, other.readied_order);
    }
    set_readied(held[at], readied);
    held[at].readied_order = readied ? last + 1 : -1;
}
} // namespace

// The items as the fight has left them: what each usable one has left, the
// shots its missile has left, and those used up (a spent charge's last, a
// thrown weapon gone from its hand).
std::vector<Held> Session::held_now(const Fighter &f) const
{
    auto held = f.held;
    if (!f.record)
    {
        return held;
    }
    std::vector<CarriedItem> worked;
    for (const int at : f.worked_from)
    {
        worked.push_back(held[static_cast<std::size_t>(at)].carried);
    }
    const auto &n = f.numbers;
    for (const auto &usable : usable_items(worked, game_of(identity_)))
    {
        auto &item =
            held[static_cast<std::size_t>(f.worked_from[static_cast<std::size_t>(usable.ordinal)])];
        const auto now = std::ranges::find(n.items, usable.ordinal, &UsableItem::ordinal);
        if (now == n.items.end())
        {
            item.gone = true;
            continue;
        }
        auto &raw = item.carried.item;
        for (std::size_t at = 0; now->scroll && at < now->spells.size(); ++at)
        {
            if (now->spells[at] == 0)
            {
                raw.effect_codes[at] = 0;
                raw.raw[60 + at] = 0;
            }
        }
        if (!now->scroll && usable.charges > 0)
        {
            raw.effect_codes[0] = static_cast<std::uint8_t>(now->charges);
            raw.raw[60] = raw.effect_codes[0];
        }
        if (now->quantity > 0)
        {
            raw.stack_size = static_cast<std::uint8_t>(now->quantity);
            raw.raw[57] = raw.stack_size;
        }
    }
    // The missile's shots: a thrown weapon, or the arrows or quarrels its
    // launcher fires; a readied weapon gone from its hand was thrown.
    namespace bit = por::item_property;
    const auto ammunition = ammunition_types(game_of(identity_));
    for (const int at : f.worked_from)
    {
        auto &weapon = held[static_cast<std::size_t>(at)];
        const auto &c = weapon.carried;
        if (!c.item.readied() || c.type.worn_location != 0)
        {
            continue;
        }
        if (n.weapon_type != c.item.type)
        {
            weapon.gone = true;
            continue;
        }
        const auto &missile = n.missile;
        if (!missile || missile->needs_no_ammunition)
        {
            continue;
        }
        for (const int shot_at : f.worked_from)
        {
            auto &shot = held[static_cast<std::size_t>(shot_at)];
            const auto type = shot.carried.item.type;
            const bool arrows = c.type.has(bit::fires_arrows) && type == ammunition.arrows;
            const bool quarrels = c.type.has(bit::fires_quarrels) && type == ammunition.quarrels;
            const bool shoots = missile->thrown
                                    ? shot_at == at
                                    : shot.carried.item.readied() && (arrows || quarrels);
            if (!shoots)
            {
                continue;
            }
            if (!missile->loaded)
            {
                shot.gone = true;
            }
            else if (missile->quantity > 0)
            {
                shot.carried.item.stack_size = static_cast<std::uint8_t>(missile->quantity);
                shot.carried.item.raw[57] = shot.carried.item.stack_size;
            }
        }
    }
    return held;
}

// A creature's numbers with these items, as RecalcStats works them out from
// its record, at the Strength it has now: a monster's with all it carries, a
// character's with those it has readied, in the order it readied them.
// `order` gets their places among the held.
Combatant Session::numbers_with(const Fighter &f, const std::vector<Held> &held,
                                std::vector<int> &order) const
{
    order.clear();
    for (std::size_t at = 0; at < held.size(); ++at)
    {
        const bool counted = f.weighs_unreadied || held[at].carried.item.readied();
        if (!held[at].gone && counted)
        {
            order.push_back(static_cast<int>(at));
        }
    }
    if (!f.weighs_unreadied)
    {
        std::ranges::stable_sort(order,
                                 [&held](int a, int b)
                                 {
                                     return held[static_cast<std::size_t>(a)].readied_order <
                                            held[static_cast<std::size_t>(b)].readied_order;
                                 });
    }
    std::vector<CarriedItem> items;
    for (const int at : order)
    {
        items.push_back(held[static_cast<std::size_t>(at)].carried);
    }
    auto record = *f.record;
    record.current_abilities.strength = static_cast<std::uint8_t>(f.numbers.strength);
    record.current_abilities.exceptional_strength =
        static_cast<std::uint8_t>(f.numbers.exceptional_strength);
    return combatant_from_record(record, items);
}

// The creature's numbers as its items change (charhlp.pas, RecalcStats;
// cbtacts.pas, RecalcAttacksLeft): what its items give changes by the
// difference, and what the fight has done to it (drained levels, effects,
// Strength) stays. One that has not attacked yet this round gets the new
// weapon's attacks; one that has keeps no more than that.
void Session::rework_gear(Fighter &f, const std::vector<Held> &before_held,
                          std::vector<Held> after_held)
{
    std::vector<int> before_order, after_order;
    const auto before = numbers_with(f, before_held, before_order);
    const auto after = numbers_with(f, after_held, after_order);
    const auto attacks_with = [&](const Combatant &c)
    {
        const auto &missile = c.missile;
        if (missile && missile->loaded)
        {
            const int attacks = attacks_this_round(paced(f, missile->rate_per_two_rounds),
                                                   static_cast<int>(round_));
            return missile->needs_no_ammunition ? attacks
                                                : std::min(attacks, std::max(missile->quantity, 1));
        }
        return attacks_this_round(paced(f, c.attacks[0].rate_per_two_rounds),
                                  static_cast<int>(round_));
    };
    auto &n = f.numbers;
    n.thac0 += after.thac0 - before.thac0;
    n.armor_class += after.armor_class - before.armor_class;
    n.rear_armor_class += after.rear_armor_class - before.rear_armor_class;
    n.save_bonus += after.save_bonus - before.save_bonus;
    n.movement += after.movement - before.movement;
    n.attacks[0] = after.attacks[0];
    n.large_attack = after.large_attack;
    n.missile = after.missile;
    n.striker = after.striker;
    n.weapon_type = after.weapon_type;
    n.backstabs = after.backstabs;
    n.strength_counts = after.strength_counts;
    // What an effect gives (Secret's reflection) stays.
    n.wards_gaze = after.wards_gaze || (n.wards_gaze && !before.wards_gaze);
    n.mirror = after.mirror || (n.mirror && !before.mirror);
    n.items = after.items;
    const int had = attacks_with(before), now = attacks_with(after);
    f.attacks_left[0] = f.attacks_left[0] >= had ? now : std::min(f.attacks_left[0], now);
    f.held = std::move(after_held);
    f.worked_from = std::move(after_order);
}

// Readying an item, or putting one away, as the View menu does in a fight
// (viewchar.pas, ToggleReadyItem): it takes no action.
void Session::toggle_readied(Fighter &f, std::size_t index)
{
    const auto before = held_now(f);
    auto after = before;
    place_readied(after, index, !after[index].carried.item.readied());
    rework_gear(f, before, std::move(after));
    f.readied_changed = true;
}

// What the original's AI makes of a weapon (combatai.pas, CalcWeaponPriority):
// its dice, 8 a plus, 2 a point of its damage bonus, 2 a shot beyond one for a
// launcher and 3 for one hand; holy water against undead is worth 8; none
// for one needing more hands than are free, an aligned weapon of another
// alignment, the Javelin of Lightning or a cursed weapon.
int Session::weapon_priority(const Fighter &f, const Held &weapon, int other_hands) const
{
    namespace bit = por::item_property;
    constexpr int holy_water = 85, aligned_weapon = 132, javelin_of_lightning = 83;
    const auto &type = weapon.carried.type;
    const auto &item = weapon.carried.item;
    const auto &damage = type.small_medium_damage;
    int priority = damage.count * damage.sides;
    priority += item.magic_bonus > 0 ? item.magic_bonus * 8 : 0;
    priority += damage.modifier > 0 ? damage.modifier * 2 : 0;
    const auto target = std::ranges::find(fighters_, f.lock, &Fighter::id);
    const bool pool_numbers = !secret() && !vga_engine(game_of(identity_));
    if (pool_numbers && item.type == holy_water && f.lock && target != fighters_.end() &&
        target->numbers.undead_level > 0)
    {
        priority = 8;
    }
    if (type.has(bit::fires_ammunition))
    {
        priority += (type.rate_of_fire - 1) * 2;
    }
    priority += type.hands <= 1 ? 3 : 0;
    const bool other_alignment = item.effect_codes[2] == aligned_weapon &&
                                 (item.effect_codes[1] & 0xF) != f.numbers.alignment;
    if (type.hands + other_hands > 3 || other_alignment ||
        item.effect_codes[1] == javelin_of_lightning || item.cursed_raw != 0)
    {
        priority = 0;
    }
    return priority;
}

// The original's AI readies its best weapon and shield (combatai.pas,
// EquipWeaponAndArmor): its best missile weapon when it scores above half the
// best melee weapon's (its own blows set the bar), has something to shoot and
// is thrown with Strength or has no enemy next to it; else its best melee
// weapon; and the shield of the best plus its hands leave room for. A cursed
// item is never put away.
void Session::equip_best(Fighter &f)
{
    if (!f.record)
    {
        return;
    }
    namespace bit = por::item_property;
    constexpr std::uint8_t weapon_place = 0, shield_place = 1;
    const auto before = held_now(f);
    auto after = before;
    int other_hands = 0;
    std::optional<std::size_t> weapon, shield;
    for (std::size_t at = 0; at < after.size(); ++at)
    {
        const auto &c = after[at].carried;
        if (after[at].gone || !c.item.readied())
        {
            continue;
        }
        if (c.type.worn_location == weapon_place)
        {
            weapon = at;
        }
        else if (c.type.worn_location == shield_place)
        {
            shield = at;
        }
        else
        {
            other_hands += c.type.hands;
        }
    }
    const auto &unarmed = f.record->base_attacks[0].damage;
    int best_melee_score =
        unarmed.count * unarmed.sides + (unarmed.modifier > 0 ? unarmed.modifier * 2 : 0);
    int best_ranged_score = 1, best_shield_score = 0;
    std::optional<std::size_t> best_melee, best_ranged, best_shield;
    for (std::size_t at = 0; at < after.size(); ++at)
    {
        const auto &held = after[at];
        const auto &type = held.carried.type;
        if (held.gone || (type.class_restrictions & f.record->item_limit) == 0)
        {
            continue;
        }
        if (type.worn_location == weapon_place)
        {
            const int score = weapon_priority(f, held, other_hands);
            const bool launcher = type.has(bit::fires_ammunition);
            if ((launcher || type.has(bit::thrown)) && score > best_ranged_score)
            {
                best_ranged = at;
                best_ranged_score = score;
            }
            if (!launcher && score > best_melee_score)
            {
                best_melee = at;
                best_melee_score = score;
            }
        }
        else if (type.worn_location == shield_place)
        {
            const int plus = held.carried.item.magic_bonus;
            const int score = plus >= 0 ? plus + 1 : 0;
            if (score > best_shield_score)
            {
                best_shield = at;
                best_shield_score = score;
            }
        }
    }
    bool supplied = false, thrown_with_strength = false;
    if (best_ranged)
    {
        const auto &type = after[*best_ranged].carried.type;
        constexpr std::uint8_t sling = bit::fires_ammunition | bit::dexterity_missile;
        const auto ammunition = ammunition_types(game_of(identity_));
        const auto readied_of = [&after](std::uint8_t wanted)
        {
            return std::ranges::any_of(after,
                                       [wanted](const Held &h)
                                       {
                                           return !h.gone && h.carried.item.readied() &&
                                                  h.carried.item.type == wanted;
                                       });
        };
        supplied = type.has(bit::thrown) || type.properties == sling ||
                   (type.has(bit::fires_arrows) && readied_of(ammunition.arrows)) ||
                   (type.has(bit::fires_quarrels) && readied_of(ammunition.quarrels));
        thrown_with_strength =
            type.range > 1 && type.has(bit::thrown) && type.has(bit::strength_bonus);
    }
    const bool shoots = best_ranged && best_ranged_score > best_melee_score / 2 && supplied &&
                        (thrown_with_strength || !enemy_next_to(f));
    const auto chosen = shoots ? best_ranged : best_melee;
    const auto cursed = [&after](std::optional<std::size_t> at)
    {
        return at && after[*at].carried.item.cursed_raw != 0;
    };
    const auto ready = [&after](std::optional<std::size_t> at, bool readied)
    {
        if (at)
        {
            place_readied(after, *at, readied);
        }
    };
    if (weapon != chosen && !cursed(weapon))
    {
        ready(weapon, false);
        ready(chosen, true);
        weapon = chosen;
    }
    const int weapon_hands = weapon ? after[*weapon].carried.type.hands : 0;
    const int shield_hands = shield ? after[*shield].carried.type.hands : 0;
    if (other_hands + weapon_hands + shield_hands > 2)
    {
        // Too many hands: the shield goes, or the weapon when the shield is
        // cursed or there is none.
        ready(shield && !cursed(shield) ? shield : weapon, false);
    }
    else if (other_hands + weapon_hands < 2 && shield != best_shield && !cursed(shield))
    {
        ready(shield, false);
        ready(best_shield, true);
    }
    bool changed = false;
    for (std::size_t at = 0; at < after.size(); ++at)
    {
        changed = changed || after[at].carried.item.readied() != before[at].carried.item.readied();
    }
    if (changed)
    {
        rework_gear(f, before, std::move(after));
        f.readied_changed = true;
    }
}

// Youth (Secret's item power 63, SpellYouth): its user becomes 1d4+1 years
// younger, but not below 18.
void Session::make_younger(Fighter &user)
{
    constexpr int youngest = 18;
    const int years = roll(4) + 1;
    auto *age = user.find_effect(effect::age_in_years);
    if (age && age->strength - years < youngest)
    {
        age->strength = youngest;
        log({"{name} is now 18 years old.", {{"name", user.name}}});
        return;
    }
    if (age)
    {
        age->strength -= years;
    }
    log({"{name} becomes younger.", {{"name", user.name}}});
}

// What using an item spends (viewchar.pas, UseItem): a scroll the spell read,
// gone with its last; a stack one of it; another item a charge, gone at 0.
// An item of no charges never runs out.
void Session::spend_item(Fighter &user, int ordinal, int number)
{
    auto &items = user.numbers.items;
    const auto item = std::find_if(items.begin(), items.end(),
                                   [ordinal](const UsableItem &i)
                                   {
                                       return i.ordinal == ordinal;
                                   });
    if (item == items.end())
    {
        return;
    }
    if (item->scroll)
    {
        if (const auto read = std::find(item->spells.begin(), item->spells.end(), number);
            read != item->spells.end())
        {
            *read = 0;
        }
        if (item->spells == std::array<int, 3>{})
        {
            items.erase(item);
        }
        return;
    }
    if (item->charges <= 0)
    {
        return;
    }
    if (item->quantity > 1)
    {
        --item->quantity;
        return;
    }
    if (--item->charges == 0)
    {
        items.erase(item);
    }
}

// The creatures on a path from a caster through its aim, as Curse's area
// spells take them (ovr023, BuildAreaDamageTargets in its reconstruction): the
// aim is carried on the caster's way to max_range squares, kept where the
// caster can see, and every creature but the caster on the line back to it,
// and on one or two lines beside it when the path is that wide, is taken.
std::vector<EntityId> Session::path_area(const Fighter &caster, Cell aim, int max_range,
                                         int width) const
{
    constexpr std::array<Cell, 8> beside{
        {{1, 0}, {1, 0}, {0, 1}, {0, 1}, {-1, 0}, {-1, 0}, {0, -1}, {0, -1}}};
    constexpr std::array<Cell, 8> other_side{
        {{-1, 0}, {0, -1}, {0, -1}, {1, 0}, {1, 0}, {0, 1}, {0, 1}, {-1, 0}}};
    Cell end = aim;
    if (aim != caster.cell)
    {
        LineWalk out(caster.cell, {caster.cell.x + (aim.x - caster.cell.x) * max_range * 2,
                                   caster.cell.y + (aim.y - caster.cell.y) * max_range * 2});
        while (out.next() && board_.contains(out.at) &&
               board_.at(out.at) != rules::Terrain::obstacle)
        {
            if (out.distance <= LineWalk(caster.cell, aim).distance)
            {
                continue;
            }
            if (out.distance > max_range * 2)
            {
                break;
            }
            end = out.at;
        }
    }
    std::vector<EntityId> found;
    const auto gather = [&](Cell from)
    {
        if (!board_.contains(from))
        {
            return;
        }
        LineWalk back(from, caster.cell);
        do
        {
            if (const auto id = standing_at(back.at);
                id && id != caster.id && std::ranges::find(found, id) == found.end())
            {
                found.push_back(id);
            }
        } while (back.next());
    };
    gather(end);
    const auto toward_caster = static_cast<std::size_t>(direction_toward(end, caster.cell));
    if (width > 1)
    {
        gather({end.x + beside[toward_caster].x, end.y + beside[toward_caster].y});
    }
    if (width > 2)
    {
        gather({end.x + other_side[toward_caster].x, end.y + other_side[toward_caster].y});
    }
    return found;
}

// The original's resolver (magic.pas, ResolveSpell): each target saves if the
// spell allows, a touch must hit first, the one damage roll is shared, and the
// spell's effect is added through the immunities.
void Session::cast_generic(Fighter &caster, int number, int level,
                           const std::vector<EntityId> &targets, int damage, int element,
                           int save_modifier, int minutes)
{
    if (damage <= 0)
    {
        element = 0;
    }
    const auto &known = *game_spell(number);
    for (const auto id : targets)
    {
        auto &target = *find(id);
        bool saved = false;
        if (known.save_result != 0)
        {
            saved = saving_throw(target, known.save_type, save_modifier, &caster, element);
        }
        if (known.range_fixed == 255)
        {
            const int natural = roll(20);
            if (!attack_hits(natural, caster.numbers.thac0, target.numbers.armor_class,
                             attack_modifier(caster, target, game_of(identity_))))
            {
                log({"{attacker} misses {name}.",
                     {{"attacker", caster.name}, {"name", target.name}}});
                saved = true;
                damage = 0;
            }
        }
        if (damage > 0)
        {
            nonweapon_damage(target, damage, known.save_result, saved, element, level, number,
                             targets.size() == 1 && (known.area & 0x0F) < 8);
        }
        if (known.effect > 0 && target.condition != Condition::dead)
        {
            const int lasting =
                minutes >= 0 ? minutes : known.minutes_fixed + known.minutes_per_level * level;
            const int strength = known.effect == effect::charm
                                     ? (caster.side == Side::opposition ? 128 : 0) + level
                                     : level;
            if (add_with_message(target,
                                 {known.effect, lasting, strength, known.effect == effect::charm},
                                 known.save_result == 1, saved, element, level, &known) &&
                known.effect == effect::charm)
            {
                charm(target, caster.side);
            }
        }
    }
}

// The chance magic resistance resists a caster of this level: 15% at level
// 11 and 5 more a level above (50% for the stronger kind, 105); the original's
// byte wraps a chance below 0 to one that always resists (effhndlr.pas, 105, 106).
bool magic_resists(const Fighter &target, int caster_level, int roll_100)
{
    for (const auto [code, base] : {std::pair{105, 50}, std::pair{106, 15}})
    {
        if (!target.has(code))
        {
            continue;
        }
        const int chance = base - (11 - caster_level) * 5;
        if (chance < 0 || roll_100 <= chance)
        {
            return true;
        }
    }
    // Curse's (305, 306) lessen with a higher caster's level, as AD&D's do
    // (ResistMagicPercent in its reconstruction).
    for (const auto [code, base] : {std::pair{305, 50}, std::pair{306, 15}})
    {
        if (target.has(code) && roll_100 <= base + (11 - caster_level) * 5)
        {
            return true;
        }
    }
    // Secret's lessens as Curse's do, from the percent its record holds
    // (fxengine.pas, CheckSpellResistance, in its reconstruction).
    const int percent = target.numbers.magic_resistance;
    return percent > 0 && roll_100 <= percent + (11 - caster_level) * 5;
}

// Secret's spells a creature's race or flags refuse (magic.pas, in its
// reconstruction): Charm Person takes one of the player's races, or a monster
// its flags call a person; Charm Person or Mammal one they call a mammal; Slay
// Living and Death Spell cannot slay the unliving. Secret's own spells are
// numbered 200 higher in the rules (canonical_spell).
bool Session::refuses_spell(const Fighter &target, int number) const
{
    constexpr int charm_person = 10, slay_living = 176, charm_person_or_mammal = 296,
                  death_spell = 310;
    if (!secret())
    {
        return false;
    }
    const bool monster = target.numbers.race == 0;
    const int traits = target.numbers.traits;
    switch (number)
    {
    case charm_person:
        return monster && (traits & trait::person) == 0;
    case charm_person_or_mammal:
        return monster && (traits & trait::mammal) == 0;
    case slay_living:
    case death_spell:
        return (traits & trait::unliving) != 0;
    default:
        return false;
    }
}

// Whether a creature resists magic, by Pool of Radiance's or Curse's numbers
// or by its record in Secret.
bool resists_magic_at_all(const Fighter &target)
{
    return target.has(105) || target.has(106) || target.has(305) || target.has(306) ||
           target.numbers.magic_resistance > 0;
}

// Secret of the Silver Blades' own spells (magic.pas, in its reconstruction),
// by the rules' numbers (canonical_spell).
void Session::cast_secret_spell(Fighter &caster, int number, int level,
                                const std::vector<EntityId> &targets, Cell aim)
{
    constexpr int heal = 236, harm = 256, barkskin = 290, death_spell = 310, disintegrate = 311,
                  stone_to_flesh = 313, flesh_to_stone = 314, delayed_blast_fireball = 315,
                  mass_invisibility = 316, power_word_stun = 317;
    constexpr int diseased = 34;
    const auto &known = *game_spell(number);
    const auto roll_dice = [this](int count, int sides)
    {
        int total = 0;
        for (int n = 0; n < count; ++n)
        {
            total += roll(sides);
        }
        return total;
    };
    switch (number)
    {
    case heal:
    case barkskin:
        // Secret's table casts Barkskin as Heal (spellFns[90]).
        for (const auto id : targets)
        {
            heal_all_but_1d4(*find(id));
        }
        break;
    case harm:
        // Harm diseases its target and leaves it 1d4 Hit Points.
        for (const auto id : targets)
        {
            auto &target = *find(id);
            (void)add_with_message(target, {diseased, 0, level, false}, false, false, 8, level,
                                   &known);
            nonweapon_damage(target, target.hit_points - roll(4), 0, false, 8, level, number,
                             false);
        }
        break;
    case death_spell:
        cast_death_spell(targets);
        break;
    case disintegrate:
    case flesh_to_stone:
        // A failed save against spells slays outright, or turns to stone.
        for (const auto id : targets)
        {
            auto &target = *find(id);
            if (turns_aside_slaying(target) || saving_throw(target, 4, 0, &caster))
            {
                continue;
            }
            if (number == disintegrate)
            {
                kill(target, "{name} disintegrates.");
            }
            else
            {
                turn_to_stone(target);
            }
        }
        break;
    case stone_to_flesh:
        // A statue lives again with 1 Hit Point, where it stood.
        for (const auto id : targets)
        {
            auto &target = *find(id);
            if (target.condition != Condition::stoned || !cell_free(target))
            {
                log({"{name} is unaffected.", {{"name", target.name}}});
                continue;
            }
            stand_up(target, 1, "{name} is no longer a statue.");
        }
        break;
    case delayed_blast_fireball:
        cast_delayed_blast(caster, level, targets, aim);
        break;
    case mass_invisibility:
    {
        // The caster's side, but not one with an enemy next to it.
        std::vector<EntityId> hidden;
        for (const auto id : targets)
        {
            const auto &target = *find(id);
            if (target.side == caster.side && !enemy_next_to(target))
            {
                hidden.push_back(id);
            }
        }
        cast_generic(caster, number, level, hidden, 0, 0);
        break;
    }
    case power_word_stun:
        // Stunned for 4d4 rounds at up to 30 Hit Points, 2d4 to 60 and 1d4
        // to 90; above that it is unaffected.
        for (const auto id : targets)
        {
            auto &target = *find(id);
            const int hit_points = target.hit_points;
            const int rounds = hit_points <= 30   ? roll_dice(4, 4)
                               : hit_points <= 60 ? roll_dice(2, 4)
                               : hit_points <= 90 ? roll(4)
                                                  : 0;
            if (rounds == 0)
            {
                log({"{name} is unaffected.", {{"name", target.name}}});
                continue;
            }
            (void)add_with_message(target, {effect::stunned, rounds, level, false}, false, false, 0,
                                   level, &known);
        }
        break;
    default:
        break;
    }
}

// The Strength spells (magic.pas, SpellEnlarge, SpellReduce, SpellStrength,
// SpellGiantStrength): each effect keeps the Strength it gives in its
// strength, by the original's encoding (strength_code).
void Session::cast_strength_spell(Fighter &caster, int number, int level,
                                  const std::vector<EntityId> &targets)
{
    constexpr int enlarge = 12, reduce = 13, giant_strength = 59;
    const auto &known = *game_spell(number);
    const int minutes = known.minutes_fixed + known.minutes_per_level * level;
    const auto game = game_of(identity_);
    for (const auto id : targets)
    {
        auto &target = *find(id);
        const auto &n = target.numbers;
        if (number == reduce)
        {
            if (!target.has(effect::enlarged) || saving_throw(target, 4, 0, &caster))
            {
                log({"{name} is unaffected.", {{"name", target.name}}});
                continue;
            }
            target.remove(effect::enlarged);
            log({"{name} has been reduced.", {{"name", target.name}}});
            recount_strength(target);
            continue;
        }
        const auto [base, exceptional] =
            number == enlarge ? enlarged_strength(level, game)
            : number == giant_strength
                ? std::pair{21, 0}
                : strengthened(n.strength, n.exceptional_strength, n.class_levels, game,
                               roll(strength_spell_die(n.class_levels, game)));
        const int code = number == enlarge ? effect::enlarged : effect::strengthened;
        const bool stronger = stronger_than(base, exceptional, n.strength, n.exceptional_strength);
        // A Potion of Giant Strength lasts the fight (its table's no time).
        const int lasting = number == giant_strength ? 0 : minutes;
        if (add_with_message(target, {code, lasting, strength_code(base, exceptional), false},
                             false, false, 0, level, &known))
        {
            recount_strength(target);
            if (stronger)
            {
                log({"{name} is stronger.", {{"name", target.name}}});
            }
        }
    }
}

// A creature's Strength again (fxengine.pas, RecalcAbility): its own, less
// what a disease has wasted, or the best its Enlarge and Strength effects
// give; a change moves its blows' to-hit and damage when they take Strength.
void Session::recount_strength(Fighter &f)
{
    auto &n = f.numbers;
    int base = n.natural_strength, exceptional = n.natural_exceptional_strength;
    if (const auto *wasted = f.find_effect(effect::strength_wasted); wasted && wasted->strength > 0)
    {
        base = std::max(3, base - wasted->strength);
        exceptional = 0;
    }
    for (const auto &e : n.effects)
    {
        if (e.code != effect::enlarged && e.code != effect::strengthened)
        {
            continue;
        }
        const auto [given, given_exceptional] = strength_of_code(e.strength);
        if (stronger_than(given, given_exceptional, base, exceptional))
        {
            base = given;
            exceptional = given_exceptional;
        }
    }
    if (n.strength_counts)
    {
        n.thac0 += strength_to_hit_bonus(n.strength, n.exceptional_strength) -
                   strength_to_hit_bonus(base, exceptional);
        const int more_damage = strength_damage_bonus(base, exceptional) -
                                strength_damage_bonus(n.strength, n.exceptional_strength);
        n.attacks[0].modifier += more_damage;
        if (n.large_attack)
        {
            n.large_attack->modifier += more_damage;
        }
    }
    n.strength = base;
    n.exceptional_strength = exceptional;
}

// Out of doors a fireball catches only those within 2 squares of its aim,
// not 3 (magic.pas, SpellFireball, outside 3D maps).
std::vector<EntityId> Session::fireball_reach(const std::vector<EntityId> &targets, Cell aim) const
{
    return outdoors_ ? within(aim, 2) : targets;
}

// Secret's Delayed Blast Fireball (SpellDelayedBlastFireball): with no delay
// it bursts at once, the caster's level and as many d6 of fire on all within
// 3 of the aim; with one it waits on its square for those rounds (effect 100,
// EffDelayedBlastFireball). The AI never delays it: the original asked anyone
// casting it, the computer's creatures too, for the delay.
void Session::cast_delayed_blast(Fighter &caster, int level, const std::vector<EntityId> &targets,
                                 Cell aim)
{
    constexpr int fire = 9, delayed_blast_fireball = 315;
    const int delay = std::max(caster.blast_delay, 0);
    caster.blast_delay = -1;
    if (delay == 0)
    {
        int total = level;
        for (int die = 0; die < level; ++die)
        {
            total += roll(6);
        }
        cast_generic(caster, delayed_blast_fireball, level, targets, total, fire);
        return;
    }
    const int index =
        static_cast<int>(std::ranges::count(blasts_, caster.id, &DelayedBlast::caster));
    blasts_.push_back({caster.id, index, aim, level});
    caster.numbers.effects.push_back({effect::delayed_blast, delay, index, true});
    log({"{name} sets a delayed blast fireball.", {{"name", caster.name}}});
}

// The blast goes off on its square: everyone within 3 takes the caster's
// level and as many d6 of fire, half on a save against spells.
void Session::delayed_blast_goes_off(Fighter &caster, int index)
{
    constexpr int fire = 9, delayed_blast_fireball = 315;
    const auto blast = std::ranges::find_if(blasts_,
                                            [&](const DelayedBlast &b)
                                            {
                                                return b.caster == caster.id && b.index == index;
                                            });
    if (blast == blasts_.end())
    {
        return;
    }
    const auto set = *blast;
    blasts_.erase(blast);
    log({"Delayed Blast Fireball goes off.", {}});
    int total = set.level;
    for (int die = 0; die < set.level; ++die)
    {
        total += roll(6);
    }
    for (const auto id : within(set.cell, 3))
    {
        auto &target = *find(id);
        const bool saved = saving_throw(target, 4, 0, &caster);
        nonweapon_damage(target, total, 2, saved, fire, set.level, delayed_blast_fireball, false);
    }
}

// Heal (SpellHeal): disease, blindness and feeblemind cured, then every Hit
// Point but 1d4; one already that close to its maximum gains none.
void Session::heal_all_but_1d4(Fighter &target)
{
    constexpr int feebleminded = 268;
    bool cured = false;
    for (const int disease : {31, 34, 43, 44})
    {
        while (target.has(disease))
        {
            target.remove(disease);
            cured = true;
        }
    }
    if (cured)
    {
        log({"{name} is cured.", {{"name", target.name}}});
    }
    if (target.has(effect::blind))
    {
        target.remove(effect::blind);
        log({"{name} can see.", {{"name", target.name}}});
    }
    if (target.has(feebleminded))
    {
        target.remove(feebleminded);
        log({"{name} regains intellect.", {{"name", target.name}}});
    }
    const int short_of = roll(4);
    const int maximum = target.numbers.max_hit_points;
    if (target.hit_points + short_of <= maximum)
    {
        target.hit_points = maximum - short_of;
    }
    if (target.condition == Condition::dying)
    {
        target.condition = Condition::unconscious;
        target.overkill = 0;
    }
    log({"{name} is healed.", {{"name", target.name}}});
}

// Death Spell (SpellDeathSpell): 4d20 x 2 to spend on those in its area in
// turn, each costing by its level (1 at level 0, 2 at 1, 8 at 2 and 3, 16 at
// 4 and 5, 16 or 20 at 6 by whether its most Hit Points pass 51, 20 at 7 and
// 8, 162 above); each paid for drops dead. The unliving refuse it
// (refuses_spell).
void Session::cast_death_spell(const std::vector<EntityId> &targets)
{
    int budget = (roll(20) + roll(20) + roll(20) + roll(20)) * 2;
    for (const auto id : targets)
    {
        auto &target = *find(id);
        const int level = target.numbers.level;
        const int cost = level == 0   ? 1
                         : level == 1 ? 2
                         : level <= 3 ? 8
                         : level <= 5 ? 16
                         : level == 6 ? (target.numbers.max_hit_points <= 51 ? 16 : 20)
                         : level <= 8 ? 20
                                      : 162;
        if (budget < cost)
        {
            continue;
        }
        budget -= cost;
        kill(target, "{name} drops dead.");
    }
}

// Whether a spell that slays or petrifies outright is turned aside before
// its save (Secret's condition 9): an iron golem takes no spell, and an
// image takes it for its caster. Magic resistance does not turn it, as the
// original weighs resistance only against an effect or magical damage
// (CheckSpellResistance).
bool Session::turns_aside_slaying(Fighter &target)
{
    if (target.has(effect::iron_golem))
    {
        log({"{name} is unaffected.", {{"name", target.name}}});
        return true;
    }
    return mirror_absorbs(target);
}

// An effect added as a spell adds it (fxengine.pas, AddEffectWithMessage): the
// target's immunities may nullify it, a save that negates refuses it, and an
// effect it has already is replaced by a longer or lasting one.
bool Session::add_with_message(Fighter &target, Effect added, bool negates, bool saved, int element,
                               int caster_level, const Spell *known)
{
    const int code = added.code;
    const auto is = [code](std::initializer_list<int> codes)
    {
        return std::find(codes.begin(), codes.end(), code) != codes.end();
    };
    bool immune = resists_magic_at_all(target) && magic_resists(target, caster_level, roll(100));
    immune = immune || (target.has(107) && is({effect::sleep, effect::charm}) && roll(100) <= 90);
    immune = immune || (target.has(124) && is({effect::sleep, effect::charm}) && roll(100) <= 30);
    immune = immune || (target.has(108) && is({effect::charm, effect::sleep}));
    immune = immune || (target.has(109) && is({effect::paralysis}));
    immune = immune || (target.has(110) && (element & 2));
    immune = immune || (target.has(111) && is({effect::poisoned, effect::paralysis}));
    immune = immune || (target.has(112) && (element & 1));
    immune = immune || (target.has(125) &&
                        is({effect::charm, effect::sleep, effect::paralysis, effect::poisoned}));
    // Curse's protections (ovr013): from magic, every spell's effect; Minor
    // Globe of Invulnerability, a spell's below the fourth level; from cold
    // and electricity, theirs.
    const bool spell_effect = known && known->caster_class != SpellClass::item;
    immune = immune || (target.has(329) && known);
    // A dracolich shrugs off fear, enfeeblement and feeblemind (Curse's 133,
    // which it checked too late to work, docs/ORIGINAL-RULES.md, Bugs).
    immune = immune || (target.has(333) && is({342, effect::enfeebled, 268}));
    immune = immune || (target.has(263) && spell_effect && known->level < 4);
    immune = immune || (target.has(310) && (element & 2));
    immune = immune || (target.has(335) && (element & 4));
    // Secret's (effhndlr.pas, its condition 9): the Globe of Invulnerability
    // stops a spell's to the fourth level, the fearless are never afraid, and
    // an iron golem takes no effect at all.
    immune = immune ||
             (target.has(effect::globe_of_invulnerability) && spell_effect && known->level <= 4);
    immune = immune || (target.has(effect::fearless) && is({effect::fear}));
    immune = immune || target.has(effect::iron_golem);
    if (immune || (negates && saved))
    {
        log({"{name} is unaffected.", {{"name", target.name}}});
        return false;
    }
    if (auto *old = target.find_effect(code);
        old && ((old->minutes != 0 && added.minutes > old->minutes) || added.minutes == 0))
    {
        target.remove(code);
    }
    target.numbers.effects.push_back(added);
    if (code == effect::sleep && target.condition == Condition::okay)
    {
        target.initiative = 0;
    }
    log({"{name} is affected by {spell}.",
         {{"name", target.name}, {"spell", std::string(known ? known->name : ""), true}}});
    return true;
}

// Damage that is not a weapon's (fxengine.pas, InflictNonWeaponDamage): the
// target's resistances weigh it by its element (1 fire, 2 cold, 4
// electricity, 8 magic, 16 acid), then a save halves or negates it.
void Session::nonweapon_damage(Fighter &target, int damage, int save_result, bool saved,
                               int element, int caster_level, int spell_number, bool single_target)
{
    if (target.has(effect::iron_golem))
    {
        iron_golem_absorbs(target, damage, element);
        return;
    }
    // Mirror images take a single target's spell (effhndlr.pas, EffMirrorImage).
    if (single_target && mirror_absorbs(target))
    {
        damage = 0;
    }
    if (target.has(91) && (spell_number == 15 || (element & 4)))
    {
        damage = 0;
    }
    if ((element & 2) && target.has(effect::resist_cold))
    {
        damage /= 2;
    }
    if ((element & 1) && target.has(effect::resist_fire))
    {
        damage /= 2;
    }
    // Secret's resistance turns a spell's damage only when the spell carries
    // an effect too, so a Fireball's goes through (fxengine.pas,
    // CheckSpellResistance, in its reconstruction).
    const auto *cast = game_spell(spell_number);
    const bool resistible = target.numbers.magic_resistance == 0 || (cast && cast->effect != 0);
    if ((element & 8) && resistible && resists_magic_at_all(target) &&
        magic_resists(target, caster_level, roll(100)))
    {
        damage = 0;
    }
    // Curse's protections (ovr013): cold for one immune to it (310), dragon
    // breath (273), magic (329), electricity (335), and a spell below the
    // fourth level for a Minor Globe of Invulnerability (263), and Secret's
    // Globe of Invulnerability, to the fourth.
    const bool spell_cast = cast && cast->caster_class != SpellClass::item;
    if (((element & 2) && target.has(310)) || ((element & 0x20) && target.has(273)) ||
        ((element & 8) && target.has(329)) || ((element & 4) && target.has(335)) ||
        (target.has(263) && spell_cast && cast->level < 4) ||
        (target.has(effect::globe_of_invulnerability) && spell_cast && cast->level <= 4))
    {
        log({"{name} is unaffected.", {{"name", target.name}}});
        damage = 0;
    }
    // A shambling mound takes in electricity and grows: 1d8 Hit Points, past
    // its maximum (Curse's added 8 and threw its 1d8 away, docs/ORIGINAL-RULES.md, Bugs).
    if ((element & 4) && target.has(284) && damage > 0)
    {
        damage = 0;
        target.hit_points += roll(8);
        log({"{name} grows.", {{"name", target.name}}});
    }
    // Resisting fire and cold (282): a save takes none of a spell that allows
    // one, else half.
    if ((element & 3) && target.has(282) && damage > 0)
    {
        damage = saving_throw(target, 4, 0) && save_result != 0 ? 0 : damage / 2;
    }
    if ((element & 1) && target.has(112))
    {
        damage = 0;
    }
    if ((element & 4) && target.has(114))
    {
        damage /= 2;
    }
    if ((element & 2) && target.has(118))
    {
        damage /= 2;
    }
    if (spell_number == 15 && target.has(effect::shield))
    {
        damage = 0;
    }
    if ((element & 1) && target.has(93))
    {
        damage /= 2;
    }
    if (target.has(effect::troll_heal) && !target.has(effect::regeneration) &&
        !target.has(effect::regenerating_soon))
    {
        target.numbers.effects.push_back({effect::regenerating_soon, 3, 0, true});
    }
    if (saved && save_result == 1)
    {
        damage = 0;
    }
    if (saved && save_result == 2)
    {
        damage /= 2;
    }
    // A fire shield doubles what it is made of, unless saved against.
    if (!saved && (((element & 1) && target.has(250)) || ((element & 2) && target.has(254))))
    {
        damage *= 2;
    }
    if (damage <= 0)
    {
        return;
    }
    log({"{name} takes {damage}.", {{"name", target.name}, {"damage", std::to_string(damage)}}});
    inflict(target, damage);
}

// Secret's iron golem takes no damage but a weapon's (effhndlr.pas,
// EffIronGolem, in its reconstruction): magical fire heals it by the damage,
// up to its maximum, before any save; electricity slows it for 3 rounds.
void Session::iron_golem_absorbs(Fighter &golem, int damage, int element)
{
    if ((element & 1) && (element & 8))
    {
        golem.hit_points = std::min(golem.hit_points + damage, golem.numbers.max_hit_points);
        log({"{name} is healed.", {{"name", golem.name}}});
    }
    else if (element & 4)
    {
        golem.numbers.effects.push_back({effect::slowed, 3, 255, false});
        log({"{name} is slowed.", {{"name", golem.name}}});
    }
    else
    {
        log({"{name} is unaffected.", {{"name", golem.name}}});
    }
}

// A charmed creature fights for its charmer (effhndlr.pas, the charm).
void Session::charm(Fighter &target, Side charmer)
{
    target.own_side = target.side;
    target.side = charmer;
    target.fleeing = false;
}

// Sleep (magic.pas, the sleep): 4d4 levels' worth around the aim, the nearest
// first, allies too; a creature costs 1 up to level 1, 2 at 2, 4 at 3, 6 at 4,
// 10 at 5 for a monster and 20 otherwise, 20 from 6 up, spent before its
// immunities are tried. Those already asleep and the animated dead are
// passed over.
void Session::cast_sleep(int level, Cell aim)
{
    int pool = roll(4) + roll(4) + roll(4) + roll(4);
    const auto &known = *game_spell(21);
    for (const auto id : within(aim, known.area & 7))
    {
        auto &f = *find(id);
        if (f.has(effect::sleep) || f.has(effect::animated))
        {
            continue;
        }
        const int highest = f.numbers.level;
        const int cost = highest <= 1   ? 1
                         : highest == 2 ? 2
                         : highest == 3 ? 4
                         : highest == 4 ? 6
                         : highest == 5 ? (f.numbers.race == 0 ? 10 : 20)
                                        : 20;
        if (cost > pool)
        {
            continue;
        }
        pool -= cost;
        add_with_message(f, {effect::sleep, 5 * level, level, false}, false, false, 0, level,
                         &known);
    }
}

// Whether a carrier of Silence stands within a square of it, itself
// included (effhndlr.pas, EffSilence's aura).
bool Session::silenced(const Fighter &f) const
{
    return std::any_of(fighters_.begin(), fighters_.end(),
                       [&](const Fighter &carrier)
                       {
                           return carrier.able() && carrier.has(effect::silence) &&
                                  (carrier.id == f.id || distance(carrier, f) <= 1);
                       });
}

// Prayer: within 6 squares of the first one praying, in its sight, its own
// side gets +1 to attacks and saves and the other side -1 (effhndlr.pas,
// EffPrayer, EffAlliedPrayer).
int Session::prayer_bonus(const Fighter &f) const
{
    for (const auto &carrier : fighters_)
    {
        const auto *node = carrier.able() ? carrier.find_effect(effect::prayer) : nullptr;
        if (!node ||
            (carrier.id != f.id && (distance(carrier, f) > 6 || !sees(board_, carrier, f))))
        {
            continue;
        }
        const Side praying = (node->strength & 0x10) ? Side::opposition : Side::party;
        return f.side == praying ? 1 : -1;
    }
    return 0;
}

// Protected by its own spell, or by a 10' radius one within a square of it
// (effhndlr.pas, 45 and 46).
bool Session::protected_from(const Fighter &f, int code, int aura) const
{
    if (f.has(code))
    {
        return true;
    }
    return std::any_of(fighters_.begin(), fighters_.end(),
                       [&](const Fighter &carrier)
                       {
                           return carrier.able() && carrier.has(aura) &&
                                  (carrier.id == f.id || distance(carrier, f) <= 1);
                       });
}

// An evil or good attacker 2 worse against one only a 10' radius protects
// (its own spell already counts in attack_modifier).
bool Session::aura_protects(const Fighter &attacker, const Fighter &defender) const
{
    const int alignment = attacker.numbers.alignment;
    const bool evil = alignment == 2 || alignment == 5 || alignment == 8;
    const bool good = alignment == 0 || alignment == 3 || alignment == 6;
    return (evil && !defender.has(effect::protection_from_evil) &&
            protected_from(defender, effect::protection_from_evil,
                           effect::protection_from_evil_aura)) ||
           (good && !defender.has(effect::protection_from_good) &&
            protected_from(defender, effect::protection_from_good,
                           effect::protection_from_good_aura));
}

// A mirror image takes the harm on 1d(images + 1) above 1, and is gone
// (effhndlr.pas, EffMirrorImage).
bool Session::mirror_absorbs(Fighter &target)
{
    auto *images = target.find_effect(effect::mirror_image);
    if (!images || roll(images->strength + 1) <= 1)
    {
        return false;
    }
    log({"{name} loses an image.", {{"name", target.name}}});
    if (--images->strength <= 0)
    {
        target.remove(effect::mirror_image);
    }
    return true;
}

// The creature standing on a square, 0 for none.
EntityId Session::standing_at(Cell cell) const
{
    for (const auto &f : fighters_)
    {
        const auto mine = squares_of(f);
        if (f.able() && std::find(mine.begin(), mine.end(), cell) != mine.end())
        {
            return f.id;
        }
    }
    return 0;
}

// Lightning strikes a square: one standing there saves against spells for
// half; a wall turns the bolt (magic.pas, LightningAttackAt).
bool Session::lightning_at(Cell cell, int damage, int level, int save)
{
    if (const auto id = standing_at(cell))
    {
        auto &target = *find(id);
        const bool saved = saving_throw(target, save, 0, nullptr, 12);
        nonweapon_damage(target, damage, 2, saved, 12, level, 51, false);
    }
    return board_.contains(cell) && board_.at(cell) == rules::Terrain::obstacle;
}

// The bolt runs on from the aim, away from the caster (magic.pas,
// LightningPath): each leg goes to the next creature, the field's edge (where
// the bolt ends), rough ground, or the end of its length; a wall turns it back
// past the caster, costing 8 more when the wall is within 4 squares of it.
// Each leg takes the whole distance walked so far from what is left, as the
// original counts it.
void Session::lightning_path(Cell origin, Cell aim, int length, int damage, int level, bool bounce,
                             int save)
{
    if (origin == aim)
    {
        return;
    }
    Cell target = aim;
    EntityId last = standing_at(aim);
    int sign = 1, budget = length * 2;
    while (budget > 0)
    {
        LineWalk walk(target, {target.x + (target.x - origin.x) * sign * budget,
                               target.y + (target.y - origin.y) * sign * budget});
        bool blocked = false;
        do
        {
            bool off_field = false;
            EntityId here = 0;
            if (walk.start != walk.end)
            {
                bool more = true;
                do
                {
                    more = walk.next();
                    here = standing_at(walk.at);
                    off_field = !board_.contains(walk.at);
                } while (more && !(here && here != last) && !off_field &&
                         board_.at(walk.at) == rules::Terrain::open && walk.distance < budget);
            }
            last = here;
            if (off_field)
            {
                budget = 0;
            }
            blocked = lightning_at(walk.at, damage, level, save);
            if (blocked)
            {
                target = walk.at;
                LineWalk back(target, origin);
                while (back.next())
                {
                }
                if (bounce && back.distance <= 8)
                {
                    walk.distance += 8;
                }
                sign = -sign;
                bounce = false;
                last = 0;
            }
            budget = walk.distance < budget ? budget - walk.distance : 0;
        } while (!blocked && budget > 0);
    }
}

// Stinking Cloud (magic.pas, SpellStinkingCloud): a cloud on the aim and the
// squares east, south-east and south of it, lasting the caster's level in
// rounds; those in it breathe it at once. Curse's Cloudkill (SpellCloudkill)
// lays its cloud on the aim and the eight squares around it.
void Session::lay_cloud(Fighter &caster, int level, Cell aim, CloudKind kind)
{
    // The caster's clouds of the kind before the last one laid, as the
    // original counts them.
    int index = 0;
    for (std::size_t n = 0; n + 1 < clouds_.size(); ++n)
    {
        index += clouds_[n].caster == caster.id && clouds_[n].kind == kind ? 1 : 0;
    }
    Cloud cloud{caster.id, index, aim, {}, kind};
    const auto squares = cloud.squares();
    for (std::size_t n = 0; n < squares.size(); ++n)
    {
        cloud.covers[n] =
            board_.contains(squares[n]) && board_.at(squares[n]) != rules::Terrain::obstacle;
    }
    clouds_.push_back(cloud);
    const bool stinking = kind == CloudKind::stinking;
    caster.numbers.effects.push_back(
        {stinking ? effect::stinking_cloud : effect::cloudkill, level, level + index * 16, true});
    log({stinking ? "{name} creates a noxious cloud." : "{name} creates a poisonous cloud.",
         {{"name", caster.name}}});
    for (const auto square : squares)
    {
        if (const auto id = standing_at(square))
        {
            breathe_cloud(*find(id));
        }
    }
}

bool Session::in_cloud(const Fighter &f, CloudKind kind) const
{
    const auto mine = squares_of(f);
    return std::any_of(clouds_.begin(), clouds_.end(),
                       [&mine, kind](const Cloud &cloud)
                       {
                           if (cloud.kind != kind)
                           {
                               return false;
                           }
                           const auto squares = cloud.squares();
                           for (std::size_t n = 0; n < squares.size(); ++n)
                           {
                               if (cloud.covers[n] &&
                                   std::find(mine.begin(), mine.end(), squares[n]) != mine.end())
                               {
                                   return true;
                               }
                           }
                           return false;
                       });
}

// In Secret the undead and the unliving (its record's flag) breathe a stinking
// cloud unharmed (fxengine.pas, ConsiderTerrainEffects, in its reconstruction).
bool Session::breathes_clouds_freely(const Fighter &f) const
{
    return secret() && (f.numbers.undead_level > 0 || (f.numbers.traits & trait::unliving) != 0);
}

// What the clouds do to one in them (fxengine.pas, ConsiderTerrainEffects).
void Session::breathe_cloud(Fighter &f)
{
    breathe_stinking_cloud(f);
    breathe_cloudkill(f);
}

// Cloudkill kills one of level 0 to 4 outright with its poison; one of level
// 5 saves against it at -4, one of level 6 at 0; one of a higher level breathes
// it unharmed.
void Session::breathe_cloudkill(Fighter &f)
{
    const int level = f.numbers.level;
    if (!f.able() || !in_cloud(f, CloudKind::deadly) || level > 6 ||
        (level >= 5 && saving_throw(f, 0, level == 5 ? -4 : 0)))
    {
        return;
    }
    log({"{name} is poisoned.", {{"name", f.name}}});
    f.numbers.effects.push_back({effect::poisoned, 0, 255, false});
    kill(f);
}

// One in a stinking cloud saves against paralysis: saved, it coughs for the
// round; failed, nausea leaves it helpless for 1d4+1 rounds. One coughing,
// nauseous, animated or immune to poison breathes it unharmed.
void Session::breathe_stinking_cloud(Fighter &f)
{
    if (!f.able() || !in_cloud(f, CloudKind::stinking) || f.has(effect::coughing) ||
        f.has(effect::nausea) || f.has(effect::animated) || f.has(111) || f.has(125) ||
        breathes_clouds_freely(f))
    {
        return;
    }
    const auto &known = *game_spell(34);
    if (saving_throw(f, 0, 0))
    {
        add_with_message(f, {effect::coughing, 1, 255, false}, false, true, 0, 0, &known);
        return;
    }
    add_with_message(f, {effect::nausea, roll(4) + 1, 255, false}, false, false, 0, 0, &known);
}

// An effect ends before its time, its handler running as it goes: a charm
// lets go, the animated collapse, and the rest end as when their time runs out.
void Session::remove_with_handler(Fighter &f, std::size_t node)
{
    const auto ended = f.numbers.effects[node];
    f.numbers.effects.erase(f.numbers.effects.begin() + static_cast<std::ptrdiff_t>(node));
    if (ended.code == effect::charm)
    {
        f.side = f.own_side;
        return;
    }
    if (ended.code == effect::animated)
    {
        // It collapses, dead again, what it was before (effhndlr.pas, EffAnimateDead).
        if (f.before_animation)
        {
            auto numbers = *f.before_animation;
            numbers.spells.clear();
            numbers.effects = f.numbers.effects;
            f.numbers = numbers;
            f.before_animation.reset();
        }
        f.side = f.own_side;
        f.condition = Condition::dead;
        f.hit_points = 0;
        f.initiative = 0;
        log({"{name} collapses.", {{"name", f.name}}});
        return;
    }
    if (ended.code == effect::enlarged || ended.code == effect::strengthened)
    {
        recount_strength(f);
    }
    if (ended.on_remove)
    {
        expire(f, ended);
    }
}

// Dispel Magic (magic.pas, the dispel): each effect of a strength below 255
// has its level in the strength's low 4 bits, and goes on a d100 within 50%,
// 5% more for each level the caster has above it or 2% less for each below.
void Session::dispel(Fighter &target, int level)
{
    bool any = false;
    for (std::size_t node = 0; node < target.numbers.effects.size();)
    {
        const auto &e = target.numbers.effects[node];
        if (e.code == effect::age_in_years)
        {
            ++node;
            continue;
        }
        const int effect_level = e.strength & 0x0F;
        const int chance = level >= effect_level ? 50 + 5 * (level - effect_level)
                                                 : 50 - 2 * (effect_level - level);
        if (e.strength < 255 && roll(100) <= chance)
        {
            remove_with_handler(target, node);
            any = true;
            continue;
        }
        ++node;
    }
    if (any)
    {
        log({"{name} is dispelled.", {{"name", target.name}}});
    }
}

// Animate Dead (magic.pas, the animation): up to the caster's level of the
// dead of creature type 0, either side, whose squares are free, rise whole
// for the caster as undead (level 2) moving 6, sweeping no one, their spells
// forgotten; the animated die at exactly 0 Hit Points.
void Session::animate_dead(Fighter &caster, int level)
{
    int left = level;
    for (auto &f : fighters_)
    {
        if (left <= 0 || f.condition != Condition::dead || f.numbers.creature_type != 0 ||
            f.has(effect::animated) || !cell_free(f))
        {
            continue;
        }
        --left;
        f.before_animation = f.numbers;
        f.own_side = f.side;
        f.side = caster.side;
        f.numbers.undead_level = 2;
        f.numbers.movement = 6;
        f.numbers.attack_level = 0;
        f.numbers.spells.clear();
        f.numbers.creature_type = 4;
        f.condition = Condition::okay;
        f.hit_points = f.numbers.max_hit_points;
        f.overkill = 0;
        f.initiative = 0;
        f.numbers.effects.push_back(
            {effect::animated, 0, (f.own_side == Side::opposition ? 16 : 0) + level, true});
        log({"{name} is animated.", {{"name", f.name}}});
    }
}

// Slow Poison (magic.pas, the slow poison): one poisoned, even killed by it,
// stands again with at least 1 Hit Point while the spell lasts, losing a point
// every 10 minutes but never the last; when it wears off still poisoned, it dies.
void Session::slow_poison(Fighter &target, int level)
{
    if (!target.has(effect::poisoned) || target.has(effect::animated))
    {
        return;
    }
    target.hit_points = std::max(target.hit_points, 1);
    target.numbers.effects.push_back({effect::slow_poison, 60 * level, 255, true});
    log({"{name} is affected by {spell}.",
         {{"name", target.name}, {"spell", std::string(game_spell(26)->name), true}}});
    revive(target);
    target.numbers.effects.push_back({effect::slow_poison_tick, 10, 255, true});
}

// The poisoned dead stand when their square is free, or try again a minute
// later (effhndlr.pas, EffDelayedRevival).
void Session::revive(Fighter &f)
{
    if (f.condition == Condition::okay)
    {
        return;
    }
    if (cell_free(f))
    {
        stand_up(f, std::max(f.hit_points, 1), "{name} gets back up.");
        return;
    }
    f.numbers.effects.push_back({effect::revival, 1, 255, true});
}

// The original's AI, a turn (combatai.pas, CombatAiControl): the approach
// pattern, then an item, a spell begun earlier, turning undead, a spell, and
// else the fight: lock onto an enemy and move and attack until the turn is
// done. Morale is checked as every turn starts (next_actor).
void Session::ai_turn(Fighter &f, bool magic)
{
    f.fought = false;
    // The pattern: kept 3 times in 4 while it is 1 to 4; rerolled 1 to 4,
    // or 5 to 6 one time in 8 (CombatAiControl).
    if (f.stance == 0 || f.stance > 4 || roll(4) == 1)
    {
        f.stance = roll(8) == 8 ? roll(2) + 4 : roll(4);
    }
    if (f.aiming)
    {
        if (!ai_aim(f))
        {
            lose_spell(f);
            end_actions(f);
        }
        return;
    }
    if (ai_use_item(f))
    {
        end_actions(f);
        return;
    }
    const auto undead_in_sight = std::any_of(fighters_.begin(), fighters_.end(),
                                             [&](const Fighter &o)
                                             {
                                                 return o.side != f.side && o.able() && !o.turned &&
                                                        o.numbers.undead_level > 0 &&
                                                        sees(board_, f, o);
                                             });
    if (f.numbers.class_levels[static_cast<std::size_t>(ClassSlot::cleric)] > 0 &&
        !f.tried_turning && undead_in_sight && !secret())
    {
        turn_undead(f);
        return;
    }
    if (ai_cast(f, magic))
    {
        return;
    }
    equip_best(f);
    while (f.able() && f.initiative > 0)
    {
        if (!acquire_target(f, 255, true, false) || f.initiative <= 0)
        {
            finish_round(f);
            return;
        }
        if (ai_move_attack(f))
        {
            return;
        }
    }
}

// The enemies standing within a range, in sight or through walls.
std::vector<EntityId> Session::enemies_within(const Fighter &f, int range, bool sight) const
{
    std::vector<EntityId> found;
    for (const auto &o : fighters_)
    {
        if (o.side != f.side && o.able() && distance(f, o) <= range &&
            (!sight || sees(board_, f, o)))
        {
            found.push_back(o.id);
        }
    }
    return found;
}

// Locking onto an enemy (cbtacts.pas, AcquireTarget): a lock still on an
// enemy that may be singled out stays; else up to 20 random picks among the
// enemies in range and sight that may be singled out; else, unless a new
// lock was forced, among those in range through walls, any of them when
// relaxed.
bool Session::acquire_target(Fighter &f, int range, bool relaxed, bool force_new)
{
    if (const auto *locked = find(f.lock);
        locked && !force_new && locked->side != f.side && locked->able() && can_target(f, *locked))
    {
        return true;
    }
    f.lock = 0;
    for (int pass = 1; pass <= 2; ++pass)
    {
        const bool through_walls = pass == 2 && !force_new;
        auto candidates = enemies_within(f, range, !through_walls);
        for (int tries = 0; tries < 20 && !candidates.empty(); ++tries)
        {
            auto &pick =
                candidates[static_cast<std::size_t>(roll(static_cast<int>(candidates.size())) - 1)];
            if (pick == 0)
            {
                continue;
            }
            if (can_target(f, *find(pick)) || (through_walls && relaxed))
            {
                f.lock = pick;
                return true;
            }
            pick = 0;
            if (std::all_of(candidates.begin(), candidates.end(),
                            [](EntityId id)
                            {
                                return id == 0;
                            }))
            {
                break;
            }
        }
    }
    return false;
}

// Whether its readied weapon reaches past the next square: a missile weapon,
// throwables too.
bool Session::missile_in_hand(const Fighter &f) const
{
    return f.numbers.missile.has_value();
}

// Nothing more to do (combatai.pas, FinishRound): a helpless creature, one
// with a missile weapon or one without a turn left ends its actions; another
// guards.
void Session::finish_round(Fighter &f)
{
    if (f.helpless() || missile_in_hand(f) || f.initiative <= 0)
    {
        end_actions(f);
        return;
    }
    end_actions(f);
    f.guarding = true;
    log({"{name} guards.", {{"name", f.name}}});
}

// Moving and attacking (combatai.pas, AiMoveAttack): up to 20 passes, each a
// step or an attack on an enemy within its weapon's reach. A fleeing creature
// spends its movement running first. True when its turn is done; false when
// it downed its target with attacks left, and locks onto another.
bool Session::ai_move_attack(Fighter &f)
{
    monster_specials(f);
    if (f.initiative <= 0 || !f.able())
    {
        return true;
    }
    int last_direction = 8, stuck = 0;
    for (int pass = 1; pass <= 20; ++pass)
    {
        if (f.fleeing)
        {
            while (f.half_moves > 0 && f.initiative > 0 && f.able())
            {
                if (!ai_step(f, last_direction, stuck))
                {
                    break;
                }
            }
        }
        if (f.initiative <= 0 || !f.able())
        {
            return true;
        }
        const int reach = missile_in_hand(f) ? f.numbers.missile->range : 1;
        // The lock, when it is an enemy within reach and in sight. The
        // original skipped any lock on the party's side, so monsters never
        // struck theirs first (an obvious bug, fixed).
        Fighter *target = find(f.lock);
        if (target && (!target->able() || target->side == f.side || !can_target(f, *target) ||
                       distance(f, *target) > reach || !sees(board_, f, *target)))
        {
            target = nullptr;
        }
        if (!target)
        {
            const auto in_reach = enemies_within(f, reach, true);
            if (in_reach.empty())
            {
                if (!acquire_target(f, 255, false, false))
                {
                    finish_round(f);
                    return true;
                }
                if (!ai_step(f, last_direction, stuck))
                {
                    return true;
                }
                continue;
            }
            auto &pick = *find(
                in_reach[static_cast<std::size_t>(roll(static_cast<int>(in_reach.size())) - 1)]);
            // A missile weapon with an enemy next to it: it readies another
            // weapon, and its turn ends (AiMoveAttack).
            if (missile_in_hand(f) && !f.numbers.missile->throwable && enemy_next_to(f))
            {
                equip_best(f);
                finish_round(f);
                return true;
            }
            // The original struck a pick next to it even one it may not single
            // out (an obvious bug, fixed).
            if (!can_target(f, pick))
            {
                continue;
            }
            target = &pick;
        }
        const auto verb = attack_verb(f, *target);
        if (!verb)
        {
            finish_round(f);
            return true;
        }
        note_attack(f, *target);
        f.fought = true;
        f.routing = false;
        if (*verb == "melee")
        {
            if (!sweep(f, *target))
            {
                make_attack(f, *target, false);
            }
        }
        else
        {
            shoot(f, *target);
        }
        if (f.attacks() <= 0 || f.initiative <= 0)
        {
            end_actions(f);
            return true;
        }
        if (!target->able())
        {
            return false;
        }
    }
    // The original ran on through the 21st pass (an obvious bug, fixed).
    finish_round(f);
    return true;
}

// A monster's own specials as its AI moves and attacks (combatai.pas,
// MONSTER_SPECIAL): the petrifying gaze, the charming gaze, the lightning
// breath and the ankheg's acid, at its lock.
void Session::monster_specials(Fighter &f)
{
    auto *lock = find(f.lock);
    if (f.has(83) && lock && lock->able())
    {
        // The petrifying gaze, at any range; a mirror turns a gaze that can be
        // reflected back on the gazer (effhndlr.pas, EffPetrifyingGaze).
        log({"{name} gazes.", {{"name", f.name}}});
        Fighter *victim = lock;
        if (f.has(127) && lock->numbers.mirror)
        {
            log({"{name} reflects it.", {{"name", lock->name}}});
            victim = &f;
        }
        if (!saving_throw(*victim, 1, 0, &f))
        {
            victim->condition = Condition::stoned;
            victim->hit_points = 0;
            victim->initiative = 0;
            log({"{name} is stoned.", {{"name", victim->name}}});
            end_effects_at_zero(*victim);
            went_down(*victim);
        }
        if (!f.able())
        {
            return;
        }
    }
    lock = find(f.lock);
    // Effect 84 is the charming gaze in Pool of Radiance only: Curse's and
    // Gateway's shambling mounds carry it as their absorbing of electricity
    // (their effhndlr.pas), which charms nothing.
    if (f.has(84) && game_of(identity_) == GoldBoxGame::pool_of_radiance && lock && lock->able() &&
        can_target(f, *lock) && sees(board_, f, *lock))
    {
        // The charming gaze: saved against at -2. The original's reach was an
        // unset value; it reaches what the gazer sees here (an obvious bug, fixed).
        log({"{name} gazes.", {{"name", f.name}}});
        const bool saved = saving_throw(*lock, 4, -2, &f);
        if (add_with_message(*lock,
                             {effect::charm, 0, (f.side == Side::opposition ? 128 : 0) + 12, true},
                             true, saved, 0, 12, game_spell(10)))
        {
            charm(*lock, f.side);
        }
    }
    if (const auto *breath = f.find_effect(88); breath && (round_ == 1 || roll(100) > 50))
    {
        // The lightning breath: always in the first round, then half the time;
        // at an enemy within 4 squares and the dragon's magic-user level, for
        // its maximum Hit Points, saved against for half; 3, 2 or 1 breaths by
        // its strength (effhndlr.pas and magic.pas, EffBreatheLightning). With
        // no enemy in reach the original breathed on itself and spent the
        // breath (an obvious bug, fixed: it holds its breath).
        const int strength = breath->strength;
        const int reach =
            4 + f.numbers.class_levels[static_cast<std::size_t>(ClassSlot::magic_user)];
        if (acquire_target(f, reach, false, true))
        {
            log({"{name} breathes.", {{"name", f.name}}});
            const Cell aim = find(f.lock)->cell;
            const int sx = (aim.x > f.cell.x) - (aim.x < f.cell.x);
            const int sy = (aim.y > f.cell.y) - (aim.y < f.cell.y);
            const Cell first{f.cell.x + sx + (sx > 0 ? 1 : 0), f.cell.y + sy + (sy > 0 ? 1 : 0)};
            while (f.has(effect::invisible))
            {
                f.remove(effect::invisible);
            }
            const int damage = f.numbers.max_hit_points;
            lightning_at(first, damage, 12, 3);
            lightning_path(f.cell, first, 10, damage, 12, false, 3);
            // A breath that came back off a wall may have downed the dragon,
            // which loses the effect with its life.
            if (auto *left = f.find_effect(88); left && strength > 253)
            {
                left->strength = strength - 1;
            }
            else if (left)
            {
                f.remove(88);
            }
            end_actions(f);
            return;
        }
    }
    lock = find(f.lock);
    if (f.has(121) && roll(100) <= 25 && lock && lock->able() && distance(f, *lock) < 4)
    {
        // The ankheg's acid, once a fight: 8d4 acid, saved against for half
        // (effhndlr.pas, EffSpitAcid). The original left it untyped (an
        // obvious bug, fixed).
        end_actions(f);
        log({"{name} spits acid.", {{"name", f.name}}});
        int damage = 0;
        for (int die = 0; die < 8; ++die)
        {
            damage += roll(4);
        }
        nonweapon_damage(*lock, damage, 2, saving_throw(*lock, 3, 0, &f, 16), 16, 12, 0, false);
        f.remove(121);
        f.remove(80);
    }
    curse_specials(f);
    secret_specials(f);
}

// The target a Secret monster's power picks, as its AI aims a spell
// (cbtacts.pas, SelectSpellTarget, in its reconstruction): a new lock on an
// enemy it sees within range, in one try. A power aimed as a paralyzing spell
// is (its picker's spell 61) passes a helpless one over.
Fighter *Session::secret_target(Fighter &f, int range, bool skip_helpless)
{
    if (!acquire_target(f, range, false, true))
    {
        return nullptr;
    }
    auto *target = find(f.lock);
    return skip_helpless && target->helpless() ? nullptr : target;
}

// Secret's monsters' powers as each of their AI's turns starts (fxengine.pas,
// its condition 14, in its reconstruction), in its order: the petrifying gaze,
// a pyrohydra's fire, the umber hulk's confusing gaze, a hell hound's fire, a
// giant slug's acid and a dragon's breath. Only the breath ends its turn.
void Session::secret_specials(Fighter &f)
{
    if (f.has(effect::petrifying_gaze) && f.able())
    {
        petrifying_gaze(f);
    }
    // A pyrohydra breathes 8 of fire at its lock beside it, from the second
    // round for as many rounds as its first attack's rate (EffBreatheFireAdjacent).
    auto *lock = find(f.lock);
    const bool fire_round =
        round_ > 1 && static_cast<int>(round_) - 1 <= f.numbers.attacks[0].rate_per_two_rounds;
    if (f.has(effect::fire_beside) && f.able() && fire_round && lock && lock->able() &&
        distance(f, *lock) < 2)
    {
        log({"{name} breathes fire.", {{"name", f.name}}});
        nonweapon_damage(*lock, 8, 2, saving_throw(*lock, 3, 0, &f, 0x21), 0x21, 12, 0, false);
    }
    // The umber hulk's gaze confuses one beside it for 1d10+2 rounds unless
    // it saves against spells (EffReflectSpell).
    if (f.has(effect::confusing_gaze) && f.able())
    {
        if (auto *target = secret_target(f, 1, false))
        {
            log({"{name} gazes.", {{"name", f.name}}});
            if (!saving_throw(*target, 4, 0, &f))
            {
                (void)add_with_message(*target, {235, roll(10) + 2, f.numbers.level, true}, true,
                                       false, 0, f.numbers.level, game_spell(182));
            }
        }
    }
    // A hell hound breathes 7 of fire at one beside it, every turn
    // (EffBreatheFireAtTarget).
    if (f.has(effect::fire_at_target) && f.able())
    {
        if (auto *target = secret_target(f, 1, false))
        {
            log({"{name} breathes fire.", {{"name", f.name}}});
            nonweapon_damage(*target, 7, 2, saving_throw(*target, 3, 0, &f, 0x21), 0x21, 12, 0,
                             false);
        }
    }
    // A giant slug spits 4d8 of acid at one within 6 squares: 1 time in 10 in
    // the first round, then 1 in 10 for each square nearer than 11 (EffSpitAtTarget).
    if (f.has(effect::spits_acid) && f.able())
    {
        if (auto *target = secret_target(f, 6, true))
        {
            const int chance = round_ == 1 ? 10 : (11 - distance(f, *target)) * 10;
            if (roll(100) <= chance)
            {
                log({"{name} spits acid.", {{"name", f.name}}});
                int damage = 0;
                for (int die = 0; die < 4; ++die)
                {
                    damage += roll(8);
                }
                nonweapon_damage(*target, damage, 2, saving_throw(*target, 3, 0, &f, 16), 16, 12, 0,
                                 false);
            }
            else
            {
                log({"{name} spits acid and misses.", {{"name", f.name}}});
            }
        }
    }
    // A white dragon breathes cold 7 squares long, a red one fire 9 long
    // (magic.pas, EffDragonBreath).
    if (f.has(effect::cold_breath) && f.able() && f.initiative > 0)
    {
        (void)secret_breath(f, effect::cold_breath, 7, 0x22);
    }
    if (f.has(effect::fire_breath) && f.able() && f.initiative > 0)
    {
        (void)secret_breath(f, effect::fire_breath, 9, 0x21);
    }
}

// A Secret gaze that turns to stone (effhndlr.pas, EffPetrifyingGaze, in its
// reconstruction): at an enemy within 6 squares that is not helpless, saved
// against as petrification. A target with a mirror turns a gaze that can be
// reflected back on the gazer.
void Session::petrifying_gaze(Fighter &gazer)
{
    auto *target = secret_target(gazer, 6, true);
    if (!target)
    {
        return;
    }
    log({"{name} gazes.", {{"name", gazer.name}}});
    Fighter *victim = target;
    if (gazer.has(effect::reflectable_gaze) && target->numbers.mirror)
    {
        log({"{name} reflects it.", {{"name", target->name}}});
        victim = &gazer;
    }
    if (!saving_throw(*victim, 1, 0, &gazer))
    {
        turn_to_stone(*victim);
    }
}

// A Secret dragon's breath (magic.pas, EffDragonBreath, in its reconstruction):
// three a fight, the effect's strength counting them down from the first
// round; aimed at an enemy within 6 squares that is not helpless, along a
// path of the length three wide, never while any creature of its own side
// stands in it; its maximum Hit Points to each, half on a save against
// breath. A breath ends its turn.
bool Session::secret_breath(Fighter &dragon, int code, int length, int element)
{
    auto *breath = dragon.find_effect(code);
    if (round_ == 1 || breath->strength > 3)
    {
        breath->strength = 3;
    }
    if (breath->strength <= 0)
    {
        return false;
    }
    const auto *target = secret_target(dragon, 6, true);
    if (!target)
    {
        return false;
    }
    const auto targets = path_area(dragon, target->cell, length, 3);
    const bool spares_friends = std::ranges::none_of(targets,
                                                     [&](EntityId id)
                                                     {
                                                         return find(id)->side == dragon.side;
                                                     });
    if (targets.empty() || !spares_friends)
    {
        return false;
    }
    --breath->strength;
    log({"{name} breathes.", {{"name", dragon.name}}});
    for (const auto id : targets)
    {
        auto &victim = *find(id);
        nonweapon_damage(victim, dragon.numbers.max_hit_points, 2,
                         saving_throw(victim, 3, 0, &dragon, element), element, 12, 0, false);
    }
    end_actions(dragon);
    return true;
}

// A Secret dragon's fear as a fight starts (effhndlr.pas, EffDragonFear, in
// its reconstruction): each enemy below level 5 that is not fearless and not
// afraid already is terrified and runs for the rest of the fight.
void Session::dragon_fear(const Fighter &dragon)
{
    for (auto &f : fighters_)
    {
        if (f.side == dragon.side || !f.able() || f.has(effect::fearless) || f.has(effect::fear) ||
            f.numbers.level >= 5)
        {
            continue;
        }
        if (add_with_message(f, {effect::fear, 0, 255, true}, true, false, 0, dragon.numbers.level,
                             game_spell(184)))
        {
            f.turned = true;
        }
    }
}

// How many times a creature has used a power this fight, kept on it as an
// effect of the power's code plus 100.
int Session::uses(Fighter &f, int code)
{
    if (const auto *count = f.find_effect(code + 100))
    {
        return count->strength;
    }
    return 0;
}

void Session::spend_use(Fighter &f, int code)
{
    if (auto *count = f.find_effect(code + 100))
    {
        ++count->strength;
        return;
    }
    f.numbers.effects.push_back({code + 100, 0, 1, false});
}

// Curse's monsters' own powers at their turn (its reconstruction's ovr023):
// a breath of acid or fire along a path, a hell hound's fire, Tyranthraxus's
// lightning, a slug's acid, a paralyzing gaze and a beholder's rays.
void Session::curse_specials(Fighter &f)
{
    const auto breathe = [&](int code, int length, int width, int element, bool spare_friends)
    {
        if (!f.has(code) || uses(f, code) >= 3 || !acquire_target(f, 6, false, false))
        {
            return false;
        }
        const auto targets = path_area(f, find(f.lock)->cell, length, width);
        if (spare_friends && std::ranges::any_of(targets,
                                                 [&](EntityId id)
                                                 {
                                                     return find(id)->side == f.side;
                                                 }))
        {
            return false;
        }
        log({"{name} breathes.", {{"name", f.name}}});
        spend_use(f, code);
        for (const auto id : targets)
        {
            auto &target = *find(id);
            nonweapon_damage(target, f.numbers.max_hit_points, 2,
                             saving_throw(target, 3, 0, &f, element), element, 12, 0, false);
        }
        end_actions(f);
        return true;
    };
    // A black dragon breathes acid along 6 squares, never through its own;
    // a red one fire along 9, three wide (DragonBreathAcid, DragonBreathFire).
    if (breathe(290, 6, 1, 0x30, true) || breathe(328, 9, 3, 0x21, false))
    {
        return;
    }
    auto *lock = find(f.lock);
    // A hell hound breathes 7 of fire at one beside it, half the time.
    if (f.has(331) && acquire_target(f, 12, false, false) && (lock = find(f.lock)) &&
        distance(f, *lock) <= 1 && roll(100) <= 50)
    {
        log({"{name} breathes fire.", {{"name", f.name}}});
        nonweapon_damage(*lock, 7, 2, saving_throw(*lock, 3, 0, &f, 1), 0x21, 12, 0, false);
        end_actions(f);
        return;
    }
    // Tyranthraxus throws lightning in the first four rounds: 16d6 along
    // the bolt and again on its way back (cast_throw_lightening).
    if (f.has(332) && round_ <= 4 && acquire_target(f, 12, false, false))
    {
        const Cell aim = find(f.lock)->cell;
        while (f.has(effect::invisible))
        {
            f.remove(effect::invisible);
        }
        log({"{name} throws lightning.", {{"name", f.name}}});
        const auto bolt = [this]
        {
            int total = 0;
            for (int die = 0; die < 16; ++die)
            {
                total += roll(6);
            }
            return total;
        };
        lightning_at(aim, bolt(), 12);
        lightning_path(f.cell, aim, 8, bolt(), 12, true);
        end_actions(f);
        return;
    }
    // A giant slug spits acid as far as 6 squares, 3 times in 10, for its
    // Hit Points, saved against for half (AffectSpitAcid); Curse left it
    // untyped, as Pool of Radiance's ankheg (an obvious bug, fixed: acid).
    if (f.has(286) && acquire_target(f, 12, false, false) && (lock = find(f.lock)) &&
        distance(f, *lock) <= 6)
    {
        if (roll(100) <= 30)
        {
            log({"{name} spits acid.", {{"name", f.name}}});
            nonweapon_damage(*lock, f.numbers.max_hit_points, 2, saving_throw(*lock, 3, 0, &f, 16),
                             16, 12, 0, false);
        }
        else
        {
            log({"{name} spits acid and misses.", {{"name", f.name}}});
        }
    }
    // A paralyzing gaze: saved against as petrification, or held for an hour
    // (cast_gaze_paralyze).
    if (f.has(326) && acquire_target(f, 12, false, false) && (lock = find(f.lock)) &&
        sees(board_, f, *lock))
    {
        log({"{name} gazes.", {{"name", f.name}}});
        if (!saving_throw(*lock, 1, 0, &f))
        {
            log({"{name} is paralyzed.", {{"name", lock->name}}});
            lock->numbers.effects.push_back({effect::paralysis, 60, 12, false});
        }
    }
    if (f.has(287))
    {
        beholder_rays(f);
    }
}

// A beholder's eyes (Curse's attack_or_kill): up to four tries, each power
// once a fight, by the target's distance: a disintegrating ray at exactly 2
// squares, a petrifying one at 3, death at 4, a 2d8+1 wound at 5; then Fear,
// Slow and Sleep.
void Session::beholder_rays(Fighter &f)
{
    for (int tries = 0; tries < 4 && f.able(); ++tries)
    {
        if (!acquire_target(f, 12, false, false))
        {
            return;
        }
        auto &target = *find(f.lock);
        const int range = distance(f, target);
        const int used = uses(f, 287);
        const auto power = [&](int bit)
        {
            if (used & bit)
            {
                return false;
            }
            if (auto *count = f.find_effect(387))
            {
                count->strength |= bit;
            }
            else
            {
                f.numbers.effects.push_back({387, 0, bit, false});
            }
            return true;
        };
        if (range == 2 && power(1))
        {
            log({"{name} fires a disintegrating ray.", {{"name", f.name}}});
            if (!saving_throw(target, 3, 0, &f))
            {
                target.condition = Condition::gone;
                target.hit_points = 0;
                target.initiative = 0;
                log({"{name} is disintegrated.", {{"name", target.name}}});
                end_effects_at_zero(target);
                went_down(target);
            }
        }
        else if (range == 3 && power(2))
        {
            log({"{name} fires a petrifying ray.", {{"name", f.name}}});
            if (!saving_throw(target, 1, 0, &f))
            {
                target.condition = Condition::stoned;
                target.hit_points = 0;
                target.initiative = 0;
                log({"{name} is stoned.", {{"name", target.name}}});
                end_effects_at_zero(target);
                went_down(target);
            }
        }
        else if (range == 4 && power(4))
        {
            log({"{name} fires a death ray.", {{"name", f.name}}});
            if (!saving_throw(target, 0, 0, &f))
            {
                log({"{name} is slain.", {{"name", target.name}}});
                kill(target);
            }
        }
        else if (range == 5 && power(8))
        {
            log({"{name} fires a wounding ray.", {{"name", f.name}}});
            nonweapon_damage(target, roll(8) + roll(8) + 1, 0, false, 8, 12, 0, false);
        }
        else
        {
            for (const auto [bit, number] :
                 {std::pair{16, 184}, std::pair{32, 55}, std::pair{64, 21}})
            {
                if (power(bit))
                {
                    f.aiming = number;
                    f.using_item = -1;
                    resolve_spell(f, {target.id}, target.cell);
                    break;
                }
            }
        }
    }
}

// The original's approach patterns: what each pattern adds to the direction,
// in the order it tries them (combatai.pas, aiDirOffsets).
constexpr std::array<std::array<int, 5>, 6> approach{{
    {8, 7, 6, 1, 2},
    {8, 1, 2, 7, 6},
    {7, 1, 8, 6, 2},
    {1, 7, 8, 2, 6},
    {8, 7, 6, 5, 4},
    {8, 1, 2, 3, 4},
}};

// Whether it can step that way (combatai.pas, AiCanStep): onto the field,
// free of creatures, on ground it can pay for; a stinking cloud only for one
// that breathes it unharmed. Off the field, edge is set.
bool Session::can_step(const Fighter &f, int direction, bool &edge) const
{
    edge = false;
    const auto dx = step_x[static_cast<std::size_t>(direction)];
    const auto dy = step_y[static_cast<std::size_t>(direction)];
    auto moved = f;
    moved.cell = {f.cell.x + dx, f.cell.y + dy};
    for (const auto square : squares_of(moved))
    {
        if (!board_.contains(square))
        {
            edge = true;
            return false;
        }
    }
    // Each step is a stop: it may not end on another creature's square.
    const auto grid = grid_for(f);
    const auto cost = grid.step_cost(f.cell, moved.cell);
    if (!cost || *cost > f.half_moves || !grid.can_stop_at(moved.cell))
    {
        return false;
    }
    if (secret())
    {
        // A stinking cloud's save is rolled as the step is taken (ai_step).
        return cloud_balk(f, moved) != CloudBalk::always;
    }
    const bool breathes_freely = f.has(effect::animated) || f.has(effect::coughing) || f.has(111) ||
                                 f.has(125) || breathes_clouds_freely(f);
    return breathes_freely || !in_cloud(moved, CloudKind::stinking);
}

// Secret's AI balks at a step into a cloud (combatai.pas, AiCanStep): into a
// cloudkill when its movement is below 7, unless proof against poison and
// paralysis (its effect 99) or turned; into a stinking cloud unless it coughs
// already, a globe guards it (its effects 57 and 63), it is proof or turned,
// or it saves against paralysis.
Session::CloudBalk Session::cloud_balk(const Fighter &f, const Fighter &moved) const
{
    const auto game = game_of(identity_);
    const bool proof = f.has(canonical_effect(99, game));
    if (in_cloud(moved, CloudKind::deadly) && f.numbers.movement < 7 && !proof && !f.turned)
    {
        return CloudBalk::always;
    }
    const bool guarded = f.has(effect::coughing) || f.has(canonical_effect(57, game)) ||
                         f.has(canonical_effect(63, game)) || proof || f.turned;
    return in_cloud(moved, CloudKind::stinking) && !guarded ? CloudBalk::unless_saved
                                                            : CloudBalk::none;
}

// The first step of the shortest way, round walls and creatures, to a square
// beside the nearest enemy it may single out, when that step can be taken. A
// departure from the original, whose AI a wall could leave with no way to
// fight or get away for good, so a fight no side could end went on forever
// (docs/DEVIATIONS.md).
std::optional<Session::Route> Session::route_to_enemy(const Fighter &f) const
{
    constexpr int unlimited = 1'000'000;
    const auto reach = grid_for(f).reachable(unlimited);
    std::optional<int> best_cost;
    rules::Cell best{};
    EntityId target{};
    auto moved = f;
    for (int y = 0; y < board_.height; ++y)
    {
        for (int x = 0; x < board_.width; ++x)
        {
            moved.cell = {x, y};
            const auto cost = reach.cost_to(moved.cell);
            if (!cost || (best_cost && *cost >= *best_cost))
            {
                continue;
            }
            for (const auto &o : fighters_)
            {
                if (o.side != f.side && o.able() && can_target(f, o) && distance(moved, o) <= 1)
                {
                    best_cost = cost;
                    best = moved.cell;
                    target = o.id;
                    break;
                }
            }
        }
    }
    if (!best_cost)
    {
        return std::nullopt;
    }
    const auto path = reach.path_to(best);
    for (int direction = 0; direction < 8 && !path.empty(); ++direction)
    {
        const rules::Cell step{f.cell.x + step_x[static_cast<std::size_t>(direction)],
                               f.cell.y + step_y[static_cast<std::size_t>(direction)]};
        bool edge = false;
        if (step == path.front() && can_step(f, direction, edge))
        {
            return Route{direction, target};
        }
    }
    return std::nullopt;
}

// One step (combatai.pas, AiMoveStep): toward its lock, or away when
// fleeing, trying the pattern's five directions; a fleeing creature that
// finds the edge tries to get away. Stuck (no way, or turning back), it
// changes pattern, then drops its lock, then stops; a creature the computer
// plays that has not fought this turn takes the shortest way to an enemy
// instead of stopping (route_to_enemy). False when it cannot go on.
bool Session::ai_step(Fighter &f, int &last_direction, int &stuck)
{
    const bool lone_mage =
        f.numbers.class_levels[static_cast<std::size_t>(ClassSlot::magic_user)] > 0 &&
        std::count_if(f.numbers.class_levels.begin(), f.numbers.class_levels.end(),
                      [](int level)
                      {
                          return level > 0;
                      }) == 1;
    if (f.half_moves / 2 <= 0 || f.initiative <= 0 || (lone_mage && !f.fleeing))
    {
        finish_round(f);
        return false;
    }
    if (f.routing)
    {
        if (const auto route = route_to_enemy(f))
        {
            f.lock = route->target;
            return take_ai_step(f, route->direction, last_direction);
        }
        f.routing = false;
    }
    const Fighter *locked = f.fleeing ? nullptr : find(f.lock);
    int base = 0;
    if (f.fleeing)
    {
        // Away from its enemies; the original ran along the party's facing,
        // which the fight learns with step 7.
        f.stance = roll(2);
        int x = 0, y = 0, enemies = 0;
        for (const auto &o : fighters_)
        {
            if (o.side != f.side && o.able())
            {
                x += o.cell.x;
                y += o.cell.y;
                ++enemies;
            }
        }
        base = enemies ? direction_toward({x / enemies, y / enemies}, f.cell) : 0;
    }
    else if (locked)
    {
        base = direction_toward(f.cell, locked->cell);
    }
    int chosen = -1;
    const auto &pattern = approach[static_cast<std::size_t>(std::clamp(f.stance, 1, 6) - 1)];
    for (const int offset : pattern)
    {
        const int direction = (base + offset) % 8;
        bool edge = false;
        auto moved = f;
        moved.cell = {f.cell.x + step_x[static_cast<std::size_t>(direction)],
                      f.cell.y + step_y[static_cast<std::size_t>(direction)]};
        if (can_step(f, direction, edge) &&
            (!secret() || cloud_balk(f, moved) != CloudBalk::unless_saved || saving_throw(f, 0, 0)))
        {
            chosen = direction;
            break;
        }
        if (edge && f.fleeing)
        {
            attempt_flee(f);
            f.half_moves = 0;
            f.fleeing = false;
            return false;
        }
    }
    if (chosen < 0 || chosen == (last_direction + 4) % 8)
    {
        ++stuck;
        f.stance = f.stance % 6 + 1;
        if (stuck > 1)
        {
            f.lock = 0;
            if (stuck > 2)
            {
                // Its turn left it no way to fight or get away.
                const auto route =
                    f.numbers.morale >= non_player && !f.fought ? route_to_enemy(f) : std::nullopt;
                if (route)
                {
                    f.routing = true;
                    f.fleeing = false;
                    f.lock = route->target;
                    return take_ai_step(f, route->direction, last_direction);
                }
                f.half_moves = 0;
                return false;
            }
            if (!acquire_target(f, 255, true, false))
            {
                finish_round(f);
                return false;
            }
        }
    }
    if (chosen < 0)
    {
        return true;
    }
    return take_ai_step(f, chosen, last_direction);
}

// The step an AI chose; false when the step left it unable to go on.
bool Session::take_ai_step(Fighter &f, int direction, int &last_direction)
{
    last_direction = direction;
    step_to(f, {f.cell.x + step_x[static_cast<std::size_t>(direction)],
                f.cell.y + step_y[static_cast<std::size_t>(direction)]});
    if (!f.able())
    {
        end_actions(f);
        return false;
    }
    return true;
}

// An AI uses an item (combatai.pas, SelectItemToUse): 1d7 passes, the k-th
// taking powers of priority 8 - k or more; the first readied item (not a
// scroll) whose spell it decides on is used.
bool Session::ai_use_item(Fighter &f)
{
    const int passes = roll(7);
    const bool enemy_left = std::any_of(fighters_.begin(), fighters_.end(),
                                        [&f](const Fighter &o)
                                        {
                                            return o.side != f.side && o.able();
                                        });
    if (!f.item_use_allowed || !enemy_left)
    {
        return false;
    }
    for (int pass = 1; pass <= passes; ++pass)
    {
        for (const auto &item : f.numbers.items)
        {
            const int number = item.spells[0];
            if (item.scroll || !item.readied || !spell_built(number) ||
                !ai_decides(f, number, 8 - pass))
            {
                continue;
            }
            f.using_item = item.ordinal;
            log({"{name} uses an item.", {{"name", f.name}}});
            f.aiming = number;
            f.picked.clear();
            const auto &known = *game_spell(number);
            f.picks_left = (known.area & 0x0F) <= 7 ? (known.area & 3) + 1 : 1;
            if (!ai_aim(f))
            {
                f.aiming = f.picks_left = 0;
                f.using_item = -1;
            }
            return true;
        }
    }
    return false;
}

// An AI casts (combatai.pas, SelectSpellToCast): one whose morale counts (a
// monster or NPC); 1d7 passes, three random picks each among its memorized
// spells, the first it decides on cast as anyone casts it.
bool Session::ai_cast(Fighter &f, bool magic)
{
    const int passes = roll(7);
    const bool enemy_left = std::any_of(fighters_.begin(), fighters_.end(),
                                        [&f](const Fighter &o)
                                        {
                                            return o.side != f.side && o.able();
                                        });
    const auto &spells = f.numbers.spells;
    // A party member casts only with the party's Quick magic on (Curse's
    // reference card: ALT M); monsters and NPCs always may.
    if (!f.cast_allowed || silenced(f) || !enemy_left ||
        (f.numbers.morale < non_player && !magic) || spells.empty())
    {
        return false;
    }
    for (int pass = 1; pass <= passes; ++pass)
    {
        for (int pick = 0; pick < 3; ++pick)
        {
            const int number =
                spells[static_cast<std::size_t>(roll(static_cast<int>(spells.size())) - 1)];
            if (game_spell(number) && game_spell(number)->when != 0 && spell_built(number) &&
                ai_decides(f, number, 8 - pass))
            {
                begin_cast(f, number);
                if (f.aiming && !ai_aim(f))
                {
                    lose_spell(f);
                    end_actions(f);
                }
                return true;
            }
        }
    }
    return false;
}

// Whether the AI takes this spell (combatai.pas, DecideCastSpell): of high
// enough priority; one on itself always, Cure Light Wounds when someone needs
// it; one at an enemy when an enemy is in range and sight, and none of those
// has a friend within the spell's blast.
bool Session::ai_decides(const Fighter &f, int number, int least_priority) const
{
    const auto &known = *game_spell(number);
    if (known.ai_priority == 0 || known.ai_priority < least_priority)
    {
        return false;
    }
    if (number == 3)
    {
        return const_cast<Session *>(this)->cure_search(f) != nullptr;
    }
    if (!known.offensive)
    {
        return true;
    }
    const int range = spell_range(known, caster_level(f, known, game_of(identity_)));
    const auto targets = enemies_within(f, range, true);
    if (targets.empty())
    {
        return false;
    }
    if (known.ai_radius == 0)
    {
        return true;
    }
    return std::none_of(targets.begin(), targets.end(),
                        [&](EntityId id)
                        {
                            const auto &t = *find(id);
                            return std::any_of(fighters_.begin(), fighters_.end(),
                                               [&](const Fighter &o)
                                               {
                                                   return o.side == f.side && o.able() &&
                                                          distance(o, t) <= known.ai_radius;
                                               });
                        });
}

// Whom an AI heals (combatai.pas, the cure search): within a square, the
// wounded of its side with the fewest Hit Points (itself below half), else,
// unless that one has fewer than 8, one downed beside it.
Fighter *Session::cure_search(const Fighter &f)
{
    Fighter *best = nullptr;
    for (auto &o : fighters_)
    {
        if (o.side != f.side || !o.able() || distance(f, o) > 1 ||
            o.hit_points >= o.numbers.max_hit_points ||
            (o.id == f.id && o.hit_points * 2 >= o.numbers.max_hit_points))
        {
            continue;
        }
        if (!best || o.hit_points < best->hit_points)
        {
            best = &o;
        }
    }
    if (best && best->hit_points < 8)
    {
        return best;
    }
    for (auto &o : fighters_)
    {
        if (o.side == f.side && o.downed() && distance(f, o) <= 1)
        {
            return &o;
        }
    }
    return best;
}

// The AI aims what it casts or uses (cbtacts.pas, SelectCombatTargets): on
// itself, or Cure Light Wounds on whom the cure search finds, or at random
// enemies in range and sight, not one with a friend within the blast nor a
// helpless one for a spell that makes helpless. False when it finds none.
bool Session::ai_aim(Fighter &f)
{
    const auto &known = *game_spell(f.aiming);
    const int area = known.area & 0x0F;
    if (f.aiming == 3)
    {
        auto *patient = cure_search(f);
        if (!patient)
        {
            return false;
        }
        resolve_spell(f, {patient->id}, patient->cell);
        return true;
    }
    if (!known.offensive)
    {
        resolve_spell(
            f, area >= 8 && area <= 14 ? within(f.cell, area & 7) : std::vector<EntityId>{f.id},
            f.cell);
        return true;
    }
    const int range = spell_range(known, caster_level(f, known, game_of(identity_)));
    const bool makes_helpless = std::find(helpless_effects.begin(), helpless_effects.end(),
                                          known.effect) != helpless_effects.end();
    std::vector<EntityId> picked;
    const int picks = area <= 7 ? (area & 3) + 1 : 1;
    for (int n = 0; n < picks; ++n)
    {
        for (int tries = 0; tries < 20; ++tries)
        {
            if (!acquire_target(f, range, false, true))
            {
                break;
            }
            const auto &pick = *find(f.lock);
            const bool crowded =
                known.ai_radius > 0 && std::any_of(fighters_.begin(), fighters_.end(),
                                                   [&](const Fighter &o)
                                                   {
                                                       return o.side == f.side && o.able() &&
                                                              distance(o, pick) <= known.ai_radius;
                                                   });
            if (crowded || (makes_helpless && pick.helpless()))
            {
                continue;
            }
            if (std::find(picked.begin(), picked.end(), pick.id) == picked.end())
            {
                picked.push_back(pick.id);
            }
            break;
        }
    }
    if (picked.empty())
    {
        log({"{name} aborts the spell.", {{"name", f.name}}});
        return false;
    }
    const auto aim = find(picked.front())->cell;
    resolve_spell(f, area >= 8 && area <= 14 ? within(aim, area & 7) : picked, aim);
    return true;
}

// The roll a cleric needs to turn undead, by the undead's level (1 skeleton to
// 10 vampire) and the cleric's level (1 to 8, 9 to 13, 14 up): above 0 it
// turns, 0 destroys, below 0 destroys on that much or more, 99 never
// (cbtacts.pas, CombatTurnUndead).
constexpr std::array<std::array<int, 10>, 10> turning_table{{
    {10, 7, 4, 1, 1, 0, 0, -1, -1, -1},
    {13, 10, 7, 1, 1, 0, 0, 0, -1, -1},
    {16, 13, 10, 4, 1, 1, 0, 0, 0, -1},
    {19, 16, 13, 7, 4, 1, 1, 0, 0, -1},
    {20, 19, 16, 10, 7, 4, 1, 1, 0, 0},
    {99, 20, 19, 13, 10, 7, 4, 1, 1, 0},
    {99, 99, 20, 16, 13, 10, 7, 4, 1, 0},
    {99, 99, 99, 20, 16, 13, 10, 7, 4, 1},
    {99, 99, 99, 99, 20, 16, 13, 10, 7, 1},
    {99, 99, 99, 99, 99, 20, 16, 13, 10, 4},
}};

// Turning undead, once a fight (cbtacts.pas, CombatTurnUndead): one d20 for
// the whole attempt against up to 1d12 undead enemies in sight, the lowest
// first and the nearest of those; each it beats is turned, or destroyed, and
// the first it fails ends the attempt. Destruction goes on to at least six.
// The cleric's actions end.
void Session::turn_undead(Fighter &cleric)
{
    cleric.tried_turning = true;
    log({"{name} turns undead.", {{"name", cleric.name}}});
    const int level = cleric.numbers.class_levels[static_cast<std::size_t>(ClassSlot::cleric)];
    const auto column = static_cast<std::size_t>(level <= 8 ? level - 1 : level <= 13 ? 8 : 9);
    int budget = roll(12);
    int at_least = 6;
    const int natural = roll(20);
    bool any = false;
    while (budget > 0)
    {
        Fighter *target = nullptr;
        for (auto &f : fighters_)
        {
            if (f.side == cleric.side || !f.able() || f.turned || f.numbers.undead_level < 1 ||
                f.numbers.undead_level > 10 || !sees(board_, cleric, f))
            {
                continue;
            }
            if (!target || f.numbers.undead_level < target->numbers.undead_level ||
                (f.numbers.undead_level == target->numbers.undead_level &&
                 distance(cleric, f) < distance(cleric, *target)))
            {
                target = &f;
            }
        }
        if (!target)
        {
            break;
        }
        const int need =
            turning_table[static_cast<std::size_t>(target->numbers.undead_level - 1)][column];
        if (natural < std::abs(need))
        {
            break;
        }
        any = true;
        if (need > 0)
        {
            target->turned = true;
            log({"{name} is turned.", {{"name", target->name}}});
        }
        else
        {
            target->condition = Condition::gone;
            target->hit_points = 0;
            target->initiative = 0;
            log({"{name} is destroyed.", {{"name", target->name}}});
            end_effects_at_zero(*target);
        }
        at_least = std::max(at_least - 1, 0);
        --budget;
        if (budget == 0 && at_least > 0 && need < 0)
        {
            budget = 1;
        }
    }
    if (!any)
    {
        log({"Nothing happens.", {}});
    }
    end_actions(cleric);
}

// Killed outright, whatever its Hit Points.
void Session::kill(Fighter &victim, std::string_view message)
{
    victim.condition = Condition::dead;
    victim.hit_points = 0;
    victim.initiative = 0;
    log({std::string(message), {{"name", victim.name}}});
    end_effects_at_zero(victim);
    went_down(victim);
}

// Both claws hit: two rakes of 2d4 with the rear claws, then its actions end
// (cbtacts.pas, RearClawRake).
void Session::rake(Fighter &attacker, Fighter &victim)
{
    auto &rear = attacker.numbers.attacks[1];
    const auto own = rear;
    rear = {own.rate_per_two_rounds, 2, 4, own.modifier};
    attacker.attacks_left = {0, 2};
    attacker.attack_phase = 1;
    log({"{attacker} rakes {name}.", {{"attacker", attacker.name}, {"name", victim.name}}});
    make_attack(attacker, victim, false);
    rear = own;
    attacker.attacks_left = {0, 0};
    end_actions(attacker);
}

// Curse's conditions at a turn's start (its reconstruction's ovr013): one
// entangled cannot move; a feebleminded one cannot cast; one fighting snakes
// spends its turn on them until its attacks wear them out; a confused one
// rolls what it does, then may shake it off.
void Session::curse_turn_start(Fighter &f)
{
    if (!f.able())
    {
        return;
    }
    if (f.has(336))
    {
        f.half_moves = 0;
    }
    if (f.has(268))
    {
        f.cast_allowed = false;
        f.item_use_allowed = false;
    }
    if (auto *snakes = f.find_effect(203))
    {
        log({"{name} is fighting with snakes.", {{"name", f.name}}});
        snakes->strength -= f.attacks_left[0] + f.attacks_left[1];
        if (snakes->strength <= 0)
        {
            f.remove(203);
        }
        end_actions(f);
        return;
    }
    if (f.has(235))
    {
        const int roll_100 = roll(100);
        if (roll_100 <= 10)
        {
            // It breaks and runs, afraid for 10 rounds.
            f.remove(235);
            f.fleeing = true;
            f.numbers.effects.push_back({342, 10, 0, true});
            log({"{name} flees.", {{"name", f.name}}});
            return;
        }
        if (roll_100 <= 60)
        {
            log({"{name} is confused.", {{"name", f.name}}});
            end_actions(f);
        }
        else if (roll_100 <= 80)
        {
            // Berserk for a round: against the nearest creature, of either side.
            const Fighter *nearest = nullptr;
            for (const auto &other : fighters_)
            {
                if (other.id != f.id && other.able() &&
                    (!nearest || distance(f, other) < distance(f, *nearest)))
                {
                    nearest = &other;
                }
            }
            if (nearest)
            {
                log({"{name} goes berserk.", {{"name", f.name}}});
                f.own_side = f.has(effect::charm) ? f.own_side : f.side;
                f.side = nearest->side == Side::party ? Side::opposition : Side::party;
                f.lock = nearest->id;
                f.numbers.effects.push_back({337, 1, 0, true});
            }
        }
        else
        {
            log({"{name} is enraged.", {{"name", f.name}}});
        }
        if (saving_throw(f, 4, -2))
        {
            f.remove(235);
        }
    }
}

// At its turn's start a creature holding a victim bites it from behind with
// 3d4, and a stirge drinks 1d4 from its victim; either spends its turn. A
// stirge that has drunk its fill lets go and flies off (effhndlr.pas, 74 and 75).
void Session::attached_turn(Fighter &f)
{
    // One engulfed suffocates when its count runs out (Curse, Suffocates).
    if (auto *air = f.find_effect(effect::suffocating); air && f.able() && --air->strength <= 0)
    {
        log({"{name} suffocates.", {{"name", f.name}}});
        f.remove(effect::suffocating);
        kill(f);
        return;
    }
    // A shambling mound crushes its engulfed victim with two blows of 2d8;
    // an owlbear squeezes its hugged one with one (Curse; its owlbear's
    // squeeze was never called, docs/ORIGINAL-RULES.md, Bugs).
    for (const auto [code, blows] :
         {std::pair{effect::engulfing, 2}, std::pair{effect::hugging, 1}})
    {
        const auto *hold = f.find_effect(code);
        if (!hold)
        {
            continue;
        }
        auto *victim = find(static_cast<EntityId>(hold->strength));
        if (!f.able() || !victim || !victim->able())
        {
            release(f, code);
            continue;
        }
        auto &blow = f.numbers.attacks[0];
        const auto own = blow;
        blow = {own.rate_per_two_rounds, 2, 8, 0};
        f.attacks_left = {blows, 0};
        f.attack_phase = 0;
        make_attack(f, *victim, false);
        blow = own;
        end_actions(f);
        if (!victim->able())
        {
            release(f, code);
        }
        return;
    }
    if (const auto *hold = f.find_effect(effect::bite_hold))
    {
        auto *victim = find(static_cast<EntityId>(hold->strength));
        if (!f.able() || !victim || !victim->able())
        {
            release(f, effect::bite_hold);
        }
        else
        {
            auto &bite = f.numbers.attacks[0];
            const auto own = bite;
            bite = {own.rate_per_two_rounds, 3, 4, own.modifier};
            f.attacks_left = {1, 0};
            f.attack_phase = 0;
            make_attack(f, *victim, true);
            bite = own;
            end_actions(f);
            if (!victim->able())
            {
                release(f, effect::bite_hold);
            }
        }
    }
    if (const auto *drain = f.find_effect(effect::blood_drain))
    {
        auto *victim = find(static_cast<EntityId>(drain->strength));
        if (!f.able() || !victim || !victim->able())
        {
            release(f, effect::blood_drain);
            return;
        }
        const int damage = roll(4);
        log({"{attacker} drinks {name}'s blood for {damage}.",
             {{"attacker", f.name}, {"name", victim->name}, {"damage", std::to_string(damage)}}});
        inflict(*victim, damage);
        end_actions(f);
        auto *left = f.find_effect(effect::blood_drain);
        if (left && victim->able() && left->minutes / 16 > damage)
        {
            left->minutes -= damage * 16;
            return;
        }
        release(f, effect::blood_drain);
        if (f.able() && victim->able())
        {
            f.turned = true;
        }
    }
}

// An attack on a helpless creature is one blow of its Hit Points and 5 more,
// which leaves it dying 5 below 0; the attacker's attacks are spent
// (cbtacts.pas, RollAttacks).
void Session::slay_helpless(Fighter &attacker, Fighter &defender)
{
    log({"{attacker} slays the helpless {name}.",
         {{"attacker", attacker.name}, {"name", defender.name}}});
    inflict(defender, defender.hit_points + 5);
    attacker.attacks_left = {0, 0};
    attacker.sweep_budget = 0;
    end_actions(attacker);
}

// A round is a minute: every creature's timed effects age by one, and those
// with a minute or less left end (fxengine.pas, the clock's advance).
void Session::age_effects()
{
    for (auto &f : fighters_)
    {
        auto &effects = f.numbers.effects;
        std::vector<Effect> ended;
        std::erase_if(effects,
                      [&ended](const Effect &e)
                      {
                          if (e.minutes == 1)
                          {
                              ended.push_back(e);
                          }
                          return e.minutes == 1;
                      });
        for (auto &e : effects)
        {
            e.minutes = e.minutes > 1 ? e.minutes - 1 : e.minutes;
        }
        for (const auto &e : ended)
        {
            expire(f, e);
        }
    }
}

// An effect whose time ran out, by its handler (effhndlr.pas): a troll's
// regeneration starts 3 rounds after its first wound; one fighting on past
// its death falls dead; a troll comes back with all its Hit Points when its
// square is free, or tries again a minute later.
void Session::expire(Fighter &f, const Effect &ended)
{
    switch (ended.code)
    {
    case effect::delayed_blast:
        delayed_blast_goes_off(f, ended.strength);
        break;
    case effect::enlarged:
    case effect::strengthened:
        recount_strength(f);
        break;
    case 342:
        // Fear ends: it stops fleeing and is the player's again (Curse, AffectFear).
        f.fleeing = false;
        break;
    case 337:
        // Berserk ends: it is back on its own side.
        f.side = f.own_side;
        break;
    case effect::regenerating_soon:
        f.numbers.effects.push_back({effect::regeneration, 0, 0, false});
        break;
    case effect::fighting_on:
        if (f.able())
        {
            f.condition = Condition::dead;
            f.hit_points = 0;
            f.initiative = 0;
            log({"{name} falls dead.", {{"name", f.name}}});
        }
        break;
    case effect::slow_poison_tick:
        // Slow Poison costs a point every 10 minutes, never the last
        // (effhndlr.pas, EffSlowPoisonHit).
        if (f.hit_points > 1)
        {
            --f.hit_points;
        }
        f.numbers.effects.push_back({effect::slow_poison_tick, 10, 255, true});
        break;
    case effect::slow_poison:
        // Still poisoned when it wears off, it dies (EffSlowPoisonEnd).
        f.remove(effect::slow_poison_tick);
        if (f.has(effect::poisoned) && f.condition != Condition::dead)
        {
            log({"{name} dies from poison.", {{"name", f.name}}});
            kill(f);
        }
        break;
    case effect::revival:
        revive(f);
        break;
    case effect::stinking_cloud:
    case effect::cloudkill:
    {
        // The caster's cloud of this kind and number clears (effhndlr.pas,
        // EffClearStinkingCloud; Curse's EffClearCloudkill).
        const auto kind = ended.code == effect::cloudkill ? CloudKind::deadly : CloudKind::stinking;
        const auto cloud = std::find_if(clouds_.begin(), clouds_.end(),
                                        [&](const Cloud &c)
                                        {
                                            return c.caster == f.id && c.kind == kind &&
                                                   c.index == ended.strength / 16;
                                        });
        if (cloud != clouds_.end())
        {
            clouds_.erase(cloud);
            log({"The air clears a little.", {}});
        }
        break;
    }
    case effect::troll_return:
        if (f.condition == Condition::okay)
        {
            break;
        }
        if (cell_free(f))
        {
            stand_up(f, f.numbers.max_hit_points, "{name} rises again.");
        }
        else
        {
            f.numbers.effects.push_back({effect::troll_return, 1, 0, true});
        }
        break;
    default:
        break;
    }
}

// At a fight's start (combatst.pas, COMBAT_START): one that fights on while
// down may stand; a mummy's fear grips its enemies once; displacement is
// ready; camouflage and a ring of invisibility hide; a dragon frightens.
void Session::combat_start()
{
    bool feared = false;
    for (auto &f : fighters_)
    {
        // What its lasting effects make of its Strength (a Strength spell cast
        // in camp, a disease's wasting).
        if (f.has(effect::enlarged) || f.has(effect::strengthened) ||
            f.has(effect::strength_wasted))
        {
            recount_strength(f);
        }
        // Curse's alias starts every fight invisible (add_affect_19).
        if (f.has(338) && !f.has(effect::invisible))
        {
            f.numbers.effects.push_back({effect::invisible, 255, 255, false});
        }
        if (f.has(effect::fights_while_down))
        {
            went_down(f);
        }
        if (f.has(effect::mummy_fear) && !feared)
        {
            feared = true;
            // The first mummy's fear is the only one: every mummy loses its own.
            for (auto &other : fighters_)
            {
                other.remove(effect::mummy_fear);
            }
            const auto enemies = std::count_if(fighters_.begin(), fighters_.end(),
                                               [&f](const Fighter &other)
                                               {
                                                   return other.side != f.side;
                                               });
            for (auto &other : fighters_)
            {
                if (other.side == f.side || !other.able())
                {
                    continue;
                }
                // Paralysis by fear: saved against with +1 for a side of 6
                // or more and +2 for a human; immunity to paralysis spares
                // (effhndlr.pas, EffMummyFear).
                const int modifier = (enemies >= 6 ? 1 : 0) + (other.numbers.race == 7 ? 2 : 0);
                if (other.has(109) || other.has(111) || other.has(125) ||
                    saving_throw(other, 0, modifier))
                {
                    log({"{name} is unaffected.", {{"name", other.name}}});
                    continue;
                }
                other.numbers.effects.push_back({effect::paralysis, roll(4), 12, false});
                log({"{name} is paralyzed with fear.", {{"name", other.name}}});
            }
        }
        if (auto *displacement = f.find_effect(effect::displacement))
        {
            displacement->strength &= ~effect::displaced;
        }
        if (f.has(effect::camouflage) && roll(100) <= 95)
        {
            f.numbers.effects.push_back({effect::invisible, 1, 12, false});
        }
        // A ring of invisibility hides its wearer for the round (effhndlr.pas,
        // EffRenewInvisibility): Secret's Dreadlord starts every fight unseen.
        if (f.has(effect::renew_invisibility))
        {
            f.numbers.effects.push_back({effect::invisible, 1, 12, false});
        }
        // A Secret dragon's fear grips the weaker of its enemies (EffDragonFear).
        if (f.has(effect::dragon_fear) && f.able())
        {
            dragon_fear(f);
        }
    }
}

// At a round's end, after effects age (combat.pas, EndOfRound): regeneration
// heals 3; camouflage hides again, 95 times in 100, and a ring of invisibility
// every time; displacement is ready for the next round.
void Session::round_end_effects()
{
    for (auto &f : fighters_)
    {
        if (f.has(effect::regeneration))
        {
            f.hit_points = std::min(f.hit_points + 3, f.numbers.max_hit_points);
        }
        if (f.has(effect::camouflage) && roll(100) <= 95)
        {
            f.numbers.effects.push_back({effect::invisible, 1, 12, false});
        }
        if (f.has(effect::renew_invisibility))
        {
            f.numbers.effects.push_back({effect::invisible, 1, 12, false});
        }
        if (auto *displacement = f.find_effect(effect::displacement))
        {
            displacement->strength &= ~effect::displaced;
        }
    }
}

// A creature just gone down (fxengine.pas, HIT_POINTS_ZERO): one that fights
// on while down stands again, dying less than 6 below 0 or unconscious, with
// 6 less its wounds, for 1d4+1 rounds; a troll downed by anything but fire or
// acid (any weapon) comes back in 3d6 rounds.
void Session::went_down(Fighter &f)
{
    if (f.has(effect::fights_while_down) &&
        ((f.condition == Condition::dying && f.overkill < 6) ||
         f.condition == Condition::unconscious) &&
        cell_free(f))
    {
        const int hit_points = f.condition == Condition::dying ? 6 - f.overkill : 6;
        f.remove(effect::fights_while_down);
        stand_up(f, hit_points, "{name} gets back up.");
        f.numbers.effects.push_back({effect::fighting_on, roll(4) + 1, 0, true});
        return;
    }
    if (f.has(effect::troll_burn) && !f.able() && !f.has(effect::troll_return))
    {
        f.numbers.effects.push_back({effect::troll_return, roll(6) + roll(6) + roll(6), 0, true});
    }
}

bool Session::cell_free(const Fighter &f) const
{
    const auto mine = squares_of(f);
    return std::none_of(fighters_.begin(), fighters_.end(),
                        [&](const Fighter &other)
                        {
                            if (other.id == f.id || other.condition == Condition::dead ||
                                other.condition == Condition::fled ||
                                other.condition == Condition::surrendered ||
                                other.condition == Condition::gone)
                            {
                                return false;
                            }
                            const auto theirs = squares_of(other);
                            return std::any_of(mine.begin(), mine.end(),
                                               [&theirs](Cell c)
                                               {
                                                   return std::find(theirs.begin(), theirs.end(),
                                                                    c) != theirs.end();
                                               });
                        });
}

// Back on its feet, without turns this round (its initiative is spent).
void Session::stand_up(Fighter &f, int hit_points, std::string_view message)
{
    f.condition = Condition::okay;
    f.hit_points = hit_points;
    f.overkill = 0;
    f.initiative = 0;
    log({std::string(message), {{"name", f.name}}});
}

// Whether the attacker may single out the target (cbtacts.pas, CanTarget): not
// an invisible one unless it sees the invisible, not a blinking one before it
// has acted this round, and nothing that holds up a mirror or holy symbol to a
// gazing monster.
bool Session::can_target(const Fighter &attacker, const Fighter &target) const
{
    if (target.has(effect::invisible) && !attacker.has(effect::see_invisible))
    {
        return false;
    }
    if (target.has(effect::blink) && target.initiative > 0)
    {
        return false;
    }
    // Curse's blink hides it once it has acted (AffectBlink in its reconstruction).
    if (target.has(237) && target.initiative <= 0)
    {
        return false;
    }
    // Invisibility to animals: an animal that cannot see the invisible
    // cannot single it out (Curse, AffectInvisToAnimals).
    if (target.has(269) && attacker.numbers.creature_type == 19 &&
        !attacker.has(effect::see_invisible))
    {
        return false;
    }
    // Dust of Disappearance hides it even from one that sees the invisible
    // (effhndlr.pas, EffDustDisappear).
    if (target.has(effect::dust))
    {
        return false;
    }
    return !(attacker.has(effect::gaze_block) && target.numbers.wards_gaze);
}

// An attack roll a defender turns aside, even a natural 20 (fxengine.pas,
// ATTACK_ROLL_DEF): a blinking one before it has acted this round, and a
// displaced one the first attack each round. The original readied
// displacement only at the fight's start, so it turned aside one attack a
// fight; once a round here, as its own notes say (an obvious bug, fixed).
bool Session::turned_aside(Fighter &defender)
{
    if ((defender.has(effect::blink) && defender.initiative > 0) ||
        (defender.has(237) && defender.initiative <= 0))
    {
        return true;
    }
    auto *displacement = defender.find_effect(effect::displacement);
    if (displacement && (displacement->strength & effect::displaced) == 0)
    {
        displacement->strength |= effect::displaced;
        return true;
    }
    return false;
}

// What the defender's effects make of a weapon's damage, in the original's
// order (effhndlr.pas, WEAPON_DAMAGE_DEFENDER): evading missiles and boulders
// from afar, a troll's regeneration beginning, halving by weapon class or
// magic, weapons that cannot harm it, and oil or holy water.
int Session::weapon_damage_defenses(const Fighter &attacker, Fighter &defender, int damage,
                                    int phase, int &primary_hits)
{
    const auto &striker = attacker.numbers.striker;
    const int plus = striker ? striker->plus : 0;
    const bool silver = striker && striker->silver;
    const int weapon_class = striker ? striker->weapon_class : 0;
    const bool piercing = (weapon_class & 0x01) != 0, bludgeoning = (weapon_class & 0x80) != 0;
    const auto evades = [&](int chance)
    {
        if (distance(attacker, defender) <= 1 || roll(100) > chance)
        {
            return false;
        }
        log({"{name} avoids it.", {{"name", defender.name}}});
        // The original took the hit from the primary attacks' count whatever
        // attack it was, wrapping below 0 (an obvious bug, fixed).
        if (phase == 0 && primary_hits > 0)
        {
            --primary_hits;
        }
        return true;
    };
    // Curse's rakshasa (its effect 130) struck by a banishing quarrel is gone
    // for good (effhndlr.pas, EffBanishByWeapon).
    constexpr int banished_by_quarrel = 330;
    if (defender.has(banished_by_quarrel) && striker && striker->banishing)
    {
        log({"{name} is banished.", {{"name", defender.name}}});
        put_out_of_play(defender, Condition::gone);
        return 0;
    }
    // Ray of Enfeeblement takes a quarter of the attacker's damage (effhndlr.pas, EffEnfeeble).
    if (attacker.has(effect::enfeebled))
    {
        damage -= damage / 4;
    }
    // Mirror images take blows too, as the manual says; the original let them
    // take only a spell's (an obvious bug, fixed).
    if (mirror_absorbs(defender))
    {
        damage = 0;
    }
    if (defender.has(104) && attacker.numbers.weapon_type >= 0 && evades(60))
    {
        damage = 0;
    }
    // Protection from Normal Missiles: no harm from a mundane weapon from afar
    // (effhndlr.pas, EffProtNormalMissiles).
    if (defender.has(effect::missile_protection) && striker && plus == 0 &&
        attacker.numbers.weapon_type >= 0 && evades(100))
    {
        damage = 0;
    }
    const int boulder = attacker.numbers.weapon_type;
    if (defender.has(120) && (boulder == 87 || boulder == 88) && evades(50))
    {
        damage = 0;
    }
    if (defender.has(effect::troll_heal) && !defender.has(effect::regeneration) &&
        !defender.has(effect::regenerating_soon))
    {
        defender.numbers.effects.push_back({effect::regenerating_soon, 3, 0, true});
    }
    if (defender.has(115) && striker && (weapon_class == 0 || piercing))
    {
        damage /= 2;
    }
    if (defender.has(116) && plus > 0)
    {
        damage /= 2;
    }
    if (defender.has(119) && plus == 0 && (attacker.numbers.race > 0 || attacker.numbers.level < 4))
    {
        damage = 0;
    }
    // Secret's: an iron golem takes harm only from a weapon of +3 or better,
    // and a giant slug none from a bludgeoning one (effhndlr.pas,
    // EffNeedMagicWeapon and EffHalfBash, in its reconstruction).
    if (defender.has(effect::needs_plus_three) && (!striker || plus < 3))
    {
        damage = 0;
    }
    if (defender.has(effect::bashing_proof) && striker && bludgeoning)
    {
        damage = 0;
    }
    if (defender.has(123))
    {
        damage = plus > 0 ? damage : silver ? damage / 2 : 0;
    }
    if (defender.has(96) && plus == 0 && !silver)
    {
        damage = 0;
    }
    if (defender.has(94) && striker && (piercing || bludgeoning))
    {
        damage /= 2;
    }
    // Curse's defenses (ovr013): normal weapons do nothing and +1 or +2
    // half (260); every blow does half (281); a piercing one 1 (285).
    if (defender.has(260))
    {
        damage = plus == 0 ? 0 : plus < 3 ? damage / 2 : damage;
    }
    if (defender.has(281))
    {
        damage /= 2;
    }
    if (defender.has(285) && weapon_class == 1)
    {
        damage = std::min(damage, 1);
    }
    // A ranger adds its level against giants (Curse, AffectRangerVsGiant); in
    // Secret against the creatures its flags name, from next to them
    // (EffRangerDamageBonus).
    const bool ranger_foe =
        secret() ? (defender.numbers.traits & trait::ranger_foe) && adjacent(attacker, defender)
                 : (defender.numbers.kin & kin::giant) != 0;
    if (attacker.has(334) && ranger_foe && damage > 0)
    {
        damage += attacker.numbers.class_levels[static_cast<std::size_t>(ClassSlot::ranger)];
    }
    // A salamander's heat adds 1d6 unless the target resists fire (303).
    if (attacker.has(303) && damage > 0 && !defender.has(effect::resist_fire) &&
        !defender.has(254) && !defender.has(61))
    {
        damage += roll(6);
    }
    // A flask of oil burns a mummy for 3d8; holy water burns the undead for 1d6+1.
    if (defender.has(122) && attacker.numbers.weapon_type == 86)
    {
        damage = roll(8) + roll(8) + roll(8);
    }
    if (defender.has(117) && attacker.numbers.weapon_type == 85)
    {
        damage = roll(6) + 1;
    }
    return damage;
}

// A saving throw (fxengine.pas, RollSavingThrow): a natural 1 fails and a
// natural 20 saves; otherwise the roll with the creature's save bonus and the
// modifier must reach its number for the save. A total below 0 fails, where
// the original's byte wrapped it to a success (an obvious bug, fixed).
bool Session::saving_throw(const Fighter &f, int save, int modifier, const Fighter *caster,
                           int element)
{
    const int natural = roll(20);
    if (natural == 1 || natural == 20)
    {
        return natural == 20;
    }
    // Immunity to paralysis and poison makes those saves (effhndlr.pas, 111 and 125).
    if (save == 0 && (f.has(111) || f.has(125)))
    {
        return true;
    }
    // The saver's effects (effhndlr.pas, SAVING_THROW): Constitution for a
    // dwarf, gnome or halfling against poison, spells and wands; Shield;
    // protection against a caster of that alignment; and resistance to the
    // cold or fire the spell carries.
    const int constitution = f.numbers.constitution;
    const int hardiness = constitution < 4     ? 0
                          : constitution <= 6  ? 1
                          : constitution <= 10 ? 2
                          : constitution <= 13 ? 3
                          : constitution <= 17 ? 4
                          : constitution <= 20 ? 5
                                               : 0;
    // Secret's 94 counts Constitution against all three (EffConPsnSave).
    const bool poison_spell_or_wand = save == 0 || save == 2 || save == 4;
    if ((f.has(90) && save == 0) || (f.has(97) && (save == 4 || save == 2)) ||
        (f.has(effect::hardy) && poison_spell_or_wand))
    {
        modifier += hardiness;
    }
    // Secret's luckstone saves 1 better (EffSaveBonus).
    modifier += f.has(effect::luck) ? 1 : 0;
    modifier += f.has(effect::shield) ? 1 : 0;
    modifier -= f.has(effect::blind) ? 4 : 0;
    modifier -= f.has(effect::bestow_curse) ? 4 : 0;
    modifier += prayer_bonus(f);
    if (caster)
    {
        const int alignment = caster->numbers.alignment;
        const bool evil = alignment == 2 || alignment == 5 || alignment == 8;
        const bool good = alignment == 0 || alignment == 3 || alignment == 6;
        if ((evil &&
             protected_from(f, effect::protection_from_evil, effect::protection_from_evil_aura)) ||
            (good &&
             protected_from(f, effect::protection_from_good, effect::protection_from_good_aura)))
        {
            modifier += 2;
        }
    }
    if (((element & 2) && f.has(effect::resist_cold)) ||
        ((element & 1) && f.has(effect::resist_fire)))
    {
        modifier += 3;
    }
    // Curse's fire shields: the hot one saves 2 better against cold, the
    // cold one against fire (HotFireShield, ColdFireShield).
    if (((element & 2) && f.has(250)) || ((element & 1) && f.has(254)))
    {
        modifier += 2;
    }
    const int total = natural + f.numbers.save_bonus + modifier;
    return total >= 0 && total >= f.numbers.saving_throws[static_cast<std::size_t>(save)];
}

// The opposition's Hit Points left, those of the creatures still fighting
// against the whole of every one's, rounded down to a step of 5.
void Session::update_hostile_hit_points()
{
    int left = 0, whole = 0;
    for (const auto &f : fighters_)
    {
        if (f.side != Side::opposition)
        {
            continue;
        }
        left += f.able() ? f.hit_points : 0;
        whole += f.numbers.max_hit_points;
    }
    if (whole > 0)
    {
        hostile_hit_points_percent_ = left * 20 / whole * 5;
    }
}

int Session::highest_enemy_movement(const Fighter &f) const
{
    int highest = 0;
    for (const auto &other : fighters_)
    {
        if (other.side != f.side && other.able())
        {
            highest = std::max(highest, movement_of(other));
        }
    }
    return highest;
}

// A creature whose morale counts checks it as its turn starts (combatai.pas,
// CheckMorale): it holds while its morale (twice the byte's low 7 bits, 0
// past 102) is at least the share of its Hit Points it has lost; an enemy of
// the party also holds while the opposition's Hit Points left are at least
// 100 less the encounter's morale. Broken, it runs when no enemy is faster,
// gives up when its Intelligence is 6 or more, and otherwise fights on. It
// may rally at its next turn. True when it surrendered.
bool Session::check_morale(Fighter &f)
{
    f.fleeing = false;
    release(f, effect::bite_hold);
    release(f, effect::blood_drain);
    if (f.turned)
    {
        f.fleeing = true;
        log({"{name} is forced to flee.", {{"name", f.name}}});
        return false;
    }
    if (f.numbers.morale < non_player)
    {
        return false;
    }
    // A charmed creature never breaks (effhndlr.pas, the charm at MORALE_CHECK).
    if (f.has(effect::charm))
    {
        return false;
    }
    int morale = static_cast<int>(f.numbers.morale & 0x7F) * 2;
    if (morale > 102)
    {
        morale = 0;
    }
    // Bless adds 5 and Curse takes 5, never below 0, which fails (EffBless, EffCurse).
    const auto blessed = [&f](int value)
    {
        value += f.has(effect::bless) ? 5 : 0;
        return f.has(effect::curse) ? std::max(value - 5, 0) : value;
    };
    morale = morale == 0 ? 0 : blessed(morale);
    if (morale != 0 && morale >= 100 - f.hit_points * 100 / f.numbers.max_hit_points)
    {
        return false;
    }
    const int group = hostile_hit_points_percent_ == 0 ? 0 : blessed(hostile_hit_points_percent_);
    if (group != 0 && group >= 100 - static_cast<int>(morale_threshold_) &&
        f.side == Side::opposition)
    {
        return false;
    }
    if (highest_enemy_movement(f) <= movement_of(f))
    {
        f.fleeing = true;
        log({"{name} flees in panic.", {{"name", f.name}}});
        return false;
    }
    if (f.numbers.intelligence > 5)
    {
        put_out_of_play(f, Condition::surrendered);
        log({"{name} surrenders.", {{"name", f.name}}});
        return true;
    }
    return false;
}

// Off the field (fxengine.pas, PutOutOfPlayWithStatus): one that gave up has
// no Hit Points left; one that got away keeps its own. Its actions end.
void Session::put_out_of_play(Fighter &f, Condition condition)
{
    f.condition = condition;
    if (condition != Condition::fled)
    {
        f.hit_points = 0;
    }
    f.fleeing = false;
    end_actions(f);
    end_effects_at_zero(f);
}

// Stepping off the field (cbtacts.pas, AttemptFlee): it gets away with no
// enemy in sight, or with more movement than the fastest enemy, or on a 1 in
// 2 when they tie. Its actions end either way.
void Session::attempt_flee(Fighter &f)
{
    const bool watched =
        std::any_of(fighters_.begin(), fighters_.end(),
                    [this, &f](const Fighter &other)
                    {
                        return other.side != f.side && other.able() && sees(board_, f, other);
                    });
    const int fastest = highest_enemy_movement(f);
    const int own = movement_of(f);
    const bool away = !watched || fastest < own || (fastest == own && roll(2) == 1);
    if (away)
    {
        put_out_of_play(f, Condition::fled);
        log({"{name} got away.", {{"name", f.name}}});
        if (escape_ends_fight_ && f.side == Side::party && f.numbers.morale < non_player)
        {
            end_fight_by_escape(f);
        }
        return;
    }
    log({"{name} cannot get away.", {{"name", f.name}}});
    end_actions(f);
}

// One member's escape ends Gateway's maze fights (cbtacts.pas, AttemptFlee in
// combat mode 1): the party leaves by the side it ran off, and the monsters
// left stay behind, worth nothing.
void Session::end_fight_by_escape(const Fighter &escaped)
{
    escape_side_ = escaped.cell.y <= 0                   ? 0U
                   : escaped.cell.x >= board_.width - 1  ? 1U
                   : escaped.cell.y >= board_.height - 1 ? 2U
                                                         : 3U;
    for (auto &f : fighters_)
    {
        if (f.side == Side::opposition && f.condition == Condition::okay)
        {
            put_out_of_play(f, Condition::fled);
        }
    }
    settle_outcome();
}

// The squares just off the field next to a creature on its edge.
std::vector<Cell> Session::off_field_steps(const Fighter &f) const
{
    std::vector<Cell> steps;
    for (const auto square : squares_of(f))
    {
        for (int direction = 0; direction < 8; ++direction)
        {
            const Cell step{square.x + step_x[static_cast<std::size_t>(direction)],
                            square.y + step_y[static_cast<std::size_t>(direction)]};
            if (!board_.contains(step) &&
                std::find(steps.begin(), steps.end(), step) == steps.end())
            {
                steps.push_back(step);
            }
        }
    }
    return steps;
}

// A side with no one left able to fight has lost. When the party wins, its
// dying members wake unconscious (postcbt.pas).
void Session::settle_outcome()
{
    const auto side_standing = [this](Side side)
    {
        return std::any_of(fighters_.begin(), fighters_.end(),
                           [side](const Fighter &f)
                           {
                               return f.side == side && f.able();
                           });
    };
    // With none of the party left fighting, it fled when one of its own
    // characters got away, and is defeated otherwise (postcbt.pas,
    // DetermineCombatResults).
    if (!side_standing(Side::party))
    {
        const bool got_away = std::any_of(fighters_.begin(), fighters_.end(),
                                          [](const Fighter &f)
                                          {
                                              return f.side == Side::party &&
                                                     f.condition == Condition::fled &&
                                                     f.numbers.morale < non_player;
                                          });
        outcome_ = got_away ? rules::Outcome::fled : rules::Outcome::defeat;
    }
    else if (!side_standing(Side::opposition))
    {
        outcome_ = rules::Outcome::victory;
        for (auto &f : fighters_)
        {
            if (f.side == Side::party && f.condition == Condition::dying)
            {
                f.condition = Condition::unconscious;
                f.overkill = 0;
            }
        }
    }
    if (outcome_ != rules::Outcome::ongoing)
    {
        actor_ = 0;
        log({outcome_ == rules::Outcome::victory ? "The party wins the fight."
             : outcome_ == rules::Outcome::fled  ? "The party has fled."
                                                 : "The party is defeated.",
             {}});
    }
}

rules::MovementGrid Session::grid_for(const Fighter &mover) const
{
    std::vector<rules::Occupant> occupants;
    for (const auto &f : fighters_)
    {
        if (f.id == mover.id || f.condition == Condition::dead || f.condition == Condition::fled ||
            f.condition == Condition::surrendered || f.condition == Condition::gone ||
            f.condition == Condition::stoned)
        {
            continue;
        }
        occupants.push_back({f.cell, f.side != mover.side, !f.able(), f.downed(), f.footprint});
    }
    return rules::MovementGrid(board_, mover.cell, occupants, false, false, false, mover.footprint,
                               half_moves);
}

namespace
{
// A scroll's spells not yet read, a bit for each of its record's spell bytes.
int unread_spells(const UsableItem &scroll)
{
    int bits = 0;
    for (std::size_t at = 0; at < scroll.spells.size(); ++at)
    {
        if (scroll.spells[at] != 0)
        {
            bits |= 1 << at;
        }
    }
    return bits;
}
} // namespace

rules::Snapshot Session::snapshot() const
{
    rules::Snapshot state;
    state.identity = identity_;
    state.revision = revision_;
    state.round = round_;
    state.actor = actor_;
    state.outcome = outcome_;
    state.escape_side = escape_side_;
    state.battlefield = board_;
    state.log_entries = log_;
    std::vector<const Fighter *> order;
    for (const auto &f : fighters_)
    {
        order.push_back(&f);
    }
    std::stable_sort(order.begin(), order.end(),
                     [](const Fighter *a, const Fighter *b)
                     {
                         return a->initiative > b->initiative;
                     });
    for (const auto *f : order)
    {
        rules::CombatantView view;
        view.id = f->id;
        view.name = f->name;
        view.definition = f->definition;
        view.side = f->side;
        view.cell = f->cell;
        view.footprint = f->footprint;
        view.facing_left = f->facing_left;
        view.hit_points = f->condition == Condition::dying ? -f->overkill : f->hit_points;
        view.max_hit_points = f->numbers.max_hit_points;
        view.armor_class = f->numbers.armor_class;
        view.initiative = f->initiative;
        // Half-moves are 5 feet each on the original's 10-foot squares.
        view.movement_feet = f->half_moves * 5 / 2;
        view.action = f->id == actor_;
        view.conscious = f->able();
        view.dead = f->condition == Condition::dead || f->condition == Condition::gone;
        view.status = f->condition == Condition::dead          ? "dead"
                      : f->condition == Condition::dying       ? "dying"
                      : f->condition == Condition::unconscious ? "unconscious"
                      : f->condition == Condition::fled        ? "fled"
                      : f->condition == Condition::surrendered ? "surrendered"
                      : f->condition == Condition::gone        ? "gone"
                      : f->condition == Condition::stoned      ? "stoned"
                      : f->helpless()                          ? "helpless"
                                                               : "";
        view.fled = f->condition == Condition::fled;
        view.surrendered = f->condition == Condition::surrendered;
        view.panicked = f->fleeing;
        // What outlasts the fight: its Hit Points (one that got away keeps
        // them), death, and its numbers as the fight left them (levels
        // drained, effects still running), for the game to keep.
        const bool keeps = f->condition == Condition::okay || f->condition == Condition::fled;
        // A statue stays one until Stone to Flesh (status PETRIFIED).
        view.persistent = {keeps ? f->hit_points : 0, view.dead, f->numbers.encode(),
                           f->condition == Condition::stoned ? std::string(stoned_description)
                                                             : std::string()};
        // What is left of a party member's items, for the game to trim its
        // inventory to: an item gone is 0, a scroll the bits of its spells
        // not read.
        if (f->record)
        {
            // What it carries as the fight has left it: each item whose
            // count changed, and the items readied when the fight changed
            // which.
            const auto held = held_now(*f);
            std::vector<std::pair<int, std::uint64_t>> readied;
            for (const auto &item : held)
            {
                if (!item.inventory_id)
                {
                    continue;
                }
                const int left = item.gone ? 0 : held_count(item.carried);
                if (left != item.start_count)
                {
                    view.item_uses_left.emplace_back(item.inventory_id,
                                                     static_cast<unsigned>(left));
                }
                if (!item.gone && item.carried.item.readied())
                {
                    readied.emplace_back(item.readied_order, item.inventory_id);
                }
            }
            if (f->readied_changed)
            {
                std::ranges::sort(readied);
                auto &ids = view.readied_items.emplace();
                for (const auto &[order, id] : readied)
                {
                    ids.push_back(id);
                }
            }
            state.combatants.push_back(std::move(view));
            continue;
        }
        for (const auto &[ordinal, id] : f->item_ids)
        {
            const auto &items = f->numbers.items;
            const auto usable = std::ranges::find(items, ordinal, &UsableItem::ordinal);
            const int left = usable == items.end() ? 0
                             : usable->scroll      ? unread_spells(*usable)
                             : usable->charges > 0 ? usable->charges
                                                   : std::max(usable->quantity, 1);
            view.item_uses_left.emplace_back(id, static_cast<unsigned>(left));
        }
        if (f->shot_item)
        {
            const auto &missile = f->numbers.missile;
            const int left = missile && missile->loaded ? std::max(missile->quantity, 1) : 0;
            view.item_uses_left.emplace_back(f->shot_item, static_cast<unsigned>(left));
        }
        state.combatants.push_back(std::move(view));
    }
    return state;
}

std::vector<rules::Command> Session::legal_commands() const
{
    std::vector<rules::Command> commands;
    const auto *actor = find(actor_);
    if (outcome_ != rules::Outcome::ongoing || !actor)
    {
        return commands;
    }
    if (actor->aiming)
    {
        return aiming_commands(*actor);
    }
    for (const auto &cell : movement_reach(actor_))
    {
        rules::Command move;
        move.revision = revision_;
        move.actor = actor_;
        move.verb = "move";
        move.destination = cell;
        commands.push_back(move);
    }
    if (actor->attacks() > 0)
    {
        for (const auto &f : fighters_)
        {
            // A creature down is out of the fight (charhlp.pas, InflictDamage
            // clears its enabled flag) and is no target.
            if (f.side == actor->side || !f.able())
            {
                continue;
            }
            if (auto verb = attack_verb(*actor, f))
            {
                rules::Command attack;
                attack.revision = revision_;
                attack.actor = actor_;
                attack.target = f.id;
                attack.verb = std::move(*verb);
                attack.intent = rules::Intent::hostile;
                commands.push_back(attack);
            }
        }
    }
    // The Done menu (combat.pas, DoneMenu): Guard, Delay, Bandage and ending its actions.
    const auto offer = [&commands, actor, this](std::string verb, EntityId target = 0)
    {
        rules::Command command;
        command.revision = revision_;
        command.actor = actor->id;
        command.verb = std::move(verb);
        command.target = target;
        commands.push_back(command);
    };
    // Cast, while it may and has a spell ready for a fight (combat.pas, CombatMenu).
    if (actor->cast_allowed && !silenced(*actor))
    {
        std::vector<int> offered_spells;
        for (const int number : actor->numbers.spells)
        {
            const auto *known = game_spell(number);
            if (known && known->when != 0 && spell_built(number) &&
                std::find(offered_spells.begin(), offered_spells.end(), number) ==
                    offered_spells.end())
            {
                offered_spells.push_back(number);
                rules::Command cast;
                cast.revision = revision_;
                cast.actor = actor->id;
                cast.verb = "cast";
                cast.item = static_cast<unsigned>(number);
                cast.label = std::string(known->name);
                commands.push_back(cast);
            }
        }
    }
    // Use, for a readied item with a spell or power for a fight; a scroll's
    // spells once it is known, or by Read Magic, or a cleric's scroll for a
    // cleric (viewchar.pas, UseItemMenu; magic.pas, the scroll).
    if (actor->item_use_allowed && !silenced(*actor))
    {
        const bool cleric =
            actor->numbers.class_levels[static_cast<std::size_t>(ClassSlot::cleric)] > 0;
        for (const auto &item : actor->numbers.items)
        {
            const bool readable = !item.scroll || item.identified ||
                                  actor->has(effect::read_magic) || (cleric && item.slot == 12);
            if (!item.readied || !readable)
            {
                continue;
            }
            for (const int number : item.spells)
            {
                const auto *known = game_spell(number);
                if (!known || known->when == 0 || !spell_built(number))
                {
                    continue;
                }
                rules::Command use;
                use.revision = revision_;
                use.actor = actor->id;
                use.verb = "use";
                use.item = static_cast<unsigned>(item.ordinal * 256 + number);
                use.label = std::string(known->name);
                commands.push_back(use);
                if (!item.scroll)
                {
                    break;
                }
            }
        }
    }
    // Ready, for an item a party member carries and may ready, and put away,
    // for one readied that is not cursed (viewchar.pas, ToggleReadyItem).
    if (actor->record)
    {
        const auto held = held_now(*actor);
        std::vector<CarriedItem> readied;
        for (const auto &item : held)
        {
            if (!item.gone && item.carried.item.readied())
            {
                readied.push_back(item.carried);
            }
        }
        for (std::size_t at = 0; at < held.size(); ++at)
        {
            const auto &item = held[at];
            const bool is_readied = item.carried.item.readied();
            if (item.gone || !item.inventory_id ||
                (is_readied ? item.carried.item.cursed_raw != 0
                            : readying_refusal(item.carried, readied, actor->record->item_limit,
                                               game_of(identity_))
                                  .has_value()))
            {
                continue;
            }
            rules::Command toggle;
            toggle.revision = revision_;
            toggle.actor = actor_;
            toggle.verb = is_readied ? "unready" : "ready";
            toggle.item = static_cast<unsigned>(at);
            toggle.label = item.name;
            commands.push_back(toggle);
        }
    }
    // Guard needs a weapon that strikes next to it (combat.pas, DoneMenu).
    if (!actor->numbers.missile || actor->numbers.missile->throwable)
    {
        offer("guard");
    }
    offer("delay");
    // Quick: the original's AI plays its turn (combat.pas, TurnOnAuto); a party
    // member's also with its spells, as with the party's Quick magic on.
    offer("quick");
    if (actor->numbers.morale < non_player)
    {
        offer("quick_magic");
    }
    // Turn, once a fight for one with cleric levels (combat.pas, CombatMenu).
    // Secret has no turning (its CombatMenu offers none).
    if (actor->numbers.class_levels[static_cast<std::size_t>(ClassSlot::cleric)] > 0 &&
        !actor->tried_turning && !secret())
    {
        offer("turn");
    }
    for (const auto &f : fighters_)
    {
        if (can_bandage(*actor, f))
        {
            offer("bandage", f.id);
        }
    }
    // Stepping off the field offers to flee, with a step left (combat.pas, MoveMenu).
    if (actor->half_moves >= 2)
    {
        for (const auto step : off_field_steps(*actor))
        {
            rules::Command flee;
            flee.revision = revision_;
            flee.actor = actor->id;
            flee.verb = "flee";
            flee.destination = step;
            commands.push_back(flee);
        }
    }
    offer("end");
    return commands;
}

std::vector<Cell> Session::movement_reach(EntityId id) const
{
    std::vector<Cell> cells;
    const auto *mover = find(id);
    if (!mover || id != actor_ || mover->half_moves <= 0)
    {
        return cells;
    }
    const auto reach = grid_for(*mover).reachable(mover->half_moves);
    for (int y = 0; y < board_.height; ++y)
    {
        for (int x = 0; x < board_.width; ++x)
        {
            if (reach.cost_to({x, y}))
            {
                cells.push_back({x, y});
            }
        }
    }
    return cells;
}

bool Session::submit(const rules::Command &command)
{
    const auto legal = legal_commands();
    const bool offered =
        std::any_of(legal.begin(), legal.end(),
                    [&command](const rules::Command &c)
                    {
                        return c.revision == command.revision && c.actor == command.actor &&
                               c.verb == command.verb && c.target == command.target &&
                               c.item == command.item &&
                               ((c.verb != "move" && c.verb != "flee" && c.verb != "aim") ||
                                c.destination == command.destination);
                    });
    if (!offered)
    {
        return false;
    }
    auto &actor = *find(actor_);
    if (command.verb == "move")
    {
        // Step by step along the cheapest path, which free attacks and guards may cut short.
        for (const auto cell :
             grid_for(actor).reachable(actor.half_moves).path_to(command.destination))
        {
            if (!actor.able() || actor.half_moves <= 0)
            {
                break;
            }
            step_to(actor, cell);
        }
    }
    else if (command.verb == "melee")
    {
        auto &target = *find(command.target);
        note_attack(actor, target);
        if (!sweep(actor, target))
        {
            make_attack(actor, target, false);
        }
    }
    else if (command.verb == "ranged" || command.verb == "throw")
    {
        auto &target = *find(command.target);
        note_attack(actor, target);
        shoot(actor, target);
    }
    else if (command.verb == "turn")
    {
        turn_undead(actor);
    }
    else if (command.verb == "cast")
    {
        begin_cast(actor, static_cast<int>(command.item));
    }
    else if (command.verb == "use")
    {
        // An item's spell goes off at once (viewchar.pas, UseItem).
        actor.using_item = static_cast<int>(command.item / 256);
        log({"{name} uses an item.", {{"name", actor.name}}});
        start_aiming(actor, static_cast<int>(command.item % 256));
    }
    else if (command.verb == "aim")
    {
        const auto &known = *game_spell(actor.aiming);
        const int area = known.area & 0x0F;
        if (area >= 8 && area <= 14)
        {
            resolve_spell(actor, within(command.destination, area & 7), command.destination);
        }
        else
        {
            actor.picked.push_back(command.target);
            if (--actor.picks_left <= 0)
            {
                resolve_spell(actor, actor.picked, find(command.target)->cell);
            }
        }
    }
    else if (command.verb == "blast_delay")
    {
        actor.blast_delay = static_cast<int>(command.item);
    }
    else if (command.verb == "flames")
    {
        resolve_spell(actor, {actor.id}, actor.cell, static_cast<Flames>(command.item));
    }
    else if (command.verb == "aim_done")
    {
        resolve_spell(actor, actor.picked, find(actor.picked.back())->cell);
    }
    else if (command.verb == "abort")
    {
        log({"{name} aborts the spell.", {{"name", actor.name}}});
        // An item's aim given up ends the turn but spends nothing; the
        // original spent the charge (an obvious bug, fixed).
        if (actor.using_item >= 0)
        {
            actor.aiming = actor.picks_left = 0;
            actor.picked.clear();
            actor.using_item = -1;
        }
        else
        {
            lose_spell(actor);
        }
        end_actions(actor);
    }
    else if (command.verb == "flee")
    {
        attempt_flee(actor);
    }
    else if (command.verb == "quick" || command.verb == "quick_magic")
    {
        ai_turn(actor, command.verb == "quick_magic");
    }
    else if (command.verb == "ready" || command.verb == "unready")
    {
        toggle_readied(actor, command.item);
    }
    else if (command.verb == "guard")
    {
        end_actions(actor);
        actor.guarding = true;
        log({"{name} guards.", {{"name", actor.name}}});
    }
    else if (command.verb == "delay")
    {
        // It acts again later this round, at initiative 1; in Secret, last of
        // all (combat.pas, ManualControl, in its reconstruction).
        actor.initiative = 1;
        if (secret())
        {
            turn_order_.push_back(actor.id);
        }
        actor_ = 0;
    }
    else if (command.verb == "bandage")
    {
        auto &dying = *find(command.target);
        dying.condition = Condition::unconscious;
        dying.overkill = 0;
        log({"{name} is bandaged.", {{"name", dying.name}}});
        end_actions(actor);
    }
    else
    {
        end_actions(actor);
    }
    ++revision_;
    if (outcome_ == rules::Outcome::ongoing && (actor_ == 0 || !actor.aiming) &&
        (actor_ == 0 || actor.initiative == 0 || !actor.able() ||
         (actor.half_moves == 0 && actor.attacks() == 0)))
    {
        next_actor();
    }
    return true;
}

void Session::turn_against_party(const std::vector<EntityId> &turning)
{
    for (const auto id : turning)
    {
        if (auto *f = find(id))
        {
            f->side = Side::opposition;
        }
    }
    ++revision_;
}

std::string Session::save() const
{
    Writer out;
    out(std::string(checkpoint_format), board_.width, board_.height);
    for (const auto tile : board_.terrain)
    {
        out(static_cast<unsigned>(tile));
    }
    out(board_.plants.size());
    for (const bool plant : board_.plants)
    {
        out(plant);
    }
    out(random_.value, round_, revision_, actor_, static_cast<unsigned>(outcome_),
        morale_threshold_, outdoors_, escape_ends_fight_, escape_side_.value_or(4),
        hostile_hit_points_percent_, clouds_.size());
    for (const auto &cloud : clouds_)
    {
        out(cloud.caster, cloud.index, cloud.corner.x, cloud.corner.y,
            static_cast<unsigned>(cloud.kind));
        for (const bool covers : cloud.covers)
        {
            out(covers);
        }
    }
    out(blasts_.size());
    for (const auto &blast : blasts_)
    {
        out(blast.caster, blast.index, blast.cell.x, blast.cell.y, blast.level);
    }
    out(turn_order_.size());
    for (const auto id : turn_order_)
    {
        out(id);
    }
    out(fighters_.size());
    for (const auto &f : fighters_)
    {
        out(f.id, f.name, f.definition, static_cast<unsigned>(f.side), f.cell.x, f.cell.y,
            f.footprint.width, f.footprint.height, f.facing_left, f.numbers.encode(), f.hit_points,
            static_cast<unsigned>(f.condition), f.overkill, f.initiative, f.half_moves,
            f.attacks_left[0], f.attacks_left[1], f.surprised, f.attack_phase, f.sweep_budget,
            f.direction, f.flank_count, f.flank_spread, f.guarding, f.fleeing, f.turned,
            f.tried_turning, f.cast_allowed, f.pending_spell, f.aiming, f.picks_left, f.blast_delay,
            static_cast<unsigned>(f.own_side), f.item_use_allowed, f.using_item, f.stance, f.lock,
            f.before_animation ? f.before_animation->encode() : std::string(), f.picked.size());
        for (const auto id : f.picked)
        {
            out(id);
        }
        out(f.shot_item, f.item_ids.size());
        for (const auto &[ordinal, id] : f.item_ids)
        {
            out(ordinal, id);
        }
        out(f.record ? encode_fight_record(*f.record) : std::string(), f.weighs_unreadied,
            f.readied_changed, f.held.size());
        for (const auto &held : f.held)
        {
            const auto &item = held.carried.item;
            out(por::original_item_key(item), item.readied_raw, item.stack_size, held.inventory_id,
                held.name, held.start_count, held.gone, held.readied_order);
        }
        out(f.worked_from.size());
        for (const int at : f.worked_from)
        {
            out(at);
        }
    }
    out(log_.size());
    for (const auto &entry : log_)
    {
        out(entry.message.source, entry.message.arguments.size());
        for (const auto &argument : entry.message.arguments)
        {
            out(argument.name, argument.value, argument.translate);
        }
    }
    return out.text();
}

namespace
{
// A fighter's record and held items as a checkpoint keeps them, their types
// from the game's.
void read_gear(Reader &in, Fighter &f, const Content &content)
{
    const auto invalid = []
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    };
    if (const auto record = in.text(); !record.empty())
    {
        f.record = decode_fight_record(record);
        if (!f.record)
        {
            invalid();
        }
    }
    f.weighs_unreadied = in.as<bool>();
    f.readied_changed = in.as<bool>();
    const auto count = in.as<std::size_t>();
    if (count > 255)
    {
        invalid();
    }
    for (std::size_t n = 0; n < count; ++n)
    {
        auto item = por::item_of_original_key(in.text());
        if (!item || item->type >= content.item_types.size())
        {
            invalid();
        }
        Held held{{*item, content.item_types[item->type]}};
        auto &raw = held.carried.item;
        raw.readied_raw = in.as<std::uint8_t>();
        raw.stack_size = in.as<std::uint8_t>();
        raw.raw[52] = raw.readied_raw;
        raw.raw[57] = raw.stack_size;
        held.inventory_id = in.as<std::uint64_t>();
        held.name = in.text();
        held.start_count = in.as<int>();
        held.gone = in.as<bool>();
        held.readied_order = in.as<int>();
        f.held.push_back(std::move(held));
    }
    const auto worked = in.as<std::size_t>();
    for (std::size_t n = 0; n < worked; ++n)
    {
        const auto at = in.as<int>();
        if (at < 0 || static_cast<std::size_t>(at) >= f.held.size())
        {
            invalid();
        }
        f.worked_from.push_back(at);
    }
}
} // namespace

std::unique_ptr<Session> Session::restored(rules::Identity identity, std::string_view checkpoint,
                                           const Content &content)
{
    Reader in(checkpoint);
    if (in.text() != checkpoint_format)
    {
        throw std::runtime_error(rules::older_save_message);
    }
    std::unique_ptr<Session> session(new Session);
    session->identity_ = std::move(identity);
    auto &board = session->board_;
    board.width = in.as<int>();
    board.height = in.as<int>();
    if (board.width < 2 || board.height < 2 || board.width > 64 || board.height > 64)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    board.terrain.resize(static_cast<std::size_t>(board.width * board.height));
    for (auto &tile : board.terrain)
    {
        tile = in.as<rules::Terrain>();
    }
    const auto plants = in.as<std::size_t>();
    if (plants != 0 && plants != board.terrain.size())
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < plants; ++n)
    {
        board.plants.push_back(in.as<bool>());
    }
    rules::validate_battlefield(board);
    session->random_.value = in.as<std::uint64_t>();
    session->round_ = in.as<unsigned>();
    session->revision_ = in.as<std::uint64_t>();
    session->actor_ = in.as<EntityId>();
    session->outcome_ = in.as<rules::Outcome>();
    session->morale_threshold_ = in.as<unsigned>();
    session->outdoors_ = in.as<bool>();
    session->escape_ends_fight_ = in.as<bool>();
    if (const auto side = in.as<unsigned>(); side < 4)
    {
        session->escape_side_ = side;
    }
    session->hostile_hit_points_percent_ = in.as<int>();
    const auto clouds = in.as<std::size_t>();
    if (clouds > 64)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < clouds; ++n)
    {
        Cloud cloud;
        cloud.caster = in.as<EntityId>();
        cloud.index = in.as<int>();
        cloud.corner = {in.as<int>(), in.as<int>()};
        const auto kind = in.as<unsigned>();
        if (kind > static_cast<unsigned>(CloudKind::deadly))
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        cloud.kind = static_cast<CloudKind>(kind);
        for (auto &covers : cloud.covers)
        {
            covers = in.as<bool>();
        }
        session->clouds_.push_back(cloud);
    }
    const auto blasts = in.as<std::size_t>();
    if (blasts > 64)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < blasts; ++n)
    {
        DelayedBlast blast;
        blast.caster = in.as<EntityId>();
        blast.index = in.as<int>();
        blast.cell = {in.as<int>(), in.as<int>()};
        blast.level = in.as<int>();
        session->blasts_.push_back(blast);
    }
    const auto ordered = in.as<std::size_t>();
    if (ordered > 256)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < ordered; ++n)
    {
        session->turn_order_.push_back(in.as<EntityId>());
    }
    if (session->morale_threshold_ > 100 || session->hostile_hit_points_percent_ < 0 ||
        session->hostile_hit_points_percent_ > 100)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    const auto count = in.as<std::size_t>();
    if (count > 256)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < count; ++n)
    {
        Fighter f;
        f.id = in.as<EntityId>();
        f.name = in.text();
        f.definition = in.text();
        f.side = in.as<Side>();
        f.cell = {in.as<int>(), in.as<int>()};
        f.footprint = {in.as<int>(), in.as<int>()};
        f.facing_left = in.as<bool>();
        const auto numbers = Combatant::decode(in.text());
        if (!numbers)
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        f.numbers = *numbers;
        f.hit_points = in.as<int>();
        f.condition = in.as<Condition>();
        f.overkill = in.as<int>();
        f.initiative = in.as<int>();
        f.half_moves = in.as<int>();
        f.attacks_left = {in.as<int>(), in.as<int>()};
        f.surprised = in.as<bool>();
        f.attack_phase = in.as<int>();
        f.sweep_budget = in.as<int>();
        f.direction = in.as<int>();
        f.flank_count = in.as<int>();
        f.flank_spread = in.as<int>();
        f.guarding = in.as<bool>();
        f.fleeing = in.as<bool>();
        f.turned = in.as<bool>();
        f.tried_turning = in.as<bool>();
        f.cast_allowed = in.as<bool>();
        f.pending_spell = in.as<int>();
        f.aiming = in.as<int>();
        f.picks_left = in.as<int>();
        f.blast_delay = in.as<int>();
        f.own_side = in.as<Side>();
        f.item_use_allowed = in.as<bool>();
        f.using_item = in.as<int>();
        f.stance = in.as<int>();
        f.lock = in.as<EntityId>();
        if (const auto before = in.text(); !before.empty())
        {
            f.before_animation = Combatant::decode(before);
            if (!f.before_animation)
            {
                throw std::runtime_error("Invalid original-rules combat checkpoint");
            }
        }
        const auto picked = in.as<std::size_t>();
        if (picked > 8 || (f.pending_spell && !spell(f.pending_spell)) ||
            (f.aiming && !spell(f.aiming)))
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        for (std::size_t n = 0; n < picked; ++n)
        {
            f.picked.push_back(in.as<EntityId>());
        }
        f.shot_item = in.as<std::uint64_t>();
        const auto counted = in.as<std::size_t>();
        if (counted > 255)
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        for (std::size_t n = 0; n < counted; ++n)
        {
            const auto ordinal = in.as<int>();
            f.item_ids.emplace_back(ordinal, in.as<std::uint64_t>());
        }
        read_gear(in, f, content);
        if (f.condition > Condition::stoned || f.attack_phase < 0 || f.attack_phase > 1 ||
            f.direction < 0 || f.direction > 7)
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        session->fighters_.push_back(std::move(f));
    }
    const auto entries = in.as<std::size_t>();
    for (std::size_t n = 0; n < entries; ++n)
    {
        Message message;
        message.source = in.text();
        const auto arguments = in.as<std::size_t>();
        for (std::size_t a = 0; a < arguments; ++a)
        {
            rules::MessageArgument argument;
            argument.name = in.text();
            argument.value = in.text();
            argument.translate = in.as<bool>();
            message.arguments.push_back(std::move(argument));
        }
        session->log_.push_back({english(message), std::move(message)});
    }
    const bool order_known = std::ranges::all_of(session->turn_order_,
                                                 [&session](EntityId id)
                                                 {
                                                     return session->find(id) != nullptr;
                                                 });
    if (!in.finished() || !order_known)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    return session;
}

} // namespace

namespace
{
// The counts a party member's inventory keeps, which its profile's items
// leave out: its usable items' charges or stacks, and how many of what its
// missile shoots it has.
void take_counts(Fighter &f, const std::vector<rules::CarriedEquipment> &inventory,
                 const Content &content)
{
    namespace bit = por::item_property;
    const auto [arrows_type, quarrels_type] = ammunition_types(content.game);
    for (const auto &carried : inventory)
    {
        const auto record = por::item_of_original_key(carried.definition);
        if (carried.equipment_index < 0 || !record)
        {
            continue;
        }
        const int count = static_cast<int>(carried.quantity);
        for (auto &usable : f.numbers.items)
        {
            if (usable.ordinal != carried.equipment_index)
            {
                continue;
            }
            f.item_ids.emplace_back(usable.ordinal, carried.inventory_id);
            if (usable.charges > 0)
            {
                usable.charges = count;
            }
            else
            {
                usable.quantity = count > 1 ? count : 0;
            }
        }
        auto &missile = f.numbers.missile;
        if (!missile || f.shot_item || f.numbers.weapon_type < 0 ||
            static_cast<std::size_t>(f.numbers.weapon_type) >= content.item_types.size())
        {
            continue;
        }
        const auto &launcher = content.item_types[static_cast<std::size_t>(f.numbers.weapon_type)];
        const bool shot =
            missile->thrown
                ? record->type == f.numbers.weapon_type
                : (launcher.has(bit::fires_arrows) && record->type == arrows_type) ||
                      (launcher.has(bit::fires_quarrels) && record->type == quarrels_type);
        if (shot)
        {
            f.shot_item = carried.inventory_id;
            missile->quantity = count > 1 ? count : 0;
        }
    }
}

// What a party member readies from in the fight: its record and the items it
// carries, each with its count, the readied in the order its profile has
// them. One with a readied item of no original record readies nothing.
void take_gear(Fighter &f, const rules::Participant &p, const Content &content)
{
    auto record = decode_fight_record(p.character_record);
    if (!record)
    {
        return;
    }
    std::vector<Held> held;
    for (const auto &carried : p.inventory)
    {
        const auto item = por::item_of_original_key(carried.definition);
        if (!item || item->type >= content.item_types.size())
        {
            if (carried.equipment_index >= 0)
            {
                return;
            }
            continue;
        }
        Held h{{*item, content.item_types[item->type]}, carried.inventory_id, carried.name};
        const auto count = static_cast<int>(carried.quantity);
        if (count > 1 && held_count(h.carried) != count)
        {
            h.carried.item.stack_size = static_cast<std::uint8_t>(count);
            h.carried.item.raw[57] = h.carried.item.stack_size;
        }
        set_readied(h, carried.equipment_index >= 0);
        h.readied_order = carried.equipment_index;
        h.start_count = held_count(h.carried);
        held.push_back(std::move(h));
    }
    std::vector<int> order;
    for (std::size_t at = 0; at < held.size(); ++at)
    {
        if (held[at].readied_order >= 0)
        {
            order.push_back(static_cast<int>(at));
        }
    }
    std::ranges::sort(order,
                      [&held](int a, int b)
                      {
                          return held[static_cast<std::size_t>(a)].readied_order <
                                 held[static_cast<std::size_t>(b)].readied_order;
                      });
    f.record = std::move(*record);
    f.held = std::move(held);
    f.worked_from = std::move(order);
}

// What a monster readies from: its record and all it carries, as its
// numbers were worked out from them.
void take_monster_gear(Fighter &f, const MonsterGear &gear)
{
    f.record = gear.record;
    f.weighs_unreadied = true;
    for (std::size_t at = 0; at < gear.carried.size(); ++at)
    {
        Held h{gear.carried[at]};
        h.readied_order = h.carried.item.readied() ? static_cast<int>(at) : -1;
        h.start_count = held_count(h.carried);
        f.held.push_back(std::move(h));
        f.worked_from.push_back(static_cast<int>(at));
    }
}
} // namespace

std::unique_ptr<rules::CombatSession> create_session(rules::Identity identity,
                                                     rules::Encounter encounter, std::uint64_t seed,
                                                     const Content &content)
{
    std::vector<Fighter> fighters;
    for (auto &p : encounter.participants)
    {
        Fighter f;
        f.id = p.id;
        f.name = std::move(p.name);
        f.definition = p.definition;
        f.side = p.side;
        f.cell = p.cell;
        f.footprint = p.footprint;
        f.facing_left = p.facing_left;
        f.surprised = p.surprised;
        if (!p.character_profile.empty())
        {
            const auto numbers = Combatant::decode(p.character_profile);
            if (!numbers)
            {
                throw std::runtime_error("A party member has no original-rules profile: " + f.name);
            }
            f.numbers = *numbers;
            take_counts(f, p.inventory, content);
            take_gear(f, p, content);
            // A member's last fight left it the spells it had not cast, which
            // it keeps until a day's rest memorizes them again, and the
            // conditions that outlast fights and rests (lasting_condition).
            if (p.state && !p.state->resources.empty())
            {
                const auto carried = carried_state(p.state->resources, content.game);
                if (carried.last_fight)
                {
                    f.numbers.spells = carried.last_fight->spells;
                }
                for (const auto &kept : carried.conditions)
                {
                    if (!f.has(kept.code))
                    {
                        f.numbers.effects.push_back(kept);
                    }
                }
            }
        }
        else if (const auto found = content.bestiary.find(p.definition);
                 found != content.bestiary.end())
        {
            f.numbers = found->second;
            if (const auto gear = content.gear.find(p.definition); gear != content.gear.end())
            {
                take_monster_gear(f, gear->second);
            }
        }
        else
        {
            throw std::runtime_error("No original-rules record for " + p.definition);
        }
        f.hit_points = p.state ? p.state->hit_points : f.numbers.hit_points;
        if (p.state && p.state->dead)
        {
            f.condition = Condition::dead;
        }
        else if (p.state && p.state->description == stoned_description)
        {
            f.condition = Condition::stoned;
            f.hit_points = 0;
        }
        else if (f.hit_points <= 0)
        {
            f.condition = Condition::unconscious;
            f.hit_points = 0;
        }
        fighters.push_back(std::move(f));
    }
    // A party NPC without a morale of its own takes the encounter's (combatst.pas,
    // InitCreatureCombatData).
    const auto threshold = std::min(encounter.morale, 100U);
    for (auto &f : fighters)
    {
        const auto own = f.numbers.morale & 0x7F;
        if (f.side == Side::party && f.numbers.morale >= non_player && (own == 0 || own > 102))
        {
            // The original stored the threshold unhalved, so an ally's morale
            // was twice the encounter's and failed every check from 52 up (an
            // obvious bug, fixed).
            f.numbers.morale = non_player + threshold / 2;
        }
    }
    return std::make_unique<Session>(std::move(identity), std::move(encounter.battlefield),
                                     std::move(fighters), seed, threshold, encounter.outdoors,
                                     encounter.escape_ends_fight);
}

std::unique_ptr<rules::CombatSession>
restore_session(rules::Identity identity, std::string_view checkpoint, const Content &content)
{
    return Session::restored(std::move(identity), checkpoint, content);
}

} // namespace opengold::original::detail
