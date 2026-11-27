#include "godot_images.h"
#include "game_rules.h"
#include "class_text.h"
#include "godot_nodes.h"
#include "application_settings.h"
#include "game_lifecycle.h"
#include "character_text.h"
#include "localization.h"
#include "game_resources.h"
#include "shop_data_files.h"
#include "game_companions.h"
#include "game_encounters.h"
#include "game_monsters.h"
#include "game_wall_art.h"
#include "rolf_tour_view.h"
#include "rolf_tour_view_internal.h"
#include "opengold/campaign_session.h"
#include "opengold/original_rules.h"
#include "opengold/diagnostic_log.h"
#include "inventory_label.h"
#include "opengold/campaign_party.h"
#include "opengold/npc_portraits.h"
#include "opengold/srd5.h"
#include "opengold/scroll_key.h"
#include "opengold/ua_session.h"
#include "godot_path.h"
#include "guarded_handlers.h"
#include "godot_sound_output.h"
#include "monster_picture_timing.h"
#include "close_up_art.h"
#include "unloadable_art.h"
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/aspect_ratio_container.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/video_stream.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/video_stream_player.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <vector>

using namespace godot;
using namespace opengold::por;
using opengold::Coin;
using opengold::gold_value;
using presentation::required_node;

namespace
{
const std::array<Vector2, 4> direction{Vector2(0, -1), Vector2(1, 0), Vector2(0, 1),
                                       Vector2(-1, 0)};
// The original's fireball sample (START.EXE effect 11), which Valhingen
// Graveyard's storm plays as thunder.
constexpr unsigned original_fireball_sound = 11;
constexpr std::array<const char *, 4> direction_name{N_("North"), N_("East"), N_("South"),
                                                     N_("West")};
constexpr std::array<const char *, 8> wilderness_direction_name{
    N_("North"), N_("Northeast"), N_("East"), N_("Southeast"),
    N_("South"), N_("Southwest"), N_("West"), N_("Northwest")};

// Curse's wilderness scripts keep the town the party is at in this cell; the
// dungeon and the ruins that several towns share read it to tell them apart.
constexpr EclAddress curse_town_cell{0x4C9B};

// Which town's dungeon map 53 is: Shadowdale's (town 1), Ashabenford's (2) or
// Essembra's (8), as its script's opening tells them apart.
[[nodiscard]] const char *curse_town_dungeon(std::uint16_t town)
{
    if (town < 2)
    {
        return N_("Shadowdale Dungeon");
    }
    return town == 2 ? N_("Ashabenford Dungeon") : N_("Essembra Dungeon");
}

// A Curse of the Azure Bonds area's name, as the cluebook's maps and the
// scripts' own words name it. Area 0 is the wilderness, which has no map. A
// map that holds two places is named by the script that plays it, and a map
// several towns share by the town.
[[nodiscard]] const char *curse_area_name(const TourSnapshot &s, const RolfTourSession &session)
{
    switch (s.area_id)
    {
    case 0:
        return N_("Overland");
    case 1:
        return s.script_id == 2 ? N_("Tilverton Thieves' Guild") : N_("Tilverton");
    case 3:
        return N_("Tilverton Sewers");
    case 4:
        return N_("Fire Knife Hideout");
    case 16:
        return N_("Yulash");
    case 17:
        return N_("Pit of Moander");
    case 21:
        return session.script_variable(curse_town_cell) == 5 ? N_("Voonlar Ruins")
                                                             : N_("Phlan Ruins");
    case 32:
        return s.script_id == 33 ? N_("Shrine of Bane") : N_("Zhentil Keep");
    case 33:
        return N_("Cave of the Beholder");
    case 37:
        return N_("Oxam's Tower");
    case 50:
        return s.script_id == 50 ? N_("Cave of the Dracolich") : N_("Village of Haptooth");
    case 51:
        return N_("Wizard's Tower");
    case 53:
        return curse_town_dungeon(session.script_variable(curse_town_cell));
    case 64:
        return N_("Burial Glen");
    case 66:
        return N_("Ruins of Myth Drannor");
    case 67:
        return N_("Grand Ruined Temple");
    case 69:
        return session.script_variable(curse_town_cell) == 7 ? N_("Teshwave Ruins")
                                                             : N_("Hillsfar Ruins");
    default:
        return N_("Curse of the Azure Bonds");
    }
}

// A Secret of the Silver Blades area's name, as the cluebook's maps and the
// scripts' own words name it. Area 0 is the opening vision, which has no map.
[[nodiscard]] const char *secret_area_name(unsigned area_id)
{
    switch (area_id)
    {
    case 16:
        return N_("New Verdigris");
    case 32:
        return N_("Ruins of Old Verdigris");
    case 33:
        return N_("Well of Knowledge");
    case 34:
        return N_("Black Circle Headquarters");
    // Mine levels 1 to 8, then levels 9 and 10 with the way to the temple.
    case 48:
    case 49:
        return N_("Mines of Verdigris");
    case 50:
        return N_("Temple of Tyr");
    case 64:
        return N_("Dungeon, levels 10 to 7");
    case 65:
        return N_("Dungeon, levels 6 to 3");
    case 66:
        return N_("Dungeon, levels 2 and 1");
    case 68:
        return N_("Drider Base");
    case 80:
        return N_("Western Crevasses");
    case 81:
        return N_("Frost Giant Village");
    case 82:
        return N_("Eastern Crevasses");
    case 96:
        return N_("Castle Entrance");
    case 97:
        return N_("Middle Level of the Castle");
    case 98:
        return N_("Dreadlord's Sanctum");
    default:
        return N_("Secret of the Silver Blades");
    }
}

// Gateway's maps as the cluebook's sections name them (docs/games/gateway.md,
// Area names): one map a place, the overland's six by their region, and the
// final battle's four as Ascore.
[[nodiscard]] const char *gateway_area_name(unsigned area_id)
{
    switch (area_id)
    {
    case 1:
        return N_("Yartar");
    case 2:
        return N_("Kraken Complex in Sub-Yartar");
    case 3:
        return N_("Nesme");
    case 4:
        return N_("Silverymoon");
    case 5:
        return N_("Everlund");
    case 6:
        return N_("Neverwinter");
    case 7:
        return N_("Port Llast and the Gallant Prince");
    case 8:
        return N_("Luskan");
    case 9:
        return N_("Hosttower of the Arcane");
    case 10:
        return N_("Uttersea, Tuern");
    case 11:
        return N_("Gundarlun");
    case 12:
        return N_("Purple Rocks");
    case 13:
        return N_("Kraken Headquarters");
    case 14:
        return N_("Secomber");
    case 15:
        return N_("Loudwater");
    case 16:
        return N_("Llorkh");
    case 17:
        return N_("Arena Beneath Llorkh");
    case 18:
        return N_("Star Mounts");
    case 19:
        return N_("Sundabar");
    case 20:
    case 27:
    case 28:
    case 29:
    case 30:
        return N_("Ascore");
    case 21:
        return N_("The Sword Coast");
    case 22:
        return N_("The Dessarin Valley");
    case 23:
        return N_("The Silver Marches");
    case 24:
        return N_("The Eastern Wilds");
    case 25:
        return N_("The High Forest");
    case 26:
        return N_("The Delimbiyr Vale");
    default:
        return N_("Gateway to the Savage Frontier");
    }
}

// The areas the session can enter: the town (0), the Slums (20), Kuto's
// Well's plaza (29) and catacombs (32), and Podol Plaza (18).
[[nodiscard]] const char *district_name(unsigned area_id)
{
    if (area_id == 20)
    {
        return N_("Slums");
    }
    if (area_id == 18)
    {
        return N_("Podol Plaza");
    }
    if (area_id == 29 || area_id == 32)
    {
        return N_("Kuto's Well");
    }
    if (area_id == 2)
    {
        return N_("Cadorna Textile House");
    }
    if (area_id == 15)
    {
        return N_("Mendor's Library");
    }
    if (area_id == 21)
    {
        return N_("Sokal Keep");
    }
    if (area_id == 26)
    {
        return N_("Wilderness");
    }
    if (area_id == 14)
    {
        return N_("Kovel Mansion");
    }
    if (area_id == 24 || area_id == 31)
    {
        return N_("Temple of Bane");
    }
    if (area_id == 17)
    {
        return N_("Nomad Camp");
    }
    if (area_id == 22 || area_id == 23)
    {
        return N_("Pyramid");
    }
    // The western and eastern windows' caves.
    if (area_id == 25 || area_id == 27)
    {
        return N_("Wilderness");
    }
    if (area_id == 13)
    {
        return N_("Kobold Caves");
    }
    if (area_id == 16 || area_id == 30)
    {
        return N_("Lizard Men's Keep");
    }
    if (area_id == 10)
    {
        return N_("Valhingen Graveyard");
    }
    if (area_id == 9)
    {
        return N_("Stojanow Gate");
    }
    if (area_id == 6)
    {
        return N_("Old Town");
    }
    if (area_id == 3 || area_id == 4 || area_id == 5 || area_id == 7)
    {
        return N_("Valjevo Castle");
    }
    if (area_id == 1)
    {
        return N_("Buccaneers' Base");
    }
    if (area_id == 28)
    {
        return N_("Zhentil Outpost");
    }
    return N_("New Phlan");
}

} // namespace

// The name of the area the party is in, in the game being played, in the
// player's language; with no session, the game's first area. A design names
// its areas itself, as its designer wrote them.
[[nodiscard]] String place_name(opengold::GoldBoxGame game,
                                const opengold::ExplorationSession *session)
{
    if (session && !session->dax())
    {
        const auto name = session->area_name();
        return name.empty() ? i18n::text(N_("Unlimited Adventures")) : String::utf8(name.c_str());
    }
    static const TourSnapshot none;
    const auto *campaign = session ? session->dax() : nullptr;
    const TourSnapshot &s = campaign ? campaign->snapshot() : none;
    switch (game)
    {
    case opengold::GoldBoxGame::pool_of_radiance:
        return i18n::text(district_name(s.area_id));
    case opengold::GoldBoxGame::secret_of_the_silver_blades:
        return secret_area_name(s.area_id);
    case opengold::GoldBoxGame::gateway_to_the_savage_frontier:
        return gateway_area_name(s.area_id);
    case opengold::GoldBoxGame::curse_of_the_azure_bonds:
        return i18n::text(campaign ? curse_area_name(s, *campaign)
                                   : N_("Curse of the Azure Bonds"));
    default:
        return i18n::text(opengold::game_title(game).data());
    }
}

namespace
{
// A Teleport destination's name: its place, and which of the place's maps.
[[nodiscard]] const char *teleport_place(unsigned map)
{
    switch (map)
    {
    case 29:
        return N_("Kuto's Well, the plaza");
    case 32:
        return N_("Kuto's Well, the catacombs");
    case 31:
        return N_("Temple of Bane, the street east of the temple");
    case 24:
        return N_("Temple of Bane, the temple's block");
    case 26:
        return N_("Central Wilderness, the grove");
    case 22:
        return N_("Pyramid, the prison of souls");
    case 23:
        return N_("Pyramid, the laboratories");
    case 25:
        return N_("Western wilderness, the cave");
    case 27:
        return N_("Eastern wilderness, the cave");
    case 16:
        return N_("Lizard Men's Keep, the first level");
    case 30:
        return N_("Lizard Men's Keep, the flooded level");
    case 3:
        return N_("Valjevo Castle, the northwest quarter");
    case 4:
        return N_("Valjevo Castle, the northeast quarter");
    case 5:
        return N_("Valjevo Castle, the southeast quarter");
    case 7:
        return N_("Valjevo Castle, the tower's upper level");
    default:
        return district_name(map);
    }
}

// What a shop's filter lists, ShopFilter's items in order; All is the default.
constexpr std::array shop_filters{N_("Armor"), N_("Weapons"), N_("Other"), N_("All")};
constexpr std::int64_t all_items_filter = 3;

} // namespace

void RolfTourView::_bind_methods()
{
    ADD_SIGNAL(MethodInfo("save_requested", PropertyInfo(Variant::BOOL, "saving")));
    // Each refresh tells the host whether the game is won, for the winner
    // bank (opengold::WinnerCapture).
    ADD_SIGNAL(MethodInfo("game_shown", PropertyInfo(Variant::BOOL, "game_won")));
    // The Menu button: the host opens the in-game menu.
    ADD_SIGNAL(MethodInfo("menu_requested"));
    // The solar map's Change: the host opens the Options.
    ADD_SIGNAL(MethodInfo("options_requested"));
    ADD_SIGNAL(MethodInfo("party_member_selected", PropertyInfo(Variant::INT, "slot")));
    ADD_SIGNAL(MethodInfo("level_up_requested", PropertyInfo(Variant::INT, "id")));
    // Reincarnate from camp: the new body's pages belong to the creation view.
    ADD_SIGNAL(MethodInfo("reincarnation_requested", PropertyInfo(Variant::INT, "caster"),
                          PropertyInfo(Variant::INT, "target"),
                          PropertyInfo(Variant::STRING, "action")));
    ClassDB::bind_method(D_METHOD("tour_visuals"), &RolfTourView::tour_visuals);
}

void RolfTourView::_notification(int what)
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  respond_to_notification(what);
                              });
}

void RolfTourView::respond_to_notification(int what)
{
    if (what == NOTIFICATION_RESIZED && ready_)
    {
        layout();
        required_node<Control>(*this, "TourVisuals").queue_redraw();
    }
}

void RolfTourView::_ready()
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  original_walls_ = settings::original_wall_art();
                                  prepare_view();
                              });
}

void RolfTourView::show_original_walls(bool original)
{
    original_walls_ = original;
    rendered_pose_.reset();
    // A close-up on screen switches art too.
    shown_monster_picture_ = nullptr;
    refresh();
}

