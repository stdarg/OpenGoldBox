#include "dark_queen_progression.h"
#include "dark_queen_rules.h"
#include "installed_games.h"
#include "opengold/campaign_save.h"
#include "opengold/campaign_session.h"
#include "opengold/character_creator.h"
#include "opengold/combat_demo.h"
#include "opengold/creature_catalog.h"
#include "opengold/dark_queen_import.h"
#include "opengold/dark_queen_records.h"
#include "opengold/death_knights_import.h"
#include "opengold/ecl.h"
#include "opengold/ecl_address.h"
#include "opengold/ecl_machine.h"
#include "opengold/exploration_view.h"
#include "opengold/game_census.h"
#include "opengold/original.h"
#include "opengold/original_combat.h"
#include "opengold/original_content.h"
#include "opengold/original_rules.h"
#include "opengold/original_spells.h"
#include "opengold/rolf_tour.h"
#include "opengold/title_screens.h"
#include "opengold/training_hall.h"
#include "opengold/ua_design.h"
#include "opengold/ua_formats.h"
#include "opengold/ua_view.h"
#include "original_creation_support.h"
#include "test_support.h"
#include "tour_access.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace
{

using opengold::test::check;
using Bytes = std::vector<std::uint8_t>;
constexpr auto game = opengold::GoldBoxGame::dark_queen_of_krynn;

Bytes packed(std::span<const std::uint8_t> raw)
{
    Bytes bytes{static_cast<std::uint8_t>(raw.size() >> 8), static_cast<std::uint8_t>(raw.size())};
    for (std::size_t at = 0; at < raw.size(); at += 128)
    {
        const auto size = std::min(std::size_t{128}, raw.size() - at);
        bytes.push_back(static_cast<std::uint8_t>(size - 1));
        bytes.insert(bytes.end(), raw.begin() + at, raw.begin() + at + size);
    }
    return bytes;
}

// The save against paralysis, poison and death (herostat.c, recalc_saves):
// after each class's table, a dwarf's or kender's Constitution raises the
// number needed by its step (4 at 16, 5 at 18), and Constitution above 18
// raises anyone's (1 at 19, 4 at 25). The shipped code raises it, so these
// make the save worse; a dwarf's and kender's own Constitution bonus on the
// roll (effect 94) cancels theirs against this save.
void native_constitution_raises_the_death_save()
{
    using opengold::original::ClassSlot;
    using opengold::original::dark_queen_saving_throws;
    using opengold::original::Race;
    const auto only = [](ClassSlot slot)
    {
        opengold::original::ClassLevels levels{};
        levels[static_cast<std::size_t>(slot)] = 5;
        return levels;
    };
    const auto fighter = only(ClassSlot::fighter);
    const auto table = dark_queen_saving_throws(fighter, Race::krynn_human, 16);
    const auto raised = [&](Race race, int constitution)
    {
        const auto saves = dark_queen_saving_throws(fighter, race, constitution);
        check(std::equal(saves.begin() + 1, saves.end(), table.begin() + 1),
              "Constitution moves only the save against paralysis, poison and death");
        return saves[0] - table[0];
    };
    check(raised(Race::hill_dwarf, 16) == 4 && raised(Race::mountain_dwarf, 7) == 2 &&
              raised(Race::kender, 18) == 5,
          "A dwarf's or kender's Constitution raises the number it needs");
    check(raised(Race::krynn_human, 18) == 0 && raised(Race::krynn_human, 19) == 1 &&
              raised(Race::krynn_human, 25) == 4,
          "Anyone's Constitution above 18 raises it");
    // A cleric/fighter dwarf: the cleric's table plus 4, then the lower of
    // that and the fighter's, plus 4 again.
    auto both = fighter;
    both[static_cast<std::size_t>(ClassSlot::cleric)] = 5;
    const int cleric = dark_queen_saving_throws(only(ClassSlot::cleric), Race::krynn_human, 16)[0];
    check(dark_queen_saving_throws(both, Race::hill_dwarf, 16)[0] ==
              std::min(cleric + 4, table[0]) + 4,
          "Each of a dwarf's classes adds it after that class's table");
}

void native_progression_and_spells_use_dark_queen_rules()
{
    using namespace opengold;
    using namespace original;
    using A = rules::Ability;
    check(original_mask_classes(64, game) == std::vector<std::string_view>{"knight"} &&
              original_mask_classes(32, game) == std::vector<std::string_view>{"ranger"} &&
              original_mask_classes(127, game).size() == 7 && original_training_fee(game) == 0,
          "Native halls train their seven actual classes using class_bit and charge no fee");
    check(canonical_effect(13, game) == 413 && canonical_effect(66, game) == 115 &&
              canonical_effect(67, game) == 537 && canonical_effect(93, game) == 112 &&
              canonical_effect(97, game) == 97 && canonical_effect(101, game) == 85 &&
              canonical_effect(102, game) == 86 && canonical_effect(108, game) == 708 &&
              canonical_effect(121, game) == 1774 && canonical_effect(7, game) != 7,
          "Native effects retain their own meanings and separate primary and secondary fire hits");
    check(starting_experience(Profession::knight, game) == 1000001 &&
              starting_experience(Profession::cleric_magic_user, game) == 500000 &&
              starting_experience(Profession::cleric_fighter_magic_user, game) == 333333,
          "Creation uses the installed game's exact XP shares");
    check(dark_queen_experience_for_level(ClassSlot::fighter, 18) == 2500001 &&
              dark_queen_experience_for_level(ClassSlot::fighter, 40) == 8000001 &&
              !dark_queen_experience_for_level(ClassSlot::fighter, 41) &&
              dark_queen_experience_for_level(ClassSlot::magic_user, 20, {0, 1, 0}) == 3000000 &&
              dark_queen_experience_for_level(ClassSlot::magic_user, 20, {0, 2, 0}) == 2500000,
          "Progression continues beyond eighteen with the native class and robe steps");
    const auto half_elves = professions_for(Race::krynn_half_elf, game);
    check(std::ranges::find(half_elves, Profession::knight) != half_elves.end() &&
              dark_queen_level_limit(Race::krynn_half_elf, ClassSlot::monk) == 10 &&
              dark_queen_level_limit(Race::krynn_half_elf, ClassSlot::monk, {0, 0, 3}) == 40 &&
              dark_queen_alignment_allowed(Profession::thief, Alignment::neutral_good) &&
              !dark_queen_alignment_allowed(Profession::thief, Alignment::lawful_good),
          "Native knight slot one and the Rose cap do not become a druid or obsolete class seven");
    check(ability_limits(Race::qualinesti_elf, Gender::male, game).minimum[A::wisdom] == 6 &&
              ability_limits(Race::qualinesti_elf, Gender::male, GoldBoxGame::champions_of_krynn)
                      .minimum[A::wisdom] == 3,
          "Dark Queen race floors preserve earlier games");
    ClassLevels knight{};
    knight[7] = 10;
    const ModifiedCharacter modified_knight{
        Race::krynn_human, Gender::male, Profession::knight, knight, {0, 0, 1}};
    RandomState hit_dice{7};
    check(modify_hit_point_maximum(modified_knight, 18, game) == 142 &&
              roll_training_hit_dice(knight, std::array{ClassSlot::monk}, game, hit_dice) == 2,
          "Native tenth-level knights use their own maximum and fixed training gain");
    ClassLevels mage{};
    mage[5] = 29;
    const auto capable = dark_queen_base_slots(mage, 18, 18, {0, 1, 0});
    const auto limited = dark_queen_base_slots(mage, 18, 11, {0, 1, 0});
    check(capable[3][0] == 8 && capable[3][8] == 6 && limited[3][5] == 0 && limited[3][8] == 0,
          "Native full slot rows retain the extra first-level mage spell and Intelligence gates");
    const auto initial = dark_queen_starting_spells(1, capable[3]);
    check(std::ranges::find(initial, 15) != initial.end() &&
              std::ranges::find(initial, 17) != initial.end() &&
              std::ranges::find(initial, 7) == initial.end() && elf_weapon(21, game) &&
              !elf_weapon(36, game),
          "Native creation teaches its first-level mage spells and uses native elven weapon types");
    ClassLevels cleric{};
    cleric[0] = 29;
    check(dark_queen_base_slots(cleric, 18, 18, {4, 0, 0})[0][6] == 7 &&
              dark_queen_base_slots(cleric, 16, 18, {4, 0, 0})[0][5] == 0,
          "High cleric spells require their native Wisdom");
    ClassLevels ranger{};
    ranger[4] = 12;
    check(dark_queen_caster_level(ranger, SpellClass::magic_user, 0, 3) == 4 &&
              dark_queen_caster_level(ranger, SpellClass::druid, 0, 3) == 5,
          "Ranger mage and druid spells use their distinct native caster levels");
    for (int own = 1; own <= 137; ++own)
    {
        const auto number = canonical_spell(own, game);
        check(!number || spell(number, game),
              "Every named native spell has existing mechanics: " + std::to_string(own));
    }
    check(canonical_spell(57, game) == 501 && canonical_spell(63, game) == 506 &&
              canonical_spell(101, game) == 301 && canonical_spell(124, game) == 324 &&
              native_spell(501, game) == 57 && !canonical_spell(65, game) &&
              spell(324, game)->level == 9 && spell(501, game)->level == 0,
          "Native deity gifts, empty rows and ninth-level spells retain their identities");
}

void every_native_creation_choice_produces_a_live_profile()
{
    using namespace opengold;
    using namespace original;
    const std::shared_ptr<const rules::CharacterRules> creation =
        character_rules(Aging::original, game);
    Content content;
    content.game = game;
    std::shared_ptr<const por::PhlanResources> assets;
    if (const auto folder = test::installed_game_folder(game); !folder.empty())
    {
        content = load_original_content(folder, game);
        assets = load_campaign(folder, game).resources();
    }
    const auto module = rules_module(content);
    check(game_of(creation->identity()) == game, "Persist a separate Dark Queen rules identity");
    RandomState random{1987};
    unsigned created = 0;
    for (const auto &race : creation->choices(rules::CreationField::race))
    {
        for (const auto &profession : creation->choices(rules::CreationField::character_class))
        {
            rules::CharacterDraft draft;
            draft.name = "Native hero";
            draft.race = race.id;
            draft.gender = "male";
            if (!creation->allows(draft, rules::CreationField::character_class, profession.id))
            {
                continue;
            }
            draft.character_class = profession.id;
            draft.background =
                (race.id == "mountain_dwarf" || race.id == "hill_dwarf") ? "reorx" : "mishakal";
            if (!creation->allows(draft, rules::CreationField::background, draft.background))
            {
                draft.background.clear();
            }
            for (const auto &alignment : creation->choices(rules::CreationField::alignment))
            {
                if (creation->allows(draft, rules::CreationField::alignment, alignment.id))
                {
                    draft.alignment = alignment.id;
                    break;
                }
            }
            check(!draft.alignment.empty(), "Each native class offers an eligible alignment");
            creation->roll_in_place(draft, random);
            const auto sheet = creation->evaluate(draft, rules::NameRequirement::required);
            const auto &n = *sheet.original;
            check(
                game_of(sheet.identity) == game && sheet.hit_points > 0 && n.platinum == 200 &&
                    n.experience ==
                        starting_experience(static_cast<Profession>(n.profession), game),
                "Every native class creates alive with exact starting experience and 200 steel: " +
                    race.id + "/" + profession.id);
            if (profession.id == "knight")
            {
                check(n.knight_order == 1 &&
                          n.class_levels[7] ==
                              (static_cast<Race>(n.race) == Race::krynn_human ? 11 : 10) &&
                          n.class_levels[1] == 0 && sheet.level_classes.front() == "knight",
                      "New Crown Knights respect the half-elf cap and human XP threshold");
            }
            const auto profile = Combatant::decode(module->character_profile(sheet, {}).data);
            check(profile && profile->max_hit_points == sheet.hit_points && !profile->large,
                  "Each native creation has a live single-square combat profile");
            // Creation gives every hero immunity bit 2 (create.c 1394), which
            // the computer's charm and hold ratings read (cbaction.c,
            // rank_target): 0x2000, dark_queen_effect::mind_spell_trait.
            check((profile->traits & 0x2000) != 0,
                  "Every native hero may be charmed or held: " + race.id + "/" + profession.id);
            if (assets)
            {
                CampaignParty party(rules_module(content), creation);
                const auto member = party.add_pc(Character(*creation, draft, {}));
                outfit_original_character(party, member, assets->item_definitions);
                check(!party.member(member).character.inventory().items().empty(),
                      "Every native race/class receives its own actual starting equipment");
                for (const auto &[id, item] : party.member(member).item_sources)
                {
                    check(item.stored.type != 30 && item.stored.type != 12 &&
                              item.stored.type != 74,
                          "Starting equipment follows unlimited ranged ammunition");
                    if (item.stored.type == 5)
                    {
                        check(item.stored.stack_size == 40 && item.stored.raw[57] == 40,
                              "Native thrown darts retain their actual physical starting quantity");
                    }
                }
            }
            ++created;
        }
    }
    check(created == 66, "Exercise all 66 native race/class choices");
}

void native_knight_petitions_apply_every_required_level_loss()
{
    using namespace opengold;
    using namespace original;
    Content content;
    content.game = game;
    const auto module = rules_module(content);
    rules::CharacterSheet knight;
    knight.identity = identity(game);
    knight.name = "Crown knight";
    knight.level = 11;
    knight.hit_points = 144;
    knight.level_classes.assign(11, "knight");
    for (const auto ability : rules::all_abilities)
    {
        knight.base[ability] = knight.scores[ability] = 18;
    }
    knight.original = rules::CharacterSheet::Original{};
    auto &numbers = *knight.original;
    numbers.race = static_cast<int>(Race::krynn_human);
    numbers.profession = static_cast<int>(Profession::knight);
    numbers.class_levels[7] = 11;
    numbers.knight_order = 1;
    numbers.rolled_hit_points = 100;
    numbers.experience = 1000001;
    const auto choices =
        module->hall_training_options(knight, std::array<std::string, 1>{"knight"});
    check(choices.options.size() == 1, "A native Crown knight may petition for Sword");
    module->train_at_hall(knight, choices.options.front().id);
    check(knight.original->knight_order == 2 && knight.original->class_levels[7] == 9 &&
              knight.original->rolled_hit_points == 96 && knight.hit_points == 140 &&
              knight.level == 9,
          "Native petition checks successive XP thresholds and loses both required levels");
}

void native_calendar_history_survives_long_campaign_saves()
{
    using namespace opengold;
    using namespace original;
    Content content;
    content.game = game;
    const std::shared_ptr<const rules::CharacterRules> creation =
        character_rules(default_aging, game);
    auto party = std::make_shared<CampaignParty>(rules_module(content), creation);
    CharacterCreator creator(creation, 45);
    creator.select(rules::CreationField::race, "krynn_human");
    creator.select(rules::CreationField::character_class, "fighter");
    creator.name("Long campaign hero");
    test::walk_original_creation(creator);
    const auto hero = party->add_pc(creator.create_character());
    for (unsigned day = 0; day < 1100; ++day)
    {
        party->advance_time(std::chrono::minutes{1440});
    }
    check(party->member(hero).character.advancements().size() > 1000,
          "The ordinary Krynn calendar produces more than a thousand history entries");
    const auto bytes = encode_campaign(*party, nullptr, "long-native-campaign", game);
    const auto module = rules_module(content);
    auto saved = decode_campaign(bytes, *creation, *module, "long-native-campaign", nullptr);
    party->restore(saved.party);
    check(encode_campaign(*party, nullptr, "long-native-campaign", game) == bytes,
          "A long native calendar history reloads and saves byte for byte");
}

void native_potions_can_be_prepared_before_combat()
{
    using namespace opengold;
    using namespace original;
    Content content;
    content.game = game;
    const auto module = rules_module(content);
    rules::CharacterSheet knight;
    knight.identity = identity(game);
    knight.name = "Prepared knight";
    knight.level = 12;
    knight.hit_points = 100;
    knight.level_classes.assign(12, "knight");
    knight.original = rules::CharacterSheet::Original{};
    knight.original->race = static_cast<int>(Race::krynn_human);
    knight.original->profession = static_cast<int>(Profession::knight);
    knight.original->class_levels[7] = 12;
    knight.original->age = 30;
    for (const auto ability : rules::all_abilities)
    {
        knight.base[ability] = knight.scores[ability] = 18;
    }
    for (const unsigned power : {57u, 59u, 97u})
    {
        std::vector<std::uint8_t> raw(63);
        raw[46] = power == 59 ? 53 : 47;
        raw[60] = 1;
        raw[61] = static_cast<std::uint8_t>(power);
        const auto item = por::original_item_key(por::decode_items(raw)->front());
        check(module->usable_outside_combat(item),
              "Native Speed, Frost Strength and Invisibility potions are usable before combat");
        rules::VitalState state{100, false, {}, {}};
        RandomState random{19};
        module->drink_potion(knight, state, item, random);
        const auto effects = carried_state(state.resources, game).conditions;
        const int code = power == 57 ? 39 : power == 59 ? 113 : 25;
        const auto effect = std::ranges::find(effects, code, &Effect::code);
        check(effect != effects.end(), "A prepared native potion retains its actual effect");
        if (power == 57)
        {
            const auto age = std::ranges::find(effects, age_in_years, &Effect::code);
            check(effect->minutes >= 5 && effect->minutes <= 20 && (effect->strength & 0x40) != 0 &&
                      age != effects.end() && age->strength == 31 &&
                      module->lasting_changes(knight, state).size() == 1,
                  "Speed lasts 5d4 minutes and charges one year before combat");
            module->pass_time(state, knight, 0, 20);
            const auto expired = carried_state(state.resources, game).conditions;
            check(std::ranges::find(expired, 39, &Effect::code) == expired.end() &&
                      std::ranges::find(expired, age_in_years, &Effect::code)->strength == 31,
                  "Speed expiry retains the year already charged");
            state.resources = rested_state(std::array{Effect{42, 10, 12, false}});
            module->drink_potion(knight, state, item, random);
            const auto cured = carried_state(state.resources, game).conditions;
            check(std::ranges::none_of(cured,
                                       [](const Effect &entry)
                                       {
                                           return entry.code == 39 || entry.code == 42 ||
                                                  entry.code == age_in_years;
                                       }),
                  "Speed only cures Slow when slowed, without ageing or Haste");
        }
        else if (power == 59)
        {
            check(effect->minutes >= 50 && effect->minutes <= 80 &&
                      effect->strength == strength_code(21, 0),
                  "Native Frost Strength grants strength 21 for 40 plus 1d4 times ten minutes");
        }
        else
        {
            check(effect->minutes == 0, "Prepared invisibility lasts until its normal removal");
        }
    }
}

void native_fire_shield_can_be_prepared_before_combat()
{
    using namespace opengold;
    using namespace original;
    Content content;
    content.game = game;
    const auto module = rules_module(content);
    rules::CharacterSheet mage;
    mage.identity = identity(game);
    mage.name = "Shielded mage";
    mage.level = 17;
    mage.hit_points = 50;
    mage.level_classes.assign(17, "magic_user");
    for (const auto ability : rules::all_abilities)
    {
        mage.base[ability] = mage.scores[ability] = 18;
    }
    mage.original = rules::CharacterSheet::Original{};
    mage.original->race = static_cast<int>(Race::krynn_human);
    mage.original->profession = static_cast<int>(Profession::magic_user);
    mage.original->class_levels[5] = 17;
    mage.original->robe_order = 1;
    mage.original->known_spells = {185};
    mage.prepared_spells = {"magic_user:fire_shield"};
    for (const bool cold : {false, true})
    {
        rules::VitalState state{50, false, {}, {}}, neighbour{50, false, {}, {}};
        auto other = mage;
        other.name = "Neighbour";
        const std::string action =
            cold ? "cast:magic_user:fire_shield:cold" : "cast:magic_user:fire_shield:hot";
        const auto offered = module->camp_actions(mage, state);
        check(std::ranges::find(offered, action, &rules::CampAction::id) != offered.end(),
              "Native Fire Shield offers both flame choices before combat");
        RandomState random{19};
        module->use_camp_action(mage, state, other, neighbour, action, random);
        const auto conditions = carried_state(state.resources, game).conditions;
        const auto flames = std::ranges::find(conditions, cold ? 254 : 250, &Effect::code);
        const auto retaliation = std::ranges::find(conditions, 343, &Effect::code);
        check(flames != conditions.end() && retaliation != conditions.end() &&
                  flames->minutes > 0 && flames->minutes == retaliation->minutes &&
                  module->memorized_spells(mage, state).empty() && neighbour.resources.empty() &&
                  neighbour.hit_points == 50,
              "Fire Shield spends one spell and protects its caster, retaining both timed effects");
        const auto duration = flames->minutes;
        module->pass_time(state, mage, 0, static_cast<std::uint64_t>(duration));
        const auto expired = carried_state(state.resources, game).conditions;
        check(std::ranges::none_of(expired,
                                   [](const Effect &effect)
                                   {
                                       return effect.code == 250 || effect.code == 254 ||
                                              effect.code == 343;
                                   }),
              "Both native Fire Shield effects expire with the actual campaign clock");
    }
}

void native_drain_rolls_keep_separate_hp_and_xp()
{
    using namespace opengold;
    using namespace original;
    NativeProgression progression;
    progression.profession = static_cast<int>(Profession::fighter);
    progression.race = static_cast<int>(Race::krynn_human);
    progression.rolled_hit_points = 24;
    progression.experience = 20000;
    ClassLevels levels{};
    level_in(levels, ClassSlot::fighter) = 4;
    RandomState expected{1337}, actual{1337};
    const int first = roll_die(expected, 10), second = roll_die(expected, 10);
    const int raw = std::max(first, second);
    const auto loss = roll_native_drain(progression, levels, 30, 18, progression.race, actual);
    check(loss && loss->rolled_hit_points == raw && loss->hit_points == raw + 4 &&
              loss->previous_experience == 20000 && loss->previous_max_hit_points == 30 &&
              loss->experience == 6001 && actual == expected,
          "Native drain retains the actual better-of-two roll, Constitution HP "
          "and midpoint XP");
    progression.peak_class_levels[6] = 1;
    progression.profession = static_cast<int>(Profession::fighter_thief);
    expected = RandomState{1337};
    actual = expected;
    const int split_first = roll_die(expected, 10), split_second = roll_die(expected, 10);
    const int split_raw = std::max(1, std::max(split_first, split_second) / 2);
    const auto split = roll_native_drain(progression, levels, 20, 18, progression.race, actual);
    check(split && split->rolled_hit_points == split_raw && split->hit_points == split_raw + 2 &&
              actual == expected,
          "A native class drained to zero still counts in the HP divisor");
    progression.profession = static_cast<int>(Profession::knight);
    progression.knight_order = 1;
    progression.peak_class_levels = {};
    levels = {};
    level_in(levels, ClassSlot::monk) = 2;
    expected = RandomState{1338};
    actual = expected;
    const int first_pair = roll_die(expected, 10) + roll_die(expected, 10);
    const int second_pair = roll_die(expected, 10) + roll_die(expected, 10);
    const auto knight = roll_native_drain(progression, levels, 30, 18, progression.race, actual);
    check(knight && knight->rolled_hit_points == std::max(first_pair, second_pair) &&
              actual == expected,
          "A second-level native knight loses the better of two two-die rolls");
    Content content;
    content.game = game;
    const auto module = rules_module(content);
    rules::CharacterSheet legacy;
    legacy.identity = identity(game);
    legacy.name = "Legacy native drain";
    legacy.level = 4;
    legacy.hit_points = 30;
    legacy.level_classes.assign(4, "fighter");
    legacy.original = rules::CharacterSheet::Original{};
    legacy.original->race = static_cast<int>(Race::krynn_human);
    legacy.original->profession = static_cast<int>(Profession::fighter);
    legacy.original->class_levels[2] = 4;
    legacy.original->rolled_hit_points = 24;
    for (const auto ability : rules::all_abilities)
    {
        legacy.base[ability] = legacy.scores[ability] = 18;
    }
    rules::AdvancementChoice old_loss;
    old_loss.rules_data = "native_drain 2 8 4 6001 0";
    rules::VitalState old_state{30, false, {}, {}};
    check(module->advance_character(legacy, old_state, old_loss) &&
              legacy.original->class_levels[2] == 3 && legacy.hit_points == 22 &&
              legacy.original->rolled_hit_points == 20,
          "Existing native_drain history entries retain their original replay behavior");
}

void native_combat_drain_updates_campaign_and_restores_live_xp()
{
    using namespace opengold;
    using namespace original;
    Content content;
    content.game = game;
    const std::shared_ptr<const rules::CharacterRules> creation =
        character_rules(default_aging, game);
    auto party = std::make_shared<CampaignParty>(rules_module(content), creation);
    CharacterCreator creator(creation, 73);
    creator.select(rules::CreationField::race, "krynn_human");
    creator.select(rules::CreationField::character_class, "fighter");
    creator.name("Drained native hero");
    test::walk_original_creation(creator);
    const auto hero = party->add_pc(creator.create_character());
    party->award_experience(17017, "test:native-live-xp-before-combat", hero);
    const auto before_sheet = party->member(hero).character.sheet();
    const unsigned before_xp = party->member(hero).experience;
    const int before_level = before_sheet.original->class_levels[2];
    check(before_level > 2, "The native drain fixture begins above second level");
    Combatant drainer;
    drainer.hit_points = drainer.max_hit_points = 100;
    drainer.thac0 = -40;
    drainer.attacks[0] = {2, 1, 1, 0};
    drainer.movement = 0;
    drainer.effects = {{86, 0, 0, false}};
    rules::Snapshot after;
    {
        CombatDemo combat(rules_module(content));
        combat.campaign_party(party);
        CampaignEncounter encounter;
        encounter.field.geometry.width = encounter.field.geometry.height = 8;
        encounter.field.geometry.terrain.assign(64, rules::Terrain::open);
        encounter.positions = {{3, 3}, {4, 3}};
        encounter.enemies.push_back({1000,
                                     "test:native-drainer",
                                     "Native drainer",
                                     rules::Side::opposition,
                                     {4, 3},
                                     drainer.encode(),
                                     std::nullopt});
        combat.encounter(std::move(encounter), 47);
        bool drained = false;
        for (unsigned command = 0; command < 100 && !drained; ++command)
        {
            const auto state = combat.combat().snapshot();
            const auto choices = combat.combat().legal_commands();
            const auto selected = std::ranges::find_if(choices,
                                                       [&](const rules::Command &choice)
                                                       {
                                                           return state.actor == 1000
                                                                      ? choice.verb == "melee" &&
                                                                            choice.target == hero
                                                                      : choice.verb == "end";
                                                       });
            check(selected != choices.end() && combat.submit(*selected),
                  "The native fixture uses ordinary combat commands");
            after = combat.combat().snapshot();
            const auto pc = std::ranges::find(after.combatants, hero, &rules::CombatantView::id);
            const auto profile = Combatant::decode(pc->persistent.resources);
            drained = profile && profile->native_progression &&
                      profile->native_progression->drains.size() == 2;
        }
        check(drained, "A real native two-level touch reaches the campaign adapter");
        const auto pc = std::ranges::find(after.combatants, hero, &rules::CombatantView::id);
        const auto profile = *Combatant::decode(pc->persistent.resources);
        check(party->member(hero).character.sheet().original->class_levels[2] == before_level - 2 &&
                  party->member(hero).experience ==
                      profile.native_progression->drains.back().experience &&
                  party->member(hero).character.sheet().original->rolled_hit_points ==
                      profile.native_progression->rolled_hit_points &&
                  party->member(hero).vitals == pc->persistent,
              "Core keeps exact native levels, raw HP and XP without rewriting "
              "combat's continuation");
        const auto history = party->member(hero).character.advancements().size();
        const auto xp = party->member(hero).experience;
        party->apply_combat(after);
        check(party->member(hero).character.advancements().size() == history &&
                  party->member(hero).experience == xp,
              "Applying the same native combat receipt twice does not drain or "
              "award XP again");
        const auto module = rules_module(content);
        const auto checkpoint = combat.combat().save();
        check(module->restore(checkpoint)->save() == checkpoint,
              "A mid-fight native checkpoint keeps both receipts and its exact "
              "random continuation");
    }
    const auto bytes = encode_campaign(*party, nullptr, "native-combat-drain", game);
    const auto module = rules_module(content);
    auto saved = decode_campaign(bytes, *creation, *module, "native-combat-drain", nullptr);
    party->restore(saved.party);
    check(encode_campaign(*party, nullptr, "native-combat-drain", game) == bytes,
          "A campaign save replays both exact combat losses byte for byte");
    party->temple_service(hero, rules::TempleService::restoration, 0);
    check(party->member(hero).experience ==
              static_cast<unsigned>(
                  dark_queen_experience_for_level(ClassSlot::fighter, before_level - 1).value()),
          "An intermediate native Restoration sets exactly the restored level's "
          "threshold XP");
    party->award_experience(555, "test:native-xp-between-restorations", hero);
    party->temple_service(hero, rules::TempleService::restoration, 0);
    check(party->member(hero).experience == before_xp &&
              party->member(hero).character.sheet().original->class_levels ==
                  before_sheet.original->class_levels &&
              party->member(hero).character.sheet().hit_points == before_sheet.hit_points,
          "The last native Restoration recovers retained live XP and the "
          "original maximum HP");
    const auto restored = encode_campaign(*party, nullptr, "native-combat-drain", game);
    saved = decode_campaign(restored, *creation, *module, "native-combat-drain", nullptr);
    party->restore(saved.party);
    check(encode_campaign(*party, nullptr, "native-combat-drain", game) == restored,
          "Native combat losses and restorations preserve the ordinary save "
          "journal");
}

void native_training_does_not_become_a_later_level_drain()
{
    using namespace opengold;
    using namespace original;
    Content content;
    content.game = game;
    const auto module = rules_module(content);
    rules::CharacterSheet knight;
    knight.identity = identity(game);
    knight.name = "Trained knight";
    knight.level = 11;
    knight.hit_points = 117;
    knight.level_classes.assign(11, "knight");
    for (const auto ability : rules::all_abilities)
    {
        knight.base[ability] = knight.scores[ability] = 18;
    }
    knight.original = rules::CharacterSheet::Original{};
    knight.original->race = static_cast<int>(Race::krynn_human);
    knight.original->profession = static_cast<int>(Profession::knight);
    knight.original->class_levels[7] = 11;
    knight.original->knight_order = 3;
    knight.original->rolled_hit_points = 80;
    knight.original->experience = 4000000;
    auto profile = *Combatant::decode(module->character_profile(knight, {}).data);
    profile.hit_points = 100;
    rules::VitalState state{100, false, profile.encode(), {}};
    auto choice = module->default_advancement(knight);
    choice.training["hall"] = {"knight"};
    RandomState random{7};
    module->roll_advancement(knight, 4000000, choice, random);
    check(module->advance_character(knight, state, choice) && knight.level == 12,
          "Train the native knight after its last fight");
    check(module->lasting_changes(knight, state).empty(),
          "Later field healing must not mistake the old combat record for a new drain");
    const auto after = Combatant::decode(state.resources);
    check(after && after->class_levels == knight.original->class_levels &&
              after->max_hit_points == knight.hit_points && after->hit_points == state.hit_points &&
              knight.hit_points - state.hit_points == 17 && after->spells == profile.spells &&
              after->effects == profile.effects,
          "Native training retains wounds, remaining spells and effects in its live record");
}

void native_camps_respect_disabled_encounter_checks()
{
    using namespace opengold;
    using namespace original;
    using namespace por;
    const std::shared_ptr<const rules::CharacterRules> creation =
        character_rules(default_aging, game);
    Content content;
    content.game = game;
    for (const auto [interval, chance] : {std::pair{0, 255}, std::pair{1, 100}, std::pair{1, 255}})
    {
        auto party = std::make_shared<CampaignParty>(rules_module(content), creation);
        CharacterCreator creator(creation, 45);
        creator.select(rules::CreationField::race, "krynn_human");
        creator.select(rules::CreationField::character_class, "fighter");
        creator.name("Citadel camper");
        test::walk_original_creation(creator);
        party->add_pc(creator.create_character());
        Bytes bytes;
        for (unsigned entry = 0; entry < 5; ++entry)
        {
            bytes.insert(bytes.end(),
                         {1, 1, static_cast<std::uint8_t>(entry == 2 ? 0x15 : 0x14), 0x80});
        }
        bytes.insert(bytes.end(), {0, 9, 0, static_cast<std::uint8_t>(interval), 1, 0x2B, 0, 9, 0,
                                   static_cast<std::uint8_t>(chance), 1, 0x2C, 0, 0});
        const auto program = std::make_shared<const EclProgram>(
            EclProgram::decode(bytes, "Native citadel camp fixture", game));
        auto resources = std::make_shared<PhlanResources>();
        resources->opening_script = 4;
        resources->programs.emplace(4, program);
        resources->script_banks.emplace(4, 1);
        resources->bank = 1;
        resources->map = GeoMap{};
        RolfTourSession world(*resources->map, program, {}, program->entries()[4], {}, resources);
        world.campaign_party(party);
        world.advance(1);
        const auto before = party->state().time_minutes;
        check(world.camp(RestKind::long_rest), "Open the native camp through its actual host");
        world.advance(1);
        check(
            world.snapshot().diagnostic.empty() &&
                (interval == 0 ? party->state().time_minutes > before
                               : party->state().time_minutes == before),
            "A native zero interval permits rest even at chance 255; certain immediate encounters forbid it");
    }
}

void native_scripts_spend_one_flagged_prepared_copy()
{
    using namespace opengold;
    using namespace original;
    using namespace por;
    // ECL59's sealed door reuses C0 for a flag check, then writes a spent
    // Dispel Magic byte at that offset rather than its earlier spell index.
    // The native byte write consumes the entry actually overwritten.
    for (const std::uint8_t spent : std::to_array<std::uint8_t>({0x83, 0xA9}))
    {
        const std::shared_ptr<const rules::CharacterRules> creation =
            character_rules(default_aging, game);
        Content content;
        content.game = game;
        auto party = std::make_shared<CampaignParty>(rules_module(content), creation);
        CharacterCreator creator(creation, 45);
        creator.select(rules::CreationField::race, "krynn_human");
        creator.select(rules::CreationField::character_class, "cleric");
        creator.name("Prison healer");
        test::walk_original_creation(creator);
        const auto id = party->add_pc(creator.create_character());
        check(party->rest(RestKind::long_rest).has_value(), "Rest the native healer");
        party->choose_spells(
            id, {{},
                 std::vector<std::string>{"cleric:cure_light_wounds", "cleric:cure_light_wounds#2",
                                          "cleric:bless"}});
        const auto prepared = party->memorized_spells(id);
        const auto first = std::ranges::find(prepared, "cleric:cure_light_wounds");
        check(first != prepared.end() && std::ranges::count(prepared, *first) == 2,
              "The healer starts with two genuinely prepared cure copies");
        const auto address = static_cast<std::uint16_t>(0x41F + (first - prepared.begin()));
        Bytes bytes;
        for (unsigned entry = 0; entry < 5; ++entry)
        {
            bytes.insert(bytes.end(), {1, 1, 0x14, 0x80});
        }
        bytes.insert(bytes.end(), {10, 0, 0, 9, 0, spent, 1, static_cast<std::uint8_t>(address),
                                   static_cast<std::uint8_t>(address >> 8), 0});
        const auto program = std::make_shared<const EclProgram>(
            EclProgram::decode(bytes, "Native high-bit spell-spend fixture", game));
        auto resources = std::make_shared<PhlanResources>();
        resources->opening_script = 4;
        resources->programs.emplace(4, program);
        resources->script_banks.emplace(4, 1);
        resources->bank = 1;
        resources->map = GeoMap{};
        RolfTourSession world(*resources->map, program, {}, program->entries()[4], {}, resources);
        world.campaign_party(party);
        world.advance(1);
        const auto remaining = party->memorized_spells(id);
        check(world.snapshot().diagnostic.empty() &&
                  world.snapshot().phase == TourPhase::completed &&
                  std::ranges::count(remaining, "cleric:cure_light_wounds") == 1 &&
                  std::ranges::count(remaining, "cleric:bless") == 1,
              "A native script's 80h used bit spends exactly one selected spell: " +
                  world.snapshot().diagnostic);
    }
}

void native_scripted_transfer_records_the_settled_previous_square()
{
    using namespace opengold;
    using namespace original;
    using namespace por;
    const std::shared_ptr<const rules::CharacterRules> creation =
        character_rules(default_aging, game);
    Content content;
    content.game = game;
    auto party = std::make_shared<CampaignParty>(rules_module(content), creation);
    CharacterCreator creator(creation, 45);
    creator.select(rules::CreationField::race, "krynn_human");
    creator.select(rules::CreationField::character_class, "fighter");
    creator.name("Transferred hero");
    test::walk_original_creation(creator);
    (void)party->add_pc(creator.create_character());
    Bytes bytes;
    for (const std::uint8_t entry : std::to_array<std::uint8_t>({0x14, 0x27, 0x36, 0x36, 0x36}))
    {
        bytes.insert(bytes.end(), {1, 1, entry, 0x80});
    }
    // The pre-entry transfers to (3,3) and blocks the physical step. The
    // post-entry records the previous-square cells as the palace does.
    bytes.insert(bytes.end(),
                 {9, 0, 3, 1,    0x34, 0, 9,    0, 3, 1, 0x35, 0, 9, 0,    0xFF, 1, 3, 0,
                  0, 9, 1, 0x17, 0,    1, 0xC9, 0, 9, 1, 0x18, 0, 1, 0xCA, 0,    0, 0});
    const auto program = std::make_shared<const EclProgram>(
        EclProgram::decode(bytes, "Native transferred previous-square fixture", game));
    auto resources = std::make_shared<PhlanResources>();
    resources->opening_script = 4;
    resources->programs.emplace(4, program);
    resources->script_banks.emplace(4, 1);
    resources->bank = 1;
    resources->map = GeoMap{};
    RolfTourSession world(*resources->map, program, {}, program->entries()[4], {}, resources);
    world.campaign_party(party);
    world.advance(1);
    test::TourAccess::place(world, {2, 2, MapDirection::north});
    check(world.explore(ExplorationCommand::forward), "Attempt a native scripted transfer");
    world.advance(1);
    check(world.snapshot().diagnostic.empty() && world.snapshot().phase == TourPhase::completed &&
              world.snapshot().pose.x == 3 && world.snapshot().pose.y == 3 &&
              world.script_variable(EclAddress{0xC9}) == 3 &&
              world.script_variable(EclAddress{0xCA}) == 3,
          "Native post-move scripts see the transferred square before a blocked physical step: " +
              world.snapshot().diagnostic);
}

void native_square_flags_follow_redraw_scripted_and_player_moves()
{
    using namespace opengold;
    using namespace original;
    using namespace por;
    const std::shared_ptr<const rules::CharacterRules> creation =
        character_rules(default_aging, game);
    Content content;
    content.game = game;
    auto party = std::make_shared<CampaignParty>(rules_module(content), creation);
    CharacterCreator creator(creation, 45);
    creator.select(rules::CreationField::race, "krynn_human");
    creator.select(rules::CreationField::character_class, "fighter");
    creator.name("Tower explorer");
    test::walk_original_creation(creator);
    (void)party->add_pc(creator.create_character());
    Bytes bytes;
    for (const std::uint8_t entry : std::to_array<std::uint8_t>({0x14, 0x15, 0x14, 0x14, 0x1D}))
    {
        bytes.insert(bytes.end(), {1, 1, entry, 0x80});
    }
    bytes.insert(bytes.end(),
                 {0, 9, 1,    0x3D, 0,  1, 0xC9, 0, 0,    9, 0,    3, 1, 0x34, 0, 9, 0,
                  3, 1, 0x35, 0,    9,  0, 0,    1, 0x11, 0, 45,   1, 1, 0,    9, 1, 0x3D,
                  0, 1, 0xCB, 0,    45, 1, 2,    0, 9,    1, 0x3D, 0, 1, 0xCC, 0, 0});
    const auto program = std::make_shared<const EclProgram>(
        EclProgram::decode(bytes, "Native floor and shaft flags fixture", game));
    GeoMap map;
    const auto squares = map.width * map.height;
    map.raw.resize(8 + 4 * squares);
    map.raw[0] = static_cast<std::uint8_t>(map.height);
    map.raw[1] = static_cast<std::uint8_t>(map.width);
    map.raw[8 + 3 * squares + 3 * map.width + 3] = 0x0A;
    map.raw[8 + 3 * squares + 2 * map.width + 3] = 0x02;
    map.raw[8 + 3 * squares + map.width + 3] = 0x08;
    auto resources = std::make_shared<PhlanResources>();
    resources->opening_script = 4;
    resources->programs.emplace(4, program);
    resources->script_banks.emplace(4, 1);
    resources->bank = 1;
    resources->map = map;
    RolfTourSession world(map, program, {}, program->entries()[4], {}, resources);
    world.campaign_party(party);
    world.advance(1);
    check(world.snapshot().diagnostic.empty() && world.script_variable(EclAddress{0xCB}) == 3 &&
              world.script_variable(EclAddress{0xCC}) == 1,
          "Native redraw and scripted Forward expose the actual square's two floor bits: " +
              world.snapshot().diagnostic);
    for (const auto expected : {2, 0})
    {
        check(world.explore(ExplorationCommand::forward), "Walk across native floor kinds");
        world.advance(1);
        check(world.snapshot().diagnostic.empty() &&
                  world.script_variable(EclAddress{0xC9}) == expected,
              "Native post-step scripts distinguish shafts and flaming areas from ordinary floors");
    }
}

void native_find_special_queries_the_selected_members_active_effects()
{
    using namespace opengold;
    using namespace original;
    using namespace por;
    const std::shared_ptr<const rules::CharacterRules> creation =
        character_rules(default_aging, game);
    Content content;
    content.game = game;
    auto party = std::make_shared<CampaignParty>(rules_module(content), creation);
    CharacterCreator cleric(creation, 45);
    cleric.select(rules::CreationField::race, "krynn_human");
    cleric.select(rules::CreationField::character_class, "cleric");
    cleric.name("Protected explorer");
    test::walk_original_creation(cleric);
    const auto protected_id = party->add_pc(cleric.create_character());
    check(party->rest(RestKind::long_rest).has_value(), "Prepare the actual native fire ward");
    party->choose_spells(protected_id, {{}, std::vector<std::string>{"cleric:resist_fire"}});
    party->use_camp_action(protected_id, protected_id, "cast:cleric:resist_fire");
    CharacterCreator fighter(creation, 46);
    fighter.select(rules::CreationField::race, "krynn_human");
    fighter.select(rules::CreationField::character_class, "fighter");
    fighter.name("Unprotected explorer");
    test::walk_original_creation(fighter);
    const auto unprotected_id = party->add_pc(fighter.create_character());
    const auto protected_before = party->member(protected_id).vitals;
    const auto unprotected_before = party->member(unprotected_id).vitals;
    Bytes bytes;
    for (unsigned entry = 0; entry < 5; ++entry)
    {
        bytes.insert(bytes.end(), {1, 1, 0x14, 0x80});
    }
    for (const std::uint8_t slot : std::to_array<std::uint8_t>({0, 1}))
    {
        const auto result = static_cast<std::uint8_t>(0xC9 + slot);
        bytes.insert(bytes.end(), {10, 0,      slot, 63, 0, 20, 22, 9, 0,      1,
                                   1,  result, 0,    23, 9, 0,  2,  1, result, 0});
    }
    bytes.push_back(0);
    const auto program = std::make_shared<const EclProgram>(
        EclProgram::decode(bytes, "Native FIND SPECIAL fixture", game));
    auto resources = std::make_shared<PhlanResources>();
    resources->opening_script = 4;
    resources->programs.emplace(4, program);
    resources->script_banks.emplace(4, 1);
    resources->bank = 1;
    resources->map = GeoMap{};
    RolfTourSession world(*resources->map, program, {}, program->entries()[4], {}, resources);
    world.campaign_party(party);
    world.advance(1);
    check(world.snapshot().diagnostic.empty() && world.snapshot().phase == TourPhase::completed &&
              world.script_variable(EclAddress{0xC9}) == 1 &&
              world.script_variable(EclAddress{0xCA}) == 2,
          "Native FIND SPECIAL sees only the selected member's actual fire protection: " +
              world.snapshot().diagnostic);
    check(party->member(protected_id).vitals == protected_before &&
              party->member(unprotected_id).vitals == unprotected_before,
          "Finding an effect spends no spell and changes neither character");
}

void library_wall_overlays_keep_pixels_outside_the_plain_piece()
{
    using namespace opengold;
    ua::ViewArt view;
    ua::WallSet set;
    set.palette[1] = {0, 0, 63};
    set.palette[2] = {63, 0, 0};
    set.glyphs.resize(48);
    for (auto &entry : set.glyphs)
    {
        ua::Glyph glyph;
        glyph.kind = ua::GlyphKind::image;
        glyph.image.width = glyph.image.height = 1;
        glyph.image.pixels = {1};
        entry = std::move(glyph);
    }
    // Variant three's near-front decoration extends left of its base wall.
    set.glyphs[27]->origin_column = 1;
    set.glyphs[27]->origin_row = -2;
    set.glyphs[27]->image.pixels = {2};
    view.sets[0] = std::move(set);
    const auto art = ua::wall_art(view);
    check(art.offsets[2][6].x == -1 && art.appearances[2][6].width == 57,
          "Preserve a native overlay's negative origin and full extent");
    por::GeoMap map;
    map.cells[5 * map.width + 5].walls[0] = 3;
    const auto drawn = por::render_exploration_view(map, art, 5, 5, por::MapDirection::north);
    const auto red = (10 * drawn.image.width + 15) * 4;
    const auto blue = (8 * drawn.image.width + 16) * 4;
    check(drawn.image.rgba[red] == 255 && drawn.image.rgba[red + 2] == 0 &&
              drawn.image.rgba[blue] == 0 && drawn.image.rgba[blue + 2] == 255,
          "Draw the extended decoration and base wall at their native anchors without clipping");
}

void native_world_and_virtual_character_fields_remain_distinct()
{
    using namespace opengold;
    using namespace original;
    using namespace por;
    check(game_address(ecl_party_x, game) == EclAddress{0x34} &&
              game_address(ecl_party_y, game) == EclAddress{0x35} &&
              pool_of_radiance_address(game_address(EclAddress{0x9801}, game), game) ==
                  EclAddress{0x9801} &&
              ecl_memory(game).limit == 0xAEE3 && ecl_engine_routines(game).move_forward == 2 &&
              game_address(EclAddress{0x6DE3}, game) == EclAddress{0x1E},
          "Native world cells, scratch aliases and CALLs use their exact memory zones");
    const std::shared_ptr<const rules::CharacterRules> creation =
        character_rules(default_aging, game);
    Content content;
    content.game = game;
    auto party = std::make_shared<CampaignParty>(rules_module(std::move(content)), creation);
    CharacterCreator creator(creation, 45);
    creator.select(rules::CreationField::race, "krynn_human");
    creator.select(rules::CreationField::character_class, "knight");
    creator.name("Native Knight");
    test::walk_original_creation(creator);
    const auto id = party->add_pc(creator.create_character());
    const auto experience_before = party->member(id).experience;
    const auto hall = party->hall_training(id, 64);
    check(
        hall.denial != TrainingDenial::other_hall && hall.denial != TrainingDenial::gold,
        "A native knight reaches its own hall's progression check without a false class or fee denial");
    Bytes bytes;
    for (unsigned entry = 0; entry < 5; ++entry)
    {
        bytes.insert(bytes.end(), {1, 1, 0x14, 0x80});
    }
    // LOAD CHARACTER 0; copy virtual class, knight level and order into
    // fixture locals; CHECK PARTY leaves its output operands unchanged.
    const Bytes body{10, 0, 0, 9,    1,    0xAF, 4,    1,    0x97, 0, 9,    1, 0x53, 5,    1,  0x98,
                     0,  9, 1, 0xC9, 4,    1,    0x99, 0,    9,    0, 77,   1, 0x9A, 0,    30, 0,
                     0,  0, 0, 1,    0x9A, 0,    1,    0x9B, 0,    1, 0x9C, 0, 1,    0x9D, 0,  0};
    bytes.insert(bytes.end(), body.begin(), body.end());
    bytes.pop_back();
    // Explicit script awards are hundreds and are independent of the
    // ordinary post-combat no-XP flag.
    bytes.insert(bytes.end(), {9, 0, 1, 1, 0x1F, 0, 65, 0, 1, 0, 2, 0});
    auto program = std::make_shared<const EclProgram>(
        EclProgram::decode(bytes, "Native virtual-record fixture", game));
    auto root = std::make_shared<PhlanResources>();
    root->opening_script = 4;
    root->programs.emplace(4, program);
    root->script_banks.emplace(4, 1);
    root->bank = 1;
    root->map = GeoMap{};
    RolfTourSession world(*root->map, program, {}, program->entries()[4], {}, root);
    world.campaign_party(party);
    world.advance(0);
    check(world.snapshot().diagnostic.empty() && world.snapshot().phase == TourPhase::completed,
          "The native fixture executes through the real campaign host");
    check(party->member(id).experience == experience_before + 200,
          "A native scripted award of two grants two hundred actual XP");
    auto eligibility = party->member(id).vitals;
    const auto &sheet = party->member(id).character.sheet();
    check(party->rule_module().receives_experience(sheet, eligibility),
          "An enabled native character receives XP");
    eligibility.hit_points = 0;
    check(!party->rule_module().receives_experience(sheet, eligibility),
          "Native XP excludes unconscious characters before post-combat recovery");
    Content previous;
    previous.game = GoldBoxGame::death_knights_of_krynn;
    const auto previous_rules = rules_module(std::move(previous));
    auto previous_sheet = sheet;
    previous_sheet.identity = previous_rules->identity();
    check(previous_rules->receives_experience(previous_sheet, eligibility),
          "Native eligibility changes preserve the released Death Knights XP policy");
    eligibility.hit_points = 1;
    eligibility.description = "Turned to stone.";
    check(!party->rule_module().receives_experience(sheet, eligibility),
          "A native statue receives no XP");
    check(
        world.script_variable(EclAddress{0x97}) == 1 &&
            world.script_variable(EclAddress{0x98}) == 11 &&
            world.script_variable(EclAddress{0x99}) == 1 &&
            world.script_variable(EclAddress{0x9A}) == 77,
        "Virtual class one and slot one describe a Crown Knight without altering CHECK PARTY outputs");
    check(world.script_variable(EclAddress{0x27}) == 3 &&
              world.script_variable(EclAddress{0x01}) == 0 &&
              party->member(id).vitals.hit_points == party->member(id).character.sheet().hit_points,
          "Native difficulty, nonparty attack bonus and player HP retain independent fields");
    for (const unsigned mode : {0U, 1U, 2U})
    {
        const bool wilderness = mode != 0;
        const bool blocked = mode == 2;
        Bytes context;
        for (unsigned entry = 0; entry < 5; ++entry)
        {
            context.insert(context.end(), {1, 1, 0x14, 0x80});
        }
        context.insert(context.end(), {9, 0,    0,
                                       1, 0x22, 0,
                                       9, 0,    static_cast<std::uint8_t>(wilderness),
                                       1, 0x24, 0,
                                       9, 0,    static_cast<std::uint8_t>(blocked ? 255 : 0),
                                       1, 3,    0,
                                       9, 0,    241,
                                       1, 0x3E, 0,
                                       0});
        const auto context_program = std::make_shared<const EclProgram>(
            EclProgram::decode(context, "Native outdoor movement context", game));
        auto context_resources = std::make_shared<PhlanResources>(*root);
        context_resources->opening_script = 26;
        context_resources->programs[26] = context_program;
        context_resources->script_banks[26] = 1;
        Image north;
        north.width = north.height = 1;
        north.rgba = {255, 0, 0, 255};
        Image south = north;
        south.rgba = {0, 0, 255, 255};
        context_resources->pictures[240] = north;
        context_resources->pictures[241] = south;
        RolfTourSession outdoors(GeoMap{}, context_program, {}, 0, {}, context_resources);
        outdoors.campaign_party(party);
        outdoors.advance(1);
        check(outdoors.snapshot().diagnostic.empty() &&
                  outdoors.snapshot().phase == TourPhase::completed,
              "Native script 26 never tries to load Pool of Radiance's START.EXE wilderness");
        check(
            outdoors.explore(ExplorationCommand::east) == wilderness,
            "Native outdoor courtyards retain first-person controls until wilderness flag 24h is set");
        if (wilderness)
        {
            outdoors.advance(1);
            check(
                outdoors.script_variable(EclAddress{0x25}) == (blocked ? 0 : 1),
                "Native wilderness honors scripted transfers and blocked terrain before ordinary movement");
            check(
                outdoors.observe_view().rgba == south.rgba,
                "Native wilderness displays the script-selected southern map instead of always the north");
        }
    }
}

void packed_monsters_retain_native_fields_and_tails()
{
    Bytes raw(414 + 2 + 5 + 17);
    constexpr std::string_view name = "Native knight";
    std::copy(name.begin(), name.end(), raw.begin() + 0x68);
    raw[0xA6] = 9;
    raw[0xA7] = 4;
    raw[0x89] = 90;
    raw[0x198] = 14;
    raw[0x19A] = 3;
    raw[0x19B] = 5;
    raw[0x19D] = 1;
    raw[0x157 + 15] = 0x80;
    raw[0x167 + 8] = 3;
    raw[414] = 1;
    raw[415] = 1;
    raw[416] = 67;
    raw[417] = 0x34;
    raw[418] = 0x12;
    raw[419] = 7;
    raw[420] = 255;
    static constexpr std::array<std::uint8_t, 17> item{19,  4, 5,    6, 0x34, 0x12, 0x78, 0x56, 253,
                                                       254, 1, 0x31, 1, 4,    7,    8,    9};
    std::copy(item.begin(), item.end(), raw.begin() + 421);
    const auto creature = opengold::dark_queen::decode_creature(packed(raw));
    check(creature && creature->character.game == game && creature->character.name == name &&
              creature->raw == raw,
          "Keep Dark Queen's own character and expanded bytes");
    const auto &character = creature->character;
    check(character.levels.monk == 9 && character.levels.druid == 0 &&
              character.levels.fighter == 4 && character.knight_order == 3 &&
              character.deity == 5 && character.robe_order == 1,
          "Native level slot one is the knight, with independent order, deity and robe");
    check(character.max_hit_points == 90 && character.current.hit_points_raw == 14 &&
              creature->spells.slots[0][8] == 3 && character.spell_book[127] == 1,
          "Preserve wounds, ninth-level slots and all 128 known-spell bits");
    check(creature->effects.size() == 1 && creature->effects[0].code == 67 &&
              creature->effects[0].duration_raw == 0x1234 && creature->effects[0].table_flag == 255,
          "Decode the native five-byte effect without following its original pointers");
    check(creature->items.size() == 1 && creature->items[0].type == 19 &&
              creature->items[0].magic_bonus == -3 && creature->items[0].save_bonus == -2 &&
              creature->items[0].weight == 0x1234 && creature->items[0].value == 0x5678 &&
              creature->items[0].hidden_name_words == 0x31 && creature->items[0].stack_size == 4 &&
              creature->items[0].readied(),
          "Reorder Dark Queen's carried item into the existing inventory representation");
    auto truncated = packed(raw);
    truncated.pop_back();
    check(!opengold::dark_queen::decode_creature(truncated), "Reject a truncated PackBits monster");
    raw[415] = 2;
    check(!opengold::dark_queen::decode_creature(packed(raw)),
          "Reject item counts that leave the expanded record");
    check(!opengold::dark_queen::decode_items(std::span{item}.first(16)),
          "A carried item must have all seventeen bytes");
    raw.resize(414 + 2 + 5);
    raw[415] = 0;
    raw.push_back(0xA6);
    const auto padded = opengold::dark_queen::decode_creature(packed(raw));
    check(padded && padded->items.empty() && padded->effects.size() == 1 &&
              padded->raw.back() == 0xA6,
          "An odd-sized native creature tail retains its alignment byte");
    raw.resize(416);
    raw[414] = 0;
    raw.insert(raw.end(), item.begin(), item.end());
    raw.push_back(0);
    const auto unused = opengold::dark_queen::decode_creature(packed(raw));
    check(unused && unused->effects.empty() && unused->items.empty() && unused->raw == raw,
          "Unused native tail bytes remain provenance rather than becoming active equipment");
}

void glb_scripts_have_no_dax_prefix()
{
    Bytes bytes;
    for (unsigned entry = 0; entry < 5; ++entry)
    {
        bytes.insert(bytes.end(), {1, 1, 0x14, 0x80});
    }
    bytes.insert(bytes.end(), {9, 0, 65, 1, 0x1B, 0x80, 0, 0});
    const auto program = std::make_shared<opengold::por::EclProgram>(
        opengold::por::EclProgram::decode(bytes, "Headerless GLB script", game));
    check(program->raw() == bytes && program->memory().record_header_size == 0 &&
              program->body_start() == 0x8014 && program->instruction(0x8014).opcode == 9,
          "Decode native ECL bytes at 8000 without a synthetic DAX prefix");
    opengold::por::EclMachine first(program), second(program);
    check(first.variable(opengold::por::EclAddress{0x8000}) == 1,
          "The machine reads the first native program byte at its actual address");
    check(first.start(0) && first.run().state == opengold::por::EclState::completed,
          "The native STORE and EXIT execute through their declared entry");
    check(first.variable(opengold::por::EclAddress{0x801B}) == 65 &&
              second.variable(opengold::por::EclAddress{0x801B}) == 0,
          "Self-modification reaches the final GLB byte and stays in its own machine");
    bytes.resize(20);
    bytes.insert(bytes.end(), {55, 129, 0xDB, 1, 1, 0xBF, 0, 0});
    const auto find_npc = std::make_shared<const opengold::por::EclProgram>(
        opengold::por::EclProgram::decode(bytes, "Native companion lookup fixture", game));
    opengold::por::EclMachine lookup(find_npc);
    lookup.enable_host(55);
    lookup.bind_string(opengold::por::EclAddress{0x1DB}, "CAPTAIN DAENOR");
    lookup.bind_variable(opengold::por::EclAddress{0xBF}, 0);
    check(lookup.start(0), "Start native companion lookup");
    const auto find_request = lookup.run();
    check(find_request.request &&
              find_request.request->arguments.at(0).text() == "CAPTAIN DAENOR" &&
              find_request.request->arguments.at(1).address() == opengold::por::EclAddress{0xBF},
          "Native FIND NPC passes a name and an output address to the campaign");
    opengold::por::EclHostReply found;
    found.writes.push_back({opengold::por::EclAddress{0xBF}, 6});
    check(lookup.resume_host(find_request.request->id, found) &&
              lookup.run().state == opengold::por::EclState::completed &&
              lookup.variable(opengold::por::EclAddress{0xBF}) == 6,
          "Native scripts receive the companion's actual party slot");
    for (const auto cell : {0x2B, 0x2C, 0x97, 0xC8, 0xC9})
    {
        lookup.bind_variable(opengold::por::EclAddress{static_cast<std::uint16_t>(cell)}, 45);
    }
    lookup.switch_program(find_npc);
    check(
        lookup.variable(opengold::por::EclAddress{0x2B}) == 0 &&
            lookup.variable(opengold::por::EclAddress{0x2C}) == 0 &&
            lookup.variable(opengold::por::EclAddress{0x97}) == 0 &&
            lookup.variable(opengold::por::EclAddress{0xC8}) == 0 &&
            lookup.variable(opengold::por::EclAddress{0xC9}) == 45,
        "Native script changes clear map locals and rest counters while retaining campaign state");
    bytes.resize(20);
    bytes.insert(bytes.end(), {9, 2, 255, 255, 1,    0x11, 0, 9, 1,    0x11, 0, 1,    0xC9, 0,
                               9, 0, 5,   1,   0x11, 0,    9, 1, 0x11, 0,    1, 0xCA, 0,    0});
    const auto headings = std::make_shared<const opengold::por::EclProgram>(
        opengold::por::EclProgram::decode(bytes, "Native wrapped headings fixture", game));
    opengold::por::EclMachine turning(headings);
    for (const auto cell : {0x11, 0xC9, 0xCA})
    {
        turning.bind_variable(opengold::por::EclAddress{static_cast<std::uint16_t>(cell)}, 0);
    }
    check(turning.start(0) && turning.run().state == opengold::por::EclState::completed &&
              turning.variable(opengold::por::EclAddress{0xC9}) == 3 &&
              turning.variable(opengold::por::EclAddress{0xCA}) == 1,
          "Native heading writes wrap before the following script instruction reads them");
    bytes.resize(20);
    bytes.insert(bytes.end(), {36, 0});
    const auto combat = std::make_shared<const opengold::por::EclProgram>(
        opengold::por::EclProgram::decode(bytes, "Native combat result fixture", game));
    for (const bool won : {false, true})
    {
        opengold::por::EclMachine machine(combat);
        machine.enable_host(36);
        check(machine.start(0), "Start the native combat result fixture");
        const auto pending = machine.run();
        check(pending.request.has_value(), "Native COMBAT asks the campaign for its outcome");
        opengold::por::EclHostReply reply;
        reply.conditions = opengold::por::EclConditions{won, !won, false, false, false, false};
        check(machine.resume_host(pending.request->id, reply) &&
                  machine.run().state == opengold::por::EclState::completed,
              "Native COMBAT accepts both victory and flight flags before resuming its script");
    }
}

void rectangular_maps_keep_every_square()
{
    constexpr std::size_t squares = 20 * 39;
    Bytes raw(8 + squares * 4);
    raw[0] = 20;
    raw[1] = 39;
    raw[2] = 1;
    raw[3] = 2;
    raw[4] = 255;
    raw[8 + squares - 1] = 0x12;
    raw[8 + 2 * squares - 1] = 0x34;
    raw[8 + 3 * squares - 1] = 0xC5;
    raw.back() = 0xE4;
    const auto area = opengold::dark_queen::decode_area(raw);
    check(area && area->rows == 20 && area->columns == 39 && area->cells.size() == squares &&
              area->raw == raw,
          "Retain a whole native rectangular map beyond the DAX grid");
    check(area->cells.back().walls == std::array<std::uint8_t, 4>{1, 2, 3, 4} &&
              area->cells.back().doors == std::array<std::uint8_t, 4>{0, 1, 0, 1} &&
              area->cells.back().event_raw == 0xC5,
          "Keep four directional walls and passage bits without treating backdrop bits as locks");
    opengold::por::GeoMap map;
    map.width = area->columns;
    map.height = area->rows;
    map.cells = area->cells;
    check(map.valid_geometry() && map.at(38, 19).event_raw == 0xC5 && map.wrapped_x(-1) == 38 &&
              map.wrapped_x(39) == 0 && map.wrapped_y(-1) == 19 && map.wrapped_y(20) == 0,
          "Native rectangular coordinates wrap across their own extents");
    auto open_map = map;
    open_map.cells.back().walls = {};
    open_map.wrap_view = true;
    const auto wrapped = opengold::por::render_exploration_view(open_map, {}, 0, 19,
                                                                opengold::por::MapDirection::west);
    check(wrapped.visible.test(19 * 39 + 38),
          "A native corner view reveals the square across its wrapped edge");
    const auto view = opengold::por::render_exploration_view(open_map, {}, 38, 19,
                                                             opengold::por::MapDirection::north);
    check(view.visible.test(779), "The far corner beyond square 255 remains visible");
    check(opengold::por::open_squares_ahead(map, {0, 19, opengold::por::MapDirection::west}) == 1,
          "Encounter visibility reaches a wall through the rectangular wrap");
    raw.pop_back();
    check(!opengold::dark_queen::decode_area(raw), "Reject a truncated area plane");
    raw[0] = 39;
    check(!opengold::dark_queen::decode_area(raw),
          "Reject areas exceeding the native 784-square allocation");
}

Bytes read(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    check(static_cast<bool>(input), "Open installed Dark Queen data");
    return {std::istreambuf_iterator<char>{input}, {}};
}

void released_death_knights_winners_keep_their_save_bytes()
{
    using namespace opengold;
    constexpr auto previous = GoldBoxGame::death_knights_of_krynn;
    const auto folder = test::installed_game_folder(previous);
    const char *source = std::getenv("OPENGOLD_DEATH_KNIGHTS_WINNERS");
    if (folder.empty() || !source)
    {
        return;
    }
    const auto content = load_original_content(folder, previous);
    const std::shared_ptr<const rules::CharacterRules> creation =
        original::character_rules(original::default_aging, previous);
    auto module = original::rules_module(content);
    auto world = load_campaign(folder, previous);
    const auto bytes = read(source);
    const std::string plain(bytes.begin(), bytes.end());
    const auto identity = campaign_asset_identity(folder);
    auto saved = decode_campaign(plain, *creation, *module, identity, &world);
    check(saved.game == previous && saved.town.has_value(),
          "Load the actual released Death Knights winning save");
    auto party = std::make_shared<CampaignParty>(std::move(module), creation);
    party->restore(std::move(saved.party));
    saved.town->attach_restored_party(party);
    check(encode_campaign(*party, &*saved.town, identity, previous) == plain,
          "Larger-map support preserves every byte of the released winning save");
}

void installed_records_decode()
{
    const auto folder = opengold::test::installed_game_folder(game);
    if (folder.empty())
    {
        std::cout << "Dark Queen installed-data checks need its game files.\n";
        return;
    }
    check(opengold::identify_game(folder) == game,
          "The installed Dark Queen identifies independently of the kit");
    const auto check_library =
        [&](const std::filesystem::path &path, auto decode, std::size_t expected)
    {
        const auto library = opengold::ua::decode_library(read(path));
        check(library.has_value(), "Open the native resource container");
        const auto index = opengold::ua::decode_index(*library);
        check(index && index->size() == expected, "Read every native library index entry");
        for (const auto &entry : *index)
        {
            check(decode(library->items[entry.item]),
                  "Decode installed record " + std::to_string(entry.number));
        }
    };
    check_library(
        folder / "DISK2/MONCHA.GLB",
        [](auto bytes)
        {
            return opengold::dark_queen::decode_creature(bytes).has_value();
        },
        98);
    check_library(
        folder / "DISK3/GEO.GLB",
        [](auto bytes)
        {
            return opengold::dark_queen::decode_area(bytes).has_value();
        },
        20);
    check_library(
        folder / "DISK3/ECL.GLB",
        [](auto bytes)
        {
            (void)opengold::por::EclProgram::decode(bytes, "Installed Dark Queen", game);
            return true;
        },
        47);
    const auto items = opengold::dark_queen::decode_items(read(folder / "DISK1/ITEM.DAT"));
    check(items && items->size() == 254, "Read all 254 native item definitions");
    const auto words = opengold::por::decode_item_words(read(folder / "DQK.EXE"), game);
    check(words && words->size() == 155 && words->at(126) == "Hoopak" &&
              words->at(153) == "Blank" && words->at(154) == "Item",
          "Read the complete native far-pointer item-word table, including Krynn weapons");
    const auto creatures = opengold::por::CreatureCatalog::load(folder, game);
    check(creatures.all().size() == 98 &&
              std::ranges::all_of(creatures.all(),
                                  [](const auto &entry)
                                  {
                                      return entry.second.stored.game == game &&
                                             !entry.second.stored.name.empty();
                                  }),
          "Load every native creature and its counted equipment through the shared catalog");
    const auto content = opengold::load_original_content(folder, game);
    check(content.bestiary.size() == 98 && content.item_types.size() == 128,
          "Select native original-rule content from the actual disk directories");
    const opengold::ua::DesignFiles files(folder, folder);
    unsigned composed = 0;
    for (const auto *file : {"8X8DB.TLB", "8X8DC.TLB"})
    {
        const auto library = files.library(file);
        check(library.has_value(), "Read a native wall library");
        const auto index = opengold::ua::decode_index(*library);
        check(index.has_value(), "Read the native wall-set index");
        for (const auto &entry : *index)
        {
            const auto tiles = opengold::ua::nested_tiles(
                *library, entry.number, std::string(file) + ":" + std::to_string(entry.number));
            opengold::ua::ViewArt view;
            opengold::ua::WallSet set;
            opengold::ua::apply_palette_glyph(set.palette, tiles);
            set.glyphs = tiles.glyphs;
            view.sets[0] = std::move(set);
            const auto art = opengold::ua::wall_art(view);
            check(art.appearances.size() == 15 && art.appearances[4][7].width >= 16 &&
                      art.appearances[4][7].height >= 88 &&
                      art.offsets.size() == art.appearances.size(),
                  "Compose every native wall variant into the existing perspective renderer");
            ++composed;
        }
    }
    check(composed > 0, "Exercise actual library wall sets");
    const auto closing = opengold::load_closing_film(folder, game);
    check(closing.screens.size() == 70 && closing.screens.front().sound == 39U &&
              closing.screens.front().screen.rgba != closing.screens.back().screen.rgba &&
              std::ranges::all_of(closing.screens,
                                  [](const auto &frame)
                                  {
                                      return frame.screen.width == 320 &&
                                             frame.screen.height == 200 && frame.seconds > 0;
                                  }),
          "All three native closing scenes compose into seventy timed screens");
    auto world = opengold::load_campaign(folder, game);
    check(
        world.game() == game && world.snapshot().diagnostic.empty(),
        "Construct the native campaign from all installed resources and its actual opening script");
    check(world.resources()->area_map_marker && world.resources()->area_map_marker->width == 8 &&
              world.resources()->area_map_marker->height == 8,
          "The installed native frame supplies the eight-pixel wilderness party marker");
    check(world.resources()->item_definitions.at(1).stored.type == 30 &&
              world.resources()->item_definitions.at(2).stored.type == 1 &&
              world.resources()->item_definitions.at(80).stored.type == 30 &&
              world.resources()->stores.size() == 9,
          "Native ITEM.DAT occupies IDs one through 254 and supplies nine shops");
    check(opengold::load_item_definitions(folder, game).at(2).stored.type == 1,
          "The application's starting-equipment loader uses the native item table");
    const std::shared_ptr<const opengold::rules::CharacterRules> creation =
        opengold::original::character_rules(opengold::original::default_aging, game);
    for (const unsigned record : {112U, 113U, 114U, 115U, 116U, 117U, 118U, 119U})
    {
        const auto creature = creatures.find({1, static_cast<std::uint8_t>(record)});
        check(creature.has_value(),
              "The installed native companion has a creature record: " + std::to_string(record));
        const auto draft = creation->record_draft(creature->get().stored.raw);
        check(draft.has_value(), "Decode an actual native companion for recruitment");
        const opengold::Character companion(*creation, *draft, {});
        check(companion.sheet().hit_points == creature->get().stored.max_hit_points &&
                  companion.sheet().original.has_value(),
              "The native companion retains its maximum HP and Original profile: " +
                  std::to_string(record));
    }
    auto party = std::make_shared<opengold::CampaignParty>(
        opengold::original::rules_module(content), creation);
    opengold::CharacterCreator creator(creation, 2069);
    creator.select(opengold::rules::CreationField::race, "krynn_human");
    creator.select(opengold::rules::CreationField::character_class, "fighter");
    creator.name("Opening hero");
    opengold::test::walk_original_creation(creator);
    const auto hero = party->add_pc(creator.create_character());
    opengold::outfit_original_character(*party, hero, world.resources()->item_definitions);
    // Ready the actual +2 bow while the party carries no ammunition.
    party->take_item(hero, world.resources()->item_definitions.at(82));
    const auto bow = party->member(hero).character.inventory().items().back().id;
    for (const auto readied : std::vector(party->member(hero).equipped))
    {
        party->unequip(hero, readied);
    }
    (void)party->equip(hero, bow);
    const auto archer = opengold::original::Combatant::decode(party->profile(hero).data);
    check(archer && archer->missile && archer->missile->loaded &&
              archer->missile->needs_no_ammunition && !archer->missile->thrown,
          "The native bow is immediately playable with unlimited ammunition");
    // Every installed creature must construct a real Original Rules combat
    // session with its counted equipment and abilities. This is a fixture
    // audit, not a claim that those encounters have been won.
    const auto audit_module = opengold::original::rules_module(content);
    for (const auto &[definition, numbers] : content.bestiary)
    {
        opengold::rules::Encounter encounter;
        encounter.battlefield = {
            50, 25, std::vector<opengold::rules::Terrain>(1250, opengold::rules::Terrain::open)};
        opengold::rules::Participant pc;
        pc.id = 1;
        pc.name = "Audit hero";
        pc.side = opengold::rules::Side::party;
        pc.cell = {5, 10};
        pc.character_profile = party->profile(hero).data;
        encounter.participants.push_back(std::move(pc));
        opengold::rules::Participant monster;
        monster.id = 2;
        monster.name = definition;
        monster.definition = definition;
        monster.side = opengold::rules::Side::opposition;
        monster.cell = {25, 10};
        encounter.participants.push_back(std::move(monster));
        const auto fight = audit_module->create(std::move(encounter), 71);
        check(fight->snapshot().combatants.size() == 2,
              "Construct an actual native creature's combat: " + definition);
    }
    world.campaign_party(party);
    world.advance(0);
    check(world.snapshot().diagnostic.empty() && world.script_diagnostics().empty(),
          "Execute the actual opening: " + world.snapshot().diagnostic);
    const auto view = world.observe_view();
    check(view.width == 88 && view.height == 88,
          "Render the installed opening through native map and backdrop resources");
    bool reached_play = false;
    for (unsigned screen = 0; screen < 200; ++screen)
    {
        world.advance(1);
        const auto &snapshot = world.snapshot();
        check(snapshot.diagnostic.empty() && world.script_diagnostics().empty(),
              "Continue the installed opening: " + snapshot.diagnostic);
        if (snapshot.phase == opengold::por::TourPhase::combat || world.can_leave())
        {
            reached_play = true;
            break;
        }
        if (snapshot.phase == opengold::por::TourPhase::awaiting_continue)
        {
            if (snapshot.dialogue.find("THE CITY OF PALANTHAS.") != std::string::npos &&
                snapshot.choices.size() > 2)
            {
                reached_play = true;
                break;
            }
            const auto no = std::ranges::find(snapshot.choices, "NO");
            check(world.choose(snapshot.continue_ticket,
                               no == snapshot.choices.end()
                                   ? 0U
                                   : static_cast<unsigned>(no - snapshot.choices.begin())),
                  "Continue the native opening and decline optional rest");
        }
    }
    check(reached_play, "The actual opening reaches player control or its first encounter: " +
                            std::to_string(static_cast<unsigned>(world.snapshot().phase)) + " " +
                            world.snapshot().dialogue);
    // This isolated fixture checks staging, not an earned campaign victory.
    std::optional<opengold::por::EclInstruction> native_monster;
    for (const auto &[id, script] : world.resources()->programs)
    {
        for (const auto &instruction : opengold::por::walk_reachable(*script).instructions)
        {
            if (instruction.opcode != 11 || instruction.operands.size() != 3 ||
                instruction.operands[0].tag != 0 || instruction.operands[2].tag != 0)
            {
                continue;
            }
            const auto creature =
                world.resources()->encounter_creatures.find(instruction.operands[0].value);
            if (creature != world.resources()->encounter_creatures.end() &&
                creature->second.stored.hostile && !creature->second.equipment.empty() &&
                creature->second.stored.base_xp_award > 0)
            {
                native_monster = instruction;
                break;
            }
        }
        if (native_monster)
        {
            break;
        }
    }
    check(native_monster.has_value(), "Find an actual native opposing monster/icon pair");
    std::optional<int> base_monster_to_hit;
    for (unsigned terrain = 0; terrain <= 32; ++terrain)
    {
        Bytes fixture;
        for (unsigned entry = 0; entry < 5; ++entry)
        {
            fixture.insert(fixture.end(), {1, 1, 0x14, 0x80});
        }
        fixture.insert(fixture.end(), {33, 0, 10, 0, 1});
        // Stage outside the older sixteen-column/row allocation too.
        fixture.insert(fixture.end(), {9, 0, 24, 1, 0x34, 0, 9, 0, 20, 1, 0x35, 0});
        fixture.insert(fixture.end(), {9, 0, static_cast<std::uint8_t>(terrain == 32), 1, 0x3C, 0});
        fixture.insert(fixture.end(), {9, 0, static_cast<std::uint8_t>(terrain == 32), 1, 2, 0});
        if (terrain == 32)
        {
            for (const std::uint8_t cell : std::to_array<std::uint8_t>({0x1E, 0x1F, 0x1D}))
            {
                fixture.insert(fixture.end(), {9, 0, 1, 1, cell, 0});
            }
            fixture.insert(fixture.end(),
                           {9, 0, 3, 1, 0x1C, 0, 9, 0, 4, 1, 1, 0, 9, 0, 5, 1, 0x2F, 0});
        }
        if (terrain < 32)
        {
            fixture.insert(fixture.end(), {9, 0, 0, 1, 0x22, 0, 9, 0, 1, 1, 0x33, 0, 9, 0,
                                           static_cast<std::uint8_t>(terrain), 1, 0xA2, 1});
        }
        fixture.insert(fixture.end(),
                       {11, 0, static_cast<std::uint8_t>(native_monster->operands[0].value), 0, 1,
                        0, static_cast<std::uint8_t>(native_monster->operands[2].value)});
        if (terrain == 0)
        {
            // Two actual undead loads with seven copies must grant two
            // turning attempts, independently of the number of enemies.
            fixture.insert(fixture.end(), {9, 0, 2,    1,  0x0F, 0,    11, 0, 0x19, 0,
                                           3, 0, 0x19, 11, 0,    0x24, 0,  4, 0,    0x24});
        }
        fixture.insert(fixture.end(), {9, 0, 100, 1, 0x1B, 0, 36, 0});
        auto resources = std::make_shared<opengold::por::PhlanResources>(*world.resources());
        auto program = std::make_shared<const opengold::por::EclProgram>(
            opengold::por::EclProgram::decode(fixture, "Native dungeon staging fixture", game));
        resources->programs[4] = program;
        opengold::por::RolfTourSession combat(opengold::por::GeoMap{}, program, {}, 0, {},
                                              resources);
        combat.campaign_party(party);
        for (unsigned screen = 0; screen < 20 && !combat.pending_encounter(); ++screen)
        {
            combat.advance(1);
            check(combat.snapshot().diagnostic.empty() && combat.script_diagnostics().empty(),
                  "Stage actual native dungeon art and creatures: " + combat.snapshot().diagnostic);
            if (combat.snapshot().phase == opengold::por::TourPhase::awaiting_continue)
            {
                check(combat.choose(combat.snapshot().continue_ticket, 0),
                      "Dismiss the native monster's close-up before its battle");
            }
        }
        check(combat.pending_encounter().has_value(),
              "Native dungeon staging reaches a real encounter");
        const auto &fight = *combat.pending_encounter();
        check(fight.morale == 100,
              "Native COMBAT receives the script's actual morale, including its gnome allies");
        check(fight.field.geometry.width == 50 && fight.field.geometry.height == 25 &&
                  fight.terrain_art.size() == (terrain == 32 ? 25 : 39) &&
                  fight.outdoors == (terrain < 32) && fight.underwater == (terrain == 32) &&
                  !fight.enemies.empty() && !fight.art.empty() &&
                  fight.art.front().image.width > 0 && fight.art.front().action,
              "The native encounter supplies its complete field, tiles, ready and action art");
        if (terrain == 0)
        {
            const bool first_undead =
                world.resources()
                    ->encounter_creatures.at(native_monster->operands[0].value)
                    .stored.undead_level != 0;
            check(fight.turning_tries == 2 + static_cast<int>(first_undead) &&
                      fight.turning_penalty == 2,
                  "Native encounters retain undead-load attempts and the authored turning penalty");
            base_monster_to_hit = fight.enemies.front().original_strength.to_hit;
        }
        if (terrain == 32)
        {
            check(fight.party_to_hit == 5 && fight.party_move_modifier == 3 &&
                      fight.enemies.front().original_strength.to_hit == *base_monster_to_hit + 4,
                  "Native battle context forwards both hit modifiers and the movement modifier");
            // Isolated transaction fixture, not an earned battle: feed the
            // staged host a complete result to check its reward/recovery flags.
            const auto before = party->state();
            opengold::rules::Snapshot outcome;
            outcome.identity = party->identity();
            outcome.outcome = opengold::rules::Outcome::victory;
            opengold::rules::CombatantView pc;
            pc.id = hero;
            pc.side = opengold::rules::Side::party;
            pc.hit_points = party->member(hero).vitals.hit_points;
            pc.persistent = party->member(hero).vitals;
            outcome.combatants.push_back(pc);
            opengold::rules::CombatantView enemy;
            enemy.id = fight.enemies.front().id;
            enemy.side = opengold::rules::Side::opposition;
            // Death is authoritative even when a lasting effect leaves a
            // positive HP value, as in the earned anemone battle.
            enemy.hit_points = 2;
            enemy.dead = true;
            outcome.combatants.push_back(enemy);
            check(combat.resolve_combat(outcome), "Resolve the isolated native reward fixture");
            combat.advance(1);
            check(combat.snapshot().diagnostic.empty() &&
                      party->member(hero).experience == before.roster.front().experience &&
                      party->member(hero).character.inventory().items().size() ==
                          before.roster.front().character.inventory().items().size(),
                  "Native no-XP and no-item flags suppress only their respective rewards");
            for (const std::uint16_t cell :
                 std::to_array<std::uint16_t>({0x01, 0x1C, 0x1D, 0x1E, 0x1F, 0x2F}))
            {
                check(combat.script_variable(opengold::por::EclAddress{cell}) == 0,
                      "Native post-combat cleanup clears transient battle flags");
            }
            party->restore(before);
            // Restart this independent fixture through its own opening; an
            // active host request cannot be reconfigured as a restored save.
            combat.campaign_party(party);
            for (unsigned screen = 0; screen < 20 && !combat.pending_encounter(); ++screen)
            {
                combat.advance(1);
                if (combat.snapshot().phase == opengold::por::TourPhase::awaiting_continue)
                {
                    check(combat.choose(combat.snapshot().continue_ticket, 0),
                          "Dismiss the recovery fixture's monster close-up");
                }
            }
            check(combat.pending_encounter().has_value(), "Restage the native recovery fixture");
            opengold::rules::HazardAttack knockout{
                0, 0, 0, party->member(hero).vitals.hit_points, {}};
            knockout.automatic = true;
            (void)party->hazard_attack(hero, knockout);
            check(!party->member(hero).vitals.dead && party->member(hero).vitals.hit_points == 0,
                  "The nonlethal recovery fixture begins unconscious and alive");
            outcome.outcome = opengold::rules::Outcome::defeat;
            outcome.combatants.front().hit_points = 0;
            outcome.combatants.front().persistent = party->member(hero).vitals;
            outcome.combatants.back().hit_points = 1;
            check(combat.resolve_combat(outcome), "Resolve a recoverable native defeat");
            combat.advance(1);
            check(combat.snapshot().diagnostic.empty() &&
                      combat.snapshot().phase == opengold::por::TourPhase::completed &&
                      !party->member(hero).vitals.dead &&
                      party->member(hero).vitals.hit_points == 1 &&
                      party->member(hero).experience == before.roster.front().experience,
                  "Native nonlethal battles recover living unconscious members without XP or loot");
            party->restore(before);
            // A party that gets away leaves its lent allies on the field
            // (the Font's gnomes, ECL64 9FE8); they are not members to lose.
            combat.campaign_party(party);
            for (unsigned screen = 0; screen < 20 && !combat.pending_encounter(); ++screen)
            {
                combat.advance(1);
                if (combat.snapshot().phase == opengold::por::TourPhase::awaiting_continue)
                {
                    check(combat.choose(combat.snapshot().continue_ticket, 0),
                          "Dismiss the flight fixture's monster close-up");
                }
            }
            check(combat.pending_encounter().has_value(), "Restage the native flight fixture");
            outcome.outcome = opengold::rules::Outcome::fled;
            outcome.combatants.front().fled = true;
            outcome.combatants.front().hit_points = party->member(hero).vitals.hit_points;
            outcome.combatants.front().persistent = party->member(hero).vitals;
            opengold::rules::CombatantView gnome;
            gnome.id = opengold::first_ally_id;
            gnome.side = opengold::rules::Side::party;
            gnome.hit_points = 10;
            outcome.combatants.push_back(gnome);
            check(combat.resolve_combat(outcome), "Resolve the native flight fixture");
            combat.advance(1);
            check(combat.snapshot().diagnostic.empty() && !party->member(hero).vitals.dead &&
                      party->state().slots.front() == hero,
                  "Getting away keeps every member and leaves the lent ally behind");
            party->restore(before);
        }
    }
    // Synthetic combat-result transactions, not earned campaign outcomes:
    // actual Native members/creatures exercise loss, host selection and saves.
    {
        using namespace opengold;
        const auto before_flight = party->state();
        CharacterCreator survivor_creator(creation, 2070);
        survivor_creator.select(rules::CreationField::race, "krynn_human");
        survivor_creator.select(rules::CreationField::character_class, "fighter");
        survivor_creator.name("Escaped member");
        test::walk_original_creation(survivor_creator);
        const auto survivor = party->add_pc(survivor_creator.create_character());
        const auto companion_record = creatures.find({1, 113});
        check(companion_record.has_value(), "The flight fixture uses actual Grunschka");
        const auto companion_draft = creation->record_draft(companion_record->get().stored.raw);
        check(companion_draft.has_value(), "Decode the permanent native companion");
        const auto companion =
            party->recruit("por:MON1CHA:113", Character(*creation, *companion_draft, {}));
        const auto baseline = party->state();
        check(baseline.slots[0] == hero && baseline.slots[1] == survivor &&
                  baseline.slots[6] == companion && baseline.selected_slot.index == 0,
              "The selected original member differs from the member who escapes");
        for (unsigned scenario = 0; scenario < 3; ++scenario)
        {
            party->restore(baseline);
            if (scenario == 2)
            {
                rules::HazardAttack knockout{0, 0, 0, party->member(hero).vitals.hit_points, {}};
                knockout.automatic = true;
                (void)party->hazard_attack(hero, knockout);
                check(party->member(hero).vitals.hit_points == 0,
                      "The charmed original is down before its result is submitted");
            }
            Bytes fixture;
            for (unsigned entry = 0; entry < 5; ++entry)
            {
                fixture.insert(fixture.end(), {1, 1, 0x14, 0x80});
            }
            fixture.insert(fixture.end(),
                           {33, 0, 10, 0, 1, 11, 0,
                            static_cast<std::uint8_t>(native_monster->operands[0].value), 0, 1, 0,
                            static_cast<std::uint8_t>(native_monster->operands[2].value), 36,
                            // Chosen-member DAMAGE: one automatic point, no save.
                            46, 0, 0x80, 0, 0, 0, 0, 0, 1, 0, 0x80, 0});
            auto resources = std::make_shared<por::PhlanResources>(*world.resources());
            auto program = std::make_shared<const por::EclProgram>(
                por::EclProgram::decode(fixture, "Native flight and chosen damage fixture", game));
            resources->programs[4] = program;
            por::RolfTourSession flight(por::GeoMap{}, program, {}, 0, {}, resources);
            flight.campaign_party(party);
            for (unsigned screen = 0; screen < 20 && !flight.pending_encounter(); ++screen)
            {
                flight.advance(1);
                check(flight.snapshot().diagnostic.empty(),
                      "Stage native flight: " + flight.snapshot().diagnostic);
                if (flight.snapshot().phase == por::TourPhase::awaiting_continue)
                {
                    check(flight.choose(flight.snapshot().continue_ticket, 0),
                          "Dismiss the flight fixture's monster picture");
                }
            }
            check(flight.pending_encounter().has_value(), "Stage the native flight transaction");
            rules::Snapshot outcome;
            outcome.identity = party->identity();
            outcome.outcome = rules::Outcome::fled;
            for (const auto id : {hero, survivor, companion})
            {
                rules::CombatantView member;
                member.id = id;
                member.name = party->member(id).character.sheet().name;
                member.side =
                    id == hero && scenario != 0 ? rules::Side::opposition : rules::Side::party;
                member.hit_points = party->member(id).vitals.hit_points;
                member.persistent = party->member(id).vitals;
                member.fled = id == survivor || (id == companion && scenario == 0);
                outcome.combatants.push_back(std::move(member));
            }
            rules::CombatantView enemy;
            enemy.id = flight.pending_encounter()->enemies.front().id;
            enemy.side = rules::Side::opposition;
            enemy.hit_points = 1;
            outcome.combatants.push_back(enemy);
            // Lent creatures are never PartyMembers, on either combat side.
            rules::CombatantView gnome;
            gnome.id = first_ally_id;
            gnome.side = scenario == 2 ? rules::Side::opposition : rules::Side::party;
            gnome.hit_points = scenario == 2 ? 0 : 10;
            outcome.combatants.push_back(gnome);
            const auto survivor_hp = party->member(survivor).vitals.hit_points;
            const auto survivor_xp = party->member(survivor).experience;
            check(flight.resolve_combat(outcome), "Resolve synthetic native flight");
            check(flight.snapshot().diagnostic.empty() &&
                      flight.snapshot().phase == por::TourPhase::awaiting_continue,
                  "Flight resumes ordinary DAMAGE: " + flight.snapshot().diagnostic);
            check(party->state().slots[0] == 0 && party->state().slots[1] == survivor &&
                      party->state().selected_slot.index == 1 && party->member(hero).vitals.dead &&
                      party->member(hero).vitals.hit_points == 0,
                  "Every non-fled original is lost regardless of its combat side");
            check(party->state().slots[6] == (scenario == 0 ? companion : 0) &&
                      party->member(companion).vitals.dead == (scenario != 0),
                  "A permanent companion stays only when it actually fled");
            check(party->member(survivor).vitals.hit_points == survivor_hp - 1 &&
                      party->member(survivor).experience == survivor_xp,
                  "DAMAGE reaches the selected survivor; allies and charmed members pay no XP");
            check(flight.choose(flight.snapshot().continue_ticket, 0),
                  "Dismiss the ordinary damage message");
            flight.advance(1);
            check(flight.snapshot().diagnostic.empty() &&
                      flight.snapshot().phase == por::TourPhase::completed &&
                      party->member(hero).vitals.dead && party->member(hero).vitals.hit_points == 0,
                  "The continuation never restores a left-behind original");
            const auto after_flight = party->state();
            for (unsigned format = 49; format <= 54; ++format)
            {
                const auto bytes = encode_campaign_in_format(*party, nullptr,
                                                             "native-flight-fixture", format, game);
                auto saved = decode_campaign(bytes, *creation, *audit_module,
                                             "native-flight-fixture", nullptr);
                party->restore(std::move(saved.party));
                check(encode_campaign_in_format(*party, nullptr, "native-flight-fixture", format,
                                                game) == bytes &&
                          party->state().slots == after_flight.slots &&
                          party->state().roster.size() == after_flight.roster.size() &&
                          party->member(hero).vitals.dead &&
                          party->member(survivor).vitals.hit_points == survivor_hp - 1,
                      "Supported Native save history retains escaped slots and lost records: " +
                          std::to_string(format));
            }
        }
        party->restore(before_flight);
    }
    // The vault is a real host service. Only this isolated request is
    // synthetic; money/item mutations and save/load use campaign commands.
    {
        using namespace opengold;
        const auto before_vault = party->state();
        const auto held_count = party->member(hero).character.inventory().items().size();
        Bytes fixture;
        for (unsigned entry = 0; entry < 5; ++entry)
        {
            fixture.insert(fixture.end(), {1, 1, 0x14, 0x80});
        }
        fixture.insert(fixture.end(), {33, 0, 4, 0, 1, 9, 0, 1, 1, 0x31, 0, 36, 0});
        auto resources = std::make_shared<por::PhlanResources>(*world.resources());
        auto program = std::make_shared<const por::EclProgram>(
            por::EclProgram::decode(fixture, "Native vault request fixture", game));
        resources->programs[4] = program;
        por::RolfTourSession vault(por::GeoMap{}, program, {}, 0, {}, resources);
        vault.campaign_party(party);
        vault.advance(1);
        const auto choose = [&](por::RolfTourSession &session, std::string label)
        {
            const auto &snapshot = session.snapshot();
            const auto found = std::ranges::find(snapshot.choices, label);
            check(found != snapshot.choices.end(), "The native vault offers " + label);
            check(session.choose(snapshot.continue_ticket,
                                 static_cast<std::size_t>(found - snapshot.choices.begin())),
                  "Choose the native vault's " + label);
            session.advance(1);
            check(session.snapshot().diagnostic.empty(),
                  "The native vault continues without a fault");
        };
        choose(vault, "Money");
        check(party->state().wealth[4] == 0 && party->state().wealth[5] == 0 &&
                  party->state().wealth[6] == 0,
              "Native vault deposits steel, gems and jewelry from the actual purse");
        choose(vault, "Items");
        check(vault.snapshot().choices.size() > 1,
              "The vault lists the member's actual carried items");
        const auto item_label = vault.snapshot().choices.front();
        choose(vault, item_label);
        check(party->member(hero).character.inventory().items().size() + 1 == held_count,
              "A deposit removes exactly one actual item from its owner");
        choose(vault, "Exit");
        check(vault.can_leave() && vault.script_variable(por::EclAddress{0x31}) == 0,
              "Leaving the vault resumes the script and clears its service flag");
        const auto encoded = encode_campaign(*party, &vault, "native-vault-fixture", game);
        check(encoded.starts_with("OPENGOLD-CAMPAIGN 54\n"),
              "Only the new native campaign writes the vault extension");
        test::rejects(
            [&]
            {
                (void)encode_campaign_in_format(*party, &vault, "native-vault-fixture", 53, game);
            },
            "An older native format cannot silently discard stored property");
        auto saved =
            decode_campaign(encoded, *creation, *audit_module, "native-vault-fixture", &vault);
        party->restore(saved.party);
        saved.town->attach_restored_party(party);
        check(encode_campaign(*party, &*saved.town, "native-vault-fixture", game) == encoded,
              "Saved vault money and full equipment roundtrip byte for byte");
        check(saved.town->explore(por::ExplorationCommand::look), "Reenter the saved native vault");
        saved.town->advance(1);
        choose(*saved.town, "Take");
        choose(*saved.town, "Money");
        check(party->state().wealth == before_vault.wealth,
              "Withdrawing restores exactly the deposited purse after reload");
        choose(*saved.town, "Take");
        check(saved.town->snapshot().choices.size() == 3,
              "The saved vault still holds its one deposited item");
        choose(*saved.town, saved.town->snapshot().choices.at(1));
        choose(*saved.town, party->member(hero).character.sheet().name);
        check(party->member(hero).character.inventory().items().size() == held_count,
              "The saved item is returned to its chosen recipient");
        choose(*saved.town, "Exit");
        const auto empty_legacy =
            encode_campaign_in_format(*party, &*saved.town, "native-vault-fixture", 53, game);
        auto legacy =
            decode_campaign(empty_legacy, *creation, *audit_module, "native-vault-fixture", &vault);
        check(legacy.town.has_value(), "Earlier native checkpoints still load with an empty vault");
        party->restore(before_vault);
        // A later unsupported instruction must roll back both sides of a
        // vault transaction, rather than duplicate the deposited property.
        Bytes failing;
        for (unsigned entry = 0; entry < 5; ++entry)
        {
            failing.insert(failing.end(),
                           {1, 1, static_cast<std::uint8_t>(entry == 1 ? 0x15 : 0x14), 0x80});
        }
        failing.insert(failing.end(), {0, 9, 0, 1, 1, 0x31, 0, 36, 255});
        auto broken_resources = std::make_shared<por::PhlanResources>(*world.resources());
        auto broken_program = std::make_shared<const por::EclProgram>(
            por::EclProgram::decode(failing, "Native vault rollback fixture", game));
        broken_resources->programs[4] = broken_program;
        por::RolfTourSession interrupted(por::GeoMap{}, broken_program, {}, 0, {},
                                         broken_resources);
        interrupted.campaign_party(party);
        interrupted.advance(1);
        check(interrupted.can_leave(), "The rollback fixture begins in idle exploration");
        check(interrupted.explore(por::ExplorationCommand::look),
              "Visit the rollback fixture's vault");
        interrupted.advance(1);
        choose(interrupted, "Money");
        choose(interrupted, "Items");
        choose(interrupted, interrupted.snapshot().choices.front());
        choose(interrupted, "Exit");
        check(party->state().wealth == before_vault.wealth &&
                  party->member(hero).character.inventory().items().size() == held_count &&
                  interrupted.snapshot().dialogue.find("rolled back") != std::string::npos,
              "A later script fault restores the party's money and deposited item");
        choose(interrupted, "Continue");
        check(interrupted.explore(por::ExplorationCommand::look), "Revisit the rolled-back vault");
        interrupted.advance(1);
        check(interrupted.snapshot().dialogue.find("0 steel, 0 gems, 0 jewelry and 0 items") !=
                  std::string::npos,
              "A rolled-back vault keeps no duplicate of the restored property");
        party->restore(before_vault);
    }
    // Native rot permits magic healing but replaces ordinary rest recovery
    // with one HP each ten days, and keeps the kit-compatible monthly decay.
    const auto &rot_sheet = party->member(hero).character.sheet();
    const auto rot_module = opengold::original::rules_module(content);
    auto rot_numbers = *opengold::original::Combatant::decode(party->profile(hero).data);
    const int recovery = opengold::original::canonical_effect(215, game);
    rot_numbers.effects.push_back({recovery, 14400, 255, true});
    rot_numbers.effects.push_back({opengold::original::kit_rotting, 43200, 31, true});
    opengold::rules::VitalState rot_state{1, false, rot_numbers.encode(), {}};
    rot_module->recover_days(rot_state, rot_sheet, 1);
    check(rot_state.hit_points == 1, "Native rot blocks ordinary day-of-rest healing");
    rot_module->pass_time(rot_state, rot_sheet, 0, 14400);
    check(rot_state.hit_points == 2 && !rot_state.dead,
          "The native rot timer restores exactly one HP each ten days");
    opengold::RandomState rot_random{43};
    rot_module->temple_service(rot_state, rot_sheet, opengold::rules::TempleService::cure_disease,
                               rot_random);
    const auto cured_rot = opengold::original::carried_state(rot_state.resources, game);
    check(std::ranges::none_of(cured_rot.conditions,
                               [recovery](const auto &effect)
                               {
                                   return effect.code == recovery ||
                                          effect.code == opengold::original::kit_rotting;
                               }),
          "A native temple removes both rot timers");
    rot_module->recover_days(rot_state, rot_sheet, 1);
    check(rot_state.hit_points == 3, "Ordinary healing resumes after native Cure Disease");
    unsigned saved_native_hazards = 0, failed_native_hazards = 0;
    for (const bool take_full_when_saved : {false, true})
    {
        for (std::uint64_t seed = 1; seed <= 40; ++seed)
        {
            opengold::rules::HazardAttack native_hazard{0, 0, 0, 10, {}};
            native_hazard.automatic = true;
            native_hazard.original_save = 0;
            native_hazard.half_on_save = take_full_when_saved;
            native_hazard.original_half_on_failure = true;
            native_hazard.original_roll_damage_first = true;
            auto hurt_state = party->member(hero).vitals;
            hurt_state.hit_points = 100;
            opengold::RandomState hazard_random{seed};
            const auto result =
                rot_module->hazard_attack(hurt_state, rot_sheet, {}, native_hazard, hazard_random);
            saved_native_hazards += result.saved;
            failed_native_hazards += !result.saved;
            check(result.damage == (result.saved ? (take_full_when_saved ? 10 : 0) : 5),
                  "Native random/all-party DAMAGE keeps the shipped reversed save behavior");
        }
    }
    check(saved_native_hazards > 0 && failed_native_hazards > 0,
          "Exercise both native hazard save outcomes");
    // Road-time green fire must retain both its subminute timer and RNG
    // across ordinary profile saves and differently sized clock updates.
    opengold::original::Combatant burning_profile;
    burning_profile.max_hit_points = burning_profile.hit_points = 1000;
    burning_profile.effects.push_back(
        {opengold::original::canonical_effect(196, game), 1, 255, true});
    std::vector<opengold::rules::Participant> whole_burning;
    for (const unsigned id : {2U, 1U})
    {
        opengold::rules::Participant member;
        member.id = id;
        member.character_profile = burning_profile.encode();
        member.state = opengold::rules::VitalState{500, false, burning_profile.encode(), {}};
        whole_burning.push_back(std::move(member));
    }
    auto sliced_burning = whole_burning;
    opengold::RandomState whole_random{67}, sliced_random{67};
    rot_module->elapse(whole_burning, std::chrono::minutes{31}, whole_random);
    for (unsigned step = 0; step < 310; ++step)
    {
        rot_module->elapse(sliced_burning, std::chrono::seconds{6}, sliced_random);
    }
    check(whole_random == sliced_random && whole_burning[0].state == sliced_burning[0].state &&
              whole_burning[1].state == sliced_burning[1].state &&
              whole_burning[0].state->hit_points < 500 && whole_burning[1].state->hit_points < 500,
          "Native road fire preserves HP, profile bytes and RNG across six-second clock slices");
    const auto road_fire =
        opengold::original::carried_state(whole_burning[0].state->resources, game);
    check(std::ranges::any_of(road_fire.conditions,
                              [](const auto &effect)
                              {
                                  return effect.code ==
                                             opengold::original::canonical_effect(196, game) &&
                                         effect.minutes == 10;
                              }),
          "Road fire rearms at ten minutes after its initial one-minute flare");
    auto infection_numbers = burning_profile;
    infection_numbers.effects = {
        {opengold::original::canonical_effect(185, game), 1440, 255, true}};
    opengold::rules::VitalState infection{10, false, infection_numbers.encode(), {}};
    rot_module->pass_time(infection, rot_sheet, 0, 1440);
    check(infection.dead && infection.hit_points == 0,
          "A native gas-spore infection reaches its authored one-day deadline");
    // Native scripts can force a sprite to distance two even beside a wall.
    Bytes sprite_fixture;
    for (unsigned entry = 0; entry < 5; ++entry)
    {
        sprite_fixture.insert(sprite_fixture.end(), {1, 1, 0x14, 0x80});
    }
    sprite_fixture.insert(sprite_fixture.end(),
                          {33, 0,    10, 0,  1, 9,   0, 24, 1, 0x34, 0, 9, 0, 20,
                           1,  0x35, 0,  12, 0, 192, 0, 16, 0, 255,  0, 0, 0});
    auto sprite_resources = std::make_shared<opengold::por::PhlanResources>(*world.resources());
    auto sprite_program = std::make_shared<const opengold::por::EclProgram>(
        opengold::por::EclProgram::decode(sprite_fixture, "Native forced-distance fixture", game));
    sprite_resources->programs[4] = sprite_program;
    opengold::por::RolfTourSession distant(opengold::por::GeoMap{}, sprite_program, {}, 0, {},
                                           sprite_resources);
    distant.campaign_party(party);
    distant.advance(1);
    check(distant.snapshot().diagnostic.empty() &&
              distant.snapshot().phase == opengold::por::TourPhase::completed &&
              distant.snapshot().sprite_id == 192 && distant.snapshot().sprite_frame == 2,
          "Native SETUP MONSTER 10h displays the actual distant sprite without a corridor clamp");
    // Exercise the installed flag routines, rather than a second copy of
    // their algorithm: setting flag 28h must leave adjacent flag 29h clear.
    {
        using namespace opengold;
        auto bytes = world.resources()->programs.at(43)->raw();
        // Use the unused entry/camp code for this isolated caller; leave
        // both installed flag routines and their mask table untouched.
        constexpr std::uint16_t caller = 0x8014;
        for (unsigned entry = 0; entry < 5; ++entry)
        {
            bytes[entry * 4 + 2] = static_cast<std::uint8_t>(caller);
            bytes[entry * 4 + 3] = static_cast<std::uint8_t>(caller >> 8);
        }
        const Bytes calls{9,    0,    0x28, 1,    0xBF, 0,    2,    1,    0x9F, 0xAD, 9,
                          0,    0x29, 1,    0xBF, 0,    2,    1,    0x11, 0xAE, 9,    1,
                          0xBF, 0,    1,    0xC9, 0,    9,    0,    0x29, 1,    0xBF, 0,
                          2,    1,    0x9F, 0xAD, 9,    0,    0x28, 1,    0xBF, 0,    2,
                          1,    0x11, 0xAE, 9,    1,    0xBF, 0,    1,    0xCA, 0,    0};
        std::copy(calls.begin(), calls.end(), bytes.begin() + (caller - 0x8000));
        const auto program = std::make_shared<const por::EclProgram>(
            por::EclProgram::decode(bytes, "Installed native adjacent-flag fixture", game));
        por::EclMachine flags(program);
        for (std::uint16_t cell = 1; cell <= 0x400; ++cell)
        {
            flags.bind_variable(por::EclAddress{cell}, 0);
        }
        check(flags.start(0) && flags.run().state == por::EclState::completed &&
                  flags.variable(por::EclAddress{0xC9}) == 0 &&
                  flags.variable(por::EclAddress{0xCA}) == 1 &&
                  flags.variable(por::EclAddress{0x16E}) == 3,
              "Installed native quest flags keep distinct bits within one byte");
    }
    // Enter the actual lighthouse, decline it, then take a real wilderness
    // step. Native script changes run entry, pre-move and arrival in order.
    {
        using namespace opengold;
        const auto before_departure = party->state();
        Bytes fixture;
        for (unsigned entry = 0; entry < 5; ++entry)
        {
            fixture.insert(fixture.end(), {1, 1, 0x14, 0x80});
        }
        fixture.insert(fixture.end(), {32, 0, 22, 0});
        auto resources = std::make_shared<por::PhlanResources>(*world.resources());
        auto program = std::make_shared<const por::EclProgram>(
            por::EclProgram::decode(fixture, "Native lighthouse departure fixture", game));
        resources->programs[4] = program;
        por::RolfTourSession departure(por::GeoMap{}, program, {}, 0, {}, resources);
        departure.campaign_party(party);
        for (unsigned screen = 0; screen < 30 && !departure.can_leave(); ++screen)
        {
            departure.advance(1);
            if (departure.snapshot().phase == por::TourPhase::awaiting_continue)
            {
                const auto &snapshot = departure.snapshot();
                const auto no = std::ranges::find(snapshot.choices, "NO");
                check(no != snapshot.choices.end(), "The installed lighthouse offers departure");
                check(departure.choose(snapshot.continue_ticket,
                                       static_cast<unsigned>(no - snapshot.choices.begin())),
                      "Decline the actual lighthouse entrance");
            }
        }
        check(departure.can_leave() && departure.snapshot().script_id == 5 &&
                  departure.script_variable(por::EclAddress{0x9C}) == 0 &&
                  departure.script_variable(por::EclAddress{0x9D}) == 0,
              "Native entry consumes departure and arrival locks before the first player step");
        test::TourAccess::seed_dice(departure, 23);
        check(departure.explore(por::ExplorationCommand::south),
              "Take the first wilderness step after leaving Luminari");
        departure.advance(1);
        check(departure.snapshot().diagnostic.empty() && departure.snapshot().script_id == 5 &&
                  departure.script_variable(por::EclAddress{0x111}) == 4 &&
                  departure.script_variable(por::EclAddress{0x112}) == 2,
              "Luminari's first ordinary step leaves town instead of reentering the lighthouse");
        party->restore(before_departure);
    }
    // Kristophan's real entry loads its smaller GEO before replacing the
    // coordinates from the Tombs. LOAD FILES must not validate that transient
    // old square against the new map before the script's redraw.
    {
        using namespace opengold;
        const auto before_departure = party->state();
        Bytes fixture;
        for (unsigned entry = 0; entry < 5; ++entry)
        {
            fixture.insert(fixture.end(), {1, 1, 0x14, 0x80});
        }
        fixture.insert(fixture.end(),
                       {33, 0, 42,   0, 1, 9, 0, 27, 1,    0x34, 0,  9, 0, 27, 1,  0x35, 0,  9, 0,
                        42, 1, 0x16, 0, 9, 0, 2, 1,  0x5F, 1,    45, 1, 1, 0,  32, 0,    38, 0});
        auto resources = std::make_shared<por::PhlanResources>(*world.resources());
        auto program = std::make_shared<const por::EclProgram>(
            por::EclProgram::decode(fixture, "Native smaller-map departure fixture", game));
        resources->opening_script = 43;
        resources->programs[43] = program;
        por::RolfTourSession departure(por::GeoMap{}, program, {}, 0, {}, resources);
        departure.campaign_party(party);
        for (unsigned tick = 0;
             tick < 100 && !departure.can_leave() && departure.snapshot().diagnostic.empty();
             ++tick)
        {
            departure.advance(1);
        }
        check(departure.snapshot().diagnostic.empty() && departure.can_leave() &&
                  departure.snapshot().area_id == 38 && departure.snapshot().pose.x == 25 &&
                  departure.snapshot().pose.y == 0,
              "The installed Kristophan entry replaces the Tombs square after loading its map: " +
                  departure.snapshot().diagnostic + ", area " +
                  std::to_string(departure.snapshot().area_id) + ", pose " +
                  std::to_string(departure.snapshot().pose.x) + "," +
                  std::to_string(departure.snapshot().pose.y) + ", phase " +
                  std::to_string(static_cast<int>(departure.snapshot().phase)));
        party->restore(before_departure);
    }
    {
        using namespace opengold;
        const auto before_temple = party->state();
        Bytes fixture;
        for (unsigned entry = 0; entry < 5; ++entry)
        {
            fixture.insert(fixture.end(), {1, 1, 0x14, 0x80});
        }
        fixture.insert(fixture.end(), {9, 0, 1, 1, 0x14, 0, 36, 0, 0});
        auto resources = std::make_shared<por::PhlanResources>(*world.resources());
        auto program = std::make_shared<const por::EclProgram>(
            por::EclProgram::decode(fixture, "Native temple service fixture", game));
        resources->programs[4] = program;
        por::RolfTourSession temple(por::GeoMap{}, program, {}, 0, {}, resources);
        temple.campaign_party(party);
        temple.advance(1);
        check(std::ranges::find(temple.snapshot().choices, "Restoration (0 steel)") !=
                  temple.snapshot().choices.end(),
              "Native temples offer their original free Restoration service");
        party->restore(before_temple);
    }
    // Native PROGRAM 3 must change ordinary replayable history, preserve the
    // wound deficit, and survive the same save path as an actual campaign.
    {
        using namespace opengold;
        const auto before_drain = party->state();
        auto wounded = before_drain;
        wounded.roster.front().vitals.dead = false;
        wounded.roster.front().vitals.description.clear();
        wounded.roster.front().vitals.resources.clear();
        wounded.roster.front().vitals.hit_points =
            wounded.roster.front().character.sheet().hit_points - 7;
        party->restore(wounded);
        party->award_experience(12345, "test:native-drain-live-xp", hero);
        const unsigned before_experience = party->member(hero).experience;
        const auto before_sheet = party->member(hero).character.sheet();
        const int old_level = before_sheet.original->class_levels[2];
        Bytes fixture;
        for (unsigned entry = 0; entry < 5; ++entry)
        {
            fixture.insert(fixture.end(), {1, 1, 0x14, 0x80});
        }
        fixture.insert(fixture.end(), {33, 0, 4, 0, 1, 56, 0, 3, 0});
        auto resources = std::make_shared<por::PhlanResources>(*world.resources());
        auto program = std::make_shared<const por::EclProgram>(
            por::EclProgram::decode(fixture, "Native scripted drain fixture", game));
        resources->programs[4] = program;
        por::RolfTourSession drain(por::GeoMap{}, program, {}, 0, {}, resources);
        drain.campaign_party(party);
        drain.advance(1);
        const auto &after = party->member(hero);
        const auto &sheet = after.character.sheet();
        const auto lower = old_level <= 2 ? 0
                                          : original::dark_queen_experience_for_level(
                                                original::ClassSlot::fighter, old_level - 1)
                                                .value();
        const auto upper =
            old_level <= 1
                ? 0
                : original::dark_queen_experience_for_level(original::ClassSlot::fighter, old_level)
                      .value();
        check(drain.snapshot().diagnostic.empty() &&
                  drain.snapshot().phase == por::TourPhase::completed &&
                  sheet.original->class_levels[2] == old_level - 1 &&
                  sheet.original->peak_class_levels[2] == old_level &&
                  sheet.hit_points < before_sheet.hit_points &&
                  sheet.hit_points - after.vitals.hit_points == 7 &&
                  after.experience == static_cast<unsigned>(lower + (upper - lower) / 2),
              "Native PROGRAM 3 drains a level and midpoint XP while preserving wounds: " +
                  drain.snapshot().diagnostic + "; level " + std::to_string(old_level) + " -> " +
                  std::to_string(sheet.original->class_levels[2]) + "; HP " +
                  std::to_string(before_sheet.hit_points) + " -> " +
                  std::to_string(after.vitals.hit_points) + "/" + std::to_string(sheet.hit_points) +
                  "; XP " + std::to_string(after.experience) + " expected " +
                  std::to_string(lower + (upper - lower) / 2));
        const auto encoded = encode_campaign(*party, &drain, "native-drain-fixture", game);
        auto saved =
            decode_campaign(encoded, *creation, *rot_module, "native-drain-fixture", &drain);
        party->restore(saved.party);
        saved.town->attach_restored_party(party);
        check(encode_campaign(*party, &*saved.town, "native-drain-fixture", game) == encoded,
              "The native drain's rolled HP, levels, wounds and XP replay byte for byte");
        party->temple_service(hero, rules::TempleService::restoration, 0);
        const auto &restored = party->member(hero);
        check(restored.experience == before_experience &&
                  restored.character.sheet().original->class_levels ==
                      before_sheet.original->class_levels &&
                  restored.character.sheet().hit_points == before_sheet.hit_points &&
                  restored.character.sheet().hit_points - restored.vitals.hit_points == 7,
              "The final native Restoration returns actual pre-drain XP and "
              "preserves wounds");
        const auto restored_bytes =
            encode_campaign(*party, &*saved.town, "native-drain-fixture", game);
        auto restored_save = decode_campaign(restored_bytes, *creation, *rot_module,
                                             "native-drain-fixture", &*saved.town);
        party->restore(restored_save.party);
        restored_save.town->attach_restored_party(party);
        check(encode_campaign(*party, &*restored_save.town, "native-drain-fixture", game) ==
                  restored_bytes,
              "Exact native Restoration and retained XP replay byte for byte");
        auto tied = before_sheet;
        tied.original->class_levels = {0, 0, 8, 0, 0, 8, 8, 0};
        tied.original->robe_order = 1;
        tied.original->profession =
            static_cast<int>(original::Profession::fighter_magic_user_thief);
        rules::VitalState tied_state{tied.hit_points, false, {}, {}};
        RandomState tied_random{601};
        const auto tied_loss = rot_module->scripted_level_drain(tied, tied_random);
        check(tied_loss && rot_module->advance_character(tied, tied_state, *tied_loss) &&
                  tied.original->class_levels[6] == 7 && tied.original->class_levels[2] == 8 &&
                  tied.original->class_levels[5] == 8,
              "Equal native levels drain the class with the smallest XP threshold");
        auto last = before_sheet;
        last.original->class_levels = {0, 0, 1, 0, 0, 0, 0, 0};
        rules::VitalState last_state{last.hit_points, false, {}, {}};
        RandomState last_random{602};
        const auto last_loss = rot_module->scripted_level_drain(last, last_random);
        check(last_loss && rot_module->advance_character(last, last_state, *last_loss) &&
                  last_state.dead && last_state.hit_points == 0 &&
                  last.original->class_levels[2] == 0,
              "Losing the last native class level causes death");
        party->restore(before_drain);
    }
    // Command routing fixture only. A genuine victory must reach this command
    // through the installed campaign without replacing its script.
    for (const bool wilderness : {false, true})
    {
        Bytes camp_fixture;
        for (unsigned entry = 0; entry < 5; ++entry)
        {
            camp_fixture.insert(camp_fixture.end(), {1, 1, 0x14, 0x80});
        }
        camp_fixture.insert(camp_fixture.end(), {9, 0, static_cast<std::uint8_t>(wilderness), 1,
                                                 0x24, 0, 56, 0, 9, 9, 0, 77, 1, 0xC9, 0, 0});
        auto camp_resources = std::make_shared<opengold::por::PhlanResources>(*world.resources());
        auto camp_program = std::make_shared<const opengold::por::EclProgram>(
            opengold::por::EclProgram::decode(camp_fixture, "Native scripted camp fixture", game));
        camp_resources->programs[4] = camp_program;
        opengold::por::RolfTourSession camp(opengold::por::GeoMap{}, camp_program, {}, 0, {},
                                            camp_resources);
        camp.campaign_party(party);
        const auto before_rest = party->state().time_minutes;
        camp.advance(1);
        check(camp.snapshot().diagnostic.empty() &&
                  camp.snapshot().phase == opengold::por::TourPhase::completed &&
                  camp.script_variable(opengold::por::EclAddress{0xC9}) == (wilderness ? 0 : 77) &&
                  (wilderness ? camp.snapshot().camp_offered
                              : party->state().time_minutes > before_rest),
              "Native scripted camps resume the story indoors and end the wilderness script");
    }
    Bytes ending_fixture;
    for (unsigned entry = 0; entry < 5; ++entry)
    {
        ending_fixture.insert(ending_fixture.end(), {1, 1, 0x14, 0x80});
    }
    ending_fixture.insert(ending_fixture.end(), {56, 0, 1, 0});
    auto ending_resources = std::make_shared<opengold::por::PhlanResources>(*world.resources());
    auto ending_program = std::make_shared<const opengold::por::EclProgram>(
        opengold::por::EclProgram::decode(ending_fixture, "Native ending command fixture", game));
    ending_resources->programs[4] = ending_program;
    opengold::por::RolfTourSession ending(opengold::por::GeoMap{}, ending_program, {}, 0, {},
                                          ending_resources);
    const auto before_ending = party->member(hero).vitals;
    ending.campaign_party(party);
    ending.advance(1);
    check(ending.snapshot().diagnostic.empty() && ending.snapshot().game_won &&
              ending.snapshot().phase == opengold::por::TourPhase::awaiting_continue &&
              party->member(hero).vitals == before_ending,
          "Native PROGRAM 1 registers victory without altering the winning character");
}

void actual_death_knights_winners_keep_their_earned_state()
{
    using namespace opengold;
    using namespace opengold::original;
    const auto *source = std::getenv("OPENGOLD_DEATH_KNIGHTS_WINNERS");
    if (!source || !*source)
    {
        std::cout << "Actual Death Knights import needs OPENGOLD_DEATH_KNIGHTS_WINNERS.\n";
        return;
    }
    constexpr auto previous = GoldBoxGame::death_knights_of_krynn;
    const auto source_folder = test::installed_game_folder(previous);
    const auto folder = test::installed_game_folder(game);
    if (source_folder.empty() || folder.empty())
    {
        std::cout << "Actual Death Knights import needs both games' files.\n";
        return;
    }
    const auto source_creation = character_rules(default_aging, previous);
    const auto source_rules = rules_module(load_original_content(source_folder, previous));
    const auto source_world = load_campaign(source_folder, previous);
    const auto saved = decode_campaign(read_campaign_file(source), *source_creation, *source_rules,
                                       campaign_asset_identity(source_folder), &source_world);
    check(saved.game == previous, "Read the actual earned Death Knights winning campaign");
    const std::shared_ptr<const rules::CharacterRules> creation =
        character_rules(default_aging, game);
    const auto content = load_original_content(folder, game);
    auto party = std::make_shared<CampaignParty>(rules_module(content), creation);
    const auto convert = death_knights_items_for_dark_queen(folder);
    std::vector<const PartyMember *> winners;
    for (const auto slot : saved.party.slots)
    {
        const auto member = std::ranges::find(saved.party.roster, slot, &PartyMember::id);
        if (slot && member != saved.party.roster.end())
        {
            winners.push_back(&*member);
        }
    }
    check(winners.size() == 6, "Import the six active winners without the departed Durfey");
    unsigned wounded = 0;
    for (std::size_t index = 0; index < winners.size(); ++index)
    {
        const auto &from = *winners[index];
        const auto id = party->bring_across(
            from, convert,
            champions_money_for_death_knights(saved.party.wealth, index, winners.size()),
            CampaignParty::ConvertedItems::readied_as_they_were);
        const auto &to = party->member(id);
        const auto &before = from.character.sheet();
        const auto &after = to.character.sheet();
        const auto profile = Combatant::decode(to.vitals.resources);
        check(profile.has_value(), "Imported remaining spells use the destination combat profile");
        const auto source_profile = Combatant::decode(from.vitals.resources);
        std::vector<int> expected_spells;
        if (source_profile)
        {
            for (const int number : source_profile->spells)
            {
                if (const auto mapped = dark_queen_import_spell(number))
                {
                    expected_spells.push_back(mapped);
                }
            }
            check(profile->spells == expected_spells,
                  "Actual remaining spells map without replenishing copies already cast");
        }
        check(
            to.experience == from.experience && after.scores == before.scores &&
                after.original->class_levels == before.original->class_levels &&
                after.original->age == before.original->age &&
                after.hit_points == before.hit_points &&
                after.original->rolled_hit_points == before.original->rolled_hit_points &&
                to.vitals.hit_points == from.vitals.hit_points &&
                to.vitals.dead == from.vitals.dead && after.original->platinum == 0,
            "Native import preserves earned scores, levels, XP, age and wounded HP without new money");
        wounded += to.vitals.hit_points < after.hit_points;
        std::vector<por::ItemRecord> expected;
        for (const auto &item : from.character.inventory().items())
        {
            if (const auto converted = convert(from.item_sources.at(item.id)))
            {
                expected.push_back(converted->stored);
            }
        }
        const auto &items = to.character.inventory().items();
        check(items.size() == expected.size(), "Native import retains exactly its surviving items");
        for (std::size_t item = 0; item < items.size(); ++item)
        {
            check(to.item_sources.at(items[item].id).stored.raw == expected[item].raw,
                  "Converted native item fields and quantities survive byte for byte");
        }
    }
    check(wounded > 0 && party->state().wealth == saved.party.wealth,
          "The actual wounded party receives its existing purse exactly once");
    const auto assets = campaign_asset_identity(folder);
    const auto bytes = encode_campaign(*party, nullptr, assets, game);
    auto reloaded = decode_campaign(bytes, *creation, *rules_module(content), assets, nullptr);
    auto restored = std::make_shared<CampaignParty>(rules_module(content), creation);
    restored->restore(std::move(reloaded.party));
    check(encode_campaign(*restored, nullptr, assets, game) == bytes,
          "The actual imported winners roundtrip byte for byte as an ordinary native party save");
    auto opening = load_campaign(folder, game);
    opening.campaign_party(party);
    bool visited_palace = false, left_palanthas = false, reached_caergoth = false;
    for (unsigned screen = 0; screen < 300; ++screen)
    {
        opening.advance(1);
        const auto &snapshot = opening.snapshot();
        check(snapshot.diagnostic.empty() && opening.script_diagnostics().empty(),
              "The actual winners play the authored opening: " + snapshot.diagnostic);
        if (left_palanthas && (opening.can_leave() || snapshot.phase == por::TourPhase::combat))
        {
            reached_caergoth = true;
            break;
        }
        if (snapshot.phase == por::TourPhase::awaiting_continue)
        {
            std::size_t choice = 0;
            if (snapshot.dialogue.find("THE CITY OF PALANTHAS.") != std::string::npos &&
                snapshot.choices.size() > 2)
            {
                const auto *destination = visited_palace ? "LEAVE" : "PALACE";
                const auto found = std::ranges::find(snapshot.choices, destination);
                std::string offered;
                for (const auto &label : snapshot.choices)
                {
                    offered += " [" + label + "]";
                }
                check(found != snapshot.choices.end(),
                      std::string{"Palanthas offers the authored destination "} + destination +
                          offered);
                choice = static_cast<std::size_t>(found - snapshot.choices.begin());
                left_palanthas = visited_palace;
                visited_palace = true;
            }
            check(opening.choose(snapshot.continue_ticket, choice),
                  "Meet Laurana and leave Palanthas through its authored menu");
        }
    }
    check(reached_caergoth, "The actual imported party reaches Caergoth from Palanthas");
    if (opening.can_leave())
    {
        const auto exploring = encode_campaign(*party, &opening, assets, game);
        auto decoded =
            decode_campaign(exploring, *creation, *rules_module(content), assets, &opening);
        auto reloaded_party = std::make_shared<CampaignParty>(rules_module(content), creation);
        reloaded_party->restore(std::move(decoded.party));
        check(decoded.town.has_value(), "An ordinary Caergoth save restores native exploration");
        decoded.town->attach_restored_party(reloaded_party);
        check(encode_campaign(*reloaded_party, &*decoded.town, assets, game) == exploring,
              "The real Caergoth exploration save roundtrips byte for byte");
        const auto view = decoded.town->observe_view();
        check(view.width == 88 && view.height == 88,
              "Native first-person resources remain available after reloading exploration");
        std::cout << "Actual imported winners reach Caergoth script "
                  << opening.snapshot().script_id << ", GEO " << opening.snapshot().area_id
                  << " at " << opening.snapshot().pose.x << ',' << opening.snapshot().pose.y
                  << ".\n";
    }
}

} // namespace

