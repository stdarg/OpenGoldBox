#include "session.h"

// Session: the commands a fighter may give (legal and offered), compulsions
// and movement reach.
namespace opengold::srd5::detail
{
using namespace rules;

std::vector<Command> Session::legal_commands() const
{
    // A banished creature is off the field: nothing can be aimed at it.
    auto commands = offered_commands();
    std::erase_if(commands,
                  [&](const auto &command)
                  {
                      return command.target != command.actor &&
                             std::any_of(actors_.begin(), actors_.end(),
                                         [&](const auto &a)
                                         {
                                             return a.source.id == command.target && absent(a);
                                         });
                  });
    // The features that strike or heal one creature, offered apart from the
    // weapon attacks and spells that mark their own.
    for (auto &command : commands)
    {
        if (command.intent == Intent::neutral && command.target)
        {
            command.intent = struck_by(command.verb)   ? Intent::hostile
                             : healed_by(command.verb) ? Intent::friendly
                                                       : Intent::neutral;
        }
    }
    // The targets CLOSE-1 opened come after the rest, which keep their order:
    // an attack on the actor's own side or on a downed character, healing on a
    // foe.
    std::stable_partition(commands.begin(), commands.end(),
                          [&](const Command &command)
                          {
                              const auto *target = find_actor(command.target);
                              const auto *actor = find_actor(command.actor);
                              if (!target || !actor || command.intent == Intent::neutral)
                              {
                                  return true;
                              }
                              const bool ally = target->source.side == actor->source.side;
                              return command.intent == Intent::friendly
                                         ? ally
                                         : !ally && (target->life.hp > 0 || may_rise(*target));
                          });
    for (auto &command : commands)
    {
        // In the units of melee_rating: a choice of weapon has no target yet.
        if (command.verb == "weapon_select")
        {
            if (const auto *chooser = find_actor(command.actor))
            {
                command.expected_damage =
                    melee_rating(equipped_definition(*chooser, items_, command.item));
            }
        }
        else if (command.target && find_actor(command.target))
        {
            if (const auto *attacker = find_actor(command.actor))
            {
                command.expected_damage = expected_damage(*attacker, command);
                if (command.verb == "ranged" || command.verb == "shoot" || command.verb == "throw")
                {
                    command.close_damage =
                        expected_damage(*attacker, actor(command.target), AttackRange::melee);
                }
            }
        }
    }
    return commands;
}

std::vector<Command> Session::offered_commands() const
{
    std::vector<Command> commands;
    if (outcome_ != Outcome::ongoing)
    {
        return commands;
    }
    const auto add = [&](EntityId who, std::string verb, std::string label, EntityId target = 0,
                         Cell destination = Cell{})
    {
        commands.push_back(
            {revision_, who, target, std::move(verb), std::move(label), destination});
    };
    if (reaction_prompt_)
    {
        const auto &question = reaction_prompt_->question;
        const auto &who = actor(question.target);
        if (question.asked == Asked::redirect)
        {
            add(who.source.id, "redirect", "Redirect the attack (1 Focus)");
        }
        else if (question.asked == Asked::rebuke)
        {
            add(who.source.id, "rebuke", "Cast Hellish Rebuke");
        }
        else if (question.asked == Asked::boon)
        {
            add(who.source.id, "giant_boon", std::string(giant_boon_label(def(who).giant)));
        }
        else if (question.asked == Asked::endure)
        {
            add(who.source.id, "stones_endurance", "Stone's Endurance");
        }
        else if (question.asked == Asked::thunder)
        {
            add(who.source.id, "storms_thunder", "Storm's Thunder");
        }
        else if (question.asked == Asked::retaliate)
        {
            add(who.source.id, "retaliate", "Retaliation attack");
        }
        else if (question.asked == Asked::counter)
        {
            add(who.source.id, "counterspell", "Cast Counterspell");
        }
        else
        {
            if (can_shield(who) && !question.critical)
            {
                add(who.source.id, "shield",
                    question.asked == Asked::missile ? "Cast Shield against Magic Missile"
                                                     : "Cast Shield against the hit");
            }
            if (question.asked == Asked::hit && can_deflect(who, question.deflectable))
            {
                add(who.source.id, "deflect", "Deflect Attacks");
            }
            if (question.asked == Asked::hit && can_dodge(who))
            {
                add(who.source.id, "uncanny_dodge", "Uncanny Dodge");
            }
            if (question.asked == Asked::hit && can_defend(who))
            {
                add(who.source.id, "superior_defense", "Superior Hunter's Defense");
            }
        }
        add(who.source.id, "decline", "Decline reaction");
        return commands;
    }
    // While aiming an area spell, only moving the preview, casting or cancelling.
    if (area_)
    {
        const auto &caster = actor(area_->caster);
        const auto &spell = *detail::find_spell(area_->verb);
        // Misty Step, Flaming Sphere and the summoned need an unoccupied
        // space, as large as the creature put there (FOOT-1).
        const bool open_square = spell.rider == detail::Rider::misty_step ||
                                 spell.rider == detail::Rider::flaming_sphere ||
                                 spell.rider == detail::Rider::guardian_of_faith ||
                                 spell.rider == detail::Rider::faithful_hound ||
                                 spell.rider == detail::Rider::giant_insect ||
                                 spell.rider == detail::Rider::summon_dragon;
        const Footprint placed =
            spell.rider == detail::Rider::giant_insect    ? summoned_footprint("giant-insect")
            : spell.rider == detail::Rider::summon_dragon ? summoned_footprint("draconic-spirit")
            : spell.rider == detail::Rider::misty_step ||
                    spell.rider == detail::Rider::dimension_door
                ? footprint(caster)
                : Footprint{};
        // An area centered on a square needs a clear line to that square; a
        // cone, line or cube spreads from the caster and is only pointed.
        // Dimension Door needs only a free square, seen or not.
        const bool door = spell.rider == detail::Rider::dimension_door;
        const bool centered = !spell.cone && !spell.line && !spell.cube && !door;
        const auto aimable = [&](Cell cell)
        {
            const bool free = room_for(&caster, placed, cell);
            // Telekinesis moves its creature up to 30 feet.
            const bool within = spell.rider != detail::Rider::telekinesis ||
                                distance(actor(area_->companion), cell) <= 30;
            return (!open_square || teleport_open(caster, cell, placed)) && (!door || free) &&
                   within && (!centered || line_of_sight(caster, cell));
        };
        // Call Lightning's later bolts strike under its cloud, wherever the
        // caster has gone.
        std::optional<Cell> origin;
        if (area_->verb == "call_lightning_again")
        {
            for (const auto &zone : zones_)
            {
                if (zone.kind == ZoneKind::storm && zone.caster == caster.source.id)
                {
                    origin = zone.cells.front();
                }
            }
        }
        // Nature's Sanctuary moves up to 60 feet from where it stands.
        const bool trees_move = area_->verb == "move_sanctuary";
        if (trees_move)
        {
            for (const auto &zone : zones_)
            {
                if (zone.kind == ZoneKind::sanctuary && zone.caster == caster.source.id)
                {
                    origin = zone.cells[zone.cells.size() / 2];
                }
            }
        }
        for (int y = 0; y < board_.height; ++y)
        {
            for (int x = 0; x < board_.width; ++x)
            {
                // From the caster's nearest square, or the cloud's or trees'.
                const int away =
                    origin ? distance(*origin, Cell{x, y}) : distance(caster, Cell{x, y});
                if (away <= (trees_move ? 60 : spell_range(caster, spell)) && aimable(Cell{x, y}))
                {
                    add(caster.source.id, "area_move", std::string(spell.label), 0, Cell{x, y});
                }
            }
        }
        if (spell.storm ? !area_->cubes.empty() : aimable(area_->center))
        {
            add(caster.source.id, "area_cast", "Cast spell");
        }
        add(caster.source.id, "spell_cancel", "Cancel");
        return commands;
    }
    // While choosing a spell's creatures, only the choice itself is open:
    // any living creature in range toggles, then cast or cancel.
    if (selection_)
    {
        const auto &caster = actor(selection_->caster);
        const auto &spell = *detail::find_spell(selection_->verb);
        const bool split = splits_among_targets(spell);
        for (const auto &other : actors_)
        {
            if (other.life.dead || distance(caster, other) > spell.range || sealed_off(other))
            {
                continue;
            }
            // Abjure Foes: enemies the Paladin sees.
            if (spell.rider == detail::Rider::abjure_foes &&
                (other.source.side == caster.source.side || other.life.hp == 0 ||
                 !can_see(caster, other)))
            {
                continue;
            }
            // A dart or ray can only be aimed at a creature the spell could
            // strike alone; another one may be aimed at it again. A ray, not a
            // dart, may be aimed at a downed but living creature.
            if (split &&
                (other.source.side == caster.source.side ||
                 (other.life.hp <= 0 && spell.pattern != detail::SpellPattern::repeat_attack) ||
                 (spell.requires_sight ? !can_see(caster, other) : !line_of_sight(caster, other))))
            {
                continue;
            }
            add(caster.source.id, selection_->verb, std::string(spell.label), other.source.id);
        }
        // Every dart or ray is aimed before the spell is cast.
        if (!selection_->chosen.empty() && !split)
        {
            add(caster.source.id, "spell_cast", "Cast spell");
        }
        add(caster.source.id, "spell_cancel", "Cancel");
        return commands;
    }
    if (!initiative_choices_.empty())
    {
        for (const auto id : initiative_choices_)
        {
            const auto &a = actor(id);
            add(id, "initiative_keep", "Keep initiative");
            if (def(a).alert)
            {
                for (const auto &ally : actors_)
                {
                    if (ally.source.id != id && ally.source.side == a.source.side &&
                        conscious(ally))
                    {
                        add(id, "initiative_swap", "Swap initiative", ally.source.id);
                    }
                }
            }
            if (metabolism_ready(a))
            {
                add(id, "uncanny_metabolism", "Uncanny Metabolism");
            }
            if (persistent_ready(a))
            {
                add(id, "persistent_rage", "Persistent Rage: regain all Rage uses");
            }
        }
        return commands;
    }
    const auto add_weapons = [&](const Actor &a, bool reacting)
    {
        for (const auto &item : items_)
        {
            if (item.holder == a.source.id && !item.stowed && detail::weapon(item.definition) &&
                item.id != held_weapon(a))
            {
                // Mid-move, only a weapon that could make the reaction.
                bool offer = !reacting;
                if (!offer)
                {
                    const auto loadout = item_loadout(a, item.id);
                    offer = weapon_reaction(a, def(a, loadout));
                }
                if (offer)
                {
                    add(a.source.id, "weapon_select", "Select weapon");
                    commands.back().item = item.id;
                }
            }
        }
    };
    const auto filtered = [&]
    {
        // A Charmed creature cannot attack or target its charmer.
        std::erase_if(commands,
                      [&](const auto &command)
                      {
                          return command.target && command.target != command.actor &&
                                 charmed_by(actor(command.actor), command.target);
                      });
        std::erase_if(
            commands,
            [&](const auto &command)
            {
                const auto range =
                    command.verb == "ranged" || command.verb == "throw" ||
                            command.verb == "light_ranged" || command.verb == "light_throw" ||
                            command.verb == "nick_ranged" || command.verb == "nick_throw"
                        ? AttackRange::ranged
                        : AttackRange::melee;
                if (range == AttackRange::melee && command.verb != "melee" &&
                    command.verb != "opportunity" && command.verb != "light_melee" &&
                    command.verb != "nick_melee")
                {
                    return false;
                }
                const auto &attacker = actor(command.actor);
                const auto &target = actor(command.target);
                // A thrown, light or nick attack is made with that weapon.
                if (command.item && (command.verb == "throw" || command.verb == "light_throw" ||
                                     command.verb == "nick_throw"))
                {
                    const auto thrown =
                        thrown_loadout(attacker, items_.at(command.item - 1).definition);
                    return !mastery_capacity(attacker, def(attacker, thrown), target, range);
                }
                if (command.item &&
                    (command.verb.starts_with("light_") || command.verb.starts_with("nick_")))
                {
                    const auto held = item_loadout(attacker, command.item);
                    return !mastery_capacity(attacker, def(attacker, held), target, range);
                }
                return !mastery_capacity(attacker, def(attacker), target, range);
            });
        return commands;
    };
    if (graze_)
    {
        add(graze_->actor, "effect_use", "Use", graze_->target);
        add(graze_->actor, "effect_skip", "Skip", graze_->target);
        return commands;
    }
    if (!champion_move_ && effect_waiting())
    {
        if (mastery_ && mastery_->targeting)
        {
            const auto &m = *mastery_;
            if (m.kind == detail::Mastery::cleave)
            {
                for (auto target : cleave_targets(m))
                {
                    add(m.actor, "effect_attack", "Cleave attack", target);
                }
            }
            else
            {
                for (auto cell : push_cells(m))
                {
                    add(m.actor, "effect_push", "Push", m.target, cell);
                }
            }
            add(m.actor, "effect_skip", "Skip", m.target);
            commands.back().item = 1;
            if (m.kind == detail::Mastery::cleave &&
                std::none_of(commands.begin(), commands.end(),
                             [&](const auto &c)
                             {
                                 return c.verb == "effect_attack" &&
                                        actor(c.target).source.side != actor(m.actor).source.side;
                             }))
            {
                std::rotate(commands.begin(), commands.end() - 1, commands.end());
            }
        }
        else
        {
            const auto choice = effect_choices();
            for (const auto &option : choice.options)
            {
                if (option.available)
                {
                    add(choice.actor, "effect_use", "Use", choice.target);
                    commands.back().item = option.id;
                }
                add(choice.actor, "effect_skip", "Skip", choice.target);
                commands.back().item = option.id;
            }
        }
        return commands;
    }
    if (champion_move_)
    {
        add(champion_move_->actor, "end", "Finish free move");
        const auto &mover_cell = actor(champion_move_->actor).source.cell;
        for (const auto cell : movement_reach(champion_move_->actor))
        {
            // Forceful Blow's move closes on its target the whole way.
            if (!champion_move_->toward || distance(cell, actor(champion_move_->toward)) ==
                                               distance(mover_cell, actor(champion_move_->toward)) -
                                                   distance(mover_cell, cell))
            {
                add(champion_move_->actor, "move", "Free move", 0, cell);
            }
        }
        return filtered();
    }
    if (check_choice_)
    {
        add(check_choice_->actor, "mind_use", "Use Tactical Mind", check_choice_->target);
        add(check_choice_->actor, "mind_skip", "Keep failed check", check_choice_->target);
        return filtered();
    }
    if (temporary_offer_)
    {
        add(actors_[turn_].source.id, "temp_hp_keep", "Keep current");
        add(actors_[turn_].source.id, "temp_hp_use", "Use new");
        return filtered();
    }
    if (pending())
    {
        const auto reactor = std::find_if(actors_.begin(), actors_.end(),
                                          [&](const auto &a)
                                          {
                                              return a.source.id == pending();
                                          });
        add_weapons(*reactor, true);
        if (weapon_reaction(*reactor, def(*reactor)))
        {
            add(pending(), "opportunity", "Opportunity attack", mover().source.id);
        }
        add(pending(), "decline", "Decline reaction");
        return filtered();
    }
    const auto &a = actors_[turn_];
    if (!conscious(a))
    {
        return filtered();
    }
    const auto &d = def(a);
    const auto id = a.source.id;
    add(id, "end", "End turn");
    add_weapons(a, false);
    const int speed = a.aim_used ? 0 : std::max(0, d.speed - detail::speed_penalty(a.effects));
    if (a.effects.prone && speed > 0 && movement_left(a) >= speed / 2)
    {
        add(id, "stand_up", "Stand up");
    }
    if (d.surges && a.surges[CharacterClass::fighter] > 0 && !a.surge_used)
    {
        add(id, "action_surge", "Action Surge", id);
    }
    if (a.bonus && d.sneak_level >= 3 && !a.moved)
    {
        add(id, "steady_aim", "Steady Aim");
    }
    if (a.bonus && d.cunning)
    {
        add(id, "cunning_dash", "Cunning Action: Dash");
        add(id, "cunning_disengage", "Cunning Action: Disengage");
        if (can_hide(a))
        {
            add(id, "cunning_hide", "Cunning Action: Hide");
        }
    }
    else if (a.bonus && d.nimble_escape)
    {
        add(id, "cunning_disengage", "Nimble Escape: Disengage");
        if (can_hide(a))
        {
            add(id, "cunning_hide", "Nimble Escape: Hide");
        }
    }
    if (a.bonus && a.rushes > 0)
    {
        add(id, "adrenaline_rush", "Adrenaline Rush", id);
    }
    // The Cleric's Channel Divinity: Divine Spark heals or harms another creature
    // within 30 feet; Turn Undead needs an Undead enemy within 30 feet.
    // Revivify: an adjacent ally dead no more than a minute, while the purse
    // holds its offering.
    if (const auto &raise = *detail::find_spell("revivify");
        a.actions.available(true) && !a.spent_slot && detail::knows_spell(d.spells, raise.id))
    {
        for (const auto &other : actors_)
        {
            if (other.source.side == a.source.side && other.life.dead &&
                distance(a, other) <= raise.range &&
                detail::has_effect(other.effects, detail::EffectKind::fallen) &&
                !detail::has_effect(other.effects, detail::EffectKind::dust) &&
                std::cmp_less_equal(300 * def(other).level, gold_ - offerings_gold_))
            {
                for (const unsigned level : cast_levels(a, raise))
                {
                    auto [verb, label] = cast_at(raise, level, "revivify", "Revivify");
                    add(id, std::move(verb), std::move(label), other.source.id);
                }
            }
        }
    }
    // Animate Dead: an enemy's Small or Medium Humanoid corpse within 10 feet,
    // while the caster is not already casting it.
    // Create Undead too (LEVELS-3: at any hour).
    for (const auto *raising : {"animate_dead", "create_undead"})
    {
        if (const auto &animate = *detail::find_spell(raising);
            a.actions.available(true) && !a.spent_slot &&
            detail::knows_spell(d.spells, animate.id) &&
            !detail::has_effect(a.effects, detail::EffectKind::animating))
        {
            for (const auto &other : actors_)
            {
                if (animatable(other, a))
                {
                    for (const unsigned level : cast_levels(a, animate))
                    {
                        auto [verb, label] = cast_at(animate, level, std::string(animate.id),
                                                     std::string(animate.label));
                        add(id, std::move(verb), std::move(label), other.source.id);
                    }
                }
            }
        }
    }
    // Dispel Evil and Good: a Magic action ends it to free an ally within 5
    // feet from such creatures' charm or fear, or to send one of them home.
    if (detail::has_effect(a.effects, detail::EffectKind::dispel_evil) && a.actions.available(true))
    {
        for (const auto &other : actors_)
        {
            if (other.life.dead || distance(a, other) > 5)
            {
                continue;
            }
            if (other.source.side == a.source.side && enchanted_by_otherworld(other))
            {
                add(id, "dispel_break", "Dispel Evil and Good: Break Enchantment", other.source.id);
            }
            if (other.source.side != a.source.side && other.life.hp > 0 && otherworldly(other) &&
                can_see(a, other))
            {
                add(id, "dispel_dismissal", "Dispel Evil and Good: Dismissal", other.source.id);
            }
        }
    }
    // Abjure Foes: enemies the Paladin sees within 60 feet.
    if (a.channel_divinity[CharacterClass::cleric] > 0 && a.actions.available() && d.abjure_foes)
    {
        for (const auto &other : actors_)
        {
            if (other.source.side != a.source.side && other.life.hp > 0 &&
                distance(a, other) <= 60 && can_see(a, other))
            {
                add(id, "abjure_foes", "Abjure Foes", other.source.id);
            }
        }
    }
    if (a.channel_divinity[CharacterClass::cleric] > 0 && a.actions.available() && d.divine_spark)
    {
        bool undead = false;
        for (const auto &other : actors_)
        {
            const int feet = distance(a, other);
            if (other.source.id == a.source.id || other.life.dead || feet > 30)
            {
                continue;
            }
            if (other.source.side != a.source.side && other.life.hp > 0 &&
                def(other).creature_type == "undead")
            {
                undead = true;
            }
            // Divine Spark heals any wounded creature or strikes any creature
            // (CLOSE-1).
            if (!can_see(a, other))
            {
                continue;
            }
            if (other.life.hp < max_hp(other) && !detail::healing_blocked(other.effects))
            {
                add(id, "divine_spark", "Divine Spark", other.source.id);
            }
            add(id, "divine_spark_radiant", "Divine Spark (Radiant)", other.source.id);
        }
        if (undead)
        {
            add(id, "turn_undead", "Turn Undead", id);
        }
        // Preserve Life (Life Domain) needs a Bloodied ally within 30 feet.
        if (d.life_domain && std::any_of(actors_.begin(), actors_.end(),
                                         [&](const auto &other)
                                         {
                                             return other.source.side == a.source.side &&
                                                    !other.life.dead &&
                                                    other.life.hp * 2 <= max_hp(other) &&
                                                    distance(a, other) <= 30;
                                         }))
        {
            add(id, "preserve_life", "Preserve Life", id);
        }
    }
    // A creature caught by Ensnaring Strike or Entangle may spend its Action to
    // break free.
    if (a.actions.available() && detail::restrained(a.effects) &&
        std::any_of(a.effects.active.begin(), a.effects.active.end(),
                    [](const auto &e)
                    {
                        return e.kind == detail::EffectKind::ensnaring_strike ||
                               e.kind == detail::EffectKind::entangle ||
                               e.kind == detail::EffectKind::webbed ||
                               e.kind == detail::EffectKind::tentacled;
                    }))
    {
        add(id, "escape",
            detail::has_effect(a.effects, detail::EffectKind::webbed)      ? "Escape the webs"
            : detail::has_effect(a.effects, detail::EffectKind::tentacled) ? "Escape the tentacles"
                                                                           : "Escape the vines",
            id);
    }
    // Sleep ends when someone within 5 feet spends an Action to shake the
    // sleeper; Hypnotic Pattern too, for anyone else.
    if (a.actions.available())
    {
        for (const auto &other : actors_)
        {
            if (other.source.id != a.source.id && distance(a, other) <= 5 &&
                ((other.source.side == a.source.side &&
                  (detail::has_effect(other.effects, detail::EffectKind::drowsy) ||
                   detail::has_effect(other.effects, detail::EffectKind::asleep) ||
                   detail::has_effect(other.effects, detail::EffectKind::knocked_out))) ||
                 detail::has_effect(other.effects, detail::EffectKind::entranced)))
            {
                add(id, "shake_awake", "Shake awake", other.source.id);
            }
        }
    }
    // Martial Arts: an Unarmed Strike as a Bonus Action.
    if (a.bonus && d.martial_arts)
    {
        for (const auto &other : actors_)
        {
            if (other.source.id != a.source.id && !other.life.dead && distance(a, other) <= 5)
            {
                add(id, "martial_arts", "Unarmed Strike", other.source.id);
            }
        }
    }
    // Font of Magic: a Bonus Action turns Sorcery Points into a slot; a slot
    // becomes Sorcery Points without an action.
    if (d.sorcery_points)
    {
        // A slot of level 1 to 5, from Sorcerer level 2, 3, 5, 7 and 9.
        for (unsigned level = 1; level <= 5; ++level)
        {
            if (a.bonus && d.class_levels[CharacterClass::sorcerer] >= font_of_magic_level[level] &&
                a.lay_on_hands[CharacterClass::sorcerer] >= font_of_magic_cost[level] &&
                a.slots[level] < slot_room(d, level))
            {
                add(id, "create_slot_" + std::to_string(level),
                    "Font of Magic: create a level-" + std::to_string(level) + " slot (" +
                        std::to_string(font_of_magic_cost[level]) + " Sorcery Points)");
            }
        }
        for (unsigned level = 1; level <= max_slot_level; ++level)
        {
            if (a.slots[level] > 0 && a.lay_on_hands[CharacterClass::sorcerer] < d.sorcery_points)
            {
                add(id, "convert_slot_" + std::to_string(level),
                    "Font of Magic: a level-" + std::to_string(level) + " slot into " +
                        std::to_string(level) +
                        (level == 1 ? " Sorcery Point" : " Sorcery Points"));
            }
        }
    }
    // Metamagic: ready an affordable option for the next spell this turn, no
    // action; the Sorcery Points are spent when a spell it changes is cast.
    // Sorcery Incarnate: a second option while Innate Sorcery is active.
    if (!d.metamagic.empty())
    {
        const auto ready = readied_metamagic(a);
        int committed = 0;
        for (const auto &r : ready)
        {
            committed += metamagic_cost(r.option);
        }
        if (!ready.empty())
        {
            add(id, "metamagic_cancel", "Metamagic: cancel");
        }
        if (ready.size() < metamagic_room(a))
        {
            for (const auto &known : d.metamagic)
            {
                const auto index = static_cast<std::size_t>(
                    std::find(metamagic_ids.begin(), metamagic_ids.end(), known) -
                    metamagic_ids.begin());
                const auto option = static_cast<Metamagic>(index);
                const int cost = metamagic_cost(option);
                // Subtle Spell retired with spell components (CLASS-11).
                if (a.lay_on_hands[CharacterClass::sorcerer] < committed + cost ||
                    option == Metamagic::subtle ||
                    std::any_of(
                        ready.begin(), ready.end(),
                        [&](const auto &r)
                        {
                            return r.option == option;
                        }))
                {
                    continue;
                }
                const std::string points = " (" + std::to_string(cost) +
                                           (cost == 1 ? " Sorcery Point)" : " Sorcery Points)");
                const std::string label(metamagic_labels[index]);
                if (option != Metamagic::transmuted)
                {
                    add(id, "metamagic_" + known, "Metamagic: " + label + points);
                }
                else
                {
                    for (const auto type : transmuted_types)
                    {
                        add(id, "metamagic_transmuted_" + std::string(type),
                            "Metamagic: " + label + " to " +
                                std::string(detail::damage_name(detail::damage_type(type))) +
                                points);
                    }
                }
            }
        }
    }
    // Font of Inspiration (Bard 5): a spell slot, no action, gives back a
    // Bardic Inspiration use.
    if (d.font_of_inspiration && a.arcane[CharacterClass::bard] < d.bardic_inspiration)
    {
        for (unsigned level = 1; level <= max_slot_level; ++level)
        {
            if (a.slots[level] > 0)
            {
                add(id, "font_of_inspiration_" + std::to_string(level),
                    "Font of Inspiration (a level-" + std::to_string(level) + " slot)");
            }
        }
    }
    // Bardic Inspiration: a Bonus Action gives an ally within 60 feet the die.
    if (a.bonus && d.bardic_inspiration && a.arcane[CharacterClass::bard] > 0)
    {
        for (const auto &other : actors_)
        {
            if (other.source.side == a.source.side && other.source.id != id && !other.life.dead &&
                distance(a, other) <= 60 &&
                !detail::has_effect(other.effects, detail::EffectKind::inspired))
            {
                add(id, "bardic_inspiration", "Bardic Inspiration", other.source.id);
            }
        }
    }
    // Innate Sorcery: a Bonus Action, twice per Long Rest.
    if (a.bonus && d.innate_sorcery && a.free_casts[CharacterClass::sorcerer] > 0 &&
        !detail::has_effect(a.effects, detail::EffectKind::innate_sorcery))
    {
        add(id, "innate_sorcery", "Innate Sorcery");
    }
    // Sorcery Incarnate (Sorcerer 7): with no use left, 2 Sorcery Points.
    else if (a.bonus && d.sorcery_incarnate && a.lay_on_hands[CharacterClass::sorcerer] >= 2 &&
             !detail::has_effect(a.effects, detail::EffectKind::innate_sorcery))
    {
        add(id, "innate_sorcery_points", "Innate Sorcery (2 Sorcery Points)");
    }
    // A Basilisk's Petrifying Gaze, while it is charged.
    if (a.bonus && d.gaze_dc && trait_uses_left(a) > 0)
    {
        commands.push_back(
            {revision_, id, 0, "petrifying_gaze", "Petrifying Gaze", Cell{}, 0, true});
        commands.back().spell = spell_facts(a, *detail::find_spell("petrifying_gaze"));
    }
    // A monster's breath weapon, while it is charged.
    if (!d.monster_breath.empty() && trait_uses_left(a) > 0 && a.actions.available())
    {
        const auto &breath = *detail::find_spell(d.monster_breath);
        commands.push_back(
            {revision_, id, 0, d.monster_breath, std::string(breath.label), Cell{}, 0, true});
        commands.back().spell = spell_facts(a, breath);
    }
    // A Wraith's Create Specter, while it controls fewer than its limit.
    if (d.specter_limit && a.actions.available() && specters_of(a) < d.specter_limit)
    {
        for (const auto &corpse : actors_)
        {
            if (specter_corpse(a, corpse))
            {
                add(id, "create_specter", "Create Specter", corpse.source.id);
            }
        }
    }
    // Dragon Wings (Sorcerer 14): once per Long Rest, or for 3 Sorcery Points.
    if (a.bonus && d.dragon_wings &&
        !detail::has_effect(a.effects, detail::EffectKind::dragon_wings))
    {
        if (!detail::has_effect(a.effects, detail::EffectKind::wings_used))
        {
            add(id, "dragon_wings", "Dragon Wings");
        }
        else if (a.lay_on_hands[CharacterClass::sorcerer] >= 3)
        {
            add(id, "dragon_wings_points", "Dragon Wings (3 Sorcery Points)");
        }
    }
    // Monk's Focus: Patient Defense and Step of the Wind, free or with a Focus
    // Point, and Flurry of Blows for a Focus Point.
    // Heightened Focus: Step of the Wind with Focus may carry an ally beside
    // the Monk, Large or smaller, standing or not.
    const auto offer_carry = [&]
    {
        if (!d.heightened_focus || a.surges[CharacterClass::monk] <= 0)
        {
            return;
        }
        for (const auto &ally : actors_)
        {
            if (ally.source.side == a.source.side && ally.source.id != id && !ally.life.dead &&
                def(ally).size <= 3 && distance(a, ally) <= 5)
            {
                add(id, "step_of_the_wind_carry",
                    "Step of the Wind: Disengage and Dash, carrying an ally (1 Focus)",
                    ally.source.id);
            }
        }
    };
    // Fleet Step: Step of the Wind right after another Bonus Action.
    if (!a.bonus && a.fleet_step)
    {
        add(id, "step_of_the_wind", "Step of the Wind: Dash");
        if (a.surges[CharacterClass::monk] > 0)
        {
            add(id, "step_of_the_wind_focus", "Step of the Wind: Disengage and Dash (1 Focus)");
        }
        offer_carry();
    }
    if (a.bonus && d.focus)
    {
        add(id, "patient_defense", "Patient Defense: Disengage");
        add(id, "step_of_the_wind", "Step of the Wind: Dash");
        offer_carry();
        if (a.surges[CharacterClass::monk] > 0)
        {
            add(id, "patient_defense_focus", "Patient Defense: Disengage and Dodge (1 Focus)");
            add(id, "step_of_the_wind_focus", "Step of the Wind: Disengage and Dash (1 Focus)");
            for (const auto &other : actors_)
            {
                if (other.source.id != a.source.id && !other.life.dead && distance(a, other) <= 5)
                {
                    add(id, "flurry_of_blows", "Flurry of Blows", other.source.id);
                    // Open Hand Technique, chosen for the whole Flurry.
                    if (d.open_hand)
                    {
                        add(id, "flurry_addle", "Flurry of Blows: Addle", other.source.id);
                        add(id, "flurry_push", "Flurry of Blows: Push", other.source.id);
                        add(id, "flurry_topple", "Flurry of Blows: Topple", other.source.id);
                    }
                    // Stunning Strike on a Flurry hit costs one more Focus Point.
                    if (d.stunning_strike && d.martial_arts && !a.stun_used &&
                        a.surges[CharacterClass::monk] > 1)
                    {
                        add(id, "flurry_stun", "Flurry of Blows: Stunning Strike", other.source.id);
                    }
                }
            }
        }
    }
    // Animal Shapes: a Bonus Action leaves the form.
    if (a.bonus && detail::has_effect(a.effects, detail::EffectKind::animal_shaped))
    {
        add(id, "leave_animal_shape", "Leave the animal shape");
    }
    // Wild Resurgence (Druid 5): with no Wild Shape use left, a spell slot, no
    // action, once a turn, gives one; once per Long Rest a use gives a
    // level-1 slot.
    if (d.wild_resurgence)
    {
        if (!a.channel_divinity[CharacterClass::druid] &&
            std::none_of(a.effects.active.begin(), a.effects.active.end(),
                         [](const auto &e)
                         {
                             return e.kind == detail::EffectKind::resurged && !e.dc;
                         }))
        {
            for (unsigned level = 1; level <= max_slot_level; ++level)
            {
                if (a.slots[level] > 0)
                {
                    add(id, "wild_resurgence_" + std::to_string(level),
                        "Wild Resurgence: a level-" + std::to_string(level) +
                            " slot for Wild Shape");
                }
            }
        }
        if (a.channel_divinity[CharacterClass::druid] > 0 && a.slots[1] < d.slots[1] &&
            std::none_of(
                a.effects.active.begin(), a.effects.active.end(),
                [](const auto &e)
                {
                    return e.kind == detail::EffectKind::resurged && e.dc;
                }))
        {
            add(id, "wild_resurgence_slot", "Wild Resurgence: Wild Shape for a level-1 slot");
        }
    }
    // Wild Shape: a Bonus Action takes a Beast form or leaves it.
    if (a.bonus && d.wild_shapes)
    {
        if (a.channel_divinity[CharacterClass::druid] > 0)
        {
            for (const auto &form : detail::beast_forms)
            {
                if (d.class_levels[CharacterClass::druid] >= static_cast<int>(form.level))
                {
                    add(id, "wild_shape_" + std::string(form.key),
                        "Wild Shape: " + std::string(form.label));
                }
            }
        }
        if (a.form)
        {
            add(id, "leave_wild_shape", "Leave Wild Shape");
        }
    }
    // Flame Tongue: a Bonus Action lights the blade, and another douses it.
    if (a.bonus && d.flame_tongue && a.flame_lit)
    {
        add(id, "douse_flame_tongue", "Flame Tongue: douse");
    }
    else if (a.bonus && d.flame_tongue)
    {
        add(id, "ignite_flame_tongue", "Flame Tongue: light");
    }
    // Basic Poison: a Bonus Action coats the weapon with a carried vial.
    if (a.bonus && basic_poison_left(a) && !a.coated_until_round)
    {
        add(id, "coat_weapon", "Basic Poison: coat weapon");
    }
    // Rage: a Bonus Action outside Heavy armor; on a later turn a Bonus Action
    // extends it.
    if (a.bonus && d.rages)
    {
        const auto *rage = rage_of(a);
        if (!rage && a.channel_divinity[CharacterClass::barbarian] > 0 && !d.heavy_armor)
        {
            add(id, "rage", "Rage");
        }
        else if (rage && !d.persistent_rage && rage->remaining_ms <= detail::round_ms)
        {
            add(id, "extend_rage", "Extend Rage");
        }
    }
    if (a.bonus && d.intimidating_presence)
    {
        if (a.free_casts[CharacterClass::barbarian] > 0)
        {
            add(id, "intimidating_presence", "Intimidating Presence");
        }
        else if (a.channel_divinity[CharacterClass::barbarian] > 0)
        {
            add(id, "intimidating_presence_rage", "Intimidating Presence (1 Rage)");
        }
    }
    // Expeditious Retreat: Dash as a Bonus Action while it lasts.
    if (a.bonus && detail::has_effect(a.effects, detail::EffectKind::expeditious_retreat))
    {
        add(id, "retreat_dash", "Expeditious Retreat: Dash");
    }
    // Conjure Woodland Beings: Disengage as a Bonus Action while it lasts.
    if (a.bonus && !a.disengaged &&
        std::any_of(zones_.begin(), zones_.end(),
                    [&](const auto &zone)
                    {
                        return zone.kind == ZoneKind::beings && zone.caster == id;
                    }))
    {
        add(id, "woodland_disengage", "Disengage (Conjure Woodland Beings)");
    }
    // Nature's Veil (Ranger 14): a Bonus Action makes the Ranger Invisible
    // until the end of its next turn.
    if (a.bonus && d.natures_veil && a.lay_on_hands[CharacterClass::ranger] > 0 &&
        !detail::has_effect(a.effects, detail::EffectKind::veiled) && detail::can_apply(a.effects))
    {
        add(id, "natures_veil", "Nature's Veil", id);
    }
    // Flaming Sphere: a Bonus Action rolls it up to 30 feet into a creature's
    // space, where it stops beside the creature and burns it.
    for (const auto &zone : zones_)
    {
        if (zone.kind == ZoneKind::flaming_sphere && zone.caster == id && a.bonus)
        {
            for (const auto &other : actors_)
            {
                if (other.life.hp > 0 && !other.life.dead && other.source.id != id &&
                    distance(zone.cells.front(), other) <= 30)
                {
                    add(id, "roll_flaming_sphere", "Roll Flaming Sphere", other.source.id);
                }
            }
        }
    }
    // Compulsion: a Bonus Action names the way its charmed creatures go next.
    if (a.bonus &&
        std::any_of(actors_.begin(), actors_.end(),
                    [&](const auto &other)
                    {
                        return !other.life.dead &&
                               std::any_of(other.effects.active.begin(), other.effects.active.end(),
                                           [&](const auto &e)
                                           {
                                               return e.kind == detail::EffectKind::compelled &&
                                                      e.source_actor == id &&
                                                      e.source_scope == scope_;
                                           });
                    }))
    {
        commands.push_back({revision_, id, 0, "compulsion_heading", "Compulsion: name a direction",
                            Cell{}, 0, true});
    }
    // Heat Metal: a Bonus Action on later turns heats the metal again.
    if (a.bonus)
    {
        for (const auto &other : actors_)
        {
            if (!other.life.dead && other.life.hp > 0 && distance(a, other) <= 60 &&
                std::any_of(other.effects.active.begin(), other.effects.active.end(),
                            [&](const auto &e)
                            {
                                return e.kind == detail::EffectKind::heated &&
                                       e.source_actor == id && !e.dc;
                            }))
            {
                add(id, "heat_metal_again", "Heat Metal again", other.source.id);
            }
        }
    }
    // Moonbeam: a Magic action moves the beam up to 60 feet, here onto a creature.
    for (const auto &zone : zones_)
    {
        if (zone.kind == ZoneKind::moonbeam && zone.caster == id && a.actions.available(true))
        {
            for (const auto &other : actors_)
            {
                if (other.life.hp > 0 && !other.life.dead && !in_zone(zone, other) &&
                    distance(zone.cells.front(), other) <= 60)
                {
                    add(id, "move_moonbeam", "Move Moonbeam", other.source.id);
                }
            }
        }
    }
    // Call Lightning: a Magic action calls another bolt while it lasts.
    if (a.actions.available(true) && std::any_of(zones_.begin(), zones_.end(),
                                                 [&](const auto &zone)
                                                 {
                                                     return zone.kind == ZoneKind::storm &&
                                                            zone.caster == id;
                                                 }))
    {
        commands.push_back({revision_, id, 0, "call_lightning_again",
                            "Call Lightning: another bolt", Cell{}, 0, true});
    }
    // Sunbeam: a Magic action fires a new Line while it lasts.
    if (a.actions.available(true) && std::any_of(zones_.begin(), zones_.end(),
                                                 [&](const auto &zone)
                                                 {
                                                     return zone.kind == ZoneKind::sunbeam &&
                                                            zone.caster == id;
                                                 }))
    {
        commands.push_back(
            {revision_, id, 0, "sunbeam_line", "Sunbeam: a new Line", Cell{}, 0, true});
    }
    // Conjure Animals: once the caster has moved this turn, its pack may move
    // too, here beside a creature it can see.
    for (const auto &zone : zones_)
    {
        if (zone.kind == ZoneKind::animals && zone.caster == id && a.moved &&
            !detail::has_effect(a.effects, detail::EffectKind::pack_moved) &&
            detail::can_apply(a.effects))
        {
            for (const auto &other : actors_)
            {
                if (other.source.side != a.source.side && other.life.hp > 0 && !other.life.dead &&
                    can_see(a, other) && pack_destination(zone, other))
                {
                    add(id, "move_spirit_pack", "Move spirit pack", other.source.id);
                }
            }
        }
    }
    // Conjure Celestial: once the caster has moved this turn, the pillar may
    // move too, onto a creature it can see.
    for (const auto &zone : zones_)
    {
        if (zone.kind == ZoneKind::pillar && zone.caster == id && a.moved &&
            !detail::has_effect(a.effects, detail::EffectKind::pack_moved) &&
            detail::can_apply(a.effects))
        {
            for (const auto &other : actors_)
            {
                if (pillar_reaches(zone, other))
                {
                    add(id, "move_pillar", "Move the pillar of light", other.source.id);
                }
            }
        }
    }
    // Eyebite: a Magic action on later turns targets another creature.
    if (detail::has_effect(a.effects, detail::EffectKind::eyebiting) && a.actions.available(true))
    {
        for (const auto &other : actors_)
        {
            if (other.source.id != a.source.id && !other.life.dead && !absent(other) &&
                !sealed_off(other) && can_see(a, other) && distance(a, other) <= 60 &&
                !std::any_of(other.effects.active.begin(), other.effects.active.end(),
                             [&](const auto &e)
                             {
                                 return e.source_actor == id && e.source_scope == scope_ &&
                                        (e.kind == detail::EffectKind::eye_resisted ||
                                         e.kind == detail::EffectKind::eye_panic ||
                                         e.kind == detail::EffectKind::sickened ||
                                         e.kind == detail::EffectKind::asleep);
                             }))
            {
                for (const auto effect : {"asleep", "panicked", "sickened"})
                {
                    add(id, "evil_eye_" + std::string(effect), "Eyebite: " + std::string(effect),
                        other.source.id);
                }
            }
        }
    }
    // Irresistible Dance: the dancer's action repeats the save.
    if (detail::has_effect(a.effects, detail::EffectKind::dancing) && a.actions.available())
    {
        add(id, "stop_dancing", "Try to stop dancing", id);
    }
    // Magic Jar: the possessing soul's Magic action returns it to its body.
    if (const auto *boss = controller(a);
        boss && a.actions.available(true) &&
        detail::has_effect(boss->effects, detail::EffectKind::jarred))
    {
        add(id, "leave_host", "Return to your body (Magic Jar)", id);
    }
    // Arcane Hand: as it appears, then with a Bonus Action, the hand moves up
    // to 60 feet and strikes or pushes a creature beside it.
    for (const auto &zone : zones_)
    {
        if (zone.kind == ZoneKind::hand && zone.caster == id &&
            (a.bonus || detail::has_effect(a.effects, detail::EffectKind::hand_ready)))
        {
            for (const auto &other : actors_)
            {
                if (other.source.id != id && other.life.hp > 0 && !other.life.dead &&
                    !absent(other) && !sealed_off(other) && can_see(a, other) &&
                    distance(zone.cells.front(), other) <= 65)
                {
                    if (other.source.side != a.source.side)
                    {
                        add(id, "hand_fist", "Arcane Hand: Clenched Fist", other.source.id);
                    }
                    if (def(other).size <= 4)
                    {
                        add(id, "hand_push", "Arcane Hand: Forceful Hand", other.source.id);
                    }
                }
            }
        }
    }
    // Telekinesis: a Magic action seizes a creature again, or another.
    if (detail::has_effect(a.effects, detail::EffectKind::telekinetic) && a.actions.available(true))
    {
        for (const auto &other : actors_)
        {
            if (other.source.id != id && other.life.hp > 0 && !other.life.dead && !absent(other) &&
                !sealed_off(other) && def(other).size <= 4 && can_see(a, other) &&
                distance(a, other) <= 60)
            {
                commands.push_back({revision_, id, other.source.id, "telekinesis_again",
                                    "Telekinesis", Cell{}, 0, false});
            }
        }
    }
    // Summon Dragon: its spirit breathes once a turn besides its Rends.
    if (detail::has_effect(a.effects, detail::EffectKind::spirit_breath) &&
        !detail::has_effect(a.effects, detail::EffectKind::breathed) && conscious(a))
    {
        commands.push_back({revision_, id, 0, "spirit_breath", "Breath Weapon", Cell{}, 0, true});
    }
    // Mislead: a Magic action moves the double up to twice the Speed, beside
    // a creature the caster can see.
    for (const auto &twin : actors_)
    {
        if (const auto illusion =
                std::find_if(twin.effects.active.begin(), twin.effects.active.end(),
                             [&](const auto &e)
                             {
                                 return e.kind == detail::EffectKind::illusory &&
                                        e.source_actor == id && e.source_scope == scope_;
                             });
            illusion != twin.effects.active.end() && a.actions.available(true))
        {
            for (const auto &other : actors_)
            {
                if (other.source.id != id && other.source.id != twin.source.id &&
                    !other.life.dead && !absent(other) && can_see(a, other) &&
                    distance(twin, other) <= 2 * d.speed + 5)
                {
                    add(id, "move_double", "Move the double", other.source.id);
                }
            }
        }
    }
    // Faithful Hound: a Magic action moves it up to 50 feet, beside an enemy
    // the caster can see (LEVELS-4).
    for (const auto &zone : zones_)
    {
        if (zone.kind == ZoneKind::hound && zone.caster == id && a.actions.available(true))
        {
            for (const auto &other : actors_)
            {
                if (other.source.side != a.source.side && other.life.hp > 0 && !other.life.dead &&
                    !absent(other) && can_see(a, other) &&
                    distance(zone.cells.front(), other) <= 55 &&
                    distance(zone.cells.front(), other) > 5)
                {
                    add(id, "move_hound", "Move Faithful Hound", other.source.id);
                }
            }
        }
    }
    // Vampiric Touch: a Magic action touches again while it lasts.
    if (detail::has_effect(a.effects, detail::EffectKind::vampiric) && a.actions.available(true))
    {
        for (const auto &other : actors_)
        {
            if (other.source.id != a.source.id && !other.life.dead && !absent(other) &&
                distance(a, other) <= d.reach)
            {
                add(id, "vampiric_touch_again", "Vampiric Touch", other.source.id);
            }
        }
    }
    // Improved Blessed Strikes (Cleric 14): after a Cleric cantrip's damage,
    // Temporary Hit Points for the Cleric or a creature within 60 feet.
    if (detail::has_effect(a.effects, detail::EffectKind::vitality))
    {
        for (const auto &other : actors_)
        {
            if (!other.life.dead && !absent(other) && distance(a, other) <= 60)
            {
                add(id, "potent_vitality", "Potent Spellcasting: Temporary Hit Points",
                    other.source.id);
            }
        }
    }
    // Spiritual Weapon: on later turns a Bonus Action moves the force up to 20
    // feet and attacks a creature within 5 feet of it.
    if (const auto *force = spiritual_weapon(a); force && a.bonus)
    {
        for (const auto &other : actors_)
        {
            if (other.source.id != a.source.id && !other.life.dead &&
                distance(force->cells.front(), other) <= 25)
            {
                add(id, "spiritual_weapon_strike", "Spiritual Weapon attack", other.source.id);
            }
        }
    }
    // Arcane Sword: on later turns a Bonus Action moves the sword up to 30
    // feet and attacks a creature within 5 feet of it.
    for (const auto &zone : zones_)
    {
        if (zone.kind == ZoneKind::sword && zone.caster == id && !zone.dealt && a.bonus)
        {
            for (const auto &other : actors_)
            {
                if (other.source.id != a.source.id && !other.life.dead && !absent(other) &&
                    distance(zone.cells.front(), other) <= 35)
                {
                    add(id, "arcane_sword_strike", "Arcane Sword attack", other.source.id);
                }
            }
        }
    }
    // Conjure Fey: on later turns a Bonus Action teleports the spirit 30 feet
    // and attacks a creature within 5 feet of it.
    for (const auto &zone : zones_)
    {
        if (zone.kind == ZoneKind::fey && zone.caster == id && !zone.dealt && a.bonus)
        {
            for (const auto &other : actors_)
            {
                if (other.source.id != a.source.id && !other.life.dead && !absent(other) &&
                    distance(zone.cells.front(), other) <= 35)
                {
                    add(id, "conjure_fey_strike", "Fey spirit attack", other.source.id);
                }
            }
        }
    }
    // Nature's Sanctuary (Land 14): a Magic action and a Wild Shape use raise
    // it; a Bonus Action moves it 60 feet.
    if (d.natures_sanctuary && a.channel_divinity[CharacterClass::druid] > 0 &&
        a.actions.available(true))
    {
        commands.push_back(
            {revision_, id, 0, "natures_sanctuary", "Nature's Sanctuary", Cell{}, 0, true});
    }
    if (a.bonus && std::any_of(zones_.begin(), zones_.end(),
                               [&](const auto &zone)
                               {
                                   return zone.kind == ZoneKind::sanctuary && zone.caster == id;
                               }))
    {
        commands.push_back(
            {revision_, id, 0, "move_sanctuary", "Move Nature's Sanctuary", Cell{}, 0, true});
    }
    // Delayed Blast Fireball: ending the spell, which takes no action, bursts
    // the bead.
    if (std::any_of(zones_.begin(), zones_.end(),
                    [&](const auto &zone)
                    {
                        return zone.kind == ZoneKind::bead && zone.caster == id;
                    }))
    {
        add(id, "detonate", "Burst the bead (Delayed Blast Fireball)");
    }
    // Overchannel (Wizard 14): readied for the next spell, no action; free
    // the first time each Long Rest, then it hurts the Wizard.
    if (d.overchannel && !detail::has_effect(a.effects, detail::EffectKind::overchannel))
    {
        const auto used = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                       [](const auto &e)
                                       {
                                           return e.kind == detail::EffectKind::overchanneled;
                                       });
        if (used == a.effects.active.end())
        {
            add(id, "overchannel", "Overchannel (free)");
        }
        else
        {
            add(id, "overchannel",
                "Overchannel (" + std::to_string(used->dc + 1) +
                    "d12 Necrotic per slot level to you)");
        }
    }
    // Superior Hunter's Prey (Ranger 11): once per turn, after damaging its
    // quarry, the mark's damage strikes another creature within 30 feet of it.
    if (d.superior_prey && a.prey_origin && !a.prey_used)
    {
        const auto &origin = actor(a.prey_origin);
        for (const auto &other : actors_)
        {
            if (other.source.id != origin.source.id && other.source.id != a.source.id &&
                !other.life.dead && can_see(a, other) && distance(origin, other) <= 30)
            {
                add(id, "superior_hunters_prey", "Superior Hunter's Prey", other.source.id);
            }
        }
    }
    // Horde Breaker: once per turn, after a weapon attack, another creature
    // within 5 feet of the first target and within the weapon's reach or range.
    if (d.horde_breaker && a.horde_origin && !a.horde_used)
    {
        const auto &origin = actor(a.horde_origin);
        for (const auto &other : actors_)
        {
            const int feet = distance(a, other);
            if (other.source.id != origin.source.id && other.source.id != a.source.id &&
                !other.life.dead && distance(origin, other) <= 5 &&
                (feet <= d.reach || (d.range > 0 && feet <= d.long_range)))
            {
                add(id, "horde_breaker", "Horde Breaker", other.source.id);
            }
        }
    }
    // Favored Enemy casts Hunter's Mark without a slot, while no living quarry
    // carries the mark already; a dropped quarry's mark moves for a Bonus Action.
    const bool marking =
        std::any_of(actors_.begin(), actors_.end(),
                    [&](const auto &other)
                    {
                        return other.life.hp > 0 && !other.life.dead && marked_by(other, a);
                    });
    const bool free_mark = a.bonus && a.free_casts[CharacterClass::ranger] > 0 && d.favored_enemy &&
                           !marking && detail::knows_spell(d.spells, "hunters_mark") &&
                           !d.str_dex_disadvantage;
    const bool moving_mark = a.bonus && mark_can_move(a);
    // Hex moves to a new creature as a Bonus Action once its first drops.
    if (a.bonus && hex_can_move(a))
    {
        for (const auto &other : actors_)
        {
            if (other.source.side != a.source.side && other.life.hp > 0 && !other.life.dead &&
                distance(a, other) <= 90 && can_see(a, other))
            {
                add(id, "hex_move", "Move Hex", other.source.id);
            }
        }
    }
    if (free_mark || moving_mark)
    {
        for (const auto &other : actors_)
        {
            if (other.source.side != a.source.side && other.life.hp > 0 && !other.life.dead &&
                distance(a, other) <= 90 && can_see(a, other) && !marked_by(other, a))
            {
                if (free_mark)
                {
                    add(id, "hunters_mark_free", "Hunter's Mark (Favored Enemy)", other.source.id);
                }
                if (moving_mark)
                {
                    add(id, "hunters_mark_move", "Move Hunter's Mark", other.source.id);
                }
            }
        }
    }
    // Sacred Weapon comes with the Attack action, so it is offered while the
    // Action is unspent and costs only a Channel Divinity use. The player picks
    // the weapon's own damage or Radiant when imbuing it (SACRED-1).
    if (d.sacred_weapon && a.channel_divinity[CharacterClass::cleric] > 0 &&
        a.actions.available() && d.melee.count &&
        !detail::has_effect(a.effects, detail::EffectKind::sacred_weapon))
    {
        add(id, "sacred_weapon", "Sacred Weapon", id);
        add(id, "sacred_weapon_radiant", "Sacred Weapon (Radiant)", id);
    }
    if (a.bonus && d.aggressive && speed > 0 && enemy_in_sight(a))
    {
        add(id, "aggressive", "Aggressive");
    }
    if (a.bonus && d.bonus_charge && speed > 0 && enemy_in_sight(a))
    {
        add(id, "bonus_charge", "Charge");
    }
    // A Rampage's Rend at a creature within reach.
    if (detail::has_effect(a.effects, detail::EffectKind::rampaging) && !attached_host(a))
    {
        for (const auto &other : actors_)
        {
            if (!other.life.dead && other.source.id != a.source.id && line_of_sight(a, other) &&
                distance(a, other) <= d.reach)
            {
                add(id, "rampage", "Rampage", other.source.id);
            }
        }
    }
    if (a.bonus && a.winds > 0 && a.life.hp < max_hp(a))
    {
        add(id, "second_wind", "Second Wind", id);
    }
    // Wholeness of Body: a Bonus Action heals the Martial Arts die + Wisdom.
    if (a.bonus && d.wholeness && a.free_casts[CharacterClass::monk] > 0 && a.life.hp < max_hp(a))
    {
        add(id, "wholeness_of_body", "Wholeness of Body", id);
    }
    // Tireless (Ranger 10): a Magic action gives 1d8 + Wisdom Temporary Hit Points.
    if (d.tireless && a.arcane[CharacterClass::ranger] > 0 && a.actions.available(true))
    {
        add(id, "tireless", "Tireless", id);
    }
    // Lay On Hands: touch yourself or any adjacent wounded creature whose healing
    // can take effect, including one at 0 Hit Points.
    if (a.bonus && d.lay_on_hands && a.lay_on_hands[CharacterClass::paladin] > 0)
    {
        for (const auto &other : actors_)
        {
            if (!other.life.dead && other.life.hp < max_hp(other) &&
                !detail::healing_blocked(other.effects) && distance(a, other) <= 5)
            {
                add(id, "lay_on_hands", "Lay On Hands", other.source.id);
            }
        }
    }
    // Lay On Hands can instead spend 5 points to end Poisoned (SRD 5.2.1 p. 61).
    if (a.bonus && d.lay_on_hands && a.lay_on_hands[CharacterClass::paladin] >= 5)
    {
        for (const auto &other : actors_)
        {
            if (!other.life.dead && has_condition(other, "poisoned") && distance(a, other) <= 5)
            {
                add(id, "lay_on_hands_cure", "Lay On Hands: cure Poisoned", other.source.id);
            }
        }
    }
    // Restoring Touch (Paladin 14): 5 points end each of several conditions.
    if (a.bonus && d.restoring_touch && a.lay_on_hands[CharacterClass::paladin] >= 5)
    {
        for (const auto &other : actors_)
        {
            if (!other.life.dead && touch_restorable(other) && distance(a, other) <= 5)
            {
                add(id, "lay_on_hands_restore", "Lay On Hands: Restoring Touch (5 per condition)",
                    other.source.id);
            }
        }
    }
    // Cloud's Jaunt: a Bonus Action teleport, aimed like Misty Step.
    if (a.bonus && d.giant == GiantBoon::cloud && trait_uses_left(a) > 0)
    {
        commands.push_back({revision_, id, 0, "clouds_jaunt", "Cloud's Jaunt", Cell{}, 0, true});
    }
    // Divine Strike (Blessed Strikes, Cleric 7): once on each of its turns,
    // after a weapon hit, 1d8 more Radiant or Necrotic damage, the player's choice.
    if (d.divine_strike && a.smite_target && !a.strike_used && actor(a.smite_target).life.hp > 0)
    {
        add(id, "divine_strike_radiant", "Divine Strike (Radiant)", a.smite_target);
        add(id, "divine_strike_necrotic", "Divine Strike (Necrotic)", a.smite_target);
    }
    // Eldritch Smite and Lifedrinker (Warlock invocations): once on each of
    // its turns, right after a pact weapon hit.
    if (a.smite_target && a.smite_melee && d.pact_of_the_blade && actor(a.smite_target).life.hp > 0)
    {
        if (d.eldritch_smite && !a.smite_used &&
            a.slots[static_cast<std::size_t>(d.pact_slot_level)] > 0)
        {
            add(id, "eldritch_smite", "Eldritch Smite (a Pact Magic slot)", a.smite_target);
        }
        if (d.lifedrinker && !a.drink_used)
        {
            for (const std::string type : {"necrotic", "psychic", "radiant"})
            {
                const std::string name(detail::damage_name(detail::damage_type(type)));
                add(id, "lifedrinker_" + type, "Lifedrinker (" + name + ")", a.smite_target);
                if (a.hit_dice > 0 && a.life.hp < max_hp(a))
                {
                    add(id, "lifedrinker_" + type + "_heal",
                        "Lifedrinker (" + name + ", spend a Hit Point Die)", a.smite_target);
                }
            }
        }
    }
    // Hurl Through Hell (Fiend 14): once on each of its turns, right after an
    // attack roll hits; once per Long Rest, then for a Pact Magic slot.
    if (d.hurl_through_hell && a.smite_target && !a.hurl_used &&
        actor(a.smite_target).life.hp > 0 && !absent(actor(a.smite_target)))
    {
        if (!detail::has_effect(a.effects, detail::EffectKind::hurl_used))
        {
            add(id, "hurl_through_hell", "Hurl Through Hell", a.smite_target);
        }
        else if (a.slots[static_cast<std::size_t>(d.pact_slot_level)] > 0)
        {
            add(id, "hurl_through_hell_slot", "Hurl Through Hell (a Pact Magic slot)",
                a.smite_target);
        }
    }
    // Primal Strike (Elemental Fury, Druid 7): once on each of its turns, after
    // a weapon or Beast form hit, 1d8 (2d8 from 15) of a chosen element.
    if (d.primal_strike && a.smite_target && !a.strike_used && actor(a.smite_target).life.hp > 0)
    {
        for (const std::string type : {"cold", "fire", "lightning", "thunder"})
        {
            add(id, "primal_strike_" + type,
                "Primal Strike (" + std::string(detail::damage_name(detail::damage_type(type))) +
                    ")",
                a.smite_target);
        }
    }
    // Smites, right after this actor's own melee hit on a creature still standing.
    if (a.bonus && a.smite_target && actor(a.smite_target).life.hp > 0 && !d.spells.empty())
    {
        const bool melee = a.smite_melee;
        // A smite from each slot level cast_levels() offers.
        const auto add_smite = [&](std::string_view id_of_spell)
        {
            const auto &spell = *detail::find_spell(id_of_spell);
            if (!melee || a.spent_slot || !detail::knows_spell(d.spells, spell.id))
            {
                return;
            }
            for (const unsigned level : cast_levels(a, spell))
            {
                auto [verb, label] =
                    cast_at(spell, level, std::string(spell.id), std::string(spell.label));
                add(id, std::move(verb), std::move(label), a.smite_target);
                commands.back().spell = spell_facts(a, spell);
            }
        };
        if (melee && detail::knows_spell(d.spells, "divine_smite") &&
            a.free_casts[CharacterClass::paladin] > 0)
        {
            add(id, "divine_smite_free", "Divine Smite (Paladin's Smite)", a.smite_target);
        }
        add_smite("divine_smite");
        add_smite("searing_smite");
        add_smite("shining_smite");
        // Ensnaring Strike follows a Ranged hit too.
        if (detail::knows_spell(d.spells, "ensnaring_strike") && !a.spent_slot)
        {
            const auto &spell = *detail::find_spell("ensnaring_strike");
            for (const unsigned level : cast_levels(a, spell))
            {
                auto [verb, label] =
                    cast_at(spell, level, std::string(spell.id), std::string(spell.label));
                add(id, std::move(verb), std::move(label), a.smite_target);
                commands.back().spell = spell_facts(a, spell);
            }
        }
    }
    for (const auto &other : actors_)
    {
        const int feet = distance(a, other);
        for (const auto scope : {detail::SpellTarget::wounded_ally, detail::SpellTarget::ally,
                                 detail::SpellTarget::self, detail::SpellTarget::enemy})
        {
            offer_spells(commands, a, other, feet, scope, true);
        }
        // A spell aimed at any creature, such as a Quickened Poison Spray, needs
        // the same clear line as in the Action pass.
        if (line_of_sight(a, other))
        {
            offer_spells(commands, a, other, feet, detail::SpellTarget::any_creature, true);
        }
    }
    offer_spells(commands, a, a, 0, detail::SpellTarget::area, true);
    if (!a.light_extra && !a.light_origins.empty())
    {
        for (const auto &item : items_)
        {
            if (item.holder == id && light_eligible(a, item.id))
            {
                const auto *weapon = detail::weapon(item.definition);
                for (const auto &other : actors_)
                {
                    if (other.source.id != a.source.id && !other.life.dead &&
                        line_of_sight(a, other))
                    {
                        const auto offer = [&](const char *light, const char *nick)
                        {
                            if (a.bonus)
                            {
                                add(id, light, "Light extra attack", other.source.id);
                                commands.back().item = item.id;
                            }
                            if (nick_active_ && a.nick_origin && a.nick_origin != item.id &&
                                nick_weapon(a, item.id))
                            {
                                add(id, nick, "Nick attack", other.source.id);
                                commands.back().item = item.id;
                            }
                        };
                        const int feet = distance(a, other);
                        if (!item.stowed && !weapon->ranged && feet <= melee_reach)
                        {
                            offer("light_melee", "nick_melee");
                        }
                        if (!item.stowed && weapon->ranged && feet <= weapon->long_range)
                        {
                            offer("light_ranged", "nick_ranged");
                        }
                        if (weapon->thrown && feet <= weapon->long_range)
                        {
                            offer("light_throw", "nick_throw");
                        }
                    }
                }
            }
        }
    }
    // Cunning Strike: offered when the attack would deal Sneak Attack damage.
    const auto offer_cunning = [&](AttackRange range, bool in_reach, const Actor &other)
    {
        if (!d.cunning_strike || a.sneak_used || !in_reach ||
            !sneak_eligible(a, other, range,
                            attack_modifiers(a, other, range, AttackSource::weapon).mode()))
        {
            return;
        }
        const std::string base = range == AttackRange::melee ? "melee" : "ranged";
        const std::string label =
            range == AttackRange::melee ? "Melee attack (" : "Ranged attack (";
        const int dice = detail::sneak_attack_dice(d.sneak_level).count;
        const auto usable = [&](const CunningOption &option)
        {
            const bool devious = option.kind == CunningStrike::daze ||
                                 option.kind == CunningStrike::knock_out ||
                                 option.kind == CunningStrike::obscure;
            return option.cost <= dice && (!devious || d.devious_strikes) &&
                   (option.kind != CunningStrike::stealth || (d.supreme_sneak && hidden(a))) &&
                   (option.kind != CunningStrike::poison || carries(a, poisoners_kit));
        };
        for (std::size_t i = 0; i < cunning_strikes.size(); ++i)
        {
            const auto &first = cunning_strikes[i];
            if (!usable(first))
            {
                continue;
            }
            add(id, base + "_" + std::string(first.suffix), label + std::string(first.name) + ")",
                other.source.id);
            // Improved Cunning Strike: a second effect the dice can also pay for.
            for (std::size_t j = i + 1; d.improved_cunning && j < cunning_strikes.size(); ++j)
            {
                if (const auto &second = cunning_strikes[j];
                    usable(second) && first.cost + second.cost <= dice)
                {
                    add(id,
                        base + "_" + std::string(first.suffix) + "+" + std::string(second.suffix),
                        label + std::string(first.name) + ", " + std::string(second.name) + ")",
                        other.source.id);
                }
            }
        }
    };
    // Tactical Master: the attack with Push, Sap or Slow, other than the
    // weapon's own mastery.
    const auto offer_tactical = [&](AttackRange range, bool in_reach, const Actor &other)
    {
        const auto own = weapon_mastery(a, range);
        if (!d.tactical_master || !in_reach || own == detail::Mastery::none)
        {
            return;
        }
        const bool melee = range == AttackRange::melee;
        for (const auto &option : tactical_masteries)
        {
            if (option.kind != own)
            {
                add(id, std::string(melee ? "melee_" : "ranged_") + std::string(option.suffix),
                    std::string(melee ? option.melee_label : option.ranged_label), other.source.id);
            }
        }
    };
    // The Attack action's attacks on one creature, with the Magic actions that
    // attack (which need the Action itself); Extra Attack offers the weapon
    // attacks again after the action's first attack.
    const auto offer_weapon_attacks = [&](const Actor &other, int feet)
    {
        // Reverse Gravity: no melee attack reaches up to a creature fallen
        // upward, or down from it.
        if (aloft(a) || aloft(other))
        {
            feet = std::max(feet, 15);
        }
        // A Wild Shape form cannot wield its equipment (SRD 5.2.1), so it
        // draws no Torch or bow and throws no flask.
        const bool wields_equipment = !a.form;
        // A carried Torch is drawn as part of the attack (SRD 5.2.1: a weapon
        // can be equipped with each attack of the Attack action).
        if (wields_equipment && feet <= 5 && carries(a, "torch") && !bound_to_cursed_item(a, true))
        {
            add(id, "torch", "Torch attack", other.source.id);
        }
        // A carried bow or crossbow is drawn as part of the attack, too.
        // A cursed weapon cannot be put down to draw it.
        if (const auto bow = wields_equipment && !bound_to_cursed_item(a, true)
                                 ? carried_ranged_weapon(a)
                                 : std::string_view{};
            !bow.empty() && feet <= detail::weapon(bow)->long_range &&
            (!detail::weapon(bow)->loading || reloaded(a)))
        {
            add(id, "shoot", "Shoot", other.source.id);
        }
        for (const auto &gear : detail::thrown_gear_items)
        {
            if (wields_equipment && feet <= detail::thrown_gear_range &&
                a.thrown_gear_left[static_cast<std::size_t>(gear.effect)])
            {
                add(id, "throw_" + std::string(gear.key), "Throw " + std::string(gear.label),
                    other.source.id);
            }
        }
        if (wields_equipment && physical_inventory_)
        {
            for (const auto &item : items_)
            {
                if (item.holder == id)
                {
                    if (const auto *w = detail::weapon(item.definition);
                        w && w->thrown && feet <= w->long_range)
                    {
                        add(id, "throw", "Throw", other.source.id);
                        commands.back().item = item.id;
                        // The Javelin of Lightning's bolt, once until a Long
                        // Rest (MAGIC-3).
                        if (rules::parse_item_key(item.definition).power ==
                                rules::ItemPower::lightning &&
                            !detail::has_effect(a.effects, detail::EffectKind::javelin_spent))
                        {
                            add(id, "javelin_lightning", "Javelin of Lightning", other.source.id);
                            commands.back().item = item.id;
                        }
                    }
                }
            }
        }
        // An attached Stirge cannot attack.
        if (feet <= d.reach && !attached_host(a))
        {
            add(id, "melee",
                a.source.definition.starts_with("slums-kobold") ? "Dagger attack" : "Melee attack",
                other.source.id);
        }
        // Pact of the Blade: the pact weapon may deal Necrotic, Psychic
        // or Radiant damage instead of its own type.
        if (feet <= d.reach && d.pact_of_the_blade && d.melee.count && !d.ranged_weapon &&
            !d.weapon_label.empty())
        {
            add(id, "melee_necrotic", "Melee attack (Necrotic)", other.source.id);
            add(id, "melee_psychic", "Melee attack (Psychic)", other.source.id);
            add(id, "melee_radiant", "Melee attack (Radiant)", other.source.id);
        }
        // Shillelagh: the weapon may deal Force damage instead of its own type.
        if (feet <= d.reach && shillelagh(a))
        {
            add(id, "melee_force", "Melee attack (Force)", other.source.id);
        }
        offer_tactical(AttackRange::melee, feet <= d.reach, other);
        offer_cunning(AttackRange::melee, feet <= d.reach, other);
        // Stunning Strike: a Monk weapon or Unarmed Strike, once a turn, 1 Focus on a hit.
        if (feet <= d.reach && d.stunning_strike && d.martial_arts && !a.stun_used &&
            a.surges[CharacterClass::monk] > 0)
        {
            add(id, "melee_stun", "Melee attack (Stunning Strike)", other.source.id);
        }
        // Flame Blade: a melee spell attack with the blade as an Action.
        if (feet <= 5 && a.actions.available(true) &&
            detail::has_effect(a.effects, detail::EffectKind::flame_blade))
        {
            add(id, "flame_blade_strike", "Flame Blade attack", other.source.id);
        }
        // Produce Flame: the flame in hand is hurled as an Action.
        if (feet <= 60 && a.actions.available(true) &&
            detail::has_effect(a.effects, detail::EffectKind::produce_flame))
        {
            add(id, "hurl_flame", "Hurl flame (Produce Flame)", other.source.id);
        }
        // Reckless Attack: chosen with the turn's first attack roll, which
        // is the Attack action's first attack.
        if (feet <= d.reach && d.reckless && !a.attacked && strength_attack(a, AttackRange::melee))
        {
            add(id, "reckless", "Reckless attack", other.source.id);
        }
        // Brutal Strike: after Reckless Attack, once a turn, one Strength attack
        // gives up its Advantage for 1d10 more damage and a blow.
        if (feet <= d.reach && d.brutal_strike && !a.brutal_used &&
            detail::has_effect(a.effects, detail::EffectKind::reckless) &&
            strength_attack(a, AttackRange::melee))
        {
            for (const auto &[verb, label, blow] : brutal_blows)
            {
                if (d.improved_brutal || blow == BrutalBlow::forceful ||
                    blow == BrutalBlow::hamstring)
                {
                    add(id, std::string(verb), std::string(label), other.source.id);
                }
            }
        }
        // True Strike: the cantrip's attack with the melee weapon in hand.
        if (feet <= d.reach && d.melee.count && !d.ranged_weapon &&
            detail::knows_spell(d.spells, "true_strike") && a.actions.available(true))
        {
            add(id, "true_strike", "True Strike", other.source.id);
            add(id, "true_strike_radiant", "True Strike (Radiant)", other.source.id);
        }
        if (d.range > 0 && feet <= d.long_range && (!d.ranged_loading || reloaded(a)) &&
            !shield_blocks_bow(a))
        {
            add(id, "ranged", "Ranged attack", other.source.id);
            offer_tactical(AttackRange::ranged, true, other);
            offer_cunning(AttackRange::ranged, true, other);
        }
    };
    // Every attack it offers is hostile.
    const auto offer_attacks = [&](const Actor &other, int feet)
    {
        const auto first = commands.size();
        offer_weapon_attacks(other, feet);
        for (auto i = first; i < commands.size(); ++i)
        {
            commands[i].intent = Intent::hostile;
        }
    };
    // Breath Weapon takes the place of one of the Attack action's attacks.
    const auto offer_breath = [&]
    {
        if (!def(a).breath || trait_uses_left(a) <= 0 || a.actions.exhaled)
        {
            return;
        }
        commands.push_back(
            {revision_, id, 0, "breath_weapon_cone", "Breath Weapon (cone)", Cell{}, 0, true});
        commands.back().spell = spell_facts(a, *detail::find_spell("breath_weapon_cone"));
        commands.push_back(
            {revision_, id, 0, "breath_weapon_line", "Breath Weapon (line)", Cell{}, 0, true});
        commands.back().spell = spell_facts(a, *detail::find_spell("breath_weapon_line"));
    };
    // Extra Attack: the Attack action's later attacks, once its first is made.
    if (!a.actions.available() && a.actions.extra_attacks > 0)
    {
        offer_breath();
        for (const auto &other : actors_)
        {
            if (!other.life.dead && other.source.id != a.source.id && line_of_sight(a, other))
            {
                offer_attacks(other, distance(a, other));
            }
        }
    }
    if (a.actions.available())
    {
        offer_spells(commands, a, a, 0, detail::SpellTarget::area, false);
        // Armor of Shadows and Fiendish Vigor: Mage Armor and False Life on the
        // Warlock, cast without a slot.
        if (a.actions.available(true) && !rage_of(a))
        {
            if (d.armor_of_shadows && d.mage_armor_ac &&
                !detail::has_effect(a.effects, detail::EffectKind::mage_armor))
            {
                add(id, "armor_of_shadows", "Armor of Shadows (Mage Armor)", id);
            }
            if (d.fiendish_vigor)
            {
                add(id, "fiendish_vigor", "Fiendish Vigor (False Life)", id);
            }
        }
        // Land's Aid: a Magic action spending a use of Wild Shape, aimed at a point.
        if (d.lands_aid && a.channel_divinity[CharacterClass::druid] > 0 &&
            a.actions.available(true))
        {
            commands.push_back({revision_, id, 0, "lands_aid", "Land's Aid", Cell{}, 0, true});
        }
        // Dragon's Breath: its holder exhales the chosen cone as an Action.
        for (const auto &effect : a.effects.active)
        {
            if (effect.kind == detail::EffectKind::dragons_breath)
            {
                for (const auto type : detail::dragon_types)
                {
                    if (detail::damage_type(type) == static_cast<detail::DamageType>(effect.dc))
                    {
                        commands.push_back({revision_, id, 0,
                                            "dragons_breath_exhale_" + std::string(type),
                                            "Exhale (Dragon's Breath)", Cell{}, 0, true});
                        commands.back().spell =
                            spell_facts(a, *detail::find_spell("dragons_breath_exhale"));
                    }
                }
            }
        }
        offer_breath();
        add(id, "dash", "Dash");
        add(id, "dodge", "Dodge");
        if (can_hide(a))
        {
            add(id, "hide", "Hide");
        }
        if (std::any_of(actors_.begin(), actors_.end(),
                        [&](const auto &other)
                        {
                            return other.source.side != a.source.side && hidden(other);
                        }))
        {
            add(id, "search", "Search");
        }
        if (shield_blocks_bow(a))
        {
            add(id, "doff_shield", "Take off shield");
        }
        if (a.burning)
        {
            add(id, "extinguish", "Put out the fire");
        }
        // A Stirge clinging to this creature or one within 5 feet of it.
        for (const auto &other : actors_)
        {
            if (const auto host = attached_host(other);
                host && !other.life.dead && find_actor(*host) && distance(a, actor(*host)) <= 5)
            {
                add(id, "detach", "Pull off", other.source.id);
            }
        }
        add(id, "disengage", "Disengage");
        for (const auto &other : actors_)
        {
            if (other.life.dead || !line_of_sight(a, other))
            {
                continue;
            }
            const int feet = distance(a, other);
            if (!a.source.character_profile.empty() && other.life.hp == 0 && !other.life.stable &&
                !may_rise(other) && feet <= 5)
            {
                add(id, "stabilize", "Stabilize", other.source.id);
            }
            offer_spells(commands, a, other, feet, detail::SpellTarget::any_creature, false);
            // Attacks and healing may take any creature, a party member or one
            // at 0 Hit Points included (CLOSE-1).
            if (other.source.id != a.source.id)
            {
                offer_attacks(other, feet);
                offer_spells(commands, a, other, feet, detail::SpellTarget::enemy, false);
            }
            for (const auto scope : {detail::SpellTarget::wounded_ally, detail::SpellTarget::ally,
                                     detail::SpellTarget::self, detail::SpellTarget::dying_ally})
            {
                offer_spells(commands, a, other, feet, scope, false);
            }
        }
    }
    else if (a.actions.haste)
    {
        // Haste's action: one weapon attack, Dash, Disengage, Hide or Utilize.
        add(id, "dash", "Dash");
        if (can_hide(a))
        {
            add(id, "hide", "Hide");
        }
        if (shield_blocks_bow(a))
        {
            add(id, "doff_shield", "Take off shield");
        }
        add(id, "disengage", "Disengage");
        if (!a.actions.extra_attacks)
        {
            for (const auto &other : actors_)
            {
                if (!other.life.dead && other.source.id != a.source.id && line_of_sight(a, other))
                {
                    offer_attacks(other, distance(a, other));
                }
            }
        }
    }
    for (const auto cell : movement_reach(id))
    {
        add(id, "move", "Move", 0, cell);
    }
    if (can_run_off(a))
    {
        for (const auto step : off_field_steps(a))
        {
            add(id, "flee", "Flee", 0, step);
        }
    }
    auto offered = filtered();
    obey_command(offered, a);
    obey_compulsion(offered, a);
    flee_turning(offered, a);
    hold_still(offered, a);
    if (a.panicked && a.must_stay)
    {
        charge_in_panic(offered, a);
    }
    // Nothing passes through a Wall of Force or of Stone: no attack across it.
    if (walled(true))
    {
        std::erase_if(offered,
                      [&](const Command &command)
                      {
                          return command.target && attack_action(command.verb) &&
                                 !line_of_effect(a, actor(command.target));
                      });
    }
    // Forcecage: nothing crosses its box, and no weapon its cage's bars.
    std::erase_if(offered,
                  [&](const Command &command)
                  {
                      return command.target && command.target != id &&
                             caged_apart(a.source.cell, actor(command.target).source.cell,
                                         attack_action(command.verb));
                  });
    // Antilife Shell: no melee attack reaches across its edge, but from a
    // Construct or an Undead.
    if (def(a).creature_type != "construct" && def(a).creature_type != "undead")
    {
        std::erase_if(offered,
                      [&](const Command &command)
                      {
                          if (!command.target || command.target == id ||
                              (command.verb != "melee" && command.verb != "torch" &&
                               !command.verb.starts_with("melee_")))
                          {
                              return false;
                          }
                          const Cell to = actor(command.target).source.cell;
                          return std::any_of(zones_.begin(), zones_.end(),
                                             [&](const auto &zone)
                                             {
                                                 const auto *caster = find_actor(zone.caster);
                                                 return zone.kind == ZoneKind::antilife && caster &&
                                                        caster->source.id != id &&
                                                        (distance(*caster, a) <= 10) !=
                                                            (distance(*caster, to) <= 10);
                                             });
                      });
    }
    // Mislead: once an attack has passed through the double, foes ignore it.
    std::erase_if(offered,
                  [&](const Command &command)
                  {
                      return command.target &&
                             std::any_of(actor(command.target).effects.active.begin(),
                                         actor(command.target).effects.active.end(),
                                         [](const auto &e)
                                         {
                                             return e.kind == detail::EffectKind::illusory && e.dc;
                                         });
                  });
    // Antimagic Field: no spell or other magic in it, nor reaching into it;
    // none from a befuddled creature.
    std::erase_if(
        offered,
        [&](const Command &command)
        {
            const bool magic = magical_command(command.verb) || is_smite(command.verb) ||
                               (detail::find_spell(command.verb) && command.verb != "flee");
            return magic && (in_antimagic(a.source.cell) ||
                             detail::has_effect(a.effects, detail::EffectKind::befuddled) ||
                             (command.target && in_antimagic(actor(command.target).source.cell)));
        });
    return offered;
}

