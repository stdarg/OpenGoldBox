// Champion critical hits with an optional mastery, built with the current writer.
namespace mastery_choice_checks
{
using namespace mastery_combat_checks;

// Returns the checkpoint just before a level-3 Champion's attack with `weapon` that
// actually lands a critical hit, so callers see both the mastery and Champion options.
std::string critical_before(std::string_view weapon)
{
    auto r = module();
    CampaignParty p(module());
    auto h = hero(std::string(weapon), "fighter", "soldier");
    h.inventory().add(std::string(weapon), "Mastery weapon");
    p.add_pc(h);
    p.equip(1, 1);
    p.award_experience(900, "choice-baseline");
    while (p.member(1).character.sheet().level < 3)
        p.advance(1, p.default_advancement(1));
    auto actors = p.participants();
    actors.front().cell = {1, 1};
    const bool ranged = weapon == "longbow";
    actors.push_back({99, "vanguard", "Target", 1, {ranged ? 3 : 2, 1}});
    for (unsigned seed = 1; seed <= 128; ++seed)
    {
        auto c = r->create({{12, 8, std::vector<std::uint8_t>(96)}, actors, 777}, seed);
        turn(*c, 1);
        const auto before = c->save();
        act(*c, ranged ? "ranged" : "melee", 99);
        if (arg(result(*c), "hit") == "CRITICAL")
            return before;
    }
    throw std::runtime_error("No critical hit for an optional mastery weapon");
}
} // namespace mastery_choice_checks
