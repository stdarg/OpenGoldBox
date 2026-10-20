#include "opengold/campaign_party.h"
#include "opengold/authored_items.h"
#include "opengold/dice.h"
#include "opengold/ecl_party_host.h"
#include "opengold/item_key.h"
#include "opengold/scroll_key.h"
#include "opengold/magic_item_catalog.h"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

namespace opengold
{
// Commits that install a prepared member in place rely on this not throwing.
static_assert(std::is_nothrow_move_assignable_v<PartyMember>);

namespace
{
// The original's scroll item types (MAGIC-6).
constexpr std::uint8_t magic_user_scroll = 61, cleric_scroll = 62;
std::string_view scroll_spell(std::uint8_t number);

// Erases the spells read from a scroll, keeping those whose bits are set in
// `kept` (a bit for each spell on it, in order), by clearing their spell
// numbers in the original record (MAGIC-6). A scroll with no spell left is
// gone.
void keep_scroll_spells(PartyMember &member, std::uint64_t id, unsigned kept)
{
    const auto source = member.item_sources.find(id);
    if (source == member.item_sources.end())
    {
        throw std::runtime_error("A scroll needs its original record");
    }
    auto &raw = source->second.stored;
    unsigned entry = 0;
    for (std::size_t n = 0; n < raw.effect_codes.size(); ++n)
    {
        // A number no spell has is no entry on the scroll.
        if (scroll_spell(raw.effect_codes[n]).empty())
        {
            continue;
        }
        if (!(kept & (1U << entry)))
        {
            raw.effect_codes[n] = 0;
            raw.raw[60 + n] = 0;
        }
        ++entry;
    }
    auto inventory = member.character.inventory();
    const auto definition = equipment_conversion(source->second);
    if (rules::parse_scroll_key(definition))
    {
        inventory.redefine(id, definition);
    }
    else
    {
        inventory.remove(id, 1);
        std::erase(member.equipped, id);
        member.item_sources.erase(id);
    }
    member.character.replace_inventory(std::move(inventory));
}

// Removes the thrown gear (Acid, Alchemist's Fire, Oil) a fight used up, so the
// member carries `left` of `definition`, taking from the newest stacks first.
void keep_thrown_gear(PartyMember &member, const std::string &definition, unsigned left)
{
    std::vector<std::pair<std::uint64_t, std::uint32_t>> stacks;
    unsigned carried = 0;
    for (const auto &item : member.character.inventory().items())
    {
        if (item.definition_id == definition)
        {
            stacks.emplace_back(item.id, item.quantity);
            carried += item.quantity;
        }
    }
    if (left > carried)
    {
        throw std::runtime_error("Combat reported more thrown gear than was carried");
    }
    auto inventory = member.character.inventory();
    for (auto stack = stacks.rbegin(); stack != stacks.rend() && carried > left; ++stack)
    {
        const auto used = std::min(stack->second, carried - left);
        inventory.remove(stack->first, used);
        carried -= used;
        if (used == stack->second)
        {
            member.item_sources.erase(stack->first);
        }
    }
    member.character.replace_inventory(std::move(inventory));
}

// Trims a charged item (MAGIC-3) to the charges, missiles or potions a fight left
// it; one with none left is gone, as in the original.
void keep_item_uses(PartyMember &member, std::uint64_t id, unsigned left)
{
    const auto item = member.character.inventory().find(id);
    // A scroll reports the spells still on it (MAGIC-6).
    if (item && rules::parse_scroll_key(item->get().definition_id))
    {
        keep_scroll_spells(member, id, left);
        return;
    }
    if (!item || left > item->get().quantity)
    {
        throw std::runtime_error("Combat reported more item uses than were carried");
    }
    const auto used = item->get().quantity - left;
    if (!used)
    {
        return;
    }
    auto inventory = member.character.inventory();
    inventory.remove(id, used);
    if (!left)
    {
        std::erase(member.equipped, id);
        member.item_sources.erase(id);
    }
    member.character.replace_inventory(std::move(inventory));
}

// Remove Curse lets a member put down its cursed items, which are then lost:
// the original's Remove Curse let a character drop a cursed object (MAGIC-1).
void put_down_cursed_items(PartyMember &member)
{
    auto inventory = member.character.inventory();
    std::erase_if(member.equipped,
                  [&](std::uint64_t worn)
                  {
                      const auto &definition = inventory.find(worn)->get().definition_id;
                      if (rules::parse_item_key(definition).power != rules::ItemPower::cursed)
                      {
                          return false;
                      }
                      inventory.remove(worn, 1);
                      member.item_sources.erase(worn);
                      return true;
                  });
    member.character.replace_inventory(std::move(inventory));
}

// The definitions of a member's equipped items, which the rules read as gear.
std::vector<std::string> equipped_definitions(const PartyMember &member)
{
    std::vector<std::string> definitions;
    for (const auto equipped : member.equipped)
    {
        const auto item = member.character.inventory().find(equipped);
        if (!item)
        {
            throw std::runtime_error("Equipped item is missing");
        }
        definitions.push_back(rules_definition(member, item->get()));
    }
    return definitions;
}

// The catalog key of an original weapon, ammunition, armor or shield type,
// the items that can be enchanted; empty for any other type.
std::string_view enchantable_conversion(int type)
{
    switch (type)
    {
    case 1:
        return "battleaxe";
    case 2:
        return "handaxe";
    case 3:
    case 5:
    case 10:
    case 11:
    case 14:
    case 15:
    case 16:
    case 17:
    case 40:
        return "glaive";
    case 4:
    case 18:
    case 19:
        return "halberd";
    case 6:
    case 22:
    case 33:
        return "quarterstaff";
    case 7:
        return "club";
    case 8:
        return "dagger";
    case 9:
        return "dart";
    case 12:
        return "flail";
    case 13:
    case 25:
    case 27:
    case 29:
    case 32:
        return "pike";
    case 20:
        return "warhammer";
    case 21:
        return "javelin";
    case 23:
        return "mace";
    case 24:
        return "morningstar";
    case 26:
        return "war_pick";
    case 30:
        return "scimitar";
    case 31:
        return "spear";
    case 34:
    case 35:
    case 36:
        return "longsword";
    case 37:
        return "shortsword";
    case 38:
        return "greatsword";
    case 39:
        return "trident";
    case 41:
    case 43:
    case 45:
        return "longbow";
    case 42:
    case 44:
        return "shortbow";
    case 46:
        return "light_crossbow";
    case 47:
        return "sling";
    case 28:
        return "bolt";
    case 73:
        return "arrow";
    case 50:
        return "leather";
    case 51:
        return "padded";
    case 52:
        return "studded_leather";
    case 53:
        return "ring_mail";
    case 54:
        return "scale_mail";
    case 55:
        return "chain_mail";
    // SRD 5.2.1 has no Banded Mail; Splint is its heavy armor with the same AD&D AC 4.
    case 56:
    case 57:
        return "splint";
    case 58:
        return "plate";
    case 59:
        return "shield";
    default:
        return {};
    }
}

// The original's special weapons, known by the name word START.EXE gives them.
constexpr std::uint8_t flame_tongue_word = 241, bane_undead_word = 253;

rules::ItemPower item_power(const por::ItemRecord &raw)
{
    const auto named = [&](std::uint8_t word)
    {
        return std::find(raw.name_components.begin(), raw.name_components.end(), word) !=
               raw.name_components.end();
    };
    if (raw.cursed_raw)
    {
        return rules::ItemPower::cursed;
    }
    if (named(flame_tongue_word))
    {
        return rules::ItemPower::flame_tongue;
    }
    if (named(bane_undead_word))
    {
        return rules::ItemPower::bane_undead;
    }
    return rules::ItemPower::none;
}

// The original's charged and single-use items, known by their spell or item
// power (byte 61), become their SRD counterparts (MAGIC-3); empty for any other
// record. Byte 62's bit 0x80 marks an effect applied when readied, not a use.
std::string_view charged_conversion(const por::ItemRecord &raw)
{
    constexpr std::uint8_t cure_light_wounds = 3, lightning_bolt = 51, speed = 80,
                           giant_strength = 82, javelin_lightning = 83, paralysis = 84,
                           healing = 85, disappearance = 86, missiles = 87, magic_missiles = 88,
                           bodily_health = 90;
    constexpr std::uint8_t wand_type = 78;
    const auto [charges, power, flags] = raw.effect_codes;
    // A scroll's bytes are its spells (scroll_conversion).
    if (!charges || (flags & 0x80) || raw.type == magic_user_scroll || raw.type == cleric_scroll)
    {
        return {};
    }
    switch (power)
    {
    case magic_missiles:
        return "wand:magic_missiles";
    case lightning_bolt:
        return raw.type == wand_type ? "wand:lightning_bolts" : "";
    case paralysis:
        return "wand:paralysis";
    case healing:
        return "potion_of_healing";
    // Potion Extra Healing's doses are Potions of Greater Healing.
    case cure_light_wounds:
        return "potion_of_greater_healing";
    case speed:
        return "potion_of_speed";
    case giant_strength:
        return "potion_of_giant_strength";
    case disappearance:
        return "dust_of_disappearance";
    case missiles:
        return "necklace_of_missiles";
    case javelin_lightning:
        return "javelin:lightning";
    case bodily_health:
        return "manual_of_bodily_health";
    default:
        return {};
    }
}

// The original's spells by number (START.EXE's table, MAGIC_ITEMS.md) as SRD
// 5.2.1 spells or stand-ins (MAGIC-6): "invisibility@3" is cast from a 3rd-level
// slot. Empty for a number no spell has, which random treasure can roll.
std::string_view scroll_spell(std::uint8_t number)
{
    static constexpr std::array<std::string_view, 57> spells{
        "",
        "bless",
        "bane",                          // Curse
        "cure_wounds",                   // Cure Light Wounds
        "inflict_wounds",                // Cause Light Wounds
        "detect_magic",                  // cleric
        "protection_from_evil_and_good", // Protection From Evil, cleric
        "protection_from_evil_and_good", // Protection From Good, cleric
        "protection_from_energy",        // Resist Cold
        "burning_hands",
        "charm_person",
        "detect_magic", // magic-user
        "enlarge_reduce",
        "enlarge_reduce", // Reduce
        "friends",
        "magic_missile",
        "protection_from_evil_and_good",
        "protection_from_evil_and_good",
        "read_magic",
        "shield",
        "shocking_grasp",
        "sleep",
        "find_traps",
        "hold_person", // cleric
        "protection_from_energy", // Resist Fire
        "silence",
        "protection_from_poison", // Slow Poison
        "snake_charm",
        "spiritual_weapon", // Spiritual Hammer
        "see_invisibility", // Detect Invisibility
        "invisibility",
        "knock",
        "mirror_image",
        "ray_of_enfeeblement",
        "stinking_cloud",
        "enhance_ability", // Strength
        "animate_dead",
        "lesser_restoration", // Cure Blindness
        "blindness",          // Cause Blindness
        "lesser_restoration", // Cure Disease
        "contagion",          // Cause Disease
        "dispel_magic",       // cleric
        "bless",              // Prayer
        "remove_curse",
        "bestow_curse",
        "blink",
        "dispel_magic", // magic-user
        "fireball",
        "haste",
        "hold_person",    // magic-user
        "invisibility@3", // Invisibility, 10' Radius: two creatures
        "lightning_bolt",
        "magic_circle", // Protection From Evil, 10' Radius
        "magic_circle", // Protection From Good, 10' Radius
        "protection_from_normal_missiles",
        "slow",
        "greater_restoration"}; // Restoration
    return number < spells.size() ? spells[number] : std::string_view{};
}

// The original's magic-user and cleric scrolls (MAGIC-6): bytes 60-62 are the
// spells on it, and an arcane scroll is a magic-user's. Empty for another
// record; "por:unsupported" for a scroll with no spell left.
std::string scroll_conversion(const por::ItemRecord &raw)
{
    if (raw.type != magic_user_scroll && raw.type != cleric_scroll)
    {
        return {};
    }
    std::vector<std::string> spells;
    for (const auto number : raw.effect_codes)
    {
        if (const auto spell = scroll_spell(number); !spell.empty())
        {
            spells.emplace_back(spell);
        }
    }
    return spells.empty() ? "por:unsupported:" + std::to_string(raw.type)
                          : rules::scroll_key(raw.type == magic_user_scroll, spells);
}

// The original's rings, bracers, cloak, gauntlets and necklace (MAGIC-4), known
// by their item type or by the effect byte 62 applies when they are readied
// (byte 61 is its argument); empty for any other record.
std::string worn_conversion(const por::ItemRecord &raw)
{
    constexpr std::uint8_t gauntlets_type = 63, fire_resistance_type = 69,
                           cursed_necklace_type = 76, bracers_type = 77,
                           feather_falling_type = 90, cloak_type = 92, protection_type = 93;
    constexpr std::uint8_t ogre_power = 0x83, grants_effect = 0x81, cloak_effect = 0x85;
    constexpr std::uint8_t fire_resistance = 61, displacement = 89;
    const auto [charges, argument, effect] = raw.effect_codes;
    if (effect == ogre_power || raw.type == gauntlets_type)
    {
        return "gauntlets_of_ogre_power";
    }
    if ((effect == grants_effect && argument == fire_resistance) ||
        raw.type == fire_resistance_type)
    {
        return "ring_of_fire_resistance";
    }
    // A second Cloak of Displacement record has another item type.
    if ((effect == cloak_effect && argument == displacement) || raw.type == cloak_type)
    {
        return "cloak_of_displacement";
    }
    switch (raw.type)
    {
    case protection_type:
        return rules::item_key("ring_of_protection", raw.magic_bonus);
    case bracers_type:
        return rules::item_key("bracers", raw.magic_bonus);
    case feather_falling_type:
        return "ring_of_feather_falling";
    case cursed_necklace_type:
        return rules::item_key("necklace", raw.magic_bonus,
                               raw.cursed_raw ? rules::ItemPower::cursed : rules::ItemPower::none);
    default:
        return {};
    }
}

bool has_effects(const por::ItemRecord &raw)
{
    return std::any_of(raw.effect_codes.begin(), raw.effect_codes.end(),
                       [](auto n)
                       {
                           return n != 0;
                       });
}
} // namespace

// The key the rules see for an item: it says so when the item's magic is not
// yet identified. A charged item's uses are known either way.
std::string rules_definition(const PartyMember &member, const InventoryItem &item)
{
    auto definition = item.definition_id;
    const auto enchantment = rules::parse_item_key(item.definition_id);
    const auto source = member.item_sources.find(item.id);
    if (source == member.item_sources.end() || source->second.identified())
    {
        return definition;
    }
    const auto &raw = source->second.stored;
    const bool worn_magic = por::is_magic(raw) && !raw.effect_codes[0];
    const bool scroll = rules::parse_scroll_key(item.definition_id).has_value();
    if (enchantment.bonus || enchantment.power != rules::ItemPower::none || worn_magic || scroll)
    {
        definition += ":unidentified";
    }
    return definition;
}

unsigned carried_quantity(const por::Equipment &item)
{
    // A stack counts its items; a single charged item, its charges or doses.
    const unsigned stack = item.stored.stack_size;
    return stack > 1                                   ? stack
           : charged_conversion(item.stored).empty() ? 1u
                                                     : item.stored.effect_codes[0];
}

bool identify_item(PartyMember &member, std::uint64_t item)
{
    if (!member.character.inventory().find(item))
    {
        throw std::runtime_error("Unknown item");
    }
    const auto source = member.item_sources.find(item);
    if (source == member.item_sources.end() || source->second.identified())
    {
        return false;
    }
    source->second.identify();
    auto inventory = member.character.inventory();
    inventory.rename(item, source->second.label());
    member.character.replace_inventory(std::move(inventory));
    return true;
}

std::string equipment_conversion(const por::Equipment &item)
{
    const auto &raw = item.stored;
    const auto unsupported = "por:unsupported:" + std::to_string(raw.type);
    if (auto scroll = scroll_conversion(raw); !scroll.empty())
    {
        return scroll;
    }
    if (const auto charged = charged_conversion(raw); !charged.empty())
    {
        return std::string(charged);
    }
    if (auto worn = worn_conversion(raw); !worn.empty())
    {
        return worn;
    }
    if (const auto base = enchantable_conversion(raw.type); !base.empty())
    {
        const auto power = item_power(raw);
        // Charges (the Javelin of Lightning) are a power of their own, not an
        // enchantment. Flame Tongue's byte 61 and the bit-0x80 effects in byte
        // 62 are not decoded; the name word says what the weapon does.
        const bool decoded = !has_effects(raw) || power == rules::ItemPower::flame_tongue ||
                             (raw.effect_codes[0] == 0 && (raw.effect_codes[2] & 0x80));
        return decoded ? rules::item_key(base, raw.magic_bonus, power) : unsupported;
    }
    if (raw.magic_bonus || raw.cursed_raw || has_effects(raw))
    {
        return unsupported;
    }
    switch (raw.type)
    {
    case 79:
        return "wand";
    case original_item::flask_of_oil:
        return "oil";
    case authored_item::torch:
        return "torch";
    case authored_item::acid:
        return "acid";
    case authored_item::alchemists_fire:
        return "alchemists_fire";
    case authored_item::magic_ink:
        return "magic_ink";
    default:
        return unsupported;
    }
}

CampaignParty::CampaignParty(std::unique_ptr<rules::RulesModule> rules) : rules_(std::move(rules))
{
    if (!rules_)
    {
        throw std::runtime_error("Party requires a rules module");
    }
}

void CampaignParty::outside_combat() const
{
    if (combat_)
    {
        throw std::runtime_error("Finish combat before changing the party");
    }
}

void CampaignParty::editable() const
{
    outside_combat();
    if (state_.short_rest)
    {
        throw std::runtime_error("Finish Short Rest spending before changing the party");
    }
    if (state_.spell_rest)
    {
        throw std::runtime_error("Finish Long Rest spell choices before changing the party");
    }
    if (state_.training_rest)
    {
        throw std::runtime_error("Finish Long Rest training choices before changing the party");
    }
}

const PartyMember &CampaignParty::member(MemberId id) const
{
    return member_in(state_, id);
}

const PartyMember &CampaignParty::member_in(const PartyState &state, MemberId id)
{
    const auto found = std::find_if(state.roster.begin(), state.roster.end(),
                                    [&](const auto &m)
                                    {
                                        return m.id == id;
                                    });
    if (found == state.roster.end())
    {
        throw std::runtime_error("Unknown party member");
    }
    return *found;
}

PartyMember &CampaignParty::member_in(PartyState &state, MemberId id)
{
    return const_cast<PartyMember &>(member_in(std::as_const(state), id));
}

PartyMember &CampaignParty::edit(MemberId id)
{
    return const_cast<PartyMember &>(std::as_const(*this).member(id));
}

void CampaignParty::select(PartySlot slot)
{
    outside_combat();
    if (slot.index >= 8 || !state_.slots[slot.index])
    {
        throw std::runtime_error("Empty party position");
    }
    state_.selected_slot = slot;
}

MemberId CampaignParty::leader() const
{
    if (state_.leader &&
        std::find(state_.slots.begin(), state_.slots.end(), state_.leader) != state_.slots.end())
    {
        return state_.leader;
    }
    for (const auto id : state_.slots)
    {
        if (id)
        {
            return id;
        }
    }
    return 0;
}

void CampaignParty::make_leader(MemberId id)
{
    if (std::find(state_.slots.begin(), state_.slots.end(), id) == state_.slots.end() || !id)
    {
        throw std::runtime_error("Member is not in party");
    }
    state_.leader = id;
}

void CampaignParty::set_quick(MemberId id)
{
    if (std::find(state_.slots.begin(), state_.slots.end(), id) == state_.slots.end() || !id)
    {
        throw std::runtime_error("Member is not in party");
    }
    edit(id).quick = true;
}

void CampaignParty::take_control()
{
    for (auto &member : state_.roster)
    {
        member.quick = false;
    }
}

void CampaignParty::set_quick_magic(bool on)
{
    state_.quick_magic = on;
}

void CampaignParty::lose(MemberId id)
{
    editable();
    auto &member = member_in(state_, id);
    // remove() refuses before changing anything, so the member is marked dead
    // only once it has left the party (Effective C++ Item 29).
    remove(id);
    member.vitals.hit_points = 0;
    member.vitals.dead = true;
}

MemberId CampaignParty::spokesman() const
{
    const auto conscious = [&](MemberId id)
    {
        const auto &vitals = member(id).vitals;
        return id && !vitals.dead && vitals.hit_points > 0;
    };
    if (const auto first = leader(); first && conscious(first))
    {
        return first;
    }
    for (const auto id : state_.slots)
    {
        if (id && conscious(id))
        {
            return id;
        }
    }
    return 0;
}

void CampaignParty::join(MemberId id, bool npc)
{
    if (std::find(state_.slots.begin(), state_.slots.end(), id) != state_.slots.end())
    {
        throw std::runtime_error("Already in party");
    }
    const auto start = npc ? 6 : 0, end = npc ? 8 : 6;
    for (int i = start; i < end; ++i)
    {
        if (!state_.slots[i])
        {
            state_.slots[i] = id;
            if (!selected())
            {
                state_.selected_slot = PartySlot{static_cast<unsigned>(i)};
            }
            return;
        }
    }
    throw std::runtime_error(npc ? "Both NPC positions are occupied"
                                 : "All six PC positions are occupied");
}

MemberId CampaignParty::add_pc(Character character)
{
    editable();
    if (state_.roster.size() >= 128)
    {
        throw std::runtime_error("Roster is full");
    }
    if (std::none_of(state_.slots.begin(), state_.slots.begin() + 6,
                     [](auto id)
                     {
                         return !id;
                     }))
    {
        throw std::runtime_error("All six PC positions are occupied");
    }
    const auto id = state_.next_id;
    const auto hp = character.sheet().hit_points;
    state_.roster.push_back({id, std::move(character), {}, {hp, false, {}}});
    ++state_.next_id;
    join(id, false);
    return id;
}

MemberId CampaignParty::recruit(std::string source, Character converted, unsigned morale)
{
    editable();
    if (source.empty() || morale > 255)
    {
        throw std::runtime_error("NPC conversion requires a source identity and byte morale");
    }
    const auto found = std::find_if(state_.roster.begin(), state_.roster.end(),
                                    [&](const auto &m)
                                    {
                                        return m.npc_source == source;
                                    });
    if (found != state_.roster.end())
    {
        const auto id = found->id;
        rejoin(id);
        return id;
    }
    if (state_.roster.size() >= 128)
    {
        throw std::runtime_error("Roster is full");
    }
    if (state_.slots[6] && state_.slots[7])
    {
        throw std::runtime_error("Both NPC positions are occupied");
    }
    (void)rules_->character_profile(converted.sheet(),
                                    {}); // Reject unknown conversion before recruitment.
    const auto id = state_.next_id;
    const auto hp = converted.sheet().hit_points;
    state_.roster.push_back({.id = id,
                             .character = std::move(converted),
                             .npc_source = std::move(source),
                             .vitals = {hp, false, {}},
                             .morale = morale});
    ++state_.next_id;
    join(id, true);
    return id;
}

MemberId CampaignParty::create_simulacrum(MemberId caster, MemberId original,
                                          std::string_view action, std::string name)
{
    editable();
    outside_combat();
    if (state_.short_rest || state_.spell_rest || state_.training_rest)
    {
        throw std::runtime_error("Finish the rest's choices before casting Simulacrum");
    }
    for (const auto id : {caster, original})
    {
        if (std::find(state_.slots.begin(), state_.slots.end(), id) == state_.slots.end())
        {
            throw std::runtime_error("Camp actions are for active members");
        }
    }
    const auto actions = camp_actions(caster);
    if (!action.starts_with("simulacrum") || action == "simulacrum_repair" ||
        std::none_of(actions.begin(), actions.end(),
                     [&](const auto &a)
                     {
                         return a.id == action;
                     }))
    {
        throw std::runtime_error("That spell or feature cannot be used now");
    }
    if (state_.roster.size() >= 128)
    {
        throw std::runtime_error("Roster is full");
    }
    auto next = state_;
    // The caster's earlier simulacrum is destroyed.
    const auto source = "simulacrum:" + std::to_string(caster) + ":";
    for (auto &slot : next.slots)
    {
        if (slot && member_in(next, slot).npc_source.starts_with(source))
        {
            auto &old = member_in(next, slot);
            old.vitals.hit_points = 0;
            old.vitals.dead = true;
            if (next.leader == slot)
            {
                next.leader = 0;
            }
            slot = 0;
        }
    }
    const auto position = std::find(next.slots.begin() + 6, next.slots.end(), MemberId{});
    if (position == next.slots.end())
    {
        throw std::runtime_error("Both NPC positions are occupied");
    }
    // Copying oneself passes one state as both, as camp actions do.
    auto &wizard = member_in(next, caster);
    auto &copied = member_in(next, original);
    rules_->use_camp_action(wizard.character.sheet(), wizard.vitals, copied.character.sheet(),
                            caster == original ? wizard.vitals : copied.vitals, action,
                            next.random_state);
    // A copy of the creature, not of what it carries.
    auto copy = copied.character;
    copy.rename(std::move(name));
    copy.replace_inventory({});
    auto vitals = rules_->simulacrum_state(copy.sheet());
    const auto id = next.next_id++;
    next.roster.push_back({.id = id,
                           .character = std::move(copy),
                           .npc_source = source + std::to_string(id),
                           .vitals = std::move(vitals)});
    *position = id;
    if (!next.slots[next.selected_slot.index])
    {
        next.selected_slot = PartySlot{static_cast<unsigned>(position - next.slots.begin())};
    }
    // The casting takes twelve hours.
    elapse(next, std::chrono::hours{12});
    next.short_rest.reset();
    state_ = std::move(next);
    return id;
}

bool CampaignParty::is_simulacrum(MemberId id) const
{
    return member(id).npc_source.starts_with("simulacrum:");
}

std::string CampaignParty::reincarnation_species() const
{
    auto dice = state_.random_state;
    return rules_->reincarnation_species(dice);
}

void CampaignParty::reincarnate(const rules::CharacterRules &creation, MemberId caster,
                                MemberId target, std::string_view action,
                                ReincarnationChoices choices, por::CharacterAppearance appearance)
{
    editable();
    outside_combat();
    if (state_.short_rest || state_.spell_rest || state_.training_rest)
    {
        throw std::runtime_error("Finish the rest's choices before casting Reincarnate");
    }
    for (const auto id : {caster, target})
    {
        if (std::find(state_.slots.begin(), state_.slots.end(), id) == state_.slots.end())
        {
            throw std::runtime_error("Camp actions are for active members");
        }
    }
    const auto actions = camp_actions(caster);
    if (!action.starts_with("reincarnate") || caster == target ||
        std::none_of(actions.begin(), actions.end(),
                     [&](const auto &a)
                     {
                         return a.id == action;
                     }))
    {
        throw std::runtime_error("That spell or feature cannot be used now");
    }
    auto next = state_;
    // Rolled first, so the body is the species reincarnation_species() read.
    const auto species = rules_->reincarnation_species(next.random_state);
    for (const auto &[group, picks] : choices.training)
    {
        if (!group.starts_with("species:" + species + ":") || group.ends_with(":kept"))
        {
            throw std::runtime_error("A choice that is not the new species'");
        }
    }
    for (const auto &[group, picks] : choices.initiate_spells)
    {
        if (!group.starts_with("feat:magic_initiate_"))
        {
            throw std::runtime_error("A spell choice that is not Magic Initiate's");
        }
    }
    auto &druid = member_in(next, caster);
    auto &body = member_in(next, target);
    const auto gold = rules_->camp_action_gold(body.character.sheet(), body.vitals, action);
    if (gold > coins(next.wealth, Coin::gold))
    {
        throw std::runtime_error("The party purse cannot pay the offering");
    }
    rules_->use_camp_action(druid.character.sheet(), druid.vitals, body.character.sheet(),
                            body.vitals, action, next.random_state);
    coins(next.wealth, Coin::gold) -= gold;
    // The old species' traits and choices go; the rest of the character stays.
    auto draft = reincarnation_draft(body.character, species);
    draft.training.merge(choices.training);
    if (!choices.initiate_spells.empty())
    {
        if (!draft.spells)
        {
            draft.spells.emplace();
        }
        for (auto &[group, picks] : choices.initiate_spells)
        {
            if (!draft.spells->learning.emplace(group, std::move(picks)).second)
            {
                throw std::runtime_error("A spell choice the character already made");
            }
        }
    }
    const auto before = body.character.sheet();
    body.character = body.character.recreated(creation, *rules_, std::move(draft));
    body.character.appearance(std::move(appearance));
    const auto &after = body.character.sheet();
    body.vitals = rules_->fitted_state(before, body.vitals, after);
    rules_->set_hit_points(body.vitals, after, rules_->hit_point_maximum(after, body.vitals));
    validate(next);
    state_ = std::move(next);
}

void CampaignParty::end_combat() noexcept
{
    combat_ = false;
    for (auto &slot : state_.slots)
    {
        for (const auto &m : state_.roster)
        {
            if (slot && m.id == slot && m.vitals.dead && m.npc_source.starts_with("simulacrum:"))
            {
                if (state_.leader == slot)
                {
                    state_.leader = 0;
                }
                slot = 0;
            }
        }
    }
    if (state_.slots[state_.selected_slot.index])
    {
        return;
    }
    for (unsigned i = 0; i < 8; ++i)
    {
        if (state_.slots[i])
        {
            state_.selected_slot = PartySlot{i};
            return;
        }
    }
}

void CampaignParty::rejoin(MemberId id)
{
    editable();
    join(id, !member(id).npc_source.empty());
}

void CampaignParty::remove(MemberId id)
{
    editable();
    (void)member(id);
    auto it = std::find(state_.slots.begin(), state_.slots.end(), id);
    if (it == state_.slots.end())
    {
        throw std::runtime_error("Member is not in party");
    }
    *it = 0;
    if (state_.leader == id)
    {
        state_.leader = 0;
    }
    if (!selected())
    {
        for (unsigned i = 0; i < 8; ++i)
        {
            if (state_.slots[i])
            {
                state_.selected_slot = PartySlot{i};
                break;
            }
        }
    }
}

rules::RecoveryInfo CampaignParty::recovery_info(MemberId id) const
{
    const auto &who = member(id);
    return rules_->recovery_info(who.character.sheet(), who.vitals);
}

rules::CharacterProfile CampaignParty::profile(MemberId id) const
{
    const auto &m = member(id);
    return rules_->character_profile(m.character.sheet(), equipped_definitions(m));
}

rules::AbilityCheckModifier CampaignParty::ability_check(MemberId id, rules::Ability ability,
                                                         std::string_view skill) const
{
    const auto &m = member(id);
    return rules_->ability_check(m.character.sheet(), equipped_definitions(m), ability, skill);
}

std::vector<rules::EquipmentChoice> CampaignParty::equipment_choices(MemberId id,
                                                                     std::uint64_t item) const
{
    const auto &m = member(id);
    const auto selected = m.character.inventory().find(item);
    if (!selected)
    {
        throw std::runtime_error("Unknown item");
    }
    if (selected->get().quantity == 1 &&
        std::find(m.equipped.begin(), m.equipped.end(), item) != m.equipped.end())
    {
        return {};
    }
    auto keys = equipped_definitions(m);
    keys.push_back(selected->get().definition_id);
    return rules_->equipment_choices(m.character.sheet(), keys,
                                     static_cast<unsigned>(keys.size() - 1));
}

void CampaignParty::equip(MemberId id, std::uint64_t item, rules::EquipmentOperation operation)
{
    if (operation == rules::EquipmentOperation::unequip)
    {
        throw std::runtime_error("Invalid equip operation");
    }
    change_equipment(id, item, operation);
}

void CampaignParty::unequip(MemberId id, std::uint64_t item)
{
    change_equipment(id, item, rules::EquipmentOperation::unequip);
}

void CampaignParty::change_equipment(MemberId id, std::uint64_t item,
                                     rules::EquipmentOperation operation)
{
    editable();
    const auto &before = member(id);
    auto candidates = before.equipped;
    const auto found = std::find(candidates.begin(), candidates.end(), item);
    if (operation == rules::EquipmentOperation::equip && found != candidates.end())
    {
        return;
    }
    if (operation == rules::EquipmentOperation::unequip && found == candidates.end())
    {
        return;
    }
    const unsigned selected = operation != rules::EquipmentOperation::unequip
                                  ? static_cast<unsigned>(candidates.size())
                                  : static_cast<unsigned>(found - candidates.begin());
    if (operation != rules::EquipmentOperation::unequip)
    {
        candidates.push_back(item);
    }
    std::vector<std::string> keys;
    for (auto id : candidates)
    {
        const auto entry = before.character.inventory().find(id);
        if (!entry)
        {
            throw std::runtime_error("Unknown item");
        }
        keys.push_back(entry->get().definition_id);
    }
    const auto plan = rules_->equipment_change(before.character.sheet(), keys, selected, operation);
    auto inventory = before.character.inventory();
    auto sources = before.item_sources;
    bool inventory_changed = false;
    if (plan.separate_selected_unit)
    {
        const auto unit = inventory.find(item)->get();
        if (unit.quantity > 1)
        {
            inventory_changed = true;
            inventory.remove(item, 1);
            candidates[selected] = inventory.add({.definition_id = unit.definition_id,
                                                  .name = unit.name,
                                                  .quantity = 1,
                                                  .original_type = unit.original_type});
            if (const auto source = sources.find(item); source != sources.end())
            {
                sources.emplace(candidates[selected], source->second);
            }
        }
    }
    std::vector<std::uint64_t> next;
    next.reserve(plan.indices.size());
    for (auto index : plan.indices)
    {
        if (index >= candidates.size() ||
            std::find(next.begin(), next.end(), candidates[index]) != next.end())
        {
            throw std::runtime_error("Invalid equipment result from rules module");
        }
        next.push_back(candidates[index]);
    }
    // A cursed item cannot be put down until Remove Curse is cast on its
    // wielder (MAGIC-1).
    for (const auto worn : before.equipped)
    {
        const auto &definition = before.character.inventory().find(worn)->get().definition_id;
        if (std::find(next.begin(), next.end(), worn) == next.end() &&
            rules::parse_item_key(definition).power == rules::ItemPower::cursed)
        {
            throw std::runtime_error("This item is cursed. It cannot be unequipped until Remove "
                                     "Curse is cast on its wielder.");
        }
    }
    auto &target = edit(id);
    if (inventory_changed)
    {
        target.character.replace_inventory(std::move(inventory));
        target.item_sources = std::move(sources);
    }
    target.equipped = std::move(next);
}

rules::EquipmentInfo CampaignParty::equipment_info(MemberId id, std::uint64_t item) const
{
    const auto found = member(id).character.inventory().find(item);
    if (!found)
    {
        throw std::runtime_error("Unknown item");
    }
    return rules_->equipment_info(found->get().definition_id);
}

void CampaignParty::purchase(MemberId id, const por::Equipment &item)
{
    editable();
    auto &m = edit(id);
    if (m.character.inventory().items().size() >= 16)
    {
        throw std::runtime_error("Inventory is full (16 items)");
    }
    if (coins(state_.wealth, Coin::gold) < item.stored.value)
    {
        throw std::runtime_error("Not enough gold");
    }
    auto inventory = m.character.inventory();
    auto sources = m.item_sources;
    const auto key =
        inventory.add({.definition_id = equipment_conversion(item),
                       .name = item.label(),
                       .quantity = carried_quantity(item),
                       .original_type = item.stored.type});
    sources.emplace(key, item);
    m.character.replace_inventory(std::move(inventory));
    m.item_sources = std::move(sources);
    coins(state_.wealth, Coin::gold) -= item.stored.value;
}

void CampaignParty::read_manual(MemberId id, std::uint64_t item)
{
    editable();
    outside_combat();
    auto reader = member(id);
    const auto book = reader.character.inventory().find(item);
    if (!book || book->get().definition_id != "manual_of_bodily_health")
    {
        throw std::runtime_error("That is not a Manual of Bodily Health");
    }
    if (reader.vitals.dead)
    {
        throw std::runtime_error("The dead cannot read");
    }
    const int before = reader.character.sheet().hit_points;
    // Recorded with the hall training, so a save rebuilds it at its level.
    reader.character.train_at_hall(*rules_, "boon:bodily_health");
    reader.vitals.hit_points += reader.character.sheet().hit_points - before;
    reader.character.remove_item(item);
    reader.item_sources.erase(item);
    std::erase(reader.equipped, item);
    edit(id) = std::move(reader);
}

void CampaignParty::drink_potion(MemberId id, std::uint64_t item)
{
    editable();
    outside_combat();
    auto next = state_;
    auto &drinker = member_in(next, id);
    const auto potion = drinker.character.inventory().find(item);
    if (!potion)
    {
        throw std::runtime_error("Unknown item");
    }
    const auto left = potion->get().quantity - 1;
    rules_->drink_potion(drinker.character.sheet(), drinker.vitals, potion->get().definition_id,
                         next.random_state);
    keep_item_uses(drinker, item, left);
    state_ = std::move(next);
}

bool CampaignParty::identify_at_shop(MemberId id, std::uint64_t item)
{
    constexpr unsigned identify_fee = 200;
    editable();
    if (coins(state_.wealth, Coin::gold) < identify_fee)
    {
        throw std::runtime_error("Not enough gold");
    }
    auto holder = member(id);
    const bool revealed = identify_item(holder, item);
    edit(id) = std::move(holder);
    coins(state_.wealth, Coin::gold) -= identify_fee;
    return revealed;
}

void CampaignParty::set_wealth(Purse wealth)
{
    editable();
    state_.wealth = wealth;
}

void CampaignParty::add_coins(Coin coin, std::uint32_t count)
{
    editable();
    auto &held = coins(state_.wealth, coin);
    if (count > purse_limit - held)
    {
        throw std::runtime_error("The party purse cannot hold more coins");
    }
    held += count;
}

bool CampaignParty::award_loot(const std::array<unsigned, 7> &wealth,
                               const std::vector<por::Equipment> &items, std::string reward_id)
{
    editable();
    if (reward_id.empty() || reward_id.size() > 160)
    {
        throw std::runtime_error("Loot requires a bounded stable identity");
    }
    if (std::find(state_.claimed_rewards.begin(), state_.claimed_rewards.end(), reward_id) !=
        state_.claimed_rewards.end())
    {
        return true;
    }
    if (state_.claimed_rewards.size() >= 1024 || items.size() > 256)
    {
        throw std::runtime_error("Loot collection exceeds supported limits");
    }
    auto next = state_;
    std::vector<std::size_t> recipients;
    for (auto id : next.slots)
    {
        if (id)
        {
            const auto &member = member_in(next, id);
            if (!member.vitals.dead)
            {
                recipients.push_back(static_cast<std::size_t>(&member - next.roster.data()));
            }
        }
    }
    if (recipients.empty())
    {
        return false;
    }
    for (unsigned coin = 0; coin < 7; ++coin)
    {
        auto &purse = next.wealth[coin];
        if (wealth[coin] > purse_limit - purse)
        {
            return false;
        }
        purse += wealth[coin];
    }
    for (const auto &item : items)
    {
        const auto recipient =
            *std::min_element(recipients.begin(), recipients.end(),
                              [&](auto a, auto b)
                              {
                                  return next.roster[a].character.inventory().items().size() <
                                         next.roster[b].character.inventory().items().size();
                              });
        auto &m = next.roster[recipient];
        // Encounter rewards are retained even beyond the shop's purchase cap.
        const auto id = m.character.add_item(
            {.definition_id = equipment_conversion(item),
             .name = item.label(),
             .quantity = carried_quantity(item),
             .original_type = item.stored.type});
        m.item_sources.emplace(id, item);
    }
    next.claimed_rewards.push_back(std::move(reward_id));
    state_ = std::move(next);
    return true;
}

void CampaignParty::award_experience(unsigned amount, std::string reward_id)
{
    editable();
    if (reward_id.empty() || reward_id.size() > 160)
    {
        throw std::runtime_error("Reward requires a bounded stable identity");
    }
    if (std::find(state_.claimed_rewards.begin(), state_.claimed_rewards.end(), reward_id) !=
        state_.claimed_rewards.end())
    {
        return;
    }
    if (state_.claimed_rewards.size() >= 1024)
    {
        throw std::runtime_error("Reward history is full");
    }
    auto next = state_;
    bool awarded = false;
    for (auto id : next.slots)
    {
        if (id)
        {
            auto &member = member_in(next, id);
            if (member.vitals.dead)
            {
                continue;
            }
            if (amount > std::numeric_limits<unsigned>::max() - member.experience)
            {
                throw std::runtime_error("Experience overflow");
            }
            awarded = true;
            member.experience += amount;
        }
    }
    if (!awarded)
    {
        throw std::runtime_error("Reward requires a living active member");
    }
    next.claimed_rewards.push_back(std::move(reward_id));
    state_ = std::move(next);
}

ExperienceOffer CampaignParty::experience_offer(MemberId id, TrainingHall hall) const
{
    const auto &m = member(id);
    const auto next_level = this->next_level(m);
    ExperienceOffer offer;
    if (next_level)
    {
        const auto level = next_level - 1;
        offer.fee = experience_session_fee(level);
        offer.experience =
            (rules_->experience_for_level(next_level) - rules_->experience_for_level(level)) / 2;
    }
    const auto active =
        std::find(state_.slots.begin(), state_.slots.end(), id) != state_.slots.end();
    if (training_hall_for_class(m.character.creation_data().character_class) != hall)
    {
        offer.denial = TrainingDenial::other_hall;
    }
    else if (!active || m.vitals.dead)
    {
        offer.denial = TrainingDenial::not_active;
    }
    else if (!next_level)
    {
        offer.denial = TrainingDenial::highest_level;
    }
    else if (m.experience >= rules_->experience_for_level(next_level))
    {
        offer.denial = TrainingDenial::ready_to_advance;
    }
    else if (m.hall_experience_level == next_level - 1)
    {
        offer.denial = TrainingDenial::already_trained;
    }
    else if (coins(state_.wealth, Coin::gold) < offer.fee)
    {
        offer.denial = TrainingDenial::gold;
    }
    return offer;
}

void CampaignParty::buy_experience(MemberId id, TrainingHall hall)
{
    editable();
    outside_combat();
    const auto offer = experience_offer(id, hall);
    if (offer.denial != TrainingDenial::none)
    {
        throw std::runtime_error("The training hall cannot sell this member experience");
    }
    auto &m = edit(id);
    m.experience += offer.experience;
    m.hall_experience_level = rules_->advancement_options(m.character.sheet()).level - 1;
    coins(state_.wealth, Coin::gold) -= offer.fee;
}

TrainingDenial CampaignParty::skill_training_denial(MemberId id, TrainingHall hall) const
{
    const auto &m = member(id);
    const auto active =
        std::find(state_.slots.begin(), state_.slots.end(), id) != state_.slots.end();
    const auto &trained = m.character.hall_training();
    if (training_hall_for_class(m.character.creation_data().character_class) != hall)
    {
        return TrainingDenial::other_hall;
    }
    if (!active || m.vitals.dead)
    {
        return TrainingDenial::not_active;
    }
    // A Manual of Bodily Health is recorded with the hall training, but is none.
    const auto last_trained = std::find_if(trained.rbegin(), trained.rend(),
                                           [](const auto &entry)
                                           {
                                               return !entry.grant.starts_with("boon:");
                                           });
    if (last_trained != trained.rend() &&
        last_trained->level == static_cast<unsigned>(m.character.sheet().level))
    {
        return TrainingDenial::already_trained;
    }
    if (coins(state_.wealth, Coin::gold) < proficiency_training_fee)
    {
        return TrainingDenial::gold;
    }
    return TrainingDenial::none;
}

rules::TrainingChoiceGroup CampaignParty::skill_lessons(MemberId id, TrainingHall hall) const
{
    std::vector<std::string> classes;
    for (const auto klass : hall_classes(hall))
    {
        classes.emplace_back(klass);
    }
    return rules_->hall_training_options(member(id).character.sheet(), classes);
}

void CampaignParty::train_skill(MemberId id, TrainingHall hall, const std::string &grant)
{
    editable();
    const auto lessons = skill_lessons(id, hall).options;
    const auto fee = skill_training_fee(grant);
    if (skill_training_denial(id, hall) != TrainingDenial::none ||
        std::none_of(lessons.begin(), lessons.end(),
                     [&](const auto &lesson)
                     {
                         return lesson.id == grant;
                     }) ||
        coins(state_.wealth, Coin::gold) < fee)
    {
        throw std::runtime_error("The training hall cannot teach this lesson");
    }
    auto character = member(id).character;
    character.train_at_hall(*rules_, grant);
    edit(id).character = std::move(character);
    coins(state_.wealth, Coin::gold) -= fee;
}

bool CampaignParty::can_advance(MemberId id) const
{
    if (combat_ || state_.short_rest || state_.spell_rest || state_.training_rest)
    {
        return false;
    }
    const auto &m = member(id);
    const auto level = next_level(m);
    // A simulacrum gains no levels.
    return !m.vitals.dead && level && !is_simulacrum(id) &&
           m.experience >= rules_->experience_for_level(level);
}

unsigned CampaignParty::next_level(const PartyMember &m) const
{
    const auto level = rules_->advancement_options(m.character.sheet()).level;
    const auto hall = training_hall_for_class(m.character.creation_data().character_class);
    return hall && level > hall_level_limit ? 0 : level;
}

rules::AdvancementOptions
CampaignParty::advancement_options(MemberId id, const rules::AdvancementChoice &choice) const
{
    return rules_->advancement_options(member(id).character.sheet(), choice);
}

rules::AdvancementChoice CampaignParty::default_advancement(MemberId id) const
{
    return rules_->default_advancement(member(id).character.sheet());
}

PartyMember CampaignParty::preview_advancement(MemberId id,
                                               const rules::AdvancementChoice &choice) const
{
    if (!can_advance(id))
    {
        throw std::runtime_error("This character is not ready to level up");
    }
    auto next = member(id);
    const auto level = next.character.sheet().level;
    if (rules_->default_advancement(next.character.sheet()).spell_learning &&
        !choice.spell_learning)
    {
        throw std::runtime_error("Independent spell learning choices are required");
    }
    for (const auto &group : rules_->advancement_options(next.character.sheet()).training)
    {
        const auto found = choice.training.find(group.id);
        if (found == choice.training.end() || found->second.size() != group.count)
        {
            throw std::runtime_error("Complete required advancement training");
        }
    }
    if (!next.character.advance(*rules_, next.vitals, choice) ||
        next.character.sheet().level != level + 1)
    {
        throw std::runtime_error("Unsupported advancement");
    }
    return next;
}

void CampaignParty::advance(MemberId id, const rules::AdvancementChoice &choice)
{
    editable();
    auto member = preview_advancement(id, choice);
    // The preview did everything that can throw; installing it cannot, so
    // the party is not copied to commit it (Effective C++ Item 29).
    edit(id) = std::move(member);
}

rules::SpellChoiceOptions CampaignParty::spell_choice_options(MemberId id) const
{
    return rules_->spell_choice_options(member(id).character.sheet(), rest_spell_context());
}

rules::SpellChoiceContext CampaignParty::rest_spell_context() const
{
    return state_.spells_after_short_rest ? rules::SpellChoiceContext::short_rest
                                          : rules::SpellChoiceContext::long_rest;
}

PartyMember CampaignParty::preview_spell_choices(MemberId id,
                                                 const rules::SpellChoices &choices) const
{
    outside_combat();
    if (state_.short_rest)
    {
        throw std::runtime_error("Finish resting before spell choices");
    }
    if (!state_.spell_rest || state_.spell_rest->completed_minutes != state_.time_minutes ||
        state_.spell_rest->completed_subminute_milliseconds != state_.subminute_milliseconds ||
        std::find(state_.spell_rest->members.begin(), state_.spell_rest->members.end(), id) ==
            state_.spell_rest->members.end())
    {
        throw std::runtime_error("No completed Long Rest spell choices");
    }
    auto candidate = member(id);
    candidate.character.choose_spells(*rules_, choices, state_.spell_rest->ticket.session,
                                      rest_spell_context());
    rules_->validate_character_state(candidate.character.sheet(), candidate.vitals);
    return candidate;
}

void CampaignParty::choose_spells(MemberId id, const rules::SpellChoices &choices)
{
    auto candidate = preview_spell_choices(id, choices);
    // The preview did everything that can throw; what is left cannot.
    edit(id) = std::move(candidate);
    std::erase(state_.spell_rest->members, id);
    if (state_.spell_rest->members.empty())
    {
        state_.spell_rest.reset();
    }
}

void CampaignParty::keep_rest_spells(MemberId id)
{
    outside_combat();
    if (!state_.spell_rest ||
        std::find(state_.spell_rest->members.begin(), state_.spell_rest->members.end(), id) ==
            state_.spell_rest->members.end())
    {
        throw std::runtime_error("No Long Rest spell choice to decline");
    }
    std::erase(state_.spell_rest->members, id);
    if (state_.spell_rest->members.empty())
    {
        state_.spell_rest.reset();
    }
}

PartyMember CampaignParty::preview_rest_training(RestTicket ticket, MemberId id,
                                                 std::span<const std::string> selections) const
{
    outside_combat();
    const auto &rest = state_.training_rest;
    if (state_.spell_rest || state_.short_rest || !rest || rest->ticket != ticket ||
        rest->completed_minutes != state_.time_minutes ||
        rest->completed_subminute_milliseconds != state_.subminute_milliseconds ||
        std::find(rest->members.begin(), rest->members.end(), id) == rest->members.end())
    {
        throw std::runtime_error("No completed Long Rest training choice");
    }
    // A Short Rest changes only some choices; the rules accept any of them.
    const auto offered = rules_->rest_training_options(
        member(id).character.sheet(),
        state_.training_after_short_rest ? RestKind::short_rest : RestKind::long_rest);
    for (const auto &pick : selections)
    {
        if (std::none_of(offered.begin(), offered.end(),
                         [&](const auto &choice)
                         {
                             return std::any_of(choice.group.options.begin(),
                                                choice.group.options.end(),
                                                [&](const auto &option)
                                                {
                                                    return option.id == pick;
                                                });
                         }))
        {
            throw std::runtime_error("Not a choice this rest can change");
        }
    }
    auto candidate = member(id);
    candidate.character.replace_rest_training(*rules_, selections, ticket.session);
    rules_->validate_character_state(candidate.character.sheet(), candidate.vitals);
    return candidate;
}

void CampaignParty::replace_rest_training(RestTicket ticket, MemberId id,
                                          std::span<const std::string> selections)
{
    auto candidate = preview_rest_training(ticket, id, selections);
    // The preview did everything that can throw; what is left cannot.
    edit(id) = std::move(candidate);
    std::erase(state_.training_rest->members, id);
    if (state_.training_rest->members.empty())
    {
        state_.training_rest.reset();
    }
}

void CampaignParty::keep_rest_training(RestTicket ticket, MemberId id)
{
    // Keeping the current legal set consumes exactly the same entitlement.
    const auto options = rules_->rest_training_options(
        member(id).character.sheet(),
        state_.training_after_short_rest ? RestKind::short_rest : RestKind::long_rest);
    if (options.empty())
    {
        throw std::runtime_error("No Long Rest training choice to decline");
    }
    std::vector<std::string> current;
    for (const auto &choice : options)
    {
        current.insert(current.end(), choice.selected.begin(), choice.selected.end());
    }
    replace_rest_training(ticket, id, current);
}

void CampaignParty::advance_time(std::chrono::milliseconds elapsed)
{
    outside_combat();
    const bool passes = elapsed != std::chrono::milliseconds::zero();
    if ((state_.spell_rest || state_.training_rest) && passes)
    {
        throw std::runtime_error("Finish Long Rest spell choices before advancing time");
    }
    // Everything that can throw comes first; then only moves.
    apply(state_, this->elapsed(state_, elapsed));
    if (passes)
    {
        state_.short_rest.reset();
    }
}

void CampaignParty::elapse(PartyState &state, std::chrono::milliseconds elapsed,
                           std::span<const MemberId> in_combat) const
{
    apply(state, this->elapsed(state, elapsed, in_combat));
}

CampaignParty::Elapsed CampaignParty::elapsed(const PartyState &state,
                                              std::chrono::milliseconds elapsed,
                                              std::span<const MemberId> in_combat) const
{
    if (elapsed < std::chrono::milliseconds::zero())
    {
        throw std::runtime_error("Time cannot run backwards");
    }
    const auto milliseconds = static_cast<std::uint64_t>(elapsed.count());
    const auto remainder = state.subminute_milliseconds + milliseconds % 60000;
    const auto minutes = milliseconds / 60000 + remainder / 60000;
    if (minutes > std::numeric_limits<std::uint64_t>::max() - state.time_minutes)
    {
        throw std::runtime_error("Campaign clock overflow");
    }
    Elapsed result{{},
                   state.random_state,
                   state.time_minutes + minutes,
                   static_cast<unsigned>(remainder % 60000)};
    std::vector<rules::Participant> participants;
    for (const auto &member : state.roster)
    {
        if (std::find(in_combat.begin(), in_combat.end(), member.id) != in_combat.end())
        {
            continue;
        }
        auto profile =
            rules_->character_profile(member.character.sheet(), equipped_definitions(member));
        participants.push_back({member.id,
                                "campaign-character",
                                member.character.sheet().name,
                                rules::Side::party,
                                {},
                                std::move(profile.data),
                                member.vitals});
    }
    rules_->elapse(participants, elapsed, result.random);
    for (auto &participant : participants)
    {
        result.vitals.emplace_back(participant.id, std::move(*participant.state));
    }
    return result;
}

void CampaignParty::apply(PartyState &state, Elapsed &&elapsed) noexcept
{
    for (auto &[id, vitals] : elapsed.vitals)
    {
        for (auto &member : state.roster)
        {
            if (member.id == id)
            {
                member.vitals = std::move(vitals);
            }
        }
    }
    state.random_state = elapsed.random;
    state.time_minutes = elapsed.time_minutes;
    state.subminute_milliseconds = elapsed.subminute_milliseconds;
}

namespace
{
bool tries_door(const PartyMember &m, DoorMethod method)
{
    const bool conscious = !m.vitals.dead && m.vitals.hit_points > 0;
    return conscious &&
           (method == DoorMethod::bash || m.character.creation_data().character_class == "rogue");
}
} // namespace

bool CampaignParty::can_try_door(DoorMethod method) const
{
    return std::any_of(state_.slots.begin(), state_.slots.end(),
                       [&](MemberId id)
                       {
                           if (!id)
                           {
                               return false;
                           }
                           const auto &m = member(id);
                           return method == DoorMethod::knock
                                      ? rules_->can_cast_exploration_spell(m.character.sheet(),
                                                                           m.vitals, "knock")
                                      : tries_door(m, method);
                       });
}

std::vector<DoorAttempt> CampaignParty::try_door(DoorMethod method, int difficulty)
{
    editable();
    if (method == DoorMethod::knock)
    {
        return cast_knock(difficulty);
    }
    std::vector<DoorAttempt> attempts;
    auto random_state = state_.random_state;
    for (auto id : state_.slots)
    {
        if (!id)
        {
            continue;
        }
        const auto &m = member(id);
        if (!tries_door(m, method))
        {
            continue;
        }
        const auto gear = equipped_definitions(m);

        const auto roll =
            method == DoorMethod::bash
                ? rules_->roll_ability_check(m.character.sheet(), gear, rules::Ability::strength,
                                             "athletics", random_state)
                : rules_->roll_ability_check(m.character.sheet(), gear, rules::Ability::dexterity,
                                             "sleight_of_hand", random_state);
        attempts.push_back({id, roll});
        if (roll.total >= difficulty)
        {
            break;
        }
    }
    state_.random_state = random_state;
    return attempts;
}

std::vector<DoorAttempt> CampaignParty::cast_knock(int difficulty)
{
    // The first active member able to cast it does; the lock always opens.
    auto next = state_;
    for (const auto id : next.slots)
    {
        if (!id)
        {
            continue;
        }
        auto &caster = member_in(next, id);
        if (!rules_->can_cast_exploration_spell(caster.character.sheet(), caster.vitals, "knock"))
        {
            continue;
        }
        rules_->cast_exploration_spell(caster.character.sheet(), caster.vitals, "knock");
        state_ = std::move(next);
        return {{id, {0, difficulty}}};
    }
    throw std::runtime_error("No member can cast Knock");
}

void CampaignParty::temple_heal(MemberId target)
{
    editable();
    const auto &current = member(target);
    if (std::find(state_.slots.begin(), state_.slots.end(), target) == state_.slots.end())
    {
        throw std::runtime_error("Temple target must be active");
    }
    if (current.vitals.dead || current.vitals.hit_points >= hit_point_maximum(target))
    {
        throw std::runtime_error("Cure Wounds requires a wounded living member");
    }
    constexpr std::uint32_t temple_fee = 100;
    if (coins(state_.wealth, Coin::gold) < temple_fee)
    {
        throw std::runtime_error("Party cannot afford temple service");
    }
    auto next = state_;
    auto &healed = member_in(next, target);
    coins(next.wealth, Coin::gold) -= temple_fee;
    rules_->temple_heal(healed.vitals, healed.character.sheet(), next.random_state);
    state_ = std::move(next);
}

std::optional<HazardHit> CampaignParty::hazard_attack(const rules::HazardAttack &attack)
{
    editable();
    std::vector<MemberId> conscious;
    for (const auto id : state_.slots)
    {
        if (id && member(id).vitals.hit_points > 0)
        {
            conscious.push_back(id);
        }
    }
    if (conscious.empty())
    {
        return std::nullopt;
    }
    auto next = state_;
    const auto target =
        conscious[roll_die(next.random_state, static_cast<int>(conscious.size())) - 1];
    auto &struck = member_in(next, target);
    HazardHit hit{target,
                  rules_->hazard_attack(struck.vitals, struck.character.sheet(),
                                        equipped_definitions(struck), attack, next.random_state)};
    state_ = std::move(next);
    return hit;
}

int CampaignParty::hit_point_maximum(MemberId id) const
{
    const auto &m = member(id);
    return rules_->hit_point_maximum(m.character.sheet(), m.vitals);
}

std::vector<rules::StatusEffectView> CampaignParty::status_effects(MemberId id) const
{
    const auto &m = member(id);
    return rules_->status_effects(m.character.sheet(), m.vitals);
}

std::vector<rules::CampAction> CampaignParty::camp_actions(MemberId id) const
{
    const auto &m = member(id);
    if (std::find(state_.slots.begin(), state_.slots.end(), id) == state_.slots.end())
    {
        return {};
    }
    auto actions = rules_->camp_actions(m.character.sheet(), m.vitals);
    std::erase_if(actions,
                  [](const auto &action)
                  {
                      return action.id.starts_with("identify");
                  });
    return actions;
}

std::optional<MemberId> CampaignParty::identifier() const
{
    for (const auto id : state_.slots)
    {
        if (!id)
        {
            continue;
        }
        const auto &m = member(id);
        const auto actions = rules_->camp_actions(m.character.sheet(), m.vitals);
        if (std::any_of(actions.begin(), actions.end(),
                        [](const auto &action)
                        {
                            return action.id.starts_with("identify");
                        }))
        {
            return id;
        }
    }
    return std::nullopt;
}

bool CampaignParty::cast_identify(MemberId caster_id, MemberId holder_id, std::uint64_t item)
{
    editable();
    for (const auto id : {caster_id, holder_id})
    {
        if (std::find(state_.slots.begin(), state_.slots.end(), id) == state_.slots.end())
        {
            throw std::runtime_error("Identify is for active members");
        }
    }
    auto next = state_;
    auto &caster = member_in(next, caster_id);
    // The lowest slot first, as the rules offer them.
    const auto actions = rules_->camp_actions(caster.character.sheet(), caster.vitals);
    const auto spell = std::find_if(actions.begin(), actions.end(),
                                    [](const auto &action)
                                    {
                                        return action.id.starts_with("identify");
                                    });
    if (spell == actions.end())
    {
        throw std::runtime_error("That spell or feature cannot be used now");
    }
    auto &holder = member_in(next, holder_id);
    rules_->use_camp_action(caster.character.sheet(), caster.vitals, holder.character.sheet(),
                            holder.vitals, spell->id, next.random_state);
    const bool revealed = identify_item(holder, item);
    state_ = std::move(next);
    return revealed;
}

bool CampaignParty::magical(MemberId id, std::uint64_t item) const
{
    const auto &sources = member(id).item_sources;
    const auto source = sources.find(item);
    return source != sources.end() && por::is_magic(source->second.stored);
}

std::vector<rules::CampAction> CampaignParty::scroll_actions(MemberId id, std::uint64_t item) const
{
    const auto &reader = member(id);
    const auto carried = reader.character.inventory().find(item);
    if (!carried || !rules::parse_scroll_key(carried->get().definition_id))
    {
        return {};
    }
    return rules_->scroll_actions(reader.character.sheet(), reader.vitals,
                                  rules_definition(reader, carried->get()));
}

namespace
{
// What a spell cast outside combat leaves to the campaign, for the spells
// Detect Magic, Friends, Read Magic, Find Traps and Remove Curse (MAGIC-2,
// MAGIC-6).
void keep_camp_spell(PartyState &state, PartyMember &caster, PartyMember &patient,
                     std::string_view verb)
{
    if (verb.starts_with("detect_magic"))
    {
        constexpr std::uint64_t detect_magic_minutes = 10;
        state.detect_magic_until = state.time_minutes + detect_magic_minutes;
    }
    // Friends, as the original's from a scroll, which casts at the 6th level:
    // 2d4 more Charisma for six minutes, to at most 25.
    if (verb == "friends")
    {
        state.friends_member = caster.id;
        state.friends_bonus = roll_die(state.random_state, 4) + roll_die(state.random_state, 4);
        state.friends_until = state.time_minutes + 6;
    }
    // Read Magic reveals the caster's scrolls, as the original's did when they
    // were opened while it lasted.
    if (verb == "read_magic")
    {
        for (const auto &carried : caster.character.inventory().items())
        {
            if (rules::parse_scroll_key(carried.definition_id))
            {
                (void)identify_item(caster, carried.id);
            }
        }
    }
    // Find Traps lasts 30 minutes, as the original's.
    if (verb == "find_traps")
    {
        state.find_traps_until = state.time_minutes + 30;
    }
    if (verb.starts_with("remove_curse"))
    {
        put_down_cursed_items(patient);
    }
}
} // namespace

bool CampaignParty::read_scroll(MemberId user, std::uint64_t item, MemberId target,
                                std::string_view action)
{
    editable();
    outside_combat();
    for (const auto id : {user, target})
    {
        if (std::find(state_.slots.begin(), state_.slots.end(), id) == state_.slots.end())
        {
            throw std::runtime_error("Camp actions are for active members");
        }
    }
    const auto actions = scroll_actions(user, item);
    const auto chosen = std::find_if(actions.begin(), actions.end(),
                                     [&](const auto &a)
                                     {
                                         return a.id == action;
                                     });
    if (chosen == actions.end())
    {
        throw std::runtime_error("That spell cannot be read from the scroll now");
    }
    auto next = state_;
    auto &reader = member_in(next, user);
    const auto key = rules_definition(reader, reader.character.inventory().find(item)->get());
    auto &patient = member_in(next, chosen->whole_party ? user : target);
    bool cast = false;
    if (chosen->whole_party)
    {
        std::vector<rules::CampTarget> party;
        for (const auto id : next.slots)
        {
            if (id)
            {
                auto &m = member_in(next, id);
                party.push_back({m.character.sheet(), m.vitals});
            }
        }
        cast = rules_->use_party_scroll(reader.character.sheet(), reader.vitals, party, key,
                                        action, next.random_state);
    }
    else
    {
        cast = rules_->use_scroll(reader.character.sheet(), reader.vitals,
                                  patient.character.sheet(), patient.vitals, key, action,
                                  next.random_state);
    }
    if (cast)
    {
        keep_camp_spell(next, reader, patient, action.substr(2));
    }
    // The action's id begins with the spell's place on the scroll.
    const auto spells = rules::parse_scroll_key(key)->spells.size();
    keep_scroll_spells(reader, item, ((1U << spells) - 1) & ~(1U << (action[0] - '0')));
    state_ = std::move(next);
    return cast;
}

std::vector<CampaignParty::ScrollCopy> CampaignParty::copyable_spells(MemberId id,
                                                                      std::uint64_t item) const
{
    std::vector<ScrollCopy> copies;
    const auto &wizard = member(id);
    const auto carried = wizard.character.inventory().find(item);
    // The parsed key's spell names point into `key`.
    const auto key = carried ? rules_definition(wizard, carried->get()) : std::string{};
    const auto scroll = rules::parse_scroll_key(key);
    if (!scroll || scroll->unidentified || !scroll->arcane || wizard.vitals.dead)
    {
        return copies;
    }
    for (unsigned n = 0; n < scroll->spells.size(); ++n)
    {
        const auto spell = std::string(scroll->spells[n].id);
        const bool listed = std::any_of(copies.begin(), copies.end(),
                                        [&](const auto &copy)
                                        {
                                            return copy.spell == spell;
                                        });
        if (const auto level = rules_->copyable_spell_level(wizard.character.sheet(), spell);
            level && !listed)
        {
            copies.push_back({n, spell, *level});
        }
    }
    return copies;
}

void CampaignParty::copy_scroll_spell(MemberId id, std::uint64_t item, unsigned entry)
{
    editable();
    outside_combat();
    if (std::find(state_.slots.begin(), state_.slots.end(), id) == state_.slots.end())
    {
        throw std::runtime_error("Only an active member copies a spell");
    }
    const auto copies = copyable_spells(id, item);
    const auto copy = std::find_if(copies.begin(), copies.end(),
                                   [&](const auto &c)
                                   {
                                       return c.entry == entry;
                                   });
    if (copy == copies.end())
    {
        throw std::runtime_error("That spell cannot be copied into the spellbook");
    }
    auto next = state_;
    auto &wizard = member_in(next, id);
    // A bottle of Magic Ink for each spell level, from the Wizard's own pack.
    auto inventory = wizard.character.inventory();
    std::uint32_t ink = 0;
    for (const auto &carried : inventory.items())
    {
        ink += carried.definition_id == "magic_ink" ? carried.quantity : 0;
    }
    if (ink < copy->level)
    {
        throw std::runtime_error("Copying the spell takes a bottle of Magic Ink for each spell "
                                 "level");
    }
    for (std::uint32_t needed = copy->level; needed;)
    {
        const auto bottle = std::find_if(inventory.items().begin(), inventory.items().end(),
                                         [](const auto &carried)
                                         {
                                             return carried.definition_id == "magic_ink";
                                         });
        const auto spent = std::min(needed, bottle->quantity);
        const auto bottle_id = bottle->id;
        inventory.remove(bottle_id, spent);
        if (!inventory.find(bottle_id))
        {
            wizard.item_sources.erase(bottle_id);
        }
        needed -= spent;
    }
    wizard.character.replace_inventory(std::move(inventory));
    // Recorded with the hall training, so a save rebuilds the spellbook.
    wizard.character.train_at_hall(*rules_, "boon:scroll:" + copy->spell);
    const auto spells =
        rules::parse_scroll_key(wizard.character.inventory().find(item)->get().definition_id)
            ->spells.size();
    keep_scroll_spells(wizard, item, ((1U << spells) - 1) & ~(1U << entry));
    elapse(next, std::chrono::hours{2 * copy->level});
    state_ = std::move(next);
}

int CampaignParty::reaction_charisma(MemberId id) const
{
    const int charisma = member(id).character.sheet().scores[rules::Ability::charisma];
    const bool friends = state_.friends_member == id && state_.time_minutes < state_.friends_until;
    return friends ? std::min(25, charisma + state_.friends_bonus) : charisma;
}

void CampaignParty::use_camp_action(MemberId user, MemberId target, std::string_view action)
{
    editable();
    if (action.starts_with("identify"))
    {
        throw std::runtime_error("Identify is cast on an item from the inventory");
    }
    for (const auto id : {user, target})
    {
        if (std::find(state_.slots.begin(), state_.slots.end(), id) == state_.slots.end())
        {
            throw std::runtime_error("Camp actions are for active members");
        }
    }
    auto next = state_;
    const auto find = [&](MemberId id) -> PartyMember &
    {
        return member_in(next, id);
    };
    auto &caster = find(user);
    // Reincarnate's new body takes the species' choices: reincarnate().
    if (action.starts_with("reincarnate"))
    {
        throw std::runtime_error("Reincarnate needs the new species' choices");
    }
    const auto actions = camp_actions(user);
    const auto chosen = std::find_if(actions.begin(), actions.end(),
                                     [&](const auto &a)
                                     {
                                         return a.id == action;
                                     });
    // A whole-party action such as Prayer of Healing picks its own members
    // among the active ones; `target` is not used.
    if (chosen != actions.end() && chosen->whole_party)
    {
        std::vector<rules::CampTarget> party;
        for (const auto id : next.slots)
        {
            if (id)
            {
                auto &m = find(id);
                party.push_back({m.character.sheet(), m.vitals});
            }
        }
        rules_->use_party_camp_action(caster.character.sheet(), caster.vitals, party, action,
                                      next.random_state);
        if (action.starts_with("detect_magic"))
        {
            constexpr std::uint64_t detect_magic_minutes = 10;
            next.detect_magic_until = next.time_minutes + detect_magic_minutes;
        }
    }
    else
    {
        auto &patient = find(target);
        // An offering, such as Revivify's, is paid from the party purse's gold.
        const auto gold =
            rules_->camp_action_gold(patient.character.sheet(), patient.vitals, action);
        if (gold > coins(next.wealth, Coin::gold))
        {
            throw std::runtime_error("The party purse cannot pay the offering");
        }
        rules_->use_camp_action(caster.character.sheet(), caster.vitals, patient.character.sheet(),
                                patient.vitals, action, next.random_state);
        coins(next.wealth, Coin::gold) -= gold;
        if (action.starts_with("remove_curse"))
        {
            put_down_cursed_items(patient);
        }
    }
    state_ = std::move(next);
}

std::optional<CoinExchange> CampaignParty::read_character(PartySlot slot, const por::EclMachine &vm)
{
    outside_combat();
    if (slot.index >= 8)
    {
        throw std::runtime_error("Invalid ECL party position");
    }
    const auto id = state_.slots[slot.index];
    if (!id)
    {
        return {};
    }
    const auto &current = member(id);
    const auto hp = vm.variable(por::ecl_character_hit_points);
    if (hp > hit_point_maximum(id) || (current.vitals.dead && hp))
    {
        throw std::runtime_error("Unsupported script HP change");
    }
    Purse after_script;
    for (unsigned n = 0; n < 7; ++n)
    {
        after_script[n] = vm.variable(por::ecl_coin_addresses[n]);
    }
    // A script read changes only HP and coins, which pending Long Rest spell or
    // training choices do not depend on; the inn's script continues after its rest.
    if (state_.short_rest)
    {
        throw std::runtime_error("Finish Short Rest spending before changing the party");
    }
    auto settled = settle_script_coins(state_.wealth, after_script);
    auto &m = edit(id);
    auto vitals = m.vitals;
    rules_->set_hit_points(vitals, m.character.sheet(), hp);
    state_.wealth = settled.purse;
    m.vitals = std::move(vitals);
    return settled.exchange;
}

void CampaignParty::validate(const PartyState &state)
{
    if (state.roster.size() > 128 || state.selected_slot.index >= 8 || !state.next_id ||
        state.claimed_rewards.size() > 1024 || state.subminute_milliseconds >= 60000 ||
        !state.next_combat_scope || !state.next_rest_session ||
        (state.leader &&
         std::find(state.slots.begin(), state.slots.end(), state.leader) == state.slots.end()) ||
        // Friends adds 2d4 to one member's Charisma (MAGIC-6).
        state.friends_bonus < 0 || state.friends_bonus > 8 ||
        (state.friends_member &&
         std::none_of(state.roster.begin(), state.roster.end(),
                      [&](const auto &m)
                      {
                          return m.id == state.friends_member;
                      })))
    {
        throw std::runtime_error("Invalid party checkpoint");
    }
    std::set<MemberId> ids, active;
    std::set<std::string> sources, creation_sources;
    for (const auto &m : state.roster)
    {
        if (!m.creation_source.empty() &&
            (m.creation_source.size() > 160 || !creation_sources.insert(m.creation_source).second))
        {
            throw std::runtime_error("Invalid creation source checkpoint");
        }
        if (!m.id || m.id >= state.next_id || !ids.insert(m.id).second || m.vitals.hit_points < 0 ||
            (m.last_rest_minutes &&
             (*m.last_rest_minutes > state.time_minutes ||
              (*m.last_rest_minutes == state.time_minutes &&
               m.last_rest_subminute_milliseconds > state.subminute_milliseconds))) ||
            m.last_rest_subminute_milliseconds >= 60000 ||
            (!m.last_rest_minutes && m.last_rest_subminute_milliseconds) ||
            (m.vitals.dead && m.vitals.hit_points) || m.morale > 255 ||
            (!m.npc_source.empty() && !sources.insert(m.npc_source).second))
        {
            throw std::runtime_error("Invalid roster checkpoint");
        }
        std::set<std::uint64_t> equipment;
        for (auto item : m.equipped)
        {
            if (!m.character.inventory().find(item) || !equipment.insert(item).second)
            {
                throw std::runtime_error("Invalid equipment checkpoint");
            }
        }
    }
    if (std::any_of(state.claimed_rewards.begin(), state.claimed_rewards.end(),
                    [](const auto &id)
                    {
                        return id.empty() || id.size() > 160;
                    }) ||
        std::set<std::string>(state.claimed_rewards.begin(), state.claimed_rewards.end()).size() !=
            state.claimed_rewards.size())
    {
        throw std::runtime_error("Invalid claimed rewards");
    }
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        if (auto id = state.slots[slot])
        {
            if (!ids.contains(id) || !active.insert(id).second)
            {
                throw std::runtime_error("Invalid active party checkpoint");
            }
            if ((slot < 6) != member_in(state, id).npc_source.empty())
            {
                throw std::runtime_error("Invalid PC/NPC checkpoint position");
            }
        }
    }
    if (!active.empty() && !state.slots[state.selected_slot.index])
    {
        throw std::runtime_error("Invalid selected member checkpoint");
    }
    if (state.spell_rest)
    {
        const auto &rest = *state.spell_rest;
        std::set<MemberId> members;
        if (state.short_rest || !rest.ticket.session ||
            rest.ticket.session >= state.next_rest_session || !rest.ticket.revision ||
            rest.completed_minutes != state.time_minutes ||
            rest.completed_subminute_milliseconds != state.subminute_milliseconds ||
            rest.members.empty() || rest.members.size() > 8)
        {
            throw std::runtime_error("Invalid spell-choice rest checkpoint");
        }
        for (auto id : rest.members)
        {
            if (!active.contains(id) || !members.insert(id).second)
            {
                throw std::runtime_error("Invalid spell-choice rest member");
            }
            const auto &m = member_in(state, id);
            // Only a Long Rest is recorded as the member's last rest.
            const bool rested =
                state.spells_after_short_rest ||
                (m.last_rest_minutes && *m.last_rest_minutes == rest.completed_minutes &&
                 m.last_rest_subminute_milliseconds == rest.completed_subminute_milliseconds);
            if (!rested ||
                std::any_of(m.character.spell_edits().begin(), m.character.spell_edits().end(),
                            [&](const auto &e)
                            {
                                return e.rest_session >= rest.ticket.session;
                            }))
            {
                throw std::runtime_error("Invalid completed-rest entitlement");
            }
        }
    }
    if (state.training_rest)
    {
        const auto &rest = *state.training_rest;
        std::set<MemberId> members;
        if (state.short_rest || !rest.ticket.session ||
            rest.ticket.session >= state.next_rest_session || !rest.ticket.revision ||
            rest.completed_minutes != state.time_minutes ||
            rest.completed_subminute_milliseconds != state.subminute_milliseconds ||
            rest.members.empty() || rest.members.size() > 8)
        {
            throw std::runtime_error("Invalid training-choice rest checkpoint");
        }
        if (state.spell_rest && state.spell_rest->ticket != rest.ticket)
        {
            throw std::runtime_error("Rest choice tickets disagree");
        }
        for (auto id : rest.members)
        {
            if (!active.contains(id) || !members.insert(id).second)
            {
                throw std::runtime_error("Invalid training-choice rest member");
            }
            const auto &m = member_in(state, id);
            // Only a Long Rest is recorded as the member's last rest.
            const bool rested =
                state.training_after_short_rest ||
                (m.last_rest_minutes && *m.last_rest_minutes == rest.completed_minutes &&
                 m.last_rest_subminute_milliseconds == rest.completed_subminute_milliseconds);
            if (!rested || std::any_of(m.character.training_edits().begin(),
                                       m.character.training_edits().end(),
                                       [&](const auto &e)
                                       {
                                           return e.rest_session >= rest.ticket.session;
                                       }))
            {
                throw std::runtime_error("Invalid completed-rest training entitlement");
            }
        }
    }
    if (state.short_rest)
    {
        const auto &rest = *state.short_rest;
        std::set<MemberId> members;
        if (!rest.ticket.session || rest.ticket.session >= state.next_rest_session ||
            !rest.ticket.revision || rest.completed_minutes != state.time_minutes ||
            rest.completed_subminute_milliseconds != state.subminute_milliseconds ||
            rest.members.empty() || rest.members.size() > 8)
        {
            throw std::runtime_error("Invalid Short Rest checkpoint");
        }
        for (auto id : rest.members)
        {
            if (!active.contains(id) || !members.insert(id).second)
            {
                throw std::runtime_error("Invalid Short Rest member");
            }
        }
    }
}

