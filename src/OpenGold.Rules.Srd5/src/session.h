#ifndef OPENGOLD_SRD5_SESSION_H
#define OPENGOLD_SRD5_SESSION_H
#include "actor_model.h"
#include "armor.h"
#include "character_definition.h"
#include "combat_geometry.h"
#include "combat_grid.h"
#include "conditions.h"
#include "definition.h"
#include "equipment_rules.h"
#include "recovery_timeline.h"
#include "resource_pools.h"
#include "sneak_attack.h"
#include "spell_access.h"
#include "spell_costs.h"
#include "spell_traits.h"
#include "thrown_gear.h"
#include "weapon_mastery.h"
#include "opengold/scroll_key.h"
#include "opengold/srd5.h"
#include <istream>
#include <iterator>
#include <stdexcept>

// One SRD fight: the actors on the board, their turns, commands and effects.
// The declaration keeps its small bodies; the rest live in the themed
// session_*.cpp files.
namespace opengold::srd5::detail
{
using rules::Battlefield;
using rules::CombatSession;
using rules::Encounter;
using rules::HeldItemView;
using rules::LogEntry;
using rules::MessageArgument;
using rules::OptionalEffectChoice;
using rules::Outcome;
using rules::Side;
using rules::Snapshot;
using rules::TemporaryHitPoints;
using rules::Terrain;

class Session final : public CombatSession
{
  public:
    Session(std::shared_ptr<const Content> content, Encounter encounter, std::uint64_t seed,
            bool restoring = false)
        : content_(std::move(content)), board_(std::move(encounter.battlefield)), rng_(seed),
          scope_(encounter.scope), morale_(encounter.morale),
          spares_the_party_(encounter.spares_the_party), daylight_(encounter.daylight),
          gold_(encounter.party_gold)
    {
        if (!scope_)
        {
            throw std::runtime_error("Invalid encounter scope");
        }
        if (morale_ > 100 ||
            std::any_of(encounter.participants.begin(), encounter.participants.end(),
                        [](const auto &p)
                        {
                            return p.morale > 100 || p.intelligence > 30;
                        }))
        {
            throw std::runtime_error("Invalid encounter morale");
        }
        detail::validate_battlefield(board_);
        // A restored fight may have one creature left on the field, the rest
        // having run off it.
        if (encounter.participants.size() < (restoring ? 1u : 2u) ||
            encounter.participants.size() > 64)
        {
            throw std::runtime_error("Invalid encounter size");
        }
        // Room for every participant and the creatures spells bring in, so a
        // summoning never moves the actors others hold.
        actors_.reserve(128);
        std::set<EntityId> ids;
        std::set<Cell> cells;
        std::set<Side> sides;
        for (auto &p : encounter.participants)
        {
            // Every square of its footprint is on the field and open, and
            // none is another's (FOOT-1).
            const bool sized = p.footprint.width >= 1 && p.footprint.width <= 3 &&
                               p.footprint.height >= 1 && p.footprint.height <= 3;
            const auto squares = sized ? squares_of(p.cell, p.footprint) : std::vector<Cell>{};
            const bool placed =
                sized && std::all_of(squares.begin(), squares.end(),
                                     [&](Cell square)
                                     {
                                         return board_.at(square) != Terrain::obstacle &&
                                                (cells.insert(square).second || restoring);
                                     });
            if (!p.id || !ids.insert(p.id).second || !placed || p.side > Side::opposition ||
                p.name.empty() || p.name.size() > 160 ||
                (p.character_profile.empty() && !content_->definitions.contains(p.definition)))
            {
                throw std::runtime_error("Invalid participant or unsupported rules definition: " +
                                         p.definition);
            }
            sides.insert(p.side);
            Actor a;
            a.definition = p.character_profile.empty() ? fielded(*content_, p)
                                                       : character_definition(p.character_profile);
            const auto &d = a.definition;
            a.source = std::move(p);
            a.life.hp = d.hp;
            a.winds = d.winds;
            a.slots = d.slots;
            a.hit_dice = d.hit_die ? d.level : 0;
            a.rushes = d.rushes;
            a.surges = surge_capacity(d);
            a.arcane = arcane_capacity(d);
            a.lay_on_hands = lay_capacity(d);
            a.free_casts = free_cast_capacity(d);
            a.channel_divinity = channel_capacity(d);
            a.initiate_casts = initiate_capacity(d);
            a.facing_left = a.source.facing_left;
            if (a.source.state)
            {
                restore_vitals(a, *a.source.state);
            }
            // A Druid still in a Beast form from earlier fights in that form.
            refresh_form(a);
            a.initiative =
                detail::d20({d.champion || d.feral_instinct,
                             d.str_dex_disadvantage || a.source.surprised || suffers_poisoned(a) ||
                                 (d.sunlight_sensitive && open_sky(a.source.cell)),
                             d.lucky},
                            rng_) +
                d.initiative - return_penalty(a);
            a.movement = a.form ? a.form->speed : d.speed;
            actors_.push_back(std::move(a));
        }
        if (sides.size() != 2 && !restoring)
        {
            throw std::runtime_error("Encounter needs both sides");
        }
        // Fixed tie adjudication: descending initiative, then stable entity ID.
        std::stable_sort(actors_.begin(), actors_.end(),
                         [](const Actor &a, const Actor &b)
                         {
                             return a.initiative != b.initiative ? a.initiative > b.initiative
                                                                 : a.source.id < b.source.id;
                         });
        log("Combat begins. Each square is 5 feet.");
        if (!restoring)
        {
            wake_resting_participants();
        }
        // Contingency: a spell waiting for the fight to start takes effect.
        if (!restoring)
        {
            for (auto &a : actors_)
            {
                if (detail::has_effect(a.effects, detail::EffectKind::contingency))
                {
                    for (const auto &e : std::vector<detail::Effect>(a.effects.active))
                    {
                        if (e.kind == detail::EffectKind::contingency && e.dc % 4 == 1)
                        {
                            fire_contingency(a);
                        }
                    }
                }
            }
        }
        update_outcome();
        frost_movement_ = std::any_of(
            actors_.begin(), actors_.end(),
            [](const auto &a)
            {
                return a.definition.cunning ||
                       (detail::knows_spell(a.definition.known_cantrips, "ray_of_frost")) ||
                       detail::speed_penalty(a.effects);
            });
        if (!restoring)
        {
            for (const auto &a : actors_)
            {
                for (const auto &key : a.definition.equipment_keys)
                {
                    if (const auto *w = detail::weapon(key); w && (w->thrown || w->light))
                    {
                        physical_inventory_ = true;
                    }
                }
                for (const auto &item : a.source.inventory)
                {
                    if (const auto *w = detail::weapon(item.definition);
                        w && (w->thrown || w->light))
                    {
                        physical_inventory_ = true;
                    }
                }
            }
            // Thrown gear is counted once, from what each character carries in.
            for (auto &a : actors_)
            {
                for (const auto &item : a.source.inventory)
                {
                    if (const auto *gear = detail::thrown_gear(item.definition))
                    {
                        a.thrown_gear_left[static_cast<std::size_t>(gear->effect)] += item.quantity;
                    }
                    if (detail::magic_item(item.definition) || item.definition == basic_poison)
                    {
                        a.item_uses.emplace_back(item.inventory_id, item.quantity);
                    }
                    // A scroll's uses are a bit for each spell still on it
                    // (MAGIC-6).
                    else if (const auto scroll = rules::parse_scroll_key(item.definition))
                    {
                        a.item_uses.emplace_back(item.inventory_id,
                                                 (1U << scroll->spells.size()) - 1);
                    }
                }
            }
            for (const auto &a : actors_)
            {
                if (a.definition.other_weapon)
                {
                    physical_inventory_ = true;
                }
            }
            if (physical_inventory_)
            {
                initialize_items();
            }
        }
        if (!restoring && std::any_of(actors_.begin(), actors_.end(),
                                      [](const auto &a)
                                      {
                                          return a.definition.two_weapon_fighting;
                                      }))
        {
            activate_light();
        }
        if (!restoring)
        {
            if (outcome_ == Outcome::ongoing)
            {
                for (const auto &a : actors_)
                {
                    if (a.source.side == Side::party &&
                        (def(a).alert || metabolism_ready(a) || persistent_ready(a)) &&
                        conscious(a))
                    {
                        initiative_choices_.push_back(a.source.id);
                    }
                }
            }
            if (initiative_choices_.empty())
            {
                start_encounter_turns();
            }
        }
    }

    Snapshot snapshot() const override;
    std::vector<Command> legal_commands() const override;
    [[nodiscard]] std::vector<Command> offered_commands() const;
    std::vector<Cell> movement_reach(EntityId actor) const override;
    bool submit(const Command &command) override;
    void turn_against_party(const std::vector<EntityId> &turning) override;
    // Records the enemy a command on the actor's own turn aims at.
    void remember_target(const Command &command);
    // What the computer weighs in the spell as the actor casts it (#266).
    [[nodiscard]] rules::SpellFacts spell_facts(const Actor &a,
                                                const detail::SpellDef &spell) const;
    std::string save() const override;
    static std::unique_ptr<Session> restore(std::shared_ptr<const Content> content,
                                            std::string_view bytes);

  private:
    std::shared_ptr<const Content> content_;
    Battlefield board_;
    std::vector<Actor> actors_;
    std::uint64_t rng_{}, revision_{1};
    // The dice rolled since the last line of the fight's log, while a command
    // is applied.
    std::vector<RolledDie> rolled_dice_;
    bool attack_dice_held_{};
    std::uint64_t scope_{1}, elapsed_ms_{};
    unsigned turn_{}, round_{1};
    // The spells cancelled during the turn these name. Not saved: after a load the
    // computer at worst tries one once more.
    std::vector<std::string> given_up_spells_;
    unsigned given_up_turn_{}, given_up_round_{};
    void give_up_spell(const std::string &verb);
    Outcome outcome_{Outcome::ongoing};
    std::optional<TemporaryHitPoints> temporary_offer_;
    std::optional<PendingCheck> check_choice_;
    std::optional<ChampionMove> champion_move_;
    std::optional<PendingGraze> graze_;
    // The damage type a Pact of the Blade attack being resolved deals instead
    // of its weapon's; set only during that attack.
    std::optional<detail::DamageType> pact_damage_;
    // Whether the attack being resolved is an Opportunity Attack; set only
    // during it (Escape the Horde).
    bool opportunity_{};
    // The type of the damage most recently resolved, for Undead Fortitude.
    detail::DamageType last_damage_type_{};

    // Tactical Master (Fighter 9): the mastery the attacker uses for one attack
    // instead of its weapon's own.
    struct TacticalMastery
    {
        EntityId actor{};
        detail::Mastery kind{};
    };

    std::optional<TacticalMastery> tactical_mastery_;