void RolfTourView::prepare_view()
{
    i18n::prepare_ui(*this);
    // Resizing the window can notify this view before dynamic controls exist.
    ready_ = false;
    // Child nodes are scene-owned; these lookups are temporary non-owning views.
    required_node<Button>(*this, "%Continue")
        .connect("pressed", presentation::guarded(this, &RolfTourView::next));
    required_node<Button>(*this, "%Restart")
        .connect("pressed", presentation::guarded(this, &RolfTourView::restart));
    required_node<Button>(*this, "%Left")
        .connect("pressed", presentation::guarded(this, &RolfTourView::left));
    required_node<Button>(*this, "%Right")
        .connect("pressed", presentation::guarded(this, &RolfTourView::right));
    required_node<Button>(*this, "%Forward")
        .connect("pressed", presentation::guarded(this, &RolfTourView::forward));
    required_node<Button>(*this, "%Look")
        .connect("pressed", presentation::guarded(this, &RolfTourView::look));
    required_node<Button>(*this, "%Camp")
        .connect("pressed", presentation::guarded(this, &RolfTourView::camp));
    required_node<Button>(*this, "%Search")
        .connect("pressed", presentation::guarded(this, &RolfTourView::toggle_search));
    required_node<Button>(*this, "%Inventory")
        .connect("pressed", presentation::guarded(this, &RolfTourView::inventory));
    required_node<Button>(*this, "InventoryPanel/%Close")
        .connect("pressed", presentation::guarded(this, &RolfTourView::inventory));
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        auto *member = &required_node<Button>(*this, String("%PartyList/Rows/Member") +
                                                         String::num_uint64(slot));
        member->connect("pressed",
                        presentation::guarded(this, &RolfTourView::party_selected).bind(slot));
        member->connect("opened",
                        presentation::guarded(this, &RolfTourView::open_member_sheet).bind(slot));
        auto arrow = presentation::instantiate_control<Button>(
            "res://scenes/control_templates/party_advance.tscn");
        arrow->set_tooltip_text(i18n::text(N_("Level up")));
        arrow->connect("pressed",
                       presentation::guarded(this, &RolfTourView::level_up_requested).bind(slot));
        presentation::attach_child(*member, std::move(arrow));
    }
    required_node<ItemList>(*this, "%Choices")
        .connect("gui_input", presentation::guarded(this, &RolfTourView::choices_input));
    auto &filter = required_node<OptionButton>(*this, "%ShopFilter");
    for (const char *kind : shop_filters)
    {
        filter.add_item(i18n::text(kind));
    }
    filter.select(static_cast<std::int32_t>(all_items_filter));
    filter.connect("item_selected",
                   presentation::guarded(this, &RolfTourView::shop_filter_selected));
    required_node<Label>(*this, "%FilterLabel").set_text(i18n::text(N_("Filter")));
    required_node<ItemList>(*this, "%Choices")
        .connect("resized", presentation::guarded(this, &RolfTourView::fit_choice_columns));
    required_node<ItemList>(*this, "InventoryPanel/%Items")
        .connect("item_selected", presentation::guarded(this, &RolfTourView::inventory_selected));
    required_node<Button>(*this, "InventoryPanel/%Equip")
        .connect("pressed", presentation::guarded(this, &RolfTourView::equip_item).bind(true));
    required_node<Button>(*this, "InventoryPanel/%Unequip")
        .connect("pressed", presentation::guarded(this, &RolfTourView::equip_item).bind(false));
    required_node<Button>(*this, "InventoryPanel/%Identify")
        .connect("pressed", presentation::guarded(this, &RolfTourView::identify_item));
    required_node<Button>(*this, "InventoryPanel/%CastIdentify")
        .connect("pressed", presentation::guarded(this, &RolfTourView::cast_identify));
    required_node<Button>(*this, "InventoryPanel/%Sell")
        .connect("pressed", presentation::guarded(this, &RolfTourView::give_up_item).bind(true));
    required_node<Button>(*this, "InventoryPanel/%Drop")
        .connect("pressed", presentation::guarded(this, &RolfTourView::give_up_item).bind(false));
    required_node<Button>(*this, "InventoryPanel/%UseItem")
        .connect("pressed", presentation::guarded(this, &RolfTourView::use_inventory_item));
    required_node<Button>(*this, "%Cast")
        .connect("pressed", presentation::guarded(this, &RolfTourView::cast_menu));
    required_node<Button>(*this, "CastPanel/%Close")
        .connect("pressed", presentation::guarded(this, &RolfTourView::cast_menu));
    required_node<OptionButton>(*this, "CastPanel/%Action")
        .connect("item_selected", presentation::guarded(this, &RolfTourView::cast_action_selected));
    required_node<Button>(*this, "CastPanel/%Use")
        .connect("pressed", presentation::guarded(this, &RolfTourView::cast_use));
    required_node<Button>(*this, "MemberSheet/%Close")
        .connect("pressed", presentation::guarded(this, &RolfTourView::close_sheet));
    required_node<Window>(*this, "MemberSheet")
        .connect("close_requested", presentation::guarded(this, &RolfTourView::close_sheet));
    required_node<Button>(*this, "%LeaveShop")
        .connect("pressed", presentation::guarded(this, &RolfTourView::leave_shop));
    connect_solar_map();
    auto &menu = required_node<Button>(*this, "%Menu");
    menu.connect("pressed", presentation::guarded(this, &RolfTourView::request_menu));
    menu.set_visible(embedded_party_);
    ready_ = true;
    layout();
    if (Engine::get_singleton()->is_editor_hint())
    {
        return;
    }
    const auto args = OS::get_singleton()->get_cmdline_user_args();
    no_fog_ = settings::flag("--no-fog");
    town_check_ = args.has("--town-check");
    checking_ = args.has("--tour-check") || town_check_;
    rest_check_ = args.has("--rest-check");
    mastery_rest_check_ = args.has("--mastery-rest-check");
    capture_ = args.has("--capture");
    if (campaign_ && args.has("--party-check"))
    {
        town_check_ = checking_ = true;
    }
    // An original OpenGoldBox footstep cue, not extracted SSI sound data.
    footstep_.instantiate();
    footstep_->set_format(AudioStreamWAV::FORMAT_16_BITS);
    footstep_->set_mix_rate(22050);
    PackedByteArray samples;
    samples.resize(2205 * 2);
    std::uint32_t noise = 7;
    for (int i = 0; i < 2205; ++i)
    {
        noise = noise * 1664525U + 1013904223U;
        const double envelope = std::exp(-i / 280.0);
        const auto sample =
            static_cast<std::int16_t>((static_cast<int>(noise >> 16) - 32768) * 0.13 * envelope);
        samples.set(i * 2, static_cast<std::uint16_t>(sample) & 255);
        samples.set(i * 2 + 1, static_cast<std::uint16_t>(sample) >> 8);
    }
    footstep_->set_data(samples);
    auto &footstep = required_node<AudioStreamPlayer>(*this, "Footstep");
    footstep.set_stream(footstep_);
    footstep_full_volume_ = footstep.get_volume_linear();
    // The storm's thunder is the original's own sample, from the player's
    // game; a game without the sound bank leaves it silent.
    const auto game = presentation::path_from_godot(application_settings(*this).game_path());
    if (std::filesystem::is_directory(game))
    {
        try
        {
            original_sound_ = std::make_unique<SoundPlayer>(
                SoundBank::load(game),
                std::make_unique<GodotSoundOutput>(
                    required_node<AudioStreamPlayer>(*this, "OriginalSound")));
        }
        catch (const std::runtime_error &)
        {
            original_sound_.reset();
        }
    }
    // Pools of Darkness and Treasures step to their own footstep (explore.pas,
    // StepForward) and play their scripts' sound effects (eclvm.pas,
    // EclPlaySound), from the player's copy of the game.
    const auto chosen = application_settings(*this).chosen_game();
    if (chosen == opengold::GoldBoxGame::pools_of_darkness ||
        chosen == opengold::GoldBoxGame::treasures_of_the_savage_frontier)
    {
        const auto folder =
            presentation::path_from_godot(application_settings(*this).game_folder(chosen));
        try
        {
            if (std::filesystem::is_directory(folder))
            {
                game_sounds_ = std::make_unique<SoundPlayer>(
                    SoundBank::load(folder, chosen), std::make_unique<GodotSoundOutput>(footstep));
            }
        }
        catch (const std::runtime_error &)
        {
            game_sounds_.reset();
        }
    }
    apply_sound_volume();
    setup_rest();
    restart();
    if (embedded_party_)
    {
        required_node<Button>(*this, "%Restart").hide();
    }
}

bool RolfTourView::shop_layout() const
{
    // A treasure lists its items as a shop does.
    return session_ && (session_->snapshot().phase == TourPhase::shopping ||
                        session_->snapshot().phase == TourPhase::treasure);
}

void RolfTourView::layout()
{
    required_node<Node>(*this, "Layout").call("apply_layout", shop_layout());
    required_node<Label>(*this, "%Legend").hide();
    required_node<Node>(*this, "MemberSheet/WindowLayout").call("fit", Vector2i(get_size()));
}

RolfTourSession &RolfTourView::dax_session() const
{
    auto *campaign = dax();
    if (!campaign)
    {
        throw std::logic_error("No DAX campaign is being played");
    }
    return *campaign;
}

void RolfTourView::restart()
{
    forget_monster_picture();
    try
    {
        auto &settings = application_settings(*this);
        error_ = "";
        if (!campaign_ || !embedded_party_)
        {
            // The town on its own plays an SRD fighter.
            campaign_ = new_campaign_party(GameRules{}, game_rules_files(settings));
            opengold::rules::CharacterDraft d;
            d.race = "human";
            d.gender = "female";
            d.character_class = "fighter";
            d.alignment = "neutral_good";
            d.background = "soldier";
            d.name = "Adventurer";
            d.rolled = true;
            for (auto &roll : d.rolls)
            {
                roll = {{6, 5, 4, 1}, 3};
            }
            (void)campaign_->add_pc(opengold::Character(*campaign_->character_rules(), d, {}));
            campaign_->add_coins(Coin::gold, 9999);
        }
        if (session_)
        {
            session_->restart();
            session_->campaign_party(campaign_);
        }
        else if (settings.chosen_game() == opengold::GoldBoxGame::unlimited_adventures)
        {
            // A design made with the kit, from the kit's folder and the
            // design's (docs/games/ua.md, From the startup menu).
            session_ = std::make_unique<opengold::ua::UaSession>(
                opengold::ua::UaSession::start(opengold::ua::DesignFiles(
                    presentation::path_from_godot(settings.game_folder(settings.chosen_game())),
                    presentation::path_from_godot(settings.design_folder()))));
            apply_design_difficulty();
            session_->campaign_party(campaign_);
        }
        else
        {
            // The game chosen on the startup menu: Pool of Radiance's campaign
            // with OpenGoldBox's data for it, or another game's from its folder.
            const auto game = settings.chosen_game();
            const bool pool_of_radiance = game == opengold::GoldBoxGame::pool_of_radiance;
            if (pool_of_radiance)
            {
                session_ =
                    std::make_unique<opengold::DaxExplorationSession>(opengold::load_campaign(
                        presentation::path_from_godot(settings.game_path()), game,
                        {&game_encounters(), &game_wall_art(), game_monsters()}));
            }
            else
            {
                session_ =
                    std::make_unique<opengold::DaxExplorationSession>(opengold::load_campaign(
                        presentation::path_from_godot(settings.game_folder(game)), game));
            }
            auto &campaign = dax_session();
            campaign.encounter_challenge(settings::encounter_challenge());
            campaign.difficulty(settings::difficulty_setting());
            campaign.set_auto_equip(auto_equip_setting());
            if (pool_of_radiance)
            {
                use_shop_data(settings, campaign);
                campaign.companion_rules(campaign_->character_rules(), game_companions());
            }
            if (campaign_)
            {
                campaign.campaign_party(campaign_);
            }
            for (unsigned i = 0; i < sprites_.size(); ++i)
            {
                const auto &source = campaign.sprites()[i];
                // Only Rolf's tour has a guide to show (Curse's campaign has none).
                if (source.width == 0 || source.height == 0)
                {
                    continue;
                }
                sprites_[i] = presentation::image_texture(source);
            }
        }
        rendered_pose_.reset();
        rendered_sprite_id_ = 999;
        displayed_ticket_ = 0;
        played_footsteps_ = 0;
        played_thunder_ = 0;
        played_script_sounds_ = 0;
        played_music_starts_ = 0;
        music_.reset();
        shown_revision_ = 0;
        session_->advance(0);
    }
    catch (const std::exception &error)
    {
        session_.reset();
        forget_monster_picture();
        error_ = String::utf8(error.what());
    }
    refresh();
}

void RolfTourView::next()
{
    if (!session_)
    {
        return;
    }
    if (fight_waits_on_picture_)
    {
        return;
    }
    const auto &s = session_->snapshot();
    const auto selected = required_node<ItemList>(*this, "%Choices").get_selected_items();
    if (s.phase == TourPhase::shopping)
    {
        if (!selected.is_empty())
        {
            dax_session().buy(s.continue_ticket,
                              shop_rows_.at(static_cast<std::size_t>(selected[0])));
        }
    }
    else if (s.phase == TourPhase::treasure)
    {
        take_treasure(selected.is_empty() ? -1 : selected[0]);
    }
    else if (s.phase == TourPhase::awaiting_input)
    {
        session_->input(s.continue_ticket,
                        required_node<LineEdit>(*this, "%Answer").get_text().utf8().get_data());
    }
    else
    {
        session_->choose(s.continue_ticket, selected.is_empty() ? 0 : selected[0]);
    }
    refresh();
}

namespace
{
// A shop's stock fills three columns, each narrower than a third of the list
// by its item's padding and the gap between columns.
constexpr int shop_column_count = 3;
constexpr int shop_column_margin = 16;

// The rows in the order an ItemList, which fills across, needs so that they
// read down its columns, as a newspaper's do: the first columns one row
// longer than the rest, so only the last row is short.
std::vector<std::size_t> down_the_columns(const std::vector<std::size_t> &rows, int columns)
{
    if (rows.empty())
    {
        return {};
    }
    const auto count = rows.size();
    const auto width = static_cast<std::size_t>(columns);
    const auto height = (count + width - 1) / width;
    const auto long_columns = count - (height - 1) * width;
    std::vector<std::size_t> across;
    for (std::size_t row = 0; row < height; ++row)
    {
        for (std::size_t column = 0; column < width; ++column)
        {
            if (row == height - 1 && column >= long_columns)
            {
                break;
            }
            const auto top = column < long_columns
                                 ? column * height
                                 : long_columns * height + (column - long_columns) * (height - 1);
            across.push_back(rows[top + row]);
        }
    }
    return across;
}

} // namespace

// A shop lists its stock in three equal columns across the list; every other
// list, a treasure's too, in one.
void RolfTourView::fit_choice_columns()
{
    auto &choices = required_node<ItemList>(*this, "%Choices");
    const bool shop = session_ && session_->snapshot().phase == TourPhase::shopping;
    choices.set_max_columns(shop ? shop_column_count : 1);
    choices.set_fixed_column_width(
        shop ? std::max(0, static_cast<int>(choices.get_size().x) / shop_column_count -
                               shop_column_margin)
             : 0);
}

// Whether a shop's filter lists the item: armor and shields, weapons, the
// rest, or all.
bool RolfTourView::passes_shop_filter(const opengold::por::Equipment &item) const
{
    if (!campaign_ || shop_filter_ == all_items_filter)
    {
        return true;
    }
    using opengold::rules::EquipmentSlot;
    const auto slot = campaign_->equipment_slot(item);
    const bool armor = slot == EquipmentSlot::armor || slot == EquipmentSlot::shield;
    const bool weapon = slot == EquipmentSlot::weapon;
    return shop_filter_ == 0 ? armor : shop_filter_ == 1 ? weapon : !armor && !weapon;
}

