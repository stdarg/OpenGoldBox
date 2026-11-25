#include "combat_session_impl.h"

// The Session's forecast of a party member's area spell, and the aim it
// chooses by it (combat_session_impl.h).

namespace opengold::original::detail
{

// A party member's area spells (docs/ORIGINAL-RULES.md, Party area spells;
// the user's decision, 2026-11-21): a computer-run member of the party, or a
// creature the computer plays on the party's side, weighs what an area spell
// would likely do to its own side against what it would likely do to the
// enemies, and aims it where that is best (rules::judge_area_spell).
// Monsters and the creatures against the party keep the original's AI.
class Session::Forecast
{
  public:
    Forecast(const Session &session, const Fighter &caster, int number, int level)
        : session_(session), caster_(caster), number_(number), level_(level),
          spell_(*session.game_spell(number))
    {
        int enemies = 0, levels = 0;
        for (const auto &f : session_.fighters_)
        {
            if (f.able() && f.side != caster_.side && !is_ally(f))
            {
                enemy_ids_.push_back(f.id);
                ++enemies;
                levels += f.numbers.level + 1;
            }
        }
        average_enemy_level_ = enemies > 0 ? static_cast<double>(levels) / enemies : 1.0;
    }

    // The spells the forecast weighs: those that reach an area around their
    // aim or along a path, whoever stands there.
    [[nodiscard]] static bool weighs(int number) noexcept
    {
        static constexpr std::array<int, 23> area_spells{21,  34,  41,  46,  47,  51,  55,  60,
                                                         64,  178, 182, 184, 187, 191, 192, 198,
                                                         301, 310, 315, 320, 323, 324, 325};
        return std::ranges::find(area_spells, number) != area_spells.end();
    }

    [[nodiscard]] rules::AreaOutlook outlook(Cell aim)
    {
        const auto reach = reach_at(aim);
        return summed(reach, scenarios(reach));
    }

    // The best of the aims the rules let the caster give the spell, among
    // those the limits let it cast at; none when no aim passes.
    [[nodiscard]] std::optional<Cell> best_aim(const rules::AreaSpellLimits &limits)
    {
        std::optional<Cell> best;
        rules::AreaSpellJudgement best_judgement;
        std::map<std::vector<std::int64_t>, rules::AreaSpellJudgement> judged;
        for (const auto aim : aims())
        {
            const auto reach = reach_at(aim);
            if (reach.hits.empty() && reach.downed.empty())
            {
                continue;
            }
            auto [found, unjudged] = judged.try_emplace(reach.key());
            if (unjudged)
            {
                found->second = rules::judge_area_spell(summed(reach, scenarios(reach)), limits);
            }
            const auto &judgement = found->second;
            if (judgement.cast && (!best || rules::better_area_aim(judgement, best_judgement)))
            {
                best = aim;
                best_judgement = judgement;
            }
        }
        return best;
    }

  private:
    // How the caster knows a creature: of its own side or not; its Hit
    // Points, an enemy's estimated from its Hit Dice (4.5 a die, 2 below one)
    // less what it has visibly lost, at least 1; an enemy's worth by its
    // level against the fight's enemies' average (each level plus 1); and
    // whether an able enemy of the caster's stands next to it.
    struct Known
    {
        bool ally{};
        int hit_points{}, max_hit_points{};
        double weight{1.0};
        bool enemy_beside{};
    };

    // One way the spell reaches a creature: a burst or path takes it (0), a
    // bolt strikes it once (1, Javelin of Lightning's mark 2), or a meteor
    // strikes it on its way (3) or bursts on it (4). A creature may be
    // reached several ways.
    struct Hit
    {
        EntityId id{};
        int way{};
    };

    // Whom the spell reaches at an aim, in the rules' order, and the members
    // already down lying in its area.
    struct Reach
    {
        Cell aim;
        std::vector<Hit> hits;
        std::vector<EntityId> downed;

        // Aims that reach alike fare alike.
        [[nodiscard]] std::vector<std::int64_t> key() const
        {
            std::vector<std::int64_t> key;
            for (const auto &hit : hits)
            {
                key.push_back(static_cast<std::int64_t>(hit.id) * 8 + hit.way);
            }
            key.push_back(-1);
            key.insert(key.end(), downed.begin(), downed.end());
            return key;
        }
    };

    // What one creature faces in one way the spell's shared dice fall.
    struct Fate
    {
        std::vector<double> loss{1.0};
        double disabled{}, helps_lost{}, hindrances_lost{}, hindrances_gained{}, freed{};
    };

    // One way the spell's shared dice fall, its chance, and the fate of each
    // creature it reaches then, each apart from the others'.
    struct Scenario
    {
        double chance{1.0};
        std::map<EntityId, Fate> fates;
    };