    // Brutal Strike (Barbarian 9): the attack it is chosen for, and its effect.
    struct BrutalStrike
    {
        EntityId actor{};
        BrutalBlow blow{};
    };

    std::optional<BrutalStrike> brutal_;

    // Cunning Strike (Rogue 5): the effect chosen for one attack, paid with a
    // Sneak Attack die when the attack deals Sneak Attack damage.
    struct PendingCunning
    {
        EntityId actor{};
        CunningStrike kind{};
        std::optional<CunningStrike> second; // Improved Cunning Strike

        [[nodiscard]] bool has(CunningStrike wanted) const
        {
            return kind == wanted || second == wanted;
        }

        [[nodiscard]] int cost() const
        {
            return cunning_option(kind).cost + (second ? cunning_option(*second).cost : 0);
        }
    };

    std::optional<PendingCunning> cunning_;
    // Stunning Strike (Monk 5): the attack it is tried with, if it hits.
    EntityId stunning_{};
    std::optional<PendingMastery> mastery_;

    // Creatures chosen so far for a spell that affects several (CLASS-2).
    struct PendingSelection
    {
        EntityId caster{};
        std::string verb;
        std::vector<EntityId> chosen;
    };

    std::optional<PendingSelection> selection_;

    // An area spell being aimed (CLASS-5); nothing is spent until it is cast.
    struct PendingArea
    {
        EntityId caster{};
        std::string verb;
        Cell center;
        // Fire Storm: the corners of the 10-foot cubes placed so far, each on
        // even coordinates (LEVELS-3).
        std::vector<Cell> cubes;
        // Dimension Door: the willing creature that goes too, if any.
        EntityId companion{};
    };

    std::optional<PendingArea> area_;
    // A spell's lasting area, which lasts as long as its caster's Concentration
    // or, for Grease, a minute: Entangle's plants, Grease and Web are Difficult
    // Terrain, Fog Cloud is Heavily Obscured.
    enum class ZoneKind : unsigned
    {
        plants,
        fog,
        silence,
        spiritual_weapon, // one square: where the spectral force floats
        grease,
        web,
        flaming_sphere, // one square: where the fire burns
        gust,           // Gust of Wind's line
        darkness,       // magical Darkness, Heavily Obscured except to Devil's Sight
        moonbeam,       // Moonbeam's beam; cells.front() is its centre
        spikes,         // Spike Growth's Difficult Terrain
        guardian,       // one square: Guardian of Faith's guardian
        circle,         // Magic Circle, held by Concentration while it is cast
        animals,        // Conjure Animals' Large pack of spirits, two squares by two
        wind_wall,      // Wind Wall's squares
        overgrowth,     // Plant Growth's Overgrowth, lasting the fight
        beings,         // Conjure Woodland Beings' 10-foot Emanation, around its caster
        guardians,      // Spirit Guardians' 15-foot Emanation, around its caster
        raising,        // the corpses Animate Dead or Create Undead is being cast on
        swarm,          // Insect Plague's swarm, Difficult Terrain
        blades,         // Blade Barrier's wall or ring, Difficult Terrain
        sunbeam,        // Sunbeam's mote above its caster, while it lasts
        pillar,         // Conjure Celestial's pillar of light; cells.front() is its centre
        quake,          // Earthquake's tremor, Difficult Terrain
        holy,           // Holy Aura's 30-foot Emanation, around its caster
        antimagic,      // Antimagic Field's 10-foot Emanation, around its caster
        sleet,          // Sleet Storm, Heavily Obscured and Difficult Terrain
        stench,         // Stinking Cloud, Heavily Obscured
        tentacles,      // Black Tentacles, Difficult Terrain
        hail,           // Ice Storm's hailstones, Difficult Terrain until it ends_ms
        fire_wall,      // Wall of Fire's squares, opaque
        fire_side,      // the squares within 10 feet of Wall of Fire's burning side
        hound,          // one square: Faithful Hound's watchdog, until it ends_ms
        elementals,     // Conjure Minor Elementals' 15-foot Emanation, around its caster
        hand,           // one square: Arcane Hand's hand
        cloud,          // Cloudkill's fog; cells.front() is its centre
        elemental,      // Conjure Elemental's Large spirit, two squares by two
        force_wall,     // Wall of Force, which nothing passes
        stone_wall,     // Wall of Stone, which blocks movement, sight and effects
        globe,          // Globe of Invulnerability's still 10-foot sphere
        ice_wall,       // Wall of Ice, which blocks movement and effects
        sword,          // one square: Arcane Sword's sword
        bead,           // Delayed Blast Fireball's bead; cells.front() is its point
        cage,           // Forcecage's cage of bars, which creatures cannot cross
        box,            // Forcecage's solid box, which nothing crosses
        embers,         // Incendiary Cloud; cells.front() is its centre
        storm,          // Call Lightning's cloud; cells.front() is the square under its centre
        antilife,       // Antilife Shell's 10-foot Emanation, around its caster
        fey,            // one square: Conjure Fey's spirit
        thorns,         // Wall of Thorns, 4 feet of movement a foot, opaque
        sanctuary,      // Nature's Sanctuary's spectral trees, until it ends_ms
        // Tsunami's wall of water: `slot` the d10s of its next move, `dealt`
        // its heading, an index into wave_headings.
        wave
    };

    struct Zone
    {
        EntityId caster{};
        ZoneKind kind{};
        std::vector<Cell> cells;
        std::uint64_t ends_ms{}; // combat time it vanishes; 0 while held by Concentration
        // Grease outlasts a caster who runs off the field, so a zone keeps the
        // DC its creatures save against rather than looking the caster up.
        int save_dc{};
        // Guardian of Faith's damage so far, which it vanishes at 60, or the
        // turns Delayed Blast Fireball's bead has waited.
        int dealt{};
        unsigned warded{}; // Magic Circle's creature type, an index into ward_types
        unsigned slot{};   // the slot level a zone whose damage grows with it was cast at
    };

    std::vector<Zone> zones_;

    // walled_board()'s last results, without and with the walls only force
    // stops; a session is used from one thread at a time (Item 16).
    struct WalledBoard
    {
        std::vector<Cell> walls;
        Battlefield board;
    };

    mutable std::array<std::optional<WalledBoard>, 2> walled_boards_;
    std::shared_ptr<ProfileDefinitions> profiles_ = std::make_shared<ProfileDefinitions>();
    std::vector<ChampionMove> champion_offers_;
    std::optional<EffectReaction> effect_reaction_origin_;
    // Reactions to a hit (Shield, Deflect Attacks) land deep inside a command.
    // The command is undone at that moment and the creature asked; its answer
    // replays the command with the same dice.
    enum class Asked : unsigned
    {
        hit,       // an attack roll hit: Shield or Deflect Attacks
        missile,   // targeted by Magic Missile: Shield
        redirect,  // Deflect Attacks stopped all the damage: redirect it for 1 Focus
        rebuke,    // an attack damaged a Warlock: Hellish Rebuke
        boon,      // a Goliath's attack roll hit and dealt damage: Fire's Burn and so on
        endure,    // an attack is about to damage a Goliath: Stone's Endurance
        thunder,   // an attack damaged a Goliath: Storm's Thunder
        retaliate, // a creature within 5 feet damaged a Berserker: Retaliation
        counter    // a foe within 60 feet is casting a spell: Counterspell
    };

    struct ReactionQuestion
    {
        EntityId target{};
        Asked asked{};
        bool deflectable{}; // the hit deals Bludgeoning, Piercing or Slashing damage
        bool critical{};    // a hit Shield cannot turn aside
        // The hit's attack roll total and the AC it hit, shown with the
        // question so Shield is chosen knowing whether +5 turns it (#266).
        int total{}, armor_class{};
    };

    // A creature's answer during the command being replayed: "shield",
    // "deflect", "redirect", "rebuke", "giant_boon", "stones_endurance",
    // "storms_thunder" or "decline".
    struct ReactionAnswer
    {
        EntityId target{};
        Asked asked{};
        std::string verb;
    };

    struct PendingReaction
    {
        ReactionQuestion question;
        Command command;
        std::vector<ReactionAnswer> answers;
    };

    std::optional<PendingReaction> reaction_prompt_;
    std::vector<ReactionAnswer> reaction_answers_;

    // Creatures damaged during the command, each with the creature acting then
    // (the one whose turn it is, or one making an Opportunity Attack). Hellish
    // Rebuke and Storm's Thunder answer them after the attack or the command.
    struct DamageTaken
    {
        EntityId target{}, source{};
    };

    std::vector<DamageTaken> damage_taken_;
    void answer_damage();
    // True while perform() can undo the command to ask a question; damage
    // from turns ended outside it (a creature fled or fell) asks nothing.
    bool askable_{};
    // While perform() carries out a command, whose reactions it can ask; turns
    // ended outside one take no Legendary Actions after them.
    bool performing_{};
    // While a spell is cast: Magic Resistance gives Advantage on its saves.
    bool against_magic_{};
    // The save in progress avoids or ends Poisoned (Dwarven Resilience).
    bool against_poisoned_{};
    [[nodiscard]] const ReactionAnswer *answer_of(EntityId target, Asked asked) const;
    [[nodiscard]] bool can_shield(const Actor &target) const;
    [[nodiscard]] bool can_deflect(const Actor &target, bool deflectable) const;
    [[nodiscard]] bool can_dodge(const Actor &target) const;
    [[nodiscard]] bool can_defend(const Actor &target) const;
    // Uncanny Dodge (Rogue 5): the hit's damage halved, when the Rogue chose it.
    int dodged(Actor &target, int amount);
    void ask_reaction(const Actor &target, Asked asked, bool deflectable = false,
                      bool critical = false, int total = 0, int armor_class = 0) const;
    int deflected(const Actor &attacker, Actor &target, int amount, detail::DamageType type,
                  bool ranged);
    [[nodiscard]] bool can_rebuke(const Actor &warlock, const Actor &attacker) const;
    // Giant Ancestry's Reactions and on-hit boons, each asked before use.
    [[nodiscard]] bool can_use_giant_reaction(const Actor &goliath, GiantBoon boon) const;
    void giant_boon(const Actor &attacker, Actor &target, bool critical);
    [[nodiscard]] int endured(Actor &goliath, int amount);
    void storms_thunder(const Actor &attacker, Actor &goliath);
    // Bardic Inspiration's die added to a roll that failed by `shortfall`.
    int inspiration_roll(const Actor &creature, int shortfall);
    void log_inspiration(const Actor &inspired, int die, bool success);
    // Cutting Words' die taken from an attack roll that hit, or an ability
    // check that succeeded, by `margin`.
    enum class CutRoll : unsigned
    {
        attack,
        check
    };
    int cutting_words(const Actor &roller, int margin, CutRoll cut = CutRoll::attack);
    // Cutting Words' die taken from a damage roll of `amount` that would drop
    // `target`, on the Bard's side, to 0 Hit Points (CUT-1).
    int cut_damage(const Actor &roller, const Actor &target, int amount, detail::DamageType type);
    // A Lore Bard who may use Cutting Words against the roller, or none.
    Actor *cutting_bard(const Actor &roller);
    void bless_fiends(const Actor &fallen);
    void rebuke(const Actor &attacker, Actor &warlock);
    void clouds_jaunt(Actor &goliath, Cell destination);
    void retaliate(const Actor &attacker, Actor &berserker);
    void perform(const Command &command);
    void dispatch(const Command &command);
    void answer_reaction(const Command &command);

