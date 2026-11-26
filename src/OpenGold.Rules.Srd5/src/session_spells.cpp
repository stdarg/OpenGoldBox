#include "session.h"
#include "scope_guards.h"
#include <limits>
#include <numbers>

// Session: casting a spell, its riders and attacks, the spells offered, and
// the class features cast like spells.
namespace opengold::srd5::detail
{
using namespace rules;

void Session::apply_rider(const detail::SpellDef &spell, std::string_view verb, Actor &a,
                          Actor &target, int dc)
{
    // Each rider keeps its own duration rule and its own log line; the wording
    // is unchanged so the message catalogue does not move.
    switch (spell.rider)
    {
    case detail::Rider::none:
    case detail::Rider::entangle:
    case detail::Rider::fog_cloud:
    case detail::Rider::silence:
    case detail::Rider::thunderwave:
    case detail::Rider::petrifying_gaze:
        return; // Area spells resolve through cast_area(), the gaze through petrifying_gaze().
    case detail::Rider::spiritual_weapon:
    case detail::Rider::arcane_sword:
    case detail::Rider::ice_knife:
        return; // Its force or sword is placed, or its shard bursts, with the attack.
    case detail::Rider::abjure_foes:
    case detail::Rider::dispel_magic:
    case detail::Rider::guardian_of_faith:
    case detail::Rider::magic_circle:
    case detail::Rider::revivify:
    case detail::Rider::mass_healing_word:
    case detail::Rider::contingency:
        return; // Resolved by abjure(), the aimed cast, revivify() or camp, not as a rider.
    case detail::Rider::ray_of_sickness:
    {
        // Poisoned until the end of the caster's next turn.
        const auto index = index_of(a.source.id);
        const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
        detail::apply_poisoned(target.effects, effect_source(a), a.source.name,
                               next_turn_ms(a) + slot);
        log(target.source.name + " is Poisoned.",
            {"{name} is Poisoned.", {{"name", target.source.name}}});
        return;
    }
    case detail::Rider::resistance:
    {
        const auto type = *detail::resistance_type(verb);
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::resistance, static_cast<int>(type));
        const auto name = std::string(detail::damage_name(type));
        log(target.source.name + " gains Resistance against " + name + " damage.",
            {"{name} gains Resistance against {type} damage.",
             {{"name", target.source.name}, {"type", name, true}}});
        return;
    }
    case detail::Rider::guidance:
    {
        const int skill = *detail::guidance_skill(verb);
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::guidance, skill);
        const auto name = std::string(skill == 0 ? "Athletics" : "Medicine");
        log(target.source.name + " gains Guidance for " + name + " checks.",
            {"{name} gains Guidance for {skill} checks.",
             {{"name", target.source.name}, {"skill", name, true}}});
        return;
    }
    case detail::Rider::sanctuary:
        detail::apply_sanctuary(target.effects, effect_source(a), a.source.name, dc);
        log(target.source.name + " gains Sanctuary.",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", "Sanctuary", true}}});
        return;
    case detail::Rider::warding_bond:
    {
        // A new bond ends any other on either creature.
        for (auto &other : actors_)
        {
            std::erase_if(other.effects.active,
                          [&](const auto &e)
                          {
                              return e.kind == detail::EffectKind::warding_bond &&
                                     ((e.source_scope == scope_ && e.source_actor == a.source.id) ||
                                      other.source.id == target.source.id);
                          });
        }
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::warding_bond, 0);
        log(target.source.name + " gains Warding Bond.",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", "Warding Bond", true}}});
        return;
    }
    case detail::Rider::protection_from_poison:
        std::erase_if(target.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::poisoned ||
                                 e.kind == detail::EffectKind::envenomed;
                      });
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::protection_from_poison, 0);
        log(target.source.name + " gains Protection from Poison.",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", "Protection from Poison", true}}});
        return;
    case detail::Rider::mage_armor:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::mage_armor, 0);
        log(target.source.name + " gains Mage Armor.",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", "Mage Armor", true}}});
        return;
    case detail::Rider::false_life:
    {
        // 2d4 + 4, and 5 more for each slot level above 1. Temporary Hit
        // Points do not stack: the creature keeps the better or is asked, as
        // for Adrenaline Rush.
        TemporaryHitPoints offered{
            dice({2, 4, 4}) + 5 * static_cast<int>(upcast_steps(verb, spell)), "spell:false_life"};
        log(target.source.name + " gains False Life.",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", "False Life", true}}});
        if (target.life.temporary_hp.amount)
        {
            temporary_offer_ = std::move(offered);
        }
        else
        {
            detail::grant_temporary_hp(target.life, offered, TemporaryHpChoice::use_new);
        }
        return;
    }
    case detail::Rider::expeditious_retreat:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::expeditious_retreat, 0);
        log(target.source.name + " gains Expeditious Retreat.",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", "Expeditious Retreat", true}}});
        target.movement += def(target).speed;
        ++target.dashes;
        return;
    case detail::Rider::hideous_laughter:
        detail::apply_repeating_condition(
            target.effects, detail::EffectKind::laughing, effect_source(a), a.source.name,
            detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(target.source.id)});
        target.effects.prone = true;
        log(target.source.name + " falls Prone, laughing.",
            {"{name} falls Prone, laughing.", {{"name", target.source.name}}});
        end_incapacitated_effects(target);
        return;
    case detail::Rider::sleep:
    case detail::Rider::color_spray:
    case detail::Rider::grease:
    case detail::Rider::web:
    case detail::Rider::misty_step:
        return; // Aimed areas, resolved by cast_area().
    case detail::Rider::invisibility:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::invisible, 0);
        log(target.source.name + " turns Invisible.",
            {"{name} turns Invisible.", {{"name", target.source.name}}});
        return;
    case detail::Rider::see_invisibility:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::see_invisibility, 0);
        log(target.source.name + " gains See Invisibility.",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", "See Invisibility", true}}});
        return;
    case detail::Rider::darkness:
    case detail::Rider::gust_of_wind:
    case detail::Rider::flaming_sphere:
    case detail::Rider::moonbeam:
    case detail::Rider::spike_growth:
        return; // Aimed areas, resolved by cast_area().
    case detail::Rider::heat_metal:
        return; // Resolved by heat_metal().
    case detail::Rider::flame_blade:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::flame_blade, 0);
        target.effects.active.back().slot = slot_level_of(verb, spell);
        log(target.source.name + " gains Flame Blade.",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", "Flame Blade", true}}});
        return;
    case detail::Rider::vicious_mockery:
        // Disadvantage on its next attack roll before the end of its next turn.
        detail::apply_attack_mastery(target.effects, detail::EffectKind::sap, effect_source(a),
                                     a.source.name, next_save_ms(target.source.id));
        log(target.source.name + " has Disadvantage on its next attack roll.",
            {"{name} has Disadvantage on its next attack roll.", {{"name", target.source.name}}});
        return;
    case detail::Rider::dissonant_whispers:
        // Its Reaction carries it as far from the caster as it can.
        if (!target.reaction || detail::incapacitated(target.effects))
        {
            return;
        }
        {
            target.reaction = false;
            log(target.source.name + " flees from " + a.source.name + ".",
                {"{name} flees from {caster}.",
                 {{"name", target.source.name}, {"caster", a.source.name}}});
            // It moves with its Reaction, so leaving a reach provokes as any move.
            const int budget =
                std::max(0, def(target).speed - detail::speed_penalty(target.effects));
            auto route = flight_route(a, target, budget);
            if (route.empty() || !path_.empty())
            {
                return;
            }
            flight_ = Flight{target.source.id, budget};
            path_ = std::move(route);
            path_index_ = 0;
            progress_movement();
            return;
        }
    case detail::Rider::starry_wisp:
    {
        const auto index = index_of(a.source.id);
        const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
        detail::apply_poisoned(target.effects, effect_source(a), a.source.name,
                               next_turn_ms(a) + slot, detail::EffectKind::lit);
        return;
    }
    case detail::Rider::faerie_fire:
    case detail::Rider::conjure_animals:
    case detail::Rider::wind_wall:
    case detail::Rider::plant_growth:
        return; // An aimed area, resolved by cast_area().
    case detail::Rider::shillelagh:
    case detail::Rider::produce_flame:
    {
        // Casting it again ends the earlier one.
        const auto kind = spell.rider == detail::Rider::shillelagh
                              ? detail::EffectKind::shillelagh
                              : detail::EffectKind::produce_flame;
        std::erase_if(target.effects.active,
                      [&](const auto &e)
                      {
                          return e.kind == kind;
                      });
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name, kind, 0);
        log(target.source.name + " gains " + std::string(spell.label) + ".",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", std::string(spell.label), true}}});
        return;
    }
    case detail::Rider::stoneskin:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::stoneskin, 0);
        log(target.source.name + " gains Stoneskin.",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", "Stoneskin", true}}});
        return;
    case detail::Rider::conjure_woodland_beings:
    {
        // The spirits strike only creatures that later come near or stay.
        Zone beings{a.source.id, ZoneKind::beings, {a.source.cell}};
        beings.save_dc = spell_dc(a, spell.id);
        beings.slot = slot_level_of(verb, spell);
        zones_.push_back(std::move(beings));
        log("Woodland spirits flit around " + a.source.name + ".",
            {"Woodland spirits flit around {name}.", {{"name", a.source.name}}});
        return;
    }
    case detail::Rider::dominate_beast:
    case detail::Rider::dominate_person:
    case detail::Rider::dominate_monster:
        dominate(a, target, dc);
        return;
    case detail::Rider::call_lightning:
    case detail::Rider::giant_insect:
    case detail::Rider::wall_of_thorns:
    case detail::Rider::natures_sanctuary:
    case detail::Rider::finger_of_death:
    case detail::Rider::delayed_blast_fireball:
    case detail::Rider::forcecage:
    case detail::Rider::prismatic_spray:
    case detail::Rider::reverse_gravity:
    case detail::Rider::incendiary_cloud:
        return; // Resolved with its damage, or an aimed area.
    case detail::Rider::befuddlement:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::befuddled, dc);
        log(target.source.name + " is befuddled: no spells or Magic actions.",
            {"{name} is befuddled: no spells or Magic actions.", {{"name", target.source.name}}});
        return;
    case detail::Rider::maze:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::mazed, 0);
        log(target.source.name + " is lost in a maze.",
            {"{name} is lost in a maze.", {{"name", target.source.name}}});
        end_incapacitated_effects(target);
        return;
    case detail::Rider::conjure_fey:
        fey_frighten(a, target);
        return;
    case detail::Rider::antilife_shell:
        zones_.push_back({a.source.id, ZoneKind::antilife, {a.source.cell}});
        log("An aura of antilife surrounds " + a.source.name + ".",
            {"An aura of antilife surrounds {name}.", {{"name", a.source.name}}});
        return;
    case detail::Rider::mind_blank:
        std::erase_if(target.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::mind_blank;
                      });
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::mind_blank, 0);
        log(target.source.name + " gains Mind Blank.",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", "Mind Blank", true}}});
        return;
    case detail::Rider::power_word_stun:
        // Stunned at 150 Hit Points or fewer; otherwise Speed 0 until the
        // start of the caster's next turn.
        if (target.life.hp <= 150)
        {
            detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                        detail::EffectKind::word_stunned, dc);
            log(target.source.name + " is Stunned.",
                {"{name} is Stunned.", {{"name", target.source.name}}});
            end_incapacitated_effects(target);
            return;
        }
        detail::apply_poisoned(target.effects, effect_source(a), a.source.name, next_turn_ms(a),
                               detail::EffectKind::rooted);
        log(target.source.name + " cannot move until " + a.source.name + "'s next turn.",
            {"{name} cannot move until {caster}'s next turn.",
             {{"name", target.source.name}, {"caster", a.source.name}}});
        return;
    case detail::Rider::disintegrate:
    case detail::Rider::wall_of_ice:
        return; // Resolved with its damage, and an aimed area.
    case detail::Rider::eyebite:
    {
        // Asleep (Sleep's Unconscious), panicked or sickened, as chosen.
        const auto effect =
            detail::without_cast_suffix(verb).substr(std::string_view("eyebite_").size());
        if (effect == "asleep")
        {
            detail::apply_asleep(target.effects, effect_source(a), a.source.name);
        }
        else
        {
            detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                        effect == "panicked" ? detail::EffectKind::eye_panic
                                                             : detail::EffectKind::sickened,
                                        0);
        }
        if (!detail::has_effect(a.effects, detail::EffectKind::eyebiting))
        {
            detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                        detail::EffectKind::eyebiting, 0);
        }
        log(target.source.name + (effect == "asleep"     ? " falls asleep."
                                  : effect == "panicked" ? " panics."
                                                         : " is sickened."),
            {effect == "asleep"     ? "{name} falls asleep."
             : effect == "panicked" ? "{name} panics."
                                    : "{name} is sickened.",
             {{"name", target.source.name}}});
        return;
    }
    case detail::Rider::flesh_to_stone:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::stoning, 0);
        log(target.source.name + " begins to turn to stone.",
            {"{name} begins to turn to stone.", {{"name", target.source.name}}});
        return;
    case detail::Rider::irresistible_dance:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::dancing, 0);
        log(target.source.name + " breaks into a helpless dance.",
            {"{name} breaks into a helpless dance.", {{"name", target.source.name}}});
        return;
    case detail::Rider::magic_jar:
        // The caster's soul takes the body (LEVELS-3); its own lies helpless.
        dominate(a, target, dc);
        if (detail::can_apply(a.effects))
        {
            detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                        detail::EffectKind::jarred, 0);
        }
        return;
    case detail::Rider::globe_of_invulnerability:
    {
        // Still, where it was raised; it wards against spells of level 5 or
        // lower, one more per slot level above 6.
        std::vector<Cell> cells;
        for (int y = 0; y < board_.height; ++y)
        {
            for (int x = 0; x < board_.width; ++x)
            {
                if (distance(a, Cell{x, y}) <= 10)
                {
                    cells.push_back({x, y});
                }
            }
        }
        Zone globe{a.source.id, ZoneKind::globe, std::move(cells)};
        globe.slot = slot_level_of(verb, spell);
        zones_.push_back(std::move(globe));
        log("A shimmering globe rises around " + a.source.name + ".",
            {"A shimmering globe rises around {name}.", {{"name", a.source.name}}});
        return;
    }
    case detail::Rider::arcane_hand:
    case detail::Rider::cloudkill:
    case detail::Rider::conjure_elemental:
    case detail::Rider::summon_dragon:
    case detail::Rider::wall_of_force:
    case detail::Rider::wall_of_stone:
    case detail::Rider::telekinesis:
        return; // An aimed area, resolved by cast_area().
    case detail::Rider::animate_objects:
    {
        // As many Medium objects as the spellcasting modifier (LEVELS-1),
        // rising in the free squares nearest the caster.
        const auto squares = free_squares_near(a.source.cell);
        const auto count = std::min<std::size_t>(
            static_cast<std::size_t>(std::max(1, casting_modifier(def(a)))), squares.size());
        const auto key = "animated-object-" + std::to_string(slot_level_of(verb, spell));
        for (std::size_t n = 0; n < count; ++n)
        {
            (void)summon(a, key, "Animated Object", squares[n]);
        }
        log(std::to_string(count) + " objects animate for " + a.source.name + ".",
            {"{count} objects animate for {name}.",
             {{"count", std::to_string(count)}, {"name", a.source.name}}});
        return;
    }
    case detail::Rider::mislead:
    {
        // Invisible, with an illusory double in the nearest free square.
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::invisible, 0);
        const auto squares = free_squares_near(a.source.cell);
        if (!squares.empty())
        {
            auto &twin = summon(a, "illusory-double", a.source.name, squares.front());
            detail::apply_spell_benefit(twin.effects, effect_source(a), a.source.name,
                                        detail::EffectKind::illusory, 0);
        }
        log(a.source.name + " turns Invisible, and an illusory double appears.",
            {"{name} turns Invisible, and an illusory double appears.", {{"name", a.source.name}}});
        return;
    }
    case detail::Rider::spirit_guardians:
    {
        // The guardians strike only creatures that later come near or stay.
        Zone guardians{a.source.id, ZoneKind::guardians, {a.source.cell}};
        guardians.save_dc = spell_dc(a, spell.id);
        guardians.slot = slot_level_of(verb, spell);
        zones_.push_back(std::move(guardians));
        log("Spirit guardians flit around " + a.source.name + ".",
            {"Spirit guardians flit around {name}.", {{"name", a.source.name}}});
        return;
    }
    case detail::Rider::bestow_curse:
    {
        // From a level-5 slot the curse needs no Concentration: 10 more.
        const int curse = *detail::curse_of(verb);
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::cursed,
                                    curse + (slot_level_of(verb, spell) >= 5 ? 10 : 0));
        log(target.source.name + " is cursed: " + curse_label(static_cast<std::size_t>(curse)) +
                ".",
            {"{name} is cursed: {curse}.",
             {{"name", target.source.name},
              {"curse", curse_label(static_cast<std::size_t>(curse)), true}}});
        return;
    }
    case detail::Rider::animate_dead:
        begin_animating(a, target, spell, slot_level_of(verb, spell));
        return;
    case detail::Rider::contagion:
    {
        const int amount = resolved_damage(target, spell.damage, dice(spell.dice));
        log(target.source.name + " takes " + std::to_string(amount) + " Necrotic damage.",
            {"{name} takes {damage} {type} damage.",
             {{"name", target.source.name},
              {"damage", std::to_string(amount)},
              {"type", "Necrotic", true}}});
        damage(target, amount);
        if (target.life.dead || !detail::can_apply(target.effects))
        {
            return;
        }
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::contagion,
                                    static_cast<int>(*detail::contagion_ability(verb)));
        log(target.source.name + " is Poisoned by the contagion.",
            {"{name} is Poisoned by the contagion.", {{"name", target.source.name}}});
        return;
    }
    case detail::Rider::dispel_evil_and_good:
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::dispel_evil, 0);
        log(a.source.name + " gains Dispel Evil and Good.",
            {"{name} gains {spell}.",
             {{"name", a.source.name}, {"spell", "Dispel Evil and Good", true}}});
        return;
    case detail::Rider::greater_restoration:
    {
        const auto harm = *detail::greater_harm(verb);
        if (detail::greater_harms[harm] == "charmed")
        {
            std::erase_if(target.effects.active,
                          [](const auto &e)
                          {
                              return e.kind == detail::EffectKind::charmed ||
                                     e.kind == detail::EffectKind::enthralled ||
                                     e.kind == detail::EffectKind::turn_charmed ||
                                     e.kind == detail::EffectKind::compelled ||
                                     e.kind == detail::EffectKind::dominated;
                          });
            release(target);
            log(target.source.name + " is no longer Charmed.",
                {"{name} is no longer Charmed.", {{"name", target.source.name}}});
        }
        else if (detail::greater_harms[harm] == "befuddled")
        {
            std::erase_if(target.effects.active,
                          [](const auto &e)
                          {
                              return e.kind == detail::EffectKind::befuddled;
                          });
            log(target.source.name + "'s mind clears.",
                {"{name}'s mind clears.", {{"name", target.source.name}}});
        }
        else if (detail::greater_harms[harm] == "petrified")
        {
            std::erase_if(target.effects.active,
                          [](const auto &e)
                          {
                              return e.kind == detail::EffectKind::petrified ||
                                     e.kind == detail::EffectKind::stoning;
                          });
            log(target.source.name + " is flesh again.",
                {"{name} is flesh again.", {{"name", target.source.name}}});
        }
        else if (detail::greater_harms[harm] == "drained")
        {
            std::erase_if(target.effects.active,
                          [](const auto &e)
                          {
                              return e.kind == detail::EffectKind::drained ||
                                     e.kind == detail::EffectKind::life_drained;
                          });
            log(target.source.name + "'s Hit Point maximum is whole again.",
                {"{name}'s Hit Point maximum is whole again.", {{"name", target.source.name}}});
        }
        else
        {
            std::erase_if(target.effects.active,
                          [](const auto &e)
                          {
                              return curse(e.kind);
                          });
            log(target.source.name + "'s curses end.",
                {"{name}'s curses end.", {{"name", target.source.name}}});
        }
        return;
    }
    case detail::Rider::insect_plague:
    case detail::Rider::mass_cure_wounds:
    case detail::Rider::blade_barrier:
    case detail::Rider::sunbeam:
        return; // An aimed area, resolved by cast_area().
    case detail::Rider::harm:
        return; // Resolved with its damage.
    case detail::Rider::true_seeing:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::true_seeing, 0);
        log(target.source.name + " gains True Seeing.",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", "True Seeing", true}}});
        return;
    case detail::Rider::create_undead:
        begin_animating(a, target, spell, slot_level_of(verb, spell) + 10);
        return;
    case detail::Rider::conjure_celestial:
    case detail::Rider::earthquake:
    case detail::Rider::sunburst:
    case detail::Rider::hypnotic_pattern:
    case detail::Rider::fear:
    case detail::Rider::sleet_storm:
    case detail::Rider::stinking_cloud:
        return; // An aimed area, resolved by cast_area().
    case detail::Rider::vampiric_touch:
        return; // A touch resolved with its attack.
    case detail::Rider::conjure_minor_elementals:
    {
        const auto type = detail::damage_type(detail::without_cast_suffix(verb).substr(
            std::string_view("conjure_minor_elementals_").size()));
        Zone spirits{a.source.id, ZoneKind::elementals, {a.source.cell}};
        spirits.slot = slot_level_of(verb, spell);
        zones_.push_back(std::move(spirits));
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::minor_elementals, static_cast<int>(type));
        log("Elemental spirits flit around " + a.source.name + ".",
            {"Elemental spirits flit around {name}.", {{"name", a.source.name}}});
        return;
    }
    case detail::Rider::faithful_hound:
    case detail::Rider::black_tentacles:
    case detail::Rider::confusion:
    case detail::Rider::slow:
    case detail::Rider::tsunami:
    case detail::Rider::dimension_door:
    case detail::Rider::ice_storm:
    case detail::Rider::vitriolic_sphere:
    case detail::Rider::wall_of_fire:
        return; // An aimed area, resolved by cast_area().
    case detail::Rider::fire_shield:
    {
        const bool chill = verb.find("chill") != std::string_view::npos;
        std::erase_if(target.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::fire_shield;
                      });
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::fire_shield, chill ? 1 : 0);
        log(target.source.name +
                (chill ? " is wreathed in a chill shield." : " is wreathed in a warm shield."),
            {chill ? "{name} is wreathed in a chill shield."
                   : "{name} is wreathed in a warm shield.",
             {{"name", target.source.name}}});
        return;
    }
    case detail::Rider::greater_invisibility:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::greater_invisible, 0);
        log(target.source.name + " turns Invisible.",
            {"{name} turns Invisible.", {{"name", target.source.name}}});
        return;
    case detail::Rider::phantasmal_killer:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::haunted,
                                    spell.dice.count + static_cast<int>(upcast_steps(verb, spell)));
        log(target.source.name + " is gripped by its worst fears.",
            {"{name} is gripped by its worst fears.", {{"name", target.source.name}}});
        return;
    case detail::Rider::animal_shapes:
    {
        const auto key =
            detail::without_cast_suffix(verb).substr(std::string_view("animal_shapes_").size());
        const auto *form = detail::beast_form(key);
        const int old_speed = def(target).speed;
        std::erase_if(target.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::wild_shape ||
                                 e.kind == detail::EffectKind::polymorphed ||
                                 e.kind == detail::EffectKind::animal_shaped;
                      });
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::animal_shaped,
                                    static_cast<int>(form - detail::beast_forms.data()) + 1);
        refresh_form(target);
        target.source.cell = nearest_open(target);
        change_speed(target, old_speed);
        if (target.life.temporary_hp.amount < form->hit_points)
        {
            detail::grant_temporary_hp(target.life, {form->hit_points, "spell:animal_shapes"},
                                       TemporaryHpChoice::use_new);
        }
        log(target.source.name + " turns into a " + std::string(form->label) + ".",
            {"{name} turns into a {form}.",
             {{"name", target.source.name}, {"form", std::string(form->label), true}}});
        return;
    }
    case detail::Rider::polymorph:
    {
        const auto key =
            detail::without_cast_suffix(verb).substr(std::string_view("polymorph_").size());
        const auto *form = detail::beast_form(key);
        const int old_speed = def(target).speed;
        std::erase_if(target.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::wild_shape ||
                                 e.kind == detail::EffectKind::polymorphed;
                      });
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::polymorphed,
                                    static_cast<int>(form - detail::beast_forms.data()) + 1);
        refresh_form(target);
        target.source.cell = nearest_open(target);
        change_speed(target, old_speed);
        // Temporary Hit Points equal to the Beast's Hit Points.
        if (target.life.temporary_hp.amount < form->hit_points)
        {
            detail::grant_temporary_hp(target.life, {form->hit_points, "spell:polymorph"},
                                       TemporaryHpChoice::use_new);
        }
        log(target.source.name + " turns into a " + std::string(form->label) + ".",
            {"{name} turns into a {form}.",
             {{"name", target.source.name}, {"form", std::string(form->label), true}}});
        return;
    }
    case detail::Rider::resilient_sphere:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::sphered, 0);
        log(target.source.name + " is sealed in a shimmering sphere.",
            {"{name} is sealed in a shimmering sphere.", {{"name", target.source.name}}});
        return;
    case detail::Rider::haste:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::hasted, 0);
        log(target.source.name + " gains Haste.",
            {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Haste", true}}});
        // The doubled Speed applies at once; the second action from its next turn.
        return;
    case detail::Rider::blink:
        std::erase_if(target.effects.active,
                      [](const auto &e)
                      {
                          return e.kind == detail::EffectKind::blinking;
                      });
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::blinking, 0);
        log(target.source.name + " gains Blink.",
            {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Blink", true}}});
        return;
    case detail::Rider::divine_word:
        divine_word(a, dc);
        return;
    case detail::Rider::regenerate:
        // A minute of casting: Concentration, and the caster's Action each turn.
        begin_concentration(a, spell);
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::knitting,
                                    static_cast<int>(slot_level_of(verb, spell)));
        log(a.source.name + " begins casting Regenerate.",
            {"{name} begins casting {spell}.",
             {{"name", a.source.name}, {"spell", "Regenerate", true}}});
        return;
    case detail::Rider::antimagic_field:
        zones_.push_back({a.source.id, ZoneKind::antimagic, {a.source.cell}});
        log("An aura of antimagic surrounds " + a.source.name + ".",
            {"An aura of antimagic surrounds {name}.", {{"name", a.source.name}}});
        return;
    case detail::Rider::holy_aura:
    {
        Zone aura{a.source.id, ZoneKind::holy, {a.source.cell}};
        aura.save_dc = dc;
        zones_.push_back(std::move(aura));
        log("A holy aura shines around " + a.source.name + ".",
            {"A holy aura shines around {name}.", {{"name", a.source.name}}});
        return;
    }

    case detail::Rider::barkskin:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::barkskin, 0);
        log(target.source.name + " gains Barkskin.",
            {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Barkskin", true}}});
        return;
    case detail::Rider::hex:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::hex, 0);
        log(target.source.name + " is hexed.",
            {"{name} is hexed.", {{"name", target.source.name}}});
        return;
    case detail::Rider::charm_person:
    case detail::Rider::compulsion:
        // Mindless Rage: a raging Berserker cannot be Charmed.
        if (mindless(target))
        {
            log(target.source.name + "'s Mindless Rage shrugs off the charm.",
                {"{name}'s Mindless Rage shrugs off the charm.", {{"name", target.source.name}}});
            return;
        }
        if (aura_of_devotion(target))
        {
            log(target.source.name + " is immune to the charm (Aura of Devotion).",
                {"{name} is immune to the charm (Aura of Devotion).",
                 {{"name", target.source.name}}});
            return;
        }
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    spell.rider == detail::Rider::compulsion
                                        ? detail::EffectKind::compelled
                                        : detail::EffectKind::charmed,
                                    0);
        log(target.source.name + " is Charmed by " + a.source.name + ".",
            {"{name} is Charmed by {caster}.",
             {{"name", target.source.name}, {"caster", a.source.name}}});
        return;
    case detail::Rider::protection_from_energy:
    {
        const auto type = *detail::energy_type(verb);
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::energy_ward, static_cast<int>(type));
        log(target.source.name + " gains Resistance to " + std::string(detail::damage_name(type)) +
                " damage.",
            {"{name} gains Resistance to {type} damage.",
             {{"name", target.source.name},
              {"type", std::string(detail::damage_name(type)), true}}});
        return;
    }
    case detail::Rider::dragons_breath:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::dragons_breath,
                                    static_cast<int>(*detail::dragon_type(verb)));
        target.effects.active.back().slot = slot_level_of(verb, spell);
        log(target.source.name + " gains Dragon's Breath.",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", "Dragon's Breath", true}}});
        return;
    case detail::Rider::enlarge_reduce:
    {
        const bool enlarge = verb == "enlarge";
        detail::apply_spell_benefit(
            target.effects, effect_source(a), a.source.name,
            enlarge ? detail::EffectKind::enlarged : detail::EffectKind::reduced, 0);
        log(target.source.name + (enlarge ? " is enlarged." : " is reduced."),
            {enlarge ? "{name} is enlarged." : "{name} is reduced.",
             {{"name", target.source.name}}});
        return;
    }
    case detail::Rider::blur:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::blur, 0);
        log(target.source.name + " gains Blur.",
            {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Blur", true}}});
        return;
    case detail::Rider::mirror_image:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::mirror_image, 3);
        log(target.source.name + " gains Mirror Image.",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", "Mirror Image", true}}});
        return;
    case detail::Rider::magic_weapon:
        // Casting it again ends the earlier one.
        for (auto &other : actors_)
        {
            std::erase_if(other.effects.active,
                          [&](const auto &e)
                          {
                              return e.kind == detail::EffectKind::magic_weapon &&
                                     e.source_actor == a.source.id && e.source_scope == scope_;
                          });
        }
        // +1, +2 from a level-4 slot, +3 from a level-6 one.
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::magic_weapon,
                                    static_cast<int>(slot_level_of(verb, spell) / 2));
        log(target.source.name + " gains Magic Weapon.",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", "Magic Weapon", true}}});
        return;
    case detail::Rider::acid_arrow:
        detail::apply_poisoned(target.effects, effect_source(a), a.source.name,
                               next_save_ms(target.source.id), detail::EffectKind::acid_arrow);
        target.effects.active.back().slot = slot_level_of(verb, spell);
        return;
    case detail::Rider::ray_of_enfeeblement:
        detail::apply_repeating_condition(
            target.effects, detail::EffectKind::enfeebled, effect_source(a), a.source.name,
            detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(target.source.id)});
        log(target.source.name + " is enfeebled.",
            {"{name} is enfeebled.", {{"name", target.source.name}}});
        return;
    case detail::Rider::hold_person:
        detail::apply_hold_person(
            target.effects, effect_source(a), a.source.name,
            detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(target.source.id)});
        log(target.source.name + " is Paralyzed.",
            {"{name} is Paralyzed.", {{"name", target.source.name}}});
        end_incapacitated_effects(target);
        return;
    case detail::Rider::paralysis_ray:
        detail::apply_repeating_condition(
            target.effects, detail::EffectKind::paralysis_ray, effect_source(a), a.source.name,
            detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(target.source.id)});
        log(target.source.name + " is Paralyzed.",
            {"{name} is Paralyzed.", {{"name", target.source.name}}});
        end_incapacitated_effects(target);
        return;
    case detail::Rider::giant_strength:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::giant_strength, 0);
        refresh_form(target);
        log(target.source.name + "'s Strength becomes 21.",
            {"{name}'s Strength becomes 21.", {{"name", target.source.name}}});
        return;
    case detail::Rider::disappearance:
        // The user and each ally within 10 feet turn Invisible; attacking or
        // casting a spell ends it, as Invisibility does.
        for (auto &ally : actors_)
        {
            if (ally.source.side == a.source.side && !ally.life.dead && distance(a, ally) <= 10 &&
                detail::can_apply(ally.effects) &&
                !detail::has_effect(ally.effects, detail::EffectKind::invisible))
            {
                detail::apply_spell_benefit(ally.effects, effect_source(a), a.source.name,
                                            detail::EffectKind::invisible, 0);
                log(ally.source.name + " turns Invisible.",
                    {"{name} turns Invisible.", {{"name", ally.source.name}}});
            }
        }
        return;
    case detail::Rider::enhance_ability:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::enhance_ability, 0);
        log(target.source.name + " gains Enhance Ability (Strength).",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", "Enhance Ability (Strength)", true}}});
        return;
    case detail::Rider::missile_ward:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::missile_ward, 0);
        log(target.source.name + " gains Protection From Normal Missiles.",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", "Protection From Normal Missiles", true}}});
        return;
    case detail::Rider::snake_charm:
        charm_snakes(a);
        return;
    case detail::Rider::guiding_bolt:
    {
        // Until the end of the caster's next turn.
        const auto index = index_of(a.source.id);
        const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
        detail::apply_guiding_bolt(target.effects, effect_source(a), a.source.name,
                                   next_turn_ms(a) + slot);
        log(target.source.name + " is lit by Guiding Bolt.",
            {"{name} is lit by Guiding Bolt.", {{"name", target.source.name}}});
        return;
    }
    case detail::Rider::bane:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::bane, 0);
        log(target.source.name + " is weakened by Bane.",
            {"{name} is weakened by Bane.", {{"name", target.source.name}}});
        return;
    case detail::Rider::remove_curse:
        std::erase_if(target.effects.active,
                      [](const auto &e)
                      {
                          return curse(e.kind);
                      });
        log(target.source.name + "'s curses end.",
            {"{name}'s curses end.", {{"name", target.source.name}}});
        if (bound_to_cursed_item(target, true))
        {
            target.curse_lifted = true;
            log(target.source.name + " can put down the cursed weapon.",
                {"{name} can put down the cursed weapon.", {{"name", target.source.name}}});
        }
        else if (bound_to_cursed_item(target))
        {
            target.curse_lifted = true;
            log(target.source.name + " can take off the cursed item.",
                {"{name} can take off the cursed item.", {{"name", target.source.name}}});
        }
        return;
    case detail::Rider::banishment:
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::banished, 0);
        log(target.source.name + " is banished.",
            {"{name} is banished.", {{"name", target.source.name}}});
        end_incapacitated_effects(target);
        return;
    case detail::Rider::death_ward:
    case detail::Rider::freedom_of_movement:
    {
        const auto kind = spell.rider == detail::Rider::death_ward
                              ? detail::EffectKind::death_ward
                              : detail::EffectKind::free_moving;
        std::erase_if(target.effects.active,
                      [&](const auto &e)
                      {
                          return e.kind == kind;
                      });
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name, kind, 0);
        log(target.source.name + " gains " + std::string(spell.label) + ".",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", std::string(spell.label), true}}});
        return;
    }
    case detail::Rider::aura_of_life:
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::aura_of_life, 0);
        log(a.source.name + " radiates an Aura of Life.",
            {"{name} radiates an Aura of Life.", {{"name", a.source.name}}});
        return;
    case detail::Rider::beacon_of_hope:
        // The game takes every ally within 30 feet, the caster included.
        for (auto &ally : actors_)
        {
            if (ally.source.side == a.source.side && !ally.life.dead &&
                distance(a, ally) <= spell.range && detail::can_apply(ally.effects))
            {
                detail::apply_spell_benefit(ally.effects, effect_source(a), a.source.name,
                                            detail::EffectKind::hopeful, 0);
                log(ally.source.name + " gains Beacon of Hope.",
                    {"{name} gains {spell}.",
                     {{"name", ally.source.name}, {"spell", "Beacon of Hope", true}}});
            }
        }
        return;
    case detail::Rider::aid:
    {
        // Aid does not stack with itself; a creature already aided keeps its own.
        if (detail::has_effect(target.effects, detail::EffectKind::aid) || target.life.dead ||
            !detail::can_apply(target.effects))
        {
            return;
        }
        const int raise = 5 + 5 * static_cast<int>(upcast_steps(verb, spell));
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::aid, raise);
        log(target.source.name + " gains Aid.",
            {"{name} gains {spell}.", {{"name", target.source.name}, {"spell", "Aid", true}}});
        heal(target, raise);
        return;
    }
    case detail::Rider::lesser_restoration:
    {
        const auto condition = *detail::restoration_condition(verb);
        // Contagion's Poisoned ends only on a successful Constitution save.
        const auto *contagion = [&]() -> const detail::Effect *
        {
            for (const auto &e : target.effects.active)
            {
                if (e.kind == detail::EffectKind::contagion)
                {
                    return &e;
                }
            }
            return nullptr;
        }();
        const bool holds = contagion && condition == "poisoned" &&
                           !saving_throw_succeeds(target, detail::Ability::constitution,
                                                  spell_dc(a, "contagion"), def(target).dwarf);
        std::erase_if(target.effects.active,
                      [&](const auto &e)
                      {
                          return gives_condition(e.kind, condition) &&
                                 !(holds && e.kind == detail::EffectKind::contagion);
                      });
        const std::string message = condition == "blinded"     ? "{name} is no longer Blinded."
                                    : condition == "paralyzed" ? "{name} is no longer Paralyzed."
                                                               : "{name} is no longer Poisoned.";
        log(target.source.name + " is no longer " + condition_name(condition) + ".",
            {message, {{"name", target.source.name}}});
        return;
    }
    case detail::Rider::chill_touch:
    {
        const unsigned slot = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
        detail::apply_chill_touch(target.effects, effect_source(a), a.source.name,
                                  detail::round_ms + slot);
        log(target.source.name + " cannot regain HP until the end of the caster's next turn.",
            {"{name} cannot regain HP until the end of the caster's next turn.",
             {{"name", target.source.name}}});
        return;
    }
    case detail::Rider::shocking_grasp:
        detail::apply_shocking_grasp(target.effects, effect_source(a), a.source.name,
                                     next_turn_ms(target));
        log(target.source.name + " cannot make Opportunity Attacks until its next turn.",
            {"{name} cannot make Opportunity Attacks until its next turn.",
             {{"name", target.source.name}}});
        return;
    case detail::Rider::ray_of_frost:
        detail::apply_ray_of_frost(target.effects, effect_source(a), a.source.name,
                                   next_turn_ms(a));
        log(target.source.name + " is slowed by Ray of Frost.",
            {"{name} is slowed by Ray of Frost.", {{"name", target.source.name}}});
        return;
    case detail::Rider::shield_of_faith:
    case detail::Rider::heroism:
    case detail::Rider::divine_favor:
    case detail::Rider::bless:
    case detail::Rider::protection_from_evil_and_good:
    case detail::Rider::hunters_mark:
    case detail::Rider::longstrider:
    {
        // Heroism's Temporary HP equal the caster's spellcasting modifier.
        const int value =
            spell.rider == detail::Rider::heroism ? std::max(0, casting_modifier(def(a))) : 0;
        detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                    rider_effect(spell.rider), value);
        log(target.source.name + " gains " + std::string(spell.label) + ".",
            {"{name} gains {spell}.",
             {{"name", target.source.name}, {"spell", std::string(spell.label), true}}});
        if (spell.rider == detail::Rider::hunters_mark)
        {
            reveal_lore(a, target);
        }
        return;
    }
    case detail::Rider::command:
    {
        const auto option = detail::command_option(verb).value();
        const auto index = index_of(target.source.id);
        const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
        detail::apply_command(target.effects, effect_source(a), a.source.name, option,
                              next_turn_ms(target) + slot);
        log(target.source.name + " must obey " + a.source.name +
                "'s Command: " + command_label(option) + ".",
            {"{name} must obey {caster}'s Command: {option}.",
             {{"name", target.source.name},
              {"caster", a.source.name},
              {"option", command_label(option), true}}});
        return;
    }
    case detail::Rider::blindness:
        detail::apply_blindness(
            target.effects, effect_source(a), a.source.name,
            detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(target.source.id)});
        log(target.source.name + " is Blinded.",
            {"{name} is Blinded.", {{"name", target.source.name}}});
        return;
    }
}