void RolfTourView::shop_filter_selected(std::int64_t index)
{
    shop_filter_ = index;
    // The list is drawn again under the new filter.
    displayed_ticket_ = 0;
    refresh();
}

// A double click on a shop's item buys it, as choosing it and Buy do. The list
// selects the item on the first click.
void RolfTourView::choices_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventMouseButton> click = event;
    if (click.is_null() || !click->is_double_click() ||
        click->get_button_index() != MouseButton::MOUSE_BUTTON_LEFT || !session_ ||
        session_->snapshot().phase != TourPhase::shopping)
    {
        return;
    }
    auto &choices = required_node<ItemList>(*this, "%Choices");
    if (const auto row = choices.get_item_at_position(click->get_position(), true); row >= 0)
    {
        choices.select(row);
        next();
    }
}

// On the wilderness map the three buttons step west, north and east, as the
// original's arrow keys do there.
bool RolfTourView::on_overland() const
{
    return session_ && session_->snapshot().overland;
}

void RolfTourView::left()
{
    movement(on_overland() ? ExplorationCommand::west : ExplorationCommand::turn_left);
}

void RolfTourView::right()
{
    movement(on_overland() ? ExplorationCommand::east : ExplorationCommand::turn_right);
}

void RolfTourView::forward()
{
    movement(on_overland() ? ExplorationCommand::north : ExplorationCommand::forward);
}

void RolfTourView::toggle_search()
{
    if (!session_)
    {
        return;
    }
    session_->set_searching(!session_->snapshot().searching);
    refresh();
}

void RolfTourView::request_menu()
{
    emit_signal("menu_requested");
}

std::optional<opengold::UnidentifiedMagic> RolfTourView::auto_equip_setting()
{
    if (!settings::auto_equip())
    {
        return std::nullopt;
    }
    return settings::skip_unidentified_magic() ? opengold::UnidentifiedMagic::skip
                                               : opengold::UnidentifiedMagic::equip;
}

void RolfTourView::apply_design_difficulty()
{
    if (auto *design = dynamic_cast<opengold::ua::UaSession *>(session_.get()))
    {
        design->difficulty(settings::difficulty(), settings::design_difficulty());
    }
}

void RolfTourView::apply_settings()
{
    if (auto *campaign = dax())
    {
        campaign->difficulty(settings::difficulty_setting());
        campaign->encounter_challenge(settings::encounter_challenge());
        campaign->set_auto_equip(auto_equip_setting());
    }
    apply_design_difficulty();
    // The solar map's options show at once (rolf_tour_solar_map.cpp).
    if (on_solar_map())
    {
        refresh();
    }
}

void RolfTourView::apply_sound_volume()
{
    if (original_sound_)
    {
        original_sound_->set_volume(settings::sound_volume(settings::SoundKind::effects));
    }
    if (music_)
    {
        music_->set_volume(settings::sound_volume(settings::SoundKind::music));
    }
    if (game_sounds_)
    {
        game_sounds_->set_volume(settings::sound_volume(settings::SoundKind::effects));
        return;
    }
    const auto share = static_cast<float>(settings::volume_share(settings::SoundKind::effects));
    required_node<AudioStreamPlayer>(*this, "Footstep")
        .set_volume_linear(footstep_full_volume_ * share);
}

void RolfTourView::look()
{
    movement(ExplorationCommand::look);
}

void RolfTourView::leave_shop()
{
    if (!dax())
    {
        return;
    }
    const auto &s = session_->snapshot();
    if (s.phase != TourPhase::treasure)
    {
        dax_session().leave_shop(s.continue_ticket);
        refresh();
        return;
    }
    // As in the original, leaving with items in the pile asks first.
    if (!dax_session().treasure_pile().empty() && !treasure_leave_warned_)
    {
        treasure_leave_warned_ = true;
        refresh();
        return;
    }
    treasure_leave_warned_ = false;
    treasure_notice_ = String();
    dax_session().leave_treasure(s.continue_ticket);
    refresh();
}

void RolfTourView::take_treasure(std::int64_t item)
{
    const auto &s = session_->snapshot();
    if (item < 0 || item >= static_cast<std::int64_t>(dax_session().treasure_pile().size()) ||
        !campaign_ || !campaign_->selected())
    {
        return;
    }
    const auto label = String::utf8(dax_session().treasure_pile()[item].label().c_str());
    const auto taker =
        String::utf8(campaign_->member(campaign_->selected()).character.sheet().name.c_str());
    if (dax_session().take_treasure(s.continue_ticket, static_cast<std::size_t>(item)))
    {
        treasure_leave_warned_ = false;
        treasure_notice_ =
            i18n::format(N_("{name} takes {item}."), {{"name", taker}, {"item", label}});
    }
    else
    {
        // The item would take the member past what it can carry.
        treasure_notice_ =
            i18n::format(N_("{name} cannot carry that much weight."), {{"name", taker}});
    }
}

void RolfTourView::inventory()
{
    auto *panel = &required_node<Control>(*this, "InventoryPanel");
    if (panel->is_visible())
    {
        panel->hide();
        return;
    }
    if (!session_)
    {
        return;
    }
    refresh_inventory();
    panel->show();
}

void RolfTourView::refresh_inventory()
{
    auto *items = &required_node<ItemList>(*this, "InventoryPanel/%Items");
    items->clear();
    if (!campaign_ || !campaign_->selected())
    {
        return;
    }
    const auto &m = campaign_->member(campaign_->selected());
    required_node<Label>(*this, "InventoryPanel/%Header")
        .set_text(i18n::format("{name} / {class} / party {gold} gp",
                               {{"name", String::utf8(m.character.sheet().name.c_str())},
                                {"class", i18n::class_summary(m.character.sheet())},
                                {"gold", campaign_->purse_gold()}}));
    // In a shop every line says what the shop pays for it (SELL-1).
    const bool shopping = session_ && session_->snapshot().phase == TourPhase::shopping;
    for (const auto &item : m.character.inventory().items())
    {
        // Detect Magic marks magic items, as the original's item lists did.
        const bool sensed =
            campaign_->detecting_magic() && campaign_->magical(campaign_->selected(), item.id);
        auto line = (sensed ? String("* ") : String()) +
                    (std::find(m.equipped.begin(), m.equipped.end(), item.id) != m.equipped.end()
                         ? i18n::text("Equipped / ")
                         : String()) +
                    inventory_label(m, item);
        if (shopping)
        {
            line = i18n::format(
                N_("{item} / sells for {gold} gp"),
                {{"item", line},
                 {"gold", campaign_->sale_offer(campaign_->selected(), item.id, sale_market())}});
        }
        items->add_item(line);
    }
    if (items->get_item_count())
    {
        items->select(0);
    }
    required_node<Button>(*this, "InventoryPanel/%Equip").set_disabled(!items->get_item_count());
    required_node<Button>(*this, "InventoryPanel/%Unequip").set_disabled(!items->get_item_count());
    // As in the original, only a shop identifies items, and it takes any item.
    auto &identify = required_node<Button>(*this, "InventoryPanel/%Identify");
    identify.set_visible(session_ && session_->snapshot().phase == TourPhase::shopping);
    identify.set_disabled(!items->get_item_count());
    // Any shop buys any item but a cursed one, and any item but a cursed one
    // can be dropped (SELL-1).
    auto &sell = required_node<Button>(*this, "InventoryPanel/%Sell");
    sell.set_visible(identify.is_visible());
    sell.set_disabled(!items->get_item_count());
    required_node<Button>(*this, "InventoryPanel/%Drop").set_disabled(!items->get_item_count());
    give_up_warned_.reset();
    // Any member who can cast Identify may cast it on the selected item.
    auto &spell = required_node<Button>(*this, "InventoryPanel/%CastIdentify");
    spell.set_visible(campaign_->identifier().has_value());
    spell.set_disabled(!items->get_item_count());
    required_node<Button>(*this, "InventoryPanel/%UseItem").hide();
    required_node<Label>(*this, "InventoryPanel/%Status")
        .set_text(i18n::text(N_("Inventory is empty. Visit a shop to buy equipment.")));
    if (items->get_item_count())
    {
        inventory_selected(0);
    }
}

void RolfTourView::inventory_selected(std::int64_t index)
{
    if (!campaign_ || !campaign_->selected())
    {
        return;
    }
    const auto &m = campaign_->member(campaign_->selected());
    const auto items = m.character.inventory().items();
    if (index < 0 || static_cast<std::size_t>(index) >= items.size())
    {
        return;
    }
    // The Manual and some potions are used outside combat (MAGIC-3), and a
    // scroll whose spell the member can read or copy now (MAGIC-6).
    give_up_warned_.reset();
    const auto &item = items[index];
    const bool scroll = opengold::rules::parse_scroll_key(item.definition_id).has_value();
    // Where a Pools of Darkness script bars magic, the items menu offers no
    // Use (viewchar.pas, UseItemMenu).
    required_node<Button>(*this, "InventoryPanel/%UseItem")
        .set_visible(
            !(dax() && dax()->magic_barred()) &&
            (campaign_->rule_module().usable_outside_combat(item.definition_id) ||
             !campaign_->copyable_spells(campaign_->selected(), item.id).empty() ||
             (scroll && !campaign_->scroll_actions(campaign_->selected(), item.id).empty())));
    try
    {
        required_node<Label>(*this, "InventoryPanel/%Status")
            .set_text(i18n::text(campaign_->rule_module().equipment_note(
                m.character.sheet(), opengold::rules_definition(m, items[index]))));
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "InventoryPanel/%Status").set_text(i18n::text(e.what()));
    }
}

void RolfTourView::equip_item(bool equip)
{
    try
    {
        const auto selection =
            required_node<ItemList>(*this, "InventoryPanel/%Items").get_selected_items();
        if (selection.is_empty())
        {
            throw std::runtime_error("Select an item first.");
        }
        const auto id = campaign_->selected();
        // Copies: equipping can replace the member's inventory, and with it
        // anything still pointing into it (Effective C++ Item 28).
        std::uint64_t item{};
        std::string definition;
        {
            const auto items = campaign_->member(id).character.inventory().items();
            if (selection[0] < 0 || static_cast<std::size_t>(selection[0]) >= items.size())
            {
                throw std::runtime_error("Select an existing item.");
            }
            item = items[selection[0]].id;
            definition = opengold::rules_definition(campaign_->member(id), items[selection[0]]);
        }
        std::optional<opengold::rules::HazardAttackResult> burn;
        if (equip)
        {
            burn = campaign_->equip(id, item);
        }
        else
        {
            campaign_->unequip(id, item);
        }
        refresh_inventory();
        required_node<ItemList>(*this, "InventoryPanel/%Items").select(selection[0]);
        refresh();
        // An alignment-bound sword refuses a wielder of another alignment (#267).
        if (burn)
        {
            required_node<Label>(*this, "InventoryPanel/%Status")
                .set_text(i18n::format(
                    N_("The sword will not stay in {name}'s hand and burns for {damage} Force damage."),
                    {{"name", String::utf8(campaign_->member(id).character.sheet().name.c_str())},
                     {"damage", burn->damage}}));
            return;
        }
        required_node<Label>(*this, "InventoryPanel/%Status")
            .set_text(equip ? i18n::text("Equipped.") + " " +
                                  i18n::text(campaign_->rule_module().equipment_note(
                                      campaign_->member(id).character.sheet(), definition))
                            : i18n::text("Item unequipped."));
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "InventoryPanel/%Status").set_text(i18n::text(e.what()));
    }
}

void RolfTourView::identify_item()
{
    try
    {
        const auto selection =
            required_node<ItemList>(*this, "InventoryPanel/%Items").get_selected_items();
        if (selection.is_empty())
        {
            throw std::runtime_error("Select an item first.");
        }
        const auto id = campaign_->selected();
        const auto items = campaign_->member(id).character.inventory().items();
        if (selection[0] < 0 || static_cast<std::size_t>(selection[0]) >= items.size())
        {
            throw std::runtime_error("Select an existing item.");
        }
        const auto item = items[selection[0]].id;
        const bool revealed = campaign_->identify_at_shop(id, item);
        refresh_inventory();
        required_node<ItemList>(*this, "InventoryPanel/%Items").select(selection[0]);
        refresh();
        const auto name = campaign_->member(id).character.inventory().find(item)->get().name;
        required_node<Label>(*this, "InventoryPanel/%Status")
            .set_text(i18n::format(revealed ? N_("The shop identifies it: {item}.")
                                            : N_("The shop learns nothing new about {item}."),
                                   {{"item", i18n::text(name)}}));
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "InventoryPanel/%Status").set_text(i18n::text(e.what()));
    }
}

opengold::SaleMarket RolfTourView::sale_market() const
{
    // A design has no shops yet: an item sells for half its own value.
    if (const auto *campaign = dax())
    {
        return {.shop = campaign->shop_stock(), .town = campaign->town_stock()};
    }
    return {};
}

void RolfTourView::give_up_item(bool sell)
{
    try
    {
        const auto selection =
            required_node<ItemList>(*this, "InventoryPanel/%Items").get_selected_items();
        if (selection.is_empty())
        {
            throw std::runtime_error("Select an item first.");
        }
        const auto id = campaign_->selected();
        const auto &m = campaign_->member(id);
        const auto items = m.character.inventory().items();
        if (selection[0] < 0 || static_cast<std::size_t>(selection[0]) >= items.size())
        {
            throw std::runtime_error("Select an existing item.");
        }
        const auto item = items[selection[0]].id;
        const auto label = inventory_label(m, items[selection[0]]);
        const auto name = String::utf8(m.character.sheet().name.c_str());
        auto &status = required_node<Label>(*this, "InventoryPanel/%Status");
        // As in the original, the shop makes its offer and Drop warns before
        // the item goes.
        if (!give_up_warned_ || give_up_warned_->item != item || give_up_warned_->sell != sell)
        {
            give_up_warned_ = GiveUpWarning{item, sell};
            status.set_text(
                sell
                    ? i18n::format(
                          N_("The shop offers {gold} gp for {item}. Press Sell again to sell it."),
                          {{"gold", campaign_->sale_offer(id, item, sale_market())},
                           {"item", label}})
                    : i18n::format(N_("{item} will be gone for good. Press Drop again to drop it."),
                                   {{"item", label}}));
            return;
        }
        String done;
        if (sell)
        {
            const auto gold = campaign_->sell(id, item, sale_market());
            done = i18n::format(N_("{name} sells {item} for {gold} gp."),
                                {{"name", name}, {"item", label}, {"gold", gold}});
        }
        else
        {
            campaign_->drop(id, item);
            done = i18n::format(N_("{name} drops {item}. It is gone."),
                                {{"name", name}, {"item", label}});
        }
        refresh_inventory();
        auto &list = required_node<ItemList>(*this, "InventoryPanel/%Items");
        if (list.get_item_count())
        {
            const auto next = std::min<std::int32_t>(selection[0], list.get_item_count() - 1);
            list.select(next);
            inventory_selected(next);
        }
        refresh();
        status.set_text(done);
    }
    catch (const std::exception &e)
    {
        give_up_warned_.reset();
        required_node<Label>(*this, "InventoryPanel/%Status").set_text(i18n::text(e.what()));
    }
}

