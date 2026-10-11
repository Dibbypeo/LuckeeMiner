#pragma once

#include "luckee/entity.hpp"

namespace luckee {

struct InputState;

class Player final : public Entity {
public:
    using Entity::tick;

    explicit Player(Level& level);

    void resetPosition();
    void tick(const InputState& input);

private:
    // Mirrors the original key flag being consumed by a dry-land jump.
    bool jumpConsumed_ = false;
};

} // namespace luckee
