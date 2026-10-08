#include "luckee/perlin_noise.hpp"

#include <algorithm>
#include <cstdint>
#include <random>

namespace luckee {

PerlinNoiseFilter::PerlinNoiseFilter(int levels)
    : levels_(levels) {
}

std::vector<int> PerlinNoiseFilter::read(
    int width,
    int height) {
    std::mt19937 random(
        std::random_device{}());

    std::vector<int> tmp(
        static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height),
        0);

    const int level = levels_;
    int step = width >> level;

    for (int y = 0; y < height; y += step) {
        for (int x = 0; x < width; x += step) {
            tmp[
                static_cast<std::size_t>(x) +
                static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(width)] =
                (static_cast<int>(random() & 0xFFu) - 128) *
                fuzz_;
        }
    }

    step = width >> level;

    while (step > 1) {
        const int value =
            256 * (step << level);
        const int half = step / 2;

        for (int y = 0; y < height; y += step) {
            for (int x = 0; x < width; x += step) {
                const int ul =
                    tmp[
                        static_cast<std::size_t>(x % width) +
                        static_cast<std::size_t>(y % height) *
                            static_cast<std::size_t>(width)];

                const int ur =
                    tmp[
                        static_cast<std::size_t>((x + step) % width) +
                        static_cast<std::size_t>(y % height) *
                            static_cast<std::size_t>(width)];

                const int dl =
                    tmp[
                        static_cast<std::size_t>(x % width) +
                        static_cast<std::size_t>((y + step) % height) *
                            static_cast<std::size_t>(width)];

                const int dr =
                    tmp[
                        static_cast<std::size_t>((x + step) % width) +
                        static_cast<std::size_t>((y + step) % height) *
                            static_cast<std::size_t>(width)];

                const int noise =
                    static_cast<int>(
                        random() %
                        static_cast<std::uint32_t>(value * 2)) -
                    value;

                tmp[
                    static_cast<std::size_t>(x + half) +
                    static_cast<std::size_t>(y + half) *
                        static_cast<std::size_t>(width)] =
                    (ul + dl + ur + dr) / 4 + noise;
            }
        }

        for (int y = 0; y < height; y += step) {
            for (int x = 0; x < width; x += step) {
                const int c =
                    tmp[
                        static_cast<std::size_t>(x) +
                        static_cast<std::size_t>(y) *
                            static_cast<std::size_t>(width)];

                const int r =
                    tmp[
                        static_cast<std::size_t>((x + step) % width) +
                        static_cast<std::size_t>(y) *
                            static_cast<std::size_t>(width)];

                const int d =
                    tmp[
                        static_cast<std::size_t>(x) +
                        static_cast<std::size_t>((y + step) % height) *
                            static_cast<std::size_t>(width)];

                const int mu =
                    tmp[
                        static_cast<std::size_t>(
                            (x + half - step) & (width - 1)) +
                        static_cast<std::size_t>(
                            (y + half - step) & (height - 1)) *
                            static_cast<std::size_t>(width)];

                const int ml =
                    tmp[
                        static_cast<std::size_t>(
                            (x + half - step) & (width - 1)) +
                        static_cast<std::size_t>(
                            (y + half) & (height - 1)) *
                            static_cast<std::size_t>(width)];

                const int m =
                    tmp[
                        static_cast<std::size_t>((x + half) % width) +
                        static_cast<std::size_t>((y + half) % height) *
                            static_cast<std::size_t>(width)];

                const int upperNoise =
                    static_cast<int>(
                        random() %
                        static_cast<std::uint32_t>(value * 2)) -
                    value;

                const int lowerNoise =
                    static_cast<int>(
                        random() %
                        static_cast<std::uint32_t>(value * 2)) -
                    value;

                tmp[
                    static_cast<std::size_t>(x + half) +
                    static_cast<std::size_t>(y) *
                        static_cast<std::size_t>(width)] =
                    (c + r + m + mu) / 4 + upperNoise;

                tmp[
                    static_cast<std::size_t>(x) +
                    static_cast<std::size_t>(y + half) *
                        static_cast<std::size_t>(width)] =
                    (c + d + m + ml) / 4 + lowerNoise;
            }
        }

        step /= 2;
    }

    std::vector<int> result(
        static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height),
        0);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            result[
                static_cast<std::size_t>(x) +
                static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(width)] =
                tmp[
                    static_cast<std::size_t>(x % width) +
                    static_cast<std::size_t>(y % height) *
                        static_cast<std::size_t>(width)] / 512 +
                128;
        }
    }

    return result;
}

} // namespace luckee
