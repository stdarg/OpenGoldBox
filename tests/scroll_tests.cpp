#include "combat_fixture.h"
#include "opengold/authored_items.h"
#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/combat_demo.h"
#include "opengold/scroll_key.h"
#include "opengold/srd5.h"
#include "test_support.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

// The original's spell scrolls (#265, MAGIC-6).
namespace
{
using opengold::test::check;

const auto source = std::filesystem::path(OPENGOLD_SOURCE_DIR);

std::string read(const std::filesystem::path &path)
{
    std::ifstream in(path);
    check(bool(in), "Read the rules");
    return {std::istreambuf_iterator<char>(in), {}};
}

// The game's rules, and a giant snake for Snake Charm.
std::unique_ptr<RulesModule> module()
{
    return srd5::parse_content(read(source / "data/rules/srd-5.2.1/combat.rules") +
                               "\ncreature giant-snake 12 5 1 30 3 1 6 1 3 1 8 1 80 320 0 0 0 1 0\n");
}

CharacterDraft draft(std::string klass, std::string background)
{
    CharacterDraft d;
    d.race = "human";
    d.gender = "female";
    d.character_class = std::move(klass);
    d.background = std::move(background);
    d.alignment = "neutral_good";
    d.name = "Reader";
    d.rolled = true;
    for (auto &r : d.rolls)
    {
        r = {{6, 5, 4, 1}, 3};
    }
    return d;
}

Character wizard()
{
    return Character(*srd5::character_rules(), draft("wizard", "sage"), {});
}

Character cleric()
{
    auto d = draft("cleric", "acolyte");
    d.cantrips = std::vector<std::string>{"sacred_flame", "spare_the_dying", "resistance"};
    d.spells = SpellChoices{{}, std::vector<std::string>{"bless"}, {}, {}};
    d.training = {{"class:cleric", {"medicine", "persuasion"}},
                  {"class:cleric:divine_order", {"protector"}}};
    return Character(*srd5::character_rules(), d, {});
}

Character fighter()
{
    return Character(*srd5::character_rules(), draft("fighter", "soldier"), {});
}

// An original scroll: magic-user (61) or cleric (62), with spell numbers.
por::Equipment scroll(std::uint8_t type, std::array<std::uint8_t, 3> spells)
{
    por::Equipment item;
    item.stored.type = type;
    item.stored.magic_bonus = 1;
    item.stored.effect_codes = spells;
    item.stored.stack_size = 1;
    return item;
}

void conversion()
{
    check(equipment_conversion(scroll(61, {47, 33, 34})) ==
                  "scroll:arcane:fireball,ray_of_enfeeblement,stinking_cloud" &&
              equipment_conversion(scroll(62, {56, 56, 0})) ==
                  "scroll:divine:greater_restoration,greater_restoration",
          "Scrolls convert with their spells");
    check(equipment_conversion(scroll(61, {50, 14, 54})) ==
              "scroll:arcane:invisibility@3,friends,protection_from_normal_missiles",
          "Invisibility, 10' Radius is Invisibility from a 3rd-level slot; stand-ins convert");
    check(equipment_conversion(scroll(62, {3, 90, 0})) == "scroll:divine:cure_wounds" &&
              equipment_conversion(scroll(61, {85, 90, 0})).starts_with("por:unsupported:"),
          "A number no spell has holds nothing");
    const auto parsed = parse_scroll_key("scroll:arcane:invisibility@3,friends:unidentified");
    check(parsed && parsed->arcane && parsed->unidentified && parsed->spells.size() == 2 &&
              parsed->spells[0].id == "invisibility" && parsed->spells[0].level == 3 &&
              !parse_scroll_key("scroll:arcane:") && !parse_scroll_key("scroll:wild:bless"),
          "Scroll keys parse");
}

bool submit(CombatSession &c, std::string_view verb, EntityId target = 0)
{
    for (const auto &command : c.legal_commands())
    {
        if (command.verb == verb && (!target || command.target == target))
        {
            return c.submit(command);
        }
    }
    return false;
}

bool offered(const CombatSession &c, std::string_view verb)
{
    const auto commands = c.legal_commands();
    return std::any_of(commands.begin(), commands.end(),
                       [&](const auto &command)
                       {
                           return command.verb == verb;
                       });
}

unsigned uses_left(const CombatSession &c, std::uint64_t item)
{
    for (const auto &combatant : c.snapshot().combatants)
    {
        for (const auto &[id, left] : combatant.item_uses_left)
        {
            if (combatant.id == 1 && id == item)
            {
                return left;
            }
        }
    }
    throw std::runtime_error("The item is not reported");
}

// The reader's first turn, carrying `scroll` as inventory item 7, against a
// foe at `foe`, or two creatures when `second` names one.
std::unique_ptr<CombatSession> reader_turn(const RulesModule &rules, const Character &reader,
                                           const std::string &scroll, Cell foe = {6, 1},
                                           std::string foe_kind = "vanguard")
{
    Participant p{1, "campaign-character", "Reader", Side::party, {1, 1},
                  rules.character_profile(reader.sheet(), std::vector<std::string>{}).data};
    p.inventory.push_back({7, scroll, 1});
    auto c = rules.create(
        {{12, 6, std::vector<Terrain>(72)}, {p, {2, foe_kind, "Foe", Side::opposition, foe}}}, 5);
    test::keep_initiative(*c);
    for (unsigned turns = 0; c->snapshot().actor != 1 && turns < 4; ++turns)
    {
        check(submit(*c, "end"), "End the foe's turn");
    }
    check(c->snapshot().actor == 1, "The reader acts");
    return c;
}

bool aim_and_cast(CombatSession &c, std::string_view verb, Cell center)
{
    if (!submit(c, verb))
    {
        return false;
    }
    for (const auto &command : c.legal_commands())
    {
        if (command.verb == "area_move" && command.destination == center)
        {
            (void)c.submit(command);
            break;
        }
    }
    return submit(c, "area_cast");
}

// In a fight, a scroll's spell is read like a spell, with the scroll's DC, and
// leaves the scroll.
void combat()
{
    const auto rules = module();
    auto c = reader_turn(*rules, wizard(), "scroll:arcane:fireball,haste");
    check(offered(*c, "fireball_scroll") && offered(*c, "haste_scroll"),
          "A Wizard reads an arcane scroll's spells, whatever its level");
    check(aim_and_cast(*c, "fireball_scroll", {6, 1}) &&
              test::logged(*c, "reads Fireball from a scroll") && test::logged(*c, "DC 15"),
          "Fireball from a scroll has the scroll's DC 15");
    check(uses_left(*c, 7) == 0b10 && !offered(*c, "fireball_scroll"),
          "Fireball leaves the scroll; Haste stays");
    check(rules->restore(c->save())->save() == c->save(), "A checkpoint keeps the scroll's spells");

    check(!offered(*reader_turn(*rules, cleric(), "scroll:arcane:fireball"), "fireball_scroll") &&
              !offered(*reader_turn(*rules, fighter(), "scroll:divine:bless"), "bless_scroll"),
          "A Cleric reads no arcane scroll, and a Fighter none at all");
    check(offered(*reader_turn(*rules, cleric(), "scroll:divine:hold_person"),
                  "hold_person_scroll"),
          "A Cleric reads a divine scroll");
    check(!offered(*reader_turn(*rules, wizard(), "scroll:arcane:fireball:unidentified"),
                   "fireball_scroll"),
          "An unidentified scroll cannot be read");
    check(offered(*reader_turn(*rules, wizard(), "scroll:arcane:invisibility@3"),
                  "invisibility_scroll_3"),
          "Invisibility, 10' Radius is read from a 3rd-level slot");

    // The stand-ins work in a fight as the original's did.
    auto charm = reader_turn(*rules, cleric(), "scroll:divine:snake_charm", {2, 1}, "giant-snake");
    check(submit(*charm, "snake_charm_scroll", 1) &&
              test::logged(*charm, "Foe is charmed by Snake Charm"),
          "Snake Charm holds a snake whose Hit Points fit within the caster's");
    check(submit(*charm, "end") && submit(*charm, "end") && charm->snapshot().actor == 1,
          "The snake loses its turn");
    auto ward = reader_turn(*rules, wizard(), "scroll:arcane:protection_from_normal_missiles");
    check(submit(*ward, "protection_from_normal_missiles_scroll", 1) &&
              test::logged(*ward, "gains Protection From Normal Missiles"),
          "Protection From Normal Missiles is read on the reader");
}

// Outside combat, a scroll's spell is read from the Cast menu; partly used
// scrolls keep their other spells in a save.
void camp()
{
    CampaignParty party(module());
    const auto id = party.add_pc(cleric());
    check(party.award_loot({}, {scroll(62, {3, 3, 0})}, "test:scroll"),
          "Collect a scroll of Cure Light Wounds, Cure Light Wounds");
    const auto item = party.member(id).character.inventory().items().back().id;
    auto wounded = party.checkpoint();
    wounded.roster.front().vitals.hit_points = 1;
    party.restore(std::move(wounded));
    const auto actions = party.scroll_actions(id, item);
    check(actions.size() == 2 && actions.front().id == "0:cure_wounds",
          "The Cast menu offers each spell on the scroll");
    party.read_scroll(id, item, id, "0:cure_wounds");
    check(party.member(id).vitals.hit_points > 1 &&
              party.member(id).character.inventory().find(item)->get().definition_id ==
                  "scroll:divine:cure_wounds",
          "Cure Wounds heals, and one spell is left");

    CampaignParty restored(module());
    const auto rules = module();
    restored.restore(decode_campaign(encode_campaign(party, nullptr, "scroll"),
                                     *srd5::character_rules(), *rules, "scroll", nullptr)
                         .party);
    check(restored.member(id).character.inventory().find(item)->get().definition_id ==
              "scroll:divine:cure_wounds",
          "A save keeps the partly used scroll");
    auto again = restored.checkpoint();
    again.roster.front().vitals.hit_points = 1;
    restored.restore(std::move(again));
    restored.read_scroll(id, item, id, "0:cure_wounds");
    check(!restored.member(id).character.inventory().find(item),
          "The scroll is gone with its last spell");

    test::rejects(
        [&]
        {
            CampaignParty unread(module());
            const auto fighter_id = unread.add_pc(fighter());
            (void)unread.award_loot({}, {scroll(62, {3, 0, 0})}, "test:unread");
            const auto held = unread.member(fighter_id).character.inventory().items().back().id;
            unread.read_scroll(fighter_id, held, fighter_id, "0:cure_wounds");
        },
        "A Fighter cannot read a scroll");
}

// Friends, Read Magic and Find Traps are read in camp, as the original's.
void stand_ins()
{
    CampaignParty party(module());
    const auto id = party.add_pc(wizard());
    auto hidden = scroll(61, {47, 0, 0});
    hidden.stored.name_components = {0, 1, 2};
    hidden.stored.hidden_name_words = 6;
    check(party.award_loot({}, {scroll(61, {14, 18, 0}), hidden}, "test:stand-ins"),
          "Collect a scroll of Friends and Read Magic, and a hidden scroll");
    const auto items = party.member(id).character.inventory().items();
    const auto both = items[items.size() - 2].id, fireball = items.back().id;
    const int charisma = party.reaction_charisma(id);
    party.read_scroll(id, both, id, "0:friends");
    const int raised = party.reaction_charisma(id) - charisma;
    check(raised >= 2 && raised <= 8, "Friends adds 2d4 to Charisma");
    party.advance_time(std::chrono::minutes{6});
    check(party.reaction_charisma(id) == charisma, "Friends ends after six minutes");

    check(party.scroll_actions(id, fireball).empty() && party.copyable_spells(id, fireball).empty(),
          "A hidden scroll offers nothing");
    party.read_scroll(id, both, id, "0:read_magic");
    check(!party.member(id).character.inventory().find(both) &&
              rules::parse_scroll_key(rules_definition(
                                          party.member(id),
                                          party.member(id).character.inventory().find(fireball)->get()))
                  ->unidentified == false,
          "Read Magic reveals the reader's scrolls, and its scroll is used up");

    CampaignParty traps(module());
    const auto priest = traps.add_pc(cleric());
    check(traps.award_loot({}, {scroll(62, {22, 0, 0})}, "test:traps"), "Collect Find Traps");
    traps.read_scroll(priest, traps.member(priest).character.inventory().items().back().id, priest,
                      "0:find_traps");
    check(traps.finding_traps(), "Find Traps is in effect");
    traps.advance_time(std::chrono::minutes{30});
    check(!traps.finding_traps(), "Find Traps ends after 30 minutes");
}

// A Wizard copies a spell into the spellbook with Magic Ink and two hours a
// spell level.
void copying()
{
    CampaignParty party(module());
    const auto id = party.add_pc(wizard());
    const auto rules = module();
    const auto known = rules->spell_access(party.member(id).character.sheet()).spellbook;
    std::uint8_t number = 0;
    for (const auto &[candidate, spell] : std::vector<std::pair<std::uint8_t, std::string>>{
             {9, "burning_hands"}, {10, "charm_person"}, {21, "sleep"}, {15, "magic_missile"}})
    {
        if (!number && std::none_of(known.begin(), known.end(),
                                    [&](const auto &s)
                                    {
                                        return s.id == spell;
                                    }))
        {
            number = candidate;
        }
    }
    check(number != 0, "A level-1 spell the Wizard lacks");
    check(party.award_loot({}, {scroll(61, {number, 47, 0})}, "test:copy"), "Collect a scroll");
    const auto item = party.member(id).character.inventory().items().back().id;
    const auto copies = party.copyable_spells(id, item);
    check(copies.size() == 1 && copies.front().entry == 0 && copies.front().level == 1,
          "Only the level-1 spell can be copied by a level-1 Wizard");
    test::rejects(
        [&]
        {
            party.copy_scroll_spell(id, item, 0);
        },
        "Copying needs Magic Ink");
    Purse purse{};
    coins(purse, Coin::gold) = 50;
    party.set_wealth(purse);
    por::Equipment ink;
    ink.stored.type = authored_item::magic_ink;
    ink.stored.stored_name = "Bottle of Magic Ink";
    ink.stored.value = 50;
    ink.stored.stack_size = 1;
    party.purchase(id, ink);
    const auto before = party.state().time_minutes;
    party.copy_scroll_spell(id, item, 0);
    const auto spell = copies.front().spell;
    const auto has = [&](const CampaignParty &p)
    {
        const auto now = rules->spell_access(p.member(id).character.sheet()).spellbook;
        return std::any_of(now.begin(), now.end(),
                           [&](const auto &s)
                           {
                               return s.id == spell;
                           });
    };
    check(has(party) && party.state().time_minutes == before + 120 &&
              party.member(id).character.inventory().find(item)->get().definition_id ==
                  "scroll:arcane:fireball",
          "The spell is in the spellbook after two hours, and leaves the scroll");
    check(std::none_of(party.member(id).character.inventory().items().begin(),
                       party.member(id).character.inventory().items().end(),
                       [](const auto &carried)
                       {
                           return carried.definition_id == "magic_ink";
                       }),
          "The bottle of Magic Ink is used");

    CampaignParty restored(module());
    restored.restore(decode_campaign(encode_campaign(party, nullptr, "copy"),
                                     *srd5::character_rules(), *rules, "copy", nullptr)
                         .party);
    check(has(restored), "A save keeps the copied spell");
}

// The computer reads no scroll.
void quick()
{
    const auto rules = module();
    auto c = reader_turn(*rules, wizard(), "scroll:arcane:magic_missile,fireball");
    for (unsigned n = 0; n < 3; ++n)
    {
        check(choose_quick_command(*c, true).verb.find("_scroll") == std::string::npos,
              "Quick never reads a scroll");
    }
}
} // namespace

int main()
{
    try
    {
        conversion();
        combat();
        camp();
        stand_ins();
        copying();
        quick();
        std::cout << "Scroll tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
