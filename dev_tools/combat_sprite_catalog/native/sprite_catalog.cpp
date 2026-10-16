#include "../../../src/OpenGold.Core/src/archive_io.h"
#include "../../../src/OpenGoldBox/godot_text.h"
#include "sprite_catalog.h"
#include "../../../src/OpenGoldBox/godot_images.h"
#include "../../../src/OpenGoldBox/godot_path.h"
#include "opengold/formats.h"
#include <godot_cpp/core/class_db.hpp>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>

using namespace godot;
using namespace opengold;
using namespace opengold::por;

namespace {
using presentation::gs;
std::vector<std::uint8_t> read_bytes(const std::filesystem::path &file)
{
    return opengold::por::detail::read_archive_bytes<std::runtime_error>(file,
        "Combat archive exceeds size limit: " + file.string(),
        "Cannot open " + file.string(), "Cannot read " + file.string());
}
}

void SpriteCatalog::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("load_data", "game_directory", "art_directory"), &SpriteCatalog::load_data);
    ClassDB::bind_method(D_METHOD("creatures"), &SpriteCatalog::creatures);
    ClassDB::bind_method(D_METHOD("monster_art", "index"), &SpriteCatalog::monster_art);
    ClassDB::bind_method(D_METHOD("monster_image", "index", "action"), &SpriteCatalog::monster_image);
    ClassDB::bind_method(D_METHOD("weapons"), &SpriteCatalog::weapons);
    ClassDB::bind_method(D_METHOD("combat_body_groups"), &SpriteCatalog::combat_body_groups);
    ClassDB::bind_method(D_METHOD("character_art", "head", "body", "size", "weapon_type", "shield", "colors"), &SpriteCatalog::character_art);
}

String SpriteCatalog::load_data(const String &game_directory, const String &art_directory)
{
    creatures_.reset(); characters_.reset(); bodies_.reset(); ids_.clear(); extra_art_.clear(); art_.clear(); archive_bytes_.clear();
    try {
        const auto game = presentation::path_from_godot(game_directory);
        const auto source = presentation::path_from_godot(art_directory);
        creatures_ = CreatureCatalog::load(game);
        characters_ = CharacterArt::load(game);
        bodies_ = CombatBodyCatalog::load(source / "combat-body-looks.tsv", source / "combat-weapon-options.tsv");
        std::map<int, std::vector<std::filesystem::path>> by_bank;
        for (const auto &file : std::filesystem::directory_iterator(game)) {
            const auto name = opengold::por::detail::upper_ascii(file.path().filename().string());
            if (!file.is_regular_file() || !name.starts_with("CPIC") || !name.ends_with(".DAX")) continue;
            const auto digit = name.substr(4, name.size() - 8);
            if (digit.size() != 1 || digit[0] < '1' || digit[0] > '8') continue;
            by_bank[digit[0] - '0'].push_back(file.path());
            archive_bytes_.emplace(name, read_bytes(file.path()));
        }
        const auto paired = [&](const std::filesystem::path &file, unsigned base) {
            const auto &bytes = archive_bytes_.at(opengold::por::detail::upper_ascii(file.filename().string()));
            return base < 128 && decode_ega_combat_icon(bytes, static_cast<std::uint8_t>(base), 0) &&
                decode_ega_combat_icon(bytes, static_cast<std::uint8_t>(base + 128), 0);
        };
        for (const auto &[id, creature] : creatures_->all()) {
            ids_.push_back(id);
            // Same ID in the same archive bank is a candidate, never a verified binding.
            for (const auto &file : by_bank[id.bank])
                if (paired(file, id.record)) {
                    art_.emplace(id, MonsterArt{file, id.record, "Same ID, same bank (candidate)"});
                    break;
                }
            if (art_.contains(id)) continue;
            for (const auto &[bank, files] : by_bank) {
                for (const auto &file : files)
                    if (paired(file, id.record)) {
                        art_.emplace(id, MonsterArt{file, id.record, "Same ID, different bank (candidate)"});
                        break;
                    }
                if (art_.contains(id)) break;
            }
            if (art_.contains(id)) continue;
            // Duplicate original names may share art, but this remains a weak candidate.
            for (const auto &[other_id, other] : creatures_->all()) {
                if (other_id == id || opengold::por::detail::upper_ascii(other.stored.name) != opengold::por::detail::upper_ascii(creature.stored.name)) continue;
                for (const auto &file : by_bank[other_id.bank])
                    if (paired(file, other_id.record)) {
                        art_.emplace(id, MonsterArt{file, other_id.record, "Same name alias (weak candidate)"});
                        break;
                    }
                if (art_.contains(id)) break;
            }
        }
        // Keep every archive-local combat pair visible, including art without a
        // confidently named MON record. An identical ID in another bank can differ.
        for (const auto &[bank, files] : by_bank) {
            for (const auto &file : files) {
                const auto &bytes = archive_bytes_.at(opengold::por::detail::upper_ascii(file.filename().string()));
                const auto decoded = decode_dax_archive(bytes);
                if (!decoded) continue;
                for (const auto &record : decoded.records) {
                    if (record.id >= 128 || !paired(file, record.id)) continue;
                    const bool named = std::any_of(art_.begin(), art_.end(), [&](const auto &item) {
                        return item.second.archive == file && item.second.base == record.id;
                    });
                    if (!named)
                        extra_art_.push_back({file, record.id, "Unidentified archive pair"});
                }
            }
        }
        return {};
    } catch (const std::exception &e) {
        creatures_.reset(); characters_.reset(); bodies_.reset(); ids_.clear(); extra_art_.clear(); art_.clear(); archive_bytes_.clear();
        return gs(e.what());
    }
}