void CampaignParty::validate_rest_choices(const PartyState &state, const rules::RulesModule &rules)
{
    validate(state);
    // The maximum is the rules': Aid raises it above the sheet's.
    for (const auto &m : state.roster)
    {
        if (m.vitals.hit_points > rules.hit_point_maximum(m.character.sheet(), m.vitals))
        {
            throw std::runtime_error("Invalid party checkpoint");
        }
    }
    for (const auto &m : state.roster)
    {
        for (const auto &e : m.character.spell_edits())
        {
            if (e.rest_session >= state.next_rest_session)
            {
                throw std::runtime_error("Spell history exceeds rest sequence");
            }
        }
    }
    for (const auto &m : state.roster)
    {
        for (const auto &e : m.character.training_edits())
        {
            if (e.rest_session >= state.next_rest_session)
            {
                throw std::runtime_error("Training history exceeds rest sequence");
            }
        }
    }
    if (state.training_rest)
    {
        for (auto id : state.training_rest->members)
        {
            const auto &m = member_in(state, id);
            if (!rules.recovery_info(m.character.sheet(), m.vitals).can_rest ||
                rules
                    .rest_training_options(m.character.sheet(), state.training_after_short_rest
                                                                    ? RestKind::short_rest
                                                                    : RestKind::long_rest)
                    .empty())
            {
                throw std::runtime_error("Invalid Long Rest training eligibility");
            }
        }
    }
    if (state.spell_rest)
    {
        for (auto id : state.spell_rest->members)
        {
            const auto &m = member_in(state, id);
            const auto options = rules.spell_choice_options(
                m.character.sheet(), state.spells_after_short_rest
                                         ? rules::SpellChoiceContext::short_rest
                                         : rules::SpellChoiceContext::long_rest);
            if (!rules.recovery_info(m.character.sheet(), m.vitals).can_rest ||
                (!options.may_prepare && !options.may_replace))
            {
                throw std::runtime_error("Invalid Long Rest spell eligibility");
            }
        }
    }
    if (state.short_rest)
    {
        for (auto id : state.short_rest->members)
        {
            const auto &m = member_in(state, id);
            if (!rules.recovery_info(m.character.sheet(), m.vitals).can_rest)
            {
                throw std::runtime_error("Invalid Short Rest vitality");
            }
        }
    }
}

