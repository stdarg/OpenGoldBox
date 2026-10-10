// Included by the SRD session implementation. All trigger legality and outcomes
// remain in the static rules library; presentation consumes the resulting views.
Actor Session::mastery_actor(const PendingMastery &m) const
{
    return thrown_actor(actor(m.actor), m.weapon);
}

std::vector<EntityId> Session::cleave_targets(const PendingMastery &m,
        std::optional<Cell> from) const
{
    std::vector<EntityId> result;
    const auto &a = actor(m.actor);
    const auto &first = actor(m.target);
    if (a.cleave_used)
        return result;
    const auto reach = def(mastery_actor(m)).reach;
    for (const auto &target : actors_)
        if (target.source.id != m.actor && target.source.id != m.target && !target.life.dead &&
                distance(first.source.cell, target.source.cell) <= 5 &&
                distance(from.value_or(a.source.cell), target.source.cell) <= reach &&
                line_of_sight(from.value_or(a.source.cell), target.source.cell))
            result.push_back(target.source.id);
    std::stable_sort(result.begin(), result.end(),
                     [&](auto x, auto y)
    {
        return (actor(x).source.side != a.source.side) >
               (actor(y).source.side != a.source.side);
    });
    return result;
}

std::vector<Cell> Session::push_cells(const PendingMastery &m, std::optional<Cell> from) const
{
    std::vector<Cell> result;
    const auto &a = actor(m.actor);
    const auto &target = actor(m.target);
    if (target.life.dead || def(target).size > 3)
        return result;
    const auto start = target.source.cell, origin = from.value_or(a.source.cell);
    const int dx = start.x - origin.x, dy = start.y - origin.y;
    if (!dx && !dy)
        return result;
    auto obstacles = board_;
    for (const auto &other : actors_)
        if (!other.life.dead && other.source.id != target.source.id)
        {
            const auto cell = other.source.id == a.source.id ? origin : other.source.cell;
            obstacles.terrain[cell.y * board_.width + cell.x] = 1;
        }
    for (int y = std::max(0, start.y - 2); y <= std::min(board_.height - 1, start.y + 2); ++y)
        for (int x = std::max(0, start.x - 2); x <= std::min(board_.width - 1, start.x + 2); ++x)
        {
            const int step_x = x - start.x, step_y = y - start.y;
            const Cell cell{x, y};
            if (step_x * dy != step_y * dx || step_x * dx + step_y * dy <= 0 ||
                    obstacles.at(cell) == 1)
                continue;
            if (detail::has_line_of_sight(obstacles, start, cell))
                result.push_back(cell);
        }
    return result;
}

bool Session::mastery_available(const PendingMastery &m) const
{
    const auto &target = actor(m.target);
    const auto &source = actor(m.actor);
    if (!conscious(source))
        return false;
    if (m.kind == detail::Mastery::cleave)
        return !cleave_targets(m).empty();
    if (target.life.dead)
        return false;
    if (m.kind == detail::Mastery::push)
        return !push_cells(m).empty();
    if (m.kind == detail::Mastery::topple)
        return !target.effects.prone;
    if (m.kind != detail::Mastery::slow ||
            !detail::can_apply_attack_mastery(target.effects, detail::EffectKind::slow, scope_,
                    m.actor))
        return false;
    const auto duration = next_turn_ms(source);
    if (std::any_of(target.effects.active.begin(), target.effects.active.end(),
                    [&](const auto & effect)
{
    return effect.kind == detail::EffectKind::slow &&
           effect.remaining_ms >= duration;
}))
    return false;
    auto after = target.effects;
    detail::apply_attack_mastery(after, detail::EffectKind::slow, scope_, m.actor,
                                 source.source.name, next_turn_ms(source));
    return after != target.effects;
}

