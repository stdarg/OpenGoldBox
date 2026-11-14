#ifndef OPENGOLD_DEFAULT_PARTY_H
#define OPENGOLD_DEFAULT_PARTY_H
#include "opengold/campaign_party.h"
#include "opengold/character_art.h"
#include "opengold/companions.h"
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace opengold
{
// An item a default party member starts with: under the original rules an
// item type of its game (its ITEMS file's numbering) and the name it shows;
// under the SRD an equipment key. Readied ones are worn or held.
struct DefaultItem
{
    int type{-1};
    std::string key, name;
    unsigned quantity{1};
    bool readied{};
};

// One of a game's six default party members (data/default_parties/<game
// folder>.json, docs/DEFAULT-PARTIES.md): the same person under each rule
// set, with one look, and the spells it starts with memorized or prepared.
struct DefaultPartyMember
{
    std::string name, gender;
    // The portrait and the combat icon; the icon's size follows the race.
    por::CharacterAppearance appearance;

    struct Original
    {
        std::string race, character_class, alignment;
        // The rolls the creation's own roll is made into (set_rolled_scores).
        rules::AbilityArray<int> scores{};
        std::vector<DefaultItem> items;
        std::vector<std::string> spells;
    } original;

    struct Srd
    {
        // Made as a companion is (build_companion), at level 1, with no items.
        CompanionDefinition character;
        std::vector<DefaultItem> items;
    } srd;
};

// Reads one game's default party. Throws std::runtime_error, naming the file,
// the member and the field, on a file the game cannot use.
[[nodiscard]] std::vector<DefaultPartyMember> load_default_party(const std::filesystem::path &file);

// Adds the default party to `party`, each member made by the party's creation
// rules as a new character is: under the original rules its fixed rolls in the
// creation's own roll (each game's starting experience, training, gold and
// platinum), its items given readied, and its spells memorized at a Long Rest;
// under the SRD as a level-1 companion with 3d6 x 10 gold pieces. Their items
// cost nothing. Throws, leaving the party as it was, when the rules refuse a
// member.
void add_default_party(CampaignParty &party, std::span<const DefaultPartyMember> members);
} // namespace opengold
#endif
