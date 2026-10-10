#include "startup_view.h"
#include "localization.h"
#include "application_settings.h"
#include "guarded_handlers.h"
#include "godot_nodes.h"
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/file_dialog.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/translation_server.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <algorithm>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;
using presentation::required_node;

namespace
{
constexpr double text_fade_seconds = 0.6;
}

void StartupView::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("open_character_creation"),
                         &StartupView::open_character_creation);
}

void StartupView::_ready()
{
    set_process(false);
    i18n::initialize();
    choose_language();
}

void StartupView::choose_game_path()
{
    // Translate these dialogs only after language selection has been confirmed.
    i18n::prepare_ui(required_node<Window>(*this, "PathDialog"));
    i18n::prepare_ui(required_node<Window>(*this, "ChecksumWarning"));
    auto *dialog = &required_node<Window>(*this, "PathDialog"); // scene-owned
    dialog->connect("close_requested", presentation::guarded(this, &StartupView::close_language));
    required_node<Button>(*dialog, "Cancel").connect("pressed",
            presentation::guarded(this, &StartupView::close_language));
    required_node<Button>(*dialog, "Continue")
    .connect("pressed", presentation::guarded(this, &StartupView::accept_path));
    required_node<Button>(*dialog, "Browse").connect("pressed",
            presentation::guarded(this, &StartupView::browse_path));
    required_node<LineEdit>(*dialog, "Path").connect("text_submitted",
            presentation::guarded(this, &StartupView::submitted_path));
    required_node<LineEdit>(*dialog, "Path").connect("text_changed",
            presentation::guarded(this, &StartupView::path_edited));
    required_node<FileDialog>(*dialog, "BrowseDialog")
    .connect("dir_selected", presentation::guarded(this, &StartupView::picked_path));
    auto *warning = &required_node<Window>(*this, "ChecksumWarning");
    warning->connect("close_requested", presentation::guarded(this, &StartupView::close_language));
    required_node<Button>(*warning, "Quit").connect("pressed",
            presentation::guarded(this, &StartupView::close_language));
    required_node<Button>(*warning, "Continue")
    .connect("pressed", presentation::guarded(this, &StartupView::continue_path));
    const auto saved = settings::saved_game_path();
    pending_path_ = settings::game_path();
    if (pending_path_.is_empty())
        pending_path_ = OS::get_singleton()->get_environment("OPENGOLD_GAME_DIR");
    if (settings::flag("--reset-game-path") || saved.is_empty() ||
            !settings::validate_game_path(saved).usable)
    {
        show_path();
        return;
    }
    check_path();
}

void StartupView::choose_language()
{
    if (settings::flag("--reset-lang") || !settings::valid_language(settings::saved_language()))
    {
        choosing_language_ = true;
        auto *dialog = &required_node<Window>(*this, "LanguageDialog"); // scene-owned
        // Native language names remain stable; other text previews the selected locale.
        dialog->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
        dialog->connect("close_requested", presentation::guarded(this, &StartupView::close_language));
        auto *choices = &required_node<ItemList>(*dialog, "Choices");
        choices->add_item("English");
        choices->set_item_metadata(0, "en");
        choices->add_item(String::utf8("Español"));
        choices->set_item_metadata(1, "es");
        choices->select(i18n::language() == "es" ? 1 : 0);
        choices->connect("item_selected", presentation::guarded(this, &StartupView::preview_language));
        preview_language(choices->get_selected_items()[0]);
        choices->connect("item_activated", presentation::guarded(this, &StartupView::activate_language));
        required_node<Button>(*dialog, "Continue")
        .connect("pressed", presentation::guarded(this, &StartupView::accept_language));
        dialog->popup_centered();
        choices->grab_focus();
        required_node<Button>(*dialog, "Cancel").connect(
            "pressed", presentation::guarded(this, &StartupView::close_language));
        return;
    }
    choose_game_path();
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
    browser->popup_centered_ratio(.7);
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
    const auto result = settings::validate_game_path(pending_path_);
    if (!result.usable)
    {
        String message = result.error.is_empty() ? i18n::text("Missing or unreadable game files:")
                         : i18n::text(result.error.utf8().get_data());
        for (int i = 0; i < std::min(static_cast<int>(result.missing.size()), 5); ++i)
            message += " " + result.missing[i];
        if (result.missing.size() > 5)
            message += " ...";
        show_path(message);
        return;
    }
    required_node<Window>(*this, "PathDialog").hide();
    if (!result.different.is_empty())
    {
        choosing_path_ = true;
        auto *warning = &required_node<Window>(*this, "ChecksumWarning");
        required_node<RichTextLabel>(*warning, "Files").set_text(pending_path_ + "\n\n" +
                String("\n").join(result.different));
        warning->popup_centered();
        required_node<Button>(*warning, "Quit").grab_focus();
        return;
    }
    continue_path();
}

