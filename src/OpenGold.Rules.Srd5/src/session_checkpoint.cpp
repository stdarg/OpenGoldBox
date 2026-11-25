#include "session.h"
#include <iomanip>
#include <istream>
#include <sstream>
#include <stdexcept>

// Session: the combat checkpoint, its restore and the validation of a
// restored fight.
namespace opengold::srd5::detail
{
using namespace rules;

std::string Session::save() const
{
    // The module owns the checkpoint format, including RNG and pending reactions.
    std::ostringstream out;
    out << "OGCOMBAT " << checkpoint_format << ' ' << std::quoted(content_->identity.module) << ' '
        << std::quoted(content_->identity.version) << ' ' << std::quoted(content_->identity.content)
        << '\n';
    out << board_.width << ' ' << board_.height << '\n';
    for (auto cell : board_.terrain)
    {
        out << static_cast<unsigned>(cell) << ' ';
    }
    out << board_.plants.size() << ' ';
    for (const bool plant : board_.plants)
    {
        out << plant;
    }
    out << '\n';
    out << rng_ << ' ' << revision_ << ' ' << turn_ << ' ' << round_ << ' '
        << static_cast<int>(outcome_) << ' ' << actors_.size() << '\n';
    for (const auto &a : actors_)
    {
        out << a.source.id << ' ' << std::quoted(a.source.definition) << ' '
            << std::quoted(a.source.name) << ' ' << static_cast<unsigned>(a.source.side) << ' '
            << a.source.cell.x << ' ' << a.source.cell.y << ' ' << a.source.footprint.width << ' '
            << a.source.footprint.height << ' ' << a.life.hp << ' ' << a.initiative << ' '
            << a.movement << ' ' << a.winds << ' ';
        for (unsigned level = 1; level <= max_slot_level; ++level)
        {
            out << a.slots[level] << ' ';
        }
        out << a.life.successes << ' ' << a.life.failures << ' ' << a.actions.normal << ' '
            << a.actions.haste << ' ' << a.bonus << ' ' << a.reaction << ' ' << a.dodge << ' '
            << a.disengaged << ' ' << a.life.stable << ' ' << a.life.dead << ' '
            << std::quoted(a.source.character_profile) << ' ' << a.spent_slot << ' '
            << a.savage_used << ' ' << a.facing_left << ' ' << a.involuntary_overlap << ' '
            << a.hit_dice << ' ' << a.life.recovery.death_save_in_ms << ' '
            << detail::encode_stable_recovery(a.life.recovery) << ' ' << a.life.temporary_hp.amount
            << ' ' << std::quoted(a.life.temporary_hp.source_id) << ' ' << a.rushes << ' '
            << a.rush_used << ' ' << a.surges << ' ' << a.surge_used << ' ' << a.actions.surge
            << ' ' << a.actions.extra_attacks << ' ' << a.actions.loading_fired << ' '
            << a.actions.exhaled << ' ' << a.dashes << ' ' << a.arcane << ' ' << a.lay_on_hands
            << ' ' << a.free_casts << ' ' << a.channel_divinity << ' ' << a.initiate_casts << ' '
            << a.innate_spent << ' ' << a.smite_target << ' ' << a.smite_critical << ' '
            << a.sneak_used << ' ' << a.aim_used << ' ' << a.aim_ready << ' ' << a.moved << ' '
            << a.attacked << ' ' << a.pounce << ' ' << a.brutal_used << ' ' << a.stun_used << ' '
            << a.fleet_step << ' ' << a.carried << ' ' << a.selected_weapon << ' '
            << a.light_origins.size();
        for (auto id : a.light_origins)
        {
            out << ' ' << id;
        }
        out << ' ' << a.light_extra << ' ' << a.nick_origin << ' ' << a.cleave_used << ' '
            << a.colossus_used << ' ' << a.horde_used << ' ' << a.horde_origin << ' ' << a.prey_used
            << ' ' << a.prey_origin << ' ' << a.turned << ' ' << a.skips_turn << ' '
            << a.strike_used << ' ' << a.smite_used << ' ' << a.drink_used << ' ' << a.hurl_used
            << ' ' << a.smite_melee << ' ' << a.resistance_used << ' ' << a.regeneration_blocked
            << ' ' << a.rises_on_round << ' ' << a.thrown_gear_left[0] << ' '
            << a.thrown_gear_left[1] << ' ' << a.thrown_gear_left[2] << ' ' << a.burning << ' '
            << a.oiled_until_round << ' ' << a.coated_until_round << ' ' << a.shield_off << ' '
            << std::quoted(a.drawn_weapon) << ' ' << static_cast<int>(a.weapon_set_shown) << ' '
            << a.must_stay << ' ' << a.panicked << ' ' << a.closest_to_edge << ' ' << a.turns_stuck
            << ' ' << a.source.morale << ' ' << a.source.intelligence << ' '
            << a.source.hit_points_percent << ' ' << a.source.attack_bonus << ' '
            << std::quoted(a.source.melee_weapon) << ' ' << std::quoted(a.source.ranged_weapon)
            << ' ' << a.source.holds_shield << ' ' << a.run_start.x << ' ' << a.run_start.y << ' '
            << a.run_end.x << ' ' << a.run_end.y << ' ' << a.run_step.x << ' ' << a.run_step.y
            << ' ' << a.run_feet << ' ' << a.flame_lit << ' ' << a.curse_lifted << ' '
            << a.displacement_spent << ' ' << a.last_target << ' ' << a.item_uses.size() << ' ';
        for (const auto &[id, left] : a.item_uses)
        {
            out << id << ' ' << left << ' ';
        }
        detail::write_concentration(out, a.concentration);
        out << '\n';
    }
    out << path_.size() << ' ' << path_index_ << '\n';
    for (auto p : path_)
    {
        out << p.x << ' ' << p.y << ' ';
    }
    out << '\n';
    out << reactors_.size() << ' ' << reactor_index_ << '\n';
    for (auto id : reactors_)
    {
        out << id << ' ';
    }
    out << '\n';
    out << (flight_ ? flight_->actor : 0) << ' ' << (flight_ ? flight_->remaining : 0) << '\n';
    out << log_.size() << '\n';
    for (const auto &entry : log_)
    {
        out << std::quoted(entry.english) << '\n';
    }
    out << scope_ << ' ' << elapsed_ms_ << ' ' << morale_ << ' ' << spares_the_party_ << ' '
        << gold_ << ' ' << offerings_gold_ << ' ' << daylight_ << ' ' << allies_attacked_ << ' '
        << actors_.size() << '\n';
    for (const auto &a : actors_)
    {
        detail::write_effects(out, all_effects(a));
        out << '\n';
    }
    out << static_cast<bool>(temporary_offer_) << '\n';
    if (temporary_offer_)
    {
        out << temporary_offer_->amount << ' ' << std::quoted(temporary_offer_->source_id) << '\n';
    }
    out << frost_movement_ << ' ' << items_active_ << '\n';
    out << physical_inventory_ << '\n';
    if (items_active_)
    {
        out << items_.size() << '\n';
        for (const auto &item : items_)
        {
            out << item.id << ' ' << item.origin << ' ' << item.equipment_index << ' '
                << item.inventory_id << ' ' << std::quoted(item.definition) << ' ' << item.quantity
                << ' ' << item.stowed << ' ' << item.holder << '\n';
        }
    }
    out << static_cast<bool>(check_choice_) << '\n';
    if (check_choice_)
    {
        const auto &c = *check_choice_;
        out << c.actor << ' ' << c.target << ' ' << c.natural << ' ' << c.surge_spent << ' '
            << c.guidance << '\n';
    }
    out << static_cast<bool>(champion_move_) << '\n';
    if (champion_move_)
    {
        const auto &c = *champion_move_;
        out << c.actor << ' ' << c.target << ' ' << c.natural << ' ' << c.remaining << ' '
            << c.spell << ' ' << c.origin.x << ' ' << c.origin.y << ' ' << c.shift << ' '
            << c.toward << ' ' << c.withdraw << '\n';
    }
    out << static_cast<bool>(graze_) << '\n';
    if (graze_)
    {
        const auto &g = *graze_;
        out << g.actor << ' ' << g.target << ' ' << g.natural << ' ';
    }
    out << light_active_ << ' ' << nick_active_ << '\n';
    out << static_cast<bool>(mastery_) << '\n';
    if (mastery_)
    {
        const auto &m = *mastery_;
        out << m.actor << ' ' << m.target << ' ' << static_cast<unsigned>(m.kind) << ' '
            << m.natural << ' ' << m.ranged << ' ' << m.targeting << ' ' << std::quoted(m.weapon)
            << ' ' << m.origin.x << ' ' << m.origin.y << ' ' << m.thrown_item << ' ' << m.tactical
            << '\n';
    }
    const auto extra = [&](const ChampionMove &c)
    {
        out << c.triggered << ' ' << c.cleave << ' ' << c.helpless << ' ' << c.trigger_origin.x
            << ' ' << c.trigger_origin.y << ' ' << c.target_origin.x << ' ' << c.target_origin.y
            << '\n';
    };
    out << champion_offers_.size() << '\n';
    for (const auto &c : champion_offers_)
    {
        out << c.actor << ' ' << c.target << ' ' << c.natural << ' ' << c.remaining << ' '
            << c.spell << ' ' << c.origin.x << ' ' << c.origin.y << ' ';
        extra(c);
    }
    if (champion_move_)
    {
        extra(*champion_move_);
    }
    out << static_cast<bool>(effect_reaction_origin_) << ' ';
    if (effect_reaction_origin_)
    {
        out << effect_reaction_origin_->actor << ' ' << effect_reaction_origin_->source.x << ' '
            << effect_reaction_origin_->source.y << ' ' << effect_reaction_origin_->mover.x << ' '
            << effect_reaction_origin_->mover.y << ' ' << effect_reaction_origin_->movement << ' '
            << effect_reaction_origin_->prone;
    }
    out << '\n';
    out << initiative_choices_.size();
    for (const auto id : initiative_choices_)
    {
        out << ' ' << id;
    }
    out << '\n';
    out << static_cast<bool>(selection_);
    if (selection_)
    {
        out << ' ' << selection_->caster << ' ' << selection_->verb << ' '
            << selection_->chosen.size();
        for (const auto id : selection_->chosen)
        {
            out << ' ' << id;
        }
    }
    out << '\n';
    out << static_cast<bool>(area_);
    if (area_)
    {
        out << ' ' << area_->caster << ' ' << area_->verb << ' ' << area_->center.x << ' '
            << area_->center.y << ' ' << area_->cubes.size();
        for (const auto cube : area_->cubes)
        {
            out << ' ' << cube.x << ' ' << cube.y;
        }
        out << ' ' << area_->companion;
    }
    out << '\n' << zones_.size();
    for (const auto &zone : zones_)
    {
        out << ' ' << zone.caster << ' ' << static_cast<unsigned>(zone.kind) << ' ' << zone.ends_ms
            << ' ' << zone.save_dc << ' ' << zone.dealt << ' ' << zone.warded << ' ' << zone.slot
            << ' ' << zone.cells.size();
        for (const auto cell : zone.cells)
        {
            out << ' ' << cell.x << ' ' << cell.y;
        }
    }
    out << '\n' << static_cast<bool>(reaction_prompt_);
    if (reaction_prompt_)
    {
        const auto &prompt = *reaction_prompt_;
        const auto &question = prompt.question;
        const auto &command = prompt.command;
        out << ' ' << question.target << ' ' << static_cast<unsigned>(question.asked) << ' '
            << question.deflectable << ' ' << question.critical << ' ' << question.total << ' '
            << question.armor_class << ' ' << command.actor << ' ' << command.target << ' '
            << std::quoted(command.verb) << ' ' << command.destination.x << ' '
            << command.destination.y << ' ' << command.item << ' ' << prompt.answers.size();
        for (const auto &answer : prompt.answers)
        {
            out << ' ' << answer.target << ' ' << static_cast<unsigned>(answer.asked) << ' '
                << std::quoted(answer.verb);
        }
    }
    out << '\n';
    // What each actor carries in (a Torch or bow to draw, thrown gear), one line
    // per actor in turn order.
    for (const auto &a : actors_)
    {
        out << a.source.inventory.size();
        for (const auto &item : a.source.inventory)
        {
            out << ' ' << item.inventory_id << ' ' << std::quoted(item.definition) << ' '
                << item.quantity << ' ' << item.equipment_index;
        }
        out << '\n';
    }
    // Members who ran off the field, with the vitals and flasks they left with.
    out << fled_.size() << '\n';
    for (const auto &gone : fled_)
    {
        out << gone.source.id << ' ' << std::quoted(gone.source.definition) << ' '
            << std::quoted(gone.source.name) << ' ' << static_cast<unsigned>(gone.source.side)
            << ' ' << gone.source.cell.x << ' ' << gone.source.cell.y << ' '
            << std::quoted(gone.source.character_profile) << ' ' << gone.hp << ' ' << gone.max_hp
            << ' ' << gone.armor_class << ' ' << gone.vitals.hit_points << ' ' << gone.vitals.dead
            << ' ' << std::quoted(gone.vitals.resources) << ' '
            << std::quoted(gone.vitals.description) << ' ' << gone.surrendered << ' '
            << gone.thrown_gear_left.size();
        for (const auto &[gear, left] : gone.thrown_gear_left)
        {
            out << ' ' << std::quoted(gear) << ' ' << left;
        }
        out << ' ' << gone.item_uses.size();
        for (const auto &[id, left] : gone.item_uses)
        {
            out << ' ' << id << ' ' << left;
        }
        out << '\n';
    }
    return out.str();
}

// Parse one actor independently of session mutation.
Actor read_checkpoint_actor(std::istream &input, const Content &content)
{
    Actor actor;
    auto &source = actor.source;
    unsigned side{}, light_count{};
    input >> source.id >> std::quoted(source.definition) >> std::quoted(source.name) >> side >>
        source.cell.x >> source.cell.y >> source.footprint.width >> source.footprint.height >>
        actor.life.hp >> actor.initiative >> actor.movement >> actor.winds;
    for (unsigned level = 1; level <= max_slot_level; ++level)
    {
        input >> actor.slots[level];
    }
    int weapon_set_shown{};
    input >> actor.life.successes >> actor.life.failures >> actor.actions.normal >>
        actor.actions.haste >> actor.bonus >> actor.reaction >> actor.dodge >> actor.disengaged >>
        actor.life.stable >> actor.life.dead >> std::quoted(source.character_profile) >>
        actor.spent_slot >> actor.savage_used >> actor.facing_left >> actor.involuntary_overlap >>
        actor.hit_dice >> actor.life.recovery.death_save_in_ms >>
        actor.life.recovery.stable_recovery_in_ms >> actor.life.temporary_hp.amount >>
        std::quoted(actor.life.temporary_hp.source_id) >> actor.rushes >> actor.rush_used >>
        actor.surges >> actor.surge_used >> actor.actions.surge >> actor.actions.extra_attacks >>
        actor.actions.loading_fired >> actor.actions.exhaled >> actor.dashes >> actor.arcane >>
        actor.lay_on_hands >> actor.free_casts >> actor.channel_divinity >> actor.initiate_casts >>
        actor.innate_spent >> actor.smite_target >> actor.smite_critical >> actor.sneak_used >>
        actor.aim_used >> actor.aim_ready >> actor.moved >> actor.attacked >> actor.pounce >>
        actor.brutal_used >> actor.stun_used >> actor.fleet_step >> actor.carried >>
        actor.selected_weapon >> light_count;
    if (!input || light_count > 2)
    {
        throw std::runtime_error("Invalid Light attack count");
    }
    // The checkpoint stores a side as its enumerator's value.
    if (side > static_cast<unsigned>(Side::opposition))
    {
        throw std::runtime_error("Invalid checkpoint side");
    }
    source.side = static_cast<Side>(side);
    for (unsigned i = 0; i < light_count; ++i)
    {
        unsigned id{};
        input >> id;
        actor.light_origins.push_back(id);
    }
    input >> actor.light_extra >> actor.nick_origin >> actor.cleave_used >> actor.colossus_used >>
        actor.horde_used >> actor.horde_origin >> actor.prey_used >> actor.prey_origin >>
        actor.turned >> actor.skips_turn >> actor.strike_used >> actor.smite_used >>
        actor.drink_used >> actor.hurl_used >> actor.smite_melee >> actor.resistance_used >>
        actor.regeneration_blocked >> actor.rises_on_round >> actor.thrown_gear_left[0] >>
        actor.thrown_gear_left[1] >> actor.thrown_gear_left[2] >> actor.burning >>
        actor.oiled_until_round >> actor.coated_until_round >> actor.shield_off >>
        std::quoted(actor.drawn_weapon) >> weapon_set_shown >> actor.must_stay >> actor.panicked >>
        actor.closest_to_edge >> actor.turns_stuck >> actor.source.morale >>
        actor.source.intelligence >> actor.source.hit_points_percent >> actor.source.attack_bonus >>
        std::quoted(actor.source.melee_weapon) >> std::quoted(actor.source.ranged_weapon) >>
        actor.source.holds_shield >> actor.run_start.x >> actor.run_start.y >> actor.run_end.x >>
        actor.run_end.y >> actor.run_step.x >> actor.run_step.y >> actor.run_feet >>
        actor.flame_lit >> actor.curse_lifted >> actor.displacement_spent >> actor.last_target;
    if (weapon_set_shown < 0 ||
        weapon_set_shown > static_cast<int>(rules::WeaponSetShown::ranged) ||
        (!actor.drawn_weapon.empty() && !detail::weapon(actor.drawn_weapon)))
    {
        throw std::runtime_error("Invalid weapon in hand");
    }
    actor.weapon_set_shown = static_cast<rules::WeaponSetShown>(weapon_set_shown);
    std::size_t item_count{};
    input >> item_count;
    // The inventory comes later in the checkpoint; 64 items is more than any
    // creature carries.
    if (!input || item_count > 64)
    {
        throw std::runtime_error("Invalid checkpoint item uses");
    }
    actor.item_uses.resize(item_count);
    for (auto &[id, left] : actor.item_uses)
    {
        input >> id >> left;
    }
    if (actor.source.morale > 100 || actor.source.intelligence > 30 ||
        actor.source.hit_points_percent < 1 || actor.source.hit_points_percent > 1000 ||
        actor.source.attack_bonus < -20 || actor.source.attack_bonus > 20)
    {
        throw std::runtime_error("Invalid checkpoint morale");
    }
    if (actor.source.melee_weapon.size() > 80 || actor.source.ranged_weapon.size() > 80)
    {
        throw std::runtime_error("Invalid checkpoint weapon");
    }
    if (actor.run_feet < 0 || actor.run_feet % 5 || actor.run_feet > 5 * 64 * 64 ||
        std::abs(actor.run_step.x) > 1 || std::abs(actor.run_step.y) > 1)
    {
        throw std::runtime_error("Invalid checkpoint charge run");
    }
    actor.concentration = detail::read_concentration(input);
    // Hunter's Mark, 1 hour, is the longest Concentration spell in the game.
    if (const auto &held = actor.concentration.active();
        held && (held->source.caster != source.id || held->remaining_ms > 3600000))
    {
        throw std::runtime_error("Invalid checkpoint concentration");
    }
    detail::decode_stable_recovery(actor.life.recovery);
    if (!input ||
        (source.character_profile.empty() && !content.definitions.contains(source.definition)))
    {
        throw std::runtime_error("Invalid checkpoint actor");
    }
    actor.definition = source.character_profile.empty()
                           ? fielded(content, source)
                           : character_definition(source.character_profile);
    if (actor.shield_off && source.character_profile.empty())
    {
        if (!source.holds_shield)
        {
            throw std::runtime_error("Invalid shield removal");
        }
        actor.definition = monster_without_shield(actor.definition);
    }
    else if (actor.shield_off)
    {
        if (!wears_shield(actor.definition))
        {
            throw std::runtime_error("Invalid shield removal");
        }
        actor.definition = without_shield(source.character_profile, actor.definition);
    }
    const auto &definition = actor.definition;
    if (actor.cleave_used && std::none_of(definition.masteries.begin(), definition.masteries.end(),
                                          [](const auto &key)
                                          {
                                              const auto *weapon = detail::weapon(key);
                                              return weapon &&
                                                     weapon->mastery == detail::Mastery::cleave;
                                          }))
    {
        throw std::runtime_error("Invalid Cleave expenditure source");
    }
    if ((actor.colossus_used && !definition.colossus_slayer) ||
        ((actor.horde_used || actor.horde_origin) && !definition.horde_breaker) ||
        ((actor.prey_used || actor.prey_origin) && !definition.superior_prey))
    {
        throw std::runtime_error("Invalid Hunter's Prey state");
    }
    if ((actor.sneak_used && !definition.sneak_level) ||
        (actor.aim_used && (definition.sneak_level < 3 || actor.bonus || actor.moved)) ||
        (actor.aim_ready && !actor.aim_used))
    {
        throw std::runtime_error("Invalid Rogue attack expenditure");
    }
    // Cunning Action, Aggressive, Step of the Wind and Expeditious Retreat
    // spend the bonus action on a Dash. Effects are read later, so knowing the
    // spell is enough here.
    const bool bonus_dash = (definition.cunning || definition.aggressive || definition.focus ||
                             definition.bonus_charge || definition.rampage ||
                             detail::knows_spell(definition.spells, "expeditious_retreat")) &&
                            !actor.bonus;
    if (actor.dashes < 0 ||
        actor.dashes > static_cast<int>(!actor.actions.normal) +
                           static_cast<int>(actor.rush_used || bonus_dash) +
                           static_cast<int>(actor.surge_used && !actor.actions.surge))
    {
        throw std::runtime_error("Invalid Dash allowance count");
    }
    if (actor.pounce < 0 || actor.pounce > definition.speed || actor.pounce > actor.movement)
    {
        throw std::runtime_error("Invalid Instinctive Pounce movement");
    }
    // Extra Attack's attacks follow an Attack action already taken.
    if (actor.actions.extra_attacks > static_cast<unsigned>(definition.extra_attacks) ||
        (actor.actions.extra_attacks && actor.actions.normal && !actor.surge_used))
    {
        throw std::runtime_error("Invalid Extra Attack count");
    }
    if ((actor.actions.loading_fired || actor.actions.exhaled) && actor.actions.normal &&
        !actor.surge_used)
    {
        throw std::runtime_error("Invalid Loading shot or exhalation");
    }
    if (actor.fleet_step && (!definition.fleet_step || actor.bonus))
    {
        throw std::runtime_error("Invalid Fleet Step");
    }
    if (actor.carried && (!definition.heightened_focus || actor.carried == actor.source.id))
    {
        throw std::runtime_error("Invalid carried ally");
    }
    // The upper bound waits for the effects, read later: Aid raises it.
    if (actor.life.hp < 0 || (actor.life.dead && actor.life.hp > 0) ||
        // Dash spends the action before adding a second movement allowance.
        // Accepting both extra movement and an unused action lets a later Dash
        // create a state outside the checkpoint's own movement bounds.
        actor.movement < 0 || !within(actor.arcane, arcane_capacity(definition)) ||
        !within(actor.surges, surge_capacity(definition)) ||
        (actor.surge_used &&
         (!definition.surges || actor.surges[CharacterClass::fighter] == definition.surges)) ||
        (actor.actions.surge && !actor.surge_used) || actor.rushes < 0 ||
        actor.rushes > definition.rushes ||
        (actor.rush_used &&
         (actor.bonus || !definition.rushes || actor.rushes == definition.rushes)) ||
        actor.winds < 0 || actor.winds > definition.winds ||
        !slots_in_range(actor.slots, definition) || actor.hit_dice < 0 ||
        actor.hit_dice > (definition.hit_die ? definition.level : 0) || actor.life.successes < 0 ||
        actor.life.successes > 3 || actor.life.failures < 0 || actor.life.failures > 4 ||
        !within(actor.lay_on_hands, lay_capacity(definition)) ||
        !within(actor.free_casts, free_cast_capacity(definition)) ||
        !within(actor.channel_divinity, channel_capacity(definition)) || actor.initiate_casts < 0 ||
        (actor.initiate_casts & ~initiate_capacity(definition)) ||
        !valid_innate_spent(definition, actor.innate_spent))
    {
        throw std::runtime_error("Invalid checkpoint actor state");
    }
    detail::validate_recovery(actor.life);
    detail::validate_temporary_hp(actor.life.temporary_hp);
    return actor;
}

Battlefield read_checkpoint_board(std::istream &input)
{
    Battlefield board;
    input >> board.width >> board.height;
    // Bound dimensions before multiplication or allocation, even for truncated
    // input. No serialized count is allowed to control an unbounded allocation.
    if (!input || board.width < 2 || board.height < 2 || board.width > 64 || board.height > 64)
    {
        throw std::runtime_error("Invalid checkpoint board");
    }
    for (int i = 0; i < board.width * board.height; ++i)
    {
        unsigned terrain{};
        input >> terrain;
        if (!input || terrain > 2)
        {
            throw std::runtime_error("Invalid checkpoint terrain");
        }
        board.terrain.push_back(static_cast<Terrain>(terrain));
    }
    std::size_t plants{};
    input >> plants;
    if (!input || (plants && plants != board.terrain.size()))
    {
        throw std::runtime_error("Invalid checkpoint plants");
    }
    std::string bits;
    if (plants)
    {
        input >> bits;
    }
    if (!input || bits.size() != plants || bits.find_first_not_of("01") != bits.npos)
    {
        throw std::runtime_error("Invalid checkpoint plants");
    }
    for (const char bit : bits)
    {
        board.plants.push_back(bit == '1');
    }
    return board;
}

void Session::restore_movement(std::istream &input)
{
    std::size_t count{};
    input >> count >> path_index_;
    if (!input || count > 1024 || path_index_ > count)
    {
        throw std::runtime_error("Invalid checkpoint path");
    }
    for (std::size_t i = 0; i < count; ++i)
    {
        Cell cell;
        input >> cell.x >> cell.y;
        if (!input || board_.at(cell) == Terrain::obstacle)
        {
            throw std::runtime_error("Invalid checkpoint path cell");
        }
        path_.push_back(cell);
    }
    input >> count >> reactor_index_;
    if (!input || count > 64 || reactor_index_ > count)
    {
        throw std::runtime_error("Invalid checkpoint reactions");
    }
    std::set<EntityId> seen;
    for (std::size_t i = 0; i < count; ++i)
    {
        EntityId id{};
        input >> id;
        const auto exists = std::any_of(actors_.begin(), actors_.end(),
                                        [id](const auto &actor)
                                        {
                                            return actor.source.id == id;
                                        });
        if (!input || !seen.insert(id).second || !exists)
        {
            throw std::runtime_error("Invalid checkpoint reactor");
        }
        reactors_.push_back(id);
    }
    // A fleeing creature, paused on an Opportunity Attack.
    Flight flight;
    input >> flight.actor >> flight.remaining;
    const bool fleeing = std::any_of(actors_.begin(), actors_.end(),
                                     [&](const auto &actor)
                                     {
                                         return actor.source.id == flight.actor && conscious(actor);
                                     });
    if (!input ||
        (flight.actor &&
         (!fleeing || flight.remaining < 0 || flight.remaining > 120 || path_.empty())) ||
        (!flight.actor && flight.remaining))
    {
        throw std::runtime_error("Invalid checkpoint flight");
    }
    if (flight.actor)
    {
        flight_ = flight;
    }
}

void Session::validate_champion_move() const
{
    if (!champion_move_)
    {
        return;
    }
    const auto &c = *champion_move_;
    const auto who = std::find_if(actors_.begin(), actors_.end(),
                                  [&](const auto &a)
                                  {
                                      return a.source.id == c.actor;
                                  });
    if (c.toward)
    {
        // Forceful Blow: the turn's Barbarian follows the creature it struck.
        const int budget =
            who == actors_.end()
                ? 0
                : std::max(0, def(*who).speed - detail::speed_penalty(who->effects)) / 2;
        if (who == actors_.end() || !def(*who).brutal_strike || !conscious(*who) ||
            c.actor != actors_[turn_].source.id || c.target != c.toward || c.natural || c.spell ||
            c.triggered || c.shift || !who->brutal_used || pending() || c.remaining < 0 ||
            c.remaining > budget || !board_.contains(c.origin) ||
            distance(c.origin, *who) > budget - c.remaining ||
            std::none_of(actors_.begin(), actors_.end(),
                         [&](const auto &a)
                         {
                             return a.source.id == c.toward && !a.life.dead;
                         }))
        {
            throw std::runtime_error("Invalid Forceful Blow movement");
        }
        return;
    }
    if (c.withdraw)
    {
        // Withdraw: the turn's Rogue, after a Cunning Strike.
        const int budget =
            who == actors_.end()
                ? 0
                : std::max(0, def(*who).speed - detail::speed_penalty(who->effects)) / 2;
        if (who == actors_.end() || !def(*who).cunning_strike || !conscious(*who) ||
            c.actor != actors_[turn_].source.id || c.target != c.actor || c.natural || c.spell ||
            c.triggered || c.shift || c.toward || !who->sneak_used || pending() ||
            c.remaining < 0 || c.remaining > budget || !board_.contains(c.origin) ||
            distance(c.origin, *who) > budget - c.remaining)
        {
            throw std::runtime_error("Invalid Withdraw movement");
        }
        return;
    }
    if (c.shift)
    {
        // Tactical Shift: the turn's Fighter, after its Second Wind.
        const int budget =
            who == actors_.end()
                ? 0
                : std::max(0, def(*who).speed - detail::speed_penalty(who->effects)) / 2;
        if (who == actors_.end() || !def(*who).tactical_shift || !conscious(*who) ||
            c.actor != actors_[turn_].source.id || c.target != c.actor || c.natural || c.spell ||
            c.triggered || who->bonus || pending() || c.remaining < 0 || c.remaining > budget ||
            !board_.contains(c.origin) || distance(c.origin, *who) > budget - c.remaining)
        {
            throw std::runtime_error("Invalid Tactical Shift movement");
        }
        return;
    }
    const auto target = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto &a)
                                     {
                                         return a.source.id == c.target;
                                     });
    if (who == actors_.end() || target == actors_.end() || c.actor == c.target ||
        !def(*who).champion || !conscious(*who) || c.natural < 2 || c.natural > 20 ||
        c.origin.x < 0 || c.origin.y < 0 || c.origin.x >= board_.width ||
        c.origin.y >= board_.height || board_.at(c.origin) == Terrain::obstacle || check_choice_ ||
        temporary_offer_ || outcome_ != Outcome::ongoing)
    {
        throw std::runtime_error("Invalid Champion movement source");
    }
    if (c.triggered)
    {
        if (!board_.contains(c.trigger_origin) ||
            board_.at(c.trigger_origin) == Terrain::obstacle || !board_.contains(c.target_origin) ||
            board_.at(c.target_origin) == Terrain::obstacle || (c.cleave && !who->cleave_used) ||
            (c.helpless && distance(c.trigger_origin, c.target_origin) > 5))
        {
            throw std::runtime_error("Invalid Champion trigger positions");
        }
    }
    else if (c.cleave || c.helpless || c.trigger_origin != Cell{} || c.target_origin != Cell{})
    {
        throw std::runtime_error("Unexpected Champion trigger metadata");
    }
    const int budget = std::max(0, def(*who).speed - detail::speed_penalty(who->effects)) / 2;
    if ((c.actor == actors_[turn_].source.id &&
         (who->actions.surge || (who->actions.normal && !who->surge_used))) ||
        c.remaining < 0 || c.remaining > budget ||
        distance(c.origin, *who) > budget - c.remaining ||
        !(c.natural == 20 || (!c.spell && c.natural >= def(*who).critical_from) ||
          ((c.helpless || target->life.hp == 0) &&
           (c.triggered ? distance(c.trigger_origin, c.target_origin)
                        : distance(c.origin, *target)) <= 5)) ||
        (pending() ? (pending() != c.actor || who->reaction ||
                      (!c.cleave && c.target != mover().source.id))
                   : (c.actor != actors_[turn_].source.id &&
                      !((c.cleave ? actors_[turn_].life.hp == 0
                                  : c.target == actors_[turn_].source.id && target->life.hp == 0) &&
                        !who->reaction))))
    {
        throw std::runtime_error("Invalid Champion movement allowance/trigger");
    }
}

