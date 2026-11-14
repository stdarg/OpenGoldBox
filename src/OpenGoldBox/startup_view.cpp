#include "startup_view.h"
#include "localization.h"
#include "application_settings.h"
#include "guarded_handlers.h"
#include "godot_nodes.h"
#include "game_menu_view.h"
#include "save_slots.h"
#include "godot_images.h"
#include "godot_path.h"
#include "godot_sound_output.h"
#include "opengold/music_render.h"
#include "opengold/diagnostic_log.h"
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/file_dialog.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/translation_server.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <algorithm>
#include <array>
#include <optional>
#include <stdexcept>
#include <utility>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;
using presentation::required_node;

void StartupView::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("open_character_creation"),
                         &StartupView::open_character_creation);
}

void StartupView::_ready()
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  open_startup();
                              });
}

void StartupView::open_startup()
{
    i18n::initialize();
    show_backdrop();
    choose_language();
}

// The picture behind OpenGoldBox's title and every startup dialog.
void StartupView::show_backdrop()
{
    auto *image = &required_node<TextureRect>(*this, "ImageFit/Image"); // scene-owned
    Ref<Texture2D> texture =
        ResourceLoader::get_singleton()->load("res://bin/splashes/OpenGoldBoxSplashBackground.png");
    if (texture.is_valid())
    {
        image->set_texture(texture);
    }
}

namespace
{

struct MenuGame
{
    opengold::GoldBoxGame game;
    const char *title;
};

// The games the startup menu offers, in release order (docs/GAMES.md).
constexpr std::array<MenuGame, 7> playable_games{
    {{opengold::GoldBoxGame::pool_of_radiance, N_("Pool of Radiance")},
     {opengold::GoldBoxGame::curse_of_the_azure_bonds, N_("Curse of the Azure Bonds")},
     {opengold::GoldBoxGame::secret_of_the_silver_blades, N_("Secret of the Silver Blades")},
     {opengold::GoldBoxGame::gateway_to_the_savage_frontier, N_("Gateway to the Savage Frontier")},
     {opengold::GoldBoxGame::pools_of_darkness, N_("Pools of Darkness")},
     {opengold::GoldBoxGame::treasures_of_the_savage_frontier,
      N_("Treasures of the Savage Frontier")},
     {opengold::GoldBoxGame::unlimited_adventures, N_("Unlimited Adventures")}}};

const char *menu_title(opengold::GoldBoxGame game)
{
    for (const auto &entry : playable_games)
    {
        if (entry.game == game)
        {
            return entry.title;
        }
    }
    return "";
}

// OpenGoldBox's title stays this long unless a key or click passes it (the
// user's design, 2026-11-14).
constexpr double opening_title_seconds = 2.0;
// Pool of Radiance's new title stays as long as its original's first picture.
constexpr double new_title_seconds = 5.0;

// The newest save of any game, and its game: what the main menu's Continue
// loads.
std::optional<std::pair<opengold::GoldBoxGame, std::filesystem::path>> newest_save()
{
    std::optional<std::pair<opengold::GoldBoxGame, std::filesystem::path>> newest;
    std::filesystem::file_time_type newest_time{};
    for (const auto &entry : playable_games)
    {
        const auto save = newest_campaign_save(entry.game);
        if (!save)
        {
            continue;
        }
        const auto written = std::filesystem::last_write_time(*save);
        if (!newest || written > newest_time)
        {
            newest.emplace(entry.game, *save);
            newest_time = written;
        }
    }
    return newest;
}

// --game=POOLRAD (or CURSE, SECRET, GATEWAY, DARKNESS, TREASURE or UA): the game a check plays,
// without the titles and the menus.
std::optional<opengold::GoldBoxGame> game_from_command_line()
{
    auto *os = OS::get_singleton(); // borrowed engine singleton
    for (const auto &arguments : {os->get_cmdline_args(), os->get_cmdline_user_args()})
    {
        for (int i = 0; i < arguments.size(); ++i)
        {
            if (!arguments[i].begins_with("--game="))
            {
                continue;
            }
            const auto folder = arguments[i].trim_prefix("--game=");
            for (const auto &[game, title] : playable_games)
            {
                if (folder == String(std::string(opengold::game_folder(game)).c_str()))
                {
                    return game;
                }
            }
        }
    }
    return std::nullopt;
}

// The first of a menu's buttons that can be pressed, to take the focus.
void focus_first_enabled(Node &buttons)
{
    for (int i = 0; i < buttons.get_child_count(); ++i)
    {
        auto *button = Object::cast_to<Button>(buttons.get_child(i));
        if (button && button->is_visible() && !button->is_disabled())
        {
            button->grab_focus();
            return;
        }
    }
}

} // namespace

void StartupView::language_chosen()
{
    connect_path_dialogs();
    if (const auto game = game_from_command_line())
    {
        game_from_flag_ = true;
        settings::choose_game(*game);
        choose_folders(*game, AfterFolders::new_party);
        return;
    }
    show_opening_title();
}