    static constexpr int burst_way = 0, bolt_way = 1, mark_way = 2, meteor_path_way = 3,
                         meteor_burst_way = 4;

    const Session &session_;
    const Fighter &caster_;
    int number_, level_;
    const Spell &spell_;
    std::vector<EntityId> enemy_ids_;
    double average_enemy_level_{1.0};
    std::map<EntityId, Known> known_;
    std::map<std::tuple<EntityId, int, bool, int, int, int, int>, std::vector<double>> harm_;

    // The side a creature fights for at heart: a charmed one its own.
    [[nodiscard]] bool is_ally(const Fighter &f) const
    {
        return (f.has(effect::charm) ? f.own_side : f.side) == caster_.side;
    }

    [[nodiscard]] const Known &known(const Fighter &f)
    {
        if (const auto found = known_.find(f.id); found != known_.end())
        {
            return found->second;
        }
        Known known;
        known.ally = is_ally(f);
        known.hit_points = f.hit_points;
        known.max_hit_points = f.numbers.max_hit_points;
        if (!known.ally)
        {
            const int level = f.numbers.level;
            known.max_hit_points = level >= 1 ? (9 * level + 1) / 2 : 2;
            const int lost = std::max(f.numbers.max_hit_points - f.hit_points, 0);
            known.hit_points = std::max(known.max_hit_points - lost, 1);
            known.weight = (level + 1) / average_enemy_level_;
        }
        known.enemy_beside = known.ally && std::ranges::any_of(session_.fighters_,
                                                               [&](const Fighter &o)
                                                               {
                                                                   return o.able() &&
                                                                          o.side != caster_.side &&
                                                                          adjacent(o, f);
                                                               });
        return known_.emplace(f.id, known).first->second;
    }

    // A creature's most Hit Points as the caster knows them.
    [[nodiscard]] auto known_most_hit_points()
    {
        return [this](const Fighter &f)
        {
            return known(f).max_hit_points;
        };
    }

    // The largest loss a creature's outlook tells apart: all it has.
    [[nodiscard]] int cap_of(const Fighter &f)
    {
        const auto &k = known(f);
        return std::max({k.hit_points, k.max_hit_points, 1});
    }

    // A loss of `chances` kept to a creature's cap, more lumped with it.
    [[nodiscard]] std::vector<double> capped(std::vector<double> chances, int cap) const
    {
        const auto size = static_cast<std::size_t>(cap) + 1;
        if (chances.size() > size)
        {
            for (std::size_t lost = size; lost < chances.size(); ++lost)
            {
                chances[size - 1] += chances[lost];
            }
            chances.resize(size);
        }
        return chances;
    }

    // The loss a non-weapon harm of `damage` does a creature once its
    // defenses weigh it, each way they can go (nonweapon_damage).
    [[nodiscard]] const std::vector<double> &harm(const Fighter &target, int damage,
                                                  int save_result, bool saved, int element,
                                                  int spell, int damage_dice)
    {
        const auto key =
            std::tuple{target.id, damage, saved, save_result, element, spell, damage_dice};
        if (const auto found = harm_.find(key); found != harm_.end())
        {
            return found->second;
        }
        const int cap = cap_of(target);
        std::vector<double> chances(static_cast<std::size_t>(cap) + 1, 0.0);
        if (absorbs_harm(target, element))
        {
            chances[0] = 1;
        }
        else
        {
            each_way<int>(
                [&](auto &&dice)
                {
                    return session_
                        .defended_harm(target, damage, save_result, saved, element, level_, spell,
                                       false, damage_dice, dice)
                        .damage;
                },
                [&](double way, int taken)
                {
                    chances[static_cast<std::size_t>(std::clamp(taken, 0, cap))] += way;
                });
        }
        return harm_.emplace(key, std::move(chances)).first->second;
    }

    // The loss a harm of `damage` does a creature that saves with this
    // chance, which halves or negates it as `save_result` says.
    [[nodiscard]] std::vector<double> struck(const Fighter &target, int damage, double save,
                                             int save_result, int element, int spell,
                                             int damage_dice)
    {
        auto loss = harm(target, damage, save_result, false, element, spell, damage_dice);
        if (save_result == 0 || save <= 0)
        {
            return loss;
        }
        const auto &saved = harm(target, damage, save_result, true, element, spell, damage_dice);
        for (std::size_t lost = 0; lost < loss.size(); ++lost)
        {
            loss[lost] = (1 - save) * loss[lost] + save * saved[lost];
        }
        return loss;
    }