void Session::obey_command(std::vector<Command> &commands, const Actor &a) const
{
    // Fear: a creature that sees the caster it fears flees it, as Command's
    // Flee does.
    const auto feared = fears_in_sight(a);
    const auto terror =
        std::find_if(a.effects.active.begin(), a.effects.active.end(),
                     [&](const auto &e)
                     {
                         return (e.kind == detail::EffectKind::terrified ||
                                 e.kind == detail::EffectKind::eye_panic) &&
                                e.source_scope == scope_ &&
                                std::any_of(feared.begin(), feared.end(),
                                            [&](const Actor *other)
                                            {
                                                return other->source.id == e.source_actor;
                                            });
                     });
    const auto *command = detail::command_effect(a.effects);
    if (!command && terror != a.effects.active.end())
    {
        command = &*terror;
    }
    if (!command)
    {
        return;
    }
    const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto &other)
                                     {
                                         return command->source_scope == scope_ &&
                                                other.source.id == command->source_actor;
                                     });
    // With no caster left there is nothing to approach or flee.
    if (caster == actors_.end() || caster->life.dead)
    {
        return;
    }
    const bool approach = command->kind == detail::EffectKind::command &&
                          command->dc == static_cast<int>(detail::CommandOption::approach);
    const int start = distance(a, *caster);
    // Approach ends the turn within 5 feet of the caster.
    const Command *best = nullptr;
    int best_feet = start;
    if (!approach || start > 5)
    {
        for (const auto &offer : commands)
        {
            if (offer.verb == "move")
            {
                const int feet = distance(offer.destination, *caster);
                if (approach ? feet < best_feet : feet > best_feet)
                {
                    best = &offer;
                    best_feet = feet;
                }
            }
        }
    }
    const auto offered = [&](std::string_view verb)
    {
        return std::find_if(commands.begin(), commands.end(),
                            [&](const auto &offer)
                            {
                                return offer.verb == verb;
                            });
    };
    // Flee uses the fastest means, so it Dashes once its movement runs out.
    auto kept = best                                             ? *best
                : !approach && offered("dash") != commands.end() ? *offered("dash")
                                                                 : *offered("end");
    commands.clear();
    commands.push_back(std::move(kept));
}

