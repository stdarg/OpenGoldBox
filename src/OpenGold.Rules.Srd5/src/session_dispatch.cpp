#include "session.h"
#include "scope_guards.h"
#include <tuple>

// Session: a submitted command's dispatch, reactions and pending checks.
namespace opengold::srd5::detail
{
using namespace rules;

void Session::remember_target(const Command &command)
{
    const auto find = [&](rules::EntityId id)
    {
        return std::find_if(actors_.begin(), actors_.end(),
                            [&](const Actor &a)
                            {
                                return a.source.id == id;
                            });
    };
    const auto user = find(command.actor), target = find(command.target);
    if (command.target && user != actors_.end() && target != actors_.end() &&
        target->source.side != user->source.side)
    {
        user->last_target = command.target;
    }
}

bool Session::submit(const Command &command)
{
    const auto offered = legal_commands();
    if (std::none_of(offered.begin(), offered.end(),
                     [&](const auto &c)
                     {
                         return same_command(c, command);
                     }))
    {
        return false;
    }
    // A command applies completely or not at all: an exception part way
    // through restores the session as it was (Effective C++ Item 29). The
    // restore moves, so it cannot itself fail part way.
    static_assert(std::is_nothrow_move_assignable_v<Session>);
    Session before = *this;
    rolled_dice_.clear();
    const DiceTally tally(rolled_dice_);
    // A copy keeps no spare room; a summoning must not move the actors.
    actors_.reserve(actors_.size() + 64);
    try
    {
        if (reaction_prompt_)
        {
            answer_reaction(command);
        }
        else
        {
            remember_target(command);
            perform(command);
        }
        limit_slowed_turn();
        shed_immune_conditions();
        if (!reaction_prompt_)
        {
            settle_dominated();
        }
        notice_hidden();
        // Revisions are command tickets; zero is reserved for invalid commands.
        // Unsigned wrap is defined, but must skip that reserved value.
        if (++revision_ == 0)
        {
            revision_ = 1;
        }
        update_outcome();
        // One who ran off the field ends its turn and leaves the fight.
        if (outcome_ == Outcome::ongoing && !pending() && !reaction_prompt_ && actors_[turn_].fled)
        {
            end_turn();
        }
        remove_fled();
        update_outcome();
        if (initiative_choices_.empty() && outcome_ == Outcome::ongoing && !pending() &&
            !reaction_prompt_ && !champion_move_ && !effect_waiting() &&
            actors_[turn_].life.hp == 0)
        {
            end_turn();
            // A creature whose morale broke as its turn began leaves now too.
            remove_fled();
            update_outcome();
        }
        if (outcome_ != Outcome::ongoing)
        {
            advance_turn_time();
        }
        suppress_spells();
    }
    catch (...)
    {
        *this = std::move(before);
        throw;
    }
    return true;
}

const Session::ReactionAnswer *Session::answer_of(EntityId target, Asked asked) const
{
    for (const auto &answer : reaction_answers_)
    {
        if (answer.target == target && answer.asked == asked)
        {
            return &answer;
        }
    }
    return nullptr;
}

// Shield takes a slot, unless Magic Initiate's free cast pays for it.
bool Session::can_shield(const Actor &target) const
{
    const bool slot = lowest_slot(target, 1) && !target.spent_slot;
    const bool free = (target.initiate_casts & initiate_bit(def(target), "shield")) != 0;
    return conscious(target) && target.reaction && (slot || free) &&
           detail::knows_spell(def(target).spells, "shield") &&
           !detail::incapacitated(target.effects) &&
           !detail::has_effect(target.effects, detail::EffectKind::shield);
}

bool Session::can_defend(const Actor &target) const
{
    // Superior Hunter's Defense: the Reaction when an attack would damage it.
    return def(target).superior_defense && conscious(target) && target.reaction &&
           !detail::incapacitated(target.effects);
}

bool Session::can_dodge(const Actor &target) const
{
    // Uncanny Dodge: the attacker must be seen; a Blinded Rogue sees no one.
    return def(target).uncanny_dodge && conscious(target) && target.reaction &&
           !detail::incapacitated(target.effects) && !detail::blinded(target.effects);
}

bool Session::can_deflect(const Actor &target, bool deflectable) const
{
    return (deflectable || def(target).deflect_energy) && def(target).deflect &&
           conscious(target) && target.reaction && !detail::incapacitated(target.effects);
}

void Session::ask_reaction(const Actor &target, Asked asked, bool deflectable, bool critical,
                           int total, int armor_class) const
{
    // Unwinds the command to perform(), which asks the creature. A creature
    // asks once per command about the same kind of moment.
    if (answer_of(target.source.id, asked))
    {
        return;
    }
    const bool askable =
        asked == Asked::redirect || asked == Asked::rebuke || asked == Asked::boon ||
        asked == Asked::endure || asked == Asked::thunder || asked == Asked::retaliate ||
        asked == Asked::counter || (can_shield(target) && !critical) ||
        (asked == Asked::hit &&
         (can_deflect(target, deflectable) || can_dodge(target) || can_defend(target)));
    if (askable)
    {
        throw ReactionQuestion{target.source.id, asked, deflectable, critical, total, armor_class};
    }
}

int Session::dodged(Actor &target, int amount)
{
    const auto found = std::find_if(reaction_answers_.begin(), reaction_answers_.end(),
                                    [&](const auto &answer)
                                    {
                                        return answer.target == target.source.id &&
                                               answer.asked == Asked::hit &&
                                               answer.verb == "uncanny_dodge";
                                    });
    if (found == reaction_answers_.end())
    {
        return amount;
    }
    // Spent once: a later hit in the same command is taken in full.
    found->verb = "dodged";
    log(target.source.name + " uses Uncanny Dodge and takes half the damage.",
        {"{name} uses Uncanny Dodge and takes half the damage.", {{"name", target.source.name}}});
    return amount / 2;
}

int Session::deflected(const Actor &attacker, Actor &target, int amount, detail::DamageType type,
                       bool ranged)
{
    // Deflect Attacks: 1d10 + Dexterity + Monk level less damage. Brought to 0,
    // a Focus Point redirects the force at the attacker, if it is near enough.
    const auto found = std::find_if(reaction_answers_.begin(), reaction_answers_.end(),
                                    [&](const auto &answer)
                                    {
                                        return answer.target == target.source.id &&
                                               answer.asked == Asked::hit &&
                                               answer.verb == "deflect";
                                    });
    if (found == reaction_answers_.end())
    {
        return amount;
    }
    // Spent once: a later hit in the same command is not deflected.
    found->verb = "deflected";
    const auto &d = def(target);
    const int reduction = roll(10) + d.dexterity + d.class_levels[CharacterClass::monk];
    const int left = std::max(0, amount - reduction);
    log(target.source.name + " deflects " + std::to_string(amount - left) + " damage.",
        {"{name} deflects {damage} damage.",
         {{"name", target.source.name}, {"damage", std::to_string(amount - left)}}});
    const int reach = ranged ? 60 : 5;
    if (left || target.surges[CharacterClass::monk] <= 0 || attacker.life.hp <= 0 ||
        distance(target, attacker) > reach || !can_see(target, attacker))
    {
        return left;
    }
    ask_reaction(target, Asked::redirect);
    if (answer_of(target.source.id, Asked::redirect)->verb != "redirect")
    {
        return left;
    }
    --target.surges[CharacterClass::monk];
    auto &foe = actor(attacker.source.id);
    log(target.source.name + " redirects the attack at " + foe.source.name + ".",
        {"{name} redirects the attack at {target}.",
         {{"name", target.source.name}, {"target", foe.source.name}}});
    if (!saving_throw_succeeds(foe, detail::Ability::dexterity, d.focus_dc))
    {
        const int force = resolved_damage(foe, type, dice({2, 6, d.dexterity}));
        log(foe.source.name + " takes " + std::to_string(force) + " damage.",
            {"{name} takes {damage} damage.",
             {{"name", foe.source.name}, {"damage", std::to_string(force)}}});
        damage(foe, force);
    }
    return left;
}

void Session::bless_fiends(const Actor &fallen)
{
    // Dark One's Blessing: a Fiend Warlock gains Temporary Hit Points when it
    // drops an enemy, or someone drops one within 10 feet of it. The creature
    // acting (or reacting) now is taken as the one who dropped it, and the
    // better Temporary Hit Points are kept, as with Heroism.
    const auto by = pending() ? pending() : actors_[turn_].source.id;
    for (auto &warlock : actors_)
    {
        const int amount = def(warlock).dark_ones_blessing;
        if (!amount || warlock.life.hp <= 0 || warlock.life.dead ||
            warlock.source.side == fallen.source.side ||
            (by != warlock.source.id && distance(warlock, fallen) > 10) ||
            amount <= warlock.life.temporary_hp.amount)
        {
            continue;
        }
        detail::grant_temporary_hp(warlock.life, {amount, "feature:dark_ones_blessing"},
                                   TemporaryHpChoice::use_new);
        log("Dark One's Blessing: " + warlock.source.name + " gains " + std::to_string(amount) +
                " Temporary Hit Points.",
            {"Dark One's Blessing: {name} gains {amount} Temporary Hit Points.",
             {{"name", warlock.source.name}, {"amount", std::to_string(amount)}}});
    }
}

int Session::inspiration_roll(const Actor &creature, int shortfall)
{
    // Used without asking, and only when the die could make up the shortfall:
    // the roll and its target number are in the log for the player too.
    auto &inspired = actor(creature.source.id);
    const auto found = std::find_if(inspired.effects.active.begin(), inspired.effects.active.end(),
                                    [](const auto &e)
                                    {
                                        return e.kind == detail::EffectKind::inspired;
                                    });
    if (found == inspired.effects.active.end() || !conscious(inspired) || found->dc < shortfall)
    {
        return 0;
    }
    const int sides = found->dc;
    inspired.effects.active.erase(found);
    const int die = roll(sides);
    log_inspiration(inspired, die, die >= shortfall);
    return die;
}

void Session::log_inspiration(const Actor &inspired, int die, bool success)
{
    const auto amount = std::to_string(die);
    log("Bardic Inspiration adds " + amount + " to " + inspired.source.name + "'s roll" +
            (success ? ", turning it into a success." : ", not enough to succeed."),
        {success ? "Bardic Inspiration adds {amount} to {name}'s roll, turning it into a success."
                 : "Bardic Inspiration adds {amount} to {name}'s roll, not enough to succeed.",
         {{"amount", amount}, {"name", inspired.source.name}}});
}

Actor *Session::cutting_bard(const Actor &roller)
{
    // A Lore Bard who sees the roller within 60 feet spends a Reaction and a
    // Bardic Inspiration use to subtract the die from the roll.
    for (auto &bard : actors_)
    {
        if (def(bard).cutting_words && bard.source.side != roller.source.side && conscious(bard) &&
            bard.reaction && !detail::incapacitated(bard.effects) &&
            bard.arcane[CharacterClass::bard] > 0 && distance(bard, roller) <= 60 &&
            can_see(bard, roller))
        {
            return &bard;
        }
    }
    return nullptr;
}

int Session::cutting_words(const Actor &roller, int margin, CutRoll cut)
{
    // It is used without asking, and only when the die could turn the success
    // into a failure.
    auto *const bard = cutting_bard(roller);
    if (!bard || margin >= def(*bard).bardic_die)
    {
        return 0;
    }
    bard->reaction = false;
    --bard->arcane[CharacterClass::bard];
    const int die = roll(def(*bard).bardic_die);
    const bool failed = die > margin;
    const auto amount = std::to_string(die);
    const bool attack = cut == CutRoll::attack;
    const std::string outcome =
        attack ? (failed ? ", turning the hit into a miss." : "; the attack still hits.")
               : (failed ? ", turning the success into a failure." : "; the check still succeeds.");
    log(bard->source.name + " uses Cutting Words: -" + amount + outcome,
        {attack
             ? (failed ? "{name} uses Cutting Words: -{amount}, turning the hit into a miss."
                       : "{name} uses Cutting Words: -{amount}; the attack still hits.")
             : (failed ? "{name} uses Cutting Words: -{amount}, turning the success into a failure."
                       : "{name} uses Cutting Words: -{amount}; the check still succeeds."),
         {{"name", bard->source.name}, {"amount", amount}}});
    return die;
}

int Session::cut_damage(const Actor &roller, const Actor &target, int amount,
                        detail::DamageType type)
{
    // Used without asking, and only when the roll would drop the target to 0
    // Hit Points and the d6 could keep it standing (CUT-1). The estimate
    // counts resistances and Temporary Hit Points, nothing rolled later.
    std::vector<detail::DamageAffinity> added;
    const auto defenses = affinities(target, added);
    const auto taken = [&](int rolled)
    {
        const std::array parts{detail::DamagePart{type, std::max(0, rolled)}};
        return detail::resolve_damage(parts, defenses).total;
    };
    const int standing = target.life.hp + target.life.temporary_hp.amount;
    auto *const bard = cutting_bard(roller);
    if (!bard || bard->source.side != target.source.side || target.life.hp <= 0 ||
        taken(amount) < standing || taken(amount - def(*bard).bardic_die) >= standing)
    {
        return 0;
    }
    bard->reaction = false;
    --bard->arcane[CharacterClass::bard];
    const int die = roll(def(*bard).bardic_die);
    const bool saved = taken(amount - die) < standing;
    const auto cut = std::to_string(die);
    log(bard->source.name + " uses Cutting Words: -" + cut + " damage" +
            (saved ? ", keeping " + target.source.name + " standing."
                   : ", not enough to save " + target.source.name + "."),
        {saved ? "{name} uses Cutting Words: -{amount} damage, keeping {target} standing."
               : "{name} uses Cutting Words: -{amount} damage, not enough to save {target}.",
         {{"name", bard->source.name}, {"amount", cut}, {"target", target.source.name}}});
    return die;
}

bool Session::can_rebuke(const Actor &warlock, const Actor &attacker) const
{
    // A slot, or an Infernal Tiefling's free cast.
    const bool free = (warlock.initiate_casts & initiate_bit(def(warlock), "hellish_rebuke")) != 0;
    return warlock.source.side != attacker.source.side && conscious(warlock) && warlock.reaction &&
           !detail::incapacitated(warlock.effects) &&
           (free || (lowest_slot(warlock, 1) && !warlock.spent_slot)) &&
           detail::knows_spell(def(warlock).spells, "hellish_rebuke") &&
           distance(warlock, attacker) <= 60 && can_see(warlock, attacker) && !attacker.life.dead;
}

void Session::rebuke(const Actor &attacker, Actor &warlock)
{
    // Hellish Rebuke: the attacker saves against 2d10 Fire, 1d10 more for
    // each slot level above 1, taking half on a success.
    if (!can_rebuke(warlock, attacker))
    {
        return;
    }
    ask_reaction(warlock, Asked::rebuke);
    if (answer_of(warlock.source.id, Asked::rebuke)->verb != "rebuke")
    {
        return;
    }
    warlock.reaction = false;
    // The free cast first: it can pay for nothing else.
    const auto bit = initiate_bit(def(warlock), "hellish_rebuke");
    const bool free = (warlock.initiate_casts & bit) != 0;
    const unsigned level = free ? 1 : lowest_slot(warlock, 1);
    if (free)
    {
        warlock.initiate_casts &= ~bit;
    }
    else
    {
        warlock.spent_slot = true;
        --warlock.slots[level];
    }
    auto &foe = actor(attacker.source.id);
    end_hiding(warlock.source.id);
    log(warlock.source.name + " casts Hellish Rebuke at " + foe.source.name + ".",
        {"{name} casts Hellish Rebuke at {target}.",
         {{"name", warlock.source.name}, {"target", foe.source.name}}});
    const bool saved =
        saving_throw_succeeds(foe, detail::Ability::dexterity, spell_dc(warlock, "hellish_rebuke"));
    const int rolled = dice({static_cast<int>(level) + 1, 10, 0});
    const int amount = resolved_damage(foe, detail::DamageType::fire, saved ? rolled / 2 : rolled);
    log(foe.source.name + " takes " + std::to_string(amount) + " Fire damage.",
        {"{name} takes {damage} {type} damage.",
         {{"name", foe.source.name}, {"damage", std::to_string(amount)}, {"type", "Fire", true}}});
    damage(foe, amount);
}

bool Session::can_use_giant_reaction(const Actor &goliath, GiantBoon boon) const
{
    return def(goliath).giant == boon && conscious(goliath) && goliath.reaction &&
           !detail::incapacitated(goliath.effects) && trait_uses_left(goliath) > 0;
}

void Session::giant_boon(const Actor &attacker, Actor &target, bool critical)
{
    // Fire's Burn, Frost's Chill and Hill's Tumble follow a damaging hit with
    // an attack roll, at most once in a command (Scorching Ray's rays).
    const auto boon = def(attacker).giant;
    if ((boon != GiantBoon::fire && boon != GiantBoon::frost && boon != GiantBoon::hill) ||
        trait_uses_left(attacker) <= 0 ||
        (boon == GiantBoon::hill && (def(target).size > 3 || target.effects.prone)))
    {
        return;
    }
    ask_reaction(attacker, Asked::boon);
    const auto used = std::find_if(reaction_answers_.begin(), reaction_answers_.end(),
                                   [&](const auto &answer)
                                   {
                                       return answer.target == attacker.source.id &&
                                              answer.asked == Asked::boon &&
                                              answer.verb == "giant_boon";
                                   });
    if (used == reaction_answers_.end())
    {
        return;
    }
    used->verb = "spent";
    auto &goliath = actor(attacker.source.id);
    spend_trait_use(goliath);
    const auto label = std::string(giant_boon_label(boon));
    log(goliath.source.name + " uses " + label + ".",
        {"{name} uses {boon}.", {{"name", goliath.source.name}, {"boon", label, true}}});
    if (boon == GiantBoon::hill)
    {
        target.effects.prone = true;
        log(target.source.name + " is knocked Prone.",
            {"{name} is knocked Prone.", {{"name", target.source.name}}});
        return;
    }
    // The boon's die is part of the hit's damage, so a Critical Hit doubles it.
    const auto type = boon == GiantBoon::fire ? detail::DamageType::fire : detail::DamageType::cold;
    const int amount =
        resolved_damage(target, type, dice({1, boon == GiantBoon::fire ? 10 : 6, 0}, critical));
    const auto name = std::string(detail::damage_name(type));
    log(target.source.name + " takes " + std::to_string(amount) + " " + name + " damage.",
        {"{name} takes {damage} {type} damage.",
         {{"name", target.source.name}, {"damage", std::to_string(amount)}, {"type", name, true}}});
    if (boon == GiantBoon::frost && detail::can_apply(target.effects))
    {
        detail::apply_frosts_chill(target.effects, effect_source(goliath), goliath.source.name,
                                   next_turn_ms(goliath));
        log(target.source.name + "'s Speed drops by 10 feet.",
            {"{name}'s Speed drops by 10 feet.", {{"name", target.source.name}}});
    }
    damage(target, amount, false);
}

int Session::endured(Actor &goliath, int amount)
{
    // Stone's Endurance: 1d12 + Constitution modifier less damage.
    if (amount <= 0 || !askable_ || !can_use_giant_reaction(goliath, GiantBoon::stone))
    {
        return amount;
    }
    ask_reaction(goliath, Asked::endure);
    if (answer_of(goliath.source.id, Asked::endure)->verb != "stones_endurance")
    {
        return amount;
    }
    goliath.reaction = false;
    spend_trait_use(goliath);
    // A negative modifier can bring the total to nothing, never to more damage.
    const int lessened = std::max(0, roll(12) + def(goliath).constitution);
    const int left = std::max(0, amount - lessened);
    log(goliath.source.name + " uses Stone's Endurance: " + std::to_string(amount - left) +
            " less damage.",
        {"{name} uses Stone's Endurance: {damage} less damage.",
         {{"name", goliath.source.name}, {"damage", std::to_string(amount - left)}}});
    return left;
}

void Session::storms_thunder(const Actor &attacker, Actor &goliath)
{
    // Storm's Thunder: 1d8 Thunder damage to a creature within 60 feet that
    // damaged the Goliath.
    if (attacker.source.side == goliath.source.side || attacker.life.dead ||
        attacker.life.hp <= 0 || distance(goliath, attacker) > 60 ||
        !can_use_giant_reaction(goliath, GiantBoon::storm))
    {
        return;
    }
    ask_reaction(goliath, Asked::thunder);
    if (answer_of(goliath.source.id, Asked::thunder)->verb != "storms_thunder")
    {
        return;
    }
    goliath.reaction = false;
    spend_trait_use(goliath);
    auto &foe = actor(attacker.source.id);
    const int amount = resolved_damage(foe, detail::DamageType::thunder, roll(8));
    log(goliath.source.name + " uses Storm's Thunder: " + foe.source.name + " takes " +
            std::to_string(amount) + " Thunder damage.",
        {"{name} uses Storm's Thunder: {target} takes {damage} Thunder damage.",
         {{"name", goliath.source.name},
          {"target", foe.source.name},
          {"damage", std::to_string(amount)}}});
    damage(foe, amount);
}

void Session::retaliate(const Actor &attacker, Actor &berserker)
{
    // Retaliation: a melee attack, with the Reaction, against a creature
    // within 5 feet that damaged the Berserker.
    if (!def(berserker).retaliation || attacker.source.side == berserker.source.side ||
        !conscious(berserker) || !berserker.reaction || detail::incapacitated(berserker.effects) ||
        attacker.life.dead || attacker.life.hp <= 0 || distance(berserker, attacker) > 5)
    {
        return;
    }
    ask_reaction(berserker, Asked::retaliate);
    if (answer_of(berserker.source.id, Asked::retaliate)->verb != "retaliate")
    {
        return;
    }
    berserker.reaction = false;
    log(berserker.source.name + " retaliates.",
        {"{name} retaliates.", {{"name", berserker.source.name}}});
    attack(berserker, actor(attacker.source.id), AttackRange::melee);
}

void Session::clouds_jaunt(Actor &goliath, Cell destination)
{
    // Teleporting provokes no Opportunity Attacks.
    goliath.bonus = false;
    spend_trait_use(goliath);
    goliath.source.cell = destination;
    clear_departed_overlaps();
    log(goliath.source.name + " uses Cloud's Jaunt.",
        {"{name} uses Cloud's Jaunt.", {{"name", goliath.source.name}}});
}

void Session::answer_damage()
{
    // Indexed: a Reaction's own damage is recorded too, though only against
    // the creature acting, which never answers itself.
    for (std::size_t n = 0; n < damage_taken_.size(); ++n)
    {
        const auto [target, source] = damage_taken_[n];
        rebuke(actor(source), actor(target));
        storms_thunder(actor(source), actor(target));
        retaliate(actor(source), actor(target));
    }
    damage_taken_.clear();
}

void Session::perform(const Command &command)
{
    take_up_weapon(actor(command.actor), command);
    casting_metamagic_.clear();
    heightened_target_ = 0;
    overchanneling_ = false;
    const bool askable =
        std::any_of(actors_.begin(), actors_.end(),
                    [&](const auto &other)
                    {
                        return can_shield(other) || def(other).deflect ||
                               detail::knows_spell(def(other).spells, "hellish_rebuke") ||
                               detail::knows_spell(def(other).spells, "counterspell") ||
                               def(other).giant != GiantBoon::none || def(other).retaliation ||
                               def(other).uncanny_dodge || def(other).superior_defense;
                    });
    damage_taken_.clear();
    const detail::SetWhileAlive performing{performing_};
    if (!askable)
    {
        dispatch_tracking_turn(command);
        overchannel_backlash();
        ward_nature();
        damage_taken_.clear();
        reaction_answers_.clear();
        return;
    }
    // Undo everything the command did before the hit: the replay rolls the
    // same dice from the same state. The restore moves, as submit()'s does.
    Session before = *this;
    try
    {
        askable_ = true;
        dispatch_tracking_turn(command);
        overchannel_backlash();
        ward_nature();
        answer_damage();
        askable_ = false;
        reaction_answers_.clear();
    }
    catch (const ReactionQuestion &question)
    {
        *this = std::move(before);
        reaction_prompt_ = PendingReaction{question, command, std::move(reaction_answers_)};
        reaction_answers_.clear();
    }
}

void Session::answer_reaction(const Command &command)
{
    auto prompt = std::move(*reaction_prompt_);
    reaction_prompt_.reset();
    reaction_answers_ = std::move(prompt.answers);
    reaction_answers_.push_back({prompt.question.target, prompt.question.asked, command.verb});
    auto &defender = actor(prompt.question.target);
    if (command.verb == "shield")
    {
        defender.reaction = false;
        // Magic Initiate's free cast first: it can pay for nothing else, while
        // a slot could.
        const auto &shield = *detail::find_spell("shield");
        const bool free = (defender.initiate_casts & initiate_bit(def(defender), "shield")) != 0;
        pay_for_spell(defender,
                      free ? "shield" + std::string(detail::initiate_suffix)
                           : "shield_" + std::to_string(lowest_slot(defender, 1)),
                      shield);
        detail::apply_poisoned(defender.effects, effect_source(defender), defender.source.name,
                               next_turn_ms(defender), detail::EffectKind::shield);
        end_hiding(defender.source.id);
        log(defender.source.name + " casts Shield.",
            {"{name} casts {spell}.", {{"name", defender.source.name}, {"spell", "Shield", true}}});
    }
    else if (command.verb == "deflect" || command.verb == "uncanny_dodge" ||
             command.verb == "superior_defense")
    {
        defender.reaction = false;
    }
    perform(prompt.command);
}

void Session::follow_carrier(const Actor &monk, Cell departed)
{
    if (!monk.carried || flight_)
    {
        return;
    }
    auto &ally = actor(monk.carried);
    if (!ally.life.dead && distance(ally, monk) <= 10)
    {
        ally.source.cell = departed;
    }
}

void Session::dispatch_tracking_turn(const Command &command)
{
    const auto acting = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto &a)
                                     {
                                         return a.source.id == command.actor;
                                     });
    if (acting == actors_.end())
    {
        dispatch(command);
        return;
    }
    const bool had_bonus = acting->bonus, had_action = acting->actions.normal,
               had_moved = acting->moved;
    dispatch(command);
    // Daze (Rogue 14) and Abjure Foes (Paladin 9): on its turn the creature
    // does only one of moving, an action or a Bonus Action; the first it does
    // takes away the others.
    if (auto &dazed = actor(command.actor);
        (detail::has_effect(dazed.effects, detail::EffectKind::dazed) ||
         detail::has_effect(dazed.effects, detail::EffectKind::abjured)) &&
        actors_[turn_].source.id == dazed.source.id)
    {
        if (!had_moved && dazed.moved)
        {
            dazed.actions = {.normal = false};
            dazed.bonus = false;
        }
        else if (had_action && !dazed.actions.normal)
        {
            dazed.movement = 0;
            dazed.bonus = false;
        }
        else if (had_bonus && !dazed.bonus)
        {
            dazed.movement = 0;
            dazed.actions = {.normal = false};
        }
    }
    if (!had_bonus)
    {
        return;
    }
    // Fleet Step (Monk 11): a Bonus Action other than Step of the Wind leaves
    // Step of the Wind for right after it.
    auto &monk = actor(command.actor);
    if (!monk.bonus && def(monk).fleet_step && !command.verb.starts_with("step_of_the_wind"))
    {
        monk.fleet_step = true;
    }
}

