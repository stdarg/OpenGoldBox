#include "session.h"
#include <iterator>

// Session: damage and healing, attack rolls and hits, weapon mastery,
// hiding, concentration and the attack's class riders.
namespace opengold::srd5::detail
{
using namespace rules;

int Session::resolved_damage(Actor &target, detail::DamageType type, int amount, bool ordinary_fire)
{
    last_damage_type_ = type;
    // Resistance (the cantrip): 1d4 less of its type, once per turn, before
    // resistances halve the rest.
    const auto ward = std::find_if(target.effects.active.begin(), target.effects.active.end(),
                                   [&](const auto &e)
                                   {
                                       return e.kind == detail::EffectKind::resistance &&
                                              e.dc == static_cast<int>(type);
                                   });
    if (ward != target.effects.active.end() && !target.resistance_used && amount > 0)
    {
        target.resistance_used = true;
        const int reduced = std::min(amount, roll(4));
        amount -= reduced;
        log(target.source.name + "'s Resistance reduces the damage by " + std::to_string(reduced) +
                ".",
            {"{name}'s Resistance reduces the damage by {amount}.",
             {{"name", target.source.name}, {"amount", std::to_string(reduced)}}});
    }
    // The original's Ring of Fire Resistance (MAGIC-4): ordinary fire deals no
    // damage; magical fire 2 less per damage die, but at least 1 per die.
    const bool fire_ward = type == detail::DamageType::fire && def(target).fire_ward && amount > 0;
    if (fire_ward)
    {
        const int warded = ordinary_fire ? 0 : std::max(damage_dice_, amount - 2 * damage_dice_);
        log(target.source.name + "'s Ring of Fire Resistance reduces the Fire damage from " +
                std::to_string(amount) + " to " + std::to_string(warded) + ".",
            {"{name}'s Ring of Fire Resistance reduces the Fire damage from {before} to {after}.",
             {{"name", target.source.name},
              {"before", std::to_string(amount)},
              {"after", std::to_string(warded)}}});
        amount = warded;
    }
    // Oil: Fire damage on an oiled creature deals 5 more, once, from the burning
    // oil, which is ordinary fire.
    if (type == detail::DamageType::fire && amount > 0 &&
        target.oiled_until_round >= static_cast<int>(round_))
    {
        target.oiled_until_round = 0;
        if (!fire_ward)
        {
            amount += 5;
            log("The oil on " + target.source.name + " burns for 5 more damage.",
                {"The oil on {name} burns for 5 more damage.", {{"name", target.source.name}}});
        }
    }
    const std::array parts{detail::DamagePart{type, amount}};
    std::vector<detail::DamageAffinity> added;
    const auto result = detail::resolve_damage(parts, affinities(target, added));
    if (def(target).regeneration && result.total > 0 &&
        (type == detail::DamageType::acid || type == detail::DamageType::fire))
    {
        target.regeneration_blocked = true;
    }
    if (result.total != amount)
    {
        const auto name = std::string(detail::damage_name(type));
        log(target.source.name + ": " + name + " damage " + std::to_string(amount) + " -> " +
                std::to_string(result.total) + ".",
            {"{name}: {type} damage {before} -> {after}.",
             {{"name", target.source.name},
              {"type", name, true},
              {"before", std::to_string(amount)},
              {"after", std::to_string(result.total)}}});
    }
    return result.total;
}

void Session::damage(Actor &target, int amount, bool critical)
{
    if (!amount || target.life.dead || absent(target) ||
        detail::has_effect(target.effects, detail::EffectKind::illusory))
    {
        return;
    }
    amount = endured(target, amount);
    if (!amount)
    {
        return;
    }
    const bool standing = target.life.hp > 0;
    const auto source = pending() ? pending() : actors_[turn_].source.id;
    if (standing && source != target.source.id)
    {
        damage_taken_.push_back({target.source.id, source});
    }
    // Superior Hunter's Prey: the Ranger's damage to its quarry on its own turn.
    if (auto &ranger = actors_[turn_]; source == ranger.source.id && def(ranger).superior_prey &&
                                       !ranger.prey_used && !ranger.prey_origin &&
                                       marked_by(target, ranger))
    {
        ranger.prey_origin = target.source.id;
    }
    const detail::DamageOptions options{
        .critical = critical,
        .dies_at_zero = (target.source.side == Side::opposition || target.turned ||
                         detail::has_effect(target.effects, detail::EffectKind::raised)) &&
                        !def(target).regeneration};
    detail::damage_life(target.life, amount, max_hp(target), options);
    // Contingency: dropping to half Hit Points or to 0 sets off its spell. A
    // copy of the effects, which firing changes, only when one waits.
    if (detail::has_effect(target.effects, detail::EffectKind::contingency))
    {
        for (const auto &e : std::vector<detail::Effect>(target.effects.active))
        {
            if (e.kind == detail::EffectKind::contingency && !target.life.dead &&
                ((e.dc % 4 == 0 && target.life.hp * 2 <= max_hp(target)) ||
                 (e.dc % 4 == 2 && target.life.hp == 0)))
            {
                fire_contingency(target);
            }
        }
    }
    // A summoned creature vanishes at 0 Hit Points; a simulacrum melts away.
    if (target.life.hp == 0 && detail::has_effect(target.effects, detail::EffectKind::summoned))
    {
        target.fled = true;
        log(target.source.name + " vanishes.",
            {"{name} vanishes.", {{"name", target.source.name}}});
        return;
    }
    if (target.life.hp == 0 && !target.life.dead &&
        detail::has_effect(target.effects, detail::EffectKind::simulacrum))
    {
        target.life.dead = true;
        target.life.recovery = {};
        end_concentration(target);
        log(target.source.name + " melts away.",
            {"{name} melts away.", {{"name", target.source.name}}});
        return;
    }
    // Polymorph ends once the Beast form's Temporary Hit Points are gone.
    if (const auto form = std::find_if(target.effects.active.begin(), target.effects.active.end(),
                                       [](const auto &e)
                                       {
                                           return e.kind == detail::EffectKind::polymorphed;
                                       });
        form != target.effects.active.end() && !target.life.temporary_hp.amount)
    {
        const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                         [&](const auto &other)
                                         {
                                             return form->source_scope == scope_ &&
                                                    other.source.id == form->source_actor &&
                                                    other.concentration.active();
                                         });
        if (caster != actors_.end())
        {
            end_concentration(*caster);
        }
        else
        {
            end_polymorph(target);
        }
    }
    // Regeneration: damage never kills it and it makes no death saves; the start
    // of each of its turns decides (regenerate). As in the original game, it may
    // get back up 3d6 rounds after it falls.
    if (def(target).regeneration && target.life.hp == 0)
    {
        target.life.dead = false;
        target.life.successes = target.life.failures = 0;
        target.life.recovery = {detail::death_turn_ms, 0};
        if (standing)
        {
            target.rises_on_round = static_cast<int>(round_) + dice({3, 6, 0});
        }
    }
    // Undead Fortitude: a Zombie dropped to 0 Hit Points saves (DC 5 + the
    // damage) to drop to 1 instead, unless the damage is Radiant or a Critical Hit.
    if (standing && target.life.hp == 0 && def(target).undead_fortitude && !critical &&
        last_damage_type_ != detail::DamageType::radiant &&
        saving_throw_succeeds(target, detail::Ability::constitution, 5 + amount))
    {
        target.life.dead = false;
        target.life.recovery = {};
        (void)detail::heal_life(target.life, 1, max_hp(target), true);
        log(target.source.name + " drops to 1 Hit Point instead (Undead Fortitude).",
            {"{name} drops to 1 Hit Point instead (Undead Fortitude).",
             {{"name", target.source.name}}});
    }
    // Damage is never dealt to the dead, so a dead target died of this blow.
    const bool killed = target.life.dead;
    // Death Ward: the first drop to 0 Hit Points, even a deadly one, leaves 1.
    if (standing && target.life.hp == 0 && target.source.side == Side::party &&
        std::erase_if(target.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::death_ward;
                      }))
    {
        target.life.dead = false;
        target.life.successes = target.life.failures = 0;
        (void)detail::heal_life(target.life, 1, max_hp(target), true);
        log(target.source.name + " drops to 1 Hit Point instead (Death Ward).",
            {"{name} drops to 1 Hit Point instead (Death Ward).", {{"name", target.source.name}}});
    }
    if (killed && target.life.dead)
    {
        fall(target);
    }
    // Dominate Beast: damage lets the creature save again; a success ends the
    // spell. Magic Jar's caster returns to its body then.
    const auto dominated = std::find_if(target.effects.active.begin(), target.effects.active.end(),
                                        [](const auto &e)
                                        {
                                            return e.kind == detail::EffectKind::dominated;
                                        });
    if (const auto *boss = controller(target);
        boss && dominated != target.effects.active.end() && target.life.hp > 0 && !target.life.dead)
    {
        if (saving_throw_succeeds(target, detail::Ability::wisdom, dominated->dc,
                                  def(target).fey_ancestry))
        {
            auto &caster = actor(boss->source.id);
            end_concentration(caster);
            std::erase_if(target.effects.active,
                          [](const auto &e)
                          {
                              return e.kind == detail::EffectKind::dominated;
                          });
            release(target);
            if (std::erase_if(caster.effects.active,
                              [](const auto &e)
                              {
                                  return e.kind == detail::EffectKind::jarred;
                              }))
            {
                log(caster.source.name + "'s soul returns to its body.",
                    {"{name}'s soul returns to its body.", {{"name", caster.source.name}}});
            }
        }
    }
    // Magic Jar: the host's death sends the caster's soul home on a Charisma
    // save against its own spell DC, or kills the caster.
    else if (const auto *jarrer = controller(target);
             jarrer && dominated != target.effects.active.end() && target.life.dead &&
             detail::has_effect(jarrer->effects, detail::EffectKind::jarred))
    {
        auto &caster = actor(jarrer->source.id);
        std::erase_if(caster.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::jarred;
                      });
        if (saving_throw_succeeds(caster, detail::Ability::charisma, spell_dc(caster, "magic_jar")))
        {
            log(caster.source.name + "'s soul returns to its body.",
                {"{name}'s soul returns to its body.", {{"name", caster.source.name}}});
        }
        else
        {
            caster.life.hp = 0;
            caster.life.dead = true;
            caster.life.recovery = {};
            log(caster.source.name + " dies with its host.",
                {"{name} dies with its host.", {{"name", caster.source.name}}});
            fall(caster);
        }
    }
    // Relentless Rage: a raging Barbarian dropped to 0 Hit Points but not
    // killed outright makes a Constitution save, DC 10 and 5 more for each
    // earlier try since its last rest; success leaves twice its level in Hit
    // Points. It is always tried, as Relentless Endurance is.
    if (standing && target.life.hp == 0 && !target.life.dead && def(target).relentless_rage &&
        target.lay_on_hands[CharacterClass::barbarian] > 0 && rage_of(target))
    {
        const int dc =
            10 + 5 * (def(target).relentless_rage - target.lay_on_hands[CharacterClass::barbarian]);
        --target.lay_on_hands[CharacterClass::barbarian];
        log(target.source.name + " tries Relentless Rage.",
            {"{name} tries Relentless Rage.", {{"name", target.source.name}}});
        if (saving_throw_succeeds(target, detail::Ability::constitution, dc))
        {
            (void)detail::heal_life(target.life,
                                    2 * def(target).class_levels[CharacterClass::barbarian],
                                    max_hp(target), true);
            log(target.source.name + " fights on with " + std::to_string(target.life.hp) +
                    " Hit Points (Relentless Rage).",
                {"{name} fights on with {hp} Hit Points (Relentless Rage).",
                 {{"name", target.source.name}, {"hp", std::to_string(target.life.hp)}}});
        }
    }
    // Relentless Endurance: an Orc dropped to 0 Hit Points but not killed
    // outright drops to 1 instead, once per Long Rest (always taken, AUTO-1).
    if (standing && target.life.hp == 0 && !target.life.dead && def(target).relentless &&
        !detail::has_effect(target.effects, detail::EffectKind::relentless_used) &&
        detail::can_apply(target.effects))
    {
        // Not healing (Chill Touch cannot stop it); the dying clock is cleared.
        (void)detail::heal_life(target.life, 1, max_hp(target), true);
        detail::apply_spell_benefit(target.effects, effect_source(target), target.source.name,
                                    detail::EffectKind::relentless_used, 0);
        log(target.source.name + " endures: Relentless Endurance leaves 1 Hit Point.",
            {"{name} endures: Relentless Endurance leaves 1 Hit Point.",
             {{"name", target.source.name}}});
    }
    if (standing && target.life.hp == 0 && !target.life.dead)
    {
        protect(target);
    }
    if (standing && target.life.hp == 0)
    {
        bless_fiends(target);
    }
    // Warding Bond: the caster takes the same damage; the bond ends when the
    // caster drops to 0 Hit Points or is more than 60 feet away.
    for (const auto bond : bonds_on(target))
    {
        auto &caster = actor(bond);
        log(caster.source.name + " shares " + std::to_string(amount) +
                " damage through Warding Bond.",
            {"{name} shares {damage} damage through Warding Bond.",
             {{"name", caster.source.name}, {"damage", std::to_string(amount)}}});
        damage(caster, amount, false);
    }
    if (target.life.hp == 0)
    {
        for (auto &other : actors_)
        {
            std::erase_if(other.effects.active,
                          [&](const auto &e)
                          {
                              return e.kind == detail::EffectKind::warding_bond &&
                                     e.source_scope == scope_ && e.source_actor == target.source.id;
                          });
        }
    }
    // Damage tests Concentration; dropping to 0 Hit Points ends it.
    // Relentless Hunter (Ranger 13): damage cannot break Hunter's Mark.
    const bool relentless_mark = def(target).relentless_hunter && target.life.hp > 0 &&
                                 !target.life.dead && holds_hunters_mark(target);
    if (target.concentration.active() && !relentless_mark)
    {
        const bool rolls = target.life.hp > 0 && !target.life.dead;
        auto modifiers = detail::saving_modifiers(detail::Ability::constitution,
                                                  def(target).str_dex_disadvantage, target.dodge);
        modifiers.advantage |= detail::has_effect(target.effects, detail::EffectKind::extended) ||
                               def(target).eldritch_mind;
        const auto held = target.concentration.active();
        auto result = target.concentration.damage(
            amount,
            def(target).saves[2] + aura_of_protection(target) + (rolls ? blessing_die(target) : 0),
            modifiers, target.life.hp == 0 || target.life.dead, rng_, target.effects.active);
        if (result.save)
        {
            log_save(target, *result.save);
        }
        if (result.inspiration)
        {
            detail::spend_inspiration(target.effects);
            log_inspiration(target, result.inspiration, result.save->success);
        }
        // Dark One's Own Luck may still keep the Concentration.
        if (result.ended && result.save && !result.save->success)
        {
            dark_ones_luck(target, *result.save);
            if (result.save->success)
            {
                (void)target.concentration.start(*held);
                result.ended.reset();
            }
        }
        if (result.ended)
        {
            drop_concentration_effects(target);
        }
    }
    if (target.life.hp == 0)
    {
        target.effects.prone = true;
        end_wild_shape(target);
        end_incapacitated_effects(target);
    }
    // Damage ends Turn Undead, Sleep, Charm Person, Abjure Foes and Hypnotic
    // Pattern on the creature. Adaptation: any damage ends the charm, not
    // only the charmer's side's.
    std::erase_if(target.effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::turned ||
                             e.kind == detail::EffectKind::drowsy ||
                             e.kind == detail::EffectKind::asleep ||
                             e.kind == detail::EffectKind::charmed ||
                             e.kind == detail::EffectKind::knocked_out ||
                             e.kind == detail::EffectKind::abjured ||
                             e.kind == detail::EffectKind::entranced;
                  });
    // Hideous Laughter: damage calls for a new Wisdom save, with Advantage.
    for (std::size_t n = 0; n < target.effects.active.size() && target.life.hp > 0; ++n)
    {
        if (target.effects.active[n].kind == detail::EffectKind::laughing &&
            saving_throw_succeeds(
                target, detail::Ability::wisdom, target.effects.active[n].dc, true,
                detail::heightened_against(target.effects, target.effects.active[n].source_scope,
                                           target.effects.active[n].source_actor,
                                           "hideous_laughter")))
        {
            target.effects.active.erase(target.effects.active.begin() +
                                        static_cast<std::ptrdiff_t>(n));
            log(target.source.name + " stops laughing.",
                {"{name} stops laughing.", {{"name", target.source.name}}});
            break;
        }
    }
    // Sacred Weapon ends when its wielder is Incapacitated.
    if (target.life.hp == 0)
    {
        std::erase_if(target.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::sacred_weapon;
                      });
    }
    if (target.life.hp == 0 && !target.life.dead && !target.life.stable)
    {
        target.life.recovery.death_save_in_ms = next_turn_ms(target);
    }
    if (target.life.hp == 0)
    {
        target.dodge = false;
        if (!target.life.dead && shares_occupied_space(target))
        {
            target.involuntary_overlap = true;
        }
        log(target.source.name + (target.life.dead ? " is defeated." : " falls unconscious."),
            {target.life.dead ? "{name} is defeated." : "{name} falls unconscious.",
             {{"name", target.source.name}}});
    }
    clear_departed_overlaps();
}

