#include "session.h"
#include "scope_guards.h"
#include <limits>
#include <stdexcept>

// Session: areas and zones a spell leaves on the field, summons, pushes and
// the creatures they strike.
namespace opengold::srd5::detail
{
using namespace rules;

void Session::resolve_smite(Actor &a, std::string_view verb)
{
    const auto smite = detail::without_cast_suffix(verb);
    if (smite == "ensnaring_strike")
    {
        resolve_ensnaring_strike(a, verb);
        return;
    }
    const bool searing = smite == "searing_smite", shining = smite == "shining_smite";
    if (sanctuary_stops(a, actor(a.smite_target)))
    {
        a.smite_target = 0;
        a.bonus = false;
        return;
    }
    const auto &spell = *detail::find_spell(searing   ? "searing_smite"
                                            : shining ? "shining_smite"
                                                      : "divine_smite");
    auto &target = actor(a.smite_target);
    a.smite_target = 0;
    a.bonus = false;
    if (verb == "divine_smite_free")
    {
        --a.free_casts[CharacterClass::paladin];
    }
    else
    {
        pay_for_spell(a, verb, spell);
    }
    auto rolled = spell.dice;
    // Each slot level above the spell's adds a die.
    rolled.count += static_cast<int>(upcast_steps(verb, spell) * spell.upcast.extra_dice);
    // Divine Smite deals 1d8 more to a Fiend or an Undead.
    const auto &type = def(target).creature_type;
    if (!searing && !shining && (type == "fiend" || type == "undead"))
    {
        ++rolled.count;
    }
    // The damage is part of the attack, so a critical hit doubles its dice.
    if (a.smite_critical)
    {
        rolled.count *= 2;
    }
    const auto damage_type = std::string(detail::damage_name(spell.damage));
    const int amount = resolved_damage(target, spell.damage, dice(rolled));
    log(a.source.name + " casts " + std::string(spell.label) + ": " + target.source.name +
            " takes " + std::to_string(amount) + " " + damage_type + " damage.",
        {"{name} casts {spell}: {target} takes {damage} {type} damage.",
         {{"name", a.source.name},
          {"spell", std::string(spell.label), true},
          {"target", target.source.name},
          {"damage", std::to_string(amount)},
          {"type", damage_type, true}}});
    damage(target, amount, false);
    if (searing && target.life.hp > 0 && detail::can_apply(target.effects))
    {
        detail::apply_searing_smite(target.effects, effect_source(a), a.source.name, spell_dc(a));
        target.effects.active.back().slot = slot_level_of(verb, spell);
    }
    // Smite of Protection (Devotion 15): the aura shields until the
    // Paladin's next turn.
    if (!searing && !shining && def(a).smite_of_protection && detail::can_apply(a.effects))
    {
        detail::apply_poisoned(a.effects, effect_source(a), a.source.name, next_turn_ms(a),
                               detail::EffectKind::smite_protected);
        log(a.source.name + "'s aura shields its allies (Smite of Protection).",
            {"{name}'s aura shields its allies (Smite of Protection).", {{"name", a.source.name}}});
    }
    if (shining && target.life.hp > 0 && detail::can_apply(target.effects))
    {
        begin_concentration(a, spell);
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::shining, 0);
        log(target.source.name + " shines.", {"{name} shines.", {{"name", target.source.name}}});
    }
}

std::vector<Cell> Session::area_cells(const detail::SpellDef &spell, std::string_view verb,
                                      Cell origin, Cell center) const
{
    // An area reaches only the squares with a clear line from its point of
    // origin: the caster for a cone, line or cube, the aimed square otherwise.
    const Cell point = spell.cone || spell.line || spell.cube ? origin : center;
    auto cells = area_shape(spell, verb, origin, center);
    std::erase_if(cells,
                  [&](Cell cell)
                  {
                      return !line_of_sight(point, cell);
                  });
    return cells;
}

std::vector<Cell> Session::area_shape(const detail::SpellDef &spell, std::string_view verb,
                                      Cell origin, Cell center) const
{
    std::vector<Cell> cells;
    // A straight wall through the aimed square, along a row, a column or a
    // diagonal (LEVELS-3).
    if (spell.wall && detail::wall_direction(verb) == 4)
    {
        // A ring: the squares at its radius from the aimed one.
        for (int y = 0; y < board_.height; ++y)
        {
            for (int x = 0; x < board_.width; ++x)
            {
                if (distance(center, Cell{x, y}) == spell.ring / 2)
                {
                    cells.push_back({x, y});
                }
            }
        }
        return cells;
    }
    if (spell.wall)
    {
        static constexpr std::array<Cell, 4> steps{Cell{1, 0}, Cell{0, 1}, Cell{1, -1}, Cell{1, 1}};
        const auto step = steps[detail::wall_direction(verb).value_or(0)];
        const int length = spell.wall / 5;
        for (int n = -length / 2; n < length - length / 2; ++n)
        {
            if (const Cell cell{center.x + n * step.x, center.y + n * step.y};
                board_.contains(cell))
            {
                cells.push_back(cell);
            }
        }
        // Tsunami's wall is 10 feet thick (LEVELS-5): a second row beside the
        // first, away from the caster, square by square along a diagonal.
        if (spell.rider == detail::Rider::tsunami)
        {
            const auto heading = wave_headings[wave_heading(verb, origin, center)];
            const Cell beside{heading.x, heading.x ? 0 : heading.y};
            for (int n = -length / 2; n < length - length / 2; ++n)
            {
                if (const Cell cell{center.x + n * step.x + beside.x,
                                    center.y + n * step.y + beside.y};
                    board_.contains(cell))
                {
                    cells.push_back(cell);
                }
            }
        }
        return cells;
    }
    // The aimed direction from the caster; aiming at the caster faces east.
    const int dx = center.x - origin.x, dy = center.y - origin.y;
    const double ax = dx || dy ? dx : 1, ay = dy;
    // A cone: squares within its length and about 30 degrees of the aim.
    if (spell.cone)
    {
        for (int y = 0; y < board_.height; ++y)
        {
            for (int x = 0; x < board_.width; ++x)
            {
                const double vx = x - origin.x, vy = y - origin.y;
                if ((!vx && !vy) || distance(origin, Cell{x, y}) > spell.cone)
                {
                    continue;
                }
                const double cosine = (vx * ax + vy * ay) /
                                      (std::sqrt(vx * vx + vy * vy) * std::sqrt(ax * ax + ay * ay));
                if (cosine >= 0.866)
                {
                    cells.push_back({x, y});
                }
            }
        }
        return cells;
    }
    // A line two squares wide, or one, from the caster toward the aim.
    if (spell.line)
    {
        const double length = std::sqrt(ax * ax + ay * ay);
        const double right_edge = spell.narrow_line ? 0.5 : 1.0;
        for (int y = 0; y < board_.height; ++y)
        {
            for (int x = 0; x < board_.width; ++x)
            {
                const double vx = x - origin.x, vy = y - origin.y;
                const double along = (vx * ax + vy * ay) / length;
                const double across = (vx * ay - vy * ax) / length;
                if (along > 0 && along * 5 <= spell.line && across > -0.5 && across <= right_edge)
                {
                    cells.push_back({x, y});
                }
            }
        }
        return cells;
    }
    // A cube with a face against the caster, on the aimed side.
    if (spell.cube)
    {
        const int side = spell.cube / 5;
        const auto step = [&](double along, double across)
        {
            return std::abs(along) * 2 < std::abs(across) ? 0 : along > 0 ? 1 : along < 0 ? -1 : 0;
        };
        const int sx = step(ax, ay), sy = step(ay, ax);
        const auto first = [&](int start, int sign)
        {
            return sign > 0 ? start + 1 : sign < 0 ? start - side : start - (side - 1) / 2;
        };
        for (int y = first(origin.y, sy); y < first(origin.y, sy) + side; ++y)
        {
            for (int x = first(origin.x, sx); x < first(origin.x, sx) + side; ++x)
            {
                if (board_.contains(Cell{x, y}))
                {
                    cells.push_back({x, y});
                }
            }
        }
        return cells;
    }
    // A sphere: every square within its radius of the aimed one.
    if (spell.radius)
    {
        const int radius =
            spell.radius + static_cast<int>(upcast_steps(verb, spell)) * spell.upcast.extra_radius;
        for (int y = 0; y < board_.height; ++y)
        {
            for (int x = 0; x < board_.width; ++x)
            {
                if (distance(center, Cell{x, y}) <= radius)
                {
                    cells.push_back({x, y});
                }
            }
        }
        return cells;
    }
    // A square of `area` feet around the aimed cell, clipped to the board;
    // Forcecage's box is 10 feet.
    const bool box = spell.rider == detail::Rider::forcecage &&
                     detail::without_cast_suffix(verb).ends_with("_box");
    const int side = (box ? 10 : spell.area) / 5, first = (side - 1) / 2;
    for (int y = center.y - first; y < center.y - first + side; ++y)
    {
        for (int x = center.x - first; x < center.x - first + side; ++x)
        {
            if (board_.contains(Cell{x, y}))
            {
                cells.push_back({x, y});
            }
        }
    }
    return cells;
}

bool Session::teleport_open(const Actor &caster, Cell cell, Footprint size) const
{
    // Misty Step: an unoccupied space the caster can see.
    return cell != caster.source.cell && line_of_sight(caster, cell) && !obscured(caster, cell) &&
           room_for(&caster, size, cell);
}

bool Session::blocked_for(const Actor &mover, Cell anchor, const Battlefield &board) const
{
    for (const auto square : squares_of(anchor, footprint(mover)))
    {
        if (board.at(square) == Terrain::obstacle ||
            std::any_of(actors_.begin(), actors_.end(),
                        [&](const auto &other)
                        {
                            return other.source.id != mover.source.id && !other.life.dead &&
                                   !absent(other) && occupies(other, square);
                        }))
        {
            return true;
        }
    }
    return false;
}

bool Session::room_for(const Actor *mover, Footprint size, Cell anchor) const
{
    for (const auto square : squares_of(anchor, size))
    {
        if (board_.at(square) == Terrain::obstacle || guarded_square(square) ||
            std::any_of(actors_.begin(), actors_.end(),
                        [&](const auto &other)
                        {
                            return &other != mover && !other.life.dead && !absent(other) &&
                                   occupies(other, square);
                        }))
        {
            return false;
        }
    }
    return true;
}

Cell Session::default_area_center(const Actor &caster, const detail::SpellDef &spell) const
{
    // Compulsion's aim starts on the nearest foe, so the charmed go away from
    // the Bard unless it is moved.
    if (spell.id == "compulsion_heading")
    {
        const Actor *nearest = nullptr; // Borrowed from actors_.
        for (const auto &other : actors_)
        {
            if (other.source.side != caster.source.side && other.life.hp > 0 && !other.life.dead &&
                (!nearest || distance(caster, other) < distance(caster, *nearest)))
            {
                nearest = &other;
            }
        }
        return nearest ? nearest->source.cell : caster.source.cell;
    }
    // A teleport's preview starts on the caster; it must be moved before casting.
    if (spell.rider == detail::Rider::misty_step || spell.rider == detail::Rider::dimension_door)
    {
        return caster.source.cell;
    }
    // Flaming Sphere starts in the open square beside the nearest enemy.
    if (spell.rider == detail::Rider::flaming_sphere)
    {
        const Actor *nearest = nullptr; // Borrowed from actors_.
        for (const auto &other : actors_)
        {
            if (other.source.side != caster.source.side && other.life.hp > 0 && !other.life.dead &&
                (!nearest || distance(caster, other) < distance(caster, *nearest)))
            {
                nearest = &other;
            }
        }
        return nearest ? spectral_cell(*nearest, caster.source.cell) : caster.source.cell;
    }
    // The preview starts on the nearest living enemy in range and in clear
    // line, else the caster.
    Cell best = caster.source.cell;
    int best_feet = spell.range + 1;
    for (const auto &other : actors_)
    {
        const int feet = distance(caster, other);
        if (other.source.side != caster.source.side && other.life.hp > 0 && feet < best_feet &&
            line_of_sight(caster, other))
        {
            best = other.source.cell;
            best_feet = feet;
        }
    }
    return best;
}

void Session::aim_area(const Command &command)
{
    if (command.verb == "spell_cancel")
    {
        give_up_spell(area_->verb);
        area_.reset();
    }
    else if (command.verb == "area_cast")
    {
        cast_area();
    }
    else if (detail::find_spell(area_->verb)->storm)
    {
        place_storm_cube(command.destination);
    }
    else
    {
        area_->center = command.destination;
    }
}

void Session::potent_cantrip(Actor &target, const detail::SpellDef &spell,
                             detail::DamageDice rolled)
{
    // A missed cantrip still deals half its damage, without its other effects.
    const int amount = resolved_damage(target, spell.damage, dice(rolled) / 2);
    log("Potent Cantrip: " + target.source.name + " takes " + std::to_string(amount) + " damage.",
        {"Potent Cantrip: {name} takes {damage} damage.",
         {{"name", target.source.name}, {"damage", std::to_string(amount)}}});
    damage(target, amount, false);
}

std::vector<EntityId> Session::sculpted(const Actor &caster, const detail::SpellDef &spell,
                                        unsigned slot_level, const std::vector<Cell> &cells) const
{
    // Sculpt Spells: up to 1 + the spell's level allies the Evoker can see in
    // an Evocation area are spared, chosen automatically.
    std::vector<EntityId> spared;
    if (!def(caster).sculptor || !spell.evocation)
    {
        return spared;
    }
    for (const auto &other : actors_)
    {
        if (spared.size() < 1 + slot_level && other.source.side == caster.source.side &&
            other.source.id != caster.source.id && !other.life.dead && can_see(caster, other) &&
            touches(cells, other))
        {
            spared.push_back(other.source.id);
        }
    }
    return spared;
}

int Session::area_dc(const Actor &caster, const detail::SpellDef &spell) const
{
    // The Draconic Spirit's breath: its summoner's spell save DC.
    if (spell.id == "spirit_breath")
    {
        for (const auto &e : caster.effects.active)
        {
            if (e.kind == detail::EffectKind::summoned)
            {
                return 8 + e.dc;
            }
        }
    }
    // Breath Weapon: 8 + the Constitution modifier + the Proficiency Bonus.
    if (spell.id.starts_with("breath_weapon_"))
    {
        return 8 + def(caster).constitution + 2 + (def(caster).level - 1) / 4;
    }
    // A breath given by Dragon's Breath uses the spell caster's save DC.
    if (spell.id == "dragons_breath_exhale")
    {
        for (const auto &effect : caster.effects.active)
        {
            if (effect.kind == detail::EffectKind::dragons_breath)
            {
                return spell_dc(actor(effect.source_actor));
            }
        }
    }
    return spell_dc(caster, spell.id);
}