void Session::dispatch(const Command &command)
{
    if (command.verb == "lash_out")
    {
        const auto &a = actor(command.actor);
        const auto near = creatures_in_reach(a);
        log(a.source.name + " lashes out in panic.",
            {"{name} lashes out in panic.", {{"name", a.source.name}}});
        auto melee = command;
        melee.verb = "melee";
        melee.target = near[static_cast<std::size_t>(roll(static_cast<int>(near.size())) - 1)];
        dispatch(melee);
        return;
    }
    if (command.verb == "reckless")
    {
        attack_recklessly(actor(command.actor));
        auto melee = command;
        melee.verb = "melee";
        dispatch(melee);
        return;
    }
    if (const auto chosen = cunning_choice(command.verb))
    {
        cunning_ = PendingCunning{command.actor, chosen->first, chosen->second};
        auto plain = command;
        plain.verb = command.verb.starts_with("melee_") ? "melee" : "ranged";
        dispatch(plain);
        cunning_.reset();
        return;
    }
    if (command.verb == "melee_stun")
    {
        stunning_ = command.actor;
        auto melee = command;
        melee.verb = "melee";
        dispatch(melee);
        stunning_ = 0;
        return;
    }
    if (const auto found = std::find_if(brutal_blows.begin(), brutal_blows.end(),
                                        [&](const auto &option)
                                        {
                                            return std::get<0>(option) == command.verb;
                                        });
        found != brutal_blows.end())
    {
        actor(command.actor).brutal_used = true;
        brutal_ = BrutalStrike{command.actor, std::get<2>(*found)};
        auto melee = command;
        melee.verb = "melee";
        dispatch(melee);
        brutal_.reset();
        return;
    }
    // Tactical Master: Push, Sap or Slow in place of the weapon's mastery.
    for (const auto &option : tactical_masteries)
    {
        for (const std::string_view base : {"melee", "ranged"})
        {
            if (command.verb == std::string(base) + "_" + std::string(option.suffix))
            {
                tactical_mastery_ = TacticalMastery{command.actor, option.kind};
                auto plain = command;
                plain.verb = base;
                dispatch(plain);
                tactical_mastery_.reset();
                return;
            }
        }
    }
    if (command.verb == "melee_necrotic" || command.verb == "melee_psychic" ||
        command.verb == "melee_radiant" || command.verb == "melee_force")
    {
        pact_damage_ = detail::damage_type(std::string_view(command.verb).substr(6));
        auto melee = command;
        melee.verb = "melee";
        dispatch(melee);
        pact_damage_.reset();
        return;
    }
    auto &a = actor(command.actor);
    const auto &d = def(a);
    if (command.verb != "move" && command.verb != "melee")
    {
        a.run_feet = 0;
    }
    // Instinctive Pounce's movement belongs to entering the Rage; anything
    // else first gives up what is left of it.
    if (command.verb != "move" && a.pounce > 0)
    {
        a.movement = std::max(0, a.movement - a.pounce);
        a.pounce = 0;
    }
    // A smite must follow the hit at once. Resolving that hit's own weapon
    // mastery choice keeps the chance open; any other command ends it.
    if (!is_smite(command.verb) && !command.verb.starts_with("effect_") &&
        !command.verb.starts_with("divine_strike_") && command.verb != "eldritch_smite" &&
        !command.verb.starts_with("lifedrinker_") &&
        !command.verb.starts_with("hurl_through_hell") &&
        !command.verb.starts_with("primal_strike_"))
    {
        a.smite_target = 0;
    }
    if (turns_to_attack(command.verb) && command.target)
    {
        const auto &target = actor(command.target);
        if (target.source.cell.x != a.source.cell.x)
        {
            const bool new_left = target.source.cell.x < a.source.cell.x;
            if (new_left != a.facing_left)
            {
                a.facing_left = new_left;
                log(a.source.name + (new_left ? " turns left." : " turns right."));
            }
        }
    }
    if (!initiative_choices_.empty())
    {
        resolve_initiative(command);
    }
    else if (area_)
    {
        aim_area(command);
    }
    else if (selection_)
    {
        choose_target(command);
    }
    else if (!graze_ && !champion_move_ && effect_waiting())
    {
        use_effect(command);
    }
    else if (graze_)
    {
        const auto g = std::move(*graze_);
        graze_.reset();
        if (command.verb == "effect_use")
        {
            auto &target = actor(g.target);
            const int amount = graze_damage(a, target);
            log(a.source.name + " grazes " + target.source.name + " for " + std::to_string(amount) +
                    " damage.",
                {"{actor} grazes {target} for {damage} damage.",
                 {{"actor", a.source.name},
                  {"target", target.source.name},
                  {"damage", std::to_string(amount)}}});
            damage(target, amount, false);
        }
        if (pending())
        {
            finish_reaction();
        }
    }
    else if (champion_move_)
    {
        if (command.verb == "end")
        {
            finish_champion_move();
        }
        else
        {
            const auto grid = movement_grid(a);
            const auto path =
                grid.reachable(champion_move_->remaining).path_to(command.destination);
            for (const auto cell : path)
            {
                champion_move_->remaining -= *grid.step_cost(a.source.cell, cell);
                a.source.cell = cell;
                a.moved = true;
            }
            clear_departed_overlaps();
            if (champion_move_->remaining == 0)
            {
                finish_champion_move();
            }
        }
    }
    else if (command.verb == "mind_use" || command.verb == "mind_skip")
    {
        const auto check = std::move(*check_choice_);
        check_choice_.reset();
        const int boost = command.verb == "mind_use" ? roll(10) : 0;
        if (boost && check.natural + d.medicine + check.guidance + boost >= 10)
        {
            --a.winds;
        }
        finish_check(check, boost);
    }
    else if (command.verb == "stabilize")
    {
        PendingCheck check{
            a.source.id, command.target,
            reliable(a, "medicine", detail::d20({false, check_disadvantage(a), d.lucky}, rng_)),
            a.actions.surge};
        check.guidance = guidance_bonus(a, "medicine");
        a.nick_origin = 0;
        a.actions.spend();
        if (check.natural + d.medicine + check.guidance < 10 && d.tactical_mind && a.winds > 0)
        {
            check_choice_ = check;
        }
        else
        {
            finish_check(check, 0);
        }
    }
    else if (command.verb == "temp_hp_keep" || command.verb == "temp_hp_use")
    {
        detail::grant_temporary_hp(a.life, *temporary_offer_,
                                   command.verb == "temp_hp_keep" ? TemporaryHpChoice::keep_current
                                                                  : TemporaryHpChoice::use_new);
        temporary_offer_.reset();
    }
    else if (command.verb == "weapon_select")
    {
        activate_light();
        a.selected_weapon = command.item;
        a.definition = equipped_definition(a, items_);
        a.drawn_weapon.clear();
    }
    else if (command.verb.starts_with("light_") || command.verb.starts_with("nick_"))
    {
        const bool nick = command.verb.starts_with("nick_");
        if (!nick)
        {
            a.bonus = false;
        }
        if (nick_active_)
        {
            a.light_extra = nick ? 2 : 1;
        }
        if (command.verb == "light_throw" || command.verb == "nick_throw")
        {
            throw_weapon(a, actor(command.target), command.item, true);
        }
        else
        {
            auto attacker = item_loadout(a, command.item);
            attacker.light_damage = true;
            attack_as(a, std::move(attacker), actor(command.target),
                      command.verb == "light_ranged" || command.verb == "nick_ranged"
                          ? AttackRange::ranged
                          : AttackRange::melee);
        }
    }
    else if (command.verb == "stand_up")
    {
        a.movement -= std::max(0, d.speed - detail::speed_penalty(a.effects)) / 2;
        a.effects.prone = false;
    }
    else if (command.verb == "action_surge")
    {
        --a.surges[CharacterClass::fighter];
        a.surge_used = true;
        a.actions.surge = true;
        log(a.source.name + " uses Action Surge.",
            {"{name} uses Action Surge.", {{"name", a.source.name}}});
    }
    else if (command.verb == "steady_aim")
    {
        a.bonus = false;
        a.aim_used = a.aim_ready = true;
        log(a.source.name + " uses Steady Aim.",
            {"{name} uses Steady Aim.", {{"name", a.source.name}}});
    }
    else if (command.verb == "cunning_hide")
    {
        a.bonus = false;
        hide(a);
    }
    else if (command.verb == "cunning_dash" || command.verb == "cunning_disengage")
    {
        a.bonus = false;
        if (command.verb == "cunning_dash")
        {
            a.movement += d.speed;
            ++a.dashes;
            log(a.source.name + " dashes.", {"{name} dashes.", {{"name", a.source.name}}});
        }
        else
        {
            a.disengaged = true;
            log(a.source.name + " disengages.", {"{name} disengages.", {{"name", a.source.name}}});
        }
    }
    else if (command.verb == "bonus_charge")
    {
        // Only monsters charge and the policy spends it closing in, as with
        // Aggressive, so "straight toward an enemy" is not enforced.
        a.bonus = false;
        a.movement += d.speed / 2;
        ++a.dashes;
        log(a.source.name + " charges.", {"{name} charges.", {{"name", a.source.name}}});
    }
    else if (command.verb == "rampage")
    {
        std::erase_if(a.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::rampaging;
                      });
        attack(a, actor(command.target), AttackRange::melee);
    }
    else if (command.verb == "aggressive")
    {
        // Only monsters have Aggressive and the policy spends it closing in, so
        // "toward a hostile creature" is not enforced on the extra movement.
        a.bonus = false;
        a.movement += d.speed;
        ++a.dashes;
        log(a.source.name + " moves aggressively.",
            {"{name} moves aggressively.", {{"name", a.source.name}}});
    }
    else if (command.verb == "divine_spark" || command.verb == "divine_spark_radiant")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(false);
        --a.channel_divinity[CharacterClass::cleric];
        divine_spark(a, actor(command.target), command.verb == "divine_spark_radiant");
    }
    else if (command.verb == "preserve_life")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(false);
        --a.channel_divinity[CharacterClass::cleric];
        preserve_life(a);
    }
    else if (command.verb == "turn_undead")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(false);
        --a.channel_divinity[CharacterClass::cleric];
        turn_undead(a);
    }
    else if (command.verb == "escape")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(false);
        escape_ensnaring(a);
    }
    else if (command.verb == "create_specter")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(false);
        log(a.source.name + " uses Create Specter.",
            {"{name} uses Create Specter.", {{"name", a.source.name}}});
        raise_undead(a, actor(command.target), d.specter_key);
    }
    else if (command.verb == "flurry_of_blows" || command.verb.starts_with("flurry_"))
    {
        // Two Unarmed Strikes; the second only if the first leaves the target up.
        a.bonus = false;
        --a.surges[CharacterClass::monk];
        log(a.source.name + " uses Flurry of Blows.",
            {"{name} uses Flurry of Blows.", {{"name", a.source.name}}});
        if (command.verb == "flurry_stun")
        {
            stunning_ = a.source.id;
        }
        // Heightened Focus (Monk 10): three strikes.
        const int strikes = d.heightened_focus ? 3 : 2;
        for (int strike = 0; strike < strikes && actor(command.target).life.hp > 0; ++strike)
        {
            const bool hit =
                attack_as(a, unarmed_loadout(a), actor(command.target), AttackRange::melee);
            if (hit && actor(command.target).life.hp > 0)
            {
                open_hand(a, actor(command.target), command.verb);
            }
        }
        stunning_ = 0;
    }
    else if (command.verb.starts_with("patient_defense") ||
             command.verb.starts_with("step_of_the_wind"))
    {
        use_focus_movement(a, command.verb,
                           command.verb == "step_of_the_wind_carry" ? command.target : 0);
    }
    else if (command.verb.starts_with("create_slot_") || command.verb.starts_with("convert_slot_"))
    {
        use_font_of_magic(a, command.verb);
    }
    else if (command.verb.starts_with("metamagic_"))
    {
        ready_metamagic(a, command.verb.substr(10));
    }
    else if (command.verb == "armor_of_shadows" || command.verb == "fiendish_vigor")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        end_sanctuary(a);
        const bool was_invisible = invisible(a);
        if (command.verb == "armor_of_shadows")
        {
            detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                        detail::EffectKind::mage_armor, 0);
            log(a.source.name + " gains Mage Armor.",
                {"{name} gains {spell}.",
                 {{"name", a.source.name}, {"spell", "Mage Armor", true}}});
        }
        else
        {
            // Fiendish Vigor takes False Life's highest result: 2d4 + 4 = 12.
            TemporaryHitPoints offered{12, "spell:false_life"};
            log(a.source.name + " gains False Life.",
                {"{name} gains {spell}.",
                 {{"name", a.source.name}, {"spell", "False Life", true}}});
            if (a.life.temporary_hp.amount)
            {
                temporary_offer_ = std::move(offered);
            }
            else
            {
                detail::grant_temporary_hp(a.life, offered, TemporaryHpChoice::use_new);
            }
        }
        if (was_invisible)
        {
            end_invisibility(a.source.id);
        }
        end_hiding(a.source.id);
    }
    else if (command.verb == "flame_blade_strike")
    {
        // A Magic action: 3d6 + the spellcasting modifier Fire on a hit, 1d6
        // more for each slot level above 2.
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        end_sanctuary(a);
        const auto blade = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                        [](const auto &e)
                                        {
                                            return e.kind == detail::EffectKind::flame_blade;
                                        });
        const int count = 1 + static_cast<int>(std::max(2u, blade->slot));
        (void)attack(a, actor(command.target), AttackRange::melee,
                     SpellAttack{.dice = {count, 6, casting_modifier(def(a))},
                                 .type = detail::DamageType::fire});
    }
    else if (command.verb.starts_with("wild_shape_"))
    {
        a.bonus = false;
        --a.channel_divinity[CharacterClass::druid];
        const auto *form = detail::beast_form(std::string_view(command.verb).substr(11));
        std::erase_if(a.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::wild_shape;
                      });
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::wild_shape,
                                    static_cast<int>(form - detail::beast_forms.data()) + 1);
        const int druid_speed = def(a).speed;
        refresh_form(a);
        // A form too big for its space takes the nearest with room (FOOT-1).
        a.source.cell = nearest_open(a);
        change_speed(a, druid_speed);
        log(a.source.name + " takes the shape of a " + std::string(form->label) + ".",
            {"{name} takes the shape of a {form}.",
             {{"name", a.source.name}, {"form", std::string(form->label), true}}});
        // Temporary Hit Points equal to the Druid level.
        TemporaryHitPoints offered{def(a).class_levels[CharacterClass::druid],
                                   "feature:wild_shape"};
        if (a.life.temporary_hp.amount)
        {
            temporary_offer_ = std::move(offered);
        }
        else
        {
            detail::grant_temporary_hp(a.life, offered, TemporaryHpChoice::use_new);
        }
    }
    else if (command.verb == "leave_wild_shape")
    {
        a.bonus = false;
        end_wild_shape(a);
    }
    else if (command.verb == "heat_metal_again")
    {
        a.bonus = false;
        log(a.source.name + " heats the metal again.",
            {"{name} heats the metal again.", {{"name", a.source.name}}});
        auto &target = actor(command.target);
        const auto heated = std::find_if(target.effects.active.begin(), target.effects.active.end(),
                                         [&](const auto &e)
                                         {
                                             return e.kind == detail::EffectKind::heated &&
                                                    e.source_actor == a.source.id;
                                         });
        heat_metal(a, target, heated->slot);
    }
    else if (command.verb == "move_moonbeam")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        move_moonbeam(a, actor(command.target));
    }
    else if (command.verb == "hurl_flame")
    {
        // A Magic action, not a casting: a ranged spell attack for 1d8 Fire.
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        end_sanctuary(a);
        log(a.source.name + " hurls Produce Flame.",
            {"{name} hurls {spell}.", {{"name", a.source.name}, {"spell", "Produce Flame", true}}});
        (void)attack(a, actor(command.target), AttackRange::ranged,
                     SpellAttack{.dice = {1, 8, 0},
                                 .type = detail::DamageType::fire,
                                 .spell = "produce_flame"});
    }
    else if (command.verb.starts_with("font_of_inspiration_"))
    {
        --a.slots[static_cast<unsigned>(command.verb.back() - '0')];
        ++a.arcane[CharacterClass::bard];
        log(a.source.name + " turns a spell slot into Bardic Inspiration.",
            {"{name} turns a spell slot into Bardic Inspiration.", {{"name", a.source.name}}});
    }
    else if (command.verb == "bardic_inspiration")
    {
        a.bonus = false;
        --a.arcane[CharacterClass::bard];
        auto &ally = actor(command.target);
        detail::apply_spell_benefit(ally.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::inspired, d.bardic_die);
        log(a.source.name + " inspires " + ally.source.name + ".",
            {"{name} inspires {target}.", {{"name", a.source.name}, {"target", ally.source.name}}});
    }
    else if (command.verb == "dragon_wings" || command.verb == "dragon_wings_points")
    {
        a.bonus = false;
        if (command.verb == "dragon_wings")
        {
            detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                        detail::EffectKind::wings_used, 0);
        }
        else
        {
            a.lay_on_hands[CharacterClass::sorcerer] -= 3;
        }
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::dragon_wings, 0);
        log(a.source.name + " spreads dragon wings.",
            {"{name} spreads dragon wings.", {{"name", a.source.name}}});
    }
    else if (command.verb == "innate_sorcery" || command.verb == "innate_sorcery_points")
    {
        a.bonus = false;
        if (command.verb == "innate_sorcery")
        {
            --a.free_casts[CharacterClass::sorcerer];
        }
        else
        {
            a.lay_on_hands[CharacterClass::sorcerer] -= 2;
        }
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::innate_sorcery, 0);
        log(a.source.name + " unleashes Innate Sorcery.",
            {"{name} unleashes Innate Sorcery.", {{"name", a.source.name}}});
    }
    else if (command.verb == "martial_arts")
    {
        a.bonus = false;
        attack_as(a, unarmed_loadout(a), actor(command.target), AttackRange::melee);
    }
    else if (command.verb == "coat_weapon")
    {
        a.bonus = false;
        --*basic_poison_left(a);
        a.coated_until_round = static_cast<int>(round_) + 10;
        log(a.source.name + " coats a weapon with Basic Poison.",
            {"{name} coats a weapon with Basic Poison.", {{"name", a.source.name}}});
    }
    else if (command.verb == "ignite_flame_tongue" || command.verb == "douse_flame_tongue")
    {
        a.bonus = false;
        a.flame_lit = command.verb == "ignite_flame_tongue";
        log(a.source.name + (a.flame_lit ? "'s blade bursts into flame." : " douses the blade."),
            {a.flame_lit ? "{name}'s blade bursts into flame." : "{name} douses the blade.",
             {{"name", a.source.name}}});
    }
    else if (command.verb == "rage")
    {
        a.bonus = false;
        --a.channel_divinity[CharacterClass::barbarian];
        // A raging creature cannot keep Concentration.
        end_concentration(a);
        detail::apply_poisoned(a.effects, effect_source(a), a.source.name, rage_duration(a),
                               detail::EffectKind::raging);
        log(a.source.name + " enters a Rage.",
            {"{name} enters a Rage.", {{"name", a.source.name}}});
        // Instinctive Pounce: up to half Speed, as part of entering the Rage.
        if (const int feet = std::max(0, d.speed - detail::speed_penalty(a.effects)) / 2;
            d.instinctive_pounce && feet > 0)
        {
            a.movement += feet;
            a.pounce = feet;
            log(a.source.name + " may move " + std::to_string(feet) +
                    " feet as part of entering the Rage (Instinctive Pounce).",
                {"{name} may move {feet} feet as part of entering the Rage (Instinctive Pounce).",
                 {{"name", a.source.name}, {"feet", std::to_string(feet)}}});
        }
        // Mindless Rage: entering the Rage ends Charmed and Frightened.
        if (d.mindless_rage)
        {
            std::erase_if(a.effects.active,
                          [](const auto &e)
                          {
                              return e.kind == detail::EffectKind::charmed ||
                                     e.kind == detail::EffectKind::enthralled ||
                                     e.kind == detail::EffectKind::turn_charmed ||
                                     e.kind == detail::EffectKind::turned ||
                                     e.kind == detail::EffectKind::frightened ||
                                     e.kind == detail::EffectKind::abjured ||
                                     e.kind == detail::EffectKind::terrified ||
                                     e.kind == detail::EffectKind::entranced ||
                                     e.kind == detail::EffectKind::compelled;
                          });
        }
    }
    else if (command.verb == "intimidating_presence" ||
             command.verb == "intimidating_presence_rage")
    {
        // A Rage use restores the once-per-Long-Rest use.
        a.bonus = false;
        if (command.verb == "intimidating_presence")
        {
            --a.free_casts[CharacterClass::barbarian];
        }
        else
        {
            --a.channel_divinity[CharacterClass::barbarian];
        }
        intimidate(a);
    }
    else if (command.verb == "extend_rage")
    {
        a.bonus = false;
        extend_rage(a);
    }
    else if (command.verb == "shake_awake")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(false);
        auto &sleeper = actor(command.target);
        std::erase_if(sleeper.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::drowsy ||
                                 e.kind == detail::EffectKind::asleep ||
                                 e.kind == detail::EffectKind::knocked_out ||
                                 e.kind == detail::EffectKind::entranced;
                      });
        log(a.source.name + " shakes " + sleeper.source.name + " awake.",
            {"{name} shakes {target} awake.",
             {{"name", a.source.name}, {"target", sleeper.source.name}}});
    }
    else if (command.verb == "retreat_dash")
    {
        a.bonus = false;
        a.movement += d.speed;
        ++a.dashes;
        log(a.source.name + " dashes.", {"{name} dashes.", {{"name", a.source.name}}});
    }
    else if (command.verb == "roll_flaming_sphere")
    {
        a.bonus = false;
        auto &target = actor(command.target);
        auto &sphere = *std::find_if(zones_.begin(), zones_.end(),
                                     [&](const auto &zone)
                                     {
                                         return zone.kind == ZoneKind::flaming_sphere &&
                                                zone.caster == a.source.id;
                                     });
        sphere.cells.front() = spectral_cell(target, sphere.cells.front());
        log(a.source.name + " rolls the Flaming Sphere into " + target.source.name + ".",
            {"{name} rolls the Flaming Sphere into {target}.",
             {{"name", a.source.name}, {"target", target.source.name}}});
        // A copy: the burn can end the caster's Concentration and the sphere.
        const Zone rolled = sphere;
        sphere_burns(rolled, target);
    }
    else if (command.verb == "hurl_through_hell" || command.verb == "hurl_through_hell_slot")
    {
        // A Charisma save, or the creature is gone until the end of the
        // Warlock's next turn, Incapacitated, and takes 8d10 Psychic unless a
        // Fiend.
        a.hurl_used = true;
        if (command.verb == "hurl_through_hell")
        {
            detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                        detail::EffectKind::hurl_used, 0);
        }
        else
        {
            --a.slots[static_cast<std::size_t>(d.pact_slot_level)];
        }
        auto &target = actor(command.target);
        if (saving_throw_succeeds(target, detail::Ability::charisma, spell_dc(a, "")))
        {
            return;
        }
        if (def(target).creature_type != "fiend")
        {
            const int amount =
                resolved_damage(target, detail::DamageType::psychic, dice({8, 10, 0}));
            log(target.source.name + " takes " + std::to_string(amount) +
                    " Psychic damage from Hurl Through Hell.",
                {"{name} takes {damage} Psychic damage from Hurl Through Hell.",
                 {{"name", target.source.name}, {"damage", std::to_string(amount)}}});
            damage(target, amount, false);
        }
        if (target.life.dead || !detail::can_apply(target.effects))
        {
            return;
        }
        const auto index = index_of(a.source.id);
        const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
        detail::apply_poisoned(target.effects, effect_source(a), a.source.name,
                               next_turn_ms(a) + slot, detail::EffectKind::hurled);
        log(target.source.name + " is hurled through the Lower Planes.",
            {"{name} is hurled through the Lower Planes.", {{"name", target.source.name}}});
        end_incapacitated_effects(target);
    }
    else if (command.verb == "eldritch_smite")
    {
        // 1d8 Force and 1d8 more per slot level; a Huge or smaller creature
        // falls Prone.
        a.smite_used = true;
        const auto level = static_cast<unsigned>(d.pact_slot_level);
        --a.slots[level];
        auto &target = actor(command.target);
        const int count = (1 + static_cast<int>(level)) * (a.smite_critical ? 2 : 1);
        const int amount = resolved_damage(target, detail::DamageType::force, dice({count, 8, 0}));
        log(target.source.name + " takes " + std::to_string(amount) +
                " Force damage from Eldritch Smite.",
            {"{name} takes {damage} Force damage from Eldritch Smite.",
             {{"name", target.source.name}, {"damage", std::to_string(amount)}}});
        damage(target, amount, false);
        if (!target.life.dead && def(target).size <= 4 && !target.effects.prone)
        {
            target.effects.prone = true;
            log(target.source.name + " is knocked Prone.",
                {"{name} is knocked Prone.", {{"name", target.source.name}}});
        }
    }
    else if (command.verb.starts_with("lifedrinker_"))
    {
        // 1d6 more of the chosen type, and a Hit Point Die spent if chosen.
        a.drink_used = true;
        auto rest = std::string_view(command.verb).substr(std::string_view("lifedrinker_").size());
        const bool heals = rest.ends_with("_heal");
        if (heals)
        {
            rest.remove_suffix(std::string_view("_heal").size());
        }
        const auto type = detail::damage_type(rest);
        const auto name = std::string(detail::damage_name(type));
        auto &target = actor(command.target);
        const int amount = resolved_damage(target, type, dice({a.smite_critical ? 2 : 1, 6, 0}));
        log(target.source.name + " takes " + std::to_string(amount) + " " + name +
                " damage from Lifedrinker.",
            {"{name} takes {damage} {type} damage from Lifedrinker.",
             {{"name", target.source.name},
              {"damage", std::to_string(amount)},
              {"type", name, true}}});
        damage(target, amount, false);
        if (heals)
        {
            const int die = next_hit_die(d, a.hit_dice);
            --a.hit_dice;
            heal(a, std::max(1, roll(die) + d.constitution));
        }
    }
    else if (command.verb == "arcane_sword_strike")
    {
        a.bonus = false;
        end_sanctuary(a);
        auto &target = actor(command.target);
        auto &sword =
            *std::find_if(zones_.begin(), zones_.end(),
                          [&](const auto &zone)
                          {
                              return zone.kind == ZoneKind::sword && zone.caster == a.source.id;
                          });
        sword.cells.front() = spectral_cell(target, sword.cells.front());
        log(a.source.name + "'s Arcane Sword strikes.",
            {"{name}'s Arcane Sword strikes.", {{"name", a.source.name}}});
        attack(
            a, target, AttackRange::melee,
            SpellAttack{.dice = {4, 12, casting_modifier(d)}, .type = detail::DamageType::force});
    }
    else if (command.verb == "conjure_fey_strike")
    {
        a.bonus = false;
        end_sanctuary(a);
        auto &target = actor(command.target);
        auto &spirit =
            *std::find_if(zones_.begin(), zones_.end(),
                          [&](const auto &zone)
                          {
                              return zone.kind == ZoneKind::fey && zone.caster == a.source.id;
                          });
        spirit.cells.front() = spectral_cell(target, spirit.cells.front());
        const int count = 3 + static_cast<int>(spirit.slot) - 6;
        log(a.source.name + "'s fey spirit strikes.",
            {"{name}'s fey spirit strikes.", {{"name", a.source.name}}});
        if (attack(a, target, AttackRange::melee,
                   SpellAttack{.dice = {count, 12, casting_modifier(d)},
                               .type = detail::DamageType::psychic}))
        {
            fey_frighten(a, target);
        }
    }
    else if (command.verb == "leave_animal_shape")
    {
        a.bonus = false;
        end_animal_shape(a);
    }
    else if (command.verb == "detonate")
    {
        end_concentration(a);
    }
    else if (command.verb == "overchannel")
    {
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::overchannel, 0);
        log(a.source.name + " readies Overchannel.",
            {"{name} readies {feature}.",
             {{"name", a.source.name}, {"feature", "Overchannel", true}}});
    }
    else if (command.verb == "spiritual_weapon_strike")
    {
        a.bonus = false;
        end_sanctuary(a);
        auto &target = actor(command.target);
        auto &force = *std::find_if(zones_.begin(), zones_.end(),
                                    [&](const auto &zone)
                                    {
                                        return zone.kind == ZoneKind::spiritual_weapon &&
                                               zone.caster == a.source.id;
                                    });
        force.cells.front() = spectral_cell(target, force.cells.front());
        log(a.source.name + "'s Spiritual Weapon strikes.",
            {"{name}'s Spiritual Weapon strikes.", {{"name", a.source.name}}});
        // 1d8 more for each slot level above 2.
        attack(a, target, AttackRange::melee,
               SpellAttack{.dice = {static_cast<int>(force.slot) - 1, 8, casting_modifier(d)},
                           .type = detail::DamageType::force});
    }
    else if (command.verb == "dispel_break" || command.verb == "dispel_dismissal")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        auto &target = actor(command.target);
        if (command.verb == "dispel_break")
        {
            std::erase_if(target.effects.active,
                          [&](const auto &e)
                          {
                              return enchants(e) && otherworldly_source(e);
                          });
            release(target);
            log(a.source.name + " breaks the enchantment on " + target.source.name + ".",
                {"{name} breaks the enchantment on {target}.",
                 {{"name", a.source.name}, {"target", target.source.name}}});
        }
        else if (!saving_throw_succeeds(target, detail::Ability::charisma,
                                        spell_dc(a, "dispel_evil_and_good")))
        {
            // Sent home, it counts as defeated, as a banished one does (LEVELS-1).
            target.life.hp = 0;
            target.life.dead = true;
            target.life.recovery = {};
            log(target.source.name + " is sent home.",
                {"{name} is sent home.", {{"name", target.source.name}}});
        }
        end_concentration(a);
    }
    else if (command.verb == "woodland_disengage")
    {
        a.bonus = false;
        a.disengaged = true;
        log(a.source.name + " disengages.", {"{name} disengages.", {{"name", a.source.name}}});
    }
    else if (command.verb == "natures_veil")
    {
        a.bonus = false;
        --a.lay_on_hands[CharacterClass::ranger];
        const auto index = index_of(a.source.id);
        const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
        detail::apply_poisoned(a.effects, effect_source(a), a.source.name, next_turn_ms(a) + slot,
                               detail::EffectKind::veiled);
        log(a.source.name + " uses Nature's Veil and is Invisible until the end of its next turn.",
            {"{name} uses Nature's Veil and is Invisible until the end of its next turn.",
             {{"name", a.source.name}}});
    }
    else if (command.verb == "move_spirit_pack")
    {
        move_spirit_pack(a, actor(command.target));
    }
    else if (command.verb == "move_pillar")
    {
        move_pillar(a, actor(command.target));
    }
    else if (command.verb == "hand_fist" || command.verb == "hand_push")
    {
        if (!std::erase_if(a.effects.active,
                           [](const auto &e)
                           {
                               return e.kind == detail::EffectKind::hand_ready;
                           }))
        {
            a.bonus = false;
        }
        auto &hand =
            *std::find_if(zones_.begin(), zones_.end(),
                          [&](const auto &zone)
                          {
                              return zone.kind == ZoneKind::hand && zone.caster == a.source.id;
                          });
        auto &target = actor(command.target);
        hand.cells = {spectral_cell(target, hand.cells.front())};
        const auto &spell = *detail::find_spell("arcane_hand");
        if (command.verb == "hand_fist")
        {
            log("The hand strikes " + target.source.name + ".",
                {"The hand strikes {name}.", {{"name", target.source.name}}});
            (void)attack(a, target, AttackRange::melee,
                         SpellAttack{.dice = {spell.dice.count +
                                                  2 * static_cast<int>(hand.slot - spell.level),
                                              spell.dice.sides, 0},
                                     .type = spell.damage,
                                     .spell = spell.id});
        }
        else
        {
            log("The hand shoves " + target.source.name + ".",
                {"The hand shoves {name}.", {{"name", target.source.name}}});
            if (!saving_throw_succeeds(target, detail::Ability::strength, hand.save_dc, false,
                                       heightened_against(target, a, spell.id)))
            {
                // Away from the hand: 5 feet plus five times the modifier.
                Actor origin = a;
                origin.source.cell = hand.cells.front();
                push_away(origin, target, 1 + std::max(0, casting_modifier(def(a))));
                hand.cells = {spectral_cell(target, hand.cells.front())};
            }
        }
    }
    else if (command.verb.starts_with("evil_eye_"))
    {
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        auto &target = actor(command.target);
        const auto &spell = *detail::find_spell("eyebite");
        const auto effect = "eyebite_" + command.verb.substr(std::string_view("evil_eye_").size());
        log(a.source.name + " turns its eyes on " + target.source.name + ".",
            {"{name} turns its eyes on {target}.",
             {{"name", a.source.name}, {"target", target.source.name}}});
        // Dwarven Resilience helps against the sickness, which Poisons.
        if (!saving_throw_succeeds(target, spell.save, spell_dc(a, spell.id),
                                   effect == "eyebite_sickened" && def(target).dwarf))
        {
            apply_rider(spell, effect, a, target, spell_dc(a, spell.id));
        }
        else if (detail::can_apply(target.effects))
        {
            detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                        detail::EffectKind::eye_resisted, 0);
        }
    }
    else if (command.verb == "stop_dancing")
    {
        a.nick_origin = 0;
        (void)a.actions.spend();
        const auto dance = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                        [](const auto &e)
                                        {
                                            return e.kind == detail::EffectKind::dancing;
                                        });
        if (saving_throw_succeeds(a, detail::Ability::wisdom,
                                  spell_dc(actor(dance->source_actor), "irresistible_dance"),
                                  def(a).fey_ancestry))
        {
            std::erase_if(a.effects.active,
                          [](const auto &e)
                          {
                              return e.kind == detail::EffectKind::dancing;
                          });
            log(a.source.name + " stops dancing.",
                {"{name} stops dancing.", {{"name", a.source.name}}});
        }
    }
    else if (command.verb == "leave_host")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        auto &caster = actor(controller(a)->source.id);
        std::erase_if(a.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::dominated;
                      });
        std::erase_if(caster.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::jarred;
                      });
        log(caster.source.name + "'s soul returns to its body.",
            {"{name}'s soul returns to its body.", {{"name", caster.source.name}}});
    }
    else if (command.verb == "move_double")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        for (auto &twin : actors_)
        {
            if (std::any_of(twin.effects.active.begin(), twin.effects.active.end(),
                            [&](const auto &e)
                            {
                                return e.kind == detail::EffectKind::illusory &&
                                       e.source_actor == a.source.id;
                            }))
            {
                twin.source.cell = spectral_cell(actor(command.target), twin.source.cell);
                log(a.source.name + " moves the illusory double.",
                    {"{name} moves the illusory double.", {{"name", a.source.name}}});
            }
        }
    }
    else if (command.verb == "move_hound")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        auto &hound =
            *std::find_if(zones_.begin(), zones_.end(),
                          [&](const auto &zone)
                          {
                              return zone.kind == ZoneKind::hound && zone.caster == a.source.id;
                          });
        hound.cells = {spectral_cell(actor(command.target), hound.cells.front())};
        log(a.source.name + " sends the hound after " + actor(command.target).source.name + ".",
            {"{name} sends the hound after {target}.",
             {{"name", a.source.name}, {"target", actor(command.target).source.name}}});
    }
    else if (command.verb == "vampiric_touch_again")
    {
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        const auto touch = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                        [](const auto &e)
                                        {
                                            return e.kind == detail::EffectKind::vampiric;
                                        });
        vampiric_touch(a, actor(command.target), static_cast<unsigned>(touch->dc));
    }

    else if (command.verb == "potent_vitality")
    {
        grant_vitality(a, actor(command.target));
    }
    else if (command.verb == "superior_hunters_prey")
    {
        a.prey_used = true;
        auto &target = actor(command.target);
        const int extra = resolved_damage(target, detail::DamageType::force, dice({1, 6, 0}));
        log(target.source.name + " takes " + std::to_string(extra) +
                " Force damage from Superior Hunter's Prey.",
            {"{name} takes {damage} Force damage from Superior Hunter's Prey.",
             {{"name", target.source.name}, {"damage", std::to_string(extra)}}});
        damage(target, extra, false);
    }
    else if (command.verb == "tireless")
    {
        a.actions.spend(true);
        --a.arcane[CharacterClass::ranger];
        TemporaryHitPoints offered{std::max(1, roll(8) + d.wisdom), "feature:tireless"};
        log(a.source.name + " uses Tireless.",
            {"{name} uses Tireless.", {{"name", a.source.name}}});
        if (a.life.temporary_hp.amount)
        {
            temporary_offer_ = std::move(offered);
        }
        else
        {
            detail::grant_temporary_hp(a.life, offered, TemporaryHpChoice::use_new);
        }
    }
    else if (command.verb == "horde_breaker")
    {
        a.horde_used = true;
        auto &target = actor(command.target);
        log(a.source.name + " uses Horde Breaker.",
            {"{name} uses Horde Breaker.", {{"name", a.source.name}}});
        const bool beyond_reach = distance(a, target) > d.reach;
        attack(a, target, beyond_reach ? AttackRange::ranged : AttackRange::melee);
    }
    else if (command.verb == "hunters_mark_free")
    {
        const auto &spell = *detail::find_spell("hunters_mark");
        a.bonus = false;
        --a.free_casts[CharacterClass::ranger];
        end_hiding(a.source.id);
        log(a.source.name + " casts Hunter's Mark (Favored Enemy).",
            {"{name} casts Hunter's Mark (Favored Enemy).", {{"name", a.source.name}}});
        begin_concentration(a, spell);
        apply_rider(spell, command.verb, a, actor(command.target), spell_dc(a));
    }
    else if (command.verb == "hex_move")
    {
        a.bonus = false;
        for (auto &other : actors_)
        {
            std::erase_if(other.effects.active,
                          [&](const auto &e)
                          {
                              return e.kind == detail::EffectKind::hex &&
                                     e.source_scope == scope_ && e.source_actor == a.source.id;
                          });
        }
        auto &cursed = actor(command.target);
        detail::apply_spell_benefit(cursed.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::hex, 0);
        log(a.source.name + " moves Hex to " + cursed.source.name + ".",
            {"{name} moves Hex to {target}.",
             {{"name", a.source.name}, {"target", cursed.source.name}}});
    }
    else if (command.verb == "hunters_mark_move")
    {
        a.bonus = false;
        for (auto &other : actors_)
        {
            std::erase_if(other.effects.active,
                          [&](const auto &e)
                          {
                              return e.kind == detail::EffectKind::hunters_mark &&
                                     e.source_scope == scope_ && e.source_actor == a.source.id;
                          });
        }
        auto &quarry = actor(command.target);
        detail::apply_spell_benefit(quarry.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::hunters_mark, 0);
        log(a.source.name + " moves Hunter's Mark to " + quarry.source.name + ".",
            {"{name} moves Hunter's Mark to {target}.",
             {{"name", a.source.name}, {"target", quarry.source.name}}});
        reveal_lore(a, quarry);
    }
    else if (command.verb == "sacred_weapon" || command.verb == "sacred_weapon_radiant")
    {
        // The effect records the chosen damage: 1 for Radiant, 0 for the weapon's own.
        const bool radiant = command.verb == "sacred_weapon_radiant";
        --a.channel_divinity[CharacterClass::cleric];
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::sacred_weapon, radiant ? 1 : 0);
        if (radiant)
        {
            log(a.source.name + " uses Sacred Weapon, dealing Radiant damage.",
                {"{name} uses Sacred Weapon, dealing Radiant damage.", {{"name", a.source.name}}});
        }
        else
        {
            log(a.source.name + " uses Sacred Weapon.",
                {"{name} uses Sacred Weapon.", {{"name", a.source.name}}});
        }
    }
    else if (command.verb == "adrenaline_rush")
    {
        a.bonus = false;
        --a.rushes;
        a.rush_used = true;
        a.movement += d.speed;
        ++a.dashes;
        TemporaryHitPoints offered{d.rushes, std::string(rush_source)};
        if (a.life.temporary_hp.amount)
        {
            temporary_offer_ = std::move(offered);
        }
        else
        {
            detail::grant_temporary_hp(a.life, offered, TemporaryHpChoice::use_new);
        }
        log(a.source.name + " uses Adrenaline Rush.",
            {"{name} uses Adrenaline Rush.", {{"name", a.source.name}}});
    }
    else if (command.verb == "opportunity" || command.verb == "decline")
    {
        if (command.verb == "opportunity")
        {
            a.reaction = false;
            if (const auto *boss = controller(a))
            {
                actor(boss->source.id).reaction = false;
            }
            opportunity_ = true;
            attack(a, actor(command.target), AttackRange::melee);
            opportunity_ = false;
        }
        if (!champion_move_ && !graze_ && !effect_waiting())
        {
            finish_reaction();
        }
    }
    else if (command.verb == "move")
    {
        if (held_by_wave(a))
        {
            return;
        }
        // Resilient Sphere: rolling the sphere takes the creature's action.
        if (detail::has_effect(a.effects, detail::EffectKind::sphered) && !a.moved)
        {
            (void)a.actions.spend();
        }
        path_ = path_to(a, command.destination);
        path_index_ = 0;
        const auto mover = a.source.id;
        progress_movement();
        // Compulsion's save follows the named move, once it is all walked.
        if (path_index_ >= path_.size())
        {
            finish_compulsion(actor(mover));
        }
    }
    else if (command.verb == "flee")
    {
        if (held_by_wave(a))
        {
            return;
        }
        // One step off the field; leaving an enemy's reach provokes as any move.
        path_ = {command.destination};
        path_index_ = 0;
        progress_movement();
    }
    else if (command.verb == "end")
    {
        end_turn();
    }
    else if (command.verb == "wholeness_of_body")
    {
        a.bonus = false;
        --a.free_casts[CharacterClass::monk];
        log(a.source.name + " uses Wholeness of Body.",
            {"{name} uses Wholeness of Body.", {{"name", a.source.name}}});
        heal(a, healing_roll(a, {1, d.martial_die, std::max(1, d.wisdom)}));
    }
    else if (command.verb == "second_wind")
    {
        a.bonus = false;
        --a.winds;
        heal(a, healing_roll(a, {1, 10, d.class_levels[CharacterClass::fighter]}));
        // Tactical Shift: up to half Speed without provoking Opportunity Attacks.
        if (const int feet = std::max(0, d.speed - detail::speed_penalty(a.effects)) / 2;
            d.tactical_shift && feet > 0 && conscious(a))
        {
            champion_move_ = ChampionMove{.actor = a.source.id,
                                          .target = a.source.id,
                                          .remaining = feet,
                                          .origin = a.source.cell,
                                          .shift = true};
            log("Tactical Shift: " + a.source.name + " may move " + std::to_string(feet) +
                    " feet without provoking Opportunity Attacks.",
                {"Tactical Shift: {name} may move {feet} feet without provoking Opportunity Attacks.",
                 {{"name", a.source.name}, {"feet", std::to_string(feet)}}});
        }
    }
    else if (command.verb == "lay_on_hands_restore")
    {
        restore_touch(a, actor(command.target));
    }
    else if (command.verb == "lay_on_hands_cure")
    {
        auto &target = actor(command.target);
        a.bonus = false;
        a.lay_on_hands[CharacterClass::paladin] -= 5;
        std::erase_if(target.effects.active,
                      [](const auto &e)
                      {
                          return gives_condition(e.kind, "poisoned");
                      });
        log(a.source.name + " uses Lay On Hands on " + target.source.name + ".",
            {"{name} uses Lay On Hands on {target}.",
             {{"name", a.source.name}, {"target", target.source.name}}});
        log(target.source.name + " is no longer Poisoned.",
            {"{name} is no longer Poisoned.", {{"name", target.source.name}}});
    }
    else if (command.verb == "lay_on_hands")
    {
        // Restores what the target is missing, up to the pool; only the
        // Hit Points actually restored are spent.
        auto &target = actor(command.target);
        a.bonus = false;
        log(a.source.name + " uses Lay On Hands on " + target.source.name + ".",
            {"{name} uses Lay On Hands on {target}.",
             {{"name", a.source.name}, {"target", target.source.name}}});
        a.lay_on_hands[CharacterClass::paladin] -=
            heal(target, std::min(a.lay_on_hands[CharacterClass::paladin],
                                  max_hp(target) - target.life.hp));
    }
    else if (const auto *aimed = detail::find_spell(command.verb);
             aimed && (aimed->target == detail::SpellTarget::area ||
                       aimed->rider == detail::Rider::telekinesis))
    {
        area_ = PendingArea{a.source.id, command.verb, default_area_center(a, *aimed)};
        area_->companion = command.target;
        // Telekinesis aims where its creature goes, from where it stands.
        if (aimed->rider == detail::Rider::telekinesis)
        {
            area_->center = actor(command.target).source.cell;
        }
        // Fire Storm's first cube holds the preview's square.
        if (aimed->storm)
        {
            area_->center = {area_->center.x - area_->center.x % 2,
                             area_->center.y - area_->center.y % 2};
            area_->cubes.push_back(area_->center);
        }
    }
    else if (detail::without_cast_suffix(command.verb) == "revivify")
    {
        revivify(a, command);
    }
    else if (command.verb == "abjure_foes" &&
             selection_maximum({a.source.id, command.verb, {}}) == 1)
    {
        abjure(a, std::array{command.target});
    }
    else if (const auto *multi = detail::find_spell(command.verb);
             multi && selects_creatures(*multi) &&
             selection_maximum({a.source.id, command.verb, {}}) > 1)
    {
        // The first creature starts the choice; nothing is spent until the cast.
        selection_ = PendingSelection{a.source.id, command.verb, {command.target}};
        if (selection_->chosen.size() == selection_maximum(*selection_))
        {
            cast_on_selection();
        }
    }
    else if (command.verb.starts_with("primal_strike_"))
    {
        // Part of the hit's damage, so a Critical Hit doubles its dice.
        a.strike_used = true;
        auto &target = actor(command.target);
        const auto type = detail::damage_type(std::string_view(command.verb).substr(14));
        const auto name = std::string(detail::damage_name(type));
        const int amount =
            resolved_damage(target, type, dice({d.primal_strike, 8, 0}, a.smite_critical));
        log(target.source.name + " takes " + std::to_string(amount) + " " + name +
                " damage from Primal Strike.",
            {"{name} takes {damage} {type} damage from Primal Strike.",
             {{"name", target.source.name},
              {"damage", std::to_string(amount)},
              {"type", name, true}}});
        damage(target, amount, false);
    }
    else if (command.verb == "wild_resurgence_slot")
    {
        --a.channel_divinity[CharacterClass::druid];
        ++a.slots[1];
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::resurged, 1);
        log(a.source.name + " turns Wild Shape into a spell slot.",
            {"{name} turns Wild Shape into a spell slot.", {{"name", a.source.name}}});
    }
    else if (command.verb.starts_with("wild_resurgence_"))
    {
        --a.slots[static_cast<unsigned>(command.verb.back() - '0')];
        ++a.channel_divinity[CharacterClass::druid];
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::resurged, 0);
        log(a.source.name + " turns a spell slot into Wild Shape.",
            {"{name} turns a spell slot into Wild Shape.", {{"name", a.source.name}}});
    }
    else if (command.verb == "divine_strike_radiant" || command.verb == "divine_strike_necrotic")
    {
        // Part of the hit's damage, so a Critical Hit doubles its dice.
        a.strike_used = true;
        auto &target = actor(command.target);
        const auto type = command.verb == "divine_strike_radiant" ? detail::DamageType::radiant
                                                                  : detail::DamageType::necrotic;
        const auto name = std::string(detail::damage_name(type));
        const int amount =
            resolved_damage(target, type, dice({d.divine_strike, 8, 0}, a.smite_critical));
        log(target.source.name + " takes " + std::to_string(amount) + " " + name +
                " damage from Divine Strike.",
            {"{name} takes {damage} {type} damage from Divine Strike.",
             {{"name", target.source.name},
              {"damage", std::to_string(amount)},
              {"type", name, true}}});
        damage(target, amount, false);
    }
    else if (is_smite(command.verb))
    {
        resolve_smite(a, command.verb);
    }
    else if (const auto *bonus_spell = detail::find_spell(command.verb);
             bonus_spell && !command.verb.ends_with(detail::intervention_suffix) &&
             (bonus_spell->bonus_action || readies(a, Metamagic::quickened, *bonus_spell) ||
              bonus_action_verb(command.verb)))
    {
        // A Bonus Action spell spends no Action, so it resolves outside the
        // Action block below.
        a.bonus = false;
        resolve_spell(*bonus_spell, command.verb, a, command.target);
    }
    else
    {
        // Extra Attack: a later attack of the same Attack action spends nothing.
        // A Loading weapon already fired in it shoots again with a new action.
        const bool attacking = attack_action(command.verb);
        const bool loading_shot =
            (command.verb == "ranged" && d.ranged_loading) ||
            (command.verb == "shoot" && detail::weapon(carried_ranged_weapon(a))->loading);
        const bool later_attack =
            attacking && !(loading_shot && a.actions.loading_fired) && a.actions.attack_again();
        // Haste's action takes a Dash, Disengage, Hide or Utilize first, and
        // a single attack once the Action is spent.
        const bool hasty =
            !later_attack && a.actions.haste &&
            (attacking ? !a.actions.available()
                       : command.verb == "dash" || command.verb == "disengage" ||
                             command.verb == "hide" || command.verb == "doff_shield");
        if (hasty)
        {
            a.nick_origin = 0;
            a.actions.haste = false;
            a.actions.loading_fired = false;
        }
        else if (!later_attack)
        {
            a.nick_origin = 0;
            (void)a.actions.spend(detail::find_spell(command.verb) != nullptr);
            // Thirsting Blade's Extra Attack follows a pact weapon's attack.
            if (attacking)
            {
                a.actions.extra_attacks = d.thirsting_blade && command.verb != "melee"
                                              ? 0u
                                              : static_cast<unsigned>(d.extra_attacks);
            }
        }
        if (loading_shot)
        {
            a.actions.loading_fired = true;
        }
        if (command.verb == "dash")
        {
            a.movement += d.speed;
            ++a.dashes;
            log(a.source.name + " dashes.", {"{name} dashes.", {{"name", a.source.name}}});
        }
        else if (command.verb == "doff_shield")
        {
            // Taking off a shield takes the Utilize action (SRD 5.2.1); it stays
            // off for the rest of the fight.
            a.shield_off = true;
            a.definition = a.source.character_profile.empty()
                               ? monster_without_shield(a.definition)
                               : without_shield(a.source.character_profile, a.definition);
            a.drawn_weapon = carried_ranged_weapon(a);
            a.weapon_set_shown = rules::WeaponSetShown::ranged;
            log(a.source.name + " takes off a shield to use a bow.",
                {"{name} takes off a shield to use a bow.", {{"name", a.source.name}}});
        }
        else if (command.verb == "extinguish")
        {
            // Rolling on the ground puts out the fire and leaves the creature Prone.
            a.burning = false;
            a.effects.prone = true;
            log(a.source.name + " rolls on the ground and puts out the fire.",
                {"{name} rolls on the ground and puts out the fire.", {{"name", a.source.name}}});
        }
        else if (command.verb == "torch")
        {
            torch_attack(a, actor(command.target));
        }
        else if (command.verb == "shoot")
        {
            attack_as(a, thrown_loadout(a, carried_ranged_weapon(a)), actor(command.target),
                      AttackRange::ranged);
        }
        else if (const auto *gear =
                     command.verb.starts_with("throw_")
                         ? detail::thrown_gear(std::string_view(command.verb).substr(6))
                         : nullptr)
        {
            throw_gear(a, actor(command.target), *gear);
        }
        else if (command.verb == "detach")
        {
            pull_off(a, actor(command.target));
        }
        else if (command.verb == "dodge")
        {
            a.dodge = true;
            log(a.source.name + " dodges.", {"{name} dodges.", {{"name", a.source.name}}});
        }
        else if (command.verb == "hide")
        {
            hide(a);
        }
        else if (command.verb == "search")
        {
            search(a);
        }
        else if (command.verb == "disengage")
        {
            a.disengaged = true;
            log(a.source.name + " disengages.", {"{name} disengages.", {{"name", a.source.name}}});
        }
        else if (command.verb == "true_strike" || command.verb == "true_strike_radiant")
        {
            strike_true(a, actor(command.target),
                        command.verb == "true_strike_radiant" ? TrueStrikeDamage::radiant
                                                              : TrueStrikeDamage::weapon);
        }
        else if (const auto *spell = detail::find_spell(command.verb))
        {
            resolve_spell(*spell, command.verb, a, command.target);
        }
        else if (command.verb == "javelin_lightning")
        {
            lightning_javelin(a, actor(command.target), command.item);
        }
        else if (command.verb == "throw")
        {
            const auto *w = detail::weapon(items_.at(command.item - 1).definition);
            if (w && w->light)
            {
                activate_light();
            }
            throw_weapon(a, actor(command.target), command.item);
            if (light_active_)
            {
                qualify_light(a, command.item);
            }
        }
        else
        {
            // Only a weapon Attack action earns Light; spells and unarmed fallback do not.
            const auto selected_key = std::find_if(d.equipment_keys.begin(), d.equipment_keys.end(),
                                                   [](const auto &key)
                                                   {
                                                       return detail::weapon(key) != nullptr;
                                                   });
            const auto *weapon =
                selected_key == d.equipment_keys.end() ? nullptr : detail::weapon(*selected_key);
            const bool qualifies =
                weapon && weapon->light &&
                (command.verb == "ranged" || (command.verb == "melee" && !weapon->ranged));
            if (qualifies)
            {
                activate_light();
            }
            const auto token = held_weapon(a);
            if (physical_inventory_ && command.verb == "ranged" && weapon && weapon->thrown &&
                token)
            {
                throw_weapon(a, actor(command.target), token);
                if (qualifies)
                {
                    qualify_light(a, token);
                }
            }
            else
            {
                // Fire Bolt used to reach this fallthrough and borrow attack()'s
                // default 1d10 fire arguments; it is now an explicit table row.
                if (d.horde_breaker && !a.horde_origin)
                {
                    a.horde_origin = command.target;
                }
                // Multiattack: a monster's attacks follow on the same target
                // while it stands, ranged ones only for a stat block whose
                // Multiattack allows them. A Pirate first trades one for
                // Enthralling Panache on a target it sees and has not charmed,
                // and a Wight one melee attack for Life Drain.
                const bool melee = command.verb == "melee";
                const int attacks = melee || d.multiattack_ranged ? d.multiattack : 1;
                auto &target = actor(command.target);
                // The attacks follow on while the target stands or, aimed at
                // one already down (CLOSE-1), while it lives.
                const bool downed = target.life.hp == 0;
                const auto goes_on = [&]
                {
                    const auto &aimed = actor(command.target);
                    return aimed.life.hp > 0 || (downed && !aimed.life.dead);
                };
                int n = 0;
                if (attacks > 1 && d.panache_dc && !has_condition(a, "blinded") &&
                    !invisible(target) && !charmed_by(target, a.source.id))
                {
                    enthrall(a, target);
                    ++n;
                }
                else if (attacks > 1 && melee && d.life_drain_dc)
                {
                    drain_life(a, target);
                    ++n;
                }
                // A Mummy glares besides its two Rotting Fists. A Vampire bites
                // first, before its Grave Strikes' damage ends a Charm.
                if (attacks > 1 && melee && d.glare_dc)
                {
                    dreadful_glare(a, target);
                }
                if (attacks > 1 && melee && d.bite_dc)
                {
                    bite(a, target);
                }
                // A Pirate Captain charms the creature it attacks with its
                // Bonus Action.
                if (d.charm_dc && a.bonus)
                {
                    captains_charm(a, target);
                }
                // A Ghoul claws a creature it can paralyze instead of biting
                // it twice; a Minotaur gores the creature it has charged.
                if (melee && d.claw && claw_paralyzes(target))
                {
                    attack_as(a, claw_loadout(a), target, AttackRange::melee);
                    n = attacks;
                }
                else if (melee && d.gore && charge_run(a, target, d.gore_feet, 3))
                {
                    attack_as(a, gore_loadout(a), target, AttackRange::melee);
                    n = attacks;
                }
                for (; n < attacks && goes_on(); ++n)
                {
                    // An Adult Bronze Dragon breathes Repulsion Breath for
                    // its last attack when it catches two enemies or more.
                    if (melee && d.repulsion_dc && n == attacks - 1 &&
                        repulsion_catches(a, actor(command.target).source.cell) >= 2)
                    {
                        repulsion_breath(a, actor(command.target).source.cell);
                    }
                    else if (melee && d.last_attack && n == attacks - 1)
                    {
                        attack_as(a, attack_loadout(a, *d.last_attack), actor(command.target),
                                  AttackRange::melee);
                    }
                    else
                    {
                        attack(a, actor(command.target),
                               melee ? AttackRange::melee : AttackRange::ranged);
                    }
                }
                // A Centaur tramples with its Bonus Action after its attacks
                // when a straight run tramples two enemies or more.
                if (d.trample_dc && a.bonus && trait_uses_left(a) > 0 && conscious(a))
                {
                    if (const auto run = trampling_run(a); run && run->enemies >= 2)
                    {
                        trample(a, *run);
                    }
                }
                // A Phase Spider slips away with its Bonus Action after its
                // attacks; it comes back as its next turn starts.
                if (d.ethereal_jaunt && a.bonus && conscious(a) && detail::can_apply(a.effects))
                {
                    jaunt_away(a);
                }
                if (qualifies)
                {
                    qualify_light(a, token);
                }
            }
        }
    }
}