int Session::heal(Actor &target, int amount)
{
    if (target.life.hp == 0 && shares_occupied_space(target))
    {
        target.involuntary_overlap = true;
    }
    const bool was_unconscious = target.life.hp == 0;
    const int restored = detail::heal_life(target.life, amount, max_hp(target),
                                           !detail::healing_blocked(target.effects));
    if (was_unconscious && restored)
    {
        target.effects.prone = true;
    }
    log(target.source.name + " recovers " + std::to_string(restored) + " HP.",
        {"{name} recovers {hp} HP.",
         {{"name", target.source.name}, {"hp", std::to_string(restored)}}});
    return restored;
}

int Session::healing_roll(const Actor &target, detail::DamageDice rolled)
{
    // Beacon of Hope: healing restores the most its dice can.
    if (!detail::has_effect(target.effects, detail::EffectKind::hopeful))
    {
        return dice(rolled);
    }
    log(target.source.name + "'s healing is maximized (Beacon of Hope).",
        {"{name}'s healing is maximized (Beacon of Hope).", {{"name", target.source.name}}});
    return rolled.count * rolled.sides + rolled.bonus;
}

bool Session::fought_advantage(const detail::SpellDef &spell) noexcept
{
    // Charm Person and the Dominate spells: Advantage on the save while the
    // caster's side fights the creature, which in combat is always.
    return spell.rider == detail::Rider::charm_person ||
           spell.rider == detail::Rider::dominate_beast ||
           spell.rider == detail::Rider::dominate_person ||
           spell.rider == detail::Rider::dominate_monster;
}

int Session::armor_class(const Actor &target) const
{
    // Smite of Protection: +2, as Half Cover gives (LEVELS-2).
    return armor_class_of(target) + (smite_protected(target) ? 2 : 0);
}

std::span<const detail::DamageAffinity>
Session::affinities(const Actor &target, std::vector<detail::DamageAffinity> &added) const
{
    auto &result = added;
    result.clear();
    if (detail::has_effect(target.effects, detail::EffectKind::warding_bond))
    {
        result.push_back({detail::AffinityKind::resistance, std::nullopt, "spell:warding_bond"});
    }
    if (detail::has_effect(target.effects, detail::EffectKind::protection_from_poison))
    {
        result.push_back({detail::AffinityKind::resistance, detail::DamageType::poison,
                          "spell:protection_from_poison"});
    }
    for (const auto &e : target.effects.active)
    {
        if (e.kind == detail::EffectKind::energy_ward)
        {
            result.push_back({detail::AffinityKind::resistance,
                              static_cast<detail::DamageType>(e.dc),
                              "spell:protection_from_energy"});
        }
        else if (e.kind == detail::EffectKind::defending)
        {
            result.push_back({detail::AffinityKind::resistance,
                              static_cast<detail::DamageType>(e.dc),
                              "feature:superior_hunters_defense"});
        }
    }
    if (detail::has_effect(target.effects, detail::EffectKind::feasted))
    {
        result.push_back(
            {detail::AffinityKind::resistance, detail::DamageType::poison, "spell:heroes_feast"});
    }
    // Nature's Sanctuary: an ally among its trees shares the Druid's Nature's
    // Ward Resistance.
    for (const auto &zone : zones_)
    {
        if (const auto *druid = find_actor(zone.caster);
            zone.kind == ZoneKind::sanctuary && druid && druid->source.side == target.source.side &&
            in_zone(zone, target))
        {
            for (const auto &ward : def(*druid).affinities)
            {
                if (ward.source_id == "feature:natures_ward")
                {
                    result.push_back(
                        {detail::AffinityKind::resistance, ward.type, "feature:natures_sanctuary"});
                }
            }
        }
    }
    // Mind Blank: Immunity to Psychic damage.
    if (detail::has_effect(target.effects, detail::EffectKind::mind_blank))
    {
        result.push_back(
            {detail::AffinityKind::immunity, detail::DamageType::psychic, "spell:mind_blank"});
    }
    // Petrified: Resistance to every damage.
    if (detail::has_effect(target.effects, detail::EffectKind::petrified))
    {
        result.push_back({detail::AffinityKind::resistance, std::nullopt, "condition:petrified"});
    }
    // Summon Dragon's shared Resistance.
    for (const auto &e : target.effects.active)
    {
        if (e.kind == detail::EffectKind::shared_resistance)
        {
            result.push_back({detail::AffinityKind::resistance,
                              static_cast<detail::DamageType>(e.dc), "spell:summon_dragon"});
        }
    }
    // Fire Shield: the warm shield resists Cold, the chill one Fire.
    for (const auto &e : target.effects.active)
    {
        if (e.kind == detail::EffectKind::fire_shield)
        {
            result.push_back({detail::AffinityKind::resistance,
                              e.dc ? detail::DamageType::fire : detail::DamageType::cold,
                              "spell:fire_shield"});
        }
    }
    if (detail::has_effect(target.effects, detail::EffectKind::stoneskin))
    {
        for (const auto type : {detail::DamageType::bludgeoning, detail::DamageType::piercing,
                                detail::DamageType::slashing})
        {
            result.push_back({detail::AffinityKind::resistance, type, "spell:stoneskin"});
        }
    }
    if (rage_of(target))
    {
        for (const auto type : {detail::DamageType::bludgeoning, detail::DamageType::piercing,
                                detail::DamageType::slashing})
        {
            result.push_back({detail::AffinityKind::resistance, type, "feature:rage"});
        }
    }
    if (silenced(target.source.cell))
    {
        result.push_back(
            {detail::AffinityKind::immunity, detail::DamageType::thunder, "spell:silence"});
    }
    if (in_aura_of_life(target))
    {
        result.push_back(
            {detail::AffinityKind::resistance, detail::DamageType::necrotic, "spell:aura_of_life"});
    }
    const auto &own = def(target).affinities;
    if (result.empty())
    {
        return own;
    }
    // The creature's own first, as they always came.
    result.insert(result.begin(), own.begin(), own.end());
    return result;
}

std::vector<EntityId> Session::bonds_on(const Actor &target) const
{
    std::vector<EntityId> casters;
    for (const auto &e : target.effects.active)
    {
        if (e.kind == detail::EffectKind::warding_bond && e.source_scope == scope_)
        {
            for (const auto &caster : actors_)
            {
                if (caster.source.id == e.source_actor && caster.life.hp > 0 && !caster.life.dead &&
                    caster.source.id != target.source.id && distance(caster, target) <= 60)
                {
                    casters.push_back(caster.source.id);
                }
            }
        }
    }
    return casters;
}

const detail::Effect *Session::rage_of(const Actor &a) noexcept
{
    const auto found = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                    [](const auto &e)
                                    {
                                        return e.kind == detail::EffectKind::raging;
                                    });
    return found == a.effects.active.end() ? nullptr : &*found;
}

unsigned Session::rage_duration(const Actor &a) const
{
    // Until the end of the Barbarian's next turn, counted from its current one.
    return next_save_ms(a.source.id) + detail::round_ms;
}

void Session::extend_rage(Actor &a)
{
    for (auto &e : a.effects.active)
    {
        if (e.kind == detail::EffectKind::raging)
        {
            e.remaining_ms = rage_duration(a);
        }
    }
}

bool Session::invisible(const Actor &a) noexcept
{
    return detail::has_effect(a.effects, detail::EffectKind::invisible) ||
           detail::has_effect(a.effects, detail::EffectKind::veiled) ||
           detail::has_effect(a.effects, detail::EffectKind::greater_invisible);
}

void Session::end_invisibility(EntityId id)
{
    auto &a = actor(id);
    if (std::erase_if(a.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::invisible;
                      }))
    {
        log(a.source.name + " is no longer Invisible.",
            {"{name} is no longer Invisible.", {{"name", a.source.name}}});
    }
}

bool Session::can_hide(const Actor &a) const
{
    if (hidden(a) || a.source.character_profile.empty())
    {
        return false;
    }
    // Naturally Stealthy: a creature at least one size larger also blocks
    // the view (the viewer itself aside).
    return std::none_of(
        actors_.begin(), actors_.end(),
        [&](const auto &enemy)
        {
            if (enemy.source.side == a.source.side || !conscious(enemy) ||
                obscured(enemy, a.source.cell))
            {
                return false;
            }
            auto board = board_;
            for (const auto &other : actors_)
            {
                if (def(a).naturally_stealthy && !other.life.dead &&
                    other.source.id != a.source.id && other.source.id != enemy.source.id &&
                    def(other).size > def(a).size)
                {
                    for (const auto square : squares_of(other))
                    {
                        board
                            .terrain[static_cast<std::size_t>(square.y * board_.width + square.x)] =
                            Terrain::obstacle;
                    }
                }
            }
            // From any of the enemy's squares (FOOT-1).
            const auto eyes = squares_of(enemy);
            return std::any_of(eyes.begin(), eyes.end(),
                               [&](Cell eye)
                               {
                                   return !obscured(enemy, eye) &&
                                          detail::has_line_of_sight(board, eye, a.source.cell);
                               });
        });
}

void Session::hide(Actor &a)
{
    // Armor that hinders stealth, Frightened and Poisoned give the check
    // Disadvantage.
    constexpr int dc = 15;
    const auto &d = def(a);
    bool lucky_reroll = false;
    const bool hindered = d.stealth_disadvantage || check_disadvantage(a);
    int natural = detail::d20({d.stealth_advantage, hindered, d.lucky}, rng_, &lucky_reroll);
    log_luck(a, lucky_reroll);
    natural = reliable(a, "stealth", natural);
    int total = natural + d.stealth;
    // Elvenkind boots or cloak give the check Advantage; with Disadvantage too
    // both cancel.
    const std::string disadvantage = hindered && !d.stealth_advantage   ? " (disadvantage)"
                                     : d.stealth_advantage && !hindered ? " (advantage)"
                                                                        : "";
    log(a.source.name + " Stealth check: d20 " + std::to_string(natural) + disadvantage + " + " +
            std::to_string(d.stealth) + " vs DC 15.",
        {"{name} Stealth check: d20 {roll}{disadvantage} + {bonus} vs DC {dc}.",
         {{"name", a.source.name},
          {"roll", std::to_string(natural)},
          {"disadvantage", disadvantage, true},
          {"bonus", std::to_string(d.stealth)},
          {"dc", std::to_string(dc)}}});
    if (total < dc && d.stealth + 20 >= dc)
    {
        if (const auto again = heroic_reroll(a))
        {
            total = *again + d.stealth;
        }
    }
    if (total < dc)
    {
        total += inspiration_roll(a, dc - total);
    }
    if (total < dc)
    {
        total += peerless_skill(a, dc - total);
    }
    if (total < dc)
    {
        total += dark_luck_check(a, dc - total);
    }
    if (total < dc)
    {
        log(a.source.name + " fails to hide.",
            {"{name} fails to hide.", {{"name", a.source.name}}});
        return;
    }
    detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                detail::EffectKind::hidden, std::min(total, 40));
    log(a.source.name + " is hidden (Stealth " + std::to_string(total) + ").",
        {"{name} is hidden (Stealth {total}).",
         {{"name", a.source.name}, {"total", std::to_string(total)}}});
}