void StartupView::show_opening_title()
{
    stage_ = Stage::opening_title;
    title_seconds_left_ = opening_title_seconds;
    show_lettering("OpenGoldBoxEngineLettering");
}

// Lettering fades in over the shared picture, in the player's language.
void StartupView::show_lettering(const char *name)
{
    const String path =
        String("res://bin/splashes/") + name + (i18n::language() == "es" ? ".es.png" : ".png");
    Ref<Texture2D> lettering = ResourceLoader::get_singleton()->load(path);
    auto *text = &required_node<TextureRect>(*this, "ImageFit/Image/Text"); // scene-owned
    if (lettering.is_null())
    {
        UtilityFunctions::push_error(String("Missing title lettering: ") + path);
        text->set_texture({});
        return;
    }
    text->set_texture(lettering);
    text->show();
    text->call("begin");
}

void StartupView::connect_escape(Window &dialog, void (StartupView::*back)())
{
    dialog.connect("window_input", presentation::guarded(this, &StartupView::dialog_input)
                                       .bind(presentation::guarded(this, back)));
    dialog.connect("close_requested", presentation::guarded(this, back));
}

void StartupView::dialog_input(const Ref<InputEvent> &event, const Callable &back)
{
    const Ref<InputEventKey> key = event;
    if (key.is_valid() && key->is_pressed() && !key->is_echo() && key->get_keycode() == KEY_ESCAPE)
    {
        get_viewport()->set_input_as_handled();
        back.call_deferred();
    }
}

// After the title: Continue, Pick game, Options and Exit to desktop (the
// user's design, 2026-11-14). Continue loads the newest save of any game.
void StartupView::show_main_menu()
{
    stage_ = Stage::menus;
    auto *menu = &required_node<Window>(*this, "MainMenu"); // scene-owned
    if (!main_menu_ready_)
    {
        main_menu_ready_ = true;
        i18n::prepare_ui(*menu);
        menu->connect("close_requested",
                      presentation::guarded(this, &StartupView::exit_to_desktop));
        required_node<Button>(*menu, "Buttons/Continue")
            .connect("pressed", presentation::guarded(this, &StartupView::continue_newest));
        required_node<Button>(*menu, "Buttons/PickGame")
            .connect("pressed", presentation::guarded(this, &StartupView::show_game_picker));
        required_node<Button>(*menu, "Buttons/Options")
            .connect("pressed", presentation::guarded(this, &StartupView::open_options_from_main));
        required_node<Button>(*menu, "Buttons/Exit")
            .connect("pressed", presentation::guarded(this, &StartupView::exit_to_desktop));
    }
    required_node<Window>(*this, "GamePicker").hide();
    required_node<Window>(*this, "GameMenu").hide();
    required_node<Button>(*menu, "Buttons/Continue").set_disabled(!newest_save());
    menu->popup_centered();
    focus_first_enabled(required_node<Node>(*menu, "Buttons"));
}

void StartupView::continue_newest()
{
    const auto newest = newest_save();
    if (!newest)
    {
        return;
    }
    required_node<Window>(*this, "MainMenu").hide();
    settings::choose_game(newest->first);
    save_to_load_ = newest->second;
    choose_folders(newest->first, AfterFolders::saved_game);
}

// A button for each game, and Back to the main menu.
void StartupView::show_game_picker()
{
    auto *picker = &required_node<Window>(*this, "GamePicker"); // scene-owned
    if (!game_picker_ready_)
    {
        game_picker_ready_ = true;
        i18n::prepare_ui(*picker);
        connect_escape(*picker, &StartupView::show_main_menu);
        required_node<Button>(*picker, "Buttons/Back")
            .connect("pressed", presentation::guarded(this, &StartupView::show_main_menu));
        auto *games = &required_node<VBoxContainer>(*picker, "Buttons/Games");
        for (const auto &[game, title] : playable_games)
        {
            auto *button = memnew(Button); // owned by the picker once added
            button->set_name(std::string(opengold::game_folder(game)).c_str());
            button->set_text(i18n::text(title));
            button->set_custom_minimum_size({0, 52});
            button->connect("pressed", presentation::guarded(this, &StartupView::game_chosen)
                                           .bind(static_cast<std::int64_t>(game)));
            games->add_child(button);
        }
    }
    stage_ = Stage::menus;
    required_node<Window>(*this, "MainMenu").hide();
    required_node<Window>(*this, "GameMenu").hide();
    picker->popup_centered();
    focus_first_enabled(required_node<Node>(*picker, "Buttons/Games"));
}

void StartupView::game_chosen(std::int64_t chosen)
{
    const auto game = static_cast<opengold::GoldBoxGame>(chosen);
    settings::choose_game(game);
    required_node<Window>(*this, "GamePicker").hide();
    choose_folders(game, AfterFolders::game_title);
}

