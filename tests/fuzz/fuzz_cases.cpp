#include "fuzz_cases.h"
#include "../support/synthetic_dax.h"
#include "opengold/character_art.h"
#include "opengold/ecl_machine.h"
#include "opengold/por_sound.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace opengold::test
{
namespace
{
void require(bool condition, const char *message)
{
    // Parser runtime_errors are expected rejections. Invariant failures use a
    // different exception type so the harness cannot accidentally swallow them.
    if (!condition)
        throw std::logic_error(message);
}

void check_image(const ImageDecodeResult &result)
{
    if (!result)
    {
        require(result.image.rgba.empty(), "Rejected image contains partial pixels");
        return;
    }
    const auto &image = result.image;
    require(image.width && image.height, "Accepted image has empty dimensions");
    require(image.rgba.size() == std::size_t(image.width) * image.height * 4,
            "Image buffer size mismatch");
    for (std::size_t i = 3; i < image.rgba.size(); i += 4)
        require(image.rgba[i] == 0 || image.rgba[i] == 255, "EGA alpha must be binary");
}

const rules::RulesModule &module()
{
    static const auto rules =
        srd5::parse_content("OPENGOLD_SRD5 1 fuzz.synthetic.1\n"
                        "creature hero 16 40 20 30 5 1 8 3 4 1 6 2 80 320 2 2 4 1 7\n"
                        "creature enemy 12 18 -10 30 3 1 6 1 3 1 6 1 80 320 0 0 0 1 0\n"
                        "creature caster 16 40 20 30 5 1 8 3 4 1 6 2 80 320 2 2 4 3 7\n"
                        "saves enemy 0 1 1 0 0 0\n"
                        "spellcasting caster 2 39\n");
    return *rules;
}

FuzzSeed text_seed(std::string name, const std::string &bytes)
{
    return {std::move(name), {bytes.begin(), bytes.end()}};
}

FuzzSeed format_seed(std::string name, std::uint8_t mode, std::vector<std::uint8_t> bytes)
{
    bytes.insert(bytes.begin(), mode);
    return {std::move(name), std::move(bytes)};
}
} // namespace

void exercise_formats(std::span<const std::uint8_t> bytes)
{
    if (bytes.empty() || bytes.size() > 4096)
        return;
    const auto mode = bytes.front() % 5;
    const auto data = bytes.subspan(1);
    switch (mode)
    {
    case 0:
    {
        const auto archive = decode_dax_archive(data);
        if (!archive)
            require(archive.records.empty(), "Rejected DAX leaked partial records");
        std::set<unsigned> ids;
        for (const auto &record : archive.records)
        {
            require(ids.insert(record.id).second, "Accepted duplicate DAX IDs");
            require(record.bytes.size() <= 65535, "Expanded DAX record exceeds format bound");
        }
        // ID 0 is present in the authored seeds. Mutations can also choose any
        // byte ID, including missing records and a later animation frame.
        const auto id = data.empty() ? 0 : data.back();
        for (const auto selected :
                {
                    std::uint8_t(0), std::uint8_t(id)
                })
        {
            check_image(decode_ega_sprite(data, selected, 0));
            check_image(decode_ega_sprite(data, selected, 1));
            check_image(decode_ega_combat_icon(data, selected, 0));
            check_image(decode_ega_combat_icon(data, selected, 1));
        }
        break;
    }
    case 1:
    {
        check_image(decode_ega_picture(data));
        std::optional<por::IndexedIcon> icon;
        try
        {
            icon = por::decode_character_icon(data);
        }
        catch (const std::runtime_error &)
        {
            return;
        }
        require(icon->width == 24 && icon->height > 0 && icon->height <= 24,
                "Invalid accepted character dimensions");
        require(icon->pixels.size() == icon->width * icon->height,
                "Character pixel count mismatch");
        require(std::all_of(icon->pixels.begin(), icon->pixels.end(),
                            [](auto p)
        {
            return p < 16;
        }),
        "Character pixels are not palette indices");
        por::IndexedIcon body{24, 24, std::vector<std::uint8_t>(576)};
        const auto composed = por::compose_character_icon(*icon, body, {});
        require(composed.rgba.size() == 24 * 24 * 4, "Composed character dimensions changed");
        break;
    }
    case 2:
    {
        std::shared_ptr<const por::EclProgram> program;
        try
        {
            program = std::make_shared<const por::EclProgram>(
                          por::EclProgram::decode(data, "synthetic fuzz"));
        }
        catch (const por::EclError &)
        {
            return;
        }
        require(program->raw() == std::vector<std::uint8_t>(data.begin(), data.end()),
                "ECL lost owned source bytes");
        por::EclMachine machine(program);
        machine.seed_random(123);
        if (machine.start(0))
        {
            auto copy = machine;
            const auto first = machine.run(64), second = copy.run(64);
            require(first.instructions <= 64, "ECL exceeded instruction budget");
            require(first.state == second.state && first.diagnostic == second.diagnostic &&
                    machine.address() == copy.address() && machine.trace() == copy.trace(),
                    "Copied ECL machine is not deterministic");
        }
        break;
    }
    case 3:
    {
        std::vector<std::uint8_t> unpacked;
        try
        {
            unpacked = por::unpack_sound_executable(data);
        }
        catch (const std::runtime_error &)
        {
            return;
        }
        require(unpacked.size() <= 1024 * 1024, "Sound expansion exceeds decoder limit");
        break;
    }
    case 4:
    {
        if (data.size() < 8)
            return;
        std::array<std::uint16_t, 4> entries{};
        for (unsigned i = 0; i < entries.size(); ++i)
            entries[i] = data[2 * i] | (data[2 * i + 1] << 8);
        std::vector<por::SpeakerTick> ticks;
        try
        {
            ticks = por::decode_speaker_sequence(data.subspan(8), entries);
        }
        catch (const std::runtime_error &)
        {
            return;
        }
        require(ticks.size() <= 4096, "Sound sequence exceeds tick budget");
        break;
    }
    }
}

void exercise_checkpoint(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() > 4 * 1024 * 1024 + 1)
        return;
    const std::string text(bytes.begin(), bytes.end());
    std::unique_ptr<rules::CombatSession> session;
    try
    {
        session = module().restore(text);
    }
    catch (const std::runtime_error &)
    {
        return;
    }

    // Once restore accepts input, subsequent canonical restores and legal
    // commands must work. Exceptions here are bugs, not parser rejections.
    auto copy = module().restore(session->save());
    require(session->save() == copy->save(), "Accepted checkpoint does not round trip");
    for (unsigned step = 0; step < 3; ++step)
    {
        const auto saved = session->save();
        const auto commands = session->legal_commands();
        rules::Command invalid;
        if (!commands.empty())
            invalid = commands.back();
        invalid.revision = 0; // Valid session revisions are nonzero.
        require(!session->submit(invalid) && session->save() == saved,
                "Rejected command changed state/RNG");
        if (commands.empty())
            break;
        const auto index = (formats_checksum(bytes) + step) % commands.size();
        const auto command = commands[index];
        require(session->submit(command) && copy->submit(command), "Offered command was rejected");
        require(session->save() == copy->save(), "Restored sessions diverged on the same command");
        copy = module().restore(copy->save());
    }
}

