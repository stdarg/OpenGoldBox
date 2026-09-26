#include "character_creation_view.h"
namespace
{
godot::String review_text(std::string_view value)
{
    return godot::String::utf8(value.data(), value.size());
}
} // namespace
#include "../../../src/OpenGoldBox/training_review_impl.h"

#include "../../../src/OpenGoldBox/spellbook_dialog_impl.h"
