#pragma once

namespace luckee {

// Camera orientation adapted from RubyDung's original mouse-look behavior.
class Camera {
public:
    void applyLook(float deltaX, float deltaY);

    float yaw() const { return yaw_; }
    float pitch() const { return pitch_; }

private:
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
};

} // namespace luckee