void Session::damage_area(Actor &caster, const detail::SpellDef &spell, std::string_view verb,
                          const std::vector<Cell> &cells, int dc)
{
    // Each creature in the area, the caster aside, saves for half.
    const auto spared = sculpted(caster, spell, slot_level_of(verb, spell), cells);
    for (const auto id : spared)
    {
        log(actor(id).source.name + " is spared by Sculpt Spells.",
            {"{name} is spared by Sculpt Spells.", {{"name", actor(id).source.name}}});
    }
    auto rolled = spell.dice;
    rolled.count += static_cast<int>(upcast_steps(verb, spell) * spell.upcast.extra_dice);
    // Dragon's Breath: 1d6 more for each slot level above 2 it was cast with.
    for (const auto &e : caster.effects.active)
    {
        if (spell.id == "dragons_breath_exhale" && e.kind == detail::EffectKind::dragons_breath &&
            e.slot > 2)
        {
            rolled.count += static_cast<int>(e.slot - 2);
        }
    }
    // A cantrip's dice, and a Breath Weapon's, grow with the caster's level;
    // a monster's breath has its own, and Dragon's Breath grows with its slot.
    if (!spell.level && spell.id != def(caster).monster_breath &&
        spell.id != "dragons_breath_exhale")
    {
        rolled.count *= cantrip_tier(def(caster));
    }
    if (def(caster).agonizing.contains(spell.id))
    {
        rolled.bonus += casting_modifier(def(caster));
    }
    // Empowered Evocation (Wizard 10).
    if (def(caster).empowered && spell.evocation &&
        detail::on_class_list(CharacterClass::wizard, spell.id))
    {
        rolled.bonus += casting_modifier(def(caster));
    }
    // Elemental Affinity (Sorcerer 6); Prismatic Spray's rays each have their own type.
    if (def(caster).elemental_affinity && spell.rider != detail::Rider::prismatic_spray &&
        !spell.id.starts_with("breath_weapon_") && spell.id != "spirit_breath" &&
        cast_damage_type(detail::dragon_type(verb).value_or(spell.damage)) ==
            *def(caster).elemental_affinity)
    {
        rolled.bonus += casting_modifier(def(caster));
    }
    const int total = spell_dice(caster, rolled);
    // Flame Strike's Radiant beside its Fire, resisted on its own.
    auto second = spell.second_dice;
    // Ice Storm's Cold does not grow with the slot; Flame Strike's Radiant does.
    // A spell with no second damage has no second dice to grow.
    if (second.count && spell.id != "ice_storm")
    {
        second.count += static_cast<int>(upcast_steps(verb, spell) * spell.upcast.extra_dice);
    }
    const int second_total = second.count ? spell_dice(caster, second) : 0;
    const auto second_type = std::string(detail::damage_name(spell.second_damage));
    auto damage_type = spell.id.starts_with("breath_weapon_")
                           ? *def(caster).breath
                           : cast_damage_type(detail::dragon_type(verb).value_or(spell.damage));
    // The Draconic Spirit breathes the type chosen when it was summoned.
    for (const auto &e : caster.effects.active)
    {
        if (spell.id == "spirit_breath" && e.kind == detail::EffectKind::spirit_breath)
        {
            damage_type = static_cast<detail::DamageType>(e.dc);
        }
    }
    const auto type = std::string(detail::damage_name(damage_type));
    const bool was_against_fire =
        std::exchange(against_fire_, damage_type == detail::DamageType::fire);
    const detail::ResetAtExit<bool> against_fire_reset{against_fire_, was_against_fire};
    // Careful Spell: the chosen allies succeed and so take no half damage.
    const auto careful = careful_allies(caster, cells);
    for (auto &other : actors_)
    {
        if (other.life.dead || other.source.id == caster.source.id || absent(other) ||
            !touches(cells, other) ||
            std::find(spared.begin(), spared.end(), other.source.id) != spared.end() ||
            std::find(careful.begin(), careful.end(), other.source.id) != careful.end() ||
            sealed_off(other) ||
            globe_wards(caster, other.source.cell, slot_level_of(verb, spell)) ||
            caged_apart(caster.source.cell, other.source.cell, false))
        {
            continue;
        }
        const bool saved = saving_throw_succeeds(other, spell.save, dc);
        // Prismatic Spray: a ray for each creature, or two.
        if (spell.rider == detail::Rider::prismatic_spray)
        {
            prismatic_rays(caster, other, saved, dc, rolled);
            continue;
        }
        // Potent Cantrip: an Evoker's saved-against cantrip still deals half.
        const bool halved = spell.half_on_success || (def(caster).evoker && !spell.level);
        if (saved && !halved)
        {
            continue;
        }
        const int amount =
            resolved_damage(other, damage_type, evaded(other, spell.save, halved, saved, total));
        log(other.source.name + " takes " + std::to_string(amount) + " " + type + " damage.",
            {"{name} takes {damage} {type} damage.",
             {{"name", other.source.name},
              {"damage", std::to_string(amount)},
              {"type", type, true}}});
        damage(other, amount);
        if (second_total && !other.life.dead)
        {
            const int more = resolved_damage(
                other, spell.second_damage, evaded(other, spell.save, halved, saved, second_total));
            log(other.source.name + " takes " + std::to_string(more) + " " + second_type +
                    " damage.",
                {"{name} takes {damage} {type} damage.",
                 {{"name", other.source.name},
                  {"damage", std::to_string(more)},
                  {"type", second_type, true}}});
            damage(other, more);
        }
        if (amount)
        {
            curse_strikes(caster, other);
        }
        if (!saved && spell.rider == detail::Rider::thunderwave && !other.life.dead)
        {
            push_away(caster, other, 2);
        }
        // Sunbeam: Blinded until the start of the caster's next turn.
        if (!saved && spell.rider == detail::Rider::sunbeam && !other.life.dead &&
            detail::can_apply(other.effects))
        {
            detail::apply_poisoned(other.effects, effect_source(caster), caster.source.name,
                                   next_turn_ms(caster), detail::EffectKind::dazzled);
            log(other.source.name + " is Blinded.",
                {"{name} is Blinded.", {{"name", other.source.name}}});
        }
        // Vitriolic Sphere: 5d4 more Acid at the end of its next turn.
        if (!saved && spell.rider == detail::Rider::vitriolic_sphere && !other.life.dead &&
            detail::can_apply(other.effects))
        {
            const auto index = index_of(other.source.id);
            const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
            detail::apply_poisoned(other.effects, effect_source(caster), caster.source.name,
                                   next_turn_ms(other) + slot, detail::EffectKind::vitriol);
        }
        // Sunburst: Blinded for a minute, saving again at the end of each turn.
        if (!saved && spell.rider == detail::Rider::sunburst && !other.life.dead &&
            detail::can_apply(other.effects))
        {
            detail::apply_blindness(
                other.effects, effect_source(caster), caster.source.name,
                detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(other.source.id)});
            log(other.source.name + " is Blinded.",
                {"{name} is Blinded.", {{"name", other.source.name}}});
        }
    }
}

void Session::condition_area(Actor &caster, const detail::SpellDef &spell,
                             const std::vector<Cell> &cells)
{
    // Sleep chooses the enemies in its sphere; Color Spray's cone takes every
    // creature in it.
    const bool sleep = spell.rider == detail::Rider::sleep;
    const int dc = spell_dc(caster, spell.id);
    const auto index = index_of(caster.source.id);
    const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
    const auto careful = careful_allies(caster, cells);
    for (auto &other : actors_)
    {
        if (other.life.dead || other.life.hp == 0 || other.source.id == caster.source.id ||
            absent(other) || !touches(cells, other) ||
            std::find(careful.begin(), careful.end(), other.source.id) != careful.end() ||
            (sleep && other.source.side == caster.source.side) ||
            !detail::can_apply(other.effects) || sealed_off(other) ||
            globe_wards(caster, other.source.cell, spell.level) ||
            caged_apart(caster.source.cell, other.source.cell, false))
        {
            continue;
        }
        if (sleep && (def(other).sleepless || def(other).creature_type == "undead" ||
                      def(other).creature_type == "construct"))
        {
            log(other.source.name + " does not sleep.",
                {"{name} does not sleep.", {{"name", other.source.name}}});
            continue;
        }
        if (saving_throw_succeeds(other, spell.save, dc))
        {
            continue;
        }
        if (spell.rider == detail::Rider::faerie_fire)
        {
            detail::apply_spell_benefit(other.effects, effect_source(caster), caster.source.name,
                                        detail::EffectKind::outlined, 0);
            log(other.source.name + " is outlined.",
                {"{name} is outlined.", {{"name", other.source.name}}});
        }
        else if (sleep)
        {
            detail::apply_repeating_condition(
                other.effects, detail::EffectKind::drowsy, effect_source(caster),
                caster.source.name,
                detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(other.source.id)});
            log(other.source.name + " grows drowsy.",
                {"{name} grows drowsy.", {{"name", other.source.name}}});
            end_incapacitated_effects(other);
        }
        else
        {
            detail::apply_poisoned(other.effects, effect_source(caster), caster.source.name,
                                   next_turn_ms(caster) + slot, detail::EffectKind::dazzled);
            log(other.source.name + " is Blinded.",
                {"{name} is Blinded.", {{"name", other.source.name}}});
        }
    }
}

void Session::blow(const Zone &line, Actor &creature)
{
    // Gust of Wind: a Strength save or 15 feet away from the caster.
    if (creature.life.dead || creature.life.hp == 0 || creature.source.id == line.caster)
    {
        return;
    }
    const auto &caster = actor(line.caster);
    if (!saving_throw_succeeds(creature, detail::Ability::strength, spell_dc(caster), false,
                               heightened_against(creature, caster, "gust_of_wind")))
    {
        push_away(caster, creature, 3);
    }
}

void Session::burn_beside_spheres(Actor &creature)
{
    // Copies: the burn can end a caster's Concentration and with it the sphere.
    const auto zones = zones_;
    for (const auto &zone : zones)
    {
        if (zone.kind == ZoneKind::flaming_sphere && distance(zone.cells.front(), creature) <= 5)
        {
            sphere_burns(zone, creature);
        }
    }
}

void Session::sphere_burns(const Zone &sphere, Actor &creature)
{
    if (creature.life.dead || creature.life.hp == 0)
    {
        return;
    }
    const auto &caster = actor(sphere.caster);
    const auto &spell = *detail::find_spell("flaming_sphere");
    const bool saved = saving_throw_succeeds(creature, spell.save, spell_dc(caster), false,
                                             heightened_against(creature, caster, spell.id));
    // 1d6 more for each slot level above 2.
    const int rolled =
        dice({spell.dice.count + static_cast<int>(sphere.slot - spell.level), spell.dice.sides, 0});
    const int amount = resolved_damage(creature, spell.damage, saved ? rolled / 2 : rolled);
    log(creature.source.name + " takes " + std::to_string(amount) +
            " Fire damage from the Flaming Sphere.",
        {"{name} takes {damage} Fire damage from the Flaming Sphere.",
         {{"name", creature.source.name}, {"damage", std::to_string(amount)}}});
    damage(creature, amount);
}

void Session::refresh_form(Actor &a)
{
    const auto shape = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                    [](const auto &e)
                                    {
                                        return e.kind == detail::EffectKind::wild_shape;
                                    });
    // Polymorph's or Animal Shapes' form, or else Wild Shape's.
    const auto polymorph = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                        [](const auto &e)
                                        {
                                            return e.kind == detail::EffectKind::polymorphed ||
                                                   e.kind == detail::EffectKind::animal_shaped;
                                        });
    // A summoned creature attacks with its summoner's spell attack bonus; a
    // Draconic Spirit's Rend reaches 10 feet.
    const auto summoned = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                       [](const auto &e)
                                       {
                                           return e.kind == detail::EffectKind::summoned;
                                       });
    if (summoned != a.effects.active.end() && polymorph == a.effects.active.end())
    {
        auto d = a.definition;
        d.melee_bonus = summoned->dc;
        // A Giant Insect's Poison Jab reaches 10 feet; the spider's Web Bolt
        // uses the caster's spell attack too.
        if (a.source.definition.starts_with("draconic-spirit") ||
            a.source.definition.starts_with("giant-insect"))
        {
            d.reach = 10;
        }
        if (d.range > 0)
        {
            d.ranged_bonus = summoned->dc;
        }
        a.form = std::move(d);
    }
    else if (polymorph != a.effects.active.end())
    {
        a.form = shaped(a.definition,
                        detail::beast_forms.at(static_cast<std::size_t>(polymorph->dc - 1)));
    }
    else if (shape == a.effects.active.end())
    {
        a.form.reset();
    }
    else
    {
        a.form =
            shaped(a.definition, detail::beast_forms.at(static_cast<std::size_t>(shape->dc - 1)));
    }
    if (detail::has_effect(a.effects, detail::EffectKind::giant_strength))
    {
        a.form = strengthened(a.form ? *a.form : a.definition, 5);
    }
}

// Gift of the Protectors: the whole party is written in the Warlock's book
// (LEVELS-5), so the first member dropped to 0 Hit Points but not killed
// outright drops to 1 instead, once per the Warlock's Long Rest.
void Session::protect(Actor &target)
{
    const auto warlock = std::find_if(
        actors_.begin(), actors_.end(),
        [&](const auto &a)
        {
            return def(a).gift_of_protectors && a.source.side == target.source.side &&
                   !detail::has_effect(a.effects, detail::EffectKind::protectors_used) &&
                   detail::can_apply(a.effects);
        });
    if (warlock == actors_.end())
    {
        return;
    }
    // Not healing (Chill Touch cannot stop it); the dying clock is cleared.
    (void)detail::heal_life(target.life, 1, max_hp(target), true);
    detail::apply_spell_benefit(warlock->effects, effect_source(*warlock), warlock->source.name,
                                detail::EffectKind::protectors_used, 0);
    log(target.source.name + " drops to 1 Hit Point instead (Gift of the Protectors).",
        {"{name} drops to 1 Hit Point instead (Gift of the Protectors).",
         {{"name", target.source.name}}});
}

void Session::end_animal_shape(Actor &a)
{
    if (!std::erase_if(a.effects.active,
                       [](const auto &e)
                       {
                           return e.kind == detail::EffectKind::animal_shaped;
                       }))
    {
        return;
    }
    if (a.life.temporary_hp.source_id == "spell:animal_shapes")
    {
        a.life.temporary_hp = {};
    }
    const int form_speed = def(a).speed;
    refresh_form(a);
    change_speed(a, form_speed);
    log(a.source.name + " returns to its own shape.",
        {"{name} returns to its own shape.", {{"name", a.source.name}}});
}

