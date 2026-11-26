#include "combat_session_impl.h"

// A fight under the original rules, as Pool of Radiance's combat units play it
// (docs/ORIGINAL-RULES.md, "Combat"): melee and missiles.
namespace opengold::original::detail
{

// Whether two creatures stand side by side, any square of one next to any of the other's.
bool adjacent(const Fighter &a, const Fighter &b)
{
    const auto gap = [](int first, int size, int other_first, int other_size)
    {
        return std::max(
            {0, other_first - (first + size - 1), first - (other_first + other_size - 1)});
    };
    const int dx = gap(a.cell.x, a.footprint.width, b.cell.x, b.footprint.width);
    const int dy = gap(a.cell.y, a.footprint.height, b.cell.y, b.footprint.height);
    return std::max(dx, dy) == 1;
}

// The squares a creature covers.
std::vector<Cell> squares_of(const Fighter &f)
{
    std::vector<Cell> squares;
    for (int dy = 0; dy < f.footprint.height; ++dy)
    {
        for (int dx = 0; dx < f.footprint.width; ++dx)
        {
            squares.push_back({f.cell.x + dx, f.cell.y + dy});
        }
    }
    return squares;
}

// The original's distance between two creatures: a straight line's steps
// counted 2 straight and 3 diagonal, halved, between their nearest squares
// (charhlp.pas, CalcDistance; pathing.pas, NextPathStep).
int distance(const Fighter &a, const Fighter &b)
{
    int nearest = 1000;
    for (const auto from : squares_of(a))
    {
        for (const auto to : squares_of(b))
        {
            const int dx = std::abs(to.x - from.x), dy = std::abs(to.y - from.y);
            nearest = std::min(nearest, (2 * std::max(dx, dy) + std::min(dx, dy)) / 2);
        }
    }
    return nearest;
}

// Whether a line runs from a square of one creature to a square of the other
// that the ground does not block; creatures do not (pathing.pas, BuildTargetList).
bool sees(const rules::Battlefield &board, const Fighter &a, const Fighter &b)
{
    for (const auto from : squares_of(a))
    {
        for (const auto to : squares_of(b))
        {
            if (clear_line(board, from, to))
            {
                return true;
            }
        }
    }
    return false;
}

// Whether Dispel Evil takes a creature for evil: by its alignment in Curse,
// by its record's flag in Secret (effhndlr.pas, EffDispelEvil and
// EffDispelEvilTouch, in its reconstruction).
bool dispel_evil_takes(const Fighter &f, GoldBoxGame game)
{
    if (game == GoldBoxGame::secret_of_the_silver_blades)
    {
        return (f.numbers.traits & trait::dispel_evil) != 0;
    }
    const int alignment = f.numbers.alignment;
    return alignment == 2 || alignment == 5 || alignment == 8;
}

// Whether a giant-bane weapon bites on the defender, one its record flags
// for it (target flag bit 0): Secret's (its effect 75, EffHolyWater) or the
// kit's giant-slaying sword (fxcombat.c, fx_damage_vs_immunity1), each 6d6
// and the wielder's Strength bonus.
bool giant_bane_strikes(const Fighter &attacker, const Fighter &defender, GoldBoxGame game)
{
    const bool secret_bane =
        game == GoldBoxGame::secret_of_the_silver_blades && attacker.has(effect::giant_bane);
    return (secret_bane || attacker.has(effect::kit_giant_bane)) &&
           (defender.numbers.traits & trait::giant_bane) != 0;
}

// Haste doubles a creature's rates of movement and attacks and Slow halves
// them; together they cancel (effhndlr.pas, EffHasted, EffSlowed).
int paced(const Fighter &f, int rate)
{
    const bool hasted = f.has(effect::hasted),
               slowed = f.has(effect::slowed) && !f.has(dq_effect::free_action);
    if (hasted && !slowed)
    {
        return rate * 2;
    }
    return slowed && !hasted ? rate / 2 : rate;
}

// A caster's level for a spell: its cleric or magic-user level, 12 for an
// item's own power (magic.pas, GetCasterLevel). Curse and the games after it
// count a paladin's cleric spells at its level less 8 (Secret at its level),
// a ranger's druid spells at its level less 7 and its magic-user spells at
// its level less 8, and cast at level 6 for one with no cleric or magic-user
// levels that is no paladin above 8 nor ranger above 7.
int caster_level(const Fighter &caster, const Spell &spell, GoldBoxGame game)
{
    // From an item a spell is cast at level 6, an item's own power at 12
    // (magic.pas, GetCasterLevel).
    if (caster.using_item >= 0)
    {
        return spell.caster_class == SpellClass::item ? 12 : 6;
    }
    const auto level = [&caster](ClassSlot slot)
    {
        return caster.numbers.class_levels[static_cast<std::size_t>(slot)];
    };
    if (spell.caster_class == SpellClass::item)
    {
        return 12;
    }
    if (game == GoldBoxGame::death_knights_of_krynn || game == GoldBoxGame::dark_queen_of_krynn)
    {
        const auto *moon = caster.find_effect(dkk_effect::casting_moon);
        const int encoded = moon ? moon->strength : 3;
        return game == GoldBoxGame::dark_queen_of_krynn
                   ? dark_queen_caster_level(caster.numbers.class_levels, spell.caster_class,
                                             encoded / 4, encoded % 4)
                   : death_knights_caster_level(caster.numbers.class_levels, spell.caster_class,
                                                encoded / 4, encoded % 4);
    }
    if (game == GoldBoxGame::pool_of_radiance)
    {
        return level(spell.caster_class == SpellClass::cleric ? ClassSlot::cleric
                                                              : ClassSlot::magic_user);
    }
    const int paladin = level(ClassSlot::paladin), ranger = level(ClassSlot::ranger);
    if (level(ClassSlot::cleric) == 0 && level(ClassSlot::magic_user) == 0 && paladin < 9 &&
        ranger < 8)
    {
        return 6;
    }
    const int paladin_less = game == GoldBoxGame::secret_of_the_silver_blades ? 0 : 8;
    switch (spell.caster_class)
    {
    case SpellClass::cleric:
        return std::max(level(ClassSlot::cleric), paladin - paladin_less);
    case SpellClass::druid:
        return std::max(ranger - 7, 0);
    default:
        return std::max(level(ClassSlot::magic_user), ranger - 8);
    }
}

// A spell's range in squares at a level: a touch is 1 (magic.pas).
int spell_range(const Spell &spell, int level)
{
    if (spell.range_fixed == 255)
    {
        return 1;
    }
    const int range = spell.range_fixed + spell.range_per_level * level;
    return range == 0 && spell.area != 0 ? 1 : range;
}

// The original's doubled distance between two squares along a line: 2 a
// straight step, 3 a diagonal one (pathing.pas, NextPathStep).
int doubled_distance(Cell a, Cell b)
{
    const int dx = std::abs(a.x - b.x), dy = std::abs(a.y - b.y);
    return 2 * std::max(dx, dy) + std::min(dx, dy);
}

// The spells built so far; the rest are not offered yet.
bool underwater_spell(int number)
{
    static constexpr std::array blocked{9, 47, 174, 315, 318, 324};
    return std::ranges::find(blocked, number) == blocked.end();
}

bool spell_built(int number)
{
    static constexpr std::array built{
        26, 27, 36, 41, 46, 57, 58, 59, 60, 61, 62, 63, 64, 65, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
        12, 13, 15, 16, 17, 19, 20, 21, 23, 24, 25, 28, 29, 30, 32, 33, 35, 37, 38, 40, 42, 34, 43,
        44, 45, 47, 48, 49, 50, 51, 52, 53, 54, 55,
        // Curse's (spells.cpp, curse_spells).
        158, 166, 168, 169, 170, 171, 172, 173, 174, 176, 177, 178, 179, 180, 181, 182, 183, 184,
        185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200,
        native_elixir_youth,
        // Secret's (spells.cpp, secret_spells).
        236, 256, 290, 296, 298, 310, 311, 312, 313, 314, 315, 316, 317,
        // Pools of Darkness's that are cast in fights (spells.cpp, darkness_spells).
        301, 303, 304, 308, 309, 318, 319, 320, 321, 322, 323, 324, 325, 326, 501, 502, 503, 504,
        505, 506, 507};
    return std::find(built.begin(), built.end(), number) != built.end();
}

Fighter *Session::find(EntityId id)
{
    return first_matching(fighters_, has_id(id));
}

const Fighter *Session::find(EntityId id) const
{
    return first_matching(fighters_, has_id(id));
}

void Session::log(Message message)
{
    log_.push_back({english(message), std::move(message)});
}

// A creature's movement in squares before Haste and Slow (cbtacts.pas,
// CalcCreatureMovement): the party's side moves by the script's modifier
// too, a byte that wraps, and a movement outside 1 to 96 is 1. Secret's
// boots of speed double it (charhlp.pas, RecalcMovement), as the kit's do
// (hero.c, recalc_stats).
int Session::squares_of_movement(const Fighter &f) const
{
    int squares = f.numbers.movement;
    if ((secret() && f.has(effect::boots_of_speed)) || f.has(effect::speed_boots))
    {
        squares *= 2;
    }
    if (f.side == Side::party)
    {
        squares = (squares + party_move_modifier_) % 256;
    }
    return squares < 1 || squares > 96 ? 1 : squares;
}

// The half-moves it has this round: twice its movement, doubled by Haste and
// halved by Slow. Pools of Darkness adds its script's modifier (cell 1Ch, a
// signed byte) to a party member's movement after Haste and Slow, and a sum
// below 1 is 1 (charhlp.pas, RecalcMovement).
int Session::half_moves_of(const Fighter &f) const
{
    if (darkness() && f.side == Side::party && party_move_modifier_ != 0)
    {
        const int modifier = static_cast<std::int8_t>(party_move_modifier_ & 0xFF);
        return 2 * std::max(paced(f, f.numbers.movement) + modifier, 1);
    }
    const int moves = paced(f, 2 * squares_of_movement(f));
    return underwater_ && !f.has(2150) ? moves / 3 : moves;
}

// Its movement in squares this round, as the original weighs flight and
// morale with it.
int Session::movement_of(const Fighter &f) const
{
    return half_moves_of(f) / 2;
}

// Every creature able to act rolls initiative: d6 with its Dexterity's
// missile adjustment, at least 1; a surprised one loses 6 in the first round,
// and a result outside 0 to 20 lets it not act (combat.pas, BeginRound).
void Session::begin_round()
{
    ++round_;
    if (buck_rogers())
    {
        ground_begin_round();
        return;
    }
    rules::CharacterSheet::Original calendar;
    if (game_of(identity_) == GoldBoxGame::death_knights_of_krynn)
    {
        const auto passed = static_cast<std::uint64_t>(round_ - 1);
        if (starting_time_minutes_ > std::numeric_limits<std::uint64_t>::max() - passed)
        {
            throw std::overflow_error("Combat calendar overflow");
        }
        death_knights_calendar(calendar,
                               death_knights_calendar_day(starting_time_minutes_ + passed));
    }
    for (auto &f : fighters_)
    {
        if (game_of(identity_) == GoldBoxGame::death_knights_of_krynn)
        {
            if (auto *moon = f.find_effect(dkk_effect::casting_moon))
            {
                const int robe = moon->strength / 4;
                if (robe >= 1 && robe <= 3)
                {
                    moon->strength =
                        robe * 4 + calendar.krynn_moon_phases[static_cast<std::size_t>(robe - 1)];
                }
            }
        }
    }
    for (auto &f : fighters_)
    {
        f.initiative = 0;
        if (!f.able())
        {
            continue;
        }
        f.initiative = std::max(roll(6) + f.numbers.initiative_bonus, 1);
        if (round_ == 1 && f.surprised)
        {
            f.initiative -= 6;
            if (f.initiative < 0 || f.initiative > 20)
            {
                f.initiative = 0;
            }
        }
        // A Treasures vilstrak moves twice as far in the first round
        // (its 146, at condition 18).
        f.half_moves = f.has(effect::immobile)                     ? 0
                       : f.has(effect::quick_start) && round_ <= 1 ? 2 * half_moves_of(f)
                                                                   : half_moves_of(f);
        f.cast_allowed = true;
        f.item_use_allowed = true;
        f.using_item = -1;
        f.pending_spell = f.aiming = f.picks_left = 0;
        f.picked.clear();
        f.attack_phase = 1;
        f.sweep_budget = f.numbers.attack_level;
        for (std::size_t form = 0; form < f.attacks_left.size(); ++form)
        {
            f.attacks_left[form] = attacks_this_round(
                paced(f, f.numbers.attacks[form].rate_per_two_rounds), static_cast<int>(round_));
        }
        // A loaded missile weapon attacks at its own rate, no more often than
        // it has shots (cbtacts.pas, RecalcAttacksLeft).
        if (const auto &missile = f.numbers.missile; missile && missile->loaded)
        {
            f.attacks_left[0] = attacks_this_round(paced(f, missile->rate_per_two_rounds),
                                                   static_cast<int>(round_));
            if (!missile->needs_no_ammunition)
            {
                f.attacks_left[0] = std::min(f.attacks_left[0], std::max(missile->quantity, 1));
            }
        }
    }
    age_the_hasted();
    if (ordered_rounds())
    {
        order_turns();
    }
}

// Secret's order for the round: each creature in turn goes before the first
// already placed whose initiative is lower, or the same on a d2 of 1
// (combat.pas, OrderCreatures, in its reconstruction).
void Session::order_turns()
{
    turn_order_.clear();
    for (const auto &f : fighters_)
    {
        auto place = turn_order_.begin();
        while (place != turn_order_.end())
        {
            const int other = find(*place)->initiative;
            if (f.initiative > other || (f.initiative == other && roll(2) == 1))
            {
                break;
            }
            ++place;
        }
        turn_order_.insert(place, f.id);
    }
}

// A Secret caster who begins a spell moves back among those yet to act, past
// each whose initiative is higher than its own, or the same on a d2 of 1
// (cbtacts.pas, CastSpellCombat, in its reconstruction).
void Session::reorder_after_cast(Fighter &caster)
{
    auto place = turn_order_.begin();
    while (place != turn_order_.end())
    {
        const int other = find(*place)->initiative;
        if (other < caster.initiative || (other == caster.initiative && roll(2) != 1))
        {
            break;
        }
        ++place;
    }
    turn_order_.insert(place, caster.id);
}

// The highest initiative left acts next; a tie goes to the higher of fresh
// d100 rolls (combat.pas, NextCreature). In Secret the next in the round's
// order still able to act does. With none left the round ends.
void Session::next_actor()
{
    actor_ = 0;
    while (outcome_ == rules::Outcome::ongoing)
    {
        Fighter *best = nullptr;
        int best_tiebreak = 0;
        while (ordered_rounds() && !best && !turn_order_.empty())
        {
            auto *first = find(turn_order_.front());
            turn_order_.erase(turn_order_.begin());
            if (first->able() && first->initiative > 0)
            {
                best = first;
            }
        }
        for (auto &f : fighters_)
        {
            // Secret's, Darkness's and the kit's order has picked already.
            if (ordered_rounds() || !f.able() || f.initiative <= 0)
            {
                continue;
            }
            if (!best || f.initiative > best->initiative)
            {
                best = &f;
                best_tiebreak = roll(100);
            }
            else if (f.initiative == best->initiative)
            {
                const auto tiebreak = roll(100);
                // A full tie goes to the later creature (combat.pas, NextCreature).
                if (tiebreak >= best_tiebreak)
                {
                    best = &f;
                    best_tiebreak = tiebreak;
                }
            }
        }
        if (best)
        {
            // A turn starts afresh: no flanking counted and no guard kept
            // (combat.pas, the creature's turn).
            best->flank_count = best->flank_spread = 0;
            best->guarding = false;
            if (best->ground)
            {
                if (!ground_turn_starts(*best) || check_morale(*best))
                {
                    continue;
                }
                actor_ = best->id;
                return;
            }
            // A helpless creature's turn ends as it starts (effhndlr.pas, the
            // helpless effects at TURN_START).
            // So does a stunned one's in Secret (Power Word Stun's 106, EffHelpless
            // at its ACTION_START), though it is not helpless.
            // And a dancing one's in Pools of Darkness (Otto's Irresistible
            // Dance's lost turn, EffClearGuard at TURN_START).
            if (best->helpless() || best->has(effect::stunned) || best->has(effect::dancing))
            {
                end_actions(*best);
                continue;
            }
            // A holding bite or a drinking stirge spends its turn on its victim
            // (effhndlr.pas, ACTION_START).
            attached_turn(*best);
            curse_turn_start(*best);
            kit_turn_start(*best);
            champions_turn_start(*best);
            if (best->initiative <= 0 || !best->able() || check_morale(*best))
            {
                continue;
            }
            // Silence within a square stops its casting for the round
            // (effhndlr.pas, EffSilence at ACTION_START).
            if (silenced(*best) && best->cast_allowed)
            {
                best->cast_allowed = false;
                best->item_use_allowed = false;
                log({"{name} is silenced.", {{"name", best->name}}});
            }
            // Coughing stops casting too (effhndlr.pas, EffCoughing).
            if (best->has(effect::coughing) && best->cast_allowed)
            {
                best->cast_allowed = false;
                best->item_use_allowed = false;
                log({"{name} is coughing.", {{"name", best->name}}});
            }
            // A spell begun earlier in the round goes off now (combat.pas,
            // ManualControl): its targets are chosen as it does.
            if (best->pending_spell)
            {
                const int pending = best->pending_spell;
                best->pending_spell = 0;
                start_aiming(*best, pending);
                if (best->initiative <= 0)
                {
                    continue;
                }
            }
            actor_ = best->id;
            return;
        }
        end_round();
    }
}

// The round ends: each dying creature bleeds a point and dies past 9 below 0
// (combat.pas, EndOfRound).
void Session::end_round()
{
    age_effects();
    settle_hammers();
    update_hostile_hit_points();
    round_end_effects();
    for (auto &f : fighters_)
    {
        breathe_cloud(f, true);
    }
    if (buck_rogers())
    {
        ground_end_round();
    }
    for (auto &f : fighters_)
    {
        if (!f.ground && f.condition == Condition::dying && ++f.overkill > 9)
        {
            f.condition = Condition::dead;
            log({"{name} dies of its wounds.", {{"name", f.name}}});
        }
    }
    settle_outcome();
    if (outcome_ == rules::Outcome::ongoing && has_round_limit() && round_ >= fight_end_round_)
    {
        end_quiet_fight();
    }
    if (outcome_ == rules::Outcome::ongoing)
    {
        join_arrivals();
        begin_round();
    }
}

// Those held back whose round has come join the fight where they would have
// stood, or on the free square nearest it (combat.pas,
// JoinDelayedCombatants; combatst.pas, JoinBattle). One with no room stays
// away.
void Session::join_arrivals()
{
    for (auto &f : fighters_)
    {
        if (f.arrival_round == 0 || static_cast<unsigned>(f.arrival_round) > round_)
        {
            continue;
        }
        f.arrival_round = 0;
        if (place_near(f, f.cell))
        {
            f.condition = Condition::okay;
            log({"{name} joins the battle.", {{"name", f.name}}});
        }
    }
}

// Puts the fighter on the open, free square nearest the wanted one, ring by
// ring; false, leaving it where it was, when there is none.
bool Session::place_near(Fighter &f, Cell wanted)
{
    // The squares its enemies can walk to: it is not shut away from them
    // (the original places it as the fight places its side, which never
    // leaves it walled in).
    const auto at = [this](Cell c)
    {
        return static_cast<std::size_t>(c.y * board_.width + c.x);
    };
    std::vector<bool> reached(static_cast<std::size_t>(board_.width * board_.height));
    std::vector<Cell> frontier;
    for (const auto &o : fighters_)
    {
        if (o.side != f.side && o.able() && board_.contains(o.cell))
        {
            reached[at(o.cell)] = true;
            frontier.push_back(o.cell);
        }
    }
    const bool enemies_stand = !frontier.empty();
    for (std::size_t n = 0; n < frontier.size(); ++n)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const Cell next{frontier[n].x + dx, frontier[n].y + dy};
                if (board_.contains(next) && !reached[at(next)] &&
                    !rules::blocks_movement(board_.at(next)))
                {
                    reached[at(next)] = true;
                    frontier.push_back(next);
                }
            }
        }
    }
    const auto fits = [&]
    {
        const auto squares = squares_of(f);
        return cell_free(f) &&
               std::all_of(squares.begin(), squares.end(),
                           [this](Cell c)
                           {
                               return board_.contains(c) && !rules::blocks_movement(board_.at(c));
                           }) &&
               (!enemies_stand || reached[at(f.cell)]);
    };
    const int reach = std::max(board_.width, board_.height);
    for (int ring = 0; ring <= reach; ++ring)
    {
        for (int dy = -ring; dy <= ring; ++dy)
        {
            for (int dx = -ring; dx <= ring; ++dx)
            {
                if (std::max(std::abs(dx), std::abs(dy)) != ring)
                {
                    continue;
                }
                f.cell = {wanted.x + dx, wanted.y + dy};
                if (fits())
                {
                    return true;
                }
            }
        }
    }
    f.cell = wanted;
    return false;
}

// Its actions end: no initiative, guard or movement left, though the attacks
// it has not made stay for a guard's strike or a free attack (charhlp.pas, EndActions).
void Session::end_actions(Fighter &fighter)
{
    fighter.initiative = 0;
    fighter.pending_spell = 0;
    fighter.guarding = false;
    fighter.half_moves = 0;
}

// Counts an attack on the defender for flanking: one more attacker, and the
// angle between its facing and the attack (0 to 4) added to its spread, mod 8
// (cbtacts.pas, NoteAttackDirection).
void Session::note_attack(const Fighter &attacker, Fighter &defender)
{
    ++defender.flank_count;
    int angle = (direction_toward(defender.cell, attacker.cell) - defender.direction + 8) % 8;
    if (angle > 4)
    {
        angle = 8 - angle;
    }
    defender.flank_spread = (defender.flank_spread + angle) % 8;
}

// An attack (cbtacts.pas, MakeAttack and RollAttacks). The defender turns to
// face the attacker unless it is struck from behind or already attacked three
// times. A thief backstabs a defender attacked more than once from the side it
// faces away from, against its Armor Class from behind less 2, for its level
// div 4 + 2 times the damage; others strike from behind there once the attacks
// on it have spread wide. Attacks run from the attacker's current form down to
// the first until the defender goes down; with none left its actions end.
void Session::make_attack(Fighter &attacker, Fighter &defender, bool from_behind)
{
    // The original counts its rounds from 0, so this round's is one less.
    fight_end_round_ = round_ - 1 + rounds_without_attacks;
    // Buck Rogers attacks are never made as from behind: the rear is found
    // from the defender's facing (cbtacts.pas RollAttacks).
    if (attacker.ground)
    {
        ground_attack(attacker, defender);
        return;
    }
    if (defender.flank_count < 3 && !from_behind)
    {
        defender.direction = direction_toward(defender.cell, attacker.cell);
    }
    attacker.direction = direction_toward(attacker.cell, defender.cell);
    attacker.lock = defender.id;
    attacker.facing_left = defender.cell.x < attacker.cell.x;
    const bool behind_its_back =
        defender.flank_count > 1 &&
        direction_toward(attacker.cell, defender.cell) == defender.direction;
    // Secret's thief backstabs only a defender of one square next to it
    // (cbtacts.pas, IsBackstab, in its reconstruction).
    const bool one_square = defender.footprint.width == 1 && defender.footprint.height == 1;
    const bool backstab = attacker.numbers.backstabs && behind_its_back &&
                          (!secret_engine() || (one_square && adjacent(attacker, defender)));
    const bool behind = from_behind || (behind_its_back && defender.flank_spread > 4);
    // Shield keeps the front Armor Class at 3 or better (effhndlr.pas, EffShield).
    const int front = defender.has(effect::shield) ? std::min(defender.numbers.armor_class, 3)
                                                   : defender.numbers.armor_class;
    // Blindness leaves both Armor Classes 4 worse (effhndlr.pas, EffBlind);
    // coughing leaves the rear one 2 worse, no worse than 10, and the front as
    // bad (EffCoughing).
    // Faerie Fire leaves both 2 worse (Curse made them better, docs/ORIGINAL-RULES.md, Bugs).
    // Pools of Darkness's dance leaves both 4 worse and Iron Skin 4 better
    // (effhndlr.pas, EffDancing, EffIronSkin).
    const int blind = (defender.has(effect::blind) ? 4 : 0) + (defender.has(207) ? 2 : 0) +
                      (defender.has(effect::dancing) ? 4 : 0) -
                      (defender.has(effect::iron_skin) ? 4 : 0);
    const bool coughs = defender.has(effect::coughing);
    const int rear = coughs ? std::max(std::min(defender.numbers.rear_armor_class + 2, 10),
                                       defender.numbers.rear_armor_class)
                            : defender.numbers.rear_armor_class;
    // Secret takes 4 from the Armor Class from behind for a backstab (RollAttacks).
    const int backstab_armor_class = rear + (secret_engine() ? 4 : 2);
    const int armor_class = (backstab ? backstab_armor_class
                             : behind ? rear
                             : coughs ? rear
                                      : front) +
                            blind;
    if (defender.helpless())
    {
        slay_helpless(attacker, defender);
        return;
    }
    if (backstab)
    {
        log({"{attacker} backstabs {name}.",
             {{"attacker", attacker.name}, {"name", defender.name}}});
    }
    int primary_hits = 0;
    // The first attacks made at this defender, hit or miss (cbtacts.pas,
    // RollAttacks' attackCounts), which pick Kalistes' weapon.
    int primary_attacks = 0;
    for (int phase = attacker.attack_phase; phase >= 0 && defender.able(); --phase)
    {
        auto &left = attacker.attacks_left[static_cast<std::size_t>(phase)];
        auto attack = attacker.numbers.attacks[static_cast<std::size_t>(phase)];
        if (phase == 0 && defender.numbers.large && attacker.numbers.large_attack)
        {
            attack = *attacker.numbers.large_attack;
        }
        // Each attack is spent before it is rolled, so that what a hit carries
        // (a rake) may take over the rest.
        while (left > 0 && defender.able())
        {
            --left;
            attacker.attack_phase = phase;
            primary_attacks += phase == 0 ? 1 : 0;
            const auto natural = roll(20);
            // Attacking makes it visible (fxengine.pas, AttackRoll).
            while (attacker.has(effect::invisible))
            {
                attacker.remove(effect::invisible);
            }
            const int modifier = attack_modifier(attacker, defender, game_of(identity_)) +
                                 prayer_bonus(attacker) + love_modifier(attacker, defender) -
                                 (aura_protects(attacker, defender) ? 2 : 0);
            // One made helpless partway through is hit by the rest (cbtacts.pas, RollAttacks).
            const bool aside = natural > 1 && turned_aside(defender);
            if ((aside || !attack_hits(natural, attacker.numbers.thac0, armor_class, modifier)) &&
                !defender.helpless())
            {
                log({"{attacker} misses {name}.",
                     {{"attacker", attacker.name}, {"name", defender.name}}});
                continue;
            }
            // A Secret creature that swallows whole does so on a natural 20,
            // before the blow lands (EffSwallowWhole, at its damage).
            if (attacker.has(effect::swallows_whole) && natural == 20)
            {
                swallow_whole(attacker, defender);
                continue;
            }
            int dice = 0;
            for (int die = 0; die < attack.dice; ++die)
            {
                dice += roll(std::max(attack.sides, 1));
            }
            auto damage = hit_damage(dice, attack.modifier);
            // A giant-bane weapon's blow is 6d6 and the Strength bonus in
            // place of the weapon's own (EffHolyWater, at its damage).
            if (giant_bane_strikes(attacker, defender, game_of(identity_)))
            {
                damage = roll(6) + roll(6) + roll(6) + roll(6) + roll(6) + roll(6) +
                         strength_damage_bonus(attacker.numbers.strength,
                                               attacker.numbers.exceptional_strength);
            }
            // Secret multiplies a backstab after the effects on the blow, so the
            // multiple covers a ranger's bonus too (CalcWeaponDamage), with
            // (level - 1) div 4 + 2; Pools of Darkness by 5 at most.
            if (backstab && !secret_engine())
            {
                damage *= attacker.numbers.thief_level / 4 + 2;
            }
            damage = weapon_damage_defenses(attacker, defender, damage, phase, primary_attacks,
                                            primary_hits);
            if (backstab && secret_engine())
            {
                const int multiple = (attacker.numbers.thief_level - 1) / 4 + 2;
                damage *= darkness() ? std::min(multiple, 5) : multiple;
            }
            log({"{attacker} hits {name} for {damage}.",
                 {{"attacker", attacker.name},
                  {"name", defender.name},
                  {"damage", std::to_string(damage)}}});
            primary_hits += phase == 0 ? 1 : 0;
            if (attacker.has(effect::paralyzing_strike))
            {
                damage = 0;
            }
            striker_ = attacker.id;
            inflict(defender, damage);
            striker_ = 0;
            // In Secret only a blow that did damage carries anything (RollAttacks).
            if (defender.able() && (damage > 0 || !secret_engine()))
            {
                // The original's attack roll, its bonuses in, a natural
                // 20 counting as 100 (cbtacts.pas, RollAttacks).
                on_hit(attacker, defender, phase, primary_hits, natural,
                       natural == 20 ? 100 : natural + modifier);
            }
            // Fire Touch burns what its bearer hits for 2d6, halved by a save
            // (effhndlr.pas, EffFireTouch); Pools of Darkness's fire with a hit
            // for 4d6 (EffFireDmg4d6).
            if (const auto *touch = attacker.find_effect(effect::fire_touch);
                touch && defender.able())
            {
                const bool saved = saving_throw(defender, 4, 0, &attacker, 9);
                nonweapon_damage(defender, roll(6) + roll(6), 2, saved, 9, touch->strength, 318,
                                 false);
            }
            if (attacker.has(effect::fire_hit) && defender.able())
            {
                const bool saved = saving_throw(defender, 4, 0, &attacker, 9);
                const int fire = roll(6) + roll(6) + roll(6) + roll(6);
                nonweapon_damage(defender, fire, 2, saved, 9, attacker.numbers.level, 0, false);
            }

            // Native MakeAttack stops the remaining blows of a disabled
            // attacker. The triggering hit still lands after retaliation;
            // neither this form nor a later one can attack from death.
            if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn && !attacker.able())
            {
                attacker.attacks_left = {};
                break;
            }
        }
    }
    attacker.sweep_budget = 0;
    if (attacker.attacks() == 0)
    {
        end_actions(attacker);
    }
}

// What an attacker's and a defender's effects do to its attack roll
// (fxengine.pas, AttackRoll): Bless +1, Curse -1, an invisible defender 4
// harder, and a protected one 2 harder for an attacker of the alignment it is
// protected from. The original lost that 2 to a roll made after it (an
// obvious bug, fixed).
int attack_modifier(const Fighter &attacker, const Fighter &defender, GoldBoxGame game)
{
    int modifier = (attacker.has(effect::bless) ? 1 : 0) - (attacker.has(effect::curse) ? 1 : 0);
    modifier += attacker.has(effect::aurak_combat_bonus) ? 2 : 0;
    modifier -= defender.has(effect::aurak_combat_bonus) ? 2 : 0;
    // Blindness and Bestow Curse take 4 from the attacker's rolls (effhndlr.pas, 33, 36).
    modifier -= attacker.has(effect::blind) ? 4 : 0;
    modifier -= attacker.has(dkk_effect::smoked) ? 2 : 0;
    if (defender.has(dkk_effect::dispel_evil_ward) &&
        (attacker.numbers.traits & trait::dispel_evil))
    {
        modifier -= 7;
    }
    modifier -= attacker.has(effect::bestow_curse) ? 4 : 0;
    // The kit's 148, which a ghast's stench gives, takes 2 (effects.c); so
    // does Pools of Darkness's dragon fear (effhndlr.pas, EffDragonFeared).
    modifier -= attacker.has(effect::sickened) ? 2 : 0;
    modifier -= attacker.has(dq_effect::venomed) ? 1 : 0;
    modifier -= attacker.has(effect::dragon_feared) ? 2 : 0;
    modifier -= attacker.has(effect::champions_sickened) ? 2 : 0;
    modifier -= attacker.has(effect::champions_afraid) ? 1 : 0;
    if (defender.has(effect::invisible) || defender.has(effect::dust))
    {
        modifier -= 4;
    }
    // A dwarf or gnome strikes its foes 1 better, and giants, bugbears and
    // gnolls strike it 4 worse (effhndlr.pas, 18, 26, 47, 48). The original
    // judged the giant by the defender's own target instead of the attacker,
    // reading eight names from its list of five (obvious bugs, fixed).
    const int foe = defender.numbers.kin, kind = attacker.numbers.kin;
    modifier += attacker.has(26) && (foe & kin::dwarf_foe) ? 1 : 0;
    modifier += attacker.has(18) && (foe & kin::gnome_foe) ? 1 : 0;
    modifier -= defender.has(47) && (kind & kin::giant) ? 4 : 0;
    modifier -= defender.has(48) && (kind & kin::bugbear) ? 4 : 0;
    const int alignment = attacker.numbers.alignment;
    const bool evil = alignment == 2 || alignment == 5 || alignment == 8;
    const bool good = alignment == 0 || alignment == 3 || alignment == 6;
    if ((evil && defender.has(effect::protection_from_evil)) ||
        (good && defender.has(effect::protection_from_good)))
    {
        modifier -= 2;
    }
    // Dispel Evil keeps the evil 7 worse off at its bearer (Curse, DispelEvil).
    modifier -= dispel_evil_takes(attacker, game) && defender.has(204) ? 7 : 0;
    // A giant-bane weapon strikes its quarry 1 better. The kit added the 1
    // after the roll, where it changed nothing (an obvious bug, fixed;
    // fxcombat.c, fx_damage_vs_immunity1).
    modifier += giant_bane_strikes(attacker, defender, game) ? 1 : 0;
    modifier +=
        attacker.has(effect::dragon_slayer) && (defender.numbers.traits & champions_dragon_trait)
            ? 2
            : 0;
    return modifier;
}

// The attack the attacker can make on the target, if any (cbtacts.pas,
// AimTargetMenu): melee next to it, without a missile weapon or with one it
// can throw; otherwise a loaded missile weapon shoots a target it sees within
// range, unless an enemy stands next to it and the weapon is not throwable.
std::optional<std::string> Session::attack_verb(const Fighter &attacker,
                                                const Fighter &target) const
{
    if (!can_target(attacker, target))
    {
        return std::nullopt;
    }
    const auto &missile = attacker.numbers.missile;
    const bool next_to = adjacent(attacker, target);
    if (!missile)
    {
        return next_to ? std::optional<std::string>("melee") : std::nullopt;
    }
    if (!missile->loaded || (!missile->throwable && enemy_next_to(attacker)))
    {
        return std::nullopt;
    }
    if (next_to)
    {
        return "melee";
    }
    if (underwater_ || distance(attacker, target) > missile->range ||
        !sees(board_, attacker, target))
    {
        return std::nullopt;
    }
    return missile->thrown ? "throw" : "ranged";
}

bool Session::enemy_next_to(const Fighter &f) const
{
    return std::any_of(fighters_.begin(), fighters_.end(),
                       [&f](const Fighter &other)
                       {
                           return other.side != f.side && other.able() && adjacent(f, other);
                       });
}

