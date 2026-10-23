#ifndef OPENGOLDBOX_SHOP_DATA_FILES_H
#define OPENGOLDBOX_SHOP_DATA_FILES_H

#include "game_resources.h"
#include "godot_path.h"
#include "opengold/rolf_tour.h"
#include "opengold/shop_data.h"

// Gives the session the shops and temples data/shops describes (TEMPLE-1),
// which the Shop & Temple Manager edits. Throws on a file the game cannot use.
inline void use_shop_data(opengold::por::RolfTourSession &session)
{
    auto data =
        opengold::load_shop_data(presentation::path_from_godot(game_data_path("data/shops")));
    session.temples(std::move(data.temples));
    session.shops(std::move(data.shops));
}

#endif