void Session::end_wild_shape(Actor &a)
{
    if (!a.form)
    {
        return;
    }
    std::erase_if(a.effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::wild_shape;
                  });
    const int form_speed = def(a).speed;
    a.form.reset();
    change_speed(a, form_speed);
    log(a.source.name + " leaves Wild Shape.",
        {"{name} leaves Wild Shape.", {{"name", a.source.name}}});
}

// A Speed change mid-turn changes the movement left by the difference; movement
// already spent stays spent (SRD 5.2.1 p. 14).
void Session::change_speed(Actor &a, int old_speed)
{
    a.movement = std::max(0, a.movement + def(a).speed - old_speed);
}

void Session::lands_aid(Actor &druid, Cell center)
{
    druid.nick_origin = 0;
    (void)druid.actions.spend(true);
    --druid.channel_divinity[CharacterClass::druid];
    log(druid.source.name + " uses Land's Aid.",
        {"{name} uses Land's Aid.", {{"name", druid.source.name}}});
    // 2d6, 3d6 from Druid 10, 4d6 from 14.
    auto aid = *detail::find_spell("lands_aid");
    const int druid_level = def(druid).class_levels[CharacterClass::druid];
    aid.dice.count = druid_level >= 14 ? 4 : druid_level >= 10 ? 3 : 2;
    const auto cells = area_cells(aid, aid.id, origin_toward(druid, center), center);
    const auto inside = [&](const Actor &other)
    {
        return !other.life.dead && touches(cells, other);
    };
    // The Druid chooses its enemies for the thorns and its most wounded ally
    // for the flowers.
    const int dc = spell_dc(druid), rolled = dice(aid.dice);
    for (auto &other : actors_)
    {
        if (!inside(other) || other.source.side == druid.source.side || other.life.hp == 0)
        {
            continue;
        }
        const bool saved = saving_throw_succeeds(other, aid.save, dc);
        const int amount = resolved_damage(other, aid.damage, saved ? rolled / 2 : rolled);
        log(other.source.name + " takes " + std::to_string(amount) +
                " Necrotic damage from the thorns.",
            {"{name} takes {damage} Necrotic damage from the thorns.",
             {{"name", other.source.name}, {"damage", std::to_string(amount)}}});
        damage(other, amount);
    }
    Actor *wounded = nullptr; // Borrowed from actors_.
    for (auto &other : actors_)
    {
        if (inside(other) && other.source.side == druid.source.side &&
            other.life.hp < max_hp(other) &&
            (!wounded || max_hp(other) - other.life.hp > max_hp(*wounded) - wounded->life.hp))
        {
            wounded = &other;
        }
    }
    if (wounded)
    {
        (void)heal(*wounded, dice(aid.dice));
    }
}

bool Session::wears_metal(const Actor &creature) const
{
    if (creature.source.character_profile.empty())
    {
        return def(creature).metal_armor;
    }
    const auto &keys = def(creature).equipment_keys;
    return std::any_of(keys.begin(), keys.end(),
                       [](const auto &key)
                       {
                           const auto *worn = detail::armor(key);
                           return worn && detail::metal_armor(*worn);
                       });
}

void Session::heat_metal(const Actor &caster, Actor &creature, unsigned slot)
{
    const auto &spell = *detail::find_spell("heat_metal");
    // 1d8 more for each slot level above 2.
    const int amount = resolved_damage(
        creature, spell.damage,
        dice({spell.dice.count + static_cast<int>(slot - spell.level), spell.dice.sides, 0}));
    log(creature.source.name + " takes " + std::to_string(amount) +
            " Fire damage from the hot metal.",
        {"{name} takes {damage} Fire damage from the hot metal.",
         {{"name", creature.source.name}, {"damage", std::to_string(amount)}}});
    damage(creature, amount);
    if (creature.life.dead || creature.life.hp == 0 || !detail::can_apply(creature.effects) ||
        saving_throw_succeeds(creature, spell.save, spell_dc(caster), false,
                              heightened_against(creature, caster, spell.id)))
    {
        return;
    }
    detail::apply_poisoned(creature.effects, effect_source(caster), caster.source.name,
                           next_turn_ms(caster), detail::EffectKind::scorched);
    log(creature.source.name + " has Disadvantage on attack rolls.",
        {"{name} has Disadvantage on attack rolls.", {{"name", creature.source.name}}});
}

void Session::give_up_spell(const std::string &verb)
{
    if (given_up_turn_ != turn_ || given_up_round_ != round_)
    {
        given_up_spells_.clear();
        given_up_turn_ = turn_;
        given_up_round_ = round_;
    }
    given_up_spells_.push_back(verb);
}

void Session::move_moonbeam(Actor &caster, const Actor &onto)
{
    auto &beam =
        *std::find_if(zones_.begin(), zones_.end(),
                      [&](const auto &zone)
                      {
                          return zone.kind == ZoneKind::moonbeam && zone.caster == caster.source.id;
                      });
    const auto before = beam;
    beam.cells = beam_cells(onto.source.cell);
    const auto after = beam;
    log(caster.source.name + " moves the Moonbeam.",
        {"{name} moves the Moonbeam.", {{"name", caster.source.name}}});
    for (auto &other : actors_)
    {
        if (in_zone(after, other) && !in_zone(before, other))
        {
            moonbeam_burns(after, other);
        }
    }
}

bool Session::near_pack(const Zone &pack, const Actor &a)
{
    const auto squares = squares_of(a);
    return std::any_of(squares.begin(), squares.end(),
                       [&](Cell square)
                       {
                           return near_pack(pack, square);
                       });
}

bool Session::in_emanation(const Zone &zone, const Actor &a) const
{
    const auto squares = squares_of(a);
    return std::any_of(squares.begin(), squares.end(),
                       [&](Cell square)
                       {
                           return in_emanation(zone, square);
                       });
}

bool Session::near_pack(const Zone &pack, Cell cell)
{
    return std::any_of(pack.cells.begin(), pack.cells.end(),
                       [&](Cell square)
                       {
                           return distance(square, cell) <= 10;
                       });
}

bool Session::in_emanation(const Zone &zone, Cell cell) const
{
    return distance(actor(zone.caster), cell) <=
           (zone.kind == ZoneKind::holy                                             ? 30
            : zone.kind == ZoneKind::guardians || zone.kind == ZoneKind::elementals ? 15
                                                                                    : 10);
}

bool Session::emanates(const Zone &zone)
{
    return zone.kind == ZoneKind::beings || zone.kind == ZoneKind::guardians;
}

void Session::spirits_strike(const Zone &zone, Actor &creature)
{
    // The caster forces a save on each enemy it can see, once per turn; the
    // spirits may have ended with their caster's Concentration mid-turn.
    const bool pack = zone.kind == ZoneKind::animals, guardians = zone.kind == ZoneKind::guardians;
    const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto &a)
                                     {
                                         return a.source.id == zone.caster;
                                     });
    const bool standing =
        std::any_of(zones_.begin(), zones_.end(),
                    [&](const auto &other)
                    {
                        return other.kind == zone.kind && other.caster == zone.caster;
                    });
    const auto tested = pack        ? detail::EffectKind::pack_tested
                        : guardians ? detail::EffectKind::guarded
                                    : detail::EffectKind::beings_tested;
    if (!standing || caster == actors_.end() || creature.source.side == caster->source.side ||
        creature.life.dead || creature.life.hp == 0 || absent(creature) ||
        detail::has_effect(creature.effects, tested) || !can_see(*caster, creature) ||
        sealed_off(creature))
    {
        return;
    }
    const unsigned rest_of_turn = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
    if (detail::can_apply(creature.effects))
    {
        detail::apply_poisoned(creature.effects, effect_source(*caster), caster->source.name,
                               rest_of_turn, tested);
    }
    const auto &spell = *detail::find_spell(pack        ? "conjure_animals"
                                            : guardians ? "spirit_guardians"
                                                        : "conjure_woodland_beings");
    const bool saved = saving_throw_succeeds(creature, spell.save, zone.save_dc, false,
                                             heightened_against(creature, *caster, spell.id));
    if (saved && !spell.half_on_success)
    {
        return;
    }
    const int count = spell.dice.count + static_cast<int>(zone.slot - spell.level);
    const int rolled = dice({count, spell.dice.sides, 0});
    // Spirit Guardians deal Necrotic damage for an evil caster.
    const bool fiendish = guardians && def(*caster).evil;
    const auto type = fiendish ? detail::DamageType::necrotic : spell.damage;
    const int amount = resolved_damage(creature, type, saved ? rolled / 2 : rolled);
    if (pack)
    {
        log(creature.source.name + " takes " + std::to_string(amount) +
                " Slashing damage from the spirit pack.",
            {"{name} takes {damage} Slashing damage from the spirit pack.",
             {{"name", creature.source.name}, {"damage", std::to_string(amount)}}});
    }
    else if (guardians)
    {
        const auto name = std::string(detail::damage_name(type));
        log(creature.source.name + " takes " + std::to_string(amount) + " " + name +
                " damage from the spirit guardians.",
            {"{name} takes {damage} {type} damage from the spirit guardians.",
             {{"name", creature.source.name},
              {"damage", std::to_string(amount)},
              {"type", name, true}}});
    }
    else
    {
        log(creature.source.name + " takes " + std::to_string(amount) +
                " Force damage from the woodland spirits.",
            {"{name} takes {damage} Force damage from the woodland spirits.",
             {{"name", creature.source.name}, {"damage", std::to_string(amount)}}});
    }
    damage(creature, amount);
}

std::optional<std::vector<Cell>> Session::pack_destination(const Zone &pack,
                                                           const Actor &onto) const
{
    // An unoccupied space two squares by two that the caster can see, up to 30
    // feet from where the pack is, as near the creature as it can get.
    const auto &caster = actor(pack.caster);
    std::optional<std::vector<Cell>> best;
    int best_feet = 0;
    for (int y = 0; y + 1 < board_.height; ++y)
    {
        for (int x = 0; x + 1 < board_.width; ++x)
        {
            const std::vector<Cell> cells{{x, y}, {x + 1, y}, {x, y + 1}, {x + 1, y + 1}};
            if (distance(cells.front(), pack.cells.front()) > 30 ||
                !line_of_sight(caster, cells.front()) ||
                std::any_of(cells.begin(), cells.end(),
                            [&](Cell cell)
                            {
                                return board_.at(cell) == Terrain::obstacle ||
                                       std::any_of(actors_.begin(), actors_.end(),
                                                   [&](const auto &other)
                                                   {
                                                       return !other.life.dead && !absent(other) &&
                                                              occupies(other, cell);
                                                   });
                            }))
            {
                continue;
            }
            int feet = std::numeric_limits<int>::max();
            for (const auto cell : cells)
            {
                feet = std::min(feet, distance(cell, onto));
            }
            if (feet <= 10 && (!best || feet < best_feet))
            {
                best = cells;
                best_feet = feet;
            }
        }
    }
    return best;
}

void Session::move_spirit_pack(Actor &caster, const Actor &onto)
{
    auto &pack =
        *std::find_if(zones_.begin(), zones_.end(),
                      [&](const auto &zone)
                      {
                          return zone.kind == ZoneKind::animals && zone.caster == caster.source.id;
                      });
    const auto before = pack;
    pack.cells = *pack_destination(pack, onto);
    const auto after = pack;
    detail::apply_spell_benefit(caster.effects, effect_source(caster), caster.source.name,
                                detail::EffectKind::pack_moved, 0);
    log(caster.source.name + " moves the spirit pack.",
        {"{name} moves the spirit pack.", {{"name", caster.source.name}}});
    for (auto &other : actors_)
    {
        if (near_pack(after, other) && !near_pack(before, other))
        {
            spirits_strike(after, other);
        }
    }
}

bool Session::across_wind_wall(Cell from, Cell to) const
{
    // The squares a straight shot passes, sampled four times a square.
    const int steps = 4 * std::max(std::abs(to.x - from.x), std::abs(to.y - from.y));
    for (int n = 1; n <= steps; ++n)
    {
        const double along = static_cast<double>(n) / steps;
        const Cell cell{static_cast<int>(std::lround(from.x + (to.x - from.x) * along)),
                        static_cast<int>(std::lround(from.y + (to.y - from.y) * along))};
        if (cell != from && std::any_of(zones_.begin(), zones_.end(),
                                        [&](const auto &zone)
                                        {
                                            return zone.kind == ZoneKind::wind_wall &&
                                                   in_zone(zone, cell);
                                        }))
        {
            return true;
        }
    }
    return false;
}

void Session::turn_against_party(const std::vector<EntityId> &turning)
{
    allies_attacked_ = true;
    for (auto &a : actors_)
    {
        if (a.source.side == Side::party && !a.life.dead && a.life.hp > 0 &&
            std::ranges::find(turning, a.source.id) != turning.end())
        {
            a.source.side = Side::opposition;
            log(a.source.name + " turns on the party.",
                {"{name} turns on the party.", {{"name", a.source.name}}});
        }
    }
    ++revision_;
}

void Session::dominate(Actor &caster, Actor &target, int dc)
{
    detail::apply_spell_benefit(target.effects, effect_source(caster), caster.source.name,
                                detail::EffectKind::dominated, dc);
    log(target.source.name + " is dominated by " + caster.source.name + ".",
        {"{name} is dominated by {caster}.",
         {{"name", target.source.name}, {"caster", caster.source.name}}});
    if (target.source.side == caster.source.side)
    {
        return;
    }
    target.source.side = caster.source.side;
    target.turned = true;
}

const Actor *Session::controller(const Actor &a) const
{
    for (const auto &e : a.effects.active)
    {
        if ((e.kind == detail::EffectKind::dominated || e.kind == detail::EffectKind::raised ||
             e.kind == detail::EffectKind::summoned) &&
            e.source_scope == scope_)
        {
            for (const auto &other : actors_)
            {
                if (other.source.id == e.source_actor)
                {
                    return &other;
                }
            }
        }
    }
    return nullptr;
}

void Session::release(Actor &a)
{
    if (!a.turned || detail::has_effect(a.effects, detail::EffectKind::dominated))
    {
        return;
    }
    a.turned = a.skips_turn = false;
    a.source.side = a.source.side == Side::party ? Side::opposition : Side::party;
    log(a.source.name + " is no longer dominated.",
        {"{name} is no longer dominated.", {{"name", a.source.name}}});
}

