#include "combat_geometry.h"
#include "opengold/ai_tuning.h"
#include "opengold/combat_demo.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <queue>
#include <stdexcept>
#include <tuple>

// The computer player: the choices the demonstration AI, Quick combat and the
// flee policy make from a fight's public state and legal commands. It submits
// nothing and rolls nothing; CombatDemo (combat_demo.cpp) runs the fight.
namespace opengold
{
using namespace rules;
using namespace por;
using namespace combat_geometry;

namespace
{
std::optional<Command> run_for_edge(const rules::Snapshot &state,
                                    const std::vector<Command> &offered,
                                    const rules::CombatantView &active);
// The demonstration AI's choice from what it reads of a fight: its state and
// the commands open to it. It submits nothing, so it takes no session
// (Effective C++ Item 32).
Command choose_from(const rules::Snapshot &state, const std::vector<Command> &offered,
                    Searching searching, Difficulty difficulty);
Command choose_in_sight(const rules::Snapshot &state, const std::vector<Command> &offered,
                        Searching searching, Difficulty difficulty);
Command charge_in_panic(const rules::Snapshot &state, const std::vector<Command> &offered,
                        const rules::CombatantView &active);
std::optional<Command> choose_item(const rules::Snapshot &state, const CombatantView &active,
                                   const std::vector<Command> &offered);

bool item_verb(std::string_view verb);

// At or below the Hit Points where the computer treats a creature as hurt.
bool is_hurt(const CombatantView &unit)
{
    return unit.hit_points <= ai_tuning::hurt_share * unit.max_hit_points;
}

// A choice the original made at random, here repeatable from the fight's state
// so a replay or a test sees the same one: an index below `count`.
std::size_t pick(const rules::Snapshot &state, EntityId actor, std::uint64_t salt,
                 std::size_t count)
{
    std::uint64_t x =
        state.revision * 0x9E3779B97F4A7C15ull ^ (static_cast<std::uint64_t>(actor) << 32) ^ salt;
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return static_cast<std::size_t>(x % count);
}

// The spells the computer casts with a slot, each with the original's priority
// (#266): it rolls 1d7 and tries the priorities from 7 down to 8 less the roll,
// so the strongest come first and the weakest only on a high roll. A helpful
// spell goes on the caster or an ally. Healing, cantrips and the spells with
// their own rules below (Moonbeam, Hex, Spiritual Weapon, Hunter's Mark) are
// not here; neither is any spell left to the player.
struct AiSpell
{
    std::string_view id;
    int priority;
    bool helpful{};
};

constexpr std::array ai_spells{AiSpell{"fireball", 7},
                               AiSpell{"hold_monster", 7},
                               AiSpell{"confusion", 7},
                               AiSpell{"ice_storm", 7},
                               AiSpell{"finger_of_death", 7},
                               AiSpell{"circle_of_death", 7},
                               AiSpell{"chain_lightning", 7},
                               AiSpell{"sunburst", 7},
                               AiSpell{"power_word_stun", 7},
                               AiSpell{"disintegrate", 7},
                               AiSpell{"harm", 7},
                               AiSpell{"prismatic_spray", 7},
                               AiSpell{"freezing_sphere", 7},
                               AiSpell{"hold_person", 6},
                               AiSpell{"lightning_bolt", 6},
                               AiSpell{"cone_of_cold", 6},
                               AiSpell{"flame_strike", 6},
                               AiSpell{"inflict_wounds", 6},
                               AiSpell{"fear", 6},
                               AiSpell{"charm_monster", 6},
                               AiSpell{"blight", 6},
                               AiSpell{"phantasmal_killer", 6},
                               AiSpell{"contagion", 6},
                               AiSpell{"flesh_to_stone", 6},
                               AiSpell{"eyebite", 6},
                               AiSpell{"dominate_person", 6},
                               AiSpell{"banishment", 6},
                               AiSpell{"hypnotic_pattern", 6},
                               AiSpell{"vitriolic_sphere", 6},
                               AiSpell{"irresistible_dance", 6},
                               AiSpell{"spirit_guardians", 5, true},
                               AiSpell{"stinking_cloud", 5},
                               AiSpell{"bestow_curse", 5},
                               AiSpell{"cloudkill", 5},
                               AiSpell{"sleet_storm", 5},
                               AiSpell{"black_tentacles", 5},
                               AiSpell{"shatter", 5},
                               AiSpell{"blindness", 5},
                               AiSpell{"vampiric_touch", 5},
                               AiSpell{"insect_plague", 5},
                               AiSpell{"magic_missile", 4},
                               AiSpell{"dispel_magic", 4},
                               AiSpell{"charm_person", 4},
                               AiSpell{"silence", 4},
                               AiSpell{"slow", 4},
                               AiSpell{"faerie_fire", 4},
                               AiSpell{"scorching_ray", 4},
                               AiSpell{"acid_arrow", 4},
                               AiSpell{"guiding_bolt", 4},
                               AiSpell{"mind_spike", 4},
                               AiSpell{"ray_of_enfeeblement", 4},
                               AiSpell{"hideous_laughter", 4},
                               AiSpell{"dissonant_whispers", 4},
                               AiSpell{"chromatic_orb", 4},
                               AiSpell{"ice_knife", 4},
                               AiSpell{"ray_of_sickness", 4},
                               AiSpell{"web", 4},
                               AiSpell{"mirror_image", 3, true},
                               AiSpell{"haste", 3, true},
                               AiSpell{"dragons_breath", 3, true},
                               AiSpell{"protection_from_normal_missiles", 3, true},
                               AiSpell{"entangle", 3},
                               AiSpell{"bane", 3},
                               AiSpell{"blur", 3, true},
                               AiSpell{"greater_invisibility", 3, true},
                               AiSpell{"stoneskin", 3, true},
                               AiSpell{"fire_shield", 3, true},
                               AiSpell{"heroism", 3, true},
                               AiSpell{"sleep", 2},
                               AiSpell{"burning_hands", 2},
                               AiSpell{"thunderwave", 2},
                               AiSpell{"color_spray", 2},
                               AiSpell{"blink", 2, true},
                               AiSpell{"mage_armor", 2, true},
                               AiSpell{"false_life", 2, true},
                               AiSpell{"magic_weapon", 2, true},
                               AiSpell{"barkskin", 2, true},
                               AiSpell{"bless", 1, true},
                               AiSpell{"protection_from_evil_and_good", 1, true},
                               AiSpell{"divine_favor", 1, true},
                               AiSpell{"shield_of_faith", 1, true}};

const AiSpell *ai_spell(std::string_view id)
{
    const auto found = std::find_if(ai_spells.begin(), ai_spells.end(),
                                    [&](const AiSpell &spell)
                                    {
                                        return spell.id == id;
                                    });
    return found == ai_spells.end() ? nullptr : &*found;
}

// The known spell a command casts: the longest of the creature's spell ids that
// is the verb or begins it before a suffix ("magic_missile_2"); empty if none.
std::string_view spell_of(const CombatantView &caster, std::string_view verb)
{
    std::string_view found;
    for (const auto &id : caster.spells)
    {
        if ((verb == id ||
             (verb.starts_with(id) && verb.size() > id.size() && verb[id.size()] == '_')) &&
            id.size() > found.size())
        {
            found = id;
        }
    }
    return found;
}

// How a cast ranks among the same spell's casts: its own level first, then the
// lowest slot above it, then any other form.
int cast_rank(std::string_view verb, std::string_view spell)
{
    if (verb == spell)
    {
        return 0;
    }
    const auto suffix = verb.substr(spell.size() + 1);
    return suffix.size() == 1 && suffix[0] >= '2' && suffix[0] <= '9' ? suffix[0] - '0' : 100;
}

// Whether an area aimed at `center` might catch one of the caster's side,
// judged on the safe side of the area the rules will draw (#266): anyone
// within its reach of the center, plus a square; or, for a cone, a cube
// beside the caster or a line, anyone but the caster ahead of it within the
// reach and near the line toward the center.
bool area_risks_side(const rules::Snapshot &state, const CombatantView &caster,
                     const SpellFacts &spell, Cell center)
{
    const int reach = (spell.area_feet + 4) / 5 + (spell.from_caster ? 2 : 1);
    return std::any_of(state.combatants.begin(), state.combatants.end(),
                       [&](const auto &a)
                       {
                           if (a.side != caster.side || a.dead)
                           {
                               return false;
                           }
                           if (!spell.from_caster)
                           {
                               return squares_from(center, a) <= reach;
                           }
                           if (a.id == caster.id)
                           {
                               return false;
                           }
                           const double dx = center.x - caster.cell.x,
                                        dy = center.y - caster.cell.y;
                           const double length = std::max(1.0, std::sqrt(dx * dx + dy * dy));
                           const double cx = a.cell.x - caster.cell.x,
                                        cy = a.cell.y - caster.cell.y;
                           const double along = (cx * dx + cy * dy) / length;
                           const double off = std::abs(cx * dy - cy * dx) / length;
                           return along > 0 && along <= reach && off <= along / 2 + 1.5;
                       });
}

// Whether an area spell can be aimed at `target` safely: within its range
// and in the caster's sight, as the rules let it be aimed, within the reach of
// a cone or line, and catching no one of the caster's side.
bool safe_area_target(const rules::Snapshot &state, const CombatantView &caster,
                      const SpellFacts &spell, const CombatantView &target)
{
    // The aim goes on the target's own square, and the rules measure range and
    // sight to that square alone: a large enemy's other squares may show when
    // that one does not, and a cast that cannot be aimed would be given up and
    // chosen again without end.
    const int distance = 5 * squares_from(target.cell, caster);
    const auto squares = squares_of(caster);
    const bool aim_in_sight =
        std::any_of(squares.begin(), squares.end(),
                    [&](Cell square)
                    {
                        return rules::has_line_of_sight(state.battlefield, square, target.cell);
                    });
    // A cone or line runs from the caster's square nearest the aim, and its
    // length is measured straight, as the rules draw it.
    const auto squares_to_aim = [&](Cell square)
    {
        return std::max(std::abs(square.x - target.cell.x), std::abs(square.y - target.cell.y));
    };
    Cell origin = caster.cell;
    for (const auto square : squares_of(caster))
    {
        if (squares_to_aim(square) < squares_to_aim(origin))
        {
            origin = square;
        }
    }
    const double reach = 5.0 * std::hypot(target.cell.x - origin.x, target.cell.y - origin.y);
    return target.side != caster.side && target.conscious && distance <= spell.range_feet &&
           (!spell.from_caster || reach <= spell.area_feet) && aim_in_sight &&
           !area_risks_side(state, caster, spell, target.cell);
}

// Whether the creature already shows the spell's effect, by its name in the
// creature's status effects ("Paralyzed (Hold Person)").
bool shows_spell(const CombatantView &unit, std::string_view label)
{
    label = label.substr(0, label.find(" ("));
    return std::any_of(unit.status_effects.begin(), unit.status_effects.end(),
                       [&](const auto &effect)
                       {
                           return effect.name.source.find(label) != std::string::npos;
                       });
}
} // namespace

Command choose_demo_command(const CombatSession &session, Searching searching,
                            Difficulty difficulty)
{
    return choose_from(session.snapshot(), session.legal_commands(), searching, difficulty);
}

Command choose_demo_command(const rules::Snapshot &state, const std::vector<Command> &offered,
                            Searching searching, Difficulty difficulty)
{
    return choose_from(state, offered, searching, difficulty);
}

namespace
{
// Any creature may be attacked or healed (CLOSE-1), but the computer keeps its
// attacks off its own side and its healing off its foes, and strikes a
// character already down only on Nightmare and Infernal, to finish it; a
// monster at 0 Hit Points still up is a regenerating one, which it may.
std::vector<Command> aimed_as_before(const rules::Snapshot &state,
                                     const std::vector<Command> &offered, Difficulty difficulty)
{
    const bool finishing = ai_tuning::level(difficulty).finishes_downed;
    const auto side_of = [&](EntityId id) -> const CombatantView *
    {
        const auto found = std::find_if(state.combatants.begin(), state.combatants.end(),
                                        [&](const auto &a)
                                        {
                                            return a.id == id;
                                        });
        return found == state.combatants.end() ? nullptr : &*found;
    };
    const auto *active = side_of(state.actor);
    std::vector<Command> result;
    std::copy_if(offered.begin(), offered.end(), std::back_inserter(result),
                 [&](const Command &command)
                 {
                     const auto *target = side_of(command.target);
                     if (!active || !target || command.intent == rules::Intent::neutral)
                     {
                         return true;
                     }
                     const bool ally = target->side == active->side;
                     if (command.intent == rules::Intent::friendly)
                     {
                         return ally;
                     }
                     return !ally && (target->hit_points > 0 || finishing ||
                                      target->side == Side::opposition);
                 });
    return result;
}

// The AI does not know where a hidden enemy is (HIDE-1): it chooses as if the
// enemy were absent, and with no other enemy left it takes the Search action.
Command choose_from(const rules::Snapshot &state, const std::vector<Command> &every,
                    Searching searching, Difficulty difficulty)
{
    // Rules with an AI of their own (the original's Quick) play the turn by it.
    if (const auto own_ai = std::ranges::find(every, std::string_view("quick"), &Command::verb);
        own_ai != every.end())
    {
        return *own_ai;
    }
    // A spell given up this turn stays given up.
    const auto given_up = [&](const Command &command)
    {
        return std::ranges::find(state.given_up_spells, command.verb) !=
               state.given_up_spells.end();
    };
    if (std::ranges::any_of(every, given_up))
    {
        std::vector<Command> kept;
        std::ranges::copy_if(every, std::back_inserter(kept), std::not_fn(given_up));
        return choose_from(state, kept, searching, difficulty);
    }
    const auto active = std::find_if(state.combatants.begin(), state.combatants.end(),
                                     [&](const auto &a)
                                     {
                                         return a.id == state.actor;
                                     });
    const auto offered = aimed_as_before(state, every, difficulty);
    const auto hidden_enemy = [&](EntityId id)
    {
        return std::any_of(state.combatants.begin(), state.combatants.end(),
                           [&](const auto &a)
                           {
                               return a.id == id && a.hidden && active != state.combatants.end() &&
                                      a.side != active->side;
                           });
    };
    if (std::none_of(state.combatants.begin(), state.combatants.end(),
                     [&](const auto &a)
                     {
                         return hidden_enemy(a.id);
                     }))
    {
        return choose_in_sight(state, offered, searching, difficulty);
    }
    auto seen = state;
    std::erase_if(seen.combatants,
                  [&](const auto &a)
                  {
                      return hidden_enemy(a.id);
                  });
    std::vector<Command> open;
    std::copy_if(offered.begin(), offered.end(), std::back_inserter(open),
                 [&](const auto &c)
                 {
                     return !hidden_enemy(c.target);
                 });
    const bool enemy_left = std::any_of(seen.combatants.begin(), seen.combatants.end(),
                                        [&](const auto &a)
                                        {
                                            return a.side != active->side && a.conscious;
                                        });
    if (!enemy_left)
    {
        for (const auto &command : open)
        {
            if (command.verb == "search")
            {
                return command;
            }
        }
    }
    return choose_in_sight(seen, open, searching, difficulty);
}

Command choose_in_sight(const rules::Snapshot &state, const std::vector<Command> &offered,
                        Searching searching, Difficulty difficulty)
{
    if (offered.empty())
    {
        throw std::runtime_error("No legal combat command");
    }
    const auto &active = *std::find_if(state.combatants.begin(), state.combatants.end(),
                                       [&](const auto &a)
                                       {
                                           return a.id == state.actor;
                                       });
    // The module orders its pending check choices by its default AI preference.
    if (!state.initiative_choices.empty() || state.effect_targeting ||
        state.optional_effect_choice || state.ability_check_choice)
    {
        return offered.front();
    }
    if (state.temporary_hp_offer)
    {
        const auto verb =
            state.temporary_hp_offer->current.amount >= state.temporary_hp_offer->offered.amount
                ? "temp_hp_keep"
                : "temp_hp_use";
        for (const auto &command : offered)
        {
            if (command.verb == verb)
            {
                return command;
            }
        }
    }
    for (const auto &command : offered)
    {
        if (command.verb == "stand_up")
        {
            return command;
        }
    }
    // One that could not get away charges (MORALE-2): all it has left.
    if (active.charging_in_panic && !state.reaction_pending)
    {
        return charge_in_panic(state, offered, active);
    }
    // A creature whose morale broke runs for the edge; cornered, it fights on.
    if (active.panicked && !state.reaction_pending)
    {
        if (const auto run = run_for_edge(state, offered, active))
        {
            return *run;
        }
    }
    const auto &board = state.battlefield;
    // The enemies the creature can see: it goes for those, and with none in
    // view it holds its ground (#266).
    // On Nightmare and Infernal a downed enemy is one to finish (CLOSE-1).
    const auto &tuning = ai_tuning::level(difficulty);
    const bool finishing = tuning.finishes_downed;
    std::vector<const CombatantView *> in_view;
    for (const auto &enemy : state.combatants)
    {
        if (enemy.side != active.side && !enemy.dead && !enemy.fled &&
            (enemy.hit_points > 0 || finishing) && sees(board, active, enemy))
        {
            in_view.push_back(&enemy);
        }
    }
    // Spending a resource on a fight takes seeing an enemy; hearing one only
    // brings the creature toward it.
    const bool enemy_seen = !in_view.empty();
    // Sound carries 100 feet, walls or not, so two creatures either side of a
    // wall's corner do not wait on each other.
    // A stand-in for the player, or in a check every creature, goes looking
    // for the enemies it cannot see, magic-users too.
    const bool seeking =
        !enemy_seen && (searching == Searching::everyone ||
                        (searching == Searching::party && active.side == Side::party));
    if (!enemy_seen)
    {
        for (const auto &enemy : state.combatants)
        {
            if (enemy.side != active.side && !enemy.dead && !enemy.fled &&
                (enemy.hit_points > 0 || finishing) &&
                (seeking || squares_apart(active, enemy) <= ai_tuning::hearing_squares))
            {
                in_view.push_back(&enemy);
            }
        }
    }
    // It keeps its target while that target stands in view, as the original
    // did; otherwise any enemy in view. Above Normal it goes after the weakest
    // enemy, the one with the fewest Hit Points and then the lowest AC: on
    // Hard once its target is gone, on Nightmare and Infernal at once, so the
    // side's attacks converge on one enemy until it drops (CLOSE-1).
    const auto downed = [](const CombatantView &enemy)
    {
        return enemy.hit_points <= 0 && !enemy.conscious;
    };
    const auto weaker = [&](const CombatantView *a, const CombatantView *b)
    {
        const bool a_down = downed(*a), b_down = downed(*b);
        return std::tie(a_down, a->hit_points, a->armor_class, a->id) <
               std::tie(b_down, b->hit_points, b->armor_class, b->id);
    };
    const auto weakest = std::min_element(in_view.begin(), in_view.end(), weaker);
    const auto targets_weakest_now = tuning.targeting == ai_tuning::Targeting::weakest_now ||
                                     tuning.targeting == ai_tuning::Targeting::most_harm_stopped;
    auto kept = targets_weakest_now ? weakest
                                    : std::find_if(in_view.begin(), in_view.end(),
                                                   [&](const auto *enemy)
                                                   {
                                                       return enemy->id == active.last_target;
                                                   });
    if (kept == in_view.end() && tuning.targeting == ai_tuning::Targeting::weakest_once_lost)
    {
        kept = weakest;
    }
    // Whether a move it is offered brings it beside `enemy`, so it can strike this
    // turn.
    const auto within_a_move = [&](const CombatantView &enemy)
    {
        return std::any_of(offered.begin(), offered.end(),
                           [&](const Command &command)
                           {
                               if (command.verb != "move")
                               {
                                   return false;
                               }
                               const auto squares =
                                   squares_of(command.destination, active.footprint);
                               return std::any_of(squares.begin(), squares.end(),
                                                  [&](Cell square)
                                                  {
                                                      return squares_from(square, enemy) <= 1;
                                                  });
                           });
    };
    // Downing the whole party ends the fight, so a downed character waits while a
    // standing enemy can be struck this turn.
    const bool standing_in_reach =
        finishing &&
        std::any_of(in_view.begin(), in_view.end(),
                    [&](const auto *enemy)
                    {
                        return !downed(*enemy) &&
                               (squares_apart(active, *enemy) <= 1 || within_a_move(*enemy));
                    });
    // On Infernal, how much harm a target does for each Hit Point it has left:
    // what it wields, more while it holds a spell a hit could break. All of it
    // shows on the field, and a downed character does none.
    const auto harm_per_hit_point = [&](const CombatantView &target)
    {
        if (downed(target))
        {
            return 0.0;
        }
        const bool concentrating = std::any_of(target.conditions.begin(), target.conditions.end(),
                                               [](const auto &condition)
                                               {
                                                   return condition.source == "Concentrating";
                                               });
        const double wielded =
            std::max({static_cast<double>(target.melee_rating),
                      static_cast<double>(target.ranged_rating), ai_tuning::minimum_harm});
        const double holding = concentrating ? ai_tuning::concentration_weight : 1.0;
        return wielded * holding / std::max(1, target.hit_points);
    };
    // One of the creatures commands with `verb` are aimed at: its target if one
    // is, else one at random from the other side (#266).
    const bool by_harm = tuning.targeting == ai_tuning::Targeting::most_harm_stopped;
    const auto aim = [&](std::string_view verb) -> const Command *
    {
        std::vector<const Command *> aims;
        for (const auto &command : offered)
        {
            if (command.verb != verb)
            {
                continue;
            }
            const auto target = std::find_if(state.combatants.begin(), state.combatants.end(),
                                             [&](const auto &a)
                                             {
                                                 return a.id == command.target;
                                             });
            if (target == state.combatants.end() || target->side == active.side ||
                (target->hit_points <= 0 && !finishing) || (standing_in_reach && downed(*target)))
            {
                continue;
            }
            // The harm stopped is weighed for every target of a weapon attack below.
            if (kept != in_view.end() && command.target == (*kept)->id &&
                !(by_harm && command.expected_damage))
            {
                return &command;
            }
            aims.push_back(&command);
        }
        if (aims.empty())
        {
            return nullptr;
        }
        // The harm this attack is expected to take away: its share of the
        // target's Hit Points, times what that target can still do.
        if (by_harm && aims.front()->expected_damage)
        {
            const auto harm_stopped = [&](const Command *command)
            {
                const auto &target = *std::find_if(state.combatants.begin(), state.combatants.end(),
                                                   [&](const auto &a)
                                                   {
                                                       return a.id == command->target;
                                                   });
                return command->expected_damage * harm_per_hit_point(target);
            };
            return *std::max_element(aims.begin(), aims.end(),
                                     [&](const Command *a, const Command *b)
                                     {
                                         return harm_stopped(a) < harm_stopped(b);
                                     });
        }
        if (tuning.targeting == ai_tuning::Targeting::keep_target)
        {
            return aims[pick(state, active.id, verb.size(), aims.size())];
        }
        // Above Normal the weakest of those it can reach.
        const auto view_of = [&](const Command *command)
        {
            return &*std::find_if(state.combatants.begin(), state.combatants.end(),
                                  [&](const auto &a)
                                  {
                                      return a.id == command->target;
                                  });
        };
        return *std::min_element(aims.begin(), aims.end(),
                                 [&](const Command *a, const Command *b)
                                 {
                                     return weaker(view_of(a), view_of(b));
                                 });
    };
    // Who the walk heads for. Up to Normal, its target. Above Normal, its target
    // or else the weakest enemy it can strike this turn, and with none in reach
    // the nearest: chasing a weak enemy across the field gives up the attack and
    // leaves its own side (docs/AI.md section 2).
    const CombatantView *walk_target = kept != in_view.end() ? *kept : nullptr;
    if (tuning.walks_within_reach && !(walk_target && within_a_move(*walk_target)))
    {
        walk_target = nullptr;
        for (const auto *enemy : in_view)
        {
            if (within_a_move(*enemy) &&
                (!walk_target ||
                 (by_harm ? harm_per_hit_point(*enemy) > harm_per_hit_point(*walk_target)
                          : weaker(enemy, walk_target))))
            {
                walk_target = enemy;
            }
        }
    }
    // Rank offered destinations by a geometric route around obstacles to its
    // target, or to the nearest enemy in view. Straight distance alone can
    // strand both sides on opposite corners of a wall. This is an AI
    // heuristic; legal movement and its costs remain module-owned.
    const auto index = [&](Cell p)
    {
        return p.y * board.width + p.x;
    };
    std::vector<int> routes(board.terrain.size(), 10000);
    std::queue<Cell> frontier;
    for (const auto *enemy : in_view)
    {
        if (!walk_target || enemy == walk_target)
        {
            for (const auto square : squares_of(*enemy))
            {
                if (board.contains(square))
                {
                    routes[index(square)] = 0;
                    frontier.push(square);
                }
            }
        }
    }
    while (!frontier.empty())
    {
        const auto p = frontier.front();
        frontier.pop();
        for (int y = -1; y <= 1; ++y)
        {
            for (int x = -1; x <= 1; ++x)
            {
                const Cell next{p.x + x, p.y + y};
                if ((!x && !y) || board.at(next) == Terrain::obstacle)
                {
                    continue;
                }
                if (x && y &&
                    (board.at({p.x + x, p.y}) == Terrain::obstacle ||
                     board.at({p.x, p.y + y}) == Terrain::obstacle))
                {
                    continue;
                }
                if (routes[index(next)] <= routes[index(p)] + 1)
                {
                    continue;
                }
                routes[index(next)] = routes[index(p)] + 1;
                frontier.push(next);
            }
        }
    }
    // From the nearest of the squares the creature would fill there.
    const auto nearest = [&](Cell p)
    {
        int steps = 10000;
        for (const auto square : squares_of(p, active.footprint))
        {
            if (board.contains(square))
            {
                steps = std::min(steps, routes[index(square)]);
            }
        }
        return steps;
    };
    // A free move (a Champion's) is taken toward the enemy, not given up.
    if (state.free_movement)
    {
        const Command *closer = nullptr;
        int closest = nearest(active.cell);
        for (const auto &command : offered)
        {
            if (command.verb == "move" && nearest(command.destination) < closest)
            {
                closer = &command;
                closest = nearest(command.destination);
            }
        }
        return closer ? *closer : offered.front();
    }
    // A choice of creatures left open casts on those already chosen. Magic
    // Missile and Scorching Ray cannot be cast early: every dart or ray goes
    // where the last one went while it can, else at the first creature offered.
    if (const auto &targeting = state.spell_targeting)
    {
        // A spell for several creatures, such as Bless, takes in more of the
        // side its first creature is on before it is cast.
        const auto side_of = [&](EntityId id)
        {
            return std::find_if(state.combatants.begin(), state.combatants.end(),
                                [&](const auto &a)
                                {
                                    return a.id == id;
                                });
        };
        if (!targeting->repeats && !targeting->chosen.empty())
        {
            const auto first = side_of(targeting->chosen.front());
            for (const auto &command : offered)
            {
                const auto other = side_of(command.target);
                if (command.verb == targeting->verb && first != state.combatants.end() &&
                    other != state.combatants.end() && other->side == first->side && !other->dead &&
                    std::find(targeting->chosen.begin(), targeting->chosen.end(), command.target) ==
                        targeting->chosen.end())
                {
                    return command;
                }
            }
        }
        for (const auto &command : offered)
        {
            if (command.verb == (targeting->chosen.empty() ? "spell_cancel" : "spell_cast"))
            {
                return command;
            }
        }
        const Command *spell_aim = nullptr;
        for (const auto &command : offered)
        {
            if (command.verb == targeting->verb &&
                (!spell_aim || command.target == targeting->chosen.back()))
            {
                spell_aim = &command;
            }
        }
        if (spell_aim)
        {
            return *spell_aim;
        }
    }
    // Paladin's Smite costs no slot, so a Paladin uses it on the first hit it can;
    // slots stay for healing.
    for (const auto &command : offered)
    {
        if (command.verb == "divine_smite_free")
        {
            return command;
        }
    }
    // A smite that costs a slot only on a Critical Hit, from the lowest slot,
    // so slots stay for spells (#269).
    if (active.critical_hit)
    {
        for (const auto smite :
             {"divine_smite", "searing_smite", "shining_smite", "ensnaring_strike"})
        {
            const Command *lowest = nullptr;
            for (const auto &command : offered)
            {
                if (spell_of(active, command.verb) == smite &&
                    !(command.spell.concentration &&
                      std::any_of(active.conditions.begin(), active.conditions.end(),
                                  [](const auto &c)
                                  {
                                      return c.source == "Concentrating";
                                  })) &&
                    (!lowest || cast_rank(command.verb, smite) < cast_rank(lowest->verb, smite)))
                {
                    lowest = &command;
                }
            }
            if (lowest)
            {
                return *lowest;
            }
        }
    }
    // Favored Enemy's Hunter's Mark and moving it cost no slot, and Horde
    // Breaker's attack, Superior Hunter's Prey and Divine Strike cost nothing,
    // so they come first.
    for (const auto verb :
         {"hunters_mark_move", "hunters_mark_free", "horde_breaker", "superior_hunters_prey",
          "divine_strike_radiant", "spiritual_weapon_strike"})
    {
        for (const auto &command : offered)
        {
            if (command.verb == verb)
            {
                return command;
            }
        }
    }
    // A Barbarian rages before it fights; its attacks keep the Rage going.
    for (const auto &command : offered)
    {
        if (command.verb == "rage")
        {
            return command;
        }
    }
    // Sacred Weapon lasts the fight, so it is taken before the first melee attack.
    const bool can_strike = std::any_of(offered.begin(), offered.end(),
                                        [](const auto &command)
                                        {
                                            return command.verb == "melee";
                                        });
    for (const auto &command : offered)
    {
        if (can_strike && command.verb == "sacred_weapon")
        {
            return command;
        }
    }
    // Burning at half its Hit Points or lower, a creature rolls on the ground
    // to put the fire out (#269); otherwise it fights on.
    for (const auto &command : offered)
    {
        if (command.verb == "extinguish" && is_hurt(active))
        {
            return command;
        }
    }
    // A creature caught by vines spends its Action trying to break free.
    for (const auto &command : offered)
    {
        if (command.verb == "escape")
        {
            return command;
        }
    }
    for (const auto &command : offered)
    {
        // Shield only when its +5 turns the hit into a miss (#266), or
        // against Magic Missile, which it stops whole.
        const bool shield_helps = !state.reaction_attack ||
                                  state.reaction_attack->total <
                                      state.reaction_attack->armor_class + ai_tuning::shield_margin;
        if (command.verb == "opportunity" || (command.verb == "shield" && shield_helps) ||
            command.verb == "deflect" || command.verb == "redirect" || command.verb == "rebuke" ||
            command.verb == "giant_boon" || command.verb == "stones_endurance" ||
            command.verb == "storms_thunder" ||
            ((command.verb == "second_wind" || command.verb == "wholeness_of_body") &&
             is_hurt(active)))
        {
            return command;
        }
    }
    // The other reactions are used whenever offered; otherwise declined.
    if (state.reaction_pending)
    {
        for (const auto verb :
             {"uncanny_dodge", "superior_defense", "retaliate", "counterspell", "decline"})
        {
            for (const auto &command : offered)
            {
                if (command.verb == verb)
                {
                    return command;
                }
            }
        }
    }
    // The most wounded ally at half its Hit Points or lower is healed (#266),
    // by Lay On Hands first because it costs no spell slot. Borrowed view into
    // local offered commands.
    const auto heal_most_wounded =
        [&](std::initializer_list<std::string_view> verbs) -> const Command *
    {
        const Command *best = nullptr;
        const CombatantView *worst = nullptr;
        for (const auto &command : offered)
        {
            if (std::find(verbs.begin(), verbs.end(), command.verb) == verbs.end())
            {
                continue;
            }
            const auto &target = *std::find_if(state.combatants.begin(), state.combatants.end(),
                                               [&](const auto &a)
                                               {
                                                   return a.id == command.target;
                                               });
            // The lower share of its maximum, compared without division.
            if (target.side == active.side && is_hurt(target) &&
                (!worst || static_cast<long long>(target.hit_points) * worst->max_hit_points <
                               static_cast<long long>(worst->hit_points) * target.max_hit_points))
            {
                best = &command;
                worst = &target;
            }
        }
        return best;
    };
    // Magic items first, as the original's Quick characters used their
    // readied ones, whether or not Quick magic is on (MAGIC-3).
    if (const auto item = choose_item(state, active, offered))
    {
        return *item;
    }
    // Lay On Hands and Divine Spark cost no spell slot.
    if (const auto *command = heal_most_wounded({"lay_on_hands", "divine_spark"}))
    {
        return *command;
    }
    // Preserve Life is offered only beside a Bloodied ally, and Lesser
    // Restoration only on a Blinded one.
    for (const auto &command : offered)
    {
        if (command.verb == "turn_undead" || command.verb == "preserve_life" ||
            command.verb.starts_with("lesser_restoration_") || command.verb == "spare_the_dying")
        {
            return command;
        }
    }
    if (const auto *command = heal_most_wounded({"cure_wounds", "cure_wounds_2", "healing_word",
                                                 "healing_word_2", "healing_word_innate_bonus"}))
    {
        return *command;
    }
    // A dying ally who is not healed is stabilized when no enemy is within reach.
    const bool threatened = std::any_of(state.combatants.begin(), state.combatants.end(),
                                        [&](const auto &a)
                                        {
                                            return a.side != active.side && a.conscious &&
                                                   squares_apart(a, active) <= 1;
                                        });
    for (const auto &command : offered)
    {
        if (!threatened && command.verb == "stabilize")
        {
            const auto &target = *std::find_if(state.combatants.begin(), state.combatants.end(),
                                               [&](const auto &a)
                                               {
                                                   return a.id == command.target;
                                               });
            if (target.side == active.side)
            {
                return command;
            }
        }
    }
    for (const auto &command : offered)
    {
        if (command.verb == "shake_awake")
        {
            return command;
        }
    }
    const auto offers = [&](std::string_view verb) -> const Command *
    {
        for (const auto &command : offered)
        {
            if (command.verb == verb)
            {
                return &command;
            }
        }
        return nullptr;
    };
    // A Wraith raises a Specter from a corpse beside it before it attacks.
    if (const auto *raise = offers("create_specter"))
    {
        return *raise;
    }
    const auto shows = [](const CombatantView &unit, std::string_view condition)
    {
        return std::any_of(unit.conditions.begin(), unit.conditions.end(),
                           [&](const auto &c)
                           {
                               return c.source == condition;
                           });
    };
    const auto unit = [&](EntityId id) -> const CombatantView &
    {
        return *std::find_if(state.combatants.begin(), state.combatants.end(),
                             [&](const auto &a)
                             {
                                 return a.id == id;
                             });
    };
    const bool concentrating = shows(active, "Concentrating");
    // Moonbeam's 5-foot radius is the target's square and those around it, so
    // it is aimed only at an enemy with no ally of the caster beside it.
    const auto clear_of_allies = [&](Cell center)
    {
        return std::none_of(state.combatants.begin(), state.combatants.end(),
                            [&](const auto &a)
                            {
                                return a.side == active.side && !a.dead &&
                                       squares_from(center, a) <= 1;
                            });
    };
    // Moonbeam reaches 120 feet, 24 squares, and is aimed on the enemy's own square,
    // which the rules want in sight. An enemy can show while that square does not;
    // aiming there would be given up and chosen again without end.
    const auto moonbeam_aimable = [&](const CombatantView &enemy)
    {
        const auto squares = squares_of(active);
        return enemy.conscious && clear_of_allies(enemy.cell) &&
               squares_from(enemy.cell, active) <= 24 &&
               std::any_of(squares.begin(), squares.end(),
                           [&](Cell square)
                           {
                               return rules::has_line_of_sight(state.battlefield, square,
                                                               enemy.cell);
                           });
    };
    if (state.area_targeting)
    {
        if (state.area_targeting->verb == "moonbeam" || state.area_targeting->verb == "lands_aid")
        {
            for (const auto *enemy : in_view)
            {
                const auto &a = *enemy;
                if (moonbeam_aimable(a))
                {
                    if (a.cell == state.area_targeting->center)
                    {
                        if (const auto *cast = offers("area_cast"))
                        {
                            return *cast;
                        }
                    }
                    for (const auto &command : offered)
                    {
                        if (command.verb == "area_move" && command.destination == a.cell)
                        {
                            return command;
                        }
                    }
                }
            }
        }
        // Any other area spell goes off where its drawn area holds an enemy
        // and no one of the caster's side; else it is aimed at the next enemy
        // where that is safe, else given up (#266).
        const auto &aimed = *state.area_targeting;
        const auto in_area = [&](const CombatantView &a)
        {
            return !a.dead && in_cells(aimed.cells, a);
        };
        if (aimed.verb != "moonbeam" && aimed.verb != "lands_aid")
        {
            const bool ally = std::any_of(state.combatants.begin(), state.combatants.end(),
                                          [&](const auto &a)
                                          {
                                              return a.side == active.side && in_area(a);
                                          });
            const bool enemy = std::any_of(in_view.begin(), in_view.end(),
                                           [&](const auto *a)
                                           {
                                               return a->conscious && in_area(*a);
                                           });
            if (enemy && !ally)
            {
                if (const auto *cast = offers("area_cast"))
                {
                    return *cast;
                }
            }
            for (const auto *target : in_view)
            {
                if (target->cell == aimed.center ||
                    !safe_area_target(state, active, aimed.spell, *target))
                {
                    continue;
                }
                for (const auto &command : offered)
                {
                    if (command.verb == "area_move" && command.destination == target->cell)
                    {
                        return command;
                    }
                }
            }
        }
        if (const auto *cancel = offers("spell_cancel"))
        {
            return *cancel;
        }
    }
    const bool moonbeam_target = std::any_of(in_view.begin(), in_view.end(),
                                             [&](const auto *enemy)
                                             {
                                                 return moonbeam_aimable(*enemy);
                                             });

    // The extra attacks of Light and Nick weapons follow the Attack action at
    // the same target (#269).
    for (const auto verb :
         {"nick_melee", "nick_ranged", "nick_throw", "light_melee", "light_ranged", "light_throw"})
    {
        if (const auto *command = aim(verb))
        {
            return *command;
        }
    }

    // Bonus Action buffs come before the Action: Innate Sorcery before a spell
    // attack, a Druid's flame or staff, Hex and Bardic Inspiration on an ally.
    struct Buff
    {
        const char *verb, *condition;
    };

    for (const auto buff :
         {Buff{"innate_sorcery", "Innate Sorcery"}, Buff{"produce_flame", "Produce Flame"},
          Buff{"shillelagh", "Shillelagh"}})
    {
        if (const auto *command = offers(buff.verb);
            command && !shows(active, buff.condition) &&
            (std::string_view(buff.verb) != "innate_sorcery" || offers("eldritch_blast") ||
             offers("sorcerous_burst_fire")))
        {
            return *command;
        }
    }
    if (!concentrating)
    {
        for (const auto verb : {"hex", "spiritual_weapon"})
        {
            if (const auto *command = offers(verb))
            {
                return *command;
            }
        }
    }
    for (const auto &command : offered)
    {
        if (command.verb == "bardic_inspiration" && command.target != active.id &&
            !shows(unit(command.target), "Inspired"))
        {
            return command;
        }
    }
    // A Druid holds Moonbeam on the enemies, then fights as a Wolf; a Wolf
    // whose Moonbeam has ended leaves the form to cast it again.
    if (!active.form.empty() && !concentrating)
    {
        if (const auto *command = offers("leave_wild_shape"))
        {
            return *command;
        }
    }
    if (!concentrating && moonbeam_target)
    {
        if (const auto *command = offers("moonbeam"))
        {
            return *command;
        }
    }
    if (active.form.empty() && concentrating)
    {
        if (const auto *command = offers("wild_shape_wolf"))
        {
            return *command;
        }
    }
    if (const auto *command = offers("move_moonbeam"); command && moonbeam_target)
    {
        const auto &beamed = state.moonbeams;
        const bool burning =
            std::any_of(state.combatants.begin(), state.combatants.end(),
                        [&](const auto &a)
                        {
                            return a.side != active.side && a.conscious && in_cells(beamed, a);
                        });
        if (!burning)
        {
            for (const auto &move : offered)
            {
                if (move.verb == "move_moonbeam" && unit(move.target).side != active.side &&
                    clear_of_allies(unit(move.target).cell))
                {
                    return move;
                }
            }
        }
    }
    // A spell with a slot, by the original's priorities (#266): a 1d7 roll,
    // then the priorities from 7 down to 8 less the roll, and at each one of
    // the spells that make sense, chosen at random. A harmful spell needs an
    // enemy in view not already under it, an area one a place where it
    // catches no one of the caster's side; a helpful one a creature of its
    // side not already under it. A spell that would end the caster's
    // Concentration waits.
    const auto makes_sense = [&](const Command &command, const AiSpell &spell)
    {
        if (command.aims_area)
        {
            return std::any_of(in_view.begin(), in_view.end(),
                               [&](const auto *enemy)
                               {
                                   return safe_area_target(state, active, command.spell, *enemy);
                               });
        }
        const auto target = std::find_if(state.combatants.begin(), state.combatants.end(),
                                         [&](const auto &a)
                                         {
                                             return a.id == command.target;
                                         });
        if (target == state.combatants.end() || shows_spell(*target, command.label))
        {
            return false;
        }
        if (spell.helpful)
        {
            return target->side == active.side && !target->dead;
        }
        return target->conscious &&
               std::find(in_view.begin(), in_view.end(), &*target) != in_view.end();
    };
    if (!in_view.empty())
    {
        // Without the roll, the strongest come first, always.
        constexpr int sides = ai_tuning::spell_roll_sides;
        const int roll = tuning.rolls_for_spells
                             ? static_cast<int>(pick(state, active.id, 7, sides)) + 1
                             : sides;
        for (int priority = sides; priority >= sides + 1 - roll; --priority)
        {
            std::vector<std::string_view> spells;
            for (const auto &command : offered)
            {
                const auto spell =
                    item_verb(command.verb) ? std::string_view{} : spell_of(active, command.verb);
                const auto *known = spell.empty() ? nullptr : ai_spell(spell);
                if (known && known->priority == priority &&
                    !(concentrating && command.spell.concentration) &&
                    makes_sense(command, *known) &&
                    std::find(spells.begin(), spells.end(), spell) == spells.end())
                {
                    spells.push_back(spell);
                }
            }
            if (spells.empty())
            {
                continue;
            }
            const auto spell = spells[pick(state, active.id, 77, spells.size())];
            const auto &known = *ai_spell(spell);
            // The lowest slot it is offered at, then the creature: its target
            // or the caster when one is offered, else one at random.
            int lowest = 1000;
            for (const auto &command : offered)
            {
                if (spell_of(active, command.verb) == spell && makes_sense(command, known))
                {
                    lowest = std::min(lowest, cast_rank(command.verb, spell));
                }
            }
            std::vector<const Command *> casts;
            for (const auto &command : offered)
            {
                if (spell_of(active, command.verb) == spell &&
                    cast_rank(command.verb, spell) == lowest && makes_sense(command, known))
                {
                    if (command.target == (known.helpful ? active.id : active.last_target) &&
                        (known.helpful || kept != in_view.end()))
                    {
                        return command;
                    }
                    casts.push_back(&command);
                }
            }
            return *casts[pick(state, active.id, 777, casts.size())];
        }
    }
    // A downed enemy is still offered only when it may rise again (a troll's
    // Regeneration): only Acid or Fire stops it, so finish it with one of those,
    // a spell or a Torch before a thrown flask that is used up.
    for (const auto verb :
         {"fire_bolt", "sorcerous_burst_fire", "scorching_ray", "acid_arrow", "hurl_flame",
          "flame_blade_strike", "torch", "throw_alchemists_fire", "throw_acid"})
    {
        for (const auto &command : offered)
        {
            if (command.verb == verb)
            {
                const auto &target = *std::find_if(state.combatants.begin(), state.combatants.end(),
                                                   [&](const auto &a)
                                                   {
                                                       return a.id == command.target;
                                                   });
                if (target.side != active.side && target.hit_points == 0)
                {
                    return command;
                }
            }
        }
    }
    // A standing regenerating enemy is set Burning with Alchemist's Fire: the fire
    // at the start of each of its turns keeps it from regenerating. Oil goes on
    // first when anyone on the side can follow with Alchemist's Fire for 5 more
    // damage; before a Fire spell or a Torch, an action spent on Oil is worth
    // less than a second attack.
    const auto target_of = [&](const Command &command) -> const CombatantView &
    {
        return *std::find_if(state.combatants.begin(), state.combatants.end(),
                             [&](const auto &a)
                             {
                                 return a.id == command.target;
                             });
    };
    const bool alchemists_fire_follows = std::any_of(
        state.combatants.begin(), state.combatants.end(),
        [&](const auto &ally)
        {
            return ally.side == active.side && ally.conscious &&
                   std::any_of(ally.thrown_gear_left.begin(), ally.thrown_gear_left.end(),
                               [](const auto &gear)
                               {
                                   return gear.first == "alchemists_fire" && gear.second > 0;
                               });
        });
    for (const auto &command : offered)
    {
        if (command.verb == "throw_oil" && alchemists_fire_follows)
        {
            const auto &target = target_of(command);
            if (target.side != active.side && target.regenerates && target.hit_points > 0 &&
                !target.oiled && !target.burning)
            {
                return command;
            }
        }
    }
    for (const auto &command : offered)
    {
        if (command.verb == "throw_alchemists_fire")
        {
            const auto &target = target_of(command);
            if (target.side != active.side && target.regenerates && target.hit_points > 0 &&
                !target.burning)
            {
                return command;
            }
        }
    }
    // Fire stops the regeneration that would undo more than a spell's or a
    // weapon's damage, unless something already stopped it this round: a Fire
    // spell from afar, or a Torch in melee.
    for (const auto verb : {"scorching_ray", "fire_bolt", "sorcerous_burst_fire", "hurl_flame",
                            "flame_blade_strike", "torch"})
    {
        for (const auto &command : offered)
        {
            if (command.verb == verb)
            {
                const auto &target = target_of(command);
                if (target.side != active.side && target.regenerates && target.hit_points > 0 &&
                    !target.burning && !target.regeneration_stopped)
                {
                    return command;
                }
            }
        }
    }
    // Attacks, the strongest kinds first. With no enemy beside it, a creature
    // whose ranged weapon rates more than half its melee weapon shoots, held
    // or drawn, as the original readied its bow (#266).
    const bool enemy_beside =
        std::any_of(in_view.begin(), in_view.end(),
                    [&](const auto *enemy)
                    {
                        return enemy->conscious && squares_apart(*enemy, active) <= 1;
                    });
    const bool by_rating = tuning.weapon_choice == ai_tuning::WeaponChoice::original_rating;
    const bool shoots = by_rating && !enemy_beside &&
                        active.ranged_rating > ai_tuning::shot_worth_share * active.melee_rating;
    // A thrown weapon is rated as a ranged one, as the original did (#269).
    const bool throws = by_rating && !enemy_beside &&
                        active.thrown_rating > ai_tuning::shot_worth_share * active.melee_rating;
    // An unarmored Wizard or Sorcerer, or a monster magic-user, never closes
    // in, as the original's magic-users did not: it casts from where it stands.
    const bool stays_back = active.unarmored_arcane_caster && !active.panicked && !seeking;
    // The weapon the difficulty picks (section 3 of docs/AI.md), if it picks one;
    // `closes` walks up to the enemy instead of using a weapon from afar.
    std::string_view chosen_weapon;
    bool closes = false;
    if (tuning.weapon_choice == ai_tuning::WeaponChoice::arbitrary)
    {
        std::vector<std::string_view> choices;
        for (const auto verb : {"melee", "ranged", "shoot", "throw"})
        {
            if (aim(verb))
            {
                choices.push_back(verb);
            }
        }
        if (!aim("melee") && !stays_back && !choices.empty())
        {
            choices.push_back("close");
        }
        if (!choices.empty())
        {
            const auto choice = choices[pick(state, active.id, 7777, choices.size())];
            closes = choice == "close";
            chosen_weapon = closes ? std::string_view{} : choice;
        }
    }
    else if (tuning.weapon_choice == ai_tuning::WeaponChoice::expected_damage)
    {
        const Command *best = nullptr;
        std::string_view best_verb;
        for (const auto verb : {"ranged", "shoot", "throw"})
        {
            if (const auto *weapon = aim(verb);
                weapon && (!best || weapon->expected_damage > best->expected_damage))
            {
                best = weapon;
                best_verb = verb;
            }
        }
        if (const auto *melee = aim("melee"))
        {
            // A tie stays in melee.
            chosen_weapon =
                best && best->expected_damage > melee->expected_damage ? best_verb : "melee";
        }
        else if (best)
        {
            // As the original: closing in costs time, so a shot worth more than
            // half the melee weapon is taken.
            closes = !stays_back &&
                     best->expected_damage <= ai_tuning::shot_worth_share * best->close_damage;
            chosen_weapon = closes ? std::string_view{} : best_verb;
        }
    }
    // Choosing by damage, it first takes the strongest melee weapon in its hands.
    // Selecting costs nothing, and what it left is then the weaker, so this settles.
    if (chosen_weapon == "melee")
    {
        const Command *stronger = nullptr;
        for (const auto &command : offered)
        {
            if (command.verb == "weapon_select" &&
                command.expected_damage >
                    (stronger ? stronger->expected_damage : active.melee_rating))
            {
                stronger = &command;
            }
        }
        if (stronger && tuning.weapon_choice == ai_tuning::WeaponChoice::expected_damage)
        {
            return *stronger;
        }
    }
    // A Breath Weapon, Dragon's Breath, a dragon's Lightning Breath, an
    // Ankheg's Acid Spray or a Basilisk's Petrifying Gaze goes off before the
    // attacks, where it catches an enemy and no ally (#269).
    for (const auto &command : offered)
    {
        if ((command.verb.starts_with("breath_weapon_") ||
             command.verb.starts_with("dragons_breath_exhale_") ||
             command.verb == "lightning_breath" || command.verb == "acid_spray" ||
             command.verb == "petrifying_gaze") &&
            std::any_of(in_view.begin(), in_view.end(),
                        [&](const auto *enemy)
                        {
                            return safe_area_target(state, active, command.spell, *enemy);
                        }))
        {
            return command;
        }
    }
    // A monster that would rather shoot, its shield in the way of its bow, spends
    // its Action taking the shield off while no enemy is beside it. The weapon is
    // weighed by its rating at every difficulty: the rules offer no shot to weigh yet.
    if (!enemy_beside && !stays_back && !in_view.empty() &&
        active.ranged_rating > ai_tuning::shot_worth_share * active.melee_rating)
    {
        if (const auto *command = offers("doff_shield"))
        {
            return *command;
        }
    }
    std::vector<std::string_view> attacks{
        "rampage",    "eldritch_blast",  "sorcerous_burst_fire", "flame_blade_strike",
        "hurl_flame", "heat_metal_again"};
    if (shoots)
    {
        attacks.insert(attacks.end(), {"ranged", "shoot"});
    }
    if (throws)
    {
        attacks.push_back("throw");
    }
    if (!chosen_weapon.empty())
    {
        attacks.push_back(chosen_weapon);
    }
    attacks.insert(attacks.end(),
                   {"melee", "fire_bolt", "sacred_flame", "vicious_mockery", "starry_wisp",
                    "ray_of_frost", "chill_touch", "shocking_grasp", "poison_spray"});
    if (!closes)
    {
        attacks.push_back("ranged");
    }
    // Squares from a place to the nearest enemy in view, walls aside.
    const auto gap_to_enemies = [&](Cell place)
    {
        int gap = 10000;
        for (const auto *enemy : in_view)
        {
            for (const auto square : squares_of(*enemy))
            {
                gap = std::min(
                    gap, std::max(std::abs(square.x - place.x), std::abs(square.y - place.y)));
            }
        }
        return gap;
    };
    // Nightmare and up keep their distance from what they shoot; Infernal first
    // slips free of an enemy beside it when that costs no Action. Every ranged
    // weapon reaches 4 squares without penalty, so it goes no further than that,
    // and only where it still sees its target.
    constexpr int keep_away_squares = ai_tuning::keep_away_squares;
    const bool shooting = !chosen_weapon.empty() && chosen_weapon != "melee";
    if (tuning.disengages_to_shoot && shooting && enemy_beside && !active.disengaged)
    {
        if (const auto *slip = offers("cunning_disengage"))
        {
            return *slip;
        }
    }
    if (tuning.keeps_distance && shooting && active.action &&
        (!enemy_beside || active.disengaged) && gap_to_enemies(active.cell) < keep_away_squares)
    {
        const auto target_id = aim(chosen_weapon)->target;
        const auto &target = *std::find_if(state.combatants.begin(), state.combatants.end(),
                                           [&](const auto &a)
                                           {
                                               return a.id == target_id;
                                           });
        const auto steps_to = [&](Cell place)
        {
            return std::max(std::abs(place.x - active.cell.x), std::abs(place.y - active.cell.y));
        };
        const auto keeps_target_in_sight = [&](Cell place)
        {
            auto there = active;
            there.cell = place;
            return sees(board, there, target);
        };
        const Command *away = nullptr;
        int widest = gap_to_enemies(active.cell);
        for (const auto &command : offered)
        {
            if (command.verb != "move" || !keeps_target_in_sight(command.destination))
            {
                continue;
            }
            const int gap = std::min(gap_to_enemies(command.destination), keep_away_squares);
            if (gap > widest || (away && gap == widest &&
                                 steps_to(command.destination) < steps_to(away->destination)))
            {
                away = &command;
                widest = gap;
            }
        }
        if (away)
        {
            return *away;
        }
    }
    // Able to attack without moving, a Rogue takes Steady Aim first (#269).
    if (const auto *steady = offers("steady_aim"); steady && active.action &&
                                                   std::any_of(attacks.begin(), attacks.end(),
                                                               [&](auto verb)
                                                               {
                                                                   return aim(verb) != nullptr;
                                                               }))
    {
        return *steady;
    }
    for (const auto verb : attacks)
    {
        if (const auto *command = aim(verb))
        {
            return *command;
        }
    }
    // Its Action spent, a Fighter surges for another while an enemy is in view
    // (#269).
    if (const auto *surge = offers("action_surge"); surge && !active.action && enemy_seen)
    {
        return *surge;
    }
    // With the Action spent, a Bard's Compulsion names a direction; its aim
    // starts on the nearest foe, sending the charmed away.
    if (const auto *heading = offers("compulsion_heading");
        heading && !active.action && !in_view.empty())
    {
        return *heading;
    }
    // With the Action spent, a Bonus Action may still strike: Flurry of Blows,
    // a Martial Arts strike, Hex or Heat Metal again.
    if (!active.action)
    {
        for (const auto verb : {"flurry_of_blows", "martial_arts", "hex_move", "heat_metal_again"})
        {
            if (const auto *command = offers(verb))
            {
                return *command;
            }
        }
    }
    // With no action left, approaching for another attack cannot help this
    // turn, unless a Gnoll's Rampage still has its Rend to make. The
    // bonus-action recovery choices above still get their chance.
    if (!active.action && !shows_spell(active, "Rampaging"))
    {
        for (const auto &command : offered)
        {
            if (command.verb == "end")
            {
                return command;
            }
        }
    }
    // Out of reach of its target even with all its movement, a Rogue or an Orc
    // dashes with its Bonus Action and keeps its Action to attack (#269).
    int reachable = nearest(active.cell);
    for (const auto &command : offered)
    {
        if (command.verb == "move")
        {
            reachable = std::min(reachable, nearest(command.destination));
        }
    }
    if (active.action && !stays_back && !in_view.empty() && reachable > 1 && reachable < 10000)
    {
        for (const auto verb : {"cunning_dash", "adrenaline_rush"})
        {
            if (const auto *command = offers(verb))
            {
                return *command;
            }
        }
    }
    // The walk looks past creatures, so a creature whose way in is held by its
    // own side, with members down around it, can find no step closer and stand
    // there for good (the Slums trolls' room). It then goes around creatures to
    // a free square beside an enemy it walks for, when one can be reached.
    const bool can_get_closer = std::any_of(
        offered.begin(), offered.end(),
        [&](const Command &command)
        {
            return command.verb == "move" && nearest(command.destination) < nearest(active.cell);
        });
    const auto detour = [&]() -> const Command *
    {
        if (can_get_closer || stays_back || in_view.empty())
        {
            return nullptr;
        }
        std::vector<bool> held(board.terrain.size()), blocked(board.terrain.size());
        for (const auto &unit : state.combatants)
        {
            if (unit.id == active.id || unit.dead || unit.fled || !unit.conscious)
            {
                continue;
            }
            for (const auto square : squares_of(unit))
            {
                if (board.contains(square))
                {
                    held[index(square)] = true;
                    blocked[index(square)] = blocked[index(square)] || unit.side != active.side;
                }
            }
        }
        const auto steps_between = [&](Cell from, Cell to)
        {
            return std::max(std::abs(from.x - to.x), std::abs(from.y - to.y)) == 1 &&
                   (from.x == to.x || from.y == to.y ||
                    (board.at({from.x, to.y}) != Terrain::obstacle &&
                     board.at({to.x, from.y}) != Terrain::obstacle));
        };
        std::vector<int> around(board.terrain.size(), 10000);
        std::queue<Cell> open;
        for (int y = 0; y < board.height; ++y)
        {
            for (int x = 0; x < board.width; ++x)
            {
                const Cell place{x, y};
                if (board.at(place) == Terrain::obstacle || held[index(place)])
                {
                    continue;
                }
                for (const auto *enemy : in_view)
                {
                    for (const auto square : squares_of(*enemy))
                    {
                        if ((!walk_target || enemy == walk_target) &&
                            steps_between(place, square) && around[index(place)] != 0)
                        {
                            around[index(place)] = 0;
                            open.push(place);
                        }
                    }
                }
            }
        }
        while (!open.empty())
        {
            const auto p = open.front();
            open.pop();
            for (int y = -1; y <= 1; ++y)
            {
                for (int x = -1; x <= 1; ++x)
                {
                    const Cell next{p.x + x, p.y + y};
                    if (!board.contains(next) || board.at(next) == Terrain::obstacle ||
                        blocked[index(next)] || !steps_between(p, next) ||
                        around[index(next)] <= around[index(p)] + 1)
                    {
                        continue;
                    }
                    around[index(next)] = around[index(p)] + 1;
                    open.push(next);
                }
            }
        }
        const Command *best = nullptr;
        int fewest = around[index(active.cell)];
        for (const auto &command : offered)
        {
            if (command.verb == "move" && around[index(command.destination)] < fewest)
            {
                best = &command;
                fewest = around[index(command.destination)];
            }
        }
        return best;
    };
    const auto *way_around = detour();
    // With nothing to attack yet, Aggressive or a Troll's Charge covers more
    // ground toward an enemy in view, when there is ground to cover.
    for (const auto &command : offered)
    {
        if ((command.verb == "aggressive" || command.verb == "bonus_charge") && !in_view.empty() &&
            !stays_back && (can_get_closer || way_around))
        {
            return command;
        }
    }
    // Nightmare and up, between equally close squares, the one beside the fewest
    // enemies.
    const auto enemies_beside = [&](Cell place)
    {
        return std::count_if(in_view.begin(), in_view.end(),
                             [&](const auto *enemy)
                             {
                                 const auto squares = squares_of(*enemy);
                                 return std::any_of(
                                     squares.begin(), squares.end(),
                                     [&](Cell square)
                                     {
                                         return std::max(std::abs(square.x - place.x),
                                                         std::abs(square.y - place.y)) <= 1;
                                     });
                             });
    };
    const Command *move = nullptr;
    int closest = nearest(active.cell);
    for (const auto &command : offered)
    {
        if (stays_back || command.verb != "move")
        {
            continue;
        }
        const int steps = nearest(command.destination);
        if (steps < closest ||
            (tuning.avoids_crowds && move && steps == closest &&
             enemies_beside(command.destination) < enemies_beside(move->destination)))
        {
            move = &command;
            closest = steps;
        }
    }
    if (move)
    {
        return *move;
    }
    if (way_around)
    {
        return *way_around;
    }
    // Unable to get any closer, a character shoots or throws what it has: a
    // carried bow or crossbow drawn, and whatever it meant to walk past.
    for (const auto verb : {"shoot", "ranged", "throw"})
    {
        if (const auto *shot = aim(verb))
        {
            return *shot;
        }
    }
    // Stuck with no enemy it can strike in melee (and so none that can strike
    // it), a shield-bearer carrying a bow takes its shield off to shoot.
    if (!can_strike)
    {
        if (const auto *command = offers("doff_shield"))
        {
            return *command;
        }
    }
    // With its Action unused and an enemy beside it, it Dodges (#269).
    if (const auto *dodge = offers("dodge"); dodge && active.action && enemy_beside)
    {
        return *dodge;
    }
    for (const auto &command : offered)
    {
        if (command.verb == "end")
        {
            return command;
        }
    }
    return offered.front();
}
} // namespace

namespace
{
// Steps from the given squares to every open square, around walls; -1 where no
// way leads.
std::vector<int> steps_from(const rules::Battlefield &board, const std::vector<Cell> &sources)
{
    const auto index = [&](Cell p)
    {
        return static_cast<std::size_t>(p.y * board.width + p.x);
    };
    std::vector<int> steps(static_cast<std::size_t>(board.width * board.height), -1);
    std::queue<Cell> frontier;
    for (const auto source : sources)
    {
        if (board.contains(source) && steps[index(source)] < 0)
        {
            steps[index(source)] = 0;
            frontier.push(source);
        }
    }
    while (!frontier.empty())
    {
        const auto p = frontier.front();
        frontier.pop();
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (const Cell next{p.x + dx, p.y + dy}; board.contains(next) &&
                                                         board.at(next) != Terrain::obstacle &&
                                                         steps[index(next)] < 0)
                {
                    steps[index(next)] = steps[index(p)] + 1;
                    frontier.push(next);
                }
            }
        }
    }
    return steps;
}