// The game's folder, and Pool of Radiance's: the party's portraits and
// combat art still come from its files (docs/GAMES.md). Unlimited Adventures'
// folder is the kit's, and then its design is asked.
void StartupView::choose_folders(opengold::GoldBoxGame game, AfterFolders after)
{
    after_folders_ = after;
    folders_needed_ = {game};
    // A design's save names its design, so only a new game asks for one.
    design_needed_ =
        game == opengold::GoldBoxGame::unlimited_adventures && after != AfterFolders::saved_game;
    if (game != opengold::GoldBoxGame::pool_of_radiance &&
        !settings::validate_game_path(settings::game_path()).usable)
    {
        folders_needed_.push_back(opengold::GoldBoxGame::pool_of_radiance);
    }
    next_folder();
}

void StartupView::folders_ready()
{
    switch (after_folders_)
    {
    case AfterFolders::game_title:
        show_game_title();
        return;
    case AfterFolders::saved_game:
        start_game({settings::GameStart::Kind::saved_game, save_to_load_});
        return;
    case AfterFolders::new_party:
        new_game_kind_ = settings::GameStart::Kind::new_party;
        ask_rules();
        return;
    }
}

// Closing a folder's question goes back to where it was asked from.
void StartupView::cancel_folders()
{
    required_node<Window>(*this, "PathDialog").hide();
    required_node<Window>(*this, "ChecksumWarning").hide();
    save_pending_path_ = false;
    folders_needed_.clear();
    design_needed_ = false;
    if (game_from_flag_)
    {
        exit_to_desktop();
        return;
    }
    if (after_folders_ == AfterFolders::saved_game)
    {
        show_main_menu();
        return;
    }
    show_game_picker();
}

// The chosen game's menu after its title: Continue (its newest save), Load
// game, Options, Play with default party, Create new party, Main menu and Exit
// to desktop (the user's design, 2026-11-14).
void StartupView::show_game_menu()
{
    stage_ = Stage::menus;
    auto *menu = &required_node<Window>(*this, "GameMenu"); // scene-owned
    if (!game_menu_ready_)
    {
        game_menu_ready_ = true;
        i18n::prepare_ui(*menu);
        connect_escape(*menu, &StartupView::show_game_picker);
        for (const auto &[name, method] :
             {std::pair{"Buttons/Continue", &StartupView::continue_game},
              std::pair{"Buttons/Load", &StartupView::load_game},
              std::pair{"Buttons/Options", &StartupView::open_options_from_game},
              std::pair{"Buttons/DefaultParty", &StartupView::play_default_party},
              std::pair{"Buttons/NewParty", &StartupView::create_new_party},
              std::pair{"Buttons/MainMenu", &StartupView::show_main_menu},
              std::pair{"Buttons/Exit", &StartupView::exit_to_desktop}})
        {
            required_node<Button>(*menu, name)
                .connect("pressed", presentation::guarded(this, method));
        }
    }
    const auto game = settings::chosen_game();
    menu->set_title(i18n::text(menu_title(game)));
    const bool saved = newest_campaign_save(game).has_value();
    required_node<Button>(*menu, "Buttons/Continue").set_disabled(!saved);
    required_node<Button>(*menu, "Buttons/Load").set_disabled(!saved);
    // No default party is made for a design yet (docs/games/ua.md).
    required_node<Button>(*menu, "Buttons/DefaultParty")
        .set_disabled(game == opengold::GoldBoxGame::unlimited_adventures);
    menu->popup_centered();
    focus_first_enabled(required_node<Node>(*menu, "Buttons"));
}

void StartupView::continue_game()
{
    if (const auto save = newest_campaign_save(settings::chosen_game()))
    {
        start_game({settings::GameStart::Kind::saved_game, *save});
    }
}

// The game's saves, as its save and load screens list them.
void StartupView::load_game()
{
    auto *slots = Object::cast_to<SaveSlots>(get_node_or_null("SaveSlots"));
    if (!slots)
    {
        auto owned = presentation::make_node<SaveSlots>();
        owned->set_name("SaveSlots");
        // A window shows when added; this one waits for open().
        owned->hide();
        owned->connect_host(
            [](const std::filesystem::path &)
            {
                throw std::runtime_error("Startup saves no game");
            },
            [this](const std::filesystem::path &save)
            {
                start_game({settings::GameStart::Kind::saved_game, save});
            });
        owned->connect("visibility_changed",
                       presentation::guarded(this, &StartupView::saves_closed));
        slots = owned.get();
        presentation::attach_child(*this, std::move(owned));
    }
    required_node<Window>(*this, "GameMenu").hide();
    slots->open(false);
}

void StartupView::saves_closed()
{
    const auto &slots = required_node<SaveSlots>(*this, "SaveSlots");
    if (!slots.is_visible() && !starting_)
    {
        // Once the closing window is no longer the exclusive one.
        presentation::guarded(this, &StartupView::show_game_menu).call_deferred();
    }
}

void StartupView::play_default_party()
{
    required_node<Window>(*this, "GameMenu").hide();
    new_game_kind_ = settings::GameStart::Kind::default_party;
    ask_rules();
}

