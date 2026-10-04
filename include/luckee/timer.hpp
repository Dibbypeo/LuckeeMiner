#pragma once

#include <chrono>

namespace luckee {

// Fixed-rate simulation clock modeled after rd-132211's Timer.
// Rendering may run at any rate, while simulation advances in 60 Hz steps.
class Timer {
public:
    explicit Timer(float ticksPerSecond = 60.0f);

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
