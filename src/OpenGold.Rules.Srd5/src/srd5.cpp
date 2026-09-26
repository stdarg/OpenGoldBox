#include "dice.h"
#include "rest_activity.h"
#include "damage_roll.h"
#include "sneak_attack.h"
#include "action_budget.h"
#include "feature_grants.h"
#include "training.h"
#include "spell_access.h"
#include "spell_components.h"
#include "combat_grid.h"
#include "status_effects.h"
#include "life_cycle.h"
#include "recovery_timeline.h"
#include "weapons.h"
#include "armor.h"
#include "opengold/srd5.h"
#include <algorithm>
#include <initializer_list>
#include <utility>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace opengold::srd5 {
using namespace rules;
int ability_modifier(int score) noexcept { return static_cast<int>(std::floor((static_cast<double>(score)-10)/2.0)); }
int minimum_save_roll(int dc, int bonus) noexcept
{ return static_cast<int>(std::clamp(static_cast<long long>(dc)-bonus,1LL,21LL)); }
bool attack_hits(int natural, int bonus, int ac) noexcept
{ return natural==20 || (natural!=1 && static_cast<std::int64_t>(natural)+bonus>=ac); }
namespace {
std::string weapon_label(std::string_view key)
{
    if(const auto* item=detail::weapon(key))return std::string(item->label);
    if(const auto* item=detail::armor(key))return std::string(item->label);
    return std::string(key);
}
std::string attack_ability(std::string_view key)
{
    const auto* item=detail::weapon(key);
    return item&&item->finesse?"higher of Strength or Dexterity":item&&item->ranged?"Dexterity":"Strength";
}
bool trained(std::string_view klass,std::string_view key)
{
    // Starting-class grants, SRD 5.2.1 pp. 49 and 61. Multiclass entry and
    // optional feature grants are separate, not yet implemented capabilities.
    if(const auto* weapon=detail::weapon(key))
        return !weapon->martial||klass=="Barbarian"||klass=="Fighter"||klass=="Paladin"||klass=="Ranger"||
            (klass=="Rogue"&&(weapon->finesse||weapon->light))||
            (klass=="Monk"&&weapon->light);
    if(const auto* armor=detail::armor(key))return detail::armor_trained(klass,armor->category);
    throw std::runtime_error("Unsupported equipment conversion: "+std::string(key));
}
}
std::string equipment_note(const CharacterSheet& sheet,std::string_view item)
{
    std::string text;
    if(trained(sheet.character_class,item))text="Class training: no untrained-use penalty.";
    else if(item=="shield")text="Untrained shield: no AC bonus.";
    else if(detail::armor(item))text="Untrained armor: disadvantage on Strength/Dexterity attacks, checks (including initiative), and saves; cannot cast spells.";
    else text="Untrained weapon: no proficiency bonus on attack rolls (no +2 at level 1).";
    if(item=="chain_mail"&&sheet.scores[0]<13)text+=" Chain mail requires Strength 13: speed reduced by 10 feet.";
    if(item=="wand")text+=" Plain focus only; no charged wand spell is granted.";
    return text;
}
namespace {
constexpr std::string_view rush_source="species:orc/trait:adrenaline_rush";
// Compatibility is checked before this helper. Feature introduction boundaries
// must stay fixed when the current module version advances.
bool module_before(const Identity& identity,std::array<unsigned,3> introduced)
{
    std::istringstream in(identity.version);std::array<unsigned,3> version{};char first{},second{};
    in>>version[0]>>first>>version[1]>>second>>version[2];
    if(!in||first!='.'||second!='.')throw std::runtime_error("Invalid module version");
    in>>std::ws;if(!in.eof())throw std::runtime_error("Invalid module version");return version<introduced;
}

// The character-profile tag is a monotonic capability marker: a profile written
// as PC<n> contains every capability introduced at or before n. The enum value
// *is* the introducing tag, so a new feature adds one row here instead of
// another link in a boolean chain -- the old form required editing the
// neighbouring line as well, which is what made it error-prone.
enum class Cap : unsigned {
    selected=3, training=7, spells=10, cantrips=11, cleric_cantrips=12,
    surge=13, frost=14, sage=15, backgrounds=16, archery=17, styles=18,
    class_skills=19, instruments=20, monk_tools=21, herbalism=22, gaming=23,
    cunning=24, warlock=25, shocking=26, warlock_poison=27, sorcerer=28,
    chill=29, mind=30, champion=31, arcane=32, scholar=33, choices=34,
    rogue=35,
    // Spells are written as an explicit id list instead of the legacy bitmask.
    // Only reached once a spell exists that no bit can encode, so profiles
    // written today are unchanged.
    explicit_spells=36
};
// Which spells a class may legitimately have stored at a level, given the
// capabilities its tag implies. This replaces a packed allow-mask, which could
// not express a spell beyond the 31st bit. Capability gates are monotonic, so
// one Cap per row is enough: a row requiring a later capability implicitly
// requires every earlier one. Cap::selected means "no gate beyond being a
// profile that stores spells at all".
struct SpellAccessRow { std::string_view klass,spell;unsigned min_level;Cap required; };
constexpr std::array class_spell_access{
    SpellAccessRow{"Cleric","cure_wounds",1,Cap::selected},
    SpellAccessRow{"Cleric","healing_word",1,Cap::selected},
    SpellAccessRow{"Cleric","blindness",3,Cap::selected},
    SpellAccessRow{"Cleric","sacred_flame",1,Cap::cleric_cantrips},
    // A spell with no legacy bit can only appear in an explicit-list profile,
    // which is what Cap::explicit_spells states. Every spell added from now on
    // gates here, so no new capability is needed per spell.
    SpellAccessRow{"Cleric","inflict_wounds",1,Cap::explicit_spells},
    SpellAccessRow{"Wizard","fire_bolt",1,Cap::selected},
    SpellAccessRow{"Wizard","magic_missile",1,Cap::selected},
    SpellAccessRow{"Wizard","scorching_ray",3,Cap::selected},
    SpellAccessRow{"Wizard","blindness",3,Cap::selected},
    SpellAccessRow{"Wizard","poison_spray",1,Cap::cantrips},
    SpellAccessRow{"Wizard","ray_of_frost",1,Cap::frost},
    SpellAccessRow{"Wizard","shocking_grasp",1,Cap::shocking},
    SpellAccessRow{"Wizard","chill_touch",1,Cap::chill},
    SpellAccessRow{"Warlock","eldritch_blast",1,Cap::warlock},
    SpellAccessRow{"Warlock","poison_spray",1,Cap::warlock_poison},
    SpellAccessRow{"Warlock","chill_touch",1,Cap::chill},
    SpellAccessRow{"Sorcerer","fire_bolt",1,Cap::sorcerer},
    SpellAccessRow{"Sorcerer","poison_spray",1,Cap::sorcerer},
    SpellAccessRow{"Sorcerer","ray_of_frost",1,Cap::sorcerer},
    SpellAccessRow{"Sorcerer","shocking_grasp",1,Cap::sorcerer},
    SpellAccessRow{"Sorcerer","chill_touch",1,Cap::chill}};
std::vector<std::string> allowed_spells(std::string_view klass,unsigned level,unsigned tag)
{
    std::vector<std::string> result;
    for(const auto& row:class_spell_access)
        if(row.klass==klass&&level>=row.min_level&&tag>=static_cast<unsigned>(row.required))
            result.emplace_back(row.spell);
    return result;
}
constexpr unsigned max_profile_tag=static_cast<unsigned>(Cap::explicit_spells);
// Zero for anything that is not a known PC tag, so the profile validation below
// rejects it exactly as the previous chain did. Untrusted input: an unknown
// tag must never be treated as "has everything".
unsigned profile_tag(std::string_view magic)
{
    if(!magic.starts_with("PC")||magic.size()<3||magic.size()>4)return 0;
    unsigned value=0;
    for(const char c:magic.substr(2)){if(c<'0'||c>'9')return 0;value=value*10+unsigned(c-'0');}
    return value>=1&&value<=max_profile_tag?value:0;
}
// Highest capability present decides the written tag. The source lists are in
// descending tag order, so taking the maximum matches the if-else chains these
// replaced.
unsigned highest_present(unsigned floor_tag,std::initializer_list<std::pair<unsigned,bool>> present)
{
    unsigned tag=floor_tag;
    for(const auto& [candidate,is_present]:present)if(is_present)tag=std::max(tag,candidate);
    return tag;
}
using Dice=detail::DamageDice;
struct Definition {
    int ac{}, hp{}, initiative{}, speed{}, melee_bonus{};
    Dice melee;
    int ranged_bonus{};
    int reach{5};
    Dice ranged;
    int range{}, long_range{}, winds{}, slots{}, casting{}, level{}, slots2{};
    // Known and prepared spells, by id. Not a bitmask: an int caps the catalog
    // at 31 spells and the level-four milestone needs 139.
    std::vector<std::string> spells;
    // Cantrip knowledge for display. Captured before equipment is applied,
    // because untrained armor clears `spells` while the character still knows
    // the cantrip -- knowledge persists while casting is unavailable.
    std::vector<std::string> known_cantrips;
    bool str_dex_disadvantage{},savage{},stealth_disadvantage{};
    std::string weapon_label;
    bool melee_heavy_disadvantage{},ranged_heavy_disadvantage{};
    std::array<int,6> saves{};
    unsigned weapon_hands{};
    int versatile_sides{};
    bool shield{};
    int hit_die{},constitution{},rushes{},surges{},arcane{};
    bool dwarf{},cunning{},tactical_mind{},champion{};
    unsigned sneak_level{};
    bool finesse{},ranged_weapon{};
    int medicine{};
    detail::DamageType melee_type{detail::DamageType::bludgeoning},ranged_type{detail::DamageType::bludgeoning};
    std::vector<detail::DamageAffinity> affinities;
    std::vector<std::string> equipment_keys;
};
bool legacy_two_hands(std::string_view key)
{return key=="quarterstaff"||key=="spear"||key=="battleaxe"||key=="trident";}
std::vector<GripOption> grip_options(const Definition& d)
{
    if(!d.versatile_sides)return {};
    return {{1,{"One hand — {dice}",{{"dice","1d"+std::to_string(d.melee.sides)}}},true},
        {2,{"Two hands — {dice}",{{"dice","1d"+std::to_string(d.versatile_sides)}}},!d.shield}};
}
void validate_grip(const Definition& d,unsigned hands)
{
    if(d.versatile_sides?(hands!=1&&hands!=2)||(hands==2&&d.shield):hands!=d.weapon_hands)
        throw std::runtime_error("This grip is incompatible with the equipped weapon or shield.");
}
struct CombatDisplay {const char* type;const char* melee;const char* ranged;};
CombatDisplay combat_display(std::string_view definition)
{
    if(definition=="slums-kobold")return {"Kobold","Dagger",nullptr};
    if(definition=="slums-kobold-leader")return {"Kobold Leader","Short sword","Short bow"};
    if(definition=="slums-kobold-leader-sword")return {"Kobold Leader","Short sword",nullptr};
    if(definition=="bandit")return {"Bandit","Scimitar","Light crossbow"};
    if(definition=="slums-goblin")return {"Goblin Guard","Short sword",nullptr};
    if(definition=="slums-goblin-leader")return {"Goblin Leader","Short sword",nullptr};
    if(definition=="slums-orc")return {"Orc",nullptr,nullptr};
    if(definition=="slums-orc-leader")return {"Orc Leader",nullptr,nullptr};
    if(definition=="slums-bugbear")return {"Bugbear",nullptr,nullptr};
    return {nullptr,nullptr,nullptr};
}
struct Content { Identity identity; std::vector<Identity> previous_campaign_identities;std::map<std::string,Definition> definitions; };
bool somatic_hand(const Definition& d)
{
    // Two-Handed/Versatile specifies hands when attacking (SRD p.90). A
    // weapon can be held in one hand while gesturing, retaining its attack
    // grip. A separate shield occupies the remaining hand; a wand is held too.
    return !d.shield||!d.weapon_hands;
}
struct Actor : detail::LifeState {
    Participant source;
    Definition definition;
    int initiative{}, movement{}, winds{}, slots{},slots2{};
    int hit_dice{},rushes{},surges{},dashes{},arcane{};
    bool rush_used{},surge_used{};
    detail::ActionBudget actions;
    bool bonus{true}, reaction{true}, dodge{}, disengaged{};
    bool spent_slot{},savage_used{},facing_left{};
    bool sneak_used{},aim_used{},aim_ready{},moved{};
    unsigned weapon_hands{};
    bool involuntary_overlap{}; // Interrupted in an occupied space; retained through recovery until separated.
    detail::EffectState effects;
    bool object_interaction{true};
};
bool unconscious(const Actor& a){return a.hp==0||a.effects.sleeping;}
bool conscious(const Actor& a){return !a.dead&&!unconscious(a);}
struct ArcaneAllocation { std::string_view id,label; unsigned first,second; };
constexpr std::array arcane_allocations{
    ArcaneAllocation{"arcane_recovery:1:0","One level-one spell slot",1,0},
    ArcaneAllocation{"arcane_recovery:2:0","Two level-one spell slots",2,0},
    ArcaneAllocation{"arcane_recovery:0:1","One level-two spell slot",0,1}
};
bool can_recover(const Actor& actor,const ArcaneAllocation& choice)
{
    const auto& d=actor.definition;
    return conscious(actor)&&d.arcane&&actor.arcane>0&&
        choice.first+2*choice.second<=unsigned((d.level+1)/2)&&
        choice.first<=unsigned(d.slots-actor.slots)&&choice.second<=unsigned(d.slots2-actor.slots2);
}
int movement_left(const Actor& a)
{
    if(!conscious(a)||a.aim_used)return 0;
    const int penalty=std::min(a.definition.speed,detail::speed_penalty(a.effects));
    return std::max(0,a.movement-penalty*(1+a.dashes));
}
struct ChampionMove {
    EntityId actor{},target{};
    int natural{},remaining{};
    bool spell{};
    Cell origin;
};
struct PendingCheck {
    EntityId actor{},target{};
    int natural{};
    bool surge_spent{};
};
struct PendingWeaponHit {
    EntityId attacker{},target{};
    bool ranged{};
    int natural{},mode{},first{};
    std::optional<int> second;
    unsigned thrown_item{};
    bool sneak_pending{},aimed{};
    int sneak_extra{};
};
int maximum_hit_points(int die,bool dwarf,std::span<const int> modifiers)
{
    if(modifiers.empty()||modifiers.size()>4||
        std::any_of(modifiers.begin(),modifiers.end(),[](int n){return n<-4||n>5;}))
        throw std::runtime_error("Invalid HP advancement history");
    int hp=die+modifiers.front()+(dwarf?int(modifiers.size()):0);
    for(std::size_t i=1;i<modifiers.size();++i){
        const int increase=modifiers[i]-modifiers[i-1];
        if(increase<0||increase>1||(i!=3&&increase))throw std::runtime_error("Invalid Constitution advancement history");
        // SRD p. 23: gain HP first, then apply a new modifier per attained level.
        hp+=std::max(1,die/2+1+modifiers[i-1])+increase*int(i+1);
    }
    return hp;
}
// Versioned, module-owned character recipe. Original item IDs never enter this layer.
Definition character_definition(std::string_view bytes,std::optional<std::span<const std::string>> equipment_override=std::nullopt)
{
    if(bytes.size()>8192)throw std::runtime_error("Character profile exceeds limit");
    std::istringstream in{std::string(bytes)};
    std::string magic,klass,race;std::array<int,6> scores{};unsigned count{};
    unsigned level=1,features=0,selected_spells=0;in>>magic;
    const unsigned tag=profile_tag(magic);
    const auto has=[tag](Cap capability){return tag>=static_cast<unsigned>(capability);};
    const bool with_choices=has(Cap::choices);
    const bool with_scholar=has(Cap::scholar);
    const bool with_arcane=has(Cap::arcane);
    const bool with_champion=has(Cap::champion);
    const bool with_mind=has(Cap::mind);
    const bool with_chill=has(Cap::chill);
    const bool with_sorcerer=has(Cap::sorcerer);
    const bool with_warlock_poison=has(Cap::warlock_poison);
    const bool with_shocking=has(Cap::shocking);
    const bool with_warlock=has(Cap::warlock);
    const bool with_cunning=has(Cap::cunning);
    const bool with_gaming=has(Cap::gaming);
    const bool with_herbalism=has(Cap::herbalism);
    const bool with_monk_tools=has(Cap::monk_tools);
    const bool with_instruments=has(Cap::instruments);
    const bool with_class_skills=has(Cap::class_skills);
    const bool with_styles=has(Cap::styles);
    const bool with_archery=has(Cap::archery);
    const bool with_backgrounds=has(Cap::backgrounds);
    const bool with_sage=has(Cap::sage);
    const bool with_frost=has(Cap::frost);
    const bool with_surge=has(Cap::surge);
    const bool with_cleric_cantrips=has(Cap::cleric_cantrips);
    const bool with_cantrips=has(Cap::cantrips);
    const bool with_spells=has(Cap::spells);
    const bool with_training=has(Cap::training);
    const bool selected=has(Cap::selected);
    if(magic=="PC2"||selected)in>>level;
    std::vector<std::string> stored_spells;
    if(selected){
        in>>features;
        if(has(Cap::explicit_spells)){
            unsigned listed{};in>>listed;
            // Untrusted input: bound the list and require every id to be a
            // supported spell before anything else looks at it.
            if(!in||listed>detail::spell_table.size())throw std::runtime_error("Invalid character profile");
            for(unsigned n=0;n<listed;++n){
                std::string id;in>>id;
                if(!in||!detail::find_spell(id)||detail::knows_spell(stored_spells,id))
                    throw std::runtime_error("Invalid character profile");
                stored_spells.push_back(std::move(id));
            }
        }else{
            in>>selected_spells;
            stored_spells=detail::spells_from_mask(selected_spells);
            // Every bit must round-trip, so a bit that maps to no spell is
            // rejected rather than silently dropped.
            if(detail::mask_from_spells(stored_spells)!=selected_spells)
                throw std::runtime_error("Invalid character profile");
        }
    }
    in>>std::quoted(klass)>>std::quoted(race);
    // Rogue attack progression is a class marker the writer encoded in the tag:
    // it only ever emits PC35 for a Rogue. Now that a higher tag exists, the
    // capability alone no longer implies the class, so take it from the class.
    // The exact-PC35 rejection further down preserves the original contract.
    const bool with_rogue=has(Cap::rogue)&&klass=="Rogue";
    for(auto& score:scores)in>>score;
    if(!in||(magic!="PC1"&&magic!="PC2"&&!selected)||level<1||level>(selected?4u:2u)||features>(with_archery?7u:3u)||selected_spells>(with_chill?4095u:with_shocking?2047u:with_warlock?1023u:with_frost?511u:with_cleric_cantrips?255u:with_cantrips?127u:63u)||std::any_of(scores.begin(),scores.end(),[](int n){return n<3||n>20;}))
        throw std::runtime_error("Invalid character profile");
    if(level>1&&klass!="Fighter"&&klass!="Cleric"&&klass!="Wizard"&&!(with_cunning&&klass=="Rogue"&&(level==2||with_rogue)))throw std::runtime_error("Advancement is unsupported for this class");
    const auto races=character_rules()->choices(CreationField::race);
    if(std::none_of(races.begin(),races.end(),[&](const auto& r){return r.label==race;}))throw std::runtime_error("Unknown species");
    const int str=ability_modifier(scores[0]),dex=ability_modifier(scores[1]),con=ability_modifier(scores[2]);
    std::vector<int> hp_modifiers(level,con);
    if(magic=="PC4"||magic=="PC5"||magic=="PC6"||with_training)for(auto& modifier:hp_modifiers)in>>modifier;
    in>>count;
    if(!in||count>3||hp_modifiers.back()!=con||((features&1)&&!with_styles&&hp_modifiers.front()!=con))
        throw std::runtime_error("Invalid character HP history or equipment count");
    const auto classes=character_rules()->choices(CreationField::character_class);
    if(std::none_of(classes.begin(),classes.end(),[&](const auto& c){return c.label==klass;}))throw std::runtime_error("Unknown class");
    const int die=klass=="Barbarian"?12:(klass=="Fighter"||klass=="Paladin"||klass=="Ranger")?10:(klass=="Wizard"||klass=="Sorcerer")?6:8;
    Definition d;d.hp=maximum_hit_points(die,race=="Dwarf",hp_modifiers);d.hit_die=die;d.constitution=con;d.dwarf=race=="Dwarf";d.rushes=race=="Orc"?2+(level-1)/4:0;
    const auto trained_saves=detail::class_save_proficiencies(klass);
    for(unsigned i=0;i<6;++i)d.saves[i]=ability_modifier(scores[i])+((i==trained_saves[0]||i==trained_saves[1])?2:0);
    d.ac=10+dex;d.initiative=dex;d.speed=race=="Goliath"?35:30;d.level=level;
    d.melee_bonus=2+str;d.melee={0,0,std::max(0,1+str)};
    d.champion=with_champion&&klass=="Fighter"&&level>=3;
    d.arcane=with_arcane&&klass=="Wizard"?1:0;
    d.medicine=ability_modifier(scores[4]);d.tactical_mind=with_mind&&klass=="Fighter"&&level>=2;
    d.cunning=with_cunning&&klass=="Rogue"&&level>=2;
    if(tag==static_cast<unsigned>(Cap::rogue)&&klass!="Rogue")throw std::runtime_error("Rogue profile requires Rogue class");
    d.sneak_level=with_rogue?level:0;
    d.surges=with_surge&&klass=="Fighter"&&level>=2?1:0;
    d.winds=klass=="Fighter"?(level==4?3:2):0;d.slots=(klass=="Cleric"||klass=="Wizard")?(level==1?2:level==2?3:4):0;
    d.slots2=(klass=="Cleric"||klass=="Wizard")&&level>=3?(level==3?2:3):0;
    d.casting=2+ability_modifier(scores[klass=="Cleric"?4:(klass=="Warlock"||(with_sorcerer&&klass=="Sorcerer"))?5:3]);    // Pre-selection defaults for the oldest profiles, which stored no choices.
    if(klass=="Cleric")d.spells={"cure_wounds"};
    else if(klass=="Wizard")d.spells={"fire_bolt","magic_missile"};
    if(selected){
        const auto allowed=allowed_spells(klass,level,tag);
        const bool eligible=std::all_of(stored_spells.begin(),stored_spells.end(),
            [&](const auto& id){return detail::knows_spell(allowed,id);});
        if(!eligible||(features&1)&&klass!="Fighter")throw std::runtime_error("Invalid prepared spells or feat prerequisites");
        d.spells=stored_spells;d.savage=(features&2)!=0;
    }
    d.known_cantrips=detail::spells_of_level(d.spells,true);
    
    bool weapon=false,armor=false,shield=false;unsigned hands=0;
    for(unsigned i=0;i<count;++i){std::string key;in>>std::quoted(key);d.equipment_keys.push_back(std::move(key));}
    if(equipment_override)d.equipment_keys.assign(equipment_override->begin(),equipment_override->end());
    for(const auto& key:d.equipment_keys){
        if(const auto* item=detail::weapon(key)){
            if(weapon)throw std::runtime_error("Only one weapon may be equipped");weapon=true;d.weapon_label=item->label;d.finesse=item->finesse;d.ranged_weapon=item->ranged;
            d.weapon_hands=magic!="PC5"&&magic!="PC6"&&magic!="PC7"&&magic!="PC8"&&magic!="PC9"&&!with_spells&&legacy_two_hands(key)?2:item->hands;
            d.versatile_sides=item->versatile_sides;hands+=d.weapon_hands;
            const int modifier=item->finesse?std::max(str,dex):item->ranged?dex:str;
            const int bonus=(trained(klass,key)?2:0)+modifier;
            if(item->dice&&!item->ranged){d.melee_bonus=bonus;d.melee={item->dice,item->sides,modifier};d.melee_type=item->type;d.reach=item->reach;d.melee_heavy_disadvantage=item->heavy_disadvantage(scores);}
            if(item->range){d.ranged_bonus=bonus+((item->ranged&&(features&4))?2:0);d.ranged={item->dice,item->sides,item->fixed_damage?item->fixed_damage:modifier};d.ranged_type=item->type;d.range=item->range;d.long_range=item->long_range;d.ranged_heavy_disadvantage=item->heavy_disadvantage(scores);}
        }else if(const auto* item=detail::armor(key);item&&item->category!=detail::ArmorCategory::shield){
            if(armor)throw std::runtime_error("Only one armor may be equipped");
            if(!trained(klass,key)){d.str_dex_disadvantage=true;d.spells.clear();}
            armor=true;d.ac=item->base_ac+item->dexterity_contribution(dex);
            d.stealth_disadvantage=item->stealth_disadvantage;
            if(scores[0]<item->strength)d.speed-=10;
        }else if(key=="shield"){
            if(shield)throw std::runtime_error("Only one shield may be equipped");shield=true;++hands;
        }else throw std::runtime_error("Unsupported equipment conversion: "+key);
    }
    d.shield=shield;
    if(magic=="PC5"||magic=="PC6"||with_training){
        unsigned requested{};in>>requested;
        if(!in)throw std::runtime_error("Invalid character grip");
        if(equipment_override)requested=0;
        if(requested){validate_grip(d,requested);hands=hands-d.weapon_hands+requested;d.weapon_hands=requested;}
    }
    if(magic=="PC6"||with_training){
        std::string background;in>>std::quoted(background);
        const auto grants=detail::read_grants(in);
        if(with_training){const auto training=detail::training_profile(grants,detail::grant_source_id(klass),background,level,scores,with_scholar?detail::TrainingPolicy::scholar:with_gaming?detail::TrainingPolicy::soldier_gaming:with_herbalism?detail::TrainingPolicy::druid_herbalism:with_monk_tools?detail::TrainingPolicy::monk_tools:with_instruments?detail::TrainingPolicy::bard_instruments:with_class_skills?detail::TrainingPolicy::class_skills:with_styles?detail::TrainingPolicy::fighter_style:with_backgrounds?detail::TrainingPolicy::all_backgrounds:with_sage?detail::TrainingPolicy::sage:detail::TrainingPolicy::legacy);d.medicine=std::find_if(training.skills.begin(),training.skills.end(),[](const auto& skill){return skill.id=="medicine";})->bonus;}
        if(with_spells){
            std::vector<std::string> prepared;
            if(klass=="Wizard")prepared=detail::spells_of_level(stored_spells,false);
            if(!with_choices&&klass=="Wizard"&&(std::none_of(grants.begin(),grants.end(),[](const auto& g){return g.id=="spell:magic_missile"&&g.level==1;})||std::any_of(grants.begin(),grants.end(),[](const auto& g){return detail::is_spell_grant(g)&&g.choices.contains("learned_at");})))throw std::runtime_error("Invalid legacy Wizard knowledge");
            const auto access=detail::spell_access(grants,klass,level,prepared);
            // Compare as sets: the grants and the stored list must describe the
            // same spells, independent of the order either was written in.
            const auto same_spells=[](std::vector<std::string> left,std::vector<std::string> right){
                std::sort(left.begin(),left.end());std::sort(right.begin(),right.end());
                return left==right;
            };
            const auto stored_cantrips=[&]{return detail::spells_of_level(stored_spells,true);};
            if(klass=="Sorcerer"&&((!with_sorcerer&&!access.cantrips.empty())||!same_spells(detail::known_cantrip_ids(access),stored_spells)))
                throw std::runtime_error("Character cantrip access disagrees with Sorcerer grants");
            if(klass=="Warlock"&&((!with_warlock&&!access.cantrips.empty())||!same_spells(detail::known_cantrip_ids(access),stored_spells)))
                throw std::runtime_error("Character cantrip access disagrees with Warlock grants");
            if(klass=="Cleric"&&((!with_cleric_cantrips&&!access.cantrips.empty())||!same_spells(detail::known_cantrip_ids(access),stored_cantrips())))
                throw std::runtime_error("Character cantrip access disagrees with Cleric grants");
            if(!with_cantrips&&klass=="Wizard"&&
                (std::any_of(access.cantrips.begin(),access.cantrips.end(),[](const auto& c){return c.id!="fire_bolt";})||
                 std::none_of(access.cantrips.begin(),access.cantrips.end(),[](const auto& c){return c.id=="fire_bolt"&&c.acquired_level==1;})))
                throw std::runtime_error("Legacy character recipe cannot contain changed cantrip choices");
            if(klass=="Wizard"&&!same_spells(detail::wizard_casting_ids(access),stored_spells))
                throw std::runtime_error("Character casting access disagrees with spell grants");
        }else if(std::any_of(grants.begin(),grants.end(),detail::is_spell_grant))
            throw std::runtime_error("Legacy character recipe cannot contain new spell grants");
        const auto features_only=detail::without_spell_grants(with_training?detail::without_training(grants):grants);
        const auto effects=detail::validate_grants(features_only,detail::grant_source_id(klass),detail::grant_source_id(race),background,level,magic=="PC8"||magic=="PC9"||with_spells,magic=="PC9"||with_spells,with_surge,with_archery,with_styles,with_mind,with_champion,with_arcane,with_rogue);
        if(effects.feats!=features)throw std::runtime_error("Character effects disagree with acquired grants");
        const int initial_con=ability_modifier(scores[2]-effects.abilities[2]);
        for(unsigned i=0;i<level;++i)if(hp_modifiers[i]!=(i==3?con:initial_con))
            throw std::runtime_error("HP history disagrees with acquired ability choices");
    }
    if(race=="Dwarf")d.affinities.push_back({detail::AffinityKind::resistance,detail::DamageType::poison,"species:dwarf/trait:dwarven_resilience"});
    if(hands>2)throw std::runtime_error("Not enough free hands. Unequip the shield or two-handed weapon first.");
    if(!armor&&klass=="Barbarian")d.ac=std::max(d.ac,10+dex+con);
    if(!armor&&!shield&&klass=="Monk")d.ac=std::max(d.ac,10+dex+ability_modifier(scores[4]));
    if(shield&&trained(klass,"shield"))d.ac+=2;
    if(armor&&(features&1))++d.ac;
    in>>std::ws;if(!in.eof())throw std::runtime_error("Invalid character profile fields");return d;
}
void restore_vitals(Actor& a,const VitalState& state)
{
    a.hp=state.hit_points;a.dead=state.dead;
    // Prior vital formats predate spendable Hit Dice, so their dice are unspent.
    a.hit_dice=a.definition.hit_die?a.definition.level:0;
    a.rushes=a.definition.rushes;a.surges=a.definition.surges;a.arcane=a.definition.arcane;
    bool timed=false,temporary=false;
    if(!state.resources.empty()) {
        std::istringstream in(state.resources);std::string magic;
        in>>magic>>a.winds>>a.slots;temporary=magic=="SRD6"||magic=="SRD7"||magic=="SRD8"||magic=="SRD9";timed=magic=="SRD5"||temporary;if(magic=="SRD2"||magic=="SRD3"||magic=="SRD4"||timed)in>>a.slots2;
        in>>a.successes>>a.failures>>a.stable;
        if(magic=="SRD4"||timed)in>>a.hit_dice;
        if(timed){in>>a.recovery.death_save_in_ms>>a.recovery.stable_recovery_in_ms;detail::decode_stable_recovery(a.recovery);}
        if(temporary)in>>a.temporary_hp.amount>>std::quoted(a.temporary_hp.source_id);
        if(magic=="SRD7"||magic=="SRD8"||magic=="SRD9")in>>a.rushes;
        if(magic=="SRD8"||magic=="SRD9")in>>a.surges;
        if(magic=="SRD9")in>>a.arcane;
        if(!in||(magic!="SRD1"&&magic!="SRD2"&&magic!="SRD3"&&magic!="SRD4"&&!timed))throw std::runtime_error("Invalid character resource state");
        if(magic=="SRD3"||magic=="SRD4"||timed)a.effects=detail::read_effects(in);
        in>>std::ws;if(!in.eof())throw std::runtime_error("Trailing character resource state");
    }
    const auto& d=a.definition;
    if(a.effects.sleeping&&(a.dead||a.hp<=0))throw std::runtime_error("Invalid naturally sleeping vitality");
    if(a.hp<0||a.hp>d.hp||(a.dead&&a.hp!=0)||a.winds<0||a.winds>d.winds||a.slots<0||a.slots>d.slots||
        a.arcane<0||a.arcane>d.arcane||a.slots2<0||a.slots2>d.slots2||a.surges<0||a.surges>d.surges||a.rushes<0||a.rushes>d.rushes||a.hit_dice<0||a.hit_dice>(d.hit_die?d.level:0)||
        a.successes<0||a.successes>3||a.failures<0||a.failures>4)throw std::runtime_error("Invalid character vitals");
    // Earlier campaigns could retain completed counters after stabilization.
    if(a.stable)a.successes=a.failures=0;
    if(!timed)detail::initialize_legacy_recovery(a);
    detail::validate_recovery(a);detail::validate_temporary_hp(a.temporary_hp);
    if(a.recovery.stable_recovery_due&&!detail::healing_blocked(a.effects))throw std::runtime_error("Earned recovery requires active healing prevention");
}
VitalState vitals(const Actor& a)
{
    const bool arcane=a.arcane<a.definition.arcane,surge=arcane||a.surges<a.definition.surges,rush=surge||a.definition.rushes>0,temporary=rush||a.temporary_hp.amount>0,timed=temporary||(a.hp==0&&!a.dead);
    const bool effects=a.effects.next_id!=1||a.effects.prone,spent_dice=a.hit_dice<(a.definition.hit_die?a.definition.level:0);
    // Preserve the compact previous format when every Hit Die is available.
    std::ostringstream out;out<<(arcane?"SRD9 ":surge?"SRD8 ":rush?"SRD7 ":temporary?"SRD6 ":timed?"SRD5 ":spent_dice?"SRD4 ":effects?"SRD3 ":a.definition.slots2?"SRD2 ":"SRD1 ")<<a.winds<<' '<<a.slots<<' ';
    if(timed||spent_dice||effects||a.definition.slots2)out<<a.slots2<<' ';out<<a.successes<<' '<<a.failures<<' '<<a.stable;
    if(timed||spent_dice)out<<' '<<a.hit_dice;
    if(timed)out<<' '<<a.recovery.death_save_in_ms<<' '<<detail::encode_stable_recovery(a.recovery);
    if(temporary)out<<' '<<a.temporary_hp.amount<<' '<<std::quoted(a.temporary_hp.source_id);
    if(rush)out<<' '<<a.rushes;
    if(surge)out<<' '<<a.surges;
    if(arcane)out<<' '<<a.arcane;
    if(timed||spent_dice||effects){out<<' ';detail::write_effects(out,a.effects);}
    std::string description;
    if(a.definition.slots)description="Level-one spell slots: "+std::to_string(a.slots)+" / "+std::to_string(a.definition.slots);
    if(a.definition.slots2)description+="\nLevel-two spell slots: "+std::to_string(a.slots2)+" / "+std::to_string(a.definition.slots2);
    if(a.definition.winds)description="Second Wind uses: "+std::to_string(a.winds)+" / "+std::to_string(a.definition.winds);
    if(a.definition.arcane)description+=(description.empty()?"":"\n")+std::string("Arcane Recovery uses: ")+std::to_string(a.arcane)+" / 1";
    if(a.definition.surges&&surge)description+=(description.empty()?"":"\n")+std::string("Action Surge uses: ")+std::to_string(a.surges)+" / "+std::to_string(a.definition.surges);
    if(a.hp==0)description+=(description.empty()?"":"\n")+std::string(a.dead?"Dead":a.stable?"Stable, unconscious":"Unconscious; death saves ")+(!a.dead&&!a.stable?std::to_string(a.successes)+" successes, "+std::to_string(a.failures)+" failures":"");
    if(detail::healing_blocked(a.effects))description+="\nChill Touch: cannot regain HP.";
    if(detail::opportunity_blocked(a.effects))description+="\nShocking Grasp: cannot make Opportunity Attacks.";
    if(detail::speed_penalty(a.effects))description+="\nRay of Frost: Speed reduced by 10 feet.";
    if(detail::blinded(a.effects))description+="\nBlinded";
    if(a.effects.sleeping)description+="\nNaturally asleep";
    if(a.effects.prone)description+="\nProne";
    return {a.hp,a.dead,out.str(),description};
}
int distance(Cell a,Cell b) { return std::max(std::abs(a.x-b.x),std::abs(a.y-b.y))*5; }
bool same_command(const Command& a,const Command& b)
{ return a.revision==b.revision && a.actor==b.actor && a.target==b.target && a.verb==b.verb && a.destination==b.destination && a.item==b.item; }
bool turns_to_attack(std::string_view verb)
{
    // A caster turns toward a foe, not toward an ally being healed, so every
    // spell except the ally-targeted ones counts alongside weapon attacks.
    if(verb=="throw"||verb=="melee"||verb=="ranged")return true;
    const auto* spell=detail::find_spell(verb);
    return spell&&spell->target!=detail::SpellTarget::wounded_ally;
}
class Session final : public CombatSession {
public:
    Session(std::shared_ptr<const Content> content,Encounter encounter,std::uint64_t seed,bool restoring=false)
        : content_(std::move(content)), board_(std::move(encounter.battlefield)), rng_(seed), scope_(encounter.scope)
    {
        if(!scope_)throw std::runtime_error("Invalid encounter scope");
        detail::validate_battlefield(board_);
        if (encounter.participants.size()<2 || encounter.participants.size()>64) throw std::runtime_error("Invalid encounter size");
        std::set<EntityId> ids;std::set<Cell> cells;std::set<unsigned> sides;
        for (auto& p:encounter.participants) {
            if (!p.id || !ids.insert(p.id).second || (!cells.insert(p.cell).second&&!restoring) || board_.at(p.cell)==1 ||
                p.side>1 || p.name.empty() || p.name.size()>160 || (p.character_profile.empty()&&!content_->definitions.contains(p.definition)))
                throw std::runtime_error("Invalid participant or unsupported rules definition: "+p.definition);
            sides.insert(p.side);
            const auto d=p.character_profile.empty()?content_->definitions.at(p.definition):character_definition(p.character_profile);
            Actor a; a.definition=d;a.weapon_hands=d.weapon_hands;a.source=std::move(p);a.hp=d.hp;a.winds=d.winds;a.slots=d.slots;a.slots2=d.slots2;a.hit_dice=d.hit_die?d.level:0;a.rushes=d.rushes;a.surges=d.surges;a.arcane=d.arcane;
            a.facing_left=a.source.facing_left;
            if(a.source.state)restore_vitals(a,*a.source.state);
            a.initiative=detail::d20({d.champion,d.str_dex_disadvantage||a.source.surprised||a.effects.sleeping},rng_)+d.initiative;a.movement=d.speed;
            actors_.push_back(std::move(a));
        }
        if (sides.size()!=2) throw std::runtime_error("Encounter needs both sides");
        // Fixed tie adjudication: descending initiative, then stable entity ID.
        std::stable_sort(actors_.begin(),actors_.end(),[](const Actor& a,const Actor& b){
            return a.initiative!=b.initiative?a.initiative>b.initiative:a.source.id<b.source.id;
        });
        // Recovery checks follow the target's new initiative in a new encounter.
        // Exact timers in a restored encounter are installed after construction.
        for(std::size_t i=0;i<actors_.size();++i){
            for(auto& effect:actors_[i].effects.active)if(effect.kind==detail::EffectKind::blindness)effect.save_in_ms=turn_end_ms(i);
            if(actors_[i].hp==0&&!actors_[i].dead&&!actors_[i].stable)
                actors_[i].recovery.death_save_in_ms=i?turn_end_ms(i-1):0;
        }
        log("Combat begins. Each square is 5 feet.");update_outcome();
        frost_movement_=std::any_of(actors_.begin(),actors_.end(),[](const auto& a){return a.definition.cunning||(detail::knows_spell(a.definition.known_cantrips,"ray_of_frost"))||detail::speed_penalty(a.effects);});
        if(!restoring){
            for(const auto& a:actors_){
                for(const auto& key:a.definition.equipment_keys)if(const auto* w=detail::weapon(key);w&&w->thrown)physical_inventory_=true;
                for(const auto& item:a.source.inventory)if(const auto* w=detail::weapon(item.definition);w&&w->thrown)physical_inventory_=true;
            }
            if(physical_inventory_)initialize_items();
        }
        if(!restoring)for(auto& a:actors_){
            if(unconscious(a))drop_held(a);
            if(a.source.ground_equipment.empty())continue;
            initialize_items();std::set<unsigned> seen;
            for(const auto index:a.source.ground_equipment){
                const auto item=std::find_if(items_.begin(),items_.end(),[&](const auto& i){return i.origin==a.source.id&&i.equipment_index==index;});
                if(!seen.insert(index).second||item==items_.end())throw std::runtime_error("Invalid initial ground equipment");
                ground_one(item->id,a.source.cell);
            }
            a.definition=equipped_definition(a,items_);a.weapon_hands=a.definition.weapon_hands;
        }
        if(!restoring&&outcome_==Outcome::ongoing&&!begin_turn())end_turn();
    }
    Snapshot snapshot() const override;
    SafeRecovery safe_recovery() const override;
    std::vector<Command> legal_commands() const override;
    std::vector<Cell> movement_reach(EntityId actor) const override;
    bool submit(const Command& command) override;
    std::string save() const override;
    static std::unique_ptr<Session> restore(std::shared_ptr<const Content> content,std::string_view bytes);
private:
    std::shared_ptr<const Content> content_;
    Battlefield board_;
    std::vector<Actor> actors_;
    std::uint64_t rng_{}, revision_{1};
    std::uint64_t scope_{1}, elapsed_ms_{};
    unsigned turn_{}, round_{1};
    Outcome outcome_{Outcome::ongoing};
    std::optional<TemporaryHitPoints> temporary_offer_;
    std::optional<PendingWeaponHit> weapon_hit_;
    std::optional<PendingCheck> check_choice_;
    std::optional<ChampionMove> champion_move_;
    void finish_champion_move();
    void validate_champion_move() const;
    void finish_check(const PendingCheck& check,int boost);
    void validate_check() const;
    bool frost_movement_{}, items_active_{}, physical_inventory_{};
    Actor thrown_actor(const Actor&,std::string_view weapon) const;
    Actor hit_actor(const PendingWeaponHit&) const;
    unsigned ground_one(unsigned item,Cell cell);
    Message throw_label(const Actor&,const HeldItemView&) const;
    void throw_weapon(Actor&,Actor&,unsigned item);
    std::vector<HeldItemView> items_;
    void initialize_items();
    void drop_held(Actor& a);
    Definition equipped_definition(const Actor& a,const std::vector<HeldItemView>& items) const;
    bool can_pick_up(const Actor& a,const HeldItemView& item) const;
    std::vector<std::string> log_;
    std::vector<Message> log_messages_;
    std::vector<Cell> path_;
    std::size_t path_index_{};
    std::vector<EntityId> reactors_;
    std::size_t reactor_index_{};
    const Definition& def(const Actor& a) const {return a.definition;}
    const Actor& actor(EntityId id) const { return *std::find_if(actors_.begin(),actors_.end(),[&](const auto& a){return a.source.id==id;}); }
    Actor& actor(EntityId id) { return *std::find_if(actors_.begin(),actors_.end(),[&](const auto& a){return a.source.id==id;}); }
    int roll(int sides) {return roll_die(rng_,sides);}
    int dice(Dice d,bool critical=false) {return detail::roll_damage(rng_,d,critical);}
    void log(std::string english, Message message={}) {
        if(message.source.empty())message.source=english;
        if(log_.size()==80){log_.erase(log_.begin());log_messages_.erase(log_messages_.begin());}
        log_.push_back(std::move(english));log_messages_.push_back(std::move(message));
    }
    bool line_of_sight(Cell a,Cell b) const { return detail::has_line_of_sight(board_,a,b); }
    bool can_see(const Actor& a,const Actor& b) const {return conscious(a)&&!detail::blinded(a.effects)&&line_of_sight(a.source.cell,b.source.cell);}
    unsigned turn_end_ms(std::size_t index) const {return unsigned((index+1)*detail::round_ms/actors_.size());}
    bool shares_occupied_space(const Actor& who) const {
        return std::any_of(actors_.begin(),actors_.end(),[&](const auto& other){
            return other.source.id!=who.source.id&&!other.dead&&other.source.cell==who.source.cell;
        });
    }
    void clear_departed_overlaps() {
        for(auto& a:actors_)if(a.dead||!shares_occupied_space(a))a.involuntary_overlap=false;
    }
    // Emits every table spell whose row matches `scope` and pass. Called once
    // per original offer position so the observable order is unchanged.
    void offer_spells(std::vector<Command>& commands,const Actor& a,const Actor& other,int feet,
                      detail::SpellTarget scope,bool bonus_pass) const;
    // Resolves one table spell. `upcast` is the "_2" level-two slot form.
    void resolve_spell(const detail::SpellDef& spell,bool upcast,Actor& a,EntityId target_id);
    void apply_rider(const detail::SpellDef& spell,Actor& a,Actor& target,int dc);
    unsigned next_save_ms(EntityId target) const;
    unsigned next_turn_ms(const Actor& target) const;
    void advance_turn_time();
    void log_save(const Actor& target,const detail::SaveResult& result);
    bool saving_throw_succeeds(const Actor& target,detail::Ability ability,int dc);
    detail::MovementGrid movement_grid(const Actor& mover) const;
    std::vector<Cell> path_to(const Actor& a,Cell destination) const;
    EntityId pending() const {return reactor_index_<reactors_.size()?reactors_[reactor_index_]:0;}
    bool critical_hit(const Actor& a,const Actor& target,int natural,bool spell=false) const {return natural==20||(!spell&&def(a).champion&&natural==19)||(unconscious(target)&&distance(a.source.cell,target.source.cell)<=5);}
    bool attack(Actor& a,Actor& target,bool ranged,bool spell=false,Dice spell_dice={1,10,0},detail::DamageType spell_type=detail::DamageType::fire);
    detail::RollModifiers attack_modifiers(const Actor& a,const Actor& target,bool ranged,bool spell) const;
    Dice weapon_dice(const Actor& a,bool ranged) const;
    void apply_hit(Actor& a,Actor& target,int natural,int bonus,int mode,int amount,bool savage,detail::DamageType type,bool spell=false);
    void resolve_weapon_hit(int amount);
    bool sneak_eligible(const Actor& a,const Actor& target,bool ranged,int mode) const;
    void finish_reaction();
    void validate_weapon_hit() const;
    int resolved_damage(const Actor& target,detail::DamageType type,int amount);
    void damage(Actor& target,int amount,bool critical=false);
    void heal(Actor& target,int amount);
    void update_outcome();
    bool begin_turn();
    void end_turn();
    void progress_movement();
    void restore_movement(std::istream& input);
    void validate_restored_state(bool legacy_facing_reaction=false) const;
    void validate_pending_movement() const;
    void validate_legacy_facing_reaction() const;
    void restore_log(std::istream& input);
};

void Session::initialize_items()
{
    if(items_active_)return;
    std::vector<const Actor*> ordered;for(const auto& a:actors_)ordered.push_back(&a);
    std::sort(ordered.begin(),ordered.end(),[](auto a,auto b){return a->source.id<b->source.id;});
    for(const auto* a:ordered){
        std::set<std::uint64_t> ids;
        for(const auto& source:a->source.inventory)if(!source.inventory_id||!source.quantity||!ids.insert(source.inventory_id).second)throw std::runtime_error("Invalid carried item source");
        for(unsigned i=0;i<a->definition.equipment_keys.size();++i){
            const auto& key=a->definition.equipment_keys[i];if(key!="shield"&&!detail::weapon(key))continue;
            HeldItemView item{unsigned(items_.size()+1),a->source.id,a->source.id,i,key,{key=="shield"?"Shield":std::string(detail::weapon(key)->label),{}},{}};
            if(physical_inventory_)for(const auto& source:a->source.inventory)if(source.equipment_index==static_cast<int>(i)){
                if(source.definition!=key||item.inventory_id)throw std::runtime_error("Carried item disagrees with equipped source");
                item.inventory_id=source.inventory_id;item.quantity=source.quantity;
            }
            items_.push_back(std::move(item));
        }
        if(physical_inventory_)for(const auto& source:a->source.inventory)if(source.equipment_index<0&&detail::weapon(source.definition))
            items_.push_back({unsigned(items_.size()+1),a->source.id,a->source.id,0,source.definition,{std::string(detail::weapon(source.definition)->label),{}},{},source.inventory_id,source.quantity,true});
    }
    items_active_=true;
    // Older checkpoints can contain already-unconscious equipment holders.
    // Once a drop activates the ledger, reconcile every holder so that the
    // resulting checkpoint obeys the same invariant as a fresh encounter.
    for(auto& a:actors_)if(unconscious(a))drop_held(a);
}
Definition Session::equipped_definition(const Actor& a,const std::vector<HeldItemView>& items) const
{
    if(a.source.character_profile.empty())throw std::runtime_error("Equipment exchange requires a character equipment profile");
    const auto original=character_definition(a.source.character_profile);
    std::vector<std::string> keys;
    for(const auto& key:original.equipment_keys)if(key!="shield"&&!detail::weapon(key))keys.push_back(key);
    for(const auto& item:items)if(item.holder==a.source.id&&!item.stowed)keys.push_back(item.definition);
    return character_definition(a.source.character_profile,std::span<const std::string>(keys));
}
void Session::drop_held(Actor& a)
{
    if(a.source.character_profile.empty())return;
    initialize_items();bool changed=false;
    const auto count=items_.size();for(unsigned i=0;i<count;++i)if(items_[i].holder==a.source.id&&!items_[i].stowed){
        ground_one(items_[i].id,a.source.cell);changed=true;
    }
    if(changed){a.definition=equipped_definition(a,items_);a.weapon_hands=a.definition.weapon_hands;}
}
Actor Session::thrown_actor(const Actor& a,std::string_view weapon) const
{
    auto result=a;std::vector<std::string> keys;
    for(const auto& key:a.definition.equipment_keys)if(!detail::weapon(key))keys.push_back(key);
    keys.emplace_back(weapon);
    result.definition=character_definition(a.source.character_profile,std::span<const std::string>(keys));
    result.weapon_hands=1;return result;
}
Actor Session::hit_actor(const PendingWeaponHit& hit) const
{
    const auto& a=actor(hit.attacker);
    if(!hit.thrown_item)return a;
    if(!physical_inventory_||hit.thrown_item>items_.size()||!hit.ranged)throw std::runtime_error("Invalid pending thrown item");
    const auto& item=items_[hit.thrown_item-1];const auto* weapon=detail::weapon(item.definition);
    if(item.holder||item.quantity!=1||!weapon||!weapon->thrown||item.cell!=actor(hit.target).source.cell)
        throw std::runtime_error("Invalid pending thrown weapon position");
    return thrown_actor(a,item.definition);
}
unsigned Session::ground_one(unsigned token,Cell cell)
{
    auto& item=items_.at(token-1);
    if(item.quantity>1){
        auto unit=item;--item.quantity;item.stowed=true;item.cell={};
        unit.id=unsigned(items_.size()+1);unit.quantity=1;unit.holder=0;unit.stowed=false;unit.cell=cell;
        items_.push_back(std::move(unit));return unsigned(items_.size());
    }
    item.holder=0;item.stowed=false;item.cell=cell;return token;
}
Message Session::throw_label(const Actor& a,const HeldItemView& item) const
{
    Message label{"{weapon} ×{count}",{{"weapon",item.label.source,true},{"count",std::to_string(item.quantity)}}};
    const bool shield=std::any_of(items_.begin(),items_.end(),[&](const auto& i){return i.holder==a.source.id&&!i.stowed&&i.definition=="shield";});
    if(item.stowed&&shield)for(const auto& held:items_)if(held.holder==a.source.id&&!held.stowed&&detail::weapon(held.definition)){
        label.source="{weapon} ×{count} (stow {held})";label.arguments.push_back({"held",held.label.source,true});break;
    }
    return label;
}
void Session::throw_weapon(Actor& a,Actor& target,unsigned token)
{
    const auto selected=items_.at(token-1);
    const bool shield=std::any_of(items_.begin(),items_.end(),[&](const auto& i){return i.holder==a.source.id&&!i.stowed&&i.definition=="shield";});
    if(selected.stowed&&shield)for(auto& held:items_)if(held.holder==a.source.id&&!held.stowed&&detail::weapon(held.definition))held.stowed=true;
    auto attacker=thrown_actor(a,selected.definition);
    attack(attacker,target,true,false);
    a.aim_ready=attacker.aim_ready;
    const auto ground=ground_one(token,target.source.cell);
    if(weapon_hit_)weapon_hit_->thrown_item=ground;
    const auto previous=a.definition.weapon_label;
    a.definition=equipped_definition(a,items_);
    if(previous!=a.definition.weapon_label)a.weapon_hands=a.definition.weapon_hands;
}
bool Session::can_pick_up(const Actor& a,const HeldItemView& item) const
{
    if(item.holder||a.source.character_profile.empty()||distance(a.source.cell,item.cell)>5||!line_of_sight(a.source.cell,item.cell))return false;
    // A Shield requires the Utilize action to don (SRD p.92); other objects
    // use the turn's free interaction, then a Utilize action for another.
    if((item.definition=="shield"||!a.object_interaction)&&!a.actions.available())return false;
    auto next=items_;next[item.id-1].holder=a.source.id;
    try{(void)equipped_definition(a,next);return true;}catch(const std::runtime_error&){return false;}
}
SafeRecovery Session::safe_recovery() const
{
    SafeRecovery result;
    if(outcome_!=Outcome::victory)return result;
    std::set<unsigned> reachable_items;
    for(const auto& a:actors_)if(a.source.side==0&&conscious(a)){
        result.members.push_back(a.source.id);
        // Safe exploration has no turn budget; walls and occupied destinations
        // still constrain reach. Zero Speed permits only nearby collection.
        const int budget=a.definition.speed>detail::speed_penalty(a.effects)?board_.width*board_.height*15:0;
        const auto reachable=movement_grid(a).reachable(budget);
        for(const auto& item:items_)if(!item.holder&&actor(item.origin).source.side==0){
            bool near=distance(a.source.cell,item.cell)<=5&&line_of_sight(a.source.cell,item.cell);
            for(int y=item.cell.y-1;!near&&y<=item.cell.y+1;++y)
                for(int x=item.cell.x-1;!near&&x<=item.cell.x+1;++x){
                    const Cell cell{x,y};
                    near=reachable.cost_to(cell).has_value()&&distance(cell,item.cell)<=5&&line_of_sight(cell,item.cell);
                }
            if(near)reachable_items.insert(item.id);
        }
    }
    result.items.assign(reachable_items.begin(),reachable_items.end());return result;
}
detail::MovementGrid Session::movement_grid(const Actor& mover) const
{
    std::vector<detail::Occupant> occupants;
    for (const auto& other : actors_) {
        // Unconscious actors still occupy space; corpses do not. The mover's
        // current cell is the path origin, not an obstacle.
        if (!other.dead && other.source.id != mover.source.id)
            occupants.push_back({other.source.cell, other.source.side != mover.source.side, unconscious(other)});
    }
    return {board_, mover.source.cell, occupants, mover.effects.prone};
}

std::vector<Cell> Session::path_to(const Actor& actor, Cell destination) const
{
    return movement_grid(actor).reachable(movement_left(actor)).path_to(destination);
}

Snapshot Session::snapshot() const
{
    Snapshot s;s.identity=content_->identity;s.revision=revision_;s.round=round_;s.outcome=outcome_;
    s.elapsed_milliseconds=elapsed_ms_;s.held_items=items_;s.physical_inventory=physical_inventory_;
    s.actor=champion_move_?champion_move_->actor:pending()?pending():actors_[turn_].source.id;s.reaction_pending=!champion_move_&&pending()!=0;s.battlefield=board_;s.log=log_;s.log_messages=log_messages_;
    if(weapon_hit_){const auto& h=*weapon_hit_;const auto a=hit_actor(h);const auto dice=weapon_dice(a,h.ranged);const bool critical=critical_hit(a,actor(h.target),h.natural);
        if(h.sneak_pending)s.sneak_attack_choice=SneakAttackChoice{h.attacker,h.target,int((def(a).sneak_level+1)/2)*(critical?2:1),6,critical};
        else s.savage_attack_choice=SavageAttackChoice{h.attacker,h.target,def(a).weapon_label,dice.count*(critical?2:1),dice.sides,dice.bonus,h.first,h.second,critical,h.sneak_extra};}
    if(champion_move_)s.free_movement=FreeMovement{champion_move_->actor,champion_move_->remaining};
    if(check_choice_){const auto& c=*check_choice_;s.ability_check_choice=AbilityCheckChoice{c.actor,c.target,c.natural,def(actor(c.actor)).medicine,c.natural+def(actor(c.actor)).medicine,10,actor(c.actor).winds};}
    if(temporary_offer_)s.temporary_hp_offer=TemporaryHpOffer{actors_[turn_].source.id,actors_[turn_].temporary_hp,*temporary_offer_};
    for(const auto& a:actors_) {
        std::string status=a.dead?"Dead":a.hp==0?(a.stable?"Stable, unconscious":"Unconscious"):a.effects.sleeping?"Naturally asleep":a.effects.prone?"Prone":a.dodge?"Dodging":"Ready";
        if(def(a).slots)status+=" | slots "+std::to_string(a.slots);
        if(def(a).slots2)status+=" | L2 slots "+std::to_string(a.slots2);
        if(def(a).winds)status+=" | Second Wind "+std::to_string(a.winds);
        s.combatants.push_back({a.source.id,a.source.name,a.source.definition,a.source.side,a.source.cell,
            a.hp,def(a).hp,def(a).ac,a.initiative,champion_move_&&champion_move_->actor==a.source.id?champion_move_->remaining:movement_left(a),a.actions.available()&&conscious(a),a.bonus&&conscious(a),a.reaction&&conscious(a),conscious(a),a.dead,a.facing_left,status,vitals(a)});
        const auto display=combat_display(a.source.definition);
        auto& view=s.combatants.back();view.temporary_hp=a.temporary_hp;
        view.naturally_sleeping=a.effects.sleeping;view.prone=a.effects.prone;
        if(physical_inventory_)for(const auto& item:items_)if(item.holder==a.source.id)if(const auto* w=detail::weapon(item.definition);w&&w->thrown){
            const auto offered=legal_commands();view.thrown_weapons.push_back({item.id,throw_label(a,item),std::any_of(offered.begin(),offered.end(),[&](const auto& c){return c.verb=="throw"&&c.item==item.id;})});
        }
        if(a.effects.sleeping)view.conditions.push_back({"Naturally asleep",{}});
        if(a.effects.prone)view.conditions.push_back({"Prone",{}});
        if(def(a).cunning)view.bonus_actions={"cunning_dash","cunning_disengage"};
        if(def(a).sneak_level>=3)view.bonus_actions.push_back("steady_aim");
        if(def(a).surges)view.resources.push_back({"action_surge",{"Action Surge",{}},unsigned(a.surges),unsigned(def(a).surges),unsigned(def(a).surges)});
        // Explicit display order: this list feeds the combat cantrip dropdown,
        // so its order is observable in the UI and is not the table's order.
        for(const auto* id:{"fire_bolt","chill_touch","shocking_grasp","eldritch_blast",
                            "poison_spray","ray_of_frost","sacred_flame"})
            if(detail::knows_spell(def(a).known_cantrips,id))view.known_cantrips.push_back(id);
        if(def(a).rushes)view.resources.push_back({"adrenaline_rush",{"Adrenaline Rush",{}},unsigned(a.rushes),unsigned(def(a).rushes),unsigned(def(a).rushes)});
        if(def(a).hit_die){
            view.hp_messages.push_back({"Maximum HP includes level {level}, d{die} Hit Die and Constitution modifier {modifier}.",{{"level",std::to_string(def(a).level)},{"die",std::to_string(def(a).hit_die)},{"modifier",std::to_string(def(a).constitution)}}});
            if(def(a).dwarf)view.hp_messages.push_back({"Dwarven Toughness: +{hp} maximum HP.",{{"hp",std::to_string(def(a).level)}}});
        }
        if(display.type)view.type_name=display.type;
        if(display.melee)view.melee_weapon=display.melee;
        if(display.ranged)view.ranged_weapon=display.ranged;
        view.ranged_attack_available=def(a).range>0;
        view.equipment={a.weapon_hands};view.grips=grip_options(def(a));
        auto& messages=s.combatants.back().status_messages;
        messages.push_back({a.dead?"Dead":a.hp==0?(a.stable?"Stable, unconscious":"Unconscious"):a.effects.sleeping?"Naturally asleep":a.effects.prone?"Prone":a.dodge?"Dodging":"Ready",{}});
        if(def(a).slots)messages.push_back({"Spell slots: {count}",{{"count",std::to_string(a.slots)}}});
        if(def(a).slots2)messages.push_back({"L2 slots: {count}",{{"count",std::to_string(a.slots2)}}});
        if(a.actions.surge)messages.push_back({"Action Surge action ready (no Magic).",{}});
        if(def(a).winds)messages.push_back({"Second Wind: {count}",{{"count",std::to_string(a.winds)}}});
        for(const auto& effect:a.effects.active)if(effect.kind==detail::EffectKind::chill_touch){
            Message message{"Chill Touch ({source}): cannot regain HP.",{{"source",effect.source_name}}};
            messages.push_back(message);view.conditions.push_back(std::move(message));
        }
        if(detail::opportunity_blocked(a.effects)){messages.push_back({"Shocking Grasp: cannot make Opportunity Attacks.",{}});view.conditions.push_back({"Shocking Grasp: cannot make Opportunity Attacks.",{}});}
        if(detail::speed_penalty(a.effects)){messages.push_back({"Ray of Frost: Speed reduced by 10 feet.",{}});view.conditions.push_back({"Ray of Frost: Speed reduced by 10 feet.",{}});}
        if(detail::blinded(a.effects)){
            messages.push_back({"Blinded",{}});s.combatants.back().status+=" | Blinded";
            s.combatants.back().conditions.push_back({"Blinded",{}});
        }
    }
    return s;
}
void Session::apply_rider(const detail::SpellDef& spell,Actor& a,Actor& target,int dc)
{
    // Each rider keeps its own duration rule and its own log line; the wording
    // is unchanged so the message catalogue does not move.
    switch(spell.rider){
    case detail::Rider::none:return;
    case detail::Rider::chill_touch:{
        const unsigned slot=turn_end_ms(turn_)-(turn_?turn_end_ms(turn_-1):0);
        detail::apply_chill_touch(target.effects,scope_,a.source.id,a.source.name,detail::round_ms+slot);
        log(target.source.name+" cannot regain HP until the end of the caster's next turn.",
            {"{name} cannot regain HP until the end of the caster's next turn.",{{"name",target.source.name}}});
        return;
    }
    case detail::Rider::shocking_grasp:
        detail::apply_shocking_grasp(target.effects,scope_,a.source.id,a.source.name,next_turn_ms(target));
        log(target.source.name+" cannot make Opportunity Attacks until its next turn.",
            {"{name} cannot make Opportunity Attacks until its next turn.",{{"name",target.source.name}}});
        return;
    case detail::Rider::ray_of_frost:
        detail::apply_ray_of_frost(target.effects,scope_,a.source.id,a.source.name,next_turn_ms(a));
        log(target.source.name+" is slowed by Ray of Frost.",
            {"{name} is slowed by Ray of Frost.",{{"name",target.source.name}}});
        return;
    case detail::Rider::blindness:
        detail::apply_blindness(target.effects,scope_,a.source.id,a.source.name,dc,next_save_ms(target.source.id));
        log(target.source.name+" is Blinded.",{"{name} is Blinded.",{{"name",target.source.name}}});
        return;
    }
}
void Session::resolve_spell(const detail::SpellDef& spell,bool upcast,Actor& a,EntityId target_id)
{
    const auto& d=def(a);
    // A level-two spell always draws a level-two slot; a level-one spell draws
    // one only in its upcast form.
    if(spell.level){
        if(upcast||spell.level>=2)--a.slots2;else --a.slots;
        a.spent_slot=true;
    }
    auto rolled=spell.dice;
    rolled.count+=static_cast<int>(upcast?spell.upcast.extra_dice:0u);
    if(spell.add_casting_modifier)rolled.bonus=d.casting-2;
    const unsigned instances=spell.instances+(upcast?spell.upcast.extra_instances:0u);
    const int dc=8+d.casting;
    const auto name=std::string(spell.label);
    switch(spell.pattern){
    case detail::SpellPattern::heal:
        heal(actor(target_id),dice(rolled));
        return;
    case detail::SpellPattern::spell_attack:{
        auto& target=actor(target_id);
        if(attack(a,target,!spell.melee,true,rolled,spell.damage))apply_rider(spell,a,target,dc);
        return;
    }
    case detail::SpellPattern::repeat_attack:
        // Re-read the target each pass: it may drop before the later rays.
        for(unsigned ray=0;ray<instances&&actor(target_id).hp>0;++ray)
            attack(a,actor(target_id),!spell.melee,true,rolled,spell.damage);
        return;
    case detail::SpellPattern::auto_damage:{
        auto& target=actor(target_id);int total=0;
        // Instances resolve separately so resistance applies per instance.
        for(unsigned n=0;n<instances;++n)total+=resolved_damage(target,spell.damage,dice(rolled));
        auto lowered=std::string(detail::damage_name(spell.damage));
        for(auto& c:lowered)if(c>='A'&&c<='Z')c+=32;
        log(a.source.name+" casts "+name+" for "+std::to_string(total)+" "+lowered+" damage.",
            {"{name} casts "+name+" for {damage} "+lowered+" damage.",
             {{"name",a.source.name},{"damage",std::to_string(total)}}});
        damage(target,total);
        return;
    }
    case detail::SpellPattern::save_damage:{
        auto& target=actor(target_id);
        log(a.source.name+" casts "+name+" at "+target.source.name+".",
            {"{name} casts "+name+" at {target}.",{{"name",a.source.name},{"target",target.source.name}}});
        if(saving_throw_succeeds(target,spell.save,dc))return;
        const auto type=std::string(detail::damage_name(spell.damage));
        const int amount=resolved_damage(target,spell.damage,dice(rolled));
        log(target.source.name+" takes "+std::to_string(amount)+" "+type+" damage.",
            {"{name} takes {damage} "+type+" damage.",
             {{"name",target.source.name},{"damage",std::to_string(amount)}}});
        damage(target,amount);
        return;
    }
    case detail::SpellPattern::save_condition:{
        auto& target=actor(target_id);
        if(!saving_throw_succeeds(target,spell.save,dc))apply_rider(spell,a,target,dc);
        return;
    }
    }
}
void Session::offer_spells(std::vector<Command>& commands,const Actor& a,const Actor& other,int feet,
                          detail::SpellTarget scope,bool bonus_pass) const
{
    // The Bonus Action pass runs before the loop that skips corpses, so filter
    // them here too; the Action pass has already done it and is unaffected.
    if(other.dead)return;
    const auto& d=def(a);
    for(const auto& spell:detail::spell_table){
        if(spell.target!=scope||spell.bonus_action!=bonus_pass)continue;
        if(!detail::knows_spell(d.spells,spell.id))continue;
        switch(spell.target){
        case detail::SpellTarget::enemy:
            if(other.source.side==a.source.side||other.hp<=0)continue;
            break;
        case detail::SpellTarget::wounded_ally:
            if(other.source.side!=a.source.side||other.hp>=def(other).hp)continue;
            break;
        case detail::SpellTarget::any_creature:
            break;
        }
        if(feet>spell.range)continue;
        if(spell.requires_sight&&!can_see(a,other))continue;
        if(spell.requires_effect_capacity&&!detail::can_apply(other.effects))continue;
        const auto* components=detail::spell_components(spell.id);
        if(!components||(components->somatic&&!somatic_hand(d)))continue;
        if(spell.bonus_action?!a.bonus:!a.actions.available(true))continue;
        const auto offer=[&](std::string verb,std::string label){
            commands.push_back({revision_,a.source.id,other.source.id,std::move(verb),std::move(label),Cell{}});
        };
        if(!spell.level){offer(std::string(spell.id),std::string(spell.label));continue;}
        if(a.spent_slot)continue;
        // A level-two spell is only ever cast from a level-two slot, so it has
        // no separate upcast verb.
        if(spell.level>=2){if(a.slots2>0)offer(std::string(spell.id),std::string(spell.label));continue;}
        if(a.slots>0)offer(std::string(spell.id),std::string(spell.label));
        if(a.slots2>0)offer(std::string(spell.id)+"_2",std::string(spell.label)+" (level 2 slot)");
    }
}
std::vector<Command> Session::legal_commands() const
{
    std::vector<Command> commands;if(outcome_!=Outcome::ongoing)return commands;
    const auto add=[&](EntityId who,std::string verb,std::string label,EntityId target=0,Cell destination=Cell{}) {
        commands.push_back({revision_,who,target,std::move(verb),std::move(label),destination});
    };
    const auto add_grips=[&](const Actor& a){
        for(const auto& option:grip_options(def(a)))if(option.available&&option.hands!=a.weapon_hands)
            add(a.source.id,option.hands==1?"grip_one":"grip_two",option.hands==1?"One hand":"Two hands");
    };
    if(champion_move_){add(champion_move_->actor,"end","Finish free move");for(const auto cell:movement_reach(champion_move_->actor))add(champion_move_->actor,"move","Free move",0,cell);return commands;}
    if(check_choice_){add(check_choice_->actor,"mind_use","Use Tactical Mind",check_choice_->target);add(check_choice_->actor,"mind_skip","Keep failed check",check_choice_->target);return commands;}
    if(weapon_hit_){
        const auto& h=*weapon_hit_;
        if(h.sneak_pending){add(h.attacker,"sneak_use","Use Sneak Attack",h.target);add(h.attacker,"sneak_skip","Keep hit; save Sneak Attack",h.target);}
        else if(!h.second){add(h.attacker,"savage_use","Use Savage Attacker",h.target);add(h.attacker,"savage_skip","Keep damage; save feat",h.target);}
        else {add(h.attacker,"savage_first","Keep first roll",h.target);add(h.attacker,"savage_second","Keep second roll",h.target);}
        return commands;
    }
    if(temporary_offer_){
        add(actors_[turn_].source.id,"temp_hp_keep","Keep current");
        add(actors_[turn_].source.id,"temp_hp_use","Use new");return commands;
    }
    if(pending()) {
        const auto reactor=std::find_if(actors_.begin(),actors_.end(),[&](const auto& a){return a.source.id==pending();});
        add_grips(*reactor);
        add(pending(),"opportunity","Opportunity attack",actors_[turn_].source.id);
        add(pending(),"decline","Decline reaction");return commands;
    }
    const auto& a=actors_[turn_];if(!conscious(a))return commands;const auto& d=def(a);const auto id=a.source.id;
    add(id,"end","End turn");add_grips(a);
    for(const auto& item:items_)if(can_pick_up(a,item))
        add(id,"pick_up",item.definition=="shield"?"Pick up (Action)":a.object_interaction?"Pick up (interaction)":"Pick up (Action)",item.id);
    const int speed=a.aim_used?0:std::max(0,d.speed-detail::speed_penalty(a.effects));
    if(a.effects.prone&&speed>0&&movement_left(a)>=speed/2)add(id,"stand_up","Stand up");
    if(a.surges>0&&!a.surge_used)add(id,"action_surge","Action Surge",id);
    if(a.bonus&&d.sneak_level>=3&&!a.moved)add(id,"steady_aim","Steady Aim");
    if(a.bonus&&d.cunning){add(id,"cunning_dash","Cunning Action: Dash");add(id,"cunning_disengage","Cunning Action: Disengage");}
    if(a.bonus&&a.rushes>0)add(id,"adrenaline_rush","Adrenaline Rush",id);
    if(a.bonus&&a.winds>0&&a.hp<d.hp)add(id,"second_wind","Second Wind",id);
    for(const auto& other:actors_)
        offer_spells(commands,a,other,distance(a.source.cell,other.source.cell),detail::SpellTarget::wounded_ally,true);
    if(a.actions.available()) {
        add(id,"dash","Dash");add(id,"dodge","Dodge");add(id,"disengage","Disengage");
        for(const auto& other:actors_) {
            if(other.dead || !line_of_sight(a.source.cell,other.source.cell))continue;
            const int feet=distance(a.source.cell,other.source.cell);
            if(!a.source.character_profile.empty()&&other.hp==0&&!other.stable&&feet<=5)
                add(id,"stabilize","Stabilize",other.source.id);
            if(other.source.side==a.source.side&&other.effects.sleeping&&feet<=5)
                add(id,"wake_ally","Wake ally",other.source.id);
            offer_spells(commands,a,other,feet,detail::SpellTarget::any_creature,false);
            if(other.source.side!=a.source.side && other.hp>0) {
                if(physical_inventory_)for(const auto& item:items_)if(item.holder==id)if(const auto* w=detail::weapon(item.definition);w&&w->thrown&&feet<=w->long_range){
                    add(id,"throw","Throw",other.source.id);commands.back().item=item.id;
                }
                if(feet<=d.reach)add(id,"melee",a.source.definition=="slums-kobold"?"Dagger attack":
                    a.source.definition=="slums-kobold-leader"||a.source.definition=="slums-kobold-leader-sword"?"Short sword attack":"Melee attack",other.source.id);
                if(d.range>0&&feet<=d.long_range)add(id,"ranged",
                    a.source.definition=="slums-kobold-leader"?"Short bow attack":"Ranged attack",other.source.id);
                offer_spells(commands,a,other,feet,detail::SpellTarget::enemy,false);
            } else offer_spells(commands,a,other,feet,detail::SpellTarget::wounded_ally,false);
        }
    }
    for(const auto cell:movement_reach(id))add(id,"move","Move",0,cell);
    return commands;
}
std::vector<Cell> Session::movement_reach(EntityId id) const
{
    std::vector<Cell> cells;
    if(outcome_!=Outcome::ongoing||(!champion_move_&&pending())||temporary_offer_||weapon_hit_||check_choice_||(champion_move_&&id!=champion_move_->actor))return cells;
    const auto actor=std::find_if(actors_.begin(),actors_.end(),[&](const auto& a){return a.source.id==id;});
    if(actor==actors_.end()||!conscious(*actor))return cells;
    const auto budget=champion_move_?champion_move_->remaining:movement_left(*actor);
    if(budget<=0)return cells;
    const auto reachable=movement_grid(*actor).reachable(budget);
    for(int y=0;y<board_.height;++y)for(int x=0;x<board_.width;++x)
        if(reachable.cost_to({x,y}))cells.push_back({x,y});
    return cells;
}
int Session::resolved_damage(const Actor& target,detail::DamageType type,int amount)
{
    const std::array parts{detail::DamagePart{type,amount}};
    const auto result=detail::resolve_damage(parts,def(target).affinities);
    if(result.total!=amount){
        const auto name=std::string(detail::damage_name(type));
        log(target.source.name+": "+name+" damage "+std::to_string(amount)+" -> "+std::to_string(result.total)+".",
            {"{name}: {type} damage {before} -> {after}.",{{"name",target.source.name},{"type",name,true},{"before",std::to_string(amount)},{"after",std::to_string(result.total)}}});
    }
    return result.total;
}
void Session::damage(Actor& target,int amount,bool critical)
{
    if(!amount||target.dead)return;
    target.effects.sleeping=false;
    detail::damage_life(target,amount,def(target).hp,critical,target.source.side==1);
    if(target.hp==0){target.effects.prone=true;drop_held(target);}
    if(target.hp==0&&!target.dead&&!target.stable)target.recovery.death_save_in_ms=next_turn_ms(target);
    if(target.hp==0) {
        target.dodge=false;
        if(!target.dead&&shares_occupied_space(target))target.involuntary_overlap=true;
        log(target.source.name+(target.dead?" is defeated.":" falls unconscious."),
            {target.dead?"{name} is defeated.":"{name} falls unconscious.",{{"name",target.source.name}}});
    }
    clear_departed_overlaps();
}
void Session::heal(Actor& target,int amount)
{
    if(target.hp==0&&shares_occupied_space(target))target.involuntary_overlap=true;
    const bool was_unconscious=target.hp==0;
    const int restored=detail::heal_life(target,amount,def(target).hp,!detail::healing_blocked(target.effects));
    if(was_unconscious&&restored)target.effects.prone=true;
    log(target.source.name+" recovers "+std::to_string(restored)+" HP.",
        {"{name} recovers {hp} HP.",{{"name",target.source.name},{"hp",std::to_string(restored)}}});
}
detail::RollModifiers Session::attack_modifiers(const Actor& a,const Actor& target,bool ranged,bool spell) const
{
    const auto& d=def(a);bool disadvantaged=!spell&&(d.str_dex_disadvantage||
        (ranged?d.ranged_heavy_disadvantage:d.melee_heavy_disadvantage));
    if(ranged){
        if(!spell&&distance(a.source.cell,target.source.cell)>d.range)disadvantaged=true;
        for(const auto& other:actors_)if(other.source.side!=a.source.side&&conscious(other)&&distance(a.source.cell,other.source.cell)<=5&&can_see(other,a))disadvantaged=true;
    }
    auto result=detail::attack_modifiers(detail::blinded(a.effects),detail::blinded(target.effects),target.dodge,disadvantaged||a.effects.prone);
    // At zero HP the creature is Unconscious and Prone (SRD pp.187,191).
    // At longer range their opposing attack modifiers cancel, not stack.
    if(unconscious(target)||a.aim_ready)result.advantage=true;
    if(target.effects.prone||target.hp==0){if(distance(a.source.cell,target.source.cell)<=5)result.advantage=true;else result.disadvantage=true;}
    return result;
}
Dice Session::weapon_dice(const Actor& a,bool ranged) const
{
    const auto& d=def(a);auto result=ranged?d.ranged:d.melee;
    if(!ranged&&d.versatile_sides&&a.weapon_hands==2)result.sides=d.versatile_sides;
    return result;
}
void Session::apply_hit(Actor& a,Actor& target,int natural,int bonus,int mode,int amount,bool savage,detail::DamageType type,bool spell)
{
    const std::string modifier_label=mode<0?" (disadvantage)":mode>0?" (advantage)":"";
    std::string message=a.source.name+" -> "+target.source.name+": d20 "+std::to_string(natural)+
        " + "+std::to_string(bonus)+" vs AC "+std::to_string(def(target).ac)+modifier_label;
    std::vector<MessageArgument> arguments{{"actor",a.source.name},{"target",target.source.name},{"roll",std::to_string(natural)},
        {"bonus",std::to_string(bonus)},{"ac",std::to_string(def(target).ac)},{"disadvantage",modifier_label,true}};
    if(!(!spell&&def(a).champion&&natural==19)&&!attack_hits(natural,bonus,def(target).ac)){log(message+" misses.",{"{actor} -> {target}: d20 {roll} + {bonus} vs AC {ac}{disadvantage} misses.",arguments});return;}
    const bool critical=critical_hit(a,target,natural,spell);
    if(savage)message+=" (Savage Attacker)";
    amount=resolved_damage(target,type,amount);
    arguments.push_back({"savage",savage?" (Savage Attacker)":"",true});
    arguments.push_back({"hit",critical?"CRITICAL":"hits",true});arguments.push_back({"damage",std::to_string(amount)});
    log(message+(critical?" CRITICAL":" hits")+" for "+std::to_string(amount)+" damage.",
        {"{actor} -> {target}: d20 {roll} + {bonus} vs AC {ac}{disadvantage}{savage} {hit} for {damage} damage.",arguments});damage(target,amount,critical);
    if(critical&&def(a).champion&&conscious(a)){
        champion_move_=ChampionMove{a.source.id,target.source.id,natural,std::max(0,def(a).speed-detail::speed_penalty(a.effects))/2,spell,a.source.cell};
        if(pending()==a.source.id&&target.source.id==actors_[turn_].source.id&&target.hp==0){path_.clear();path_index_=0;reactors_.clear();reactor_index_=0;}
    }
}
bool Session::sneak_eligible(const Actor& a,const Actor& target,bool ranged,int mode) const
{
    const auto& d=def(a);if(!d.sneak_level||a.sneak_used)return false;
    const bool ally=std::any_of(actors_.begin(),actors_.end(),[&](const auto& other){return other.source.id!=a.source.id&&other.source.side==a.source.side&&conscious(other)&&distance(other.source.cell,target.source.cell)<=5;});
    // A ranged weapon's fallback melee attack is unarmed, not that weapon.
    return detail::sneak_attack_eligible({ranged?d.range>0:!d.weapon_label.empty()&&!d.ranged_weapon,d.finesse,d.ranged_weapon,mode,ally});
}
bool Session::attack(Actor& a,Actor& target,bool ranged,bool spell,Dice spell_dice,detail::DamageType spell_type)
{
    if(target.source.cell.x!=a.source.cell.x)a.facing_left=target.source.cell.x<a.source.cell.x;
    const auto& d=def(a);const auto modifiers=attack_modifiers(a,target,ranged,spell);
    const bool aimed=a.aim_ready;a.aim_ready=false;
    const int natural=detail::d20(modifiers,rng_),bonus=spell?d.casting:ranged?d.ranged_bonus:d.melee_bonus;
    const auto damage_dice=spell?spell_dice:weapon_dice(a,ranged);
    const bool hit=(!spell&&d.champion&&natural==19)||attack_hits(natural,bonus,def(target).ac);
    const bool sneak=hit&&!spell&&sneak_eligible(a,target,ranged,modifiers.mode());
    const int amount=hit?(!spell&&d.sneak_level?detail::roll_damage_component(rng_,damage_dice,critical_hit(a,target,natural)):dice(damage_dice,critical_hit(a,target,natural,spell))):0;
    if(hit&&!spell&&(sneak||(damage_dice.count&&d.savage&&!a.savage_used))){
        weapon_hit_=PendingWeaponHit{a.source.id,target.source.id,ranged,natural,modifiers.mode(),amount};weapon_hit_->sneak_pending=sneak;weapon_hit_->aimed=aimed;return hit;
    }
    apply_hit(a,target,natural,bonus,modifiers.mode(),std::max(0,amount),false,spell?spell_type:ranged?d.ranged_type:d.melee_type,spell);
    return hit;
}
void Session::resolve_weapon_hit(int amount)
{
    const auto h=*weapon_hit_;auto a=hit_actor(h);weapon_hit_.reset();const auto& d=def(a);
    apply_hit(a,actor(h.target),h.natural,h.ranged?d.ranged_bonus:d.melee_bonus,h.mode,std::max(0,amount+h.sneak_extra),h.second.has_value(),h.ranged?d.ranged_type:d.melee_type);
    if(pending()&&!champion_move_)finish_reaction();
}
void Session::finish_reaction()
{
    ++reactor_index_;update_outcome();
    if(outcome_==Outcome::ongoing){
        if(actors_[turn_].hp==0){path_.clear();path_index_=0;reactors_.clear();reactor_index_=0;}
        else if(!pending())progress_movement();
    }
}
void Session::update_outcome()
{
    bool party=false,enemies=false;for(const auto& a:actors_)if(a.hp>0&&!a.dead)(a.source.side==0?party:enemies)=true;
    if(!party||!enemies) {
        outcome_=!party?Outcome::defeat:Outcome::victory;champion_move_.reset();path_.clear();path_index_=0;reactors_.clear();reactor_index_=0;
        log(outcome_==Outcome::victory?"Victory.":"The party is incapacitated. Defeat.");
    }
}
bool Session::begin_turn()
{
    auto& a=actors_[turn_];
    std::erase_if(a.effects.active,[](const auto& effect){return effect.kind==detail::EffectKind::shocking_grasp;});
    if(a.dead||a.effects.sleeping)return false;
    if(a.hp==0){
        if(!a.stable){
            const int result=detail::death_save(a,rng_,!detail::healing_blocked(a.effects));
            log(a.source.name+" death save: "+std::to_string(result),
                {"{name} death save: {roll}",{{"name",a.source.name},{"roll",std::to_string(result)}}});
            if(a.hp>0){a.effects.prone=true;if(shares_occupied_space(a))a.involuntary_overlap=true;}
            if(a.dead)clear_departed_overlaps();
        }
        if(a.hp==0)return false;
    }
    for(auto& actor:actors_){actor.savage_used=false;actor.sneak_used=false;actor.aim_used=actor.aim_ready=false;actor.moved=false;}
    a.actions={};a.surge_used=false;a.bonus=a.reaction=true;a.dodge=a.disengaged=false;a.movement=def(a).speed;a.dashes=0;
    a.spent_slot=false;a.rush_used=false;a.object_interaction=true;
    log("Round "+std::to_string(round_)+": "+a.source.name+" acts.",{"Round {round}: {name} acts.",{{"round",std::to_string(round_)},{"name",a.source.name}}});
    return true;
}
unsigned Session::next_turn_ms(const Actor& target) const
{
    const auto index=static_cast<std::size_t>(&target-actors_.data());
    const unsigned start=index?turn_end_ms(index-1):0,current=turn_?turn_end_ms(turn_-1):0;
    return start>current?start-current:detail::round_ms-current+start;
}
unsigned Session::next_save_ms(EntityId target) const
{
    const auto found=std::find_if(actors_.begin(),actors_.end(),[&](const auto& a){return a.source.id==target;});
    const auto end=turn_end_ms(found-actors_.begin());
    const auto start=turn_?turn_end_ms(turn_-1):0;
    return end>start?end-start:detail::round_ms-start+end;
}
void Session::log_save(const Actor& target,const detail::SaveResult& result)
{
    constexpr std::array names{"Strength","Dexterity","Constitution","Intelligence","Wisdom","Charisma"};
    constexpr std::array messages{
        "{name} Strength save: d20 {roll} + {bonus} vs DC {dc} ({result}).",
        "{name} Dexterity save: d20 {roll} + {bonus} vs DC {dc} ({result}).",
        "{name} Constitution save: d20 {roll} + {bonus} vs DC {dc} ({result}).",
        "{name} Intelligence save: d20 {roll} + {bonus} vs DC {dc} ({result}).",
        "{name} Wisdom save: d20 {roll} + {bonus} vs DC {dc} ({result}).",
        "{name} Charisma save: d20 {roll} + {bonus} vs DC {dc} ({result})."};
    const auto ability=static_cast<unsigned>(result.ability);const std::string outcome=result.success?"success":"failure";
    log(target.source.name+" "+names[ability]+" save: d20 "+std::to_string(result.natural)+" + "+std::to_string(result.bonus)+
        " vs DC "+std::to_string(result.dc)+" ("+outcome+").",
        {messages[ability],{{"name",target.source.name},{"roll",std::to_string(result.natural)},{"bonus",std::to_string(result.bonus)},
          {"dc",std::to_string(result.dc)},{"result",outcome,true}}});
}
bool Session::saving_throw_succeeds(const Actor& target,detail::Ability ability,int dc)
{
    if(unconscious(target)&&(ability==detail::Ability::strength||ability==detail::Ability::dexterity)){
        const std::string name=ability==detail::Ability::strength?"Strength":"Dexterity";
        log(target.source.name+" automatically fails the "+name+" save while Unconscious.",
            {"{name} automatically fails the {ability} save while Unconscious.",{{"name",target.source.name},{"ability",name,true}}});
        return false;
    }
    const auto result=detail::saving_throw(ability,def(target).saves[static_cast<unsigned>(ability)],dc,
        detail::saving_modifiers(ability,def(target).str_dex_disadvantage,target.dodge),rng_);
    log_save(target,result);return result.success;
}
void Session::advance_turn_time()
{
    // Partition one six-second round across its fixed initiative slots. Integer
    // boundaries telescope to exactly 6000 ms, even with 7 or 64 participants.
    // Dead/unconscious slots still pass time; menus and repeated snapshots don't.
    const unsigned delta=turn_end_ms(turn_)-(turn_?turn_end_ms(turn_-1):0);
    for(auto& a:actors_)detail::start_stable_recovery(a,rng_);
    std::vector<detail::RecoverySubject> subjects;
    std::vector<EntityId> unconscious;
    for(auto& a:actors_){
        subjects.push_back({{a.source.id,a.effects,def(a).saves,a.dead,def(a).str_dex_disadvantage,a.dodge},a});
        if(a.hp==0)unconscious.push_back(a.source.id);
    }
    detail::elapse_recovery(subjects,delta,rng_,detail::RecoveryMode::combat,[&](const detail::EffectEvent& event){
        const auto& target=actor(event.target);
        if(event.save)log_save(target,*event.save);
        if(event.removed&&event.effect.kind==detail::EffectKind::chill_touch)log(target.source.name+" loses a Chill Touch effect.",{ "{name} loses a Chill Touch effect.",{{"name",target.source.name}}});
        if(event.removed&&event.effect.kind==detail::EffectKind::shocking_grasp)log(target.source.name+" loses a Shocking Grasp effect.",{"{name} loses a Shocking Grasp effect.",{{"name",target.source.name}}});
        if(event.removed&&event.effect.kind==detail::EffectKind::ray_of_frost)log(target.source.name+" loses a Ray of Frost effect.",{"{name} loses a Ray of Frost effect.",{{"name",target.source.name}}});
        if(event.removed&&event.effect.kind==detail::EffectKind::blindness)log(target.source.name+" recovers from a blindness effect.",
            {"{name} recovers from a blindness effect.",{{"name",target.source.name}}});
    });
    for(auto& a:actors_)if(a.hp>0&&std::find(unconscious.begin(),unconscious.end(),a.source.id)!=unconscious.end()){
        a.effects.prone=true;
        if(shares_occupied_space(a))a.involuntary_overlap=true;
        log(a.source.name+" recovers 1 HP naturally.",{"{name} recovers 1 HP naturally.",{{"name",a.source.name}}});
    }
    elapsed_ms_+=std::min<std::uint64_t>(delta,std::numeric_limits<std::uint64_t>::max()-elapsed_ms_);
}
void Session::end_turn()
{
    actors_[turn_].actions.surge=false;
    for(std::size_t checked=0;checked<=actors_.size()*2;++checked) {
        advance_turn_time();
        turn_=(turn_+1)%actors_.size();
        // The round is a display counter. Saturation avoids wrapping it to
        // zero while keeping an extremely long (or edited) combat playable.
        if(turn_==0 && round_<std::numeric_limits<unsigned>::max())++round_;
        if(begin_turn())return;
    }
    update_outcome();
}
void Session::progress_movement()
{
    auto& a=actors_[turn_];
    const auto grid = movement_grid(a);
    while(path_index_<path_.size()&&a.hp>0) {
        const auto destination=path_[path_index_];
        if(reactors_.empty()&&!a.disengaged)for(const auto& other:actors_)
            if(other.source.side!=a.source.side&&conscious(other)&&other.reaction&&!detail::opportunity_blocked(other.effects)&&
                distance(a.source.cell,other.source.cell)<=def(other).reach&&distance(destination,other.source.cell)>def(other).reach&&
                can_see(other,a))reactors_.push_back(other.source.id);
        if(pending())return;
        const auto cost = grid.step_cost(a.source.cell, destination);
        if (!cost || *cost > movement_left(a)) throw std::logic_error("Invalid accepted movement path");
        a.movement -= *cost;
        a.source.cell = destination;a.moved=true;
        clear_departed_overlaps();
        ++path_index_;
        reactors_.clear();reactor_index_=0;
    }
    path_.clear();path_index_=0;reactors_.clear();reactor_index_=0;
}
bool Session::submit(const Command& command)
{
    const auto offered=legal_commands();
    if(std::none_of(offered.begin(),offered.end(),[&](const auto& c){return same_command(c,command);}))return false;
    auto& a=actor(command.actor);const auto& d=def(a);
    if(turns_to_attack(command.verb)&&command.target){
        const auto& target=actor(command.target);
        if(target.source.cell.x!=a.source.cell.x){
            const bool new_left=target.source.cell.x<a.source.cell.x;
            if(new_left!=a.facing_left){
                a.facing_left=new_left;
                log(a.source.name+(new_left?" turns left.":" turns right."));
            }
        }
    }
    const bool second=command.verb.ends_with("_2");
    if(champion_move_){
        if(command.verb=="end")finish_champion_move();
        else{const auto grid=movement_grid(a);const auto path=grid.reachable(champion_move_->remaining).path_to(command.destination);
            for(const auto cell:path){champion_move_->remaining-=*grid.step_cost(a.source.cell,cell);a.source.cell=cell;a.moved=true;}clear_departed_overlaps();
            if(champion_move_->remaining==0)finish_champion_move();}
    }else if(command.verb=="mind_use"||command.verb=="mind_skip"){
        const auto check=*check_choice_;check_choice_.reset();
        const int boost=command.verb=="mind_use"?roll(10):0;
        if(boost&&check.natural+d.medicine+boost>=10)--a.winds;
        finish_check(check,boost);
    }else if(command.verb=="stabilize"){
        PendingCheck check{a.source.id,command.target,detail::d20({},rng_),a.actions.surge};
        a.actions.spend();
        if(check.natural+d.medicine<10&&d.tactical_mind&&a.winds>0)check_choice_=check;
        else finish_check(check,0);
    }else if(command.verb=="sneak_use"||command.verb=="sneak_skip"){
        auto& h=*weapon_hit_;h.sneak_pending=false;
        if(command.verb=="sneak_use"){
            a.sneak_used=true;h.sneak_extra=dice(detail::sneak_attack_dice(d.sneak_level),critical_hit(a,actor(h.target),h.natural));
            log(a.source.name+" uses Sneak Attack.",{"{name} uses Sneak Attack.",{{"name",a.source.name}}});
        }
        const auto attacker=hit_actor(h);
        if(!d.savage||a.savage_used||!weapon_dice(attacker,h.ranged).count)resolve_weapon_hit(h.first);
    }else if(command.verb=="savage_use"){
        a.savage_used=true;const auto attacker=hit_actor(*weapon_hit_);weapon_hit_->second=def(attacker).sneak_level?detail::roll_damage_component(rng_,weapon_dice(attacker,weapon_hit_->ranged),critical_hit(attacker,actor(weapon_hit_->target),weapon_hit_->natural)):dice(weapon_dice(attacker,weapon_hit_->ranged),critical_hit(attacker,actor(weapon_hit_->target),weapon_hit_->natural));
    }else if(command.verb=="savage_skip"||command.verb=="savage_first"||command.verb=="savage_second"){
        resolve_weapon_hit(command.verb=="savage_second"?*weapon_hit_->second:weapon_hit_->first);
    }else if(command.verb=="temp_hp_keep"||command.verb=="temp_hp_use") {
        detail::grant_temporary_hp(a,*temporary_offer_,command.verb=="temp_hp_keep"?TemporaryHpChoice::keep_current:TemporaryHpChoice::use_new);
        temporary_offer_.reset();
    } else if(command.verb=="pick_up") {
        auto& item=items_.at(command.target-1);
        if(item.definition=="shield"||!a.object_interaction)a.actions.spend();else a.object_interaction=false;
        item.holder=a.source.id;item.cell={};a.definition=equipped_definition(a,items_);a.weapon_hands=a.definition.weapon_hands;
    } else if(command.verb=="stand_up") {
        a.movement-=std::max(0,d.speed-detail::speed_penalty(a.effects))/2;a.effects.prone=false;
    } else if(command.verb=="wake_ally") {
        a.actions.spend();auto& target=actor(command.target);target.effects.sleeping=false;
        if(shares_occupied_space(target))target.involuntary_overlap=true;
    } else if(command.verb=="action_surge") {
        --a.surges;a.surge_used=true;a.actions.surge=true;
        log(a.source.name+" uses Action Surge.",{"{name} uses Action Surge.",{{"name",a.source.name}}});
    } else if(command.verb=="steady_aim") {
        a.bonus=false;a.aim_used=a.aim_ready=true;
        log(a.source.name+" uses Steady Aim.",{"{name} uses Steady Aim.",{{"name",a.source.name}}});
    } else if(command.verb=="cunning_dash"||command.verb=="cunning_disengage") {
        a.bonus=false;
        if(command.verb=="cunning_dash"){a.movement+=d.speed;++a.dashes;log(a.source.name+" dashes.",{"{name} dashes.",{{"name",a.source.name}}});}
        else {a.disengaged=true;log(a.source.name+" disengages.",{"{name} disengages.",{{"name",a.source.name}}});}
    } else if(command.verb=="adrenaline_rush") {
        a.bonus=false;--a.rushes;a.rush_used=true;a.movement+=d.speed;++a.dashes;
        TemporaryHitPoints offered{d.rushes,std::string(rush_source)};
        if(a.temporary_hp.amount)temporary_offer_=std::move(offered);
        else detail::grant_temporary_hp(a,offered,TemporaryHpChoice::use_new);
        log(a.source.name+" uses Adrenaline Rush.",{"{name} uses Adrenaline Rush.",{{"name",a.source.name}}});
    } else if(command.verb=="grip_one"||command.verb=="grip_two") {
        a.weapon_hands=command.verb=="grip_one"?1:2;
    } else if(command.verb=="opportunity"||command.verb=="decline") {
        if(command.verb=="opportunity"){a.reaction=false;attack(a,actor(command.target),false);}
        if(!weapon_hit_&&!champion_move_)finish_reaction();
    } else if(command.verb=="move") {
        path_=path_to(a,command.destination);path_index_=0;progress_movement();
    } else if(command.verb=="end")end_turn();
    else if(command.verb=="second_wind") {a.bonus=false;--a.winds;heal(a,roll(10)+d.level);}
    else if(const auto* bonus_spell=detail::find_spell(command.verb);bonus_spell&&bonus_spell->bonus_action){
        // A Bonus Action spell spends no Action, so it resolves outside the
        // Action block below.
        a.bonus=false;resolve_spell(*bonus_spell,second,a,command.target);
    }
    else {
        (void)a.actions.spend(detail::spell_components(command.verb)!=nullptr);
        if(command.verb=="dash"){a.movement+=d.speed;++a.dashes;log(a.source.name+" dashes.",{"{name} dashes.",{{"name",a.source.name}}});}
        else if(command.verb=="dodge"){a.dodge=true;log(a.source.name+" dodges.",{"{name} dodges.",{{"name",a.source.name}}});}
        else if(command.verb=="disengage"){a.disengaged=true;log(a.source.name+" disengages.",{"{name} disengages.",{{"name",a.source.name}}});}
        else if(const auto* spell=detail::find_spell(command.verb))resolve_spell(*spell,second,a,command.target);
        else if(command.verb=="throw")throw_weapon(a,actor(command.target),command.item);
        else {
            const auto held=std::find_if(items_.begin(),items_.end(),[&](const auto& item){const auto* w=detail::weapon(item.definition);return item.holder==a.source.id&&!item.stowed&&w&&w->thrown;});
            if(physical_inventory_&&command.verb=="ranged"&&held!=items_.end())throw_weapon(a,actor(command.target),held->id);
            // Fire Bolt used to reach this fallthrough and borrow attack()'s
            // default 1d10 fire arguments; it is now an explicit table row.
            else attack(a,actor(command.target),command.verb!="melee");
        }
    }
    // Revisions are command tickets; zero is reserved for invalid commands.
    // Unsigned wrap is defined, but must skip that reserved value.
    if (++revision_ == 0) revision_ = 1;
    update_outcome();
    if(outcome_==Outcome::ongoing&&!pending()&&!champion_move_&&actors_[turn_].hp==0)end_turn();
    if(outcome_!=Outcome::ongoing)advance_turn_time();
    return true;
}

std::string Session::save() const
{
    // The module owns the checkpoint format, including RNG and pending reactions.
    // Combat checkpoint format, one row per feature that widened it. Thirteen is
    // the floor; the highest present feature decides, matching the descending
    // if-else chain this replaced.
    const unsigned format=highest_present(13,{
        {21,std::any_of(actors_.begin(),actors_.end(),[](const auto& a){return a.definition.sneak_level!=0;})},
        {20,std::any_of(actors_.begin(),actors_.end(),[](const auto& a){return a.arcane<a.definition.arcane;})},
        {19,physical_inventory_},
        {18,bool(champion_move_)},
        {17,bool(check_choice_)},
        {16,items_active_},
        {15,frost_movement_},
        {14,std::any_of(actors_.begin(),actors_.end(),[](const auto& a){return a.definition.surges>0;})}});
    std::ostringstream out;out<<"OGCOMBAT "<<format<<' '<<std::quoted(content_->identity.module)<<' '<<std::quoted(content_->identity.version)<<' '<<std::quoted(content_->identity.content)<<'\n';
    out<<board_.width<<' '<<board_.height<<'\n';for(auto cell:board_.terrain)out<<unsigned(cell)<<' ';out<<'\n';
    out<<rng_<<' '<<revision_<<' '<<turn_<<' '<<round_<<' '<<static_cast<int>(outcome_)<<' '<<actors_.size()<<'\n';
    for(const auto& a:actors_){out<<a.source.id<<' '<<std::quoted(a.source.definition)<<' '<<std::quoted(a.source.name)<<' '<<a.source.side<<' '<<a.source.cell.x<<' '<<a.source.cell.y<<' '
        <<a.hp<<' '<<a.initiative<<' '<<a.movement<<' '<<a.winds<<' '<<a.slots<<' '<<a.successes<<' '<<a.failures<<' '
        <<a.actions.normal<<' '<<a.bonus<<' '<<a.reaction<<' '<<a.dodge<<' '<<a.disengaged<<' '<<a.stable<<' '<<a.dead<<' '<<std::quoted(a.source.character_profile)<<' '<<a.slots2<<' '<<a.spent_slot<<' '<<a.savage_used<<' '<<a.facing_left<<' '<<a.involuntary_overlap<<' '<<a.weapon_hands<<' '<<a.hit_dice<<' '<<a.recovery.death_save_in_ms<<' '<<detail::encode_stable_recovery(a.recovery)<<' '<<a.temporary_hp.amount<<' '<<std::quoted(a.temporary_hp.source_id)<<' '<<a.rushes<<' '<<a.rush_used;
        if(format>=14)out<<' '<<a.surges<<' '<<a.surge_used<<' '<<a.actions.surge;if(format>=15)out<<' '<<a.dashes;if(format>=20)out<<' '<<a.arcane;if(format>=21)out<<' '<<a.sneak_used<<' '<<a.aim_used<<' '<<a.aim_ready<<' '<<a.moved;out<<'\n';}
    out<<path_.size()<<' '<<path_index_<<'\n';for(auto p:path_)out<<p.x<<' '<<p.y<<' ';out<<'\n';
    out<<reactors_.size()<<' '<<reactor_index_<<'\n';for(auto id:reactors_)out<<id<<' ';out<<'\n';
    out<<log_.size()<<'\n';for(const auto& line:log_)out<<std::quoted(line)<<'\n';
    out<<scope_<<' '<<elapsed_ms_<<' '<<actors_.size()<<'\n';
    for(const auto& a:actors_){detail::write_effects(out,a.effects);out<<'\n';}
    out<<bool(temporary_offer_)<<'\n';
    if(temporary_offer_)out<<temporary_offer_->amount<<' '<<std::quoted(temporary_offer_->source_id)<<'\n';
    out<<bool(weapon_hit_)<<'\n';
    if(weapon_hit_){const auto& h=*weapon_hit_;out<<h.attacker<<' '<<h.target<<' '<<h.ranged<<' '<<h.natural<<' '<<h.mode<<' '<<h.first<<' '<<h.second.value_or(-1);if(format>=19)out<<' '<<h.thrown_item;if(format>=21)out<<' '<<h.second.has_value()<<' '<<h.sneak_pending<<' '<<h.aimed<<' '<<h.sneak_extra;out<<'\n';}
    if(format>=17)out<<frost_movement_<<' '<<items_active_<<'\n';
    if(format>=20)out<<physical_inventory_<<'\n';
    if(items_active_){
        out<<items_.size()<<'\n';
        for(const auto& item:items_){out<<item.id<<' ';
            if(format>=19)out<<item.origin<<' '<<item.equipment_index<<' '<<item.inventory_id<<' '<<std::quoted(item.definition)<<' '<<item.quantity<<' '<<item.stowed<<' ';
            out<<item.holder<<' '<<item.cell.x<<' '<<item.cell.y<<'\n';}
        for(const auto& a:actors_)out<<a.object_interaction<<' ';out<<'\n';
    }
    if(format>=19)out<<bool(check_choice_)<<'\n';
    if(format==17||(format>=19&&check_choice_)){const auto& c=*check_choice_;out<<c.actor<<' '<<c.target<<' '<<c.natural<<' '<<c.surge_spent<<'\n';}
    if(format>=19)out<<bool(champion_move_)<<'\n';
    if(format==18||(format>=19&&champion_move_)){const auto& c=*champion_move_;out<<c.actor<<' '<<c.target<<' '<<c.natural<<' '<<c.remaining<<' '<<c.spell<<' '<<c.origin.x<<' '<<c.origin.y<<'\n';}
    return out.str();
}
// Parse one actor independently of session mutation. Old checkpoint versions
// omit later fields; Actor's value initializers supply their original defaults.
Actor read_checkpoint_actor(std::istream& input, unsigned version, const Content& content)
{
    Actor actor;
    auto& source = actor.source;
    input >> source.id >> std::quoted(source.definition) >> std::quoted(source.name)
          >> source.side >> source.cell.x >> source.cell.y
          >> actor.hp >> actor.initiative >> actor.movement >> actor.winds >> actor.slots
          >> actor.successes >> actor.failures >> actor.actions.normal >> actor.bonus >> actor.reaction
          >> actor.dodge >> actor.disengaged >> actor.stable >> actor.dead;
    if (version >= 2) input >> std::quoted(source.character_profile);
    if (version >= 3) input >> actor.slots2 >> actor.spent_slot >> actor.savage_used;
    if (version >= 5) input >> actor.facing_left;
    if (version >= 7) input >> actor.involuntary_overlap;
    if (version >= 8) input >> actor.weapon_hands;
    if (version >= 9) input >> actor.hit_dice;
    if (version >= 10){input >> actor.recovery.death_save_in_ms >> actor.recovery.stable_recovery_in_ms;detail::decode_stable_recovery(actor.recovery);}
    if (version >= 11) input >> actor.temporary_hp.amount >> std::quoted(actor.temporary_hp.source_id);
    if(version>=12)input>>actor.rushes>>actor.rush_used;
    if(version>=14)input>>actor.surges>>actor.surge_used>>actor.actions.surge;
    if(version>=15)input>>actor.dashes;
    if(version>=20)input>>actor.arcane;
    if(version>=21)input>>actor.sneak_used>>actor.aim_used>>actor.aim_ready>>actor.moved;
    if (!input || (source.character_profile.empty() && !content.definitions.contains(source.definition)))
        throw std::runtime_error("Invalid checkpoint actor");
    actor.definition = source.character_profile.empty()
        ? content.definitions.at(source.definition) : character_definition(source.character_profile);
    const auto& definition = actor.definition;
    if(version<20)actor.arcane=definition.arcane;
    if(version<21&&definition.sneak_level)throw std::runtime_error("Rogue attacks require version-21 checkpoint");
    if((actor.sneak_used&&!definition.sneak_level)||(actor.aim_used&&(definition.sneak_level<3||actor.bonus||actor.moved))||(actor.aim_ready&&!actor.aim_used))throw std::runtime_error("Invalid Rogue attack expenditure");
    if(version<15&&(detail::knows_spell(definition.known_cantrips,"ray_of_frost")||definition.cunning))throw std::runtime_error("Movement features require a version-15 checkpoint");
    if(version>=15&&(actor.dashes<0||actor.dashes>int(!actor.actions.normal)+int(actor.rush_used||(definition.cunning&&!actor.bonus))+int(actor.surge_used&&!actor.actions.surge)||actor.movement>definition.speed*(1+actor.dashes)))throw std::runtime_error("Invalid Dash allowance count");
    if(version<14&&definition.surges)throw std::runtime_error("Action Surge requires a version-14 checkpoint");
    if(version<12)actor.rushes=definition.rushes;
    if(version<9)actor.hit_dice=definition.hit_die?definition.level:0;
    if(version<8)actor.weapon_hands=definition.weapon_hands;
    if(version<16)validate_grip(definition,actor.weapon_hands);
    if (actor.hp < 0 || actor.hp > definition.hp || (actor.dead && actor.hp > 0) ||
        // Dash spends the action before adding a second movement allowance.
        // Accepting both extra movement and an unused action lets a later Dash
        // create a state outside the checkpoint's own movement bounds.
        actor.movement < 0 || actor.movement > definition.speed*(1+!actor.actions.normal+int(actor.rush_used||(definition.cunning&&!actor.bonus))+(actor.surge_used&&!actor.actions.surge)) ||
        actor.arcane<0||actor.arcane>definition.arcane||actor.surges<0||actor.surges>definition.surges||
        (actor.surge_used&&(!definition.surges||actor.surges==definition.surges))||
        (actor.actions.surge&&!actor.surge_used)||
        actor.rushes<0||actor.rushes>definition.rushes||
        (actor.rush_used&&(actor.bonus||!definition.rushes||actor.rushes==definition.rushes))||
        actor.winds < 0 || actor.winds > definition.winds ||
        actor.slots < 0 || actor.slots > definition.slots ||
        actor.slots2 < 0 || actor.slots2 > definition.slots2 ||
        actor.hit_dice < 0 || actor.hit_dice > (definition.hit_die?definition.level:0) ||
        actor.successes < 0 || actor.successes > 3 || actor.failures < 0 || actor.failures > 4)
        throw std::runtime_error("Invalid checkpoint actor state");
    if(version<10)detail::initialize_legacy_recovery(actor);
    detail::validate_recovery(actor);detail::validate_temporary_hp(actor.temporary_hp);
    return actor;
}

Battlefield read_checkpoint_board(std::istream& input)
{
    Battlefield board;
    input >> board.width >> board.height;
    // Bound dimensions before multiplication or allocation, even for truncated
    // input. No serialized count is allowed to control an unbounded allocation.
    if (!input || board.width < 2 || board.height < 2 || board.width > 64 || board.height > 64)
        throw std::runtime_error("Invalid checkpoint board");
    for (int i = 0; i < board.width*board.height; ++i) {
        unsigned terrain{};
        input >> terrain;
        if (!input || terrain > 2) throw std::runtime_error("Invalid checkpoint terrain");
        board.terrain.push_back(static_cast<std::uint8_t>(terrain));
    }
    return board;
}

void Session::restore_movement(std::istream& input)
{
    std::size_t count{};
    input >> count >> path_index_;
    if (!input || count > 1024 || path_index_ > count)
        throw std::runtime_error("Invalid checkpoint path");
    for (std::size_t i = 0; i < count; ++i) {
        Cell cell;
        input >> cell.x >> cell.y;
        if (!input || board_.at(cell) == 1) throw std::runtime_error("Invalid checkpoint path cell");
        path_.push_back(cell);
    }
    input >> count >> reactor_index_;
    if (!input || count > 64 || reactor_index_ > count)
        throw std::runtime_error("Invalid checkpoint reactions");
    std::set<EntityId> seen;
    for (std::size_t i = 0; i < count; ++i) {
        EntityId id{};
        input >> id;
        const auto exists = std::any_of(actors_.begin(), actors_.end(),
                                       [id](const auto& actor) { return actor.source.id == id; });
        if (!input || !seen.insert(id).second || !exists)
            throw std::runtime_error("Invalid checkpoint reactor");
        reactors_.push_back(id);
    }
}

void Session::validate_weapon_hit() const
{
    if(!weapon_hit_)return;
    const auto& h=*weapon_hit_;
    const auto a=std::find_if(actors_.begin(),actors_.end(),[&](const auto& v){return v.source.id==h.attacker;});
    const auto t=std::find_if(actors_.begin(),actors_.end(),[&](const auto& v){return v.source.id==h.target;});
    if(a==actors_.end()||t==actors_.end())throw std::runtime_error("Unknown pending attacker/target");
    auto attacking=hit_actor(h);attacking.aim_ready=h.aimed;
    const auto d=weapon_dice(attacking,h.ranged);const int count=d.count*(critical_hit(*a,*t,h.natural)?2:1);
    if(temporary_offer_||outcome_!=Outcome::ongoing||!conscious(*a)||t->hp<=0||t->dead||a->source.side==t->source.side||
        h.natural<2||h.natural>20||
        (!(def(*a).champion&&h.natural==19)&&!attack_hits(h.natural,h.ranged?def(attacking).ranged_bonus:def(attacking).melee_bonus,def(*t).ac))||
        h.mode!=attack_modifiers(attacking,*t,h.ranged,false).mode()||!line_of_sight(a->source.cell,t->source.cell)||
        distance(a->source.cell,t->source.cell)>(h.ranged?def(attacking).long_range:def(attacking).reach)||
        (pending()?(pending()!=h.attacker||h.target!=actors_[turn_].source.id||a->reaction||h.ranged):(h.attacker!=actors_[turn_].source.id||(a->actions.normal&&(!a->surge_used||a->actions.surge)))))
        throw std::runtime_error("Invalid pending weapon hit");
    if(h.aimed&&(!a->aim_used||a->aim_ready||def(*a).sneak_level<3||h.attacker!=actors_[turn_].source.id))throw std::runtime_error("Invalid aimed weapon hit");
    if(h.sneak_pending){
        if(h.second||h.sneak_extra||!sneak_eligible(attacking,*t,h.ranged,h.mode))throw std::runtime_error("Invalid pending Sneak Attack");
    }else if(!def(*a).savage||!count||a->savage_used!=h.second.has_value())throw std::runtime_error("Invalid pending Savage Attacker");
    if(h.sneak_extra){
        attacking.sneak_used=false;
        const int extra_count=int((def(*a).sneak_level+1)/2)*(critical_hit(*a,*t,h.natural)?2:1);
        if(!a->sneak_used||!sneak_eligible(attacking,*t,h.ranged,h.mode)||h.sneak_extra<extra_count||h.sneak_extra>extra_count*6)throw std::runtime_error("Invalid Sneak Attack dice");
    }
    const auto bound=[&](int n){return def(attacking).sneak_level?n:std::max(0,n);};
    const auto valid=[&](int n){return n>=bound(count+d.bonus)&&n<=bound(count*d.sides+d.bonus);};
    if(!valid(h.first)||(h.second&&!valid(*h.second)))throw std::runtime_error("Invalid pending weapon damage roll");
}
void Session::finish_check(const PendingCheck& check,int boost)
{
    auto& a=actor(check.actor);auto& target=actor(check.target);
    const int total=check.natural+def(a).medicine+boost;const bool success=total>=10;
    if(success)detail::stabilize(target,rng_);
    Message message{"{actor} stabilizes {target}: d20 {roll} + {modifier} + {boost} = {total} vs DC {dc}: {result}.",
        {{"actor",a.source.name},{"target",target.source.name},{"roll",std::to_string(check.natural)},
         {"modifier",std::to_string(def(a).medicine)},{"boost",std::to_string(boost)},{"total",std::to_string(total)},
         {"dc","10"},{"result",success?"Success":"Failure",true}}};
    log(a.source.name+" Medicine: "+std::to_string(check.natural)+" + "+std::to_string(def(a).medicine)+" + "+std::to_string(boost)+" = "+std::to_string(total)+" vs DC 10: "+(success?"success":"failure"),message);
}
void Session::finish_champion_move()
{
    champion_move_.reset();
    if(!pending())return;
    const auto& mover=actors_[turn_];
    // The critical mover can now occupy the interrupted route. Stop a route
    // that is no longer legal; never walk through the newly occupied cell.
    const auto grid=movement_grid(mover);auto cell=mover.source.cell;int budget=movement_left(mover);bool valid=true;
    for(auto i=path_index_;i<path_.size();++i){const auto cost=grid.step_cost(cell,path_[i]);if(!cost||*cost>budget){valid=false;break;}budget-=*cost;cell=path_[i];}
    if(!valid||!grid.can_stop_at(cell)){path_.clear();path_index_=0;reactors_.clear();reactor_index_=0;return;}
    finish_reaction();
}
void Session::validate_champion_move() const
{
    if(!champion_move_)return;const auto& c=*champion_move_;
    const auto who=std::find_if(actors_.begin(),actors_.end(),[&](const auto& a){return a.source.id==c.actor;});
    const auto target=std::find_if(actors_.begin(),actors_.end(),[&](const auto& a){return a.source.id==c.target;});
    if(who==actors_.end()||target==actors_.end()||c.actor==c.target||!def(*who).champion||!conscious(*who)||
       c.natural<2||c.natural>20||c.origin.x<0||c.origin.y<0||c.origin.x>=board_.width||c.origin.y>=board_.height||board_.at(c.origin)==1||
       check_choice_||weapon_hit_||temporary_offer_||outcome_!=Outcome::ongoing)throw std::runtime_error("Invalid Champion movement source");
    const int budget=std::max(0,def(*who).speed-detail::speed_penalty(who->effects))/2;
    if((c.actor==actors_[turn_].source.id&&(who->actions.surge||(who->actions.normal&&!who->surge_used)))||
       c.remaining<0||c.remaining>budget||distance(c.origin,who->source.cell)>budget-c.remaining||
       !(c.natural==20||(!c.spell&&c.natural==19)||(target->hp==0&&distance(c.origin,target->source.cell)<=5))||
       (pending()?(pending()!=c.actor||who->reaction||c.target!=actors_[turn_].source.id):(c.actor!=actors_[turn_].source.id&&!(c.target==actors_[turn_].source.id&&target->hp==0&&!who->reaction))))
        throw std::runtime_error("Invalid Champion movement allowance/trigger");
}
void Session::validate_check() const
{
    if(!check_choice_)return;
    const auto& c=*check_choice_;const auto& a=actors_[turn_];
    const auto target=std::find_if(actors_.begin(),actors_.end(),[&](const auto& t){return t.source.id==c.target;});
    if(c.actor!=a.source.id||!conscious(a)||!def(a).tactical_mind||a.winds<=0||c.natural<1||c.natural>20||
       c.natural+def(a).medicine>=10||target==actors_.end()||target->hp!=0||target->dead||target->stable||
       distance(a.source.cell,target->source.cell)>5||!line_of_sight(a.source.cell,target->source.cell)||
       pending()||temporary_offer_||weapon_hit_||outcome_!=Outcome::ongoing||a.actions.surge||
       (c.surge_spent?!a.surge_used:a.actions.normal))throw std::runtime_error("Invalid pending ability check");
}
void Session::validate_restored_state(bool legacy_facing_reaction) const
{
    validate_weapon_hit();validate_check();validate_champion_move();
    if (pending() && !legacy_facing_reaction && path_index_ >= path_.size())
        throw std::runtime_error("Reaction without movement");
    const auto& mover = actors_[turn_];
    if(temporary_offer_&&(outcome_!=Outcome::ongoing||pending()||!conscious(mover)||!mover.rush_used||mover.bonus||
        mover.temporary_hp.amount<=0||temporary_offer_->amount!=mover.definition.rushes||temporary_offer_->source_id!=rush_source))
        throw std::runtime_error("Invalid pending Temporary HP replacement");
    bool party = false, enemies = false;
    for (const auto& actor : actors_) {
        if(actor.aim_used&&actor.source.id!=mover.source.id)throw std::runtime_error("Steady Aim outside current turn");
        if(actor.actions.surge&&actor.source.id!=mover.source.id)throw std::runtime_error("Action Surge allowance outside its turn");
        if(actor.involuntary_overlap&&(actor.dead||!shares_occupied_space(actor)))
            throw std::runtime_error("Invalid involuntary checkpoint overlap");
        if(actor.hp>0&&!actor.dead)(actor.source.side==0?party:enemies)=true;
        if(actor.dead)continue;
        for (const auto& other : actors_) {
            if(other.source.id<=actor.source.id||other.dead||actor.source.cell!=other.source.cell)continue;
            const bool in_transit=pending()&&!legacy_facing_reaction&&path_index_>0&&
                path_[path_index_-1]==mover.source.cell&&
                (actor.source.id==mover.source.id||other.source.id==mover.source.id);
            if((in_transit&&(actor.source.side==other.source.side||unconscious(actor)||unconscious(other)))||
                actor.involuntary_overlap||other.involuntary_overlap)continue;
            throw std::runtime_error("Invalid overlapping checkpoint actors");
        }
    }
    const auto expected = !party ? Outcome::defeat : !enemies ? Outcome::victory : Outcome::ongoing;
    if (outcome_ != expected || (expected == Outcome::ongoing && mover.hp == 0&&!champion_move_))
        throw std::runtime_error("Invalid checkpoint outcome/turn");
    if (!pending() && (!path_.empty() || !reactors_.empty() || legacy_facing_reaction))
        throw std::runtime_error("Unpaused checkpoint movement");
    if (legacy_facing_reaction) validate_legacy_facing_reaction();
    else if (pending()) validate_pending_movement();
}

void Session::validate_legacy_facing_reaction() const
{
    const auto& attacker=actors_[turn_];
    if(!pending()||!path_.empty()||path_index_||!conscious(attacker)||attacker.actions.normal)
        throw std::runtime_error("Invalid pending turn reaction");
    for(auto i=reactor_index_;i<reactors_.size();++i){
        const auto found=std::find_if(actors_.begin(),actors_.end(),[&](const auto& a){return a.source.id==reactors_[i];});
        const auto& reactor=*found;
        if(detail::opportunity_blocked(reactor.effects)||reactor.hp<=0||!reactor.reaction||reactor.source.side==attacker.source.side||
            (attacker.facing_left?reactor.source.cell.x<=attacker.source.cell.x:reactor.source.cell.x>=attacker.source.cell.x)||
            distance(reactor.source.cell,attacker.source.cell)>def(reactor).reach||!can_see(reactor,attacker))
            throw std::runtime_error("Invalid pending turn opportunity attack");
    }
}

void Session::validate_pending_movement() const
{
    const auto& mover = actors_[turn_];
    if (outcome_ != Outcome::ongoing || mover.disengaged)
        throw std::runtime_error("Invalid pending movement");
    std::vector<detail::Occupant> occupants;
    for(const auto& other:actors_)if(!other.dead&&other.source.id!=mover.source.id)
        occupants.push_back({champion_move_&&other.source.id==champion_move_->actor?champion_move_->origin:other.source.cell,other.source.side!=mover.source.side,unconscious(other)});
    const detail::MovementGrid grid{board_,mover.source.cell,occupants,mover.effects.prone};
    auto cell = mover.source.cell;
    int remaining = movement_left(mover);
    // Only the suffix remains to be travelled. The prefix is history and has
    // already spent its movement budget; charging for it again breaks restores.
    for (auto i = path_index_; i < path_.size(); ++i) {
        const auto next = path_[i];
        const auto cost = grid.step_cost(cell, next);
        if (!cost || *cost > remaining) throw std::runtime_error("Invalid checkpoint movement step/budget");
        remaining -= *cost;
        cell = next;
    }
    if (!grid.can_stop_at(cell)) throw std::runtime_error("Invalid checkpoint movement destination");
    for (auto i = reactor_index_; i < reactors_.size(); ++i) {
        const auto reactor = std::find_if(actors_.begin(), actors_.end(),
            [&](const auto& actor) { return actor.source.id == reactors_[i]; });
        // restore_movement already established that every reactor ID exists.
        const auto& actor = *reactor;
        if(champion_move_&&i==reactor_index_&&actor.source.id==champion_move_->actor){
            if(actor.source.side==mover.source.side||distance(champion_move_->origin,mover.source.cell)>def(actor).reach||distance(champion_move_->origin,path_[path_index_])<=def(actor).reach)
                throw std::runtime_error("Invalid Champion reaction origin");
            continue;
        }
        if (detail::opportunity_blocked(actor.effects) || actor.hp == 0 || (!actor.reaction&&!(weapon_hit_&&weapon_hit_->attacker==actor.source.id&&i==reactor_index_)) || actor.source.side == mover.source.side ||
            distance(actor.source.cell, mover.source.cell) > def(actor).reach ||
            distance(actor.source.cell, path_[path_index_]) <= def(actor).reach ||
            !can_see(actor,mover))
            throw std::runtime_error("Invalid checkpoint opportunity attack");
    }
}

void Session::restore_log(std::istream& input)
{
    std::size_t count{};
    input >> count;
    if (!input || count > 80) throw std::runtime_error("Invalid checkpoint log");
    log_.clear();
    log_messages_.clear();
    for (std::size_t i = 0; i < count; ++i) {
        std::string line;
        input >> std::quoted(line);
        if (!input || line.size() > 1000) throw std::runtime_error("Invalid checkpoint log line");
        log_messages_.push_back({line,{}});
        log_.push_back(std::move(line));
    }
}

std::unique_ptr<Session> Session::restore(std::shared_ptr<const Content> content, std::string_view bytes)
{
    if (bytes.size() > 4*1024*1024) throw std::runtime_error("Combat checkpoint exceeds limit");
    std::istringstream input{std::string(bytes)};
    std::string magic;
    unsigned version{};
    Identity identity;
    input >> magic >> version >> std::quoted(identity.module)
          >> std::quoted(identity.version) >> std::quoted(identity.content);
    auto compatible_identity=identity;compatible_identity.version=content->identity.version;
    const bool previous_module=(((version>=13&&version<=20)&&identity.version=="0.6.51")||((version>=13&&version<=20)&&identity.version=="0.6.50")||((version>=13&&version<=20)&&identity.version=="0.6.49")||((version>=13&&version<=19)&&identity.version=="0.6.48")||((version>=13&&version<=19)&&identity.version=="0.6.47")||((version>=13&&version<=18)&&identity.version=="0.6.46")||((version>=13&&version<=17)&&identity.version=="0.6.45")||(version==5&&identity.version=="0.6.4")||
        ((version>=13&&version<=16)&&(identity.version=="0.6.42"||identity.version=="0.6.43"||identity.version=="0.6.44"))||(version==6&&identity.version=="0.6.5")||(version==7&&identity.version=="0.6.6")||(version==8&&(identity.version=="0.6.7"||identity.version=="0.6.8"||identity.version=="0.6.9"))||(version==9&&identity.version=="0.6.10")||(version==10&&(identity.version=="0.6.11"||identity.version=="0.6.12"||identity.version=="0.6.13"))||(version==11&&identity.version=="0.6.14")||(version==12&&(identity.version=="0.6.15"||identity.version=="0.6.16"||identity.version=="0.6.17"||identity.version=="0.6.18"||identity.version=="0.6.19"))||(version==13&&(identity.version=="0.6.20"||identity.version=="0.6.21"||identity.version=="0.6.22"||identity.version=="0.6.23"))||((version==13||version==14)&&identity.version=="0.6.24")||((version>=13&&version<=15)&&(identity.version=="0.6.25"||identity.version=="0.6.26"||identity.version=="0.6.27"||identity.version=="0.6.28"||identity.version=="0.6.29"||identity.version=="0.6.30"||identity.version=="0.6.31"||identity.version=="0.6.32"||identity.version=="0.6.33"||identity.version=="0.6.34"||identity.version=="0.6.35"||identity.version=="0.6.36"||identity.version=="0.6.37"||identity.version=="0.6.38"||identity.version=="0.6.39"||identity.version=="0.6.40"||identity.version=="0.6.41")))&&(compatible_identity==content->identity||
            (compatible_identity.module==content->identity.module&&compatible_identity.content=="srd-5.2.1-demo.1/15052881321234871607"&&
             content->previous_campaign_identities.end()!=std::find(content->previous_campaign_identities.begin(),content->previous_campaign_identities.end(),compatible_identity)));
    if (!input || magic != "OGCOMBAT" || version < 1 || version > 21 ||
        (identity != content->identity && !previous_module))
        throw std::runtime_error("Combat checkpoint rules/content version mismatch");
    Encounter encounter;
    encounter.battlefield = read_checkpoint_board(input);
    std::uint64_t rng{}, revision{};
    unsigned turn{}, round{}, outcome{}, count{};
    input >> rng >> revision >> turn >> round >> outcome >> count;
    if (!input || count < 2 || count > 64 || turn >= count || round == 0 || outcome > 2 || !revision)
        throw std::runtime_error("Invalid checkpoint header");
    std::vector<Actor> actors;
    for (unsigned i = 0; i < count; ++i) {
        auto actor = read_checkpoint_actor(input, version, *content);
        if(module_before(identity,{0,6,52})&&(version>=21||actor.source.character_profile.starts_with("PC35 ")))throw std::runtime_error("Legacy checkpoint cannot contain Rogue attack profiles");
        if(module_before(identity,{0,6,51})&&actor.source.character_profile.starts_with("PC34 "))throw std::runtime_error("Legacy checkpoint cannot contain spell-choice profiles");
        if(module_before(identity,{0,6,50})&&actor.source.character_profile.starts_with("PC33 "))throw std::runtime_error("Legacy checkpoint cannot contain Scholar profiles");
        if(module_before(identity,{0,6,49})&&actor.source.character_profile.starts_with("PC32 "))throw std::runtime_error("Legacy checkpoint cannot contain Arcane Recovery profiles");
        if(module_before(identity,{0,6,26})&&(actor.source.character_profile.starts_with("PC15 ")||actor.source.character_profile.starts_with("PC16 ")||actor.source.character_profile.starts_with("PC17 ")||(actor.source.character_profile.starts_with("PC18 ")||(actor.source.character_profile.starts_with("PC19 ")||(actor.source.character_profile.starts_with("PC20 ")||(actor.source.character_profile.starts_with("PC21 ")||(actor.source.character_profile.starts_with("PC22 ")||(actor.source.character_profile.starts_with("PC23 ")||actor.source.character_profile.starts_with("PC24 ")))))))))
            throw std::runtime_error("Legacy checkpoint cannot contain a Sage-training profile");
        if(module_before(identity,{0,6,27})&&(actor.source.character_profile.starts_with("PC16 ")||actor.source.character_profile.starts_with("PC17 ")||(actor.source.character_profile.starts_with("PC18 ")||(actor.source.character_profile.starts_with("PC19 ")||(actor.source.character_profile.starts_with("PC20 ")||(actor.source.character_profile.starts_with("PC21 ")||(actor.source.character_profile.starts_with("PC22 ")||(actor.source.character_profile.starts_with("PC23 ")||actor.source.character_profile.starts_with("PC24 ")))))))))
            throw std::runtime_error("Legacy checkpoint cannot contain the completed fixed-background profile");
        if(module_before(identity,{0,6,28})&&(actor.source.character_profile.starts_with("PC17 ")||(actor.source.character_profile.starts_with("PC18 ")||(actor.source.character_profile.starts_with("PC19 ")||(actor.source.character_profile.starts_with("PC20 ")||(actor.source.character_profile.starts_with("PC21 ")||(actor.source.character_profile.starts_with("PC22 ")||(actor.source.character_profile.starts_with("PC23 ")||actor.source.character_profile.starts_with("PC24 ")))))))))
            throw std::runtime_error("Legacy checkpoint cannot contain an Archery profile");
        if(module_before(identity,{0,6,29})&&(actor.source.character_profile.starts_with("PC18 ")||(actor.source.character_profile.starts_with("PC19 ")||(actor.source.character_profile.starts_with("PC20 ")||(actor.source.character_profile.starts_with("PC21 ")||(actor.source.character_profile.starts_with("PC22 ")||(actor.source.character_profile.starts_with("PC23 ")||actor.source.character_profile.starts_with("PC24 "))))))))
            throw std::runtime_error("Legacy checkpoint cannot contain a starting-style profile");
        if(module_before(identity,{0,6,46})&&actor.source.character_profile.starts_with("PC31 "))throw std::runtime_error("Legacy checkpoint cannot contain Champion profiles");
        if(module_before(identity,{0,6,45})&&actor.source.character_profile.starts_with("PC30 "))throw std::runtime_error("Legacy checkpoint cannot contain Tactical Mind profiles");
        if(module_before(identity,{0,6,43})&&actor.source.character_profile.starts_with("PC29 "))throw std::runtime_error("Legacy checkpoint cannot contain Chill Touch profiles");
        if(module_before(identity,{0,6,40})&&actor.source.character_profile.starts_with("PC28 "))throw std::runtime_error("Legacy checkpoint cannot contain Sorcerer cantrip profiles");
        if(module_before(identity,{0,6,39})&&actor.source.character_profile.starts_with("PC27 "))throw std::runtime_error("Legacy checkpoint cannot contain Warlock Poison Spray profiles");
        if(module_before(identity,{0,6,38})&&actor.source.character_profile.starts_with("PC26 "))throw std::runtime_error("Legacy checkpoint cannot contain Shocking Grasp profiles");
        if(module_before(identity,{0,6,37})&&actor.source.character_profile.starts_with("PC25 "))throw std::runtime_error("Legacy checkpoint cannot contain Warlock cantrip profiles");
        if(module_before(identity,{0,6,35})&&actor.source.character_profile.starts_with("PC24 "))throw std::runtime_error("Legacy checkpoint cannot contain Cunning Action profiles");
        if(module_before(identity,{0,6,34})&&(actor.source.character_profile.starts_with("PC23 ")||actor.source.character_profile.starts_with("PC24 ")))
            throw std::runtime_error("Legacy combat cannot contain Gaming Set profiles");
        if(module_before(identity,{0,6,33})&&(actor.source.character_profile.starts_with("PC22 ")||(actor.source.character_profile.starts_with("PC23 ")||actor.source.character_profile.starts_with("PC24 "))))
            throw std::runtime_error("Legacy combat cannot contain Herbalism Kit profiles");
        if(module_before(identity,{0,6,32})&&(actor.source.character_profile.starts_with("PC21 ")||(actor.source.character_profile.starts_with("PC22 ")||(actor.source.character_profile.starts_with("PC23 ")||actor.source.character_profile.starts_with("PC24 ")))))
            throw std::runtime_error("Legacy combat cannot contain Monk tool profiles");
        if(module_before(identity,{0,6,31})&&(actor.source.character_profile.starts_with("PC20 ")||(actor.source.character_profile.starts_with("PC21 ")||(actor.source.character_profile.starts_with("PC22 ")||(actor.source.character_profile.starts_with("PC23 ")||actor.source.character_profile.starts_with("PC24 "))))))
            throw std::runtime_error("Legacy combat cannot contain Bard instrument profiles");
        if(module_before(identity,{0,6,30})&&(actor.source.character_profile.starts_with("PC19 ")||(actor.source.character_profile.starts_with("PC20 ")||(actor.source.character_profile.starts_with("PC21 ")||(actor.source.character_profile.starts_with("PC22 ")||(actor.source.character_profile.starts_with("PC23 ")||actor.source.character_profile.starts_with("PC24 ")))))))
            throw std::runtime_error("Legacy checkpoint cannot contain an all-class skill profile");
        encounter.participants.push_back(actor.source);
        actors.push_back(std::move(actor));
    }
    if(module_before(identity,{0,6,36}))for(const auto& a:actors)for(const auto& b:actors)
        if(a.source.id<b.source.id&&!a.dead&&!b.dead&&a.source.side!=b.source.side&&a.source.cell==b.source.cell)
            throw std::runtime_error("Legacy combat cannot contain enemy transit overlap");
    // Build and validate a separate owned candidate. Any failure destroys it;
    // callers never receive a partially restored session or lose a live one.
    // The constructor checks identities/geometry; saved order and RNG then
    // replace its fresh initiative state before relational checks run.
    auto session = std::make_unique<Session>(content, std::move(encounter), 0, true);
    session->actors_ = std::move(actors);session->frost_movement_=version>=15;
    session->rng_ = rng;
    session->revision_ = revision;
    session->turn_ = turn;
    session->round_ = round;
    session->outcome_ = static_cast<Outcome>(outcome);
    if(version<10)for(auto& a:session->actors_)if(a.hp==0&&!a.dead&&!a.stable)
        a.recovery.death_save_in_ms=session->next_turn_ms(a);
    session->restore_movement(input);
    session->restore_log(input);
    if(version>=4){
        unsigned effects_count{};input>>session->scope_>>session->elapsed_ms_>>effects_count;
        if(!input||!session->scope_||effects_count!=session->actors_.size())throw std::runtime_error("Invalid checkpoint effect header");
        for(auto& a:session->actors_){a.effects=detail::read_effects(input);
            if(a.recovery.stable_recovery_due&&(module_before(identity,{0,6,44})||!detail::healing_blocked(a.effects)))throw std::runtime_error("Invalid earned recovery checkpoint");if(module_before(identity,{0,6,43})&&detail::healing_blocked(a.effects))throw std::runtime_error("Legacy combat cannot contain Chill Touch");if(a.effects.sleeping&&(a.dead||a.hp<=0))throw std::runtime_error("Invalid naturally sleeping vitality");if(module_before(identity,{0,6,41})&&a.effects.prone)throw std::runtime_error("Legacy combat cannot contain natural sleep/posture state");if(module_before(identity,{0,6,38})&&detail::opportunity_blocked(a.effects))throw std::runtime_error("Legacy combat cannot contain Shocking Grasp");if(version<15&&detail::speed_penalty(a.effects))throw std::runtime_error("Legacy combat cannot contain Ray of Frost");}
    }
    if(version>=12){
        bool pending_offer{};input>>pending_offer;
        if(pending_offer){TemporaryHitPoints offer;input>>offer.amount>>std::quoted(offer.source_id);detail::validate_temporary_hp(offer);session->temporary_offer_=std::move(offer);}
        if(!input)throw std::runtime_error("Invalid Temporary HP choice checkpoint");
    }
    if(version>=13){
        bool pending_hit{};input>>pending_hit;
        if(pending_hit){PendingWeaponHit h;int second{};input>>h.attacker>>h.target>>h.ranged>>h.natural>>h.mode>>h.first>>second;if(second>=0)h.second=second;else if(second!=-1&&version<21)throw std::runtime_error("Invalid legacy second damage");if(version>=19)input>>h.thrown_item;if(version>=21){bool rolled{};input>>rolled>>h.sneak_pending>>h.aimed>>h.sneak_extra;if(rolled)h.second=second;else if(second!=-1)throw std::runtime_error("Unexpected second damage");}session->weapon_hit_=h;}
        if(!input)throw std::runtime_error("Invalid Savage Attacker choice checkpoint");
    }
    bool has_items=version>=16;
    if(version>=17){
        if(module_before(identity,{0,6,45}))throw std::runtime_error("Legacy checkpoint cannot contain ability-check choices");
        input>>session->frost_movement_>>has_items;
        if(!input)throw std::runtime_error("Invalid ability-check checkpoint flags");
    }
    if(version>=20){
        input>>session->physical_inventory_;
        if(!input||module_before(identity,{0,6,49})||(session->physical_inventory_&&!has_items))throw std::runtime_error("Invalid recovery checkpoint flags");
        session->items_active_=has_items;
    }else if(version>=19){
        if(module_before(identity,{0,6,47})||!has_items)throw std::runtime_error("Invalid physical inventory checkpoint flags");
        session->physical_inventory_=true;session->items_active_=true;
    }
    if(has_items){
        if(module_before(identity,{0,6,42}))throw std::runtime_error("Legacy checkpoint cannot contain held items");
        if(version<19)session->initialize_items();std::size_t count{};input>>count;
        if(!input||count>100000||(version<19&&count!=session->items_.size()))throw std::runtime_error("Invalid held item count");
        if(version>=19)session->items_.resize(count);
        for(unsigned index=0;index<count;++index){
            auto& item=session->items_[index];unsigned id{};input>>id;
            if(version>=19){
                item.id=index+1;input>>item.origin>>item.equipment_index>>item.inventory_id>>std::quoted(item.definition)>>item.quantity>>item.stowed;
                const auto source=std::find_if(session->actors_.begin(),session->actors_.end(),[&](const auto& a){return a.source.id==item.origin;});
                const auto* weapon=detail::weapon(item.definition);
                if(!input||source==session->actors_.end()||source->source.character_profile.empty()||(!weapon&&item.definition!="shield")||!item.quantity)
                    throw std::runtime_error("Invalid physical inventory source");
                const auto original=character_definition(source->source.character_profile);
                if(!item.inventory_id&&(item.equipment_index>=original.equipment_keys.size()||original.equipment_keys[item.equipment_index]!=item.definition))
                    throw std::runtime_error("Invalid physical equipment source");
                item.label={weapon?std::string(weapon->label):"Shield",{}};
            }
            input>>item.holder>>item.cell.x>>item.cell.y;
            if(!input||id!=item.id)throw std::runtime_error("Invalid held item identity");
            if(item.holder){
                const auto holder=std::find_if(session->actors_.begin(),session->actors_.end(),[&](const auto& a){return a.source.id==item.holder;});
                if(holder==session->actors_.end()||holder->source.character_profile.empty()||(!item.stowed&&(unconscious(*holder)||holder->dead))||item.cell!=Cell{})throw std::runtime_error("Invalid held item holder");
            }else if(!session->board_.contains(item.cell)||session->board_.at(item.cell)==1||item.stowed||item.quantity!=1)throw std::runtime_error("Invalid dropped item position");
        }
        for(auto& a:session->actors_){unsigned available{};input>>available;if(!input||available>1)throw std::runtime_error("Invalid object interaction budget");a.object_interaction=available;
            if(!a.source.character_profile.empty())a.definition=session->equipped_definition(a,session->items_);
            validate_grip(a.definition,a.weapon_hands);
        }
    }
    if(version>=17&&!has_items)for(const auto& a:session->actors_)validate_grip(a.definition,a.weapon_hands);
    bool pending_check=false,pending_champion=false;
    if(version>=19)input>>pending_check;
    if(version==17||pending_check){PendingCheck c;input>>c.actor>>c.target>>c.natural>>c.surge_spent;if(!input)throw std::runtime_error("Invalid ability-check checkpoint");session->check_choice_=c;}
    if(version>=19)input>>pending_champion;
    if(version==18||pending_champion){if(module_before(identity,{0,6,46}))throw std::runtime_error("Legacy checkpoint cannot contain Champion movement");ChampionMove c;input>>c.actor>>c.target>>c.natural>>c.remaining>>c.spell>>c.origin.x>>c.origin.y;if(!input)throw std::runtime_error("Invalid Champion movement checkpoint");session->champion_move_=c;}
    if(!input)throw std::runtime_error("Invalid checkpoint continuation");
    unsigned legacy_facing_reaction{};
    if(version==5){
        input>>legacy_facing_reaction;
        if(!input||legacy_facing_reaction>2)throw std::runtime_error("Invalid checkpoint turn reaction");
    }
    if(version<7)for(auto& a:session->actors_)
        if(a.hp==0&&!a.dead&&session->shares_occupied_space(a))a.involuntary_overlap=true;
    // Validate the old queue before removing it. Migration must not conceal a
    // malformed checkpoint or change damage, spent resources, time or dice.
    session->validate_restored_state(legacy_facing_reaction!=0);
    input >> std::ws;
    if (!input.eof()) throw std::runtime_error("Trailing checkpoint data");
    if(legacy_facing_reaction){
        session->reactors_.clear();session->reactor_index_=0;
        if(++session->revision_==0)session->revision_=1;
        session->validate_restored_state();
    }
    return session;
}
class Module final : public RulesModule {
public:
    explicit Module(Content content):content_(std::make_shared<const Content>(std::move(content))){}
    Identity identity() const override{return content_->identity;}
    bool accepts_campaign_identity(const Identity& saved) const override {
        if(saved.version!=content_->identity.version&&saved.version!="0.3.0"&&saved.version!="0.4.0"&&saved.version!="0.5.0"&&saved.version!="0.6.0"&&saved.version!="0.6.1"&&saved.version!="0.6.2"&&saved.version!="0.6.3"&&saved.version!="0.6.4"&&saved.version!="0.6.5"&&saved.version!="0.6.6"&&saved.version!="0.6.7"&&saved.version!="0.6.8"&&saved.version!="0.6.9"&&saved.version!="0.6.10"&&saved.version!="0.6.11"&&saved.version!="0.6.12"&&saved.version!="0.6.13"&&saved.version!="0.6.14"&&saved.version!="0.6.15"&&saved.version!="0.6.16"&&saved.version!="0.6.17"&&saved.version!="0.6.18"&&saved.version!="0.6.19"&&saved.version!="0.6.20"&&saved.version!="0.6.21"&&saved.version!="0.6.22"&&saved.version!="0.6.23"&&saved.version!="0.6.24"&&saved.version!="0.6.25"&&saved.version!="0.6.26"&&saved.version!="0.6.27"&&saved.version!="0.6.28"&&saved.version!="0.6.29"&&saved.version!="0.6.30"&&saved.version!="0.6.31"&&saved.version!="0.6.32"&&saved.version!="0.6.33"&&saved.version!="0.6.34"&&saved.version!="0.6.35"&&saved.version!="0.6.36"&&saved.version!="0.6.37"&&saved.version!="0.6.38"&&saved.version!="0.6.39"&&saved.version!="0.6.40"&&saved.version!="0.6.41"&&saved.version!="0.6.42"&&saved.version!="0.6.43"&&saved.version!="0.6.45"&&saved.version!="0.6.44"&&saved.version!="0.6.46"&&saved.version!="0.6.47"&&saved.version!="0.6.48"&&saved.version!="0.6.49"&&saved.version!="0.6.50"&&saved.version!="0.6.51")return false;
        auto compatible=saved;compatible.version=content_->identity.version;
        return compatible==content_->identity||std::find(content_->previous_campaign_identities.begin(),content_->previous_campaign_identities.end(),compatible)!=content_->previous_campaign_identities.end();
    }
    std::vector<std::string> supported_features() const override{return {"scholar","arcane_recovery","champion","stabilize","tactical_mind","chill_touch","shocking_grasp","eldritch_blast","initiative","movement","unconscious_enemy_transit","melee","ranged","critical_hits","dodge","dash","disengage","opportunity_attacks","facing","turn_opportunity_attacks","death_saves","second_wind","action_surge","cunning_dash","cunning_disengage","sneak_attack","steady_aim","ray_of_frost","fire_bolt","poison_spray","sacred_flame","cure_wounds","magic_missile","healing_word","scorching_ray","level_two_slots","manual_advancement","ability_score_improvement","defense","archery","savage_attacker","saving_throws","blinded","blindness","timed_effects","versatile","feature_grants","training_grants","rest_resources","hit_dice","recovery_clocks","campaign_recovery","typed_damage","damage_affinities","dwarven_poison_resistance","temporary_hp","adrenaline_rush","heavy_weapons","weapon_catalog","armor_catalog","wizard_spellbook","somatic_components","checkpoint"};}
    std::unique_ptr<CombatSession> create(Encounter e,std::uint64_t seed) const override{return std::make_unique<Session>(content_,std::move(e),seed);}
    std::unique_ptr<CombatSession> restore(std::string_view checkpoint) const override{return Session::restore(content_,checkpoint);}
    unsigned experience_for_level(unsigned level) const override
    {static constexpr unsigned thresholds[]{0,0,300,900,2700};if(level<1||level>4)throw std::runtime_error("Unsupported character level");return thresholds[level];}
    bool advance_character(CharacterSheet& sheet,VitalState& state) const override
    {return advance_character(sheet,state,default_advancement(sheet));}
    std::vector<TrainingChoiceGroup> training_options(const CharacterSheet& sheet) const override {
        if(sheet.character_class=="Wizard"&&sheet.level>=2)return {detail::scholar_options(sheet.grants)};
        return {};
    }
    AdvancementOptions advancement_options(const CharacterSheet& sheet) const override
    {
        if(sheet.level>=4||(sheet.character_class!="Fighter"&&sheet.character_class!="Cleric"&&sheet.character_class!="Wizard"&&sheet.character_class!="Rogue"))return {};
        AdvancementOptions result;result.level=sheet.level+1;
        if(sheet.character_class=="Wizard"&&result.level==2)result.training={detail::scholar_options(sheet.grants)};
        result.description="Fixed-average HP growth. Resources gain only their new capacity;\nexisting expenditure remains.";
        if(sheet.character_class=="Fighter"&&result.level==3)result.description="Champion: weapon/unarmed criticals on 19–20.\nAdvantage on Initiative and Strength (Athletics).\nCritical hit: optional half-Speed move, no opportunity attacks.";
        if(sheet.character_class=="Rogue"&&result.level>=3)result.description="Sneak Attack: 2d6. Steady Aim: Bonus Action; next attack roll has Advantage, Speed becomes 0.\nThief features, Hide and weapon mastery remain unavailable.";
        if(result.level==4)result.feats={
            {"ability_score_improvement","Ability points","Add 2 to one ability or 1 to two abilities; maximum 20."},
            {"defense","Defense","+1 AC while wearing armor. Requires the Fighter's Fighting Style feature.",detail::has_grant(sheet.grants,"feature:fighting_style")&&!detail::has_grant(sheet.grants,"feat:defense")},
            {"savage_attacker","Savage Attacker","Once per turn on a weapon hit, choose whether to roll weapon damage twice and keep either roll.",!detail::has_grant(sheet.grants,"feat:savage_attacker")},
            {"grappler","Grappler","Unavailable: grappling is not implemented.",false},
            {"magic_initiate","Magic Initiate","Unavailable: its complete spell-selection feature is not implemented.",false},
            {"archery","Archery","+2 to attack rolls with Ranged weapons. Requires Fighting Style.",detail::has_grant(sheet.grants,"feature:fighting_style")&&!detail::has_grant(sheet.grants,"feat:archery")}};
        if(sheet.character_class=="Cleric")result.spells={
            {"cure_wounds","Cure Wounds","Action; touch; heals 2d8 + Wisdom modifier."},
            {"healing_word","Healing Word","Bonus action; 60 feet; heals 2d4 + Wisdom modifier."},
            {"blindness","Blindness","Blindness/Deafness (blindness option): Constitution save; repeat at end of turn; up to 1 minute. Level 2 slot.",result.level>=3},
            {"inflict_wounds","Inflict Wounds","Action; melee spell attack; 3d10 Necrotic damage, +1d10 from a level 2 slot."},
            {"bless","Bless","Unavailable: concentration is not implemented.",false}};
        if(sheet.character_class=="Wizard")result.spells={
            {"magic_missile","Magic Missile","Action; 120 feet; three darts at one target."},
            {"scorching_ray","Scorching Ray","Action; 120 feet; three spell attacks at one target. Requires level 3.",result.level>=3},
            {"blindness","Blindness","Blindness/Deafness (blindness option): Constitution save; repeat at end of turn; up to 1 minute. Level 2 slot.",result.level>=3},
            {"shield","Shield","Unavailable: spell reactions are not implemented.",false}};
        return result;
    }
    AdvancementChoice default_advancement(const CharacterSheet& sheet) const override
    {
        AdvancementChoice choice;const auto options=advancement_options(sheet);if(!options.level)return choice;
        choice.spells=sheet.prepared_spells;
        for(const auto& group:options.training)if(!group.options.empty())choice.training[group.id]={group.options.front().id};
        if(choice.spells.empty()){if(sheet.character_class=="Cleric")choice.spells={"cure_wounds"};else if(sheet.character_class=="Wizard")choice.spells={"magic_missile"};}
        if(sheet.character_class=="Wizard"){
            auto next=sheet;next.level=options.level;choice.spell_learning.emplace();
            for(const auto& group:detail::spell_choice_options(next,SpellChoiceContext::advancement).learning){
                auto& values=(*choice.spell_learning)[group.id];
                for(const auto& option:group.options){if(values.size()==group.count)break;values.push_back(option.id);}
            }
            detail::apply_spell_choices(next,SpellChoices{*choice.spell_learning,{}, {},{}},SpellChoiceContext::advancement,false);
            const auto access=detail::spell_access(next.grants,next.character_class,next.level,next.prepared_spells);
            choice.spells=sheet.prepared_spells;
            for(const auto& spell:access.spellbook)if(choice.spells.size()<access.prepared_choices&&std::find(choice.spells.begin(),choice.spells.end(),spell.id)==choice.spells.end())choice.spells.push_back(spell.id);
        }
        if(options.level==4){
            choice.feat="ability_score_improvement";const unsigned primary=sheet.character_class=="Fighter"?0:sheet.character_class=="Cleric"?4:sheet.character_class=="Rogue"?1:3;
            unsigned remaining=2;for(unsigned n=0;n<6&&remaining;++n){const auto index=(primary+n)%6;
                choice.abilities[index]=std::min(remaining,unsigned(std::max(0,20-sheet.scores[index])));remaining-=choice.abilities[index];}
        }
        return choice;
    }
    bool advance_character(CharacterSheet& sheet,VitalState& state,const AdvancementChoice& choice) const override
    {
        const auto options=advancement_options(sheet);if(!options.level)return false;
        const auto old=character_definition(character_profile(sheet,{}).data);
        unsigned points=0;for(auto n:choice.abilities){if(n>2)throw std::runtime_error("An ability increase cannot exceed 2");points+=n;}
        if(options.level==4){
            const auto feat=std::find_if(options.feats.begin(),options.feats.end(),[&](const auto& f){return f.id==choice.feat&&f.available;});
            if(feat==options.feats.end())throw std::runtime_error("Choose an available feat or ability points");
            if(points!=(choice.feat=="ability_score_improvement"?2u:0u))throw std::runtime_error("Assign exactly two ability points, or choose a feat");
        }else if(!choice.feat.empty()||points)throw std::runtime_error("Feats and ability points are available at level 4");
        std::set<std::string> selected;
        for(const auto& spell:choice.spells){if(!selected.insert(spell).second||std::none_of(options.spells.begin(),options.spells.end(),[&](const auto& s){return s.id==spell&&s.available;}))throw std::runtime_error("Choose only available, distinct spells");}
        if(!options.spells.empty()&&choice.spells.empty())throw std::runtime_error("Choose at least one supported spell");
        Actor actor;actor.definition=old;actor.winds=old.winds;actor.slots=old.slots;actor.slots2=old.slots2;restore_vitals(actor,state);
        auto next=sheet;++next.level;
        for(const auto& [id,values]:choice.training){
            const auto group=std::find_if(options.training.begin(),options.training.end(),[&](const auto& g){return g.id==id;});
            if(group==options.training.end()||values.size()!=group->count||std::any_of(values.begin(),values.end(),[&](const auto& value){return std::none_of(group->options.begin(),group->options.end(),[&](const auto& o){return o.id==value;});}))throw std::runtime_error("Invalid advancement training choice");
            for(const auto& value:values)next.grants.push_back({"expertise:"+value,id,unsigned(next.level),{}});
        }
        if(next.character_class=="Rogue"&&next.level==3)next.grants.push_back({"feature:steady_aim","class:rogue",3,{}});
        if(next.character_class=="Rogue"&&next.level==2)next.grants.push_back({"feature:cunning_action","class:rogue",2,{}});
        if(next.character_class=="Fighter"&&next.level==2){next.grants.push_back({"feature:action_surge","class:fighter",2,{}});next.grants.push_back({"feature:tactical_mind","class:fighter",2,{}});}
        if(next.character_class=="Fighter"&&next.level==3){next.grants.push_back({"subclass:champion","class:fighter",3,{}});next.grants.push_back({"feature:improved_critical","subclass:fighter:champion",3,{}});next.grants.push_back({"feature:remarkable_athlete","subclass:fighter:champion",3,{}});}
        for(unsigned n=0;n<6;++n){next.scores[n]+=choice.abilities[n];next.bonuses[n]+=choice.abilities[n];
            if(next.scores[n]>20)throw std::runtime_error("Ability scores cannot exceed 20");
            next.modifiers[n]=ability_modifier(next.scores[n]);next.saving_throws[n]=next.modifiers[n]+(next.save_proficiencies[n]?2:0);}
        if(points){
            AbilityAdjustment adjustment{"feat:"+choice.feat,"Level "+std::to_string(next.level)+" Ability Score Improvement",unsigned(next.level)};
            for(unsigned n=0;n<6;++n)adjustment.bonuses[n]=int(choice.abilities[n]);
            adjustment.label_message={"Level {level} Ability Score Improvement",{{"level",std::to_string(next.level)}}};
            next.ability_adjustments.push_back(std::move(adjustment));
        }
        if(!choice.feat.empty())next.grants.push_back(detail::advancement_grant(detail::grant_source_id(sheet.character_class),next.level,choice));next.prepared_spells=choice.spells;
        if(choice.spell_learning){
            next.prepared_spells=sheet.prepared_spells;
            detail::apply_spell_choices(next,SpellChoices{*choice.spell_learning,choice.spells,{},{}},SpellChoiceContext::advancement);
        }else detail::learn_advancement_spells(next,choice.spells);
        next.training=detail::training_profile(next.grants,detail::grant_source_id(next.character_class),detail::grant_source_id(next.background),next.level,next.scores);
        next.hit_point_modifiers.push_back(next.modifiers[2]);
        next.hit_points=maximum_hit_points(next.hit_die,next.race=="Dwarf",next.hit_point_modifiers);
        if(next.race=="Dwarf"){
            next.racial_modifiers.replace(0,next.racial_modifiers.find('\n'),"Dwarven Toughness: +"+std::to_string(next.level)+" maximum HP.");
            for(auto& message:next.racial_messages)if(message.source=="Dwarven Toughness: +{hp} maximum HP.")
                message.arguments={{"hp",std::to_string(next.level)}};
        }
        const int growth=next.hit_points-sheet.hit_points;
        next.hp_explanation="Level "+std::to_string(next.level)+": "+std::to_string(next.hit_points)+" maximum HP; gain "+std::to_string(growth)+". Fixed-average Hit Die growth includes Constitution and any retroactive Constitution increase.";
        next.hp_messages={{"Level {level}: {hp} maximum HP; gain {growth}. Fixed-average Hit Die growth includes Constitution and any retroactive Constitution increase.",
            {{"level",std::to_string(next.level)},{"hp",std::to_string(next.hit_points)},{"growth",std::to_string(growth)}}}};
        if(next.character_class=="Rogue"&&next.level==2){
            next.class_modifiers+="\nCunning Action: Dash or Disengage as a Bonus Action on your turn. Hide remains unavailable.";
            next.class_messages.push_back({"Cunning Action: Dash or Disengage as a Bonus Action on your turn. Hide remains unavailable.",{}});
        }
        if(next.character_class=="Rogue"&&next.level==3){
            next.class_modifiers+="\nSneak Attack: 2d6. Steady Aim: Bonus Action; next attack roll has Advantage, Speed becomes 0.";
            next.class_messages.push_back({"Sneak Attack: 2d6. Steady Aim: Bonus Action; next attack roll has Advantage, Speed becomes 0.",{}});
        }
        next.class_modifiers+="\nLevel "+std::to_string(next.level)+": HP and spell-slot advancement applied. Additional class and subclass features remain unavailable.";
        next.class_messages.push_back({"Level {level}: HP and spell-slot advancement applied. Additional class and subclass features remain unavailable.",{{"level",std::to_string(next.level)}}});
        if(next.character_class=="Fighter"&&next.level==2){
            next.class_modifiers+="\nAction Surge: one additional action, except Magic, on your turn. One use per Short or Long Rest.";
            next.class_messages.push_back({"Action Surge: one additional action, except Magic, on your turn. One use per Short or Long Rest.",{}});
        }
        actor.definition=character_definition(character_profile(next,{}).data);
        if(actor.hp>0)actor.hp+=growth;
        actor.slots+=actor.definition.slots-old.slots;actor.slots2+=actor.definition.slots2-old.slots2;actor.winds+=actor.definition.winds-old.winds;actor.rushes+=actor.definition.rushes-old.rushes;actor.surges+=actor.definition.surges-old.surges;actor.hit_dice+=actor.definition.level-old.level;
        auto continuation=vitals(actor);sheet=std::move(next);state=std::move(continuation);
        return true;
    }
    void validate_character_state(const CharacterSheet& sheet,const VitalState& state) const override
    {
        Actor actor;actor.definition=character_definition(character_profile(sheet,{}).data);
        actor.winds=actor.definition.winds;actor.slots=actor.definition.slots;actor.slots2=actor.definition.slots2;restore_vitals(actor,state);
    }
    void validate_saved_grants(const Identity& saved,const CharacterSheet& sheet,std::span<const FeatureGrant> grants) const override {
        if(module_before(saved,{0,6,35})&&sheet.character_class=="Rogue"&&sheet.level>1)throw std::runtime_error("Legacy campaign cannot contain advanced Rogues");
        if(module_before(saved,{0,6,52})&&sheet.character_class=="Rogue"&&sheet.level>2)throw std::runtime_error("Legacy campaign cannot contain level-three Rogues");
        if(!accepts_campaign_identity(saved))throw std::runtime_error("Unsupported grant migration");
        if(module_before(saved,{0,6,40})&&std::any_of(grants.begin(),grants.end(),[](const auto& g){return detail::is_spell_grant(g)&&g.source_id=="class:sorcerer:spellcasting";}))throw std::runtime_error("Legacy campaign cannot grant Sorcerer cantrips");
        if(module_before(saved,{0,6,39})&&std::any_of(grants.begin(),grants.end(),[](const auto& g){return g.id=="spell:poison_spray"&&g.source_id=="class:warlock:pact_magic";}))throw std::runtime_error("Legacy campaign cannot grant Warlock Poison Spray");
        if(module_before(saved,{0,6,43})&&std::any_of(grants.begin(),grants.end(),[](const auto& g){return g.id=="spell:chill_touch";}))throw std::runtime_error("Legacy campaign cannot grant Chill Touch");
        if(module_before(saved,{0,6,38})&&std::any_of(grants.begin(),grants.end(),[](const auto& g){return g.id=="spell:shocking_grasp";}))throw std::runtime_error("Legacy campaign cannot grant Shocking Grasp");
        if(module_before(saved,{0,6,37})&&std::any_of(grants.begin(),grants.end(),[](const auto& g){return g.id=="spell:eldritch_blast";}))throw std::runtime_error("Legacy campaign cannot grant Eldritch Blast");
        if(module_before(saved,{0,6,25})&&std::any_of(grants.begin(),grants.end(),[](const auto& g){return g.id=="spell:ray_of_frost";}))throw std::runtime_error("Legacy campaign cannot grant Ray of Frost");
        if(module_before(saved,{0,6,29})&&std::any_of(grants.begin(),grants.end(),[](const auto& g){return g.source_id=="class:fighter:fighting_style";}))throw std::runtime_error("Legacy campaign cannot contain starting Fighting Style grants");
        if(module_before(saved,{0,6,34})&&std::any_of(grants.begin(),grants.end(),[](const auto& g){return g.source_id=="background:soldier:gaming_set";}))throw std::runtime_error("Legacy campaign cannot contain Soldier Gaming Set choices");
        if(module_before(saved,{0,6,33})&&detail::has_grant(grants,"tool:herbalism_kit"))throw std::runtime_error("Legacy campaign cannot contain Herbalism Kit proficiency");
        if(module_before(saved,{0,6,32})&&std::any_of(grants.begin(),grants.end(),[](const auto& g){return g.source_id=="class:monk:tools";}))throw std::runtime_error("Legacy campaign cannot contain Monk tool choices");
        if(module_before(saved,{0,6,31})&&std::any_of(grants.begin(),grants.end(),[](const auto& g){return g.source_id=="class:bard:instruments";}))throw std::runtime_error("Legacy campaign cannot contain Bard instrument choices");
        if(module_before(saved,{0,6,30})&&std::any_of(grants.begin(),grants.end(),[](const auto& g){return g.id.starts_with("skill:")&&g.source_id.starts_with("class:")&&g.source_id!="class:rogue";}))throw std::runtime_error("Legacy campaign cannot contain new class skill choices");
        if(module_before(saved,{0,6,49})&&detail::has_grant(grants,"feature:arcane_recovery"))throw std::runtime_error("Legacy campaign cannot grant Arcane Recovery");
        if(module_before(saved,{0,6,51})&&std::any_of(grants.begin(),grants.end(),[](const auto& g){return detail::is_spell_grant(g)&&g.choices.contains("learned_at");}))throw std::runtime_error("Legacy campaign cannot contain cantrip replacements");
        if(module_before(saved,{0,6,50})&&std::any_of(grants.begin(),grants.end(),[](const auto& g){return g.source_id=="class:wizard:scholar";}))throw std::runtime_error("Legacy campaign cannot grant Scholar Expertise");
        if(module_before(saved,{0,6,52})&&(detail::has_grant(grants,"feature:sneak_attack")||detail::has_grant(grants,"feature:steady_aim")))throw std::runtime_error("Legacy campaign cannot contain Rogue attack grants");
        auto expected=saved.version=="0.6.8"?detail::without_training(sheet.grants):sheet.grants;
        if(module_before(saved,{0,6,52}))std::erase_if(expected,[](const auto& g){return g.id=="feature:sneak_attack";});
        if(module_before(saved,{0,6,49}))std::erase_if(expected,[](const auto& g){return g.id=="feature:arcane_recovery";});
        if(module_before(saved,{0,6,33}))std::erase_if(expected,[](const auto& g){return g.id=="tool:herbalism_kit"&&g.source_id=="class:druid";});
        if(module_before(saved,{0,6,28})&&detail::has_grant(grants,"feat:archery"))throw std::runtime_error("Legacy campaign cannot contain Archery grants");
        if(module_before(saved,{0,6,27}))std::erase_if(expected,[](const auto& g){return
            (g.source_id=="background:acolyte"&&(g.id=="skill:insight"||g.id=="skill:religion"||g.id=="tool:calligraphers_supplies"))||
            (g.source_id=="background:soldier"&&(g.id=="skill:athletics"||g.id=="skill:intimidation"));});
        if(module_before(saved,{0,6,26}))std::erase_if(expected,[](const auto& g){return g.source_id=="background:sage"&&(g.id=="skill:arcana"||g.id=="skill:history"||g.id=="tool:calligraphers_supplies");});
        if(module_before(saved,{0,6,13}))std::erase_if(expected,[](const auto& g){return g.id=="trait:dwarven_resilience";});
        if(module_before(saved,{0,6,15}))std::erase_if(expected,[](const auto& g){return g.id=="trait:adrenaline_rush";});
        if(module_before(saved,{0,6,46}))std::erase_if(expected,[](const auto& g){return g.id=="subclass:champion"||g.source_id=="subclass:fighter:champion";});
        if(module_before(saved,{0,6,45}))std::erase_if(expected,[](const auto& g){return g.id=="feature:tactical_mind";});
        if(module_before(saved,{0,6,24}))std::erase_if(expected,[](const auto& g){return g.id=="feature:action_surge";});
        if(module_before(saved,{0,6,19}))expected=detail::without_spell_grants(expected);
        if(!std::equal(grants.begin(),grants.end(),expected.begin(),expected.end()))
            throw std::runtime_error("Saved grants disagree with creation or advancement choices");
    }
    void migrate_character_state(const Identity& saved,const CharacterSheet& sheet,VitalState& state) const override
    {
        if(module_before(saved,{0,6,35})&&sheet.character_class=="Rogue"&&sheet.level>1)throw std::runtime_error("Legacy campaign cannot contain advanced Rogues");
        if(module_before(saved,{0,6,52})&&sheet.character_class=="Rogue"&&sheet.level>2)throw std::runtime_error("Legacy campaign cannot contain level-three Rogues");
        if(!accepts_campaign_identity(saved))throw std::runtime_error("Unsupported campaign migration");
        auto definition=character_definition(character_profile(sheet,{}).data);
        if(module_before(saved,{0,6,43})&&state.resources.find("FX5 ")!=std::string::npos)throw std::runtime_error("Legacy campaign cannot contain Chill Touch");
        if(module_before(saved,{0,6,38})&&state.resources.find("FX3 ")!=std::string::npos)throw std::runtime_error("Legacy campaign cannot contain Shocking Grasp");
        if(module_before(saved,{0,6,25})&&state.resources.find("FX2 ")!=std::string::npos)throw std::runtime_error("Legacy campaign cannot contain Ray of Frost");
        if(module_before(saved,{0,6,49})&&state.resources.starts_with("SRD9 "))throw std::runtime_error("Legacy campaign cannot contain Arcane Recovery expenditure");
        if(module_before(saved,{0,6,24})&&state.resources.starts_with("SRD8 "))throw std::runtime_error("Legacy campaign cannot contain Action Surge expenditure");
        const bool old_hp=saved.version=="0.3.0"||saved.version=="0.4.0"||saved.version=="0.5.0"||
            saved.version=="0.6.0"||saved.version=="0.6.1"||saved.version=="0.6.2";
        if(old_hp){
            // Prior modules rebuilt every gain with the final modifier. Validate
            // against that old maximum before restoring the missing HP to a
            // conscious member; zero HP/death and all resources remain intact.
            const int con=ability_modifier(sheet.scores[2]);
            definition.hp=sheet.hit_die+con+(sheet.race=="Dwarf"?sheet.level:0)+
                (sheet.level-1)*std::max(1,sheet.hit_die/2+1+con);
        }
        Actor actor;actor.definition=definition;actor.winds=definition.winds;
        actor.slots=definition.slots;actor.slots2=definition.slots2;restore_vitals(actor,state);
        if(module_before(saved,{0,6,44})&&actor.recovery.stable_recovery_due)throw std::runtime_error("Legacy campaign cannot contain earned recovery");
        if(module_before(saved,{0,6,41})&&actor.effects.prone)throw std::runtime_error("Legacy campaign cannot contain natural sleep/posture state");
        // Only pre-Adrenaline saves introduce a new resource pool. Later version
        // bumps preserve already-supported Orc state just like other species.
        const bool introduce_rush=module_before(saved,{0,6,15});
        auto next=definition.rushes&&introduce_rush?vitals(actor):state;if(next.hit_points>0)next.hit_points+=sheet.hit_points-definition.hp;
        validate_character_state(sheet,next);state=std::move(next);
    }
    RestPolicy long_rest_policy() const override {return rest::policy(RestKind::long_rest);}
    RestPolicy short_rest_policy() const override {return rest::policy(RestKind::short_rest);}
    RestProgress begin_rest(RestKind kind) const override {return rest::begin(kind);}
    RestTransition advance_rest(const RestProgress& p,std::uint64_t ms,RestWork work) const override {return rest::advance(p,ms,work);}
    RestTransition interrupt_rest(const RestProgress& p,RestInterruption cause) const override {return rest::interrupt(p,cause);}
    RestProgress resume_rest(const RestProgress& p) const override {return rest::resume(p);}
    std::uint64_t remaining_rest(const RestProgress& p) const override {return rest::remaining(p);}
    void validate_rest(const RestProgress& p) const override {rest::validate(p);}
    void elapse(std::span<Participant> participants,std::uint64_t milliseconds,std::uint64_t& random_state) const override
    {
        // Work on owned candidates so malformed state cannot partly advance a
        // party or consume its RNG. No Godot or campaign data enters the rules.
        std::vector<Actor> actors;actors.reserve(participants.size());
        for(const auto& p:participants){
            Actor a;a.source=p;
            a.definition=p.character_profile.empty()?content_->definitions.at(p.definition):character_definition(p.character_profile);
            a.hp=a.definition.hp;a.winds=a.definition.winds;a.slots=a.definition.slots;a.slots2=a.definition.slots2;a.hit_dice=a.definition.hit_die?a.definition.level:0;
            if(p.state)restore_vitals(a,*p.state);
            actors.push_back(std::move(a));
        }
        std::vector<detail::RecoverySubject> subjects;
        for(auto& a:actors)subjects.push_back({{a.source.id,a.effects,a.definition.saves,a.dead,a.definition.str_dex_disadvantage,false},a});
        auto rng=random_state;detail::elapse_recovery(subjects,milliseconds,rng);
        if(!milliseconds)return;
        std::vector<VitalState> next;next.reserve(actors.size());
        for(const auto& a:actors)next.push_back(vitals(a));
        for(std::size_t i=0;i<participants.size();++i)
            if(participants[i].state&&((participants[i].state->hit_points==0&&!participants[i].state->dead)||participants[i].state->resources.starts_with("SRD3 ")||participants[i].state->resources.starts_with("SRD4 ")||participants[i].state->resources.starts_with("SRD5 ")||participants[i].state->resources.starts_with("SRD6 ")||participants[i].state->resources.starts_with("SRD7 ")||participants[i].state->resources.starts_with("SRD8 ")||participants[i].state->resources.starts_with("SRD9 ")))participants[i].state=std::move(next[i]);
        random_state=rng;
    }
    void recover(VitalState& state,const CharacterSheet& sheet) const override
    {
        const auto d=character_definition(character_profile(sheet,{}).data);
        Actor actor;actor.definition=d;actor.winds=d.winds;actor.slots=d.slots;actor.slots2=d.slots2;restore_vitals(actor,state);
        if(actor.dead||actor.hp<1)throw std::runtime_error("Long rest requires at least one HP at its start");
        actor.effects.sleeping=false;
        (void)detail::heal_life(actor,d.hp,d.hp,!detail::healing_blocked(actor.effects));actor.winds=d.winds;actor.slots=d.slots;actor.slots2=d.slots2;actor.hit_dice=d.hit_die?d.level:0;actor.successes=actor.failures=0;actor.stable=false;actor.recovery={};actor.temporary_hp={};actor.rushes=d.rushes;actor.surges=d.surges;actor.arcane=d.arcane;
        state=vitals(actor);
    }
    RecoveryInfo recovery_info(const CharacterSheet& sheet,const VitalState& state) const override
    {
        const auto d=character_definition(character_profile(sheet,{}).data);
        Actor actor;actor.definition=d;actor.winds=d.winds;actor.slots=d.slots;actor.slots2=d.slots2;restore_vitals(actor,state);
        RecoveryInfo result{unsigned(d.hit_die),unsigned(actor.hit_dice),unsigned(d.level),!actor.dead&&actor.hp>0,{},actor.temporary_hp};
        if(d.surges)result.resources.push_back({"action_surge",{"Action Surge",{}},unsigned(actor.surges),unsigned(d.surges),unsigned(d.surges)});
        if(d.rushes)result.resources.push_back({"adrenaline_rush",{"Adrenaline Rush",{}},unsigned(actor.rushes),unsigned(d.rushes),unsigned(d.rushes)});
        if(d.winds)result.resources.push_back({"second_wind",{"Second Wind",{}},unsigned(actor.winds),unsigned(d.winds),1});
        if(d.slots)result.resources.push_back({"spell_slot:1",{"Level-one spell slots",{}},unsigned(actor.slots),unsigned(d.slots),0});
        if(d.slots2)result.resources.push_back({"spell_slot:2",{"Level-two spell slots",{}},unsigned(actor.slots2),unsigned(d.slots2),0});
        if(d.arcane)result.resources.push_back({"arcane_recovery",{"Arcane Recovery",{}},unsigned(actor.arcane),unsigned(d.arcane),0});
        for(const auto& choice:arcane_allocations)if(can_recover(actor,choice))
            result.choices.push_back({std::string(choice.id),{std::string(choice.label),{}}});
        return result;
    }
    void grant_temporary_hit_points(VitalState& state,const CharacterSheet& sheet,const TemporaryHitPoints& offered,TemporaryHpChoice choice) const override
    {
        const auto d=character_definition(character_profile(sheet,{}).data);
        Actor actor;actor.definition=d;actor.winds=d.winds;actor.slots=d.slots;actor.slots2=d.slots2;restore_vitals(actor,state);
        detail::grant_temporary_hp(actor,offered,choice);auto next=vitals(actor);state=std::move(next);
    }
    void recover_short_rest(VitalState& state,const CharacterSheet& sheet) const override
    {
        const auto d=character_definition(character_profile(sheet,{}).data);
        Actor actor;actor.definition=d;actor.winds=d.winds;actor.slots=d.slots;actor.slots2=d.slots2;restore_vitals(actor,state);
        if(actor.dead||actor.hp<1)throw std::runtime_error("Short rest requires at least one HP at its start");
        actor.winds=std::min(d.winds,actor.winds+1);actor.rushes=d.rushes;actor.surges=d.surges;state=vitals(actor);
    }
    Message recover_rest_choice(VitalState& state,const CharacterSheet& sheet,std::string_view choice_id) const override
    {
        const auto d=character_definition(character_profile(sheet,{}).data);
        Actor actor;actor.definition=d;actor.winds=d.winds;actor.slots=d.slots;actor.slots2=d.slots2;restore_vitals(actor,state);
        const auto choice=std::find_if(arcane_allocations.begin(),arcane_allocations.end(),
            [&](const auto& value){return value.id==choice_id;});
        if(choice==arcane_allocations.end()||!can_recover(actor,*choice))
            throw std::runtime_error("This rest recovery choice is unavailable");
        actor.slots+=int(choice->first);actor.slots2+=int(choice->second);--actor.arcane;
        auto next=vitals(actor);
        Message result{"Arcane Recovery restored {first} level-one and {second} level-two spell slots.",
            {{"first",std::to_string(choice->first)},{"second",std::to_string(choice->second)}}};
        state=std::move(next);return result;
    }
    HitDieResult spend_hit_die(VitalState& state,const CharacterSheet& sheet,std::uint64_t& random_state) const override
    {
        const auto d=character_definition(character_profile(sheet,{}).data);
        Actor actor;actor.definition=d;actor.winds=d.winds;actor.slots=d.slots;actor.slots2=d.slots2;restore_vitals(actor,state);
        if(actor.dead||actor.hp<1||actor.hit_dice<1)throw std::runtime_error("No Hit Die can be spent by this character");
        auto rng=random_state;const int rolled=roll_die(rng,d.hit_die);
        const int healing=detail::heal_life(actor,std::max(1,rolled+d.constitution),d.hp,!detail::healing_blocked(actor.effects));
        --actor.hit_dice;
        HitDieResult result{unsigned(d.hit_die),rolled,d.constitution,healing,unsigned(actor.hit_dice)};
        auto next=vitals(actor);state=std::move(next);random_state=rng;return result;
    }
    bool recover_at_safety(VitalState& state,const CharacterSheet& sheet,std::span<const std::string> equipment) const override
    {
        Actor actor;actor.definition=character_definition(character_profile(sheet,equipment).data);
        actor.winds=actor.definition.winds;actor.slots=actor.definition.slots;actor.slots2=actor.definition.slots2;restore_vitals(actor,state);
        if(!conscious(actor))return false;
        if(actor.effects.prone&&actor.definition.speed>detail::speed_penalty(actor.effects)){
            actor.effects.prone=false;state=vitals(actor);
        }
        return true;
    }
    void set_rest_work(VitalState& state,const CharacterSheet& sheet,RestWork work) const override
    {
        if(work!=RestWork::sleep&&work!=RestWork::light_activity&&work!=RestWork::exertion)throw std::runtime_error("Invalid rest work");
        Actor actor;actor.definition=character_definition(character_profile(sheet,{}).data);
        actor.winds=actor.definition.winds;actor.slots=actor.definition.slots;actor.slots2=actor.definition.slots2;restore_vitals(actor,state);
        if(work==RestWork::sleep){if(actor.dead||actor.hp==0)throw std::runtime_error("Natural sleep requires a living conscious character");actor.effects.sleeping=actor.effects.prone=true;}
        else actor.effects.sleeping=false;
        state=vitals(actor);
    }
    std::vector<unsigned> released_equipment(const CharacterSheet& sheet,const VitalState& state,std::span<const std::string> equipment) const override
    {
        Actor actor;actor.definition=character_definition(character_profile(sheet,equipment).data);
        actor.winds=actor.definition.winds;actor.slots=actor.definition.slots;actor.slots2=actor.definition.slots2;restore_vitals(actor,state);
        std::vector<unsigned> result;
        if(unconscious(actor))for(unsigned i=0;i<equipment.size();++i)
            if(equipment[i]=="shield"||detail::weapon(equipment[i]))result.push_back(i);
        return result;
    }
    void set_hit_points(VitalState& state,const CharacterSheet& sheet,int hp) const override
    {
        if(hp<0||hp>sheet.hit_points||(state.dead&&hp))throw std::runtime_error("Unsupported script HP change");
        Actor actor;actor.definition=character_definition(character_profile(sheet,{}).data);
        actor.winds=actor.definition.winds;actor.slots=actor.definition.slots;actor.slots2=actor.definition.slots2;restore_vitals(actor,state);
        if(hp==state.hit_points||(hp>state.hit_points&&detail::healing_blocked(actor.effects)))return;
        if(hp<state.hit_points)actor.effects.sleeping=false;
        detail::set_life_hit_points(actor,hp,actor.definition.hp);
        if(hp==0||state.hit_points==0)actor.effects.prone=true;
        state=vitals(actor);
    }
    void temple_heal(VitalState& state,const CharacterSheet& sheet,std::uint64_t& random_state) const override
    {
        const auto d=character_definition(character_profile(sheet,{}).data);
        Actor actor;actor.definition=d;actor.winds=d.winds;actor.slots=d.slots;actor.slots2=d.slots2;restore_vitals(actor,state);
        if(actor.dead||actor.hp>=d.hp)throw std::runtime_error("Cure Wounds requires a wounded living member");
        // Authored temple caster: Cure Wounds, Wisdom +3. Same SplitMix64 as combat.
        auto rng=random_state;int amount=3;
        for(int i=0;i<2;++i)amount+=roll_die(rng,8);
        if(actor.hp==0)actor.effects.prone=true;
        (void)detail::heal_life(actor,amount,d.hp,!detail::healing_blocked(actor.effects));
        auto next=vitals(actor);state=std::move(next);random_state=rng;
    }
    SpellChoiceOptions spell_choice_options(const CharacterSheet& sheet,SpellChoiceContext context) const override {return detail::spell_choice_options(sheet,context);}
    void apply_spell_choices(CharacterSheet& sheet,const SpellChoices& choice,SpellChoiceContext context,bool complete) const override {detail::apply_spell_choices(sheet,choice,context,complete);}
    SpellAccess spell_access(const CharacterSheet& sheet) const override {
        return detail::spell_access(sheet.grants,sheet.character_class,sheet.level,sheet.prepared_spells);
    }
    EquipmentInfo equipment_info(std::string_view key) const override {
        if(detail::ammunition(key))return {EquipmentSlot::carried,0};
        if(const auto* item=detail::weapon(key))return {EquipmentSlot::weapon,item->hands};
        if(key=="shield")return {EquipmentSlot::shield,1};
        if(detail::armor(key))return {EquipmentSlot::armor,0};
        return {};
    }
    AbilityCheckModifier ability_check(const CharacterSheet& sheet,std::span<const std::string> gear,unsigned ability,
        std::string_view skill,std::string_view tool,EquipmentState equipment) const override {
        const auto d=character_definition(character_profile(sheet,gear,equipment).data);
        auto result=character_rules()->ability_check(sheet,ability,skill,tool);
        result.disadvantage=(ability<2&&d.str_dex_disadvantage)||(ability==1&&skill=="stealth"&&d.stealth_disadvantage);
        return result;
    }
    EquipmentState migrate_equipment(std::span<const std::string> gear) const override {
        for(const auto& key:gear)if(legacy_two_hands(key))return {2};
        return {};
    }
    CharacterProfile character_profile(const CharacterSheet& sheet,std::span<const std::string> gear,EquipmentState equipment={}) const override {
        if(sheet.identity!=character_rules()->identity()||sheet.level<1||sheet.level>4)throw std::runtime_error("Unsupported character rules identity or level");
        (void)detail::training_profile(sheet.grants,detail::grant_source_id(sheet.character_class),detail::grant_source_id(sheet.background),sheet.level,sheet.scores);
        const auto features_only=detail::without_spell_grants(detail::without_training(sheet.grants));
        const auto access=spell_access(sheet);
        const auto effects=detail::validate_grants(features_only,detail::grant_source_id(sheet.character_class),
            detail::grant_source_id(sheet.race),detail::grant_source_id(sheet.background),sheet.level);
        const unsigned features=effects.feats;
        if(sheet.hit_point_modifiers.size()!=sheet.level)throw std::runtime_error("HP history does not match character advancement");
        const bool asi=detail::has_grant(sheet.grants,"feat:ability_score_improvement");
        if(sheet.ability_adjustments.size()!=(asi?2u:1u))throw std::runtime_error("Ability sources disagree with acquired grants");
        if(asi){const auto& adjustment=sheet.ability_adjustments.back();
            if(adjustment.source_id!="feat:ability_score_improvement"||adjustment.level!=4||adjustment.bonuses!=effects.abilities)
                throw std::runtime_error("Ability sources disagree with acquired choices");}
        for(unsigned i=0;i<6;++i){
            const auto bonuses=sheet.ability_adjustments.front().bonuses[i]+effects.abilities[i];
            if(sheet.bonuses[i]!=bonuses||sheet.scores[i]!=sheet.base[i]+bonuses)
                throw std::runtime_error("Ability totals disagree with acquired choices");
        }
        auto spells=sheet.character_class=="Wizard"?detail::wizard_casting_ids(access):detail::known_cantrip_ids(access);
        std::set<std::string> selected;
        const auto record=[&](std::string id){if(!detail::knows_spell(spells,id))spells.push_back(std::move(id));};
        if(sheet.prepared_spells.empty()&&sheet.character_class=="Cleric")record("cure_wounds");
        // Preparable spells come from the eligibility table, at the newest
        // capability because a fresh profile is being written. Cantrips are
        // filtered out: they are known, never prepared. This replaces a chain of
        // per-spell class and level comparisons.
        const auto preparable=detail::spells_of_level(allowed_spells(sheet.character_class,sheet.level,max_profile_tag),false);
        for(const auto& spell:sheet.prepared_spells){
            if(!selected.insert(spell).second)throw std::runtime_error("Duplicate prepared spell");
            if(!detail::knows_spell(preparable,spell))throw std::runtime_error("Unsupported prepared spell");
            record(spell);
        }
        // The legacy bitmask is still written whenever it can express the set,
        // so existing profiles stay byte identical. A spell with no bit forces
        // the explicit list instead.
        const unsigned packed=detail::mask_from_spells(spells);
        const bool list_spells=!spells.empty()&&!packed;
        // One row per capability that raises the written tag. PC28 is the floor
        // because every capability below it is unconditionally written.
        const unsigned tag=highest_present(static_cast<unsigned>(Cap::sorcerer),{
            {static_cast<unsigned>(Cap::rogue),sheet.character_class=="Rogue"},
            {static_cast<unsigned>(Cap::choices),sheet.character_class=="Wizard"&&
                (std::none_of(sheet.grants.begin(),sheet.grants.end(),[](const auto& g){return g.id=="spell:magic_missile"&&g.level==1;})||
                 std::any_of(sheet.grants.begin(),sheet.grants.end(),[](const auto& g){return detail::is_spell_grant(g)&&g.choices.contains("learned_at");}))},
            {static_cast<unsigned>(Cap::scholar),std::any_of(sheet.grants.begin(),sheet.grants.end(),[](const auto& g){return g.source_id=="class:wizard:scholar";})},
            {static_cast<unsigned>(Cap::arcane),detail::has_grant(sheet.grants,"feature:arcane_recovery")},
            {static_cast<unsigned>(Cap::champion),detail::has_grant(sheet.grants,"subclass:champion")},
            {static_cast<unsigned>(Cap::mind),detail::has_grant(sheet.grants,"feature:tactical_mind")},
            {static_cast<unsigned>(Cap::chill),detail::knows_spell(spells,"chill_touch")},
            {static_cast<unsigned>(Cap::explicit_spells),list_spells}});
        std::ostringstream out;out<<"PC"<<tag<<' '<<sheet.level<<' '<<features<<' ';
        if(list_spells){out<<spells.size();for(const auto& id:spells)out<<' '<<id;}
        else out<<packed;
        out<<' '<<std::quoted(sheet.character_class)<<' '<<std::quoted(sheet.race);
        for(auto score:sheet.scores)out<<' '<<score;
        for(auto modifier:sheet.hit_point_modifiers)out<<' '<<modifier;
        out<<' '<<gear.size();for(const auto& item:gear)out<<' '<<std::quoted(item);
        out<<' '<<equipment.weapon_hands<<' '<<std::quoted(detail::grant_source_id(sheet.background));
        detail::write_grants(out,sheet.grants);
        const auto data=out.str();const auto d=character_definition(data);
        if(d.hp!=sheet.hit_points)throw std::runtime_error("Character HP does not match rules profile");
        CharacterProfile result{data,d.hp,d.ac,"Level 1-4 subset: HP, selected feats, supported prepared spells and level-one/two slots. Additional class/subclass and species features remain unavailable.",d.speed,d.melee_bonus};
        result.strength_dexterity_disadvantage=d.str_dex_disadvantage;
        result.equipment={d.weapon_hands};result.grips=grip_options(d);
        if(d.versatile_sides){
            const auto sides=d.weapon_hands==2?d.versatile_sides:d.melee.sides;
            result.item_messages.push_back({"Weapon grip: {hands} hands; melee damage 1d{sides} + ability modifier.",{{"hands",std::to_string(d.weapon_hands)},{"sides",std::to_string(sides)}}});
            result.item_modifiers+="Weapon grip: "+std::to_string(d.weapon_hands)+" hands; melee damage 1d"+std::to_string(sides)+" + ability modifier.\n";
        }
        for(const auto& key:gear){
            if(key=="shield")result.item_modifiers+=trained(sheet.character_class,key)?"Source: equipped Shield: +2 AC.\n":"Source: equipped Shield: +0 AC (untrained).\n";
            else if(key=="leather")result.item_modifiers+="Source: equipped Leather armor and Dexterity score "+std::to_string(sheet.scores[1])+". AC becomes 11 + Dexterity modifier ("+std::to_string(sheet.modifiers[1])+").\n";
            else if(key=="chain_mail")result.item_modifiers+="Source: equipped Chain mail. AC becomes 16; speed -10 feet below Strength 13 (current Strength "+std::to_string(sheet.scores[0])+").\n";
            else if(const auto* item=detail::armor(key))
                result.item_modifiers+="Source: equipped "+std::string(item->label)+" ("+std::string(detail::armor_category_label(item->category))+"). Base AC "+std::to_string(item->base_ac)+"; applied Dexterity modifier "+std::to_string(item->dexterity_contribution(sheet.modifiers[1]))+"; armor AC "+std::to_string(item->base_ac+item->dexterity_contribution(sheet.modifiers[1]))+".\n";
            else if(key=="wand")result.item_modifiers+="Source: equipped Wand. Held focus; melee uses unarmed strike.\n";
            else if(const auto* item=detail::weapon(key);item&&item->fixed_damage)
                result.item_modifiers+="Source: equipped "+weapon_label(key)+". Attack uses "+attack_ability(key)+" modifier"+(trained(sheet.character_class,key)?" +2 class proficiency":" without proficiency")+"; damage is fixed at "+std::to_string(item->fixed_damage)+" without an ability modifier.\n";
            else result.item_modifiers+="Source: equipped "+weapon_label(key)+" and "+sheet.character_class+" weapon proficiency. Weapon attack uses "+attack_ability(key)+" modifier"+(trained(sheet.character_class,key)?" +2 class proficiency":" without proficiency")+"; damage adds that ability modifier.\n";
        }
        for(const auto& key:gear)result.item_modifiers+="Source: equipped "+weapon_label(key)+". "+equipment_note(sheet,key)+"\n";
        if(features&1)result.item_modifiers+="Defense feat: +1 AC while wearing armor.\n";
        if(features&2)result.item_modifiers+="Savage Attacker: once per turn on a weapon hit, choose whether to roll damage twice and keep either result.\n";
        if(gear.empty())result.item_modifiers="No equipment modifiers. Source: unarmed strike rules and Strength score "+std::to_string(sheet.scores[0])+". Attack uses Strength modifier +2 level-one proficiency; damage is 1 + Strength modifier (minimum 0).";
        result.spell_modifiers="Active conditions are shown in the character status.";
        if(sheet.character_class=="Wizard")result.spell_modifiers="Source: Fire Bolt and Wizard spellcasting, Intelligence score "+std::to_string(sheet.scores[3])+". Attack: Intelligence modifier +2 level-one proficiency = "+std::to_string(d.casting)+". Magic Missile has no ability modifier to damage.\n"+result.spell_modifiers;
        if(sheet.character_class=="Cleric")result.spell_modifiers="Source: Cure Wounds and Cleric spellcasting, Wisdom score "+std::to_string(sheet.scores[4])+". Healing: 2d8 + Wisdom modifier ("+std::to_string(d.casting-2)+").\n"+result.spell_modifiers;
        if(d.str_dex_disadvantage)result.spell_modifiers="Cannot cast spells while wearing untrained armor.\n"+result.spell_modifiers;
        // Language-independent presentation of the same computed rule results.
        for(const auto& key:gear){
            if(key=="shield")result.item_messages.push_back({trained(sheet.character_class,key)?"Source: equipped Shield: +2 AC.":"Source: equipped Shield: +0 AC (untrained).",{}});
            else if(key=="leather")result.item_messages.push_back({"Source: equipped Leather armor and Dexterity score {score}. AC becomes 11 + Dexterity modifier ({modifier}).",{{"score",std::to_string(sheet.scores[1])},{"modifier",std::to_string(sheet.modifiers[1])}}});
            else if(key=="chain_mail")result.item_messages.push_back({"Source: equipped Chain mail. AC becomes 16; speed -10 feet below Strength 13 (current Strength {score}).",{{"score",std::to_string(sheet.scores[0])}}});
            else if(const auto* item=detail::armor(key))
                result.item_messages.push_back({"Source: equipped {item} ({category}). Base AC {base}; applied Dexterity modifier {dexterity}; armor AC {ac}.",
                    {{"item",std::string(item->label),true},{"category",std::string(detail::armor_category_label(item->category)),true},
                     {"base",std::to_string(item->base_ac)},{"dexterity",std::to_string(item->dexterity_contribution(sheet.modifiers[1]))},{"ac",std::to_string(item->base_ac+item->dexterity_contribution(sheet.modifiers[1]))}}});
            else if(key=="wand")result.item_messages.push_back({"Source: equipped Wand. Held focus; melee uses unarmed strike.",{}});
            else if(const auto* item=detail::weapon(key);item&&item->fixed_damage)
                result.item_messages.push_back({"Source: equipped {item}. Attack uses {ability} modifier {proficiency}; damage is fixed at {damage} without an ability modifier.",
                    {{"item",weapon_label(key),true},{"ability",attack_ability(key),true},{"proficiency",trained(sheet.character_class,key)?"+2 class proficiency":"without proficiency",true},{"damage",std::to_string(item->fixed_damage)}}});
            else result.item_messages.push_back({"Source: equipped {item} and {class} weapon proficiency. Weapon attack uses {ability} modifier {proficiency}; damage adds that ability modifier.",
                {{"item",weapon_label(key),true},{"class",sheet.character_class,true},{"ability",attack_ability(key),true},
                 {"proficiency",trained(sheet.character_class,key)?"+2 class proficiency":"without proficiency",true}}});
            result.item_messages.push_back({equipment_note(sheet,key),{}});
            if(const auto* item=detail::armor(key)){
                const auto note=[&](std::string text){result.item_modifiers+=text+"\n";result.item_messages.push_back({std::move(text),{}});};
                if(item->category==detail::ArmorCategory::medium)note("Medium armor limits the Dexterity modifier added to AC to +2; negative modifiers still apply.");
                if(item->category==detail::ArmorCategory::heavy)note("Heavy armor ignores the Dexterity modifier when calculating AC.");
                if(item->stealth_disadvantage)note("This armor imposes Disadvantage on Dexterity (Stealth) checks.");
                if(item->strength&&key!="chain_mail"){
                    const bool penalty=sheet.scores[0]<item->strength;
                    result.item_modifiers+="Source: equipped "+std::string(item->label)+". Requires Strength "+std::to_string(item->strength)+"; current score "+std::to_string(sheet.scores[0])+". "+(penalty?"Speed reduced by 10 feet.":"Requirement met.")+"\n";
                    result.item_messages.push_back({penalty?
                        "Source: equipped {item}. Requires Strength {required}; current score {score}. Speed reduced by 10 feet.":
                        "Source: equipped {item}. Requires Strength {required}; current score {score}. Requirement met.",
                        {{"item",std::string(item->label),true},{"required",std::to_string(item->strength)},{"score",std::to_string(sheet.scores[0])}}});
                }
            }
            if(const auto* item=detail::weapon(key);item&&item->heavy){
                const std::string ability=item->ranged?"Dexterity":"Strength";
                const auto score=std::to_string(sheet.scores[item->ranged?1:0]);
                const bool penalty=item->heavy_disadvantage(sheet.scores);
                result.item_modifiers+="Source: equipped "+weapon_label(key)+" (Heavy). Requires "+ability+" 13; current score "+score+". "+
                    (penalty?"Attacks with this weapon have Disadvantage.":"Requirement met.")+"\n";
                result.item_messages.push_back({penalty?
                    "Source: equipped {item} (Heavy). Requires {ability} 13; current score {score}. Attacks with this weapon have Disadvantage.":
                    "Source: equipped {item} (Heavy). Requires {ability} 13; current score {score}. Requirement met.",
                    {{"item",weapon_label(key),true},{"ability",ability,true},{"score",score}}});
            }
        }
        if(features&1)result.item_messages.push_back({"Defense feat: +1 AC while wearing armor.",{}});
        if(features&2)result.item_messages.push_back({"Savage Attacker: once per turn on a weapon hit, choose whether to roll damage twice and keep either result.",{}});
        if(gear.empty())result.item_messages.push_back({"No equipment modifiers. Source: unarmed strike rules and Strength score {score}. Attack uses Strength modifier +2 level-one proficiency; damage is 1 + Strength modifier (minimum 0).",{{"score",std::to_string(sheet.scores[0])}}});
        if(!d.spells.empty()&&!somatic_hand(d)){
            const std::string note="Equipped weapon or wand and shield occupy both hands. Spells with Somatic components are unavailable.";
            result.spell_modifiers=note+"\n"+result.spell_modifiers;
            result.spell_messages.push_back({"Equipped weapon or wand and shield occupy both hands. Spells with Somatic components are unavailable.",{}});
        }
        if(d.str_dex_disadvantage)result.spell_messages.push_back({"Cannot cast spells while wearing untrained armor.",{}});
        if(sheet.character_class=="Wizard")result.spell_messages.push_back({"Source: Fire Bolt and Wizard spellcasting, Intelligence score {score}. Attack: Intelligence modifier +2 level-one proficiency = {attack}. Magic Missile has no ability modifier to damage.",{{"score",std::to_string(sheet.scores[3])},{"attack",std::to_string(d.casting)}}});
        if(sheet.character_class=="Cleric")result.spell_messages.push_back({"Source: Cure Wounds and Cleric spellcasting, Wisdom score {score}. Healing: 2d8 + Wisdom modifier ({modifier}).",{{"score",std::to_string(sheet.scores[4])},{"modifier",std::to_string(d.casting-2)}}});
        if(access.cantrip_choices){
            for(const auto& spell:access.cantrips){
                result.spell_modifiers+="\nKnown cantrip: "+spell.label+".";
                result.spell_messages.push_back({"Known cantrip: {spell}.",{{"spell",spell.label,true}}});
            }
        }
        if(sheet.character_class=="Sorcerer"){
            result.spell_modifiers+="\nSorcerer cantrips: Charisma score "+std::to_string(sheet.scores[5])+"; spell attack bonus "+std::to_string(d.casting)+"; pending choices "+std::to_string(access.cantrip_choices-access.cantrips.size())+".";
            result.spell_messages.push_back({"Sorcerer cantrips: Charisma score {score}; spell attack bonus {attack}; pending choices {cantrips}.",
                {{"score",std::to_string(sheet.scores[5])},{"attack",std::to_string(d.casting)},{"cantrips",std::to_string(access.cantrip_choices-access.cantrips.size())}}});
        }
        if(sheet.character_class=="Warlock"){
            result.spell_modifiers+="\nPact Magic cantrips: Charisma score "+std::to_string(sheet.scores[5])+"; spell attack bonus "+std::to_string(d.casting)+"; pending choices "+std::to_string(access.cantrip_choices-access.cantrips.size())+".";
            result.spell_messages.push_back({"Pact Magic cantrips: Charisma score {score}; spell attack bonus {attack}; pending choices {cantrips}.",
                {{"score",std::to_string(sheet.scores[5])},{"attack",std::to_string(d.casting)},{"cantrips",std::to_string(access.cantrip_choices-access.cantrips.size())}}});
        }
        if(sheet.character_class=="Cleric"){
            result.spell_modifiers+="\nPending Cleric cantrip choices: "+std::to_string(access.cantrip_choices-access.cantrips.size())+".";
            result.spell_messages.push_back({"Pending Cleric cantrip choices: {cantrips}.",{{"cantrips",std::to_string(access.cantrip_choices-access.cantrips.size())}}});
        }
        if(sheet.character_class=="Wizard"){
            for(const auto& spell:access.spellbook){
                result.spell_modifiers+="\nSpellbook: "+spell.label+" (learned at Wizard level "+std::to_string(spell.acquired_level)+").";
                result.spell_messages.push_back({"Spellbook: {spell} (learned at Wizard level {level}).",{{"spell",spell.label,true},{"level",std::to_string(spell.acquired_level)}}});
            }
            result.spell_modifiers+="\nPending Wizard choices: "+std::to_string(access.cantrip_choices-access.cantrips.size())+" cantrips, "+std::to_string(access.spellbook_choices-access.spellbook.size())+" spellbook spells, "+std::to_string(access.prepared_choices-access.prepared.size())+" prepared spells.";
            result.spell_messages.push_back({"Pending Wizard choices: {cantrips} cantrips, {book} spellbook spells, {prepared} prepared spells.",
                {{"cantrips",std::to_string(access.cantrip_choices-access.cantrips.size())},{"book",std::to_string(access.spellbook_choices-access.spellbook.size())},{"prepared",std::to_string(access.prepared_choices-access.prepared.size())}}});
        }
        result.spell_messages.push_back({"Active conditions are shown in the character status.",{}});
        return result;
    }
private: std::shared_ptr<const Content> content_;
};
}
std::unique_ptr<RulesModule> load(const std::filesystem::path& file)
{
    if(std::filesystem::file_size(file)>65536)throw std::runtime_error("Rules content exceeds size limit");
    std::ifstream input(file,std::ios::binary);if(!input)throw std::runtime_error("Cannot open rules content: "+file.string());
    std::string bytes{std::istreambuf_iterator<char>(input),{}};
    if(bytes.size()>65536||input.bad())throw std::runtime_error("Invalid rules content size/read");
    return parse_content(bytes);
}
std::unique_ptr<RulesModule> parse_content(std::string_view content_bytes)
{
    if(content_bytes.size()>65536)throw std::runtime_error("Rules content exceeds size limit");
    std::string bytes(content_bytes);
    // Git may translate line endings; identical content must keep its identity.
    bytes.erase(std::remove(bytes.begin(),bytes.end(),'\r'),bytes.end());
    std::uint64_t hash=14695981039346656037ULL;for(unsigned char c:bytes){hash^=c;hash*=1099511628211ULL;}
    std::istringstream lines(bytes);std::string line,magic,revision;unsigned version;
    std::getline(lines,line);std::istringstream header(line);header>>magic>>version>>revision;
    if(!header||magic!="OPENGOLD_SRD5"||version!=1)throw std::runtime_error("Unsupported rules content format");
    header>>std::ws;
    if(!header.eof()||revision.empty()||revision.size()>80)throw std::runtime_error("Invalid rules content header");
    Content content;content.identity={"opengold.srd5","0.6.52",revision+"/"+std::to_string(hash)};
    // Preserve campaign saves from the preceding pack and the frozen v1/v2 fixtures.
    if(revision=="srd-5.2.1-demo.1")for(const auto fingerprint:
        {"15286736505479635800","1436083463150607054","4820123901484423331"})
        content.previous_campaign_identities.push_back({"opengold.srd5",content.identity.version,revision+"/"+fingerprint});
    std::string before_damage;
    {std::istringstream previous(bytes);std::string row;
        while(std::getline(previous,row))if(!row.starts_with("damage_types ")&&!row.starts_with("affinity "))before_damage+=row+'\n';}
    std::uint64_t prior_hash=14695981039346656037ULL;for(unsigned char c:before_damage){prior_hash^=c;prior_hash*=1099511628211ULL;}
    if(revision=="srd-5.2.1-demo.1"&&prior_hash==15052881321234871607ULL)
        content.previous_campaign_identities.push_back({"opengold.srd5",content.identity.version,revision+"/"+std::to_string(prior_hash)});
    // These additive rows introduce saves and an isolated casting fixture. Old
    // campaign sheets can migrate; combat checkpoints still require exact rules.
    // Reconstruct both supported historical packs without guessing fingerprints.
    for(bool remove_roaming:{false,true}){
        std::istringstream previous_lines(before_damage);std::string previous,line_before;
        const std::array<std::string_view,7> additions{"slums-kobold","slums-goblin","slums-kobold-leader","slums-kobold-leader-sword","slums-goblin-leader","slums-orc-leader","slums-bugbear"};
        while(std::getline(previous_lines,line_before)){
            std::istringstream row(line_before);std::string tag,key;row>>tag>>key;
            if(tag=="saves"||tag=="spellcasting"||(tag=="creature"&&key=="blindness-adept"))continue;
            if(remove_roaming&&tag=="creature"&&std::find(additions.begin(),additions.end(),key)!=additions.end())continue;
            previous+=line_before+'\n';
        }
        std::uint64_t previous_hash=14695981039346656037ULL;for(unsigned char c:previous){previous_hash^=c;previous_hash*=1099511628211ULL;}
        if(previous_hash!=hash)content.previous_campaign_identities.push_back({"opengold.srd5",content.identity.version,revision+"/"+std::to_string(previous_hash)});
    }
    std::set<std::string> save_rows,casting_rows,damage_rows;
    while(std::getline(lines,line)) {
        if(line.empty()||line[0]=='#'||line=="\r")continue;
        std::istringstream row(line);std::string tag,key;Definition d;row>>tag>>key;
        if(tag=="damage_types"||tag=="affinity"){
            const auto found=content.definitions.find(key);
            if(found==content.definitions.end())throw std::runtime_error("Unknown damage profile: "+key);
            auto& definition=found->second;std::string first,second;row>>first>>second;
            if(tag=="damage_types"){
                if(!damage_rows.insert(key).second)throw std::runtime_error("Duplicate damage type row");
                definition.melee_type=detail::damage_type(first);definition.ranged_type=detail::damage_type(second);
            }else{
                std::string type;row>>type;
                if(first.empty()||first.size()>128||definition.affinities.size()>=128||
                    std::any_of(definition.affinities.begin(),definition.affinities.end(),[&](const auto& a){return a.source_id==first;}))
                    throw std::runtime_error("Invalid damage affinity source");
                detail::DamageAffinity affinity;affinity.source_id=first;
                if(second=="resistance")affinity.kind=detail::AffinityKind::resistance;
                else if(second=="vulnerability")affinity.kind=detail::AffinityKind::vulnerability;
                else if(second=="immunity")affinity.kind=detail::AffinityKind::immunity;
                else throw std::runtime_error("Unknown damage affinity");
                if(type!="all")affinity.type=detail::damage_type(type);
                definition.affinities.push_back(std::move(affinity));
            }
            if(!row)throw std::runtime_error("Truncated damage profile");
            row>>std::ws;if(!row.eof())throw std::runtime_error("Unknown damage profile fields");continue;
        }
        if(tag=="saves"||tag=="spellcasting"){
            const auto found=content.definitions.find(key);
            auto& seen=tag=="saves"?save_rows:casting_rows;
            if(found==content.definitions.end()||!seen.insert(key).second)throw std::runtime_error("Invalid supplemental creature definition: "+key);
            auto& definition=found->second;
            if(tag=="saves"){
                for(auto& bonus:definition.saves)row>>bonus;
                if(std::any_of(definition.saves.begin(),definition.saves.end(),[](int n){return n< -10||n>30;}))throw std::runtime_error("Invalid saving throw bonus");
            }else{
                // Authored creature rows keep the compact bitmask: they are a
                // handful of spells, so the 31-bit limit is no constraint there.
                // It is translated to ids at the boundary; nothing downstream
                // sees a mask.
                int casting_spells{};
                row>>definition.slots2>>casting_spells;
                if(definition.level<3||definition.slots2<0||definition.slots2>20||casting_spells<0||casting_spells>63)throw std::runtime_error("Invalid supplemental spellcasting");
                definition.spells=detail::spells_from_mask(unsigned(casting_spells));
                definition.known_cantrips=detail::spells_of_level(definition.spells,true);
                if(detail::mask_from_spells(definition.spells)!=unsigned(casting_spells))throw std::runtime_error("Invalid supplemental spellcasting");
            }
            if(!row)throw std::runtime_error("Truncated supplemental creature definition");
            row>>std::ws;if(!row.eof())throw std::runtime_error("Unknown supplemental creature fields");
            continue;
        }
        int creature_spells{};
        row>>d.ac>>d.hp>>d.initiative>>d.speed>>d.melee_bonus>>d.melee.count>>d.melee.sides>>d.melee.bonus
            >>d.ranged_bonus>>d.ranged.count>>d.ranged.sides>>d.ranged.bonus>>d.range>>d.long_range>>d.winds>>d.slots>>d.casting>>d.level>>creature_spells;

        const bool ranged_none=d.range==0&&d.long_range==0&&d.ranged_bonus==0&&
            d.ranged.count==0&&d.ranged.sides==0&&d.ranged.bonus==0;
        const bool ranged_weapon=d.range>=5&&d.long_range>=d.range&&d.long_range<=600&&
            d.ranged_bonus>= -10&&d.ranged_bonus<=30&&d.ranged.count>=1&&d.ranged.count<=10&&
            d.ranged.sides>=2&&d.ranged.sides<=20&&d.ranged.bonus>= -10&&d.ranged.bonus<=30;
        if(!row||tag!="creature"||key.size()>80||content.definitions.contains(key)||d.ac<1||d.ac>40||d.hp<1||d.hp>1000||
            d.initiative< -10||d.initiative>20||d.speed<5||d.speed>120||d.speed%5||d.melee.count<1||d.melee.count>10||d.melee.sides<2||d.melee.sides>20||
            (!ranged_none&&!ranged_weapon)||
            d.winds<0||d.winds>10||d.slots<0||d.slots>20||d.level<1||d.level>4||creature_spells<0||creature_spells>7||
            d.melee_bonus< -10||d.melee_bonus>30||d.casting< -10||d.casting>30||
            d.melee.bonus< -10||d.melee.bonus>30)
            throw std::runtime_error("Invalid or unsupported creature definition: "+key);
        d.spells=detail::spells_from_mask(unsigned(creature_spells));
        d.known_cantrips=detail::spells_of_level(d.spells,true);
        if(detail::mask_from_spells(d.spells)!=unsigned(creature_spells))
            throw std::runtime_error("Invalid or unsupported creature definition: "+key);
        row>>std::ws;if(!row.eof())throw std::runtime_error("Unknown creature fields: "+key);
        content.definitions.emplace(std::move(key),d);
    }
    if(content.definitions.empty())throw std::runtime_error("Empty rules content");return std::make_unique<Module>(std::move(content));
}
}