int main()
{
    try
    {
        check(static_cast<unsigned>(game) == 9 &&
                  static_cast<unsigned>(opengold::GoldBoxGame::death_knights_of_krynn) == 8,
              "Append Dark Queen without renumbering released game identities");
        native_progression_and_spells_use_dark_queen_rules();
        native_constitution_raises_the_death_save();
        every_native_creation_choice_produces_a_live_profile();
        native_knight_petitions_apply_every_required_level_loss();
        native_training_does_not_become_a_later_level_drain();
        native_drain_rolls_keep_separate_hp_and_xp();
        native_combat_drain_updates_campaign_and_restores_live_xp();
        native_fire_shield_can_be_prepared_before_combat();
        native_potions_can_be_prepared_before_combat();
        native_calendar_history_survives_long_campaign_saves();
        native_camps_respect_disabled_encounter_checks();
        native_scripts_spend_one_flagged_prepared_copy();
        native_scripted_transfer_records_the_settled_previous_square();
        native_square_flags_follow_redraw_scripted_and_player_moves();
        native_find_special_queries_the_selected_members_active_effects();
        library_wall_overlays_keep_pixels_outside_the_plain_piece();
        native_world_and_virtual_character_fields_remain_distinct();
        packed_monsters_retain_native_fields_and_tails();
        glb_scripts_have_no_dax_prefix();
        rectangular_maps_keep_every_square();
        released_death_knights_winners_keep_their_save_bytes();
        installed_records_decode();
        actual_death_knights_winners_keep_their_earned_state();
        std::cout << "Dark Queen checks passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
