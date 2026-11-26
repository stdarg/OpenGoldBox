#include "character_class.h"
#include "module.h"
#include "training.h"
#include "spell_table.h"
#include "armor.h"
#include "magic_items.h"
#include "worn_items.h"
#include "equipment_rules.h"
#include "character_definition.h"
#include "effect_labels.h"
#include "opengold/item_key.h"
#include "opengold/path_text.h"
#include "opengold/scroll_key.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace opengold::srd5
{
using namespace rules;
using detail::basic_poison;
using detail::magic_lock_picks;
using detail::poisoners_kit;
using detail::stone_of_good_luck;
using detail::thieves_tools;
using detail::trained;

int ability_modifier(int score) noexcept
{
    return static_cast<int>(std::floor((static_cast<double>(score) - 10) / 2.0));
}

std::vector<HitDice> hit_dice(const CharacterSheet &sheet)
{
    std::vector<HitDice> result;
    for (const auto &id : detail::level_class_ids(sheet))
    {
        const int die = detail::class_hit_die(detail::class_from_id(id));
        auto held = std::find_if(result.begin(), result.end(),
                                 [&](const auto &dice)
                                 {
                                     return dice.die == die;
                                 });
        if (held == result.end())
        {
            result.push_back({die, 0});
            held = result.end() - 1;
        }
        ++held->count;
    }
    std::sort(result.begin(), result.end(),
              [](const auto &a, const auto &b)
              {
                  return a.die > b.die;
              });
    return result;
}

int minimum_save_roll(int difficulty_class, int bonus) noexcept
{
    return static_cast<int>(
        std::clamp(static_cast<long long>(difficulty_class) - bonus, 1LL, 21LL));
}

bool attack_hits(int natural_roll, int bonus, int armor_class) noexcept
{
    return natural_roll == 20 ||
           (natural_roll != 1 && static_cast<std::int64_t>(natural_roll) + bonus >= armor_class);
}

bool usable_outside_combat(std::string_view item) noexcept
{
    return item == "manual_of_bodily_health" || item == "potion_of_healing" ||
           item == "potion_of_greater_healing" || item == "potion_of_giant_strength" ||
           item == "wand_of_secrets";
}

std::string_view spell_label(std::string_view spell) noexcept
{
    const auto *found = detail::find_spell(spell);
    return found && found->id == spell ? found->label : std::string_view{};
}

std::string_view feat_label(std::string_view feat) noexcept
{
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 11> labels{{
        {"ability_score_improvement", "Ability Score Improvement"},
        {"alert", "Alert"},
        {"archery", "Archery"},
        {"defense", "Defense"},
        {"great_weapon_fighting", "Great Weapon Fighting"},
        {"magic_initiate_cleric", "Magic Initiate (Cleric)"},
        {"magic_initiate_druid", "Magic Initiate (Druid)"},
        {"magic_initiate_wizard", "Magic Initiate (Wizard)"},
        {"savage_attacker", "Savage Attacker"},
        {"skilled", "Skilled"},
        {"two_weapon_fighting", "Two-Weapon Fighting"},
    }};
    const auto found = std::find_if(labels.begin(), labels.end(),
                                    [&](const auto &entry)
                                    {
                                        return entry.first == feat;
                                    });
    return found == labels.end() ? std::string_view{} : found->second;
}

std::string_view charge_noun(std::string_view item) noexcept
{
    const auto *magic = detail::magic_item(item);
    return magic ? magic->noun : std::string_view{};
}

std::string equipment_note(const CharacterSheet &sheet, std::string_view item)
{
    if (const auto *magic = detail::magic_item(item))
    {
        return std::string(magic->note);
    }
    if (item == poisoners_kit)
    {
        return "A Rogue carrying it can poison a foe with Cunning Strike.";
    }
    if (item == basic_poison)
    {
        return "In combat, a Bonus Action coats your weapon: its next Piercing or Slashing hit "
               "within a minute deals 1d4 extra Poison damage.";
    }
    if (item == thieves_tools)
    {
        return "A Rogue carrying them has twice the chance to pick a lock.";
    }
    if (item == magic_lock_picks)
    {
        return "A Rogue picking a lock uses one set for Advantage on the check.";
    }
    if (item == stone_of_good_luck)
    {
        return "Carried: +1 to ability checks, Initiative included, and saving throws. A second "
               "stone adds nothing.";
    }
    if (rules::magic_ink_level(item))
    {
        return "A bottle writes a spell of its level or lower on a scroll, or copies one from a "
               "scroll into a Wizard's spellbook.";
    }
    if (const auto scroll = rules::parse_scroll_key(item))
    {
        return scroll->unidentified ? "Identify the scroll to learn its spells."
               : scroll->arcane     ? "An arcane scroll: a Bard, Sorcerer, Warlock or Wizard reads "
                                      "its spells, and a Wizard can copy them. A level-13 Thief "
                                      "reads any scroll."
                                    : "A divine scroll: a Cleric, Druid, Paladin or Ranger reads "
                                      "its spells. A level-13 Thief reads any scroll.";
    }
    // An unidentified worn item does not say what it does (#262).
    if (const auto *worn = detail::worn_item(item))
    {
        return rules::parse_item_key(item).unidentified ? "Worn: it needs no hand."
                                                        : std::string(worn->note);
    }
    std::string text;
    if (trained(detail::class_of(sheet), sheet.grants, item))
    {
        text = "Class training: no untrained-use penalty.";
    }
    else if (detail::is_shield(item))
    {
        text = "Untrained shield: no AC bonus.";
    }
    else if (detail::armor(item))
    {
        text =
            "Untrained armor: disadvantage on Strength/Dexterity attacks, checks (including initiative), and saves; cannot cast spells.";
    }
    else
    {
        text = "Untrained weapon: no proficiency bonus on attack rolls.";
    }
    if (rules::base_item_key(item) == "chain_mail" && sheet.scores[Ability::strength] < 13)
    {
        text += " Chain mail requires Strength 13: speed reduced by 10 feet.";
    }
    if (item == "wand")
    {
        text += " Plain focus only; no charged wand spell is granted.";
    }
    // An alignment-bound sword says so once identified (#267).
    if (const auto key = rules::parse_item_key(item);
        !key.unidentified &&
        (key.power == rules::ItemPower::good || key.power == rules::ItemPower::evil))
    {
        text += key.power == rules::ItemPower::good
                    ? " Only a Good wielder can hold it; it burns anyone else who takes it up."
                    : " Only an Evil wielder can hold it; it burns anyone else who takes it up.";
    }
    return text;
}

namespace
{
// The monster ability rows an `off` row may leave out, so that one ability can
// be switched off without deleting its rows.
constexpr std::array switchable_abilities{std::string_view{"advantage_damage"},
                                          std::string_view{"attach"},
                                          std::string_view{"bite"},
                                          std::string_view{"bloodied_fury"},
                                          std::string_view{"breath"},
                                          std::string_view{"charge"},
                                          std::string_view{"cunning_action"},
                                          std::string_view{"dreadful_glare"},
                                          std::string_view{"enthralling_panache"},
                                          std::string_view{"incorporeal"},
                                          std::string_view{"innate"},
                                          std::string_view{"last_attack"},
                                          std::string_view{"legendary"},
                                          std::string_view{"legendary_resistance"},
                                          std::string_view{"life_drain"},
                                          std::string_view{"life_drain_attack"},
                                          std::string_view{"nimble_escape"},
                                          std::string_view{"pack_tactics"},
                                          std::string_view{"parry"},
                                          std::string_view{"petrifying_gaze"},
                                          std::string_view{"prone_large"},
                                          std::string_view{"prone_on_hit"},
                                          std::string_view{"regeneration"},
                                          std::string_view{"repulsion_breath"},
                                          std::string_view{"rotting_curse"},
                                          std::string_view{"sunlight_sensitivity"},
                                          std::string_view{"undead_fortitude"},
                                          std::string_view{"web_walker"},
                                          std::string_view{"hampering"},
                                          std::string_view{"poisoning"},
                                          std::string_view{"pushing"},
                                          std::string_view{"hit_advantage"},
                                          std::string_view{"captains_charm"},
                                          std::string_view{"riposte"},
                                          std::string_view{"create_specter"},
                                          std::string_view{"flyby"},
                                          std::string_view{"magic_resistance"},
                                          std::string_view{"protective_magic"},
                                          std::string_view{"paralyzing_claw"},
                                          std::string_view{"gore"},
                                          std::string_view{"paralyzing_venom"},
                                          std::string_view{"bonus_charge"},
                                          std::string_view{"rampage"},
                                          std::string_view{"trampling_charge"},
                                          std::string_view{"ethereal_jaunt"}};

// Whether `entry` names a switchable ability, "ability" or "ability creature".
bool switchable(std::string_view entry)
{
    const auto ability = entry.substr(0, entry.find(' '));
    return std::find(switchable_abilities.begin(), switchable_abilities.end(), ability) !=
           switchable_abilities.end();
}

// The rows `off ability [creature]` leave out: "ability" for every creature,
// "ability creature" for one.
std::set<std::string> switched_off(const std::string &bytes)
{
    std::set<std::string> off;
    std::istringstream lines(bytes);
    std::string line;
    while (std::getline(lines, line))
    {
        std::istringstream row(line);
        std::string tag, ability, creature;
        row >> tag >> ability >> creature >> std::ws;
        if (tag != "off")
        {
            continue;
        }
        if (std::find(switchable_abilities.begin(), switchable_abilities.end(), ability) ==
                switchable_abilities.end() ||
            !row.eof())
        {
            throw std::runtime_error("Invalid off row: " + line);
        }
        off.insert(creature.empty() ? ability : ability + " " + creature);
    }
    return off;
}
} // namespace

std::unique_ptr<RulesModule> load(const std::filesystem::path &file)
{
    if (std::filesystem::file_size(file) > 65536)
    {
        throw std::runtime_error("Rules content exceeds size limit");
    }
    std::ifstream input(file, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Cannot open rules content: " + path_text(file));
    }
    std::string bytes{std::istreambuf_iterator<char>(input), {}};
    if (bytes.size() > 65536 || input.bad())
    {
        throw std::runtime_error("Invalid rules content size/read");
    }
    return parse_content(bytes);
}

std::unique_ptr<RulesModule> parse_content(std::string_view content_bytes)
{
    return detail::make_module(content_bytes);
}

namespace detail
{
Content read_content(std::string_view content_bytes, const std::set<std::string> &switched)
{
    if (content_bytes.size() > 65536)
    {
        throw std::runtime_error("Rules content exceeds size limit");
    }
    std::string bytes(content_bytes);
    // Git may translate line endings; identical content must keep its identity.
    bytes.erase(std::remove(bytes.begin(), bytes.end(), '\r'), bytes.end());
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : bytes)
    {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    std::istringstream lines(bytes);
    std::string line, magic, revision;
    unsigned version;
    std::getline(lines, line);
    std::istringstream header(line);
    header >> magic >> version >> revision;
    if (!header || magic != "OPENGOLD_SRD5" || version != 1)
    {
        throw std::runtime_error("Unsupported rules content format");
    }
    header >> std::ws;
    if (!header.eof() || revision.empty() || revision.size() > 80)
    {
        throw std::runtime_error("Invalid rules content header");
    }
    Content content;
    content.identity = {"opengold.srd5", "0.6.275", revision + "/" + std::to_string(hash)};
    auto off = switched_off(bytes);
    // A game option naming no ability (a misspelt one) switches nothing off.
    std::copy_if(switched.begin(), switched.end(), std::inserter(off, off.end()),
                 [](const auto &entry)
                 {
                     return switchable(entry);
                 });
    std::set<std::string> save_rows, casting_rows, damage_rows, size_rows, trait_rows, type_rows,
        equipment_rows;
    while (std::getline(lines, line))
    {
        if (line.empty() || line[0] == '#' || line == "\r")
        {
            continue;
        }
        std::istringstream row(line);
        std::string tag, key;
        Definition d;
        row >> tag >> key;
        if (tag == "off" || off.contains(tag) || off.contains(tag + " " + key))
        {
            continue;
        }
        if (tag == "damage_types" || tag == "affinity")
        {
            const auto found = content.definitions.find(key);
            if (found == content.definitions.end())
            {
                throw std::runtime_error("Unknown damage profile: " + key);
            }
            auto &definition = found->second;
            std::string first, second;
            row >> first >> second;
            if (tag == "damage_types")
            {
                if (!damage_rows.insert(key).second)
                {
                    throw std::runtime_error("Duplicate damage type row");
                }
                definition.melee_type = detail::damage_type(first);
                definition.ranged_type = detail::damage_type(second);
            }
            else
            {
                std::string type;
                row >> type;
                if (first.empty() || first.size() > 128 || definition.affinities.size() >= 128 ||
                    std::any_of(definition.affinities.begin(), definition.affinities.end(),
                                [&](const auto &a)
                                {
                                    return a.source_id == first;
                                }))
                {
                    throw std::runtime_error("Invalid damage affinity source");
                }
                detail::DamageAffinity affinity;
                affinity.source_id = first;
                if (second == "resistance")
                {
                    affinity.kind = detail::AffinityKind::resistance;
                }
                else if (second == "vulnerability")
                {
                    affinity.kind = detail::AffinityKind::vulnerability;
                }
                else if (second == "immunity")
                {
                    affinity.kind = detail::AffinityKind::immunity;
                }
                else
                {
                    throw std::runtime_error("Unknown damage affinity");
                }
                if (type != "all")
                {
                    affinity.type = detail::damage_type(type);
                }
                definition.affinities.push_back(std::move(affinity));
            }
            if (!row)
            {
                throw std::runtime_error("Truncated damage profile");
            }
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown damage profile fields");
            }
            continue;
        }
        if (tag == "size")
        {
            std::string size;
            row >> size;
            static constexpr std::array<std::string_view, 6> names{"tiny",  "small", "medium",
                                                                   "large", "huge",  "gargantuan"};
            const auto found = std::find(names.begin(), names.end(), size);
            if (!row || !content.definitions.contains(key) || found == names.end() ||
                !size_rows.insert(key).second)
            {
                throw std::runtime_error("Invalid creature size");
            }
            content.definitions.at(key).size = static_cast<int>(found - names.begin());
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown creature size fields");
            }
            continue;
        }
        if (tag == "type")
        {
            // SRD 5.2.1 creature types; a definition without a row is Humanoid.
            std::string type;
            row >> type;
            static constexpr std::array<std::string_view, 14> types{
                "aberration", "beast", "celestial", "construct",   "dragon", "elemental", "fey",
                "fiend",      "giant", "humanoid",  "monstrosity", "ooze",   "plant",     "undead"};
            if (!row || !content.definitions.contains(key) ||
                std::find(types.begin(), types.end(), type) == types.end() ||
                !type_rows.insert(key).second)
            {
                throw std::runtime_error("Invalid creature type");
            }
            content.definitions.at(key).creature_type = type;
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown creature type fields");
            }
            continue;
        }
        // "equipment <creature> metal_armor <source record> <armor>": the source
        // and armor document where the fact comes from (#231).
        if (tag == "equipment")
        {
            std::string fact, source, armor;
            row >> fact >> source >> armor;
            const auto found = content.definitions.find(key);
            if (!row || found == content.definitions.end() || fact != "metal_armor" ||
                !equipment_rows.insert(key).second)
            {
                throw std::runtime_error("Invalid creature equipment: " + key);
            }
            found->second.metal_armor = true;
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown creature equipment fields: " + key);
            }
            continue;
        }
        // regeneration: Hit Points the creature regains at the start of each turn.
        if (tag == "regeneration")
        {
            const auto found = content.definitions.find(key);
            int value{};
            row >> value;
            if (!row || found == content.definitions.end() || value <= 0 || value > 50 ||
                !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid regeneration: " + key);
            }
            found->second.regeneration = value;
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown regeneration fields: " + key);
            }
            continue;
        }
        // ranged_extra_damage or melee_extra_damage creature count sides type:
        // a ranged or melee hit's added damage.
        if (tag == "ranged_extra_damage" || tag == "melee_extra_damage")
        {
            const bool ranged = tag == "ranged_extra_damage";
            const auto found = content.definitions.find(key);
            Dice extra;
            std::string type;
            row >> extra.count >> extra.sides >> type;
            if (!row || found == content.definitions.end() ||
                !(ranged ? found->second.ranged.count : found->second.melee.count) ||
                extra.count < 1 || extra.count > 10 || extra.sides < 2 || extra.sides > 20 ||
                !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid extra damage: " + key);
            }
            (ranged ? found->second.ranged_extra : found->second.melee_extra) = extra;
            (ranged ? found->second.ranged_extra_type : found->second.melee_extra_type) =
                detail::damage_type(type);
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown ranged extra damage fields: " + key);
            }
            continue;
        }
        if (tag == "perception")
        {
            const auto found = content.definitions.find(key);
            int bonus{};
            row >> bonus;
            if (!row || found == content.definitions.end() || bonus < -5 || bonus > 20 ||
                !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid Perception: " + key);
            }
            found->second.perception = bonus;
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown Perception fields: " + key);
            }
            continue;
        }
        if (tag == "multiattack")
        {
            const auto found = content.definitions.find(key);
            int attacks{};
            row >> attacks;
            if (!row || found == content.definitions.end() || attacks < 2 || attacks > 4 ||
                !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid Multiattack: " + key);
            }
            found->second.multiattack = attacks;
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown Multiattack fields: " + key);
            }
            continue;
        }
        // innate creature label uses action|bonus spell...: a pool of daily
        // spell uses ("Divine_Aid" is shown as "Divine Aid"). Uses "recharge5"
        // is one use regained on a d6 roll of 5 or more (Recharge 5-6).
        if (tag == "innate")
        {
            const auto found = content.definitions.find(key);
            Definition::InnatePool pool;
            std::string uses, casting, spell;
            row >> pool.label >> uses >> casting;
            while (row >> spell)
            {
                pool.spells.push_back(spell);
            }
            std::replace(pool.label.begin(), pool.label.end(), '_', ' ');
            pool.bonus_action = casting == "bonus";
            constexpr std::string_view recharge = "recharge";
            const bool recharging = uses.starts_with(recharge);
            const auto count = std::string_view(uses).substr(recharging ? recharge.size() : 0);
            int number{};
            if (count.empty() ||
                std::from_chars(count.data(), count.data() + count.size(), number).ptr !=
                    count.data() + count.size())
            {
                number = 0;
            }
            pool.uses = recharging ? 1 : number;
            pool.recharge = recharging ? number : 0;
            if (found == content.definitions.end() || pool.label.empty() ||
                pool.label.size() > 40 || pool.uses < 1 || pool.uses > 15 ||
                (recharging && (pool.recharge < 2 || pool.recharge > 6)) ||
                (casting != "action" && casting != "bonus") || pool.spells.empty() ||
                found->second.innate.size() >= 4 ||
                std::any_of(pool.spells.begin(), pool.spells.end(),
                            [](const std::string &id)
                            {
                                const auto *known = detail::find_spell(id);
                                return !known || known->id != id || !known->level;
                            }))
            {
                throw std::runtime_error("Invalid innate spells: " + key);
            }
            // The creature knows its innate spells, as a caster knows its own.
            auto &known = found->second.spells;
            for (const auto &id : pool.spells)
            {
                if (std::find(known.begin(), known.end(), id) == known.end())
                {
                    known.push_back(id);
                }
            }
            found->second.innate.push_back(std::move(pool));
            continue;
        }
        // life_drain creature dc count sides bonus: a Wight's Life Drain.
        if (tag == "life_drain")
        {
            const auto found = content.definitions.find(key);
            int dc{};
            Dice drain;
            row >> dc >> drain.count >> drain.sides >> drain.bonus;
            if (!row || found == content.definitions.end() || dc < 1 || dc > 30 ||
                drain.count < 1 || drain.count > 10 || drain.sides < 2 || drain.sides > 20 ||
                drain.bonus < 0 || drain.bonus > 30 || !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid Life Drain: " + key);
            }
            found->second.life_drain_dc = dc;
            found->second.life_drain = drain;
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown Life Drain fields: " + key);
            }
            continue;
        }
        // paralyzing_claw creature dc bonus count sides modifier type: a
        // Ghoul's Claw. gore creature feet charge_count charge_sides bonus
        // count sides modifier type: a Minotaur's Gore after a charge.
        if (tag == "paralyzing_claw" || tag == "gore")
        {
            const auto found = content.definitions.find(key);
            Definition::LastAttack attack;
            int first{};
            detail::DamageDice charge;
            std::string type;
            row >> first;
            if (tag == "gore")
            {
                row >> charge.count >> charge.sides;
            }
            row >> attack.bonus >> attack.dice.count >> attack.dice.sides >> attack.dice.bonus >>
                type;
            if (!row || found == content.definitions.end() || first < 1 || first > 30 ||
                (tag == "gore" && (first % 5 || charge.count < 1 || charge.count > 10 ||
                                   charge.sides < 2 || charge.sides > 20)) ||
                attack.bonus < -10 || attack.bonus > 30 || attack.dice.count < 1 ||
                attack.dice.count > 10 || attack.dice.sides < 2 || attack.dice.sides > 20 ||
                attack.dice.bonus < -10 || attack.dice.bonus > 30 ||
                !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid " + tag + ": " + key);
            }
            attack.type = detail::damage_type(type);
            if (tag == "gore")
            {
                found->second.gore = attack;
                found->second.gore_feet = first;
                found->second.gore_charge = charge;
            }
            else
            {
                found->second.claw = attack;
                found->second.claw_dc = first;
            }
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown " + tag + " fields: " + key);
            }
            continue;
        }
        // trampling_charge creature dc count sides modifier recharge: a
        // Centaur Trooper's Trampling Charge.
        if (tag == "trampling_charge")
        {
            const auto found = content.definitions.find(key);
            int dc{}, recharge{};
            detail::DamageDice damage;
            row >> dc >> damage.count >> damage.sides >> damage.bonus >> recharge;
            if (!row || found == content.definitions.end() || dc < 1 || dc > 30 ||
                damage.count < 1 || damage.count > 10 || damage.sides < 2 || damage.sides > 20 ||
                damage.bonus < 0 || damage.bonus > 30 || recharge < 2 || recharge > 6 ||
                !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid Trampling Charge: " + key);
            }
            found->second.trample_dc = dc;
            found->second.trample_damage = damage;
            found->second.trample_recharge = recharge;
            found->second.trait_uses = 1;
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown Trampling Charge fields: " + key);
            }
            continue;
        }
        // last_attack creature bonus count sides modifier type extra_count
        // extra_sides extra_type: the Multiattack's last melee attack.
        if (tag == "last_attack")
        {
            const auto found = content.definitions.find(key);
            Definition::LastAttack last;
            std::string type, extra_type;
            row >> last.bonus >> last.dice.count >> last.dice.sides >> last.dice.bonus >> type >>
                last.extra.count >> last.extra.sides >> extra_type;
            if (!row || found == content.definitions.end() || found->second.multiattack < 2 ||
                last.bonus < -10 || last.bonus > 30 || last.dice.count < 1 ||
                last.dice.count > 10 || last.dice.sides < 2 || last.dice.sides > 20 ||
                last.dice.bonus < -10 || last.dice.bonus > 30 || last.extra.count < 0 ||
                last.extra.count > 10 ||
                (last.extra.count && (last.extra.sides < 2 || last.extra.sides > 20)) ||
                !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid last attack: " + key);
            }
            last.type = detail::damage_type(type);
            last.extra_type = detail::damage_type(extra_type);
            found->second.last_attack = last;
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown last attack fields: " + key);
            }
            continue;
        }
        // attach creature count sides type: a Stirge's drain while attached.
        if (tag == "attach")
        {
            const auto found = content.definitions.find(key);
            detail::DamageDice drain;
            std::string type;
            row >> drain.count >> drain.sides >> type;
            if (!row || found == content.definitions.end() || drain.count < 1 || drain.count > 10 ||
                drain.sides < 2 || drain.sides > 20 || !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid attachment: " + key);
            }
            found->second.attach_drain = drain;
            found->second.attach_type = detail::damage_type(type);
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown attachment fields: " + key);
            }
            continue;
        }
        // petrifying_gaze creature dc recharge: a Basilisk's Petrifying Gaze
        // (Recharge 4-6 is 4); its one use is the creature's trait use.
        if (tag == "petrifying_gaze")
        {
            const auto found = content.definitions.find(key);
            int dc{}, recharge{};
            row >> dc >> recharge;
            if (!row || found == content.definitions.end() || dc < 1 || dc > 30 || recharge < 2 ||
                recharge > 6 || !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid Petrifying Gaze: " + key);
            }
            found->second.gaze_dc = dc;
            found->second.gaze_recharge = recharge;
            found->second.trait_uses = 1;
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown Petrifying Gaze fields: " + key);
            }
            continue;
        }
        // rotting_curse creature: a Mummy's Rotting Fist, whose hits curse.
        if (tag == "rotting_curse")
        {
            const auto found = content.definitions.find(key);
            if (found == content.definitions.end() || !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid Rotting Fist curse: " + key);
            }
            found->second.rotting_curse = true;
            continue;
        }
        // dreadful_glare creature dc range: a Mummy's Dreadful Glare.
        if (tag == "dreadful_glare")
        {
            const auto found = content.definitions.find(key);
            int dc{}, range{};
            row >> dc >> range;
            if (!row || found == content.definitions.end() || dc < 1 || dc > 30 || range < 5 ||
                range > 300 || !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid Dreadful Glare: " + key);
            }
            found->second.glare_dc = dc;
            found->second.glare_range = range;
            continue;
        }
        // bite creature dc count sides bonus count sides: a Vampire's Bite, its
        // Piercing dice and then its Necrotic dice.
        if (tag == "bite")
        {
            const auto found = content.definitions.find(key);
            int dc{};
            detail::DamageDice pierce, necrotic;
            row >> dc >> pierce.count >> pierce.sides >> pierce.bonus >> necrotic.count >>
                necrotic.sides;
            if (!row || found == content.definitions.end() || dc < 1 || dc > 30 ||
                pierce.count < 1 || pierce.count > 10 || pierce.sides < 2 || pierce.sides > 20 ||
                pierce.bonus < 0 || pierce.bonus > 30 || necrotic.count < 1 ||
                necrotic.count > 10 || necrotic.sides < 2 || necrotic.sides > 20 ||
                !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid Bite: " + key);
            }
            found->second.bite_dc = dc;
            found->second.bite_pierce = pierce;
            found->second.bite_necrotic = necrotic;
            continue;
        }
        // parry creature bonus: a monster's Parry, the AC its Reaction adds
        // against a melee attack roll that hits it.
        if (tag == "parry")
        {
            const auto found = content.definitions.find(key);
            int bonus{};
            row >> bonus;
            if (!row || found == content.definitions.end() || bonus < 1 || bonus > 10 ||
                !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid Parry: " + key);
            }
            found->second.parry = bonus;
            continue;
        }
        // breath creature spell dc recharge: a monster's breath weapon, such as
        // an Adult Bronze Dragon's Lightning Breath (Recharge 5-6 is 5); its
        // one use is the creature's trait use.
        if (tag == "breath")
        {
            const auto found = content.definitions.find(key);
            std::string spell;
            int dc{}, recharge{};
            row >> spell >> dc >> recharge;
            const auto *known = detail::find_spell(spell);
            if (!row || found == content.definitions.end() || !known || known->id != spell ||
                known->level || known->pattern != detail::SpellPattern::save_damage || dc < 1 ||
                dc > 30 || recharge < 2 || recharge > 6 || found->second.gaze_recharge ||
                found->second.protective_magic || !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid breath weapon: " + key);
            }
            found->second.monster_breath = spell;
            found->second.breath_dc = dc;
            found->second.breath_recharge = recharge;
            found->second.trait_uses = 1;
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown breath weapon fields: " + key);
            }
            continue;
        }
        // hampering creature melee|ranged|last, poisoning creature
        // melee|ranged|last [own_turn], pushing creature melee|ranged feet: the
        // riders of the attack's hits.
        if (tag == "hampering" || tag == "poisoning" || tag == "pushing")
        {
            const auto found = content.definitions.find(key);
            std::string strike, extra;
            row >> strike;
            int feet{};
            if (tag == "pushing")
            {
                row >> feet;
            }
            else if (tag == "poisoning")
            {
                row >> extra;
            }
            row.clear();
            row >> std::ws;
            const bool last = strike == "last";
            const auto kind = strike == "melee"    ? Definition::Strike::melee
                              : strike == "ranged" ? Definition::Strike::ranged
                                                   : Definition::Strike::none;
            if (found == content.definitions.end() || !row.eof() ||
                (kind == Definition::Strike::none && (!last || tag == "pushing")) ||
                (last && !found->second.last_attack) ||
                (tag == "pushing" && (feet < 5 || feet > 60 || feet % 5)) ||
                (!extra.empty() && extra != "own_turn") ||
                !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid hit rider: " + line);
            }
            auto &definition = found->second;
            if (tag == "hampering" && last)
            {
                definition.last_attack->hampering = true;
            }
            else if (tag == "hampering")
            {
                definition.hampering = kind;
            }
            else if (tag == "poisoning")
            {
                if (last)
                {
                    definition.last_attack->poisoning = true;
                    definition.last_attack->poisoning_own_turn = !extra.empty();
                }
                else
                {
                    definition.poisoning = kind;
                    definition.poisoning_own_turn = !extra.empty();
                }
            }
            else
            {
                definition.pushing = kind;
                definition.push_feet = feet;
            }
            continue;
        }
        // flyby creature, magic_resistance creature, protective_magic
        // creature uses_a_day.
        if (tag == "flyby" || tag == "magic_resistance" || tag == "protective_magic")
        {
            const auto found = content.definitions.find(key);
            int uses{};
            if (tag == "protective_magic")
            {
                row >> uses;
            }
            if (!row || found == content.definitions.end() ||
                (tag == "protective_magic" &&
                 (uses < 1 || uses > 10 || found->second.trait_uses)) ||
                !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid monster ability: " + line);
            }
            auto &definition = found->second;
            if (tag == "flyby")
            {
                definition.flyby = true;
            }
            else if (tag == "magic_resistance")
            {
                definition.magic_resistance = true;
            }
            else
            {
                definition.protective_magic = true;
                definition.trait_uses = uses;
            }
            continue;
        }
        // hit_advantage creature, riposte creature bonus, create_specter
        // creature limit undead, captains_charm creature dc range.
        if (tag == "hit_advantage" || tag == "riposte" || tag == "create_specter" ||
            tag == "captains_charm")
        {
            const auto found = content.definitions.find(key);
            int first{}, second{};
            std::string undead;
            if (tag != "hit_advantage")
            {
                row >> first;
            }
            if (tag == "captains_charm")
            {
                row >> second;
            }
            if (tag == "create_specter")
            {
                row >> undead;
            }
            if (!row || found == content.definitions.end() ||
                (tag == "riposte" && (first < 1 || first > 10 || found->second.parry)) ||
                (tag == "create_specter" &&
                 (first < 1 || first > 20 || !content.definitions.contains(undead))) ||
                (tag == "captains_charm" &&
                 (first < 1 || first > 30 || second < 5 || second > 120)) ||
                !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid monster ability: " + line);
            }
            auto &definition = found->second;
            if (tag == "hit_advantage")
            {
                definition.hit_advantage = true;
            }
            else if (tag == "riposte")
            {
                definition.parry = first;
                definition.riposte = true;
            }
            else if (tag == "create_specter")
            {
                definition.specter_limit = first;
                definition.specter_key = undead;
            }
            else
            {
                definition.charm_dc = first;
                definition.charm_range = second;
            }
            continue;
        }
        // repulsion_breath creature dc: an Adult Bronze Dragon's Repulsion
        // Breath, its Strength save DC.
        if (tag == "repulsion_breath")
        {
            const auto found = content.definitions.find(key);
            int dc{};
            row >> dc;
            if (!row || found == content.definitions.end() || dc < 1 || dc > 30 ||
                !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid Repulsion Breath: " + key);
            }
            found->second.repulsion_dc = dc;
            continue;
        }
        // legendary creature uses option...: Legendary Actions, `uses` a round,
        // with the options beguile, deathless_strike, guiding_light, pounce
        // and thunderclap.
        if (tag == "legendary")
        {
            const auto found = content.definitions.find(key);
            int uses{};
            row >> uses;
            if (!row || found == content.definitions.end() || uses < 1 || uses > 5 ||
                !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid Legendary Actions: " + key);
            }
            using Option = Definition::LegendaryAction;
            static constexpr std::array<std::pair<std::string_view, Option>, 5> options{
                {{"beguile", Option::beguile},
                 {"deathless_strike", Option::deathless_strike},
                 {"guiding_light", Option::guiding_light},
                 {"pounce", Option::pounce},
                 {"thunderclap", Option::thunderclap}}};
            std::string name;
            while (row >> name)
            {
                const auto option = std::find_if(options.begin(), options.end(),
                                                 [&](const auto &known)
                                                 {
                                                     return known.first == name;
                                                 });
                if (option == options.end())
                {
                    throw std::runtime_error("Unknown Legendary Action: " + name);
                }
                found->second.legendary_actions.push_back(option->second);
            }
            if (found->second.legendary_actions.empty())
            {
                throw std::runtime_error("Legendary Actions need an option: " + key);
            }
            found->second.legendary_uses = uses;
            continue;
        }
        // legendary_resistance creature uses: Legendary Resistance, `uses` a
        // day.
        if (tag == "legendary_resistance")
        {
            const auto found = content.definitions.find(key);
            int uses{};
            row >> uses;
            if (!row || found == content.definitions.end() || uses < 1 || uses > 10 ||
                !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid Legendary Resistance: " + key);
            }
            found->second.legendary_resistance = uses;
            continue;
        }
        // condition_immunity creature condition...: SRD condition names
        // ("restrained", "prone").
        if (tag == "condition_immunity")
        {
            const auto found = content.definitions.find(key);
            if (found == content.definitions.end() || !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid condition immunity: " + key);
            }
            std::string condition;
            while (row >> condition)
            {
                const auto name = detail::condition_label(condition);
                if (name.empty())
                {
                    throw std::runtime_error("Unknown condition: " + condition);
                }
                found->second.condition_immunities.emplace_back(name);
            }
            if (found->second.condition_immunities.empty())
            {
                throw std::runtime_error("Invalid condition immunity: " + key);
            }
            continue;
        }
        if (tag == "enthralling_panache")
        {
            const auto found = content.definitions.find(key);
            int dc{};
            row >> dc;
            if (!row || found == content.definitions.end() || dc < 1 || dc > 30 ||
                !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid Enthralling Panache: " + key);
            }
            found->second.panache_dc = dc;
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown Enthralling Panache fields: " + key);
            }
            continue;
        }
        if (tag == "innate_ranged")
        {
            const auto found = content.definitions.find(key);
            std::string missile;
            row >> missile;
            if (found == content.definitions.end() || !row.eof() ||
                (!missile.empty() && missile != "boulder"))
            {
                throw std::runtime_error("Invalid innate_ranged row: " + key);
            }
            found->second.innate_ranged = true;
            found->second.ranged_missile =
                missile.empty() ? rules::Missile::none : rules::Missile::boulder;
            continue;
        }
        if (tag == "pack_tactics" || tag == "aggressive" || tag == "advantage_damage" ||
            tag == "undead_fortitude" || tag == "multiattack_ranged" ||
            tag == "sunlight_sensitivity" || tag == "evil" || tag == "life_drain_attack" ||
            tag == "flies" || tag == "incorporeal" || tag == "web_walker" ||
            tag == "cunning_action" || tag == "bloodied_fury" || tag == "charge" ||
            tag == "prone_on_hit" || tag == "prone_large" || tag == "nimble_escape" ||
            tag == "paralyzing_venom" || tag == "bonus_charge" || tag == "rampage" ||
            tag == "ethereal_jaunt")
        {
            const auto found = content.definitions.find(key);
            if (found == content.definitions.end() || !trait_rows.insert(tag + " " + key).second)
            {
                throw std::runtime_error("Invalid creature trait: " + key);
            }
            auto &definition = found->second;
            if (tag == "pack_tactics")
            {
                definition.pack_tactics = true;
            }
            else if (tag == "aggressive")
            {
                definition.aggressive = true;
            }
            else if (tag == "undead_fortitude")
            {
                definition.undead_fortitude = true;
            }
            else if (tag == "multiattack_ranged")
            {
                definition.multiattack_ranged = true;
            }
            else if (tag == "sunlight_sensitivity")
            {
                definition.sunlight_sensitive = true;
            }
            else if (tag == "evil")
            {
                definition.evil = true;
            }
            else if (tag == "life_drain_attack")
            {
                definition.life_drain_attack = true;
            }
            else if (tag == "flies")
            {
                definition.flies = true;
            }
            else if (tag == "incorporeal")
            {
                definition.incorporeal = true;
            }
            else if (tag == "web_walker")
            {
                definition.web_walker = true;
            }
            else if (tag == "cunning_action")
            {
                definition.cunning = true;
            }
            // The beast forms' attack traits: Bloodied Fury, a Boar's charge
            // (20 feet straight: 1d6 more and Prone), a melee hit that knocks
            // a Medium or smaller target Prone (Large too with prone_large).
            else if (tag == "bloodied_fury")
            {
                definition.bloodied_fury = true;
            }
            else if (tag == "charge")
            {
                definition.charge = true;
            }
            else if (tag == "prone_on_hit" || tag == "prone_large")
            {
                definition.prone_bite = true;
                definition.prone_large = tag == "prone_large";
            }
            else if (tag == "nimble_escape")
            {
                definition.nimble_escape = true;
            }
            else if (tag == "paralyzing_venom")
            {
                definition.paralyzing_venom = true;
            }
            else if (tag == "bonus_charge")
            {
                definition.bonus_charge = true;
            }
            else if (tag == "ethereal_jaunt")
            {
                definition.ethereal_jaunt = true;
            }
            else if (tag == "rampage")
            {
                definition.rampage = true;
                definition.trait_uses = 1;
            }
            else
            {
                auto &extra = definition.advantage_damage;
                row >> extra.count >> extra.sides;
                if (!row || extra.count < 1 || extra.count > 10 || extra.sides < 2 ||
                    extra.sides > 20)
                {
                    throw std::runtime_error("Invalid Advantage damage: " + key);
                }
            }
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown creature trait fields: " + key);
            }
            continue;
        }
        if (tag == "saves" || tag == "spellcasting")
        {
            const auto found = content.definitions.find(key);
            auto &seen = tag == "saves" ? save_rows : casting_rows;
            if (found == content.definitions.end() || !seen.insert(key).second)
            {
                throw std::runtime_error("Invalid supplemental creature definition: " + key);
            }
            auto &definition = found->second;
            if (tag == "saves")
            {
                for (auto &bonus : definition.saves)
                {
                    row >> bonus;
                }
                if (std::any_of(definition.saves.begin(), definition.saves.end(),
                                [](int n)
                                {
                                    return n < -10 || n > 30;
                                }))
                {
                    throw std::runtime_error("Invalid saving throw bonus");
                }
            }
            else
            {
                // Authored creature rows keep the compact bitmask: they are a
                // handful of spells, so the 31-bit limit is no constraint there.
                // It is translated to ids at the boundary; nothing downstream
                // sees a mask.
                int casting_spells{};
                row >> definition.slots[2] >> casting_spells;
                if (definition.level < 3 || definition.slots[2] < 0 || definition.slots[2] > 20 ||
                    casting_spells < 0 || casting_spells > 63)
                {
                    throw std::runtime_error("Invalid supplemental spellcasting");
                }
                definition.spells = detail::spells_from_mask(static_cast<unsigned>(casting_spells));
                definition.known_cantrips = detail::spells_of_level(definition.spells, true);
                if (detail::mask_from_spells(definition.spells) !=
                    static_cast<unsigned>(casting_spells))
                {
                    throw std::runtime_error("Invalid supplemental spellcasting");
                }
            }
            if (!row)
            {
                throw std::runtime_error("Truncated supplemental creature definition");
            }
            row >> std::ws;
            if (!row.eof())
            {
                throw std::runtime_error("Unknown supplemental creature fields");
            }
            continue;
        }
        int creature_spells{};
        row >> d.ac >> d.hp >> d.initiative >> d.speed >> d.melee_bonus >> d.melee.count >>
            d.melee.sides >> d.melee.bonus >> d.ranged_bonus >> d.ranged.count >> d.ranged.sides >>
            d.ranged.bonus >> d.range >> d.long_range >> d.winds >> d.slots[1] >> d.casting >>
            d.level >> creature_spells;

        const bool ranged_none = d.range == 0 && d.long_range == 0 && d.ranged_bonus == 0 &&
                                 d.ranged.count == 0 && d.ranged.sides == 0 && d.ranged.bonus == 0;
        const bool ranged_weapon = d.range >= 5 && d.long_range >= d.range && d.long_range <= 600 &&
                                   d.ranged_bonus >= -10 && d.ranged_bonus <= 30 &&
                                   d.ranged.count >= 1 && d.ranged.count <= 10 &&
                                   d.ranged.sides >= 2 && d.ranged.sides <= 20 &&
                                   d.ranged.bonus >= -10 && d.ranged.bonus <= 30;
        if (!row || tag != "creature" || key.size() > 80 || content.definitions.contains(key) ||
            d.ac < 1 || d.ac > 40 || d.hp < 1 || d.hp > 1000 || d.initiative < -10 ||
            d.initiative > 20 || d.speed < 5 || d.speed > 120 || d.speed % 5 || d.melee.count < 1 ||
            d.melee.count > 10 || d.melee.sides < 2 || d.melee.sides > 20 ||
            (!ranged_none && !ranged_weapon) || d.winds < 0 || d.winds > 10 || d.slots[1] < 0 ||
            d.slots[1] > 20 || d.level < 1 || d.level > 4 || creature_spells < 0 ||
            creature_spells > 7 || d.melee_bonus < -10 || d.melee_bonus > 30 || d.casting < -10 ||
            d.casting > 30 || d.melee.bonus < -10 || d.melee.bonus > 30)
        {
            throw std::runtime_error("Invalid or unsupported creature definition: " + key);
        }
        d.spells = detail::spells_from_mask(static_cast<unsigned>(creature_spells));
        d.known_cantrips = detail::spells_of_level(d.spells, true);
        if (detail::mask_from_spells(d.spells) != static_cast<unsigned>(creature_spells))
        {
            throw std::runtime_error("Invalid or unsupported creature definition: " + key);
        }
        row >> std::ws;
        if (!row.eof())
        {
            throw std::runtime_error("Unknown creature fields: " + key);
        }
        d.unarmored_arcane_caster = key.find("magic-user") != std::string::npos;
        content.definitions.emplace(std::move(key), std::move(d));
    }
    if (content.definitions.empty())
    {
        throw std::runtime_error("Empty rules content");
    }
    // A creature without the Perception skill uses its Wisdom save, which is
    // its Wisdom modifier unless it is proficient.
    for (auto &[key, definition] : content.definitions)
    {
        if (!trait_rows.contains("perception " + key))
        {
            definition.perception = definition.saves[4];
        }
        // A monster that casts spells reads any scroll it carries (DIFF-1).
        definition.reads_arcane_scrolls = definition.reads_divine_scrolls =
            !definition.spells.empty();
    }
    return content;
}
} // namespace detail
} // namespace opengold::srd5