// Running off the field: the step toward an open edge of the field, around
// walls, Dashing for more movement, then the flee itself. A party member runs
// for the nearest edge; a panicked creature for the nearest edge square farther
// from its opponents than from itself (the original sends it away from them).
// Empty when no step gets it closer.
std::optional<Command> run_for_edge(const rules::Snapshot &state,
                                    const std::vector<Command> &offered,
                                    const rules::CombatantView &active)
{
    const auto offer = [&](std::string_view verb) -> const Command *
    {
        for (const auto &command : offered)
        {
            if (command.verb == verb && command.actor == active.id)
            {
                return &command;
            }
        }
        return nullptr;
    };
    if (const auto *flee = offer("flee"))
    {
        return *flee;
    }
    // With an enemy beside it, it Disengages before running, so it is not hit
    // as it leaves (#269).
    const bool enemy_beside = std::any_of(state.combatants.begin(), state.combatants.end(),
                                          [&](const auto &other)
                                          {
                                              return other.side != active.side && other.conscious &&
                                                     squares_apart(other, active) <= 1;
                                          });
    // With nowhere left to move this turn (its run just failed), it has no use for it.
    const bool can_move =
        std::any_of(offered.begin(), offered.end(),
                    [&](const Command &command)
                    {
                        return command.verb == "move" && command.actor == active.id;
                    });
    if (enemy_beside && !active.disengaged && can_move)
    {
        for (const auto verb : {"cunning_disengage", "disengage"})
        {
            if (const auto *disengage = offer(verb))
            {
                return *disengage;
            }
        }
    }
    const auto &board = state.battlefield;
    const auto index = [&](Cell p)
    {
        return static_cast<std::size_t>(p.y * board.width + p.x);
    };
    const auto distances = [&](const std::vector<Cell> &sources)
    {
        return steps_from(board, sources);
    };
    std::vector<Cell> edges;
    for (int y = 0; y < board.height; ++y)
    {
        for (int x = 0; x < board.width; ++x)
        {
            if ((x == 0 || y == 0 || x == board.width - 1 || y == board.height - 1) &&
                board.at({x, y}) != Terrain::obstacle)
            {
                edges.push_back({x, y});
            }
        }
    }
    if (active.side != Side::party)
    {
        std::vector<Cell> opponents;
        for (const auto &other : state.combatants)
        {
            if (other.side != active.side && other.hit_points > 0 && !other.dead && !other.fled)
            {
                const auto squares = squares_of(other);
                opponents.insert(opponents.end(), squares.begin(), squares.end());
            }
        }
        const auto mine = distances({active.cell}), theirs = distances(opponents);
        std::vector<Cell> away;
        for (const auto edge : edges)
        {
            if (mine[index(edge)] >= 0 &&
                (theirs[index(edge)] < 0 || theirs[index(edge)] > mine[index(edge)]))
            {
                away.push_back(edge);
            }
        }
        if (!away.empty())
        {
            edges = std::move(away);
        }
    }
    const auto to_edge = distances(edges);
    const auto steps_to_edge = [&](Cell p)
    {
        const auto d = to_edge[index(p)];
        return d < 0 ? 1 << 20 : d;
    };
    const Command *move = nullptr;
    int closest = steps_to_edge(active.cell);
    for (const auto &command : offered)
    {
        if (command.verb == "move" && command.actor == active.id &&
            steps_to_edge(command.destination) < closest)
        {
            move = &command;
            closest = steps_to_edge(command.destination);
        }
    }
    if (move)
    {
        return *move;
    }
    // Out of movement short of the edge: Dash for more.
    if (steps_to_edge(active.cell) > 0 && steps_to_edge(active.cell) < 1 << 20)
    {
        if (const auto *dash = offer("dash"))
        {
            return *dash;
        }
    }
    return std::nullopt;
}

