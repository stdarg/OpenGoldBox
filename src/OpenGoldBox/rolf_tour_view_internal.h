#ifndef OPENGOLDBOX_ROLF_TOUR_VIEW_INTERNAL_H
#define OPENGOLDBOX_ROLF_TOUR_VIEW_INTERNAL_H

// The town view's text helpers that its acceptance checks (rolf_tour_checks.cpp)
// also compare against: shared by those two files only.
#include "opengold/exploration_session.h"
#include "opengold/gold_box_game.h"
#include "opengold/rolf_tour.h"
#include <godot_cpp/variant/string.hpp>

// The name of the area the party is in, in the game being played, in the
// player's language.
[[nodiscard]] godot::String place_name(opengold::GoldBoxGame game,
                                       const opengold::ExplorationSession *session);

// What a payer paid, for what was owed, and the change.
[[nodiscard]] godot::String
payment_notice(const opengold::por::CoinPayment &payment,
               opengold::GoldBoxGame game = opengold::GoldBoxGame::pool_of_radiance);

#endif