    [[nodiscard]] bool effect_waiting() const
    {
        return mastery_.has_value() || !champion_offers_.empty();
    }

    // The statistics of the attack that earned the mastery, with the weapon it
    // was made with.
    [[nodiscard]] Definition mastery_definition(const PendingMastery &) const;
    [[nodiscard]] std::vector<EntityId> cleave_targets(const PendingMastery &,
                                                       std::optional<Cell> from = {}) const;
    [[nodiscard]] std::vector<Cell> push_cells(const PendingMastery &,
                                               std::optional<Cell> from = {}) const;
    [[nodiscard]] bool mastery_available(const PendingMastery &) const;
    [[nodiscard]] OptionalEffectChoice effect_choices() const;
    bool use_effect(const Command &);
    void finish_effects();
    // Clears the rest of the mover's route when a reaction left it illegal: a
    // creature now stands on it, or a bite knocked the mover Prone and it must
    // crawl farther than its movement allows. The mover stops where it is,
    // keeping the movement it has left. True when it stopped.
    bool stop_illegal_route();
    void validate_mastery_state() const;
    void offer_mastery(const Actor &, const Actor &, int natural, AttackRange range, int damage,
                       bool critical);
    void validate_graze() const;

    [[nodiscard]] int graze_damage(const Actor &a, const Actor &target) const
    {
        const int cap = std::max(0, def(a).melee_ability);
        const std::array parts{detail::DamagePart{def(a).melee_type, cap}};
        std::vector<detail::DamageAffinity> added;
        const auto all = affinities(target, added);
        std::vector<detail::DamageAffinity> defenses(all.begin(), all.end());
        // The approved Graze policy permits reductions, never vulnerability increases.
        std::erase_if(defenses,
                      [](const auto &affinity)
                      {
                          return affinity.kind == detail::AffinityKind::vulnerability;
                      });
        return std::min(cap, detail::resolve_damage(parts, defenses).total);
    }

    void finish_champion_move();
    void validate_champion_move() const;
    void finish_check(const PendingCheck &check, int boost);
    void validate_check() const;
    bool frost_movement_{}, items_active_{}, physical_inventory_{}, light_active_{}, nick_active_{};
    [[nodiscard]] unsigned held_weapon(const Actor &) const;
    void activate_light();
    [[nodiscard]] bool light_eligible(const Actor &, unsigned token) const;
    [[nodiscard]] bool has_nick(const Actor &) const;
    [[nodiscard]] bool nick_weapon(const Actor &, unsigned token) const;
    void qualify_light(Actor &, unsigned token);
    [[nodiscard]] Loadout item_loadout(const Actor &, unsigned token) const;
    [[nodiscard]] Loadout unarmed_loadout(const Actor &a) const;
    void use_focus_movement(Actor &a, std::string_view verb, EntityId carried = 0);
    void use_font_of_magic(Actor &a, std::string_view verb);
    void ready_metamagic(Actor &a, std::string_view choice);
    void open_hand(const Actor &monk, Actor &target, std::string_view verb);
    [[nodiscard]] bool weapon_reaction(const Actor &, const Definition &) const;
    // Whether `a` could make an Opportunity Attack on a creature of `size`
    // leaving its reach from `from` to `to`.
    [[nodiscard]] bool has_weapon_reaction(const Actor &a, Cell from, Cell to,
                                           Footprint size) const;
    void validate_light() const;
    [[nodiscard]] Loadout thrown_loadout(const Actor &, std::string_view weapon) const;
    void throw_weapon(Actor &, Actor &, unsigned token, bool light = false);
    // Whether a character carries an item it does not already wield.
    [[nodiscard]] bool carries(const Actor &, std::string_view key) const;
    void torch_attack(Actor &, Actor &);
    // A ranged weapon a character carries while wielding no ranged weapon, and
    // has the hands for; empty when there is none.
    [[nodiscard]] std::string_view carried_ranged_weapon(const Actor &) const;
    [[nodiscard]] std::string weapon_in_hand(const Actor &) const;
    void take_up_weapon(Actor &, const Command &) const;
    [[nodiscard]] rules::Missile ranged_missile(const Actor &) const;
    // Whether taking off its shield would free a character to draw a carried
    // two-handed bow or crossbow.
    [[nodiscard]] bool shield_blocks_bow(const Actor &) const;
    // Fleeing the fight (the original's rule): a party member runs off the edge
    // of the field. Faster than every conscious enemy, it gets away; as fast as
    // the fastest, it has an even chance; slower than any, it cannot leave.
    [[nodiscard]] int current_speed(const Actor &) const;
    [[nodiscard]] bool can_run_off(const Actor &) const;
    [[nodiscard]] std::vector<Cell> off_field_steps(const Actor &) const;
    // Whether `a` with its top-left square at `anchor` has a square off the
    // field: a step there runs off it (FOOT-1).
    [[nodiscard]] bool off_field(const Actor &a, Cell anchor) const;
    void run_off_field(Actor &);
    [[nodiscard]] bool flight_always_succeeds(const Actor &) const;
    void spare_the_party();
    void remove_fled();

    // A member who got away: out of the fight, back with the party after it.
    struct Departed
    {
        Participant source;
        int hp{}, max_hp{}, armor_class{};
        VitalState vitals;
        std::vector<std::pair<std::string, unsigned>> thrown_gear_left;
        std::vector<std::pair<std::uint64_t, unsigned>> item_uses;
        bool surrendered{};
    };

    std::vector<Departed> fled_;
    // Morale (the original's rule): checked at the start of each turn of a
    // creature the rules control. The encounter's morale is set by its script.
    unsigned morale_{100};
    bool spares_the_party_{};
    bool daylight_{}; // Encounter::daylight
    bool allies_attacked_{};
    // What the party purse is worth in gold, and what offerings have taken of it.
    unsigned gold_{}, offerings_gold_{};
    // A party member who has just died: Revivify may bring it back for a minute.
    void fall(Actor &a);
    void revivify(Actor &cleric, const Command &command);
    void check_morale(Actor &);
    void note_flight(Actor &);
    [[nodiscard]] int squares_to_edge(const Actor &) const;
    void throw_gear(Actor &, Actor &, const detail::ThrownGear &);
    std::vector<HeldItemView> items_;
    void initialize_items();
    // The actor's statistics with what it holds, `selected` its weapon in
    // hand (its own selection when 0).
    [[nodiscard]] Definition equipped_definition(const Actor &a,
                                                 const std::vector<HeldItemView> &items,
                                                 unsigned selected = 0) const;
    std::vector<LogEntry> log_;
    std::vector<Cell> path_;
    std::size_t path_index_{};
    std::vector<EntityId> reactors_;
    std::size_t reactor_index_{};

    // A creature moving outside its own turn (Dissonant Whispers' flight),
    // with the feet it has left; path_ and reactors_ then follow it.
    struct Flight
    {
        EntityId actor{};
        int remaining{};
    };

    std::optional<Flight> flight_;

    // The creature path_ moves: the fleeing one, or the one whose turn it is.
    Actor &mover()
    {
        return flight_ ? actor(flight_->actor) : actors_[turn_];
    }

    const Actor &mover() const
    {
        return flight_ ? actor(flight_->actor) : actors_[turn_];
    }

    [[nodiscard]] int mover_budget(const Actor &m) const
    {
        return flight_ ? flight_->remaining : movement_left(m);
    }

    const Definition &def(const Actor &a) const
    {
        return a.form ? *a.form : a.definition;
    }

    // A temporary's definition would dangle (Effective Modern C++ Item 11).
    const Definition &def(Actor &&) const = delete;

    const Actor &actor(EntityId id) const
    {
        const auto found = std::find_if(actors_.begin(), actors_.end(),
                                        [&](const auto &a)
                                        {
                                            return a.source.id == id;
                                        });
        if (found == actors_.end())
        {
            throw std::logic_error("No combatant has id " + std::to_string(id));
        }
        return *found;
    }

    Actor &actor(EntityId id)
    {
        return const_cast<Actor &>(std::as_const(*this).actor(id));
    }

    int roll(int sides)
    {
        return roll_die(rng_, sides);
    }

    int dice(Dice d, bool critical = false)
    {
        damage_dice_ = d.count * (critical ? 2 : 1);
        return detail::roll_damage(rng_, d, critical);
    }

    void log(std::string english, Message message = {})
    {
        if (message.source.empty())
        {
            message.source = english;
        }
        // A line that reports damage shows the dice rolled since the line
        // before it, after the text (Message::detail; the English line stays as
        // it was); a d20 already shows in its own line. An attack gathers its
        // damage dice itself (attack_dice_held_).
        if (!attack_dice_held_)
        {
            if (message.detail.empty() &&
                std::any_of(message.arguments.begin(), message.arguments.end(),
                            [](const MessageArgument &argument)
                            {
                                return argument.name == "damage";
                            }))
            {
                std::vector<RolledDie> damage_dice;
                std::copy_if(rolled_dice_.begin(), rolled_dice_.end(),
                             std::back_inserter(damage_dice),
                             [](const RolledDie &die)
                             {
                                 return die.sides != 20;
                             });
                message.detail = dice_detail(damage_dice);
            }
            rolled_dice_.clear();
        }
        if (log_.size() == 80)
        {
            log_.erase(log_.begin());
        }
        log_.push_back({std::move(english), std::move(message)});
    }

    // A Wall of Stone blocks sight; a Wall of Force or of Stone, every effect.
    [[nodiscard]] bool line_of_sight(Cell a, Cell b) const
    {
        return walled(false) ? detail::has_line_of_sight(walled_board(false), a, b)
                             : detail::has_line_of_sight(board_, a, b);
    }

    [[nodiscard]] bool line_of_effect(Cell a, Cell b) const
    {
        return walled(true) ? detail::has_line_of_sight(walled_board(true), a, b)
                            : line_of_sight(a, b);
    }

    // From any square of one creature to any of another's, or to a square.
    [[nodiscard]] bool line_of_sight(const Actor &a, const Actor &b) const
    {
        return lines_between(a, squares_of(b), false);
    }

