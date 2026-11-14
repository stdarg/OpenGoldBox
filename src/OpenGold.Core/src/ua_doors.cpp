// The doors of an Unlimited Adventures dungeon as the kit opens them
// (docs/games/ua.md, Locked doors; the reconstruction's move.c): a step into a
// locked door asks "Blocked:" and offers Bash, Pick and Knock; a secret door
// says so as the party goes through it.
#include "opengold/ua_session.h"

#include <array>
#include <optional>
#include <utility>

namespace opengold::ua
{
namespace
{

using rules::Message;

// The original rules' class slot of a thief.
constexpr std::size_t thief_slot = 6;
// The kit's Knock (spell 1Fh), the original rules' number too.
constexpr std::string_view knock_spell = "knock";

// A d`sides` roll of at most `needed`: the chance of forcing a door.
struct ForceChance
{
    unsigned needed{}, sides{};
};

[[nodiscard]] bool wizard_locked(WallKind kind) noexcept
{
    return kind == WallKind::locked_wizard || kind == WallKind::locked_wizard_secret;
}

// The chance that a member of this Strength (and exceptional Strength, 1 to
// 100 for 18/01 to 18/00) forces a door (move.c, door_bash); none below the
// table. A wizard lock takes more.
[[nodiscard]] std::optional<ForceChance> force_chance(int strength, int exceptional,
                                                      bool wizard_lock)
{
    if (wizard_lock)
    {
        if (strength == 18)
        {
            if (exceptional == 100)
            {
                return ForceChance{2, 6};
            }
            return exceptional >= 91 ? std::optional{ForceChance{1, 6}} : std::nullopt;
        }
        if (strength < 18)
        {
            return std::nullopt;
        }
        if (strength <= 20)
        {
            return ForceChance{3, 6};
        }
        if (strength <= 22)
        {
            return ForceChance{4, 6};
        }
        if (strength == 23)
        {
            return ForceChance{5, 6};
        }
        return strength == 24 ? ForceChance{7, 8} : ForceChance{1, 1};
    }
    if (strength < 3)
    {
        return std::nullopt;
    }
    if (strength <= 7)
    {
        return ForceChance{1, 6};
    }
    if (strength <= 15)
    {
        return ForceChance{2, 6};
    }
    if (strength <= 17)
    {
        return ForceChance{3, 6};
    }
    if (strength == 18)
    {
        if (exceptional <= 50)
        {
            return ForceChance{3, 6};
        }
        return exceptional <= 98 ? ForceChance{4, 6} : ForceChance{5, 6};
    }
    if (strength <= 20)
    {
        return ForceChance{7, 8};
    }
    if (strength == 21)
    {
        return ForceChance{9, 10};
    }
    if (strength <= 23)
    {
        return ForceChance{11, 12};
    }
    return strength == 24 ? ForceChance{19, 20} : ForceChance{1, 1};
}

} // namespace

bool UaSession::door_ahead(WallKind kind)
{
    switch (kind)
    {
    case WallKind::open_secret:
        say_kit({"A secret door!", {}});
        wait_for_return(
            [this]
            {
                step_ahead();
            });
        refresh_snapshot();
        return true;
    case WallKind::locked_secret:
    case WallKind::locked_wizard_secret:
        say_kit({"A secret door!", {}});
        wait_for_return(
            [this, kind]
            {
                door_menu(kind);
            });
        refresh_snapshot();
        return true;
    case WallKind::locked:
    case WallKind::locked_wizard:
    case WallKind::false_door:
        door_menu(kind);
        refresh_snapshot();
        return true;
    default:
        return false;
    }
}

// The kit offers each of Bash and Pick once until the party steps on, Pick
// only to a party with a thief, Knock while a member has it memorized, and
// always Exit.
void UaSession::door_menu(WallKind kind)
{
    enum class Way : std::uint8_t
    {
        bash,
        pick,
        knock,
        exit,
    };
    std::vector<Way> ways;
    std::vector<Message> words;
    if (!bash_tried_)
    {
        ways.push_back(Way::bash);
        words.push_back({"Bash", {}});
    }
    if (!pick_tried_ && party_has_thief())
    {
        ways.push_back(Way::pick);
        words.push_back({"Pick", {}});
    }
    if (party_->can_try_door(DoorMethod::knock))
    {
        ways.push_back(Way::knock);
        words.push_back({"Knock", {}});
    }
    ways.push_back(Way::exit);
    words.push_back({"Exit", {}});
    say_kit({"Blocked:", {}});
    ask_kit(std::move(words),
            [this, kind, ways](std::size_t choice)
            {
                bool opened = false;
                switch (ways[choice])
                {
                case Way::bash:
                    opened = bash_door(kind);
                    break;
                case Way::pick:
                    opened = pick_lock(kind);
                    break;
                case Way::knock:
                    opened = knock(kind);
                    break;
                case Way::exit:
                    break;
                }
                if (opened)
                {
                    step_ahead();
                }
            });
}

// Each member who can act tries in turn until one forces the door; a false
// door never gives. A failed bash is not offered again until the party steps.
bool UaSession::bash_door(WallKind kind)
{
    bool forced = false;
    for (const auto id : active_members())
    {
        const auto &sheet = party_->member(id).character.sheet();
        const int exceptional = sheet.original ? sheet.original->exceptional_strength : 0;
        const auto chance =
            force_chance(sheet.scores[rules::Ability::strength], exceptional, wizard_locked(kind));
        if (chance && roll(chance->sides) <= chance->needed)
        {
            forced = true;
            break;
        }
    }
    if (kind == WallKind::false_door)
    {
        forced = false;
    }
    bash_tried_ = bash_tried_ || !forced;
    return forced;
}

// Each member who can act tries its open-locks chance until one picks it;
// once until the party steps on. No pick opens a wizard lock or a false door.
bool UaSession::pick_lock(WallKind kind)
{
    pick_tried_ = true;
    if (wizard_locked(kind) || kind == WallKind::false_door)
    {
        return false;
    }
    for (const auto id : active_members())
    {
        const auto &sheet = party_->member(id).character.sheet();
        const int chance = sheet.original ? original::skill_chance(sheet.original->thief_skills,
                                                                   original::ThiefSkill::open_locks)
                                          : 0;
        if (static_cast<int>(roll(100)) <= chance)
        {
            return true;
        }
    }
    return false;
}

// The first member with Knock memorized casts it, spending it; it opens any
// lock but a false door's.
bool UaSession::knock(WallKind kind)
{
    (void)party_->cast_exploration_spell(knock_spell);
    return kind != WallKind::false_door;
}

bool UaSession::party_has_thief() const
{
    for (const auto id : party_members())
    {
        const auto &sheet = party_->member(id).character.sheet();
        if (sheet.original && sheet.original->class_levels[thief_slot] > 0)
        {
            return true;
        }
    }
    return false;
}

} // namespace opengold::ua
