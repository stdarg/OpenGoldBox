#include "session.h"

// Session: monster features (legendary actions, breath, gazes, drains,
// charges) and their loadouts.
namespace opengold::srd5::detail
{
using namespace rules;

void Session::fey_frighten(Actor &caster, Actor &target)
{
    if (target.life.dead || fearless(target) || !detail::can_apply(target.effects))
    {
        return;
    }
    detail::apply_poisoned(target.effects, effect_source(caster), caster.source.name,
                           next_turn_ms(caster), detail::EffectKind::fey_fright);
    log(target.source.name + " is Frightened.",
        {"{name} is Frightened.", {{"name", target.source.name}}});
}

void Session::enthrall(Actor &pirate, Actor &target)
{
    log(pirate.source.name + " uses Enthralling Panache on " + target.source.name + ".",
        {"{name} uses Enthralling Panache on {target}.",
         {{"name", pirate.source.name}, {"target", target.source.name}}});
    if (mindless(target) || aura_of_devotion(target) ||
        detail::has_effect(target.effects, detail::EffectKind::mind_blank))
    {
        log(target.source.name + " is immune to the charm.",
            {"{name} is immune to the charm.", {{"name", target.source.name}}});
        return;
    }
    if (charm_or_fear_save(target, detail::Ability::wisdom, def(pirate).panache_dc,
                           def(target).fey_ancestry) ||
        !detail::can_apply(target.effects))
    {
        return;
    }
    detail::apply_poisoned(target.effects, effect_source(pirate), pirate.source.name,
                           next_turn_ms(pirate), detail::EffectKind::enthralled);
    log(target.source.name + " is Charmed by " + pirate.source.name + ".",
        {"{name} is Charmed by {caster}.",
         {{"name", target.source.name}, {"caster", pirate.source.name}}});
}

// A Mummy's Dreadful Glare (SRD 5.2.1) at a creature it sees within range.
void Session::dreadful_glare(Actor &mummy, Actor &target)
{
    const auto &d = def(mummy);
    const bool resisted = std::any_of(target.effects.active.begin(), target.effects.active.end(),
                                      [&](const auto &e)
                                      {
                                          return e.kind == detail::EffectKind::glare_resisted &&
                                                 e.source_actor == mummy.source.id;
                                      });
    if (!conscious(target) || resisted || invisible(target) || has_condition(mummy, "blinded") ||
        distance(mummy, target) > d.glare_range || !line_of_sight(mummy, target) ||
        fearless(target) || detail::has_effect(target.effects, detail::EffectKind::frightened) ||
        detail::has_effect(target.effects, detail::EffectKind::glared) ||
        !detail::can_apply(target.effects))
    {
        return;
    }
    log(mummy.source.name + " uses Dreadful Glare on " + target.source.name + ".",
        {"{name} uses Dreadful Glare on {target}.",
         {{"name", mummy.source.name}, {"target", target.source.name}}});
    if (charm_or_fear_save(target, detail::Ability::wisdom, d.glare_dc, def(target).brave))
    {
        detail::apply_spell_benefit(target.effects, effect_source(mummy), mummy.source.name,
                                    detail::EffectKind::glare_resisted, 0);
        return;
    }
    // Until the end of the mummy's next turn.
    const auto index = index_of(mummy.source.id);
    const unsigned turn = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
    detail::apply_poisoned(target.effects, effect_source(mummy), mummy.source.name,
                           next_turn_ms(mummy) + turn, detail::EffectKind::glared);
    log(target.source.name + " is Frightened.",
        {"{name} is Frightened.", {{"name", target.source.name}}});
}

// A Vampire's Bite (SRD 5.2.1) at an adjacent creature it has Charmed or that
// is Incapacitated or Restrained.
void Session::bite(Actor &vampire, Actor &target)
{
    const auto &d = def(vampire);
    if (target.life.dead || distance(vampire, target) > 5 ||
        !(charmed_by(target, vampire.source.id) || detail::incapacitated(target.effects) ||
          has_condition(target, "restrained")))
    {
        return;
    }
    log(vampire.source.name + " uses Bite on " + target.source.name + ".",
        {"{name} uses Bite on {target}.",
         {{"name", vampire.source.name}, {"target", target.source.name}}});
    if (saving_throw_succeeds(target, detail::Ability::constitution, d.bite_dc))
    {
        return;
    }
    for (const auto &[type, rolled] : {std::pair{detail::DamageType::piercing, d.bite_pierce},
                                       std::pair{detail::DamageType::necrotic, d.bite_necrotic}})
    {
        if (target.life.dead)
        {
            break;
        }
        const int amount = resolved_damage(target, type, dice(rolled));
        const auto name = std::string(detail::damage_name(type));
        log(target.source.name + " takes " + std::to_string(amount) + " " + name + " damage.",
            {"{name} takes {damage} {type} damage.",
             {{"name", target.source.name},
              {"damage", std::to_string(amount)},
              {"type", name, true}}});
        damage(target, amount, false);
        if (type == detail::DamageType::necrotic)
        {
            lower_hit_point_maximum(vampire, target, amount);
            (void)heal(vampire, amount);
        }
    }
}

void Session::strike_riders(Actor &a, Actor &target, bool ranged)
{
    const auto &d = def(a);
    const auto strike = ranged ? Definition::Strike::ranged : Definition::Strike::melee;
    const auto index = index_of(target.source.id);
    const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
    if (d.poisoning == strike && detail::can_apply(target.effects))
    {
        detail::apply_poisoned(target.effects, effect_source(a), a.source.name,
                               d.poisoning_own_turn ? next_turn_ms(a)
                                                    : next_turn_ms(target) + slot);
        log(target.source.name + " is Poisoned.",
            {"{name} is Poisoned.", {{"name", target.source.name}}});
    }
    if (d.hampering == strike && detail::can_apply(target.effects))
    {
        detail::apply_poisoned(target.effects, effect_source(a), a.source.name,
                               next_turn_ms(target) + slot, detail::EffectKind::hampered);
        log(target.source.name + " has Disadvantage on its next attack roll.",
            {"{name} has Disadvantage on its next attack roll.", {{"name", target.source.name}}});
    }
    if (d.pushing == strike)
    {
        push_away(a, target, d.push_feet / 5);
    }
    if (!ranged && d.paralysis_dc)
    {
        paralyze(a, target);
    }
    // Until this turn ends: its next attack roll this turn has Advantage.
    if (!ranged && d.hit_advantage && detail::can_apply(a.effects))
    {
        detail::apply_poisoned(a.effects, effect_source(a), a.source.name, 1,
                               detail::EffectKind::poised);
    }
}

// A Pirate Captain's Captain's Charm (SRD 5.2.1), a Bonus Action: a Wisdom
// save or Charmed until the start of the captain's next turn.
void Session::captains_charm(Actor &captain, Actor &target)
{
    const auto &d = def(captain);
    if (!conscious(target) || invisible(target) || has_condition(captain, "blinded") ||
        distance(captain, target) > d.charm_range || !line_of_sight(captain, target) ||
        charmed_by(target, captain.source.id) || !detail::can_apply(target.effects))
    {
        return;
    }
    captain.bonus = false;
    log(captain.source.name + " uses Captain's Charm on " + target.source.name + ".",
        {"{name} uses Captain's Charm on {target}.",
         {{"name", captain.source.name}, {"target", target.source.name}}});
    if (mindless(target) || aura_of_devotion(target) ||
        detail::has_effect(target.effects, detail::EffectKind::mind_blank))
    {
        log(target.source.name + " is immune to the charm.",
            {"{name} is immune to the charm.", {{"name", target.source.name}}});
        return;
    }
    if (charm_or_fear_save(target, detail::Ability::wisdom, d.charm_dc, def(target).fey_ancestry))
    {
        return;
    }
    detail::apply_poisoned(target.effects, effect_source(captain), captain.source.name,
                           next_turn_ms(captain), detail::EffectKind::turn_charmed);
    log(target.source.name + " is Charmed by " + captain.source.name + ".",
        {"{name} is Charmed by {caster}.",
         {{"name", target.source.name}, {"caster", captain.source.name}}});
}

bool Session::specter_corpse(const Actor &wraith, const Actor &corpse) const
{
    return corpse.life.dead && def(corpse).creature_type == "humanoid" && !absent(corpse) &&
           !detail::has_effect(corpse.effects, detail::EffectKind::dust) &&
           !detail::has_effect(corpse.effects, detail::EffectKind::petrified) &&
           distance(wraith, corpse) <= 10;
}

int Session::specters_of(const Actor &wraith) const
{
    return static_cast<int>(std::count_if(
        actors_.begin(), actors_.end(),
        [&](const auto &other)
        {
            return !other.life.dead && other.source.definition == def(wraith).specter_key &&
                   std::any_of(other.effects.active.begin(), other.effects.active.end(),
                               [&](const auto &e)
                               {
                                   return e.kind == detail::EffectKind::raised &&
                                          e.source_actor == wraith.source.id;
                               });
        }));
}

void Session::take_legendary_actions()
{
    std::vector<EntityId> legendary;
    for (const auto &a : actors_)
    {
        if (def(a).legendary_uses && a.source.id != actors_[turn_].source.id)
        {
            legendary.push_back(a.source.id);
        }
    }
    for (const auto id : legendary)
    {
        if (find_actor(id) && outcome_ == Outcome::ongoing)
        {
            take_legendary_action(actor(id));
            update_outcome();
        }
    }
}

// The computer prefers a Thunderclap on two enemies or more, then a strike at
// an enemy beside the monster (which it takes without moving), then Beguile on
// the nearest enemy, then Guiding Light.
void Session::take_legendary_action(Actor &monster)
{
    using Option = Definition::LegendaryAction;
    const auto &d = def(monster);
    const auto spent_effect = [&]
    {
        return std::find_if(monster.effects.active.begin(), monster.effects.active.end(),
                            [](const auto &e)
                            {
                                return e.kind == detail::EffectKind::legendary_spent;
                            });
    };
    const auto spent = spent_effect();
    const bool any_spent = spent != monster.effects.active.end();
    const int taken = any_spent ? spent->dc & 15 : 0;
    const bool once_taken = any_spent && (spent->dc & 16);
    if (taken >= d.legendary_uses || !conscious(monster) || monster.fled || absent(monster) ||
        detail::incapacitated(monster.effects) || has_condition(monster, "blinded") ||
        (!any_spent && !detail::can_apply(monster.effects)))
    {
        return;
    }
    const auto offers = [&](Option option)
    {
        return std::find(d.legendary_actions.begin(), d.legendary_actions.end(), option) !=
               d.legendary_actions.end();
    };
    std::optional<Option> chosen;
    Actor *target = nullptr;
    std::optional<Cell> center;
    if (offers(Option::thunderclap) && (center = thunderclap_center(monster)))
    {
        chosen = Option::thunderclap;
    }
    else if ((offers(Option::deathless_strike) || offers(Option::pounce)) &&
             (target = legendary_target(monster, 5, true)))
    {
        chosen = offers(Option::deathless_strike) ? Option::deathless_strike : Option::pounce;
    }
    else if (offers(Option::beguile) && !once_taken &&
             (target = legendary_target(monster, detail::find_spell("command")->range, false)))
    {
        chosen = Option::beguile;
    }
    else if (offers(Option::guiding_light) &&
             (target = legendary_target(monster, detail::find_spell("guiding_bolt")->range, false)))
    {
        chosen = Option::guiding_light;
    }
    if (!chosen)
    {
        return;
    }
    if (!any_spent)
    {
        detail::apply_spell_benefit(monster.effects, effect_source(monster), monster.source.name,
                                    detail::EffectKind::legendary_spent, 0);
    }
    spent_effect()->dc += 1 + (*chosen == Option::beguile ? 16 : 0);
    static constexpr std::array<std::string_view, 5> labels{
        "Beguile", "Deathless Strike", "Guiding Light", "Pounce", "Thunderclap"};
    const std::string label(labels[static_cast<std::size_t>(*chosen)]);
    log(monster.source.name + " uses " + label + " (Legendary Action).",
        {"{name} uses {action} (Legendary Action).",
         {{"name", monster.source.name}, {"action", label, true}}});
    switch (*chosen)
    {
    case Option::thunderclap:
        thunderclap(monster, *center);
        return;
    case Option::deathless_strike:
    case Option::pounce:
        attack(monster, *target, AttackRange::melee);
        return;
    case Option::beguile:
        // Command's Grovel: the target falls Prone and loses its turn.
        resolve_spell(*detail::find_spell("command"),
                      std::string("command_grovel") + std::string(detail::legendary_suffix),
                      monster, target->source.id);
        return;
    case Option::guiding_light:
        // Guiding Bolt's level 2 version.
        resolve_spell(*detail::find_spell("guiding_bolt"),
                      std::string("guiding_bolt") + std::string(detail::legendary_suffix) + "_2",
                      monster, target->source.id);
        return;
    }
}

Actor *Session::legendary_target(const Actor &a, int range, bool adjacent)
{
    Actor *chosen = nullptr;
    for (auto &other : actors_)
    {
        if (other.source.side == a.source.side || other.life.hp <= 0 || other.life.dead ||
            other.fled || absent(other) || invisible(other) || distance(a, other) > range ||
            !line_of_sight(a, other))
        {
            continue;
        }
        if (!chosen || (adjacent ? other.life.hp < chosen->life.hp
                                 : distance(a, other) < distance(a, *chosen)))
        {
            chosen = &other;
        }
    }
    return chosen;
}

std::optional<Cell> Session::thunderclap_center(const Actor &dragon) const
{
    const auto &spell = *detail::find_spell("thunderclap");
    std::optional<Cell> best;
    int most = 1;
    for (const auto &aimed : actors_)
    {
        if (aimed.source.side == dragon.source.side || aimed.life.dead || absent(aimed) ||
            invisible(aimed) || distance(dragon, aimed) > spell.range ||
            !line_of_sight(dragon, aimed))
        {
            continue;
        }
        const auto cells = area_cells(spell, spell.id, origin_toward(dragon, aimed.source.cell),
                                      aimed.source.cell);
        int enemies = 0;
        bool friendly = false;
        for (const auto &other : actors_)
        {
            if (other.life.dead || absent(other) || !touches(cells, other))
            {
                continue;
            }
            if (other.source.side == dragon.source.side)
            {
                friendly = true;
            }
            else if (other.life.hp > 0)
            {
                ++enemies;
            }
        }
        if (!friendly && enemies > most)
        {
            most = enemies;
            best = aimed.source.cell;
        }
    }
    return best;
}

// An Adult Bronze Dragon's Thunderclap (SRD 5.2.1): each creature in a 20-foot
// Sphere makes a Constitution save against the dragon's spell save DC or takes
// 3d6 Thunder damage. Its Deafened has nothing to act on (DEAF-1).
void Session::thunderclap(Actor &dragon, Cell center)
{
    const auto &spell = *detail::find_spell("thunderclap");
    const auto cells = area_cells(spell, spell.id, origin_toward(dragon, center), center);
    const int dc = spell_dc(dragon);
    std::vector<EntityId> caught;
    for (const auto &other : actors_)
    {
        if (!other.life.dead && !absent(other) && touches(cells, other))
        {
            caught.push_back(other.source.id);
        }
    }
    for (const auto id : caught)
    {
        auto &target = actor(id);
        if (target.life.dead || saving_throw_succeeds(target, detail::Ability::constitution, dc))
        {
            continue;
        }
        const auto type = detail::DamageType::thunder;
        const int amount = resolved_damage(target, type, dice(spell.dice));
        const auto name = std::string(detail::damage_name(type));
        log(target.source.name + " takes " + std::to_string(amount) + " " + name + " damage.",
            {"{name} takes {damage} {type} damage.",
             {{"name", target.source.name},
              {"damage", std::to_string(amount)},
              {"type", name, true}}});
        damage(target, amount, false);
    }
}

int Session::repulsion_catches(const Actor &dragon, Cell toward) const
{
    const auto &spell = *detail::find_spell("repulsion_breath");
    const auto cells = area_cells(spell, spell.id, origin_toward(dragon, toward), toward);
    int enemies = 0;
    for (const auto &other : actors_)
    {
        if (other.source.id == dragon.source.id || other.life.dead || absent(other) ||
            !touches(cells, other))
        {
            continue;
        }
        if (other.source.side == dragon.source.side)
        {
            return 0;
        }
        enemies += other.life.hp > 0;
    }
    return enemies;
}

// An Adult Bronze Dragon's Repulsion Breath (SRD 5.2.1): each creature in a
// 30-foot Cone makes a Strength save or is pushed 60 feet straight away from
// the dragon and knocked Prone.
void Session::repulsion_breath(Actor &dragon, Cell toward)
{
    const auto &spell = *detail::find_spell("repulsion_breath");
    const auto cells = area_cells(spell, spell.id, origin_toward(dragon, toward), toward);
    log(dragon.source.name + " uses Repulsion Breath.",
        {"{name} uses Repulsion Breath.", {{"name", dragon.source.name}}});
    std::vector<EntityId> caught;
    for (const auto &other : actors_)
    {
        if (other.source.id != dragon.source.id && !other.life.dead && !absent(other) &&
            touches(cells, other))
        {
            caught.push_back(other.source.id);
        }
    }
    for (const auto id : caught)
    {
        auto &target = actor(id);
        if (saving_throw_succeeds(target, detail::Ability::strength, def(dragon).repulsion_dc))
        {
            continue;
        }
        push_away(dragon, target, 12);
        if (!target.effects.prone)
        {
            target.effects.prone = true;
            log(target.source.name + " is knocked Prone.",
                {"{name} is knocked Prone.", {{"name", target.source.name}}});
        }
    }
}

void Session::drain_life(Actor &wight, Actor &target)
{
    log(wight.source.name + " uses Life Drain on " + target.source.name + ".",
        {"{name} uses Life Drain on {target}.",
         {{"name", wight.source.name}, {"target", target.source.name}}});
    const auto &d = def(wight);
    if (saving_throw_succeeds(target, detail::Ability::constitution, d.life_drain_dc))
    {
        return;
    }
    const auto type = detail::DamageType::necrotic;
    const int amount = resolved_damage(target, type, dice(d.life_drain));
    const auto name = std::string(detail::damage_name(type));
    log(target.source.name + " takes " + std::to_string(amount) + " " + name + " damage.",
        {"{name} takes {damage} {type} damage.",
         {{"name", target.source.name}, {"damage", std::to_string(amount)}, {"type", name, true}}});
    damage(target, amount, false);
    lower_hit_point_maximum(wight, target, amount);
}

// Life Drain's lowering: the Hit Point maximum drops by the damage taken, never
// below 1, unless Aura of Life guards it; a Long Rest restores it.
void Session::lower_hit_point_maximum(const Actor &drainer, Actor &target, int amount)
{
    if (amount <= 0 || target.life.dead || in_aura_of_life(target) ||
        !detail::can_apply(target.effects))
    {
        return;
    }
    const int lowered = std::min(amount, max_hp(target) - 1);
    if (lowered > 0)
    {
        detail::apply_spell_benefit(target.effects, effect_source(drainer), drainer.source.name,
                                    detail::EffectKind::life_drained, lowered);
        target.life.hp = std::min(target.life.hp, max_hp(target));
        log(target.source.name + "'s Hit Point maximum drops by " + std::to_string(lowered) + ".",
            {"{name}'s Hit Point maximum drops by {amount}.",
             {{"name", target.source.name}, {"amount", std::to_string(lowered)}}});
    }
}

// A creature immune to a condition shakes off whatever imposed it.
void Session::shed_immune_conditions()
{
    for (auto &a : actors_)
    {
        for (const auto &condition : def(a).condition_immunities)
        {
            bool shed = false;
            if (condition == "Prone" && a.effects.prone)
            {
                a.effects.prone = false;
                shed = true;
            }
            shed |= std::erase_if(a.effects.active,
                                  [&](const auto &e)
                                  {
                                      return detail::imposes_condition(e.kind, condition);
                                  }) > 0;
            if (shed)
            {
                log(a.source.name + " is immune to the " + condition + " condition.",
                    {"{name} is immune to the {condition} condition.",
                     {{"name", a.source.name}, {"condition", condition, true}}});
            }
        }
    }
}

// A spent Petrifying Gaze or breath weapon comes back on a d6 roll of its
// Recharge number or more as the creature's turn starts.
void Session::recharge_gaze(Actor &a)
{
    const auto &d = def(a);
    const int recharge = d.gaze_recharge     ? d.gaze_recharge
                         : d.breath_recharge ? d.breath_recharge
                                             : d.trample_recharge;
    if (!recharge || trait_uses_left(a) > 0)
    {
        return;
    }
    const int rolled = roll(6);
    if (rolled < recharge)
    {
        return;
    }
    std::erase_if(a.effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::trait_uses;
                  });
    if (d.breath_recharge || d.trample_recharge)
    {
        const std::string label = d.breath_recharge
                                      ? std::string(detail::find_spell(d.monster_breath)->label)
                                      : std::string("Trampling Charge");
        log(a.source.name + "'s " + label + " recharges (" + std::to_string(rolled) + ").",
            {"{name}'s {ability} recharges ({roll}).",
             {{"name", a.source.name},
              {"ability", label, true},
              {"roll", std::to_string(rolled)}}});
        return;
    }
    log(a.source.name + "'s Petrifying Gaze recharges (" + std::to_string(rolled) + ").",
        {"{name}'s Petrifying Gaze recharges ({roll}).",
         {{"name", a.source.name}, {"roll", std::to_string(rolled)}}});
}

