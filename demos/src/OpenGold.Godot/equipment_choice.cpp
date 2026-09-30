#include "character_creation_view.h"

namespace
{
godot::String review_text(std::string_view value)
{
    return godot::String::utf8(value.data(), value.size());
}
} // namespace

#include "../../../src/OpenGoldBox/equipment_choice_impl.h"