    // The same for a harm of its own dice.
    [[nodiscard]] std::vector<double> struck_by_dice(const Fighter &target, const SpellDice &dice,
                                                     double save, int save_result, int element,
                                                     int spell)
    {
        std::vector<double> loss(static_cast<std::size_t>(cap_of(target)) + 1, 0.0);
        const auto totals = dice_chances(dice.count, dice.sides, dice.bonus);
        for (std::size_t total = 0; total < totals.size(); ++total)
        {
            if (totals[total] <= 0)
            {
                continue;
            }
            const auto one =
                struck(target, static_cast<int>(total), save, save_result, element, spell, 0);
            for (std::size_t lost = 0; lost < loss.size(); ++lost)
            {
                loss[lost] += totals[total] * one[lost];
            }
        }
        return loss;
    }

    // The chance the target's immunities nullify an effect the spell adds
    // (refuses_effect), at a caster level.
    [[nodiscard]] double refusal(const Fighter &target, int code, int caster_level,
                                 const Spell &known) const
    {
        double chance = 0;
        each_way<bool>(
            [&](auto &&dice)
            {
                return session_.refuses_effect(target, code, 0, caster_level, &known, dice);
            },
            [&](double way, bool refused)
            {
                chance += refused ? way : 0;
            });
        return chance;
    }

    // The chance an effect the spell adds takes the target (cast_generic): a
    // save of this modifier, where the spell allows one, refuses it when the
    // save negates; then its immunities may.
    [[nodiscard]] double effect_takes(const Fighter &target, int code, int save_modifier) const
    {
        const double save = spell_.save_result == 1 ? session_.save_chance(target, spell_.save_type,
                                                                           save_modifier, &caster_)
                                                    : 0.0;
        return (1 - save) * (1 - refusal(target, code, level_, spell_));
    }

    // A fate that slays with this chance.
    [[nodiscard]] Fate slain(const Fighter &target, double chance)
    {
        Fate fate;
        fate.loss.assign(static_cast<std::size_t>(cap_of(target)) + 1, 0.0);
        fate.loss.front() = 1 - chance;
        fate.loss.back() += chance;
        return fate;
    }

    [[nodiscard]] static Fate disabled_by(double chance)
    {
        Fate fate;
        fate.disabled = chance;
        return fate;
    }

    // Two fates of one creature, falling apart from each other.
    [[nodiscard]] Fate joined(const Fate &first, const Fate &second, int cap) const
    {
        Fate fate;
        fate.loss = capped(sum_chances(first.loss, second.loss), cap);
        fate.disabled = 1 - (1 - first.disabled) * (1 - second.disabled);
        fate.helps_lost = first.helps_lost + second.helps_lost;
        fate.hindrances_lost = first.hindrances_lost + second.hindrances_lost;
        fate.hindrances_gained = first.hindrances_gained + second.hindrances_gained;
        fate.freed = std::max(first.freed, second.freed);
        return fate;
    }

    void add_fate(Scenario &scenario, const Fighter &target, Fate fate)
    {
        if (auto found = scenario.fates.find(target.id); found != scenario.fates.end())
        {
            found->second = joined(found->second, fate, cap_of(target));
            return;
        }
        scenario.fates.emplace(target.id, std::move(fate));
    }

    // The aims the rules let the caster give the spell (aiming_commands):
    // for one that reaches around a square, each open square in range and
    // sight; for Javelin of Lightning, each creature it may single out.
    [[nodiscard]] std::vector<Cell> aims() const
    {
        std::vector<Cell> aims;
        const int area = spell_.area & 0x0F;
        if (area < 8 || area > 14)
        {
            for (const auto &f : session_.fighters_)
            {
                if (session_.reachable_by_spell(caster_, f, number_))
                {
                    aims.push_back(f.cell);
                }
            }
            return aims;
        }
        const auto &board = session_.board_;
        const int range = spell_range(spell_, level_);
        for (int y = 0; y < board.height; ++y)
        {
            for (int x = 0; x < board.width; ++x)
            {
                const Cell cell{x, y};
                if (!rules::blocks_movement(board.at(cell)) &&
                    doubled_distance(caster_.cell, cell) / 2 <= range &&
                    clear_line(board, caster_.cell, cell))
                {
                    aims.push_back(cell);
                }
            }
        }
        return aims;
    }

    // The targets a spell reaching around its aim is given (the aim
    // command's), less those that refuse it (resolve_spell).
    [[nodiscard]] std::vector<EntityId> burst_targets(Cell aim) const
    {
        auto targets = session_.within(aim, spell_.area & 7);
        std::erase_if(targets,
                      [&](EntityId id)
                      {
                          return session_.refuses_spell(*session_.find(id), number_);
                      });
        return targets;
    }