// A Stirge drains the creature it clings to as its turn starts, or lets go of
// one that is gone or out of reach.
void Session::drain_attached(Actor &a)
{
    const auto host = attached_host(a);
    if (!host)
    {
        return;
    }
    const auto *target = find_actor(*host);
    if (!target || target->life.dead || a.life.hp == 0 || distance(a, *target) > 5)
    {
        std::erase_if(a.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::attached;
                      });
        log(a.source.name + " lets go.", {"{name} lets go.", {{"name", a.source.name}}});
        return;
    }
    auto &host_actor = actor(*host);
    const auto type = def(a).attach_type;
    const auto name = std::string(detail::damage_name(type));
    const int amount = resolved_damage(host_actor, type, dice(*def(a).attach_drain));
    log(host_actor.source.name + " takes " + std::to_string(amount) + " " + name + " damage from " +
            a.source.name + ".",
        {"{name} takes {damage} {type} damage from {source}.",
         {{"name", host_actor.source.name},
          {"damage", std::to_string(amount)},
          {"type", name, true},
          {"source", a.source.name}}});
    damage(host_actor, amount, false);
}

// The creature a Stirge clings to, or one within 5 feet of it, pulls it off
// with an action.
void Session::pull_off(Actor &a, Actor &clinger)
{
    std::erase_if(clinger.effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::attached;
                  });
    log(a.source.name + " pulls " + clinger.source.name + " off.",
        {"{name} pulls {target} off.", {{"name", a.source.name}, {"target", clinger.source.name}}});
}