void StartupView::create_new_party()
{
    required_node<Window>(*this, "GameMenu").hide();
    new_game_kind_ = settings::GameStart::Kind::new_party;
    ask_rules();
}

void StartupView::open_options_from_main()
{
    options_from_game_ = false;
    required_node<Window>(*this, "MainMenu").hide();
    open_options();
}

void StartupView::open_options_from_game()
{
    options_from_game_ = true;
    required_node<Window>(*this, "GameMenu").hide();
    open_options();
}

void StartupView::open_options()
{
    auto owned = presentation::instantiate_scene("res://scenes/game_menu.tscn");
    auto *options = Object::cast_to<GameMenuView>(owned.get());
    if (!options)
    {
        throw std::runtime_error("Invalid game menu scene");
    }
    options->set_name("Options");
    options->connect("closed", presentation::guarded(this, &StartupView::options_closed));
    presentation::attach_child(*this, std::move(owned));
    options->open_options_alone();
}

void StartupView::options_closed()
{
    if (auto *options = get_node_or_null("Options"))
    {
        options->queue_free();
    }
    if (options_from_game_)
    {
        show_game_menu();
        return;
    }
    show_main_menu();
}

void StartupView::exit_to_desktop()
{
    // The same graceful shutdown as the window's close button.
    get_tree()->get_root()->emit_signal("close_requested");
}

// Asks for the next game's folder when it is not set or no longer holds the
// game; startup goes on once every folder is known.
void StartupView::next_folder()
{
    if (asking_design())
    {
        ask_design();
        return;
    }
    if (folders_needed_.empty())
    {
        folders_ready();
        return;
    }
    const auto game = folders_needed_.front();
    auto *dialog = &required_node<Window>(*this, "PathDialog");
    const auto heading =
        game == opengold::GoldBoxGame::pool_of_radiance
            ? i18n::text("Pool of Radiance game files")
            : i18n::format(N_("{game} game files"), {{"game", i18n::text(menu_title(game))}});
    dialog->set_title(heading);
    required_node<Label>(*dialog, "Title").set_text(heading);
    required_node<Label>(*dialog, "Instructions")
        .set_text(i18n::text(
            N_("Choose the folder containing START.EXE and the original DAX game files.")));
    pending_path_ = settings::game_folder(game);
    if (game == opengold::GoldBoxGame::pool_of_radiance)
    {
        const auto saved = settings::saved_game_path();
        if (pending_path_.is_empty())
        {
            pending_path_ = OS::get_singleton()->get_environment("OPENGOLD_GAME_DIR");
        }
        if (settings::flag("--reset-game-path") || saved.is_empty() ||
            !settings::validate_game_path(saved).usable)
        {
            show_path();
            return;
        }
    }
    else if (!settings::validate_game_folder(pending_path_, game).usable)
    {
        show_path();
        return;
    }
    check_path();
}

// The design is asked each time Unlimited Adventures is chosen, the last one
// offered, as the player may have several; a check's --game=UA plays the
// saved one.
void StartupView::ask_design()
{
    auto *dialog = &required_node<Window>(*this, "PathDialog");
    const auto heading = i18n::text(N_("Unlimited Adventures design"));
    dialog->set_title(heading);
    required_node<Label>(*dialog, "Title").set_text(heading);
    required_node<Label>(*dialog, "Instructions")
        .set_text(i18n::text(N_(
            "Choose the folder of the design to play, which the construction kit names NAME.DSN.")));
    pending_path_ = settings::design_folder();
    if (game_from_flag_ && settings::validate_design_folder(pending_path_).usable)
    {
        check_path();
        return;
    }
    show_path();
}

void StartupView::connect_path_dialogs()
{
    // Translate these dialogs only after language selection has been confirmed.
    i18n::prepare_ui(required_node<Window>(*this, "PathDialog"));
    i18n::prepare_ui(required_node<Window>(*this, "ChecksumWarning"));
    auto *dialog = &required_node<Window>(*this, "PathDialog"); // scene-owned
    connect_escape(*dialog, &StartupView::cancel_folders);
    required_node<Button>(*dialog, "Cancel")
        .connect("pressed", presentation::guarded(this, &StartupView::cancel_folders));
    required_node<Button>(*dialog, "Continue")
        .connect("pressed", presentation::guarded(this, &StartupView::accept_path));
    required_node<Button>(*dialog, "Browse")
        .connect("pressed", presentation::guarded(this, &StartupView::browse_path));
    required_node<LineEdit>(*dialog, "Path")
        .connect("text_submitted", presentation::guarded(this, &StartupView::submitted_path));
    required_node<LineEdit>(*dialog, "Path")
        .connect("text_changed", presentation::guarded(this, &StartupView::path_edited));
    required_node<FileDialog>(*dialog, "BrowseDialog")
        .connect("dir_selected", presentation::guarded(this, &StartupView::picked_path));
    auto *warning = &required_node<Window>(*this, "ChecksumWarning");
    connect_escape(*warning, &StartupView::cancel_folders);
    required_node<Button>(*warning, "Quit")
        .connect("pressed", presentation::guarded(this, &StartupView::close_language));
    required_node<Button>(*warning, "Continue")
        .connect("pressed", presentation::guarded(this, &StartupView::continue_path));
}

