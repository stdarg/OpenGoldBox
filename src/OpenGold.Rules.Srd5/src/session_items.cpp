#include "session.h"
#include <stdexcept>

// Session: items and weapons in hand, morale and flight, thrown gear.
namespace opengold::srd5::detail
{
using namespace rules;

unsigned Session::held_weapon(const Actor &a) const
{
    const auto eligible = [&](const auto &item)
    {
        return item.holder == a.source.id && !item.stowed && detail::weapon(item.definition);
    };
    if (a.selected_weapon)
    {
        for (const auto &item : items_)
        {
            if (item.id == a.selected_weapon && eligible(item))
            {
                return item.id;
            }
        }
    }
    for (const auto &item : items_)
    {
        if (eligible(item))
        {
            return item.id;
        }
    }
    return 0;
}

void Session::activate_light()
{
    if (light_active_)
    {
        return;
    }
    physical_inventory_ = true;
    initialize_items();
    const auto count = items_.size();
    for (unsigned i = 0; i < count; ++i)
    {
        if (items_[i].holder && !items_[i].stowed && items_[i].quantity > 1)
        {
            auto carried = items_[i];
            carried.id = static_cast<unsigned>(items_.size() + 1);
            --carried.quantity;
            carried.stowed = true;
            items_[i].quantity = 1;
            items_.push_back(std::move(carried));
        }
    }
    light_active_ = true;
}

void Session::open_hand(const Actor &monk, Actor &target, std::string_view verb)
{
    // Open Hand Technique on a Flurry hit: Addle stops Opportunity Attacks
    // until the target's next turn; Push (Strength save) shoves it 15 feet;
    // Topple (Dexterity save) knocks it Prone.
    if (verb == "flurry_addle")
    {
        detail::apply_poisoned(target.effects, effect_source(monk), monk.source.name,
                               next_turn_ms(target), detail::EffectKind::addled);
        log(target.source.name + " is addled.",
            {"{name} is addled.", {{"name", target.source.name}}});
    }
    else if (verb == "flurry_push" &&
             !saving_throw_succeeds(target, detail::Ability::strength, def(monk).focus_dc))
    {
        push_away(monk, target, 3);
    }
    else if (verb == "flurry_topple" && !target.effects.prone &&
             !saving_throw_succeeds(target, detail::Ability::dexterity, def(monk).focus_dc))
    {
        target.effects.prone = true;
        log(target.source.name + " is knocked Prone.",
            {"{name} is knocked Prone.", {{"name", target.source.name}}});
    }
}

void Session::ready_metamagic(Actor &a, std::string_view choice)
{
    // A second option joins the first during Sorcery Incarnate.
    if (choice == "cancel" || readied_metamagic(a).size() >= metamagic_room(a))
    {
        std::erase_if(a.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::metamagic;
                      });
    }
    if (choice == "cancel")
    {
        return;
    }
    int value = 0;
    std::string label;
    if (choice.starts_with("transmuted_"))
    {
        const auto type = choice.substr(11);
        value = 10 +
                static_cast<int>(std::find(transmuted_types.begin(), transmuted_types.end(), type) -
                                 transmuted_types.begin());
        label = "Transmuted Spell";
    }
    else
    {
        value = static_cast<int>(std::find(metamagic_ids.begin(), metamagic_ids.end(), choice) -
                                 metamagic_ids.begin());
        label = metamagic_labels[static_cast<std::size_t>(value)];
    }
    detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                detail::EffectKind::metamagic, value);
    log(a.source.name + " readies " + label + ".",
        {"{name} readies {feature}.", {{"name", a.source.name}, {"feature", label, true}}});
}