void CampaignParty::restore(PartyState state)
{
    outside_combat();
    validate_rest_choices(state, *rules_);
    for (const auto &m : state.roster)
    {
        (void)rules_->character_profile(m.character.sheet(), equipped_definitions(m));
    }
    state_ = std::move(state);
}

std::vector<rules::Participant> CampaignParty::participants() const
{
    std::vector<rules::Participant> result;
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        if (auto id = state_.slots[slot])
        {
            const auto &m = member(id);
            if (m.vitals.dead)
            {
                continue;
            }
            auto p = rules_->character_profile(m.character.sheet(), equipped_definitions(m));
            result.push_back({id,
                              "campaign-character",
                              m.character.sheet().name,
                              rules::Side::party,
                              {1 + static_cast<int>(slot / 4), 1 + static_cast<int>(slot % 4) * 2},
                              std::move(p.data),
                              m.vitals});
            for (const auto &item : m.character.inventory().items())
            {
                const auto equipped = std::find(m.equipped.begin(), m.equipped.end(), item.id);
                result.back().inventory.push_back(
                    {item.id, rules_definition(m, item), item.quantity,
                     equipped == m.equipped.end()
                         ? -1
                         : static_cast<int>(equipped - m.equipped.begin())});
            }
        }
    }
    if (result.empty())
    {
        throw std::runtime_error("Add a living combat-ready character first");
    }
    return result;
}