void Session::resolve_spell(const detail::SpellDef &spell, std::string_view verb, Actor &a,
                            EntityId target_id)
{
    end_sanctuary(a);
    const bool was_invisible = invisible(a);
    const auto id = a.source.id;
    const auto damaged_before = damage_taken_.size();
    cast_resolved_spell(spell, verb, a, std::span<const EntityId>(&target_id, 1));
    ready_vitality(actor(id), spell, damaged_before);
    if (was_invisible)
    {
        end_invisibility(id);
    }
    end_hiding(id);
}

void Session::strike_with_spell(const detail::SpellDef &spell, std::string_view verb, Actor &a,
                                Actor &target, const Dice &rolled, unsigned steps, int dc)
{
    const auto &d = def(a);
    // Spiritual Weapon's force appears beside the target and lasts with the
    // caster's Concentration, whether or not this first attack hits.
    if (spell.rider == detail::Rider::spiritual_weapon)
    {
        begin_concentration(a, spell);
        Zone force{a.source.id, ZoneKind::spiritual_weapon, {spectral_cell(target, a.source.cell)}};
        force.slot = slot_level_of(verb, spell);
        zones_.push_back(std::move(force));
    }
    // Arcane Sword's sword and Conjure Fey's spirit appear beside the target
    // the same way; they wait for the caster's later turns to strike again.
    if (spell.rider == detail::Rider::arcane_sword || spell.rider == detail::Rider::conjure_fey)
    {
        begin_concentration(a, spell);
        Zone spirit{a.source.id,
                    spell.rider == detail::Rider::arcane_sword ? ZoneKind::sword : ZoneKind::fey,
                    {spectral_cell(target, a.source.cell)}};
        spirit.dealt = 1;
        spirit.slot = spell.rider == detail::Rider::conjure_fey ? slot_level_of(verb, spell) : 0;
        zones_.push_back(std::move(spirit));
    }
    const auto type = cast_damage_type(
        detail::chromatic_type(verb).value_or(detail::burst_type(verb).value_or(spell.damage)));
    // Sorcerous Burst: each 8 adds a d8, at most the spellcasting modifier.
    const int bursts = spell.id == "sorcerous_burst" ? std::max(1, casting_modifier(d)) : 0;
    const bool struck =
        attack(a, target, spell.melee ? AttackRange::melee : AttackRange::ranged,
               SpellAttack{.dice = rolled, .type = type, .bursts = bursts, .spell = spell.id});
    // Repelling Blast pushes a Large or smaller creature 10 feet away.
    if (struck && d.repelling.contains(spell.id) && !target.life.dead && def(target).size <= 3)
    {
        push_away(a, target, 2);
    }
    if (struck)
    {
        apply_rider(spell, verb, a, target, dc);
    }
    else if (spell.rider == detail::Rider::acid_arrow && target.life.hp > 0)
    {
        // A miss splashes half the initial damage, without the later burn.
        const int amount = resolved_damage(target, spell.damage, dice(rolled) / 2);
        log(target.source.name + " takes " + std::to_string(amount) +
                " Acid damage from the splash.",
            {"{name} takes {damage} Acid damage from the splash.",
             {{"name", target.source.name}, {"damage", std::to_string(amount)}}});
        damage(target, amount, false);
    }
    else if (d.evoker && !spell.level && target.life.hp > 0)
    {
        potent_cantrip(target, spell, rolled);
    }
    // Ice Knife's shard explodes, hit or miss.
    if (spell.rider == detail::Rider::ice_knife)
    {
        ice_burst(a, target, steps);
    }
}