std::vector<FuzzSeed> format_seeds()
{
    std::vector<FuzzSeed> seeds;
    std::vector<std::uint8_t> icon(17 + 24 * 8 / 2);
    word(icon, 0, 8);
    word(icon, 2, 3);
    icon[8] = 1;
    for (std::size_t i = 17; i < icon.size(); ++i)
        icon[i] = i;
    seeds.push_back(format_seed("character-icon", 1, icon));
    seeds.push_back(format_seed("combat-dax", 0, literal_dax({{0, icon}})));
    seeds.push_back(format_seed("two-records", 0, literal_dax({{0, icon}, {1, {1, 2, 3}}})));
    std::vector<std::uint8_t> sprite(1 + 21 + 8);
    sprite[0] = 1;
    word(sprite, 5, 2);
    word(sprite, 7, 1);
    sprite[22] = 0x08;
    sprite[23] = 0xd1;
    seeds.push_back(format_seed("sprite-dax", 0, literal_dax({{0, sprite}})));
    // 0x80 is the signed RLE command -128, not an end marker.
    seeds.push_back(format_seed("rle-128", 0, {9, 0, 0, 0, 0, 0, 0, 128, 0, 2, 0, 128, 85}));
    std::vector<std::uint8_t> ecl{0, 0};
    for (unsigned entry = 0; entry < 5; ++entry)
        ecl.insert(ecl.end(), {1, 1, 0x14, 0x99});
    ecl.push_back(0); // Five jumps to EXIT.
    seeds.push_back(format_seed("ecl-exit", 2, ecl));
    ecl.back() = 1;
    ecl.insert(ecl.end(), {1, 0x14, 0x99}); // Bounded infinite GOTO.
    seeds.push_back(format_seed("ecl-loop", 2, ecl));
    std::vector<std::uint8_t> mz(36);
    word(mz, 0, 0x5a4d);
    word(mz, 8, 2);
    mz[32] = 91;
    seeds.push_back(format_seed("sound-plain-exe", 3, mz));
    std::vector<std::uint8_t> packed(66);
    word(packed, 0, 0x5a4d);
    word(packed, 8, 2);
    word(packed, 20, 18);
    word(packed, 22, 1);
    packed[32] = 7;
    word(packed, 33, 16);
    packed[35] = 0xb1;
    std::fill(packed.begin() + 36, packed.begin() + 48, 255);
    word(packed, 60, 1);
    word(packed, 62, 1);
    word(packed, 64, 0x4252);
    seeds.push_back(format_seed("sound-exepack", 3, packed));
    std::vector<std::uint8_t> sound(8 + 16);
    word(sound, 0, 16);
    sound.insert(sound.end(), {255, 10, 3, 0, 255, 4, 0xe8, 3, 255, 0, 2, 0, 255, 0, 0, 0});
    seeds.push_back(format_seed("sound-sequence", 4, sound));
    return seeds;
}

