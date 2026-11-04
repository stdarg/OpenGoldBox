#ifndef OPENGOLD_CREATURE_EDITOR_H
#define OPENGOLD_CREATURE_EDITOR_H

#include "creature_data.h"
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <filesystem>
#include <optional>

// The Creature Stats Editor's link to the game's code: what the game reads and
// computes (tools/creature_data.h), and the importer that puts edits in data/
// (tools/creature_edits.h). Every query answers in JSON; a failure answers
// {"ok": false, "error": ...}.
class CreatureEditor : public godot::RefCounted
{
    GDCLASS(CreatureEditor, godot::RefCounted)

  public:
    // Reads the repository's data/ and the player's install; "" or the error.
    godot::String open(const godot::String &repository, const godot::String &game_directory);

    [[nodiscard]] godot::String creatures() const;
    [[nodiscard]] godot::String creature(int script, int record) const;
    [[nodiscard]] godot::Ref<godot::ImageTexture> creature_art(int script, int record) const;
    [[nodiscard]] godot::String stat_lines() const;
    [[nodiscard]] godot::String encounters() const;
    [[nodiscard]] godot::String fight(const godot::String &site,
                                      const godot::PackedInt32Array &levels, int challenge) const;
    [[nodiscard]] godot::String companions() const;
    [[nodiscard]] godot::String chances() const;

    // What importing `edits` (an edits file's text) would change, with each
    // file's diff; writes nothing.
    [[nodiscard]] godot::String check(const godot::String &edits, const godot::String &date) const;

    // Imports `edits` into data/ and reads the data again.
    godot::String publish(const godot::String &edits, const godot::String &date);

  protected:
    static void _bind_methods();

  private:
    std::filesystem::path repository_, game_directory_;
    std::optional<opengold::creatures::CreatureData> data_;

    [[nodiscard]] godot::String import(const godot::String &edits, const godot::String &date,
                                       bool write) const;
};
#endif