void Session::missile_hits(Actor &a, Actor &target, const detail::SpellDef &spell,
                           const Dice &rolled, unsigned darts, bool split)
{
    if (sanctuary_stops(a, target))
    {
        return;
    }
    const auto name = std::string(spell.label);
    if (spell.id == "magic_missile")
    {
        ask_reaction(target, Asked::missile);
    }
    if (spell.id == "magic_missile" &&
        detail::has_effect(target.effects, detail::EffectKind::shield))
    {
        log(target.source.name + "'s Shield blocks Magic Missile.",
            {"{name}'s Shield blocks Magic Missile.", {{"name", target.source.name}}});
        return;
    }
    int total = 0;
    // Instances resolve separately so resistance applies per instance.
    for (unsigned n = 0; n < darts; ++n)
    {
        total += resolved_damage(target, spell.damage,
                                 overchanneling_ && a.source.id == overchannel_caster_
                                     ? rolled.count * rolled.sides + rolled.bonus
                                     : dice(rolled));
    }
    // The plain English keeps its original lower-case damage word so stored
    // combat logs stay byte identical. The template uses the translatable
    // capitalised name, matching the convention in resolved_damage().
    auto lowered = std::string(detail::damage_name(spell.damage));
    for (auto &c : lowered)
    {
        if (c >= 'A' && c <= 'Z')
        {
            c += 32;
        }
    }
    if (split)
    {
        log(a.source.name + " casts " + name + " at " + target.source.name + " for " +
                std::to_string(total) + " " + lowered + " damage.",
            {"{name} casts {spell} at {target} for {damage} {type} damage.",
             {{"name", a.source.name},
              {"spell", name, true},
              {"target", target.source.name},
              {"damage", std::to_string(total)},
              {"type", std::string(detail::damage_name(spell.damage)), true}}});
    }
    else
    {
        log(a.source.name + " casts " + name + " for " + std::to_string(total) + " " + lowered +
                " damage.",
            {"{name} casts {spell} for {damage} {type} damage.",
             {{"name", a.source.name},
              {"spell", name, true},
              {"damage", std::to_string(total)},
              {"type", std::string(detail::damage_name(spell.damage)), true}}});
    }
    damage(target, total);
}

void Session::cast_resolved_spell(const detail::SpellDef &spell, std::string_view verb, Actor &a,
                                  std::span<const EntityId> targets)
{
    const auto target_id = targets.front();
    const auto &d = def(a);
    const unsigned steps = upcast_steps(verb, spell);
    // A potion, missile or dust is not a spell: nothing counters it (MAGIC-3).
    const bool from_item = detail::item_verb(verb);
    if (from_item)
    {
        log_item_use(a, spell);
    }
    const bool from_scroll = detail::scroll_verb(verb);
    if (from_scroll)
    {
        log_scroll_use(a, spell);
        if (scroll_check_fails(a, spell, verb))
        {
            pay_for_spell(a, verb, spell);
            return;
        }
    }
    // Counterspell: a stopped spell keeps its slot.
    if (!item_effect(verb) && (countered(a, spell) || slowed_spell_fails(a, verb, spell)))
    {
        return;
    }
    // A wand's or a scroll's spell has the item's save DC (MAGIC-6).
    item_caster_ = from_item || from_scroll ? a.source.id : 0;
    item_dc_ = from_scroll ? scroll_dc(slot_level_of(verb, spell)) : 15;
    const detail::ResetAtExit<EntityId> item_caster_reset{item_caster_, 0};
    const detail::SetWhileAlive against_magic{against_magic_};
    // A spell without damage dice keeps the default damage type, Fire.
    against_fire_ = spell.dice.count && spell.damage == detail::DamageType::fire;
    const detail::ResetAtExit<bool> against_fire_reset{against_fire_, false};
    take_metamagic(a, spell);
    take_overchannel(a, spell, verb);
    if (casting_with(Metamagic::heightened))
    {
        heightened_target_ = target_id;
    }
    mark_heightened(a, spell);
    if (spell.level)
    {
        pay_for_spell(a, verb, spell);
    }
    auto rolled = spell.dice;
    rolled.count += static_cast<int>(steps * spell.upcast.extra_dice);
    // A cantrip's dice grow with the caster's level; Eldritch Blast adds beams.
    if (!spell.level && spell.id != "eldritch_blast")
    {
        rolled.count *= cantrip_tier(d);
    }
    if (spell.add_casting_modifier)
    {
        rolled.bonus = casting_modifier(d, spell.id);
    }
    // Agonizing Blast adds the Charisma modifier to its cantrip's damage, and
    // Potent Spellcasting (Cleric 7) the Wisdom modifier to a Cleric cantrip's.
    if (d.agonizing.contains(spell.id))
    {
        rolled.bonus += casting_modifier(d);
    }
    if (d.potent_spellcasting && !spell.level && spell.dice.count &&
        detail::on_class_list(CharacterClass::cleric, spell.id))
    {
        rolled.bonus += casting_modifier(d);
    }
    // Potent Spellcasting (Druid 7, Elemental Fury) for a Druid cantrip.
    if (d.druid_potent && !spell.level && spell.dice.count &&
        detail::on_class_list(CharacterClass::druid, spell.id))
    {
        rolled.bonus += casting_modifier(d);
    }
    // Empowered Evocation (Wizard 10): the Intelligence modifier added to a
    // Wizard Evocation spell's damage roll. Magic Missile's darts are rolled
    // one by one, so they take none.
    if (d.empowered && spell.evocation && spell.dice.count &&
        spell.pattern != detail::SpellPattern::auto_damage &&
        spell.pattern != detail::SpellPattern::heal &&
        detail::on_class_list(CharacterClass::wizard, spell.id))
    {
        rolled.bonus += casting_modifier(d);
    }
    // Elemental Affinity (Sorcerer 6): the Charisma modifier added to the
    // damage roll of a spell of its type.
    if (d.elemental_affinity && spell.dice.count &&
        spell.pattern != detail::SpellPattern::auto_damage &&
        spell.pattern != detail::SpellPattern::heal &&
        cast_damage_type(detail::chromatic_type(verb).value_or(
            detail::burst_type(verb).value_or(spell.damage))) == *d.elemental_affinity)
    {
        rolled.bonus += casting_modifier(d);
    }
    const unsigned instances = spell.instances + steps * spell.upcast.extra_instances;
    const int dc = spell_dc(a, spell.id);
    const auto name = std::string(spell.label);
    switch (spell.pattern)
    {
    case detail::SpellPattern::smite:
        return; // Smites resolve through resolve_smite, after the caster's own hit.
    case detail::SpellPattern::camp:
    case detail::SpellPattern::exploration:
        return; // Never offered in combat.
    case detail::SpellPattern::weapon_strike:
        return; // Resolved by strike_true().
    case detail::SpellPattern::reaction:
        return; // Cast through the Shield prompt.
    case detail::SpellPattern::stabilize:
    {
        auto &target = actor(target_id);
        detail::stabilize(target.life, rng_);
        log(a.source.name + " casts " + name + ": " + target.source.name + " is Stable.",
            {"{name} casts {spell}: {target} is Stable.",
             {{"name", a.source.name}, {"spell", name, true}, {"target", target.source.name}}});
        return;
    }
    case detail::SpellPattern::buff:
        if (spell.concentration)
        {
            begin_concentration(a, spell);
        }
        apply_rider(spell, verb, a, actor(target_id), dc);
        return;
    case detail::SpellPattern::heal:
    {
        // Mass Healing Word heals each chosen creature. Heal restores 70, 10
        // more for each slot level above 6, and ends Blinded, Poisoned and
        // Befuddlement.
        if (spell.id == "heal")
        {
            rolled.bonus = 70 + 10 * static_cast<int>(steps);
        }
        for (const auto id : targets)
        {
            // Disciple of Life favors the Cleric's own spells, not potions.
            heal(actor(id), healing_roll(actor(id), rolled) +
                                (from_item ? 0 : disciple_of_life(a, slot_level_of(verb, spell))));
            if (spell.id == "heal")
            {
                std::erase_if(actor(id).effects.active,
                              [](const auto &e)
                              {
                                  return gives_condition(e.kind, "blinded") ||
                                         gives_condition(e.kind, "poisoned") ||
                                         e.kind == detail::EffectKind::befuddled;
                              });
            }
        }
        if (!from_item)
        {
            blessed_healer(a, spell, verb, targets);
        }
        return;
    }
    case detail::SpellPattern::spell_attack:
    {
        // Vampiric Touch: the first touch, then a Magic action each turn.
        if (spell.rider == detail::Rider::vampiric_touch)
        {
            const unsigned slot = slot_level_of(verb, spell);
            begin_concentration(a, spell);
            detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                        detail::EffectKind::vampiric, static_cast<int>(slot));
            vampiric_touch(a, actor(target_id), slot);
            return;
        }
        // Eldritch Blast: a beam for each cantrip step, each its own attack, in
        // the order aimed; a beam whose creature has fallen turns to the
        // nearest foe in range, as a ray does (SPLIT-1).
        const unsigned beams = spell.id == "eldritch_blast" ? cantrip_tier(d) : 1;
        for (unsigned beam = 0; beam < beams; ++beam)
        {
            auto aim = targets[std::min<std::size_t>(beam, targets.size() - 1)];
            if (beam && (actor(aim).life.dead || actor(aim).life.hp == 0))
            {
                const Actor *best = nullptr;
                for (const auto &other : actors_)
                {
                    if (!other.life.dead && other.life.hp > 0 &&
                        other.source.side != a.source.side && !absent(other) &&
                        distance(a, other) <= spell_range(a, spell) && line_of_sight(a, other) &&
                        (!best || distance(a, other) < distance(a, *best)))
                    {
                        best = &other;
                    }
                }
                if (!best)
                {
                    break;
                }
                aim = best->source.id;
                log("The beam turns to " + best->source.name + ".",
                    {"The beam turns to {target}.", {{"target", best->source.name}}});
            }
            strike_with_spell(spell, verb, a, actor(aim), rolled, steps, dc);
        }
        return;
    }
    case detail::SpellPattern::repeat_attack:
    {
        // One ray per aimed target, in the order aimed; a single target takes
        // them all. A ray whose target has died turns to the next aimed foe
        // still alive, else to the nearest foe it could have been aimed at,
        // standing ones first (SPLIT-1); with none left it is lost. A downed
        // party member stays a target: each hit is a failed death save.
        std::vector<EntityId> rays(targets.begin(), targets.end());
        if (rays.size() == 1)
        {
            rays.assign(instances, target_id);
        }
        const auto alive = [&](EntityId id)
        {
            return !actor(id).life.dead;
        };
        const auto nearest = [&]() -> EntityId
        {
            const Actor *best = nullptr;
            const auto rank = [&](const Actor &who)
            {
                return std::pair{who.life.hp <= 0, distance(a, who)};
            };
            for (const auto &other : actors_)
            {
                if (other.life.dead || other.source.side == a.source.side ||
                    distance(a, other) > spell.range ||
                    (spell.requires_sight ? !can_see(a, other) : !line_of_sight(a, other)))
                {
                    continue;
                }
                if (!best || rank(other) < rank(*best))
                {
                    best = &other;
                }
            }
            return best ? best->source.id : 0;
        };
        for (std::size_t n = 0; n < rays.size(); ++n)
        {
            auto ray = rays[n];
            if (!alive(ray))
            {
                const auto next = std::find_if(rays.begin() + static_cast<std::ptrdiff_t>(n) + 1,
                                               rays.end(), alive);
                ray = next != rays.end() ? *next : nearest();
                if (!ray)
                {
                    continue;
                }
                log("The ray turns to " + actor(ray).source.name + ".",
                    {"The ray turns to {target}.", {{"target", actor(ray).source.name}}});
            }
            attack(a, actor(ray), spell.melee ? AttackRange::melee : AttackRange::ranged,
                   SpellAttack{
                       .dice = rolled, .type = cast_damage_type(spell.damage), .spell = spell.id});
        }
        return;
    }
    case detail::SpellPattern::auto_damage:
    {
        // The darts strike together: each creature takes its darts as one hit.
        std::vector<std::pair<EntityId, unsigned>> struck;
        for (const auto id : targets)
        {
            const auto found = std::find_if(struck.begin(), struck.end(),
                                            [&](const auto &entry)
                                            {
                                                return entry.first == id;
                                            });
            if (found != struck.end())
            {
                ++found->second;
            }
            else
            {
                struck.push_back({id, 1});
            }
        }
        if (targets.size() == 1)
        {
            struck.front().second = instances;
        }
        for (const auto &[id, darts] : struck)
        {
            missile_hits(a, actor(id), spell, rolled, darts, struck.size() > 1);
        }
        return;
    }
    case detail::SpellPattern::save_damage:
    {
        // The spirits of Conjure Woodland Beings and Spirit Guardians strike
        // only later, around the caster.
        if (spell.rider == detail::Rider::conjure_woodland_beings ||
            spell.rider == detail::Rider::spirit_guardians)
        {
            begin_concentration(a, spell);
            apply_rider(spell, verb, a, a, dc);
            return;
        }
        // Chain Lightning: the first creature and those chosen within 30 feet
        // of it each save against one roll.
        if (spell.id == "chain_lightning")
        {
            log(a.source.name + " casts " + name + ".",
                {"{name} casts {spell}.", {{"name", a.source.name}, {"spell", name, true}}});
            const auto first = actor(targets.front()).source.id;
            const int rolled_damage = spell_dice(a, rolled);
            const auto type = std::string(detail::damage_name(spell.damage));
            for (const auto id : targets)
            {
                auto &struck = actor(id);
                if (struck.life.dead || absent(struck) || sealed_off(struck) ||
                    globe_wards(a, struck.source.cell, slot_level_of(verb, spell)) ||
                    (id != first && distance(actor(first), struck) > 30))
                {
                    continue;
                }
                const bool saved = saving_throw_succeeds(struck, spell.save, dc);
                const int amount = resolved_damage(
                    struck, spell.damage, evaded(struck, spell.save, true, saved, rolled_damage));
                log(struck.source.name + " takes " + std::to_string(amount) + " " + type +
                        " damage.",
                    {"{name} takes {damage} {type} damage.",
                     {{"name", struck.source.name},
                      {"damage", std::to_string(amount)},
                      {"type", type, true}}});
                damage(struck, amount);
            }
            return;
        }
        auto &target = actor(target_id);
        if (sanctuary_stops(a, target))
        {
            return;
        }
        if (spell.concentration)
        {
            begin_concentration(a, spell);
        }
        log(a.source.name + " casts " + name + " at " + target.source.name + ".",
            {"{name} casts {spell} at {target}.",
             {{"name", a.source.name}, {"spell", name, true}, {"target", target.source.name}}});
        // Blight: a Plant fails its save.
        const bool saved = !(spell.id == "blight" && def(target).creature_type == "plant") &&
                           saving_throw_succeeds(target, spell.save, dc);
        // Phantasmal Killer ends on a success, after its half damage.
        if (saved && spell.rider == detail::Rider::phantasmal_killer)
        {
            end_concentration(a);
        }
        // Potent Cantrip: a saved-against cantrip still deals half damage.
        const bool halves = spell.half_on_success || (d.evoker && !spell.level);
        if (saved && !halves)
        {
            return;
        }
        // Halving comes before resistance, which resolved_damage applies.
        const int rolled_damage = spell_dice(a, rolled);
        const auto damage_type = cast_damage_type(spell.damage);
        const auto type = std::string(detail::damage_name(damage_type));
        const int amount = resolved_damage(
            target, damage_type, evaded(target, spell.save, halves, saved, rolled_damage));
        log(target.source.name + " takes " + std::to_string(amount) + " " + type + " damage.",
            {"{name} takes {damage} {type} damage.",
             {{"name", target.source.name},
              {"damage", std::to_string(amount)},
              {"type", type, true}}});
        damage(target, amount);
        if (amount)
        {
            curse_strikes(a, target);
        }
        // Disintegrate: a creature it drops to 0 Hit Points turns to dust, which
        // only Resurrection revives (LEVELS-3).
        if (spell.rider == detail::Rider::disintegrate && amount > 0 && target.life.hp == 0)
        {
            target.life.dead = true;
            target.life.recovery = {};
            if (detail::can_apply(target.effects))
            {
                detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                            detail::EffectKind::dust, 0);
            }
            log(target.source.name + " turns to dust.",
                {"{name} turns to dust.", {{"name", target.source.name}}});
            fall(target);
            return;
        }
        // Harm: the Hit Point maximum drops by the damage, never below 1, unless
        // Aura of Life guards it.
        if (!saved && spell.rider == detail::Rider::harm && amount > 0 && !target.life.dead &&
            !in_aura_of_life(target) && detail::can_apply(target.effects))
        {
            const int lowered = std::min(amount, max_hp(target) - 1);
            if (lowered > 0)
            {
                detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                            detail::EffectKind::drained, lowered);
                target.life.hp = std::min(target.life.hp, max_hp(target));
                log(target.source.name + "'s Hit Point maximum drops by " +
                        std::to_string(lowered) + ".",
                    {"{name}'s Hit Point maximum drops by {amount}.",
                     {{"name", target.source.name}, {"amount", std::to_string(lowered)}}});
            }
            return;
        }
        // Finger of Death: a Humanoid it kills rises as the caster's Zombie at
        // the start of the caster's next turn.
        if (spell.rider == detail::Rider::finger_of_death && target.life.dead &&
            def(target).creature_type == "humanoid" && detail::can_apply(target.effects))
        {
            detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                        detail::EffectKind::rising, 0);
        }
        if (!saved && spell.rider != detail::Rider::none && !target.life.dead)
        {
            apply_rider(spell, verb, a, target, dc);
        }
        return;
    }
    case detail::SpellPattern::save_condition:
    {
        auto &target = actor(target_id);
        if (spell.concentration &&
            !(spell.rider == detail::Rider::bestow_curse && slot_level_of(verb, spell) >= 5))
        {
            begin_concentration(a, spell);
        }
        if (spell.rider == detail::Rider::heat_metal)
        {
            std::erase_if(target.effects.active,
                          [&](const auto &e)
                          {
                              return e.kind == detail::EffectKind::heated &&
                                     e.source_actor == a.source.id;
                          });
            if (detail::can_apply(target.effects))
            {
                detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                            detail::EffectKind::heated, 1);
                target.effects.active.back().slot = slot_level_of(verb, spell);
            }
            heat_metal(a, target, slot_level_of(verb, spell));
            return;
        }
        // Mind Blank: no charm or control takes hold of its creature.
        if (charms(spell) && detail::has_effect(target.effects, detail::EffectKind::mind_blank))
        {
            log(target.source.name + "'s Mind Blank shields its mind.",
                {"{name}'s Mind Blank shields its mind.", {{"name", target.source.name}}});
            return;
        }
        // A willing creature forgoes the save, as does Maze's and Power Word
        // Stun's; a Construct turns no flesh to stone.
        const bool construct = spell.rider == detail::Rider::flesh_to_stone &&
                               def(target).creature_type == "construct";
        // Countercharm may give a save against a charm or a fright a second try.
        const bool charm_or_fear =
            charms(spell) || detail::without_cast_suffix(verb) == "eyebite_panicked";
        // Contagion's and Eyebite's sickness Poison those who fail.
        against_poisoned_ = spell.rider == detail::Rider::contagion ||
                            detail::without_cast_suffix(verb) == "eyebite_sickened";
        const detail::ResetAtExit<bool> against_poisoned_reset{against_poisoned_, false};
        if (verb == "enlarge" || spell.rider == detail::Rider::maze ||
            spell.rider == detail::Rider::power_word_stun ||
            (!construct &&
             !(charm_or_fear
                   ? charm_or_fear_save(
                         target, spell.save, dc,
                         fought_advantage(spell) ||
                             (charms(spell) ? def(target).fey_ancestry : def(target).brave))
                   : saving_throw_succeeds(target, spell.save, dc, fought_advantage(spell)))))
        {
            apply_rider(spell, verb, a, target, dc);
        }
        else if (spell.rider == detail::Rider::eyebite && detail::can_apply(target.effects))
        {
            // It cannot be targeted again by this casting.
            detail::apply_spell_benefit(target.effects, effect_source(a), a.source.name,
                                        detail::EffectKind::eye_resisted, 0);
            if (!detail::has_effect(a.effects, detail::EffectKind::eyebiting))
            {
                detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                            detail::EffectKind::eyebiting, 0);
            }
        }
        else if (spell.rider == detail::Rider::flesh_to_stone && detail::can_apply(target.effects))
        {
            detail::apply_poisoned(target.effects, effect_source(a), a.source.name, next_turn_ms(a),
                                   detail::EffectKind::rooted);
            log(target.source.name + " stiffens: Speed 0 until " + a.source.name + "'s next turn.",
                {"{name} stiffens: Speed 0 until {caster}'s next turn.",
                 {{"name", target.source.name}, {"caster", a.source.name}}});
        }
        else if (spell.rider == detail::Rider::irresistible_dance &&
                 detail::can_apply(target.effects))
        {
            const auto index = index_of(target.source.id);
            const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
            detail::apply_poisoned(target.effects, effect_source(a), a.source.name,
                                   next_turn_ms(target) + slot, detail::EffectKind::jigging);
            log(target.source.name + " dances in place until the end of its next turn.",
                {"{name} dances in place until the end of its next turn.",
                 {{"name", target.source.name}}});
        }
        else if (spell.rider == detail::Rider::ray_of_enfeeblement)
        {
            // A success still leaves Disadvantage on its next attack roll until
            // the start of the caster's next turn, as Sap does.
            detail::apply_attack_mastery(target.effects, detail::EffectKind::sap, effect_source(a),
                                         a.source.name, next_turn_ms(a));
            log(target.source.name + " has Disadvantage on its next attack roll.",
                {"{name} has Disadvantage on its next attack roll.",
                 {{"name", target.source.name}}});
        }
        return;
    }
    }
}

