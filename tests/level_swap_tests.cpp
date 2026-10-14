#include "opengold/campaign_party.h"
#include "opengold/campaign_save.h"
#include "opengold/character.h"
#include "opengold/character_pool.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace opengold;
using namespace opengold::rules;

// Replacing a known cantrip, spell, Eldritch Invocation or Metamagic option on
// gaining a level (SRD 5.2.1): what each class is offered, what a swap
// changes, a refused swap and a save's replay.
namespace
{
const auto root = std::filesystem::path(OPENGOLD_SOURCE_DIR);

void check(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::unique_ptr<RulesModule> module()
{
    return srd5::load(root / "data/rules/srd-5.2.1/combat.rules");
}

// A Character Pool draft of the class, as a player adds one.
CharacterDraft pool_draft(std::string_view klass)
{
    por::CharacterArt art;
    Image head;
    head.width = 88;
    head.height = 40;
    head.rgba.assign(88 * 40 * 4, 128);
    Image body;
    body.width = 88;
    body.height = 48;
    body.rgba.assign(88 * 48 * 4, 128);
    art.set_original_head(1, por::PortraitPart{"fixture", head});
    art.set_original_body(1, por::PortraitPart{"fixture", body});
    for (const auto &preset : character_pool(*srd5::character_rules(), art))
        if (preset.creation_data().character_class == klass)
            return preset.creation_data();
    throw std::runtime_error("No pool character of the class");
}

struct Party
{
    std::shared_ptr<CampaignParty> party = std::make_shared<CampaignParty>(module());
    MemberId id{};

    explicit Party(std::string_view klass)
    {
        id = party->add_pc(Character(*srd5::character_rules(), pool_draft(klass), {}));
        party->award_experience(2700, "swap-xp");
    }

    const CharacterSheet &sheet() const
    {
        return party->member(id).character.sheet();
    }

    AdvancementOptions options() const
    {
        return party->rule_module().advancement_options(sheet(), party->default_advancement(id));
    }

    // The default advancement with the swaps, and the spell choices the swaps
    // open (Pact of the Tome's picks) filled with their first options.
    AdvancementChoice choice(const LevelSwap &cantrip, const LevelSwap &spell,
                             const LevelSwap &option) const
    {
        auto result = party->default_advancement(id);
        result.cantrip_swap = cantrip;
        result.spell_swap = spell;
        result.option_swap = option;
        return filled(std::move(result), spell.replaced, option.replacement);
    }

    // The choice with its training and spell picks filled with the first
    // options, leaving out a replaced spell and a swapped-in option.
    AdvancementChoice filled(AdvancementChoice result, std::string_view replaced_spell = {},
                             std::string_view taken_option = {}) const
    {
        // Two picks are owed at level two; the dialog asks for them.
        for (const auto &group : party->rule_module().advancement_options(sheet(), result).training)
        {
            auto &picks = result.training[group.id];
            picks.clear();
            for (const auto &o : group.options)
                if (picks.size() < group.count && o.id != taken_option)
                    picks.push_back(o.id);
        }
        if (result.spell_learning)
        {
            const auto &rules = party->rule_module();
            const auto offered = rules.spell_choice_options(rules.spell_choice_sheet(sheet(), result),
                                 SpellChoiceContext::advancement);
            for (const auto &group : offered.learning)
            {
                auto &fill = (*result.spell_learning)[group.id];
                fill.clear();
                for (const auto &o : group.options)
                    if (fill.size() < group.count)
                        fill.push_back(o.id);
            }
            // The locked list follows the swap; free places take the first offers.
            result.spells = offered.locked_prepared;
            for (const auto &o : offered.preparation)
                if (result.spells.size() < offered.prepared_count && o.id != replaced_spell &&
                        std::find(result.spells.begin(), result.spells.end(), o.id) ==
                        result.spells.end())
                    result.spells.push_back(o.id);
        }
        return result;
    }

    void advance(const AdvancementChoice &c)
    {
        party->advance(id, c);
    }

    void roundtrip() const
    {
        const auto bytes = encode_campaign(*party, nullptr, "level-swaps");
        CampaignParty next(module());
        next.restore(decode_campaign(bytes, *srd5::character_rules(), party->rule_module(),
                                     "level-swaps", nullptr).party);
        check(encode_campaign(next, nullptr, "level-swaps") == bytes, "The swaps replay exactly");
    }
};

bool has(const CharacterSheet &sheet, std::string_view id)
{
    return std::any_of(sheet.grants.begin(), sheet.grants.end(), [&](const auto & g)
    {
        return g.id == id;
    });
}

bool prepared(const CharacterSheet &sheet, std::string_view id)
{
    return std::find(sheet.prepared_spells.begin(), sheet.prepared_spells.end(), id) !=
           sheet.prepared_spells.end();
}

LevelSwap first(const SwapOffer &offer)
{
    check(!offer.replaceable.empty() && !offer.replacements.empty(),
          offer.label + " has something to replace");
    return {offer.replaceable.front().id, offer.replacements.front().id};
}

// A Sorcerer swaps a cantrip and a spell at level two, a Metamagic option at
// level three; a swap the offer lacks is refused and changes nothing.
void sorcerer_checks()
{
    Party p("sorcerer");
    auto options = p.options();
    check(options.cantrip_swap.label == "Cantrip" && options.spell_swap.label == "Spell" &&
          options.option_swap.replaceable.empty(),
          "At level two a Sorcerer may swap a cantrip and a spell; no Metamagic yet");
    const auto cantrip = first(options.cantrip_swap);
    const auto spell = first(options.spell_swap);
    const auto before = encode_campaign(*p.party, nullptr, "level-swaps");
    bool refused = false;
    try
    {
        p.advance(p.choice({cantrip.replaced, "sacred_flame"}, {}, {}));
    }
    catch (const std::exception &)
    {
        refused = true;
    }
    check(refused && encode_campaign(*p.party, nullptr, "level-swaps") == before,
          "A cantrip from another list is refused atomically");
    p.advance(p.choice(cantrip, spell, {}));
    check(has(p.sheet(), "spell:" + cantrip.replacement) &&
          !has(p.sheet(), "spell:" + cantrip.replaced),
          "The cantrip is replaced");
    check(prepared(p.sheet(), spell.replacement) && !prepared(p.sheet(), spell.replaced),
          "The spell is replaced");
    options = p.options();
    check(options.option_swap.label == "Metamagic" && options.option_swap.replaceable.size() == 2,
          "At level three both Metamagic options may be swapped");
    const auto metamagic = first(options.option_swap);
    p.advance(p.choice({}, {}, metamagic));
    check(has(p.sheet(), "metamagic:" + metamagic.replacement) &&
          !has(p.sheet(), "metamagic:" + metamagic.replaced),
          "The Metamagic option is replaced");
    p.roundtrip();
}

// A Warlock may swap its level-1 invocation at level two for one that needs
// level two, and take Pact of the Tome later, choosing its spells then.
void warlock_checks()
{
    Party p("warlock");
    auto options = p.options();
    check(options.option_swap.label == "Eldritch Invocation" &&
          options.option_swap.replaceable.size() == 1,
          "At level two the Warlock's one invocation may be swapped");
    const auto old = options.option_swap.replaceable.front().id;
    const auto agonizing = std::any_of(options.option_swap.replacements.begin(),
                                       options.option_swap.replacements.end(),
                                       [](const auto & o)
    {
        return o.id == "agonizing_blast:eldritch_blast";
    });
    check(agonizing, "A level-two invocation is offered for the level-1 one");
    p.advance(p.choice({}, {}, {old, "agonizing_blast:eldritch_blast"}));
    check(has(p.sheet(), "invocation:agonizing_blast:eldritch_blast") &&
          !has(p.sheet(), "invocation:" + old),
          "The level-1 invocation becomes Agonizing Blast");
    p.roundtrip();

    options = p.options();
    const auto swapped = options.option_swap.replaceable.back().id;
    p.advance(p.choice({}, {}, {swapped, "pact_of_the_tome"}));
    const auto &sheet = p.sheet();
    const auto tome = std::count_if(sheet.grants.begin(), sheet.grants.end(), [](const auto & g)
    {
        return g.source_id == "invocation:pact_of_the_tome";
    });
    check(has(sheet, "invocation:pact_of_the_tome") && tome == 5,
          "Pact of the Tome taken at level three brings its five spells");
    p.roundtrip();

    p.advance(p.choice({}, {}, {"pact_of_the_tome", "eldritch_mind"}));
    check(std::none_of(p.sheet().grants.begin(), p.sheet().grants.end(), [](const auto & g)
    {
        return g.source_id == "invocation:pact_of_the_tome";
    }), "Giving up Pact of the Tome gives up its spells");
    p.roundtrip();
}

// A Druid may swap a cantrip on gaining a level; a Wizard swaps cantrips on
// a Long Rest instead, and a Fighter has nothing to swap.
void other_class_checks()
{
    Party druid("druid");
    const auto cantrip = first(druid.options().cantrip_swap);
    check(druid.options().spell_swap.replaceable.empty(),
          "A Druid prepares spells and swaps none on gaining a level");
    druid.advance(druid.choice(cantrip, {}, {}));
    check(has(druid.sheet(), "spell:" + cantrip.replacement), "The Druid's cantrip is replaced");
    druid.roundtrip();
    check(Party("cleric").options().cantrip_swap.replacements.empty(),
          "A Cleric who knows every Cleric cantrip in the game has none to swap to");
    const auto wizard = Party("wizard").options();
    check(wizard.cantrip_swap.replaceable.empty() && wizard.spell_swap.replaceable.empty(),
          "A Wizard swaps nothing on gaining a level");
    const auto fighter = Party("fighter").options();
    check(fighter.cantrip_swap.replaceable.empty() && fighter.option_swap.replaceable.empty(),
          "A Fighter has nothing to swap");
}

// A Bard swaps a cantrip and a spell on gaining a level (#126).
void bard_checks()
{
    Party p("bard");
    const auto options = p.options();
    const auto cantrip = first(options.cantrip_swap);
    const auto spell = first(options.spell_swap);
    p.advance(p.choice(cantrip, spell, {}));
    check(has(p.sheet(), "spell:" + cantrip.replacement) &&
          !has(p.sheet(), "spell:" + cantrip.replaced),
          "The Bard's cantrip is replaced");
    check(prepared(p.sheet(), spell.replacement) && !prepared(p.sheet(), spell.replaced),
          "The Bard's spell is replaced");
    p.roundtrip();
}

// Blessed Warrior's and Druidic Warrior's cantrips: one may be replaced with
// another of the same list on a later level (#140, #147).
void warrior_cantrip_checks(std::string_view klass, std::string_view style)
{
    Party p(klass);
    auto second = p.party->default_advancement(p.id);
    second.fighting_style = style;
    p.advance(p.filled(std::move(second)));
    const auto options = p.options();
    check(options.cantrip_swap.replaceable.size() == 2 &&
          !options.cantrip_swap.replacements.empty(),
          std::string(style) + " offers both cantrips for replacement");
    const auto cantrip = first(options.cantrip_swap);
    p.advance(p.choice(cantrip, {}, {}));
    check(has(p.sheet(), "spell:" + cantrip.replacement) &&
          !has(p.sheet(), "spell:" + cantrip.replaced),
          std::string(style) + "'s cantrip is replaced");
    p.roundtrip();
}
} // namespace

int main()
{
    try
    {
        sorcerer_checks();
        warlock_checks();
        other_class_checks();
        bard_checks();
        warrior_cantrip_checks("paladin", "blessed_warrior");
        warrior_cantrip_checks("ranger", "druidic_warrior");
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Level swap tests passed\n";
}