void Session::search(Actor &searcher)
{
    const auto &d = def(searcher);
    bool lucky_reroll = false;
    const int natural =
        detail::d20({false, check_disadvantage(searcher), d.lucky}, rng_, &lucky_reroll);
    log_luck(searcher, lucky_reroll);
    int bonus = d.perception;
    // An enemy Lore Bard's Cutting Words may keep the best-hidden creature the
    // search would find hidden.
    int margin = 6;
    for (const auto &other : actors_)
    {
        if (other.source.side != searcher.source.side && hidden(other) &&
            clear_view(searcher, other) && natural + bonus >= stealth_total(other))
        {
            margin = std::min(margin, natural + bonus - stealth_total(other));
        }
    }
    bonus -= cutting_words(searcher, margin, CutRoll::check);
    const int total = natural + bonus;
    log(searcher.source.name + " searches: Perception d20 " + std::to_string(natural) + " + " +
            std::to_string(bonus) + " = " + std::to_string(total) + ".",
        {"{name} searches: Perception d20 {roll} + {bonus} = {total}.",
         {{"name", searcher.source.name},
          {"roll", std::to_string(natural)},
          {"bonus", std::to_string(bonus)},
          {"total", std::to_string(total)}}});
    // A Cloak of Elvenkind gives the check Disadvantage against its wearer:
    // the lower of the two d20s counts against it.
    std::optional<int> second;
    for (auto &other : actors_)
    {
        if (other.source.side != searcher.source.side && hidden(other) &&
            clear_view(searcher, other))
        {
            int against = total;
            if (def(other).hard_to_perceive && !check_disadvantage(searcher))
            {
                if (!second)
                {
                    second = detail::d20({false, false, d.lucky}, rng_);
                    log(searcher.source.name + " rolls again against a Cloak of Elvenkind: d20 " +
                            std::to_string(*second) + ".",
                        {"{name} rolls again against a Cloak of Elvenkind: d20 {roll}.",
                         {{"name", searcher.source.name}, {"roll", std::to_string(*second)}}});
                }
                against = std::min(natural, *second) + bonus;
            }
            if (against >= stealth_total(other))
            {
                find_hidden(other, searcher);
            }
        }
    }
}

void Session::notice_hidden()
{
    for (auto &hider : actors_)
    {
        for (const auto &enemy : actors_)
        {
            // Disadvantage against a Cloak of Elvenkind takes 5 from Passive
            // Perception.
            const int passive = 10 + def(enemy).perception - (def(hider).hard_to_perceive ? 5 : 0);
            if (hidden(hider) && enemy.source.side != hider.source.side &&
                clear_view(enemy, hider) && passive >= stealth_total(hider))
            {
                find_hidden(hider, enemy);
            }
        }
    }
}

void Session::find_hidden(Actor &hider, const Actor &finder)
{
    std::erase_if(hider.effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::hidden;
                  });
    log(finder.source.name + " finds " + hider.source.name + ".",
        {"{finder} finds {name}.", {{"finder", finder.source.name}, {"name", hider.source.name}}});
}

void Session::end_hiding(EntityId id)
{
    auto &a = actor(id);
    if (std::erase_if(a.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::hidden;
                      }))
    {
        log(a.source.name + " is no longer hidden.",
            {"{name} is no longer hidden.", {{"name", a.source.name}}});
    }
}

void Session::end_sanctuary(Actor &a)
{
    std::erase_if(a.effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::sanctuary;
                  });
}

bool Session::sanctuary_stops(Actor &attacker, const Actor &target)
{
    end_sanctuary(attacker);
    const auto ward = std::find_if(target.effects.active.begin(), target.effects.active.end(),
                                   [](const auto &e)
                                   {
                                       return e.kind == detail::EffectKind::sanctuary;
                                   });
    if (ward == target.effects.active.end() || attacker.source.side == target.source.side ||
        saving_throw_succeeds(attacker, detail::Ability::wisdom, ward->dc))
    {
        return false;
    }
    log(attacker.source.name + "'s attack on " + target.source.name + " is lost to Sanctuary.",
        {"{name}'s attack on {target} is lost to Sanctuary.",
         {{"name", attacker.source.name}, {"target", target.source.name}}});
    return true;
}

void Session::begin_concentration(Actor &caster, const detail::SpellDef &spell)
{
    // A new Concentration spell ends the old one first, removing its effects.
    // Extended Spell doubles it and gives Advantage on the saves to keep it.
    end_concentration(caster);
    const bool extended = casting_with(Metamagic::extended);
    caster.concentration.start(
        {{scope_, 1, caster.source.id},
         detail::benefit_duration_ms(rider_effect(spell.rider)) * (extended ? 2 : 1)});
    if (extended)
    {
        detail::apply_spell_benefit(caster.effects, effect_source(caster), caster.source.name,
                                    detail::EffectKind::extended, 0);
    }
}

void Session::end_concentration(Actor &caster)
{
    if (caster.concentration.stop())
    {
        drop_concentration_effects(caster);
    }
}

void Session::end_incapacitated_effects(Actor &a)
{
    end_concentration(a);
    for (auto &other : actors_)
    {
        std::erase_if(other.effects.active,
                      [&](const auto &e)
                      {
                          return e.kind == detail::EffectKind::turned &&
                                 e.source_actor == a.source.id && e.source_scope == scope_;
                      });
    }
}

void Session::drop_concentration_effects(const Actor &caster)
{
    // Delayed Blast Fireball's bead bursts as the spell ends.
    std::vector<Zone> beads;
    std::copy_if(zones_.begin(), zones_.end(), std::back_inserter(beads),
                 [&](const auto &zone)
                 {
                     return zone.kind == ZoneKind::bead && zone.caster == caster.source.id;
                 });
    std::erase_if(zones_,
                  [&](const auto &zone)
                  {
                      return zone.caster == caster.source.id && !zone.ends_ms;
                  });
    // A Magic Circle or Animate Dead whose casting is broken fails, but keeps
    // its slot.
    for (auto &e : actor(caster.source.id).effects.active)
    {
        if (e.kind == detail::EffectKind::circling)
        {
            ++actor(caster.source.id).slots[static_cast<unsigned>(e.dc)];
            log("The Magic Circle fails; the spell slot is kept.",
                {"The Magic Circle fails; the spell slot is kept.", {}});
        }
        else if (e.kind == detail::EffectKind::animating)
        {
            // Create Undead's slot level is 10 more.
            ++actor(caster.source.id).slots[static_cast<unsigned>(e.dc % 10)];
            log("The undead fail to rise; the spell slot is kept.",
                {"The undead fail to rise; the spell slot is kept.", {}});
        }
    }
    for (const auto &other : actors_)
    {
        for (const auto &e : other.effects.active)
        {
            if (e.kind == detail::EffectKind::knitting && e.source_actor == caster.source.id &&
                e.source_scope == scope_)
            {
                ++actor(caster.source.id).slots[static_cast<unsigned>(e.dc)];
                log("Regenerate fails; the spell slot is kept.",
                    {"{spell} fails; the spell slot is kept.", {{"spell", "Regenerate", true}}});
            }
        }
    }
    // Banishment ends early: its creatures come back, as Maze's do. Reverse
    // Gravity's fall back down.
    std::vector<EntityId> banished, lost, lifted;
    for (const auto &other : actors_)
    {
        for (const auto &e : other.effects.active)
        {
            if (e.source_actor == caster.source.id && e.source_scope == scope_)
            {
                if (e.kind == detail::EffectKind::banished)
                {
                    banished.push_back(other.source.id);
                }
                else if (e.kind == detail::EffectKind::mazed)
                {
                    lost.push_back(other.source.id);
                }
                else if (e.kind == detail::EffectKind::aloft)
                {
                    lifted.push_back(other.source.id);
                }
            }
        }
    }
    // Haste's end brings its lethargy; Polymorph's, the creature's own form.
    std::vector<EntityId> hastened, transformed;
    for (const auto &other : actors_)
    {
        for (const auto &e : other.effects.active)
        {
            if (e.kind == detail::EffectKind::polymorphed && e.source_actor == caster.source.id &&
                e.source_scope == scope_)
            {
                transformed.push_back(other.source.id);
            }
        }
    }
    for (const auto &other : actors_)
    {
        for (const auto &e : other.effects.active)
        {
            if (e.kind == detail::EffectKind::hasted && e.source_actor == caster.source.id &&
                e.source_scope == scope_)
            {
                hastened.push_back(other.source.id);
            }
        }
    }
    // A curse from a level-5 slot is not held by Concentration.
    for (auto &other : actors_)
    {
        for (auto *effects : {&other.effects.active, &other.suppressed})
        {
            std::erase_if(*effects,
                          [&](const auto &e)
                          {
                              return concentration_effect(e.kind) &&
                                     e.source_actor == caster.source.id &&
                                     e.source_scope == scope_ &&
                                     !(e.kind == detail::EffectKind::cursed && e.dc >= 10) &&
                                     !(e.kind == detail::EffectKind::petrified && e.dc);
                          });
        }
    }
    log(caster.source.name + " loses Concentration.",
        {"{name} loses Concentration.", {{"name", caster.source.name}}});
    for (const auto id : banished)
    {
        return_from_banishment(actor(id));
    }
    for (const auto id : lost)
    {
        auto &returned = actor(id);
        returned.source.cell = nearest_open(returned);
        log(returned.source.name + " returns from the maze.",
            {"{name} returns from the maze.", {{"name", returned.source.name}}});
    }
    for (const auto id : lifted)
    {
        if (!actor(id).life.dead)
        {
            log(actor(id).source.name + " falls back down.",
                {"{name} falls back down.", {{"name", actor(id).source.name}}});
            fall_hard(actor(id));
        }
    }
    for (const auto &bead : beads)
    {
        explode_bead(actor(caster.source.id), bead);
    }
    for (const auto id : hastened)
    {
        tire(actor(id));
    }
    for (const auto id : transformed)
    {
        end_polymorph(actor(id));
    }
    // An Antimagic Field that ended gives back what it held.
    suppress_spells();
}

void Session::burn_searing_smites(Actor &a)
{
    // At the start of each of its turns the target burns, then saves to end it.
    for (std::size_t n = 0; n < a.effects.active.size() && a.life.hp > 0; ++n)
    {
        if (a.effects.active[n].kind != detail::EffectKind::searing_smite)
        {
            continue;
        }
        const int dc = a.effects.active[n].dc;
        // 1d6, and 1d6 more for each slot level above 1.
        const int burn = static_cast<int>(std::max(1u, a.effects.active[n].slot));
        const int amount = resolved_damage(a, detail::DamageType::fire, dice({burn, 6, 0}));
        log(a.source.name + " burns for " + std::to_string(amount) + " Fire damage.",
            {"{name} burns for {damage} Fire damage.",
             {{"name", a.source.name}, {"damage", std::to_string(amount)}}});
        damage(a, amount, false);
        if (a.life.hp > 0 && saving_throw_succeeds(a, detail::Ability::constitution, dc))
        {
            a.effects.active[n].remaining_ms = 0;
        }
    }
    std::erase_if(a.effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::searing_smite && !e.remaining_ms;
                  });
}

detail::Mastery Session::weapon_mastery(const Actor &a, AttackRange range) const
{
    return weapon_mastery(a, def(a), range);
}

detail::Mastery Session::weapon_mastery(const Actor &a, const Definition &d,
                                        AttackRange range) const
{
    if (d.masteries.empty() || (range == AttackRange::melee && d.ranged_weapon))
    {
        return detail::Mastery::none;
    }
    for (const auto &key : d.equipment_keys)
    {
        if (const auto *weapon = detail::weapon(key))
        {
            if (std::find(d.masteries.begin(), d.masteries.end(), weapon->key) == d.masteries.end())
            {
                return detail::Mastery::none;
            }
            return tactical_mastery_ && tactical_mastery_->actor == a.source.id
                       ? tactical_mastery_->kind
                       : weapon->mastery;
        }
    }
    return detail::Mastery::none;
}

bool Session::mastery_capacity(const Actor &a, const Definition &d, const Actor &target,
                               AttackRange range) const
{
    const auto kind = weapon_mastery(a, d, range);
    if (kind != detail::Mastery::sap && kind != detail::Mastery::vex)
    {
        return true;
    }
    return detail::can_apply_attack_mastery(target.effects,
                                            kind == detail::Mastery::sap ? detail::EffectKind::sap
                                                                         : detail::EffectKind::vex,
                                            effect_source(a));
}

