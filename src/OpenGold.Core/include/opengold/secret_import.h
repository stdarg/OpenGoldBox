#ifndef OPENGOLD_SECRET_IMPORT_H
#define OPENGOLD_SECRET_IMPORT_H

#include "opengold/campaign_party.h"

#include <filesystem>

namespace opengold
{

// How Secret of the Silver Blades takes in a Curse of the Azure Bonds
// character's items (its Add Character's Curse; docs/games/secret.md,
// Bringing a Curse party across). Both games keep an item in the same 63-byte
// record, but Secret numbers its item types and name words as Pool of
// Radiance does, not as Curse. No source of Secret's import is at hand, so an
// item is renumbered by what it is called: each name word becomes Secret's
// word of the same letters; its type becomes the type of a Secret item of
// those same words (its treasure and creature item lists), or else the type
// Secret names with the word Curse names the item's type with, as both games
// name their arms and armor. An item left without a word or a type stays
// behind. Readied items stay readied. Reads Curse's START.EXE and Secret's
// START.EXE, ITEMS and item lists; throws when they cannot be read.
[[nodiscard]] CampaignParty::ItemConversion
curse_items_for_secret(const std::filesystem::path &curse_folder,
                       const std::filesystem::path &secret_folder);

} // namespace opengold

#endif
