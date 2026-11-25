#include "combat_session_impl.h"

// The Session's Buck Rogers ground combat (combat_session_impl.h).

namespace opengold::original::detail
{

// Buck Rogers ground combat (buck_rogers_combat.h; docs/games/countdown.md,
// its ground combat). The fight keeps its grid, turns and computer play; a
// GroundFighter's rolls, damage, injuries, area weapons, clouds and commands
// are its game's.

// A participant of a Buck Rogers fight: its record and gear from its
// profile, a party member's Hit Points as the game kept them, and the
// difficulty (rules::OriginalStrength): a hostile creature's Hit Points as
// LOAD MONSTER scales them, which become its maximum (eclvm.pas
// EclLoadMonster), and what its side's rolls and its Armor Class gain.
GroundFighter ground_fighter(const rules::Participant &p, const std::string &name)
{
    auto gear = decode_ground_record(p.character_profile);
    if (!gear)
    {
        throw std::runtime_error("A Buck Rogers creature has no record: " + name);
    }
    GroundFighter fighter{std::move(*gear)};
    auto &creature = fighter.creature();
    const auto &strength = p.original_strength;
    if (strength.hit_points_percent != 100)
    {
        const int scaled =
            std::max(creature.hit_points * static_cast<int>(strength.hit_points_percent) / 100, 1);
        creature.hit_points = static_cast<std::uint16_t>(scaled);
        creature.hit_point_maximum = creature.hit_points;
    }
    if (p.state)
    {
        creature.hit_points = static_cast<std::uint16_t>(
            std::clamp(p.state->hit_points, 0, static_cast<int>(creature.hit_point_maximum)));
        if (p.state->dead)
        {
            set_status(creature, BuckRogersStatus::dead);
        }
    }
    fighter.to_hit = strength.to_hit;
    fighter.armor_class_bonus = strength.armor_class_bonus;
    fighter.computer_plays = p.side == Side::party && creature.companion != 0;
    return fighter;
}

// What the fight's frame reads of a Buck Rogers creature (Combatant): its Hit
// Points, movement, attacks and missile, level, Intelligence and morale, and
// its attack roll and Armor Class read descending, as the computer weighs
// them: a combat value b and an Armor Class a hit when b + d20 reaches a, so
// a THAC0 of 60 - b against an Armor Class of 60 - a (viewchar.pas
// PrintCombatStats shows the THAC0 so).
Combatant ground_combatant(const GroundFighter &fighter, const GroundNumbers &numbers)
{
    const auto &creature = fighter.creature();
    const auto &items = fighter.gear.items;
    const auto &types = fighter.gear.types;
    Combatant c;
    c.thac0 = 60 - numbers.attack_bonus;
    c.armor_class = 60 - numbers.armor_class[0];
    c.rear_armor_class = 60 - numbers.armor_class[1];
    c.hit_points = creature.hit_points;
    c.max_hit_points = creature.hit_point_maximum;
    c.movement = numbers.movement;
    const auto weapon = readied_weapon(items, types);
    const int rate =
        weapon ? type_of(items[*weapon], types)->attacks_doubled : creature.unarmed_attacks_doubled;
    for (std::size_t slot = 0; slot < c.attacks.size(); ++slot)
    {
        const auto &dice = numbers.damage[slot];
        c.attacks[slot] = {slot == 0 ? rate : creature.attacks_doubled, dice.count, dice.sides,
                           dice.bonus};
    }
    c.level = creature.level;
    c.intelligence = creature.abilities[static_cast<std::size_t>(BuckRogersAbility::intelligence)];
    c.morale = creature.npc;
    c.initiative_bonus = buck_rogers_missile_adjustment(
        creature.abilities[static_cast<std::size_t>(BuckRogersAbility::dexterity)]);
    if (const int range = readied_weapon_range(items, types); weapon && range > 1)
    {
        Missile missile;
        missile.range = range;
        missile.rate_per_two_rounds = rate;
        missile.quantity = items[*weapon].quantity;
        missile.loaded = missile.quantity > 0;
        c.missile = missile;
    }
    return c;
}

// Its numbers this moment (charhlp.pas RecalcStats): its side's tactics, the
// difficulty's Armor Class, and blindness 4 worse on both (EffBlind).
GroundNumbers Session::ground_numbers_of(const Fighter &f) const
{
    const auto &g = *f.ground;
    auto numbers = ground_numbers(g.creature(), g.gear.items, g.gear.types,
                                  tactics_[static_cast<std::size_t>(f.side)]);
    const int better = g.armor_class_bonus - (g.has(ground_effect::blind) ? 4 : 0);
    numbers.armor_class[0] += better;
    numbers.armor_class[1] += better;
    return numbers;
}

// Works its numbers out again into its record, as the original does
// (RecalcStats), and into what the fight's frame reads.
void Session::ground_refresh(Fighter &f)
{
    auto &creature = f.ground->creature();
    const auto numbers = ground_numbers_of(f);
    const auto byte = [](int value)
    {
        return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
    };
    creature.attack_bonus = byte(numbers.attack_bonus);
    creature.armor_class = {byte(numbers.armor_class[0]), byte(numbers.armor_class[1])};
    creature.armor_flags = numbers.armor_flags;
    creature.current_movement = byte(numbers.movement);
    creature.encumbrance = static_cast<std::uint16_t>(numbers.encumbrance);
    f.numbers = ground_combatant(*f.ground, numbers);
}

// The tactics rolls (cbtacts.pas RollTactics): in the party's order, each
// side's creatures may roll until one makes it.
void Session::ground_tactics()
{
    tactics_ = {};
    for (auto &f : fighters_)
    {
        auto &made = tactics_[static_cast<std::size_t>(f.side)];
        if (!f.ground || made ||
            !rolls_tactics(f.ground->creature(), f.ground->has(ground_effect::stunned)) ||
            !ground_skill_succeeds(f.ground->creature(), Skill::battle_tactics, random_))
        {
            continue;
        }
        made = true;
        log({"{name} makes the tactics roll.", {{"name", f.name}}});
    }
}

// A round begins (combat.pas CombatMain; cbtacts.pas BeginRound). As the
// fight begins, Leadership takes companions off the computer's control
// (UseLeadership). Then the tactics rolls; each creature's attacks, its
// initiative (d10, its Dexterity's missile adjustment and 1, at least 2, 6
// less when surprised in the first round, 0 outside 0 to 20) with a d127 to
// break ties, and its movement; and the round's order, highest first
// (combat.pas OrderCreatures).
void Session::ground_begin_round()
{
    if (round_ == 1)
    {
        for (auto &leader : fighters_)
        {
            if (!leader.ground || leader.side != Side::party ||
                !may_lead(leader.ground->creature()))
            {
                continue;
            }
            for (auto &follower : fighters_)
            {
                if (!follower.ground || follower.side != Side::party ||
                    !follower.ground->computer_plays ||
                    !may_follow(follower.ground->creature(),
                                follower.ground->has(ground_effect::unkillable)) ||
                    !ground_skill_succeeds(leader.ground->creature(), Skill::leadership, random_))
                {
                    continue;
                }
                follower.ground->computer_plays = false;
                log({"{leader} leads {name}.", {{"leader", leader.name}, {"name", follower.name}}});
            }
        }
    }
    ground_tactics();
    for (auto &f : fighters_)
    {
        f.initiative = 0;
        if (!f.ground)
        {
            continue;
        }
        auto &g = *f.ground;
        const auto &creature = g.creature();
        const auto attacks =
            ground_attacks(creature, g.gear.items, g.gear.types, static_cast<int>(round_) - 1,
                           g.has(ground_effect::dizzy));
        f.attacks_left = attacks;
        f.attack_phase = 1;
        g.attacked = false;
        f.cast_allowed = f.item_use_allowed = false;
        if (f.able())
        {
            const int dexterity =
                creature.abilities[static_cast<std::size_t>(BuckRogersAbility::dexterity)];
            f.initiative = std::max(roll(10) + buck_rogers_missile_adjustment(dexterity) + 1, 2);
            if (round_ == 1 && f.surprised)
            {
                f.initiative -= 6;
            }
            if (f.initiative < 0 || f.initiative > 20)
            {
                f.initiative = 0;
            }
            g.tie_break = roll(127);
        }
        ground_refresh(f);
        const int bonus = f.side == Side::party ? party_move_modifier_ : 0;
        f.half_moves = 2 * (f.numbers.movement + bonus);
    }
    turn_order_.clear();
    for (const auto &f : fighters_)
    {
        auto place = turn_order_.begin();
        while (place != turn_order_.end())
        {
            const auto &other = *find(*place);
            if (f.initiative > other.initiative ||
                (f.initiative == other.initiative && f.ground && other.ground &&
                 other.ground->tie_break < f.ground->tie_break))
            {
                break;
            }
            ++place;
        }
        turn_order_.insert(place, f.id);
    }
}

// Its turn starts (combat.pas CreatureAction): dodging and a failed Zero-G
// roll end; helplessness, fear and a stun each cost it the turn, and the
// fear goes (effhndlr.pas EffHelpless, EffScared, EffStunned); in zero
// gravity it rolls Maneuver in Zero-G, and a failure leaves it 2 worse to
// hit with a quarter of its movement (cbtacts.pas RollZeroG). False when it
// does not act.
bool Session::ground_turn_starts(Fighter &f)
{
    auto &g = *f.ground;
    auto &creature = g.creature();
    const auto status = status_of(creature);
    if (status == BuckRogersStatus::dodging || status == BuckRogersStatus::failed_zero_g)
    {
        set_status(creature, BuckRogersStatus::okay);
    }
    for (const auto [code, message] : {std::pair{ground_effect::helpless, "{name} is helpless."},
                                       std::pair{ground_effect::scared, "{name} is intimidated."},
                                       std::pair{ground_effect::stunned, "{name} is stunned."}})
    {
        if (g.has(code) && f.initiative > 0)
        {
            end_actions(f);
            log({message, {{"name", f.name}}});
        }
    }
    const auto scared = std::ranges::find(g.effects, ground_effect::scared, &GroundEffect::code);
    if (scared != g.effects.end())
    {
        g.effects.erase(scared);
    }
    if (f.initiative <= 0)
    {
        return false;
    }
    ground_refresh(f);
    if (rolls_zero_g(zero_gravity_, creature.enabled != 0, f.initiative))
    {
        if (ground_skill_succeeds(creature, Skill::maneuver_in_zero_g, random_))
        {
            log({"{name} makes the Zero-G roll.", {{"name", f.name}}});
        }
        else
        {
            log({"{name} fails the Zero-G roll.", {{"name", f.name}}});
            set_command_status(creature, BuckRogersStatus::failed_zero_g);
            f.half_moves = zero_g_movement(f.half_moves);
            ground_refresh(f);
        }
    }
    return true;
}

bool Session::ground_cloud_at(Cell cell, CloudMatter matter) const
{
    return std::ranges::any_of(ground_clouds_,
                               [cell, matter](const GroundCloud &cloud)
                               {
                                   return cloud.matter == matter && cloud.covers_square(cell);
                               });
}

// A line walked as the original walks it (pathing.pas WalkLine): it sees
// chaff and mist on any square, cover (line class 2 ground, which may be
// impassable) past its first two squares where no cloud lies, and stops on a
// wall, on chaff when asked to, or past `range` straight steps (its doubled
// distance past 2 * range + 1).
Session::GroundWalk Session::ground_walk(Cell from, Cell to, int range, bool stops_at_chaff) const
{
    GroundWalk walk;
    LineWalk line(from, to);
    const int limit = range * 2 + 1;
    int grace = 2;
    do
    {
        const Cell at = line.at;
        bool stop = false;
        if (ground_cloud_at(at, CloudMatter::chaff))
        {
            walk.chaff = true;
            stop = stops_at_chaff;
        }
        else if (ground_cloud_at(at, CloudMatter::mist))
        {
            walk.mist = true;
        }
        else if (grace > 0)
        {
            --grace;
        }
        else if (!ground_cloud_at(at, CloudMatter::gas) &&
                 (board_.at(at) == rules::Terrain::cover ||
                  board_.at(at) == rules::Terrain::impassable_cover))
        {
            walk.blocked = true;
        }
        const bool wall = board_.at(at) == rules::Terrain::obstacle && from != to;
        if (wall || stop || line.distance > limit)
        {
            walk.stop = at;
            walk.distance = line.distance;
            return walk;
        }
    } while (line.next());
    walk.reached = true;
    walk.stop = to;
    walk.distance = line.distance;
    return walk;
}

// What lies on the line between an attacker and its defender: chaff and mist
// walking from the defender (cbtacts.pas CalcWeaponDamage), cover walking
// from the attacker (ApplyRangePenalty).
GroundLine Session::ground_line(const Fighter &attacker, const Fighter &defender) const
{
    const auto toward = ground_walk(defender.cell, attacker.cell, 255, false);
    const auto from = ground_walk(attacker.cell, defender.cell, 255, false);
    return {distance(attacker, defender), from.blocked, toward.chaff, toward.mist};
}

// Whether its readied weapon may strike a target (cbtacts.pas AimTargetMenu):
// within its range and in a line no wall stops, and with rounds left in a
// weapon that is not a melee one.
bool Session::ground_reaches(const Fighter &attacker, const Fighter &target) const
{
    const auto &g = *attacker.ground;
    const auto &items = g.gear.items;
    const auto &types = g.gear.types;
    if (distance(attacker, target) > readied_weapon_range(items, types) ||
        !sees(board_, attacker, target))
    {
        return false;
    }
    const auto weapon = readied_weapon(items, types);
    return !weapon || weapon_class_of(items, types) == WeaponClass::melee ||
           items[*weapon].quantity > 0;
}

// A Buck Rogers attack (cbtacts.pas MakeAttack and RollAttacks). The
// defender turns to face it unless it has drawn two attacks; an area weapon
// launches at the defender's square. Otherwise each attack left, the second
// slot's first, rolls against the defender's Armor Class from the front or
// the rear (target_number); a helpless defender is always hit. A backstab
// multiplies the damage; what stops a hit takes it away; a Sonic Stunner
// stuns instead. A poisonous creature's hits from its second slot poison. A
// ranged weapon spends a round for each first-slot attack.
void Session::ground_attack(Fighter &attacker, Fighter &defender)
{
    auto &a = *attacker.ground;
    auto &items = a.gear.items;
    const auto &types = a.gear.types;
    if (defender.flank_count < 2)
    {
        defender.direction = direction_toward(defender.cell, attacker.cell);
    }
    const int blow = direction_toward(attacker.cell, defender.cell);
    attacker.direction = blow;
    attacker.lock = defender.id;
    attacker.facing_left = defender.cell.x < attacker.cell.x;
    const auto weapon = readied_weapon(items, types);
    if (weapon && projectile_kind(items[*weapon]) != 0)
    {
        if (items[*weapon].quantity > 0)
        {
            ground_launch(attacker, defender.cell);
        }
        else
        {
            end_actions(attacker);
        }
        return;
    }
    if (attacker.attacks() == 0 || !defender.ground)
    {
        return;
    }
    auto &d = *defender.ground;
    ground_refresh(defender);
    const auto defending = ground_numbers_of(defender);
    const auto attacking = ground_numbers_of(attacker);
    const bool one_square = defender.footprint.width == 1 && defender.footprint.height == 1;
    const bool rear = rear_attack(defender.direction, blow, defender.flank_count, one_square);
    const int range = readied_weapon_range(items, types);
    const bool backstab = rear && backstab_position(defender.direction, blow, range) &&
                          backstab_check(a.creature(), random_);
    const auto line = ground_line(attacker, defender);
    const AttackSetting setting{rear, a.creature().career == 5, range > 1, attacker.guarding, range,
                                line};
    const int target = target_number(defending, setting);
    const std::optional<WeaponClass> weapon_class =
        weapon ? std::optional{weapon_class_of(items, types)} : std::nullopt;
    const bool stunner = weapon && items[*weapon].type == ground_item::sonic_stunner;
    if (backstab)
    {
        log({"{attacker} backstabs {name}.",
             {{"attacker", attacker.name}, {"name", defender.name}}});
    }
    bool hit_any = false;
    int first_slot_attacks = 0;
    a.attacked = true;
    for (int phase = attacker.attack_phase; phase >= 0 && defender.able(); --phase)
    {
        auto &left = attacker.attacks_left[static_cast<std::size_t>(phase)];
        while (left > 0 && defender.able() && attacker.able())
        {
            --left;
            attacker.attack_phase = phase;
            first_slot_attacks += phase == 0 ? 1 : 0;
            const int natural = roll(20);
            const int modifier = a.to_hit - (a.has(ground_effect::blind) ? 4 : 0);
            if (!ground_attack_hits(natural, attacking.attack_bonus, modifier, target) &&
                !defender.helpless())
            {
                continue;
            }
            hit_any = true;
            if (stunner)
            {
                const bool saved = roll_ground_save(d.creature(), ground_save::stun, 0,
                                                    d.has(ground_effect::blind), random_);
                const bool stunned =
                    give_ground_effect(d.effects, {ground_effect::stunned, stun_rounds(random_), 0},
                                       saved, false, true);
                log({stunned ? "{name} is stunned." : "{name} is unaffected.",
                     {{"name", defender.name}}});
                continue;
            }
            const auto &dice = attacking.damage[static_cast<std::size_t>(phase)];
            int rolled = 0;
            for (int die = 0; die < dice.count; ++die)
            {
                rolled += roll(std::max(dice.sides, 1));
            }
            int damage = std::max(rolled + dice.bonus, 0);
            if (backstab)
            {
                damage = backstab_damage(damage, a.creature());
            }
            const HitDefense defense{weapon_class, line.distance, line, defending.armor_flags,
                                     defender.hit_points};
            damage = damage_after_defenses(damage, defense, d.effects, random_);
            log({"{attacker} hits {name} for {damage}.",
                 {{"attacker", attacker.name},
                  {"name", defender.name},
                  {"damage", std::to_string(damage)}}});
            ground_inflict(defender, damage);
            if (phase == 1 && damage > 0 && defender.able())
            {
                if (const auto dizzying = std::ranges::find(
                        a.effects, ground_effect::dizzying_poison, &GroundEffect::code);
                    dizzying != a.effects.end())
                {
                    const bool saved =
                        roll_ground_save(d.creature(), ground_save::gas, dizzying->strength,
                                         d.has(ground_effect::blind), random_);
                    if (give_ground_effect(d.effects, {ground_effect::dizzy, 0, 0}, saved, false,
                                           false))
                    {
                        log({"{name} is poisoned and dizzy.", {{"name", defender.name}}});
                    }
                }
                if (const auto lethal = std::ranges::find(a.effects, ground_effect::lethal_poison,
                                                          &GroundEffect::code);
                    lethal != a.effects.end() &&
                    !roll_ground_save(d.creature(), ground_save::gas, lethal->strength,
                                      d.has(ground_effect::blind), random_))
                {
                    poison_kills(d.creature());
                    ground_down(defender);
                    log({"{name} is poisoned.", {{"name", defender.name}}});
                }
            }
        }
    }
    if (!hit_any)
    {
        log({"{attacker} misses {name}.", {{"attacker", attacker.name}, {"name", defender.name}}});
    }
    if (weapon && weapon_class != WeaponClass::melee &&
        spend_rounds(items[*weapon], *type_of(items[*weapon], types), first_slot_attacks))
    {
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(*weapon));
    }
    ground_refresh(attacker);
    if (attacker.attacks() == 0 || !attacker.able())
    {
        end_actions(attacker);
    }
}