// A missile attack, which spends a shot for each attack made: a thrown
// weapon's last throw leaves its thrower empty-handed, and a launcher's last
// arrow or quarrel leaves it nothing to shoot (cbtacts.pas, MakeAttack).
void Session::shoot(Fighter &attacker, Fighter &target)
{
    // A Buck Rogers weapon spends its own rounds (ground_attack).
    if (attacker.ground)
    {
        make_attack(attacker, target, false);
        return;
    }
    const int before = attacker.attacks_left[0];
    make_attack(attacker, target, false);
    auto &missile = *attacker.numbers.missile;
    if (missile.needs_no_ammunition)
    {
        return;
    }
    if (missile.quantity > 0)
    {
        missile.quantity = std::max(missile.quantity - (before - attacker.attacks_left[0]), 0);
    }
    if (missile.quantity > 0)
    {
        return;
    }
    if (!missile.thrown)
    {
        missile.loaded = false;
        return;
    }
    auto &numbers = attacker.numbers;
    numbers.thac0 = missile.empty_handed_thac0;
    numbers.attacks[0] = missile.empty_handed_attack;
    numbers.backstabs = missile.empty_handed_backstabs;
    numbers.large_attack.reset();
    numbers.missile.reset();
    numbers.striker.reset();
    numbers.weapon_type = -1;
}

// A fighter sweeps through level-0 enemies: when the defender is one, next to
// it, and more of them stand around it than it has attacks this round, it
// strikes each once, the defender first, up to its attack level (cbtacts.pas,
// SweepAttack).
bool Session::sweep(Fighter &attacker, Fighter &defender)
{
    if (attacker.attacks_left[0] >= attacker.sweep_budget || defender.numbers.level != 0 ||
        !adjacent(attacker, defender))
    {
        return false;
    }
    std::vector<Fighter *> weak{&defender};
    for (auto &f : fighters_)
    {
        if (&f != &defender && f.side != attacker.side && f.able() && f.numbers.level == 0 &&
            adjacent(attacker, f))
        {
            weak.push_back(&f);
        }
    }
    if (static_cast<int>(weak.size()) <= attacker.attacks_left[0])
    {
        return false;
    }
    weak.resize(
        std::min<std::size_t>(weak.size(), static_cast<std::size_t>(attacker.sweep_budget)));
    log({"{attacker} sweeps.", {{"attacker", attacker.name}}});
    for (auto *target : weak)
    {
        note_attack(attacker, *target);
        attacker.attacks_left = {1, 0};
        attacker.attack_phase = 0;
        make_attack(attacker, *target, false);
    }
    end_actions(attacker);
    return true;
}

// Each enemy next to the mover now but not after its step strikes once, as
// from behind, while the mover stays up: unless it no longer acts this round,
// has already been attacked and has the mover out of the five directions in
// front of it (cbtacts.pas, AttacksOfOpportunity).
void Session::free_attacks(Fighter &mover, Cell next)
{
    auto stepped = mover;
    stepped.cell = next;
    for (auto &enemy : fighters_)
    {
        if (!mover.able() || enemy.side == mover.side || !enemy.able() || enemy.helpless() ||
            enemy.has(effect::bite_hold) || enemy.has(effect::blood_drain) ||
            !can_target(enemy, mover) || !adjacent(enemy, mover) || adjacent(enemy, stepped))
        {
            continue;
        }
        bool facing = false;
        for (int scan = enemy.direction + 6; scan <= enemy.direction + 10; ++scan)
        {
            facing = facing || leads_toward(enemy.cell, mover.cell, scan % 8);
        }
        if (enemy.initiative <= 0 && enemy.flank_count > 0 && !facing)
        {
            continue;
        }
        int phase = enemy.numbers.attacks[0].rate_per_two_rounds == 0 ? 1 : 0;
        for (int form = 0; form < 2; ++form)
        {
            if (enemy.attacks_left[static_cast<std::size_t>(form)] > 0)
            {
                phase = form;
            }
        }
        auto &left = enemy.attacks_left[static_cast<std::size_t>(phase)];
        left = std::max(left, 1);
        enemy.attack_phase = phase;
        // Native's final Aurak forms (its raw 210 and 213, both carrying 165)
        // strike no one leaving, after their attack is readied (cbaction.c).
        if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
            enemy.has(effect::aurak_invulnerable))
        {
            continue;
        }
        log({"{attacker} strikes {name} leaving.",
             {{"attacker", enemy.name}, {"name", mover.name}}});
        make_attack(enemy, mover, true);
    }
}

// A guarding enemy the mover comes next to strikes it once and stops guarding
// (cbtacts.pas, GuardersAttack).
void Session::guards_strike(Fighter &mover)
{
    for (auto &guard : fighters_)
    {
        if (!mover.able() || !guard.guarding || guard.side == mover.side || !guard.able() ||
            guard.helpless() || !adjacent(guard, mover))
        {
            continue;
        }
        guard.guarding = false;
        if (guard.attacks() == 0)
        {
            continue;
        }
        note_attack(guard, mover);
        log({"{attacker} strikes from guard.", {{"attacker", guard.name}}});
        make_attack(guard, mover, false);
    }
}

// One step: the enemies left behind strike first, then the mover pays 2
// half-moves, or 3 on a diagonal, times the ground's cost, its flanking counts
// clear, and guards it comes next to strike (cbtacts.pas, CombatMoveCreature).
void Session::step_to(Fighter &mover, Cell next)
{
    // A Buck Rogers creature leaving an enemy is not struck for it; guards
    // strike it as it steps (cbtacts.pas CombatMoveCreature).
    if (mover.ground)
    {
        const auto price = grid_for(mover).step_price(mover.cell, next);
        if (!price || price->need > mover.half_moves)
        {
            mover.half_moves = 0;
            return;
        }
        mover.half_moves -= price->charge;
        mover.direction = direction_toward(mover.cell, next);
        mover.facing_left = next.x < mover.cell.x || (next.x == mover.cell.x && mover.facing_left);
        mover.cell = next;
        mover.flank_count = mover.flank_spread = 0;
        ground_after_step(mover);
        return;
    }
    free_attacks(mover, next);
    if (!mover.able())
    {
        mover.half_moves = 0;
        return;
    }
    const auto price = grid_for(mover).step_price(mover.cell, next);
    if (!price || price->need > mover.half_moves)
    {
        mover.half_moves = 0;
        return;
    }
    mover.half_moves -= price->charge;
    mover.direction = direction_toward(mover.cell, next);
    mover.facing_left = next.x < mover.cell.x || (next.x == mover.cell.x && mover.facing_left);
    mover.cell = next;
    mover.flank_count = mover.flank_spread = 0;
    breathe_cloud(mover);
    guards_strike(mover);
    if (!mover.able())
    {
        mover.half_moves = 0;
    }
}

// Bandaging saves a dying party member next to the one bandaging (the user,
// 2026-11-12; the original reached any on the field).
bool Session::can_bandage(const Fighter &bandager, const Fighter &dying) const
{
    return bandager.side == Side::party && dying.side == Side::party &&
           dying.condition == Condition::dying && adjacent(bandager, dying);
}

// Exactly 0 Hit Points leaves a creature unconscious, below 0 dying, and
// more than 9 below 0 dead (charhlp.pas, InflictDamage).
void Session::inflict(Fighter &target, int damage)
{
    if (target.ground)
    {
        ground_inflict(target, damage);
        return;
    }
    if (target.has(effect::aurak_invulnerable))
    {
        return;
    }
    // Any damage stops casting this round and loses a spell being cast
    // (cbtacts.pas, ProcessAttackResult).
    if (damage > 0)
    {
        target.cast_allowed = false;
        if (target.pending_spell || target.aiming)
        {
            lose_spell(target);
        }
    }
    if (target.hit_points >= damage)
    {
        target.hit_points -= damage;
        if (target.hit_points > 0)
        {
            return;
        }
        // The animated dead die at exactly 0 (charhlp.pas, InflictDamage).
        target.condition = target.has(effect::animated) ? Condition::dead : Condition::unconscious;
        log({target.condition == Condition::dead ? "{name} is killed."
                                                 : "{name} falls unconscious.",
             {{"name", target.name}}});
    }
    else
    {
        target.overkill = damage - target.hit_points;
        target.hit_points = 0;
        target.condition = target.overkill > 9 ? Condition::dead : Condition::dying;
        log({target.condition == Condition::dead ? "{name} is killed." : "{name} is dying.",
             {{"name", target.name}}});
    }
    target.initiative = 0;
    end_effects_at_zero(target);
    went_down(target);
}

// It loses the effects that end at 0 Hit Points; a hold or a drain it had
// lets its victim go (fxengine.pas, RemoveEffectsHp0).
void Session::end_effects_at_zero(Fighter &f)
{
    for (const auto code : effects_ending_at_zero)
    {
        if (code == effect::bite_hold || code == effect::blood_drain)
        {
            release(f, code);
            release(f, effect::engulfing);
            release(f, effect::hugging);
        }
        else if ((code == effect::charm || code == effect::animated) && f.has(code))
        {
            const auto *node = f.find_effect(code);
            remove_with_handler(f, static_cast<std::size_t>(node - f.numbers.effects.data()));
        }
        else
        {
            f.remove(code);
        }
    }
    // Its effects at 0 Hit Points then play (fxengine.pas, condition
    // ZERO_HIT_POINTS); one gone off the field has none.
    if (f.condition != Condition::fled && f.condition != Condition::surrendered)
    {
        give_up_crystal(f);
        love_at_fall(f);
    }
}

// A hold or a drain ends: its victim, whose combat number its strength keeps,
// may move again.
void Session::release(Fighter &holder, int code)
{
    const auto *hold = holder.find_effect(code);
    if (!hold)
    {
        return;
    }
    if (auto *victim = find(static_cast<EntityId>(hold->strength)))
    {
        victim->remove(effect::immobile);
        victim->remove(effect::suffocating);
        if (code == effect::squeezing)
        {
            victim->remove(effect::held_still);
        }
    }
    holder.remove(code);
}

void Session::hold_fast(Fighter &holder, Fighter &victim, int hold, int victim_effect)
{
    holder.numbers.effects.push_back({hold, 0, static_cast<int>(victim.id), true});
    victim.numbers.effects.push_back({victim_effect, 0, 0, false});
    victim.half_moves = 0;
}

bool Session::weapon_named(const Fighter &f, int word) const
{
    constexpr std::uint8_t weapon_slot = 0;
    return std::ranges::any_of(held_now(f),
                               [word](const Held &h)
                               {
                                   return !h.gone && h.carried.item.readied() &&
                                          h.carried.type.worn_location == weapon_slot &&
                                          std::ranges::find(h.carried.item.name_components, word) !=
                                              h.carried.item.name_components.end();
                               });
}

bool Session::armor_named(const Fighter &f, int word) const
{
    constexpr std::uint8_t armor_slot = 2;
    return std::ranges::any_of(held_now(f),
                               [word](const Held &h)
                               {
                                   return !h.gone && h.carried.item.readied() &&
                                          h.carried.type.worn_location == armor_slot &&
                                          std::ranges::find(h.carried.item.name_components, word) !=
                                              h.carried.item.name_components.end();
                               });
}

// What a hit that leaves its target standing carries (cbtacts.pas, RollAttacks;
// effhndlr.pas). The original stacked a new hold at each hit of a holding
// bite; one hold at a time here (an obvious bug, fixed).
void Session::on_hit(Fighter &attacker, Fighter &defender, int phase, int primary_hits, int natural,
                     int attack_roll)
{
    if (champions() && phase == 0 && attacker.has(65) && defender.able())
    {
        poison(defender, 4);
    }
    const auto effects = phase == 0 ? std::span<const int>(primary_hit_effects)
                                    : std::span<const int>(secondary_hit_effects);
    for (const auto code : effects)
    {
        if (code == 0 || !attacker.has(code) || !defender.able())
        {
            continue;
        }
        // Gateway's poison rides only the first attack, its paralysis both;
        // the other games' the second only (effhndlr.pas, conditions 2 and 3).
        if (!champions() &&
            ((code == 64 && (phase == 0) != gateway()) || (code == 67 && phase == 0 && !gateway())))
        {
            continue;
        }
        switch (code)
        {
        case dkk_effect::stunning_hit:
            if (!saving_throw(defender, 1, 0, &attacker))
            {
                (void)add_with_message(defender, {336, 3, 0, false}, false, false, 0,
                                       attacker.numbers.level, nullptr);
            }
            break;
        case dkk_effect::fire_touch:
            nonweapon_damage(defender, roll(6) + roll(6), 2,
                             saving_throw(defender, 4, 0, &attacker), 0, 0, 0, false);
            break;
        case effect::kapak_paralysis:
        case effect::paralyzing_strike:
            paralyze(defender, game_of(identity_) == GoldBoxGame::dark_queen_of_krynn ? 10 : 100,
                     0);
            break;
        case effect::champions_ghoul_touch:
            if (defender.numbers.race != elf_race)
            {
                paralyze(defender, 100, 0);
            }
            break;
        case effect::slight_poison:
            poison(defender, 1);
            break;
        case 64:
            poison(defender, 0);
            break;
        case dq_effect::spore_touch:
            if (!defender.has(effect::disease_proof))
            {
                defender.numbers.effects.push_back({dq_effect::spore_infection, 1440, 255, true});
                log({"{name} is diseased.", {{"name", defender.name}}});
            }
            attacker.numbers.effects.push_back({2194, 0, 255, false});
            kill(attacker);
            break;
        case dq_effect::poison_bite:
            if (!saving_throw(defender, 0, 0, &attacker))
            {
                // This shipped post-hit handler rolls the extra d6 after
                // damage has already been applied; it does not strike again.
                (void)roll(6);
                (void)add_with_message(defender,
                                       {effect::paralysis, roll(6) + roll(6) + roll(6), 12, false},
                                       false, false, 0, 0, nullptr);
            }
            break;
        case dq_effect::mummy_touch:
            if (!defender.has(250) && !defender.has(dq_effect::rot_recovery))
            {
                defender.numbers.effects.push_back({dq_effect::rot_recovery, 14400, 255, true});
                defender.numbers.effects.push_back(
                    {effect::kit_rotting, 43200, roll(6) * 32 + 31, true});
                log({"{name} is diseased.", {{"name", defender.name}}});
            }
            break;
        case dq_effect::enchanted_paralysis:
            if (!saving_throw(defender, 4, -4, &attacker))
            {
                (void)roll(6);
                (void)roll(6);
                (void)roll(6);
                paralyze(defender, 10, 0);
            }
            break;
        case dq_effect::venom_touch:
            if (!saving_throw(defender, 0, 0, &attacker))
            {
                defender.remove(dq_effect::venomed);
                defender.numbers.effects.push_back({dq_effect::venomed, roll(4), 255, false});
                log({"{name} is crippled by venom.", {{"name", defender.name}}});
            }
            break;
        case effect::drinks_blood:
            // A Gateway stirge takes hold on 1 to 4 of a d10 (cbtacts.pas,
            // AttackDrainBlood).
            if (roll(10) <= 4 && !attacker.has(effect::drinking_blood) &&
                !defender.has(effect::immobile))
            {
                hold_fast(attacker, defender, effect::drinking_blood, effect::immobile);
                log({"{attacker} attaches to {name}.",
                     {{"attacker", attacker.name}, {"name", defender.name}}});
            }
            break;
        case effect::constricts:
            // A Gateway squid's arms take hold on 1 to 4 of a d10
            // (cbtacts.pas, AttackConstrict).
            if (roll(10) <= 4 && !attacker.has(effect::constricting) &&
                !defender.has(effect::immobile))
            {
                hold_fast(attacker, defender, effect::constricting, effect::immobile);
                log({"{attacker} constricts {name}.",
                     {{"attacker", attacker.name}, {"name", defender.name}}});
            }
            break;
        case effect::charming_touch:
        {
            // A harpy's touch charms to its side unless saved against as a
            // spell (effhndlr.pas, EffCharmGaze; Treasures' EffHarpyTouch).
            if (defender.has(effect::charm))
            {
                break;
            }
            if (!saving_throw(defender, 4, 0, &attacker))
            {
                defender.numbers.effects.push_back(
                    {effect::charm, 0, (attacker.side == Side::opposition ? 128 : 0) + 3, true});
                charm(defender, attacker.side);
                log({"{name} is charmed.", {{"name", defender.name}}});
            }
            break;
        }
        case effect::disease_touch:
            // Gateway's mummy's touch diseases unless saved against: Strength
            // and Hit Points waste away at once (EffDiseaseTouch).
            if (!saving_throw(defender, 0, 0, &attacker))
            {
                diseased_now(defender, attacker.numbers.level, false);
            }
            break;
        case effect::rat_disease:
            // A Treasures rat's bite diseases one time in 20 unless saved
            // against (EffRatDisease).
            if (roll(100) <= 5 && !defender.has(34) && !saving_throw(defender, 0, 0, &attacker))
            {
                diseased_now(defender, attacker.numbers.level, true);
            }
            break;
        case effect::crushes:
            // A kampfult's hit holds its victim to be crushed (EffKampfultCrush).
            if (!attacker.has(effect::squeezing))
            {
                hold_fast(attacker, defender, effect::squeezing, effect::held_still);
                log({"{name} is being crushed.", {{"name", defender.name}}});
            }
            break;
        case effect::grabs_2d8:
        case effect::grabs_4d5:
        case effect::grabs_6d4:
            // A yeti grabs on an attack roll of 20 or more (T20GrabHelper).
            if (attack_roll >= 20 && !attacker.has(effect::squeezing))
            {
                hold_fast(attacker, defender, effect::squeezing, effect::held_still);
                log({"{attacker} grabs {name}.",
                     {{"attacker", attacker.name}, {"name", defender.name}}});
            }
            break;
        case effect::swallows_on_twenty:
            // A Treasures remorhaz swallows on an attack roll of 20 or more,
            // after the blow (EffSwallowWhole).
            if (attack_roll >= 20)
            {
                swallow_whole(attacker, defender);
            }
            break;
        case effect::poison_touch:
            // Secret's poison rides the second attack form. A creature with
            // none (a centipede, a giant snake or spider, a medusa) would never
            // poison, so its first form carries it (an obvious bug, fixed).
            if (phase == 0 && attacker.numbers.attacks[1].rate_per_two_rounds > 0)
            {
                break;
            }
            secret_poison(defender, 0);
            break;
        case effect::strong_poison_touch:
            secret_poison(defender, -2);
            break;
        case effect::vorpal:
            // A primary blow on a natural 17 or more is the kit's critical
            // hit, which beheads but a creature proof against it (fxcore.c,
            // fx_attack_roll; fxcombat.c, fx_vorpal).
            if (natural >= 17 && (defender.numbers.traits & trait::beheading_proof) == 0)
            {
                kill(defender, "{name} is beheaded.");
            }
            break;
        case effect::ghoul_paralysis:
            // The kit's ghoul holds 10 rounds unless saved against, an elf
            // never (effects.c, fx_ghoul_paralysis).
            if (defender.numbers.race != elf_race)
            {
                paralyze(defender, 10, 0);
            }
            break;
        case effect::adjacent_paralysis:
            // The kit's second blow that holds one beside it 10 rounds
            // unless saved against (fx_adjacent_paralysis).
            if (distance(attacker, defender) < 2)
            {
                paralyze(defender, 10, 0);
            }
            break;
        case effect::fire_blow:
        {
            // The kit's burning second blow: 2d6 of magical fire, halved by
            // a save against spells (fxcombat.c, fx_fire_damage_2d6).
            const bool saved = saving_throw(defender, 4, 0, &attacker, 9);
            nonweapon_damage(defender, roll(6) + roll(6), 2, saved, 9, attacker.numbers.level, 0,
                             false);
            break;
        }
        case effect::stone_touch:
            // A cockatrice's touch, saved against as petrification (EffFleshToStone).
            if (!saving_throw(defender, 1, 0, &attacker))
            {
                turn_to_stone(defender);
            }
            break;
        case effect::ghoul_touch:
            // Held 10 rounds unless saved against (EffGhoulPara).
            paralyze(defender, 10, 0);
            break;
        case 65:
            poison(defender, 4);
            break;
        case 66:
            poison(defender, 2);
            break;
        case 70:
            poison(defender, -2);
            break;
        case 67:
            paralyze(defender, roll(8) + roll(8), 0);
            break;
        case 68:
            // Race 2 is the elf (half-elves are not spared).
            if (defender.numbers.race != 2)
            {
                paralyze(defender, 63, 0);
            }
            break;
        case 69:
            // The original passed -2 and its paralysis ignored it (an obvious bug, fixed).
            paralyze(defender, roll(9) + 10, -2);
            break;
        case 79:
            // Fire on a hit, 2d10 and magical (effhndlr.pas, EffFireTouch).
            nonweapon_damage(defender, roll(10) + roll(10), 0, false, 9, 0, 0, false);
            break;
        case 80:
            // Acid on a hit, 1d4 (EffAcidTouch).
            nonweapon_damage(defender, roll(4), 0, false, 16, 0, 0, false);
            break;
        case 85:
            drain_levels(defender, 1);
            break;
        case 86:
            drain_levels(defender, 2);
            break;
        case 87:
            // Mummy rot: no healing for 10 days, and death in 1d6 months
            // unless cured (effhndlr.pas, EffRotTouch).
            if (!defender.has(effect::rot))
            {
                defender.numbers.effects.push_back({effect::rot, 14400, 0, true});
                defender.numbers.effects.push_back(
                    {effect::mummy_rot, 43200, roll(6) * 16 + 15, true});
                log({"{name} is diseased.", {{"name", defender.name}}});
            }
            break;
        case effect::rot_touch:
            // The kit's mummy rot (effects.c, fx_disease_touch): no healing
            // for 10 days (its 215, the fight's 50), and rotting a month at a
            // time for 1d6 months, its strength the months by 32s, which
            // takes Charisma and at last the victim's life as time passes
            // (OriginalModule::pass_time). The kit also spared one that held
            // its effect 50, Pool of Radiance's rot's number, which is a hot
            // Fire Shield in the kit's (Bugs).
            if (!defender.has(effect::rot))
            {
                defender.numbers.effects.push_back({effect::rot, 14400, 255, true});
                defender.numbers.effects.push_back(
                    {effect::kit_rotting, 43200, roll(6) * 32 + 31, true});
                log({"{name} is diseased.", {{"name", defender.name}}});
            }
            break;
        case 76:
            // A stirge drinks 12 to 15 Hit Points' worth, its strength the
            // victim's, which cannot move meanwhile (effhndlr.pas, EffBloodAttach).
            attacker.numbers.effects.push_back(
                {effect::blood_drain, 12 * 16 + 15, static_cast<int>(defender.id), true});
            defender.numbers.effects.push_back({effect::immobile, 0, 0, false});
            defender.half_moves = 0;
            break;
        case 73:
            if (primary_hits == 2)
            {
                rake(attacker, defender);
            }
            break;
        case 345:
        {
            // Dispel Evil: an evil creature it strikes that fails its save is
            // sent away, and the spell ends (Curse, sp_dispel_evil).
            if (!dispel_evil_takes(defender, game_of(identity_)))
            {
                break;
            }
            if (saving_throw(defender, 4, 0, &attacker))
            {
                log({"{name} resists dispel evil.", {{"name", defender.name}}});
                break;
            }
            log({"{name} is dispelled.", {{"name", defender.name}}});
            defender.condition = Condition::gone;
            defender.hit_points = 0;
            defender.initiative = 0;
            end_effects_at_zero(defender);
            went_down(defender);
            attacker.remove(204);
            attacker.remove(345);
            break;
        }
        case 322:
            // A dracolich's touch paralyzes for the fight (Curse, ovr013).
            paralyze(defender, 0, 0);
            break;
        case 323:
            // A cold touch, 2d8 of magical cold (Curse, ovr013).
            nonweapon_damage(defender, roll(8) + roll(8), 0, false, 10, 0, 0, false);
            break;
        case 296:
            // An owlbear hugs on an attack roll of 18 or more and holds its
            // victim (Curse, AffectOwlbearHugAttackCheck).
            // Gateway's owlbear never hugs one held already (AttackHug).
            if (natural >= 18 && !attacker.has(effect::hugging) &&
                (!gateway() || !defender.has(effect::immobile)))
            {
                attacker.numbers.effects.push_back(
                    {effect::hugging, 0, static_cast<int>(defender.id), true});
                defender.numbers.effects.push_back({effect::immobile, 0, 0, false});
                defender.half_moves = 0;
                log({"{attacker} hugs {name}.",
                     {{"attacker", attacker.name}, {"name", defender.name}}});
            }
            break;
        case dq_effect::constricting_touch:
            if (roll(10) > 4)
            {
                break;
            }
            [[fallthrough]];
        case 257:
            // A shambling mound whose two blows both hit engulfs its victim,
            // which suffocates in 2d4 of its turns (Curse, engulfs).
            // Gateway's on 1 to 4 of a d10 at each hit (its AttackEngulf).
            if ((gateway() ? roll(10) <= 4 : primary_hits == 2) &&
                !defender.has(effect::immobile) && !defender.has(effect::suffocating) &&
                !attacker.has(effect::engulfing) &&
                !(game_of(identity_) == GoldBoxGame::dark_queen_of_krynn && defender.has(2150)))
            {
                attacker.numbers.effects.push_back(
                    {effect::engulfing, 0, static_cast<int>(defender.id), true});
                defender.numbers.effects.push_back({effect::immobile, 0, 0, false});
                defender.numbers.effects.push_back(
                    {effect::suffocating, 0, roll(4) + roll(4), false});
                defender.half_moves = 0;
                log({code == dq_effect::constricting_touch ? "{attacker} constricts {name}."
                                                           : "{attacker} engulfs {name}.",
                     {{"attacker", attacker.name}, {"name", defender.name}}});
            }
            break;
        case 77:
            if (defender.numbers.strength < 19 && !attacker.has(effect::bite_hold))
            {
                attacker.numbers.effects.push_back(
                    {effect::bite_hold, 0, static_cast<int>(defender.id), true});
                defender.numbers.effects.push_back({effect::immobile, 0, 0, false});
                defender.half_moves = 0;
                log({"{name} is held fast.", {{"name", defender.name}}});
            }
            break;
        default:
            break;
        }
    }
}

void Session::diseased_now(Fighter &victim, int level, bool lasting)
{
    constexpr int diseased = 34, wasting_strength = 43, wasting_hit_points = 44;
    if (lasting)
    {
        victim.numbers.effects.push_back({diseased, 0, level, false});
    }
    for (const int wasting : {wasting_strength, wasting_hit_points})
    {
        if (!victim.has(wasting))
        {
            victim.numbers.effects.push_back({wasting, 0, level, false});
        }
    }
    log({"{name} is diseased.", {{"name", victim.name}}});
}

// A poisoned touch kills unless saved against (save 0, paralysis and poison).
void Session::poison(Fighter &victim, int modifier)
{
    if (saving_throw(victim, 0, modifier))
    {
        return;
    }
    log({"{name} is poisoned.", {{"name", victim.name}}});
    victim.numbers.effects.push_back({effect::poisoned, 0, 0, false});
    kill(victim);
}

void Session::paralyze(Fighter &victim, int minutes, int modifier)
{
    if (saving_throw(victim, 0, modifier))
    {
        return;
    }
    // Native natural touches dispatch the resulting paralysis through
    // effect immunity too, after the save. Paralysis-proof creatures remain
    // immune to an enchanted draconian's touch as well as magical holds.
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
        (victim.has(109) || victim.has(111) || victim.has(125) ||
         victim.has(effect::aurak_invulnerable)))
    {
        return;
    }
    log({"{name} is paralyzed.", {{"name", victim.name}}});
    victim.numbers.effects.push_back({effect::paralysis, minutes, 12, false});
}

// Secret's poisoned blow (effhndlr.pas, PoisonTarget, in its reconstruction):
// a victim poisonous itself saves better by its own poison's strength
// against plain poison, and with no penalty against strong poison.
void Session::secret_poison(Fighter &victim, int modifier)
{
    if (const auto *own = victim.find_effect(effect::poison_touch))
    {
        modifier = modifier < 0 ? 0 : modifier + own->strength;
    }
    poison(victim, modifier);
}

// Turned to stone: out of the fight for good.
void Session::turn_to_stone(Fighter &victim)
{
    victim.condition = Condition::stoned;
    victim.hit_points = 0;
    victim.initiative = 0;
    log({"{name} is stoned.", {{"name", victim.name}}});
    end_effects_at_zero(victim);
    went_down(victim);
}

// A Secret remorhaz swallows its victim whole: dead at once (effhndlr.pas,
// EffSwallowWhole, in its reconstruction).
void Session::swallow_whole(Fighter &swallower, Fighter &victim)
{
    log({"{name} is swallowed whole.", {{"name", victim.name}}});
    victim.condition = Condition::dead;
    victim.hit_points = 0;
    victim.initiative = 0;
    end_effects_at_zero(victim);
    went_down(victim);
    log({roll(100) <= 50 ? "{name} belches in satisfaction." : "{victim} is digested instantly.",
         {{"name", swallower.name}, {"victim", victim.name}}});
}

// A level drain, no save (effhndlr.pas, EffDrainOne and EffDrainTwo). Each
// level takes the Hit Points of a level (its maximum over its levels) from
// its maximum and its current; one below level 2 is dead forever. Otherwise
// the class whose level took the most experience (the later on a tie) loses
// a level, its THAC0 and saving throws following the class tables, and one
// left with no Hit Points is killed. The original divided by zero for a
// creature without class levels, which loses its highest level's share here;
// and it kept the drained class's level as the highest, where the highest of
// its classes is kept here (both obvious bugs, fixed).
void Session::drain_levels(Fighter &victim, int levels)
{
    log({levels == 1 ? "{name} is drained a level." : "{name} is drained 2 levels.",
         {{"name", victim.name}}});
    auto &numbers = victim.numbers;
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn && numbers.native_progression)
    {
        for (int n = 0; n < levels && victim.condition != Condition::gone; ++n)
        {
            auto &progression = *numbers.native_progression;
            const auto loss =
                roll_native_drain(progression, numbers.class_levels, numbers.max_hit_points,
                                  numbers.constitution, progression.race, random_);
            if (!loss)
            {
                break;
            }
            const auto before = numbers.class_levels;
            const int missing = std::max(0, numbers.max_hit_points - victim.hit_points);
            progression.peak_class_levels[loss->slot] =
                std::max(progression.peak_class_levels[loss->slot], loss->before_level);
            --numbers.class_levels[loss->slot];
            progression.rolled_hit_points =
                std::max(0, progression.rolled_hit_points - loss->rolled_hit_points);
            progression.experience = loss->experience;
            progression.previous_experience = loss->previous_experience;
            progression.previous_max_hit_points = loss->previous_max_hit_points;
            progression.drains.push_back(*loss);
            // The campaign's public dead-character maximum remains at least
            // one; the exact receipt still retains the source's full HP loss.
            numbers.max_hit_points = std::max(1, numbers.max_hit_points - loss->hit_points);
            victim.hit_points = loss->dead ? 0 : std::max(0, numbers.max_hit_points - missing);
            numbers.thac0 +=
                thac0(numbers.class_levels, game_of(identity_)) - thac0(before, game_of(identity_));
            const auto old_saves = saving_throws(before, game_of(identity_));
            const auto new_saves = saving_throws(numbers.class_levels, game_of(identity_));
            for (std::size_t save = 0; save < numbers.saving_throws.size(); ++save)
            {
                numbers.saving_throws[save] += new_saves[save] - old_saves[save];
            }
            numbers.level =
                *std::max_element(numbers.class_levels.begin(), numbers.class_levels.end());
            const auto attack_rate = [](const ClassLevels &classes)
            {
                const int warrior = std::max({level_in(classes, ClassSlot::fighter),
                                              level_in(classes, ClassSlot::paladin),
                                              level_in(classes, ClassSlot::monk)});
                const int ranger = level_in(classes, ClassSlot::ranger);
                return std::max(warrior > 12  ? 4
                                : warrior > 6 ? 3
                                              : 2,
                                ranger > 14  ? 4
                                : ranger > 7 ? 3
                                             : 2);
            };
            const int attack_change = attack_rate(numbers.class_levels) - attack_rate(before);
            numbers.attacks[0].rate_per_two_rounds =
                std::max(0, numbers.attacks[0].rate_per_two_rounds + attack_change);
            victim.attacks_left[0] =
                std::min(victim.attacks_left[0],
                         attacks_this_round(paced(victim, numbers.attacks[0].rate_per_two_rounds),
                                            static_cast<int>(round_)));
            trim_native_spells(numbers);
            if (loss->dead || victim.has(effect::animated))
            {
                victim.condition = Condition::gone;
                victim.hit_points = 0;
                victim.initiative = 0;
                log({"{name} is dead forever.", {{"name", victim.name}}});
                end_effects_at_zero(victim);
                return;
            }
            if (victim.hit_points == 0)
            {
                victim.condition = Condition::unconscious;
                victim.initiative = 0;
                end_effects_at_zero(victim);
                log({"{name} falls unconscious.", {{"name", victim.name}}});
            }
        }
        return;
    }
    for (int n = 0; n < levels && victim.condition != Condition::gone; ++n)
    {
        auto &classes = numbers.class_levels;
        const int total = std::accumulate(classes.begin(), classes.end(), 0);
        const int per_level = numbers.max_hit_points / std::max({total, numbers.level, 1});
        // One dead forever keeps the maximum it had, which the game weighs
        // as it takes its member back.
        if (numbers.level >= 2 && !victim.has(effect::animated))
        {
            numbers.max_hit_points = std::max(numbers.max_hit_points - per_level, 1);
        }
        victim.hit_points = std::max(victim.hit_points - per_level, 0);
        if (numbers.level < 2 || victim.has(effect::animated))
        {
            victim.condition = Condition::gone;
            victim.hit_points = 0;
            victim.initiative = 0;
            log({"{name} is dead forever.", {{"name", victim.name}}});
            end_effects_at_zero(victim);
            return;
        }
        std::size_t drained = classes.size();
        int most = -1;
        for (std::size_t slot = 0; slot < classes.size(); ++slot)
        {
            if (classes[slot] <= 0)
            {
                continue;
            }
            const int experience = experience_for_level(static_cast<ClassSlot>(slot), classes[slot],
                                                        game_of(identity_))
                                       .value_or(0);
            if (experience >= most)
            {
                most = experience;
                drained = slot;
            }
        }
        if (drained < classes.size())
        {
            const auto before = classes;
            --classes[drained];
            const auto game = game_of(identity_);
            numbers.thac0 += thac0(classes, game) - thac0(before, game);
            const auto saves_before = saving_throws(before, game);
            const auto saves_after = saving_throws(classes, game);
            for (std::size_t save = 0; save < numbers.saving_throws.size(); ++save)
            {
                numbers.saving_throws[save] += saves_after[save] - saves_before[save];
            }
            numbers.level = *std::max_element(classes.begin(), classes.end());
        }
        else
        {
            --numbers.level;
        }
        if (victim.hit_points == 0 && victim.condition != Condition::dead)
        {
            kill(victim);
            return;
        }
    }
}

// Casting (combat.pas, CastSpellCombat): a spell with a casting time takes it
// off the caster's initiative and goes off at its next turn this round; one
// without goes off now.
void Session::begin_cast(Fighter &caster, int spell_number)
{
    const auto &known = *game_spell(spell_number);
    const int delay = known.casting_time / 3;
    if (delay == 0)
    {
        start_aiming(caster, spell_number);
        return;
    }
    log({"{name} begins casting {spell}.",
         {{"name", caster.name}, {"spell", std::string(known.name), true}}});
    caster.pending_spell = spell_number;
    caster.initiative = std::max(caster.initiative - delay, 1);
    if (ordered_rounds())
    {
        reorder_after_cast(caster);
    }
    actor_ = 0;
}

