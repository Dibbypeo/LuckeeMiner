#pragma once

#include <vector>

namespace luckee {

class PerlinNoiseFilter {
public:
    explicit PerlinNoiseFilter(int levels);

    std::vector<int> read(int width, int height);

private:
    int levels_;
    int fuzz_ = 16;
};

} // namespace luckee
