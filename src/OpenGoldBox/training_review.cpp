#include "character_creation_view.h"
#include "localization.h"

namespace
{
godot::String review_text(std::string_view value)
{
    return i18n::text(value);
}
} // namespace

#include "training_review_impl.h"

#include "spellbook_dialog_impl.h"

#include "equipment_choice_impl.h"
