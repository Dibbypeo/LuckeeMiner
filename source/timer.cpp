#include "luckee/timer.hpp"

#include <algorithm>

namespace luckee {

Timer::Timer(float ticksPerSecond)
    : ticksPerSecond_(ticksPerSecond),
      lastTime_(clock::now()) {
}

void Timer::advanceTime() {
    const auto now = clock::now();

    auto passedNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            now - lastTime_).count();

    lastTime_ = now;

    if (passedNs < 0)
        passedNs = 0;

    if (passedNs > 1000000000LL)
        passedNs = 1000000000LL;

    passedTime_ +=
        static_cast<float>(passedNs) *
        ticksPerSecond_ /
        1000000000.0f;

    ticks_ = static_cast<int>(passedTime_);

    if (ticks_ > 100)
        ticks_ = 100;

    passedTime_ -= static_cast<float>(ticks_);
    alpha_ = passedTime_;
}

} // namespace luckee
