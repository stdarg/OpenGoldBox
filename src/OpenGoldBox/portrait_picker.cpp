#include "character_creation_view.h"
#include "localization.h"
#include "guarded_handlers.h"
#include "godot_nodes.h"
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <algorithm>
#include <set>
#include <utility>
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

// On arriving at the portrait page, the filters show portraits of the
// character's gender and race, of every class.
void CharacterCreationView::match_portrait_filters()
{
    const auto &draft = creator_->draft();
    const std::pair<const char *, const std::string *> filters[] {
        {"PortraitGender", &draft.gender}, {"PortraitClass", nullptr}, {"PortraitRace", &draft.race}};
    for (const auto &[name, value] : filters)
    {
        auto *filter = &required_node<OptionButton>(*this, name);
        filter->select(0);
        for (int item = 1; value && item < filter->get_item_count(); ++item)
            if (PortraitCatalog::same(String(filter->get_item_metadata(item)).utf8().get_data(), *value))
                filter->select(item);
    }
}

void CharacterCreationView::refresh_portraits()
{
    const bool on_page = creator_->step() == opengold::CreationStep::portrait;
    if (on_page && !on_portrait_page_)
        match_portrait_filters();
    on_portrait_page_ = on_page;
    filtered_portraits_.clear();
    const auto matches = [&](const char *node, const std::string & value)
    {
        auto *c = &required_node<OptionButton>(*this, node);
        return c->get_selected() <= 0 ||
               String(c->get_item_metadata(c->get_selected())) == gs(value);
    };
    const auto caption = [](const PortraitCatalog::Portrait & p)
    {
        return i18n::format("Race: {race}\nClass: {class}\nGender: {gender}",
        {
            {"race", i18n::text(p.race)}, {"class", i18n::text(p.klass)},
            {"gender", i18n::text(p.gender)}
        });
    };
    const auto &current = creator_->appearance().portrait;
    int selected = -1;
    PackedStringArray files, captions;
    for (std::size_t i = 0; i < portraits_->entries().size(); ++i)
    {
        const auto &p = portraits_->entries()[i];
        if (p.filename == current)
            required_node<Control>(*this, "PortraitPreview")
            .call("show_portrait", gs("res://bin/portraits/" + p.filename), caption(p));
        if (!matches("PortraitGender", p.gender) || !matches("PortraitClass", p.klass) ||
                !matches("PortraitRace", p.race))
            continue;
        if (p.filename == current)
            selected = static_cast<int>(filtered_portraits_.size());
        filtered_portraits_.push_back(i);
        files.push_back(gs("res://bin/portraits/" + p.filename));
        captions.push_back(caption(p));
    }
    required_node<Control>(*this, "PortraitScroll/PortraitSheet")
    .call("show_portraits", files, captions, selected, !added_to_party_);
}

void CharacterCreationView::portrait_filter_selected(std::int64_t)
{
    if (!refreshing_)
        refresh();
}

// The next or previous matching portrait, wrapping around.
void CharacterCreationView::portrait_part(int direction)
{
    if (added_to_party_ || filtered_portraits_.empty())
        return;
    const auto &current = creator_->appearance().portrait;
    const auto found = std::find_if(filtered_portraits_.begin(), filtered_portraits_.end(),
                                    [&](std::size_t index)
    {
        return portraits_->entries()[index].filename == current;
    });
    const int count = static_cast<int>(filtered_portraits_.size());
    const int at = found == filtered_portraits_.end()
                   ? -1 : static_cast<int>(found - filtered_portraits_.begin());
    portrait_selected(at < 0 ? (direction > 0 ? 0 : count - 1) : (at + direction + count) % count);
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