// A spent Recharge pool of innate spells comes back on a d6 roll of its number
// or more as the creature's turn starts.
void Session::recharge_innate(Actor &a)
{
    const auto &pools = def(a).innate;
    for (std::size_t pool = 0; pool < pools.size(); ++pool)
    {
        if (!pools[pool].recharge || innate_left(a, pool) > 0)
        {
            continue;
        }
        const int rolled = roll(6);
        if (rolled < pools[pool].recharge)
        {
            continue;
        }
        a.innate_spent &= ~(15 << (4 * pool));
        log(a.source.name + "'s " + pools[pool].label + " recharges (" + std::to_string(rolled) +
                ").",
            {"{name}'s {ability} recharges ({roll}).",
             {{"name", a.source.name},
              {"ability", pools[pool].label, true},
              {"roll", std::to_string(rolled)}}});
    }
}

// A Basilisk's Petrifying Gaze, its Bonus Action: each other creature in the
// Cone makes a Constitution save or is Restrained, turning to stone if it
// fails again at the end of its next turn.
void Session::petrifying_gaze(Actor &basilisk, const PendingArea &aimed)
{
    basilisk.bonus = false;
    spend_trait_use(basilisk);
    log(basilisk.source.name + " uses Petrifying Gaze.",
        {"{name} uses Petrifying Gaze.", {{"name", basilisk.source.name}}});
    const auto &gaze = *detail::find_spell(aimed.verb);
    const auto cells =
        area_cells(gaze, aimed.verb, origin_toward(basilisk, aimed.center), aimed.center);
    const int dc = def(basilisk).gaze_dc;
    for (auto &other : actors_)
    {
        if (other.life.dead || other.source.id == basilisk.source.id || absent(other) ||
            !touches(cells, other) || !detail::can_apply(other.effects) ||
            saving_throw_succeeds(other, detail::Ability::constitution, dc))
        {
            continue;
        }
        detail::apply_spell_benefit(other.effects, effect_source(basilisk), basilisk.source.name,
                                    detail::EffectKind::petrifying, dc);
        log(other.source.name + " is Restrained as its flesh stiffens.",
            {"{name} is Restrained as its flesh stiffens.", {{"name", other.source.name}}});
    }
}

