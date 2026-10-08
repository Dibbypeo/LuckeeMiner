#pragma once

#include "luckee/entity.hpp"

namespace luckee {

class Particle final : public Entity {
public:
    Particle(
        Level& level,
        float x, float y, float z,
        float xa, float ya, float za,
        int texture);

    void tick() override;

    int texture() const { return texture_; }
    float size() const { return size_; }

private:
    float xd_ = 0.0f;
    float yd_ = 0.0f;
    float zd_ = 0.0f;
    int texture_ = 0;
    float uo_ = 0.0f;
    float vo_ = 0.0f;
    int age_ = 0;
    int lifetime_ = 0;
    float size_ = 1.0f;
};

} // namespace luckee
