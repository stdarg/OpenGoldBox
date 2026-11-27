#include "opengold/town_service_host.h"
#include "host_services.h"

namespace opengold::por
{
using namespace host_services;

void TownServiceHost::show_host_menu(std::vector<Message> dialogue, std::vector<Message> choices)
{
    auto &shown = screen();
    shown.host_dialogue = dialogue;
    shown.dialogue = english(dialogue);
    shown.host_choices = choices;
    shown.choices.clear();
    for (const auto &choice : choices)
    {
        shown.choices.push_back(english(choice));
    }
    shown.diagnostic.clear();
    shown.phase = TourPhase::awaiting_continue;
    shown.continue_ticket = new_ticket();
    ++shown.revision;
}
} // namespace opengold::por
