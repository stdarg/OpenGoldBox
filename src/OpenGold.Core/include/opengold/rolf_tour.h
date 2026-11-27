#ifndef OPENGOLD_ROLF_TOUR_H
#define OPENGOLD_ROLF_TOUR_H

#include "opengold/ecl_machine.h"
#include "opengold/exploration_view.h"
#include "opengold/map_catalog.h"
#include "opengold/overland.h"
#include "opengold/formats.h"
#include "opengold/vga_picture.h"
#include "opengold/wall_art.h"
#include "opengold/wall_art_replacements.h"
#include "opengold/creature_catalog.h"
#include "opengold/campaign_party.h"
#include "opengold/campaign_encounter.h"
#include "opengold/buck_rogers_records.h"
#include "opengold/buck_rogers_rules.h"
#include "opengold/buck_rogers_solar_map.h"
#include "opengold/difficulty.h"
#include "opengold/teleport.h"
#include "opengold/monster_bundle.h"
#include "opengold/encounter_data.h"
#include "opengold/companions.h"
#include "opengold/encounter_budget.h"
#include "opengold/shop_data.h"
#include "opengold/temple_catalog.h"
#include "opengold/phlan_resources.h"
#include "opengold/tour_snapshot.h"
#include "opengold/town_service_host.h"
#include "opengold/hall_service.h"
#include "opengold/temple_service.h"
#include "opengold/vault_service.h"
#include "opengold/shop_service.h"
#include "opengold/treasure_service.h"
#include "opengold/camp_service.h"
#include "opengold/ship_service.h"
#include "opengold/buck_rogers_menu_service.h"
#include <algorithm>
#include <bitset>
#include <map>
#include <set>
#include <string>

namespace opengold::test
{
struct TourAccess;
} // namespace opengold::test

namespace opengold::por
{
// Squares the party sees ahead before a wall or door, at most two. As in the
// original, a fight's encounter distance never exceeds it in a 3D area.
[[nodiscard]] unsigned open_squares_ahead(const GeoMap &map, PartyPose pose);
// The ship fight's files and state (buck_rogers_ship_fight.h), which the
// session holds without the rules' types.
struct ShipFightFiles;
struct ShipFightState;
struct ShipFightAccess;

// Where a Wand of Secrets points (SECRET-1): a trap on square (x, y), or a
// secret door in its `side` wall, the square on the door's side nearer the
// party.
struct WandSign
{
    unsigned x{}, y{};
    MapDirection side{};
    bool trap{};
};

// The original builds sentences from pieces: "A BUGBEAR LEADING" then "A
// PARTY OF " printed one after the other, a monster name stored with a leading
// space. Its text window's wrapping hid the seams; on one line they read as
// "LEADINGA PARTY" and "OF  KOBOLDS". Joins `added` to `shown` with a space
// where two words would touch, and closes a doubled space after a word (two
// after a sentence's end are the original's style and stay).
[[nodiscard]] std::string join_script_text(std::string shown, std::string_view added);

// One answer at the original's encounter menu (eclvm.pas EclEncounter):
// `result` is what the script receives (0 the monsters leave, 1 a fight, 2
// the party gets away, 3 a parley), or nothing while the meeting goes on; then
// `closer` says whether the monsters came a step closer or both sides waited.
struct EncounterTurn
{
    std::optional<unsigned> result;
    bool closer{};
};

// The original's table for the script's `response` (0 to 4) to the party's
// `choice` (0 Fight, 1 Wait, 2 Flee, 3 Advance, 4 Parley) with the monsters
// `distance` steps away. `party_escapes` when its slowest member keeps up with
// the meeting's flight threshold; `monsters_outrun` when the monsters are
// faster than the party's fastest member.
[[nodiscard]] EncounterTurn encounter_turn(std::size_t choice, unsigned response, unsigned distance,
                                           bool party_escapes, bool monsters_outrun);

// Original tour followed by the New Phlan movement/search/script scheduler.
// Bound VM position cells are authoritative; snapshots are read-only views.
class RolfTourSession final : private TownServiceHost
{
  public:
    // A session drives a shared party, so it is not copied by accident: a
    // copy would be a second session acting on the same party (Effective C++
    // Item 6). detached_copy() is an explicit copy without the party.
    RolfTourSession(RolfTourSession &&) = default;
    RolfTourSession &operator=(RolfTourSession &&) = default;
    ~RolfTourSession() = default;
    [[nodiscard]] RolfTourSession detached_copy() const;

    // `encounters` says what each area's original creature records fight as
    // (data/encounters). New wall art replaces the original appearances it
    // covers; exported monsters bring their names, gear and art to the fights
    // they are in.
    [[nodiscard]] static RolfTourSession load(const std::filesystem::path &directory,
                                              const EncounterData &encounters,
                                              const WallArtReplacements &new_wall_art = {},
                                              std::shared_ptr<const MonsterBundle> monsters = {});
    // Also accepts wholly synthetic resources for asset-free host tests.
    RolfTourSession(GeoMap map, std::shared_ptr<const EclProgram> program,
                    std::array<opengold::Image, 3> sprites, std::uint32_t entry,
                    WallArtSet wall_art = {}, std::shared_ptr<const PhlanResources> town = {});

    // The game whose campaign this is, which its scripts belong to.
    [[nodiscard]] GoldBoxGame game() const noexcept override
    {
        return program_->game();
    }

    void restart();
    void campaign_party(std::shared_ptr<opengold::CampaignParty> party);
    // Attach an already validated replacement without restarting its restored VM.
    void attach_restored_party(std::shared_ptr<opengold::CampaignParty> party);

    [[nodiscard]] bool can_leave() const
    {
        return snapshot_.phase == TourPhase::completed;
    }

    // Word of Recall from camp (LEVELS-3): cast on the square where the party
    // used a temple of New Phlan, it prepares the party's sanctuary there;
    // elsewhere it takes the whole party back to that sanctuary. Throws when
    // it can do neither, before spending anything. `action` is its camp
    // action: "word_of_recall", or "word_of_recall_7" from a higher slot.
    void recall(opengold::MemberId caster, const std::string &action);

    [[nodiscard]] bool has_sanctuary() const
    {
        return sanctuary_.has_value();
    }

