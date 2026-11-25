#include "combat_session_impl.h"

// The Session's checkpoint (combat_session_impl.h; docs/SAVES.md): save()
// writes the fight and restored() reads one back.

namespace opengold::original::detail
{

std::string Session::save() const
{
    Writer out;
    const bool dark_queen = game_of(identity_) == GoldBoxGame::dark_queen_of_krynn;
    const bool native_calendar =
        dark_queen || game_of(identity_) == GoldBoxGame::death_knights_of_krynn;
    out(std::string(dark_queen ? dark_queen_checkpoint_format : checkpoint_format), board_.width,
        board_.height);
    if (native_calendar)
    {
        out(starting_time_minutes_);
    }
    if (dark_queen)
    {
        out(underwater_);
    }
    for (const auto tile : board_.terrain)
    {
        out(static_cast<unsigned>(tile));
    }
    out(board_.plants.size());
    for (const bool plant : board_.plants)
    {
        out(plant);
    }
    out(board_.blocking.size());
    for (std::size_t n = 0; n < board_.blocking.size(); ++n)
    {
        out(static_cast<unsigned>(board_.clearance[n]), static_cast<unsigned>(board_.blocking[n]));
    }
    out(board_.move_cost.size());
    for (const auto cost : board_.move_cost)
    {
        out(static_cast<unsigned>(cost));
    }
    out(random_.value, round_, revision_, actor_, static_cast<unsigned>(outcome_),
        morale_threshold_, outdoors_, escape_ends_fight_, escape_side_.value_or(4), party_facing_,
        party_move_modifier_, crystals_, crystal_.value_or(3), hostile_hit_points_percent_,
        turning_tries_, turning_penalty_, fight_end_round_, round_limit_reached_,
        romance_.has_value());
    if (romance_)
    {
        out(romance_->member, romance_->npc, romance_->member_love, romance_->npc_love, lover_,
            beloved_);
    }
    out(clouds_.size());
    for (const auto &cloud : clouds_)
    {
        out(cloud.caster, cloud.index, cloud.corner.x, cloud.corner.y,
            static_cast<unsigned>(cloud.kind));
        for (const bool covers : cloud.covers)
        {
            out(covers);
        }
    }
    out(blasts_.size());
    for (const auto &blast : blasts_)
    {
        out(blast.caster, blast.index, blast.cell.x, blast.cell.y, blast.level);
    }
    out(turn_order_.size());
    for (const auto id : turn_order_)
    {
        out(id);
    }
    out(fighters_.size());
    for (const auto &f : fighters_)
    {
        out(f.id, f.name, f.definition, static_cast<unsigned>(f.side), f.cell.x, f.cell.y,
            f.footprint.width, f.footprint.height, f.facing_left, f.numbers.encode(), f.hit_points,
            static_cast<unsigned>(f.condition), f.overkill, f.initiative, f.half_moves,
            f.attacks_left[0], f.attacks_left[1], f.surprised, f.attack_phase, f.sweep_budget,
            f.direction, f.flank_count, f.flank_spread, f.guarding, f.fleeing, f.turned,
            f.tried_turning, f.turn_attempts, f.cast_allowed, f.pending_spell, f.aiming,
            f.picks_left, f.blast_delay, static_cast<unsigned>(f.own_side), f.item_use_allowed,
            f.using_item, f.stance, f.lock, f.arrival_round,
            f.before_animation ? f.before_animation->encode() : std::string(), f.picked.size());
        for (const auto id : f.picked)
        {
            out(id);
        }
        out(f.shot_item, f.item_ids.size());
        for (const auto &[ordinal, id] : f.item_ids)
        {
            out(ordinal, id);
        }
        out(f.record ? encode_fight_record(*f.record) : std::string(), f.weighs_unreadied,
            f.readied_changed, f.held.size());
        for (const auto &held : f.held)
        {
            const auto &item = held.carried.item;
            out(por::original_item_key(item), item.readied_raw, item.stack_size, held.inventory_id,
                held.name, held.start_count, held.gone, held.readied_order, held.conjured);
        }
        out(f.worked_from.size());
        for (const int at : f.worked_from)
        {
            out(at);
        }
    }
    out(log_.size());
    for (const auto &entry : log_)
    {
        out(entry.message.source, entry.message.arguments.size());
        for (const auto &argument : entry.message.arguments)
        {
            out(argument.name, argument.value, argument.translate);
        }
    }
    // The Buck Rogers games' own state follows; no other game's fight has it.
    if (buck_rogers())
    {
        write_ground(out);
    }
    return out.text();
}

namespace
{
// A fighter's record and held items as a checkpoint keeps them, their types
// from the game's.
void read_gear(Reader &in, Fighter &f, const Content &content)
{
    const auto invalid = []
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    };
    if (const auto record = in.text(); !record.empty())
    {
        f.record = decode_fight_record(record);
        if (!f.record)
        {
            invalid();
        }
    }
    f.weighs_unreadied = in.as<bool>();
    f.readied_changed = in.as<bool>();
    const auto count = in.as<std::size_t>();
    if (count > 255)
    {
        invalid();
    }
    for (std::size_t n = 0; n < count; ++n)
    {
        auto item = por::item_of_original_key(in.text());
        if (!item || item->type >= content.item_types.size())
        {
            invalid();
        }
        Held held{{*item, content.item_types[item->type]}};
        auto &raw = held.carried.item;
        raw.readied_raw = in.as<std::uint8_t>();
        raw.stack_size = in.as<std::uint8_t>();
        raw.raw[52] = raw.readied_raw;
        raw.raw[57] = raw.stack_size;
        held.inventory_id = in.as<std::uint64_t>();
        held.name = in.text();
        held.start_count = in.as<int>();
        held.gone = in.as<bool>();
        held.readied_order = in.as<int>();
        held.conjured = in.as<bool>();
        f.held.push_back(std::move(held));
    }
    const auto worked = in.as<std::size_t>();
    for (std::size_t n = 0; n < worked; ++n)
    {
        const auto at = in.as<int>();
        if (at < 0 || static_cast<std::size_t>(at) >= f.held.size())
        {
            invalid();
        }
        f.worked_from.push_back(at);
    }
}
} // namespace