void Session::offer_spells(std::vector<Command> &commands, const Actor &a, const Actor &other,
                           int feet, detail::SpellTarget scope, bool bonus_pass) const
{
    // The Bonus Action pass runs before the loop that skips corpses, so filter
    // them here too; the Action pass has already done it and is unaffected.
    // A raging creature cannot cast spells, nor anyone in an Antimagic Field,
    // where no magic item works either. Befuddlement stops the creature's
    // spells.
    if (other.life.dead || sealed_off(a))
    {
        return;
    }
    const auto first = commands.size();
    const bool spells_blocked =
        rage_of(a) || detail::has_effect(a.effects, detail::EffectKind::befuddled);
    const auto &d = def(a);
    for (const auto &spell : detail::spell_table())
    {
        // Smites follow the caster's own melee hit; the smite window offers them.
        if (spell.pattern == detail::SpellPattern::smite ||
            spell.pattern == detail::SpellPattern::camp ||
            spell.pattern == detail::SpellPattern::exploration ||
            spell.pattern == detail::SpellPattern::weapon_strike ||
            spell.pattern == detail::SpellPattern::reaction || spell.target != scope)
        {
            continue;
        }
        // Divine Intervention (Cleric 10): any Cleric spell of level 5 or lower,
        // prepared or not, cast as a Magic action without a slot.
        const bool intervene = !spells_blocked && !bonus_pass && d.divine_intervention &&
                               a.arcane[CharacterClass::cleric] > 0 && spell.level &&
                               spell.level <= 5 &&
                               detail::on_class_list(CharacterClass::cleric, spell.id);
        // Mystic Arcanum (Warlock 11, 13, 15): its spell once per Long Rest,
        // as a Magic action without a slot.
        const bool arcanum = !spells_blocked && !bonus_pass && d.arcanum.contains(spell.id) &&
                             !arcanum_spent(a, spell.level);
        const bool known = !spells_blocked && detail::knows_spell(d.spells, spell.id);
        // A carried item's spell or effect (MAGIC-3): a potion with the Bonus
        // Action; a wand, missile or dust with the Action. A raging creature can
        // still drink or throw, but casts nothing from a wand.
        // Fast Hands takes the Magic or Utilize action to use one as a Bonus
        // Action, too.
        const auto *item = usable_item(a, spell);
        const bool drink = item && item->use == detail::ItemUse::drink;
        const bool item_ready = item && (drink ? bonus_pass : !bonus_pass || d.fast_hands) &&
                                !(spells_blocked && item->use == detail::ItemUse::wand);
        // A spell on a carried scroll (MAGIC-6), read with its own casting time.
        const auto scroll = spells_blocked ? std::nullopt : readable_scroll(a, spell);
        const bool scroll_ready = scroll && spell.bonus_action == bonus_pass;
        // A monster's innate spell, with the pass's own action.
        const auto innate = spells_blocked ? std::nullopt : innate_pool(a, spell, bonus_pass);
        if (!known && !intervene && !arcanum && !item_ready && !scroll_ready && !innate)
        {
            continue;
        }
        // Quickened Spell offers an Action spell in the Bonus Action pass.
        const bool as_bonus = spell.bonus_action || readies(a, Metamagic::quickened, spell);
        const bool usual = known && as_bonus == bonus_pass;
        if (!usual && !intervene && !arcanum && !item_ready && !scroll_ready && !innate)
        {
            continue;
        }
        // Natural Recovery (Land 6): a Circle spell once per Long Rest, no slot.
        const bool natural = usual && d.circle.contains(spell.id) && d.natural_recovery &&
                             !detail::has_effect(a.effects, detail::EffectKind::natural_cast);
        switch (spell.target)
        {
        case detail::SpellTarget::enemy:
            // Any creature but the caster, a downed one included (CLOSE-1).
            if (other.source.id == a.source.id)
            {
                continue;
            }
            break;
        case detail::SpellTarget::wounded_ally:
            // Healing takes any wounded creature (CLOSE-1).
            if (other.life.hp >= max_hp(other))
            {
                continue;
            }
            break;
        case detail::SpellTarget::any_creature:
            break;
        case detail::SpellTarget::ally:
            if (other.source.side != a.source.side)
            {
                continue;
            }
            break;
        case detail::SpellTarget::self:
            if (other.source.id != a.source.id)
            {
                continue;
            }
            break;
        case detail::SpellTarget::area:
            break; // Aimed at a point after it is chosen.
        case detail::SpellTarget::dying_ally:
            if (other.life.hp != 0 || other.life.stable)
            {
                continue;
            }
            break;
        case detail::SpellTarget::corpse:
            continue; // Offered with the corpses it can raise (Animate Dead).
        case detail::SpellTarget::dead_ally:
            continue; // Offered on its own to a just-dead ally (Revivify).
        }
        if (feet > spell_range(a, spell))
        {
            continue;
        }
        // No spell reaches into an Antimagic Field or through a Wall of Force.
        if (spell.target != detail::SpellTarget::area &&
            (sealed_off(other) || !line_of_effect(a, other)))
        {
            continue;
        }
        if (spell.requires_sight && !can_see(a, other))
        {
            continue;
        }
        if (spell.requires_effect_capacity && !detail::can_apply(other.effects))
        {
            continue;
        }
        if (spell.rider == detail::Rider::lesser_restoration && !restorable(other))
        {
            continue;
        }
        if (spell.rider == detail::Rider::remove_curse && !cursed(other))
        {
            continue;
        }
        if (spell.rider == detail::Rider::heat_metal && !wears_metal(other))
        {
            continue;
        }
        if (spell.rider == detail::Rider::snake_charm && !snakes_to_charm(a))
        {
            continue;
        }
        if (spell.rider == detail::Rider::missile_ward &&
            detail::has_effect(other.effects, detail::EffectKind::missile_ward))
        {
            continue;
        }
        if (spell.humanoid_only && def(other).creature_type != "humanoid")
        {
            continue;
        }
        if (spell.not_self && other.source.id == a.source.id)
        {
            continue;
        }
        // Mage Armor needs a creature wearing no armor, and lasts only once.
        if (spell.rider == detail::Rider::mage_armor &&
            (!def(other).mage_armor_ac ||
             detail::has_effect(other.effects, detail::EffectKind::mage_armor)))
        {
            continue;
        }
        if (spell.rider == detail::Rider::expeditious_retreat &&
            detail::has_effect(other.effects, detail::EffectKind::expeditious_retreat))
        {
            continue;
        }
        if ((spell.rider == detail::Rider::blur &&
             detail::has_effect(other.effects, detail::EffectKind::blur)) ||
            (spell.rider == detail::Rider::giant_strength &&
             detail::has_effect(other.effects, detail::EffectKind::giant_strength)) ||
            (spell.rider == detail::Rider::haste &&
             detail::has_effect(other.effects, detail::EffectKind::hasted)) ||
            (spell.rider == detail::Rider::invisibility &&
             detail::has_effect(other.effects, detail::EffectKind::invisible)) ||
            (spell.rider == detail::Rider::see_invisibility &&
             detail::has_effect(other.effects, detail::EffectKind::see_invisibility)) ||
            (spell.rider == detail::Rider::mirror_image &&
             detail::has_effect(other.effects, detail::EffectKind::mirror_image)) ||
            (spell.rider == detail::Rider::magic_weapon &&
             detail::has_effect(other.effects, detail::EffectKind::magic_weapon)) ||
            (spell.rider == detail::Rider::barkskin &&
             detail::has_effect(other.effects, detail::EffectKind::barkskin)) ||
            (spell.rider == detail::Rider::stoneskin &&
             detail::has_effect(other.effects, detail::EffectKind::stoneskin)) ||
            (spell.rider == detail::Rider::dominate_beast && def(other).creature_type != "beast") ||
            // Resilient Sphere holds a Large creature or smaller;
            // Telekinesis moves a Huge one or smaller.
            (spell.rider == detail::Rider::resilient_sphere && def(other).size > 3) ||
            (spell.rider == detail::Rider::telekinesis &&
             (def(other).size > 4 || other.source.id == a.source.id)) ||
            (spell.rider == detail::Rider::greater_invisibility &&
             detail::has_effect(other.effects, detail::EffectKind::greater_invisible)) ||
            (spell.rider == detail::Rider::shillelagh && !d.shillelagh_weapon))
        {
            continue;
        }
        const bool innate_ready = innate && (bonus_pass ? a.bonus : a.actions.available(true));
        const bool casting_ready = ((usual || intervene || arcanum) &&
                                    (usual ? (as_bonus ? a.bonus : a.actions.available(true))
                                           : a.actions.available(true))) ||
                                   innate_ready;
        // Dust is thrown with the Utilize action; a wand or missile takes the Magic
        // action.
        const bool item_offered =
            item_ready &&
            (bonus_pass ? a.bonus : a.actions.available(item->use != detail::ItemUse::dust));
        const bool scroll_offered =
            scroll_ready && (bonus_pass ? a.bonus : a.actions.available(true));
        if (!casting_ready && !item_offered && !scroll_offered)
        {
            continue;
        }
        const bool aimed = spell.target == detail::SpellTarget::area;
        const auto offer = [&](std::string verb, std::string label)
        {
            // Globe of Invulnerability: a spell too weak for it is not cast at
            // a creature inside.
            if (!aimed && globe_wards(a, other.source.cell, slot_level_of(verb, spell)))
            {
                return;
            }
            commands.push_back({revision_, a.source.id, aimed ? 0 : other.source.id,
                                std::move(verb), std::move(label), Cell{}, 0, aimed});
            commands.back().spell = spell_facts(a, spell);
        };
        if (item_offered)
        {
            const bool fast = bonus_pass && !drink;
            offer(std::string(spell.id) +
                      std::string(fast ? detail::item_bonus_suffix : detail::item_suffix),
                  std::string(item->label) + (fast ? " (Fast Hands)" : ""));
        }
        if (scroll_offered)
        {
            offer(std::string(spell.id) + std::string(detail::scroll_suffix) +
                      (scroll->level > spell.level ? "_" + std::to_string(scroll->level) : ""),
                  std::string(spell.label) + " (scroll)");
        }
        if (!casting_ready)
        {
            continue;
        }
        // A leveled spell from each slot level cast_levels() offers, one slot
        // a turn, or with Magic Initiate's free cast, which spends no slot.
        const auto offer_leveled = [&](const std::string &verb, const std::string &label)
        {
            if (usual && !a.spent_slot)
            {
                for (const unsigned level : cast_levels(a, spell))
                {
                    auto [cast_verb, cast_label] = cast_at(spell, level, verb, label);
                    offer(std::move(cast_verb), std::move(cast_label));
                }
            }
            if (usual && (a.initiate_casts & initiate_bit(d, spell.id)))
            {
                offer(verb + std::string(detail::initiate_suffix), label + " (Magic Initiate)");
            }
            if (intervene)
            {
                offer(verb + std::string(detail::intervention_suffix),
                      label + " (Divine Intervention)");
            }
            if (arcanum)
            {
                offer(verb + std::string(detail::arcanum_suffix), label + " (Mystic Arcanum)");
            }
            if (natural)
            {
                offer(verb + std::string(detail::natural_suffix), label + " (Natural Recovery)");
            }
            if (innate_ready)
            {
                offer(verb + std::string(bonus_pass ? detail::innate_bonus_suffix
                                                    : detail::innate_suffix),
                      label + " (" + d.innate[*innate].label + ")");
            }
        };
        if (spell.id == "chromatic_orb")
        {
            for (const auto type : detail::chromatic_types)
            {
                offer_leveled("chromatic_orb_" + std::string(type),
                              "Chromatic Orb: " +
                                  std::string(detail::damage_name(detail::damage_type(type))));
            }
            continue;
        }
        if (spell.rider == detail::Rider::guidance)
        {
            offer("guidance_athletics", "Guidance: Athletics");
            offer("guidance_medicine", "Guidance: Medicine");
            continue;
        }
        if (spell.rider == detail::Rider::resistance)
        {
            for (const auto type : detail::resistance_types)
            {
                offer("resistance_" + std::string(type),
                      "Resistance: " + std::string(detail::damage_name(detail::damage_type(type))));
            }
            continue;
        }
        if (spell.id == "sorcerous_burst")
        {
            for (const auto type : detail::burst_types)
            {
                offer("sorcerous_burst_" + std::string(type),
                      "Sorcerous Burst: " +
                          std::string(detail::damage_name(detail::damage_type(type))));
            }
            continue;
        }
        if (!spell.level)
        {
            offer(std::string(spell.id), std::string(spell.label));
            continue;
        }
        // Magic Circle: one entry per creature type, from its lowest slot only:
        // a fight never lasts long enough for a higher slot's extra hours.
        if (spell.rider == detail::Rider::magic_circle)
        {
            const auto levels = cast_levels(a, spell);
            if (a.spent_slot || levels.empty() || other.source.id != a.source.id)
            {
                continue;
            }
            for (std::size_t type = 0; type < detail::ward_types.size(); ++type)
            {
                auto [verb, label] = cast_at(
                    spell, levels.front(), "magic_circle_" + std::string(detail::ward_types[type]),
                    "Magic Circle: " + ward_label(type));
                offer(std::move(verb), std::move(label));
            }
            continue;
        }
        if (spell.rider == detail::Rider::contagion)
        {
            static constexpr std::array<std::pair<std::string_view, std::string_view>, 6> abilities{
                {{"strength", "Strength"},
                 {"dexterity", "Dexterity"},
                 {"constitution", "Constitution"},
                 {"intelligence", "Intelligence"},
                 {"wisdom", "Wisdom"},
                 {"charisma", "Charisma"}}};
            for (const auto &[id, name] : abilities)
            {
                offer_leveled("contagion_" + std::string(id), "Contagion: " + std::string(name));
            }
            continue;
        }
        if (spell.rider == detail::Rider::greater_restoration)
        {
            for (std::size_t n = 0; n < detail::greater_harms.size(); ++n)
            {
                if (has_greater_harm(other, n))
                {
                    offer_leveled("greater_restoration_" + std::string(detail::greater_harms[n]),
                                  std::string("Greater Restoration: ") + (n == 4   ? "Befuddlement"
                                                                          : n == 3 ? "Petrified"
                                                                          : n == 2
                                                                              ? "Hit Point maximum"
                                                                          : n ? "curse"
                                                                              : "Charmed"));
                }
            }
            continue;
        }
        if (spell.rider == detail::Rider::bestow_curse)
        {
            for (std::size_t n = 0; n < detail::curses.size(); ++n)
            {
                offer_leveled("bestow_curse_" + std::string(detail::curses[n]),
                              "Bestow Curse: " + curse_label(n));
            }
            continue;
        }
        if (spell.rider == detail::Rider::protection_from_energy)
        {
            if (!detail::has_effect(other.effects, detail::EffectKind::energy_ward))
            {
                for (const auto type : detail::energy_types)
                {
                    offer_leveled("protection_from_energy_" + std::string(type),
                                  "Protection from Energy: " +
                                      std::string(detail::damage_name(detail::damage_type(type))));
                }
            }
            continue;
        }
        // A wall: one entry for each direction it may run, and a ring when
        // it may stand as one (LEVELS-3).
        if (spell.wall)
        {
            for (std::size_t n = 0; n < detail::wall_directions.size(); ++n)
            {
                if (detail::wall_directions[n] == "ring" && !spell.ring)
                {
                    continue;
                }
                const auto verb =
                    std::string(spell.id) + "_" + std::string(detail::wall_directions[n]);
                // Wall of Fire: each direction twice, once for each burning side.
                if (spell.rider == detail::Rider::wall_of_fire)
                {
                    for (const std::size_t side : {2 * n, 2 * n + 1})
                    {
                        offer_leveled(verb + "_" + std::string(detail::wall_sides[side]),
                                      std::string(spell.label) + " (" + wall_direction_label(n) +
                                          ", " + std::string(detail::wall_sides[side]) +
                                          " side burning)");
                    }
                }
                else
                {
                    offer_leveled(verb,
                                  std::string(spell.label) + " (" + wall_direction_label(n) + ")");
                }
            }
            continue;
        }
        // Animal Shapes: one Beast form for every creature chosen.
        if (spell.rider == detail::Rider::animal_shapes)
        {
            for (const auto &form : detail::beast_forms)
            {
                offer_leveled("animal_shapes_" + std::string(form.key),
                              "Animal Shapes: " + std::string(form.label));
            }
            continue;
        }
        // Giant Insect: a centipede, a spider or a wasp.
        if (spell.rider == detail::Rider::giant_insect)
        {
            for (const auto form : {"centipede", "spider", "wasp"})
            {
                offer_leveled("giant_insect_" + std::string(form),
                              "Giant Insect: " + std::string(form));
            }
            continue;
        }
        // Forcecage: a cage of bars or a solid box.
        if (spell.rider == detail::Rider::forcecage)
        {
            offer_leveled("forcecage_cage", "Forcecage (cage)");
            offer_leveled("forcecage_box", "Forcecage (box)");
            continue;
        }
        // Conjure Minor Elementals: the extra damage's type, chosen at the
        // casting (LEVELS-4).
        if (spell.rider == detail::Rider::conjure_minor_elementals)
        {
            for (const auto type : {"acid", "cold", "fire", "lightning"})
            {
                offer_leveled("conjure_minor_elementals_" + std::string(type),
                              "Conjure Minor Elementals: " +
                                  std::string(detail::damage_name(detail::damage_type(type))));
            }
            continue;
        }
        // Eyebite: asleep, panicked or sickened, as chosen; not a creature that
        // resisted this casting.
        if (spell.rider == detail::Rider::eyebite)
        {
            if (detail::has_effect(other.effects, detail::EffectKind::eye_resisted))
            {
                continue;
            }
            for (const auto effect : {"asleep", "panicked", "sickened"})
            {
                offer_leveled("eyebite_" + std::string(effect), "Eyebite: " + std::string(effect));
            }
            continue;
        }
        // Conjure Elemental: the spirit's element; Summon Dragon: the spirit's
        // breath and shared Resistance.
        if (spell.rider == detail::Rider::conjure_elemental)
        {
            for (const auto element : {"air", "earth", "fire", "water"})
            {
                offer_leveled("conjure_elemental_" + std::string(element),
                              "Conjure Elemental: " + std::string(element));
            }
            continue;
        }
        if (spell.rider == detail::Rider::summon_dragon)
        {
            for (const auto type : {"acid", "cold", "fire", "lightning", "poison"})
            {
                offer_leveled("summon_dragon_" + std::string(type),
                              "Summon Dragon: " +
                                  std::string(detail::damage_name(detail::damage_type(type))));
            }
            continue;
        }
        // Fire Shield: the warm shield or the chill one.
        if (spell.rider == detail::Rider::fire_shield)
        {
            offer_leveled("fire_shield_warm", "Fire Shield (warm)");
            offer_leveled("fire_shield_chill", "Fire Shield (chill)");
            continue;
        }
        // Polymorph: one of the game's Beast forms.
        if (spell.rider == detail::Rider::polymorph)
        {
            for (const auto &form : detail::beast_forms)
            {
                offer_leveled("polymorph_" + std::string(form.key),
                              "Polymorph: " + std::string(form.label));
            }
            continue;
        }
        // Dimension Door: alone, or with a willing creature beside the caster.
        if (spell.rider == detail::Rider::dimension_door)
        {
            offer_leveled("dimension_door", "Dimension Door");
            if (usual && !a.spent_slot)
            {
                for (const auto &ally : actors_)
                {
                    if (ally.source.side == a.source.side && ally.source.id != a.source.id &&
                        !ally.life.dead && !absent(ally) && distance(a, ally) <= 5)
                    {
                        for (const unsigned level : cast_levels(a, spell))
                        {
                            auto [cast_verb, cast_label] =
                                cast_at(spell, level, "dimension_door_with",
                                        "Dimension Door with " + ally.source.name);
                            commands.push_back({revision_, a.source.id, ally.source.id,
                                                std::move(cast_verb), std::move(cast_label), Cell{},
                                                0, true});
                        }
                    }
                }
            }
            continue;
        }
        if (spell.rider == detail::Rider::dragons_breath)
        {
            if (!detail::has_effect(other.effects, detail::EffectKind::dragons_breath))
            {
                for (const auto type : detail::dragon_types)
                {
                    offer_leveled("dragons_breath_" + std::string(type),
                                  "Dragon's Breath: " +
                                      std::string(detail::damage_name(detail::damage_type(type))));
                }
            }
            continue;
        }
        if (spell.rider == detail::Rider::enlarge_reduce)
        {
            if (other.source.side == a.source.side)
            {
                offer_leveled("enlarge", "Enlarge");
            }
            else
            {
                offer_leveled("reduce", "Reduce");
            }
            continue;
        }
        // The caster chooses which one condition to end.
        if (spell.rider == detail::Rider::lesser_restoration)
        {
            for (const auto condition : detail::restoration_conditions)
            {
                if (has_condition(other, condition))
                {
                    offer_leveled("lesser_restoration_" + std::string(condition),
                                  "Lesser Restoration: " + condition_name(condition));
                }
            }
            continue;
        }
        if (spell.rider == detail::Rider::command)
        {
            for (const auto option : detail::command_options)
            {
                const auto verb = "command_" + std::string(option);
                offer_leveled(verb,
                              "Command: " + command_label(detail::command_option(verb).value()));
            }
            continue;
        }
        offer_leveled(std::string(spell.id), std::string(spell.label));
    }
    const auto intent =
        scope == detail::SpellTarget::enemy ? Intent::hostile
        : scope == detail::SpellTarget::wounded_ally || scope == detail::SpellTarget::dying_ally
            ? Intent::friendly
            : Intent::neutral;
    for (auto i = first; i < commands.size(); ++i)
    {
        commands[i].intent = intent;
    }
}