    [[nodiscard]] bool line_of_sight(const Actor &a, Cell cell) const
    {
        return lines_between(a, {cell}, false);
    }

    [[nodiscard]] bool line_of_effect(const Actor &a, const Actor &b) const
    {
        return lines_between(a, squares_of(b), true);
    }

    [[nodiscard]] bool lines_between(const Actor &a, const std::vector<Cell> &targets,
                                     bool effect) const
    {
        for (const auto from : squares_of(a))
        {
            for (const auto to : targets)
            {
                if (effect ? line_of_effect(from, to) : line_of_sight(from, to))
                {
                    return true;
                }
            }
        }
        return false;
    }

    [[nodiscard]] bool walled(bool force) const
    {
        return std::any_of(zones_.begin(), zones_.end(),
                           [&](const auto &zone)
                           {
                               return zone.kind == ZoneKind::stone_wall ||
                                      (force && (zone.kind == ZoneKind::force_wall ||
                                                 zone.kind == ZoneKind::ice_wall));
                           });
    }

    // The board with the walls standing now, built again only when they
    // change: line of sight asks for it many times a command. The board
    // itself never changes during a fight.
    const Battlefield &walled_board(bool force) const
    {
        std::vector<Cell> walls;
        for (const auto &zone : zones_)
        {
            if (zone.kind == ZoneKind::stone_wall ||
                (force && (zone.kind == ZoneKind::force_wall || zone.kind == ZoneKind::ice_wall)))
            {
                walls.insert(walls.end(), zone.cells.begin(), zone.cells.end());
            }
        }
        auto &cached = walled_boards_[force ? 1 : 0];
        if (!cached || cached->walls != walls)
        {
            auto board = board_;
            for (const auto cell : walls)
            {
                board.terrain[static_cast<std::size_t>(cell.y * board.width + cell.x)] =
                    Terrain::obstacle;
            }
            cached = WalledBoard{std::move(walls), std::move(board)};
        }
        return cached->board;
    }

    // Heavily Obscured squares block sight into and out of them.
    [[nodiscard]] bool clear_view(const Actor &a, const Actor &b) const
    {
        if (!conscious(a) || detail::blinded(a.effects))
        {
            return false;
        }
        for (const auto from : squares_of(a))
        {
            for (const auto to : squares_of(b))
            {
                if (!obscured(a, from) && !obscured(a, to) && line_of_sight(from, to))
                {
                    return true;
                }
            }
        }
        return false;
    }

    [[nodiscard]] bool can_see(const Actor &a, const Actor &b) const
    {
        return clear_view(a, b) && !unseen(a, b);
    }

    // An Invisible creature hides from all but those with See Invisibility; a
    // hidden one has the Invisible condition until it is found.
    static bool unseen(const Actor &viewer, const Actor &target)
    {
        // Faerie Fire, Starry Wisp and Shining Smite keep a creature from
        // being Invisible.
        // Witch Sight: Truesight to 30 feet.
        return (invisible(target) || hidden(target)) &&
               !detail::has_effect(viewer.effects, detail::EffectKind::see_invisibility) &&
               !detail::has_effect(viewer.effects, detail::EffectKind::true_seeing) &&
               !(viewer.definition.witch_sight && distance(viewer, target) <= 30) &&
               !detail::has_effect(target.effects, detail::EffectKind::outlined) &&
               !detail::has_effect(target.effects, detail::EffectKind::lit) &&
               !detail::has_effect(target.effects, detail::EffectKind::shining);
    }

    [[nodiscard]] static bool hidden(const Actor &a)
    {
        return detail::has_effect(a.effects, detail::EffectKind::hidden);
    }

    // Hide (SRD 5.2.1, HIDE-1): out of every enemy's line of sight, or Heavily
    // Obscured from it, a DC 15 Dexterity (Stealth) check hides the creature;
    // the total is the DC to find it.
    [[nodiscard]] bool can_hide(const Actor &a) const;
    void hide(Actor &a);
    // The Search action: a Wisdom (Perception) check finds each hidden enemy
    // in clear view whose Stealth total it reaches.
    void search(Actor &searcher);
    // An enemy in clear view whose Passive Perception reaches a hidden
    // creature's Stealth total finds it (HIDE-1).
    void notice_hidden();
    void find_hidden(Actor &hider, const Actor &finder);
    // Hiding ends after its creature makes an attack roll, casts any spell
    // (HIDE-1) or makes a loud noise, such as a Breath Weapon.
    void end_hiding(EntityId id);

    // Invisibility ends right after its creature casts a spell, once any attack
    // roll the spell makes has had the benefit; casting Invisibility on oneself
    // starts it rather than ending it, so callers check before the spell. The
    // end is an ordinary call after the action rather than a destructor's work,
    // so an exception (a reaction question undoing the action) skips it and
    // nothing can throw from a destructor (Effective C++ Item 8).
    [[nodiscard]] static bool invisible(const Actor &a) noexcept;
    void end_invisibility(EntityId id);
    // The Barbarian's Rage effect, if it is raging; borrowed from its effects.
    [[nodiscard]] static const detail::Effect *rage_of(const Actor &a) noexcept;
    [[nodiscard]] unsigned rage_duration(const Actor &a) const;
    void extend_rage(Actor &a);

    [[nodiscard]] bool enemy_in_sight(const Actor &a) const
    {
        return std::any_of(actors_.begin(), actors_.end(),
                           [&](const auto &other)
                           {
                               return other.source.side != a.source.side && conscious(other) &&
                                      can_see(a, other);
                           });
    }

    // An actor's place in the turn order, found by id: a reference may be to a
    // copy (thrown weapons attack with one), so its address says nothing
    // (Effective C++ Item 27).
    [[nodiscard]] std::size_t index_of(EntityId id) const
    {
        const auto found = std::find_if(actors_.begin(), actors_.end(),
                                        [&](const auto &a)
                                        {
                                            return a.source.id == id;
                                        });
        if (found == actors_.end())
        {
            throw std::logic_error("No combatant has id " + std::to_string(id));
        }
        return static_cast<std::size_t>(found - actors_.begin());
    }

    [[nodiscard]] unsigned turn_end_ms(std::size_t index) const
    {
        return static_cast<unsigned>((index + 1) * detail::round_ms / actors_.size());
    }

    [[nodiscard]] bool shares_occupied_space(const Actor &who) const
    {
        return std::any_of(actors_.begin(), actors_.end(),
                           [&](const auto &other)
                           {
                               return other.source.id != who.source.id && !other.life.dead &&
                                      distance(other, who) == 0;
                           });
    }

    void clear_departed_overlaps()
    {
        for (auto &a : actors_)
        {
            if (a.life.dead || !shares_occupied_space(a))
            {
                a.involuntary_overlap = false;
            }
        }
    }

