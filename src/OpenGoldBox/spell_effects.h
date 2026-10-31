#ifndef OPENGOLDBOX_SPELL_EFFECTS_H
#define OPENGOLDBOX_SPELL_EFFECTS_H

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace presentation
{
// How one spell looks when it is cast. `travel` names what flies from the caster
// (empty when nothing does) and `arrive` what plays on each target or aimed
// square; both name drawings in scenes/spell_effects.gd.
struct SpellEffect
{
    std::string_view spell, travel, arrive;
    std::uint32_t color{}, accent{};
    float seconds{};   // How long `arrive` plays on one target or square.
    float size{};      // 1 fills a square.
    int amount{};      // How many flames, shards or sparks the drawing makes.
    bool flies_past{}; // A ranged spell attack: a miss sails past its target.
};

// One row for every spell that can be cast in a fight, the same spell table's
// ids; spell_effects_tests holds the two together. Upcast, scroll and item verbs
// such as magic_missile_2 show as their spell.
inline constexpr std::array spell_effects{
    // Cantrips
    SpellEffect{"chill_touch", "", "hand", 0x7fe0c0, 0x143028, 0.8F, 1.1F, 5, false},
    SpellEffect{"shocking_grasp", "", "strike", 0x80c8ff, 0xffffff, 0.5F, 0.9F, 3, false},
    SpellEffect{"eldritch_blast", "helix", "shock", 0xb050ff, 0x60ff90, 0.5F, 1.0F, 3, true},
    SpellEffect{"ray_of_frost", "ray", "frost", 0x90e8ff, 0xffffff, 0.7F, 1.0F, 6, true},
    SpellEffect{"sacred_flame", "", "pillar", 0xffe080, 0xffffff, 0.8F, 1.0F, 6, false},
    SpellEffect{"poison_spray", "wave", "gas", 0x80d040, 0xc8ff60, 0.8F, 0.9F, 6, true},
    SpellEffect{"fire_bolt", "ember", "flames", 0xff7a20, 0xffe060, 0.5F, 0.9F, 7, true},
    SpellEffect{"acid_splash", "spit", "acid", 0x9ce030, 0xe8ff80, 0.6F, 1.0F, 8, false},
    SpellEffect{"resistance", "", "ward", 0xc0a060, 0xfff0c0, 0.7F, 1.0F, 6, false},
    SpellEffect{"guidance", "", "orbit", 0xc0e8ff, 0xffffff, 0.8F, 0.9F, 4, false},
    SpellEffect{"true_strike", "", "sword", 0xffe8a0, 0xffffff, 0.5F, 1.1F, 1, false},
    SpellEffect{"sorcerous_burst", "orb", "prism", 0xff60c0, 0x60e0ff, 0.6F, 1.0F, 6, true},
    SpellEffect{"vicious_mockery", "note", "smiles", 0xe080ff, 0xffe060, 0.8F, 1.0F, 4, false},
    SpellEffect{"starry_wisp", "mote", "sparkles", 0xfff0a0, 0xa0c8ff, 0.6F, 1.0F, 8, true},
    SpellEffect{"spare_the_dying", "", "heal", 0xa0c0ff, 0xffffff, 0.9F, 0.8F, 3, false},
    SpellEffect{"shillelagh", "", "vines", 0x60a040, 0xc0ff80, 0.8F, 0.9F, 4, false},
    SpellEffect{"produce_flame", "", "orbit", 0xff9030, 0xffe080, 0.8F, 0.9F, 3, false},
    // Level 1
    SpellEffect{"magic_missile", "dart", "explosion", 0xa070ff, 0xffffff, 0.3F, 0.5F, 6, false},
    SpellEffect{"cure_wounds", "", "heal", 0x70f090, 0xffffff, 0.9F, 1.0F, 6, false},
    SpellEffect{"inflict_wounds", "", "skull", 0x60ff70, 0x102810, 0.9F, 1.0F, 1, false},
    SpellEffect{"divine_smite", "", "pillar", 0xfff0b0, 0xffc040, 0.7F, 1.3F, 8, false},
    SpellEffect{"searing_smite", "", "flames", 0xff4010, 0xffc040, 0.8F, 1.2F, 12, false},
    SpellEffect{"ensnaring_strike", "", "vines", 0x40a030, 0xa0ff50, 1.0F, 1.2F, 6, false},
    SpellEffect{"entangle", "", "vines", 0x509030, 0xd0e060, 1.1F, 1.0F, 3, false},
    SpellEffect{"fog_cloud", "", "gas", 0xc0d0e0, 0xffffff, 1.3F, 1.5F, 3, false},
    SpellEffect{"guiding_bolt", "comet", "radiance", 0xfff0a0, 0xffffff, 0.8F, 1.1F, 8, true},
    SpellEffect{"bane", "", "sink", 0x905090, 0xe060c0, 0.9F, 1.0F, 3, false},
    SpellEffect{"sanctuary", "", "ward", 0xfff0d0, 0xa0d0ff, 1.0F, 1.1F, 8, false},
    SpellEffect{"burning_hands", "", "flames", 0xff6010, 0xffd040, 0.6F, 1.0F, 5, false},
    SpellEffect{"thunderwave", "", "shock", 0x80a0ff, 0xffffff, 0.5F, 1.1F, 2, false},
    SpellEffect{"mage_armor", "", "ward", 0x70a0ff, 0xd0e0ff, 0.9F, 1.1F, 6, false},
    SpellEffect{"false_life", "", "wisps", 0x60b080, 0xc0ffd0, 1.0F, 1.0F, 5, false},
    SpellEffect{"expeditious_retreat", "", "wind", 0xc0f0ff, 0xffffff, 0.8F, 1.0F, 6, false},
    SpellEffect{"ray_of_sickness", "ray", "gas", 0x90b020, 0xd0e040, 0.8F, 0.9F, 5, true},
    SpellEffect{"ice_knife", "shard", "frost", 0xa0e0ff, 0xffffff, 0.6F, 1.1F, 8, true},
    SpellEffect{"chromatic_orb", "orb", "bubble", 0xff50a0, 0x50ffd0, 0.7F, 1.0F, 5, true},
    SpellEffect{"sleep", "", "zzz", 0x8090ff, 0xe0e4ff, 1.2F, 1.0F, 2, false},
    SpellEffect{"hideous_laughter", "", "smiles", 0xffd040, 0xff70c0, 1.1F, 1.1F, 5, false},
    SpellEffect{"color_spray", "", "prism", 0xff60a0, 0x60ffa0, 0.6F, 1.0F, 5, false},
    SpellEffect{"grease", "", "acid", 0x806030, 0xd0b060, 1.0F, 1.0F, 4, false},
    SpellEffect{"shield", "", "ward", 0x90d0ff, 0xffffff, 0.5F, 1.3F, 6, false},
    SpellEffect{"charm_person", "", "hearts", 0xff70a0, 0xffd0e0, 1.1F, 1.0F, 5, false},
    SpellEffect{"hex", "", "sigil", 0xa030c0, 0x40ff80, 0.9F, 1.1F, 5, false},
    SpellEffect{"hellish_rebuke", "", "flames", 0xe02020, 0xff9020, 0.7F, 1.3F, 10, false},
    SpellEffect{"dissonant_whispers", "", "rings", 0xc060ff, 0xffb0ff, 0.8F, 1.0F, 4, false},
    SpellEffect{"faerie_fire", "", "sparkles", 0xd070ff, 0x80ffc0, 0.9F, 1.0F, 4, false},
    SpellEffect{"shield_of_faith", "", "ward", 0xffe090, 0xffffff, 0.9F, 1.1F, 8, false},
    SpellEffect{"heroism", "", "rise", 0xffc040, 0xffffff, 0.9F, 1.0F, 3, false},
    SpellEffect{"bless", "", "crown", 0xfff0a0, 0xffffff, 0.9F, 1.0F, 5, false},
    SpellEffect{"protection_from_evil_and_good", "", "runes", 0xe0e0ff, 0xffe080, 1.0F, 1.1F, 6,
                false},
    SpellEffect{"command", "", "eye", 0xffc040, 0xffffff, 0.8F, 1.0F, 1, false},
    SpellEffect{"hunters_mark", "", "sigil", 0x60e060, 0xe0ffc0, 0.8F, 1.0F, 4, false},
    SpellEffect{"longstrider", "", "wind", 0x90e0a0, 0xe0ffe0, 0.8F, 0.9F, 4, false},
    SpellEffect{"divine_favor", "", "crown", 0xffb040, 0xfff0c0, 0.8F, 1.1F, 7, false},
    SpellEffect{"healing_word", "note", "heal", 0x70ffc0, 0xffffff, 0.7F, 0.9F, 4, false},
    SpellEffect{"potion_of_healing", "", "bubble", 0xff5060, 0xffc0c0, 0.8F, 0.8F, 6, false},
    SpellEffect{"potion_of_greater_healing", "", "bubble", 0xff2080, 0xffe0f0, 0.9F, 1.0F, 8,
                false},
    SpellEffect{"potion_of_speed", "", "wind", 0xffe030, 0xffffff, 0.7F, 1.1F, 8, false},
    SpellEffect{"potion_of_giant_strength", "", "rise", 0xe07030, 0xffd080, 0.8F, 1.3F, 4, false},
    SpellEffect{"dust_of_disappearance", "", "poof", 0xe0d0c0, 0xffffff, 0.9F, 1.2F, 12, false},
    SpellEffect{"wand_of_paralysis", "ray", "chains", 0x60e0e0, 0xe0ffff, 0.9F, 1.0F, 5, false},
    SpellEffect{"dragons_breath_exhale", "", "flames", 0xff5020, 0xffe040, 0.6F, 1.1F, 6, false},
    SpellEffect{"breath_weapon_cone", "", "explosion", 0xff9040, 0xfff0b0, 0.5F, 1.2F, 4, false},
    SpellEffect{"lightning_breath", "", "strike", 0x60a0ff, 0xe0f0ff, 0.5F, 1.0F, 2, false},
    SpellEffect{"petrifying_gaze", "", "eye", 0xa0a090, 0xe0e0c0, 0.7F, 0.8F, 1, false},
    SpellEffect{"acid_spray", "", "acid", 0x60e040, 0xe0ff60, 0.6F, 1.0F, 6, false},
    SpellEffect{"thunderclap", "", "shock", 0xc0c0ff, 0xffffff, 0.5F, 1.0F, 3, false},
    SpellEffect{"repulsion_breath", "", "wind", 0xd0e0ff, 0xffffff, 0.6F, 1.0F, 4, false},
    SpellEffect{"breath_weapon_line", "", "strike", 0xc0ff60, 0xffffff, 0.5F, 0.9F, 2, false},
    SpellEffect{"clouds_jaunt", "", "poof", 0xd0e8ff, 0xffffff, 0.7F, 1.0F, 8, false},
    SpellEffect{"compulsion_heading", "", "question", 0xe090ff, 0xffffff, 0.6F, 0.9F, 2, false},
    SpellEffect{"lands_aid", "", "vines", 0x306020, 0x90e040, 0.8F, 0.9F, 3, false},
    SpellEffect{"spirit_breath", "", "wisps", 0xff8040, 0xffe0a0, 0.6F, 1.0F, 4, false},
    SpellEffect{"natures_sanctuary", "", "leaves", 0x60c040, 0xe0e060, 1.0F, 1.0F, 4, false},
    SpellEffect{"abjure_foes", "", "sink", 0xffe090, 0xffffff, 0.8F, 1.0F, 3, false},
    // Level 2
    SpellEffect{"scorching_ray", "ray", "flames", 0xff5a10, 0xffd030, 0.4F, 0.8F, 6, true},
    SpellEffect{"blindness", "", "shadow", 0x302040, 0x8060a0, 1.0F, 1.0F, 3, false},
    SpellEffect{"shining_smite", "", "radiance", 0xfff8c0, 0xffffff, 0.8F, 1.3F, 12, false},
    SpellEffect{"hold_person", "", "chains", 0xd0c080, 0xfff0c0, 1.0F, 1.0F, 6, false},
    SpellEffect{"warding_bond", "chainbeam", "ward", 0xe0c070, 0xfff0c0, 0.7F, 1.0F, 6, false},
    SpellEffect{"protection_from_poison", "", "ward", 0x80e080, 0xe0ffe0, 0.8F, 1.0F, 5, false},
    SpellEffect{"silence", "", "rings", 0x8090a0, 0xe0e8f0, 1.0F, 1.2F, 2, false},
    SpellEffect{"spiritual_weapon", "", "hammer", 0xffe070, 0xffffff, 0.7F, 1.1F, 1, false},
    SpellEffect{"shatter", "", "crack", 0xc0d0ff, 0xffffff, 0.6F, 1.2F, 6, false},
    SpellEffect{"web", "", "web", 0xe8e8e8, 0xb0b0b0, 1.1F, 1.2F, 8, false},
    SpellEffect{"misty_step", "", "poof", 0xc0e0ff, 0xffffff, 0.6F, 1.1F, 12, false},
    SpellEffect{"acid_arrow", "shard", "acid", 0x90f020, 0xf0ff80, 0.9F, 1.1F, 10, true},
    SpellEffect{"mind_spike", "needle", "rings", 0xff60d0, 0xffe0f8, 0.6F, 1.0F, 3, false},
    SpellEffect{"ray_of_enfeeblement", "ray", "sink", 0x806040, 0xd0c080, 0.8F, 1.0F, 2, false},
    SpellEffect{"blur", "", "mirrors", 0xa0b0ff, 0xe0e8ff, 0.9F, 1.0F, 3, false},
    SpellEffect{"mirror_image", "", "mirrors", 0xffe0ff, 0xffffff, 1.0F, 1.3F, 5, false},
    SpellEffect{"magic_weapon", "", "sword", 0x80c0ff, 0xffffff, 0.8F, 1.0F, 2, false},
    SpellEffect{"invisibility", "", "poof", 0xb0c0e0, 0xe8f0ff, 1.0F, 1.0F, 8, false},
    SpellEffect{"see_invisibility", "", "eye", 0x80e0ff, 0xffffff, 0.8F, 0.9F, 1, false},
    SpellEffect{"darkness", "", "shadow", 0x080818, 0x303050, 1.3F, 1.5F, 4, false},
    SpellEffect{"flaming_sphere", "", "explosion", 0xff6a10, 0xffe060, 1.0F, 1.0F, 8, false},
    SpellEffect{"enlarge_reduce", "", "rise", 0xc090ff, 0xffffff, 0.9F, 1.2F, 2, false},
    SpellEffect{"dragons_breath", "", "orbit", 0xff5030, 0xffc040, 0.9F, 1.1F, 5, false},
    SpellEffect{"gust_of_wind", "", "wind", 0xb0e8e0, 0xffffff, 0.7F, 1.0F, 5, false},
    SpellEffect{"aid", "", "rise", 0xf0f0d0, 0xffffff, 0.9F, 0.9F, 2, false},
    SpellEffect{"lesser_restoration", "", "heal", 0xd0f0a0, 0xffffff, 0.8F, 0.9F, 3, false},
    SpellEffect{"barkskin", "", "leaves", 0x806030, 0xb09050, 0.9F, 1.1F, 7, false},
    SpellEffect{"flame_blade", "", "sword", 0xff7a20, 0xffe060, 0.8F, 1.1F, 1, false},
    SpellEffect{"moonbeam", "", "moon", 0xc0d8ff, 0xffffff, 1.0F, 1.1F, 3, false},
    SpellEffect{"spike_growth", "", "thorns", 0x608030, 0xc0d060, 0.9F, 1.0F, 5, false},
    SpellEffect{"heat_metal", "", "sigil", 0xff5010, 0xffc040, 0.8F, 1.0F, 6, false},
    SpellEffect{"enhance_ability", "", "rise", 0x60e0d0, 0xffffff, 0.8F, 0.9F, 3, false},
    SpellEffect{"snake_charm", "", "note", 0x80d080, 0xe0ff80, 0.9F, 0.9F, 4, false},
    // Level 3
    SpellEffect{"magic_circle", "", "runes", 0xa0d0ff, 0xffffff, 1.1F, 1.1F, 8, false},
    SpellEffect{"protection_from_energy", "", "ward", 0xff90d0, 0x90e0ff, 0.9F, 1.1F, 6, false},
    SpellEffect{"conjure_animals", "", "claws", 0xc08040, 0xffe0b0, 0.7F, 1.0F, 3, false},
    SpellEffect{"wind_wall", "", "vortex", 0xc0e0d0, 0xffffff, 1.0F, 0.9F, 4, false},
    SpellEffect{"mass_healing_word", "note", "heal", 0x60ffa0, 0xffffc0, 0.8F, 1.0F, 5, false},
    SpellEffect{"spirit_guardians", "", "orbit", 0xfff0c0, 0xd0e0ff, 1.2F, 1.8F, 8, false},
    SpellEffect{"bestow_curse", "", "sigil", 0x803090, 0xe040a0, 1.0F, 1.2F, 7, false},
    SpellEffect{"animate_dead", "", "skull", 0x90f070, 0x182818, 1.1F, 1.1F, 2, false},
    SpellEffect{"fireball", "comet", "explosion", 0xff6010, 0xffe050, 0.6F, 1.3F, 6, false},
    SpellEffect{"lightning_bolt", "", "strike", 0x90c0ff, 0xffffff, 0.5F, 1.0F, 2, false},
    SpellEffect{"counterspell", "", "shock", 0xe0e0e0, 0x8040ff, 0.5F, 1.2F, 4, false},
    SpellEffect{"haste", "", "wind", 0xffe060, 0xffffff, 0.8F, 1.1F, 7, false},
    SpellEffect{"hypnotic_pattern", "", "vortex", 0xff60e0, 0x60e0ff, 1.1F, 1.0F, 6, false},
    SpellEffect{"fear", "", "skull", 0x803060, 0x201018, 0.7F, 0.8F, 1, false},
    SpellEffect{"sleet_storm", "", "rain", 0xb0d8f0, 0xffffff, 1.1F, 1.2F, 6, false},
    SpellEffect{"stinking_cloud", "", "gas", 0xa0b030, 0xe0e070, 1.2F, 1.4F, 4, false},
    SpellEffect{"vampiric_touch", "", "wisps", 0xd01040, 0x400818, 0.9F, 1.1F, 6, false},
    SpellEffect{"blink", "", "poof", 0x90a0ff, 0xf0f0ff, 0.7F, 1.0F, 10, false},
    SpellEffect{"call_lightning", "", "clouds", 0x8090c0, 0xc0e0ff, 0.9F, 1.3F, 3, false},
    SpellEffect{"plant_growth", "", "vines", 0x40b040, 0xffe060, 1.1F, 0.8F, 2, false},
    SpellEffect{"revivify", "", "pillar", 0xa0ffd0, 0xffffff, 1.2F, 1.1F, 8, false},
    SpellEffect{"remove_curse", "", "sparkles", 0xf0e0ff, 0xffffff, 0.9F, 1.1F, 10, false},
    SpellEffect{"beacon_of_hope", "", "radiance", 0xc0e8ff, 0xfff8e0, 1.1F, 1.4F, 10, false},
    SpellEffect{"dispel_magic", "", "rings", 0xf0f0f0, 0x6090ff, 0.7F, 1.2F, 5, false},
    SpellEffect{"slow", "", "sink", 0x7090b0, 0xd0e0f0, 0.9F, 0.8F, 2, false},
    SpellEffect{"protection_from_normal_missiles", "", "ward", 0xd0c0a0, 0xf0e0c0, 0.9F, 1.2F, 4,
                false},
    // Level 4
    SpellEffect{"black_tentacles", "", "tentacles", 0x201830, 0x705090, 1.1F, 1.0F, 3, false},
    SpellEffect{"blight", "", "wisps", 0x708020, 0x202808, 1.0F, 1.2F, 7, false},
    SpellEffect{"charm_monster", "", "hearts", 0xe04090, 0xffb0d0, 1.1F, 1.3F, 7, false},
    SpellEffect{"compulsion", "", "eye", 0xe080ff, 0xffffff, 0.9F, 1.1F, 2, false},
    SpellEffect{"confusion", "", "question", 0xc0a0ff, 0xffe080, 1.0F, 1.0F, 3, false},
    SpellEffect{"dimension_door", "", "vortex", 0x8040e0, 0xe0c0ff, 0.8F, 1.3F, 3, false},
    SpellEffect{"fire_shield", "", "orbit", 0xff6020, 0x90d0ff, 1.0F, 1.2F, 6, false},
    SpellEffect{"greater_invisibility", "", "poof", 0x90a0d0, 0xffffff, 1.1F, 1.3F, 14, false},
    SpellEffect{"ice_storm", "", "rain", 0xd0f0ff, 0xffffff, 0.9F, 1.3F, 10, false},
    SpellEffect{"phantasmal_killer", "", "skull", 0xc060ff, 0x180830, 1.1F, 1.3F, 3, false},
    SpellEffect{"polymorph", "", "vortex", 0x60f0a0, 0xffe060, 1.1F, 1.2F, 5, false},
    SpellEffect{"resilient_sphere", "", "bubble", 0x90d0ff, 0xffffff, 1.0F, 1.5F, 1, false},
    SpellEffect{"vitriolic_sphere", "spit", "acid", 0xa0ff20, 0xffff90, 0.8F, 1.3F, 8, false},
    SpellEffect{"wall_of_fire", "", "flames", 0xff4a10, 0xfff060, 1.2F, 1.2F, 6, false},
    SpellEffect{"faithful_hound", "", "beast", 0xc0d0e0, 0xffffff, 0.9F, 1.1F, 3, false},
    SpellEffect{"conjure_minor_elementals", "", "orbit", 0x60d0ff, 0xff8040, 1.1F, 1.4F, 4, false},
    SpellEffect{"giant_insect", "", "insects", 0x907020, 0xd0c050, 1.0F, 1.2F, 14, false},
    SpellEffect{"stoneskin", "", "rocks", 0x908878, 0xc8c0b0, 0.9F, 1.0F, 8, false},
    SpellEffect{"death_ward", "", "ward", 0xf0f0ff, 0x60ff90, 1.0F, 1.1F, 6, false},
    SpellEffect{"aura_of_life", "", "leaves", 0x70f0a0, 0xfff0a0, 1.2F, 1.5F, 10, false},
    SpellEffect{"freedom_of_movement", "", "wind", 0xa0f0d0, 0xfff0a0, 0.9F, 1.1F, 5, false},
    SpellEffect{"guardian_of_faith", "", "sword", 0xfff0c0, 0x90c0ff, 1.1F, 1.5F, 2, false},
    SpellEffect{"banishment", "", "vortex", 0xff3060, 0x400818, 1.1F, 1.3F, 4, false},
    SpellEffect{"dominate_beast", "", "eye", 0x90c040, 0xf0ff90, 1.0F, 1.1F, 1, false},
    SpellEffect{"conjure_woodland_beings", "", "leaves", 0xd0e050, 0xff90d0, 1.1F, 1.3F, 14, false},
    // Level 5
    SpellEffect{"arcane_hand", "", "hand", 0x70b0ff, 0xffffff, 0.8F, 1.5F, 5, false},
    SpellEffect{"cloudkill", "", "gas", 0x50a040, 0xa0e060, 1.3F, 1.4F, 6, false},
    SpellEffect{"cone_of_cold", "", "frost", 0x70d0ff, 0xffffff, 0.8F, 1.1F, 6, false},
    SpellEffect{"conjure_elemental", "", "vortex", 0x60a0ff, 0xffffff, 1.0F, 1.4F, 6, false},
    SpellEffect{"contagion", "", "insects", 0x608030, 0x304010, 1.0F, 1.0F, 20, false},
    SpellEffect{"dispel_evil_and_good", "", "radiance", 0xf0f8ff, 0xc0e0ff, 1.0F, 1.5F, 16, false},
    SpellEffect{"flame_strike", "", "pillar", 0xff7020, 0xfff0a0, 0.9F, 1.1F, 10, false},
    SpellEffect{"insect_plague", "", "insects", 0x302010, 0xa08040, 1.2F, 1.1F, 18, false},
    SpellEffect{"mass_cure_wounds", "", "heal", 0x50f0d0, 0xffffff, 1.0F, 0.9F, 4, false},
    SpellEffect{"greater_restoration", "", "sparkles", 0xfff0c0, 0x90ffd0, 1.0F, 1.2F, 14, false},
    SpellEffect{"dominate_person", "", "eye", 0xd03060, 0xffc0d0, 1.0F, 1.2F, 3, false},
    SpellEffect{"hold_monster", "", "chains", 0xa0a0d0, 0xf0f0ff, 1.1F, 1.3F, 8, false},
    SpellEffect{"summon_dragon", "", "explosion", 0xe04030, 0xffc040, 1.1F, 1.8F, 8, false},
    SpellEffect{"wall_of_force", "", "cage", 0x90b0ff, 0xe0e8ff, 1.1F, 1.0F, 2, false},
    SpellEffect{"wall_of_stone", "", "rocks", 0x807868, 0xb8b098, 1.1F, 1.1F, 6, false},
    SpellEffect{"animate_objects", "", "orbit", 0xd0a060, 0xfff0c0, 1.0F, 1.4F, 7, false},
    SpellEffect{"mislead", "", "mirrors", 0x90a0a0, 0xe0f0f0, 1.0F, 1.1F, 2, false},
    SpellEffect{"telekinesis", "", "hand", 0xd080ff, 0xffffff, 0.9F, 1.3F, 5, false},
    SpellEffect{"antilife_shell", "", "ward", 0x70ff70, 0x202020, 1.1F, 1.6F, 8, false},
    // Level 6
    SpellEffect{"blade_barrier", "", "blades", 0xd0e0f0, 0xffffff, 1.2F, 1.0F, 3, false},
    SpellEffect{"harm", "", "skull", 0x801020, 0x180408, 1.2F, 1.3F, 1, false},
    SpellEffect{"heal", "", "heal", 0xfff0a0, 0xffffff, 1.2F, 1.4F, 10, false},
    SpellEffect{"sunbeam", "", "pillar", 0xfff4b0, 0xffffff, 0.8F, 0.9F, 5, false},
    SpellEffect{"true_seeing", "", "eye", 0xf0f8ff, 0x60c0ff, 1.0F, 1.2F, 2, false},
    SpellEffect{"chain_lightning", "zigzag", "strike", 0x70b0ff, 0xffffff, 0.5F, 1.1F, 3, false},
    SpellEffect{"circle_of_death", "", "skull", 0x50e060, 0x081808, 0.9F, 0.9F, 1, false},
    SpellEffect{"disintegrate", "ray", "poof", 0x50ff60, 0xe0ffe0, 0.9F, 1.3F, 18, false},
    SpellEffect{"eyebite", "", "eye", 0x90ff40, 0x104000, 0.9F, 1.1F, 1, false},
    SpellEffect{"flesh_to_stone", "", "rocks", 0xa0988a, 0xe0d8c0, 1.1F, 1.0F, 12, false},
    SpellEffect{"freezing_sphere", "orb", "frost", 0x90d8ff, 0xffffff, 0.8F, 1.2F, 6, false},
    SpellEffect{"globe_of_invulnerability", "", "bubble", 0xf0d060, 0xfff8d0, 1.1F, 1.8F, 1, false},
    SpellEffect{"irresistible_dance", "", "note", 0xff70d0, 0x80f0ff, 1.2F, 1.1F, 7, false},
    SpellEffect{"magic_jar", "", "vortex", 0x70f0d0, 0x103028, 1.1F, 1.1F, 3, false},
    SpellEffect{"wall_of_ice", "", "frost", 0xb0ecff, 0xe8ffff, 1.1F, 1.3F, 8, false},
    SpellEffect{"conjure_fey", "", "beast", 0xc080ff, 0x70ffb0, 0.9F, 1.2F, 2, false},
    SpellEffect{"wall_of_thorns", "", "thorns", 0x408020, 0xa0a030, 1.1F, 1.1F, 7, false},
    SpellEffect{"create_undead", "", "skull", 0x70a0a0, 0x101820, 1.2F, 1.2F, 2, false},
    // Level 7
    SpellEffect{"conjure_celestial", "", "pillar", 0xf0f0ff, 0xffe080, 1.1F, 1.3F, 12, false},
    SpellEffect{"divine_word", "", "radiance", 0xffffff, 0xffd040, 0.9F, 1.8F, 20, false},
    SpellEffect{"fire_storm", "", "flames", 0xff3a10, 0xffb020, 1.1F, 1.4F, 7, false},
    SpellEffect{"regenerate", "", "leaves", 0x60ff80, 0xffffff, 1.2F, 1.0F, 6, false},
    SpellEffect{"delayed_blast_fireball", "comet", "explosion", 0xffc020, 0xff3010, 0.9F, 1.5F, 10,
                false},
    SpellEffect{"finger_of_death", "needle", "skull", 0x30ff50, 0x001000, 1.1F, 1.4F, 3, false},
    SpellEffect{"forcecage", "", "cage", 0xc0a0ff, 0xf0e8ff, 1.1F, 1.0F, 3, false},
    SpellEffect{"prismatic_spray", "", "prism", 0xff4040, 0x4080ff, 0.8F, 1.2F, 7, false},
    SpellEffect{"reverse_gravity", "", "rise", 0xa0c0ff, 0xe0f0ff, 1.0F, 0.9F, 4, false},
    SpellEffect{"arcane_sword", "", "sword", 0xa0e0ff, 0xffffff, 0.7F, 1.4F, 1, false},
    // Level 8
    SpellEffect{"antimagic_field", "", "rings", 0x808090, 0x202030, 1.1F, 1.9F, 6, false},
    SpellEffect{"earthquake", "", "crack", 0x907050, 0xd0b080, 0.9F, 1.3F, 5, false},
    SpellEffect{"holy_aura", "", "crown", 0xfff8d0, 0xa0d0ff, 1.2F, 1.5F, 12, false},
    SpellEffect{"sunburst", "", "radiance", 0xffe870, 0xffffff, 0.9F, 1.2F, 10, false},
    SpellEffect{"animal_shapes", "", "beast", 0xa07040, 0xe0c090, 1.1F, 1.2F, 4, false},
    SpellEffect{"befuddlement", "", "question", 0xff80c0, 0xffffff, 1.0F, 1.3F, 5, false},
    SpellEffect{"dominate_monster", "", "eye", 0x8020d0, 0xf0c0ff, 1.1F, 1.4F, 4, false},
    SpellEffect{"incendiary_cloud", "", "gas", 0xd05010, 0xffb030, 1.2F, 1.5F, 5, false},
    SpellEffect{"maze", "", "vortex", 0xd0b070, 0x403010, 1.1F, 1.4F, 7, false},
    SpellEffect{"mind_blank", "", "ward", 0xe0a0ff, 0xffffff, 1.0F, 1.0F, 10, false},
    SpellEffect{"power_word_stun", "", "shock", 0xffe040, 0xffffff, 0.7F, 1.5F, 5, false},
    SpellEffect{"tsunami", "", "tide", 0x3080d0, 0xc0f0ff, 1.1F, 1.4F, 4, false},
};

// Actions of a spell whose verbs don't begin with its id.
inline constexpr std::array<std::pair<std::string_view, std::string_view>, 6> spell_verb_names{{
    {"rebuke", "hellish_rebuke"},
    {"hurl_flame", "produce_flame"},
    {"roll_flaming_sphere", "flaming_sphere"},
    {"hand_fist", "arcane_hand"},
    {"hand_push", "arcane_hand"},
    {"detonate", "delayed_blast_fireball"},
}};

// The effect of a command's verb, or null when it is no spell's. An upcast, a
// scroll or an item adds to the spell's id; the longest match keeps Shield of
// Faith from showing as Shield.
[[nodiscard]] constexpr const SpellEffect *spell_effect_for_verb(std::string_view verb)
{
    for (const auto &[other_name, spell] : spell_verb_names)
    {
        if (verb == other_name)
        {
            verb = spell;
            break;
        }
    }
    const SpellEffect *found = nullptr;
    for (const auto &effect : spell_effects)
    {
        const bool whole = verb == effect.spell;
        const bool prefix = verb.size() > effect.spell.size() && verb.starts_with(effect.spell) &&
                            verb[effect.spell.size()] == '_';
        if ((whole || prefix) && (!found || effect.spell.size() > found->spell.size()))
        {
            found = &effect;
        }
    }
    return found;
}
} // namespace presentation

#endif