void Session::settle_dominated()
{
    for (auto &a : actors_)
    {
        release(a);
    }
    for (bool moved = true; moved;)
    {
        moved = false;
        for (std::size_t from = 0; from < actors_.size() && !moved; ++from)
        {
            const auto *boss = controller(actors_[from]);
            if (!boss)
            {
                continue;
            }
            // In place: after its controller, with only that controller's other
            // creatures between them.
            const auto boss_index = index_of(boss->source.id);
            bool placed = boss_index < from;
            for (std::size_t between = boss_index + 1; placed && between < from; ++between)
            {
                placed = controller(actors_[between]) == boss;
            }
            if (placed)
            {
                continue;
            }
            const auto boss_id = boss->source.id;
            auto creature = std::move(actors_[from]);
            // One that has already acted this round waits for the next.
            creature.skips_turn = from < turn_;
            actors_.erase(actors_.begin() + static_cast<std::ptrdiff_t>(from));
            if (from < turn_)
            {
                --turn_;
            }
            const auto after = index_of(boss_id) + 1;
            actors_.insert(actors_.begin() + static_cast<std::ptrdiff_t>(after),
                           std::move(creature));
            moved = true;
        }
    }
}

bool Session::animatable(const Actor &corpse, const Actor &caster) const
{
    return corpse.life.dead && corpse.source.side != caster.source.side && !absent(corpse) &&
           !detail::has_effect(corpse.effects, detail::EffectKind::dust) &&
           def(corpse).creature_type == "humanoid" &&
           (def(corpse).size == 1 || def(corpse).size == 2) && distance(caster, corpse) <= 10 &&
           std::none_of(zones_.begin(), zones_.end(),
                        [&](const auto &zone)
                        {
                            return zone.kind == ZoneKind::raising && in_zone(zone, corpse);
                        });
}

void Session::begin_animating(Actor &caster, const Actor &corpse, const detail::SpellDef &spell,
                              unsigned slot)
{
    // A minute of casting: Concentration, and the caster's Action each turn.
    const auto raising =
        std::find_if(zones_.begin(), zones_.end(),
                     [&](const auto &zone)
                     {
                         return zone.kind == ZoneKind::raising && zone.caster == caster.source.id;
                     });
    if (raising != zones_.end())
    {
        raising->cells.push_back(corpse.source.cell);
        return;
    }
    begin_concentration(caster, spell);
    detail::apply_spell_benefit(caster.effects, effect_source(caster), caster.source.name,
                                detail::EffectKind::animating, static_cast<int>(slot));
    zones_.push_back({caster.source.id, ZoneKind::raising, {corpse.source.cell}});
    log(caster.source.name + " begins casting " + std::string(spell.label) + ".",
        {"{name} begins casting {spell}.",
         {{"name", caster.source.name}, {"spell", std::string(spell.label), true}}});
}

void Session::finish_animating(Actor &caster, std::string_view undead)
{
    const auto raising =
        std::find_if(zones_.begin(), zones_.end(),
                     [&](const auto &zone)
                     {
                         return zone.kind == ZoneKind::raising && zone.caster == caster.source.id;
                     });
    if (raising == zones_.end())
    {
        return;
    }
    const auto cells = raising->cells;
    zones_.erase(raising);
    for (const auto cell : cells)
    {
        for (auto &corpse : actors_)
        {
            if (corpse.life.dead && occupies(corpse, cell) &&
                corpse.source.side != caster.source.side && !absent(corpse))
            {
                raise_undead(caster, corpse, undead);
                break;
            }
        }
    }
    caster.concentration.stop();
}

void Session::raise_undead(const Actor &caster, Actor &corpse, std::string_view undead)
{
    corpse.source.definition = std::string(undead);
    corpse.source.melee_weapon.clear();
    corpse.source.ranged_weapon.clear();
    corpse.definition = content_->definitions.at(corpse.source.definition);
    corpse.form.reset();
    corpse.effects.active.clear();
    corpse.effects.prone = false;
    corpse.life = {};
    corpse.life.hp = corpse.definition.hp;
    corpse.source.side = caster.source.side;
    // It acts from its next turn on, with a turn's whole allowance.
    corpse.actions = {};
    corpse.bonus = corpse.reaction = true;
    corpse.dodge = corpse.disengaged = corpse.moved = corpse.attacked = false;
    corpse.dashes = 0;
    corpse.movement = corpse.definition.speed;
    detail::apply_spell_benefit(corpse.effects, effect_source(caster), caster.source.name,
                                detail::EffectKind::raised, 0);
    if (undead == "ghoul")
    {
        log(corpse.source.name + " rises as a Ghoul.",
            {"{name} rises as a Ghoul.", {{"name", corpse.source.name}}});
    }
    else if (def(caster).specter_key == undead)
    {
        log(corpse.source.name + " rises as a Specter.",
            {"{name} rises as a Specter.", {{"name", corpse.source.name}}});
    }
    else
    {
        log(corpse.source.name + " rises as a Zombie.",
            {"{name} rises as a Zombie.", {{"name", corpse.source.name}}});
    }
}

const detail::Effect *Session::curse_on(const Actor &a, int curse) const
{
    for (const auto &e : a.effects.active)
    {
        if (e.kind == detail::EffectKind::cursed && e.source_scope == scope_ && e.dc % 10 == curse)
        {
            return &e;
        }
    }
    return nullptr;
}

void Session::curse_strikes(const Actor &caster, Actor &target)
{
    const auto *curse = curse_on(target, 8);
    if (!curse || curse->source_actor != caster.source.id || target.life.dead)
    {
        return;
    }
    const int extra = resolved_damage(target, detail::DamageType::necrotic, roll(8));
    log(target.source.name + " takes " + std::to_string(extra) + " Necrotic damage from the curse.",
        {"{name} takes {damage} Necrotic damage from the curse.",
         {{"name", target.source.name}, {"damage", std::to_string(extra)}}});
    damage(target, extra, false);
}

void Session::swarm_strikes(const Zone &swarm, Actor &creature)
{
    // Every creature in it but the caster saves, once per turn; the swarm may
    // have ended with its caster's Concentration mid-turn.
    const bool blades = swarm.kind == ZoneKind::blades, thorns = swarm.kind == ZoneKind::thorns;
    const auto tested = blades   ? detail::EffectKind::bladed
                        : thorns ? detail::EffectKind::thorned
                                 : detail::EffectKind::swarmed;
    const bool standing =
        std::any_of(zones_.begin(), zones_.end(),
                    [&](const auto &zone)
                    {
                        return zone.kind == swarm.kind && zone.caster == swarm.caster;
                    });
    if (!standing || creature.source.id == swarm.caster || creature.life.dead ||
        creature.life.hp == 0 || absent(creature) || detail::has_effect(creature.effects, tested) ||
        sealed_off(creature))
    {
        return;
    }
    const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto &a)
                                     {
                                         return a.source.id == swarm.caster;
                                     });
    const unsigned rest_of_turn = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
    if (detail::can_apply(creature.effects))
    {
        detail::apply_poisoned(creature.effects,
                               caster != actors_.end() ? effect_source(*caster)
                                                       : effect_source(creature),
                               caster != actors_.end() ? caster->source.name : creature.source.name,
                               rest_of_turn, tested);
    }
    const auto &spell = *detail::find_spell(blades   ? "blade_barrier"
                                            : thorns ? "wall_of_thorns"
                                                     : "insect_plague");
    const bool saved = saving_throw_succeeds(creature, spell.save, swarm.save_dc);
    const int count = spell.dice.count + static_cast<int>(swarm.slot - spell.level);
    const int rolled = dice({count, spell.dice.sides, 0});
    // The thorns slash a creature moving in them; they pierced as they rose.
    const auto type = thorns ? detail::DamageType::slashing : spell.damage;
    const int amount = resolved_damage(creature, type, saved ? rolled / 2 : rolled);
    if (thorns)
    {
        log(creature.source.name + " takes " + std::to_string(amount) +
                " Slashing damage from the thorns.",
            {"{name} takes {damage} Slashing damage from the thorns.",
             {{"name", creature.source.name}, {"damage", std::to_string(amount)}}});
    }
    else if (blades)
    {
        log(creature.source.name + " takes " + std::to_string(amount) +
                " Force damage from the blades.",
            {"{name} takes {damage} Force damage from the blades.",
             {{"name", creature.source.name}, {"damage", std::to_string(amount)}}});
    }
    else
    {
        log(creature.source.name + " takes " + std::to_string(amount) +
                " Piercing damage from the swarm.",
            {"{name} takes {damage} Piercing damage from the swarm.",
             {{"name", creature.source.name}, {"damage", std::to_string(amount)}}});
    }
    damage(creature, amount);
}

std::vector<Cell> Session::pillar_cells(Cell center) const
{
    // A 10-foot radius around the centre, which comes first.
    std::vector<Cell> cells{center};
    for (int y = 0; y < board_.height; ++y)
    {
        for (int x = 0; x < board_.width; ++x)
        {
            if (const Cell cell{x, y}; cell != center && distance(center, cell) <= 10)
            {
                cells.push_back(cell);
            }
        }
    }
    return cells;
}

void Session::bathe(const Zone &pillar, Actor &creature)
{
    // The light falls on a creature its caster can see, once a turn; the
    // pillar may have ended with its caster's Concentration mid-turn.
    const bool standing =
        std::any_of(zones_.begin(), zones_.end(),
                    [&](const auto &zone)
                    {
                        return zone.kind == ZoneKind::pillar && zone.caster == pillar.caster;
                    });
    const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto &a)
                                     {
                                         return a.source.id == pillar.caster;
                                     });
    if (!standing || caster == actors_.end() || creature.life.dead || absent(creature) ||
        sealed_off(creature) || detail::has_effect(creature.effects, detail::EffectKind::bathed) ||
        !can_see(*caster, creature))
    {
        return;
    }
    const unsigned rest_of_turn = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
    if (detail::can_apply(creature.effects))
    {
        detail::apply_poisoned(creature.effects, effect_source(*caster), caster->source.name,
                               rest_of_turn, detail::EffectKind::bathed);
    }
    const auto &spell = *detail::find_spell("conjure_celestial");
    const int more = static_cast<int>(pillar.slot - spell.level);
    // Healing Light for the caster's allies: 4d12 + the spellcasting modifier.
    if (creature.source.side == caster->source.side)
    {
        log("Healing light shines on " + creature.source.name + ".",
            {"Healing light shines on {name}.", {{"name", creature.source.name}}});
        heal(creature, healing_roll(creature, {4 + more, 12, casting_modifier(def(*caster))}) +
                           disciple_of_life(*caster, pillar.slot));
        return;
    }
    // Searing Light for the others: a Dexterity save against 6d12 Radiant.
    const bool saved = saving_throw_succeeds(creature, spell.save, pillar.save_dc, false,
                                             heightened_against(creature, *caster, spell.id));
    const int rolled = dice({spell.dice.count + more, spell.dice.sides, 0});
    const int amount = resolved_damage(creature, spell.damage, saved ? rolled / 2 : rolled);
    log(creature.source.name + " takes " + std::to_string(amount) +
            " Radiant damage from the searing light.",
        {"{name} takes {damage} Radiant damage from the searing light.",
         {{"name", creature.source.name}, {"damage", std::to_string(amount)}}});
    damage(creature, amount);
}

bool Session::pillar_reaches(const Zone &pillar, const Actor &onto) const
{
    // It moves up to 30 feet, here onto a creature its caster can see.
    return !onto.life.dead && !absent(onto) && !in_zone(pillar, onto) &&
           distance(pillar.cells.front(), onto) <= 30 && can_see(actor(pillar.caster), onto);
}

void Session::move_pillar(Actor &caster, const Actor &onto)
{
    auto &pillar =
        *std::find_if(zones_.begin(), zones_.end(),
                      [&](const auto &zone)
                      {
                          return zone.kind == ZoneKind::pillar && zone.caster == caster.source.id;
                      });
    const auto before = pillar;
    pillar.cells = pillar_cells(onto.source.cell);
    const auto after = pillar;
    // Conjure Animals' marker serves: no caster holds both spells.
    detail::apply_spell_benefit(caster.effects, effect_source(caster), caster.source.name,
                                detail::EffectKind::pack_moved, 0);
    log(caster.source.name + " moves the pillar of light.",
        {"{name} moves the pillar of light.", {{"name", caster.source.name}}});
    for (auto &other : actors_)
    {
        if (in_zone(after, other) && !in_zone(before, other))
        {
            bathe(after, other);
        }
    }
}

void Session::divine_word(Actor &cleric, int dc)
{
    const auto &spell = *detail::find_spell("divine_word");
    log(cleric.source.name + " utters a divine word.",
        {"{name} utters a divine word.", {{"name", cleric.source.name}}});
    for (auto &other : actors_)
    {
        if (other.source.side == cleric.source.side || other.life.dead || absent(other) ||
            distance(cleric, other) > spell.range || sealed_off(other) ||
            saving_throw_succeeds(other, detail::Ability::charisma, dc, false,
                                  heightened_against(other, cleric, spell.id)))
        {
            continue;
        }
        const auto &type = def(other).creature_type;
        const bool sent_home =
            type == "celestial" || type == "elemental" || type == "fey" || type == "fiend";
        // Sent home for a day, it counts as defeated, as after Banishment
        // (LEVELS-1); 20 Hit Points or fewer, it dies.
        if (sent_home || other.life.hp <= 20)
        {
            other.life.hp = 0;
            other.life.dead = true;
            other.life.recovery = {};
            if (sent_home)
            {
                log(other.source.name + " is sent back to its plane.",
                    {"{name} is sent back to its plane.", {{"name", other.source.name}}});
            }
            else
            {
                log(other.source.name + " dies.", {"{name} dies.", {{"name", other.source.name}}});
            }
            fall(other);
            continue;
        }
        if (other.life.hp > 40 || !detail::can_apply(other.effects))
        {
            continue;
        }
        // 21-30 Hit Points: Blinded and Stunned for an hour; 31-40: Blinded for
        // 10 minutes. Deafened is not modeled (DEAF-1).
        const bool stunned = other.life.hp <= 30;
        detail::apply_spell_benefit(other.effects, effect_source(cleric), cleric.source.name,
                                    detail::EffectKind::worded, stunned ? 1 : 2);
        if (stunned)
        {
            log(other.source.name + " is Blinded and Stunned.",
                {"{name} is Blinded and Stunned.", {{"name", other.source.name}}});
        }
        else
        {
            other.effects.active.back().remaining_ms = 600000;
            log(other.source.name + " is Blinded.",
                {"{name} is Blinded.", {{"name", other.source.name}}});
        }
    }
}

void Session::tremor(const Zone &quake)
{
    const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto &a)
                                     {
                                         return a.source.id == quake.caster;
                                     });
    if (caster == actors_.end())
    {
        return;
    }
    const auto &spell = *detail::find_spell("earthquake");
    log("The ground shakes.", {"The ground shakes.", {}});
    for (auto &other : actors_)
    {
        if (other.life.dead || absent(other) || !in_zone(quake, other) || sealed_off(other) ||
            saving_throw_succeeds(other, spell.save, quake.save_dc, false,
                                  heightened_against(other, *caster, spell.id)))
        {
            continue;
        }
        if (!other.effects.prone)
        {
            other.effects.prone = true;
            log(other.source.name + " is knocked Prone.",
                {"{name} is knocked Prone.", {{"name", other.source.name}}});
        }
        // Its caster too: the tremor may end itself.
        end_concentration(other);
    }
}

