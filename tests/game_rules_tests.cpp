#include "game_rules.h"
#include "opengold/original.h"
#include "test_support.h"
#include <cstdlib>
#include <iostream>

using opengold::test::check;
using opengold::test::throws;

namespace
{
// The original rules read Pool of Radiance's folder (OPENGOLD_GAME_DIR).
RulesFiles rules_files()
{
    const char *game = std::getenv("OPENGOLD_GAME_DIR");
    return {std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/rules/srd-5.2.1/combat.rules",
            game ? std::filesystem::path(game) : std::filesystem::path{}};
}

const RulesFiles files = rules_files();

const GameRules original_add{GameRules::Set::original, opengold::original::Aging::add};

opengold::rules::CharacterDraft rolled(const opengold::CampaignParty &party,
                                       opengold::rules::CharacterDraft draft)
{
    opengold::RandomState random{3};
    party.character_rules()->roll_in_place(draft, random);
    return draft;
}

void a_new_game_plays_by_its_rules()
{
    const auto srd = new_campaign_party(GameRules{}, files);
    check(game_rules_of(*srd) == GameRules{} && srd->game_setting().empty() &&
              srd->identity().module != opengold::original::identity().module,
          "A game under the SRD rules plays by the SRD's module, with no setting");
    const auto original = new_campaign_party(original_add, files);
    check(game_rules_of(*original) == original_add &&
              original->identity() == opengold::original::identity() &&
              original->character_rules()->identity() == opengold::original::identity(),
          "A game under the original rules plays by their module and character rules");
    check(new_game_rules == GameRules{},
          "A new game plays by the SRD rules until the original rules can fight");
}

void a_save_loads_under_its_own_rules()
{
    for (const auto &rules : {GameRules{}, original_add})
    {
        const auto party = new_campaign_party(rules, files);
        opengold::rules::CharacterDraft draft;
        draft.name = "Tester";
        draft.gender = rules == GameRules{} ? "female" : "male";
        draft.race = rules == GameRules{} ? "human" : "dwarf";
        draft.character_class = "fighter";
        draft.alignment = rules == GameRules{} ? "neutral_good" : "lawful_good";
        if (rules == GameRules{})
        {
            draft.background = "soldier";
            draft.rolled = true;
            for (auto &roll : draft.rolls)
            {
                roll = {{6, 5, 4, 1}, 3};
            }
        }
        else
        {
            draft = rolled(*party, draft);
        }
        (void)party->add_pc(opengold::Character(*party->character_rules(), draft, {}));
        const auto saved = opengold::encode_campaign(*party, nullptr, "rules-check");
        auto loaded = load_campaign_under_its_rules(saved, files, "rules-check", {});
        check(game_rules_of(*loaded.party) == rules && loaded.saved.party.roster.size() == 1,
              "A save loads under the rules and setting it was made with");
        loaded.party->restore(std::move(loaded.saved.party));
        check(opengold::encode_campaign(*loaded.party, nullptr, "rules-check") == saved,
              "A loaded game saves again unchanged");
        check(decode_under_party_rules(saved, *party, "rules-check").party.roster.size() == 1,
              "A reload into the same game decodes under the party's rules");
    }
    check(throws(
              []
              {
                  (void)game_rules_of(
                      opengold::CampaignRules{opengold::original::identity(), "ad&d"});
              }),
          "An original-rules save with an unknown aging is refused");
}

// The creation screen outfits a new Pools of Darkness character under the
// original rules with the game's starting equipment, and under the SRD's
// with none (OPENGOLD_GOLDBOX_DIR, docs/GAMES.md).
void darkness_characters_are_outfitted_at_creation()
{
    const char *games = std::getenv("OPENGOLD_GOLDBOX_DIR");
    const auto darkness = opengold::GoldBoxGame::pools_of_darkness;
    const auto folder = games ? std::filesystem::path(games) / opengold::game_folder(darkness)
                              : std::filesystem::path{};
    if (!games || !std::filesystem::exists(folder / "ITEM0.DAX"))
    {
        std::cout << "Pools of Darkness's files are not here; its outfitting is not checked.\n";
        return;
    }
    const RulesFiles darkness_files{files.srd_pack, folder, darkness};
    const auto made = [&](const GameRules &rules)
    {
        const auto party = new_campaign_party(rules, darkness_files);
        opengold::rules::CharacterDraft draft;
        draft.name = "Vala";
        draft.gender = "male";
        draft.race = "human";
        draft.character_class = "fighter";
        draft.alignment = "neutral_good";
        if (rules.set == GameRules::Set::srd)
        {
            draft.background = "soldier";
            draft.rolled = true;
            for (auto &roll : draft.rolls)
            {
                roll = {{6, 5, 4, 1}, 3};
            }
        }
        else
        {
            draft = rolled(*party, draft);
        }
        const auto id = party->add_pc(opengold::Character(*party->character_rules(), draft, {}));
        outfit_for_game(*party, id, darkness_files);
        return party->member(id);
    };
    const auto original = made(original_add);
    check(original.character.inventory().items().size() == 5 && original.equipped.size() == 4,
          "A Darkness fighter under the original rules starts with five items, four readied");
    check(made(GameRules{}).character.inventory().items().empty(),
          "Under the SRD rules it starts with none of them");
}
} // namespace

int main()
{
    try
    {
        a_new_game_plays_by_its_rules();
        a_save_loads_under_its_own_rules();
        darkness_characters_are_outfitted_at_creation();
        std::cout << "Game rules tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