void Session::use_font_of_magic(Actor &a, std::string_view verb)
{
    const auto slot = static_cast<unsigned>(verb.back() - '0');
    if (verb.starts_with("create_slot_"))
    {
        a.bonus = false;
        a.lay_on_hands[CharacterClass::sorcerer] -= font_of_magic_cost[slot];
        ++a.slots[slot];
    }
    else
    {
        --a.slots[slot];
        a.lay_on_hands[CharacterClass::sorcerer] =
            std::min(def(a).sorcery_points,
                     a.lay_on_hands[CharacterClass::sorcerer] + static_cast<int>(slot));
    }
    const std::string level(1, verb.back());
    log(a.source.name + (verb.starts_with("create_slot_")
                             ? " creates a level-" + level + " spell slot."
                             : " turns a level-" + level + " spell slot into Sorcery Points."),
        {verb.starts_with("create_slot_")
             ? "{name} creates a level-{level} spell slot."
             : "{name} turns a level-{level} spell slot into Sorcery Points.",
         {{"name", a.source.name}, {"level", level}}});
}

void Session::use_focus_movement(Actor &a, std::string_view verb, EntityId carried)
{
    // Disengage with Patient Defense, Dash with Step of the Wind; a Focus Point
    // adds Dodge or Disengage.
    const bool focus = verb.ends_with("_focus") || verb == "step_of_the_wind_carry";
    const bool patient = verb.starts_with("patient_defense");
    a.bonus = false;
    if (focus)
    {
        --a.surges[CharacterClass::monk];
    }
    if (patient || focus)
    {
        a.disengaged = true;
    }
    if (patient && focus)
    {
        a.dodge = true;
    }
    // Heightened Focus: Patient Defense with Focus gives two Martial Arts dice
    // of Temporary Hit Points.
    if (patient && focus && def(a).heightened_focus)
    {
        if (const int amount = roll(def(a).martial_die) + roll(def(a).martial_die);
            amount > a.life.temporary_hp.amount)
        {
            detail::grant_temporary_hp(a.life, {amount, "feature:heightened_focus"},
                                       TemporaryHpChoice::use_new);
        }
    }
    if (!patient)
    {
        a.fleet_step = false;
    }
    if (!patient)
    {
        a.movement += def(a).speed;
        ++a.dashes;
    }
    const std::string name = patient ? "Patient Defense" : "Step of the Wind";
    log(a.source.name + " uses " + name + ".",
        {"{name} uses {feature}.", {{"name", a.source.name}, {"feature", name, true}}});
    // Heightened Focus: the ally moves with the Monk for the rest of the turn.
    if (carried)
    {
        a.carried = carried;
        log(a.source.name + " carries " + actor(carried).source.name + " (Step of the Wind).",
            {"{name} carries {target} (Step of the Wind).",
             {{"name", a.source.name}, {"target", actor(carried).source.name}}});
    }
}

Loadout Session::unarmed_loadout(const Actor &a) const
{
    // A Monk's Unarmed Strike: the Martial Arts die plus its modifier, with no
    // weapon's mastery, grip or feat riders.
    Loadout result{a.definition, a.selected_weapon, a.light_damage};
    auto &d = result.definition;
    d.melee = {1, d.martial_die, d.martial_modifier};
    d.melee_bonus = 2 + (d.level - 1) / 4 + d.martial_modifier;
    d.melee_ability = d.martial_modifier;
    d.melee_type =
        d.empowered_strikes ? detail::DamageType::force : detail::DamageType::bludgeoning;
    d.reach = 5;
    d.versatile_sides = 0;
    d.masteries.clear();
    d.savage = d.great_weapon_fighting = false;
    return result;
}

Loadout Session::item_loadout(const Actor &a, unsigned token) const
{
    const auto &item = items_.at(token - 1);
    if (item.holder != a.source.id || item.stowed || !detail::weapon(item.definition))
    {
        throw std::runtime_error("Weapon is not held");
    }
    return {equipped_definition(a, items_, token), token, a.light_damage};
}