// Choosing targets as the spell goes off (cbtacts.pas, SelectCombatTargets):
// a spell on the caster goes off at once; others wait for their aim.
void Session::start_aiming(Fighter &caster, int spell_number)
{
    const auto &known = *game_spell(spell_number);
    const int area = known.area & 0x0F;
    caster.aiming = spell_number;
    caster.picked.clear();
    caster.blast_delay = -1;
    // Fire Shield waits for its flames to be chosen (SpellFireShield).
    if (spell_number == fire_shield)
    {
        caster.picks_left = 0;
        return;
    }
    if (area == 0)
    {
        resolve_spell(caster, {caster.id}, caster.cell);
        return;
    }
    caster.picks_left = area <= 7 ? (area & 3) + 1 : 1;
}

// Whether the caster can single out a creature for its spell: within range,
// in sight, and no veto (cbtacts.pas, AimTargetMenu). Healing reaches the
// downed but not the dead, Secret's Stone to Flesh a statue.
bool Session::reachable_by_spell(const Fighter &caster, const Fighter &target, int spell) const
{
    constexpr int heal = 236, druid_cure = 298, stone_to_flesh = 313;
    const auto &known = *game_spell(spell);
    const bool heals = spell == 3 || spell == heal || spell == druid_cure;
    const bool slows_poison =
        spell == 26 && target.has(effect::poisoned) && target.condition == Condition::dead;
    const bool unstones = spell == stone_to_flesh && target.condition == Condition::stoned;
    if (!(target.able() || (heals && target.downed()) || slows_poison || unstones) ||
        std::find(caster.picked.begin(), caster.picked.end(), target.id) != caster.picked.end())
    {
        return false;
    }
    return distance(caster, target) <=
               spell_range(known, caster_level(caster, known, game_of(identity_))) &&
           sees(board_, caster, target) && can_target(caster, target);
}

std::vector<rules::Command> Session::aiming_commands(const Fighter &caster) const
{
    std::vector<rules::Command> commands;
    const auto &known = *game_spell(caster.aiming);
    const int area = known.area & 0x0F;
    const auto command = [&](std::string verb)
    {
        rules::Command c;
        c.revision = revision_;
        c.actor = caster.id;
        c.verb = std::move(verb);
        c.label = std::string(known.name);
        c.item = static_cast<unsigned>(caster.aiming);
        return c;
    };
    if (caster.aiming == delayed_blast_spell && caster.blast_delay < 0)
    {
        // How many rounds it waits (the original's 1 to 50 segments, in tens).
        static constexpr std::array<const char *, 6> delays{"No delay",       "Delay 1 round",
                                                            "Delay 2 rounds", "Delay 3 rounds",
                                                            "Delay 4 rounds", "Delay 5 rounds"};
        for (std::size_t rounds = 0; rounds < delays.size(); ++rounds)
        {
            auto choice = command("blast_delay");
            choice.label = delays[rounds];
            choice.item = static_cast<unsigned>(rounds);
            commands.push_back(choice);
        }
    }
    else if (caster.aiming == fire_shield)
    {
        // The flames, hot or cold (SpellFireShield's menu).
        for (const auto flames : {Flames::hot, Flames::cold})
        {
            if (underwater_ && flames == Flames::hot)
            {
                continue;
            }
            auto choice = command("flames");
            choice.label = flames == Flames::hot ? "Hot flames" : "Cold flames";
            choice.item = static_cast<unsigned>(flames);
            commands.push_back(choice);
        }
    }
    else if (area >= 8 && area <= 14)
    {
        // A square in range and in sight, open ground (cbtacts.pas, ManualAimCursor).
        const int range = spell_range(known, caster_level(caster, known, game_of(identity_)));
        for (int y = 0; y < board_.height; ++y)
        {
            for (int x = 0; x < board_.width; ++x)
            {
                const Cell cell{x, y};
                if (rules::blocks_movement(board_.at(cell)) ||
                    doubled_distance(caster.cell, cell) / 2 > range ||
                    !clear_line(board_, caster.cell, cell))
                {
                    continue;
                }
                auto aim = command("aim");
                aim.destination = cell;
                commands.push_back(aim);
            }
        }
    }
    else
    {
        if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn && area == 15)
        {
            // Native case15 singles out a creature, or uses the high nibble
            // as a radius around empty ground (cbaction.c, combat_spell_targets).
            const int range = spell_range(known, caster_level(caster, known, game_of(identity_)));
            for (int y = 0; y < board_.height; ++y)
            {
                for (int x = 0; x < board_.width; ++x)
                {
                    const Cell cell{x, y};
                    if (standing_at(cell) || board_.at(cell) == rules::Terrain::obstacle ||
                        doubled_distance(caster.cell, cell) / 2 > range ||
                        !clear_line(board_, caster.cell, cell))
                    {
                        continue;
                    }
                    auto aim = command("aim");
                    aim.destination = cell;
                    commands.push_back(aim);
                }
            }
        }
        for (const auto &f : fighters_)
        {
            if (reachable_by_spell(caster, f, caster.aiming))
            {
                auto aim = command("aim");
                aim.target = f.id;
                aim.destination = f.cell;
                aim.intent = known.offensive ? rules::Intent::hostile : rules::Intent::friendly;
                commands.push_back(aim);
            }
        }
        if (!caster.picked.empty())
        {
            commands.push_back(command("aim_done"));
        }
    }
    commands.push_back(command("abort"));
    // The AI aims a spell that comes due on its turn (combat.pas, ManualControl).
    commands.push_back(command("quick"));
    return commands;
}

// Everyone standing within a radius of a square that a line from it reaches,
// allies and the caster too; the nearest first (cbtacts.pas, SelectCombatTargets).
std::vector<EntityId> Session::within(Cell aim, int radius) const
{
    std::vector<std::pair<int, EntityId>> found;
    for (const auto &f : fighters_)
    {
        if (!f.able())
        {
            continue;
        }
        if (const int gap = burst_gap(aim, f); in_burst(gap, radius))
        {
            found.emplace_back(gap, f.id);
        }
    }
    std::stable_sort(found.begin(), found.end(),
                     [](const auto &a, const auto &b)
                     {
                         return a.first < b.first;
                     });
    std::vector<EntityId> ids;
    for (const auto &[gap, id] : found)
    {
        ids.push_back(id);
    }
    return ids;
}

// How far a burst at a square reaches to a creature: the doubled distance to
// the nearest of its squares a line from the square reaches, 1000 for none.
int Session::burst_gap(Cell aim, const Fighter &f) const
{
    int nearest = 1000;
    for (const auto square : squares_of(f))
    {
        if (clear_line(board_, aim, square))
        {
            nearest = std::min(nearest, doubled_distance(aim, square));
        }
    }
    return nearest;
}

// The damage dice an area spell rolls once for all it reaches, by the
// caster's level (magic.pas): Fireball's and Lightning Bolt's level d6, Cone
// of Cold's level d4 and the level, Ice Storm's 3d10, a blast's 6d6, Delayed
// Blast Fireball's level d6 and the level, and a Necklace of Missiles' 3, 5
// or 7 d6 as its missile (1d3) falls; none for any other spell.
SpellDice Session::shared_dice(int number, int level, int necklace_missile)
{
    switch (number)
    {
    case 47:
    case 51:
        return {level, 6, 0};
    case 64:
        return {necklace_missile * 2 + 1, 6, 0};
    case 187:
        return {3, 10, 0};
    case 192:
        return {level, 4, level};
    case 198:
        return {6, 6, 0};
    case delayed_blast_spell:
        return {level, 6, level};
    default:
        return {};
    }
}

// A spell lost: its memorized copy is gone (cbtacts.pas, ProcessAttackResult).
void Session::lose_spell(Fighter &caster)
{
    const int lost = caster.aiming ? caster.aiming : caster.pending_spell;
    auto &spells = caster.numbers.spells;
    if (const auto copy = std::find(spells.begin(), spells.end(), lost); copy != spells.end())
    {
        spells.erase(copy);
    }
    if (caster.pending_spell || caster.aiming)
    {
        log({"{name} loses a spell.", {{"name", caster.name}}});
    }
    caster.pending_spell = caster.aiming = caster.picks_left = 0;
    caster.picked.clear();
}

// The spell goes off at its targets (magic.pas, CastSpell): the caster turns
// to its aim and shows itself, the memorized copy is spent, and its actions end.
void Session::resolve_spell(Fighter &caster, std::vector<EntityId> targets, Cell aim, Flames flames)
{
    const int number = caster.aiming;
    const auto &known = *game_spell(number);
    const int level = caster_level(caster, known, game_of(identity_));
    caster.aiming = caster.picks_left = 0;
    caster.picked.clear();
    if (aim != caster.cell)
    {
        caster.direction = direction_toward(caster.cell, aim);
        caster.facing_left = aim.x < caster.cell.x;
    }
    while (caster.has(effect::invisible))
    {
        caster.remove(effect::invisible);
    }
    const int item = caster.using_item;
    if (item >= 0)
    {
        spend_item(caster, item, number);
    }
    else
    {
        auto &spells = caster.numbers.spells;
        if (const auto copy = std::find(spells.begin(), spells.end(), number); copy != spells.end())
        {
            spells.erase(copy);
        }
    }
    log({"{name} casts {spell}.",
         {{"name", caster.name}, {"spell", std::string(known.name), true}}});
    if (underwater_ && !underwater_spell(number))
    {
        log({"That has no effect underwater!", {}});
        end_actions(caster);
        return;
    }
    const auto roll_dice = [this](int count, int sides)
    {
        damage_dice_ = count;
        int total = 0;
        for (int n = 0; n < count; ++n)
        {
            total += roll(sides);
        }
        return total;
    };
    if (champions())
    {
        for (auto &id : targets)
        {
            const auto *target = find(id);
            if (target && target->has(effect::death_knight_reflects) &&
                ((known.area < 8 && roll(10) == 1) || known.area > 14))
            {
                log({"{name} reflects the spell.", {{"name", target->name}}});
                id = caster.id;
            }
        }
    }
    std::erase_if(targets,
                  [&](EntityId id)
                  {
                      const auto &target = *find(id);
                      if (!refuses_spell(target, number))
                      {
                          return false;
                      }
                      log({"{name} is unaffected.", {{"name", target.name}}});
                      return true;
                  });
    std::vector<EntityId> kept;
    switch (number)
    {
    case 505:
    case 1:
        // Bless: the caster's side, but not one with an enemy next to it.
        for (const auto id : targets)
        {
            const auto &f = *find(id);
            if (f.side == caster.side && !enemy_next_to(f))
            {
                kept.push_back(id);
            }
        }
        cast_generic(caster, number, level, kept, 0, 0);
        break;
    case 2:
        for (const auto id : targets)
        {
            if (find(id)->side != caster.side)
            {
                kept.push_back(id);
            }
        }
        cast_generic(caster, number, level, kept, 0, 0);
        break;
    case 3:
    case 298:
        // Cure Light Wounds (the cleric's, and Secret's druid's): 1d8, not
        // through rot; the dying stop bleeding but stay down (magic.pas, the cure).
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.has(effect::rot) || !(f.able() || f.downed()))
            {
                continue;
            }
            f.hit_points = std::min(f.hit_points + roll(8), f.numbers.max_hit_points);
            if (f.condition == Condition::dying)
            {
                f.condition = Condition::unconscious;
                f.overkill = 0;
            }
            log({"{name} is healed.", {{"name", f.name}}});
        }
        break;
    case 4:
        cast_generic(caster, number, level, targets, roll(8), 8);
        break;
    case 507:
    case 9:
        // Burning Hands: the caster's level in fire.
        cast_generic(caster, number, level, targets, level, 9);
        break;
    case 506:
    case 10:
        // Charm Person cannot take a creature of type above 1 or a large one
        // (magic.pas, the charm); Secret and Pools of Darkness judge by race
        // and flags instead (refuses_spell).
        for (const auto id : targets)
        {
            auto &f = *find(id);
            // Treasures' takes any but a monster its flags do not call a
            // person or one of more than a square (its SpellCharmPerson);
            // every character stands on one square (its chargen.pas), though
            // a tall one counts as large here.
            const bool refused =
                treasures() ? f.numbers.race == 0 &&
                                  ((f.numbers.traits & trait::person) == 0 || f.numbers.large)
                            : f.numbers.creature_type > 1 || f.numbers.large;
            if (!secret_engine() && !champions() && refused)
            {
                log({"{name} is unaffected.", {{"name", f.name}}});
                continue;
            }
            // Treasures' frees one charmed already, with no save (its
            // SpellCharmPerson).
            if (treasures() && f.has(effect::charm))
            {
                const auto *node = f.find_effect(effect::charm);
                remove_with_handler(f, static_cast<std::size_t>(node - f.numbers.effects.data()));
                log({"{name} is uncharmed.", {{"name", f.name}}});
                continue;
            }
            kept.push_back(id);
        }
        cast_generic(caster, number, level, kept, 0, 0);
        break;
    case 15:
    {
        // Magic Missile: a missile of 1d4+1 for every two levels, the first at
        // level 1. The original's level div 2 gave none at level 1, against
        // its manual (an obvious bug, fixed).
        const int missiles = (level + 1) / 2;
        cast_generic(caster, number, level, targets, roll_dice(missiles, 4) + missiles, 8);
        break;
    }
    case 20:
        cast_generic(caster, number, level, targets, roll(8) + level, 12);
        break;
    case 21:
        cast_sleep(level, aim);
        break;
    case 23:
    case 49:
    {
        // Hold Person: the fewer held, the harder to save, each in turn from
        // the last picked; a creature of type above 1 or a large one is not
        // held (magic.pas, the hold). In Secret and Pools of Darkness a
        // monster is never held, whatever its size (magic.pas, their
        // multi-target resolvers).
        const int count = static_cast<int>(targets.size());
        const int modifier = count == 1 ? (number == 23 ? -2 : -3) : count == 2 ? -1 : 0;
        for (auto id = targets.rbegin(); id != targets.rend(); ++id)
        {
            auto &f = *find(*id);
            // Treasures' holds no monster either, and its size is no matter
            // (treasures_refuses_spell).
            const bool held_off = secret_engine() || treasures()
                                      ? f.numbers.race == 0
                                      : f.numbers.creature_type > 1 || f.numbers.large;
            if (held_off)
            {
                log({"{name} is unaffected.", {{"name", f.name}}});
                continue;
            }
            cast_generic(caster, number, level, {*id}, 0, 0, modifier);
        }
        break;
    }
    case 32:
    {
        // Native packs 1d4 images above its five-bit caster level.
        const int minutes = known.minutes_fixed + known.minutes_per_level * level;
        const int images = roll(4);
        const int strength =
            game_of(identity_) == GoldBoxGame::dark_queen_of_krynn ? (images << 5) | level : images;
        add_with_message(caster, {effect::mirror_image, minutes, strength, false}, false, false, 0,
                         level, &known);
        break;
    }
    case 37:
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.has(effect::blind))
            {
                f.remove(effect::blind);
                log({"{name} can see.", {{"name", f.name}}});
            }
        }
        break;
    case 40:
        // Cause Disease lasts 1d6 x 10 minutes before the disease takes hold.
        cast_generic(caster, number, level, targets, 0, 0, 0, roll(6) * 10);
        break;
    case 42:
        // Prayer on the caster, or in Gateway on those it touches: its
        // strength keeps the caster's side in bit 4.
        for (const auto id : targets)
        {
            (void)add_with_message(*find(id),
                                   {effect::prayer, level,
                                    (caster.side == Side::opposition ? 0x10 : 0) + level, false},
                                   false, false, 0, level, &known);
        }
        break;
    case 504:
    case 43:
    case 189:
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.has(effect::bestow_curse))
            {
                f.remove(effect::bestow_curse);
                log({"{name} is un-cursed.", {{"name", f.name}}});
            }
        }
        break;
    case 34:
        lay_cloud(caster, level, aim, CloudKind::stinking);
        break;
    case 191:
        lay_cloud(caster, level, aim, CloudKind::deadly);
        break;
    case 26:
        for (const auto id : targets)
        {
            slow_poison(*find(id), level);
        }
        break;
    case 27:
    {
        // Snake Charm: every snake on the field, either side, whose Hit Points
        // the caster's own can pay for (magic.pas, the snake charm). Secret
        // and Pools of Darkness know a snake by its flags and pass over one
        // charmed already (SpellSnakeCharm).
        int pool = caster.hit_points;
        for (auto &f : fighters_)
        {
            const bool snake =
                (secret_engine() || champions())
                    ? (f.numbers.traits & trait::snake) && !f.has(effect::snake_charm)
                    : f.numbers.creature_type == 14;
            if (!f.able() || !snake || f.hit_points > pool)
            {
                continue;
            }
            pool -= f.hit_points;
            add_with_message(f, {effect::snake_charm, 0, level, false}, false, false, 0, level,
                             &known);
        }
        break;
    }
    case 36:
    case 190:
        animate_dead(caster, level);
        break;
    case 57:
        // A Potion of Speed cures Slow, or hastes for 5d4 rounds.
        if (caster.has(effect::slowed))
        {
            caster.remove(effect::slowed);
            log({"{name} is cured.", {{"name", caster.name}}});
        }
        else
        {
            add_with_message(caster, {effect::hasted, roll_dice(5, 4), 12, false}, false, false, 0,
                             level, &known);
        }
        break;
    case 58:
        // Keoghtum's Ointment: poison out (the dead stay dead), else disease,
        // else 1d4+8 healed.
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.has(effect::poisoned))
            {
                f.remove(effect::poisoned);
                f.remove(effect::slow_poison);
            }
            else if (f.has(effect::rot) || f.has(34) || f.has(43))
            {
                for (const int code :
                     {34, 43, 44, 31, effect::rot, effect::mummy_rot, effect::kit_rotting})
                {
                    f.remove(code);
                }
                log({"{name} is cured.", {{"name", f.name}}});
            }
            else if (f.able())
            {
                f.hit_points = std::min(f.hit_points + roll(4) + 8, f.numbers.max_hit_points);
                log({"{name} is healed.", {{"name", f.name}}});
            }
        }
        break;
    case 60:
        // A Javelin of Lightning: 1d6+20 at its mark, then a bolt of 3 squares
        // of 20 beyond it (magic.pas, SpellJavelinOfLightning).
        lightning_at(aim, roll(6) + 20, level);
        lightning_path(caster.cell, aim, 3, 20, level, false);
        break;
    case 61:
        cast_generic(caster, number, level, targets, 0, 0, 0, roll_dice(5, 4));
        break;
    case 62:
        caster.hit_points =
            std::min(caster.hit_points + roll_dice(2, 4) + 2, caster.numbers.max_hit_points);
        log({"{name} is healed.", {{"name", caster.name}}});
        break;
    case 63:
        // Dust of Disappearance for 2d10 x 10 rounds in a fight.
        cast_generic(caster, number, level, targets, 0, 0, 0, roll_dice(2, 10) * 10);
        break;
    case 64:
    {
        // A Necklace of Missiles: a fireball of 3, 5 or 7 dice.
        const auto dice = shared_dice(number, level, roll(3));
        cast_generic(caster, number, level, fireball_reach(targets, aim),
                     roll_dice(dice.count, dice.sides) + dice.bonus, 9);
        break;
    }
    case 65:
        // A Wand of Magic Missiles: 2d4+2, which Shield does not stop.
        cast_generic(caster, number, level, targets, roll_dice(2, 4) + 2, 8);
        break;
    case 41:
    case 46:
        // Dispel Magic on everyone within a square of the aim. The original
        // dispelled the first of them once for each (an obvious bug, fixed).
        for (const auto id : targets)
        {
            dispel(*find(id), level);
        }
        break;
    case 51:
    {
        // Lightning Bolt: the caster's level in d6, one roll, at the aim and
        // then along a bolt of 8 squares in Pool of Radiance, 9 in Treasures
        // and 7 in the other games (magic.pas, SpellLightningBolt; the kit's
        // spellfx.c, spell_lightning_bolt). OpenGoldBox ran Pool of
        // Radiance's bolt in every game (Bugs).
        const auto dice = shared_dice(number, level);
        const int damage = roll_dice(dice.count, dice.sides) + dice.bonus;
        if (underwater_)
        {
            cast_generic(caster, number, level, within(aim, 2), damage, 9);
            break;
        }
        lightning_at(aim, damage, level);
        lightning_path(caster.cell, aim, bolt_length(), damage, level, true);
        break;
    }
    case 47:
    {
        // Fireball: the caster's level in d6, one roll for all, saved for half.
        const auto dice = shared_dice(number, level);
        cast_generic(caster, number, level, fireball_reach(targets, aim),
                     roll_dice(dice.count, dice.sides) + dice.bonus, 9);
        break;
    }
    case 178:
        // Entangle: out of doors, each that fails its save is held by the
        // plants (Curse's SpellEntangle); in a dungeon nothing grows.
        if (!outdoors_)
        {
            log({"Nothing happens.", {}});
            break;
        }
        for (const auto id : targets)
        {
            auto &target = *find(id);
            const bool saved = saving_throw(target, 4, 0, &caster);
            (void)add_with_message(target, {336, known.minutes_fixed, level, false}, true, saved, 0,
                                   level, &known);
        }
        break;
    case 48:
    case 55:
    {
        // Haste the caster's side, or Slow its enemies, up to its level of
        // them, the nearest first; one under the opposite spell is only freed
        // of it (magic.pas, Haste and Slow).
        const bool haste = number == 48;
        for (const auto id : haste_slow_takes(caster, number, level, targets))
        {
            auto &f = *find(id);
            if (!haste && game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
                f.has(dq_effect::free_action))
            {
                continue;
            }
            const int opposite = haste ? effect::slowed : effect::hasted;
            if (f.has(opposite))
            {
                f.remove(opposite);
                log({"{name} is cured.", {{"name", f.name}}});
                continue;
            }
            cast_generic(caster, number, level, {id}, 0, 0);
        }
        break;
    }
    case 181:
        // Charm Monsters: Charm Person on any creature (Curse let it charm
        // only what Charm Person can, as it does Hold Monster).
        cast_generic(caster, number, level, targets, 0, 0);
        break;
    case 182:
    {
        // Confusion: at most 2d8 of those in its area.
        auto confused = targets;
        confused.resize(
            std::min<std::size_t>(confused.size(), static_cast<std::size_t>(roll(8) + roll(8))));
        cast_generic(caster, number, level, confused, 0, 0);
        break;
    }
    case 184:
        // Fear: those on a path 6 squares long and 3 wide that fail to save
        // flee (Curse, SpellFear). Pools of Darkness's are forced to flee
        // while the fright lasts (its SpellFear sets their turned flag, which
        // its EffFear clears as the fright ends).
        for (const auto id : path_area(caster, aim, 6, 3))
        {
            auto &f = *find(id);
            if (!saving_throw(f, 4, 0, &caster) &&
                add_with_message(f, {342, known.minutes_per_level * level, level, true}, false,
                                 false, 0, level, &known))
            {
                f.fleeing = true;
                f.turned = f.turned || darkness();
            }
        }
        break;
    case 186:
        // Fumble: a failed save leaves it helpless, a made one slowed
        // (Curse, SpellFumble).
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
                f.has(dq_effect::free_action))
            {
                continue;
            }
            const int minutes = known.minutes_per_level * level;
            const bool saved = saving_throw(f, 4, 0, &caster);
            // Native Fumble uses its own end-actions effect rather than
            // the earlier game's inherited Fumble effect.
            const int fumbled =
                game_of(identity_) == GoldBoxGame::dark_queen_of_krynn ? effect::stunned : 31;
            (void)add_with_message(f, {saved ? effect::slowed : fumbled, minutes, level, false},
                                   false, false, 0, level, &known);
        }
        break;
    case 193:
        // Feeblemind: a cleric saves 1 better, a magic-user 4 worse, any
        // other 2 worse; one feebleminded casts nothing (Curse, AffectFeebleMind).
        for (const auto id : targets)
        {
            auto &f = *find(id);
            const auto &l = f.numbers.class_levels;
            const int modifier = l[static_cast<std::size_t>(ClassSlot::cleric)] > 0       ? 1
                                 : l[static_cast<std::size_t>(ClassSlot::magic_user)] > 0 ? -4
                                                                                          : -2;
            cast_generic(caster, number, level, {id}, 0, 0, modifier);
            if (f.has(268))
            {
                f.numbers.spells.clear();
                f.cast_allowed = false;
            }
        }
        break;
    case 170:
        // Sticks to Snakes: those under 6 Hit Dice fight snakes for a while;
        // stronger ones smash them flat (Curse, SpellSticksToSnakes).
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.numbers.level >= 6)
            {
                log({"{name} smashes them flat.", {{"name", f.name}}});
                continue;
            }
            (void)add_with_message(f, {203, 0, level, false}, false, false, 0, level, &known);
        }
        break;
    case 173:
        // Dispel Evil: on the caster, whose blows send the evil away.
        (void)add_with_message(caster, {204, known.minutes_fixed, level, false}, false, false, 0,
                               level, &known);
        caster.numbers.effects.push_back({345, known.minutes_fixed, level, false});
        break;
    case 185:
    {
        // Fire Shield, hot or cold as its caster chose (the AI and Quick at
        // random, hot on a d10 above 5). Darkness pairs them with its
        // retaliating shield (magic.pas, SpellFireShield).
        const int minutes = known.minutes_fixed + known.minutes_per_level * level;
        const bool hot =
            !underwater_ && (flames == Flames::random ? roll(10) > 5 : flames == Flames::hot);
        (void)add_with_message(caster, {hot ? 250 : 254, minutes, level, false}, false, false, 0,
                               level, &known);
        caster.numbers.effects.push_back(
            {darkness() || game_of(identity_) == GoldBoxGame::dark_queen_of_krynn
                 ? effect::retaliating_shield
                 : 335,
             minutes, level, false});
        break;
    }
    case 183:
    {
        // Dimension Door: to the square aimed at, free of any hold.
        if (standing_at(aim) || !board_.contains(aim) || rules::blocks_movement(board_.at(aim)))
        {
            break;
        }
        for (auto &other : fighters_)
        {
            for (const int hold : {effect::hugging, effect::engulfing, effect::bite_hold})
            {
                if (const auto *held = other.find_effect(hold);
                    held && held->strength == static_cast<int>(caster.id))
                {
                    release(other, hold);
                }
            }
        }
        caster.remove(effect::immobile);
        caster.cell = aim;
        log({"{name} teleports.", {{"name", caster.name}}});
        break;
    }
    case 158:
    case 171:
    case 199:
    {
        // Curse's cures (ovr023): Cure Serious Wounds 2d8+1, Cure Critical
        // Wounds 3d8+3, a healing power 2d4+2.
        const int healed = number == 158   ? roll_dice(2, 8) + 1
                           : number == 171 ? roll_dice(3, 8) + 3
                                           : roll_dice(2, 4) + 2;
        for (const auto id : targets)
        {
            auto &f = *find(id);
            if (f.has(effect::rot) || !(f.able() || f.downed()))
            {
                continue;
            }
            f.hit_points = std::min(f.hit_points + healed, f.numbers.max_hit_points);
            if (f.condition == Condition::dying)
            {
                f.condition = Condition::unconscious;
                f.overkill = 0;
            }
            log({"{name} is healed.", {{"name", f.name}}});
        }
        break;
    }
    case 166:
        cast_generic(caster, number, level, targets, roll_dice(2, 8) + 1, 8);
        break;
    case 172:
        cast_generic(caster, number, level, targets, roll_dice(3, 8) + 3, 8);
        break;
    case 174:
        // Flame Strike: 6d8 of fire.
        cast_generic(caster, number, level, targets, roll_dice(6, 8), 9);
        break;
    case 187:
        // Ice Storm: 3d10 of cold and magic, so magic resistance can turn it
        // (Curse's, Secret's, Gateway's and Treasures' magic.pas,
        // SpellIceStorm), of magic alone in Pools of Darkness and the kit
        // (its spellfx.c, spell_ice_storm).
        {
            auto dice = shared_dice(number, level);
            if (underwater_)
            {
                dice.count = 1;
            }
            cast_generic(caster, number, level, underwater_ ? within(aim, 3) : targets,
                         roll_dice(dice.count, dice.sides) + dice.bonus, ice_storm_element());
            break;
        }
    case 192:
    {
        // Cone of Cold: the caster's level and as many d4 of cold and magic,
        // along a path two wide to half its level (magic.pas,
        // SpellConeOfCold; the kit's spellfx.c, spell_cone_of_cold).
        const auto dice = shared_dice(number, level);
        cast_generic(caster, number, level, path_area(caster, aim, cone_length(level), 2),
                     roll_dice(dice.count, dice.sides) + dice.bonus, 10);
        break;
    }
    case 198:
    {
        // A blast (Curse's Wand of Defoliation, a monster's): 6d6 along a path
        // to 3 squares; a plant takes it whole, with no save (ovr023, sub_61727).
        constexpr int plant = 18;
        const auto dice = shared_dice(number, level);
        const int damage = roll_dice(dice.count, dice.sides) + dice.bonus;
        std::vector<EntityId> others;
        for (const auto id : path_area(caster, aim, 3, 1))
        {
            auto &target = *find(id);
            if (target.numbers.creature_type == plant)
            {
                nonweapon_damage(target, damage, game_spell(number)->save_result, false, 8, level,
                                 number, false);
            }
            else
            {
                others.push_back(id);
            }
        }
        cast_generic(caster, number, level, others, damage, 8);
        break;
    }
    case 194:
    {
        // Hold Monster: Hold Person's saves, on any creature (Curse let it
        // hold only what Hold Person can, docs/ORIGINAL-RULES.md, Bugs).
        const int count = static_cast<int>(targets.size());
        const int modifier = count == 1 ? -3 : count == 2 ? -1 : 0;
        for (auto id = targets.rbegin(); id != targets.rend(); ++id)
        {
            cast_generic(caster, number, level, {*id}, 0, 0, modifier);
        }
        break;
    }
    case 176:
    {
        // Slay Living: unless magic resistance turns it, a failed save
        // kills; a saved one takes 2d8+1 (ovr023, SpellSlayLiving).
        for (const auto id : targets)
        {
            auto &target = *find(id);
            if (magic_resists(target, level, roll(100)))
            {
                log({"{name} is unaffected.", {{"name", target.name}}});
                continue;
            }
            if (!saving_throw(target, 4, 0, &caster))
            {
                log({"{name} is slain.", {{"name", target.name}}});
                kill(target);
                continue;
            }
            nonweapon_damage(target, roll_dice(2, 8) + 1, 0, false, 8, level, number, true);
        }
        break;
    }
    case 168:
        // Poison: the caster's touch carries poison (ovr023, SpellPoison).
        add_with_message(caster, {64, 0, level, false}, false, false, 0, level, &known);
        break;
    case 12:
    case 13:
    case 35:
    case 59:
        cast_strength_spell(caster, number, level, targets);
        break;
    case 236:
    case 256:
    case 290:
    case 310:
    case 311:
    case 313:
    case 314:
    case 315:
    case 316:
    case 317:
        cast_secret_spell(caster, number, level, targets, aim);
        break;
    case native_elixir_youth:
    case 263:
        make_younger(caster);
        break;
    case 301:
    case 303:
    case 304:
    case 308:
    case 309:
    case 320:
    case 321:
    case 323:
    case 324:
    case 325:
    case 326:
        cast_darkness_spell(caster, number, level, targets, aim);
        break;
    case 322:
        // Mind Blank lasts a day (magic.pas, CalcSpellDuration).
        cast_generic(caster, number, level, targets, 0, 0, 0, 1440);
        break;
    case 28:
        cast_generic(caster, number, level, targets, 0, 0);
        conjure_hammer(caster);
        break;
    default:
        cast_generic(caster, number, level, targets, 0, 0);
        break;
    }
    damage_dice_ = 0;
    settle_hammers();
    caster.using_item = -1;
    end_actions(caster);
}

// Haste ages its creature a year the first time it works on it, which a bit
// of its strength marks (effhndlr.pas, EffHasted): as the round's numbers are
// worked out (RecalcStats). A member's age is its marker's.
void Session::age_the_hasted()
{
    for (auto &f : fighters_)
    {
        auto *haste = f.find_effect(effect::hasted);
        if (!haste || (haste->strength & effect::hasted_aged) != 0)
        {
            continue;
        }
        haste->strength |= effect::hasted_aged;
        if (auto *age = f.find_effect(effect::age_in_years))
        {
            ++age->strength;
        }
        log({"{name} ages.", {{"name", f.name}}});
    }
}

namespace
{
// An item that holds spells to read: its type sits in slots 11 to 13
// (magic.pas, ItemReadable).
bool readable_item(const CarriedItem &c)
{
    return c.type.worn_location > 10 && c.type.worn_location < 14;
}

// What the inventory counts of an item: a scroll the bits of its spells left,
// a charged item its charges, a stack its size, anything else one.
int held_count(const CarriedItem &c)
{
    const auto &bytes = c.item.effect_codes;
    if (readable_item(c))
    {
        int bits = 0;
        for (std::size_t at = 0; at < bytes.size(); ++at)
        {
            bits |= bytes[at] != 0 ? 1 << at : 0;
        }
        return bits;
    }
    if (bytes[1] > 0 && bytes[2] < 0x80 && bytes[0] > 0)
    {
        return bytes[0];
    }
    return std::max<int>(c.item.stack_size, 1);
}

void set_readied(Held &held, bool readied)
{
    held.carried.item.readied_raw = readied ? 1 : 0;
    held.carried.item.raw[52] = held.carried.item.readied_raw;
}

// Readies an item last among the readied, or puts it away.
void place_readied(std::vector<Held> &held, std::size_t at, bool readied)
{
    int last = -1;
    for (const auto &other : held)
    {
        last = std::max(last, other.readied_order);
    }
    set_readied(held[at], readied);
    held[at].readied_order = readied ? last + 1 : -1;
}
} // namespace

// The items as the fight has left them: what each usable one has left, the
// shots its missile has left, and those used up (a spent charge's last, a
// thrown weapon gone from its hand).
std::vector<Held> Session::held_now(const Fighter &f) const
{
    auto held = f.held;
    if (!f.record)
    {
        return held;
    }
    std::vector<CarriedItem> worked;
    for (const int at : f.worked_from)
    {
        worked.push_back(held[static_cast<std::size_t>(at)].carried);
    }
    const auto &n = f.numbers;
    for (const auto &usable : usable_items(worked, game_of(identity_)))
    {
        auto &item =
            held[static_cast<std::size_t>(f.worked_from[static_cast<std::size_t>(usable.ordinal)])];
        const auto now = std::ranges::find(n.items, usable.ordinal, &UsableItem::ordinal);
        if (now == n.items.end())
        {
            item.gone = true;
            continue;
        }
        auto &raw = item.carried.item;
        for (std::size_t at = 0; now->scroll && at < now->spells.size(); ++at)
        {
            if (now->spells[at] == 0)
            {
                raw.effect_codes[at] = 0;
                raw.raw[60 + at] = 0;
            }
        }
        if (!now->scroll && usable.charges > 0)
        {
            raw.effect_codes[0] = static_cast<std::uint8_t>(now->charges);
            raw.raw[60] = raw.effect_codes[0];
        }
        if (now->quantity > 0)
        {
            raw.stack_size = static_cast<std::uint8_t>(now->quantity);
            raw.raw[57] = raw.stack_size;
        }
    }
    // The missile's shots: a thrown weapon, or the arrows or quarrels its
    // launcher fires; a readied weapon gone from its hand was thrown.
    namespace bit = por::item_property;
    const auto ammunition = ammunition_types(game_of(identity_));
    for (const int at : f.worked_from)
    {
        auto &weapon = held[static_cast<std::size_t>(at)];
        const auto &c = weapon.carried;
        if (!c.item.readied() || c.type.worn_location != 0)
        {
            continue;
        }
        if (n.weapon_type != c.item.type)
        {
            weapon.gone = true;
            continue;
        }
        const auto &missile = n.missile;
        if (!missile || missile->needs_no_ammunition)
        {
            continue;
        }
        for (const int shot_at : f.worked_from)
        {
            auto &shot = held[static_cast<std::size_t>(shot_at)];
            const auto type = shot.carried.item.type;
            const bool arrows = c.type.has(bit::fires_arrows) && type == ammunition.arrows;
            const bool quarrels =
                c.type.has(bit::fires_quarrels) && ammunition.shot_as_quarrel(type);
            const bool shoots = missile->thrown
                                    ? shot_at == at
                                    : shot.carried.item.readied() && (arrows || quarrels);
            if (!shoots)
            {
                continue;
            }
            if (!missile->loaded)
            {
                shot.gone = true;
            }
            else if (missile->quantity > 0)
            {
                shot.carried.item.stack_size = static_cast<std::uint8_t>(missile->quantity);
                shot.carried.item.raw[57] = shot.carried.item.stack_size;
            }
        }
    }
    return held;
}