// A missile or grenade fired at a square (cbtacts.pas
// CombatLaunchProjectile). Its damage is rolled once; the rocket launcher
// and plasma thrower, and a grenade launcher, must be readied again. A shot
// whose combat value and d20, less the range's penalty, come to 50 or less
// goes astray, up to four tries at a square near the shooter; chaff on its
// way sets it off there. Every creature its blast reaches saves or takes
// the damage and the effect; a cloud stays. It ends the shooter's turn and
// spends a round, the last of a grenade bundle with it. The original put the
// launcher away before it rolled, so the shot went as if unarmed, without
// the weapon's range; and it threw away a stray shot's fourth landing; here
// the shot is the weapon's and every landing counts (docs/games/countdown.md,
// Bugs).
void Session::ground_launch(Fighter &attacker, Cell aim)
{
    auto &g = *attacker.ground;
    auto &items = g.gear.items;
    const auto &types = g.gear.types;
    const auto weapon = *readied_weapon(items, types);
    const auto &type = *type_of(items[weapon], types);
    const int kind = projectile_kind(items[weapon]);
    const auto &shot = projectile(kind);
    const auto weapon_class = static_cast<WeaponClass>(type.weapon_class);
    const int range = readied_weapon_range(items, types);
    const int attack_bonus = ground_numbers_of(attacker).attack_bonus;
    const auto blast = roll_blast(kind, type, items[weapon].plus, random_);
    g.attacked = true;
    const bool launched =
        put_away_launcher(items, types, weapon) || weapon_class != WeaponClass::grenade;
    log({launched ? "{name} launches a missile." : "{name} throws a grenade.",
         {{"name", attacker.name}}});
    const Cell from = attacker.cell;
    const int across = std::abs(aim.x - from.x), down = std::abs(aim.y - from.y);
    const int far = across > down ? across + down / 2 : down + across / 2;
    if (projectile_strays(attack_bonus, roll(20), far, range))
    {
        int limit = 4;
        for (int tries = 0; tries < 4; ++tries)
        {
            const Cell landing{from.x + scatter_offset(aim.x - from.x, random_),
                               from.y + scatter_offset(aim.y - from.y, random_)};
            if (!board_.contains(landing))
            {
                continue;
            }
            const auto walk = ground_walk(from, landing, limit, false);
            limit = walk.distance;
            if (walk.reached)
            {
                aim = landing;
                log({"It goes astray.", {}});
                break;
            }
        }
    }
    if (const auto way = ground_walk(from, aim, 255, true); !way.reached && way.chaff)
    {
        aim = way.stop;
        log({"It hits chaff.", {}});
    }
    std::vector<std::pair<int, Fighter *>> reached;
    for (auto &f : fighters_)
    {
        if (!f.able() || !f.ground)
        {
            continue;
        }
        int nearest = std::numeric_limits<int>::max();
        for (const auto square : squares_of(f))
        {
            if (const auto walk = ground_walk(aim, square, shot.radius, false); walk.reached)
            {
                nearest = std::min(nearest, walk.distance);
            }
        }
        if (nearest != std::numeric_limits<int>::max())
        {
            reached.emplace_back(nearest, &f);
        }
    }
    std::ranges::stable_sort(reached, {}, &std::pair<int, Fighter *>::first);
    if (shot.lingers)
    {
        if (const auto matter = cloud_of(kind))
        {
            ground_clouds_.insert(ground_clouds_.begin(),
                                  lay_ground_cloud(aim, *matter, zero_gravity_,
                                                   [this](Cell cell)
                                                   {
                                                       return board_.contains(cell) &&
                                                              !rules::blocks_movement(
                                                                  board_.at(cell));
                                                   }));
        }
    }
    const bool dazzle = items[weapon].type == ground_item::dazzle_grenade;
    const bool stun_grenade = items[weapon].type == ground_item::stun_grenade;
    for (const auto &[gap, target] : reached)
    {
        auto &t = *target->ground;
        const bool saved =
            spared_by_gear(kind, ground_numbers_of(*target).armor_flags) ||
            roll_ground_save(t.creature(), shot.save, 0, t.has(ground_effect::blind), random_);
        if (blast.damage > 0)
        {
            int damage = blast.damage;
            // Non-weapon damage meets two effects (fxengine.pas
            // InflictNonWeaponDamage): an immunity to heat and plasma, and
            // the unkillable one's luck.
            if (t.has(ground_effect::immune_to_heat_and_plasma) &&
                (weapon_class == WeaponClass::plasma || weapon_class == WeaponClass::heat))
            {
                damage = 0;
            }
            if (t.has(ground_effect::unkillable))
            {
                damage = std::min(damage, target->hit_points);
            }
            damage = blast_damage(damage, shot.saved, saved);
            if (damage > 0)
            {
                log({"{name} takes {damage}.",
                     {{"name", target->name}, {"damage", std::to_string(damage)}}});
                ground_inflict(*target, damage);
            }
        }
        if (target->able() && shot.effect > 0)
        {
            const bool given = give_ground_effect(t.effects, {shot.effect, blast.minutes, 0},
                                                  saved && shot.saved == SaveEffect::cancels,
                                                  dazzle, stun_grenade);
            log({given ? "{name} is affected." : "{name} is unaffected.",
                 {{"name", target->name}}});
        }
    }
    end_actions(attacker);
    if (spend_rounds(items[weapon], type, 1))
    {
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(weapon));
    }
    ground_refresh(attacker);
}