    // Emits every table spell whose row matches `scope` and pass. Called once
    // per original offer position so the observable order is unchanged.
    void offer_spells(std::vector<Command> &commands, const Actor &a, const Actor &other, int feet,
                      detail::SpellTarget scope, bool bonus_pass) const;
    // Resolves one table spell. `verb` is the offered command: it names the
    // "_2" level-two slot form and Command's option.
    void resolve_spell(const detail::SpellDef &spell, std::string_view verb, Actor &a,
                       EntityId target_id);
    // Darts striking one creature together, as one hit (Magic Missile).
    // `split`: other creatures take darts too, so the log names this one.
    // One spell attack roll and what follows it: a single-target attack
    // spell, or one beam of Eldritch Blast.
    void strike_with_spell(const detail::SpellDef &spell, std::string_view verb, Actor &a,
                           Actor &target, const Dice &rolled, unsigned steps, int dc);
    void missile_hits(Actor &a, Actor &target, const detail::SpellDef &spell, const Dice &rolled,
                      unsigned darts, bool split);
    // `targets` holds one creature, or for a split spell one per dart or ray.
    void cast_resolved_spell(const detail::SpellDef &spell, std::string_view verb, Actor &a,
                             std::span<const EntityId> targets);
    void apply_rider(const detail::SpellDef &spell, std::string_view verb, Actor &a, Actor &target,
                     int dc);
    // Command's Approach and Flee: the commanded creature only moves toward or
    // away from the caster, then ends its turn.
    void obey_command(std::vector<Command> &commands, const Actor &a) const;
    // Compulsion: the caster names a direction (an index into compass) that each
    // creature its Compulsion charms moves on its next turn.
    void name_compulsion_heading(const Actor &caster, std::size_t direction);
    // While a named direction waits, a compelled creature's only move is the
    // one taking it furthest that way.
    void obey_compulsion(std::vector<Command> &commands, const Actor &a) const;
    // After moving the named way, the creature saves again; a success ends the
    // charm on it. The direction is spent either way.
    void finish_compulsion(Actor &a);
    [[nodiscard]] bool marked_by(const Actor &target, const Actor &caster) const;
    // Whether the caster's Concentration holds a Hunter's Mark on a creature.
    [[nodiscard]] bool holds_hunters_mark(const Actor &caster) const;
    void reveal_lore(const Actor &caster, const Actor &target);
    void resolve_ensnaring_strike(Actor &a, std::string_view verb);
    void divine_spark(Actor &cleric, Actor &target, bool harm);
    [[nodiscard]] int disciple_of_life(const Actor &caster, unsigned slot_level) const;
    void blessed_healer(Actor &cleric, const detail::SpellDef &spell, std::string_view verb,
                        std::span<const EntityId> healed);
    void preserve_life(Actor &cleric);
    void turn_undead(Actor &cleric);
    void flee_turning(std::vector<Command> &commands, const Actor &a) const;
    void hold_still(std::vector<Command> &commands, const Actor &a) const;
    // `verb` names the spell's "_2" form, which a sphere grows with.
    // `origin` is the caster's square, from which cones and cubes extend.
    [[nodiscard]] std::vector<Cell> area_cells(const detail::SpellDef &spell, std::string_view verb,
                                               Cell origin, Cell center) const;
    // The area's squares on an open field, before walls block any of them.
    [[nodiscard]] std::vector<Cell> area_shape(const detail::SpellDef &spell, std::string_view verb,
                                               Cell origin, Cell center) const;
    // Whether `viewer` cannot see into the square.
    [[nodiscard]] bool obscured(const Actor &viewer, Cell cell) const;
    // Inside Silence: no Thunder damage.
    [[nodiscard]] bool silenced(Cell cell) const;
    [[nodiscard]] Cell spectral_cell(const Actor &target, Cell from) const;
    [[nodiscard]] const Zone *spiritual_weapon(const Actor &caster) const;
    [[nodiscard]] Cell default_area_center(const Actor &caster,
                                           const detail::SpellDef &spell) const;
    void aim_area(const Command &command);
    void damage_area(Actor &caster, const detail::SpellDef &spell, std::string_view verb,
                     const std::vector<Cell> &cells, int dc);
    [[nodiscard]] int area_dc(const Actor &caster, const detail::SpellDef &spell) const;
    void push_away(const Actor &from, Actor &target, int squares);
    void push_along(Actor &target, Cell step, int squares);
    void move_wave(EntityId caster);
    [[nodiscard]] bool held_by_wave(Actor &a);
    // A pushed creature stopped by a wall, an obstacle or another creature with
    // `short_feet` of its push left takes 1d6 Bludgeoning for each 10 feet or
    // part of it, up to 3d6 (PUSH-1).
    void slam(Actor &target, int short_feet);
    // Whether a wall, an obstacle or another creature stands in `cell` (on the board).
    [[nodiscard]] bool blocks_push(Cell cell, EntityId pushed) const;
    // Whether `other` may make an Opportunity Attack on `moving` stepping from
    // `from` to `to`.
    [[nodiscard]] bool provokes(const Actor &other, const Actor &moving, Cell from, Cell to) const;
    // Dissonant Whispers' flight from `from` within `budget` feet: the
    // farthest square, by the route that provokes fewest Opportunity Attacks.
    [[nodiscard]] std::vector<Cell> flight_route(const Actor &from, const Actor &target,
                                                 int budget) const;
    void ice_burst(Actor &caster, const Actor &target, unsigned steps);
    void condition_area(Actor &caster, const detail::SpellDef &spell,
                        const std::vector<Cell> &cells);
    // An unoccupied space the caster can see, for a creature of `size` there.
    [[nodiscard]] bool teleport_open(const Actor &caster, Cell cell, Footprint size) const;
    // Every square of `size` at `anchor` is open, unguarded and holds no
    // creature but `mover` (FOOT-1).
    [[nodiscard]] bool room_for(const Actor *mover, Footprint size, Cell anchor) const;
    // Whether `mover` cannot stand with its top-left square at `anchor`: one of
    // its squares is off `board` or an obstacle there, or holds another
    // creature (FOOT-1).
    [[nodiscard]] bool blocked_for(const Actor &mover, Cell anchor, const Battlefield &board) const;
    [[nodiscard]] static bool in_zone(const Zone &zone, Cell cell) noexcept;
    // Any of the creature's squares is in the zone (FOOT-1).
    [[nodiscard]] static bool in_zone(const Zone &zone, const Actor &a);
    // Makes a creature in a Grease or Web zone save; true when it is caught.
    bool spring_zone(const Zone &zone, Actor &creature);
    void spring_zones(Actor &creature, ZoneKind kind);
    void blow(const Zone &line, Actor &creature);
    // Flaming Sphere burns a creature ending its turn within 5 feet of it.
    void burn_beside_spheres(Actor &creature);
    void sphere_burns(const Zone &sphere, Actor &creature);
    [[nodiscard]] std::vector<Cell> beam_cells(Cell center) const;
    // A save against the beam's Radiant damage, once per turn.
    void moonbeam_burns(const Zone &beam, Actor &creature);
    // Moonbeam's beam moves onto a creature, burning those it reaches.
    void move_moonbeam(Actor &caster, const Actor &onto);
    // Conjure Animals: whether a square is within 10 feet of the pack, the
    // pack's squares beside a creature within 30 feet of where it is, and
    // its move there, which strikes those it comes near.
    [[nodiscard]] static bool near_pack(const Zone &pack, Cell cell);
    [[nodiscard]] static bool near_pack(const Zone &pack, const Actor &a);
    // Conjure Woodland Beings and Spirit Guardians: whether a square is within
    // the Emanation around their caster.
    [[nodiscard]] bool in_emanation(const Zone &zone, Cell cell) const;
    // Any of the creature's squares is near or in it (FOOT-1).
    [[nodiscard]] bool in_emanation(const Zone &zone, const Actor &a) const;
    [[nodiscard]] static bool emanates(const Zone &zone);
    // The spirits of Conjure Animals, Conjure Woodland Beings or Spirit
    // Guardians strike a creature that comes near or ends its turn there: a
    // save, once a turn.
    void spirits_strike(const Zone &zone, Actor &creature);
    [[nodiscard]] std::optional<std::vector<Cell>> pack_destination(const Zone &pack,
                                                                    const Actor &onto) const;
    void move_spirit_pack(Actor &caster, const Actor &onto);
    // Wind Wall: whether a shot from one square to another crosses its wind.
    [[nodiscard]] bool across_wind_wall(Cell from, Cell to) const;
    // Dominate Beast (LEVELS-1): the creature fights for its caster; it moves
    // in the turn order to right after its caster once the command is done.
    void dominate(Actor &caster, Actor &target, int dc);
    // The creature controlling a dominated one, if any.
    [[nodiscard]] const Actor *controller(const Actor &a) const;
    // A dominated creature whose spell has ended returns to its own side.
    void release(Actor &a);
    // Releases ended dominations and puts each dominated creature right after
    // its controller; only between commands, as it moves actors.
    void settle_dominated();
    // Animate Dead: an enemy's Small or Medium Humanoid corpse within 10 feet
    // of the caster; the minute of casting begins, or takes another corpse;
    // when it is done the corpses rise as Zombies the caster commands, which
    // fall again when the fight ends (LEVELS-3).
    [[nodiscard]] bool animatable(const Actor &corpse, const Actor &caster) const;
    void begin_animating(Actor &caster, const Actor &corpse, const detail::SpellDef &spell,
                         unsigned slot);
    void finish_animating(Actor &caster, std::string_view undead);
    void raise_undead(const Actor &caster, Actor &corpse, std::string_view undead);
    // Bestow Curse's curse of `curse` (an index into curses) the source laid
    // on a creature, if any.
    [[nodiscard]] const detail::Effect *curse_on(const Actor &a, int curse) const;
    // Bestow Curse's necrotic curse: 1d8 more when the caster damages it.
    void curse_strikes(const Actor &caster, Actor &target);
    // Insect Plague and Blade Barrier: a creature in the swarm or the blades
    // saves once a turn.
    void swarm_strikes(const Zone &swarm, Actor &creature);
    // Conjure Celestial: the squares of a pillar centred on a square; the
    // light on a creature in it, once a turn (its caster's allies heal, the
    // others save against Radiant); the pillar moving onto a creature.
    [[nodiscard]] std::vector<Cell> pillar_cells(Cell center) const;
    void bathe(const Zone &pillar, Actor &creature);
    [[nodiscard]] bool pillar_reaches(const Zone &pillar, const Actor &onto) const;
    void move_pillar(Actor &caster, const Actor &onto);
    // Divine Word: each foe within 30 feet saves or suffers by its Hit Points.
    void divine_word(Actor &cleric, int dc);
    // Earthquake: each creature in the tremor saves or falls Prone and loses
    // Concentration.
    void tremor(const Zone &quake);
    // Regenerate, its minute of casting over: the Hit Points and the
    // regeneration.
    void finish_knitting(Actor &target, EntityId caster);
    // Holy Aura: the aura that wards a creature, an ally of its caster in it.
    [[nodiscard]] const Zone *holy_aura(const Actor &a) const;
    // Antimagic Field: whether a square is in one; moving each creature's
    // spell effects aside while it is in one and back once it is not.
    [[nodiscard]] bool in_antimagic(Cell cell) const;
    void suppress_spells();
    // Fire Storm: placing or removing a 10-foot cube at a square (LEVELS-3),
    // and the squares the cubes fill.
    void place_storm_cube(Cell cell);
    [[nodiscard]] std::vector<Cell> storm_cells(const std::vector<Cell> &cubes) const;
    // Haste's end: Incapacitated with a Speed of 0 until the end of its next turn.
    void tire(Actor &a);
    // Black Tentacles: a creature in them saves once a turn or is battered
    // and held.
    void grope(const Zone &tentacles, Actor &creature);
    // Wall of Fire: the squares within 10 feet of the wall on its burning
    // side, and the burn of a creature entering the wall or ending its turn
    // in it or beside that side.
    [[nodiscard]] std::vector<Cell> burning_side(std::string_view verb, Cell center,
                                                 const std::vector<Cell> &wall) const;
    void wall_burns(const Zone &fire, Actor &creature);
    // Confusion: the turn's behavior by a d10; false when the creature takes
    // no turn of its own.
    [[nodiscard]] bool confused_turn(Actor &a);
    // The living creatures in reach of a melee attack, friend or foe.
    [[nodiscard]] std::vector<EntityId> creatures_in_reach(const Actor &a) const;
    // Charging in panic, all a creature does is run and lash out (MORALE-2).
    void charge_in_panic(std::vector<Command> &offered, const Actor &a) const;
    // Resilient Sphere and Antimagic Field: nothing from outside reaches the
    // creature, and it reaches nothing outside.
    [[nodiscard]] bool sealed_off(const Actor &a) const;
    // Polymorph: the creature's Beast form ends, and its Temporary Hit Points.
    void end_polymorph(Actor &a);
    // A creature a spell brings into the fight on its caster's side, acting
    // right after it (LEVELS-1, LEVELS-4); its attack bonus is the caster's
    // spell attack bonus.
    Actor &summon(const Actor &caster, std::string definition, std::string name, Cell cell);
    // The free squares nearest the caster, nearest first.
    // Where a creature of `size` could stand, nearest `from` first.
    [[nodiscard]] std::vector<Cell> free_squares_near(Cell from, Footprint size = {}) const;
    // Conjure Elemental: the spirit seizes a creature entering its space or
    // starting its turn beside it, while it holds no one.
    void engulf(const Zone &spirit, Actor &creature);
    // Cloudkill: a creature in the fog saves once a turn.
    void choke_cloud(const Zone &cloud, Actor &creature);
    // Telekinesis: the creature, failing its save, is moved toward `to`.
    void hurl(Actor &caster, Actor &creature, Cell to);
    // Globe of Invulnerability: whether a spell of `level` the caster casts
    // from outside a globe leaves the square inside it be.
    [[nodiscard]] bool globe_wards(const Actor &caster, Cell cell, unsigned level) const;
    // Contingency: the stored spell takes effect on its caster.
    void fire_contingency(Actor &a);
    // Dark One's Own Luck (Fiend 6): 1d10 on a failed save it could still pass.
    void dark_ones_luck(Actor &warlock, detail::SaveResult &result);
    // And on a failed ability check `shortfall` short: the die rolled, or 0.
    int dark_luck_check(Actor &warlock, int shortfall);
    // Spends one of Dark One's Own Luck's uses, if one is left.
    bool spend_dark_luck(Actor &warlock);
    // The combatant with the id, or none once it has left the field.
    [[nodiscard]] const Actor *find_actor(EntityId id) const;
    // A save against an effect that Charms or Frightens: Countercharm (Bard 7)
    // rerolls a failure with Advantage.
    bool charm_or_fear_save(const Actor &target, detail::Ability ability, int dc,
                            bool advantage = false, bool disadvantage = false);
    // Peerless Skill (Lore Bard 14): the Bardic Inspiration die added to the
    // Bard's own failed check or attack roll it could fix; the use is spent
    // only if it does.
    int peerless_skill(Actor &bard, int shortfall);
    // Delayed Blast Fireball: the bead bursts as its spell ends.
    void explode_bead(Actor &caster, const Zone &bead);
    // Prismatic Spray: the ray, or the two, that strike a creature in the cone.
    void prismatic_rays(Actor &caster, Actor &struck, bool saved, int dc, const Dice &rolled);
    // Forcecage: whether a box stands between the two squares or, for a
    // weapon, a cage's bars.
    [[nodiscard]] bool caged_apart(Cell from, Cell to, bool weapon) const;
    // Reverse Gravity: a fall, up or down, on the PUSH-1 scale.
    void fall_hard(Actor &a);
    // Maze: the lost creature spends its turn seeking the way out.
    void seek_maze_exit(Actor &a);
    // Overchannel (Wizard 14): a readied Overchannel makes this casting deal
    // its maximum damage when it may.
    void take_overchannel(Actor &caster, const detail::SpellDef &spell, std::string_view verb);
    // Overchannel's Necrotic damage to its Wizard, after the spell.
    void overchannel_backlash();
    // Nature's Ward (Land 10): the Druid cannot be Poisoned.
    void ward_nature();
    // Conjure Fey: Frightened of the caster until the start of its next turn.
    void fey_frighten(Actor &caster, Actor &target);
    // Enthralling Panache (SRD Pirate): a Wisdom save or Charmed by the pirate
    // until the start of its next turn.
    void enthrall(Actor &pirate, Actor &target);
    // Hypnotic Pattern and Fear: each creature in the area that fails its
    // Wisdom save is entranced or terrified while the caster concentrates.
    void slow_area(Actor &caster, const detail::SpellDef &spell, const std::vector<Cell> &cells,
                   Cell center);
    void enchant_area(Actor &caster, const detail::SpellDef &spell, const std::vector<Cell> &cells);
    // Sleet Storm: a creature entering the sleet or starting its turn there
    // saves, once a turn, or falls Prone and loses Concentration.
    void sleet_falls(const Zone &sleet, Actor &creature);
    // Stinking Cloud: a creature starting its turn in the cloud saves or
    // retches through the turn.
    void choke(const Zone &cloud, Actor &creature);
    // Counterspell: whether a creature may answer this casting, and whether
    // one stops it.
    [[nodiscard]] bool can_counter(const Actor &wizard, const Actor &caster) const;
    // A monster's Protective Magic counters the spell without asking.
    [[nodiscard]] bool protective_counter(const Actor &wizard, const Actor &caster,
                                          const detail::SpellDef &spell) const;
    [[nodiscard]] bool countered(Actor &caster, const detail::SpellDef &spell);
    [[nodiscard]] bool slowed_spell_fails(Actor &caster, std::string_view verb,
                                          const detail::SpellDef &spell);
    void limit_slowed_turn();
    // Vampiric Touch: the touch, at the casting and with each later Magic
    // action, for 3d6 Necrotic and 1d6 more for each slot level above 3.
    void vampiric_touch(Actor &caster, Actor &target, unsigned slot);
    // Improved Blessed Strikes (Cleric 14): after a Cleric cantrip damages a
    // creature, Temporary Hit Points of twice the Wisdom modifier.
    void ready_vitality(Actor &caster, const detail::SpellDef &spell, std::size_t damaged_before);
    void grant_vitality(Actor &cleric, Actor &target);
    // Mass Cure Wounds: up to six allies in its Sphere, the most hurt first.
    void mass_cure(Actor &cleric, const detail::SpellDef &spell, std::string_view verb,
                   const std::vector<Cell> &cells);
    // Contagion: a Constitution save at the end of each of its turns until
    // three successes end it or three failures make it last.
    void contagion_save(Actor &a);
    // Dispel Evil and Good: whether a creature is of the types it wards
    // against; whether an effect is a charm or fear such a creature caused;
    // whether an ally suffers one.
    [[nodiscard]] bool otherworldly(const Actor &a) const;
    [[nodiscard]] static bool enchants(const detail::Effect &e) noexcept;
    [[nodiscard]] bool otherworldly_source(const detail::Effect &e) const;
    [[nodiscard]] bool enchanted_by_otherworld(const Actor &a) const;
    // Superior Hunter's Defense: Resistance to the hit's type for the turn.
    void hunters_defense(Actor &target, detail::DamageType type);
    void spikes_pierce(Actor &creature);
    // Wild Shape's Beast form, kept in step with the wild_shape effect.
    static void refresh_form(Actor &a);
    void end_wild_shape(Actor &a);
    // Animal Shapes' form ends, and its Temporary Hit Points with it.
    void end_animal_shape(Actor &a);
    void protect(Actor &target);
    void change_speed(Actor &a, int old_speed);
    // Land's Aid: thorns in a 10-foot Sphere and flowers that heal one ally.
    void lands_aid(Actor &druid, Cell center);
    void breath_weapon(Actor &dragonborn, const PendingArea &aimed);
    void spend_trait_use(Actor &a);
    // Heroic Inspiration (a Human's Resourceful, HEROIC-1): the d20 of the
    // first failed test that a reroll could save is rolled again. A Long Rest
    // gives it; a new character has none yet. The new roll, if it was used.
    std::optional<int> heroic_reroll(Actor &a);
    // Heat Metal targets only metal armor a creature wears.
    [[nodiscard]] bool wears_metal(const Actor &creature) const;
    // Heat Metal's 2d8 Fire and Constitution save.
    // Heat Metal's burn, of a spell cast with a slot of `slot`.
    void heat_metal(const Actor &caster, Actor &creature, unsigned slot);
    void potent_cantrip(Actor &target, const detail::SpellDef &spell, detail::DamageDice rolled);
    [[nodiscard]] std::vector<EntityId> sculpted(const Actor &caster, const detail::SpellDef &spell,
                                                 unsigned slot_level,
                                                 const std::vector<Cell> &cells) const;
    void cast_area();
    // Logs that `a` uses the carried item a spell comes from (MAGIC-3), and rolls
    // Use Magic Device's d6 for a charge.
    void log_item_use(Actor &a, const detail::SpellDef &spell);
    void log_scroll_use(const Actor &a, const detail::SpellDef &spell);
    // Use Magic Device's Arcana check for a scroll's spell: true when it fails.
    bool scroll_check_fails(const Actor &a, const detail::SpellDef &spell, std::string_view verb);
    // Snake Charm (MAGIC-6): the snakes and scorpions it charms.
    void charm_snakes(Actor &a);
    [[nodiscard]] bool snakes_to_charm(const Actor &a) const;
    // Throws the Javelin of Lightning `token` at `target` as a bolt (MAGIC-3).
    void lightning_javelin(Actor &a, Actor &target, unsigned token);
    void cast_aimed_area(Actor &a, const PendingArea &aimed);
    void squeeze_ensnared(Actor &a);
    void escape_ensnaring(Actor &a);