void StartupView::choose_language()
{
    if (settings::flag("--reset-lang") || !settings::valid_language(settings::saved_language()))
    {
        choosing_language_ = true;
        auto *dialog = &required_node<Window>(*this, "LanguageDialog"); // scene-owned
        // Native language names remain stable; other text previews the selected locale.
        dialog->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
        dialog->connect("close_requested",
                        presentation::guarded(this, &StartupView::close_language));
        auto *choices = &required_node<ItemList>(*dialog, "Choices");
        choices->add_item("English");
        choices->set_item_metadata(0, "en");
        choices->add_item(String::utf8("Español"));
        choices->set_item_metadata(1, "es");
        choices->select(i18n::language() == "es" ? 1 : 0);
        choices->connect("item_selected",
                         presentation::guarded(this, &StartupView::preview_language));
        preview_language(choices->get_selected_items()[0]);
        choices->connect("item_activated",
                         presentation::guarded(this, &StartupView::activate_language));
        required_node<Button>(*dialog, "Continue")
            .connect("pressed", presentation::guarded(this, &StartupView::accept_language));
        dialog->popup_centered();
        choices->grab_focus();
        required_node<Button>(*dialog, "Cancel")
            .connect("pressed", presentation::guarded(this, &StartupView::close_language));
        return;
    }
    language_chosen();
}

void StartupView::show_path(const String &message)
{
    auto *dialog = &required_node<Window>(*this, "PathDialog");
    required_node<LineEdit>(*dialog, "Path").set_text(pending_path_);
    required_node<Label>(*dialog, "Status").set_text(message);
    dialog->popup_centered();
    required_node<LineEdit>(*dialog, "Path").grab_focus();
}

void StartupView::browse_path()
{
    auto *browser = &required_node<FileDialog>(*this, "PathDialog/BrowseDialog");
    browser->set_current_dir(required_node<LineEdit>(*this, "PathDialog/Path").get_text());
    browser->popup_centered_ratio(.7f);
}

void StartupView::picked_path(const String &directory)
{
    required_node<LineEdit>(*this, "PathDialog/Path").set_text(directory);
    // Programmatic text changes do not emit LineEdit's text_changed signal.
    path_edited(directory);
    required_node<Button>(*this, "PathDialog/Continue").grab_focus();
}

void StartupView::path_edited(const String &)
{
    required_node<Label>(*this, "PathDialog/Status").set_text(String());
}

void StartupView::submitted_path(const String &)
{
    accept_path();
}

void StartupView::accept_path()
{
    pending_path_ = required_node<LineEdit>(*this, "PathDialog/Path").get_text().strip_edges();
    save_pending_path_ = true;
    check_path();
}

void StartupView::check_path()
{
    const auto result =
        asking_design() ? settings::validate_design_folder(pending_path_)
                        : settings::validate_game_folder(pending_path_, folders_needed_.front());
    if (!result.usable)
    {
        String message = result.error.is_empty() ? i18n::text("Missing or unreadable game files:")
                                                 : i18n::text(result.error.utf8().get_data());
        for (int i = 0; i < std::min(static_cast<int>(result.missing.size()), 5); ++i)
        {
            message += " " + result.missing[i];
        }
        if (result.missing.size() > 5)
        {
            message += " ...";
        }
        show_path(message);
        return;
    }
    required_node<Window>(*this, "PathDialog").hide();
    if (!result.different.is_empty())
    {
        auto *warning = &required_node<Window>(*this, "ChecksumWarning");
        required_node<RichTextLabel>(*warning, "Files")
            .set_text(pending_path_ + "\n\n" + String("\n").join(result.different));
        warning->popup_centered();
        required_node<Button>(*warning, "Quit").grab_focus();
        return;
    }
    continue_path();
}

void StartupView::continue_path()
{
    required_node<Window>(*this, "ChecksumWarning").hide();
    const bool design = asking_design();
    if (save_pending_path_ &&
        !(design ? settings::save_design_folder(pending_path_)
                 : settings::save_game_folder(folders_needed_.front(), pending_path_)))
    {
        show_path(i18n::text(
            "Cannot save settings. Check that settings.cfg in the game's user data folder can be written and is valid."));
        return;
    }
    save_pending_path_ = false;
    get_viewport()->set_input_as_handled();
    if (design)
    {
        design_needed_ = false;
    }
    else
    {
        folders_needed_.erase(folders_needed_.begin());
    }
    next_folder();
}