    // Teleport (TELEPORT-1): the maps with a teleport spot that the party has
    // been on, in the spots' order.
    [[nodiscard]] std::vector<unsigned> teleport_destinations() const;
    // Teleport from camp or exploring, `action` its camp action ("teleport",
    // or "teleport_8" from a higher slot): the whole party goes to the map's
    // spot, through the map's own way in when it is another script's. Throws
    // when the party cannot leave or has not been on that map, before
    // spending anything.
    void teleport(opengold::MemberId caster, const std::string &action, unsigned map);
    // A spell cast outside combat while exploring, `action` its camp action,
    // on `patient`. Treasures' scripts may wait for one (the last spell's
    // cell, 4Bh, at FFh, as before Cortarra's twins): the spell's number is
    // recorded there and the square's event runs again, as its map menu runs
    // the step and location entries after a cast (its NoncombatCastMenu).
    // Throws, casting nothing, while magic_barred().
    void cast_outside_combat(opengold::MemberId caster, opengold::MemberId patient,
                             const std::string &action);
    // True while a Pools of Darkness script bars magic (its cell 2,
    // antiMagic): no one casts a spell, in a fight or in camp ("cannot cast
    // spells in this area", camp.pas, CanDoSpellActivity), and no one uses an
    // item (viewchar.pas, UseItemMenu).
    [[nodiscard]] bool magic_barred() const;

    // The Wand of Secrets (SECRET-1): while the party explores, `member` spends
    // a charge of the wand `item`, and the hidden secret door or armed trap
    // nearest the party within 60 feet, six of the map's ten-foot squares, is
    // found: the door shows, and the trap's script sees it as Find Traps
    // would. It returns where it is, or nothing when none is that near.
    // Throws, spending nothing, when the party is busy or the wand is empty.
    [[nodiscard]] std::optional<WandSign> point_wand_of_secrets(opengold::MemberId member,
                                                                std::uint64_t item);

    // Auto-equip (EQUIP-1), from the game's settings: nothing for off, or
    // whether an unidentified magic item is put on. It applies to the items
    // bought and found from then on.
    void set_auto_equip(std::optional<opengold::UnidentifiedMagic> auto_equip) noexcept
    {
        auto_equip_ = auto_equip;
    }

    // Outside events, and while shopping to choose the buyer or at a treasure
    // to choose who takes. The shop script receives whoever is selected when
    // the shop closes.
    [[nodiscard]] bool can_select_member() const
    {
        return can_leave() || snapshot_.phase == TourPhase::shopping ||
               snapshot_.phase == TourPhase::treasure || buck_rogers_menus_.selects_member();
    }

    void advance(double seconds) override;
    // Treasures' overland colors turn as time passes, a step each time the
    // map's color cycle's delay in hundredths of a second has passed (its
    // MenuCore.UpdateAnimation); true when they turned and the view is to be
    // drawn again.
    bool turn_colors(double seconds);
    bool continue_dialogue(std::uint64_t ticket);
    bool choose(std::uint64_t ticket, std::size_t choice);
    bool input(std::uint64_t ticket, std::string_view value);
    bool buy(std::uint64_t ticket, std::size_t item);
    bool leave_shop(std::uint64_t ticket);
    // At a treasure: the selected member takes the pile's `item`. False, with
    // the reason in the diagnostic, when the member can carry no more.
    bool take_treasure(std::uint64_t ticket, std::size_t item);
    // Leaves what is still in the pile behind for good; the script goes on.
    bool leave_treasure(std::uint64_t ticket);
    // The Buck Rogers games' solar map while snapshot().solar_map
    // (docs/games/countdown.md, "The solar map"): the bodies' phase and the
    // rocket as zone 0 keeps them. The rocket flies by explore() with a step
    // in one of the eight directions: the script's step entry spends the
    // fuel, the rocket steps (original::step_rocket), the team rests three
    // days, and the script's search entry finds the body it reached.
    [[nodiscard]] original::SolarMapState solar_map_state() const;
    // The body the scripts found the rocket on (the planet cell, 4BA5h):
    // its number in the scripts' order, or none en route.
    [[nodiscard]] std::optional<std::size_t> orbited_body() const;

    // The team's ship as zone 0 keeps it from 4D00h (types.pas TShip): its
    // name and its six systems, each as it is now and whole, in the order
    // of ship_systems.
    struct ShipSystem
    {
        unsigned now{}, whole{};
    };

    enum class ShipSystemKind : std::uint8_t
    {
        hull,
        sensors,
        controls,
        life_support,
        fuel,
        engines
    };

    struct TeamShip
    {
        std::string name;
        std::array<ShipSystem, 6> systems;

        [[nodiscard]] const ShipSystem &system(ShipSystemKind kind) const
        {
            return systems.at(static_cast<std::size_t>(kind));
        }
    };

    [[nodiscard]] TeamShip team_ship() const;

    // The game clock as the scripts read it: the year, the month of twelve
    // and the day of thirty, from 1, and the hour and minute of the day.
    struct GameClock
    {
        unsigned year{}, month{1}, day{1}, hour{}, minute{};
    };

    [[nodiscard]] GameClock game_clock() const;
    // The ship fight itself, its files and what followed it are opened and
    // read through buck_rogers_ship_fight.h (ship_fight, ship_fight_files,
    // ship_fight_aftermath).
    // The Buck Rogers team's combat icons (CHARS), one for each member in
    // party order (an empty image where the file lacks it), as its ground
    // fights and the ship-combat screen draw it.
    [[nodiscard]] std::vector<CombatArt> buck_rogers_team_art() const;
    // The open ship fight is over: what follows it plays (follow_ship_fight),
    // its end and the team's ship go into the script's memory
    // (ship_combat_memory) and the team's records, and the script goes on;
    // after a boarding, script 52's fight on the enemy's decks is next, and
    // the repairs follow that fight. False when no fight waits or it is not
    // over.
    bool finish_ship_combat(std::uint64_t ticket);

    // The training while snapshot().phase is training; null otherwise. The
    // trainee's record is unchanged until its choices are kept.
    [[nodiscard]] const BuckRogersTraining *buck_rogers_training() const noexcept;

    // The choices, made as the rules allow, a specialization only in a
    // weapon the menu offers (item templates 1 to 15); each throws
    // std::logic_error outside a training.
    [[nodiscard]] original::SkillRefusal raise_training_skill(original::Skill skill);
    bool lower_training_skill(original::Skill skill);
    [[nodiscard]] bool specialize_in_training(std::uint8_t item_type);
    bool unspecialize_in_training(std::uint8_t item_type);
    // Keeps the trained member's record and shows the team menu again.
    // Throws std::logic_error outside a training or while its specialization
    // is unchosen.
    void finish_buck_rogers_training();
    bool explore(ExplorationCommand command);