    // A Strength (Athletics) check against `dc`, before its log line, and
    // the help a failure may still get after it.
    struct AthleticsRoll
    {
        int natural{}, bonus{};
        bool advantage{}, success{};
    };

    AthleticsRoll roll_athletics(Actor &a, int dc);
    bool rescue_athletics(Actor &a, const AthleticsRoll &check, int dc);
    // Guidance's 1d4 for an ability check with `skill`, logged; 0 without it.
    [[nodiscard]] int guidance_bonus(const Actor &a, std::string_view skill);
    [[nodiscard]] bool mark_can_move(const Actor &caster) const;
    [[nodiscard]] bool hexed_by(const Actor &target, const Actor &caster) const;
    [[nodiscard]] bool hex_can_move(const Actor &caster) const;
    // Sacred Weapon's attack bonus, 0 without it.
    [[nodiscard]] int sacred_weapon_bonus(const Actor &a) const;
    [[nodiscard]] detail::DamageType sacred_damage_type(const Actor &a) const;
    [[nodiscard]] unsigned next_save_ms(EntityId target) const;
    [[nodiscard]] unsigned next_turn_ms(const Actor &target) const;
    void advance_turn_time();
    // Halfling Luck, logged when it rerolled a 1 (automatic, as AUTO-1).
    void log_luck(const Actor &a, bool rerolled);
    void log_save(const Actor &target, const detail::SaveResult &result);
    // `advantage` is a feature's own Advantage, such as a Large creature against
    // Ensnaring Strike, and `disadvantage` a lasting spell's Heightened Spell;
    // conditions and armor are applied here.
    bool saving_throw_succeeds(const Actor &target, detail::Ability ability, int dc,
                               bool advantage = false, bool disadvantage = false);
    // The saving throw itself, before Legendary Resistance.
    bool rolled_save_succeeds(const Actor &target, detail::Ability ability, int dc, bool advantage,
                              bool disadvantage);
    // Legendary Resistance: a monster with a use left turns the failed save
    // into a success, which it always does.
    bool resists_legendarily(const Actor &target);
    // Heightened Spell: the creature saves against the caster's spell with
    // Disadvantage for as long as it lasts.
    void mark_heightened(const Actor &caster, const detail::SpellDef &spell);

    [[nodiscard]] bool heightened_against(const Actor &target, const Actor &caster,
                                          std::string_view spell) const
    {
        return detail::heightened_against(target.effects, scope_, caster.source.id, spell);
    }

    [[nodiscard]] detail::MovementGrid movement_grid(const Actor &mover) const;
    // The board with spell zones applied, as movement and the view see it.
    [[nodiscard]] Battlefield zoned_board() const;
    [[nodiscard]] std::vector<Cell> path_to(const Actor &a, Cell destination) const;

    [[nodiscard]] EntityId pending() const
    {
        return reactor_index_ < reactors_.size() ? reactors_[reactor_index_] : 0;
    }

    [[nodiscard]] bool critical_hit(const Actor &a, const Actor &target, int natural,
                                    bool spell = false) const
    {
        return natural == 20 || (!spell && natural >= def(a).critical_from) ||
               (helpless(target) && distance(a, target) <= 5);
    }

    // Unconscious (or asleep) or Paralyzed: attacked with Advantage, critically hit within
    // 5 feet, failing Strength and Dexterity saves.
    static bool helpless(const Actor &target)
    {
        return unconscious(target) || detail::paralyzed(target.effects) ||
               detail::has_effect(target.effects, detail::EffectKind::asleep) ||
               detail::has_effect(target.effects, detail::EffectKind::knocked_out);
    }