// A monster's breath weapon, its action: each other creature in the area
// saves against the `breath` row's DC.
void Session::exhale_breath(Actor &dragon, const PendingArea &aimed)
{
    (void)dragon.actions.spend();
    spend_trait_use(dragon);
    const auto &breath = *detail::find_spell(aimed.verb);
    log(dragon.source.name + " uses " + std::string(breath.label) + ".",
        {"{name} uses {feature}.",
         {{"name", dragon.source.name}, {"feature", std::string(breath.label), true}}});
    damage_area(dragon, breath, aimed.verb,
                area_cells(breath, aimed.verb, origin_toward(dragon, aimed.center), aimed.center),
                def(dragon).breath_dc);
}

// The Petrifying Gaze's second save, at the end of the Restrained creature's
// turn: success ends it, failure turns the creature to stone until Greater
// Restoration.
void Session::resist_petrifying(Actor &a)
{
    const auto gazed = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                    [](const auto &e)
                                    {
                                        return e.kind == detail::EffectKind::petrifying;
                                    });
    if (gazed == a.effects.active.end() || a.life.dead)
    {
        return;
    }
    if (saving_throw_succeeds(a, detail::Ability::constitution, gazed->dc))
    {
        a.effects.active.erase(gazed);
        log(a.source.name + " shakes off the stone.",
            {"{name} shakes off the stone.", {{"name", a.source.name}}});
        return;
    }
    gazed->kind = detail::EffectKind::petrified;
    gazed->dc = 1;
    gazed->remaining_ms = detail::benefit_duration_ms(detail::EffectKind::petrified);
    log(a.source.name + " turns to stone.", {"{name} turns to stone.", {{"name", a.source.name}}});
}

