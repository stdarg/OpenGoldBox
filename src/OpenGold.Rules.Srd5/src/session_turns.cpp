#include "session.h"
#include <iterator>
#include <limits>
#include <stdexcept>

// Session: the turn order, saving throws, the outcome, and what happens as
// a turn begins, advances and ends.
namespace opengold::srd5::detail
{
using namespace rules;

bool Session::confused_turn(Actor &a)
{
    // No Bonus Actions or Reactions, and a d10 for the turn.
    a.bonus = false;
    a.reaction = false;
    const int behavior = roll(10);
    if (behavior >= 9)
    {
        log(a.source.name + " shakes off the confusion this turn (d10 " + std::to_string(behavior) +
                ").",
            {"{name} shakes off the confusion this turn (d10 {roll}).",
             {{"name", a.source.name}, {"roll", std::to_string(behavior)}}});
        return true;
    }
    if (behavior == 1)
    {
        // It wanders north, east, south or west as far as its movement allows.
        static constexpr std::array<Cell, 4> directions{Cell{0, -1}, Cell{1, 0}, Cell{0, 1},
                                                        Cell{-1, 0}};
        const auto step = directions[static_cast<std::size_t>(roll(4) - 1)];
        int left = movement_left(a);
        const auto board = zoned_board();
        for (Cell next{a.source.cell.x + step.x, a.source.cell.y + step.y};
             left >= 5 && !blocked_for(a, next, board_); next = {next.x + step.x, next.y + step.y})
        {
            left -= board.at(next) == Terrain::difficult ? 10 : 5;
            if (left < 0)
            {
                break;
            }
            a.source.cell = next;
        }
        log(a.source.name + " wanders in confusion.",
            {"{name} wanders in confusion.", {{"name", a.source.name}}});
        return false;
    }
    if (behavior <= 6)
    {
        log(a.source.name + " stands confused and does nothing.",
            {"{name} stands confused and does nothing.", {{"name", a.source.name}}});
        return false;
    }
    // A melee attack against a random creature within reach, if there is one.
    const auto near = creatures_in_reach(a);
    if (near.empty())
    {
        log(a.source.name + " stands confused and does nothing.",
            {"{name} stands confused and does nothing.", {{"name", a.source.name}}});
        return false;
    }
    auto &target = actor(near[static_cast<std::size_t>(roll(static_cast<int>(near.size())) - 1)]);
    log(a.source.name + " lashes out in confusion.",
        {"{name} lashes out in confusion.", {{"name", a.source.name}}});
    attack(a, target, AttackRange::melee);
    return false;
}

std::vector<EntityId> Session::creatures_in_reach(const Actor &a) const
{
    std::vector<EntityId> near;
    for (const auto &other : actors_)
    {
        if (other.source.id != a.source.id && !other.life.dead && other.life.hp > 0 &&
            !absent(other) && distance(a, other) <= def(a).reach && def(a).melee.count)
        {
            near.push_back(other.source.id);
        }
    }
    return near;
}

void Session::charge_in_panic(std::vector<Command> &offered, const Actor &a) const
{
    const bool can_strike = std::any_of(offered.begin(), offered.end(),
                                        [](const Command &command)
                                        {
                                            return command.verb == "melee";
                                        }) &&
                            !creatures_in_reach(a).empty();
    std::erase_if(offered,
                  [](const Command &command)
                  {
                      return command.verb != "move" && command.verb != "dash" &&
                             command.verb != "stand_up" && command.verb != "end";
                  });
    if (can_strike)
    {
        offered.push_back({revision_, a.source.id, 0, "lash_out", "Lash out", Cell{}});
    }
}

bool Session::sealed_off(const Actor &a) const
{
    return in_antimagic(a.source.cell) ||
           detail::has_effect(a.effects, detail::EffectKind::sphered) ||
           detail::has_effect(a.effects, detail::EffectKind::illusory);
}

void Session::finish_reaction()
{
    ++reactor_index_;
    update_outcome();
    if (outcome_ == Outcome::ongoing)
    {
        if (mover().life.hp == 0)
        {
            path_.clear();
            path_index_ = 0;
            reactors_.clear();
            reactor_index_ = 0;
            flight_.reset();
        }
        else if (!pending() && !stop_illegal_route())
        {
            progress_movement();
        }
    }
}

// Easy (DIFF-2): no member stays dead or down after a fight the party won. A
// member who came in dead is no participant, so this is only this fight's harm.
void Session::spare_the_party()
{
    for (auto &a : actors_)
    {
        if (a.source.side == Side::party && !a.source.character_profile.empty() && a.life.hp <= 0)
        {
            a.life.hp = 1;
            a.life.dead = false;
            a.life.recovery = {};
            log(a.source.name + " gets back up.",
                {"{name} gets back up.", {{"name", a.source.name}}});
        }
    }
}

void Session::update_outcome()
{
    // Decided once: a later call must not announce the outcome again.
    if (outcome_ != Outcome::ongoing)
    {
        return;
    }
    // A dominated creature still counts for its own side.
    bool party = false, enemies = false;
    // Mislead's double is no one.
    for (const auto &a : actors_)
    {
        if (a.life.hp > 0 && !a.life.dead && !a.fled &&
            !detail::has_effect(a.effects, detail::EffectKind::illusory) &&
            !std::any_of(a.effects.active.begin(), a.effects.active.end(),
                         [](const auto &e)
                         {
                             // Stone for good is out of the fight.
                             return e.kind == detail::EffectKind::petrified && e.dc;
                         }))
        {
            ((a.source.side == Side::party) != a.turned ? party : enemies) = true;
        }
    }
    if (!party || !enemies)
    {
        // No member left on the field: it fled if anyone got away.
        const bool party_fled = std::any_of(fled_.begin(), fled_.end(),
                                            [](const auto &gone)
                                            {
                                                return gone.source.side == Side::party;
                                            }) ||
                                std::any_of(actors_.begin(), actors_.end(),
                                            [](const auto &a)
                                            {
                                                return a.fled && a.source.side == Side::party;
                                            });
        outcome_ = party ? Outcome::victory : party_fled ? Outcome::fled : Outcome::defeat;
        // Concentration is tracked in combat only; it ends with the combat, and
        // so do Wild Shape (WILD-1) and Befuddlement (LEVELS-5).
        for (auto &a : actors_)
        {
            end_concentration(a);
            end_wild_shape(a);
            end_animal_shape(a);
            end_befuddlement(a);
        }
        for (auto &a : actors_)
        {
            release(a);
        }
        // Animate Dead's Zombies fall again with the fight (LEVELS-3).
        for (auto &a : actors_)
        {
            if (detail::has_effect(a.effects, detail::EffectKind::raised))
            {
                a.effects.active.clear();
                a.life.hp = 0;
                a.life.dead = true;
                a.life.recovery = {};
                a.source.side = a.source.side == Side::party ? Side::opposition : Side::party;
                log(a.source.name + " falls again.",
                    {"{name} falls again.", {{"name", a.source.name}}});
            }
        }
        if (spares_the_party_ && outcome_ == Outcome::victory)
        {
            spare_the_party();
        }
        champion_move_.reset();
        mastery_.reset();
        champion_offers_.clear();
        effect_reaction_origin_.reset();
        path_.clear();
        path_index_ = 0;
        reactors_.clear();
        reactor_index_ = 0;
        log(outcome_ == Outcome::victory ? "Victory." : "The party is incapacitated. Defeat.");
        if (outcome_ == Outcome::victory)
        {
            // The original game ends the fight once every enemy is down; a
            // downed troll does not get up afterwards.
            for (auto &a : actors_)
            {
                if (a.source.side == Side::opposition && downed_riser(a))
                {
                    a.life.dead = true;
                    a.life.recovery = {};
                    a.rises_on_round = 0;
                    log(a.source.name + " stays down.",
                        {"{name} stays down.", {{"name", a.source.name}}});
                }
            }
            bandage_after_victory();
        }
    }
}

// SIMPLIFY-1: nobody sleeps through a fight. A character whose rest the
// encounter interrupted starts awake but still lying down.
void Session::wake_resting_participants()
{
    for (auto &a : actors_)
    {
        if (a.source.resting && conscious(a))
        {
            a.effects.prone = true;
            log(a.source.name + " wakes up prone.",
                {"{name} wakes up prone.", {{"name", a.source.name}}});
        }
    }
}

// As in the original game, whoever is still dying when the fight is won has
// its wounds bound afterward: it is Stable and recovers on the usual clock.
// Prone does not outlast combat: the party is standing (or lying unconscious)
// when exploration resumes.
void Session::bandage_after_victory()
{
    for (auto &a : actors_)
    {
        if (a.life.hp != 0 || a.life.dead || a.life.stable)
        {
            continue;
        }
        detail::stabilize(a.life, rng_);
        log(a.source.name + " is bandaged and stable.",
            {"{name} is bandaged and stable.", {{"name", a.source.name}}});
    }
    for (auto &a : actors_)
    {
        a.effects.prone = false;
    }
}

// Regeneration at the start of the creature's turn: Acid or Fire damage since its
// last turn stops it, and a creature at 0 HP that cannot regenerate dies.
void Session::regenerate(Actor &a)
{
    const bool blocked = a.regeneration_blocked;
    a.regeneration_blocked = false;
    if (blocked)
    {
        if (a.life.hp > 0)
        {
            log(a.source.name + " cannot regenerate this turn.",
                {"{name} cannot regenerate this turn.", {{"name", a.source.name}}});
            return;
        }
        a.life.dead = true;
        a.life.stable = false;
        a.life.recovery = {};
        a.rises_on_round = 0;
        log(a.source.name + " cannot regenerate and dies.",
            {"{name} cannot regenerate and dies.", {{"name", a.source.name}}});
        clear_departed_overlaps();
        return;
    }
    if (a.life.hp == 0)
    {
        get_up(a);
        return;
    }
    const int healed = detail::heal_life(a.life, def(a).regeneration, max_hp(a),
                                         !detail::healing_blocked(a.effects));
    if (!healed)
    {
        return;
    }
    log(a.source.name + " regenerates " + std::to_string(healed) + " Hit Points.",
        {"{name} regenerates {amount} Hit Points.",
         {{"name", a.source.name}, {"amount", std::to_string(healed)}}});
}

// As in the original game, a downed creature with Regeneration gets back up with
// all its Hit Points once its 3d6 rounds have passed, but not while a creature
// stands on its square; it tries again on each of its turns.
void Session::get_up(Actor &a)
{
    const bool stood_on = std::any_of(actors_.begin(), actors_.end(),
                                      [&](const auto &other)
                                      {
                                          return other.source.id != a.source.id &&
                                                 conscious(other) && distance(other, a) == 0;
                                      });
    if (static_cast<int>(round_) < a.rises_on_round || stood_on)
    {
        return;
    }
    if (!detail::heal_life(a.life, max_hp(a), max_hp(a), !detail::healing_blocked(a.effects)))
    {
        return;
    }
    a.rises_on_round = 0;
    a.effects.prone = true;
    if (shares_occupied_space(a))
    {
        a.involuntary_overlap = true;
    }
    log(a.source.name + " gets back up.", {"{name} gets back up.", {{"name", a.source.name}}});
}

bool Session::begin_turn()
{
    for (auto &other : actors_)
    {
        release(other);
    }
    auto &a = actors_[turn_];
    // Legendary Actions return as the monster's turn starts.
    std::erase_if(a.effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::legendary_spent;
                  });
    if (a.fled)
    {
        return false;
    }
    if (a.skips_turn)
    {
        a.skips_turn = false;
        return false;
    }
    if (a.source.side != Side::party && conscious(a))
    {
        check_morale(a);
        if (a.fled)
        {
            return false;
        }
    }
    std::erase_if(a.effects.active,
                  [](const auto &effect)
                  {
                      return effect.kind == detail::EffectKind::shocking_grasp;
                  });
    // Maze: the lost creature's turn goes to seeking the way out.
    if (!a.life.dead && detail::has_effect(a.effects, detail::EffectKind::mazed))
    {
        seek_maze_exit(a);
        return false;
    }
    // A Phase Spider's Ethereal Jaunt back takes this turn's Bonus Action.
    const bool jaunted_back = !a.life.dead && jaunt_back(a);
    if (a.life.dead || absent(a))
    {
        return false;
    }
    // Finger of Death: the caster's slain Humanoids rise as its Zombies.
    for (auto &other : actors_)
    {
        if (other.life.dead && std::any_of(other.effects.active.begin(), other.effects.active.end(),
                                           [&](const auto &e)
                                           {
                                               return e.kind == detail::EffectKind::rising &&
                                                      e.source_actor == a.source.id &&
                                                      e.source_scope == scope_;
                                           }))
        {
            raise_undead(a, other, "zombie");
        }
    }
    // Prismatic Spray's violet ray: a Wisdom save ends the blindness; a
    // failure sends the creature to another plane, which counts as defeated
    // (LEVELS-1).
    for (auto &other : actors_)
    {
        const auto violet = std::find_if(other.effects.active.begin(), other.effects.active.end(),
                                         [&](const auto &e)
                                         {
                                             return e.kind == detail::EffectKind::violet &&
                                                    e.source_actor == a.source.id &&
                                                    e.source_scope == scope_;
                                         });
        if (violet == other.effects.active.end() || other.life.dead)
        {
            continue;
        }
        const int dc = violet->dc;
        other.effects.active.erase(violet);
        if (saving_throw_succeeds(other, detail::Ability::wisdom, dc))
        {
            log(other.source.name + " is no longer Blinded.",
                {"{name} is no longer Blinded.", {{"name", other.source.name}}});
            continue;
        }
        other.life.hp = 0;
        other.life.dead = true;
        other.life.recovery = {};
        log(other.source.name + " is sent to another plane.",
            {"{name} is sent to another plane.", {{"name", other.source.name}}});
    }
    // Mislead's double takes no turns of its own.
    if (detail::has_effect(a.effects, detail::EffectKind::illusory))
    {
        return false;
    }
    // Cloudkill and Incendiary Cloud drift 10 feet away from their caster as
    // the caster's turn starts.
    for (auto &zone : zones_)
    {
        if ((zone.kind == ZoneKind::cloud || zone.kind == ZoneKind::embers) &&
            zone.caster == a.source.id)
        {
            const Cell from = zone.cells.front();
            const int sx = (from.x > a.source.cell.x) - (from.x < a.source.cell.x);
            const int sy = (from.y > a.source.cell.y) - (from.y < a.source.cell.y);
            Cell to{from.x + 2 * sx, from.y + 2 * sy};
            to = {std::clamp(to.x, 0, board_.width - 1), std::clamp(to.y, 0, board_.height - 1)};
            const auto &spell = *detail::find_spell(
                zone.kind == ZoneKind::embers ? "incendiary_cloud" : "cloudkill");
            const auto before = zone;
            auto cells = area_cells(spell, spell.id, origin_toward(a, to), to);
            std::erase(cells, to);
            cells.insert(cells.begin(), to);
            zone.cells = std::move(cells);
            const auto after = zone;
            log("The cloud drifts.", {"The cloud drifts.", {}});
            for (auto &other : actors_)
            {
                if (in_zone(after, other) && !in_zone(before, other))
                {
                    choke_cloud(after, other);
                }
            }
            break;
        }
    }
    // Conjure Elemental: a creature starting its turn beside the spirit may be
    // seized; one it holds saves again, taking 4d8 on a failure.
    for (const auto &zone : std::vector<Zone>(zones_))
    {
        if (zone.kind == ZoneKind::elemental && std::any_of(zone.cells.begin(), zone.cells.end(),
                                                            [&](Cell cell)
                                                            {
                                                                return distance(cell, a) <= 5;
                                                            }))
        {
            engulf(zone, a);
        }
    }
    for (const auto &e : std::vector<detail::Effect>(a.effects.active))
    {
        if (e.kind == detail::EffectKind::engulfed && e.source_scope == scope_)
        {
            const auto &caster = actor(e.source_actor);
            const auto spirit = std::find_if(zones_.begin(), zones_.end(),
                                             [&](const auto &zone)
                                             {
                                                 return zone.kind == ZoneKind::elemental &&
                                                        zone.caster == caster.source.id;
                                             });
            if (spirit == zones_.end())
            {
                continue;
            }
            const auto &spell = *detail::find_spell("conjure_elemental");
            if (saving_throw_succeeds(a, spell.save, spirit->save_dc))
            {
                std::erase_if(a.effects.active,
                              [&](const auto &other)
                              {
                                  return other.id == e.id;
                              });
                log(a.source.name + " breaks free of the spirit.",
                    {"{name} breaks free of the spirit.", {{"name", a.source.name}}});
                continue;
            }
            const auto type = static_cast<detail::DamageType>(e.dc);
            const auto name = std::string(detail::damage_name(type));
            const int amount = resolved_damage(
                a, type, dice({4 + static_cast<int>(spirit->slot - spell.level), 8, 0}));
            log(a.source.name + " takes " + std::to_string(amount) + " " + name +
                    " damage from the elemental spirit.",
                {"{name} takes {damage} {type} damage from the elemental spirit.",
                 {{"name", a.source.name},
                  {"damage", std::to_string(amount)},
                  {"type", name, true}}});
            damage(a, amount);
        }
    }
    if (a.life.dead)
    {
        return false;
    }
    // Warding Bond ends once its caster is down or more than 60 feet away.
    for (auto &other : actors_)
    {
        const auto kept = bonds_on(other);
        std::erase_if(other.effects.active,
                      [&](const auto &e)
                      {
                          return e.kind == detail::EffectKind::warding_bond &&
                                 std::find(kept.begin(), kept.end(), e.source_actor) == kept.end();
                      });
    }
    burn_searing_smites(a);
    squeeze_ensnared(a);
    guardian_tests(a);
    // Wild Shape ends when it lapses or its Druid is Incapacitated.
    if (a.form && detail::incapacitated(a.effects))
    {
        end_wild_shape(a);
    }
    refresh_form(a);
    // Heat Metal may be repeated from the caster's next turn on.
    for (auto &other : actors_)
    {
        for (auto &effect : other.effects.active)
        {
            if (effect.kind == detail::EffectKind::heated && effect.source_actor == a.source.id)
            {
                effect.dc = 0;
            }
        }
    }
    if (a.life.dead)
    {
        return false;
    }
    spring_zones(a, ZoneKind::web);
    // Heroism: Temporary HP at the start of each of the target's turns, kept
    // only when higher than what the target already has.
    for (const auto &effect : a.effects.active)
    {
        if (effect.kind == detail::EffectKind::heroism && a.life.hp > 0 &&
            effect.dc > a.life.temporary_hp.amount)
        {
            detail::grant_temporary_hp(a.life, {effect.dc, "spell:heroism"},
                                       TemporaryHpChoice::use_new);
        }
    }
    // Persistent Rage (Barbarian 15): the Rage needs no extending.
    if (def(a).persistent_rage && rage_of(a))
    {
        extend_rage(a);
    }
    // Heroic Warrior (Champion 10): Heroic Inspiration at the start of each
    // turn in a fight, when the Fighter lacks it.
    if (def(a).heroic_warrior && conscious(a) &&
        !detail::has_effect(a.effects, detail::EffectKind::heroic_inspiration) &&
        detail::can_apply(a.effects))
    {
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::heroic_inspiration, 0);
        log(a.source.name + " gains Heroic Inspiration (Heroic Warrior).",
            {"{name} gains Heroic Inspiration (Heroic Warrior).", {{"name", a.source.name}}});
    }
    recharge_gaze(a);
    recharge_innate(a);
    drain_attached(a);
    // Burning: 1d4 Fire damage at the start of each turn until put out.
    if (a.burning && !a.life.dead)
    {
        const int amount = resolved_damage(a, detail::DamageType::fire, roll(4), true);
        log(a.source.name + " burns for " + std::to_string(amount) + " Fire damage.",
            {"{name} burns for {damage} Fire damage.",
             {{"name", a.source.name}, {"damage", std::to_string(amount)}}});
        damage(a, amount, false);
        if (a.life.dead)
        {
            a.burning = false;
        }
    }
    if (def(a).regeneration)
    {
        regenerate(a);
        if (a.life.hp == 0)
        {
            return false;
        }
    }
    // Regenerate: 1 Hit Point at the start of each of its turns.
    if (detail::has_effect(a.effects, detail::EffectKind::regenerating) && !a.life.dead &&
        a.life.hp < max_hp(a) && !detail::healing_blocked(a.effects))
    {
        heal(a, 1);
    }
    // Aura of Life: an ally at 0 Hit Points regains 1 as its turn starts.
    if (a.life.hp == 0 && !a.life.dead && in_aura_of_life(a) && !detail::healing_blocked(a.effects))
    {
        log(a.source.name + " stirs in the Aura of Life.",
            {"{name} stirs in the Aura of Life.", {{"name", a.source.name}}});
        heal(a, 1);
    }
    if (a.life.hp == 0)
    {
        if (!a.life.stable)
        {
            const int aura = aura_of_protection(a);
            // Beacon of Hope: Advantage on death saves.
            const int result = detail::death_save(
                a.life, rng_, !detail::healing_blocked(a.effects), def(a).lucky, aura,
                detail::has_effect(a.effects, detail::EffectKind::hopeful));
            fall(a);
            if (aura)
            {
                log(a.source.name + " death save: " + std::to_string(result) + " + " +
                        std::to_string(aura) + " (Aura of Protection)",
                    {"{name} death save: {roll} + {bonus} (Aura of Protection)",
                     {{"name", a.source.name},
                      {"roll", std::to_string(result)},
                      {"bonus", std::to_string(aura)}}});
            }
            else
            {
                log(a.source.name + " death save: " + std::to_string(result),
                    {"{name} death save: {roll}",
                     {{"name", a.source.name}, {"roll", std::to_string(result)}}});
            }
            if (a.life.hp > 0)
            {
                a.effects.prone = true;
                if (shares_occupied_space(a))
                {
                    a.involuntary_overlap = true;
                }
            }
            if (a.life.dead)
            {
                clear_departed_overlaps();
            }
        }
        if (a.life.hp == 0)
        {
            return false;
        }
    }
    // One spell slot per turn, whoever's turn it is: a Reaction spell on
    // another creature's turn may spend a slot again.
    for (auto &actor : actors_)
    {
        actor.spent_slot = false;
        actor.cleave_used = false;
        actor.colossus_used = actor.horde_used = actor.resistance_used = false;
        actor.strike_used = actor.smite_used = actor.drink_used = actor.hurl_used = false;
        std::erase_if(actor.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::defended ||
                                 e.kind == detail::EffectKind::pack_moved ||
                                 e.kind == detail::EffectKind::defending ||
                                 e.kind == detail::EffectKind::vitality;
                      });
        actor.horde_origin = 0;
        actor.prey_used = false;
        actor.prey_origin = 0;
        actor.savage_used = false;
        actor.sneak_used = false;
        actor.aim_used = actor.aim_ready = false;
        actor.moved = false;
        actor.attacked = false;
        actor.pounce = 0;
        actor.brutal_used = false;
        actor.stun_used = false;
        actor.fleet_step = false;
        actor.carried = 0;
        actor.run_feet = 0;
        actor.light_origins.clear();
        actor.light_extra = 0;
        actor.nick_origin = 0;
        actor.smite_target = 0;
        actor.smite_critical = actor.smite_melee = false;
    }
    // Historical checkpoints do not distinguish a spent Light attack from any
    // other Bonus Action. Keep their current turn exact; enable Nick at the first
    // fresh turn boundary, where the new shared allowance is unambiguous.
    if (!nick_active_ && std::any_of(actors_.begin(), actors_.end(),
                                     [&](const auto &actor)
                                     {
                                         return has_nick(actor);
                                     }))
    {
        activate_light();
        nick_active_ = true;
    }
    a.actions = {};
    a.surge_used = false;
    a.bonus = !jaunted_back;
    // Slow: no Reactions.
    a.reaction = !detail::has_effect(a.effects, detail::EffectKind::time_slowed);
    a.dodge = a.disengaged = false;
    a.movement = def(a).speed;
    a.dashes = 0;
    a.rush_used = false;
    // Magic Circle's and Animate Dead's casting takes the Action each turn of
    // its minute.
    if (detail::has_effect(a.effects, detail::EffectKind::circling))
    {
        (void)a.actions.spend(true);
        log(a.source.name + " keeps casting Magic Circle.",
            {"{name} keeps casting Magic Circle.", {{"name", a.source.name}}});
    }
    for (const auto &e : a.effects.active)
    {
        if (e.kind == detail::EffectKind::animating)
        {
            // Create Undead's slot level is 10 more.
            const std::string spell = e.dc >= 10 ? "Create Undead" : "Animate Dead";
            (void)a.actions.spend(true);
            log(a.source.name + " keeps casting " + spell + ".",
                {"{name} keeps casting {spell}.",
                 {{"name", a.source.name}, {"spell", spell, true}}});
            break;
        }
    }
    // Regenerate: its caster keeps casting through the minute.
    if (std::any_of(actors_.begin(), actors_.end(),
                    [&](const auto &other)
                    {
                        return std::any_of(other.effects.active.begin(), other.effects.active.end(),
                                           [&](const auto &e)
                                           {
                                               return e.kind == detail::EffectKind::knitting &&
                                                      e.source_actor == a.source.id &&
                                                      e.source_scope == scope_;
                                           });
                    }))
    {
        (void)a.actions.spend(true);
        log(a.source.name + " keeps casting Regenerate.",
            {"{name} keeps casting {spell}.",
             {{"name", a.source.name}, {"spell", "Regenerate", true}}});
    }
    // Haste: the second, limited action.
    a.actions.haste = detail::has_effect(a.effects, detail::EffectKind::hasted);
    move_wave(a.source.id);
    // Faithful Hound: it bites the nearest enemy within 5 feet of it.
    for (const auto &zone : std::vector<Zone>(zones_))
    {
        if (zone.kind == ZoneKind::hound && zone.caster == a.source.id)
        {
            Actor *prey = nullptr; // Borrowed from actors_.
            for (auto &other : actors_)
            {
                if (other.source.side != a.source.side && other.life.hp > 0 && !other.life.dead &&
                    !absent(other) && !sealed_off(other) &&
                    distance(zone.cells.front(), other) <= 5 && !prey)
                {
                    prey = &other;
                }
            }
            if (!prey)
            {
                continue;
            }
            const auto &spell = *detail::find_spell("faithful_hound");
            log("The hound bites at " + prey->source.name + ".",
                {"The hound bites at {name}.", {{"name", prey->source.name}}});
            if (saving_throw_succeeds(*prey, spell.save, zone.save_dc, false,
                                      heightened_against(*prey, a, spell.id)))
            {
                continue;
            }
            const int amount = resolved_damage(*prey, spell.damage, dice(spell.dice));
            log(prey->source.name + " takes " + std::to_string(amount) + " Force damage.",
                {"{name} takes {damage} Force damage.",
                 {{"name", prey->source.name}, {"damage", std::to_string(amount)}}});
            damage(*prey, amount);
        }
    }
    // Charging in panic, as confused, it has no Bonus Actions or Reactions.
    if (a.panicked && a.must_stay)
    {
        a.bonus = false;
        a.reaction = false;
    }
    // Confusion: a d10 decides the turn.
    if (detail::has_effect(a.effects, detail::EffectKind::confused) && conscious(a) &&
        !confused_turn(a))
    {
        return false;
    }
    // Sleet Storm and Stinking Cloud act on a creature starting its turn in them.
    for (const auto &zone : std::vector<Zone>(zones_))
    {
        if (zone.kind == ZoneKind::sleet && in_zone(zone, a))
        {
            sleet_falls(zone, a);
        }
        else if (zone.kind == ZoneKind::stench && in_zone(zone, a))
        {
            choke(zone, a);
        }
    }

    // Bestow Curse: a Wisdom save as its turn starts, or only the Dodge action.
    if (const auto *curse = curse_on(a, 7); curse && conscious(a))
    {
        if (const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                             [&](const auto &other)
                                             {
                                                 return other.source.id == curse->source_actor;
                                             });
            caster != actors_.end() &&
            !saving_throw_succeeds(a, detail::Ability::wisdom, spell_dc(*caster, "bestow_curse")))
        {
            (void)a.actions.spend(true);
            a.dodge = true;
            log(a.source.name + " is forced to take the Dodge action.",
                {"{name} is forced to take the Dodge action.", {{"name", a.source.name}}});
        }
    }
    // Command's Grovel and Halt take the whole turn; Approach and Flee limit
    // the turn's commands instead (obey_command).
    if (const auto *command = detail::command_effect(a.effects))
    {
        if (command->dc == static_cast<int>(detail::CommandOption::grovel))
        {
            a.effects.prone = true;
            log(a.source.name + " grovels.", {"{name} grovels.", {{"name", a.source.name}}});
            return false;
        }
        if (command->dc == static_cast<int>(detail::CommandOption::halt))
        {
            log(a.source.name + " halts.", {"{name} halts.", {{"name", a.source.name}}});
            return false;
        }
    }
    log("Round " + std::to_string(round_) + ": " + a.source.name + " acts.",
        {"Round {round}: {name} acts.",
         {{"round", std::to_string(round_)}, {"name", a.source.name}}});
    return true;
}