detail::RollModifiers Session::attack_modifiers(const Actor &a, const Actor &target,
                                                AttackRange range, AttackSource source) const
{
    const bool ranged = range == AttackRange::ranged;
    const bool spell = source == AttackSource::spell;
    const auto &d = def(a);
    bool disadvantaged =
        !spell && (d.str_dex_disadvantage ||
                   (ranged ? d.ranged_heavy_disadvantage : d.melee_heavy_disadvantage));
    if (ranged)
    {
        if (!spell && distance(a, target) > d.range)
        {
            disadvantaged = true;
        }
        for (const auto &other : actors_)
        {
            if (other.source.side != a.source.side && conscious(other) && distance(a, other) <= 5 &&
                can_see(other, a))
            {
                disadvantaged = true;
            }
        }
    }
    // Fog hides either creature from the other, as Blinded would.
    const bool attacker_fogged = obscured(a, a.source.cell) || obscured(a, target.source.cell);
    const bool target_fogged =
        obscured(target, a.source.cell) || obscured(target, target.source.cell);
    auto result = detail::attack_modifiers(detail::AttackConditions{
        .attacker_blind = detail::blinded(a.effects) || attacker_fogged || unseen(a, target),
        .target_blind = detail::blinded(target.effects) || target_fogged || unseen(target, a),
        .target_dodging = target.dodge,
        .other_disadvantage = disadvantaged || a.effects.prone});
    // At zero HP the creature is Unconscious and Prone (SRD pp.187,191).
    // At longer range their opposing attack modifiers cancel, not stack.
    const bool pack_tactics = !spell && d.pack_tactics && ally_beside(a, target);
    // A Boar's Bloodied Fury: Advantage at half its Hit Points or fewer.
    if (!spell && d.bloodied_fury && a.life.hp * 2 <= max_hp(a))
    {
        result.advantage = true;
    }
    if (helpless(target) || a.aim_ready || pack_tactics ||
        detail::has_effect(a.effects, detail::EffectKind::poised) ||
        detail::vexed_by(target.effects, effect_source(a)) ||
        detail::studied_by(target.effects, effect_source(a)))
    {
        result.advantage = true;
    }
    // Phantasmal Killer: Disadvantage on attack rolls.
    if (detail::has_effect(a.effects, detail::EffectKind::haunted))
    {
        result.disadvantage = true;
    }
    // Charging in panic: it strikes wildly and leaves itself open (MORALE-2).
    if (a.panicked && a.must_stay)
    {
        result.disadvantage = true;
    }
    if (target.panicked && target.must_stay)
    {
        result.advantage = true;
    }
    if (detail::sapped(a.effects) || suffers_poisoned(a) ||
        detail::has_effect(a.effects, detail::EffectKind::scorched) || !fears_in_sight(a).empty() ||
        in_sunlight(a))
    {
        result.disadvantage = true;
    }
    // Ray of Enfeeblement: Disadvantage on Strength-based attack rolls, here
    // every melee weapon attack.
    if (!spell && !ranged && detail::has_effect(a.effects, detail::EffectKind::enfeebled))
    {
        result.disadvantage = true;
    }
    // Dispel Evil and Good: such creatures attack its caster with Disadvantage.
    if (detail::has_effect(target.effects, detail::EffectKind::dispel_evil) && otherworldly(a))
    {
        result.disadvantage = true;
    }
    // Bestow Curse: Disadvantage on attack rolls against its caster.
    if (const auto *curse = curse_on(a, 6); curse && curse->source_actor == target.source.id)
    {
        result.disadvantage = true;
    }
    // Reckless Attack: Advantage on the attacker's Strength attack rolls and on
    // attack rolls against it.
    if ((!spell && detail::has_effect(a.effects, detail::EffectKind::reckless) &&
         strength_attack(a, range)) ||
        detail::has_effect(target.effects, detail::EffectKind::reckless))
    {
        result.advantage = true;
    }
    // Magic Circle: its creature type attacks those within it with Disadvantage.
    if (std::any_of(zones_.begin(), zones_.end(),
                    [&](const auto &zone)
                    {
                        return zone.kind == ZoneKind::circle && zone.ends_ms &&
                               def(a).creature_type == detail::ward_types[zone.warded] &&
                               in_zone(zone, target);
                    }))
    {
        result.disadvantage = true;
    }
    // Escape the Horde: Opportunity Attacks against the Ranger have
    // Disadvantage. Multiattack Defense: so do the other attacks this turn of
    // a creature that hit it.
    if ((opportunity_ && def(target).escape_the_horde) ||
        std::any_of(target.effects.active.begin(), target.effects.active.end(),
                    [&](const auto &e)
                    {
                        return e.kind == detail::EffectKind::defended &&
                               e.source_actor == a.source.id && e.source_scope == scope_;
                    }))
    {
        result.disadvantage = true;
    }
    // Faerie Fire: Advantage against an outlined creature the attacker sees.
    if (detail::has_effect(target.effects, detail::EffectKind::outlined) && can_see(a, target))
    {
        result.advantage = true;
    }
    // Shining Smite: Advantage against the shining creature.
    if (detail::has_effect(target.effects, detail::EffectKind::shining))
    {
        result.advantage = true;
    }
    // Innate Sorcery: Advantage on spell attack rolls.
    if (spell && detail::has_effect(a.effects, detail::EffectKind::innate_sorcery))
    {
        result.advantage = true;
    }
    // Blur: attack rolls against the creature have Disadvantage.
    if (detail::has_effect(target.effects, detail::EffectKind::blur))
    {
        result.disadvantage = true;
    }
    // Irresistible Dance: a dancer attacks with Disadvantage and is attacked
    // with Advantage; a Petrified creature is attacked with Advantage.
    if (detail::has_effect(a.effects, detail::EffectKind::dancing))
    {
        result.disadvantage = true;
    }
    if (detail::has_effect(target.effects, detail::EffectKind::dancing) ||
        detail::has_effect(target.effects, detail::EffectKind::petrified))
    {
        result.advantage = true;
    }
    // Holy Aura: attackers have Disadvantage against the creatures it wards.
    if (holy_aura(target))
    {
        result.disadvantage = true;
    }
    // Guiding Bolt: the next attack roll against the target has Advantage.
    if (detail::has_effect(target.effects, detail::EffectKind::guiding_bolt))
    {
        result.advantage = true;
    }
    // Stunned: attack rolls against it have Advantage.
    if (detail::stunned(target.effects))
    {
        result.advantage = true;
    }
    // Restrained: attacks against it have Advantage, its own have Disadvantage.
    if (detail::restrained(target.effects))
    {
        result.advantage = true;
    }
    if (detail::restrained(a.effects))
    {
        result.disadvantage = true;
    }
    // Protection from Evil and Good guards against these creature types.
    const auto &type = def(a).creature_type;
    if (detail::has_effect(target.effects, detail::EffectKind::protection_from_evil_and_good) &&
        (type == "aberration" || type == "celestial" || type == "elemental" || type == "fey" ||
         type == "fiend" || type == "undead"))
    {
        result.disadvantage = true;
    }
    if (target.effects.prone || target.life.hp == 0)
    {
        if (distance(a, target) <= 5)
        {
            result.advantage = true;
        }
        else
        {
            result.disadvantage = true;
        }
    }
    result.lucky = d.lucky;
    return result;
}

detail::DamageDieRule Session::weapon_die_rule(const Actor &a, AttackRange range) const
{
    const auto &d = def(a);
    return d.great_weapon_fighting && range == AttackRange::melee && !d.ranged_weapon &&
                   !d.weapon_label.empty() && d.weapon_hands == 2
               ? detail::DamageDieRule::great_weapon_fighting
               : detail::DamageDieRule::normal;
}

Dice Session::weapon_dice(const Actor &a, AttackRange range) const
{
    const bool ranged = range == AttackRange::ranged;
    const auto &d = def(a);
    auto result = ranged ? d.ranged : d.melee;
    if (!ranged && d.versatile_sides && d.weapon_hands == 2)
    {
        result.sides = d.versatile_sides;
    }
    // Shillelagh: a d8 plus the spellcasting modifier; a d10 from level 5, a
    // d12 from 11 and 2d6 from 17.
    if (!ranged && shillelagh(a))
    {
        static constexpr std::array<Dice, 4> dice{Dice{1, 8, 0}, Dice{1, 10, 0}, Dice{1, 12, 0},
                                                  Dice{2, 6, 0}};
        result = dice[static_cast<std::size_t>(cantrip_tier(d) - 1)];
        result.bonus = casting_modifier(d, "shillelagh");
    }
    if (a.cleave_damage || (a.light_damage && !d.two_weapon_fighting))
    {
        result.bonus = std::min(0, result.bonus);
    }
    return result;
}

void Session::extend_run(Actor &a, Cell from, Cell to)
{
    const Cell step{to.x - from.x, to.y - from.y};
    if (a.run_feet && a.run_end == from && a.run_step == step)
    {
        a.run_feet += 5;
    }
    else
    {
        a.run_start = from;
        a.run_step = step;
        a.run_feet = 5;
    }
    a.run_end = to;
}

void Session::study(const Actor &fighter, Actor &target)
{
    // Studied Attacks lasts until the end of the Fighter's next turn, as Vex does.
    const auto index = index_of(fighter.source.id);
    const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
    detail::apply_studied(target.effects, effect_source(fighter), fighter.source.name,
                          next_turn_ms(actor(fighter.source.id)) + slot);
    log(fighter.source.name + " studies " + target.source.name + ".",
        {"{name} studies {target}.",
         {{"name", fighter.source.name}, {"target", target.source.name}}});
}

bool Session::charges(const Actor &a, const Actor &target) const
{
    const auto &d = def(a);
    return d.charge && charge_run(a, target, d.charge_feet, d.charge_size);
}

bool Session::charge_run(const Actor &a, const Actor &target, int feet, int largest) const
{
    // `feet` or more in one straight line, each step 5 feet closer to the
    // target, ending where the charger stands.
    return def(target).size <= largest && a.run_feet >= feet && a.run_end == a.source.cell &&
           distance(a.run_start, target) - distance(a.run_end, target) == a.run_feet;
}

void Session::apply_hit(Actor &a, Actor &target, const ResolvedAttack &resolved)
{
    const int natural = resolved.natural, bonus = resolved.bonus, mode = resolved.mode;
    const bool spell = resolved.spell, savage = resolved.savage;
    const auto range = resolved.range;
    const bool ranged = range == AttackRange::ranged;
    detail::consume_attack_masteries(actor(a.source.id).effects, target.effects, effect_source(a));
    const std::string modifier_label = mode < 0   ? " (disadvantage)"
                                       : mode > 0 ? " (advantage)"
                                                  : "";
    // A Parry raises the AC against this attack only.
    const int armor = armor_class(target) + resolved.parry;
    std::string message = a.source.name + " -> " + target.source.name + ": d20 " +
                          std::to_string(natural) + " + " + std::to_string(bonus) + " vs AC " +
                          std::to_string(armor) + modifier_label;
    std::vector<MessageArgument> arguments{
        {"actor", a.source.name},          {"target", target.source.name},
        {"roll", std::to_string(natural)}, {"bonus", std::to_string(bonus)},
        {"ac", std::to_string(armor)},     {"disadvantage", modifier_label, true}};
    if (!(!spell && natural >= def(a).critical_from) && !attack_hits(natural, bonus, armor))
    {
        log(message + " misses.",
            {"{actor} -> {target}: d20 {roll} + {bonus} vs AC {ac}{disadvantage} misses.",
             arguments});
        if (!spell && !ranged && weapon_mastery(a, AttackRange::melee) == detail::Mastery::graze &&
            graze_damage(a, target) > 0)
        {
            graze_ = PendingGraze{a.source.id, target.source.id, natural};
        }
        if (def(a).studied_attacks)
        {
            study(a, target);
        }
        return;
    }
    const bool critical = critical_hit(a, target, natural, spell);
    const bool helpless = unconscious(target) && distance(a, target) <= 5;
    if (savage)
    {
        message += " (Savage Attacker)";
    }
    // A Torch's fire is ordinary fire (MAGIC-4).
    const auto &keys = def(a).equipment_keys;
    const bool torch_fire =
        !spell && !ranged && std::find(keys.begin(), keys.end(), "torch") != keys.end();
    int amount = resolved_damage(target, resolved.type, resolved.damage, torch_fire);
    // A creature Snake Charm holds is slain by a hit (MAGIC-6).
    if (detail::has_effect(target.effects, detail::EffectKind::snake_charmed))
    {
        amount = std::max(amount, target.life.hp);
        log(target.source.name + " is helpless and slain.",
            {"{name} is helpless and slain.", {{"name", target.source.name}}});
    }
    // The Versatile grip is chosen automatically, so the damage line names it.
    const std::string grip = spell || ranged || !def(a).versatile_sides ? ""
                             : def(a).weapon_hands == 2                 ? " (two-handed)"
                                                                        : " (one-handed)";
    arguments.push_back({"savage", savage ? " (Savage Attacker)" : "", true});
    arguments.push_back({"hit", critical ? "CRITICAL" : "hits", true});
    arguments.push_back({"damage", std::to_string(amount)});
    arguments.push_back({"grip", grip, true});
    log(message + (critical ? " CRITICAL" : " hits") + " for " + std::to_string(amount) +
            " damage" + grip + ".",
        {"{actor} -> {target}: d20 {roll} + {bonus} vs AC {ac}{disadvantage}{savage} {hit} for {damage} damage{grip}.",
         arguments, resolved.dice});
    damage(target, amount, critical);
    // Multiattack Defense: the attacker's other attacks against the Ranger
    // this turn have Disadvantage.
    if (def(target).multiattack_defense && detail::can_apply(target.effects) &&
        std::none_of(target.effects.active.begin(), target.effects.active.end(),
                     [&](const auto &e)
                     {
                         return e.kind == detail::EffectKind::defended &&
                                e.source_actor == a.source.id;
                     }))
    {
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::defended, 0);
    }
    // A Wolf's bite knocks a Medium or smaller target Prone.
    if (!spell && !ranged && def(a).prone_bite &&
        def(target).size <= (def(a).prone_large ? 3 : 2) && target.life.hp > 0 &&
        !target.effects.prone)
    {
        target.effects.prone = true;
        log(target.source.name + " is knocked Prone.",
            {"{name} is knocked Prone.", {{"name", target.source.name}}});
    }
    // A smite may follow a weapon hit or an Unarmed Strike on the attacker's own
    // turn; Divine and Searing Smite need it to be a Melee hit.
    // Hurl Through Hell follows any attack roll's hit, a spell's too.
    if ((!spell || def(a).hurl_through_hell) && actors_[turn_].source.id == a.source.id)
    {
        auto &attacker = actor(a.source.id);
        attacker.smite_target = target.source.id;
        attacker.smite_critical = critical;
        attacker.smite_melee = !ranged && !spell;
    }
    if (!spell)
    {
        const auto property = weapon_mastery(a, range);
        if (property == detail::Mastery::sap || (property == detail::Mastery::vex && amount > 0))
        {
            const auto &source = actor(a.source.id);
            const auto index = index_of(source.source.id);
            const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
            const auto kind = property == detail::Mastery::sap ? detail::EffectKind::sap
                                                               : detail::EffectKind::vex;
            detail::apply_attack_mastery(target.effects, kind, effect_source(a), a.source.name,
                                         next_turn_ms(source) +
                                             (kind == detail::EffectKind::vex ? slot : 0));
            const auto label = std::string(detail::mastery_name(property));
            log(target.source.name + " gains " + label + " from " + a.source.name + ".",
                {"{name} gains {mastery} from {source}.",
                 {{"name", target.source.name},
                  {"mastery", label, true},
                  {"source", a.source.name}}});
        }
    }
    if (!spell)
    {
        offer_mastery(a, target, natural, range, amount, critical);
    }
    if (critical && def(a).champion && conscious(a))
    {
        ChampionMove move{
            a.source.id, target.source.id,
            natural,     std::max(0, def(a).speed - detail::speed_penalty(a.effects)) / 2,
            spell,       a.source.cell};
        if (effect_waiting() || a.cleave_damage)
        {
            move.triggered = true;
            move.cleave = a.cleave_damage;
            move.helpless = helpless;
            move.trigger_origin = a.source.cell;
            move.target_origin = target.source.cell;
        }
        if (effect_waiting())
        {
            champion_offers_.push_back(move);
        }
        else
        {
            champion_move_ = move;
        }
    }
    if (pending() == a.source.id && mover().life.hp == 0 && (effect_waiting() || champion_move_))
    {
        path_.clear();
        path_index_ = 0;
        reactors_.clear();
        reactor_index_ = 0;
        flight_.reset();
    }
}