    // The original's Search mode: each step searches as Look does, and takes
    // ten minutes more.
    void set_searching(bool searching) noexcept
    {
        snapshot_.searching = searching;
    }

    // Both kinds run the original pre-camp and interruption services.
    bool camp(RestKind kind);
    // A long rest of `length`, under rules that rest any length
    // (CampaignParty::rest(std::chrono::minutes)), as the original's camp
    // rests for the time its rest menu shows.
    bool camp(std::chrono::minutes length);
    // Treasures' camp opens as the original's does (eclvm.pas, EnterCamp):
    // the map's camp entry runs, then the romance's scene is told if one is
    // due (camp.pas, CampMainMenu, UpdateRelationship), and the snapshot's
    // camp_menu_open asks for the camp dialog. A camp, Fix included, made
    // while the menu is open rests without running the entry again. False in
    // the other games, whose dialog opens at once, and when the party cannot
    // camp now.
    bool open_camp();
    // Treasures' and Secret's camp menus (camp.pas, CampMainMenu). Fix camps
    // as a rest does, but the party rests only while its casters cure and
    // heal it (fix_party). Crown asks Amanitas, through the crown he gave, for the
    // next luck reading, while the party has it (cell 3Fh) and its last
    // reading has not been given (cell 25Ch at 2). False when the camp
    // cannot do so now.
    [[nodiscard]] bool camp_offers_fix() const;
    bool camp_fix();
    [[nodiscard]] bool camp_offers_crown() const;
    bool camp_crown();
    // The crystals the party holds, which Treasures' camp shows (cell 3Ch:
    // bit 0 red, 1 green, 2 blue); 0 in the other games.
    [[nodiscard]] unsigned camp_crystals() const;
    // The party leaves a camp a script offered without resting. In Curse's
    // wilderness its menu returns, as the original runs the script again.
    void decline_camp();
    // New dice for the script, as a reload above Easy gives (reseed_after_load).
    void reseed_dice(std::uint32_t seed);
    // A percentage of the SRD's Moderate XP budget, 0-200. Original encounters
    // above the party's budget, or with more creatures than characters, shrink.
    void encounter_challenge(unsigned challenge);

    // On Hard and above, monsters bring their potions, wands and scrolls into
    // a fight and use them (DIFF-1), and temples cast no Resurrection
    // (TEMPLE-1).
    void difficulty(opengold::DifficultySetting setting) noexcept
    {
        difficulty_setting_ = setting;
        difficulty_ = opengold::played_difficulty(setting);
        if (opengold::is_buck_rogers(game()))
        {
            (void)machine_.refresh_bound_variable(EclAddress{0x4BB6},
                                                  static_cast<std::uint16_t>(setting));
        }
    }

    void difficulty(opengold::Difficulty difficulty) noexcept
    {
        this->difficulty(opengold::difficulty_setting(difficulty));
    }

    // The original's NPCs who join the party (data/companions) and the
    // character creation rules they are built with (Skullcrusher, CADORNA-1).
    // Without them, ADD NPC accepts only the profiles in the area's resources.
    void companion_rules(std::shared_ptr<const rules::CharacterRules> creation,
                         std::vector<CompanionDefinition> companions)
    {
        companion_rules_ = std::move(creation);
        companions_ =
            std::make_shared<const std::vector<CompanionDefinition>>(std::move(companions));
    }

    // The temples data/shops describes (TEMPLE-1); a temple it does not name
    // offers the original's services and prices.
    void temples(std::vector<opengold::TempleSite> sites)
    {
        temple_service_.temples(std::move(sites));
    }

    [[nodiscard]] const std::vector<opengold::TempleSite> &temples() const noexcept
    {
        return temple_service_.temples();
    }

    // The shops data/shops describes: each sells its record's items at its
    // prices. A shop it does not name sells the original's list. Throws when an
    // item's source list is not in the loaded game. With the original's stock
    // (the per-game Original Shops setting), a shop OpenGoldBox opens behind a
    // door of its own (the Rogue's Shop) is left out, and the others sell only
    // the original's items, at their records' values.
    void shops(std::vector<opengold::ShopSite> sites,
               opengold::ShopStock stock = opengold::ShopStock::modified);

    // The town's shops and temples as the game gives them to a new campaign
    // and to an opened save: those the data/shops folder `shop_folder`
    // describes, with the stock the game's Original Shops setting chooses.
    // Throws on a file the game cannot use.
    void use_shop_data(const std::filesystem::path &shop_folder, opengold::ShopStock stock);

    [[nodiscard]] const std::vector<opengold::ShopSite> &shops() const noexcept
    {
        return shop_service_.shops();
    }

    // Every shop's items, in the order of shops(), at their prices (SELL-1).
    [[nodiscard]] const std::vector<std::vector<Equipment>> &town_stock() const noexcept
    {
        return shop_service_.town_stock();
    }

    [[nodiscard]] const TourSnapshot &snapshot() const noexcept
    {
        return snapshot_;
    }

    // The areas as load() read them, for a tool that shows what the game reads
    // (the Creature Stats Editor). Null for synthetic resources.
    [[nodiscard]] std::shared_ptr<const PhlanResources> resources() const noexcept
    {
        return town_;
    }

    [[nodiscard]] const GeoMap &map() const noexcept
    {
        return map_;
    }

    [[nodiscard]] const auto &sprites() const noexcept
    {
        return sprites_;
    }

    // The walls new art replaced shown in their original art (#274); the
    // view's setting, not saved with the game.
    void show_original_walls(bool original) noexcept
    {
        original_walls_ = original;
    }

    [[nodiscard]] const WallArtSet &wall_art() const noexcept
    {
        return wall_art_;
    }

    // Compose the current 3D view and remember its visible cells. Call only
    // when presenting exploration, not when an encounter picture replaces it.
    [[nodiscard]] Image observe_view();

    [[nodiscard]] const TownParty &party() const noexcept
    {
        return party_;
    }

    [[nodiscard]] const std::vector<Equipment> &shop_stock() const noexcept
    {
        return shop_service_.shop_stock();
    }