unsigned Session::next_turn_ms(const Actor &target) const
{
    const auto index = index_of(target.source.id);
    const unsigned start = index ? turn_end_ms(index - 1) : 0,
                   current = turn_ ? turn_end_ms(turn_ - 1) : 0;
    return start > current ? start - current : detail::round_ms - current + start;
}

unsigned Session::next_save_ms(EntityId target) const
{
    const auto end = turn_end_ms(index_of(target));
    const auto start = turn_ ? turn_end_ms(turn_ - 1) : 0;
    return end > start ? end - start : detail::round_ms - start + end;
}

void Session::log_luck(const Actor &a, bool rerolled)
{
    if (rerolled)
    {
        log(a.source.name + " rolls a 1 and rerolls it with Luck.",
            {"{name} rolls a 1 and rerolls it with Luck.", {{"name", a.source.name}}});
    }
}

void Session::log_save(const Actor &target, const detail::SaveResult &result)
{
    static constexpr std::array names{"Strength",     "Dexterity", "Constitution",
                                      "Intelligence", "Wisdom",    "Charisma"};
    static constexpr std::array messages{
        "{name} Strength save: d20 {roll} + {bonus} vs DC {dc} ({result}).",
        "{name} Dexterity save: d20 {roll} + {bonus} vs DC {dc} ({result}).",
        "{name} Constitution save: d20 {roll} + {bonus} vs DC {dc} ({result}).",
        "{name} Intelligence save: d20 {roll} + {bonus} vs DC {dc} ({result}).",
        "{name} Wisdom save: d20 {roll} + {bonus} vs DC {dc} ({result}).",
        "{name} Charisma save: d20 {roll} + {bonus} vs DC {dc} ({result})."};
    // The Aura of Protection's share of the bonus is named with the roll.
    static constexpr std::array aura_messages{
        "{name} Strength save: d20 {roll} + {bonus} (Aura of Protection +{aura}) vs DC {dc} ({result}).",
        "{name} Dexterity save: d20 {roll} + {bonus} (Aura of Protection +{aura}) vs DC {dc} ({result}).",
        "{name} Constitution save: d20 {roll} + {bonus} (Aura of Protection +{aura}) vs DC {dc} ({result}).",
        "{name} Intelligence save: d20 {roll} + {bonus} (Aura of Protection +{aura}) vs DC {dc} ({result}).",
        "{name} Wisdom save: d20 {roll} + {bonus} (Aura of Protection +{aura}) vs DC {dc} ({result}).",
        "{name} Charisma save: d20 {roll} + {bonus} (Aura of Protection +{aura}) vs DC {dc} ({result})."};
    const auto ability = static_cast<unsigned>(result.ability);
    const std::string outcome = result.success ? "success" : "failure";
    const int aura = aura_of_protection(target);
    const std::string aura_note =
        aura ? " (Aura of Protection +" + std::to_string(aura) + ")" : std::string();
    log(target.source.name + " " + names[ability] + " save: d20 " + std::to_string(result.natural) +
            " + " + std::to_string(result.bonus) + aura_note + " vs DC " +
            std::to_string(result.dc) + " (" + outcome + ").",
        {aura ? aura_messages[ability] : messages[ability],
         {{"name", target.source.name},
          {"roll", std::to_string(result.natural)},
          {"bonus", std::to_string(result.bonus)},
          {"aura", std::to_string(aura)},
          {"dc", std::to_string(result.dc)},
          {"result", outcome, true}}});
}

