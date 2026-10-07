#pragma once

#include "luckee/entity.hpp"

namespace luckee {

struct InputState;

class Player final : public Entity {
public:
    explicit Player(Level& level);

    void tick(const InputState& input);
};

} // namespace luckee
