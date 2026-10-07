#pragma once

#include <array>

#include "luckee/entity.hpp"

namespace luckee {

struct CharacterPart {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    float minX = 0.0f;
    float minY = 0.0f;
    float minZ = 0.0f;

    int width = 0;
    int height = 0;
    int depth = 0;

    int texX = 0;
    int texY = 0;

    float xRot = 0.0f;
    float yRot = 0.0f;
    float zRot = 0.0f;
};

class Zombie final : public Entity {
public:
    Zombie(Level& level, float x, float y, float z);

    void tick() override;

    float bodyRotation() const { return rot_; }
    float speed() const { return speed_; }
    float timeOffset() const { return timeOffs_; }

    const std::array<CharacterPart, 6>& parts() const {
        return parts_;
    }

private:
    float rot_ = 0.0f;
    float timeOffs_ = 0.0f;
    float speed_ = 1.0f;
    float rotA_ = 0.01f;

    std::array<CharacterPart, 6> parts_{};
};

} // namespace luckee