bool Session::saving_throw_succeeds(const Actor &target, detail::Ability ability, int dc,
                                    bool advantage, bool disadvantage)
{
    return rolled_save_succeeds(target, ability, dc, advantage, disadvantage) ||
           resists_legendarily(target);
}

bool Session::resists_legendarily(const Actor &target)
{
    const int uses = def(target).legendary_resistance;
    if (!uses || target.life.dead)
    {
        return false;
    }
    auto &resister = actor(target.source.id);
    auto spent = std::find_if(resister.effects.active.begin(), resister.effects.active.end(),
                              [](const auto &e)
                              {
                                  return e.kind == detail::EffectKind::legendary_resisted;
                              });
    if (spent == resister.effects.active.end())
    {
        if (!detail::can_apply(resister.effects))
        {
            return false;
        }
        detail::apply_spell_benefit(resister.effects, effect_source(resister), resister.source.name,
                                    detail::EffectKind::legendary_resisted, 0);
        spent = std::prev(resister.effects.active.end());
    }
    if (spent->dc >= uses)
    {
        return false;
    }
    ++spent->dc;
    const auto left = std::to_string(uses - spent->dc);
    log(resister.source.name + " uses Legendary Resistance and succeeds instead (" + left +
            " left).",
        {"{name} uses Legendary Resistance and succeeds instead ({count} left).",
         {{"name", resister.source.name}, {"count", left}}});
    return true;
}

