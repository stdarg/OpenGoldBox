#ifndef OPENGOLDBOX_DIALOG_HOST_H
#define OPENGOLDBOX_DIALOG_HOST_H
#include <exception>
#include <functional>

namespace opengold
{
class CampaignParty;
}

// How a dialog reaches its host: the host's campaign, read at each use
// because loading a save replaces it, and where a failed handler is shown.
using CampaignAccess = std::function<opengold::CampaignParty &()>;
using FailureReport = std::function<void(const std::exception &)>;

#endif