void Session::offer_mastery(const Actor &a, const Actor &target, int natural, bool ranged,
                            int damage, bool critical)
{
    const auto kind = weapon_mastery(a, ranged);
    if (kind != detail::Mastery::slow && kind != detail::Mastery::topple &&
            kind != detail::Mastery::cleave && kind != detail::Mastery::push)
        return;
    if ((kind == detail::Mastery::slow && damage <= 0) ||
            (kind == detail::Mastery::cleave && (ranged || actor(a.source.id).cleave_used)) ||
            (kind != detail::Mastery::cleave && target.life.dead) ||
            (kind == detail::Mastery::push && def(target).size > 3))
        return;
    std::string key;
    for (const auto &candidate : def(a).equipment_keys)
        if (detail::weapon(candidate))
        {
            key = candidate;
            break;
        }
    PendingMastery m{a.source.id, target.source.id, kind, natural, ranged, false,
                     key,         a.source.cell};
    if (!mastery_available(m))
    {
        bool possible = false;
        if (critical && def(a).champion && actors_[turn_].source.side == 0 &&
                (kind == detail::Mastery::cleave || kind == detail::Mastery::push))
        {
            const auto reachable =
                movement_grid(actor(a.source.id))
                .reachable(std::max(0, def(a).speed - detail::speed_penalty(a.effects)) / 2);
            for (int y = 0; y < board_.height && !possible; ++y)
                for (int x = 0; x < board_.width && !possible; ++x)
                    if (reachable.cost_to({x, y}))
                        possible = kind == detail::Mastery::cleave
                                   ? !cleave_targets(m, Cell{x, y}).empty()
                                   : !push_cells(m, Cell{x, y}).empty();
        }
        if (!possible)
            return;
    }
    mastery_ = std::move(m);
    if (pending() && !effect_reaction_origin_)
        effect_reaction_origin_ =
            EffectReaction{a.source.id, a.source.cell, actors_[turn_].source.cell,
                           movement_left(actors_[turn_]), actors_[turn_].effects.prone};
}

OptionalEffectChoice Session::effect_choices() const
{
    OptionalEffectChoice choice;
    if (mastery_)
    {
        const auto &m = *mastery_;
        choice.actor = m.actor;
        choice.target = m.target;
        const auto label = std::string(detail::mastery_name(m.kind));
        Message description;
        if (m.kind == detail::Mastery::topple)
        {
            const auto attacking = mastery_actor(m);
            const auto &d = def(attacking);
            const int dc =
                8 + 2 + (d.level - 1) / 4 + (m.ranged ? d.ranged_ability : d.melee_ability);
            description =
            {
                "Target: {target}\nConstitution save DC {dc}; failure makes the target Prone.\nThe triggering Action or Reaction is already spent.",
                {{"target", actor(m.target).source.name}, {"dc", std::to_string(dc)}}
            };
        }
        else
            description =
        {
            m.kind == detail::Mastery::slow
            ? "Target: {target}\nReduce Speed by 10 feet until your next turn starts.\nThe triggering Action or Reaction is already spent."
            : m.kind == detail::Mastery::cleave
            ? "Target: {target}\nMake one extra attack against another creature within 5 feet of this target and your reach.\nThe triggering Action or Reaction is already spent."
            : "Target: {target}\nPush up to 10 feet straight away from you.\nThe triggering Action or Reaction is already spent.",
            {{"target", actor(m.target).source.name}}
        };
        choice.options.push_back({1, {label, {}}, description, mastery_available(m)});
    }
    // On an enemy turn, the predetermined order is mastery before movement.
    if (!mastery_ || actors_[turn_].source.side == 0)
        for (unsigned i = 0; i < champion_offers_.size(); ++i)
        {
            const auto &move = champion_offers_[i];
            if (!choice.actor)
            {
                choice.actor = move.actor;
                choice.target = move.target;
            }
            choice.options.push_back(
            {
                i + 2,
                {"Champion movement ({target})", {{"target", actor(move.target).source.name}}},
                {
                    "Move up to {feet} feet without Opportunity Attacks or spending normal movement. Other pending effects wait.",
                    {{"feet", std::to_string(move.remaining)}}
                },
                true});
        }
    if (!choice.options.empty())
    {
        choice.title = choice.options.front().title;
        choice.description = choice.options.front().description;
    }
    return choice;
}