Loadout Session::attack_loadout(const Actor &a, const Definition::LastAttack &profile) const
{
    Loadout last{a.definition, a.selected_weapon, a.light_damage};
    auto &d = last.definition;
    d.melee_bonus = profile.bonus;
    d.melee = profile.dice;
    d.melee_type = profile.type;
    d.melee_extra = profile.extra;
    d.melee_extra_type = profile.extra_type;
    d.melee_magic = 0;
    // The last attack carries its own riders, not the other attacks' (an
    // Ettin's Morningstar hampers where its Battleaxe knocks Prone).
    d.prone_bite = d.prone_large = false;
    d.hampering = profile.hampering ? Definition::Strike::melee : Definition::Strike::none;
    d.poisoning = profile.poisoning ? Definition::Strike::melee : Definition::Strike::none;
    d.poisoning_own_turn = profile.poisoning_own_turn;
    d.pushing = Definition::Strike::none;
    return last;
}

Loadout Session::claw_loadout(const Actor &a) const
{
    auto claw = attack_loadout(a, *def(a).claw);
    claw.definition.paralysis_dc = def(a).claw_dc;
    return claw;
}

Loadout Session::gore_loadout(const Actor &a) const
{
    const auto &d = def(a);
    auto gore = attack_loadout(a, *d.gore);
    gore.definition.charge = true;
    gore.definition.charge_feet = d.gore_feet;
    gore.definition.charge_size = 3;
    gore.definition.charge_dice = d.gore_charge;
    return gore;
}