    [[nodiscard]] int magic_weapon_bonus(const Actor &a) const;
    // What the weapon's magic adds to the attack and damage rolls: its own
    // enchantment, or Magic Weapon's bonus on a weapon with none, since that
    // spell enchants a nonmagical weapon.
    [[nodiscard]] int weapon_magic(const Actor &a, const Actor &target, AttackRange range);
    [[nodiscard]] int weapon_magic_against(const Actor &a, const Actor &target,
                                           AttackRange range) const;
    [[nodiscard]] int expected_damage(const Actor &a, const Actor &target, AttackRange range) const;
    [[nodiscard]] int expected_damage(const Actor &a, const Command &command) const;
    // Shillelagh is on the Club or Quarterstaff the creature holds.
    [[nodiscard]] bool shillelagh(const Actor &a) const;

    // A readied Metamagic option and, for Transmuted Spell, its new type.
    struct ReadyMetamagic
    {
        Metamagic option{};
        std::optional<detail::DamageType> type;
    };

    // The readied options: one, or two during Sorcery Incarnate.
    [[nodiscard]] static std::vector<ReadyMetamagic> readied_metamagic(const Actor &a);
    // How many options the Sorcerer may ready for one spell (Sorcery
    // Incarnate, Sorcerer 7: two while Innate Sorcery is active).
    [[nodiscard]] static std::size_t metamagic_room(const Actor &a) noexcept;
    // A readied option, if it can change this spell and is affordable.
    [[nodiscard]] static bool readies(const Actor &a, Metamagic option,
                                      const detail::SpellDef &spell);
    // Spends the readied options that apply on the spell being cast.
    void take_metamagic(Actor &caster, const detail::SpellDef &spell);

    [[nodiscard]] bool casting_with(Metamagic option) const
    {
        return std::any_of(casting_metamagic_.begin(), casting_metamagic_.end(),
                           [&](const auto &ready)
                           {
                               return ready.option == option;
                           });
    }

    [[nodiscard]] int spell_range(const Actor &caster, const detail::SpellDef &spell) const;
    [[nodiscard]] detail::DamageType cast_damage_type(detail::DamageType type) const;
    int spell_dice(const Actor &caster, Dice dice, bool critical = false);
    // Careful Spell: up to the Charisma modifier (at least one) of the caster's
    // allies in the area, who then succeed on their saves.
    [[nodiscard]] std::vector<EntityId> careful_allies(const Actor &caster,
                                                       const std::vector<Cell> &cells) const;
    // Metamagic spent on the spell now resolving, and Heightened Spell's target.
    std::vector<ReadyMetamagic> casting_metamagic_;
    EntityId heightened_target_{};
    // The creature using an item's spell (MAGIC-3), whose save DC is then the
    // item's 15; 0 otherwise.
    EntityId item_caster_{};
    // That item's save DC: a wand's 15, or a scroll's by the spell's level.
    int item_dc_{15};
    // The dice in the latest damage roll, doubled on a critical hit: a Ring of
    // Fire Resistance takes 2 from magical fire per die (MAGIC-4).
    int damage_dice_{};
    // Whether the spell or breath now resolving deals Fire damage: a Ring of
    // Fire Resistance adds 4 to saves against it (MAGIC-4).
    bool against_fire_{};
    // Overchannel: the spell its Wizard is casting deals its maximum damage,
    // and the Wizard then takes this many d12 of Necrotic damage.
    bool overchanneling_{};
    EntityId overchannel_caster_{};
    int overchannel_dice_{};

    // 8 + the caster's spellcasting modifier and Proficiency Bonus, +1 during
    // Innate Sorcery. A spell Magic Initiate teaches uses the feat's ability;
    // a class feature names no spell and uses the class's.
    [[nodiscard]] int spell_dc(const Actor &caster, std::string_view spell = {}) const
    {
        if (item_caster_ && caster.source.id == item_caster_)
        {
            return item_dc_;
        }
        return 8 + casting_bonus(def(caster), spell) +
               (detail::has_effect(caster.effects, detail::EffectKind::innate_sorcery) ? 1 : 0);
    }

    // The source recorded on an effect that `a` imposes.
    [[nodiscard]] detail::EffectSource effect_source(const Actor &a) const
    {
        return {.scope = scope_, .actor = a.source.id};
    }

    [[nodiscard]] bool strength_attack(const Actor &a, AttackRange range) const;
    void attack_recklessly(Actor &a);
    // Brutal Strike's blow after its hit: Forceful Blow pushes the target 15
    // feet and lets the Barbarian follow; Hamstring Blow slows it; Staggering
    // and Sundering Blows mark it until the Barbarian's next turn.
    void brutal_blow(Actor &barbarian, Actor &target, BrutalBlow blow);
    // Reliable Talent (Rogue 7): a proficient skill check's d20 counts as at least 10.
    int reliable(const Actor &a, std::string_view skill, int natural);
    // Evasion (Rogue 7): the damage a creature takes from a save-for-half
    // effect, given whether it saved.
    [[nodiscard]] int evaded(const Actor &target, detail::Ability save, bool halves, bool saved,
                             int total) const;
    // Mindless Rage (Berserker 6): raging, immune to Charmed and Frightened.
    [[nodiscard]] bool mindless(const Actor &a) const;
    int frenzy_damage(Actor &berserker, bool critical);
    // Enlarge or Reduce changes a weapon hit's damage.
    int resized_damage(const Actor &a, int amount);
    int burst_damage(const SpellAttack &spell, bool critical);
    [[nodiscard]] static bool fought_advantage(const detail::SpellDef &spell) noexcept;
    // Whether `a` is Charmed by `other`, which it then cannot attack or target.
    [[nodiscard]] bool charmed_by(const Actor &a, EntityId other) const;
    [[nodiscard]] bool aura_covers(const Actor &paladin, const Actor &a) const;
    [[nodiscard]] int aura_of_protection(const Actor &a) const;
    [[nodiscard]] bool aura_of_devotion(const Actor &a) const;
    [[nodiscard]] bool aura_of_courage(const Actor &a) const;
    // Immune to being Frightened: a raging Berserker, or in an Aura of Courage.
    [[nodiscard]] bool fearless(const Actor &a) const;
    void abjure(Actor &paladin, std::span<const EntityId> targets);
    void dispel(Actor &caster, Cell cell, unsigned slot_level);
    [[nodiscard]] int healing_roll(const Actor &target, detail::DamageDice rolled);
    [[nodiscard]] bool smite_protected(const Actor &a) const;
    [[nodiscard]] bool in_aura_of_life(const Actor &a) const;
    void restore_touch(Actor &paladin, Actor &target);
    [[nodiscard]] bool guarded_square(Cell cell) const;
    void guardian_tests(Actor &creature);
    // The nearest square to `near` (its own by default) the creature can stand in.
    [[nodiscard]] Cell nearest_open(const Actor &a, std::optional<Cell> near = {}) const;
    void return_from_banishment(Actor &a);
    // Frightened: a living creature that frightens `a` is in its line of sight.
    [[nodiscard]] std::vector<const Actor *> fears_in_sight(const Actor &a) const;
    [[nodiscard]] bool check_disadvantage(const Actor &a) const;
    // Sunlight Sensitivity on a square in sunlight.
    [[nodiscard]] bool in_sunlight(const Actor &a) const;
    // Whether the square is in sunlight: open to the sky in a fight by day.
    [[nodiscard]] bool open_sky(Cell cell) const;
    // Intimidating Presence (Berserker 14): each enemy within 30 feet makes a
    // Wisdom save or is Frightened for up to a minute.
    void intimidate(const Actor &barbarian);

    // Persistent Rage (Barbarian 15) is offered at Initiative while a Rage use is spent.
    [[nodiscard]] bool persistent_ready(const Actor &a) const
    {
        return def(a).persistent_rage && a.arcane[CharacterClass::barbarian] > 0 &&
               a.channel_divinity[CharacterClass::barbarian] < def(a).rages;
    }
    // True Strike deals Radiant damage or the weapon's own type, as the caster chooses.
    enum class TrueStrikeDamage
    {
        weapon,
        radiant
    };

    void strike_true(Actor &a, Actor &target, TrueStrikeDamage choice);
    bool strikes_duplicate(const Actor &attacker, Actor &target);
    // A weapon attack, or a spell attack when `spell` is given.
    bool attack(Actor &a, Actor &target, AttackRange range,
                const std::optional<SpellAttack> &spell = std::nullopt);
    // A weapon attack made with `loadout`'s statistics (an Unarmed Strike, a
    // thrown, off-hand or Torch strike, True Strike): the attacker wears the
    // loadout's definition, held weapon and light-attack flag for the attack,
    // so what the attack does to the attacker (Fire Shield, Sanctuary ending,
    // once-a-turn features) happens to the attacker itself.
    bool attack_as(Actor &a, Loadout loadout, Actor &target, AttackRange range);
    // A Wight's Life Drain on the creature (MON4CHA 20).
    void drain_life(Actor &wight, Actor &target);
    void dreadful_glare(Actor &mummy, Actor &target);
    void bite(Actor &vampire, Actor &target);
    // A monster's hit riders: its `hampering`, `poisoning`, `pushing` and
    // `hit_advantage` rows for the attack that hit.
    void strike_riders(Actor &a, Actor &target, bool ranged);
    // A Pirate Captain's Captain's Charm at the creature it attacks.
    void captains_charm(Actor &captain, Actor &target);
    // A Wraith's Create Specter: the Humanoid corpses it can raise, and the
    // specters it controls.
    [[nodiscard]] bool specter_corpse(const Actor &wraith, const Actor &corpse) const;
    [[nodiscard]] int specters_of(const Actor &wraith) const;
    // Legendary Actions: right after a creature's turn, each other monster
    // with a use left takes one, as the computer chooses.
    void take_legendary_actions();
    void take_legendary_action(Actor &monster);
    // The living enemy of `a` it sees nearest, within `range` feet and, with
    // `adjacent`, beside it; the weakest among those beside it.
    [[nodiscard]] Actor *legendary_target(const Actor &a, int range, bool adjacent);
    // Where an Adult Bronze Dragon's Thunderclap catches the most enemies,
    // at least two, and none of its allies.
    [[nodiscard]] std::optional<Cell> thunderclap_center(const Actor &dragon) const;
    void thunderclap(Actor &dragon, Cell center);
    // The enemies an Adult Bronze Dragon's Repulsion Breath toward `toward`
    // would catch, none when it would catch one of its allies.
    [[nodiscard]] int repulsion_catches(const Actor &dragon, Cell toward) const;
    void repulsion_breath(Actor &dragon, Cell toward);
    void lower_hit_point_maximum(const Actor &drainer, Actor &target, int amount);
    void shed_immune_conditions();
    void recharge_gaze(Actor &a);
    void recharge_innate(Actor &a);
    void drain_attached(Actor &a);
    void pull_off(Actor &a, Actor &clinger);
    void petrifying_gaze(Actor &basilisk, const PendingArea &aimed);
    void exhale_breath(Actor &dragon, const PendingArea &aimed);
    void resist_petrifying(Actor &a);
    // The attacker wearing the statistics of another attack of its stat block:
    // its `last_attack`, Claw or Gore.
    [[nodiscard]] Loadout attack_loadout(const Actor &a,
                                         const Definition::LastAttack &profile) const;
    // A Ghoul's Claw, carrying its paralysis.
    [[nodiscard]] Loadout claw_loadout(const Actor &a) const;
    // A Minotaur's Gore, carrying its charge.
    [[nodiscard]] Loadout gore_loadout(const Actor &a) const;
    // Whether a Ghoul's Claw can paralyze the creature.
    [[nodiscard]] bool claw_paralyzes(const Actor &target) const;
    // Paralyzed by a Ghoul's Claw on a failed Constitution save.
    void paralyze(Actor &ghoul, Actor &target);
    // A Phase Spider's venom in a creature its Bite dropped to 0 Hit Points.
    void envenom(Actor &spider, Actor &target);
    // A Gnoll's Rampage begins: half its Speed and one Rend left this turn.
    void start_rampage(Actor &gnoll);