bool Session::has_nick(const Actor &a) const
{
    return std::any_of(def(a).masteries.begin(), def(a).masteries.end(),
                       [](const auto &key)
                       {
                           const auto *w = detail::weapon(key);
                           return w && w->mastery == detail::Mastery::nick;
                       });
}

bool Session::nick_weapon(const Actor &a, unsigned token) const
{
    if (!token || token > items_.size())
    {
        return false;
    }
    const auto &key = items_[token - 1].definition;
    const auto *w = detail::weapon(key);
    return w && w->mastery == detail::Mastery::nick &&
           std::find(def(a).masteries.begin(), def(a).masteries.end(), w->key) !=
               def(a).masteries.end();
}

bool Session::light_eligible(const Actor &a, unsigned token) const
{
    if (!token || token > items_.size())
    {
        return false;
    }
    const auto *weapon = detail::weapon(items_[token - 1].definition);
    return weapon && weapon->light &&
           std::any_of(a.light_origins.begin(), a.light_origins.end(),
                       [&](auto prior)
                       {
                           return prior != token;
                       });
}

void Session::qualify_light(Actor &a, unsigned token)
{
    if (!token)
    {
        return;
    }
    const auto *weapon = detail::weapon(items_.at(token - 1).definition);
    if (nick_active_ && weapon && weapon->light)
    {
        a.nick_origin = token;
    }
    if (weapon && weapon->light &&
        std::find(a.light_origins.begin(), a.light_origins.end(), token) == a.light_origins.end())
    {
        a.light_origins.push_back(token);
    }
}

bool Session::has_weapon_reaction(const Actor &a, Cell from, Cell to, Footprint size) const
{
    if (aloft(a))
    {
        return false;
    }
    const auto reaches = [&](int reach)
    {
        return distance(a, from, size) <= reach && distance(a, to, size) > reach;
    };
    if (reaches(def(a).reach))
    {
        return true;
    }
    for (const auto &item : items_)
    {
        if (item.holder == a.source.id && !item.stowed)
        {
            if (const auto *w = detail::weapon(item.definition); w && reaches(melee_reach))
            {
                return true;
            }
        }
    }
    return false;
}

bool Session::weapon_reaction(const Actor &a, const Definition &weapon) const
{
    if (!pending() || path_index_ >= path_.size())
    {
        return false;
    }
    return distance(a, mover()) <= weapon.reach &&
           distance(a, path_[path_index_], footprint(mover())) > weapon.reach;
}

void Session::validate_light() const
{
    if (nick_active_ && (!light_active_ || std::none_of(actors_.begin(), actors_.end(),
                                                        [&](const auto &a)
                                                        {
                                                            return has_nick(a);
                                                        })))
    {
        throw std::runtime_error("Nick requires an actual mastery entitlement");
    }
    if (light_active_ && (!physical_inventory_ || !items_active_))
    {
        throw std::runtime_error("Light attacks require physical weapon identities");
    }
    for (const auto &a : actors_)
    {
        if ((!nick_active_ && a.nick_origin) ||
            (a.nick_origin && std::find(a.light_origins.begin(), a.light_origins.end(),
                                        a.nick_origin) == a.light_origins.end()))
        {
            throw std::runtime_error("Invalid Nick Attack action origin");
        }
        if (a.light_extra > 2 || (!nick_active_ && a.light_extra) ||
            (a.light_extra &&
             (a.light_origins.empty() || a.source.id != actors_[turn_].source.id)) ||
            (a.light_extra == 1 && a.bonus) || (a.light_extra == 2 && !has_nick(a)))
        {
            throw std::runtime_error("Invalid shared Light/Nick expenditure");
        }
        if (!light_active_ &&
            (a.selected_weapon || !a.light_origins.empty() || a.definition.two_weapon_fighting))
        {
            throw std::runtime_error("Missing Light attack checkpoint state");
        }
        if (a.selected_weapon &&
            (a.selected_weapon > items_.size() || held_weapon(a) != a.selected_weapon))
        {
            throw std::runtime_error("Invalid selected weapon");
        }
        if (!a.light_origins.empty() && (a.source.id != actors_[turn_].source.id ||
                                         (a.actions.normal && (!a.surge_used || a.actions.surge))))
        {
            throw std::runtime_error("Light attack lacks an Attack action");
        }
        if (a.light_origins.size() > 1 && (!a.surge_used || a.actions.normal || a.actions.surge))
        {
            throw std::runtime_error("Too many qualifying Attack actions");
        }
        std::set<unsigned> seen;
        for (auto id : a.light_origins)
        {
            if (!id || id > items_.size() || !seen.insert(id).second)
            {
                throw std::runtime_error("Invalid qualifying weapon identity");
            }
            const auto *w = detail::weapon(items_[id - 1].definition);
            if (!w || !w->light)
            {
                throw std::runtime_error("Qualifying weapon is not Light");
            }
        }
    }
}

