#include "landing/manager.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace landing;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message); // Active in Release too.
}

Observation centered(std::uint64_t id, std::int64_t stamp) {
    return {id, stamp, 0.0, 0.0};
}

void expect_hold(const Decision& d, Reason reason) {
    check(d.state == State::Hold && d.command == Command::Hold, "movement survived rejection");
    check(d.reason == reason, "wrong rejection reason");
    check(!d.observation_id && d.expires_ms == d.generated_ms, "rejection retained source or validity");
}

void descend(Manager& m) {
    m.step(1000, centered(1, 1000), true);
    m.step(1100, centered(2, 1100), true);
    check(m.step(1200, centered(3, 1200), true).command == Command::DescendIntent,
          "three fresh centered observations must permit demo descent");
}

void normal() {
    Manager m;
    const auto offset = m.step(900, Observation{0, 900, 0, 1}, true);
    check(offset.state == State::Align && offset.command == Command::AlignHorizontal,
          "offset pad must request horizontal alignment");
    const auto first = m.step(1000, centered(1, 1000), true);
    check(first.state == State::Align && first.command == Command::Hold, "first center allowed descent");
    check(m.step(1100, centered(2, 1100), true).command == Command::Hold, "second center allowed descent");
    const auto third = m.step(1200, centered(3, 1200), true);
    check(third.state == State::Descend && third.observation_id == 3, "third source/state mismatch");
    check(m.step(1300, centered(4, 1300), true).command == Command::DescendIntent,
          "fresh centered tracking did not sustain descent intention");
}

void recovery() {
    Manager m;
    descend(m);
    expect_hold(m.step(1300, std::nullopt, true), Reason::Missing);
    check(m.step(1400, centered(4, 1400), true).command == Command::Hold, "loss retained alignment streak");
    check(m.step(1500, centered(5, 1500), true).command == Command::Hold, "recovery too early");
    check(m.step(1600, centered(6, 1600), true).command == Command::DescendIntent, "recovery failed");
    expect_hold(m.step(1800, centered(7, 1600), true), Reason::Stale);
    check(m.step(1801, centered(8, 1801), true).command == Command::Hold, "stale input retained streak");
}

void freshness() {
    for (const auto age : {0, 199, 200, 201}) {
        Manager m;
        const auto d = m.step(1000 + age, centered(1, 1000), true);
        if (age < 200) check(d.state == State::Align, "fresh boundary rejected");
        else expect_hold(d, Reason::Stale);
    }
    Manager m;
    expect_hold(m.step(1000, centered(1, 1001), true), Reason::Future);
    expect_hold(m.step(1000, centered(2, -1), true), Reason::Invalid);
}

void duplicate() {
    Manager m;
    descend(m);
    expect_hold(m.step(1250, centered(3, 1200), true), Reason::Repeated);
    expect_hold(m.step(1251, centered(2, 1251), true), Reason::Repeated);
    check(m.step(1300, centered(4, 1300), true).command == Command::Hold, "replay retained alignment streak");
}

void out_of_order() {
    Manager m;
    m.step(1000, centered(1, 1000), true);
    expect_hold(m.step(1001, centered(2, 1000), true), Reason::OutOfOrder);
    expect_hold(m.step(1002, centered(3, 999), true), Reason::OutOfOrder);
    check(m.step(1003, centered(4, 1003), true).command == Command::Hold, "timestamp rejection retained streak");
}

void invalid() {
    const auto inf = std::numeric_limits<double>::infinity();
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    for (const auto value : {inf, -inf, nan}) {
        for (bool forward : {true, false}) {
            Manager m;
            descend(m);
            auto o = centered(4, 1300);
            (forward ? o.forward_m : o.right_m) = value;
            expect_hold(m.step(1300, o, true), Reason::Invalid);
            check(m.step(1400, centered(5, 1400), true).command == Command::Hold, "invalid value retained streak");
        }
    }
    Manager m;
    auto o = centered(1, 1000);
    o.valid = false;
    expect_hold(m.step(1000, o, true), Reason::Invalid);
}

