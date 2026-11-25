#include "combat_session_impl.h"

// The Session's per-game specials (combat_session_impl.h): Curse's, Secret's,
// Pools of Darkness's, the kit's, Treasures', Champions' and Dark Queen's
// creatures' own actions.

namespace opengold::original::detail
{

// In a crystal battle the first creature carrying a crystal to fall gives
// it to the party, and every other one still fighting smashes its own, so
// that one battle yields one crystal (effhndlr.pas, EffRedCrystal and
// ShowCrystalSmash; the cluebook's Crystals).
void Session::give_up_crystal(Fighter &f)
{
    if (!crystals_ || crystal_)
    {
        return;
    }
    const auto carried = [](const Fighter &bearer) -> std::optional<unsigned>
    {
        for (unsigned colour = 0; colour < 3; ++colour)
        {
            if (bearer.has(effect::red_crystal + static_cast<int>(colour)))
            {
                return colour;
            }
        }
        return std::nullopt;
    };
    const auto colour = carried(f);
    if (!colour)
    {
        return;
    }
    crystal_ = colour;
    f.remove(effect::red_crystal + static_cast<int>(*colour));
    for (const auto &other : fighters_)
    {
        if (other.able() && carried(other))
        {
            log({"{name} smashes a crystal.", {{"name", other.name}}});
        }
    }
}

void Session::pair_lovers(std::optional<rules::Romance> romance)
{
    romance_ = std::move(romance);
    lover_ = beloved_ = 0;
    if (!romance_)
    {
        return;
    }
    const auto named = [](std::string_view name, std::string_view wanted)
    {
        return !wanted.empty() && std::ranges::equal(name, wanted,
                                                     [](unsigned char a, unsigned char b)
                                                     {
                                                         return std::toupper(a) == std::toupper(b);
                                                     });
    };
    for (const auto &f : fighters_)
    {
        if (named(f.name, romance_->npc))
        {
            beloved_ = f.id;
        }
        if (named(f.name, romance_->member))
        {
            lover_ = f.id;
        }
    }
    for (auto &f : fighters_)
    {
        const bool paired = f.id == lover_ || f.id == beloved_;
        if (paired && !f.has(effect::in_love))
        {
            f.numbers.effects.push_back({effect::in_love, 0, 0, false});
        }
        else if (!paired)
        {
            f.remove(effect::in_love);
        }
    }
}

// A lover falls (EffInLove at ZERO_HIT_POINTS): the other, still fighting
// and loving it 100 or more, goes into a frenzy, the computer playing it to
// the fight's end. A foe that falls to a lover (EffInLove's condition 25,
// for the one whose weapon felled it or who acts) moves the other's love: 2
// up for an evil foe, to 255 at most, 20 down for a good one, to 0 at least.
void Session::love_at_fall(Fighter &fallen)
{
    if (!romance_)
    {
        return;
    }
    const auto other_of = [this](const Fighter &one) -> Fighter *
    {
        const EntityId other = one.id == beloved_ ? lover_ : beloved_;
        return other ? find(other) : nullptr;
    };
    // The love of `one` for the other.
    const auto love_of = [this](const Fighter &one) -> int &
    {
        return one.id == beloved_ ? romance_->npc_love : romance_->member_love;
    };
    if (fallen.has(effect::in_love))
    {
        if (auto *ally = other_of(fallen); ally && ally->able() && love_of(*ally) >= 100)
        {
            if (auto *love = ally->find_effect(effect::in_love);
                love && love->strength != effect::frenzied_love)
            {
                log({"{name} goes into a frenzy.", {{"name", ally->name}}});
                love->strength = effect::frenzied_love;
                if (ally->numbers.morale < non_player)
                {
                    ally->numbers.morale = animated_player;
                }
            }
        }
    }
    if (fallen.able())
    {
        return;
    }
    auto *killer = find(striker_ ? striker_ : actor_);
    if (!killer || killer->id == fallen.id || !killer->has(effect::in_love))
    {
        return;
    }
    // The killer's beloved's love: the member's when the NPC kills.
    int &love = killer->id == beloved_ ? romance_->member_love : romance_->npc_love;
    const int alignment = fallen.numbers.alignment;
    if (alignment == 2 || alignment == 5 || alignment == 8)
    {
        love += love < 0xFE ? 2 : 0;
    }
    else if (alignment == 0 || alignment == 3 || alignment == 6)
    {
        love = love >= 20 ? love - 20 : 0;
    }
}

// Love on an attack roll (EffInLove at ATTACKER_ATTACK_ROLL and at the
// defender's): a lover in a frenzy strikes 2 better, one whose proposal was
// refused 2 worse; one who loves and is loved 100 or more 1 better, one whose
// beloved is not fighting, or who loves 100 or more and is loved below 50, 1
// worse. A lover in a frenzy, or refused, is struck 2 better.
int Session::love_modifier(const Fighter &attacker, const Fighter &defender) const
{
    int modifier = 0;
    if (const auto *love = attacker.find_effect(effect::in_love))
    {
        if (love->strength == effect::frenzied_love)
        {
            modifier += 2;
        }
        else if (love->strength == effect::refused_love)
        {
            modifier -= 2;
        }
        else if (romance_)
        {
            const bool npc = attacker.id == beloved_;
            const int own = npc ? romance_->npc_love : romance_->member_love;
            const int other = npc ? romance_->member_love : romance_->npc_love;
            const EntityId ally_id = npc ? lover_ : beloved_;
            const auto *ally = ally_id ? find(ally_id) : nullptr;
            if (own >= 100 && other >= 100)
            {
                modifier += 1;
            }
            else if (!((own < 100 || other >= 50) && ally && ally->able()))
            {
                modifier -= 1;
            }
        }
    }
    if (const auto *love = defender.find_effect(effect::in_love);
        love && (love->strength == effect::frenzied_love || love->strength == effect::refused_love))
    {
        modifier += 2;
    }
    return modifier;
}

bool Session::frenzied(const Fighter &f) const
{
    const auto *love = f.find_effect(effect::in_love);
    return love && love->strength == effect::frenzied_love && f.side == Side::party;
}

// Pools of Darkness's gazes, played only where the game's option asks
// (docs/CONFIGURATION.md, darkness_gazes): the paralyzing gaze holds its
// lock for 10 rounds unless it saves against paralysis at +3 or meets gazes
// unharmed, a mirror turning a reflectable gaze back on the gazer
// (effhndlr.pas, EffParalyzeGaze); the charming gaze casts Charm Person at
// it, saved against as the spell is (magic.pas, EffGazeCharm).
void Session::darkness_gazes(Fighter &f)
{
    auto *lock = find(f.lock);
    const auto in_sight = [&]
    {
        return f.able() && lock && lock->able() && lock->side != f.side && can_target(f, *lock) &&
               sees(board_, f, *lock);
    };
    if (f.has(effect::darkness_paralyzing_gaze) && in_sight())
    {
        log({"{name} gazes.", {{"name", f.name}}});
        Fighter *victim = lock;
        if (f.has(effect::reflectable_gaze) && lock->numbers.mirror)
        {
            log({"{name} reflects it.", {{"name", lock->name}}});
            victim = &f;
        }
        if (!saving_throw(*victim, 1, 3, &f) && !victim->has(effect::gaze_proof))
        {
            log({"{name} is paralyzed.", {{"name", victim->name}}});
            victim->numbers.effects.push_back({effect::paralysis, 10, 12, false});
        }
    }
    lock = find(f.lock);
    if (f.has(effect::darkness_charming_gaze) && in_sight())
    {
        log({"{name} gazes.", {{"name", f.name}}});
        const bool saved = saving_throw(*lock, 4, 0, &f) || lock->has(effect::gaze_proof);
        if (add_with_message(*lock,
                             {effect::charm, 0, (f.side == Side::opposition ? 128 : 0) + 12, true},
                             true, saved, 0, 12, game_spell(10)))
        {
            charm(*lock, f.side);
        }
    }
}

// A monster's own specials as its AI moves and attacks (combatai.pas,
// MONSTER_SPECIAL): the petrifying gaze, the charming gaze, the lightning
// breath and the ankheg's acid, at its lock.
void Session::monster_specials(Fighter &f)
{
    if (game_of(identity_) == GoldBoxGame::death_knights_of_krynn)
    {
        if (f.has(dkk_effect::death_breath) && f.able() && f.initiative > 0)
        {
            (void)secret_breath(f, dkk_effect::death_breath, 8, 0x60, 1);
        }
        if (f.has(effect::darkness_charming_gaze) && f.able() && f.initiative > 0)
        {
            if (auto *target = secret_target(f, 6, false))
            {
                log({"{name} gazes.", {{"name", f.name}}});
                if (!saving_throw(*target, 4, 0, &f) &&
                    add_with_message(*target, {effect::charm, 0, 255, true}, false, false, 0,
                                     f.numbers.level, game_spell(10)))
                {
                    charm(*target, f.side);
                }
            }
        }
        if (f.has(dkk_effect::smoke) && f.able() && f.initiative > 0)
        {
            Fighter *best = nullptr; // Borrowed from fighters_ for this query only.
            int rating = 0;
            for (auto &target : fighters_)
            {
                if (target.side == f.side || !target.able() || distance(f, target) > 1 ||
                    target.has(dkk_effect::smoked))
                {
                    continue;
                }
                const int score = 60 - target.numbers.thac0 +
                                  target.attacks_left[0] * (6 * target.numbers.attacks[0].dice +
                                                            target.numbers.attacks[0].modifier);
                if (score > rating)
                {
                    best = &target;
                    rating = score;
                }
            }
            if (best && !saving_throw(*best, 3, 0, &f))
            {
                (void)add_with_message(*best, {dkk_effect::smoked, 0, 0, false}, false, false, 0,
                                       f.numbers.level, nullptr);
                log({"{name} is blinded by smoke.", {{"name", best->name}}});
            }
        }
    }
    auto *lock = find(f.lock);
    // Gateway's gazes only at one it sees within 20 squares, not one hidden
    // (its EffPetrifyingGaze).
    const bool gaze_reaches =
        !gateway() || (lock && !lock->has(effect::invisible) && !lock->has(effect::dust) &&
                       distance(f, *lock) <= 20 && sees(board_, f, *lock));
    if (f.has(83) && lock && lock->able() && gaze_reaches)
    {
        // The petrifying gaze, at any range; a mirror turns a gaze that can be
        // reflected back on the gazer (effhndlr.pas, EffPetrifyingGaze).
        log({"{name} gazes.", {{"name", f.name}}});
        Fighter *victim = lock;
        if (f.has(127) && lock->numbers.mirror)
        {
            log({"{name} reflects it.", {{"name", lock->name}}});
            victim = &f;
        }
        if (!saving_throw(*victim, 1, 0, &f))
        {
            victim->condition = Condition::stoned;
            victim->hit_points = 0;
            victim->initiative = 0;
            log({"{name} is stoned.", {{"name", victim->name}}});
            end_effects_at_zero(*victim);
            went_down(*victim);
        }
        if (!f.able())
        {
            return;
        }
    }
    lock = find(f.lock);
    // Effect 84 is the charming gaze in Pool of Radiance, and the kit's
    // vampires' (its 124), only: Curse's and Gateway's shambling mounds carry
    // it as their absorbing of electricity (their effhndlr.pas), which charms
    // nothing.
    const bool charming = game_of(identity_) == GoldBoxGame::pool_of_radiance || kit();
    if (f.has(84) && charming && lock && lock->able() && can_target(f, *lock) &&
        sees(board_, f, *lock))
    {
        // The charming gaze: saved against at -2, and always by one that
        // meets gazes unharmed. The original's reach was an unset value; it
        // reaches what the gazer sees here (an obvious bug, fixed).
        log({"{name} gazes.", {{"name", f.name}}});
        // The kit's gaze casts Charm Person, saved against as the spell is
        // (fxcombat.c, fx_spell_gaze).
        const int gaze_modifier = kit() ? 0 : -2;
        const bool saved =
            saving_throw(*lock, 4, gaze_modifier, &f) || lock->has(effect::gaze_proof);
        if (add_with_message(*lock,
                             {effect::charm, 0, (f.side == Side::opposition ? 128 : 0) + 12, true},
                             true, saved, 0, 12, game_spell(10)))
        {
            charm(*lock, f.side);
        }
    }
    if (darkness() && gazes_played_)
    {
        darkness_gazes(f);
    }
    if (const auto *breath = f.find_effect(88); breath && (round_ == 1 || roll(100) > 50))
    {
        // The lightning breath: always in the first round, then half the time;
        // at an enemy within 4 squares and the dragon's magic-user level, for
        // its maximum Hit Points, saved against for half; 3, 2 or 1 breaths by
        // its strength (effhndlr.pas and magic.pas, EffBreatheLightning). With
        // no enemy in reach the original breathed on itself and spent the
        // breath (an obvious bug, fixed: it holds its breath).
        const int strength = breath->strength;
        const int reach =
            4 + f.numbers.class_levels[static_cast<std::size_t>(ClassSlot::magic_user)];
        if (acquire_target(f, reach, false, true))
        {
            log({"{name} breathes.", {{"name", f.name}}});
            const Cell aim = find(f.lock)->cell;
            const int sx = (aim.x > f.cell.x) - (aim.x < f.cell.x);
            const int sy = (aim.y > f.cell.y) - (aim.y < f.cell.y);
            const Cell first{f.cell.x + sx + (sx > 0 ? 1 : 0), f.cell.y + sy + (sy > 0 ? 1 : 0)};
            while (f.has(effect::invisible))
            {
                f.remove(effect::invisible);
            }
            const int damage = f.numbers.max_hit_points;
            lightning_at(first, damage, 12, 3);
            lightning_path(f.cell, first, 10, damage, 12, false, 3);
            // A breath that came back off a wall may have downed the dragon,
            // which loses the effect with its life.
            if (auto *left = f.find_effect(88); left && strength > 253)
            {
                left->strength = strength - 1;
            }
            else if (left)
            {
                f.remove(88);
            }
            end_actions(f);
            return;
        }
    }
    lock = find(f.lock);
    if (f.has(121) && roll(100) <= 25 && lock && lock->able() && distance(f, *lock) < 4)
    {
        // The ankheg's acid, once a fight: 8d4 acid, saved against for half
        // (effhndlr.pas, EffSpitAcid). The original left it untyped (an
        // obvious bug, fixed).
        end_actions(f);
        log({"{name} spits acid.", {{"name", f.name}}});
        int damage = 0;
        for (int die = 0; die < 8; ++die)
        {
            damage += roll(4);
        }
        nonweapon_damage(*lock, damage, 2, saving_throw(*lock, 3, 0, &f, 16), 16, 12, 0, false);
        f.remove(121);
        f.remove(80);
    }
    dark_queen_specials(f);
    curse_specials(f);
    secret_specials(f);
    kit_specials(f);
    darkness_specials(f);
    savage_frontier_specials(f);
    stone_golem_turn(f);
}

// A Treasures stone golem at its turn (effhndlr.pas, EffSlowAura,
// MONSTER_SPECIAL): stone, it slows an enemy it picks for 6 rounds, one not
// slowed already, and fights on; flesh, it counts down to stone again.
void Session::stone_golem_turn(Fighter &golem)
{
    auto *stone = golem.find_effect(effect::stone_golem);
    if (!stone || !golem.able())
    {
        return;
    }
    if (stone->strength != effect::golem_stone)
    {
        stone->strength = (stone->strength + 255) % 256;
        if (stone->strength == effect::golem_stone)
        {
            log({"{name} returns to stone.", {{"name", golem.name}}});
        }
        return;
    }
    if (!acquire_target(golem, 255, false, true))
    {
        return;
    }
    auto &target = *find(golem.lock);
    if (target.has(effect::slowed))
    {
        return;
    }
    log({"{name} slows {target}.", {{"name", golem.name}, {"target", target.name}}});
    target.numbers.effects.push_back({effect::slowed, 6, 255, false});
}

// Gateway's and Treasures' monsters' own powers at their turn (their
// effhndlr.pas, MONSTER_SPECIAL): Gateway's paralyzing gaze; Treasures' hell
// hound's fire and ice hound's cold, a manticore's spines and a gorgimera's
// stone breath.
void Session::savage_frontier_specials(Fighter &f)
{
    if (!f.able())
    {
        return;
    }
    auto *lock = find(f.lock);
    // At its lock, at any range: held for good unless saved against as
    // petrification (Gateway's EffParalyzingGaze).
    if (f.has(effect::gateway_paralyzing_gaze) && lock && lock->able())
    {
        log({"{name} gazes.", {{"name", f.name}}});
        if (!saving_throw(*lock, 1, 0, &f))
        {
            log({"{name} is paralyzed.", {{"name", lock->name}}});
            lock->numbers.effects.push_back({effect::paralysis, 0, 12, false});
        }
    }
    // 7 of fire or cold at one beside it, saved against for half
    // (EffHellHoundBreath, EffIceHoundBreath); the turn goes on.
    for (const auto [code, element] :
         {std::pair{effect::hell_hound_breath, 0x21}, std::pair{effect::ice_hound_breath, 0x22}})
    {
        if (f.has(code) && acquire_target(f, 12, false, false) && (lock = find(f.lock)) &&
            distance(f, *lock) <= 1)
        {
            log({element == 0x21 ? "{name} breathes fire." : "{name} breathes ice.",
                 {{"name", f.name}}});
            nonweapon_damage(*lock, 7, 2, saving_throw(*lock, 3, 0, &f, element & 3), element, 12,
                             0, false);
        }
    }
    // Six spines of 1d6 at one it sees within 10 squares (EffManticoreSpikes).
    if (f.has(effect::manticore_spines) && acquire_target(f, 10, false, true) &&
        (lock = find(f.lock)) && sees(board_, f, *lock))
    {
        log({"{name} fires its spines.", {{"name", f.name}}});
        auto &blow = f.numbers.attacks[0];
        const auto own = blow;
        blow = {own.rate_per_two_rounds, 1, 6, 0};
        f.attacks_left = {6, 0};
        f.attack_phase = 0;
        make_attack(f, *lock, false);
        blow = own;
        end_actions(f);
        return;
    }
    // Twice a fight, at one within 2 squares: stoned unless saved against as
    // petrification or proof against gazes (EffStoneBreath).
    if (f.has(effect::stone_breath) && uses(f, effect::stone_breath) < 2 &&
        acquire_target(f, 2, false, true) && (lock = find(f.lock)))
    {
        spend_use(f, effect::stone_breath);
        log({"{name} breathes.", {{"name", f.name}}});
        if (!lock->has(effect::gaze_proof) && !saving_throw(*lock, 1, 0, &f))
        {
            turn_to_stone(*lock);
        }
        end_actions(f);
    }
}

// The kit's monsters' breath as their AI attacks (aiturn.c, ai_attack;
// fxcombat.c and effects.c): a blue dragon breathes lightning as Pools of
// Darkness's does (its 91, the fight's 561); a green dragon breathes poison three times a
// fight, its maximum Hit Points along 3 squares at an enemy within them, half
// on a save against breath (90); a gorgon four times, along 6 squares two
// wide at an enemy within them but never through its own side, turning to
// stone each that fails a save against petrification (195). Each breath ends
// its turn.
void Session::kit_specials(Fighter &f)
{
    if (kit() && f.has(effect::blue_breath) && f.able() && f.initiative > 0 && lightning_breath(f))
    {
        return;
    }
    if (f.has(effect::eye_rays))
    {
        beholder_rays(f);
    }
    auto *poison_or_steam = f.find_effect(effect::poison_breath);
    if (!poison_or_steam)
    {
        poison_or_steam = f.find_effect(dq_effect::steam_breath);
    }
    if (auto *breath = poison_or_steam; breath && f.able() && f.initiative > 0)
    {
        breath->strength = std::min(breath->strength, 3);
        const auto targets = breath->strength > 0 && acquire_target(f, 3, false, true)
                                 ? path_area(f, find(f.lock)->cell, 3, 1)
                                 : std::vector<EntityId>{};
        if (!targets.empty())
        {
            --breath->strength;
            log({breath->code == dq_effect::steam_breath ? "{name} breathes steam."
                                                         : "{name} breathes poison.",
                 {{"name", f.name}}});
            for (const auto id : targets)
            {
                auto &victim = *find(id);
                nonweapon_damage(victim, f.numbers.max_hit_points, 2,
                                 saving_throw(victim, 3, 0, &f, 0x20), 0x20, 12, 0, false);
            }
            end_actions(f);
            return;
        }
    }
    if (auto *breath = f.find_effect(effect::gorgon_breath); breath && f.able() && f.initiative > 0)
    {
        breath->strength = std::min(breath->strength, 4);
        const auto targets = breath->strength > 0 && acquire_target(f, 6, false, true)
                                 ? path_area(f, find(f.lock)->cell, 6, 2)
                                 : std::vector<EntityId>{};
        const bool spares_friends = std::ranges::none_of(targets,
                                                         [&](EntityId id)
                                                         {
                                                             return find(id)->side == f.side;
                                                         });
        if (!targets.empty() && spares_friends)
        {
            --breath->strength;
            log({"{name} breathes.", {{"name", f.name}}});
            for (const auto id : targets)
            {
                auto &victim = *find(id);
                if (victim.able() && !saving_throw(victim, 1, 0, &f))
                {
                    turn_to_stone(victim);
                }
            }
            end_actions(f);
        }
    }
}

// Native ray facts: a fireshadow uses a narrow 13-square ray at most once
// per six rounds; the Eye of the Deep stuns along a three-square, two-wide path.
void Session::dark_queen_specials(Fighter &f)
{
    if (!f.able() || f.initiative <= 0)
    {
        return;
    }
    if (auto *breath = f.find_effect(dq_effect::baldric_breath))
    {
        if (round_ == 1 || breath->strength > 3)
        {
            breath->strength = 3;
        }
        if (breath->strength > 0)
        {
            const bool paralysis = roll(2) == 2;
            const auto *target = secret_target(f, 6, true);
            if (target)
            {
                const auto targets =
                    path_area(f, target->cell, paralysis ? 5 : 7, paralysis ? 4 : 3);
                if (!targets.empty() && std::ranges::none_of(targets,
                                                             [&](EntityId id)
                                                             {
                                                                 return find(id)->side == f.side;
                                                             }))
                {
                    log({paralysis ? "{name} breathes paralysis." : "{name} breathes cold.",
                         {{"name", f.name}}});
                    for (const auto id : targets)
                    {
                        auto &victim = *find(id);
                        const bool saved = saving_throw(victim, 3, 0, &f);
                        (void)add_with_message(victim, {effect::paralysis, 10, 12, false}, true,
                                               saved, paralysis ? 0 : 0x22, 12, nullptr);
                    }
                    --breath->strength;
                    end_actions(f);
                    return;
                }
            }
        }
    }
    const bool death_ray = f.has(dq_effect::disintegrating_ray);
    auto *ray = f.find_effect(death_ray ? dq_effect::disintegrating_ray : dq_effect::blinding_ray);
    if (!ray)
    {
        return;
    }
    if (death_ray && (round_ % 6 == 0 || ray->strength > 1))
    {
        ray->strength = 1;
    }
    if (death_ray && ray->strength <= 0)
    {
        return;
    }
    const auto *aim = secret_target(f, 6, true);
    if (!aim)
    {
        return;
    }
    const auto targets = path_area(f, aim->cell, death_ray ? 13 : 3, death_ray ? 1 : 2);
    if (targets.empty() || std::ranges::any_of(targets,
                                               [&](EntityId id)
                                               {
                                                   return find(id)->side == f.side;
                                               }))
    {
        return;
    }
    log({death_ray ? "{name} flashes a ray." : "{name} gazes.", {{"name", f.name}}});
    for (const auto id : targets)
    {
        auto &target = *find(id);
        const bool saved = saving_throw(target, death_ray ? 3 : 0, 0, &f);
        if (death_ray)
        {
            nonweapon_damage(target, roll(4) + roll(4) + roll(4) + roll(4), 1, saved, 0, 0, 0,
                             false);
            if (target.hit_points == 0)
            {
                put_out_of_play(target, Condition::gone);
                log({"{name} is disintegrated.", {{"name", target.name}}});
            }
        }
        else
        {
            (void)add_with_message(target, {effect::paralysis, roll(4) + roll(4), 255, false}, true,
                                   saved, 0, 0, nullptr);
        }
    }
    if (death_ray)
    {
        --ray->strength;
    }
    end_actions(f);
}

// The kit's effects as a creature's turn starts (combat.c, its turn;
// effects.c): a ghast's stench leaves each enemy beside it that fails a save
// against spells 2 worse at its attacks for the round (137, the kit's 148 on
// it), and a bulette strikes 4 times with its second attack, 8 one time in 4
// (99, as its AI readies its weapons).
void Session::kit_turn_start(Fighter &f)
{
    if (!f.able())
    {
        return;
    }
    if (f.has(effect::stench))
    {
        log({"{name} emits an evil stench.", {{"name", f.name}}});
        for (auto &other : fighters_)
        {
            if (other.side != f.side && other.able() && adjacent(f, other) &&
                !saving_throw(other, 4, 0, &f))
            {
                log({"{name} is sickened.", {{"name", other.name}}});
                other.numbers.effects.push_back({effect::sickened, 1, 0, false});
            }
        }
    }
    if (f.has(effect::bulette_attacks))
    {
        f.attacks_left[1] = roll(4) == 1 ? 8 : 4;
    }
}

// The target a Secret monster's power picks, as its AI aims a spell
// (cbtacts.pas, SelectSpellTarget, in its reconstruction): a new lock on an
// enemy it sees within range, in one try. A power aimed as a paralyzing spell
// is (its picker's spell 61) passes a helpless one over.
Fighter *Session::secret_target(Fighter &f, int range, bool skip_helpless)
{
    if (!acquire_target(f, range, false, true))
    {
        return nullptr;
    }
    auto *target = find(f.lock);
    return skip_helpless && target->helpless() ? nullptr : target;
}

// Secret's monsters' powers as each of their AI's turns starts (fxengine.pas,
// its condition 14, in its reconstruction), in its order: the petrifying gaze,
// a pyrohydra's fire, the umber hulk's confusing gaze, a hell hound's fire, a
// giant slug's acid and a dragon's breath. Only the breath ends its turn.
void Session::secret_specials(Fighter &f)
{
    if (f.has(effect::petrifying_gaze) && f.able())
    {
        petrifying_gaze(f);
    }
    // A pyrohydra breathes 8 of fire at its lock beside it, from the second
    // round for as many rounds as its first attack's rate (EffBreatheFireAdjacent).
    auto *lock = find(f.lock);
    const bool fire_round =
        round_ > 1 && static_cast<int>(round_) - 1 <= f.numbers.attacks[0].rate_per_two_rounds;
    if (f.has(effect::fire_beside) && f.able() && fire_round && lock && lock->able() &&
        distance(f, *lock) < 2)
    {
        log({"{name} breathes fire.", {{"name", f.name}}});
        nonweapon_damage(*lock, 8, 2, saving_throw(*lock, 3, 0, &f, 0x21), 0x21, 12, 0, false);
    }
    // The umber hulk's gaze confuses one beside it for 1d10+2 rounds unless
    // it saves against spells (EffReflectSpell) or meets gazes unharmed.
    if (f.has(effect::confusing_gaze) && f.able())
    {
        if (auto *target = secret_target(f, 1, false))
        {
            log({"{name} gazes.", {{"name", f.name}}});
            if (!saving_throw(*target, 4, 0, &f) && !target->has(effect::gaze_proof))
            {
                (void)add_with_message(*target, {235, roll(10) + 2, f.numbers.level, true}, true,
                                       false, 0, f.numbers.level, game_spell(182));
            }
        }
    }
    // A hell hound breathes 7 of fire at one beside it, every turn
    // (EffBreatheFireAtTarget).
    if (f.has(effect::fire_at_target) && f.able())
    {
        if (auto *target = secret_target(f, 1, false))
        {
            log({"{name} breathes fire.", {{"name", f.name}}});
            nonweapon_damage(*target, 7, 2, saving_throw(*target, 3, 0, &f, 0x21), 0x21, 12, 0,
                             false);
        }
    }
    // A giant slug spits 4d8 of acid at one within 6 squares: 1 time in 10 in
    // the first round, then 1 in 10 for each square nearer than 11 (EffSpitAtTarget).
    if (f.has(effect::spits_acid) && f.able())
    {
        if (auto *target = secret_target(f, 6, true))
        {
            const int chance = round_ == 1 ? 10 : (11 - distance(f, *target)) * 10;
            if (roll(100) <= chance)
            {
                log({"{name} spits acid.", {{"name", f.name}}});
                int damage = 0;
                for (int die = 0; die < 4; ++die)
                {
                    damage += roll(8);
                }
                nonweapon_damage(*target, damage, 2, saving_throw(*target, 3, 0, &f, 16), 16, 12, 0,
                                 false);
            }
            else
            {
                log({"{name} spits acid and misses.", {{"name", f.name}}});
            }
        }
    }
    // Maya's native cold cone reaches eight squares and is used once.
    if (champions() && f.has(effect::maya_breath) && f.able() && f.initiative > 0)
    {
        (void)secret_breath(f, effect::maya_breath, 8, 0x22);
    }
    // A white dragon breathes cold 7 squares long, a red one fire 9 long
    // (magic.pas, EffDragonBreath).
    if (f.has(effect::cold_breath) && f.able() && f.initiative > 0)
    {
        (void)secret_breath(f, effect::cold_breath, 7, 0x22);
    }
    if (f.has(effect::fire_breath) && f.able() && f.initiative > 0)
    {
        (void)secret_breath(f, effect::fire_breath, 9, 0x21);
    }
}

// A Secret gaze that turns to stone (effhndlr.pas, EffPetrifyingGaze, in its
// reconstruction): at an enemy within 6 squares that is not helpless, saved
// against as petrification. A target with a mirror turns a gaze that can be
// reflected back on the gazer. Unlimited Adventures' vampires and dracolisk
// meet every gaze unharmed (the kit's 126).
void Session::petrifying_gaze(Fighter &gazer)
{
    auto *target = secret_target(gazer, 6, true);
    if (!target)
    {
        return;
    }
    log({"{name} gazes.", {{"name", gazer.name}}});
    Fighter *victim = target;
    if (gazer.has(effect::reflectable_gaze) && target->numbers.mirror)
    {
        log({"{name} reflects it.", {{"name", target->name}}});
        victim = &gazer;
    }
    // The kit's and Pools of Darkness's immunity to gazes spares it
    // (effhndlr.pas, EffImmuneGaze).
    // The kit spares an iron golem too (its 79; effects.c, fx_stone_gaze).
    const bool spared =
        victim->has(effect::gaze_proof) || (kit() && victim->has(effect::iron_golem));
    if (!saving_throw(*victim, 1, 0, &gazer, 16) && !spared)
    {
        turn_to_stone(*victim);
    }
}

// A Secret dragon's breath (magic.pas, EffDragonBreath, in its reconstruction):
// three a fight, the effect's strength counting them down from the first
// round; aimed at an enemy within 6 squares that is not helpless, along a
// path of the length, three wide (or as wide as given), never while any
// creature of its own side stands in it; its maximum Hit Points to each (or
// the damage given), half on a save against breath. A breath ends its turn.
bool Session::secret_breath(Fighter &dragon, int code, int length, int element, int width,
                            std::optional<int> damage)
{
    auto *breath = dragon.find_effect(code);
    if (round_ == 1 || breath->strength > 3)
    {
        breath->strength = champions() && code == effect::maya_breath ? 1 : 3;
    }
    if (breath->strength <= 0)
    {
        return false;
    }
    const auto *target = secret_target(dragon, 6, true);
    if (!target)
    {
        return false;
    }
    const auto targets = path_area(dragon, target->cell, length, width);
    const bool spares_friends = std::ranges::none_of(targets,
                                                     [&](EntityId id)
                                                     {
                                                         return find(id)->side == dragon.side;
                                                     });
    if (targets.empty() || !spares_friends)
    {
        return false;
    }
    --breath->strength;
    log({"{name} breathes.", {{"name", dragon.name}}});
    const int harm = damage.value_or(dragon.numbers.max_hit_points);
    for (const auto id : targets)
    {
        auto &victim = *find(id);
        nonweapon_damage(victim, harm, 2, saving_throw(victim, 3, 0, &dragon, element), element, 12,
                         0, false);
    }
    end_actions(dragon);
    return true;
}

// Pools of Darkness's monsters' own powers as their AI attacks (fxengine.pas,
// MONSTER_SPECIAL, in its order, and magic.pas): the biggest red dragon
// breathes fire 9 squares long and three wide for 24d10+12, a blue one
// lightning, a black one acid 6 squares long and one wide, each three times a
// fight as Secret's dragons do (EffDragonBreath, EffBreatheLightning); its
// green dragon breathes as the kit's (poison_breath). Its paralyzing and
// charming gazes (114, 124) have handlers the original registers but never
// runs, so the gaze spider and the vampires never gaze, as there.
void Session::darkness_specials(Fighter &f)
{
    if ((!darkness() && !champions()) || !f.able() || f.initiative <= 0)
    {
        return;
    }
    if (f.has(effect::big_red_breath))
    {
        int fire = 12;
        for (int die = 0; die < 24; ++die)
        {
            fire += roll(10);
        }
        if (secret_breath(f, effect::big_red_breath, 9, 0x21, 3, fire))
        {
            return;
        }
    }
    if (f.has(effect::blue_breath) && lightning_breath(f))
    {
        return;
    }
    if (f.has(effect::black_breath) && secret_breath(f, effect::black_breath, 6, 0x20, 1))
    {
        return;
    }
}

// A blue dragon's lightning, Pools of Darkness's and the kit's alike
// (magic.pas, EffBreatheLightning; fxcombat.c, fx_lightning_breath): three a
// fight from the first round, aimed as the dragon would aim a Lightning Bolt,
// its maximum Hit Points to the aim and to each creature along a bolt of 9
// squares that does not bounce, half on a save against spells. The breath is
// electricity, not magic (element 24h), and no spell. With no enemy to aim at
// it holds its breath. It ends its turn.
bool Session::lightning_breath(Fighter &dragon)
{
    auto *breath = dragon.find_effect(effect::blue_breath);
    if (round_ == 1 || breath->strength > 3)
    {
        breath->strength = 3;
    }
    if (breath->strength <= 0)
    {
        return false;
    }
    const auto *target = lightning_bolt_aim(dragon);
    if (!target)
    {
        return false;
    }
    --breath->strength;
    log({"{name} breathes.", {{"name", dragon.name}}});
    const Cell aim = target->cell;
    constexpr int electric_breath = 0x24;
    const int damage = dragon.numbers.max_hit_points, level = dragon.numbers.level;
    lightning_at(aim, damage, level, 4, electric_breath, 0);
    lightning_path(dragon.cell, aim, 9, damage, level, false, 4, electric_breath, 0);
    end_actions(dragon);
    return true;
}

// The enemy a monster's Lightning Bolt is aimed at, as Pools of Darkness's and
// the kit's AI aim it (cbtacts.pas and cbaction.c, aim_target and
// rank_target): within the spell's range there, 4 squares and 4 a caster
// level (their spell tables' row; the spell table here keeps Pool of
// Radiance's 1 a level), and none immune to electricity or within a Globe of
// Invulnerability or a Minor Globe, nor one of magic resistance above 130.
const Fighter *Session::lightning_bolt_aim(Fighter &caster)
{
    constexpr int lightning_bolt = 51;
    const int level = caster_level(caster, *game_spell(lightning_bolt), game_of(identity_));
    const auto *target = secret_target(caster, 4 + 4 * level, false);
    const bool proof = target && (target->has(335) || target->has(263) ||
                                  target->has(effect::globe_of_invulnerability) ||
                                  target->numbers.magic_resistance > 130);
    return proof ? nullptr : target;
}

// A Secret dragon's fear as a fight starts (effhndlr.pas, EffDragonFear, in
// its reconstruction): each enemy below level 5 that is not fearless and not
// afraid already is terrified and runs for the rest of the fight.
void Session::dragon_fear(const Fighter &dragon)
{
    // The kit's terror (effects.c, fx_terrify) reaches one below 6 Hit Dice,
    // saved against as a spell, for 4d6 rounds.
    const int fearless_from = kit() ? 6 : 5;
    for (auto &f : fighters_)
    {
        if (f.side == dragon.side || !f.able() || f.has(effect::fearless) || f.has(effect::fear) ||
            f.numbers.level >= fearless_from)
        {
            continue;
        }
        const bool saved = kit() && saving_throw(f, 4, 0, &dragon);
        const int rounds = kit() ? roll(6) + roll(6) + roll(6) + roll(6) : 0;
        if (add_with_message(f, {effect::fear, rounds, 255, true}, true, saved, 0,
                             dragon.numbers.level, game_spell(184)))
        {
            f.turned = true;
        }
    }
}

// A Pools of Darkness dragon's fear (effhndlr.pas, EffDragonFear, in its
// reconstruction): each able enemy not terrified already that fails a save
// against petrification at -4 is terrified for the fight, 2 worse to hit and
// 2 less harmful a blow (EffDragonFeared).
void Session::darkness_dragon_fear(const Fighter &dragon)
{
    for (auto &f : fighters_)
    {
        if (f.side == dragon.side || !f.able() || f.has(effect::dragon_feared) ||
            saving_throw(f, 1, -4, &dragon))
        {
            continue;
        }
        f.numbers.effects.push_back({effect::dragon_feared, 0, 0, false});
        log({"{name} is terrified.", {{"name", f.name}}});
    }
}

// How many times a creature has used a power this fight, kept on it as an
// effect of the power's code plus 100.
int Session::uses(Fighter &f, int code)
{
    if (const auto *count = f.find_effect(code + 100))
    {
        return count->strength;
    }
    return 0;
}

void Session::spend_use(Fighter &f, int code)
{
    if (auto *count = f.find_effect(code + 100))
    {
        ++count->strength;
        return;
    }
    f.numbers.effects.push_back({code + 100, 0, 1, false});
}

// Curse's monsters' own powers at their turn (its reconstruction's ovr023):
// a breath of acid or fire along a path, a hell hound's fire, Tyranthraxus's
// lightning, a slug's acid, a paralyzing gaze and a beholder's rays.
void Session::curse_specials(Fighter &f)
{
    const auto breathe = [&](int code, int length, int width, int element, bool spare_friends)
    {
        if (!f.has(code) || uses(f, code) >= 3 || !acquire_target(f, 6, false, false))
        {
            return false;
        }
        const auto targets = path_area(f, find(f.lock)->cell, length, width);
        if (spare_friends && std::ranges::any_of(targets,
                                                 [&](EntityId id)
                                                 {
                                                     return find(id)->side == f.side;
                                                 }))
        {
            return false;
        }
        log({"{name} breathes.", {{"name", f.name}}});
        spend_use(f, code);
        for (const auto id : targets)
        {
            auto &target = *find(id);
            nonweapon_damage(target, f.numbers.max_hit_points, 2,
                             saving_throw(target, 3, 0, &f, element), element, 12, 0, false);
        }
        end_actions(f);
        return true;
    };
    // A black dragon breathes acid along 6 squares, never through its own;
    // a red one fire along 9, three wide (DragonBreathAcid, DragonBreathFire).
    if (breathe(290, 6, 1, 0x30, true) || breathe(328, 9, 3, 0x21, false))
    {
        return;
    }
    auto *lock = find(f.lock);
    // A hell hound breathes 7 of fire at one beside it, half the time.
    if (f.has(331) && acquire_target(f, 12, false, false) && (lock = find(f.lock)) &&
        distance(f, *lock) <= 1 && roll(100) <= 50)
    {
        log({"{name} breathes fire.", {{"name", f.name}}});
        nonweapon_damage(*lock, 7, 2, saving_throw(*lock, 3, 0, &f, 1), 0x21, 12, 0, false);
        end_actions(f);
        return;
    }
    // Tyranthraxus throws lightning in the first four rounds: 16d6 along
    // the bolt and again on its way back (cast_throw_lightening).
    if (f.has(332) && round_ <= 4 && acquire_target(f, 12, false, false))
    {
        const Cell aim = find(f.lock)->cell;
        while (f.has(effect::invisible))
        {
            f.remove(effect::invisible);
        }
        log({"{name} throws lightning.", {{"name", f.name}}});
        const auto bolt = [this]
        {
            int total = 0;
            for (int die = 0; die < 16; ++die)
            {
                total += roll(6);
            }
            return total;
        };
        lightning_at(aim, bolt(), 12);
        lightning_path(f.cell, aim, 8, bolt(), 12, true);
        end_actions(f);
        return;
    }
    // A giant slug spits acid as far as 6 squares, 3 times in 10, for its
    // Hit Points, saved against for half (AffectSpitAcid); Curse left it
    // untyped, as Pool of Radiance's ankheg (an obvious bug, fixed: acid).
    // Gateway's for 1d12 (its EffSpitAcidBreath), Treasures' for 1d13+3
    // (its EffSpitAcid).
    if ((f.has(286) || f.has(effect::slug_acid)) && acquire_target(f, 12, false, false) &&
        (lock = find(f.lock)) && distance(f, *lock) <= 6)
    {
        if (roll(100) <= 30)
        {
            log({"{name} spits acid.", {{"name", f.name}}});
            const int damage = gateway()                  ? roll(12)
                               : f.has(effect::slug_acid) ? roll(13) + 3
                                                          : f.numbers.max_hit_points;
            nonweapon_damage(*lock, damage, 2, saving_throw(*lock, 3, 0, &f, 16), 16, 12, 0, false);
        }
        else
        {
            log({"{name} spits acid and misses.", {{"name", f.name}}});
        }
    }
    // A paralyzing gaze: saved against as petrification, or held for an hour
    // (cast_gaze_paralyze).
    if (f.has(326) && acquire_target(f, 12, false, false) && (lock = find(f.lock)) &&
        sees(board_, f, *lock))
    {
        log({"{name} gazes.", {{"name", f.name}}});
        if (!saving_throw(*lock, 1, 0, &f))
        {
            log({"{name} is paralyzed.", {{"name", lock->name}}});
            lock->numbers.effects.push_back({effect::paralysis, 60, 12, false});
        }
    }
    if (f.has(287))
    {
        beholder_rays(f);
    }
}

// A beholder's eyes (Curse's attack_or_kill): up to four tries, each power
// once a fight, by the target's distance: a disintegrating ray at exactly 2
// squares, a petrifying one at 3, death at 4, a 2d8+1 wound at 5; then Fear,
// Slow and Sleep. The kit's (cbaction.c, eye_rays; its 117) and Pools of
// Darkness's (cbtacts.pas, AttackEyeRays; its 117, as its AI's move begins)
// lock onto an enemy anew anywhere on the field as they open, and use each
// power once a turn, a ray within its distance; the wound is Cause Serious
// Wounds.
void Session::beholder_rays(Fighter &f)
{
    const bool kit_eyes = kit() || darkness();
    int used_this_turn = 0;
    for (int tries = 0; tries < 4 && f.able(); ++tries)
    {
        const bool fresh_lock = kit_eyes && tries == 0;
        if (!acquire_target(f, kit_eyes ? 255 : 12, false, fresh_lock))
        {
            return;
        }
        auto &target = *find(f.lock);
        const int range = distance(f, target);
        const int used = kit_eyes ? used_this_turn : uses(f, 287);
        const auto power = [&](int bit)
        {
            if (used & bit)
            {
                return false;
            }
            if (kit_eyes)
            {
                used_this_turn |= bit;
            }
            else if (auto *count = f.find_effect(387))
            {
                count->strength |= bit;
            }
            else
            {
                f.numbers.effects.push_back({387, 0, bit, false});
            }
            return true;
        };
        const auto reaches = [&](int squares)
        {
            return kit_eyes ? range <= squares : range == squares;
        };
        if (reaches(2) && power(1))
        {
            log({"{name} fires a disintegrating ray.", {{"name", f.name}}});
            if (!saving_throw(target, 3, 0, &f))
            {
                target.condition = Condition::gone;
                target.hit_points = 0;
                target.initiative = 0;
                log({"{name} is disintegrated.", {{"name", target.name}}});
                end_effects_at_zero(target);
                went_down(target);
            }
        }
        else if (reaches(3) && power(2))
        {
            log({"{name} fires a petrifying ray.", {{"name", f.name}}});
            if (!saving_throw(target, 1, 0, &f))
            {
                turn_to_stone(target);
            }
        }
        else if (reaches(4) && power(4))
        {
            log({"{name} fires a death ray.", {{"name", f.name}}});
            if (!saving_throw(target, 0, 0, &f))
            {
                log({"{name} is slain.", {{"name", target.name}}});
                kill(target);
            }
        }
        else if (reaches(5) && power(8))
        {
            log({"{name} fires a wounding ray.", {{"name", f.name}}});
            constexpr int cause_serious_wounds = 66;
            const int wound =
                kit_eyes ? canonical_spell(cause_serious_wounds, game_of(identity_)) : 0;
            nonweapon_damage(target, roll(8) + roll(8) + 1, 0, false, 8, 12, wound, false);
        }
        else
        {
            for (const auto [bit, number] :
                 {std::pair{16, 184}, std::pair{32, 55}, std::pair{64, 21}})
            {
                if (power(bit))
                {
                    f.aiming = number;
                    f.using_item = -1;
                    resolve_spell(f, {target.id}, target.cell);
                    break;
                }
            }
        }
    }
}

// Curse's conditions at a turn's start (its reconstruction's ovr013): one
// entangled cannot move; a feebleminded one cannot cast; one fighting snakes
// spends its turn on them until its attacks wear them out; a confused one
// rolls what it does, then may shake it off.
void Session::curse_turn_start(Fighter &f)
{
    if (!f.able())
    {
        return;
    }
    if (f.has(336))
    {
        f.half_moves = 0;
    }
    // Gateway's held, engulfed or hugged cannot cast (its EffImmobile, at
    // ACTION_START).
    if (gateway() && f.has(effect::immobile))
    {
        f.cast_allowed = false;
    }
    if (f.has(268))
    {
        f.cast_allowed = false;
        f.item_use_allowed = false;
    }
    if (auto *snakes = f.find_effect(203))
    {
        log({"{name} is fighting with snakes.", {{"name", f.name}}});
        snakes->strength -= f.attacks_left[0] + f.attacks_left[1];
        if (snakes->strength <= 0)
        {
            f.remove(203);
        }
        end_actions(f);
        return;
    }
    if (f.has(235))
    {
        const int roll_100 = roll(100);
        if (roll_100 <= 10)
        {
            // It breaks and runs, afraid for 10 rounds.
            f.remove(235);
            f.fleeing = true;
            f.numbers.effects.push_back({342, 10, 0, true});
            log({"{name} flees.", {{"name", f.name}}});
            return;
        }
        if (roll_100 <= 60)
        {
            log({"{name} is confused.", {{"name", f.name}}});
            end_actions(f);
        }
        else if (roll_100 <= 80)
        {
            // Berserk for a round: against the nearest creature, of either side.
            const Fighter *nearest = nullptr;
            for (const auto &other : fighters_)
            {
                if (other.id != f.id && other.able() &&
                    (!nearest || distance(f, other) < distance(f, *nearest)))
                {
                    nearest = &other;
                }
            }
            if (nearest)
            {
                log({"{name} goes berserk.", {{"name", f.name}}});
                f.own_side = f.has(effect::charm) ? f.own_side : f.side;
                f.side = nearest->side == Side::party ? Side::opposition : Side::party;
                f.lock = nearest->id;
                f.numbers.effects.push_back({337, 1, 0, true});
            }
        }
        else
        {
            log({"{name} is enraged.", {{"name", f.name}}});
        }
        if (saving_throw(f, 4, -2))
        {
            f.remove(235);
        }
    }
}

// Native knights may take living NPC allies off Quick at combat start.
void Session::native_leadership()
{
    if (game_of(identity_) != GoldBoxGame::dark_queen_of_krynn)
    {
        return;
    }
    // Native combat_leadership_apply resets Quick at each new fight, then
    // lets each living PC knight try NPCs not already following a leader.
    for (auto &f : fighters_)
    {
        f.remove(dq_effect::led_by_knight);
    }
    static constexpr std::array odds{20, 40, 80};
    for (const auto &leader : fighters_)
    {
        if (leader.side != Side::party || !leader.able() || !leader.record ||
            leader.record->status != 0 || (leader.record->npc_flag & 0x80) != 0 ||
            leader.numbers.class_levels[static_cast<std::size_t>(ClassSlot::monk)] <= 0 ||
            leader.record->knight_order < 1 || leader.record->knight_order > 3)
        {
            continue;
        }
        const int threshold =
            odds[leader.record->knight_order - 1] +
            std::max(static_cast<int>(leader.record->current_abilities.charisma) - 13, 0) * 5;
        for (auto &follower : fighters_)
        {
            if (follower.side != Side::party || !follower.able() || !follower.record ||
                follower.record->status != 0 || (follower.record->npc_flag & 0x80) == 0 ||
                follower.record->current_abilities.intelligence <= 3 ||
                follower.has(dq_effect::led_by_knight) || threshold <= roll(100))
            {
                continue;
            }
            follower.numbers.effects.push_back({dq_effect::led_by_knight, 0, 255, false});
            log({"{leader} leads {name}.", {{"leader", leader.name}, {"name", follower.name}}});
        }
    }
}

void Session::aurak_final_form(Fighter &f, bool revived)
{
    if (game_of(identity_) == GoldBoxGame::dark_queen_of_krynn)
    {
        f.remove(effect::aurak_energy);
    }
    for (const auto code :
         {effect::aurak_madness, effect::aurak_transformation, effect::aurak_immolation,
          effect::aurak_combat_bonus, effect::aurak_transformation_timer})
    {
        f.remove(code);
    }
    if (revived)
    {
        stand_up(f, 20, "{name} becomes a blazing shape.");
    }
    else
    {
        log({"{name} becomes a blazing shape.", {{"name", f.name}}});
    }
    f.numbers.effects.push_back({effect::aurak_invulnerable, 0, 255, false});
    f.numbers.effects.push_back({effect::aurak_explosion, 3, 255, true});
    if (revived || game_of(identity_) != GoldBoxGame::dark_queen_of_krynn)
    {
        f.numbers.effects.push_back({effect::aurak_zap, 0, 255, false});
    }
}

void Session::draconian_explosion(Fighter &f, int dice, bool stuns)
{
    const bool native_enchanted = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
                                  f.has(dq_effect::enchanted_draconian) && stuns;
    const int die_faces = native_enchanted ? 8 : 6;
    if (native_enchanted)
    {
        dice = 16;
    }
    for (const auto code : {effect::bozak_explosion, effect::aurak_invulnerable,
                            effect::aurak_explosion, effect::aurak_zap})
    {
        f.remove(code);
    }
    log({"{name} explodes.", {{"name", f.name}}});
    put_out_of_play(f, Condition::gone);
    for (auto &other : fighters_)
    {
        if (other.id == f.id || !other.able() || distance(f, other) > 1)
        {
            continue;
        }
        int damage = 0;
        for (int count = 0; count < dice; ++count)
        {
            damage += roll(die_faces);
        }
        nonweapon_damage(other, damage, 0, false, 0, 0, 0, false);
        if (stuns && other.able() && !saving_throw(other, 4, 0) &&
            !(game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
              (other.has(effect::aurak_madness) || other.has(effect::aurak_transformation) ||
               other.has(effect::aurak_explosion))))
        {
            other.numbers.effects.push_back({effect::stunned, 0, 255, false});
            log({"{name} is stunned.", {{"name", other.name}}});
        }
    }
}

void Session::kender_yell(Fighter &f)
{
    // Once a fight, even when every opponent saves; the mark is checkpointed
    // with ordinary combat effects and does not carry into another fight.
    f.numbers.effects.push_back({effect::kender_yelled, 0, 0, false});
    log({"{name} yells a taunt.", {{"name", f.name}}});
    for (auto &other : fighters_)
    {
        if (other.side != f.side && other.able() && !other.helpless() &&
            other.numbers.intelligence > 3 && !saving_throw(other, 4, 0, &f))
        {
            other.numbers.effects.push_back(
                {effect::kender_taunted, 0, static_cast<int>(f.id), false});
            log({"{name} wants to kill {yeller}.", {{"name", other.name}, {"yeller", f.name}}});
        }
    }
    end_actions(f);
}

void Session::champions_turn_start(Fighter &f)
{
    if (!champions() || !f.able())
    {
        return;
    }
    if (f.has(dkk_effect::regenerate_three))
    {
        f.hit_points = std::min(f.hit_points + 3, f.numbers.max_hit_points);
    }
    if (f.has(dkk_effect::immolation))
    {
        for (auto &other : fighters_)
        {
            if (other.id != f.id && other.able() && distance(f, other) <= 1)
            {
                nonweapon_damage(other, roll(6), 0, false, 1, 0, 0, false);
            }
        }
    }
    if (f.has(effect::death_knight_gate) && f.hit_points < f.numbers.max_hit_points / 2)
    {
        f.remove(effect::death_knight_gate);
        for (auto &reserve : fighters_)
        {
            if (reserve.side != f.side || reserve.arrival_round != gated_reserve_round)
            {
                continue;
            }
            const auto old_cell = reserve.cell;
            bool placed = false;
            for (const auto &enemy : fighters_)
            {
                if (enemy.side == f.side || !enemy.able())
                {
                    continue;
                }
                for (std::size_t direction = 0; direction < step_x.size(); ++direction)
                {
                    reserve.cell = {enemy.cell.x + step_x[direction],
                                    enemy.cell.y + step_y[direction]};
                    const auto squares = squares_of(reserve);
                    if (cell_free(reserve) &&
                        std::ranges::all_of(squares,
                                            [&](Cell cell)
                                            {
                                                return board_.contains(cell) &&
                                                       board_.at(cell) == rules::Terrain::open;
                                            }))
                    {
                        reserve.condition = Condition::okay;
                        reserve.arrival_round = 0;
                        reserve.hit_points = reserve.numbers.max_hit_points;
                        reserve.initiative = 0;
                        log({"{name} gates in.", {{"name", reserve.name}}});
                        placed = true;
                        break;
                    }
                }
                if (placed)
                {
                    break;
                }
            }
            if (!placed)
            {
                reserve.cell = old_cell;
            }
        }
    }
    if (f.has(effect::death_knight_fireball))
    {
        const auto &fireball = *game_spell(47);
        const int level = caster_level(f, fireball, game_of(identity_));
        const int range = spell_range(fireball, level);
        for (unsigned attempt = 0; attempt < 20; ++attempt)
        {
            const auto *target = secret_target(f, range, false);
            if (!target)
            {
                break;
            }
            const auto aim = target->cell;
            if (std::ranges::any_of(fighters_,
                                    [&](const Fighter &other)
                                    {
                                        return other.side == f.side && other.able() &&
                                               distance(other, *target) <= fireball.ai_radius;
                                    }))
            {
                continue;
            }
            log({"{name} throws a fireball.", {{"name", f.name}}});
            f.remove(effect::death_knight_fireball);
            const auto targets = fireball_reach(within(aim, fireball.area & 7), aim);
            int damage = 0;
            for (unsigned die = 0; die < 15; ++die)
            {
                damage += roll(6);
            }
            cast_generic(f, 47, level, targets, damage, 9);
            break;
        }
    }
    if (f.has(effect::death_knight_dispel) && roll(20) == 1)
    {
        const auto &spell = *game_spell(41);
        const int level = caster_level(f, spell, game_of(identity_));
        if (const auto *target = secret_target(f, spell_range(spell, level), false))
        {
            log({"{name} invokes Dispel Magic.", {{"name", f.name}}});
            for (const auto id : within(target->cell, spell.area & 7))
            {
                dispel(*find(id), level);
            }
        }
    }
    if (f.has(effect::ghast_stench) || f.has(effect::death_knight_fear))
    {
        for (auto &other : fighters_)
        {
            if (other.side == f.side || !other.able() || distance(f, other) > 1)
            {
                continue;
            }
            if (f.has(effect::ghast_stench) && !other.has(effect::champions_sickened) &&
                !saving_throw(other, 4, 0, &f))
            {
                other.numbers.effects.push_back({effect::champions_sickened, 0, 255, false});
                log({"{name} is affected by the evil stench.", {{"name", other.name}}});
            }
            if (f.has(effect::death_knight_fear) && !other.has(effect::fearless) &&
                !other.has(effect::fear) && !saving_throw(other, 4, 0, &f))
            {
                if (add_with_message(other, {effect::fear, 0, 255, true}, false, false, 0,
                                     f.numbers.level, game_spell(184)))
                {
                    other.turned = true;
                }
            }
        }
    }
    if (f.has(effect::aurak_immolation))
    {
        // Native's frenzy burns only the other side, with fire (effects.c,
        // fx_immolate).
        const bool native = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn;
        for (auto &other : fighters_)
        {
            if (other.id != f.id && other.able() && distance(f, other) <= 1 &&
                !(native && other.side == f.side))
            {
                nonweapon_damage(other, roll(6), 0, false, native ? 1 : 0, 0, 0, false);
            }
        }
    }
    if (f.has(effect::aurak_zap))
    {
        for (auto &other : fighters_)
        {
            if (other.side != f.side && other.able() && distance(f, other) <= 1)
            {
                log({"{name} is struck by an energy bolt.", {{"name", other.name}}});
                const bool native_enchanted =
                    game_of(identity_) == GoldBoxGame::dark_queen_of_krynn &&
                    f.has(dq_effect::enchanted_draconian);
                int damage = 0;
                for (int die = 0; die < (native_enchanted ? 8 : 2); ++die)
                {
                    damage += roll(native_enchanted ? 8 : 6);
                }
                nonweapon_damage(other, damage, 0, false, 0, 0, 0, false);
                end_actions(f);
                break;
            }
        }
    }
    if (game_of(identity_) != GoldBoxGame::dark_queen_of_krynn && f.able() && f.initiative > 0 &&
        f.has(effect::aurak_energy) && roll(100) <= 75)
    {
        auto *power = f.find_effect(effect::aurak_energy);
        if (power->strength > 0 && roll(100) <= 25)
        {
            Fighter *target = nullptr;
            for (auto &other : fighters_)
            {
                if (other.side != f.side && other.able() && distance(f, other) <= 1)
                {
                    target = &other;
                }
            }
            if (target)
            {
                --power->strength;
                log({"{name} breathes a blinding cloud.", {{"name", f.name}}});
                const bool saved = saving_throw(*target, 3, 0, &f);
                nonweapon_damage(*target, saved ? 10 : 20, 0, false, 0, 0, 0, false);
                if (!saved)
                {
                    (void)add_with_message(*target, {effect::blind, 0, 0, false}, false, false, 0,
                                           0, nullptr);
                    end_actions(f);
                }
                return;
            }
        }
        if (auto *target = secret_target(f, 6, true))
        {
            log({"{name} fires an energy bolt.", {{"name", f.name}}});
            const bool targeted = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn;
            nonweapon_damage(*target, roll(8) + 2, 0, false, 0, 0, 0, targeted);
            if (target->able())
            {
                nonweapon_damage(*target, roll(8) + 2, 0, false, 0, 0, 0, targeted);
            }
            end_actions(f);
        }
    }
}

} // namespace opengold::original::detail