bool Session::rolled_save_succeeds(const Actor &target, detail::Ability ability, int dc,
                                   bool advantage, bool disadvantage)
{
    if ((detail::paralyzed(target.effects) ||
         detail::has_effect(target.effects, detail::EffectKind::petrified)) &&
        !unconscious(target) &&
        (ability == detail::Ability::strength || ability == detail::Ability::dexterity))
    {
        const std::string name = ability == detail::Ability::strength ? "Strength" : "Dexterity";
        log(target.source.name + " automatically fails the " + name + " save while Paralyzed.",
            {"{name} automatically fails the {ability} save while Paralyzed.",
             {{"name", target.source.name}, {"ability", name, true}}});
        return false;
    }
    if (detail::stunned(target.effects) &&
        (ability == detail::Ability::strength || ability == detail::Ability::dexterity))
    {
        const std::string name = ability == detail::Ability::strength ? "Strength" : "Dexterity";
        log(target.source.name + " automatically fails the " + name + " save while Stunned.",
            {"{name} automatically fails the {ability} save while Stunned.",
             {{"name", target.source.name}, {"ability", name, true}}});
        return false;
    }
    if ((detail::has_effect(target.effects, detail::EffectKind::asleep) ||
         detail::has_effect(target.effects, detail::EffectKind::knocked_out)) &&
        (ability == detail::Ability::strength || ability == detail::Ability::dexterity))
    {
        const std::string name = ability == detail::Ability::strength ? "Strength" : "Dexterity";
        log(target.source.name + " automatically fails the " + name + " save while asleep.",
            {"{name} automatically fails the {ability} save while asleep.",
             {{"name", target.source.name}, {"ability", name, true}}});
        return false;
    }
    if (unconscious(target) &&
        (ability == detail::Ability::strength || ability == detail::Ability::dexterity))
    {
        const std::string name = ability == detail::Ability::strength ? "Strength" : "Dexterity";
        log(target.source.name + " automatically fails the " + name + " save while Unconscious.",
            {"{name} automatically fails the {ability} save while Unconscious.",
             {{"name", target.source.name}, {"ability", name, true}}});
        return false;
    }
    // Warding Bond: +1 to saving throws; Smite of Protection +2 to Dexterity
    // saves, Slow -2.
    const int bless =
        blessing_die(target) - return_penalty(target) +
        (detail::has_effect(target.effects, detail::EffectKind::warding_bond) ? 1 : 0) +
        aura_of_protection(target) +
        (ability == detail::Ability::dexterity && smite_protected(target) ? 2 : 0) +
        (against_fire_ && def(target).fire_ward ? 4 : 0) -
        (ability == detail::Ability::dexterity &&
                 detail::has_effect(target.effects, detail::EffectKind::time_slowed)
             ? 2
             : 0);
    auto modifiers =
        detail::saving_modifiers(ability, def(target).str_dex_disadvantage, target.dodge);
    modifiers.advantage |= advantage;
    // Heightened Spell: Disadvantage on the save against the spell.
    modifiers.disadvantage |= heightened_target_ == target.source.id || disadvantage;
    // Staggering Blow: Disadvantage on the next saving throw.
    modifiers.disadvantage |= detail::spend_staggering(actor(target.source.id).effects);
    // Bestow Curse: Disadvantage on the cursed ability's saves; Contagion too,
    // on the ability chosen.
    modifiers.disadvantage |=
        curse_on(target, static_cast<int>(ability)) != nullptr ||
        std::any_of(target.effects.active.begin(), target.effects.active.end(),
                    [&](const auto &e)
                    {
                        return e.kind == detail::EffectKind::contagion &&
                               e.dc % 10 == static_cast<int>(ability);
                    });
    // Gnomish Cunning: Advantage on Intelligence, Wisdom and Charisma saves.
    modifiers.advantage |= def(target).gnomish_cunning && detail::mental(ability);
    // Magic Resistance: Advantage on saves against a spell being cast.
    modifiers.advantage |= against_magic_ && def(target).magic_resistance;
    // Dwarven Resilience: Advantage on saves to avoid or end Poisoned.
    modifiers.advantage |= against_poisoned_ && def(target).dwarf;
    // Holy Aura: Advantage on every save.
    modifiers.advantage |= holy_aura(target) != nullptr;
    // Beacon of Hope: Advantage on Wisdom saves.
    modifiers.advantage |= ability == detail::Ability::wisdom &&
                           detail::has_effect(target.effects, detail::EffectKind::hopeful);
    // Haste: Advantage on Dexterity saves.
    modifiers.advantage |= ability == detail::Ability::dexterity &&
                           detail::has_effect(target.effects, detail::EffectKind::hasted);
    // Irresistible Dance: Disadvantage on Dexterity saves.
    modifiers.disadvantage |= ability == detail::Ability::dexterity &&
                              detail::has_effect(target.effects, detail::EffectKind::dancing);
    // Danger Sense: Advantage on Dexterity saves unless Incapacitated.
    modifiers.advantage |= ability == detail::Ability::dexterity && def(target).danger_sense &&
                           !detail::incapacitated(target.effects);
    // Restrained: Disadvantage on Dexterity saves.
    modifiers.disadvantage |=
        ability == detail::Ability::dexterity && detail::restrained(target.effects);
    // Ray of Enfeeblement and Reduce: Disadvantage on Strength saves; Enlarge:
    // Advantage.
    if (ability == detail::Ability::strength)
    {
        // Conjure Animals: Advantage within 5 feet of the caster's own pack.
        modifiers.advantage |=
            std::any_of(zones_.begin(), zones_.end(),
                        [&](const auto &zone)
                        {
                            return zone.kind == ZoneKind::animals &&
                                   zone.caster == target.source.id &&
                                   std::any_of(zone.cells.begin(), zone.cells.end(),
                                               [&](Cell cell)
                                               {
                                                   return distance(cell, target) <= 5;
                                               });
                        });
        modifiers.disadvantage |=
            detail::has_effect(target.effects, detail::EffectKind::enfeebled) ||
            detail::has_effect(target.effects, detail::EffectKind::reduced);
        modifiers.advantage |=
            detail::has_effect(target.effects, detail::EffectKind::enlarged) || rage_of(target);
    }
    modifiers.lucky = def(target).lucky;
    bool lucky_reroll = false;
    auto result =
        detail::saving_throw(ability, def(target).saves[static_cast<unsigned>(ability)] + bless, dc,
                             modifiers, rng_, &lucky_reroll);
    log_luck(target, lucky_reroll);
    log_save(target, result);
    // Heroic Inspiration rolls a failed save again, if a 20 could pass it.
    if (!result.success && result.bonus + 20 >= dc)
    {
        if (const auto again = heroic_reroll(actor(target.source.id)))
        {
            result.natural = *again;
            result.success = result.natural + result.bonus >= dc;
            log_save(target, result);
        }
    }
    // Indomitable: the Fighter rerolls a failed save it could still pass,
    // adding its Fighter level, without asking, as Heroic Inspiration does.
    if (auto &fighter = actor(target.source.id);
        !result.success && def(fighter).indomitable &&
        fighter.arcane[CharacterClass::fighter] > 0 && conscious(fighter) &&
        result.bonus + def(fighter).class_levels[CharacterClass::fighter] + 20 >= dc)
    {
        --fighter.arcane[CharacterClass::fighter];
        result.natural = roll(20);
        result.bonus += def(fighter).class_levels[CharacterClass::fighter];
        result.success = result.natural + result.bonus >= dc;
        log(fighter.source.name + " uses Indomitable and rolls the save again.",
            {"{name} uses Indomitable and rolls the save again.", {{"name", fighter.source.name}}});
        log_save(target, result);
    }
    // Disciplined Survivor: the Monk spends a Focus Point to reroll a failed
    // save a 20 could pass, without asking (LEVELS-2).
    if (auto &monk = actor(target.source.id); !result.success && def(monk).disciplined_survivor &&
                                              monk.surges[CharacterClass::monk] > 0 &&
                                              conscious(monk) && result.bonus + 20 >= dc)
    {
        --monk.surges[CharacterClass::monk];
        result.natural = roll(20);
        result.success = result.natural + result.bonus >= dc;
        log(monk.source.name +
                " spends 1 Focus Point and rolls the save again (Disciplined Survivor).",
            {"{name} spends 1 Focus Point and rolls the save again (Disciplined Survivor).",
             {{"name", monk.source.name}}});
        log_save(target, result);
    }
    if (!result.success)
    {
        dark_ones_luck(actor(target.source.id), result);
    }
    // Bardic Inspiration may turn the failure into a success.
    if (!result.success)
    {
        if (const int die = inspiration_roll(target, dc - result.natural - result.bonus))
        {
            result.success = result.natural + result.bonus + die >= dc;
        }
    }
    return result.success;
}