void Session::finish_check(const PendingCheck &check, int boost)
{
    auto &a = actor(check.actor);
    auto &target = actor(check.target);
    const int total = check.natural + def(a).medicine + check.guidance + boost;
    bool success = total >= 10;
    Message message{
        "{actor} stabilizes {target}: d20 {roll} + {modifier} + {boost} = {total} vs DC {dc}: {result}.",
        {{"actor", a.source.name},
         {"target", target.source.name},
         {"roll", std::to_string(check.natural)},
         {"modifier", std::to_string(def(a).medicine + check.guidance)},
         {"boost", std::to_string(boost)},
         {"total", std::to_string(total)},
         {"dc", "10"},
         {"result", success ? "Success" : "Failure", true}}};
    log(a.source.name + " Medicine: " + std::to_string(check.natural) + " + " +
            std::to_string(def(a).medicine + check.guidance) + " + " + std::to_string(boost) +
            " = " + std::to_string(total) + " vs DC 10: " + (success ? "success" : "failure"),
        message);
    // Bardic Inspiration follows the failed check in the log.
    if (!success)
    {
        success = inspiration_roll(a, 10 - total) >= 10 - total;
    }
    if (success)
    {
        detail::stabilize(target.life, rng_);
    }
}

void Session::finish_champion_move()
{
    champion_move_.reset();
    if (mastery_ && !mastery_available(*mastery_) && champion_offers_.empty())
    {
        mastery_.reset();
    }
    if (effect_waiting())
    {
        return;
    }
    finish_effects();
}