namespace
{
// Where the Wand of Secrets points (SECRET-1), or that it found nothing.
String wand_of_secrets_notice(const String &name, const std::optional<WandSign> &sign)
{
    if (!sign)
    {
        return i18n::format(
            N_("{name} points the Wand of Secrets. It does not pulse: no hidden secret door or trap is within 60 feet."),
            {{"name", name}});
    }
    if (sign->trap)
    {
        return i18n::format(
            N_("{name} points the Wand of Secrets. It pulses toward a trap at ({x}, {y})."),
            {{"name", name}, {"x", sign->x}, {"y", sign->y}});
    }
    static constexpr std::array<const char *, 4> walls{
        N_("{name} points the Wand of Secrets. It pulses toward a secret door in the north wall of ({x}, {y})."),
        N_("{name} points the Wand of Secrets. It pulses toward a secret door in the east wall of ({x}, {y})."),
        N_("{name} points the Wand of Secrets. It pulses toward a secret door in the south wall of ({x}, {y})."),
        N_("{name} points the Wand of Secrets. It pulses toward a secret door in the west wall of ({x}, {y}).")};
    return i18n::format(walls[index(sign->side)], {{"name", name}, {"x", sign->x}, {"y", sign->y}});
}
} // namespace

void RolfTourView::use_inventory_item()
{
    try
    {
        const auto selection =
            required_node<ItemList>(*this, "InventoryPanel/%Items").get_selected_items();
        if (selection.is_empty())
        {
            throw std::runtime_error("Select an item first.");
        }
        const auto id = campaign_->selected();
        const auto items = campaign_->member(id).character.inventory().items();
        if (selection[0] < 0 || static_cast<std::size_t>(selection[0]) >= items.size())
        {
            throw std::runtime_error("Select an existing item.");
        }
        const auto item = items[selection[0]];
        const auto name = String::utf8(campaign_->member(id).character.sheet().name.c_str());
        // A scroll's spells are read, or copied, from the Cast menu (MAGIC-6);
        // an original scroll's are scribed.
        if (opengold::rules::parse_scroll_key(item.definition_id) ||
            !campaign_->copyable_spells(id, item.id).empty())
        {
            scroll_item_ = item.id;
            required_node<Control>(*this, "InventoryPanel").hide();
            required_node<Label>(*this, "CastPanel/%Status").set_text(String());
            refresh_cast();
            required_node<Control>(*this, "CastPanel").show();
            return;
        }
        String result;
        if (item.definition_id == "manual_of_bodily_health")
        {
            campaign_->read_manual(id, item.id);
            result = i18n::format(
                N_("{name} reads the Manual of Bodily Health: Constitution rises by 2."),
                {{"name", name}});
        }
        else if (item.definition_id == "wand_of_secrets")
        {
            if (!dax())
            {
                throw std::runtime_error("The wand can be used only while exploring.");
            }
            result = wand_of_secrets_notice(name, dax_session().point_wand_of_secrets(id, item.id));
        }
        else
        {
            campaign_->drink_potion(id, item.id);
            result = i18n::format(N_("{name} uses the {item}."),
                                  {{"name", name}, {"item", i18n::text(item.name)}});
        }
        refresh_inventory();
        refresh();
        required_node<Label>(*this, "InventoryPanel/%Status").set_text(result);
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "InventoryPanel/%Status").set_text(i18n::text(e.what()));
    }
}

// Reads the scroll's spell `id` on `patient`, or copies one ("copy:N") into
// the member's spellbook (MAGIC-6).
void RolfTourView::read_or_copy_scroll(const std::string &id, opengold::MemberId patient,
                                       const String &label)
{
    const auto caster = campaign_->selected();
    const auto name = String::utf8(campaign_->member(caster).character.sheet().name.c_str());
    String result;
    if (id.starts_with("copy:"))
    {
        campaign_->copy_scroll_spell(caster, *scroll_item_, static_cast<unsigned>(id[5] - '0'));
        result =
            campaign_->rule_module().plays_original_records()
                ? i18n::format(N_("{name} will scribe the spell at the next rest."),
                               {{"name", name}})
                : i18n::format(N_("{name} copies the spell into the spellbook."), {{"name", name}});
    }
    else
    {
        result = campaign_->read_scroll(caster, *scroll_item_, patient, id)
                     ? i18n::format(N_("{name} reads {spell} from the scroll."),
                                    {{"name", name}, {"spell", label}})
                     : i18n::format(N_("{name} fails to read {spell}, and it is lost."),
                                    {{"name", name}, {"spell", label}});
    }
    refresh_cast();
    refresh();
    required_node<Label>(*this, "CastPanel/%Status").set_text(result);
}

void RolfTourView::cast_identify()
{
    try
    {
        const auto selection =
            required_node<ItemList>(*this, "InventoryPanel/%Items").get_selected_items();
        if (selection.is_empty())
        {
            throw std::runtime_error("Select an item first.");
        }
        const auto holder = campaign_->selected();
        const auto items = campaign_->member(holder).character.inventory().items();
        if (selection[0] < 0 || static_cast<std::size_t>(selection[0]) >= items.size())
        {
            throw std::runtime_error("Select an existing item.");
        }
        const auto item = items[selection[0]].id;
        const auto caster = campaign_->identifier();
        if (!caster)
        {
            throw std::runtime_error("That spell or feature cannot be used now");
        }
        const bool revealed = campaign_->cast_identify(*caster, holder, item);
        refresh_inventory();
        required_node<ItemList>(*this, "InventoryPanel/%Items").select(selection[0]);
        refresh();
        const auto name = campaign_->member(holder).character.inventory().find(item)->get().name;
        const auto caster_name =
            String::utf8(campaign_->member(*caster).character.sheet().name.c_str());
        required_node<Label>(*this, "InventoryPanel/%Status")
            .set_text(i18n::format(
                revealed ? N_("{name} casts Identify: {item}.")
                         : N_("{name} casts Identify and learns nothing new about {item}."),
                {{"name", caster_name}, {"item", i18n::text(name)}}));
    }
    catch (const std::exception &e)
    {
        required_node<Label>(*this, "InventoryPanel/%Status").set_text(i18n::text(e.what()));
    }
}

// The Cast menu: the selected member's spells and features outside combat,
// the same ones the Camp dialog's Cast / Use offers.
void RolfTourView::cast_menu()
{
    auto *panel = &required_node<Control>(*this, "CastPanel");
    if (panel->is_visible())
    {
        panel->hide();
        return;
    }
    if (!session_ || !campaign_ || !campaign_->selected() || campaign_->in_combat())
    {
        return;
    }
    scroll_item_.reset();
    required_node<Label>(*this, "CastPanel/%Status").set_text(String());
    refresh_cast();
    panel->show();
}

void RolfTourView::refresh_cast()
{
    auto *action = &required_node<OptionButton>(*this, "CastPanel/%Action");
    auto *target = &required_node<OptionButton>(*this, "CastPanel/%Target");
    action->clear();
    target->clear();
    if (!campaign_ || !campaign_->selected())
    {
        return;
    }
    const auto caster = campaign_->selected();
    const auto &member = campaign_->member(caster);
    // A scroll read or copied from: the member's own, while it still holds a
    // spell (MAGIC-6).
    if (scroll_item_ && !member.character.inventory().find(*scroll_item_))
    {
        scroll_item_.reset();
    }
    const auto name = String::utf8(member.character.sheet().name.c_str());
    required_node<Label>(*this, "CastPanel/%Header")
        .set_text(scroll_item_ ? i18n::format(N_("{name}: read a scroll"), {{"name", name}})
                               : i18n::format(N_("{name}: cast a spell or use a feature"),
                                              {{"name", name}}));
    cast_options_.clear();
    const auto add = [&](const String &label, const std::string &id, bool whole_party)
    {
        action->add_item(label);
        action->set_item_metadata(action->get_item_count() - 1, String::utf8(id.c_str()));
        cast_options_.push_back({id, whole_party});
    };
    if (scroll_item_)
    {
        for (const auto &offered : campaign_->scroll_actions(caster, *scroll_item_))
        {
            add(i18n::render(offered.label), offered.id, offered.whole_party);
        }
        // Copying takes a bottle of Magic Ink of the spell's level or higher
        // and two hours a spell level; the original's scribing takes no ink
        // and is done at the next day's rest.
        const bool original = campaign_->rule_module().plays_original_records();
        for (const auto &copy : campaign_->copyable_spells(caster, *scroll_item_))
        {
            if (original)
            {
                add(i18n::format(N_("Scribe {spell} into the spellbook at the next rest"),
                                 {{"spell", i18n::text(copy.name)}}),
                    "copy:" + std::to_string(copy.entry), true);
                continue;
            }
            add(i18n::format(
                    N_("Copy {spell} into the spellbook (Magic Ink of level {level} or higher, {hours} hours)"),
                    {{"spell", i18n::text(opengold::srd5::spell_label(copy.spell))},
                     {"level", static_cast<int>(copy.level)},
                     {"hours", static_cast<int>(2 * copy.level)}}),
                "copy:" + std::to_string(copy.entry), true);
        }
    }
    else
    {
        auto actions = campaign_->camp_actions(caster);
        std::vector<String> labels;
        for (const auto &offered : actions)
        {
            labels.push_back(i18n::render(offered.label));
        }
        expand_teleport(actions, labels);
        for (std::size_t n = 0; n < actions.size(); ++n)
        {
            add(labels[n], actions[n].id, actions[n].whole_party);
        }
    }
    for (const auto id : campaign_->state().slots)
    {
        if (id)
        {
            const int index = target->get_item_count();
            target->add_item(String::utf8(campaign_->member(id).character.sheet().name.c_str()));
            target->set_item_metadata(index, static_cast<int64_t>(id));
        }
    }
    const bool offered = action->get_item_count() > 0;
    required_node<Button>(*this, "CastPanel/%Use").set_disabled(!offered);
    if (!offered)
    {
        required_node<Label>(*this, "CastPanel/%Status")
            .set_text(i18n::text(N_("Nothing to cast or use outside combat.")));
    }
    cast_action_selected(action->get_selected());
}

void RolfTourView::expand_teleport(std::vector<opengold::rules::CampAction> &actions,
                                   std::vector<String> &labels) const
{
    const auto teleport = [](const std::string &id)
    {
        return id == "teleport" || id.starts_with("teleport_");
    };
    const auto first = std::find_if(actions.begin(), actions.end(),
                                    [&](const auto &offered)
                                    {
                                        return teleport(offered.id);
                                    });
    if (first == actions.end())
    {
        return;
    }
    const auto at = static_cast<std::size_t>(first - actions.begin());
    const auto cheapest = actions[at];
    const auto label = labels[at];
    for (std::size_t n = actions.size(); n-- > 0;)
    {
        if (teleport(actions[n].id))
        {
            actions.erase(actions.begin() + static_cast<std::ptrdiff_t>(n));
            labels.erase(labels.begin() + static_cast<std::ptrdiff_t>(n));
        }
    }
    const auto destinations = dax() ? dax()->teleport_destinations() : std::vector<unsigned>{};
    for (std::size_t n = 0; n < destinations.size(); ++n)
    {
        auto entry = cheapest;
        entry.id += "@" + std::to_string(destinations[n]);
        actions.insert(actions.begin() + static_cast<std::ptrdiff_t>(at + n), entry);
        labels.insert(labels.begin() + static_cast<std::ptrdiff_t>(at + n),
                      i18n::format(N_("{spell}: {place}"),
                                   {{"spell", label},
                                    {"place", i18n::text(teleport_place(destinations[n]))}}));
    }
}

void RolfTourView::cast_action_selected(std::int64_t index)
{
    // A whole-party action, and copying a spell, choose no member.
    const bool party = index >= 0 && static_cast<std::size_t>(index) < cast_options_.size() &&
                       cast_options_[static_cast<std::size_t>(index)].whole_party;
    required_node<OptionButton>(*this, "CastPanel/%Target").set_visible(!party);
}

void RolfTourView::cast_use()
{
    auto *status = &required_node<Label>(*this, "CastPanel/%Status");
    try
    {
        auto *action = &required_node<OptionButton>(*this, "CastPanel/%Action");
        auto *target = &required_node<OptionButton>(*this, "CastPanel/%Target");
        if (!campaign_ || campaign_->in_combat() || action->get_selected() < 0 ||
            target->get_selected() < 0)
        {
            return;
        }
        const String id = action->get_selected_metadata();
        const auto patient =
            static_cast<unsigned>(static_cast<int64_t>(target->get_selected_metadata()));
        if (scroll_item_)
        {
            read_or_copy_scroll(std::string(id.utf8().get_data()), patient,
                                action->get_item_text(action->get_selected()));
            return;
        }
        const auto result = use_outside_combat(
            campaign_->selected(), patient, id, action->get_item_text(action->get_selected()),
            target->is_visible() ? target->get_item_text(target->get_selected()) : String());
        if (!result)
        {
            return;
        }
        refresh_cast();
        refresh();
        status->set_text(*result);
    }
    catch (const std::exception &e)
    {
        status->set_text(i18n::text(e.what()));
    }
}

void RolfTourView::party_selected(std::int64_t index)
{
    if (index < 0 || index >= 8 || !session_ || !campaign_ || !campaign_->state().slots[index])
    {
        return;
    }
    const auto slot = static_cast<unsigned>(index);
    if (session_->snapshot().phase == TourPhase::shopping ||
        session_->snapshot().phase == TourPhase::treasure)
    {
        // In a shop, clicking a member chooses the buyer; at a treasure, who takes.
        select_buyer(slot);
        return;
    }
    if (session_->can_select_member())
    {
        campaign_->select(opengold::PartySlot{slot});
    }
    if (required_node<Control>(*this, "CastPanel").is_visible())
    {
        // The Cast menu follows the chosen member, and their own spells: a
        // scroll belongs to the member who opened it.
        scroll_item_.reset();
        refresh_cast();
    }
    refresh();
}