bool Session::use_effect(const Command &command)
{
    if (command.verb == "effect_attack")
    {
        const auto m = *mastery_;
        mastery_.reset();
        auto &source = actor(m.actor);
        source.cleave_used = true;
        auto attacking = source;
        attacking.cleave_damage = true;
        attack(attacking, actor(command.target), false);
        source.facing_left = attacking.facing_left;
    }
    else if (command.verb == "effect_push")
    {
        const auto m = *mastery_;
        mastery_.reset();
        auto &target = actor(m.target);
        target.source.cell = command.destination;
        clear_departed_overlaps();
        log(target.source.name + " is pushed.",
        {"{name} is pushed.", {{"name", target.source.name}}});
    }
    else if (command.item >= 2)
    {
        const auto index = command.item - 2;
        auto move = champion_offers_.at(index);
        champion_offers_.erase(champion_offers_.begin() + index);
        if (command.verb == "effect_use")
        {
            move.origin = actor(move.actor).source.cell;
            champion_move_ = move;
        }
    }
    else
    {
        const auto m = *mastery_;
        if (command.verb == "effect_use" &&
                (m.kind == detail::Mastery::cleave || m.kind == detail::Mastery::push))
        {
            mastery_->targeting = true;
            return true;
        }
        mastery_.reset();
        if (command.verb == "effect_use")
        {
            auto &target = actor(m.target);
            const auto &source = actor(m.actor);
            if (m.kind == detail::Mastery::slow)
            {
                detail::apply_attack_mastery(target.effects, detail::EffectKind::slow, scope_,
                                             m.actor, source.source.name, next_turn_ms(source));
                log(target.source.name + " gains Slow from " + source.source.name + ".",
                {
                    "{name} gains {mastery} from {source}.",
                    {   {"name", target.source.name},
                        {"mastery", "Slow", true},
                        {"source", source.source.name}
                    }
                });
            }
            else if (m.kind == detail::Mastery::topple)
            {
                const auto attacking = mastery_actor(m);
                const auto &d = def(attacking);
                if (!saving_throw_succeeds(target, detail::Ability::constitution,
                                           8 + 2 + (d.level - 1) / 4 +
                                           (m.ranged ? d.ranged_ability : d.melee_ability)))
                    target.effects.prone = true;
            }
        }
    }
    if (mastery_ && !mastery_available(*mastery_) && champion_offers_.empty() && !champion_move_)
        mastery_.reset();
    if (!effect_waiting() && !champion_move_)
        finish_effects();
    return true;
}