void Session::finish_knitting(Actor &target, EntityId caster)
{
    if (target.life.dead)
    {
        return;
    }
    const auto &spell = *detail::find_spell("regenerate");
    log("Regenerate takes hold of " + target.source.name + ".",
        {"{spell} takes hold of {name}.",
         {{"spell", "Regenerate", true}, {"name", target.source.name}}});
    heal(target, healing_roll(target, spell.dice) + disciple_of_life(actor(caster), spell.level));
    std::erase_if(target.effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::regenerating;
                  });
    if (detail::can_apply(target.effects))
    {
        detail::apply_spell_benefit(target.effects, effect_source(actor(caster)),
                                    actor(caster).source.name, detail::EffectKind::regenerating, 0);
    }
}

const Session::Zone *Session::holy_aura(const Actor &a) const
{
    for (const auto &zone : zones_)
    {
        if (zone.kind == ZoneKind::holy && actor(zone.caster).source.side == a.source.side &&
            in_emanation(zone, a) && !in_antimagic(a.source.cell))
        {
            return &zone;
        }
    }
    return nullptr;
}

bool Session::in_antimagic(Cell cell) const
{
    return std::any_of(zones_.begin(), zones_.end(),
                       [&](const auto &zone)
                       {
                           return zone.kind == ZoneKind::antimagic && in_emanation(zone, cell);
                       });
}

void Session::suppress_spells()
{
    for (auto &a : actors_)
    {
        if (!a.life.dead && in_antimagic(a.source.cell))
        {
            const auto spells =
                std::stable_partition(a.effects.active.begin(), a.effects.active.end(),
                                      [](const auto &e)
                                      {
                                          return !spell_level_of(e.kind);
                                      });
            a.suppressed.insert(a.suppressed.end(), spells, a.effects.active.end());
            a.effects.active.erase(spells, a.effects.active.end());
        }
        else if (!a.suppressed.empty())
        {
            a.effects = all_effects(a);
            a.suppressed.clear();
        }
    }
}

void Session::place_storm_cube(Cell cell)
{
    // The board splits into 10-foot cubes, two squares by two. Clicking the
    // current cube again removes it; another placed cube becomes current; a
    // new one must touch a placed cube, unless it is the only one, which
    // then moves.
    const Cell cube{cell.x - cell.x % 2, cell.y - cell.y % 2};
    auto &cubes = area_->cubes;
    const auto placed = std::find(cubes.begin(), cubes.end(), cube);
    if (placed != cubes.end())
    {
        if (cube == area_->center && cubes.size() > 1)
        {
            cubes.erase(placed);
            area_->center = cubes.back();
        }
        else
        {
            area_->center = cube;
        }
        return;
    }
    const bool touches =
        std::any_of(cubes.begin(), cubes.end(),
                    [&](Cell other)
                    {
                        return std::abs(other.x - cube.x) <= 2 && std::abs(other.y - cube.y) <= 2;
                    });
    if (cubes.size() == 1 && !touches)
    {
        cubes.front() = cube;
    }
    else if (touches && cubes.size() < detail::find_spell(area_->verb)->storm)
    {
        cubes.push_back(cube);
    }
    else
    {
        return;
    }
    area_->center = cube;
}

std::vector<Cell> Session::storm_cells(const std::vector<Cell> &cubes) const
{
    std::vector<Cell> cells;
    for (const auto cube : cubes)
    {
        for (const auto cell : {cube, Cell{cube.x + 1, cube.y}, Cell{cube.x, cube.y + 1},
                                Cell{cube.x + 1, cube.y + 1}})
        {
            if (board_.contains(cell))
            {
                cells.push_back(cell);
            }
        }
    }
    return cells;
}

void Session::grope(const Zone &tentacles, Actor &creature)
{
    const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto &a)
                                     {
                                         return a.source.id == tentacles.caster;
                                     });
    const bool writhing =
        std::any_of(zones_.begin(), zones_.end(),
                    [&](const auto &zone)
                    {
                        return zone.kind == ZoneKind::tentacles && zone.caster == tentacles.caster;
                    });
    if (!writhing || caster == actors_.end() || creature.life.dead || creature.life.hp == 0 ||
        absent(creature) || sealed_off(creature) ||
        detail::has_effect(creature.effects, detail::EffectKind::groped) ||
        !detail::can_apply(creature.effects))
    {
        return;
    }
    const unsigned rest_of_turn = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
    detail::apply_poisoned(creature.effects, effect_source(*caster), caster->source.name,
                           rest_of_turn, detail::EffectKind::groped);
    const auto &spell = *detail::find_spell("black_tentacles");
    if (saving_throw_succeeds(creature, spell.save, tentacles.save_dc, false,
                              heightened_against(creature, *caster, spell.id)))
    {
        return;
    }
    const int amount =
        resolved_damage(creature, spell.damage,
                        dice({spell.dice.count + static_cast<int>(tentacles.slot - spell.level),
                              spell.dice.sides, 0}));
    log(creature.source.name + " takes " + std::to_string(amount) +
            " Bludgeoning damage from the tentacles.",
        {"{name} takes {damage} Bludgeoning damage from the tentacles.",
         {{"name", creature.source.name}, {"damage", std::to_string(amount)}}});
    damage(creature, amount);
    if (creature.life.dead || detail::has_effect(creature.effects, detail::EffectKind::tentacled) ||
        !detail::can_apply(creature.effects))
    {
        return;
    }
    detail::apply_entangle(creature.effects, effect_source(*caster), caster->source.name,
                           tentacles.save_dc, detail::EffectKind::tentacled);
    log(creature.source.name + " is Restrained.",
        {"{name} is Restrained.", {{"name", creature.source.name}}});
}

std::vector<Cell> Session::burning_side(std::string_view verb, Cell center,
                                        const std::vector<Cell> &wall) const
{
    // A straight wall's side by the sign of the square's offset across it;
    // a ring's inside or outside by its distance from the centre.
    static constexpr std::array<Cell, 4> steps{Cell{1, 0}, Cell{0, 1}, Cell{1, -1}, Cell{1, 1}};
    const auto side = *detail::wall_side(verb);
    const auto direction = side / 2;
    const bool first = side % 2 == 0;
    std::vector<Cell> cells;
    for (int y = 0; y < board_.height; ++y)
    {
        for (int x = 0; x < board_.width; ++x)
        {
            const Cell cell{x, y};
            if (std::find(wall.begin(), wall.end(), cell) != wall.end() ||
                std::none_of(wall.begin(), wall.end(),
                             [&](Cell square)
                             {
                                 return distance(square, cell) <= 10;
                             }))
            {
                continue;
            }
            bool burning = false;
            if (direction == 4)
            {
                burning = first == (distance(center, cell) < 10);
            }
            else
            {
                const auto step = steps[direction];
                // Positive across a row is north, across a column west.
                const int across = (cell.x - center.x) * step.y - (cell.y - center.y) * step.x;
                burning = across != 0 && first == (direction == 1 ? across < 0 : across > 0);
            }
            if (burning)
            {
                cells.push_back(cell);
            }
        }
    }
    return cells;
}

void Session::wall_burns(const Zone &fire, Actor &creature)
{
    const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto &a)
                                     {
                                         return a.source.id == fire.caster;
                                     });
    const bool burning =
        std::any_of(zones_.begin(), zones_.end(),
                    [&](const auto &zone)
                    {
                        return zone.kind == fire.kind && zone.caster == fire.caster;
                    });
    if (!burning || caster == actors_.end() || creature.life.dead || absent(creature) ||
        sealed_off(creature))
    {
        return;
    }
    const auto &spell = *detail::find_spell("wall_of_fire");
    const int amount = resolved_damage(
        creature, spell.damage,
        dice({spell.dice.count + static_cast<int>(fire.slot - spell.level), spell.dice.sides, 0}));
    log(creature.source.name + " takes " + std::to_string(amount) +
            " Fire damage from the wall of fire.",
        {"{name} takes {damage} Fire damage from the wall of fire.",
         {{"name", creature.source.name}, {"damage", std::to_string(amount)}}});
    damage(creature, amount);
}

Actor &Session::summon(const Actor &caster, std::string definition, std::string name, Cell cell)
{
    EntityId id = 1;
    for (const auto &a : actors_)
    {
        id = std::max(id, a.source.id + 1);
    }
    for (const auto &gone : fled_)
    {
        id = std::max(id, gone.source.id + 1);
    }
    Actor creature;
    creature.definition = content_->definitions.at(definition);
    creature.source.id = id;
    creature.source.definition = std::move(definition);
    creature.source.name = std::move(name);
    creature.source.side = caster.source.side;
    creature.source.cell = cell;
    creature.source.footprint = summoned_footprint(creature.source.definition);
    creature.life.hp = creature.definition.hp;
    creature.initiative = caster.initiative;
    creature.movement = creature.definition.speed;
    creature.slots = creature.definition.slots;
    // Its attack bonus is its summoner's spell attack bonus.
    detail::apply_spell_benefit(creature.effects, effect_source(caster), caster.source.name,
                                detail::EffectKind::summoned,
                                std::clamp(spell_dc(caster) - 8, 0, 30));
    refresh_form(creature);
    // The room reserved for summons keeps every reference to an actor valid;
    // settle_dominated() puts it right after its summoner once the command ends.
    if (actors_.size() == actors_.capacity())
    {
        throw std::runtime_error("No room for another creature");
    }
    actors_.push_back(std::move(creature));
    return actors_.back();
}

std::vector<Cell> Session::free_squares_near(Cell from, Footprint size) const
{
    std::vector<Cell> cells;
    const auto board = zoned_board();
    const auto free = [&](Cell square)
    {
        return board.at(square) != Terrain::obstacle && !guarded_square(square) &&
               std::none_of(actors_.begin(), actors_.end(),
                            [&](const auto &other)
                            {
                                return !other.life.dead && !absent(other) &&
                                       occupies(other, square);
                            });
    };
    for (int y = 0; y < board_.height; ++y)
    {
        for (int x = 0; x < board_.width; ++x)
        {
            const auto squares = squares_of({x, y}, size);
            if (std::all_of(squares.begin(), squares.end(), free))
            {
                cells.push_back({x, y});
            }
        }
    }
    std::stable_sort(cells.begin(), cells.end(),
                     [&](Cell x, Cell y)
                     {
                         return distance(from, x) < distance(from, y);
                     });
    return cells;
}

void Session::engulf(const Zone &spirit, Actor &creature)
{
    const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto &a)
                                     {
                                         return a.source.id == spirit.caster;
                                     });
    const bool standing =
        std::any_of(zones_.begin(), zones_.end(),
                    [&](const auto &zone)
                    {
                        return zone.kind == ZoneKind::elemental && zone.caster == spirit.caster;
                    });
    // The spirit seizes the caster's foes, one at a time.
    const bool holding =
        std::any_of(actors_.begin(), actors_.end(),
                    [&](const auto &other)
                    {
                        return std::any_of(other.effects.active.begin(), other.effects.active.end(),
                                           [&](const auto &e)
                                           {
                                               return e.kind == detail::EffectKind::engulfed &&
                                                      e.source_actor == spirit.caster &&
                                                      e.source_scope == scope_;
                                           });
                    });
    if (!standing || holding || caster == actors_.end() ||
        creature.source.side == caster->source.side || creature.life.dead ||
        creature.life.hp == 0 || absent(creature) || sealed_off(creature) ||
        !can_see(*caster, creature))
    {
        return;
    }
    const auto type_of = std::find_if(caster->effects.active.begin(), caster->effects.active.end(),
                                      [](const auto &e)
                                      {
                                          return e.kind == detail::EffectKind::elemental_spirit;
                                      });
    if (type_of == caster->effects.active.end())
    {
        return;
    }
    const auto type = static_cast<detail::DamageType>(type_of->dc);
    const auto &spell = *detail::find_spell("conjure_elemental");
    if (saving_throw_succeeds(creature, spell.save, spirit.save_dc, false,
                              heightened_against(creature, *caster, spell.id)))
    {
        return;
    }
    const auto name = std::string(detail::damage_name(type));
    const int amount = resolved_damage(
        creature, type,
        dice({spell.dice.count + static_cast<int>(spirit.slot - spell.level), 8, 0}));
    log(creature.source.name + " takes " + std::to_string(amount) + " " + name +
            " damage from the elemental spirit.",
        {"{name} takes {damage} {type} damage from the elemental spirit.",
         {{"name", creature.source.name},
          {"damage", std::to_string(amount)},
          {"type", name, true}}});
    damage(creature, amount);
    if (creature.life.dead || !detail::can_apply(creature.effects))
    {
        return;
    }
    detail::apply_spell_benefit(creature.effects, effect_source(*caster), caster->source.name,
                                detail::EffectKind::engulfed, static_cast<int>(type));
    log(creature.source.name + " is Restrained by the spirit.",
        {"{name} is Restrained by the spirit.", {{"name", creature.source.name}}});
}

void Session::choke_cloud(const Zone &cloud, Actor &creature)
{
    const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto &a)
                                     {
                                         return a.source.id == cloud.caster;
                                     });
    const bool lingering =
        std::any_of(zones_.begin(), zones_.end(),
                    [&](const auto &zone)
                    {
                        return zone.kind == cloud.kind && zone.caster == cloud.caster;
                    });
    if (!lingering || caster == actors_.end() || creature.life.dead || creature.life.hp == 0 ||
        absent(creature) || sealed_off(creature) ||
        detail::has_effect(creature.effects, detail::EffectKind::clouded) ||
        !detail::can_apply(creature.effects))
    {
        return;
    }
    const unsigned rest_of_turn = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
    detail::apply_poisoned(creature.effects, effect_source(*caster), caster->source.name,
                           rest_of_turn, detail::EffectKind::clouded);
    // Incendiary Cloud's embers burn as Cloudkill's fog poisons.
    const bool embers = cloud.kind == ZoneKind::embers;
    const auto &spell = *detail::find_spell(embers ? "incendiary_cloud" : "cloudkill");
    const bool saved = saving_throw_succeeds(creature, spell.save, cloud.save_dc, false,
                                             heightened_against(creature, *caster, spell.id));
    const int rolled = dice({spell.dice.count + static_cast<int>(cloud.slot - spell.level), 8, 0});
    const int amount = resolved_damage(creature, spell.damage, saved ? rolled / 2 : rolled);
    if (embers)
    {
        log(creature.source.name + " takes " + std::to_string(amount) +
                " Fire damage from the embers.",
            {"{name} takes {damage} Fire damage from the embers.",
             {{"name", creature.source.name}, {"damage", std::to_string(amount)}}});
    }
    else
    {
        log(creature.source.name + " takes " + std::to_string(amount) +
                " Poison damage from the fog.",
            {"{name} takes {damage} Poison damage from the fog.",
             {{"name", creature.source.name}, {"damage", std::to_string(amount)}}});
    }
    damage(creature, amount);
}

