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
    bool jumpWasHeld_ = false;
};

} // namespace luckee
