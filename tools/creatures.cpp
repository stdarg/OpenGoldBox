#include "opengold/creature_catalog.h"

#include <iostream>

namespace
{
void dice(const opengold::por::DamageDice &d)
{
    std::cout << unsigned(d.count) << 'd' << unsigned(d.sides);
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
    std::cout << c.id.key() << "  " << s.name << "\n  HP " << unsigned(s.max_hit_points)
              << ", base AC " << s.base_armor_class << ", base THAC0 " << s.base_thac0
              << ", movement " << unsigned(s.base_movement) << '\n';
    std::cout << "  STR " << unsigned(s.abilities.strength) << '/'
              << unsigned(s.abilities.exceptional_strength) << ", INT "
              << unsigned(s.abilities.intelligence) << ", WIS " << unsigned(s.abilities.wisdom)
              << ", DEX " << unsigned(s.abilities.dexterity) << ", CON "
              << unsigned(s.abilities.constitution) << ", CHA " << unsigned(s.abilities.charisma)
              << '\n';
    for (std::size_t i = 0; i < s.base_attacks.size(); ++i)
    {
        std::cout << "  Base attack " << i + 1 << ": " << s.base_attacks[i].attacks_per_round()
                  << " per round, ";
        dice(s.base_attacks[i].damage);
        std::cout << '\n';
    }
    std::cout << "  Saves (death, petrification, wand, breath, spell): "
              << unsigned(s.saves.paralysis_poison_death) << ' '
              << unsigned(s.saves.petrification_polymorph) << ' '
              << unsigned(s.saves.rods_staves_wands) << ' ' << unsigned(s.saves.breath) << ' '
              << unsigned(s.saves.spells) << '\n';
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
                std::cout << ' ' << unsigned(code);
            std::cout << '\n';
        }
    }
    for (const auto &effect : c.effects)
    {
        const auto &e = effect.definition;
        std::cout << "  Effect " << unsigned(e.code) << ": " << e.name;
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
} // namespace

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3)
    {
        std::cerr << "Usage: opengold_creatures GAME_DIRECTORY [EXACT_NAME]\n";
        return 2;
    }
    try
    {
        const auto catalog = opengold::por::CreatureCatalog::load(argv[1]);
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