// Whether `cell` lies in a 5-foot-wide Line from `from` to `to`, between them.
bool in_line(Cell from, Cell to, Cell cell)
{
    const double dx = to.x - from.x, dy = to.y - from.y;
    const double length = dx * dx + dy * dy;
    const double cx = cell.x - from.x, cy = cell.y - from.y;
    const double along = length ? (cx * dx + cy * dy) / length : 0;
    if (along <= 0 || along >= 1)
    {
        return false;
    }
    const double off_x = cx - along * dx, off_y = cy - along * dy;
    return off_x * off_x + off_y * off_y <= 0.25;
}

// SRD 5.2.1 Javelin of Lightning (MAGIC-3): the throw becomes a bolt in a Line
// to the target. Each other creature in it makes a DC 13 Dexterity save, 4d6
// Lightning damage on a failure and half on a success; then the javelin's
// attack deals 4d6 more Lightning damage on a hit. Once until a Long Rest.
void Session::lightning_javelin(Actor &a, Actor &target, unsigned token)
{
    constexpr int javelin_dc = 13;
    // Effects that last past the fight carry scope 1, as camp effects do.
    detail::apply_spell_benefit(a.effects, detail::EffectSource{.scope = 1, .actor = 1},
                                a.source.name, detail::EffectKind::javelin_spent, 0);
    log(a.source.name + "'s javelin becomes a bolt of lightning.",
        {"{name}'s javelin becomes a bolt of lightning.", {{"name", a.source.name}}});
    const int rolled = dice({4, 6, 0});
    for (auto &other : actors_)
    {
        const auto squares = squares_of(other);
        if (other.life.dead || absent(other) || other.source.id == a.source.id ||
            other.source.id == target.source.id ||
            std::none_of(squares.begin(), squares.end(),
                         [&](Cell square)
                         {
                             return in_line(a.source.cell, target.source.cell, square);
                         }))
        {
            continue;
        }
        const bool saved = saving_throw_succeeds(other, detail::Ability::dexterity, javelin_dc);
        const int amount =
            resolved_damage(other, detail::DamageType::lightning,
                            evaded(other, detail::Ability::dexterity, true, saved, rolled));
        log(other.source.name + " takes " + std::to_string(amount) + " Lightning damage.",
            {"{name} takes {damage} {type} damage.",
             {{"name", other.source.name},
              {"damage", std::to_string(amount)},
              {"type", "Lightning", true}}});
        damage(other, amount);
    }
    a.lightning_throw = true;
    throw_weapon(a, target, token);
    a.lightning_throw = false;
}

void Session::log_item_use(Actor &a, const detail::SpellDef &spell)
{
    if (const auto *item = usable_item(a, spell))
    {
        const auto label = std::string(item->label);
        log(a.source.name + " uses the " + label + ".",
            {"{name} uses the {item}.", {{"name", a.source.name}, {"item", label, true}}});
        // Use Magic Device (Thief 13): a 6 on 1d6 keeps a spent charge.
        if (def(a).use_magic_device && item->noun == "charges" && roll(6) == 6)
        {
            a.keeps_charge = true;
            log("Use Magic Device: the charge is not spent.",
                {"Use Magic Device: the charge is not spent.", {}});
        }
    }
}

void Session::log_scroll_use(const Actor &a, const detail::SpellDef &spell)
{
    log(a.source.name + " reads " + std::string(spell.label) + " from a scroll.",
        {"{name} reads {spell} from a scroll.",
         {{"name", a.source.name}, {"spell", std::string(spell.label), true}}});
}

bool Session::scroll_check_fails(const Actor &a, const detail::SpellDef &spell,
                                 std::string_view verb)
{
    const unsigned level = slot_level_of(verb, spell);
    if (!needs_arcana_check(def(a), level))
    {
        return false;
    }
    const int natural = detail::d20({false, check_disadvantage(a), false}, rng_),
              bonus = def(a).arcana, dc = 10 + static_cast<int>(level);
    log(a.source.name + "'s Arcana check for the scroll: d20 " + std::to_string(natural) + " + " +
            std::to_string(bonus) + " vs DC " + std::to_string(dc) + ".",
        {"{name}'s Arcana check for the scroll: d20 {roll} + {bonus} vs DC {dc}.",
         {{"name", a.source.name},
          {"roll", std::to_string(natural)},
          {"bonus", std::to_string(bonus)},
          {"dc", std::to_string(dc)}}});
    if (natural + bonus >= dc)
    {
        return false;
    }
    log("The " + std::string(spell.label) + " on the scroll is lost.",
        {"The {spell} on the scroll is lost.", {{"spell", std::string(spell.label), true}}});
    return true;
}

// As the original's Snake Charm: the caster's current Hit Points are a pool,
// and each snake or scorpion, in turn order, whose own still fit is
// Incapacitated for the fight, its Hit Points leaving the pool. No save.
void Session::charm_snakes(Actor &a)
{
    int pool = a.life.hp;
    for (auto &other : actors_)
    {
        if (&other == &a || other.life.dead || other.life.hp <= 0 || other.life.hp > pool ||
            !charmable_snake(other) || !detail::can_apply(other.effects) ||
            detail::has_effect(other.effects, detail::EffectKind::snake_charmed))
        {
            continue;
        }
        pool -= other.life.hp;
        detail::apply_spell_benefit(other.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::snake_charmed, 0);
        log(other.source.name + " is charmed by Snake Charm.",
            {"{name} is charmed by Snake Charm.", {{"name", other.source.name}}});
        end_incapacitated_effects(other);
    }
}

bool Session::snakes_to_charm(const Actor &a) const
{
    return std::any_of(actors_.begin(), actors_.end(),
                       [&](const auto &other)
                       {
                           return &other != &a && !other.life.dead && other.life.hp > 0 &&
                                  other.life.hp <= a.life.hp && charmable_snake(other) &&
                                  !detail::has_effect(other.effects,
                                                      detail::EffectKind::snake_charmed);
                       });
}

void Session::cast_area()
{
    const auto aimed = std::move(*area_);
    area_.reset();
    auto &a = actor(aimed.caster);
    if (aimed.verb == "lands_aid")
    {
        lands_aid(a, aimed.center);
        return;
    }
    // Compulsion: the direction from the Bard to the aimed square, to the
    // nearest of eight.
    if (aimed.verb == "compulsion_heading")
    {
        const int dx = aimed.center.x - a.source.cell.x, dy = aimed.center.y - a.source.cell.y;
        if (dx || dy)
        {
            a.bonus = false;
            const double eighths = std::atan2(dx, -dy) / (std::numbers::pi / 4);
            name_compulsion_heading(a, static_cast<std::size_t>((std::lround(eighths) + 8) % 8));
        }
        return;
    }
    if (aimed.verb == "natures_sanctuary" || aimed.verb == "move_sanctuary")
    {
        const auto &trees = *detail::find_spell("natures_sanctuary");
        auto cells = area_cells(trees, trees.id, origin_toward(a, aimed.center), aimed.center);
        if (aimed.verb == "move_sanctuary")
        {
            a.bonus = false;
            for (auto &zone : zones_)
            {
                if (zone.kind == ZoneKind::sanctuary && zone.caster == a.source.id)
                {
                    zone.cells = std::move(cells);
                }
            }
            log("The spectral trees move.", {"The spectral trees move.", {}});
            return;
        }
        a.nick_origin = 0;
        (void)a.actions.spend(true);
        --a.channel_divinity[CharacterClass::druid];
        std::erase_if(zones_,
                      [&](const auto &zone)
                      {
                          return zone.kind == ZoneKind::sanctuary && zone.caster == a.source.id;
                      });
        zones_.push_back({a.source.id, ZoneKind::sanctuary, std::move(cells), elapsed_ms_ + 60000});
        log(a.source.name + " raises Nature's Sanctuary.",
            {"{name} raises Nature's Sanctuary.", {{"name", a.source.name}}});
        return;
    }
    // Not a spell: the teleport leaves Sanctuary, Invisibility and hiding be.
    if (aimed.verb == "clouds_jaunt")
    {
        clouds_jaunt(a, aimed.center);
        return;
    }
    end_sanctuary(a);
    const bool was_invisible = invisible(a);
    const auto id = a.source.id;
    if (aimed.verb.starts_with("breath_weapon_"))
    {
        breath_weapon(a, aimed);
    }
    else if (aimed.verb == "petrifying_gaze")
    {
        petrifying_gaze(a, aimed);
    }
    else if (aimed.verb == def(a).monster_breath)
    {
        exhale_breath(a, aimed);
    }
    else
    {
        cast_aimed_area(a, aimed);
    }
    if (was_invisible)
    {
        end_invisibility(id);
    }
    end_hiding(id);
}

void Session::breath_weapon(Actor &dragonborn, const PendingArea &aimed)
{
    // The exhalation replaces one of the Attack action's attacks: a later one,
    // or the first, which leaves Extra Attack's others to make.
    if (!dragonborn.actions.attack_again())
    {
        dragonborn.nick_origin = 0;
        (void)dragonborn.actions.spend();
        dragonborn.actions.extra_attacks =
            def(dragonborn).thirsting_blade ? 0u
                                            : static_cast<unsigned>(def(dragonborn).extra_attacks);
    }
    dragonborn.actions.exhaled = true;
    spend_trait_use(dragonborn);
    log(dragonborn.source.name + " uses Breath Weapon.",
        {"{name} uses Breath Weapon.", {{"name", dragonborn.source.name}}});
    const auto &breath = *detail::find_spell(aimed.verb);
    damage_area(
        dragonborn, breath, aimed.verb,
        area_cells(breath, aimed.verb, origin_toward(dragonborn, aimed.center), aimed.center),
        area_dc(dragonborn, breath));
}

std::optional<int> Session::heroic_reroll(Actor &a)
{
    if (!conscious(a) || !std::erase_if(a.effects.active,
                                        [](const auto &e)
                                        {
                                            return e.kind == detail::EffectKind::heroic_inspiration;
                                        }))
    {
        return std::nullopt;
    }
    const int natural = roll(20);
    log(a.source.name +
            " uses Heroic Inspiration and rolls the d20 again: " + std::to_string(natural) + ".",
        {"{name} uses Heroic Inspiration and rolls the d20 again: {roll}.",
         {{"name", a.source.name}, {"roll", std::to_string(natural)}}});
    return natural;
}

void Session::spend_trait_use(Actor &a)
{
    for (auto &effect : a.effects.active)
    {
        if (effect.kind == detail::EffectKind::trait_uses)
        {
            ++effect.dc;
            return;
        }
    }
    detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                detail::EffectKind::trait_uses, 1);
}