std::optional<Session::Trample> Session::trampling_run(const Actor &centaur) const
{
    const auto size = footprint(centaur);
    const auto &board = walled_board(true);
    std::optional<Trample> best;
    for (const Cell step : {Cell{1, 0}, Cell{-1, 0}, Cell{0, 1}, Cell{0, -1}, Cell{1, 1},
                            Cell{1, -1}, Cell{-1, 1}, Cell{-1, -1}})
    {
        Trample run;
        for (int squares = 1; squares <= def(centaur).speed / 5; ++squares)
        {
            const Cell anchor{centaur.source.cell.x + step.x * squares,
                              centaur.source.cell.y + step.y * squares};
            const auto covered = squares_of(anchor, size);
            if (std::any_of(covered.begin(), covered.end(),
                            [&](Cell square)
                            {
                                return board.at(square) == Terrain::obstacle;
                            }))
            {
                break;
            }
            bool open = true, blocked = false;
            for (const auto &other : actors_)
            {
                if (other.source.id == centaur.source.id || other.life.dead || absent(other) ||
                    std::none_of(covered.begin(), covered.end(),
                                 [&](Cell square)
                                 {
                                     return occupies(other, square);
                                 }))
                {
                    continue;
                }
                open = false;
                blocked |= def(other).size > 2;
                if (std::find(run.entered.begin(), run.entered.end(), other.source.id) ==
                    run.entered.end())
                {
                    run.entered.push_back(other.source.id);
                    if (conscious(other))
                    {
                        ++(other.source.side == centaur.source.side ? run.allies : run.enemies);
                    }
                }
            }
            if (blocked)
            {
                break;
            }
            if (open && room_for(&centaur, size, anchor) && run.allies == 0 &&
                (!best || run.enemies > best->enemies))
            {
                run.end = anchor;
                best = run;
            }
        }
    }
    return best;
}