// Damage (charhlp.pas InflictDamage): down at 0 and below (ground_down).
void Session::ground_inflict(Fighter &target, int damage)
{
    if (!target.able())
    {
        return;
    }
    auto &g = *target.ground;
    inflict_ground_damage(g.creature(), damage, g.dying_count);
    target.hit_points = g.creature().hit_points;
    const auto status = status_of(g.creature());
    if (can_act(status))
    {
        return;
    }
    ground_down(target);
    log({status == BuckRogersStatus::unconscious ? "{name} goes down."
         : status == BuckRogersStatus::dying     ? "{name} goes down, and is dying."
         : status == BuckRogersStatus::comatose  ? "{name} goes down, comatose."
                                                 : "{name} is killed.",
         {{"name", target.name}}});
}

// It goes down: out of the fight's turns, without the effects that disable
// (fxengine.pas RemoveIncapacitatingEffects). A bleeding one is the fight's
// dying; the dead and gone its dead; the rest, unconscious, comatose or
// poisoned, lie as the unconscious do.
void Session::ground_down(Fighter &f)
{
    auto &creature = f.ground->creature();
    remove_incapacitating(f.ground->effects);
    creature.enabled = 0;
    f.hit_points = creature.hit_points;
    const auto status = status_of(creature);
    f.condition = status == BuckRogersStatus::dying ? Condition::dying
                  : status == BuckRogersStatus::dead || status == BuckRogersStatus::gone
                      ? Condition::dead
                      : Condition::unconscious;
    end_actions(f);
}