void Session::name_compulsion_heading(const Actor &caster, std::size_t direction)
{
    for (auto &other : actors_)
    {
        for (auto &e : other.effects.active)
        {
            if (e.kind == detail::EffectKind::compelled && e.source_actor == caster.source.id &&
                e.source_scope == scope_)
            {
                e.dc = static_cast<int>(direction) + 1;
            }
        }
    }
    const std::string name(compass_names.at(direction));
    log(caster.source.name + " compels the charmed to go " + name + ".",
        {"{name} compels the charmed to go {direction}.",
         {{"name", caster.source.name}, {"direction", name, true}}});
}

void Session::obey_compulsion(std::vector<Command> &commands, const Actor &a) const
{
    const auto compelled = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                        [](const auto &e)
                                        {
                                            return e.kind == detail::EffectKind::compelled && e.dc;
                                        });
    if (compelled == a.effects.active.end())
    {
        return;
    }
    // "Taking the safest route" is not weighed: the move that goes furthest.
    const auto heading = compass.at(static_cast<std::size_t>(compelled->dc - 1));
    const Command *best = nullptr;
    int best_progress = 0;
    for (const auto &offer : commands)
    {
        if (offer.verb == "move")
        {
            const int progress = (offer.destination.x - a.source.cell.x) * heading.x +
                                 (offer.destination.y - a.source.cell.y) * heading.y;
            if (progress > best_progress)
            {
                best = &offer;
                best_progress = progress;
            }
        }
    }
    if (best)
    {
        auto kept = *best;
        commands.clear();
        commands.push_back(std::move(kept));
    }
}