void Session::initialize_items()
{
    if (items_active_)
    {
        return;
    }
    std::vector<const Actor *> ordered;
    for (const auto &a : actors_)
    {
        ordered.push_back(&a);
    }
    std::sort(ordered.begin(), ordered.end(),
              [](auto a, auto b)
              {
                  return a->source.id < b->source.id;
              });
    for (const auto *a : ordered)
    {
        std::set<std::uint64_t> ids;
        for (const auto &source : a->source.inventory)
        {
            if (!source.inventory_id || !source.quantity || !ids.insert(source.inventory_id).second)
            {
                throw std::runtime_error("Invalid carried item source");
            }
        }
        for (unsigned i = 0; i < a->definition.equipment_keys.size(); ++i)
        {
            const auto &key = a->definition.equipment_keys[i];
            if (!detail::is_shield(key) && !detail::weapon(key))
            {
                continue;
            }
            HeldItemView item{static_cast<unsigned>(items_.size() + 1),
                              a->source.id,
                              a->source.id,
                              i,
                              key,
                              {weapon_label(key), {}}};
            if (physical_inventory_)
            {
                for (const auto &source : a->source.inventory)
                {
                    if (source.equipment_index == static_cast<int>(i))
                    {
                        if (source.definition != key || item.inventory_id)
                        {
                            throw std::runtime_error("Carried item disagrees with equipped source");
                        }
                        item.inventory_id = source.inventory_id;
                        item.quantity = source.quantity;
                    }
                }
            }
            items_.push_back(std::move(item));
        }
        if (physical_inventory_)
        {
            for (const auto &source : a->source.inventory)
            {
                if (source.equipment_index < 0 && detail::weapon(source.definition))
                {
                    items_.push_back({static_cast<unsigned>(items_.size() + 1),
                                      a->source.id,
                                      a->source.id,
                                      0,
                                      source.definition,
                                      {weapon_label(source.definition), {}},
                                      source.inventory_id,
                                      source.quantity,
                                      true});
                }
            }
        }
    }
    items_active_ = true;
}

Definition Session::equipped_definition(const Actor &a, const std::vector<HeldItemView> &items,
                                        unsigned selected) const
{
    if (a.source.character_profile.empty())
    {
        throw std::runtime_error("Equipment exchange requires a character equipment profile");
    }
    if (!selected)
    {
        selected = a.selected_weapon;
    }
    std::vector<std::string> keys;
    for (const auto &key : profiles_->of(a.source.character_profile).equipment_keys)
    {
        if (!detail::is_shield(key) && !detail::weapon(key))
        {
            keys.push_back(key);
        }
    }
    if (selected)
    {
        for (const auto &item : items)
        {
            if (item.id == selected && item.holder == a.source.id && !item.stowed)
            {
                keys.push_back(item.definition);
            }
        }
    }
    for (const auto &item : items)
    {
        if (item.holder == a.source.id && !item.stowed && item.id != selected &&
            !(a.shield_off && detail::is_shield(item.definition)))
        {
            keys.push_back(item.definition);
        }
    }
    return profiles_->of(a.source.character_profile, std::span<const std::string>(keys));
}