// The round ends (combat.pas EndOfRound): sprinters and jetpack fliers are
// okay again; a minute passes for every effect; the clouds count down, and
// those that clear go (fxengine.pas CountDownAreaEffects); the bleeding
// count on, slipping into a coma past 9 (charhlp.pas ProcessDyingCharacters).
void Session::ground_end_round()
{
    for (auto &f : fighters_)
    {
        if (!f.ground)
        {
            continue;
        }
        auto &creature = f.ground->creature();
        const auto status = status_of(creature);
        if (status == BuckRogersStatus::sprinting || status == BuckRogersStatus::used_jetpack)
        {
            set_status(creature, BuckRogersStatus::okay);
        }
        age_ground_effects(f.ground->effects);
    }
    const auto before = ground_clouds_.size();
    std::erase_if(ground_clouds_,
                  [](GroundCloud &cloud)
                  {
                      return !cloud_lingers(cloud);
                  });
    if (ground_clouds_.size() != before)
    {
        log({"The air clears a little.", {}});
    }
    for (auto &f : fighters_)
    {
        if (f.ground && bleed(f.ground->creature(), f.ground->dying_count))
        {
            f.condition = Condition::unconscious;
            log({"{name} slips into a coma.", {{"name", f.name}}});
        }
    }
}

// The guards strike a creature that moves or lands near them (cbtacts.pas
// GuardersAttack): each enemy guard not helpless, with a ranged weapon or
// next to it, whose line reaches it within the weapon's range (any line, for
// one flying on a jetpack), strikes once and stops guarding.
void Session::ground_guards_strike(Fighter &mover)
{
    const bool flying = status_of(mover.ground->creature()) == BuckRogersStatus::using_jetpack;
    for (auto &guard : fighters_)
    {
        if (!mover.able() || !guard.ground || !guard.guarding || guard.side == mover.side ||
            !guard.able() || guard.helpless() || !sees(board_, guard, mover))
        {
            continue;
        }
        const int range = readied_weapon_range(guard.ground->gear.items, guard.ground->gear.types);
        if (range <= 1 && distance(guard, mover) > 1)
        {
            continue;
        }
        if (!flying && !ground_walk(guard.cell, mover.cell, range, false).reached)
        {
            continue;
        }
        note_attack(guard, mover);
        log({"{attacker} strikes from guard.", {{"attacker", guard.name}}});
        make_attack(guard, mover, false);
        guard.guarding = false;
    }
}