// Charging in panic (MORALE-2): the rules offer it nothing but running and lashing
// out. It runs at the nearest enemy, wherever that is, so that the fight ends.
Command charge_in_panic(const rules::Snapshot &state, const std::vector<Command> &offered,
                        const rules::CombatantView &active)
{
    const auto offer = [&](std::string_view verb) -> const Command *
    {
        const auto found =
            std::find_if(offered.begin(), offered.end(),
                         [&](const Command &command)
                         {
                             return command.verb == verb && command.actor == active.id;
                         });
        return found == offered.end() ? nullptr : &*found;
    };
    std::vector<Cell> enemy_squares;
    bool enemy_beside = false;
    for (const auto &other : state.combatants)
    {
        if (other.side != active.side && other.hit_points > 0 && !other.dead && !other.fled)
        {
            const auto squares = squares_of(other);
            enemy_squares.insert(enemy_squares.end(), squares.begin(), squares.end());
            enemy_beside = enemy_beside || squares_apart(other, active) <= 1;
        }
    }
    const auto *lash_out = offer("lash_out");
    if (enemy_beside && lash_out)
    {
        return *lash_out;
    }
    const auto &board = state.battlefield;
    const auto steps = steps_from(board, enemy_squares);
    const auto steps_at = [&](Cell p)
    {
        const auto found = steps[static_cast<std::size_t>(p.y * board.width + p.x)];
        return found < 0 ? 1 << 20 : found;
    };
    const Command *move = nullptr;
    int closest = steps_at(active.cell);
    for (const auto &command : offered)
    {
        if (command.verb == "move" && command.actor == active.id &&
            steps_at(command.destination) < closest)
        {
            move = &command;
            closest = steps_at(command.destination);
        }
    }
    if (move)
    {
        return *move;
    }
    if (lash_out)
    {
        return *lash_out;
    }
    if (const auto *dash = offer("dash"); dash && !enemy_beside)
    {
        return *dash;
    }
    return *offer("end");
}
} // namespace

