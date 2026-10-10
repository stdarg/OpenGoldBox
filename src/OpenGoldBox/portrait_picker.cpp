#include "character_creation_view.h"
#include "localization.h"
#include "guarded_handlers.h"
#include "godot_nodes.h"
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <set>
#include <stdexcept>
using namespace godot;
using presentation::required_node;

namespace
{
String gs(std::string_view s)
{
    return String::utf8(s.data(), s.size());
}
} // namespace

void CharacterCreationView::load_portraits()
{
    portraits_ = PortraitCatalog::load();
    for (int field = 0; field < 3; ++field)
    {
        const char *names[] {"PortraitGender", "PortraitClass", "PortraitRace"};
        const char *labels[] {N_("All genders"), N_("All classes"), N_("All races")};
        auto *control = &required_node<OptionButton>(*this, names[field]);
        control->add_item(i18n::text(labels[field]));
        std::set<std::string> values;
        for (const auto &p : portraits_->entries())
            values.insert(field == 0 ? p.gender : field == 1 ? p.klass : p.race);
        for (const auto &value : values)
        {
            control->add_item(i18n::text(value));
            control->set_item_metadata(control->get_item_count() - 1, gs(value));
        }
        control->connect("item_selected",
                         presentation::guarded(this, &CharacterCreationView::portrait_filter_selected));
    }
}

void CharacterCreationView::refresh_portraits()
{
    filtered_portraits_.clear();
    auto *list = &required_node<OptionButton>(*this, "PortraitSelect");
    list->clear();
    const auto matches = [&](const char *node, const std::string & value)
    {
        auto *c = &required_node<OptionButton>(*this, node);
        return c->get_selected() <= 0 ||
               String(c->get_item_metadata(c->get_selected())) == gs(value);
    };
    int selected = -1;
    for (std::size_t i = 0; i < portraits_->entries().size(); ++i)
    {
        const auto &p = portraits_->entries()[i];
        if (!matches("PortraitGender", p.gender) || !matches("PortraitClass", p.klass) ||
                !matches("PortraitRace", p.race))
            continue;
        if (p.filename == creator_->appearance().portrait)
            selected = static_cast<int>(filtered_portraits_.size());
        filtered_portraits_.push_back(i);
        list->add_item(i18n::text(p.klass) + " / " + i18n::text(p.race) + " / " +
                       i18n::text(p.gender));
    }
    list->select(selected);
    if (selected < 0)
        list->set_text(i18n::text(filtered_portraits_.empty() ? N_("No matching portraits")
                                  : N_("Choose portrait")));
    const bool disabled = added_to_party_ || filtered_portraits_.empty();
    list->set_disabled(disabled);
    required_node<Button>(*this, "PortraitPrevious").set_disabled(disabled);
    required_node<Button>(*this, "PortraitNext").set_disabled(disabled);
    const auto &filename = creator_->appearance().portrait;
    list->set_tooltip_text(i18n::format(
                               "Current portrait: {file}\n{matches} Filters do not change your character.",
    {
        {"file", gs(filename)},
        {
            "matches", i18n::plural("{count} matching portrait.", "{count} matching portraits.",
                                    static_cast<int>(filtered_portraits_.size()))
        }
    }));
}

void CharacterCreationView::portrait_filter_selected(std::int64_t)
{
    if (!refreshing_)
        refresh();
}

void CharacterCreationView::portrait_part(int direction)
{
    if (added_to_party_ || filtered_portraits_.empty())
        return;
    const int current = required_node<OptionButton>(*this, "PortraitSelect").get_selected(),
              count = static_cast<int>(filtered_portraits_.size());
    portrait_selected(current < 0 ? (direction > 0 ? 0 : count - 1)
                      : (current + direction + count) % count);
}

void CharacterCreationView::portrait_selected(std::int64_t index)
{
    if (refreshing_ || added_to_party_)
        return;
    perform(
        [&]
    {
        if (index < 0 || static_cast<std::size_t>(index) >= filtered_portraits_.size())
            throw std::runtime_error("Invalid portrait selection");
        auto a = creator_->appearance();
        a.portrait = portraits_->entries().at(filtered_portraits_[index]).filename;
        creator_->appearance(a);
        if (completed_)
            completed_->appearance(a);
        portrait_chosen_ = true;
    });
}