// After a step (combat.pas MoveMenu): the guards strike, and gas on its new
// square may leave it helpless (fxengine.pas ConsiderTerrainEffects), which
// ends its turn.
void Session::ground_after_step(Fighter &mover)
{
    ground_guards_strike(mover);
    auto &g = *mover.ground;
    if (mover.able() && ground_cloud_at(mover.cell, CloudMatter::gas))
    {
        if (const auto minutes = gassed(g.creature(), ground_numbers_of(mover).armor_flags,
                                        mover.helpless(), g.has(ground_effect::blind), random_);
            minutes && give_ground_effect(g.effects, {ground_effect::helpless, *minutes, 0}, false,
                                          false, false))
        {
            log({"{name} is helpless.", {{"name", mover.name}}});
        }
    }
    if (!mover.able() || mover.helpless())
    {
        end_actions(mover);
    }
}

// Intimidate (cbtacts.pas IntimidateTarget).
void Session::ground_intimidate(Fighter &f)
{
    if (!ground_skill_succeeds(f.ground->creature(), Skill::intimidate, random_))
    {
        log({"{name} fails to intimidate.", {{"name", f.name}}});
        return;
    }
    log({"{name} looks real menacing.", {{"name", f.name}}});
    for (auto &enemy : fighters_)
    {
        if (!enemy.ground || enemy.side == f.side || !enemy.able() || enemy.helpless() ||
            distance(f, enemy) > 1 || !sees(board_, f, enemy))
        {
            continue;
        }
        const bool saved = roll_ground_save(enemy.ground->creature(), ground_save::stun, 0,
                                            enemy.ground->has(ground_effect::blind), random_);
        const bool scared =
            give_ground_effect(enemy.ground->effects, scared_effect(), saved, false, false);
        log({scared ? "{name} is intimidated." : "{name} is unaffected.", {{"name", enemy.name}}});
    }
}