const Actor *Session::find_actor(EntityId id) const
{
    const auto found = std::find_if(actors_.begin(), actors_.end(),
                                    [&](const auto &a)
                                    {
                                        return a.source.id == id;
                                    });
    return found == actors_.end() ? nullptr : &*found;
}

bool Session::charm_or_fear_save(const Actor &target, detail::Ability ability, int dc,
                                 bool advantage, bool disadvantage)
{
    if (saving_throw_succeeds(target, ability, dc, advantage, disadvantage))
    {
        return true;
    }
    // A Bard within 30 feet, the creature itself included, spends its
    // Reaction without asking, as roll effects are used.
    for (auto &bard : actors_)
    {
        if (def(bard).countercharm && bard.source.side == target.source.side && conscious(bard) &&
            bard.reaction && !detail::incapacitated(bard.effects) && !absent(bard) &&
            distance(bard, target) <= 30)
        {
            bard.reaction = false;
            log(bard.source.name + " uses Countercharm: " + target.source.name +
                    " rolls the save again with Advantage.",
                {"{name} uses Countercharm: {target} rolls the save again with Advantage.",
                 {{"name", bard.source.name}, {"target", target.source.name}}});
            return saving_throw_succeeds(target, ability, dc, true, disadvantage);
        }
    }
    return false;
}

int Session::peerless_skill(Actor &bard, int shortfall)
{
    if (!def(bard).peerless_skill || bard.arcane[CharacterClass::bard] < 1 || shortfall < 1 ||
        shortfall > def(bard).bardic_die || !conscious(bard))
    {
        return 0;
    }
    const int die = roll(def(bard).bardic_die);
    const bool fixed = die >= shortfall;
    if (fixed)
    {
        --bard.arcane[CharacterClass::bard];
    }
    const auto amount = std::to_string(die);
    log(bard.source.name + " adds " + amount + " with Peerless Skill" +
            (fixed ? ", turning the roll into a success." : ", not enough; the use is kept."),
        {fixed ? "{name} adds {amount} with Peerless Skill, turning the roll into a success."
               : "{name} adds {amount} with Peerless Skill, not enough; the use is kept.",
         {{"name", bard.source.name}, {"amount", amount}}});
    return fixed ? die : 0;
}

bool Session::spend_dark_luck(Actor &warlock)
{
    const auto used = std::find_if(warlock.effects.active.begin(), warlock.effects.active.end(),
                                   [](const auto &e)
                                   {
                                       return e.kind == detail::EffectKind::luck_used;
                                   });
    if (used == warlock.effects.active.end() && !detail::can_apply(warlock.effects))
    {
        return false;
    }
    if (used == warlock.effects.active.end())
    {
        detail::apply_spell_benefit(warlock.effects, effect_source(warlock), warlock.source.name,
                                    detail::EffectKind::luck_used, 1);
        return true;
    }
    if (used->dc >= def(warlock).dark_luck)
    {
        return false;
    }
    ++used->dc;
    return true;
}

void Session::dark_ones_luck(Actor &warlock, detail::SaveResult &result)
{
    // Dark One's Own Luck (Fiend 6): 1d10 added to a failed save it could
    // still pass, without asking, once a roll.
    if (!def(warlock).dark_luck || !conscious(warlock) ||
        result.natural + result.bonus + 10 < result.dc || !spend_dark_luck(warlock))
    {
        return;
    }
    const int die = roll(10);
    result.bonus += die;
    result.success = result.natural + result.bonus >= result.dc;
    log(warlock.source.name + " adds " + std::to_string(die) +
            " to the save (Dark One's Own Luck).",
        {"{name} adds {die} to the save (Dark One's Own Luck).",
         {{"name", warlock.source.name}, {"die", std::to_string(die)}}});
    log_save(warlock, result);
}

int Session::dark_luck_check(Actor &warlock, int shortfall)
{
    // As for saves: a failed check it could still pass, without asking.
    if (!def(warlock).dark_luck || !conscious(warlock) || shortfall < 1 || shortfall > 10 ||
        !spend_dark_luck(warlock))
    {
        return 0;
    }
    const int die = roll(10);
    log(warlock.source.name + " adds " + std::to_string(die) +
            " to the check (Dark One's Own Luck).",
        {"{name} adds {die} to the check (Dark One's Own Luck).",
         {{"name", warlock.source.name}, {"die", std::to_string(die)}}});
    return die;
}

