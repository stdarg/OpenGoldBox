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
constexpr std::string_view checkpoint_format = "OGORIGINALCOMBAT 16";

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
              prayer = 49, coughing = 30, nausea = 31, stinking_cloud = 40, animated = 32,
              snake_charm = 51, slow_poison_tick = 15, slow_poison = 22, revival = 78,
              read_magic = 16, dust = 71;
// Displacement keeps whether it has turned an attack aside in this bit of its strength.
constexpr int displaced = 0x10;
} // namespace effect

// What a hit carries, by the attacker's effects, in the original's order: a
// primary attack's hit and a secondary one's (effhndlr.pas, HIT_PHASE_1 and
// HIT_PHASE_2).
constexpr std::array<int, 13> primary_hit_effects{85, 86, 87, 68, 79, 80, 76, 73, 77,
                                                  // Curse's (canonical_effect).
                                                  257, 296, 322, 323};
constexpr std::array<int, 11> secondary_hit_effects{64, 65, 66, 67, 68, 69, 70, 79, 85, 86, 87};

struct Fighter
{
    EntityId id{};
    std::string name, definition;
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

int attack_modifier(const Fighter &attacker, const Fighter &defender);

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
// item's own power (magic.pas, GetCasterLevel).
int caster_level(const Fighter &caster, const Spell &spell)
{
    // From an item a spell is cast at level 6, an item's own power at 12
    // (magic.pas, GetCasterLevel).
    if (caster.using_item >= 0)
    {
        return spell.caster_class == SpellClass::item ? 12 : 6;
    }
    switch (spell.caster_class)
    {
    case SpellClass::cleric:
        return caster.numbers.class_levels[static_cast<std::size_t>(ClassSlot::cleric)];
    case SpellClass::magic_user:
        return caster.numbers.class_levels[static_cast<std::size_t>(ClassSlot::magic_user)];
    default:
        return 12;
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

// The spells built so far; the rest are not offered yet.
bool spell_built(int number)
{
    constexpr std::array built{26, 27, 36, 41, 46, 57, 58, 60, 61, 62, 63, 64, 65, 1, 2, 3, 4, 5, 6,
                               7, 8, 9, 10, 11, 15, 16, 17, 19, 20, 21, 23, 24, 25, 29, 30, 32, 33,
                               37, 38, 40, 42, 34, 43, 44, 45, 47, 48, 49, 50, 51, 52, 53, 54, 55,
                               // Curse's (spells.cpp, curse_spells).
                               158, 166, 168, 169, 171, 172, 174, 176, 187, 192, 194, 198, 199,
                               200};
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

// A stinking cloud on the field: the caster's, its number among the caster's
// clouds, and the squares it covers, the corner and those east, south-east and
// south of it where the ground can be walked (magic.pas, SpellStinkingCloud).
struct Cloud
{
    EntityId caster{};
    int index{};
    Cell corner;
    std::array<bool, 4> covers{};

    [[nodiscard]] std::array<Cell, 4> squares() const
    {
        return {corner, Cell{corner.x + 1, corner.y}, Cell{corner.x + 1, corner.y + 1},
                Cell{corner.x, corner.y + 1}};
    }
};

class Session final : public rules::CombatSession
{
  public:
    Session(rules::Identity identity, rules::Battlefield board, std::vector<Fighter> fighters,
            std::uint64_t seed, unsigned morale)
        : identity_(std::move(identity)), board_(std::move(board)), fighters_(std::move(fighters)),
          random_{seed}, morale_threshold_(std::min(morale, 100U))
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

    static std::unique_ptr<Session> restored(rules::Identity identity, std::string_view checkpoint);

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
    std::vector<Cloud> clouds_;
    // The opposition's Hit Points left of its whole, in steps of 5, as of the
    // round's start (cbtacts.pas, UpdateHostileHpPercent).
    int hostile_hit_points_percent_{100};

    int roll(int sides)
    {
        return roll_die(random_, sides);
    }

    [[nodiscard]] Fighter *find(EntityId id);
    [[nodiscard]] const Fighter *find(EntityId id) const;
    [[nodiscard]] rules::MovementGrid grid_for(const Fighter &mover) const;
    void log(Message message);
    void begin_round();
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
    void kill(Fighter &victim);
    void drain_levels(Fighter &victim, int levels);
    void rake(Fighter &attacker, Fighter &victim);
    void attached_turn(Fighter &f);
    void turn_undead(Fighter &cleric);
    void begin_cast(Fighter &caster, int spell_number);
    void start_aiming(Fighter &caster, int spell_number);
    void resolve_spell(Fighter &caster, std::vector<EntityId> targets, Cell aim);
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
    void cast_stinking_cloud(Fighter &caster, int level, Cell aim);
    void remove_with_handler(Fighter &f, std::size_t node);
    void dispel(Fighter &target, int level);
    void animate_dead(Fighter &caster, int level);
    void slow_poison(Fighter &target, int level);
    void revive(Fighter &f);
    [[nodiscard]] bool in_cloud(const Fighter &f) const;
    void breathe_cloud(Fighter &f);
    bool lightning_at(Cell cell, int damage, int level, int save = 4);
    void lightning_path(Cell origin, Cell aim, int length, int damage, int level, bool bounce,
                        int save = 4);
    void monster_specials(Fighter &f);
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
    // The original's AI (combatai.pas).
    void ai_turn(Fighter &f);
    bool ai_use_item(Fighter &f);
    bool ai_cast(Fighter &f);
    [[nodiscard]] bool ai_decides(const Fighter &f, int number, int least_priority) const;
    bool ai_aim(Fighter &f);
    [[nodiscard]] Fighter *cure_search(const Fighter &f);
    bool acquire_target(Fighter &f, int range, bool relaxed, bool force_new);
    [[nodiscard]] std::vector<EntityId> enemies_within(const Fighter &f, int range,
                                                       bool sight) const;
    bool ai_move_attack(Fighter &f);
    bool ai_step(Fighter &f, int &last_direction, int &stuck);
    bool can_step(const Fighter &f, int direction, bool &edge) const;
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
}

// The highest initiative left acts next; a tie goes to the higher of fresh
// d100 rolls (combat.pas, NextCreature). With none left the round ends.
void Session::next_actor()
{
    actor_ = 0;
    while (outcome_ == rules::Outcome::ongoing)
    {
        Fighter *best = nullptr;
        int best_tiebreak = 0;
        for (auto &f : fighters_)
        {
            if (!f.able() || f.initiative <= 0)
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
            if (best->helpless())
            {
                end_actions(*best);
                continue;
            }
            // A holding bite or a drinking stirge spends its turn on its victim
            // (effhndlr.pas, ACTION_START).
            attached_turn(*best);
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
    const bool backstab = attacker.numbers.backstabs && behind_its_back;
    const bool behind = from_behind || (behind_its_back && defender.flank_spread > 4);
    // Shield keeps the front Armor Class at 3 or better (effhndlr.pas, EffShield).
    const int front = defender.has(effect::shield) ? std::min(defender.numbers.armor_class, 3)
                                                   : defender.numbers.armor_class;
    // Blindness leaves both Armor Classes 4 worse (effhndlr.pas, EffBlind);
    // coughing leaves the rear one 2 worse, no worse than 10, and the front as
    // bad (EffCoughing).
    const int blind = defender.has(effect::blind) ? 4 : 0;
    const bool coughs = defender.has(effect::coughing);
    const int rear = coughs ? std::max(std::min(defender.numbers.rear_armor_class + 2, 10),
                                       defender.numbers.rear_armor_class)
                            : defender.numbers.rear_armor_class;
    const int armor_class = (backstab ? rear + 2 : behind ? rear : coughs ? rear : front) + blind;
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
            const int modifier = attack_modifier(attacker, defender) + prayer_bonus(attacker) -
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
            int dice = 0;
            for (int die = 0; die < attack.dice; ++die)
            {
                dice += roll(std::max(attack.sides, 1));
            }
            auto damage = hit_damage(dice, attack.modifier);
            if (backstab)
            {
                damage *= attacker.numbers.thief_level / 4 + 2;
            }
            damage = weapon_damage_defenses(attacker, defender, damage, phase, primary_hits);
            log({"{attacker} hits {name} for {damage}.",
                 {{"attacker", attacker.name},
                  {"name", defender.name},
                  {"damage", std::to_string(damage)}}});
            primary_hits += phase == 0 ? 1 : 0;
            inflict(defender, damage);
            if (defender.able())
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
int attack_modifier(const Fighter &attacker, const Fighter &defender)
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
    const auto &known = *spell(spell_number);
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
    actor_ = 0;
}

// Choosing targets as the spell goes off (cbtacts.pas, SelectCombatTargets):
// a spell on the caster goes off at once; others wait for their aim.
void Session::start_aiming(Fighter &caster, int spell_number)
{
    const auto &known = *spell(spell_number);
    const int area = known.area & 0x0F;
    caster.aiming = spell_number;
    caster.picked.clear();
    if (area == 0)
    {
        resolve_spell(caster, {caster.id}, caster.cell);
        return;
    }
    caster.picks_left = area <= 7 ? (area & 3) + 1 : 1;
}

// Whether the caster can single out a creature for its spell: within range,
// in sight, and no veto (cbtacts.pas, AimTargetMenu). Healing reaches the
// downed but not the dead.
bool Session::reachable_by_spell(const Fighter &caster, const Fighter &target) const
{
    const auto &known = *spell(caster.aiming);
    const bool heals = caster.aiming == 3;
    const bool slows_poison =
        caster.aiming == 26 && target.has(effect::poisoned) && target.condition == Condition::dead;
    if (!(target.able() || (heals && target.downed()) || slows_poison) ||
        std::find(caster.picked.begin(), caster.picked.end(), target.id) != caster.picked.end())
    {
        return false;
    }
    return distance(caster, target) <= spell_range(known, caster_level(caster, known)) &&
           sees(board_, caster, target) && can_target(caster, target);
}

std::vector<rules::Command> Session::aiming_commands(const Fighter &caster) const
{
    std::vector<rules::Command> commands;
    const auto &known = *spell(caster.aiming);
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
    if (area >= 8 && area <= 14)
    {
        // A square in range and in sight, open ground (cbtacts.pas, ManualAimCursor).
        const int range = spell_range(known, caster_level(caster, known));
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

void Session::resolve_spell(Fighter &caster, std::vector<EntityId> targets, Cell aim)
{
    const int number = caster.aiming;
    const auto &known = *spell(number);
    const int level = caster_level(caster, known);
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
        // Cure Light Wounds: 1d8, not through rot; the dying stop bleeding but
        // stay down (magic.pas, the cure).
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
        // (magic.pas, the charm).
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.numbers.creature_type > 1 || f.numbers.large)
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
        // held (magic.pas, the hold).
        const int count = static_cast<int>(targets.size());
        const int modifier = count == 1 ? (number == 23 ? -2 : -3) : count == 2 ? -1 : 0;
        for (auto id = targets.rbegin(); id != targets.rend(); ++id)
        {
            auto &f = *find(*id);
            if (f.numbers.creature_type > 1 || f.numbers.large)
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
        // Prayer: its strength keeps the caster's side in bit 4.
        add_with_message(
            caster,
            {effect::prayer, level, (caster.side == Side::opposition ? 0x10 : 0) + level, false},
            false, false, 0, level, &known);
        break;
    case 43:
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
        cast_stinking_cloud(caster, level, aim);
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
        // the caster's own can pay for (magic.pas, the snake charm).
        int pool = caster.hit_points;
        for (auto &f : fighters_)
        {
            if (!f.able() || f.numbers.creature_type != 14 || f.hit_points > pool)
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
        cast_generic(caster, number, level, targets, roll_dice(roll(3) * 2 + 1, 6), 9);
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
        cast_generic(caster, number, level, targets, roll_dice(level, 6), 9);
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
        // A monster's blast: 6d6 along a path to 3 squares.
        cast_generic(caster, number, level, path_area(caster, aim, 3, 1), roll_dice(6, 6), 8);
        break;
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
    default:
        cast_generic(caster, number, level, targets, 0, 0);
        break;
    }
    caster.using_item = -1;
    end_actions(caster);
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
    const auto &known = *spell(number);
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
                             attack_modifier(caster, target)))
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
    return false;
}

// Whether a creature resists magic, by Pool of Radiance's or Curse's numbers.
bool resists_magic_at_all(const Fighter &target)
{
    return target.has(105) || target.has(106) || target.has(305) || target.has(306);
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
    immune = immune || (target.has(263) && spell_effect && known->level < 4);
    immune = immune || (target.has(310) && (element & 2));
    immune = immune || (target.has(335) && (element & 4));
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
    if ((element & 8) && resists_magic_at_all(target) &&
        magic_resists(target, caster_level, roll(100)))
    {
        damage = 0;
    }
    // Curse's protections (ovr013): cold for one immune to it (310), dragon
    // breath (273), magic (329), electricity (335), and a spell below the
    // fourth level for a Minor Globe of Invulnerability (263).
    const auto *cast = spell(spell_number);
    if (((element & 2) && target.has(310)) || ((element & 0x20) && target.has(273)) ||
        ((element & 8) && target.has(329)) || ((element & 4) && target.has(335)) ||
        (target.has(263) && cast && cast->caster_class != SpellClass::item && cast->level < 4))
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
    if (damage <= 0)
    {
        return;
    }
    log({"{name} takes {damage}.", {{"name", target.name}, {"damage", std::to_string(damage)}}});
    inflict(target, damage);
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
    const auto &known = *spell(21);
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
// rounds; those in it breathe it at once.
void Session::cast_stinking_cloud(Fighter &caster, int level, Cell aim)
{
    // The caster's clouds before the last one laid, as the original counts them.
    int index = 0;
    for (std::size_t n = 0; n + 1 < clouds_.size(); ++n)
    {
        index += clouds_[n].caster == caster.id ? 1 : 0;
    }
    Cloud cloud{caster.id, index, aim, {}};
    const auto squares = cloud.squares();
    for (std::size_t n = 0; n < squares.size(); ++n)
    {
        cloud.covers[n] =
            board_.contains(squares[n]) && board_.at(squares[n]) != rules::Terrain::obstacle;
    }
    clouds_.push_back(cloud);
    caster.numbers.effects.push_back({effect::stinking_cloud, level, level + index * 16, true});
    log({"{name} creates a noxious cloud.", {{"name", caster.name}}});
    for (const auto square : squares)
    {
        if (const auto id = standing_at(square))
        {
            breathe_cloud(*find(id));
        }
    }
}

bool Session::in_cloud(const Fighter &f) const
{
    const auto mine = squares_of(f);
    return std::any_of(clouds_.begin(), clouds_.end(),
                       [&mine](const Cloud &cloud)
                       {
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

// One in a cloud saves against paralysis: saved, it coughs for the round;
// failed, nausea leaves it helpless for 1d4+1 rounds. One coughing, nauseous,
// animated or immune to poison breathes it unharmed (fxengine.pas,
// ConsiderTerrainEffects).
void Session::breathe_cloud(Fighter &f)
{
    if (!f.able() || !in_cloud(f) || f.has(effect::coughing) || f.has(effect::nausea) ||
        f.has(effect::animated) || f.has(111) || f.has(125))
    {
        return;
    }
    const auto &known = *spell(34);
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
         {{"name", target.name}, {"spell", std::string(spell(26)->name), true}}});
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
void Session::ai_turn(Fighter &f)
{
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
        !f.tried_turning && undead_in_sight)
    {
        turn_undead(f);
        return;
    }
    if (ai_cast(f))
    {
        return;
    }
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
            // A missile weapon with an enemy next to it: the original readied
            // another weapon here; until a fight knows a creature's items it
            // guards (step 7).
            if (missile_in_hand(f) && !f.numbers.missile->throwable && enemy_next_to(f))
            {
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
    if (f.has(84) && lock && lock->able() && can_target(f, *lock) && sees(board_, f, *lock))
    {
        // The charming gaze: saved against at -2. The original's reach was an
        // unset value; it reaches what the gazer sees here (an obvious bug, fixed).
        log({"{name} gazes.", {{"name", f.name}}});
        const bool saved = saving_throw(*lock, 4, -2, &f);
        if (add_with_message(*lock,
                             {effect::charm, 0, (f.side == Side::opposition ? 128 : 0) + 12, true},
                             true, saved, 0, 12, spell(10)))
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
    const auto cost = grid_for(f).step_cost(f.cell, moved.cell);
    if (!cost || *cost > f.half_moves)
    {
        return false;
    }
    const bool breathes_freely =
        f.has(effect::animated) || f.has(effect::coughing) || f.has(111) || f.has(125);
    return breathes_freely || !in_cloud(moved);
}

// One step (combatai.pas, AiMoveStep): toward its lock, or away when
// fleeing, trying the pattern's five directions; a fleeing creature that
// finds the edge tries to get away. Stuck (no way, or turning back), it
// changes pattern, then drops its lock, then stops. False when it cannot go on.
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
    else if (const auto *locked = find(f.lock))
    {
        base = direction_toward(f.cell, locked->cell);
    }
    const auto &pattern = approach[static_cast<std::size_t>(std::clamp(f.stance, 1, 6) - 1)];
    int chosen = -1;
    for (const int offset : pattern)
    {
        const int direction = (base + offset) % 8;
        bool edge = false;
        if (can_step(f, direction, edge))
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
    last_direction = chosen;
    step_to(f, {f.cell.x + step_x[static_cast<std::size_t>(chosen)],
                f.cell.y + step_y[static_cast<std::size_t>(chosen)]});
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
            const auto &known = *spell(number);
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
bool Session::ai_cast(Fighter &f)
{
    const int passes = roll(7);
    const bool enemy_left = std::any_of(fighters_.begin(), fighters_.end(),
                                        [&f](const Fighter &o)
                                        {
                                            return o.side != f.side && o.able();
                                        });
    const auto &spells = f.numbers.spells;
    if (!f.cast_allowed || silenced(f) || !enemy_left || f.numbers.morale < non_player ||
        spells.empty())
    {
        return false;
    }
    for (int pass = 1; pass <= passes; ++pass)
    {
        for (int pick = 0; pick < 3; ++pick)
        {
            const int number =
                spells[static_cast<std::size_t>(roll(static_cast<int>(spells.size())) - 1)];
            if (spell(number) && spell(number)->when != 0 && spell_built(number) &&
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
    const auto &known = *spell(number);
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
    const int range = spell_range(known, caster_level(f, known));
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
    const auto &known = *spell(f.aiming);
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
    const int range = spell_range(known, caster_level(f, known));
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
void Session::kill(Fighter &victim)
{
    victim.condition = Condition::dead;
    victim.hit_points = 0;
    victim.initiative = 0;
    log({"{name} is killed.", {{"name", victim.name}}});
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
    {
        // The caster's cloud of this number clears (effhndlr.pas, EffClearStinkingCloud).
        const auto cloud =
            std::find_if(clouds_.begin(), clouds_.end(),
                         [&](const Cloud &c)
                         {
                             return c.caster == f.id && c.index == ended.strength / 16;
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
// ready; camouflage hides.
void Session::combat_start()
{
    bool feared = false;
    for (auto &f : fighters_)
    {
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
    }
}

// At a round's end, after effects age (combat.pas, EndOfRound): regeneration
// heals 3; camouflage hides again, 95 times in 100; displacement is ready for
// the next round.
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
    if (defender.has(effect::blink) && defender.initiative > 0)
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
    if ((f.has(90) && save == 0) || (f.has(97) && (save == 4 || save == 2)))
    {
        modifier += hardiness;
    }
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
        return;
    }
    log({"{name} cannot get away.", {{"name", f.name}}});
    end_actions(f);
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

rules::Snapshot Session::snapshot() const
{
    rules::Snapshot state;
    state.identity = identity_;
    state.revision = revision_;
    state.round = round_;
    state.actor = actor_;
    state.outcome = outcome_;
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
        view.persistent = {keeps ? f->hit_points : 0, view.dead, f->numbers.encode(), {}};
        // What is left of a party member's items, for the game to trim its
        // inventory to: an item gone is 0.
        for (const auto &[ordinal, id] : f->item_ids)
        {
            const auto &items = f->numbers.items;
            const auto usable = std::ranges::find(items, ordinal, &UsableItem::ordinal);
            const int left = usable == items.end() ? 0
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
            const auto *known = spell(number);
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
                const auto *known = spell(number);
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
    // Guard needs a weapon that strikes next to it (combat.pas, DoneMenu).
    if (!actor->numbers.missile || actor->numbers.missile->throwable)
    {
        offer("guard");
    }
    offer("delay");
    // Quick: the original's AI plays its turn (combat.pas, TurnOnAuto).
    offer("quick");
    // Turn, once a fight for one with cleric levels (combat.pas, CombatMenu).
    if (actor->numbers.class_levels[static_cast<std::size_t>(ClassSlot::cleric)] > 0 &&
        !actor->tried_turning)
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
        const auto &known = *spell(actor.aiming);
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
    else if (command.verb == "quick")
    {
        ai_turn(actor);
    }
    else if (command.verb == "guard")
    {
        end_actions(actor);
        actor.guarding = true;
        log({"{name} guards.", {{"name", actor.name}}});
    }
    else if (command.verb == "delay")
    {
        // It acts again later this round, at initiative 1.
        actor.initiative = 1;
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
        morale_threshold_, hostile_hit_points_percent_, clouds_.size());
    for (const auto &cloud : clouds_)
    {
        out(cloud.caster, cloud.index, cloud.corner.x, cloud.corner.y, cloud.covers[0],
            cloud.covers[1], cloud.covers[2], cloud.covers[3]);
    }
    out(fighters_.size());
    for (const auto &f : fighters_)
    {
        out(f.id, f.name, f.definition, static_cast<unsigned>(f.side), f.cell.x, f.cell.y,
            f.footprint.width, f.footprint.height, f.facing_left, f.numbers.encode(), f.hit_points,
            static_cast<unsigned>(f.condition), f.overkill, f.initiative, f.half_moves,
            f.attacks_left[0], f.attacks_left[1], f.surprised, f.attack_phase, f.sweep_budget,
            f.direction, f.flank_count, f.flank_spread, f.guarding, f.fleeing, f.turned,
            f.tried_turning, f.cast_allowed, f.pending_spell, f.aiming, f.picks_left,
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

std::unique_ptr<Session> Session::restored(rules::Identity identity, std::string_view checkpoint)
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
        for (auto &covers : cloud.covers)
        {
            covers = in.as<bool>();
        }
        session->clouds_.push_back(cloud);
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
    if (!in.finished())
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
    constexpr std::uint8_t arrows_type = 73, quarrels_type = 28;
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
            // A member's last fight left it the spells it had not cast, which
            // it keeps until a day's rest memorizes them again.
            if (p.state && !p.state->resources.empty())
            {
                if (const auto last = Combatant::decode(p.state->resources))
                {
                    f.numbers.spells = last->spells;
                }
            }
        }
        else if (const auto found = content.bestiary.find(p.definition);
                 found != content.bestiary.end())
        {
            f.numbers = found->second;
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
                                     std::move(fighters), seed, threshold);
}

std::unique_ptr<rules::CombatSession> restore_session(rules::Identity identity,
                                                      std::string_view checkpoint)
{
    return Session::restored(std::move(identity), checkpoint);
}

} // namespace opengold::original::detail
