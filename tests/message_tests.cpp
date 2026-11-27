// How a message's English is rendered (rules::english; opengold/message.h).
#include "opengold/message.h"
#include "test_support.h"

#include <iostream>

namespace
{
using namespace opengold::rules;
using opengold::test::check;

void each_placeholder_takes_its_value()
{
    const Message message{
        "{actor} hits {target}; {target} falls.", {{"actor", "Arya"}, {"target", "the orc"}}, {}};
    check(english(message) == "Arya hits the orc; the orc falls.",
          "Every {name} takes its argument's value");
}

// A character the player names "{name}" must not freeze the game.
void a_value_holding_its_own_placeholder_is_inserted_literally()
{
    const Message message{"{name} and {name} rest.", {{"name", "{name}"}}, {}};
    check(english(message) == "{name} and {name} rest.",
          "A value holding its own placeholder stays literal");
}

void a_value_forming_a_placeholder_with_its_neighbours_is_inserted_literally()
{
    const Message message{"{{name}}", {{"name", "name"}}, {}};
    check(english(message) == "{name}",
          "A placeholder formed with the text around it stays literal");
}

// An actor the player named "{target}" keeps that name, not the target's.
void a_value_holding_a_later_placeholder_is_inserted_literally()
{
    const Message message{
        "{actor} hits {target}.", {{"actor", "{target}"}, {"target", "the orc"}}, {}};
    check(english(message) == "{target} hits the orc.",
          "A value holding a later argument's placeholder stays literal");
}

// The reverse order: a later value holding an earlier placeholder.
void a_value_holding_an_earlier_placeholder_is_inserted_literally()
{
    const Message message{"{actor} hits {target}.", {{"actor", "Arya"}, {"target", "{actor}"}}, {}};
    check(english(message) == "Arya hits {actor}.",
          "A value holding an earlier argument's placeholder stays literal");
}

// A {...} no argument names stays as it is.
void an_unknown_placeholder_stays_as_written()
{
    const Message message{"{actor} sees {nobody}{", {{"actor", "Arya"}}, {}};
    check(english(message) == "Arya sees {nobody}{", "An unknown placeholder stays as written");
}

} // namespace

int main()
{
    try
    {
        each_placeholder_takes_its_value();
        a_value_holding_its_own_placeholder_is_inserted_literally();
        a_value_forming_a_placeholder_with_its_neighbours_is_inserted_literally();
        a_value_holding_a_later_placeholder_is_inserted_literally();
        a_value_holding_an_earlier_placeholder_is_inserted_literally();
        an_unknown_placeholder_stays_as_written();
        std::cout << "Message rendering passed.\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