    // The items of the treasure the party stands at (TourPhase::treasure).
    [[nodiscard]] const std::vector<Equipment> &treasure_pile() const noexcept
    {
        return treasure_service_.treasure_pile();
    }

    [[nodiscard]] const std::vector<std::string> &script_diagnostics() const noexcept
    {
        return diagnostics_;
    }

    [[nodiscard]] const std::optional<Image> &picture() const noexcept
    {
        return picture_;
    }

    [[nodiscard]] std::uint16_t script_variable(EclAddress address) const
    {
        return machine_.variable(address);
    }

    // Whether the member will fight for the monsters in the next fight, as a
    // script or a treacherous companion decided; the original shows its name
    // in yellow.
    [[nodiscard]] bool turns_on_party(MemberId id) const
    {
        return campaign_ && turncoat(id);
    }

    [[nodiscard]] const std::optional<opengold::CampaignEncounter> &pending_encounter() const
    {
        static const std::optional<opengold::CampaignEncounter> not_ready;
        // Combat waits until the player dismisses the monster close-up.
        return showing_monster_picture_ ? not_ready : encounter_;
    }

    // The monster close-up that loops before combat, or null when none is showing.
    [[nodiscard]] const std::vector<AnimationFrame> *monster_picture() const;
    // The frames of PIC record `id` as the game shows them, null for none:
    // Treasures draws the picture its cell 4Eh names in the colors of the
    // record its cell 4Fh names, laid over the picture's own (its portrait.pas,
    // LoadAnimation, which loads that record as it loads the picture).
    [[nodiscard]] const std::vector<AnimationFrame> *pic_frames(unsigned id) const;
    // The animation on screen now, if any: the monster's close-up once it is beside
    // the party, or a picture the script shows. Null shows the still view.
    [[nodiscard]] const std::vector<AnimationFrame> *scene_animation() const;
    // Where scene_animation() comes from, for its new art: the PIC archive bank
    // and record. Empty when none is on screen.
    [[nodiscard]] std::optional<std::pair<unsigned, unsigned>> scene_animation_source() const;
    // The player pressed a key on the close-up; returns false when none is showing.
    bool start_encounter();

    bool resolve_combat(const rules::Snapshot &result);
    // Reject a pending handoff before combat starts, using the event rollback path.
    bool reject_combat(std::string diagnostic);

    // Reads `tour` from a campaign save, or writes a const one, only while
    // the party is idle between events (campaign_save.cpp, docs/SAVES.md).
    template <class Codec, class Tour> static void save(Codec &codec, Tour &tour);

  private:
    RolfTourSession(const RolfTourSession &) = default;
    RolfTourSession &operator=(const RolfTourSession &) = default;
    // Tests set a flag the route cannot reach through the game (tests/tour_access.h).
    friend struct opengold::test::TourAccess;
    // Clears everything one run of the tour builds up; restart() also reloads
    // the town's map and a fresh script machine.
    void reset_run();
    void start_opening();
    // The PIC record scene_animation() shows, if any.
    [[nodiscard]] std::optional<unsigned> scene_animation_record() const;
    GeoMap map_;
    std::shared_ptr<const EclProgram> program_;
    std::array<opengold::Image, 3> sprites_;
    WallArtSet wall_art_;
    bool original_walls_{};
    EclMachine machine_;
    std::uint32_t entry_;
    TourSnapshot snapshot_;
    std::uint64_t next_ticket_{}, menu_request_{}, delayed_request_{};
    double remaining_delay_{};
    std::shared_ptr<const PhlanResources> town_;
    TownParty party_;
    std::shared_ptr<opengold::CampaignParty> campaign_;
    // The experience a creature record of the area awards: its conversion's,
    // or under rules that play the original's records, the record's own.
    [[nodiscard]] unsigned record_experience(unsigned record) const;
    // What the treasure a fight won is worth in experience: under rules that
    // play the original's records, its coins, gems, jewelry and magic; else
    // nothing.
    [[nodiscard]] unsigned treasure_experience(const PendingLoot &loot) const;
    // Each member's share of a fight's experience: under rules that play the
    // original's records, the total divided among the party, as the original
    // does; otherwise the total, each member earning it.
    [[nodiscard]] unsigned experience_share(unsigned total) const;

    // Whether the party's rules fight the original's records as they are.
    [[nodiscard]] bool plays_original_records() const
    {
        return campaign_ && campaign_->rule_module().plays_original_records();
    }

    std::uint64_t who_request_{};
    // A DAMAGE volley waits here until its results are acknowledged.
    std::uint64_t damage_request_{};
    // What the script hears as those words are acknowledged (Treasures' luck
    // reading after the crown's last).
    std::vector<EclMemoryWrite> damage_writes_;
    // Tyranthraxus defeated: the game's won cell is written as the closing
    // words are acknowledged (PROGRAM 8).
    bool victory_{};
    // The cell that wins the game (Secret's soul cell 4CD9h, Treasures'
    // Freezefire cell 25Ch) as the event began: the event that sets it from 0
    // to 1 wins the game as it ends.
    std::uint16_t won_cell_at_event_start_{};

    std::shared_ptr<const rules::CharacterRules> companion_rules_;
    std::shared_ptr<const std::vector<CompanionDefinition>> companions_;
    // OpenGoldBox's shops with a hidden entrance (SECRET-1): each entrance's
    // state by its shop's data/shops id (saved).
    enum class EntranceState : std::uint8_t
    {
        hidden,
        found, // a locked door
        open
    };