    [[nodiscard]] Reach reach_at(Cell aim) const
    {
        Reach reach{aim, {}, {}};
        const auto &session = session_;
        const auto take = [&](const std::vector<EntityId> &ids, int way)
        {
            for (const auto id : ids)
            {
                reach.hits.push_back({id, way});
            }
        };
        const auto standing_on = [&](const std::vector<Cell> &cells, int way)
        {
            std::vector<EntityId> found;
            for (const auto cell : cells)
            {
                if (const auto id = session.standing_at(cell);
                    id && std::ranges::find(found, id) == found.end())
                {
                    found.push_back(id);
                }
            }
            take(found, way);
        };
        const auto downed = [&](const auto &lies_in_area)
        {
            for (const auto &f : session.fighters_)
            {
                if (f.downed() && is_ally(f) && lies_in_area(f) &&
                    std::ranges::find(reach.downed, f.id) == reach.downed.end())
                {
                    reach.downed.push_back(f.id);
                }
            }
        };
        const auto downed_in_burst = [&](Cell at, int radius)
        {
            downed(
                [&](const Fighter &f)
                {
                    return in_burst(session.burst_gap(at, f), radius);
                });
        };
        const auto downed_on = [&](const std::vector<Cell> &cells)
        {
            downed(
                [&](const Fighter &f)
                {
                    return std::ranges::any_of(squares_of(f),
                                               [&](Cell square)
                                               {
                                                   return std::ranges::find(cells, square) !=
                                                          cells.end();
                                               });
                });
        };
        const int radius = spell_.area & 7;
        switch (number_)
        {
        case 47:
        case 64:
            take(session.fireball_reach(burst_targets(aim), aim), burst_way);
            downed_in_burst(aim, session.outdoors_ ? 2 : radius);
            break;
        case 21:
            take(session.within(aim, radius), burst_way);
            downed_in_burst(aim, radius);
            break;
        case 184:
        case 192:
        case 198:
        {
            const int length = number_ == 184 ? 6 : number_ == 192 ? cone_length(level_) : 3;
            const int width = number_ == 184 ? 3 : number_ == 192 ? 2 : 1;
            take(session.path_area(caster_, aim, length, width), burst_way);
            downed_on(session.path_squares(caster_, aim, length, width));
            break;
        }
        case 34:
        case 191:
        case 301:
        {
            const auto kind = number_ == 34    ? CloudKind::stinking
                              : number_ == 191 ? CloudKind::deadly
                                               : CloudKind::blades;
            const auto cloud = session.cloud_over(caster_.id, 0, aim, kind);
            const auto squares = cloud.squares();
            std::vector<Cell> covered;
            for (std::size_t n = 0; n < squares.size(); ++n)
            {
                if (cloud.covers[n])
                {
                    covered.push_back(squares[n]);
                }
            }
            standing_on(covered, burst_way);
            downed_on(covered);
            break;
        }
        case 51:
        case 60:
        {
            const bool javelin = number_ == 60;
            if (const auto id = session.standing_at(aim))
            {
                reach.hits.push_back({id, javelin ? mark_way : bolt_way});
            }
            std::vector<Cell> line;
            Cell from = aim;
            line.push_back(aim);
            session.bolt_walk(caster_.cell, aim, javelin ? 3 : session.bolt_length(), !javelin,
                              [&](Cell cell)
                              {
                                  if (const auto id = session.standing_at(cell))
                                  {
                                      reach.hits.push_back({id, bolt_way});
                                  }
                                  LineWalk walk(from, cell);
                                  while (walk.next())
                                  {
                                      line.push_back(walk.at);
                                  }
                                  from = cell;
                                  return session.turns_bolt(cell);
                              });
            downed_on(line);
            break;
        }
        case 324:
            for (const auto offset : meteor_landings)
            {
                const auto fall = session.meteor_fall(caster_, aim, offset);
                take(fall.struck, meteor_path_way);
                take(session.within(fall.landing, 1), meteor_burst_way);
                std::vector<Cell> path;
                LineWalk walk(caster_.cell, fall.landing);
                while (walk.next())
                {
                    path.push_back(walk.at);
                }
                downed_on(path);
                downed_in_burst(fall.landing, 1);
            }
            break;
        default:
            take(burst_targets(aim), burst_way);
            downed_in_burst(aim, radius);
            break;
        }
        return reach;
    }

    [[nodiscard]] const Fighter &fighter(EntityId id) const
    {
        return *session_.find(id);
    }

    // The ways the spell's shared dice fall at an aim, and each creature's
    // fate in each, as its resolver plays it.
    [[nodiscard]] std::vector<Scenario> scenarios(const Reach &reach)
    {
        switch (number_)
        {
        case 47:
        case 64:
        case 187:
        case 192:
        case 198:
        case 315:
            return damage_scenarios(reach);
        case 51:
        case 60:
            return bolt_scenarios(reach);
        case 324:
            return meteor_scenarios(reach);
        case 21:
            return sleep_scenarios(reach);
        case 182:
            return confusion_scenarios(reach);
        case 310:
            return death_spell_scenarios(reach);
        default:
            return {fixed_scenario(reach)};
        }
    }