    // A Trampling Charge's straight run: where it ends, the creatures whose
    // space it enters, and the conscious enemies and allies among them.
    struct Trample
    {
        Cell end;
        std::vector<EntityId> entered;
        int enemies{}, allies{};
    };

    // The straight run trampling the most conscious enemies and no ally.
    [[nodiscard]] std::optional<Trample> trampling_run(const Actor &centaur) const;
    void trample(Actor &centaur, const Trample &run);
    // A Phase Spider's Ethereal Jaunt away, and back as its turn starts; true
    // when it came back.
    void jaunt_away(Actor &spider);
    bool jaunt_back(Actor &spider);

    // The statistics an attack with the loadout uses: a Wild Shape form's
    // still, as def() gives.
    static const Definition &def(const Actor &a, const Loadout &loadout)
    {
        return a.form ? *a.form : loadout.definition;
    }

    static const Definition &def(const Actor &a, Loadout &&loadout) = delete;
    [[nodiscard]] detail::Mastery weapon_mastery(const Actor &, AttackRange range) const;
    [[nodiscard]] detail::Mastery weapon_mastery(const Actor &, const Definition &,
                                                 AttackRange range) const;
    [[nodiscard]] bool mastery_capacity(const Actor &, const Definition &, const Actor &,
                                        AttackRange range) const;
    [[nodiscard]] detail::RollModifiers attack_modifiers(const Actor &a, const Actor &target,
                                                         AttackRange range,
                                                         AttackSource source) const;
    [[nodiscard]] Dice weapon_dice(const Actor &a, AttackRange range) const;
    [[nodiscard]] detail::DamageDieRule weapon_die_rule(const Actor &a, AttackRange range) const;
    void apply_hit(Actor &a, Actor &target, const ResolvedAttack &resolved);
    static void extend_run(Actor &a, Cell from, Cell to);
    [[nodiscard]] bool charges(const Actor &a, const Actor &target) const;
    [[nodiscard]] bool charge_run(const Actor &a, const Actor &target, int feet, int largest) const;
    [[nodiscard]] bool sneak_eligible(const Actor &a, const Actor &target, AttackRange range,
                                      int mode) const;
    [[nodiscard]] bool ally_beside(const Actor &a, const Actor &target) const;
    int roll_advantage_damage(const Actor &a, const Actor &target, int natural);
    // `forgo` dice are given up for a Cunning Strike.
    [[nodiscard]] int roll_sneak_attack(Actor &a, const Actor &target, int natural, int forgo = 0);
    // Cunning Strike's effect after a hit that dealt Sneak Attack damage.
    void cunning_effect(Actor &rogue, Actor &target, CunningStrike kind);
    // The Cunning Strike effects a command's verb names, if it names any.
    [[nodiscard]] static std::optional<std::pair<CunningStrike, std::optional<CunningStrike>>>
    cunning_choice(std::string_view verb);
    // Stunning Strike's Constitution save after a hit.
    void stunning_strike(Actor &monk, Actor &target);
    // Studied Attacks: a miss gives the Fighter's next attack on the target Advantage.
    void study(const Actor &fighter, Actor &target);
    [[nodiscard]] int keep_higher_savage_roll(Actor &a, int first, int second);
    void finish_reaction();
    // Ordinary fire is Alchemist's Fire or burning oil, not magic (MAGIC-4).
    int resolved_damage(Actor &target, detail::DamageType type, int amount,
                        bool ordinary_fire = false);
    // The creature's damage affinities with those its effects grant: Warding
    // Bond resists all damage, Protection from Poison resists Poison. The
    // creature's own list is read as it is unless something is added, which
    // `added` then holds with it, so most damage copies nothing.
    [[nodiscard]] std::span<const detail::DamageAffinity>
    affinities(const Actor &target, std::vector<detail::DamageAffinity> &added) const;
    // Sanctuary: whether the warded target stops this attack or harmful spell,
    // after the attacker's Wisdom save. Attacking or casting ends the
    // attacker's own Sanctuary.
    bool sanctuary_stops(Actor &attacker, const Actor &target);
    void end_sanctuary(Actor &a);
    // The living casters bonded to `target` by Warding Bond within 60 feet.
    [[nodiscard]] std::vector<EntityId> bonds_on(const Actor &target) const;
    void damage(Actor &target, int amount, bool critical = false);
    int heal(Actor &target, int amount); // Returns the Hit Points restored.
    void resolve_smite(Actor &a, std::string_view verb);
    [[nodiscard]] int armor_class(const Actor &target) const;
    void begin_concentration(Actor &caster, const detail::SpellDef &spell);
    void end_concentration(Actor &caster);
    void drop_concentration_effects(const Actor &caster);
    // An Incapacitated creature loses Concentration and ends its Turn Undead.
    void end_incapacitated_effects(Actor &a);
    [[nodiscard]] unsigned selection_maximum(const PendingSelection &) const;
    void choose_target(const Command &command);
    void cast_on_selection();
    void cast_selected(Actor &a, const PendingSelection &selection);
    // Bless adds 1d4 to attack rolls and saving throws; Bane subtracts 1d4.
    int blessing_die(const Actor &a);
    void burn_searing_smites(Actor &a);
    void update_outcome();
    void wake_resting_participants();
    void bandage_after_victory();

    // A creature with Regeneration that has not died: down at 0 HP, it may rise.
    [[nodiscard]] bool may_rise(const Actor &a) const
    {
        return def(a).regeneration && !a.life.dead;
    }

    // As in the original game, a downed troll's square can be stood on, and a
    // creature standing there keeps it from getting up.
    [[nodiscard]] bool downed_riser(const Actor &a) const
    {
        return may_rise(a) && a.life.hp == 0;
    }

    void regenerate(Actor &a);
    void get_up(Actor &a);
    std::vector<EntityId> initiative_choices_;

    void start_encounter_turns()
    {
        // Recovery checks follow the target's new initiative in a new encounter.
        // Exact timers in a restored encounter are installed after construction.
        for (std::size_t i = 0; i < actors_.size(); ++i)
        {
            for (auto &effect : actors_[i].effects.active)
            {
                if (effect.kind == detail::EffectKind::blindness)
                {
                    effect.save_in_ms = turn_end_ms(i);
                }
            }
            if (actors_[i].life.hp == 0 && !actors_[i].life.dead && !actors_[i].life.stable)
            {
                actors_[i].life.recovery.death_save_in_ms = i ? turn_end_ms(i - 1) : 0;
            }
        }
        // Perfect Focus (Monk 15): at least 4 Focus Points when the fight starts.
        for (auto &a : actors_)
        {
            if (def(a).perfect_focus && a.surges[CharacterClass::monk] < 4 && conscious(a))
            {
                a.surges[CharacterClass::monk] = 4;
                log(a.source.name + " has 4 Focus Points (Perfect Focus).",
                    {"{name} has 4 Focus Points (Perfect Focus).", {{"name", a.source.name}}});
            }
        }
        if (outcome_ == Outcome::ongoing && !begin_turn())
        {
            end_turn();
        }
    }

    // Uncanny Metabolism is offered at Initiative only while it would restore
    // something: spent Focus or missing Hit Points.
    [[nodiscard]] bool metabolism_ready(const Actor &a) const
    {
        return def(a).metabolism && a.arcane[CharacterClass::monk] > 0 &&
               (a.surges[CharacterClass::monk] < def(a).focus || a.life.hp < max_hp(a));
    }

    void resolve_initiative(const Command &command)
    {
        if (command.verb == "initiative_swap")
        {
            std::swap(actor(command.actor).initiative, actor(command.target).initiative);
        }
        if (command.verb == "uncanny_metabolism")
        {
            // All Focus Points back, and the Martial Arts die plus the Monk
            // level in Hit Points.
            auto &monk = actor(command.actor);
            --monk.arcane[CharacterClass::monk];
            monk.surges = surge_capacity(def(monk));
            log(monk.source.name + " uses Uncanny Metabolism.",
                {"{name} uses Uncanny Metabolism.", {{"name", monk.source.name}}});
            heal(monk, healing_roll(monk, {1, def(monk).martial_die,
                                           def(monk).class_levels[CharacterClass::monk]}));
        }
        if (command.verb == "persistent_rage")
        {
            auto &barbarian = actor(command.actor);
            --barbarian.arcane[CharacterClass::barbarian];
            barbarian.channel_divinity[CharacterClass::barbarian] = def(barbarian).rages;
            log(barbarian.source.name + " regains every Rage use (Persistent Rage).",
                {"{name} regains every Rage use (Persistent Rage).",
                 {{"name", barbarian.source.name}}});
        }
        std::erase(initiative_choices_, command.actor);
        std::stable_sort(actors_.begin(), actors_.end(),
                         [](const Actor &a, const Actor &b)
                         {
                             return a.initiative != b.initiative ? a.initiative > b.initiative
                                                                 : a.source.id < b.source.id;
                         });
        if (initiative_choices_.empty())
        {
            start_encounter_turns();
        }
    }

    void validate_initiative() const;
    bool begin_turn();
    void end_turn();
    void restore_self(Actor &a);
    void dispatch_tracking_turn(const Command &command);
    // Heightened Focus: the carried ally steps into the square the Monk left,
    // provoking nothing.
    void follow_carrier(const Actor &monk, Cell departed);
    void progress_movement();
    void restore_movement(std::istream &input);
    void validate_restored_state() const;
    void validate_pending_movement() const;
    void restore_log(std::istream &input);
};
} // namespace opengold::srd5::detail
#endif
