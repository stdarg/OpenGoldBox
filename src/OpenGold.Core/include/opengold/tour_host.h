#ifndef OPENGOLD_TOUR_HOST_H
#define OPENGOLD_TOUR_HOST_H

// What the tour's script hosts (encounter_host.h, vga_host.h,
// buck_rogers_host.h) and its doors (door_host.h) reach of the tour they
// serve, beyond what a town service does (town_service_host.h).
// RolfTourSession implements it and lends itself to a host for one call at a
// time, as it does to its services.

#include "opengold/campaign_encounter.h"
#include "opengold/formats.h"
#include "opengold/geo_map.h"
#include "opengold/town_service_host.h"
#include "opengold/treasure_service.h"
#include "opengold/wall_art.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opengold::por
{
class BuckRogersMenuService;
class CampService;
class ShipService;
class ShopService;

class TourHost : public TownServiceHost
{
  public:
    using TownServiceHost::screen;
    [[nodiscard]] virtual const TourSnapshot &screen() const noexcept = 0;
    // The areas as load() read them; null for synthetic resources.
    [[nodiscard]] virtual std::shared_ptr<const PhlanResources> resources() const noexcept = 0;
    // The areas of the map the party is on (RolfTourSession::area_resources).
    [[nodiscard]] virtual const PhlanResources &area_resources() const = 0;
    // The party is idle between events, where it may leave.
    [[nodiscard]] virtual bool can_leave() const = 0;
    // The script running, and the district (0 the town) whose map is loaded.
    [[nodiscard]] virtual unsigned current_script() const noexcept = 0;
    [[nodiscard]] virtual unsigned current_area() const noexcept = 0;
    // The loaded map, its walls' art, the encounter sprite's three distances,
    // and the picture a script shows with the PIC record it begins.
    [[nodiscard]] virtual GeoMap &loaded_map() noexcept = 0;
    [[nodiscard]] virtual const GeoMap &loaded_map() const noexcept = 0;
    [[nodiscard]] virtual WallArtSet &loaded_wall_art() noexcept = 0;
    [[nodiscard]] virtual std::array<Image, 3> &encounter_sprites() noexcept = 0;
    [[nodiscard]] virtual std::optional<Image> &shown_picture() noexcept = 0;
    [[nodiscard]] virtual std::optional<unsigned> &shown_animation() noexcept = 0;
    // What the script hears as the words on screen are acknowledged
    // (await_continue).
    [[nodiscard]] virtual std::vector<EclMemoryWrite> &damage_writes() noexcept = 0;
    // The step or turn the player asked for, while its events run.
    [[nodiscard]] virtual const std::optional<ExplorationCommand> &
    pending_movement() const noexcept = 0;
    // The game's settings: the difficulty chosen, the encounter challenge.
    [[nodiscard]] virtual DifficultySetting difficulty_setting() const noexcept = 0;
    [[nodiscard]] virtual unsigned encounter_challenge() const noexcept = 0;
    // Dark Queen's combat tile set, which its LOAD FILES chooses.
    [[nodiscard]] virtual unsigned dark_queen_combat_set() const noexcept = 0;
    // The original's travel mode: 1 in a 3D area, 2-4 on the wilderness map.
    [[nodiscard]] virtual unsigned travel_mode() const noexcept = 0;
    // The town services the hosts hand the party to.
    [[nodiscard]] virtual TreasureService &treasure_service() noexcept = 0;
    [[nodiscard]] virtual ShopService &shop_service() noexcept = 0;
    [[nodiscard]] virtual CampService &camp_service() noexcept = 0;
    [[nodiscard]] virtual BuckRogersMenuService &buck_rogers_menus() noexcept = 0;
    [[nodiscard]] virtual ShipService &ship_service() noexcept = 0;

    // The tour's own steps the hosts take (rolf_tour.h says what each does).
    virtual void change_area(unsigned id) = 0;
    virtual void pass_time(std::chrono::milliseconds elapsed) = 0;
    virtual void synchronize_clock() = 0;
    [[nodiscard]] virtual EclHostReply clock_reply() const = 0;
    virtual void read_character() = 0;
    virtual void await_continue(std::uint64_t request) = 0;
    [[nodiscard]] virtual std::string shoot_arrows(unsigned arrows, std::string_view missile,
                                                   const rules::HazardAttack &arrow) = 0;
    [[nodiscard]] virtual std::string hurt(const std::optional<HazardHit> &hit, int save_dc,
                                           unsigned sides) const = 0;
    [[nodiscard]] virtual const std::vector<AnimationFrame> *pic_frames(unsigned id) const = 0;
    [[nodiscard]] virtual std::string treasure_identity(std::uint16_t address) const = 0;
    virtual void award_staged_treasure(bool with_experience = true) = 0;
    [[nodiscard]] virtual PendingLoot encounter_loot(unsigned area, std::vector<unsigned> records,
                                                     std::string reward, bool items,
                                                     std::vector<SpentGear> spent = {}) const = 0;
    [[nodiscard]] virtual unsigned room_ahead() const = 0;
    [[nodiscard]] virtual bool gateway_overland() const = 0;
    [[nodiscard]] virtual bool in_curse_wilderness() const = 0;
    // The rest's encounter interrupts a camp (EventStage::camp_interrupted).
    [[nodiscard]] virtual bool party_resting() const noexcept = 0;
    // This event searched: a Look, or a step in Search mode.
    [[nodiscard]] virtual bool event_searched() const noexcept = 0;
    // Whether a data/shops location's map is the one the party is on.
    [[nodiscard]] virtual bool on_map(const std::string &archive, unsigned record) const = 0;
    // The party steps, turns or looks; false when the way is blocked.
    virtual bool move_party(ExplorationCommand command) = 0;
    virtual void announce_camp_attack() = 0;
    // Treasures' romance scene is told: the camp goes on without telling it
    // again.
    virtual void romance_told() noexcept = 0;
    // A won fight's results stand: a later fault rolls back no further.
    virtual void keep_event() noexcept = 0;
    virtual void sound_effect_played() = 0;
    // Whether the member fights for the monsters (EncounterHost::turncoat).
    [[nodiscard]] virtual bool turncoat(MemberId id) const = 0;
    [[nodiscard]] virtual bool handle_town_host(const EclRequest &request) = 0;
    // What the 256-color engine's host answers for the other hosts
    // (vga_host.h).
    [[nodiscard]] virtual bool vga_area_map() const = 0;
    [[nodiscard]] virtual bool magic_barred() const = 0;
    [[nodiscard]] virtual std::optional<rules::Romance> treasures_romance() const = 0;

    // Whether the party's rules fight the original's records as they are.
    [[nodiscard]] bool plays_original_records() const
    {
        return campaign() && campaign()->rule_module().plays_original_records();
    }

    // On the wilderness map, where the party steps in eight directions.
    [[nodiscard]] bool overland() const noexcept
    {
        return travel_mode() > 1;
    }

  protected:
    TourHost() = default;
    TourHost(const TourHost &) = default;
    TourHost(TourHost &&) = default;
    TourHost &operator=(const TourHost &) = default;
    TourHost &operator=(TourHost &&) = default;
    ~TourHost() = default;
};
} // namespace opengold::por
#endif