    // Spells of damage rolled once for all they reach (resolve_spell).
    [[nodiscard]] std::vector<Scenario> damage_scenarios(const Reach &reach)
    {
        struct Roll
        {
            double chance{};
            int total{}, dice{};
        };

        std::vector<Roll> rolls;
        // A necklace's missile (1d3) sets its dice; Delayed Blast
        // Fireball's are rolled one at a time, which the kit does not
        // weigh by.
        for (int missile = 1; missile <= (number_ == 64 ? 3 : 1); ++missile)
        {
            const auto dice = shared_dice(number_, level_, missile);
            const auto totals = dice_chances(dice.count, dice.sides, dice.bonus);
            const double weight = number_ == 64 ? 1.0 / 3 : 1.0;
            for (std::size_t total = 0; total < totals.size(); ++total)
            {
                if (totals[total] > 0)
                {
                    rolls.push_back({weight * totals[total], static_cast<int>(total),
                                     number_ == delayed_blast_spell ? 0 : dice.count});
                }
            }
        }
        const int element = number_ == 187   ? session_.ice_storm_element()
                            : number_ == 192 ? 10
                            : number_ == 198 ? 8
                                             : 9;
        // A blast takes a plant whole, with no save.
        constexpr int plant = 18;
        std::vector<int> save_results;
        std::vector<double> saves;
        for (const auto &hit : reach.hits)
        {
            const auto &target = fighter(hit.id);
            const bool whole = number_ == 198 && target.numbers.creature_type == plant;
            save_results.push_back(whole ? 0 : spell_.save_result);
            saves.push_back(
                save_results.back() != 0
                    ? session_.save_chance(target, spell_.save_type, 0, &caster_, element)
                    : 0.0);
        }
        std::vector<Scenario> scenarios;
        for (const auto &roll : rolls)
        {
            Scenario scenario{roll.chance, {}};
            for (std::size_t n = 0; n < reach.hits.size(); ++n)
            {
                const auto &target = fighter(reach.hits[n].id);
                Fate fate;
                fate.loss = struck(target, roll.total, saves[n], save_results[n], element, number_,
                                   roll.dice);
                add_fate(scenario, target, std::move(fate));
            }
            scenarios.push_back(std::move(scenario));
        }
        return scenarios;
    }

    // Lightning Bolt's one roll at its aim and each square its bolt
    // strikes, or Javelin of Lightning's 1d6+20 at its mark and 20 along its
    // bolt; each strike saved for half against spells (lightning_at).
    [[nodiscard]] std::vector<Scenario> bolt_scenarios(const Reach &reach)
    {
        constexpr int bolt_element = 12, bolt_spell = 51;
        std::vector<std::pair<double, int>> rolls;
        int damage_dice = 0;
        if (number_ == 60)
        {
            rolls.emplace_back(1.0, 20);
        }
        else
        {
            const auto dice = shared_dice(number_, level_);
            damage_dice = dice.count;
            const auto totals = dice_chances(dice.count, dice.sides, dice.bonus);
            for (std::size_t total = 0; total < totals.size(); ++total)
            {
                if (totals[total] > 0)
                {
                    rolls.emplace_back(totals[total], static_cast<int>(total));
                }
            }
        }
        std::vector<double> saves;
        for (const auto &hit : reach.hits)
        {
            saves.push_back(session_.save_chance(fighter(hit.id), 4, 0, nullptr, bolt_element));
        }
        std::vector<Scenario> scenarios;
        for (const auto [chance, total] : rolls)
        {
            Scenario scenario{chance, {}};
            for (std::size_t n = 0; n < reach.hits.size(); ++n)
            {
                const auto &target = fighter(reach.hits[n].id);
                Fate fate;
                fate.loss =
                    reach.hits[n].way == mark_way
                        ? struck_by_dice(target, {1, 6, 20}, saves[n], 2, bolt_element, bolt_spell)
                        : struck(target, total, saves[n], 2, bolt_element, bolt_spell, damage_dice);
                add_fate(scenario, target, std::move(fate));
            }
            scenarios.push_back(std::move(scenario));
        }
        return scenarios;
    }

    // Meteor Swarm's meteors, each striking for its own 10d4 on its way and
    // bursting on those around where it lands, saved for half
    // (cast_meteor_swarm).
    [[nodiscard]] std::vector<Scenario> meteor_scenarios(const Reach &reach)
    {
        constexpr int fire_and_magic = 9;
        Scenario scenario;
        for (const auto &hit : reach.hits)
        {
            const auto &target = fighter(hit.id);
            const bool burst = hit.way == meteor_burst_way;
            const double save =
                burst ? session_.save_chance(target, 4, 0, &caster_, fire_and_magic) : 0.0;
            Fate fate;
            fate.loss =
                struck_by_dice(target, meteor_dice, save, burst ? 2 : 0, fire_and_magic, number_);
            add_fate(scenario, target, std::move(fate));
        }
        return {scenario};
    }