void Session::finish_effects()
{
    effect_reaction_origin_.reset();
    if (!pending())
    {
        return;
    }
    // The critical mover can now occupy the interrupted route; never walk
    // through the newly occupied cell.
    if (stop_illegal_route())
    {
        return;
    }
    finish_reaction();
}

bool Session::stop_illegal_route()
{
    if (path_index_ >= path_.size())
    {
        return false;
    }
    const auto &moving = mover();
    const auto grid = movement_grid(moving);
    auto cell = moving.source.cell;
    int budget = mover_budget(moving);
    bool valid = true, leaves_field = false;
    for (auto i = path_index_; i < path_.size(); ++i)
    {
        // A fleeing route ends with a 5-foot step off the field.
        if (off_field(moving, path_[i]))
        {
            valid = budget >= 5;
            leaves_field = true;
            break;
        }
        const auto cost = grid.step_cost(cell, path_[i]);
        if (!cost || *cost > budget)
        {
            valid = false;
            break;
        }
        budget -= *cost;
        cell = path_[i];
    }
    if (valid && (leaves_field || grid.can_stop_at(cell)))
    {
        return false;
    }
    path_.clear();
    path_index_ = 0;
    reactors_.clear();
    reactor_index_ = 0;
    flight_.reset();
    return true;
}
} // namespace opengold::srd5::detail