void Session::validate_check() const
{
    if (!check_choice_)
    {
        return;
    }
    const auto &c = *check_choice_;
    const auto &a = actors_[turn_];
    const auto target = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto &t)
                                     {
                                         return t.source.id == c.target;
                                     });
    if (c.actor != a.source.id || !conscious(a) || !def(a).tactical_mind || a.winds <= 0 ||
        c.natural < 1 || c.natural > 20 || c.natural + def(a).medicine + c.guidance >= 10 ||
        target == actors_.end() || target->life.hp != 0 || target->life.dead ||
        target->life.stable || distance(a, *target) > 5 || !line_of_sight(a, *target) ||
        pending() || temporary_offer_ || outcome_ != Outcome::ongoing || a.actions.surge ||
        (c.surge_spent ? !a.surge_used : a.actions.normal))
    {
        throw std::runtime_error("Invalid pending ability check");
    }
}

void Session::validate_graze() const
{
    if (!graze_)
    {
        return;
    }
    const auto &g = *graze_;
    const auto a = std::find_if(actors_.begin(), actors_.end(),
                                [&](const auto &a)
                                {
                                    return a.source.id == g.actor;
                                });
    const auto t = std::find_if(actors_.begin(), actors_.end(),
                                [&](const auto &a)
                                {
                                    return a.source.id == g.target;
                                });
    if (a == actors_.end() || t == actors_.end() || a == t || !conscious(*a) || t->life.dead ||
        weapon_mastery(*a, AttackRange::melee) != detail::Mastery::graze || g.natural < 1 ||
        g.natural > 20 || attack_hits(g.natural, def(*a).melee_bonus, armor_class(*t)) ||
        g.natural >= def(*a).critical_from || distance(*a, *t) > def(*a).reach ||
        !line_of_sight(*a, *t) || check_choice_ || temporary_offer_ || champion_move_ ||
        outcome_ != Outcome::ongoing ||
        (pending() ? (pending() != g.actor || a->reaction || g.target != mover().source.id)
                   : (g.actor != actors_[turn_].source.id ||
                      (a->actions.normal && (!a->surge_used || a->actions.surge)))))
    {
        throw std::runtime_error("Invalid pending Graze");
    }
}