    // Sleep's 4d4 levels, spent on the nearest first (sleep_takes), each
    // asleep unless its immunities refuse it.
    [[nodiscard]] std::vector<Scenario> sleep_scenarios(const Reach &reach)
    {
        const auto pools = dice_chances(4, 4, 0);
        std::vector<Scenario> scenarios;
        for (std::size_t pool = 0; pool < pools.size(); ++pool)
        {
            if (pools[pool] <= 0)
            {
                continue;
            }
            Scenario scenario{pools[pool], {}};
            for (const auto id : session_.sleep_takes(reach.aim, static_cast<int>(pool)))
            {
                const auto &target = fighter(id);
                add_fate(scenario, target,
                         disabled_by(1 - refusal(target, effect::sleep, level_, spell_)));
            }
            scenarios.push_back(std::move(scenario));
        }
        return scenarios;
    }

    // Confusion takes at most 2d8 of its targets, the nearest first.
    [[nodiscard]] std::vector<Scenario> confusion_scenarios(const Reach &reach)
    {
        constexpr int confused = 235;
        const auto counts = dice_chances(2, 8, 0);
        std::vector<Scenario> scenarios;
        for (std::size_t count = 0; count < counts.size(); ++count)
        {
            if (counts[count] <= 0)
            {
                continue;
            }
            Scenario scenario{counts[count], {}};
            for (std::size_t n = 0; n < reach.hits.size() && n < count; ++n)
            {
                const auto &target = fighter(reach.hits[n].id);
                add_fate(scenario, target, disabled_by(effect_takes(target, confused, 0)));
            }
            scenarios.push_back(std::move(scenario));
        }
        return scenarios;
    }

    // Death Spell's 4d20 x 2 to spend on its targets by their levels
    // (death_spell_takes), weighing an enemy's most Hit Points as the
    // caster knows them.
    [[nodiscard]] std::vector<Scenario> death_spell_scenarios(const Reach &reach)
    {
        std::vector<EntityId> targets;
        for (const auto &hit : reach.hits)
        {
            targets.push_back(hit.id);
        }
        const auto budgets = dice_chances(4, 20, 0);
        std::vector<Scenario> scenarios;
        for (std::size_t budget = 0; budget < budgets.size(); ++budget)
        {
            if (budgets[budget] <= 0)
            {
                continue;
            }
            Scenario scenario{budgets[budget], {}};
            for (const auto id : session_.death_spell_takes(targets, static_cast<int>(budget) * 2,
                                                            known_most_hit_points()))
            {
                add_fate(scenario, fighter(id), slain(fighter(id), 1.0));
            }
            scenarios.push_back(std::move(scenario));
        }
        return scenarios;
    }

    // The spells with no dice shared among those they reach.
    [[nodiscard]] Scenario fixed_scenario(const Reach &reach)
    {
        Scenario scenario;
        std::vector<EntityId> targets;
        for (const auto &hit : reach.hits)
        {
            targets.push_back(hit.id);
        }
        const auto fate_of = [&](EntityId id, Fate fate)
        {
            add_fate(scenario, fighter(id), std::move(fate));
        };
        switch (number_)
        {
        case 34:
            for (const auto id : targets)
            {
                fate_of(id, disabled_by(sickening(fighter(id))));
            }
            break;
        case 191:
            for (const auto id : targets)
            {
                fate_of(id, poisoning(fighter(id)));
            }
            break;
        case 301:
            for (const auto id : targets)
            {
                fate_of(id, cutting(fighter(id)));
            }
            break;
        case 55:
            // Slow: up to the caster's level of its enemies, a hasted one
            // only losing its Haste (resolve_spell).
            for (const auto id : session_.haste_slow_takes(caster_, number_, level_, targets))
            {
                const auto &target = fighter(id);
                Fate fate;
                if (target.has(effect::hasted))
                {
                    fate.helps_lost = 1;
                }
                else
                {
                    fate.disabled = effect_takes(target, effect::slowed, 0);
                }
                fate_of(id, std::move(fate));
            }
            break;
        case 178:
            // Entangle holds those who fail their saves, out of doors only.
            for (const auto id : session_.outdoors_ ? targets : std::vector<EntityId>{})
            {
                const auto &target = fighter(id);
                const double save = session_.save_chance(target, 4, 0, &caster_);
                fate_of(id, disabled_by((1 - save) *
                                        (1 - refusal(target, effect::held_still, level_, spell_))));
            }
            break;
        case 184:
            // Fear: those who fail their saves flee.
            for (const auto id : targets)
            {
                const auto &target = fighter(id);
                const double save = session_.save_chance(target, 4, 0, &caster_);
                fate_of(id, disabled_by((1 - save) *
                                        (1 - refusal(target, effect::fear, level_, spell_))));
            }
            break;
        case 320:
            for (const auto [id, take] : session_.mass_charm_takes(caster_, level_, targets))
            {
                if (take == MassCharmTake::tried)
                {
                    const auto &target = fighter(id);
                    const double save = session_.save_chance(target, 4, -2, &caster_);
                    fate_of(id, disabled_by((1 - save) *
                                            (1 - refusal(target, effect::charm, level_, spell_))));
                }
            }
            break;
        case 323:
            for (const auto id : session_.blinded_by_word(targets, known_most_hit_points()))
            {
                Fate fate;
                fate.hindrances_gained = 1 - refusal(fighter(id), effect::blind, 0, spell_);
                fate_of(id, std::move(fate));
            }
            break;
        case 325:
            for (const auto id : session_.slain_by_word(targets, known_most_hit_points()))
            {
                const auto &target = fighter(id);
                const auto *images = target.find_effect(effect::mirror_image);
                const double turned =
                    target.has(effect::iron_golem) ? 1.0
                    : images ? static_cast<double>(images->strength) / (images->strength + 1)
                             : 0.0;
                fate_of(id, slain(target, 1 - turned));
            }
            break;
        case 41:
        case 46:
            for (const auto id : targets)
            {
                fate_of(id, dispelling(fighter(id)));
            }
            break;
        default:
            break;
        }
        return scenario;
    }

