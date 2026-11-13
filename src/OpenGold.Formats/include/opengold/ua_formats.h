#ifndef OPENGOLD_UA_FORMATS_H
#define OPENGOLD_UA_FORMATS_H

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "opengold/vga_picture.h"

// The data of Forgotten Realms: Unlimited Adventures (docs/games/ua.md). Unlike
// the DAX games, every .TLB and .GLB file is a resource library: a header, a
// table of item offsets and the items. Pictures are glyphs drawn in the VGA's
// planar 256-color mode; the design data (monsters, areas, labels, the game's
// settings and the event editor's pages) are records in DATA libraries.
namespace opengold::ua
{

// A resource library, unpacked: the shipped files are all stored unpacked.
struct Library
{
    // "TILE" for glyphs, "DATA" for records, "DIG4" for speaker sounds, and
    // "HLIB" for a library whose items are libraries.
    std::string signature;
    // Item 0 is an index that maps the numbers the game uses to items.
    bool indexed{};
    std::vector<std::vector<std::uint8_t>> items;
};

// nullopt when the header is not "HLIB", the library is packed, or the item
// offsets run backwards or past the end.
[[nodiscard]] std::optional<Library> decode_library(std::span<const std::uint8_t> bytes);

struct IndexEntry
{
    std::uint16_t number{}; // The number the game asks for.
    std::uint16_t item{};
};

// The index of an indexed library; nullopt when it is not one or an entry names
// an item the library does not have.
[[nodiscard]] std::optional<std::vector<IndexEntry>> decode_index(const Library &library);

// One step of a color cycle: entries first .. first + count - 1 rotate every
// rate ticks, towards the first when bit 0 of flags is set.
struct ColorCycle
{
    std::uint8_t flags{};
    std::uint8_t rate{};
    std::uint8_t first{};
    std::uint8_t count{};
};

// A glyph of a composite glyph, drawn at the composite's place moved by row and
// column.
struct GlyphPart
{
    std::uint8_t glyph{}; // The item of the same library.
    std::int16_t row{};
    std::int16_t column{};
};

enum class GlyphKind
{
    image,
    palette,
    composite,
};

struct Glyph
{
    GlyphKind kind{};
    // Subtracted from the place the glyph is drawn at.
    std::int16_t origin_row{};
    std::int16_t origin_column{};
    // An image's pixels, and which of them are see-through (empty when none is).
    VgaImage image;
    std::vector<bool> see_through;
    // A palette's colors and cycles.
    VgaPaletteRange palette;
    std::vector<ColorCycle> cycles;
    std::vector<GlyphPart> parts;
};

// One item of a TILE library. Images come in four layouts: plain or keyed
// (0FFh is see-through) planes, planes behind a mask, PackBits rows, and coded
// runs that skip see-through bytes. nullopt for a layout the shipped files do
// not use or sizes that do not add up.
[[nodiscard]] std::optional<Glyph> decode_glyph(std::span<const std::uint8_t> item);

// The glyphs of a TILE library, one for each item; an indexed library's item 0
// is its index, which is no glyph. nullopt when a glyph does not decode or a
// composite names a glyph the library lacks or another composite.
struct TileLibrary
{
    std::vector<IndexEntry> index;
    std::vector<std::optional<Glyph>> glyphs;
};

[[nodiscard]] std::optional<TileLibrary> decode_tile_library(const Library &library);

// GAME.FON: 96 characters, space to DEL, 12 rows of 8 pixels each, the high
// bit leftmost. A longer font adds 32 characters for the control codes.
struct Font
{
    using Cell = std::array<std::uint8_t, 12>;
    std::vector<Cell> characters;
    std::vector<Cell> control_characters;
};

[[nodiscard]] std::optional<Font> decode_font(std::span<const std::uint8_t> bytes);

// ITEM.DAT: the 254 items the kit offers, numbered from 1, 18 bytes each.
struct ItemRecord
{
    std::uint8_t type{}; // The item type: an entry of ITEMS.DAT.
    std::array<std::uint8_t, 3> name_words{};
    std::uint16_t weight{};
    std::uint16_t value{};
    std::int8_t magic_bonus{};
    std::int8_t save_bonus{};
    std::uint8_t readied{};
    std::uint8_t hidden_name_words{}; // Bits 0 to 2: words shown only once identified.
    std::uint8_t cursed{};
    std::uint8_t count{};
    std::array<std::uint8_t, 3> effects{};
};

[[nodiscard]] std::optional<std::vector<ItemRecord>>
decode_items(std::span<const std::uint8_t> bytes);

// A MONST.GLB item: a 450-byte creature record, PackBits-packed behind its
// big-endian unpacked size. The fields the kit's records share with a party
// member; raw keeps the whole record.
struct Monster
{
    std::array<std::uint8_t, 450> raw{};
    std::string name;
    std::uint32_t experience{};
    std::uint8_t race{};
    std::uint8_t character_class{};
    std::array<std::uint8_t, 7> levels{}; // One for each class; 0 when not of it.
    std::uint8_t hit_points{};
    std::uint8_t hit_dice{};
    std::uint8_t armor_class_raw{};
};

[[nodiscard]] std::optional<Monster> decode_monster(std::span<const std::uint8_t> item);

// Six-bit text: four characters in three bytes, the first in the top bits;
// 1 to 31 are the letters, 32 to 63 space, digits and punctuation, and 0 ends
// the text.
[[nodiscard]] std::string unpack_text(std::span<const std::uint8_t> packed);

// The text pool of an area (and of textbank.c): 400 slots of six-bit texts.
struct TextPool
{
    std::uint16_t capacity{};       // Bytes the texts may use.
    std::vector<std::string> texts; // One a slot; empty for a free slot.
};

[[nodiscard]] std::optional<TextPool> decode_text_pool(std::span<const std::uint8_t> bytes);

// A GEO.GLB item: an area as the kit saves it, in IFF chunks (FORM AMOD with
// HDR, MAP, ENCR and STRG).
struct AreaSquare
{
    std::array<std::uint8_t, 4> walls{}; // By direction: north, east, south, west.
    std::uint8_t event{};
    std::uint8_t bits{};
};

struct Area
{
    std::uint16_t version{};
    std::uint8_t rows{};
    std::uint8_t columns{};
    std::array<std::uint8_t, 290> header{}; // HDR as stored.
    std::vector<AreaSquare> squares;        // 576 squares, row by row of columns.
    std::vector<std::uint8_t> events;       // ENCR: 2000 bytes of event records.
    TextPool texts;
};

[[nodiscard]] std::optional<Area> decode_area(std::span<const std::uint8_t> item);

// A STRG.GLB item: the names the kit shows for numbered things (monsters, wall
// sets, spells, quests, ...), 14 bytes a label.
struct Label
{
    std::uint8_t number{};
    std::uint8_t kinds{}; // Bits of the kinds of thing it names.
    std::string text;
};

[[nodiscard]] std::optional<std::vector<Label>> decode_labels(std::span<const std::uint8_t> item);

// GAME.GLB's record: the design's title, the eight keys' and twelve special
// items' names, and its password.
struct GameSettings
{
    std::array<std::uint8_t, 388> raw{};
    std::string title;
    std::array<std::string, 8> key_names;
    std::array<std::string, 12> special_item_names;
    std::string password;
};

[[nodiscard]] std::optional<GameSettings> decode_game_settings(std::span<const std::uint8_t> item);

// A SCRIPT.GLB item: the event editor's form for one kind of event. Each page
// lists its fields; a field's kind says what it shows (3 a label, 4 a check
// box, 5 a pop-up menu, 6 a box, 7 a number, 8 a text, 9 and 10 an edit
// field, 20h to 22h page settings), and its operations tie it to the event's
// bytes. An empty item is a number no kind of event uses: a form with no pages.
struct EditorField
{
    std::uint8_t kind{};
    std::uint8_t row{}; // Kinds below 20h: where the field is, in text cells.
    std::uint8_t column{};
    std::uint8_t setting{}; // Kinds 20h and 21h: the page setting they set.
    std::string text;
    std::vector<std::uint8_t> operations;
};

struct EditorPage
{
    std::uint8_t number{};
    std::vector<EditorField> fields;
};

struct EditorForm
{
    std::string title;
    std::vector<EditorPage> pages;
};

[[nodiscard]] std::optional<EditorForm> decode_editor_form(std::span<const std::uint8_t> item);

// A SOUNDS.GLB item: a sound for the PC speaker, 4-bit samples two to a byte
// (the low nibble first), at rate samples a second. samples holds one 0..15
// level a sample.
struct SpeakerSound
{
    std::uint16_t rate{};
    std::vector<std::uint8_t> samples;
};

[[nodiscard]] std::optional<SpeakerSound> decode_speaker_sound(std::span<const std::uint8_t> item);

} // namespace opengold::ua

#endif