// AID's patients (viewchar.pas FindPatients, by the after-fight rules'
// find_patients): of the medic and its side's creatures on its square or next
// to it, those its skills can treat. The candidates, the medic first, as the
// after-fight rules take a team, and the patients among them.
struct GroundPatients
{
    std::vector<std::size_t> fighters;
    std::vector<TeamMember> team;
    std::vector<std::size_t> patients;
};

GroundPatients ground_patients(const std::vector<Fighter> &fighters, const Fighter &medic)
{
    GroundPatients found;
    found.fighters.push_back(0);
    found.team.push_back({});
    for (std::size_t index = 0; index < fighters.size(); ++index)
    {
        const auto &f = fighters[index];
        if (!f.ground || f.side != medic.side || f.condition == Condition::fled ||
            std::abs(f.cell.x - medic.cell.x) > 1 || std::abs(f.cell.y - medic.cell.y) > 1)
        {
            continue;
        }
        TeamMember member;
        member.character = f.ground->creature();
        // The after-fight rules know the team by its record's side: the
        // fight's side counts.
        member.character.side = f.side == Side::party ? 0 : 1;
        member.items = f.ground->gear.items;
        member.stunned =
            f.ground->has(ground_effect::stunned) || f.ground->has(ground_effect::dizzy);
        const std::size_t at = f.id == medic.id ? 0 : found.team.size();
        if (at == 0)
        {
            found.fighters[0] = index;
            found.team[0] = std::move(member);
        }
        else
        {
            found.fighters.push_back(index);
            found.team.push_back(std::move(member));
        }
    }
    found.patients = find_patients(0, found.team);
    return found;
}

bool Session::ground_has_patient(const Fighter &medic) const
{
    return !ground_patients(fighters_, medic).patients.empty();
}

// Aid (viewchar.pas TreatPatients, by the after-fight rules'
// treat_patients): each bleeding patient is bandaged, unconscious and no
// longer bleeding, and each stunned or dizzy one may be treated on a Treat
// Stun/Paralysis check.
void Session::ground_aid(Fighter &medic)
{
    auto found = ground_patients(fighters_, medic);
    for (const auto &done : treat_patients(0, found.team, found.patients, random_))
    {
        auto &patient = fighters_[found.fighters[done.patient]];
        auto &p = *patient.ground;
        if (done.kind == PatientTreatment::Kind::bandaged)
        {
            p.creature().status = found.team[done.patient].character.status;
            p.creature().enabled = 0;
            p.dying_count = 0;
            patient.condition = Condition::unconscious;
            log({"{name} is bandaged.", {{"name", patient.name}}});
            continue;
        }
        std::erase_if(p.effects,
                      [](const GroundEffect &e)
                      {
                          return e.code == ground_effect::dizzy || e.code == ground_effect::stunned;
                      });
        log({"{name} is treated.", {{"name", patient.name}}});
    }
}