void Session::advance_turn_time()
{
    // Partition one six-second round across its fixed initiative slots. Integer
    // boundaries telescope to exactly 6000 ms, even with 7 or 64 participants.
    // Dead/unconscious slots still pass time; menus and repeated snapshots don't.
    const unsigned delta = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
    for (auto &a : actors_)
    {
        detail::start_stable_recovery(a.life, rng_);
    }
    std::vector<detail::RecoverySubject> subjects;
    std::vector<EntityId> unconscious;
    // Acid Arrow burns once its effect runs out, after the effects are walked.
    // Acid Arrow's targets, with the slot level each arrow was cast at.
    std::vector<std::pair<EntityId, unsigned>> burned;
    // Banishment that has lasted its full minute.
    std::vector<EntityId> unbanished;
    // Magic Circles whose minute of casting is over, with their slot levels.
    std::vector<std::pair<EntityId, unsigned>> circled;
    // Animate Dead or Create Undead (true) cast for its full minute.
    std::vector<std::pair<EntityId, bool>> animated;
    // Regenerate cast for its full minute: the creature and its caster.
    std::vector<std::pair<EntityId, EntityId>> knitted;
    // Haste that ran its minute, blinkers back from the Ethereal Plane, and
    // Vitriolic Sphere's later acid.
    std::vector<EntityId> hastened, returned, corroded, hurled;
    // Flesh to Stone held for its full minute: the stone lasts.
    std::vector<detail::Effect> stoned;
    std::vector<EntityId> stoned_targets;
    for (auto &a : actors_)
    {
        auto saves = def(a).saves;
        for (auto &save : saves)
        {
            save += aura_of_protection(a);
        }
        subjects.push_back(
            {{a.source.id, a.effects, saves, a.life.dead, def(a).str_dex_disadvantage, a.dodge,
              def(a).gnomish_cunning, def(a).lucky, def(a).brave, def(a).dwarf},
             a.life});
        if (a.life.hp == 0)
        {
            unconscious.push_back(a.source.id);
        }
    }
    detail::elapse_recovery(
        subjects, delta, rng_, detail::RecoveryMode::combat,
        [&](const detail::EffectEvent &event)
        {
            const auto &target = actor(event.target);
            if (event.save)
            {
                log_save(target, *event.save);
            }
            if (event.inspiration)
            {
                log_inspiration(target, event.inspiration, event.save->success);
            }
            if (event.removed && event.effect.kind == detail::EffectKind::banished)
            {
                unbanished.push_back(event.target);
            }
            if (event.removed && event.effect.kind == detail::EffectKind::circling)
            {
                circled.push_back({event.target, static_cast<unsigned>(event.effect.dc)});
            }
            if (event.removed && event.effect.kind == detail::EffectKind::animating)
            {
                animated.push_back({event.target, event.effect.dc >= 10});
            }
            if (event.removed && event.effect.kind == detail::EffectKind::knitting)
            {
                knitted.push_back({event.target, event.effect.source_actor});
            }
            if (event.removed && event.effect.kind == detail::EffectKind::hasted)
            {
                hastened.push_back(event.target);
            }
            if (event.removed && event.effect.kind == detail::EffectKind::ethereal)
            {
                returned.push_back(event.target);
            }
            if (event.removed && event.effect.kind == detail::EffectKind::hurled)
            {
                hurled.push_back(event.target);
            }
            if (event.removed && event.effect.kind == detail::EffectKind::vitriol)
            {
                corroded.push_back(event.target);
            }
            if (event.removed && event.effect.kind == detail::EffectKind::petrified &&
                !event.effect.dc)
            {
                stoned.push_back(event.effect);
            }
            if (event.removed && event.effect.kind == detail::EffectKind::petrified &&
                !event.effect.dc)
            {
                stoned_targets.push_back(event.target);
            }
            if (event.removed && (event.effect.kind == detail::EffectKind::sap ||
                                  event.effect.kind == detail::EffectKind::vex ||
                                  event.effect.kind == detail::EffectKind::slow))
            {
                log(target.source.name + " loses a mastery effect.",
                    {"{name} loses {mastery} from {source}.",
                     {{"name", target.source.name},
                      {"mastery",
                       event.effect.kind == detail::EffectKind::sap    ? "Sap"
                       : event.effect.kind == detail::EffectKind::slow ? "Slow"
                                                                       : "Vex",
                       true},
                      {"source", event.effect.source_name}}});
            }
            if (event.removed && event.effect.kind == detail::EffectKind::chill_touch)
            {
                log(target.source.name + " loses a Chill Touch effect.",
                    {"{name} loses a Chill Touch effect.", {{"name", target.source.name}}});
            }
            if (event.removed && event.effect.kind == detail::EffectKind::shocking_grasp)
            {
                log(target.source.name + " loses a Shocking Grasp effect.",
                    {"{name} loses a Shocking Grasp effect.", {{"name", target.source.name}}});
            }
            if (event.removed && event.effect.kind == detail::EffectKind::ray_of_frost)
            {
                log(target.source.name + " loses a Ray of Frost effect.",
                    {"{name} loses a Ray of Frost effect.", {{"name", target.source.name}}});
            }
            if (event.removed && event.effect.kind == detail::EffectKind::blindness)
            {
                log(target.source.name + " recovers from a blindness effect.",
                    {"{name} recovers from a blindness effect.", {{"name", target.source.name}}});
            }
            if (event.removed && event.effect.kind == detail::EffectKind::hold_person)
            {
                log(target.source.name + " is no longer Paralyzed.",
                    {"{name} is no longer Paralyzed.", {{"name", target.source.name}}});
            }
            if (event.removed && event.effect.kind == detail::EffectKind::time_slowed)
            {
                log(target.source.name + " is no longer slowed.",
                    {"{name} is no longer slowed.", {{"name", target.source.name}}});
            }
            if (event.removed && event.effect.kind == detail::EffectKind::acid_arrow)
            {
                burned.push_back({event.target, event.effect.slot});
            }
        });
    // A Magic Circle cast for its full minute stands for an hour, and an hour
    // more for each slot level above 3.
    for (const auto &[id, level] : circled)
    {
        for (auto &zone : zones_)
        {
            if (zone.kind == ZoneKind::circle && zone.caster == id && !zone.ends_ms)
            {
                zone.ends_ms = elapsed_ms_ + 3600000ull * (level - 2);
                log("The Magic Circle is complete.", {"The Magic Circle is complete.", {}});
            }
        }
    }
    for (const auto &[id, ghouls] : animated)
    {
        finish_animating(actor(id), ghouls ? "ghoul" : "zombie");
    }
    for (const auto &[id, caster] : knitted)
    {
        finish_knitting(actor(id), caster);
        actor(caster).concentration.stop();
    }
    for (const auto id : hastened)
    {
        tire(actor(id));
    }
    for (std::size_t n = 0; n < stoned.size(); ++n)
    {
        auto &target = actor(stoned_targets[n]);
        auto statue = stoned[n];
        statue.dc = 1;
        statue.remaining_ms = 4000000000u;
        statue.id = target.effects.next_id++;
        target.effects.active.push_back(std::move(statue));
        log(target.source.name + " stays stone.",
            {"{name} stays stone.", {{"name", target.source.name}}});
    }
    for (const auto id : corroded)
    {
        auto &target = actor(id);
        if (target.life.dead)
        {
            continue;
        }
        const int amount = resolved_damage(target, detail::DamageType::acid, dice({5, 4, 0}));
        log(target.source.name + " takes " + std::to_string(amount) +
                " Acid damage from the vitriol.",
            {"{name} takes {damage} Acid damage from the vitriol.",
             {{"name", target.source.name}, {"damage", std::to_string(amount)}}});
        damage(target, amount, false);
    }
    for (const auto id : returned)
    {
        auto &blinker = actor(id);
        blinker.source.cell = nearest_open(blinker);
        log(blinker.source.name + " returns from the Ethereal Plane.",
            {"{name} returns from the Ethereal Plane.", {{"name", blinker.source.name}}});
    }
    // Hurl Through Hell's creature returns to its space or the nearest free one.
    for (const auto id : hurled)
    {
        auto &target = actor(id);
        target.source.cell = nearest_open(target);
        log(target.source.name + " returns from the Lower Planes.",
            {"{name} returns from the Lower Planes.", {{"name", target.source.name}}});
    }
    // Suppressed effects keep time too.
    for (auto &a : actors_)
    {
        for (auto &e : a.suppressed)
        {
            e.remaining_ms -= std::min(e.remaining_ms, delta);
        }
        std::erase_if(a.suppressed,
                      [](const auto &e)
                      {
                          return !e.remaining_ms;
                      });
    }
    // After a full minute an Aberration, a Celestial, an Elemental, a Fey or a
    // Fiend never comes back: it counts as defeated (LEVELS-1); any other
    // creature returns.
    for (const auto id : unbanished)
    {
        auto &target = actor(id);
        const auto &type = def(target).creature_type;
        if (type == "aberration" || type == "celestial" || type == "elemental" || type == "fey" ||
            type == "fiend")
        {
            target.life.hp = 0;
            target.life.dead = true;
            target.life.recovery = {};
            log(target.source.name + " does not return from its banishment.",
                {"{name} does not return from its banishment.", {{"name", target.source.name}}});
        }
        else
        {
            return_from_banishment(target);
        }
    }
    for (const auto &[id, slot] : burned)
    {
        auto &target = actor(id);
        if (target.life.dead)
        {
            continue;
        }
        // 2d4, and 1d4 more for each slot level above 2.
        const int count = static_cast<int>(std::max(2u, slot));
        const int amount = resolved_damage(target, detail::DamageType::acid, dice({count, 4, 0}));
        log(target.source.name + " takes " + std::to_string(amount) +
                " Acid damage from Acid Arrow.",
            {"{name} takes {damage} Acid damage from Acid Arrow.",
             {{"name", target.source.name}, {"damage", std::to_string(amount)}}});
        damage(target, amount, false);
    }
    for (auto &a : actors_)
    {
        if (a.concentration.elapse(delta))
        {
            drop_concentration_effects(a);
        }
    }
    // Rage ends early when the Barbarian is Incapacitated or falls.
    for (auto &a : actors_)
    {
        if (rage_of(a) && (detail::incapacitated(a.effects) || a.life.hp == 0))
        {
            std::erase_if(a.effects.active,
                          [](const auto &e)
                          {
                              return e.kind == detail::EffectKind::raging;
                          });
        }
    }
    // An ended Aid lowers the maximum, and Hit Points above it with it.
    for (auto &a : actors_)
    {
        a.life.hp = std::min(a.life.hp, max_hp(a));
    }
    for (auto &a : actors_)
    {
        if (a.life.hp > 0 &&
            std::find(unconscious.begin(), unconscious.end(), a.source.id) != unconscious.end())
        {
            a.effects.prone = true;
            if (shares_occupied_space(a))
            {
                a.involuntary_overlap = true;
            }
            log(a.source.name + " recovers 1 HP naturally.",
                {"{name} recovers 1 HP naturally.", {{"name", a.source.name}}});
        }
    }
    elapsed_ms_ +=
        std::min<std::uint64_t>(delta, std::numeric_limits<std::uint64_t>::max() - elapsed_ms_);
    std::erase_if(zones_,
                  [&](const auto &zone)
                  {
                      return zone.ends_ms && zone.ends_ms <= elapsed_ms_;
                  });
}