// A creature's numbers with these items, as RecalcStats works them out from
// its record, at the Strength it has now: a monster's with all it carries, a
// character's with those it has readied, in the order it readied them.
// `order` gets their places among the held.
Combatant Session::numbers_with(const Fighter &f, const std::vector<Held> &held,
                                std::vector<int> &order) const
{
    order.clear();
    for (std::size_t at = 0; at < held.size(); ++at)
    {
        const bool counted = f.weighs_unreadied || held[at].carried.item.readied();
        if (!held[at].gone && counted)
        {
            order.push_back(static_cast<int>(at));
        }
    }
    if (!f.weighs_unreadied)
    {
        std::ranges::stable_sort(order,
                                 [&held](int a, int b)
                                 {
                                     return held[static_cast<std::size_t>(a)].readied_order <
                                            held[static_cast<std::size_t>(b)].readied_order;
                                 });
    }
    std::vector<CarriedItem> items;
    for (const int at : order)
    {
        items.push_back(held[static_cast<std::size_t>(at)].carried);
    }
    auto record = *f.record;
    record.current_abilities.strength = static_cast<std::uint8_t>(f.numbers.strength);
    record.current_abilities.exceptional_strength =
        static_cast<std::uint8_t>(f.numbers.exceptional_strength);
    return combatant_from_record(record, items);
}

// The creature's numbers as its items change (charhlp.pas, RecalcStats;
// cbtacts.pas, RecalcAttacksLeft): what its items give changes by the
// difference, and what the fight has done to it (drained levels, effects,
// Strength) stays. One that has not attacked yet this round gets the new
// weapon's attacks; one that has keeps no more than that.
void Session::rework_gear(Fighter &f, const std::vector<Held> &before_held,
                          std::vector<Held> after_held) const
{
    std::vector<int> before_order, after_order;
    const auto before = numbers_with(f, before_held, before_order);
    const auto after = numbers_with(f, after_held, after_order);
    const auto attacks_with = [&](const Combatant &c)
    {
        const auto &missile = c.missile;
        if (missile && missile->loaded)
        {
            const int attacks = attacks_this_round(paced(f, missile->rate_per_two_rounds),
                                                   static_cast<int>(round_));
            return missile->needs_no_ammunition ? attacks
                                                : std::min(attacks, std::max(missile->quantity, 1));
        }
        return attacks_this_round(paced(f, c.attacks[0].rate_per_two_rounds),
                                  static_cast<int>(round_));
    };
    auto &n = f.numbers;
    n.thac0 += after.thac0 - before.thac0;
    n.armor_class += after.armor_class - before.armor_class;
    n.rear_armor_class += after.rear_armor_class - before.rear_armor_class;
    n.save_bonus += after.save_bonus - before.save_bonus;
    n.movement += after.movement - before.movement;
    n.attacks[0] = after.attacks[0];
    n.large_attack = after.large_attack;
    n.missile = after.missile;
    n.striker = after.striker;
    n.weapon_type = after.weapon_type;
    n.backstabs = after.backstabs;
    n.strength_counts = after.strength_counts;
    // What an effect gives (Secret's reflection) stays.
    n.wards_gaze = after.wards_gaze || (n.wards_gaze && !before.wards_gaze);
    n.mirror = after.mirror || (n.mirror && !before.mirror);
    n.items = after.items;
    // An item put away takes the effect it gave, one readied lays its own
    // (ToggleItemEffect); the numbers worked from the items hold only theirs.
    for (const auto &given : before.effects)
    {
        if (std::ranges::find(after.effects, given.code, &Effect::code) == after.effects.end())
        {
            f.remove(given.code);
        }
    }
    for (const auto &given : after.effects)
    {
        if (!f.has(given.code))
        {
            n.effects.push_back(given);
        }
    }
    const int had = attacks_with(before), now = attacks_with(after);
    f.attacks_left[0] = f.attacks_left[0] >= had ? now : std::min(f.attacks_left[0], now);
    f.held = std::move(after_held);
    f.worked_from = std::move(after_order);
}

// Readying an item, or putting one away, as the View menu does in a fight
// (viewchar.pas, ToggleReadyItem): it takes no action.
void Session::toggle_readied(Fighter &f, std::size_t index)
{
    const auto before = held_now(f);
    auto after = before;
    const bool putting_away = after[index].carried.item.readied();
    place_readied(after, index, !putting_away);
    rework_gear(f, before, std::move(after));
    f.readied_changed = true;
    // Pool of Radiance's hammer put away ends its spell (its special byte's
    // handler 137, EffHammerGone), as Secret's and the later games' do by
    // taking the effect the hammer gives; Curse's and Gateway's stay.
    const auto game = game_of(identity_);
    const bool curse_engine = game == GoldBoxGame::curse_of_the_azure_bonds ||
                              game == GoldBoxGame::gateway_to_the_savage_frontier ||
                              game == GoldBoxGame::unlimited_adventures;
    if (putting_away && f.held[index].conjured && !curse_engine)
    {
        f.remove(effect::spiritual_hammer);
    }
    settle_hammers();
}

// The caster of Spiritual Hammer gains its hammer while the spell lasts,
// unless it has one or carries 16 items (effhndlr.pas, EffSpiritualHammer).
void Session::conjure_hammer(Fighter &caster)
{
    const auto making = hammer_making(game_of(identity_));
    if (!caster.record || !caster.has(effect::spiritual_hammer) ||
        making.type >= item_types_.size())
    {
        return;
    }
    const auto before = held_now(caster);
    const auto living = std::ranges::count_if(before,
                                              [](const Held &h)
                                              {
                                                  return !h.gone;
                                              });
    const bool has_one = std::ranges::any_of(before,
                                             [](const Held &h)
                                             {
                                                 return h.conjured && !h.gone;
                                             });
    if (has_one || living >= 16)
    {
        return;
    }
    Held hammer;
    auto &item = hammer.carried.item;
    item.type = making.type;
    item.name_components = {0, making.type, making.name_word};
    item.magic_bonus = 1;
    item.effect_codes = {0, effect::spiritual_hammer, making.special};
    item.raw[46] = item.type;
    item.raw[47 + 1] = item.name_components[1];
    item.raw[47 + 2] = item.name_components[2];
    item.raw[50] = 1;
    item.raw[61] = item.effect_codes[1];
    item.raw[62] = item.effect_codes[2];
    hammer.carried.type = item_types_[making.type];
    hammer.name = "Spiritual Hammer";
    hammer.conjured = true;
    hammer.start_count = 1;
    auto after = before;
    after.push_back(std::move(hammer));
    const std::size_t at = after.size() - 1;
    std::vector<CarriedItem> readied;
    bool armed = false;
    for (const auto &h : before)
    {
        if (!h.gone && h.carried.item.readied())
        {
            readied.push_back(h.carried);
            armed = armed || h.carried.type.worn_location == 0;
        }
    }
    const bool ready = making.readied_always || (making.readied_unarmed && !armed);
    if (ready && !readying_refusal(after[at].carried, readied, caster.record->item_limit,
                                   game_of(identity_)))
    {
        place_readied(after, at, true);
        caster.readied_changed = true;
    }
    rework_gear(caster, before, std::move(after));
    log({"{name} gains an item.", {{"name", caster.name}}});
}

// A hammer whose spell has ended is gone (EffSpiritualHammer, as its effect
// is removed).
void Session::settle_hammers()
{
    for (auto &f : fighters_)
    {
        const auto conjured = std::ranges::find_if(f.held,
                                                   [](const Held &h)
                                                   {
                                                       return h.conjured && !h.gone;
                                                   });
        if (conjured == f.held.end() || f.has(effect::spiritual_hammer))
        {
            continue;
        }
        const auto at = static_cast<std::size_t>(conjured - f.held.begin());
        const auto before = held_now(f);
        auto after = before;
        after[at].gone = true;
        if (after[at].carried.item.readied())
        {
            place_readied(after, at, false);
            f.readied_changed = true;
        }
        rework_gear(f, before, std::move(after));
    }
}

// What the original's AI makes of a weapon (combatai.pas, CalcWeaponPriority):
// its dice, 8 a plus, 2 a point of its damage bonus, 2 a shot beyond one for a
// launcher and 3 for one hand; holy water against undead is worth 8; none
// for one needing more hands than are free, an aligned weapon of another
// alignment, the Javelin of Lightning or a cursed weapon.
int Session::weapon_priority(const Fighter &f, const Held &weapon, int other_hands) const
{
    namespace bit = por::item_property;
    constexpr int holy_water = 85, aligned_weapon = 132, javelin_of_lightning = 83;
    const auto &type = weapon.carried.type;
    const auto &item = weapon.carried.item;
    const auto &damage = type.small_medium_damage;
    int priority = damage.count * damage.sides;
    priority += item.magic_bonus > 0 ? item.magic_bonus * 8 : 0;
    priority += damage.modifier > 0 ? damage.modifier * 2 : 0;
    const auto target = std::ranges::find(fighters_, f.lock, &Fighter::id);
    const bool pool_numbers = !secret() && !vga_engine(game_of(identity_));
    if (pool_numbers && item.type == holy_water && f.lock && target != fighters_.end() &&
        target->numbers.undead_level > 0)
    {
        priority = 8;
    }
    if (type.has(bit::fires_ammunition))
    {
        priority += (type.rate_of_fire - 1) * 2;
    }
    priority += type.hands <= 1 ? 3 : 0;
    const bool other_alignment = item.effect_codes[2] == aligned_weapon &&
                                 (item.effect_codes[1] & 0xF) != f.numbers.alignment;
    if (type.hands + other_hands > 3 || other_alignment ||
        item.effect_codes[1] == javelin_of_lightning || item.cursed_raw != 0)
    {
        priority = 0;
    }
    return priority;
}

// The original's AI readies its best weapon and shield (combatai.pas,
// EquipWeaponAndArmor): its best missile weapon when it scores above half the
// best melee weapon's (its own blows set the bar), has something to shoot and
// is thrown with Strength or has no enemy next to it; else its best melee
// weapon; and the shield of the best plus its hands leave room for. A cursed
// item is never put away.
void Session::equip_best(Fighter &f)
{
    if (!f.record)
    {
        return;
    }
    namespace bit = por::item_property;
    constexpr std::uint8_t weapon_place = 0, shield_place = 1;
    const auto before = held_now(f);
    auto after = before;
    int other_hands = 0;
    std::optional<std::size_t> weapon, shield;
    for (std::size_t at = 0; at < after.size(); ++at)
    {
        const auto &c = after[at].carried;
        if (after[at].gone || !c.item.readied())
        {
            continue;
        }
        if (c.type.worn_location == weapon_place)
        {
            weapon = at;
        }
        else if (c.type.worn_location == shield_place)
        {
            shield = at;
        }
        else
        {
            other_hands += c.type.hands;
        }
    }
    const auto &unarmed = f.record->base_attacks[0].damage;
    int best_melee_score =
        unarmed.count * unarmed.sides + (unarmed.modifier > 0 ? unarmed.modifier * 2 : 0);
    int best_ranged_score = 1, best_shield_score = 0;
    std::optional<std::size_t> best_melee, best_ranged, best_shield;
    for (std::size_t at = 0; at < after.size(); ++at)
    {
        const auto &held = after[at];
        const auto &type = held.carried.type;
        const bool banked =
            std::ranges::any_of(f.numbers.effects,
                                [at](const Effect &power)
                                {
                                    return power.code == effect::baaz_banked_weapon &&
                                           power.strength == static_cast<int>(at + 1);
                                });
        const bool wrong_order =
            champions() &&
            champions_ready_refusal(held.carried.item, f.record->race, f.record->robe_order)
                .has_value();
        if (held.gone || banked || wrong_order ||
            (type.class_restrictions & f.record->item_limit) == 0)
        {
            continue;
        }
        if (type.worn_location == weapon_place)
        {
            const int score = weapon_priority(f, held, other_hands);
            const bool launcher = type.has(bit::fires_ammunition);
            if ((launcher || type.has(bit::thrown)) && score > best_ranged_score)
            {
                best_ranged = at;
                best_ranged_score = score;
            }
            if (!launcher && score > best_melee_score)
            {
                best_melee = at;
                best_melee_score = score;
            }
        }
        else if (type.worn_location == shield_place)
        {
            const int plus = held.carried.item.magic_bonus;
            const int score = plus >= 0 ? plus + 1 : 0;
            if (score > best_shield_score)
            {
                best_shield = at;
                best_shield_score = score;
            }
        }
    }
    bool supplied = false, thrown_with_strength = false;
    if (best_ranged)
    {
        const auto &type = after[*best_ranged].carried.type;
        constexpr std::uint8_t sling = bit::fires_ammunition | bit::dexterity_missile;
        const auto ammunition = ammunition_types(game_of(identity_));
        const auto readied_of = [&after](auto wanted)
        {
            return std::ranges::any_of(after,
                                       [wanted](const Held &h)
                                       {
                                           return !h.gone && h.carried.item.readied() &&
                                                  wanted(h.carried.item.type);
                                       });
        };
        const auto arrows = [&ammunition](std::uint8_t t)
        {
            return t == ammunition.arrows;
        };
        const auto quarrels = [&ammunition](std::uint8_t t)
        {
            return ammunition.shot_as_quarrel(t);
        };
        supplied = type.has(bit::thrown) || type.properties == sling ||
                   (type.has(bit::fires_arrows) && readied_of(arrows)) ||
                   (type.has(bit::fires_quarrels) && readied_of(quarrels));
        thrown_with_strength =
            type.range > 1 && type.has(bit::thrown) && type.has(bit::strength_bonus);
    }
    const bool shoots = best_ranged && best_ranged_score > best_melee_score / 2 && supplied &&
                        (thrown_with_strength || !enemy_next_to(f));
    const auto chosen = shoots ? best_ranged : best_melee;
    const auto cursed = [&after](std::optional<std::size_t> at)
    {
        return at && after[*at].carried.item.cursed_raw != 0;
    };
    const auto ready = [&after](std::optional<std::size_t> at, bool readied)
    {
        if (at)
        {
            place_readied(after, *at, readied);
        }
    };
    if (weapon != chosen && !cursed(weapon))
    {
        ready(weapon, false);
        ready(chosen, true);
        weapon = chosen;
    }
    const int weapon_hands = weapon ? after[*weapon].carried.type.hands : 0;
    const int shield_hands = shield ? after[*shield].carried.type.hands : 0;
    if (other_hands + weapon_hands + shield_hands > 2)
    {
        // Too many hands: the shield goes, or the weapon when the shield is
        // cursed or there is none.
        ready(shield && !cursed(shield) ? shield : weapon, false);
    }
    else if (other_hands + weapon_hands < 2 && shield != best_shield && !cursed(shield))
    {
        ready(shield, false);
        ready(best_shield, true);
    }
    bool changed = false;
    for (std::size_t at = 0; at < after.size(); ++at)
    {
        changed = changed || after[at].carried.item.readied() != before[at].carried.item.readied();
    }
    if (changed)
    {
        rework_gear(f, before, std::move(after));
        f.readied_changed = true;
    }
}

// Youth (Secret's item power 63, SpellYouth): its user becomes 1d4+1 years
// younger, but not below 18.
void Session::make_younger(Fighter &user)
{
    constexpr int youngest = 18;
    const int years = roll(4) + 1;
    auto *age = user.find_effect(effect::age_in_years);
    if (age && age->strength - years < youngest)
    {
        age->strength = youngest;
        log({"{name} is now 18 years old.", {{"name", user.name}}});
        return;
    }
    if (age)
    {
        age->strength -= years;
    }
    log({"{name} becomes younger.", {{"name", user.name}}});
}

// What using an item spends (viewchar.pas, UseItem): a scroll the spell read,
// gone with its last; a stack one of it; another item a charge, gone at 0.
// An item of no charges never runs out.
void Session::spend_item(Fighter &user, int ordinal, int number)
{
    auto &items = user.numbers.items;
    const auto item = std::find_if(items.begin(), items.end(),
                                   [ordinal](const UsableItem &i)
                                   {
                                       return i.ordinal == ordinal;
                                   });
    if (item == items.end())
    {
        return;
    }
    if (item->scroll)
    {
        if (const auto read = std::find(item->spells.begin(), item->spells.end(), number);
            read != item->spells.end())
        {
            *read = 0;
        }
        if (item->spells == std::array<int, 3>{})
        {
            items.erase(item);
        }
        return;
    }
    if (item->charges <= 0)
    {
        return;
    }
    if (item->quantity > 1)
    {
        --item->quantity;
        return;
    }
    if (--item->charges == 0)
    {
        items.erase(item);
    }
}

// The creatures on a path from a caster through its aim, as Curse's area
// spells take them (ovr023, BuildAreaDamageTargets in its reconstruction): the
// aim is carried on the caster's way to max_range squares, kept where the
// caster can see, and every creature but the caster on the line back to it,
// and on one or two lines beside it when the path is that wide, is taken.
std::vector<EntityId> Session::path_area(const Fighter &caster, Cell aim, int max_range,
                                         int width) const
{
    std::vector<EntityId> found;
    for (const auto square : path_squares(caster, aim, max_range, width))
    {
        if (const auto id = standing_at(square);
            id && id != caster.id && std::ranges::find(found, id) == found.end())
        {
            found.push_back(id);
        }
    }
    return found;
}

// The squares of path_area's path, in the order it takes them: each line
// from its end back to the caster.
std::vector<Cell> Session::path_squares(const Fighter &caster, Cell aim, int max_range,
                                        int width) const
{
    static constexpr std::array<Cell, 8> beside{
        {{1, 0}, {1, 0}, {0, 1}, {0, 1}, {-1, 0}, {-1, 0}, {0, -1}, {0, -1}}};
    static constexpr std::array<Cell, 8> other_side{
        {{-1, 0}, {0, -1}, {0, -1}, {1, 0}, {1, 0}, {0, 1}, {0, 1}, {-1, 0}}};
    Cell end = aim;
    if (aim != caster.cell)
    {
        LineWalk out(caster.cell, {caster.cell.x + (aim.x - caster.cell.x) * max_range * 2,
                                   caster.cell.y + (aim.y - caster.cell.y) * max_range * 2});
        while (out.next() && board_.contains(out.at) &&
               board_.at(out.at) != rules::Terrain::obstacle)
        {
            if (out.distance <= LineWalk(caster.cell, aim).distance)
            {
                continue;
            }
            if (out.distance > max_range * 2)
            {
                break;
            }
            end = out.at;
        }
    }
    std::vector<Cell> squares;
    const auto gather = [&](Cell from)
    {
        if (!board_.contains(from))
        {
            return;
        }
        LineWalk back(from, caster.cell);
        do
        {
            squares.push_back(back.at);
        } while (back.next());
    };
    gather(end);
    const auto toward_caster = static_cast<std::size_t>(direction_toward(end, caster.cell));
    if (width > 1)
    {
        gather({end.x + beside[toward_caster].x, end.y + beside[toward_caster].y});
    }
    if (width > 2)
    {
        gather({end.x + other_side[toward_caster].x, end.y + other_side[toward_caster].y});
    }
    return squares;
}

// Whom Haste (48) or Slow takes among its targets: up to the caster's level of
// the caster's side or of its enemies, the nearest first (resolve_spell).
std::vector<EntityId> Session::haste_slow_takes(const Fighter &caster, int number, int level,
                                                const std::vector<EntityId> &targets) const
{
    const bool haste = number == 48;
    std::vector<EntityId> taken;
    for (const auto id : targets)
    {
        if (static_cast<int>(taken.size()) < level && (find(id)->side == caster.side) == haste)
        {
            taken.push_back(id);
        }
    }
    return taken;
}

// The original's resolver (magic.pas, ResolveSpell): each target saves if the
// spell allows, a touch must hit first, the one damage roll is shared, and the
// spell's effect is added through the immunities.
void Session::cast_generic(Fighter &caster, int number, int level,
                           const std::vector<EntityId> &targets, int damage, int element,
                           int save_modifier, int minutes)
{
    if (damage <= 0)
    {
        element = 0;
    }
    const auto &known = *game_spell(number);
    for (const auto id : targets)
    {
        auto &target = *find(id);
        bool saved = false;
        if (known.save_result != 0)
        {
            saved = saving_throw(target, known.save_type, save_modifier, &caster, element);
        }
        if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
            target.has(dq_effect::free_action) && (number == 23 || number == 49 || number == 194))
        {
            saved = true;
        }
        if (known.range_fixed == 255)
        {
            const int natural = roll(20);
            if (!attack_hits(natural, caster.numbers.thac0, target.numbers.armor_class,
                             attack_modifier(caster, target, game_of(identity_)) +
                                 love_modifier(caster, target)))
            {
                log({"{attacker} misses {name}.",
                     {{"attacker", caster.name}, {"name", target.name}}});
                saved = true;
                damage = 0;
            }
        }
        if (damage > 0)
        {
            nonweapon_damage(target, damage, known.save_result, saved, element, level, number,
                             targets.size() == 1 && (known.area & 0x0F) < 8);
        }
        if (known.effect > 0 && target.condition != Condition::dead)
        {
            const int lasting =
                minutes >= 0 ? minutes : known.minutes_fixed + known.minutes_per_level * level;
            const int strength = known.effect == effect::charm
                                     ? (caster.side == Side::opposition ? 128 : 0) + level
                                     : level;
            if (add_with_message(target,
                                 {known.effect, lasting, strength, known.effect == effect::charm},
                                 known.save_result == 1, saved, element, level, &known) &&
                known.effect == effect::charm)
            {
                charm(target, caster.side);
            }
        }
    }
}

// The chance magic resistance resists a caster of this level: 15% at level
// 11 and 5 more a level above (50% for the stronger kind, 105); the original's
// byte wraps a chance below 0 to one that always resists (effhndlr.pas, 105, 106).
bool magic_resists(const Fighter &target, int caster_level, int roll_100)
{
    for (const auto [code, base] : {std::pair{105, 50}, std::pair{106, 15}})
    {
        if (!target.has(code))
        {
            continue;
        }
        const int chance = base - (11 - caster_level) * 5;
        if (chance < 0 || roll_100 <= chance)
        {
            return true;
        }
    }
    // Curse's (305, 306) lessen with a higher caster's level, as AD&D's do
    // (ResistMagicPercent in its reconstruction).
    for (const auto [code, base] : {std::pair{305, 50}, std::pair{306, 15}})
    {
        if (target.has(code) && roll_100 <= base + (11 - caster_level) * 5)
        {
            return true;
        }
    }
    // Secret's lessens as Curse's do, from the percent its record holds
    // (fxengine.pas, CheckSpellResistance, in its reconstruction).
    const int percent = target.numbers.magic_resistance;
    return percent > 0 && roll_100 <= percent + (11 - caster_level) * 5;
}

// Secret's spells a creature's race or flags refuse (magic.pas, in its
// reconstruction): Charm Person takes one of the player's races, or a monster
// its flags call a person; Charm Person or Mammal one they call a mammal; Slay
// Living and Death Spell cannot slay the unliving. Secret's own spells are
// numbered 200 higher in the rules (canonical_spell).
bool Session::refuses_spell(const Fighter &target, int number) const
{
    constexpr int charm_person = 10, slay_living = 176, charm_person_or_mammal = 296,
                  death_spell = 310;
    if (champions() && (number == charm_person || number == 506))
    {
        // The native person flag admits humanoid monsters; draconians and
        // undead carry different type flags, and cannot be charmed as people.
        return target.numbers.large ||
               (target.numbers.race == 0 && (target.numbers.traits & trait::person) == 0);
    }
    if (treasures())
    {
        return treasures_refuses_spell(target, number);
    }
    if (!secret_engine())
    {
        return false;
    }
    const bool monster = target.numbers.race == 0;
    const int traits = target.numbers.traits;
    switch (number)
    {
    case charm_person:
        // Pools of Darkness's takes a monster its flags call a mammal, of
        // one square (its SpellCharmPerson).
        if (darkness())
        {
            return (monster && (traits & trait::mammal) == 0) || target.numbers.large;
        }
        return monster && (traits & trait::person) == 0;
    case charm_person_or_mammal:
        return monster && (traits & trait::mammal) == 0;
    case slay_living:
    case death_spell:
        return (traits & trait::unliving) != 0;
    default:
        return false;
    }
}

// Treasures' spells that take only a creature its flags call a person (bit 2
// of its "immunities", which every character has): Hold Person, Charm
// Monsters, Hold Monster and Mass Charm (its magic.pas, ApplyEffectToTargets
// and SpellMassCharm). Hold Person never takes a monster, flagged or not.
// Charm Person judges its targets itself (resolve_spell).
bool Session::treasures_refuses_spell(const Fighter &target, int number) const
{
    constexpr int cleric_hold_person = 23, mage_hold_person = 49, charm_monsters = 181,
                  hold_monster = 194, mass_charm = 320;
    const bool monster = target.numbers.race == 0;
    const bool person = (target.numbers.traits & trait::person) != 0;
    switch (number)
    {
    case cleric_hold_person:
    case mage_hold_person:
        return monster;
    case charm_monsters:
    case hold_monster:
    case mass_charm:
        return monster && !person;
    default:
        return false;
    }
}

// Whether a creature resists magic, by Pool of Radiance's or Curse's numbers
// or by its record in Secret.
bool resists_magic_at_all(const Fighter &target)
{
    return target.has(105) || target.has(106) || target.has(305) || target.has(306) ||
           target.numbers.magic_resistance > 0;
}

// Secret of the Silver Blades' own spells (magic.pas, in its reconstruction),
// by the rules' numbers (canonical_spell).
void Session::cast_secret_spell(Fighter &caster, int number, int level,
                                const std::vector<EntityId> &targets, Cell aim)
{
    constexpr int heal = 236, harm = 256, barkskin = 290, death_spell = 310, disintegrate = 311,
                  stone_to_flesh = 313, flesh_to_stone = 314, delayed_blast_fireball = 315,
                  mass_invisibility = 316, power_word_stun = 317;
    constexpr int diseased = 34;
    const auto &known = *game_spell(number);
    const auto roll_dice = [this](int count, int sides)
    {
        damage_dice_ = count;
        int total = 0;
        for (int n = 0; n < count; ++n)
        {
            total += roll(sides);
        }
        return total;
    };
    switch (number)
    {
    case heal:
    case barkskin:
        // Secret's table casts Barkskin as Heal (spellFns[90]).
        for (const auto id : targets)
        {
            heal_all_but_1d4(*find(id));
        }
        break;
    case harm:
        // Harm diseases its target and leaves it 1d4 Hit Points.
        for (const auto id : targets)
        {
            auto &target = *find(id);
            (void)add_with_message(target, {diseased, 0, level, false}, false, false, 8, level,
                                   &known);
            nonweapon_damage(target, target.hit_points - roll(4), 0, false, 8, level, number,
                             false);
        }
        break;
    case death_spell:
        cast_death_spell(targets);
        break;
    case disintegrate:
    case flesh_to_stone:
        // A failed save against spells slays outright, or turns to stone.
        for (const auto id : targets)
        {
            auto &target = *find(id);
            if (turns_aside_slaying(target) || saving_throw(target, 4, 0, &caster))
            {
                continue;
            }
            if (number == disintegrate)
            {
                kill(target, "{name} disintegrates.");
            }
            else
            {
                turn_to_stone(target);
            }
        }
        break;
    case stone_to_flesh:
        // A statue lives again with 1 Hit Point, where it stood.
        for (const auto id : targets)
        {
            auto &target = *find(id);
            // A Treasures stone guardian dies by it, and a stone golem
            // becomes flesh for two of its turns (SpellStoneToFlesh).
            if (target.has(effect::stone_to_flesh_kills) && target.able())
            {
                kill(target);
                continue;
            }
            if (auto *golem = target.find_effect(effect::stone_golem); golem && target.able())
            {
                const bool stone = golem->strength == effect::golem_stone;
                golem->strength = stone ? 1 : golem->strength;
                log({stone ? "{name} becomes flesh." : "{name} is still flesh.",
                     {{"name", target.name}}});
                continue;
            }
            if (target.condition != Condition::stoned || !cell_free(target))
            {
                log({"{name} is unaffected.", {{"name", target.name}}});
                continue;
            }
            stand_up(target, 1, "{name} is no longer a statue.");
        }
        break;
    case delayed_blast_fireball:
        cast_delayed_blast(caster, level, targets, aim);
        break;
    case mass_invisibility:
    {
        // The caster's side, but not one with an enemy next to it.
        std::vector<EntityId> hidden;
        for (const auto id : targets)
        {
            const auto &target = *find(id);
            if (target.side == caster.side && !enemy_next_to(target))
            {
                hidden.push_back(id);
            }
        }
        cast_generic(caster, number, level, hidden, 0, 0);
        break;
    }
    case power_word_stun:
        // Stunned for 4d4 rounds at up to 30 Hit Points, 2d4 to 60 and 1d4
        // to 90; above that it is unaffected.
        for (const auto id : targets)
        {
            auto &target = *find(id);
            const int hit_points = target.hit_points;
            const int rounds = hit_points <= 30   ? roll_dice(4, 4)
                               : hit_points <= 60 ? roll_dice(2, 4)
                               : hit_points <= 90 ? roll(4)
                                                  : 0;
            if (rounds == 0)
            {
                log({"{name} is unaffected.", {{"name", target.name}}});
                continue;
            }
            (void)add_with_message(target, {effect::stunned, rounds, level, false}, false, false, 0,
                                   level, &known);
        }
        break;
    default:
        break;
    }
}

// The Strength spells (magic.pas, SpellEnlarge, SpellReduce, SpellStrength,
// SpellGiantStrength): each effect keeps the Strength it gives in its
// strength, by the original's encoding (strength_code).
void Session::cast_strength_spell(Fighter &caster, int number, int level,
                                  const std::vector<EntityId> &targets)
{
    constexpr int enlarge = 12, reduce = 13, giant_strength = 59;
    const auto &known = *game_spell(number);
    const int minutes = known.minutes_fixed + known.minutes_per_level * level;
    const auto game = game_of(identity_);
    for (const auto id : targets)
    {
        auto &target = *find(id);
        const auto &n = target.numbers;
        if (number == reduce)
        {
            if (!target.has(effect::enlarged) || saving_throw(target, 4, 0, &caster))
            {
                log({"{name} is unaffected.", {{"name", target.name}}});
                continue;
            }
            target.remove(effect::enlarged);
            log({"{name} has been reduced.", {{"name", target.name}}});
            recount_strength(target);
            continue;
        }
        const auto [base, exceptional] =
            number == enlarge ? enlarged_strength(level, game)
            : number == giant_strength
                ? std::pair{21, 0}
                : strengthened(n.strength, n.exceptional_strength, n.class_levels, game,
                               roll(strength_spell_die(n.class_levels, game)));
        const bool native_frost =
            number == giant_strength && game == GoldBoxGame::dark_queen_of_krynn;
        const int code = number == enlarge ? effect::enlarged
                         : native_frost    ? 113
                                           : effect::strengthened;
        const bool stronger = stronger_than(base, exceptional, n.strength, n.exceptional_strength);
        // A Potion of Giant Strength lasts the fight (its table's no time).
        const int lasting = native_frost               ? 40 + roll(4) * 10
                            : number == giant_strength ? 0
                                                       : minutes;
        if (add_with_message(target, {code, lasting, strength_code(base, exceptional), false},
                             false, false, 0, level, &known))
        {
            recount_strength(target);
            if (stronger)
            {
                log({"{name} is stronger.", {{"name", target.name}}});
            }
        }
    }
}

// A creature's Strength again (fxengine.pas, RecalcAbility): its own, less
// what a disease has wasted, or the best its Enlarge and Strength effects
// give; a change moves its blows' to-hit and damage when they take Strength.
void Session::recount_strength(Fighter &f)
{
    auto &n = f.numbers;
    int base = n.natural_strength, exceptional = n.natural_exceptional_strength;
    if (const auto *wasted = f.find_effect(effect::strength_wasted); wasted && wasted->strength > 0)
    {
        base = std::max(3, base - wasted->strength);
        exceptional = 0;
    }
    for (const auto &e : n.effects)
    {
        if (e.code != effect::enlarged && e.code != effect::strengthened &&
            !(game_of(identity_) == GoldBoxGame::dark_queen_of_krynn && e.code == 113))
        {
            continue;
        }
        const auto [given, given_exceptional] = strength_of_code(e.strength);
        if (stronger_than(given, given_exceptional, base, exceptional))
        {
            base = given;
            exceptional = given_exceptional;
        }
    }
    if (n.strength_counts)
    {
        n.thac0 += strength_to_hit_bonus(n.strength, n.exceptional_strength) -
                   strength_to_hit_bonus(base, exceptional);
        const int more_damage = strength_damage_bonus(base, exceptional) -
                                strength_damage_bonus(n.strength, n.exceptional_strength);
        n.attacks[0].modifier += more_damage;
        if (n.large_attack)
        {
            n.large_attack->modifier += more_damage;
        }
    }
    n.strength = base;
    n.exceptional_strength = exceptional;
}

// Out of doors a fireball catches only those within 2 squares of its aim,
// not 3 (magic.pas, SpellFireball, outside 3D maps).
std::vector<EntityId> Session::fireball_reach(const std::vector<EntityId> &targets, Cell aim) const
{
    return outdoors_ ? within(aim, 2) : targets;
}

// Secret's Delayed Blast Fireball (SpellDelayedBlastFireball): with no delay
// it bursts at once, the caster's level and as many d6 of fire on all within
// 3 of the aim; with one it waits on its square for those rounds (effect 100,
// EffDelayedBlastFireball). The AI never delays it: the original asked anyone
// casting it, the computer's creatures too, for the delay.
void Session::cast_delayed_blast(Fighter &caster, int level, const std::vector<EntityId> &targets,
                                 Cell aim)
{
    constexpr int fire = 9, delayed_blast_fireball = 315;
    const int delay = std::max(caster.blast_delay, 0);
    caster.blast_delay = -1;
    if (delay == 0)
    {
        if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
        {
            damage_dice_ = level;
        }
        const auto dice = shared_dice(delayed_blast_fireball, level);
        int total = dice.bonus;
        for (int die = 0; die < dice.count; ++die)
        {
            total += roll(dice.sides);
        }
        cast_generic(caster, delayed_blast_fireball, level, targets, total, fire);
        return;
    }
    const int index =
        static_cast<int>(std::ranges::count(blasts_, caster.id, &DelayedBlast::caster));
    blasts_.push_back({caster.id, index, aim, level});
    caster.numbers.effects.push_back({effect::delayed_blast, delay, index, true});
    log({"{name} sets a delayed blast fireball.", {{"name", caster.name}}});
}