void Session::hurl(Actor &caster, Actor &creature, Cell to)
{
    // Square by square toward the chosen square while the way is open; a wall
    // or a creature in the way stops it, with the PUSH-1 damage.
    Cell at = creature.source.cell;
    int moved = 0;
    const int squares = std::max(std::abs(to.x - at.x), std::abs(to.y - at.y));
    for (int n = 1; n <= squares; ++n)
    {
        const double along = static_cast<double>(n) / squares;
        const Cell next{static_cast<int>(std::lround(creature.source.cell.x +
                                                     (to.x - creature.source.cell.x) * along)),
                        static_cast<int>(std::lround(creature.source.cell.y +
                                                     (to.y - creature.source.cell.y) * along))};
        if (blocked_for(creature, next, walled_board(true)))
        {
            break;
        }
        at = next;
        ++moved;
    }
    creature.source.cell = at;
    log(caster.source.name + " moves " + creature.source.name + " " + std::to_string(5 * moved) +
            " feet.",
        {"{name} moves {target} {feet} feet.",
         {{"name", caster.source.name},
          {"target", creature.source.name},
          {"feet", std::to_string(5 * moved)}}});
    if (moved < squares)
    {
        slam(creature, 5 * (squares - moved));
    }
}

bool Session::globe_wards(const Actor &caster, Cell cell, unsigned level) const
{
    return std::any_of(zones_.begin(), zones_.end(),
                       [&](const auto &zone)
                       {
                           return zone.kind == ZoneKind::globe && in_zone(zone, cell) &&
                                  !in_zone(zone, caster) && level <= zone.slot - 1;
                       });
}

void Session::fire_contingency(Actor &a)
{
    const auto stored = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                     [](const auto &e)
                                     {
                                         return e.kind == detail::EffectKind::contingency;
                                     });
    if (stored == a.effects.active.end() || a.life.dead)
    {
        return;
    }
    // std::span has no at() before C++26: check the stored place as at() did.
    const auto spells = detail::spell_table();
    const auto stored_place = static_cast<std::size_t>(stored->dc / 4);
    if (stored_place >= spells.size())
    {
        throw std::out_of_range("contingency: no spell at the stored place");
    }
    const auto &spell = spells[stored_place];
    a.effects.active.erase(stored);
    log("Contingency: " + std::string(spell.label) + " takes effect on " + a.source.name + ".",
        {"Contingency: {spell} takes effect on {name}.",
         {{"spell", std::string(spell.label), true}, {"name", a.source.name}}});
    if (spell.pattern == detail::SpellPattern::heal)
    {
        auto rolled = spell.dice;
        if (spell.add_casting_modifier)
        {
            rolled.bonus = casting_modifier(def(a), spell.id);
        }
        heal(a, healing_roll(a, rolled) + disciple_of_life(a, spell.level));
        return;
    }
    if (spell.concentration)
    {
        begin_concentration(a, spell);
    }
    apply_rider(spell, spell.id, a, a, spell_dc(a, spell.id));
}

void Session::explode_bead(Actor &caster, const Zone &bead)
{
    // 12d6, and 1d6 more for each slot level above 7 and each turn it waited.
    auto spell = *detail::find_spell("delayed_blast_fireball");
    spell.dice.count += static_cast<int>(bead.slot - spell.level) + bead.dealt;
    log("The bead bursts.", {"The bead bursts.", {}});
    damage_area(caster, spell, spell.id, bead.cells, bead.save_dc);
}

void Session::prismatic_rays(Actor &caster, Actor &struck, bool saved, int dc, const Dice &rolled)
{
    // An 8 strikes with two rays, rerolling any 8.
    std::vector<int> rays{roll(8)};
    if (rays.front() == 8)
    {
        for (rays.clear(); rays.size() < 2;)
        {
            if (const int ray = roll(8); ray != 8)
            {
                rays.push_back(ray);
            }
        }
    }
    static constexpr std::array<std::string_view, 7> colors{"red",  "orange", "yellow", "green",
                                                            "blue", "indigo", "violet"};
    static constexpr std::array types{detail::DamageType::fire, detail::DamageType::acid,
                                      detail::DamageType::lightning, detail::DamageType::poison,
                                      detail::DamageType::cold};
    for (const int ray : rays)
    {
        if (struck.life.dead)
        {
            return;
        }
        const std::string color(colors[static_cast<std::size_t>(ray - 1)]);
        log(struck.source.name + " is struck by the " + color + " ray.",
            {"{name} is struck by the {color} ray.",
             {{"name", struck.source.name}, {"color", color, true}}});
        if (ray <= 5)
        {
            const auto damage_type = cast_damage_type(types[static_cast<std::size_t>(ray - 1)]);
            const auto type = std::string(detail::damage_name(damage_type));
            const int amount = resolved_damage(struck, damage_type,
                                               evaded(struck, detail::Ability::dexterity, true,
                                                      saved, spell_dice(caster, rolled)));
            log(struck.source.name + " takes " + std::to_string(amount) + " " + type + " damage.",
                {"{name} takes {damage} {type} damage.",
                 {{"name", struck.source.name},
                  {"damage", std::to_string(amount)},
                  {"type", type, true}}});
            damage(struck, amount);
            continue;
        }
        if (saved || !detail::can_apply(struck.effects))
        {
            continue;
        }
        // Indigo: Restrained, turning to stone after three failed saves.
        // Violet: Blinded, and sent away on a failed save at the caster's
        // next turn.
        if (ray == 6)
        {
            detail::apply_spell_benefit(struck.effects, effect_source(caster), caster.source.name,
                                        detail::EffectKind::indigo, 100 * dc);
            log(struck.source.name + " is Restrained.",
                {"{name} is Restrained.", {{"name", struck.source.name}}});
        }
        else
        {
            detail::apply_spell_benefit(struck.effects, effect_source(caster), caster.source.name,
                                        detail::EffectKind::violet, dc);
            log(struck.source.name + " is Blinded.",
                {"{name} is Blinded.", {{"name", struck.source.name}}});
        }
    }
}

bool Session::caged_apart(Cell from, Cell to, bool weapon) const
{
    return std::any_of(zones_.begin(), zones_.end(),
                       [&](const auto &zone)
                       {
                           return (zone.kind == ZoneKind::box ||
                                   (weapon && zone.kind == ZoneKind::cage)) &&
                                  in_zone(zone, from) != in_zone(zone, to);
                       });
}

void Session::fall_hard(Actor &a)
{
    // The cylinder is 100 feet high: the PUSH-1 scale's most, 3d6.
    const int amount = resolved_damage(a, detail::DamageType::bludgeoning, dice({3, 6, 0}));
    log(a.source.name + " takes " + std::to_string(amount) + " Bludgeoning damage from the fall.",
        {"{name} takes {damage} Bludgeoning damage from the fall.",
         {{"name", a.source.name}, {"damage", std::to_string(amount)}}});
    damage(a, amount);
}

void Session::seek_maze_exit(Actor &a)
{
    // A DC 20 Intelligence (Investigation) check; a creature's Intelligence
    // save bonus stands in for Investigation.
    const int natural = reliable(a, "investigation",
                                 detail::d20({false, check_disadvantage(a), def(a).lucky}, rng_));
    int bonus = (a.source.character_profile.empty() ? def(a).saves[3] : def(a).investigation) +
                guidance_bonus(a, "investigation");
    if (natural + bonus < 20)
    {
        bonus += dark_luck_check(a, 20 - natural - bonus);
    }
    const bool escaped = natural + bonus >= 20;
    log(a.source.name + " Investigation check: d20 " + std::to_string(natural) + " + " +
            std::to_string(bonus) + " vs DC 20" + (escaped ? " (escapes)." : " (still lost)."),
        {escaped ? "{name} Investigation check: d20 {roll} + {bonus} vs DC 20 (escapes)."
                 : "{name} Investigation check: d20 {roll} + {bonus} vs DC 20 (still lost).",
         {{"name", a.source.name},
          {"roll", std::to_string(natural)},
          {"bonus", std::to_string(bonus)}}});
    if (!escaped)
    {
        return;
    }
    const auto lost = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                   [](const auto &e)
                                   {
                                       return e.kind == detail::EffectKind::mazed;
                                   });
    const auto caster = lost->source_actor;
    a.effects.active.erase(lost);
    a.source.cell = nearest_open(a);
    log(a.source.name + " finds its way out of the maze.",
        {"{name} finds its way out of the maze.", {{"name", a.source.name}}});
    // Its escape ends the spell.
    if (const auto found = std::find_if(actors_.begin(), actors_.end(),
                                        [&](const auto &other)
                                        {
                                            return other.source.id == caster;
                                        });
        found != actors_.end())
    {
        end_concentration(*found);
    }
}

void Session::take_overchannel(Actor &caster, const detail::SpellDef &spell, std::string_view verb)
{
    // A Wizard spell that deals damage, cast with a level 1-5 slot.
    overchanneling_ = false;
    const unsigned slot = slot_level_of(verb, spell);
    if (!detail::has_effect(caster.effects, detail::EffectKind::overchannel) || !spell.level ||
        slot > 5 || !spell.dice.count || spell.pattern == detail::SpellPattern::heal ||
        verb.ends_with(detail::initiate_suffix) || verb.ends_with(detail::intervention_suffix) ||
        !detail::on_class_list(CharacterClass::wizard, spell.id))
    {
        return;
    }
    std::erase_if(caster.effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::overchannel;
                  });
    overchanneling_ = true;
    overchannel_caster_ = caster.source.id;
    const auto used = std::find_if(caster.effects.active.begin(), caster.effects.active.end(),
                                   [](const auto &e)
                                   {
                                       return e.kind == detail::EffectKind::overchanneled;
                                   });
    // Each use after the first adds 1d12 per slot level to the 2d12.
    if (used == caster.effects.active.end())
    {
        detail::apply_spell_benefit(caster.effects, effect_source(caster), caster.source.name,
                                    detail::EffectKind::overchanneled, 1);
    }
    else
    {
        overchannel_dice_ = (used->dc + 1) * static_cast<int>(slot);
        used->dc = std::min(used->dc + 1, 20);
    }
    log(caster.source.name + " overchannels the spell.",
        {"{name} overchannels the spell.", {{"name", caster.source.name}}});
}

void Session::enchant_area(Actor &caster, const detail::SpellDef &spell,
                           const std::vector<Cell> &cells)
{
    // Hypnotic Pattern takes only creatures that can see it.
    begin_concentration(caster, spell);
    const bool pattern = spell.rider == detail::Rider::hypnotic_pattern;
    const int dc = area_dc(caster, spell);
    const auto careful = careful_allies(caster, cells);
    for (auto &other : actors_)
    {
        if (other.life.dead || other.life.hp == 0 || other.source.id == caster.source.id ||
            absent(other) || sealed_off(other) || !touches(cells, other) ||
            std::find(careful.begin(), careful.end(), other.source.id) != careful.end() ||
            !detail::can_apply(other.effects) || (pattern && detail::blinded(other.effects)) ||
            globe_wards(caster, other.source.cell, spell.level) ||
            caged_apart(caster.source.cell, other.source.cell, false) ||
            (pattern && detail::has_effect(other.effects, detail::EffectKind::mind_blank)) ||
            charm_or_fear_save(other, spell.save, dc,
                               pattern ? def(other).fey_ancestry : def(other).brave,
                               heightened_against(other, caster, spell.id)))
        {
            continue;
        }
        detail::apply_spell_benefit(
            other.effects, effect_source(caster), caster.source.name,
            pattern ? detail::EffectKind::entranced : detail::EffectKind::terrified, 0);
        if (pattern)
        {
            log(other.source.name + " is entranced by the pattern.",
                {"{name} is entranced by the pattern.", {{"name", other.source.name}}});
        }
        else
        {
            log(other.source.name + " is Frightened.",
                {"{name} is Frightened.", {{"name", other.source.name}}});
        }
    }
}

void Session::slow_area(Actor &caster, const detail::SpellDef &spell,
                        const std::vector<Cell> &cells, Cell center)
{
    // Up to six creatures of the caster's choice: its enemies in the Cube, the
    // nearest to its centre first.
    begin_concentration(caster, spell);
    const int dc = area_dc(caster, spell);
    std::vector<EntityId> chosen;
    for (const auto &other : actors_)
    {
        if (other.source.side != caster.source.side && !other.life.dead && other.life.hp > 0 &&
            !absent(other) && !sealed_off(other) && touches(cells, other) &&
            !globe_wards(caster, other.source.cell, spell.level) &&
            !caged_apart(caster.source.cell, other.source.cell, false))
        {
            chosen.push_back(other.source.id);
        }
    }
    std::stable_sort(chosen.begin(), chosen.end(),
                     [&](EntityId left, EntityId right)
                     {
                         return distance(center, actor(left)) < distance(center, actor(right));
                     });
    if (chosen.size() > 6)
    {
        chosen.resize(6);
    }
    for (const auto id : chosen)
    {
        auto &other = actor(id);
        if (!detail::can_apply(other.effects) ||
            saving_throw_succeeds(other, spell.save, dc, false,
                                  heightened_against(other, caster, spell.id)))
        {
            continue;
        }
        detail::apply_repeating_condition(
            other.effects, detail::EffectKind::time_slowed, effect_source(caster),
            caster.source.name, detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(id)});
        other.reaction = false;
        log(other.source.name + " is slowed.",
            {"{name} is slowed.", {{"name", other.source.name}}});
    }
}

void Session::sleet_falls(const Zone &sleet, Actor &creature)
{
    const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto &a)
                                     {
                                         return a.source.id == sleet.caster;
                                     });
    const bool falling =
        std::any_of(zones_.begin(), zones_.end(),
                    [&](const auto &zone)
                    {
                        return zone.kind == ZoneKind::sleet && zone.caster == sleet.caster;
                    });
    if (!falling || caster == actors_.end() || creature.life.dead || absent(creature) ||
        sealed_off(creature) || detail::has_effect(creature.effects, detail::EffectKind::sleeted))
    {
        return;
    }
    const unsigned rest_of_turn = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
    if (detail::can_apply(creature.effects))
    {
        detail::apply_poisoned(creature.effects, effect_source(*caster), caster->source.name,
                               rest_of_turn, detail::EffectKind::sleeted);
    }
    if (saving_throw_succeeds(creature, detail::Ability::dexterity, sleet.save_dc, false,
                              heightened_against(creature, *caster, "sleet_storm")))
    {
        return;
    }
    if (!creature.effects.prone)
    {
        creature.effects.prone = true;
        log(creature.source.name + " slips and falls Prone.",
            {"{name} slips and falls Prone.", {{"name", creature.source.name}}});
    }
    // Its caster too: the storm may end itself.
    end_concentration(creature);
}