void Session::cast_aimed_area(Actor &a, const PendingArea &aimed)
{
    const auto &spell = *detail::find_spell(aimed.verb);
    // The Draconic Spirit breathes besides its Rends, once a turn.
    if (spell.id == "spirit_breath")
    {
        const unsigned rest_of_turn = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
        detail::apply_poisoned(a.effects, effect_source(a), a.source.name, rest_of_turn,
                               detail::EffectKind::breathed);
        log(a.source.name + " breathes.", {"{name} breathes.", {{"name", a.source.name}}});
        damage_area(a, spell, aimed.verb,
                    area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center),
                    area_dc(a, spell));
        return;
    }
    // Quickened Spell casts an Action spell as a Bonus Action.
    const bool quickened = !spell.bonus_action && readies(a, Metamagic::quickened, spell);
    take_metamagic(a, spell);
    take_overchannel(a, spell, aimed.verb);
    if (spell.bonus_action || quickened || bonus_action_verb(aimed.verb))
    {
        a.bonus = false;
    }
    else
    {
        a.nick_origin = 0;
        (void)a.actions.spend(true);
    }
    // Counterspell: a stopped spell keeps its slot. Sunbeam's later Lines and
    // Dragon's Breath are not castings.
    const bool again = aimed.verb == "sunbeam_line" || aimed.verb == "telekinesis_again" ||
                       aimed.verb == "call_lightning_again";
    const bool from_item = detail::item_verb(aimed.verb);
    if (from_item)
    {
        log_item_use(a, spell);
    }
    const bool from_scroll = detail::scroll_verb(aimed.verb);
    if (from_scroll)
    {
        log_scroll_use(a, spell);
        if (scroll_check_fails(a, spell, aimed.verb))
        {
            pay_for_spell(a, aimed.verb, spell);
            return;
        }
    }
    if (!again && spell.id != "dragons_breath_exhale" && !item_effect(aimed.verb) &&
        (countered(a, spell) || slowed_spell_fails(a, aimed.verb, spell)))
    {
        return;
    }
    // A wand's spell, a missile's Fireball and a scroll's spell have the item's
    // save DC (MAGIC-3, MAGIC-6).
    item_caster_ = from_item || from_scroll ? a.source.id : 0;
    item_dc_ = from_scroll ? scroll_dc(slot_level_of(aimed.verb, spell)) : 15;
    const detail::ResetAtExit<EntityId> item_caster_reset{item_caster_, 0};
    const detail::SetWhileAlive against_magic{against_magic_};
    if (spell.level && !again)
    {
        pay_for_spell(a, aimed.verb, spell);
    }
    if (spell.id == "dragons_breath_exhale")
    {
        log(a.source.name + " exhales Dragon's Breath.",
            {"{name} exhales Dragon's Breath.", {{"name", a.source.name}}});
    }
    else
    {
        log(a.source.name + " casts " + std::string(spell.label) + ".",
            {"{name} casts {spell}.",
             {{"name", a.source.name}, {"spell", std::string(spell.label), true}}});
    }
    // Heightened Spell takes the first enemy in the area.
    if (casting_with(Metamagic::heightened))
    {
        for (const auto cell :
             spell.storm
                 ? storm_cells(aimed.cubes)
                 : area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center))
        {
            for (const auto &other : actors_)
            {
                if (!heightened_target_ && !other.life.dead && occupies(other, cell) &&
                    other.source.side != a.source.side)
                {
                    heightened_target_ = other.source.id;
                }
            }
        }
    }
    mark_heightened(a, spell);
    if (spell.rider == detail::Rider::flaming_sphere)
    {
        begin_concentration(a, spell);
        Zone sphere{a.source.id, ZoneKind::flaming_sphere, {aimed.center}};
        sphere.slot = slot_level_of(aimed.verb, spell);
        zones_.push_back(std::move(sphere));
        return;
    }
    if (spell.rider == detail::Rider::moonbeam)
    {
        begin_concentration(a, spell);
        Zone moonbeam{a.source.id, ZoneKind::moonbeam, beam_cells(aimed.center)};
        moonbeam.slot = slot_level_of(aimed.verb, spell);
        zones_.push_back(std::move(moonbeam));
        const auto beam = zones_.back();
        for (auto &other : actors_)
        {
            if (in_zone(beam, other))
            {
                moonbeam_burns(beam, other);
            }
        }
        return;
    }
    if (spell.rider == detail::Rider::gust_of_wind)
    {
        begin_concentration(a, spell);
        const auto cells =
            area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center);
        zones_.push_back({a.source.id, ZoneKind::gust, cells});
        const auto line = zones_.back();
        for (auto &other : actors_)
        {
            if (other.source.id != a.source.id && in_zone(line, other))
            {
                blow(line, other);
            }
        }
        return;
    }
    if (spell.rider == detail::Rider::dispel_magic)
    {
        dispel(a, aimed.center, slot_level_of(aimed.verb, spell));
        return;
    }
    if (spell.rider == detail::Rider::magic_circle)
    {
        // A minute of casting: Concentration, and the caster's Action each turn.
        begin_concentration(a, spell);
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::circling,
                                    static_cast<int>(slot_level_of(aimed.verb, spell)));
        Zone circle{a.source.id, ZoneKind::circle,
                    area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center)};
        circle.warded = static_cast<unsigned>(ward_of(aimed.verb));
        zones_.push_back(std::move(circle));
        log(a.source.name + " begins a Magic Circle against " + ward_label(ward_of(aimed.verb)) +
                ".",
            {"{name} begins a Magic Circle against {types}.",
             {{"name", a.source.name}, {"types", ward_label(ward_of(aimed.verb)), true}}});
        return;
    }
    if (spell.rider == detail::Rider::guardian_of_faith)
    {
        // Eight hours: it lasts the fight, until it has dealt 60 damage.
        zones_.push_back({a.source.id,
                          ZoneKind::guardian,
                          {aimed.center},
                          elapsed_ms_ + 28800000,
                          spell_dc(a, spell.id)});
        log("A Guardian of Faith appears.", {"A Guardian of Faith appears.", {}});
        return;
    }
    if (spell.rider == detail::Rider::conjure_animals)
    {
        // The pack strikes only creatures that later come near it.
        begin_concentration(a, spell);
        Zone pack{a.source.id, ZoneKind::animals,
                  area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center)};
        pack.save_dc = spell_dc(a, spell.id);
        pack.slot = slot_level_of(aimed.verb, spell);
        zones_.push_back(std::move(pack));
        log("A pack of spirit animals appears.", {"A pack of spirit animals appears.", {}});
        return;
    }
    if (spell.rider == detail::Rider::insect_plague)
    {
        // Everyone in the swarm when it appears saves, once this turn.
        begin_concentration(a, spell);
        Zone swarm{a.source.id, ZoneKind::swarm,
                   area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center)};
        swarm.save_dc = area_dc(a, spell);
        swarm.slot = slot_level_of(aimed.verb, spell);
        zones_.push_back(swarm);
        for (auto &other : actors_)
        {
            if (in_zone(swarm, other))
            {
                swarm_strikes(swarm, other);
            }
        }
        return;
    }
    if (spell.rider == detail::Rider::tsunami)
    {
        // The wave strikes those in it as it rises, then moves at the start of
        // the caster's turns.
        begin_concentration(a, spell);
        Zone wave{a.source.id, ZoneKind::wave,
                  area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center)};
        wave.save_dc = area_dc(a, spell);
        wave.slot = 5;
        wave.dealt = static_cast<int>(
            wave_heading(aimed.verb, origin_toward(a, aimed.center), aimed.center));
        log("A wall of water rises.", {"A wall of water rises.", {}});
        damage_area(a, spell, aimed.verb, wave.cells, wave.save_dc);
        // Caught in its own wave, the caster may lose Concentration at once.
        if (a.concentration.active())
        {
            zones_.push_back(std::move(wave));
        }
        return;
    }
    if (spell.rider == detail::Rider::wall_of_thorns)
    {
        // The thorns pierce those in the wall as it rises; later they slash.
        begin_concentration(a, spell);
        Zone thorns{a.source.id, ZoneKind::thorns,
                    area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center)};
        thorns.save_dc = area_dc(a, spell);
        thorns.slot = slot_level_of(aimed.verb, spell);
        damage_area(a, spell, aimed.verb, thorns.cells, thorns.save_dc);
        zones_.push_back(std::move(thorns));
        log("A wall of thorns grows.", {"A wall of thorns grows.", {}});
        return;
    }
    if (spell.rider == detail::Rider::blade_barrier)
    {
        // Everyone in the wall when it rises saves, once this turn.
        begin_concentration(a, spell);
        Zone blades{a.source.id, ZoneKind::blades,
                    area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center)};
        blades.save_dc = area_dc(a, spell);
        blades.slot = slot_level_of(aimed.verb, spell);
        zones_.push_back(blades);
        for (auto &other : actors_)
        {
            if (in_zone(blades, other))
            {
                swarm_strikes(blades, other);
            }
        }
        return;
    }
    if (spell.rider == detail::Rider::call_lightning)
    {
        // The cloud forms over the caster with the casting; each bolt strikes
        // within 60 feet of the square under its centre.
        if (aimed.verb != "call_lightning_again")
        {
            begin_concentration(a, spell);
            Zone storm{a.source.id, ZoneKind::storm, {a.source.cell}};
            storm.slot = slot_level_of(aimed.verb, spell);
            zones_.push_back(std::move(storm));
        }
        auto bolt = spell;
        for (const auto &zone : zones_)
        {
            if (zone.kind == ZoneKind::storm && zone.caster == a.source.id)
            {
                bolt.dice.count = 3 + static_cast<int>(zone.slot - spell.level);
            }
        }
        log("Lightning strikes from the storm cloud.",
            {"Lightning strikes from the storm cloud.", {}});
        damage_area(a, bolt, bolt.id,
                    area_cells(bolt, bolt.id, origin_toward(a, aimed.center), aimed.center),
                    area_dc(a, spell));
        return;
    }
    if (spell.rider == detail::Rider::giant_insect)
    {
        // The form chosen at the casting, of the slot's level.
        const auto form = std::string(detail::without_cast_suffix(aimed.verb)
                                          .substr(std::string_view("giant_insect_").size()));
        begin_concentration(a, spell);
        const auto caster = a.source.id;
        const std::string name = form == "centipede" ? "Giant Centipede"
                                 : form == "spider"  ? "Giant Spider"
                                                     : "Giant Wasp";
        (void)summon(
            a, "giant-insect-" + form + "-" + std::to_string(slot_level_of(aimed.verb, spell)),
            name, aimed.center);
        log("A giant insect answers " + actor(caster).source.name + ".",
            {"A giant insect answers {name}.", {{"name", actor(caster).source.name}}});
        return;
    }
    if (spell.rider == detail::Rider::sunbeam)
    {
        // The first Line comes with the casting; later ones with a Magic action.
        if (aimed.verb != "sunbeam_line")
        {
            begin_concentration(a, spell);
            zones_.push_back({a.source.id, ZoneKind::sunbeam, {a.source.cell}});
        }
        damage_area(a, spell, aimed.verb,
                    area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center),
                    area_dc(a, spell));
        return;
    }
    if (spell.rider == detail::Rider::mass_cure_wounds)
    {
        mass_cure(a, spell, aimed.verb,
                  area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center));
        return;
    }
    if (spell.rider == detail::Rider::wind_wall)
    {
        begin_concentration(a, spell);
        const auto cells =
            area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center);
        zones_.push_back({a.source.id, ZoneKind::wind_wall, cells});
        damage_area(a, spell, aimed.verb, cells, area_dc(a, spell));
        return;
    }
    // Plant Growth's Overgrowth reaches only squares with plants, and lasts.
    if (spell.rider == detail::Rider::plant_growth)
    {
        std::vector<Cell> grown;
        for (const auto cell :
             area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center))
        {
            if (!board_.plants.empty() &&
                board_.plants[static_cast<std::size_t>(cell.y * board_.width + cell.x)])
            {
                grown.push_back(cell);
            }
        }
        if (grown.empty())
        {
            log("No plants grow there.", {"No plants grow there.", {}});
            return;
        }
        zones_.push_back({a.source.id, ZoneKind::overgrowth, std::move(grown),
                          std::numeric_limits<std::uint64_t>::max()});
        log("The plants grow thick and overgrown.", {"The plants grow thick and overgrown.", {}});
        return;
    }
    if (spell.rider == detail::Rider::arcane_hand)
    {
        // The hand acts at once, without a Bonus Action, this turn.
        begin_concentration(a, spell);
        Zone hand{a.source.id, ZoneKind::hand, {aimed.center}};
        hand.save_dc = area_dc(a, spell);
        hand.slot = slot_level_of(aimed.verb, spell);
        zones_.push_back(std::move(hand));
        const unsigned rest_of_turn = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
        detail::apply_poisoned(a.effects, effect_source(a), a.source.name, rest_of_turn,
                               detail::EffectKind::hand_ready);
        log("A hand of shimmering force appears.", {"A hand of shimmering force appears.", {}});
        return;
    }
    if (spell.rider == detail::Rider::cloudkill || spell.rider == detail::Rider::incendiary_cloud)
    {
        begin_concentration(a, spell);
        auto cells = area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center);
        std::erase(cells, aimed.center);
        cells.insert(cells.begin(), aimed.center);
        Zone cloud{a.source.id,
                   spell.rider == detail::Rider::cloudkill ? ZoneKind::cloud : ZoneKind::embers,
                   std::move(cells)};
        cloud.save_dc = area_dc(a, spell);
        cloud.slot = slot_level_of(aimed.verb, spell);
        zones_.push_back(cloud);
        for (auto &other : actors_)
        {
            if (in_zone(cloud, other))
            {
                choke_cloud(cloud, other);
            }
        }
        return;
    }
    if (spell.rider == detail::Rider::conjure_elemental)
    {
        // Air is Lightning, earth Thunder, fire Fire and water Cold.
        const auto element = detail::without_cast_suffix(aimed.verb)
                                 .substr(std::string_view("conjure_elemental_").size());
        const auto type = element == "air"     ? detail::DamageType::lightning
                          : element == "earth" ? detail::DamageType::thunder
                          : element == "water" ? detail::DamageType::cold
                                               : detail::DamageType::fire;
        begin_concentration(a, spell);
        std::vector<Cell> cells;
        for (const auto cell : {aimed.center, Cell{aimed.center.x + 1, aimed.center.y},
                                Cell{aimed.center.x, aimed.center.y + 1},
                                Cell{aimed.center.x + 1, aimed.center.y + 1}})
        {
            if (board_.contains(cell))
            {
                cells.push_back(cell);
            }
        }
        Zone spirit{a.source.id, ZoneKind::elemental, std::move(cells)};
        spirit.save_dc = area_dc(a, spell);
        spirit.slot = slot_level_of(aimed.verb, spell);
        zones_.push_back(std::move(spirit));
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::elemental_spirit, static_cast<int>(type));
        log("An elemental spirit appears.", {"An elemental spirit appears.", {}});
        return;
    }
    if (spell.rider == detail::Rider::summon_dragon)
    {
        // Its breath and the caster's shared Resistance are of the type chosen.
        const auto type =
            detail::damage_type(detail::without_cast_suffix(aimed.verb)
                                    .substr(std::string_view("summon_dragon_").size()));
        begin_concentration(a, spell);
        const auto caster = a.source.id;
        auto &spirit =
            summon(a, "draconic-spirit-" + std::to_string(slot_level_of(aimed.verb, spell)),
                   "Draconic Spirit", aimed.center);
        detail::apply_spell_benefit(spirit.effects, effect_source(actor(caster)),
                                    actor(caster).source.name, detail::EffectKind::spirit_breath,
                                    static_cast<int>(type));
        auto &summoner = actor(caster);
        detail::apply_spell_benefit(summoner.effects, effect_source(summoner), summoner.source.name,
                                    detail::EffectKind::shared_resistance, static_cast<int>(type));
        log("A Draconic Spirit answers " + summoner.source.name + ".",
            {"A Draconic Spirit answers {name}.", {{"name", summoner.source.name}}});
        return;
    }
    if (spell.rider == detail::Rider::wall_of_ice)
    {
        // A creature in its way is pushed aside and saves against the cold.
        begin_concentration(a, spell);
        const auto cells =
            area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center);
        std::vector<Cell> struck;
        for (const auto &other : actors_)
        {
            if (!other.life.dead && other.source.id != a.source.id && touches(cells, other))
            {
                struck.push_back(other.source.cell);
            }
        }
        damage_area(a, spell, aimed.verb, struck, area_dc(a, spell));
        zones_.push_back({a.source.id, ZoneKind::ice_wall, cells});
        for (auto &other : actors_)
        {
            if (!other.life.dead && touches(cells, other))
            {
                if (const auto squares = free_squares_near(other.source.cell, footprint(other));
                    !squares.empty())
                {
                    other.source.cell = squares.front();
                    log(other.source.name + " is pushed aside by the wall.",
                        {"{name} is pushed aside by the wall.", {{"name", other.source.name}}});
                }
            }
        }
        log("A wall of ice rises.", {"A wall of ice rises.", {}});
        return;
    }
    if (spell.rider == detail::Rider::wall_of_stone || spell.rider == detail::Rider::wall_of_force)
    {
        // Stone cannot fill a creature's square; force pushes the creature
        // to the nearest free square beside the wall.
        begin_concentration(a, spell);
        const bool stone = spell.rider == detail::Rider::wall_of_stone;
        auto cells = area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center);
        if (stone)
        {
            std::erase_if(cells,
                          [&](Cell cell)
                          {
                              return std::any_of(actors_.begin(), actors_.end(),
                                                 [&](const auto &other)
                                                 {
                                                     return !other.life.dead &&
                                                            occupies(other, cell);
                                                 });
                          });
        }
        zones_.push_back({a.source.id, stone ? ZoneKind::stone_wall : ZoneKind::force_wall, cells});
        for (auto &other : actors_)
        {
            if (!other.life.dead && touches(cells, other))
            {
                if (const auto squares = free_squares_near(other.source.cell, footprint(other));
                    !squares.empty())
                {
                    other.source.cell = squares.front();
                    log(other.source.name + " is pushed aside by the wall.",
                        {"{name} is pushed aside by the wall.", {{"name", other.source.name}}});
                }
            }
        }
        log(stone ? "A wall of stone rises." : "A wall of force springs up.",
            {stone ? "A wall of stone rises." : "A wall of force springs up.", {}});
        return;
    }
    if (spell.rider == detail::Rider::telekinesis)
    {
        auto &target = actor(aimed.companion);
        // A new creature takes the place of the one held before.
        for (auto &other : actors_)
        {
            std::erase_if(other.effects.active,
                          [&](const auto &e)
                          {
                              return e.kind == detail::EffectKind::lifted &&
                                     e.source_actor == a.source.id && e.source_scope == scope_;
                          });
        }
        if (aimed.verb != "telekinesis_again")
        {
            begin_concentration(a, spell);
            detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                        detail::EffectKind::telekinetic, 0);
        }
        log(a.source.name + " seizes " + target.source.name + " with telekinesis.",
            {"{name} seizes {target} with telekinesis.",
             {{"name", a.source.name}, {"target", target.source.name}}});
        if (sealed_off(target) ||
            saving_throw_succeeds(target, spell.save, area_dc(a, spell), false,
                                  heightened_against(target, a, spell.id)))
        {
            return;
        }
        hurl(a, target, aimed.center);
        if (!target.life.dead && detail::can_apply(target.effects))
        {
            const auto index = index_of(a.source.id);
            const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
            detail::apply_poisoned(target.effects, effect_source(a), a.source.name,
                                   next_turn_ms(a) + slot, detail::EffectKind::lifted);
        }
        return;
    }
    if (spell.rider == detail::Rider::faithful_hound)
    {
        // Eight hours: it lasts the fight.
        zones_.push_back({a.source.id,
                          ZoneKind::hound,
                          {aimed.center},
                          elapsed_ms_ + 28800000,
                          area_dc(a, spell)});
        log("A phantom watchdog appears.", {"A phantom watchdog appears.", {}});
        return;
    }
    if (spell.rider == detail::Rider::black_tentacles)
    {
        // Everyone in the tentacles when they rise saves, once this turn.
        begin_concentration(a, spell);
        Zone tentacles{a.source.id, ZoneKind::tentacles,
                       area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center)};
        tentacles.save_dc = area_dc(a, spell);
        tentacles.slot = slot_level_of(aimed.verb, spell);
        zones_.push_back(tentacles);
        for (auto &other : actors_)
        {
            if (in_zone(tentacles, other))
            {
                grope(tentacles, other);
            }
        }
        return;
    }
    if (spell.rider == detail::Rider::confusion)
    {
        begin_concentration(a, spell);
        const auto cells =
            area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center);
        const int dc = area_dc(a, spell);
        const auto careful = careful_allies(a, cells);
        for (auto &other : actors_)
        {
            if (other.life.dead || other.life.hp == 0 || other.source.id == a.source.id ||
                absent(other) || sealed_off(other) || !touches(cells, other) ||
                std::find(careful.begin(), careful.end(), other.source.id) != careful.end() ||
                !detail::can_apply(other.effects) ||
                saving_throw_succeeds(other, spell.save, dc, false,
                                      heightened_against(other, a, spell.id)))
            {
                continue;
            }
            detail::apply_repeating_condition(
                other.effects, detail::EffectKind::confused, effect_source(a), a.source.name,
                detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(other.source.id)});
            other.reaction = false;
            log(other.source.name + " is Confused.",
                {"{name} is Confused.", {{"name", other.source.name}}});
        }
        return;
    }
    if (spell.rider == detail::Rider::slow)
    {
        slow_area(a, spell,
                  area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center),
                  aimed.center);
        return;
    }
    // Forcecage: teleporting out takes a Charisma save, or the spell is wasted.
    if (spell.rider == detail::Rider::misty_step || spell.rider == detail::Rider::dimension_door)
    {
        for (const auto &zone : zones_)
        {
            if ((zone.kind == ZoneKind::cage || zone.kind == ZoneKind::box) && in_zone(zone, a) &&
                !in_zone(zone, aimed.center))
            {
                if (saving_throw_succeeds(a, detail::Ability::charisma, zone.save_dc))
                {
                    break;
                }
                log("The Forcecage holds " + a.source.name + ".",
                    {"The Forcecage holds {name}.", {{"name", a.source.name}}});
                return;
            }
        }
    }
    if (spell.rider == detail::Rider::delayed_blast_fireball)
    {
        // The bead waits for the spell to end, growing as each of the
        // caster's turns ends.
        begin_concentration(a, spell);
        auto cells = area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center);
        std::erase(cells, aimed.center);
        cells.insert(cells.begin(), aimed.center);
        Zone bead{a.source.id, ZoneKind::bead, std::move(cells)};
        bead.save_dc = area_dc(a, spell);
        bead.slot = slot_level_of(aimed.verb, spell);
        zones_.push_back(std::move(bead));
        log("A glowing bead hangs in the air.", {"A glowing bead hangs in the air.", {}});
        return;
    }
    if (spell.rider == detail::Rider::forcecage)
    {
        // The creatures inside are trapped; a box also cuts them off.
        begin_concentration(a, spell);
        const bool box = detail::without_cast_suffix(aimed.verb).ends_with("_box");
        Zone prison{a.source.id, box ? ZoneKind::box : ZoneKind::cage,
                    area_shape(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center)};
        prison.save_dc = area_dc(a, spell);
        zones_.push_back(std::move(prison));
        if (box)
        {
            log("A box of force closes.", {"A box of force closes.", {}});
        }
        else
        {
            log("A cage of force closes.", {"A cage of force closes.", {}});
        }
        return;
    }
    if (spell.rider == detail::Rider::reverse_gravity)
    {
        // Each creature in the cylinder that fails a Dexterity save falls
        // upward and hangs out of melee reach (LEVELS-3).
        begin_concentration(a, spell);
        const auto cells =
            area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center);
        const int dc = area_dc(a, spell);
        for (auto &other : actors_)
        {
            if (!other.life.dead && !absent(other) && !aloft(other) && touches(cells, other) &&
                !sealed_off(other) && !caged_apart(a.source.cell, other.source.cell, false) &&
                !globe_wards(a, other.source.cell, slot_level_of(aimed.verb, spell)) &&
                detail::can_apply(other.effects) &&
                !saving_throw_succeeds(other, spell.save, dc, false,
                                       heightened_against(other, a, spell.id)))
            {
                detail::apply_spell_benefit(other.effects, effect_source(a), a.source.name,
                                            detail::EffectKind::aloft, 0);
                log(other.source.name + " falls upward.",
                    {"{name} falls upward.", {{"name", other.source.name}}});
                fall_hard(other);
            }
        }
        return;
    }
    if (spell.rider == detail::Rider::dimension_door)
    {
        // The caster, and a willing creature beside it, step to the square.
        const Cell from = a.source.cell;
        a.source.cell = aimed.center;
        log(a.source.name + " steps through a door in space.",
            {"{name} steps through a door in space.", {{"name", a.source.name}}});
        if (aimed.companion)
        {
            auto &friend_ = actor(aimed.companion);
            if (!friend_.life.dead && distance(friend_, from) <= 5)
            {
                friend_.source.cell = nearest_open(friend_, aimed.center);
                log(friend_.source.name + " steps through with " + a.source.name + ".",
                    {"{name} steps through with {caster}.",
                     {{"name", friend_.source.name}, {"caster", a.source.name}}});
            }
        }
        clear_departed_overlaps();
        return;
    }
    if (spell.rider == detail::Rider::wall_of_fire)
    {
        // The wall burns those in it as it rises; its burning side burns
        // those that end their turns beside it.
        begin_concentration(a, spell);
        const auto cells =
            area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center);
        Zone wall{a.source.id, ZoneKind::fire_wall, cells};
        wall.slot = slot_level_of(aimed.verb, spell);
        Zone side{a.source.id, ZoneKind::fire_side, burning_side(aimed.verb, aimed.center, cells)};
        side.slot = wall.slot;
        zones_.push_back(wall);
        if (!side.cells.empty())
        {
            zones_.push_back(std::move(side));
        }
        damage_area(a, spell, aimed.verb, cells, area_dc(a, spell));
        return;
    }
    if (spell.rider == detail::Rider::ice_storm)
    {
        // The hailstones make Difficult Terrain until the end of the caster's
        // next turn.
        const auto cells =
            area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center);
        const auto index = index_of(a.source.id);
        const unsigned slot = turn_end_ms(index) - (index ? turn_end_ms(index - 1) : 0);
        zones_.push_back(
            {a.source.id, ZoneKind::hail, cells, elapsed_ms_ + next_turn_ms(a) + slot});
        damage_area(a, spell, aimed.verb, cells, area_dc(a, spell));
        return;
    }
    if (spell.rider == detail::Rider::hypnotic_pattern || spell.rider == detail::Rider::fear)
    {
        enchant_area(a, spell,
                     area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center));
        return;
    }
    if (spell.rider == detail::Rider::sleet_storm || spell.rider == detail::Rider::stinking_cloud)
    {
        // The sleet and the cloud act on creatures that enter or start their
        // turns there.
        begin_concentration(a, spell);
        Zone zone{a.source.id,
                  spell.rider == detail::Rider::sleet_storm ? ZoneKind::sleet : ZoneKind::stench,
                  area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center)};
        zone.save_dc = area_dc(a, spell);
        zones_.push_back(std::move(zone));
        return;
    }
    if (spell.rider == detail::Rider::conjure_celestial)
    {
        // The light falls on everyone in the pillar when it appears.
        begin_concentration(a, spell);
        Zone pillar{a.source.id, ZoneKind::pillar, pillar_cells(aimed.center)};
        pillar.save_dc = area_dc(a, spell);
        pillar.slot = slot_level_of(aimed.verb, spell);
        zones_.push_back(pillar);
        for (auto &other : actors_)
        {
            if (in_zone(pillar, other))
            {
                bathe(pillar, other);
            }
        }
        return;
    }
    if (spell.rider == detail::Rider::earthquake)
    {
        // The tremor shakes the ground, without regard to walls, as it starts.
        begin_concentration(a, spell);
        Zone quake{a.source.id, ZoneKind::quake,
                   area_shape(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center)};
        quake.save_dc = area_dc(a, spell);
        zones_.push_back(quake);
        tremor(quake);
        return;
    }
    if (spell.storm)
    {
        damage_area(a, spell, aimed.verb, storm_cells(aimed.cubes), area_dc(a, spell));
        return;
    }
    if (spell.rider == detail::Rider::sunburst)
    {
        // Spell Darkness in the Sphere ends.
        const auto cells =
            area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center);
        std::vector<EntityId> darkened;
        for (const auto &zone : zones_)
        {
            if (zone.kind == ZoneKind::darkness &&
                std::any_of(zone.cells.begin(), zone.cells.end(),
                            [&](Cell cell)
                            {
                                return std::find(cells.begin(), cells.end(), cell) != cells.end();
                            }))
            {
                darkened.push_back(zone.caster);
            }
        }
        for (const auto id : darkened)
        {
            log("The sunlight ends the magical Darkness.",
                {"The sunlight ends the magical Darkness.", {}});
            end_concentration(actor(id));
        }
        damage_area(a, spell, aimed.verb, cells, area_dc(a, spell));
        return;
    }
    if (spell.rider == detail::Rider::misty_step)
    {
        // Teleporting provokes no Opportunity Attacks.
        a.source.cell = aimed.center;
        clear_departed_overlaps();
        log(a.source.name + " steps through the mist.",
            {"{name} steps through the mist.", {{"name", a.source.name}}});
        return;
    }
    const auto cells = area_cells(spell, aimed.verb, origin_toward(a, aimed.center), aimed.center);
    if (spell.pattern == detail::SpellPattern::save_damage)
    {
        damage_area(a, spell, aimed.verb, cells, area_dc(a, spell));
        return;
    }
    if (spell.rider == detail::Rider::sleep || spell.rider == detail::Rider::color_spray ||
        spell.rider == detail::Rider::faerie_fire)
    {
        if (spell.concentration)
        {
            begin_concentration(a, spell);
        }
        condition_area(a, spell, cells);
        return;
    }
    if (spell.concentration)
    {
        begin_concentration(a, spell);
    }
    const auto kind = spell.rider == detail::Rider::fog_cloud      ? ZoneKind::fog
                      : spell.rider == detail::Rider::darkness     ? ZoneKind::darkness
                      : spell.rider == detail::Rider::silence      ? ZoneKind::silence
                      : spell.rider == detail::Rider::grease       ? ZoneKind::grease
                      : spell.rider == detail::Rider::web          ? ZoneKind::web
                      : spell.rider == detail::Rider::spike_growth ? ZoneKind::spikes
                                                                   : ZoneKind::plants;
    zones_.push_back({a.source.id, kind, cells, kind == ZoneKind::grease ? elapsed_ms_ + 60000 : 0,
                      spell_dc(a, spell.id)});
    if (kind == ZoneKind::grease || kind == ZoneKind::web)
    {
        const auto zone = zones_.back();
        for (auto &other : actors_)
        {
            if (other.source.id != a.source.id && in_zone(zone, other))
            {
                (void)spring_zone(zone, other);
            }
        }
        return;
    }
    if (kind != ZoneKind::plants)
    {
        return;
    }
    const int dc = spell_dc(a, spell.id);
    for (auto &other : actors_)
    {
        if (other.life.dead || other.source.id == a.source.id || !touches(cells, other) ||
            !detail::can_apply(other.effects) || saving_throw_succeeds(other, spell.save, dc))
        {
            continue;
        }
        detail::apply_entangle(other.effects, effect_source(a), a.source.name, dc);
        log(other.source.name + " is Restrained.",
            {"{name} is Restrained.", {{"name", other.source.name}}});
    }
}