void Session::validate_initiative() const
{
    if (initiative_choices_.empty())
    {
        return;
    }
    if (round_ != 1 || turn_ != 0 || elapsed_ms_ || outcome_ != Outcome::ongoing || pending() ||
        !path_.empty() || temporary_offer_ || check_choice_ || champion_move_ || graze_ ||
        effect_waiting())
    {
        throw std::runtime_error("Invalid pre-turn Initiative phase");
    }
    std::set<EntityId> seen;
    for (const auto id : initiative_choices_)
    {
        const auto &a = actor(id);
        if (!seen.insert(id).second || a.source.side != Side::party ||
            (!def(a).alert && !metabolism_ready(a) && !persistent_ready(a)) || !conscious(a))
        {
            throw std::runtime_error("Invalid Initiative holder");
        }
    }
    for (std::size_t i = 0; i < actors_.size(); ++i)
    {
        const auto &a = actors_[i];
        if (!a.actions.normal || a.actions.surge || !a.bonus || !a.reaction || a.dodge ||
            a.surge_used || a.rush_used || a.savage_used || a.sneak_used || a.aim_used || a.moved ||
            a.cleave_used || a.movement != def(a).speed)
        {
            throw std::runtime_error("Spent action before Initiative decisions");
        }
        if (i &&
            (actors_[i - 1].initiative < a.initiative ||
             (actors_[i - 1].initiative == a.initiative && actors_[i - 1].source.id > a.source.id)))
        {
            throw std::runtime_error("Unsorted Initiative phase");
        }
    }
}