Array SpriteCatalog::creatures() const
{
    Array result;
    if (!creatures_) return result;
    for (const auto &id : ids_) {
        const auto &creature = creatures_->find(id)->get();
        Dictionary row;
        row["key"] = gs(id.key()); row["name"] = gs(creature.stored.name);
        row["archive"] = gs(id.archive()); row["record"] = id.record;
        row["npc"] = creature.stored.npc_flag != 0;
        Array equipment;
        for (const auto &item : creature.equipment) {
            Dictionary entry;
            entry["label"] = gs(item.label());
            entry["readied"] = item.stored.readied();
            entry["type"] = item.stored.type;
            entry["hands"] = item.base.hands;
            entry["range"] = item.base.range;
            equipment.push_back(entry);
        }
        row["equipment"] = equipment;
        const bool assembled = creature.stored.raw[189] < 14 && creature.stored.raw[190] < 35;
        row["art_basis"] = art_.contains(id) ? gs(art_.at(id).basis) :
            (assembled ? String("MON character components (candidate)") : String("Unresolved binding"));
        result.push_back(row);
    }
    for (const auto &art : extra_art_) {
        Dictionary row;
        row["key"] = gs(opengold::por::detail::upper_ascii(art.archive.filename().string()) + ":" + std::to_string(art.base));
        row["name"] = String("Unidentified combat sprite");
        row["archive"] = gs(art.archive.filename().string());
        row["record"] = art.base;
        row["npc"] = false;
        row["equipment"] = Array();
        row["art_basis"] = gs(art.basis);
        result.push_back(row);
    }
    return result;
}

Dictionary SpriteCatalog::monster_art(int index) const
{
    Dictionary result;
    if (index < 0 || static_cast<std::size_t>(index) >= ids_.size() + extra_art_.size()) return result;
    const MonsterArt *art = nullptr;
    if (static_cast<std::size_t>(index) < ids_.size()) {
        const auto found = art_.find(ids_[index]);
        if (found != art_.end()) art = &found->second;
        if (!art) {
            const auto &record = creatures_->find(ids_[index])->get().stored;
            if (record.raw[189] < 14 && record.raw[190] < 35) {
                result["archive"] = gs(ids_[index].archive());
                result["record"] = ids_[index].record;
                result["basis"] = String("MON character components (candidate)");
                result["assembled"] = true;
            }
            return result;
        }
    } else art = &extra_art_[index - ids_.size()];
    result["archive"] = gs(art->archive.filename().string());
    result["record"] = art->base;
    result["basis"] = gs(art->basis);
    return result;
}