// Jetpack (cbtacts.pas CombatUseJetpack): on a Use Jetpack check it takes
// off, to land where it chooses (ground_land); on a failure it lurches
// through eleven random squares near it, lands on the last it may stand on,
// meets the guards there and ends its turn.
void Session::ground_jetpack(Fighter &f)
{
    auto &g = *f.ground;
    if (ground_skill_succeeds(g.creature(), Skill::use_jetpack, random_))
    {
        set_command_status(g.creature(), BuckRogersStatus::using_jetpack);
        g.flying = true;
        log({"{name} takes off.", {{"name", f.name}}});
        return;
    }
    log({"{name} fails at use jetpack.", {{"name", f.name}}});
    set_command_status(g.creature(), BuckRogersStatus::using_jetpack);
    f.half_moves = 0;
    Cell at = f.cell, landing = f.cell;
    for (int step = 0; step < lurch_steps; ++step)
    {
        do
        {
            at = lurch_step(at, random_);
        } while (!board_.contains(at));
        auto moved = f;
        moved.cell = at;
        if (!rules::blocks_movement(board_.at(at)) && cell_free(moved) && at != f.cell)
        {
            landing = at;
        }
    }
    f.cell = landing;
    ground_guards_strike(f);
    if (f.able())
    {
        set_command_status(g.creature(), BuckRogersStatus::used_jetpack);
    }
    end_actions(f);
}

// A jetpack flight lands: the guards strike as it leaves and as it lands; it
// may still attack, but moves no more this round.
void Session::ground_land(Fighter &f, Cell landing)
{
    auto &g = *f.ground;
    g.flying = false;
    ground_guards_strike(f);
    f.half_moves = 0;
    if (!f.able())
    {
        return;
    }
    f.facing_left = landing.x < f.cell.x || (landing.x == f.cell.x && f.facing_left);
    f.cell = landing;
    ground_guards_strike(f);
    if (f.able())
    {
        set_command_status(g.creature(), BuckRogersStatus::used_jetpack);
    }
}

// The commands a Buck Rogers creature has (combat.pas CombatMenu): Move,
// Target (a blow or a shot at an enemy, or a launch at a square), the new
// commands (ground_menu), Wait, Quick, End and fleeing off the field; while
// it flies, only landing; and only Quick for a companion the computer plays.
std::vector<rules::Command> Session::ground_commands(const Fighter &actor) const
{
    std::vector<rules::Command> commands;
    const auto add = [&](std::string verb, EntityId target = 0, Cell destination = {})
    {
        rules::Command command;
        command.revision = revision_;
        command.actor = actor.id;
        command.verb = std::move(verb);
        command.target = target;
        command.destination = destination;
        command.intent = target != 0 ? rules::Intent::hostile : rules::Intent{};
        commands.push_back(std::move(command));
    };
    const auto &g = *actor.ground;
    const auto &creature = g.creature();
    const auto &items = g.gear.items;
    const auto &types = g.gear.types;
    if (g.computer_plays)
    {
        add("quick");
        return commands;
    }
    const auto open = [this, &actor](Cell cell)
    {
        auto moved = actor;
        moved.cell = cell;
        return board_.contains(cell) && !rules::blocks_movement(board_.at(cell)) &&
               cell_free(moved);
    };
    if (g.flying)
    {
        for (int y = 0; y < board_.height; ++y)
        {
            for (int x = 0; x < board_.width; ++x)
            {
                if (open({x, y}))
                {
                    add("land", 0, {x, y});
                }
            }
        }
        return commands;
    }
    for (const auto &cell : movement_reach(actor.id))
    {
        add("move", 0, cell);
    }
    const auto weapon = readied_weapon(items, types);
    if (actor.attacks() > 0 && may_strike(creature, items, types))
    {
        const bool area = weapon && projectile_kind(items[*weapon]) != 0;
        if (area && items[*weapon].quantity > 0)
        {
            const int range = readied_weapon_range(items, types);
            for (int y = 0; y < board_.height; ++y)
            {
                for (int x = 0; x < board_.width; ++x)
                {
                    const Cell cell{x, y};
                    const int dx = std::abs(x - actor.cell.x), dy = std::abs(y - actor.cell.y);
                    if (cell != actor.cell &&
                        (2 * std::max(dx, dy) + std::min(dx, dy)) / 2 <= range &&
                        !rules::blocks_movement(board_.at(cell)) &&
                        ground_walk(actor.cell, cell, 255, false).reached)
                    {
                        add("launch", 0, cell);
                    }
                }
            }
        }
        for (const auto &f : fighters_)
        {
            if (!area && f.side != actor.side && f.able() && ground_reaches(actor, f))
            {
                add(distance(actor, f) > 1 ? "ranged" : "melee", f.id);
            }
        }
    }
    const bool enemy_next_to =
        std::ranges::any_of(fighters_,
                            [&actor](const Fighter &f)
                            {
                                return f.side != actor.side && f.able() && distance(actor, f) <= 1;
                            });
    const bool patient = ground_has_patient(actor);
    const auto menu = ground_menu(creature, items, types,
                                  {actor.half_moves > 0, enemy_next_to, patient, outdoors_});
    for (const auto &[offered, verb] :
         {std::pair{menu.intimidate, "intimidate"}, std::pair{menu.aid, "aid"},
          std::pair{menu.sprint, "sprint"}, std::pair{menu.dodge, "dodge"},
          std::pair{menu.jetpack, "jetpack"}, std::pair{menu.guard, "guard"}})
    {
        if (offered)
        {
            add(verb);
        }
    }
    for (std::size_t at = 0; at < items.size(); ++at)
    {
        if (may_ready(items, types, at))
        {
            add(readied(items[at]) ? "unready" : "ready");
            commands.back().item = static_cast<unsigned>(at);
            commands.back().label = item_name(items[at], item_words_);
        }
    }
    add("delay");
    add("quick");
    if (actor.half_moves >= 2)
    {
        for (const auto step : off_field_steps(actor))
        {
            add("flee", 0, step);
        }
    }
    add("end");
    return commands;
}