// The blast goes off on its square: everyone within 3 takes the caster's
// level and as many d6 of fire, half on a save against spells.
void Session::delayed_blast_goes_off(Fighter &caster, int index)
{
    constexpr int fire = 9, delayed_blast_fireball = 315;
    const auto blast = std::ranges::find_if(blasts_,
                                            [&](const DelayedBlast &b)
                                            {
                                                return b.caster == caster.id && b.index == index;
                                            });
    if (blast == blasts_.end())
    {
        return;
    }
    const auto set = *blast;
    blasts_.erase(blast);
    const int previous_dice = damage_dice_;
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
    {
        damage_dice_ = set.level;
    }
    log({"Delayed Blast Fireball goes off.", {}});
    const auto dice = shared_dice(delayed_blast_fireball, set.level);
    int total = dice.bonus;
    for (int die = 0; die < dice.count; ++die)
    {
        total += roll(dice.sides);
    }
    for (const auto id : within(set.cell, 3))
    {
        auto &target = *find(id);
        const bool saved = saving_throw(target, 4, 0, &caster);
        nonweapon_damage(target, total, 2, saved, fire, set.level, delayed_blast_fireball, false);
    }
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
    {
        damage_dice_ = previous_dice;
    }
}

// Heal (SpellHeal): disease, blindness and feeblemind cured, then every Hit
// Point but 1d4; one already that close to its maximum gains none.
void Session::heal_all_but_1d4(Fighter &target)
{
    constexpr int feebleminded = 268;
    bool cured = false;
    for (const int disease : {31, 34, 43, 44})
    {
        while (target.has(disease))
        {
            target.remove(disease);
            cured = true;
        }
    }
    if (cured)
    {
        log({"{name} is cured.", {{"name", target.name}}});
    }
    if (target.has(effect::blind))
    {
        target.remove(effect::blind);
        log({"{name} can see.", {{"name", target.name}}});
    }
    if (target.has(feebleminded))
    {
        target.remove(feebleminded);
        log({"{name} regains intellect.", {{"name", target.name}}});
    }
    const int short_of = roll(4);
    const int maximum = target.numbers.max_hit_points;
    if (target.hit_points + short_of <= maximum)
    {
        target.hit_points = maximum - short_of;
    }
    if (target.condition == Condition::dying)
    {
        target.condition = Condition::unconscious;
        target.overkill = 0;
    }
    log({"{name} is healed.", {{"name", target.name}}});
}

// Death Spell (SpellDeathSpell): 4d20 x 2 to spend on those in its area in
// turn, each costing by its level (1 at level 0, 2 at 1, 8 at 2 and 3, 16 at
// 4 and 5, 16 or 20 at 6 by whether its most Hit Points pass 51, 20 at 7 and
// 8, 162 above); each paid for drops dead. The unliving refuse it
// (refuses_spell).
void Session::cast_death_spell(const std::vector<EntityId> &targets)
{
    const int budget = (roll(20) + roll(20) + roll(20) + roll(20)) * 2;
    for (const auto id : death_spell_takes(targets, budget, true_most_hit_points))
    {
        kill(*find(id), "{name} drops dead.");
    }
}

// Pools of Darkness's own spells in a fight (magic.pas, in its
// reconstruction), by the rules' numbers (canonical_spell).
void Session::cast_darkness_spell(Fighter &caster, int number, int level,
                                  const std::vector<EntityId> &targets, Cell aim)
{
    constexpr int blade_barrier = 301, energy_drain = 303, destruction = 304, hold_animal = 308,
                  protection_from_fire = 309, mass_charm = 320, irresistible_dance = 321,
                  power_word_blind = 323, meteor_swarm = 324, power_word_kill = 325,
                  monster_summoning = 326;
    const auto &known = *game_spell(number);
    switch (number)
    {
    case blade_barrier:
        lay_cloud(caster, level, aim, CloudKind::blades);
        break;
    case energy_drain:
        for (const auto id : targets)
        {
            drain_levels(*find(id), 1);
        }
        break;
    case destruction:
        // Gone for good, unless magic resistance turns it away (SpellDestruction).
        for (const auto id : targets)
        {
            auto &target = *find(id);
            if (turns_aside_slaying(target) ||
                (resists_magic_at_all(target) && magic_resists(target, level, roll(100))))
            {
                log({"{name} is unaffected.", {{"name", target.name}}});
                continue;
            }
            target.condition = Condition::gone;
            target.hit_points = 0;
            target.initiative = 0;
            log({"{name} is destroyed.", {{"name", target.name}}});
            end_effects_at_zero(target);
            went_down(target);
        }
        break;
    case hold_animal:
    {
        // The fewer it holds, the harder they save: -4 for one, -2 for two,
        // -1 for three (SpellHoldAnimal).
        const auto count = targets.size();
        const int modifier = count == 1 ? -4 : count == 2 ? -2 : count == 3 ? -1 : 0;
        cast_generic(caster, number, level, targets, 0, 0, modifier);
        break;
    }
    case protection_from_fire:
        // A ranger gets the druid's protection, until it has taken 12 points
        // of fire a caster level; anyone else Resist Fire, with no end
        // (SpellProtectionFromFire).
        for (const auto id : targets)
        {
            auto &target = *find(id);
            const bool ranger =
                target.numbers.class_levels[static_cast<std::size_t>(ClassSlot::ranger)] > 0;
            (void)add_with_message(target,
                                   ranger ? Effect{effect::druid_fire_protection, 0, level, false}
                                          : Effect{effect::resist_fire, 0, 0, false},
                                   false, false, 0, level, &known);
        }
        break;
    case mass_charm:
        cast_mass_charm(caster, level, targets);
        break;
    case irresistible_dance:
        // A touch that must hit; the dance lasts 1d4+1 rounds
        // (SpellIrresistibleDance, CalcSpellDuration).
        for (const auto id : targets)
        {
            auto &target = *find(id);
            if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
                target.has(dq_effect::free_action))
            {
                continue;
            }
            if (!attack_hits(roll(20), caster.numbers.thac0, target.numbers.armor_class,
                             attack_modifier(caster, target, game_of(identity_)) +
                                 love_modifier(caster, target)))
            {
                log({"{attacker} misses {name}.",
                     {{"attacker", caster.name}, {"name", target.name}}});
                continue;
            }
            (void)add_with_message(target, {effect::dancing, roll(4) + 1, level, false}, false,
                                   false, 0, level, &known);
        }
        break;
    case power_word_blind:
        cast_power_word_blind(targets);
        break;
    case meteor_swarm:
        cast_meteor_swarm(caster, level, aim);
        break;
    case power_word_kill:
        cast_power_word_kill(targets);
        break;
    case monster_summoning:
        // It gates in creatures that wait off the field for it, which no
        // fight of OpenGoldBox's keeps (SpellMonsterSummoning).
        log({"{name}'s summons is not answered.", {{"name", caster.name}}});
        break;
    default:
        break;
    }
}

// Mass Charm charms enemies of up to twice the caster's level in Hit Dice,
// each saving at -2; it passes over a large creature, the caster's side and,
// in Pools of Darkness, a monster its record does not call a person (bit 2 of
// its "immunities", which every character has; SpellMassCharm).
void Session::cast_mass_charm(Fighter &caster, int level, const std::vector<EntityId> &targets)
{
    const auto &known = *game_spell(320);
    for (const auto [id, take] : mass_charm_takes(caster, level, targets))
    {
        auto &target = *find(id);
        if (take == MassCharmTake::unaffected)
        {
            log({"{name} is unaffected.", {{"name", target.name}}});
            continue;
        }
        if (take == MassCharmTake::passed_over)
        {
            continue;
        }
        const bool saved = saving_throw(target, 4, -2, &caster);
        const int strength = (caster.side == Side::opposition ? 128 : 0) + level;
        if (add_with_message(target, {effect::charm, 0, strength, true}, true, saved, 0, level,
                             &known))
        {
            charm(target, caster.side);
        }
    }
}

// What Mass Charm does with each target in turn, until its Hit Dice are spent
// (cast_mass_charm).
std::vector<std::pair<EntityId, Session::MassCharmTake>>
Session::mass_charm_takes(const Fighter &caster, int level,
                          const std::vector<EntityId> &targets) const
{
    std::vector<std::pair<EntityId, MassCharmTake>> takes;
    const int limit = 2 * level;
    int total = 0;
    for (const auto id : targets)
    {
        if (total == limit)
        {
            break;
        }
        const auto &target = *find(id);
        const bool person =
            target.numbers.race != 0 || (target.numbers.traits & trait::person) != 0;
        if ((darkness() && !person) || target.numbers.large || target.side == caster.side)
        {
            takes.emplace_back(id, MassCharmTake::unaffected);
            continue;
        }
        const int cost = target.numbers.level;
        if (total + cost > limit)
        {
            takes.emplace_back(id, MassCharmTake::passed_over);
            continue;
        }
        total += cost;
        takes.emplace_back(id, MassCharmTake::tried);
    }
    return takes;
}

// Power Word Blind blinds those within reach, the nearest first, up to 100
// Hit Points of them in all: for 1d4+1 rounds, ten times as long when they
// have fewer than 51 between them (SpellPowerWordBlind, which worked the time
// out and then blinded them for good, docs/ORIGINAL-RULES.md, Bugs).
void Session::cast_power_word_blind(const std::vector<EntityId> &targets)
{
    const auto &known = *game_spell(323);
    const auto blinded = blinded_by_word(targets, true_most_hit_points);
    int total = 0;
    for (const auto id : blinded)
    {
        total += find(id)->numbers.max_hit_points;
    }
    const int rounds = (roll(4) + 1) * (total < 51 ? 10 : 1);
    for (const auto id : blinded)
    {
        (void)add_with_message(*find(id), {effect::blind, rounds, 0, false}, false, false, 0, 0,
                               &known);
    }
}

// Meteor Swarm: four meteors land on the squares diagonal to the aim, or on
// it where the caster cannot see them; each strikes all on its way for 10d4
// and bursts on all within a square of where it lands for 10d4, halved by a
// save (SpellMeteorSwarm).
void Session::cast_meteor_swarm(Fighter &caster, int level, Cell aim)
{
    const int previous_dice = damage_dice_;
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
    {
        damage_dice_ = 10;
    }
    constexpr int fire_and_magic = 9;
    const auto ten_d4 = [this]
    {
        int total = meteor_dice.bonus;
        for (int die = 0; die < meteor_dice.count; ++die)
        {
            total += roll(meteor_dice.sides);
        }
        return total;
    };
    for (const auto offset : meteor_landings)
    {
        const auto [landing, struck] = meteor_fall(caster, aim, offset);
        for (const auto id : struck)
        {
            auto &target = *find(id);
            if (target.able())
            {
                nonweapon_damage(target, ten_d4(), 0, false, fire_and_magic, level, 324, false);
            }
        }
        for (const auto id : within(landing, 1))
        {
            auto &target = *find(id);
            if (target.able())
            {
                const bool saved = saving_throw(target, 4, 0, &caster, fire_and_magic);
                nonweapon_damage(target, ten_d4(), 2, saved, fire_and_magic, level, 324, false);
            }
        }
    }
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
    {
        damage_dice_ = previous_dice;
    }
}

// Where a meteor of Meteor Swarm lands, by its offset from the aim (on the
// aim where the caster cannot see the square), and whom it strikes on its
// way there: everyone standing on its path but the caster and the one where
// it lands (cast_meteor_swarm).
Session::MeteorFall Session::meteor_fall(const Fighter &caster, Cell aim, Cell offset) const
{
    MeteorFall fall{{aim.x + offset.x, aim.y + offset.y}, {}};
    if (!board_.contains(fall.landing) ||
        !rules::has_line_of_sight(board_, caster.cell, fall.landing))
    {
        fall.landing = aim;
    }
    LineWalk path(caster.cell, fall.landing);
    while (path.next())
    {
        const auto id = standing_at(path.at);
        if (id && id != caster.id && path.at != fall.landing &&
            std::ranges::find(fall.struck, id) == fall.struck.end())
        {
            fall.struck.push_back(id);
        }
    }
    return fall;
}

// Power Word Kill slays its first target of 11 to 60 most Hit Points, or
// else every one of 10 or fewer (SpellPowerWordKill).
void Session::cast_power_word_kill(const std::vector<EntityId> &targets)
{
    for (const auto id : slain_by_word(targets, true_most_hit_points))
    {
        if (auto &target = *find(id); !turns_aside_slaying(target))
        {
            kill(target, "{name} is slain.");
        }
    }
}

// Whether a spell that slays or petrifies outright is turned aside before
// its save (Secret's condition 9): an iron golem takes no spell, and an
// image takes it for its caster. Magic resistance does not turn it, as the
// original weighs resistance only against an effect or magical damage
// (CheckSpellResistance).
bool Session::turns_aside_slaying(Fighter &target)
{
    if (target.has(effect::iron_golem))
    {
        log({"{name} is unaffected.", {{"name", target.name}}});
        return true;
    }
    return mirror_absorbs(target);
}

// An effect added as a spell adds it (fxengine.pas, AddEffectWithMessage): the
// target's immunities may nullify it, a save that negates refuses it, and an
// effect it has already is replaced by a longer or lasting one.
bool Session::add_with_message(Fighter &target, Effect added, bool negates, bool saved, int element,
                               int caster_level, const Spell *known)
{
    const int code = added.code;
    auto dice = fight_dice();
    if (refuses_effect(target, code, element, caster_level, known, dice) || (negates && saved))
    {
        log({"{name} is unaffected.", {{"name", target.name}}});
        return false;
    }
    if (auto *old = target.find_effect(code);
        old && ((old->minutes != 0 && added.minutes > old->minutes) || added.minutes == 0))
    {
        target.remove(code);
    }
    target.numbers.effects.push_back(added);
    if (code == effect::sleep && target.condition == Condition::okay)
    {
        target.initiative = 0;
    }
    log({"{name} is affected by {spell}.",
         {{"name", target.name}, {"spell", std::string(known ? known->name : ""), true}}});
    return true;
}

// Whether a non-weapon harm does something else to the target than damage:
// an iron golem's, a flesh golem's and fire on one that fire heals
// (absorb_harm).
bool absorbs_harm(const Fighter &target, int element)
{
    return target.has(effect::iron_golem) || target.has(dkk_effect::flesh_golem) ||
           (target.has(dkk_effect::fire_heals) && (element & 1));
}

// Damage that is not a weapon's (fxengine.pas, InflictNonWeaponDamage): the
// target's resistances weigh it by its element (1 fire, 2 cold, 4
// electricity, 8 magic, 16 acid), then a save halves or negates it
// (defended_harm).
void Session::nonweapon_damage(Fighter &target, int damage, int save_result, bool saved,
                               int element, int caster_level, int spell_number, bool single_target)
{
    if (absorbs_harm(target, element))
    {
        absorb_harm(target, damage, element);
        return;
    }
    auto dice = fight_dice();
    const auto defended = defended_harm(target, damage, save_result, saved, element, caster_level,
                                        spell_number, single_target, damage_dice_, dice);
    if (defended.image_took_it)
    {
        lose_image(target);
    }
    if (auto *soak = target.find_effect(effect::druid_fire_protection);
        soak && (defended.soak_spent > 0 || defended.soak_ends))
    {
        soak->strength -= defended.soak_spent;
        if (defended.soak_ends)
        {
            target.remove(effect::druid_fire_protection);
        }
    }
    if (defended.unaffected)
    {
        log({"{name} is unaffected.", {{"name", target.name}}});
    }
    if (defended.grown > 0)
    {
        target.hit_points += defended.grown;
        log({"{name} grows.", {{"name", target.name}}});
    }
    if (defended.regenerates_soon)
    {
        target.numbers.effects.push_back({effect::regenerating_soon, 3, 0, true});
    }
    if (defended.damage <= 0)
    {
        return;
    }
    log({"{name} takes {damage}.",
         {{"name", target.name}, {"damage", std::to_string(defended.damage)}}});
    inflict(target, defended.damage);
}

// What an iron golem, a flesh golem or one that fire heals does with a
// non-weapon harm (absorbs_harm).
void Session::absorb_harm(Fighter &target, int damage, int element)
{
    if (target.has(effect::iron_golem))
    {
        iron_golem_absorbs(target, damage, element);
        return;
    }
    if (target.has(dkk_effect::flesh_golem))
    {
        if ((element & 4) && (element & 8))
        {
            target.hit_points = std::min(target.hit_points + std::max(damage_dice_, 0),
                                         target.numbers.max_hit_points);
            log({"{name} is healed.", {{"name", target.name}}});
        }
        else if ((element & 3) && (element & 8))
        {
            target.numbers.effects.push_back({effect::slowed, roll(6) + roll(6), 255, false});
            log({"{name} is slowed.", {{"name", target.name}}});
        }
        return;
    }
    if (element & 8)
    {
        target.hit_points = std::min(target.hit_points + damage, target.numbers.max_hit_points);
        log({"{name} is healed.", {{"name", target.name}}});
    }
}

// Secret's iron golem takes no damage but a weapon's (effhndlr.pas,
// EffIronGolem, in its reconstruction): magical fire heals it by the damage,
// up to its maximum, before any save; electricity slows it for 3 rounds.
void Session::iron_golem_absorbs(Fighter &golem, int damage, int element)
{
    if ((element & 1) && (element & 8))
    {
        golem.hit_points = std::min(golem.hit_points + damage, golem.numbers.max_hit_points);
        log({"{name} is healed.", {{"name", golem.name}}});
    }
    else if (element & 4)
    {
        golem.numbers.effects.push_back({effect::slowed, 3, 255, false});
        log({"{name} is slowed.", {{"name", golem.name}}});
    }
    else
    {
        log({"{name} is unaffected.", {{"name", golem.name}}});
    }
}

// A charmed creature fights for its charmer (effhndlr.pas, the charm).
void Session::charm(Fighter &target, Side charmer)
{
    target.own_side = target.side;
    target.side = charmer;
    target.fleeing = false;
}

// Sleep (magic.pas, the sleep): 4d4 levels' worth around the aim, the nearest
// first, allies too; a creature costs 1 up to level 1, 2 at 2, 4 at 3, 6 at 4,
// 10 at 5 for a monster and 20 otherwise, 20 from 6 up, spent before its
// immunities are tried. Those already asleep and the animated dead are
// passed over.
void Session::cast_sleep(int level, Cell aim)
{
    const int pool = roll(4) + roll(4) + roll(4) + roll(4);
    const auto &known = *game_spell(21);
    for (const auto id : sleep_takes(aim, pool))
    {
        add_with_message(*find(id), {effect::sleep, 5 * level, level, false}, false, false, 0,
                         level, &known);
    }
}

// Whom a Sleep of this pool of levels at a square puts to sleep, the
// nearest first (cast_sleep).
std::vector<EntityId> Session::sleep_takes(Cell aim, int pool) const
{
    std::vector<EntityId> taken;
    for (const auto id : within(aim, game_spell(21)->area & 7))
    {
        const auto &f = *find(id);
        if (f.has(effect::sleep) || f.has(effect::animated))
        {
            continue;
        }
        const int highest = f.numbers.level;
        const int cost = highest <= 1   ? 1
                         : highest == 2 ? 2
                         : highest == 3 ? 4
                         : highest == 4 ? 6
                         : highest == 5 ? (f.numbers.race == 0 ? 10 : 20)
                                        : 20;
        if (cost > pool)
        {
            continue;
        }
        pool -= cost;
        taken.push_back(id);
    }
    return taken;
}

// Whether a carrier of Silence stands within a square of it, itself
// included (effhndlr.pas, EffSilence's aura).
bool Session::silenced(const Fighter &f) const
{
    return std::any_of(fighters_.begin(), fighters_.end(),
                       [&](const Fighter &carrier)
                       {
                           return carrier.able() && carrier.has(effect::silence) &&
                                  (carrier.id == f.id || distance(carrier, f) <= 1);
                       });
}

// Prayer: within 6 squares of the first one praying, in its sight, its own
// side gets +1 to attacks and saves and the other side -1 (effhndlr.pas,
// EffPrayer, EffAlliedPrayer).
int Session::prayer_bonus(const Fighter &f) const
{
    for (const auto &carrier : fighters_)
    {
        const auto *node = carrier.able() ? carrier.find_effect(effect::prayer) : nullptr;
        if (!node ||
            (carrier.id != f.id && (distance(carrier, f) > 6 || !sees(board_, carrier, f))))
        {
            continue;
        }
        const Side praying = (node->strength & 0x10) ? Side::opposition : Side::party;
        return f.side == praying ? 1 : -1;
    }
    return 0;
}

// Protected by its own spell, or by a 10' radius one within a square of it
// (effhndlr.pas, 45 and 46).
bool Session::protected_from(const Fighter &f, int code, int aura) const
{
    if (f.has(code))
    {
        return true;
    }
    return std::any_of(fighters_.begin(), fighters_.end(),
                       [&](const Fighter &carrier)
                       {
                           return carrier.able() && carrier.has(aura) &&
                                  (carrier.id == f.id || distance(carrier, f) <= 1);
                       });
}

// An evil or good attacker 2 worse against one only a 10' radius protects
// (its own spell already counts in attack_modifier).
bool Session::aura_protects(const Fighter &attacker, const Fighter &defender) const
{
    const int alignment = attacker.numbers.alignment;
    const bool evil = alignment == 2 || alignment == 5 || alignment == 8;
    const bool good = alignment == 0 || alignment == 3 || alignment == 6;
    return (evil && !defender.has(effect::protection_from_evil) &&
            protected_from(defender, effect::protection_from_evil,
                           effect::protection_from_evil_aura)) ||
           (good && !defender.has(effect::protection_from_good) &&
            protected_from(defender, effect::protection_from_good,
                           effect::protection_from_good_aura));
}

// A mirror image takes the harm on 1d(images + 1) above 1, and is gone
// (effhndlr.pas, EffMirrorImage).
bool Session::mirror_absorbs(Fighter &target)
{
    const auto *images = target.find_effect(effect::mirror_image);
    auto dice = fight_dice();
    if (!images || !image_takes_it(*images, dice))
    {
        return false;
    }
    lose_image(target);
    return true;
}

// The image that took the harm is gone.
void Session::lose_image(Fighter &target)
{
    log({"{name} loses an image.", {{"name", target.name}}});
    auto *images = target.find_effect(effect::mirror_image);
    const bool packed = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn;
    const int count = packed ? images->strength >> 5 : images->strength;
    if (count <= 1)
    {
        target.remove(effect::mirror_image);
    }
    else
    {
        images->strength = packed ? ((count - 1) << 5) | (images->strength & 0x1F) : count - 1;
    }
}

// The creature standing on a square, 0 for none.
EntityId Session::standing_at(Cell cell) const
{
    for (const auto &f : fighters_)
    {
        const auto mine = squares_of(f);
        if (f.able() && std::find(mine.begin(), mine.end(), cell) != mine.end())
        {
            return f.id;
        }
    }
    return 0;
}

// Lightning strikes a square: one standing there saves against spells for
// half; a wall turns the bolt (magic.pas, LightningAttackAt). Lightning Bolt's
// is electricity and magic (element 12); a breath's is not a spell.
bool Session::lightning_at(Cell cell, int damage, int level, int save, int element,
                           int spell_number)
{
    if (const auto id = standing_at(cell))
    {
        auto &target = *find(id);
        const bool saved = saving_throw(target, save, 0, nullptr, element);
        nonweapon_damage(target, damage, 2, saved, element, level, spell_number, false);
    }
    return turns_bolt(cell);
}

// Whether a bolt striking a square turns there: a wall stands on it.
bool Session::turns_bolt(Cell cell) const
{
    return board_.contains(cell) && board_.at(cell) == rules::Terrain::obstacle;
}

// The bolt runs on from the aim, away from the caster (magic.pas,
// LightningPath): each leg goes to the next creature, the field's edge (where
// the bolt ends), rough ground, or the end of its length; a wall turns it back
// past the caster, costing 8 more when the wall is within 4 squares of it.
// Each leg takes the whole distance walked so far from what is left, as the
// original counts it.
void Session::lightning_path(Cell origin, Cell aim, int length, int damage, int level, bool bounce,
                             int save, int element, int spell_number)
{
    const auto strike = [&](Cell cell)
    {
        return lightning_at(cell, damage, level, save, element, spell_number);
    };
    bolt_walk(origin, aim, length, bounce, strike);
}

// Stinking Cloud (magic.pas, SpellStinkingCloud): a cloud on the aim and the
// squares east, south-east and south of it, lasting the caster's level in
// rounds; those in it breathe it at once. Curse's Cloudkill (SpellCloudkill)
// lays its cloud on the aim and the eight squares around it.
void Session::lay_cloud(Fighter &caster, int level, Cell aim, CloudKind kind)
{
    // The caster's clouds of the kind before the last one laid, as the
    // original counts them.
    int index = 0;
    for (std::size_t n = 0; n + 1 < clouds_.size(); ++n)
    {
        index += clouds_[n].caster == caster.id && clouds_[n].kind == kind ? 1 : 0;
    }
    const auto cloud = cloud_over(caster.id, index, aim, kind);
    const auto squares = cloud.squares();
    clouds_.push_back(cloud);
    if (kind == CloudKind::blades)
    {
        // The blades whirl three rounds a caster level, and those in them as
        // they rise may save (fxengine.pas, ConsiderTerrainEffects).
        caster.numbers.effects.push_back(
            {effect::blade_barrier, 3 * level, level + index * 16, true});
        log({"{name} raises a barrier of whirling blades.", {{"name", caster.name}}});
        for (const auto square : squares)
        {
            if (const auto id = standing_at(square))
            {
                cut_by_blades(*find(id), true);
            }
        }
        return;
    }
    const bool stinking = kind == CloudKind::stinking;
    caster.numbers.effects.push_back(
        {stinking ? effect::stinking_cloud : effect::cloudkill, level, level + index * 16, true});
    log({stinking ? "{name} creates a noxious cloud." : "{name} creates a poisonous cloud.",
         {{"name", caster.name}}});
    for (const auto square : squares)
    {
        if (const auto id = standing_at(square))
        {
            breathe_cloud(*find(id));
        }
    }
}

// A cloud of the kind laid at a square, covering its squares on the field
// that no wall stands on.
Cloud Session::cloud_over(EntityId caster, int index, Cell aim, CloudKind kind) const
{
    Cloud cloud{caster, index, aim, {}, kind};
    const auto squares = cloud.squares();
    for (std::size_t n = 0; n < squares.size(); ++n)
    {
        cloud.covers[n] =
            board_.contains(squares[n]) && board_.at(squares[n]) != rules::Terrain::obstacle;
    }
    return cloud;
}

bool Session::in_cloud(const Fighter &f, CloudKind kind) const
{
    const auto mine = squares_of(f);
    return std::any_of(clouds_.begin(), clouds_.end(),
                       [&mine, kind](const Cloud &cloud)
                       {
                           if (cloud.kind != kind)
                           {
                               return false;
                           }
                           const auto squares = cloud.squares();
                           for (std::size_t n = 0; n < squares.size(); ++n)
                           {
                               if (cloud.covers[n] &&
                                   std::find(mine.begin(), mine.end(), squares[n]) != mine.end())
                               {
                                   return true;
                               }
                           }
                           return false;
                       });
}

// In Secret and Pools of Darkness the undead and the unliving (the record's
// flag) breathe a stinking cloud unharmed (fxengine.pas,
// ConsiderTerrainEffects, in their reconstructions).
bool Session::breathes_clouds_freely(const Fighter &f) const
{
    return secret_engine() &&
           (f.numbers.undead_level > 0 || (f.numbers.traits & trait::unliving) != 0);
}

// What the clouds do to one in them (fxengine.pas, ConsiderTerrainEffects).
// Pools of Darkness's has its own stench and cloudkill, and passes over the
// stench as the round ends (combat.pas, EndOfRound, skipCloud).
void Session::breathe_cloud(Fighter &f, bool round_ends)
{
    if (darkness())
    {
        if (!round_ends)
        {
            breathe_darkness_stench(f);
        }
        breathe_darkness_cloudkill(f);
    }
    else
    {
        breathe_stinking_cloud(f);
        breathe_cloudkill(f);
    }
    cut_by_blades(f, false);
    if (f.able() && in_cloud(f, CloudKind::acid))
    {
        log({"{name} is burned by acid.", {{"name", f.name}}});
        nonweapon_damage(f, roll(6), 0, false, 16, 0, 0, false);
    }
}

// The level of the caster of the first cloud of the kind, for the magic
// resistance its harm meets; 12 once the caster is gone.
int Session::cloud_caster_level(CloudKind kind, int spell_number) const
{
    const auto cloud = std::find_if(clouds_.begin(), clouds_.end(),
                                    [kind](const Cloud &c)
                                    {
                                        return c.kind == kind;
                                    });
    const auto *caster = cloud != clouds_.end() ? find(cloud->caster) : nullptr;
    return caster ? caster_level(*caster, *game_spell(spell_number), game_of(identity_)) : 12;
}

// Pools of Darkness's Blade Barrier cuts one in it for 8d8, which magic
// resistance may turn away; only as the blades rise may it save
// (fxengine.pas, ConsiderTerrainEffects).
void Session::cut_by_blades(Fighter &f, bool may_save)
{
    if (!f.able() || !in_cloud(f, CloudKind::blades))
    {
        return;
    }
    const auto blades = std::find_if(clouds_.begin(), clouds_.end(),
                                     [](const Cloud &c)
                                     {
                                         return c.kind == CloudKind::blades;
                                     });
    const auto *caster = find(blades->caster);
    const int level = cloud_caster_level(CloudKind::blades, 301);
    if (resists_magic_at_all(f) && magic_resists(f, level, roll(100)))
    {
        log({"{name} is unaffected.", {{"name", f.name}}});
        return;
    }
    if (may_save && saving_throw(f, 4, 0, caster))
    {
        return;
    }
    log({"{name} is cut by the blades.", {{"name", f.name}}});
    int damage = blade_dice.bonus;
    for (int die = 0; die < blade_dice.count; ++die)
    {
        damage += roll(blade_dice.sides);
    }
    nonweapon_damage(f, damage, 0, false, 0, level, 301, false);
}

// Cloudkill kills one of level 0 to 4 outright with its poison; one of level
// 5 saves against it at -4, one of level 6 at 0; one of a higher level breathes
// it unharmed.
void Session::breathe_cloudkill(Fighter &f)
{
    const int level = f.numbers.level;
    if (!f.able() || !in_cloud(f, CloudKind::deadly) || level > 6 ||
        (level >= 5 && saving_throw(f, 0, level == 5 ? -4 : 0)))
    {
        return;
    }
    log({"{name} is poisoned.", {{"name", f.name}}});
    f.numbers.effects.push_back({effect::poisoned, 0, 255, false});
    kill(f);
}

// Pools of Darkness's cloudkill (fxengine.pas, ConsiderTerrainEffects): one
// its record calls proof against poison breathes it unharmed; one of 0 to 4
// Hit Dice dies of its poison, one of 5 saves at -4, one of 6 at 0, and one
// of more takes 1d10, magical and poisonous.
void Session::breathe_darkness_cloudkill(Fighter &f)
{
    if (!f.able() || !in_cloud(f, CloudKind::deadly) ||
        (f.numbers.traits & trait::poison_proof) != 0)
    {
        return;
    }
    const int hit_dice = f.numbers.level;
    if (hit_dice > 6)
    {
        constexpr int magical_poison = 0x88;
        nonweapon_damage(f, roll(10), 0, false, magical_poison,
                         cloud_caster_level(CloudKind::deadly, 191), 191, false);
        return;
    }
    if (hit_dice >= 5 && saving_throw(f, 0, hit_dice == 5 ? -4 : 0))
    {
        return;
    }
    log({"{name} is poisoned.", {{"name", f.name}}});
    f.numbers.effects.push_back({effect::poisoned, 0, 255, false});
    kill(f);
}

// Pools of Darkness's stinking cloud (fxengine.pas, ConsiderTerrainEffects):
// the undead, the nauseous and one its record calls unliving or proof against
// poison breathe it unharmed; the rest save against paralysis, one that saves
// coughing for 1d4+1 rounds and losing the spell it was casting, one that
// fails helpless with nausea for 1d4+1.
void Session::breathe_darkness_stench(Fighter &f)
{
    constexpr int spared = trait::unliving | trait::poison_proof;
    if (!f.able() || !in_cloud(f, CloudKind::stinking) || f.numbers.undead_level > 0 ||
        f.has(effect::nausea) || (f.numbers.traits & spared) != 0)
    {
        return;
    }
    const auto &known = *game_spell(34);
    if (!saving_throw(f, 0, 0))
    {
        add_with_message(f, {effect::nausea, roll(4) + 1, 255, false}, false, false, 0, 0, &known);
        return;
    }
    add_with_message(f, {effect::coughing, roll(4) + 1, 255, false}, false, true, 0, 0, &known);
    if (f.pending_spell || f.aiming)
    {
        lose_spell(f);
        end_actions(f);
    }
}

// One in a stinking cloud saves against paralysis: saved, it coughs for the
// round; failed, nausea leaves it helpless for 1d4+1 rounds. One coughing,
// nauseous, animated or immune to poison breathes it unharmed.
void Session::breathe_stinking_cloud(Fighter &f)
{
    if (!f.able() || !in_cloud(f, CloudKind::stinking) || f.has(effect::coughing) ||
        f.has(effect::nausea) || f.has(effect::animated) || f.has(111) || f.has(125) ||
        breathes_clouds_freely(f))
    {
        return;
    }
    const auto &known = *game_spell(34);
    const bool native = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn;
    // Native terrain dispatch uses the current actor's casting level:
    // the caster on initial placement, the creature on its later turn.
    const auto *acting = find(actor_);
    const int level = native ? caster_level(acting ? *acting : f, known, game_of(identity_)) : 0;
    if (saving_throw(f, 0, 0))
    {
        add_with_message(f, {effect::coughing, native ? roll(4) + 1 : 1, 255, false}, false, true,
                         0, level, &known);
        return;
    }
    add_with_message(f, {effect::nausea, roll(4) + 1, 255, false}, false, false, 0, level, &known);
}

// An effect ends before its time, its handler running as it goes: a charm
// lets go, the animated collapse, and the rest end as when their time runs out.
void Session::remove_with_handler(Fighter &f, std::size_t node)
{
    const auto ended = f.numbers.effects[node];
    f.numbers.effects.erase(f.numbers.effects.begin() + static_cast<std::ptrdiff_t>(node));
    if (ended.code == effect::charm)
    {
        f.side = f.own_side;
        return;
    }
    if (ended.code == effect::animated)
    {
        // It collapses, dead again, what it was before (effhndlr.pas, EffAnimateDead).
        if (f.before_animation)
        {
            auto numbers = *f.before_animation;
            numbers.spells.clear();
            numbers.effects = f.numbers.effects;
            f.numbers = numbers;
            f.before_animation.reset();
        }
        f.side = f.own_side;
        f.condition = Condition::dead;
        f.hit_points = 0;
        f.initiative = 0;
        log({"{name} collapses.", {{"name", f.name}}});
        return;
    }
    if (ended.code == effect::enlarged || ended.code == effect::strengthened)
    {
        recount_strength(f);
    }
    if (ended.on_remove)
    {
        expire(f, ended);
    }
}