    std::map<std::string, EntranceState> entrances_;
    // The traps a Wand of Secrets pointed out (saved), as map * 256 + y * 16
    // + x: on such a square a script asking for Find Traps finds it.
    std::set<unsigned> wand_traps_;
    [[nodiscard]] bool on_wand_trap() const;
    // What the town services reach of the tour (town_service_host.h).
    [[nodiscard]] EclMachine &script_machine() noexcept override;
    [[nodiscard]] const EclMachine &script_machine() const noexcept override;
    [[nodiscard]] CampaignParty *campaign() const noexcept override;
    [[nodiscard]] TourSnapshot &screen() noexcept override;
    [[nodiscard]] std::uint64_t new_ticket() noexcept override;
    [[nodiscard]] PartySlot selected_character() const noexcept override;
    void select_character(PartySlot slot) noexcept override;
    [[nodiscard]] EclHostReply character_reply(PartySlot slot) const override;
    [[nodiscard]] bool
    standing_at(const std::string &archive, unsigned record,
                const std::vector<std::pair<unsigned, unsigned>> &markers) const override;
    [[nodiscard]] Difficulty played_difficulty() const noexcept override;
    void log_player(const std::string &action) const override;
    [[nodiscard]] const PhlanResources &town_resources() const override;
    [[nodiscard]] TownParty &town_party() noexcept override;
    [[nodiscard]] HeldItems held_items() const override;
    void equip_gained(const HeldItems &before) override;
    void choose_shopper() override;
    void step_out_of_shop() override;
    // The town services, each the session's to forward to.
    HallService hall_service_;
    TempleService temple_service_;
    VaultService vault_service_;
    ShopService shop_service_;
    TreasureService treasure_service_;
    CampService camp_service_;
    ShipService ship_service_;
    BuckRogersMenuService buck_rogers_menus_;
    std::vector<PartySlot> who_slots_;
    // How a pre-combat meeting ended without a fight, said when its event ends.
    std::string encounter_outcome_;
    // Whether this event's script asked which member acts (WHO).
    bool member_chosen_{};
    // Where the party met monsters this event, and whether it was robbed.
    std::optional<PartyPose> meeting_pose_;
    bool robbed_{};
    std::optional<opengold::UnidentifiedMagic> auto_equip_;
    std::optional<Image> picture_;
    std::optional<unsigned> picture_animation_id_; // The PIC record picture_ begins.

    // A Treasures picture drawn in a color record's colors (pic_frames): the
    // picture, the color record and the frames, kept while both stay the same.
    struct RecolouredPicture
    {
        unsigned picture{}, colors{};
        std::vector<AnimationFrame> frames;
    };

    mutable std::optional<RecolouredPicture> recoloured_picture_;

    // Everything an event can change, saved when it begins so a failed event
    // rolls back as a whole rather than member by hand-paired member
    // (Effective C++ Item 29).
    struct EventCheckpoint
    {
        EclMachine machine;
        TownParty party;
        std::optional<opengold::PartyState> campaign;
        unsigned script{}, area{};
        std::map<unsigned, MapKnowledge> visited_areas, seen_areas;
        std::map<std::string, EntranceState> entrances;
        std::set<unsigned> wand_traps;
        unsigned rest_checks{};
        unsigned travel_mode{};
        unsigned dark_queen_combat_set{};
        VaultService dark_queen_vault;
        GeoMap map;
        std::vector<PendingLoot> pending_loot;
        TourSnapshot snapshot;
        PartySlot selected_character{};
    };

    std::optional<EventCheckpoint> event_checkpoint_;
    void roll_back_event();
    std::vector<std::string> diagnostics_;
    std::optional<ExplorationCommand> pending_movement_;
    unsigned current_script_{};
    PartySlot selected_character_{};
    unsigned current_area_{};
    // The original's travel mode: 1 in a 3D area, 2-4 on the west, central or
    // east window of the wilderness map, where the party's square is the
    // script's 0x49C3/0x49C4 and it steps in eight directions.
    unsigned travel_mode_{1};

    [[nodiscard]] bool overland() const noexcept
    {
        return travel_mode_ > 1;
    }

    void update_travel_mode();
    // Steps the party one wilderness square in its direction, unless the
    // terrain stops it; false when it stays.
    bool overland_step();
    // Gateway's overland: its script cleared the in-dungeon cell (4BE6h), and
    // the map is drawn from above with symbols rather than walls.
    [[nodiscard]] bool gateway_overland() const;
    // The open squares ahead of the party where monsters may stand, at most 2.
    [[nodiscard]] unsigned room_ahead() const;
    // A LOAD PIECES on Gateway's overland: the three symbol sets that draw
    // the map's cells, each into its slot of 70 symbols.
    void load_overland_symbols(const std::array<unsigned, 3> &banks);
    WallTiles overland_symbols_;
    // The 256-color engine's wall sets in the view's three slots, and
    // Treasures' overland picture its script loaded last.
    std::array<unsigned, 3> wall_slots_{};
    // Native LOAD FILES' second operand selects DUNGCOM/WILDCOM, independently
    // of the exploration map's three wall sets.
    unsigned dark_queen_combat_set_{};
    unsigned treasures_overland_picture_{};
    // How far Treasures' overland colors have turned, and the time since
    // their last turn (turn_colors).
    unsigned overland_color_steps_{};
    double overland_color_seconds_{};
    std::map<unsigned, MapKnowledge> visited_areas_;
    std::map<unsigned, MapKnowledge> seen_areas_;
    std::vector<rules::Participant> staged_enemies_;
    std::vector<opengold::BandMember> staged_band_;
    std::vector<opengold::CombatArt> staged_art_;
    // Monsters a script fields on the party's side, and their art.
    std::vector<rules::Participant> staged_allies_;
    std::vector<opengold::CombatArt> staged_ally_art_;
    std::vector<unsigned> staged_records_;
    // Pools of Darkness: the LOAD MONSTERs since the last fight that brought
    // undead, each a try at turning them (rules::Encounter::turning_tries).
    unsigned staged_undead_loads_{};
    // A Magic Circle kept the camp's attackers away: the rest completes.
    bool rest_warded_{};
    std::optional<EclRequest> encounter_menu_;
    unsigned encounter_distance_{};
    // The sides of the party SETUP MONSTER's monsters come from (Pools of
    // Darkness's and Treasures' fourth operand: bit 0 north, 1 east, 2 south,
    // 3 west), which the original keeps until the next SETUP MONSTER.
    unsigned monster_sides_{};
    std::optional<opengold::CampaignEncounter> encounter_;
    std::optional<unsigned> monster_picture_id_; // SETUP MONSTER's PIC record.
    // CALL 0x8001 before COMBAT: the member WHO chose fights alone.
    bool duel_{};
    // CALL 0x8000 before COMBAT: the training hall's arena duel, the member
    // WHO chose against a copy of itself.
    bool mirror_duel_{};
    // Stages that copy as the fight's one enemy.
    void stage_mirror();
    // Members a script made hostile (the record's byte 0x10C set to 0x81, as
    // a vote to join Tyranthraxus does): in the event's next COMBAT they fight
    // for the monsters, and afterwards they leave the party.
    std::set<MemberId> turncoats_;
    // Whether the member fights for the monsters: one a script made hostile,
    // or a companion the original makes hostile as it joins.
    [[nodiscard]] bool turncoat(MemberId id) const;