Loadout Session::thrown_loadout(const Actor &a, std::string_view weapon) const
{
    std::vector<std::string> keys;
    for (const auto &key : a.definition.equipment_keys)
    {
        if (!detail::weapon(key))
        {
            keys.push_back(key);
        }
    }
    keys.emplace_back(weapon);
    return {profiles_->of(a.source.character_profile, std::span<const std::string>(keys)),
            a.selected_weapon, a.light_damage};
}

bool Session::carries(const Actor &a, std::string_view key) const
{
    const auto &keys = def(a).equipment_keys;
    return !a.source.character_profile.empty() &&
           std::find(keys.begin(), keys.end(), key) == keys.end() &&
           std::any_of(a.source.inventory.begin(), a.source.inventory.end(),
                       [&](const auto &item)
                       {
                           return item.definition == key && item.quantity;
                       });
}

rules::Missile Session::ranged_missile(const Actor &a) const
{
    const auto &d = def(a);
    if (!d.range)
    {
        const auto bow = carried_ranged_weapon(a);
        return bow.empty() ? rules::Missile::none : weapon_missile(*detail::weapon(bow));
    }
    // A stat block's weapon without an item: its short range is a thrown one's.
    return d.ranged_missile.value_or(d.range <= 30 ? rules::Missile::axe : rules::Missile::arrow);
}

std::string_view Session::carried_ranged_weapon(const Actor &a) const
{
    if (a.source.character_profile.empty() || def(a).range)
    {
        return {};
    }
    for (const auto &item : a.source.inventory)
    {
        if (const auto *w = detail::weapon(item.definition);
            w && w->ranged && item.quantity && (w->hands == 1 || !wears_shield(def(a))))
        {
            // The whole key, so a Short Bow +1 is drawn with its enchantment.
            return item.definition;
        }
    }
    return {};
}

std::string Session::weapon_in_hand(const Actor &a) const
{
    if (a.source.character_profile.empty())
    {
        return {};
    }
    if (!a.drawn_weapon.empty())
    {
        return a.drawn_weapon;
    }
    // A Definition lists the weapon held first, as equipped_definition builds it.
    const auto &keys = def(a).equipment_keys;
    const auto held = std::find_if(keys.begin(), keys.end(),
                                   [](const auto &key)
                                   {
                                       return detail::weapon(key) != nullptr;
                                   });
    return held == keys.end() ? std::string{} : *held;
}

// An attack puts its weapon in the creature's hand, whoever chose it.
void Session::take_up_weapon(Actor &a, const Command &command) const
{
    const std::string_view verb = command.verb;
    if (verb == "melee" || verb == "opportunity" || verb.starts_with("melee_") ||
        verb == "light_melee" || verb == "nick_melee")
    {
        a.drawn_weapon.clear();
        a.weapon_set_shown = rules::WeaponSetShown::melee;
        return;
    }
    const bool fires = verb == "ranged" || verb.starts_with("ranged_");
    const bool item_fired = verb == "light_ranged" || verb == "nick_ranged";
    if (!fires && !item_fired && verb != "shoot" && !verb.starts_with("throw") &&
        !(verb.ends_with("_item") && !verb.starts_with("potion")) && verb != "javelin_lightning")
    {
        return;
    }
    a.weapon_set_shown = rules::WeaponSetShown::ranged;
    if (a.source.character_profile.empty())
    {
        return;
    }
    if (verb == "shoot")
    {
        a.drawn_weapon = carried_ranged_weapon(a);
    }
    else if (item_fired)
    {
        a.drawn_weapon = items_.at(command.item - 1).definition;
    }
    else if (fires)
    {
        // A thrown weapon leaves the hand; a bow stays in it.
        for (const auto &key : def(a).equipment_keys)
        {
            const auto *weapon = detail::weapon(key);
            if (weapon && weapon->ranged)
            {
                a.drawn_weapon = key;
                break;
            }
        }
    }
}

