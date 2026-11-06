#ifndef OPENGOLD_DEV_SPRITE_CATALOG_H
#define OPENGOLD_DEV_SPRITE_CATALOG_H

#include "opengold/character_art.h"
#include "opengold/combat_body_catalog.h"
#include "opengold/creature_catalog.h"
#include "opengold/formats.h"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <filesystem>
#include <map>
#include <optional>
#include <vector>

class SpriteCatalog : public godot::RefCounted
{
    GDCLASS(SpriteCatalog, godot::RefCounted)
  public:
    godot::String load_data(const godot::String &game_directory,
                            const godot::String &art_directory);
    godot::Array creatures() const;
    godot::Array identical_art_groups() const;
    bool bind_monster_art(int index, const godot::String &archive, int record,
                          const godot::String &basis);
    godot::Dictionary monster_art(int index) const;
    godot::Ref<godot::Texture2D> monster_image(int index, bool action) const;
    godot::Array weapons() const;
    godot::Array item_types() const;
    godot::Array combat_body_groups() const;
    godot::Dictionary character_art(int head, int body, int size, int weapon_type, bool shield,
                                    const godot::Array &colors) const;
    godot::Array close_ups() const;
    godot::Dictionary close_up_art(const godot::String &folder,
                                   const godot::PackedInt32Array &delays) const;
    int close_up_frame_at(const godot::PackedInt32Array &delays, double seconds) const;

  protected:
    static void _bind_methods();

  private:
    struct CloseUp
    {
        unsigned bank{}, record{};
        std::string key;
        std::vector<opengold::AnimationFrame> frames;
        std::vector<std::string> copies; // Other records with the identical picture.
    };

    void load_close_ups(const std::filesystem::path &game);

    struct MonsterArt
    {
        std::filesystem::path archive;
        unsigned base{};
        std::string basis;
    };

    std::optional<opengold::por::CreatureCatalog> creatures_;
    std::optional<opengold::por::CharacterArt> characters_;
    std::optional<opengold::por::CombatBodyCatalog> bodies_;
    std::vector<opengold::por::CreatureId> ids_;
    std::vector<MonsterArt> extra_art_;
    std::vector<std::vector<std::string>> identical_art_groups_;
    std::map<opengold::por::CreatureId, MonsterArt> art_;
    std::map<std::string, std::vector<std::uint8_t>> archive_bytes_;
    std::vector<CloseUp> close_ups_;
};
#endif