void Session::finish_compulsion(Actor &a)
{
    const auto compelled = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                        [](const auto &e)
                                        {
                                            return e.kind == detail::EffectKind::compelled && e.dc;
                                        });
    if (compelled == a.effects.active.end() || a.life.dead || a.life.hp <= 0)
    {
        return;
    }
    compelled->dc = 0;
    const auto id = compelled->id;
    const auto &caster = actor(compelled->source_actor);
    if (saving_throw_succeeds(a, detail::Ability::wisdom, spell_dc(caster, "compulsion"), false,
                              heightened_against(a, caster, "compulsion")))
    {
        std::erase_if(a.effects.active,
                      [&](const auto &e)
                      {
                          return e.id == id;
                      });
        log(a.source.name + " is no longer Charmed.",
            {"{name} is no longer Charmed.", {{"name", a.source.name}}});
    }
}

std::vector<Cell> Session::movement_reach(EntityId id) const
{
    std::vector<Cell> cells;
    if (outcome_ != Outcome::ongoing || (!champion_move_ && pending()) || temporary_offer_ ||
        check_choice_ || graze_ || (!champion_move_ && effect_waiting()) ||
        (champion_move_ && id != champion_move_->actor))
    {
        return cells;
    }
    const auto actor = std::find_if(actors_.begin(), actors_.end(),
                                    [&](const auto &a)
                                    {
                                        return a.source.id == id;
                                    });
    if (actor == actors_.end() || !conscious(*actor))
    {
        return cells;
    }
    const auto budget = champion_move_ ? champion_move_->remaining : movement_left(*actor);
    if (budget <= 0)
    {
        return cells;
    }
    const auto reachable = movement_grid(*actor).reachable(budget);
    for (int y = 0; y < board_.height; ++y)
    {
        for (int x = 0; x < board_.width; ++x)
        {
            if (reachable.cost_to({x, y}))
            {
                cells.push_back({x, y});
            }
        }
    }
    return cells;
}
} // namespace opengold::srd5::detail