void Session::trample(Actor &centaur, const Trample &run)
{
    const auto &d = def(centaur);
    centaur.bonus = false;
    spend_trait_use(centaur);
    centaur.source.cell = run.end;
    log(centaur.source.name + " uses Trampling Charge.",
        {"{name} uses Trampling Charge.", {{"name", centaur.source.name}}});
    for (const auto id : run.entered)
    {
        auto &target = actor(id);
        if (target.life.dead ||
            saving_throw_succeeds(target, detail::Ability::strength, d.trample_dc))
        {
            continue;
        }
        const int amount =
            resolved_damage(target, detail::DamageType::bludgeoning, dice(d.trample_damage));
        log(target.source.name + " takes " + std::to_string(amount) + " Bludgeoning damage.",
            {"{name} takes {damage} {type} damage.",
             {{"name", target.source.name},
              {"damage", std::to_string(amount)},
              {"type", "Bludgeoning", true}}});
        damage(target, amount, false);
        if (!target.life.dead && !target.effects.prone)
        {
            target.effects.prone = true;
            log(target.source.name + " is knocked Prone.",
                {"{name} is knocked Prone.", {{"name", target.source.name}}});
        }
    }
}

void Session::jaunt_away(Actor &spider)
{
    spider.bonus = false;
    detail::apply_spell_benefit(spider.effects, effect_source(spider), spider.source.name,
                                detail::EffectKind::jaunted, 0);
    log(spider.source.name + " slips into the Ethereal Plane.",
        {"{name} slips into the Ethereal Plane.", {{"name", spider.source.name}}});
}