void RolfTourView::open_member_sheet(std::int64_t index)
{
    party_selected(index);
    if (index < 0 || index >= 8 || !session_ || !campaign_ || !campaign_->state().slots[index])
    {
        return;
    }
    const auto slot = static_cast<unsigned>(index);
    if (embedded_party_)
    {
        emit_signal("party_member_selected", slot);
        return;
    }
    const auto &m = campaign_->member(campaign_->state().slots[slot]);
    const auto &s = m.character.sheet();
    std::string text = s.name + "\nLevel " + std::to_string(s.level) + " " + s.race + " " +
                       s.gender + " " + s.character_class + "\n" + s.alignment + " / " +
                       s.background + "\nXP " + std::to_string(m.experience) + "\nAC " +
                       std::to_string(campaign_->profile(m.id).armor_class) + " / HP " +
                       std::to_string(m.vitals.hit_points) + "/" + std::to_string(s.hit_points) +
                       "\n" + m.vitals.description + "\n\n";
    for (unsigned i = 0; i < 6; ++i)
    {
        const auto ability = opengold::rules::all_abilities[i];
        text += std::string(i18n::ability_names[i]) + ": " + std::to_string(s.scores[ability]) +
                " / Save " + std::to_string(s.saving_throws[ability]) + "\n";
    }
    required_node<RichTextLabel>(*this, "MemberSheet/%Text").set_text(String::utf8(text.c_str()));
    required_node<Window>(*this, "MemberSheet").popup_centered();
}

void RolfTourView::select_buyer(unsigned slot)
{
    if (!session_ || !campaign_ || !session_->can_select_member() || slot >= 8 ||
        !campaign_->state().slots[slot])
    {
        return;
    }
    campaign_->select(opengold::PartySlot{slot});
    refresh();
}

std::vector<unsigned> RolfTourView::occupied_slots() const
{
    std::vector<unsigned> slots;
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        if (campaign_->state().slots[slot])
        {
            slots.push_back(slot);
        }
    }
    return slots;
}

void RolfTourView::buyer_key(Key keycode)
{
    const auto slots = occupied_slots();
    if (slots.empty())
    {
        return;
    }
    if (keycode == Key::KEY_TAB)
    {
        // Cycle through the member rows in display order, wrapping at the end.
        const auto current =
            std::find(slots.begin(), slots.end(), campaign_->state().selected_slot.index);
        const auto next =
            current == slots.end() || current + 1 == slots.end() ? slots.begin() : current + 1;
        select_buyer(*next);
        return;
    }
    // Number keys pick the nth visible member row.
    const auto row = static_cast<std::size_t>(keycode) - static_cast<std::size_t>(Key::KEY_1);
    if (row < slots.size())
    {
        select_buyer(slots[row]);
    }
}

void RolfTourView::close_sheet()
{
    required_node<Window>(*this, "MemberSheet").hide();
}

void RolfTourView::level_up_requested(int slot)
{
    if (embedded_party_ && campaign_ && session_ && session_->can_leave())
    {
        emit_signal("level_up_requested", campaign_->state().slots.at(slot));
    }
}

void RolfTourView::movement(ExplorationCommand command)
{
    if (!session_)
    {
        return;
    }
    if (session_->explore(command))
    {
        refresh();
    }
    else if (session_->snapshot().phase == TourPhase::completed)
    {
        required_node<Label>(*this, "%Movement").set_text(i18n::text(N_("The way is blocked")));
    }
}

void RolfTourView::_input(const Ref<InputEvent> &event)
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  respond_to_input(event);
                              });
}

void RolfTourView::respond_to_input(const Ref<InputEvent> &event)
{
    if (ending_film_playing())
    {
        const Ref<InputEventKey> film_key = event;
        const Ref<InputEventMouseButton> film_click = event;
        // The original's own film takes a key only on screens it lets end it.
        const bool key_ends_it =
            !closing_film_ || closing_film_->screens.at(closing_screen_).after_key.has_value();
        if (key_ends_it &&
            ((film_key.is_valid() && film_key->is_pressed() && !film_key->is_echo()) ||
             (film_click.is_valid() && film_click->is_pressed())))
        {
            end_ending_film();
        }
        get_viewport()->set_input_as_handled();
        return;
    }
    if (required_node<Window>(*this, "RestDialog").is_visible())
    {
        return;
    }
    if (on_solar_map())
    {
        solar_map_pointer(event);
    }
    const Ref<InputEventKey> key = event;
    if (key.is_null() || !key->is_pressed() || key->is_echo())
    {
        return;
    }
    // Escape closes the inventory or the spell list first; with neither open
    // it goes on to the host, which opens the in-game menu.
    if (key->get_keycode() == Key::KEY_ESCAPE && !fight_waits_on_picture_)
    {
        if (required_node<Control>(*this, "InventoryPanel").is_visible())
        {
            inventory();
            get_viewport()->set_input_as_handled();
        }
        else if (required_node<Control>(*this, "CastPanel").is_visible())
        {
            cast_menu();
            get_viewport()->set_input_as_handled();
        }
        return;
    }
    if (fight_waits_on_picture_)
    {
        // The close-up opens the fight by itself; a key only hurries it.
        dismiss_monster_picture();
        get_viewport()->set_input_as_handled();
        return;
    }
    if (required_node<Control>(*this, "InventoryPanel").is_visible() ||
        required_node<Control>(*this, "CastPanel").is_visible())
    {
        return;
    }
    if (required_node<LineEdit>(*this, "%Answer").has_focus() &&
        key->get_keycode() != Key::KEY_ENTER)
    {
        return;
    }
    const bool listing = session_ && (session_->snapshot().phase == TourPhase::shopping ||
                                      session_->snapshot().phase == TourPhase::treasure);
    if (listing && campaign_ &&
        (key->get_keycode() == Key::KEY_TAB ||
         (key->get_keycode() >= Key::KEY_1 && key->get_keycode() <= Key::KEY_8)))
    {
        buyer_key(key->get_keycode());
        get_viewport()->set_input_as_handled();
        return;
    }
    if (session_ && (session_->snapshot().choices.size() > 1 || listing) &&
        key->get_keycode() != Key::KEY_ENTER)
    {
        return;
    }
    if (on_solar_map() && solar_map_key(key->get_keycode()))
    {
        get_viewport()->set_input_as_handled();
        return;
    }
    switch (key->get_keycode())
    {
    case Key::KEY_ENTER:
        next();
        break;
    case Key::KEY_LEFT:
        left();
        break;
    case Key::KEY_RIGHT:
        right();
        break;
    case Key::KEY_UP:
        forward();
        break;
    case Key::KEY_DOWN:
        movement(on_overland() ? ExplorationCommand::south : ExplorationCommand::turn_around);
        break;
    // The wilderness map's diagonal steps: Home, Page Up, End and Page Down, or
    // the keypad's corners.
    case Key::KEY_HOME:
    case Key::KEY_KP_7:
        movement(ExplorationCommand::northwest);
        break;
    case Key::KEY_PAGEUP:
    case Key::KEY_KP_9:
        movement(ExplorationCommand::northeast);
        break;
    case Key::KEY_END:
    case Key::KEY_KP_1:
        movement(ExplorationCommand::southwest);
        break;
    case Key::KEY_PAGEDOWN:
    case Key::KEY_KP_3:
        movement(ExplorationCommand::southeast);
        break;
    case Key::KEY_L:
        look();
        break;
    case Key::KEY_S:
        toggle_search();
        break;
    case Key::KEY_C:
        camp();
        break;
    case Key::KEY_M:
        // M for the original's Magic menu; unavailable while the Cast button is.
        if (!required_node<Button>(*this, "%Cast").is_disabled())
        {
            cast_menu();
        }
        break;
    default:
        return;
    }
    get_viewport()->set_input_as_handled();
}

void RolfTourView::_process(double delta)
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  advance_frame(delta);
                                  advance_closing_film(delta);
                              });
}

void RolfTourView::advance_frame(double delta)
{
    if (Engine::get_singleton()->is_editor_hint())
    {
        return;
    }
    if (rest_save_open_)
    {
        auto *save = Object::cast_to<Window>(get_parent()->get_node_or_null("SaveSlots"));
        if (!save || !save->is_visible())
        {
            rest_save_open_ = false;
            refresh_rest();
        }
    }
    if (note_seconds_ > 0 && (note_seconds_ -= delta) <= 0)
    {
        refresh();
    }
    if (session_)
    {
        session_->advance(checking_ ? 0.3 : delta);
        if (session_->snapshot().footsteps != played_footsteps_)
        {
            played_footsteps_ = session_->snapshot().footsteps;
            constexpr unsigned game_footstep = 10;
            if (!checking_ && game_sounds_)
            {
                game_sounds_->play(game_footstep);
            }
            else if (!checking_)
            {
                required_node<AudioStreamPlayer>(*this, "Footstep").play();
            }
        }
        if (session_->snapshot().script_sounds != played_script_sounds_)
        {
            played_script_sounds_ = session_->snapshot().script_sounds;
            if (!checking_ && game_sounds_)
            {
                game_sounds_->play(session_->snapshot().script_sound);
            }
        }
        if (session_->snapshot().thunder != played_thunder_)
        {
            played_thunder_ = session_->snapshot().thunder;
            if (!checking_ && original_sound_)
            {
                original_sound_->play(original_fireball_sound);
            }
        }
        follow_music(session_->snapshot());
        // Treasures' overland colors turn while it is shown.
        auto *treasures = dax();
        const bool colors_turned = treasures && !checking_ && treasures->turn_colors(delta);
        if (session_->snapshot().revision != shown_revision_ || colors_turned)
        {
            refresh();
        }
        if (shown_monster_picture_)
        {
            monster_picture_seconds_ += delta;
            required_node<Control>(*this, "TourVisuals").queue_redraw();
            if (fight_waits_on_picture_ &&
                monster_picture_seconds_ >= presentation::monster_picture_lasts(monster_delays_))
            {
                dismiss_monster_picture();
            }
        }
    }
    if (mastery_rest_check_)
    {
        check_mastery_rest_controls();
        return;
    }
    if (rest_check_)
    {
        check_rest_controls();
        return;
    }
    if (checking_)
    {
        check_run();
    }
}

namespace
{
String rest_notice(const std::string &resource, const std::string &text)
{
    auto result = i18n::campaign(resource, text);
    // Original dialogue stays in its original-content domain; these appended
    // messages are generated by our campaign host and use the engine catalog.
    for (
        const auto *source :
        {N_("Rest is not allowed here."),
         N_("Short rest complete: one hour passed; eligible members can spend Hit Dice."),
         N_("Long rest complete: eight hours passed; eligible members recovered HP and supported resources."),
         N_("Eight hours passed; no one can benefit from another Long Rest so soon."),
         N_("Rest denied: no active member is eligible."),
         N_("The rest was interrupted. Rest again to recover."),
         N_("Your camp is attacked!"),
         N_("The monsters go on their way."),
         N_("You get away."),
         N_("Your party flees the battle."),
         N_("You have lost the fight."),
         N_("Loot is retained until your party has room in its purses."),
         N_("This temple is now the party's sanctuary for Word of Recall."),
         N_("Word of Recall takes the party back to its sanctuary."),
         N_("Teleport takes the party away."),
         N_("The way is blocked."),
         N_("Locked."),
         N_("Wizard locked."),
         N_("You find a secret door in the north wall."),
         N_("You find a secret door in the east wall."),
         N_("You find a secret door in the south wall."),
         N_("You find a secret door in the west wall."),
         N_("The way to Stojanow Gate is not in the game yet."),
         N_("The way to Valjevo Castle is not in the game yet."),
         N_("The way to Valhingen Graveyard is not in the game yet."),
         N_("The way to Kovel Mansion is not in the game yet."),
         N_("The way to the nomad camp is not in the game yet."),
         N_("The way to the Pyramid is not in the game yet."),
         N_("The way to the Temple of Bane is not in the game yet."),
         N_("The way to the western wilderness is not in the game yet."),
         N_("The way to the eastern wilderness is not in the game yet."),
         N_("The river's stench drives you back."),
         N_("Tyranthraxus is destroyed, and Phlan is free of him. You have won."),
         N_("You have won."),
         N_("Eldamar's soul is free, and the Dreadlord is no more. You have won."),
         N_("The party gains experience."),
         N_("The way to another area is not in the game yet.")})
    {
        result = result.replace(String::utf8(source), i18n::text(source));
    }
    // The parley's spokesman and the members lost in a flight are named.
    const String speaks = " speaks for the party.", lost = " is left behind and lost.",
                 gains = " gains experience.";
    auto lines = result.split("\n");
    for (int n = 0; n < lines.size(); ++n)
    {
        if (lines[n].ends_with(speaks))
        {
            lines[n] =
                i18n::format(N_("{name} speaks for the party."),
                             {{"name", lines[n].substr(0, lines[n].length() - speaks.length())}});
        }
        else if (lines[n].ends_with(lost))
        {
            lines[n] =
                i18n::format(N_("{name} is left behind and lost."),
                             {{"name", lines[n].substr(0, lines[n].length() - lost.length())}});
        }
        else if (lines[n].begins_with("The party rests ") && lines[n].ends_with(" minutes."))
        {
            // "The party rests D days, H hours and M minutes."
            const auto numbers = lines[n].replace(",", " ").split(" ", false);
            lines[n] = i18n::format(
                N_("The party rests {days} days, {hours} hours and {minutes} minutes."),
                {{"days", numbers[3]}, {"hours", numbers[5]}, {"minutes", numbers[8]}});
        }
        else if (lines[n].ends_with(gains) && lines[n] != "The party gains experience.")
        {
            lines[n] =
                i18n::format(N_("{name} gains experience."),
                             {{"name", lines[n].substr(0, lines[n].length() - gains.length())}});
        }
    }
    return String("\n").join(lines);
}

// Choices the campaign host adds use the engine catalog; the rest are original.
String choice_label(const std::string &resource, const std::string &choice)
{
    for (const auto *host : {N_("Bash"), N_("Pick"), N_("Knock"), N_("Exit")})
    {
        if (choice == host)
        {
            return i18n::text(host);
        }
    }
    return i18n::campaign(resource + "/choices", choice);
}

String coin_list(const opengold::Coins &coins,
                 opengold::GoldBoxGame game = opengold::GoldBoxGame::pool_of_radiance)
{
    static constexpr std::array<const char *, 5> amounts{
        N_("{count} cp"), N_("{count} sp"), N_("{count} ep"), N_("{count} gp"), N_("{count} pp")};
    static constexpr std::array<const char *, 5> champions_amounts{
        N_("{count} silver"), N_("{count} copper"), N_("{count} bronze"), N_("{count} platinum"),
        N_("{count} steel")};
    const auto &labels = opengold::krynn_game(game) ? champions_amounts : amounts;
    String result;
    for (std::size_t coin = coins.size(); coin-- > 0;)
    {
        if (!coins[coin])
        {
            continue;
        }
        if (!result.is_empty())
        {
            result += ", ";
        }
        result += i18n::format(labels[coin], {{"count", coins[coin]}});
    }
    return result;
}

} // namespace

