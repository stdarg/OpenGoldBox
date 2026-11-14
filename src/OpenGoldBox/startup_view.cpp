#include "startup_view.h"
#include "localization.h"
#include "application_settings.h"
#include "guarded_handlers.h"
#include "godot_nodes.h"
#include "game_menu_view.h"
#include "godot_images.h"
#include "godot_path.h"
#include "godot_sound_output.h"
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
    show_title();
    choose_language();
}

// The title screen behind the startup dialogs: a placeholder until the game's
// own title art is made.
void StartupView::show_title()
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
constexpr std::array<MenuGame, 3> playable_games{
    {{opengold::GoldBoxGame::pool_of_radiance, N_("Pool of Radiance")},
     {opengold::GoldBoxGame::curse_of_the_azure_bonds, N_("Curse of the Azure Bonds")},
     {opengold::GoldBoxGame::secret_of_the_silver_blades, N_("Secret of the Silver Blades")}}};

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

} // namespace

// After the language, the startup menu: Options, a button for each game and
// Exit to desktop (the user's design, 2026-11-12).
void StartupView::show_game_menu()
{
    auto *menu = &required_node<Window>(*this, "GameMenu"); // scene-owned
    if (!game_menu_ready_)
    {
        game_menu_ready_ = true;
        connect_path_dialogs();
        i18n::prepare_ui(*menu);
        menu->connect("close_requested",
                      presentation::guarded(this, &StartupView::exit_to_desktop));
        required_node<Button>(*menu, "Buttons/Options")
            .connect("pressed", presentation::guarded(this, &StartupView::open_options));
        required_node<Button>(*menu, "Buttons/Exit")
            .connect("pressed", presentation::guarded(this, &StartupView::exit_to_desktop));
        auto *games = &required_node<VBoxContainer>(*menu, "Buttons/Games");
        for (const auto &[game, title] : playable_games)
        {
            auto *button = memnew(Button); // owned by the menu once added
            button->set_name(std::string(opengold::game_folder(game)).c_str());
            button->set_text(i18n::text(title));
            button->set_custom_minimum_size({0, 52});
            button->connect("pressed", presentation::guarded(this, &StartupView::game_chosen)
                                           .bind(static_cast<std::int64_t>(game)));
            games->add_child(button);
        }
    }
    // --game=POOLRAD (or CURSE, or SECRET) picks the game without the menu, for the
    // packaged game's own checks.
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
                    game_from_flag_ = true;
                    game_chosen(static_cast<std::int64_t>(game));
                    return;
                }
            }
        }
    }
    choosing_game_ = true;
    menu->popup_centered();
    required_node<Button>(*menu, "Buttons/Games/POOLRAD").grab_focus();
}

void StartupView::game_chosen(std::int64_t chosen)
{
    const auto game = static_cast<opengold::GoldBoxGame>(chosen);
    settings::choose_game(game);
    required_node<Window>(*this, "GameMenu").hide();
    choosing_game_ = false;
    folders_needed_ = {game};
    // The party's portraits and combat art still come from Pool of Radiance's
    // files (docs/GAMES.md), so another game needs its folder too.
    if (game != opengold::GoldBoxGame::pool_of_radiance &&
        !settings::validate_game_path(settings::game_path()).usable)
    {
        folders_needed_.push_back(opengold::GoldBoxGame::pool_of_radiance);
    }
    next_folder();
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
    required_node<Window>(*this, "GameMenu").hide();
    options->open_options_alone();
}