void StartupView::accept_language()
{
    if (!choosing_language_)
    {
        return;
    }
    auto *dialog = &required_node<Window>(*this, "LanguageDialog");
    auto *choices = &required_node<ItemList>(*dialog, "Choices");
    const auto selected = choices->get_selected_items();
    if (selected.is_empty())
    {
        return;
    }
    const String locale = choices->get_item_metadata(selected[0]);
    if (!i18n::select_language(locale))
    {
        language_save_failed_ = true;
        preview_language(selected[0]);
        return;
    }
    dialog->hide();
    choosing_language_ = false;
    // Do not let the Enter press used here also pass the title.
    dialog->set_input_as_handled();
    language_chosen();
}

void StartupView::activate_language(std::int64_t)
{
    accept_language();
}

void StartupView::preview_language(std::int64_t index)
{
    auto *dialog = &required_node<Window>(*this, "LanguageDialog");
    auto *choices = &required_node<ItemList>(*dialog, "Choices");
    if (index < 0 || index >= choices->get_item_count())
    {
        return;
    }

    // Preview does not change the active or saved language until
    // confirmation: the guard puts the locale back however this ends
    // (Effective C++ Item 13).
    class RestoreLocale
    {
      public:
        explicit RestoreLocale(TranslationServer &translations)
            : translations_(translations), previous_(translations.get_locale())
        {
        }

        RestoreLocale(const RestoreLocale &) = delete;
        RestoreLocale &operator=(const RestoreLocale &) = delete;

        ~RestoreLocale()
        {
            translations_.set_locale(previous_);
        }

      private:
        TranslationServer &translations_;
        String previous_;
    };

    auto *translations = TranslationServer::get_singleton();
    const RestoreLocale restore(*translations);
    translations->set_locale(choices->get_item_metadata(static_cast<std::int32_t>(index)));
    dialog->set_title(i18n::text("Language"));
    required_node<Label>(*dialog, "Title").set_text(i18n::text("Choose language"));
    required_node<Button>(*dialog, "Continue").set_text(i18n::text("Continue"));
    required_node<Button>(*dialog, "Cancel").set_text(i18n::text("Cancel"));
    required_node<Label>(*dialog, "Status")
        .set_text(language_save_failed_ ? i18n::text("Cannot save language.") : String());
}

void StartupView::close_language()
{
    // The startup dialog's X follows the same global graceful shutdown path.
    get_tree()->get_root()->emit_signal("close_requested");
}

void StartupView::_input(const Ref<InputEvent> &event)
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  respond_to_input(event);
                              });
}

namespace
{
// Application shortcuts must not pass a title.
bool application_shortcut(const Ref<InputEventKey> &key)
{
    return key.is_valid() && key->is_ctrl_pressed() &&
           (key->get_keycode() == KEY_X || key->get_keycode() == KEY_S);
}
} // namespace

// On a title any key or click passes it, and Escape goes back (the user's
// design, 2026-11-14).
void StartupView::respond_to_input(const Ref<InputEvent> &event)
{
    if (stage_ == Stage::menus)
    {
        return;
    }
    const Ref<InputEventKey> key = event;
    if (application_shortcut(key))
    {
        return;
    }
    const Ref<InputEventMouseButton> click = event;
    const bool pressed_key = key.is_valid() && key->is_pressed() && !key->is_echo();
    if (!pressed_key && !(click.is_valid() && click->is_pressed()))
    {
        return;
    }
    get_viewport()->set_input_as_handled();
    if (pressed_key && key->get_keycode() == KEY_ESCAPE)
    {
        back_from_title();
        return;
    }
    advance_title();
}

void StartupView::advance_title()
{
    switch (stage_)
    {
    case Stage::opening_title:
        required_node<TextureRect>(*this, "ImageFit/Image/Text").call("cancel");
        required_node<TextureRect>(*this, "ImageFit/Image/Text").hide();
        show_main_menu();
        return;
    case Stage::new_title:
        end_game_title();
        return;
    case Stage::original_title:
        if (const auto after_key = game_title_->screens.at(title_screen_).after_key)
        {
            go_to_title_screen(*after_key);
        }
        return;
    case Stage::menus:
        return;
    }
}

// OpenGoldBox's title has nothing before it; a game's goes back to the games.
void StartupView::back_from_title()
{
    if (stage_ == Stage::opening_title)
    {
        advance_title();
        return;
    }
    stop_game_title();
    show_game_picker();
}

void StartupView::_process(double delta)
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  if (stage_ == Stage::menus)
                                  {
                                      return;
                                  }
                                  title_seconds_left_ -= delta;
                                  if (title_seconds_left_ > 0)
                                  {
                                      return;
                                  }
                                  if (stage_ == Stage::original_title)
                                  {
                                      next_title_screen();
                                      return;
                                  }
                                  advance_title();
                              });
}