    // The chance a stinking cloud laid on a creature leaves it helpless with
    // nausea (breathe_stinking_cloud, breathe_darkness_stench).
    [[nodiscard]] double sickening(const Fighter &f) const
    {
        const bool spared =
            session_.darkness()
                ? f.numbers.undead_level > 0 || f.has(effect::nausea) ||
                      (f.numbers.traits & (trait::unliving | trait::poison_proof)) != 0
                : f.has(effect::coughing) || f.has(effect::nausea) || f.has(effect::animated) ||
                      f.has(111) || f.has(125) || session_.breathes_clouds_freely(f);
        if (spared)
        {
            return 0;
        }
        const double save = session_.save_chance(f, 0, 0);
        return (1 - save) * (1 - refusal(f, effect::nausea, 0, *session_.game_spell(34)));
    }

    // What a cloudkill laid on a creature does (breathe_cloudkill,
    // breathe_darkness_cloudkill).
    [[nodiscard]] Fate poisoning(const Fighter &f)
    {
        const int level = f.numbers.level;
        if (session_.darkness())
        {
            if ((f.numbers.traits & trait::poison_proof) != 0)
            {
                return {};
            }
            if (level > 6)
            {
                constexpr int magical_poison = 0x88;
                Fate fate;
                fate.loss = struck_by_dice(f, {1, 10, 0}, 0, 0, magical_poison, number_);
                return fate;
            }
        }
        else if (level > 6)
        {
            return {};
        }
        const double save = level >= 5 ? session_.save_chance(f, 0, level == 5 ? -4 : 0) : 0.0;
        return slain(f, 1 - save);
    }

    // What a Blade Barrier rising on a creature does: unless magic
    // resistance turns it or a save, 8d8 (cut_by_blades).
    [[nodiscard]] Fate cutting(const Fighter &f)
    {
        double resisted = 0;
        if (resists_magic_at_all(f))
        {
            each_way<bool>(
                [&](auto &&dice)
                {
                    return magic_resists(f, level_, dice(100));
                },
                [&](double way, bool resists)
                {
                    resisted += resists ? way : 0;
                });
        }
        const double cut = (1 - resisted) * (1 - session_.save_chance(f, 4, 0, &caster_));
        Fate fate;
        fate.loss = struck_by_dice(f, blade_dice, 0, 0, 0, number_);
        for (auto &chance : fate.loss)
        {
            chance *= cut;
        }
        fate.loss.front() += 1 - cut;
        return fate;
    }

    // What Dispel Magic likely takes from a creature (dispel): each effect
    // it may take goes by its own chance, counted as helping or hindering
    // the creature, and the creature is freed once every effect that
    // disables it goes.
    [[nodiscard]] Fate dispelling(const Fighter &f) const
    {
        Fate fate;
        bool disabled = false;
        double freed = 1;
        for (const auto &e : f.numbers.effects)
        {
            const bool disables =
                std::ranges::find(disabling_effects, e.code) != disabling_effects.end();
            const double chance =
                session_.dispellable(e)
                    ? std::clamp(session_.dispel_chance(e, level_), 0, 100) / 100.0
                    : 0.0;
            if (disables)
            {
                disabled = true;
                freed *= chance;
            }
            else if (std::ranges::find(helping_effects, e.code) != helping_effects.end())
            {
                fate.helps_lost += chance;
            }
            else if (std::ranges::find(hindering_effects, e.code) != hindering_effects.end())
            {
                fate.hindrances_lost += chance;
            }
        }
        fate.freed = disabled ? freed : 0.0;
        return fate;
    }