void Session::restore_self(Actor &a)
{
    // Self-Restoration (Monk 10): one of Charmed, Frightened or Poisoned ends.
    if (!def(a).self_restoration || !conscious(a))
    {
        return;
    }
    for (const auto &[kind, name] : {std::pair{detail::EffectKind::charmed, "Charmed"},
                                     std::pair{detail::EffectKind::frightened, "Frightened"},
                                     std::pair{detail::EffectKind::abjured, "Frightened"},
                                     std::pair{detail::EffectKind::terrified, "Frightened"},
                                     std::pair{detail::EffectKind::entranced, "Charmed"},
                                     std::pair{detail::EffectKind::compelled, "Charmed"},
                                     std::pair{detail::EffectKind::poisoned, "Poisoned"}})
    {
        const auto found = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                        [&](const auto &e)
                                        {
                                            return e.kind == kind;
                                        });
        if (found == a.effects.active.end())
        {
            continue;
        }
        a.effects.active.erase(found);
        log(a.source.name + " is no longer " + name + " (Self-Restoration).",
            {"{name} is no longer {condition} (Self-Restoration).",
             {{"name", a.source.name}, {"condition", name, true}}});
        return;
    }
}

void Session::end_turn()
{
    actors_[turn_].actions.surge = false;
    // A compelled creature that could not move the named way saves now.
    finish_compulsion(actors_[turn_]);
    // A readied Metamagic option or Overchannel lapses with the turn.
    std::erase_if(actors_[turn_].effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::metamagic ||
                             e.kind == detail::EffectKind::overchannel ||
                             (e.kind == detail::EffectKind::resurged && !e.dc);
                  });
    // Delayed Blast Fireball's bead grows by 1d6 as its caster's turn ends;
    // Arcane Sword's sword may strike on the caster's later turns.
    // Tsunami sinks away as its caster's turn ends after its last move.
    if (std::any_of(zones_.begin(), zones_.end(),
                    [&](const auto &zone)
                    {
                        return zone.kind == ZoneKind::wave &&
                               zone.caster == actors_[turn_].source.id && !zone.slot;
                    }))
    {
        log("The tsunami subsides.", {"The tsunami subsides.", {}});
        end_concentration(actors_[turn_]);
    }
    for (auto &zone : zones_)
    {
        if (zone.kind == ZoneKind::bead && zone.caster == actors_[turn_].source.id)
        {
            zone.dealt = std::min(zone.dealt + 1, 59);
        }
        else if ((zone.kind == ZoneKind::sword || zone.kind == ZoneKind::fey) &&
                 zone.caster == actors_[turn_].source.id)
        {
            zone.dealt = 0;
        }
    }
    restore_self(actors_[turn_]);
    contagion_save(actors_[turn_]);
    spring_zones(actors_[turn_], ZoneKind::grease);
    burn_beside_spheres(actors_[turn_]);
    for (const auto &zone : std::vector<Zone>(zones_))
    {
        if (zone.kind == ZoneKind::moonbeam && in_zone(zone, actors_[turn_]))
        {
            moonbeam_burns(zone, actors_[turn_]);
        }
    }
    for (const auto &zone : std::vector<Zone>(zones_))
    {
        if (zone.kind == ZoneKind::gust && in_zone(zone, actors_[turn_]))
        {
            blow(zone, actors_[turn_]);
        }
    }
    for (const auto &zone : std::vector<Zone>(zones_))
    {
        if ((zone.kind == ZoneKind::swarm || zone.kind == ZoneKind::blades ||
             zone.kind == ZoneKind::thorns) &&
            in_zone(zone, actors_[turn_]))
        {
            swarm_strikes(zone, actors_[turn_]);
        }
        else if ((zone.kind == ZoneKind::animals && near_pack(zone, actors_[turn_])) ||
                 (emanates(zone) && zone.caster != actors_[turn_].source.id &&
                  in_emanation(zone, actors_[turn_])))
        {
            spirits_strike(zone, actors_[turn_]);
        }
        else if (zone.kind == ZoneKind::pillar && in_zone(zone, actors_[turn_]))
        {
            bathe(zone, actors_[turn_]);
        }
    }
    resist_petrifying(actors_[turn_]);
    // Flesh to Stone: a Constitution save at the end of each of its turns;
    // three successes end it, three failures turn the creature to stone.
    for (auto &e : actors_[turn_].effects.active)
    {
        if (e.kind == detail::EffectKind::stoning && e.source_scope == scope_ &&
            !actors_[turn_].life.dead)
        {
            const auto source = e.source_actor;
            const bool saved = saving_throw_succeeds(actors_[turn_], detail::Ability::constitution,
                                                     spell_dc(actor(source), "flesh_to_stone"));
            e.dc += saved ? 1 : 10;
            if (e.dc % 10 >= 3)
            {
                end_concentration(actor(source));
                log(actors_[turn_].source.name + " shakes off the stone.",
                    {"{name} shakes off the stone.", {{"name", actors_[turn_].source.name}}});
            }
            else if (e.dc / 10 >= 3)
            {
                e.kind = detail::EffectKind::petrified;
                e.dc = 0;
                log(actors_[turn_].source.name + " turns to stone.",
                    {"{name} turns to stone.", {{"name", actors_[turn_].source.name}}});
            }
            break;
        }
    }
    // Prismatic Spray's indigo ray: the same three-and-three saves, against
    // the spray's DC.
    if (const auto indigo =
            std::find_if(actors_[turn_].effects.active.begin(), actors_[turn_].effects.active.end(),
                         [](const auto &e)
                         {
                             return e.kind == detail::EffectKind::indigo;
                         });
        indigo != actors_[turn_].effects.active.end() && !actors_[turn_].life.dead)
    {
        const bool saved =
            saving_throw_succeeds(actors_[turn_], detail::Ability::constitution, indigo->dc / 100);
        indigo->dc += saved ? 1 : 10;
        if (indigo->dc % 10 >= 3)
        {
            actors_[turn_].effects.active.erase(indigo);
            log(actors_[turn_].source.name + " shakes off the stone.",
                {"{name} shakes off the stone.", {{"name", actors_[turn_].source.name}}});
        }
        else if (indigo->dc % 100 / 10 >= 3)
        {
            // Stone for good, until Greater Restoration.
            indigo->kind = detail::EffectKind::petrified;
            indigo->dc = 1;
            indigo->remaining_ms = detail::benefit_duration_ms(detail::EffectKind::petrified);
            log(actors_[turn_].source.name + " turns to stone.",
                {"{name} turns to stone.", {{"name", actors_[turn_].source.name}}});
        }
    }
    // Power Word Stun: a Constitution save at the end of each of its turns.
    if (const auto stun =
            std::find_if(actors_[turn_].effects.active.begin(), actors_[turn_].effects.active.end(),
                         [](const auto &e)
                         {
                             return e.kind == detail::EffectKind::word_stunned;
                         });
        stun != actors_[turn_].effects.active.end() && !actors_[turn_].life.dead &&
        saving_throw_succeeds(actors_[turn_], detail::Ability::constitution, stun->dc))
    {
        actors_[turn_].effects.active.erase(stun);
        log(actors_[turn_].source.name + " is no longer Stunned.",
            {"{name} is no longer Stunned.", {{"name", actors_[turn_].source.name}}});
    }
    // Eyebite: panic ends at least 60 feet from the caster, out of its sight.
    std::erase_if(actors_[turn_].effects.active,
                  [&](const auto &e)
                  {
                      if (e.kind != detail::EffectKind::eye_panic || e.source_scope != scope_)
                      {
                          return false;
                      }
                      const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                                       [&](const auto &other)
                                                       {
                                                           return other.source.id == e.source_actor;
                                                       });
                      return caster == actors_.end() || (distance(*caster, actors_[turn_]) >= 60 &&
                                                         !can_see(actors_[turn_], *caster));
                  });
    // Phantasmal Killer: a Wisdom save at the end of each of its turns; a
    // failure deals the Psychic damage again, a success ends the spell.
    for (const auto &e : std::vector<detail::Effect>(actors_[turn_].effects.active))
    {
        if (e.kind == detail::EffectKind::haunted && e.source_scope == scope_ &&
            !actors_[turn_].life.dead)
        {
            auto &caster = actor(e.source_actor);
            if (saving_throw_succeeds(actors_[turn_], detail::Ability::wisdom,
                                      spell_dc(caster, "phantasmal_killer")))
            {
                end_concentration(caster);
                continue;
            }
            const int amount =
                resolved_damage(actors_[turn_], detail::DamageType::psychic, dice({e.dc, 10, 0}));
            log(actors_[turn_].source.name + " takes " + std::to_string(amount) +
                    " Psychic damage from its nightmares.",
                {"{name} takes {damage} Psychic damage from its nightmares.",
                 {{"name", actors_[turn_].source.name}, {"damage", std::to_string(amount)}}});
            damage(actors_[turn_], amount);
        }
    }
    // Black Tentacles, Cloudkill and Wall of Fire act on a creature ending its
    // turn in them.
    for (const auto &zone : std::vector<Zone>(zones_))
    {
        if (zone.kind == ZoneKind::tentacles && in_zone(zone, actors_[turn_]))
        {
            grope(zone, actors_[turn_]);
        }
        else if ((zone.kind == ZoneKind::cloud || zone.kind == ZoneKind::embers) &&
                 in_zone(zone, actors_[turn_]))
        {
            choke_cloud(zone, actors_[turn_]);
        }
        else if ((zone.kind == ZoneKind::fire_wall || zone.kind == ZoneKind::fire_side) &&
                 in_zone(zone, actors_[turn_]))
        {
            wall_burns(zone, actors_[turn_]);
        }
    }
    // Fear: out of the caster's sight at the end of its turn, the creature
    // saves to end it.
    for (const auto &e : std::vector<detail::Effect>(actors_[turn_].effects.active))
    {
        if (e.kind == detail::EffectKind::terrified && e.source_scope == scope_ &&
            std::none_of(actors_.begin(), actors_.end(),
                         [&](const auto &other)
                         {
                             return other.source.id == e.source_actor && !other.life.dead &&
                                    line_of_sight(actors_[turn_], other);
                         }) &&
            saving_throw_succeeds(actors_[turn_], detail::Ability::wisdom,
                                  spell_dc(actor(e.source_actor), "fear")))
        {
            std::erase_if(actors_[turn_].effects.active,
                          [&](const auto &other)
                          {
                              return other.id == e.id;
                          });
            log(actors_[turn_].source.name + " is no longer Frightened.",
                {"{name} is no longer Frightened.", {{"name", actors_[turn_].source.name}}});
        }
    }
    // Blink: a d6 at the end of each of its caster's turns; on 4-6 it slips
    // into the Ethereal Plane until its next turn.
    if (auto &blinker = actors_[turn_];
        detail::has_effect(blinker.effects, detail::EffectKind::blinking) && conscious(blinker) &&
        detail::can_apply(blinker.effects))
    {
        const int rolled = roll(6);
        if (rolled >= 4)
        {
            detail::apply_poisoned(blinker.effects, effect_source(blinker), blinker.source.name,
                                   next_turn_ms(blinker), detail::EffectKind::ethereal);
            log(blinker.source.name + " blinks into the Ethereal Plane (d6 " +
                    std::to_string(rolled) + ").",
                {"{name} blinks into the Ethereal Plane (d6 {roll}).",
                 {{"name", blinker.source.name}, {"roll", std::to_string(rolled)}}});
        }
        else
        {
            log(blinker.source.name + " stays (Blink d6 " + std::to_string(rolled) + ").",
                {"{name} stays (Blink d6 {roll}).",
                 {{"name", blinker.source.name}, {"roll", std::to_string(rolled)}}});
        }
    }
    // Earthquake: the tremor again at the end of each of its caster's turns.
    for (const auto &zone : std::vector<Zone>(zones_))
    {
        if (zone.kind == ZoneKind::quake && zone.caster == actors_[turn_].source.id)
        {
            tremor(zone);
        }
    }
    if (performing_)
    {
        take_legendary_actions();
    }
    if (outcome_ != Outcome::ongoing)
    {
        return;
    }
    for (std::size_t checked = 0; checked <= actors_.size() * 2; ++checked)
    {
        advance_turn_time();
        turn_ = (turn_ + 1) % actors_.size();
        // The round is a display counter. Saturation avoids wrapping it to
        // zero while keeping an extremely long (or edited) combat playable.
        if (turn_ == 0 && round_ < std::numeric_limits<unsigned>::max())
        {
            ++round_;
        }
        if (begin_turn())
        {
            return;
        }
    }
    update_outcome();
}