bool Session::shield_blocks_bow(const Actor &a) const
{
    if (a.source.character_profile.empty())
    {
        return a.source.holds_shield && !a.shield_off && def(a).ranged_two_handed;
    }
    if (a.shield_off || !wears_shield(def(a)) || !carried_ranged_weapon(a).empty())
    {
        return false;
    }
    auto unshielded = a;
    unshielded.definition = without_shield(a.source.character_profile, a.definition);
    return !carried_ranged_weapon(unshielded).empty();
}

// The original's morale test. A creature with morale of its own holds while
// it has lost no more of its Hit Points (in percent) than that morale; one
// without, or one that loses it, holds while its side keeps at least 100 less
// the encounter's morale percent of its Hit Points (in steps of 5). Broken, it
// flees in panic when no opponent is faster; otherwise one with Intelligence
// above 5 surrenders, and the rest fight on. It is tested again every turn.
// Charging in panic (MORALE-2), it only stops if it rallies.
void Session::check_morale(Actor &a)
{
    const int lost = 100 - 100 * a.life.hp / std::max(1, max_hp(a));
    bool holds = a.source.morale > 0 && static_cast<int>(a.source.morale) >= lost;
    if (!holds)
    {
        int remaining = 0, total = 0;
        for (const auto &other : actors_)
        {
            if (other.source.side == a.source.side)
            {
                total += max_hp(other);
                if (!other.life.dead && !other.fled)
                {
                    remaining += other.life.hp;
                }
            }
        }
        for (const auto &gone : fled_)
        {
            if (gone.source.side == a.source.side)
            {
                total += gone.max_hp;
            }
        }
        const int percent = total ? remaining * 100 / total / 5 * 5 : 0;
        holds = percent > 0 && percent >= 100 - static_cast<int>(morale_);
    }
    if (holds)
    {
        a.panicked = false;
        a.must_stay = false;
        return;
    }
    if (a.must_stay)
    {
        return;
    }
    const bool outpaced = std::any_of(actors_.begin(), actors_.end(),
                                      [&](const auto &other)
                                      {
                                          return other.source.side != a.source.side &&
                                                 other.life.hp > 0 && !other.life.dead &&
                                                 !other.fled &&
                                                 current_speed(other) > current_speed(a);
                                      });
    if (!outpaced)
    {
        note_flight(a);
        return;
    }
    a.panicked = false;
    if (a.source.intelligence > 5)
    {
        end_concentration(a);
        a.surrendered = true;
        a.fled = true;
        log(a.source.name + " surrenders.", {"{name} surrenders.", {{"name", a.source.name}}});
    }
}

// A broken monster's flight, turn by turn. Two of its turns gone by with the edge
// no nearer, whether walled in or failing to get off the field, and it stops
// trying and charges (MORALE-2), so that no fight is left standing still.
void Session::note_flight(Actor &a)
{
    const int to_edge = squares_to_edge(a);
    if (!a.panicked)
    {
        log(a.source.name + " flees in panic.",
            {"{name} flees in panic.", {{"name", a.source.name}}});
        a.panicked = true;
        a.closest_to_edge = to_edge;
        a.turns_stuck = 0;
        return;
    }
    if (to_edge < a.closest_to_edge)
    {
        a.closest_to_edge = to_edge;
        a.turns_stuck = 0;
        return;
    }
    if (++a.turns_stuck >= panic_charge_after_turns)
    {
        a.must_stay = true;
        log(a.source.name + " cannot get away and charges in panic!",
            {"{name} cannot get away and charges in panic!", {{"name", a.source.name}}});
    }
}

