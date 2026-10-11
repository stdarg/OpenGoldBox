#include "opengold/campaign_party.h"
#include "opengold/creature_catalog.h"
#include "armor.h"

#include <iostream>

namespace
{
void dice(const opengold::por::DamageDice &d)
{
    std::cout << static_cast<unsigned>(d.count) << 'd' << static_cast<unsigned>(d.sides);
    if (d.modifier != 0)
        std::cout << (d.modifier > 0 ? "+" : "") << d.modifier;
}

void modifier(std::string_view label, const std::optional<int> &value)
{
    std::cout << "  " << label << ": ";
    if (value)
        std::cout << *value;
    else
        std::cout << "unknown";
    std::cout << '\n';
}

void print(const opengold::por::Creature &c)
{
    const auto &s = c.stored;
    std::cout << c.id.key() << "  " << s.name << "\n  HP "
              << static_cast<unsigned>(s.max_hit_points)
              << ", base AC " << s.base_armor_class << ", base THAC0 " << s.base_thac0
              << ", movement " << static_cast<unsigned>(s.base_movement) << '\n';
    std::cout << "  STR " << static_cast<unsigned>(s.abilities.strength) << '/'
              << static_cast<unsigned>(s.abilities.exceptional_strength) << ", INT "
              << static_cast<unsigned>(s.abilities.intelligence) << ", WIS "
              << static_cast<unsigned>(s.abilities.wisdom)
              << ", DEX " << static_cast<unsigned>(s.abilities.dexterity) << ", CON "
              << static_cast<unsigned>(s.abilities.constitution) << ", CHA "
              << static_cast<unsigned>(s.abilities.charisma)
              << '\n';
    for (std::size_t i = 0; i < s.base_attacks.size(); ++i)
    {
        std::cout << "  Base attack " << i + 1 << ": " << s.base_attacks[i].attacks_per_round()
                  << " per round, ";
        dice(s.base_attacks[i].damage);
        std::cout << '\n';
    }
    std::cout << "  Saves (death, petrification, wand, breath, spell): "
              << static_cast<unsigned>(s.saves.paralysis_poison_death) << ' '
              << static_cast<unsigned>(s.saves.petrification_polymorph) << ' '
              << static_cast<unsigned>(s.saves.rods_staves_wands) << ' '
              << static_cast<unsigned>(s.saves.breath) << ' '
              << static_cast<unsigned>(s.saves.spells) << '\n';
    modifier("Reference STR to hit", c.abilities.strength_to_hit);
    modifier("Reference STR damage", c.abilities.strength_damage);
    modifier("Reference DEX missile to hit", c.abilities.dexterity_missile_to_hit);
    modifier("Reference DEX AC adjustment", c.abilities.dexterity_ac_adjustment);
    modifier("Reference CON HP/hit die", c.abilities.constitution_hp_per_hit_die);
    std::cout << "  Strength bonus permission hint (unverified CHA[170]): ";
    if (c.abilities.strength_bonus_allowed_hint)
        std::cout << (*c.abilities.strength_bonus_allowed_hint ? "allowed" : "disabled");
    else
        std::cout << "unknown";
    std::cout << '\n';
    for (const auto &item : c.equipment)
    {
        std::cout << "  Item " << item.index << ": " << item.label()
                  << (item.stored.readied() ? " [readied]" : " [carried]") << ", raw bonus "
                  << item.stored.magic_bonus << ", save bonus " << item.bonuses.save_bonus << '\n';
        if (item.bonuses.weapon_to_hit)
        {
            std::cout << "    Weapon to hit " << *item.bonuses.weapon_to_hit << ", damage bonus "
                      << *item.bonuses.weapon_damage << ", small/medium ";
            dice(item.base.small_medium_damage);
            std::cout << ", large ";
            dice(item.base.large_damage);
            std::cout << '\n';
        }
        if (item.bonuses.armor_base_ac)
            modifier("  Armor base AC", item.bonuses.armor_base_ac);
        if (item.bonuses.ac_adjustment)
            modifier("  Item AC adjustment", item.bonuses.ac_adjustment);
        if (item.stored.effect_codes != std::array<std::uint8_t, 3> {})
        {
            std::cout << "    Item activation/effect bytes (not SPC IDs):";
            for (auto code : item.stored.effect_codes)
                std::cout << ' ' << static_cast<unsigned>(code);
            std::cout << '\n';
        }
    }
    for (const auto &effect : c.effects)
    {
        const auto &e = effect.definition;
        std::cout << "  Effect " << static_cast<unsigned>(e.code) << ": " << e.name;
        if (effect.stored.duration_raw == 0)
            std::cout << " [permanent]";
        else
            std::cout << " [raw duration " << effect.stored.duration_raw << ']';
        if (e.regeneration)
        {
            std::cout << ", " << e.regeneration->hp_per_round << " HP/round";
            if (e.regeneration->revival_delay_rounds)
            {
                std::cout << ", revival after ";
                dice(*e.regeneration->revival_delay_rounds);
                std::cout << " rounds";
            }
        }
        if (e.resistance_percent)
            std::cout << ", " << *e.resistance_percent << '%';
        if (e.incoming_damage_multiplier)
            std::cout << ", damage x" << *e.incoming_damage_multiplier;
        if (e.target_save_adjustment)
            std::cout << ", target save adjustment " << *e.target_save_adjustment;
        if (e.levels_drained)
            std::cout << ", drains " << *e.levels_drained << " level(s)";
        std::cout << '\n';
    }
    for (const auto &note : c.interpretation_notes)
        std::cout << "  Note: " << note << '\n';
    std::cout << '\n';
}
// The original record each Slums combat creature converts (as the campaign
// session maps them), for `equipment` rows in combat.rules (#231).
struct Conversion
{
    const char *creature;
    std::uint8_t record;
};

constexpr std::array slums_conversions{
    Conversion{"slums-kobold", 0}, Conversion{"slums-kobold-leader", 1},
    Conversion{"slums-kobold-leader-sword", 11}, Conversion{"slums-goblin", 2},
    Conversion{"slums-goblin-leader", 3}, Conversion{"slums-orc", 4},
    Conversion{"slums-orc-leader", 5}, Conversion{"slums-orc-leader-archer", 5},
    Conversion{"slums-bugbear", 63}, Conversion{"slums-hobgoblin", 6},
    Conversion{"slums-hobgoblin", 7}, Conversion{"ogre", 8}, Conversion{"troll", 31},
    Conversion{"gnoll-warrior", 73}, Conversion{"slums-magic-user", 94}};

// Prints an `equipment` row for each creature whose readied armor is Medium or
// Heavy metal armor (Heat Metal's rule), and a comment for the others.
void print_equipment(const opengold::por::CreatureCatalog &catalog)
{
    for (const auto &conversion : slums_conversions)
    {
        const auto &creature = catalog.find({2, conversion.record})->get();
        std::string armor = "none";
        bool metal = false;
        for (const auto &item : creature.equipment)
        {
            // The armor's kind, whatever its magic.
            auto plain = item;
            plain.stored.magic_bonus = 0;
            plain.stored.cursed_raw = 0;
            plain.stored.effect_codes = {};
            const auto key = opengold::equipment_conversion(plain);
            const auto *worn = opengold::srd5::detail::armor(key);
            if (!item.stored.readied() || !worn ||
                    worn->category == opengold::srd5::detail::ArmorCategory::shield)
                continue;
            armor = key;
            metal = opengold::srd5::detail::metal_armor(*worn);
        }
        const auto source = creature.id.key();
        if (metal)
            std::cout << "equipment " << conversion.creature << " metal_armor " << source << ' '
                      << armor << '\n';
        else
            std::cout << "# " << conversion.creature << ' ' << source << ' ' << armor
                      << ": no metal armor\n";
    }
}
} // namespace

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3)
    {
        std::cerr << "Usage: opengold_creatures GAME_DIRECTORY [EXACT_NAME | --equipment]\n";
        return 2;
    }
    try
    {
        const auto catalog = opengold::por::CreatureCatalog::load(argv[1]);
        if (argc == 3 && std::string_view(argv[2]) == "--equipment")
        {
            print_equipment(catalog);
            return 0;
        }
        std::cout
                << "Loaded " << catalog.all().size() << " monster/NPC records.\n"
                << "Template statistics and separate modifier contributions; no encounter/effect stacking applied.\n\n";
        if (argc == 3)
        {
            const auto matches = catalog.find_by_name(argv[2]);
            if (matches.empty())
            {
                std::cerr << "No matching creature name.\n";
                return 1;
            }
            for (const auto &c : matches)
                print(c.get());
        }
        else
        {
            for (const auto &[id, c] : catalog.all())
                print(c);
        }
    }
    catch (const opengold::por::CatalogError &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