void Session::validate_restored_state() const
{
    for (const auto &a : actors_)
    {
        if (a.life.hp > max_hp(a))
        {
            throw std::runtime_error("Invalid checkpoint Hit Points");
        }
    }
    for (const auto &a : actors_)
    {
        if ((a.turned != detail::has_effect(a.effects, detail::EffectKind::dominated)) ||
            (a.skips_turn && !a.turned &&
             !detail::has_effect(a.effects, detail::EffectKind::raised)))
        {
            throw std::runtime_error("Invalid dominated creature");
        }
    }
    for (const auto &a : actors_)
    {
        for (const auto origin : {a.horde_origin, a.prey_origin})
        {
            if (origin && std::none_of(actors_.begin(), actors_.end(),
                                       [&](const auto &other)
                                       {
                                           return other.source.id == origin;
                                       }))
            {
                throw std::runtime_error("Invalid Hunter's Prey target");
            }
        }
    }
    validate_initiative();
    validate_check();
    validate_champion_move();
    validate_graze();
    validate_mastery_state();
    if (pending() && path_index_ >= path_.size())
    {
        throw std::runtime_error("Reaction without movement");
    }
    const auto &mover = actors_[turn_];
    if (temporary_offer_ &&
        (outcome_ != Outcome::ongoing || pending() || !conscious(mover) || !mover.rush_used ||
         mover.bonus || mover.life.temporary_hp.amount <= 0 ||
         temporary_offer_->amount != mover.definition.rushes ||
         temporary_offer_->source_id != rush_source))
    {
        throw std::runtime_error("Invalid pending Temporary HP replacement");
    }
    bool party = false, enemies = false;
    for (const auto &actor : actors_)
    {
        if (actor.cleave_used &&
            (actor.source.id == mover.source.id
                 ? (actor.actions.normal && (!actor.surge_used || actor.actions.surge))
                 : actor.reaction))
        {
            throw std::runtime_error("Cleave without a spent triggering attack");
        }
        if (actor.aim_used && actor.source.id != mover.source.id)
        {
            throw std::runtime_error("Steady Aim outside current turn");
        }
        if (actor.actions.surge && actor.source.id != mover.source.id)
        {
            throw std::runtime_error("Action Surge allowance outside its turn");
        }
        if (actor.involuntary_overlap && (actor.life.dead || !shares_occupied_space(actor)))
        {
            throw std::runtime_error("Invalid involuntary checkpoint overlap");
        }
        if (actor.regeneration_blocked && !def(actor).regeneration)
        {
            throw std::runtime_error("Regeneration blocked without Regeneration");
        }
        // 3d6 rounds at most.
        if (actor.rises_on_round < 0 || actor.rises_on_round > static_cast<int>(round_) + 18 ||
            (actor.rises_on_round && !def(actor).regeneration))
        {
            throw std::runtime_error("Invalid getting-up round");
        }
        if ((actor.burning && actor.life.dead) || actor.oiled_until_round < 0 ||
            actor.oiled_until_round > static_cast<int>(round_) + 10 ||
            actor.coated_until_round < 0 ||
            actor.coated_until_round > static_cast<int>(round_) + 10 ||
            (actor.source.character_profile.empty() &&
             std::any_of(actor.thrown_gear_left.begin(), actor.thrown_gear_left.end(),
                         [](auto left)
                         {
                             return left > 0;
                         })))
        {
            throw std::runtime_error("Invalid fire, oil or thrown gear state");
        }
        if (actor.life.hp > 0 && !actor.life.dead)
        {
            (actor.source.side == Side::party ? party : enemies) = true;
        }
        if (actor.life.dead)
        {
            continue;
        }
        for (const auto &other : actors_)
        {
            if (other.source.id <= actor.source.id || other.life.dead ||
                distance(actor, other) != 0)
            {
                continue;
            }
            const auto &moving = this->mover();
            const bool in_transit =
                pending() && path_index_ > 0 && path_[path_index_ - 1] == moving.source.cell &&
                (actor.source.id == moving.source.id || other.source.id == moving.source.id);
            if ((in_transit && (actor.source.side == other.source.side || unconscious(actor) ||
                                unconscious(other))) ||
                actor.involuntary_overlap || other.involuntary_overlap || actor.life.hp == 0 ||
                other.life.hp == 0)
            {
                continue;
            }
            throw std::runtime_error("Invalid overlapping checkpoint actors");
        }
    }
    const bool party_fled = std::any_of(fled_.begin(), fled_.end(),
                                        [](const auto &gone)
                                        {
                                            return gone.source.side == Side::party;
                                        });
    const auto expected = !party     ? (party_fled ? Outcome::fled : Outcome::defeat)
                          : !enemies ? Outcome::victory
                                     : Outcome::ongoing;
    if (outcome_ != expected || (initiative_choices_.empty() && expected == Outcome::ongoing &&
                                 mover.life.hp == 0 && !champion_move_ && !effect_waiting()))
    {
        throw std::runtime_error("Invalid checkpoint outcome/turn");
    }
    if (!pending() && (!path_.empty() || !reactors_.empty()))
    {
        throw std::runtime_error("Unpaused checkpoint movement");
    }
    if (pending())
    {
        validate_pending_movement();
    }
}