int Session::squares_to_edge(const Actor &a) const
{
    int nearest = std::max(board_.width, board_.height);
    for (const auto square : squares_of(a))
    {
        nearest = std::min({nearest, square.x, square.y, board_.width - 1 - square.x,
                            board_.height - 1 - square.y});
    }
    return nearest;
}

int Session::current_speed(const Actor &a) const
{
    return std::max(0, def(a).speed - detail::speed_penalty(a.effects));
}

bool Session::can_run_off(const Actor &a) const
{
    if ((a.source.side != Side::party && !a.panicked) || !conscious(a) || a.must_stay || a.fled ||
        actors_[turn_].source.id != a.source.id || outcome_ != Outcome::ongoing || pending() ||
        temporary_offer_ || check_choice_ || graze_ || effect_waiting() || champion_move_ ||
        movement_left(a) < 5 || off_field_steps(a).empty())
    {
        return false;
    }
    if (flight_always_succeeds(a))
    {
        return true;
    }
    for (const auto &other : actors_)
    {
        if (other.source.side != a.source.side && conscious(other) &&
            current_speed(other) > current_speed(a))
        {
            return false;
        }
    }
    return true;
}

std::vector<Cell> Session::off_field_steps(const Actor &a) const
{
    std::vector<Cell> steps;
    for (const auto step : {Cell{0, -1}, Cell{1, 0}, Cell{0, 1}, Cell{-1, 0}})
    {
        if (const Cell next{a.source.cell.x + step.x, a.source.cell.y + step.y}; off_field(a, next))
        {
            steps.push_back(next);
        }
    }
    return steps;
}

bool Session::off_field(const Actor &a, Cell anchor) const
{
    const auto squares = squares_of(anchor, footprint(a));
    return std::any_of(squares.begin(), squares.end(),
                       [&](Cell square)
                       {
                           return !board_.contains(square);
                       });
}

bool Session::flight_always_succeeds(const Actor &a) const
{
    return spares_the_party_ && a.source.side == Side::party;
}

void Session::run_off_field(Actor &a)
{
    int fastest = 0;
    for (const auto &other : actors_)
    {
        if (other.source.side != a.source.side && conscious(other))
        {
            fastest = std::max(fastest, current_speed(other));
        }
    }
    if (!flight_always_succeeds(a) &&
        (current_speed(a) < fastest || (current_speed(a) == fastest && roll(2) == 1)))
    {
        if (a.source.side == Side::party)
        {
            a.must_stay = true;
            log(a.source.name + " cannot get away and must stay.",
                {"{name} cannot get away and must stay.", {{"name", a.source.name}}});
            return;
        }
        // A broken monster may try again next turn; this turn's run is spent, and
        // this counts as the first turn it could not get away.
        a.movement = 0;
        a.closest_to_edge = squares_to_edge(a);
        log(a.source.name + " cannot get away.",
            {"{name} cannot get away.", {{"name", a.source.name}}});
        return;
    }
    end_concentration(a);
    // Wild Shape lasts until the fight ends (WILD-1); for the one who flees, it
    // has, as Animal Shapes (LEVELS-3) and Befuddlement have (LEVELS-5).
    end_wild_shape(a);
    end_animal_shape(a);
    end_befuddlement(a);
    a.fled = true;
    log(a.source.name + " flees the battle.",
        {"{name} flees the battle.", {{"name", a.source.name}}});
}

