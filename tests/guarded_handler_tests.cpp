#include "../src/OpenGoldBox/scoped_flag.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

// Godot calls a game handler from engine code that a C++ exception cannot
// unwind through. These checks keep every handler behind the guard in
// guarded_handlers.h, keep a failed handler from leaving a screen stuck, and
// keep nodes taken out of the tree owned.
namespace
{
void check(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}

void scoped_flag_clears_however_the_scope_ends()
{
    bool refreshing = false;
    {
        const presentation::ScopedFlag flag(refreshing);
        check(refreshing, "The flag is set inside its scope");
    }
    check(!refreshing, "The flag clears when its scope ends");
    try
    {
        const presentation::ScopedFlag flag(refreshing);
        throw std::runtime_error("A refresh fails");
    }
    catch (const std::runtime_error &)
    {
    }
    check(!refreshing, "The flag clears when an exception leaves its scope");
}

// callable_mp would let a handler's exception reach the engine; every
// connection goes through presentation::guarded instead.
void every_handler_is_guarded()
{
    const auto game = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "src/OpenGoldBox";
    unsigned sources = 0;
    for (const auto &entry : std::filesystem::directory_iterator(game))
    {
        const auto extension = entry.path().extension();
        if (extension != ".cpp" && extension != ".h")
            continue;
        ++sources;
        if (entry.path().filename() == "guarded_handlers.h")
            continue;
        std::ifstream in(entry.path());
        const std::string text{std::istreambuf_iterator<char>(in), {}};
        check(text.find("callable_mp(") == std::string::npos,
              entry.path().filename().string() +
              " connects a handler with callable_mp; use presentation::guarded");
        // A node taken out of the tree must be owned in the same step.
        check(entry.path().filename() == "godot_nodes.h" ||
              text.find("remove_child(") == std::string::npos,
              entry.path().filename().string() +
              " removes a child by hand; use presentation::detach_child");
        // get_node<T> returns null for a missing node and every caller
        // dereferenced it; a lookup that may fail says so with
        // get_node_or_null.
        check(entry.path().filename() == "godot_nodes.h" ||
              text.find("get_node<") == std::string::npos,
              entry.path().filename().string() +
              " looks up a node with get_node<T>; use presentation::required_node");
    }
    check(sources > 20, "The game's sources are found");
}

// A header, mixins included, changes nothing in the file that includes it
// beyond what it declares (Effective C++ Items 30 and 31): it has an include
// guard, and no namespace-scope using-directive or unnamed namespace.
void headers_keep_to_themselves()
{
    const auto game = std::filesystem::path(OPENGOLD_SOURCE_DIR) / "src/OpenGoldBox";
    unsigned headers = 0;
    for (const auto &entry : std::filesystem::directory_iterator(game))
    {
        if (entry.path().extension() != ".h")
            continue;
        ++headers;
        std::ifstream in(entry.path());
        const std::string text{std::istreambuf_iterator<char>(in), {}};
        const auto name = entry.path().filename().string();
        check(text.find("#ifndef ") != std::string::npos &&
              text.find("#define ") != std::string::npos,
              name + " has no include guard");
        check(text.find("\nusing namespace ") == std::string::npos,
              name + " has a using-directive at namespace scope");
        check(text.find("\nnamespace\n") == std::string::npos,
              name + " has an unnamed namespace");
    }
    check(headers > 20, "The game's headers are found");
}
} // namespace

int main()
{
    try
    {
        scoped_flag_clears_however_the_scope_ends();
        every_handler_is_guarded();
        headers_keep_to_themselves();
        std::cout << "Guarded handler checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