bool Session::sneak_eligible(const Actor &a, const Actor &target, AttackRange range, int mode) const
{
    const auto &d = def(a);
    if (!d.sneak_level || a.sneak_used)
    {
        return false;
    }
    const bool ally = std::any_of(actors_.begin(), actors_.end(),
                                  [&](const auto &other)
                                  {
                                      return other.source.id != a.source.id &&
                                             other.source.side == a.source.side &&
                                             conscious(other) && distance(other, target) <= 5;
                                  });
    // A ranged weapon's fallback melee attack is unarmed, not that weapon.
    return detail::sneak_attack_eligible(
        {range == AttackRange::ranged ? d.range > 0 : !d.weapon_label.empty() && !d.ranged_weapon,
         d.finesse, d.ranged_weapon, mode, ally});
}

// Pack Tactics: a conscious ally of the attacker stands within 5 feet of the target.
bool Session::ally_beside(const Actor &a, const Actor &target) const
{
    return std::any_of(actors_.begin(), actors_.end(),
                       [&](const auto &other)
                       {
                           return other.source.id != a.source.id &&
                                  other.source.side == a.source.side && conscious(other) &&
                                  distance(other, target) <= 5;
                       });
}

int Session::roll_advantage_damage(const Actor &a, const Actor &target, int natural)
{
    const auto &extra_dice = def(a).advantage_damage;
    const bool critical = critical_hit(a, target, natural);
    const int extra = dice(extra_dice, critical);
    const auto count = std::to_string(extra_dice.count * (critical ? 2 : 1));
    const auto sides = std::to_string(extra_dice.sides);
    log(a.source.name + " adds " + count + "d" + sides + " for " + std::to_string(extra) +
            " extra damage (Advantage).",
        {"{name} adds {count}d{sides} for {damage} extra damage (Advantage).",
         {{"name", a.source.name},
          {"count", count},
          {"sides", sides},
          {"damage", std::to_string(extra)}}});
    return extra;
}

int Session::roll_sneak_attack(Actor &a, const Actor &target, int natural, int forgo)
{
    a.sneak_used = actor(a.source.id).sneak_used = true;
    const bool critical = critical_hit(a, target, natural);
    auto sneak_dice = detail::sneak_attack_dice(def(a).sneak_level);
    sneak_dice.count = std::max(0, sneak_dice.count - forgo);
    const int extra = dice(sneak_dice, critical);
    const auto count = std::to_string(sneak_dice.count * (critical ? 2 : 1));
    log(a.source.name + " adds Sneak Attack: " + count + "d6 for " + std::to_string(extra) +
            " extra damage.",
        {"{name} adds Sneak Attack: {count}d6 for {damage} extra damage.",
         {{"name", a.source.name}, {"count", count}, {"damage", std::to_string(extra)}}});
    return extra;
}

int Session::keep_higher_savage_roll(Actor &a, int first, int second)
{
    a.savage_used = actor(a.source.id).savage_used = true;
    const int kept = std::max(first, second);
    log(a.source.name + " rerolls weapon damage (Savage Attacker): " + std::to_string(first) +
            " and " + std::to_string(second) + ", keeps " + std::to_string(kept) + ".",
        {"{name} rerolls weapon damage (Savage Attacker): {first} and {second}, keeps {kept}.",
         {{"name", a.source.name},
          {"first", std::to_string(first)},
          {"second", std::to_string(second)},
          {"kept", std::to_string(kept)}}});
    return kept;
}