void CampaignParty::begin_combat()
{
    outside_combat();
    if (state_.spell_rest || state_.training_rest)
    {
        throw std::runtime_error("Finish Long Rest choices before combat");
    }
    if (state_.next_combat_scope == std::numeric_limits<std::uint64_t>::max())
    {
        throw std::runtime_error("Combat identity exhausted");
    }
    combat_ = true;
    combat_registered_ = false;
    combat_elapsed_ = 0;
    combat_offerings_ = 0;
    combat_checked_.clear();
}

void CampaignParty::apply_combat(const rules::Snapshot &snapshot)
{
    if (!combat_ || snapshot.identity != rules_->identity())
    {
        throw std::runtime_error("Combat rules identity mismatch");
    }
    if (snapshot.elapsed_milliseconds < combat_elapsed_)
    {
        throw std::runtime_error("Combat clock moved backward");
    }
    if (snapshot.offerings_gold < combat_offerings_ ||
        snapshot.offerings_gold - combat_offerings_ > coins(state_.wealth, Coin::gold))
    {
        throw std::runtime_error("Combat offerings exceed the party purse");
    }
    // Everything that can throw is worked out first, without touching the
    // party; the commit below only moves. A member is copied only when the
    // fight used up thrown gear it carried.
    std::vector<MemberId> active;
    for (const auto &actor : snapshot.combatants)
    {
        if (actor.side == rules::Side::party)
        {
            active.push_back(actor.id);
        }
    }
    // Combat has already advanced active actors' effects and mortality. Only reserves
    // need campaign-side updates, preventing duplicate recovery rolls.
    auto reserves = elapsed(state_,
                            std::chrono::milliseconds{static_cast<std::chrono::milliseconds::rep>(
                                snapshot.elapsed_milliseconds - combat_elapsed_)},
                            active);
    std::vector<std::pair<MemberId, rules::VitalState>> vitals;
    std::vector<PartyMember> thrown;
    std::vector<std::pair<MemberId, int>> newly_checked;
    for (const auto &actor : snapshot.combatants)
    {
        if (actor.side == rules::Side::party)
        {
            const auto it = std::find_if(state_.roster.begin(), state_.roster.end(),
                                         [&](const auto &m)
                                         {
                                             return m.id == actor.id;
                                         });
            if (it == state_.roster.end())
            {
                throw std::runtime_error("Combat party identity mismatch");
            }
            const auto checked = std::find(combat_checked_.begin(), combat_checked_.end(),
                                           std::pair{actor.id, actor.max_hit_points});
            if (checked == combat_checked_.end())
            {
                if (actor.max_hit_points !=
                    rules_->hit_point_maximum(it->character.sheet(), actor.persistent))
                {
                    throw std::runtime_error("Combat party identity mismatch");
                }
                (void)rules_->character_profile(it->character.sheet(), equipped_definitions(*it));
                newly_checked.emplace_back(actor.id, actor.max_hit_points);
            }
            vitals.emplace_back(actor.id, actor.persistent);
            if (!actor.thrown_gear_left.empty() || actor.curse_lifted ||
                !actor.item_uses_left.empty())
            {
                auto member = *it;
                for (const auto &[definition, left] : actor.thrown_gear_left)
                {
                    keep_thrown_gear(member, definition, left);
                }
                for (const auto &[item, left] : actor.item_uses_left)
                {
                    keep_item_uses(member, item, left);
                }
                if (actor.curse_lifted)
                {
                    put_down_cursed_items(member);
                }
                thrown.push_back(std::move(member));
            }
        }
    }
    // Room now, so the commit allocates nothing.
    combat_checked_.reserve(combat_checked_.size() + newly_checked.size());
    // Revivify's offering is paid from the party purse as it is made.
    coins(state_.wealth, Coin::gold) -= snapshot.offerings_gold - combat_offerings_;
    apply(state_, std::move(reserves));
    for (auto &member : state_.roster)
    {
        for (auto &changed : thrown)
        {
            if (changed.id == member.id)
            {
                member = std::move(changed);
            }
        }
        for (auto &[id, state] : vitals)
        {
            if (id == member.id)
            {
                member.vitals = std::move(state);
            }
        }
    }
    state_.short_rest.reset();
    if (!combat_registered_)
    {
        ++state_.next_combat_scope;
    }
    std::erase_if(combat_checked_,
                  [&](const auto &entry)
                  {
                      return std::any_of(newly_checked.begin(), newly_checked.end(),
                                         [&](const auto &now)
                                         {
                                             return now.first == entry.first;
                                         });
                  });
    combat_checked_.insert(combat_checked_.end(), newly_checked.begin(), newly_checked.end());
    combat_elapsed_ = snapshot.elapsed_milliseconds;
    combat_offerings_ = snapshot.offerings_gold;
    combat_registered_ = true;
}
} // namespace opengold