    // What the members who fought for the monsters leave once a fight ends:
    // each leaves the party, and a beaten one's gear and its record's
    // experience go to the victors.
    struct TurncoatLoot
    {
        unsigned defeated{}, experience{};
        std::vector<Equipment> items;
    };

    TurncoatLoot part_with_turncoats(const rules::Snapshot &result);
    // Whether Podol Plaza's script rolled a roaming group since the party
    // entered the area; a cancelled auction needs one (PODOL-1).
    bool plaza_group_rolled_{};
    void roll_plaza_group(const EclRequest &request, EclHostReply &reply);
    bool showing_monster_picture_{};
    std::uint64_t combat_request_{};

    // The ship fight's side of the session (buck_rogers_ship_fight.h).
    friend struct ShipFightAccess;
    unsigned encounter_challenge_{opengold::default_encounter_challenge};
    opengold::Difficulty difficulty_{opengold::default_difficulty};
    // Runtime option, like difficulty_: saves retain their existing layout.
    opengold::DifficultySetting difficulty_setting_{
        opengold::difficulty_setting(opengold::default_difficulty)};
    // The original encounter before fitting: its experience and loot are awarded.
    std::vector<unsigned> encounter_records_;
    // Doors forced open stay open until the district map is reloaded; the
    // event checkpoint keeps the map as the event found it.
    bool door_menu_{};
    // As in the original, one Pick per lock until the party moves again.
    bool pick_tried_{};
    std::vector<opengold::DoorMethod> door_choices_;
    void stage_treasure(const EclRequest &request);
    void award_staged_treasure(bool with_experience = true);
    [[nodiscard]] std::string treasure_identity(std::uint16_t address) const;
    [[nodiscard]] bool locked_door_ahead() const;
    // Whether the lock ahead is a wizard lock (door code 3).
    [[nodiscard]] bool wizard_locked_ahead() const;
    void show_locked_door();
    void try_locked_door(opengold::DoorMethod method);
    void search_destination();
    // The coins and items of an encounter's original creature records, less
    // the gear the monsters used up.
    [[nodiscard]] PendingLoot encounter_loot(unsigned area, std::vector<unsigned> records,
                                             std::string reward, bool items,
                                             std::vector<SpentGear> spent = {}) const;
    void show_encounter_menu();
    [[nodiscard]] std::vector<unsigned> living_member_levels() const;
    void fit_staged_encounter();
    void announce_spokesman();
    bool choose_encounter(std::size_t choice);
    [[nodiscard]] const PhlanResources &area_resources() const;
    // The override for this LOAD MONSTER of the current script, or null.
    [[nodiscard]] const EncounterOverride *
    encounter_override(const EclInstruction &instruction) const;
    // The export of the original creature `record` of `area`'s bank, if any.
    [[nodiscard]] const ExportedMonster *exported_creature(const PhlanResources &area,
                                                           unsigned record) const;
    void change_area(unsigned id);

    // Which script of an exploration event is running, so finish_event knows
    // what follows it.
    enum class EventStage : std::uint8_t
    {
        none, // No event: the tour itself.
        step, // Before the party steps forward.
        search,
        area_entry,
        pre_camp,
        camp_interrupted
    };

    EventStage event_stage_{EventStage::none};
    // The camp running is Treasures' Fix (camp_fix), not a rest.
    bool camp_fixing_{};

    // Treasures' romance scene at a camp (tell_romance): what is still to
    // come, in order, and the pair's members.
    struct RomanceStep
    {
        enum class Kind
        {
            text,
            proposal,
            npc_leaves,
            refused
        } kind{};
        unsigned entry{};
    };

    struct RomanceScene
    {
        std::vector<RomanceStep> steps;
        MemberId member{}, npc{};
    };

    std::optional<RomanceScene> romance_scene_;
    // The camp's scene is told: the camp goes on (camp_resumes_ as the
    // session next runs) without telling it again. A camp made from an open
    // menu goes on so too, its entry having run as the menu opened.
    bool romance_told_{}, camp_resumes_{};
    // The camp event running opens Treasures' camp menu (open_camp); the
    // menu is open, and the next camp event rests without running the entry;
    // the camp event running is that rest, made from the open menu.
    bool camp_opening_{}, camp_menu_open_{}, camp_from_menu_{};
    // The third member a scene names last (romance.pas, otherPartyMember),
    // which the original keeps only while it runs.
    MemberId romance_bystander_{};
    using Dice = CampService::Dice;
    bool start_camp(RestKind kind, std::optional<std::chrono::minutes> length);
    // The party is idle where it may camp, out of a fight and any rest.
    [[nodiscard]] bool can_camp() const;
    // The rest's encounter comes: the camp's interruption script.
    void interrupt_camp();
    void announce_camp_attack();
    [[nodiscard]] bool in_curse_wilderness() const;
    [[nodiscard]] bool in_champions_story_rest() const;
    [[nodiscard]] bool camp_warded() const;
    bool transition_{}, message_only_{};

    // Word of Recall: the square where the party last used a temple of New
    // Phlan this session, and the sanctuary it prepared (saved).
    struct Sanctuary
    {
        unsigned script{};
        PartyPose pose;
        auto operator<=>(const Sanctuary &) const = default;
    };

    std::optional<Sanctuary> temple_square_, sanctuary_;

    // Teleport's map and spot, waiting for the map's entry script to finish.
    struct TeleportArrival
    {
        unsigned map{};
        PartyPose pose;
    };

    std::optional<TeleportArrival> teleport_arrival_;
    // Puts the party on a Teleport's spot on its map.
    void arrive(const TeleportArrival &arrival);
    void enable_party_commands();
    // The encounter commands the host answers (SETUP MONSTER, LOAD MONSTER,
    // COMBAT and the rest), for a new machine or one a save restores.
    void enable_encounter_commands();
    // Another script takes over at `arrival`, and its entry runs.
    void enter_script(unsigned script, const TeleportArrival &arrival);
    // Whether this event searched: a Look, or a step in Search mode.
    bool event_searched_{};
    // Whether a data/shops location's map is the one the party is on.
    [[nodiscard]] bool on_map(const std::string &archive, unsigned record) const;
    // Changes the loaded map for the hidden entrances on it: the walls that
    // close each shop's square, the square's original event cleared, and the
    // secret door as a wall, a locked door once found, or an open one.
    void apply_entrances();
    // The shop whose secret door is on the edge ahead of the party, if any.
    [[nodiscard]] const opengold::ShopSite *entrance_ahead() const;
    // After a step or a Look: the rolls to find a hidden secret door the
    // party stands outside of, and OpenGoldBox's shop on its square opening.
    void search_for_secret_door();
    void configure_town();
    void synchronize_clock();
    // Moves the campaign clock on to a time a script instruction wrote into
    // the clock cells.
    void pass_time(std::chrono::milliseconds elapsed);
    bool adopt_script_clock();
    [[nodiscard]] EclHostReply clock_reply() const;