String payment_notice(const CoinPayment &payment, opengold::GoldBoxGame game)
{
    const auto &coins = payment.coins;
    const auto name = String::utf8(payment.payer.c_str());
    if (coins.change == opengold::Coins{})
    {
        return i18n::format("{name} pays {paid} for {owed}.",
                            {{"name", name},
                             {"paid", coin_list(coins.paid, game)},
                             {"owed", coin_list(coins.owed, game)}});
    }
    return i18n::format("{name} pays {paid} for {owed} and receives {change} in change.",
                        {{"name", name},
                         {"paid", coin_list(coins.paid, game)},
                         {"owed", coin_list(coins.owed, game)},
                         {"change", coin_list(coins.change, game)}});
}

namespace
{
// The shop types the Shop & Temple Manager offers, for the catalog: a shop
// names its type over the shopkeeper's portrait.
[[maybe_unused]] constexpr std::array shop_types{N_("Arms & Armor"), N_("General Items"),
                                                 N_("Silver Items"), N_("Jewelry"),
                                                 N_("Rogue's Shop"), N_("Magic User's Shop")};

// A bonus as the roll log shows it: "+5", "-1".
String signed_number(int number)
{
    return (number < 0 ? String() : String("+")) + String::num_int64(number);
}

// Damage dice as "1d8+3"; a fixed damage as its number.
String damage_dice(int dice, int sides, int bonus)
{
    if (!sides)
    {
        return String::num_int64(bonus);
    }
    return String::num_int64(dice) + "d" + String::num_int64(sides) +
           (bonus ? signed_number(bonus) : String());
}

// A weapon's hover lines for its buyer: the d20 roll that hits Armor Class 13
// (a natural 1 always misses and a 20 always hits) and the damage a hit deals.
String weapon_hover(const opengold::rules::WeaponAttackView &attack)
{
    const auto roll = std::clamp(opengold::rated_armor_class - attack.attack_bonus, 2, 20);
    const std::initializer_list<i18n::Argument> to_hit{
        {"ac", opengold::rated_armor_class},
        {"roll", roll},
        {"bonus", signed_number(attack.attack_bonus)}};
    String text =
        attack.disadvantage
            ? i18n::format(N_("To hit AC {ac}: roll {roll} or more ({bonus}), with Disadvantage"),
                           to_hit)
            : i18n::format(N_("To hit AC {ac}: roll {roll} or more ({bonus})"), to_hit);
    if (!attack.trained)
    {
        text += "\n" + i18n::text(N_("Not trained: no training bonus to hit"));
    }
    const auto damage = damage_dice(attack.dice, attack.sides, attack.damage_bonus);
    const auto type = i18n::text(attack.damage_type);
    text += "\n" +
            (attack.two_handed_sides
                 ? i18n::format(N_("Damage: {dice} {type}; {two_handed} with both hands"),
                                {{"dice", damage},
                                 {"type", type},
                                 {"two_handed", damage_dice(attack.dice, attack.two_handed_sides,
                                                            attack.damage_bonus)}})
                 : i18n::format(N_("Damage: {dice} {type}"), {{"dice", damage}, {"type", type}}));
    return text;
}

// A weight in hundredths of a pound as pounds: "3", "0.25".
String pounds(unsigned hundredths)
{
    return hundredths % 100 ? String::num(hundredths / 100.0, 2)
                            : String::num_uint64(hundredths / 100);
}

// What hovering over a shop's item tells of it for the member buying it: a
// weapon's roll to hit and damage, armor or a shield's Armor Class on the
// member, and every item's price and weight.
String shop_item_hover(const opengold::ItemPreview &preview, const String &item,
                       const String &buyer, std::uint32_t price,
                       opengold::GoldBoxGame game = opengold::GoldBoxGame::pool_of_radiance)
{
    String text = item;
    if (preview.attack)
    {
        text += "\n" + weapon_hover(*preview.attack);
    }
    if (preview.armor_class)
    {
        text += "\n" + i18n::format(N_("Armor Class: {ac} for {name} (now {now})"),
                                    {{"ac", *preview.armor_class},
                                     {"name", buyer},
                                     {"now", preview.armor_class_now}});
    }
    return text + "\n" +
           i18n::format(opengold::krynn_game(game) ? N_("{price} steel, {weight} lb")
                                                   : N_("{price} gp, {weight} lb"),
                        {{"price", price}, {"weight", pounds(preview.weight_hundredths)}});
}

// What the member carries, in whole pounds, rounded up so that a load at the
// limit never reads as under it.
std::int64_t carried_pounds(const opengold::CampaignParty &party, opengold::MemberId id)
{
    return (static_cast<std::int64_t>(party.carried_weight(id)) + 99) / 100;
}

// The campaign party's refusals a shop shows as its diagnostic, for the
// catalog.
[[maybe_unused]] constexpr std::array shop_refusals{N_("Not enough gold"),
                                                    N_("Too heavy to carry")};

String loot_notice(const LootTaken &loot,
                   opengold::GoldBoxGame game = opengold::GoldBoxGame::pool_of_radiance)
{
    PackedStringArray parts;
    const auto coins = coin_list(
        {loot.wealth[0], loot.wealth[1], loot.wealth[2], loot.wealth[3], loot.wealth[4]}, game);
    if (!coins.is_empty())
    {
        parts.push_back(coins);
    }
    const auto gems = loot.wealth[static_cast<std::size_t>(opengold::Coin::gems)];
    if (gems)
    {
        parts.push_back(i18n::plural("{count} gem", "{count} gems", static_cast<int>(gems)));
    }
    const auto jewelry = loot.wealth[static_cast<std::size_t>(opengold::Coin::jewelry)];
    if (jewelry)
    {
        parts.push_back(i18n::plural("{count} piece of jewelry", "{count} pieces of jewelry",
                                     static_cast<int>(jewelry)));
    }
    // Each kind of item once, in the order found, with how many there are.
    std::vector<std::pair<std::string, int>> kinds;
    for (const auto &item : loot.items)
    {
        const auto kind = std::find_if(kinds.begin(), kinds.end(),
                                       [&](const auto &seen)
                                       {
                                           return seen.first == item;
                                       });
        if (kind == kinds.end())
        {
            kinds.emplace_back(item, 1);
        }
        else
        {
            ++kind->second;
        }
    }
    for (const auto &[name, count] : kinds)
    {
        const auto label = String::utf8(name.c_str());
        parts.push_back(
            count == 1 ? label
                       : i18n::format(N_("{item} ×{count}"), {{"item", label}, {"count", count}}));
    }
    if (parts.is_empty())
    {
        return {};
    }
    return i18n::format(N_("The party takes {loot}."), {{"loot", String(", ").join(parts)}});
}

// What auto-equip (EQUIP-1) did for one member.
String gear_change_notice(const opengold::GearChange &change)
{
    const auto name = String::utf8(change.name.c_str());
    const auto items = [](const std::vector<std::string> &names)
    {
        PackedStringArray parts;
        for (const auto &item : names)
        {
            parts.push_back(i18n::text(item));
        }
        return String(", ").join(parts);
    };
    if (!change.burned_by.empty())
    {
        return i18n::format(
            N_("{name} takes up {item}, which burns them for {damage} damage and stays in the pack."),
            {{"name", name},
             {"item", i18n::text(change.burned_by)},
             {"damage", change.burn_damage}});
    }
    auto notice = i18n::format(N_("{name} equips {items}."),
                               {{"name", name}, {"items", items(change.equipped)}});
    if (!change.unequipped.empty())
    {
        notice += " " + i18n::plural(N_("{items} goes to the pack."), N_("{items} go to the pack."),
                                     static_cast<int>(change.unequipped.size()),
                                     {{"items", items(change.unequipped)}});
    }
    return notice;
}

// What auto-equip put on, a line for each member, each line first.
String gear_change_notices(const TourSnapshot &s)
{
    String notices;
    for (const auto &change : s.gear_changes)
    {
        notices += "\n" + gear_change_notice(change);
    }
    return notices;
}

String door_check_notice(const DoorCheck &check, opengold::GoldBoxGame game)
{
    const auto name = String::utf8(check.member.c_str());
    if (game == opengold::GoldBoxGame::champions_of_krynn &&
        check.method == opengold::DoorMethod::bash)
    {
        // Krynn uses its native Strength table and variable-sided dice;
        // the overall door result is printed below the attempts.
        return i18n::format("{name} tries to force the door using Strength.", {{"name", name}});
    }
    if (check.method == opengold::DoorMethod::knock)
    {
        return i18n::format("{name} casts Knock, and the lock opens.", {{"name", name}});
    }
    if (check.method == opengold::DoorMethod::pick && check.magic_lock_picks)
    {
        return i18n::format(
            "{name} uses up a set of Magic Lock Picks for Advantage{tools}: Dexterity (Sleight of Hand) {total} (d20 {die}) against DC {dc}.",
            {{"name", name},
             {"tools", check.thieves_tools
                           ? i18n::text(N_(", with Thieves' Tools doubling the chance"))
                           : String()},
             {"total", check.total},
             {"die", check.die},
             {"dc", check.difficulty}});
    }
    if (check.method == opengold::DoorMethod::pick && check.thieves_tools)
    {
        return i18n::format(
            "{name} tries to pick the lock with Thieves' Tools, which double the chance: Dexterity (Sleight of Hand) {total} (d20 {die}) against DC {dc}.",
            {{"name", name}, {"total", check.total}, {"die", check.die}, {"dc", check.difficulty}});
    }
    if (check.method == opengold::DoorMethod::pick)
    {
        return i18n::format(
            "{name} tries to pick the lock: Dexterity (Sleight of Hand) {total} (d20 {die}) against DC {dc}.",
            {{"name", name}, {"total", check.total}, {"die", check.die}, {"dc", check.difficulty}});
    }
    return i18n::format(
        "{name} tries to force the door: Strength (Athletics) {total} (d20 {die}) against DC {dc}.",
        {{"name", name}, {"total", check.total}, {"die", check.die}, {"dc", check.difficulty}});
}

String secret_door_search_notice(const SecretDoorSearch &search)
{
    return i18n::format("{name} searches for a secret door: d6 {die}, needing {needed} or less.",
                        {{"name", String::utf8(search.member.c_str())},
                         {"die", search.die},
                         {"needed", search.needed}});
}

// Script dialogue followed by the coins the party's purses exchanged for it,
// any checks made against a locked door and any search for a secret door.
String event_dialogue(const std::string &resource, const TourSnapshot &s,
                      opengold::GoldBoxGame game = opengold::GoldBoxGame::pool_of_radiance)
{
    // Our campaign host's own dialogue, such as a training hall's, is
    // translated from its messages; the script's text is original.
    auto result =
        s.host_dialogue.empty() ? rest_notice(resource, s.dialogue) : i18n::render(s.host_dialogue);
    for (const auto &payment : s.payments)
    {
        result += "\n" + payment_notice(payment, game);
    }
    for (const auto &loot : s.loot_taken)
    {
        const auto notice = loot_notice(loot, game);
        if (!notice.is_empty())
        {
            result += (result.is_empty() ? "" : "\n") + notice;
        }
    }
    result += gear_change_notices(s);
    for (const auto &check : s.door_checks)
    {
        result += "\n" + door_check_notice(check, game);
    }
    if (!s.door_checks.empty())
    {
        result +=
            "\n" + i18n::text(s.door_opened ? N_("The door opens.") : N_("The door stays locked."));
    }
    for (const auto &search : s.secret_door_searches)
    {
        result += (result.is_empty() ? "" : "\n") + secret_door_search_notice(search);
    }
    return result;
}
} // namespace

void RolfTourView::sync_monster_picture()
{
    const auto *picture = dax() ? dax()->scene_animation() : nullptr;
    const bool fight_waits = dax() && dax()->monster_picture();
    if (picture == shown_monster_picture_)
    {
        fight_waits_on_picture_ = fight_waits;
        return;
    }
    forget_monster_picture();
    fight_waits_on_picture_ = fight_waits;
    shown_monster_picture_ = picture;
    if (!picture)
    {
        return;
    }
    for (const auto &frame : *picture)
    {
        monster_frames_.push_back(presentation::image_texture(frame.image));
        monster_delays_.push_back(frame.delay);
    }
    // New art drawn from the close-up's description replaces it, with its
    // timing, unless the 3D view shows the original's art; a folder that cannot
    // be used leaves the original, and the log names its file.
    const auto source = dax_session().scene_animation_source();
    const auto closeups = new_art_path("closeups");
    if (!source || original_walls_ || closeups.is_empty())
    {
        return;
    }
    const auto folder =
        closeups.path_join(presentation::close_up_folder_name(source->first, source->second));
    auto replacement = presentation::load_close_up_art(folder, monster_delays_);
    if (!replacement.problem.is_empty())
    {
        log_unloadable_art(std::string(
            ("Cannot load close-up art " + folder + ": " + replacement.problem).utf8().get_data()));
    }
    if (replacement.art)
    {
        monster_frames_ = std::move(replacement.art->frames);
        monster_delays_ = std::move(replacement.art->delays);
    }
}

void RolfTourView::forget_monster_picture()
{
    shown_monster_picture_ = nullptr;
    fight_waits_on_picture_ = false;
    monster_frames_.clear();
    monster_delays_.clear();
    monster_picture_seconds_ = 0;
    monster_picture_check_frames_ = 0;
}

std::size_t RolfTourView::current_monster_frame() const
{
    return presentation::monster_frame_at(monster_delays_, monster_picture_seconds_);
}

void RolfTourView::dismiss_monster_picture()
{
    if (dax() && dax()->start_encounter())
    {
        refresh();
    }
}