void Session::choke(const Zone &cloud, Actor &creature)
{
    const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto &a)
                                     {
                                         return a.source.id == cloud.caster;
                                     });
    if (caster == actors_.end() || creature.life.dead || creature.life.hp == 0 ||
        absent(creature) || sealed_off(creature) || !detail::can_apply(creature.effects) ||
        saving_throw_succeeds(creature, detail::Ability::constitution, cloud.save_dc,
                              def(creature).dwarf,
                              heightened_against(creature, *caster, "stinking_cloud")))
    {
        return;
    }
    const unsigned rest_of_turn = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
    detail::apply_poisoned(creature.effects, effect_source(*caster), caster->source.name,
                           rest_of_turn, detail::EffectKind::retching);
    creature.actions = {false};
    creature.bonus = false;
    log(creature.source.name + " retches in the cloud and can take no action this turn.",
        {"{name} retches in the cloud and can take no action this turn.",
         {{"name", creature.source.name}}});
}

bool Session::protective_counter(const Actor &wizard, const Actor &caster,
                                 const detail::SpellDef &spell) const
{
    // Only a spell of level 1 or higher is worth a use.
    return def(wizard).protective_magic && trait_uses_left(wizard) > 0 && spell.level > 0 &&
           wizard.source.side != caster.source.side && conscious(wizard) && wizard.reaction &&
           !detail::incapacitated(wizard.effects) && distance(wizard, caster) <= 60 &&
           can_see(wizard, caster) && !sealed_off(wizard) && !absent(wizard);
}

bool Session::can_counter(const Actor &wizard, const Actor &caster) const
{
    // A level-3 slot or higher, one slot a turn, a Reaction, and the casting
    // seen within 60 feet; no spell is cast in an Antimagic Field.
    return wizard.source.side != caster.source.side && conscious(wizard) && wizard.reaction &&
           !detail::incapacitated(wizard.effects) && lowest_slot(wizard, 3) && !wizard.spent_slot &&
           detail::knows_spell(def(wizard).spells, "counterspell") &&
           distance(wizard, caster) <= 60 && can_see(wizard, caster) && !sealed_off(wizard) &&
           !absent(wizard);
}

bool Session::countered(Actor &caster, const detail::SpellDef &spell)
{
    for (auto &wizard : actors_)
    {
        const bool protective = protective_counter(wizard, caster, spell);
        if (!protective && !can_counter(wizard, caster))
        {
            continue;
        }
        if (!protective)
        {
            ask_reaction(wizard, Asked::counter);
            if (answer_of(wizard.source.id, Asked::counter)->verb != "counterspell")
            {
                continue;
            }
        }
        wizard.reaction = false;
        const auto &counter = *detail::find_spell("counterspell");
        if (protective)
        {
            spend_trait_use(wizard);
        }
        else
        {
            pay_for_spell(wizard,
                          cast_at(counter, lowest_slot(wizard, 3), "counterspell", "").first,
                          counter);
        }
        log(wizard.source.name + " casts Counterspell.",
            {"{name} casts {spell}.",
             {{"name", wizard.source.name}, {"spell", "Counterspell", true}}});
        if (saving_throw_succeeds(caster, detail::Ability::constitution,
                                  spell_dc(wizard, "counterspell")))
        {
            continue;
        }
        log(caster.source.name + "'s spell dissipates.",
            {"{name}'s spell dissipates.", {{"name", caster.source.name}}});
        return true;
    }
    return false;
}

// Slow: a Wisdom save against the spell's DC, in place of the SRD's 25
// percent chance (LEVELS-1), or the slowed caster's spell fails, spent.
bool Session::slowed_spell_fails(Actor &caster, std::string_view verb,
                                 const detail::SpellDef &spell)
{
    const auto slow = std::find_if(caster.effects.active.begin(), caster.effects.active.end(),
                                   [](const auto &e)
                                   {
                                       return e.kind == detail::EffectKind::time_slowed;
                                   });
    if (slow == caster.effects.active.end())
    {
        return false;
    }
    const int dc = slow->dc;
    if (saving_throw_succeeds(caster, detail::Ability::wisdom, dc))
    {
        return false;
    }
    if (spell.level)
    {
        pay_for_spell(caster, verb, spell);
    }
    log(caster.source.name + "'s spell fails under Slow.",
        {"{name}'s spell fails under Slow.", {{"name", caster.source.name}}});
    return true;
}

// Slow: an action or a Bonus Action each turn, not both, and one attack with
// the Attack action.
void Session::limit_slowed_turn()
{
    if (outcome_ != Outcome::ongoing || turn_ >= actors_.size())
    {
        return;
    }
    auto &a = actors_[turn_];
    if (!detail::has_effect(a.effects, detail::EffectKind::time_slowed))
    {
        return;
    }
    a.actions.extra_attacks = 0;
    a.actions.haste = a.actions.surge = false;
    if (!a.actions.normal)
    {
        a.bonus = false;
    }
    else if (!a.bonus)
    {
        a.actions.normal = false;
    }
}

void Session::vampiric_touch(Actor &caster, Actor &target, unsigned slot)
{
    const int before = target.life.hp + target.life.temporary_hp.amount;
    const auto &spell = *detail::find_spell("vampiric_touch");
    const bool struck =
        attack(caster, target, AttackRange::melee,
               SpellAttack{.dice = {spell.dice.count + static_cast<int>(slot - spell.level),
                                    spell.dice.sides, 0},
                           .type = spell.damage,
                           .spell = spell.id});
    const int dealt = before - (target.life.hp + target.life.temporary_hp.amount);
    if (struck && dealt > 1)
    {
        log(caster.source.name + " siphons life.",
            {"{name} siphons life.", {{"name", caster.source.name}}});
        heal(caster, dealt / 2);
    }
}

void Session::ready_vitality(Actor &caster, const detail::SpellDef &spell,
                             std::size_t damaged_before)
{
    // Potent Spellcasting from Cleric 14, after a Cleric cantrip damaged someone.
    if (!def(caster).potent_spellcasting || def(caster).class_levels[CharacterClass::cleric] < 14 ||
        spell.level || !detail::on_class_list(CharacterClass::cleric, spell.id) ||
        damaged_before >= damage_taken_.size() || !detail::can_apply(caster.effects) ||
        detail::has_effect(caster.effects, detail::EffectKind::vitality))
    {
        return;
    }
    detail::apply_spell_benefit(caster.effects, effect_source(caster), caster.source.name,
                                detail::EffectKind::vitality, 0);
}

void Session::grant_vitality(Actor &cleric, Actor &target)
{
    std::erase_if(cleric.effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::vitality;
                  });
    // Temporary Hit Points do not stack: the Cleric is asked, as for False
    // Life; another creature keeps the larger.
    TemporaryHitPoints offered{2 * std::max(1, casting_modifier(def(cleric))),
                               "feature:improved_blessed_strikes"};
    log(cleric.source.name + " gives vitality to " + target.source.name + ".",
        {"{name} gives vitality to {target}.",
         {{"name", cleric.source.name}, {"target", target.source.name}}});
    if (target.life.temporary_hp.amount && target.source.id == cleric.source.id)
    {
        temporary_offer_ = std::move(offered);
    }
    else if (target.life.temporary_hp.amount < offered.amount)
    {
        detail::grant_temporary_hp(target.life, offered, TemporaryHpChoice::use_new);
    }
}

void Session::mass_cure(Actor &cleric, const detail::SpellDef &spell, std::string_view verb,
                        const std::vector<Cell> &cells)
{
    // The caster's choice of up to six creatures in the Sphere: here its
    // allies who can be healed, the most hurt first.
    std::vector<Actor *> hurt;
    for (auto &other : actors_)
    {
        if (other.source.side == cleric.source.side && !other.life.dead && !absent(other) &&
            other.life.hp < max_hp(other) && !detail::healing_blocked(other.effects) &&
            touches(cells, other))
        {
            hurt.push_back(&other);
        }
    }
    std::sort(hurt.begin(), hurt.end(),
              [&](const Actor *x, const Actor *y)
              {
                  return x->life.hp * max_hp(*y) < y->life.hp * max_hp(*x);
              });
    if (hurt.size() > spell.instances)
    {
        hurt.resize(spell.instances);
    }
    auto rolled = spell.dice;
    rolled.count += static_cast<int>(upcast_steps(verb, spell) * spell.upcast.extra_dice);
    rolled.bonus = casting_modifier(def(cleric), spell.id);
    std::vector<EntityId> healed;
    for (auto *other : hurt)
    {
        heal(*other,
             healing_roll(*other, rolled) + disciple_of_life(cleric, slot_level_of(verb, spell)));
        healed.push_back(other->source.id);
    }
    blessed_healer(cleric, spell, verb, healed);
}

void Session::contagion_save(Actor &a)
{
    auto found = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                              [](const auto &e)
                              {
                                  return e.kind == detail::EffectKind::contagion;
                              });
    if (found == a.effects.active.end() || found->dc >= 300 || a.life.hp == 0)
    {
        return;
    }
    const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto &other)
                                     {
                                         return other.source.id == found->source_actor;
                                     });
    const int dc = caster != actors_.end() ? spell_dc(*caster, "contagion") : 15;
    const bool saved = saving_throw_succeeds(a, detail::Ability::constitution, dc, def(a).dwarf);
    // Re-found: the save may have changed the effects.
    found = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                         [](const auto &e)
                         {
                             return e.kind == detail::EffectKind::contagion;
                         });
    if (found == a.effects.active.end())
    {
        return;
    }
    found->dc += saved ? 10 : 100;
    if ((found->dc / 10) % 10 >= 3)
    {
        a.effects.active.erase(found);
        log(a.source.name + " shakes off the contagion.",
            {"{name} shakes off the contagion.", {{"name", a.source.name}}});
    }
    else if (found->dc >= 300)
    {
        log(a.source.name + "'s contagion takes hold for seven days.",
            {"{name}'s contagion takes hold for seven days.", {{"name", a.source.name}}});
    }
}

bool Session::otherworldly(const Actor &a) const
{
    const auto &type = def(a).creature_type;
    return type == "celestial" || type == "elemental" || type == "fey" || type == "fiend" ||
           type == "undead";
}

bool Session::enchants(const detail::Effect &e) noexcept
{
    return e.kind == detail::EffectKind::charmed || e.kind == detail::EffectKind::dominated ||
           e.kind == detail::EffectKind::enthralled || e.kind == detail::EffectKind::turn_charmed ||
           e.kind == detail::EffectKind::frightened || e.kind == detail::EffectKind::abjured ||
           e.kind == detail::EffectKind::terrified || e.kind == detail::EffectKind::entranced ||
           e.kind == detail::EffectKind::compelled;
}

bool Session::otherworldly_source(const detail::Effect &e) const
{
    return e.source_scope == scope_ && std::any_of(actors_.begin(), actors_.end(),
                                                   [&](const auto &other)
                                                   {
                                                       return other.source.id == e.source_actor &&
                                                              otherworldly(other);
                                                   });
}

bool Session::enchanted_by_otherworld(const Actor &a) const
{
    return std::any_of(a.effects.active.begin(), a.effects.active.end(),
                       [&](const auto &e)
                       {
                           return enchants(e) && otherworldly_source(e);
                       });
}

void Session::hunters_defense(Actor &target, detail::DamageType type)
{
    const auto found = std::find_if(reaction_answers_.begin(), reaction_answers_.end(),
                                    [&](const auto &answer)
                                    {
                                        return answer.target == target.source.id &&
                                               answer.asked == Asked::hit &&
                                               answer.verb == "superior_defense";
                                    });
    if (found == reaction_answers_.end())
    {
        return;
    }
    found->verb = "defended";
    const auto name = std::string(detail::damage_name(type));
    if (!detail::can_apply(target.effects))
    {
        return;
    }
    detail::apply_spell_benefit(target.effects, effect_source(target), target.source.name,
                                detail::EffectKind::defending, static_cast<int>(type));
    log(target.source.name + " uses Superior Hunter's Defense: Resistance to " + name +
            " damage this turn.",
        {"{name} uses Superior Hunter's Defense: Resistance to {type} damage this turn.",
         {{"name", target.source.name}, {"type", name, true}}});
}

std::vector<Cell> Session::beam_cells(Cell center) const
{
    // Moonbeam's 5-foot radius, its centre first so a moved beam keeps it.
    std::vector<Cell> cells{center};
    for (const auto cell : area_cells(*detail::find_spell("moonbeam"), "moonbeam", center, center))
    {
        if (cell != center)
        {
            cells.push_back(cell);
        }
    }
    return cells;
}

void Session::moonbeam_burns(const Zone &beam, Actor &creature)
{
    // A creature saves against a Moonbeam only once per turn.
    // The beam may have ended with its caster's Concentration mid-burn.
    const bool shining =
        std::any_of(zones_.begin(), zones_.end(),
                    [&](const auto &zone)
                    {
                        return zone.kind == ZoneKind::moonbeam && zone.caster == beam.caster;
                    });
    if (!shining || creature.life.dead || creature.life.hp == 0 ||
        detail::has_effect(creature.effects, detail::EffectKind::moonlit) || sealed_off(creature))
    {
        return;
    }
    const auto &caster = actor(beam.caster);
    const unsigned rest_of_turn = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
    if (detail::can_apply(creature.effects))
    {
        detail::apply_poisoned(creature.effects, effect_source(caster), caster.source.name,
                               rest_of_turn, detail::EffectKind::moonlit);
    }
    const auto &spell = *detail::find_spell("moonbeam");
    const bool saved = saving_throw_succeeds(creature, spell.save, spell_dc(caster), false,
                                             heightened_against(creature, caster, spell.id));
    // 1d10 more for each slot level above 2.
    const int rolled =
        dice({spell.dice.count + static_cast<int>(beam.slot - spell.level), spell.dice.sides, 0});
    const int amount = resolved_damage(creature, spell.damage, saved ? rolled / 2 : rolled);
    log(creature.source.name + " takes " + std::to_string(amount) +
            " Radiant damage from the Moonbeam.",
        {"{name} takes {damage} Radiant damage from the Moonbeam.",
         {{"name", creature.source.name}, {"damage", std::to_string(amount)}}});
    damage(creature, amount);
}