// Pool of Radiance keeps its new title (the user's answer, 2026-11-14); the
// other games show their own pictures as their originals do when they start.
void StartupView::show_game_title()
{
    const auto game = settings::chosen_game();
    // The sounds of a game chosen before, whose menu was left.
    title_sound_.reset();
    if (game == opengold::GoldBoxGame::pool_of_radiance)
    {
        stage_ = Stage::new_title;
        title_seconds_left_ = new_title_seconds;
        show_lettering("OpenGoldBoxGameLettering");
        return;
    }
    try
    {
        game_title_ = opengold::load_title_screens(
            presentation::path_from_godot(settings::game_folder(game)), game);
    }
    catch (const std::exception &failure)
    {
        // The folder passed its check; a title it lacks is only skipped.
        opengold::log_diagnostic(opengold::LogPriority::warning, "startup", failure.what());
    }
    if (!game_title_ || game_title_->screens.empty())
    {
        end_game_title();
        return;
    }
    stage_ = Stage::original_title;
    title_screen_ = 0;
    show_title_screen();
}

// The later games play a piece of their music over the title, from the screen
// the title names until the title ends.
void StartupView::play_title_music(opengold::GoldBoxGame game)
{
    const auto piece = opengold::title_music_piece(game);
    if (!piece)
    {
        return;
    }
    try
    {
        start_title_music(
            opengold::por::SoundBank::load_music(
                presentation::path_from_godot(settings::game_folder(game)), game, *piece),
            *piece);
    }
    catch (const std::runtime_error &failure)
    {
        // A game whose music cannot be read shows its title silent.
        opengold::log_diagnostic(opengold::LogPriority::warning, "startup", failure.what());
    }
}

// Pools of Darkness' opening film plays its own song until the title music.
void StartupView::play_film_song(const opengold::TitleSong &song)
{
    try
    {
        constexpr unsigned song_clip = 1;
        start_title_music(
            opengold::por::SoundBank::load_song(song.file, settings::chosen_game(), song.number),
            song_clip);
    }
    catch (const std::runtime_error &failure)
    {
        opengold::log_diagnostic(opengold::LogPriority::warning, "startup", failure.what());
    }
}

// Music in place of the title's sounds so far, at the player's music level
// (Options).
void StartupView::start_title_music(opengold::por::SoundBank music, unsigned clip)
{
    title_sound_ = std::make_unique<opengold::por::SoundPlayer>(
        std::move(music),
        std::make_unique<GodotSoundOutput>(required_node<AudioStreamPlayer>(*this, "TitleSound")));
    title_sound_->set_volume(settings::sound_volume(settings::SoundKind::music));
    title_sound_->play(clip);
}

void StartupView::show_title_screen()
{
    const auto &screen = game_title_->screens.at(title_screen_);
    required_node<TextureRect>(*this, "GameTitle/Picture")
        .set_texture(presentation::image_texture(screen.screen));
    required_node<Control>(*this, "GameTitle").show();
    if (title_screen_ == 0 && game_title_->film_song)
    {
        play_film_song(*game_title_->film_song);
    }
    if (title_screen_ == game_title_->music_screen)
    {
        play_title_music(settings::chosen_game());
    }
    if (screen.sound)
    {
        play_title_sound(*screen.sound);
    }
    title_seconds_left_ = screen.seconds;
}

void StartupView::play_title_sound(unsigned sound)
{
    if (!title_sound_)
    {
        const auto game = settings::chosen_game();
        try
        {
            title_sound_ = std::make_unique<opengold::por::SoundPlayer>(
                opengold::por::SoundBank::load(
                    presentation::path_from_godot(settings::game_folder(game)), game),
                std::make_unique<GodotSoundOutput>(
                    required_node<AudioStreamPlayer>(*this, "TitleSound")));
        }
        catch (const std::runtime_error &failure)
        {
            // A game whose sounds cannot be read shows its title silent.
            opengold::log_diagnostic(opengold::LogPriority::warning, "startup", failure.what());
            return;
        }
        // The player's sound effects level (Options).
        title_sound_->set_volume(settings::sound_volume(settings::SoundKind::effects));
    }
    title_sound_->play(sound);
}

void StartupView::next_title_screen()
{
    go_to_title_screen(title_screen_ + 1);
}

// The title's screen at `screen`, or the end of the title past its last.
void StartupView::go_to_title_screen(std::size_t screen)
{
    title_screen_ = screen;
    if (title_screen_ >= game_title_->screens.size())
    {
        end_game_title();
        return;
    }
    show_title_screen();
}

void StartupView::stop_game_title()
{
    stage_ = Stage::menus;
    game_title_.reset();
    // The title's music, if any, stops with it.
    title_sound_.reset();
    required_node<Control>(*this, "GameTitle").hide();
    required_node<TextureRect>(*this, "ImageFit/Image/Text").call("cancel");
    required_node<TextureRect>(*this, "ImageFit/Image/Text").hide();
}

void StartupView::end_game_title()
{
    stop_game_title();
    show_game_menu();
}

