#include "../src/OpenGoldBox/godot_path.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

// Paths the game builds from Godot's UTF-8 strings: save names, the game
// folder and the rules file.
namespace
{
void check(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}

void non_ascii_names_survive()
{
    const auto path = presentation::path_from_utf8("saves/named save \xC3\xBC.ogs");
    check(path.u8string() == u8"saves/named save ü.ogs", "A non-ASCII save name is kept");
    check(path.filename().u8string() == u8"named save ü.ogs" && path.extension() == ".ogs",
          "The path splits into folder, name and extension");
    check(presentation::path_from_utf8("").empty(), "An empty string is an empty path");
}

// Reading char bytes through a char8_t pointer is undefined behaviour; the
// conversion copies each byte instead.
void conversion_copies_the_bytes()
{
    const auto path = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "src/OpenGoldBox/godot_path.h";
    std::ifstream in(path);
    const std::string text{std::istreambuf_iterator<char>(in), {}};
    check(!text.empty(), "godot_path.h is found");
    check(text.find("reinterpret_cast") == std::string::npos,
          "godot_path.h reads char data through a char8_t pointer");
}
} // namespace

int main()
{
    try
    {
        non_ascii_names_survive();
        conversion_copies_the_bytes();
        std::cout << "Godot path checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
