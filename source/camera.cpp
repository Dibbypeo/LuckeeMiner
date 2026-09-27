#include "luckee/camera.hpp"

namespace luckee {

void Camera::applyLook(float deltaX, float deltaY) {
    constexpr float sensitivity = 0.15f;

    yaw_ += deltaX * sensitivity;
    pitch_ -= deltaY * sensitivity;

    if (pitch_ > 90.0f) pitch_ = 90.0f;
    if (pitch_ < -90.0f) pitch_ = -90.0f;
}

} // namespace luckee