namespace
{
// The rules the question offers, in its order, and what each is.
constexpr std::array<std::pair<const char *, const char *>, 2> rules_choices{
    {{N_("The original rules (Pool of Radiance)"),
      N_("Characters, fights and magic as the original game had them: AD&D 1st edition.")},
     {N_("SRD 5.2.1"),
      N_("The fifth edition's System Reference Document, with the original's adventures.")}}};
// The original rules' aging settings, in the question's order: the original's
// first, as it is the default.
constexpr std::array<std::pair<opengold::original::Aging, const char *>, 3> aging_choices{
    {{opengold::original::Aging::original,
      N_("As the original: the starting age changes the scores once.")},
     {opengold::original::Aging::off, N_("Off: age never changes the scores.")},
     {opengold::original::Aging::add,
      N_("AD&D: the scores change again as characters grow older.")}}};
constexpr std::array<const char *, 3> aging_names{N_("As the original"), N_("Off"), N_("AD&D")};
} // namespace

void StartupView::ask_rules()
{
    for (const auto &[flag, rules] :
         {std::pair{"--rules=srd", GameRules{}},
          std::pair{"--rules=original",
                    GameRules{GameRules::Set::original, opengold::original::default_aging}}})
    {
        if (settings::flag(flag))
        {
            settings::choose_rules(rules);
            begin_game();
            return;
        }
    }
    if (settings::flag("--combat-demo"))
    {
        begin_game();
        return;
    }
    auto *dialog = &required_node<Window>(*this, "RulesDialog"); // scene-owned
    auto &rules = required_node<OptionButton>(*dialog, "Rows/Rules");
    if (rules.get_item_count() == 0)
    {
        i18n::prepare_ui(*dialog);
        for (const auto &[name, note] : rules_choices)
        {
            rules.add_item(i18n::text(name));
        }
        rules.select(new_game_rules.set == GameRules::Set::original ? 0 : 1);
        rules.connect("item_selected", presentation::guarded(this, &StartupView::show_rules_notes));
        auto &aging = required_node<OptionButton>(*dialog, "Rows/Aging");
        for (const auto *name : aging_names)
        {
            aging.add_item(i18n::text(name));
        }
        aging.select(0);
        aging.connect("item_selected", presentation::guarded(this, &StartupView::show_rules_notes));
        required_node<Button>(*dialog, "Rows/Begin")
            .connect("pressed", presentation::guarded(this, &StartupView::begin_game));
        connect_escape(*dialog, &StartupView::cancel_new_game);
    }
    // A design plays by the kit's own rules: the SRD has none of its monsters
    // (docs/games/ua.md).
    const bool design = settings::chosen_game() == opengold::GoldBoxGame::unlimited_adventures;
    rules.set_item_disabled(1, design);
    if (design)
    {
        rules.select(0);
    }
    show_rules_notes(0);
    dialog->popup_centered();
    required_node<Button>(*dialog, "Rows/Begin").grab_focus();
}

void StartupView::show_rules_notes(std::int64_t)
{
    auto &dialog = required_node<Window>(*this, "RulesDialog");
    const auto rules = required_node<OptionButton>(dialog, "Rows/Rules").get_selected();
    const auto aging = required_node<OptionButton>(dialog, "Rows/Aging").get_selected();
    required_node<Label>(dialog, "Rows/RulesNote")
        .set_text(i18n::text(rules_choices.at(static_cast<std::size_t>(rules)).second));
    const bool original = rules == 0;
    for (const char *name : {"Rows/AgingLabel", "Rows/Aging", "Rows/AgingNote"})
    {
        required_node<Control>(dialog, name).set_visible(original);
    }
    required_node<Label>(dialog, "Rows/AgingNote")
        .set_text(i18n::text(aging_choices.at(static_cast<std::size_t>(aging)).second));
}

// Closing the question returns to the game's menu; a check's game, which has
// none, quits.
void StartupView::cancel_new_game()
{
    required_node<Window>(*this, "RulesDialog").hide();
    if (game_from_flag_)
    {
        exit_to_desktop();
        return;
    }
    show_game_menu();
}

void StartupView::begin_game()
{
    // The question's answer, when it was asked rather than answered by a flag.
    if (auto *dialog = Object::cast_to<Window>(get_node_or_null("RulesDialog"));
        dialog && dialog->is_visible())
    {
        const auto rules = required_node<OptionButton>(*dialog, "Rows/Rules").get_selected();
        const auto aging = required_node<OptionButton>(*dialog, "Rows/Aging").get_selected();
        settings::choose_rules(
            rules == 0 ? GameRules{GameRules::Set::original,
                                   aging_choices.at(static_cast<std::size_t>(aging)).first}
                       : GameRules{});
        dialog->hide();
    }
    start_game({new_game_kind_, {}});
}

void StartupView::start_game(settings::GameStart start)
{
    starting_ = true;
    settings::choose_game_start(std::move(start));
    // Scene changes must occur after ready/input dispatch has completed.
    call_deferred("open_character_creation");
}

void StartupView::open_character_creation()
{
    const bool demo = settings::flag("--combat-demo");
    if (get_tree()->change_scene_to_file(demo ? "res://scenes/combat_demo.tscn"
                                              : "res://scenes/character_creation.tscn") != OK)
    {
        UtilityFunctions::push_error("Cannot open game scene.");
        get_tree()->quit(1);
    }
}
