#pragma once

#include "luckee/aabb.hpp"

namespace luckee {

class Level;
struct InputState;

class Player {
public:
    explicit Player(Level& level);

    void tick(const InputState& input);
    void turn(float deltaX, float deltaY);

    float x() const { return x_; }
    float y() const { return y_; }
    float z() const { return z_; }
    float yRot() const { return yRot_; }
    float xRot() const { return xRot_; }
    bool onGround() const { return onGround_; }

private:
    void resetPos();
    void setPos(float x, float y, float z);
    void move(float xa, float ya, float za);
    void moveRelative(float xa, float za, float speed);

    Level& level_;
    float xo_ = 0.0f;
    float yo_ = 0.0f;
    float zo_ = 0.0f;
    float x_ = 0.0f;
    float y_ = 0.0f;
    float z_ = 0.0f;
    float xd_ = 0.0f;
    float yd_ = 0.0f;
    float zd_ = 0.0f;
    float yRot_ = 0.0f;
    float xRot_ = 0.0f;
    AABB bb_{0, 0, 0, 0, 0, 0};
    bool onGround_ = false;
};

} // namespace luckee