    // Each creature's outlook from the spell's scenarios: the chances of each
    // fate weighed together, and the chance every able enemy goes down in
    // the same fall of the dice. A member already down in the area counts as
    // slain.
    [[nodiscard]] rules::AreaOutlook summed(const Reach &reach,
                                            const std::vector<Scenario> &scenarios)
    {
        rules::AreaOutlook outlook;
        std::vector<EntityId> ids;
        for (const auto &hit : reach.hits)
        {
            if (std::ranges::find(ids, hit.id) == ids.end())
            {
                ids.push_back(hit.id);
            }
        }
        for (const auto id : ids)
        {
            const auto &target = fighter(id);
            const auto &k = known(target);
            rules::CreatureOutlook creature;
            creature.id = id;
            creature.ally = k.ally;
            creature.hit_points = k.hit_points;
            creature.max_hit_points = k.max_hit_points;
            creature.weight = k.weight;
            creature.enemy_beside = k.enemy_beside;
            creature.loss.assign(static_cast<std::size_t>(cap_of(target)) + 1, 0.0);
            for (const auto &scenario : scenarios)
            {
                const auto found = scenario.fates.find(id);
                if (found == scenario.fates.end())
                {
                    creature.loss.front() += scenario.chance;
                    continue;
                }
                const auto &fate = found->second;
                for (std::size_t lost = 0; lost < fate.loss.size(); ++lost)
                {
                    creature.loss[lost] += scenario.chance * fate.loss[lost];
                }
                creature.disabled += scenario.chance * fate.disabled;
                creature.helps_lost += scenario.chance * fate.helps_lost;
                creature.hindrances_lost += scenario.chance * fate.hindrances_lost;
                creature.hindrances_gained += scenario.chance * fate.hindrances_gained;
                creature.freed += scenario.chance * fate.freed;
            }
            outlook.creatures.push_back(std::move(creature));
        }
        for (const auto id : reach.downed)
        {
            const auto &target = fighter(id);
            rules::CreatureOutlook creature;
            creature.id = id;
            creature.ally = true;
            creature.hit_points = std::min(target.hit_points, 0);
            creature.max_hit_points = target.numbers.max_hit_points;
            outlook.creatures.push_back(std::move(creature));
        }
        for (const auto &scenario : scenarios)
        {
            double all_down = scenario.chance;
            for (const auto id : enemy_ids_)
            {
                const auto found = scenario.fates.find(id);
                if (found == scenario.fates.end())
                {
                    all_down = 0;
                    break;
                }
                const int hit_points = known(fighter(id)).hit_points;
                double down = found->second.disabled;
                for (auto lost = static_cast<std::size_t>(hit_points);
                     lost < found->second.loss.size(); ++lost)
                {
                    down += found->second.loss[lost];
                }
                all_down *= std::min(down, 1.0);
            }
            outlook.every_enemy_down += all_down;
        }
        return outlook;
    }
};

// Whether the computer weighs this spell's outlook for its caster: an area
// spell of a creature on the party's side.
bool Session::weighs_area_spell(const Fighter &f, int number) const
{
    return f.side == Side::party && Forecast::weighs(number);
}

// The best aim a party-side caster's outlook allows for an area spell
// (Forecast::best_aim), none when no aim passes the player's limits.
std::optional<Cell> Session::party_area_aim(const Fighter &f, int number) const
{
    const auto &known = *game_spell(number);
    Forecast forecast(*this, f, number, caster_level(f, known, game_of(identity_)));
    return forecast.best_aim(party_spell_limits_);
}

// A party-side caster's area spell goes off at its best aim; false, the
// spell given up, when no aim passes.
bool Session::aim_party_area_spell(Fighter &f)
{
    const auto aim = party_area_aim(f, f.aiming);
    if (!aim)
    {
        log({"{name} aborts the spell.", {{"name", f.name}}});
        return false;
    }
    const int area = game_spell(f.aiming)->area & 0x0F;
    resolve_spell(f,
                  area >= 8 && area <= 14 ? within(*aim, area & 7)
                                          : std::vector<EntityId>{standing_at(*aim)},
                  *aim);
    return true;
}

std::optional<rules::AreaOutlook> Session::area_outlook(EntityId caster, unsigned spell,
                                                        Cell aim) const
{
    const auto *f = find(caster);
    const int number = static_cast<int>(spell);
    const auto *known = game_spell(number);
    if (!f || !known || !Forecast::weighs(number))
    {
        return std::nullopt;
    }
    Forecast forecast(*this, *f, number, caster_level(*f, *known, game_of(identity_)));
    return forecast.outlook(aim);
}

} // namespace opengold::original::detail