void Session::progress_movement()
{
    auto &a = mover();
    const auto grid = movement_grid(a);
    while (path_index_ < path_.size() && a.life.hp > 0)
    {
        const auto destination = path_[path_index_];
        if (reactors_.empty() && !a.disengaged)
        {
            for (const auto &other : actors_)
            {
                if (provokes(other, a, a.source.cell, destination))
                {
                    reactors_.push_back(other.source.id);
                }
            }
        }
        if (pending())
        {
            return;
        }
        if (off_field(a, destination))
        {
            a.movement -= 5;
            ++path_index_;
            reactors_.clear();
            reactor_index_ = 0;
            run_off_field(a);
            break;
        }
        const auto cost = grid.step_cost(a.source.cell, destination);
        if (!cost || *cost > mover_budget(a))
        {
            throw std::logic_error("Invalid accepted movement path");
        }
        const auto departed = a.source.cell;
        a.source.cell = destination;
        follow_carrier(a, departed);
        // A flight spends the Reaction, not the creature's own movement.
        if (flight_)
        {
            flight_->remaining -= *cost;
        }
        else
        {
            a.movement -= *cost;
            a.pounce = std::max(0, a.pounce - *cost);
            a.moved = true;
            extend_run(a, departed, destination);
        }
        clear_departed_overlaps();
        ++path_index_;
        reactors_.clear();
        reactor_index_ = 0;
        // Antilife Shell ends when its caster forces a creature through it.
        if (a.concentration.active() &&
            std::any_of(zones_.begin(), zones_.end(),
                        [&](const auto &zone)
                        {
                            return zone.kind == ZoneKind::antilife && zone.caster == a.source.id;
                        }) &&
            std::any_of(actors_.begin(), actors_.end(),
                        [&](const auto &other)
                        {
                            return other.source.id != a.source.id && !other.life.dead &&
                                   !absent(other) && def(other).creature_type != "construct" &&
                                   def(other).creature_type != "undead" &&
                                   (distance(departed, other) <= 10) != (distance(a, other) <= 10);
                        }))
        {
            end_concentration(a);
        }
        // Entering Grease or Web calls for a save; a creature caught stops.
        // Each square moved into or within Spike Growth pierces; entering a
        // Moonbeam burns.
        const auto zones = zones_;
        bool caught = false;
        for (const auto &zone : zones)
        {
            if ((zone.kind == ZoneKind::grease || zone.kind == ZoneKind::web) &&
                in_zone(zone, destination) && !in_zone(zone, departed))
            {
                caught = spring_zone(zone, a) || caught;
            }
            if (zone.kind == ZoneKind::spikes && in_zone(zone, destination))
            {
                spikes_pierce(a);
            }
            if (zone.kind == ZoneKind::moonbeam && in_zone(zone, destination) &&
                !in_zone(zone, departed))
            {
                moonbeam_burns(zone, a);
            }
            if (zone.kind == ZoneKind::animals && near_pack(zone, destination) &&
                !near_pack(zone, departed))
            {
                spirits_strike(zone, a);
            }
            if ((zone.kind == ZoneKind::swarm || zone.kind == ZoneKind::blades) &&
                in_zone(zone, destination) && !in_zone(zone, departed))
            {
                swarm_strikes(zone, a);
            }
            if (zone.kind == ZoneKind::pillar && in_zone(zone, destination) &&
                !in_zone(zone, departed))
            {
                bathe(zone, a);
            }
            if (zone.kind == ZoneKind::sleet && in_zone(zone, destination) &&
                !in_zone(zone, departed))
            {
                sleet_falls(zone, a);
            }
            if (zone.kind == ZoneKind::tentacles && in_zone(zone, destination) &&
                !in_zone(zone, departed))
            {
                grope(zone, a);
            }
            if ((zone.kind == ZoneKind::cloud || zone.kind == ZoneKind::embers) &&
                in_zone(zone, destination) && !in_zone(zone, departed))
            {
                choke_cloud(zone, a);
            }
            if (zone.kind == ZoneKind::elemental && in_zone(zone, destination) &&
                !in_zone(zone, departed))
            {
                engulf(zone, a);
            }
            // Wall of Fire burns a creature entering the wall, once a turn.
            if (zone.kind == ZoneKind::fire_wall && in_zone(zone, destination) &&
                !in_zone(zone, departed) &&
                !detail::has_effect(a.effects, detail::EffectKind::wall_burned) &&
                detail::can_apply(a.effects))
            {
                const unsigned rest_of_turn =
                    turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
                detail::apply_poisoned(a.effects, effect_source(a), a.source.name, rest_of_turn,
                                       detail::EffectKind::wall_burned);
                wall_burns(zone, a);
            }
            // The Emanation reaches a creature coming into it, and the
            // creatures its caster carries it onto.
            if (emanates(zone) && zone.caster != a.source.id && in_emanation(zone, destination) &&
                !in_emanation(zone, departed))
            {
                spirits_strike(zone, a);
            }
            if (emanates(zone) && zone.caster == a.source.id)
            {
                for (auto &other : actors_)
                {
                    if (const int reach = zone.kind == ZoneKind::guardians ? 15 : 10;
                        other.source.id != a.source.id && distance(destination, other) <= reach &&
                        distance(departed, other) > reach)
                    {
                        spirits_strike(zone, other);
                    }
                }
            }
        }
        guardian_tests(a);
        suppress_spells();
        if (caught)
        {
            break;
        }
    }
    path_.clear();
    path_index_ = 0;
    reactors_.clear();
    reactor_index_ = 0;
    flight_.reset();
}

rules::SpellFacts Session::spell_facts(const Actor &a, const detail::SpellDef &spell) const
{
    return {spell.concentration, spell_range(a, spell),
            spell.radius ? spell.radius
            : spell.area ? spell.area / 2
            : spell.cone ? spell.cone
            : spell.cube ? spell.cube
            : spell.line ? spell.line
                         : std::max(spell.wall, spell.ring),
            spell.cone || spell.cube || spell.line};
}
} // namespace opengold::srd5::detail