// Dispel Magic (magic.pas, the dispel): each effect of a strength below 255
// has its level in the strength's low 4 bits, and goes on a d100 within 50%,
// 5% more for each level the caster has above it or 2% less for each below.
void Session::dispel(Fighter &target, int level)
{
    bool any = false;
    for (std::size_t node = 0; node < target.numbers.effects.size();)
    {
        const auto &e = target.numbers.effects[node];
        if (!dispellable(e))
        {
            ++node;
            continue;
        }
        if (roll(100) <= dispel_chance(e, level))
        {
            remove_with_handler(target, node);
            any = true;
            continue;
        }
        ++node;
    }
    if (any)
    {
        log({"{name} is dispelled.", {{"name", target.name}}});
    }
}

// Whether Dispel Magic may take an effect: one of a strength below 255, but
// never a creature's age, nor in Treasures love, a kampfult's crushing, a
// stone golem's or a yeti's grab (its magic.pas, SpellDispelMagic).
bool Session::dispellable(const Effect &e) const
{
    const bool treasures_kept =
        treasures() && (e.code == effect::in_love || e.code == effect::crushes ||
                        (e.code >= effect::stone_golem && e.code <= effect::grabs_6d4));
    return e.code != effect::age_in_years && !treasures_kept && e.strength < 255;
}

// The percent chance Dispel Magic of a caster's level takes an effect: 50,
// 5 more for each level the caster has above the effect's (its strength's
// low 4 bits, or Native's low 5) or 2 less for each below.
int Session::dispel_chance(const Effect &e, int caster_level) const
{
    const int effect_level =
        e.strength & (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn ? 0x1F : 0x0F);
    return caster_level >= effect_level ? 50 + 5 * (caster_level - effect_level)
                                        : 50 - 2 * (effect_level - caster_level);
}

// Animate Dead (magic.pas, the animation): up to the caster's level of the
// dead of creature type 0, either side, whose squares are free, rise whole
// for the caster as undead (level 2) moving 6, sweeping no one, their spells
// forgotten; the animated die at exactly 0 Hit Points.
void Session::animate_dead(Fighter &caster, int level)
{
    int left = level;
    for (auto &f : fighters_)
    {
        if (left <= 0 || f.condition != Condition::dead || f.numbers.creature_type != 0 ||
            f.has(effect::animated) || !cell_free(f))
        {
            continue;
        }
        --left;
        f.before_animation = f.numbers;
        f.own_side = f.side;
        f.side = caster.side;
        f.numbers.undead_level = 2;
        f.numbers.movement = 6;
        f.numbers.attack_level = 0;
        f.numbers.spells.clear();
        f.numbers.creature_type = 4;
        f.condition = Condition::okay;
        f.hit_points = f.numbers.max_hit_points;
        f.overkill = 0;
        f.initiative = 0;
        f.numbers.effects.push_back(
            {effect::animated, 0, (f.own_side == Side::opposition ? 16 : 0) + level, true});
        log({"{name} is animated.", {{"name", f.name}}});
    }
}

// Slow Poison (magic.pas, the slow poison): one poisoned, even killed by it,
// stands again with at least 1 Hit Point while the spell lasts, losing a point
// every 10 minutes but never the last; when it wears off still poisoned, it dies.
void Session::slow_poison(Fighter &target, int level)
{
    if (!target.has(effect::poisoned) || target.has(effect::animated))
    {
        return;
    }
    target.hit_points = std::max(target.hit_points, 1);
    target.numbers.effects.push_back({effect::slow_poison, 60 * level, 255, true});
    log({"{name} is affected by {spell}.",
         {{"name", target.name}, {"spell", std::string(game_spell(26)->name), true}}});
    revive(target);
    target.numbers.effects.push_back({effect::slow_poison_tick, 10, 255, true});
}

// The poisoned dead stand when their square is free, or try again a minute
// later (effhndlr.pas, EffDelayedRevival).
void Session::revive(Fighter &f)
{
    if (f.condition == Condition::okay)
    {
        return;
    }
    if (cell_free(f))
    {
        stand_up(f, std::max(f.hit_points, 1), "{name} gets back up.");
        return;
    }
    f.numbers.effects.push_back({effect::revival, 1, 255, true});
}

// The roll a cleric needs to turn undead, by the undead's level (1 skeleton to
// 10 vampire) and the cleric's level (1 to 8, 9 to 13, 14 up): above 0 it
// turns, 0 destroys, below 0 destroys on that much or more, 99 never
// (cbtacts.pas, CombatTurnUndead).
// Pools of Darkness's goes on to undead of levels 11 to 13 (its
// turnUndeadTable); the earlier games turn none above 10.
constexpr std::array<std::array<int, 10>, 13> turning_table{{
    {10, 7, 4, 1, 1, 0, 0, -1, -1, -1},
    {13, 10, 7, 1, 1, 0, 0, 0, -1, -1},
    {16, 13, 10, 4, 1, 1, 0, 0, 0, -1},
    {19, 16, 13, 7, 4, 1, 1, 0, 0, -1},
    {20, 19, 16, 10, 7, 4, 1, 1, 0, 0},
    {99, 20, 19, 13, 10, 7, 4, 1, 1, 0},
    {99, 99, 20, 16, 13, 10, 7, 4, 1, 0},
    {99, 99, 99, 20, 16, 13, 10, 7, 4, 1},
    {99, 99, 99, 99, 20, 16, 13, 10, 7, 1},
    {99, 99, 99, 99, 99, 20, 16, 13, 10, 4},
    {99, 99, 99, 99, 99, 99, 20, 16, 13, 10},
    {99, 99, 99, 99, 99, 99, 99, 19, 16, 10},
    {99, 99, 99, 99, 99, 99, 99, 20, 19, 13},
}};

