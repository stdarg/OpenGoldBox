#include "combat_session_impl.h"

// The Session's computer play (combat_session_impl.h; combatai.pas): the
// original's AI, and Treasures' and Dark Queen's spell ratings and area aims.

namespace opengold::original::detail
{

// The original's AI, a turn (combatai.pas, CombatAiControl): the approach
// pattern, then an item, a spell begun earlier, turning undead, a spell, and
// else the fight: lock onto an enemy and move and attack until the turn is
// done. Morale is checked as every turn starts (next_actor).
void Session::ai_turn(Fighter &f, bool magic)
{
    f.fought = false;
    // Native's final Aurak form its frenzy's timer made (raw 213: the blazing
    // shape without the slain form's zap) ends its turn at once (aiturn.c).
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
        f.has(effect::aurak_invulnerable) && !f.has(effect::aurak_zap))
    {
        end_actions(f);
        return;
    }
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
    // Pools of Darkness's AI turns undead only while it does not flee
    // (combatai.pas, TurnUndeadAi).
    if (may_turn(f) && undead_in_sight &&
        !((darkness() || game_of(identity_) == GoldBoxGame::dark_queen_of_krynn) && f.fleeing))
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
        const auto *taunt = f.find_effect(effect::kender_taunted);
        const auto yeller = taunt ? static_cast<EntityId>(taunt->strength) : EntityId{};
        if (const auto target = std::ranges::find(candidates, yeller); target != candidates.end())
        {
            std::iter_swap(candidates.begin(), target);
        }
        for (int tries = 0; tries < 20 && !candidates.empty(); ++tries)
        {
            auto &pick = candidates[tries == 0 && yeller != 0 && candidates.front() == yeller
                                        ? 0
                                        : static_cast<std::size_t>(
                                              roll(static_cast<int>(candidates.size())) - 1)];
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
    // Moving as the original does, it must leave some movement, except in Pool
    // of Radiance, whose computer spends its last half-moves.
    const auto grid = grid_for(f);
    const auto price = grid.step_price(f.cell, moved.cell);
    const bool keeps_some =
        original_movement() && game_of(identity_) != GoldBoxGame::pool_of_radiance;
    const bool affordable =
        price && (keeps_some ? price->need < f.half_moves : price->need <= f.half_moves);
    if (!affordable || !grid.can_stop_at(moved.cell))
    {
        return false;
    }
    if (secret())
    {
        // A stinking cloud's save is rolled as the step is taken (ai_step).
        return cloud_balk(f, moved) != CloudBalk::always;
    }
    if (darkness())
    {
        return !darkness_balks(f, moved);
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

// Pools of Darkness's AI never steps into a cloud that would harm it, with no
// save and whatever its movement (combatai.pas, AiCanStep): a stinking cloud
// unless it coughs already, a Minor Globe guards it (its effect 63) or its
// record calls it unliving or proof against poison; a cloudkill unless a
// Globe of Invulnerability guards it (57) or it is proof against poison; and
// Blade Barrier's blades always. One turned to flee steps anywhere.
bool Session::darkness_balks(const Fighter &f, const Fighter &moved) const
{
    if (f.turned)
    {
        return false;
    }
    const auto game = game_of(identity_);
    const int traits = f.numbers.traits;
    const bool poison_proof = (traits & trait::poison_proof) != 0;
    const bool breathes_stench = f.has(effect::coughing) || f.has(canonical_effect(63, game)) ||
                                 poison_proof || (traits & trait::unliving) != 0;
    const bool breathes_poison = f.has(canonical_effect(57, game)) || poison_proof;
    return (in_cloud(moved, CloudKind::stinking) && !breathes_stench) ||
           (in_cloud(moved, CloudKind::deadly) && !breathes_poison) ||
           in_cloud(moved, CloudKind::blades);
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
        // The party's enemies run along its facing, its own side the other
        // way (combatai.pas, AiMoveStep); the 256-color engine's turn one
        // step clockwise of the earlier games', north giving north and
        // south south.
        f.stance = roll(2);
        const int turned = vga_engine(game_of(identity_)) ? 1 : 0;
        base = (along_party_facing() + turned + (f.side == Side::party ? 4 : 0)) % 8;
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
    // The 256-color engine's computer never uses an item (its SelectItemToUse),
    // nor Dark Queen's (aiturn.c, the computer's turn).
    if (weighs_spells() || native_weighs_spells())
    {
        return false;
    }
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
    AreaAims area_aims;
    for (int pass = 1; pass <= passes; ++pass)
    {
        for (const auto &item : f.numbers.items)
        {
            const int number = item.spells[0];
            if (item.scroll || !item.readied || !spell_built(number))
            {
                continue;
            }
            // A party member weighs an item's area spell as the item casts
            // it, at the item's level (caster_level), as its aim will; the
            // original's AI decides at the creature's own level.
            f.using_item = weighs_area_spell(f, number) ? item.ordinal : -1;
            if (!ai_decides(f, number, 8 - pass, area_aims))
            {
                f.using_item = -1;
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
    if (weighs_spells() || native_weighs_spells())
    {
        return treasures_ai_cast(f, magic);
    }
    const int passes = roll(7);
    const bool enemy_left = std::any_of(fighters_.begin(), fighters_.end(),
                                        [&f](const Fighter &o)
                                        {
                                            return o.side != f.side && o.able();
                                        });
    const auto &spells = f.numbers.spells;
    // A party member casts only with the party's Quick magic on (Curse's
    // reference card: ALT M); monsters and NPCs always may.
    if (!f.cast_allowed || silenced(f) || f.has(effect::no_magic) || !enemy_left ||
        (f.numbers.morale < non_player && !magic) || spells.empty())
    {
        return false;
    }
    AreaAims area_aims;
    for (int pass = 1; pass <= passes; ++pass)
    {
        for (int pick = 0; pick < 3; ++pick)
        {
            const int number =
                spells[static_cast<std::size_t>(roll(static_cast<int>(spells.size())) - 1)];
            if (game_spell(number) && game_spell(number)->when != 0 && spell_built(number) &&
                (!underwater_ || underwater_spell(number)) &&
                ai_decides(f, number, 8 - pass, area_aims))
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

// Treasures' computer casts (its combatai.pas, SelectSpellToCast and
// PickCastableSpell): never while fleeing; of its memorized spells that may
// be cast in a fight, the one of highest AI weight (a later copy first among
// equals) that has a target worth it, and that one only if 1d100 is above
// four times its fighter, paladin and ranger levels and 1d5 no more than the
// spell's weight. The original's sort passes over the first memorized spell
// (docs/ORIGINAL-RULES.md, Bugs); OpenGoldBox weighs them all. Dark Queen's
// (aiturn.c, try_spell and spell_pick) counts its knight levels too.
bool Session::treasures_ai_cast(Fighter &f, bool magic)
{
    if (f.fleeing || !f.cast_allowed || silenced(f) || f.has(effect::no_magic) ||
        (f.numbers.morale < non_player && !magic))
    {
        return false;
    }
    std::vector<int> ranked;
    for (auto copy = f.numbers.spells.rbegin(); copy != f.numbers.spells.rend(); ++copy)
    {
        const auto *known = game_spell(*copy);
        if (known && known->when != 0 && spell_built(*copy))
        {
            ranked.push_back(*copy);
        }
    }
    std::ranges::stable_sort(ranked, std::greater{},
                             [this](int number)
                             {
                                 return game_spell(number)->ai_priority;
                             });
    const auto chosen = std::ranges::find_if(ranked,
                                             [&](int number)
                                             {
                                                 return treasures_spell_has_target(f, number);
                                             });
    if (chosen == ranked.end())
    {
        return false;
    }
    const auto &levels = f.numbers.class_levels;
    // Dark Queen keeps a knight's levels in the monk's slot.
    const int knight =
        native_weighs_spells() ? levels[static_cast<std::size_t>(ClassSlot::monk)] : 0;
    const int fighting = 4 * (knight + levels[static_cast<std::size_t>(ClassSlot::fighter)] +
                              levels[static_cast<std::size_t>(ClassSlot::paladin)] +
                              levels[static_cast<std::size_t>(ClassSlot::ranger)]);
    if (roll(100) <= fighting || game_spell(*chosen)->ai_priority < roll(5))
    {
        return false;
    }
    begin_cast(f, *chosen);
    if (f.aiming && !ai_aim(f))
    {
        lose_spell(f);
        end_actions(f);
    }
    return true;
}

// How Treasures' computer rates a creature as its spell's target (its
// cbtacts.pas, RateSpellTarget): -10 one of the side the spell would spare,
// 1 one the spell may take, 0 one it would do nothing to. A globe keeps off
// the spells it stops, and magic resistance above 50 every spell.
int Session::treasures_spell_rating(const Fighter &caster, const Fighter &target, int number) const
{
    if (native_weighs_spells())
    {
        return native_rank_target(caster, target, number);
    }
    // Pools of Darkness numbers these effects as Treasures does.
    const auto game = game_of(identity_);
    const auto &known = *game_spell(number);
    const Side enemies = caster.side == Side::party ? Side::opposition : Side::party;
    if (target.side != (known.offensive ? enemies : caster.side))
    {
        return -10;
    }
    // Its own effect numbers (canonical_effect).
    const auto has = [&target, game](int code)
    {
        return target.has(canonical_effect(code, game));
    };
    constexpr int bless = 1, immune_electricity = 6, shield = 17, blind = 33, bestow_curse = 36,
                  globe = 57, prayer = 49, held = 52, asleep = 53, minor_globe = 63,
                  immune_fear = 92, immune_fire = 93;
    const bool person = target.numbers.race != 0 || (target.numbers.traits & trait::person) != 0;
    const auto fighting_beside = [&]
    {
        return std::any_of(fighters_.begin(), fighters_.end(),
                           [&](const Fighter &o)
                           {
                               return o.side != target.side && o.able() && distance(o, target) <= 1;
                           });
    };
    bool takes = false;
    // The spells by the rules' numbers (canonical_spell).
    switch (number)
    {
    case 1: // Bless, on one no enemy stands beside
        takes = !has(bless) && !fighting_beside();
        break;
    case 42: // Prayer
        takes = !caster.has(canonical_effect(prayer, game));
        break;
    case 3:   // Cure Light Wounds
    case 158: // Cure Serious Wounds
    case 171: // Cure Critical Wounds
    case 236: // Heal
        takes = target.hit_points < target.numbers.max_hit_points;
        break;
    case 296: // Charm Person or Mammal
        takes = target.numbers.race != 0 || (target.numbers.traits & trait::mammal) != 0;
        break;
    case 10: // Charm Person, a person on one square
        takes = person && !target.numbers.large;
        break;
    case 15:  // Magic Missile
    case 165: // the wand of magic missiles
        takes = !has(shield);
        break;
    case 21: // Sleep
        takes = target.numbers.level < 5 && !has(asleep);
        break;
    case 23:
    case 49: // Hold Person, a character or person not held
        takes = target.numbers.race != 0 && person && !has(held);
        break;
    case 37: // Cure Blindness
        takes = has(blind);
        break;
    case 43: // Remove Curse
        takes = has(bestow_curse);
        break;
    case 47:  // Fireball
    case 64:  // the necklace of missiles
    case 174: // Flame Strike
    case 315: // Delayed Blast Fireball
        takes = !has(immune_fire);
        break;
    case 51: // Lightning Bolt
        takes = !has(immune_electricity);
        break;
    case 176: // Slay Living
    case 310: // Death Spell
        takes = (target.numbers.traits & trait::unliving) == 0;
        break;
    case 61:  // the wand of paralyzation
    case 182: // Confusion
        takes = true;
        break;
    case 181: // Charm Monsters
    case 194: // Hold Monster
    case 320: // Mass Charm
        takes = person;
        break;
    case 184: // Fear
        takes = !has(immune_fear);
        break;
    case 191: // Cloudkill
        takes = target.numbers.level < 6;
        break;
    default:
        takes = known.effect == 0 || !target.has(known.effect);
        break;
    }
    if (takes && ((known.level < 5 && has(globe)) || (known.level < 4 && has(minor_globe)) ||
                  target.numbers.magic_resistance > 50))
    {
        takes = false;
    }
    return takes ? 1 : 0;
}

// Whether Treasures' spell has a target worth casting at (its RateSpell):
// the caster itself for a spell on the caster, else someone in range and
// sight it rates above 0.
bool Session::treasures_spell_has_target(const Fighter &f, int number) const
{
    if (weighs_area_spell(f, number))
    {
        return party_area_aim(f, number).has_value();
    }
    const auto &known = *game_spell(number);
    if (known.area == 0)
    {
        return treasures_spell_rating(f, f, number) > 0;
    }
    const int range = spell_range(known, caster_level(f, known, game_of(identity_)));
    return std::any_of(fighters_.begin(), fighters_.end(),
                       [&](const Fighter &o)
                       {
                           return o.able() && distance(f, o) <= range &&
                                  (o.id == f.id || sees(board_, f, o)) &&
                                  treasures_spell_rating(f, o, number) > 0;
                       });
}

// The one creature Treasures' computer singles out for its spell (its
// SelectSpellTarget, PickSpellTarget): an enemy it locks onto anew, or for a
// spell on its own side one of them at random, within range and in sight;
// nobody when it rates that one at 0 or below.
Fighter *Session::treasures_spell_target(Fighter &f, int number)
{
    const auto &known = *game_spell(number);
    const int range = spell_range(known, caster_level(f, known, game_of(identity_)));
    Fighter *pick = nullptr;
    if (known.offensive)
    {
        pick = acquire_target(f, range, false, true) ? find(f.lock) : nullptr;
    }
    else
    {
        std::vector<Fighter *> friends;
        for (auto &o : fighters_)
        {
            if (o.side == f.side && o.able() && distance(f, o) <= range &&
                (o.id == f.id || sees(board_, f, o)))
            {
                friends.push_back(&o);
            }
        }
        if (!friends.empty())
        {
            pick = friends[static_cast<std::size_t>(roll(static_cast<int>(friends.size())) - 1)];
        }
    }
    return pick && treasures_spell_rating(f, *pick, number) > 0 ? pick : nullptr;
}

// Where Treasures' computer aims a spell that bursts around a square (its
// AimAreaSpell): the square in range and sight where the creatures the
// burst would take rate highest together, each it may take 1 and each of
// the side it would spare -10, a tie taking the later square one time in
// four; nowhere when no square rates above 0. The burst is the one the
// spell has as it goes off (within).
std::optional<Cell> Session::treasures_area_aim(const Fighter &f, int number)
{
    const auto &known = *game_spell(number);
    const int range = spell_range(known, caster_level(f, known, game_of(identity_)));
    const int radius = known.area & 7;
    // Only a square near one the spell may take can rate above 0.
    std::vector<Cell> takeable;
    for (const auto &o : fighters_)
    {
        if (o.able() && treasures_spell_rating(f, o, number) > 0)
        {
            takeable.push_back(o.cell);
        }
    }
    std::optional<Cell> best;
    int best_score = 0;
    for (int x = f.cell.x - range; x <= f.cell.x + range; ++x)
    {
        for (int y = f.cell.y - range; y <= f.cell.y + range; ++y)
        {
            const Cell cell{x, y};
            const bool near_one =
                std::any_of(takeable.begin(), takeable.end(),
                            [&](Cell at)
                            {
                                return doubled_distance(at, cell) <= 2 * radius + 4;
                            });
            if (!near_one || !board_.contains(cell) || rules::blocks_movement(board_.at(cell)) ||
                doubled_distance(f.cell, cell) / 2 > range || !clear_line(board_, f.cell, cell))
            {
                continue;
            }
            int score = 0;
            for (const auto id : within(cell, radius))
            {
                score += treasures_spell_rating(f, *find(id), number);
            }
            if (score > best_score || (score == best_score && score > 0 && roll(100) <= 25))
            {
                best = cell;
                best_score = score;
            }
        }
    }
    return best_score > 0 ? best : std::nullopt;
}

// How Dark Queen's computer rates a creature as its spell's target (its
// cbaction.c, rank_target): -40 one of the side the spell would spare, 4
// one the spell may take, 0 one it would do nothing to. A globe keeps off
// the spells it stops; then magic resistance above 40 rates it 3, above 70
// 2, above 100 1 and above 130 0, a globed one's too, in the original's
// order.
int Session::native_rank_target(const Fighter &caster, const Fighter &target, int number) const
{
    constexpr auto game = GoldBoxGame::dark_queen_of_krynn;
    const int native = native_spell(number, game);
    if (native == 0)
    {
        return 0;
    }
    const auto &row = dark_queen_data::spells[static_cast<std::size_t>(native - 1)];
    const Side enemies = caster.side == Side::party ? Side::opposition : Side::party;
    if (target.side != (row.hostile != 0 ? enemies : caster.side))
    {
        return -40;
    }
    // Its own effect numbers (canonical_effect).
    const auto has = [](const Fighter &one, int code)
    {
        return one.has(canonical_effect(code, game));
    };
    const auto bears = [&](int code)
    {
        return has(target, code);
    };
    constexpr int bless = 1, immune_electricity = 6, shield = 17, blind = 33, bestow_curse = 36,
                  prayer = 49, held = 52, asleep = 53, globe = 57, minor_globe = 63,
                  immune_fear = 92, immune_fire = 93, confused = 138, sleepless = 141,
                  free_of_holds = 142, poison_ward = 143, spore_infection = 185;
    const bool character = target.numbers.race != 0;
    // Older combat continuations lack the derived trait. Read the hero's
    // existing identity or the monster's saved immunity bit without changing
    // its serialized numbers; a load followed by a save keeps those bytes.
    const bool saved_record = target.record && target.record->game == game;
    const bool hero = target.numbers.native_progression.has_value() ||
                      (saved_record && !target.weighs_unreadied && character);
    const bool mind = (target.numbers.traits & dark_queen_effect::mind_spell_trait) != 0 || hero ||
                      (saved_record && (target.record->target_flags & 0x02) != 0);
    const bool wounded = target.hit_points < target.numbers.max_hit_points;
    const auto fighting_beside = [&]
    {
        return std::any_of(fighters_.begin(), fighters_.end(),
                           [&](const Fighter &o)
                           {
                               return o.side != target.side && o.able() && distance(o, target) <= 1;
                           });
    };
    bool takes = false;
    // The spells by the rules' numbers (canonical_spell), the original's own
    // in brackets.
    switch (number)
    {
    case 1: // Bless [1]
        takes = !bears(bless) && !fighting_beside();
        break;
    case 42: // Prayer [2Ah], while the caster prays not already
        takes = !has(caster, prayer);
        break;
    case 236: // Heal [24h]
        takes = wounded || bears(spore_infection);
        break;
    case 3:   // Cure Light Wounds [3]
    case 158: // Cure Serious Wounds [3Ah]
    case 171: // Cure Critical Wounds [47h]
        takes = wounded;
        break;
    case 296: // Charm Person or Mammal [60h]
        takes = character || (target.numbers.traits & trait::mammal) != 0;
        break;
    case 10: // Charm Person [0Ah], on one square
        takes = mind && !target.numbers.large;
        break;
    case 15: // Magic Missile [0Fh]
    case 65: // the wand of magic missiles [86h]
        takes = !bears(shield);
        break;
    case 21: // Sleep [15h]
        takes = target.numbers.level < 5 && !bears(sleepless) && !bears(asleep);
        break;
    case 23: // Hold Person [17h]
    case 49: // [31h]
        takes = character && !bears(free_of_holds) && mind && !bears(held);
        break;
    case 37: // Cure Blindness [25h]
        takes = bears(blind);
        break;
    case 43: // Remove Curse [2Bh]
        takes = bears(bestow_curse);
        break;
    case 47:  // Fireball [2Fh]
    case 174: // Flame Strike [4Ah]
    case 315: // Delayed Blast Fireball [73h]
    case 64:  // the necklace of missiles [85h]
        takes = !bears(immune_fire);
        break;
    case 51: // Lightning Bolt [33h]
        takes = !bears(immune_electricity);
        break;
    case 176: // Slay Living [4Ch]
    case 310: // Death Spell [6Eh]
        takes = (target.numbers.traits & trait::unliving) == 0;
        break;
    case 168: // [44h]
        takes = (target.numbers.traits & trait::poison_proof) == 0 && !bears(poison_ward);
        break;
    case 194: // Hold Monster [5Eh]
        takes = mind && !bears(free_of_holds);
        break;
    case 61: // the wand of paralyzation [82h]
        takes = !bears(free_of_holds);
        break;
    case 181: // Charm Monsters [51h]
    case 320: // Mass Charm [78h]
        takes = mind;
        break;
    case 182: // Confusion [52h]
        takes = !bears(confused);
        break;
    case 184: // Fear [54h]
        takes = !bears(immune_fear);
        break;
    case 191: // Cloudkill [5Bh]
        takes = target.numbers.level < 6;
        break;
    default:
        takes = row.effect_id == 0 || !bears(row.effect_id);
        break;
    }
    if (!takes)
    {
        return 0;
    }
    int rank = 4;
    if ((row.level < 5 && bears(globe)) || (row.level < 4 && bears(minor_globe)))
    {
        rank = 0;
    }
    const int resistance = target.numbers.magic_resistance;
    if (resistance > 40)
    {
        rank = resistance > 130 ? 0 : resistance > 100 ? 1 : resistance > 70 ? 2 : 3;
    }
    return rank;
}

// Where Dark Queen's computer aims a spell that bursts around a square (its
// cbaction.c, area_aim): of the squares within its range of the caster, or
// when range and burst together pass 18 squares within 18 less the burst of
// the creature it fights, the one where the creatures whose squares the
// burst would cover rate highest together (native_rank_target), walls
// aside, as long as the caster's range and sight reach it; a tie takes the
// later square one time in four, nowhere when none rates above 0. Stinking
// Cloud [22h] and Blade Barrier [65h] spread right and down from their
// square only.
std::optional<Cell> Session::native_area_aim(const Fighter &f, int number)
{
    constexpr int stinking_cloud = 34, fireball = 47, ice_storm = 187, blade_barrier = 301,
                  delayed_blast_fireball = 315;
    const auto &known = *game_spell(number);
    const int range = spell_range(known, caster_level(f, known, game_of(identity_)));
    // The bursts as the spells go off: Ice Storm's 3 under water, and a
    // fireball's 1 less out of doors (cbaction.c, 2075-2082).
    int radius = number == ice_storm && underwater_ ? 3 : known.area & 7;
    if ((number == fireball || number == delayed_blast_fireball) && outdoors_)
    {
        --radius;
    }
    const bool one_way = number == stinking_cloud || number == blade_barrier;
    const auto *fought = range + radius > 18 ? find(f.lock) : nullptr;
    const Cell base = fought ? fought->cell : f.cell;
    const int reach = std::min(range + radius, 18) - radius;
    // Each creature's rating, and whether a burst at a square covers it
    // (area_widths: the doubled distance's burst).
    std::vector<std::pair<Cell, int>> rated;
    for (const auto &o : fighters_)
    {
        const bool counted = !one_way || (o.cell.x >= base.x - reach && o.cell.y >= base.y - reach);
        if (o.able() && counted)
        {
            rated.emplace_back(o.cell, native_rank_target(f, o, number));
        }
    }
    const auto covers = [&](Cell aim, Cell at)
    {
        if (!one_way)
        {
            return in_burst(doubled_distance(aim, at), radius);
        }
        const int down = aim.y - at.y, right = aim.x - at.x;
        return down >= 0 && right >= 0 && in_burst(doubled_distance({0, 0}, {right, down}), radius);
    };
    std::optional<Cell> best;
    int best_score = 0;
    for (int x = base.x - reach; x <= base.x + reach; ++x)
    {
        for (int y = base.y - reach; y <= base.y + reach; ++y)
        {
            const Cell cell{x, y};
            int score = 0;
            for (const auto &[at, rating] : rated)
            {
                score += covers(cell, at) ? rating : 0;
            }
            const bool better =
                score > best_score || (score == best_score && score > 0 && roll(100) <= 25);
            if (better && board_.contains(cell) && doubled_distance(f.cell, cell) / 2 <= range &&
                clear_line(board_, f.cell, cell))
            {
                best = cell;
                best_score = score;
            }
        }
    }
    return best_score > 0 ? best : std::nullopt;
}

// Treasures' computer aims its spell (its cbtacts.pas, SelectCombatTargets):
// a burst at the square AimAreaSpell finds; a bolt or a spell on one
// creature at the one it singles out; a spell on several at as many picks
// as it takes (the same one picked again spends a pick), or for Charm
// Monsters and Faerie Fire picks until their weight passes a budget (2d4,
// or the caster's level for Faerie Fire; a creature weighs 1 to level 1, 2
// at 2, 4 at 3 and 8 above, or for Faerie Fire 1, and 2 a large one). False
// when it finds no one, and the spell is aborted.
bool Session::treasures_ai_aim(Fighter &f)
{
    constexpr int faerie_fire = 179;
    const int number = f.aiming;
    const auto &known = *game_spell(number);
    const int area = known.area & 0x0F;
    const auto abort = [&]
    {
        log({"{name} aborts the spell.", {{"name", f.name}}});
        return false;
    };
    if (area == 0)
    {
        resolve_spell(f, {f.id}, f.cell);
        return true;
    }
    if (area >= 9 && area <= 14)
    {
        const auto aim =
            native_weighs_spells() ? native_area_aim(f, number) : treasures_area_aim(f, number);
        if (!aim)
        {
            return abort();
        }
        resolve_spell(f, within(*aim, area & 7), *aim);
        return true;
    }
    if (area == 8 || area == 15)
    {
        const auto *target = treasures_spell_target(f, number);
        if (!target)
        {
            return abort();
        }
        const auto aim = target->cell;
        resolve_spell(f, area == 8 ? within(aim, 0) : std::vector<EntityId>{target->id}, aim);
        return true;
    }
    std::vector<EntityId> picked;
    const auto picked_already = [&](const Fighter &target)
    {
        return std::find(picked.begin(), picked.end(), target.id) != picked.end();
    };
    if (area == 5)
    {
        int budget =
            number == faerie_fire ? caster_level(f, known, game_of(identity_)) : roll(4) + roll(4);
        int weight = 0;
        while (budget > 0)
        {
            const auto *target = treasures_spell_target(f, number);
            if (!target)
            {
                break;
            }
            if (picked_already(*target))
            {
                --budget;
                continue;
            }
            picked.push_back(target->id);
            const int level = target->numbers.level;
            weight += number == faerie_fire ? (target->numbers.large ? 2 : 1)
                      : level <= 1          ? 1
                      : level == 2          ? 2
                      : level == 3          ? 4
                                            : 8;
            if (picked.size() > 1 && weight > budget)
            {
                break;
            }
        }
    }
    else
    {
        for (int picks = (area & 3) + 1; picks > 0; --picks)
        {
            const auto *target = treasures_spell_target(f, number);
            if (!target)
            {
                break;
            }
            if (!picked_already(*target))
            {
                picked.push_back(target->id);
            }
        }
    }
    if (picked.empty())
    {
        return abort();
    }
    resolve_spell(f, picked, find(picked.back())->cell);
    return true;
}

// Whether the AI takes this spell (combatai.pas, DecideCastSpell): of high
// enough priority; one on itself always, Cure Light Wounds when someone needs
// it; one at an enemy when an enemy is in range and sight, and none of those
// has a friend within the spell's blast. A party-side caster takes an area
// spell only where its outlook finds an aim the player's limits allow
// (party_area_aim), kept for the turn in `area_aims`.
bool Session::ai_decides(const Fighter &f, int number, int least_priority,
                         AreaAims &area_aims) const
{
    const auto &known = *game_spell(number);
    if (known.ai_priority == 0 || known.ai_priority < least_priority)
    {
        return false;
    }
    if (number == 3)
    {
        return cure_search(f) != nullptr;
    }
    if (!known.offensive)
    {
        return true;
    }
    if (weighs_area_spell(f, number))
    {
        auto [aim, unweighed] = area_aims.try_emplace(number);
        if (unweighed)
        {
            aim->second = party_area_aim(f, number);
        }
        return aim->second.has_value();
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
const Fighter *Session::cure_search(const Fighter &f) const
{
    const Fighter *best = nullptr;
    for (const auto &o : fighters_)
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
    for (const auto &o : fighters_)
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
// helpless one for a spell that makes helpless; a party-side caster an area
// spell where its outlook is best (aim_party_area_spell). False when it
// finds none.
bool Session::ai_aim(Fighter &f)
{
    if (weighs_area_spell(f, f.aiming))
    {
        return aim_party_area_spell(f);
    }
    if (weighs_spells() || native_weighs_spells())
    {
        return treasures_ai_aim(f);
    }
    const auto &known = *game_spell(f.aiming);
    const int area = known.area & 0x0F;
    if (f.aiming == 3)
    {
        const auto *patient = cure_search(f);
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

} // namespace opengold::original::detail
