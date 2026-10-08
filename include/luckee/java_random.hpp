#pragma once

#include <chrono>
#include <cstdint>

namespace luckee {

/**
 * Small Java-compatible 48-bit linear-congruential RNG.
 *
 * The historical client uses java.util.Random and Math.random(). Keeping the
 * same generator here makes random ranges and floating-point random values
 * behave like the reference while leaving seeding native to the 3DS.
 */
class JavaRandom {
public:
    JavaRandom()
        : JavaRandom(defaultSeed()) {
    }

    explicit JavaRandom(std::uint64_t seed) {
        setSeed(seed);
    }

    void setSeed(std::uint64_t seed) {
        seed_ =
            (seed ^ MULTIPLIER) &
            MASK;
    }

    int nextInt() {
        return static_cast<int>(
            nextBits(32));
    }

    int nextInt(int bound) {
        if (bound <= 0)
            return 0;

        if ((bound & -bound) == bound) {
            return static_cast<int>(
                (static_cast<std::int64_t>(bound) *
                 static_cast<std::int64_t>(
                     nextBits(31))) >>
                31);
        }

        int bits = 0;
        int value = 0;

        do {
            bits =
                static_cast<int>(
                    nextBits(31));
            value =
                bits % bound;
        } while (
            bits - value + (bound - 1) < 0);

        return value;
    }

    float nextFloat() {
        return static_cast<float>(
                   nextBits(24)) /
               16777216.0f;
    }

    double nextDouble() {
        const std::uint64_t hi =
            static_cast<std::uint64_t>(
                nextBits(26));
        const std::uint64_t lo =
            static_cast<std::uint64_t>(
                nextBits(27));

        return static_cast<double>(
                   (hi << 27) + lo) /
               9007199254740992.0;
    }

private:
    static constexpr std::uint64_t MULTIPLIER =
        0x5DEECE66DULL;
    static constexpr std::uint64_t ADDEND =
        0xBULL;
    static constexpr std::uint64_t MASK =
        (1ULL << 48) - 1ULL;

    std::uint64_t seed_ = 0;

    std::uint32_t nextBits(int bits) {
        seed_ =
            (seed_ * MULTIPLIER + ADDEND) &
            MASK;

        return static_cast<std::uint32_t>(
            seed_ >> (48 - bits));
    }

    static std::uint64_t defaultSeed() {
        static std::uint64_t counter = 0;

        const auto now =
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count();

        ++counter;

        return static_cast<std::uint64_t>(now) ^
               (counter *
                0x9E3779B97F4A7C15ULL);
    }
};

} // namespace luckee