// The Buck Rogers commands; false for those the fight plays as every game's
// (move, delay, quick, flee, end).
bool Session::ground_submit(Fighter &actor, const rules::Command &command)
{
    auto &creature = actor.ground->creature();
    const auto &verb = command.verb;
    if (verb == "melee" || verb == "ranged")
    {
        auto &target = *find(command.target);
        note_attack(actor, target);
        make_attack(actor, target, false);
    }
    else if (verb == "launch")
    {
        ground_launch(actor, command.destination);
    }
    else if (verb == "intimidate")
    {
        ground_intimidate(actor);
        end_actions(actor);
    }
    else if (verb == "aid")
    {
        ground_aid(actor);
        end_actions(actor);
    }
    else if (verb == "sprint")
    {
        if (status_of(creature) != BuckRogersStatus::sprinting)
        {
            set_command_status(creature, BuckRogersStatus::sprinting);
            actor.half_moves = sprint_movement(actor.half_moves);
        }
        log({"{name} sprints.", {{"name", actor.name}}});
    }
    else if (verb == "dodge")
    {
        set_command_status(creature, BuckRogersStatus::dodging);
        end_actions(actor);
        ground_refresh(actor);
        log({"{name} dodges.", {{"name", actor.name}}});
    }
    else if (verb == "jetpack")
    {
        ground_jetpack(actor);
    }
    else if (verb == "land")
    {
        ground_land(actor, command.destination);
    }
    else if (verb == "ready" || verb == "unready")
    {
        // Readying an item (viewchar.pas ToggleReadyItem): a heavy one ends
        // the turn; the attacks left follow the weapon readied, and only fall
        // once it has attacked (cbtacts.pas RecalcAttacksLeft).
        auto &g = *actor.ground;
        auto &item = g.gear.items[command.item];
        item.flags ^= buck_rogers::Item::readied;
        if (verb == "ready" && readying_ends_turn(*type_of(item, g.gear.types)))
        {
            end_actions(actor);
        }
        const int first =
            ground_attacks(creature, g.gear.items, g.gear.types, static_cast<int>(round_) - 1,
                           g.has(ground_effect::dizzy))[0];
        auto &left = actor.attacks_left[0];
        left = g.attacked ? std::min(left, first) : first;
        ground_refresh(actor);
    }
    else if (verb == "guard")
    {
        end_actions(actor);
        if (may_guard_with(actor.ground->gear.items, actor.ground->gear.types))
        {
            actor.guarding = true;
            log({"{name} guards.", {{"name", actor.name}}});
        }
    }
    else
    {
        return false;
    }
    return true;
}

// A Buck Rogers fight's own state in its checkpoint: zero gravity, the
// sides' tactics, the clouds, and each creature's record and gear, effects
// and counts.
void Session::write_ground(Writer &out) const
{
    out(zero_gravity_, tactics_[0], tactics_[1], ground_clouds_.size());
    for (const auto &cloud : ground_clouds_)
    {
        out(cloud.center.x, cloud.center.y, cloud.rounds_left, static_cast<unsigned>(cloud.matter));
        for (const bool covers : cloud.covers)
        {
            out(covers);
        }
    }
    for (const auto &f : fighters_)
    {
        out(f.ground.has_value());
        if (!f.ground)
        {
            continue;
        }
        const auto &g = *f.ground;
        out(encode_ground_record(g.gear), g.effects.size());
        for (const auto &e : g.effects)
        {
            out(e.code, e.minutes, e.strength);
        }
        out(g.dying_count, g.tie_break, g.to_hit, g.armor_class_bonus, g.flying, g.computer_plays,
            g.attacked);
    }
}

void Session::read_ground(Reader &in)
{
    const auto invalid = []
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    };
    zero_gravity_ = in.as<bool>();
    tactics_ = {in.as<bool>(), in.as<bool>()};
    const auto clouds = in.as<std::size_t>();
    if (clouds > 64)
    {
        invalid();
    }
    for (std::size_t n = 0; n < clouds; ++n)
    {
        GroundCloud cloud;
        cloud.center = {in.as<int>(), in.as<int>()};
        cloud.rounds_left = in.as<int>();
        const auto matter = in.as<unsigned>();
        if (matter > static_cast<unsigned>(CloudMatter::mist))
        {
            invalid();
        }
        cloud.matter = static_cast<CloudMatter>(matter);
        for (auto &covers : cloud.covers)
        {
            covers = in.as<bool>();
        }
        ground_clouds_.push_back(cloud);
    }
    for (auto &f : fighters_)
    {
        if (!in.as<bool>())
        {
            continue;
        }
        auto gear = decode_ground_record(in.text());
        const auto effects = in.as<std::size_t>();
        if (!gear || effects > 64)
        {
            invalid();
        }
        GroundFighter g{std::move(*gear)};
        for (std::size_t n = 0; n < effects; ++n)
        {
            const int code = in.as<int>(), minutes = in.as<int>();
            g.effects.push_back({code, minutes, in.as<int>()});
        }
        g.dying_count = in.as<int>();
        g.tie_break = in.as<int>();
        g.to_hit = in.as<int>();
        g.armor_class_bonus = in.as<int>();
        g.flying = in.as<bool>();
        g.computer_plays = in.as<bool>();
        g.attacked = in.as<bool>();
        f.ground = std::move(g);
    }
}

} // namespace opengold::original::detail
