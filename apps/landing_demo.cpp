#include "landing/manager.hpp"

#include <iostream>
#include <optional>

int main() {
    using namespace landing;
    struct Input {
        const char* label;
        std::int64_t now;
        std::optional<Observation> observation;
        bool enabled = true;
    };
    const Input inputs[] = {
        {"pad_right", 1000, Observation{1, 1000, 0.0, 1.0}},
        {"center_1", 1100, Observation{2, 1100, 0.0, 0.0}},
        {"center_2", 1200, Observation{3, 1200, 0.0, 0.0}},
        {"center_3", 1300, Observation{4, 1300, 0.0, 0.0}},
        {"delayed_frame", 1500, Observation{5, 1300, 0.0, 0.0}},
        {"observation_lost", 1600, std::nullopt},
        {"recovered_1", 1700, Observation{6, 1700, 0.0, 0.0}},
        {"recovered_2", 1800, Observation{7, 1800, 0.0, 0.0}},
        {"recovered_3", 1900, Observation{8, 1900, 0.0, 0.0}},
        {"replayed_frame", 1950, Observation{8, 1900, 0.0, 0.0}},
        {"mission_disabled", 2000, Observation{9, 2000, 0.0, 0.0}, false},
    };
    Manager manager;
    std::cout << "scenario,sensor_ms,state,intent,reason,source_id,expires_ms\n";
    for (const auto& input : inputs) {
        const auto result = manager.step(input.now, input.observation, input.enabled);
        std::cout << input.label << ',' << input.now << ',' << name(result.state) << ','
                  << name(result.command) << ',' << name(result.reason) << ',';
        if (result.observation_id) std::cout << *result.observation_id;
        std::cout << ',' << result.expires_ms << '\n';
    }
}
