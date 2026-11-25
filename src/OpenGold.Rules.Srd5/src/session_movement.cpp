#include "session.h"

// Session: the zoned board, movement grids and paths, and the snapshot a
// viewer sees.
namespace opengold::srd5::detail
{
using namespace rules;

Battlefield Session::zoned_board() const
{
    // Entangle's plants, Grease and Web make their open squares Difficult
    // Terrain. Adaptation: so does Gust of Wind's line, whose wind only slows
    // movement toward its caster.
    auto board = board_;
    // Walls of Force and Stone stop movement.
    for (const auto &zone : zones_)
    {
        if (zone.kind == ZoneKind::force_wall || zone.kind == ZoneKind::stone_wall ||
            zone.kind == ZoneKind::ice_wall)
        {
            for (const auto cell : zone.cells)
            {
                board.terrain[static_cast<std::size_t>(cell.y * board.width + cell.x)] =
                    Terrain::obstacle;
            }
        }
    }
    for (const auto &zone : zones_)
    {
        // Wall of Thorns: 4 feet of movement for each foot, as Overgrowth.
        if (zone.kind == ZoneKind::thorns)
        {
            for (const auto cell : zone.cells)
            {
                if (board.at(cell) != Terrain::obstacle)
                {
                    board.terrain[static_cast<std::size_t>(cell.y * board.width + cell.x)] =
                        Terrain::overgrown;
                }
            }
        }
    }
    for (const auto &zone : zones_)
    {
        if (zone.kind == ZoneKind::plants || zone.kind == ZoneKind::grease ||
            zone.kind == ZoneKind::web || zone.kind == ZoneKind::gust ||
            zone.kind == ZoneKind::spikes || zone.kind == ZoneKind::swarm ||
            zone.kind == ZoneKind::blades || zone.kind == ZoneKind::quake ||
            zone.kind == ZoneKind::sleet || zone.kind == ZoneKind::tentacles ||
            zone.kind == ZoneKind::hail)
        {
            for (const auto cell : zone.cells)
            {
                if (board.at(cell) == Terrain::open)
                {
                    board.terrain[static_cast<std::size_t>(cell.y * board.width + cell.x)] =
                        Terrain::difficult;
                }
            }
        }
    }
    // Plant Growth's Overgrowth: 4 feet of movement for each foot.
    for (const auto &zone : zones_)
    {
        if (zone.kind == ZoneKind::overgrowth)
        {
            for (const auto cell : zone.cells)
            {
                if (board.at(cell) != Terrain::obstacle)
                {
                    board.terrain[static_cast<std::size_t>(cell.y * board.width + cell.x)] =
                        Terrain::overgrown;
                }
            }
        }
    }
    return board;
}

Cell Session::spectral_cell(const Actor &target, Cell from) const
{
    // The open, unoccupied square beside the target nearest `from`.
    std::optional<Cell> best;
    const auto size = footprint(target);
    for (int dy = -1; dy <= size.height; ++dy)
    {
        for (int dx = -1; dx <= size.width; ++dx)
        {
            const Cell cell{target.source.cell.x + dx, target.source.cell.y + dy};
            if (occupies(target, cell) || !board_.contains(cell) ||
                board_.at(cell) == Terrain::obstacle ||
                std::any_of(actors_.begin(), actors_.end(),
                            [&](const auto &other)
                            {
                                return !other.life.dead && occupies(other, cell);
                            }))
            {
                continue;
            }
            if (!best || distance(cell, from) < distance(*best, from))
            {
                best = cell;
            }
        }
    }
    return best.value_or(target.source.cell);
}

const Session::Zone *Session::spiritual_weapon(const Actor &caster) const
{
    const auto found = std::find_if(zones_.begin(), zones_.end(),
                                    [&](const auto &zone)
                                    {
                                        return zone.kind == ZoneKind::spiritual_weapon &&
                                               zone.caster == caster.source.id;
                                    });
    return found == zones_.end() ? nullptr : &*found;
}

bool Session::silenced(Cell cell) const
{
    return std::any_of(zones_.begin(), zones_.end(),
                       [&](const auto &zone)
                       {
                           return zone.kind == ZoneKind::silence &&
                                  std::find(zone.cells.begin(), zone.cells.end(), cell) !=
                                      zone.cells.end();
                       });
}

bool Session::obscured(const Actor &viewer, Cell cell) const
{
    // Fog Cloud hides from everyone; magical Darkness from all but Devil's Sight.
    return std::any_of(
        zones_.begin(), zones_.end(),
        [&](const auto &zone)
        {
            return (zone.kind == ZoneKind::fog || zone.kind == ZoneKind::sleet ||
                    zone.kind == ZoneKind::stench || zone.kind == ZoneKind::fire_wall ||
                    zone.kind == ZoneKind::cloud || zone.kind == ZoneKind::embers ||
                    zone.kind == ZoneKind::thorns ||
                    (zone.kind == ZoneKind::darkness && !def(viewer).devils_sight &&
                     !detail::has_effect(viewer.effects, detail::EffectKind::true_seeing) &&
                     !(def(viewer).witch_sight && distance(viewer, cell) <= 30))) &&
                   std::find(zone.cells.begin(), zone.cells.end(), cell) != zone.cells.end();
        });
}

detail::MovementGrid Session::movement_grid(const Actor &mover) const
{
    const bool free = detail::has_effect(mover.effects, detail::EffectKind::free_moving);
    // Dragon Wings: over Difficult Terrain and enemies alike (LEVELS-3).
    const bool winged = detail::has_effect(mover.effects, detail::EffectKind::dragon_wings);
    std::vector<detail::Occupant> occupants;
    for (const auto &other : actors_)
    {
        // Unconscious actors still occupy space; corpses do not. The mover's
        // current cell is the path origin, not an obstacle. As in the original
        // game, a creature may stop on the downed (dying heroes walling in the
        // last one could otherwise leave a fight no one can end). Halfling
        // Nimbleness passes through a larger enemy as through an ally: never
        // stopping there.
        if (other.life.dead || downed_riser(other) || other.source.id == mover.source.id ||
            absent(other))
        {
            continue;
        }
        const bool nimble_pass = def(mover).nimble && def(other).size > def(mover).size;
        // Freedom of Movement: an unconscious enemy's square, Difficult
        // Terrain, costs nothing extra.
        const bool free_pass = free && unconscious(other);
        occupants.push_back(
            {other.source.cell,
             other.source.side != mover.source.side && !nimble_pass && !free_pass && !winged,
             unconscious(other), other.life.hp == 0, footprint(other)});
    }
    // A finished Magic Circle: a creature of its type cannot enter it.
    std::vector<Cell> warded_cells;
    for (const auto &zone : zones_)
    {
        if (zone.kind == ZoneKind::circle && zone.ends_ms &&
            def(mover).creature_type == detail::ward_types[zone.warded] && !in_zone(zone, mover))
        {
            warded_cells.insert(warded_cells.end(), zone.cells.begin(), zone.cells.end());
        }
    }
    // A Guardian of Faith holds its square: enemies cannot pass it.
    // A guardian whose caster has left the fight serves no one.
    for (const auto &zone : zones_)
    {
        if (const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                             [&](const auto &a)
                                             {
                                                 return a.source.id == zone.caster;
                                             });
            zone.kind == ZoneKind::guardian && caster != actors_.end())
        {
            occupants.push_back(
                {zone.cells.front(), caster->source.side != mover.source.side, false, false});
        }
    }
    // Frightened: no square closer to a creature it fears and can see.
    auto board = zoned_board();
    // Web Walker: a web's squares are as they were before it.
    if (def(mover).web_walker)
    {
        for (const auto &zone : zones_)
        {
            for (const auto cell : zone.kind == ZoneKind::web ? zone.cells : std::vector<Cell>{})
            {
                const auto index = static_cast<std::size_t>(cell.y * board.width + cell.x);
                board.terrain[index] = board_.terrain[index];
            }
        }
    }
    // Spirit Guardians halve an enemy's Speed in their Emanation: here its
    // squares are Difficult Terrain to it.
    for (const auto &zone : zones_)
    {
        if ((zone.kind == ZoneKind::guardians || zone.kind == ZoneKind::elementals) &&
            actor(zone.caster).source.side != mover.source.side)
        {
            for (int y = 0; y < board.height; ++y)
            {
                for (int x = 0; x < board.width; ++x)
                {
                    if (const Cell cell{x, y};
                        board.at(cell) == Terrain::open && in_emanation(zone, cell))
                    {
                        board.terrain[static_cast<std::size_t>(y * board.width + x)] =
                            Terrain::difficult;
                    }
                }
            }
        }
    }
    if (free || winged)
    {
        for (auto &tile : board.terrain)
        {
            if (tile == Terrain::difficult)
            {
                tile = Terrain::open;
            }
        }
    }
    for (const auto cell : warded_cells)
    {
        if (!occupies(mover, cell))
        {
            board.terrain[static_cast<std::size_t>(cell.y * board.width + cell.x)] =
                Terrain::obstacle;
        }
    }
    // Antilife Shell: only its caster, Constructs and Undead cross its edge.
    for (const auto &zone : zones_)
    {
        if (const auto *caster = find_actor(zone.caster);
            zone.kind == ZoneKind::antilife && caster && caster->source.id != mover.source.id &&
            def(mover).creature_type != "construct" && def(mover).creature_type != "undead")
        {
            const bool inside = distance(*caster, mover) <= 10;
            for (int y = 0; y < board.height; ++y)
            {
                for (int x = 0; x < board.width; ++x)
                {
                    if ((distance(*caster, Cell{x, y}) <= 10) != inside)
                    {
                        board.terrain[static_cast<std::size_t>(y * board.width + x)] =
                            Terrain::obstacle;
                    }
                }
            }
        }
    }
    // Forcecage: no one crosses its bars or walls, in or out.
    for (const auto &zone : zones_)
    {
        if (zone.kind == ZoneKind::cage || zone.kind == ZoneKind::box)
        {
            for (int y = 0; y < board.height; ++y)
            {
                for (int x = 0; x < board.width; ++x)
                {
                    if (in_zone(zone, Cell{x, y}) != in_zone(zone, mover))
                    {
                        board.terrain[static_cast<std::size_t>(y * board.width + x)] =
                            Terrain::obstacle;
                    }
                }
            }
        }
    }
    for (const auto *fear : fears_in_sight(mover))
    {
        for (int y = 0; y < board.height; ++y)
        {
            for (int x = 0; x < board.width; ++x)
            {
                if (distance(Cell{x, y}, *fear) < distance(mover, *fear))
                {
                    board.terrain[static_cast<std::size_t>(y * board.width + x)] =
                        Terrain::obstacle;
                }
            }
        }
    }
    return {board,
            mover.source.cell,
            occupants,
            mover.effects.prone,
            def(mover).flies,
            def(mover).incorporeal,
            footprint(mover)};
}