// Turning undead, once a fight (cbtacts.pas, CombatTurnUndead): one d20 for
// the whole attempt against up to 1d12 undead enemies in sight, the lowest
// first and the nearest of those; each it beats is turned, or destroyed, and
// the first it fails ends the attempt. Destruction goes on to at least six.
// The cleric's actions end.
void Session::turn_undead(Fighter &cleric)
{
    if (treasures())
    {
        treasures_turn_undead(cleric);
        return;
    }
    if (darkness() || game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
    {
        darkness_turn_undead(cleric);
        return;
    }
    cleric.tried_turning = true;
    log({"{name} turns undead.", {{"name", cleric.name}}});
    const int level = cleric.numbers.class_levels[static_cast<std::size_t>(ClassSlot::cleric)];
    const auto column = static_cast<std::size_t>(level <= 8 ? level - 1 : level <= 13 ? 8 : 9);
    int budget = roll(12);
    int at_least = 6;
    const int natural = roll(20);
    bool any = false;
    while (budget > 0)
    {
        Fighter *target = nullptr;
        for (auto &f : fighters_)
        {
            if (f.side == cleric.side || !f.able() || f.turned || f.numbers.undead_level < 1 ||
                f.numbers.undead_level > 10 || !sees(board_, cleric, f))
            {
                continue;
            }
            if (!target || f.numbers.undead_level < target->numbers.undead_level ||
                (f.numbers.undead_level == target->numbers.undead_level &&
                 distance(cleric, f) < distance(cleric, *target)))
            {
                target = &f;
            }
        }
        if (!target)
        {
            break;
        }
        const int need =
            turning_table[static_cast<std::size_t>(target->numbers.undead_level - 1)][column];
        if (natural < std::abs(need))
        {
            break;
        }
        any = true;
        if (need > 0)
        {
            target->turned = true;
            log({"{name} is turned.", {{"name", target->name}}});
        }
        else
        {
            target->condition = Condition::gone;
            target->hit_points = 0;
            target->initiative = 0;
            log({"{name} is destroyed.", {{"name", target->name}}});
            end_effects_at_zero(*target);
        }
        at_least = std::max(at_least - 1, 0);
        --budget;
        if (budget == 0 && at_least > 0 && need < 0)
        {
            budget = 1;
        }
    }
    if (!any)
    {
        log({"Nothing happens.", {}});
    }
    end_actions(cleric);
}

// Pools of Darkness's turning (cbtacts.pas, CombatTurnUndead and
// FindTurnUndeadTarget): its column is the better of the cleric's level and
// the paladin's less 2 (to 8, 9 to 13, 14 up), less the area's penalty
// (rules::Encounter::turning_penalty), which may leave none to turn with.
// One d20 for the attempt. The undead enemy in sight of the highest level not
// above the last one tried comes first, the nearest of those; one the roll
// beats is turned off the field or destroyed, and one it fails sends the
// attempt on to the levels below. The first one tried sets how many it may
// affect: 1d6+6 where it would be destroyed past doubt, else 1d12.
// Destruction goes on to at least six. The turner's actions end.
void Session::darkness_turn_undead(Fighter &cleric)
{
    ++cleric.turn_attempts;
    log({"{name} turns undead.", {{"name", cleric.name}}});
    const auto level = [&cleric](ClassSlot slot)
    {
        return cleric.numbers.class_levels[static_cast<std::size_t>(slot)];
    };
    const bool native = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn;
    const int deity_bonus = native && cleric.record && cleric.record->deity == 2 ? 2 : 0;
    const int best =
        std::max(level(ClassSlot::cleric), level(ClassSlot::paladin) - 2) + deity_bonus;
    const int full_column = best <= 8 ? best : best <= 13 ? 9 : 10;
    const int column = turning_penalty_ > full_column ? 0 : full_column - turning_penalty_;
    const int natural = roll(20);
    int highest = static_cast<int>(turning_table.size());
    std::optional<int> budget;
    int at_least = 6;
    bool any = false;
    while (!budget || *budget > 0)
    {
        Fighter *target = nullptr;
        for (auto &f : fighters_)
        {
            const int undead = f.numbers.undead_level;
            if (f.side == cleric.side || !f.able() || f.turned || undead < 1 || undead > highest ||
                !sees(board_, cleric, f))
            {
                continue;
            }
            if (!target || undead > target->numbers.undead_level ||
                (undead == target->numbers.undead_level &&
                 distance(cleric, f) < distance(cleric, *target)))
            {
                target = &f;
            }
        }
        if (!target)
        {
            break;
        }
        highest = target->numbers.undead_level;
        constexpr int never = 99;
        const int need = column == 0 ? never
                                     : turning_table[static_cast<std::size_t>(highest - 1)]
                                                    [static_cast<std::size_t>(column - 1)];
        if (!budget)
        {
            budget = need < 0 ? roll(6) + 6 : roll(12);
        }
        if (natural < std::abs(need))
        {
            --highest;
            continue;
        }
        any = true;
        if (need > 0)
        {
            log({"{name} is turned.", {{"name", target->name}}});
            put_out_of_play(*target, Condition::fled);
        }
        else
        {
            target->condition = Condition::gone;
            target->hit_points = 0;
            target->initiative = 0;
            log({"{name} is destroyed.", {{"name", target->name}}});
            end_effects_at_zero(*target);
        }
        at_least = std::max(at_least - 1, 0);
        --*budget;
        if (*budget == 0 && at_least > 0 && need < 0)
        {
            budget = 1;
        }
    }
    if (!any)
    {
        log({"Nothing happens.", {}});
    }
    end_actions(cleric);
}

// Whether the creature may turn undead now: one with cleric levels, once a
// fight (combat.pas, CombatMenu), but in Secret, which has no turning. In
// Treasures a cleric or a paladin past level 2, as many times a fight as
// the undead the fight began with (its combat.pas, CombatMenu; eclvm.pas,
// LOAD MONSTER's undeadCount). In Pools of Darkness a cleric or a paladin past
// level 2, as often as the fight's LOAD MONSTERs brought undead
// (turnUndeadLimit).
bool Session::may_turn(const Fighter &f) const
{
    const auto &levels = f.numbers.class_levels;
    if (darkness() || game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
    {
        return (levels[static_cast<std::size_t>(ClassSlot::cleric)] > 0 ||
                levels[static_cast<std::size_t>(ClassSlot::paladin)] > 2) &&
               f.turn_attempts < turning_tries_;
    }
    if (!treasures())
    {
        return levels[static_cast<std::size_t>(ClassSlot::cleric)] > 0 && !f.tried_turning &&
               !secret();
    }
    const auto undead = std::ranges::count_if(fighters_,
                                              [](const Fighter &o)
                                              {
                                                  return o.numbers.undead_level > 0;
                                              });
    return (levels[static_cast<std::size_t>(ClassSlot::cleric)] > 0 ||
            levels[static_cast<std::size_t>(ClassSlot::paladin)] > 2) &&
           f.turn_attempts < undead;
}

// Treasures' table (its gdconsts.pas, turnUndeadTable): undead levels 1 to
// 13, by the turning column 0 (none) to 10, read as turning_table.
constexpr std::array<std::array<int, 11>, 13> treasures_turning_table{{
    {99, 10, 7, 4, 1, 1, 0, 0, -1, -1, -1},
    {99, 13, 10, 7, 1, 1, 0, 0, 0, -1, -1},
    {99, 16, 13, 10, 4, 1, 1, 0, 0, 0, -1},
    {99, 19, 16, 13, 7, 4, 1, 1, 0, 0, -1},
    {99, 20, 19, 16, 10, 7, 4, 1, 1, 0, 0},
    {99, 99, 20, 19, 13, 10, 7, 4, 1, 1, 0},
    {99, 99, 99, 20, 16, 13, 10, 7, 4, 1, 0},
    {99, 99, 99, 99, 20, 16, 13, 10, 7, 4, 1},
    {99, 99, 99, 99, 99, 20, 16, 13, 10, 7, 1},
    {99, 99, 99, 99, 99, 99, 20, 16, 13, 10, 4},
    {99, 99, 99, 99, 99, 99, 99, 20, 16, 13, 10},
    {99, 99, 99, 99, 99, 99, 99, 99, 19, 16, 10},
    {99, 99, 99, 99, 99, 99, 99, 99, 20, 19, 13},
}};

// The strongest undead enemy in sight, not turned, of a level no higher
// than given, the nearest of equals (its cbtacts.pas, FindTurnUndeadTarget).
Fighter *Session::strongest_undead(const Fighter &turner, int highest_level)
{
    Fighter *target = nullptr;
    for (auto &f : fighters_)
    {
        const int level = f.numbers.undead_level;
        if (f.side == turner.side || !f.able() || f.turned || level < 1 || level > highest_level ||
            !sees(board_, turner, f))
        {
            continue;
        }
        if (!target || level > target->numbers.undead_level ||
            (level == target->numbers.undead_level &&
             distance(turner, f) < distance(turner, *target)))
        {
            target = &f;
        }
    }
    return target;
}

// Treasures' turning (its cbtacts.pas, CombatTurnUndead): its column is the
// cleric level or the paladin level less 2, whichever is more, 9 from 9 to
// 13 and 10 from 14. One d20 is rolled against the strongest undead in sight
// first; each it beats is turned or destroyed, up to 1d12, or 1d6+6 when
// the first is destroyed, destruction going on to at least six; a roll it
// fails sends it to the weaker undead. The turner's actions end. (The area's
// turning penalty, its cell 0Fh, no Treasures script sets.)
void Session::treasures_turn_undead(Fighter &turner)
{
    ++turner.turn_attempts;
    log({"{name} turns undead.", {{"name", turner.name}}});
    const auto &levels = turner.numbers.class_levels;
    int column = std::max(levels[static_cast<std::size_t>(ClassSlot::cleric)],
                          levels[static_cast<std::size_t>(ClassSlot::paladin)] - 2);
    if (column > 8)
    {
        column = column < 14 ? 9 : 10;
    }
    column = std::max(column, 0);
    constexpr int strongest = 13;
    int highest_level = strongest, at_least = 6, left = -1;
    const int natural = roll(20);
    bool any = false;
    while (left != 0)
    {
        auto *target = strongest_undead(turner, highest_level);
        if (!target)
        {
            break;
        }
        const int need =
            treasures_turning_table[static_cast<std::size_t>(target->numbers.undead_level - 1)]
                                   [static_cast<std::size_t>(column)];
        if (left < 0)
        {
            left = need < 0 ? roll(6) + 6 : roll(12);
        }
        if (natural < std::abs(need))
        {
            --highest_level;
            continue;
        }
        any = true;
        if (need > 0)
        {
            target->turned = true;
            log({"{name} is turned.", {{"name", target->name}}});
        }
        else
        {
            target->condition = Condition::gone;
            target->hit_points = 0;
            target->initiative = 0;
            log({"{name} is destroyed.", {{"name", target->name}}});
            end_effects_at_zero(*target);
        }
        at_least = std::max(at_least - 1, 0);
        --left;
        if (left == 0 && at_least > 0 && need < 0)
        {
            left = 1;
        }
    }
    if (!any)
    {
        log({"Nothing happens.", {}});
    }
    end_actions(turner);
}

// Killed outright, whatever its Hit Points.
void Session::kill(Fighter &victim, std::string_view message)
{
    if (victim.has(effect::aurak_invulnerable))
    {
        return;
    }
    victim.condition = Condition::dead;
    victim.hit_points = 0;
    victim.initiative = 0;
    log({std::string(message), {{"name", victim.name}}});
    end_effects_at_zero(victim);
    went_down(victim);
}

// Both claws hit: two rakes of 2d4 with the rear claws, then its actions end
// (cbtacts.pas, RearClawRake).
void Session::rake(Fighter &attacker, Fighter &victim)
{
    auto &rear = attacker.numbers.attacks[1];
    const auto own = rear;
    rear = {own.rate_per_two_rounds, 2, 4, own.modifier};
    attacker.attacks_left = {0, 2};
    attacker.attack_phase = 1;
    log({"{attacker} rakes {name}.", {{"attacker", attacker.name}, {"name", victim.name}}});
    make_attack(attacker, victim, false);
    rear = own;
    attacker.attacks_left = {0, 0};
    end_actions(attacker);
}

// At its turn's start a creature holding a victim bites it from behind with
// 3d4, and a stirge drinks 1d4 from its victim; either spends its turn. A
// stirge that has drunk its fill lets go and flies off (effhndlr.pas, 74 and 75).
void Session::attached_turn(Fighter &f)
{
    // One engulfed suffocates when its count runs out (Curse, Suffocates).
    if (auto *air = f.find_effect(effect::suffocating); air && f.able() && --air->strength <= 0)
    {
        log({"{name} suffocates.", {{"name", f.name}}});
        f.remove(effect::suffocating);
        kill(f);
        return;
    }

    // A shambling mound crushes its engulfed victim with two blows of 2d8;
    // an owlbear squeezes its hugged one with one (Curse; its owlbear's
    // squeeze was never called, docs/ORIGINAL-RULES.md, Bugs).
    // Gateway's owlbear squeezes with two (its BiteAndHold), its stirge
    // drinks with two blows of 1d4, and its squid squeezes with two of 1d2
    // (its 156 was never called, docs/ORIGINAL-RULES.md, Bugs).
    struct Squeeze
    {
        int code, blows, sides;
    };

    for (const auto [code, blows, sides] :
         {Squeeze{effect::engulfing, 2, 8}, Squeeze{effect::hugging, gateway() ? 2 : 1, 8},
          Squeeze{effect::drinking_blood, 2, 4}, Squeeze{effect::constricting, 2, 2}})
    {
        const auto *hold = f.find_effect(code);
        if (!hold)
        {
            continue;
        }
        auto *victim = find(static_cast<EntityId>(hold->strength));
        if (!f.able() || !victim || !victim->able())
        {
            release(f, code);
            continue;
        }
        auto &blow = f.numbers.attacks[0];
        const auto own = blow;
        blow = {own.rate_per_two_rounds, sides == 8 ? 2 : 1, sides, 0};
        f.attacks_left = {blows, 0};
        f.attack_phase = 0;
        make_attack(f, *victim, false);
        blow = own;
        end_actions(f);
        if (!victim->able())
        {
            release(f, code);
        }
        return;
    }
    // A Treasures kampfult crushes its victim for 5, a yeti squeezes for
    // 2d8, 4d5 or 6d4 (EffKampfultCrush, T20GrabHelper).
    if (const auto *hold = f.find_effect(effect::squeezing))
    {
        auto *victim = find(static_cast<EntityId>(hold->strength));
        if (!f.able() || !victim || !victim->able())
        {
            release(f, effect::squeezing);
            return;
        }
        const auto dice = [this](int count, int sides)
        {
            int total = 0;
            for (int die = 0; die < count; ++die)
            {
                total += roll(sides);
            }
            return total;
        };
        const int damage = f.has(effect::crushes)     ? 5
                           : f.has(effect::grabs_6d4) ? dice(6, 4)
                           : f.has(effect::grabs_4d5) ? dice(4, 5)
                                                      : dice(2, 8);
        log({"{attacker} squeezes {name} for {damage}.",
             {{"attacker", f.name}, {"name", victim->name}, {"damage", std::to_string(damage)}}});
        inflict(*victim, damage);
        end_actions(f);
        if (!victim->able())
        {
            release(f, effect::squeezing);
        }
        return;
    }
    if (const auto *hold = f.find_effect(effect::bite_hold))
    {
        auto *victim = find(static_cast<EntityId>(hold->strength));
        if (!f.able() || !victim || !victim->able())
        {
            release(f, effect::bite_hold);
        }
        else
        {
            auto &bite = f.numbers.attacks[0];
            const auto own = bite;
            bite = {own.rate_per_two_rounds, 3, 4, own.modifier};
            f.attacks_left = {1, 0};
            f.attack_phase = 0;
            make_attack(f, *victim, true);
            bite = own;
            end_actions(f);
            if (!victim->able())
            {
                release(f, effect::bite_hold);
            }
        }
    }
    if (const auto *drain = f.find_effect(effect::blood_drain))
    {
        auto *victim = find(static_cast<EntityId>(drain->strength));
        if (!f.able() || !victim || !victim->able())
        {
            release(f, effect::blood_drain);
            return;
        }
        const int damage = roll(4);
        log({"{attacker} drinks {name}'s blood for {damage}.",
             {{"attacker", f.name}, {"name", victim->name}, {"damage", std::to_string(damage)}}});
        inflict(*victim, damage);
        end_actions(f);
        auto *left = f.find_effect(effect::blood_drain);
        if (left && victim->able() && left->minutes / 16 > damage)
        {
            left->minutes -= damage * 16;
            return;
        }
        release(f, effect::blood_drain);
        if (f.able() && victim->able())
        {
            f.turned = true;
        }
    }
}

// An attack on a helpless creature is one blow of its Hit Points and 5 more,
// which leaves it dying 5 below 0; the attacker's attacks are spent
// (cbtacts.pas, RollAttacks).
void Session::slay_helpless(Fighter &attacker, Fighter &defender)
{
    log({"{attacker} slays the helpless {name}.",
         {{"attacker", attacker.name}, {"name", defender.name}}});
    inflict(defender, defender.hit_points + 5);
    attacker.attacks_left = {0, 0};
    attacker.sweep_budget = 0;
    end_actions(attacker);
}

// A round is a minute: every creature's timed effects age by one, and those
// with a minute or less left end (fxengine.pas, the clock's advance).
void Session::age_effects()
{
    for (auto &f : fighters_)
    {
        auto &effects = f.numbers.effects;
        std::vector<Effect> ended;
        std::erase_if(effects,
                      [&ended](const Effect &e)
                      {
                          if (e.minutes == 1)
                          {
                              ended.push_back(e);
                          }
                          return e.minutes == 1;
                      });
        for (auto &e : effects)
        {
            e.minutes = e.minutes > 1 ? e.minutes - 1 : e.minutes;
        }
        for (const auto &e : ended)
        {
            expire(f, e);
        }
    }
}

// An effect whose time ran out, by its handler (effhndlr.pas): a troll's
// regeneration starts 3 rounds after its first wound; one fighting on past
// its death falls dead; a troll comes back with all its Hit Points when its
// square is free, or tries again a minute later.
void Session::expire(Fighter &f, const Effect &ended)
{
    switch (ended.code)
    {
    case dq_effect::green_fire:
        if (f.downed())
        {
            kill(f, "{name} is engulfed by green fire!");
        }
        else if (f.able() && f.hit_points < f.numbers.max_hit_points)
        {
            const int damage = dark_queen_green_fire_damage(roll(8), f.numbers.effects, random_);
            inflict(f, damage);
            if (f.hit_points <= 0)
            {
                kill(f, "{name} is engulfed by green fire!");
            }
            else
            {
                f.numbers.effects.push_back({dq_effect::green_fire, 1, 255, true});
            }
        }
        break;
    case dq_effect::spore_infection:
        kill(f, "{name} dies from disease.");
        break;
    case dq_effect::rot_recovery:
        if (!f.able() && !f.downed())
        {
            break;
        }
        f.numbers.effects.push_back({dq_effect::rot_recovery, 14400, ended.strength, true});
        f.hit_points = std::min(f.hit_points + 1, f.numbers.max_hit_points);
        break;
    case effect::acid_pool_timer:
        std::erase_if(clouds_,
                      [&f](const Cloud &cloud)
                      {
                          return cloud.caster == f.id && cloud.kind == CloudKind::acid;
                      });
        log({"The acid drains away.", {}});
        break;
    case effect::aurak_transformation_timer:
        aurak_final_form(f, false);
        break;
    case effect::aurak_explosion:
        draconian_explosion(f, 3, true);
        break;
    case effect::delayed_blast:
        delayed_blast_goes_off(f, ended.strength);
        break;
    case effect::enlarged:
    case effect::strengthened:
        recount_strength(f);
        break;
    case 113:
        if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
        {
            recount_strength(f);
        }
        break;
    case 342:
        // Fear ends: it stops fleeing and is the player's again (Curse, AffectFear).
        f.fleeing = false;
        // Treasures' fear also ends the flight its yetis' surprise forced, and
        // Pools of Darkness's the flight its Fear forced (their effhndlr.pas,
        // EffFear, as the effect is removed).
        if (treasures() || darkness())
        {
            f.turned = false;
        }
        break;
    case 337:
        // Berserk ends: it is back on its own side.
        f.side = f.own_side;
        break;
    case effect::regenerating_soon:
        f.numbers.effects.push_back({effect::regeneration, 0, 0, false});
        break;
    case effect::fighting_on:
        if (f.able())
        {
            f.condition = Condition::dead;
            f.hit_points = 0;
            f.initiative = 0;
            log({"{name} falls dead.", {{"name", f.name}}});
        }
        break;
    case effect::slow_poison_tick:
        // Slow Poison costs a point every 10 minutes, never the last
        // (effhndlr.pas, EffSlowPoisonHit).
        if (f.hit_points > 1)
        {
            --f.hit_points;
        }
        f.numbers.effects.push_back({effect::slow_poison_tick, 10, 255, true});
        break;
    case effect::slow_poison:
        // Still poisoned when it wears off, it dies (EffSlowPoisonEnd).
        f.remove(effect::slow_poison_tick);
        if (f.has(effect::poisoned) && f.condition != Condition::dead)
        {
            log({"{name} dies from poison.", {{"name", f.name}}});
            kill(f);
        }
        break;
    case effect::revival:
        revive(f);
        break;
    case effect::stinking_cloud:
    case effect::cloudkill:
    case effect::blade_barrier:
    {
        // The caster's cloud of this kind and number clears (effhndlr.pas,
        // EffClearStinkingCloud; Curse's EffClearCloudkill).
        const auto kind = ended.code == effect::cloudkill       ? CloudKind::deadly
                          : ended.code == effect::blade_barrier ? CloudKind::blades
                                                                : CloudKind::stinking;
        const auto cloud = std::find_if(clouds_.begin(), clouds_.end(),
                                        [&](const Cloud &c)
                                        {
                                            return c.caster == f.id && c.kind == kind &&
                                                   c.index == ended.strength / 16;
                                        });
        if (cloud != clouds_.end())
        {
            clouds_.erase(cloud);
            log({"The air clears a little.", {}});
        }
        break;
    }
    case effect::troll_return:
        if (f.condition == Condition::okay)
        {
            break;
        }
        if (cell_free(f))
        {
            stand_up(f, f.numbers.max_hit_points, "{name} rises again.");
        }
        else
        {
            f.numbers.effects.push_back({effect::troll_return, 1, 0, true});
        }
        break;
    default:
        break;
    }
}

// At a fight's start (combatst.pas, COMBAT_START): one that fights on while
// down may stand; a mummy's fear grips its enemies once; displacement is
// ready; camouflage and a ring of invisibility hide; a dragon frightens.
void Session::combat_start()
{
    bool feared = false;
    for (auto &f : fighters_)
    {
        // Native raw151 has a registered handler but no resident trigger;
        // keep its inherent metadata instead of starting the older game's counter.
        if (champions() && game_of(identity_) != GoldBoxGame::dark_queen_of_krynn)
        {
            if (auto *power = f.find_effect(effect::aurak_energy))
            {
                power->strength = 3;
            }
        }
        // What its lasting effects make of its Strength (a Strength spell cast
        // in camp, a disease's wasting).
        if (f.has(effect::enlarged) || f.has(effect::strengthened) ||
            f.has(effect::strength_wasted))
        {
            recount_strength(f);
        }
        // Curse's alias starts every fight invisible (add_affect_19).
        if (f.has(338) && !f.has(effect::invisible))
        {
            f.numbers.effects.push_back({effect::invisible, 255, 255, false});
        }
        // Gateway's fight start fires neither (its condition 8).
        if (f.has(effect::fights_while_down) && !gateway())
        {
            went_down(f);
        }
        if (f.has(effect::mummy_fear) && !feared)
        {
            feared = true;
            // The first mummy's fear is the only one: every mummy loses its own.
            for (auto &other : fighters_)
            {
                other.remove(effect::mummy_fear);
            }
            const auto enemies = std::count_if(fighters_.begin(), fighters_.end(),
                                               [&f](const Fighter &other)
                                               {
                                                   return other.side != f.side;
                                               });
            for (auto &other : fighters_)
            {
                if (other.side == f.side || !other.able())
                {
                    continue;
                }
                // Paralysis by fear: saved against with +1 for a side of 6
                // or more and +2 for a human; immunity to paralysis spares
                // (effhndlr.pas, EffMummyFear).
                const int modifier = (enemies >= 6 ? 1 : 0) + (other.numbers.race == 7 ? 2 : 0);
                if (other.has(109) || other.has(111) || other.has(125) ||
                    saving_throw(other, 0, modifier))
                {
                    log({"{name} is unaffected.", {{"name", other.name}}});
                    continue;
                }
                other.numbers.effects.push_back({effect::paralysis, roll(4), 12, false});
                log({"{name} is paralyzed with fear.", {{"name", other.name}}});
            }
        }
        if (auto *displacement = f.find_effect(effect::displacement))
        {
            displacement->strength &= ~effect::displaced;
        }
        if (f.has(effect::camouflage) && roll(100) <= 95)
        {
            f.numbers.effects.push_back({effect::invisible, 1, 12, false});
        }
        // A ring of invisibility hides its wearer for the round (effhndlr.pas,
        // EffRenewInvisibility): Secret's Dreadlord starts every fight unseen.
        // Gateway's ring first hides at the round's end.
        if (f.has(effect::renew_invisibility) && !gateway())
        {
            f.numbers.effects.push_back({effect::invisible, 1, 12, false});
        }
        if (champions() && f.has(effect::champions_dragon_fear) && f.able())
        {
            for (auto &other : fighters_)
            {
                if (other.side == f.side || !other.able() || other.has(effect::fearless) ||
                    other.has(effect::fear) || other.has(effect::champions_afraid))
                {
                    continue;
                }
                if (other.numbers.level <= 3)
                {
                    if (add_with_message(other, {effect::fear, 0, 255, true}, false, false, 0,
                                         f.numbers.level, game_spell(184)))
                    {
                        other.turned = true;
                    }
                }
                else if (!saving_throw(other, 4, 0, &f))
                {
                    other.numbers.effects.push_back({effect::champions_afraid, 0, 255, false});
                    log({"{name} is afraid.", {{"name", other.name}}});
                }
            }
        }
        // A Secret dragon's fear grips the weaker of its enemies (EffDragonFear).
        if (f.has(effect::dragon_fear) && f.able())
        {
            dragon_fear(f);
        }
        // A Pools of Darkness dragon's, tried by both of the original's
        // conditions of a fight's start (combatst.pas, CombatStart, and
        // combat.pas, CombatMain), so one that saves once faces it again.
        if (f.has(effect::darkness_dragon_fear) && f.able())
        {
            darkness_dragon_fear(f);
            darkness_dragon_fear(f);
        }
        if (frightening_surprise_ && f.has(effect::frightens_on_surprise))
        {
            frighten_on_surprise(f);
        }
    }
}

// A Treasures yeti's fear when it surprises (effhndlr.pas, EffFrightenAura,
// as a fight starts with the script's cell 53h set): each able enemy that
// fails a save against petrification, unless afraid already, is frightened
// and flees.
void Session::frighten_on_surprise(const Fighter &yeti)
{
    for (auto &f : fighters_)
    {
        if (f.side == yeti.side || !f.able() || saving_throw(f, 1, 0, &yeti) || f.has(effect::fear))
        {
            continue;
        }
        f.numbers.effects.push_back({effect::fear, 2, 255, true});
        f.turned = true;
        log({"{name} is frightened.", {{"name", f.name}}});
    }
}

// At a round's end, after effects age (combat.pas, EndOfRound): regeneration
// heals 3; camouflage hides again, 95 times in 100, and a ring of invisibility
// every time; displacement is ready for the next round.
void Session::round_end_effects()
{
    for (auto &f : fighters_)
    {
        if (f.has(effect::regeneration))
        {
            f.hit_points = std::min(f.hit_points + 3, f.numbers.max_hit_points);
        }
        // Gateway's Vaalgamon regenerates 1 (EffRegenerateOne).
        const bool native_regeneration =
            game_of(identity_) != GoldBoxGame::dark_queen_of_krynn ||
            std::ranges::any_of(fighters_,
                                [](const Fighter &other)
                                {
                                    return other.side == rules::Side::opposition && other.able();
                                });
        if (f.has(effect::regenerates_one) && native_regeneration &&
            f.hit_points < f.numbers.max_hit_points)
        {
            f.hit_points = std::min(f.hit_points + 1, f.numbers.max_hit_points);
        }
        if (f.has(effect::camouflage) && roll(100) <= 95)
        {
            f.numbers.effects.push_back({effect::invisible, 1, 12, false});
        }
        if (f.has(effect::renew_invisibility))
        {
            f.numbers.effects.push_back({effect::invisible, 1, 12, false});
        }
        if (auto *displacement = f.find_effect(effect::displacement))
        {
            displacement->strength &= ~effect::displaced;
        }
    }
}

// A creature just gone down (fxengine.pas, HIT_POINTS_ZERO): one that fights
// on while down stands again, dying less than 6 below 0 or unconscious, with
// 6 less its wounds, for 1d4+1 rounds; a troll downed by anything but fire or
// acid (any weapon) comes back in 3d6 rounds.
void Session::went_down(Fighter &f)
{
    if (f.has(dq_effect::spore_burst))
    {
        f.remove(dq_effect::spore_burst);
        if (!f.has(2194))
        {
            log({"{name} explodes.", {{"name", f.name}}});
            for (const auto id : within(f.cell, 2))
            {
                auto &other = *find(id);
                if (other.id == f.id || !other.able() || other.has(2183))
                {
                    continue;
                }
                const bool saved = saving_throw(other, 2, 0, &f);
                int damage = 0;
                for (int die = 0; die < 6; ++die)
                {
                    damage += roll(6);
                }
                nonweapon_damage(other, damage, 2, saved, 0, 0, 0, false);
            }
        }
    }
    if (f.has(dq_effect::ice_explosion))
    {
        f.remove(dq_effect::ice_explosion);
        log({"{name} explodes.", {{"name", f.name}}});
        const int damage = roll(10) + roll(10) + roll(10);
        for (const auto id : within(f.cell, 2))
        {
            auto &other = *find(id);
            if (other.id != f.id && other.able())
            {
                nonweapon_damage(other, damage, 0, false, 8, 12, 187, false);
            }
        }
    }
    if (f.has(dq_effect::meteor_explosion))
    {
        f.remove(dq_effect::meteor_explosion);
        log({"{name} explodes.", {{"name", f.name}}});
        cast_meteor_swarm(f, 12, f.cell);
    }

    if (champions())
    {
        if (f.has(effect::baaz_disarm))
        {
            auto *attacker = find(striker_);
            if (attacker && attacker->record && distance(*attacker, f) <= 1 &&
                roll(20) >= attacker->record->current_abilities.dexterity - 3)
            {
                const auto held = held_now(*attacker);
                const auto weapon =
                    std::ranges::find_if(held,
                                         [](const Held &item)
                                         {
                                             return !item.gone && !item.conjured &&
                                                    item.carried.item.readied() &&
                                                    item.carried.item.cursed_raw == 0 &&
                                                    item.carried.type.worn_location == 0;
                                         });
                if (weapon != held.end())
                {
                    const auto at = static_cast<std::size_t>(weapon - held.begin());
                    toggle_readied(*attacker, at);
                    attacker->numbers.effects.push_back(
                        {effect::baaz_banked_weapon, 0, static_cast<int>(at + 1), false});
                    log({"{name} loses a weapon in the Baaz's stone body.",
                         {{"name", attacker->name}}});
                }
            }
        }
        if (f.has(effect::kapak_acid))
        {
            f.remove(effect::kapak_acid);
            Cloud pool;
            pool.caster = f.id;
            pool.corner = f.cell;
            pool.covers[0] = true;
            pool.kind = CloudKind::acid;
            clouds_.push_back(pool);
            put_out_of_play(f, Condition::gone);
            f.numbers.effects.push_back({effect::acid_pool_timer, roll(6), 0, true});
            log({"{name} dissolves into acid.", {{"name", f.name}}});
            return;
        }
        if (f.has(effect::aurak_madness))
        {
            f.remove(effect::aurak_madness);
            stand_up(f, 20, "{name} rises in a frenzy.");
            f.numbers.spells.clear();
            f.numbers.effects.push_back({effect::aurak_transformation, 0, 255, false});
            f.numbers.effects.push_back({effect::aurak_immolation, 0, 255, false});
            f.numbers.effects.push_back({effect::aurak_combat_bonus, 0, 255, false});
            f.numbers.effects.push_back({effect::aurak_transformation_timer, 6, 255, true});
            return;
        }
        if (f.has(effect::aurak_transformation))
        {
            aurak_final_form(f, true);
            return;
        }
        if (f.has(effect::bozak_explosion))
        {
            draconian_explosion(f, 1, false);
            return;
        }
    }
    if (f.has(effect::fights_while_down) &&
        ((f.condition == Condition::dying && f.overkill < 6) ||
         f.condition == Condition::unconscious) &&
        cell_free(f))
    {
        const int hit_points = f.condition == Condition::dying ? 6 - f.overkill : 6;
        f.remove(effect::fights_while_down);
        stand_up(f, hit_points, "{name} gets back up.");
        f.numbers.effects.push_back({effect::fighting_on, roll(4) + 1, 0, true});
        return;
    }
    if (f.has(effect::troll_burn) && !f.able() && !f.has(effect::troll_return))
    {
        f.numbers.effects.push_back({effect::troll_return, roll(6) + roll(6) + roll(6), 0, true});
    }
}

bool Session::cell_free(const Fighter &f) const
{
    const auto mine = squares_of(f);
    return std::none_of(fighters_.begin(), fighters_.end(),
                        [&](const Fighter &other)
                        {
                            if (other.id == f.id || other.condition == Condition::dead ||
                                other.condition == Condition::fled ||
                                other.condition == Condition::surrendered ||
                                other.condition == Condition::gone)
                            {
                                return false;
                            }
                            const auto theirs = squares_of(other);
                            return std::any_of(mine.begin(), mine.end(),
                                               [&theirs](Cell c)
                                               {
                                                   return std::find(theirs.begin(), theirs.end(),
                                                                    c) != theirs.end();
                                               });
                        });
}

// Back on its feet, without turns this round (its initiative is spent).
void Session::stand_up(Fighter &f, int hit_points, std::string_view message)
{
    f.condition = Condition::okay;
    f.hit_points = hit_points;
    f.overkill = 0;
    f.initiative = 0;
    log({std::string(message), {{"name", f.name}}});
}

// Whether the attacker may single out the target (cbtacts.pas, CanTarget): not
// an invisible one unless it sees the invisible, not a blinking one before it
// has acted this round, and nothing that holds up a mirror or holy symbol to a
// gazing monster.
bool Session::can_target(const Fighter &attacker, const Fighter &target) const
{
    if (target.has(effect::invisible) && !attacker.has(effect::see_invisible))
    {
        return false;
    }
    if (target.has(effect::blink) && target.initiative > 0)
    {
        return false;
    }
    // Curse's blink hides it once it has acted (AffectBlink in its reconstruction).
    if (target.has(237) && target.initiative <= 0)
    {
        return false;
    }
    // Invisibility to animals: an animal that cannot see the invisible
    // cannot single it out (Curse, AffectInvisToAnimals).
    if (target.has(269) && attacker.numbers.creature_type == 19 &&
        !attacker.has(effect::see_invisible))
    {
        return false;
    }
    // Dust of Disappearance hides it even from one that sees the invisible
    // (effhndlr.pas, EffDustDisappear).
    if (target.has(effect::dust))
    {
        return false;
    }
    return !(attacker.has(effect::gaze_block) && target.numbers.wards_gaze);
}

// An attack roll a defender turns aside, even a natural 20 (fxengine.pas,
// ATTACK_ROLL_DEF): a blinking one before it has acted this round, and a
// displaced one the first attack each round. The original readied
// displacement only at the fight's start, so it turned aside one attack a
// fight; once a round here, as its own notes say (an obvious bug, fixed).
bool Session::turned_aside(Fighter &defender)
{
    if ((defender.has(effect::blink) && defender.initiative > 0) ||
        (defender.has(237) && defender.initiative <= 0))
    {
        return true;
    }
    auto *displacement = defender.find_effect(effect::displacement);
    if (displacement && (displacement->strength & effect::displaced) == 0)
    {
        displacement->strength |= effect::displaced;
        return true;
    }
    return false;
}

// Pools of Darkness's Kalistes strikes the first six attacks of each attack
// she makes with six weapons in turn (effhndlr.pas, EffMultiAttackNewWeapons): each
// blow does its weapon's dice, 4 more and her Strength's bonus, in place of
// what her record's dice did, and names the weapon. Her further attacks, and
// any of her second form, keep their damage, as the original's count of first
// attacks picks no weapon for them.
int Session::six_weapons_blow(const Fighter &attacker, int primary_attacks, int damage)
{
    struct Weapon
    {
        int dice, sides;
        std::string_view name;
    };

    static constexpr std::array<Weapon, 6> weapons{{{1, 10, "Two-Handed Sword +4"},
                                                    {2, 4, "Bastard Sword +4"},
                                                    {1, 8, "Long Sword +4"},
                                                    {1, 8, "Scimitar +4"},
                                                    {1, 4, "Dagger +4"},
                                                    {1, 6, "Spear +4"}}};
    if (primary_attacks < 1 || primary_attacks > static_cast<int>(weapons.size()))
    {
        return damage;
    }
    const auto &weapon = weapons[static_cast<std::size_t>(primary_attacks - 1)];
    int blow =
        4 + strength_damage_bonus(attacker.numbers.strength, attacker.numbers.exceptional_strength);
    for (int die = 0; die < weapon.dice; ++die)
    {
        blow += roll(weapon.sides);
    }
    log({"{attacker} strikes with a {weapon}.",
         {{"attacker", attacker.name}, {"weapon", std::string(weapon.name)}}});
    return std::max(blow, 0);
}

// What the defender's effects make of a weapon's damage, in the original's
// order (effhndlr.pas, WEAPON_DAMAGE_DEFENDER): evading missiles and boulders
// from afar, a troll's regeneration beginning, halving by weapon class or
// magic, weapons that cannot harm it, and oil or holy water.
int Session::weapon_damage_defenses(Fighter &attacker, Fighter &defender, int damage, int phase,
                                    int primary_attacks, int &primary_hits)
{
    if (attacker.has(dq_effect::green_fire_touch) && !saving_throw(defender, 4, 0, &attacker) &&
        !defender.has(dq_effect::green_fire))
    {
        defender.numbers.effects.push_back({dq_effect::green_fire, 1, 255, true});
        log({"{name} is turning to green fire!", {{"name", defender.name}}});
    }
    if (defender.has(dq_effect::anemone_acid) && distance(attacker, defender) <= 1 && damage <= 15)
    {
        log({"{name} is squirted by anemone's acid!", {{"name", attacker.name}}});
        nonweapon_damage(attacker, roll(4) + roll(4) + roll(4), 0, false, 256, 0, 0, false);
    }
    const auto &striker = attacker.numbers.striker;
    const int plus = striker ? striker->plus : 0;
    const bool silver = striker && striker->silver;
    const int weapon_class = striker ? striker->weapon_class : 0;
    const bool piercing = (weapon_class & 0x01) != 0, bludgeoning = (weapon_class & 0x80) != 0;
    const auto evades = [&](int chance)
    {
        if (distance(attacker, defender) <= 1 || roll(100) > chance)
        {
            return false;
        }
        log({"{name} avoids it.", {{"name", defender.name}}});
        // The original took the hit from the primary attacks' count whatever
        // attack it was, wrapping below 0 (an obvious bug, fixed).
        if (phase == 0 && primary_hits > 0)
        {
            --primary_hits;
        }
        return true;
    };
    if (attacker.has(dq_effect::underwater_weapons) && !attacker.has(2150) && striker)
    {
        damage = attacker.numbers.missile ? 0
                 : weapon_class != 0 ? std::min(damage, attacker.numbers.attacks[0].modifier + 1)
                                     : damage;
    }
    // A black pudding's acid spends the blow on the armor it eats (the kit's
    // 186), as the attacker's effects come before the defender's.
    if (attacker.has(effect::eats_armor) && eat_armor(attacker, defender))
    {
        damage = 0;
    }
    // Curse's rakshasa (its effect 130) struck by a banishing quarrel is gone
    // for good (effhndlr.pas, EffBanishByWeapon).
    constexpr int banished_by_quarrel = 330;
    if (defender.has(banished_by_quarrel) && striker && striker->banishing)
    {
        log({"{name} is banished.", {{"name", defender.name}}});
        put_out_of_play(defender, Condition::gone);
        return 0;
    }
    // Pools of Darkness's rakshasa struck by a blessed bolt is slain
    // (effhndlr.pas, EffBlessedBoltKills), as the kit's by its quarrel of
    // slaying (effects.c, fx_slain_by_type).
    if (defender.has(effect::slain_by_blessed_bolt) && striker && striker->blessed_bolt)
    {
        kill(defender, "{name} is slain.");
        return 0;
    }
    // Ray of Enfeeblement takes a quarter of the attacker's damage (effhndlr.pas, EffEnfeeble).
    if (attacker.has(effect::enfeebled))
    {
        damage -= damage / 4;
    }
    if (attacker.has(dq_effect::disruption))
    {
        if (defender.numbers.traits & dq_effect::disruption_double_trait)
        {
            damage *= 2;
        }
        else if ((defender.has(dq_effect::disintegrating_ray) && roll(2) == 1) ||
                 defender.numbers.undead_level > 0)
        {
            log({"{name} is disrupted.", {{"name", defender.name}}});
            put_out_of_play(defender, Condition::gone);
            return 0;
        }
    }
    if (attacker.has(dq_effect::dragon_slayer) &&
        (defender.numbers.traits & champions_dragon_trait))
    {
        damage = defender.has(504)
                     ? damage + 6
                     : roll(12) + roll(12) + roll(12) + 6 +
                           strength_damage_bonus(attacker.numbers.strength,
                                                 attacker.numbers.exceptional_strength);
    }
    if (champions())
    {
        const bool dragon = (defender.numbers.traits & champions_dragon_trait) != 0;
        if (dragon && attacker.has(effect::dragon_slayer))
        {
            // The native handler triples one d12, then adds 4 and Strength.
            damage = 3 * roll(12) + 4 +
                     strength_damage_bonus(attacker.numbers.strength,
                                           attacker.numbers.exceptional_strength);
        }
        if (dragon && attacker.has(effect::dragonlance))
        {
            damage = attacker.hit_points;
        }
        if (attacker.has(effect::reptile_bane) &&
            (defender.numbers.traits & champions_reptile_trait))
        {
            damage += 3;
        }
        if (attacker.has(effect::mace_disruption) && (defender.numbers.traits & trait::unliving))
        {
            if (defender.numbers.undead_level > 0)
            {
                log({"{name} is disrupted.", {{"name", defender.name}}});
                put_out_of_play(defender, Condition::gone);
                return 0;
            }
            damage *= 2;
        }
    }
    // Pools of Darkness's dragon fear takes 2 from a blow (EffDragonFeared).
    if (attacker.has(effect::dragon_feared))
    {
        damage = std::max(damage - 2, 0);
    }
    if (attacker.has(dkk_effect::smoked))
    {
        damage = std::max(0, damage - 2);
    }
    if (attacker.has(effect::six_weapons))
    {
        damage = six_weapons_blow(attacker, primary_attacks, damage);
    }
    // Mirror images take blows too, as the manual says; the original let them
    // take only a spell's (an obvious bug, fixed).
    if (mirror_absorbs(defender))
    {
        damage = 0;
    }
    if (defender.has(104) && attacker.numbers.weapon_type >= 0 && evades(60))
    {
        damage = 0;
    }
    // Protection from Normal Missiles: no harm from a mundane weapon from afar
    // (effhndlr.pas, EffProtNormalMissiles).
    if (defender.has(effect::missile_protection) && striker && plus == 0 &&
        attacker.numbers.weapon_type >= 0 && evades(100))
    {
        damage = 0;
    }
    const int boulder = attacker.numbers.weapon_type;
    if (defender.has(120) && (boulder == 87 || boulder == 88) && evades(50))
    {
        damage = 0;
    }
    if (defender.has(effect::troll_heal) && !defender.has(effect::regeneration) &&
        !defender.has(effect::regenerating_soon))
    {
        defender.numbers.effects.push_back({effect::regenerating_soon, 3, 0, true});
    }
    if (defender.has(115) && striker && (weapon_class == 0 || piercing))
    {
        damage /= 2;
    }
    if (defender.has(116) && plus > 0)
    {
        damage /= 2;
    }
    if (defender.has(119) && plus == 0 && (attacker.numbers.race > 0 || attacker.numbers.level < 4))
    {
        damage = 0;
    }
    // Secret's: an iron golem takes harm only from a weapon of +3 or better,
    // and a giant slug none from a bludgeoning one (effhndlr.pas,
    // EffNeedMagicWeapon and EffHalfBash, in its reconstruction).
    if (defender.has(dkk_effect::needs_plus_three) && (!striker || plus < 3) &&
        (attacker.numbers.race > 0 || attacker.numbers.level < 4))
    {
        damage = 0;
    }
    if (defender.has(effect::needs_plus_three) && (!striker || plus < 3))
    {
        damage = 0;
    }
    if (defender.has(effect::bashing_proof) && striker && bludgeoning)
    {
        damage = 0;
    }
    // Pools of Darkness's crushing weapon does half (effhndlr.pas,
    // EffHalfDmgCrush); its weapons below +3 and +4 and its need of a +2
    // weapon are the kit's, below (EffLuckStone, EffHalfDmgWeaponLt3,
    // EffNeedPlus2).
    if (defender.has(effect::halves_crushing) && striker && bludgeoning)
    {
        damage /= 2;
    }
    // The kit's (effects.c): a weapon below +3 or +4 does half (80, 81); one
    // below +2 nothing, nor a creature's bare blows unless a monster's of 7
    // Hit Dice or more (125); and a black pudding takes nothing from any
    // blow (193).
    if (defender.has(effect::halved_below_plus_three) && striker && plus < 3)
    {
        damage /= 2;
    }
    if (defender.has(effect::halved_below_plus_four) && striker && plus < 4)
    {
        damage /= 2;
    }
    // Darkness's shield returns twice the damage of an adjacent weapon
    // as magic, before the blow lands (effhndlr.pas, EffZapAttacker).
    // A returned magical hit is not another weapon attack, so shields
    // cannot recursively return one another's retaliation.
    if ((darkness() || game_of(identity_) == GoldBoxGame::dark_queen_of_krynn) &&
        defender.has(effect::retaliating_shield) && damage > 0 && distance(attacker, defender) < 2)
    {
        const int returned = damage > std::numeric_limits<int>::max() / 2
                                 ? std::numeric_limits<int>::max()
                                 : damage * 2;
        log({"{name}'s Fire Shield strikes {attacker}.",
             {{"name", defender.name}, {"attacker", attacker.name}}});
        nonweapon_damage(attacker, returned, 0, false, 8, 0, 0, false);
    }
    // The kit's need of a weapon of +n (fx_need_plus): one below it does
    // nothing, nor bare blows but a monster's of (n - 1) * 2 + 5 Hit Dice or
    // more: +1 (103), +2 (125), +3 (60).
    const auto turns_aside = [&](int needed)
    {
        const bool strong_monster =
            attacker.numbers.race == 0 && attacker.numbers.level >= (needed - 1) * 2 + 5;
        return striker ? plus < needed : !strong_monster;
    };
    if ((defender.has(effect::needs_plus_one) && turns_aside(1)) ||
        (defender.has(effect::needs_plus_two) && turns_aside(2)) ||
        (defender.has(effect::needs_plus_three_kit) && turns_aside(3)))
    {
        damage = 0;
    }
    if (defender.has(effect::blow_proof))
    {
        damage = 0;
    }
    if (defender.has(dq_effect::mundane_blows_fail) && distance(attacker, defender) <= 1 &&
        (!striker || plus == 0))
    {
        damage = 0;
    }
    // A Treasures stone golem, while stone, takes nothing from a weapon
    // below +2 (EffSlowAura, LimitDamageByResistance).
    if (const auto *golem = defender.find_effect(effect::stone_golem);
        golem && golem->strength == effect::golem_stone && turns_aside(2))
    {
        damage = 0;
    }
    if (defender.has(123))
    {
        damage = plus > 0 ? damage : silver ? damage / 2 : 0;
    }
    if (defender.has(96) && plus == 0 && !silver)
    {
        damage = 0;
    }
    if (defender.has(94) && striker && (piercing || bludgeoning))
    {
        damage /= 2;
    }
    // Curse's defenses (ovr013): normal weapons do nothing and +1 or +2
    // half (260); every blow does half (281); a piercing one 1 (285).
    // Gateway's holy water harms such a mummy in full (its
    // EffRequirePlusThreeWeapon).
    const bool holy_water_blow = gateway() && attacker.numbers.weapon_type == 85;
    if (defender.has(260) && !holy_water_blow)
    {
        damage = plus == 0 ? 0 : plus < 3 ? damage / 2 : damage;
    }
    if (defender.has(281))
    {
        damage /= 2;
    }
    // Gateway's and Treasures' weapons of Icewind Dale and of stonecutting do
    // double to the creatures they bane, and their glacier armor turns half a
    // fire creature's blow (EffVulnerableToIcewindDale,
    // EffVulnerableToStonecutting, EffGlacierArmor): name words 144, 250 and
    // 137 in Gateway, 127, 129 and 128 in Treasures.
    const bool frontier = gateway() || treasures();
    if (frontier && defender.has(effect::icewind_dale_harms) &&
        weapon_named(attacker, gateway() ? 144 : 127))
    {
        damage *= 2;
    }
    if (frontier && defender.has(effect::stonecutting_harms) &&
        weapon_named(attacker, gateway() ? 250 : 129))
    {
        damage *= 2;
    }
    if (frontier && attacker.has(effect::glacier_halves) &&
        armor_named(defender, gateway() ? 137 : 128))
    {
        damage /= 2;
    }
    // A Treasures stone guardian takes a quarter from an edge or a point,
    // and nothing from a weapon whose type may be thrown, even one swung
    // (EffResistItemTypes). The original reads that type of no item for a
    // bare blow (Bugs).
    if (defender.has(effect::quarter_from_edges) && striker && weapon_class <= 1)
    {
        damage /= 4;
    }
    if (defender.has(effect::quarter_from_edges) && striker && striker->thrown)
    {
        damage = 0;
    }
    if (defender.has(285) && weapon_class == 1)
    {
        damage = std::min(damage, 1);
    }
    // A ranger adds its level against giants (Curse, AffectRangerVsGiant); in
    // Secret and Pools of Darkness against the creatures its flags name, and
    // in the kit those its monster types do, from next to them
    // (EffRangerDamageBonus; the kit's effects.c, 105).
    const bool ranger_foe =
        secret_engine() || kit() || game_of(identity_) == GoldBoxGame::dark_queen_of_krynn
            ? (defender.numbers.traits & trait::ranger_foe) && adjacent(attacker, defender)
            : (defender.numbers.kin & kin::giant) != 0;
    if (attacker.has(334) && ranger_foe && damage > 0)
    {
        damage += attacker.numbers.class_levels[static_cast<std::size_t>(ClassSlot::ranger)];
    }
    // A salamander's heat adds 1d6 unless the target resists fire (303).
    if (attacker.has(303) && damage > 0 && !defender.has(effect::resist_fire) &&
        !defender.has(254) && !defender.has(61) && !defender.has(effect::fire_resistance_ring))
    {
        damage += roll(6);
    }
    // A flask of oil burns a mummy for 3d8; holy water burns the undead for 1d6+1.
    if (defender.has(122) && attacker.numbers.weapon_type == 86)
    {
        damage = roll(8) + roll(8) + roll(8);
    }
    // The kit's holy water is its item type 36h (effects.c,
    // fx_weapon_type36_damage).
    const int holy_water = kit() ? 0x36 : 85;
    if (defender.has(117) && attacker.numbers.weapon_type == holy_water)
    {
        damage = roll(6) + 1;
    }
    if (defender.has(dq_effect::disarming_body) && attacker.record &&
        distance(attacker, defender) <= 1 && damage > 0 && damage % 3 == 0 &&
        roll(20) >= attacker.record->current_abilities.dexterity - 3)
    {
        const auto held = held_now(attacker);
        const auto weapon = std::ranges::find_if(held,
                                                 [](const Held &item)
                                                 {
                                                     return !item.gone && !item.conjured &&
                                                            item.carried.item.readied() &&
                                                            item.carried.item.cursed_raw == 0 &&
                                                            item.carried.type.worn_location == 0;
                                                 });
        if (weapon != held.end() &&
            !(weapon->carried.item.type == 6 && weapon->carried.item.name_components[2] == 121))
        {
            const auto at = static_cast<std::size_t>(weapon - held.begin());
            toggle_readied(attacker, at);
            attacker.numbers.effects.push_back(
                {effect::baaz_banked_weapon, 0, static_cast<int>(at + 1), false});
            log({"{name} loses a weapon.", {{"name", attacker.name}}});
        }
    }
    return damage;
}

// A black pudding's blow eats into the armor its target has readied (the
// kit's effects.c, 186): a plus at a time, banded and plate mail (the kit's
// item types 23h and 24h) to -1 and other armor to 0, then dissolves it.
// Whether there was armor to eat, which takes the blow's harm.
bool Session::eat_armor(const Fighter &pudding, Fighter &target)
{
    constexpr std::uint8_t armor_slot = 2, banded_mail = 0x23, plate_mail = 0x24;
    if (!target.record)
    {
        return false;
    }
    const auto before = held_now(target);
    auto after = before;
    bool eaten = false;
    for (auto &held : after)
    {
        auto &armor = held.carried.item;
        if (held.gone || !armor.readied() || held.carried.type.worn_location != armor_slot)
        {
            continue;
        }
        eaten = true;
        const int lowest = armor.type == banded_mail || armor.type == plate_mail ? -1 : 0;
        if (armor.magic_bonus > lowest)
        {
            --armor.magic_bonus;
            armor.raw[50] = static_cast<std::uint8_t>(armor.magic_bonus);
            log({"{name} is dissolving {target}'s armor!",
                 {{"name", pudding.name}, {"target", target.name}}});
        }
        else
        {
            held.gone = true;
            log({"{name} dissolves {target}'s armor!",
                 {{"name", pudding.name}, {"target", target.name}}});
        }
    }
    if (eaten)
    {
        rework_gear(target, before, std::move(after));
        if (!target.has(effect::armor_eaten))
        {
            target.numbers.effects.push_back({effect::armor_eaten, 0, 0, false});
        }
    }
    return eaten;
}

// A saving throw (fxengine.pas, RollSavingThrow): its natural roll
// (save_die), judged by saves_with.
bool Session::saving_throw(const Fighter &f, int save, int modifier, const Fighter *caster,
                           int element)
{
    auto dice = fight_dice();
    return saves_with(f, save_die(f, dice), save, modifier, caster, element);
}

// The chance a saving throw saves, each way its natural roll falls weighed.
double Session::save_chance(const Fighter &f, int save, int modifier, const Fighter *caster,
                            int element) const
{
    double chance = 0;
    each_way<bool>(
        [&](auto &&draw)
        {
            return saves_with(f, save_die(f, draw), save, modifier, caster, element);
        },
        [&](double way, bool saved)
        {
            chance += saved ? way : 0;
        });
    return chance;
}

// Whether a saving throw of this natural roll saves: a natural 1 fails and a
// natural 20 saves; otherwise the roll with the creature's save bonus and the
// modifier must reach its number for the save. A total below 0 fails, where
// the original's byte wrapped it to a success (an obvious bug, fixed).
bool Session::saves_with(const Fighter &f, int natural, int save, int modifier,
                         const Fighter *caster, int element) const
{
    if (natural == 1 || natural == 20)
    {
        return natural == 20;
    }
    // Immunity to paralysis and poison makes those saves (effhndlr.pas, 111 and 125).
    if (save == 0 && (f.has(111) || f.has(125)))
    {
        return true;
    }
    // The saver's effects (effhndlr.pas, SAVING_THROW): Constitution for a
    // dwarf, gnome or halfling against poison, spells and wands; Shield;
    // protection against a caster of that alignment; and resistance to the
    // cold or fire the spell carries.
    const int constitution = f.numbers.constitution;
    const int hardiness = constitution < 4     ? 0
                          : constitution <= 6  ? 1
                          : constitution <= 10 ? 2
                          : constitution <= 13 ? 3
                          : constitution <= 17 ? 4
                          : constitution <= 20 ? 5
                                               : 0;
    // Secret's 94 counts Constitution against all three (EffConPsnSave).
    const bool poison_spell_or_wand = save == 0 || save == 2 || save == 4;
    if ((f.has(90) && save == 0) || (f.has(97) && (save == 4 || save == 2)) ||
        (f.has(effect::hardy) && poison_spell_or_wand))
    {
        modifier += hardiness;
    }
    // Secret's luckstone saves 1 better (EffSaveBonus). Dark Queen's bonuses
    // are those of its trigger group 0Ch (fxcore.c, fx_trigger_group): its
    // magic stone, which shares the luckstone's number, is not among them,
    // and its Barkskin saves 1 better against what is not magic
    // (effects.c, fx_barkskin).
    const bool native = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn;
    modifier += f.has(effect::luck) && !native ? 1 : 0;
    modifier +=
        native && !(element & 8) && f.has(canonical_effect(13, GoldBoxGame::dark_queen_of_krynn))
            ? 1
            : 0;
    modifier += f.has(effect::shield) ? 1 : 0;
    modifier -= f.has(effect::blind) ? 4 : 0;
    modifier -= f.has(effect::bestow_curse) ? 4 : 0;
    modifier += prayer_bonus(f);
    if (caster)
    {
        const int alignment = caster->numbers.alignment;
        const bool evil = alignment == 2 || alignment == 5 || alignment == 8;
        const bool good = alignment == 0 || alignment == 3 || alignment == 6;
        if ((evil &&
             protected_from(f, effect::protection_from_evil, effect::protection_from_evil_aura)) ||
            (good &&
             protected_from(f, effect::protection_from_good, effect::protection_from_good_aura)))
        {
            modifier += 2;
        }
    }
    if (((element & 2) && f.has(effect::resist_cold)) ||
        ((element & 1) && f.has(effect::resist_fire)))
    {
        modifier += 3;
    }
    modifier += (element & 1) && f.has(effect::fire_resistance_ring) ? 4 : 0;
    modifier += (element & 1) && f.has(dq_effect::resists_fire) ? 4 : 0;
    if ((element & 16) && f.has(dq_effect::mask_16_save_bonus))
    {
        modifier = 20;
    }
    // Curse's fire shields: the hot one saves 2 better against cold, the
    // cold one against fire (HotFireShield, ColdFireShield).
    if (((element & 2) && f.has(250)) || ((element & 1) && f.has(254)))
    {
        modifier += 2;
    }
    // Dark Queen's mage of a robe saves 1 worse in its moon's phase 0 and 1
    // better in phase 2 (fxcore.c, fx_saving_throw), the phases that also
    // move its caster level.
    if (native)
    {
        if (const auto *moon = f.find_effect(dkk_effect::casting_moon))
        {
            const int phase = moon->strength % 4;
            modifier += phase == 0 ? -1 : phase == 2 ? 1 : 0;
        }
    }
    const int total = natural + f.numbers.save_bonus + modifier;
    return total >= 0 && total >= f.numbers.saving_throws[static_cast<std::size_t>(save)];
}

// The opposition's Hit Points left, those of the creatures still fighting
// against the whole of every one's, rounded down to a step of 5.
void Session::update_hostile_hit_points()
{
    int left = 0, whole = 0;
    for (const auto &f : fighters_)
    {
        if (f.side != Side::opposition)
        {
            continue;
        }
        left += f.able() ? f.hit_points : 0;
        whole += f.numbers.max_hit_points;
    }
    if (whole > 0)
    {
        hostile_hit_points_percent_ = left * 20 / whole * 5;
    }
}

int Session::highest_enemy_movement(const Fighter &f) const
{
    int highest = 0;
    for (const auto &other : fighters_)
    {
        if (other.side != f.side && other.able())
        {
            highest = std::max(highest, movement_of(other));
        }
    }
    return highest;
}

// A creature whose morale counts checks it as its turn starts (combatai.pas,
// CheckMorale): it holds while its morale (twice the byte's low 7 bits, 0
// past 102) is at least the share of its Hit Points it has lost; an enemy of
// the party also holds while the opposition's Hit Points left are at least
// 100 less the encounter's morale. Broken, it runs when no enemy is faster,
// gives up when its Intelligence is 6 or more, and otherwise fights on. It
// may rally at its next turn. True when it surrendered.
bool Session::check_morale(Fighter &f)
{
    f.fleeing = false;
    // Gateway's black dragon gives up below 20 Hit Points when the last of
    // its side (effhndlr.pas, EffSurrender, at MORALE_CHECK).
    if (f.has(effect::surrenders_when_last) && f.hit_points < 20 &&
        std::ranges::none_of(
            fighters_,
            [&f](const Fighter &other)
            {
                return other.id != f.id && other.side == f.side && other.able();
            }))
    {
        put_out_of_play(f, Condition::surrendered);
        log({"{name} surrenders.", {{"name", f.name}}});
        return true;
    }
    release(f, effect::bite_hold);
    release(f, effect::blood_drain);
    release(f, effect::drinking_blood);
    release(f, effect::constricting);
    release(f, effect::squeezing);
    if (f.turned)
    {
        f.fleeing = true;
        log({"{name} is forced to flee.", {{"name", f.name}}});
        return false;
    }
    if (f.numbers.morale < non_player)
    {
        return false;
    }
    // A charmed creature never breaks (effhndlr.pas, the charm at MORALE_CHECK).
    if (f.has(effect::charm))
    {
        return false;
    }
    int morale = static_cast<int>(f.numbers.morale & 0x7F) * 2;
    if (morale > 102)
    {
        morale = 0;
    }
    // Bless adds 5 and Curse takes 5, never below 0, which fails (EffBless, EffCurse).
    const auto blessed = [&f](int value)
    {
        value += f.has(effect::bless) ? 5 : 0;
        return f.has(effect::curse) ? std::max(value - 5, 0) : value;
    };
    morale = morale == 0 ? 0 : blessed(morale);
    if (morale != 0 && morale >= 100 - f.hit_points * 100 / f.numbers.max_hit_points)
    {
        return false;
    }
    const int group = hostile_hit_points_percent_ == 0 ? 0 : blessed(hostile_hit_points_percent_);
    if (group != 0 && group >= 100 - static_cast<int>(morale_threshold_) &&
        f.side == Side::opposition)
    {
        return false;
    }
    if (highest_enemy_movement(f) <= movement_of(f))
    {
        f.fleeing = true;
        log({"{name} flees in panic.", {{"name", f.name}}});
        return false;
    }
    if (f.numbers.intelligence > 5)
    {
        put_out_of_play(f, Condition::surrendered);
        log({"{name} surrenders.", {{"name", f.name}}});
        return true;
    }
    return false;
}

// Off the field (fxengine.pas, PutOutOfPlayWithStatus): one that gave up has
// no Hit Points left; one that got away keeps its own. Its actions end.
void Session::put_out_of_play(Fighter &f, Condition condition)
{
    f.condition = condition;
    if (condition != Condition::fled)
    {
        f.hit_points = 0;
    }
    f.fleeing = false;
    end_actions(f);
    end_effects_at_zero(f);
}

// Stepping off the field (cbtacts.pas, AttemptFlee): it gets away with no
// enemy in sight, or with more movement than the fastest enemy, or on a 1 in
// 2 when they tie. Its actions end either way.
void Session::attempt_flee(Fighter &f)
{
    const bool watched =
        std::any_of(fighters_.begin(), fighters_.end(),
                    [this, &f](const Fighter &other)
                    {
                        return other.side != f.side && other.able() && sees(board_, f, other);
                    });
    const int fastest = highest_enemy_movement(f);
    const int own = movement_of(f);
    const bool away = !watched || fastest < own || (fastest == own && roll(2) == 1);
    if (away)
    {
        put_out_of_play(f, Condition::fled);
        log({"{name} got away.", {{"name", f.name}}});
        if (escape_ends_fight_ && f.side == Side::party && f.numbers.morale < non_player)
        {
            end_fight_by_escape(f);
        }
        return;
    }
    log({"{name} cannot get away.", {{"name", f.name}}});
    end_actions(f);
}

// One member's escape ends Gateway's maze fights (cbtacts.pas, AttemptFlee in
// combat mode 1): the party leaves by the side it ran off, and the monsters
// left stay behind, worth nothing.
void Session::end_fight_by_escape(const Fighter &escaped)
{
    escape_side_ = escaped.cell.y <= 0                   ? 0U
                   : escaped.cell.x >= board_.width - 1  ? 1U
                   : escaped.cell.y >= board_.height - 1 ? 2U
                                                         : 3U;
    for (auto &f : fighters_)
    {
        if (f.side == Side::opposition && f.condition == Condition::okay)
        {
            put_out_of_play(f, Condition::fled);
        }
    }
    settle_outcome();
}

// The squares just off the field next to a creature on its edge.
std::vector<Cell> Session::off_field_steps(const Fighter &f) const
{
    std::vector<Cell> steps;
    for (const auto square : squares_of(f))
    {
        for (int direction = 0; direction < 8; ++direction)
        {
            const Cell step{square.x + step_x[static_cast<std::size_t>(direction)],
                            square.y + step_y[static_cast<std::size_t>(direction)]};
            if (!board_.contains(step) &&
                std::find(steps.begin(), steps.end(), step) == steps.end())
            {
                steps.push_back(step);
            }
        }
    }
    return steps;
}

// A side with no one left able to fight has lost. When the party wins, its
// dying members wake unconscious (postcbt.pas).
void Session::settle_outcome()
{
    const auto side_standing = [this](Side side)
    {
        return std::any_of(fighters_.begin(), fighters_.end(),
                           [side](const Fighter &f)
                           {
                               return f.side == side && f.able();
                           });
    };
    // With none of the party left fighting, it fled when one of its own
    // characters got away, and is defeated otherwise (postcbt.pas,
    // DetermineCombatResults).
    if (!side_standing(Side::party))
    {
        const bool got_away =
            std::any_of(fighters_.begin(), fighters_.end(),
                        [](const Fighter &f)
                        {
                            return f.side == Side::party && f.condition == Condition::fled &&
                                   f.arrival_round == 0 && f.numbers.morale < non_player;
                        });
        outcome_ = got_away ? rules::Outcome::fled : rules::Outcome::defeat;
    }
    else if (!side_standing(Side::opposition))
    {
        win_fight();
    }
    if (outcome_ != rules::Outcome::ongoing)
    {
        // A frenzy ends with the fight, and the party plays the lover again
        // (postcbt.pas, DetermineCombatResults).
        for (auto &f : fighters_)
        {
            if (auto *love = f.find_effect(effect::in_love);
                love && love->strength == effect::frenzied_love)
            {
                love->strength = 0;
                if (f.numbers.morale == animated_player)
                {
                    f.numbers.morale = 0;
                }
            }
        }
        actor_ = 0;
        // A Buck Rogers fight ends with the new commands' statuses back to
        // okay, and no one blind, helpless or stunned (combat.pas
        // CombatDoneCleanup).
        for (auto &f : fighters_)
        {
            if (f.ground)
            {
                auto &creature = f.ground->creature();
                if (status_of(creature) >= BuckRogersStatus::dodging)
                {
                    set_status(creature, BuckRogersStatus::okay);
                }
                std::erase_if(f.ground->effects,
                              [](const GroundEffect &e)
                              {
                                  return e.code == ground_effect::blind ||
                                         e.code == ground_effect::helpless ||
                                         e.code == ground_effect::stunned;
                              });
            }
        }
        log({outcome_ == rules::Outcome::victory ? "The party wins the fight."
             : outcome_ == rules::Outcome::fled  ? "The party has fled."
                                                 : "The party is defeated.",
             {}});
    }
}

// The party wins, and its dying members wake unconscious (postcbt.pas,
// DetermineCombatResults).
void Session::win_fight()
{
    outcome_ = rules::Outcome::victory;
    for (auto &f : fighters_)
    {
        if (f.side == Side::party && f.condition == Condition::dying)
        {
            f.condition = Condition::unconscious;
            f.overkill = 0;
        }
    }
}

// A fight in which no one has attacked for 15 rounds ends (combat.pas,
// EndOfRound). With some of the party still on its feet the original counts
// it won (postcbt.pas, DetermineCombatResults: a member okay wins the
// fight); the monsters still standing leave the field, worth neither
// experience nor treasure, as the original pays only for those put down
// (DetermineLootExp).
void Session::end_quiet_fight()
{
    round_limit_reached_ = true;
    log({"No one has attacked for 15 rounds, and the fight ends.", {}});
    win_fight();
    actor_ = 0;
    log({"The party wins the fight.", {}});
}

rules::MovementGrid Session::grid_for(const Fighter &mover) const
{
    std::vector<rules::Occupant> occupants;
    for (const auto &f : fighters_)
    {
        if (f.id == mover.id || f.condition == Condition::dead || f.condition == Condition::fled ||
            f.condition == Condition::surrendered || f.condition == Condition::gone ||
            f.condition == Condition::stoned)
        {
            continue;
        }
        occupants.push_back({f.cell, f.side != mover.side, !f.able(), f.downed(), f.footprint});
    }
    return rules::MovementGrid(
        board_, mover.cell, occupants, false, false, false, mover.footprint, half_moves,
        original_movement() ? rules::StepRules::original : rules::StepRules::standard);
}

namespace
{
// A scroll's spells not yet read, a bit for each of its record's spell bytes.
int unread_spells(const UsableItem &scroll)
{
    int bits = 0;
    for (std::size_t at = 0; at < scroll.spells.size(); ++at)
    {
        if (scroll.spells[at] != 0)
        {
            bits |= 1 << at;
        }
    }
    return bits;
}
} // namespace

rules::Snapshot Session::snapshot() const
{
    rules::Snapshot state;
    state.identity = identity_;
    state.revision = revision_;
    state.round = round_;
    if (game_of(identity_) == GoldBoxGame::death_knights_of_krynn)
    {
        // EndOfRound advances one minute, including a fight's final round.
        const auto finished =
            round_ - (round_ > 0 && outcome_ == rules::Outcome::ongoing ? 1U : 0U);
        state.elapsed_milliseconds = static_cast<std::uint64_t>(finished) * 60000;
    }
    state.actor = actor_;
    state.outcome = outcome_;
    state.escape_side = escape_side_;
    state.crystal = crystal_;
    state.romance = romance_;
    state.round_limit_reached = round_limit_reached_;
    state.battlefield = board_;
    state.log_entries = log_;
    // Buck Rogers chaff and aerosol mist are shown where they lie, as the
    // original draws its clouds: the player must see what stops a launched
    // missile or grenade (ground_launch) and some shots (CalcWeaponDamage).
    for (const auto &cloud : ground_clouds_)
    {
        if (cloud.matter == CloudMatter::chaff || cloud.matter == CloudMatter::mist)
        {
            std::ranges::copy(cloud.squares(), std::back_inserter(state.obscured));
        }
    }
    std::vector<const Fighter *> order;
    for (const auto &f : fighters_)
    {
        order.push_back(&f);
    }
    std::stable_sort(order.begin(), order.end(),
                     [](const Fighter *a, const Fighter *b)
                     {
                         return a->initiative > b->initiative;
                     });
    for (const auto *f : order)
    {
        rules::CombatantView view;
        view.id = f->id;
        view.name = f->name;
        view.definition = f->definition;
        view.side = f->side;
        view.cell = f->cell;
        view.footprint = f->footprint;
        view.facing_left = f->facing_left;
        view.hit_points = f->condition == Condition::dying ? -f->overkill : f->hit_points;
        view.max_hit_points = f->numbers.max_hit_points;
        view.armor_class = f->numbers.armor_class;
        view.initiative = f->initiative;
        // Half-moves are 5 feet each on the original's 10-foot squares.
        view.movement_feet = f->half_moves * 5 / 2;
        view.action = f->id == actor_;
        view.conscious = f->able();
        view.dead = f->condition == Condition::dead || f->condition == Condition::gone;
        view.status = f->condition == Condition::dead          ? "dead"
                      : f->condition == Condition::dying       ? "dying"
                      : f->condition == Condition::unconscious ? "unconscious"
                      : f->condition == Condition::fled
                          ? (f->arrival_round == gated_reserve_round ? "waiting" : "fled")
                      : f->condition == Condition::surrendered ? "surrendered"
                      : f->condition == Condition::gone        ? "gone"
                      : f->condition == Condition::stoned      ? "stoned"
                      : f->helpless()                          ? "helpless"
                                                               : "";
        view.fled = f->condition == Condition::fled;
        view.surrendered = f->condition == Condition::surrendered;
        view.panicked = f->fleeing;
        // What outlasts the fight: its Hit Points (one that got away keeps
        // them), death, and its numbers as the fight left them (levels
        // drained, effects still running), for the game to keep.
        const bool keeps = f->condition == Condition::okay || f->condition == Condition::fled;
        // A statue stays one until Stone to Flesh (status PETRIFIED).
        const bool native_npc = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
                                f->side == Side::party && f->record &&
                                (f->record->npc_flag & 0x80) != 0;
        // Leadership takes a Buck Rogers companion off the computer's control
        // for the fight (ground_begin_round); nothing else does.
        const bool led_companion = f->ground && f->side == Side::party &&
                                   f->ground->creature().companion != 0 &&
                                   !f->ground->computer_plays;
        view.player_controls_ally =
            (native_npc && f->has(dq_effect::led_by_knight)) || led_companion;
        view.computer_plays = frenzied(*f) || (f->ground && f->ground->computer_plays) ||
                              (native_npc && !view.player_controls_ally);
        view.persistent = {keeps ? f->hit_points : 0, view.dead, f->numbers.encode(),
                           f->condition == Condition::stoned ? std::string(stoned_description)
                                                             : std::string()};
        // A Buck Rogers creature shows its ascending Armor Class and its
        // record's status, and leaves its record and items as the fight left
        // them: Hit Points, injury, rounds and readied gear
        // (decode_ground_record).
        if (f->ground)
        {
            const auto status = status_of(f->ground->creature());
            view.armor_class = ground_numbers_of(*f).armor_class[0];
            view.max_hit_points = f->ground->creature().hit_point_maximum;
            view.hit_points = f->hit_points;
            view.dead = status == BuckRogersStatus::dead || status == BuckRogersStatus::gone;
            view.status = status == BuckRogersStatus::comatose   ? "comatose"
                          : status == BuckRogersStatus::poisoned ? "poisoned"
                                                                 : view.status;
            view.persistent = {f->hit_points, view.dead, encode_ground_record(f->ground->gear),
                               std::string()};
            state.combatants.push_back(std::move(view));
            continue;
        }
        // What is left of a party member's items, for the game to trim its
        // inventory to: an item gone is 0, a scroll the bits of its spells
        // not read.
        if (f->record)
        {
            // What it carries as the fight has left it: each item whose
            // count changed, and the items readied when the fight changed
            // which.
            const auto held = held_now(*f);
            std::vector<std::pair<int, std::uint64_t>> readied;
            for (const auto &item : held)
            {
                // A hammer the fight conjured outlasts it with its spell
                // (EffSpiritualHammer), as an item the member carries.
                if (!item.inventory_id && item.conjured && !item.gone)
                {
                    view.items_gained.push_back({por::original_item_key(item.carried.item),
                                                 item.name, item.carried.item.readied()});
                    continue;
                }
                if (!item.inventory_id)
                {
                    continue;
                }
                const int left = item.gone ? 0 : held_count(item.carried);
                if (left != item.start_count)
                {
                    view.item_uses_left.emplace_back(item.inventory_id,
                                                     static_cast<unsigned>(left));
                }
                if (!item.gone && item.carried.item.readied())
                {
                    readied.emplace_back(item.readied_order, item.inventory_id);
                }
                // Armor a black pudding has eaten into keeps the plus it has left.
                if (!item.gone && f->has(effect::armor_eaten) &&
                    item.carried.type.worn_location == 2)
                {
                    view.item_pluses.emplace_back(item.inventory_id, item.carried.item.magic_bonus);
                }
            }
            if (f->readied_changed)
            {
                std::ranges::sort(readied);
                auto &ids = view.readied_items.emplace();
                for (const auto &[readied_order, id] : readied)
                {
                    ids.push_back(id);
                }
            }
            state.combatants.push_back(std::move(view));
            continue;
        }
        for (const auto &[ordinal, id] : f->item_ids)
        {
            const auto &items = f->numbers.items;
            const auto usable = std::ranges::find(items, ordinal, &UsableItem::ordinal);
            const int left = usable == items.end() ? 0
                             : usable->scroll      ? unread_spells(*usable)
                             : usable->charges > 0 ? usable->charges
                                                   : std::max(usable->quantity, 1);
            view.item_uses_left.emplace_back(id, static_cast<unsigned>(left));
        }
        if (f->shot_item)
        {
            const auto &missile = f->numbers.missile;
            const int left = missile && missile->loaded ? std::max(missile->quantity, 1) : 0;
            view.item_uses_left.emplace_back(f->shot_item, static_cast<unsigned>(left));
        }
        state.combatants.push_back(std::move(view));
    }
    return state;
}

std::vector<rules::Command> Session::legal_commands() const
{
    std::vector<rules::Command> commands;
    const auto *actor = find(actor_);
    if (outcome_ != rules::Outcome::ongoing || !actor)
    {
        return commands;
    }
    if (actor->aiming)
    {
        return aiming_commands(*actor);
    }
    if (actor->ground)
    {
        return ground_commands(*actor);
    }
    // A lover in a frenzy fights under the computer's control (EffInLove).
    if (frenzied(*actor))
    {
        rules::Command quick;
        quick.revision = revision_;
        quick.actor = actor_;
        quick.verb = "quick";
        commands.push_back(quick);
        return commands;
    }
    for (const auto &cell : movement_reach(actor_))
    {
        rules::Command move;
        move.revision = revision_;
        move.actor = actor_;
        move.verb = "move";
        move.destination = cell;
        commands.push_back(move);
    }
    if (actor->attacks() > 0)
    {
        for (const auto &f : fighters_)
        {
            // A creature down is out of the fight (charhlp.pas, InflictDamage
            // clears its enabled flag) and is no target.
            if (f.side == actor->side || !f.able())
            {
                continue;
            }
            if (auto verb = attack_verb(*actor, f))
            {
                rules::Command attack;
                attack.revision = revision_;
                attack.actor = actor_;
                attack.target = f.id;
                attack.verb = std::move(*verb);
                attack.intent = rules::Intent::hostile;
                commands.push_back(attack);
            }
        }
    }
    // The Done menu (combat.pas, DoneMenu): Guard, Delay, Bandage and ending its actions.
    const auto offer = [&commands, actor, this](std::string verb, EntityId target = 0)
    {
        rules::Command command;
        command.revision = revision_;
        command.actor = actor->id;
        command.verb = std::move(verb);
        command.target = target;
        commands.push_back(command);
    };
    // Cast, while it may and has a spell ready for a fight (combat.pas, CombatMenu).
    if (actor->cast_allowed && !silenced(*actor) && !actor->has(effect::no_magic))
    {
        std::vector<int> offered_spells;
        for (const int number : actor->numbers.spells)
        {
            const auto *known = game_spell(number);
            if (known && known->when != 0 && spell_built(number) &&
                (!underwater_ || underwater_spell(number)) &&
                std::find(offered_spells.begin(), offered_spells.end(), number) ==
                    offered_spells.end())
            {
                offered_spells.push_back(number);
                rules::Command cast;
                cast.revision = revision_;
                cast.actor = actor->id;
                cast.verb = "cast";
                cast.item = static_cast<unsigned>(number);
                cast.label = std::string(known->name);
                commands.push_back(cast);
            }
        }
    }
    // Use, for a readied item with a spell or power for a fight; a scroll's
    // spells once it is known, or by Read Magic, or a cleric's scroll for a
    // cleric (viewchar.pas, UseItemMenu; magic.pas, the scroll).
    if (actor->item_use_allowed && !silenced(*actor) &&
        !((darkness() || game_of(identity_) == GoldBoxGame::dark_queen_of_krynn) &&
          actor->has(effect::no_magic)))
    {
        const bool cleric =
            actor->numbers.class_levels[static_cast<std::size_t>(ClassSlot::cleric)] > 0;
        for (const auto &item : actor->numbers.items)
        {
            const bool readable = !item.scroll || item.identified ||
                                  actor->has(effect::read_magic) || (cleric && item.slot == 12);
            if (!item.readied || !readable)
            {
                continue;
            }
            for (const int number : item.spells)
            {
                const auto *known = game_spell(number);
                if (!known || known->when == 0 || !spell_built(number))
                {
                    continue;
                }
                rules::Command use;
                use.revision = revision_;
                use.actor = actor->id;
                use.verb = "use";
                use.item = static_cast<unsigned>(item.ordinal * 256 + number);
                use.label = std::string(known->name);
                commands.push_back(use);
                if (!item.scroll)
                {
                    break;
                }
            }
        }
    }
    // Ready, for an item a party member carries and may ready, and put away,
    // for one readied that is not cursed (viewchar.pas, ToggleReadyItem).
    if (actor->record)
    {
        const auto held = held_now(*actor);
        std::vector<CarriedItem> readied;
        for (const auto &item : held)
        {
            if (!item.gone && item.carried.item.readied())
            {
                readied.push_back(item.carried);
            }
        }
        for (std::size_t at = 0; at < held.size(); ++at)
        {
            const auto &item = held[at];
            const bool is_readied = item.carried.item.readied();
            const bool banked =
                std::ranges::any_of(actor->numbers.effects,
                                    [at](const Effect &power)
                                    {
                                        return power.code == effect::baaz_banked_weapon &&
                                               power.strength == static_cast<int>(at + 1);
                                    });
            const bool wrong_order =
                champions() && champions_ready_refusal(item.carried.item, actor->record->race,
                                                       actor->record->robe_order)
                                   .has_value();
            if (item.gone || banked || (!is_readied && wrong_order) ||
                (!item.inventory_id && !item.conjured) ||
                (is_readied ? item.carried.item.cursed_raw != 0
                            : readying_refusal(item.carried, readied, actor->record->item_limit,
                                               game_of(identity_))
                                  .has_value()))
            {
                continue;
            }
            rules::Command toggle;
            toggle.revision = revision_;
            toggle.actor = actor_;
            toggle.verb = is_readied ? "unready" : "ready";
            toggle.item = static_cast<unsigned>(at);
            toggle.label = item.name;
            commands.push_back(toggle);
        }
    }
    // Guard needs a weapon that strikes next to it (combat.pas, DoneMenu).
    if (!actor->numbers.missile || actor->numbers.missile->throwable)
    {
        offer("guard");
    }
    offer("delay");
    // Quick: the original's AI plays its turn (combat.pas, TurnOnAuto); a party
    // member's also with its spells, as with the party's Quick magic on.
    offer("quick");
    if (actor->numbers.morale < non_player)
    {
        offer("quick_magic");
    }
    // Turn, once a fight for one with cleric levels (combat.pas, CombatMenu).
    // Secret has no turning (its CombatMenu offers none).
    if (may_turn(*actor))
    {
        offer("turn");
    }
    if (champions() && actor->numbers.race == 5 &&
        std::ranges::none_of(fighters_,
                             [](const Fighter &fighter)
                             {
                                 return fighter.has(effect::kender_yelled);
                             }))
    {
        offer("yell");
    }
    for (const auto &f : fighters_)
    {
        if (can_bandage(*actor, f))
        {
            offer("bandage", f.id);
        }
    }
    // Stepping off the field offers to flee, with a step left (combat.pas, MoveMenu).
    if (actor->half_moves >= 2)
    {
        for (const auto step : off_field_steps(*actor))
        {
            rules::Command flee;
            flee.revision = revision_;
            flee.actor = actor->id;
            flee.verb = "flee";
            flee.destination = step;
            commands.push_back(flee);
        }
    }
    offer("end");
    return commands;
}

std::vector<Cell> Session::movement_reach(EntityId id) const
{
    std::vector<Cell> cells;
    const auto *mover = find(id);
    if (!mover || id != actor_ || mover->half_moves <= 0)
    {
        return cells;
    }
    const auto reach = grid_for(*mover).reachable(mover->half_moves);
    for (int y = 0; y < board_.height; ++y)
    {
        for (int x = 0; x < board_.width; ++x)
        {
            if (reach.cost_to({x, y}))
            {
                cells.push_back({x, y});
            }
        }
    }
    return cells;
}

bool Session::submit(const rules::Command &command)
{
    const auto legal = legal_commands();
    const bool offered =
        std::any_of(legal.begin(), legal.end(),
                    [&command](const rules::Command &c)
                    {
                        return c.revision == command.revision && c.actor == command.actor &&
                               c.verb == command.verb && c.target == command.target &&
                               c.item == command.item &&
                               ((c.verb != "move" && c.verb != "flee" && c.verb != "aim" &&
                                 c.verb != "launch" && c.verb != "land") ||
                                c.destination == command.destination);
                    });
    if (!offered)
    {
        return false;
    }
    auto &actor = *find(actor_);
    if (actor.ground && ground_submit(actor, command))
    {
    }
    else if (command.verb == "move")
    {
        // Step by step along the cheapest path, which free attacks and guards may cut short.
        for (const auto cell :
             grid_for(actor).reachable(actor.half_moves).path_to(command.destination))
        {
            if (!actor.able() || actor.half_moves <= 0)
            {
                break;
            }
            step_to(actor, cell);
        }
        // As the move menu closes, less than a straight step is nothing
        // (combat.pas, the move menu).
        if (original_movement() && actor.half_moves < 2)
        {
            actor.half_moves = 0;
        }
    }
    else if (command.verb == "melee")
    {
        auto &target = *find(command.target);
        note_attack(actor, target);
        if (!sweep(actor, target))
        {
            make_attack(actor, target, false);
        }
    }
    else if (command.verb == "ranged" || command.verb == "throw")
    {
        auto &target = *find(command.target);
        note_attack(actor, target);
        shoot(actor, target);
    }
    else if (command.verb == "yell")
    {
        kender_yell(actor);
    }
    else if (command.verb == "turn")
    {
        turn_undead(actor);
    }
    else if (command.verb == "cast")
    {
        begin_cast(actor, static_cast<int>(command.item));
    }
    else if (command.verb == "use")
    {
        // An item's spell goes off at once (viewchar.pas, UseItem).
        actor.using_item = static_cast<int>(command.item / 256);
        log({"{name} uses an item.", {{"name", actor.name}}});
        start_aiming(actor, static_cast<int>(command.item % 256));
    }
    else if (command.verb == "aim")
    {
        const auto &known = *game_spell(actor.aiming);
        const int area = known.area & 0x0F;
        const bool native_ground = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
                                   area == 15 && command.target == 0;
        if ((area >= 8 && area <= 14) || native_ground)
        {
            const int radius = native_ground ? known.area >> 4 : area & 7;
            resolve_spell(actor, within(command.destination, radius), command.destination);
        }
        else
        {
            actor.picked.push_back(command.target);
            if (--actor.picks_left <= 0)
            {
                resolve_spell(actor, actor.picked, find(command.target)->cell);
            }
        }
    }
    else if (command.verb == "blast_delay")
    {
        actor.blast_delay = static_cast<int>(command.item);
    }
    else if (command.verb == "flames")
    {
        resolve_spell(actor, {actor.id}, actor.cell, static_cast<Flames>(command.item));
    }
    else if (command.verb == "aim_done")
    {
        resolve_spell(actor, actor.picked, find(actor.picked.back())->cell);
    }
    else if (command.verb == "abort")
    {
        log({"{name} aborts the spell.", {{"name", actor.name}}});
        // An item's aim given up ends the turn but spends nothing; the
        // original spent the charge (an obvious bug, fixed).
        if (actor.using_item >= 0)
        {
            actor.aiming = actor.picks_left = 0;
            actor.picked.clear();
            actor.using_item = -1;
        }
        else
        {
            lose_spell(actor);
        }
        end_actions(actor);
    }
    else if (command.verb == "flee")
    {
        attempt_flee(actor);
    }
    else if (command.verb == "quick" || command.verb == "quick_magic")
    {
        ai_turn(actor, command.verb == "quick_magic");
    }
    else if (command.verb == "ready" || command.verb == "unready")
    {
        toggle_readied(actor, command.item);
    }
    else if (command.verb == "guard")
    {
        end_actions(actor);
        actor.guarding = true;
        log({"{name} guards.", {{"name", actor.name}}});
    }
    else if (command.verb == "delay")
    {
        // It acts again later this round, at initiative 1; in Secret and Pools
        // of Darkness, last of all (combat.pas, ManualControl, in their
        // reconstructions).
        actor.initiative = 1;
        if (ordered_rounds())
        {
            turn_order_.push_back(actor.id);
        }
        actor_ = 0;
    }
    else if (command.verb == "bandage")
    {
        auto &dying = *find(command.target);
        dying.condition = Condition::unconscious;
        dying.overkill = 0;
        log({"{name} is bandaged.", {{"name", dying.name}}});
        end_actions(actor);
    }
    else
    {
        end_actions(actor);
    }
    ++revision_;
    // A creature in a jetpack's flight lands before its turn goes on.
    const bool flying = actor_ != 0 && actor.ground && actor.ground->flying && actor.able();
    if (outcome_ == rules::Outcome::ongoing && (actor_ == 0 || !actor.aiming) && !flying &&
        (actor_ == 0 || actor.initiative == 0 || !actor.able() ||
         (actor.half_moves == 0 && actor.attacks() == 0)))
    {
        next_actor();
    }
    return true;
}

void Session::turn_against_party(const std::vector<EntityId> &turning)
{
    for (const auto id : turning)
    {
        if (auto *f = find(id))
        {
            f->side = Side::opposition;
        }
    }
    ++revision_;
}

namespace
{
// The counts a party member's inventory keeps, which its profile's items
// leave out: its usable items' charges or stacks, and how many of what its
// missile shoots it has.
void take_counts(Fighter &f, const std::vector<rules::CarriedEquipment> &inventory,
                 const Content &content)
{
    namespace bit = por::item_property;
    const auto ammunition = ammunition_types(content.game);
    for (const auto &carried : inventory)
    {
        const auto record = por::item_of_original_key(carried.definition);
        if (carried.equipment_index < 0 || !record)
        {
            continue;
        }
        const int count = static_cast<int>(carried.quantity);
        for (auto &usable : f.numbers.items)
        {
            if (usable.ordinal != carried.equipment_index)
            {
                continue;
            }
            f.item_ids.emplace_back(usable.ordinal, carried.inventory_id);
            if (usable.charges > 0)
            {
                usable.charges = count;
            }
            else
            {
                usable.quantity = count > 1 ? count : 0;
            }
        }
        auto &missile = f.numbers.missile;
        if (!missile || f.shot_item || f.numbers.weapon_type < 0 ||
            static_cast<std::size_t>(f.numbers.weapon_type) >= content.item_types.size())
        {
            continue;
        }
        const auto &launcher = content.item_types[static_cast<std::size_t>(f.numbers.weapon_type)];
        const bool shot = missile->thrown ? record->type == f.numbers.weapon_type
                                          : (launcher.has(bit::fires_arrows) &&
                                             record->type == ammunition.arrows) ||
                                                (launcher.has(bit::fires_quarrels) &&
                                                 ammunition.shot_as_quarrel(record->type));
        if (shot)
        {
            f.shot_item = carried.inventory_id;
            missile->quantity = count > 1 ? count : 0;
        }
    }
}

// What a party member readies from in the fight: its record and the items it
// carries, each with its count, the readied in the order its profile has
// them. One with a readied item of no original record readies nothing.
void take_gear(Fighter &f, const rules::Participant &p, const Content &content)
{
    auto record = decode_fight_record(p.character_record);
    if (!record)
    {
        return;
    }
    std::vector<Held> held;
    for (const auto &carried : p.inventory)
    {
        const auto item = por::item_of_original_key(carried.definition);
        if (!item || item->type >= content.item_types.size())
        {
            if (carried.equipment_index >= 0)
            {
                return;
            }
            continue;
        }
        Held h{{*item, content.item_types[item->type]}, carried.inventory_id, carried.name};
        // A hammer an earlier fight conjured goes as its spell does.
        h.conjured = is_spiritual_hammer(*item, content.game);
        const auto count = static_cast<int>(carried.quantity);
        if (count > 1 && held_count(h.carried) != count)
        {
            h.carried.item.stack_size = static_cast<std::uint8_t>(count);
            h.carried.item.raw[57] = h.carried.item.stack_size;
        }
        set_readied(h, carried.equipment_index >= 0);
        h.readied_order = carried.equipment_index;
        h.start_count = held_count(h.carried);
        held.push_back(std::move(h));
    }
    std::vector<int> order;
    for (std::size_t at = 0; at < held.size(); ++at)
    {
        if (held[at].readied_order >= 0)
        {
            order.push_back(static_cast<int>(at));
        }
    }
    std::ranges::sort(order,
                      [&held](int a, int b)
                      {
                          return held[static_cast<std::size_t>(a)].readied_order <
                                 held[static_cast<std::size_t>(b)].readied_order;
                      });
    f.record = std::move(*record);
    f.held = std::move(held);
    f.worked_from = std::move(order);
}

// What a monster readies from: its record and all it carries, as its
// numbers were worked out from them.
void take_monster_gear(Fighter &f, const MonsterGear &gear)
{
    f.record = gear.record;
    f.weighs_unreadied = true;
    for (std::size_t at = 0; at < gear.carried.size(); ++at)
    {
        Held h{gear.carried[at]};
        h.readied_order = h.carried.item.readied() ? static_cast<int>(at) : -1;
        h.start_count = held_count(h.carried);
        f.held.push_back(std::move(h));
        f.worked_from.push_back(static_cast<int>(at));
    }
}

// What the difficulty does to a creature as its fight begins
// (rules::OriginalStrength). A thrown weapon's empty-handed THAC0 moves with
// its THAC0, for when the last one is thrown.
void strengthen(Combatant &numbers, const rules::OriginalStrength &strength)
{
    if (strength.hit_points_percent != 100)
    {
        const auto scaled = [percent = static_cast<int>(strength.hit_points_percent)](int points)
        {
            return std::max(points * percent / 100, 1);
        };
        numbers.hit_points = scaled(numbers.hit_points);
        numbers.max_hit_points = scaled(numbers.max_hit_points);
    }
    numbers.thac0 -= strength.to_hit;
    if (numbers.missile)
    {
        numbers.missile->empty_handed_thac0 -= strength.to_hit;
    }
    numbers.armor_class -= strength.armor_class_bonus;
    numbers.rear_armor_class -= strength.armor_class_bonus;
}
} // namespace

std::unique_ptr<rules::CombatSession> create_session(rules::Identity identity,
                                                     rules::Encounter encounter, std::uint64_t seed,
                                                     const Content &content, bool gazes_played)
{
    std::vector<Fighter> fighters;
    for (auto &p : encounter.participants)
    {
        Fighter f;
        f.id = p.id;
        f.name = std::move(p.name);
        f.definition = p.definition;
        f.side = p.side;
        f.cell = p.cell;
        f.footprint = p.footprint;
        f.facing_left = p.facing_left;
        f.starting_facing = p.facing;
        f.surprised = p.surprised;
        if (is_buck_rogers(game_of(identity)))
        {
            f.ground = ground_fighter(p, f.name);
            f.hit_points = f.ground->creature().hit_points;
            f.numbers = ground_combatant(*f.ground,
                                         ground_numbers(f.ground->creature(), f.ground->gear.items,
                                                        f.ground->gear.types, false));
            const auto status = status_of(f.ground->creature());
            f.condition = status == BuckRogersStatus::dead || status == BuckRogersStatus::gone
                              ? Condition::dead
                          : f.hit_points <= 0 || !can_act(status) ? Condition::unconscious
                                                                  : Condition::okay;
            fighters.push_back(std::move(f));
            continue;
        }
        if (!p.character_profile.empty())
        {
            const auto numbers = Combatant::decode(p.character_profile);
            if (!numbers)
            {
                throw std::runtime_error("A party member has no original-rules profile: " + f.name);
            }
            f.numbers = *numbers;
            take_counts(f, p.inventory, content);
            take_gear(f, p, content);
            // A member's last fight left it the spells it had not cast, which
            // it keeps until a day's rest memorizes them again, and the
            // conditions that outlast fights and rests (lasting_condition).
            if (p.state && !p.state->resources.empty())
            {
                const auto carried = carried_state(p.state->resources, content.game);
                if (carried.last_fight)
                {
                    f.numbers.spells = carried.last_fight->spells;
                }
                for (const auto &kept : carried.conditions)
                {
                    if (!f.has(kept.code))
                    {
                        f.numbers.effects.push_back(kept);
                    }
                }
            }
        }
        else if (const auto found = content.bestiary.find(p.definition);
                 found != content.bestiary.end())
        {
            f.numbers = found->second;
            if (const auto gear = content.gear.find(p.definition); gear != content.gear.end())
            {
                take_monster_gear(f, gear->second);
            }
        }
        else
        {
            throw std::runtime_error("No original-rules record for " + p.definition);
        }
        if (content.game == GoldBoxGame::dark_queen_of_krynn)
        {
            for (const auto &effect : f.numbers.effects)
            {
                if (dark_queen_effect::unimplemented(effect.code))
                {
                    throw std::runtime_error(
                        "Dark Queen effect " +
                        std::to_string(effect.code - dark_queen_effect::offset) +
                        " is not implemented yet");
                }
            }
        }
        strengthen(f.numbers, p.original_strength);
        f.hit_points = p.state ? p.state->hit_points : f.numbers.hit_points;
        if (p.state && p.state->dead)
        {
            f.condition = Condition::dead;
        }
        else if (p.state && p.state->description == stoned_description)
        {
            f.condition = Condition::stoned;
            f.hit_points = 0;
        }
        else if (f.hit_points <= 0)
        {
            f.condition = Condition::unconscious;
            f.hit_points = 0;
        }
        else if (p.arrival_round > 0)
        {
            f.condition = Condition::fled;
            f.arrival_round = static_cast<int>(p.arrival_round);
        }
        else if (game_of(identity) == GoldBoxGame::champions_of_krynn && f.record &&
                 f.record->status == 9)
        {
            // The native REMOVED status is a gated reserve. Sir Lebaum's
            // four ghasts wait off the arena until his power brings them in.
            f.condition = Condition::fled;
            f.arrival_round = gated_reserve_round;
        }
        fighters.push_back(std::move(f));
    }
    // In a "no magic" fight every fighter holds the bar, so it lasts as the
    // fight's effects are kept.
    if (encounter.no_magic)
    {
        for (auto &f : fighters)
        {
            f.numbers.effects.push_back({effect::no_magic, 0, 0, false});
        }
    }
    // A party NPC without a morale of its own takes the encounter's (combatst.pas,
    // InitCreatureCombatData).
    const auto threshold = std::min(encounter.morale, 100U);
    for (auto &f : fighters)
    {
        const auto own = f.numbers.morale & 0x7F;
        if (f.side == Side::party && f.numbers.morale >= non_player && (own == 0 || own > 102))
        {
            // The original stored the threshold unhalved, so an ally's morale
            // was twice the encounter's and failed every check from 52 up (an
            // obvious bug, fixed).
            f.numbers.morale = non_player + threshold / 2;
        }
    }
    auto session = std::make_unique<Session>(
        std::move(identity), std::move(encounter.battlefield), std::move(fighters), seed, threshold,
        encounter.outdoors, encounter.escape_ends_fight, encounter.party_facing,
        encounter.party_move_modifier, encounter.crystals, encounter.frightening_surprise,
        encounter.turning_tries, encounter.turning_penalty, encounter.time_minutes,
        encounter.zero_gravity,
        content.game == GoldBoxGame::dark_queen_of_krynn && encounter.underwater);
    session->know_item_types(content.item_types);
    session->know_item_words(content.buck_rogers_item_words);
    session->play_gazes(gazes_played);
    session->move_as_the_original(content.original_movement);
    session->limit_party_area_spells(content.party_spell_limits);
    session->pair_lovers(std::move(encounter.romance));
    return session;
}

std::unique_ptr<rules::CombatSession> restore_session(rules::Identity identity,
                                                      std::string_view checkpoint,
                                                      const Content &content, bool gazes_played)
{
    auto session = Session::restored(std::move(identity), checkpoint, content);
    session->know_item_words(content.buck_rogers_item_words);
    session->play_gazes(gazes_played);
    session->move_as_the_original(content.original_movement);
    session->limit_party_area_spells(content.party_spell_limits);
    return session;
}

} // namespace opengold::original::detail