std::unique_ptr<Session> Session::restored(rules::Identity identity, std::string_view checkpoint,
                                           const Content &content)
{
    Reader in(checkpoint);
    const auto format = in.text();
    const bool has_costs = format == checkpoint_format || format == dark_queen_checkpoint_format;
    const bool dark_queen =
        format == dark_queen_checkpoint_format || format == costless_dark_queen_checkpoint_format;
    if (!has_costs && format != costless_checkpoint_format &&
        format != costless_death_knights_checkpoint_format && !dark_queen)
    {
        throw std::runtime_error(std::string(rules::older_save_message));
    }
    const bool native_calendar =
        dark_queen || format == costless_death_knights_checkpoint_format ||
        (format == checkpoint_format && game_of(identity) == GoldBoxGame::death_knights_of_krynn);
    std::unique_ptr<Session> session(new Session);
    session->identity_ = std::move(identity);
    session->item_types_ = content.item_types;
    auto &board = session->board_;
    board.width = in.as<int>();
    board.height = in.as<int>();
    if (native_calendar)
    {
        session->starting_time_minutes_ = in.as<std::uint64_t>();
    }
    if (dark_queen)
    {
        session->underwater_ = in.as<bool>();
    }
    if (board.width < 2 || board.height < 2 || board.width > 64 || board.height > 64)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    board.terrain.resize(static_cast<std::size_t>(board.width * board.height));
    for (auto &tile : board.terrain)
    {
        tile = in.as<rules::Terrain>();
    }
    const auto plants = in.as<std::size_t>();
    if (plants != 0 && plants != board.terrain.size())
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < plants; ++n)
    {
        board.plants.push_back(in.as<bool>());
    }
    const auto heights = in.as<std::size_t>();
    if (heights != 0 && heights != board.terrain.size())
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < heights; ++n)
    {
        board.clearance.push_back(static_cast<std::uint8_t>(in.as<unsigned>()));
        board.blocking.push_back(static_cast<std::uint8_t>(in.as<unsigned>()));
    }
    const auto costs = has_costs ? in.as<std::size_t>() : 0;
    if (costs != 0 && costs != board.terrain.size())
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < costs; ++n)
    {
        const auto cost = in.as<std::uint64_t>();
        if (cost == 0 || cost > std::numeric_limits<std::uint8_t>::max())
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        board.move_cost.push_back(static_cast<std::uint8_t>(cost));
    }
    rules::validate_battlefield(board);
    session->random_.value = in.as<std::uint64_t>();
    session->round_ = in.as<unsigned>();
    session->revision_ = in.as<std::uint64_t>();
    session->actor_ = in.as<EntityId>();
    session->outcome_ = in.as<rules::Outcome>();
    session->morale_threshold_ = in.as<unsigned>();
    session->outdoors_ = in.as<bool>();
    session->escape_ends_fight_ = in.as<bool>();
    if (const auto side = in.as<unsigned>(); side < 4)
    {
        session->escape_side_ = side;
    }
    session->party_facing_ = in.as<int>();
    session->party_move_modifier_ = in.as<int>();
    session->crystals_ = in.as<bool>();
    if (const auto colour = in.as<unsigned>(); colour < 3)
    {
        session->crystal_ = colour;
    }
    session->hostile_hit_points_percent_ = in.as<int>();
    session->turning_tries_ = in.as<int>();
    session->turning_penalty_ = in.as<int>();
    session->fight_end_round_ = in.as<unsigned>();
    session->round_limit_reached_ = in.as<bool>();
    if (in.as<bool>())
    {
        rules::Romance romance;
        romance.member = in.text();
        romance.npc = in.text();
        romance.member_love = in.as<int>();
        romance.npc_love = in.as<int>();
        session->romance_ = std::move(romance);
        session->lover_ = in.as<EntityId>();
        session->beloved_ = in.as<EntityId>();
    }
    const auto clouds = in.as<std::size_t>();
    if (clouds > 64)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < clouds; ++n)
    {
        Cloud cloud;
        cloud.caster = in.as<EntityId>();
        cloud.index = in.as<int>();
        cloud.corner = {in.as<int>(), in.as<int>()};
        const auto kind = in.as<unsigned>();
        if (kind > static_cast<unsigned>(CloudKind::acid))
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        cloud.kind = static_cast<CloudKind>(kind);
        for (auto &covers : cloud.covers)
        {
            covers = in.as<bool>();
        }
        session->clouds_.push_back(cloud);
    }
    const auto blasts = in.as<std::size_t>();
    if (blasts > 64)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < blasts; ++n)
    {
        DelayedBlast blast;
        blast.caster = in.as<EntityId>();
        blast.index = in.as<int>();
        blast.cell = {in.as<int>(), in.as<int>()};
        blast.level = in.as<int>();
        session->blasts_.push_back(blast);
    }
    const auto ordered = in.as<std::size_t>();
    if (ordered > 256)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t n = 0; n < ordered; ++n)
    {
        session->turn_order_.push_back(in.as<EntityId>());
    }
    if (session->morale_threshold_ > 100 || session->hostile_hit_points_percent_ < 0 ||
        session->hostile_hit_points_percent_ > 100)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    const auto count = in.as<std::size_t>();
    if (count > 256)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    for (std::size_t fighter_index = 0; fighter_index < count; ++fighter_index)
    {
        Fighter f;
        f.id = in.as<EntityId>();
        f.name = in.text();
        f.definition = in.text();
        f.side = in.as<Side>();
        f.cell = {in.as<int>(), in.as<int>()};
        f.footprint = {in.as<int>(), in.as<int>()};
        f.facing_left = in.as<bool>();
        const auto numbers = Combatant::decode(in.text());
        if (!numbers)
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        f.numbers = *numbers;
        f.hit_points = in.as<int>();
        f.condition = in.as<Condition>();
        f.overkill = in.as<int>();
        f.initiative = in.as<int>();
        f.half_moves = in.as<int>();
        f.attacks_left = {in.as<int>(), in.as<int>()};
        f.surprised = in.as<bool>();
        f.attack_phase = in.as<int>();
        f.sweep_budget = in.as<int>();
        f.direction = in.as<int>();
        f.flank_count = in.as<int>();
        f.flank_spread = in.as<int>();
        f.guarding = in.as<bool>();
        f.fleeing = in.as<bool>();
        f.turned = in.as<bool>();
        f.tried_turning = in.as<bool>();
        f.turn_attempts = in.as<int>();
        f.cast_allowed = in.as<bool>();
        f.pending_spell = in.as<int>();
        f.aiming = in.as<int>();
        f.picks_left = in.as<int>();
        f.blast_delay = in.as<int>();
        f.own_side = in.as<Side>();
        f.item_use_allowed = in.as<bool>();
        f.using_item = in.as<int>();
        f.stance = in.as<int>();
        f.lock = in.as<EntityId>();
        f.arrival_round = in.as<int>();
        if (const auto before = in.text(); !before.empty())
        {
            f.before_animation = Combatant::decode(before);
            if (!f.before_animation)
            {
                throw std::runtime_error("Invalid original-rules combat checkpoint");
            }
        }
        const auto picked = in.as<std::size_t>();
        if (picked > 8 || (f.pending_spell && !spell(f.pending_spell)) ||
            (f.aiming && !spell(f.aiming)))
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        for (std::size_t n = 0; n < picked; ++n)
        {
            f.picked.push_back(in.as<EntityId>());
        }
        f.shot_item = in.as<std::uint64_t>();
        const auto counted = in.as<std::size_t>();
        if (counted > 255)
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        for (std::size_t n = 0; n < counted; ++n)
        {
            const auto ordinal = in.as<int>();
            f.item_ids.emplace_back(ordinal, in.as<std::uint64_t>());
        }
        read_gear(in, f, content);
        if (f.condition > Condition::stoned || f.attack_phase < 0 || f.attack_phase > 1 ||
            f.direction < 0 || f.direction > 7)
        {
            throw std::runtime_error("Invalid original-rules combat checkpoint");
        }
        session->fighters_.push_back(std::move(f));
    }
    const auto entries = in.as<std::size_t>();
    for (std::size_t n = 0; n < entries; ++n)
    {
        Message message;
        message.source = in.text();
        const auto arguments = in.as<std::size_t>();
        for (std::size_t a = 0; a < arguments; ++a)
        {
            rules::MessageArgument argument;
            argument.name = in.text();
            argument.value = in.text();
            argument.translate = in.as<bool>();
            message.arguments.push_back(std::move(argument));
        }
        session->log_.push_back({english(message), std::move(message)});
    }
    if (session->buck_rogers())
    {
        session->read_ground(in);
    }
    const bool order_known = std::ranges::all_of(session->turn_order_,
                                                 [&session](EntityId id)
                                                 {
                                                     return session->find(id) != nullptr;
                                                 });
    if (!in.finished() || !order_known)
    {
        throw std::runtime_error("Invalid original-rules combat checkpoint");
    }
    return session;
}

} // namespace opengold::original::detail