    // What the player did to start an event. Each value is the original script
    // entry slot that runs it.
    enum class EventTrigger : std::uint8_t
    {
        step = 0,
        look = 1,
        camp = 2
    };

    // `entry` starts another script slot than the trigger's, such as a town's entry.
    // `takes` is the time the command that starts the event takes (Look's
    // ten minutes), passed inside the event so a failed event takes it back.
    void begin_event(EventTrigger trigger, std::optional<std::size_t> entry = std::nullopt,
                     std::chrono::minutes takes = {});
    void finish_event();
    bool handle_town_host(const EclRequest &request);
    // Treasures' PROGRAM 3, Amanitas's word through his crown: the luck
    // reading of the next Lucky Paper mission (its ShowNextLuckText); and
    // PROGRAM 5, the party made whole (its RepairParty). Each waits for the
    // player to go on. vga_host.cpp.
    void treasures_program(const EclRequest &request, unsigned number);
    [[nodiscard]] std::string next_luck_reading();
    void repair_party();

    // Treasures' Fix (its CampFixMenu), PROGRAM 4 and the camp's: the party
    // rests while its casters, by their spell slots, cure what they can and
    // spend their healing on the wounded. What it did, and the minutes rested
    // before an interruption ended it, if one did. vga_host.cpp.
    struct FixOutcome
    {
        std::vector<opengold::rules::Message> words;
        std::optional<unsigned> interrupted_after;
    };

    // Treasures' romance (its romance.pas), vga_host.cpp: the pair its
    // scripts name and their loves, for a fight (TagRelationshipPair), or
    // nothing before a script names the NPC (cell 105h) and in the other games.
    [[nodiscard]] std::optional<opengold::rules::Romance> treasures_romance() const;
    // As a camp begins, after the area's camp script (UpdateRelationship):
    // when both lovers stand and a love has passed into another band of 25,
    // the scene for the two bands is told, the NPC may leave, and at the top
    // band of both it proposes. True while the scene waits for the player;
    // the camp goes on as it ends.
    bool tell_romance();
    // The scene's next text or the proposal, or the camp's going on when
    // nothing is left; choose_in_romance answers it.
    void show_romance_step();
    bool choose_in_romance(std::size_t choice);
    // A romance text (the LUCK file's entries 101 to 117) with the names and
    // pronouns of the people in it, in capitals; its pages apart.
    [[nodiscard]] std::string romance_text(unsigned entry);
    // The party's time passes; the caller brings the script's clock up to
    // date, by binding it or in its reply.
    [[nodiscard]] FixOutcome fix_party(const Dice &dice);
    // Secret's Fix (its own CampFixMenu): its memorized cures rolled, then
    // after the rest one roll for each cure slot, spent on the party in
    // order; the cures it counts applied after each of its two rests.
    [[nodiscard]] FixOutcome secret_fix_party(const Dice &dice);
    // One of the Fix's two rests: its minutes pass unless an interruption
    // roll ends it first (the minutes then rested).
    [[nodiscard]] std::optional<unsigned> fix_rest(unsigned minutes, const Dice &dice);
    // The 256-color engine's LOAD FILES: a map (below 7Fh, indoors only) and a
    // wall set (255 keeps the current one); vga_host.cpp.
    [[nodiscard]] EclHostReply vga_load_files(unsigned map, unsigned walls);
    // Its map loading for LOAD FILES and LOAD WALL SETS: the party's square's
    // wall and event in the new map, and the map's number.
    [[nodiscard]] EclHostReply vga_load_map(unsigned map);
    // The walls of the view's three slots, as the scripts load them.
    void vga_load_wall_slots(const std::array<unsigned, 3> &slots);
    // Treasures' LOAD WALL SETS: a map and the three slots' wall sets.
    [[nodiscard]] EclHostReply treasures_load_wall_sets(unsigned map,
                                                        const std::array<unsigned, 3> &sets);
    // Treasures' overland: its script set the overland cell (40h), and the
    // view shows the map's picture from above with the party's arrow.
    [[nodiscard]] bool treasures_overland() const;
    [[nodiscard]] Image treasures_overland_view() const;
    // Its PLAY SOUND: a sound effect, or music from 80h up.
    void vga_play_sound(unsigned sound);
    // A sound effect plays: it stops the later games' music (music.pas,
    // SendSoundCommand), the footstep too.
    void sound_effect_played();
    // Its scripts read the current character's record byte for byte from
    // 401h (docs/games/darkness.md, Script memory): the record's fields, last
    // in the reply so that they stand.
    void add_vga_record(PartySlot slot, EclHostReply &reply) const;
    // The Buck Rogers games' script host (buck_rogers_host.cpp;
    // docs/games/countdown.md, "Scripts"): the commands the party answers,
    // the commands their table changed or added, and the current
    // character's record as their scripts read it from 7C00h (eclmenu.pas
    // SpecialReadZ1), and as they change it (SpecialWriteZ1).
    void enable_buck_rogers_party_commands();
    // Matrix's PROGRAM 2 quits after its closing scene. Format 55 preserves
    // its validated terminal instruction position as a victory receipt.
    [[nodiscard]] bool matrix_ending_completed() const;
    [[nodiscard]] bool handle_buck_rogers_host(const EclRequest &request);
    void buck_rogers_damage(const EclRequest &request);
    // A new game's state (gameinit.pas InitializeGame, InitShipAndOrbit):
    // the difficulty, the area kind, the message speed and the team's ship,
    // the Maelstrom Rider, in zone 0 from 4D00h.
    void configure_buck_rogers();
    // The level the scripts read (C_Diff: Novice 0 to Ace 4, Admiral 5).
    [[nodiscard]] std::uint16_t buck_rogers_difficulty_level() const noexcept;
    // LOAD MONSTER: copies of a MON0CHA creature with its items, drawn with a
    // CPIC icon (eclvm.pas EclLoadMonster).
    void stage_buck_rogers_creatures(const EclRequest &request);
    // COMBAT and SOLO COMBAT: the fight on the arena the area's kind names,
    // or with no one hostile, the fight's end at once (eclvm.pas EclCombat).
    void buck_rogers_combat(const EclRequest &request, bool solo);
    // The fight's end (postcbt.pas PostCombat): the results, the wounded
    // and their treatment, experience, then the booty; the script goes on
    // once the booty is left.
    [[nodiscard]] bool resolve_buck_rogers_combat(const rules::Snapshot &result);
    void finish_buck_rogers_fight(std::uint64_t request, std::uint16_t combat_result,
                                  unsigned defeated);
    // After the fight on a boarded ship's decks (post-combat mode 2): the
    // repairs, the captured ship's fuel and the ship fight's experience.
    void repair_after_boarding(std::uint64_t request);

