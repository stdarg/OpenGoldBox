#ifndef OPENGOLD_TEST_CAMPAIGN_FIXTURE_H
#define OPENGOLD_TEST_CAMPAIGN_FIXTURE_H
#include "opengold/character_rules.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <regex>
#include <stdexcept>
namespace opengold::test
{
// Independent expected migration: insert the one fixed level-one Wizard grant
// immediately after Spellcasting, preserving all other serialized bytes.
inline std::string with_arcane_recovery_grants(std::string body)
{
    const std::string marker = "\"feature:spellcasting\" \"class:wizard\" 1 0";
    const std::string soldier = "\"feat:savage_attacker\" \"background:soldier\" 1 0 ";
    std::size_t search = 0;
    while (true)
    {
        auto at = body.find(marker, search);
        if (at == std::string::npos)
            break;
        if (at >= soldier.size() && body.substr(at - soldier.size(), soldier.size()) == soldier)
            at -= soldier.size();
        if (at < 3 || body[at - 1] != ' ')
            throw std::runtime_error("Missing frozen Wizard grant count");
        auto begin = at - 2;
        while (begin && body[begin - 1] >= '0' && body[begin - 1] <= '9')
            --begin;
        std::istringstream input(body.substr(begin));
        unsigned count{};
        input >> count;
        if (!input || count > 32)
            throw std::runtime_error("Invalid frozen Wizard grant ledger");
        std::vector<rules::FeatureGrant> grants;
        for (unsigned i = 0; i < count; ++i)
        {
            rules::FeatureGrant g;
            unsigned choices{};
            input >> std::quoted(g.id) >> std::quoted(g.source_id) >> g.level >> choices;
            if (!input || choices > 6)
                throw std::runtime_error("Invalid frozen Wizard grant");
            for (unsigned n = 0; n < choices; ++n)
            {
                std::string k, v;
                input >> std::quoted(k) >> std::quoted(v);
                g.choices.emplace(k, v);
            }
            grants.push_back(std::move(g));
        }
        if (!input)
            throw std::runtime_error("Truncated frozen Wizard grants");
        const auto length = static_cast<std::size_t>(input.tellg());
        if (std::none_of(grants.begin(), grants.end(),
                         [](const auto &g)
                         {
                             return g.id == "feature:arcane_recovery";
                         }))
        {
            const auto spellcasting = std::find_if(grants.begin(), grants.end(),
                                                   [](const auto &g)
                                                   {
                                                       return g.id == "feature:spellcasting";
                                                   });
            if (spellcasting == grants.end())
                throw std::runtime_error("Missing frozen Spellcasting grant");
            grants.insert(std::next(spellcasting),
                          {"feature:arcane_recovery", "class:wizard", 1, {}});
        }
        std::ostringstream out;
        out << grants.size();
        for (const auto &g : grants)
        {
            out << ' ' << std::quoted(g.id) << ' ' << std::quoted(g.source_id) << ' ' << g.level
                << ' ' << g.choices.size();
            for (const auto &[k, v] : g.choices)
                out << ' ' << std::quoted(k) << ' ' << std::quoted(v);
        }
        const auto next = out.str();
        body.replace(begin, length, next);
        search = begin + next.size();
    }
    return body;
}
// Add independently specified fixed background packages to authored campaign
// fixtures. Locate each draft's background and its following grant ledger;
// never invoke current grant generation or the production save decoder.
inline std::string with_background_training_grants(std::string body, unsigned packages = 3)
{
    const std::regex draft(
        R"re("(?:dragonborn|dwarf|elf|gnome|goliath|halfling|human|orc|tiefling)" "(?:female|male|nonbinary)" "(?:barbarian|bard|cleric|druid|fighter|monk|paladin|ranger|rogue|sorcerer|warlock|wizard)" )re");
    const std::regex first_grant(
        R"re("(?:feature:|feat:|trait:|language:)[^"]+" "[^"]+" [0-9]+ [0-9]+)re");
    std::size_t search = 0;
    std::smatch match;
    while (search < body.size())
    {
        auto rest = body.substr(search);
        if (!std::regex_search(rest, match, draft))
            break;
        const auto start = search + match.position();
        std::istringstream fields(body.substr(start));
        std::string field, background;
        for (unsigned n = 0; n < 6; ++n)
        {
            fields >> std::quoted(field);
            if (n == 4)
                background = field;
        }
        if (!fields)
            throw std::runtime_error("Malformed frozen draft identity");
        search = start + static_cast<std::size_t>(fields.tellg());
        if (!((background == "sage" && (packages & 1)) ||
              ((background == "acolyte" || background == "soldier") && (packages & 2))))
            continue;
        rest = body.substr(search);
        if (!std::regex_search(rest, match, first_grant))
            throw std::runtime_error("Missing background ledger");
        const auto first = search + match.position();
        auto begin = first - 2;
        while (begin && body[begin - 1] >= '0' && body[begin - 1] <= '9')
            --begin;
        std::istringstream input(body.substr(begin));
        unsigned count{};
        input >> count;
        if (!input || !count || count > 128)
            throw std::runtime_error("Invalid background ledger count");
        std::vector<rules::FeatureGrant> grants;
        for (unsigned n = 0; n < count; ++n)
        {
            rules::FeatureGrant g;
            unsigned choices{};
            input >> std::quoted(g.id) >> std::quoted(g.source_id) >> g.level >> choices;
            if (!input || choices > 6)
                throw std::runtime_error("Malformed frozen background grant");
            for (unsigned k = 0; k < choices; ++k)
            {
                std::string key, value;
                input >> std::quoted(key) >> std::quoted(value);
                g.choices.emplace(key, value);
            }
            grants.push_back(std::move(g));
        }
        if (!input)
            throw std::runtime_error("Truncated background ledger");
        const auto length = static_cast<std::size_t>(input.tellg());
        const auto common =
            std::find_if(grants.begin(), grants.end(),
                         [](const auto &g)
                         {
                             return g.id == "language:common" && g.source_id == "origin:languages";
                         });
        std::vector<rules::FeatureGrant> added;
        if (background == "sage")
            added = {{"skill:arcana", "background:sage", 1, {}},
                     {"skill:history", "background:sage", 1, {}},
                     {"tool:calligraphers_supplies", "background:sage", 1, {}}};
        else if (background == "acolyte")
            added = {{"skill:insight", "background:acolyte", 1, {}},
                     {"skill:religion", "background:acolyte", 1, {}},
                     {"tool:calligraphers_supplies", "background:acolyte", 1, {}}};
        else
            added = {{"skill:athletics", "background:soldier", 1, {}},
                     {"skill:intimidation", "background:soldier", 1, {}}};
        if (common == grants.end() || std::any_of(added.begin(), added.end(),
                                                  [&](const auto &g)
                                                  {
                                                      return std::find(grants.begin(), grants.end(),
                                                                       g) != grants.end();
                                                  }))
            throw std::runtime_error("Unexpected prior background grants");
        grants.insert(common + 1, added.begin(), added.end());
        std::ostringstream out;
        out << grants.size();
        for (const auto &g : grants)
        {
            out << ' ' << std::quoted(g.id) << ' ' << std::quoted(g.source_id) << ' ' << g.level
                << ' ' << g.choices.size();
            for (const auto &[k, v] : g.choices)
                out << ' ' << std::quoted(k) << ' ' << std::quoted(v);
        }
        const auto next = out.str();
        body.replace(begin, length, next);
        search = begin + next.size();
    }
    return body;
}
// Add only the fixed Druid grant after Common, preserving all other bytes.
inline std::string with_druid_herbalism_grants(std::string body)
{
    const std::regex draft(
        R"re("(?:dragonborn|dwarf|elf|gnome|goliath|halfling|human|orc|tiefling)" "(?:female|male|nonbinary)" "druid" )re");
    const std::regex first_grant(
        R"re("(?:feature:|feat:|trait:|language:)[^"]+" "[^"]+" [0-9]+ [0-9]+)re");
    std::size_t search = 0;
    std::smatch match;
    while (search < body.size())
    {
        auto rest = body.substr(search);
        if (!std::regex_search(rest, match, draft))
            break;
        const auto start = search + match.position();
        std::istringstream fields(body.substr(start));
        std::string field;
        for (unsigned n = 0; n < 6; ++n)
            fields >> std::quoted(field);
        if (!fields)
            throw std::runtime_error("Malformed frozen draft identity");
        search = start + static_cast<std::size_t>(fields.tellg());
        rest = body.substr(search);
        if (!std::regex_search(rest, match, first_grant))
            throw std::runtime_error("Missing Druid ledger");
        const auto first = search + match.position();
        auto begin = first - 2;
        while (begin && body[begin - 1] >= '0' && body[begin - 1] <= '9')
            --begin;
        std::istringstream input(body.substr(begin));
        unsigned count{};
        input >> count;
        if (!input || !count || count > 128)
            throw std::runtime_error("Invalid Druid ledger count");
        std::vector<rules::FeatureGrant> grants;
        for (unsigned n = 0; n < count; ++n)
        {
            rules::FeatureGrant g;
            unsigned choices{};
            input >> std::quoted(g.id) >> std::quoted(g.source_id) >> g.level >> choices;
            if (!input || choices > 6)
                throw std::runtime_error("Malformed frozen Druid grant");
            for (unsigned k = 0; k < choices; ++k)
            {
                std::string key, value;
                input >> std::quoted(key) >> std::quoted(value);
                g.choices.emplace(key, value);
            }
            grants.push_back(std::move(g));
        }
        if (!input)
            throw std::runtime_error("Truncated Druid ledger");
        const auto length = static_cast<std::size_t>(input.tellg());
        const auto common =
            std::find_if(grants.begin(), grants.end(),
                         [](const auto &g)
                         {
                             return g.id == "language:common" && g.source_id == "origin:languages";
                         });
        const std::vector<rules::FeatureGrant> added{{"tool:herbalism_kit", "class:druid", 1, {}}};
        if (common == grants.end() || std::any_of(added.begin(), added.end(),
                                                  [&](const auto &g)
                                                  {
                                                      return std::find(grants.begin(), grants.end(),
                                                                       g) != grants.end();
                                                  }))
            throw std::runtime_error("Unexpected prior Druid grants");
        grants.insert(common + 1, added.begin(), added.end());
        std::ostringstream out;
        out << grants.size();
        for (const auto &g : grants)
        {
            out << ' ' << std::quoted(g.id) << ' ' << std::quoted(g.source_id) << ' ' << g.level
                << ' ' << g.choices.size();
            for (const auto &[k, v] : g.choices)
                out << ' ' << std::quoted(k) << ' ' << std::quoted(v);
        }
        const auto next = out.str();
        body.replace(begin, length, next);
        search = begin + next.size();
    }
    return body;
}
// One fixed level-one Sneak grant before species/training; other bytes preserved.
inline std::string with_sneak_attack_grants(std::string body)
{
    const std::regex draft(
        R"re("(?:dragonborn|dwarf|elf|gnome|goliath|halfling|human|orc|tiefling)" "(?:female|male|nonbinary)" "rogue" )re");
    const std::regex first_grant(
        R"re("(?:feature:|feat:|trait:|language:)[^"]+" "[^"]+" [0-9]+ [0-9]+)re");
    std::size_t search = 0;
    std::smatch match;
    while (search < body.size())
    {
        auto rest = body.substr(search);
        if (!std::regex_search(rest, match, draft))
            break;
        const auto start = search + match.position();
        std::istringstream fields(body.substr(start));
        std::string field;
        for (unsigned n = 0; n < 6; ++n)
            fields >> std::quoted(field);
        if (!fields)
            throw std::runtime_error("Malformed frozen draft identity");
        search = start + static_cast<std::size_t>(fields.tellg());
        rest = body.substr(search);
        if (!std::regex_search(rest, match, first_grant))
            throw std::runtime_error("Missing Rogue ledger");
        const auto first = search + match.position();
        auto begin = first - 2;
        while (begin && body[begin - 1] >= '0' && body[begin - 1] <= '9')
            --begin;
        std::istringstream input(body.substr(begin));
        unsigned count{};
        input >> count;
        if (!input || !count || count > 128)
            throw std::runtime_error("Invalid Rogue ledger count");
        std::vector<rules::FeatureGrant> grants;
        for (unsigned n = 0; n < count; ++n)
        {
            rules::FeatureGrant g;
            unsigned choices{};
            input >> std::quoted(g.id) >> std::quoted(g.source_id) >> g.level >> choices;
            if (!input || choices > 6)
                throw std::runtime_error("Malformed frozen Rogue grant");
            for (unsigned k = 0; k < choices; ++k)
            {
                std::string key, value;
                input >> std::quoted(key) >> std::quoted(value);
                g.choices.emplace(key, value);
            }
            grants.push_back(std::move(g));
        }
        if (!input)
            throw std::runtime_error("Truncated Rogue ledger");
        const auto length = static_cast<std::size_t>(input.tellg());
        const auto common =
            std::find_if(grants.begin(), grants.end(),
                         [](const auto &g)
                         {
                             return g.id == "language:common" && g.source_id == "origin:languages";
                         });
        const std::vector<rules::FeatureGrant> added{
            {"feature:sneak_attack", "class:rogue", 1, {}}};
        if (common == grants.end() || std::any_of(added.begin(), added.end(),
                                                  [&](const auto &g)
                                                  {
                                                      return std::find(grants.begin(), grants.end(),
                                                                       g) != grants.end();
                                                  }))
            throw std::runtime_error("Unexpected prior Rogue grants");
        const auto position =
            std::find_if(grants.begin(), grants.end(),
                         [](const auto &g)
                         {
                             return g.id.starts_with("trait:") || g.id == "language:common";
                         });
        grants.insert(position, added.begin(), added.end());
        std::ostringstream out;
        out << grants.size();
        for (const auto &g : grants)
        {
            out << ' ' << std::quoted(g.id) << ' ' << std::quoted(g.source_id) << ' ' << g.level
                << ' ' << g.choices.size();
            for (const auto &[k, v] : g.choices)
                out << ' ' << std::quoted(k) << ' ' << std::quoted(v);
        }
        const auto next = out.str();
        body.replace(begin, length, next);
        search = begin + next.size();
    }
    return body;
}
// Independently add the three fixed Champion grants to normalized v11+ fixture
// bodies. Read attained level from the documented draft/appearance prefix, not
// from production replay or from the current result being compared.
inline std::string with_champion_grants(std::string body)
{
    const std::regex draft(
        R"re("(?:dragonborn|dwarf|elf|gnome|goliath|halfling|human|orc|tiefling)" "(?:female|male|nonbinary)" "fighter" )re");
    const std::regex first_grant(
        R"re("(?:feature:|feat:|trait:|language:)[^"]+" "[^"]+" [0-9]+ [0-9]+)re");
    std::size_t search = 0;
    std::smatch match;
    while (search < body.size())
    {
        auto rest = body.substr(search);
        if (!std::regex_search(rest, match, draft))
            break;
        const auto start = search + match.position();
        std::istringstream fields(body.substr(start));
        std::string text;
        unsigned count{}, n{}, level{};
        for (unsigned i = 0; i < 6; ++i)
            fields >> std::quoted(text);
        fields >> count;
        for (unsigned i = 0; i < count; ++i)
            fields >> std::quoted(text);
        for (unsigned i = 0; i < 38; ++i)
            fields >> n;
        fields >> count;
        for (unsigned i = 0; i < count; ++i)
        {
            unsigned choices{};
            fields >> std::quoted(text) >> choices;
            for (unsigned j = 0; j < choices; ++j)
                fields >> std::quoted(text);
        }
        fields >> n;
        if (n)
        {
            fields >> count;
            for (unsigned i = 0; i < count; ++i)
                fields >> std::quoted(text);
        }
        for (unsigned i = 0; i < 17; ++i)
            fields >> n;
        fields >> std::quoted(text) >> level;
        if (!fields || level < 1 || level > 4)
            throw std::runtime_error("Malformed frozen Champion draft/level");
        search = start + static_cast<std::size_t>(fields.tellg());
        if (level < 3)
            continue;
        rest = body.substr(search);
        if (!std::regex_search(rest, match, first_grant))
            throw std::runtime_error("Missing frozen Champion ledger");
        const auto first = search + match.position();
        auto begin = first - 2;
        while (begin && body[begin - 1] >= '0' && body[begin - 1] <= '9')
            --begin;
        std::istringstream input(body.substr(begin));
        input >> count;
        if (!input || !count || count > 128)
            throw std::runtime_error("Invalid frozen Champion ledger");
        std::vector<rules::FeatureGrant> grants;
        for (unsigned i = 0; i < count; ++i)
        {
            rules::FeatureGrant g;
            unsigned choices{};
            input >> std::quoted(g.id) >> std::quoted(g.source_id) >> g.level >> choices;
            if (!input || choices > 6)
                throw std::runtime_error("Malformed frozen Champion grant");
            for (unsigned j = 0; j < choices; ++j)
            {
                std::string key, value;
                input >> std::quoted(key) >> std::quoted(value);
                g.choices.emplace(key, value);
            }
            grants.push_back(std::move(g));
        }
        if (!input)
            throw std::runtime_error("Truncated frozen Champion ledger");
        const auto length = static_cast<std::size_t>(input.tellg());
        if (std::any_of(grants.begin(), grants.end(),
                        [](const auto &g)
                        {
                            return g.id == "subclass:champion";
                        }))
            throw std::runtime_error("Champion fixture already upgraded");
        const std::vector<rules::FeatureGrant> added{
            {"subclass:champion", "class:fighter", 3, {}},
            {"feature:improved_critical", "subclass:fighter:champion", 3, {}},
            {"feature:remarkable_athlete", "subclass:fighter:champion", 3, {}}};
        grants.insert(std::find_if(grants.begin(), grants.end(),
                                   [](const auto &g)
                                   {
                                       return g.level > 3;
                                   }),
                      added.begin(), added.end());
        std::ostringstream out;
        out << grants.size();
        for (const auto &g : grants)
        {
            out << ' ' << std::quoted(g.id) << ' ' << std::quoted(g.source_id) << ' ' << g.level
                << ' ' << g.choices.size();
            for (const auto &[k, v] : g.choices)
                out << ' ' << std::quoted(k) << ' ' << std::quoted(v);
        }
        const auto next = out.str();
        body.replace(begin, length, next);
        search = begin + next.size();
    }
    return body;
}
// The fixture author supplies one attained-level decision per Fighter ledger.
// Add fixed level-two grants, then independently migrate Champion entitlements.
inline std::string with_action_surge_grants(std::string body, std::initializer_list<bool> eligible,
                                            bool only_tactical = false)
{
    const std::string marker = "\"feature:fighting_style\" \"class:fighter\" 1 0";
    auto wanted = eligible.begin();
    std::size_t search = 0;
    while (true)
    {
        auto at = body.find(marker, search);
        if (at == body.npos)
            break;
        bool add{};
        if (!only_tactical)
        {
            if (wanted == eligible.end())
                throw std::runtime_error("Unexpected Fighter ledger");
            add = *wanted++;
        }
        const std::string soldier = "\"feat:savage_attacker\" \"background:soldier\" 1 0 ";
        if (at >= soldier.size() && body.substr(at - soldier.size(), soldier.size()) == soldier)
            at -= soldier.size();
        auto begin = at - 2;
        while (begin && body[begin - 1] >= '0' && body[begin - 1] <= '9')
            --begin;
        std::istringstream input(body.substr(begin));
        unsigned count{};
        input >> count;
        if (!input || count > 32)
            throw std::runtime_error("Invalid frozen Fighter grant count");
        std::vector<rules::FeatureGrant> grants;
        for (unsigned n = 0; n < count; ++n)
        {
            rules::FeatureGrant g;
            unsigned choices{};
            input >> std::quoted(g.id) >> std::quoted(g.source_id) >> g.level >> choices;
            if (!input || choices > 6)
                throw std::runtime_error("Malformed frozen Fighter grant");
            for (unsigned i = 0; i < choices; ++i)
            {
                std::string k, v;
                input >> std::quoted(k) >> std::quoted(v);
                g.choices.emplace(k, v);
            }
            grants.push_back(std::move(g));
        }
        if (!input)
            throw std::runtime_error("Truncated frozen Fighter ledger");
        const auto length = static_cast<std::size_t>(input.tellg());
        if (only_tactical)
            add = std::any_of(grants.begin(), grants.end(),
                              [](const auto &g)
                              {
                                  return g.id == "feature:action_surge";
                              }) &&
                  !std::any_of(grants.begin(), grants.end(),
                               [](const auto &g)
                               {
                                   return g.id == "feature:tactical_mind";
                               });
        if (!add)
        {
            search = begin + length;
            continue;
        }
        if (!only_tactical)
            grants.insert(std::find_if(grants.begin(), grants.end(),
                                       [](const auto &g)
                                       {
                                           return g.level > 2;
                                       }),
                          {"feature:action_surge", "class:fighter", 2, {}});
        grants.insert(std::find_if(grants.begin(), grants.end(),
                                   [](const auto &g)
                                   {
                                       return g.level > 2;
                                   }),
                      {"feature:tactical_mind", "class:fighter", 2, {}});
        std::ostringstream out;
        out << grants.size();
        for (const auto &g : grants)
        {
            out << ' ' << std::quoted(g.id) << ' ' << std::quoted(g.source_id) << ' ' << g.level
                << ' ' << g.choices.size();
            for (const auto &[k, v] : g.choices)
                out << ' ' << std::quoted(k) << ' ' << std::quoted(v);
        }
        const auto next = out.str();
        body.replace(begin, length, next);
        search = begin + next.size();
    }
    if (wanted != eligible.end())
        throw std::runtime_error("Missing frozen Fighter ledger");
    return with_arcane_recovery_grants(with_champion_grants(std::move(body)));
}
// Post-Action-Surge fixtures already carry the level-two eligibility evidence.
inline std::string with_tactical_mind_grants(std::string body)
{
    return with_action_surge_grants(std::move(body), {}, true);
}
// v11 adds one absent optional-cantrip field to each v9/v10 creation draft.
// Locate drafts by their independently known identity grammar, then skip the
// published draft fields. Preserve every other byte of these frozen bodies.
inline std::string with_legacy_cantrip_choices(std::string body)
{
    const std::regex draft(
        R"re("(?:dragonborn|dwarf|elf|gnome|goliath|halfling|human|orc|tiefling)" "(?:female|male|nonbinary)" "(?:barbarian|bard|cleric|druid|fighter|monk|paladin|ranger|rogue|sorcerer|warlock|wizard)" )re");
    std::size_t search = 0;
    std::smatch match;
    while (search < body.size())
    {
        const auto rest = body.substr(search);
        if (!std::regex_search(rest, match, draft))
            break;
        const auto start = search + match.position();
        std::istringstream in(body.substr(start));
        std::string text;
        unsigned n{}, count{};
        for (unsigned i = 0; i < 6; ++i)
            in >> std::quoted(text);
        in >> count;
        for (unsigned i = 0; i < count; ++i)
            in >> std::quoted(text);
        for (unsigned i = 0; i < 38; ++i)
            in >> n;
        in >> count;
        for (unsigned i = 0; i < count; ++i)
        {
            unsigned choices{};
            in >> std::quoted(text) >> choices;
            for (unsigned j = 0; j < choices; ++j)
                in >> std::quoted(text);
        }
        if (!in)
            throw std::runtime_error("Malformed frozen creation draft");
        const auto at = start + static_cast<std::size_t>(in.tellg());
        body.insert(at, " 0");
        search = at + 2;
    }
    return body;
}
// Independent expected migration for authored Wizard fixtures: their ledger
// begins with Spellcasting, optionally preceded by Soldier Savage Attacker. Add only the two known
// old preset selections, leaving all other campaign bytes untouched. No save decoder or current
// grant generator is used to construct this oracle.
inline std::string with_initial_wizard_spell_grants(std::string body)
{
    const std::string marker = "\"feature:spellcasting\" \"class:wizard\" 1 0";
    std::size_t search = 0;
    while (true)
    {
        auto at = body.find(marker, search);
        if (at == std::string::npos)
            break;
        const std::string soldier = "\"feat:savage_attacker\" \"background:soldier\" 1 0 ";
        if (at >= soldier.size() && body.substr(at - soldier.size(), soldier.size()) == soldier)
            at -= soldier.size();
        if (at < 3 || body[at - 1] != ' ')
            throw std::runtime_error("Missing frozen grant count");
        auto begin = at - 2;
        while (begin && body[begin - 1] >= '0' && body[begin - 1] <= '9')
            --begin;
        std::istringstream input(body.substr(begin));
        unsigned count{};
        input >> count;
        if (!input || count > 32)
            throw std::runtime_error("Unexpected frozen grant ledger");
        std::vector<rules::FeatureGrant> grants;
        for (unsigned n = 0; n < count; ++n)
        {
            rules::FeatureGrant g;
            unsigned choices{};
            input >> std::quoted(g.id) >> std::quoted(g.source_id) >> g.level >> choices;
            if (!input || choices > 6)
                throw std::runtime_error("Malformed frozen grant");
            for (unsigned i = 0; i < choices; ++i)
            {
                std::string k, v;
                input >> std::quoted(k) >> std::quoted(v);
                g.choices.emplace(k, v);
            }
            grants.push_back(std::move(g));
        }
        if (!input)
            throw std::runtime_error("Truncated frozen grant ledger");
        const auto length = static_cast<std::size_t>(input.tellg());
        grants.push_back(
            {"spell:fire_bolt", "class:wizard:spellcasting", 1, {{"access", "cantrip"}}});
        grants.push_back(
            {"spell:magic_missile", "class:wizard:spellcasting", 1, {{"access", "spellbook"}}});
        std::stable_sort(grants.begin(), grants.end(),
                         [](const auto &a, const auto &b)
                         {
                             return a.level < b.level;
                         });
        std::ostringstream out;
        out << grants.size();
        for (const auto &g : grants)
        {
            out << ' ' << std::quoted(g.id) << ' ' << std::quoted(g.source_id) << ' ' << g.level
                << ' ' << g.choices.size();
            for (const auto &[k, v] : g.choices)
                out << ' ' << std::quoted(k) << ' ' << std::quoted(v);
        }
        const auto next = out.str();
        body.replace(begin, length, next);
        search = begin + next.size();
    }
    return with_arcane_recovery_grants(
        with_background_training_grants(with_legacy_cantrip_choices(std::move(body))));
}
} // namespace opengold::test
#endif
