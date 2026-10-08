#pragma once

#include <vector>

#include "luckee/aabb.hpp"

namespace luckee {

class Level;

class Entity {
public:
    explicit Entity(Level& level);
    virtual ~Entity() = default;

    virtual void tick();

    void turn(float deltaX, float deltaY);

    void move(float xa, float ya, float za);
    void moveRelative(float xa, float za, float speed);

    void remove();
    bool removed() const { return removed_; }

    void resetPosition();

    float x() const { return x_; }
    float y() const { return y_; }
    float z() const { return z_; }

    float previousX() const { return xo_; }
    float previousY() const { return yo_; }
    float previousZ() const { return zo_; }

    float renderX(float alpha) const {
        return xo_ + (x_ - xo_) * alpha;
    }

    float renderY(float alpha) const {
        return yo_ + (y_ - yo_) * alpha;
    }

    float renderZ(float alpha) const {
        return zo_ + (z_ - zo_) * alpha;
    }

    float yRot() const { return yRot_; }
    float xRot() const { return xRot_; }
    bool onGround() const { return onGround_; }

    bool isLit() const;

protected:
    void resetPos();
    void setSize(float width, float height);
    void setPos(float x, float y, float z);

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
    bool removed_ = false;

    float heightOffset_ = 0.0f;
    float bbWidth_ = 0.6f;
    float bbHeight_ = 1.8f;

    std::vector<AABB> collisionCubes_;
};

} // namespace luckee