void Session::blessed_healer(Actor &cleric, const detail::SpellDef &spell, std::string_view verb,
                             std::span<const EntityId> healed)
{
    // Blessed Healer (Life Domain 6): a spell slot's healing of another creature
    // also restores 2 + the slot's level to the Cleric.
    if (!def(cleric).blessed_healer || !spell.level || verb.ends_with(detail::initiate_suffix) ||
        std::none_of(healed.begin(), healed.end(),
                     [&](EntityId id)
                     {
                         return id != cleric.source.id;
                     }))
    {
        return;
    }
    log("Blessed Healer: " + cleric.source.name + " is healed too.",
        {"Blessed Healer: {name} is healed too.", {{"name", cleric.source.name}}});
    heal(cleric, 2 + static_cast<int>(slot_level_of(verb, spell)));
}

int Session::disciple_of_life(const Actor &caster, unsigned slot_level) const
{
    // Disciple of Life: healing from a spell slot restores 2 + the slot's level more.
    return def(caster).life_domain ? 2 + static_cast<int>(slot_level) : 0;
}

void Session::preserve_life(Actor &cleric)
{
    // Five times the Cleric level, divided among Bloodied allies within 30
    // feet, none raised above half its Hit Point maximum. The most hurt first.
    log(cleric.source.name + " uses Preserve Life.",
        {"{name} uses Preserve Life.", {{"name", cleric.source.name}}});
    int pool = 5 * def(cleric).class_levels[CharacterClass::cleric];
    std::vector<Actor *> bloodied;
    for (auto &other : actors_)
    {
        if (other.source.side == cleric.source.side && !other.life.dead &&
            other.life.hp * 2 <= max_hp(other) && distance(cleric, other) <= 30 &&
            def(other).creature_type != "undead" && def(other).creature_type != "construct")
        {
            bloodied.push_back(&other);
        }
    }
    std::sort(bloodied.begin(), bloodied.end(),
              [&](const Actor *x, const Actor *y)
              {
                  return x->life.hp * max_hp(*y) < y->life.hp * max_hp(*x);
              });
    for (auto *other : bloodied)
    {
        const int room = max_hp(*other) / 2 - other->life.hp;
        if (pool <= 0 || room <= 0)
        {
            continue;
        }
        pool -= heal(*other, std::min(pool, room));
    }
}

void Session::divine_spark(Actor &cleric, Actor &target, bool harm)
{
    // 1d8 + Wisdom modifier (2d8 from Cleric 7, 3d8 from 13): restored, or
    // Radiant damage (the SRD's Necrotic is not offered, SPARK-1) halved on a
    // Constitution save.
    const int cleric_level = def(cleric).class_levels[CharacterClass::cleric];
    const int sparks = cleric_level >= 13 ? 3 : cleric_level >= 7 ? 2 : 1;
    const int total = std::max(0, dice({sparks, 8, 0}) + casting_modifier(def(cleric)));
    log(cleric.source.name + " uses Divine Spark on " + target.source.name + ".",
        {"{name} uses Divine Spark on {target}.",
         {{"name", cleric.source.name}, {"target", target.source.name}}});
    if (!harm)
    {
        heal(target,
             detail::has_effect(target.effects, detail::EffectKind::hopeful)
                 ? std::max(0, healing_roll(target, {sparks, 8, casting_modifier(def(cleric))}))
                 : total);
        return;
    }
    const auto type = detail::DamageType::radiant;
    const bool saved =
        saving_throw_succeeds(target, detail::Ability::constitution, spell_dc(cleric));
    const int amount = resolved_damage(target, type, saved ? total / 2 : total);
    const auto name = std::string(detail::damage_name(type));
    log(target.source.name + " takes " + std::to_string(amount) + " " + name + " damage.",
        {"{name} takes {damage} {type} damage.",
         {{"name", target.source.name}, {"damage", std::to_string(amount)}, {"type", name, true}}});
    damage(target, amount, false);
}

void Session::turn_undead(Actor &cleric)
{
    log(cleric.source.name + " uses Turn Undead.",
        {"{name} uses Turn Undead.", {{"name", cleric.source.name}}});
    // Sear Undead (Cleric 5): one roll of a d8 for each point of the Wisdom
    // modifier (at least one), Radiant damage to each Undead that fails; the
    // damage does not end the turning.
    const int seared =
        def(cleric).sear_undead ? dice({std::max(1, casting_modifier(def(cleric))), 8, 0}) : 0;
    for (auto &other : actors_)
    {
        if (other.source.side != cleric.source.side && other.life.hp > 0 &&
            def(other).creature_type == "undead" && distance(cleric, other) <= 30 &&
            detail::can_apply(other.effects) &&
            !saving_throw_succeeds(other, detail::Ability::wisdom, spell_dc(cleric)))
        {
            if (seared)
            {
                const int amount = resolved_damage(other, detail::DamageType::radiant, seared);
                log(other.source.name + " takes " + std::to_string(amount) +
                        " Radiant damage from Sear Undead.",
                    {"{name} takes {damage} Radiant damage from Sear Undead.",
                     {{"name", other.source.name}, {"damage", std::to_string(amount)}}});
                damage(other, amount);
                if (other.life.dead || other.life.hp == 0)
                {
                    continue;
                }
            }
            detail::apply_spell_benefit(other.effects, effect_source(cleric), cleric.source.name,
                                        detail::EffectKind::turned, 0);
            log(other.source.name + " is turned.",
                {"{name} is turned.", {{"name", other.source.name}}});
        }
    }
}

void Session::hold_still(std::vector<Command> &commands, const Actor &a) const
{
    // Resilient Sphere: the creature only rolls its sphere, with its action,
    // or ends its turn.
    if (detail::has_effect(a.effects, detail::EffectKind::sphered))
    {
        std::erase_if(commands,
                      [&](const auto &offer)
                      {
                          return offer.verb != "end" &&
                                 (offer.verb != "move" || (!a.moved && !a.actions.available()));
                      });
    }
    // Incapacitated (Paralyzed, Sleep, Hideous Laughter): it can only end its
    // turn. Adaptation: a Prone, laughing creature could otherwise crawl.
    if (!detail::incapacitated(a.effects))
    {
        return;
    }
    const auto end = std::find_if(commands.begin(), commands.end(),
                                  [](const auto &offer)
                                  {
                                      return offer.verb == "end";
                                  });
    // Fear or Command may already have narrowed the choices to fleeing.
    auto kept =
        end != commands.end() ? *end : Command{revision_, a.source.id, 0, "end", "End turn"};
    commands.clear();
    commands.push_back(std::move(kept));
}

void Session::flee_turning(std::vector<Command> &commands, const Actor &a) const
{
    // Turned: Incapacitated, it can only move as far from the Cleric as it can.
    const auto turned = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                     [](const auto &e)
                                     {
                                         return e.kind == detail::EffectKind::turned;
                                     });
    if (turned == a.effects.active.end())
    {
        return;
    }
    const auto cleric = std::find_if(actors_.begin(), actors_.end(),
                                     [&](const auto &other)
                                     {
                                         return turned->source_scope == scope_ &&
                                                other.source.id == turned->source_actor;
                                     });
    if (cleric == actors_.end() || !conscious(*cleric))
    {
        return;
    }
    const Command *farthest = nullptr;
    int best = distance(a, *cleric);
    for (const auto &offer : commands)
    {
        if (offer.verb == "move" && distance(offer.destination, *cleric) > best)
        {
            farthest = &offer;
            best = distance(offer.destination, *cleric);
        }
    }
    const auto end = std::find_if(commands.begin(), commands.end(),
                                  [](const auto &offer)
                                  {
                                      return offer.verb == "end";
                                  });
    // Fear or Command may already have narrowed the choices to fleeing.
    auto kept = farthest                ? *farthest
                : end != commands.end() ? *end
                                        : Command{revision_, a.source.id, 0, "end", "End turn"};
    commands.clear();
    commands.push_back(std::move(kept));
}

void Session::resolve_ensnaring_strike(Actor &a, std::string_view verb)
{
    // Vines grasp the creature just hit; a Large or larger one has Advantage.
    const auto &spell = *detail::find_spell("ensnaring_strike");
    auto &target = actor(a.smite_target);
    a.smite_target = 0;
    a.bonus = false;
    pay_for_spell(a, verb, spell);
    log(a.source.name + " casts Ensnaring Strike on " + target.source.name + ".",
        {"{name} casts Ensnaring Strike on {target}.",
         {{"name", a.source.name}, {"target", target.source.name}}});
    const int dc = spell_dc(a);
    if (target.life.hp == 0 || !detail::can_apply(target.effects) ||
        saving_throw_succeeds(target, detail::Ability::strength, dc, def(target).size >= 3))
    {
        return;
    }
    begin_concentration(a, spell);
    detail::apply_ensnaring_strike(target.effects, effect_source(a), a.source.name, dc);
    // The vines' dice live on the caster, ending with its Concentration.
    detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                detail::EffectKind::ensnaring,
                                static_cast<int>(1 + upcast_steps(verb, spell)));
    log(target.source.name + " is Restrained.",
        {"{name} is Restrained.", {{"name", target.source.name}}});
}

void Session::squeeze_ensnared(Actor &a)
{
    // Ensnaring Strike's vines deal 1d6 Piercing at the start of each turn,
    // more from a higher slot: the caster's ensnaring effect holds the dice.
    const auto vines = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                    [](const auto &e)
                                    {
                                        return e.kind == detail::EffectKind::ensnaring_strike;
                                    });
    if (vines == a.effects.active.end() || a.life.hp == 0)
    {
        return;
    }
    int count = 1;
    for (const auto &caster : actors_)
    {
        if (caster.source.id == vines->source_actor)
        {
            for (const auto &e : caster.effects.active)
            {
                if (e.kind == detail::EffectKind::ensnaring)
                {
                    count = e.dc;
                }
            }
        }
    }
    const int amount = resolved_damage(a, detail::DamageType::piercing, dice({count, 6, 0}));
    log(a.source.name + " takes " + std::to_string(amount) + " Piercing damage from the vines.",
        {"{name} takes {damage} Piercing damage from the vines.",
         {{"name", a.source.name}, {"damage", std::to_string(amount)}}});
    damage(a, amount, false);
}

int Session::guidance_bonus(const Actor &a, std::string_view skill)
{
    const bool guided =
        std::any_of(a.effects.active.begin(), a.effects.active.end(),
                    [&](const auto &e)
                    {
                        return e.kind == detail::EffectKind::guidance &&
                               detail::guidance_skills[static_cast<std::size_t>(e.dc)] == skill;
                    });
    if (!guided)
    {
        return 0;
    }
    const int bonus = roll(4);
    log(a.source.name + "'s Guidance adds " + std::to_string(bonus) + ".",
        {"{name}'s Guidance adds {amount}.",
         {{"name", a.source.name}, {"amount", std::to_string(bonus)}}});
    return bonus;
}

Session::AthleticsRoll Session::roll_athletics(Actor &a, int dc)
{
    // A creature's Strength save bonus stands in for Athletics. Rage and the
    // Champion's Remarkable Athlete give the check Advantage.
    AthleticsRoll check;
    check.advantage = rage_of(a) || def(a).champion ||
                      detail::has_effect(a.effects, detail::EffectKind::enhance_ability);
    check.natural = reliable(
        a, "athletics", detail::d20({check.advantage, check_disadvantage(a), def(a).lucky}, rng_));
    check.bonus = (a.source.character_profile.empty() ? def(a).saves[0] : def(a).athletics) +
                  guidance_bonus(a, "athletics");
    check.success = check.natural + check.bonus >= dc;
    // An enemy Lore Bard's Cutting Words may spoil it; the check's line shows
    // the lowered bonus, as an attack's does.
    if (check.success)
    {
        check.bonus -= cutting_words(a, check.natural + check.bonus - dc, CutRoll::check);
        check.success = check.natural + check.bonus >= dc;
    }
    int kept = check.natural;
    if (!check.success && check.bonus + 20 >= dc)
    {
        if (const auto again = heroic_reroll(a))
        {
            kept = *again;
            check.success = *again + check.bonus >= dc;
        }
    }
    if (!check.success)
    {
        check.bonus += dark_luck_check(a, dc - kept - check.bonus);
        check.success = kept + check.bonus >= dc;
    }
    return check;
}

// Bardic Inspiration or Peerless Skill, for a failed check.
bool Session::rescue_athletics(Actor &a, const AthleticsRoll &check, int dc)
{
    const int shortfall = dc - check.natural - check.bonus;
    return inspiration_roll(a, shortfall) >= shortfall || peerless_skill(a, shortfall) > 0;
}

void Session::escape_ensnaring(Actor &a)
{
    // A Strength (Athletics) check against the spell's DC ends Ensnaring
    // Strike.
    const auto found = std::find_if(a.effects.active.begin(), a.effects.active.end(),
                                    [](const auto &e)
                                    {
                                        return e.kind == detail::EffectKind::ensnaring_strike ||
                                               e.kind == detail::EffectKind::entangle ||
                                               e.kind == detail::EffectKind::webbed ||
                                               e.kind == detail::EffectKind::tentacled;
                                    });
    const auto vines = *found;
    const auto check = roll_athletics(a, vines.dc);
    const int natural = check.natural, bonus = check.bonus;
    bool escaped = check.success;
    const std::string advantage_label = check.advantage ? " (advantage)" : "";
    log(a.source.name + " Athletics check: d20 " + std::to_string(natural) + advantage_label +
            " + " + std::to_string(bonus) + " vs DC " + std::to_string(vines.dc) +
            (escaped ? " (escapes)." : " (still Restrained)."),
        {escaped
             ? "{name} Athletics check: d20 {roll}{advantage} + {bonus} vs DC {dc} (escapes)."
             : "{name} Athletics check: d20 {roll}{advantage} + {bonus} vs DC {dc} (still Restrained).",
         {{"name", a.source.name},
          {"roll", std::to_string(natural)},
          {"advantage", advantage_label, true},
          {"bonus", std::to_string(bonus)},
          {"dc", std::to_string(vines.dc)}}});
    if (!escaped)
    {
        escaped = rescue_athletics(a, check, vines.dc);
    }
    if (!escaped)
    {
        return;
    }
    // Ensnaring Strike ends, and with it the caster's Concentration; Entangle
    // only lets this creature go.
    if (vines.kind == detail::EffectKind::ensnaring_strike)
    {
        for (auto &caster : actors_)
        {
            if (vines.source_scope == scope_ && caster.source.id == vines.source_actor)
            {
                end_concentration(caster);
            }
        }
    }
    std::erase_if(a.effects.active,
                  [&](const auto &e)
                  {
                      return e.id == vines.id;
                  });
}