bool Session::jaunt_back(Actor &spider)
{
    if (!std::erase_if(spider.effects.active,
                       [](const auto &e)
                       {
                           return e.kind == detail::EffectKind::jaunted;
                       }))
    {
        return false;
    }
    spider.source.cell = nearest_open(spider);
    log(spider.source.name + " returns from the Ethereal Plane.",
        {"{name} returns from the Ethereal Plane.", {{"name", spider.source.name}}});
    return true;
}

void Session::start_rampage(Actor &gnoll)
{
    gnoll.bonus = false;
    spend_trait_use(gnoll);
    gnoll.movement += def(gnoll).speed / 2;
    ++gnoll.dashes;
    const unsigned rest_of_turn = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
    detail::apply_poisoned(gnoll.effects, effect_source(gnoll), gnoll.source.name, rest_of_turn,
                           detail::EffectKind::rampaging);
    log(gnoll.source.name + " rampages.", {"{name} rampages.", {{"name", gnoll.source.name}}});
}

void Session::envenom(Actor &spider, Actor &target)
{
    detail::stabilize(target.life, rng_);
    const auto &immunities = def(target).condition_immunities;
    if (!detail::can_apply(target.effects) ||
        std::find(immunities.begin(), immunities.end(), "Poisoned") != immunities.end())
    {
        return;
    }
    detail::apply_spell_benefit(target.effects, effect_source(spider), spider.source.name,
                                detail::EffectKind::envenomed_paralyzed, 0);
    log(target.source.name + " is Stable, and Poisoned and Paralyzed for an hour.",
        {"{name} is Stable, and Poisoned and Paralyzed for an hour.",
         {{"name", target.source.name}}});
}

bool Session::claw_paralyzes(const Actor &target) const
{
    const auto &d = def(target);
    return conscious(target) && d.creature_type != "undead" && !d.elf &&
           !detail::paralyzed(target.effects) &&
           std::find(d.condition_immunities.begin(), d.condition_immunities.end(), "Paralyzed") ==
               d.condition_immunities.end();
}

void Session::paralyze(Actor &ghoul, Actor &target)
{
    if (!claw_paralyzes(target) || !detail::can_apply(target.effects) ||
        saving_throw_succeeds(target, detail::Ability::constitution, def(ghoul).paralysis_dc))
    {
        return;
    }
    const auto index = index_of(target.source.id);
    const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
    detail::apply_poisoned(target.effects, effect_source(ghoul), ghoul.source.name,
                           next_turn_ms(target) + slot, detail::EffectKind::clawed);
    log(target.source.name + " is Paralyzed.",
        {"{name} is Paralyzed.", {{"name", target.source.name}}});
}

void Session::ward_nature()
{
    for (auto &a : actors_)
    {
        if (def(a).natures_ward)
        {
            std::erase_if(a.effects.active,
                          [](const auto &e)
                          {
                              return gives_condition(e.kind, "poisoned");
                          });
        }
    }
}

void Session::overchannel_backlash()
{
    if (!overchannel_dice_)
    {
        return;
    }
    auto &caster = actor(overchannel_caster_);
    const int count = overchannel_dice_;
    overchannel_dice_ = 0;
    if (caster.life.dead)
    {
        return;
    }
    // It ignores Resistance and Immunity.
    const int amount = dice({count, 12, 0});
    log(caster.source.name + " takes " + std::to_string(amount) +
            " Necrotic damage from Overchannel.",
        {"{name} takes {damage} Necrotic damage from Overchannel.",
         {{"name", caster.source.name}, {"damage", std::to_string(amount)}}});
    damage(caster, amount);
}

void Session::end_polymorph(Actor &a)
{
    std::erase_if(a.effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::polymorphed;
                  });
    if (a.life.temporary_hp.source_id == "spell:polymorph")
    {
        a.life.temporary_hp = {};
    }
    const int form_speed = def(a).speed;
    refresh_form(a);
    change_speed(a, form_speed);
    log(a.source.name + " returns to its own form.",
        {"{name} returns to its own form.", {{"name", a.source.name}}});
}

void Session::tire(Actor &a)
{
    if (a.life.dead || !detail::can_apply(a.effects))
    {
        return;
    }
    const auto index = index_of(a.source.id);
    const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
    detail::apply_poisoned(a.effects, effect_source(a), a.source.name, next_turn_ms(a) + slot,
                           detail::EffectKind::lethargic);
    a.actions.haste = false;
    log("A wave of lethargy washes over " + a.source.name + ".",
        {"A wave of lethargy washes over {name}.", {{"name", a.source.name}}});
}
} // namespace opengold::srd5::detail
