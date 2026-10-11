#pragma once

#include <array>
#include <vector>

#include "luckee/java_random.hpp"

namespace luckee {

// The Classic client uses improved Perlin noise, eight stacked octaves and
// a two-field distortion step. These classes preserve that algorithm while
// keeping the implementation independent of the original Java classes.
class ClassicImprovedNoise {
public:
    explicit ClassicImprovedNoise(JavaRandom& random);
    double sample(double x, double z) const;

private:
    static double fade(double value);
    static double lerp(double amount, double from, double to);
    static double gradient(int hash, double x, double y, double z);

    std::array<int, 512> permutation_{};
};

class ClassicOctaveNoise {
public:
    ClassicOctaveNoise(JavaRandom& random, int octaveCount = 8);
    double sample(double x, double z) const;

private:
    std::vector<ClassicImprovedNoise> octaves_;
};

class ClassicDistortedNoise {
public:
    ClassicDistortedNoise(
        const ClassicOctaveNoise& primary,
        const ClassicOctaveNoise& distortion);

    double sample(double x, double z) const;

private:
    const ClassicOctaveNoise& primary_;
    const ClassicOctaveNoise& distortion_;
};

} // namespace luckee