int Session::blessing_die(const Actor &a)
{
    return (detail::has_effect(a.effects, detail::EffectKind::bless) ? roll(4) : 0) -
           (detail::has_effect(a.effects, detail::EffectKind::bane) ? roll(4) : 0);
}

unsigned Session::selection_maximum(const PendingSelection &selection) const
{
    const auto &spell = *detail::find_spell(selection.verb);
    if (spell.rider == detail::Rider::abjure_foes)
    {
        return static_cast<unsigned>(def(actor(selection.caster)).abjure_foes);
    }
    // Eldritch Blast: a beam for each cantrip step.
    if (spell.id == "eldritch_blast")
    {
        return static_cast<unsigned>(cantrip_tier(def(actor(selection.caster))));
    }
    // Twinned Spell raises the spell's effective level by one.
    const bool twinned = readies(actor(selection.caster), Metamagic::twinned, spell);
    return spell.instances +
           (upcast_steps(selection.verb, spell) + (twinned ? 1 : 0)) * spell.upcast.extra_instances;
}

void Session::choose_target(const Command &command)
{
    if (command.verb == "spell_cancel")
    {
        give_up_spell(selection_->verb);
        selection_.reset();
        return;
    }
    if (command.verb == "spell_cast")
    {
        cast_on_selection();
        return;
    }
    auto &chosen = selection_->chosen;
    const auto found = std::find(chosen.begin(), chosen.end(), command.target);
    if (found != chosen.end() && !splits_among_targets(*detail::find_spell(selection_->verb)))
    {
        chosen.erase(found);
    }
    else
    {
        chosen.push_back(command.target);
    }
    if (chosen.size() == selection_maximum(*selection_))
    {
        cast_on_selection();
    }
}

void Session::cast_on_selection()
{
    const auto selection = std::move(*selection_);
    selection_.reset();
    auto &a = actor(selection.caster);
    // Not a spell: Sanctuary, Invisibility and hiding stay.
    if (detail::find_spell(selection.verb)->rider == detail::Rider::abjure_foes)
    {
        abjure(a, selection.chosen);
        return;
    }
    end_sanctuary(a);
    const bool was_invisible = invisible(a);
    const auto id = a.source.id;
    cast_selected(a, selection);
    if (was_invisible)
    {
        end_invisibility(id);
    }
    end_hiding(id);
}

void Session::cast_selected(Actor &a, const PendingSelection &selection)
{
    const auto &spell = *detail::find_spell(selection.verb);
    // Fast Hands uses an item as a Bonus Action, as Quickened Spell casts one.
    const bool quickened =
        readies(a, Metamagic::quickened, spell) || bonus_action_verb(selection.verb);
    if (splits_among_targets(spell) || spell.pattern == detail::SpellPattern::heal ||
        spell.id == "chain_lightning")
    {
        if (quickened || spell.bonus_action)
        {
            a.bonus = false;
        }
        else
        {
            a.nick_origin = 0;
            (void)a.actions.spend(true);
        }
        const auto damaged_before = damage_taken_.size();
        cast_resolved_spell(spell, selection.verb, a, selection.chosen);
        ready_vitality(a, spell, damaged_before);
        return;
    }
    take_metamagic(a, spell);
    take_overchannel(a, spell, selection.verb);
    if (casting_with(Metamagic::heightened))
    {
        heightened_target_ = selection.chosen.front();
    }
    mark_heightened(a, spell);
    if (quickened)
    {
        a.bonus = false;
    }
    else
    {
        a.nick_origin = 0;
        (void)a.actions.spend(true);
    }
    pay_for_spell(a, selection.verb, spell);
    log(a.source.name + " casts " + std::string(spell.label) + ".",
        {"{name} casts {spell}.",
         {{"name", a.source.name}, {"spell", std::string(spell.label), true}}});
    if (spell.concentration)
    {
        begin_concentration(a, spell);
    }
    const int dc = spell_dc(a, spell.id);
    for (const auto id : selection.chosen)
    {
        auto &target = actor(id);
        if (spell.humanoid_only && def(target).creature_type != "humanoid")
        {
            continue;
        }
        if (spell.pattern == detail::SpellPattern::save_condition &&
            saving_throw_succeeds(target, spell.save, dc, fought_advantage(spell)))
        {
            continue;
        }
        apply_rider(spell, selection.verb, a, target, dc);
    }
}

// Frightened, while a source is in sight, Poisoned and Sunlight Sensitivity
// in sunlight give an ability check Disadvantage.
bool Session::check_disadvantage(const Actor &a) const
{
    return !fears_in_sight(a).empty() || suffers_poisoned(a) || in_sunlight(a);
}

bool Session::in_sunlight(const Actor &a) const
{
    return def(a).sunlight_sensitive && open_sky(a.source.cell);
}

bool Session::open_sky(Cell cell) const
{
    return daylight_ && !board_.plants.empty() &&
           board_.plants[static_cast<std::size_t>(cell.y * board_.width + cell.x)];
}

std::vector<const Actor *> Session::fears_in_sight(const Actor &a) const
{
    std::vector<const Actor *> result;
    // Aura of Courage: Frightened has no effect while the creature is in it;
    // Heroes' Feast makes it immune.
    if (aura_of_courage(a) || detail::has_effect(a.effects, detail::EffectKind::feasted))
    {
        return result;
    }
    for (const auto &e : a.effects.active)
    {
        if ((e.kind == detail::EffectKind::frightened || e.kind == detail::EffectKind::abjured ||
             e.kind == detail::EffectKind::terrified || e.kind == detail::EffectKind::eye_panic ||
             e.kind == detail::EffectKind::fey_fright || e.kind == detail::EffectKind::glared) &&
            e.source_scope == scope_)
        {
            for (const auto &other : actors_)
            {
                if (other.source.id == e.source_actor && !other.life.dead &&
                    line_of_sight(a, other))
                {
                    result.push_back(&other);
                }
            }
        }
    }
    return result;
}

void Session::intimidate(const Actor &barbarian)
{
    const auto &d = def(barbarian);
    const int dc = 8 + d.strength + 2 + (d.level - 1) / 4;
    log(barbarian.source.name + " uses Intimidating Presence.",
        {"{name} uses Intimidating Presence.", {{"name", barbarian.source.name}}});
    for (auto &other : actors_)
    {
        if (other.source.side == barbarian.source.side || !conscious(other) ||
            distance(barbarian, other) > 30 || !line_of_sight(barbarian, other) ||
            fearless(other) || detail::has_effect(other.effects, detail::EffectKind::frightened) ||
            !detail::can_apply(other.effects))
        {
            continue;
        }
        if (saving_throw_succeeds(other, detail::Ability::wisdom, dc, def(other).brave))
        {
            continue;
        }
        detail::apply_repeating_condition(
            other.effects, detail::EffectKind::frightened, effect_source(barbarian),
            barbarian.source.name,
            detail::RepeatingSave{.dc = dc, .first_save_ms = next_save_ms(other.source.id)});
        log(other.source.name + " is Frightened.",
            {"{name} is Frightened.", {{"name", other.source.name}}});
    }
}

bool Session::charmed_by(const Actor &a, EntityId other) const
{
    // Aura of Devotion: a Charm has no effect while its target is in the aura.
    if (aura_of_devotion(a))
    {
        return false;
    }
    return std::any_of(a.effects.active.begin(), a.effects.active.end(),
                       [&](const auto &e)
                       {
                           return (e.kind == detail::EffectKind::charmed ||
                                   e.kind == detail::EffectKind::enthralled ||
                                   e.kind == detail::EffectKind::turn_charmed ||
                                   e.kind == detail::EffectKind::compelled ||
                                   e.kind == detail::EffectKind::dominated) &&
                                  e.source_actor == other && e.source_scope == scope_;
                       });
}

// A Paladin's 10-foot Emanation reaches itself and its allies in a clear line,
// while it is conscious and not Incapacitated.
bool Session::aura_covers(const Actor &paladin, const Actor &a) const
{
    return def(paladin).aura_of_protection && paladin.source.side == a.source.side &&
           conscious(paladin) && !detail::incapacitated(paladin.effects) && !absent(a) &&
           distance(paladin, a) <= 10 && line_of_sight(paladin, a);
}

int Session::aura_of_protection(const Actor &a) const
{
    // A creature in two auras benefits from one; the game takes the larger.
    int bonus = 0;
    for (const auto &paladin : actors_)
    {
        if (aura_covers(paladin, a))
        {
            bonus = std::max(bonus, def(paladin).aura_of_protection);
        }
    }
    return bonus;
}

bool Session::aura_of_devotion(const Actor &a) const
{
    return std::any_of(actors_.begin(), actors_.end(),
                       [&](const auto &paladin)
                       {
                           return def(paladin).aura_of_devotion && aura_covers(paladin, a);
                       });
}

bool Session::aura_of_courage(const Actor &a) const
{
    return std::any_of(actors_.begin(), actors_.end(),
                       [&](const auto &paladin)
                       {
                           return def(paladin).aura_of_courage && aura_covers(paladin, a);
                       });
}

void Session::fall(Actor &a)
{
    if (a.source.side == Side::party && a.life.dead && detail::can_apply(a.effects))
    {
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::fallen, 0);
    }
    if (a.source.side == Side::party && a.life.dead && detail::can_apply(a.effects))
    {
        detail::apply_spell_benefit(a.effects, effect_source(a), a.source.name,
                                    detail::EffectKind::deceased, 0);
    }
}

void Session::revivify(Actor &cleric, const Command &command)
{
    // An Action and the offering: 300 gp times the creature's level, from the
    // party purse (LEVELS-1). It returns with 1 Hit Point, Prone.
    const auto &spell = *detail::find_spell("revivify");
    auto &target = actor(command.target);
    cleric.nick_origin = 0;
    (void)cleric.actions.spend(true);
    pay_for_spell(cleric, command.verb, spell);
    const auto offering = static_cast<unsigned>(300 * def(target).level);
    offerings_gold_ += offering;
    std::erase_if(target.effects.active,
                  [](const auto &e)
                  {
                      return e.kind == detail::EffectKind::fallen;
                  });
    target.life.dead = false;
    target.life.successes = target.life.failures = 0;
    target.life.stable = false;
    target.life.recovery = {};
    target.life.hp = 1;
    target.effects.prone = true;
    if (shares_occupied_space(target))
    {
        target.involuntary_overlap = true;
    }
    log(cleric.source.name + " casts Revivify: " + target.source.name +
            " returns to life (an offering of " + std::to_string(offering) + " gp).",
        {"{name} casts Revivify: {target} returns to life (an offering of {gold} gp).",
         {{"name", cleric.source.name},
          {"target", target.source.name},
          {"gold", std::to_string(offering)}}});
}

bool Session::guarded_square(Cell cell) const
{
    return std::any_of(zones_.begin(), zones_.end(),
                       [&](const auto &zone)
                       {
                           return zone.kind == ZoneKind::guardian && zone.cells.front() == cell;
                       });
}

void Session::guardian_tests(Actor &creature)
{
    // Guardian of Faith: an enemy within 10 feet saves once a turn against 20
    // Radiant damage, half on a success; the guardian vanishes at 60 dealt.
    for (std::size_t n = 0; n < zones_.size(); ++n)
    {
        auto &guardian = zones_[n];
        if (guardian.kind != ZoneKind::guardian || creature.life.hp == 0 || creature.life.dead ||
            absent(creature) ||
            detail::has_effect(creature.effects, detail::EffectKind::faith_tested) ||
            distance(guardian.cells.front(), creature) > 10)
        {
            continue;
        }
        const auto caster = std::find_if(actors_.begin(), actors_.end(),
                                         [&](const auto &a)
                                         {
                                             return a.source.id == guardian.caster;
                                         });
        if (caster == actors_.end() || caster->source.side == creature.source.side)
        {
            continue;
        }
        const unsigned rest_of_turn = turn_end_ms(turn_) - (turn_ ? turn_end_ms(turn_ - 1) : 0);
        if (detail::can_apply(creature.effects))
        {
            detail::apply_poisoned(creature.effects, effect_source(*caster), caster->source.name,
                                   rest_of_turn, detail::EffectKind::faith_tested);
        }
        const bool saved =
            saving_throw_succeeds(creature, detail::Ability::dexterity, guardian.save_dc);
        const int amount = resolved_damage(creature, detail::DamageType::radiant, saved ? 10 : 20);
        log(creature.source.name + " takes " + std::to_string(amount) +
                " Radiant damage from the Guardian of Faith.",
            {"{name} takes {damage} Radiant damage from the Guardian of Faith.",
             {{"name", creature.source.name}, {"damage", std::to_string(amount)}}});
        guardian.dealt += amount;
        const bool spent = guardian.dealt >= 60;
        if (spent)
        {
            zones_.erase(zones_.begin() + static_cast<std::ptrdiff_t>(n));
            log("The Guardian of Faith vanishes.", {"The Guardian of Faith vanishes.", {}});
        }
        damage(creature, amount);
        return;
    }
}

Cell Session::nearest_open(const Actor &a, std::optional<Cell> near) const
{
    const Cell from = near.value_or(a.source.cell);
    // Its own square, or the nearest one with room for it that no creature or
    // guardian holds.
    const auto open = [&](Cell cell)
    {
        return room_for(&a, footprint(a), cell);
    };
    // Measured to the nearest of its squares there, a space that still
    // covers `from` being no move at all; then by its top-left square, so a
    // creature that fits stays put.
    const auto away = [&](Cell cell)
    {
        return std::pair{distance(from, Footprint{}, cell, footprint(a)),
                         distance(a.source.cell, cell)};
    };
    std::optional<Cell> best;
    for (int y = 0; y < board_.height; ++y)
    {
        for (int x = 0; x < board_.width; ++x)
        {
            if (const Cell cell{x, y}; open(cell) && (!best || away(cell) < away(*best)))
            {
                best = cell;
            }
        }
    }
    return best.value_or(a.source.cell);
}

void Session::return_from_banishment(Actor &a)
{
    a.source.cell = nearest_open(a);
    log(a.source.name + " returns from its banishment.",
        {"{name} returns from its banishment.", {{"name", a.source.name}}});
}

bool Session::smite_protected(const Actor &a) const
{
    return std::any_of(actors_.begin(), actors_.end(),
                       [&](const auto &paladin)
                       {
                           return detail::has_effect(paladin.effects,
                                                     detail::EffectKind::smite_protected) &&
                                  aura_covers(paladin, a);
                       });
}

bool Session::in_aura_of_life(const Actor &a) const
{
    // The caster's 30-foot Emanation, which reaches allies in a clear line.
    return std::any_of(actors_.begin(), actors_.end(),
                       [&](const auto &caster)
                       {
                           return detail::has_effect(caster.effects,
                                                     detail::EffectKind::aura_of_life) &&
                                  caster.source.side == a.source.side &&
                                  distance(caster, a) <= 30 && line_of_sight(caster, a);
                       });
}

void Session::restore_touch(Actor &paladin, Actor &target)
{
    // Restoring Touch (Paladin 14): 5 points from the pool end each condition,
    // as many as the pool pays for.
    paladin.bonus = false;
    log(paladin.source.name + " uses Lay On Hands on " + target.source.name + ".",
        {"{name} uses Lay On Hands on {target}.",
         {{"name", paladin.source.name}, {"target", target.source.name}}});
    for (const auto &[condition, kinds] : restorable_conditions)
    {
        if (paladin.lay_on_hands[CharacterClass::paladin] < 5 ||
            !std::erase_if(
                target.effects.active,
                [&](const auto &e)
                {
                    return std::find(kinds.begin(), kinds.end(), e.kind) != kinds.end();
                }))
        {
            continue;
        }
        paladin.lay_on_hands[CharacterClass::paladin] -= 5;
        log(target.source.name + " is no longer " + std::string(condition) + ".",
            {"{name} is no longer {condition}.",
             {{"name", target.source.name}, {"condition", std::string(condition), true}}});
    }
}

bool Session::fearless(const Actor &a) const
{
    return mindless(a) || aura_of_courage(a);
}

void Session::abjure(Actor &paladin, std::span<const EntityId> targets)
{
    // A Magic action and a Channel Divinity use; each creature makes a Wisdom
    // save or is Frightened until it takes damage, for up to a minute.
    paladin.nick_origin = 0;
    (void)paladin.actions.spend(false);
    --paladin.channel_divinity[CharacterClass::cleric];
    log(paladin.source.name + " uses Abjure Foes.",
        {"{name} uses Abjure Foes.", {{"name", paladin.source.name}}});
    const int dc = spell_dc(paladin);
    for (const auto id : targets)
    {
        auto &target = actor(id);
        if (target.life.hp == 0 || fearless(target) || !detail::can_apply(target.effects) ||
            saving_throw_succeeds(target, detail::Ability::wisdom, dc, def(target).brave))
        {
            continue;
        }
        detail::apply_spell_benefit(target.effects, effect_source(paladin), paladin.source.name,
                                    detail::EffectKind::abjured, 0);
        log(target.source.name + " is Frightened.",
            {"{name} is Frightened.", {{"name", target.source.name}}});
    }
}

void Session::dispel(Actor &caster, Cell cell, unsigned slot_level)
{
    // A spell no higher than the slot ends; a higher one needs a spellcasting
    // ability check against 10 + its level.
    const auto ends = [&](unsigned level)
    {
        if (level <= slot_level)
        {
            return true;
        }
        const int natural = detail::d20({false, check_disadvantage(caster), false}, rng_),
                  bonus = casting_modifier(def(caster)), dc = 10 + static_cast<int>(level);
        log(caster.source.name + " Dispel Magic check: d20 " + std::to_string(natural) + " + " +
                std::to_string(bonus) + " vs DC " + std::to_string(dc) + ".",
            {"{name} Dispel Magic check: d20 {roll} + {bonus} vs DC {dc}.",
             {{"name", caster.source.name},
              {"roll", std::to_string(natural)},
              {"bonus", std::to_string(bonus)},
              {"dc", std::to_string(dc)}}});
        return natural + bonus >= dc ||
               natural + bonus + dark_luck_check(caster, dc - natural - bonus) >= dc;
    };
    // A Concentration spell ends for every creature and area it holds.
    const auto end_spell = [&](EntityId source)
    {
        for (auto &other : actors_)
        {
            if (other.source.id == source && other.concentration.active())
            {
                end_concentration(other);
                return true;
            }
        }
        return false;
    };
    for (auto &target : actors_)
    {
        if (target.life.dead || !occupies(target, cell))
        {
            continue;
        }
        const auto found = target.effects.active;
        for (const auto &effect : found)
        {
            const auto level = spell_level_of(effect.kind);
            const auto present = [&]
            {
                return std::any_of(target.effects.active.begin(), target.effects.active.end(),
                                   [&](const auto &e)
                                   {
                                       return e.id == effect.id;
                                   });
            };
            if (!level || !present() || !ends(*level))
            {
                continue;
            }
            log("Dispel Magic ends a spell on " + target.source.name + ".",
                {"Dispel Magic ends a spell on {name}.", {{"name", target.source.name}}});
            if (!concentration_effect(effect.kind) || effect.source_scope != scope_ ||
                !end_spell(effect.source_actor))
            {
                std::erase_if(target.effects.active,
                              [&](const auto &e)
                              {
                                  return e.id == effect.id;
                              });
            }
        }
    }
    // The spells whose areas cover the square: Entangle, Fog Cloud and Grease
    // are level 1, Guardian of Faith level 4, the others level 2.
    for (std::size_t n = zones_.size(); n-- > 0;)
    {
        if (n >= zones_.size() || !in_zone(zones_[n], cell))
        {
            continue;
        }
        const auto zone = zones_[n];
        const unsigned level = zone.kind == ZoneKind::plants || zone.kind == ZoneKind::fog ||
                                       zone.kind == ZoneKind::grease
                                   ? 1
                               : zone.kind == ZoneKind::guardian ? 4
                               : zone.kind == ZoneKind::circle   ? 3
                                                                 : 2;
        if (!ends(level))
        {
            continue;
        }
        log("Dispel Magic ends a spell's area.", {"Dispel Magic ends a spell's area.", {}});
        if (zone.ends_ms || !end_spell(zone.caster))
        {
            zones_.erase(zones_.begin() + static_cast<std::ptrdiff_t>(n));
        }
    }
}
} // namespace opengold::srd5::detail
