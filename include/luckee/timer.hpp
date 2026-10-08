#pragma once

#include <chrono>

namespace luckee {

// Fixed-rate simulation clock modeled after the rd-20090515 Timer.
// Rendering may run at any rate, while simulation advances in 20 Hz steps.
class Timer {
public:
    explicit Timer(float ticksPerSecond = 20.0f);

    void advanceTime();

    int ticks() const { return ticks_; }
    float alpha() const { return alpha_; }

private:
    using clock = std::chrono::steady_clock;

    float ticksPerSecond_;
    clock::time_point lastTime_;
    float passedTime_ = 0.0f;
    int ticks_ = 0;
    float alpha_ = 0.0f;
};

} // namespace luckee