void StartupView::options_closed()
{
    if (auto *options = get_node_or_null("Options"))
    {
        options->queue_free();
    }
    show_game_menu();
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
    if (folders_needed_.empty())
    {
        begin_startup();
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

void StartupView::connect_path_dialogs()
{
    // Translate these dialogs only after language selection has been confirmed.
    i18n::prepare_ui(required_node<Window>(*this, "PathDialog"));
    i18n::prepare_ui(required_node<Window>(*this, "ChecksumWarning"));
    auto *dialog = &required_node<Window>(*this, "PathDialog"); // scene-owned
    dialog->connect("close_requested", presentation::guarded(this, &StartupView::close_language));
    required_node<Button>(*dialog, "Cancel")
        .connect("pressed", presentation::guarded(this, &StartupView::close_language));
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
    warning->connect("close_requested", presentation::guarded(this, &StartupView::close_language));
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
    show_game_menu();
}

void StartupView::show_path(const String &message)
{
    choosing_path_ = true;
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
    const auto result = settings::validate_game_folder(pending_path_, folders_needed_.front());
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
        choosing_path_ = true;
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
    if (save_pending_path_ && !settings::save_game_folder(folders_needed_.front(), pending_path_))
    {
        show_path(i18n::text(
            "Cannot save settings. Check that settings.cfg in the game's user data folder can be written and is valid."));
        return;
    }
    save_pending_path_ = false;
    choosing_path_ = false;
    get_viewport()->set_input_as_handled();
    folders_needed_.erase(folders_needed_.begin());
    next_folder();
}

void StartupView::begin_startup()
{
    auto *os = OS::get_singleton(); // borrowed engine singleton
    if (!os->get_cmdline_args().has("--splash") && !os->get_cmdline_user_args().has("--splash"))
    {
        finish();
        return;
    }
    show_screen();
}

void StartupView::accept_language()
{
    if (!choosing_language_ || finishing_)
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
    // Do not let the Enter press used here also advance the first splash.
    dialog->set_input_as_handled();
    show_game_menu();
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

void StartupView::show_screen()
{
    auto *image = &required_node<TextureRect>(*this, "ImageFit/Image"); // scene-owned
    // Load once: advancing changes only text, never the backdrop texture or geometry.
    if (image->get_texture().is_null())
    {
        Ref<Texture2D> texture = ResourceLoader::get_singleton()->load(
            "res://bin/splashes/OpenGoldBoxSplashBackground.png");
        if (texture.is_null())
        {
            UtilityFunctions::push_error("Missing shared splash background");
            finish();
            return;
        }
        image->set_texture(texture);
    }
    const String lettering_path =
        String("res://bin/splashes/") +
        (screen_ == 0 ? "OpenGoldBoxEngineLettering" : "OpenGoldBoxGameLettering") +
        (i18n::language() == "es" ? ".es.png" : ".png");
    Ref<Texture2D> lettering = ResourceLoader::get_singleton()->load(lettering_path);
    if (lettering.is_null())
    {
        UtilityFunctions::push_error(String("Missing splash lettering: ") + lettering_path);
        finish();
        return;
    }
    auto *text = &required_node<TextureRect>(*this, "ImageFit/Image/Text"); // scene-owned
    text->set_texture(lettering);
    text->call("begin");
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
// Application shortcuts must not advance a splash or a title.
bool application_shortcut(const Ref<InputEventKey> &key)
{
    return key.is_valid() && key->is_ctrl_pressed() &&
           (key->get_keycode() == KEY_X || key->get_keycode() == KEY_S);
}
} // namespace

void StartupView::respond_to_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventKey> key = event;
    if (application_shortcut(key))
    {
        return;
    }
    if (game_title_)
    {
        // Any key or click, as Secret of the Silver Blades takes its mouse's.
        const Ref<InputEventMouseButton> click = event;
        if ((key.is_valid() && key->is_pressed() && !key->is_echo()) ||
            (click.is_valid() && click->is_pressed()))
        {
            get_viewport()->set_input_as_handled();
            if (game_title_->key_ends_all)
            {
                end_game_title();
            }
            else
            {
                next_title_screen();
            }
        }
        return;
    }
    if (finishing_ || choosing_language_ || choosing_path_ || choosing_game_ || key.is_null() ||
        !key->is_pressed() || key->is_echo())
    {
        return;
    }
    get_viewport()->set_input_as_handled();
    if (key->get_keycode() == KEY_ESCAPE || screen_ == 1)
    {
        finish();
    }
    else
    {
        ++screen_;
        show_screen();
    }
}

void StartupView::finish()
{
    if (finishing_)
    {
        return;
    }
    finishing_ = true;
    required_node<TextureRect>(*this, "ImageFit/Image/Text").call("cancel");
    show_game_title();
}

void StartupView::show_game_title()
{
    const auto game = settings::chosen_game();
    if (!game_from_flag_ && !settings::flag("--combat-demo"))
    {
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
    }
    if (!game_title_ || game_title_->screens.empty())
    {
        end_game_title();
        return;
    }
    title_screen_ = 0;
    show_title_screen();
}

void StartupView::show_title_screen()
{
    const auto &screen = game_title_->screens.at(title_screen_);
    required_node<TextureRect>(*this, "GameTitle/Picture")
        .set_texture(presentation::image_texture(screen.screen));
    required_node<Control>(*this, "GameTitle").show();
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
        // The level the exploration screen plays the original's sounds at.
        title_sound_->set_volume(0.125);
    }
    title_sound_->play(sound);
}

void StartupView::_process(double delta)
{
    presentation::run_guarded(*this,
                              [&]
                              {
                                  if (game_title_ && (title_seconds_left_ -= delta) <= 0)
                                  {
                                      next_title_screen();
                                  }
                              });
}

void StartupView::next_title_screen()
{
    if (++title_screen_ == game_title_->screens.size())
    {
        end_game_title();
        return;
    }
    show_title_screen();
}

void StartupView::end_game_title()
{
    game_title_.reset();
    required_node<Control>(*this, "GameTitle").hide();
    ask_rules();
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
        dialog->connect("close_requested",
                        presentation::guarded(this, &StartupView::cancel_new_game));
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

// Closing the question returns to the games.
void StartupView::cancel_new_game()
{
    required_node<Window>(*this, "RulesDialog").hide();
    finishing_ = false;
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
