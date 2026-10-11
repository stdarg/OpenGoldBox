#include "../src/OpenGoldBox/cached_combat.h"
#include "opengold/srd5.h"
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

// The combat view serves every snapshot query from one cached snapshot. These
// checks keep the cache from serving a snapshot older than the combat.
namespace
{
void check(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}

std::unique_ptr<opengold::CombatDemo> training_demo()
{
    auto demo = std::make_unique<opengold::CombatDemo>(opengold::srd5::load(
            std::filesystem::path(OPENGOLD_SOURCE_DIR) / "data/rules/srd-5.2.1/combat.rules"));
    demo->training();
    return demo;
}

void snapshot_is_built_once_until_a_change()
{
    presentation::CachedCombat combat;
    check(!combat, "A view starts without a combat");
    combat = training_demo();
    check(combat && combat->has_combat(), "Training starts a combat");
    const auto &first = combat.snapshot();
    check(&combat.snapshot() == &first, "A second query reuses the cached snapshot");
    check(first.revision == combat->combat().snapshot().revision,
          "The cached snapshot matches the combat");
}

void a_change_drops_the_snapshot()
{
    presentation::CachedCombat combat;
    combat = training_demo();
    const auto before = combat.snapshot().revision;
    check(combat.change().submit(opengold::choose_demo_command(combat->combat())),
          "The demo accepts its own command");
    check(combat.snapshot().revision != before, "A submitted command is seen at once");
    check(combat.snapshot().revision == combat->combat().snapshot().revision,
          "The rebuilt snapshot matches the combat");
}

void a_new_demo_drops_the_snapshot()
{
    presentation::CachedCombat combat;
    combat = training_demo();
    check(combat.change().submit(opengold::choose_demo_command(combat->combat())),
          "The demo accepts its own command");
    const auto played = combat.snapshot().revision;
    combat = training_demo();
    const auto fresh = combat->combat().snapshot().revision;
    check(fresh != played, "A fresh training combat has not moved yet");
    check(combat.snapshot().revision == fresh, "A new demo is not served the old snapshot");
}
} // namespace

int main()
{
    try
    {
        snapshot_is_built_once_until_a_change();
        a_change_drops_the_snapshot();
        a_new_demo_drops_the_snapshot();
        std::cout << "Cached combat checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
