#include "creature_editor.h"
#include "creature_edits.h"
#include "godot_images.h"
#include "godot_path.h"
#include "wall_art_files.h"
#include <godot_cpp/core/class_db.hpp>
#include <exception>
#include <functional>
#include <string>
#include <vector>

using godot::ClassDB;
using godot::D_METHOD;
using godot::String;

namespace
{
String utf8(const std::string &text)
{
    return String::utf8(text.c_str(), static_cast<std::int64_t>(text.size()));
}

std::string text_of(const String &text)
{
    const auto bytes = text.utf8();
    return {bytes.get_data(), static_cast<std::size_t>(bytes.length())};
}

// A JSON string holding `message`.
std::string quoted(const std::string &message)
{
    std::string out = "\"";
    for (const char c : message)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
        }
        out += c == '\n' ? ' ' : c;
    }
    return out + "\"";
}

String failure(const std::string &message)
{
    return utf8("{\"ok\": false, \"error\": " + quoted(message) + "}\n");
}

// The answer, or the failure that stopped it.
String answer(const std::function<std::string()> &query)
{
    try
    {
        return utf8(query());
    }
    catch (const std::exception &error)
    {
        return failure(error.what());
    }
}
} // namespace

void CreatureEditor::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("open", "repository", "game_directory"), &CreatureEditor::open);
    ClassDB::bind_method(D_METHOD("creatures"), &CreatureEditor::creatures);
    ClassDB::bind_method(D_METHOD("creature", "script", "record"), &CreatureEditor::creature);
    ClassDB::bind_method(D_METHOD("creature_art", "script", "record"),
                         &CreatureEditor::creature_art);
    ClassDB::bind_method(D_METHOD("stat_lines"), &CreatureEditor::stat_lines);
    ClassDB::bind_method(D_METHOD("encounters"), &CreatureEditor::encounters);
    ClassDB::bind_method(D_METHOD("fight", "site", "levels", "challenge"), &CreatureEditor::fight);
    ClassDB::bind_method(D_METHOD("companions"), &CreatureEditor::companions);
    ClassDB::bind_method(D_METHOD("chances"), &CreatureEditor::chances);
    ClassDB::bind_method(D_METHOD("check", "edits", "date"), &CreatureEditor::check);
    ClassDB::bind_method(D_METHOD("publish", "edits", "date"), &CreatureEditor::publish);
}

String CreatureEditor::open(const String &repository, const String &game_directory)
{
    repository_ = presentation::path_from_godot(repository);
    game_directory_ = presentation::path_from_godot(game_directory);
    try
    {
        data_.emplace(repository_, game_directory_, decode_wall_png);
        return {};
    }
    catch (const std::exception &error)
    {
        data_.reset();
        return utf8(error.what());
    }
}

String CreatureEditor::creatures() const
{
    return answer(
        [&]
        {
            return data_.value().creatures();
        });
}

String CreatureEditor::creature(int script, int record) const
{
    return answer(
        [&]
        {
            return data_.value().creature(static_cast<unsigned>(script),
                                          static_cast<unsigned>(record));
        });
}

godot::Ref<godot::ImageTexture> CreatureEditor::creature_art(int script, int record) const
{
    try
    {
        const auto art = data_.value().creature_art(static_cast<unsigned>(script),
                                                    static_cast<unsigned>(record));
        return art ? presentation::image_texture(*art) : godot::Ref<godot::ImageTexture>();
    }
    catch (const std::exception &)
    {
        return {};
    }
}

String CreatureEditor::stat_lines() const
{
    return answer(
        [&]
        {
            return data_.value().stat_lines();
        });
}

String CreatureEditor::encounters() const
{
    return answer(
        [&]
        {
            return data_.value().encounters();
        });
}

String CreatureEditor::fight(const String &site, const godot::PackedInt32Array &levels,
                             int challenge) const
{
    return answer(
        [&]
        {
            std::vector<unsigned> party;
            for (std::int64_t n = 0; n < levels.size(); ++n)
            {
                party.push_back(static_cast<unsigned>(levels[n]));
            }
            return data_.value().fight(text_of(site), party, static_cast<unsigned>(challenge));
        });
}

String CreatureEditor::companions() const
{
    return answer(
        [&]
        {
            return data_.value().companions();
        });
}

String CreatureEditor::chances() const
{
    return answer(
        [&]
        {
            return data_.value().chances();
        });
}

String CreatureEditor::import(const String &edits, const String &date, bool write) const
{
    return answer(
        [&]
        {
            const opengold::edits::ImportOptions options{
                repository_, {}, game_directory_, write, text_of(date)};
            return opengold::edits::report_json(
                opengold::edits::import_edits(options, text_of(edits), "the editor's edits"),
                opengold::edits::ReportFiles::diffs);
        });
}

String CreatureEditor::check(const String &edits, const String &date) const
{
    return import(edits, date, false);
}

String CreatureEditor::publish(const String &edits, const String &date)
{
    const auto report = import(edits, date, true);
    if (!report.begins_with("{\"ok\": false"))
    {
        const auto reopened = open(utf8(repository_.string()), utf8(game_directory_.string()));
        if (!reopened.is_empty())
        {
            return failure("Imported, but the data could not be read again: " + text_of(reopened));
        }
    }
    return report;
}
