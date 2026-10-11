#include "luckee/classic_noise.hpp"

#include <cmath>

namespace luckee {

ClassicImprovedNoise::ClassicImprovedNoise(JavaRandom& random) {
    for (int i = 0; i < 256; ++i)
        permutation_[static_cast<std::size_t>(i)] = i;

    for (int i = 0; i < 256; ++i) {
        const int swapIndex = random.nextInt(256 - i) + i;
        const int value = permutation_[static_cast<std::size_t>(i)];
        permutation_[static_cast<std::size_t>(i)] =
            permutation_[static_cast<std::size_t>(swapIndex)];
        permutation_[static_cast<std::size_t>(swapIndex)] = value;
        permutation_[static_cast<std::size_t>(i + 256)] =
            permutation_[static_cast<std::size_t>(i)];
    }
}

double ClassicImprovedNoise::fade(double value) {
    return value * value * value *
           (value * (value * 6.0 - 15.0) + 10.0);
}

double ClassicImprovedNoise::lerp(
    double amount, double from, double to) {
    return from + amount * (to - from);
}

double ClassicImprovedNoise::gradient(
    int hash, double x, double y, double z) {
    const int h = hash & 15;
    const double first = h < 8 ? x : y;
    const double second =
        h < 4 ? y : ((h == 12 || h == 14) ? x : z);

    return ((h & 1) == 0 ? first : -first) +
           ((h & 2) == 0 ? second : -second);
}

double ClassicImprovedNoise::sample(double x, double z) const {
    const double floorX = std::floor(x);
    const double floorZ = std::floor(z);
    const int ix = static_cast<int>(floorX) & 255;
    const int iz = static_cast<int>(floorZ) & 255;

    x -= floorX;
    z -= floorZ;

    // The Classic generator evaluates the third noise axis at exactly zero.
    constexpr double y = 0.0;
    const double u = fade(x);
    const double v = fade(z);
    const double w = fade(y);

    const int a = permutation_[static_cast<std::size_t>(ix)] + iz;
    const int aa = permutation_[static_cast<std::size_t>(a)];
    const int ab = permutation_[static_cast<std::size_t>(a + 1)];
    const int b = permutation_[static_cast<std::size_t>(ix + 1)] + iz;
    const int ba = permutation_[static_cast<std::size_t>(b)];
    const int bb = permutation_[static_cast<std::size_t>(b + 1)];

    return lerp(
        w,
        lerp(
            v,
            lerp(
                u,
                gradient(permutation_[static_cast<std::size_t>(aa)], x, y, z),
                gradient(permutation_[static_cast<std::size_t>(ba)], x - 1.0, y, z)),
            lerp(
                u,
                gradient(permutation_[static_cast<std::size_t>(ab)], x, y - 1.0, z),
                gradient(permutation_[static_cast<std::size_t>(bb)], x - 1.0, y - 1.0, z))),
        lerp(
            v,
            lerp(
                u,
                gradient(permutation_[static_cast<std::size_t>(aa + 1)], x, y, z - 1.0),
                gradient(permutation_[static_cast<std::size_t>(ba + 1)], x - 1.0, y, z - 1.0)),
            lerp(
                u,
                gradient(permutation_[static_cast<std::size_t>(ab + 1)], x, y - 1.0, z - 1.0),
                gradient(permutation_[static_cast<std::size_t>(bb + 1)], x - 1.0, y - 1.0, z - 1.0))));
}

ClassicOctaveNoise::ClassicOctaveNoise(
    JavaRandom& random, int octaveCount) {
    if (octaveCount < 1)
        octaveCount = 1;
    octaves_.reserve(static_cast<std::size_t>(octaveCount));
    for (int i = 0; i < octaveCount; ++i)
        octaves_.emplace_back(random);
}

double ClassicOctaveNoise::sample(double x, double z) const {
    double value = 0.0;
    double scale = 1.0;
    for (const ClassicImprovedNoise& octave : octaves_) {
        value += octave.sample(x / scale, z / scale) * scale;
        scale *= 2.0;
    }
    return value;
}

ClassicDistortedNoise::ClassicDistortedNoise(
    const ClassicOctaveNoise& primary,
    const ClassicOctaveNoise& distortion)
    : primary_(primary), distortion_(distortion) {
}

double ClassicDistortedNoise::sample(double x, double z) const {
    return primary_.sample(x + distortion_.sample(x, z), z);
}

} // namespace luckee