bool Session::attack(Actor &a, Actor &target, AttackRange range,
                     const std::optional<SpellAttack> &spell_attack)
{
    // Mislead: an attack on the double passes through it (LEVELS-1), and foes
    // ignore it after.
    for (auto &e : target.effects.active)
    {
        if (e.kind == detail::EffectKind::illusory)
        {
            e.dc = 1;
            log("The attack passes through an illusory double.",
                {"The attack passes through an illusory double.", {}});
            return false;
        }
    }
    const bool ranged = range == AttackRange::ranged;
    const bool spell = spell_attack.has_value();
    if (target.source.cell.x != a.source.cell.x)
    {
        a.facing_left = target.source.cell.x < a.source.cell.x;
    }
    if (sanctuary_stops(a, target))
    {
        return false;
    }
    const auto &d = def(a);
    auto modifiers =
        attack_modifiers(a, target, range, spell ? AttackSource::spell : AttackSource::weapon);
    // Brutal Strike gives up the roll's Advantage; a roll with Disadvantage
    // cannot be chosen, so it stays an ordinary attack.
    const bool brutal = brutal_ && brutal_->actor == a.source.id && !spell &&
                        range == AttackRange::melee && !modifiers.disadvantage;
    if (brutal)
    {
        modifiers.advantage = false;
    }
    a.aim_ready = false;
    if (actors_[turn_].source.id == a.source.id)
    {
        a.attacked = true;
    }
    // Guiding Bolt's and Studied Attacks' Advantage is spent on this attack roll.
    const auto attacker = effect_source(a);
    std::erase_if(target.effects.active,
                  [&](const auto &e)
                  {
                      return e.kind == detail::EffectKind::guiding_bolt ||
                             (e.kind == detail::EffectKind::studied &&
                              e.source_scope == attacker.scope && e.source_actor == attacker.actor);
                  });
    const int weapon_magic = spell ? 0 : this->weapon_magic(a, target, range);
    const bool charging = !spell && !ranged && charges(a, target);
    a.run_feet = 0;
    bool lucky_reroll = false;
    int natural = detail::d20(modifiers, rng_, &lucky_reroll);
    log_luck(a, lucky_reroll);
    end_invisibility(a.source.id);
    // Supreme Sneak's Stealth Attack keeps a hidden Rogue hidden when it deals
    // Sneak Attack damage; that is known after the roll.
    const bool stealthy =
        cunning_ && cunning_->actor == a.source.id && cunning_->has(CunningStrike::stealth);
    if (!stealthy)
    {
        end_hiding(a.source.id);
    }
    // An attack roll against an enemy on its own turn extends a Rage.
    if (target.source.side != a.source.side && actors_[turn_].source.id == a.source.id)
    {
        extend_rage(a);
    }
    // A scroll's spell attack has the scroll's bonus, its DC less 8 (MAGIC-6).
    const bool item_attack = item_caster_ && item_caster_ == a.source.id;
    int bonus = (spell && item_attack       ? item_dc_ - 8
                 : spell                    ? casting_bonus(d, spell_attack->spell)
                 : !ranged && shillelagh(a) ? casting_bonus(d, "shillelagh")
                 : ranged                   ? d.ranged_bonus
                                            : d.melee_bonus) +
                blessing_die(a) + (spell || ranged ? 0 : sacred_weapon_bonus(a)) + weapon_magic -
                return_penalty(a);
    // Sundering Blow: +5 to the next attack roll another creature makes against it.
    if (detail::spend_sundering(target.effects, effect_source(a)))
    {
        bonus += 5;
        log("Sundering Blow adds 5 to the attack roll.",
            {"Sundering Blow adds 5 to the attack roll.", {}});
    }
    const auto damage_dice = spell ? spell_attack->dice : weapon_dice(a, range);
    // Seeking Spell: a missed spell attack rolls its d20 again, once.
    if (spell && casting_with(Metamagic::seeking) && natural != 20 &&
        !attack_hits(natural, bonus, armor_class(target)))
    {
        std::erase_if(casting_metamagic_,
                      [](const auto &ready)
                      {
                          return ready.option == Metamagic::seeking;
                      });
        natural = detail::d20(modifiers, rng_);
        log("Seeking Spell rerolls the missed attack.",
            {"Seeking Spell rerolls the missed attack.", {}});
    }
    // Heroic Inspiration rolls a missed attack again.
    if (natural != 20 && !(!spell && natural >= d.critical_from) &&
        !attack_hits(natural, bonus, armor_class(target)))
    {
        if (const auto again = heroic_reroll(a))
        {
            natural = *again;
        }
    }
    const bool automatic = natural == 20 || (!spell && natural >= d.critical_from);
    bool hit = automatic || attack_hits(natural, bonus, armor_class(target));
    // Bardic Inspiration may rescue a miss; Cutting Words may spoil a hit.
    if (!hit && natural != 1)
    {
        bonus += inspiration_roll(a, armor_class(target) - natural - bonus);
        hit = attack_hits(natural, bonus, armor_class(target));
        if (!hit)
        {
            bonus += peerless_skill(a, armor_class(target) - natural - bonus);
            hit = attack_hits(natural, bonus, armor_class(target));
        }
    }
    else if (hit && !automatic)
    {
        bonus -= cutting_words(a, natural + bonus - armor_class(target));
        hit = attack_hits(natural, bonus, armor_class(target));
    }
    // Wind Wall: arrows, bolts and other ordinary projectiles shot across it miss.
    if (hit && ranged && !spell && across_wind_wall(a.source.cell, target.source.cell))
    {
        hit = false;
        log("The wind deflects the shot.", {"The wind deflects the shot.", {}});
    }
    // Protection From Normal Missiles (MAGIC-6): a shot or a thrown weapon
    // without magic misses from more than 5 feet away. There is no ammunition
    // (AMMO-UNLIMITED), so a bow's shot has the bow's magic.
    if (hit && ranged && !spell &&
        detail::has_effect(target.effects, detail::EffectKind::missile_ward) &&
        distance(a, target) > 5 && weapon_magic == 0)
    {
        hit = false;
        log("The missile cannot touch " + target.source.name + ".",
            {"The missile cannot touch {name}.", {{"name", target.source.name}}});
    }
    // Protective Magic: a monster's Reaction casts Shield when its +5 turns the
    // hit into a miss.
    if (hit && !automatic && def(target).protective_magic && trait_uses_left(target) > 0 &&
        target.reaction && conscious(target) && !detail::incapacitated(target.effects) &&
        !detail::has_effect(target.effects, detail::EffectKind::shield) &&
        !attack_hits(natural, bonus, armor_class(target) + 5))
    {
        target.reaction = false;
        spend_trait_use(target);
        detail::apply_poisoned(target.effects, effect_source(target), target.source.name,
                               next_turn_ms(target), detail::EffectKind::shield);
        log(target.source.name + " casts Shield.",
            {"{name} casts {spell}.", {{"name", target.source.name}, {"spell", "Shield", true}}});
        hit = attack_hits(natural, bonus, armor_class(target));
    }
    // Parry: a monster hit by a melee attack roll uses its Reaction when the
    // bonus turns the hit into a miss. A critical hit stands.
    int parry = 0;
    if (hit && !ranged && !spell && !automatic && def(target).parry && target.reaction &&
        conscious(target) && !detail::incapacitated(target.effects) &&
        !attack_hits(natural, bonus, armor_class(target) + def(target).parry))
    {
        parry = def(target).parry;
        target.reaction = false;
        hit = false;
        log(target.source.name + " parries " + a.source.name + "'s attack.",
            {"{name} parries {attacker}'s attack.",
             {{"name", target.source.name}, {"attacker", a.source.name}}});
    }
    // Cloak of Displacement (MAGIC-4): the first attack roll against its wearer
    // in a fight misses. A natural 1 misses anyway and leaves it for the next.
    if (def(target).displacement && !target.displacement_spent && natural != 1)
    {
        target.displacement_spent = true;
        hit = false;
        log(a.source.name + " strikes " + target.source.name + "'s displaced image.",
            {"{attacker} strikes {name}'s displaced image.",
             {{"attacker", a.source.name}, {"name", target.source.name}}});
    }
    const auto damage_type = spell    ? spell_attack->type
                             : ranged ? d.ranged_type
                                      : sacred_damage_type(a);
    // Shield cannot turn a critical hit into a miss, but Deflect Attacks can
    // still lessen one.
    if (hit)
    {
        ask_reaction(target, Asked::hit,
                     damage_type == detail::DamageType::bludgeoning ||
                         damage_type == detail::DamageType::piercing ||
                         damage_type == detail::DamageType::slashing,
                     automatic, natural + bonus, armor_class(target));
    }
    if (hit && strikes_duplicate(a, target))
    {
        return false;
    }
    const bool sneak = hit && !spell && sneak_eligible(a, target, range, modifiers.mode());
    // The damage dice go on the hit's line; the lines on the way, such as
    // Rage's, show none.
    rolled_dice_.clear();
    attack_dice_held_ = true;
    const auto roll_damage = [&]
    {
        return !spell && (d.sneak_level || d.great_weapon_fighting || a.light_damage ||
                          a.cleave_damage)
                   ? detail::roll_damage_component(rng_, damage_dice,
                                                   critical_hit(a, target, natural),
                                                   weapon_die_rule(a, range))
               : spell ? this->spell_dice(a, damage_dice, critical_hit(a, target, natural, spell))
                       : dice(damage_dice, critical_hit(a, target, natural, spell));
    };
    const bool bursting = spell && spell_attack->bursts;
    int weapon_damage =
        hit ? (bursting ? burst_damage(*spell_attack, critical_hit(a, target, natural, spell))
                        : roll_damage())
            : 0;
    const auto weapon_dice_end = rolled_dice_.size();
    // Sneak Attack and Savage Attacker have one right answer, so they apply
    // automatically and the log records what they added.
    const bool cunning = sneak && cunning_ && cunning_->actor == a.source.id;
    const int sneak_damage =
        sneak ? roll_sneak_attack(a, target, natural, cunning ? cunning_->cost() : 0) : 0;
    const int advantage_damage = hit && !spell && d.advantage_damage.count && modifiers.mode() > 0
                                     ? roll_advantage_damage(a, target, natural)
                                     : 0;
    const bool savage = hit && !spell && damage_dice.count && d.savage && !a.savage_used;
    if (savage)
    {
        const int first = weapon_damage;
        const auto second_from = rolled_dice_.size();
        const int second = roll_damage();
        weapon_damage = keep_higher_savage_roll(a, first, second);
        // The roll set aside leaves the hit's line.
        if (weapon_damage == first)
        {
            rolled_dice_.erase(rolled_dice_.begin() + static_cast<std::ptrdiff_t>(second_from),
                               rolled_dice_.end());
        }
        else
        {
            rolled_dice_.erase(rolled_dice_.begin(),
                               rolled_dice_.begin() + static_cast<std::ptrdiff_t>(weapon_dice_end));
        }
    }
    if (hit)
    {
        weapon_damage += weapon_magic;
    }
    // Rage Damage: Strength-based weapon and unarmed hits.
    if (hit && !spell && rage_of(a) && strength_attack(a, range))
    {
        weapon_damage += d.rage_damage;
        log("Rage adds " + std::to_string(d.rage_damage) + " damage.",
            {"Rage adds {damage} damage.", {{"damage", std::to_string(d.rage_damage)}}});
        weapon_damage += frenzy_damage(a, critical_hit(a, target, natural));
    }
    if (hit && !spell)
    {
        weapon_damage = resized_damage(a, weapon_damage);
    }
    if (hit && brutal)
    {
        const int extra = dice({1, 10, 0}, critical_hit(a, target, natural));
        weapon_damage += extra;
        log("Brutal Strike adds " + std::to_string(extra) + " damage.",
            {"Brutal Strike adds {damage} damage.", {{"damage", std::to_string(extra)}}});
    }
    // Colossus Slayer: once per turn, 1d8 more on a creature already missing HP.
    if (hit && !spell && d.colossus_slayer && !a.colossus_used && target.life.hp < max_hp(target))
    {
        a.colossus_used = true;
        const int extra = dice({1, 8, 0}, critical_hit(a, target, natural));
        weapon_damage += extra;
        log("Colossus Slayer adds " + std::to_string(extra) + " damage.",
            {"Colossus Slayer adds {damage} damage.", {{"damage", std::to_string(extra)}}});
    }
    if (hit && charging)
    {
        const int extra = dice(d.charge_dice, critical_hit(a, target, natural));
        weapon_damage += extra;
        log("The charge adds " + std::to_string(extra) + " damage.",
            {"The charge adds {damage} damage.", {{"damage", std::to_string(extra)}}});
    }
    // Ray of Enfeeblement: 1d8 less on each of the creature's damage rolls. Its
    // own line shows it.
    const auto before_enfeebled = rolled_dice_.size();
    const int enfeebled =
        hit && detail::has_effect(a.effects, detail::EffectKind::enfeebled) ? roll(8) : 0;
    rolled_dice_.resize(before_enfeebled);
    if (enfeebled)
    {
        log("Ray of Enfeeblement subtracts " + std::to_string(enfeebled) + " damage.",
            {"Ray of Enfeeblement subtracts {damage} damage.",
             {{"damage", std::to_string(enfeebled)}}});
    }
    int rolled = std::max(0, weapon_damage + sneak_damage + advantage_damage - enfeebled);
    const std::string damage_dice_shown =
        hit ? dice_detail(rolled_dice_, weapon_damage + sneak_damage + advantage_damage) : "";
    rolled_dice_.clear();
    attack_dice_held_ = false;
    if (hit)
    {
        rolled = std::max(0, rolled - cut_damage(a, target, rolled, damage_type));
    }
    if (hit)
    {
        hunters_defense(target, damage_type);
    }
    const int amount = hit ? dodged(target, deflected(a, target, rolled, damage_type, ranged)) : 0;
    const int hit_points_before = target.life.hp;
    apply_hit(a, target,
              ResolvedAttack{.natural = natural,
                             .bonus = bonus,
                             .mode = modifiers.mode(),
                             .damage = amount,
                             .type = damage_type,
                             .range = range,
                             .spell = spell,
                             .savage = savage,
                             .parry = parry,
                             .dice = damage_dice_shown});
    // A Specter's Life Drain: the damage taken lowers the Hit Point maximum.
    if (hit && !ranged && !spell && d.life_drain_attack)
    {
        lower_hit_point_maximum(a, target, hit_points_before - target.life.hp);
    }
    // A Mummy's Rotting Fist curses the creature it hits.
    if (hit && !ranged && !spell && d.rotting_curse && !target.life.dead &&
        detail::can_apply(target.effects) &&
        !detail::has_effect(target.effects, detail::EffectKind::rotting_curse))
    {
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::rotting_curse, 0);
        log(target.source.name + " is cursed.",
            {"{name} is cursed.", {{"name", target.source.name}}});
    }
    if (hit && !spell && !target.life.dead)
    {
        strike_riders(a, target, ranged);
    }
    // A Stirge's Proboscis attaches it to the creature it hits.
    if (hit && !ranged && !spell && d.attach_drain && !target.life.dead && !attached_host(a) &&
        detail::can_apply(a.effects))
    {
        detail::apply_spell_benefit(a.effects, effect_source(target), target.source.name,
                                    detail::EffectKind::attached, 0);
        log(a.source.name + " attaches to " + target.source.name + ".",
            {"{name} attaches to {target}.",
             {{"name", a.source.name}, {"target", target.source.name}}});
    }
    if (hit && charging && target.life.hp > 0 && !target.effects.prone)
    {
        target.effects.prone = true;
        log(target.source.name + " is knocked Prone.",
            {"{name} is knocked Prone.", {{"name", target.source.name}}});
    }
    // Holy Aura: a Fiend or Undead that hits a warded creature in melee saves
    // or is Blinded until the end of its next turn.
    if (const auto *aura = hit && !ranged ? holy_aura(target) : nullptr;
        aura && (d.creature_type == "fiend" || d.creature_type == "undead") && !a.life.dead &&
        detail::can_apply(a.effects))
    {
        const auto &caster = actor(aura->caster);
        if (!saving_throw_succeeds(a, detail::Ability::constitution, aura->save_dc))
        {
            const auto index = index_of(a.source.id);
            const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
            detail::apply_poisoned(a.effects, effect_source(caster), caster.source.name,
                                   next_turn_ms(a) + slot, detail::EffectKind::dazzled);
            log(a.source.name + " is Blinded by the holy aura.",
                {"{name} is Blinded by the holy aura.", {{"name", a.source.name}}});
        }
    }
    // Conjure Minor Elementals: a hit on a creature in the caster's Emanation
    // deals 2d8 more of the chosen type, 1d8 more per slot level above 4.
    if (const auto spirits = std::find_if(zones_.begin(), zones_.end(),
                                          [&](const auto &zone)
                                          {
                                              return zone.kind == ZoneKind::elementals &&
                                                     zone.caster == a.source.id;
                                          });
        hit && spirits != zones_.end() && !target.life.dead && in_emanation(*spirits, target))
    {
        for (const auto &e : a.effects.active)
        {
            if (e.kind == detail::EffectKind::minor_elementals)
            {
                const auto type = static_cast<detail::DamageType>(e.dc);
                const auto name = std::string(detail::damage_name(type));
                const int spirit_damage = resolved_damage(
                    target, type, dice({2 + static_cast<int>(spirits->slot) - 4, 8, 0}));
                log(target.source.name + " takes " + std::to_string(spirit_damage) + " " + name +
                        " damage from the elemental spirits.",
                    {"{name} takes {damage} {type} damage from the elemental spirits.",
                     {{"name", target.source.name},
                      {"damage", std::to_string(spirit_damage)},
                      {"type", name, true}}});
                damage(target, spirit_damage, false);
                break;
            }
        }
    }
    // Fire Shield: a creature within 5 feet that hits in melee takes 2d8 Fire
    // from the warm shield or Cold from the chill one.
    if (const auto shield = std::find_if(target.effects.active.begin(), target.effects.active.end(),
                                         [](const auto &e)
                                         {
                                             return e.kind == detail::EffectKind::fire_shield;
                                         });
        hit && !ranged && shield != target.effects.active.end() && !a.life.dead &&
        distance(a, target) <= 5)
    {
        const auto type = shield->dc ? detail::DamageType::cold : detail::DamageType::fire;
        const auto name = std::string(detail::damage_name(type));
        const int shield_damage = resolved_damage(a, type, dice({2, 8, 0}));
        log(a.source.name + " takes " + std::to_string(shield_damage) + " " + name +
                " damage from the fire shield.",
            {"{name} takes {damage} {type} damage from the fire shield.",
             {{"name", a.source.name},
              {"damage", std::to_string(shield_damage)},
              {"type", name, true}}});
        damage(a, shield_damage, false);
    }
    // A hit's added damage of a second type (the Hobgoblin Warrior's poison,
    // a Ghoul's Necrotic bite).
    if (const auto &added = ranged ? d.ranged_extra : d.melee_extra;
        hit && !spell && added.count && !target.life.dead)
    {
        const auto type = ranged ? d.ranged_extra_type : d.melee_extra_type;
        const int extra_roll = dice(added, critical_hit(a, target, natural));
        const int extra = resolved_damage(
            target, type, std::max(0, extra_roll - cut_damage(a, target, extra_roll, type)));
        const auto name = std::string(detail::damage_name(type));
        log(target.source.name + " takes " + std::to_string(extra) + " " + name + " damage.",
            {"{name} takes {damage} {type} damage.",
             {{"name", target.source.name},
              {"damage", std::to_string(extra)},
              {"type", name, true}}});
        damage(target, extra, false);
    }
    // A Gnoll rampages after damaging a creature that was already Bloodied.
    if (hit && amount > 0 && d.rampage && a.bonus && trait_uses_left(a) > 0 &&
        actors_[turn_].source.id == a.source.id && hit_points_before > 0 &&
        hit_points_before * 2 <= max_hp(target))
    {
        start_rampage(a);
    }
    // A Phase Spider's Bite, its Poison damage too, dropped the creature to 0.
    if (hit && !ranged && !spell && d.paralyzing_venom && hit_points_before > 0 &&
        target.life.hp == 0 && !target.life.dead)
    {
        envenom(a, target);
    }
    // Hunter's Mark: any attack-roll hit on the caster's quarry deals 1d6 Force.
    if (hit && marked_by(target, a) && !target.life.dead)
    {
        const int extra = resolved_damage(target, detail::DamageType::force,
                                          dice({1, 6, 0}, critical_hit(a, target, natural, spell)));
        log(target.source.name + " takes " + std::to_string(extra) +
                " Force damage from Hunter's Mark.",
            {"{name} takes {damage} Force damage from Hunter's Mark.",
             {{"name", target.source.name}, {"damage", std::to_string(extra)}}});
        damage(target, extra, false);
    }
    if (hit)
    {
        curse_strikes(a, target);
    }
    // Hex: the caster's attack-roll hits deal 1d6 more Necrotic damage.
    if (hit && hexed_by(target, a) && !target.life.dead)
    {
        const int extra = resolved_damage(target, detail::DamageType::necrotic,
                                          dice({1, 6, 0}, critical_hit(a, target, natural, spell)));
        log(target.source.name + " takes " + std::to_string(extra) + " Necrotic damage from Hex.",
            {"{name} takes {damage} Necrotic damage from Hex.",
             {{"name", target.source.name}, {"damage", std::to_string(extra)}}});
        damage(target, extra, false);
    }
    // Divine Favor: a weapon hit deals an extra 1d4 Radiant damage.
    if (hit && !spell && detail::has_effect(a.effects, detail::EffectKind::divine_favor) &&
        !target.life.dead)
    {
        const int extra = resolved_damage(target, detail::DamageType::radiant,
                                          dice({1, 4, 0}, critical_hit(a, target, natural)));
        log(target.source.name + " takes " + std::to_string(extra) +
                " Radiant damage from Divine Favor.",
            {"{name} takes {damage} Radiant damage from Divine Favor.",
             {{"name", target.source.name}, {"damage", std::to_string(extra)}}});
        damage(target, extra, false);
    }
    // The Javelin of Lightning's bolt: its hit deals 4d6 more Lightning damage.
    if (hit && !spell && ranged && a.lightning_throw && !target.life.dead)
    {
        const int extra = resolved_damage(target, detail::DamageType::lightning,
                                          dice({4, 6, 0}, critical_hit(a, target, natural)));
        log(target.source.name + " takes " + std::to_string(extra) +
                " Lightning damage from the Javelin of Lightning.",
            {"{name} takes {damage} Lightning damage from the Javelin of Lightning.",
             {{"name", target.source.name}, {"damage", std::to_string(extra)}}});
        damage(target, extra, false);
    }
    // Basic Poison: the coated weapon's next Piercing or Slashing damage within
    // a minute adds 1d4 Poison, which spends the coat.
    if (amount > 0 && !spell && a.coated_until_round >= static_cast<int>(round_) &&
        (damage_type == detail::DamageType::piercing ||
         damage_type == detail::DamageType::slashing) &&
        !target.life.dead)
    {
        a.coated_until_round = 0;
        const int extra = resolved_damage(target, detail::DamageType::poison,
                                          dice({1, 4, 0}, critical_hit(a, target, natural)));
        log(target.source.name + " takes " + std::to_string(extra) +
                " Poison damage from Basic Poison.",
            {"{name} takes {damage} Poison damage from Basic Poison.",
             {{"name", target.source.name}, {"damage", std::to_string(extra)}}});
        damage(target, extra, false);
    }
    // Flame Tongue: while lit, a hit deals an extra 2d6 Fire damage.
    if (hit && !spell && !ranged && def(a).flame_tongue && a.flame_lit && !target.life.dead)
    {
        const int extra = resolved_damage(target, detail::DamageType::fire,
                                          dice({2, 6, 0}, critical_hit(a, target, natural)));
        log(target.source.name + " takes " + std::to_string(extra) +
                " Fire damage from Flame Tongue.",
            {"{name} takes {damage} Fire damage from Flame Tongue.",
             {{"name", target.source.name}, {"damage", std::to_string(extra)}}});
        damage(target, extra, false);
    }
    // Radiant Strikes (Paladin 11): a Melee weapon or Unarmed Strike hit deals
    // an extra 1d8 Radiant damage.
    if (hit && !spell && !ranged && def(a).radiant_strikes && !target.life.dead)
    {
        const int extra = resolved_damage(target, detail::DamageType::radiant,
                                          dice({1, 8, 0}, critical_hit(a, target, natural)));
        log(target.source.name + " takes " + std::to_string(extra) +
                " Radiant damage from Radiant Strikes.",
            {"{name} takes {damage} Radiant damage from Radiant Strikes.",
             {{"name", target.source.name}, {"damage", std::to_string(extra)}}});
        damage(target, extra, false);
    }
    if (hit && amount > 0 && target.life.hp > 0)
    {
        giant_boon(a, target, critical_hit(a, target, natural, spell));
    }
    if (hit && brutal && !target.life.dead)
    {
        brutal_blow(a, target, brutal_->blow);
    }
    if (cunning)
    {
        // Withdraw's move comes after the other effect.
        auto kinds = std::vector{cunning_->kind};
        if (cunning_->second)
        {
            kinds.push_back(*cunning_->second);
        }
        std::stable_partition(kinds.begin(), kinds.end(),
                              [](auto kind)
                              {
                                  return kind != CunningStrike::withdraw;
                              });
        for (const auto kind : kinds)
        {
            cunning_effect(a, target, kind);
        }
    }
    else if (stealthy)
    {
        end_hiding(a.source.id);
    }
    if (hit && stunning_ == a.source.id && !spell && !ranged && d.martial_arts && !a.stun_used &&
        a.surges[CharacterClass::monk] > 0 && !target.life.dead && target.life.hp > 0)
    {
        stunning_strike(a, target);
    }
    // A Pirate Captain's Riposte: having parried, it strikes back at the
    // attacker within reach.
    if (parry && def(target).riposte && conscious(target) && !a.life.dead && a.life.hp > 0 &&
        distance(a, target) <= 5)
    {
        log(target.source.name + " ripostes.",
            {"{name} ripostes.", {{"name", target.source.name}}});
        attack(target, a, AttackRange::melee);
    }
    // Hellish Rebuke and Storm's Thunder answer the damage once the attack is done.
    answer_damage();
    return hit;
}