Ref<Texture2D> SpriteCatalog::monster_image(int index, bool action) const
{
    if (index < 0 || static_cast<std::size_t>(index) >= ids_.size() + extra_art_.size()) return {};
    const MonsterArt *art = nullptr;
    if (static_cast<std::size_t>(index) < ids_.size()) {
        const auto found = art_.find(ids_[index]);
        if (found != art_.end()) art = &found->second;
        if (!art && characters_) {
            const auto &record = creatures_->find(ids_[index])->get().stored;
            if (record.raw[189] < 14 && record.raw[190] < 35) {
                CharacterAppearance appearance;
                appearance.combat_head = record.raw[189];
                appearance.combat_body = record.raw[190];
                appearance.icon_size = IconSize::regular;
                try {
                    return presentation::image_texture(characters_->icon(appearance,
                        action ? IconPose::action : IconPose::ready));
                } catch (const std::exception &) { return {}; }
            }
        }
    } else art = &extra_art_[index - ids_.size()];
    if (!art) return {};
    const auto &bytes = archive_bytes_.at(opengold::por::detail::upper_ascii(art->archive.filename().string()));
    auto decoded = decode_ega_combat_icon(bytes,
        static_cast<std::uint8_t>(art->base + (action ? 128 : 0)), 0);
    if (!decoded) return {};
    return presentation::image_texture(decoded.image);
}

Array SpriteCatalog::weapons() const
{
    Array result;
    if (!bodies_) return result;
    for (const auto &option : bodies_->options()) {
        Dictionary row; row["label"] = gs(option.label); row["type"] = option.original_type;
        result.push_back(row);
    }
    return result;
}

Array SpriteCatalog::combat_body_groups() const
{
    Array result;
    if (!characters_) return result;
    for (const auto &group : characters_->combat_body_groups())
        result.append(static_cast<int>(group.front()));
    return result;
}

Dictionary SpriteCatalog::character_art(int head, int body, int size, int weapon_type,
                                        bool shield, const Array &colors) const
{
    Dictionary result;
    if (!characters_ || !bodies_) return result;
    try {
        if (head < 0 || head >= 14 || body < 0 || body >= 35 ||
            size < 0 || size > static_cast<int>(IconSize::large) || colors.size() != 12)
            throw std::runtime_error("Invalid character picker selection");
        CharacterAppearance appearance;
        appearance.combat_head = head; appearance.combat_body = body;
        appearance.icon_size = static_cast<IconSize>(size);
        for (int i = 0; i < 12; ++i) {
            const int color = colors[i];
            if (color < 0 || color >= 16) throw std::runtime_error("Color out of range");
            appearance.colors[i / 6][i % 6] = color;
        }
        std::vector<CombatEquipment> equipped;
        if (weapon_type > 0) equipped.push_back({weapon_type, "", ""});
        if (shield) equipped.push_back({59, "Shield", "shield"});
        const auto selected = bodies_->choose(equipped, appearance.combat_body);
        const unsigned equipment_body = selected.matched ? selected.body : 0;
        result["ready"] = presentation::image_texture(characters_->equipped_icon(appearance, equipment_body, IconPose::ready));
        result["action"] = presentation::image_texture(characters_->equipped_icon(appearance, equipment_body, IconPose::action));
        result["selection"] = gs(selected.label);
        result["matched"] = selected.matched;
        result["body"] = equipment_body;
        Array usage;
        const auto color_usage = characters_->color_usage(appearance);
        for (unsigned bank = 0; bank < 2; ++bank)
            for (unsigned part = 0; part < 6; ++part)
                usage.append(color_usage.contains(bank, part));
        result["color_usage"] = usage;
        return result;
    } catch (const std::exception &e) {
        result["error"] = gs(e.what());
        return result;
    }
}