namespace
{
// A carried item's use: a potion, wand, missile or dust (MAGIC-3), or a spell
// read from a scroll (MAGIC-6).
bool item_verb(std::string_view verb)
{
    return verb.ends_with("_item") || verb.ends_with("_item_bonus") ||
           verb.find("_scroll") != std::string_view::npos;
}

// Whether an item's area aimed at `center` might catch an ally, judged before
// aiming and on the safe side of the area the rules will draw: the necklace's
// Fireball within 5 squares of its center, and a lightning wand's bolt within
// a square of its line from the user out past the center.
bool area_risks_ally(const rules::Snapshot &state, const CombatantView &active,
                     std::string_view verb, Cell center)
{
    const auto lightning = verb.starts_with("lightning_bolt");
    return std::any_of(state.combatants.begin(), state.combatants.end(),
                       [&](const auto &a)
                       {
                           if (a.side != active.side || a.dead)
                           {
                               return false;
                           }
                           if (!lightning)
                           {
                               return squares_from(center, a) <= 5;
                           }
                           if (a.id == active.id)
                           {
                               return false;
                           }
                           const double dx = center.x - active.cell.x,
                                        dy = center.y - active.cell.y;
                           const double length = std::max(1.0, dx * dx + dy * dy);
                           const double cx = a.cell.x - active.cell.x,
                                        cy = a.cell.y - active.cell.y;
                           const double along = (cx * dx + cy * dy) / length;
                           const double off_x = cx - along * dx, off_y = cy - along * dy;
                           return along > 0 && off_x * off_x + off_y * off_y <= 2.0;
                       });
}

// A magic item a Quick character uses now (MAGIC-3), as the original's did:
// healing for a creature at half its Hit Points or lower, Speed or Giant
// Strength once, a wand at an enemy (Paralysis at one not yet Paralyzed), a
// scroll's spell (MAGIC-6), and an area only where no ally can be caught. Dust
// of Disappearance is left to players: attacking ends its Invisibility.
std::optional<Command> choose_item(const rules::Snapshot &state, const CombatantView &active,
                                   const std::vector<Command> &offered)
{
    const auto unit = [&](EntityId id) -> const CombatantView *
    {
        const auto found = std::find_if(state.combatants.begin(), state.combatants.end(),
                                        [&](const auto &a)
                                        {
                                            return a.id == id;
                                        });
        return found == state.combatants.end() ? nullptr : &*found;
    };
    // An item's area being aimed: set off where the drawn area holds an enemy
    // and no ally, else aimed at the next safe enemy, else given up.
    if (const auto &aim = state.area_targeting; aim && item_verb(aim->verb))
    {
        const auto in_area = [&](const CombatantView &a)
        {
            return !a.dead && in_cells(aim->cells, a);
        };
        const bool ally = std::any_of(state.combatants.begin(), state.combatants.end(),
                                      [&](const auto &a)
                                      {
                                          return a.side == active.side && in_area(a);
                                      });
        const bool enemy =
            std::any_of(state.combatants.begin(), state.combatants.end(),
                        [&](const auto &a)
                        {
                            return a.side != active.side && a.conscious && in_area(a);
                        });
        for (const auto &command : offered)
        {
            if (enemy && !ally && command.verb == "area_cast")
            {
                return command;
            }
        }
        for (const auto &a : state.combatants)
        {
            if (a.side == active.side || !a.conscious || a.cell == aim->center ||
                area_risks_ally(state, active, aim->verb, a.cell))
            {
                continue;
            }
            for (const auto &command : offered)
            {
                if (command.verb == "area_move" && command.destination == a.cell)
                {
                    return command;
                }
            }
        }
        for (const auto &command : offered)
        {
            if (command.verb == "spell_cancel")
            {
                return command;
            }
        }
        return std::nullopt;
    }
    for (const auto &command : offered)
    {
        if (!item_verb(command.verb))
        {
            continue;
        }
        const auto *target = unit(command.target);
        const auto verb = std::string_view(command.verb);
        // A scroll's spell (MAGIC-6): healing for a creature at half its Hit
        // Points or lower, a spell at an enemy, or an area only where no ally
        // can be caught; spells that help an ally otherwise wait for the
        // player.
        if (verb.find("_scroll") != std::string_view::npos && !command.aims_area)
        {
            const bool heals = verb.starts_with("cure_wounds") || verb.starts_with("healing_word");
            if (heals ? target && target->side == active.side && is_hurt(*target)
                      : target && target->side != active.side && target->conscious)
            {
                return command;
            }
            continue;
        }
        if (verb.starts_with("potion_of_healing") || verb.starts_with("potion_of_greater_healing"))
        {
            if (target && is_hurt(*target))
            {
                return command;
            }
        }
        else if (verb.starts_with("potion_of_speed") || verb.starts_with("potion_of_giant"))
        {
            if (target && target->id == active.id)
            {
                return command;
            }
        }
        else if (verb.starts_with("wand_of_paralysis"))
        {
            if (target && target->side != active.side &&
                std::none_of(target->conditions.begin(), target->conditions.end(),
                             [](const auto &c)
                             {
                                 return c.source.starts_with("Paralyzed");
                             }))
            {
                return command;
            }
        }
        else if (verb.starts_with("magic_missile"))
        {
            if (target && target->side != active.side)
            {
                return command;
            }
        }
        else if (command.aims_area)
        {
            // Only an enemy it can aim at, in range and sight: one out of
            // reach would be given up while aiming, and chosen again.
            const bool safe_enemy =
                std::any_of(state.combatants.begin(), state.combatants.end(),
                            [&](const auto &a)
                            {
                                return safe_area_target(state, active, command.spell, a) &&
                                       !area_risks_ally(state, active, verb, a.cell);
                            });
            if (safe_enemy)
            {
                return command;
            }
        }
    }
    return std::nullopt;
}

// The commands a Quick member with magic off chooses from: all but the active
// creature's spells.
std::vector<Command> without_spells(const rules::Snapshot &state, std::vector<Command> commands)
{
    std::vector<std::string> spells;
    for (const auto &a : state.combatants)
    {
        if (a.id == state.actor)
        {
            spells = a.spells;
        }
    }
    // A magic item is used whether or not Quick magic is on (MAGIC-3).
    std::erase_if(commands,
                  [&](const auto &command)
                  {
                      return !item_verb(command.verb) &&
                             std::any_of(spells.begin(), spells.end(),
                                         [&](const auto &spell)
                                         {
                                             return command.verb == spell ||
                                                    command.verb.starts_with(spell + "_");
                                         });
                  });
    return commands;
}
} // namespace

