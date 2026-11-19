#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace landing {

// M1: one synthetic monotonic clock, integer milliseconds, one stream per manager.
// Positions are pad offsets in body FRD axes (forward, right), in metres.
struct Observation {
    std::uint64_t id;
    std::int64_t captured_ms;
    double forward_m;
    double right_m;
    bool valid = true;
};

enum class State { Hold, Align, Descend };
enum class Command { Hold, AlignHorizontal, DescendIntent };
enum class Reason {
    Missing, Invalid, Future, Stale, Repeated, OutOfOrder, ClockError,
    Disabled, Offset, Confirming, Aligned
};

struct Decision {
    State state;
    Command command;
    Reason reason;
    std::optional<std::uint64_t> observation_id;
    std::int64_t generated_ms;
    std::int64_t expires_ms; // Intent validity is [generated_ms, expires_ms).
};

class Manager {
public:
    // Demonstration parameters, not flight requirements or tuned control limits.
    static constexpr std::int64_t observation_lifetime_ms = 200;
    static constexpr std::int64_t intent_lifetime_ms = 100;
    static constexpr double alignment_radius_m = 0.2;
    static constexpr unsigned required_observations = 3;

    // Every call consumes a newly delivered observation, or nullopt (loss).
    // enabled is a synthetic mission gate, NOT a real vehicle safety assessment.
    Decision step(std::int64_t now_ms, std::optional<Observation> observation,
                  bool enabled);

private:
    unsigned aligned_count_ = 0;
    std::optional<std::int64_t> last_now_;
    std::optional<std::uint64_t> last_id_;
    std::optional<std::int64_t> last_capture_;
};

std::string_view name(State value);
std::string_view name(Command value);
std::string_view name(Reason value);

} // namespace landing