void Session::reveal_lore(const Actor &caster, const Actor &target)
{
    // Hunter's Lore: the marked creature's damage Immunities, Resistances and
    // Vulnerabilities become known.
    if (!def(caster).hunters_lore)
    {
        return;
    }
    const auto &affinities = def(target).affinities;
    if (affinities.empty())
    {
        log("Hunter's Lore: " + target.source.name +
                " has no damage Immunities, Resistances or Vulnerabilities.",
            {"Hunter's Lore: {name} has no damage Immunities, Resistances or Vulnerabilities.",
             {{"name", target.source.name}}});
    }
    for (const auto &affinity : affinities)
    {
        const std::string kind = affinity.kind == detail::AffinityKind::resistance ? "Resistance"
                                 : affinity.kind == detail::AffinityKind::immunity
                                     ? "Immunity"
                                     : "Vulnerability";
        const std::string type =
            affinity.type ? std::string(detail::damage_name(*affinity.type)) : "all damage";
        log("Hunter's Lore: " + target.source.name + " has " + kind + " to " + type + ".",
            {"Hunter's Lore: {name} has {kind} to {type}.",
             {{"name", target.source.name}, {"kind", kind, true}, {"type", type, true}}});
    }
}

bool Session::holds_hunters_mark(const Actor &caster) const
{
    return std::any_of(actors_.begin(), actors_.end(),
                       [&](const auto &other)
                       {
                           return marked_by(other, caster);
                       });
}

bool Session::marked_by(const Actor &target, const Actor &caster) const
{
    return std::any_of(target.effects.active.begin(), target.effects.active.end(),
                       [&](const auto &e)
                       {
                           return e.kind == detail::EffectKind::hunters_mark &&
                                  e.source_scope == scope_ && e.source_actor == caster.source.id;
                       });
}

bool Session::hexed_by(const Actor &target, const Actor &caster) const
{
    return std::any_of(target.effects.active.begin(), target.effects.active.end(),
                       [&](const auto &e)
                       {
                           return e.kind == detail::EffectKind::hex && e.source_scope == scope_ &&
                                  e.source_actor == caster.source.id;
                       });
}

bool Session::hex_can_move(const Actor &caster) const
{
    // Hex moves once its creature drops, while the caster concentrates on it.
    bool hexed = false;
    for (const auto &other : actors_)
    {
        if (hexed_by(other, caster))
        {
            if (other.life.hp > 0 && !other.life.dead)
            {
                return false;
            }
            hexed = true;
        }
    }
    return hexed;
}

bool Session::mark_can_move(const Actor &caster) const
{
    // The mark moves once its creature drops to 0 Hit Points, while the
    // caster still concentrates on it.
    bool marked = false;
    for (const auto &other : actors_)
    {
        if (marked_by(other, caster))
        {
            if (other.life.hp > 0 && !other.life.dead)
            {
                return false;
            }
            marked = true;
        }
    }
    return marked;
}

bool Session::attack_as(Actor &a, Loadout loadout, Actor &target, AttackRange range)
{
    // Given back however the attack ends, a reaction prompt unwinding it too.
    struct Lent
    {
        Actor &a;
        Loadout own;

        Lent(Actor &actor, Loadout own_loadout) : a(actor), own(std::move(own_loadout))
        {
        }

        Lent(const Lent &) = delete;
        Lent &operator=(const Lent &) = delete;

        ~Lent()
        {
            a.definition = std::move(own.definition);
            a.selected_weapon = own.selected_weapon;
            a.light_damage = own.light_damage;
        }
    } lent{a,
           {std::exchange(a.definition, std::move(loadout.definition)),
            std::exchange(a.selected_weapon, loadout.selected_weapon),
            std::exchange(a.light_damage, loadout.light_damage)}};

    return attack(a, target, range);
}

void Session::strike_true(Actor &a, Actor &target, TrueStrikeDamage choice)
{
    // The attack uses the spellcasting modifier for its attack and damage
    // rolls; `casting` is that modifier plus the +2 Proficiency Bonus.
    Loadout striker{a.definition, a.selected_weapon, a.light_damage};
    auto &d = striker.definition;
    const int modifier = casting_modifier(d, "true_strike");
    d.melee_bonus += modifier - d.melee_ability;
    d.melee_ability = modifier;
    d.melee.bonus = modifier;
    if (choice == TrueStrikeDamage::radiant)
    {
        d.melee_type = detail::DamageType::radiant;
    }
    // From level 5 the strike adds Radiant damage: 1d6, 2d6 from 11, 3d6 from 17.
    if (cantrip_tier(d) > 1)
    {
        d.melee_extra = {cantrip_tier(d) - 1, 6, 0};
        d.melee_extra_type = detail::DamageType::radiant;
    }
    log(a.source.name + " casts True Strike.",
        {"{name} casts {spell}.", {{"name", a.source.name}, {"spell", "True Strike", true}}});
    attack_as(a, std::move(striker), target, AttackRange::melee);
}

std::vector<Session::ReadyMetamagic> Session::readied_metamagic(const Actor &a)
{
    std::vector<ReadyMetamagic> result;
    for (const auto &e : a.effects.active)
    {
        if (e.kind == detail::EffectKind::metamagic)
        {
            if (e.dc < 10)
            {
                result.push_back({static_cast<Metamagic>(e.dc), std::nullopt});
            }
            else
            {
                result.push_back({Metamagic::transmuted,
                                  detail::damage_type(
                                      transmuted_types.at(static_cast<std::size_t>(e.dc - 10)))});
            }
        }
    }
    return result;
}

std::size_t Session::metamagic_room(const Actor &a) noexcept
{
    return a.definition.sorcery_incarnate &&
                   detail::has_effect(a.effects, detail::EffectKind::innate_sorcery)
               ? 2
               : 1;
}

bool Session::readies(const Actor &a, Metamagic option, const detail::SpellDef &spell)
{
    const auto ready = readied_metamagic(a);
    return std::any_of(ready.begin(), ready.end(),
                       [&](const auto &r)
                       {
                           return r.option == option;
                       }) &&
           metamagic_applies(option, spell) &&
           a.lay_on_hands[CharacterClass::sorcerer] >= metamagic_cost(option);
}

void Session::mark_heightened(const Actor &caster, const detail::SpellDef &spell)
{
    // A new cast of the spell replaces the old one's mark.
    const int index = detail::spell_index(spell.id);
    for (auto &other : actors_)
    {
        std::erase_if(other.effects.active,
                      [&](const auto &e)
                      {
                          return e.kind == detail::EffectKind::heightened &&
                                 e.source_scope == scope_ && e.source_actor == caster.source.id &&
                                 e.dc == index;
                      });
    }
    if (!heightened_target_)
    {
        return;
    }
    auto &target = actor(heightened_target_);
    if (detail::can_apply(target.effects))
    {
        detail::apply_spell_benefit(target.effects, effect_source(caster), caster.source.name,
                                    detail::EffectKind::heightened, index);
    }
}

void Session::take_metamagic(Actor &caster, const detail::SpellDef &spell)
{
    casting_metamagic_.clear();
    heightened_target_ = 0;
    for (const auto &ready : readied_metamagic(caster))
    {
        if (!readies(caster, ready.option, spell))
        {
            continue;
        }
        caster.lay_on_hands[CharacterClass::sorcerer] -= metamagic_cost(ready.option);
        // Each option spent leaves; one that cannot change this spell waits.
        const int value =
            ready.type ? 10 + static_cast<int>(
                                  std::find_if(transmuted_types.begin(), transmuted_types.end(),
                                               [&](auto id)
                                               {
                                                   return detail::damage_type(id) == *ready.type;
                                               }) -
                                  transmuted_types.begin())
                       : static_cast<int>(ready.option);
        const auto spent =
            std::find_if(caster.effects.active.begin(), caster.effects.active.end(),
                         [&](const auto &e)
                         {
                             return e.kind == detail::EffectKind::metamagic && e.dc == value;
                         });
        caster.effects.active.erase(spent);
        casting_metamagic_.push_back(ready);
        const std::string label(metamagic_labels[static_cast<std::size_t>(ready.option)]);
        log(caster.source.name + " uses " + label + ".",
            {"{name} uses {feature}.", {{"name", caster.source.name}, {"feature", label, true}}});
    }
}

int Session::spell_range(const Actor &caster, const detail::SpellDef &spell) const
{
    const auto &d = def(caster);
    // Eldritch Spear: 30 feet more per Warlock level for its cantrip.
    int range =
        spell.range +
        (d.eldritch_spear.contains(spell.id) ? 30 * d.class_levels[CharacterClass::warlock] : 0);
    // Improved Elemental Fury (Druid 15): Potent Spellcasting's Druid
    // cantrips with a range of 10 feet or more reach 300 feet farther.
    if (d.druid_potent && d.class_levels[CharacterClass::druid] >= 15 && !spell.level &&
        spell.range >= 10 && detail::on_class_list(CharacterClass::druid, spell.id))
    {
        range += 300;
    }
    // Distant Spell: double the range, or 30 feet for Touch.
    if (!readies(caster, Metamagic::distant, spell))
    {
        return range;
    }
    return range <= 5 ? 30 : 2 * range;
}

detail::DamageType Session::cast_damage_type(detail::DamageType type) const
{
    for (const auto &ready : casting_metamagic_)
    {
        if (ready.option == Metamagic::transmuted && transmutable(type))
        {
            return *ready.type;
        }
    }
    return type;
}

int Session::spell_dice(const Actor &caster, Dice dice, bool critical)
{
    // Overchannel: the most the dice can show.
    if (overchanneling_ && caster.source.id == overchannel_caster_)
    {
        damage_dice_ = dice.count * (critical ? 2 : 1);
        return dice.count * (critical ? 2 : 1) * dice.sides + dice.bonus;
    }
    // Empowered Spell rerolls the lowest dice below their average, up to the
    // Charisma modifier (at least one), and keeps the new rolls.
    if (!casting_with(Metamagic::empowered))
    {
        return this->dice(dice, critical);
    }
    damage_dice_ = dice.count * (critical ? 2 : 1);
    std::vector<int> rolls;
    for (int n = 0; n < damage_dice_; ++n)
    {
        rolls.push_back(roll(dice.sides));
    }
    std::sort(rolls.begin(), rolls.end());
    const int rerolls = std::max(1, casting_modifier(def(caster)));
    for (int n = 0;
         n < rerolls && n < static_cast<int>(rolls.size()) && rolls[n] * 2 < dice.sides + 1; ++n)
    {
        rolls[n] = roll(dice.sides);
    }
    int total = dice.bonus;
    for (const int value : rolls)
    {
        total += value;
    }
    return total;
}

std::vector<EntityId> Session::careful_allies(const Actor &caster,
                                              const std::vector<Cell> &cells) const
{
    std::vector<EntityId> spared;
    if (!casting_with(Metamagic::careful))
    {
        return spared;
    }
    const auto most = static_cast<std::size_t>(std::max(1, casting_modifier(def(caster))));
    for (const auto &other : actors_)
    {
        if (spared.size() < most && other.source.side == caster.source.side &&
            other.source.id != caster.source.id && !other.life.dead && touches(cells, other))
        {
            spared.push_back(other.source.id);
        }
    }
    return spared;
}

int Session::burst_damage(const SpellAttack &spell, bool critical)
{
    // Each die showing its maximum adds another, up to `bursts` extra dice.
    const auto &dice = spell.dice;
    const int count = dice.count * (critical ? 2 : 1);
    int total = dice.bonus, extra = 0;
    for (int n = 0; n < count + extra; ++n)
    {
        const int rolled = roll(dice.sides);
        total += rolled;
        if (rolled == dice.sides && extra < spell.bursts)
        {
            ++extra;
        }
    }
    return total;
}

int Session::resized_damage(const Actor &a, int amount)
{
    // Enlarge: 1d4 more weapon damage. Reduce: 1d4 less, but not below 1.
    if (detail::has_effect(a.effects, detail::EffectKind::enlarged))
    {
        const int extra = roll(4);
        log("Enlarge adds " + std::to_string(extra) + " damage.",
            {"Enlarge adds {damage} damage.", {{"damage", std::to_string(extra)}}});
        return amount + extra;
    }
    if (detail::has_effect(a.effects, detail::EffectKind::reduced))
    {
        const int less = roll(4);
        log("Reduce subtracts " + std::to_string(less) + " damage.",
            {"Reduce subtracts {damage} damage.", {{"damage", std::to_string(less)}}});
        return std::max(1, amount - less);
    }
    return amount;
}

int Session::frenzy_damage(Actor &berserker, bool critical)
{
    // Frenzy: raging and reckless, the turn's first Strength-based hit deals a
    // d6 per point of Rage Damage. The Reckless effect records that it struck.
    if (!def(berserker).frenzy || actors_[turn_].source.id != berserker.source.id)
    {
        return 0;
    }
    for (auto &e : berserker.effects.active)
    {
        if (e.kind == detail::EffectKind::reckless && !e.dc)
        {
            e.dc = 1;
            const int extra = dice({def(berserker).rage_damage, 6, 0}, critical);
            log("Frenzy adds " + std::to_string(extra) + " damage.",
                {"Frenzy adds {damage} damage.", {{"damage", std::to_string(extra)}}});
            return extra;
        }
    }
    return 0;
}

void Session::attack_recklessly(Actor &a)
{
    // Until the start of its next turn: Advantage on its Strength attack rolls,
    // and attack rolls against it have Advantage.
    detail::apply_poisoned(a.effects, effect_source(a), a.source.name, next_turn_ms(a),
                           detail::EffectKind::reckless);
    log(a.source.name + " attacks recklessly.",
        {"{name} attacks recklessly.", {{"name", a.source.name}}});
}