Command choose_quick_command(const CombatSession &session, bool magic, Searching searching)
{
    constexpr auto quick_difficulty = ai_tuning::quick_difficulty;
    if (magic)
    {
        // The original's AI casts for a party member with Quick magic on.
        const auto offered = session.legal_commands();
        if (const auto own_ai =
                std::ranges::find(offered, std::string_view("quick_magic"), &Command::verb);
            own_ai != offered.end())
        {
            return *own_ai;
        }
        return choose_demo_command(session, searching, quick_difficulty);
    }
    const auto state = session.snapshot();
    return choose_from(state, without_spells(state, session.legal_commands()), searching,
                       quick_difficulty);
}

Command choose_flee_command(const CombatSession &session, bool magic, Searching searching,
                            Difficulty difficulty)
{
    const auto state = session.snapshot();
    const auto offered = session.legal_commands();
    const auto active = std::find_if(state.combatants.begin(), state.combatants.end(),
                                     [&](const auto &a)
                                     {
                                         return a.id == state.actor;
                                     });
    const auto end = std::find_if(offered.begin(), offered.end(),
                                  [&](const auto &c)
                                  {
                                      return c.verb == "end" && c.actor == state.actor;
                                  });
    // Monsters, on either side, fight on by the difficulty and with their spells
    // unless they can run; a member who cannot flee, a reaction or a choice is
    // played as on Quick.
    if (active == state.combatants.end() || active->side != Side::party ||
        (is_ally(active->id) && !active->can_flee))
    {
        return choose_demo_command(session, searching, difficulty);
    }
    if (!active->can_flee || state.reaction_pending || end == offered.end())
    {
        return choose_quick_command(session, magic, searching);
    }
    if (const auto run = run_for_edge(state, offered, *active))
    {
        return *run;
    }
    return *end;
}
} // namespace opengold