void RolfTourView::refresh()
{
    layout();
    if (session_ && session_->snapshot().game_won != won_game_shown_)
    {
        won_game_shown_ = session_->snapshot().game_won;
        if (won_game_shown_)
        {
            play_ending_film();
        }
    }
    if (session_)
    {
        emit_signal("game_shown", session_->snapshot().game_won);
    }
    sync_monster_picture();
    if (auto *campaign = dax())
    {
        campaign->show_original_walls(original_walls_);
    }
    if (dax() && rendered_sprite_id_ != session_->snapshot().sprite_id)
    {
        for (unsigned n = 0; n < 3; ++n)
        {
            const auto &source = dax()->sprites()[n];
            // Only Rolf's tour has a guide to show (Curse's campaign has none).
            if (source.width == 0 || source.height == 0)
            {
                continue;
            }
            sprites_[n] = presentation::image_texture(source);
        }
        rendered_sprite_id_ = session_->snapshot().sprite_id;
    }
    // A design's view shows from its start; a DAX campaign's once the party
    // has stood somewhere.
    const auto solar_state =
        on_solar_map()
            ? std::optional<opengold::original::SolarMapState>(dax_session().solar_map_state())
            : std::nullopt;
    if (session_ &&
        (!dax() || session_->snapshot().visited.any() || session_->snapshot().overland) &&
        (!rendered_pose_ || *rendered_pose_ != session_->snapshot().pose ||
         rendered_overland_ != std::array{session_->snapshot().overland_x,
                                          session_->snapshot().overland_y,
                                          session_->snapshot().overland_direction} ||
         rendered_picture_revision_ != session_->snapshot().picture_revision ||
         rendered_solar_ != solar_state))
    {
        try
        {
            const auto pose = session_->snapshot().pose;
            const auto source = session_->view_picture();
            if (!source)
            {
                // Nothing to draw here yet (a design's overland): no 3D view.
                wall_image_.unref();
                wall_view_.unref();
            }
            else
            {
                const auto image = presentation::rgba_image(*source);
                wall_image_ = image;
                if (wall_view_.is_null())
                {
                    wall_view_ = ImageTexture::create_from_image(image);
                }
                else
                {
                    wall_view_->set_image(image);
                }
            }
            rendered_pose_ = pose;
            rendered_overland_ = {session_->snapshot().overland_x, session_->snapshot().overland_y,
                                  session_->snapshot().overland_direction};
            rendered_picture_revision_ = session_->snapshot().picture_revision;
            rendered_solar_ = solar_state;
        }
        catch (const std::exception &error)
        {
            session_.reset();
            forget_monster_picture();
            error_ = String::utf8(error.what());
        }
    }
    const bool loaded = session_ != nullptr;
    const auto game = dax() ? dax()->game() : opengold::GoldBoxGame::pool_of_radiance;
    const bool champions = opengold::krynn_game(game);
    static const TourSnapshot none;
    const TourSnapshot &s = loaded ? session_->snapshot() : none;
    refresh_rest();
    const bool waiting = loaded && s.phase == TourPhase::awaiting_continue;
    const bool completed = loaded && s.phase == TourPhase::completed;
    // A script that opens the camp menu (the nomads' lodging) shows the camp
    // dialog once for that event.
    if (completed && s.camp_offered && camp_offer_run_ != s.event_runs)
    {
        camp_offer_run_ = s.event_runs;
        camp();
    }
    if (completed && s.camp_menu_open && camp_menu_run_ != s.event_runs)
    {
        camp_menu_run_ = s.event_runs;
        show_camp_dialog();
    }
    const bool faulted = !loaded || s.phase == TourPhase::faulted;
    const bool treasure = loaded && s.phase == TourPhase::treasure;
    // A treasure's pile is listed and chosen from as a shop's stock is.
    const bool shopping = loaded && (s.phase == TourPhase::shopping || treasure);
    const bool answer = loaded && s.phase == TourPhase::awaiting_input;
    const bool multiple = waiting && s.choices.size() > 1;
    const String district = place_name(application_settings(*this).chosen_game(), session_.get());
    // While a shopkeeper greets or serves the party, the shopkeeper's name
    // and the shop's type stand over the portrait.
    const auto *shop = dax() ? opengold::phlan_shop_serving(s, dax()->shops()) : nullptr;
    required_node<Label>(*this, "%Location")
        .set_text(
            shop ? i18n::format(N_("{shopkeeper} - {type}"),
                                {{"shopkeeper", String::utf8(shop->shopkeeper.c_str())},
                                 {"type", i18n::text(shop->type)}})
                 : i18n::format("{district} / {direction} view",
                                {{"district", district},
                                 {"direction", i18n::text(direction_name[index(s.pose.facing)])}}));
    required_node<Label>(*this, "%MapTitle")
        .set_text(i18n::format("{district}    N ↑", {{"district", district}}));
    // Rolf's welcome names the opening tour; afterwards the title names the area.
    required_node<Label>(*this, "%Title")
        .set_text(s.tour_finished
                      ? i18n::format("OPENGOLDBOX  /  {district}", {{"district", district}})
                      : i18n::text("OPENGOLDBOX  /  Rolf's welcome"));
    auto coordinates =
        i18n::format("Party ({x}, {y})   {direction}",
                     {{"x", s.pose.x},
                      {"y", s.pose.y},
                      {"direction", i18n::text(direction_name[index(s.pose.facing)])}});
    if (opengold::is_buck_rogers(game) && s.overland && !s.solar_map)
    {
        coordinates = i18n::format(
            "Party ({x}, {y})   {direction}",
            {{"x", s.overland_x},
             {"y", s.overland_y},
             {"direction", i18n::text(wilderness_direction_name[s.overland_direction % 8])}});
    }
    if (s.searching)
    {
        coordinates += String("   ") + i18n::text(N_("Searching"));
    }
    required_node<Label>(*this, "%Coordinates").set_text(coordinates);
    required_node<Label>(*this, "%Speaker")
        .set_text(faulted           ? i18n::text(N_("Unable to continue"))
                  : shopping        ? i18n::text(N_("Shop / select an item"))
                  : s.tour_finished ? district
                                    : i18n::text(N_("Rolf  /  Council guide")));
    if (treasure && campaign_ && campaign_->selected())
    {
        const auto &taker = campaign_->member(campaign_->selected());
        required_node<Label>(*this, "%Speaker")
            .set_text(
                i18n::format(N_("Treasure / {name} takes: carrying {carried}/{capacity} lb"),
                             {{"name", String::utf8(taker.character.sheet().name.c_str())},
                              {"carried", carried_pounds(*campaign_, campaign_->selected())},
                              {"capacity", campaign_->carrying_capacity(campaign_->selected())}}));
    }
    else if (shopping && campaign_ && campaign_->selected())
    {
        const auto &buyer = campaign_->member(campaign_->selected());
        required_node<Label>(*this, "%Speaker")
            .set_text(i18n::format(
                champions
                    ? N_("Shop / buying for {name}: party {gold} steel, carrying {carried}/{capacity} lb")
                    : N_("Shop / buying for {name}: party {gold} gp, carrying {carried}/{capacity} lb"),
                {{"name", String::utf8(buyer.character.sheet().name.c_str())},
                 {"gold", campaign_->purse_gold()},
                 {"carried", carried_pounds(*campaign_, campaign_->selected())},
                 {"capacity", campaign_->carrying_capacity(campaign_->selected())}}));
    }
    const auto resource =
        "por/area/" + std::to_string(s.area_id) + "/script/" + std::to_string(s.script_id);
    // The text that led to a fight stays up while the fight's close-up plays.
    if (!fight_waits_on_picture_ || !s.dialogue.empty())
    {
        required_node<RichTextLabel>(*this, "%Dialogue")
            .set_text(
                faulted
                    ? (loaded ? i18n::text(s.diagnostic) : error_) + "\n" +
                          i18n::text(
                              "Restart with --reset-game-path to choose your Pool of Radiance data folder.")
                : s.dialogue.empty() && s.secret_door_searches.empty() && s.loot_taken.empty()
                    ? (s.tour_finished ? String() : i18n::text("Following Rolf..."))
                    : event_dialogue(resource + "/dialogue", s, game));
    }
    required_node<Button>(*this, "%Continue")
        .set_disabled(fight_waits_on_picture_ || (!waiting && !shopping && !answer));
    required_node<Button>(*this, "%Continue")
        .set_text(i18n::text(treasure   ? N_("Take [Enter]")
                             : shopping ? N_("Buy [Enter]")
                             : multiple ? N_("Choose [Enter]")
                             : answer   ? N_("Submit [Enter]")
                                        : N_("Continue [Enter]")));
    required_node<Button>(*this, "%Continue").set_visible(!completed);
    required_node<Label>(*this, "%Progress")
        .set_text(faulted                 ? i18n::text("Stopped")
                  : treasure && campaign_ ? i18n::text(N_("Tab or number keys: choose who takes"))
                  : shopping && campaign_ ? i18n::text(N_("Tab or number keys: choose the buyer"))
                  : s.tour_finished || waiting ? String()
                                               : i18n::text("Following the guide"));
    required_node<Label>(*this, "%Movement")
        .set_text(note_seconds_ > 0 ? note_
                                    : i18n::text(completed ? N_("Explore  /  arrow keys")
                                                           : N_("Movement paused")));
    for (const char *name : {"%Left", "%Forward", "%Right", "%Look", "%Camp", "%Cast", "%Search"})
    {
        required_node<Button>(*this, name).set_disabled(!completed);
    }
    // Pressed while Search mode is on, whether the button or S turned it on.
    required_node<Button>(*this, "%Search").set_pressed_no_signal(s.searching);
    const bool overland = s.overland;
    required_node<Button>(*this, "%Left")
        .set_text(i18n::text(overland ? N_("← West") : N_("← Turn")));
    required_node<Button>(*this, "%Forward")
        .set_text(i18n::text(overland ? N_("↑ North") : N_("↑ Step")));
    required_node<Button>(*this, "%Right")
        .set_text(i18n::text(overland ? N_("East →") : N_("Turn →")));
    required_node<Button>(*this, "%LeaveShop").set_visible(shopping);
    required_node<Button>(*this, "%LeaveShop")
        .set_text(i18n::text(!treasure                ? N_("Leave shop")
                             : treasure_leave_warned_ ? N_("Leave it behind")
                                                      : N_("Leave")));
    required_node<LineEdit>(*this, "%Answer").set_visible(answer);
    required_node<LineEdit>(*this, "%Answer").set_max_length(s.number_input ? 6 : 40);
    auto *choices = &required_node<ItemList>(*this, "%Choices");
    choices->set_visible(shopping || multiple);
    fit_choice_columns();
    // A shop's filter shows in a shop, and lists everything again in the next.
    const bool in_shop = shopping && !treasure;
    required_node<Control>(*this, "%FilterLabel").set_visible(in_shop);
    required_node<Control>(*this, "%ShopFilter").set_visible(in_shop);
    if (!in_shop && shop_filter_ != all_items_filter)
    {
        shop_filter_ = all_items_filter;
        required_node<OptionButton>(*this, "%ShopFilter")
            .select(static_cast<std::int32_t>(all_items_filter));
    }
    if (displayed_ticket_ != s.continue_ticket)
    {
        choices->clear();
        if (treasure)
        {
            for (const auto &item : dax_session().treasure_pile())
            {
                choices->add_item(i18n::text(item.label()));
            }
        }
        else if (shopping)
        {
            // The stock the filter passes, in alphabetical order of its names
            // as the player reads them, down each column.
            shop_rows_.clear();
            const auto &stock = dax_session().shop_stock();
            std::vector<String> names;
            for (std::size_t n = 0; n < stock.size(); ++n)
            {
                names.push_back(i18n::text(stock[n].label()));
                if (passes_shop_filter(stock[n]))
                {
                    shop_rows_.push_back(n);
                }
            }
            std::ranges::stable_sort(shop_rows_,
                                     [&](std::size_t a, std::size_t b)
                                     {
                                         return names[a].naturalnocasecmp_to(names[b]) < 0;
                                     });
            shop_rows_ = down_the_columns(shop_rows_, shop_column_count);
            for (const auto n : shop_rows_)
            {
                const auto &item = stock[n];
                // Stock without an equipment conversion stays for sale, disclosed.
                const bool usable =
                    !opengold::equipment_conversion(item).starts_with("por:unsupported:");
                choices->add_item(i18n::format(
                    champions ? (usable ? N_("{item} / {price} steel")
                                        : N_("{item} / {price} steel / cannot be equipped"))
                              : (usable ? N_("{item} / {price} gp")
                                        : N_("{item} / {price} gp / cannot be equipped")),
                    {{"item", names[n]}, {"price", item.stored.value}}));
            }
        }
        else if (!s.host_choices.empty())
        {
            for (const auto &c : s.host_choices)
            {
                choices->add_item(i18n::render(c));
            }
        }
        else
        {
            for (const auto &c : s.choices)
            {
                choices->add_item(s.choosing_member ? String::utf8(c.c_str())
                                                    : choice_label(resource, c));
            }
        }
        if (choices->get_item_count())
        {
            choices->select(0);
        }
        if (shopping || multiple)
        {
            choices->grab_focus();
        }
        if (answer)
        {
            required_node<LineEdit>(*this, "%Answer").clear();
            required_node<LineEdit>(*this, "%Answer").grab_focus();
        }
        displayed_ticket_ = s.continue_ticket;
    }
    // What the chosen member could not use fully is drawn in dark red; in a
    // shop, else what the purse cannot pay for in yellow, else armor or a
    // shield that would not raise the member's Armor Class in gray. All are
    // drawn again whenever another member is chosen.
    String color_notes;
    if (shopping && campaign_ && campaign_->selected())
    {
        const auto chosen = campaign_->selected();
        const auto name = String::utf8(campaign_->member(chosen).character.sheet().name.c_str());
        const auto &listed = treasure ? dax_session().treasure_pile() : dax_session().shop_stock();
        const auto purse = campaign_->purse_gold();
        bool any_unusable = false, any_unaffordable = false, any_no_gain = false;
        for (int row = 0; row < choices->get_item_count() && row < static_cast<int>(listed.size());
             ++row)
        {
            const auto &item = listed[treasure ? static_cast<std::size_t>(row)
                                               : shop_rows_.at(static_cast<std::size_t>(row))];
            // The theme's shop_item_* colors: dark red for an item the chosen
            // member cannot use fully, yellow for one the purse cannot pay for,
            // gray for armor or a shield that would not raise its Armor Class.
            Color color;
            if (!campaign_->can_use(chosen, item))
            {
                any_unusable = true;
                color = get_theme_color("shop_item_unusable", "OpenGoldPalette");
            }
            else if (!treasure && item.stored.value > purse)
            {
                any_unaffordable = true;
                color = get_theme_color("shop_item_unaffordable", "OpenGoldPalette");
            }
            else if (!treasure && campaign_->adds_no_armor_class(chosen, item))
            {
                any_no_gain = true;
                color = get_theme_color("shop_item_no_gain", "OpenGoldPalette");
            }
            choices->set_item_custom_fg_color(row, color);
            if (!treasure)
            {
                choices->set_item_tooltip(row, shop_item_hover(campaign_->preview(chosen, item),
                                                               i18n::text(item.label()), name,
                                                               item.stored.value, game));
            }
        }
        std::vector<String> notes;
        if (any_unusable)
        {
            notes.push_back(i18n::format(
                treasure
                    ? N_("Dark red: {name} is not trained to use it, or cannot use it.")
                    : N_("Dark red: {name} is not trained to use it, or cannot use it, but can still buy it."),
                {{"name", name}}));
        }
        if (any_unaffordable)
        {
            notes.push_back(i18n::text(N_("Yellow: the party cannot afford it.")));
        }
        if (any_no_gain)
        {
            notes.push_back(i18n::format(N_("Gray: it would not raise {name}'s Armor Class."),
                                         {{"name", name}}));
        }
        for (const auto &note : notes)
        {
            color_notes += (color_notes.is_empty() ? String() : String("  ")) + note;
        }
    }
    auto &dialogue_box = required_node<Node>(*this, "%DialogueBounds");
    dialogue_box.call("show_mode", shopping   ? "shopping"
                                   : multiple ? "multiple"
                                   : answer   ? "answer"
                                              : "normal");
    if (treasure)
    {
        // The list leaves room for one line: the warning, else the last take
        // and what auto-equip put on with it, else the coins the party took.
        const auto equipped = gear_change_notices(s).replace("\n", " ");
        String line =
            treasure_leave_warned_
                ? i18n::text(N_("There is still treasure here. Leave it behind, or take more?"))
                : treasure_notice_ +
                      (treasure_notice_.is_empty() ? equipped.trim_prefix(" ") : equipped);
        for (const auto &loot : s.loot_taken)
        {
            if (line.is_empty())
            {
                line = loot_notice(loot, game);
            }
        }
        // The dark red note follows the coins or the prompt, not a take or
        // a warning.
        if (line.is_empty() || (!treasure_leave_warned_ && treasure_notice_.is_empty()))
        {
            const auto lead = line.is_empty() ? i18n::text(N_("Choose what to take.")) : line;
            line = color_notes.is_empty() ? lead : lead + String("  ") + color_notes;
        }
        dialogue_box.call("show_news", line);
    }
    else if (shopping)
    {
        // A purchase's news first; otherwise the color notes, else prices.
        dialogue_box.call("show_news", (!s.diagnostic.empty() ? i18n::text(s.diagnostic)
                                        : !color_notes.is_empty()
                                            ? color_notes
                                            : i18n::text("Prices are per listed item or bundle.")) +
                                           gear_change_notices(s));
    }
    if (answer && !s.diagnostic.empty())
    {
        required_node<RichTextLabel>(*this, "%Dialogue")
            .add_text("\n" + String::utf8(s.diagnostic.c_str()));
    }
    if (waiting && !s.diagnostic.empty())
    {
        required_node<RichTextLabel>(*this, "%Dialogue")
            .set_text(i18n::text(s.diagnostic) + "\n" +
                      event_dialogue(resource + "/dialogue", s, game));
    }
    if (const auto *campaign = dax())
    {
        const auto &p = campaign->party();
        required_node<Label>(*this, "%Party")
            .set_text(
                i18n::format("Fighter / Level 1 / HP {current}/{maximum}\n{gold} gp / {items}",
                             {{"current", p.hit_points},
                              {"maximum", p.max_hit_points},
                              {"gold", gold_value(p.wealth)},
                              {"items", i18n::plural("{count} item", "{count} items",
                                                     static_cast<int>(p.inventory.size()))}}));
    }
    if (loaded && campaign_ && campaign_->selected())
    {
        const auto &m = campaign_->member(campaign_->selected());
        required_node<Label>(*this, "%Party")
            .set_text(i18n::format(
                champions ? N_("{name} / HP {current}/{maximum}\nparty {gold} steel / {items}")
                          : N_("{name} / HP {current}/{maximum}\nparty {gold} gp / {items}"),
                {{"name", String::utf8(m.character.sheet().name.c_str())},
                 {"current", m.vitals.hit_points},
                 {"maximum", campaign_->hit_point_maximum(campaign_->selected())},
                 {"gold", campaign_->purse_gold()},
                 {"items",
                  i18n::plural("{count} item", "{count} items",
                               static_cast<int>(m.character.inventory().items().size()))}}));
    }
    show_solar_map(s);
    show_party_rows();
    required_node<Control>(*this, "TourVisuals").queue_redraw();
    // Marked shown only once everything above succeeded, so a failed refresh
    // is tried again on the next frame (Effective C++ Item 29).
    shown_revision_ = s.revision;
}

