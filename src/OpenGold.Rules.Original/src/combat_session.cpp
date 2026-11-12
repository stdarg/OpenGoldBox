#include "combat_session.h"

#include "opengold/dice.h"
#include "opengold/movement_grid.h"
#include "opengold/original_rules.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// A fight under the original rules, as Pool of Radiance's combat units play it
// (docs/ORIGINAL-RULES.md, "Combat"): plan step 1, melee.
namespace opengold::original::detail
{
namespace
{

using rules::Cell;
using rules::EntityId;
using rules::Message;
using rules::Side;

// The original counts movement in half-moves: 2 a straight step, 3 a diagonal one.
constexpr rules::StepCosts half_moves{2, 3, 2, 4, 0};
constexpr std::string_view checkpoint_format = "OGORIGINALCOMBAT 1";

enum class Condition : unsigned
{
    okay,
    unconscious,
    dying,
    dead
};

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
    bool surprised{};

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

    long long number()
    {
        skip();
        long long value{};
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
        return static_cast<T>(number());
    }

    std::string text()
    {
        const auto size = static_cast<std::size_t>(number());
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
            std::uint64_t seed)
        : identity_(std::move(identity)), board_(std::move(board)), fighters_(std::move(fighters)),
          random_{seed}
    {
        rules::validate_battlefield(board_);
        log({"Combat begins.", {}});
        begin_round();
        next_actor();
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
    void melee(Fighter &attacker, Fighter &target);
    void inflict(Fighter &target, int damage);
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
        f.half_moves = 2 * f.numbers.movement;
        for (std::size_t form = 0; form < f.attacks_left.size(); ++form)
        {
            f.attacks_left[form] = attacks_this_round(f.numbers.attacks[form].rate_per_two_rounds,
                                                      static_cast<int>(round_));
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
                if (tiebreak > best_tiebreak)
                {
                    best = &f;
                    best_tiebreak = tiebreak;
                }
            }
        }
        if (best)
        {
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

void Session::end_actions(Fighter &fighter)
{
    fighter.initiative = 0;
    fighter.attacks_left = {};
    fighter.half_moves = 0;
}

// Each attack the attacker has left this round, against one target: d20
// against THAC0 minus Armor Class, then the attack form's dice, until the
// target is down (cbtacts.pas, RollAttacks).
void Session::melee(Fighter &attacker, Fighter &target)
{
    for (std::size_t form = 0; form < attacker.attacks_left.size(); ++form)
    {
        const auto &attack = attacker.numbers.attacks[form];
        for (; attacker.attacks_left[form] > 0; --attacker.attacks_left[form])
        {
            if (!target.able())
            {
                return;
            }
            const auto natural = roll(20);
            if (!attack_hits(natural, attacker.numbers.thac0, target.numbers.armor_class, 0))
            {
                log({"{attacker} misses {name}.",
                     {{"attacker", attacker.name}, {"name", target.name}}});
                continue;
            }
            int dice = 0;
            for (int die = 0; die < attack.dice; ++die)
            {
                dice += roll(std::max(attack.sides, 1));
            }
            const auto damage = hit_damage(dice, attack.modifier);
            log({"{attacker} hits {name} for {damage}.",
                 {{"attacker", attacker.name},
                  {"name", target.name},
                  {"damage", std::to_string(damage)}}});
            inflict(target, damage);
        }
    }
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
    if (!side_standing(Side::party))
    {
        outcome_ = rules::Outcome::defeat;
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
                                                 : "The party is defeated.",
             {}});
    }
}

rules::MovementGrid Session::grid_for(const Fighter &mover) const
{
    std::vector<rules::Occupant> occupants;
    for (const auto &f : fighters_)
    {
        if (f.id == mover.id || f.condition == Condition::dead)
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
        view.dead = f->condition == Condition::dead;
        view.status = f->condition == Condition::dead          ? "dead"
                      : f->condition == Condition::dying       ? "dying"
                      : f->condition == Condition::unconscious ? "unconscious"
                                                               : "";
        view.persistent = {f->condition == Condition::okay ? f->hit_points : 0,
                           f->condition == Condition::dead,
                           {},
                           {}};
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
            if (f.side != actor->side && f.able() && adjacent(*actor, f))
            {
                rules::Command melee;
                melee.revision = revision_;
                melee.actor = actor_;
                melee.target = f.id;
                melee.verb = "melee";
                melee.intent = rules::Intent::hostile;
                commands.push_back(melee);
            }
        }
    }
    rules::Command end;
    end.revision = revision_;
    end.actor = actor_;
    end.verb = "end";
    commands.push_back(end);
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
                               (c.verb != "move" || c.destination == command.destination);
                    });
    if (!offered)
    {
        return false;
    }
    auto &actor = *find(actor_);
    if (command.verb == "move")
    {
        const auto cost = grid_for(actor).reachable(actor.half_moves).cost_to(command.destination);
        actor.facing_left = command.destination.x < actor.cell.x ||
                            (command.destination.x == actor.cell.x && actor.facing_left);
        actor.half_moves -= *cost;
        actor.cell = command.destination;
    }
    else if (command.verb == "melee")
    {
        auto &target = *find(command.target);
        actor.facing_left = target.cell.x < actor.cell.x;
        melee(actor, target);
        end_actions(actor);
        settle_outcome();
    }
    else
    {
        end_actions(actor);
    }
    ++revision_;
    if (outcome_ == rules::Outcome::ongoing &&
        (actor.initiative == 0 || (actor.half_moves == 0 && actor.attacks() == 0)))
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
        fighters_.size());
    for (const auto &f : fighters_)
    {
        out(f.id, f.name, f.definition, static_cast<unsigned>(f.side), f.cell.x, f.cell.y,
            f.footprint.width, f.footprint.height, f.facing_left, f.numbers.encode(), f.hit_points,
            static_cast<unsigned>(f.condition), f.overkill, f.initiative, f.half_moves,
            f.attacks_left[0], f.attacks_left[1], f.surprised);
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
    return std::make_unique<Session>(std::move(identity), std::move(encounter.battlefield),
                                     std::move(fighters), seed);
}

std::unique_ptr<rules::CombatSession> restore_session(rules::Identity identity,
                                                      std::string_view checkpoint)
{
    return Session::restored(std::move(identity), checkpoint);
}

} // namespace opengold::original::detail
