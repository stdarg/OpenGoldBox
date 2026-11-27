#ifndef OPENGOLD_VGA_HOST_H
#define OPENGOLD_VGA_HOST_H

// The 256-color engine's script commands the tour answers where their
// numbers mean other commands in the other games (vga_host.cpp,
// darkness_area_map.cpp; docs/games/darkness.md, Scripts), with Treasures'
// overland, camp and romance. It reaches the tour through TourHost
// (tour_host.h), lent for each call.

#include "opengold/camp_service.h"
#include "opengold/campaign_party.h"
#include "opengold/ecl_machine.h"
#include "opengold/equipment.h"
#include "opengold/formats.h"
#include "opengold/message.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace opengold::por
{
struct PhlanResources;
class ShopService;
class TourHost;
class TreasureService;

class VgaHost
{
  public:
    using Dice = CampService::Dice;

    // Treasures' Fix (its CampFixMenu), PROGRAM 4 and the camp's: the party
    // rests while its casters, by their spell slots, cure what they can and
    // spend their healing on the wounded. What it did, and the minutes rested
    // before an interruption ended it, if one did.
    struct FixOutcome
    {
        std::vector<opengold::rules::Message> words;
        std::optional<unsigned> interrupted_after;
    };

    // Treasures' overland colors turn as time passes, a step each time the
    // map's color cycle's delay in hundredths of a second has passed (its
    // MenuCore.UpdateAnimation); true when they turned and the view is to be
    // drawn again.
    bool turn_colors(TourHost &tour, double seconds);
    // True while a Pools of Darkness script bars magic (its cell 2,
    // antiMagic): no one casts a spell, in a fight or in camp ("cannot cast
    // spells in this area", camp.pas, CanDoSpellActivity), and no one uses an
    // item (viewchar.pas, UseItemMenu).
    [[nodiscard]] bool magic_barred(const TourHost &tour) const;
    // Treasures' and Secret's camp menus (camp.pas, CampMainMenu). Fix camps
    // as a rest does, but the party rests only while its casters cure and
    // heal it (fix_party). Crown asks Amanitas, through the crown he gave, for the
    // next luck reading, while the party has it (cell 3Fh) and its last
    // reading has not been given (cell 25Ch at 2). False when the camp
    // cannot do so now.
    [[nodiscard]] bool camp_offers_fix(const TourHost &tour) const;
    [[nodiscard]] bool camp_offers_crown(const TourHost &tour) const;
    bool camp_crown(TourHost &tour);
    // The crystals the party holds, which Treasures' camp shows (cell 3Ch:
    // bit 0 red, 1 green, 2 blue); 0 in the other games.
    [[nodiscard]] unsigned camp_crystals(const TourHost &tour) const;

    // Treasures' PROGRAM 3, Amanitas's word through his crown: the luck
    // reading of the next Lucky Paper mission (its ShowNextLuckText); and
    // PROGRAM 5, the party made whole (its RepairParty). Each waits for the
    // player to go on.
    void treasures_program(TourHost &tour, const EclRequest &request, unsigned number);
    // The party's time passes; the caller brings the script's clock up to
    // date, by binding it or in its reply.
    [[nodiscard]] FixOutcome fix_party(TourHost &tour, const Dice &dice);

    // Treasures' romance (its romance.pas): the pair its scripts name and
    // their loves, for a fight (TagRelationshipPair), or nothing before a
    // script names the NPC (cell 105h) and in the other games.
    [[nodiscard]] std::optional<opengold::rules::Romance>
    treasures_romance(const TourHost &tour) const;
    // As a camp begins, after the area's camp script (UpdateRelationship):
    // when both lovers stand and a love has passed into another band of 25,
    // the scene for the two bands is told, the NPC may leave, and at the top
    // band of both it proposes. True while the scene waits for the player;
    // the camp goes on as it ends.
    bool tell_romance(TourHost &tour);

    [[nodiscard]] bool in_romance() const noexcept
    {
        return romance_scene_.has_value();
    }

    // Answers the scene's proposal, or goes on with its text.
    bool choose_in_romance(TourHost &tour, std::size_t choice);

    // The 256-color engine's LOAD FILES: a map (below 7Fh, indoors only) and a
    // wall set (255 keeps the current one).
    [[nodiscard]] EclHostReply vga_load_files(TourHost &tour, unsigned map, unsigned walls);
    // Treasures' LOAD WALL SETS: a map and the three slots' wall sets.
    [[nodiscard]] EclHostReply treasures_load_wall_sets(TourHost &tour, unsigned map,
                                                        const std::array<unsigned, 3> &sets);
    // Treasures' overland: its script set the overland cell (40h), and the
    // view shows the map's picture from above with the party's arrow.
    [[nodiscard]] bool treasures_overland(const TourHost &tour) const;
    [[nodiscard]] Image treasures_overland_view(const TourHost &tour) const;
    // Its PLAY SOUND: a sound effect, or music from 80h up.
    void vga_play_sound(TourHost &tour, unsigned sound);
    // Its scripts read the current character's record byte for byte from
    // 401h (docs/games/darkness.md, Script memory): the record's fields, last
    // in the reply so that they stand.
    void add_vga_record(const TourHost &tour, PartySlot slot, EclHostReply &reply) const;
    // Its TREASURE: platinum, gems, jewelry and item definitions, handed out
    // as the other games' treasure is.
    void stage_vga_treasure(TourHost &tour, const EclRequest &request);
    // Its STORE: one of the program's eleven stores opens with its goods.
    void open_vga_store(TourHost &tour, const EclRequest &request);
    // Its SPELL: the first member with the spell memorized casts it; its place
    // in the member's list (0, or FFh for no one) into `place`, the member's
    // into `member`.
    [[nodiscard]] EclHostReply vga_spell(TourHost &tour, unsigned spell, EclAddress place,
                                         EclAddress member);
    // Its area map: outside a dungeon (cell 22h at 0) the party steps on a
    // map of 38 by 15 squares, in eight directions, its square in cells 25h
    // and 26h; a step takes half a day.
    [[nodiscard]] bool vga_area_map(const TourHost &tour) const;
    void vga_area_step(TourHost &tour);
    // Pools of Darkness's party on its area map (darkness_area_map.cpp).
    [[nodiscard]] bool darkness_area_map(const TourHost &tour) const;
    // The area map as the original draws it: the tile set's big picture with
    // the party's marker over its square.
    [[nodiscard]] Image render_darkness_area_map(const TourHost &tour) const;
    // Shows the party on the area map as on a wilderness map: its square and
    // its last step's direction, so the view steps it in eight directions.
    void publish_darkness_area_square(TourHost &tour);
    // Its DAMAGE's forms, told in the dialogue.
    void vga_damage(TourHost &tour, const EclRequest &request);
    // Its encounter sprite's view at a distance (0 near, 2 far), in the
    // colors the sprite sets, its color 0 see-through.
    [[nodiscard]] static ImageDecodeResult vga_sprite(const TourHost &tour, unsigned id,
                                                      unsigned distance);
    // Its FIND NPC: the named companion's place in the party into `place`.
    [[nodiscard]] EclHostReply vga_find_npc(const TourHost &tour, const std::string &name,
                                            EclAddress place) const;

  private:
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

    [[nodiscard]] std::string next_luck_reading(TourHost &tour);
    void repair_party(TourHost &tour);
    // The scene's next text or the proposal, or the camp's going on when
    // nothing is left; choose_in_romance answers it.
    void show_romance_step(TourHost &tour);
    // A romance text (the LUCK file's entries 101 to 117) with the names and
    // pronouns of the people in it, in capitals; its pages apart.
    [[nodiscard]] std::string romance_text(TourHost &tour, unsigned entry);
    // Secret's Fix (its own CampFixMenu): its memorized cures rolled, then
    // after the rest one roll for each cure slot, spent on the party in
    // order; the cures it counts applied after each of its two rests.
    [[nodiscard]] FixOutcome secret_fix_party(TourHost &tour, const Dice &dice);
    // One of the Fix's two rests: its minutes pass unless an interruption
    // roll ends it first (the minutes then rested).
    [[nodiscard]] std::optional<unsigned> fix_rest(TourHost &tour, unsigned minutes,
                                                   const Dice &dice);
    // Its map loading for LOAD FILES and LOAD WALL SETS: the party's square's
    // wall and event in the new map, and the map's number.
    [[nodiscard]] EclHostReply vga_load_map(TourHost &tour, unsigned map);
    // The walls of the view's three slots, as the scripts load them.
    void vga_load_wall_slots(TourHost &tour, const std::array<unsigned, 3> &slots);
    // Its item definition of this number, or for FFh a scroll of three
    // random magic-user spells drawn on the request's dice; nothing for an
    // empty definition.
    [[nodiscard]] std::optional<Equipment> vga_item(TourHost &tour, unsigned number,
                                                    std::uint64_t request);

    // The 256-color engine's wall sets in the view's three slots, and
    // Treasures' overland picture its script loaded last.
    std::array<unsigned, 3> wall_slots_{};
    unsigned treasures_overland_picture_{};
    // How far Treasures' overland colors have turned, and the time since
    // their last turn (turn_colors).
    unsigned overland_color_steps_{};
    double overland_color_seconds_{};
    std::optional<RomanceScene> romance_scene_;
    // The third member a scene names last (romance.pas, otherPartyMember),
    // which the original keeps only while it runs.
    MemberId romance_bystander_{};
};
} // namespace opengold::por
#endif