    // Whether the solar map is on screen, from the area kind the scripts set
    // and the PICTURE MODE they give, `picture_mode_given` when one is given
    // (TourSnapshot::solar_map).
    void update_solar_map(std::optional<std::uint16_t> picture_mode_given = std::nullopt);
    // The area kind update_solar_map last saw; none in a new game or a
    // loaded save.
    std::optional<std::uint16_t> solar_area_kind_seen_;
    // Writes the solar map's state into zone 0, and each body's cell from
    // 4B85h as the original's PaintOrbitMap does, for the scripts to compare.
    void keep_solar_map(const original::SolarMapState &state);
    // The rocket's step the explore() command asked for, unless a script
    // holds it (moveBlocked, 7EC9h, at FFh), and the team's three days.
    void solar_map_step();
    // The solar map as the original draws it: the backdrop, the Sun, each
    // body at its place and the rocket.
    [[nodiscard]] Image render_solar_map() const;
    void add_buck_rogers_record(PartySlot slot, EclHostReply &reply) const;
    void read_buck_rogers_character();
    // Pools of Darkness's to-hit cells, which its scripts set before a fight
    // and its end clears: cell 1 (monsterHitMod) is added to the monsters'
    // attack rolls, cell 2Fh (partyHitMod) to the party's side's
    // (fxengine.pas, AttackRoll; postcbt.pas).
    void add_script_to_hit();
    static constexpr EclAddress darkness_monster_hit_mod{0x01}, darkness_party_hit_mod{0x2F};
    // Pools of Darkness's cell 1Bh (breakLossPct): the encounter's morale,
    // which its monsters' side breaks below (combatai.pas, CheckMorale); and
    // cell 1Ch (partyMoveMod), added to the party's movement and cleared by
    // the fight's end (charhlp.pas, RecalcMovement; postcbt.pas).
    static constexpr EclAddress darkness_break_point{0x1B}, darkness_party_move_mod{0x1C};
    // Its item definition of this number, or for FFh a scroll of three
    // random magic-user spells drawn on the request's dice; nothing for an
    // empty definition.
    [[nodiscard]] std::optional<Equipment> vga_item(unsigned number, std::uint64_t request);
    // Its TREASURE: platinum, gems, jewelry and item definitions, handed out
    // as the other games' treasure is.
    void stage_vga_treasure(const EclRequest &request);
    // Its STORE: one of the program's eleven stores opens with its goods.
    void open_vga_store(const EclRequest &request);
    // Its SPELL: the first member with the spell memorized casts it; its place
    // in the member's list (0, or FFh for no one) into `place`, the member's
    // into `member`.
    [[nodiscard]] EclHostReply vga_spell(unsigned spell, EclAddress place, EclAddress member);
    // Its area map: outside a dungeon (cell 22h at 0) the party steps on a
    // map of 38 by 15 squares, in eight directions, its square in cells 25h
    // and 26h; a step takes half a day.
    [[nodiscard]] bool vga_area_map() const;
    void vga_area_step();
    // Pools of Darkness's party on its area map (darkness_area_map.cpp).
    [[nodiscard]] bool darkness_area_map() const;
    // The area map as the original draws it: the tile set's big picture with
    // the party's marker over its square.
    [[nodiscard]] Image render_darkness_area_map() const;
    // Shows the party on the area map as on a wilderness map: its square and
    // its last step's direction, so the view steps it in eight directions.
    void publish_darkness_area_square();
    // Its DAMAGE's forms, told in the dialogue.
    void vga_damage(const EclRequest &request);
    // Its encounter sprite's view at a distance (0 near, 2 far), in the
    // colors the sprite sets, its color 0 see-through.
    [[nodiscard]] ImageDecodeResult vga_sprite(unsigned id, unsigned distance) const;
    // Its FIND NPC: the named companion's place in the party into `place`.
    [[nodiscard]] EclHostReply vga_find_npc(const std::string &name, EclAddress place) const;
    // Scripts change coins relative to the view they were given, so the script
    // must receive a fresh character_reply after every read before it continues.
    void read_character();
    // Whether the script blanked the selected companion's record to send it away.
    [[nodiscard]] bool dismissed_companion() const;
    [[nodiscard]] std::uint16_t native_cell_flags(unsigned x, unsigned y) const;
    void bind_pose(PartyPose pose);
    bool move_party(ExplorationCommand command);
    void notice(std::string message);
    // Waits for Continue on what the snapshot shows, then answers the script's
    // `request` (a DAMAGE or a program's report).
    void await_continue(std::uint64_t request);
    // Each arrow attacks a random conscious active member; returns a report line for each.
    [[nodiscard]] std::string shoot_arrows(unsigned arrows, std::string_view missile,
                                           const rules::HazardAttack &arrow);
    // A DAMAGE call's report line for one member: the damage taken, or the
    // saving throw against `save_dc` that avoided it.
    [[nodiscard]] std::string hurt(const std::optional<HazardHit> &hit, int save_dc,
                                   unsigned sides) const;
    void publish_pose();
    void handle_host(const EclRequest &request);
    void fail(std::string diagnostic) override;
    // Set when a step leads to an area the game does not have yet: fail()
    // then rolls the step back with this notice instead of a diagnostic.
    std::string closed_way_;
    // The rune lines a PROTECTION scroll showed, kept above cleared text
    // until the next pause.
    std::string rune_scroll_;
};
} // namespace opengold::por
#endif