void Session::validate_pending_movement() const
{
    auto moving = mover();
    if (effect_reaction_origin_)
    {
        moving.source.cell = effect_reaction_origin_->mover;
        moving.effects.prone = effect_reaction_origin_->prone;
    }
    if (outcome_ != Outcome::ongoing || moving.disengaged)
    {
        throw std::runtime_error("Invalid pending movement");
    }
    std::vector<detail::Occupant> occupants;
    for (const auto &other : actors_)
    {
        if (!other.life.dead && !downed_riser(other) && other.source.id != moving.source.id)
        {
            occupants.push_back({effect_reaction_origin_ && other.source.id == pending()
                                     ? effect_reaction_origin_->source
                                 : champion_move_ && other.source.id == champion_move_->actor
                                     ? champion_move_->origin
                                     : other.source.cell,
                                 other.source.side != moving.source.side, unconscious(other),
                                 other.life.hp == 0, footprint(other)});
        }
    }
    const detail::MovementGrid grid{board_,
                                    moving.source.cell,
                                    occupants,
                                    moving.effects.prone,
                                    def(moving).flies,
                                    def(moving).incorporeal,
                                    footprint(moving)};
    auto cell = moving.source.cell;
    int remaining =
        effect_reaction_origin_ ? effect_reaction_origin_->movement : mover_budget(moving);
    // Only the suffix remains to be travelled. The prefix is history and has
    // already spent its movement budget; charging for it again breaks restores.
    for (auto i = path_index_; i < path_.size(); ++i)
    {
        const auto next = path_[i];
        const auto cost = grid.step_cost(cell, next);
        if (!cost || *cost > remaining)
        {
            throw std::runtime_error("Invalid checkpoint movement step/budget");
        }
        remaining -= *cost;
        cell = next;
    }
    if (!grid.can_stop_at(cell))
    {
        throw std::runtime_error("Invalid checkpoint movement destination");
    }
    for (auto i = reactor_index_; i < reactors_.size(); ++i)
    {
        const auto reactor = std::find_if(actors_.begin(), actors_.end(),
                                          [&](const auto &actor)
                                          {
                                              return actor.source.id == reactors_[i];
                                          });
        // restore_movement already established that every reactor ID exists.
        const auto &actor = *reactor;
        if (effect_reaction_origin_ && i == reactor_index_)
        {
            auto original = actor;
            original.source.cell = effect_reaction_origin_->source;
            if (actor.source.side == moving.source.side || actor.reaction ||
                !has_weapon_reaction(original, moving.source.cell, path_[path_index_],
                                     footprint(moving)))
            {
                throw std::runtime_error("Invalid mastery reaction trigger");
            }
            continue;
        }
        if (champion_move_ && i == reactor_index_ && actor.source.id == champion_move_->actor)
        {
            if (actor.source.side == moving.source.side ||
                distance(champion_move_->origin, moving) > def(actor).reach ||
                distance(champion_move_->origin, path_[path_index_]) <= def(actor).reach)
            {
                throw std::runtime_error("Invalid Champion reaction origin");
            }
            continue;
        }
        if (detail::opportunity_blocked(actor.effects) || actor.life.hp == 0 ||
            (!actor.reaction &&
             !(graze_ && graze_->actor == actor.source.id && i == reactor_index_)) ||
            actor.source.side == moving.source.side ||
            !has_weapon_reaction(actor, moving.source.cell, path_[path_index_],
                                 footprint(moving)) ||
            !can_see(actor, moving))
        {
            throw std::runtime_error("Invalid checkpoint opportunity attack");
        }
    }
}