void StartupView::continue_path()
{
    required_node<Window>(*this, "ChecksumWarning").hide();
    if (save_pending_path_ && !settings::save_game_path(pending_path_))
    {
        show_path(i18n::text(
                      "Cannot save settings beside the executable. Check that the folder is writable and settings.cfg is valid."));
        return;
    }
    save_pending_path_ = false;
    choosing_path_ = false;
    get_viewport()->set_input_as_handled();
    begin_startup();
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
        return;
    auto *dialog = &required_node<Window>(*this, "LanguageDialog");
    auto *choices = &required_node<ItemList>(*dialog, "Choices");
    const auto selected = choices->get_selected_items();
    if (selected.is_empty())
        return;
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
    choose_game_path();
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
        return;
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
    required_node<Label>(*dialog, "Status").set_text(
        language_save_failed_ ? i18n::text("Cannot save language.") : String());
}

void StartupView::close_language()
{
    // The startup dialog's X follows the same global graceful shutdown path.
    get_tree()->get_root()->emit_signal("close_requested");
}

void StartupView::show_screen()
{
    auto *image = &required_node<TextureRect>(*this, "Image"); // scene-owned
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
    auto *text = &required_node<TextureRect>(*this, "Text"); // scene-owned
    text->set_texture(lettering);
    auto tint = get_theme_color("startup_fade_tint", "OpenGoldPalette");
    tint.a = 0;
    text->set_self_modulate(tint);
    fade_elapsed_ = 0;
    set_process(true);
    layout_text();
}

void StartupView::_process(double delta)
{
    if (finishing_)
        return;
    fade_elapsed_ = std::min(text_fade_seconds, fade_elapsed_ + std::max(0.0, delta));
    auto tint = get_theme_color("startup_fade_tint", "OpenGoldPalette");
    tint.a = fade_elapsed_ / text_fade_seconds;
    required_node<TextureRect>(*this, "Text").set_self_modulate(tint);
    if (fade_elapsed_ >= text_fade_seconds)
        set_process(false);
}

void StartupView::_notification(int what)
{
    if (what == NOTIFICATION_RESIZED && is_node_ready())
        layout_text();
}

void StartupView::layout_text()
{
    // The text uses the same centered, uniform fit as the shared background.
    const auto texture = required_node<TextureRect>(*this, "Image").get_texture();
    if (texture.is_null())
        return;
    const auto source = texture->get_size();
    const double fit = std::min(get_size().x / source.x, get_size().y / source.y);
    const auto fitted = source * fit;
    const auto origin = (get_size() - fitted) * .5;
    // Keep the lettering proportions and comfortable margins from the reference.
    const double lettering_scale = get_theme_constant("startup_lettering_percent", "OpenGoldMetrics") / 100.0;
    const double vertical_offset = get_theme_constant("startup_lettering_up_percent", "OpenGoldMetrics") / 100.0;
    const Vector2 scene_offset(get_theme_constant("startup_lettering_offset_x", "OpenGoldMetrics"),
                               get_theme_constant("startup_lettering_offset_y", "OpenGoldMetrics"));
    const auto lettering_size = fitted * lettering_scale;
    auto *lettering = &required_node<TextureRect>(*this, "Text"); // scene-owned
    lettering->set_position(origin + (fitted - lettering_size) * .5 -
                            Vector2(0, fitted.y * vertical_offset) + scene_offset);
    lettering->set_size(lettering_size);
}

void StartupView::_input(const Ref<InputEvent> &event)
{
    const Ref<InputEventKey> key = event;
    if (finishing_ || choosing_language_ || choosing_path_ || key.is_null() || !key->is_pressed() ||
            key->is_echo())
        return;
    // Application shortcuts must not advance a splash.
    if (key->is_ctrl_pressed() && (key->get_keycode() == KEY_X || key->get_keycode() == KEY_S))
        return;
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
        return;
    finishing_ = true;
    set_process(false);
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
