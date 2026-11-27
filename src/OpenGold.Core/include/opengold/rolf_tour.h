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
#include "opengold/tour_records.h"
#include "opengold/town_service_host.h"
#include "opengold/hall_service.h"
#include "opengold/temple_service.h"
#include "opengold/vault_service.h"
#include "opengold/shop_service.h"
#include "opengold/treasure_service.h"
#include "opengold/camp_service.h"
#include "opengold/ship_service.h"
#include "opengold/buck_rogers_menu_service.h"
#include "opengold/tour_host.h"
#include "opengold/encounter_host.h"
#include "opengold/vga_host.h"
#include "opengold/buck_rogers_host.h"
#include "opengold/door_host.h"
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

// The original builds sentences from pieces: "A BUGBEAR LEADING" then "A
// PARTY OF " printed one after the other, a monster name stored with a leading
// space. Its text window's wrapping hid the seams; on one line they read as
// "LEADINGA PARTY" and "OF  KOBOLDS". Joins `added` to `shown` with a space
// where two words would touch, and closes a doubled space after a word (two
// after a sentence's end are the original's style and stay).
[[nodiscard]] std::string join_script_text(std::string shown, std::string_view added);

// Original tour followed by the New Phlan movement/search/script scheduler.
// Bound VM position cells are authoritative; snapshots are read-only views.
class RolfTourSession final : private TourHost
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

    [[nodiscard]] bool can_leave() const override
    {
        return snapshot_.phase == TourPhase::completed;
    }

    // Word of Recall from camp (LEVELS-3): cast on the square where the party
    // used a temple of New Phlan, it prepares the party's sanctuary there;
    // elsewhere it takes the whole party back to that sanctuary. Throws when
    // it can do neither, before spending anything. `action` is its camp
    // action: "word_of_recall", or "word_of_recall_7" from a higher slot.
    void recall(opengold::MemberId caster, const std::string &action);

    [[nodiscard]] bool has_sanctuary() const;
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
    // While a Pools of Darkness script bars magic (VgaHost::magic_barred).
    [[nodiscard]] bool magic_barred() const override;

    // The Wand of Secrets (DoorHost::point_wand_of_secrets).
    [[nodiscard]] std::optional<WandSign> point_wand_of_secrets(opengold::MemberId member,
                                                                std::uint64_t item);

    // Auto-equip (EQUIP-1), from the game's settings: nothing for off, or
    // whether an unidentified magic item is put on. It applies to the items
    // bought and found from then on.
    void set_auto_equip(std::optional<opengold::UnidentifiedMagic> auto_equip) noexcept;
    // Outside events, and while shopping to choose the buyer or at a treasure
    // to choose who takes. The shop script receives whoever is selected when
    // the shop closes.
    [[nodiscard]] bool can_select_member() const;
    void advance(double seconds) override;
    // Treasures' overland colors turn (VgaHost::turn_colors).
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
    // The Buck Rogers games' solar map, the body the rocket orbits and the
    // team's ship (buck_rogers_host.h).
    [[nodiscard]] original::SolarMapState solar_map_state() const;
    [[nodiscard]] std::optional<std::size_t> orbited_body() const;
    [[nodiscard]] TeamShip team_ship() const;
    // The game clock as the scripts read it (tour_snapshot.h).
    [[nodiscard]] GameClock game_clock() const;
    // The ship fight itself, its files and what followed it are opened and
    // read through buck_rogers_ship_fight.h (ship_fight, ship_fight_files,
    // ship_fight_aftermath).
    // The Buck Rogers team's combat icons (BuckRogersHost::buck_rogers_team_art).
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
    void set_searching(bool searching) noexcept;
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
    // Treasures' and Secret's camp menus' Fix and Crown, and the crystals
    // Treasures' camp shows (VgaHost::camp_offers_fix).
    [[nodiscard]] bool camp_offers_fix() const;
    bool camp_fix();
    [[nodiscard]] bool camp_offers_crown() const;
    bool camp_crown();
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
    void difficulty(opengold::DifficultySetting setting) noexcept;
    void difficulty(opengold::Difficulty difficulty) noexcept;
    // The original's NPCs who join the party (data/companions) and the
    // character creation rules they are built with (Skullcrusher, CADORNA-1).
    // Without them, ADD NPC accepts only the profiles in the area's resources.
    void companion_rules(std::shared_ptr<const rules::CharacterRules> creation,
                         std::vector<CompanionDefinition> companions);
    // The temples data/shops describes (TEMPLE-1); a temple it does not name
    // offers the original's services and prices.
    void temples(std::vector<opengold::TempleSite> sites);
    [[nodiscard]] const std::vector<opengold::TempleSite> &temples() const noexcept;
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

    [[nodiscard]] const std::vector<opengold::ShopSite> &shops() const noexcept;
    // Every shop's items, in the order of shops(), at their prices (SELL-1).
    [[nodiscard]] const std::vector<std::vector<Equipment>> &town_stock() const noexcept;
    [[nodiscard]] const TourSnapshot &snapshot() const noexcept;

    // The areas as load() read them, for a tool that shows what the game reads
    // (the Creature Stats Editor). Null for synthetic resources.
    [[nodiscard]] std::shared_ptr<const PhlanResources> resources() const noexcept override
    {
        return town_;
    }

    [[nodiscard]] const GeoMap &map() const noexcept;
    [[nodiscard]] const std::array<opengold::Image, 3> &sprites() const noexcept;
    // The walls new art replaced shown in their original art (#274); the
    // view's setting, not saved with the game.
    void show_original_walls(bool original) noexcept;
    [[nodiscard]] const WallArtSet &wall_art() const noexcept;
    // Compose the current 3D view and remember its visible cells. Call only
    // when presenting exploration, not when an encounter picture replaces it.
    [[nodiscard]] Image observe_view();

    [[nodiscard]] const TownParty &party() const noexcept;
    [[nodiscard]] const std::vector<Equipment> &shop_stock() const noexcept;
    // The items of the treasure the party stands at (TourPhase::treasure).
    [[nodiscard]] const std::vector<Equipment> &treasure_pile() const noexcept;
    [[nodiscard]] const std::vector<std::string> &script_diagnostics() const noexcept;
    [[nodiscard]] const std::optional<Image> &picture() const noexcept;
    [[nodiscard]] std::uint16_t script_variable(EclAddress address) const;
    // Whether the member will fight for the monsters in the next fight, as a
    // script or a treacherous companion decided; the original shows its name
    // in yellow.
    [[nodiscard]] bool turns_on_party(MemberId id) const;
    // Combat waits until the player dismisses the monster close-up.
    [[nodiscard]] const std::optional<opengold::CampaignEncounter> &pending_encounter() const;
    // The monster close-up that loops before combat, or null when none is showing.
    [[nodiscard]] const std::vector<AnimationFrame> *monster_picture() const;
    // The frames of PIC record `id` as the game shows them, null for none:
    // Treasures draws the picture its cell 4Eh names in the colors of the
    // record its cell 4Fh names, laid over the picture's own (its portrait.pas,
    // LoadAnimation, which loads that record as it loads the picture).
    [[nodiscard]] const std::vector<AnimationFrame> *pic_frames(unsigned id) const override;
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
    // What the script hosts reach of it besides (tour_host.h).
    [[nodiscard]] const TourSnapshot &screen() const noexcept override;
    [[nodiscard]] unsigned current_script() const noexcept override;
    [[nodiscard]] unsigned current_area() const noexcept override;
    [[nodiscard]] GeoMap &loaded_map() noexcept override;
    [[nodiscard]] const GeoMap &loaded_map() const noexcept override;
    [[nodiscard]] WallArtSet &loaded_wall_art() noexcept override;
    [[nodiscard]] std::array<Image, 3> &encounter_sprites() noexcept override;
    [[nodiscard]] std::optional<Image> &shown_picture() noexcept override;
    [[nodiscard]] std::optional<unsigned> &shown_animation() noexcept override;
    [[nodiscard]] std::vector<EclMemoryWrite> &damage_writes() noexcept override;
    [[nodiscard]] const std::optional<ExplorationCommand> &
    pending_movement() const noexcept override;
    [[nodiscard]] DifficultySetting difficulty_setting() const noexcept override;
    [[nodiscard]] unsigned encounter_challenge() const noexcept override;
    [[nodiscard]] unsigned dark_queen_combat_set() const noexcept override;
    [[nodiscard]] unsigned travel_mode() const noexcept override;
    [[nodiscard]] TreasureService &treasure_service() noexcept override;
    [[nodiscard]] ShopService &shop_service() noexcept override;
    [[nodiscard]] CampService &camp_service() noexcept override;
    [[nodiscard]] BuckRogersMenuService &buck_rogers_menus() noexcept override;
    [[nodiscard]] ShipService &ship_service() noexcept override;
    [[nodiscard]] bool party_resting() const noexcept override;
    [[nodiscard]] bool event_searched() const noexcept override;
    void romance_told() noexcept override;
    void keep_event() noexcept override;
    void sound_effect_played() override;
    [[nodiscard]] bool vga_area_map() const override;
    [[nodiscard]] std::optional<rules::Romance> treasures_romance() const override;
    // The town services, each the session's to forward to.
    HallService hall_service_;
    TempleService temple_service_;
    VaultService vault_service_;
    ShopService shop_service_;
    TreasureService treasure_service_;
    CampService camp_service_;
    ShipService ship_service_;
    BuckRogersMenuService buck_rogers_menus_;
    // The script hosts and the doors, each the session's to call.
    EncounterHost encounters_;
    VgaHost vga_;
    BuckRogersHost buck_rogers_;
    DoorHost doors_;
    std::vector<PartySlot> who_slots_;
    // Whether this event's script asked which member acts (WHO).
    bool member_chosen_{};
    std::optional<opengold::UnidentifiedMagic> auto_equip_;
    std::optional<Image> picture_;
    std::optional<unsigned> picture_animation_id_; // The PIC record picture_ begins.

    mutable std::optional<RecolouredPicture> recoloured_picture_;

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

    void update_travel_mode();
    // Steps the party one wilderness square in its direction, unless the
    // terrain stops it; false when it stays.
    bool overland_step();
    // Gateway's overland: its script cleared the in-dungeon cell (4BE6h), and
    // the map is drawn from above with symbols rather than walls.
    [[nodiscard]] bool gateway_overland() const override;
    // The open squares ahead of the party where monsters may stand, at most 2.
    [[nodiscard]] unsigned room_ahead() const override;
    // A LOAD PIECES on Gateway's overland: the three symbol sets that draw
    // the map's cells, each into its slot of 70 symbols.
    void load_overland_symbols(const std::array<unsigned, 3> &banks);
    WallTiles overland_symbols_;
    // Native LOAD FILES' second operand selects DUNGCOM/WILDCOM, independently
    // of the exploration map's three wall sets.
    unsigned dark_queen_combat_set_{};
    std::map<unsigned, MapKnowledge> visited_areas_;
    std::map<unsigned, MapKnowledge> seen_areas_;
    // A Magic Circle kept the camp's attackers away: the rest completes.
    bool rest_warded_{};
    [[nodiscard]] bool turncoat(MemberId id) const override;
    // Whether Podol Plaza's script rolled a roaming group since the party
    // entered the area; a cancelled auction needs one (PODOL-1).
    bool plaza_group_rolled_{};
    void roll_plaza_group(const EclRequest &request, EclHostReply &reply);

    // The ship fight's side of the session (buck_rogers_ship_fight.h).
    friend struct ShipFightAccess;
    unsigned encounter_challenge_{opengold::default_encounter_challenge};
    opengold::Difficulty difficulty_{opengold::default_difficulty};
    // Runtime option, like difficulty_: saves retain their existing layout.
    opengold::DifficultySetting difficulty_setting_{
        opengold::difficulty_setting(opengold::default_difficulty)};
    void stage_treasure(const EclRequest &request);
    void award_staged_treasure(bool with_experience = true) override;
    [[nodiscard]] std::string treasure_identity(std::uint16_t address) const override;
    void search_destination();
    // The coins and items of an encounter's original creature records, less
    // the gear the monsters used up.
    [[nodiscard]] PendingLoot encounter_loot(unsigned area, std::vector<unsigned> records,
                                             std::string reward, bool items,
                                             std::vector<SpentGear> spent = {}) const override;
    void announce_spokesman();
    [[nodiscard]] const PhlanResources &area_resources() const override;
    void change_area(unsigned id) override;

    EventStage event_stage_{EventStage::none};
    // The camp running is Treasures' Fix (camp_fix), not a rest.
    bool camp_fixing_{};

    // The camp's scene is told: the camp goes on (camp_resumes_ as the
    // session next runs) without telling it again. A camp made from an open
    // menu goes on so too, its entry having run as the menu opened.
    bool romance_told_{}, camp_resumes_{};
    // The camp event running opens Treasures' camp menu (open_camp); the
    // menu is open, and the next camp event rests without running the entry;
    // the camp event running is that rest, made from the open menu.
    bool camp_opening_{}, camp_menu_open_{}, camp_from_menu_{};
    bool start_camp(RestKind kind, std::optional<std::chrono::minutes> length);
    // The party is idle where it may camp, out of a fight and any rest.
    [[nodiscard]] bool can_camp() const;
    // The rest's encounter comes: the camp's interruption script.
    void interrupt_camp();
    void announce_camp_attack() override;
    [[nodiscard]] bool in_curse_wilderness() const override;
    [[nodiscard]] bool in_champions_story_rest() const;
    [[nodiscard]] bool camp_warded() const;
    bool transition_{}, message_only_{};

    // Word of Recall: the square where the party last used a temple of New
    // Phlan this session, and the sanctuary it prepared (saved).
    std::optional<Sanctuary> temple_square_, sanctuary_;

    // Teleport's map and spot, waiting for the map's entry script to finish.
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
    [[nodiscard]] bool on_map(const std::string &archive, unsigned record) const override;
    void configure_town();
    void synchronize_clock() override;
    // Moves the campaign clock on to a time a script instruction wrote into
    // the clock cells.
    void pass_time(std::chrono::milliseconds elapsed) override;
    bool adopt_script_clock();
    [[nodiscard]] EclHostReply clock_reply() const override;

    // `entry` starts another script slot than the trigger's, such as a town's entry.
    // `takes` is the time the command that starts the event takes (Look's
    // ten minutes), passed inside the event so a failed event takes it back.
    void begin_event(EventTrigger trigger, std::optional<std::size_t> entry = std::nullopt,
                     std::chrono::minutes takes = {});
    void finish_event();
    bool handle_town_host(const EclRequest &request) override;
    // Matrix's ending is done (BuckRogersHost::matrix_ending_completed).
    [[nodiscard]] bool matrix_ending_completed() const;
    // Scripts change coins relative to the view they were given, so the script
    // must receive a fresh character_reply after every read before it continues.
    void read_character() override;
    // Whether the script blanked the selected companion's record to send it away.
    [[nodiscard]] bool dismissed_companion() const;
    [[nodiscard]] std::uint16_t native_cell_flags(unsigned x, unsigned y) const;
    void bind_pose(PartyPose pose);
    bool move_party(ExplorationCommand command) override;
    void notice(std::string message);
    // Waits for Continue on what the snapshot shows, then answers the script's
    // `request` (a DAMAGE or a program's report).
    void await_continue(std::uint64_t request) override;
    // Each arrow attacks a random conscious active member; returns a report line for each.
    [[nodiscard]] std::string shoot_arrows(unsigned arrows, std::string_view missile,
                                           const rules::HazardAttack &arrow) override;
    // A DAMAGE call's report line for one member: the damage taken, or the
    // saving throw against `save_dc` that avoided it.
    [[nodiscard]] std::string hurt(const std::optional<HazardHit> &hit, int save_dc,
                                   unsigned sides) const override;
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