std::vector<FuzzSeed> checkpoint_seeds()
{
    std::vector<FuzzSeed> seeds;
    for (unsigned seed = 0; seed < 4; ++seed)
    {
        rules::Encounter encounter
        {
            {8, 8, std::vector<std::uint8_t>(64)},
            {{1, "hero", "Hero", 0, {2, 2}}, {2, "enemy", "Enemy", 1, {3, 2}}}};
        encounter.battlefield.terrain[2 * 8 + 1] = 2;
        auto session = module().create(encounter, seed);
        seeds.push_back(text_seed("initial-" + std::to_string(seed), session->save()));
        const auto commands = session->legal_commands();
        const auto move = std::find_if(commands.begin(), commands.end(),
                                       [](const auto & command)
        {
            return command.verb == "move" &&
                   command.destination == rules::Cell{1, 2};
        });
        require(move != commands.end() && session->submit(*move),
                "Invalid synthetic movement seed");
        require(session->snapshot().reaction_pending, "Synthetic seed should pause for a reaction");
        seeds.push_back(text_seed("reaction-" + std::to_string(seed), session->save()));
        session->submit(session->legal_commands().front());
        seeds.push_back(text_seed("continued-" + std::to_string(seed), session->save()));
    }
    rules::Encounter effects
    {
        {8, 8, std::vector<std::uint8_t>(64)},
        {{1, "caster", "Caster", 0, {2, 2}}, {2, "enemy", "Target", 1, {3, 2}}}};
    auto session = module().create(effects, 3);
    auto commands = session->legal_commands();
    auto cast = std::find_if(commands.begin(), commands.end(),
                             [](const auto & c)
    {
        return c.verb == "blindness";
    });
    require(cast != commands.end() && session->submit(*cast), "Condition corpus casts Blindness");
    require(!session->snapshot().combatants.back().conditions.empty(),
            "Condition corpus contains an active effect");
    seeds.push_back(text_seed("active-blindness", session->save()));
    for (unsigned turn = 0; turn < 2; ++turn)
    {
        commands = session->legal_commands();
        const auto end = std::find_if(commands.begin(), commands.end(),
                                      [](const auto & c)
        {
            return c.verb == "end";
        });
        require(end != commands.end() && session->submit(*end), "Condition corpus advances time");
    }
    commands = session->legal_commands();
    cast = std::find_if(commands.begin(), commands.end(),
                        [](const auto & c)
    {
        return c.verb == "blindness";
    });
    require(cast != commands.end() && session->submit(*cast), "Condition corpus recasts Blindness");
    seeds.push_back(text_seed("overlapping-effects", session->save()));
    return seeds;
}
} // namespace opengold::test