void Session::spikes_pierce(Actor &creature)
{
    const int amount = resolved_damage(creature, detail::DamageType::piercing, dice({2, 4, 0}));
    log(creature.source.name + " takes " + std::to_string(amount) +
            " Piercing damage from the spikes.",
        {"{name} takes {damage} Piercing damage from the spikes.",
         {{"name", creature.source.name}, {"damage", std::to_string(amount)}}});
    damage(creature, amount);
}

bool Session::in_zone(const Zone &zone, Cell cell) noexcept
{
    return std::find(zone.cells.begin(), zone.cells.end(), cell) != zone.cells.end();
}

bool Session::in_zone(const Zone &zone, const Actor &a)
{
    const auto squares = squares_of(a);
    return std::any_of(squares.begin(), squares.end(),
                       [&](Cell square)
                       {
                           return in_zone(zone, square);
                       });
}

bool Session::spring_zone(const Zone &zone, Actor &creature)
{
    // Grease knocks a creature Prone and Web Restrains it, each on a failed
    // Dexterity save against the caster's spell DC.
    const bool grease = zone.kind == ZoneKind::grease;
    if (creature.life.dead || creature.life.hp == 0 || (!grease && def(creature).web_walker) ||
        (grease ? creature.effects.prone
                : detail::has_effect(creature.effects, detail::EffectKind::webbed) ||
                      !detail::can_apply(creature.effects)))
    {
        return false;
    }
    // Grease outlasts its caster, so its mark is found by the caster's id.
    const bool heightened = detail::heightened_against(creature.effects, scope_, zone.caster,
                                                       grease ? "grease" : "web");
    if (saving_throw_succeeds(creature, detail::Ability::dexterity, zone.save_dc, false,
                              heightened))
    {
        return false;
    }
    if (grease)
    {
        creature.effects.prone = true;
        log(creature.source.name + " slips and falls Prone.",
            {"{name} slips and falls Prone.", {{"name", creature.source.name}}});
        return true;
    }
    // Web needs Concentration, so its caster is still on the field.
    const auto &caster = actor(zone.caster);
    detail::apply_entangle(creature.effects, effect_source(caster), caster.source.name,
                           zone.save_dc, detail::EffectKind::webbed);
    log(creature.source.name + " is Restrained by the webs.",
        {"{name} is Restrained by the webs.", {{"name", creature.source.name}}});
    return true;
}

void Session::spring_zones(Actor &creature, ZoneKind kind)
{
    // Copies: a save can end a caster's Concentration and with it the zone.
    const auto zones = zones_;
    for (const auto &zone : zones)
    {
        if (zone.kind == kind && in_zone(zone, creature))
        {
            (void)spring_zone(zone, creature);
        }
    }
}

void Session::ice_burst(Actor &caster, const Actor &target, unsigned steps)
{
    // The target and each creature within 5 feet of it save against 2d6 Cold,
    // 1d6 more for each slot level above 1.
    const int total = dice({2 + static_cast<int>(steps), 6, 0});
    const int dc = spell_dc(caster, "ice_knife");
    for (auto &other : actors_)
    {
        if (other.life.dead || distance(target, other) > 5)
        {
            continue;
        }
        if (saving_throw_succeeds(other, detail::Ability::dexterity, dc))
        {
            continue;
        }
        const int amount = resolved_damage(other, detail::DamageType::cold, total);
        log(other.source.name + " takes " + std::to_string(amount) + " Cold damage.",
            {"{name} takes {damage} {type} damage.",
             {{"name", other.source.name},
              {"damage", std::to_string(amount)},
              {"type", "Cold", true}}});
        damage(other, amount);
    }
}

void Session::push_away(const Actor &from, Actor &target, int squares)
{
    // Straight away from `from`, centre from centre: doubled, a footprint's
    // centre falls on a square.
    const auto centre = [](const Actor &a)
    {
        const auto size = footprint(a);
        return Cell{2 * a.source.cell.x + size.width - 1, 2 * a.source.cell.y + size.height - 1};
    };
    const auto away = centre(target), toward = centre(from);
    const int sx = (away.x > toward.x) - (away.x < toward.x);
    const int sy = (away.y > toward.y) - (away.y < toward.y);
    push_along(target, Cell{sx, sy}, squares);
}

void Session::push_along(Actor &target, Cell step, int squares)
{
    // Square by square, while the way is open.
    const int sx = step.x, sy = step.y;
    int moved = 0;
    for (; moved < squares && (sx || sy); ++moved)
    {
        const Cell next{target.source.cell.x + sx, target.source.cell.y + sy};
        // A Wall of Force or of Stone stops a push as the map's walls do.
        if (blocked_for(target, next, walled_board(true)))
        {
            break;
        }
        target.source.cell = next;
    }
    if (moved)
    {
        log(target.source.name + " is pushed " + std::to_string(5 * moved) + " feet.",
            {"{name} is pushed {feet} feet.",
             {{"name", target.source.name}, {"feet", std::to_string(5 * moved)}}});
    }
    // The battlefield's edge is open ground beyond, not a wall.
    const auto beyond =
        squares_of({target.source.cell.x + sx, target.source.cell.y + sy}, footprint(target));
    if (moved < squares && (sx || sy) &&
        std::all_of(
            beyond.begin(), beyond.end(),
            [&](Cell square)
            {
                return board_.contains(square);
            }))
    {
        slam(target, 5 * (squares - moved));
    }
}

// Tsunami: at the start of its caster's turns the wall moves 50 feet away,
// carrying the creatures in it or in its way, who save against its damage;
// each move deals 1d10 less (LEVELS-5).
void Session::move_wave(EntityId caster)
{
    const auto found = std::find_if(zones_.begin(), zones_.end(),
                                    [&](const auto &zone)
                                    {
                                        return zone.kind == ZoneKind::wave && zone.caster == caster;
                                    });
    if (found == zones_.end())
    {
        return;
    }
    const auto before = *found;
    const auto heading = wave_headings[static_cast<std::size_t>(before.dealt)];
    constexpr int squares = 10;
    const auto shifted = [&](Cell cell, int k)
    {
        return Cell{cell.x + k * heading.x, cell.y + k * heading.y};
    };
    std::vector<Cell> swept, ahead;
    for (int k = 0; k <= squares; ++k)
    {
        for (const auto cell : before.cells)
        {
            if (const auto moved = shifted(cell, k); board_.contains(moved))
            {
                if (std::find(swept.begin(), swept.end(), moved) == swept.end())
                {
                    swept.push_back(moved);
                }
                if (k == squares)
                {
                    ahead.push_back(moved);
                }
            }
        }
    }
    found->cells = ahead;
    --found->slot;
    log("The tsunami surges forward.", {"The tsunami surges forward.", {}});
    // The foremost first, so no one carried blocks another.
    std::vector<EntityId> caught;
    for (const auto &other : actors_)
    {
        if (!other.life.dead && !absent(other) && !sealed_off(other) && touches(swept, other))
        {
            caught.push_back(other.source.id);
        }
    }
    std::stable_sort(caught.begin(), caught.end(),
                     [&](EntityId left, EntityId right)
                     {
                         const auto along = [&](EntityId id)
                         {
                             const auto cell = actor(id).source.cell;
                             return cell.x * heading.x + cell.y * heading.y;
                         };
                         return along(left) > along(right);
                     });
    for (const auto id : caught)
    {
        auto &other = actor(id);
        if (!saving_throw_succeeds(other, detail::Ability::strength, before.save_dc))
        {
            const int amount = resolved_damage(other, detail::DamageType::bludgeoning,
                                               dice({static_cast<int>(before.slot), 10, 0}));
            log(other.source.name + " takes " + std::to_string(amount) +
                    " Bludgeoning damage from the tsunami.",
                {"{name} takes {damage} Bludgeoning damage from the tsunami.",
                 {{"name", other.source.name}, {"damage", std::to_string(amount)}}});
            damage(other, amount);
        }
        // Carried until it is in the wall's new place, or the way is shut.
        int carry = 0;
        while (carry < squares && std::find(ahead.begin(), ahead.end(),
                                            shifted(other.source.cell, carry)) == ahead.end())
        {
            ++carry;
        }
        if (!other.life.dead && carry)
        {
            push_along(other, heading, carry);
        }
    }
    // Off the field, the wave is spent.
    if (ahead.empty())
    {
        end_concentration(actor(caster));
    }
}

// Tsunami: a creature in the wave moves only by swimming, after a Strength
// (Athletics) check against the spell's DC, once a turn; a failure holds it
// for the turn.
bool Session::held_by_wave(Actor &a)
{
    const auto wave = std::find_if(zones_.begin(), zones_.end(),
                                   [&](const auto &zone)
                                   {
                                       return zone.kind == ZoneKind::wave && in_zone(zone, a);
                                   });
    if (wave == zones_.end() || detail::has_effect(a.effects, detail::EffectKind::swum))
    {
        return false;
    }
    const int dc = wave->save_dc;
    const auto check = roll_athletics(a, dc);
    const std::string advantage_label = check.advantage ? " (advantage)" : "";
    log(a.source.name + " Athletics check: d20 " + std::to_string(check.natural) + advantage_label +
            " + " + std::to_string(check.bonus) + " vs DC " + std::to_string(dc) +
            (check.success ? " (swims)." : " (held by the wave)."),
        {check.success
             ? "{name} Athletics check: d20 {roll}{advantage} + {bonus} vs DC {dc} (swims)."
             : "{name} Athletics check: d20 {roll}{advantage} + {bonus} vs DC {dc} (held by the wave).",
         {{"name", a.source.name},
          {"roll", std::to_string(check.natural)},
          {"advantage", advantage_label, true},
          {"bonus", std::to_string(check.bonus)},
          {"dc", std::to_string(dc)}}});
    if (!check.success && !rescue_athletics(a, check, dc))
    {
        a.movement = 0;
        a.dashes = 0;
        return true;
    }
    const unsigned rest_of_turn = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
    if (detail::can_apply(a.effects))
    {
        detail::apply_poisoned(a.effects, effect_source(a), a.source.name, rest_of_turn,
                               detail::EffectKind::swum);
    }
    return false;
}

bool Session::blocks_push(Cell cell, EntityId pushed) const
{
    return board_.contains(cell) && (board_.at(cell) == Terrain::obstacle ||
                                     std::any_of(actors_.begin(), actors_.end(),
                                                 [&](const auto &other)
                                                 {
                                                     return !other.life.dead &&
                                                            other.source.id != pushed &&
                                                            occupies(other, cell);
                                                 }));
}

void Session::slam(Actor &target, int short_feet)
{
    if (short_feet <= 0 || target.life.dead)
    {
        return;
    }
    const int dice = std::min(3, (short_feet + 9) / 10);
    int rolled = 0;
    for (int n = 0; n < dice; ++n)
    {
        rolled += roll(6);
    }
    const int amount = resolved_damage(target, detail::DamageType::bludgeoning, rolled);
    log(target.source.name + " slams into an obstacle and takes " + std::to_string(amount) +
            " Bludgeoning damage.",
        {"{name} slams into an obstacle and takes {damage} Bludgeoning damage.",
         {{"name", target.source.name}, {"damage", std::to_string(amount)}}});
    damage(target, amount);
}

bool Session::provokes(const Actor &other, const Actor &moving, Cell from, Cell to) const
{
    // A dominated creature reacts only with its controller's Reaction too.
    const auto *boss = controller(other);
    return other.source.side != moving.source.side && !def(moving).flyby && conscious(other) &&
           other.reaction && (!boss || (boss->reaction && conscious(*boss))) &&
           !detail::has_effect(other.effects, detail::EffectKind::turned) &&
           !detail::incapacitated(other.effects) && !detail::opportunity_blocked(other.effects) &&
           !charmed_by(other, moving.source.id) &&
           has_weapon_reaction(other, from, to, footprint(moving)) && can_see(other, moving);
}

std::vector<Cell> Session::flight_route(const Actor &from, const Actor &target, int budget) const
{
    // Squares are searched by feet spent, keeping for each square and spend
    // the route with the fewest Opportunity Attacks provoked.
    const auto grid = movement_grid(target);
    const int width = board_.width, cells = board_.width * board_.height, spends = budget / 5 + 1;
    constexpr int unreached = std::numeric_limits<int>::max();

    struct Visit
    {
        // Spelled out: MSVC cannot default-construct a local class whose
        // member initializer names a local of the enclosing function.
        int provoked = std::numeric_limits<int>::max();
        int previous = -1;
    };

    std::vector<Visit> visits(static_cast<std::size_t>(cells * spends));
    const auto at = [&](Cell c, int feet)
    {
        return static_cast<std::size_t>((c.y * width + c.x) * spends + feet / 5);
    };
    const auto cell_of = [&](std::size_t visit)
    {
        const int square = static_cast<int>(visit) / spends;
        return Cell{square % width, square / width};
    };
    visits[at(target.source.cell, 0)].provoked = 0;
    for (int feet = 0; feet < budget; feet += 5)
    {
        for (int square = 0; square < cells; ++square)
        {
            const Cell here{square % width, square / width};
            const auto &visit = visits[at(here, feet)];
            if (visit.provoked == unreached)
            {
                continue;
            }
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const Cell next{here.x + dx, here.y + dy};
                    const auto cost = (dx || dy) && board_.contains(next)
                                          ? grid.step_cost(here, next)
                                          : std::nullopt;
                    if (!cost || feet + *cost > budget)
                    {
                        continue;
                    }
                    const auto provoked = static_cast<int>(
                        std::count_if(actors_.begin(), actors_.end(),
                                      [&](const auto &other)
                                      {
                                          return provokes(other, target, here, next);
                                      }));
                    auto &reached = visits[at(next, feet + *cost)];
                    if (visit.provoked + provoked < reached.provoked)
                    {
                        reached = {visit.provoked + provoked, static_cast<int>(at(here, feet))};
                    }
                }
            }
        }
    }
    // The farthest square it may stop on, then the safest route, then the shortest.
    std::size_t best = at(target.source.cell, 0);
    const auto better = [&](std::size_t candidate)
    {
        const auto a = cell_of(candidate), b = cell_of(best);
        const int far = distance(from, a), best_far = distance(from, b);
        if (far != best_far)
        {
            return far > best_far;
        }
        return visits[candidate].provoked < visits[best].provoked;
    };
    for (std::size_t visit = 0; visit < visits.size(); ++visit)
    {
        if (visits[visit].provoked != unreached && grid.can_stop_at(cell_of(visit)) &&
            better(visit))
        {
            best = visit;
        }
    }
    std::vector<Cell> route;
    for (auto visit = best; visits[visit].previous >= 0;
         visit = static_cast<std::size_t>(visits[visit].previous))
    {
        route.push_back(cell_of(visit));
    }
    std::reverse(route.begin(), route.end());
    return route;
}
} // namespace opengold::srd5::detail
