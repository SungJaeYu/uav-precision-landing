#include "landing/manager.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace landing {

Decision Manager::step(std::int64_t now, std::optional<Observation> observation,
                       bool enabled) {
    auto hold = [&](Reason reason) {
        aligned_count_ = 0;
        // A rejected observation never retains a prior movement intention.
        return Decision{State::Hold, Command::Hold, reason, std::nullopt, now, now};
    };
    if (now < 0 || now > std::numeric_limits<std::int64_t>::max() - observation_lifetime_ms ||
        (last_now_ && now < *last_now_)) {
        return hold(Reason::ClockError);
    }
    if (last_now_ && now - *last_now_ >= observation_lifetime_ms) aligned_count_ = 0;
    last_now_ = now;
    if (!observation) return hold(Reason::Missing);
    const auto& o = *observation;
    if (!o.valid || !std::isfinite(o.forward_m) || !std::isfinite(o.right_m) || o.captured_ms < 0) {
        return hold(Reason::Invalid);
    }
    if (o.captured_ms > now) return hold(Reason::Future);
    if (now - o.captured_ms >= observation_lifetime_ms) return hold(Reason::Stale);
    if (last_id_ && o.id <= *last_id_) return hold(Reason::Repeated);
    if (last_capture_ && o.captured_ms <= *last_capture_) return hold(Reason::OutOfOrder);
    if (last_capture_ && o.captured_ms - *last_capture_ >= observation_lifetime_ms) aligned_count_ = 0;
    last_id_ = o.id;
    last_capture_ = o.captured_ms;
    if (!enabled) return hold(Reason::Disabled);

    const auto expires = std::min(now + intent_lifetime_ms,
                                  o.captured_ms + observation_lifetime_ms);
    if (std::hypot(o.forward_m, o.right_m) > alignment_radius_m) {
        aligned_count_ = 0;
        return {State::Align, Command::AlignHorizontal, Reason::Offset, o.id, now, expires};
    }
    aligned_count_ = std::min(aligned_count_ + 1, required_observations);
    if (aligned_count_ < required_observations) {
        return {State::Align, Command::Hold, Reason::Confirming, o.id, now, expires};
    }
    return {State::Descend, Command::DescendIntent, Reason::Aligned, o.id, now, expires};
}

std::string_view name(State value) {
    switch (value) {
    case State::Hold: return "HOLD";
    case State::Align: return "ALIGN";
    case State::Descend: return "DESCEND";
    }
    return "UNKNOWN";
}
std::string_view name(Command value) {
    switch (value) {
    case Command::Hold: return "HOLD";
    case Command::AlignHorizontal: return "ALIGN_HORIZONTAL";
    case Command::DescendIntent: return "DESCEND_INTENT";
    }
    return "UNKNOWN";
}
std::string_view name(Reason value) {
    switch (value) {
    case Reason::Missing: return "missing";
    case Reason::Invalid: return "invalid";
    case Reason::Future: return "future";
    case Reason::Stale: return "stale";
    case Reason::Repeated: return "repeated_or_reversed_id";
    case Reason::OutOfOrder: return "out_of_order_capture";
    case Reason::ClockError: return "clock_error";
    case Reason::Disabled: return "disabled";
    case Reason::Offset: return "offset";
    case Reason::Confirming: return "confirming_alignment";
    case Reason::Aligned: return "aligned";
    }
    return "unknown";
}

} // namespace landing