void Session::stunning_strike(Actor &monk, Actor &target)
{
    monk.stun_used = true;
    --monk.surges[CharacterClass::monk];
    log(monk.source.name + " attempts a Stunning Strike.",
        {"{name} attempts a Stunning Strike.", {{"name", monk.source.name}}});
    if (!saving_throw_succeeds(target, detail::Ability::constitution, def(monk).focus_dc))
    {
        detail::apply_poisoned(target.effects, effect_source(monk), monk.source.name,
                               next_turn_ms(monk), detail::EffectKind::stunned);
        log(target.source.name + " is Stunned.",
            {"{name} is Stunned.", {{"name", target.source.name}}});
        return;
    }
    // Resisted: Speed halved, and the next attack roll against it has Advantage.
    detail::apply_poisoned(target.effects, effect_source(monk), monk.source.name,
                           next_turn_ms(monk), detail::EffectKind::staggered);
    detail::apply_guiding_bolt(target.effects, effect_source(monk), monk.source.name,
                               next_turn_ms(monk));
    log(target.source.name +
            " is staggered: Speed halved, and the next attack against it has Advantage.",
        {"{name} is staggered: Speed halved, and the next attack against it has Advantage.",
         {{"name", target.source.name}}});
}

std::optional<std::pair<CunningStrike, std::optional<CunningStrike>>>
Session::cunning_choice(std::string_view verb)
{
    for (const std::string_view base : {"melee_", "ranged_"})
    {
        if (!verb.starts_with(base))
        {
            continue;
        }
        const auto rest = verb.substr(base.size());
        const auto plus = rest.find('+');
        const auto named = [](std::string_view suffix) -> std::optional<CunningStrike>
        {
            for (const auto &option : cunning_strikes)
            {
                if (option.suffix == suffix)
                {
                    return option.kind;
                }
            }
            return std::nullopt;
        };
        const auto first = named(rest.substr(0, plus));
        if (!first)
        {
            return std::nullopt;
        }
        if (plus == std::string_view::npos)
        {
            return std::pair{*first, std::optional<CunningStrike>{}};
        }
        const auto second = named(rest.substr(plus + 1));
        if (!second)
        {
            return std::nullopt;
        }
        return std::pair{*first, second};
    }
    return std::nullopt;
}

void Session::cunning_effect(Actor &rogue, Actor &target, CunningStrike kind)
{
    const auto &d = def(rogue);
    if (kind == CunningStrike::stealth)
    {
        log(rogue.source.name + " stays hidden (Stealth Attack).",
            {"{name} stays hidden (Stealth Attack).", {{"name", rogue.source.name}}});
        return;
    }
    // The saves are against 8 + Dexterity + Proficiency Bonus.
    const int dc = 8 + d.dexterity + 2 + (d.level - 1) / 4;
    const bool standing = !target.life.dead && target.life.hp > 0;
    if (kind == CunningStrike::daze)
    {
        if (!standing || saving_throw_succeeds(target, detail::Ability::constitution, dc))
        {
            return;
        }
        detail::apply_poisoned(target.effects, effect_source(rogue), rogue.source.name,
                               next_save_ms(target.source.id), detail::EffectKind::dazed);
        log(target.source.name + " is dazed.",
            {"{name} is dazed.", {{"name", target.source.name}}});
        return;
    }
    if (kind == CunningStrike::poison)
    {
        if (!standing ||
            saving_throw_succeeds(target, detail::Ability::constitution, dc, def(target).dwarf))
        {
            return;
        }
        detail::apply_repeating_condition(
            target.effects, detail::EffectKind::envenomed, effect_source(rogue), rogue.source.name,
            detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(target.source.id)});
        log(target.source.name + " is poisoned.",
            {"{name} is poisoned.", {{"name", target.source.name}}});
        return;
    }
    if (kind == CunningStrike::knock_out)
    {
        if (!standing || saving_throw_succeeds(target, detail::Ability::constitution, dc))
        {
            return;
        }
        detail::apply_repeating_condition(
            target.effects, detail::EffectKind::knocked_out, effect_source(rogue),
            rogue.source.name,
            detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(target.source.id)});
        target.effects.prone = true;
        log(target.source.name + " is knocked out.",
            {"{name} is knocked out.", {{"name", target.source.name}}});
        end_incapacitated_effects(target);
        return;
    }
    if (kind == CunningStrike::obscure)
    {
        if (!standing || saving_throw_succeeds(target, detail::Ability::dexterity, dc))
        {
            return;
        }
        detail::apply_poisoned(target.effects, effect_source(rogue), rogue.source.name,
                               next_save_ms(target.source.id), detail::EffectKind::dazzled);
        log(target.source.name + " is Blinded.",
            {"{name} is Blinded.", {{"name", target.source.name}}});
        return;
    }
    if (kind == CunningStrike::trip)
    {
        // A Large or smaller creature saves or falls Prone.
        if (!standing || def(target).size > 3 || target.effects.prone)
        {
            return;
        }
        if (saving_throw_succeeds(target, detail::Ability::dexterity, dc))
        {
            return;
        }
        target.effects.prone = true;
        log(target.source.name + " is tripped and falls Prone.",
            {"{name} is tripped and falls Prone.", {{"name", target.source.name}}});
        return;
    }
    // Withdraw: up to half Speed without provoking Opportunity Attacks.
    if (const int feet = std::max(0, d.speed - detail::speed_penalty(rogue.effects)) / 2;
        feet > 0 && conscious(rogue) && !champion_move_)
    {
        champion_move_ = ChampionMove{.actor = rogue.source.id,
                                      .target = rogue.source.id,
                                      .remaining = feet,
                                      .origin = rogue.source.cell,
                                      .withdraw = true};
        log(rogue.source.name + " may withdraw " + std::to_string(feet) +
                " feet without provoking Opportunity Attacks.",
            {"{name} may withdraw {feet} feet without provoking Opportunity Attacks.",
             {{"name", rogue.source.name}, {"feet", std::to_string(feet)}}});
    }
}

void Session::brutal_blow(Actor &barbarian, Actor &target, BrutalBlow blow)
{
    if (blow == BrutalBlow::staggering || blow == BrutalBlow::sundering)
    {
        const bool staggering = blow == BrutalBlow::staggering;
        detail::apply_brutal_blow(
            target.effects,
            staggering ? detail::EffectKind::staggering : detail::EffectKind::sundered,
            effect_source(barbarian), barbarian.source.name, next_turn_ms(barbarian));
        if (staggering)
        {
            log(target.source.name + " staggers (Staggering Blow).",
                {"{name} staggers (Staggering Blow).", {{"name", target.source.name}}});
        }
        else
        {
            log(target.source.name + " is sundered (Sundering Blow).",
                {"{name} is sundered (Sundering Blow).", {{"name", target.source.name}}});
        }
        return;
    }
    if (blow == BrutalBlow::hamstring)
    {
        detail::apply_hamstring(target.effects, effect_source(barbarian), barbarian.source.name,
                                next_turn_ms(barbarian));
        log(target.source.name + "'s Speed drops by 15 feet (Hamstring Blow).",
            {"{name}'s Speed drops by 15 feet (Hamstring Blow).", {{"name", target.source.name}}});
        return;
    }
    push_away(barbarian, target, 3);
    // Then up to half Speed straight toward the target, provoking nothing.
    if (const int feet =
            std::max(0, def(barbarian).speed - detail::speed_penalty(barbarian.effects)) / 2;
        feet > 0 && conscious(barbarian) && !champion_move_ && !target.life.dead)
    {
        champion_move_ = ChampionMove{.actor = barbarian.source.id,
                                      .target = target.source.id,
                                      .remaining = feet,
                                      .origin = barbarian.source.cell,
                                      .toward = target.source.id};
    }
}

int Session::reliable(const Actor &a, std::string_view skill, int natural)
{
    if (natural >= 10 || !def(a).reliable.contains(skill))
    {
        return natural;
    }
    log("Reliable Talent: " + a.source.name + "'s d20 of " + std::to_string(natural) +
            " counts as 10.",
        {"Reliable Talent: {name}'s d20 of {roll} counts as 10.",
         {{"name", a.source.name}, {"roll", std::to_string(natural)}}});
    return 10;
}

int Session::evaded(const Actor &target, detail::Ability save, bool halves, bool saved,
                    int total) const
{
    if (def(target).evasion && save == detail::Ability::dexterity && halves &&
        !detail::incapacitated(target.effects))
    {
        return saved ? 0 : total / 2;
    }
    return saved ? total / 2 : total;
}

bool Session::mindless(const Actor &a) const
{
    return def(a).mindless_rage && rage_of(a);
}

bool Session::strength_attack(const Actor &a, AttackRange range) const
{
    // A melee attack without a weapon is an Unarmed Strike, always Strength;
    // a ranged one is Strength only when thrown.
    const auto &d = def(a);
    if (range == AttackRange::ranged)
    {
        return !d.ranged_weapon && d.ranged_ability == d.strength;
    }
    return !d.melee.count || d.melee_ability == d.strength;
}

bool Session::shillelagh(const Actor &a) const
{
    return def(a).shillelagh_weapon &&
           detail::has_effect(a.effects, detail::EffectKind::shillelagh);
}

int Session::weapon_magic(const Actor &a, const Actor &target, AttackRange range)
{
    const auto &d = def(a);
    if (range == AttackRange::melee && d.bane_undead && def(target).creature_type == "undead")
    {
        log("The weapon adds 2 more against the Undead.",
            {"The weapon adds 2 more against the Undead.", {}});
    }
    return weapon_magic_against(a, target, range);
}

int Session::weapon_magic_against(const Actor &a, const Actor &target, AttackRange range) const
{
    const auto &d = def(a);
    const bool ranged = range == AttackRange::ranged;
    int bonus = ranged ? d.ranged_magic : d.melee_magic;
    if (!ranged && d.bane_undead && def(target).creature_type == "undead")
    {
        bonus += 2;
    }
    if (!bonus)
    {
        bonus = magic_weapon_bonus(a);
    }
    return bonus;
}

// In hundredths. Dice that may or may not come (a Bless die, a Critical Hit's)
// are left out: they favor neither weapon.
int Session::expected_damage(const Actor &a, const Actor &target, AttackRange range) const
{
    const bool ranged = range == AttackRange::ranged;
    const auto &d = def(a);
    const int magic = weapon_magic_against(a, target, range);
    const int bonus = (ranged          ? d.ranged_bonus
                       : shillelagh(a) ? casting_bonus(d, "shillelagh")
                                       : d.melee_bonus + sacred_weapon_bonus(a)) +
                      magic - return_penalty(a);
    const int armor = armor_class(target);
    int hits_of_twenty = 0;
    for (int natural = 1; natural <= 20; ++natural)
    {
        hits_of_twenty += attack_hits(natural, bonus, armor) ? 1 : 0;
    }
    const int mode = attack_modifiers(a, target, range, AttackSource::weapon).mode();
    const int hits_of_400 = mode < 0   ? hits_of_twenty * hits_of_twenty
                            : mode > 0 ? 400 - (20 - hits_of_twenty) * (20 - hits_of_twenty)
                                       : hits_of_twenty * 20;
    const auto dice = weapon_dice(a, range);
    const int twice_average =
        dice.count ? std::max(0, dice.count * (dice.sides + 1) + 2 * (dice.bonus + magic)) : 0;
    const int attacks = 1 + d.extra_attacks + (!ranged && d.martial_arts ? 1 : 0);
    return hits_of_400 * twice_average * attacks / 8;
}

int Session::expected_damage(const Actor &a, const Command &command) const
{
    const auto &target = actor(command.target);
    if (command.verb == "melee")
    {
        return expected_damage(a, target, AttackRange::melee);
    }
    if (command.verb == "ranged")
    {
        return expected_damage(a, target, AttackRange::ranged);
    }
    if (command.verb != "shoot" && command.verb != "throw")
    {
        return 0;
    }
    // The weapon it would draw, in its hand as attack_as puts it there.
    auto armed = a;
    armed.definition =
        thrown_loadout(a, command.verb == "shoot"
                              ? carried_ranged_weapon(a)
                              : std::string_view(items_.at(command.item - 1).definition))
            .definition;
    return expected_damage(armed, target, AttackRange::ranged);
}

int Session::magic_weapon_bonus(const Actor &a) const
{
    for (const auto &effect : a.effects.active)
    {
        if (effect.kind == detail::EffectKind::magic_weapon)
        {
            return effect.dc;
        }
    }
    return 0;
}

bool Session::strikes_duplicate(const Actor &attacker, Actor &target)
{
    // Mirror Image: a d6 per duplicate left; any 3 or higher and a duplicate
    // takes the hit and vanishes. A Blinded attacker is not fooled.
    const auto image = std::find_if(target.effects.active.begin(), target.effects.active.end(),
                                    [](const auto &e)
                                    {
                                        return e.kind == detail::EffectKind::mirror_image;
                                    });
    if (image == target.effects.active.end() || detail::blinded(attacker.effects))
    {
        return false;
    }
    bool diverted = false;
    for (int duplicate = 0; duplicate < image->dc; ++duplicate)
    {
        diverted = roll(6) >= 3 || diverted;
    }
    if (!diverted)
    {
        return false;
    }
    log(attacker.source.name + " hits one of " + target.source.name +
            "'s duplicates, which vanishes.",
        {"{attacker} hits one of {name}'s duplicates, which vanishes.",
         {{"attacker", attacker.source.name}, {"name", target.source.name}}});
    if (--image->dc == 0)
    {
        target.effects.active.erase(image);
    }
    return true;
}

int Session::sacred_weapon_bonus(const Actor &a) const
{
    // The Charisma modifier, at least +1, while the weapon is imbued.
    return detail::has_effect(a.effects, detail::EffectKind::sacred_weapon)
               ? std::max(1, casting_modifier(def(a)))
               : 0;
}

detail::DamageType Session::sacred_damage_type(const Actor &a) const
{
    if (pact_damage_)
    {
        return *pact_damage_;
    }
    // The damage the Paladin chose when imbuing the weapon.
    for (const auto &effect : a.effects.active)
    {
        if (effect.kind == detail::EffectKind::sacred_weapon && effect.dc == 1)
        {
            return detail::DamageType::radiant;
        }
    }
    return def(a).melee_type;
}
} // namespace opengold::srd5::detail
