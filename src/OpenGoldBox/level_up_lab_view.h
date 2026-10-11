#ifndef OPENGOLDBOX_LEVEL_UP_LAB_VIEW_H
#define OPENGOLDBOX_LEVEL_UP_LAB_VIEW_H

#include "opengold/campaign_party.h"
#include "opengold/character_rules.h"
#include <godot_cpp/classes/control.hpp>
#include <exception>
#include <filesystem>
#include <memory>
#include <vector>

// A developer-facing entry point into the game's real advancement dialog.
// Its single-character campaigns are isolated from the normal game saves.
class LevelUpLabView : public godot::Control
{
    GDCLASS(LevelUpLabView, godot::Control)

  public:
    void _ready() override;
    void report_failure(const std::exception &failure);

  protected:
    static void _bind_methods() {}

  private:
    std::vector<opengold::rules::CreationChoice> classes_, races_, genders_;
    std::unique_ptr<opengold::CampaignParty> campaign_;
    opengold::MemberId member_{};

    void create_character();
    void open_level_up();
    void open_saves(bool saving);
    void save_character(const std::filesystem::path &path);
    void load_character(const std::filesystem::path &path);
    void show_sheet();
    void close_sheet();
    void advanced();
    void refresh_result();
};

#endif