void Session::restore_log(std::istream &input)
{
    std::size_t count{};
    input >> count;
    if (!input || count > 80)
    {
        throw std::runtime_error("Invalid checkpoint log");
    }
    log_.clear();
    for (std::size_t i = 0; i < count; ++i)
    {
        std::string line;
        input >> std::quoted(line);
        if (!input || line.size() > 1000)
        {
            throw std::runtime_error("Invalid checkpoint log line");
        }
        Message message{line, {}};
        log_.push_back({std::move(line), std::move(message)});
    }
}

std::unique_ptr<Session> Session::restore(std::shared_ptr<const Content> content,
                                          std::string_view bytes)
{
    if (bytes.size() > 4 * 1024 * 1024)
    {
        throw std::runtime_error("Combat checkpoint exceeds limit");
    }
    std::istringstream input{std::string(bytes)};
    std::string magic;
    unsigned version{};
    Identity identity;
    input >> magic >> version >> std::quoted(identity.module) >> std::quoted(identity.version) >>
        std::quoted(identity.content);
    if (!input || magic != "OGCOMBAT" || version > checkpoint_format)
    {
        throw std::runtime_error("Combat checkpoint rules/content version mismatch");
    }
    if (version < checkpoint_format || identity != content->identity)
    {
        throw std::runtime_error(std::string(older_save_message));
    }
    Encounter encounter;
    encounter.battlefield = read_checkpoint_board(input);
    std::uint64_t rng{}, revision{};
    unsigned turn{}, round{}, outcome{}, count{};
    input >> rng >> revision >> turn >> round >> outcome >> count;
    if (!input || count < 1 || count > 64 || turn >= count || round == 0 || outcome > 3 ||
        !revision)
    {
        throw std::runtime_error("Invalid checkpoint header");
    }
    std::vector<Actor> actors;
    for (unsigned i = 0; i < count; ++i)
    {
        auto actor = read_checkpoint_actor(input, *content);
        encounter.participants.push_back(actor.source);
        actors.push_back(std::move(actor));
    }
    // Build and validate a separate owned candidate. Any failure destroys it;
    // callers never receive a partially restored session or lose a live one.
    // The constructor checks identities/geometry; saved order and RNG then
    // replace its fresh initiative state before relational checks run.
    auto session = std::make_unique<Session>(std::move(content), std::move(encounter), 0, true);
    session->actors_ = std::move(actors);
    session->rng_ = rng;
    session->revision_ = revision;
    session->turn_ = turn;
    session->round_ = round;
    session->outcome_ = static_cast<Outcome>(outcome);
    session->restore_movement(input);
    session->restore_log(input);
    unsigned effects_count{};
    input >> session->scope_ >> session->elapsed_ms_ >> session->morale_ >>
        session->spares_the_party_ >> session->gold_ >> session->offerings_gold_ >>
        session->daylight_ >> session->allies_attacked_ >> effects_count;
    if (session->morale_ > 100 || session->offerings_gold_ > session->gold_)
    {
        throw std::runtime_error("Invalid checkpoint morale");
    }
    if (!input || !session->scope_ || effects_count != session->actors_.size())
    {
        throw std::runtime_error("Invalid checkpoint effect header");
    }
    for (auto &a : session->actors_)
    {
        a.effects = detail::read_effects(input);
        if (a.life.recovery.stable_recovery_due && !detail::healing_blocked(a.effects))
        {
            throw std::runtime_error("Invalid earned recovery checkpoint");
        }
    }
    bool pending_offer{};
    input >> pending_offer;
    if (pending_offer)
    {
        TemporaryHitPoints offer;
        input >> offer.amount >> std::quoted(offer.source_id);
        detail::validate_temporary_hp(offer);
        session->temporary_offer_ = std::move(offer);
    }
    if (!input)
    {
        throw std::runtime_error("Invalid Temporary HP choice checkpoint");
    }
    bool has_items{};
    input >> session->frost_movement_ >> has_items >> session->physical_inventory_;
    if (!input || (session->physical_inventory_ && !has_items))
    {
        throw std::runtime_error("Invalid recovery checkpoint flags");
    }
    session->items_active_ = has_items;
    if (has_items)
    {
        std::size_t held_item_count{};
        input >> held_item_count;
        if (!input || held_item_count > 100000)
        {
            throw std::runtime_error("Invalid held item count");
        }
        session->items_.resize(held_item_count);
        for (unsigned index = 0; index < held_item_count; ++index)
        {
            auto &item = session->items_[index];
            unsigned id{};
            item.id = index + 1;
            input >> id >> item.origin >> item.equipment_index >> item.inventory_id >>
                std::quoted(item.definition) >> item.quantity >> item.stowed;
            const auto source = std::find_if(session->actors_.begin(), session->actors_.end(),
                                             [&](const auto &a)
                                             {
                                                 return a.source.id == item.origin;
                                             });
            const auto *weapon = detail::weapon(item.definition);
            if (!input || source == session->actors_.end() ||
                source->source.character_profile.empty() ||
                (!weapon && !detail::is_shield(item.definition)) || !item.quantity)
            {
                throw std::runtime_error("Invalid physical inventory source");
            }
            const auto original = character_definition(source->source.character_profile);
            if (!item.inventory_id &&
                (item.equipment_index >= original.equipment_keys.size() ||
                 original.equipment_keys[item.equipment_index] != item.definition))
            {
                throw std::runtime_error("Invalid physical equipment source");
            }
            item.label = {weapon ? std::string(weapon->label) : "Shield", {}};
            input >> item.holder;
            if (!input || id != item.id)
            {
                throw std::runtime_error("Invalid held item identity");
            }
            const auto holder = std::find_if(session->actors_.begin(), session->actors_.end(),
                                             [&](const auto &a)
                                             {
                                                 return a.source.id == item.holder;
                                             });
            if (holder == session->actors_.end() || holder->source.character_profile.empty())
            {
                throw std::runtime_error("Invalid held item holder");
            }
        }
        for (auto &a : session->actors_)
        {
            if (!a.source.character_profile.empty())
            {
                a.definition = session->equipped_definition(a, session->items_);
            }
        }
    }
    bool pending_check{};
    input >> pending_check;
    if (pending_check)
    {
        PendingCheck c;
        input >> c.actor >> c.target >> c.natural >> c.surge_spent >> c.guidance;
        if (!input || c.guidance < 0 || c.guidance > 4)
        {
            throw std::runtime_error("Invalid ability-check checkpoint");
        }
        session->check_choice_ = c;
    }
    bool pending_champion{};
    input >> pending_champion;
    if (pending_champion)
    {
        ChampionMove c;
        input >> c.actor >> c.target >> c.natural >> c.remaining >> c.spell >> c.origin.x >>
            c.origin.y >> c.shift >> c.toward >> c.withdraw;
        if (!input)
        {
            throw std::runtime_error("Invalid Champion movement checkpoint");
        }
        session->champion_move_ = c;
    }
    bool saved_graze{}, saved_light{}, saved_nick{};
    input >> saved_graze;
    if (saved_graze)
    {
        PendingGraze g;
        input >> g.actor >> g.target >> g.natural;
        session->graze_ = g;
    }
    input >> saved_light >> saved_nick;
    bool has_mastery{};
    input >> has_mastery;
    if (has_mastery)
    {
        PendingMastery m;
        unsigned kind{};
        input >> m.actor >> m.target >> kind >> m.natural >> m.ranged >> m.targeting >>
            std::quoted(m.weapon) >> m.origin.x >> m.origin.y >> m.thrown_item >> m.tactical;
        m.kind = static_cast<detail::Mastery>(kind);
        session->mastery_ = std::move(m);
    }
    const auto extra = [&](ChampionMove &c)
    {
        input >> c.triggered >> c.cleave >> c.helpless >> c.trigger_origin.x >>
            c.trigger_origin.y >> c.target_origin.x >> c.target_origin.y;
    };
    unsigned offers{};
    input >> offers;
    if (!input || offers > 2)
    {
        throw std::runtime_error("Invalid queued effect count");
    }
    for (unsigned i = 0; i < offers; ++i)
    {
        ChampionMove c;
        input >> c.actor >> c.target >> c.natural >> c.remaining >> c.spell >> c.origin.x >>
            c.origin.y;
        extra(c);
        session->champion_offers_.push_back(c);
    }
    if (session->champion_move_)
    {
        extra(*session->champion_move_);
    }
    bool origin{};
    input >> origin;
    if (origin)
    {
        EffectReaction r;
        input >> r.actor >> r.source.x >> r.source.y >> r.mover.x >> r.mover.y >> r.movement >>
            r.prone;
        session->effect_reaction_origin_ = r;
    }
    unsigned choices{};
    input >> choices;
    if (!input || choices > session->actors_.size())
    {
        throw std::runtime_error("Invalid Initiative choice count");
    }
    for (unsigned i = 0; i < choices; ++i)
    {
        EntityId id{};
        input >> id;
        session->initiative_choices_.push_back(id);
    }
    bool selecting{};
    input >> selecting;
    if (selecting)
    {
        PendingSelection selection;
        std::size_t chosen_count{};
        input >> selection.caster >> selection.verb >> chosen_count;
        const auto *spell = detail::find_spell(selection.verb);
        if (!input || !spell || !selects_creatures(*spell) || !chosen_count || chosen_count > 8)
        {
            throw std::runtime_error("Invalid spell target choice");
        }
        for (std::size_t n = 0; n < chosen_count; ++n)
        {
            EntityId id{};
            input >> id;
            selection.chosen.push_back(id);
        }
        const auto known = [&](EntityId id)
        {
            return std::any_of(session->actors_.begin(), session->actors_.end(),
                               [&](const auto &a)
                               {
                                   return a.source.id == id;
                               });
        };
        if (!input || !known(selection.caster) ||
            !std::all_of(selection.chosen.begin(), selection.chosen.end(), known) ||
            chosen_count >= session->selection_maximum(selection))
        {
            throw std::runtime_error("Invalid spell target choice");
        }
        session->selection_ = std::move(selection);
    }
    const auto known_actor = [&](EntityId id)
    {
        return std::any_of(session->actors_.begin(), session->actors_.end(),
                           [&](const auto &a)
                           {
                               return a.source.id == id;
                           });
    };
    // A smite chance names a creature in the fight.
    for (const auto &a : session->actors_)
    {
        if (a.smite_target && !known_actor(a.smite_target))
        {
            throw std::runtime_error("Invalid smite target");
        }
    }
    bool aiming{};
    input >> aiming;
    if (aiming)
    {
        PendingArea area;
        std::size_t cubes{};
        input >> area.caster >> area.verb >> area.center.x >> area.center.y >> cubes;
        const auto *spell = detail::find_spell(area.verb);
        if (!input || !spell ||
            (spell->target != detail::SpellTarget::area &&
             spell->rider != detail::Rider::telekinesis) ||
            !known_actor(area.caster) || !session->board_.contains(area.center) ||
            session->selection_ || cubes > spell->storm || (spell->storm && !cubes))
        {
            throw std::runtime_error("Invalid area aim");
        }
        for (std::size_t n = 0; n < cubes; ++n)
        {
            Cell cube;
            input >> cube.x >> cube.y;
            if (!input || !session->board_.contains(cube) || cube.x % 2 || cube.y % 2)
            {
                throw std::runtime_error("Invalid area aim");
            }
            area.cubes.push_back(cube);
        }
        input >> area.companion;
        if (!input ||
            (area.companion &&
             (!known_actor(area.companion) || (spell->rider != detail::Rider::dimension_door &&
                                               spell->rider != detail::Rider::telekinesis))) ||
            (spell->rider == detail::Rider::telekinesis && !area.companion))
        {
            throw std::runtime_error("Invalid area aim");
        }
        session->area_ = std::move(area);
    }
    std::size_t zones{};
    input >> zones;
    if (!input || zones > 4 * session->actors_.size())
    {
        throw std::runtime_error("Invalid spell zones");
    }
    for (std::size_t n = 0; n < zones; ++n)
    {
        Zone zone;
        std::size_t cells{};
        unsigned kind{};
        input >> zone.caster >> kind >> zone.ends_ms >> zone.save_dc >> zone.dealt >> zone.warded >>
            zone.slot >> cells;
        // Only Grease, Guardian of Faith and Plant Growth keep time; every
        // other zone ends with Concentration. Grease outlasts a caster who has
        // since run off the field.
        const bool timed = kind == static_cast<unsigned>(ZoneKind::grease) ||
                           kind == static_cast<unsigned>(ZoneKind::guardian) ||
                           kind == static_cast<unsigned>(ZoneKind::overgrowth) ||
                           kind == static_cast<unsigned>(ZoneKind::hail) ||
                           kind == static_cast<unsigned>(ZoneKind::hound) ||
                           kind == static_cast<unsigned>(ZoneKind::sanctuary);
        // A Magic Circle is held by Concentration while it is cast, then keeps time.
        const bool circle = kind == static_cast<unsigned>(ZoneKind::circle);
        if (!input || (!zone.ends_ms && !known_actor(zone.caster)) ||
            kind > static_cast<unsigned>(ZoneKind::wave) || zone.slot > 9 ||
            (zone.slot && kind != static_cast<unsigned>(ZoneKind::animals) &&
             kind != static_cast<unsigned>(ZoneKind::beings) &&
             kind != static_cast<unsigned>(ZoneKind::guardians) &&
             kind != static_cast<unsigned>(ZoneKind::swarm) &&
             kind != static_cast<unsigned>(ZoneKind::blades) &&
             kind != static_cast<unsigned>(ZoneKind::pillar) &&
             kind != static_cast<unsigned>(ZoneKind::tentacles) &&
             kind != static_cast<unsigned>(ZoneKind::fire_wall) &&
             kind != static_cast<unsigned>(ZoneKind::fire_side) &&
             kind != static_cast<unsigned>(ZoneKind::elementals) &&
             kind != static_cast<unsigned>(ZoneKind::hand) &&
             kind != static_cast<unsigned>(ZoneKind::cloud) &&
             kind != static_cast<unsigned>(ZoneKind::embers) &&
             kind != static_cast<unsigned>(ZoneKind::elemental) &&
             kind != static_cast<unsigned>(ZoneKind::globe) &&
             kind != static_cast<unsigned>(ZoneKind::bead) &&
             kind != static_cast<unsigned>(ZoneKind::storm) &&
             kind != static_cast<unsigned>(ZoneKind::fey) &&
             kind != static_cast<unsigned>(ZoneKind::thorns) &&
             kind != static_cast<unsigned>(ZoneKind::spiritual_weapon) &&
             kind != static_cast<unsigned>(ZoneKind::flaming_sphere) &&
             kind != static_cast<unsigned>(ZoneKind::moonbeam) &&
             kind != static_cast<unsigned>(ZoneKind::wave)) ||
            (!circle && timed != (zone.ends_ms != 0)) || zone.warded >= detail::ward_types.size() ||
            (zone.warded && !circle) || zone.dealt < 0 || zone.dealt >= 60 ||
            (zone.dealt && kind != static_cast<unsigned>(ZoneKind::guardian) &&
             kind != static_cast<unsigned>(ZoneKind::bead) &&
             kind != static_cast<unsigned>(ZoneKind::sword) &&
             kind != static_cast<unsigned>(ZoneKind::fey) &&
             kind != static_cast<unsigned>(ZoneKind::wave)) ||
            (kind == static_cast<unsigned>(ZoneKind::wave) &&
             static_cast<std::size_t>(zone.dealt) >= wave_headings.size()) ||
            !cells || cells > session->board_.terrain.size())
        {
            throw std::runtime_error("Invalid spell zone");
        }
        zone.kind = static_cast<ZoneKind>(kind);
        for (std::size_t c = 0; c < cells; ++c)
        {
            Cell cell;
            input >> cell.x >> cell.y;
            if (!input || !session->board_.contains(cell))
            {
                throw std::runtime_error("Invalid spell zone");
            }
            zone.cells.push_back(cell);
        }
        session->zones_.push_back(std::move(zone));
    }
    bool asking{};
    input >> asking;
    if (asking)
    {
        PendingReaction prompt;
        auto &question = prompt.question;
        auto &command = prompt.command;
        unsigned asked{};
        std::size_t answers{};
        input >> question.target >> asked >> question.deflectable >> question.critical >>
            question.total >> question.armor_class >> command.actor >> command.target >>
            std::quoted(command.verb) >> command.destination.x >> command.destination.y >>
            command.item >> answers;
        if (!input || !known_actor(question.target) ||
            asked > static_cast<unsigned>(Asked::counter) || !known_actor(command.actor) ||
            (command.target && !known_actor(command.target)) || command.verb.empty() ||
            answers > 3 * session->actors_.size())
        {
            throw std::runtime_error("Invalid reaction prompt");
        }
        question.asked = static_cast<Asked>(asked);
        for (std::size_t n = 0; n < answers; ++n)
        {
            ReactionAnswer answer;
            unsigned kind{};
            input >> answer.target >> kind >> std::quoted(answer.verb);
            if (!input || !known_actor(answer.target) ||
                kind > static_cast<unsigned>(Asked::counter) ||
                (answer.verb != "shield" && answer.verb != "deflect" && answer.verb != "redirect" &&
                 answer.verb != "rebuke" && answer.verb != "giant_boon" &&
                 answer.verb != "stones_endurance" && answer.verb != "storms_thunder" &&
                 answer.verb != "retaliate" && answer.verb != "uncanny_dodge" &&
                 answer.verb != "superior_defense" && answer.verb != "decline"))
            {
                throw std::runtime_error("Invalid reaction prompt");
            }
            answer.asked = static_cast<Asked>(kind);
            prompt.answers.push_back(std::move(answer));
        }
        session->reaction_prompt_ = std::move(prompt);
    }
    for (auto &a : session->actors_)
    {
        std::size_t carried{};
        input >> carried;
        // Monsters carry their potions, wands and scrolls on Hard and above
        // (DIFF-1).
        if (!input || carried > 64)
        {
            throw std::runtime_error("Invalid checkpoint inventory");
        }
        a.source.inventory.resize(carried);
        for (auto &item : a.source.inventory)
        {
            input >> item.inventory_id >> std::quoted(item.definition) >> item.quantity >>
                item.equipment_index;
        }
    }
    std::size_t departed{};
    input >> departed;
    if (!input || departed > 8)
    {
        throw std::runtime_error("Invalid checkpoint flight");
    }
    for (std::size_t n = 0; n < departed; ++n)
    {
        Departed gone;
        unsigned side{};
        input >> gone.source.id >> std::quoted(gone.source.definition) >>
            std::quoted(gone.source.name) >> side >> gone.source.cell.x >> gone.source.cell.y >>
            std::quoted(gone.source.character_profile) >> gone.hp >> gone.max_hp >>
            gone.armor_class >> gone.vitals.hit_points >> gone.vitals.dead >>
            std::quoted(gone.vitals.resources) >> std::quoted(gone.vitals.description) >>
            gone.surrendered;
        std::size_t gear_count{};
        input >> gear_count;
        if (!input || side > static_cast<unsigned>(Side::opposition))
        {
            throw std::runtime_error("Invalid checkpoint flight");
        }
        gone.source.side = static_cast<Side>(side);
        if (gone.hp < (gone.surrendered ? 0 : 1) || gone.hp > gone.max_hp ||
            (gone.surrendered && (gone.hp || gone.source.side == Side::party)) ||
            gone.vitals.dead || gear_count > detail::thrown_gear_items.size())
        {
            throw std::runtime_error("Invalid checkpoint flight");
        }
        gone.thrown_gear_left.resize(gear_count);
        for (auto &[gear, left] : gone.thrown_gear_left)
        {
            input >> std::quoted(gear) >> left;
        }
        std::size_t item_count{};
        input >> item_count;
        if (!input || item_count > 64)
        {
            throw std::runtime_error("Invalid checkpoint flight");
        }
        gone.item_uses.resize(item_count);
        for (auto &[id, left] : gone.item_uses)
        {
            input >> id >> left;
        }
        session->fled_.push_back(std::move(gone));
    }
    if (session->actors_.size() + session->fled_.size() < 2)
    {
        throw std::runtime_error("Invalid checkpoint flight");
    }
    if (!input)
    {
        throw std::runtime_error("Invalid checkpoint continuation");
    }
    for (auto &a : session->actors_)
    {
        refresh_form(a);
        // Movement is bounded by the current Speed, a Wild Shape form's included,
        // once for each movement allowance the turn has had, and what is left of
        // Instinctive Pounce.
        if (a.movement > session->def(a).speed * (1 + a.dashes) + a.pounce)
        {
            throw std::runtime_error("Invalid Dash allowance count");
        }
    }
    session->light_active_ = saved_light;
    session->nick_active_ = saved_nick;
    session->validate_light();
    session->validate_restored_state();
    input >> std::ws;
    if (!input.eof())
    {
        throw std::runtime_error("Trailing checkpoint data");
    }
    // A checkpoint keeps suppressed effects with the others.
    session->suppress_spells();
    return session;
}
} // namespace opengold::srd5::detail