void alignment() {
    const auto radius = Manager::alignment_radius_m;
    for (double sign : {-1.0, 1.0}) {
        for (bool forward : {true, false}) {
            for (auto value : {std::nextafter(radius, 0.0), radius, std::nextafter(radius, 1.0)}) {
                Manager m;
                Decision d{};
                for (int i = 0; i < 3; ++i) {
                    auto o = centered(static_cast<std::uint64_t>(i), 1000 + i * 50);
                    (forward ? o.forward_m : o.right_m) = value * sign;
                    d = m.step(o.captured_ms, o, true);
                }
                check(d.command == (value <= radius ? Command::DescendIntent : Command::AlignHorizontal),
                      "radius/sign/axis boundary mismatch");
            }
        }
    }
    Manager m;
    descend(m);
    check(m.step(1300, Observation{4, 1300, radius, radius}, true).command == Command::AlignHorizontal,
          "diagonal distance was checked per axis instead of radius");
    check(m.step(1400, centered(5, 1400), true).command == Command::Hold, "offset retained streak");
}

void disabled() {
    Manager m;
    descend(m);
    expect_hold(m.step(1300, centered(4, 1300), false), Reason::Disabled);
    expect_hold(m.step(1350, centered(4, 1300), true), Reason::Repeated);
    check(m.step(1400, centered(5, 1400), true).command == Command::Hold, "re-enable retained streak");
}

void clock_bounds() {
    Manager m;
    expect_hold(m.step(-1, std::nullopt, true), Reason::ClockError);
    descend(m);
    expect_hold(m.step(1199, centered(4, 1199), true), Reason::ClockError);
    check(m.step(1300, centered(5, 1300), true).command == Command::Hold, "clock regression retained streak");
    Manager upper;
    const auto max = std::numeric_limits<std::int64_t>::max();
    check(upper.step(max - 200, centered(1, max - 200), true).expires_ms == max - 100,
          "highest supported clock failed");
    expect_hold(upper.step(max - 199, std::nullopt, true), Reason::ClockError);
    expect_hold(upper.step(max, std::nullopt, true), Reason::ClockError);
}

void expiry() {
    Manager m;
    check(m.step(1000, Observation{1, 1000, 1, 0}, true).expires_ms == 1100, "intent lifetime mismatch");
    Manager near;
    const auto d = near.step(1199, Observation{1, 1000, 1, 0}, true);
    check(d.expires_ms == 1200, "intent outlived its source observation");
}

void initial_loss() {
    Manager m;
    expect_hold(m.step(0, std::nullopt, true), Reason::Missing);
    expect_hold(m.step(100, std::nullopt, true), Reason::Missing);
    check(m.step(200, centered(0, 200), true).command == Command::Hold, "first-ever observation allowed descent");
}

void gap() {
    for (const auto gap_ms : {199, 200, 10000}) {
        Manager m;
        m.step(1000, centered(1, 1000), true);
        m.step(1100, centered(2, 1100), true);
        const auto t = 1100 + gap_ms;
        const auto d = m.step(t, centered(3, t), true);
        check(d.command == (gap_ms < 200 ? Command::DescendIntent : Command::Hold),
              "long gap counted as consecutive observations");
    }
    Manager m;
    m.step(1199, centered(1, 1000), true);
    m.step(1200, centered(2, 1001), true);
    check(m.step(1300, centered(3, 1300), true).command == Command::Hold,
          "capture gap ignored despite short delivery gap");
}

int main(int argc, char** argv) {
    struct Case { std::string_view name; void (*run)(); };
    const Case cases[] = {
        {"normal", normal}, {"recovery", recovery}, {"freshness", freshness},
        {"duplicate", duplicate}, {"out_of_order", out_of_order}, {"invalid", invalid},
        {"alignment", alignment}, {"disabled", disabled}, {"clock", clock_bounds},
        {"expiry", expiry}, {"initial_loss", initial_loss}, {"gap", gap}
    };
    if (argc != 2) return 2;
    for (const auto& test : cases) {
        if (test.name != argv[1]) continue;
        try {
            test.run();
            std::cout << "PASS " << test.name << '\n';
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
            return 1;
        }
    }
    return 2;
}