std::vector<Cell> Session::path_to(const Actor &actor, Cell destination) const
{
    return movement_grid(actor).reachable(movement_left(actor)).path_to(destination);
}

Snapshot Session::snapshot() const
{
    Snapshot s;
    s.identity = content_->identity;
    s.revision = revision_;
    s.round = round_;
    s.outcome = outcome_;
    s.initiative_choices = initiative_choices_;
    s.elapsed_milliseconds = elapsed_ms_;
    s.offerings_gold = offerings_gold_;
    s.allies_attacked = allies_attacked_;
    s.held_items = items_;
    s.physical_inventory = physical_inventory_;
    s.actor = champion_move_ ? champion_move_->actor
              : pending()    ? pending()
                             : actors_[turn_].source.id;
    s.reaction_pending = !champion_move_ && pending() != 0;
    if (given_up_turn_ == turn_ && given_up_round_ == round_)
    {
        s.given_up_spells = given_up_spells_;
    }
    if (reaction_prompt_)
    {
        s.actor = reaction_prompt_->question.target;
        s.reaction_pending = true;
        if (const auto &question = reaction_prompt_->question; question.asked == Asked::hit)
        {
            s.reaction_attack = Snapshot::AttackRoll{question.total, question.armor_class};
        }
    }
    s.battlefield = zoned_board();
    for (const auto &zone : zones_)
    {
        if (zone.kind == ZoneKind::fog || zone.kind == ZoneKind::darkness ||
            zone.kind == ZoneKind::sleet || zone.kind == ZoneKind::stench ||
            zone.kind == ZoneKind::cloud || zone.kind == ZoneKind::embers)
        {
            s.obscured.insert(s.obscured.end(), zone.cells.begin(), zone.cells.end());
        }
        else if (zone.kind == ZoneKind::silence)
        {
            s.silenced.insert(s.silenced.end(), zone.cells.begin(), zone.cells.end());
        }
        // Arcane Sword's sword and Conjure Fey's spirit are drawn as Spiritual
        // Weapon's force is.
        else if (zone.kind == ZoneKind::spiritual_weapon || zone.kind == ZoneKind::sword ||
                 zone.kind == ZoneKind::fey)
        {
            s.spiritual_weapons.push_back(zone.cells.front());
        }
        else if (zone.kind == ZoneKind::flaming_sphere)
        {
            s.flaming_spheres.push_back(zone.cells.front());
        }
        else if (zone.kind == ZoneKind::moonbeam || zone.kind == ZoneKind::pillar)
        {
            s.moonbeams.insert(s.moonbeams.end(), zone.cells.begin(), zone.cells.end());
        }
        else if (zone.kind == ZoneKind::holy || zone.kind == ZoneKind::antimagic ||
                 zone.kind == ZoneKind::elementals)
        {
            for (int y = 0; y < board_.height; ++y)
            {
                for (int x = 0; x < board_.width; ++x)
                {
                    if (const Cell cell{x, y}; in_emanation(zone, cell))
                    {
                        s.circles.push_back(cell);
                    }
                }
            }
        }
        else if (zone.kind == ZoneKind::guardian || zone.kind == ZoneKind::hound)
        {
            s.guardians.push_back(zone.cells.front());
        }
        else if (zone.kind == ZoneKind::circle)
        {
            s.circles.insert(s.circles.end(), zone.cells.begin(), zone.cells.end());
        }
        else if (zone.kind == ZoneKind::animals || zone.kind == ZoneKind::swarm)
        {
            s.spirits.insert(s.spirits.end(), zone.cells.begin(), zone.cells.end());
        }
        else if (emanates(zone))
        {
            for (int y = 0; y < board_.height; ++y)
            {
                for (int x = 0; x < board_.width; ++x)
                {
                    if (const Cell cell{x, y}; in_emanation(zone, cell))
                    {
                        s.spirits.push_back(cell);
                    }
                }
            }
        }
        else if (zone.kind == ZoneKind::wind_wall || zone.kind == ZoneKind::blades ||
                 zone.kind == ZoneKind::fire_wall || zone.kind == ZoneKind::thorns)
        {
            s.walls.insert(s.walls.end(), zone.cells.begin(), zone.cells.end());
        }
        else if (zone.kind == ZoneKind::tentacles)
        {
            s.spirits.insert(s.spirits.end(), zone.cells.begin(), zone.cells.end());
        }
    }
    s.log_entries = log_;
    if (!initiative_choices_.empty())
    {
        s.actor = initiative_choices_.front();
    }
    if (graze_)
    {
        const auto &g = *graze_;
        const auto &a = actor(g.actor);
        const auto &t = actor(g.target);
        const int amount = graze_damage(a, t);
        s.optional_effect_choice = OptionalEffectChoice{
            g.actor,
            g.target,
            {"Graze", {}},
            {"Target: {target}\nGraze damage: {damage}\n\nUse Graze after this miss, or skip it.\nThe attack's Action or Reaction is already spent.",
             {{"target", t.source.name}, {"damage", std::to_string(amount)}}}};
    }
    // A Shield prompt during the cast comes first; the choice resumes after it.
    if (selection_ && !s.reaction_pending)
    {
        s.actor = selection_->caster;
        s.spell_targeting =
            SpellTargeting{selection_->caster, selection_->verb, selection_->chosen,
                           selection_maximum(*selection_),
                           splits_among_targets(*detail::find_spell(selection_->verb))};
    }
    if (area_)
    {
        s.actor = area_->caster;
        const auto &spell = *detail::find_spell(area_->verb);
        s.area_targeting = AreaTargeting{
            area_->caster,
            area_->verb,
            area_->center,
            spell.storm
                ? storm_cells(area_->cubes)
                : area_cells(spell, area_->verb, origin_toward(actor(area_->caster), area_->center),
                             area_->center),
            spell.storm ? 2 : 1,
            spell_facts(actor(area_->caster), spell)};
    }
    if (!champion_move_ && effect_waiting())
    {
        if (mastery_ && mastery_->targeting)
        {
            const auto &m = *mastery_;
            s.actor = m.actor;
            s.effect_targeting = EffectTargeting{
                m.actor, m.kind == detail::Mastery::cleave ? "effect_attack" : "effect_push",
                m.kind == detail::Mastery::cleave
                    ? Message{"Choose a Cleave target, or skip.", {}}
                    : Message{"Choose a Push destination, or skip.", {}},
                m.kind == detail::Mastery::push};
        }
        else
        {
            s.optional_effect_choice = effect_choices();
            s.actor = s.optional_effect_choice->actor;
            if (s.optional_effect_choice->options.size() > 1 &&
                actors_[turn_].source.side == Side::party)
            {
                s.actor = actors_[turn_].source.id;
            }
        }
    }
    if (champion_move_)
    {
        s.free_movement = FreeMovement{champion_move_->actor, champion_move_->remaining};
    }
    if (check_choice_)
    {
        const auto &c = *check_choice_;
        s.ability_check_choice =
            AbilityCheckChoice{c.actor,
                               c.target,
                               c.natural,
                               def(actor(c.actor)).medicine + c.guidance,
                               c.natural + def(actor(c.actor)).medicine + c.guidance,
                               10,
                               actor(c.actor).winds};
    }
    if (temporary_offer_)
    {
        s.temporary_hp_offer = TemporaryHpOffer{
            actors_[turn_].source.id, actors_[turn_].life.temporary_hp, *temporary_offer_};
    }
    // What may be done now, the same for every combatant's weapons below.
    const auto offered = legal_commands();
    for (const auto &a : actors_)
    {
        std::string status = a.life.dead ? "Dead"
                             : a.life.hp == 0
                                 ? (a.life.stable ? "Stable, unconscious" : "Unconscious")
                             : a.effects.prone ? "Prone"
                             : a.dodge         ? "Dodging"
                                               : "Ready";
        if (def(a).slots[1])
        {
            status += " | slots " + std::to_string(a.slots[1]);
        }
        for (unsigned level = 2; level <= max_slot_level; ++level)
        {
            if (def(a).slots[level])
            {
                status +=
                    " | L" + std::to_string(level) + " slots " + std::to_string(a.slots[level]);
            }
        }
        if (def(a).winds)
        {
            status += " | Second Wind " + std::to_string(a.winds);
        }
        s.combatants.push_back(
            {a.source.id, a.source.name, a.source.definition, a.source.side, a.source.cell,
             footprint(a), a.life.hp, max_hp(a), armor_class(a), a.initiative,
             champion_move_ && champion_move_->actor == a.source.id ? champion_move_->remaining
                                                                    : movement_left(a),
             a.actions.available() && conscious(a), a.bonus && conscious(a),
             a.reaction && conscious(a), conscious(a), a.life.dead, a.facing_left, status,
             vitals(a)});
        const auto display = combat_display(a.source.definition);
        auto &view = s.combatants.back();
        view.regenerates = def(a).regeneration > 0;
        view.regeneration_stopped = a.regeneration_blocked;
        view.has_torch = carries(a, "torch") ||
                         std::find(def(a).equipment_keys.begin(), def(a).equipment_keys.end(),
                                   "torch") != def(a).equipment_keys.end();
        view.burning = a.burning;
        view.curse_lifted = a.curse_lifted;
        view.oiled = a.oiled_until_round >= static_cast<int>(round_);
        view.hidden = hidden(a);
        view.banished = absent(a);
        view.panicked = a.panicked;
        view.charging_in_panic = a.panicked && a.must_stay;
        view.spells = def(a).spells;
        view.can_flee = a.source.side == Side::party && conscious(a) && !a.must_stay &&
                        (flight_always_succeeds(a) ||
                         std::none_of(actors_.begin(), actors_.end(),
                                      [&](const auto &other)
                                      {
                                          return other.source.side != a.source.side &&
                                                 conscious(other) &&
                                                 current_speed(other) > current_speed(a);
                                      }));
        for (const auto &gear : detail::thrown_gear_items)
        {
            if (a.source.side == Side::party && !a.source.character_profile.empty())
            {
                view.thrown_gear_left.emplace_back(
                    std::string(gear.key),
                    a.thrown_gear_left[static_cast<std::size_t>(gear.effect)]);
            }
        }
        view.item_uses_left = a.item_uses;
        view.temporary_hp = a.life.temporary_hp;
        view.prone = a.effects.prone;
        view.last_target = a.last_target;
        view.unarmored_arcane_caster = def(a).unarmored_arcane_caster;
        view.critical_hit = a.smite_target && a.smite_critical;
        view.disengaged = a.disengaged;
        view.shield_off = a.shield_off;
        view.weapon_in_hand = weapon_in_hand(a);
        view.weapon_set_shown = a.weapon_set_shown;
        // Each rated by its attacks a turn too: Martial Arts' bonus strike
        // counts for a Monk's melee.
        const int attacks = 1 + def(a).extra_attacks;
        view.melee_rating = melee_rating(def(a));
        if (def(a).range)
        {
            view.ranged_rating =
                (def(a).ranged.count * def(a).ranged.sides + 8 * def(a).ranged_magic) * attacks;
        }
        else if (const auto bow = carried_ranged_weapon(a); !bow.empty())
        {
            const auto *weapon = detail::weapon(bow);
            view.ranged_rating =
                (weapon->dice * weapon->sides + 8 * std::max(0, rules::parse_item_key(bow).bonus)) *
                attacks;
        }
        view.status_effects =
            status_effect_views(a, a.oiled_until_round >= static_cast<int>(round_));
        if (physical_inventory_)
        {
            for (const auto &item : items_)
            {
                if (item.holder == a.source.id)
                {
                    if (const auto *w = detail::weapon(item.definition); w && w->thrown)
                    {
                        view.thrown_rating = std::max(
                            view.thrown_rating,
                            (w->dice * w->sides +
                             8 * std::max(0, rules::parse_item_key(item.definition).bonus)) *
                                attacks);
                        view.thrown_weapons.push_back({item.id, item.label,
                                                       std::any_of(offered.begin(), offered.end(),
                                                                   [&](const auto &c)
                                                                   {
                                                                       return c.verb == "throw" &&
                                                                              c.item == item.id;
                                                                   })});
                    }
                }
            }
        }
        view.selected_weapon = held_weapon(a);
        view.nick_mastery = has_nick(a);
        unsigned hand_index = 0;
        for (const auto &item : items_)
        {
            if (item.holder == a.source.id)
            {
                if (const auto *w = detail::weapon(item.definition))
                {
                    const std::string hand = item.stowed    ? "Carried"
                                             : hand_index++ ? "Other hand"
                                                            : "Main hand";
                    if (!item.stowed)
                    {
                        view.weapons.push_back(
                            {item.id,
                             {"{hand} — {weapon}",
                              {{"hand", hand, true}, {"weapon", item.label.source, true}}},
                             item.id == view.selected_weapon ||
                                 std::any_of(offered.begin(), offered.end(),
                                             [&](const auto &c)
                                             {
                                                 return c.actor == a.source.id &&
                                                        c.verb == "weapon_select" &&
                                                        c.item == item.id;
                                             })});
                    }
                    if (nick_weapon(a, item.id))
                    {
                        const auto option = [&](const char *verb, const char *label)
                        {
                            view.nick_attacks.push_back(
                                {item.id,
                                 verb,
                                 {label,
                                  {{"weapon", item.label.source, true}, {"hand", hand, true}}},
                                 std::any_of(offered.begin(), offered.end(),
                                             [&](const auto &c)
                                             {
                                                 return c.actor == a.source.id && c.verb == verb &&
                                                        c.item == item.id;
                                             })});
                        };
                        if (!item.stowed && !w->ranged)
                        {
                            option("nick_melee", "Nick attack — {weapon} ({hand})");
                        }
                        if (!item.stowed && w->ranged)
                        {
                            option("nick_ranged", "Nick ranged attack — {weapon} ({hand})");
                        }
                        if (w->thrown)
                        {
                            option("nick_throw", "Nick throw — {weapon} ({hand})");
                        }
                    }
                    if (w->light && !a.light_origins.empty())
                    {
                        const auto option = [&](const char *verb, const char *label)
                        {
                            view.light_attacks.push_back(
                                {item.id,
                                 verb,
                                 {label,
                                  {{"weapon", item.label.source, true}, {"hand", hand, true}}},
                                 std::any_of(offered.begin(), offered.end(),
                                             [&](const auto &c)
                                             {
                                                 return c.actor == a.source.id && c.verb == verb &&
                                                        c.item == item.id;
                                             })});
                        };
                        if (!item.stowed && !w->ranged)
                        {
                            option("light_melee", "Light attack — {weapon} ({hand})");
                        }
                        if (!item.stowed && w->ranged)
                        {
                            option("light_ranged", "Light ranged attack — {weapon} ({hand})");
                        }
                        if (w->thrown)
                        {
                            option("light_throw", "Light throw — {weapon} ({hand})");
                        }
                    }
                }
            }
        }
        if (a.effects.prone)
        {
            view.conditions.push_back({"Prone", {}});
        }
        // Its morale broke: it runs for the edge (MORALE-1).
        if (a.panicked)
        {
            view.conditions.push_back({a.must_stay ? "Charging in panic" : "Fleeing in panic", {}});
        }
        if (def(a).cunning)
        {
            view.bonus_actions = {"cunning_dash", "cunning_disengage", "cunning_hide"};
        }
        else if (def(a).nimble_escape)
        {
            view.bonus_actions = {"cunning_disengage", "cunning_hide"};
        }
        if (def(a).sneak_level >= 3)
        {
            view.bonus_actions.push_back("steady_aim");
        }
        if (def(a).lay_on_hands)
        {
            view.bonus_actions.push_back("lay_on_hands");
            view.bonus_actions.push_back("lay_on_hands_cure");
            if (def(a).restoring_touch)
            {
                view.bonus_actions.push_back("lay_on_hands_restore");
            }
        }
        if (def(a).rages)
        {
            view.bonus_actions.insert(view.bonus_actions.end(), {"rage", "extend_rage"});
        }
        if (def(a).flame_tongue)
        {
            view.bonus_actions.push_back(a.flame_lit ? "douse_flame_tongue"
                                                     : "ignite_flame_tongue");
        }
        if (basic_poison_left(a))
        {
            view.bonus_actions.push_back("coat_weapon");
        }
        if (def(a).intimidating_presence)
        {
            view.bonus_actions.insert(view.bonus_actions.end(),
                                      {"intimidating_presence", "intimidating_presence_rage"});
        }
        if (def(a).natures_veil)
        {
            view.bonus_actions.push_back("natures_veil");
        }
        if (detail::knows_spell(def(a).spells, "conjure_woodland_beings"))
        {
            view.bonus_actions.push_back("woodland_disengage");
        }
        if (def(a).martial_arts)
        {
            view.bonus_actions.push_back("martial_arts");
        }
        if (def(a).innate_sorcery)
        {
            view.bonus_actions.push_back("innate_sorcery");
        }
        if (def(a).bardic_inspiration)
        {
            view.bonus_actions.push_back("bardic_inspiration");
        }
        if (def(a).wild_shapes)
        {
            for (const auto &form : detail::beast_forms)
            {
                view.bonus_actions.push_back("wild_shape_" + std::string(form.key));
            }
            view.bonus_actions.push_back("leave_wild_shape");
        }
        if (def(a).sorcery_points)
        {
            for (unsigned level = 1; level <= 5; ++level)
            {
                view.bonus_actions.push_back("create_slot_" + std::to_string(level));
            }
            for (unsigned level = 1; level <= max_slot_level; ++level)
            {
                view.bonus_actions.push_back("convert_slot_" + std::to_string(level));
            }
        }
        for (const auto &known : def(a).metamagic)
        {
            if (known == "transmuted")
            {
                for (const auto type : transmuted_types)
                {
                    view.bonus_actions.push_back("metamagic_transmuted_" + std::string(type));
                }
            }
            else
            {
                view.bonus_actions.push_back("metamagic_" + known);
            }
        }
        if (!def(a).metamagic.empty())
        {
            view.bonus_actions.push_back("metamagic_cancel");
        }
        if (def(a).focus)
        {
            view.bonus_actions.insert(view.bonus_actions.end(),
                                      {"flurry_of_blows", "flurry_addle", "flurry_push",
                                       "flurry_topple", "flurry_stun", "patient_defense",
                                       "patient_defense_focus", "step_of_the_wind",
                                       "step_of_the_wind_focus", "step_of_the_wind_carry"});
        }
        if (detail::knows_spell(def(a).spells, "divine_smite") && def(a).free_smite)
        {
            view.bonus_actions.push_back("divine_smite_free");
        }
        for (const auto *smite : {"divine_smite", "searing_smite", "ensnaring_strike"})
        {
            if (detail::knows_spell(def(a).spells, smite))
            {
                view.bonus_actions.push_back(smite);
            }
        }
        // Divine Smite and Shining Smite from each higher slot level the
        // character has.
        for (unsigned level = 2; level <= max_slot_level; ++level)
        {
            if (!def(a).slots[level])
            {
                continue;
            }
            if (detail::knows_spell(def(a).spells, "divine_smite"))
            {
                view.bonus_actions.push_back("divine_smite_" + std::to_string(level));
            }
            if (detail::knows_spell(def(a).spells, "shining_smite"))
            {
                view.bonus_actions.push_back(level == 2 ? std::string("shining_smite")
                                                        : "shining_smite_" + std::to_string(level));
            }
        }
        for (const auto &descriptor : resource_descriptors)
        {
            if (descriptor.combat_view && capacity_of(descriptor, def(a)))
            {
                view.resources.push_back(resource_pool(descriptor, a, def(a)));
            }
        }
        add_initiate_pools(view.resources, a, def(a));
        add_trait_pool(view.resources, a, a.definition);
        // Explicit display order: this list feeds the combat cantrip dropdown,
        // so its order is observable in the UI and is not the table's order.
        for (const auto *id : {"fire_bolt", "chill_touch", "shocking_grasp", "eldritch_blast",
                               "poison_spray", "ray_of_frost", "sacred_flame"})
        {
            if (detail::knows_spell(def(a).known_cantrips, id))
            {
                view.known_cantrips.push_back(id);
            }
        }

        if (def(a).hit_die)
        {
            view.hp_messages.push_back(
                {"Maximum HP includes level {level}, d{die} Hit Die and Constitution modifier {modifier}.",
                 {{"level", std::to_string(def(a).level)},
                  {"die", std::to_string(def(a).hit_die)},
                  {"modifier", std::to_string(def(a).constitution)}}});
            if (def(a).dwarf)
            {
                view.hp_messages.push_back({"Dwarven Toughness: +{hp} maximum HP.",
                                            {{"hp", std::to_string(def(a).level)}}});
            }
        }
        if (display.type)
        {
            view.type_name = display.type;
        }
        if (display.melee)
        {
            view.melee_weapon = display.melee;
        }
        if (display.ranged)
        {
            view.ranged_weapon = display.ranged;
        }
        view.ranged_attack_available = def(a).range > 0;
        view.ranged_missile = ranged_missile(a);
        auto &messages = s.combatants.back().status_messages;
        messages.push_back({a.life.dead ? "Dead"
                            : a.life.hp == 0
                                ? (a.life.stable ? "Stable, unconscious" : "Unconscious")
                            : a.effects.prone ? "Prone"
                            : a.dodge         ? "Dodging"
                                              : "Ready",
                            {}});
        if (def(a).slots[1])
        {
            messages.push_back({"Spell slots: {count}", {{"count", std::to_string(a.slots[1])}}});
        }
        if (def(a).slots[2])
        {
            messages.push_back({"L2 slots: {count}", {{"count", std::to_string(a.slots[2])}}});
        }
        for (unsigned level = 3; level <= max_slot_level; ++level)
        {
            if (def(a).slots[level])
            {
                messages.push_back({"L{level} slots: {count}",
                                    {{"level", std::to_string(level)},
                                     {"count", std::to_string(a.slots[level])}}});
            }
        }
        if (a.actions.surge)
        {
            messages.push_back({"Action Surge action ready (no Magic).", {}});
        }
        if (def(a).winds)
        {
            messages.push_back({"Second Wind: {count}", {{"count", std::to_string(a.winds)}}});
        }
        for (const auto &effect : a.effects.active)
        {
            if (effect.kind == detail::EffectKind::chill_touch)
            {
                Message message{"Chill Touch ({source}): cannot regain HP.",
                                {{"source", effect.source_name}}};
                messages.push_back(message);
                view.conditions.push_back(std::move(message));
            }
        }
        for (const auto &effect : a.effects.active)
        {
            if (effect.kind == detail::EffectKind::sap || effect.kind == detail::EffectKind::vex ||
                effect.kind == detail::EffectKind::slow)
            {
                Message message{
                    effect.kind == detail::EffectKind::sap
                        ? "Sap ({source}): next attack roll has Disadvantage."
                    : effect.kind == detail::EffectKind::slow
                        ? "Slow ({source}): Speed reduced by 10 feet."
                        : "Vex ({source}): source's next attack against this creature has Advantage.",
                    {{"source", effect.source_name}}};
                messages.push_back(message);
                view.conditions.push_back(std::move(message));
            }
        }
        if (detail::has_effect(a.effects, detail::EffectKind::shocking_grasp))
        {
            messages.push_back({"Shocking Grasp: cannot make Opportunity Attacks.", {}});
            view.conditions.push_back({"Shocking Grasp: cannot make Opportunity Attacks.", {}});
        }
        if (detail::frosted(a.effects))
        {
            messages.push_back({"Ray of Frost: Speed reduced by 10 feet.", {}});
            view.conditions.push_back({"Ray of Frost: Speed reduced by 10 feet.", {}});
        }
        if (detail::has_effect(a.effects, detail::EffectKind::hamstrung))
        {
            messages.push_back({"Hamstring Blow: Speed reduced by 15 feet.", {}});
            view.conditions.push_back({"Hamstring Blow: Speed reduced by 15 feet.", {}});
        }
        if (detail::has_effect(a.effects, detail::EffectKind::frosts_chill))
        {
            messages.push_back({"Frost's Chill: Speed reduced by 10 feet.", {}});
            view.conditions.push_back({"Frost's Chill: Speed reduced by 10 feet.", {}});
        }
        for (const auto &effect : a.effects.active)
        {
            if (effect.kind == detail::EffectKind::studied)
            {
                Message message{
                    "Studied Attacks ({source}): source's next attack against this creature has Advantage.",
                    {{"source", effect.source_name}}};
                messages.push_back(message);
                view.conditions.push_back(std::move(message));
            }
        }
        if (detail::has_effect(a.effects, detail::EffectKind::guiding_bolt))
        {
            messages.push_back(
                {"Guiding Bolt: the next attack roll against it has Advantage.", {}});
            view.conditions.push_back(
                {"Guiding Bolt: the next attack roll against it has Advantage.", {}});
        }
        if (detail::has_effect(a.effects, detail::EffectKind::bane))
        {
            messages.push_back({"Bane: -1d4 to attack rolls and saving throws.", {}});
            view.conditions.push_back({"Bane: -1d4 to attack rolls and saving throws.", {}});
        }
        for (const auto &[kind, label] :
             {std::pair{detail::EffectKind::drowsy, "Incapacitated (Sleep)"},
              std::pair{detail::EffectKind::asleep, "Unconscious (Sleep)"},
              std::pair{detail::EffectKind::knocked_out, "Unconscious (Knock Out)"},
              std::pair{detail::EffectKind::dazed, "Dazed"},
              std::pair{detail::EffectKind::laughing, "Prone and Incapacitated (laughing)"},
              std::pair{detail::EffectKind::enfeebled, "Enfeebled (Ray of Enfeeblement)"},
              std::pair{detail::EffectKind::acid_arrow, "Burning acid (Acid Arrow)"},
              std::pair{detail::EffectKind::invisible, "Invisible"},
              std::pair{detail::EffectKind::veiled, "Invisible (Nature's Veil)"},
              std::pair{detail::EffectKind::stoneskin, "Stoneskin"},
              std::pair{detail::EffectKind::energy_ward, "Protection from Energy"},
              std::pair{detail::EffectKind::dominated, "Dominated"},
              std::pair{detail::EffectKind::cursed, "Cursed"},
              std::pair{detail::EffectKind::contagion, "Poisoned (Contagion)"},
              std::pair{detail::EffectKind::drained, "Hit Point maximum lowered (Harm)"},
              std::pair{detail::EffectKind::life_drained, "Hit Point maximum lowered (Life Drain)"},
              std::pair{detail::EffectKind::feasted, "Heroes' Feast"},
              std::pair{detail::EffectKind::true_seeing, "True Seeing"},
              std::pair{detail::EffectKind::regenerating, "Regenerate"},
              std::pair{detail::EffectKind::hasted, "Haste"},
              std::pair{detail::EffectKind::paralysis_ray, "Paralyzed (Wand of Paralysis)"},
              std::pair{detail::EffectKind::giant_strength, "Giant Strength"},
              std::pair{detail::EffectKind::enhance_ability, "Enhance Ability (Strength)"},
              std::pair{detail::EffectKind::missile_ward, "Protection From Normal Missiles"},
              std::pair{detail::EffectKind::snake_charmed, "Incapacitated (Snake Charm)"},
              std::pair{detail::EffectKind::envenomed, "Poisoned (Cunning Strike)"},
              std::pair{detail::EffectKind::enthralled, "Charmed (Enthralling Panache)"},
              std::pair{detail::EffectKind::turn_charmed, "Charmed"},
              std::pair{detail::EffectKind::hampered, "Hampered"},
              std::pair{detail::EffectKind::clawed, "Paralyzed (Claw)"},
              std::pair{detail::EffectKind::envenomed_paralyzed, "Poisoned and Paralyzed (venom)"},
              std::pair{detail::EffectKind::lethargic, "Incapacitated (lethargy)"},
              std::pair{detail::EffectKind::entranced, "Charmed (Hypnotic Pattern)"},
              std::pair{detail::EffectKind::compelled, "Charmed (Compulsion)"},
              std::pair{detail::EffectKind::terrified, "Frightened (Fear)"},
              std::pair{detail::EffectKind::retching, "Poisoned (Stinking Cloud)"},
              std::pair{detail::EffectKind::vampiric, "Vampiric Touch"},
              std::pair{detail::EffectKind::blinking, "Blink"},
              std::pair{detail::EffectKind::ethereal, "Ethereal"},
              std::pair{detail::EffectKind::jaunted, "Ethereal (Ethereal Jaunt)"},
              std::pair{detail::EffectKind::tentacled, "Restrained (Black Tentacles)"},
              std::pair{detail::EffectKind::confused, "Confused"},
              std::pair{detail::EffectKind::time_slowed, "Slow"},
              std::pair{detail::EffectKind::fire_shield, "Fire Shield"},
              std::pair{detail::EffectKind::greater_invisible, "Invisible"},
              std::pair{detail::EffectKind::haunted, "Phantasmal Killer"},
              std::pair{detail::EffectKind::polymorphed, "Polymorphed"},
              std::pair{detail::EffectKind::sphered, "Resilient Sphere"},
              std::pair{detail::EffectKind::eye_panic, "Frightened (Eyebite)"},
              std::pair{detail::EffectKind::sickened, "Poisoned (Eyebite)"},
              std::pair{detail::EffectKind::stoning, "Restrained (Flesh to Stone)"},
              std::pair{detail::EffectKind::petrified, "Petrified"},
              std::pair{detail::EffectKind::petrifying, "Restrained (Petrifying Gaze)"},
              std::pair{detail::EffectKind::attached, "Attached"},
              std::pair{detail::EffectKind::dancing, "Charmed (Irresistible Dance)"},
              std::pair{detail::EffectKind::jigging, "Dancing"},
              std::pair{detail::EffectKind::jarred, "Body left behind (Magic Jar)"},
              std::pair{detail::EffectKind::contingency, "Contingency"},
              std::pair{detail::EffectKind::aloft, "Fallen upward (Reverse Gravity)"},
              std::pair{detail::EffectKind::indigo, "Restrained (Prismatic Spray)"},
              std::pair{detail::EffectKind::violet, "Blinded (Prismatic Spray)"},
              std::pair{detail::EffectKind::befuddled, "Befuddled"},
              std::pair{detail::EffectKind::mind_blank, "Mind Blank"},
              std::pair{detail::EffectKind::mazed, "Lost in a maze"},
              std::pair{detail::EffectKind::word_stunned, "Stunned (Power Word Stun)"},
              std::pair{detail::EffectKind::overchannel, "Overchannel"},
              std::pair{detail::EffectKind::dragon_wings, "Dragon Wings"},
              std::pair{detail::EffectKind::hurled, "Hurled through the Lower Planes"},
              std::pair{detail::EffectKind::fey_fright, "Frightened (Conjure Fey)"},
              std::pair{detail::EffectKind::glared, "Frightened (Dreadful Glare)"},
              std::pair{detail::EffectKind::rotting_curse, "Cursed (Rotting Fist)"},
              std::pair{detail::EffectKind::animal_shaped, "Animal Shapes"},
              std::pair{detail::EffectKind::enlarged, "Enlarged"},
              std::pair{detail::EffectKind::reduced, "Reduced"},
              std::pair{detail::EffectKind::charmed, "Charmed"},
              std::pair{detail::EffectKind::raging, "Raging"},
              std::pair{detail::EffectKind::reckless, "Reckless"},
              std::pair{detail::EffectKind::addled, "Addled"},
              std::pair{detail::EffectKind::frightened, "Frightened"},
              std::pair{detail::EffectKind::abjured, "Frightened (Abjure Foes)"},
              std::pair{detail::EffectKind::hopeful, "Beacon of Hope"},
              std::pair{detail::EffectKind::smite_protected, "Smite of Protection"},
              std::pair{detail::EffectKind::death_ward, "Death Ward"},
              std::pair{detail::EffectKind::aura_of_life, "Aura of Life"},
              std::pair{detail::EffectKind::free_moving, "Freedom of Movement"},
              std::pair{detail::EffectKind::banished, "Banished"},
              std::pair{detail::EffectKind::fallen, "Fallen"},
              std::pair{detail::EffectKind::staggering, "Staggered (Staggering Blow)"},
              std::pair{detail::EffectKind::sundered, "Sundered (Sundering Blow)"},
              std::pair{detail::EffectKind::innate_sorcery, "Innate Sorcery"},
              std::pair{detail::EffectKind::metamagic, "Metamagic readied"},
              std::pair{detail::EffectKind::hex, "Hexed"},
              std::pair{detail::EffectKind::outlined, "Outlined (Faerie Fire)"},
              std::pair{detail::EffectKind::shining, "Shining (Shining Smite)"},
              std::pair{detail::EffectKind::lit, "Lit (Starry Wisp)"},
              std::pair{detail::EffectKind::inspired, "Inspired"},
              std::pair{detail::EffectKind::shillelagh, "Shillelagh"},
              std::pair{detail::EffectKind::produce_flame, "Produce Flame"},
              std::pair{detail::EffectKind::barkskin, "Barkskin"},
              std::pair{detail::EffectKind::flame_blade, "Flame Blade"},
              std::pair{detail::EffectKind::heated, "Heated (Heat Metal)"},
              std::pair{detail::EffectKind::scorched, "Scorched (Heat Metal)"},
              std::pair{detail::EffectKind::hidden, "Hidden"}})
        {
            if (detail::has_effect(a.effects, kind))
            {
                messages.push_back({label, {}});
                s.combatants.back().status += std::string(" | ") + label;
                s.combatants.back().conditions.push_back({label, {}});
            }
        }
        for (const auto &[shown, label] :
             {std::pair{a.burning, "Burning"},
              std::pair{a.oiled_until_round >= static_cast<int>(round_), "Covered in oil"}})
        {
            if (shown)
            {
                messages.push_back({label, {}});
                s.combatants.back().status += std::string(" | ") + label;
                s.combatants.back().conditions.push_back({label, {}});
            }
        }
        if (a.concentration.active())
        {
            messages.push_back({"Concentrating", {}});
            s.combatants.back().status += " | Concentrating";
            s.combatants.back().conditions.push_back({"Concentrating", {}});
        }
        // The art of a creature in a Polymorph form, a summoned creature, or
        // Mislead's double, which wears its caster's.
        for (const auto &effect : a.effects.active)
        {
            if (effect.kind == detail::EffectKind::polymorphed)
            {
                s.combatants.back().form = std::string(
                    detail::beast_forms.at(static_cast<std::size_t>(effect.dc - 1)).key);
            }
            else if (effect.kind == detail::EffectKind::illusory)
            {
                s.combatants.back().form = "copy:" + std::to_string(effect.source_actor);
            }
            else if (effect.kind == detail::EffectKind::summoned)
            {
                s.combatants.back().form =
                    a.source.definition.starts_with("draconic-spirit") ? "draconic_spirit"
                    : a.source.definition.starts_with("giant-insect")  ? "giant_insect"
                                                                       : "animated_object";
            }
        }
        for (const auto &effect : a.effects.active)
        {
            if (effect.kind == detail::EffectKind::wild_shape)
            {
                const auto &form = detail::beast_forms.at(static_cast<std::size_t>(effect.dc - 1));
                s.combatants.back().form = std::string(form.key);
                const Message shape{"Wild Shape ({form})",
                                    {{"form", std::string(form.label), true}}};
                messages.push_back(shape);
                s.combatants.back().status += " | Wild Shape (" + std::string(form.label) + ")";
                s.combatants.back().conditions.push_back(shape);
            }
        }
        for (const auto &effect : a.effects.active)
        {
            if (effect.kind == detail::EffectKind::mirror_image)
            {
                const Message duplicates{"Mirror Image ({count} duplicates)",
                                         {{"count", std::to_string(effect.dc)}}};
                messages.push_back(duplicates);
                s.combatants.back().status +=
                    " | Mirror Image (" + std::to_string(effect.dc) + " duplicates)";
                s.combatants.back().conditions.push_back(duplicates);
            }
        }
        if (detail::has_effect(a.effects, detail::EffectKind::poisoned))
        {
            messages.push_back({"Poisoned", {}});
            s.combatants.back().status += " | Poisoned";
            s.combatants.back().conditions.push_back({"Poisoned", {}});
        }
        if (detail::paralyzed(a.effects))
        {
            messages.push_back({"Paralyzed", {}});
            s.combatants.back().status += " | Paralyzed";
            s.combatants.back().conditions.push_back({"Paralyzed", {}});
        }
        if (detail::stunned(a.effects))
        {
            messages.push_back({"Stunned", {}});
            s.combatants.back().status += " | Stunned";
            s.combatants.back().conditions.push_back({"Stunned", {}});
        }
        if (detail::restrained(a.effects))
        {
            messages.push_back({"Restrained", {}});
            s.combatants.back().status += " | Restrained";
            s.combatants.back().conditions.push_back({"Restrained", {}});
        }
        if (detail::blinded(a.effects))
        {
            messages.push_back({"Blinded", {}});
            s.combatants.back().status += " | Blinded";
            s.combatants.back().conditions.push_back({"Blinded", {}});
        }
    }
    // Members who ran off the field are reported with the vitals they left with.
    for (const auto &gone : fled_)
    {
        CombatantView view{gone.source.id,   gone.source.name, gone.source.definition,
                           gone.source.side, gone.source.cell, gone.source.footprint,
                           gone.hp,          gone.max_hp,      gone.armor_class};
        view.status = gone.surrendered ? "Surrendered" : "Fled";
        view.status_messages = {{view.status, {}}};
        view.persistent = gone.vitals;
        view.thrown_gear_left = gone.thrown_gear_left;
        view.item_uses_left = gone.item_uses;
        view.fled = !gone.surrendered;
        view.surrendered = gone.surrendered;
        s.combatants.push_back(std::move(view));
    }
    return s;
}
} // namespace opengold::srd5::detail