// Takes those who ran off the field out of the turn order; the turn index
// keeps pointing at the same creature.
void Session::remove_fled()
{
    for (std::size_t i = actors_.size(); i-- > 0;)
    {
        const auto &a = actors_[i];
        if (!a.fled)
        {
            continue;
        }
        // A summoned creature vanishes; it never leaves the fight as one that fled.
        if (detail::has_effect(a.effects, detail::EffectKind::summoned))
        {
            actors_.erase(actors_.begin() + static_cast<std::ptrdiff_t>(i));
            if (i < turn_)
            {
                --turn_;
            }
            else if (turn_ >= actors_.size())
            {
                turn_ = 0;
            }
            continue;
        }
        Departed gone{{}, a.surrendered ? 0 : a.life.hp, max_hp(a), armor_class(a), vitals(a)};
        gone.surrendered = a.surrendered;
        if (!a.source.character_profile.empty())
        {
            for (const auto &gear : detail::thrown_gear_items)
            {
                gone.thrown_gear_left.emplace_back(
                    std::string(gear.key),
                    a.thrown_gear_left[static_cast<std::size_t>(gear.effect)]);
            }
        }
        gone.item_uses = a.item_uses;
        // `a` is erased next, so its participant moves rather than copies.
        gone.source = std::move(a.source);
        fled_.push_back(std::move(gone));
        actors_.erase(actors_.begin() + static_cast<std::ptrdiff_t>(i));
        if (i < turn_)
        {
            --turn_;
        }
        else if (turn_ >= actors_.size())
        {
            turn_ = 0;
        }
    }
}

// A carried Torch strikes as a Simple Melee weapon for 1 Fire damage.
void Session::torch_attack(Actor &a, Actor &target)
{
    attack_as(a, thrown_loadout(a, "torch"), target, AttackRange::melee);
}

// Acid, Alchemist's Fire or Oil thrown in place of an attack: the target makes
// a Dexterity save against 8 + the thrower's Dexterity modifier and Proficiency
// Bonus (SRD 5.2.1 adventuring gear).
void Session::throw_gear(Actor &a, Actor &target, const detail::ThrownGear &gear)
{
    --a.thrown_gear_left[static_cast<std::size_t>(gear.effect)];
    const auto &d = def(a);
    const int dc = 8 + d.dexterity + 2 + (d.level - 1) / 4;
    const auto label = std::string(gear.label);
    log(a.source.name + " throws " + label + " at " + target.source.name + ".",
        {"{name} throws {item} at {target}.",
         {{"name", a.source.name}, {"item", label, true}, {"target", target.source.name}}});
    // Forcing an enemy's save on the thrower's own turn extends a Rage.
    if (target.source.side != a.source.side && actors_[turn_].source.id == a.source.id)
    {
        extend_rage(a);
    }
    if (saving_throw_succeeds(target, detail::Ability::dexterity, dc))
    {
        return;
    }
    if (gear.effect == detail::ThrownGearEffect::oil)
    {
        target.oiled_until_round = static_cast<int>(round_) + 10;
        log(target.source.name + " is covered in oil.",
            {"{name} is covered in oil.", {{"name", target.source.name}}});
        return;
    }
    const bool acid = gear.effect == detail::ThrownGearEffect::acid;
    const auto type = acid ? detail::DamageType::acid : detail::DamageType::fire;
    const int amount = resolved_damage(target, type, acid ? roll(6) + roll(6) : roll(4), !acid);
    log(target.source.name + " takes " + std::to_string(amount) + " " +
            std::string(detail::damage_name(type)) + " damage.",
        {"{name} takes {damage} {type} damage.",
         {{"name", target.source.name},
          {"damage", std::to_string(amount)},
          {"type", std::string(detail::damage_name(type)), true}}});
    damage(target, amount, false);
    if (!acid && !target.life.dead)
    {
        target.burning = true;
        log(target.source.name + " starts burning.",
            {"{name} starts burning.", {{"name", target.source.name}}});
    }
}

// Thrown weapons work like ammunition: the weapon stays where it was, held or
// carried, so throwing never changes what the thrower holds.
void Session::throw_weapon(Actor &a, Actor &target, unsigned token, bool light)
{
    auto attacker = thrown_loadout(a, items_.at(token - 1).definition);
    attacker.light_damage = light;
    attack_as(a, std::move(attacker), target, AttackRange::ranged);
    if (mastery_)
    {
        mastery_->thrown_item = token;
    }
}
} // namespace opengold::srd5::detail