namespace
{
// What a member holds and wears, for the party list: the weapons in hand, then
// the armor and shield. Rings, cloaks and the like are left to the sheet.
struct GearLines
{
    String weapons, armor;
};

GearLines gear_lines(const opengold::CampaignParty &party, const opengold::PartyMember &member)
{
    String weapons, armor, shield;
    const auto items = member.character.inventory().items();
    for (const auto id : member.equipped)
    {
        const auto item = std::ranges::find(items, id, &opengold::InventoryItem::id);
        if (item == items.end())
        {
            continue;
        }
        const auto name = i18n::text(item->name);
        switch (party.equipment_info(member.id, id).slot)
        {
        case opengold::rules::EquipmentSlot::weapon:
            weapons += (weapons.is_empty() ? String() : String(", ")) + name;
            break;
        case opengold::rules::EquipmentSlot::armor:
            armor = name;
            break;
        case opengold::rules::EquipmentSlot::shield:
            shield = name;
            break;
        default:
            break;
        }
    }
    if (!shield.is_empty())
    {
        armor += (armor.is_empty() ? String() : String(", ")) + shield;
    }
    return {weapons.is_empty() ? i18n::text(N_("No weapon")) : weapons,
            armor.is_empty() ? i18n::text(N_("No armor")) : armor};
}

// The Armor Class and hit points line in three parts: before, at and after the
// current hit points, so the row can color them alone wherever the language
// puts them in the line.
PackedStringArray armor_and_hit_points(int armor_class, int current, int maximum)
{
    // A private-use character no message holds marks the current hit points.
    const auto marker = String::chr(0xE000);
    const auto line =
        i18n::format("AC {ac} / HP {current}/{maximum}",
                     {{"ac", armor_class}, {"current", marker}, {"maximum", maximum}});
    const auto at = line.find(marker);
    return {line.substr(0, at), String::num_int64(current), line.substr(at + marker.length())};
}

// The palette color for the current hit points: yellow under a fifth of the
// maximum, red under a tenth, none otherwise.
StringName low_hit_points_color(int current, int maximum)
{
    if (current * 10 < maximum)
    {
        return "hp_critical";
    }
    if (current * 5 < maximum)
    {
        return "hp_low";
    }
    return {};
}
} // namespace

// Each member's card, the same while exploring and in a shop: the portrait,
// then name, race and class, then Armor Class and hit points; its tooltip adds
// gender, the weapon in hand and the armor worn. The selected member, who
// receives services, is highlighted, and the list scrolls to them when the
// selection changes.
void RolfTourView::show_party_rows()
{
    const auto selected = campaign_ ? campaign_->state().selected_slot.index : 0U;
    for (unsigned slot = 0; slot < 8; ++slot)
    {
        auto *button = &required_node<Button>(*this, String("%PartyList/Rows/Member") +
                                                         String::num_uint64(slot));
        const auto id = campaign_ ? campaign_->state().slots[slot] : 0;
        button->set_visible(id != 0);
        if (!id)
        {
            continue;
        }
        if (!portraits_)
        {
            portraits_ = PortraitCatalog::load(application_settings(*this));
        }
        const bool leader = id == campaign_->leader();
        button->call("set_highlight", slot == selected);
        const auto &m = campaign_->member(id);
        const auto &cs = m.character.sheet();
        const auto name = (leader ? String::utf8("★ ") : String()) + String::utf8(cs.name.c_str());
        const auto gear = gear_lines(*campaign_, m);
        const auto health = armor_and_hit_points(campaign_->profile(id).armor_class,
                                                 m.vitals.hit_points, cs.hit_points);
        button->call(
            "show_member",
            portraits_->texture(m.character.appearance(), m.character.creation_data()),
            PackedStringArray({name, i18n::text(cs.race) + " / " + i18n::class_summary(cs)}),
            health, low_hit_points_color(m.vitals.hit_points, cs.hit_points), PackedStringArray(),
            leader, session_ && session_->turns_on_party(id));
        button->set_tooltip_text(String("\n").join(PackedStringArray(
            {name, i18n::class_summary(cs), i18n::text(cs.race), i18n::text(cs.gender),
             String().join(health), gear.weapons, gear.armor})));
        auto *arrow = &required_node<Button>(*button, "Advance");
        arrow->call("place_in_member_row", name);
        arrow->set_visible(embedded_party_ && campaign_->can_advance(id) && session_->can_leave());
        if (slot == selected && shown_selected_slot_ != slot)
        {
            shown_selected_slot_ = slot;
            required_node<Node>(*this, "Layout").call("show_party_member", button);
        }
    }
}

void RolfTourView::forget_portraits()
{
    portraits_.reset();
}

// A complete NPC or shopkeeper portrait from the new art's portraits/ folder;
// null without the new art, or, logged once, when the file cannot be loaded:
// the view then shows the original's picture.
Ref<Texture2D> RolfTourView::npc_portrait(std::string_view file)
{
    const auto folder = new_art_path("portraits");
    if (folder.is_empty())
    {
        return {};
    }
    const auto path = folder.path_join(String::utf8(std::string(file).c_str()));
    const std::string key = path.utf8().get_data();
    auto found = npc_portraits_.find(key);
    if (found == npc_portraits_.end())
    {
        const auto image = Image::load_from_file(path);
        if (image.is_null())
        {
            log_unloadable_art("Cannot load portrait " + key);
        }
        found = npc_portraits_
                    .emplace(key, image.is_null()
                                      ? Ref<Texture2D>()
                                      : Ref<Texture2D>(ImageTexture::create_from_image(image)))
                    .first;
    }
    return found->second;
}

Dictionary RolfTourView::tour_visuals()
{
    Dictionary data;
    if (!session_ || wall_view_.is_null() || (dax() && session_->snapshot().visited.none()))
    {
        return data;
    }
    const auto &state = session_->snapshot();
    data["wall"] = wall_view_;
    // The image itself: a headless check cannot read a texture's pixels back.
    data["wall_image"] = wall_image_;
    // A design has no portraits or sprites (docs/games/ua.md).
    const auto *campaign = dax();
    if (campaign && fight_waits_on_picture_ && !monster_frames_.empty())
    {
        data["picture"] = monster_frames_[current_monster_frame()];
    }
    else if (campaign)
    {
        const auto portrait_file =
            opengold::speaking_npc_portrait(state, campaign->shops(), campaign->temples());
        const auto portrait =
            portrait_file.empty() ? Ref<Texture2D>() : npc_portrait(portrait_file);
        if (portrait.is_valid())
        {
            data["picture"] = portrait;
        }
        else if (!monster_frames_.empty())
        {
            data["picture"] = monster_frames_[current_monster_frame()];
        }
        else if (state.sprite_frame >= 0 && sprites_[state.sprite_frame].is_valid())
        {
            const auto &source = campaign->sprites()[state.sprite_frame];
            data["sprite"] = sprites_[state.sprite_frame];
            data["sprite_size"] = Vector2(source.width, source.height);
        }
    }
    if (on_solar_map())
    {
        solar_visuals(data);
        return data;
    }
    const auto *map = session_->area_map();
    if (!map)
    {
        return data;
    }
    Array cells;
    cells.resize(map->cells.size());
    for (unsigned y = 0; y < map->height; ++y)
    {
        for (unsigned x = 0; x < map->width; ++x)
        {
            const unsigned index = y * map->width + x;
            if (!no_fog_ && !state.seen.test(index))
            {
                cells[index] = -1;
                continue;
            }
            const auto &cell = map->at(x, y);
            int flags = state.visited.test(index) ? 1 : 0;
            for (unsigned side = 0; side < 4; ++side)
            {
                if (cell.walls[side])
                {
                    flags |= 1 << (1 + side);
                }
                if (cell.doors[side])
                {
                    flags |= 1 << (5 + side);
                }
            }
            cells[index] = flags;
        }
    }
    data["cells"] = cells;
    data["columns"] = map->width;
    data["rows"] = map->height;
    const auto origin = session_->map_origin();
    data["pose"] = Vector2i(static_cast<int>(state.pose.x - origin.x),
                            static_cast<int>(state.pose.y - origin.y));
    data["facing"] = static_cast<int>(index(state.pose.facing));
    return data;
}

void RolfTourView::capture_frame(const String &name)
{
    if (!capture_)
    {
        return;
    }
    const auto directory = ProjectSettings::get_singleton()->globalize_path("user://checks");
    std::filesystem::create_directories(presentation::path_from_godot(directory));
    const auto path = directory.path_join(name + String(".png"));
    const auto image = get_viewport()->get_texture()->get_image();
    if (checking_ && !town_check_ && session_ && image.is_valid())
    {
        // The tour check waits a frame at each dialogue pause before capture.
        // Check actual framebuffer pixels, including hidden cells, rather than
        // merely checking the flag that the draw code was supposed to honor.
        const auto &state = session_->snapshot();
        // Named paths, not temporaries: GCC 16 takes the node reference for one
        // into the temporary NodePath.
        const NodePath map_path{"%MapBounds"};
        const auto &map = required_node<Control>(*this, map_path);
        const auto *area = session_->area_map();
        const unsigned columns = area ? area->width : opengold::por::GeoMap::default_width,
                       rows = area ? area->height : opengold::por::GeoMap::default_height;
        const double cell = std::min(map.get_size().x / columns, map.get_size().y / rows);
        for (unsigned y = 0; y < rows; ++y)
        {
            for (unsigned x = 0; x < columns; ++x)
            {
                if (x == state.pose.x && y == state.pose.y)
                {
                    continue; // Party arrow covers its cell.
                }
                const auto center = map.get_global_transform_with_canvas().xform(
                    Vector2(static_cast<godot::real_t>((x + .5) * cell),
                            static_cast<godot::real_t>((y + .5) * cell)));
                const auto expected =
                    get_theme_color(!no_fog_ && !state.seen.test(y * columns + x) ? "map_black"
                                    : state.visited.test(y * columns + x)         ? "map_visited"
                                                                                  : "map_seen",
                                    "OpenGoldPalette");
                if (!image
                         ->get_pixel(static_cast<int32_t>(center.x), static_cast<int32_t>(center.y))
                         .is_equal_approx(expected))
                {
                    throw std::runtime_error("Overhead fog pixel mismatch at " + std::to_string(x) +
                                             "," + std::to_string(y));
                }
            }
        }
    }
    if (image.is_null() || image->save_png(path) != OK)
    {
        throw std::runtime_error("Failed to capture tour scene");
    }
    UtilityFunctions::print("Screenshot: ", path);
}