void Session::validate_mastery_state() const
{
    if (champion_offers_.size() > 2)
        throw std::runtime_error("Too many pending Champion effects");
    if (!effect_waiting() && !effect_reaction_origin_)
        return;
    if (outcome_ != Outcome::ongoing || graze_ || check_choice_ || temporary_offer_)
        throw std::runtime_error("Conflicting pending mastery effects");
    if (mastery_)
    {
        const auto &m = *mastery_;
        const auto a = std::find_if(actors_.begin(), actors_.end(),
                                    [&](const auto & a)
        {
            return a.source.id == m.actor;
        });
        const auto t = std::find_if(actors_.begin(), actors_.end(),
                                    [&](const auto & a)
        {
            return a.source.id == m.target;
        });
        if (a == actors_.end() || t == actors_.end() || a == t || !conscious(*a) ||
                m.weapon.size() > 80 || !detail::weapon(m.weapon) ||
                (m.kind != detail::Mastery::slow && m.kind != detail::Mastery::topple &&
                 m.kind != detail::Mastery::cleave && m.kind != detail::Mastery::push) ||
                m.natural < 2 || m.natural > 20 || m.origin.x < 0 || m.origin.y < 0 ||
                m.origin.x >= board_.width || m.origin.y >= board_.height || board_.at(m.origin) == 1 ||
                (m.targeting && m.kind != detail::Mastery::cleave && m.kind != detail::Mastery::push))
            throw std::runtime_error("Invalid pending mastery source");
        const auto attacking = mastery_actor(m);
        const auto &d = def(attacking);
        if (weapon_mastery(attacking, m.ranged) != m.kind ||
                distance(m.origin, t->source.cell) > (m.ranged ? d.long_range : d.reach) ||
                !line_of_sight(m.origin, t->source.cell) ||
                (!(d.champion && m.natural == 19) &&
                 !attack_hits(m.natural, m.ranged ? d.ranged_bonus : d.melee_bonus, def(*t).ac)) ||
                (m.kind == detail::Mastery::cleave && (m.ranged || a->cleave_used)) ||
                (m.kind == detail::Mastery::push && def(*t).size > 3) ||
                (m.actor == actors_[turn_].source.id
                 ? (a->actions.normal && (!a->surge_used || a->actions.surge))
                 : (a->reaction ||
                    (pending() ? pending() != m.actor
                     : (m.target != actors_[turn_].source.id || actors_[turn_].life.hp > 0)))))
            throw std::runtime_error("Invalid pending mastery trigger");
        if (!m.thrown_item)
        {
            const bool owned =
                physical_inventory_
                ? std::any_of(items_.begin(), items_.end(),
                              [&](const auto & item)
            {
                return item.holder == m.actor && !item.stowed &&
                       item.definition == m.weapon;
            })
                : std::find(def(*a).equipment_keys.begin(), def(*a).equipment_keys.end(),
                            m.weapon) != def(*a).equipment_keys.end();
            if (!owned)
                throw std::runtime_error("Pending mastery weapon is not held");
        }
        if (m.thrown_item)
        {
            if (!m.ranged || m.thrown_item > items_.size())
                throw std::runtime_error("Invalid mastery thrown item");
            const auto &item = items_[m.thrown_item - 1];
            if (item.holder != m.actor || item.definition != m.weapon ||
                    !detail::weapon(m.weapon)->thrown)
                throw std::runtime_error("Invalid mastery thrown provenance");
        }
    }
    std::set<EntityId> targets;
    bool cleave = false, ordinary = false;
    for (const auto &c : champion_offers_)
    {
        const auto who = std::find_if(actors_.begin(), actors_.end(),
                                      [&](const auto & a)
        {
            return a.source.id == c.actor;
        });
        const auto target = std::find_if(actors_.begin(), actors_.end(),
                                         [&](const auto & a)
        {
            return a.source.id == c.target;
        });
        if (!targets.insert(c.target).second ||
                (c.cleave ? std::exchange(cleave, true) : std::exchange(ordinary, true)) ||
                !board_.contains(c.origin) || board_.at(c.origin) == 1 ||
                !board_.contains(c.trigger_origin) || board_.at(c.trigger_origin) == 1 ||
                !board_.contains(c.target_origin) || board_.at(c.target_origin) == 1 ||
                c.origin != c.trigger_origin || (champion_move_ && champion_move_->actor != c.actor) ||
                who == actors_.end() || target == actors_.end() || !c.triggered ||
                !def(*who).champion || !conscious(*who) || c.actor == c.target || c.natural < 2 ||
                c.natural > 20 ||
                c.remaining != std::max(0, def(*who).speed - detail::speed_penalty(who->effects)) / 2 ||
                !(c.natural == 20 || (!c.spell && c.natural == 19) ||
                  ((c.helpless || target->life.hp == 0) &&
                   distance(c.trigger_origin, c.target_origin) <= 5)) ||
                (c.cleave && !who->cleave_used) || (mastery_ && mastery_->actor != c.actor) ||
                (c.actor == actors_[turn_].source.id
                 ? (who->actions.normal && (!who->surge_used || who->actions.surge))
                 : (who->reaction || (pending() ? pending() != c.actor : actors_[turn_].life.hp > 0))))
            throw std::runtime_error("Invalid queued Champion effect");
    }
    if (effect_reaction_origin_)
    {
        const auto &r = *effect_reaction_origin_;
        const auto p = r.source;
        if (!board_.contains(p) || board_.at(p) == 1 || !board_.contains(r.mover) ||
                board_.at(r.mover) == 1 || r.movement < 0 || r.movement > actors_[turn_].movement ||
                r.actor == actors_[turn_].source.id || (pending() && pending() != r.actor) ||
                (!pending() && actors_[turn_].life.hp > 0) ||
                (!effect_waiting() && !champion_move_))
            throw std::runtime_error("Invalid mastery reaction origin");
    }
}
