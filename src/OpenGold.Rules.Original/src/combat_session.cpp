#include "combat_session.h"

#include "opengold/dice.h"
#include "opengold/movement_grid.h"
#include "opengold/original_rules.h"

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
constexpr std::string_view checkpoint_format = "OGORIGINALCOMBAT 9";

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
    gone
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
constexpr int bite_hold = 74, blood_drain = 75, rot = 50, poisoned = 55, mummy_rot = 57,
              paralysis = 52, immobile = 58, see_invisible = 24, invisible = 25, blink = 37,
              regenerating_soon = 59, camouflage = 72, mummy_fear = 82, displacement = 89,
              fighting_on = 95, regeneration = 98, fights_while_down = 99, troll_burn = 100,
              troll_heal = 101, troll_return = 102, gaze_block = 126;
// Displacement keeps whether it has turned an attack aside in this bit of its strength.
constexpr int displaced = 0x10;
} // namespace effect

// What a hit carries, by the attacker's effects, in the original's order: a
// primary attack's hit and a secondary one's (effhndlr.pas, HIT_PHASE_1 and
// HIT_PHASE_2).
constexpr std::array<int, 8> primary_hit_effects{85, 86, 87, 68, 76, 73, 77, 0};
constexpr std::array<int, 10> secondary_hit_effects{64, 65, 66, 67, 68, 69, 70, 85, 86, 87};

struct Fighter
{
    EntityId id{};
    std::string name, definition;
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
    void on_hit(Fighter &attacker, Fighter &defender, int phase, int primary_hits);
    void poison(Fighter &victim, int modifier);
    void paralyze(Fighter &victim, int minutes, int modifier);
    void kill(Fighter &victim);
    void drain_levels(Fighter &victim, int levels);
    void rake(Fighter &attacker, Fighter &victim);
    void attached_turn(Fighter &f);
    void turn_undead(Fighter &cleric);
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
    [[nodiscard]] bool saving_throw(const Fighter &f, int save, int modifier);
    void update_hostile_hit_points();
    [[nodiscard]] int highest_enemy_movement(const Fighter &f) const;
    bool check_morale(Fighter &f);
    void attempt_flee(Fighter &f);
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
        f.half_moves = f.has(effect::immobile) ? 0 : 2 * f.numbers.movement;
        f.attack_phase = 1;
        f.sweep_budget = f.numbers.attack_level;
        for (std::size_t form = 0; form < f.attacks_left.size(); ++form)
        {
            f.attacks_left[form] = attacks_this_round(f.numbers.attacks[form].rate_per_two_rounds,
                                                      static_cast<int>(round_));
        }
        // A loaded missile weapon attacks at its own rate, no more often than
        // it has shots (cbtacts.pas, RecalcAttacksLeft).
        if (const auto &missile = f.numbers.missile; missile && missile->loaded)
        {
            f.attacks_left[0] =
                attacks_this_round(missile->rate_per_two_rounds, static_cast<int>(round_));
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
    attacker.facing_left = defender.cell.x < attacker.cell.x;
    const bool behind_its_back =
        defender.flank_count > 1 &&
        direction_toward(attacker.cell, defender.cell) == defender.direction;
    const bool backstab = attacker.numbers.backstabs && behind_its_back;
    const bool behind = from_behind || (behind_its_back && defender.flank_spread > 4);
    const int armor_class = backstab ? defender.numbers.rear_armor_class + 2
                            : behind ? defender.numbers.rear_armor_class
                                     : defender.numbers.armor_class;
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
            const int modifier = defender.has(effect::invisible) ? -4 : 0;
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
                on_hit(attacker, defender, phase, primary_hits);
            }
        }
    }
    attacker.sweep_budget = 0;
    if (attacker.attacks() == 0)
    {
        end_actions(attacker);
    }
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
    if (target.hit_points >= damage)
    {
        target.hit_points -= damage;
        if (target.hit_points > 0)
        {
            return;
        }
        target.condition = Condition::unconscious;
        log({"{name} falls unconscious.", {{"name", target.name}}});
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
    }
    holder.remove(code);
}

// What a hit that leaves its target standing carries (cbtacts.pas, RollAttacks;
// effhndlr.pas). The original stacked a new hold at each hit of a holding
// bite; one hold at a time here (an obvious bug, fixed).
void Session::on_hit(Fighter &attacker, Fighter &defender, int phase, int primary_hits)
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
        if (numbers.level < 2)
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
            const int experience =
                experience_for_level(static_cast<ClassSlot>(slot), classes[slot]).value_or(0);
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
            numbers.thac0 += thac0(classes) - thac0(before);
            const auto saves_before = saving_throws(before);
            const auto saves_after = saving_throws(classes);
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
    if (defender.has(104) && attacker.numbers.weapon_type >= 0 && evades(60))
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
bool Session::saving_throw(const Fighter &f, int save, int modifier)
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
            highest = std::max(highest, other.numbers.movement);
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
    int morale = static_cast<int>(f.numbers.morale & 0x7F) * 2;
    if (morale > 102)
    {
        morale = 0;
    }
    if (morale != 0 && morale >= 100 - f.hit_points * 100 / f.numbers.max_hit_points)
    {
        return false;
    }
    const int group = hostile_hit_points_percent_;
    if (group != 0 && group >= 100 - static_cast<int>(morale_threshold_) &&
        f.side == Side::opposition)
    {
        return false;
    }
    if (highest_enemy_movement(f) <= f.numbers.movement)
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
    const bool away =
        !watched || fastest < f.numbers.movement || (fastest == f.numbers.movement && roll(2) == 1);
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
            f.condition == Condition::surrendered || f.condition == Condition::gone)
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
    // Guard needs a weapon that strikes next to it (combat.pas, DoneMenu).
    if (!actor->numbers.missile || actor->numbers.missile->throwable)
    {
        offer("guard");
    }
    offer("delay");
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
    const bool offered = std::any_of(
        legal.begin(), legal.end(),
        [&command](const rules::Command &c)
        {
            return c.revision == command.revision && c.actor == command.actor &&
                   c.verb == command.verb && c.target == command.target &&
                   ((c.verb != "move" && c.verb != "flee") || c.destination == command.destination);
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
    else if (command.verb == "flee")
    {
        attempt_flee(actor);
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
    if (outcome_ == rules::Outcome::ongoing &&
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
        morale_threshold_, hostile_hit_points_percent_, fighters_.size());
    for (const auto &f : fighters_)
    {
        out(f.id, f.name, f.definition, static_cast<unsigned>(f.side), f.cell.x, f.cell.y,
            f.footprint.width, f.footprint.height, f.facing_left, f.numbers.encode(), f.hit_points,
            static_cast<unsigned>(f.condition), f.overkill, f.initiative, f.half_moves,
            f.attacks_left[0], f.attacks_left[1], f.surprised, f.attack_phase, f.sweep_budget,
            f.direction, f.flank_count, f.flank_spread, f.guarding, f.fleeing, f.turned,
            f.tried_turning);
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
        if (f.condition > Condition::gone || f.attack_phase < 0 || f.attack_phase > 1 ||
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
            f.numbers.morale = non_player + threshold;
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
